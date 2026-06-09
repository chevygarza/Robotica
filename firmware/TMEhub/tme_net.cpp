#include "tme_net.h"
#include "secrets.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

TmeStatus g_tme;
static SemaphoreHandle_t s_mtx = nullptr;
static volatile bool s_doReset = false;

bool tme_lock(uint32_t ms) { return s_mtx && xSemaphoreTake(s_mtx, pdMS_TO_TICKS(ms)) == pdTRUE; }
void tme_unlock() { if (s_mtx) xSemaphoreGive(s_mtx); }
void tme_request_reset() { s_doReset = true; }

static String baseUrl() {
  return String("http://") + TME_AGENT_HOST + ":" + TME_AGENT_PORT;
}

static TmeState parseState(const char *s) {
  if (!strcmp(s, "starting")) return TME_STARTING;
  if (!strcmp(s, "check1"))   return TME_CHECK1;
  if (!strcmp(s, "check2"))   return TME_CHECK2;
  if (!strcmp(s, "done"))     return TME_DONE;
  if (!strcmp(s, "error"))    return TME_ERROR;
  return TME_IDLE;
}

static bool pollStatus() {
  WiFiClient client; HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  if (!http.begin(client, baseUrl() + "/status")) return false;
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    String body = http.getString();
    JsonDocument doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      if (tme_lock(50)) {
        g_tme.state   = parseState(doc["state"] | "idle");
        g_tme.pct     = doc["pct"]   | 0;
        g_tme.eta_s   = doc["eta_s"] | -1;
        g_tme.running = doc["running"] | false;
        strncpy(g_tme.label, doc["label"] | "", sizeof(g_tme.label) - 1);
        // saneo ASCII (la fuente no trae acentos)
        for (char *p = g_tme.label; *p; ++p)
          if ((unsigned char)*p < 32 || (unsigned char)*p > 126) *p = ' ';
        tme_unlock();
        ok = true;
      }
    }
  }
  http.end();
  return ok;
}

static void doReset() {
  WiFiClient client; HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(4000);
  if (!http.begin(client, baseUrl() + "/reset")) return;
  http.addHeader("Content-Type", "application/json");
  int code = http.sendRequest("POST", "{}");
  Serial.printf("[tme] POST /reset -> %d\n", code);
  http.end();
}

static void tmeTask(void *pv) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t lastOk = 0, lastPoll = 0;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      if (tme_lock(20)) { g_tme.state = TME_OFFLINE; tme_unlock(); }
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    if (s_doReset) {
      s_doReset = false;
      doReset();
      lastPoll = 0;                       // poll inmediato tras disparar
    }
    if (millis() - lastPoll > 1500) {
      lastPoll = millis();
      if (pollStatus()) lastOk = millis();
      else if (millis() - lastOk > 8000) {   // agente inalcanzable
        if (tme_lock(20)) { g_tme.state = TME_OFFLINE; tme_unlock(); }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void tme_begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(tmeTask, "tme", 8192, nullptr, 1, nullptr, 0);
}
