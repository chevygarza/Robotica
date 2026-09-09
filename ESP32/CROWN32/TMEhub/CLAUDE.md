# TMEhub — Perilla industrial de reseteos (empresa TME)

Firmware en PRODUCCION: los operadores resetean ECUs de camiones Cummins con
una perilla. Antes de tocar nada: lee `../CLAUDE.md` (reglas duras + build).

## Que hace
Una sola funcion: disparar y monitorear el reseteo que corre en una mini PC
Windows. Pantalla naranja TME (#E8631A) minimalista. La perilla habla por
WiFi con un agente HTTP en la mini PC.

```
Operador: push -> "Iniciar reseteo?" -> push -> barra de progreso en vivo
(espeja la barra del PC: check1 40%, check2 40-100% + ETA, frases rotativas)
-> "Listo! Gracias, operador" -> push = acknowledge (calla alarma MP3
y cierra dialogos en el PC)
```

## El stack completo
```
Perilla (este firmware, WiFi "TME PATIO")
  -> tme_agent.ps1 en la mini PC (puerto 8766; ver agents/)
       GET  /status   espeja barra_progreso.ps1 leyendo %TEMP%\TME_BAR_SIGNAL.txt
       POST /reset    borra signal viejo + lanza PC26_V1.exe (anti-duplicado)
       POST /silence  mata vlc (FIN.mp3 en loop) + dialogos listo/error
       POST /abort    RESCATE: mata PC26_V1 + barra + dialogos + borra signal
  -> PC26_V1.exe = macro Jitbit (fuente Power_Spec_26.mcr, lic c.egarza@gmail.com)
       automatiza Cummins PowerSpec v14.1.1 + INSITE 7.6.2.240
       escribe señales: check1 / check2 / error / done al signal file
```

## UI: vistas y gestos (tme_ui.cpp)
- **V_IDLE** dos caras (idleSetOnline togglea, ambas siguen siendo V_IDLE):
  ONLINE = naranja + "TME" + "Iniciar reseteo" + circulo verde GRANDE 64px.
  OFFLINE = rojo + "TME" + "Favor de usar pantalla interna o contactar a
  administrador". Push solo arma si online.
- **V_CONFIRM**: push 2 = dispara; timeout 10s; push largo cancela.
- **V_RUN**: barra blanca (creep hasta 38% mientras el agente confirma, luego
  % real), label del paso, "faltan mm:ss", frases cada 6s.
  **5 pushes seguidos (<1.5s) = ABORT** (rescate para macros atorados; con
  feedback "Cancelar: N toques mas" desde el 2o push; cooldown 12s para no
  re-entrar). Sin escape por push largo (rebotaria con estado stale).
  Timeout 20s sin confirmacion del agente -> V_ERROR.
- **V_DONE** naranja: push = silence + home. Solo se entra si sawAgentRun
  (evita el "done" stale del reseteo anterior).
- **V_ERROR** ROJO solido (unica pantalla no-naranja, alarma intencional):
  "Reporta el error a administracion". Push = silence + home.
- **AUTO-PUSH (V_DONE/V_ERROR)**: si nadie hace acknowledge en 10 min, se
  dispara silence solo (calla alarma + cierra dialogo en la PC) y vuelve a
  home -> deja dormir. Los operadores olvidan el ultimo push y la alarma
  sonaba toda la tarde hasta el reinicio nocturno de la PC. (endScreenT + 600s).

Sleep 60s solo en idle. Brillo fijo 55% (BL_PCT; menos calor/desgaste 24/7).
El TOUCH ya NO despierta la pantalla (ver gotcha del fantasma): solo la
perilla (girar/push) es fuente de wake.

## Red en TME (critico — saga jun-2026)
- Modem Telmex: LAN cableada **192.168.86.x** (mini PC por cable = .86.53).
- Google WiFi "TME PATIO" (pass en secrets.h): reparte **192.168.87.x**.
- AISLADAS entre si. La perilla vive en .87 => habla a la mini PC por su
  WiFi: **TME_AGENT_HOST = 192.168.87.23** (RESERVADA en Google Home).
- La mini PC tiene resiliencia instalada: connectionmode=auto,
  fMinimizeConnections=0 (Windows ya no mata WiFi por tener cable),
  tareas "WiFi Watchdog Boot" (ONSTART) y "WiFi Watchdog" (c/5min),
  "TME Agent" (ONLOGON) y "TME Agent Watchdog" (c/5min). Auto-login sin pass.
- La mini PC se auto-reinicia seguido POR DISEÑO: todo lo anterior existe
  para sobrevivir eso sin manos.

## Resiliencia en el firmware (tme_net.cpp)
- Poll /status cada 1.5s; OFFLINE tras 8s sin respuesta.
- WiFi.reconnect() cada 2s si se cae.
- ANTI-ZOMBIE: >60s sin ver agente con WiFi "conectado" => WiFi.disconnect(true)
  + begin() forzados (cada 60s max).

## Gotchas propios
- TOUCH FANTASMA (cst816t): el chip dispara toques que nadie hizo, en rafagas
  intermitentes (defecto electrico de la linea INT; no es mugre). Cada fantasma
  refrescaba g_lastActivity -> la pantalla NUNCA dormia. FIX: my_touch_read ya
  NO actualiza g_lastActivity (touch fuera del wake; solo perilla despierta).
  El sintoma es intermitente: a veces dormia, a veces no -> no te confies de
  "ya jala", el fix es de raiz.
- ALIMENTACION: usar CARGADOR DE PARED (5V), NUNCA el USB de la PC. El USB-CDC
  nativo del S3 + un host que enumere el puerto (cada reinicio de la PC) puede
  meterla en MODO DOWNLOAD (pantalla negra, sin UI). La perilla habla por WiFi,
  no necesita el cable de datos para nada. Cargador en la misma regleta, aislada.
- MODO DOWNLOAD al leer serial: ver regla dura #7. En el Mac Mini de la oficina
  el flasheo/serial mete la placa en download facil; en la laptop de Jose
  siempre funciona bien -> flashear desde la laptop. Revivir = reflashear.
- HTTP.sys de Windows rechaza POST sin cuerpo (411) => sendRequest("POST", "{}").
- Señal "Weak" del WiFi de la mini PC en Google Home: vigilar cuando todo
  quede dentro de la caja metalica (si hay drops, considerar 2.4GHz/reposicion).
- El detector de error del macro (IF IMAGE) NO cubre "no hay datos que bajar"
  (camion ya en 0km) => el macro truena sin señal y la perilla quedaria
  esperando: para eso existe el 5-push. PENDIENTE: agregar ese IF IMAGE al
  macro Jitbit para que escriba 'error' al signal.
- El monitor DUMMY de la mini PC NUNCA se quita (el macro clickea coordenadas;
  sin monitor cambia la resolucion y todo muere). El touchscreen vertical es
  respaldo temporal; su touch se mapea con `control /name Microsoft.TabletPCSettings`
  -> Setup (tocar SOLO cuando el texto este en la pantalla correcta).

## Deploy fisico (plan)
Caja tipo registro con ventilacion; perilla en la tapa (agujero 51mm,
pendiente broca). Dummy + mini PC adentro. Touchscreen vertical de respaldo
~1 mes y se retira.

## Ideas descartadas (referencia futura)
- LEDs AMBIENTALES naranja en reseteo (2026-jun): la CrowPanel tiene 8 NeoPixel
  (data GPIO48 = PIN_RGB_DIN, power GPIO17 = PIN_RGB_PWR). Idea: encenderlos
  naranja en V_RUN como senal visible desde lejos. **DESCARTADO**: son "fase
  posterior" (atras de la placa) -> al montar la perilla en la tapa de la caja
  el halo queda ADENTRO, no se ve. Si se retoma (otra carcasa con borde
  translucido): Adafruit_NeoPixel 1.15.1, strip(8, PIN_RGB_DIN, NEO_GRB+NEO_KHZ800),
  PIN_RGB_PWR HIGH, strip.setBrightness(110), strip.Color(255,50,0) = naranja
  calido VALIDADO en hardware (a brillo pleno se "lava" a blanco). Control:
  ui_led_mode()==1 en V_RUN; ledsApply() en loop + ledsApply(true) en screenWake;
  strip.begin/clear/show en setup (mata la basura inicial de los LEDs). El codigo
  completo estuvo en el working tree, revertido tras f987f63. ALTERNATIVA para
  senal FRONTAL (si se ve montada): pulsar/parpadear el fondo de pantalla en V_RUN.

## Pendientes
- Test en sitio: silence (push en Listo) y abort (5-push) — firmware y agente
  ya desplegados, falta validar en un reseteo real.
- IF IMAGE de "no hay datos" en el macro Jitbit.
- Instalar perilla en la caja cuando llegue la broca de 51mm.
