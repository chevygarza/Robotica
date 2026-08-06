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
  const uint32_t* anillo;         // 8 colores, uno por LED
};

static const uint16_t TRACKS_01[] = { 214, 204, 240, 177 };
static const uint16_t TRACKS_02[] = { 80, 175, 301, 80, 119, 205, 99, 223 };
static const uint16_t TRACKS_03[] = { 102, 115, 204, 254, 90 };
static const uint16_t TRACKS_04[] = { 198, 180, 177, 240, 240, 240, 240 };

static const uint32_t RING_01[8] = { 0x5179CC, 0x5195CC, 0x5198CC, 0x519ECC, 0x87CC51, 0x5ECC51, 0x5185CC, 0x518CCC };
static const uint32_t RING_02[8] = { 0x7DCC51, 0xCCB051, 0x51CCB4, 0x51CC7C, 0x51CC96, 0x51CCA1, 0xCCAE51, 0x6CCC51 };
static const uint32_t RING_03[8] = { 0xCC5159, 0xCC763F, 0xCC9540, 0xC6CC43, 0xCCB22D, 0xCCB845, 0xCC7448, 0xCC515A };
static const uint32_t RING_04[8] = { 0x7741CC, 0x9B51CC, 0x6441CC, 0x9151CC, 0x784FCC, 0x6340CC, 0x9A51CC, 0x8651CC };

static const Album ALBUMS[] = {
  { 1, "FIFA", "Mejores canciones", 4, 835, TRACKS_01, 0x102544, COVER_01, RING_01 },
  { 2, "ZELDA", "Mejores canciones", 8, 1282, TRACKS_02, 0xDCB35D, COVER_02, RING_02 },
  { 3, "MINECRAFT", "Bandas sonoras", 5, 765, TRACKS_03, 0xF3A342, COVER_03, RING_03 },
  { 4, "RELAX", "Para bajar el ritmo", 7, 1515, TRACKS_04, 0x36205F, COVER_04, RING_04 },
};

static const uint8_t ALBUM_COUNT = sizeof(ALBUMS) / sizeof(ALBUMS[0]);
