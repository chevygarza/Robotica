// VNL-1 — Ajustes del aparato, guardados en NVS.
//
// Todo lo que antes eran constantes en el codigo y resulto ser cuestion de
// gusto: cuanto tarda la pantalla en retirarse, que tan tenue se queda, y si
// los LEDs estorban. Se llega girando, como a cualquier disco.
#pragma once
#include <Arduino.h>

// Que hacer cuando se acaba el album.
#define FIN_DETENER   0
#define FIN_REPETIR   1
#define FIN_INFINITO  2

struct Ajustes {
  uint8_t alFin;        // FIN_*
  uint8_t reposoMin;    // minutos sin tocar antes de bajar la luz. 0 = nunca
  uint8_t luzReposo;    // % de pantalla en reposo SIN musica
  uint8_t luzMusica;    // % de pantalla en reposo CON musica
  uint8_t brillo;       // % maximo de pantalla
  bool    leds;         // anillo encendido o no
  uint8_t brilloLeds;   // % del anillo
};

void      ajustes_begin();
void      ajustes_save();
Ajustes&  ajustes();

// Valores que puede tomar cada campo, para que el giro recorra opciones
// sensatas en vez de numeros de uno en uno.
uint8_t aj_ciclo_reposo(uint8_t actual, int8_t dir);
uint8_t aj_ciclo_pct(uint8_t actual, int8_t dir, uint8_t minimo);
void    aj_texto_reposo(char* out, size_t n, uint8_t min);
const char* aj_texto_fin(uint8_t modo);
