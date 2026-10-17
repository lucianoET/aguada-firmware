#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Sondas de temperatura DS18B20 em cabo — TODAS NO MESMO BARRAMENTO 1-Wire (1 GPIO):
//   PROBE_EXT — ar de entrada da condensadora (unidade externa)
//   PROBE_SUP — ar de insuflamento da evaporadora (saída da unidade interna)
//   PROBE_RET — ar de retorno da evaporadora (entrada da unidade interna)
//   ΔT da evaporadora = RET − SUP (sem RET, cai para o AHT21 da caixa − SUP).
// Num barramento só, a porta não diz mais qual sonda é qual: o papel de cada uma é
// amarrado ao ID ROM (64 bit) do DS18B20 e gravado em NVS (namespace "probes").
//   · ROM já conhecida  → mantém o papel (trocar a sonda de conector não confunde).
//   · ROM nova          → ocupa o 1º papel livre (EXT, SUP, RET) e é gravada.
//     "Livre" = sem ROM gravada, ou com ROM gravada que não está no barramento.
//   · Ficou trocado?    → probe_swap(a, b) (portal) troca dois papéis e grava.
// Hot-plug: enquanto faltar alguma sonda, o barramento é re-varrido a cada tick.
// Conversão não-bloqueante: pede a conversão (todas de uma vez) num tick e lê no
// seguinte (tick = 8 s). Hardware: 1 pull-up 4k7 DQ→3V3 para o barramento todo;
// cabos curtos (≲ 5 m somados) — barramento em estrela com cabos longos falha.
enum ProbeId : uint8_t { PROBE_EXT = 0, PROBE_SUP = 1, PROBE_RET = 2, PROBE_COUNT = 3 };

typedef struct {
    float temp_c;
    bool  present;   // sonda detectada no barramento
    bool  valid;     // leitura válida neste tick
} ProbeReading;

void probe_init(int8_t pin);   // -1 = desactivado

// Lê o resultado da conversão anterior e dispara a próxima.
void probe_tick(void);

ProbeReading probe_get(ProbeId id);

// Troca dois papéis (ROMs gravadas e leituras) e grava em NVS. Ignora a==b / fora da faixa.
void probe_swap(ProbeId a, ProbeId b);

// Nome curto do papel: "EXT", "SUP", "RET".
const char *probe_name(ProbeId id);

// ROM do papel em hex ("28FF…", 16 chars) ou "" se não atribuído. buf ≥ 17.
void probe_rom_str(ProbeId id, char *buf);

// ── Identificador de sondas (portal) ─────────────────────────────────────────
// Modo de 5 min: lê TODAS as sondas do barramento a cada 1 s e guarda a 1ª leitura
// de cada uma como referência. Aquece-se uma sonda de cada vez com a mão; a que sobe
// é a que está na mão → atribui-se o papel e cola-se a etiqueta com o ID curto
// (4 hex = bytes 1–2 da ROM, os primeiros do nº de série).
void probe_ident_start(void);          // (re)inicia e zera as referências
void probe_ident_stop(void);
bool probe_ident_active(void);
void probe_ident_tick(void);           // chamar no loop(); só trabalha com o modo ligado
// Atribui o papel `role` à sonda com esta ROM (16 hex). Se a ROM já tinha outro papel,
// os dois papéis trocam. Grava em NVS. Devolve false se ROM inválida/ausente.
bool probe_assign(const char *rom_hex16, ProbeId role);
// JSON do estado para o portal: {"ident":b,"left":s,"dev":[{"rom","role","t","d","ok"}]}
void probe_ident_json(char *buf, size_t len);
// Nº de sondas vistas na última varredura (com ou sem papel).
uint8_t probe_bus_count(void);
