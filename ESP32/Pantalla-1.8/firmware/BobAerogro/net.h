#pragma once
#include <Arduino.h>

// Wi-Fi en tarea propia (core 0): hora exacta por NTP al RTC y vigilancia de hitos
// del mundo Claude. Hoy: version publicada de Claude Code en npm.
struct NetNews { bool fresh = false; char text[64] = ""; bool celebrate = false; };

void netBegin();
NetNews netPoll();          // devuelve fresh=true una sola vez por novedad
bool netOnline();
