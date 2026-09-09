// StuntHub — Ajustes del aparato, guardados en NVS.
//
// Adaptado de VNL1 (~/Desktop/VNL1/firmware/VNL1/settings.h). Mismas decisiones,
// que ya se pagaron caro alla; lo que cambia es el namespace de NVS y que aqui
// el brillo tiene un modo AUTOMATICO, porque StuntHub ya seguia el horario por
// NTP (70% de dia, 25% de noche) y eso estaba bien pensado.
//
// Una idea por campo. El anillo eran dos campos (encendido + color) y el reloj
// otros dos (si/no + tipo); separados permitian estados que no significan nada,
// como "LEDs No, color Caratula". Fusionados, el estado imposible no existe.
#pragma once
#include <Arduino.h>

// El anillo. Apagado es un modo mas, no un interruptor aparte.
#define LED_OFF     0
#define LED_AMBAR   1    // un solo tono calido, fijo
#define LED_COVER   2    // los ocho colores de la caratula que suena
#define LED_RGB     3    // arcoiris en transicion lenta

// Que queda en la pantalla cuando el aparato se retira.
#define REPOSO_APAGAR   0
#define REPOSO_DIGITAL  1
#define REPOSO_ANALOGO  2

// Que hacer cuando se acaba el album (vive en la app de Musica, no aqui).
#define FIN_DETENER   0
#define FIN_REPETIR   1
#define FIN_INFINITO  2

// Brillo 0 = Automatico (horario por NTP). 30..100 = fijo.
#define BRILLO_AUTO   0

// Piso ABSOLUTO de pantalla. Relativo por relativo aterriza en cero: brillo 50%
// x luz reposo 20% = 10%, y a 10% el panel deja de leerse. Es una propiedad del
// hardware, no del gusto, y por eso no es configurable.
#define BRILLO_PISO  15

struct Ajustes {
  uint8_t brillo;       // BRILLO_AUTO o 30..100. Techo real de todo lo demas.
  uint8_t reposoMin;    // minutos sin actividad antes de retirarse. 0 = nunca
  uint8_t enReposo;     // REPOSO_*
  uint8_t luzReposo;    // % DEL brillo, ya retirada. Relativo, no absoluto: asi
                        // el reposo nunca queda mas claro que el uso.
  uint8_t ledsModo;     // LED_*
  uint8_t brilloLeds;   // % del anillo
  uint8_t alFin;        // FIN_*
};

void      ajustes_begin();
void      ajustes_save();
Ajustes&  ajustes();

// % real de pantalla ahora mismo: resuelve Automatico contra la hora NTP.
uint8_t aj_brillo_uso();
// % real ya dormido: fraccion del de uso, con el piso absoluto aplicado.
uint8_t aj_brillo_reposo();

// Un campo que no puede hacer nada no se puede elegir: Luz Reposo con la
// pantalla en Apagar, o Brillo LEDs con el anillo en Apagados. Se dibuja tenue
// para que se vea que existe, pero el giro pasa de largo.
bool aj_campo_activo(uint8_t campo);

// Valores que puede tomar cada campo, para que el giro recorra opciones
// sensatas en vez de numeros de uno en uno.
uint8_t aj_ciclo_brillo(uint8_t actual, int8_t dir);
uint8_t aj_ciclo_reposo(uint8_t actual, int8_t dir);
uint8_t aj_ciclo_pct(uint8_t actual, int8_t dir, uint8_t minimo);
void    aj_texto_brillo(char* out, size_t n, uint8_t v);
void    aj_texto_reposo(char* out, size_t n, uint8_t min);
const char* aj_texto_fin(uint8_t modo);
const char* aj_texto_leds(uint8_t modo);
const char* aj_texto_en_reposo(uint8_t modo);
