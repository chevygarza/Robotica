# Pantalla-1.8 — Waveshare ESP32-S3-Touch-AMOLED-1.8

Cuarto aparato de la familia (VinilOS / StuntHub / TAC-1 / **este**).
Antes de tocar nada lee `../Perilla-1.46/CLAUDE.md` (reglas duras) y
`../Pantalla-3.5/CLAUDE.md` (estructura parecida).

## ⚠️ Identificacion (corregida 2026-09-10)
La placa es la **ESP32-S3-Touch-AMOLED-1.8**, NO la Touch-LCD-1.83. Se
confirmo con el log del firmware de fabrica (`ESP32-S3-Touch-AMOLED-1.8:
CO5300 panel initialized`, 368x448). Un dia entero de "pantalla negra" fue por
hablarle a un ST7789 que no existe. Repo oficial:
https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8 (usar `examples/arduino-v2`).

## Hardware
- Chip: ESP32-S3R8 rev v0.2, 16 MB flash QIO, 8 MB PSRAM OPI, USB-Serial/JTAG
- MAC: `28:84:85:55:56:44`. Puerto Mac: `/dev/cu.usbmodem1101` (sufijo puede cambiar)
- Panel: **AMOLED 1.8" 368x448, controlador CO5300, bus QSPI**
- Tactil: CST816S (fabrica) / FT3168 segun demos, I2C 15/14, INT 21
- IMU: QMI8658 6 ejes (acel + giro) I2C `0x6B` — OK en vivo
- Audio: **ES8311** (codec con mic integrado + PA GPIO46). NO hay ES7210.
- PMU: AXP2101 `0x34`. RTC PCF85063. microSD por SDMMC (CLK 2, CMD 1, D0 3)
- **Expansor I2C XCA9554/TCA9534 `0x20`**: P0/P1/P2 = power/reset del AMOLED.
  Sin la secuencia (bajo → 20 ms → alto) el panel no enciende.

### Pines (oficial `arduino-v2/libraries/Mylibrary/pin_config.h`)
```
QSPI: SDIO0 4, SDIO1 5, SDIO2 6, SDIO3 7, SCLK 11, CS 12   (sin RST ni BL por GPIO)
I2C SDA 15 / SCL 14, TP_INT 21
I2S MCLK 16 / BCLK 9 / LRCK 45 / DIN 10 / DOUT 8 / PA 46
```

## Backup de fabrica
- `backup/factory_full_16MB.bin` (16 MB, fuera de git)
- SHA-256: `6f188fb9d35ee793a3423934a4fa4e7c1fef9cc9dae76f9f177dabe854a6cdb3`
- Restaurar: `backup/restore_factory.sh` (probado 2026-09-10: enciende con UI esp-brookesia)

## Build / flash (entorno AISLADO, no toca el core 2.0.17 global)
```
tools/build.sh [Sketch]          # compila (default GrokBot)
tools/build.sh [Sketch] flash    # sube — SOLO con OK explicito de Jose
tools/build.sh [Sketch] monitor  # sube y lee serial 8 s
tools/.venv/bin/python tools/monitor.py /dev/cu.usbmodemXXX [seg]   # serial SIN DTR/RTS
```
- Core esp32 **3.3.11** (Waveshare exige >=3.0.5). `tools/arduino-data` y
  `tools/downloads` son symlinks al de `../Pantalla-3.5/tools/`; en una Mac
  nueva ver el encabezado de `tools/build.sh`.
- Librerias en git (`tools/arduino-libs/libraries/`): GFX_Library_for_Arduino
  **1.6.4** (bundle Waveshare, `Arduino_CO5300` + `Arduino_ESP32QSPI`),
  Adafruit_XCA9554, Adafruit_BusIO, Mylibrary.
- FQBN: `FlashMode=qio,FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB`

## Firmware
- `firmware/GrokBot`: cara animada. Dibuja en `Arduino_Canvas` (PSRAM) y hace
  `flush()` por cuadro (AMOLED sin parpadeo). Geometria pensada a 240x284 y
  escalada con `SC=1.5` en `face.cpp`. **Validado en vivo 2026-09-10.**
- `firmware/HelloWorld`: 01_HelloWorld oficial + secuencia del expansor. Prueba base.

## Gotchas
1. Serial: **nunca** abrir con DTR/RTS (`dtr=False, rts=False` antes de `open()`).
   Abrir el puerto en macOS resetea el chip igual (`USB_UART_CHIP_RESET`), es normal.
2. No escribir `0x90=0xFF` en el AXP2101.
3. `std::lerp` existe en GCC 14: no definir `lerp()` propio (usar `lerpf`).
4. Flashear no apaga el panel si ya estaba encendido: una pantalla con
   contenido viejo NO prueba que el firmware nuevo dibuje.

## Pendiente
- [x] Mic ES8311 por I2S en tarea core 0 (`mic.cpp`, driver oficial `es8311.c`). Validado en vivo.
- [ ] Leer giroscopio del QMI8658 (hoy solo acelerometro)
- [ ] Diseño "Bob Aerogro" que quiere Jose (falta referencia visual)
- [ ] Tactil (CST816/FT3168) si hace falta
