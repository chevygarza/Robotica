#pragma once
#include <Arduino.h>

// Tactil CST816 (I2C 0x15). Gestos derivados por software a partir de las coordenadas.
enum class TouchEvent : uint8_t { None = 0, Tap, DoubleTap, Swipe, Hold, HoldEnd };

struct TouchInfo {
  TouchEvent ev = TouchEvent::None;
  int16_t x = 0, y = 0;     // punto del evento
  int16_t dx = 0, dy = 0;   // desplazamiento (Swipe)
  bool down = false;        // dedo puesto ahora mismo
};

bool touchBegin();
TouchInfo touchPoll(uint32_t nowMs);
