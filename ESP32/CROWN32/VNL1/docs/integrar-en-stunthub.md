# Prompt: meter VinilOS dentro de StuntHub

Copia todo lo que sigue como primer mensaje de una sesión nueva abierta en
`CROWN32/StuntHub/`.

---

Quiero agregar **música** a StuntHub: meter VinilOS —mi reproductor de vinilos
digital— como una app más de la perilla. Esta unidad va a ser la versión **Max**:
dos bocinas, doble batería y corriente directa.

Los dos proyectos son míos y corren en la **misma placa**:

- StuntHub: `CROWN32/StuntHub/` (lee su `CLAUDE.md` y el de `CROWN32/`)
- VinilOS: `~/Desktop/VNL1/` (lee su `README.md` y `docs/producto.md`)

**Antes de escribir código, lee los cuatro documentos.** Están escritos para que
no tengas que redescubrir nada, y las dos secciones que más te van a servir son
"Energía" y "Lo que costó caro" del README de VNL1.

## Lo que ya se verificó — no lo vuelvas a investigar

**1. Es la misma placa, hasta el `board.h`.** CrowPanel 1.46" redonda,
ESP32-S3R8, panel ST77961, encoder en 45/42/41, backlight GPIO46. Las mismas
versiones pinneadas: core 2.0.17, LovyanGFX 1.2.7, LVGL 8.3.6.

Consecuencia: `display.cpp` y `knob.cpp` de VinilOS son **duplicados** de lo que
StuntHub ya tiene en `board.h` y su `.ino`. No los traigas. VinilOS se monta
sobre la infraestructura de StuntHub, no al revés.

**2. El flash es el problema estructural, y va primero.**

| | Tamaño |
|---|---|
| StuntHub hoy (`backup/stunthub_v2_bin/StuntHub.ino.bin`) | **2.21 MB** |
| Partición `huge_app` que usa hoy | 3 MB |
| Costo incremental de VinilOS | **~1.6 MB** |

Ese 1.6MB es casi todo **carátulas** (~220KB por álbum) más la fuente de reloj de
78pt (76KB); LVGL y LovyanGFX ya están y no se pagan dos veces.

2.21 + 1.6 = **3.8 MB, no cabe en 3 MB.** Hay que pasar a la partición de 8MB.
VNL1 ya tiene el archivo hecho: `firmware/VNL1/vnl1_8M.csv`, y se usa así:

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc" \
  --build-property build.partitions=vnl1_8M \
  --build-property upload.maximum_size=8388608 .
```

**Haz esto primero y compila StuntHub tal como está**, antes de tocar una sola
línea de UI. Si la partición no funciona, todo lo demás sobra. Ojo: cambiar de
partición **borra la NVS**, así que StuntHub pierde lo que tenga guardado ahí —
verifica qué guarda antes.

**3. Tira `weather.cpp` y `netclock.cpp` de VinilOS. StuntHub ya los tiene.**

StuntHub trae clima de Open-Meteo (app 0) y NTP en `app_net.cpp`. Traer los de
VinilOS duplicaría el servicio **y** agregaría un quinto task TLS en el core 0.
El `CLAUDE.md` de StuntHub lo advierte: *"3 TLS simultáneos en core 0 = task_wdt
+ boot loop; si agregas app de red nueva, dale su slot"*.

VinilOS no necesita red propia. Que lea la hora y el clima de StuntHub.

**4. El giro de la perilla choca, y ya hay precedente para resolverlo.**

En StuntHub girar = cambiar de app. En VinilOS girar = volumen si hay música, o
disco si estás en la biblioteca. **Hue y PC Gamer ya secuestran el giro** cuando
estás dentro de sus sub-vistas, con un `return` temprano en `ui_nav()`
(`app_ui.cpp:840`). Copia ese patrón; no inventes uno nuevo.

Cómo debe quedar, y por qué:

| Dónde | Girar | Push | Mantener |
|---|---|---|---|
| Portada de la app Música | **cambia de app** | entra a la biblioteca | — |
| Biblioteca de discos | disco | reproduce | sale a la portada |
| Reproduciendo | **volumen** | pausa | vuelve a la biblioteca |

La regla que sostiene esto está en el README de VNL1: **la pantalla es el
indicador de modo**. Con una sola perilla el giro no puede significar dos cosas
en la misma pantalla sin crear un modo invisible.

**5. Hay un choque de estética que tienes que decidir explícitamente.**

La regla 8 de `CROWN32/CLAUDE.md` dice *"fondos de UI: colores brillantes
sólidos, el panel tiene mura visible en fondos oscuros"*. VinilOS es negro a
propósito: fondo negro, tipografía sobria, la carátula llenando el disco.

No lo resuelvas por tu cuenta. **Pregúntame** y documenta la decisión. Mi
inclinación: dentro de la app de música la carátula ocupa casi toda la pantalla,
así que la mura casi no se ve y el negro se justifica — pero es una regla escrita
y romperla se documenta, no se ignora.

**6. El sueño de pantalla tiene dos reglas distintas y una condición dura.**

StuntHub duerme a los 30s fijos. VinilOS tiene reposo configurable con reloj o
apagado. **La condición que no se negocia: dormir la pantalla NO detiene la
música.**

StuntHub ya tiene el gancho exacto para esto: `ui_screen_off()` / `ui_screen_on()`
existen para soltar y retomar trabajo visual (hoy el GIF). El disco girando de
VinilOS debe soltarse igual — es lo más caro de dibujar que hay en el firmware.

**7. El riel de 5V ya está encendido.** StuntHub pone `PIN_SCR_PWR_B` (GPIO2) en
HIGH para la pantalla, y ése es el mismo riel que alimenta el conector UART donde
va el DFPlayer. Un problema menos, pero **no lo asumas: verifícalo en el código**.

## Hardware que hay que agregar

Está todo en el README de VNL1, sección del módulo DFPlayer:

- DFPlayer Mini al **conector UART de 4 hilos** (GPIO43 TX / GPIO44 RX). No pelea
  con el monitor serial: con USB CDC, `Serial` es el USB nativo del S3.
- **Resistencia de 1kΩ solo en el cable blanco** (TX placa → RX módulo), y la
  unión aislada.
- **Capacitor de 1000µF entre VCC y GND** del módulo, desde el primer día.
- `isACK=false` al iniciar la librería. **No es negociable** con el clon MH2024K:
  con `true`, `sendStack()` gira para siempre.

Para la Max, además: PAM8403 desde `DAC_L`/`DAC_R`, alimentado **en estrella
desde la entrada**, nunca colgado del riel de la placa. Y dos cosas de firmware
que ya están documentadas en `docs/producto.md` de VNL1: `PLAYER_VOL_MAX` tiene
otro valor con amplificador externo, y conviene un GPIO al pin `SHDN` para matar
el siseo en silencio y en reposo.

## Reglas del workspace que aplican

Están en `CROWN32/CLAUDE.md` y son contrato:

1. **Nunca flashees sin que yo lo confirme en ese mensaje.**
2. Nunca refactorices por gusto — solo lo pedido.
3. Nunca cambies versiones de librerías.
4. Nunca leas el serial toggleando RTS/DTR (deja el S3 en modo DOWNLOAD). Abrir
   con `dtr=False`, `rts=False` **antes** de `open()`.
5. Documenta cada gotcha nuevo en el `CLAUDE.md` del proyecto.

## Cómo quiero que lo hagas

**Por etapas, cada una compilando y verificable.** No me entregues la integración
completa de un jalón.

1. **Partición de 8MB.** Cambiar el build, compilar StuntHub sin tocar nada más,
   confirmar que cabe y que arranca. Reportar cuánto espacio queda.
2. **Traer el motor de audio.** `player.cpp/h` de VinilOS y nada más. Sin UI: que
   suene un archivo con un comando de prueba. Aquí se valida el cableado.
3. **La app en la perilla.** App nueva en `app_ui.cpp` con su portada, respetando
   el patrón de captura del giro de Hue/PC Gamer.
4. **La biblioteca y el disco girando.** `vinyl.cpp` y las carátulas. Aquí es
   donde el flash puede apretar: reporta el tamaño en cada paso.
5. **Reposo y sueño.** Unificar las dos reglas y garantizar que la música
   sobreviva a la pantalla dormida.

**Antes de empezar, dime qué encontraste que contradiga lo que te acabo de
decir.** Estos números salieron de una lectura, no de compilar; si al compilar
StuntHub el tamaño no es 2.21MB, quiero saberlo antes de que sigas.

Y una cosa sobre cómo trabajo: el serial reporta el estado del firmware, **no lo
que se ve en la pantalla**. El bus del panel es de una sola dirección. Si yo te
digo que veo algo distinto a lo que reporta el aparato, tengo razón yo — ya nos
costó horas una vez.
