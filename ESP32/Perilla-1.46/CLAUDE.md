# Perilla-1.46 (antes CROWN32) — Proyectos de perillas ESP32-S3 (CrowPanel 1.46")

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

Ago-2026: ya son DOS placas. TMEhub vive en la suya (TME) y StuntHub corre en
la segunda, ya con VinilOS integrado.

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
8. **Fondos de UI: OSCUROS** (`0x0B1020`). La regla decia lo contrario por la
   mura del panel en fondos oscuros; corregida ago-2026: la mura existe pero
   tras meses de uso no estorba, y un dashboard de bunker con fondo claro es una
   lampara en la cara de noche. Los dos firmwares ya usaban oscuro: la regla y
   el codigo se contradecian, que es peor que cualquiera de las dos opciones.
9. **Documentar todo gotcha nuevo en el CLAUDE.md del proyecto.**
10. **"No muevas cosas raras" significa NO muevas cosas raras.**

## Lineamiento de interfaz — donde vive
**La espec de UI de StuntHub es `StuntHub/INTERFAZ.md` y MANDA.** Su hermano de
VinilOS es `ESP32/Perilla-1.46/VinilOS/docs/interfaz.md`: mismo ADN, distinto contenido.
Hecho codigo en `StuntHub/ui_theme.h` (tokens + los cinco componentes); las apps
no escriben fuentes, alturas, radios ni hex.

Aqui vivio un rato una version resumida que se contradecia con INTERFAZ.md en
acento, tipografia y politica tactil. Se borro a proposito: dos fuentes de
verdad que no coinciden son peores que una sola, aunque sea imperfecta.

Lo minimo que hay que saber sin abrir el documento:
- Pantalla REDONDA: nada arriba de y=30 ni abajo de y=320; nunca `LEFT_MID`/`RIGHT_MID` contra la pantalla.
- **Solo perilla.** El tactil esta apagado: el toque solo despierta.
- Gestos: girar = mover, push = entrar/activar, mantener 900ms = atras (sube UN nivel).
- La portada de una app **nunca actua**: push entra.
- Todo menu es una lista vertical, mismo componente.
- Acento **ambar `0xFF7A10`**, uno solo. Verde/rojo/ambar son ESTADO, no decoracion.
- Jerarquia por opacidad, no por color. No existe un gris de texto.

## Estilo con Jose
Espanol, directo, sin verborrea. Explicar cambios en 1-3 lineas. Emojis con
moderacion. Riesgos se dicen — Jose decide. Sin acentos en strings de
firmware (la fuente Montserrat de LVGL no los trae).
