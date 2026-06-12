/*
 * Aguada Gateway Firmware — variante WiFi/MQTT
 * Board: ESP32 DevKit (esp32dev)
 *
 * Recebe ESP-NOW → publica MQTT via WiFi
 * Recebe comandos MQTT aguada/cmd/# → envia ESP-NOW aos nós
 *
 * LED built-in (GPIO2):
 *   Sem WiFi / desconfigurado → 3 flashes curtos em loop
 *   MQTT desconectado         → 2 flashes médios em loop
 *   Operação normal           → heartbeat curto periódico
 *   Evento RX                 → 1 pulso curto
 *   Evento RX HEARTBEAT       → 2 pulsos curtos
 *   Evento TX comando         → 3 pulsos curtos
 *   Evento desconexão         → 2 pulsos longos
 *
 * Tópicos MQTT:
 *   Publish:  aguada/<node_id>/<sensor_id>/state  (SENSOR, retain)
 *             aguada/<node_id>/heartbeat
 *             aguada/<node_id>/hello
 *             aguada/gateway/status               (retain, LWT)
 *   Subscribe: aguada/cmd/restart  {"node_id":"0x1234"}
 *              aguada/cmd/config   {"node_id":"0x1234",...}
 */

#ifdef GATEWAY_WIFI_BUILD

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <freertos/queue.h>
#include <string.h>
#include <time.h>

#include "espnow_gw.h"
#include "../../shared/protocol.h"

static const char *TAG = "gw_wifi";

// ── Credenciais WiFi / MQTT ────────────────────────────────────────────────────
#ifndef WIFI_SSID
#define WIFI_SSID "luciano"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "Luciano19852012"
#endif
#ifndef MQTT_BROKER
#define MQTT_BROKER "192.168.0.177"
#endif
#ifndef MQTT_PORT_NUM
#ifdef MQTT_PORT
#define MQTT_PORT_NUM MQTT_PORT
#else
#define MQTT_PORT_NUM 1883
#endif
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASS
#define MQTT_PASS ""
#endif

// ── LED built-in ──────────────────────────────────────────────────────────────
#ifndef LED_PIN
#define LED_PIN 2          // GPIO2 = LED built-in na maioria dos ESP32 DevKit
#endif
#define LED_ON  HIGH
#define LED_OFF LOW

// ── Constantes ────────────────────────────────────────────────────────────────
#define MQTT_KEEPALIVE_S     60
#define MQTT_RECONNECT_MS    5000
#define NTP_SERVER           "pool.ntp.org"
#define GW_STATUS_INTERVAL_MS 60000UL

// ── Dual output (MQTT + Serial) ───────────────────────────────────────────────
// Quando GATEWAY_DUAL_OUTPUT=1, todos os pacotes recebidos são emitidos tanto
// via MQTT quanto via Serial, tornando o gateway compatível com a bridge serial.
#ifndef GATEWAY_DUAL_OUTPUT
#define GATEWAY_DUAL_OUTPUT 0
#endif

#if GATEWAY_DUAL_OUTPUT
static void serial_out(JsonDocument &doc) {
    serializeJson(doc, Serial);
    Serial.println();
    Serial.flush();
}
#endif

// ── LED: estado de sinalização ────────────────────────────────────────────────
typedef enum {
    LED_MODE_BOOT,
    LED_MODE_NO_WIFI,
    LED_MODE_NO_MQTT,
    LED_MODE_READY,
} led_mode_t;

typedef struct {
    bool active;
    bool level_on;
    uint8_t pulses_left;
    uint16_t on_ms;
    uint16_t off_ms;
    uint32_t next_change_ms;
} led_overlay_t;

static led_mode_t     s_led_mode = LED_MODE_BOOT;
static bool           s_led_state = false;
static uint32_t       s_led_mode_since_ms = 0;
static led_overlay_t  s_led_overlay = {};

static void led_write(bool on) {
    if (s_led_state == on) return;
    s_led_state = on;
    digitalWrite(LED_PIN, on ? LED_ON : LED_OFF);
}

static void led_set_mode(led_mode_t mode) {
    if (s_led_mode == mode) return;
    s_led_mode = mode;
    s_led_mode_since_ms = millis();
}

static void led_flash_event(uint8_t pulses, uint16_t on_ms, uint16_t off_ms) {
    if (pulses == 0) return;
    s_led_overlay.active = true;
    s_led_overlay.level_on = true;
    s_led_overlay.pulses_left = pulses;
    s_led_overlay.on_ms = on_ms;
    s_led_overlay.off_ms = off_ms;
    s_led_overlay.next_change_ms = millis() + on_ms;
    led_write(true);
}

static void led_event_rx(void) {
    led_flash_event(1, 30, 30);
}

static void led_event_rx_heartbeat(void) {
    led_flash_event(2, 25, 45);
}

static void led_event_tx(void) {
    led_flash_event(3, 20, 35);
}

static void led_event_disconnect(void) {
    led_flash_event(2, 90, 100);
}

static void led_tick(void) {
    uint32_t now = millis();

    if (s_led_overlay.active) {
        if ((int32_t)(now - s_led_overlay.next_change_ms) >= 0) {
            if (s_led_overlay.level_on) {
                led_write(false);
                s_led_overlay.level_on = false;
                s_led_overlay.pulses_left--;
                if (s_led_overlay.pulses_left == 0) {
                    s_led_overlay.active = false;
                    s_led_mode_since_ms = now;
                } else {
                    s_led_overlay.next_change_ms = now + s_led_overlay.off_ms;
                }
            } else {
                led_write(true);
                s_led_overlay.level_on = true;
                s_led_overlay.next_change_ms = now + s_led_overlay.on_ms;
            }
        }
        return;
    }

    uint32_t phase = now - s_led_mode_since_ms;
    bool on = false;
    switch (s_led_mode) {
        case LED_MODE_BOOT:
            on = ((phase / 100U) % 2U) == 0U;
            break;

        case LED_MODE_NO_WIFI: {
            uint32_t p = phase % 1600U;
            on = (p < 70U) || (p >= 170U && p < 240U) || (p >= 340U && p < 410U);
            break;
        }

        case LED_MODE_NO_MQTT: {
            uint32_t p = phase % 1800U;
            on = (p < 140U) || (p >= 260U && p < 400U);
            break;
        }

        case LED_MODE_READY: {
            uint32_t p = phase % 2200U;
            on = (p < 45U);
            break;
        }
    }

    led_write(on);
}

// ── Packet queue (ESP-NOW cb → loop) ─────────────────────────────────────────
#define PKT_QUEUE_LEN 16
typedef struct { espnow_packet_t pkt; uint8_t src_mac[6]; } pkt_event_t;
static QueueHandle_t s_pkt_queue = nullptr;
static uint32_t s_queue_drops    = 0;
static int64_t s_time_offset_s   = 0;
static bool s_time_synced_from_cmd = false;

// ── Node MAC cache ────────────────────────────────────────────────────────────
#define NODE_CACHE_MAX 20
typedef struct { uint16_t node_id; uint8_t mac[6]; } node_mac_t;
static node_mac_t s_nodes[NODE_CACHE_MAX];
static int        s_nodes_len     = 0;
static uint32_t   s_last_packet_ms = 0;
static uint32_t   s_last_status_ms = 0;
static uint8_t    s_gateway_mac[6] = {0};

static WiFiClient   s_wifi_client;
static PubSubClient s_mqtt(s_wifi_client);
static uint32_t     s_last_mqtt_reconnect_ms = 0;

#if GATEWAY_DUAL_OUTPUT
// Buffer não-bloqueante para comandos seriais recebidos pelo host
static char s_serial_buf[256];
static int  s_serial_buf_len = 0;
#endif

// ── Helpers ───────────────────────────────────────────────────────────────────

static void cache_mac(uint16_t node_id, const uint8_t *mac) {
    for (int i = 0; i < s_nodes_len; i++) {
        if (s_nodes[i].node_id == node_id) { memcpy(s_nodes[i].mac, mac, 6); return; }
    }
    if (s_nodes_len < NODE_CACHE_MAX) {
        s_nodes[s_nodes_len].node_id = node_id;
        memcpy(s_nodes[s_nodes_len].mac, mac, 6);
        s_nodes_len++;
    }
}

static uint32_t unix_now(void) {
    time_t t = time(nullptr);
    if (t > 1000000) {
        return (uint32_t)t;
    }
    return (uint32_t)(s_time_offset_s + esp_timer_get_time() / 1000000LL);
}

static void apply_time_sync(int64_t ts) {
    s_time_offset_s = ts - (esp_timer_get_time() / 1000000LL);
    s_time_synced_from_cmd = true;
    ESP_LOGI(TAG, "Time sync via CMD_SET_TIME ts=%lld unix_now=%lu",
             (long long)ts, (unsigned long)unix_now());
}

static void format_mac(const uint8_t mac[6], char *out, size_t n) {
    snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void set_vbat_json(JsonDocument &doc, int8_t raw) {
    if (raw < 0) doc["vbat"] = nullptr;
    else         doc["vbat"] = raw / 10.0f;
}

// ── MQTT publish helpers ──────────────────────────────────────────────────────

static bool mqtt_publish(const char *topic, const String &payload, bool retain = false) {
    if (!s_mqtt.connected()) return false;
    return s_mqtt.publish(topic, payload.c_str(), retain);
}

static void publish_sensor(const espnow_packet_t *pkt) {
    char topic[64];
    snprintf(topic, sizeof(topic), "aguada/0x%04X/%d/state", pkt->node_id, pkt->sensor_id);

    JsonDocument doc;
    char nid[8]; snprintf(nid, sizeof(nid), "0x%04X", pkt->node_id);
    doc["v"]           = 3;
    doc["type"]        = "SENSOR";
    doc["node_id"]     = nid;
    doc["sensor_id"]   = pkt->sensor_id;
    doc["distance_cm"] = (pkt->flags & FLAG_SENSOR_ERROR) ? -1 : (int)pkt->distance_cm;
    doc["rssi"]        = pkt->rssi;
    set_vbat_json(doc, pkt->vbat);
    doc["flags"]       = pkt->flags;
    doc["seq"]         = pkt->seq;
    doc["ts"]          = unix_now();

#if GATEWAY_DUAL_OUTPUT
    serial_out(doc);
#endif
    String out;
    serializeJson(doc, out);
    mqtt_publish(topic, out, true);
}

static void publish_heartbeat(const espnow_packet_t *pkt) {
    char topic[64];
    snprintf(topic, sizeof(topic), "aguada/0x%04X/heartbeat", pkt->node_id);

    JsonDocument doc;
    char nid[8]; snprintf(nid, sizeof(nid), "0x%04X", pkt->node_id);
    doc["v"]           = 3;
    doc["type"]        = "HEARTBEAT";
    doc["node_id"]     = nid;
    doc["sensor_id"]   = pkt->sensor_id;
    doc["distance_cm"] = (int)pkt->distance_cm;
    doc["rssi"]        = pkt->rssi;
    set_vbat_json(doc, pkt->vbat);
    doc["reserved"]    = pkt->reserved;
    doc["seq"]         = pkt->seq;
    doc["ts"]          = unix_now();

#if GATEWAY_DUAL_OUTPUT
    serial_out(doc);
#endif
    String out;
    serializeJson(doc, out);
    mqtt_publish(topic, out, false);
}

static void publish_hello(const espnow_packet_t *pkt) {
    char topic[64];
    snprintf(topic, sizeof(topic), "aguada/0x%04X/hello", pkt->node_id);

    JsonDocument doc;
    char nid[8]; snprintf(nid, sizeof(nid), "0x%04X", pkt->node_id);
    doc["v"]           = 3;
    doc["type"]        = "HELLO";
    doc["node_id"]     = nid;
    doc["num_sensors"] = pkt->distance_cm;
    doc["rssi"]        = pkt->rssi;
    set_vbat_json(doc, pkt->vbat);
    doc["flags"]       = pkt->flags;
    doc["ts"]          = unix_now();

#if GATEWAY_DUAL_OUTPUT
    serial_out(doc);
#endif
    String out;
    serializeJson(doc, out);
    mqtt_publish(topic, out, false);
}

static void publish_gateway_status(void) {
    char mac_str[20];
    format_mac(s_gateway_mac, mac_str, sizeof(mac_str));

    gw_espnow_stats_t stats = {};
    gw_espnow_get_stats(&stats);

    JsonDocument doc;
    doc["v"]                 = 3;
    doc["type"]              = "GATEWAY_STATUS";
    doc["fw"]                = FW_VERSION;
    doc["mac"]               = mac_str;
    doc["port"]              = "wifi";
    doc["uptime_s"]          = millis() / 1000UL;
    doc["free_heap"]         = ESP.getFreeHeap();
    doc["rx_packets"]        = stats.rx_packets;
    doc["crc_failures"]      = stats.crc_failures;
    doc["queue_drops"]       = s_queue_drops;
    doc["known_nodes"]       = s_nodes_len;
    doc["channel"]           = ESPNOW_CHANNEL;
    doc["wifi_rssi"]         = WiFi.RSSI();
    doc["time_synced"]       = (unix_now() > 1000000UL);
    doc["time_source"]       = (time(nullptr) > 1000000) ? "ntp" : (s_time_synced_from_cmd ? "cmd" : "unsynced");
    doc["online"]            = true;
    if (s_last_packet_ms > 0)
        doc["last_packet_age_s"] = (millis() - s_last_packet_ms) / 1000UL;
    else
        doc["last_packet_age_s"] = nullptr;
    doc["ts"] = unix_now();

    String out;
    serializeJson(doc, out);
    mqtt_publish("aguada/gateway/status", out, true);
    s_last_status_ms = millis();
}

static void publish_time_ack(void) {
    JsonDocument doc;
    doc["v"] = 3;
    doc["type"] = "TIME_ACK";
    doc["fw"] = FW_VERSION;
    doc["ts"] = unix_now();

#if GATEWAY_DUAL_OUTPUT
    serial_out(doc);
#endif

    if (s_mqtt.connected()) {
        String out;
        serializeJson(doc, out);
        mqtt_publish("aguada/gateway/time_ack", out, false);
    }
}

// ── ESP-NOW receive ───────────────────────────────────────────────────────────

static void on_recv(const espnow_packet_t *pkt, const uint8_t *src_mac) {
    pkt_event_t evt;
    evt.pkt = *pkt;
    memcpy(evt.src_mac, src_mac, 6);
    s_last_packet_ms = millis();
    if (xQueueSend(s_pkt_queue, &evt, 0) != pdTRUE)
        s_queue_drops++;
}

// ── Comandos MQTT → ESP-NOW ───────────────────────────────────────────────────

static const uint8_t BCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static void cmd_restart(uint16_t node_id) {
    espnow_packet_t pkt = {};
    pkt.version = PROTO_VERSION;
    pkt.type    = PKT_CMD_RESTART;
    pkt.node_id = node_id;
    pkt.ttl     = DEFAULT_TTL;
    esp_err_t err = gw_espnow_send(&pkt, BCAST_MAC);
    if (err == ESP_OK) led_event_tx();
    ESP_LOGI(TAG, "CMD_RESTART → 0x%04X err=%d", node_id, (int)err);
}

static void cmd_config(uint16_t node_id, const JsonDocument &doc) {
    espnow_packet_t pkt = {};
    pkt.version = PROTO_VERSION;
    pkt.type    = PKT_CMD_CONFIG;
    pkt.node_id = node_id;
    pkt.ttl     = DEFAULT_TTL;
    pkt.flags   = FLAG_CONFIG_PENDING;

    if (doc["vbat_pin"].is<uint8_t>())
        pkt.sensor_id = doc["vbat_pin"].as<uint8_t>();
    if (doc["vbat_enabled"].is<bool>() || doc["vbat_enabled"].is<int>())
        pkt.reserved  = doc["vbat_enabled"].as<bool>() ? 1 : 0;

    uint8_t cfg_vbat_div = 0;
    if (doc["vbat_div"].is<uint8_t>()) cfg_vbat_div = doc["vbat_div"].as<uint8_t>();

    uint8_t cfg_num_sensors = 0;
    if (doc["num_sensors"].is<int>()) {
        int ns = doc["num_sensors"].as<int>();
        if (ns >= 0 && ns <= 2) {
            cfg_num_sensors = (uint8_t)ns;
            pkt.flags |= FLAG_CFG_NUM_SENSORS;
        }
    }
    pkt.distance_cm = ((uint16_t)cfg_num_sensors << 8) | cfg_vbat_div;
    esp_err_t err = gw_espnow_send(&pkt, BCAST_MAC);
    if (err == ESP_OK) led_event_tx();
    ESP_LOGI(TAG, "CMD_CONFIG → 0x%04X err=%d", node_id, (int)err);
}

static uint16_t parse_node_id(const JsonDocument &doc) {
    if (doc["node_id"].is<const char*>())
        return (uint16_t)strtoul(doc["node_id"].as<const char*>(), nullptr, 0);
    return doc["node_id"].as<uint16_t>();
}

static void on_mqtt_message(char *topic, byte *payload, unsigned int length) {
    char buf[512];
    if (length >= sizeof(buf)) return;
    memcpy(buf, payload, length);
    buf[length] = '\0';

    JsonDocument doc;
    if (deserializeJson(doc, buf) != DeserializationError::Ok) return;

    if (strstr(topic, "/cmd/restart")) {
        cmd_restart(parse_node_id(doc));
    } else if (strstr(topic, "/cmd/config")) {
        cmd_config(parse_node_id(doc), doc);
    } else if (strstr(topic, "/cmd/settime")) {
        int64_t ts = doc["ts"] | 0;
        if (ts > 0) {
            apply_time_sync(ts);
            publish_time_ack();
            publish_gateway_status();
        }
    }
}

#if GATEWAY_DUAL_OUTPUT
static void serial_output_gateway_ready(void) {
    char mac_str[20];
    format_mac(s_gateway_mac, mac_str, sizeof(mac_str));
    JsonDocument doc;
    doc["v"]    = 3;
    doc["type"] = "GATEWAY_READY";
    doc["fw"]   = FW_VERSION "-dual";
    doc["mac"]  = mac_str;
    doc["port"] = "dual";
    doc["ts"]   = unix_now();
    serial_out(doc);
}

// Parseia comandos chegando pelo Serial (SETTIME, CMD_RESTART, CMD_CONFIG)
static void handle_serial_input(void) {
    while (Serial.available()) {
        int c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (s_serial_buf_len > 0) {
                s_serial_buf[s_serial_buf_len] = '\0';
                s_serial_buf_len = 0;
                JsonDocument cmd_doc;
                if (deserializeJson(cmd_doc, s_serial_buf) != DeserializationError::Ok)
                    continue;
                const char *cmd = cmd_doc["cmd"] | "";
                if (strcmp(cmd, "SETTIME") == 0) {
                    // gateway usa NTP, apenas confirma
                    JsonDocument ack;
                    ack["v"]    = 3;
                    ack["type"] = "TIME_ACK";
                    ack["ts"]   = unix_now();
                    serial_out(ack);
                } else if (strcmp(cmd, "RESTART") == 0) {
                    cmd_restart(parse_node_id(cmd_doc));
                } else if (strcmp(cmd, "CONFIG") == 0) {
                    cmd_config(parse_node_id(cmd_doc), cmd_doc);
                }
            }
        } else if (s_serial_buf_len < (int)sizeof(s_serial_buf) - 1) {
            s_serial_buf[s_serial_buf_len++] = (char)c;
        }
    }
}
#endif

// ── WiFi / MQTT connection ────────────────────────────────────────────────────

static bool wifi_configured(void) {
    return strlen(WIFI_SSID) > 0 && strlen(MQTT_BROKER) > 0;
}

static bool wifi_connect(void) {
    led_set_mode(LED_MODE_NO_WIFI);
    if (!wifi_configured()) {
        ESP_LOGE(TAG, "WiFi/MQTT não configurado: SSID ou broker vazio");
        return false;
    }

    ESP_LOGI(TAG, "Connecting WiFi SSID=%s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - t0 > 15000) {
            ESP_LOGE(TAG, "WiFi timeout");
            return false;
        }
        led_tick();
        delay(50);
    }
    ESP_LOGI(TAG, "WiFi connected IP=%s ch=%d RSSI=%d",
             WiFi.localIP().toString().c_str(),
             WiFi.channel(),
             WiFi.RSSI());
    led_set_mode(LED_MODE_BOOT);
    return true;
}

static void mqtt_connect(void) {
    if (s_mqtt.connected()) return;
    uint32_t now = millis();
    if (now - s_last_mqtt_reconnect_ms < MQTT_RECONNECT_MS) return;
    s_last_mqtt_reconnect_ms = now;

    led_set_mode(LED_MODE_NO_MQTT);

    char client_id[32];
    snprintf(client_id, sizeof(client_id), "aguada-gw-%02X%02X",
             s_gateway_mac[4], s_gateway_mac[5]);

    // LWT: publica offline ao desconectar
    bool ok;
    const char *lwt = "{\"online\":false,\"v\":3}";
    if (strlen(MQTT_USER) > 0)
        ok = s_mqtt.connect(client_id, MQTT_USER, MQTT_PASS,
                            "aguada/gateway/status", 0, true, lwt);
    else
        ok = s_mqtt.connect(client_id, nullptr, nullptr,
                            "aguada/gateway/status", 0, true, lwt);

    if (ok) {
        ESP_LOGI(TAG, "MQTT connected as %s", client_id);
        s_mqtt.subscribe("aguada/cmd/#");
        led_set_mode(LED_MODE_READY);
        publish_gateway_status();
    } else {
        ESP_LOGW(TAG, "MQTT connect failed rc=%d", s_mqtt.state());
    }
}

// ── NTP ───────────────────────────────────────────────────────────────────────

static void ntp_init(void) {
    configTime(0, 0, NTP_SERVER);
    ESP_LOGI(TAG, "NTP sync started (%s)", NTP_SERVER);
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────

void setup(void) {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    led_write(false);
    s_led_mode_since_ms = millis();
    delay(300);

    ESP_LOGI(TAG, "Aguada Gateway WiFi fw=" FW_VERSION);

    s_pkt_queue = xQueueCreate(PKT_QUEUE_LEN, sizeof(pkt_event_t));

    while (!wifi_connect()) {
        led_set_mode(LED_MODE_NO_WIFI);
        uint32_t retry_start = millis();
        while (millis() - retry_start < 2000) {
            led_tick();
            delay(10);
        }
    }
    esp_wifi_get_mac(WIFI_IF_STA, s_gateway_mac);

    // Usa o canal real do AP (ESP-NOW deve coincidir com o canal WiFi STA)
    uint8_t ap_channel = ESPNOW_CHANNEL;
    {
        wifi_second_chan_t sec;
        esp_wifi_get_channel(&ap_channel, &sec);
    }
    gw_espnow_init(ap_channel, on_recv);

    ntp_init();

    s_mqtt.setServer(MQTT_BROKER, MQTT_PORT_NUM);
    s_mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
    s_mqtt.setCallback(on_mqtt_message);
    s_mqtt.setBufferSize(1024);

    mqtt_connect();

#if GATEWAY_DUAL_OUTPUT
    serial_output_gateway_ready();
#endif
}

void loop(void) {
    led_tick();

#if GATEWAY_DUAL_OUTPUT
    handle_serial_input();
#endif

    // Reconexão WiFi
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW(TAG, "WiFi lost, reconnecting...");
        led_event_disconnect();
        while (!wifi_connect()) {
            led_set_mode(LED_MODE_NO_WIFI);
            uint32_t retry_start = millis();
            while (millis() - retry_start < 2000) {
                led_tick();
                delay(10);
            }
        }
    }

    // Reconexão MQTT
    if (!s_mqtt.connected()) {
        led_set_mode(LED_MODE_NO_MQTT);
        mqtt_connect();
    }
    s_mqtt.loop();

    // Status periódico
    if (millis() - s_last_status_ms >= GW_STATUS_INTERVAL_MS) {
        publish_gateway_status();
    }

    // Drena fila ESP-NOW
    pkt_event_t evt;
    while (xQueueReceive(s_pkt_queue, &evt, 0) == pdTRUE) {
        cache_mac(evt.pkt.node_id, evt.src_mac);
        switch (evt.pkt.type) {
            case PKT_SENSOR:
                led_event_rx();
                publish_sensor(&evt.pkt);
                break;
            case PKT_HEARTBEAT:
                led_event_rx_heartbeat();
                publish_heartbeat(&evt.pkt);
                break;
            case PKT_HELLO:
                led_event_rx();
                publish_hello(&evt.pkt);
                break;
            default:
                ESP_LOGW(TAG, "Unhandled type=0x%02X node=0x%04X",
                         evt.pkt.type, evt.pkt.node_id);
                break;
        }
    }

    delay(5);
}

#endif // GATEWAY_WIFI_BUILD
