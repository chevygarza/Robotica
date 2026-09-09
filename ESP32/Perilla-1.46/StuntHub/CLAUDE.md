# StuntHub — Perilla personal de Jose (bunker)

Aparato centrado en MUSICA (VinilOS) con 4 apps de apoyo, en la CrowPanel
redonda. Antes de tocar nada: lee `../CLAUDE.md` (reglas duras + build +
CROWN Interface Guidelines).

## ⚠️ Particion de 8MB (NO uses el comando de build generico)
Desde la integracion de VinilOS el firmware usa `vnl1_8M` (huge_app de 3MB ya
no alcanza con las caratulas). SIEMPRE compilar/flashear asi:
```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc" \
  --build-property build.partitions=vnl1_8M \
  --build-property upload.maximum_size=8388608 .
```
StuntHub no guarda nada en NVS, asi que cambiar de particion no pierde datos.

## Navegacion
Girar perilla = cambiar app (animacion slide). Push corto = entrar/accion.
Push largo = atras. Tactil = botones en apps que los tienen + despertar.
**Hue, PC Gamer y VinilOS SECUESTRAN el giro** dentro de sus sub-vistas con un
`return` temprano en `ui_nav()`: ahi girar navega la lista (o sube el volumen),
no cambia de app. Es el patron a copiar, no inventes otro.
Sleep 30s. Brillo: 70% dia / 25% de 10pm a 7am (NTP).

## Las apps (app_ui.cpp)
El orden del menu son los `#define APP_*` al inicio de app_ui.cpp: mover o
insertar una app = cambiar esa lista, NO cazar numeros por el archivo.

| # | App | Fuente de datos | Push |
|---|-----|------|------|
| 0 | Luces Hue | Bridge local 192.168.86.49 (44 focos/17 cuartos) | 5 vistas: cover->favoritos BUNKER (Relax/Blanco/Fiesta tactiles; Fiesta=2rojos+2azules)->cuartos->focos->control |
| 1 | Mercados | CoinGecko BTC/ETH/SOL c/5min (retry 30s) | refresh |
| 2 | **VinilOS** (musica) | microSD del DFPlayer + manifiesto compilado (albums.h) | 3 vistas: portada->biblioteca (girar hojea discos)->reproduciendo (girar = VOLUMEN) |
| 3 | PC Gamer | poll DIRECTO al agente :8767/status c/3s | push: prender(WoL)/apagar; menu navegable + modos Normal/Sim/TV (solo con PC lista) |
| 4 | Fotos | GIFs embebidos DBZ/Pokemon/Zelda (180px + zoom 2x = full 360) | siguiente GIF |

### VinilOS (app 2) — integrado ago-2026, etapas 3 y 4
Portado de `ESP32/Perilla-1.46/VinilOS/firmware/VNL1/` SIN tocar: `player.*`, `vinyl.*`,
`covers.*`, `albums.h`. Lo unico que se cambio: `player.cpp` incluye `board.h`
en vez de `pins.h` (aqui los pines viven en board.h). **Si re-sincronizas desde
VNL1, ese include se vuelve a romper.**

| Vista | Girar | Push | Mantener |
|---|---|---|---|
| Portada | cambia de app | entra a la biblioteca | — |
| Biblioteca | hojea discos (circular) | reproduce | vuelve a la portada |
| Reproduciendo | **VOLUMEN** | pausa / reanuda | vuelve a la biblioteca |

- **Fondo NEGRO dentro de esta app: excepcion documentada a la regla 8** (fondos
  brillantes). La caratula llena el disco, la mura casi no se ve y el negro es lo
  que hace que el vinilo se lea como objeto. Decision de Jose, ago-2026.
- Los nombres de pista se indexan por **ARCHIVO** (`player_track_file()`), NUNCA
  por `player_track_index()`: con el album barajado esos dos numeros no coinciden.
- Salir a la biblioteca o dormir la pantalla **NO detiene la musica**.
- Falta (etapa 5): reposo/reloj y los Ajustes de VinilOS. Hoy al acabarse un
  album simplemente **repite el mismo** (`Al terminar` vive en NVS, sin portar).

### Apps eliminadas (ago-2026, pivote a aparato musical)
Clima, X @stuntech y Servidor (Mac Mini). Recuperables del historial de git.
- El **reloj+clima** de la app Clima vuelve como **REPOSO** del aparato (etapa 5):
  inactivo sin musica -> reloj con fecha y clima; con musica -> disco girando.
  `app_net` SIGUE trayendo NTP + Open-Meteo aunque hoy nadie lo pinte.
- **X se fue completo** (fetchX, XProfile, s_reqX): un TLS menos en el core 0.
  Las llaves X_BEARER_TOKEN/X_USERNAME quedan en secrets.h sin uso.
- **Servidor** (`app_server.*`) borrado. PC Gamer ya NO depende del health.json:
  su bloque en ui_tick estaba ANIDADO dentro de `if (server_lock(10))` y se
  desanido. OJO si restauras algo de esa app: ese anidamiento era el acople.

## Modulos
- `app_net.cpp` — NTP + clima Open-Meteo (TLS con certs.h ISRG Root X1). Task core 0.
- `app_hue.cpp` — API v1 del bridge, queue de comandos, ArduinoJson filter.
- `app_markets.cpp` — CoinGecko. setHandshakeTimeout(15) obligatorio.
- `app_wol.cpp` — magic packet DIRECTO (UDP broadcast 192.168.86.255:9,7 x3,
  MAC en secrets) + apagado via pc_agent (agents/pc_agent.ps1 corre en la PC
  gamer, puerto 8767, token; POST /shutdown con cuerpo "{}" — 411 si no).
- `player.cpp` — DFPlayer por UART1 (pines 43/44 en board.h). Cola con throttle
  de 60ms y volumen colapsado. **`isACK=false` NO ES NEGOCIABLE** con el clon
  MH2024K: con true, `sendStack()` gira para siempre esperando un ACK que no llega.
- `vinyl.cpp` — el disco: surcos estaticos + etiqueta que rota a 3 RPM, sin
  suavizado (interpolar cada pixel hunde los fps). Ver el gotcha de RAM abajo.
- `covers.cpp` / `albums.h` — GENERADOS por `sd/prepare_sd.py` de VNL1. NO editar
  a mano. Agregar un disco = correr ese script alla y volver a copiar los dos.
- `wallpapers_data.c` — GIFs en C (xxd). Regenerar: gifsicle crop cuadrado
  180px + xxd -i (workflow: Jose pasa URL de Giphy).
- GIF: creacion perezosa (wallShow) — solo existe dentro de Fotos (`APP_FOTOS`);
  se destruye al salir/dormir (ui_screen_off/on desde el .ino). LV_USE_GIF=1 en
  lv_conf. El vinilo usa este MISMO patron (musVinylOn/Off).

## Infraestructura casera (red 192.168.86.x principal / 87.x invitados-AISLADA)
- Mac Mini servidor 24/7: .66:8765 health.json (cron/launchd; tambien corre
  el bot Telegram con wol.py — NO TOCAR ese lado).
- PC gamer DESKTOP-729DKM8: Ethernet **192.168.86.58** (era .56; DHCP la movio;
  VERIFICAR en vivo siempre - regla #5; reservar DHCP pendiente). WoL MAC
  c8:7f:54:67:6f:04 (la cableada; al encender responde otra interfaz).
  pc_agent.ps1 como tarea ONSTART/SYSTEM, puerto 8767, token "stunthub".
  Endpoints: /status /shutdown /cancel /normal /sim /tv. Usuario Windows: Chevy.
- Hue bridge: .49.

## Gotchas propios
- **`powerUpScreen()` ESTA MAL Y HAY QUE ARREGLARLO** (pendiente al 2026-08-10).
  Sintoma: al reiniciar o reconectar, la pantalla sale con una banda de rayas
  horizontales al centro, y solo se quita **desconectando y volviendo a conectar
  el USB**. Intermitente, porque es una carrera de tiempos.
  El serial NO lo delata: reporta fps normales. El bus del panel es de escritura,
  el ESP32 nunca puede preguntarle al controlador como esta.
  Tres defectos en `StuntHub.ino`, y el primero es el peor:
  1. **El pulso de reset ocurre ANTES de darle corriente al panel.** Un reset sin
     VDD no resetea: el controlador arranca en el estado en que quedo.
  2. `PIN_SCR_PWR_A/B` solo suben, nunca bajan. En un reinicio por software
     —watchdog, brownout, subir firmware— el panel no pierde corriente y conserva
     el estado corrupto. Desconectar el USB era lo unico que lo revivia.
  3. No hay espera tras soltar `RST`. Los ST77xx piden ~120ms para terminar su
     encendido interno antes de aceptar comandos; `gfx.init()` llega en
     microsegundos y la configuracion se aplica a medias. Una ventana de
     direcciones mal escrita **es** esa banda de rayas.
  Arreglo verificado en VinilOS (`ESP32/Perilla-1.46/VinilOS/firmware/VNL1/display.cpp`),
  aqui ademas hay que **corregir el orden**:
  ```c
  static void powerUpScreen() {
    pinMode(PIN_LCD_RST, OUTPUT);
    digitalWrite(PIN_LCD_RST, LOW);              // en reset mientras no hay VDD
    pinMode(PIN_SCR_PWR_A, OUTPUT); digitalWrite(PIN_SCR_PWR_A, LOW);
    pinMode(PIN_SCR_PWR_B, OUTPUT); digitalWrite(PIN_SCR_PWR_B, LOW);
    delay(80);                                   // descarga de sus caps
    digitalWrite(PIN_SCR_PWR_A, HIGH);           // AHORA la corriente
    digitalWrite(PIN_SCR_PWR_B, HIGH);
    pinMode(PIN_PWR_LED, OUTPUT); digitalWrite(PIN_PWR_LED, LOW);
    pinMode(PIN_RGB_PWR, OUTPUT); digitalWrite(PIN_RGB_PWR, HIGH);
    delay(20);                                   // VDD estable
    digitalWrite(PIN_LCD_RST, LOW);              // reset CON corriente
    delay(20);
    digitalWrite(PIN_LCD_RST, HIGH);
    delay(150);                                  // el ST77961 pide ~120ms
  }
  ```
  Consecuencia buena: con esto **cualquier reinicio del ESP32 recupera la
  pantalla** y ya no hace falta desconectar el cable.
- Boot: los task de red ESCALONADOS — 3 TLS simultaneos en core 0 = task_wdt +
  boot loop. Al quitar X y Servidor quedan pocos (clima, mercados, Hue y el
  poll de la PC), pero la regla sigue: si agregas app de red nueva, dale su slot.
- deserializeJson SIEMPRE desde getString(), nunca getStream() (falla mudo).
- Wallpapers a 360 nativo = NO (8x CPU, +1.5MB); el zoom 2x es la decision.
- PERFILES PC (Normal/Sim/TV): pc_agent corre como SYSTEM (sesion 0). Los .ps1
  de perfil cambian DISPLAY/AUDIO y abren Steam = REQUIEREN sesion interactiva;
  desde SYSTEM fallan en silencio. SOLUCION: los endpoints /normal|/sim|/tv NO
  corren el .ps1; disparan tareas programadas StuntHub-Normal/Sim/TV creadas con
  /it /ru Chevy (corren en la sesion logueada). Firmware: pc_profile_async() en
  app_wol = espejo de pc_shutdown_async, POST /<path>?t=token (path=normal|sim|tv).
  Los modos solo se muestran/funcionan con la PC lista: `pcReady()` = estado
  directo encendida + uptime>=2min (lockout de arranque). OJO: la doc vieja decia
  `g_srv.pc_online`, pero **`g_srv` ya no existe** (se fue con app_server).

- **La MUSICA se atiende fuera del bloque de su app.** El avance de pista y el
  repetir-album viven en `ui_tick` PERO sin la guarda `curApp == APP_MUSICA`:
  metidos dentro, un album que se acaba mientras andas en Hue o PC se quedaba
  callado, y `player_album_fin()` es consume-once, asi que al volver a la app
  disparaba tarde y arrancaba el disco de golpe. Solo lo VISUAL (vinyl_tick, el
  overlay de volumen) depende de estar viendo el disco.
- **`ui_tick()` tiene un throttle de 500ms**: todo lo que va DESPUES corre a 2
  fps. La animacion del vinilo (`vinyl_tick()`) va ANTES del throttle a
  proposito — puesta despues, el disco gira a saltos visibles. Cualquier cosa
  que anime va arriba; lo que solo refresca datos, abajo.
- VINILO Y RAM INTERNA: los canvas de vinyl.cpp piden ~120KB de RAM **interna**
  (labelBuf, 200x200 TRUE_COLOR_ALPHA; el disco de 336px si va a PSRAM). Es la
  MISMA RAM que necesita el TLS de clima/mercados. Con el vinilo residente el
  heap caia de 148KB a **19KB** durante un handshake — funcionaba, pero sin
  colchon. SOLUCION: reserva perezosa (musVinylOn/Off en app_ui) — se crea al
  entrar a la app y se suelta al salir o al dormir. Medido: 139KB con el arreglo.
  `vinyl_destroy()` mata las animaciones ANTES de borrar los objetos (si no, el
  callback escribe sobre memoria liberada) y toda la API queda blindada con
  `if (!disc) return;`. Al recrear hay que restaurar zoom/cover_mode/spinning y
  SUBIR las capas de texto (lv_obj_move_foreground): los canvas nuevos quedan
  encima de las etiquetas creadas antes.
- player.h PROMETE que player_begin() devuelve false sin modulo y todo se vuelve
  no-op. NO ES CIERTO: siempre retorna true (con isACK=false no hay forma de
  preguntarle nada al DFPlayer). La UI no puede saber si hay audio.

## Pendientes
### Etapa 5 — cerrar la integracion de VinilOS
- **Reposo/reloj**: inactivo sin musica -> reloj (hora + fecha + clima, fuente
  `vnl_reloj_78.c` de VNL1, 78pt/76KB); con musica -> disco girando atenuado.
  `app_net` ya trae NTP + Open-Meteo aunque hoy nadie lo pinta.
- **Ajustes de VinilOS** (NVS): sobre todo `Al terminar` (detener/repetir/
  infinito). Hoy esta cableado a "repetir el mismo album".
- **`PLAYER_VOL_MAX` para la Max**: hoy 30, que es el tope de la Base con
  amplificador interno. Con PAM8403 por salida de linea satura ANTES: los
  ultimos pasos de la perilla no subirian volumen, distorsionarian. Hay que
  encontrarlo de oido con las bocinas montadas (ver `docs/producto.md` de VNL1).
- **`powerUpScreen()`**: el arreglo de las rayas horizontales sigue sin aplicar
  (ver gotchas). Es codigo listo para pegar.

### Casa / infra
- ~~Reserva DHCP de la PC gamer~~ **HECHA** (ago-2026): .58 fijada en Google Home.

### Ideas en cola
- App "Inbox del bunker" (notificaciones push del Mac Mini).
- Mini-perilla M5Dial+IR Unit para el clima (la revancha del IR, opcional).
