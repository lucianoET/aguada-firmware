#!/usr/bin/env bash
# Flash NODE-03 (CB31/CB32) safely on /dev/ttyACM2.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NODE_DIR="$ROOT_DIR/firmware/node"
NODE_PORT="/dev/ttyACM2"
GATEWAY_PORT="/dev/ttyACM0"

if [[ ! -e "$NODE_PORT" ]]; then
  echo "[erro] Porta do node não encontrada: $NODE_PORT"
  echo "       Verifique conexão USB do NODE-03."
  exit 1
fi

if [[ -e "$GATEWAY_PORT" ]]; then
  echo "[info] Gateway detectado em $GATEWAY_PORT (preservado)"
fi

echo "[build] Compilando env node3-cb3..."
cd "$NODE_DIR"
pio run -e node3-cb3

echo "[flash] Gravando NODE-03 em $NODE_PORT..."
pio run -e node3-cb3 -t upload

echo "[ok] Gravação concluída para NODE-03 (CB31/CB32)."
echo "[dica] Para monitorar:"
echo "       cd $NODE_DIR && pio device monitor -b 115200 -p $NODE_PORT"
