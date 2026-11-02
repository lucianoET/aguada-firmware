#pragma once
// Pinos e defaults do Yeti (ESP32-S3 SuperMini). Áudio/voz/alarme vêm do communicator
// (comm_config.h, pinos I2S sobrescritos no env yeti_s3). Pinagem: YETI.md

// ── Display 1.3" ST7789 240×240, SPI sem CS (módulo de 7 pinos, modo 3) ────────
#ifndef YETI_PIN_TFT_SCK
#define YETI_PIN_TFT_SCK    12    // SCL do módulo (pino nativo do FSPI)
#endif
#ifndef YETI_PIN_TFT_MOSI
#define YETI_PIN_TFT_MOSI   11    // SDA do módulo
#endif
#ifndef YETI_PIN_TFT_DC
#define YETI_PIN_TFT_DC     10
#endif
#ifndef YETI_PIN_TFT_RST
#define YETI_PIN_TFT_RST    13
#endif
#ifndef YETI_PIN_TFT_BL
#define YETI_PIN_TFT_BL     2     // BLK: PWM do backlight
#endif
#ifndef YETI_TFT_ROTATION
#define YETI_TFT_ROTATION   1     // 0..3: módulo deitado atrás da moldura, flex de lado
#endif
#ifndef YETI_TFT_HZ
#define YETI_TFT_HZ         40000000
#endif

// ── I2C: HTU21D 0x40, MPU6050/6500 0x68/0x69, BH1750 0x23/0x5C ────────────────
#ifndef YETI_PIN_SDA
#define YETI_PIN_SDA        8
#endif
#ifndef YETI_PIN_SCL
#define YETI_PIN_SCL        9
#endif

// ── Touch capacitivo do topo da cabeça (TTP223, saída ativa alta) ──────────────
#ifndef YETI_PIN_TOUCH
#define YETI_PIN_TOUCH      1
#endif
#ifndef YETI_TOUCH_ACTIVE
#define YETI_TOUCH_ACTIVE   HIGH  // TTP223 com jumper A aberto; LOW se o jumper A for fechado
#endif

// ── Comportamento ──────────────────────────────────────────────────────────────
#define YETI_HOLD_PTT_MS    500   // segurar a cabeça = falar (PTT)
#define YETI_TAP_GAP_MS     350   // fim de uma sequência de toques
#define YETI_INFO_MS        4000  // tempo da tela de temperatura/umidade
#define YETI_DARK_LUX       3.0f  // abaixo disso por YETI_DARK_S → dorme
#define YETI_DARK_S         30
#define YETI_COLD_C         18.0f
#define YETI_HOT_C          30.0f
#define YETI_FPS            25
