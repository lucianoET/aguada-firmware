# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project: Aguada — Sistema de Telemetria de Reservatórios v3.2

Reservoir water-level and air-quality telemetry for CMASM (Ilha do Engenho). ESP32-C3 nodes (HC-SR04 ultrasonic, ANI-01 air quality, or pure relay) talk ESP-NOW to an ESP32 gateway, which forwards JSON over USB serial to `tools/bridge.py`, which does all reservoir math and publishes to MQTT / Home Assistant (Discovery).

**Authoritative spec: [`AGUADA_SYSTEM_DOC.md`](AGUADA_SYSTEM_DOC.md)** — consult before touching protocol, config structure, or calculations.
**Status interface: [`ROADMAP.md`](ROADMAP.md)** — phase status and next actions; update it when starting/finishing phases. AirQ node pendências live in [`todo.md`](todo.md).

**Active milestone: GPS tracker node** (`firmware/gps_tracker/`) — planned via GSD in `.planning/` (PROJECT.md, REQUIREMENTS.md, ROADMAP.md, phases/). That roadmap is separate from the repo-root ROADMAP.md.

---

## Architecture

```
Node(s) ESP32-C3  ──ESP-NOW ch1──►  Gateway ESP32/S3/C3  ──USB serial──►  bridge.py  ──MQTT──►  Home Assistant
    ANI-01 AP  ──WiFi──►  browser  (captive portal, no broker needed)
```

### Key design decisions (from spec)

- **Single node firmware** — `num_sensors=0` triggers relay mode; `DEFAULT_SENSOR_TYPE=1` build flag selects ANI-01 air-quality mode
- **Node ID** = last 2 MAC bytes (e.g. `0x7758`) — no registration
- **Nodes transmit only raw readings** (`distance_cm`) — level/%/volume math lives in `bridge.py` + `tools/reservoirs.yaml`, never on nodes
- **Protocol v3**: 16-byte packed binary, CRC-16/CCITT, version byte `0x03` — canonical struct in `firmware/shared/protocol.h` (nodes/gateway keep local copies of `crc16.h`)

Formula: `level_cm = level_max_cm - (distance_cm - sensor_offset_cm)`, clamped to `[0, level_max_cm]`.

---

## Implementation Status

Phases 1–7 are implemented (see ROADMAP.md for detail): node firmware (sensor/relay/NVS), protocol v3 + CRC, 3-layer filter, gateway USB+JSON (✅ hardware-validated), bridge.py+MQTT+HA Discovery (✅ hardware-validated), CMD_CONFIG (partial), mesh relay (written, untested). OTA and deep sleep not started. Do NOT treat firmware sources as skeletons — they are the real implementation.

---

## Repository Structure

```
firmware/
  node/           → ESP32-C3/ESP32 node firmware (modular: main.cpp, espnow_radio, mesh,
                    nvs_config, sensor_filter, ultrasonic, ani_sensor, light_sensor,
                    sound_sensor, rgb_led). Pinout: firmware/node/node_pinout.md
  gateway/        → gateway firmware (C3 / S3 / classic ESP32; USB serial + WiFi variants)
  gps_tracker/    → GPS tracker node (ESP32/C3 + NEO-6M or ATGM336H, TinyGPSPlus; no GSM).
                    Phase 1: gps_reader → fix_gate → cadence, bench simulator GPS_BENCH_SIM.
                    Docs: firmware/gps_tracker/BENCH.md, gps_tracker_pinout.md
  rfid_test/      → standalone ID-12 RFID → captive portal test sketch (no ESP-NOW/NVS)
  shared/         → protocol.h (canonical v3 packet struct)
  .old/           → retired variants (e.g. gateway-ethernet ENC28J60)

tools/
  bridge.py       → serial→MQTT bridge v3.2 (reservoir math, HA Discovery, optional InfluxDB)
  reservoirs.yaml → per-reservoir parameters keyed by node_id hex
  nvs_tool.py, flash_*.sh, start_bridge*.sh, systemd/, udev/, mosquitto/

main/ + CMakeLists.txt → minimal ESP-IDF stub so the VS Code ESP-IDF extension works here;
                         real builds use PlatformIO under firmware/. Don't develop in it.
docs/superpowers/ → design specs (e.g. RFID captive test)
```

---

## Build & Flash (PlatformIO)

`pio` is NOT on PATH on this machine — use the venv binary `/home/luc/Dev/aguada-firmware-main/.venv/bin/pio` (or `.venv/bin/python -m platformio`). Commands below assume `pio` resolves to that.

```bash
# Node — ultrasonic (ESP32-C3 SuperMini)
cd firmware/node && pio run -e esp32-c3-supermini -t upload --upload-port /dev/ttyACM0

# Node — ANI-01 air quality (sets -DDEFAULT_SENSOR_TYPE=1)
cd firmware/node && pio run -e ani-01-c3 -t upload

# Node — classic ESP32 devkit / relay
cd firmware/node && pio run -e esp32-devkit        # or esp32-devkit-relay

# Gateway
cd firmware/gateway && pio run -e gateway-esp32-usb   # classic ESP32 (validated)
cd firmware/gateway && pio run -e gateway-s3          # ESP32-S3
cd firmware/gateway && pio run -e gateway-c3          # ESP32-C3

# GPS tracker
cd firmware/gps_tracker && pio run -e esp32-c3-supermini -t upload --upload-port /dev/ttyACM0
cd firmware/gps_tracker && pio run -e esp32-devkit    # classic ESP32 (GPS on UART2, pins 16/17)

# Monitor
pio device monitor   # 115200
```

Per-device envs exist with baked-in config (`con-ch11`, `cav-ch11`, `cb3-ch11`, `node3-cb3`, …) — `-ch11` variants use ESP-NOW channel 11 instead of 1. Check `platformio.ini` build_flags before adding a new env.

No automated test suite — validation is on-hardware (bench HC-SR04 reads, node→gateway loopback JSON, MQTT topics).

## bridge.py (server)

```bash
pip install -r tools/requirements.txt
python tools/bridge.py --port /dev/ttyACM0 --mqtt localhost
./tools/install_autostart_user_service.sh   # systemd user service, auto-starts on ttyACM*/ttyUSB*
```

MQTT topics: `aguada/{node_id}/{sensor_id}/state`, `aguada/{node_id}/status` (online/offline), `aguada/{node_id}/env/state` (AirQ temp/hum), gateway observability on `aguada/gateway/{status,health,ack}`. Commands subscribed on `aguada/cmd/{config,restart,ota}`.

---

## Protocol v3 — 16-byte Packet

```c
typedef struct __attribute__((packed)) {
    uint8_t  version;     // 0x03
    uint8_t  type;        // PacketType enum
    uint16_t node_id;     // 2 last MAC bytes
    uint8_t  sensor_id;   // 1 or 2 | 0=control/heartbeat
    uint8_t  ttl;         // decremented by relays, max 8
    uint16_t seq;
    uint16_t distance_cm; // filtered | 0xFFFF=error
    int8_t   rssi;        // filled by receiver
    int8_t   vbat;        // tenths of V (33=3.3V) | -1=n/a
    uint8_t  flags;       // bitmask (see spec §7)
    uint8_t  reserved;
    uint16_t crc;         // CRC-16/CCITT bytes 0..13
} espnow_packet_t;        // 16 bytes
```

Packet types: `0x01 SENSOR`, `0x02 HEARTBEAT`, `0x03 HELLO`, `0x10 CMD_CONFIG`, `0x11 CMD_RESTART`, `0x12 CMD_OTA_START`, `0x13 OTA_BLOCK`, `0x14 OTA_END`, `0x20 ACK`.

### Node runtime config (NVS, spec §5)

- `num_sensors`: 0=relay, 1–2=sensor mode; `trig_pin`/`echo_pin` per sensor
- `interval_measure_s=30`, `interval_send_s=120`, `heartbeat_s=60`
- 3-layer filter (§6): outlier reject (`raw==0`, `raw>MAX_RANGE`, `|raw-avg|>10cm`) → moving average window 5 → send if `|avg-last_sent|>=2cm` or timeout

---

## Node AirQ (ANI-01)

ESP32-C3 + ENS160/AHT21 (I2C GPIO6/7, **CS→3V3 mandatory** else SPI mode), optional TEMT6000 light (GPIO3/ADC1) and KY-037 sound (GPIO1/ADC1, feed 3.3 V). Serves captive portal on WiFi AP `ANI-{node_id}` → `http://192.168.4.1` (phone-first, per-hardware-component cards, sparklines). Local sampling (8 s) decoupled from ESP-NOW send (120 s). Light/sound are captive-portal-only so far — not yet in ESP-NOW/MQTT. ADC pins must be ADC1 (GPIO0–4); ADC2 dies with WiFi on.

---

## Device Inventory (CMASM)

| node_id | alias | Reservoir | sensors |
|---------|-------|-----------|---------|
| 0x7758 | CON | Castelo de Consumo | 1 |
| 0xEE02 | CAV | Castelo de Incêndio | 1 |
| 0x2EC4 | CB31/CB32 | Casa de Bombas Nº3 | 2 |
| 0x9EAC | CIE1/CIE2 | Cisterna IE | 2 |
| 0x3456 | CBIF1/CBIF2 | Casa de Bombas IF | 2 |

Gateway MAC (ESP32-S3, USB serial): `9C:13:9E:AC:E1:88`
Gateway MAC (ESP32 DevKit ETH/ECN): `24:D7:EB:5B:2E:74`
