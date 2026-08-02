// VNL-1 — Manifiesto de albumes.
//
// GENERADO por sd/prepare_sd.py desde la carpeta _origen de la microSD.
// NO editar a mano: la proxima corrida lo sobrescribe.
//
// El DFPlayer no reporta duracion ni metadata: todo sale de aqui. Las
// duraciones son reales, medidas con ffprobe. El color es el dominante
// de la caratula, y se usa en la etiqueta y en el anillo de LEDs.
#pragma once
#include <Arduino.h>
#include "covers.h"

struct Album {
  uint8_t         folder;         // carpeta en la microSD: 1 -> /01
  const char*     name;           // ASCII: Montserrat no trae acentos
  const char*     subtitle;
  uint8_t         tracks;
  uint16_t        seconds;        // total del album
  const uint16_t* track_seconds;  // duracion de cada pista
  uint32_t        color;
  const uint16_t* cover;          // nullptr = etiqueta dibujada
};

static const uint16_t TRACKS_01[] = { 240 };
static const uint16_t TRACKS_02[] = { 80, 175, 301, 80, 119, 205, 99, 223 };
static const uint16_t TRACKS_03[] = { 204, 90 };
static const uint16_t TRACKS_04[] = { 198, 180, 177 };

static const Album ALBUMS[] = {
  { 1, "FIFA", "Mejores canciones", 1, 240, TRACKS_01, 0x1DB954, nullptr },
  { 2, "ZELDA", "Mejores canciones", 8, 1282, TRACKS_02, 0x295931, COVER_02 },
  { 3, "MINECRAFT", "Bandas sonoras", 2, 294, TRACKS_03, 0xC8791E, nullptr },
  { 4, "RELAX", "Para bajar el ritmo", 3, 555, TRACKS_04, 0xB5476B, nullptr },
};

static const uint8_t ALBUM_COUNT = sizeof(ALBUMS) / sizeof(ALBUMS[0]);
