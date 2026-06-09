#include "app_server.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// === Endpoint del Mac Mini (LAN, HTTP plano). Editar si cambia la IP/puerto. ===
#define SERVER_URL  "http://192.168.86.66:8765/health.json"

ServerHealth g_srv;
static SemaphoreHandle_t s_mtx = nullptr;
static volatile bool s_req = true;

bool server_lock(uint32_t ms) { return s_mtx && xSemaphoreTake(s_mtx, pdMS_TO_TICKS(ms)) == pdTRUE; }
void server_unlock() { if (s_mtx) xSemaphoreGive(s_mtx); }
void server_request() { s_req = true; }

static void setErr(const char *e) {
  if (server_lock(30)) { g_srv.reachable = false; strncpy(g_srv.err, e, sizeof(g_srv.err) - 1); server_unlock(); }
}

static void fetchServer() {
  WiFiClient client; HTTPClient http;
  http.setConnectTimeout(4000);
  if (!http.begin(client, SERVER_URL)) { setErr("conexion fallo"); return; }
  http.setTimeout(5000);
  int code = http.GET();
  Serial.printf("[srv] HTTP=%d\n", code);
  if (code == 200) {
    String body = http.getString();
    JsonDocument doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      if (server_lock(60)) {
        strncpy(g_srv.status, doc["status"] | "", sizeof(g_srv.status) - 1);
        strncpy(g_srv.uptime, doc["uptime"] | "", sizeof(g_srv.uptime) - 1);
        JsonArray ld = doc["load"].as<JsonArray>();
        for (int i = 0; i < 3; i++) g_srv.load[i] = (i < (int)ld.size()) ? (ld[i] | 0.0f) : 0.0f;
        g_srv.procs = doc["procs"] | 0;
        JsonObject net = doc["net"];
        g_srv.net_up    = net["up"] | false;
        g_srv.down_mbps = net["down_mbps"] | -1.0f;
        g_srv.up_mbps   = net["up_mbps"]   | -1.0f;
        g_srv.ping_ms   = net["ping_ms"]   | -1.0f;
        strncpy(g_srv.thermal, doc["thermal"] | "", sizeof(g_srv.thermal) - 1);
        JsonObject ssd = doc["ssd"];
        g_srv.ssd_total = ssd["total_gi"] | 0;
        g_srv.ssd_used  = ssd["used_gi"]  | 0;
        g_srv.ssd_free  = ssd["free_gi"]  | 0;
        g_srv.ssd_pct   = ssd["pct"]      | 0;
        strncpy(g_srv.ssd_smart, ssd["smart"] | "", sizeof(g_srv.ssd_smart) - 1);
        JsonObject ram = doc["ram"];
        g_srv.ram_total = ram["total_gb"] | 0.0f;
        g_srv.ram_used  = ram["used_gb"]  | 0.0f;
        g_srv.ram_free  = ram["free_gb"]  | 0.0f;
        g_srv.ram_pct   = ram["pct"]      | 0;
        JsonArray tc = doc["top_cpu"].as<JsonArray>();
        if (tc.size() > 0) { strncpy(g_srv.top_cpu_name, tc[0]["name"] | "", sizeof(g_srv.top_cpu_name) - 1); g_srv.top_cpu = tc[0]["cpu"] | 0.0f; }
        JsonArray tr = doc["top_ram"].as<JsonArray>();
        if (tr.size() > 0) { strncpy(g_srv.top_ram_name, tr[0]["name"] | "", sizeof(g_srv.top_ram_name) - 1); g_srv.top_ram = tr[0]["ram"] | 0.0f; }
        JsonObject gp = doc["gamer_pc"];
        g_srv.pc_valid  = !gp.isNull();
        g_srv.pc_online = gp["online"] | false;
        g_srv.valid = true; g_srv.reachable = true; g_srv.err[0] = 0;
        server_unlock();
      }
    } else { setErr("JSON err"); }
  } else if (code == 503) {
    setErr("sin datos aun");
  } else {
    setErr((String("HTTP ") + code).c_str());
  }
  http.end();
}

static void srvTask(void *pv) {
  while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
  vTaskDelay(pdMS_TO_TICKS(7000));   // escalonar arranque (ver nota en app_markets)
  const uint32_t REFRESH = 45UL * 1000UL;   // cada 45s (el JSON se regenera cada 60s)
  uint32_t last = 0; bool first = true;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED && (s_req || first || millis() - last > REFRESH)) {
      fetchServer();
      last = millis(); first = false; s_req = false;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void server_begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(srvTask, "srv", 8192, nullptr, 1, nullptr, 0);
}
