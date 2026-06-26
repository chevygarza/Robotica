# StuntHub — Perilla personal de Jose (bunker)

Dashboard de 7 apps en la CrowPanel redonda. Antes de tocar nada: lee
`../CLAUDE.md` (reglas duras + build). HOY este firmware NO esta flasheado en
ninguna placa (la unica placa fisica corre TMEhub); se restaura cuando llegue
la CrowPanel #2 — binario listo en `../backup/stunthub_v2_bin/`.

## Navegacion
Girar perilla = cambiar app (animacion slide). Push corto = entrar/accion.
Push largo = atras. Tactil = botones en apps que los tienen + despertar.
Sleep 30s. Brillo: 70% dia / 25% de 10pm a 7am (NTP).

## Las 7 apps (app_ui.cpp, NUM_APPS=7)
| # | App | Fuente de datos | Push |
|---|-----|------|------|
| 0 | Clima MTY + reloj 12h | Open-Meteo (lat/lon en secrets) | detalle: lluvia prox 5h |
| 1 | X @stuntech | api.x.com/2 + bearer (secrets); refresh 7am + on-demand | detalle: bio |
| 2 | Luces Hue | Bridge local 192.168.86.49 (44 focos/17 cuartos) | 5 vistas: cover->favoritos BUNKER (Relax/Blanco/Fiesta tactiles; Fiesta=2rojos+2azules)->cuartos->focos->control |
| 3 | Mercados | CoinGecko BTC/ETH/SOL c/5min (retry 30s) | refresh |
| 4 | Servidor | health.json del Mac Mini (192.168.86.66:8765, regenera c/60s) | refresh |
| 5 | Wallpapers | GIFs embebidos DBZ/Pokemon/Zelda (180px + zoom 2x = full 360) | siguiente GIF |
| 6 | PC Gamer | estado: gamer_pc.online del health.json | push: prender(WoL)/apagar; + botones tactiles Normal/Sim/TV (perfiles monitor+audio, solo online) |

## Modulos
- `app_net.cpp` — clima + X (TLS con certs.h ISRG Root X1). Task core 0.
- `app_hue.cpp` — API v1 del bridge, queue de comandos, ArduinoJson filter.
- `app_markets.cpp` — CoinGecko. setHandshakeTimeout(15) obligatorio.
- `app_server.cpp` — health del Mac Mini (HTTP plano LAN).
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
- Boot: los task de red ESCALONADOS (clima 0s, X ~3s, servidor 7s, mercados
  14s) — 3 TLS simultaneos en core 0 = task_wdt + boot loop. Si agregas app
  de red nueva, dale su slot.
- deserializeJson SIEMPRE desde getString(), nunca getStream() (falla mudo).
- health.json chequea la PC gamer con TCP-connect a :8767, no ping.
- gamer_pc "done"/estado viejo: la UI de PC Gamer verifica transiciones, no
  estados absolutos.
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
