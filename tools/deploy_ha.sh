#!/usr/bin/env bash
# =============================================================================
# deploy_ha.sh - Copia arquivos de /homeassistant para /config no HAOS via Samba
# e dispara check_config + restart do Home Assistant via REST API.
#
# USO:
#   chmod +x tools/deploy_ha.sh
#   HA_TOKEN=SEU_TOKEN SAMBA_PASS=SUA_SENHA ./tools/deploy_ha.sh
#
# Dica:
# - HA_TOKEN: crie em Home Assistant -> Perfil -> Long-Lived Access Tokens
# - SAMBA_PASS: HA -> Settings -> Add-ons -> Samba share -> campo "password"
# =============================================================================
set -euo pipefail

# Repo paths
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_HA_DIR="$REPO_ROOT/homeassistant"

# Optional: load .env from repo root (not committed)
if [[ -f "$REPO_ROOT/.env" ]]; then
  set -a
  # shellcheck disable=SC1090
  source "$REPO_ROOT/.env"
  set +a
fi

# ---- Configuration (overridable via env) ----
HA_HOST="${HA_HOST:-192.168.0.177}"
HA_PORT="${HA_PORT:-8123}"
HA_SCHEME="${HA_SCHEME:-http}"
HA_URL="${HA_URL:-$HA_SCHEME://$HA_HOST:$HA_PORT}"

SAMBA_USER="${SAMBA_USER:-homeassistant}"
SAMBA_PASS="${SAMBA_PASS:-}"          # pass via env: SAMBA_PASS=xxx ./deploy_ha.sh
HA_TOKEN="${HA_TOKEN:-}"              # pass via env (or .env)

# Files to copy to /config (root of the config Samba share)
HA_CONFIG_FILES=(
  configuration.yaml
  automations.yaml
  template_sensors.yaml
  statistics_sensors.yaml
  input_boolean.yaml
  input_button.yaml
  input_datetime.yaml
  input_number.yaml
  input_select.yaml
  input_text.yaml
  dashboard.yaml
  mqtt_gateway_sensors.yaml
)

# ---- Pre-flight checks ----
if [[ -z "$SAMBA_PASS" ]]; then
  echo "ERRO: defina SAMBA_PASS antes de executar."
  echo "  HA_TOKEN=... SAMBA_PASS=SUA_SENHA ./tools/deploy_ha.sh"
  exit 1
fi

if [[ -z "$HA_TOKEN" ]]; then
  echo "ERRO: defina HA_TOKEN (long-lived token) antes de executar."
  echo "  HA_TOKEN=... SAMBA_PASS=... ./tools/deploy_ha.sh"
  exit 1
fi

if [[ ! -d "$REPO_HA_DIR" ]]; then
  echo "ERRO: diretório não encontrado: $REPO_HA_DIR"
  exit 1
fi

if ! command -v smbclient &>/dev/null; then
  echo "ERRO: smbclient não instalado. Instale com: sudo apt install smbclient"
  exit 1
fi

if ! command -v curl &>/dev/null; then
  echo "ERRO: curl não instalado."
  exit 1
fi

echo "──────────────────────────────────────────────────────────"
echo " Deploy Aguada HA -> $HA_URL"
echo " Usuario Samba: $SAMBA_USER"
echo " Origem: $REPO_HA_DIR"
echo "──────────────────────────────────────────────────────────"

# Test Samba connection
echo "[1/4] Testando conexao Samba..."
smbclient "//$HA_HOST/config" -U "$SAMBA_USER%$SAMBA_PASS" -c "ls" > /dev/null 2>&1 \
  || { echo "ERRO: credenciais Samba invalidas ou share inacessivel."; exit 1; }
echo "  OK: Samba"

# Copy files
echo "[2/4] Copiando arquivos para /config..."
for file in "${HA_CONFIG_FILES[@]}"; do
  src="$REPO_HA_DIR/$file"
  if [[ ! -f "$src" ]]; then
    echo "  WARN: arquivo nao encontrado, pulando: $file"
    continue
  fi
  smbclient "//$HA_HOST/config" -U "$SAMBA_USER%$SAMBA_PASS" \
    -c "put \"$src\" \"$file\"" > /dev/null 2>&1 \
    && echo "  OK: $file" \
    || echo "  FAIL: $file"
done

# Check config via REST API
echo "[3/4] Verificando configuracao do HA (check_config)..."
CHECK_JSON=$(curl -sS -X POST \
  -H "Authorization: Bearer $HA_TOKEN" \
  -H "Content-Type: application/json" \
  "$HA_URL/api/config/core/check_config")

RESULT=$(echo "$CHECK_JSON" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('result','?'), '|', d.get('errors',''))" 2>/dev/null || echo "erro ao parsear resposta")

echo "  Resultado: $RESULT"
if echo "$RESULT" | grep -qi "invalid\|error"; then
  echo "  ERRO: configuracao invalida - corrigir antes de reiniciar."
  echo "  Resposta completa: $CHECK_JSON"
  exit 1
fi

echo "  OK: configuracao valida"

# Restart HA
echo "[4/4] Reiniciando Home Assistant..."
curl -sS -X POST \
  -H "Authorization: Bearer $HA_TOKEN" \
  -H "Content-Type: application/json" \
  "$HA_URL/api/services/homeassistant/restart" \
  --data "{}" > /dev/null

echo "  OK: reinicio solicitado (aguarde ~60s)"
echo "  Dashboard: $HA_URL"
echo "──────────────────────────────────────────────────────────"
echo " Deploy concluido"
echo "──────────────────────────────────────────────────────────"
