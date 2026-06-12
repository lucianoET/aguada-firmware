#include "rgb_led.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel *s_strip = nullptr;
static uint8_t s_pin = 0;

void rgb_led_init(uint8_t pin) {
    s_pin = pin;
    if (pin == 0) return;
    s_strip = new Adafruit_NeoPixel(1, pin, NEO_GRB + NEO_KHZ800);
    s_strip->begin();
    s_strip->setBrightness(40);  // ~16% — enough to see, not blinding
    s_strip->clear();
    s_strip->show();
}

void rgb_led_set(uint8_t r, uint8_t g, uint8_t b) {
    if (!s_strip) return;
    s_strip->setPixelColor(0, s_strip->Color(r, g, b));
    s_strip->show();
}

void rgb_led_status(rgb_led_mode_t mode) {
    if (!s_strip) return;
    switch (mode) {
        case RGB_NORMAL:
            rgb_led_set(0, 255, 0);   // green
            delay(40);
            rgb_led_set(0, 0, 0);
            break;
        case RGB_NO_GW:
            rgb_led_set(255, 180, 0); // amber
            delay(40);
            rgb_led_set(0, 0, 0);
            break;
        case RGB_SENSOR_ERR:
            rgb_led_set(255, 0, 0);   // red
            delay(200);
            rgb_led_set(0, 0, 0);
            break;
        case RGB_OTA:
            rgb_led_set(0, 0, 255);   // blue (stays on)
            break;
        case RGB_OFF:
        default:
            rgb_led_set(0, 0, 0);
            break;
    }
}
