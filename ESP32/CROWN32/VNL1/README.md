# Stuntech VNL-1 · VinilOS

Reproductor de música de escritorio con estética de tornamesa. Caja de acrílico,
pantalla redonda, **interacción exclusivamente por perilla**. Proyecto
independiente: no comparte código ni carpetas con StuntHub ni TMEhub.

```
VNL1/
├── Actualizar VinilOS.command   doble clic: sincroniza la SD y flashea
├── firmware/VNL1/               el sketch completo
├── sd/prepare_sd.py             el motor del doble clic
├── sd/.venv/                    entorno con Pillow (para las carátulas)
├── backup/                      respaldo de fábrica de esta placa
└── docs/
```

Las carpetas `sd/01`, `sd/02`, `sd/03`, `sd/src` y `sd/src_test` son restos de
cuando el origen de la música vivía en el Mac. Ya no participan en nada.

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
| **Corriente del panel** | **1 en HIGH** |
| **5V de los conectores UART e I2C** | **2 en HIGH** |
| Backlight (PWM para el fade) | 46 |
| Anillo RGB: DIN / PWR | 48 / 17 — 8 LEDs |
| LED de power (activo en LOW) | 40 |
| Pantalla SPI: SCLK / MOSI / CS / DC / RST | 10 / 11 / 9 / 3 / 14 |
| Touch CST816T (bus I2C 0): SDA / SCL / INT / RST | 6 / 7 / 5 / 13 |
| I2C libre del header (bus 1) | 38 / 39 |
| DFPlayer: TX / RX del ESP32 | 43 / 44 (conector UART) |

### Cuatro cosas que ninguna guía documenta

Cada una costó al menos un ciclo de depuración, y una costó una sesión entera.

**`GPIO2` es el interruptor del riel de 5V de los conectores.** Verificado en el
esquemático oficial: `GPIO2 → R23 → Q7 (S9013) → compuerta de Q4 (PMOS) → OUT_5V`.
Cualquier sketch que hable con un periférico de esos conectores **tiene que
ponerlo en HIGH**, aunque no use la pantalla para nada. Si falta, el pin de 5V
flota y da lecturas decrecientes que parecen cable roto.

**`GPIO1` alimenta el panel.** Sin él la pantalla queda negra aunque el SPI esté
perfecto. El comentario del código de Elecrow atribuye los dos pines a la
pantalla, y eso es lo que despista.

**El táctil vive en el bus I2C 0 remapeado a 6/7.** El 38/39 es un bus
**distinto** (`TwoWire(1)`), el del header de expansión. Son dos buses, no uno.

**`memory_height` del panel va forzado a 360.** El default de LovyanGFX para el
ST77961 es 390 y la imagen queda corrida.

### El DFPlayer va en el conector UART de 4 hilos

Expone `GPIO43/44`, y eso **no se pelea con el monitor serial**: con `USB CDC On
Boot`, `Serial` es el USB nativo del S3, no UART0. Los pines quedan libres y el
firmware usa UART1 ruteado a ellos por la matriz de GPIO. No hace falta el cable
FPC del header de expansión.

Colores del cable de Elecrow: **amarillo = RX, blanco = TX, rojo = 5V, negro = GND.**

Las líneas seriales del conector están elevadas a 5V con MOSFETs BSN20 y
pull-ups de 10k, así que en reposo TX y RX miden **4.65V**, no 3.3V. Es normal.

## Entorno

- Core `esp32:esp32` **2.0.17**. No subir a 3.x: rompe `ledcSetup`, que se usa
  para el fade del backlight, y no aporta nada aquí.
- LovyanGFX **1.2.7**. La 1.2.21 deja la pantalla en blanco en esta placa.
- LVGL 8.3 con el `lv_conf.h` de `~/Documents/Arduino/libraries/`.
- `DFRobotDFPlayerMini`, `Adafruit_NeoPixel`, `cst816t`.
- Panel LovyanGFX = **ST77961**, no GC9A01 (el wiki de Elecrow se equivoca).
- `firmware/VNL1/secrets.h` con el WiFi. Está en `.gitignore`.

### Partición propia

`huge_app` da 3MB a la aplicación y con 220KB por carátula eso topa en nueve
discos. Se usa `vnl1_8M.csv`, que le da **8MB** — más de treinta discos. El
archivo vive en `<core esp32>/tools/partitions/`.

```
cd firmware/VNL1
arduino-cli compile --upload -p <PUERTO> \
  --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc" \
  --build-property build.partitions=vnl1_8M \
  --build-property upload.maximum_size=8388608 .
```

El doble clic ya usa este esquema. El puerto cambia según el conector físico
(`ls /dev/cu.usbmodem*`).

## Agregar música o un disco nuevo

Todo vive en la propia microSD, en la carpeta `_origen`. No hay carpeta local ni
copias duplicadas: un solo lugar.

**1.** Mete la microSD al Mac y conecta la perilla por USB.

**2.** Abre `_origen` y arrastra. Para un disco nuevo, crea su carpeta con el
número que le toque y el nombre que quieras:

```
_origen/05 Mario Kart/
        ├── lo que sea.mp3
        ├── otra cancion.m4a
        └── cover.png          ← opcional
```

**3.** Doble clic en **`Actualizar VinilOS.command`**.

**4.** Saca la tarjeta, ponla en el módulo.

El número decide en qué carpeta de la tarjeta cae; el nombre es el que sale en
pantalla. Solo se reconvierte lo que cambió: si únicamente tocaste una carátula,
la música se salta y termina en segundos.

### La carátula

Cualquier formato (`jpg`, `png`, `webp`). Se recorta al centro en cuadrado, se
ajusta a 336 píxeles y se le aplica máscara circular — **las esquinas se
pierden**. De ella salen dos cosas: la imagen del disco y los **ocho colores del
anillo**, uno por LED, tomados del sector que cada uno tiene detrás.

Para que el color del anillo no salga apagado, se le sube saturación y
luminosidad: un color fiel puede ser demasiado oscuro para un LED.

Se puede forzar el color desde un `album.txt` en la carpeta del disco:

```
subtitulo = Mejores canciones
color     = 0x1DB954
```

### Por qué hay un paso de proceso

El DFPlayer solo lee carpetas numéricas con archivos `001.mp3`, y **los ordena
por la tabla FAT, no por el nombre**. Además la pantalla necesita saber cosas que
el módulo nunca reporta —cuántas canciones, cuánto dura cada una, de qué color es
el disco, cómo se ve la portada— y eso viaja compilado en el firmware. El script
genera las dos mitades al mismo tiempo, y por eso tarjeta y pantalla nunca se
desincronizan.

macOS escribe metadata invisible (`.DS_Store`, `._001.mp3`) que **el DFPlayer
cuenta como pistas**: te toca silencio donde debería ir música. El script la
borra en cada corrida.

### Por qué no se puede por WiFi

El ESP32 **no tiene ningún acceso a la microSD**: está cableada solo al DFPlayer,
y ese módulo no acepta escritura por serial. Es la consecuencia directa de la
decisión de arranque — el CrowPanel no expone suficientes pines para manejar la
SD por su cuenta. Cambiar música implica sacar la tarjeta.

## Interacción

La biblioteca es circular: pasado el último disco vuelve el primero. Al final de
la fila hay dos discos especiales, **Alarma** y **Ajustes**, que se alcanzan
girando como cualquier álbum.

| Gesto | Biblioteca | Reproducción | Alarma y Ajustes |
|---|---|---|---|
| Girar | cambia de disco | **volumen** | mueve o edita el campo |
| Push | reproduce ya | pausa / reanuda | entra o confirma el campo |
| Doble push | — | siguiente canción | — |
| Mantener (900ms) | — | vuelve a la biblioteca | sale y guarda |

### Reglas que sostienen el diseño

**La pantalla es el indicador de modo.** Con una sola perilla, el giro no puede
significar dos cosas en la misma pantalla sin dejar al usuario a ciegas. Por eso
el mantener no cambia el modo del giro en su lugar: cambia de pantalla.

**Los dos estados se distinguen por escala y compañía.** En la biblioteca la
cámara está lejos: el disco al 62% y los vecinos asomando por los costados. Al
reproducir se acerca y el disco llena el cuadro, solo.

**Al volver a la biblioteca la música sigue.** El anillo sigue respirando con el
color del disco que suena. Un push sobre ese mismo disco regresa sin reiniciar.

**El mantener se ve.** Un aro crece alrededor del canto y completa la vuelta
justo cuando el gesto dispara. Sin esa realimentación, mantener se siente igual
que no hacer nada, y el usuario suelta antes de tiempo.

**El push corto llega con 260ms de retardo** (`KNOB_DOUBLE_MS`): es el precio de
tener doble push con un solo botón. Se compensa con el evento `KNOB_DOWN`, que
se emite al instante de apretar y encoge la etiqueta.

**Al pausar, el disco se detiene derecho.** Sigue de largo hasta completar la
vuelta y se asienta en cero. Un plato real para donde cae, pero en pantalla eso
se lee como falla — y esa posición torcida se arrastraba a toda la biblioteca.

## Ajustes

Siete campos, guardados en NVS:

| Campo | Opciones |
|---|---|
| Al terminar | detener · repetir · infinito |
| Reposo | 1, 3, 5, 10, 30 min · nunca |
| Luz reposo | 0 a 100% (sin música) |
| Luz música | 0 a 100% (con música) |
| Brillo | 30 a 100% |
| LEDs | sí · no |
| Brillo LEDs | 20 a 100% |

El brillo y los LEDs se aplican **mientras giras**, no al salir: ajustar a ciegas
y ver el resultado después sería adivinar. Con los LEDs en "no" se corta la
corriente de la tira (`GPIO17`), no solo el brillo — un LED apagado por brillo
sigue alimentado y sigue calentando dentro de una caja cerrada.

En reposo se retiran juntos pantalla, anillo y LED de encendido. Los primeros
valores por defecto fueron 30 segundos y apagado total, y en la práctica se leía
como aparato descompuesto: lo mirabas, estaba negro, y creías que se colgó.

## Alarma

Un disco más de la biblioteca, no un menú escondido. Hora, minuto (de cinco en
cinco), días por presets, disco a sonar y activada.

El reloj viene por **WiFi y NTP** — el ESP32 no tiene reloj con pila. La alarma
**solo dispara con hora verificada**: sin NTP el reloj arranca en 1970 y sonaría
al encender. Al sonar, la pantalla amanece con un fade de casi un segundo y
**cualquier push la apaga**, que es lo que hace una mano dormida.

## Estado

Todo lo planeado está construido y probado en placa:

- **Perilla** — horario = CW, 4 sub-pasos por muesca. El botón se sondea en el
  **segundo núcleo cada 5ms**, así que responde igual sin importar los fps.
- **Pantalla** — LVGL sobre LovyanGFX, carátula a disco completo girando.
- **Audio** — DFPlayer con barajado Fisher-Yates, throttling de 60ms, rampa de
  arranque de volumen y avance automático de pista.
- **Biblioteca** — circular, con vecinos, carátulas y datos del disco.
- **Alarma y Ajustes** — persistidos en NVS.
- **Anillo** — ocho colores tomados de la carátula.
- **Pipeline de la SD** — un doble clic, incremental.

### Rendimiento

| Estado | fps |
|---|---|
| Biblioteca (nada se mueve) | ~500 |
| Reproduciendo, carátula girando | ~13 |

Rotar los 336 píxeles de la carátula cuesta caro. Se compensa girando a **3 RPM**
—una vuelta cada veinte segundos— y **sin suavizado**, que fue lo que llevó de
6.6 a 13 fps. La perilla no se ve afectada porque vive en el otro núcleo.

## El módulo DFPlayer

El de esta caja es un **clon MH2024K**, no el YX5200 original. Dos consecuencias:

**`isACK = false` no es negociable.** Con `true`, la librería se queda en un
bucle infinito dentro de `sendStack()` esperando la confirmación de cada comando,
y este clon confirma unos sí y otros no. El sketch se cuelga en seco, sin timeout
que lo salve. No se pierde nada importante: los avisos útiles llegan solos.

**Necesita ~2 segundos tras energizarse** antes de aceptar comandos.

### Cableado

```
Cable UART            DFPlayer Mini
──────────────────────────────────
rojo    (5V)  ─────────► VCC   (pin 8)
negro   (GND) ─────────► GND   (pin 2)
blanco  (TX)  ──[1kΩ]──► RX    (pin 7)
amarillo(RX)  ◄───────── TX    (pin 6)
                         bocina entre pines 1 y 3
```

La resistencia de 1kΩ va **solo** en la línea TX→RX: sin ella entra ruido del
ESP32 al amplificador. El capacitor de 1000µF entre VCC y GND del módulo evita
que los picos de audio tumben el voltaje.

El LED rojo del módulo **no se puede apagar por software** — está cableado al
hardware. Se tapa con pintura.

## Notas

- La fuente Montserrat de LVGL no trae acentos ni `…`/`—`. Todo el texto de
  pantalla va en ASCII.
- microSD máximo 32GB, FAT32 obligatorio.
- Al leer el serial desde scripts: abrir el puerto con **DTR y RTS en `False`
  antes de `open()`** y no togglearlos, o el S3 se queda en modo DOWNLOAD.
