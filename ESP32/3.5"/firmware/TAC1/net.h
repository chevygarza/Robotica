// TAC-1 — Red: WiFi con credenciales en NVS, escaneo y hora por NTP.
//
// La red se captura desde la pantalla. secrets.h (si existe) solo siembra la
// primera vez, cuando la NVS esta vacia. Todo es no bloqueante: la UI nunca
// espera a un router.
#pragma once
#include <Arduino.h>

enum NetState : uint8_t { NET_OFF = 0, NET_CONNECTING, NET_UP, NET_FAILED };

void        net_begin();
void        net_tick();
NetState    net_state();
const char* net_ssid();            // la red configurada ("" si ninguna)
const char* net_ip();              // "" si no hay
int         net_rssi();

// Conectar a una red nueva. Se guarda en NVS solo si logra asociar.
void        net_connect(const char* ssid, const char* pass);

// Escaneo asincrono. Los resultados viven hasta el siguiente net_scan_start().
void        net_scan_start();
int         net_scan_count();      // -1 mientras escanea
const char* net_scan_ssid(int i);
int         net_scan_rssi(int i);
bool        net_scan_open(int i);

// Hora. Valida solo cuando NTP ya contesto.
bool        net_time(struct tm* out);
