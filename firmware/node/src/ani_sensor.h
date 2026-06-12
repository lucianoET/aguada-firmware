#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t eco2;      // eCO2 in ppm (400–65000)
    uint16_t tvoc;      // TVOC in ppb (0–65000)
    uint8_t  aqi;       // UBA AQI index 1–5 (1=excellent … 5=unhealthy); 0=unavailable
    float    temp_c;    // temperature in °C (AHT21)
    float    hum_pct;   // relative humidity in % (AHT21)
    bool     valid;
} AniReading;

// Initialise I2C bus and both sensors. Returns true if both respond on the bus.
bool ani_init(uint8_t sda_pin, uint8_t scl_pin);

// Take a measurement. Reads AHT21 first, feeds T/H to ENS160 for compensation,
// then reads ENS160. Returns valid=false on any I2C error.
AniReading ani_read(void);

// Report per-sensor health detected at init (and updated on reads).
void ani_status(bool *aht_ok, bool *ens_ok);
