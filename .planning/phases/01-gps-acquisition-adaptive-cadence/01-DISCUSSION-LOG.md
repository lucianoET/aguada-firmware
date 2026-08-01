# Phase 1: GPS Acquisition & Adaptive Cadence - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-08-01
**Phase:** 1-GPS Acquisition & Adaptive Cadence
**Areas discussed:** Hardware alvo, Estrutura do projeto

---

## Hardware alvo

### Placa ESP32

| Option | Description | Selected |
|--------|-------------|----------|
| ESP32-C3 SuperMini | Padrão da frota Aguada — pequeno, barato; menos pinos/ADC | |
| ESP32 devkit clássico | Mais pinos, 2 UARTs livres, mais ADC — folga para periféricos futuros | ✓ |
| Você decide | Claude escolhe no planning | |

### Módulo GPS

| Option | Description | Selected |
|--------|-------------|----------|
| NEO-6M | NMEA 9600, 1 Hz, power-save CFG-RXM | |
| NEO-M8N | Multi-GNSS, até 10 Hz | |
| Tenho os dois | Firmware deve funcionar com ambos | |

**User's choice:** free-text — "tenho o ATGM336H GPS e o neo-6m"
**Notes:** ATGM336H (AT6558, CASIC, GPS+BeiDou) + NEO-6M → firmware NMEA-agnóstico, sem UBX/CASIC obrigatório.

### Taxa/baud GPS

| Option | Description | Selected |
|--------|-------------|----------|
| Default 9600/1Hz (Recommended) | Zero config UBX, funciona nos dois módulos | ✓ |
| Reconfigurar (38400+/5-10Hz) | Desnecessário para veículo | |
| Você decide | | |

### Pinos UART

| Option | Description | Selected |
|--------|-------------|----------|
| Você decide (Recommended) | Claude escolhe pinos coerentes com convenções do repo | ✓ |
| Vou especificar | | |

**Notes:** Usuário pediu .md de pinagem para montar a bancada → criado `firmware/gps_tracker/gps_tracker_pinout.md` (GPS UART2 GPIO16/17, reservas HW-01).

---

## Estrutura do projeto

| Option | Description | Selected |
|--------|-------------|----------|
| firmware/gps_tracker/ novo (Recommended) | Projeto irmão de node/gateway, compartilha shared/protocol.h | ✓ |
| Env dentro de firmware/node | Reusa módulos direto, amarra tracker ao node | |

---

## Claude's Discretion

- Parâmetros de cadência adaptativa (limiares, janelas, heartbeat)
- Critérios de gating de fix (HDOP, sats, warmup)
- Formato de log serial de bancada; organização de src/
- Confirmação TinyGPSPlus × ATGM336H

## Deferred Ideas

- OLED/botões/buzzer, NMEA 0183 out (radar), IMU — v2 (HW-02..05)
- Power-save proprietário GPS + deep sleep — v2 (PWR-03)
- PPS do ATGM336H — não usado no v1
