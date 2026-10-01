---
phase: quick-260930-umi
plan: 01
subsystem: bridge
tags: [bridge, dedup, seq, hello, reboot]
requires: []
provides:
  - "TTL-aware seq dedup in Bridge._handle_sensor"
  - "Per-node _seen_seq purge on HELLO in Bridge._handle_hello"
affects: [tools/bridge.py]
tech-stack:
  added: []
  patterns: ["plain-assert self-check scripts under tools/"]
key-files:
  created: [tools/test_seq_dedup.py]
  modified: [tools/bridge.py]
decisions:
  - "Dedup lookup honors _seen_seq_ttl (120 s); expired entry is overwritten by the existing assignment"
  - "HELLO clears that node's dedup keys using upper-case + NODE_ALIASES, same canonical id as the dict keys; local dedup_id, node_id untouched"
metrics:
  tasks: 2
  files: 2
  completed: 2026-09-30
status: complete
---

# Quick 260930-umi: bridge.py seq dedup honors TTL and resets on HELLO

Rebooted nodes (seq restarts at 1) no longer get their SENSOR packets dropped before `tracker.seen()`: the dedup lookup now ignores entries older than `_seen_seq_ttl`, and a HELLO from a node drops that node's dedup entries.

## Commits

- ed19df9 test(quick-260930-umi): add failing seq dedup TTL/HELLO check (RED; cases a and d pass, case b fails with "expired entry must be accepted")
- 219920c fix(quick-260930-umi): seq dedup honors TTL and resets on node HELLO (GREEN)

## Changes

- `tools/bridge.py` (+7/-2): `_handle_sensor` uses `self._seen_seq.get(seq_key)` and drops only if `now - ts < _seen_seq_ttl`; `_handle_hello` rebinds `_seen_seq` without keys whose `k[0] == NODE_ALIASES.get(node_id, node_id)`. The >512 purge, `_handle_heartbeat` and the rest of `_handle_hello` are unchanged.
- `tools/test_seq_dedup.py` (new): 5 tests (within-TTL drop, expired accepted, HELLO reset, HELLO scoped to node, HELLO via alias).

## Verification

`.venv/bin/python tools/test_seq_dedup.py`, `tools/test_sensor_availability.py` and `tools/test_bridge_dispatch.py` all print `ok`. Runtime/hardware verification (restart aguada-bridge, power-cycle CON 0x7758) is left to the orchestrator (D-07).

## Deviations from Plan

The plan's automated check `git diff --numstat 4c1b48c -- tools/bridge.py` could not run as written in the platformio repo (`4c1b48c` is not a revision there); the bridge.py diff was measured against HEAD~2 instead (9 changed lines, under the 20-line limit). No code deviations.

## Known Stubs

None.

## Threat Flags

None.

## Self-Check: PASSED

- tools/test_seq_dedup.py found; commits ed19df9 and 219920c found; other-session files left unstaged.
