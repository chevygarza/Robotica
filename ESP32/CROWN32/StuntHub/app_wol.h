#pragma once
#include <Arduino.h>
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
extern volatile int g_pcProfileResult;   // 0=idle, 1=en curso, 2=ok, -1=fallo

// Estado DIRECTO de la PC (un task persistente consulta /status). Mas fresco
// que el health.json del Mac Mini (hasta 60s). pc_status_begin() en setup;
// pc_status_active(true) mientras se ve la app PC Gamer; pc_direct_state()
// devuelve 1=encendida / 0=apagada / -1=desconocido.
extern volatile int g_pcUptimeMin;    // uptime de la PC en minutos (-1=desconocido)
extern volatile uint32_t g_pcLastOk;  // millis del ultimo GET 200
void pc_status_begin();
void pc_status_active(bool on);
int  pc_direct_state();
