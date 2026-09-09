#pragma once
#include <Arduino.h>

#define HUE_MAX_ROOMS      20
#define HUE_MAX_LIGHTS     64
#define HUE_MAX_ROOMLIGHTS 44

struct HueLight {
  int  id   = 0;
  bool on   = false;
  bool reachable = true;
  char name[22] = "";
};

struct HueRoom {
  int  id    = 0;
  bool isZone = false;
  int  nLights = 0;
  int  lightIds[HUE_MAX_ROOMLIGHTS] = {0};
  char name[22] = "";
};

// Colores básicos
enum HueColor { HUE_NONE = 0, HUE_WHITE, HUE_RED, HUE_GREEN, HUE_BLUE };

extern HueRoom  g_rooms[HUE_MAX_ROOMS];   extern int g_roomCount;
extern HueLight g_lights[HUE_MAX_LIGHTS]; extern int g_lightCount;

void  hue_begin();
bool  hue_lock(uint32_t ms = 30);
void  hue_unlock();
bool  hue_consumeDirty();        // true (una vez) si los datos cambiaron
bool  hue_ready();               // ya cargó al menos una vez
const char* hue_status();

void  hue_request_rooms();       // refetch /groups + /lights (async)

// onCmd: -1 = toggle, 0 = off, 1 = on.  bri: -1 = no cambiar, 1..254.  color: HueColor.
void  hue_set(bool isRoom, int id, int onCmd, int bri, HueColor color);

HueLight* hue_findLight(int id);
