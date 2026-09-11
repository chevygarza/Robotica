#include "pet.h"
#include <Preferences.h>
#include "board.h"

static PetState g_st;
static Preferences g_prefs;
static float g_sinceSave = 0;
static uint32_t g_nowUnix = 0;

// Puntos por hora que pierde cada necesidad despierto
static const float DECAY_PER_H[(int)Need::COUNT] = { 8.f, 5.f, 10.f, 6.f };
static const float SLEEP_GAIN_PER_H = 100.f;     // dormido: una hora = bateria llena
static const char* NAMES[(int)Need::COUNT] = { "Hambre", "Sueno", "Diversion", "Carino" };
static const uint32_t MAX_CATCHUP_S = 48 * 3600; // apagado mas de 2 dias no castiga mas

static float clamp100(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

static void applyDecay(float seconds, bool asleep) {
  float h = seconds / 3600.f;
  for (int i = 0; i < (int)Need::COUNT; i++) {
    if (i == (int)Need::Sleep && asleep) g_st.need[i] = clamp100(g_st.need[i] + SLEEP_GAIN_PER_H * h);
    else g_st.need[i] = clamp100(g_st.need[i] - DECAY_PER_H[i] * h * (asleep ? 0.5f : 1.f));   // dormido gasta menos
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
  // Pantalla completa estilo videojuego: titulo + 4 barras de vida segmentadas.
  static const char* LABELS[(int)Need::COUNT] = { "HAMBRE", "ENERGIA", "DIVERSION", "AMOR" };
  const int n = (int)Need::COUNT;
  const int W = g->width(), H = g->height();
  const int rowH = (H - 100) / n;                            // 87 vertical, 67 horizontal
  const int segs = 10, segW = 28, segH = rowH - 34 < 30 ? rowH - 34 : 30, segGap = 4;
  const int barW = segs * segW + (segs - 1) * segGap;      // 316
  const int x0 = (W - barW) / 2;
  const uint16_t frame = rgb565(0x3A4150u), empty = rgb565(0x141820u), ink = rgb565(0xE8ECF2u);

  g->fillScreen(rgb565(COL_BG));
  g->drawRoundRect(6, 6, W - 12, H - 12, 18, frame);
  g->drawRoundRect(8, 8, W - 16, H - 16, 16, frame);

  g->setTextSize(4);
  g->setTextColor(rgb565(0x2ECC71u));
  g->setCursor(x0, 30);
  g->print("BOB");
  g->setTextSize(3);
  g->setTextColor(ink);
  g->setCursor(x0 + 96, 38);
  g->printf("DIA %lu", (unsigned long)petAgeDays());

  int y = 96;
  for (int i = 0; i < n; i++) {
    float v = g_st.need[i] / 100.f;
    uint16_t c = v > 0.5f ? rgb565(0x2ECC71u) : (v > 0.2f ? rgb565(0xFFB020u) : rgb565(0xFF4040u));
    g->setTextSize(3);
    g->setTextColor(ink);
    g->setCursor(x0, y);
    g->print(LABELS[i]);
    g->setTextColor(c);
    g->setCursor(x0 + barW - 3 * 18, y);
    g->printf("%3d", (int)(g_st.need[i] + .5f));
    int lit = (int)(v * segs + .5f);
    int by = y + 32;
    for (int s = 0; s < segs; s++) {
      int sx = x0 + s * (segW + segGap);
      if (s < lit) g->fillRoundRect(sx, by, segW, segH, 5, c);
      else { g->fillRoundRect(sx, by, segW, segH, 5, empty); g->drawRoundRect(sx, by, segW, segH, 5, frame); }
    }
    y += rowH;
  }
}
