#pragma once
#include <Arduino.h>

enum TmeState { TME_OFFLINE, TME_IDLE, TME_STARTING, TME_CHECK1, TME_CHECK2, TME_DONE, TME_ERROR };

struct TmeStatus {
  TmeState state = TME_OFFLINE;
  int   pct   = 0;
  int   eta_s = -1;
  bool  running = false;
  bool  wifiUp  = false;     // ¿conectada al WiFi? (distinto de ver al agente)
  char  label[44] = "";
};
extern TmeStatus g_tme;

void tme_begin();           // WiFi + task de polling (cada 1.5s)
void tme_request_reset();   // POST /reset (async, idempotente en el agente)
bool tme_lock(uint32_t ms = 30);
void tme_unlock();
