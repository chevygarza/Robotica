# CLAUDE.md — Proyecto Perillas (StuntHub + TMEhub)

Runbook completo del proyecto. Leelo COMPLETO antes de tocar nada. Cualquier sesion
nueva (incluyendo el Mac Mini de trabajo) debe poder seguir el proyecto a partir
de este archivo.

## Quien soy yo (el usuario)
- **Jose** (stuntech en X), Monterrey, MX. Hablamos en español.
- Hobbyista nivel maker; sabe codigo a nivel arquitecto, no soldador.
- Pidio que NO le des resumenes largos al final de cada respuesta — directo al grano.
- Workflow preferido: yo (Claude) hago, el aprueba, no le gusta debuggear UI manualmente — confiar en mis cambios y solo reflashear cuando algo se nota raro.
- Tiene un Mac Mini servidor 24/7 que el llama "tu Mac Mini" — corre cron+launchd, sirve health.json, tiene agentes Claude. Lo usa de cerebro de la red.

## Hardware (el cuerpo)
Dos perillas (potencialmente, hoy hay UNA placa + una #2 por comprar):
- **Elecrow CrowPanel ESP32-S3 1.46" round** (ESP32-S3R8, 16MB Flash, 8MB PSRAM, IPS 360x360, panel ST77961, touch cst816t en GPIO 13/5, rotary encoder en 45/42/41, LED power 40, RGB power 17, backlight 46 PWM ch 0). `firmware/StuntHub/board.h` y `firmware/TMEhub/board.h` son IDENTICOS — mismo hardware.
- Sin IR (ya lo descartamos, ver decisiones).

## Dos firmwares (un solo cuerpo, dos almas)
La placa actual de Jose puede correr cualquiera de los dos. Cuando llegue la CrowPanel #2, TMEhub vive alla y StuntHub vuelve a la de Jose.

### `firmware/StuntHub/` — perilla PERSONAL de Jose (su bunker)
7 apps, navegacion: girar perilla cambia app (lv_scr_load_anim), push corto =
entrar/accion, push largo = atras. Sleep 30s, brillo nocturno (70% dia / 25% 10pm-7am
via NTP), tactil solo despierta (no hace acciones — ambiente domestico ok pero
mantenemos consistencia con TMEhub).

Apps (NUM_APPS=7):
| # | App | Datos | Push |
|---|-----|-------|------|
| 0 | Clima MTY | Open-Meteo (lat 25.6866, lon -100.3161, TZ CST6); reloj NTP 12h con AM/PM | entrar a detalle (lluvia 5h con hora y prob) |
| 1 | X (@stuntech) | api.x.com/2/users/by/username + public_metrics, refresh a las 7am | entrar a detalle (bio) |
| 2 | Luces (Hue) | LAN local: hue bridge 192.168.86.49, key XkSk...; 44 focos / 17 cuartos; favoritos BUNKER: Relax/Blanco/Fiesta (Fiesta=2red+2blue) | nav 5 vistas: cover->favoritos->cuartos->focos->control |
| 3 | Mercados | CoinGecko (BTC/ETH/SOL), refresh 5min, retry 30s tras fallo | refresh manual |
| 4 | Servidor | health.json del Mac Mini: status/load/uptime/RAM/SSD/red/thermal | refresh manual |
| 5 | Wallpapers | GIFs DBZ/Pokemon/Zelda (180x180, lv_gif_create + zoom 2x = 360px full); creacion perezosa (wallShow: solo existe en app 5; ui_screen_off/on lo destruye al dormir) | siguiente GIF |
| 6 | PC Gamer | gamer_pc.online del health.json | bidireccional: apagada=>WoL magic packet directo UDP, encendida=>POST /shutdown al pc_agent |

Modulos: `app_net.cpp` (clima+X), `app_hue.cpp`, `app_markets.cpp`, `app_server.cpp`, `app_wol.cpp` (WoL+shutdown), `app_ui.cpp`, `wallpapers_data.c` (GIFs embebidos).

### `firmware/TMEhub/` — perilla INDUSTRIAL (empresa TME)
1 sola accion: reseteo de camiones. Pantalla naranja TME (#E8631A) minimalista:
"TME" gigante + "Iniciar reseteo" + LED redondo abajo (verde en linea / rojo no).
Sleep 60s, brillo 80% fijo. Tactil SOLO despierta (trapazos no disparan reseteos).

Estados (`tme_ui.cpp`):
- **V_IDLE** (naranja, LED): reposo. Push si online -> V_CONFIRM.
- **V_CONFIRM** (banda ambar): "Iniciar reseteo? push otra vez = SI". Timeout 10s. Push largo cancela.
- **V_RUN** (banda verde): barra de progreso que ESPEJA `barra_progreso.ps1` (check1 40%, check2 40-100% con ETA, total 110s) + 9 frases rotativas cada 6s para el operador. Barra "creep" (sube hasta 38%) mientras el agente confirma — para que el operador vea movimiento. Push largo = escape manual. Timeout 20s sin confirmacion del agente -> V_ERROR.
- **V_DONE** (verde): "Listo! Gracias, operador / Excelente viaje. Descansa, te esperan en casa." Push = volver.
- **V_ERROR** (rojo): "Algo fallo en el reseteo / Intenta de nuevo o reporta el error a administracion."

Modulos: `tme_net.cpp` (WiFi + polling 1.5s al agente + POST /reset + **redundancia anti-zombie: si 60s sin agente con WiFi "conectado", disconnect(true)+begin() forzados**), `tme_ui.cpp`.

## Lado servidor: agentes (NUNCA tocar el lado Telegram que ya existia)

### Mac Mini de Jose (servidor 24/7, su casa)
- IP LAN: **192.168.86.66**. Puerto **8765**.
- Genera `health.json` cada 60s (launchd) leyendo metricas macOS (uptime/load/procs/RAM/SSD/thermal/SMART/top CPU/RAM/speedtest cada 20min cacheado/`gamer_pc.online`).
- `gamer_pc.online` se calcula con **TCP connect al puerto 8767 de la PC gamer (timeout 1s)**, no con ping (mas robusto, sobrevive a firewall ICMP, verifica el agente que el ESP32 realmente usa).
- Tambien tiene `gaming-pc-control/` (config.json con la MAC + IP de la PC gamer) y `wol.py` — el bot de Telegram lo usa. NO TOCAR el Telegram.

### PC gamer (DESKTOP-729DKM8, Ryzen 7800X3D, RTX 4080)
- **IP Ethernet: 192.168.86.56**. (La .70 era WiFi viejo, ignorar; saga del 9-jun.)
- **MAC para WoL: c8:7f:54:67:6f:04** (NIC cableada; al encender responde por otra interfaz f0:c9:d1:bf:be:5d).
- Tiene `pc_agent.ps1` corriendo (ver `gamer-pc/`): puerto **8767**, token "stunthub". GET /status / POST /shutdown / POST /cancel. Tarea ONSTART como SYSTEM (sobrevive sin login).
- Apagado usa `shutdown /s /t 5` (gentil, sin /f).

### Maquina de reseteos (TME, planta industrial)
- Windows con auto-login admin sin password.
- Reseteos: tu `.bat` -> `PC26_V1.exe` (macro Jitbit, fuente Power_Spec_26.mcr; licencia c.egarza@gmail.com) que automatiza Cummins **PowerSpec v14.1.1 + INSITE 7.6.2.240**.
- Macro ya escribe senales a `%TEMP%\TME_BAR_SIGNAL.txt`: `check1`, `check2`, `error`, `done`. La UI que ven hoy es `barra_progreso.ps1` (que mantengo IGUAL — TMEhub la espeja).
- **MAQUINA TIENE DOS REDES** (saga 10-jun):
  - Ethernet -> modem Telmex -> **192.168.86.53** (red separada, inalcanzable desde el WiFi del Google).
  - WiFi -> "TME PATIO" (Google WiFi, mismo donde esta la perilla) -> **192.168.87.23**.
  - `TME_AGENT_HOST` en `firmware/TMEhub/secrets.h` apunta a **.87.23**.
- `tme_agent.ps1` (ver `company/`): puerto **8766**, GET /status espeja `barra_progreso.ps1` leyendo signal file + LastWriteTime, POST /reset borra signal viejo y lanza `PC26_V1.exe`. Tarea ONLOGON. **Watchdog instalado** (tarea c/5min revive si el agente murio).
- Red TME PATIO: SSID "TME PATIO", pass "tMe!730531" (en `firmware/TMEhub/secrets.h`, gitignored).

## Topologias de red (criticas)
**Casa de Jose (Google WiFi propio):**
- Red principal 192.168.86.x (donde vive Mac Mini .66 y PC gamer .56).
- WiFi invitados 192.168.87.x (AISLADO — si una Mac dev cae aqui, no ve nada).

**TME (oficina):**
- Modem Telmex (LAN cableada 192.168.86.x, ahi vive la maquina por cable, .53).
- Google WiFi "TME PATIO" colgado del modem (reparte 192.168.87.x).
- Las dos redes ESTAN SEPARADAS — el WiFi no enruta a la cableada del Telmex.
- Solucion adoptada: la maquina tambien se conecto al WiFi (.87.23). El agente escucha en TODAS las interfaces. La perilla habla a .87.23.

## Decisiones tomadas (no reabrir sin razon nueva)
- **NO al control de clima IR**: Jose ya tiene Lloyd's IR blasters controlados por voz con Alexa, no vale duplicarlo. Tuya/Smart Life seria el camino si algun dia (lo investigamos).
- **NO al M5Dial / Waveshare 1.8" por ahora**: para la perilla #2 personal de Jose, compra otra CrowPanel 1.46" igual (restauracion instantanea del binario, todo identico).
- **Wallpapers full screen via zoom 2x** (180px native + zoom = 360px). Decodificar 360 nativo seria 8x CPU y +1.5MB flash. No vale.
- **Mac Mini chequea gamer_pc con TCP-connect, no ping** — saga del firewall ICMP que nos perseguia.
- **Barra TMEhub espeja barra_progreso.ps1 sin tocarla** — fuente de verdad unica. Nunca alteramos el flujo del operador.

## Gotchas / fallos conocidos (no caer en estos)
1. **deserializeJson directo de https.getStream() falla silencioso.** Siempre `getString()` y deserializar.
2. **3+ handshakes TLS simultaneos al boot = task_wdt + boot loop.** Escalonar con `vTaskDelay` al inicio de cada task de red (clima 0s, X 3s, servidor 7s, mercados 14s).
3. **CoinGecko tarda primer handshake** — `setHandshakeTimeout(15)` + retry 30s tras fallo.
4. **Fuente Montserrat de LVGL NO tiene acentos / "..." / "—"** — todo ASCII.
5. **HTTP.sys de Windows rechaza POST sin cuerpo (411 Length Required)** — siempre `sendRequest("POST", "{}")` con header.
6. **Leer serial del ESP32-S3 con `RTS+DTR` mal seteado lo deja en modo DOWNLOAD** (boot:0x30, pantalla negra). Para escuchar sin tocar reset: abrir con `dtr=False, rts=False` ANTES de `open()`. Para revivir tras quedar atorado: power-cycle fisico.
7. **WiFi Mexico vs servicios cloud**: el primer fetch tras un boot puede tardar 8-12s.
8. **TmeAgent en .87.23 vs .86.53**: la perilla debe apuntar a la IP WiFi (.87.23), no a la Ethernet. Reservar DHCP en Google Home para que no cambie.
9. **`tme_agent.ps1` deja signal viejo del reseteo anterior** — version actual ya borra `TME_BAR_SIGNAL.txt` al iniciar (sino la perilla salta a "Listo" al instante leyendo el `done` viejo).
10. **PC tiene 2 NICs (Ethernet + WiFi)**: la MAC del WoL es de la cableada (c8:7f...); luego de encender responde por la otra (f0:c9...). La logica del Mac Mini ya lo maneja.

## Estructura del repo
```
crowpanel-esp32s3/
  firmware/
    StuntHub/        # perilla personal (7 apps)
    TMEhub/          # perilla industrial (reseteos)
    StuntHub/secrets.h          # gitignored
    StuntHub/secrets.h.example  # plantilla
    TMEhub/secrets.h            # gitignored
    TMEhub/secrets.h.example    # plantilla
  company/
    tme_agent.ps1      # agente Windows (TME, puerto 8766)
    INSTALL.md         # instalacion (netsh, firewall, schtasks)
  gamer-pc/
    pc_agent.ps1       # agente Windows (PC gamer, puerto 8767)
    INSTALL.md
  backup/
    factory_full_16MB.bin             # respaldo de fabrica (NUNCA subir a git, es 16MB)
    stunthub_v1_bin/                  # binario v1 (sin PC Gamer)
    stunthub_v2_bin/                  # binario v2 (CON PC Gamer) <- el bueno
  PLAN_dashboard_clima.md   # plan inicial (historico)
  PLAN_apps_nuevas.md       # plan de apps nuevas (historico)
  read_serial.py            # util para diagnosticar boot serial
  tests/                    # sketches diagnosticos (IR blink/solid, no usados)
  .gitignore
```

## Comandos canonicos

### Compilar
```bash
cd ~/Desktop/crowpanel-esp32s3/firmware/StuntHub   # o TMEhub
arduino-cli compile --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc" .
```

### Flashear (con placa conectada)
```bash
PORT=$(ls /dev/cu.usbmodem* | head -1)
cd ~/Desktop/crowpanel-esp32s3/firmware/StuntHub   # o TMEhub
arduino-cli compile --upload -p "$PORT" --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc" .
```

### Restaurar binario v2 sin recompilar
```bash
PORT=$(ls /dev/cu.usbmodem* | head -1)
arduino-cli upload -p "$PORT" --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc" --input-dir ~/Desktop/crowpanel-esp32s3/backup/stunthub_v2_bin
```

### Probar agentes desde la red
```bash
curl http://192.168.86.66:8765/health.json                       # Mac Mini health
curl http://192.168.86.56:8767/status?t=stunthub                  # PC gamer
curl http://192.168.87.23:8766/status                             # TME maquina
```

## Estilo de respuesta esperado (peticion explicita de Jose)
- Espanol, directo, sin verborrea.
- Frases tipo "Listo" no son resumenes — son cierres de accion.
- Cuando hago cambios de codigo, decir QUE cambie en 1-3 lineas y proximo paso. No describir todo el diff.
- Usar emojis con moderacion para estructurar (no decorar).
- Si veo un riesgo o decision relevante, decirla — Jose decide.
- Confianza con humor cuando aplica. El proyecto es de hobby + trabajo, no quirurgico.

## Pendientes vivos
- Reserva DHCP en Google Home: `.87.23` (maquina TME WiFi), `.56` (PC gamer), `.66` (Mac Mini).
- Reflashear `tme_agent.ps1` actualizado en la maquina TME (borra signal viejo) + reiniciar tarea.
- Beacons `IF IMAGE` del macro Jitbit: el detector de error sin camion no dispara (falso "completo"). Sugerencia: invertir logica a detectar EXITO con IF IMAGE y todo lo demas a error.
- Cuando llegue CrowPanel #2: queda en TME como TMEhub permanente; restauramos StuntHub en la de Jose desde `backup/stunthub_v2_bin/`.

## Mi historia (Claude) con este proyecto
- 9-jun: Identificacion de hardware, respaldo de fabrica, primera UI, integraciones de Hue/clima/X, exploracion IR fallida (camara iPhone filtra IR, soldadura sin solder wick), pivote a Tuya/Lloyd's, descarte de IR, apps Mercados + Servidor + Wallpapers, descubrimiento que Lloyd's es Tuya.
- 10-jun: PC Gamer (prender + apagar bidireccional), saga IP equivocada (.70 vs .56), saga POST 411, TMEhub deployment, **primer reseteo real exitoso en TME**, saga de red WiFi/Ethernet de la maquina TME (subredes separadas), UI v3 naranja TME por feedback de campo, redundancia anti-zombie WiFi.

Si llegaste leyendo esto: bienvenido al proyecto. Lee primero, lee el codigo, NUNCA flashees sin pedir confirmacion explicita de Jose, y respeta los gotchas.
