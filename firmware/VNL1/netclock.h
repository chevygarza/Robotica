// VNL-1 — Reloj de red. WiFi + NTP.
//
// El ESP32 no tiene reloj con pila: al desconectarlo pierde la hora. Sin una
// fuente externa, una alarma no sabe cuando sonar. Por eso hay WiFi aqui, y
// solo para esto — no se usa para nada mas.
//
// Nada de esto bloquea: la UI arranca y funciona completa aunque el WiFi tarde
// o nunca conecte. La alarma simplemente no dispara hasta que haya hora buena.
#pragma once
#include <Arduino.h>
#include <time.h>

void clock_begin();
void clock_tick();                  // llamar en cada pasada del loop

bool clock_wifi();                  // asociado a la red
bool clock_ready();                 // hay hora valida (ya sincronizo NTP)
bool clock_now(struct tm* out);

// "07:32", o "--:--" mientras no hay hora.
void clock_hhmm(char* out, size_t n);

// Texto corto de estado para la UI: "sin red", "buscando hora", "en hora".
const char* clock_status();
