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

**El panel se arranca en frío a propósito, y `gfx.init()` espera 150ms.** Las dos
cosas son la misma corrección, y costó una noche encontrarla.

El síntoma era una banda de rayas horizontales al centro de la pantalla, que solo
se quitaba desconectando y volviendo a conectar el USB. El serial reportaba 499
fps y todo en orden — porque **el bus del panel es de escritura**: el ESP32 empuja
píxeles y nunca puede preguntarle al controlador cómo está. Un panel mal
configurado y uno perfecto se ven idénticos desde el firmware.

Dos causas, las dos en `display_begin()`:

- `PIN_LCD_PWR` solo se ponía en HIGH, nunca en LOW. En un reinicio por software
  —watchdog, brownout, subir firmware— el pin ya venía en HIGH, así que el panel
  no perdía corriente y conservaba el estado corrupto. Desconectar el USB era lo
  único que le daba un arranque en frío. Hoy el pin se baja 80ms antes de
  subirlo: es hacer en software lo mismo que desconectar.
- No había espera tras soltar `RST`. Los ST77xx necesitan ~120ms para terminar su
  encendido interno antes de aceptar comandos, y `gfx.init()` se llamaba
  microsegundos después. La configuración se aplicaba a medias, y **una ventana
  de direcciones mal escrita es exactamente esa banda de rayas**: los píxeles
  llegan bien y aterrizan en el renglón equivocado.

Que fallara de forma intermitente era la carrera de tiempos. Con el aparato
estable casi siempre ganaba; con brownouts encadenados, casi nunca.

Consecuencia práctica: hoy **cualquier reinicio del ESP32 recupera la pantalla**.
Ya no hace falta desconectar. Si algún día vuelven las rayas sin que el ESP32 se
haya reiniciado, el botón de reset basta.

### El DFPlayer va en el conector UART de 4 hilos

Expone `GPIO43/44`, y eso **no se pelea con el monitor serial**: con `USB CDC On
Boot`, `Serial` es el USB nativo del S3, no UART0. Los pines quedan libres y el
firmware usa UART1 ruteado a ellos por la matriz de GPIO. No hace falta el cable
FPC del header de expansión.

Colores del cable de Elecrow: **amarillo = RX, blanco = TX, rojo = 5V, negro = GND.**

Las seriales van **cruzadas** — lo que sale de uno entra al otro:

```
rojo     ──────────────────────►  VCC del modulo
negro    ──────────────────────►  GND del modulo
blanco   ───[ 1kΩ ]────────────►  RX  del modulo      (TX placa → RX modulo)
amarillo ◄─────────────────────   TX  del modulo      (TX modulo → RX placa)
```

La resistencia de 1kΩ va **solo en el blanco**, y esa unión tiene que quedar
aislada: son cuatro conductores que se doblan dentro de la caja, y un corto
contra el rojo mete 5V directo a un GPIO.

Del lado del módulo, el DFPlayer Mini trae **VCC, RX y TX seguidos en la misma
fila** (pines 1, 2 y 3), y GND es el pin 7 de esa fila. Los clones MH2024K suelen
venir **sin serigrafía**, así que la referencia práctica es el capacitor de
1000µF: su pata positiva está en VCC y la de la franja en GND. Contando desde
VCC, el siguiente pin es RX y el de después TX; si VCC y GND quedan a seis
posiciones en la misma fila, vas contando en la dirección correcta.

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

**El número también decide el orden de la biblioteca**, y el DFPlayer acepta
hasta la carpeta 99. De ahí sale la convención: **90 a 99 son "siempre hasta el
final"**. `99 Relax` se queda de último aunque después agregues 05, 06 y 07,
sin renumerar nada.

Dos cosas: escribe **siempre dos dígitos** —el orden es alfabético, y `5 Algo`
caería después de `10 Algo`— y ten en cuenta que **renombrar cuenta como disco
nuevo**, así que ese álbum se reconvierte completo una vez.

### Los nombres de las canciones

El DFPlayer no entrega metadata: si el título no sale del manifiesto, no sale de
ningún lado. El script lo saca con `ffprobe` de los tags de los archivos
**originales** —no de los convertidos— y los compila en `albums.h` junto a las
duraciones. Sin tags usa el nombre del archivo, quitándole el número de pista.

En reproducción se muestran en capa fija sobre el disco, nunca en el disco: la
etiqueta gira, y un texto girando no se lee.

```
Snake Eater
Cynthia Harrell  -  2:31
```

Ojo con el índice al tocar ese código: `player_track_index()` es la posición en
la baraja y los nombres están indexados por **archivo**. Con el álbum barajado
esos dos números no coinciden nunca, y para eso existe `player_track_file()`.

**El script limpia lo que puede limpiar solo:** quita el identificador que pega
yt-dlp (`[lMRziQRmYLI]`), las extensiones sueltas (`.wmv`), los paréntesis de
ruido —*Official Video*, *Lyrics*, *Remastered*— dejando los que sí dicen algo
como *(Latino)*, y el prefijo `Artista - ` cuando de verdad coincide con el
artista del tag. Los títulos vacíos o llamados `Untitled` se descartan a favor
del nombre del archivo.

**Lo que no puede adivinar va en `album.txt`.** Casi toda esta música viene de
YouTube, y ahí el tag de artista suele ser el canal que subió el video: ningún
script puede saber que *SMORT* no compuso Minecraft. El archivo vive junto a la
música y manda sobre los tags:

```ini
subtitulo = Bandas sonoras
artista   = C418            # para todas las pistas del disco
1 = Beginning               # titulo de una pista
2 = Equinoxe
2.artista = Otro            # artista de una sola pista
```

La numeración es el orden alfabético de los archivos, que es el mismo con el que
se graban en la tarjeta. `artista =` vacío deja el renglón con solo el tiempo,
que es lo correcto para bandas sonoras sin intérprete.

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

Siete campos, guardados en NVS. Los seis de iluminación van juntos y en el orden
en que ocurren; **Al terminar** no es luz y por eso queda aparte, al final.

| Campo | Opciones | Qué controla |
|---|---|---|
| Brillo | 30 a 100% | La pantalla mientras la usas. Techo real de todo lo demás |
| Reposo | 1, 3, 5, 10, 30 min · nunca | Cuánto tarda en retirarse |
| En reposo | apagar · reloj digital · reloj análogo | Qué queda en pantalla |
| Luz reposo | 10 a 100% **del brillo** | Qué tan tenue queda |
| LEDs | apagados · amarillo · carátula · RGB | El anillo, encendido y color en un solo campo |
| Brillo LEDs | 10 a 100% | Del anillo |
| Al terminar | detener · repetir · infinito | Al acabarse el álbum |

Tres reglas de diseño, todas aprendidas de la versión anterior:

**Una idea por campo.** El anillo eran dos campos (encendido + color) y el reloj
otros dos (sí/no + tipo). Separados permitían estados que no significan nada,
como "LEDs no, color carátula". Fusionados, el estado imposible no existe.

**Luz reposo es relativa al brillo, no absoluta.** Antes los dos niveles eran
independientes, y eso dejaba que el reposo quedara **más claro** que el uso:
brillo al 50% con luz música al 80% y la pantalla en reposo brillaba más que
usándola. Siendo una fracción, bajar el brillo baja las dos cosas.

**Un control que no puede hacer nada no se puede elegir.** Con la pantalla en
"apagar", *Luz reposo* no significa nada; con el anillo en "apagados", *Brillo
LEDs* tampoco. Se dibujan tenues —para que se vea que existen— y el giro pasa de
largo.

Desapareció **Luz música**: lo que cambia al haber música no es qué tan fuerte
alumbra la pantalla, sino **qué muestra**. En reposo con música sigue el disco
girando; en reposo sin música entra el reloj. Un solo nivel para las dos.

Todo se aplica **mientras giras**, no al salir: ajustar a ciegas y ver el
resultado después sería adivinar. Con los LEDs en "apagados" se corta la
corriente de la tira (`GPIO17`), no solo el brillo — un LED apagado por brillo
sigue alimentado y sigue calentando dentro de una caja cerrada.

**El anillo se apaga al dormir, siempre**, tenga reloj la pantalla o no. Durante
un tiempo esto colgó de que la pantalla llegara a cero, y con el reloj puesto
nunca llega: el anillo se quedaba encendido toda la noche a brillo 3 de 255.
El razonamiento estaba mal, no el código — el reloj es **información** y el
anillo es **decoración que pertenece al uso**. Compartir interruptor solo tiene
sentido cuando los dos se apagan.

En reposo se retiran juntos pantalla, anillo y LED de encendido. Los primeros
valores por defecto fueron 30 segundos y apagado total, y en la práctica se leía
como aparato descompuesto: lo mirabas, estaba negro, y creías que se colgó. Por
eso "apagar" ya no es el valor de fábrica — lo es el reloj al 12%.

Los ajustes de la versión anterior **se migran solos** en el primer arranque: el
par LEDs+color se traduce a un modo, el par reloj+tipo a *En reposo*, y las
llaves viejas se borran de NVS para que no vuelvan a pisar lo nuevo.

## Alarma

Un disco más de la biblioteca, no un menú escondido. Hora, minuto (de cinco en
cinco), días por presets, disco a sonar y activada.

El reloj viene por **WiFi y NTP** — el ESP32 no tiene reloj con pila. La alarma
**solo dispara con hora verificada**: sin NTP el reloj arranca en 1970 y sonaría
al encender. Al sonar, la pantalla amanece con un fade de casi un segundo y
**cualquier push la apaga**, que es lo que hace una mano dormida.

## El reloj de reposo

Tres renglones, iguales en las dos caras: **qué día es, qué hora es, qué tiempo
hace**. La analógica los lleva más chicos y más tenues, entre los numerales y el
centro, que es donde un reloj de verdad pone su ventanilla.

**No hay AM/PM.** El formato es de 12 horas, pero en un reloj de escritorio nadie
duda entre las tres de la tarde y las tres de la madrugada, y ese texto solo le
restaba tamaño a lo que sí importa.

La hora usa una **Montserrat de 78 puntos generada con `lv_font_conv`** — LVGL
solo trae hasta 48. Lleva únicamente dígitos, dos puntos y guion, así que pesa
76KB en vez de los ~400KB de un juego completo a ese tamaño. El grado (`°`) sí
existe en la Montserrat de LVGL, código 176, así que la temperatura se escribe
de verdad y no con una "C" pegada.

### El clima

**Open-Meteo, sin llave de API.** Es deliberado: estas cajas se regalan, y un
servicio que pida registrarse convierte un regalo en un trámite.

Corre en **su propia tarea del núcleo 0**, igual que el sondeo de la perilla:
`HTTPClient` bloquea mientras negocia TLS, y hacerlo en el loop principal se
sentiría como un tirón en la pantalla. Se refresca cada 30 minutos, y cada 5 si
falló — el clima no cambia por minuto.

Sin lectura buena el renglón **queda vacío en vez de mentir**: el reloj no
depende del clima para servir, y sin red sigue dando la hora.

Las coordenadas están en `weather.h` (`CLIMA_LAT` / `CLIMA_LON`), hoy Monterrey.
Es lo único que hay que tocar si una caja va a vivir en otra ciudad.

No se usa librería de JSON: la respuesta es de forma fija y conocida, y arrastrar
ArduinoJson para leer dos números sería pagar de más.

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

## Energía

Esta sección existe porque un módulo de audio se quemó, y las tres reglas que
siguen son la única razón por la que no volverá a pasar.

### Nunca un power bank cargándose

**No alimentes el aparato con un power bank que esté conectado a la corriente al
mismo tiempo.** El modo se llama *pass-through* o *power share*, y es donde peor
se portan: muchos no están diseñados para cargar y entregar a la vez, y en esa
condición la salida puede subirse por encima de 5V, oscilar, o dejar pasar
voltaje del cargador antes de que su regulador tome control.

El DFPlayer Mini aguanta 5V y su máximo absoluto son ~5.5V. Un pico así entra por
`VCC`, golpea la etapa del amplificador y la funde cerrada.

Así se ve cuando pasa, y así se confirma:

| Señal | Qué significa |
|---|---|
| Ruido raro en la bocina al conectar | El amplificador muriéndose por sobrevoltaje |
| Humo del módulo | La unión ya se fundió |
| **3-4Ω entre VCC y GND** con el multímetro | Confirmado: unión de semiconductor en corto |

Un corto de 0Ω sería una gota de estaño. **3-4Ω es silicio fundido**, o sea causa
eléctrica externa, no error de armado. Un módulo sano pita un instante —son sus
condensadores cargándose— y luego sube a cientos de ohms.

**Un power bank mejor no cambia la regla.** Anker desaconseja el pass-through en
la mayoría de sus modelos. Uno bueno hace el pico menos probable, no imposible.
O lo cargas, o lo usas.

### Nunca desde un hub con el lector de SD trabajando

Un hub USB repartiendo corriente entre un lector de microSD que está copiando
archivos y la placa **no da lo suficiente**. Se ve como un bucle de reinicios,
aproximadamente uno por segundo, que muere siempre en `display_begin()` — que es
donde se energizan panel y riel de 5V a la vez.

**El problema no es "el hub".** Es compartirlo con el lector activo. Un adaptador
pasivo USB-A a USB-C no reparte nada y está bien.

En un Mac sin puertos USB-A:

- **Flashear** → cable directo a la Mac, o adaptador pasivo. Sin hub.
- **Leer la microSD** → ahí sí el hub, con la placa desconectada.

### Con qué sí

| Uso | Fuente |
|---|---|
| De escritorio, siempre | **Cargador de pared 5V 2A** |
| Flasheo y desarrollo | Puerto de la Mac, directo |
| Portátil | Power bank **sin cargarse**, y con cable **USB-A a USB-C** |

Los 2A no son por consumo promedio —la caja anda en 300 a 400 mA— sino por los
picos del amplificador al arrancar una canción.

Lo del cable USB-A importa en power banks "inteligentes": con C a C pueden
negociar 9V o más. El puerto USB-A siempre entrega 5V fijos y no hay negociación
posible. Elimina el riesgo de raíz.

### Para las cajas que se regalan

**Incluye el cargador en la caja.** Es lo que hace Apple y funciona porque elimina
la decisión: nadie improvisa con un power bank si ya venía con su cargador. Ponle
una etiqueta junto al puerto que diga **5V 2A**.

Como refuerzo en la placa, dos componentes de centavos en la entrada de 5V:

- **Fusible rearmable (PTC) de ~1.5A** — corta si algo se va en corto y se
  recupera solo al quitar la falla.
- **Protección de polaridad inversa** con un MOSFET de canal P en serie. Si
  alguien invierte positivo y negativo, no prende en vez de morirse.

Un TVS **no** resuelve el caso del power bank: el DFPlayer aguanta 5.5V y
cualquier supresor para un riel de 5V empieza a conducir arriba de 6V. Contra un
pico grande sirve, contra 7V sostenidos llega tarde. Por eso la primera capa
importa más — **el cargador incluido no es un consejo, es el diseño**.

## Lo que costó caro

Errores que tomaron horas o días. Están aquí para no repetirlos.

**El riel de 5V no aparece en el wiki.** `GPIO2` alimenta los conectores UART e
I2C, y `GPIO1` el panel. Un sketch de prueba que no los ponga en HIGH mide
voltajes decrecientes en el conector que parecen cable roto. Costó una sesión
entera. Solo está en el código de fábrica de Elecrow y en el esquemático.

**El bus del panel es de una sola dirección.** El ESP32 escribe píxeles y nunca
puede preguntarle al controlador cómo está. Un panel perfecto y uno mal
configurado se ven **idénticos desde el firmware**: 499 fps en los dos casos. Si
lo que se ve en pantalla contradice lo que reporta el serial, el serial no es
evidencia — hay que mirar la pantalla.

**Un `while` sin salida por confiar en un ACK.** El clon MH2024K nunca responde,
y `isACK=true` deja `sendStack()` girando para siempre. No es negociable.

**macOS envenena la FAT32.** Los `._archivo.mp3` de AppleDouble se veían como
audio: ffmpeg tronaba y aparecían pistas fantasma. Hay que filtrarlos antes de
listar, y tolerar que alguno esté protegido contra borrado.

**Un arreglo a medias es peor que ninguno.** Un parche que declaró `ajLista` pero
nunca lo creó dejó un `lv_obj_set_y(nullptr)` que reiniciaba el aparato al entrar
a Ajustes. Aplicar la mitad de un cambio deja el código en un estado que nadie
diseñó.

**Un límite en la interfaz no es un límite en los datos.** Se puso el mínimo de
*Luz Reposo* en 20% pero no se limitó el valor ya guardado en NVS, así que un 0
heredado sobrevivió y producía un reloj invisible. Lo que la perilla no puede
alcanzar, la memoria tampoco debe imponer.

**Dos problemas de energía distintos parecen uno solo.** El bucle de reinicios en
el hub y la muerte del módulo con el power bank tenían causas separadas, y
juntarlos en una sola explicación llevó a acusar al módulo de estar fallando
antes de tiempo. No estaba: eran dos fallas de fuentes distintas.

**Nunca borres la caché de compilación de una sesión ajena.** Un `rm -rf` sobre
`~/Library/Caches/arduino/sketches/...` mientras otra compilación corría la mató
con un error de archivo faltante que parece corrupción del proyecto.

## Notas

- La fuente Montserrat de LVGL no trae acentos ni `…`/`—`. Todo el texto de
  pantalla va en ASCII. El grado (`°`, código 176) **sí** existe.
- microSD máximo 32GB, FAT32 obligatorio.
- Al leer el serial desde scripts: abrir el puerto con **DTR y RTS en `False`
  antes de `open()`** y no togglearlos, o el S3 se queda en modo DOWNLOAD.
