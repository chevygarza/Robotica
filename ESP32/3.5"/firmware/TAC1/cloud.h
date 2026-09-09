// TAC-1 — Datos de la nube: clima (Open-Meteo) y mercados (CoinGecko).
//
// Un solo task en el core 0 los trae EN SECUENCIA: dos handshakes TLS a la
// vez en el core 0 disparan el watchdog (visto en StuntHub). La UI pregunta
// con *_dirty() una vez por segundo y solo repinta cuando algo llego.
#pragma once
#include <Arduino.h>

struct Weather {
  bool  valid = false;
  float tempC = 0, feelsC = 0, windKmh = 0, tMax = 0, tMin = 0;
  int   humidity = 0, code = 0;            // code: WMO
  bool  isDay = true;
  int   hrHour[5] = {0}, hrProb[5] = {0}, hrTemp[5] = {0};   // proximas 5 horas
  char  updated[6] = "";                   // "22:31"
};

#define COIN_N 3
struct Coin {
  char   sym[6] = "";
  double price  = 0;
  double chg24  = 0;
};

void cloud_begin();
bool cloud_weather(Weather* out);          // copia; false si aun no hay
bool cloud_weather_dirty();                // true UNA vez por dato nuevo
int  cloud_coins(Coin* out);               // copia; 0 si aun no hay
bool cloud_markets_dirty();
const char* cloud_markets_updated();       // "22:31" o ""
double cloud_mcap();                       // capitalizacion total, USD (0 si no hay)
float  cloud_btc_dom();                    // % de dominancia de BTC
const char* weather_text(int wmo);         // codigo WMO -> texto ASCII
