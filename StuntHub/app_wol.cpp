#include "app_wol.h"
#include "secrets.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>

static bool parseMac(const char *s, uint8_t out[6]) {
  unsigned int v[6];
  if (sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)v[i];
  return true;
}

void wol_send() {
  uint8_t mac[6];
  if (!parseMac(GAMER_MAC, mac)) { Serial.println("[wol] MAC invalida"); return; }

  // Magic packet estandar: 6x 0xFF + MAC repetida 16 veces
  uint8_t pkt[102];
  memset(pkt, 0xFF, 6);
  for (int i = 0; i < 16; i++) memcpy(pkt + 6 + i * 6, mac, 6);

  IPAddress bcast;
  bcast.fromString(WOL_BROADCAST);

  WiFiUDP udp;
  const uint16_t ports[2] = { 9, 7 };
  for (int r = 0; r < 3; r++) {            // 3 repeticiones (como wol.py)
    for (int p = 0; p < 2; p++) {
      udp.beginPacket(bcast, ports[p]);
      udp.write(pkt, sizeof(pkt));
      udp.endPacket();
    }
    delay(60);
  }
  Serial.println("[wol] magic packets enviados (broadcast :9 y :7, x3)");
}

// ---- Apagado (POST al pc_agent.ps1 en la PC, en task propio: no bloquea UI) ----
volatile int g_pcShutdownResult = 0;

static void shutdownTask(void *pv) {
  WiFiClient client; HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(4000);
  String url = String("http://") + GAMER_IP + ":" + GAMER_AGENT_PORT +
               "/shutdown?t=" + GAMER_TOKEN;
  int code = -1;
  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    // OJO: HTTP.sys de Windows rechaza POST sin cuerpo (411 Length Required)
    code = http.sendRequest("POST", "{}");
    http.end();
  }
  Serial.printf("[wol] shutdown -> %d\n", code);
  g_pcShutdownResult = (code == 200) ? 2 : -1;
  vTaskDelete(NULL);
}

void pc_shutdown_async() {
  g_pcShutdownResult = 1;
  xTaskCreatePinnedToCore(shutdownTask, "pcoff", 6144, nullptr, 1, nullptr, 0);
}

// ---- Perfil (Normal/Sim/TV): POST /<path> al pc_agent, en task propio ----
volatile int g_pcProfileResult = 0;   // 0=idle, 1=en curso, 2=ok, -1=fallo
static void profileTask(void *pv) {
  const char *path = (const char *)pv;     // literal estatico (normal|sim|tv)
  WiFiClient client; HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(4000);
  String url = String("http://") + GAMER_IP + ":" + GAMER_AGENT_PORT +
               "/" + path + "?t=" + GAMER_TOKEN;
  int code = -1;
  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    code = http.sendRequest("POST", "{}");   // HTTP.sys exige cuerpo (411 si no)
    http.end();
  }
  g_pcProfileResult = (code == 200) ? 2 : -1;   // 2=ok, -1=no respondio
  Serial.printf("[wol] profile %s -> %d\n", path, code);
  vTaskDelete(NULL);
}

void pc_profile_async(const char *path) {
  g_pcProfileResult = 1;                         // en curso
  xTaskCreatePinnedToCore(profileTask, "pcprof", 6144, (void *)path, 1, nullptr, 0);
}

// ---- Estado directo de la PC: UN task persistente que consulta /status ----
// Robusto: el task nunca se crea/destruye (no se atora). El estado lo decide el
// timestamp del ultimo GET 200: si hubo respuesta hace <10s -> encendida; si no
// -> apagada. Asi un poll colgado a una PC apagada no deja el estado pegado.
volatile int g_pcUptimeMin = -1;
volatile uint32_t g_pcLastOk = 0;     // millis del ultimo GET 200 (0 = nunca)
static volatile bool s_pollActive = false;

static void pcStatusTask(void *pv) {
  for (;;) {
    if (s_pollActive && WiFi.status() == WL_CONNECTED) {
      WiFiClient client; HTTPClient http;
      http.setConnectTimeout(3000);
      http.setTimeout(3000);
      String url = String("http://") + GAMER_IP + ":" + GAMER_AGENT_PORT +
                   "/status?t=" + GAMER_TOKEN;
      int code = -1, uptime = -1;
      if (http.begin(client, url)) {
        code = http.GET();
        if (code == 200) {
          String body = http.getString();        // {"online":true,"uptime_min":N}
          int k = body.indexOf("uptime_min");
          if (k >= 0) { int c = body.indexOf(':', k); if (c >= 0) uptime = body.substring(c + 1).toInt(); }
        }
        http.end();
      }
      if (code == 200) { g_pcLastOk = millis(); g_pcUptimeMin = uptime; }
      Serial.printf("[pc] GET -> %d up=%d\n", code, uptime);
    }
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void pc_status_begin() {
  xTaskCreatePinnedToCore(pcStatusTask, "pcstat", 8192, nullptr, 1, nullptr, 0);
}

void pc_status_active(bool on) { s_pollActive = on; }

// 1 = encendida (GET 200 hace <10s), 0 = apagada, -1 = desconocido (nunca respondio)
int pc_direct_state() {
  if (g_pcLastOk == 0) return -1;
  return (millis() - g_pcLastOk < 10000) ? 1 : 0;
}
