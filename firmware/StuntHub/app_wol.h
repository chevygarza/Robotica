#pragma once
// Wake-on-LAN: manda el magic packet a la PC gamer.
// Espejo de wol.py del Mac Mini: broadcast, puertos 9 y 7, 3 repeticiones.
void wol_send();
