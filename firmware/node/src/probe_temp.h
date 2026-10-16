#pragma once
#include <stdint.h>
#include <stdbool.h>

// Sondas de temperatura DS18B20 em cabo — uma porta 1-Wire por GPIO, para o firmware
// saber qual é qual sem mapear ROM IDs (trocar o cabo de porta não confunde nada):
//   PROBE_EXT — ar de entrada da condensadora (unidade externa)
//   PROBE_SUP — ar de insuflamento da evaporadora (saída da unidade interna)
// Ambas opcionais e "hot-plug": a cada leitura, porta sem sonda é re-procurada.
// Conversão não-bloqueante: pede a conversão num tick e lê no seguinte (tick = 8 s).
// Hardware por porta: pull-up 4k7 DQ→3V3 na placa + TVS/série 100 Ω recomendados.
enum ProbeId : uint8_t { PROBE_EXT = 0, PROBE_SUP = 1, PROBE_COUNT = 2 };

typedef struct {
    float temp_c;
    bool  present;   // sonda detectada no barramento
    bool  valid;     // leitura válida neste tick
} ProbeReading;

void probe_init(int8_t pin_ext, int8_t pin_sup);   // -1 = porta desactivada

// Lê o resultado da conversão anterior de cada porta e dispara a próxima.
void probe_tick(void);

ProbeReading probe_get(ProbeId id);
