# StuntHub — Lineamiento de interfaz

El "HIG" de StuntHub. Hermano del de VinilOS (`~/Desktop/VNL1/docs/interfaz.md`):
**mismo ADN**, distinto contenido. Lo que aquí se repite es a propósito — los dos
firmwares corren en el mismo objeto y se manejan igual.

Escrito reconciliando lo que ya existe, no inventando. Cualquier pantalla nueva
debe respetar esto; romper una regla se puede, romperla sin documentar por qué,
no.

---

# 1. Principios

**Es un objeto, no una aplicación.** Se maneja con una mano, sin instrucciones.
Si algo necesita explicarse, está mal resuelto.

**La pantalla es el indicador de modo.** Con una sola perilla, el giro no puede
significar dos cosas en la misma pantalla — sería un modo invisible. Cambia la
pantalla, cambia el significado del giro. De aquí sale casi todo lo demás.

**Una idea por pantalla, una idea por renglón.** Si una pantalla hace dos cosas,
son dos pantallas. Si un campo controla dos cosas, son dos campos.

**Jerarquía por opacidad, no por color.** El color es información. El tamaño y la
opacidad son jerarquía.

**Nada se corta de golpe.** Todo aparece y desaparece con transición. Un corte
duro se lee como falla.

**Lo que no se puede hacer, no se ofrece.** Un control inactivo se dibuja tenue y
el giro pasa de largo.

---

# 2. El lienzo

Pantalla IPS **redonda de 360 × 360**. El cuadrado existe en memoria; el círculo
es lo único que se ve.

| Zona | Regla |
|---|---|
| Radio útil | **150 px** desde el centro |
| Esquinas | **No existen.** Nunca |
| Franja superior | Nada por encima de **y = 30** |
| Franja inferior | Nada por debajo de **y = 320** |
| Ancho de contenido | **≤ 280 px**, centrado |
| Alto de una caja | **≤ 200 px** |

**La cuerda se angosta rápido.** A 100 px del centro quedan 298 px de ancho; a
140 px quedan 226. Entre más arriba o más abajo, más corto el renglón.

**Prohibido `LEFT_MID` y `RIGHT_MID`.** Los costados de un círculo se curvan: lo
que alineas a la izquierda se sale por arriba y por abajo. Usa `CENTER` o
`TOP_MID` y resuelve el ancho con la caja.

## Retícula vertical

Todas las pantallas comparten estas alturas. **No inventes otras.**

| y | Elemento |
|---|---|
| 30 | Techo del área segura |
| **32** | Título de app |
| **60** | Subtítulo o estado permanente |
| **86** | Primera fila de lista |
| 180 | Centro óptico |
| **320** | Piso del área segura |
| −18 desde abajo | Indicador de apps (los puntos) |

Filas de lista: **32 px de alto**, etiqueta en `x = 52`, valor alineado a la
derecha terminando en `x = 308`.

---

# 3. Entrada: solo la perilla

**El táctil está apagado.** No es que no funcione — se apaga a propósito.

Un objeto que se maneja girando no debe reaccionar a que lo toques para
acomodarlo. En VinilOS pasó literal: un roce con el dedo arrastraba la
biblioteca entera. Y un atajo táctil que solo existe en una app enseña que la
pantalla a veces responde y a veces no, que es peor que nunca responder.

**Única excepción:** tocar **despierta** la pantalla dormida. Despierta; no
actúa.

> **Deuda pendiente al escribir esto:** los favoritos de Hue son táctiles y el
> giro está explícitamente deshabilitado ahí (`app_ui.cpp:840`). Al apagar el
> táctil quedan inalcanzables. Tienen que convertirse en lista navegable antes
> de apagarlo. Es el único lugar del firmware que depende del dedo.

## Los cuatro gestos

| Gesto | Umbral | Significado, en TODA la interfaz |
|---|---|---|
| **Girar** | 4 sub-pasos por muesca | Mover la selección o cambiar un valor |
| **Push** | — | Activar / entrar / confirmar |
| **Doble push** | 260 ms de ventana | Siguiente (solo donde hay una secuencia) |
| **Mantener** | **900 ms** | Atrás |

**No se rotulan en pantalla.** Son convención del objeto, no instrucciones. La
franja inferior curva donde vivían los hints es ilegible y por eso se quitó.

**Mantener son 900 ms, no 3 segundos.** A tres segundos sueltas antes y el gesto
se lee como push corto: el aparato se siente roto. Ya se probó.

## El primer gesto solo despierta

Con la pantalla dormida, girar o presionar **únicamente la trae de vuelta**. No
cambia de app, no activa nada. Porque alcanzas la perilla a ciegas y eso no debe
tener consecuencias.

---

# 4. El patrón universal de navegación

Éste es el ADN. **Toda app lo obedece, sin excepciones.**

```
   ┌─────────────────────────────────────────────┐
   │  HOME — el carrusel de apps                 │
   │  girar = cambia de app                      │
   │  push  = entra                              │
   └───────────────────┬─────────────────────────┘
                       │ push
                       ▼
   ┌─────────────────────────────────────────────┐
   │  MENÚ de la app — SIEMPRE una lista         │
   │  girar   = mueve la selección               │
   │  push    = activa o entra al siguiente nivel│
   │  mantener= regresa                          │
   └───────────────────┬─────────────────────────┘
                       │ push
                       ▼
   ┌─────────────────────────────────────────────┐
   │  ACCIÓN o sub-lista — mismas reglas         │
   └─────────────────────────────────────────────┘
```

## Las cinco reglas del patrón

**1. La portada de una app nunca actúa.** Push desde la portada **entra**, no
ejecuta. Hoy Mercados y Servidor refrescaban con push desde la portada: eso es
una acción escondida en un gesto de navegación, y se quita. Refrescar es un
renglón del menú.

**2. Adentro de una app, girar NUNCA cambia de app.** El giro pertenece a la
lista. Para salir se mantiene.

**3. Todo menú es una lista vertical.** Mismo componente, mismas medidas, misma
selección. No hay rejillas, no hay botones sueltos, no hay layouts propios por
app.

**4. Máximo tres niveles**: home → menú → sub-lista. Más profundo que eso y te
pierdes sin migas de pan, y en una pantalla redonda no caben migas.

**5. Mantener siempre sube un nivel**, nunca sale al home de golpe. Predecible
vale más que rápido.

## Estado por app

Una app **recuerda dónde estabas** mientras no salgas de ella. Si sales al home y
vuelves a entrar, **empieza desde arriba**. Entrar es entrar limpio.

---

# 5. Componentes

Cinco. **No se inventan más sin quitar uno.**

## 5.1 Título de app

`y = 32`, `CENTER`, **Montserrat 22**, blanco al 100%.

**Capitalizado, no VERSALITAS**: `Luces`, `Mercados`, `Pc Gamer`, `Música`,
`Fotos`. Hoy conviven `"Luces"`, `"MERCADOS"` y `"PC GAMER"` — tres criterios en
tres apps.

## 5.2 Estado permanente

`y = 60`, `CENTER`, **Montserrat 14**, blanco al **140**.

Un solo renglón que dice cómo está la app: `En Hora`, `Sin Red`, `4 de 44
Encendidos`. Es información, no un hint de gesto.

## 5.3 Lista

**El componente principal.** Todo menú es esto.

| Propiedad | Valor |
|---|---|
| Primera fila | `y = 86` |
| Alto de fila | **32 px** |
| Etiqueta | `x = 52`, Montserrat 16 |
| Valor | derecha, termina en `x = 308`, Montserrat 16 |
| Filas visibles | **7 máximo** |

Con más de 7 la lista se desliza para dejar la selección en la banda central.
Con 7 o menos **no se desliza**: que el cursor se quede quieto y el mundo se
mueva detrás es lo peor que puede hacer una lista.

### Selección

Sin marcos ni recuadros. **Opacidad y color:**

| Estado | Etiqueta | Valor |
|---|---|---|
| Seleccionado | 235 | **255** |
| Editando | 235 | **255, en ámbar** |
| Disponible | 150 | 175 |
| **Inactivo** | **60** | **60** |

Un renglón inactivo se dibuja al 60 —se ve que existe— y **el giro pasa de
largo**. No se esconde: la lista cambiaría de largo y perderías la referencia.

## 5.4 Confirmación

Para lo irreversible: apagar la PC, borrar algo.

**Push pide, segundo push confirma.** El renglón cambia a ámbar y dice qué va a
pasar. **Girar cancela** — moverse es arrepentirse. Nunca un diálogo modal: no
hay lugar para dos botones en un círculo.

## 5.5 Indicador de apps

Los puntos abajo, `-18` desde el borde inferior. **Solo en el home.** Adentro de
una app no se dibujan: ya no estás navegando apps.

---

# 6. Tipografía

Montserrat. **Escala de la familia — común con VinilOS:**

| pt | Uso |
|---|---|
| **22** | Título de app o pantalla |
| **18** | Dato principal dentro de una app |
| **16** | Filas de lista y campos |
| **14** | Estado, datos secundarios |

Y dos tamaños de **cifra**, exclusivos de StuntHub porque VinilOS no muestra
números grandes:

| pt | Uso |
|---|---|
| **48** | La cifra protagonista. **Máximo una por pantalla** |
| **28** | Valor principal de una tarjeta |

**Se eliminan el 12 y el 20.** El 12 está por debajo del piso legible a un brazo
de distancia, que es como se mira un objeto de escritorio; sube a 14. El 20 se
reparte entre 18 y 22 según sea dato o título.

Hoy hay **siete** tamaños en `app_ui.cpp`: 12, 14, 16, 18, 20, 28 y 48.

## Reglas duras

**Todo en ASCII.** Montserrat de LVGL no trae acentos: `Música` se escribe
`Musica`. El grado (`°`, código 176) **sí** existe.

**Capitalizado** en títulos, opciones y valores: `Reloj Digital`, `Todos los
Dias`, `Sin Bateria`. Todo en minúsculas se ve inacabado; todo en mayúsculas
grita.

**Números tabulares mentalmente**: si una cifra se actualiza en su lugar (reloj,
precio, porcentaje), no debe bailar. Ancho fijo o alineación a la derecha.

---

# 7. Color

**Fondo oscuro.** Decidido, y a conciencia.

> La regla 8 de `CROWN32/CLAUDE.md` dice *"fondos brillantes sólidos, el panel
> tiene mura visible en fondos oscuros"*. **Esa regla se actualiza, no se
> ignora.** La mura existe, pero después de meses de uso no estorba, y un
> dashboard de bunker con fondo claro es una lámpara en la cara de noche.
> **Hay que corregir el texto de la regla 8** para que el código y la regla digan
> lo mismo — hoy se contradicen, y eso es peor que cualquiera de las dos
> opciones.

## Paleta

| Rol | Hex | Regla |
|---|---|---|
| Fondo | `0x0B1020` | Toda pantalla |
| Superficie | `0x161C2E` | Tarjetas. **Solo si agrupa algo** |
| Texto | `0xFFFFFF` | Siempre blanco; la jerarquía va por opacidad |
| **Acento** | **`0xFF7A10`** | **Uno solo.** Lo vivo: lo que editas, lo que corre |
| Bien | `0x3DD68C` | **Estado**, nunca decoración |
| Mal | `0xFF5B6E` | **Estado**, nunca decoración |
| Atención | `0xFFB454` | **Estado**, nunca decoración |

**El acento pasa a ámbar `0xFF7A10`**, el mismo de VinilOS. Es el ADN de la
familia: atravesando acrílico se lee como amplificador de bulbos, no como
periférico gamer. El azul `0x4EA8FF` desaparece.

**Los tres colores de estado están reservados.** Verde significa "está bien", no
"es el tercer elemento". Si un color no comunica un estado, no se usa.

**Desaparece `COL_SUB`** (el gris `0x8A93A6`). Texto secundario = blanco con
opacidad 140-175, no un gris aparte. Un color menos y una escala más.

**Nunca escribas un hex suelto.** Hoy hay nueve `lv_color_hex(...)` repitiendo
valores que ya tienen constante.

---

# 8. Movimiento

**Todo con `ease_in_out`.** Nada lineal.

| ms | Para qué |
|---|---|
| 120 | Aparecer algo que ya esperabas |
| 180 | Salidas rápidas |
| **250** | **Cambio de pantalla. El estándar** |
| 300 | Entradas suaves |
| 900 | Amanecer (alarma, despertar suave) |
| 1200 | La pantalla retirándose a negro |

Los 250 ms del `lv_scr_load_anim` ya están parejos en todo el archivo. **Eso ya
estaba bien: no lo toques.**

## Asimetría deliberada

**Se va lento, vuelve rápido.** Retirarse debe sentirse como algo que el objeto
decide solo; encenderse, como respuesta inmediata a tu mano. Apagarse tarda
1200 ms; volver, 260.

## Dirección

Cambiar de app se desliza **en la dirección del giro**: horario entra por la
derecha. Entrar a un nivel más profundo **desvanece**, no desliza — profundidad
y lateralidad son ejes distintos y mezclarlos marea.

---

# 9. Reposo

Una sola condición: **no tocaste la perilla**. No importa en qué app estés ni qué
esté corriendo.

Al dormir se retiran **juntos** pantalla, anillo de LEDs y LED de encendido. Con
la pantalla en cero se **corta la corriente** de la tira (`GPIO17`), no solo el
brillo: un LED apagado por brillo sigue alimentado y sigue calentando dentro de
una caja cerrada.

**Dormir la pantalla no detiene lo que esté corriendo** — la música sigue, las
consultas de red siguen.

---

# 10. La excepción documentada: Música

**La app de Música no se conforma a este documento, y es a propósito.**

Es contenido a pantalla completa, no una tarjeta de datos: la carátula llena el
disco, el fondo es negro, y no hay título ni puntos de app. Meterla en una
tarjeta azul con su encabezado arruinaría lo único que esa app tiene que hacer.

Es la misma decisión que toma iOS: el chrome del sistema es consistente, la
reproducción de medios es inmersiva.

**Lo que sí comparte, porque es el ADN y no la piel:**

- Los cuatro gestos y sus umbrales
- Mantener = atrás, siempre
- El primer gesto solo despierta
- Cero táctil
- La escala tipográfica
- El ámbar como único acento

**Lo que no comparte:** fondo, retícula vertical, título, indicador de apps y el
componente de lista. Su lineamiento propio está en
`~/Desktop/VNL1/docs/interfaz.md`.

Cualquier app futura que sea **contenido inmersivo** —fotos, video— puede pedir
la misma excepción. Cualquier app que muestre **datos** no.

---

# 11. Lo que no se hace

- **No se rotulan los gestos en pantalla.** El usuario ya los conoce.
- **No se usan las esquinas**, ni `LEFT_MID`, ni `RIGHT_MID`.
- **No se agregan tamaños de tipografía** fuera de la escala.
- **No se inventan duraciones** de animación.
- **No se usa color decorativo.** El color es acento o es estado.
- **No se escriben hex sueltos.** Constante o nada.
- **No hay nada táctil** salvo despertar.
- **No se actúa desde la portada de una app.** Push entra.
- **No se muestra un control que no controla nada.** Tenue, y el giro lo salta.
- **No se corta nada de golpe.**

---

# 12. Qué hay que cambiar hoy

Distancia entre este documento y el código, en orden de riesgo.

> **Estado ago-2026: todo aplicado.** El documento se volvió código en
> `ui_theme.h` (tokens + los cinco componentes). Dos detalles que el checklist
> no anticipaba y quedaron resueltos:
> **mantener estaba en 600 ms**, no 900 — corregido en `StuntHub.ino`; y
> **`LEFT_MID`/`RIGHT_MID` siguen existiendo dentro de `uiRow`**, porque alinear
> a los costados de una caja acotada y centrada sí es válido: lo que la regla
> prohíbe es alinear contra el borde de la pantalla, que se curva. Ya no hay
> usos sueltos en `app_ui.cpp`.
> Sigue pendiente el arreglo de `powerUpScreen()` (ver `CLAUDE.md`), que no es
> de interfaz.

### Bloquea apagar el táctil

- [x] **Favoritos de Hue a lista navegable.** Hoy son táctiles y el giro está
      deshabilitado ahí (`app_ui.cpp:840`). Es lo único que rompe apagar el
      dedo — los modos de PC Gamer ya se recorren con la perilla.
- [x] Quitar los dos `lv_obj_add_event_cb` de `LV_EVENT_CLICKED`
      (`app_ui.cpp:376` y `391`).
- [x] Desactivar el `lv_indev` táctil, dejando solo el despertar.

### Consistencia visible

- [x] **Títulos capitalizados**: `"MERCADOS"` → `"Mercados"`, `"PC GAMER"` →
      `"Pc Gamer"`.
- [x] **Tipografía de siete tamaños a cuatro más dos de cifra.** Eliminar el 12
      (sube a 14) y el 20 (a 18 o 22 según sea dato o título).
- [x] **Acento a ámbar `0xFF7A10`.** Retirar `COL_ACCENT` azul.
- [x] **Retirar `COL_SUB`**; el texto secundario va por opacidad.
- [x] **Sustituir los nueve hex sueltos** por sus constantes.

### Estructura

- [x] **Push desde la portada entra, no actúa.** Mercados refrescaba con push
      desde la portada; refrescar pasa a ser un renglón de su menú.
- [x] **Retirar `LEFT_MID` y `RIGHT_MID`** (tres usos).
- [x] **Ocultar los puntos de app** dentro de una app.
- [x] Alinear cada lista a la retícula: primera fila en `y = 86`, filas de 32.

### Fuera de este repo

- [x] **Corregir la regla 8 de `CROWN32/CLAUDE.md`.** Hoy pide fondos brillantes
      y los dos firmwares usan oscuro. La regla debe decir lo que el código hace,
      con la razón.
