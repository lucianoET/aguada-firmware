#pragma once
#include <stdint.h>
#include "face.h"

// Corpo do Yeti: touch nativo dos braços (fita de cobre), LEDs WS2812 das bochechas e
// tensão da bateria. Tudo chamado da ui_task. "Esquerdo/direito" = de quem olha de frente.

enum ArmEvt : uint8_t {
    ARM_NONE,
    ARM_TAP_L, ARM_TAP_R,       // toque curto: não / sim
    ARM_HOLD_L, ARM_HOLD_R,     // segurar: gatilho
    ARM_HUG,                    // os dois braços juntos: abraço
};

void   body_begin(void);
ArmEvt body_arms_tick(void);                 // ~200 Hz; devolve um evento por vez
void   body_leds(Mood m, uint8_t bright, float level);   // cor das bochechas pelo humor
void   body_leds_flash(uint8_t r, uint8_t g, uint8_t b, uint16_t ms);
float  body_vbat(void);                      // V (NAN sem divisor ligado)
int    body_vbat_pct(void);                  // 0..100, -1 sem bateria
