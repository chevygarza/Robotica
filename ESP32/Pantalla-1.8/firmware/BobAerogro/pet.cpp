#include "pet.h"
#include <Preferences.h>
#include "board.h"

static PetState g_st;
static Preferences g_prefs;
static float g_sinceSave = 0;
static uint32_t g_nowUnix = 0;

// Puntos por hora que pierde cada necesidad despierto
static const float DECAY_PER_H[(int)Need::COUNT] = { 8.f, 5.f, 10.f, 6.f };
static const float SLEEP_GAIN_PER_H = 30.f;      // dormido recupera sueño
static const char* NAMES[(int)Need::COUNT] = { "Hambre", "Sueno", "Diversion", "Carino" };
static const uint32_t MAX_CATCHUP_S = 48 * 3600; // apagado mas de 2 dias no castiga mas

static float clamp100(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

static void applyDecay(float seconds, bool asleep) {
  float h = seconds / 3600.f;
  for (int i = 0; i < (int)Need::COUNT; i++) {
    if (i == (int)Need::Sleep && asleep) g_st.need[i] = clamp100(g_st.need[i] + SLEEP_GAIN_PER_H * h);
    else g_st.need[i] = clamp100(g_st.need[i] - DECAY_PER_H[i] * h);
  }
}

void petSaveNow() {
  g_prefs.putBytes("st", &g_st, sizeof(g_st));
  g_sinceSave = 0;
}

void petBegin() {
  g_prefs.begin("bob", false);
  if (g_prefs.getBytesLength("st") == sizeof(g_st)) g_prefs.getBytes("st", &g_st, sizeof(g_st));
}

// Se llama una vez con la hora real ya disponible: aplica lo que paso apagado.
static bool g_caughtUp = false;
static void catchUp(uint32_t nowUnix) {
  g_caughtUp = true;
  if (!nowUnix) return;
  if (!g_st.born) { g_st.born = nowUnix; g_st.lastSeen = nowUnix; Serial.println("[pet] Bob acaba de nacer"); petSaveNow(); return; }
  if (nowUnix > g_st.lastSeen) {
    uint32_t away = nowUnix - g_st.lastSeen;
    if (away > MAX_CATCHUP_S) away = MAX_CATCHUP_S;
    applyDecay((float)away, false);
    Serial.printf("[pet] estuvo apagado %lu s\n", (unsigned long)away);
  }
  g_st.lastSeen = nowUnix;
  petSaveNow();
}

void petUpdate(float dt, bool asleep, uint32_t nowUnix) {
  if (nowUnix) g_nowUnix = nowUnix;
  if (!g_caughtUp) catchUp(nowUnix);
  applyDecay(dt, asleep);
  g_sinceSave += dt;
  if (g_sinceSave > 300) { if (g_nowUnix) g_st.lastSeen = g_nowUnix; petSaveNow(); }
}

void petAction(PetAction a) {
  switch (a) {
    case PetAction::Feed:   g_st.need[(int)Need::Hunger] = clamp100(g_st.need[(int)Need::Hunger] + 25); break;
    case PetAction::Caress: g_st.need[(int)Need::Love]   = clamp100(g_st.need[(int)Need::Love] + 12); break;
    case PetAction::Tickle: g_st.need[(int)Need::Fun]    = clamp100(g_st.need[(int)Need::Fun] + 10); break;
    case PetAction::Play:   g_st.need[(int)Need::Fun]    = clamp100(g_st.need[(int)Need::Fun] + 6);
                            g_st.need[(int)Need::Sleep]  = clamp100(g_st.need[(int)Need::Sleep] - 2); break;
  }
  if (g_nowUnix) g_st.lastSeen = g_nowUnix;
  petSaveNow();
  Serial.printf("[pet] hambre=%.0f sueno=%.0f diversion=%.0f carino=%.0f\n",
                g_st.need[0], g_st.need[1], g_st.need[2], g_st.need[3]);
}

const PetState& petState() { return g_st; }

Need petWorstNeed() {
  int w = 0;
  for (int i = 1; i < (int)Need::COUNT; i++) if (g_st.need[i] < g_st.need[w]) w = i;
  return (Need)w;
}

float petVitality() {
  float worst = g_st.need[(int)petWorstNeed()];
  float avg = 0; for (int i = 0; i < (int)Need::COUNT; i++) avg += g_st.need[i]; avg /= (int)Need::COUNT;
  float v = 0.6f * (worst / 100.f) + 0.4f * (avg / 100.f);
  return 0.35f + 0.65f * v;          // nunca se apaga del todo
}

uint32_t petAgeDays() {
  if (!g_st.born || !g_nowUnix || g_nowUnix < g_st.born) return 0;
  return (g_nowUnix - g_st.born) / 86400;
}

float petSizeScale() {
  uint32_t d = petAgeDays(); if (d > 30) d = 30;
  return 0.9f + 0.2f * (d / 30.f);
}

void petDrawHud(Arduino_GFX* g) {
  const int n = (int)Need::COUNT;
  const int barW = 70, barH = 12, gap = 14;
  const int x0 = (LCD_WIDTH - (n * barW + (n - 1) * gap)) / 2;
  const int y = LCD_HEIGHT - 58;
  g->setTextSize(1);
  for (int i = 0; i < n; i++) {
    int x = x0 + i * (barW + gap);
    float v = g_st.need[i] / 100.f;
    uint16_t c = v > 0.5f ? rgb565(0x2ECC71u) : (v > 0.2f ? rgb565(0xFFB020u) : rgb565(0xFF4040u));
    g->drawRoundRect(x, y, barW, barH, 4, rgb565(0x55595Fu));
    int fill = (int)((barW - 4) * v);
    if (fill > 0) g->fillRoundRect(x + 2, y + 2, fill, barH - 4, 3, c);
    g->setTextColor(rgb565(0xB8BEC6u));
    g->setCursor(x, y + barH + 6);
    g->print(NAMES[i]);
  }
  g->setTextSize(2);
  g->setTextColor(rgb565(0x8A9099u));
  g->setCursor(x0, y - 26);
  g->printf("Dia %lu", (unsigned long)petAgeDays());
}
