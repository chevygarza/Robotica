#pragma once
#include <Arduino.h>

// Nivel de microfono (RMS suavizado, 0..1) via ES8311 + I2S en una tarea propia.
// Si el codec no responde, micOk()=false y micEnergy() devuelve 0.
bool micBegin();
float micEnergy();
bool micOk();
