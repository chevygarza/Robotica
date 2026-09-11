#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Tamagotchi: cuatro necesidades 0..100 que bajan con el tiempo REAL (RTC),
// memoria en NVS (sobrevive apagados y reflasheos del app).
enum class PetAction : uint8_t { Feed, Caress, Tickle, Play };
enum class Need : uint8_t { Hunger = 0, Sleep, Fun, Love, COUNT };

struct PetState {
  float need[(int)Need::COUNT] = {80, 80, 80, 80};   // 100 = satisfecho
  uint32_t born = 0;        // unix
  uint32_t lastSeen = 0;    // unix del ultimo guardado
};

void petBegin();
void petUpdate(float dtSec, bool asleep, uint32_t nowUnix);
void petAction(PetAction a);
const PetState& petState();
float petVitality();        // 0..1: que tan bien esta (la cara se apaga al bajar)
float petSizeScale();       // crece con la edad
uint32_t petAgeDays();
Need petWorstNeed();
void petDrawHud(Arduino_GFX* g);   // barras de necesidades (se muestra al mantener el dedo)
void petSaveNow();
