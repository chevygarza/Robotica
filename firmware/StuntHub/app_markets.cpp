#include "app_markets.h"
#include "certs.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

Coin g_coins[MKT_MAX];
int  g_coinCount = 0;

static SemaphoreHandle_t s_mtx = nullptr;
static volatile bool s_req   = true;   // fetch al arrancar
static volatile bool s_dirty = false;

// Monedas a seguir (id de CoinGecko + símbolo a mostrar)
static const char *COIN_IDS[]  = { "bitcoin", "ethereum", "solana" };
static const char *COIN_SYMS[] = { "BTC", "ETH", "SOL" };
static const int   COIN_N = 3;

bool markets_lock(uint32_t ms) { return s_mtx && xSemaphoreTake(s_mtx, pdMS_TO_TICKS(ms)) == pdTRUE; }
void markets_unlock() { if (s_mtx) xSemaphoreGive(s_mtx); }
bool markets_consumeDirty() { bool d = s_dirty; s_dirty = false; return d; }
void markets_request() { s_req = true; }

static void fetchMarkets() {
  WiFiClientSecure client;
  client.setCACert(ISRG_ROOT_X1);
  HTTPClient https;
  String ids;
  for (int i = 0; i < COIN_N; i++) { if (i) ids += "%2C"; ids += COIN_IDS[i]; }
  String url = "https://api.coingecko.com/api/v3/simple/price?ids=" + ids +
               "&vs_currencies=usd&include_24hr_change=true";
  if (!https.begin(client, url)) return;
  int code = https.GET();
  Serial.printf("[mkt] HTTP=%d\n", code);
  if (code == 200) {
    String body = https.getString();
    JsonDocument doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      if (markets_lock(50)) {
        g_coinCount = COIN_N;
        for (int i = 0; i < COIN_N; i++) {
          strncpy(g_coins[i].sym, COIN_SYMS[i], sizeof(g_coins[i].sym) - 1);
          JsonObject c = doc[COIN_IDS[i]];
          g_coins[i].price = c["usd"] | 0.0;
          g_coins[i].chg24 = c["usd_24h_change"] | 0.0;
          g_coins[i].valid = true;
        }
        s_dirty = true;
        markets_unlock();
      }
    }
  }
  https.end();
}

static void mktTask(void *pv) {
  while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
  const uint32_t REFRESH = 5UL * 60UL * 1000UL;   // cada 5 min (CoinGecko gratis)
  uint32_t last = 0; bool first = true;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED && (s_req || first || millis() - last > REFRESH)) {
      fetchMarkets();
      last = millis(); first = false; s_req = false;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void markets_begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(mktTask, "mkt", 8192, nullptr, 1, nullptr, 0);
}
