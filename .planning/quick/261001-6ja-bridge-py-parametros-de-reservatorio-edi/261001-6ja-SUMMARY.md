---
phase: quick-261001-6ja
plan: 01
subsystem: bridge
tags: [bridge, mqtt, home-assistant, yaml, hc-sr04]
requires: []
provides:
  - HA number entities (altura util, offset, volume maximo) per HC-SR04 reservoir
  - retained aguada/{node}/{sid}/config state + set handler persisting to reservoirs.yaml
  - out-of-range HC-SR04 reading treated as sensor error
affects: [tools/bridge.py, tools/test_reservoir_config.py]
tech-stack:
  added: []
  patterns: [textual item-scoped YAML edit with atomic os.replace]
key-files:
  created: [tools/test_reservoir_config.py]
  modified: [tools/bridge.py]
decisions:
  - "Config-set routing placed before json.loads so non-JSON payloads still republish state"
  - "Range check written as not (lo <= v <= hi) to reject NaN"
metrics:
  tasks: 3
  files: 2
completed: 2026-10-01
status: complete
---

# Phase quick-261001-6ja Plan 01: Reservoir params editable from HA Summary

Reservoir parameters (level_max_cm, sensor_offset_cm, volume_max_L) are editable from Home Assistant via MQTT number entities, applied to the next reading, persisted to reservoirs.yaml by a one-line textual edit (comments kept), and readings beyond level_max + offset + 30 cm now mark the sensor offline instead of producing a false 0 %.

## Commits

- 9949e37 out-of-range reading = sensor error (OUT_OF_RANGE_MARGIN_CM = 30) + test scaffold
- 3477d7b update_reservoir_yaml(): item-scoped, atomic, mode-preserving
- 745e928 number discovery, retained config state, `_handle_config_set`

## MQTT surface

- Subscribe: `aguada/+/+/config/+/set`
- State (retained): `aguada/{node}/{sid}/config` = `{"level_max_cm", "sensor_offset_cm", "volume_max_L"}`
- Discovery (retained): `homeassistant/number/aguada_{alias}_cfg_{param}/config`
- default_entity_id: `number.aguada_{alias}_altura_util`, `number.aguada_{alias}_offset_sensor`, `number.aguada_{alias}_volume_maximo`
- Command topics: `aguada/{node}/{sid}/config/{level_max_cm|sensor_offset_cm|volume_max_L}/set`

## Deviations from Plan

None - plan executed exactly as written.

## Known Stubs

None.

## Self-Check: PASSED

All four self-checks print `ok`; real tools/reservoirs.yaml unchanged; commits 9949e37, 3477d7b, 745e928 exist.
