#pragma once
// Wake-on-LAN: manda el magic packet a la PC gamer.
// Espejo de wol.py del Mac Mini: broadcast, puertos 9 y 7, 3 repeticiones.
void wol_send();

// Apagado via pc_agent.ps1 en la PC (POST /shutdown). Resultado async:
// 0 = sin operacion, 1 = en curso, 2 = aceptado, -1 = fallo (agente?).
extern volatile int g_pcShutdownResult;
void pc_shutdown_async();

// Perfil de monitores/audio: POST /<path> al pc_agent (path = "normal"|"sim"|
// "tv"). El agente (SYSTEM) dispara una tarea programada que corre en la sesion
// del usuario. path debe ser un literal estatico (lo usa un task aparte).
void pc_profile_async(const char *path);

// Estado DIRECTO de la PC (GET /status al pc_agent): mas fresco que el
// health.json del Mac Mini (que tarda hasta 60s). -1 = desconocido,
// 0 = apagada (timeout), 1 = encendida (respondio). Llamar pc_status_poll()
// periodicamente mientras se ve la app PC Gamer.
extern volatile int g_pcDirectState;
void pc_status_poll();
