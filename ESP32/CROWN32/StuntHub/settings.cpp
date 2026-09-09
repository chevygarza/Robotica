#include "settings.h"
#include <Preferences.h>
#include "time.h"

static Preferences prefs;
// Por defecto: brillo automatico (lo que StuntHub ya hacia), reposo a 3 min con
// reloj. "Apagar" NO es el valor de fabrica: con la pantalla negra el aparato se
// lee como descompuesto — lo miras, no hay nada, y crees que se colgo.
static Ajustes aj = { BRILLO_AUTO, 3, REPOSO_DIGITAL, 20, LED_AMBAR, 100, FIN_DETENER };

void ajustes_begin() {
  prefs.begin("stunthub", false);
  aj.brillo     = prefs.getUChar("aj_bri",  BRILLO_AUTO);
  aj.reposoMin  = prefs.getUChar("aj_rep",  3);
  aj.enReposo   = prefs.getUChar("aj_srep", REPOSO_DIGITAL);
  aj.luzReposo  = prefs.getUChar("aj_lrep", 20);
  aj.ledsModo   = prefs.getUChar("aj_led",  LED_AMBAR);
  aj.brilloLeds = prefs.getUChar("aj_bled", 100);
  aj.alFin      = prefs.getUChar("aj_fin",  FIN_DETENER);

  // Los valores GUARDADOS tambien respetan el minimo del campo. Es el error mas
  // sutil de todos y ya costo caro en VinilOS: se puso el minimo en la perilla
  // pero no en NVS, y un 0 heredado sobrevivio produciendo un reloj puesto que
  // no se veia. Lo que la perilla no puede alcanzar, la memoria tampoco impone.
  if (aj.luzReposo  < 20) aj.luzReposo  = 20;
  if (aj.brilloLeds < 10) aj.brilloLeds = 10;
  if (aj.brillo != BRILLO_AUTO && aj.brillo < 30) aj.brillo = 30;
  if (aj.enReposo > REPOSO_ANALOGO) aj.enReposo = REPOSO_DIGITAL;
  if (aj.ledsModo > LED_RGB)        aj.ledsModo = LED_AMBAR;
  if (aj.alFin    > FIN_INFINITO)   aj.alFin    = FIN_DETENER;

  // Migracion de llaves viejas: se traducen UNA vez y se BORRAN. Si se quedaran,
  // cada arranque volveria a pisar lo nuevo. StuntHub no tiene llaves viejas
  // todavia (nunca uso NVS), pero el patron queda puesto para la proxima vez.
  if (prefs.isKey("aj_viejo")) {
    prefs.remove("aj_viejo");
    prefs.end();
    ajustes_save();
    return;
  }
  prefs.end();
}

void ajustes_save() {
  prefs.begin("stunthub", false);
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

// Automatico = el horario que StuntHub ya seguia: 70% de dia, 25% de 10pm a 7am.
// Sin hora todavia (NTP no ha respondido) se asume dia: mas vale de mas que de
// menos, porque un aparato oscuro al arrancar se lee como muerto.
uint8_t aj_brillo_uso() {
  if (aj.brillo != BRILLO_AUTO) return aj.brillo;
  struct tm t;
  if (!getLocalTime(&t, 0)) return 70;
  return (t.tm_hour >= 22 || t.tm_hour < 7) ? 25 : 70;
}

// El brillo de reposo es una FRACCION del de uso, nunca un absoluto: con dos
// niveles independientes, brillo 50% + luz reposo 80% dejaba la pantalla en
// reposo MAS CLARA que usandola. Siendo relativo, ese caso no es alcanzable.
uint8_t aj_brillo_reposo() {
  uint16_t v = (uint16_t)aj_brillo_uso() * aj.luzReposo / 100;
  return (v < BRILLO_PISO) ? BRILLO_PISO : (uint8_t)v;
}

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

// Automatico va ANTES de los porcentajes: es el valor que el aparato ya tenia y
// el que mas gente quiere. Girando hacia abajo desde 30 se vuelve a el.
uint8_t aj_ciclo_brillo(uint8_t actual, int8_t dir) {
  if (actual == BRILLO_AUTO) return (dir > 0) ? 30 : 100;
  int16_t v = (int16_t)actual + dir * 10;
  if (v > 100) return BRILLO_AUTO;
  if (v < 30)  return BRILLO_AUTO;
  return (uint8_t)v;
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

void aj_texto_brillo(char* out, size_t n, uint8_t v) {
  if (v == BRILLO_AUTO) snprintf(out, n, "Automatico");
  else                  snprintf(out, n, "%u%%", v);
}

void aj_texto_reposo(char* out, size_t n, uint8_t min) {
  if (min == 0)      snprintf(out, n, "Nunca");
  else if (min == 1) snprintf(out, n, "1 Minuto");
  else               snprintf(out, n, "%u Minutos", min);
}
