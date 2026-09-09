#include "net.h"
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef TZ_INFO
#define TZ_INFO    "CST6"
#define NTP_SERVER "pool.ntp.org"
#endif

#define CONNECT_MS   15000    // cuanto se le da a un intento
#define RETRY_MS     20000    // cada cuanto se reintenta la red guardada

static NetState st = NET_OFF;
static char     ssid[33], pass[65];
static char     ip[16];
static bool     unsaved;          // el intento en curso viene de la pantalla
static bool     timeOk;
static uint32_t t0, lastTick;

static void begin(const char* s, const char* p) {
  st = NET_CONNECTING;
  t0 = millis();
  WiFi.disconnect();
  WiFi.begin(s, p);
  Serial.printf("[net] conectando a \"%s\"\n", s);
}

void net_begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("tac-1");
  WiFi.setSleep(false);                        // va por USB: latencia antes que ahorro

  Preferences p;
  p.begin("net", true);
  p.getString("ssid", ssid, sizeof(ssid));
  p.getString("pass", pass, sizeof(pass));
  p.end();
#ifdef WIFI_SSID
  if (!ssid[0]) {                              // semilla de compilacion, una sola vez
    strlcpy(ssid, WIFI_SSID, sizeof(ssid));
    strlcpy(pass, WIFI_PASSWORD, sizeof(pass));
  }
#endif
  // SNTP arranca en segundo plano y contesta cuando haya red: no espera a nadie.
  configTzTime(TZ_INFO, NTP_SERVER, "time.google.com");
  if (ssid[0]) begin(ssid, pass);
}

void net_connect(const char* s, const char* p) {
  strlcpy(ssid, s, sizeof(ssid));
  strlcpy(pass, p, sizeof(pass));
  unsaved = true;
  begin(ssid, pass);
}

void net_tick() {
  uint32_t now = millis();
  if (now - lastTick < 250) return;
  lastTick = now;

  wl_status_t w = WiFi.status();
  switch (st) {
    case NET_CONNECTING:
      if (w == WL_CONNECTED) {
        st = NET_UP;
        strlcpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip));
        Serial.printf("[net] %s  rssi %d\n", ip, WiFi.RSSI());
        if (unsaved) {
          Preferences p;
          p.begin("net", false);
          p.putString("ssid", ssid);
          p.putString("pass", pass);
          p.end();
          unsaved = false;
        }
      } else if (w == WL_CONNECT_FAILED || w == WL_NO_SSID_AVAIL || now - t0 > CONNECT_MS) {
        st = NET_FAILED;
        t0 = now;
        Serial.printf("[net] fallo (%d)\n", (int)w);
        if (unsaved) {                         // la red nueva no entro: se olvida
          unsaved = false;
          Preferences p;
          p.begin("net", true);
          p.getString("ssid", ssid, sizeof(ssid));
          p.getString("pass", pass, sizeof(pass));
          p.end();
        }
      }
      break;
    case NET_UP:
      if (w != WL_CONNECTED) { st = NET_CONNECTING; t0 = now; ip[0] = 0; WiFi.reconnect(); }
      break;
    case NET_FAILED:
      if (ssid[0] && now - t0 > RETRY_MS) begin(ssid, pass);
      break;
    case NET_OFF:
      break;
  }

  if (!timeOk && st == NET_UP) {
    struct tm t;
    // Antes de 2021 es que SNTP aun no contesta y el reloj sigue en 1970.
    if (getLocalTime(&t, 0) && t.tm_year + 1900 > 2021) {
      timeOk = true;
      Serial.printf("[net] en hora %02d:%02d\n", t.tm_hour, t.tm_min);
    }
  }
}

NetState    net_state() { return st; }
const char* net_ssid()  { return ssid; }
const char* net_ip()    { return st == NET_UP ? ip : ""; }
int         net_rssi()  { return st == NET_UP ? WiFi.RSSI() : 0; }

void        net_scan_start()      { WiFi.scanDelete(); WiFi.scanNetworks(true); }
int         net_scan_count()      { int n = WiFi.scanComplete(); return n < 0 ? -1 : n; }
const char* net_scan_ssid(int i) {
  // WiFi.SSID() devuelve un String temporal: se copia antes de que muera.
  static char s[33];
  strlcpy(s, WiFi.SSID(i).c_str(), sizeof(s));
  return s;
}
int         net_scan_rssi(int i)  { return WiFi.RSSI(i); }
bool        net_scan_open(int i)  { return WiFi.encryptionType(i) == WIFI_AUTH_OPEN; }

bool net_time(struct tm* out) { return timeOk && getLocalTime(out, 0); }
