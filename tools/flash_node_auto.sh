#!/bin/bash
# flash_node_auto.sh - Identifica e faz flash do node conectado

set -e

cd "$(dirname "$0")/../firmware/node"

echo "=== Aguada Node Flash Tool ==="
echo

# Por padrão, NÃO pare o bridge (ele costuma estar no gateway, em outra porta).
# Se você realmente precisar liberar a serial (ex: está tentando gravar na mesma porta), rode com:
#   STOP_BRIDGE=1 ./flash_node_auto.sh
STOP_BRIDGE="${STOP_BRIDGE:-0}"
if [[ "$STOP_BRIDGE" == "1" ]]; then
    if pgrep -f bridge.py > /dev/null; then
        echo "Parando bridge.py (STOP_BRIDGE=1)..."
        pkill -f bridge.py
        sleep 2
    fi
fi

# Procura dispositivos USB
PORTS=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "")

if [ -z "$PORTS" ]; then
    echo "❌ Nenhum dispositivo USB encontrado!"
    echo "   Conecte um node via USB e tente novamente."
    exit 1
fi

# Para cada porta, identifica o dispositivo
for PORT in $PORTS; do
    echo "--- Verificando $PORT ---"
    MAC=$(timeout 3 esptool.py --port $PORT read_mac 2>&1 | grep "MAC:" | head -1 | awk '{print $2}' || echo "")
    
    if [ -z "$MAC" ]; then
        echo "⚠️  Não conseguiu ler MAC de $PORT"
        continue
    fi
    
    NODE_ID=$(echo "$MAC" | awk -F: '{printf "0x%s%s", toupper($5), toupper($6)}')
    
    echo "✓ MAC:     $MAC"
    echo "✓ Node ID: $NODE_ID"
    
    # Identifica qual node é
    case "$NODE_ID" in
        "0X7758") ALIAS="CON (Castelo de Consumo)" ;;
        "0XEE02") ALIAS="CAV (Castelo de Incêndio)" ;;
        "0X2EC4") ALIAS="CB3 (Casa de Bombas Nº3)" ;;
        "0X9EAC") ALIAS="CIE (Cisterna IE)" ;;
        "0X3456") ALIAS="CBIF (Casa de Bombas IF)" ;;
        "0X80F1B250") ALIAS="GATEWAY (ignorar)" ; continue ;;
        *) ALIAS="Desconhecido" ;;
    esac
    
    echo "✓ Alias:   $ALIAS"
    echo
    
    read -p "🔥 Flash firmware v3.2.1 neste node? (s/N) " -n 1 -r
    echo
    
    if [[ $REPLY =~ ^[SsYy]$ ]]; then
        echo "Fazendo flash em $PORT..."
        pio run -e esp32-c3-supermini -t upload --upload-port "$PORT"
        echo
        echo "✓ Flash concluído!"
        echo
    else
        echo "⊘ Pulado."
    fi
done

echo
echo "=== Flash concluído ==="
echo "Desconecte o node e conecte o próximo, ou inicie o bridge novamente."
