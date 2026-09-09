# Brief para diseño — TactOS (TAC-1)

Prompt para el equipo de diseño. Lo que sigue son las restricciones reales del
aparato y el ADN de la familia; el entregable es un lineamiento de interfaz
(HIG) con mockups de las pantallas listadas al final. Todo lo que aqui esta
medido sale del codigo o del hardware: no son preferencias, son limites.

---

## 1. Que es

Un panel tactil de escritorio de 3.5", horizontal, que vive siempre encendido
en el bunker. Tercer aparato de una familia que ya tiene lineamiento:
VNL-1 (VinilOS, tornamesa con perilla, pantalla redonda) y StuntHub (perilla
con apps). TactOS comparte su ADN y cambia el medio: aqui **si hay dedo** y
**si hay esquinas**. Referencia de tono: iOS. Un objeto, no una aplicacion:
se usa a un brazo de distancia, sin instrucciones, y lo que necesita
explicarse esta mal resuelto.

## 2. El lienzo (duro)

- **480 x 320 px**, IPS, RGB565 (65 mil colores; los degradados finos se
  tramean con ruido para que no salgan bandas).
- Sin esquinas redondeadas en el hardware. Marco negro alrededor.
- Un solo cuadro a la vez, unos 40 cuadros por segundo. Nada de video.
- Tactil capacitivo de un punto: **sin pinch, sin dos dedos.**
- Objetivo tactil minimo **44 x 44 px**. Filas de lista **44 px**.
- Margen exterior **24 px**. Ancho de contenido **432 px**, centrado.

## 3. Reticula vertical (una sola)

| y | Elemento |
|---|---|
| 22 | Titulo de pantalla |
| 52 | Estado permanente (un renglon: como esta la pantalla) |
| 80 | Primera fila / inicio del contenido |
| -14 desde abajo | Puntos del carrusel |

## 4. Tipografia (cerrada)

Montserrat. Solo estos tamanos, **sin acentos** (la fuente embebida no los
trae; el grado ° si):

| px | Uso |
|---|---|
| 22 | Titulo de pantalla |
| 18 | Dato principal |
| 16 | Filas y campos |
| 14 | Estado y datos secundarios |
| 28 | Valor principal de una tarjeta |
| 48 | La cifra protagonista: **una por pantalla** |
| 96 | La hora del Inicio (solo digitos) |

Capitalizado, no VERSALITAS ni minusculas: `Reloj Digital`, `Sin Red`.

## 5. Color

Fondo oscuro siempre. **Un acento.** El resto del color es estado.

| Rol | Hex | Regla |
|---|---|---|
| Fondo | `#0B1020` | Toda pantalla de datos |
| Superficie | `#161C2E` | Tarjeta o grupo, **solo si agrupa algo** |
| Texto | `#FFFFFF` | Siempre blanco; la jerarquia va por opacidad |
| **Acento** | **`#FF7A10`** | Ambar. Lo vivo: lo que editas, lo que esta corriendo |
| Bien | `#3DD68C` | Estado (conectado, subio) |
| Mal | `#FF5B6E` | Estado (bajo, fallo) |
| Atencion | `#FFB454` | Estado (lluvia probable) |

No existe un gris de texto. Jerarquia por opacidad sobre blanco:

| Opacidad | Que es |
|---|---|
| 100% | El dato principal |
| 69% (175) | Disponible, no elegido |
| 59% (150) | Contexto |
| 55% (140) | Renglon de estado |
| 24% (60) | Un control que hoy no hace nada (se ve, no actua) |
| 8% / 6% | Superficie tactil sobre wallpaper / fondo de tecla |

El Inicio es la unica pantalla con wallpaper (contenido inmersivo, como la
Musica en StuntHub). Wallpaper actual: negro con un resplandor ambar abajo a
la izquierda. Toda pantalla que muestre datos va sobre el fondo solido.

## 6. Movimiento (escala cerrada, todo ease-in-out)

| ms | Para que |
|---|---|
| 120 | Aparecer algo que ya esperabas |
| 180 | Salidas rapidas |
| 250 | Cambio de pantalla: **el estandar** |
| 260 | La luz volviendo a tu mano (despertar) |
| 300 | Entradas suaves |
| 900 | Amanecer (cambio de brillo dia/noche) |
| 1200 | La pantalla retirandose |

Asimetria deliberada: se va lento, vuelve rapido. Nada se corta de golpe.
Cambiar de app desliza lateral; entrar a un nivel mas profundo desvanece;
Ajustes baja como una hoja desde arriba.

## 7. Gestos y navegacion

- **Deslizar izquierda / derecha**: siguiente / anterior app. Circular.
- **Deslizar hacia abajo** (desde cualquier app): Ajustes. Hacia arriba lo cierra.
- **Tocar**: entrar o actuar. **La flecha** (arriba a la izquierda) sube UN nivel.
- **Tres niveles maximo**: carrusel -> pantalla -> sub-pantalla.
- **Lo irreversible pide dos toques**: el primero cambia la fila a ambar y dice
  que va a pasar ("Tocar para apagar"); el segundo confirma; en 4 s vuelve sola.
  Nunca un dialogo modal.
- **Reposo**: 3 minutos sin tocar, la luz baja a un resplandor. El primer
  toque solo despierta, no actua.
- Brillo automatico: 70% de dia, 25% de noche (10 pm a 7 am).

## 8. Componentes (cinco; no se inventan mas sin quitar uno)

1. **Titulo** (22, y=22, centrado).
2. **Estado** (14, y=52, centrado, 55%). Informacion, nunca un hint de gesto.
3. **Lista / Grupo**: filas de 44 px; etiqueta a la izquierda, valor a la
   derecha al 69%, chevron al 24% si navega; separador de 1 px al 6%. En
   grupo, sobre superficie con radio 14 (estilo iOS).
4. **Tarjeta**: superficie, radio 14; agrupa simbolo + valor + cambio.
5. **Pastilla**: estado tactil sobre el wallpaper (icono + texto, radio 18,
   fondo blanco al 8%).
   Ademas: slider (pista al 8%, recorrido en ambar, perilla blanca de 22 px) y
   teclado (teclas blancas al 6%, radio 10; la tecla de confirmar en ambar).

## 9. Pantallas que existen hoy (para mockup)

1. **Inicio**: wallpaper; hora 96 px a la izquierda; fecha 18 debajo;
   pastilla de red arriba a la derecha (icono verde = conectado, ambar =
   conectando, tenue = sin red); puntos del carrusel abajo.
2. **Clima**: temperatura 48 px + condicion; sensacion, humedad, viento,
   max/min; cinco columnas con las proximas horas (hora, temp, % lluvia; el %
   en color atencion si es 50 o mas).
3. **Mercados**: tres tarjetas BTC / ETH / SOL: simbolo 22, precio 28, cambio
   24 h en verde o rojo. Estado: "USD · 22:41".
4. **Ajustes** (hoja desde arriba): grupo de cinco filas: Wi-Fi (chevron, valor
   = red actual), Brillo (slider + "Auto" tocable en ambar), Volumen (inactivo:
   "Sin Audio"), Apagar, Reiniciar.
5. **Red Wi-Fi**: lista de redes (senal por opacidad; la actual en verde);
   clave con campo y teclado; conectando con spinner ambar; vuelve sola.

## 10. Lo que viene (para que el sistema lo aguante)

Luces Hue (cuartos, focos, interruptor y brillo), PC Gamer (prender, apagar,
perfiles), Musica desde tarjeta o Spotify Connect con caratula a pantalla
completa, Fotos, y un Inbox de avisos. Cada una es una app del carrusel con
titulo, estado y contenido; las inmersivas (Musica, Fotos) pueden romper el
fondo solido, como la Musica en StuntHub.

## 11. Entregable esperado

- Lineamiento (HIG) que reconcilie estos limites con el tono iOS: reticula,
  tipografia, color, opacidad, movimiento, gestos, componentes.
- Mockups 480 x 320 de las cinco pantallas, en Figma, con los componentes
  como instancias reutilizables.
- Un wallpaper alternativo dentro de la paleta (negro + ambar), y una
  propuesta de iconografia para el carrusel si el lineamiento la pide.
- Todo en ASCII, sin acentos en textos que vayan a pantalla.
