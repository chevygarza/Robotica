#include "tme_net.h"
#include "secrets.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

TmeStatus g_tme;
static SemaphoreHandle_t s_mtx = nullptr;
static volatile bool s_doReset = false;
static volatile bool s_doSilence = false;
static volatile bool s_doAbort = false;

bool tme_lock(uint32_t ms) { return s_mtx && xSemaphoreTake(s_mtx, pdMS_TO_TICKS(ms)) == pdTRUE; }
void tme_unlock() { if (s_mtx) xSemaphoreGive(s_mtx); }
void tme_request_reset() { s_doReset = true; }
void tme_request_silence() { s_doSilence = true; }
void tme_request_abort() { s_doAbort = true; }

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
  Serial.printf("[tme] GET /status -> %d\n", code);
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

static void doPost(const char *path) {
  WiFiClient client; HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(4000);
  if (!http.begin(client, baseUrl() + path)) return;
  http.addHeader("Content-Type", "application/json");
  int code = http.sendRequest("POST", "{}");
  Serial.printf("[tme] POST %s -> %d\n", path, code);
  http.end();
}

static void tmeTask(void *pv) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[tme] conectando a '%s'...\n", WIFI_SSID);
  uint32_t lastOk = 0, lastPoll = 0;
  bool wasConn = false;
  for (;;) {
    bool conn = (WiFi.status() == WL_CONNECTED);
    if (tme_lock(10)) { g_tme.wifiUp = conn; tme_unlock(); }
    if (conn && !wasConn) {
      Serial.print("[tme] ✅ WiFi OK  IP=");
      Serial.print(WiFi.localIP());
      Serial.print("  GW=");
      Serial.println(WiFi.gatewayIP());
    }
    if (!conn && wasConn) Serial.println("[tme] ⚠️ WiFi perdido");
    wasConn = conn;
    if (!conn) {
      static uint32_t lastLog = 0;
      if (millis() - lastLog > 3000) {
        lastLog = millis();
        Serial.printf("[tme] sin WiFi (status=%d)\n", WiFi.status());
      }
      if (tme_lock(20)) { g_tme.state = TME_OFFLINE; tme_unlock(); }
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    if (s_doReset) {
      s_doReset = false;
      doPost("/reset");
      lastPoll = 0;                       // poll inmediato tras disparar
    }
    if (s_doSilence) {
      s_doSilence = false;
      doPost("/silence");                 // acknowledge: apaga alarma + dialogos
    }
    if (s_doAbort) {
      s_doAbort = false;
      doPost("/abort");                   // rescate: limpia todo del lado Windows
      lastPoll = 0;
    }
    if (millis() - lastPoll > 1500) {
      lastPoll = millis();
      if (pollStatus()) lastOk = millis();
      else if (millis() - lastOk > 8000) {   // agente inalcanzable
        if (tme_lock(20)) { g_tme.state = TME_OFFLINE; tme_unlock(); }
      }
    }

    // REDUNDANCIA: WiFi "conectado" pero >60s sin ver al agente = zombie.
    // Forzar disconnect+reconnect para que el stack vuelva a empezar.
    static uint32_t lastZombieKick = 0;
    if (millis() - lastOk > 60000 && millis() - lastZombieKick > 60000) {
      lastZombieKick = millis();
      Serial.println("[tme] WiFi zombie? forzando reconexion completa");
      WiFi.disconnect(true);
      vTaskDelay(pdMS_TO_TICKS(500));
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      lastOk = millis();   // dale 60s mas antes del proximo kick
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void tme_begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(tmeTask, "tme", 8192, nullptr, 1, nullptr, 0);
}
