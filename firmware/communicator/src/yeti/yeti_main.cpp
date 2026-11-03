// Yeti — boneco de mesa com rosto no ST7789 240×240, mic (1 ou 2), falante, touch na
// cabeça e nos braços, LEDs nas bochechas e sensores (HTU21D, IMU, BH1750). É um communicator: fala por PTT (segurar a cabeça),
// toca os anúncios dos outros nodes e reage aos alarmes do Aguada. Doc: YETI.md
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_log.h>
#include <math.h>
#include "comm_config.h"
#include "yeti_config.h"
#include "protocol.h"
#include "audio_io.h"
#include "voice_dsp.h"
#include "voice_net.h"
#include "alarm.h"
#include "tones.h"
#include "face.h"
#include "sensors.h"
#include "body.h"

static const char *TAG = "yeti";

static uint16_t s_node_id = 0;
static volatile bool  s_ptt = false;
static volatile bool  s_on_air = false;
static volatile float s_level = 0;           // nível da voz (mic no ar ou falante) 0..1
static volatile float s_sound_dir = 0;       // -1 esquerda .. +1 direita (mics estéreo)
static volatile uint32_t s_sound_ms = 0;     // millis() do último som alto

// ── Efeitos sonoros ─────────────────────────────────────────────────────────────
enum Sfx : uint8_t { SFX_NONE, SFX_BOOT, SFX_HAPPY, SFX_BEEP };
static volatile Sfx s_sfx = SFX_NONE;

// Um bloco de 20 ms do efeito; false = acabou.
static bool sfx_block(int16_t *out, size_t n, Sfx k, int i) {
    static const float boot[]  = { 523.3f, 659.3f, 784.0f, 1046.5f };
    static const float happy[] = { 880.0f, 1174.7f, 1568.0f };
    switch (k) {
        case SFX_BOOT:  if (i >= 16) return false; tone_fill(out, n, boot[i / 4], 0.3f); return true;
        case SFX_HAPPY: if (i >= 9)  return false; tone_fill(out, n, happy[i / 3], 0.25f * (1 - (i % 3) / 4.0f)); return true;
        case SFX_BEEP:  if (i >= 4)  return false; tone_fill(out, n, 1000, 0.3f); return true;
        default: return false;
    }
}

static float rms01(const int16_t *p, size_t n) {
    float acc = 0;
    for (size_t i = 0; i < n; i++) acc += (float)p[i] * p[i];
    float db = 10.0f * log10f(acc / n / (32768.0f * 32768.0f) + 1e-9f);   // dBFS
    return constrain((db + 45.0f) / 40.0f, 0.0f, 1.0f);                  // −45..−5 dBFS
}

// Ocioso com 2 mics: som alto de um lado → o Yeti olha para lá. Bloqueia 20 ms no I2S.
static void listen_dir(int32_t *raw, int16_t *pcm) {
    audio_read_mic(raw, AUDIO_FRAME);
    for (int i = 0; i < AUDIO_FRAME; i++) pcm[i] = raw[i] >> 8;
    if (rms01(pcm, AUDIO_FRAME) < YETI_SOUND_LEVEL) return;
    float l, r;
    audio_mic_energy(&l, &r);
    float d = (sqrtf(r) - sqrtf(l)) / (sqrtf(r) + sqrtf(l) + 1e-6f);   // -1..1
    s_sound_dir = s_sound_dir * 0.6f + constrain(d * 3.0f, -1.0f, 1.0f) * 0.4f;
    s_sound_ms = millis();
}

// ── Task de áudio (mesmo fluxo do communicator + efeitos e nível p/ a boca) ─────
static void audio_task(void *) {
    static int32_t raw[AUDIO_FRAME];
    static int16_t pcm[AUDIO_FRAME];
    VoiceDsp dsp;
    bool was_tx = false;
    int  chime = 0;
    Sfx  sfx = SFX_NONE;
    int  sfx_i = 0;

    for (;;) {
        if (s_ptt) {
            if (!was_tx) {
                voice_rx_enable(false);
                dsp.reset();
                chime = COMM_PTT_CHIME ? 0 : 24;
                was_tx = true;
                s_on_air = true;
            }
            if (chime_block(pcm, AUDIO_FRAME, chime)) {
                chime++;
                voice_send(pcm, AUDIO_FRAME, false);
                vTaskDelay(pdMS_TO_TICKS(20));
                if (chime == 24) audio_flush_mic();
                continue;
            }
            audio_read_mic(raw, AUDIO_FRAME);
            dsp.process(raw, pcm, AUDIO_FRAME);
            voice_send(pcm, AUDIO_FRAME, false);
            s_level = rms01(pcm, AUDIO_FRAME);
            continue;
        }
        if (was_tx) {
            memset(pcm, 0, sizeof(pcm));
            voice_send(pcm, AUDIO_FRAME, true);
            voice_rx_enable(true);
            was_tx = false;
            s_on_air = false;
        }

        if (alarm_siren_on()) {
            siren_block(pcm, AUDIO_FRAME);
            audio_write(pcm, AUDIO_FRAME);
        } else if (voice_pop(pcm, AUDIO_FRAME)) {
            s_level = rms01(pcm, AUDIO_FRAME);
            audio_write(pcm, AUDIO_FRAME);
        } else {
            if (s_sfx != SFX_NONE) { sfx = s_sfx; sfx_i = 0; s_sfx = SFX_NONE; }
            if (sfx != SFX_NONE && sfx_block(pcm, AUDIO_FRAME, sfx, sfx_i++)) {
                audio_write(pcm, AUDIO_FRAME);
            } else {
                sfx = SFX_NONE;
                s_level = 0;
                if (audio_mic_stereo()) listen_dir(raw, pcm);
                else vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
    }
}

// ── Touch da cabeça: toque = carinho, 2 toques = clima, 3 = volume, segurar = PTT ─
static uint32_t s_happy_until = 0, s_awake_until = 0;

static void on_taps(int n) {
    s_awake_until = millis() + 10000;
    if (n == 1) {
        s_happy_until = millis() + 2500;
        s_sfx = SFX_HAPPY;
    } else if (n == 2) {
        face_show_info(YETI_INFO_MS);
        int p = body_vbat_pct();
        if (p >= 0) {
            char m[24];
            snprintf(m, sizeof(m), "bateria %d%%", p);
            face_toast(m);
        }
    } else {
        uint8_t v = audio_volume();
        v = v >= 100 ? 25 : (v / 25 + 1) * 25;
        audio_set_volume(v);
        char m[24];
        snprintf(m, sizeof(m), "volume %u%%", v);
        face_toast(m);
        s_sfx = SFX_BEEP;
    }
}

static void touch_tick(void) {
    static bool down = false, held = false;
    static uint32_t t0 = 0, last_up = 0;
    static int taps = 0;
    uint32_t now = millis();
    bool t = digitalRead(YETI_PIN_TOUCH) == YETI_TOUCH_ACTIVE;

    if (t && !down) { down = true; held = false; t0 = now; }
    if (t && down && !held && now - t0 >= YETI_HOLD_PTT_MS) {
        held = true;
        taps = 0;
        if (!audio_mic_present())    face_toast("sem microfone");
        else if (!voice_net_ready()) face_toast("sem WiFi");
        else                         s_ptt = true;
    }
    if (!t && down) {
        down = false;
        if (held) s_ptt = false;
        else { taps++; last_up = now; }
    }
    if (taps && !down && now - last_up > YETI_TAP_GAP_MS) { on_taps(taps); taps = 0; }
}

// ── Humor, olhar e brilho a partir do estado ────────────────────────────────────
static void mood_tick(void) {
    const YetiSensors &v = sensors();
    uint32_t now = millis();

    // Escuro contínuo → dorme; toque ou batida acorda por 10 s.
    static uint32_t dark_since = 0;
    bool dark = v.lux && !isnan(v.lux_v) && v.lux_v < YETI_DARK_LUX;
    if (!dark) dark_since = 0;
    else if (!dark_since) dark_since = now;
    if (v.tap_ms && now - v.tap_ms < 200) s_awake_until = now + 10000;
    bool sleepy = dark_since && now - dark_since > YETI_DARK_S * 1000UL && now > s_awake_until;

    Mood m = MOOD_IDLE;
    if (alarm_code() != ALARM_CLEAR)                 m = MOOD_ALARM;
    else if (s_on_air)                               m = MOOD_TALK;
    else if (voice_talker())                         m = MOOD_LISTEN;
    else if (v.shake_ms && now - v.shake_ms < 3000)  m = MOOD_DIZZY;
    else if (now < s_happy_until)                    m = MOOD_HAPPY;
    else if (sleepy)                                 m = MOOD_SLEEPY;
    else if (v.temp_c >= YETI_HOT_C)                 m = MOOD_HOT;
    else if (v.temp_c <= YETI_COLD_C)                m = MOOD_COLD;
    face_set_mood(m);
    face_set_mouth(s_level);
    face_set_net(voice_net_ready());

    // Inclinar o boneco puxa o olhar; parado, olha sozinho.
    if (v.imu && (fabsf(v.ax) > 0.2f || fabsf(v.ay) > 0.2f) && m != MOOD_DIZZY)
        face_set_gaze(constrain(-v.ay * 2.0f, -1.0f, 1.0f), constrain(v.ax * 2.0f, -1.0f, 1.0f));
    else if (s_sound_ms && now - s_sound_ms < 1500 && m == MOOD_IDLE)
        face_set_gaze(s_sound_dir, 0);
    else
        face_set_gaze(NAN, NAN);

    // Backlight segue a luz ambiente (20..255); sem sensor, fixo.
    uint8_t bl = 160;
    if (v.lux && !isnan(v.lux_v)) bl = (uint8_t)constrain(20 + 60.0f * log10f(v.lux_v + 1), 20.0f, 255.0f);
    if (m == MOOD_ALARM || m == MOOD_TALK) bl = 255;
    if (m == MOOD_SLEEPY) bl = 12;
    static uint8_t cur = 0;
    if (bl != cur) { cur = cur + (bl > cur ? 1 : -1) * max(1, abs(bl - cur) / 8); face_set_brightness(cur); }
    body_leds(m, cur, s_level);
}

// ── Braços: toque D = sim, toque E = não, segurar = gatilho, os dois = abraço ───
// Cada evento sai também na serial (YETI_EVT ...) para quem quiser automatizar.
static void arms_tick(void) {
    ArmEvt e = body_arms_tick();
    if (e == ARM_NONE) return;
    static const char *const name[] = { "", "nao", "sim", "gatilho_E", "gatilho_D", "abraco" };
    Serial.printf("YETI_EVT braco=%s\n", name[e]);
    s_awake_until = millis() + 10000;
    switch (e) {
        case ARM_TAP_R:  face_toast("sim");  body_leds_flash(0, 160, 0, 400);  s_sfx = SFX_BEEP; break;
        case ARM_TAP_L:  face_toast("nao");  body_leds_flash(160, 0, 0, 400);  s_sfx = SFX_BEEP; break;
        case ARM_HOLD_L: face_toast("gatilho E"); body_leds_flash(160, 90, 0, 600); break;
        case ARM_HOLD_R: face_toast("gatilho D"); body_leds_flash(160, 90, 0, 600); break;
        case ARM_HUG:
            s_happy_until = millis() + 3000;
            s_sfx = SFX_HAPPY;
            body_leds_flash(200, 40, 90, 1500);
            break;
        default: break;
    }
}

// Bateria: aviso ao cair abaixo de YETI_VBAT_LOW (de novo a cada 5 min).
static void vbat_tick(void) {
    static uint32_t warned = 0;
    float v = body_vbat();
    if (isnan(v) || v >= YETI_VBAT_LOW) return;
    if (!warned || millis() - warned > 300000) {
        warned = millis();
        face_toast("bateria fraca");
        body_leds_flash(160, 90, 0, 1000);
    }
}

static void ui_task(void *) {
    uint32_t last = 0;
    for (;;) {
        touch_tick();
        arms_tick();
        vbat_tick();
        alarm_tick();
        uint32_t now = millis();
        if (now - last >= 1000 / YETI_FPS) {
            last = now;
            mood_tick();
            face_draw();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ── Setup ───────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);
    pinMode(YETI_PIN_TOUCH, INPUT);
    face_begin();
    face_toast("iniciando...");
    face_draw();
    sensors_begin();
    body_begin();

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
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
    s_sfx = SFX_BOOT;

    uint8_t ch; wifi_second_chan_t sc;
    esp_wifi_get_channel(&ch, &sc);
    Serial.printf("YETI_BOOT node=0x%04X name=%s zone=%u wifi=%s ip=%s canal=%u mic=%d estereo=%d vbat=%.2f\n",
                  s_node_id, COMM_NAME, COMM_ZONE, WiFi.isConnected() ? "ok" : "--",
                  WiFi.localIP().toString().c_str(), ch, audio_mic_present(),
                  audio_mic_stereo(), (double)body_vbat());

    xTaskCreate(audio_task, "audio", 6144, nullptr, 5, nullptr);
    xTaskCreate(ui_task,    "ui",    6144, nullptr, 2, nullptr);
}

void loop() {
    static uint32_t last = 0;
    if (strlen(WIFI_SSID) && !WiFi.isConnected() && millis() - last > 10000) {
        last = millis();
        WiFi.reconnect();
    }
    if (WiFi.isConnected() && !voice_net_ready()) voice_net_begin(s_node_id, alarm_on_remote);
    delay(100);
}
