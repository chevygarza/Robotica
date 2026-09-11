#pragma once
#include <Arduino.h>

// PCF85063 (I2C 0x51). Hora real con pila: sirve para saber cuanto tiempo paso apagado.
bool rtcBegin();               // si el reloj no esta puesto, lo pone con la hora de compilacion
uint32_t rtcNow();             // unix time (0 si falla)
bool rtcSet(uint32_t unix);
