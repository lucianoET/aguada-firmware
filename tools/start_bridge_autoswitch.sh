#!/usr/bin/env bash
set -u

# Optional environment overrides (gitignored): repoRoot/.env
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="${ENV_FILE:-$SCRIPT_DIR/../.env}"
if [[ -f "$ENV_FILE" ]]; then
  set -a
  # shellcheck disable=SC1090
  source "$ENV_FILE"
  set +a
fi

VENV_PY="/home/luc/Dev/espnow-ha/.venv/bin/python"
BRIDGE="/home/luc/Dev/espnow-ha/tools/bridge.py"
BROKER="/home/luc/Dev/espnow-ha/tools/mqtt_broker.py"
# Optional USB serial short-id for the gateway adapter (udev ID_SERIAL_SHORT).
# Leave empty to accept the first available /dev/ttyACM* or /dev/ttyUSB*.
GATEWAY_USB_SERIAL_SHORT="${GATEWAY_USB_SERIAL_SHORT:-}"
SERIAL_PORT=""
REMOTE_HOST="${REMOTE_HOST:-127.0.0.1}"
REMOTE_PORT="${REMOTE_PORT:-1883}"
MQTT_USER="${MQTT_USER:-}"
MQTT_PASS="${MQTT_PASS:-}"

# When this file exists, the supervisor will keep the bridge stopped.
# Useful for flashing (serial bootloader needs exclusive access).
BRIDGE_PAUSE_FILE="${BRIDGE_PAUSE_FILE:-/tmp/aguada_bridge.pause}"

INFLUX_URL="http://localhost:8086"
INFLUX_URL="${INFLUX_URL:-}"
INFLUX_TOKEN="${INFLUX_TOKEN:-}"
INFLUX_ORG="${INFLUX_ORG:-aguada}"
INFLUX_BUCKET="${INFLUX_BUCKET:-reservoirs}"
GATEWAY_TIMEOUT_S="${GATEWAY_TIMEOUT_S:-90}"
SERIAL_RECONNECT_DELAY_S="${SERIAL_RECONNECT_DELAY_S:-2}"

BRIDGE_LOG="/tmp/bridge_debug.log"
BROKER_LOG="/tmp/local_mqtt.log"
SWITCH_LOG="/tmp/bridge_autoswitch.log"

PAUSE_STATE=0

serial_port_busy_by_other_process() {
  local port="$1"
  command -v lsof >/dev/null 2>&1 || return 1
  [[ -e "$port" ]] || return 1

  local pids pid cmd
  pids=$(lsof -t "$port" 2>/dev/null | sort -u || true)
  [[ -n "$pids" ]] || return 1

  for pid in $pids; do
    [[ -r "/proc/$pid/cmdline" ]] || continue
    cmd=$(tr '\0' ' ' < "/proc/$pid/cmdline" | tr -s ' ')
    # Allow our own bridge processes (we will pkill them when restarting)
    if [[ "$cmd" != *"bridge.py"* ]]; then
      return 0
    fi
  done

  return 1
}

detect_gateway_port() {
  # 0) If configured, match by known USB serial short-id (most reliable)
  if [[ -n "$GATEWAY_USB_SERIAL_SHORT" ]]; then
    for p in /dev/ttyACM* /dev/ttyUSB*; do
      [[ -e "$p" ]] || continue
      serial=$(udevadm info "$p" 2>/dev/null | grep -E '^(E: )?ID_(USB_)?SERIAL_SHORT=' | head -n1 | cut -d= -f2 || true)
      if [[ "$serial" == "$GATEWAY_USB_SERIAL_SHORT" ]]; then
        echo "$p"
        return 0
      fi
    done
  fi

  # 1) Prefer stable by-id symlink(s) for known CH34x gateway adapters
  local byid_known
  for byid_known in \
    "/dev/serial/by-id/usb-1a86_USB_Single_Serial_5A7B156617-if00" \
    "/dev/serial/by-id/usb-1a86_USB_Single_Serial_5A7B067329-if00"; do
    if [[ -e "$byid_known" ]]; then
      echo "$byid_known"
      return 0
    fi
  done

  # 2) Prefer any by-id symlink that looks like a CH34x/1a86 adapter
  for p in /dev/serial/by-id/usb-1a86_*; do
    [[ -e "$p" ]] || continue
    echo "$p"
    return 0
  done

  # 3) Fallback: first stable by-id serial symlink
  for p in /dev/serial/by-id/*; do
    [[ -e "$p" ]] || continue
    echo "$p"
    return 0
  done

  # 4) Fallback: pick first available ACM/USB device
  for p in /dev/ttyACM* /dev/ttyUSB*; do
    [[ -e "$p" ]] || continue
    echo "$p"
    return 0
  done

  echo ""
  return 1
}

is_remote_up() {
  python3 - <<PY
import socket
s=socket.socket(); s.settimeout(2)
try:
    s.connect(("$REMOTE_HOST", int($REMOTE_PORT)))
    print("UP")
except Exception:
    print("DOWN")
finally:
    s.close()
PY
}

is_local_target() {
  case "$REMOTE_HOST" in
    127.0.0.1|localhost|::1)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

start_local_broker_if_needed() {
  if ! pgrep -f "mqtt_broker.py --host 127.0.0.1 --port $REMOTE_PORT" >/dev/null 2>&1; then
    nohup "$VENV_PY" "$BROKER" --host 127.0.0.1 --port "$REMOTE_PORT" > "$BROKER_LOG" 2>&1 &
    echo "[$(date '+%F %T')] local broker started pid=$!" >> "$SWITCH_LOG"
  fi
}

stop_local_broker_if_running() {
  if pgrep -f "mqtt_broker.py --host 127.0.0.1 --port $REMOTE_PORT" >/dev/null 2>&1; then
    pkill -f "mqtt_broker.py --host 127.0.0.1 --port $REMOTE_PORT" || true
    echo "[$(date '+%F %T')] local broker stopped" >> "$SWITCH_LOG"
  fi
}

ensure_bridge_target() {
  local target_host="$1"

  if [[ -z "$SERIAL_PORT" ]] || [[ ! -e "$SERIAL_PORT" ]]; then
    SERIAL_PORT="$(detect_gateway_port)"
    if [[ -z "$SERIAL_PORT" ]]; then
      echo "[$(date '+%F %T')] gateway serial not found; will retry" >> "$SWITCH_LOG"
      return 0
    fi
    if [[ -n "$GATEWAY_USB_SERIAL_SHORT" ]]; then
      echo "[$(date '+%F %T')] gateway port detected: $SERIAL_PORT (usb-serial=$GATEWAY_USB_SERIAL_SHORT)" >> "$SWITCH_LOG"
    else
      echo "[$(date '+%F %T')] gateway port detected: $SERIAL_PORT (first available serial device)" >> "$SWITCH_LOG"
    fi
  fi

  if serial_port_busy_by_other_process "$SERIAL_PORT"; then
    echo "[$(date '+%F %T')] gateway serial busy (non-bridge holder); skipping bridge start for now: $SERIAL_PORT" >> "$SWITCH_LOG"
    return 0
  fi

  if pgrep -f "bridge.py --port $SERIAL_PORT --mqtt $target_host --mqtt-port $REMOTE_PORT" >/dev/null 2>&1; then
    return 0
  fi

  # Ensure only one bridge instance exists (prevents stale process on old USB port)
  pkill -f "python.*bridge.py" || true
  sleep 1

  # InfluxDB is optional: only pass --influx-* when influxdb-client is installed
  local INFLUX_ARGS=()
  if [[ -n "$INFLUX_URL" ]] && "$VENV_PY" - <<'PY' >/dev/null 2>&1
import importlib.util
raise SystemExit(0 if importlib.util.find_spec('influxdb_client') else 1)
PY
  then
    if [[ -z "$INFLUX_TOKEN" ]]; then
      echo "[$(date '+%F %T')] INFLUX_URL set but INFLUX_TOKEN is empty; starting bridge without Influx" >> "$SWITCH_LOG"
    else
      INFLUX_ARGS=(
        --influx-url "$INFLUX_URL"
        --influx-token "$INFLUX_TOKEN"
        --influx-org "$INFLUX_ORG"
        --influx-bucket "$INFLUX_BUCKET"
      )
    fi
  else
    if [[ -n "$INFLUX_URL" ]]; then
      echo "[$(date '+%F %T')] influxdb-client not found; starting bridge without Influx" >> "$SWITCH_LOG"
    fi
  fi

  # Credentials are supplied via environment (MQTT_USER/MQTT_PASS) to avoid
  # exposing secrets on the process command line.
  if [[ -n "$MQTT_USER" ]] && [[ -z "$MQTT_PASS" ]]; then
    echo "[$(date '+%F %T')] MQTT_USER set but MQTT_PASS is empty; broker auth may fail" >> "$SWITCH_LOG"
  fi

  nohup "$VENV_PY" "$BRIDGE" \
    --port "$SERIAL_PORT" \
    --mqtt "$target_host" \
    --mqtt-port "$REMOTE_PORT" \
    --gateway-timeout "$GATEWAY_TIMEOUT_S" \
    --serial-reconnect-delay "$SERIAL_RECONNECT_DELAY_S" \
    "${INFLUX_ARGS[@]}" \
    --debug > "$BRIDGE_LOG" 2>&1 &

  echo "[$(date '+%F %T')] bridge started pid=$! target=$target_host gateway_timeout=${GATEWAY_TIMEOUT_S}s reconnect_delay=${SERIAL_RECONNECT_DELAY_S}s" >> "$SWITCH_LOG"
}

mkdir -p /tmp

SERIAL_PORT="$(detect_gateway_port)"
if [[ -n "$SERIAL_PORT" ]]; then
  if [[ -n "$GATEWAY_USB_SERIAL_SHORT" ]]; then
    echo "[$(date '+%F %T')] initial gateway port: $SERIAL_PORT (usb-serial=$GATEWAY_USB_SERIAL_SHORT)" >> "$SWITCH_LOG"
  else
    echo "[$(date '+%F %T')] initial gateway port: $SERIAL_PORT (first available serial device)" >> "$SWITCH_LOG"
  fi
else
  echo "[$(date '+%F %T')] initial gateway port: not found" >> "$SWITCH_LOG"
fi

echo "[$(date '+%F %T')] autoswitch supervisor started" >> "$SWITCH_LOG"

while true; do
  if [[ -e "$BRIDGE_PAUSE_FILE" ]]; then
    if [[ $PAUSE_STATE -eq 0 ]]; then
      echo "[$(date '+%F %T')] bridge paused (lock file present): $BRIDGE_PAUSE_FILE" >> "$SWITCH_LOG"
      PAUSE_STATE=1
    fi
    pkill -f "python.*bridge.py" >/dev/null 2>&1 || true
    sleep 2
    continue
  elif [[ $PAUSE_STATE -eq 1 ]]; then
    echo "[$(date '+%F %T')] bridge resumed (lock file removed): $BRIDGE_PAUSE_FILE" >> "$SWITCH_LOG"
    PAUSE_STATE=0
  fi

  if is_local_target; then
    start_local_broker_if_needed
    ensure_bridge_target "$REMOTE_HOST"
  else
    # Most remote brokers (ex: HA/Mosquitto) require authentication.
    # If creds are missing, do NOT switch away from the local broker or we'll
    # end up with no working telemetry path.
    if [[ -z "$MQTT_USER" ]]; then
      echo "[$(date '+%F %T')] REMOTE_HOST=$REMOTE_HOST but MQTT_USER is empty; staying on local broker (configure MQTT_USER/MQTT_PASS in .env to publish remotely)" >> "$SWITCH_LOG"
      start_local_broker_if_needed
      ensure_bridge_target "127.0.0.1"
      sleep 20
      continue
    fi

    state="$(is_remote_up)"
    if [[ "$state" == "UP" ]]; then
      ensure_bridge_target "$REMOTE_HOST"
      stop_local_broker_if_running
    else
      start_local_broker_if_needed
      ensure_bridge_target "127.0.0.1"
    fi
  fi

  sleep 20
done
