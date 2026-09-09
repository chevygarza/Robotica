// TAC-1 — La interfaz. Dos pantallas en la etapa 1: Inicio y Red.
#pragma once
#include <Arduino.h>

void    ui_build();
void    ui_tick();
uint8_t ui_brillo();     // % de pantalla que toca ahora: 70 de dia, 25 de noche
