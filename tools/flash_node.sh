#!/bin/bash
# Flash Node Firmware v3.2 (com correções de overflow)
# Uso: ./flash_node.sh [porta]
# Se não especificar porta, detecta automaticamente

set -e

FIRMWARE_DIR="/home/luc/Dev/espnow-ha/firmware/node"
FIRMWARE_BIN="$FIRMWARE_DIR/.pio/build/esp32-c3-supermini/firmware.bin"
PORT="${1:-auto}"

echo "=== Flash Node Aguada v3.2 ==="
echo ""

# Verificar se firmware existe
if [ ! -f "$FIRMWARE_BIN" ]; then
    echo "❌ Firmware não encontrado!"
    echo "   Compilando firmware..."
    cd "$FIRMWARE_DIR"
    pio run -e esp32-c3-supermini
    echo ""
fi

echo "✅ Firmware: $(ls -lh $FIRMWARE_BIN | awk '{print $5}')"
echo ""

# Auto-detectar porta se não especificada
if [ "$PORT" = "auto" ]; then
    echo "🔍 Detectando porta USB..."
    
    # Listar portas disponíveis (exceto gateway)
    AVAILABLE_PORTS=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | grep -v "/dev/ttyACM1" || true)
    
    if [ -z "$AVAILABLE_PORTS" ]; then
        echo ""
        echo "⚠️  Nenhum node detectado!"
        echo ""
        echo "Por favor:"
        echo "  1. Conecte um ESP32-C3 node via USB"
        echo "  2. Execute novamente: ./flash_node.sh"
        echo ""
        echo "Portas disponíveis:"
        ls -la /dev/ttyACM* /dev/ttyUSB* 2>&1 || true
        exit 1
    fi
    
    # Pegar primeira porta disponível
    PORT=$(echo "$AVAILABLE_PORTS" | head -1)
    echo "   Encontrado: $PORT"
fi

echo ""
echo "📡 Porta: $PORT"
echo ""

# Parar bridge temporariamente se estiver usando a porta
BRIDGE_PID=$(pgrep -f "bridge.py.*$PORT" || true)
if [ -n "$BRIDGE_PID" ]; then
    echo "⏸️  Parando bridge.py (PID $BRIDGE_PID)..."
    kill $BRIDGE_PID
    sleep 2
fi

# Flash
echo "🔥 Iniciando flash..."
echo ""

cd "$FIRMWARE_DIR"
pio run -e esp32-c3-supermini -t upload --upload-port "$PORT"

FLASH_STATUS=$?

echo ""
if [ $FLASH_STATUS -eq 0 ]; then
    echo "✅ Flash concluído com sucesso!"
    echo ""
    echo "O node foi reiniciado com firmware v3.2 (overflow fixes)."
    echo ""
    echo "Próximos passos:"
    echo "  1. Desconecte o node do USB"
    echo "  2. Conecte no campo (bateria/alimentação)"
    echo "  3. Aguarde 30-60s para começar transmissão"
    echo ""
else
    echo "❌ Erro no flash!"
    exit $FLASH_STATUS
fi

# Reiniciar bridge se estava rodando
if [ -n "$BRIDGE_PID" ]; then
    echo "🔄 O bridge estava usando $PORT e foi parado." 
    echo "   Se você usa o autoswitch via systemd, reinicie com:"
    echo "     systemctl --user restart aguada-bridge-autoswitch.service"
    echo "   (ou execute novamente tools/start_bridge_autoswitch.sh)"
fi

echo ""
echo "Concluído! ✨"
