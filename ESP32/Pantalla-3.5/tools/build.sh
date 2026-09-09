#!/bin/bash
# Compila TAC-1 con el entorno aislado (core esp32 3.3.11 en tools/).
#   tools/build.sh            compila
#   tools/build.sh flash      compila y sube por USB
#   tools/build.sh erase      BORRA todo el flash (adios firmware de fabrica;
#                             el respaldo esta en backup/) y luego sube
set -e
cd "$(dirname "$0")/.."
CFG=tools/arduino-cli.yaml
# Toolchain aislado dentro de tools/ (fuera de git; se descarga la primera vez):
#   arduino-cli --config-file $CFG core install esp32:esp32@3.3.11
export ARDUINO_DIRECTORIES_DATA="$PWD/tools/arduino-data"
export ARDUINO_DIRECTORIES_DOWNLOADS="$PWD/tools/downloads"
export ARDUINO_DIRECTORIES_USER="$PWD/tools/arduino-libs"
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,UploadSpeed=921600"
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)

arduino-cli --config-file $CFG compile --fqbn "$FQBN" --build-path build --warnings default firmware/TAC1

if [ "$1" = "erase" ]; then
  [ -n "$PORT" ] || { echo "no hay puerto usbmodem"; exit 1; }
  esptool --port "$PORT" --chip esp32s3 erase-flash
  sleep 2
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
fi
if [ "$1" = "flash" ] || [ "$1" = "erase" ]; then
  [ -n "$PORT" ] || { echo "no hay puerto usbmodem"; exit 1; }
  arduino-cli --config-file $CFG upload --fqbn "$FQBN" -p "$PORT" --input-dir build firmware/TAC1
fi
