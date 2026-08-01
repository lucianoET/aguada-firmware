---
status: testing
phase: 01-gps-acquisition-adaptive-cadence
source: [01-VERIFICATION.md]
started: 2026-08-01T22:00:00Z
updated: 2026-08-01T22:00:00Z
---

## Current Test

number: 1
name: Roteiro de bancada BENCH.md (9 passos)
expected: |
  Flash em esp32-c3-supermini (/dev/ttyACM0), monitor 115200, seguir
  firmware/gps_tracker/BENCH.md "Roteiro de verificação da Fase 1":
  cold boot, warmup, primeiro ACCEPT + staleness ao cobrir antena,
  rejeições injetadas via x/z/b, supressão parado, transição p/ movimento,
  cadência ~8 s, transição de volta, confirmação nos dois módulos GPS.
  Todos os GateResult/CadenceAction alcançáveis com prefixos corretos
  ([REJECT]/[EMIT]/[SUPPRESS]/[STATE]/[HEALTH]); timings ≈ gps_config.h
  (stale 2500 ms, cadência 8 s, debounce 3 fixes); LED acompanha estado.
awaiting: user response

## Tests

### 1. Roteiro de bancada BENCH.md (9 passos)
expected: Todos os GateResult/CadenceAction alcançáveis e impressos com prefixo correto; timings ≈ constantes de gps_config.h; LED acompanha gate/cadência
result: [pending]

### 2. Regressão CR-02 — simulador → real sem lockout
expected: Rodar simulador com movimento (s, + até >5 km/h, deixar emitir), desligar (s de novo) — próximo fix real avaliado contra referência resetada (fixGate.reset()), sem REJECT_OUTLIER_JUMP permanente
result: [pending]

### 3. Decisão — forma do goal MVP no ROADMAP
expected: Ou rodar /gsd-mvp-phase 1 para canonizar o goal em User Story, ou aceitar explicitamente verificação padrão (não-MVP) para fases de firmware
result: [pending]

## Summary

total: 3
passed: 0
issues: 0
pending: 3
skipped: 0
blocked: 0

## Gaps
