#!/bin/bash
# Compila un sketch de firmware/ con el entorno AISLADO (core esp32 3.3.11 en tools/).
# Placa: Waveshare ESP32-S3-Touch-AMOLED-1.8 (CO5300 QSPI). Core 3.3.11 como su CI.
#   tools/build.sh [Sketch]         compila (default: BobAerogro)
#   tools/build.sh [Sketch] flash   compila y sube — SOLO con OK explicito de Jose
#   tools/build.sh [Sketch] monitor compila, sube y abre serial (sin DTR/RTS)
# Primera vez en una Mac nueva (si no existe ../Pantalla-3.5/tools/arduino-data):
#   rm tools/arduino-data tools/downloads; mkdir tools/arduino-data tools/downloads
#   arduino-cli --config-file tools/arduino-cli.yaml core install esp32:esp32@3.3.11
set -e
cd "$(dirname "$0")/.."
CFG=tools/arduino-cli.yaml
export ARDUINO_DIRECTORIES_DATA="$PWD/tools/arduino-data"
export ARDUINO_DIRECTORIES_DOWNLOADS="$PWD/tools/downloads"
export ARDUINO_DIRECTORIES_USER="$PWD/tools/arduino-libs"
# Igual que el CI oficial de Waveshare (AMOLED-1.8: app3M_fat9M_16MB) + PSRAM OPI, QIO y CDC por USB
FQBN="esp32:esp32:esp32s3:FlashMode=qio,FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB,UploadSpeed=921600"
SKETCH="${1:-BobAerogro}"
ACTION="${2:-}"
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)

arduino-cli --config-file $CFG compile --fqbn "$FQBN" --build-path "build/$SKETCH" --warnings default "firmware/$SKETCH"

if [ "$ACTION" = "flash" ] || [ "$ACTION" = "monitor" ]; then
  [ -n "$PORT" ] || { echo "no hay puerto usbmodem"; exit 1; }
  # bobd (puente con la Mac) tiene el puerto abierto: pedirle que lo suelte
  [ -S /tmp/bob-aerogro.sock ] && printf '__pause__\n' | nc -U -w 2 /tmp/bob-aerogro.sock >/dev/null 2>&1 && sleep 1
  echo "Flasheando $SKETCH a $PORT ..."
  arduino-cli --config-file $CFG upload --fqbn "$FQBN" -p "$PORT" --input-dir "build/$SKETCH" "firmware/$SKETCH"
fi
if [ "$ACTION" = "monitor" ]; then
  sleep 2
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
  tools/.venv/bin/python tools/monitor.py "$PORT" 8
fi
