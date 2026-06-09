#pragma once
#include <Arduino.h>

// Datos del clima (Open-Meteo, Monterrey)
struct WeatherData {
  bool  valid    = false;
  float tempC    = 0;
  float feelsC   = 0;
  float windKmh  = 0;
  float tMax     = 0;
  float tMin     = 0;
  int   humidity = 0;
  int   code     = 0;     // código WMO
  bool  isDay    = true;
  float pressure = 0;     // hPa
  float uvMax    = 0;
  int   precipProb = 0;   // % prob. lluvia (máx del día)
  // Pronóstico por hora: próximas 5 horas
  bool  hourlyValid = false;
  int   hrHour[5] = {0,0,0,0,0};  // hora del día (0-23)
  int   hrProb[5] = {0,0,0,0,0};  // % lluvia
  int   hrTemp[5] = {0,0,0,0,0};  // °C
  bool  rainSoon  = false;        // ¿lluvia >=50% en próximas 5h?
  int   rainHour  = -1;           // hora del primer slot lluvioso
  int   rainProb  = 0;            // % de ese slot
};

// Perfil de X (@stuntech)
struct XProfile {
  bool valid = false;
  char name[64]     = "";
  char username[32] = "";
  long followers = 0;
  long following = 0;
  long tweets    = 0;
  long listed    = 0;
  char bio[160]    = "";
  char created[24] = "";   // ISO; mostramos "YYYY-MM"
};

// Estado global compartido (protegido por mutex)
struct AppState {
  bool wifiUp    = false;
  bool timeValid = false;
  char status[48] = "Iniciando";
  WeatherData weather;
  XProfile    x;
};

extern AppState g_state;

void net_begin();                  // arranca WiFi + NTP + task de fetch
bool net_lock(uint32_t ms = 30);   // toma el mutex del estado
void net_unlock();                 // libera el mutex
void net_request_x();              // pide refrescar X (a demanda; tope 1/min)
void net_request_weather();        // pide refrescar clima (gratis)
