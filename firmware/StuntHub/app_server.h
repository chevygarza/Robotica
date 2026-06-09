#pragma once
#include <Arduino.h>

struct ServerHealth {
  bool  valid     = false;
  bool  reachable = false;
  char  err[28]   = "";
  char  status[12] = "";        // healthy | warn | critical
  char  uptime[24] = "";
  float load[3]   = {0, 0, 0};
  int   procs     = 0;
  bool  net_up    = false;
  float down_mbps = -1;         // -1 = null/desconocido
  float up_mbps   = -1;
  float ping_ms   = -1;
  char  thermal[12] = "";
  long  ssd_total = 0, ssd_used = 0, ssd_free = 0;
  int   ssd_pct   = 0;
  char  ssd_smart[16] = "";
  float ram_total = 0, ram_used = 0, ram_free = 0;
  int   ram_pct   = 0;
  char  top_cpu_name[24] = ""; float top_cpu = 0;
  char  top_ram_name[24] = ""; float top_ram = 0;
  bool  pc_valid  = false;   // health.json trae gamer_pc
  bool  pc_online = false;   // PC gamer responde a ping
};
extern ServerHealth g_srv;

void server_begin();
void server_request();
bool server_lock(uint32_t ms = 30);
void server_unlock();
