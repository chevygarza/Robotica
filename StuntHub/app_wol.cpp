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
