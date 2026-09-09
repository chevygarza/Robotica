// VNL-1 — Medicion de la bateria por el convertidor analogico.
//
// El modulo UPS entrega 5V constantes, asi que la placa no puede saber nada de
// la celda por su cuenta. Se le lleva el voltaje de la bateria al IO4 —que
// quedo libre cuando el DFPlayer se fue al UART— partido a la mitad por dos
// resistencias iguales:
//
//     Bateria (+) ──[100k]──┬──► IO4
//                           │
//                         [100k]
//                           │
//                          GND
//
// El divisor consume 21 microamperes y baja los 4.2V de la celda llena a 2.1V,
// dentro de lo que el ESP32 puede leer.
#pragma once
#include <Arduino.h>

void     bat_begin();
void     bat_tick();

bool     bat_presente();   // false si no hay divisor conectado
uint16_t bat_mv();         // milivolts de la celda
uint8_t  bat_pct();        // 0..100
