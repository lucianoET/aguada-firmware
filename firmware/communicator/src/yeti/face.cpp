#include <Arduino.h>
#include <math.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "yeti_config.h"
#include "face.h"
#include "sensors.h"

// Módulo 1.3" de 7 pinos: sem CS, por isso SPI modo 3. Controlador 240×320 com painel
// 240×240; o LovyanGFX ajusta o offset de cada rotação pelo memory_height.
class YetiLcd : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI      _bus;
    lgfx::Light_PWM    _light;

public:
    YetiLcd() {
        auto b = _bus.config();
        b.spi_host   = SPI2_HOST;
        b.spi_mode   = 3;
        b.freq_write = YETI_TFT_HZ;
        b.freq_read  = 16000000;
        b.spi_3wire  = true;
        b.use_lock   = true;
        b.dma_channel = SPI_DMA_CH_AUTO;
        b.pin_sclk = YETI_PIN_TFT_SCK;
        b.pin_mosi = YETI_PIN_TFT_MOSI;
        b.pin_miso = -1;
        b.pin_dc   = YETI_PIN_TFT_DC;
        _bus.config(b);
        _panel.setBus(&_bus);

        auto p = _panel.config();
        p.pin_cs   = -1;
        p.pin_rst  = YETI_PIN_TFT_RST;
        p.pin_busy = -1;
        p.memory_width  = 240;
        p.memory_height = 320;
        p.panel_width   = 240;
        p.panel_height  = 240;
        p.readable  = false;
        p.invert    = true;          // IPS: cores invertidas no ST7789
        p.rgb_order = false;
        p.bus_shared = false;
        _panel.config(p);

        auto l = _light.config();
        l.pin_bl = YETI_PIN_TFT_BL;
        l.invert = false;
        l.freq   = 12000;
        l.pwm_channel = YETI_TFT_BL_CH;
        _light.config(l);
        _panel.setLight(&_light);

        setPanel(&_panel);
    }
};

static YetiLcd      s_lcd;
static LGFX_Sprite  s_spr(&s_lcd);
static bool         s_ok = false;

static Mood     s_mood = MOOD_IDLE;
static uint32_t s_mood_since = 0;
static float    s_gaze_in_x = NAN, s_gaze_in_y = NAN;
static float    s_mouth_in = 0;
static bool     s_net = false;
static uint32_t s_info_until = 0;
static uint32_t s_toast_until = 0;
static char     s_toast[24] = "";

// Estado animado (aproxima dos alvos a cada quadro).
static float gx = 0, gy = 0, tgx = 0, tgy = 0;   // olhar
static float lid = 0;                            // 0 aberto .. 1 fechado
static float mouth = 0;
static float happy = 0;                          // 0..1 olhos ^ ^
static uint32_t next_saccade = 0, next_blink = 0, blink_t0 = 0;

static inline float approach(float v, float t, float k) { return v + (t - v) * k; }

void face_begin(void) {
    s_ok = s_lcd.init();
    s_lcd.setRotation(YETI_TFT_ROTATION);
    s_lcd.setBrightness(160);
    s_lcd.fillScreen(TFT_BLACK);
    s_spr.setColorDepth(16);
    s_spr.setPsram(false);                       // RAM interna: DMA mais rápido
    if (!s_spr.createSprite(240, 240)) {          // sem 115 KB livres (C3): 8 bit, 57 KB
        s_spr.setColorDepth(8);
        if (!s_spr.createSprite(240, 240)) s_ok = false;
    }
    Serial.printf("[face] sprite %d bit, heap livre %u\n", s_spr.getColorDepth(),
                  (unsigned)ESP.getFreeHeap());
    s_mood_since = millis();
}

void face_set_mood(Mood m) {
    if (m != s_mood) { s_mood = m; s_mood_since = millis(); }
}
Mood face_mood(void) { return s_mood; }
void face_set_gaze(float x, float y) { s_gaze_in_x = x; s_gaze_in_y = y; }
void face_set_mouth(float level) { s_mouth_in = constrain(level, 0.0f, 1.0f); }
void face_set_brightness(uint8_t v) { s_lcd.setBrightness(v); }
void face_set_net(bool ok) { s_net = ok; }
void face_show_info(uint32_t ms) { s_info_until = millis() + ms; }
void face_toast(const char *msg) {
    strncpy(s_toast, msg, sizeof(s_toast) - 1);
    s_toast_until = millis() + 2000;
}

static uint16_t mood_color(void) {
    switch (s_mood) {
        case MOOD_ALARM:  return (millis() / 250) % 2 ? s_spr.color565(255, 40, 30) : s_spr.color565(120, 0, 0);
        case MOOD_TALK:   return s_spr.color565(90, 255, 140);
        case MOOD_LISTEN: return s_spr.color565(120, 200, 255);
        case MOOD_COLD:   return s_spr.color565(110, 170, 255);
        case MOOD_HOT:    return s_spr.color565(255, 160, 60);
        case MOOD_SLEEPY: return s_spr.color565(120, 140, 160);
        default:          return s_spr.color565(190, 245, 255);
    }
}

// Um olho: retângulo arredondado com pálpebra (corta de cima), modo ^ (feliz) e
// sobrancelha brava (triângulo preto no canto interno).
static void draw_eye(int cx, int cy, int side, uint16_t col) {
    const int W = 58, H = 78;
    int h = (int)(H * (1.0f - lid));
    if (h < 6) h = 6;
    int top = cy - h / 2 + (H - h) / 4;          // fecha mais de cima que de baixo
    s_spr.fillRoundRect(cx - W / 2, top, W, h, h < 30 ? h / 2 : 22, col);

    if (happy > 0.05f) {                         // corta a metade de baixo: ^
        int oy = (int)(cy + H * (1.05f - 0.5f * happy));
        s_spr.fillEllipse(cx, oy, W * 0.75f, H * 0.62f, TFT_BLACK);
    }
    if (s_mood == MOOD_ALARM) {                  // sobrancelha brava
        int ix = cx - side * W / 2;              // canto interno
        s_spr.fillTriangle(ix - side * 4, top - 2, cx + side * W / 2 + side * 4, top - 2,
                           ix - side * 4, top + 30, TFT_BLACK);
    }
}

static void draw_dizzy(int cx, int cy, uint16_t col) {
    float a = millis() * 0.4f;
    for (int i = 0; i < 3; i++) {
        int r = 12 + i * 11;
        float s = fmodf(a + i * 120, 360.0f);
        s_spr.fillArc(cx, cy, r, r + 5, s, s + 250, col);
    }
}

static void draw_face(void) {
    uint32_t now = millis();
    uint16_t col = mood_color();

    // Olhar: entrada da IMU, senão sacadas aleatórias.
    if (!isnan(s_gaze_in_x)) {
        tgx = s_gaze_in_x; tgy = s_gaze_in_y;
    } else if (now >= next_saccade) {
        tgx = random(-100, 101) / 100.0f;
        tgy = random(-60, 61) / 100.0f;
        if (random(3) == 0) tgx = tgy = 0;
        next_saccade = now + random(1200, 4000);
    }
    gx = approach(gx, tgx, 0.35f);
    gy = approach(gy, tgy, 0.35f);

    // Piscar: 140 ms a cada 2,5–6 s (não pisca bravo nem tonto).
    float lid_t = 0;
    if (s_mood == MOOD_SLEEPY) lid_t = 0.62f + 0.08f * sinf(now / 900.0f);
    if (now >= next_blink) { blink_t0 = now; next_blink = now + random(2500, 6000); }
    if (now - blink_t0 < 140 && s_mood != MOOD_ALARM && s_mood != MOOD_DIZZY) lid_t = 1.0f;
    lid = approach(lid, lid_t, 0.55f);
    happy = approach(happy, s_mood == MOOD_HAPPY ? 1.0f : 0.0f, 0.3f);
    mouth = approach(mouth, s_mouth_in, 0.5f);

    int shiver = s_mood == MOOD_COLD ? (int)random(-2, 3) : 0;
    int ex = (int)(gx * 18) + shiver, ey = (int)(gy * 14);
    const int CY = 100;

    s_spr.fillScreen(TFT_BLACK);
    if (s_mood == MOOD_DIZZY) {
        draw_dizzy(68 + ex, CY + ey, col);
        draw_dizzy(172 + ex, CY + ey, col);
    } else {
        draw_eye(68 + ex, CY + ey, -1, col);
        draw_eye(172 + ex, CY + ey, 1, col);
    }

    // Boca: sorriso no feliz, abre com a voz no TALK/LISTEN, pequena no resto.
    int mx = 120 + ex / 2;
    if (s_mood == MOOD_HAPPY) {
        s_spr.fillArc(mx, 160, 22, 28, 20, 160, col);
    } else if (s_mood == MOOD_TALK || s_mood == MOOD_LISTEN) {
        int h = 6 + (int)(mouth * 30);
        s_spr.fillRoundRect(mx - 16, 182 - h / 2, 32, h, h / 2 < 8 ? h / 2 : 8, col);
    } else if (s_mood == MOOD_ALARM) {
        s_spr.fillRoundRect(mx - 22, 178, 44, 10, 5, col);
    } else {
        s_spr.fillRoundRect(mx - 10, 180, 20, 6, 3, col);
    }

    // Extras por humor.
    if (s_mood == MOOD_SLEEPY) {
        s_spr.setTextColor(col);
        s_spr.setFont(&fonts::Font4);
        int k = (now / 600) % 3;
        for (int i = 0; i <= k; i++) s_spr.drawString("z", 196 + i * 12, 40 - i * 14);
    } else if (s_mood == MOOD_HOT) {
        int dy = (now / 40) % 60;
        s_spr.fillCircle(214, 52 + dy, 7, s_spr.color565(120, 200, 255));
        s_spr.fillTriangle(207, 50 + dy, 221, 50 + dy, 214, 36 + dy, s_spr.color565(120, 200, 255));
    } else if (s_mood == MOOD_LISTEN) {
        int r = 8 + (int)(mouth * 14);
        s_spr.fillArc(14, 120, r, r + 3, -50, 50, col);
        s_spr.fillArc(226, 120, r, r + 3, 130, 230, col);
    }
}

static void draw_info(void) {
    const YetiSensors &v = sensors();
    char buf[24];
    s_spr.fillScreen(TFT_BLACK);
    s_spr.setTextDatum(middle_center);
    s_spr.setTextColor(s_spr.color565(190, 245, 255));
    s_spr.setFont(&fonts::FreeSansBold24pt7b);
    if (isnan(v.temp_c)) snprintf(buf, sizeof(buf), "--.-");
    else                 snprintf(buf, sizeof(buf), "%.1f", v.temp_c);
    s_spr.drawString(buf, 112, 82);
    s_spr.drawCircle(178, 62, 6, s_spr.color565(190, 245, 255));   // °
    s_spr.drawCircle(178, 62, 5, s_spr.color565(190, 245, 255));
    s_spr.setFont(&fonts::FreeSans18pt7b);
    if (isnan(v.rh)) snprintf(buf, sizeof(buf), "-- %%");
    else             snprintf(buf, sizeof(buf), "%.0f %%", v.rh);
    s_spr.drawString(buf, 120, 140);
    s_spr.setFont(&fonts::FreeSans12pt7b);
    s_spr.setTextColor(s_spr.color565(120, 140, 160));
    if (isnan(v.lux_v)) snprintf(buf, sizeof(buf), "luz --");
    else                snprintf(buf, sizeof(buf), "%.0f lx", v.lux_v);
    s_spr.drawString(buf, 120, 190);
    s_spr.setTextDatum(top_left);
}

void face_draw(void) {
    if (!s_ok) return;
    if (millis() < s_info_until) draw_info();
    else                         draw_face();

    // Rodapé e status.
    if (millis() < s_toast_until) {
        s_spr.setFont(&fonts::FreeSans9pt7b);
        s_spr.setTextDatum(bottom_center);
        s_spr.setTextColor(TFT_WHITE);
        s_spr.fillRoundRect(30, 212, 180, 26, 8, s_spr.color565(40, 40, 60));
        s_spr.drawString(s_toast, 120, 233);
        s_spr.setTextDatum(top_left);
    }
    s_spr.fillCircle(232, 8, 3, s_net ? s_spr.color565(40, 200, 90) : s_spr.color565(70, 70, 70));
    s_spr.pushSprite(0, 0);
}
