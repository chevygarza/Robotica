#include "face.h"
#include <math.h>

// Misma geometria que la maqueta bob-aerogro.html: N muestras del contorno,
// radio base R, superelipse para la pildora, cos(k*theta) para picos/bultos.
static const int   N = 72;
static const float R = 118.f;
static const float DEG = 3.14159265f / 180.f;

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

// ---- estado animado ----
static Arduino_GFX* g = nullptr;
static Emotion g_emo = Emotion::Idle;
static float g_radii[N];
static float g_col[3];
static float g_tilt, g_eyeH, g_eyeW, g_eyeGap, g_eyeRot, g_ring, g_bounce, g_sx = 1, g_sy = 1, g_lookX, g_lookY;
static float g_phase = 0.f, g_micLast = 0.f;
static uint32_t g_holdUntil = 0, g_lastMotionMs = 0, g_nextBlink = 0, g_blinkUntil = 0, g_lookUntil = 0;
static float g_lookTx = 0, g_lookTy = 0;

static float lerpf(float a, float b, float k) { return a + (b - a) * k; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float frand() { return (esp_random() % 10000) / 10000.f; }

Emotion faceEmotion() { return g_emo; }

void faceBegin(Arduino_GFX* gfx) {
  g = gfx;
  const Mood& M = MOODS[0];
  for (int i = 0; i < N; i++) g_radii[i] = M.shape(i / (float)N * 6.2831853f);
  g_col[0] = (M.color >> 16) & 0xFF; g_col[1] = (M.color >> 8) & 0xFF; g_col[2] = M.color & 0xFF;
  g_tilt = M.tilt; g_eyeH = M.eyeH; g_eyeW = M.eyeW; g_eyeGap = M.eyeGap; g_eyeRot = M.eyeRot; g_ring = 0;
  g_lastMotionMs = millis();
  g_nextBlink = millis() + 1800;
}

static Emotion pickEmotion(const ImuSample& imu, float mic, uint32_t now) {
  if (imu.jerk > 0.08f) g_lastMotionMs = now;
  if (now < g_holdUntil) return g_emo;
  Emotion next = Emotion::Idle;
  if (mic > 0.55f)       { next = Emotion::Talking;   g_holdUntil = now + 400; }
  else if (mic > 0.22f)  { next = Emotion::Listening; g_holdUntil = now + 250; }
  else if (imu.jerk > 1.4f) { next = Emotion::Angry;  g_holdUntil = now + 700; }
  else if (imu.jerk > 0.55f){ next = Emotion::Surprised; g_holdUntil = now + 500; }
  else if (now - g_lastMotionMs > 20000) next = Emotion::Sleepy;
  return next;
}

void faceUpdate(const ImuSample& imu, float mic, uint32_t now) {
  static uint32_t last = 0;
  float dt = last ? (now - last) / 1000.f : 0.016f;
  if (dt > 0.1f) dt = 0.1f;
  last = now;
  g_micLast = mic;

  Emotion e = pickEmotion(imu, mic, now);
  if (e != g_emo) { g_emo = e; Serial.printf("[bob] %s\n", emotionName(e)); }
  const Mood& M = MOODS[(int)g_emo];
  g_phase += dt;
  const float k = clampf(dt * 8.f, 0.05f, 0.45f);

  for (int i = 0; i < N; i++) g_radii[i] = lerpf(g_radii[i], M.shape(i / (float)N * 6.2831853f), k);
  float tc[3] = { (float)((M.color >> 16) & 0xFF), (float)((M.color >> 8) & 0xFF), (float)(M.color & 0xFF) };
  for (int i = 0; i < 3; i++) g_col[i] = lerpf(g_col[i], tc[i], k);
  g_tilt = lerpf(g_tilt, M.tilt, k);
  g_ring = lerpf(g_ring, M.ring ? 1.f : 0.f, k);

  // ojos + parpadeo
  float eyeH = M.eyeH;
  if (g_emo != Emotion::Sleepy) {
    if (now >= g_nextBlink && !g_blinkUntil) { g_blinkUntil = now + 110; g_nextBlink = now + 1600 + esp_random() % 2500; }
    if (g_blinkUntil && now < g_blinkUntil) eyeH = .04f;
    else if (g_blinkUntil && now >= g_blinkUntil) g_blinkUntil = 0;
  }
  g_eyeH = lerpf(g_eyeH, eyeH, clampf(dt * 14.f, 0.05f, 0.6f));
  g_eyeW = lerpf(g_eyeW, M.eyeW, k); g_eyeGap = lerpf(g_eyeGap, M.eyeGap, k); g_eyeRot = lerpf(g_eyeRot, M.eyeRot, k);

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
  g_lookX = lerpf(g_lookX, lx, k); g_lookY = lerpf(g_lookY, ly, k);

  // cuerpo: respirar, rebotar, vibrar, aplastarse con la voz
  float sx = 1 + sinf(g_phase * 2.2f) * .012f, sy = 1 - sinf(g_phase * 2.2f) * .012f, bounce = sinf(g_phase * 2.2f) * 3;
  switch (g_emo) {
    case Emotion::Talking:   { float v = .06f + mic * .10f; sx = 1 + sinf(g_phase * 22) * v; sy = 1 - sinf(g_phase * 22) * v; bounce = sinf(g_phase * 11) * 5; } break;
    case Emotion::Listening: sy = 1.04f; sx = .97f; bounce = sinf(g_phase * 3) * 2; break;
    case Emotion::Surprised: sx = 1.1f; sy = 1.12f; bounce = -8; break;
    case Emotion::Angry:     sx = 1.03f; sy = .94f; bounce = sinf(g_phase * 30) * 2; break;
    case Emotion::Sleepy:    sx = 1.05f; sy = .93f; bounce = sinf(g_phase * 1.2f) * 4 + 6; break;
    default: break;
  }
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
    float t = i / (float)N * 6.2831853f, r = g_radii[i] * R * scale;
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
  const float cx = LCD_WIDTH / 2.f, cy = LCD_HEIGHT / 2.f + 8 + g_bounce;

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

  // ojos
  float eh = g_eyeH * R, ew = g_eyeW * R, gap = g_eyeGap * R;
  float ox = g_lookX * R * .16f, oy = g_lookY * R * .12f - R * .06f;
  uint16_t white = rgb565(COL_WHITE);
  for (int s = -1; s <= 1; s += 2) {
    float ex = cx + s * gap + ox, ey = cy + oy;
    float rot = s * g_eyeRot * DEG;
    float h = fmaxf(eh, ew * .35f) - ew;
    float dx = sinf(rot) * h / 2, dy = cosf(rot) * h / 2;
    capsule(ex - dx, ey - dy, ex + dx, ey + dy, ew, white);
  }
}
