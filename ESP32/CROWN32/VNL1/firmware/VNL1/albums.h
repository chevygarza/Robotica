// VNL-1 — Manifiesto de albumes.
//
// GENERADO por sd/prepare_sd.py desde la carpeta _origen de la microSD.
// NO editar a mano: la proxima corrida lo sobrescribe.
//
// El DFPlayer no reporta duracion ni metadata: todo sale de aqui. Las
// duraciones y los nombres de pista son reales, sacados con ffprobe de
// los archivos originales. El color es el dominante
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
  const char* const* track_names; // titulo de cada pista
  const char* const* track_artists;
  uint32_t        color;
  const uint16_t* cover;          // nullptr = etiqueta dibujada
  const uint32_t* anillo;         // 8 colores, uno por LED
};

static const uint16_t TRACKS_01[] = { 214, 123, 188, 181, 240, 223, 177, 270, 223 };
static const uint16_t TRACKS_02[] = { 80, 175, 301, 80, 119, 205, 99, 223 };
static const uint16_t TRACKS_03[] = { 102, 115, 204, 254, 90 };
static const uint16_t TRACKS_05[] = { 332, 592, 323, 259, 511 };
static const uint16_t TRACKS_06[] = { 236, 208, 229, 229, 254 };
static const uint16_t TRACKS_99[] = { 240, 240, 240, 240 };

static const char* const NOMBRES_01[] = { "Alive", "Song 2", "Jerk It Out", "Ahora", "Nothing In My Way", "We Are One (Ole Ola)", "Que Calor", "La Copa de la Vida", "Dai Dai" };
static const char* const ARTISTAS_01[] = { "Empire of the Sun", "Blur", "Caesars", "Zion y Lennox", "Keane", "Pitbull ft. Jennifer Lopez", "Major Lazer ft. J Balvin", "Ricky Martin", "Shakira, Burna Boy" };
static const char* const NOMBRES_02[] = { "Title Theme", "Gerudo Valley", "Hyrule Field", "Mipha's Theme", "Vah Ruta Battle", "Breath of the Wild", "Song of Storms", "Zora's Domain" };
static const char* const ARTISTAS_02[] = { "Nintendo", "Nintendo", "Nintendo", "Nintendo", "Nintendo", "Nintendo", "Nintendo", "Nintendo" };
static const char* const NOMBRES_03[] = { "Beginning", "Equinoxe", "Haggstrom", "Minecraft", "Wet Hands" };
static const char* const ARTISTAS_03[] = { "C418", "C418", "C418", "C418", "C418" };
static const char* const NOMBRES_05[] = { "Enter Sandman", "Green Grass & High Tides", "Carry On Wayward Son", "Say It Ain't So", "Won't Get Fooled Again" };
static const char* const ARTISTAS_05[] = { "Metallica", "The Outlaws", "KANSAS", "weezer", "The Who" };
static const char* const NOMBRES_06[] = { "El Poder Nuestro Es", "Sal De Ahi Magnifico Poder", "Angeles Fuimos", "Romance Te Puedo Dar", "Mi Corazon Encantado" };
static const char* const ARTISTAS_06[] = { "Adrian Barba", "Paco Castellanos", "Adrian Barba", "Gabii Rodriguez", "Pablo Fiore" };
static const char* const NOMBRES_99[] = { "Dreaming Embers", "Focus", "Healing Vibes", "Slow Fire" };
static const char* const ARTISTAS_99[] = { "", "", "", "" };

static const uint32_t RING_01[8] = { 0x5179CC, 0x5195CC, 0x5198CC, 0x519ECC, 0x87CC51, 0x5ECC51, 0x5185CC, 0x518CCC };
static const uint32_t RING_02[8] = { 0x7DCC51, 0xCCB051, 0x51CCB4, 0x51CC7C, 0x51CC96, 0x51CCA1, 0xCCAE51, 0x6CCC51 };
static const uint32_t RING_03[8] = { 0xCC5159, 0xCC763F, 0xCC9540, 0xC6CC43, 0xCCB22D, 0xCCB845, 0xCC7448, 0xCC515A };
static const uint32_t RING_05[8] = { 0xCC8E13, 0xCC7A07, 0x51CC9B, 0x7B51CC, 0xCC2792, 0xCC1431, 0xCC3406, 0xCC6402 };
static const uint32_t RING_06[8] = { 0xCC8101, 0xBDCC51, 0x5177CC, 0x9C42CC, 0xCC2369, 0xCC2E21, 0xCC5303, 0xCC6601 };
static const uint32_t RING_99[8] = { 0x7741CC, 0x9B51CC, 0x6441CC, 0x9151CC, 0x784FCC, 0x6340CC, 0x9A51CC, 0x8651CC };

static const Album ALBUMS[] = {
  { 1, "FIFA", "Mejores canciones", 9, 1839, TRACKS_01, NOMBRES_01, ARTISTAS_01, 0x102544, COVER_01, RING_01 },
  { 2, "ZELDA", "Mejores canciones", 8, 1282, TRACKS_02, NOMBRES_02, ARTISTAS_02, 0xDCB35D, COVER_02, RING_02 },
  { 3, "MINECRAFT", "Bandas sonoras", 5, 765, TRACKS_03, NOMBRES_03, ARTISTAS_03, 0xF3A342, COVER_03, RING_03 },
  { 5, "ROCK BAND", "", 5, 2017, TRACKS_05, NOMBRES_05, ARTISTAS_05, 0xDB2107, COVER_05, RING_05 },
  { 6, "DRAGON BALL", "", 5, 1156, TRACKS_06, NOMBRES_06, ARTISTAS_06, 0xEA6801, COVER_06, RING_06 },
  { 99, "RELAX", "Para bajar el ritmo", 4, 960, TRACKS_99, NOMBRES_99, ARTISTAS_99, 0x36205F, COVER_99, RING_99 },
};

static const uint8_t ALBUM_COUNT = sizeof(ALBUMS) / sizeof(ALBUMS[0]);
