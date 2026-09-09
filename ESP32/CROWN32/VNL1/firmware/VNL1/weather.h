// VNL-1 — Clima del dia. Open-Meteo.
//
// Sin llave de API a proposito: estas cajas se regalan, y un servicio que pida
// registrarse convierte un regalo en un tramite. Open-Meteo es abierto y no
// pide nada. Si algun dia deja de responder, el reloj sigue dando la hora — el
// clima simplemente no se dibuja.
//
// La consulta corre en su propia tarea del nucleo 0, igual que el sondeo de la
// perilla: HTTPClient bloquea mientras negocia TLS, y hacerlo en el loop
// principal se sentiria como un tiron en la pantalla.
#pragma once
#include <Arduino.h>

// Monterrey. Cambialo aqui si la caja va a vivir en otra ciudad; son las
// coordenadas que le pasamos a la API, no hay nada mas que tocar.
#define CLIMA_LAT  "25.6866"
#define CLIMA_LON  "-100.3161"

void weather_begin();

bool        weather_ok();      // hay una lectura buena
int         weather_temp();    // grados centigrados, redondeados
const char* weather_texto();   // "Despejado", "Lluvia"... ASCII, sin acentos
