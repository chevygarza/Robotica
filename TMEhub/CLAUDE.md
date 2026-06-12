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
- **V_IDLE** naranja: "TME" + "Iniciar reseteo" + LED (verde=agente visible,
  rojo=sin WiFi o sin agente). Push solo arma si online.
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

Tactil SOLO despierta la pantalla (trapazos no disparan). Sleep 60s solo en
idle. Brillo fijo 80%.

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

## Pendientes
- Test en sitio: silence (push en Listo) y abort (5-push) — firmware y agente
  ya desplegados, falta validar en un reseteo real.
- IF IMAGE de "no hay datos" en el macro Jitbit.
- Instalar perilla en la caja cuando llegue la broca de 51mm.
