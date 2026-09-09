#include "battery.h"
#include "pins.h"

#define BAT_MUESTRAS   16
#define BAT_PERIODO  2000
#define BAT_MIN_MV   2500   // por debajo: no hay bateria, el pin esta al aire

static uint16_t mv = 0;
static uint32_t ultima = 0;
static uint32_t ultimoLog = 0;

// La curva de una celda de litio no es una recta: pasa casi toda su vida entre
// 3.7 y 4.0V. Un mapeo lineal de 3.0 a 4.2 diria 50% cuando en realidad queda
// el 20%, y eso hace que el indicador mienta justo cuando mas importa.
struct Punto { uint16_t mv; uint8_t pct; };
static const Punto CURVA[] = {
  { 4200, 100 }, { 4060, 90 }, { 3980, 80 }, { 3920, 70 }, { 3870, 60 },
  { 3820,  50 }, { 3790, 40 }, { 3770, 30 }, { 3740, 20 }, { 3680, 10 },
  { 3450,   5 }, { 3000,  0 },
};

void bat_begin() {
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);   // hasta ~3.1V
}

void bat_tick() {
  uint32_t ahora = millis();
  if (ahora - ultima < BAT_PERIODO) return;
  ultima = ahora;

  uint32_t suma = 0;
  for (uint8_t i = 0; i < BAT_MUESTRAS; i++) suma += analogReadMilliVolts(PIN_BAT_ADC);
  mv = (uint16_t)((suma / BAT_MUESTRAS) * 2);       // el divisor parte a la mitad

  // Una linea por minuto: sirve para ver la curva de carga y descarga completa
  // desde el monitor serial, que es la mejor forma de entender una celda.
  if (bat_presente() && ahora - ultimoLog > 60000) {
    ultimoLog = ahora;
    Serial.printf("[bateria] %u mV   %u%%\n", mv, bat_pct());
  }
}

bool     bat_presente() { return mv > BAT_MIN_MV; }
uint16_t bat_mv()       { return mv; }

uint8_t bat_pct() {
  if (!bat_presente()) return 0;
  const uint8_t n = sizeof(CURVA) / sizeof(CURVA[0]);
  if (mv >= CURVA[0].mv)     return 100;
  if (mv <= CURVA[n - 1].mv) return 0;
  for (uint8_t i = 1; i < n; i++) {
    if (mv >= CURVA[i].mv) {
      const Punto& a = CURVA[i];        // el mas bajo
      const Punto& b = CURVA[i - 1];    // el mas alto
      uint16_t rango = b.mv - a.mv;
      return (uint8_t)(a.pct + (uint32_t)(mv - a.mv) * (b.pct - a.pct) / rango);
    }
  }
  return 0;
}
