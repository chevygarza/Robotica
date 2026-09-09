#include "cloud.h"
#include "net.h"
#include "certs.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WEATHER_LAT
#define WEATHER_LAT  25.6866      // Monterrey
#define WEATHER_LON -100.3161
#endif

#define WX_MS      (10UL * 60UL * 1000UL)   // clima cada 10 min
#define MK_MS      ( 5UL * 60UL * 1000UL)   // mercados cada 5 min (CoinGecko gratis)
#define RETRY_MS   (30UL * 1000UL)

static SemaphoreHandle_t mtx;
static Weather wx;
static Coin    coins[COIN_N];
static int     coinCount;
static char    mkUpdated[6];
static double  mcap;
static float   btcDom;
static volatile bool wxDirty, mkDirty;

static const char* COIN_IDS[COIN_N]  = { "bitcoin", "ethereum", "solana" };
static const char* COIN_SYMS[COIN_N] = { "BTC", "ETH", "SOL" };

static void stamp(char* out) {
  struct tm t;
  if (net_time(&t)) snprintf(out, 6, "%02d:%02d", t.tm_hour, t.tm_min);
  else out[0] = 0;
}

// GET con TLS y el cuerpo en String: deserializeJson SIEMPRE desde getString(),
// nunca desde el stream (falla mudo). Regla heredada de StuntHub.
static bool getJson(const String& url, const char* ca, JsonDocument& doc, const char* tag) {
  WiFiClientSecure client;
  client.setCACert(ca);
  client.setHandshakeTimeout(15);
  HTTPClient https;
  https.setConnectTimeout(10000);
  https.setTimeout(10000);
  if (!https.begin(client, url)) return false;
  int code = https.GET();
  bool ok = false;
  if (code == 200) {
    String body = https.getString();
    ok = deserializeJson(doc, body) == DeserializationError::Ok;
  }
  Serial.printf("[%s] HTTP %d %s  heap %u KB\n", tag, code, ok ? "ok" : "fallo",
                (unsigned)(ESP.getFreeHeap() / 1024));
  https.end();
  return ok;
}

static bool fetchWeather() {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(WEATHER_LAT, 4) +
               "&longitude=" + String(WEATHER_LON, 4) +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m"
               "&hourly=temperature_2m,precipitation_probability"
               "&daily=temperature_2m_max,temperature_2m_min"
               "&timezone=America%2FMonterrey&forecast_days=2";
  JsonDocument doc;
  if (!getJson(url, ISRG_ROOT_X1, doc, "clima")) return false;
  JsonObject cur = doc["current"], day = doc["daily"];
  Weather w;
  w.tempC    = cur["temperature_2m"]       | 0.0f;
  w.feelsC   = cur["apparent_temperature"] | 0.0f;
  w.humidity = cur["relative_humidity_2m"] | 0;
  w.windKmh  = cur["wind_speed_10m"]       | 0.0f;
  w.code     = cur["weather_code"]         | 0;
  w.isDay    = (int)(cur["is_day"] | 1) == 1;
  w.tMax     = day["temperature_2m_max"][0] | 0.0f;
  w.tMin     = day["temperature_2m_min"][0] | 0.0f;
  struct tm t;
  if (net_time(&t)) {
    JsonArray hp = doc["hourly"]["precipitation_probability"], ht = doc["hourly"]["temperature_2m"];
    for (int i = 0; i < 5; i++) {
      int idx = t.tm_hour + 1 + i;                 // a partir de la siguiente hora
      w.hrHour[i] = idx % 24;
      w.hrProb[i] = hp[idx] | 0;
      w.hrTemp[i] = (int)lroundf(ht[idx] | 0.0f);
    }
  }
  stamp(w.updated);
  w.valid = true;
  xSemaphoreTake(mtx, portMAX_DELAY);
  wx = w;
  xSemaphoreGive(mtx);
  wxDirty = true;
  return true;
}

static bool fetchMarkets() {
  String ids;
  for (int i = 0; i < COIN_N; i++) { if (i) ids += "%2C"; ids += COIN_IDS[i]; }
  JsonDocument doc;
  if (!getJson("https://api.coingecko.com/api/v3/simple/price?ids=" + ids +
               "&vs_currencies=usd&include_24hr_change=true", GTS_ROOT_R4, doc, "mkt")) return false;
  xSemaphoreTake(mtx, portMAX_DELAY);
  for (int i = 0; i < COIN_N; i++) {
    JsonObject c = doc[COIN_IDS[i]];
    strlcpy(coins[i].sym, COIN_SYMS[i], sizeof(coins[i].sym));
    coins[i].price = c["usd"]            | 0.0;
    coins[i].chg24 = c["usd_24h_change"] | 0.0;
  }
  coinCount = COIN_N;
  stamp(mkUpdated);
  xSemaphoreGive(mtx);
  mkDirty = true;

  // El mercado entero, para el renglon de abajo. Si falla, se queda el anterior.
  JsonDocument g;
  if (getJson("https://api.coingecko.com/api/v3/global", GTS_ROOT_R4, g, "global")) {
    xSemaphoreTake(mtx, portMAX_DELAY);
    mcap   = g["data"]["total_market_cap"]["usd"]   | 0.0;
    btcDom = g["data"]["market_cap_percentage"]["btc"] | 0.0f;
    xSemaphoreGive(mtx);
    mkDirty = true;
  }
  return true;
}

static void cloudTask(void*) {
  uint32_t wxAt = 0, mkAt = 0;
  bool wxOk = false, mkOk = false, first = true;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (net_state() != NET_UP) continue;
    uint32_t now = millis();
    if (first || now - wxAt > (wxOk ? WX_MS : RETRY_MS)) { wxOk = fetchWeather(); wxAt = millis(); }
    if (first || now - mkAt > (mkOk ? MK_MS : RETRY_MS)) { mkOk = fetchMarkets(); mkAt = millis(); }
    first = false;
  }
}

void cloud_begin() {
  mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(cloudTask, "cloud", 12288, nullptr, 1, nullptr, 0);
}

bool cloud_weather(Weather* out) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  *out = wx;
  xSemaphoreGive(mtx);
  return out->valid;
}
int cloud_coins(Coin* out) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  for (int i = 0; i < coinCount; i++) out[i] = coins[i];
  xSemaphoreGive(mtx);
  return coinCount;
}
bool cloud_weather_dirty()  { bool d = wxDirty; wxDirty = false; return d; }
bool cloud_markets_dirty()  { bool d = mkDirty; mkDirty = false; return d; }
const char* cloud_markets_updated() { return mkUpdated; }
double      cloud_mcap()    { return mcap; }
float       cloud_btc_dom() { return btcDom; }

const char* weather_text(int c) {
  if (c == 0)               return "Despejado";
  if (c <= 2)               return "Poco Nublado";
  if (c == 3)               return "Nublado";
  if (c <= 48)              return "Niebla";
  if (c <= 57)              return "Llovizna";
  if (c <= 67)              return "Lluvia";
  if (c <= 77)              return "Nieve";
  if (c <= 82)              return "Chubascos";
  if (c <= 86)              return "Nieve";
  return "Tormenta";
}
