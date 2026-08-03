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

## Fase 01.1 — Periféricos I2C e display

Adiciona 4 módulos I2C ao mesmo barramento da bancada (GPIO6 SDA / GPIO7 SCL
no ESP32-C3 SuperMini — ver [`gps_tracker_pinout.md`](gps_tracker_pinout.md)
seção "Fase 01.1"): OLED SSD1306 (status ao vivo), MPU6050 (movimento →
cadência), HMC5883L/QMC5883L (rumo), HTU21D (temp/umidade). O simulador
(`GPS_BENCH_SIM`) e os hotkeys da Fase 1 acima continuam válidos e
inalterados — os periféricos desta fase nunca bloqueiam o pipeline GPS.

### Comandos da serial (estendido)

Todos os comandos da Fase 1 (`r`/`h`/`s`/`+`/`-`/`x`/`z`/`b`) continuam de
**um único caractere, sem Enter**. `cal` é o único comando novo e é
**diferente dos demais: precisa de Enter** para ser reconhecido (é
multi-caractere — o parser acumula em um buffer de 8 bytes e só age ao
receber `\r`/`\n`).

| Comando | O que faz | O que prova |
|---------|-----------|--------------|
| `cal` + Enter | Inicia uma captura de calibração hard-iron da bússola (repetir `cal` + Enter encerra e salva); ver procedimento completo abaixo | D-09: offsets min/max persistidos em NVS, sobrevivem a reboot, capturas degeneradas são rejeitadas |

### Campos novos do `[HEALTH]`

| Campo | Significado | O que um valor anômalo indica |
|-------|--------------|-------------------------------|
| `disp=`/`accel=`/`mag=`/`env=` | Estado online/offline de cada módulo I2C (`on`/`off`) | `off` persistente com o módulo fisicamente ligado indica falha de fiação ou endereço errado (D-12) |
| `i2c_drops=` | Contador acumulado de eventos de offline (soma dos 4 módulos) desde o boot | Crescendo continuamente com os módulos bem fixados aponta para um barramento instável (pull-ups, comprimento de fio, ruído) — considerar o fallback de 100 kHz |
| `accel_g=` | Magnitude de aceleração mais recente, em g (1.0g parado, plano) | Preso em 0.00 ou nunca variando com o módulo sendo mexido indica accel offline ou mal contatado |
| `accel_wakes=` | Contador acumulado de despertares (edges) de movimento detectados pelo accel | Crescendo sem o veículo se mover sugere `DEFAULT_ACCEL_WAKE_THRESHOLD_G` baixo demais para a vibração de fundo do local de montagem |
| `hdg=` | Rumo exibido em graus (já arbitrado entre bússola e GPS) | Não mudar ao girar o módulo com o veículo parado (`hdg_src=mag`) indica falha de calibração ou bússola offline |
| `hdg_src=` | Origem do rumo exibido: `mag` (bússola, parado/devagar) ou `gps` (curso do GPS, em movimento) | Preso em `gps` mesmo parado indica que a bússola caiu offline (D-08 recua para GPS course quando o módulo não responde) |
| `mag_chip=` | Chip de bússola detectado no boot (`HMC5883L`/`QMC5883L`/`VCM5883L`/`none`) | `none` com o módulo fisicamente presente indica endereço fora dos 3 candidatos (0x1E/0x0D/0x0C) ou módulo com defeito (D-11) |
| `temp=`/`hum=` | Última leitura de temperatura/umidade do HTU21D | Parado no mesmo valor por muito tempo com o ambiente mudando indica sensor offline mantendo o último valor conhecido (D-14) |
| `sats_view=` | Satélites em vista (sentenças GSV), distinto de `sats=` (satélites usados no fix) | `sats_view=0` com `rx=yes` sugere receptor sem sentenças GSV habilitadas ou GSV ainda não decodificado; normal nos primeiros segundos |
| `disp_addr=` | Endereço I2C onde o OLED foi encontrado (`0x3C`/`0x3D`), ou `0x00` se nunca encontrado | `0x00` persistente com o OLED ligado indica fiação ou endereço incorreto |

### Prefixos de linha novos

| Prefixo | Quando aparece | Campos |
|---------|-----------------|--------|
| `[I2C]` | Um módulo offline volta a responder após re-init automático (D-13) | `%s recovered addr=0x%02X` — nome do módulo e endereço |
| `[MAG]` | Uma vez no boot, ao detectar (ou não) a bússola | `chip=%s addr=0x%02X` |
| `[ACCEL]` | Um edge de despertar confirmado pelo debounce do accel (D-05/D-06) | `wake mag=%.2fg streak=%u` |
| `[CAL]` | Durante e ao final de uma captura de calibração da bússola (comando `cal`) | `capture started ...` / `active %lus x=[..] y=[..]` (periódico, dentro do `[HEALTH]`) / `saved xMin=.. xMax=.. yMin=.. yMax=..` / `rejected -- insufficient rotation ...` / `timeout -- auto-finishing capture` |

### Roteiro de verificação — Fase 01.1

Todos os passos abaixo usam bancada (não precisam de céu real), exceto onde
indicado. Fazer depois do roteiro da Fase 1 acima, com os 4 módulos
montados conforme `gps_tracker_pinout.md`.

1. **D-01 — tela densa** — com fix (real ou simulado), confirmar que fix/sats/HDOP,
   velocidade, rumo, temp/umidade e estado da cadência aparecem **todos ao
   mesmo tempo** no OLED, sem paginação e sem precisar de botão.
2. **D-02 — refresh a 1 Hz alinhado ao fix** — cronometrar: a tela atualiza
   ~1x/s e o valor mostrado corresponde ao mesmo fix que acabou de sair como
   `[EMIT]` no serial, não ao anterior.
3. **D-03 — tela de aquisição** — antes do fix (ou com o simulador desligado
   e sem GPS real), confirmar "NO FIX", satélites em vista, indicador de
   NMEA, uptime, e que rumo/temp-umidade/aceleração continuam vivos na
   mesma tela.
4. **D-04 — unidade de velocidade e rumo** — recompilar com
   `-DDEFAULT_SPEED_UNIT=1` e gravar: a tela passa a mostrar nós (`kt`) em
   vez de km/h, sem editar código-fonte; confirmar que o rumo sempre vem com
   graus + ponto cardinal.
5. **D-05/D-06/D-07 — accel promove, GPS rebaixa** — com o simulador
   parado (`s` ligado, velocidade 0), sacudir o módulo: `[ACCEL] wake ...`
   seguido de `[STATE] STATIONARY -> MOVING` deve aparecer quase
   imediatamente (sem esperar fixes). Confirmar que **só** baixar a
   velocidade simulada abaixo de `DEFAULT_GPS_STATIONARY_KMH` (comando `-`)
   — nunca ficar parado sem sacudir — devolve o estado a `STATIONARY`; isto
   prova que o accel promove mas nunca rebaixa.
6. **D-08 — arbitragem de rumo por velocidade** — com o simulador ligado,
   girar o módulo com velocidade simulada abaixo de
   `DEFAULT_GPS_STATIONARY_KMH`: `hdg_src=mag` e o rumo acompanha o giro.
   Subir a velocidade simulada (`+` repetidas vezes) acima do limiar:
   `hdg_src=gps` e o rumo passa a seguir `crs=` do `[SIM] [EMIT]`.
7. **D-09 — calibração completa** — enviar `cal` + Enter: `[CAL] capture
   started`; girar o módulo lentamente por uma volta completa horizontal
   (observar `[CAL] active ...` no `[HEALTH]` periódico); enviar `cal` +
   Enter de novo: `[CAL] saved xMin=.. xMax=.. yMin=.. yMax=..`. Reiniciar a
   placa e confirmar que o rumo calibrado persiste (offsets recarregados da
   NVS). Repetir a captura girando muito pouco (poucos graus): confirmar
   `[CAL] rejected -- insufficient rotation` e que a calibração anterior é
   mantida.
8. **D-10 — declinação** — recompilar com um valor não-zero de
   `-DDEFAULT_MAG_DECLINATION=...` e confirmar que o rumo exibido desloca
   pelo valor configurado (sem tilt compensation — montagem assumida plana).
9. **D-11 — chip da bússola** — confirmar no `[MAG] chip=...` do boot (ou no
   campo `mag_chip=` do `[HEALTH]`) se o GY-271 em mãos é HMC5883L ou
   QMC5883L; preencher a tabela de registro de campo abaixo.
10. **D-12 — degradação graciosa** — dar boot **sem nenhum módulo** ligado:
    confirmar que o GPS/simulador seguem funcionando normalmente e que
    `disp=`/`accel=`/`mag=`/`env=` aparecem todos `off`. Repetir ligando um
    subconjunto de cada vez (só accel; só bússola; só OLED + HTU21; etc.) e
    confirmar que cada subconjunto funciona de forma independente, sem que a
    ausência de um módulo trave ou desative os demais.
11. **D-13 — recuperação em runtime** — com o firmware rodando e todos os
    módulos online, arrancar um fio de um módulo (ex.: SDA do HTU21) e
    confirmar `off` no `[HEALTH]` em até `DEFAULT_I2C_ERROR_THRESHOLD` (3)
    erros consecutivos; religar o fio e confirmar `[I2C] %s recovered
    addr=0x%02X` dentro de ~`DEFAULT_I2C_RETRY_INTERVAL_MS` (10 s), com o
    módulo voltando a `on` sozinho, sem reiniciar a placa.
12. **D-14 — HTU21 display/serial apenas** — confirmar que `temp=`/`hum=`
    respondem a mudanças reais (respirar perto do sensor, por exemplo) e que
    desconectar o HTU21D marca a última leitura como obsoleta na tela (sem
    zerar o valor) em vez de travar o restante do `[HEALTH]`.
13. **Orçamento de tempo (T-01.1-03)** — com os 4 periféricos ativos,
    observar o `[HEALTH]` por alguns minutos e confirmar que `bad=`
    (checksum ruim) e `stale=` (fix obsoleto) não crescem em relação ao
    comportamento já registrado na Fase 1 — a transferência do quadro do
    OLED (~25 ms a 400 kHz, 1x/s) não pode competir de forma perceptível com
    o consumo do buffer UART do GPS.
14. **D-05 novamente — sacudir não incomoda o GPS** — com o simulador
    desligado e o GPS real recebendo, sacudir o módulo repetidas vezes e
    confirmar que `bytes=`/`ok=`/`bad=` do `[HEALTH]` continuam avançando
    normalmente durante os despertares do accel.

### Registro de calibração de campo — Fase 01.1

| Tunável | Default atual | Valor observado em campo | Decisão |
|---------|----------------|-----------------------------|---------|
| `DEFAULT_ACCEL_WAKE_THRESHOLD_G` | 0.20 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_ACCEL_WAKE_DEBOUNCE_SAMPLES` | 3 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_ACCEL_POLL_MS` | 100 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_MAG_POLL_MS` | 250 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_HTU21_POLL_MS` | 4000 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_I2C_ERROR_THRESHOLD` | 3 | _(preencher)_ | _(preencher)_ |
| `DEFAULT_I2C_CLOCK_HZ` | 400000 | _(preencher — 100000 se o barramento com os 4 módulos se mostrar instável)_ | _(preencher)_ |
| Chip de bússola detectado (D-11) | — | _(preencher: HMC5883L ou QMC5883L)_ | — |

**Ajuste de campo:** todo `DEFAULT_*` acima é um ponto de partida da
pesquisa (`01.1-RESEARCH.md`), não um valor final. Qualquer ajuste vai em
`build_flags` no `platformio.ini` (`-DDEFAULT_ACCEL_...=...`, etc.), **nunca**
editando `gps_config.h` diretamente — mesma regra já em vigor para os
tunáveis da Fase 1 acima.
