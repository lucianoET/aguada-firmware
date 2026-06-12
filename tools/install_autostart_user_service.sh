#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="aguada-bridge-autoswitch.service"
DST_DIR="$HOME/.config/systemd/user"
PATH_NAME="aguada-bridge-autoswitch.path"

mkdir -p "$DST_DIR"

for unit in "$SERVICE_NAME" "$PATH_NAME"; do
	cp "/home/luc/Dev/espnow-ha/tools/systemd/${unit}" "$DST_DIR/${unit}"
done

systemctl --user daemon-reload
systemctl --user enable --now "$SERVICE_NAME" "$PATH_NAME"

if loginctl enable-linger "$USER" >/dev/null 2>&1; then
	echo "[ok] Linger ativado para $USER"
else
	echo "[warn] Não foi possível ativar linger automaticamente. Se quiser autostart sem login ativo, rode: sudo loginctl enable-linger $USER"
fi

echo "[ok] Units instaladas e iniciadas: $SERVICE_NAME, $PATH_NAME"
echo "[status]"
systemctl --user --no-pager --full status "$SERVICE_NAME" | sed -n '1,12p'
echo
systemctl --user --no-pager --full status "$PATH_NAME" | sed -n '1,12p'

echo
echo "Comandos úteis:"
echo "  systemctl --user status $SERVICE_NAME"
echo "  systemctl --user status $PATH_NAME"
echo "  systemctl --user restart $SERVICE_NAME"
echo "  systemctl --user stop $SERVICE_NAME"
