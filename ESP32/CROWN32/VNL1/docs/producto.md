# La línea

Tres modelos, un mismo firmware. **VinilOS** corre en los tres; lo que cambia es
la caja, el audio y la energía.

| | Base | Slim | Max |
|---|---|---|---|
| Bocinas | 1 mono | 1 mono | **2 estéreo** |
| Amplificador | interno del DFPlayer | interno del DFPlayer | **PAM8403 externo** |
| Potencia real | ~2W | ~2W | **~2W por canal** |
| Caja | 9×9×8 cm | **más delgada** | **más grande** |
| Energía | por definir | cable | por definir |

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
de la carátula que tienen detrás. Respira mientras hay música.

**Alarma.** Un disco más de la biblioteca, con hora, días y qué álbum sonar. El
reloj llega por NTP. Cualquier push la apaga.

**Ajustes.** Tiempo de reposo, brillo de pantalla con y sin música, LEDs
encendidos o apagados, brillo del anillo, y qué hacer al terminar un álbum:
detener, repetir o encadenar al siguiente para siempre.

**Reposo.** A los minutos que elijas se retiran juntos pantalla, anillo y LED de
encendido. El primer gesto solo despierta, no actúa.

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

| Pieza | Especificación |
|---|---|
| Cargador USB de pared | 5V, **2A mínimo** |

Los 2A no son por consumo promedio —la caja anda en 300 a 400 mA— sino por los
picos del amplificador al arrancar una canción. Con una fuente floja el aparato
se reinicia justo al empezar a sonar.

### Lo que NO lleva

Piezas que estuvieron en el plan original y quedaron descartadas:

- **Cable FPC de 12 pines.** El módulo de audio terminó en el conector UART de 4
  hilos, que ya viene con la placa.
- **Amplificador externo.** Eso es la Max.
- **Batería.** La Base va por cable.

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

---

# Lo que comparten las tres

Mismo firmware, mismo flujo de carga de música, misma interacción, mismas
carátulas, misma alarma. Un cambio en VinilOS llega a los tres modelos con el
mismo doble clic.

Eso es deliberado: lo que distingue a los modelos es la caja y el audio, no el
comportamiento. Alguien que aprendió a usar una sabe usar las tres.
