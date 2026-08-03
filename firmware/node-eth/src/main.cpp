// Aguada — node-eth: wired Ethernet reservoir node (Arduino Nano + ENC28J60 + HC-SR04)
//
// Node 0xEE02 (CAV, Castelo de Incêndio) publishes integer-only, gateway-shaped
// sensor JSON straight to MQTT on "aguada/raw/0xEE02/1". bridge.py routes that
// topic through the existing _handle_sensor pipeline, so all level/percent/
// volume math stays in tools/reservoirs.yaml + bridge.py — never on this node.
//
// Hardware constraints: ATmega328 (classic Nano bootloader) — 2 KB RAM, 30 KB
// usable flash. No String, no floating point, no sprintf anywhere in this
// file; PSTR()/snprintf_P keep format literals in flash.

#include <Arduino.h>
#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <avr/wdt.h>

// ── Debug ────────────────────────────────────────────────────────────────
// DEBUG=0 links no serial code at all (flash savings, D-02). Set to 1 and
// reflash to see boot/connect/measure traces on the 9600-baud monitor.
#define DEBUG 0
#if DEBUG
  #define DBG(...)   Serial.print(__VA_ARGS__)
  #define DBGLN(...) Serial.println(__VA_ARGS__)
#else
  #define DBG(...)
  #define DBGLN(...)
#endif

// ── Pins ─────────────────────────────────────────────────────────────────
static const uint8_t TRIG_PIN = 6;
static const uint8_t ECHO_PIN = 7;
// ENC28J60 uses hardware SPI D11/D12/D13 with CS on D10 (UIPEthernet default).

// ── Identity ─────────────────────────────────────────────────────────────
#define NODE_ID_STR "0xEE02"
#define SENSOR_ID   1
#define CLIENT_ID   "aguada-" NODE_ID_STR
static const char TOPIC[] = "aguada/raw/" NODE_ID_STR "/1"; // RAM: PubSubClient::publish() needs a RAM pointer

// ── Network (static — no DHCP, no on-device config) ─────────────────────
static uint8_t   mac[]    = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02};
static IPAddress ipAddr(192, 168, 0, 202);
static IPAddress dnsIP(192, 168, 0, 1);
static IPAddress gwIP(192, 168, 0, 1);
static IPAddress subnet(255, 255, 255, 0);
static IPAddress brokerIP(192, 168, 0, 101);
static const uint16_t BROKER_PORT = 1883;

// ── Timing / filter ──────────────────────────────────────────────────────
static const uint32_t MEASURE_INTERVAL_MS = 30000UL;
static const uint32_t FORCE_SEND_MS       = 120000UL;
static const uint16_t DELTA_CM            = 2;
static const uint8_t  SAMPLES             = 5;
static const uint8_t  MIN_VALID           = 3;
static const uint16_t DIST_MIN_CM         = 2;
static const uint16_t DIST_MAX_CM         = 450;
static const uint32_t PULSE_TIMEOUT_US    = 30000UL;
static const uint32_t MQTT_RETRY_MS       = 5000UL;
static const uint8_t  MEAS_FAIL_LIMIT     = 3;
static const uint8_t  FAIL_ETH_REINIT     = 10;
static const uint8_t  FAIL_HARD_RESET     = 20;

// ── Globals ──────────────────────────────────────────────────────────────
static EthernetClient ethClient;
static PubSubClient   mqtt(ethClient);

static uint16_t seq          = 0;
static uint16_t lastSent     = 0xFFFF; // sentinel: never sent yet
static uint32_t lastMeasureMs = 0;
static uint32_t lastSendMs    = 0;
static uint32_t lastMqttTryMs = 0;
static uint8_t  measFail      = 0;
static uint8_t  mqttFail      = 0;

// ── Watchdog reset safety (D-02, threat T-L28-05) ────────────────────────
// The classic Nano bootloader does not clear WDRF on reset, so a deliberate
// watchdog reset in mqttEnsure() would otherwise boot-loop the node forever.
// Clear MCUSR and disable the watchdog before main() runs so the forced-
// reset recovery path below is actually survivable.
void wdtInit(void) __attribute__((naked, used, section(".init3")));
void wdtInit(void) {
    MCUSR = 0;
    wdt_disable();
}

// ── HC-SR04 ──────────────────────────────────────────────────────────────
static uint16_t readOnce() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    uint32_t duration = pulseIn(ECHO_PIN, HIGH, PULSE_TIMEOUT_US);
    if (duration == 0) {
        return 0; // timed out
    }
    return (uint16_t)(duration / 58UL);
}

static uint16_t measure() {
    uint16_t buf[SAMPLES];
    uint8_t  n = 0;

    for (uint8_t i = 0; i < SAMPLES; i++) {
        wdt_reset();
        uint16_t d = readOnce();
        if (d >= DIST_MIN_CM && d <= DIST_MAX_CM) {
            buf[n++] = d;
        }
        delay(60);
    }

    if (n < MIN_VALID) {
        return 0;
    }

    // Insertion sort the valid prefix, return the median.
    for (uint8_t i = 1; i < n; i++) {
        uint16_t key = buf[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && buf[j] > key) {
            buf[j + 1] = buf[j];
            j--;
        }
        buf[j + 1] = key;
    }

    return buf[n / 2];
}

// ── Ethernet / MQTT ──────────────────────────────────────────────────────
static void netBegin() {
    Ethernet.begin(mac, ipAddr, dnsIP, gwIP, subnet);
}

static bool mqttEnsure() {
    if (mqtt.connected()) {
        return true;
    }

    uint32_t now = millis();
    if ((uint32_t)(now - lastMqttTryMs) < MQTT_RETRY_MS) {
        return false;
    }
    lastMqttTryMs = now;

    wdt_reset();
    bool ok = mqtt.connect(CLIENT_ID);
    wdt_reset();

    if (ok) {
        mqttFail = 0;
        DBGLN(F("MQTT connected"));
        return true;
    }

    mqttFail++;
    DBG(F("MQTT connect failed, fail="));
    DBGLN(mqttFail);

    // Escalation per D-02: re-init the Ethernet stack after sustained
    // failures, and force a watchdog reset if that still doesn't recover.
    if (mqttFail >= FAIL_HARD_RESET) {
        wdt_enable(WDTO_15MS);
        for (;;) {
            // spin until the watchdog fires
        }
    } else if (mqttFail == FAIL_ETH_REINIT) {
        netBegin();
    }

    return false;
}

static void publishReading(uint16_t cm, uint8_t flags) {
    char payload[160];
    // Key order: type, node_id, sensor_id, distance_cm, rssi, vbat, flags, seq
    // — matches the gateway-shaped packet _handle_sensor already consumes.
    snprintf_P(payload, sizeof(payload), PSTR(
        "{\"type\":\"sensor\",\"node_id\":\"" NODE_ID_STR "\",\"sensor_id\":%u,"
        "\"distance_cm\":%u,\"rssi\":0,\"vbat\":-1,\"flags\":%u,\"seq\":%u}"
    ), (unsigned)SENSOR_ID, (unsigned)cm, (unsigned)flags, (unsigned)seq);

    mqtt.publish(TOPIC, payload);
    seq++;
}

// ── Setup / loop ─────────────────────────────────────────────────────────
void setup() {
#if DEBUG
    Serial.begin(9600);
#endif

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

    netBegin();

    mqtt.setServer(brokerIP, BROKER_PORT);
    mqtt.setSocketTimeout(4); // before the first connect: a stalled broker must not outlast the 8s watchdog

    wdt_enable(WDTO_8S);

    uint32_t now = millis();
    lastMeasureMs  = now;
    lastSendMs     = now;
    lastMqttTryMs  = now;
}

void loop() {
    wdt_reset();

    mqttEnsure();
    mqtt.loop();

    uint32_t now = millis();
    if ((uint32_t)(now - lastMeasureMs) >= MEASURE_INTERVAL_MS) {
        lastMeasureMs = now;

        uint16_t reading = measure();

        if (reading == 0) {
            measFail++;
            if (measFail >= MEAS_FAIL_LIMIT) {
                if (mqtt.connected()) {
                    publishReading(65535, 4); // bridge.py treats flags & 0x04 as a sensor error
                    lastSendMs = now;
                }
                measFail = 0; // don't re-emit the error packet every cycle
            }
        } else {
            measFail = 0;

            bool shouldSend;
            if (lastSent == 0xFFFF) {
                shouldSend = true;
            } else {
                uint16_t diff = (reading > lastSent) ? (reading - lastSent) : (lastSent - reading);
                shouldSend = (diff >= DELTA_CM) || ((uint32_t)(now - lastSendMs) >= FORCE_SEND_MS);
            }

            if (shouldSend && mqtt.connected()) {
                publishReading(reading, 0);
                lastSent   = reading;
                lastSendMs = now;
            }
        }
    }
}
