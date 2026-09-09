// VNL-1 — Maquina de estados y overlays de la UI.
//
// SELECTOR  girar = disco   · push = reproducir ya (sin pasos intermedios)
// TOCANDO   girar = volumen · push = pausa · doble = siguiente
//           mantener = volver a la biblioteca, con aro de progreso
//
// Al volver a la biblioteca la musica NO se detiene: el anillo de LEDs sigue
// respirando con el color del album que suena, y un push sobre ese mismo disco
// regresa a la reproduccion sin reiniciarla.
#pragma once
#include <Arduino.h>
#include "knob.h"

enum AppState : uint8_t { ST_SELECTOR = 0, ST_PLAYING, ST_ALARMA, ST_AJUSTES, ST_RELOJ };

bool      app_begin();
void      app_event(KnobEvent e);
void      app_tick();
AppState  app_state();
uint8_t   app_bl_dbg();
bool      app_sonando_dbg();
