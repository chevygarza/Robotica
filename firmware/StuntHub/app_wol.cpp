#include "app_wol.h"
#include "secrets.h"
#include <WiFi.h>
#include <WiFiUdp.h>

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
