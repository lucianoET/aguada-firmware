#!/usr/bin/env bash
# Starts the Aguada bridge.
# Usage: ./start_bridge.sh [--port /dev/ttyACM2] [--debug]

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Optional environment overrides (gitignored): repoRoot/.env
ENV_FILE="${ENV_FILE:-$SCRIPT_DIR/../.env}"
if [[ -f "$ENV_FILE" ]]; then
    set -a
    # shellcheck disable=SC1090
    source "$ENV_FILE"
    set +a
fi

# ── MQTT broker ───────────────────────────────────────────────────────────────
# Prefer REMOTE_HOST/REMOTE_PORT from .env (used by start_bridge_autoswitch.sh).
MQTT_HOST="${MQTT_HOST:-${REMOTE_HOST:-192.168.0.177}}"
MQTT_PORT="${MQTT_PORT:-${REMOTE_PORT:-1883}}"
MQTT_USER="${MQTT_USER:-}"
MQTT_PASS="${MQTT_PASS:-}"
# ── InfluxDB 2.x (optional) ───────────────────────────────────────────────────
INFLUX_URL="${INFLUX_URL:-}"
INFLUX_TOKEN="${INFLUX_TOKEN:-}"
INFLUX_ORG="${INFLUX_ORG:-aguada}"
INFLUX_BUCKET="${INFLUX_BUCKET:-reservoirs}"
# ── Detect gateway port (by serial MAC 80:F1:B2:50:31:34) ────────────────────
GATEWAY_PORT=""
for p in /dev/ttyACM* /dev/ttyUSB*; do
    [[ -e "$p" ]] || continue
    serial=$(udevadm info "$p" 2>/dev/null | grep ID_SERIAL_SHORT | cut -d= -f2)
    if [[ "$serial" == "80:F1:B2:50:31:34" ]]; then
        GATEWAY_PORT="$p"
        break
    fi
done

if [[ -z "$GATEWAY_PORT" ]]; then
    for p in /dev/ttyACM* /dev/ttyUSB*; do
        [[ -e "$p" ]] || continue
        GATEWAY_PORT="$p"
        break
    done
fi

if [[ -z "$GATEWAY_PORT" ]]; then
    echo "[warn] Gateway port not found, using /dev/ttyACM0 as fallback"
    GATEWAY_PORT="/dev/ttyACM0"
fi
echo "[start] Gateway on $GATEWAY_PORT"

# ── Parse overrides ───────────────────────────────────────────────────────────
PORT="$GATEWAY_PORT"
EXTRA_ARGS=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --port)     PORT="$2"; shift 2 ;;
        --debug)    EXTRA_ARGS="--debug"; shift ;;
        *)          shift ;;
    esac
done

# ── Start bridge ──────────────────────────────────────────────────────────────
# Kill any stale bridge instances
pkill -f "bridge.py" 2>/dev/null && sleep 1 || true

echo "[start] Connecting to MQTT $MQTT_HOST:$MQTT_PORT as $MQTT_USER"

PYTHON_BIN="python3"
if [[ -x "$SCRIPT_DIR/../.venv/bin/python" ]]; then
    PYTHON_BIN="$SCRIPT_DIR/../.venv/bin/python"
fi

INFLUX_ARGS=()
if [[ -n "$INFLUX_URL" ]]; then
    if [[ -z "$INFLUX_TOKEN" ]]; then
        echo "[warn] INFLUX_URL set but INFLUX_TOKEN is empty; starting without Influx" >&2
    else
        INFLUX_ARGS=(
            --influx-url "$INFLUX_URL"
            --influx-token "$INFLUX_TOKEN"
            --influx-org "$INFLUX_ORG"
            --influx-bucket "$INFLUX_BUCKET"
        )
    fi
fi

exec "$PYTHON_BIN" "$SCRIPT_DIR/bridge.py" \
    --port "$PORT" \
    --mqtt "$MQTT_HOST" \
    --mqtt-port "$MQTT_PORT" \
    "${INFLUX_ARGS[@]}" \
    $EXTRA_ARGS
