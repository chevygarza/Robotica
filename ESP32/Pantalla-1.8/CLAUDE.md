# Pantalla-1.8 — Pantalla 1.8" (ESP32)

Cuarto aparato de la familia (VinilOS / StuntHub / TAC-1 / **este**).
Estado: recien conectada, SIN codigo. Este archivo es el punto de partida.

Antes de tocar nada lee `../Perilla-1.46/CLAUDE.md` (reglas duras que heredan
todos los ESP32) y `../Pantalla-3.5/CLAUDE.md` (el aparato mas parecido: hub de
apps sobre wallpaper, entorno de build aislado en `tools/`).

## Pendiente de llenar (primera sesion con la placa)
- [ ] Modelo exacto de la placa y del panel (driver, resolucion, bus SPI/QSPI)
- [ ] Chip: ESP32 clasico / S3 / C3, flash y PSRAM
- [ ] Pines: LCD, backlight, tactil (si tiene), botones/encoder
- [ ] Puerto USB que aparece en la Mac (`ls /dev/cu.*`)
- [ ] Respaldo de fabrica en `backup/` ANTES de flashear nada
- [ ] Version del core esp32 y librerias (pinnearlas aqui)

## Estructura propuesta (igual que Pantalla-3.5)
```
Pantalla-1.8/
├── CLAUDE.md          este archivo
├── firmware/<NOMBRE>/ el sketch (carpeta = nombre del .ino)
├── tools/             build.sh + arduino-cli.yaml aislados (toolchain fuera de git)
├── backup/            respaldo de fabrica (.bin fuera de git)
└── docs/
```
