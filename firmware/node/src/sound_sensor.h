#pragma once
#include <stdint.h>
#include <stdbool.h>

// KY-037 / KY-038 — electret microphone + LM393. We use the ANALOG output (A0),
// which carries the raw audio waveform biased around ~Vcc/2. Read on an ADC1 pin
// (GPIO0–4 on the ESP32-C3). Power the module at 3.3 V so peaks stay within the ADC.
//
// This is NOT a calibrated dB-SPL meter: it samples A0 over a short window and
// derives RMS / peak-to-peak amplitude, mapped to a RELATIVE "dB-like" level for
// ambient-comfort indication only.
typedef struct {
    uint16_t mean;   // DC bias of the window (ADC counts) — ~2048 if biased at Vcc/2
    uint16_t rms;    // RMS amplitude (ADC counts around the DC bias)
    uint16_t vpp;    // peak-to-peak amplitude (ADC counts)
    uint8_t  db;     // relative level, "dB-like" (uncalibrated)
    bool     valid;
} SoundReading;

// Configure the ADC pin (12-bit, full-range attenuation).
void sound_init(uint8_t adc_pin);

// Sample the analog output over a short window and return rms/vpp/db.
// Blocks for a few tens of ms while sampling.
SoundReading sound_read(void);
