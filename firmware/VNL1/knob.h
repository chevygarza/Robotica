// VNL-1 — Driver del encoder: giro por interrupcion, push por sondeo.
// Emite eventos discretos a una cola para que la maquina de estados no tenga
// que saber nada de rebotes ni de cuadratura.
#pragma once
#include <Arduino.h>

enum KnobEvent : uint8_t {
  KNOB_NONE = 0,
  KNOB_CW,           // giro horario
  KNOB_CCW,          // giro antihorario
  KNOB_PRESS,        // push corto (ver KNOB_DOUBLE_MS: llega con retardo)
  KNOB_DOUBLE_PRESS, // dos push seguidos
  KNOB_LONG_PRESS,   // push sostenido, se emite al cumplir el tiempo
  KNOB_DOWN          // el boton bajo. Para feedback visual inmediato: se emite
                     // sin esperar a saber si fue push corto, doble o largo.
};

// Sub-pasos de cuadratura por muesca fisica. Verificado en la placa: 4.
#define KNOB_COUNTS_PER_DETENT  4

// Verificado en la placa: horario da CW. Si algun dia se cambia el encoder y
// sale invertido, pon esto en 1.
#define KNOB_INVERT             0

// 3 segundos se siente roto: la gente sostiene ~1s, no ve nada, suelta, y el
// gesto se lee como push corto. 900ms es el rango donde un mantener se percibe
// deliberado sin volverse una espera.
#define KNOB_LONG_PRESS_MS    900

// Imprime por serial cuanto duro cada pulsacion. Solo para depurar.
#define KNOB_DEBUG_HOLD          1
#define KNOB_DEBOUNCE_MS       25

// Ventana para distinguir push corto de doble. El precio de tener doble push es
// que el push corto llega con este retardo: no hay manera de saber que fue
// sencillo hasta que la ventana cierra. 260ms esta al filo de lo perceptible;
// KNOB_DOWN existe para poder reaccionar en pantalla mientras tanto.
#define KNOB_DOUBLE_MS        260

namespace knob {

void      begin();
void      update();      // sondea el push; llamar en cada pasada del loop
KnobEvent read();        // siguiente evento, o KNOB_NONE
uint32_t  holdMs();      // ms que lleva sostenido el push (0 si no aplica)
uint32_t  idleMs();      // ms desde el ultimo evento (para el auto-apagado)
void      touch();       // registra actividad externa, ej. el tactil
int32_t   diagCount();   // sub-pasos acumulados, solo diagnostico

} // namespace knob
