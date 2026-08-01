// VNL-1 — Manifiesto de albumes.
// El DFPlayer no reporta duracion ni metadata: todo sale de aqui. El color es
// el dominante de la caratula, precalculado a mano (el ESP32 no lo saca en
// tiempo real) y se usa para la etiqueta del vinilo y para el anillo de LEDs.
#pragma once
#include <Arduino.h>

struct Album {
  uint8_t     folder;    // carpeta en la microSD: 1 -> /01, 2 -> /02, ...
  const char* name;      // ASCII: la fuente Montserrat de LVGL no trae acentos
  const char* subtitle;
  uint8_t     tracks;
  uint16_t    seconds;   // duracion total
  uint32_t    color;
};

static const Album ALBUMS[] = {
  { 1, "FIFA",    "Mejores canciones", 10, 2040, 0x1DB954 },
  { 2, "ZELDA",   "Mejores canciones", 10, 2160, 0x3A6FD8 },
  { 3, "NATURAL", "Sonidos naturales", 10, 2400, 0xC8791E },
};

static const uint8_t ALBUM_COUNT = sizeof(ALBUMS) / sizeof(ALBUMS[0]);
