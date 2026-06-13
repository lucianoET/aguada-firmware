// Teste básico RFID (ID-12) -> Captive Portal
//
// Lê o UID de tags 125 kHz pelo leitor ID-12 (UART 9600 8N1) e mostra o último
// código numa página captive portal. Também loga cada leitura na serial USB.
//
// Ligação:  ID-12 TX --1k--+--> GPIO20 (RX)   VCC=5V  GND=GND
//                          2k2
//                          GND
//
// Spec: docs/superpowers/specs/2026-06-12-rfid-captive-test-design.md

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "page.h"

// ---- Config ----------------------------------------------------------------
static const char    *AP_SSID   = "RFID-TEST";   // rede aberta
static const uint8_t  RFID_RX_PIN = 20;          // GPIO20 = RX do Serial1
static const uint32_t RFID_BAUD   = 9600;
static const IPAddress AP_IP(192, 168, 4, 1);

// ID-12 frame: STX(0x02) + 10 hex (UID) + 2 hex (checksum) + CR + LF + ETX(0x03)
static const char STX = 0x02;
static const char ETX = 0x03;

// ---- Estado global ---------------------------------------------------------
static WebServer   server(80);
static DNSServer   dns;
static char        g_last_uid[11] = {0};   // 10 chars + '\0'
static uint32_t    g_last_ms      = 0;
static bool        g_have_read    = false;

// ---- Parser do ID-12 -------------------------------------------------------
static char    rxbuf[16];
static uint8_t rxlen = 0;
static bool    in_frame = false;

// Converte char hex ASCII para valor 0..15, ou -1 se inválido.
static int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Processa um frame completo (conteúdo entre STX e ETX, CR/LF já removidos).
// Espera 12 chars hex (10 UID + 2 checksum). Valida XOR. Atualiza estado.
static void handle_frame(const char *buf, uint8_t len) {
  if (len != 12) return;
  uint8_t bytes[6];
  for (uint8_t i = 0; i < 6; i++) {
    int hi = hexval(buf[i * 2]);
    int lo = hexval(buf[i * 2 + 1]);
    if (hi < 0 || lo < 0) return;            // char não-hex -> descarta
    bytes[i] = (uint8_t)((hi << 4) | lo);
  }
  // Checksum = XOR dos 5 bytes do UID; deve bater com bytes[5].
  uint8_t chk = bytes[0] ^ bytes[1] ^ bytes[2] ^ bytes[3] ^ bytes[4];
  if (chk != bytes[5]) {
    Serial.printf("RFID frame com checksum invalido (calc %02X != %02X)\n", chk, bytes[5]);
    return;
  }
  memcpy(g_last_uid, buf, 10);
  g_last_uid[10] = '\0';
  g_last_ms   = millis();
  g_have_read = true;
  Serial.printf("RFID UID: %s\n", g_last_uid);
}

static void rfid_poll() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == STX) { in_frame = true; rxlen = 0; continue; }
    if (!in_frame) continue;
    if (c == ETX) { handle_frame(rxbuf, rxlen); in_frame = false; continue; }
    if (c == '\r' || c == '\n') continue;     // ignora CR/LF dentro do frame
    if (rxlen < sizeof(rxbuf)) rxbuf[rxlen++] = c;
    else { in_frame = false; rxlen = 0; }      // overflow -> aborta frame
  }
}

// ---- Handlers HTTP ---------------------------------------------------------
static void handle_root() {
  server.send_P(200, "text/html", PAGE_HTML);
}

static void handle_last() {
  char json[64];
  if (g_have_read) {
    snprintf(json, sizeof(json), "{\"uid\":\"%s\",\"age_ms\":%lu}",
             g_last_uid, (unsigned long)(millis() - g_last_ms));
  } else {
    snprintf(json, sizeof(json), "{\"uid\":\"\",\"age_ms\":0}");
  }
  server.send(200, "application/json", json);
}

static void handle_redirect() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

// ---- Setup / Loop ----------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Teste RFID ID-12 -> Captive Portal ===");

  Serial1.begin(RFID_BAUD, SERIAL_8N1, RFID_RX_PIN, -1);
  Serial.printf("Serial1 RFID: %u 8N1 RX=GPIO%u\n", RFID_BAUD, RFID_RX_PIN);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID);
  Serial.printf("AP \"%s\" em http://%s/\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  dns.start(53, "*", AP_IP);

  server.on("/", handle_root);
  server.on("/last", handle_last);
  server.onNotFound(handle_redirect);
  server.begin();
  Serial.println("HTTP server pronto. Aproxime uma tag.");
}

void loop() {
  dns.processNextRequest();
  server.handleClient();
  rfid_poll();
}
