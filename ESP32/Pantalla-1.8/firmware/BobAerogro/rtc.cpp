#include "rtc.h"
#include <Wire.h>
#include <time.h>

static const uint8_t ADDR = 0x51;
static bool g_ok = false;

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

// timegm propio: newlib no lo trae y mktime depende de TZ (que NTP cambia).
static uint32_t timegm_u(const struct tm& t) {
  int y = t.tm_year + 1900, m = t.tm_mon + 1, d = t.tm_mday;
  if (m <= 2) { y--; m += 12; }
  uint32_t days = 365UL * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + d - 719469;
  return days * 86400UL + t.tm_hour * 3600UL + t.tm_min * 60UL + t.tm_sec;
}

static bool rd(uint8_t reg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(ADDR); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ADDR, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

uint32_t rtcNow() {
  if (!g_ok) return 0;
  uint8_t b[7];
  if (!rd(0x04, b, 7)) return 0;
  struct tm t = {};
  t.tm_sec = bcd2bin(b[0] & 0x7F); t.tm_min = bcd2bin(b[1] & 0x7F); t.tm_hour = bcd2bin(b[2] & 0x3F);
  t.tm_mday = bcd2bin(b[3] & 0x3F); t.tm_mon = bcd2bin(b[5] & 0x1F) - 1; t.tm_year = bcd2bin(b[6]) + 100;
  return timegm_u(t);        // siempre UTC, sin importar TZ
}

bool rtcSet(uint32_t unix) {
  if (!g_ok) return false;
  time_t u = unix; struct tm t; gmtime_r(&u, &t);
  Wire.beginTransmission(ADDR); Wire.write(0x04);
  Wire.write(bin2bcd(t.tm_sec)); Wire.write(bin2bcd(t.tm_min)); Wire.write(bin2bcd(t.tm_hour));
  Wire.write(bin2bcd(t.tm_mday)); Wire.write(t.tm_wday); Wire.write(bin2bcd(t.tm_mon + 1)); Wire.write(bin2bcd(t.tm_year - 100));
  return Wire.endTransmission() == 0;
}

static uint32_t compileTime() {
  // __DATE__ = "Sep 10 2026", __TIME__ = "16:20:00"
  static const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  struct tm t = {};
  char mon[4] = {0}; int d, y, hh, mm, ss;
  sscanf(__DATE__, "%3s %d %d", mon, &d, &y); sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);
  t.tm_mon = (strstr(months, mon) - months) / 3; t.tm_mday = d; t.tm_year = y - 1900;
  t.tm_hour = hh; t.tm_min = mm; t.tm_sec = ss;
  return timegm_u(t);
}

bool rtcBegin() {
  uint8_t b[7];
  g_ok = rd(0x04, b, 7);
  if (!g_ok) { Serial.println("[rtc] PCF85063 no responde — sin hora real"); return false; }
  bool lost = (b[0] & 0x80) != 0;                      // OS: oscilador se detuvo (pila vacia)
  uint32_t now = rtcNow();
  if (lost || now < 1700000000UL) {                    // sin poner (antes de 2023-11)
    rtcSet(compileTime());
    Serial.printf("[rtc] puesto con hora de compilacion (%s %s)\n", __DATE__, __TIME__);
  }
  Serial.printf("[rtc] ok unix=%lu\n", (unsigned long)rtcNow());
  return true;
}
