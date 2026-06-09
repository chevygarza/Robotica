#include "app_hue.h"
#include "secrets.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

HueRoom  g_rooms[HUE_MAX_ROOMS];   int g_roomCount  = 0;
HueLight g_lights[HUE_MAX_LIGHTS]; int g_lightCount = 0;

static SemaphoreHandle_t s_mtx = nullptr;
static QueueHandle_t     s_q   = nullptr;
static volatile bool     s_dirty = false;
static volatile bool     s_ready = false;
static char              s_status[40] = "Luces: ...";

bool hue_lock(uint32_t ms) { return s_mtx && xSemaphoreTake(s_mtx, pdMS_TO_TICKS(ms)) == pdTRUE; }
void hue_unlock() { if (s_mtx) xSemaphoreGive(s_mtx); }
bool hue_consumeDirty() { bool d = s_dirty; s_dirty = false; return d; }
bool hue_ready() { return s_ready; }
const char* hue_status() { return s_status; }
static void setStatus(const char *s) { strncpy(s_status, s, sizeof(s_status) - 1); }

HueLight* hue_findLight(int id) {
  for (int i = 0; i < g_lightCount; i++) if (g_lights[i].id == id) return &g_lights[i];
  return nullptr;
}

// ---- Comandos en cola ----
struct HueCmd {
  uint8_t type;     // 0 = fetch, 1 = set
  bool    isRoom;
  int     id;
  int     onCmd;
  int     bri;
  uint8_t color;
};

void hue_request_rooms() {
  HueCmd c = {0, false, 0, -1, -1, HUE_NONE};
  if (s_q) xQueueSend(s_q, &c, 0);
}

void hue_set(bool isRoom, int id, int onCmd, int bri, HueColor color) {
  // Optimista: refleja el on/off de inmediato en la UI
  if (hue_lock(30)) {
    if (isRoom) {
      for (int i = 0; i < g_roomCount; i++) if (g_rooms[i].id == id) {
        for (int j = 0; j < g_rooms[i].nLights; j++) {
          HueLight *l = hue_findLight(g_rooms[i].lightIds[j]);
          if (l && onCmd >= 0) l->on = onCmd;
          else if (l && (color != HUE_NONE)) l->on = true;
        }
      }
    } else {
      HueLight *l = hue_findLight(id);
      if (l) { if (onCmd >= 0) l->on = onCmd; else if (color != HUE_NONE) l->on = true; }
    }
    s_dirty = true;
    hue_unlock();
  }
  HueCmd c = {1, isRoom, id, onCmd, bri, (uint8_t)color};
  if (s_q) xQueueSend(s_q, &c, 0);
}

// ---- HTTP ----
static String hueBase() { return String("http://") + HUE_BRIDGE_IP + "/api/" + HUE_KEY; }

static void doFetch() {
  setStatus("Luces: cargando");
  WiFiClient client; HTTPClient http;

  // /groups
  if (http.begin(client, hueBase() + "/groups")) {
    int code = http.GET();
    if (code == 200) {
      String body = http.getString();
      JsonDocument filt;
      filt["*"]["name"] = true; filt["*"]["type"] = true; filt["*"]["lights"] = true;
      JsonDocument doc;
      if (deserializeJson(doc, body, DeserializationOption::Filter(filt)) == DeserializationError::Ok) {
        if (hue_lock(80)) {
          g_roomCount = 0;
          for (JsonPair kv : doc.as<JsonObject>()) {
            const char *type = kv.value()["type"] | "";
            JsonArray lights = kv.value()["lights"].as<JsonArray>();
            int n = lights.size();
            bool isRoomOrZone = (strcmp(type, "Room") == 0 || strcmp(type, "Zone") == 0);
            if (!isRoomOrZone || n == 0) continue;           // ocultar vacíos / no-cuartos
            if (g_roomCount >= HUE_MAX_ROOMS) break;
            HueRoom &r = g_rooms[g_roomCount];
            r.id = atoi(kv.key().c_str());
            r.isZone = (strcmp(type, "Zone") == 0);
            strncpy(r.name, kv.value()["name"] | "?", sizeof(r.name) - 1);
            r.name[sizeof(r.name) - 1] = 0;
            r.nLights = 0;
            for (JsonVariant v : lights) {
              if (r.nLights >= HUE_MAX_ROOMLIGHTS) break;
              r.lightIds[r.nLights++] = atoi(v.as<const char*>());
            }
            g_roomCount++;
          }
          hue_unlock();
        }
      } else setStatus("Luces: JSON groups");
    } else setStatus("Luces: HTTP groups");
    http.end();
  }

  // /lights
  if (http.begin(client, hueBase() + "/lights")) {
    int code = http.GET();
    if (code == 200) {
      String body = http.getString();
      JsonDocument filt;
      filt["*"]["name"] = true;
      filt["*"]["state"]["on"] = true;
      filt["*"]["state"]["reachable"] = true;
      JsonDocument doc;
      if (deserializeJson(doc, body, DeserializationOption::Filter(filt)) == DeserializationError::Ok) {
        if (hue_lock(80)) {
          g_lightCount = 0;
          for (JsonPair kv : doc.as<JsonObject>()) {
            if (g_lightCount >= HUE_MAX_LIGHTS) break;
            HueLight &l = g_lights[g_lightCount];
            l.id = atoi(kv.key().c_str());
            strncpy(l.name, kv.value()["name"] | "?", sizeof(l.name) - 1);
            l.name[sizeof(l.name) - 1] = 0;
            // saneo ASCII (la fuente no trae acentos)
            for (char *p = l.name; *p; ++p) if ((unsigned char)*p < 32 || (unsigned char)*p > 126) *p = ' ';
            l.on = kv.value()["state"]["on"] | false;
            l.reachable = kv.value()["state"]["reachable"] | true;
            g_lightCount++;
          }
          hue_unlock();
        }
      } else setStatus("Luces: JSON lights");
    } else setStatus("Luces: HTTP lights");
    http.end();
  }

  // sanea nombres de cuartos también
  if (hue_lock(50)) {
    for (int i = 0; i < g_roomCount; i++)
      for (char *p = g_rooms[i].name; *p; ++p) if ((unsigned char)*p < 32 || (unsigned char)*p > 126) *p = ' ';
    hue_unlock();
  }

  s_ready = true; s_dirty = true;
  char st[40]; snprintf(st, sizeof(st), "Luces: %d cuartos, %d focos", g_roomCount, g_lightCount);
  setStatus(st);
}

static void doSet(const HueCmd &c) {
  // construye el cuerpo JSON
  String b = "{";
  bool first = true;
  auto add = [&](const String &kv) { if (!first) b += ","; b += kv; first = false; };
  if (c.onCmd == 1)      add("\"on\":true");
  else if (c.onCmd == 0) add("\"on\":false");
  if (c.bri >= 0)        add("\"bri\":" + String(c.bri));
  switch ((HueColor)c.color) {
    case HUE_WHITE: add("\"on\":true"); add("\"sat\":0"); break;
    case HUE_RED:   add("\"on\":true"); add("\"hue\":0");     add("\"sat\":254"); break;
    case HUE_GREEN: add("\"on\":true"); add("\"hue\":25500"); add("\"sat\":254"); break;
    case HUE_BLUE:  add("\"on\":true"); add("\"hue\":46920"); add("\"sat\":254"); break;
    default: break;
  }
  b += "}";

  String url = hueBase() + (c.isRoom ? ("/groups/" + String(c.id) + "/action")
                                     : ("/lights/" + String(c.id) + "/state"));
  WiFiClient client; HTTPClient http;
  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    int code = http.sendRequest("PUT", b);
    Serial.printf("[hue] PUT %s %s -> %d\n", c.isRoom ? "group" : "light", b.c_str(), code);
    http.end();
  }
}

static void hueTask(void *pv) {
  // espera WiFi
  while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
  HueCmd c;
  for (;;) {
    if (xQueueReceive(s_q, &c, portMAX_DELAY) == pdTRUE) {
      if (WiFi.status() != WL_CONNECTED) continue;
      if (c.type == 0) doFetch();
      else             doSet(c);
    }
  }
}

void hue_begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
  if (!s_q)   s_q   = xQueueCreate(12, sizeof(HueCmd));
  xTaskCreatePinnedToCore(hueTask, "hue", 8192, nullptr, 1, nullptr, 0);
}
