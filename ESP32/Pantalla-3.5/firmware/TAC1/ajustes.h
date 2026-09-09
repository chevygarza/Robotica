// TAC-1 — Ajustes del aparato, guardados en NVS.
// Una idea por campo. Lo que no tiene hardware todavia (volumen) no se guarda.
#pragma once
#include <Arduino.h>

#define BRILLO_AUTO 0          // 70 de dia, 25 de noche, por NTP
#define BRILLO_MIN 20

struct Ajustes {
  uint8_t brillo;              // BRILLO_AUTO o BRILLO_MIN..100
};

void     ajustes_begin();
void     ajustes_save();
Ajustes& ajustes();
