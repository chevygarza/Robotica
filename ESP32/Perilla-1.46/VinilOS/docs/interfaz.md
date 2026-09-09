# VinilOS — Lineamiento de interfaz

El "HIG" de VinilOS. Todo lo que sigue está medido contra el código, no
propuesto: si una regla dice 336 píxeles es porque `LABEL_PX = 336`.

Cualquier pantalla o disco nuevo debe respetar esto. Romper una regla se puede;
romperla sin documentar por qué, no.

---

# 1. Principios

**Es un objeto, no una aplicación.** Se maneja con una mano, sin instrucciones y
sin mirar dos veces. Si algo necesita explicarse, está mal resuelto.

**La pantalla es el indicador de modo.** Con una sola perilla, el giro no puede
significar dos cosas en la misma pantalla — sería un modo invisible. Cambia la
pantalla, cambia el significado del giro. Ésta es la regla de la que se derivan
casi todas las demás.

**Nada se corta de golpe.** Todo aparece y desaparece con transición. Un corte
duro se lee como falla, no como cambio.

**Lo que no se puede hacer, no se ofrece.** Un control que hoy no controla nada
se dibuja tenue y el giro pasa de largo.

**Sobrio antes que llamativo.** Tipografía, negro, ámbar y el arte del disco. El
color de verdad lo pone la música, no la interfaz.

---

# 2. El lienzo

Pantalla IPS **redonda de 360 × 360**. El cuadrado existe en memoria; el círculo
es lo único que se ve.

## Área segura

| Zona | Regla |
|---|---|
| Radio útil | **150 px** desde el centro (300 de diámetro) |
| Esquinas | **No existen.** Nada crítico ahí, nunca |
| Franja superior | Nada por encima de **y = 30** |
| Franja inferior | Nada por debajo de **y = 320** |
| Texto de una línea | ancho **260**, en `x = 50` |
| Filas de lista | de `x = 52` a `x = 308` |

La cuerda del círculo se angosta rápido conforme te alejas del centro. A 100 px
del centro quedan 298 px de ancho; a 140 px quedan solo 226. **Entre más abajo o
más arriba, más corto el renglón.**

## Posiciones establecidas

| Elemento | y |
|---|---|
| Título de pantalla | 32 |
| Subtítulo o estado | 60 |
| Primera fila de lista | 86 |
| Centro óptico | 180 |
| Título de canción | 266 |
| Artista y tiempo | 293 |

---

# 3. La perilla es la única entrada

**El táctil está desactivado a propósito.** El panel lo tiene y funciona, pero se
apagó: en la biblioteca, un roce con el dedo arrastraba todo. Un objeto que se
maneja girando no debe reaccionar a que lo toques para acomodarlo.

Queda una sola excepción documentada: `display_touched()` existe para **despertar**
la pantalla, sin pasar por LVGL. Tocar despierta; no actúa.

## Los cuatro gestos

| Gesto | Umbral | Significado universal |
|---|---|---|
| **Girar** | 4 sub-pasos por muesca | Mover o cambiar valor |
| **Push** | — | Activar / entrar |
| **Doble push** | 260 ms de ventana | Siguiente |
| **Mantener** | 900 ms | Atrás |

**No se rotulan en pantalla.** Son convención del objeto, no instrucciones.

Dos decisiones que ya se probaron y no se re-litigan:

- **Mantener son 900 ms, no 3 segundos.** A tres segundos sueltas antes y el
  gesto se lee como push corto: el aparato se siente roto.
- **El push corto llega con 260 ms de retraso** — es la ventana del doble push.
  Se tapa con un evento inmediato al apretar que encoge la etiqueta, así que la
  mano recibe respuesta al instante aunque la acción tarde.

## El primer gesto solo despierta

Con la pantalla dormida o apagada, **girar o presionar únicamente la trae de
vuelta**. No cambia de disco, no pausa nada.

Porque alcanzas la perilla a ciegas para ver la hora, y no debe tener
consecuencias.

---

# 4. Mapa de navegación

Cinco pantallas. Cada una define qué significa el giro.

```
                    ┌──────────────┐
                    │  BIBLIOTECA  │  girar = disco
                    └──────┬───────┘
                push       │       mantener
          ┌────────────────┼────────────────┐
          ▼                ▼                ▼
   ┌─────────────┐  ┌────────────┐  ┌────────────┐
   │ REPRODUCIENDO│  │   ALARMA   │  │  AJUSTES   │
   │ girar=volumen│  │ girar=campo│  │ girar=campo│
   └─────────────┘  └────────────┘  └────────────┘

   Sin tocar la perilla el tiempo del reposo, desde cualquiera:
                    ┌──────────────┐
                    │    RELOJ     │  cualquier gesto = despertar
                    └──────────────┘
```

| Pantalla | Girar | Push | Doble | Mantener |
|---|---|---|---|---|
| **Biblioteca** | cambia de disco | entra | — | — |
| **Reproduciendo** | **volumen** | pausa / reanuda | siguiente canción | vuelve a la biblioteca |
| **Alarma / Ajustes** | campo, o valor si estás editando | alterna elegir/editar, y guarda | — | sale |
| **Reloj** | despierta | despierta | despierta | despierta |

La biblioteca es **circular**: pasado el último disco vuelve el primero. Nunca
hay tope, nunca hay que retroceder.

**Alarma y Ajustes son discos**, no menús escondidos. Se llega a ellos girando,
igual que a cualquier álbum. Es la razón por la que el aparato no tiene una
pantalla de configuración: no hay dónde esconderla.

## Cómo se distinguen biblioteca y reproducción

Con una sola perilla, confundirlas es cambiar de canción cuando querías subir el
volumen. Tres señales simultáneas, no una:

| | Biblioteca | Reproduciendo |
|---|---|---|
| Tamaño del disco | **62%** | **100%**, llena el cuadro |
| Discos vecinos | se asoman a los lados | no hay |
| El disco | quieto | **gira a 3 RPM** |

---

# 5. Tipografía

Montserrat, la de LVGL. **Escala completa en uso** — no se agregan tamaños
nuevos sin quitar otro:

| Tamaño | Uso |
|---|---|
| **22** | Título de pantalla |
| **20** | Nombre del disco, fecha y clima del reloj |
| **18** | Título de la canción |
| **16** | Campos de Ajustes y Alarma |
| **14** | Datos secundarios, batería, la cara analógica |
| **78** | La hora. Fuente generada aparte con `lv_font_conv`, solo dígitos |

## Reglas duras

**Todo en ASCII.** Montserrat de LVGL no trae acentos: `Corazón` sale `Corazon`.
El script normaliza solo. El grado (`°`, código 176) **sí** existe.

**Mayúscula inicial en cada palabra** para valores y opciones: `Reloj Digital`,
`Sin Bateria`, `Todos los Dias`. Todo en minúsculas se ve inacabado.

**VERSALITAS solo para nombres de disco** en la biblioteca: `MINECRAFT`.

## Jerarquía por opacidad, no por color

El color es información; la opacidad es jerarquía. Blanco siempre, y se gradúa:

| Opacidad | Qué es |
|---|---|
| 255 | El dato principal |
| 235 | Etiqueta del campo seleccionado |
| 175 | Dato disponible, no elegido |
| 150 | Contexto |
| **60** | **Un control que hoy no hace nada** |

---

# 6. Color

**Fondo negro.** Siempre.

**Ámbar `0xFF7A10`** es el único color de la interfaz. Marca lo que está vivo:
el valor que estás editando, el segundero, la temperatura.

Atravesando acrílico se lee como amplificador de bulbos. Un color que cambia con
el contenido se lee como periférico gamer, y el objeto va primero.

**Todo el demás color viene del arte del disco**, no de la interfaz. La
interfaz no inventa color.

Excepciones establecidas: gris `0x9AA0A6` para Ajustes y `0x6C7A89` para Alarma
cuando no tienen carátula propia — son discos que no son música.

---

# 7. Movimiento

Escala de tiempos en uso. **No inventes duraciones nuevas**, usa una de éstas:

| ms | Para qué |
|---|---|
| 120 | Aparecer algo que ya esperabas (el volumen al girar) |
| 180–200 | Salidas rápidas |
| 240–280 | El cambio estándar de pantalla |
| 300–320 | Entradas suaves |
| 420 | La cámara acercándose o alejándose del disco |
| 500 | Entrada al reloj |
| 900 | Amanecer de la alarma |
| 1200 | La pantalla retirándose a negro |

**Todo con `ease_in_out`.** Nada lineal.

## Asimetría deliberada

**Se va lento, vuelve rápido.** Retirarse debe sentirse como algo que el objeto
decide solo; encenderse, como respuesta inmediata a tu mano. Apagarse tarda
1200 ms; volver, 260.

## Zoom del disco

| % | Estado |
|---|---|
| 100 | Reproduciendo |
| 62 | Biblioteca |
| 20 | Reloj — el disco casi se retira |

Al pausar, el disco frena con inercia y **se asienta derecho**. Un disco parado
torcido se lee como falla.

---

# 8. Arte de los discos

La sección que faltaba. Antes de las reglas, lo que de verdad le pasa a tu
imagen — porque casi todas las reglas salen de aquí.

## Qué le hace el script a tu archivo

```
tu imagen  →  recorte cuadrado centrado
           →  escalado a 336 × 336
           →  transparencia compuesta sobre NEGRO
           →  RGB565 (16 bits)
           →  color dominante  →  etiqueta y acentos
           →  8 colores por sector  →  anillo de LEDs
           →  compilado dentro del firmware
```

Y en pantalla, el firmware **recorta un círculo**: todo lo que cae fuera del
radio se descarta píxel por píxel.

## Los cinco hechos que mandan sobre el arte

**1. Es UNA imagen para dos tamaños, no dos.** La misma carátula se ve a **336 px**
llenando la pantalla en la biblioteca, y **reducida a 200 px** como etiqueta que
gira mientras suena.

Por eso **no hay que hacer dos artes distintos** —uno de "etiqueta" y otro de
"cara completa"— como se venía haciendo. Hay un solo slot. El arte tiene que
funcionar en los dos tamaños, y eso significa: **si no se lee a 200 px, no
sirve.**

**2. Las esquinas se descartan, y su color da igual.** El recorte circular las
elimina. Que una portada tenga esquinas negras y otra blancas no importa —
ninguna de las dos se ve. Lo que importa es que **el motivo quepa dentro del
círculo inscrito**, porque lo que se salga se pierde.

**3. La banda del 45% al 95% del radio decide el color de los LEDs.** Ocho
sectores, empezando arriba y en sentido horario. Se promedia solo ese anillo,
porque es la zona que cada LED tiene enfrente.

Consecuencia de arte, no de código: **si el borde exterior es oscuro o de un solo
tono, el anillo sale apagado o aburrido.** Le pasó a la primera portada de Zelda
y quedó documentado en el código. El script sube saturación y luminosidad para
rescatarlo, pero rescatar no es lo mismo que tenerlo bien.

**Pon variedad de color hacia afuera.**

**4. Ni negro puro ni blanco puro cuentan como color dominante.** El extractor
ignora todo lo que tenga luminosidad bajo 28 o sobre 232. Una portada
completamente oscura o completamente clara **no tiene color dominante** y cae en
un promedio pardo.

**Necesita al menos un color saturado de luminosidad media.**

**5. Va a girar.** A 3 RPM, lentamente y para siempre. Composición radial,
equilibrada desde el centro. Un horizonte se ve raro girando; un motivo centrado,
no.

Y el centro exacto queda tapado por el agujero del eje: **nada crítico ahí**.

## Presupuesto

Cada carátula pesa **~220 KB** de firmware. La partición de 8 MB da para unos
**30 discos**. La música no cuenta — ésa vive en la tarjeta.

## El prompt

**Éste es el canónico, y no es teoría: es el que produjo la portada de Rock
Band**, que sigue siendo la mejor del catálogo — radial, legible reducida, y con
seis familias de color en el borde que le dan al anillo de LEDs algo con qué
trabajar.

Se usa tal cual. **Lo único que se cambia es la línea de `ESTILO:`.**

```
Ilustración cuadrada 1:1 para la cara completa de un disco de vinilo.
COMPOSICIÓN: toda la composición debe funcionar dentro del círculo inscrito
en el cuadrado. Las cuatro esquinas serán recortadas, así que deben quedar
vacías o con fondo plano. El motivo principal va centrado y contenido dentro
del 85% del diámetro. Composición radial y equilibrada desde el centro: la
imagen va a girar lentamente, así que debe verse bien en cualquier
orientación. El centro exacto quedará cubierto por el agujero del eje, así
que nada crítico justo en el punto medio. ESTILO: GTA, el juego. Formas
grandes y limpias, alto contraste, pocos elementos. Un color dominante
saturado que defina la pieza, y variedad de color hacia el borde exterior.
Sin texto, o a lo sumo una palabra corta y grande cerca del centro. EVITAR:
detalle fino, líneas delgadas, texto pequeño, texto en los bordes, marcos,
elementos en las esquinas, composición rectangular. SALIDA: 1:1, mínimo
1200x1200 píxeles.
```

Donde dice `ESTILO: GTA, el juego` va el tema del disco: `Minecraft, el juego`,
`rock de los setentas`, `paisaje de Hyrule`, lo que sea. **El resto no se toca**
— cada frase de ahí corresponde a una restricción real del aparato, y quitarla
rompe algo:

| Frase del prompt | Qué protege |
|---|---|
| "esquinas vacías o con fondo plano" | El recorte circular las descarta |
| "dentro del 85% del diámetro" | Lo que se salga del círculo se pierde |
| "radial y equilibrada desde el centro" | Gira a 3 RPM |
| "nada crítico justo en el punto medio" | Ahí va el agujero del eje |
| "formas grandes y limpias, pocos elementos" | Se ve reducida a 200 px |
| "un color dominante saturado" | Ni negro ni blanco puros cuentan como color |
| **"variedad de color hacia el borde exterior"** | **Alimenta los 8 LEDs** |

Ese último renglón es el que más se olvida y el que más se nota.

## Cómo verificar una portada antes de aceptarla

1. **Achícala a 200 px.** ¿Todavía se entiende? Si no, no sirve.
2. **Gírala 90°, 180°, 270°.** ¿Se ve bien en las cuatro? Si hay un "arriba"
   obligatorio, va a verse torcida la mayor parte del tiempo.
3. **Tapa el centro con un círculo.** ¿Se perdió algo importante?
4. **Mira solo el anillo exterior.** ¿Hay color, o es una banda oscura?

---

# 9. Lo que no se hace

- **No se rotulan los gestos en pantalla.** El usuario ya los conoce.
- **No se usan las esquinas.** No existen.
- **No se agregan tamaños de tipografía** fuera de la escala.
- **No se inventan duraciones** de animación.
- **No se usa color decorativo.** El color es información o es del arte.
- **No se hace nada táctil** salvo despertar.
- **No se muestra un control que no controla nada.** Tenue y se salta.
- **No se corta nada de golpe.**
