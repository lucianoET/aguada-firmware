/*
 * Aguada Node Firmware v4.0
 * ESP32-C3 SuperMini
 *
 * Single binary: num_sensors=0 → relay mode, num_sensors>=1 → sensor mode
 * All config from NVS. Reservoir math stays on the server.
 * us_dist_cm() backed by NewPing median + percentage calculation.
 * Transmission every 60 seconds.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <esp_log.h>
#include "protocol.h"
#include "node_config.h"
#include "nvs_config.h"
#include "espnow_radio.h"
#include "ultrasonic.h"
#include "ultrasonic_experiments.h"
#include "sensor_filter.h"
#include "mesh.h"
#include "crc16.h"
#include "ani_sensor.h"
#include "ani_web.h"
#include "light_sensor.h"
#include "sound_sensor.h"

// TEMT6000 ambient-light sensor (ANI/AirQ node). ADC1 pin on the C3 (GPIO0–4).
#ifndef DEFAULT_LIGHT_PIN
#define DEFAULT_LIGHT_PIN 3
#endif
// KY-037/038 ambient-noise sensor (analog A0). ADC1 pin (GPIO0–4; not 3 = light).
#ifndef DEFAULT_SOUND_PIN
#define DEFAULT_SOUND_PIN 1
#endif
#include "rgb_led.h"
#include <WebServer.h>
#include <DNSServer.h>

static const char *TAG = "node";

// ── State ────────────────────────────────────────────────────────────────────

static node_config_t g_cfg;
static uint16_t      g_seq = 0;

// Deferred config-save flag — set in ESP-NOW callback, processed in loop()
static volatile bool g_config_save_pending = false;

// ANI-01 captive portal (only active in AIR_QUALITY mode)
static AniReading    g_last_ani   = {};
static uint32_t      g_last_ani_ms = 0;   // millis() of last captive-portal refresh
static LightReading  g_last_light = {};   // TEMT6000 ambient light (ANI/AirQ node)
static SoundReading  g_last_sound = {};   // KY-037/038 ambient noise (ANI/AirQ node)
static WebServer     g_httpd(80);
static DNSServer     g_dns;

static void send_hello(bool from_button = false);
static void send_relay_env(float temp_c, float hum_pct);

// Relay mode: track seen seq numbers to avoid re-forwarding duplicates
#define RELAY_SEEN_SIZE 32
static uint32_t s_relay_seen[RELAY_SEEN_SIZE];
static uint8_t  s_relay_seen_idx = 0;

#ifndef RELAY_BTN_PIN
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define RELAY_BTN_PIN 9
#else
#define RELAY_BTN_PIN 2
#endif
#endif

#ifndef RELAY_BTN_ACTIVE_LEVEL
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define RELAY_BTN_ACTIVE_LEVEL LOW
#else
#define RELAY_BTN_ACTIVE_LEVEL HIGH
#endif
#endif

#ifndef RELAY_I2C_SDA_PIN
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define RELAY_I2C_SDA_PIN 6
#define RELAY_I2C_SCL_PIN 7
#else
#define RELAY_I2C_SDA_PIN 21
#define RELAY_I2C_SCL_PIN 22
#endif
#endif

#ifndef RELAY_I2C_SCL_PIN
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define RELAY_I2C_SCL_PIN 7
#else
#define RELAY_I2C_SCL_PIN 22
#endif
#endif

#ifndef RELAY_I2C_ADDR_SHT3X
#define RELAY_I2C_ADDR_SHT3X 0x44
#endif

#ifndef RELAY_I2C_ADDR_HD21D
#define RELAY_I2C_ADDR_HD21D 0x40
#endif

#ifndef RELAY_I2C_CLOCK_HZ
#define RELAY_I2C_CLOCK_HZ 50000
#endif

typedef enum {
    RELAY_I2C_NONE = 0,
    RELAY_I2C_SHT3X,
    RELAY_I2C_HD21D,
} relay_i2c_sensor_t;

typedef struct {
    bool     button_last;
    bool     button_pressed;
    uint32_t button_press_ms;
    uint32_t last_led_pulse_ms;
    uint32_t led_off_deadline_ms;
    uint32_t last_i2c_read_ms;
    bool     i2c_ready;
    relay_i2c_sensor_t i2c_sensor;
} relay_aux_state_t;

static relay_aux_state_t g_relay_aux = {};

static bool relay_led_conflict(void) {
    return (g_cfg.num_sensors == 0) && (DEFAULT_LED_PIN == RELAY_BTN_PIN);
}

static bool relay_button_pressed_now(void) {
    return digitalRead(RELAY_BTN_PIN) == RELAY_BTN_ACTIVE_LEVEL;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static void led_set(bool on) {
    if (!g_cfg.led_enabled) return;
    if (relay_led_conflict()) return;
#if DEFAULT_LED_ACTIVE_LOW
    digitalWrite(DEFAULT_LED_PIN, on ? LOW : HIGH);
#else
    digitalWrite(DEFAULT_LED_PIN, on ? HIGH : LOW);
#endif
}

static void led_blink(int count, int ms) {
    for (int i = 0; i < count; i++) {
        led_set(true);  delay(ms);
        led_set(false); delay(ms);
    }
}

static int8_t read_vbat(void) {
    if (!g_cfg.vbat_enabled) return -1;
    // ADC 12-bit, Vref ~3.3V — average 8 samples for noise reduction
    // vbat_div=1: direct connection; vbat_div=2: equal-resistor voltage divider
    // 4 warm-up reads first: allows ADC sampling capacitor to charge fully,
    // critical when source impedance is high (e.g. 100k–1M resistor divider)
    for (int i = 0; i < 4; i++) { analogRead(g_cfg.vbat_pin); delayMicroseconds(200); }
    int sum = 0;
    for (int i = 0; i < 8; i++) { sum += analogRead(g_cfg.vbat_pin); delayMicroseconds(200); }
    float raw = sum / 8.0f;
    float div = (float)(g_cfg.vbat_div > 0 ? g_cfg.vbat_div : 1);
    float v = (raw / 4095.0f) * 3.3f * div;
    return (int8_t)roundf(v * 10.0f);  // tenths of V, rounded
}

static bool relay_seen(uint16_t node_id, uint16_t seq) {
    uint32_t key = ((uint32_t)node_id << 16) | seq;
    for (int i = 0; i < RELAY_SEEN_SIZE; i++) {
        if (s_relay_seen[i] == key) {
            return true;
        }
    }
    s_relay_seen[s_relay_seen_idx % RELAY_SEEN_SIZE] = key;
    s_relay_seen_idx++;
    return false;
}

static bool relay_i2c_read_sht3x(float *temp_c, float *hum_pct) {
    Wire.beginTransmission(RELAY_I2C_ADDR_SHT3X);
    Wire.write(0x24);  // high repeatability
    Wire.write(0x00);  // clock stretching disabled
    if (Wire.endTransmission() != 0) {
        return false;
    }

    delay(20);  // conversion time (single-shot)

    if (Wire.requestFrom((uint8_t)RELAY_I2C_ADDR_SHT3X, (uint8_t)6) != 6) {
        return false;
    }

    uint16_t raw_t  = ((uint16_t)Wire.read() << 8) | Wire.read();
    (void)Wire.read();  // crc temp
    uint16_t raw_rh = ((uint16_t)Wire.read() << 8) | Wire.read();
    (void)Wire.read();  // crc rh

    *temp_c  = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    *hum_pct = 100.0f * ((float)raw_rh / 65535.0f);
    return true;
}

static bool relay_i2c_read_hd21d(float *temp_c, float *hum_pct) {
    // Temperature command (no hold master)
    Wire.beginTransmission(RELAY_I2C_ADDR_HD21D);
    Wire.write(0xF3);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    delay(60);
    if (Wire.requestFrom((uint8_t)RELAY_I2C_ADDR_HD21D, (uint8_t)3) != 3) {
        return false;
    }
    uint16_t raw_t = ((uint16_t)Wire.read() << 8) | Wire.read();
    (void)Wire.read(); // crc
    raw_t &= 0xFFFC;

    // Humidity command (no hold master)
    Wire.beginTransmission(RELAY_I2C_ADDR_HD21D);
    Wire.write(0xF5);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    delay(20);
    if (Wire.requestFrom((uint8_t)RELAY_I2C_ADDR_HD21D, (uint8_t)3) != 3) {
        return false;
    }
    uint16_t raw_rh = ((uint16_t)Wire.read() << 8) | Wire.read();
    (void)Wire.read(); // crc
    raw_rh &= 0xFFFC;

    *temp_c  = -46.85f + 175.72f * ((float)raw_t / 65536.0f);
    *hum_pct = -6.0f + 125.0f * ((float)raw_rh / 65536.0f);
    if (*hum_pct < 0.0f) *hum_pct = 0.0f;
    if (*hum_pct > 100.0f) *hum_pct = 100.0f;
    return true;
}

static uint8_t relay_i2c_probe_addr(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission();
}

static void relay_i2c_bus_recovery(void) {
    pinMode(RELAY_I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(RELAY_I2C_SCL_PIN, INPUT_PULLUP);

    int sda_before = digitalRead(RELAY_I2C_SDA_PIN);
    int scl_before = digitalRead(RELAY_I2C_SCL_PIN);
    Serial.printf("I2C idle before recovery: sda=%d scl=%d\n", sda_before, scl_before);

    if (sda_before == HIGH && scl_before == HIGH) {
        return;
    }

    // Common I2C recovery: 9 SCL pulses while SDA released, then STOP
    pinMode(RELAY_I2C_SCL_PIN, OUTPUT_OPEN_DRAIN);
    digitalWrite(RELAY_I2C_SCL_PIN, HIGH);
    delayMicroseconds(10);
    for (int i = 0; i < 9; i++) {
        digitalWrite(RELAY_I2C_SCL_PIN, LOW);
        delayMicroseconds(10);
        digitalWrite(RELAY_I2C_SCL_PIN, HIGH);
        delayMicroseconds(10);
    }

    pinMode(RELAY_I2C_SDA_PIN, OUTPUT_OPEN_DRAIN);
    digitalWrite(RELAY_I2C_SDA_PIN, LOW);
    delayMicroseconds(10);
    digitalWrite(RELAY_I2C_SCL_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(RELAY_I2C_SDA_PIN, HIGH);
    delayMicroseconds(10);

    pinMode(RELAY_I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(RELAY_I2C_SCL_PIN, INPUT_PULLUP);
    Serial.printf("I2C idle after recovery: sda=%d scl=%d\n",
                  digitalRead(RELAY_I2C_SDA_PIN),
                  digitalRead(RELAY_I2C_SCL_PIN));
}

static void relay_aux_init(void) {
    pinMode(RELAY_BTN_PIN, RELAY_BTN_ACTIVE_LEVEL == LOW ? INPUT_PULLUP : INPUT);
    g_relay_aux.button_last = relay_button_pressed_now();
    g_relay_aux.button_pressed = false;

    // On an ANI node the I2C bus belongs to the ENS160+AHT21 (ani_sensor.cpp).
    // relay_aux must NOT touch it: its blocking full-bus scan stalls boot ~126 s
    // and its bus recovery / Wire.begin() corrupt the ANI bus state. Keep only the
    // button + status LED + keepalive here; skip all aux-I2C work.
    if (g_cfg.sensor_type == SENSOR_TYPE_AIR_QUALITY) {
        g_relay_aux.i2c_ready  = false;
        g_relay_aux.i2c_sensor = RELAY_I2C_NONE;
        Serial.printf("RELAY_AUX btn=%d active=%s i2c=skipped(ANI owns bus) led_conflict=%s\n",
                      RELAY_BTN_PIN,
                      RELAY_BTN_ACTIVE_LEVEL == LOW ? "LOW" : "HIGH",
                      relay_led_conflict() ? "yes" : "no");
        return;
    }

    relay_i2c_bus_recovery();
    pinMode(RELAY_I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(RELAY_I2C_SCL_PIN, INPUT_PULLUP);
    Wire.begin(RELAY_I2C_SDA_PIN, RELAY_I2C_SCL_PIN);
    Wire.setClock(RELAY_I2C_CLOCK_HZ);
    delay(50);
    uint8_t err_htu = relay_i2c_probe_addr(RELAY_I2C_ADDR_HD21D);
    uint8_t err_sht = relay_i2c_probe_addr(RELAY_I2C_ADDR_SHT3X);
    if (err_htu == 0) {
        g_relay_aux.i2c_ready = true;
        g_relay_aux.i2c_sensor = RELAY_I2C_HD21D;
    } else {
        if (err_sht == 0) {
            g_relay_aux.i2c_ready = true;
            g_relay_aux.i2c_sensor = RELAY_I2C_SHT3X;
        } else {
            g_relay_aux.i2c_ready = false;
            g_relay_aux.i2c_sensor = RELAY_I2C_NONE;
        }
    }

    const char *sensor_name = (g_relay_aux.i2c_sensor == RELAY_I2C_HD21D) ? "hd21d" :
                              (g_relay_aux.i2c_sensor == RELAY_I2C_SHT3X) ? "sht3x" : "none";

    // I2C bus scan — list all responding addresses for diagnostics
    Serial.printf("I2C cfg: clock=%luHz probe_0x40=%u probe_0x44=%u\n",
                  (unsigned long)RELAY_I2C_CLOCK_HZ,
                  (unsigned)err_htu,
                  (unsigned)err_sht);
    Serial.printf("I2C scan (sda=%d scl=%d): ", RELAY_I2C_SDA_PIN, RELAY_I2C_SCL_PIN);
    bool i2c_any = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (relay_i2c_probe_addr(addr) == 0) {
            Serial.printf("0x%02X ", addr);
            i2c_any = true;
        }
    }
    if (!i2c_any) Serial.printf("none");
    Serial.printf("\n");

    ESP_LOGI(TAG, "Relay aux: btn=%d i2c(sda=%d scl=%d) sensor=%s detected=%s",
             RELAY_BTN_PIN,
             RELAY_I2C_SDA_PIN,
             RELAY_I2C_SCL_PIN,
             sensor_name,
             g_relay_aux.i2c_ready ? "yes" : "no");
    Serial.printf("RELAY_AUX btn=%d active=%s i2c_sda=%d i2c_scl=%d sensor=%s detected=%s led_conflict=%s\n",
                  RELAY_BTN_PIN,
                  RELAY_BTN_ACTIVE_LEVEL == LOW ? "LOW" : "HIGH",
                  RELAY_I2C_SDA_PIN,
                  RELAY_I2C_SCL_PIN,
                  sensor_name,
                  g_relay_aux.i2c_ready ? "yes" : "no",
                  relay_led_conflict() ? "yes" : "no");
}

static void relay_aux_tick(void) {
    uint32_t now = millis();
    static uint32_t last_keepalive_ms = 0;
    static int last_btn_raw = -1;

    // Periodic HELLO keepalive every 60s — keeps node online in bridge/HA
    if (now - last_keepalive_ms >= 60000) {
        last_keepalive_ms = now;
        send_hello(false);
    }

    // non-blocking status pulse: 30 ms every 2 s
    if (now - g_relay_aux.last_led_pulse_ms >= 2000) {
        g_relay_aux.last_led_pulse_ms = now;
        g_relay_aux.led_off_deadline_ms = now + 30;
        led_set(true);
    }
    if (g_relay_aux.led_off_deadline_ms != 0 && now >= g_relay_aux.led_off_deadline_ms) {
        g_relay_aux.led_off_deadline_ms = 0;
        led_set(false);
    }

    // button: short press -> HELLO, long press (>=5s) -> restart
    int raw = digitalRead(RELAY_BTN_PIN);
    bool pressed = relay_button_pressed_now();
    if (raw != last_btn_raw) {
        last_btn_raw = raw;
        Serial.printf("BTN raw=%d pressed=%d active=%s\n",
                      raw, pressed ? 1 : 0,
                      RELAY_BTN_ACTIVE_LEVEL == LOW ? "LOW" : "HIGH");
    }
    if (pressed && !g_relay_aux.button_last) {
        g_relay_aux.button_press_ms = now;
        g_relay_aux.button_pressed = true;
    }
    if (!pressed && g_relay_aux.button_last && g_relay_aux.button_pressed) {
        uint32_t dt = now - g_relay_aux.button_press_ms;
        g_relay_aux.button_pressed = false;
        if (dt >= 5000) {
            ESP_LOGW(TAG, "Relay button long-press (%lums): restart", (unsigned long)dt);
            delay(100);
            esp_restart();
        } else if (dt >= 40) {
            ESP_LOGI(TAG, "Relay button short-press (%lums): HELLO", (unsigned long)dt);
            send_hello(true);
        }
    }
    g_relay_aux.button_last = pressed;

    // optional local I2C telemetry log every 30s
    if (g_relay_aux.i2c_ready && (now - g_relay_aux.last_i2c_read_ms >= 30000)) {
        g_relay_aux.last_i2c_read_ms = now;
        float temp_c = 0.0f, hum = 0.0f;
        bool ok = false;
        if (g_relay_aux.i2c_sensor == RELAY_I2C_HD21D) {
            ok = relay_i2c_read_hd21d(&temp_c, &hum);
        } else if (g_relay_aux.i2c_sensor == RELAY_I2C_SHT3X) {
            ok = relay_i2c_read_sht3x(&temp_c, &hum);
        }

        if (ok) {
            const char *sensor_name = (g_relay_aux.i2c_sensor == RELAY_I2C_HD21D) ? "HD21D" : "SHT3x";
            ESP_LOGI(TAG, "Relay I2C %s: T=%.1fC RH=%.1f%%", sensor_name, temp_c, hum);
            send_relay_env(temp_c, hum);
        } else {
            ESP_LOGW(TAG, "Relay I2C read failed");
        }
    }
}

// ── Build & send a packet ─────────────────────────────────────────────────────

static void send_packet(uint8_t type, uint8_t sensor_id, uint16_t distance_cm, uint8_t percent) {
    espnow_packet_t pkt = {};
    pkt.version     = PROTO_VERSION;
    pkt.type        = type;
    pkt.node_id     = g_cfg.node_id;
    pkt.sensor_id   = sensor_id;
    pkt.ttl         = g_cfg.ttl_max;
    pkt.seq         = g_seq++;
    pkt.distance_cm = distance_cm;
    pkt.rssi        = 0; // filled by receiver
    pkt.vbat        = read_vbat();
    pkt.flags       = 0;
    pkt.reserved    = percent;

    if (pkt.vbat != -1 && pkt.vbat < VBAT_LOW_THRESHOLD)
        pkt.flags |= FLAG_LOW_BATTERY;
    if (distance_cm == DISTANCE_ERROR)
        pkt.flags |= FLAG_SENSOR_ERROR;

    espnow_send(&pkt);

    led_blink(1, 30);
    ESP_LOGI(TAG, "TX type=0x%02X sid=%d dist=%d seq=%d", type, sensor_id, distance_cm, pkt.seq - 1);
}

static void send_hello(bool from_button) {
    espnow_packet_t pkt = {};
    pkt.version     = PROTO_VERSION;
    pkt.type        = PKT_HELLO;
    pkt.node_id     = g_cfg.node_id;
    pkt.sensor_id   = 0;
    pkt.ttl         = g_cfg.ttl_max;
    pkt.seq         = g_seq++;
    pkt.distance_cm = g_cfg.num_sensors;  // reuse field to carry num_sensors in HELLO
    pkt.vbat        = read_vbat();
    if (from_button) pkt.flags |= FLAG_BTN_HELLO;
    espnow_send(&pkt);
    ESP_LOGI(TAG, "TX HELLO node_id=0x%04X num_sensors=%d btn=%d", g_cfg.node_id, g_cfg.num_sensors, from_button);
}

// Send relay I2C env telemetry as a HEARTBEAT with sensor_id=SENSOR_ID_ENV.
// Encoding: distance_cm = (int)(temp_c*10)+1000, reserved = hum% (uint8).
static void send_relay_env(float temp_c, float hum_pct) {
    espnow_packet_t pkt = {};
    pkt.version     = PROTO_VERSION;
    pkt.type        = PKT_HEARTBEAT;
    pkt.node_id     = g_cfg.node_id;
    pkt.sensor_id   = SENSOR_ID_ENV;
    pkt.ttl         = g_cfg.ttl_max;
    pkt.seq         = g_seq++;
    int enc = (int)(temp_c * 10.0f) + 1000;
    pkt.distance_cm = (uint16_t)(enc < 0 ? 0 : enc > 65000 ? 65000 : enc);
    float h = hum_pct < 0.0f ? 0.0f : hum_pct > 100.0f ? 100.0f : hum_pct;
    pkt.reserved    = (uint8_t)h;
    pkt.vbat        = read_vbat();
    espnow_send(&pkt);
    ESP_LOGI(TAG, "TX ENV T=%.1f H=%.0f enc=%u hum=%u", temp_c, hum_pct, pkt.distance_cm, pkt.reserved);
}

// ── Receive callback ──────────────────────────────────────────────────────────

static void on_recv(const espnow_packet_t *pkt, const uint8_t *src_mac) {
    // Update neighbor table
    mesh_update_neighbor(pkt->node_id, src_mac, pkt->rssi, pkt->ttl);

    // Commands addressed to us
    if (pkt->node_id == g_cfg.node_id || pkt->node_id == 0xFFFF) {
        if (pkt->type == PKT_CMD_RESTART) {
            ESP_LOGI(TAG, "CMD_RESTART received");
            delay(100);
            esp_restart();
        }
        else if (pkt->type == PKT_CMD_CONFIG) {
            ESP_LOGI(TAG, "CMD_CONFIG received, scheduling save...");
            // Gateway encodes vbat config in spare fields:
            //   pkt->sensor_id  → vbat_pin     (0 = don't update)
            //   pkt->reserved   → vbat_enabled  (0=unchanged, 1=false, 2=true)
            //   pkt->distance_cm low byte  → vbat_div    (0 = don't update)
            //   pkt->distance_cm high byte → num_sensors (0,1,2) when FLAG_CFG_NUM_SENSORS is set
            if (pkt->sensor_id > 0)
                g_cfg.vbat_pin = pkt->sensor_id;
            if (pkt->reserved != 0)
                g_cfg.vbat_enabled = (pkt->reserved == 2);
            uint8_t cfg_vbat_div = (uint8_t)(pkt->distance_cm & 0xFF);
            if (cfg_vbat_div > 0 && cfg_vbat_div <= 8)
                g_cfg.vbat_div = cfg_vbat_div;

            if (pkt->flags & FLAG_CFG_NUM_SENSORS) {
                uint8_t cfg_num_sensors = (uint8_t)((pkt->distance_cm >> 8) & 0xFF);
                if (cfg_num_sensors <= 2) {
                    g_cfg.num_sensors = cfg_num_sensors;
                    g_cfg.sensor[0].enabled = (cfg_num_sensors >= 1);
                    g_cfg.sensor[1].enabled = (cfg_num_sensors >= 2);
                }
            }
            // Defer NVS write to loop() — NVS must not be called from WiFi task
            g_config_save_pending = true;
        }
    }

    // Relay: forward packets from other nodes if TTL > 0 and not already seen.
    // Active when num_sensors==0 (relay-only) OR relay_enabled flag is set (ANI sensor+relay).
    if (g_cfg.num_sensors == 0 || g_cfg.relay_enabled) {
        if (pkt->node_id != g_cfg.node_id && pkt->ttl > 0) {
            if (!relay_seen(pkt->node_id, pkt->seq)) {
                espnow_packet_t relay_pkt = *pkt;
                relay_pkt.ttl--;
                relay_pkt.flags |= FLAG_IS_RELAY;
                espnow_send(&relay_pkt);
                ESP_LOGD(TAG, "Relay: 0x%04X seq=%d ttl=%d", pkt->node_id, pkt->seq, relay_pkt.ttl);
            }
        }
    }
}

// ── ANI-01 air quality tick ───────────────────────────────────────────────────

// Local measurement cadence for the captive portal. Decoupled from the (slower)
// ESP-NOW send cadence so the local page stays "live" without adding radio traffic.
#define ANI_MEASURE_MS 8000

static void ani_tick(void) {
    static uint32_t last_measure_ms = 0;
    static uint32_t last_send_ms    = 0;
    static bool     first_tick      = true;
    uint32_t now = millis();

    uint32_t send_ms = (uint32_t)g_cfg.interval_send_s * 1000;
    if (send_ms == 0) send_ms = 120000;

    // Gate on the fast local cadence; the first tick runs immediately so the
    // captive portal and the first ESP-NOW packet have real values within seconds.
    if (!first_tick && now - last_measure_ms < ANI_MEASURE_MS) return;
    last_measure_ms = now;

    bool do_send = first_tick || (now - last_send_ms >= send_ms);
    first_tick = false;

    AniReading r = ani_read();
    g_last_ani    = r;     // always refresh captive portal (valid flag included)
    g_last_ani_ms = now;
    g_last_light  = light_read();   // TEMT6000 — refresh every local tick too
    ESP_LOGI(TAG, "LIGHT raw=%u mv=%umV lux=%u pct=%u%%",
             g_last_light.raw, g_last_light.mv, g_last_light.lux, g_last_light.pct);
    g_last_sound  = sound_read();   // KY-037/038 — sample audio window (~50ms)
    ESP_LOGI(TAG, "SOUND mean=%u rms=%u vpp=%u db~%u",
             g_last_sound.mean, g_last_sound.rms, g_last_sound.vpp, g_last_sound.db);

    if (!r.valid) {
        ESP_LOGW(TAG, "ANI read failed");
        led_blink(3, 200);   // 3 blinks lentos = falha de leitura
        if (do_send) {
            last_send_ms = now;
            // Send error packet so bridge knows sensor is alive but failing
            espnow_packet_t pkt = {};
            pkt.version     = PROTO_VERSION;
            pkt.type        = PKT_SENSOR;
            pkt.node_id     = g_cfg.node_id;
            pkt.sensor_id   = 1;
            pkt.ttl         = g_cfg.ttl_max;
            pkt.seq         = g_seq++;
            pkt.distance_cm = DISTANCE_ERROR;
            pkt.vbat        = read_vbat();
            pkt.flags       = FLAG_SENSOR_ERROR | FLAG_IS_AIR_QUALITY;
            espnow_send(&pkt);
        }
        return;
    }

    if (!do_send) return;   // valid reading already cached for the page; skip radio
    last_send_ms = now;

    // Packet 1: eco2 + aqi (PKT_SENSOR, sensor_id=1, FLAG_IS_AIR_QUALITY)
    {
        espnow_packet_t pkt = {};
        pkt.version     = PROTO_VERSION;
        pkt.type        = PKT_SENSOR;
        pkt.node_id     = g_cfg.node_id;
        pkt.sensor_id   = 1;
        pkt.ttl         = g_cfg.ttl_max;
        pkt.seq         = g_seq++;
        pkt.distance_cm = r.eco2;
        pkt.reserved    = r.aqi;
        pkt.vbat        = read_vbat();
        pkt.flags       = FLAG_IS_AIR_QUALITY;
        espnow_send(&pkt);
        ESP_LOGI(TAG, "ANI TX eco2=%uppm aqi=%u", r.eco2, r.aqi);
    }

    // Packet 2: temp + hum (existing SENSOR_ID_ENV encoding, reuses send_relay_env)
    send_relay_env(r.temp_c, r.hum_pct);

    led_blink(1, 30);   // 1 blink curto = leitura enviada com sucesso
}

// ── Sensor loop ───────────────────────────────────────────────────────────────

static sensor_filter_t g_filters[2];
static uint16_t        g_last_sent_cm[2];
static uint32_t        g_last_measure_ms[2];
static uint32_t        g_last_send_ms[2];

static void sensor_tick(uint8_t idx) {
    if (!g_cfg.sensor[idx].enabled) return;

    uint32_t now        = millis();
    uint32_t measure_ms = (uint32_t)g_cfg.interval_measure_s * 1000;
    uint32_t send_ms    = (uint32_t)g_cfg.interval_send_s    * 1000;
    if (measure_ms == 0) measure_ms = DEFAULT_MEASURE_S * 1000;
    if (send_ms    == 0) send_ms    = DEFAULT_SEND_S    * 1000;

    if (now - g_last_measure_ms[idx] < measure_ms) return;
    g_last_measure_ms[idx] = now;

    uint8_t sid = idx + 1;

    UltrasonicReading r = us_dist_cm(
        g_cfg.sensor[idx].trig_pin,
        g_cfg.sensor[idx].echo_pin,
        5, 0, HC_MAX_RANGE_CM
    );

    if (!r.valid) {
        // Send error packet only once per send interval to avoid flooding
        if (now - g_last_send_ms[idx] >= send_ms) {
            g_last_send_ms[idx] = now;
            send_packet(PKT_SENSOR, sid, DISTANCE_ERROR, 0);
            Serial.printf("S%d dist: ERR\n", sid);
        }
        // Forget last good value: heartbeat_tick() skips DISTANCE_ERROR, so a dead
        // sensor stops being reported as a frozen level. Next valid read resends.
        g_last_sent_cm[idx] = DISTANCE_ERROR;
        return;
    }

    // Layer 1-2: outlier rejection + moving average
    uint16_t filtered = filter_update(&g_filters[idx], r.distance_cm);
    if (filtered == 0) return; // rejected by filter

    // Layer 3: send if threshold exceeded OR max interval elapsed
    int delta = abs((int)filtered - (int)g_last_sent_cm[idx]);
    bool threshold_ok = (g_last_sent_cm[idx] == DISTANCE_ERROR) ||
                        (delta >= (int)g_cfg.filter_threshold_cm);
    bool interval_ok  = (now - g_last_send_ms[idx] >= send_ms);

    if (threshold_ok || interval_ok) {
        g_last_send_ms[idx]  = now;
        g_last_sent_cm[idx]  = filtered;
        send_packet(PKT_SENSOR, sid, filtered, 0);
        Serial.printf("S%d dist: %u cm\n", sid, filtered);
    }
}

static void heartbeat_tick(void) {
    static uint32_t last_hb_ms = 0;
    uint32_t now  = millis();
    uint32_t hb_ms = (uint32_t)g_cfg.heartbeat_s * 1000;
    if (hb_ms == 0) hb_ms = DEFAULT_HEARTBEAT_S * 1000;

    if (now - last_hb_ms < hb_ms) return;
    last_hb_ms = now;

    for (uint8_t i = 0; i < 2; i++) {
        if (!g_cfg.sensor[i].enabled) continue;
        if (g_last_sent_cm[i] == DISTANCE_ERROR) continue; // no valid reading yet
        send_packet(PKT_HEARTBEAT, i + 1, g_last_sent_cm[i], 0);
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup(void) {
    Serial.begin(115200);
    delay(200);

    // Get node_id from MAC — read directly from eFuse (works on clones where
    // WiFi.macAddress() returns 00:00:00:00:00:00 before WiFi is fully started)
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    WiFi.mode(WIFI_STA);  // still needed for ESP-NOW
    Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

#if defined(DEFAULT_NODE_ID) && DEFAULT_NODE_ID != 0
    uint16_t node_id = DEFAULT_NODE_ID;
    Serial.printf("NOTE: node_id overridden by build flag: 0x%04X\n", node_id);
#else
    uint16_t node_id = ((uint16_t)mac[4] << 8) | mac[5];
    if (node_id == 0x0000) {
        // Some ESP32 clones have blank efuse — derive from full MAC bytes
        node_id = ((uint16_t)(mac[0] ^ mac[2] ^ mac[4]) << 8)
                | ((uint16_t)(mac[1] ^ mac[3] ^ mac[5]));
        if (node_id == 0x0000) node_id = 0xDEAD;  // absolute fallback
        Serial.printf("WARN: last 2 MAC bytes are 0x0000, derived node_id=0x%04X\n", node_id);
    }
#endif

    ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X  node_id=0x%04X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], node_id);

    // Load config from NVS
    nvs_config_load(&g_cfg, node_id);

    // LED init after config load so relay mode can avoid button pin conflicts
    if (!relay_led_conflict()) {
        pinMode(DEFAULT_LED_PIN, OUTPUT);
        led_set(false);
    } else {
        ESP_LOGW(TAG, "LED disabled: DEFAULT_LED_PIN=%d conflicts with RELAY_BTN_PIN=%d",
                 DEFAULT_LED_PIN, RELAY_BTN_PIN);
        Serial.printf("LED disabled: pin conflict on GPIO%d\n", RELAY_BTN_PIN);
    }

    // Init mesh
    mesh_init();

    // Init ESP-NOW
    espnow_init(g_cfg.espnow_channel, on_recv, nullptr);

    if (g_cfg.num_sensors == 0 && g_cfg.sensor_type != SENSOR_TYPE_AIR_QUALITY) {
        ESP_LOGI(TAG, "Mode: RELAY");
        Serial.printf("RELAY_BOOT node=0x%04X ttl=%u channel=%u\n",
                      g_cfg.node_id, g_cfg.ttl_max, g_cfg.espnow_channel);
        relay_aux_init();
        led_blink(3, 100);
    } else if (g_cfg.sensor_type == SENSOR_TYPE_AIR_QUALITY) {
        ESP_LOGI(TAG, "Mode: ANI (air quality%s)",
                 g_cfg.relay_enabled ? "+relay" : "");
        Serial.printf("ANI_BOOT node=0x%04X relay=%d sda=%d scl=%d\n",
                      g_cfg.node_id, g_cfg.relay_enabled,
                      g_cfg.i2c_sda_pin, g_cfg.i2c_scl_pin);
        bool sensors_ok = ani_init(g_cfg.i2c_sda_pin, g_cfg.i2c_scl_pin);
        if (!sensors_ok) {
            led_blink(5, 100);   // 5 blinks rápidos = erro de sensor no boot
            ESP_LOGW(TAG, "ANI: one or both sensors not detected on I2C bus");
        }
        light_init(DEFAULT_LIGHT_PIN);   // TEMT6000 ambient light (analog, ADC1)
        sound_init(DEFAULT_SOUND_PIN);   // KY-037/038 ambient noise (analog, ADC1)
        relay_aux_init();

        // ── Captive portal AP ─────────────────────────────────────────────────
        // Start AP on same channel as ESP-NOW so both can coexist (WIFI_AP_STA).
        char ap_ssid[20];
        snprintf(ap_ssid, sizeof(ap_ssid), "ANI-%04X", g_cfg.node_id);
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(ap_ssid, nullptr, g_cfg.espnow_channel);
        ESP_LOGI(TAG, "AP started: SSID=%s  IP=%s", ap_ssid,
                 WiFi.softAPIP().toString().c_str());

        // DNS: redirect all queries to our IP (captive portal)
        g_dns.start(53, "*", WiFi.softAPIP());

        // HTTP: serve data JSON + HTML page
        g_httpd.on("/data", HTTP_GET, []() {
            bool aht_ok = false, ens_ok = false;
            ani_status(&aht_ok, &ens_ok);
            uint32_t up_ms  = millis();
            uint32_t age_s  = g_last_ani_ms ? (up_ms - g_last_ani_ms) / 1000 : 0;
            bool     warmup = up_ms < 180000UL;   // ENS160 initial ~3 min startup
            char buf[480];
            snprintf(buf, sizeof(buf),
                "{\"valid\":%s,\"eco2\":%u,\"aqi\":%u,\"tvoc\":%u,"
                "\"temp_c\":%.1f,\"hum_pct\":%.0f,"
                "\"light\":%s,\"lux\":%u,\"lpct\":%u,\"lraw\":%u,"
                "\"sound\":%s,\"db\":%u,\"srms\":%u,\"svpp\":%u,"
                "\"aht\":%s,\"ens\":%s,\"warmup\":%s,"
                "\"node\":\"%04X\",\"fw\":\"%s\",\"up\":%lu,\"heap\":%lu,\"age\":%lu}",
                g_last_ani.valid ? "true" : "false",
                g_last_ani.eco2, g_last_ani.aqi, g_last_ani.tvoc,
                g_last_ani.temp_c, g_last_ani.hum_pct,
                g_last_light.valid ? "true" : "false",
                g_last_light.lux, g_last_light.pct, g_last_light.raw,
                g_last_sound.valid ? "true" : "false",
                g_last_sound.db, g_last_sound.rms, g_last_sound.vpp,
                aht_ok ? "true" : "false",
                ens_ok ? "true" : "false",
                warmup ? "true" : "false",
                g_cfg.node_id, g_cfg.fw_version,
                (unsigned long)(up_ms / 1000),
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)age_s);
            g_httpd.sendHeader("Access-Control-Allow-Origin", "*");
            g_httpd.send(200, "application/json", buf);
        });
        g_httpd.on("/", HTTP_GET, []() {
            g_httpd.send_P(200, "text/html; charset=utf-8", ANI_WEB_HTML);
        });
        g_httpd.onNotFound([]() {
            g_httpd.sendHeader("Location", "http://192.168.4.1/");
            g_httpd.send(302, "text/plain", "");
        });
        g_httpd.begin();
        // ─────────────────────────────────────────────────────────────────────

        led_blink(2, 100);   // 2 blinks = boot ANI OK
    } else {
        ESP_LOGI(TAG, "Mode: SENSOR (num_sensors=%d)", g_cfg.num_sensors);

        // Init ultrasonic sensors + per-sensor filter state
        for (int i = 0; i < 2; i++) {
            filter_init(&g_filters[i], g_cfg.filter_window, g_cfg.filter_outlier_cm);
            g_last_sent_cm[i]  = DISTANCE_ERROR;
            g_last_measure_ms[i] = 0;
            g_last_send_ms[i]  = 0;
            if (g_cfg.sensor[i].enabled) {
                ultrasonic_init(g_cfg.sensor[i].trig_pin, g_cfg.sensor[i].echo_pin);
                ESP_LOGI(TAG, "  Sensor %d: TRIG=%d ECHO=%d", i+1,
                         g_cfg.sensor[i].trig_pin, g_cfg.sensor[i].echo_pin);
            }
        }
        led_blink(2, 100);
    }

    // Announce on boot
    send_hello();
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop(void) {
    // Handle deferred config save (cannot call NVS from ESP-NOW WiFi callback)
    if (g_config_save_pending) {
        g_config_save_pending = false;
        esp_err_t err = nvs_config_save(&g_cfg);
        ESP_LOGI(TAG, "CMD_CONFIG save: vbat_pin=%d vbat_enabled=%d err=%d",
                 g_cfg.vbat_pin, g_cfg.vbat_enabled, (int)err);
        delay(100);
        esp_restart();
    }

    if (g_cfg.sensor_type == SENSOR_TYPE_AIR_QUALITY) {
        g_dns.processNextRequest();
        g_httpd.handleClient();
        ani_tick();
        relay_aux_tick();
#ifdef SOUND_DEBUG
        // Raw A0 diagnostic — longer continuous read once per second.
        // Reports DC bias (mean/mV), peak-to-peak, and avg sample-to-sample delta
        // (AC activity). A powered KY mic biases ~1.65V (raw ~2048); avgdelta/vpp
        // should jump when you clap.
        static uint32_t s_snd_dbg_ms = 0;
        if (millis() - s_snd_dbg_ms >= 1000) {
            s_snd_dbg_ms = millis();
            const int N = 4000;
            uint16_t mn = 4095, mx = 0, prev = analogRead(DEFAULT_SOUND_PIN);
            uint32_t sum = 0; uint64_t dsum = 0;
            for (int i = 0; i < N; i++) {
                uint16_t v = analogRead(DEFAULT_SOUND_PIN);
                sum += v;
                if (v < mn) mn = v;
                if (v > mx) mx = v;
                dsum += (v > prev) ? (v - prev) : (prev - v);
                prev = v;
            }
            uint32_t mvsum = 0;
            for (int i = 0; i < 16; i++) mvsum += analogReadMilliVolts(DEFAULT_SOUND_PIN);
            Serial.printf("RAW n=%d min=%u max=%u mean=%lu mv=%lumV vpp=%u avgdelta=%.1f\n",
                          N, mn, mx, (unsigned long)(sum / N),
                          (unsigned long)(mvsum / 16), (uint16_t)(mx - mn),
                          (float)dsum / N);
        }
#endif
    } else if (g_cfg.num_sensors > 0) {
        for (uint8_t i = 0; i < 2; i++) {
            sensor_tick(i);
        }
        heartbeat_tick();
    } else {
        relay_aux_tick();
    }
    
    delay(50);
}
