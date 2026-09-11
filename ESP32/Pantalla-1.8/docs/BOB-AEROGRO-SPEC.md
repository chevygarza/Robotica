# Bob Aerogro — guía de diseño y comportamiento

Documento autocontenido para reproducir a Bob en cualquier pantalla (panel de
cocina, app, asistente de voz). No hacen falta imágenes: todo está en fórmulas
y colores exactos. Referencia viva (misma geometría, en el navegador):
https://claude.ai/code/artifact/7ad4c6df-9f44-403a-b180-5e0909e5d1cc
Implementación de referencia: `docs/bob-aerogro-mock.html` (HTML + canvas, ~250 líneas).

## 1. Identidad

- Bob es un **blob plano de un solo color** sobre **fondo negro puro**, con **dos
  ojos blancos en forma de píldora**. Sin boca, sin contorno, sin degradados,
  sin sombras. Toda la expresión sale de la **forma**, el **color**, los
  **ojos** y el **movimiento**.
- Tiene **seis ánimos**; cada uno es una forma y un color distintos. Nunca
  "salta" de uno a otro: **morfea** (interpola contorno y color) en ~300 ms.
- **Vive solo**: cuando nadie lo estimula, respira, parpadea, mira alrededor y
  cada pocos segundos hace algo por su cuenta (§5). Nunca está quieto del todo.
- Personalidad: simpático, curioso, un poco travieso. Nunca agresivo: el enojo
  dura menos de un segundo.

## 2. Los seis ánimos

| Ánimo | Nombre | Color | Forma polar r(θ) (θ en radianes, 0 = derecha, sentido antihorario) | Inclinación | Ojos (h, w, gap, giro) | Cuándo |
|---|---|---|---|---|---|---|
| IDLE | Verde píldora | `#2ECC71` | superelipse(a=1.28, b=0.82, n=3.6) | −10° | 0.30, 0.12, 0.40, 0° | base |
| LISTENING | Azul triángulo | `#2196F3` | 1.02·(1 + 0.17·cos(3(θ − π/2))) | 0° | 0.40, 0.13, 0.34, 0° | escucha |
| TALKING | Naranja nube | `#FF6A00` | superelipse(1.2, 0.9, 2.2)·(1 + 0.13·cos(4θ + π/4)) | 0° | 0.32, 0.12, 0.38, 0° | habla |
| SURPRISED | Café flor | `#9C6B36` | 1.1·(1 + 0.2·cos(5θ + π/2)) | 0° | 0.44, 0.14, 0.36, 0° | sorpresa / te necesita |
| ANGRY | Negro redondo | `#2C2C31` + anillo `#5B5B63` | 1.0 (círculo) | 0° | 0.16, 0.12, 0.40, 22° hacia adentro | enojo / error |
| SLEEPY | Gris gota | `#8A8C90` | 0.98·(1 + 0.45·u⁴), u = max(0, cos(θ − π/2)) | −8° | 0.07, 0.13, 0.38, 0° | dormido |

- `superelipse(a, b, n)(θ) = (|cos θ / a|ⁿ + |sin θ / b|ⁿ)^(−1/n)`.
- Los ojos se expresan como **fracciones del radio base R**: alto `h`, ancho
  `w`, separación del centro `gap` (cada ojo a ±gap·R), giro (el negro inclina
  cada ojo 22° hacia el centro: ceño).
- Sobre fondo negro, el **negro** necesita el anillo (contorno relleno al 104 %
  del tamaño en `#5B5B63`) para existir.

## 3. Geometría exacta

- Contorno: **N = 72 puntos** muestreados en θ = i·2π/N, radio `r(θ)·R`.
  Relleno plano (polígono). Sirve para formas cóncavas (nube, flor).
- **R = 0.32 · ancho de pantalla** en vertical (en 368×448, R = 118 px). Todo
  lo demás escala con R. En un panel grande usa `R = 0.30 · min(ancho, alto)`.
- Centro: horizontal al centro; vertical al centro + 0.02·alto (un poco abajo).
- Ojos: dos **cápsulas blancas** (`#FFFFFF`): trazo grueso de ancho `w·R` con
  puntas redondas, alto `h·R`, a ±`gap·R` del centro y `0.06·R` arriba del
  centro. Miran desplazándose hasta ±0.16·R en x y ±0.12·R en y.
- La pantalla se puede girar (vertical/horizontal): Bob siempre queda "de pie"
  respecto a la gravedad.

## 4. Animación base (siempre, en cualquier ánimo)

- **Respira**: escala x = 1 + 0.012·sin(2.2·t), escala y = 1 − 0.012·sin(2.2·t).
- **Rebota**: centro y += 3 px·sin(2.2·t) (escala con R/118).
- **Parpadea**: cada 1.6–4.1 s (aleatorio), ojos a h = 0.04 por 110 ms.
- **Mira alrededor**: cada frame con 2 % de probabilidad elige un punto al azar
  (x ∈ [−1, 1], y ∈ [−0.6, 0.6]) y lo mira 0.7–1.6 s; luego vuelve al centro.
- **Morfeo**: cada frame, contorno y color se interpolan hacia el objetivo con
  k = clamp(dt·8, 0.05, 0.45) (≈300 ms para llegar). Ojos: k = clamp(dt·14, 0.05, 0.6).
- **Vitalidad** (0..1, opcional): al bajar, el color se desatura hacia gris y
  se apaga (factor 0.45 + 0.55·v). Sirve para "está descuidado".

Movimiento propio de cada ánimo:

| Ánimo | Cuerpo |
|---|---|
| LISTENING | escala y 1.04, x 0.97 (se estira atento), rebote suave 3 rad/s |
| TALKING | vibra: escalas ±(0.06 + 0.10·nivelVoz) a 22 rad/s, rebote 5 px a 11 rad/s |
| SURPRISED | se infla: x 1.10, y 1.12, salta −8 px |
| ANGRY | se aplasta: x 1.03, y 0.94, tiembla 2 px a 30 rad/s |
| SLEEPY | se aplasta: x 1.05, y 0.93, rebote lento 1.2 rad/s + 6 px abajo |

## 5. Vida propia (director de conducta)

Cuando está en IDLE y nadie lo toca, cada 2.5–7.5 s elige un acto (menos espera
si está contento). Duración y peso relativo:

| Acto | Qué hace | Dur. | Peso |
|---|---|---|---|
| estirarse | y ×1.18, x ×0.90, ojos cerrados a la mitad, sube 10 px | 1.5 s | 1.0 |
| brincar | 3 saltos de 32 px con aplastón al caer | 1.3 s | 1.2 |
| pasear | se desliza a un punto ±70 px en x, inclinándose hacia donde va | 2.2 s | 1.4 |
| rodar | gira 360° sobre sí mismo mientras se desplaza | 1.8 s | 0.7 |
| guiñar | cierra un ojo | 0.45 s | 1.0 |
| curiosear | se vuelve azul, mira a un lado, "?" | 2.5 s | 1.0 |
| reírse | se vuelve naranja, vibra, "ja" "je" | 1.6 s | 0.8 |
| soñar despierto | mira arriba-derecha, se mece lento | 3.5 s | 1.0 |
| siesta | gris gota, ojos cerrados, "Zz" | 5 s | 0.5 |
| esconderse | se encoge al 25 % y reaparece con ojos grandes | 1.6 s | 0.6 |
| temblar | vibra fino 3 px | 0.8 s | 0.5 |
| bailar | rebota 14 px, se inclina ±18°, y **pasa por los seis colores/formas** rápido | 3.6 s | 0.5 |

Cualquier estímulo externo (voz, toque, evento) cancela el acto en curso.

## 6. Partículas (la prueba de que "siente"). Muy sutiles.

Sin canal alfa: se desvanecen **oscureciendo hacia el negro** en la segunda
mitad de su vida.

| Momento | Efecto |
|---|---|
| come | 3 migajas cada 120 ms (puntos 2–4 px, color del cuerpo y ámbar `#FF7A10`) que saltan y caen (gravedad) |
| dormido | cada 1.3 s una "z" (40 % "Z" grande) gris `#9AA3B0` que flota arriba-derecha 1.9 s |
| mimo | 3 corazones rosa `#FF5C8A` que suben |
| cosquillas | "ja" / "je" amarillo `#FFD166` brincando |
| sorpresa | un "!" grande amarillo `#FFF3A0` sobre la cabeza |
| curiosea | "?" azul claro `#7FC4FF` |
| baila / logro | confeti de 6 colores cayendo desde arriba |
| necesidad crítica | cada 12 s: "nom?" ámbar, "meh" gris, "<3?" rosa |

## 7. Bob como cara de un asistente de voz (panel de cocina)

Mapa de estados del asistente → ánimo de Bob:

| Estado del asistente | Bob | Detalles |
|---|---|---|
| Inactivo (sin uso 20 s+) | SLEEPY | Zz cada 1.3 s. En un panel puede bajar el brillo. |
| Despierta (tap / palabra clave) | brinco + IDLE | salto de 32 px, ojos grandes un instante, globo "Hola!" opcional |
| Escuchando al usuario | LISTENING | ojos grandes; el cuerpo se estira con el nivel del micrófono (escala y = 1.04 + 0.08·nivel) |
| Pensando (esperando al modelo) | LISTENING sin mic + "..." | mira arriba-derecha; "..." azul cada 2 s. Sin travesuras |
| Hablando (TTS) | TALKING | vibra con la **amplitud del audio de salida** (0..1): escalas ±(0.06 + 0.10·nivel) |
| Terminó de hablar | IDLE | vuelve a verde y a su vida propia |
| Necesita algo del usuario (confirmar, permiso) | SURPRISED | "!" y rebote hasta que respondan |
| Error / no entendí | ANGRY 1 s → IDLE | anillo gris, ojos ceñudos; luego un guiño para quitar hierro |
| Logro / tarea completada / buena noticia | bailar | confeti 3.6 s |
| Notificación entrante | curiosear + globo | "?" y el texto en un globo blanco arriba (fuente ×2, máx. 2 líneas) |

Reglas de tono:
- Los cambios de estado **siempre morfean** (300 ms); nunca cortes secos.
- Mientras habla, **la boca no existe**: la voz se ve en la vibración del cuerpo
  y en la forma naranja. No dibujar labios ni ondas de audio "técnicas".
- Ningún ánimo negativo dura más de 1.5 s salvo que el usuario insista.
- Si no pasa nada en 20 s, se duerme; si nadie lo toca en horas, sigue viviendo
  (actos de §5) pero más lento y con menos color (vitalidad).

## 8. Interacciones táctiles (si el panel es táctil)

| Gesto | Significado | Reacción |
|---|---|---|
| tap | mimo / despertar | entrecierra ojos felices, sube 6 px, corazones |
| doble tap | comer | mastica (aplastones y ×16 rad/s), migajas |
| deslizar | cosquillas | vibra fino, "ja ja" |
| mantener | mostrar estado | pantalla de barras estilo videojuego (§9) |

## 9. HUD de estado (opcional, estilo videojuego)

Pantalla negra con doble marco redondeado gris `#3A4150`. Título "BOB" en
verde `#2ECC71` grande y "DIA n". Cuatro barras de vida **segmentadas** (10
bloques, esquinas redondeadas): HAMBRE, ENERGIA, DIVERSION, AMOR, con etiqueta
en fuente chunky y número a la derecha. Color por nivel: >50 % verde `#2ECC71`,
>20 % ámbar `#FFB020`, si no rojo `#FF4040`. Bloques vacíos `#141820` con borde.

## 10. Umbrales de referencia (sensores)

- Micrófono (RMS normalizado 0..1, suavizado con ataque rápido / caída lenta):
  > 0.30 escucha, > 0.55 habla.
- Sacudida (|Δaceleración| en g): > 0.55 sorpresa, > 1.4 enojo.
- Inclinación (aceleración en x, en g, ×1.4): mueve la mirada.
- 20 s sin estímulos → dormido.
