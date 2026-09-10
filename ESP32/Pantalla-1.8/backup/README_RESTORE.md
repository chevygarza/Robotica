# Respaldo de fabrica — Waveshare ESP32-S3-Touch-LCD-1.83

## Placa
- Chip: ESP32-S3R8 (ESP32-S3, QFN56) — WiFi/BLE, USB CDC nativo
- Flash: 16 MB
- PSRAM: 8 MB (OPI)
- MAC: 28:84:85:55:56:44
- Pantalla: IPS 1.83" ST7789P 240x284 SPI + tactil CST816D
- IMU: QMI8658 (I2C). Audio: ES8311 + ES7210 (mics duales)

## Archivo de respaldo
- `factory_full_16MB.bin` — volcado COMPLETO de la flash (offset 0x0, 16 MB)
- SHA-256: 6f188fb9d35ee793a3423934a4fa4e7c1fef9cc9dae76f9f177dabe854a6cdb3
- Capturado: 2026-09-10
- Integridad: `shasum -a 256 -c factory_full_16MB.bin.sha256`

## Como restaurar el firmware de fabrica (CUIDADO: sobreescribe la placa)
Conecta la placa y averigua el puerto (`ls /dev/cu.usbmodem*`). Luego:

    esptool --port /dev/cu.usbmodem1101 --baud 921600 write-flash 0x0 factory_full_16MB.bin

O usa el script:

    ./restore_factory.sh
    ./restore_factory.sh /dev/cu.usbmodem1101

Verificar despues:

    esptool --port /dev/cu.usbmodem1101 verify-flash 0x0 factory_full_16MB.bin
