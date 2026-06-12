/*
 * Aguada Gateway Ethernet (ESP32 + ENC28J60) v4.0.0-eth
 *
 * ESP-NOW (ch1) -> JSON -> MQTT via Ethernet
 *
 * Publishes:
 *   - aguada/gateway/raw
 *   - aguada/{node_id}/{sensor_id}/raw
 *   - aguada/gateway/status  (online/offline)
 *
 * JSON shape matches gateway USB output (v3): SENSOR/HEARTBEAT/HELLO/GATEWAY_READY
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <EthernetENC.h>
#include <utility/Enc28J60Network.h>
#include <utility/enc28j60.h>
#include <PubSubClient.h>
#include <freertos/queue.h>
#include <esp_mac.h>
#include <string.h>

#include "espnow_gw.h"
#include "../../shared/protocol.h"

static const char *TAG = "gw_eth";
static constexpr const char *GATEWAY_ETH_FW_VERSION = FW_VERSION "-eth";

// ----- ENC28J60 wiring (ESP32 DevKit) -----
static constexpr int ETH_SCK_PIN = 18;
static constexpr int ETH_MISO_PIN = 19;
static constexpr int ETH_MOSI_PIN = 23;
static constexpr int ETH_CS_PIN = 5;

// ----- MQTT -----
static const char *MQTT_HOST = "192.168.0.177";
static constexpr uint16_t MQTT_PORT = 1883;
static const char *MQTT_USER = "aguada";
static const char *MQTT_PASS = "aguadagtw01";

static const char *TOPIC_GATEWAY_STATUS = "aguada/gateway/status";
static const char *TOPIC_GATEWAY_RAW = "aguada/gateway/raw";

static EthernetClient s_eth_client;
static PubSubClient s_mqtt(s_eth_client);
static bool s_eth_ready = false;
static bool s_spi_ready = false;
static uint32_t s_last_eth_try_ms = 0;
static uint32_t s_last_eth_diag_ms = 0;
static bool s_ready_beacon_mqtt_pending = true;

// ----- Timestamp -----
// Can be synced via serial or MQTT with {"cmd":"SETTIME","ts":...}
static int64_t s_time_offset_s = 0;

// ----- Packet queue (ESP-NOW cb -> loop) -----
#define PKT_QUEUE_LEN 24
typedef struct {
    espnow_packet_t pkt;
    uint8_t src_mac[6];
} pkt_event_t;
static QueueHandle_t s_pkt_queue = nullptr;

// Commands are sent via broadcast MAC so nodes don't need to pre-register
// the gateway as a peer. Only the target node_id will process the command.
static const uint8_t BCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static char s_serial_buf[1024];
static size_t s_serial_buf_len = 0;

static const SPISettings ENC_SPI_DIAG_FAST(20000000, MSBFIRST, SPI_MODE0);
static const SPISettings ENC_SPI_DIAG_SLOW(1000000, MSBFIRST, SPI_MODE0);

typedef struct {
    uint8_t rev_fast;
    uint8_t rev_slow;
    uint8_t estat_fast;
    uint8_t estat_slow;
    uint8_t econ1_before;
    uint8_t econ1_set;
    uint8_t econ1_clear;
    uint8_t lib_rev;
    bool write_ok;
} enc_diag_result_t;

static void read_wifi_sta_mac(uint8_t mac[6]) {
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        memset(mac, 0, 6);
    }
}

static void format_mac(const uint8_t mac[6], char *out, size_t out_len) {
    snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static const char *eth_hw_status_str(EthernetHardwareStatus status) {
    switch (status) {
        case EthernetNoHardware: return "no-hardware";
        case EthernetW5100: return "w5100";
        case EthernetW5200: return "w5200";
        case EthernetW5500: return "w5500";
        case EthernetENC28J60: return "enc28j60";
        default: return "unknown";
    }
}

static const char *eth_link_status_str(EthernetLinkStatus status) {
    switch (status) {
        case Unknown: return "unknown";
        case LinkON: return "up";
        case LinkOFF: return "down";
        default: return "?";
    }
}

static uint8_t enc_spi_read_op(const SPISettings &settings, uint8_t op, uint8_t address) {
    SPI.beginTransaction(settings);
    digitalWrite(ETH_CS_PIN, LOW);
    SPI.transfer(op | (address & ADDR_MASK));
    if (address & SPRD_MASK) {
        SPI.transfer(0x00);
    }
    uint8_t value = SPI.transfer(0x00);
    digitalWrite(ETH_CS_PIN, HIGH);
    SPI.endTransaction();
    return value;
}

static void enc_spi_write_op(const SPISettings &settings, uint8_t op, uint8_t address, uint8_t data) {
    SPI.beginTransaction(settings);
    digitalWrite(ETH_CS_PIN, LOW);
    SPI.transfer(op | (address & ADDR_MASK));
    SPI.transfer(data);
    digitalWrite(ETH_CS_PIN, HIGH);
    SPI.endTransaction();
}

static void enc_spi_soft_reset(const SPISettings &settings) {
    SPI.beginTransaction(settings);
    digitalWrite(ETH_CS_PIN, LOW);
    SPI.transfer(ENC28J60_SOFT_RESET);
    digitalWrite(ETH_CS_PIN, HIGH);
    SPI.endTransaction();
    delay(2);
}

static void enc_spi_select_bank(const SPISettings &settings, uint8_t address) {
    enc_spi_write_op(settings, ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_BSEL1 | ECON1_BSEL0);
    enc_spi_write_op(settings, ENC28J60_BIT_FIELD_SET, ECON1, (address & BANK_MASK) >> 5);
}

static uint8_t enc_spi_read_reg(const SPISettings &settings, uint8_t address) {
    enc_spi_select_bank(settings, address);
    return enc_spi_read_op(settings, ENC28J60_READ_CTRL_REG, address);
}

static enc_diag_result_t enc28j60_run_spi_diag(void) {
    enc_diag_result_t r = {};

    if (!s_spi_ready) {
        SPI.begin(ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
        pinMode(ETH_CS_PIN, OUTPUT);
        digitalWrite(ETH_CS_PIN, HIGH);
    }

    enc_spi_soft_reset(ENC_SPI_DIAG_FAST);
    r.estat_fast = enc_spi_read_reg(ENC_SPI_DIAG_FAST, ESTAT);
    r.rev_fast = enc_spi_read_reg(ENC_SPI_DIAG_FAST, EREVID);

    enc_spi_soft_reset(ENC_SPI_DIAG_SLOW);
    r.estat_slow = enc_spi_read_reg(ENC_SPI_DIAG_SLOW, ESTAT);
    r.rev_slow = enc_spi_read_reg(ENC_SPI_DIAG_SLOW, EREVID);

    r.econ1_before = enc_spi_read_reg(ENC_SPI_DIAG_SLOW, ECON1);
    enc_spi_write_op(ENC_SPI_DIAG_SLOW, ENC28J60_BIT_FIELD_SET, ECON1, ECON1_BSEL0);
    r.econ1_set = enc_spi_read_reg(ENC_SPI_DIAG_SLOW, ECON1);
    enc_spi_write_op(ENC_SPI_DIAG_SLOW, ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_BSEL0);
    r.econ1_clear = enc_spi_read_reg(ENC_SPI_DIAG_SLOW, ECON1);
    r.write_ok = ((r.econ1_set & ECON1_BSEL0) != 0) && ((r.econ1_clear & ECON1_BSEL0) == 0);

    r.lib_rev = Enc28J60Network::getrev();
    return r;
}

static void enc28j60_log_spi_diag(const enc_diag_result_t &r, const char *reason) {
    Serial.printf("[gw-eth][diag] reason=%s rev_fast=0x%02X rev_slow=0x%02X estat_fast=0x%02X estat_slow=0x%02X\n",
                  reason ? reason : "n/a",
                  r.rev_fast, r.rev_slow, r.estat_fast, r.estat_slow);
    Serial.printf("[gw-eth][diag] econ1 before=0x%02X set=0x%02X clear=0x%02X write_ok=%s lib_rev=0x%02X\n",
                  r.econ1_before, r.econ1_set, r.econ1_clear,
                  r.write_ok ? "yes" : "no", r.lib_rev);

    if ((r.rev_fast == 0x00 || r.rev_fast == 0xFF) && (r.rev_slow == 0x00 || r.rev_slow == 0xFF) && !r.write_ok) {
        Serial.println("[gw-eth][diag] SPI appears non-responsive: check VCC/GND, CS, MISO, MOSI, SCK and module power level");
    } else if ((r.rev_fast == 0x00 || r.rev_fast == 0xFF) && r.rev_slow > 0 && r.rev_slow < 0xFF) {
        Serial.println("[gw-eth][diag] ENC responds only at low SPI speed: suspect wiring quality, long jumpers or weak power");
    } else if (r.rev_slow > 0 && r.rev_slow < 0xFF && !r.write_ok) {
        Serial.println("[gw-eth][diag] Read path works but register writeback failed: suspect CS instability or SPI mode/timing issue");
    } else if (r.rev_slow > 0 && r.rev_slow < 0xFF) {
        Serial.println("[gw-eth][diag] ENC28J60 answers over SPI; if DHCP still fails, inspect cable/link/DHCP path");
    }
}

static void enc28j60_diag_if_due(const char *reason, bool force = false) {
    uint32_t now = millis();
    if (!force && (now - s_last_eth_diag_ms) < 15000UL) {
        return;
    }
    s_last_eth_diag_ms = now;
    enc_diag_result_t r = enc28j60_run_spi_diag();
    enc28j60_log_spi_diag(r, reason);
}

static uint32_t unix_now(void) {
    return (uint32_t)(s_time_offset_s + esp_timer_get_time() / 1000000LL);
}

static void serial_json(const JsonDocument &doc) {
    serializeJson(doc, Serial);
    Serial.println();
}

static void mqtt_json(const JsonDocument &doc, const char *topic) {
    if (!s_mqtt.connected()) {
        return;
    }

    char payload[320];
    size_t n = serializeJson(doc, payload, sizeof(payload));
    if (n == 0 || n >= sizeof(payload)) {
        ESP_LOGW(TAG, "JSON too large for topic %s", topic);
        return;
    }
    s_mqtt.publish(topic, payload, false);
}

static void publish_packet_json(const espnow_packet_t *pkt, const char *type) {
    JsonDocument doc;
    char nid[8];
    snprintf(nid, sizeof(nid), "0x%04X", pkt->node_id);

    doc["v"] = 3;
    doc["type"] = type;
    doc["node_id"] = nid;
    doc["sensor_id"] = pkt->sensor_id;
    doc["rssi"] = pkt->rssi;
    doc["vbat"] = pkt->vbat;
    doc["flags"] = pkt->flags;
    doc["seq"] = pkt->seq;
    doc["ts"] = unix_now();

    if (strcmp(type, "HELLO") == 0) {
        doc["fw_version"] = FW_VERSION;
        doc["num_sensors"] = pkt->distance_cm;
    } else {
        doc["distance_cm"] = (pkt->flags & FLAG_SENSOR_ERROR) ? -1 : (int)pkt->distance_cm;
        if (strcmp(type, "HEARTBEAT") == 0) {
            doc["reserved"] = pkt->reserved;
        }
    }

    serial_json(doc);
    mqtt_json(doc, TOPIC_GATEWAY_RAW);

    char topic_node[64];
    snprintf(topic_node, sizeof(topic_node), "aguada/%s/%u/raw", nid, (unsigned)pkt->sensor_id);
    mqtt_json(doc, topic_node);
}

static void fill_ready_beacon(JsonDocument &doc) {
    uint8_t mac[6];
    read_wifi_sta_mac(mac);
    char mac_str[20];
    format_mac(mac, mac_str, sizeof(mac_str));

    doc["v"] = 3;
    doc["type"] = "GATEWAY_READY";
    doc["fw"] = GATEWAY_ETH_FW_VERSION;
    doc["mac"] = mac_str;
    doc["ts"] = unix_now();
}

static void publish_ready_beacon_serial(void) {
    JsonDocument doc;
    fill_ready_beacon(doc);

    serial_json(doc);
}

static void publish_ready_beacon_mqtt(void) {
    JsonDocument doc;
    fill_ready_beacon(doc);

    mqtt_json(doc, TOPIC_GATEWAY_RAW);
    s_ready_beacon_mqtt_pending = false;
}

static uint16_t parse_node_id(const JsonVariantConst &value) {
    if (value.is<const char *>()) {
        return (uint16_t)strtoul(value.as<const char *>(), nullptr, 0);
    }
    if (value.is<uint16_t>() || value.is<int>()) {
        return value.as<uint16_t>();
    }
    return 0;
}

static void cmd_restart(uint16_t node_id) {
    if (node_id == 0) {
        ESP_LOGW(TAG, "CMD_RESTART ignored: invalid node_id");
        return;
    }

    espnow_packet_t pkt = {};
    pkt.version = PROTO_VERSION;
    pkt.type = PKT_CMD_RESTART;
    pkt.node_id = node_id;
    pkt.ttl = DEFAULT_TTL;
    esp_err_t err = gw_espnow_send(&pkt, BCAST_MAC);
    ESP_LOGI(TAG, "CMD_RESTART -> 0x%04X err=%d", node_id, (int)err);
}

static void cmd_config(uint16_t node_id, const JsonDocument &doc) {
    if (node_id == 0) {
        ESP_LOGW(TAG, "CMD_CONFIG ignored: invalid node_id");
        return;
    }

    espnow_packet_t pkt = {};
    pkt.version = PROTO_VERSION;
    pkt.type = PKT_CMD_CONFIG;
    pkt.node_id = node_id;
    pkt.ttl = DEFAULT_TTL;
    pkt.flags = FLAG_CONFIG_PENDING;

    if (doc["vbat_pin"].is<uint8_t>()) {
        pkt.sensor_id = doc["vbat_pin"].as<uint8_t>();
    }
    if (doc["vbat_enabled"].is<bool>() || doc["vbat_enabled"].is<int>()) {
        pkt.reserved = doc["vbat_enabled"].as<bool>() ? 1 : 0;
    }

    uint8_t cfg_vbat_div = 0;
    if (doc["vbat_div"].is<uint8_t>()) {
        cfg_vbat_div = doc["vbat_div"].as<uint8_t>();
    }

    uint8_t cfg_num_sensors = 0;
    bool has_num_sensors = false;
    if (doc["num_sensors"].is<uint8_t>() || doc["num_sensors"].is<int>()) {
        int ns = doc["num_sensors"].as<int>();
        if (ns >= 0 && ns <= 2) {
            has_num_sensors = true;
            cfg_num_sensors = (uint8_t)ns;
            pkt.flags |= FLAG_CFG_NUM_SENSORS;
        }
    }

    pkt.distance_cm = ((uint16_t)cfg_num_sensors << 8) | cfg_vbat_div;

    esp_err_t err = gw_espnow_send(&pkt, BCAST_MAC);
    ESP_LOGI(TAG,
             "CMD_CONFIG -> 0x%04X vbat_pin=%u vbat_enabled=%u vbat_div=%u num_sensors=%u has_ns=%d err=%d",
             node_id,
             (unsigned)pkt.sensor_id,
             (unsigned)pkt.reserved,
             (unsigned)cfg_vbat_div,
             (unsigned)cfg_num_sensors,
             has_num_sensors ? 1 : 0,
             (int)err);
}

static void process_command_doc(const JsonDocument &doc) {
    const char *cmd = doc["cmd"];
    if (!cmd) {
        return;
    }

    if (strcmp(cmd, "SETTIME") == 0) {
        int64_t ts = doc["ts"].as<int64_t>();
        s_time_offset_s = ts - (esp_timer_get_time() / 1000000LL);
        ESP_LOGI(TAG, "Time synced, unix_now=%u", unix_now());
        return;
    }

    uint16_t node_id = parse_node_id(doc["node_id"]);

    if (strcmp(cmd, "RESTART") == 0) {
        cmd_restart(node_id);
    } else if (strcmp(cmd, "CONFIG") == 0) {
        cmd_config(node_id, doc);
    } else if (strcmp(cmd, "ETH_DIAG") == 0 || strcmp(cmd, "ENC_DIAG") == 0) {
        enc28j60_diag_if_due("manual", true);
    } else if (strcmp(cmd, "OTA_START") == 0 || strcmp(cmd, "OTA") == 0) {
        ESP_LOGW(TAG, "OTA via Ethernet gateway not implemented yet");
    } else {
        ESP_LOGW(TAG, "Unknown cmd: %s", cmd);
    }
}

static void process_command_line(const char *line) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err != DeserializationError::Ok) {
        ESP_LOGW(TAG, "Bad JSON cmd: %.120s", line);
        return;
    }

    process_command_doc(doc);
}

static void serial_tick(void) {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (s_serial_buf_len > 0) {
                s_serial_buf[s_serial_buf_len] = '\0';
                process_command_line(s_serial_buf);
                s_serial_buf_len = 0;
            }
        } else if (s_serial_buf_len < sizeof(s_serial_buf) - 1) {
            s_serial_buf[s_serial_buf_len++] = c;
        }
    }
}

static void mqtt_on_message(char *topic, uint8_t *payload, unsigned int length) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err != DeserializationError::Ok) {
        ESP_LOGW(TAG, "Bad MQTT JSON on %s", topic ? topic : "<null>");
        return;
    }

    if (!doc["cmd"].is<const char *>()) {
        if (strcmp(topic, "aguada/cmd/restart") == 0) {
            doc["cmd"] = "RESTART";
        } else if (strcmp(topic, "aguada/cmd/config") == 0) {
            doc["cmd"] = "CONFIG";
        } else if (strcmp(topic, "aguada/cmd/ota") == 0) {
            doc["cmd"] = "OTA";
        } else if (strcmp(topic, "aguada/cmd/gateway") == 0) {
            // Keep user-supplied cmd if present; otherwise allow gateway-only ops
            if (doc["ts"].is<int>() || doc["ts"].is<int64_t>()) {
                doc["cmd"] = "SETTIME";
            } else if (doc["diag"].is<bool>() && doc["diag"].as<bool>()) {
                doc["cmd"] = "ETH_DIAG";
            }
        }
    }

    process_command_doc(doc);
}

// Called from WiFi/ESP-NOW task - only enqueue
static void on_recv(const espnow_packet_t *pkt, const uint8_t *src_mac) {
    pkt_event_t evt;
    evt.pkt = *pkt;
    memcpy(evt.src_mac, src_mac, 6);
    xQueueSend(s_pkt_queue, &evt, 0);
}

static void mqtt_connect_if_needed() {
    if (!s_eth_ready) return;
    if (s_mqtt.connected()) return;

    static uint32_t last_try_ms = 0;
    uint32_t now = millis();
    if (now - last_try_ms < 3000) return;
    last_try_ms = now;

    char cid[48];
    uint8_t mac[6];
    Ethernet.MACAddress(mac);
    snprintf(cid, sizeof(cid), "aguada-gw-eth-%02X%02X%02X", mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Connecting MQTT %s:%u", MQTT_HOST, MQTT_PORT);
    bool ok = s_mqtt.connect(cid, MQTT_USER, MQTT_PASS, TOPIC_GATEWAY_STATUS, 1, true, "offline");
    if (!ok) {
        ESP_LOGW(TAG, "MQTT connect failed rc=%d", s_mqtt.state());
        return;
    }

    s_mqtt.publish(TOPIC_GATEWAY_STATUS, "online", true);
    s_mqtt.subscribe("aguada/cmd/#");
    if (s_ready_beacon_mqtt_pending) {
        publish_ready_beacon_mqtt();
    }
    ESP_LOGI(TAG, "MQTT connected");
}

static bool ethernet_try_init() {
    if (!s_spi_ready) {
        SPI.begin(ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
        Ethernet.init(ETH_CS_PIN);
        s_spi_ready = true;
        Serial.printf("[gw-eth] SPI init sck=%d miso=%d mosi=%d cs=%d\n",
                      ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
    }

    uint8_t base_mac[6];
    read_wifi_sta_mac(base_mac);
    // Local-admin MAC for ENC (avoid duplicate with WiFi STA MAC)
    uint8_t eth_mac[6] = {
        (uint8_t)((base_mac[0] | 0x02) & 0xFE),
        base_mac[1], base_mac[2], base_mac[3], base_mac[4], base_mac[5]
    };
    char base_mac_str[20];
    char eth_mac_str[20];
    format_mac(base_mac, base_mac_str, sizeof(base_mac_str));
    format_mac(eth_mac, eth_mac_str, sizeof(eth_mac_str));
    Serial.printf("[gw-eth] MAC wifi=%s eth=%s\n", base_mac_str, eth_mac_str);
    EthernetHardwareStatus hw_status = Ethernet.hardwareStatus();
    EthernetLinkStatus link_status = Ethernet.linkStatus();
    Serial.printf("[gw-eth] ENC status before DHCP hw=%s link=%s\n",
                  eth_hw_status_str(hw_status),
                  eth_link_status_str(link_status));

    if (hw_status == EthernetNoHardware) {
        Serial.println("[gw-eth] ENC28J60 not detected; skipping DHCP attempt");
        enc28j60_diag_if_due("no-hardware");
        s_eth_ready = false;
        return false;
    }

    // Keep DHCP try short to avoid long setup blocking.
    Serial.println("[gw-eth] DHCP begin...");
    int rc = Ethernet.begin(eth_mac, 2500, 1000);
    Serial.printf("[gw-eth] DHCP rc=%d hw=%s link=%s\n",
                  rc,
                  eth_hw_status_str(Ethernet.hardwareStatus()),
                  eth_link_status_str(Ethernet.linkStatus()));
    if (rc == 0) {
        Serial.println("[gw-eth] DHCP failed, trying static fallback 192.168.0.240");
        IPAddress ip(192, 168, 0, 240);
        IPAddress dns(192, 168, 0, 1);
        IPAddress gw(192, 168, 0, 1);
        IPAddress mask(255, 255, 255, 0);
        Ethernet.begin(eth_mac, ip, dns, gw, mask);
        Serial.printf("[gw-eth] static fallback applied hw=%s link=%s\n",
                      eth_hw_status_str(Ethernet.hardwareStatus()),
                      eth_link_status_str(Ethernet.linkStatus()));
    }

    IPAddress ip = Ethernet.localIP();
    if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
        Serial.printf("[gw-eth] Ethernet still down (IP 0.0.0.0 hw=%s link=%s)\n",
                      eth_hw_status_str(Ethernet.hardwareStatus()),
                      eth_link_status_str(Ethernet.linkStatus()));
        s_eth_ready = false;
        return false;
    }

    Serial.printf("[gw-eth] Ethernet up IP=%u.%u.%u.%u hw=%s link=%s\n",
                  ip[0], ip[1], ip[2], ip[3],
                  eth_hw_status_str(Ethernet.hardwareStatus()),
                  eth_link_status_str(Ethernet.linkStatus()));
    s_eth_ready = true;
    return true;
}

void setup() {
    Serial.begin(115200);
    Serial.setRxBufferSize(1024);
    delay(300);
    Serial.println("[gw-eth] boot");

    s_pkt_queue = xQueueCreate(PKT_QUEUE_LEN, sizeof(pkt_event_t));
    if (!s_pkt_queue) {
        Serial.println("[gw-eth] queue create failed");
    }

    gw_espnow_init(ESPNOW_CHANNEL, on_recv);

    s_mqtt.setServer(MQTT_HOST, MQTT_PORT);
    s_mqtt.setBufferSize(768);
    s_mqtt.setCallback(mqtt_on_message);

    // Boot line / compatibility with USB gateway stream.
    // Emit early (before Ethernet init) so user can always confirm firmware start.
    publish_ready_beacon_serial();

    // First Ethernet attempt after ready beacon
    s_last_eth_try_ms = millis() - 5000;
}

void loop() {
    // Retry Ethernet bring-up until ready
    if (!s_eth_ready) {
        uint32_t now = millis();
        if (now - s_last_eth_try_ms >= 5000) {
            s_last_eth_try_ms = now;
            Serial.println("[gw-eth] trying ENC28J60...");
            ethernet_try_init();
        }
    }

    mqtt_connect_if_needed();
    s_mqtt.loop();
    if (s_eth_ready) {
        Ethernet.maintain();
    }
    serial_tick();

    pkt_event_t evt;
    while (xQueueReceive(s_pkt_queue, &evt, 0) == pdTRUE) {
        switch (evt.pkt.type) {
            case PKT_SENSOR:
                publish_packet_json(&evt.pkt, "SENSOR");
                break;
            case PKT_HEARTBEAT:
                publish_packet_json(&evt.pkt, "HEARTBEAT");
                break;
            case PKT_HELLO:
                publish_packet_json(&evt.pkt, "HELLO");
                break;
            default:
                Serial.printf("[gw-eth] unhandled pkt type=0x%02X node=0x%04X\n", evt.pkt.type, evt.pkt.node_id);
                break;
        }
    }

    delay(2);
}
