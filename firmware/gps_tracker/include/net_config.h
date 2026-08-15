#pragma once
// firmware/gps_tracker/include/net_config.h
//
// Tunable network constants for the Fase 3 captive portal + MQTT telemetry
// path. Same shape as gps_config.h: every value is #ifndef-guarded so
// platformio.ini `build_flags -D...` can override it without touching source.
//
// Nothing here holds a credential. SSID/password/broker live in NVS and are
// written through the captive portal (net_store) — never compiled in, so a
// firmware image is safe to share and a network change needs no reflash.

// --- Access point (always on, even away from home) -----------------------

#ifndef DEFAULT_AP_SSID_PREFIX
#define DEFAULT_AP_SSID_PREFIX "gps-tracker"   // full SSID is <prefix>-<last 3 MAC bytes>
#endif

#ifndef DEFAULT_AP_CHANNEL
#define DEFAULT_AP_CHANNEL 1
#endif

#ifndef DEFAULT_PORTAL_PORT
#define DEFAULT_PORTAL_PORT 80
#endif

#ifndef DEFAULT_DNS_PORT
#define DEFAULT_DNS_PORT 53   // wildcard DNS is what makes the AP a *captive* portal
#endif

// --- Station (home WiFi, only while in range) ----------------------------

#ifndef DEFAULT_STA_RETRY_MS
#define DEFAULT_STA_RETRY_MS 20000   // gap between the FIRST association attempts when not connected
#endif

#ifndef DEFAULT_STA_RETRY_MAX_MS
#define DEFAULT_STA_RETRY_MAX_MS 300000   // teto do backoff (5 min).
// Porquê existe backoff: o C3 tem UM rádio. Em WIFI_AP_STA, cada tentativa de
// associação arrasta o SoftAP para o canal da rede alvo e de volta. A tentar
// sem parar contra uma rede inalcançável, o AP fica em churn permanente e
// desaparece do scan -- destruindo justamente a UI de campo que o AP existe
// para servir. Um tracker passa horas fora de alcance, portanto a cadência
// tem de abrir, não insistir.
#endif

// --- MQTT ----------------------------------------------------------------

#ifndef DEFAULT_MQTT_PORT
#define DEFAULT_MQTT_PORT 1883
#endif

#ifndef DEFAULT_MQTT_RETRY_MS
#define DEFAULT_MQTT_RETRY_MS 10000   // gap between broker connect attempts (see telemetry.h
                                       // for why this must never become a busy retry)
#endif

#ifndef DEFAULT_MQTT_PUBLISH_MS
#define DEFAULT_MQTT_PUBLISH_MS 10000   // state publish period while connected
#endif

#ifndef DEFAULT_MQTT_TOPIC_PREFIX
#define DEFAULT_MQTT_TOPIC_PREFIX "aguada"   // matches the gateway/node topic tree
#endif

#ifndef DEFAULT_HA_DISCOVERY_PREFIX
#define DEFAULT_HA_DISCOVERY_PREFIX "homeassistant"   // same prefix tools/bridge.py publishes under
#endif

// --- Scan de redes (seleção de SSID no portal) ----------------------------

#ifndef DEFAULT_SCAN_MAX_RESULTS
#define DEFAULT_SCAN_MAX_RESULTS 20   // teto de redes devolvidas em /api/scan.
// Limita o JSON (~50 B por entrada) e o tempo de montagem. A lista é ordenada
// por sinal no browser, portanto cortar no fim tira sempre as mais fracas.
#endif

#ifndef DEFAULT_SCAN_MAX_AGE_MS
#define DEFAULT_SCAN_MAX_AGE_MS 60000   // resultado servido de cache até esta idade.
// Um scan varre todos os canais e tira o rádio do canal do AP durante ~3 s --
// com o telemóvel ligado ao portal, isso é uma interrupção visível. Reutilizar
// o último resultado evita repetir a varredura a cada abertura da página.
#endif

// --- Portal state endpoint ------------------------------------------------

#ifndef DEFAULT_AP_RESTORE_MS
#define DEFAULT_AP_RESTORE_MS 10000   // periodo de verificacao do canal do AP.
// O C3 tem UM radio: enquanto a station tenta associar, o AP e arrastado para
// o canal da rede alvo. Com a station a falhar para sempre, o AP fica parado
// nesse canal em estado degradado e some do scan -- medido em bancada
// 2026-08-14 (ap_ch=11 com o AP configurado para 1, invisivel em 5 scans).
// A UI de campo tem prioridade sobre a sincronizacao com casa: sempre que a
// station NAO esta ligada, o AP e reposto no canal dele. Quando ela esta
// ligada, seguir o canal da rede e inevitavel e correto.
#endif

#ifndef DEFAULT_PORTAL_STALE_MS
#define DEFAULT_PORTAL_STALE_MS 15000   // page marks the node "sem dados" past this without a poll
#endif
