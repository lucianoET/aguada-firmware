// GPS Tracker — teste de bancada (Fase 1, walking skeleton)
// Lê NMEA do módulo GPS (ATGM336H ou NEO-6M) em UART2 e imprime:
//  - sentenças NMEA cruas (prova de fiação)
//  - resumo parseado a cada 5 s (prova de parsing)
// LED onboard: aceso = fix válido, piscando = recebendo NMEA sem fix, apagado = sem dados.

#include <Arduino.h>
#include <TinyGPSPlus.h>

#ifndef GPS_UART_NUM
#define GPS_UART_NUM 2
#endif
#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 0
#endif

TinyGPSPlus gps;
HardwareSerial GpsSerial(GPS_UART_NUM);

static inline void ledWrite(bool on) {
  digitalWrite(LED_PIN, LED_ACTIVE_LOW ? !on : on);
}

static uint32_t lastSummary = 0;
static uint32_t lastByteAt  = 0;
static uint32_t bytesTotal  = 0;
static bool     rawEcho     = true;  // 'r' na serial alterna eco cru

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  GpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println();
  Serial.printf("[gps_tracker] teste de bancada — UART%d RX=%d TX=%d @ %d\n",
                GPS_UART_NUM, GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
  Serial.println("[gps_tracker] 'r' = liga/desliga eco NMEA cru");
}

void loop() {
  while (GpsSerial.available()) {
    char c = GpsSerial.read();
    bytesTotal++;
    lastByteAt = millis();
    gps.encode(c);
    if (rawEcho) Serial.write(c);
  }

  if (Serial.available() && Serial.read() == 'r') {
    rawEcho = !rawEcho;
    Serial.printf("\n[gps_tracker] eco cru: %s\n", rawEcho ? "ON" : "OFF");
  }

  bool receiving = (millis() - lastByteAt) < 2000 && bytesTotal > 0;
  bool hasFix = gps.location.isValid() && gps.location.age() < 5000;

  if (hasFix)           ledWrite(true);
  else if (receiving)   ledWrite((millis() / 250) % 2);  // pisca
  else                  ledWrite(false);

  if (millis() - lastSummary >= 5000) {
    lastSummary = millis();
    Serial.printf("\n[resumo] bytes=%lu sentencas_ok=%lu falhas_cs=%lu sats=%d hdop=%.1f",
                  (unsigned long)bytesTotal,
                  (unsigned long)gps.passedChecksum(),
                  (unsigned long)gps.failedChecksum(),
                  gps.satellites.isValid() ? (int)gps.satellites.value() : -1,
                  gps.hdop.isValid() ? gps.hdop.hdop() : -1.0);
    if (hasFix) {
      Serial.printf(" FIX lat=%.6f lon=%.6f alt=%.1fm vel=%.1fkm/h curso=%.0f utc=%02d:%02d:%02d\n",
                    gps.location.lat(), gps.location.lng(),
                    gps.altitude.isValid() ? gps.altitude.meters() : 0.0,
                    gps.speed.isValid() ? gps.speed.kmph() : 0.0,
                    gps.course.isValid() ? gps.course.deg() : 0.0,
                    gps.time.hour(), gps.time.minute(), gps.time.second());
    } else {
      Serial.printf(" SEM FIX (%s GPIO%d)\n", receiving ? "recebendo NMEA — aguardando satelites," : "NENHUM DADO — checar fiacao GPS TX ->", GPS_RX_PIN);
    }
  }
}
