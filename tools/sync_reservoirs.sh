#!/usr/bin/env bash
# Copia a fonte única (tools/reservoirs.yaml) para o backend web e confere.
# Uso: tools/sync_reservoirs.sh [destino]
set -e
SRC="$(cd "$(dirname "$0")" && pwd)/reservoirs.yaml"
DST="${1:-$HOME/cmms-monorepo/cmms-aguada/backend/reservoirs.yaml}"
cp "$SRC" "$DST"
cmp "$SRC" "$DST" && echo "ok: $DST"
