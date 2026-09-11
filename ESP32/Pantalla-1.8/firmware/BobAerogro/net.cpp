#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include "rtc.h"
#include "secrets.h"

static const uint32_t CHECK_EVERY_MS = 30UL * 60UL * 1000UL;   // cada 30 min
static const char* NPM_URL = "https://registry.npmjs.org/@anthropic-ai/claude-code/latest";

static volatile bool g_online = false;
static NetNews g_news;             // escrito por la tarea, leido por loop()
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static Preferences g_prefs;

static void publish(const char* text, bool celebrate) {
  portENTER_CRITICAL(&g_mux);
  strncpy(g_news.text, text, sizeof(g_news.text) - 1);
  g_news.text[sizeof(g_news.text) - 1] = 0;
  g_news.celebrate = celebrate;
  g_news.fresh = true;
  portEXIT_CRITICAL(&g_mux);
}

// Busca "version":"x.y.z" sin parser JSON (la respuesta es grande, esto basta).
static bool extractVersion(const String& body, char* out, size_t n) {
  int i = body.indexOf("\"version\":\"");
  if (i < 0) return false;
  i += 11;
  int j = body.indexOf('"', i);
  if (j < 0 || j - i >= (int)n) return false;
  body.substring(i, j).toCharArray(out, n);
  return true;
}

static void checkClaudeCode() {
  WiFiClientSecure client;
  client.setInsecure();               // solo leemos un numero de version publico
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, NPM_URL)) return;
  int code = http.GET();
  if (code != 200) { Serial.printf("[net] npm HTTP %d\n", code); http.end(); return; }
  String body = http.getString();
  http.end();
  char ver[24];
  if (!extractVersion(body, ver, sizeof(ver))) { Serial.println("[net] npm sin version"); return; }
  char last[24] = "";
  g_prefs.getString("cc_ver", last, sizeof(last));
  Serial.printf("[net] Claude Code npm=%s (visto=%s)\n", ver, last[0] ? last : "-");
  if (strcmp(ver, last) != 0) {
    g_prefs.putString("cc_ver", ver);
    char msg[64];
    if (last[0]) { snprintf(msg, sizeof(msg), "Claude Code %s!", ver); publish(msg, true); }
    else         { snprintf(msg, sizeof(msg), "Claude Code %s", ver); publish(msg, false); }
  }
}

static void syncClock() {
  configTzTime(TZ_INFO, NTP_SERVER);
  for (int i = 0; i < 40; i++) {           // hasta 20 s
    time_t now = time(nullptr);
    if (now > 1700000000) {
      rtcSet((uint32_t)now);            // el RTC guarda UTC; rtcNow() lo lee igual (solo importan diferencias)
      struct tm lt; localtime_r(&now, &lt);
      Serial.printf("[net] hora NTP %02d:%02d:%02d\n", lt.tm_hour, lt.tm_min, lt.tm_sec);
      return;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  Serial.println("[net] NTP no respondio");
}

static void netTask(void*) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);                 // menos calor y consumo; no necesitamos velocidad
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t lastCheck = 0;
  bool clockDone = false;
  for (;;) {
    bool on = WiFi.status() == WL_CONNECTED;
    if (on != g_online) {
      g_online = on;
      Serial.printf("[net] wifi %s%s\n", on ? "ok " : "caido", on ? WiFi.localIP().toString().c_str() : "");
      if (!on) { WiFi.reconnect(); }
    }
    if (on) {
      if (!clockDone) { syncClock(); clockDone = true; }
      if (!lastCheck || millis() - lastCheck > CHECK_EVERY_MS) { lastCheck = millis(); checkClaudeCode(); }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void netBegin() {
  g_prefs.begin("bobnet", false);
  xTaskCreatePinnedToCore(netTask, "net", 10240, nullptr, 1, nullptr, 0);
}

NetNews netPoll() {
  NetNews out;
  portENTER_CRITICAL(&g_mux);
  if (g_news.fresh) { out = g_news; g_news.fresh = false; }
  portEXIT_CRITICAL(&g_mux);
  return out;
}

bool netOnline() { return g_online; }
