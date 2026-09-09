# La línea

Tres modelos, un mismo firmware. **VinilOS** corre en los tres; lo que cambia es
la caja, el audio y la energía.

| | Base | Slim | Max |
|---|---|---|---|
| Bocinas | 1 mono | 1 mono | **2 estéreo** |
| Amplificador | interno del DFPlayer | interno del DFPlayer | **PAM8403 externo** |
| Potencia real | ~2W | ~2W | **~2W por canal** |
| Caja | 9×9×8 cm | **más delgada** | **más grande** |
| Energía | cargador 5V 2A | cargador 5V 2A | **cargador 5V 2A o más** |

Este documento describe **la Base** en detalle, porque es la que ya existe y
está terminada. Las otras dos se definen como diferencias contra ella.

---

# Base

Reproductor de música de escritorio con estética de tornamesa. Caja de acrílico
transparente, pantalla redonda, e **interacción exclusivamente por perilla**:
girar para navegar, presionar para actuar, mantener para volver.

No tiene botones, ni app, ni pantalla táctil activa. El objeto se maneja con una
sola mano y sin instrucciones.

## Hardware

| Pieza | Especificación |
|---|---|
| Placa | Elecrow CrowPanel 1.46" HMI ESP32-S3 Rotary Display |
| Procesador | ESP32-S3, doble núcleo, 8MB PSRAM, 16MB flash |
| Pantalla | IPS redonda 360×360, panel ST77961, backlight con PWM |
| Control | Encoder rotativo con push integrado |
| Luz ambiental | Anillo de 8 LEDs RGB direccionables |
| Audio | DFPlayer Mini (clon MH2024K), amplificador interno 3W |
| Bocina | 4Ω 3W, 40mm, carcasa metálica |
| Almacenamiento | microSD 32GB FAT32 |
| Red | WiFi 2.4GHz, solo para la hora |
| Caja | Acrílico transparente 3mm, 9×9×8 cm interiores |

Componentes de soporte: resistencia de 1kΩ en la línea TX→RX, capacitor de
1000µF entre VCC y GND del módulo de audio.

## Qué hace

**Biblioteca de discos.** Cada carpeta de la microSD es un vinilo con su
carátula, su nombre y su número de canciones. Se hojea girando la perilla, en
círculo: pasado el último vuelve el primero.

**Reproducción aleatoria.** Un push y suena, barajado con Fisher-Yates. Cada
entrada al disco es un orden nuevo.

**El disco gira.** La carátula ocupa el disco completo y rota a 3 RPM. Al pausar
frena y **se asienta derecho**, nunca torcido.

**Anillo que refleja el arte.** Los ocho LEDs toman cada uno el color del sector
de la carátula que tienen detrás. Respira mientras hay música. También puede ir
en ámbar fijo o en un arcoíris lento, o apagado.

**Dice qué está sonando.** Bajo el disco, el título de la canción y el artista
con el tiempo transcurrido. El módulo de audio no entrega metadata: los nombres
salen de los tags de los MP3 y viajan compilados en el firmware, junto a las
duraciones reales de cada pista.

**Reloj de reposo.** Digital o analógico, con **fecha y clima del día**. El clima
viene de un servicio abierto que no pide registro. Sin red, el renglón del clima
desaparece y el reloj sigue dando la hora.

**Alarma.** Un disco más de la biblioteca, con hora, días y qué álbum sonar. El
reloj llega por NTP. Cualquier push la apaga.

**Ajustes.** Siete campos. Seis son iluminación —brillo, cuánto tarda en
retirarse, qué queda al retirarse, qué tan tenue, color del anillo y su brillo—
y el séptimo es qué hacer al terminar un álbum: detener, repetir o encadenar al
siguiente para siempre.

Cada campo controla una sola idea, y el brillo es el techo de todo: los niveles
de reposo son una fracción de él, así que el aparato en reposo nunca puede
quedar más claro que el aparato en uso.

**Reposo.** A los minutos que elijas se retiran juntos pantalla, anillo y LED de
encendido. El primer gesto solo despierta, no actúa.

Una sola condición lo dispara: **no tocaste la perilla**. No importa dónde
estuvieras — biblioteca, reproduciendo, en pausa, dentro de Ajustes. El mismo
minuto de abandono hace siempre lo mismo.

Lo que queda en la pantalla depende de si hay música: con música sigue el disco
girando, atenuado; en silencio entra el reloj. Si elegiste "apagar", se apaga en
los dos casos — quien lo elige quiere el cuarto a oscuras.

**El anillo se apaga siempre al dormir**, tenga reloj la pantalla o no. El reloj
es información —lo pusiste para leerlo de noche— y el anillo es decoración que
pertenece al uso.

La música **no se detiene**: duerme la pantalla, no el aparato.

## Interacción

| Gesto | Biblioteca | Reproducción |
|---|---|---|
| Girar | cambia de disco | volumen |
| Push | reproduce | pausa / reanuda |
| Doble push | — | siguiente canción |
| Mantener 900ms | — | vuelve a la biblioteca |

Las reglas de diseño detrás de esto están en el README del firmware. La central:
**la pantalla es el indicador de modo**, porque con una sola perilla el giro no
puede significar dos cosas en la misma pantalla.

## Cómo se carga la música

La microSD se saca, se mete al Mac, se arrastran los archivos a la carpeta
`_origen` y un doble clic hace el resto: convierte, ordena, procesa las
carátulas, regenera el manifiesto y flashea el firmware.

Solo se reconvierte lo que cambió. Cambiar una portada toma segundos.

## Límites conocidos

Son consecuencias de decisiones tomadas a conciencia, no defectos pendientes.

**No se puede cargar música por WiFi.** El ESP32 no tiene ningún acceso a la
microSD: está cableada solo al módulo de audio, que no acepta escritura. Viene de
que la placa no expone suficientes pines para manejar la SD por su cuenta. Es el
límite estructural de este diseño.

**Las carátulas van compiladas en el firmware**, así que agregar un disco implica
reflashear. Por eso la carga de música y el flasheo ocurren en el mismo doble
clic.

**Es mono.** El módulo tiene un solo canal amplificado. Las salidas de línea
estéreo existen pero no se usan — ahí es donde entra la Max.

**Necesita WiFi para la alarma.** Sin red no sabe qué hora es, y la alarma no
dispara hasta tener hora verificada.

**Máximo unos treinta discos**, por el espacio que ocupan las carátulas en la
partición de 8MB. La música no cuenta: vive en la tarjeta.


## Lista de materiales

Todo lo que lleva una unidad completa.

### Módulos

| Pieza | Especificación | Cant. |
|---|---|---|
| Placa principal | Elecrow CrowPanel 1.46" HMI ESP32-S3 Rotary Display | 1 |
| Módulo de audio | DFPlayer Mini — el clon MH2024K funciona | 1 |
| Tarjeta de memoria | microSD 32GB Clase 10, formateada en FAT32 | 1 |
| Bocina | 4Ω 3W, 40mm, carcasa metálica | 1 |

La microSD **no puede pasar de 32GB**: es límite del módulo de audio, no del
formato.

### Componentes

| Pieza | Valor | Cant. | Para qué |
|---|---|---|---|
| Resistencia | 1kΩ, 1/4W | 1 | En la línea TX→RX. Sin ella se acopla ruido del ESP32 al amplificador y se oye un siseo constante |
| Capacitor electrolítico | 1000µF 16V | 1 | Entre VCC y GND del módulo de audio. Sin él, el golpe de corriente al arrancar una canción tumba el voltaje y reinicia la placa |

Los dos vienen en los kits ELEGOO. La resistencia de 1kΩ es café-negro-rojo-dorado.

### Cableado

| Pieza | Notas | Cant. |
|---|---|---|
| Cable JST 1.25mm 4 pines | Viene uno en la caja del CrowPanel. Va del conector UART al módulo de audio | 1 |
| Conectores Dupont hembra | Para rematar el cable JST del lado del módulo | 4 |
| Termorretráctil o cinta | Para aislar la unión de la resistencia | — |
| Estaño | — | — |

La unión de la resistencia **tiene que quedar aislada**: va en medio de cuatro
conductores que se doblan dentro de la caja, y un corto contra el rojo mete 5V
directo a un GPIO.

### Caja

| Pieza | Especificación |
|---|---|
| Acrílico transparente colado | 3mm de espesor |
| Medidas interiores | 9 × 9 × 8 cm |
| Barreno frontal | 54mm, para la perilla |
| Rejilla superior | 25 barrenos de 3mm en patrón hexagonal |

Considerar una **ranura de acceso a la microSD** en una cara: cambiar música
implica sacar la tarjeta, y sin ranura hay que abrir la caja cada vez.

### Alimentación

| Pieza | Especificación | Cant. |
|---|---|---|
| Cargador USB de pared | 5V, **2A mínimo** | 1 |
| Cable USB-A a USB-C | El de la placa | 1 |

Los 2A no son por consumo promedio —la caja anda en 300 a 400 mA— sino por los
picos del amplificador al arrancar una canción. Con una fuente floja el aparato
se reinicia justo al empezar a sonar.

**El cargador va incluido en la caja**, no es opcional. Es la única protección
que de verdad funciona contra el modo de falla que ya nos costó un módulo de
audio: elimina la decisión, y nadie improvisa con un power bank si ya venía con
su cargador. La sección de energía del README explica por qué.

### Lo que NO lleva

Piezas que estuvieron en el plan original y quedaron descartadas:

- **Cable FPC de 12 pines.** El módulo de audio terminó en el conector UART de 4
  hilos, que ya viene con la placa.
- **Amplificador externo.** Eso es la Max.
- **Batería.** La Base va por cable.

## Para quien la recibe

La hoja que va en la caja. Tres reglas, y las tres son sobre corriente.

**1. Usa el cargador que viene en la caja.** Es un 5V 2A. Cualquier cargador de
celular equivalente sirve.

**2. Nunca la conectes a un power bank que se esté cargando.** Es el modo que los
fabricantes llaman *pass-through* o *power share*, y ahí la salida puede subirse
por encima de lo que el aparato aguanta. Si vas a usar power bank, que sea sin
estar enchufado, y con cable USB-A.

**3. Si huele raro o se calienta, desconéctala.** El olor llega antes que el
humo.

Y una de uso: **el primer gesto solo despierta.** Si la pantalla está dormida,
girar o presionar no cambia de disco ni pausa nada — solo la trae de vuelta. Es a
propósito, para que alcanzar la perilla a ciegas no tenga consecuencias.

## Rendimiento

| Estado | fps |
|---|---|
| Biblioteca | ~500 |
| Reproduciendo, disco girando | ~13 |

La perilla se sondea en el segundo núcleo cada 5ms, así que responde igual sin
importar los cuadros por segundo.

---

# Slim

Lo mismo, en una caja más delgada.

Por definir: qué bocina entra en el perfil reducido, si la placa se monta en
paralelo a la cara frontal en vez de perpendicular, y si se conserva el mismo
módulo de audio.

Alimentación por cable. En un perfil delgado no cabe batería sin comprometer la
caja, y agregar una celda de litio a un espacio apretado es justo lo que no
conviene hacer.

---

# Max

Estéreo real y el doble de potencia.

El cambio central es dejar de usar el amplificador interno del módulo y sacar el
audio por sus **salidas de línea `DAC_L` y `DAC_R`**, que hoy están sin uso, hacia
un amplificador externo PAM8403 de dos canales.

| | Base | Max |
|---|---|---|
| Salida del módulo | `SPK1` / `SPK2` | `DAC_L` / `DAC_R` |
| Amplificador | interno, un canal | PAM8403, dos canales |
| Bocinas | 1 de 4Ω | 2 de 4Ω, una por canal |
| Corriente | ~500mA pico | **más de 1A pico** |

Tres consecuencias a resolver en el diseño de la caja:

**La fuente tiene que dar 2A.** El amplificador debe alimentarse en estrella
desde el punto de entrada, nunca colgado del riel de 5V de la placa.

**Las bocinas van separadas.** A ocho centímetros de distancia no hay imagen
estéreo perceptible. Para que el estéreo signifique algo, tienen que ir en caras
opuestas y lo más lejos posible.

**El material ya es estéreo.** Los MP3 se convierten a estéreo 44.1kHz desde el
primer día; hoy se colapsa a mono en el amplificador interno. La Max no agrega
información, la deja de tirar.

## Qué cambia en el firmware

**Casi nada, y eso es la buena noticia.** El volumen se sigue mandando con el
mismo comando: en el DFPlayer es digital y ocurre **antes** del DAC, así que
gobierna igual las salidas de línea que el amplificador interno. La perilla no
cambia, la interfaz no cambia, el manifiesto no cambia, y la música ya viene en
estéreo desde el script.

Tres cosas sí piden atención, y ninguna es grande:

**`PLAYER_VOL_MAX` tiene que ser otro.** La salida de línea al máximo satura la
entrada del PAM8403 mucho antes que la del amplificador interno. Si se deja el
tope de la Base, los últimos pasos de la perilla no suben volumen: distorsionan.
Hay que encontrarlo de oído, con las bocinas definitivas montadas en la caja.

**El siseo en silencio se vuelve audible.** El PAM8403 amplifica su propio ruido
de fondo, y con dos bocinas se oye el doble. El módulo tiene pin `SHDN`: colgarlo
de un GPIO libre y apagarlo cuando no hay música —y sobre todo en reposo— quita
el siseo por completo. Encaja con la regla que ya existe: dormido es dormido.

**Un interruptor de modelo en tiempo de compilación.** Un `#define MODELO_MAX` en
`pins.h` que decida el tope de volumen y si existe el pin de `SHDN`. Un solo
código para los tres modelos, sin ramas que se desincronicen.

**La Slim no necesita nada.** Mismo audio, mismo firmware, mismo binario.

---

# Lo que comparten las tres

Mismo firmware, mismo flujo de carga de música, misma interacción, mismas
carátulas, misma alarma. Un cambio en VinilOS llega a los tres modelos con el
mismo doble clic.

Eso es deliberado: lo que distingue a los modelos es la caja y el audio, no el
comportamiento. Alguien que aprendió a usar una sabe usar las tres.
