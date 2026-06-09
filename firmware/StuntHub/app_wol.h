#pragma once
// Wake-on-LAN: manda el magic packet a la PC gamer.
// Espejo de wol.py del Mac Mini: broadcast, puertos 9 y 7, 3 repeticiones.
void wol_send();

// Apagado via pc_agent.ps1 en la PC (POST /shutdown). Resultado async:
// 0 = sin operacion, 1 = en curso, 2 = aceptado, -1 = fallo (agente?).
extern volatile int g_pcShutdownResult;
void pc_shutdown_async();
