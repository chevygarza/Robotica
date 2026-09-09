#!/bin/sh
# Restaura el firmware de fábrica completo de la Guition JC3248W535 (16 MB).
# Uso: ./restore_factory.sh [/dev/cu.usbmodemXXX]
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
[ -z "$PORT" ] && { echo "No hay puerto usbmodem. Mantén Boot, pulsa Rest, suelta Boot."; exit 1; }
cd "$(dirname "$0")"
echo "46311fde02133578d325290b5f07e39a701afdc72c9ff2e58acb6edcfaf29cb4  factory_16MB.bin" | shasum -a 256 -c || exit 1
esptool --port "$PORT" --baud 921600 --chip esp32s3 write-flash 0 factory_16MB.bin
