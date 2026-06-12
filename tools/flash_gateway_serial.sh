#!/usr/bin/env bash
# flash_gateway_serial.sh
#
# Flash the Aguada serial gateway firmware (PlatformIO) while safely releasing the
# gateway serial port and temporarily stopping the autoswitch supervisor.
#
# This targets the classic ESP32 gateway connected via a USB-serial adapter.
# Default env: gateway-esp32-safe (115200 + --no-stub).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GATEWAY_DIR="$ROOT_DIR/firmware/gateway"
HELPER="$ROOT_DIR/tools/with_serial_port.sh"

PORT="/dev/ttyACM0"
PIO_ENV="gateway-esp32-safe"
TIMEOUT_S=60

usage() {
  cat <<EOF
Usage:
  $0 [--port /dev/ttyACM0] [--env gateway-esp32-safe] [--timeout 60]

Options:
  --port <device>     Gateway serial device (default: /dev/ttyACM0)
  --env <pio_env>     PlatformIO environment (default: gateway-esp32-safe)
  --timeout <sec>     Wait time for port to become free (default: 60)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port) PORT="$2"; shift 2 ;;
    --env) PIO_ENV="$2"; shift 2 ;;
    --timeout) TIMEOUT_S="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "[erro] argumento desconhecido: $1" >&2; usage; exit 2 ;;
  esac
done

if [[ ! -x "$HELPER" ]]; then
  echo "[erro] Helper não executável: $HELPER" >&2
  echo "       Rode: chmod +x tools/with_serial_port.sh" >&2
  exit 1
fi

if [[ ! -d "$GATEWAY_DIR" ]]; then
  echo "[erro] Diretório gateway não encontrado: $GATEWAY_DIR" >&2
  exit 1
fi

if ! command -v lsof >/dev/null 2>&1; then
  echo "[erro] 'lsof' não encontrado. Instale (ex: sudo apt install lsof)." >&2
  exit 1
fi

# Remember whether autoswitch units were active
svc_was_active=0
path_was_active=0
web_backend_was_active=0
if command -v systemctl >/dev/null 2>&1; then
  if systemctl --user is-active --quiet aguada-bridge-autoswitch.service 2>/dev/null; then
    svc_was_active=1
  fi
  if systemctl --user is-active --quiet aguada-bridge-autoswitch.path 2>/dev/null; then
    path_was_active=1
  fi

  # aguada-web backend (uvicorn) may also own the gateway serial.
  if systemctl --user is-active --quiet aguada-web-backend.service 2>/dev/null; then
    web_backend_was_active=1
  fi

  # Stop both so bridge can't respawn mid-flash (and next start will pick up the latest script).
  systemctl --user stop aguada-bridge-autoswitch.path aguada-bridge-autoswitch.service 2>/dev/null || true

  # Stop the web backend service so it doesn't restart uvicorn mid-flash.
  systemctl --user stop aguada-web-backend.service 2>/dev/null || true
fi

# If aguada-web backend is holding the same serial port, stop it (it will break flashing).
if lsof "$PORT" >/dev/null 2>&1; then
  pids=$(lsof -t "$PORT" 2>/dev/null | sort -u || true)
  for pid in $pids; do
    cmdline=""
    if [[ -r "/proc/$pid/cmdline" ]]; then
      cmdline=$(tr '\0' ' ' < "/proc/$pid/cmdline" | tr -s ' ')
    fi

    if [[ "$cmdline" == *"uvicorn backend.main:app"* ]]; then
      echo "[info] Parando uvicorn (backend.main:app) que está usando $PORT (pid=$pid)" >&2
      kill "$pid" 2>/dev/null || true
    fi
  done
  sleep 1
fi

# If some other process still holds the port, abort with context.
if lsof "$PORT" >/dev/null 2>&1; then
  echo "[erro] $PORT ainda está em uso. Feche o processo abaixo e tente novamente:" >&2
  lsof "$PORT" >&2 || true
  exit 1
fi

# Use helper to ensure exclusive serial access while uploading.
"$HELPER" --port "$PORT" --timeout "$TIMEOUT_S" -- \
  pio run -d "$GATEWAY_DIR" -e "$PIO_ENV" -t upload --upload-port "$PORT"

# Restore autoswitch units (if they were active)
if command -v systemctl >/dev/null 2>&1; then
  if [[ $web_backend_was_active -eq 1 ]]; then
    systemctl --user start aguada-web-backend.service 2>/dev/null || true
  fi
  if [[ $path_was_active -eq 1 ]]; then
    systemctl --user start aguada-bridge-autoswitch.path 2>/dev/null || true
  fi
  if [[ $svc_was_active -eq 1 ]]; then
    systemctl --user start aguada-bridge-autoswitch.service 2>/dev/null || true
  fi
fi

echo "[ok] Flash concluído."