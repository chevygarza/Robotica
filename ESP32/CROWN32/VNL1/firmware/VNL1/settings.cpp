#include "settings.h"
#include <Preferences.h>

static Preferences prefs;
static Ajustes aj = { 100, 3, REPOSO_DIGITAL, 12, LED_AMBAR, 100, FIN_DETENER };

void ajustes_begin() {
  prefs.begin("vnl1", false);
  aj.brillo     = prefs.getUChar("aj_bri",  100);
  aj.reposoMin  = prefs.getUChar("aj_rep",  3);
  aj.luzReposo  = prefs.getUChar("aj_lrep", 12);
  aj.brilloLeds = prefs.getUChar("aj_bled", 100);
  aj.alFin      = prefs.getUChar("aj_fin",  FIN_DETENER);

  // Los valores guardados tambien tienen que respetar el minimo del campo. En
  // el modelo anterior "0%" era legitimo porque apagar la pantalla y atenuarla
  // eran la misma perilla; hoy apagar se elige en En Reposo, y un 0 heredado
  // solo produce un reloj puesto que no se ve. Lo que la perilla no puede
  // alcanzar, la memoria tampoco debe imponer.
  if (aj.luzReposo  < 20) aj.luzReposo  = 20;
  if (aj.brilloLeds < 10) aj.brilloLeds = 10;
  if (aj.brillo     < 30) aj.brillo     = 30;

  // Migracion desde el modelo anterior. Las llaves viejas se traducen una sola
  // vez y se borran: si se quedaran, cada arranque volveria a pisar lo nuevo.
  bool viejo = prefs.isKey("aj_leds") || prefs.isKey("aj_rel");
  if (viejo) {
    uint8_t color = prefs.getUChar("aj_luz", 0);        // 0 ambar, 1 cover, 2 rgb
    aj.ledsModo = prefs.getBool("aj_leds", true) ? (uint8_t)(color + 1) : LED_OFF;
    aj.enReposo = !prefs.getBool("aj_rel", true)
                    ? REPOSO_APAGAR
                    : (prefs.getUChar("aj_relt", 0) == 1 ? REPOSO_ANALOGO
                                                         : REPOSO_DIGITAL);
    prefs.remove("aj_luz");  prefs.remove("aj_leds");
    prefs.remove("aj_rel");  prefs.remove("aj_relt");
    prefs.remove("aj_lmus");
  } else {
    aj.ledsModo = prefs.getUChar("aj_led",  LED_AMBAR);
    aj.enReposo = prefs.getUChar("aj_srep", REPOSO_DIGITAL);
  }
  prefs.end();
  if (viejo) ajustes_save();
}

void ajustes_save() {
  prefs.begin("vnl1", false);
  prefs.putUChar("aj_bri",  aj.brillo);
  prefs.putUChar("aj_rep",  aj.reposoMin);
  prefs.putUChar("aj_srep", aj.enReposo);
  prefs.putUChar("aj_lrep", aj.luzReposo);
  prefs.putUChar("aj_led",  aj.ledsModo);
  prefs.putUChar("aj_bled", aj.brilloLeds);
  prefs.putUChar("aj_fin",  aj.alFin);
  prefs.end();
}

Ajustes& ajustes() { return aj; }

bool aj_campo_activo(uint8_t campo) {
  if (campo == 3) return aj.enReposo != REPOSO_APAGAR;   // Luz Reposo
  if (campo == 5) return aj.ledsModo != LED_OFF;         // Brillo LEDs
  return true;
}

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

const char* aj_texto_leds(uint8_t modo) {
  switch (modo) {
    case LED_AMBAR: return "Amarillo";
    case LED_COVER: return "Caratula";
    case LED_RGB:   return "RGB";
    default:        return "Apagados";
  }
}

const char* aj_texto_en_reposo(uint8_t modo) {
  switch (modo) {
    case REPOSO_DIGITAL: return "Reloj Digital";
    case REPOSO_ANALOGO: return "Reloj Analogo";
    default:             return "Apagar";
  }
}

const char* aj_texto_fin(uint8_t modo) {
  switch (modo) {
    case FIN_REPETIR:  return "Repetir";
    case FIN_INFINITO: return "Infinito";
    default:           return "Detener";
  }
}

void aj_texto_reposo(char* out, size_t n, uint8_t min) {
  if (min == 0) snprintf(out, n, "Nunca");
  else if (min == 1) snprintf(out, n, "1 Minuto");
  else snprintf(out, n, "%u Minutos", min);
}
