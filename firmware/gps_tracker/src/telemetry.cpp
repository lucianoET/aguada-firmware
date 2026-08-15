#include "telemetry.h"
#include "net_config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

NetCreds creds;

char deviceId[16]   = "unknown";   // last 3 MAC bytes, e.g. "a4c1f8"
char clientId[32]   = "";
char topicState[64] = "";
char topicAvail[64] = "";
char staIpStr[20]   = "-";

uint32_t lastStaAttemptMs  = 0;
uint32_t lastMqttAttemptMs = 0;
uint32_t lastPublishMs     = 0;
uint32_t staFailures       = 0;   // falhas consecutivas de associacao, alimenta o backoff
uint32_t publishCount      = 0;

bool discoveryDone = false;   // per-session: cleared on every disconnect so a
                              // broker restart that dropped retained configs
                              // gets them republished on reconnect

// Derives the identity from the station MAC so a board keeps its HA entities
// across reflashes, and two trackers on one broker never collide.
void deriveIdentity() {
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    snprintf(deviceId, sizeof(deviceId), "%02x%02x%02x", mac[3], mac[4], mac[5]);
    snprintf(clientId, sizeof(clientId), "gps_tracker_%s", deviceId);
    snprintf(topicState, sizeof(topicState), "%s/gps/%s/state", DEFAULT_MQTT_TOPIC_PREFIX, deviceId);
    snprintf(topicAvail, sizeof(topicAvail), "%s/gps/%s/status", DEFAULT_MQTT_TOPIC_PREFIX, deviceId);
}

// One HA discovery entity. `vt` is the value_template against the state
// topic's JSON; a null uom/dc/sc/icon is simply omitted from the payload,
// matching what tools/bridge.py does for the node entities.
struct Entity {
    const char *suffix;   // unique_id / object_id suffix
    const char *name;
    const char *vt;
    const char *uom;
    const char *dc;
    const char *sc;
    const char *icon;
};

const Entity kEntities[] = {
    {"speed",   "Velocidade",  "{{ value_json.speed_kmh }}",  "km/h", "speed",       "measurement", "mdi:speedometer"},
    {"alt",     "Altitude",    "{{ value_json.alt_m }}",      "m",    "distance",    "measurement", "mdi:altimeter"},
    {"heading", "Rumo",        "{{ value_json.heading }}",    "°",    nullptr,       "measurement", "mdi:compass"},
    {"sats",    "Satélites",   "{{ value_json.sats }}",       nullptr, nullptr,      "measurement", "mdi:satellite-variant"},
    {"hdop",    "HDOP",        "{{ value_json.hdop }}",       nullptr, nullptr,      "measurement", "mdi:crosshairs-gps"},
    {"temp",    "Temperatura", "{{ value_json.temp_c }}",     "°C",   "temperature", "measurement", nullptr},
    {"hum",     "Umidade",     "{{ value_json.hum_pct }}",    "%",    "humidity",    "measurement", nullptr},
};

void publishSensorDiscovery(const Entity &e) {
    char topic[96];
    snprintf(topic, sizeof(topic), "%s/sensor/gps_%s_%s/config",
             DEFAULT_HA_DISCOVERY_PREFIX, deviceId, e.suffix);

    char extra[192];
    int n = 0;
    extra[0] = '\0';
    if (e.uom)  n += snprintf(extra + n, sizeof(extra) - n, ",\"unit_of_measurement\":\"%s\"", e.uom);
    if (e.dc)   n += snprintf(extra + n, sizeof(extra) - n, ",\"device_class\":\"%s\"", e.dc);
    if (e.sc)   n += snprintf(extra + n, sizeof(extra) - n, ",\"state_class\":\"%s\"", e.sc);
    if (e.icon)      snprintf(extra + n, sizeof(extra) - n, ",\"icon\":\"%s\"", e.icon);

    char payload[640];
    snprintf(payload, sizeof(payload),
             "{\"name\":\"%s\",\"unique_id\":\"gps_%s_%s\",\"object_id\":\"gps_%s_%s\","
             "\"state_topic\":\"%s\",\"value_template\":\"%s\","
             "\"availability_topic\":\"%s\",\"payload_available\":\"online\","
             "\"payload_not_available\":\"offline\"%s,"
             "\"device\":{\"name\":\"GPS Tracker %s\",\"identifiers\":[\"gps_tracker_%s\"],"
             "\"manufacturer\":\"Luctronics\",\"model\":\"ESP32-C3 SuperMini\"}}",
             e.name, deviceId, e.suffix, deviceId, e.suffix,
             topicState, e.vt, topicAvail, extra, deviceId, deviceId);

    mqtt.publish(topic, payload, /*retained=*/true);
}

// The device_tracker is what puts the node on the HA map. Its coordinates
// come from json_attributes_topic (latitude/longitude/gps_accuracy keys in
// the state JSON), not from value_template -- HA reads position only from
// the attributes, which is why the state JSON carries both spellings.
void publishTrackerDiscovery() {
    char topic[96];
    snprintf(topic, sizeof(topic), "%s/device_tracker/gps_%s_pos/config",
             DEFAULT_HA_DISCOVERY_PREFIX, deviceId);

    char payload[640];
    snprintf(payload, sizeof(payload),
             "{\"name\":\"Posição\",\"unique_id\":\"gps_%s_pos\",\"object_id\":\"gps_%s_pos\","
             "\"state_topic\":\"%s\",\"value_template\":\"{{ value_json.tracker_state }}\","
             "\"json_attributes_topic\":\"%s\",\"source_type\":\"gps\","
             "\"availability_topic\":\"%s\",\"payload_available\":\"online\","
             "\"payload_not_available\":\"offline\","
             "\"device\":{\"name\":\"GPS Tracker %s\",\"identifiers\":[\"gps_tracker_%s\"],"
             "\"manufacturer\":\"Luctronics\",\"model\":\"ESP32-C3 SuperMini\"}}",
             deviceId, deviceId, topicState, topicState, topicAvail, deviceId, deviceId);

    mqtt.publish(topic, payload, /*retained=*/true);
}

void publishDiscovery() {
    for (const Entity &e : kEntities) {
        publishSensorDiscovery(e);
    }
    publishTrackerDiscovery();
    Serial.printf("[MQTT] discovery publicado (%u entidades + tracker) id=%s\n",
                  (unsigned)(sizeof(kEntities) / sizeof(kEntities[0])), deviceId);
}

void publishState(const NetState &s) {
    // gps_accuracy is HA's metres-of-uncertainty field. HDOP is unitless, so
    // this is the usual HDOP x 5m UERE approximation -- good enough to size
    // the map circle, not a survey figure. Without a fix the position keys
    // are omitted entirely rather than sent as 0/0, which HA would happily
    // plot off the coast of Africa.
    char payload[512];
    if (s.have_fix) {
        snprintf(payload, sizeof(payload),
                 "{\"tracker_state\":\"home\",\"latitude\":%.6f,\"longitude\":%.6f,"
                 "\"gps_accuracy\":%.1f,\"alt_m\":%.1f,\"speed_kmh\":%.1f,"
                 "\"heading\":%.0f,\"sats\":%u,\"hdop\":%.1f,"
                 "\"temp_c\":%.1f,\"hum_pct\":%.0f,\"moving\":%s,\"uptime_s\":%lu}",
                 s.lat, s.lon,
                 isfinite(s.hdop) ? s.hdop * 5.0f : 50.0f,
                 s.alt_m, s.speed_kmh, s.heading_deg, s.sats_used, s.hdop,
                 s.temp_c, s.hum_pct, s.cadence_moving ? "true" : "false",
                 (unsigned long)s.uptime_s);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"tracker_state\":\"not_home\",\"sats\":%u,\"hdop\":%.1f,"
                 "\"speed_kmh\":0.0,\"heading\":%.0f,\"alt_m\":0.0,"
                 "\"temp_c\":%.1f,\"hum_pct\":%.0f,\"moving\":%s,\"uptime_s\":%lu}",
                 s.sats_used, s.hdop, s.heading_deg,
                 s.temp_c, s.hum_pct, s.cadence_moving ? "true" : "false",
                 (unsigned long)s.uptime_s);
    }

    if (mqtt.publish(topicState, payload)) {
        publishCount++;
    }
}

// Backoff exponencial com teto: 20s, 40s, 80s, 160s, depois 5 min fixos.
// Ver DEFAULT_STA_RETRY_MAX_MS em net_config.h para o porquê -- resumido,
// insistir sem parar mata o SoftAP, que é a UI de campo do tracker.
uint32_t staRetryDelayMs() {
    uint32_t shift = staFailures < 4 ? staFailures : 4;
    uint32_t d     = DEFAULT_STA_RETRY_MS << shift;
    return d > DEFAULT_STA_RETRY_MAX_MS ? (uint32_t)DEFAULT_STA_RETRY_MAX_MS : d;
}

// Non-blocking association. WiFi.begin() itself returns immediately; what
// must never happen here is a WiFi.waitForConnectResult() -- out of range,
// that blocks for seconds and drops GPS bytes for the whole window.
void staTick() {
    if (WiFi.status() == WL_CONNECTED) {
        staFailures = 0;   // rede voltou: próxima queda recomeça em 20s, não em 5 min
        return;
    }
    if (lastStaAttemptMs != 0 && (millis() - lastStaAttemptMs) < staRetryDelayMs()) {
        return;
    }

    if (lastStaAttemptMs != 0) {
        staFailures++;

        // Motivo da falha, não só "down": sem isto, senha errada, SSID fora de
        // alcance e rede a 5 GHz (que o C3 não vê) são indistinguíveis no
        // [HEALTH]. O reason code do evento (ver begin()) é mais específico.
        Serial.printf("[STA] tentativa %lu falhou status=%d ssid=\"%s\" -- proxima em %lus\n",
                      (unsigned long)staFailures, (int)WiFi.status(), creds.wifi_ssid,
                      (unsigned long)(staRetryDelayMs() / 1000));

        // Larga o canal antes de esperar. Sem este disconnect a station fica
        // a segurar o rádio entre tentativas e o SoftAP não assenta no canal
        // dele -- era o que fazia a rede do tracker sumir do scan.
        WiFi.disconnect(/*wifioff=*/false);
    }

    lastStaAttemptMs = millis();
    WiFi.begin(creds.wifi_ssid, creds.wifi_pass[0] ? creds.wifi_pass : nullptr);
}

// Same rule as staTick(): PubSubClient::connect() blocks until the socket
// resolves, so it is attempted at most once per DEFAULT_MQTT_RETRY_MS and
// only while the station already has an IP.
void mqttTick() {
    if (mqtt.connected()) {
        return;
    }
    discoveryDone = false;
    if (lastMqttAttemptMs != 0 && (millis() - lastMqttAttemptMs) < DEFAULT_MQTT_RETRY_MS) {
        return;
    }
    lastMqttAttemptMs = millis();

    mqtt.setServer(creds.mqtt_host, creds.mqtt_port ? creds.mqtt_port : DEFAULT_MQTT_PORT);

    // The retained LWT is what makes HA show the tracker as unavailable the
    // moment it drives out of range, instead of freezing on its last fix.
    bool ok = mqtt.connect(clientId,
                           creds.mqtt_user[0] ? creds.mqtt_user : nullptr,
                           creds.mqtt_pass[0] ? creds.mqtt_pass : nullptr,
                           topicAvail, /*willQos=*/0, /*willRetain=*/true, "offline");
    if (ok) {
        mqtt.publish(topicAvail, "online", /*retained=*/true);
        Serial.printf("[MQTT] conectado a %s:%u como %s\n",
                      creds.mqtt_host, (unsigned)creds.mqtt_port, clientId);
    }
}

}  // namespace

namespace telemetry {

void begin(const NetCreds &c) {
    creds = c;
    deriveIdentity();

    // O reason code do driver é a única fonte que distingue os modos de falha
    // de associação: 15 = 4WAY_HANDSHAKE_TIMEOUT (senha errada), 201 =
    // NO_AP_FOUND (fora de alcance, ou rede a 5 GHz que o C3 não vê), 205 =
    // CONNECTION_FAIL. WiFi.status() colapsa todos em 6 (DISCONNECTED), o que
    // no campo é indistinguível de "ainda a tentar".
    WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) {
        Serial.printf("[STA] desligado reason=%d\n",
                      (int)info.wifi_sta_disconnected.reason);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) {
        Serial.printf("[STA] ligado ch=%d rssi=%d ip=%s\n",
                      (int)WiFi.channel(), (int)WiFi.RSSI(),
                      WiFi.localIP().toString().c_str());
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

    // PubSubClient defaults to a 256-byte buffer; the discovery payloads are
    // ~500 bytes and would be silently dropped at that size.
    // O driver retenta a associacao sozinho a cada poucos segundos, o que
    // anularia o backoff de staTick() e manteria o churn de canal que faz
    // o SoftAP desaparecer. A cadencia de retentativa e nossa, so nossa.
    WiFi.setAutoReconnect(false);

    mqtt.setBufferSize(1024);
    mqtt.setKeepAlive(30);
}

void reload(const NetCreds &c) {
    creds = c;
    staFailures = 0;   // credenciais novas merecem cadencia rapida outra vez
    if (mqtt.connected()) {
        mqtt.disconnect();
    }
    WiFi.disconnect(/*wifioff=*/false);
    lastStaAttemptMs  = 0;   // retry immediately with the new credentials
    lastMqttAttemptMs = 0;
    discoveryDone     = false;
}

void tick(const NetState &s) {
    if (!net_store::haveWifi(creds)) {
        return;   // nothing configured yet -- the AP portal is the only surface
    }

    staTick();

    if (WiFi.status() != WL_CONNECTED) {
        strcpy(staIpStr, "-");
        return;
    }
    WiFi.localIP().toString().toCharArray(staIpStr, sizeof(staIpStr));

    if (!net_store::haveMqtt(creds)) {
        return;   // associated but no broker configured: portal still works over STA
    }

    mqttTick();
    if (!mqtt.connected()) {
        return;
    }
    mqtt.loop();

    if (!discoveryDone) {
        publishDiscovery();
        discoveryDone  = true;
        lastPublishMs  = 0;   // publish a first state right away so HA has values
    }

    if (lastPublishMs == 0 || (millis() - lastPublishMs) >= DEFAULT_MQTT_PUBLISH_MS) {
        lastPublishMs = millis();
        publishState(s);
    }
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }
bool mqttConnected() { return mqtt.connected(); }
const char *staIp()  { return staIpStr; }
uint32_t published() { return publishCount; }
uint32_t staFailCount() { return staFailures; }

}  // namespace telemetry
