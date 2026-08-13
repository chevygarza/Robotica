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
8. **Fondos de UI: colores brillantes solidos** (el panel tiene mura visible
   en fondos oscuros).
9. **Documentar todo gotcha nuevo en el CLAUDE.md del proyecto.**
10. **"No muevas cosas raras" significa NO muevas cosas raras.**

## CROWN Interface Guidelines (lineamiento de UI — comun a ambos firmwares)
El "HIG" del CROWN. Como iOS/Material pero para una perilla redonda de 360x360.
Cualquier pantalla nueva DEBE respetar esto. Si rompes una regla, documenta por que.

### 1. Safe area (pantalla REDONDA — las esquinas no existen)
- Contenido legible solo dentro del circulo interior **~300px** (radio 150 menos
  margen). Lo que toque el borde se corta o se curva ilegible.
- **Nada de texto critico arriba de y=40 ni abajo de y=320.** La franja inferior
  curva fue donde vivia el hint "girar/push/manten" — ilegible, se quito.
- Titulo de app: `LV_ALIGN_TOP_MID, 0, ~28..40`. Cajas/listas: centradas
  (`LV_ALIGN_CENTER`) con tamano <= 280 de ancho y <= ~200 de alto.
- Hints/estados efimeros: `TOP_MID` debajo del titulo (y~52), NUNCA en el borde inferior.

### 2. Input model — PERILLA PRIMERO (hibrido documentado)
La perilla es la regla. El tacto es un atajo permitido solo donde se documente.
- **Gestos globales (iguales en TODA la UI, son convencion, no se rotulan en pantalla):**
  - `girar` = mover seleccion / cambiar valor
  - `push` (corto) = activar / OK / entrar
  - `manten` (largo >600ms) = atras / salir a home
- El usuario YA conoce los gestos: no se ponen instrucciones de gesto en pantalla.
- **Tacto permitido (atajos documentados), no obligatorio:**
  - Hue: favoritos tactiles (botones grandes de escena) — atajo directo.
  - Regla del atajo tactil: solo para acciones de 1 toque, target >= 44px,
    dentro del safe area, y SIEMPRE con equivalente por perilla. Nunca como
    unica via para una accion.
- Todo flujo debe completarse 100% solo con la perilla.

### 3. Colores autorizados (paleta unica — definida en app_ui.cpp)
No inventar colores. Usar SIEMPRE estos `#define`:
| Token | Hex | Uso |
|---|---|---|
| `COL_BG` | `0x0B1020` | fondo base |
| `COL_CARD` | `0x161C2E` | tarjetas/cajas |
| `COL_TXT` | `0xFFFFFF` | texto principal |
| `COL_SUB` | `0x8A93A6` | texto secundario / hints |
| `COL_ACCENT` | `0x4EA8FF` | seleccion / foco (highlight de perilla) |
| `COL_WARM` | `0xFFB454` | en curso / pendiente ("Enviando...") |
| `COL_OK` | `0x3DD68C` | exito confirmado (verde) |
| `COL_BAD` | `0xFF5B6E` | error / fallo (rojo) |
| `COL_X` | `0x1D9BF0` | marca X/Twitter (solo esa app) |
- Semaforo de estado: **azul**=foco, **ambar**=en curso, **verde**=ok, **rojo**=fallo, **gris**=bloqueado/inactivo.
- Regla dura #8: los FONDOS de pantalla van en color brillante solido (mura del panel).

### 4. Tipografia
- Fuente unica: **Montserrat** (LVGL). Tamanos en uso: 12 (hint), 14 (sub),
  16-20 (cuerpo), 28+ (titulos/valores grandes).
- **Sin acentos, sin n-tilde, sin simbolos … — — “ ”** en strings de firmware:
  la fuente no los trae (salen como tofu). "girar", "atras", "No respondio".
- Iconos: solo los `LV_SYMBOL_*` de LVGL (ej. `LV_SYMBOL_OK` para el check de exito).

### 5. Confirmaciones (feedback de toda accion remota)
Patron "item + resultado real" (PC Gamer es la referencia):
1. Al activar: el item tomado se pinta `COL_WARM` + hint "Enviando..." (en curso).
2. Cuando el POST responde: verde `COL_OK` + `LV_SYMBOL_OK " Listo"` si llego;
   rojo `COL_BAD` + "No respondio" si fallo. El color refleja si el comando
   REALMENTE llego al destino, no solo que se mando.
3. El estado efimero se limpia tras ~1.8s (ok) / ~2.5s (fallo) y se restaura el
   highlight normal. Nunca dejar un item pegado en verde/rojo.

### 6. Formas / componentes
- Cajas y botones con esquinas redondeadas (radius ~12-16), acordes al marco circular.
- Listas navegables: el item con foco lleva fondo/borde `COL_ACCENT`; los demas planos.
- Items bloqueados (ej. modos cuando la PC esta apagada): texto `COL_SUB`, sin fondo.

## Estilo con Jose
Espanol, directo, sin verborrea. Explicar cambios en 1-3 lineas. Emojis con
moderacion. Riesgos se dicen — Jose decide. Sin acentos en strings de
firmware (la fuente Montserrat de LVGL no los trae).
