# Respaldo de fábrica — Elecrow CrowPanel ESP32-S3 1.46" redonda

## Placa
- Chip: ESP32-S3 (QFN56) rev v0.2 — módulo ESP32-S3R8
- Flash: 16 MB (GigaDevice GD25Q128, quad SPI, 3.3V)
- PSRAM: 8 MB embebida
- MAC: 14:c1:9f:d7:dd:38
- Pantalla: IPS redonda 360x360

## Archivo de respaldo
- `factory_full_16MB.bin` — volcado COMPLETO de la flash (offset 0x0, 16 MB)
- SHA-256: f83859492b211d6a1a3e95acb14a409d971c4bc9dab04d0b307a57badc8fce4b
- Capturado: 2026-06-06 con esptool 5.3.0
- Integridad: verificada con `esptool verify-flash` (digest matched)

## Cómo restaurar el firmware de fábrica (CUIDADO: sobreescribe la placa)
Conecta la placa y averigua el puerto (`ls /dev/cu.usbmodem*`). Luego:

    esptool --port /dev/cu.usbmodem11401 --baud 921600 write-flash 0x0 factory_full_16MB.bin

Verificar después:

    esptool --port /dev/cu.usbmodem11401 verify-flash 0x0 factory_full_16MB.bin
