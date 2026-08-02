# Stuntech VNL-1

Reproductor de vinilos digital. Caja de acrílico transparente, pantalla redonda,
interacción exclusivamente por perilla. Proyecto independiente: no comparte código
ni carpetas con StuntHub ni TMEhub.

```
VNL1/
├── firmware/VNL1/     sketch de Arduino (VNL1.ino + pins.h + knob.h)
├── sd/                estructura de la microSD del DFPlayer (01/ 02/ 03/)
├── backup/            respaldo de fábrica de esta placa
└── docs/
```

## Placa

Elecrow CrowPanel 1.46" HMI ESP32-S3 Rotary Display.
Esta unidad: MAC `14:c1:9f:d7:e0:60`, ESP32-S3 rev v0.2, 8MB PSRAM, 16MB flash.
Venía con el demo de fábrica de Elecrow (Oct 4 2023), respaldado en
`backup/factory_full_16MB.bin` — SHA-256 `f5d6d694…a6f89208`.

Restaurar el demo de fábrica:

```
esptool --port <PUERTO> write-flash 0x0 backup/factory_full_16MB.bin
```

### Pines

| Función | GPIO |
|---|---|
| Encoder A / B / push | 45 / 42 / 41 |
| **Corriente de la pantalla** | **1 y 2, ambos en HIGH** |
| Backlight (PWM para el fade) | 46 |
| Anillo RGB: DIN / PWR | 48 / 17 — 8 LEDs |
| LED de power (activo en LOW) | 40 |
| Pantalla SPI: SCLK / MOSI / CS / DC / RST | 10 / 11 / 9 / 3 / 14 |
| Touch CST816T (bus I2C 0): SDA / SCL / INT / RST | 6 / 7 / 5 / 13 |
| I2C libre del header (bus 1) | 38 / 39 |
| DFPlayer: TX / RX del ESP32 | 4 / 12 |

Tres cosas que ninguna guía documenta y que cuestan un ciclo de flasheo cada una:

- **GPIO 1 y GPIO 2 deben estar en HIGH o la pantalla no recibe corriente.** El SPI
  puede estar perfecto y la pantalla queda negra. Sale solo del código de fábrica.
- El táctil vive en el bus I2C 0 remapeado a **6/7**. El 38/39 es un bus
  **distinto** (`TwoWire(1)`), el del header de expansión. Son dos buses, no uno.
- `memory_height` del panel va en **360**. El default de LovyanGFX para el
  ST77961 es 390 y la imagen queda corrida.

El DFPlayer va en UART1 sobre IO4/IO12, **no** en el par TX/RX del header: ese es
UART0 y se pelea con el monitor serial, único canal de depuración que hay.

## Entorno

- Core `esp32:esp32` **2.0.17**. No subir a 3.x: rompe `ledcSetup`, que se usa para
  el fade del backlight, y no aporta nada aquí.
- LovyanGFX **1.2.7**. La 1.2.21 deja la pantalla en blanco en esta placa.
- LVGL 8.3 con el `lv_conf.h` que ya vive en `~/Documents/Arduino/libraries/`.
- Panel LovyanGFX = **ST77961**, no GC9A01 (el wiki de Elecrow se equivoca).
- Pendiente instalar: `DFRobotDFPlayerMini`.

Compilar y flashear:

```
cd firmware/VNL1
arduino-cli compile --upload -p <PUERTO> \
  --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc" .
```

El puerto cambia según el conector físico (`ls /dev/cu.usbmodem*`).
En el IDE, equivale a: ESP32S3 Dev Module, OPI PSRAM, 16MB flash, huge_app,
USB CDC On Boot **Enabled** — sin eso el monitor serial no muestra nada.


## Agregar música o un disco nuevo

Todo vive en la propia microSD, en la carpeta `_origen`. No hay carpeta local
ni copias duplicadas: un solo lugar.

**1.** Mete la microSD al Mac.

**2.** Abre `_origen` y arrastra. Para música nueva en un disco que ya existe,
la sueltas dentro de su carpeta. Para un disco nuevo, creas la carpeta con el
número que le toque y el nombre que quieras:

```
_origen/05 Mario Kart/
        ├── lo que sea.mp3
        ├── otra cancion.m4a
        └── cover.jpg          ← opcional
```

El número decide en qué carpeta de la tarjeta cae y el nombre es el que sale en
pantalla. El `cover.jpg` se convierte en la etiqueta del vinilo, y de ahí sale
también el color del anillo de LEDs.

**3.** Conecta la perilla por USB.

**4.** Doble clic en **`Actualizar VinilOS.command`**.

Eso convierte la música, la copia en el orden que el DFPlayer entiende, procesa
las carátulas, regenera el manifiesto y flashea la perilla. Una sola acción.

**5.** Saca la tarjeta, ponla en el módulo y listo.

### Por qué hay un paso de proceso y no basta con copiar

El DFPlayer solo lee carpetas numéricas (`/01`, `/02`) con archivos llamados
`001.mp3`, y los ordena por la tabla FAT, no por el nombre. Además la pantalla
necesita saber cosas que el módulo nunca reporta —cuántas canciones hay, cuánto
duran, de qué color es cada disco— y eso viaja compilado en el firmware. El
script genera las dos mitades al mismo tiempo, y por eso la tarjeta y la
pantalla siempre dicen lo mismo.

Si arrastras música directo a `/01`, no queda convertida ni renombrada, y el
manifiesto sigue creyendo que ese disco está como estaba.

### Por qué no se puede por WiFi

El ESP32 **no tiene ningún acceso a la microSD**: la tarjeta está cableada solo
al DFPlayer, y ese módulo no acepta escritura por serial. Es la consecuencia
directa de la decisión de arranque del proyecto — el CrowPanel no expone
suficientes pines para manejar la SD por su cuenta. Cambiar música implica sacar
la tarjeta, y eso no es cuestión de programarlo.

## Interacción (decidida 2026-07-29)

| Gesto | Selector | Reproducción |
|---|---|---|
| Girar | cambia de disco | **volumen** |
| Push | entra / arranca el álbum barajado | pausa / reanuda |
| Doble push | — | siguiente canción |
| Push 3s | — | la aguja se levanta y vuelve a la biblioteca |

Reglas que sostienen el diseño:

- **La pantalla es el indicador de modo.** Con una sola perilla, el giro no puede
  significar dos cosas en la misma pantalla sin que el usuario quede a ciegas. Por
  eso el push profundo no cambia el modo del giro en su lugar: cambia de pantalla,
  y el giro significa lo que esa pantalla dice.
- **Al volver a la biblioteca la música sigue.** El anillo de LEDs sigue respirando
  con el color del álbum que suena, así se sabe cuál es sin leer nada.
- **Push sobre el disco que ya suena = regresar a la reproducción**, sin reiniciar.
  Salir de la biblioteca por error no cuesta nada.
- El push corto llega con **260ms de retardo** (`KNOB_DOUBLE_MS`): es el precio de
  tener doble push, no hay forma de evitarlo con un solo botón. Se compensa con
  feedback visual en `KNOB_DOWN`, que se emite al instante de apretar.

## Etapas

1. **Encoder** — ✅ verificada en placa. Horario = CW, 4 sub-pasos por muesca,
   sin eventos fantasma al presionar.
2. **Pantalla** — ✅ flasheada. LVGL + vinilo girando a 26 fps estables, táctil
   inicializando sin error. Falta la revisión visual.
3. **DFPlayer** — ⏳ código escrito y compilando, sin hardware para probar.
   `player.cpp` ya trae el throttling de 60ms, el colapso de comandos de volumen
   y el barajado Fisher-Yates. Activar con `#define STAGE3_AUDIO 1`.
4. **Integración** — ✅ flasheada. Los tres estados con sus transiciones,
   overlay de volumen, puntos de pista, inercia del disco. Verificada con la
   secuencia automática de `#define SELFTEST 1`, que inyecta todos los gestos.
5. **LEDs** — ✅ incluida en la etapa 4: el anillo toma el color del álbum que
   **suena**, no del que estás hojeando, y respira solo mientras hay música.
6. **Pulido** — pendiente: auto-apagado a 20s con fade (el backlight y
   `knob::idleMs()` ya están listos), brazo/aguja dibujado, feedback de
   `KNOB_DOWN` más allá del bump de la etiqueta.

### Sobre el progreso de la canción

El manifiesto tiene la duración del **álbum**, no de cada pista, y el DFPlayer no
reporta metadata. Dividir el total entre 10 daría una barra que termina antes o
después que la canción. Así que la reproducción muestra **un punto por canción**
en el canto del disco (eso sí se sabe con certeza) y el tiempo cuenta hacia
arriba sin total falso. Cuando los MP3 estén en `sd/`, se saca la duración exacta
de cada archivo con `ffprobe` y se regenera `albums.h` — ahí la barra real se
vuelve posible.

## Runbook para cuando llegue el DFPlayer

1. **Cableado.** VCC y GND del DFPlayer **en estrella** desde el punto de entrada
   del USB, no en serie a través del CrowPanel. Resistencia de 1kΩ en la línea
   IO4 → RX del módulo. Capacitor de 1000µF entre VCC y GND del DFPlayer, o los
   picos de audio le tiran el voltaje y se resetea. Bocina en SPK1/SPK2.
2. **microSD.** FAT32, formateada en la Mac con `MS-DOS (FAT)`. Poner los audios
   (cualquier formato) en `sd/src/01`, `sd/src/02`, `sd/src/03` y correr:

   ```
   python3 sd/prepare_sd.py --out /Volumes/<TARJETA> --clean-card
   ```

   Convierte a MP3 192kbps CBR 44.1kHz, nombra `001.mp3`…, copia en orden, y
   genera `firmware/VNL1/albums_gen.h` con la duración **exacta de cada pista**
   sacada con ffprobe. Dos trampas que el script cubre: el DFPlayer ordena por
   tabla FAT y no por nombre, y cuenta la metadata invisible de macOS
   (`.DS_Store`, `._001.mp3`, `.Spotlight-V100`) como pistas — de ahí
   `--clean-card`.
3. **Firmware.** Poner `#define STAGE3_AUDIO 1` en `VNL1.ino`, compilar, flashear.
   El push del selector arranca el álbum barajado.
4. Si el módulo no contesta, el serial lo dice y la pantalla sigue funcionando:
   revisar VCC, el 1kΩ y que la SD esté en FAT32.

## Notas

- La fuente Montserrat de LVGL no trae acentos ni `…`/`—`. Textos en pantalla en
  ASCII, o se embebe una SF Pro convertida.
- El DFPlayer no reporta duración ni metadata: todo sale del manifiesto.
- microSD máximo 32GB, FAT32 obligatorio. No hay forma de escribirla desde el
  ESP32; cambiar música implica sacar la tarjeta.
