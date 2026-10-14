# TODO — Node AirQ (qualidade do ar + analisador de AC)

Visão geral em [`Node AirQ.md`](Node%20AirQ.md).

## 🔴 Validação em bancada (código pronto e a compilar, falta hardware)

- [ ] **Confirmar GPIO20/21 expostos** no SuperMini usado (serigrafia RX/TX). Se não,
      mover INMP441 SCK/WS por build flag (`MIC_SCK_PIN`/`MIC_WS_PIN`) para 0/2.
- [ ] **INMP441:** L/R→GND; ver no serial `Leq=… dB SPL` ~30–40 em silêncio e a subir
      com palmas. Sem mic: pill "falha" e nada enviado. Se tiver sonómetro, acertar
      `MIC_DB_OFFSET`.
- [ ] **SCD41:** log `SCD41 OK serial=…`; CO₂ exterior ~420 ppm, sobe ao respirar perto.
- [ ] **DS18B20 EXT/SUP:** ligar/desligar a quente → secção "Ar-condicionado" aparece/
      some; trocar as sondas de porta → nomes acompanham a porta.
- [ ] **TC:** montar bias 10k/6k8 + 10 µF e jack com comutação. Sem plugue → sem card;
      com plugue em vazio → ~0 A; carga conhecida (ex. ferro 1000 W ≈ 4.5 A em 220 V)
      → conferir com alicate amperímetro e ajustar `CT_AMPS_PER_VOLT`/`CT_NOISE_MV`.
- [ ] **AHT21:** medir desvio contra termómetro de referência (aquecimento do ENS160).
      Se > 1 °C, decidir: offset em firmware ou 3ª sonda no retorno do ar.
- [ ] **Rajada de rádio:** 8–9 pacotes por ciclo — confirmar no gateway que nenhum se perde.

## 🟠 Gateway

- [ ] Reflash dos gateways: o JSON de SENSOR agora inclui `reserved` (AQI). Antes disto
      o AQI do pacote sid 1 chegava sempre 0 à bridge. (O AQI também vem no canal
      `0xF6`, que já funciona sem reflash.)
- [ ] Registar o node AirQ real em `tools/reservoirs.yaml` (`type: air_quality`) — sem isso
      o pacote eCO₂ (sid 1) é descartado pela bridge. Os canais 0xF0–0xF6 não precisam.

## 🟡 Evolução

- [ ] Ponderação A no INMP441 (filtro IIR) para dB(A) comparável a NR-15/NBR 10152.
- [ ] Persistir baseline do ENS160 em NVS (reduz warmup entre reboots).
- [ ] SCD41: avaliar desligar ASC em salas que nunca ventilam (ASC assume 400 ppm/semana).
- [ ] Bridge: ciclo de trabalho e kWh estimado do compressor (integração da corrente).
- [ ] Integração com o PMOC: vincular node_id ↔ equipamento (TAG) e anexar histórico ΔT.
- [ ] Renomear SSID `ANI-…` → `ARQ-…` (coerência com o nome AirQ).

## Histórico

- 2026-10-14 — KY-037/038 removido (nunca leu: alimentação/ligação), substituído por
  INMP441 I2S. Luz e som passam a ir por ESP-NOW/MQTT (canais 0xF4/0xF5).
