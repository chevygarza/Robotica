#include "app_net.h"
#include "secrets.h"
#include "certs.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"

AppState g_state;
static SemaphoreHandle_t s_mutex = nullptr;

static volatile bool s_reqWx = false;
void net_request_weather() { s_reqWx = true; }

bool net_lock(uint32_t ms) {
  if (!s_mutex) return false;
  return xSemaphoreTake(s_mutex, pdMS_TO_TICKS(ms)) == pdTRUE;
}
void net_unlock() {
  if (s_mutex) xSemaphoreGive(s_mutex);
}

static void setStatus(const char *s) {
  if (net_lock(20)) { strncpy(g_state.status, s, sizeof(g_state.status) - 1); net_unlock(); }
}

// ---------- Clima: Open-Meteo (sin API key) ----------
static void fetchWeather() {
  WiFiClientSecure client;
  client.setCACert(ISRG_ROOT_X1);
  HTTPClient https;
  String url =
    "https://api.open-meteo.com/v1/forecast?latitude=" + String(WEATHER_LAT, 4) +
    "&longitude=" + String(WEATHER_LON, 4) +
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,surface_pressure" +
    "&hourly=temperature_2m,precipitation_probability" +
    "&daily=temperature_2m_max,temperature_2m_min,uv_index_max,precipitation_probability_max" +
    "&timezone=America%2FMonterrey&forecast_days=2";

  if (!https.begin(client, url)) { setStatus("Clima: conexion fallo"); Serial.println("[clima] begin() fallo"); return; }
  int code = https.GET();
  Serial.printf("[clima] HTTP=%d  heap=%u\n", code, (unsigned)ESP.getFreeHeap());
  if (code == 200) {
    String body = https.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    Serial.printf("[clima] body=%uB parse=%s\n", (unsigned)body.length(), err.c_str());
    if (!err) {
      JsonObject cur = doc["current"];
      JsonObject day = doc["daily"];
      if (net_lock(50)) {
        g_state.weather.tempC    = cur["temperature_2m"]      | 0.0f;
        g_state.weather.feelsC   = cur["apparent_temperature"]| 0.0f;
        g_state.weather.humidity = cur["relative_humidity_2m"]| 0;
        g_state.weather.windKmh  = cur["wind_speed_10m"]      | 0.0f;
        g_state.weather.code     = cur["weather_code"]        | 0;
        g_state.weather.isDay    = (int)(cur["is_day"] | 1) == 1;
        g_state.weather.tMax     = day["temperature_2m_max"][0] | 0.0f;
        g_state.weather.tMin     = day["temperature_2m_min"][0] | 0.0f;
        g_state.weather.pressure   = cur["surface_pressure"] | 0.0f;
        g_state.weather.uvMax      = day["uv_index_max"][0] | 0.0f;
        g_state.weather.precipProb = day["precipitation_probability_max"][0] | 0;
        // Próximas 5 horas (a partir de la siguiente hora local)
        struct tm tnow;
        if (getLocalTime(&tnow, 0)) {
          JsonArray hp = doc["hourly"]["precipitation_probability"];
          JsonArray ht = doc["hourly"]["temperature_2m"];
          int base = tnow.tm_hour + 1;
          bool rs = false; int rh = -1, rp = 0;
          for (int i = 0; i < 5; i++) {
            int idx  = base + i;
            int prob = hp[idx] | 0;
            float tp = ht[idx] | 0.0f;
            g_state.weather.hrHour[i] = idx % 24;
            g_state.weather.hrProb[i] = prob;
            g_state.weather.hrTemp[i] = (int)(tp + 0.5f);
            if (!rs && prob >= 50) { rs = true; rh = idx % 24; rp = prob; }
          }
          g_state.weather.hourlyValid = true;
          g_state.weather.rainSoon = rs;
          g_state.weather.rainHour = rh;
          g_state.weather.rainProb = rp;
        }
        g_state.weather.valid    = true;
        Serial.printf("[clima] OK tempC=%.1f\n", (double)g_state.weather.tempC);
        net_unlock();
      }
    } else { setStatus("Clima: JSON err"); }
  } else { setStatus((String("Clima HTTP ") + code).c_str()); }
  https.end();
}

// ---------- Task de red ----------
static void netTask(void *pv) {
  Serial.println("\n[net] netTask iniciado");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  setStatus("Conectando WiFi");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  Serial.printf("[net] WiFi.status=%d  heap=%u\n", WiFi.status(), (unsigned)ESP.getFreeHeap());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[net] IP="); Serial.print(WiFi.localIP());
    Serial.printf("  RSSI=%d dBm\n", WiFi.RSSI());
    if (net_lock(50)) { g_state.wifiUp = true; net_unlock(); }
    setStatus("WiFi OK");
    // Monterrey = CST6 todo el año (sin horario de verano)
    configTzTime(TZ_INFO, NTP_SERVER, "time.google.com");
    struct tm tm0;
    if (getLocalTime(&tm0, 8000)) { if (net_lock(20)) { g_state.timeValid = true; net_unlock(); } }
  } else {
    setStatus("WiFi sin conexion");
  }

  const uint32_t WX_MS = 10UL * 60UL * 1000UL;  // clima: auto cada 10 min (GRATIS)
  uint32_t lastWx = 0;
  bool firstWx = true;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      // Clima: automático (gratis) o a demanda. El reloj lo da NTP (arriba).
      if (firstWx || s_reqWx || millis() - lastWx > WX_MS) {
        fetchWeather();
        lastWx = millis(); firstWx = false; s_reqWx = false;
      }
    } else {
      if (net_lock(20)) { g_state.wifiUp = false; net_unlock(); }
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void net_begin() {
  if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}
