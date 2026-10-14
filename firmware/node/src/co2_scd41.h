#pragma once
#include <stdint.h>
#include <stdbool.h>

// Sensirion SCD41 (também SCD40) — CO2 por NDIR fotoacústico, I2C 0x62.
// Opcional no node ANI: se não responder no barramento, o node continua só com o
// eCO2 *estimado* do ENS160. Este é o único CO2 válido como indicador de renovação
// de ar (RE 09/2003: <= 1000 ppm); o eCO2 do ENS160 é derivado de VOC.
//
// Driver mínimo próprio (sem lib): partilha o Wire já iniciado por ani_init().
// Modo: medição periódica (1 leitura a cada 5 s, ~15 mA médio). ASC (auto-calibração
// a 400 ppm) fica no default de fábrica = ligado; ver docs sobre ambientes fechados.
typedef struct {
    uint16_t co2_ppm;
    float    temp_c;    // T interna do SCD41 (aquece — só diagnóstico)
    float    hum_pct;
    bool     valid;
} Co2Reading;

// Detecta o sensor e arranca a medição periódica. Wire tem de estar iniciado.
bool scd41_init(void);
bool scd41_present(void);

// Lê a última medição pronta. valid=false se ausente ou ainda sem dado novo
// (nesse caso devolve o último valor válido com valid=true, se houver um recente).
Co2Reading scd41_read(void);
