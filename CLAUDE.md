# CROWN32 — Proyectos de perillas ESP32-S3 (CrowPanel 1.46")

Indice del workspace. Dos firmwares INDEPENDIENTES para el mismo hardware,
cada uno con su CLAUDE.md completo — **lee el del proyecto que vayas a tocar**:

| Carpeta | Que es | CLAUDE.md |
|---|---|---|
| `StuntHub/` | Perilla PERSONAL de Jose (bunker, 7 apps) | `StuntHub/CLAUDE.md` |
| `TMEhub/`   | Perilla INDUSTRIAL (empresa TME, reseteos de camiones) | `TMEhub/CLAUDE.md` |
| `build_env/` | Entorno de build reproducible (lv_conf.h + versiones pinneadas) | — |
| `backup/` | Respaldo de fabrica (16MB) + binario StuntHub v2 listo para flashear | — |
| `docs/` | Planes historicos, tests de IR viejos, util de serial | — |

## Hardware (comun a ambos)
Elecrow CrowPanel ESP32-S3 1.46" round: ESP32-S3R8, 16MB flash, 8MB PSRAM,
IPS 360x360 (panel ST77961), touch cst816t (GPIO 13/5), rotary encoder
(45/42/41), backlight GPIO46 PWM ch0. `board.h` identico en ambos proyectos.

HOY (jun-2026) hay UNA placa fisica corriendo TMEhub. Cuando llegue la
CrowPanel #2: TMEhub queda en TME y se restaura StuntHub
(`backup/stunthub_v2_bin/RESTORE.md` o recompilar).

## Build (igual para ambos — ver build_env/README.md ANTES de instalar nada)
```bash
cd <StuntHub|TMEhub>
arduino-cli compile --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc" .
# flashear: agregar --upload -p $(ls /dev/cu.usbmodem* | head -1)
```
⚠️ PIN DE VERSIONES: core esp32 **2.0.17**, LovyanGFX **1.2.7** (1.2.21 deja la
pantalla en blanco), lvgl 8.3.6, lv_conf.h custom de `build_env/` copiado a
`~/Documents/Arduino/libraries/`.

## ⚠️ REGLAS DURAS (contrato con cualquier sesion)
1. **NUNCA flashear sin confirmacion explicita de Jose en ese mensaje.**
2. **NUNCA refactorizar por gusto** — solo lo pedido.
3. **NUNCA cambiar versiones de librerias** (pin de arriba).
4. **NUNCA tocar agentes Windows / cron del Mac Mini sin avisar.**
5. **NUNCA cambiar IPs/MACs sin verificar en vivo (curl/ping) primero.**
6. **NUNCA borrar respaldos ni signal files "para limpiar".**
7. **NUNCA leer serial del S3 toggleando RTS/DTR** (lo deja en modo DOWNLOAD,
   pantalla negra; revivir = power-cycle). Abrir con dtr=False rts=False ANTES de open().
8. **Fondos de UI: colores brillantes solidos** (el panel tiene mura visible
   en fondos oscuros).
9. **Documentar todo gotcha nuevo en el CLAUDE.md del proyecto.**
10. **"No muevas cosas raras" significa NO muevas cosas raras.**

## Estilo con Jose
Espanol, directo, sin verborrea. Explicar cambios en 1-3 lineas. Emojis con
moderacion. Riesgos se dicen — Jose decide. Sin acentos en strings de
firmware (la fuente Montserrat de LVGL no los trae).
