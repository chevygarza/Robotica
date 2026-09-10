#pragma once
#include <Arduino.h>

// Energia de microfono (RMS) via ES7210 + I2S. Best-effort; si falla, energy=0.
bool micBegin();
float micEnergy();   // 0..1 aproximado
bool micOk();
