#include "face.h"
#include <math.h>

// Misma geometria que la maqueta bob-aerogro.html: N muestras del contorno,
// radio base R, superelipse para la pildora, cos(k*theta) para picos/bultos.
static const int   N = 72;
static const float R = 118.f;
static const float DEG = 3.14159265f / 180.f;
static const float TAU = 6.2831853f;

struct Mood {
  uint32_t color;
  float tilt;                 // grados
  float eyeH, eyeW, eyeGap, eyeRot;
  bool ring;
  float (*shape)(float t);
};

static float superellipse(float t, float a, float b, float n) {
  float c = fabsf(cosf(t) / a), s = fabsf(sinf(t) / b);
  return powf(powf(c, n) + powf(s, n), -1.f / n);
}
static float shIdle(float t)      { return superellipse(t, 1.28f, .82f, 3.6f); }
static float shListening(float t) { return 1.02f * (1.f + .17f * cosf(3.f * (t - 1.5708f))); }
static float shTalking(float t)   { return superellipse(t, 1.2f, .9f, 2.2f) * (1.f + .13f * cosf(4.f * t + .7854f)); }
static float shSurprised(float t) { return 1.1f * (1.f + .2f * cosf(5.f * t + 1.5708f)); }
static float shAngry(float)       { return 1.f; }
static float shSleepy(float t)    { float u = cosf(t - 1.5708f); if (u < 0) u = 0; u *= u; u *= u; return .98f * (1.f + .45f * u); }

static const Mood MOODS[(int)Emotion::COUNT] = {
  /* Idle      verde pildora  */ {0x2ECC71u, -10.f, .30f, .12f, .40f,  0.f, false, shIdle},
  /* Listening azul triangulo */ {0x2196F3u,   0.f, .40f, .13f, .34f,  0.f, false, shListening},
  /* Talking   naranja nube   */ {0xFF6A00u,   0.f, .32f, .12f, .38f,  0.f, false, shTalking},
  /* Surprised cafe flor      */ {0x9C6B36u,   0.f, .44f, .14f, .36f,  0.f, false, shSurprised},
  /* Angry     negro redondo  */ {0x2C2C31u,   0.f, .16f, .12f, .40f, 22.f, true,  shAngry},
  /* Sleepy    gris gota      */ {0x8A8C90u,  -8.f, .07f, .13f, .38f,  0.f, false, shSleepy},
};
static const uint32_t RING_COLOR = 0x5B5B63u;

const char* emotionName(Emotion e) {
  switch (e) {
    case Emotion::Idle: return "IDLE";
    case Emotion::Listening: return "LISTENING";
    case Emotion::Talking: return "TALKING";
    case Emotion::Surprised: return "SURPRISED";
    case Emotion::Angry: return "ANGRY";
    case Emotion::Sleepy: return "SLEEPY";
    default: return "?";
  }
}

// ---- conducta autonoma: lo que Bob hace solo cuando nadie lo molesta ----
enum class Beh : uint8_t { None = 0, Stretch, Hop, Wander, Roll, Wink, Curious, Giggle, Daydream, Nap, Peekaboo, Shiver, Dance, COUNT };
static const char* BEH_NAMES[] = { "-", "estirarse", "brincar", "pasear", "rodar", "guinar", "curiosear", "reirse", "sonar", "siesta", "esconderse", "temblar", "bailar" };

// ---- estado animado ----
static Arduino_GFX* g = nullptr;
static Emotion g_emo = Emotion::Idle;
static float g_radii[N];
static float g_col[3];
static float g_tilt, g_eyeH, g_eyeW, g_eyeGap, g_eyeRot, g_ring, g_bounce, g_sx = 1, g_sy = 1, g_lookX, g_lookY;
static float g_posX = 0, g_eyeL = 1, g_eyeR = 1, g_animScale = 1;
static float g_phase = 0.f;
static uint32_t g_holdUntil = 0, g_lastMotionMs = 0, g_nextBlink = 0, g_blinkUntil = 0, g_lookUntil = 0;
static float g_lookTx = 0, g_lookTy = 0;
static float g_vitality = 1.f, g_scale = 1.f;
static FaceAction g_action = FaceAction::None;
static uint32_t g_actionUntil = 0, g_forceUntil = 0, g_lastStimulusMs = 0;
static Emotion g_forced = Emotion::Idle;
static Beh g_beh = Beh::None;
static uint32_t g_behStart = 0, g_behUntil = 0, g_nextBeh = 0;
static float g_behA = 0, g_behB = 0;     // parametros del acto en curso (destino de paseo, etc.)
static bool g_napping = false;

static float lerpf(float a, float b, float k) { return a + (b - a) * k; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float frand() { return (esp_random() % 10000) / 10000.f; }

void faceSetVitality(float v) { g_vitality = clampf(v, 0.f, 1.f); }
void faceSetScale(float s) { g_scale = s; }
void faceAction(FaceAction a) { g_action = a; g_actionUntil = millis() + (a == FaceAction::Eat ? 1200 : 700); g_beh = Beh::None; g_lastStimulusMs = millis(); }
void faceForce(Emotion e, uint32_t ms) { g_forced = e; g_forceUntil = millis() + ms; }

static char g_say[64] = "";
static uint32_t g_sayUntil = 0, g_focusUntil = 0;
static void startBehavior(Beh b, uint32_t ms) {
  g_beh = b; g_behStart = millis(); g_behUntil = g_behStart + ms;
  g_behA = frand() * 2 - 1; g_behB = frand();
  g_lastStimulusMs = millis();
}
void faceDo(FaceMove m) {
  g_action = FaceAction::None; g_forceUntil = 0;
  switch (m) {
    case FaceMove::Hop:      startBehavior(Beh::Hop, 1300); break;
    case FaceMove::Dance:    startBehavior(Beh::Dance, 3600); break;
    case FaceMove::Wink:     startBehavior(Beh::Wink, 500); break;
    case FaceMove::Curious:  startBehavior(Beh::Curious, 2500); break;
    case FaceMove::Peekaboo: startBehavior(Beh::Peekaboo, 1600); break;
  }
}
void faceSay(const char* text, uint32_t ms) { strncpy(g_say, text, sizeof(g_say) - 1); g_say[sizeof(g_say) - 1] = 0; g_sayUntil = millis() + ms; }
void faceFocus(uint32_t ms) { g_focusUntil = millis() + ms; g_lastStimulusMs = millis(); if (g_beh != Beh::None && g_beh != Beh::Dance) g_beh = Beh::None; }
Emotion faceEmotion() { return g_napping ? Emotion::Sleepy : g_emo; }

void faceBegin(Arduino_GFX* gfx) {
  g = gfx;
  const Mood& M = MOODS[0];
  for (int i = 0; i < N; i++) g_radii[i] = M.shape(i / (float)N * TAU);
  g_col[0] = (M.color >> 16) & 0xFF; g_col[1] = (M.color >> 8) & 0xFF; g_col[2] = M.color & 0xFF;
  g_tilt = M.tilt; g_eyeH = M.eyeH; g_eyeW = M.eyeW; g_eyeGap = M.eyeGap; g_eyeRot = M.eyeRot; g_ring = 0;
  g_lastMotionMs = millis(); g_lastStimulusMs = millis();
  g_nextBlink = millis() + 1800;
  g_nextBeh = millis() + 2500;
}

static Emotion pickEmotion(const ImuSample& imu, float mic, uint32_t now) {
  if (imu.jerk > 0.08f) g_lastMotionMs = now;
  if (now < g_forceUntil) return g_forced;
  if (now < g_holdUntil) return g_emo;
  Emotion next = Emotion::Idle;
  if (mic > 0.55f)          { next = Emotion::Talking;   g_holdUntil = now + 400; }
  else if (mic > 0.22f)     { next = Emotion::Listening; g_holdUntil = now + 250; }
  else if (imu.jerk > 1.4f) { next = Emotion::Angry;     g_holdUntil = now + 700; }
  else if (imu.jerk > 0.55f){ next = Emotion::Surprised; g_holdUntil = now + 500; }
  else if (now - g_lastMotionMs > 20000 && now - g_lastStimulusMs > 20000) next = Emotion::Sleepy;
  if (next != Emotion::Idle && next != Emotion::Sleepy) g_lastStimulusMs = now;
  return next;
}

// Elige que hacer. Con mas vitalidad hace mas y mas seguido.
static void chooseBehavior(uint32_t now) {
  struct { Beh b; float w; uint32_t ms; } table[] = {
    { Beh::Stretch,  1.0f, 1500 }, { Beh::Hop,      1.2f, 1300 }, { Beh::Wander,   1.4f, 2200 },
    { Beh::Roll,     0.7f, 1800 }, { Beh::Wink,     1.0f,  450 }, { Beh::Curious,  1.0f, 2500 },
    { Beh::Giggle,   0.8f, 1600 }, { Beh::Daydream, 1.0f, 3500 }, { Beh::Nap,      0.5f, 5000 },
    { Beh::Peekaboo, 0.6f, 1600 }, { Beh::Shiver,   0.5f,  800 }, { Beh::Dance,    0.5f, 3600 },
  };
  const int n = sizeof(table) / sizeof(table[0]);
  float total = 0;
  for (int i = 0; i < n; i++) {
    float w = table[i].w;
    if (g_vitality < .6f && (table[i].b == Beh::Dance || table[i].b == Beh::Hop || table[i].b == Beh::Giggle)) w *= .3f;
    if (g_vitality < .6f && table[i].b == Beh::Nap) w *= 2.f;
    total += w;
  }
  float r = frand() * total;
  int pick = 0;
  for (int i = 0; i < n; i++) {
    float w = table[i].w;
    if (g_vitality < .6f && (table[i].b == Beh::Dance || table[i].b == Beh::Hop || table[i].b == Beh::Giggle)) w *= .3f;
    if (g_vitality < .6f && table[i].b == Beh::Nap) w *= 2.f;
    if (r < w) { pick = i; break; }
    r -= w;
  }
  g_beh = table[pick].b;
  g_behStart = now; g_behUntil = now + table[pick].ms;
  g_behA = frand() * 2 - 1; g_behB = frand();
  if (g_beh == Beh::Wander) g_behA = clampf(g_posX / 70.f + (frand() * 2 - 1) * 1.2f, -1.f, 1.f);
  Serial.printf("[bob] hace: %s\n", BEH_NAMES[(int)g_beh]);
}

void faceUpdate(const ImuSample& imu, float mic, uint32_t now) {
  static uint32_t last = 0;
  float dt = last ? (now - last) / 1000.f : 0.016f;
  if (dt > 0.1f) dt = 0.1f;
  last = now;

  Emotion e = pickEmotion(imu, mic, now);
  if (e != g_emo) { g_emo = e; Serial.printf("[bob] %s\n", emotionName(e)); if (e != Emotion::Idle) g_beh = Beh::None; }
  g_phase += dt;
  const float k = clampf(dt * 8.f, 0.05f, 0.45f);

  // ---- director: solo actua tranquilo (Idle) y sin gesto de cuidado en curso ----
  bool focused = now < g_focusUntil;
  bool calm = !focused && (g_emo == Emotion::Idle) && (g_action == FaceAction::None || now >= g_actionUntil);
  if (g_beh != Beh::None && now >= g_behUntil) {
    g_beh = Beh::None; g_napping = false;
    float rest = 2500 + frand() * 5000 * (1.6f - g_vitality);   // con mas vida, menos espera
    g_nextBeh = now + (uint32_t)rest;
  }
  if (calm && g_beh == Beh::None && now >= g_nextBeh) chooseBehavior(now);
  float bt = g_beh != Beh::None ? clampf((now - g_behStart) / (float)(g_behUntil - g_behStart), 0.f, 1.f) : 0.f;

  // animo visible: el real, o el que pide el acto en curso
  const Mood* V = &MOODS[(int)g_emo];
  if (focused && g_emo == Emotion::Idle && g_beh == Beh::None) V = &MOODS[(int)Emotion::Listening];
  if (g_beh == Beh::Curious) V = &MOODS[(int)Emotion::Listening];
  if (g_beh == Beh::Giggle)  V = &MOODS[(int)Emotion::Talking];
  if (g_beh == Beh::Nap)     { V = &MOODS[(int)Emotion::Sleepy]; g_napping = bt > .15f && bt < .85f; }
  if (g_beh == Beh::Dance)   V = &MOODS[(int)((uint32_t)(bt * 12) % (int)Emotion::COUNT)];   // pasa por todos
  const Mood& M = *V;

  float kk = (g_beh == Beh::Dance) ? clampf(dt * 16.f, 0.1f, 0.7f) : k;
  for (int i = 0; i < N; i++) g_radii[i] = lerpf(g_radii[i], M.shape(i / (float)N * TAU), kk);
  float tc[3] = { (float)((M.color >> 16) & 0xFF), (float)((M.color >> 8) & 0xFF), (float)(M.color & 0xFF) };
  float grey = (tc[0] * .3f + tc[1] * .59f + tc[2] * .11f);
  for (int i = 0; i < 3; i++) {
    float c = lerpf(grey, tc[i], g_vitality) * (0.45f + 0.55f * g_vitality);  // desatura y apaga
    g_col[i] = lerpf(g_col[i], c, kk);
  }
  g_ring = lerpf(g_ring, M.ring ? 1.f : 0.f, k);

  // ojos + parpadeo
  float eyeH = M.eyeH, eyeL = 1, eyeR = 1;
  if (g_emo != Emotion::Sleepy && g_beh != Beh::Nap) {
    if (now >= g_nextBlink && !g_blinkUntil) { g_blinkUntil = now + 110; g_nextBlink = now + 1600 + esp_random() % 2500; }
    if (g_blinkUntil && now < g_blinkUntil) eyeH = .04f;
    else if (g_blinkUntil && now >= g_blinkUntil) g_blinkUntil = 0;
  }

  // mirada: la inclinacion manda; si esta plano, mira alrededor de vez en cuando
  float tilt = clampf(imu.ax * 1.4f, -1.f, 1.f);
  float lx = tilt, ly = 0;
  if (fabsf(tilt) < .12f) {
    if (now > g_lookUntil) {
      if (frand() < .02f) { g_lookTx = frand() * 2 - 1; g_lookTy = frand() * 1.2f - .6f; g_lookUntil = now + 700 + esp_random() % 900; }
      else { g_lookTx = 0; g_lookTy = 0; }
    }
    lx = g_lookTx; ly = g_lookTy;
  }
  if (g_emo == Emotion::Sleepy) { lx = 0; ly = .3f; }

  // cuerpo: respirar, rebotar, vibrar, aplastarse con la voz
  float sx = 1 + sinf(g_phase * 2.2f) * .012f, sy = 1 - sinf(g_phase * 2.2f) * .012f, bounce = sinf(g_phase * 2.2f) * 3;
  float tiltT = M.tilt, posX = g_posX, animScale = 1;
  switch (g_emo) {
    case Emotion::Talking:   { float v = .06f + mic * .10f; sx = 1 + sinf(g_phase * 22) * v; sy = 1 - sinf(g_phase * 22) * v; bounce = sinf(g_phase * 11) * 5; } break;
    case Emotion::Listening: sy = 1.04f; sx = .97f; bounce = sinf(g_phase * 3) * 2; break;
    case Emotion::Surprised: sx = 1.1f; sy = 1.12f; bounce = -8; break;
    case Emotion::Angry:     sx = 1.03f; sy = .94f; bounce = sinf(g_phase * 30) * 2; break;
    case Emotion::Sleepy:    sx = 1.05f; sy = .93f; bounce = sinf(g_phase * 1.2f) * 4 + 6; break;
    default: break;
  }
  if (g_emo != Emotion::Idle) posX = 0;   // los sustos lo devuelven al centro

  // ---- actos autonomos ----
  const float bell = sinf(bt * 3.14159f);          // 0 -> 1 -> 0 a lo largo del acto
  switch (g_beh) {
    case Beh::Stretch:  sy = 1 + .18f * bell; sx = 1 - .10f * bell; eyeH = bell > .5f ? .05f : eyeH; bounce = -10 * bell; break;
    case Beh::Hop:      { float h = fabsf(sinf(bt * 3.14159f * 3)); bounce = -32 * h; sy = 1 + .10f * h - .12f * (h < .08f); sx = 1 - .06f * h + .10f * (h < .08f); } break;
    case Beh::Wander:   posX = lerpf(g_posX, g_behA * 70, .04f); tiltT += (g_behA * 70 - g_posX) * .25f; bounce = sinf(g_phase * 14) * 4; break;
    case Beh::Roll:     tiltT += bt * 360 * (g_behA < 0 ? -1 : 1); posX = g_behA * 40 * bell; bounce = 4 * bell; break;
    case Beh::Wink:     if (g_behA < 0) eyeL = .12f; else eyeR = .12f; sx = 1.03f; break;
    case Beh::Curious:  lx = g_behA; ly = -.4f + g_behB * .3f; tiltT += g_behA * 8; sy = 1.05f; break;
    case Beh::Giggle:   sx = 1 + sinf(g_phase * 26) * .06f; sy = 1 - sinf(g_phase * 26) * .05f; bounce = sinf(g_phase * 13) * 5; eyeH *= .5f; break;
    case Beh::Daydream: lx = .6f; ly = -.7f; sy = 1 + sinf(g_phase * 1.1f) * .02f; tiltT += 6; eyeH *= .8f; break;
    case Beh::Nap:      eyeH = (g_napping ? .05f : .2f); sx = 1.05f; sy = .93f; bounce = sinf(g_phase * 1.2f) * 4 + 6; break;
    case Beh::Peekaboo: animScale = bt < .5f ? 1 - .75f * clampf(bt / .35f, 0, 1) : .25f + .75f * clampf((bt - .5f) / .3f, 0, 1); eyeH = bt < .5f ? .05f : .42f; bounce = bt > .5f ? -8 : 0; break;
    case Beh::Shiver:   sx = 1 + sinf(g_phase * 50) * .04f; posX = g_posX + sinf(g_phase * 50) * 3; eyeH *= .7f; break;
    case Beh::Dance:    bounce = -14 * fabsf(sinf(g_phase * 8)); tiltT += sinf(g_phase * 4) * 18; sx = 1 + sinf(g_phase * 8) * .08f; sy = 1 - sinf(g_phase * 8) * .08f; posX = sinf(g_phase * 2) * 40; break;
    default: break;
  }

  // gestos de cuidado (mandan sobre lo autonomo)
  if (g_action != FaceAction::None && now < g_actionUntil) {
    switch (g_action) {
      case FaceAction::Caress: eyeH *= .45f; bounce = -6 + sinf(g_phase * 12) * 3; sx = 1.06f; sy = .96f; break;
      case FaceAction::Eat:    { float ch = sinf(g_phase * 16); sy = 1 + ch * .07f; sx = 1 - ch * .05f; bounce = ch * 3; } break;
      case FaceAction::Tickle: sx = 1 + sinf(g_phase * 40) * .05f; sy = 1 - sinf(g_phase * 40) * .04f; bounce = sinf(g_phase * 20) * 6; eyeH *= .6f; break;
      default: break;
    }
  } else g_action = FaceAction::None;

  g_eyeH = lerpf(g_eyeH, eyeH, clampf(dt * 14.f, 0.05f, 0.6f));
  g_eyeL = lerpf(g_eyeL, eyeL, .5f); g_eyeR = lerpf(g_eyeR, eyeR, .5f);
  g_eyeW = lerpf(g_eyeW, M.eyeW, k); g_eyeGap = lerpf(g_eyeGap, M.eyeGap, k); g_eyeRot = lerpf(g_eyeRot, M.eyeRot, k);
  g_lookX = lerpf(g_lookX, lx, k); g_lookY = lerpf(g_lookY, ly, k);
  g_tilt = (g_beh == Beh::Roll) ? tiltT : lerpf(g_tilt, tiltT, k);
  g_posX = lerpf(g_posX, posX, (g_beh == Beh::Wander || g_beh == Beh::Shiver) ? 1.f : k);
  g_animScale = lerpf(g_animScale, animScale, .5f);
  g_sx = lerpf(g_sx, sx, k); g_sy = lerpf(g_sy, sy, k); g_bounce = lerpf(g_bounce, bounce, k);
}

// ---- dibujo ----
// Poligono relleno por scanlines (par/impar), sirve para formas concavas (nube, flor).
static void fillPolygon(const float* px, const float* py, int n, uint16_t color) {
  float minY = py[0], maxY = py[0];
  for (int i = 1; i < n; i++) { if (py[i] < minY) minY = py[i]; if (py[i] > maxY) maxY = py[i]; }
  int y0 = (int)ceilf(minY), y1 = (int)floorf(maxY);
  if (y0 < 0) y0 = 0;
  if (y1 > LCD_HEIGHT - 1) y1 = LCD_HEIGHT - 1;
  float xs[16];
  for (int y = y0; y <= y1; y++) {
    int cnt = 0;
    for (int i = 0; i < n; i++) {
      int j = (i + 1) % n;
      float ya = py[i], yb = py[j];
      if ((ya <= y && y < yb) || (yb <= y && y < ya)) {
        float x = px[i] + (y - ya) * (px[j] - px[i]) / (yb - ya);
        if (cnt < 16) xs[cnt++] = x;
      }
    }
    for (int a = 1; a < cnt; a++) { float v = xs[a]; int b = a - 1; while (b >= 0 && xs[b] > v) { xs[b + 1] = xs[b]; b--; } xs[b + 1] = v; }
    for (int a = 0; a + 1 < cnt; a += 2) {
      int xa = (int)ceilf(xs[a]), xb = (int)floorf(xs[a + 1]);
      if (xa < 0) xa = 0;
      if (xb > LCD_WIDTH - 1) xb = LCD_WIDTH - 1;
      if (xb >= xa) g->drawFastHLine(xa, y, xb - xa + 1, color);
    }
  }
}

static void bodyPolygon(float scale, float cx, float cy, float* px, float* py) {
  float a = g_tilt * DEG, ca = cosf(a), sa = sinf(a);
  for (int i = 0; i < N; i++) {
    float t = i / (float)N * TAU, r = g_radii[i] * R * scale * g_scale * g_animScale;
    float x = cosf(t) * r * g_sx, y = -sinf(t) * r * g_sy;
    px[i] = cx + x * ca - y * sa;
    py[i] = cy + x * sa + y * ca;
  }
}

// Pildora = trazo grueso con puntas redondas: circulos a lo largo del segmento.
static void capsule(float x0, float y0, float x1, float y1, float w, uint16_t c) {
  float r = w / 2, dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
  int steps = (int)(len / (r * .5f)) + 1;
  for (int i = 0; i <= steps; i++) {
    float t = i / (float)steps;
    g->fillCircle((int)(x0 + dx * t + .5f), (int)(y0 + dy * t + .5f), (int)(r + .5f), c);
  }
}

void faceDraw() {
  if (!g) return;
  static float px[N], py[N];
  const float cx = LCD_WIDTH / 2.f + g_posX, cy = LCD_HEIGHT / 2.f + 8 + g_bounce;

  g->fillScreen(rgb565(COL_BG));
  if (g_ring > .02f) {
    // el anillo del negro aparece mezclando su gris con el fondo
    uint32_t rc = ((uint32_t)(((RING_COLOR >> 16) & 0xFF) * g_ring) << 16) |
                  ((uint32_t)(((RING_COLOR >> 8) & 0xFF) * g_ring) << 8) |
                   (uint32_t)((RING_COLOR & 0xFF) * g_ring);
    bodyPolygon(1.04f, cx, cy, px, py);
    fillPolygon(px, py, N, rgb565(rc));
  }
  uint32_t col = ((uint32_t)(g_col[0] + .5f) << 16) | ((uint32_t)(g_col[1] + .5f) << 8) | (uint32_t)(g_col[2] + .5f);
  bodyPolygon(1.f, cx, cy, px, py);
  fillPolygon(px, py, N, rgb565(col));

  // ojos: giran con el cuerpo (rodar) y cada uno puede cerrarse solo (guino)
  const float Rs = R * g_scale * g_animScale;
  float eh = g_eyeH * Rs, ew = g_eyeW * Rs, gap = g_eyeGap * Rs;
  float ox = g_lookX * Rs * .16f, oy = g_lookY * Rs * .12f - Rs * .06f;
  float ba = g_tilt * DEG, ca = cosf(ba), sa = sinf(ba);
  uint16_t white = rgb565(COL_WHITE);
  for (int s = -1; s <= 1; s += 2) {
    float lx = s * gap + ox, ly = oy;
    float ex = cx + lx * ca - ly * sa, ey = cy + lx * sa + ly * ca;
    float rot = ba + s * g_eyeRot * DEG;
    float h = fmaxf(eh * (s < 0 ? g_eyeL : g_eyeR), ew * .35f) - ew;
    float dx = sinf(rot) * h / 2, dy = cosf(rot) * h / 2;
    capsule(ex - dx, ey - dy, ex + dx, ey + dy, ew, white);
  }

  // globo de texto (hasta 2 lineas de 26 caracteres, fuente x2)
  if (g_say[0] && millis() < g_sayUntil) {
    const int cw = 12, maxc = 26, pad = 14;
    char l1[maxc + 1] = "", l2[maxc + 1] = "";
    int len = strlen(g_say);
    if (len <= maxc) strncpy(l1, g_say, maxc);
    else {
      int cut = maxc;
      for (int i = maxc; i > 8; i--) if (g_say[i] == ' ') { cut = i; break; }
      strncpy(l1, g_say, cut); l1[cut] = 0;
      const char* rest = g_say + cut; while (*rest == ' ') rest++;
      strncpy(l2, rest, maxc);
    }
    int lines = l2[0] ? 2 : 1;
    int w = (int)fmaxf(strlen(l1), strlen(l2)) * cw + pad * 2, h = lines * 20 + pad * 2 - 4;
    int bx = (LCD_WIDTH - w) / 2, by = 22;
    g->fillRoundRect(bx, by, w, h, 14, rgb565(0xFFFFFFu));
    g->fillTriangle(LCD_WIDTH / 2 - 10, by + h - 1, LCD_WIDTH / 2 + 10, by + h - 1, LCD_WIDTH / 2, by + h + 12, rgb565(0xFFFFFFu));
    g->setTextSize(2);
    g->setTextColor(rgb565(0x1B1F26u));
    g->setCursor(bx + pad, by + pad - 2); g->print(l1);
    if (l2[0]) { g->setCursor(bx + pad, by + pad + 18); g->print(l2); }
  } else if (g_say[0] && millis() >= g_sayUntil) g_say[0] = 0;
}
