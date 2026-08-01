# GPS Tracker — Bancada (Fase 1)

Procedimento para provar GPS-01, GPS-02 e GPS-03 a partir de uma mesa, sem
veículo e sem túnel — usando o simulador de bancada embutido no firmware
(`GPS_BENCH_SIM`), mais os testes que exigem céu real com os dois módulos
GPS suportados (ATGM336H, NEO-6M).

## Montagem

Ver [`gps_tracker_pinout.md`](gps_tracker_pinout.md) para a pinagem oficial
(UART do GPS, LED onboard, pinos reservados/proibidos) nas duas placas
suportadas (ESP32-C3 SuperMini e ESP32 DevKit clássico).

**Nota de alimentação:** o breakout NEO-6M (GY-GPS6MV2) tem um LDO AMS1117 e
precisa de **5 V** — em 3V3 ele faz brownout e cala. O ATGM336H já roda em
**3V3** direto. Confira a tensão do módulo antes de ligar.

## Build e flash

```bash
cd firmware/gps_tracker

# ESP32-C3 SuperMini (antena externa, /dev/ttyACM0)
/home/luc/Dev/aguada-firmware-main/.venv/bin/python -m platformio run -e esp32-c3-supermini -t upload --upload-port /dev/ttyACM0

# ESP32 DevKit clássico
/home/luc/Dev/aguada-firmware-main/.venv/bin/python -m platformio run -e esp32-devkit -t upload

# Monitor serial (os dois, 115200 baud)
/home/luc/Dev/aguada-firmware-main/.venv/bin/python -m platformio device monitor
```

(Substitua `pio` pelo caminho acima — o binário não está no `PATH` deste
ambiente de desenvolvimento; em outra máquina com `pio` instalado global,
os comandos `pio run ...` funcionam normalmente.)

## Comandos da serial

Um único caractere por comando, sem Enter necessário. `h`/`+`/`-`/`s`/`x`/`z`/`b`
só existem quando `GPS_BENCH_SIM` está ativo (default: `1` nesta fase).

| Comando | O que faz | O que prova |
|---------|-----------|--------------|
| `r` | Liga/desliga o eco do NMEA cru recebido do módulo GPS | Que o UART está de fato recebendo sentenças do módulo (debug de fiação/baud) |
| `h` | Imprime um bloco `[HELP]` listando todos os comandos | Referência rápida sem precisar reabrir este arquivo |
| `s` | Liga/desliga o simulador de bancada (1 fix sintético/s enquanto ligado) | Que gate e cadência funcionam sem GPS real — a mesma pipeline `FixGate::evaluate()` → cadência que os fixes reais usam |
| `+` | Aumenta a velocidade simulada em 2 km/h (0-150) | Transição parado → movendo, e o espaçamento de ~8 s em `[EMIT]` |
| `-` | Diminui a velocidade simulada em 2 km/h (0-150) | Transição movendo → parado |
| `x` | Injeta 1 fix com salto implausível de posição | `REJECT_OUTLIER_JUMP` |
| `z` | Injeta 1 fix em (0,0) | `REJECT_NULL_ISLAND` |
| `b` | Injeta 1 fix com HDOP ruim e poucos satélites | `REJECT_HDOP` |

## Formato das linhas

| Prefixo | Quando aparece | Campos |
|---------|-----------------|--------|
| `[FIX]` | *(não emitido diretamente — ver `[ACCEPT]`/`[REJECT]` abaixo, que já carregam o resultado do gate sobre o snapshot mais recente)* | — |
| `[REJECT]` | Todo fix (real ou `[SIM]`) que o gate rejeitou, com dedupe do motivo consecutivo | `reason=<GateResult>` `hdop=` `sats=` `age=Xms` `q=<fix_quality>` |
| `[EMIT]` | Fix aceito pelo gate **e** liberado pela cadência (`EMIT`/`EMIT_HEARTBEAT`) | `t=<utc_unix>` `lat=` `lon=` `alt=Xm` `spd=Xkm/h` `crs=` `hdop=` `sats=` `q=` `age=Xms` `state=<MOVING|STATIONARY>`; heartbeat leva `hb=1` |
| `[SUPPRESS]` | Fix aceito pelo gate mas retido pela cadência | `reason=<CadenceAction>` `spd=Xkm/h` `state=<MOVING\|STATIONARY>` |
| `[STATE]` | Toda transição MOVING↔STATIONARY (debounce confirmado) | `<old> -> <new>` `spd=Xkm/h` `elapsed_in_prev_state=Xs` |
| `[HEALTH]` | Periódico (`DEFAULT_GPS_HEALTH_PERIOD_MS`, default 5 s) | contadores de bytes/checksum do UART, contadores por motivo de rejeição do gate, `cadence=<estado>`, `since_emit=Xs`, `emit=` e `suppress=` acumulados |
| `[SIM]` | Prefixo adicionado em **toda** linha originada pelo simulador (`[SIM] [EMIT]...`, `[SIM] [REJECT]...`, `[SIM] [STATE]...`, além de mensagens de controle como `[SIM] speed=...`) | garante que um log capturado nunca confunda dado sintético com fix real |

## Roteiro de verificação da Fase 1

Passos 1-3 e 8 exigem céu real (antena com vista livre, ex.: perto de
janela); os demais usam o simulador (`s`, `+`, `-`, `x`, `z`, `b`) e podem
ser feitos 100% de mesa.

1. **Cold boot, sem fix** *(céu real)* — ligar sem lock ainda: `[REJECT]
   reason=REJECT_NO_FIX` continuamente até o primeiro dado chegar do módulo.
2. **Warmup** *(céu real)* — assim que o GPS trava o primeiro fix válido,
   esperar `REJECT_WARMUP` por `DEFAULT_GPS_WARMUP_FIXES` (default 3) fixes
   consecutivos antes do primeiro `ACCEPT`.
3. **Primeiro accept + antena coberta** *(céu real)* — confirmar o primeiro
   `[EMIT]`/`[SUPPRESS]`, depois cobrir a antena e confirmar `REJECT_STALE`
   dentro de `DEFAULT_GPS_FRESH_MS` (default 2500 ms) — este é o watchdog
   descrito em `01-01-SUMMARY.md` (a validade do TinyGPSPlus é um latch de
   uma via; sem o watchdog um fix congelado nunca seria rejeitado).
4. **Rejeições injetadas** *(simulador)* — `s` para ligar, depois `x`, `z` e
   `b` em sequência, confirmando `REJECT_OUTLIER_JUMP`, `REJECT_NULL_ISLAND`
   e `REJECT_HDOP` respectivamente. Note que `z` e `b` zeram o streak de
   warmup do gate (mesma regra que um fix real ruim) — espere alguns
   `REJECT_WARMUP` antes do próximo `[EMIT]`; `x` não zera o warmup, então o
   próximo fix normal já é aceito.
5. **Supressão parado** *(simulador)* — com o simulador ligado a 0 km/h,
   confirmar `[SIM] [SUPPRESS] reason=SUPPRESS_STATIONARY state=STATIONARY`
   se repetindo, sem nenhuma linha `[STATE]` — a banda de histerese segura
   contra o ruído real observado na bancada (~2.6 km/h).
6. **Transição parado → movendo** *(simulador)* — apertar `+` repetidas
   vezes até passar de `DEFAULT_GPS_MOVING_KMH` (5.0); confirmar `[SIM]
   [STATE] STATIONARY -> MOVING` só depois de `DEFAULT_GPS_DEBOUNCE_FIXES`
   (default 3) fixes consecutivos confirmando, seguido de um `[EMIT]`
   imediato.
7. **Cadência ~8 s em movimento** *(simulador)* — com velocidade acima do
   limiar, confirmar `[SIM] [EMIT]` a cada ~8 s (`DEFAULT_GPS_CADENCE_MOVING_S`)
   e `[SIM] [SUPPRESS] reason=SUPPRESS_INTERVAL` nos fixes intermediários (a
   1 Hz, isso é ~7 supressões por emissão).
8. **Transição movendo → parado** *(simulador, ou céu real dirigindo/andando)*
   — apertar `-` até abaixo de `DEFAULT_GPS_STATIONARY_KMH` (3.0); confirmar
   o `[STATE] MOVING -> STATIONARY` espelhado após o debounce e que a
   emissão cessa.
9. **Dois módulos** *(céu real, ver seção abaixo)* — repetir os passos 1-3
   com ATGM336H e depois com NEO-6M, confirmando que o mesmo binário
   funciona sem alteração de código (D-02).

## Verificação com os dois módulos

Este firmware é NMEA-agnóstico por decisão de projeto (D-02) — nenhum
comando UBX/CASIC proprietário é enviado. Para confirmar isso na prática:

1. Ligar `r` (raw echo) logo no boot, antes do primeiro fix.
2. Capturar ~30 s de NMEA cru de cada módulo (copiar do monitor serial).
3. Anotar os talkers observados em cada sentença (`$GNRMC`/`$GPRMC`/`$GNGGA`/
   `$GPGGA` etc.) na tabela abaixo — preencher após o teste em bancada:

| Módulo | Talker RMC observado | Talker GGA observado | Observações |
|--------|------------------------|-------------------------|--------------|
| ATGM336H | _(preencher)_ | _(preencher)_ | GPS+BeiDou — pode emitir `$GB*`/`$BD*` além de `$GN*` |
| NEO-6M | _(preencher)_ | _(preencher)_ | u-blox, só GPS — tipicamente `$GP*` |

## Registro de calibração de campo

Todo valor abaixo é um ponto de partida da pesquisa (`01-RESEARCH.md`), não
um valor final. Qualquer ajuste vai em `build_flags` no `platformio.ini`
(`-DDEFAULT_GPS_...=...`), **nunca** editando este header diretamente.

| Tunável | Default atual | Assumption (`01-RESEARCH.md`) | Valor observado em campo | Decisão |
|---------|----------------|-------------------------------|----------------------------|---------|
| `DEFAULT_GPS_HDOP_MAX` | 5.0 | A1 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_GPS_SATS_MIN` | 4 | A2 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_GPS_WARMUP_FIXES` | 3 | A4 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_GPS_MAX_PLAUSIBLE_KMH` | 200.0 | — (não listado como assumption numerada; consistente com A1/A2) | _(preencher)_ | _(preencher)_ |
| `DEFAULT_GPS_MOVING_KMH` | 5.0 | A3 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_GPS_STATIONARY_KMH` | 3.0 | A3 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_GPS_DEBOUNCE_FIXES` | 3 | A4 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_GPS_CADENCE_MOVING_S` | 8 | A5 | _(preencher)_ | _(preencher)_ |

**Preencher a coluna "Valor observado" após um teste estacionário de várias
horas no local de montagem real** (painel/casco do veículo/embarcação —
pior vista de céu que a bancada perto de janela onde o jitter de ~2.6 km/h
foi observado). A `01-RESEARCH.md` (Pitfall 5, Assumption A3) já avisa que
um local de montagem pior pode exigir uma banda de histerese mais larga.

## Nota

`GPS_BENCH_SIM` **deve** ser `0` em `build_flags` antes de qualquer build
que fale com rede (Fase 3 em diante, quando WiFi/MQTT entrarem em escopo).
O simulador de bancada é um recurso deliberadamente offline/bancada-only —
deixá-lo ligado num build de campo permitiria injetar fixes sintéticos pela
serial USB, o que nunca deve acontecer fora do desenvolvimento.
