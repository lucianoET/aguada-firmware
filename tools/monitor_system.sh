#!/bin/bash
# Monitor Aguada System Status
#
# Security note:
# - Do NOT hardcode secrets here.
# - If your broker requires credentials, put them in the repo-root .env (gitignored)
#   as MQTT_USER / MQTT_PASS. This script avoids passing passwords on the command line.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Optional env overrides (gitignored)
if [[ -f "$ROOT_DIR/.env" ]]; then
  set -a
  # shellcheck disable=SC1090
  . "$ROOT_DIR/.env"
  set +a
fi

# Prefer the same naming used by tools/start_bridge_autoswitch.sh (.env uses REMOTE_HOST/REMOTE_PORT)
MQTT_HOST="${MQTT_HOST:-${REMOTE_HOST:-127.0.0.1}}"
MQTT_PORT="${MQTT_PORT:-${REMOTE_PORT:-1883}}"
RESERVOIRS_YAML="${RESERVOIRS_YAML:-$ROOT_DIR/tools/reservoirs.yaml}"

export MQTT_HOST MQTT_PORT RESERVOIRS_YAML

echo "=== Aguada System Monitor ==="
echo "$(date)"
echo "MQTT: $MQTT_HOST:$MQTT_PORT"
echo ""

# Check bridge.py running
echo "1. Bridge Status:"
if pgrep -f "tools/bridge.py" >/dev/null 2>&1; then
  echo "   ✓ bridge.py running (PID $(pgrep -f tools/bridge.py | tr '\n' ' '))"
else
  echo "   ✗ bridge.py NOT running"
fi
echo ""

# Check gateway USB (best-effort)
echo "2. Gateway USB:"
if compgen -G "/dev/serial/by-id/*" >/dev/null; then
  echo "   ✓ /dev/serial/by-id present"
  ls -l /dev/serial/by-id/ 2>/dev/null | sed 's/^/   /'
elif [[ -e /dev/ttyACM0 ]]; then
  echo "   ✓ /dev/ttyACM0 present"
else
  echo "   ✗ No obvious USB serial device found"
fi
lsusb 2>/dev/null | grep -i espressif | sed 's/^/   /' || true
echo ""

echo "3. MQTT + Elements (from reservoirs.yaml):"
python3 - <<'PY'
import json
import os
import time
from collections import defaultdict

try:
    import yaml
    import paho.mqtt.client as mqtt
except Exception as e:
    print("   ✗ Python deps missing:", e)
    print("     Install: pip install -r tools/requirements.txt")
    raise SystemExit(0)

mqtt_host = os.environ.get("MQTT_HOST") or os.environ.get("REMOTE_HOST") or "127.0.0.1"
mqtt_port = int(os.environ.get("MQTT_PORT") or os.environ.get("REMOTE_PORT") or "1883")
mqtt_user = os.environ.get("MQTT_USER")
mqtt_pass = os.environ.get("MQTT_PASS")
cfg_path  = os.environ.get("RESERVOIRS_YAML") or "tools/reservoirs.yaml"

with open(cfg_path, "r", encoding="utf-8") as f:
    cfg = yaml.safe_load(f) or {}

reservoirs = []  # list of dicts: node_id, sensor_id, alias, name, params
for node_id, sensors in (cfg.get("reservoirs") or {}).items():
    for s in sensors or []:
        reservoirs.append({
            "node_id": str(node_id).upper(),
            "sensor_id": int(s.get("sensor_id")),
            "alias": str(s.get("alias") or "").strip() or f"{node_id}_{s.get('sensor_id')}",
            "name": str(s.get("name") or "").strip(),
            "level_max_cm": float(s.get("level_max_cm", 0) or 0),
            "volume_max_L": float(s.get("volume_max_L", 0) or 0),
            "sensor_offset_cm": float(s.get("sensor_offset_cm", 0) or 0),
        })

topics_wanted = set()
topics_wanted.add("aguada/gateway/status")
topics_wanted.add("aguada/gateway/health")
topics_wanted.add("homeassistant/sensor/+/config")
topics_wanted.add("homeassistant/binary_sensor/+/config")
topics_wanted.add("aguada/+/status")

for r in reservoirs:
    topics_wanted.add(f"aguada/{r['node_id']}/{r['sensor_id']}/state")
    topics_wanted.add(f"aguada/{r['node_id']}/status")

received = {}
discovery_ids = set()

def on_connect(client, userdata, flags, rc, properties=None):
    if rc != 0:
        print(f"   ✗ MQTT connect failed rc={rc}")
        return
    for t in topics_wanted:
        client.subscribe(t)

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode("utf-8", errors="replace")
    received[topic] = payload
    # Track discovery object_ids for aguada_*
    if topic.startswith("homeassistant/") and topic.endswith("/config"):
        parts = topic.split("/")
        if len(parts) >= 4:
            obj_id = parts[2]
            if obj_id.startswith("aguada_"):
                discovery_ids.add(obj_id)

client = mqtt.Client()
if mqtt_user and mqtt_pass:
    client.username_pw_set(mqtt_user, mqtt_pass)
client.on_connect = on_connect
client.on_message = on_message

try:
    client.connect(mqtt_host, mqtt_port, keepalive=10)
except Exception as e:
    print(f"   ✗ Cannot connect to MQTT {mqtt_host}:{mqtt_port} — {e}")
    raise SystemExit(0)

client.loop_start()
time.sleep(2.0)  # allow retained discovery + retained state/status to arrive
client.loop_stop()
client.disconnect()

print("   ✓ MQTT connected")

# Gateway snapshot
gw_status = received.get("aguada/gateway/status")
gw_health_raw = received.get("aguada/gateway/health")
if gw_status:
    print(f"   Gateway status: {gw_status.strip()}")
if gw_health_raw:
    try:
        gw = json.loads(gw_health_raw)
        seen = gw.get("ts_seen_iso") or gw.get("ts_iso")
        uptime = gw.get("uptime_s")
        heap = gw.get("free_heap")
        print(f"   Gateway health: last_seen={seen} uptime_s={uptime} free_heap={heap}")
    except Exception:
        print("   Gateway health: (invalid JSON)")

expected_suffixes = {"nivel","pct","volume","distancia","rssi","bateria","last_update"}
disc_by_alias = defaultdict(set)
for obj in discovery_ids:
    if obj.startswith("aguada_gateway_usb_"):
        continue
    if not obj.startswith("aguada_"):
        continue
    rest = obj[len("aguada_"):]
    parts = rest.split("_")
    if len(parts) < 2:
        continue
    alias = parts[0]
    suffix = "_".join(parts[1:])
    disc_by_alias[alias].add(suffix)

print(f"   HA Discovery: {len([d for d in discovery_ids if d.startswith('aguada_')])} configs (sensors + binary_sensors)")

# Node online/offline (retained)
node_status = {}
for topic, payload in received.items():
    if topic.startswith("aguada/") and topic.endswith("/status") and topic.count("/") == 2:
        node_id = topic.split("/")[1].upper()
        node_status[node_id] = payload.strip()

now = int(time.time())

def fmt_age(ts):
    try:
        ts = int(ts)
    except Exception:
        return "?"
    if ts <= 0:
        return "?"
    return f"{now - ts}s"

def expected_from_distance(distance_cm, r):
    try:
        d = float(distance_cm)
    except Exception:
        return None
    if d <= 0:
        return None
    level_max = float(r.get("level_max_cm") or 0)
    vol_max = float(r.get("volume_max_L") or 0)
    off = float(r.get("sensor_offset_cm") or 0)
    if level_max <= 0 or vol_max <= 0:
        return None
    level = level_max - (d - off)
    if level < 0:
        level = 0
    if level > level_max:
        level = level_max
    pct = round((level / level_max) * 100.0, 1)
    vol = round((level / level_max) * vol_max)
    return {"level_cm": int(round(level)), "pct": pct, "volume_L": int(vol)}

for r in sorted(reservoirs, key=lambda x: (x["node_id"], x["sensor_id"])):
    node_id = r["node_id"]
    sensor_id = r["sensor_id"]
    alias = (r["alias"] or "").strip()
    alias_l = alias.lower()
    st = node_status.get(node_id, "unknown")
    state_topic = f"aguada/{node_id}/{sensor_id}/state"
    raw = received.get(state_topic)

    disc_missing = sorted(expected_suffixes - disc_by_alias.get(alias_l, set()))
    disc_ok = (len(disc_missing) == 0)

    if not raw:
        print(f"   - {alias:6} {node_id}/{sensor_id}: status={st:7}  state=(none)  discovery={'OK' if disc_ok else 'MISSING'}")
        continue
    try:
        j = json.loads(raw)
    except Exception:
        print(f"   - {alias:6} {node_id}/{sensor_id}: status={st:7}  state=(invalid JSON)  discovery={'OK' if disc_ok else 'MISSING'}")
        continue

    ts = j.get("ts")
    age = fmt_age(ts)
    level = j.get("level_cm")
    pct = j.get("pct")
    vol = j.get("volume_L")
    dist = j.get("distance_cm")
    rssi = j.get("rssi")
    vbat = j.get("vbat")

    calc_ok = "?"
    exp = expected_from_distance(dist, r)
    if exp is not None and level is not None and pct is not None and vol is not None:
        try:
            dl = abs(int(level) - int(exp["level_cm"]))
            dp = abs(float(pct) - float(exp["pct"]))
            dv = abs(int(vol) - int(exp["volume_L"]))
            # tolerate 1cm / 0.2% / 2L rounding differences
            calc_ok = "OK" if (dl <= 1 and dp <= 0.2 and dv <= 2) else f"MISMATCH exp(level={exp['level_cm']} pct={exp['pct']} vol={exp['volume_L']})"
        except Exception:
            calc_ok = "?"

    print(f"   - {alias:6} {node_id}/{sensor_id}: status={st:7}  age={age:>6}  level={level}cm pct={pct}% vol={vol}L dist={dist}cm rssi={rssi} vbat={vbat}  calc={calc_ok}  discovery={'OK' if disc_ok else 'MISSING'}")

PY
echo ""

echo "=== Monitor Complete ==="
