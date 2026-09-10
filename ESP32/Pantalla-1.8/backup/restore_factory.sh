#!/bin/sh
# Restaura el firmware de fabrica completo de la Waveshare ESP32-S3-Touch-LCD-1.83 (16 MB).
# Uso: ./restore_factory.sh [/dev/cu.usbmodemXXX]
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
[ -z "$PORT" ] && { echo "No hay puerto usbmodem. Conecta la placa por USB-C."; exit 1; }
cd "$(dirname "$0")"
echo "6f188fb9d35ee793a3423934a4fa4e7c1fef9cc9dae76f9f177dabe854a6cdb3  factory_full_16MB.bin" | shasum -a 256 -c || exit 1
esptool --port "$PORT" --baud 921600 --chip esp32s3 write-flash 0x0 factory_full_16MB.bin
