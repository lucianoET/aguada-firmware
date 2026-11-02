// Node Communicator — fase 1 (ESP32-C3): PTT de voz por UDP multicast, alto-falante ou
// saída de linha, OLED, botões de alarme, sirene e alarme por ESP-NOW. Doc: COMMUNICATOR.md
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_log.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "comm_config.h"
#include "protocol.h"
#include "audio_io.h"
#include "voice_dsp.h"
#include "voice_net.h"
#include "alarm.h"
#include "tones.h"

static const char *TAG = "comm";

static Adafruit_SSD1306 s_oled(128, 64, &Wire, -1);
static bool     s_oled_ok = false;
static uint16_t s_node_id = 0;

// Estado compartilhado entre a task de áudio e a de UI.
static volatile bool s_ptt = false;          // botão PTT pressionado (e mic presente)
static volatile bool s_on_air = false;       // transmitindo (inclui o gongo)

// ── Task de áudio: TX do PTT ou RX/tons para a saída ────────────────────────────
static void audio_task(void *) {
    static int32_t raw[AUDIO_FRAME];
    static int16_t pcm[AUDIO_FRAME];
    VoiceDsp dsp;
    bool was_tx = false;
    int  chime = 0;

    for (;;) {
        if (s_ptt) {
            if (!was_tx) {                       // início do PTT
                voice_rx_enable(false);          // falante mudo: sem microfonia
                dsp.reset();
                chime = COMM_PTT_CHIME ? 0 : 24;
                was_tx = true;
                s_on_air = true;
            }
            if (chime_block(pcm, AUDIO_FRAME, chime)) {
                chime++;
                voice_send(pcm, AUDIO_FRAME, false);
                vTaskDelay(pdMS_TO_TICKS(20));   // gongo no ritmo do tempo real
                if (chime == 24) audio_flush_mic();
                continue;
            }
            audio_read_mic(raw, AUDIO_FRAME);    // 20 ms, ritmo do I2S
            dsp.process(raw, pcm, AUDIO_FRAME);
            voice_send(pcm, AUDIO_FRAME, false);
            continue;
        }
        if (was_tx) {                            // fim do PTT
            memset(pcm, 0, sizeof(pcm));
            voice_send(pcm, AUDIO_FRAME, true);
            voice_rx_enable(true);
            was_tx = false;
            s_on_air = false;
        }

        if (alarm_siren_on()) {                  // alarme tem prioridade sobre a voz
            siren_block(pcm, AUDIO_FRAME);
            audio_write(pcm, AUDIO_FRAME);
        } else if (voice_pop(pcm, AUDIO_FRAME)) {
            audio_write(pcm, AUDIO_FRAME);
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));        // TX com auto_clear manda silêncio
        }
    }
}

// ── Botões ──────────────────────────────────────────────────────────────────────
struct Button {
    uint8_t  pin;
    bool     down = false;
    uint32_t since = 0;
    bool     fired = false;                      // ação de "segurar" já disparou
    explicit Button(uint8_t p) : pin(p) {}
    void begin() { pinMode(pin, INPUT_PULLUP); }
    // Atualiza; devolve true na borda de soltura (toque curto se !fired).
    bool update() {
        bool d = digitalRead(pin) == LOW;
        bool released = down && !d;
        if (d && !down) { since = millis(); fired = false; }
        down = d;
        return released;
    }
    uint32_t held() const { return down ? millis() - since : 0; }
};

static Button s_btn_ptt(COMM_PIN_PTT), s_btn_alarm(COMM_PIN_ALARM), s_btn_vol(COMM_PIN_VOL);
static uint32_t s_msg_until = 0;
static char     s_msg[22] = "";

static void flash_msg(const char *m) {
    strncpy(s_msg, m, sizeof(s_msg) - 1);
    s_msg_until = millis() + 2000;
}

static void buttons_tick(void) {
    // PTT: só com mic e rede; avisa no OLED se faltar.
    s_btn_ptt.update();
    bool want = s_btn_ptt.down;
    if (want && !s_ptt) {
        if (!audio_mic_present())     { if (!s_btn_ptt.fired) flash_msg("SEM MICROFONE"); s_btn_ptt.fired = true; want = false; }
        else if (!voice_net_ready())  { if (!s_btn_ptt.fired) flash_msg("SEM WIFI");      s_btn_ptt.fired = true; want = false; }
    }
    s_ptt = want;

    // Alarme: segurar 1 s dispara; com alarme ativo, segurar 3 s cancela.
    s_btn_alarm.update();
    if (s_btn_alarm.down && !s_btn_alarm.fired) {
        if (alarm_code() == ALARM_CLEAR && s_btn_alarm.held() >= 1000) {
            alarm_raise(ALARM_GENERAL);
            s_btn_alarm.fired = true;
        } else if (alarm_code() != ALARM_CLEAR && s_btn_alarm.held() >= 3000) {
            alarm_clear();
            flash_msg("ALARME CANCELADO");
            s_btn_alarm.fired = true;
        }
    }

    // Volume: toque cicla 25 → 50 → 75 → 100.
    if (s_btn_vol.update()) {
        uint8_t v = audio_volume();
        v = v >= 100 ? 25 : (v / 25 + 1) * 25;
        audio_set_volume(v);
        char m[22];
        snprintf(m, sizeof(m), "VOLUME %u%%", v);
        flash_msg(m);
    }

    digitalWrite(COMM_PIN_LED, s_on_air ? LOW : HIGH);
}

// ── OLED ────────────────────────────────────────────────────────────────────────
static const char *alarm_name(uint8_t c) {
    switch (c) {
        case ALARM_FIRE: return "INCENDIO";
        case ALARM_TEST: return "TESTE";
        default:         return "ALARME";
    }
}

static void oled_center(const char *s, int y, int size) {
    s_oled.setTextSize(size);
    int w = strlen(s) * 6 * size;
    s_oled.setCursor(w < 128 ? (128 - w) / 2 : 0, y);
    s_oled.print(s);
}

static void oled_draw(void) {
    if (!s_oled_ok) return;
    static bool blink = false;
    blink = !blink;
    s_oled.clearDisplay();
    s_oled.setTextColor(SSD1306_WHITE);

    // Cabeçalho: nome, zona, rede.
    s_oled.setTextSize(1);
    s_oled.setCursor(0, 0);
    s_oled.printf("%s Z%u", COMM_NAME, COMM_ZONE);
    char net[12];
    if (WiFi.isConnected()) snprintf(net, sizeof(net), "%ddBm", (int)WiFi.RSSI());
    else                    snprintf(net, sizeof(net), "sem WiFi");
    s_oled.setCursor(128 - strlen(net) * 6, 0);
    s_oled.print(net);
    s_oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Centro: o estado mais importante primeiro.
    uint8_t ac = alarm_code();
    uint16_t talker = voice_talker();
    char line[22];
    if (ac != ALARM_CLEAR) {
        if (blink) { s_oled.fillRect(0, 12, 128, 38, SSD1306_WHITE); s_oled.setTextColor(SSD1306_BLACK); }
        oled_center(alarm_name(ac), 16, 2);
        snprintf(line, sizeof(line), alarm_origin() == s_node_id ? "local" : "de %04X", alarm_origin());
        oled_center(line, 38, 1);
        s_oled.setTextColor(SSD1306_WHITE);
    } else if (s_on_air) {
        s_oled.fillRect(0, 12, 128, 38, SSD1306_WHITE);
        s_oled.setTextColor(SSD1306_BLACK);
        oled_center("NO AR", 16, 2);
        oled_center("solte para encerrar", 38, 1);
        s_oled.setTextColor(SSD1306_WHITE);
    } else if (talker) {
        oled_center("OUVINDO", 16, 2);
        snprintf(line, sizeof(line), "anuncio de %04X", talker);
        oled_center(line, 38, 1);
    } else {
        oled_center("PRONTO", 16, 2);
        oled_center("segure PTT p/ falar", 38, 1);
    }

    // Rodapé: mensagem temporária ou volume / mic / sirene.
    s_oled.setTextSize(1);
    s_oled.setCursor(0, 56);
    if (millis() < s_msg_until) {
        s_oled.print(s_msg);
    } else {
        s_oled.printf("vol %u%%  mic %s%s", audio_volume(),
                      audio_mic_present() ? "ok" : "--", alarm_siren_on() ? "  SIR" : "");
    }
    s_oled.display();
}

static void ui_task(void *) {
    uint32_t last_draw = 0;
    for (;;) {
        buttons_tick();
        alarm_tick();
        if (millis() - last_draw >= 200) { last_draw = millis(); oled_draw(); }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── Setup ───────────────────────────────────────────────────────────────────────
static void boot_beep(void) {
    static int16_t pcm[AUDIO_FRAME];
    for (int i = 0; i < 8; i++) { tone_fill(pcm, AUDIO_FRAME, 1000, 0.3f); audio_write(pcm, AUDIO_FRAME); }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    pinMode(COMM_PIN_LED, OUTPUT);
    digitalWrite(COMM_PIN_LED, HIGH);
    s_btn_ptt.begin(); s_btn_alarm.begin(); s_btn_vol.begin();

    // Diagnóstico: em repouso SDA/SCL ficam em 1 (pull-up do módulo); 0 = curto ou sem VCC.
    pinMode(COMM_PIN_SDA, INPUT); pinMode(COMM_PIN_SCL, INPUT);
    Serial.printf("[i2c] repouso sda=%d scl=%d\n", digitalRead(COMM_PIN_SDA), digitalRead(COMM_PIN_SCL));
    Wire.begin(COMM_PIN_SDA, COMM_PIN_SCL);
    Serial.print("[i2c] scan:");
    int found = 0;
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) { Serial.printf(" 0x%02X", a); found++; }
    }
    Serial.println(found ? "" : " nenhum");
    if (!found) {   // diagnóstico: SDA e SCL trocados?
        Wire.end(); Wire.begin(COMM_PIN_SCL, COMM_PIN_SDA);
        Serial.print("[i2c] scan trocado (sda=G7 scl=G6):");
        for (uint8_t a = 1; a < 127; a++) {
            Wire.beginTransmission(a);
            if (Wire.endTransmission() == 0) { Serial.printf(" 0x%02X", a); found++; }
        }
        Serial.println(found ? "  <- SDA/SCL TROCADOS" : " nenhum");
        Wire.end(); Wire.begin(COMM_PIN_SDA, COMM_PIN_SCL);
    }
    Wire.beginTransmission(COMM_OLED_ADDR);
    s_oled_ok = Wire.endTransmission() == 0 && s_oled.begin(SSD1306_SWITCHCAPVCC, COMM_OLED_ADDR);
    if (s_oled_ok) {
        s_oled.clearDisplay(); s_oled.setTextColor(SSD1306_WHITE);
        oled_center("COMMUNICATOR", 20, 1);
        oled_center("iniciando...", 36, 1);
        s_oled.display();
    }

    // WiFi STA + ESP-NOW. Com AP o canal é o dele; sem WiFi fica no ESPNOW_CHANNEL.
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);                        // power-save derruba multicast e latência
    uint8_t mac[6];
    WiFi.macAddress(mac);
    s_node_id = ((uint16_t)mac[4] << 8) | mac[5];
    bool have_wifi = strlen(WIFI_SSID) > 0;
    if (have_wifi) {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        uint32_t t0 = millis();
        while (!WiFi.isConnected() && millis() - t0 < 15000) delay(100);
    }
    if (!WiFi.isConnected()) {
        esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
        ESP_LOGW(TAG, "%s — só alarme por ESP-NOW (canal %d)",
                 have_wifi ? "WiFi falhou" : "sem SSID", ESPNOW_CHANNEL);
    }
    if (esp_now_init() != ESP_OK) ESP_LOGE(TAG, "esp_now_init falhou");
    alarm_init(s_node_id);
    if (WiFi.isConnected()) voice_net_begin(s_node_id, alarm_on_remote);

    audio_init();
    boot_beep();

    uint8_t ch; wifi_second_chan_t sc;
    esp_wifi_get_channel(&ch, &sc);
    Serial.printf("COMM_BOOT node=0x%04X name=%s zone=%u wifi=%s ip=%s canal=%u mic=%d oled=%d\n",
                  s_node_id, COMM_NAME, COMM_ZONE, WiFi.isConnected() ? "ok" : "--",
                  WiFi.localIP().toString().c_str(), ch, audio_mic_present(), s_oled_ok);
    if (WiFi.isConnected() && ch != ESPNOW_CHANNEL)
        ESP_LOGW(TAG, "AP no canal %u ≠ ESPNOW_CHANNEL %d: alarme ESP-NOW não chega ao gateway",
                 ch, ESPNOW_CHANNEL);

    xTaskCreate(audio_task, "audio", 6144, nullptr, 5, nullptr);
    xTaskCreate(ui_task,    "ui",    4096, nullptr, 2, nullptr);
}

void loop() {
    // Reconecta o WiFi se cair (o multicast volta sozinho: o socket fica escutando).
    static uint32_t last = 0;
    if (strlen(WIFI_SSID) && !WiFi.isConnected() && millis() - last > 10000) {
        last = millis();
        WiFi.reconnect();
    }
    // Multicast só sobe com IP; arranca na primeira conexão (boot sem WiFi ou queda).
    if (WiFi.isConnected() && !voice_net_ready()) voice_net_begin(s_node_id, alarm_on_remote);
    delay(100);
}
