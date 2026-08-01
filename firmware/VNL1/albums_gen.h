// VNL-1 — GENERADO por sd/prepare_sd.py. No editar a mano.
// Duraciones reales de cada archivo, sacadas con ffprobe: es lo que
// permite una barra de progreso honesta (el DFPlayer no da metadata).
#pragma once
#include <Arduino.h>

struct AlbumGen {
  uint8_t         folder;
  const char*     name;
  const char*     subtitle;
  uint8_t         tracks;
  uint16_t        seconds;        // total del album
  const uint16_t* track_seconds;  // duracion de cada pista
  uint32_t        color;
};

static const uint16_t TRACKS_01[] = { 198, 180, 177 };
static const uint16_t TRACKS_02[] = { 0 };
static const uint16_t TRACKS_03[] = { 0 };

static const AlbumGen ALBUMS_GEN[] = {
  { 1, "FIFA", "Mejores canciones", 3, 555, TRACKS_01, 0x1DB954 },
  { 2, "ZELDA", "Mejores canciones", 0, 0, TRACKS_02, 0x3A6FD8 },
  { 3, "NATURAL", "Sonidos naturales", 0, 0, TRACKS_03, 0xC8791E },
};

static const uint8_t ALBUM_GEN_COUNT = sizeof(ALBUMS_GEN) / sizeof(ALBUMS_GEN[0]);
