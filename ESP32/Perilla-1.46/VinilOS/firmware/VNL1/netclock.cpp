#include "netclock.h"
#include "secrets.h"
#include <WiFi.h>

#define REINTENTO_MS  20000

static bool     sincronizado = false;
static uint32_t ultimoIntento = 0;
static uint32_t ultimoNtp     = 0;

void clock_begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);              // basta con estar en hora; ahorra corriente
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  ultimoIntento = millis();

  // configTzTime arranca SNTP en segundo plano: no espera a nadie.
  configTzTime(TZ_POSIX, "pool.ntp.org", "time.nist.gov");
  Serial.printf("reloj: conectando a \"%s\"\n", WIFI_SSID);
}

bool clock_wifi()  { return WiFi.status() == WL_CONNECTED; }
bool clock_ready() { return sincronizado; }

bool clock_now(struct tm* out) {
  if (!sincronizado) return false;
  return getLocalTime(out, 5);
}

void clock_hhmm(char* out, size_t n) {
  struct tm t;
  if (clock_now(&t)) snprintf(out, n, "%02d:%02d", t.tm_hour, t.tm_min);
  else               snprintf(out, n, "--:--");
}

const char* clock_status() {
  if (sincronizado)  return "En Hora";
  if (clock_wifi())  return "Buscando Hora";
  return "Sin Red";
}

void clock_tick() {
  static uint32_t ultimoChequeo = 0;
  uint32_t ahora = millis();
  if (ahora - ultimoChequeo < 1000) return;   // una vez por segundo basta
  ultimoChequeo = ahora;

  if (!clock_wifi()) {
    sincronizado = false;
    if (ahora - ultimoIntento > REINTENTO_MS) {
      ultimoIntento = ahora;
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    return;
  }

  struct tm t;
  if (getLocalTime(&t, 5)) {
    // Antes de 2021 significa que SNTP todavia no contesto y el reloj arranco
    // en 1970. Es la unica forma confiable de saber si la hora ya sirve.
    bool buena = (t.tm_year + 1900) > 2021;
    if (buena && !sincronizado) {
      Serial.printf("reloj: en hora, %02d:%02d del %02d/%02d/%d\n",
                    t.tm_hour, t.tm_min, t.tm_mday, t.tm_mon + 1,
                    t.tm_year + 1900);
      ultimoNtp = ahora;
    }
    sincronizado = buena;
  }

  // Resincronizar cada 6 horas: el reloj interno del ESP32 se va unos segundos
  // al dia, y una alarma que llega tarde no sirve de alarma.
  if (sincronizado && ahora - ultimoNtp > 6UL * 3600UL * 1000UL) {
    ultimoNtp = ahora;
    configTzTime(TZ_POSIX, "pool.ntp.org", "time.nist.gov");
  }
}
