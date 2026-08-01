// GPS Tracker — bench harness (Fase 1)
// Wires GpsReader -> Fix snapshot -> serial output. Reads GPS state only
// through GpsReader/Fix, never through a direct TinyGPSPlus accessor call.
// LED onboard: solid = fresh fix, blinking = receiving NMEA without a fresh
// fix, dark = no data at all.

#include <Arduino.h>
#include "gps_config.h"
#include "gps_types.h"
#include "gps_reader.h"

#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 0
#endif

static GpsReader gpsReader;
static Fix       lastFix{};
static uint32_t  lastHealthMs = 0;
static bool      everFixed    = false;
static uint32_t  lastFixAtMs  = 0;

static inline void ledWrite(bool on) {
    digitalWrite(LED_PIN, LED_ACTIVE_LOW ? !on : on);
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    gpsReader.begin();
    Serial.println();
    Serial.printf("[gps_tracker] bench harness -- UART%d RX=%d TX=%d @ %d\n",
                  GPS_UART_NUM, GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
    Serial.println("[gps_tracker] 'r' = toggle raw NMEA echo");
}

void loop() {
    Fix  fix;
    bool haveFix = gpsReader.poll(fix);

    if (Serial.available() && Serial.read() == 'r') {
        gpsReader.setRawEcho(!gpsReader.rawEcho());
        Serial.printf("\n[gps_tracker] raw echo: %s\n", gpsReader.rawEcho() ? "ON" : "OFF");
    }

    if (haveFix) {
        lastFix     = fix;
        everFixed   = true;
        lastFixAtMs = fix.mono_ms;

        Serial.printf(
            "[FIX] t=%lu lat=%.6f lon=%.6f alt=%.1fm spd=%.1fkm/h crs=%.0f hdop=%.1f sats=%u q=%u age=%lums\n",
            (unsigned long)fix.utc_unix, fix.lat, fix.lon, fix.alt_m,
            fix.speed_kmh, fix.course_deg, fix.hdop, fix.sats, fix.fix_quality,
            (unsigned long)fix.age_ms);
    }

    bool freshFix = everFixed && (millis() - lastFixAtMs) < DEFAULT_GPS_FRESH_MS;
    if (freshFix) {
        ledWrite(true);
    } else if (gpsReader.receiving()) {
        ledWrite((millis() / 250) % 2);
    } else {
        ledWrite(false);
    }

    if (millis() - lastHealthMs >= DEFAULT_GPS_HEALTH_PERIOD_MS) {
        lastHealthMs = millis();
        Serial.printf(
            "[HEALTH] bytes=%lu ok=%lu bad=%lu sats=%u hdop=%.1f rx=%s\n",
            (unsigned long)gpsReader.bytesRead(),
            (unsigned long)gpsReader.passedChecksum(),
            (unsigned long)gpsReader.failedChecksum(),
            lastFix.sats, lastFix.hdop,
            gpsReader.receiving() ? "yes" : "no");
    }
}
