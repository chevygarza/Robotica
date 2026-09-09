# Prompt: agregar la app Ajustes a StuntHub

Copia todo lo que sigue como primer mensaje de una sesión abierta en
`Perilla-1.46/StuntHub/`.

---

Quiero agregar una app **Ajustes** a StuntHub, con el mismo diseño que la de
VinilOS. No es copiar la pantalla: es traer las decisiones que ya se pagaron
caro ahí.

**Lee primero, en este orden:**

1. `StuntHub/INTERFAZ.md` — el lineamiento de UI. La app nueva lo obedece completo.
2. `StuntHub/CLAUDE.md` y `Perilla-1.46/CLAUDE.md` — reglas duras del workspace.
3. `ESP32/Perilla-1.46/VinilOS/firmware/VNL1/settings.h` y `settings.cpp` — la implementación
   de referencia. **Cópiala y adáptala; no la reescribas desde cero.**
4. `ESP32/Perilla-1.46/VinilOS/README.md`, sección "Ajustes" — el porqué de cada decisión.

## Estado de hoy, verificado

- **No existe ninguna pantalla de ajustes.** Todo está hardcodeado.
- `SLEEP_MS = 30000` fijo en `StuntHub.ino:32`.
- El brillo es automático por horario: 70% de día, 25% de 10pm a 7am con NTP
  (`StuntHub.ino:199`).
- **VinilOS entró sin sus ajustes**: no hay `settings.h`, ni `ajustes()`, ni
  modos de LED, ni "Al Terminar". La app de Música corre con valores fijos.

## Dónde va cada cosa

Ésta es la decisión de arquitectura, y sale directo del lineamiento: *una idea
por pantalla*.

**App Ajustes — lo que afecta al aparato entero:**

| Campo | Opciones |
|---|---|
| Brillo | **Automatico** / 30–100% |
| Reposo | Nunca / 1 / 3 / 5 / 10 / 30 Minutos |
| En Reposo | Apagar / Reloj Digital / Reloj Analogo |
| Luz Reposo | 20–100% **del Brillo** |
| LEDs | Apagados / Amarillo / Caratula / RGB |
| Brillo LEDs | 10–100% |

**No pierdas el brillo automático que ya existe** — hoy sigue el horario por NTP
y eso está bien pensado. Que sea la primera opción del campo, antes de los
porcentajes fijos.

**Dentro de la app Música — lo que solo le concierne a ella:**

- **Al Terminar**: Detener / Repetir / Infinito

Va como renglón de su propio menú, no en Ajustes. Un ajuste global que solo
aplica a una app confunde a las dos.

**`LEDs: Caratula`** toma los ocho colores de la portada mientras hay música, y
**cae a ámbar cuando no la hay**. Es global porque el anillo es del objeto, no de
la app.

## Las diez decisiones que hay que traer

Esto es lo que pido de verdad. Cada una viene de un error ya cometido.

**1. Una idea por campo.** `LEDs` fusiona encendido y color; `En Reposo` fusiona
reloj sí/no y tipo. Antes eran cuatro campos y permitían estados que no
significan nada, como *"LEDs No, color Carátula"*. Fusionados, el estado
imposible no existe.

**2. Brillo es el techo real de todo.** Los niveles de reposo son una **fracción**
de él, no valores absolutos. Con dos niveles independientes, poner Brillo 50% y
Luz Reposo 80% dejaba la pantalla en reposo **más clara que usándola**. Siendo
relativo, ese caso no es alcanzable.

**3. Piso absoluto de 15% de pantalla.** Relativo por relativo aterriza en cero:
Brillo 50% × Luz Reposo 20% = 10%, y a 10% el panel deja de leerse. El piso es
absoluto a propósito — es una propiedad del hardware, no del gusto.

**4. Los valores guardados también respetan el mínimo del campo.** Éste es el
error más sutil y ya me costó: se puso el mínimo en la perilla pero no en NVS, y
un `0` heredado sobrevivió produciendo un reloj perfectamente invisible.
**Lo que la perilla no puede alcanzar, la memoria tampoco debe imponer.** Aplica
el clamp al cargar.

**5. Un campo que no controla nada se dibuja al 60 de opacidad y el giro lo
salta.** *Luz Reposo* con la pantalla en Apagar; *Brillo LEDs* con el anillo
apagado. Se ve que existe, pero no se puede elegir. No lo escondas: la lista
cambiaría de largo y perderías la referencia.

**6. Guardar al dormirse, no solo al pulsar.** Si te vas a media edición y entra
el reposo, el reposo **confirma** en vez de descartar. Nadie deja un campo a la
mitad esperando perderlo. Cuidado: hazlo en la **transición** a reposo, no en
cada pasada del loop, o escribes NVS miles de veces por minuto.

**7. El reposo tiene UNA condición: no tocaste la perilla.** Desde cualquier
pantalla, esté lo que esté corriendo. Nada de excepciones por estado — las tenía
antes y volvían el comportamiento impredecible.

**8. El anillo se apaga siempre al dormir**, tenga reloj la pantalla o no. El
reloj es información; el anillo es decoración del uso. Y se corta la corriente de
la tira (`GPIO17`), no solo el brillo: un LED en brillo 0 sigue alimentado y
sigue calentando dentro de una caja cerrada.

**9. La música no se detiene al dormir la pantalla.**

**10. Migración de llaves viejas de NVS**, y **bórralas después**. Si se quedan,
cada arranque vuelve a pisar lo nuevo. Aquí no hay llaves viejas todavía, pero
deja el patrón puesto para la próxima vez.

## Cómo debe verse

Obedece `INTERFAZ.md` sin excepciones: título capitalizado en `y=32`, primera
fila en `y=86`, filas de 32px, etiqueta en `x=52`, valor a la derecha terminando
en `x=308`, **máximo 7 filas y sin deslizamiento** si caben.

Son **seis campos**, así que caben enteros y la lista **no se desliza**. Que el
cursor se quede quieto mientras el mundo se mueve detrás es lo peor que puede
hacer una lista.

Selección por opacidad, sin marcos. Editando, el valor va en **ámbar
`0xFF7A10`**. Todo capitalizado y en ASCII: `Reloj Digital`, `Sin Bateria`,
`Automatico`.

**Todo se aplica mientras giras, no al salir.** Ajustar el brillo a ciegas y ver
el resultado después sería adivinar.

## El reloj de reposo

`En Reposo: Reloj Digital / Reloj Analogo` necesita una cara de reloj, y StuntHub
no tiene una desde que se podó la app de Clima. **Está escrita y probada** en
`ESP32/Perilla-1.46/VinilOS/firmware/VNL1/app.cpp` (busca `refreshReloj`, `relojBox`,
`rjHora`): digital y analógica compartiendo caja, con fecha y clima.

La hora usa una **Montserrat de 78 puntos generada con `lv_font_conv`**
(`vnl_reloj_78.c`, 76KB) porque LVGL solo trae hasta 48.

Antes de traerla, **mídeme cuánto flash queda** — ese es el criterio, no el
gusto. Si aprieta, dilo y lo dejamos en `Apagar` por ahora.

## Cómo quiero que lo hagas

Por etapas, cada una compilando:

1. **`settings.cpp/h` adaptado** de VinilOS, sin UI. Que cargue y guarde en NVS.
2. **Aplicar brillo, reposo y LEDs** desde los ajustes, quitando los valores
   hardcodeados del `.ino`. Todavía sin pantalla — se verifica por serial.
3. **La app Ajustes** en `app_ui.cpp`, siguiendo el patrón de navegación.
4. **"Al Terminar"** dentro del menú de Música.
5. **El reloj de reposo**, solo si el flash lo permite.

**Reglas del workspace que aplican:** no flashees sin que yo lo confirme en ese
mensaje; no refactorices de paso; no cambies versiones de librerías; documenta
cualquier gotcha nuevo en `StuntHub/CLAUDE.md`.

**Antes de escribir código, dime qué encontraste que contradiga lo que te acabo
de decir.** Estos datos salieron de leer, no de compilar.
