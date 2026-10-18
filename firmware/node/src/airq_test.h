#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Gravador dos testes de campo do portal (Teste AC / Teste ambiente).
// O firmware só GRAVA: uma amostra por tick local (8 s) numa memória RAM fixa, e
// continua a gravar com o celular desligado do AP. A avaliação (critérios, laudo,
// CSV) é feita no portal, a partir de GET /test. Perde-se num reboot.
enum TestKind : uint8_t { TEST_NONE = 0, TEST_AC = 1, TEST_ENV = 2 };
enum TestState : uint8_t { TS_IDLE = 0, TS_RUN = 1, TS_DONE = 2 };

#define TEST_NULL   INT16_MIN       // canal sem leitura nesta amostra
#define TEST_MAXN   480             // 64 min a 8 s
#define TEST_NCOL   11

// Colunas (int16, escala fixa) — a mesma ordem vai no JSON ("cols")
//  ta  T ambiente AHT21 ×10   rh UR ×10      tr T retorno ×10   ts T insuflamento ×10
//  te  T externa ×10          a  corrente ×100 db dB(A) ×10     co2 SCD41 ppm
//  eco2 ENS160 ppm            tvoc ppb       lux lux
typedef struct { int16_t v[TEST_NCOL]; } TestSample;

void      test_start(TestKind kind, uint32_t dur_s, const char *tag, const char *opt);
void      test_stop(void);
void      test_add(const TestSample &s);     // chamar a cada tick; ignora se não estiver a gravar
TestState test_state(void);
TestKind  test_kind(void);

// JSON em pedaços (o histórico pode ter ~20 kB): emit() é chamado várias vezes.
typedef void (*test_emit_fn)(const char *chunk, void *ctx);
void test_json(test_emit_fn emit, void *ctx);
