#!/bin/bash
# Script para ativar o ambiente de desenvolvimento AGUADA

source .venv/bin/activate
export PATH="/home/luc/.local/bin:$PATH"

echo "✓ Ambiente AGUADA ativado"
echo "  Python: $(python3 --version)"
echo "  PlatformIO: $(python3 -m platformio --version)"
echo ""
echo "Exemplos de uso:"
echo "  cd firmware/node && python3 -m platformio run -e esp32-c3-supermini -t upload"
echo "  cd firmware/gateway && python3 -m platformio run -e gateway-esp32-usb -t upload"
echo "  python3 tools/bridge.py"
