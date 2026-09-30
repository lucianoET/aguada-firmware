// ESP-NOW sniffer: promiscuous, hops channels, prints every ESP-NOW frame seen.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

static const uint8_t CH[] = {11, 1, 6, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13};
#define DWELL_MS 65000
volatile uint8_t cur_ch = 11;

struct Ev { uint8_t mac[6]; int8_t rssi; uint8_t ch; uint16_t len; uint8_t b[16]; };
static QueueHandle_t q;

static void IRAM_ATTR sniff(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  auto *p = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *f = p->payload;
  int len = p->rx_ctrl.sig_len;
  // action frame (0xD0), category 127 vendor-specific, Espressif OUI 18:FE:34
  if (f[0] != 0xD0 || len < 40 || f[24] != 127 || f[25] != 0x18 || f[26] != 0xFE || f[27] != 0x34) return;
  Ev e; memcpy(e.mac, f + 10, 6); e.rssi = p->rx_ctrl.rssi; e.ch = cur_ch;
  // vendor element: f[32]=0xDD, f[33]=len, then OUI(3)+type(1)+ver(1), body at f[39]
  e.len = f[33] > 5 ? f[33] - 5 : 0; memcpy(e.b, f + 39, 16);
  xQueueSendFromISR(q, &e, nullptr);
}

void setup() {
  Serial.begin(115200); delay(3000);
  q = xQueueCreate(32, sizeof(Ev));
  WiFi.mode(WIFI_STA); WiFi.disconnect();
  wifi_promiscuous_filter_t flt = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&flt);
  esp_wifi_set_promiscuous_rx_cb(sniff);
  esp_wifi_set_promiscuous(true);
}

void loop() {
  static uint32_t t = 0; static int i = -1;
  if (i < 0 || millis() - t >= DWELL_MS) {
    i = (i + 1) % sizeof(CH); t = millis(); cur_ch = CH[i];
    esp_wifi_set_channel(cur_ch, WIFI_SECOND_CHAN_NONE);
    Serial.printf("#CH %u\n", cur_ch);
  }
  Ev e;
  while (xQueueReceive(q, &e, 0) == pdTRUE) {
    Serial.printf("ch=%u mac=%02X:%02X:%02X:%02X:%02X:%02X rssi=%d len=%u body=",
      e.ch, e.mac[0], e.mac[1], e.mac[2], e.mac[3], e.mac[4], e.mac[5], e.rssi, e.len);
    for (int k = 0; k < 16; k++) Serial.printf("%02X", e.b[k]);
    Serial.println();
  }
  delay(5);
}
