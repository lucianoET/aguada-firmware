# Aguada — Node GPS Tracker (sem GSM)

## What This Is

Novo tipo de node para o sistema Aguada: um GPS tracker para veículo/embarcação em ESP32 + u-blox NEO-6M/M8N, **sem GSM**. Grava o trajeto completo em flash e entrega os dados por três canais oportunistas: hotspot WiFi do celular (live, via internet → MQTT), store & forward (despeja histórico ao entrar no alcance de WiFi conhecido) e ESP-NOW quando perto de um node/gateway Aguada. Visualização no Home Assistant (mapa via `device_tracker`) + export GPX do trajeto.

## Core Value

Nenhum ponto do trajeto se perde: o tracker registra continuamente offline e sincroniza tudo sozinho assim que qualquer canal de conectividade aparece.

## Requirements

### Validated

<!-- Sistema Aguada existente (brownfield) — fora do escopo deste milestone, mas é a base. -->

- ✓ Node firmware sensor/relay/NVS + protocolo v3 (16 bytes, CRC-16/CCITT) — existing
- ✓ Gateway ESP-NOW → USB serial JSON (validado em hardware) — existing
- ✓ `tools/bridge.py` v3.2 → MQTT + HA Discovery (validado em hardware) — existing
- ✓ Node AirQ (ENS160/AHT21/TEMT6000) com captive portal — existing

### Active

- [ ] Leitura GPS NEO-6M/M8N via UART (NMEA/UBX), fix, lat/lon/velocidade/altitude/curso
- [ ] Log de trajeto em flash com cadência adaptativa (~5–10 s em movimento, pausa quando parado)
- [ ] Canal 1 — hotspot do celular: conecta em SSID conhecido, publica live via MQTT pela internet
- [ ] Canal 2 — store & forward: ao entrar em WiFi conhecido, despeja histórico completo em lote
- [ ] Canal 3 — ESP-NOW: posição atual via mesh Aguada quando ao alcance de node/gateway
- [ ] Integração HA: `device_tracker` no mapa (lat/lon/velocidade/altitude) via MQTT Discovery
- [ ] Export GPX do trajeto (conversão do log para análise externa / Google Earth)
- [ ] Alimentação híbrida: 12V do veículo + bateria Li-ion de backup, com deep sleep / gestão de energia
- [ ] Extensão de protocolo para posição via ESP-NOW (pacote v3 de 16 bytes não comporta lat/lon)

### Out of Scope

- GSM/LTE/celular — decisão explícita do usuário; conectividade só WiFi/ESP-NOW
- Frota (múltiplos trackers) — esta fase é 1 protótipo; provisionamento em escala fica para depois
- IMU/baro/vario — referência (ESP32_IMU_BARO_GPS_VARIO) usada só pelo padrão de logging/GPX, não pelos sensores
- VPN/WireGuard (padrão Car-Assistant) — sem GSM não há necessidade; MQTT direto basta
- Página de mapa própria no node — visualização fica no HA; captive portal só se sobrar tempo

## Context

- Brownfield: sistema Aguada v3.2 (CMASM, Ilha do Engenho) já operacional — nodes ESP32-C3, gateway ESP-NOW→USB, `bridge.py`→MQTT/HA. Spec em `AGUADA_SYSTEM_DOC.md`; status em `ROADMAP.md`.
- Referências do usuário:
  - **Car-Assistant** (ShonP40) — padrão de integração HA: device_tracker com lat/lon/speed/alt, monitoramento de bateria/carga. Base GSM+WireGuard é justamente o que se quer substituir.
  - **ESP32_IMU_BARO_GPS_VARIO** (har-in-air) — padrão de logging: waypoints em flash SPI com intervalo configurável, ativação automática com fix + deslocamento mínimo, download via AP WiFi, conversão GPX, OTA WiFi.
- Hardware em mãos: módulo u-blox NEO-6M/M8N (UART).
- Trajeto completo exigido (não só posição atual) → buffer em flash é requisito central, não otimização.
- Store & forward via WiFi vai direto em MQTT/HTTP JSON — não passa pelo pacote ESP-NOW de 16 bytes.

## Constraints

- **Conectividade**: sem GSM — só WiFi (hotspot celular / rede conhecida) e ESP-NOW ao alcance
- **Tech stack**: PlatformIO (Arduino/ESP-IDF), consistente com `firmware/node`; ESP32-C3 ou ESP32 clássico
- **Compatibilidade**: não quebrar protocolo v3 nem os nodes/gateway existentes; extensão de pacote deve conviver com os 16 bytes atuais
- **Hardware**: NEO-6M/M8N já disponível; alimentação híbrida 12V + Li-ion backup
- **Energia**: com veículo desligado, tracker vive da bateria — deep sleep obrigatório no design

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Sem GSM; 3 canais oportunistas (hotspot, store&forward, ESP-NOW) | Custo/simplicidade; celular já presente no veículo; ilha tem cobertura de nodes Aguada | — Pending |
| Trajeto completo em flash, não só posição atual | Usuário quer histórico + export GPX | — Pending |
| Cadência adaptativa (~5–10 s em movimento, para quando parado) | Economiza flash e bateria sem perder o trajeto | — Pending |
| Visualização: HA (device_tracker) + export GPX | HA já é o frontend do sistema; GPX para análise externa | — Pending |
| Alimentação híbrida 12V + Li-ion backup | Sobrevive com ignição desligada | — Pending |
| 1 protótipo nesta fase | Validar conceito antes de frota | — Pending |
| Posição via ESP-NOW exige novo packet type (v3 não comporta lat/lon) | 16 bytes atuais não têm espaço; a definir na fase de planning | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-08-01 after initialization*
