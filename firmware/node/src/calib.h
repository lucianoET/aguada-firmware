#pragma once
// Calibração de campo dos sensores do AirQ, gravada em NVS (namespace "calib").
// Offsets são somados à leitura bruta; ganhos multiplicam. Valores por omissão
// = sem correção. As sondas DS18B20 têm o seu offset em probe_temp (viaja com o
// papel/ROM), não aqui.
//
//   t_aht   offset °C do AHT21 (a placa aquece com o C3/ENS160 → tipicamente negativo)
//   rh_aht  offset %UR do AHT21
//   db      offset dB(A) do INMP441 (contra sonómetro/app de referência)
//   ct      ganho do TC (corrente lida × ct) — contra alicate amperímetro
//   lux     ganho do TEMT6000 (lux lido × lux) — contra luxímetro
typedef struct {
    float t_aht;
    float rh_aht;
    float db;
    float ct;
    float lux;
} Calib;

void         calib_load(void);
const Calib &calib(void);
// k: "t" | "rh" | "db" | "ct" | "lux". Valida a faixa; devolve false se inválido.
bool         calib_set(const char *k, float v);
void         calib_reset(void);   // zera offsets e ganhos (as sondas têm reset próprio)
