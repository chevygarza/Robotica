// VNL-1 — Maquina de estados y overlays de la UI.
//
// SELECTOR  girar = disco     · push = entrar (o volver a lo que suena)
// DETALLE   push  = reproducir · push 3s = atras
// TOCANDO   girar = volumen   · push = pausa · doble = siguiente · 3s = atras
//
// Al volver a la biblioteca la musica NO se detiene: el anillo de LEDs sigue
// respirando con el color del album que suena, y un push sobre ese mismo disco
// regresa a la reproduccion sin reiniciarla.
#pragma once
#include <Arduino.h>
#include "knob.h"

enum AppState : uint8_t { ST_SELECTOR = 0, ST_DETAIL, ST_PLAYING };

bool      app_begin();
void      app_event(KnobEvent e);
void      app_tick();
AppState  app_state();
