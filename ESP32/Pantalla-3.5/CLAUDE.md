# TAC-1 · TactOS — Panel tactil 3.5" de Jose (bunker)

Tercer aparato de la familia (VNL-1 / StuntHub / **TAC-1**). Mismo ADN:
`ESP32/Perilla-1.46/CLAUDE.md` (reglas duras) y `StuntHub/INTERFAZ.md` mandan;
aqui el lineamiento esta hecho codigo en `firmware/TAC1/ui_theme.h`, adaptado a
un lienzo APAISADO de 480x320 y TACTIL. Es un hub de apps sobre un wallpaper:
se van agregando apps. Hoy: Inicio (hora, fecha, red), Clima (Open-Meteo),
Mercados (CoinGecko), Ajustes (hoja desde arriba: Wi-Fi, Brillo, Volumen,
Apagar, Reiniciar) y Red Wi-Fi. Brief para diseno en `docs/BRIEF-DISENO.md`.

## Navegacion
Carrusel circular Inicio <-> Clima <-> Mercados (deslizar lateral). Deslizar
hacia abajo abre Ajustes; hacia arriba o la flecha lo cierra. Wi-Fi vive
dentro de Ajustes y tambien en la pastilla del Inicio. Lo irreversible pide
segundo toque (ambar, 4 s). Reposo 3 min; el primer toque solo despierta.
Apagar = luz a cero y luego SLPIN al panel; cualquier toque lo revive.

## Modulos
- `display.cpp` panel + tactil + backlight. `net.cpp` WiFi/NVS/NTP/escaneo.
- `cloud.cpp` UN task en core 0 trae clima y mercados EN SECUENCIA (dos TLS a
  la vez en core 0 = watchdog, visto en StuntHub). `certs.h`: ISRG Root X1
  para Open-Meteo y **GTS Root R4 para CoinGecko** (desde 2026 firma con
  Google; con ISRG el handshake da -1. StuntHub tiene el mismo problema).
- `ajustes.cpp` NVS ("aj"): brillo (0 = auto). `ui.cpp` todas las pantallas;
  `ui_theme.h` los tokens y componentes (grupo iOS, fila, slider, tap).

## Placa: Guition JC3248W535 (el listado decia "JC4832W535")
ESP32-S3-WROOM-1 N16R8 (16MB flash quad, 8MB PSRAM octal), LCD 3.5" 320x480
AXS15231B por QSPI, tactil en el mismo chip por I2C (0x3B, SDA 4 / SCL 8),
backlight GPIO 1, USB-C nativo (aparece como `/dev/cu.usbmodem*`), TF y
altavoz sin usar aun. Pines en `board.h`. MAC `28:84:85:53:45:d8`.
Respaldo de fabrica: `backup/factory_16MB.bin` (+ `restore_factory.sh`).

## Build y flash (entorno AISLADO: no toca el core 2.0.17 de VNL1/StuntHub)
```
tools/build.sh          # compila
tools/build.sh flash    # compila y sube
tools/build.sh erase    # borra TODO el flash y sube (solo la primera vez)
tools/.venv/bin/python tools/monitor.py [seg]   # serial SIN tocar DTR/RTS
```
Core esp32 **3.3.11** + LVGL **8.3.11** viven en `tools/arduino-data` y
`tools/arduino-libs` via `tools/arduino-cli.yaml`. Sin librerias de pantalla:
el driver es propio sobre `esp_lcd` (viene en el core). `lv_conf.h` en
`tools/arduino-libs/libraries/`. Particion `app3M_fat9M_16MB` (OTA x2 + FAT).
Regenerar: wallpaper `tools/make_wallpaper.py` (venv con Pillow), fuente de la
hora `npx lv_font_conv` (ver `tac_reloj_96.c`: Montserrat 96, solo digitos).

## Como funciona la pantalla (display.cpp) — leer antes de tocar
- **No rota por hardware** (MADCTL MV no hace nada en QSPI) y **no acepta
  ventanas parciales** (ignora RASET; cada escritura empieza en la fila 0).
  Por eso: LVGL en `direct_mode` con dos cuadros completos en PSRAM, y un task
  en el core 0 que gira 90 grados y manda SIEMPRE el cuadro entero en trozos
  de 40 filas por DMA (dos buffers internos que se turnan). LVGL dibuja el
  cuadro N+1 en el core 1 mientras sale el N.
- `ROT_FLIP` en `board.h` elige entre los dos apaisados; pantalla y tactil
  giran juntos.
- El bus es `spi_master` a pelo, calcado de Arduino_GFX (lo probado en esta
  placa): opcode en la fase de comando (8 bits), registro en la de direccion
  (24), modo 0, **32 MHz**, CS a mano por GPIO. Registro = `0x02 <reg> 00` en
  una linea; pixeles = `0x32 3C 00` + datos en cuatro lineas, bajo UN solo CS.
  Con `esp_lcd_panel_io_spi` (comando y datos en transacciones separadas) el
  panel NO acepto nada: se quedo mostrando los iconos de fabrica que guarda en
  su GRAM. Costo tres flasheos descubrirlo.
- Se manda CASET **y RASET** antes de cada cuadro. Espressif salta RASET en
  QSPI; aqui sin el no entraba el cuadro.
- El `post_cb` del SPI salta TAMBIEN con los comandos cortos: solo cuentan las
  transacciones marcadas con `user` (pixeles). Contarlas todas subia el CS
  antes del ultimo trozo y el panel cortaba las ultimas 120 filas (franja
  con basura al lado).
- Arranque: SWRESET + el blob del fabricante para 320x480 (`axs_init.h`, tal
  cual lo trae Arduino_GFX) + INVOFF, MADCTL 0, COLMOD 0x55. La secuencia
  minima de ESPHome tambien pinto, pero el blob es lo que el fabricante
  pretende y arranca en frio.
- `LV_COLOR_16_SWAP = 1`: el panel quiere big-endian y asi LVGL ya lo dibuja.
- El log `fps` cuenta cuadros VOLCADOS: en reposo es 0 y esta bien (solo se
  vuelca cuando algo cambia).

## Reglas propias
- El tactil SI se usa: es un panel. Filas de 40px, objetivos de 44px minimo.
- Inicio es la unica pantalla con wallpaper (contenido inmersivo); toda
  pantalla de datos va sobre `COL_BG`.
- El primer toque con la pantalla dormida solo despierta.
- Credenciales: NVS (`net`), capturadas en pantalla. `secrets.h` solo siembra
  con la NVS vacia.
