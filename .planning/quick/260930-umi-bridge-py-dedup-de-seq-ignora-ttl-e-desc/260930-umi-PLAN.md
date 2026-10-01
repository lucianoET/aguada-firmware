---
phase: quick-260930-umi
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - tools/test_seq_dedup.py
  - tools/bridge.py
autonomous: true
requirements: [QUICK-UMI]
user_setup: []

must_haves:
  truths:
    - "The same (node_id, sensor_id, seq) arriving twice within 120 s publishes state only once: the relay + direct duplicate stays blocked (D-01, D-04 case a)"
    - "A SENSOR whose (node_id, sensor_id, seq) entry is older than `_seen_seq_ttl` is accepted, reaches `tracker.seen()` and publishes state, and the entry timestamp is refreshed to now (D-01, D-04 case b)"
    - "After a HELLO from a node, that node's SENSOR packets with seq 1, 2, 3... are accepted again even inside 120 s of the previous boot, so a rebooted CON (0x7758) stops dropping to offline after 300 s (D-02, D-04 case c)"
    - "A HELLO from node A leaves node B's dedup entries untouched (D-02, D-04 case d)"
    - "HELLO purge uses the same canonical id as the dict keys: upper-case + NODE_ALIASES, so a HELLO from 0xee02 clears 0XC9C4 entries (D-02)"
    - "The over-512 purge, `_handle_heartbeat` and the rest of `_handle_hello` behave exactly as before (D-03)"
    - "`tools/test_seq_dedup.py`, `tools/test_sensor_availability.py` and `tools/test_bridge_dispatch.py` each print `ok` with `.venv/bin/python` (D-04, D-05)"
  artifacts:
    - path: tools/test_seq_dedup.py
      provides: "assert-based self-check for TTL-aware dedup and HELLO reset, reusing make_bridge/sensor_msg from test_sensor_availability"
    - path: tools/bridge.py
      provides: "TTL-aware dedup lookup in Bridge._handle_sensor; per-node `_seen_seq` purge in Bridge._handle_hello"
  key_links:
    - "Dedup keys are built in `_handle_sensor` as (NODE_ALIASES-canonical upper-case node_id, int sensor_id, seq). The HELLO purge compares `k[0]` against `NODE_ALIASES.get(msg['node_id'].upper(), ...)`. If the two normalizations diverge, HELLO clears nothing and the field bug stays"
    - "Dedup `return` happens BEFORE `self.tracker.seen(node_id)`: any wrongly dropped SENSOR also starves the 300 s online tracker, which is the field symptom"
    - "Expired entry must fall through to the existing `self._seen_seq[seq_key] = now` assignment so the timestamp is refreshed (otherwise case b's second arrival within the new window would not be blocked)"
---

<objective>
Fix the seq dedup in `tools/bridge.py` so a node that reboots is not silently dropped.

Purpose: node firmware resets `g_seq` on every boot (HELLO = seq 0, SENSOR = 1, 2, 3...). `_handle_sensor` drops any seq_key already in `self._seen_seq` with no age check (the TTL is only applied by the >512 purge), so after a reboot the node's SENSOR packets collide with the previous boot's entries, return before `tracker.seen()`, and the node falls to "offline (no data for 300s)". HELLO bypasses dedup, so the node shows online for 5 min and then "dies". Confirmed on hardware 2026-09-30 for CON (0x7758): seq 2 accepted at 21:24:09, same seq 2 at 22:02:46 republished on `aguada/gateway/rx` but never logged by the bridge.

Output: `tools/test_seq_dedup.py` (new, RED first) and a minimal `tools/bridge.py` diff (TTL check on lookup + per-node purge on HELLO).

Decision mapping (orchestrator decisions 1-7 -> D-01..D-07): D-01 TTL honored on lookup, expired entry overwritten, >512 purge unchanged; D-02 HELLO of any node clears that node's keys using upper + NODE_ALIASES (button HELLO 0x40 included, no special case); D-03 minimal diff in file style, `_handle_heartbeat` untouched; D-04 executable check with cases a-d; D-05 `.venv/bin/python` only; D-06 other-session working-tree files untouched, stage by explicit path; D-07 runtime verification on hardware is done by the orchestrator, not the executor.
</objective>

<execution_context>
@$HOME/.claude/gsd-core/workflows/execute-plan.md
@$HOME/.claude/gsd-core/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@CLAUDE.md
@tools/test_sensor_availability.py

Facts verified during planning (do not re-derive):
- Work from `/home/luc/DEV_luctronics/luctronics_firmware/platformio`. Interpreter: `.venv/bin/python` (paho-mqtt). System python3 has no paho.
- `tools/test_sensor_availability.py` and `tools/test_bridge_dispatch.py` both print `ok` today. Importing `test_sensor_availability` is side-effect free (entry guarded by `if __name__ == "__main__"`) and exposes `make_bridge()` -> `(bridge, fake)` and `sensor_msg(node, sid, dist, flags, seq)`. `fake.published` is a list of `(topic, payload, retain)`.
- `make_bridge()` replaces `b.client` and `b.tracker` with the fake; `_handle_hello` with `num_sensors=1` calls `publish_discovery` on the fake client, which works (already exercised by test_sensor_availability).
- A valid reading publishes on `aguada/{CANON}/{sid}/state`. Valid inputs already proven: `("0x7758", 1, 102, 0, seq)` -> `aguada/0X7758/1/state`; `("0x2EC4", 1, 80, 0, seq)` -> `aguada/0X2EC4/1/state`. CAV `0XC9C4` sensor 1 has the same parameters as CON (level_max 450, offset 20) and alias `also: ["0XEE02"]`, so `("0xEE02", 1, 102, 0, seq)` -> `aguada/0XC9C4/1/state`.
- bridge.py anchors: `NODE_ALIASES` dict L47; `Bridge.__init__` `_seen_seq` / `_seen_seq_ttl` L760-763; `_handle_sensor` dedup block L1019-1029 (`seq_key = (node_id, sensor_id, msg.get("seq", -1))`, membership check, assignment, >512 purge), `self.tracker.seen(node_id)` L1031; `_handle_hello` L1192, `node_id = msg["node_id"].upper()` L1193 (no alias), `log.info("HELLO %s ...")` L1198-1199.
- Handlers called via `dispatch()` run under `self._dispatch_lock`, same as the existing purge; no new locking needed.
- Working tree has unrelated modified/untracked files from another session (firmware/gps_tracker/*, tools/install_autostart_user_service.sh, tools/systemd/, .planning/debug/). Never stage them.
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: RED - add tools/test_seq_dedup.py covering TTL expiry and HELLO reset</name>
  <files>tools/test_seq_dedup.py</files>
  <behavior>
    - test_duplicate_within_ttl_dropped (case a): CON seq 1 valid twice -> exactly 1 publish on aguada/0X7758/1/state; key ("0X7758", 1, 1) in b._seen_seq
    - test_expired_entry_accepted (case b): CON seq 1 -> 1 state publish; subtract (b._seen_seq_ttl + 1) from b._seen_seq[("0X7758", 1, 1)]; CON seq 1 again -> 2 state publishes, and the entry timestamp is within 5 s of time.time()
    - test_hello_resets_node_seq (case c): CON seq 1 -> 1; HELLO 0x7758 num_sensors=1 -> no key with k[0] == "0X7758" left in b._seen_seq; CON seq 1 again -> 2
    - test_hello_scoped_to_node (case d): CON seq 1 and 0x2EC4 sid 1 seq 1 -> 1 publish each; HELLO 0x7758; 0x2EC4 sid 1 seq 1 again -> still 1 publish on aguada/0X2EC4/1/state and ("0X2EC4", 1, 1) still in b._seen_seq
    - test_hello_alias_clears_canonical (D-02 normalization): SENSOR "0xEE02" sid 1 seq 5 -> 1 publish on aguada/0XC9C4/1/state; HELLO with node_id "0xee02" (lower case) num_sensors=1; same SENSOR again -> 2
    - On the current bridge.py: cases a and d pass, cases b, c and alias fail with AssertionError
  </behavior>
  <action>
    Create `tools/test_seq_dedup.py` in the same plain-assert style as `tools/test_sensor_availability.py` (no pytest), per D-04. New file instead of extending the availability test: keeps that test focused on availability, dedup is a separate concern.

    Structure:
    - Module docstring: "Self-check: seq dedup in _handle_sensor honors the TTL and is reset by HELLO (node reboot restarts seq). Run: .venv/bin/python tools/test_seq_dedup.py"
    - Imports: `sys`, `time`, `pathlib.Path`; `sys.path.insert(0, str(Path(__file__).parent))`; then `from test_sensor_availability import make_bridge, sensor_msg` (reuse, do not copy FakeClient/make_bridge).
    - Helper `hello_msg(node, num)` returning a dict with keys v=3, type="HELLO", node_id=node, num_sensors=num, flags=0, vbat=-1, seq=0, fw_version="test", ts=1.
    - Helper `n_pub(fake, topic)` returning the count of entries in `fake.published` whose topic equals `topic`.
    - Constants for the three state topics: `aguada/0X7758/1/state`, `aguada/0X2EC4/1/state`, `aguada/0XC9C4/1/state`.
    - The five test functions listed in `<behavior>`, each building its own bridge via `make_bridge()` and feeding messages through `b._handle_sensor(...)` / `b._handle_hello(...)`. Use valid readings only (CON and CAV 102 cm flags 0, 0x2EC4 80 cm flags 0) so "accepted" is visible as a state publish.
    - Give every assertion in cases b, c and alias a short message string; the case b assertion on the second publish count must read exactly "expired entry must be accepted" (the RED gate greps for it).
    - `if __name__ == "__main__":` call the tests in order a, d, b, c, alias, then `print("ok")`.

    Run it against the unmodified bridge.py and confirm: cases a and d pass on their own, and the full script stops with the case b AssertionError (not an ImportError or KeyError). Do not touch bridge.py in this task.

    Commit (D-06): `git add tools/test_seq_dedup.py` only (explicit path, never `-A`, `.` or `-u`), message `test(quick-260930-umi): add failing seq dedup TTL/HELLO check`.
  </action>
  <verify>
    <automated>cd /home/luc/DEV_luctronics/luctronics_firmware/platformio && .venv/bin/python -c "import sys; sys.path.insert(0, 'tools'); import test_seq_dedup as t; t.test_duplicate_within_ttl_dropped(); t.test_hello_scoped_to_node(); print('guards ok')" 2>&1 | grep -qx 'guards ok' && .venv/bin/python tools/test_seq_dedup.py 2>&1 | grep -q 'AssertionError: expired entry must be accepted' && echo RED-OK</automated>
  </verify>
  <done>`tools/test_seq_dedup.py` exists with the five tests; guards a and d pass; full run fails on the case b assertion against current bridge.py; committed alone.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: GREEN - TTL-aware dedup lookup and per-node purge on HELLO in bridge.py</name>
  <files>tools/bridge.py</files>
  <behavior>
    - All five tests in tools/test_seq_dedup.py pass and the script prints ok
    - tools/test_sensor_availability.py and tools/test_bridge_dispatch.py still print ok
  </behavior>
  <action>
    Minimal diff in the file's existing style: short comments, English log messages (D-03).

    1. `Bridge._handle_sensor` dedup block (per D-01): replace the plain membership test on `self._seen_seq` with a lookup of the stored timestamp via `self._seen_seq.get(seq_key)`; treat the packet as a duplicate only when that timestamp is not None AND `now - ts < self._seen_seq_ttl`. Keep the existing `log.debug("Dedup: skip duplicate ...")` line and `return` unchanged inside that branch. An expired entry falls through to the existing `self._seen_seq[seq_key] = now` line, which overwrites it. Leave the `> 512` purge block exactly as is. Update the comment above `seq_key` to one short line, e.g. "Deduplicate within TTL: same packet may arrive via relay AND direct path".

    2. `Bridge._handle_hello` (per D-02): right after the existing `log.info("HELLO %s num_sensors=...")` call and before `if num == 0:`, add a one-line comment (e.g. "Node booted: seq restarted, forget its old dedup entries") and rebind `self._seen_seq` to a dict comprehension that drops every key whose first element equals `NODE_ALIASES.get(node_id, node_id)`. Compute that canonical id in a local (e.g. `dedup_id`) used only for this purge; do NOT reassign `node_id`, so tracker.seen, relay topics and discovery in the rest of `_handle_hello` stay byte-identical. Apply to every HELLO (sensor, relay, and button flag 0x40) with no special case, per D-02.

    3. Optionally adjust the `_seen_seq` comment in `__init__` (L760-761) by at most one short line noting entries expire after the TTL and are reset by HELLO. No other edits: `_handle_heartbeat`, `NODE_ALIASES`, the purge, and `_dispatch` stay untouched (D-03).

    Run the three self-checks with `.venv/bin/python` (D-05). Do NOT restart `aguada-bridge` or touch hardware: runtime verification belongs to the orchestrator (D-07).

    Commit (D-06): `git add tools/bridge.py` only (explicit path), message `fix(quick-260930-umi): seq dedup honors TTL and resets on node HELLO`. Confirm with `git status --short` that the other-session files are still unstaged.
  </action>
  <verify>
    <automated>cd /home/luc/DEV_luctronics/luctronics_firmware/platformio && .venv/bin/python -m py_compile tools/bridge.py && .venv/bin/python tools/test_seq_dedup.py 2>&1 | tail -1 | grep -qx ok && .venv/bin/python tools/test_sensor_availability.py 2>&1 | tail -1 | grep -qx ok && .venv/bin/python tools/test_bridge_dispatch.py 2>&1 | tail -1 | grep -qx ok && test "$(git diff --numstat 4c1b48c -- tools/bridge.py | awk '{print $1+$2}')" -le 20 && echo GREEN-OK</automated>
  </verify>
  <done>Three self-checks print ok; bridge.py diff is at most 20 changed lines confined to the dedup block, `_handle_hello` purge and optionally the `__init__` comment; committed alone; other-session files untouched.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| gateway serial / `aguada/gateway/rx` MQTT mirror -> `Bridge.dispatch` | JSON lines from ESP-NOW nodes via gateway; node_id, seq and type are untrusted |

## STRIDE Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation Plan |
|-----------|----------|-----------|----------|-------------|-----------------|
| T-umi-01 | Spoofing | `_handle_hello` purge | low | accept | A forged HELLO only clears that node's dedup entries, allowing at most one re-publish of an already-seen reading; state topics are retained and idempotent, so the effect is a duplicate log/Influx point, no wrong data |
| T-umi-02 | Denial of Service | `_seen_seq` growth | low | mitigate | Existing `> 512` purge with `_seen_seq_ttl` cutoff stays unchanged; HELLO purge only shrinks the dict |
| T-umi-03 | Tampering | replayed old SENSOR line after TTL | low | accept | Already possible before this change once the >512 purge ran; the line-level `_recent_lines` dedup in `_dispatch` still blocks exact recent replays; no auth exists on the ESP-NOW link by design |
</threat_model>

<verification>
Executor (automated, `.venv/bin/python`):
- `tools/test_seq_dedup.py` prints `ok` (cases a, b, c, d + alias)
- `tools/test_sensor_availability.py` and `tools/test_bridge_dispatch.py` print `ok`
- `git log -2 --stat` shows two commits, each touching only its own file

Orchestrator, post-execution (D-07, not the executor):
- `systemctl --user restart aguada-bridge`
- Power-cycle CON (0x7758); in `journalctl --user -u aguada-bridge -f` expect `HELLO 0X7758`, then `Sensor error: 0X7758/1` every ~120 s after each boot, and no `Node 0X7758 -> offline` after 300 s
</verification>

<success_criteria>
- A rebooted node's SENSOR packets (seq restarting at 1) are processed immediately after its HELLO
- Without a HELLO, a seq entry older than 120 s no longer blocks a packet
- Relay + direct duplicates inside 120 s are still dropped
- Diff limited to `tools/bridge.py` (small) and new `tools/test_seq_dedup.py`
</success_criteria>

<output>
Create `.planning/quick/260930-umi-bridge-py-dedup-de-seq-ignora-ttl-e-desc/260930-umi-SUMMARY.md` when done
</output>
