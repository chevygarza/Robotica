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
Sleep 30s. Brillo: 70% dia / 25% de 10pm a 7am (NTP).

## Las apps (app_ui.cpp)
El orden del menu son los `#define APP_*` al inicio de app_ui.cpp: mover o
insertar una app = cambiar esa lista, NO cazar numeros por el archivo.

| # | App | Fuente de datos | Push |
|---|-----|------|------|
| 0 | Luces Hue | Bridge local 192.168.86.49 (44 focos/17 cuartos) | 5 vistas: cover->favoritos BUNKER (Relax/Blanco/Fiesta tactiles; Fiesta=2rojos+2azules)->cuartos->focos->control |
| 1 | Mercados | CoinGecko BTC/ETH/SOL c/5min (retry 30s) | refresh |
| 2 | PC Gamer | poll DIRECTO al agente :8767/status c/3s | push: prender(WoL)/apagar; menu navegable + modos Normal/Sim/TV (solo con PC lista) |
| 3 | Fotos | GIFs embebidos DBZ/Pokemon/Zelda (180px + zoom 2x = full 360) | siguiente GIF |

**VinilOS (musica) entra como app 2** en la etapa 4, empujando PC a 3 y Fotos a 4.

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
- `wallpapers_data.c` — GIFs en C (xxd). Regenerar: gifsicle crop cuadrado
  180px + xxd -i (workflow: Jose pasa URL de Giphy).
- GIF: creacion perezosa (wallShow) — solo existe en app 5; se destruye al
  salir/dormir (ui_screen_off/on desde el .ino). LV_USE_GIF=1 en lv_conf.

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
  Los modos solo se muestran/funcionan con la PC online (g_srv.pc_online).

## Pendientes
- Comprar CrowPanel #2 -> restaurar StuntHub en la placa original.
- Reserva DHCP de la PC gamer (.56) en el Google Home de la casa.
- Ideas en cola: app "Inbox del bunker" (notificaciones push del Mac Mini),
  mini-perilla M5Dial+IR Unit para el clima (la revancha del IR, opcional).
