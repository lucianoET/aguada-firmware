#!/usr/bin/env bash
# with_serial_port.sh
#
# Run a command that needs exclusive access to a serial port (e.g. flashing an ESP)
# while temporarily pausing Aguada's serial bridge supervisor.
#
# Example:
#   ./tools/with_serial_port.sh --port /dev/ttyACM0 --timeout 30 -- \
#     pio run -d firmware/gateway -t upload --upload-port /dev/ttyACM0
#
# Notes:
# - A serial device can't be shared reliably during flashing (bootloader toggles DTR/RTS and expects
#   a clean stream). Any bridge reading/writing the port will break the handshake.
# - This script cooperates with tools/start_bridge_autoswitch.sh via BRIDGE_PAUSE_FILE.

set -euo pipefail

PORT=""
TIMEOUT_S=30
KILL_ALL_HOLDERS=0

BRIDGE_PAUSE_FILE="${BRIDGE_PAUSE_FILE:-/tmp/aguada_bridge.pause}"

usage() {
  cat <<'EOF'
Usage:
  with_serial_port.sh --port <device> [--timeout <seconds>] [--kill-all-holders] -- <command...>

Options:
  --port <device>         Serial device (e.g. /dev/ttyACM0 or /dev/serial/by-id/...).
  --timeout <seconds>     Time to wait for the port to become free (default: 30).
  --kill-all-holders      Also kill non-bridge processes holding the port (use with care).
  -h, --help              Show help.

Environment:
  BRIDGE_PAUSE_FILE        Path used to pause tools/start_bridge_autoswitch.sh
                           (default: /tmp/aguada_bridge.pause).
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port) PORT="$2"; shift 2 ;;
    --timeout) TIMEOUT_S="$2"; shift 2 ;;
    --kill-all-holders) KILL_ALL_HOLDERS=1; shift ;;
    --) shift; break ;;
    -h|--help) usage; exit 0 ;;
    *)
      echo "[erro] Argumento desconhecido: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [[ -z "$PORT" ]]; then
  echo "[erro] Você precisa passar --port <device>" >&2
  usage
  exit 2
fi

if [[ $# -lt 1 ]]; then
  echo "[erro] Você precisa passar um comando após '--'" >&2
  usage
  exit 2
fi

if [[ ! -e "$PORT" ]]; then
  echo "[erro] Porta serial não existe: $PORT" >&2
  exit 1
fi

if ! command -v lsof >/dev/null 2>&1; then
  echo "[erro] 'lsof' não encontrado. Instale (ex: sudo apt install lsof) para usar este helper." >&2
  exit 1
fi

# ModemManager frequentemente abre /dev/ttyACM* durante resets e pode corromper o protocolo do esptool
# (erro típico: "Invalid head of packet" / "chip stopped responding").
if command -v systemctl >/dev/null 2>&1; then
  if systemctl is-active --quiet ModemManager 2>/dev/null; then
    echo "[warn] ModemManager está ativo — pode interferir no flash em $PORT." >&2
    echo "       Recomendo parar temporariamente durante o flash:" >&2
    echo "         sudo systemctl stop ModemManager" >&2
    echo "       (depois você pode reativar com: sudo systemctl start ModemManager)" >&2

    STOP_MODEMMANAGER="${STOP_MODEMMANAGER:-0}"
    if [[ "$STOP_MODEMMANAGER" == "1" ]]; then
      if command -v sudo >/dev/null 2>&1; then
        if sudo -n systemctl stop ModemManager >/dev/null 2>&1; then
          echo "[info] ModemManager parado via sudo -n" >&2
        else
          echo "[warn] Não consegui parar ModemManager automaticamente (sudo precisa de senha)." >&2
        fi
      fi
    fi
  fi
fi

RESTORE_PAUSE_FILE=0

cleanup() {
  if [[ $RESTORE_PAUSE_FILE -eq 1 ]]; then
    rm -f "$BRIDGE_PAUSE_FILE" 2>/dev/null || true
  fi
}
trap cleanup EXIT

# 1) Pause the autoswitch supervisor so it doesn't respawn bridge.py while we flash.
# Only touch the pause file if it didn't exist already.
if [[ ! -e "$BRIDGE_PAUSE_FILE" ]]; then
  : > "$BRIDGE_PAUSE_FILE"
  RESTORE_PAUSE_FILE=1
fi

# 2) Kill Aguada bridge instances holding this port.
PIDS=$(lsof -t "$PORT" 2>/dev/null | sort -u || true)
if [[ -n "$PIDS" ]]; then
  for pid in $PIDS; do
    cmdline=""
    if [[ -r "/proc/$pid/cmdline" ]]; then
      cmdline=$(tr '\0' ' ' < "/proc/$pid/cmdline" | sed 's/[[:space:]]\+/ /g' | sed 's/[[:space:]]$//')
    fi

    if [[ "$cmdline" == *"tools/bridge.py"* ]] || [[ "$cmdline" == *"/tools/bridge.py"* ]] || [[ "$cmdline" == *" bridge.py"* ]]; then
      echo "[info] Parando bridge que está usando $PORT (pid=$pid)"
      kill "$pid" 2>/dev/null || true
      continue
    fi

    if [[ $KILL_ALL_HOLDERS -eq 1 ]]; then
      echo "[warn] Matando processo que segura $PORT (pid=$pid) cmd='$cmdline'"
      kill "$pid" 2>/dev/null || true
      continue
    fi

    echo "[erro] $PORT está em uso por outro processo (pid=$pid)." >&2
    if [[ -n "$cmdline" ]]; then
      echo "       cmd: $cmdline" >&2
    fi
    echo "       Pare esse processo e tente novamente, ou re-execute com --kill-all-holders." >&2
    exit 1
  done
fi

# 3) Wait for the port to be free.
deadline=$(( $(date +%s) + TIMEOUT_S ))
while true; do
  if ! lsof "$PORT" >/dev/null 2>&1; then
    break
  fi
  if (( $(date +%s) >= deadline )); then
    echo "[erro] Timeout esperando liberar $PORT (${TIMEOUT_S}s)." >&2
    echo "       Processos ainda segurando a porta:" >&2
    lsof "$PORT" 2>/dev/null >&2 || true
    exit 1
  fi
  sleep 0.2
done

# 4) Run the requested command.
"$@"
