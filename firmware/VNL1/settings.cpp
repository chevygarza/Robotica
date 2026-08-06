#include "settings.h"
#include <Preferences.h>

static Preferences prefs;
static Ajustes aj = { FIN_DETENER, 3, 12, 50, 100, true, 100 };

void ajustes_begin() {
  prefs.begin("vnl1", false);
  aj.alFin      = prefs.getUChar("aj_fin", FIN_DETENER);
  aj.reposoMin  = prefs.getUChar("aj_rep", 3);
  aj.luzReposo  = prefs.getUChar("aj_lrep", 12);
  aj.luzMusica  = prefs.getUChar("aj_lmus", 50);
  aj.brillo     = prefs.getUChar("aj_bri", 100);
  aj.leds       = prefs.getBool("aj_leds", true);
  aj.brilloLeds = prefs.getUChar("aj_bled", 100);
  prefs.end();
}

void ajustes_save() {
  prefs.begin("vnl1", false);
  prefs.putUChar("aj_fin", aj.alFin);
  prefs.putUChar("aj_rep", aj.reposoMin);
  prefs.putUChar("aj_lrep", aj.luzReposo);
  prefs.putUChar("aj_lmus", aj.luzMusica);
  prefs.putUChar("aj_bri", aj.brillo);
  prefs.putBool("aj_leds", aj.leds);
  prefs.putUChar("aj_bled", aj.brilloLeds);
  prefs.end();
}

Ajustes& ajustes() { return aj; }

// 0 = nunca. Los saltos son los que alguien elegiria de verdad; nadie pone el
// reposo en siete minutos.
static const uint8_t REPOSOS[] = { 1, 3, 5, 10, 30, 0 };

uint8_t aj_ciclo_reposo(uint8_t actual, int8_t dir) {
  const uint8_t n = sizeof(REPOSOS);
  uint8_t k = 0;
  for (uint8_t i = 0; i < n; i++) if (REPOSOS[i] == actual) k = i;
  return REPOSOS[(k + n + dir) % n];
}

uint8_t aj_ciclo_pct(uint8_t actual, int8_t dir, uint8_t minimo) {
  int16_t v = (int16_t)actual + dir * 10;
  if (v > 100) v = minimo;
  if (v < minimo) v = 100;
  return (uint8_t)v;
}

const char* aj_texto_fin(uint8_t modo) {
  switch (modo) {
    case FIN_REPETIR:  return "repetir";
    case FIN_INFINITO: return "infinito";
    default:           return "detener";
  }
}

void aj_texto_reposo(char* out, size_t n, uint8_t min) {
  if (min == 0) snprintf(out, n, "nunca");
  else if (min == 1) snprintf(out, n, "1 minuto");
  else snprintf(out, n, "%u minutos", min);
}
