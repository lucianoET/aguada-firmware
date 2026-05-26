# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project: Aguada — Sistema de Telemetria de Reservatórios v3.2

Reservoir water-level telemetry system for CMASM (Ilha do Engenho). Uses HC-SR04 ultrasonic sensors on ESP32-C3 nodes communicating via ESP-NOW mesh to an ESP32-S3 gateway, which forwards JSON over USB serial to a Linux server running `bridge.py`, which publishes to MQTT / Home Assistant.

**The authoritative specification is [`AGUADA_SYSTEM_DOC.md`](AGUADA_SYSTEM_DOC.md).** Always consult it before modifying protocol, config structure, or calculations.

---

## Architecture

```
[ESP32-C3 Node]  ──ESP-NOW ch1──►  [ESP32-S3 Gateway]  ──USB Serial──►  [bridge.py]  ──MQTT──►  Home Assistant
  single firmware                    JSON output                          reservoir math
  NVS config                         JSON command input                   HA Discovery
  num_sensors=0 → relay mode
```

### Key design decisions (from spec)
- **Single firmware** for all nodes — `num_sensors=0` triggers relay mode, no sensor init
- **Node ID** = 2 last bytes of MAC (e.g. `0x7758`) — no registration needed
- **Nodes transmit only `distance_cm`** — all reservoir math (level, %, volume) done in `bridge.py`
- **Reservoir parameters** (`level_max_cm`, `volume_max_L`, `sensor_offset_cm`) live in `bridge.py/reservoirs.yaml`, never on nodes
- **Protocol v3**: 16-byte packed binary, CRC-16/CCITT, version byte `0x03`

---

## Repository Structure

```
firmware/
  node/           → ESP32-C3 node firmware (main target: node_v3 spec)
  gateway/        → ESP32-S3 gateway firmware
  common/include/ → shared protocol header (to be replaced by v3 spec)

tools/
  mqtt_bridge.py  → old skeleton bridge (to be replaced)

framework/        → experimental framework skeleton (reference only)
docs_reference-old/ → old docs and prototype code (reference only)
```

The `firmware/node/src/node_v6.ino`, `firmware/gateway/src/gateway_v*.ino`, and `tools/mqtt_bridge.py` are **old skeletons** — they do not implement protocol v3. The actual implementation must follow the spec in `AGUADA_SYSTEM_DOC.md`.

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

---

## Node Firmware (ESP32-C3)

### Build
Uses PlatformIO (Arduino framework on ESP-IDF). Target board: `esp32-c3-devkitm-1` or equivalent SuperMini.

```bash
cd firmware/node
pio run                    # build
pio run -t upload          # flash
pio device monitor         # serial monitor 115200
```

### NVS Configuration
Node reads all runtime config from NVS at boot. See `node_config_t` struct in spec §5. Key fields:
- `num_sensors`: 0=relay, 1 or 2=sensor mode
- `trig_pin`/`echo_pin` per sensor
- `interval_measure_s` (default 30), `interval_send_s` (default 120), `heartbeat_s` (default 60)
- `filter_window=5`, `filter_outlier_cm=10`, `filter_threshold_cm=2`

### 3-layer filter (§6)
1. Outlier reject: discard if `raw==0`, `raw>MAX_RANGE`, or `|raw - moving_avg| > 10cm`
2. Moving average: window=5 samples
3. Send threshold: send if `|avg - last_sent| >= 2cm` OR timeout elapsed

---

## Gateway Firmware (ESP32-S3)

Receives ESP-NOW → outputs JSON lines on USB Serial 115200bps → receives JSON commands from USB.

```bash
cd firmware/gateway
pio run -t upload
```

JSON output format and command format defined in spec §8.

---

## bridge.py (Server)

Reads JSON from USB serial, calculates level/volume, publishes MQTT, handles HA Discovery.

```bash
cd tools
pip install pyserial paho-mqtt pyyaml
python bridge.py --port /dev/ttyACM0 --mqtt localhost
```

Reservoir parameters in `tools/reservoirs.yaml` (keyed by `node_id` hex string).

Formula: `level_cm = level_max_cm - (distance_cm - sensor_offset_cm)`, clamped to `[0, level_max_cm]`.

MQTT topics:
- `aguada/{node_id}/{sensor_id}/state` → JSON payload with alias, distance_cm, level_cm, pct, volume_L, rssi, vbat, seq, ts
- `aguada/{node_id}/status` → "online" / "offline"
- Subscribed: `aguada/cmd/config`, `aguada/cmd/restart`, `aguada/cmd/ota`

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

---

## Roadmap Priority (§16)

Critical (must ship first): firmware single binary (sensor+relay+NVS), protocol v3 + CRC-16, filters, gateway USB+JSON, bridge.py+MQTT+HA Discovery.

Important: CMD_CONFIG via ESP-NOW, mesh relay + neighbor table.

Desirable: OTA via ESP-NOW, deep sleep, water balance dashboard.
