# Requirements: Aguada — Node GPS Tracker (sem GSM)

**Defined:** 2026-08-01
**Core Value:** Nenhum ponto do trajeto se perde: o tracker registra continuamente offline e sincroniza tudo sozinho assim que qualquer canal de conectividade aparece.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### GPS

- [ ] **GPS-01**: Node lê NEO-6M/M8N via UART e extrai lat/lon/velocidade/altitude/curso + timestamp UTC (TinyGPSPlus)
- [ ] **GPS-02**: Fixes passam por gating de qualidade (validade, HDOP, sats) — fix ruim nunca entra no log nem alimenta a cadência
- [ ] **GPS-03**: Cadência adaptativa: ponto a cada ~5–10 s em movimento, pausa quando parado (classificação por velocidade GPS abaixo de limiar por N segundos)

### Logging (LOG)

- [ ] **LOG-01**: Todo fix aceito é gravado em flash (LittleFS, registros binários fixos, append + rotação de segmentos) antes de qualquer tentativa de rede
- [ ] **LOG-02**: Log sobrevive a reboot e queda de energia sem corromper o segmento em curso
- [ ] **LOG-03**: Cursor de upload persistido em flash; só avança após confirmação do servidor (idempotência — zero pontos perdidos ou duplicados)

### Sync (Canal 2 — store & forward)

- [ ] **SYNC-01**: Node detecta WiFi conhecido (SSID/senha em NVS) e conecta oportunisticamente
- [ ] **SYNC-02**: Ao conectar, despeja histórico pendente em lote (JSON via MQTT/HTTP) a partir do cursor
- [ ] **SYNC-03**: Reconexão/retry com backoff em queda de WiFi/MQTT; despejo retoma do cursor

### Servidor (SRV)

- [ ] **SRV-01**: Servidor ingere lotes do tracker (extensão bridge.py ou módulo próprio), armazena histórico completo keyed por node_id
- [ ] **SRV-02**: HA mostra posição live via `device_tracker` MQTT Discovery (`json_attributes_topic`: latitude, longitude, gps_accuracy, speed, altitude, course, battery, power_source)
- [ ] **SRV-03**: `tools/gpx_export.py` converte histórico ingerido em GPX (fonte de verdade do trajeto — HA não retro-data histórico)

### Energia (PWR)

- [ ] **PWR-01**: Alimentação híbrida 12V veículo + Li-ion backup com chaveamento automático de fonte
- [ ] **PWR-02**: Tensão da bateria e fonte ativa (external/battery) reportadas como atributos no HA

### Hardware futuro (HW)

- [ ] **HW-01**: Design do protótipo reserva pinos/conectores para integração futura: I2C (OLED), GPIOs (botões, buzzer, LEDs), segundo UART (saída NMEA 0183 p/ radar marítimo)

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Sync avançado

- **SYNC-04**: Canal 1 — live via hotspot do celular (MQTT pela internet)
- **SYNC-05**: Canal 3 — posição via ESP-NOW mesh (novo packet type `PKT_GPS_POS`, gateway/bridge decodificam)
- **SYNC-06**: Arbitragem de canais (state machine de prioridade: live > bulk > mesh quando múltiplos disponíveis)

### GPS/Logging

- **GPS-04**: Marcação de gaps de sinal (túnel/ponte/cabine) — lacuna explícita no log/GPX em vez de teleporte

### Energia

- **PWR-03**: Deep sleep com wake por movimento/ignição; VBAT do GPS mantido vivo no sleep (warm start)

### Periféricos

- **HW-02**: OLED com status (fix, sats, canal ativo, bateria) + botões
- **HW-03**: Buzzer/LEDs de status
- **HW-04**: Saída serial NMEA 0183 para radar/chartplotter marítimo
- **HW-05**: Sensor de inclinação (IMU simples) e outros sensores

## Out of Scope

| Feature | Reason |
|---------|--------|
| GSM/LTE | Decisão explícita do usuário; conectividade só WiFi/ESP-NOW |
| WireGuard/VPN | Sem GSM não há rede não confiável; MQTT direto basta |
| Mapa/web UI no próprio node | HA é o único painel; captive portal só para provisionamento |
| IMU/baro em alta taxa (padrão vario) | Caso de uso é veículo, não voo; só o padrão de logging foi emprestado |
| Frota / provisionamento em escala | Milestone é 1 protótipo; esquema node_id/tópicos já comporta N depois |
| Geofencing / alarmes de furto | Pertence a automações HA sobre o device_tracker, não a firmware |
| Scan agressivo de WiFi em movimento | Queima bateria procurando rede ausente; scan throttled em movimento, agressivo parado |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| GPS-01 | Phase 1 | Pending |
| GPS-02 | Phase 1 | Pending |
| GPS-03 | Phase 1 | Pending |
| LOG-01 | Phase 2 | Pending |
| LOG-02 | Phase 2 | Pending |
| LOG-03 | Phase 2 | Pending |
| SYNC-01 | Phase 3 | Pending |
| SYNC-02 | Phase 3 | Pending |
| SYNC-03 | Phase 3 | Pending |
| SRV-01 | Phase 4 | Pending |
| SRV-02 | Phase 4 | Pending |
| SRV-03 | Phase 4 | Pending |
| PWR-01 | Phase 5 | Pending |
| PWR-02 | Phase 5 | Pending |
| HW-01 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 15 total
- Mapped to phases: 15
- Unmapped: 0 ✓

---
*Requirements defined: 2026-08-01*
*Last updated: 2026-08-01 after roadmap creation*
