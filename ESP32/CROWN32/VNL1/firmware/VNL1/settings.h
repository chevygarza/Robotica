// VNL-1 — Ajustes del aparato, guardados en NVS.
//
// Todo lo que antes eran constantes en el codigo y resulto ser cuestion de
// gusto: cuanto tarda la pantalla en retirarse, que tan tenue se queda, y si
// los LEDs estorban. Se llega girando, como a cualquier disco.
//
// Una idea por campo. Antes el anillo eran dos (encendido + color) y el reloj
// otros dos (si/no + tipo); cada par describia una sola cosa, y separados
// permitian estados que no significan nada: "LEDs No, color Caratula".
#pragma once
#include <Arduino.h>

// El anillo. Apagado es un modo mas, no un interruptor aparte.
#define LED_OFF     0
#define LED_AMBAR   1    // un solo tono calido, fijo
#define LED_COVER   2    // los ocho colores de la caratula
#define LED_RGB     3    // arcoiris en transicion lenta

// Que queda en la pantalla cuando el aparato se retira.
#define REPOSO_APAGAR   0
#define REPOSO_DIGITAL  1
#define REPOSO_ANALOGO  2

// Que hacer cuando se acaba el album.
#define FIN_DETENER   0
#define FIN_REPETIR   1
#define FIN_INFINITO  2

struct Ajustes {
  uint8_t brillo;       // % de pantalla en uso. Techo real de todo lo demas.
  uint8_t reposoMin;    // minutos sin tocar antes de retirarse. 0 = nunca
  uint8_t enReposo;     // REPOSO_*
  uint8_t luzReposo;    // % DEL brillo, ya retirada. Relativo, no absoluto:
                        // asi el reposo nunca puede quedar mas claro que el uso.
  uint8_t ledsModo;     // LED_*
  uint8_t brilloLeds;   // % del anillo
  uint8_t alFin;        // FIN_*
};

void      ajustes_begin();
void      ajustes_save();
Ajustes&  ajustes();

// Un campo que no puede hacer nada no se puede elegir: Luz Reposo con la
// pantalla en Apagar, o Brillo LEDs con el anillo en Apagados. Se dibuja
// tenue para que se vea que existe, pero el giro pasa de largo.
bool aj_campo_activo(uint8_t campo);

// Valores que puede tomar cada campo, para que el giro recorra opciones
// sensatas en vez de numeros de uno en uno.
uint8_t aj_ciclo_reposo(uint8_t actual, int8_t dir);
uint8_t aj_ciclo_pct(uint8_t actual, int8_t dir, uint8_t minimo);
void    aj_texto_reposo(char* out, size_t n, uint8_t min);
const char* aj_texto_fin(uint8_t modo);
const char* aj_texto_leds(uint8_t modo);
const char* aj_texto_en_reposo(uint8_t modo);
