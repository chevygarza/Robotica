#include "bridge.h"

static char g_line[96];
static size_t g_len = 0;

static BridgeMsg parse(const char* line) {
  BridgeMsg m;
  if (!strncmp(line, "EV ", 3)) {
    const char* e = line + 3;
    if (!strcmp(e, "hello")) m.ev = BridgeEvent::Hello;
    else if (!strcmp(e, "working")) m.ev = BridgeEvent::Working;
    else if (!strcmp(e, "done")) m.ev = BridgeEvent::Done;
    else if (!strcmp(e, "notify")) m.ev = BridgeEvent::Notify;
    else if (!strcmp(e, "error")) m.ev = BridgeEvent::Error;
    else if (!strcmp(e, "bye")) m.ev = BridgeEvent::Bye;
  } else if (!strncmp(line, "SAY ", 4)) {
    m.ev = BridgeEvent::Say;
    strncpy(m.text, line + 4, sizeof(m.text) - 1);
  } else if (!strncmp(line, "ROT ", 4)) {
    m.ev = BridgeEvent::Rot;
    strncpy(m.text, line + 4, sizeof(m.text) - 1);
  } else if (!strcmp(line, "PING")) {
    Serial.println("PONG");
  }
  if (m.ev != BridgeEvent::None) Serial.printf("[bridge] %s\n", line);
  return m;
}

BridgeMsg bridgePoll() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (g_len) { g_line[g_len] = 0; g_len = 0; return parse(g_line); }
    } else if (g_len < sizeof(g_line) - 1) g_line[g_len++] = c;
    else g_len = 0;   // linea absurda: descartar
  }
  return BridgeMsg();
}
