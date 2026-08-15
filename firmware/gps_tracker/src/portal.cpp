#include "portal.h"
#include "net_config.h"
#include "gps_web.h"
#include "telemetry.h"   // /api/state reports the uplink status alongside the fix

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

WebServer server(DEFAULT_PORTAL_PORT);
DNSServer dns;

const IPAddress kApIp(192, 168, 4, 1);

char apSsidStr[32] = "";
bool started       = false;

NetState  cached   = {};   // last frame from main.cpp; answers /api/state
NetCreds  stored   = {};
bool      dirty    = false;
uint32_t  pollCount = 0;
uint32_t  lastApFixMs = 0;
uint32_t  scanDoneMs  = 0;   // millis() do ultimo scan concluido (cache)
bool      scanBusy    = false;

// Copies a form field into a fixed buffer, rejecting rather than truncating:
// a silently clipped SSID or passphrase produces an association failure the
// user cannot explain from the page.
bool takeField(const char *name, char *dst, size_t size, const char **err) {
    if (!server.hasArg(name)) {
        dst[0] = '\0';
        return true;
    }
    String v = server.arg(name);
    if (v.length() >= size) {
        *err = name;
        return false;
    }
    strncpy(dst, v.c_str(), size - 1);
    dst[size - 1] = '\0';
    return true;
}

void handleState() {
    pollCount++;
    const NetState &s = cached;

    char json[768];
    snprintf(json, sizeof(json),
             "{\"id\":\"%s\",\"fix\":%s,\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.1f,"
             "\"speed\":%.1f,\"hdg\":%.0f,\"hdg_mag\":%s,"
             "\"sats\":%u,\"sats_view\":%u,\"hdop\":%.1f,\"rx\":%s,"
             "\"temp\":%.1f,\"hum\":%.1f,\"env\":%s,\"moving\":%s,\"up\":%lu,"
             "\"wifi\":%s,\"ip\":\"%s\",\"mqtt\":%s,\"pub\":%lu,"
             "\"cfg_ssid\":\"%s\",\"cfg_host\":\"%s\",\"cfg_port\":%u,\"cfg_user\":\"%s\","
             "\"cfg_wpass\":%s,\"cfg_mpass\":%s}",
             apSsidStr,
             s.have_fix ? "true" : "false", s.lat, s.lon, s.alt_m,
             s.speed_kmh, s.heading_deg, s.heading_from_compass ? "true" : "false",
             s.sats_used, s.sats_in_view, s.hdop, s.receiving_nmea ? "true" : "false",
             s.temp_c, s.hum_pct, s.env_valid ? "true" : "false",
             s.cadence_moving ? "true" : "false", (unsigned long)s.uptime_s,
             telemetry::wifiConnected() ? "true" : "false", telemetry::staIp(),
             telemetry::mqttConnected() ? "true" : "false", (unsigned long)telemetry::published(),
             stored.wifi_ssid, stored.mqtt_host, (unsigned)stored.mqtt_port, stored.mqtt_user,
             // The two password fields are reported as booleans, never echoed:
             // the page only needs to know whether one is already stored so it
             // can say so in the placeholder.
             stored.wifi_pass[0] ? "true" : "false",
             stored.mqtt_pass[0] ? "true" : "false");

    server.send(200, "application/json", json);
}

// Scan assíncrono. Um scan síncrono bloqueia ~3s, o que custaria bytes do UART
// do GPS (orçamento ~266ms) -- por isso WiFi.scanNetworks(true): dispara e
// devolve logo, e a página faz polling até haver resultado.
//
// Efeito colateral inevitável: a varredura passa por todos os canais e tira o
// rádio do canal do AP durante alguns segundos. O telemóvel ligado ao portal
// pode ver um soluço. É aceitável porque o scan é sob demanda (o utilizador
// carregou no botão), nunca periódico, e o resultado fica em cache.
void handleScan() {
    int n = WiFi.scanComplete();

    if (n == WIFI_SCAN_RUNNING) {
        server.send(200, "application/json", "{\"status\":\"scanning\"}");
        return;
    }

    // Sem resultado, ou resultado velho: dispara nova varredura.
    if (n == WIFI_SCAN_FAILED ||
        (scanDoneMs != 0 && (millis() - scanDoneMs) > DEFAULT_SCAN_MAX_AGE_MS)) {
        if (n >= 0) {
            WiFi.scanDelete();
        }
        scanBusy   = true;
        scanDoneMs = 0;   // sem isto o resultado novo chega ja "velho" pela
                          // marca antiga e dispara rescan em ciclo infinito
        WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false);
        server.send(200, "application/json", "{\"status\":\"scanning\"}");
        return;
    }

    scanBusy   = false;
    scanDoneMs = millis();

    // Dedup por SSID mantendo o mais forte: uma rede mesh aparece uma vez por
    // ponto de acesso, e uma lista com cinco "luciano2ghz" seria inútil no
    // telemóvel. Redes sem nome (ocultas) saem fora -- não dá para as escolher.
    char json[1600];
    int  w = snprintf(json, sizeof(json), "{\"status\":\"ok\",\"nets\":[");
    int  emitted = 0;

    for (int i = 0; i < n && emitted < DEFAULT_SCAN_MAX_RESULTS; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0 || ssid.length() > 32) {
            continue;
        }

        bool dup = false;
        for (int j = 0; j < i; j++) {
            if (WiFi.SSID(j) == ssid) { dup = true; break; }   // scan vem ordenado por RSSI
        }
        if (dup) {
            continue;
        }

        // Aspas e barras invertidas no SSID partiriam o JSON.
        char esc[70];
        int  e = 0;
        for (size_t k = 0; k < ssid.length() && e < (int)sizeof(esc) - 2; k++) {
            char ch = ssid[k];
            if (ch == '"' || ch == '\\') { esc[e++] = '\\'; }
            if ((uint8_t)ch < 0x20)      { continue; }
            esc[e++] = ch;
        }
        esc[e] = '\0';

        int need = snprintf(json + w, sizeof(json) - w,
                            "%s{\"ssid\":\"%s\",\"rssi\":%d,\"ch\":%d,\"open\":%s}",
                            emitted ? "," : "", esc, WiFi.RSSI(i), WiFi.channel(i),
                            WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false");
        if (need <= 0 || w + need >= (int)sizeof(json) - 4) {
            break;   // buffer cheio: devolve o que já cabe em vez de truncar o JSON
        }
        w += need;
        emitted++;
    }

    snprintf(json + w, sizeof(json) - w, "]}");
    server.send(200, "application/json", json);
}

void handleConfig() {
    NetCreds c = stored;
    const char *bad = nullptr;

    bool ok = takeField("wifi_ssid", c.wifi_ssid, sizeof(c.wifi_ssid), &bad) &&
              takeField("mqtt_host", c.mqtt_host, sizeof(c.mqtt_host), &bad) &&
              takeField("mqtt_user", c.mqtt_user, sizeof(c.mqtt_user), &bad);

    // Blank password fields mean "keep what is stored", so the page never has
    // to round-trip a secret it was deliberately not given.
    if (ok && server.hasArg("wifi_pass") && server.arg("wifi_pass").length() > 0) {
        ok = takeField("wifi_pass", c.wifi_pass, sizeof(c.wifi_pass), &bad);
    }
    if (ok && server.hasArg("mqtt_pass") && server.arg("mqtt_pass").length() > 0) {
        ok = takeField("mqtt_pass", c.mqtt_pass, sizeof(c.mqtt_pass), &bad);
    }

    if (!ok) {
        char err[96];
        snprintf(err, sizeof(err), "{\"ok\":false,\"err\":\"campo %s demasiado longo\"}", bad);
        server.send(400, "application/json", err);
        return;
    }

    if (c.wifi_ssid[0] == '\0') {
        server.send(400, "application/json", "{\"ok\":false,\"err\":\"SSID em falta\"}");
        return;
    }

    long port = server.hasArg("mqtt_port") ? server.arg("mqtt_port").toInt() : DEFAULT_MQTT_PORT;
    if (port < 1 || port > 65535) {
        server.send(400, "application/json", "{\"ok\":false,\"err\":\"porta inválida\"}");
        return;
    }
    c.mqtt_port = (uint16_t)port;

    if (!net_store::save(c)) {
        server.send(500, "application/json", "{\"ok\":false,\"err\":\"NVS recusou a escrita\"}");
        return;
    }

    stored = c;
    dirty  = true;
    Serial.printf("[PORTAL] config guardada ssid=\"%s\" broker=%s:%u\n",
                  stored.wifi_ssid, stored.mqtt_host, (unsigned)stored.mqtt_port);
    server.send(200, "application/json", "{\"ok\":true}");
}

// Anything not a known route redirects to the root. This is what trips the
// OS captive-portal detector (it probes a known URL and expects a 204; a 302
// tells it there is a portal to show).
void handleNotFound() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}

}  // namespace

namespace portal {

bool begin() {
    net_store::load(stored);

    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    snprintf(apSsidStr, sizeof(apSsidStr), "%s-%02x%02x%02x",
             DEFAULT_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);

    // AP_STA, not AP: the field UI must survive the station associating to
    // the home network, and must come up even when nothing is configured.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(kApIp, kApIp, IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP(apSsidStr, nullptr, DEFAULT_AP_CHANNEL)) {
        Serial.println("[PORTAL] softAP falhou -- portal desligado");
        return false;
    }

    // Modem sleep OFF. Em WIFI_AP_STA o driver ativa poupança de energia por
    // causa da interface station; com a station ociosa (o caso normal deste
    // tracker, que passa a viagem fora de alcance) o rádio dorme entre
    // beacons e o AP fica invisível ou intermitente no scan do telemóvel.
    // Este AP é a única UI de campo, portanto não pode dormir.
    WiFi.setSleep(false);

    // Potência no máximo: a UI de campo é usada ao ar livre, e o C3 SuperMini
    // tem antena de PCB pequena. Sem custo relevante -- o consumo do rádio já
    // está dominado pelo AP acordado acima.
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    // Diagnóstico do rádio: softAP() devolver true não prova que o AP está a
    // emitir. Estes campos separam "não subiu" de "subiu e não aparece no
    // scan" -- sem eles a única pista seria a ausência no telemóvel.
    // O MAC da station vai aqui porque é o que se precisa colar num roteador
    // com filtro de MAC -- e um filtro é indistinguível de senha errada pelo
    // lado do ESP (os dois dão reason=2, AUTH_EXPIRE). Ver BENCH.md, Fase 3.
    Serial.printf("[PORTAL] radio mode=%d ap_ip=%s ap_ssid=\"%s\" ch=%d tx=%.1fdBm sleep=%s"
                  " sta_mac=%s\n",
                  (int)WiFi.getMode(), WiFi.softAPIP().toString().c_str(),
                  WiFi.softAPSSID().c_str(), (int)DEFAULT_AP_CHANNEL,
                  WiFi.getTxPower() * 0.25f, WiFi.getSleep() ? "on" : "off",
                  WiFi.macAddress().c_str());

    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(DEFAULT_DNS_PORT, "*", kApIp);

    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html; charset=utf-8", GPS_WEB_HTML);
    });
    server.on("/api/state", HTTP_GET, handleState);
    server.on("/api/scan", HTTP_GET, handleScan);
    server.on("/api/config", HTTP_POST, handleConfig);
    server.onNotFound(handleNotFound);
    server.begin();

    started = true;
    return true;
}

void tick(const NetState &s) {
    if (!started) {
        return;
    }
    cached = s;

    // Reposicao do AP no canal dele. Ver DEFAULT_AP_RESTORE_MS em
    // net_config.h: com a station a falhar, o AP fica preso no canal da rede
    // alvo e desaparece do scan. So agimos com a station desligada -- quando
    // ela esta ligada, partilhar o canal e inevitavel (um radio) e correto.
    // Durante um scan o canal salta de proposito -- repor o AP a meio
    // abortaria a varredura e devolveria uma lista vazia ao utilizador.
    if (scanBusy && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
        scanBusy = false;
    }
    if (!scanBusy && !telemetry::wifiConnected() &&
        (millis() - lastApFixMs) >= DEFAULT_AP_RESTORE_MS) {
        lastApFixMs = millis();
        bool apDown  = (WiFi.getMode() & WIFI_MODE_AP) == 0 || WiFi.softAPSSID().length() == 0;
        bool apDrift = WiFi.channel() != DEFAULT_AP_CHANNEL;
        if (apDown || apDrift) {
            Serial.printf("[PORTAL] AP %s (ch=%d) -- a repor no canal %d\n",
                          apDown ? "caiu" : "fora do canal", (int)WiFi.channel(),
                          (int)DEFAULT_AP_CHANNEL);
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAPConfig(kApIp, kApIp, IPAddress(255, 255, 255, 0));
            WiFi.softAP(apSsidStr, nullptr, DEFAULT_AP_CHANNEL);
            WiFi.setSleep(false);
        }
    }

    dns.processNextRequest();
    server.handleClient();
}

bool configDirty() {
    bool d = dirty;
    dirty  = false;
    return d;
}

const NetCreds &creds() { return stored; }
const char *apSsid()    { return apSsidStr; }
uint8_t apChannel()     { return (uint8_t)WiFi.channel(); }
uint32_t polls()        { return pollCount; }

}  // namespace portal
