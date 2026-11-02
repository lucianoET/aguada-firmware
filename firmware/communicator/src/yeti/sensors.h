#pragma once
#include <stdint.h>
#include <math.h>

// Sensores I2C do Yeti, todos opcionais (detectados no boot). Lidos numa task própria:
// IMU a 50 Hz, luz a 5 Hz, HTU21D a cada 5 s. O Wire é só desta task.

struct YetiSensors {
    bool  htu = false, imu = false, lux = false;
    float temp_c = NAN, rh = NAN;        // HTU21D
    float lux_v = NAN;                   // BH1750
    float ax = 0, ay = 0, az = 1;        // g (IMU)
    float gx = 0, gy = 0, gz = 0;        // °/s
    uint32_t shake_ms = 0;               // millis() do último chacoalhão
    uint32_t tap_ms = 0;                 // millis() da última batida no corpo
};

void sensors_begin(void);                // scan I2C + init; cria a task
const YetiSensors &sensors(void);
