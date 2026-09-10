#include "face.h"
#include <math.h>

static Arduino_GFX* g = nullptr;
static FaceState g_cur;
static FaceState g_tgt;
static Emotion g_emo = Emotion::Idle;
static uint32_t g_lastMotionMs = 0;
static uint32_t g_blinkUntil = 0;
static uint32_t g_nextBlink = 0;
static uint32_t g_lookAroundUntil = 0;
static float g_lookTx = 0, g_lookTy = 0;
static uint32_t g_emoHoldUntil = 0;
static float g_phase = 0.f;

const char* emotionName(Emotion e) {
  switch (e) {
    case Emotion::Idle: return "IDLE";
    case Emotion::Happy: return "HAPPY";
    case Emotion::Surprised: return "SURPRISED";
    case Emotion::Look: return "LOOK";
    case Emotion::Sleepy: return "SLEEPY";
    case Emotion::Angry: return "ANGRY";
    case Emotion::Excited: return "EXCITED";
    case Emotion::Listening: return "LISTENING";
    default: return "?";
  }
}

Emotion faceEmotion() { return g_emo; }

static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static void wideLine(int x0, int y0, int x1, int y1, int th, uint16_t c) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2;
  int r = th / 2; if (r < 1) r = 1;
  for (;;) {
    g->fillCircle(x0, y0, r, c);
    if (x0 == x1 && y0 == y1) break;
    e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void setTargetsFor(Emotion e) {
  g_tgt = FaceState{};
  g_tgt.emotion = e;
  g_tgt.eyeOpen = 1.f;
  g_tgt.eyeSize = 1.f;
  g_tgt.smile = 0.35f;
  g_tgt.glow = 0.75f;
  g_tgt.brow = 0.1f;
  switch (e) {
    case Emotion::Happy:
      g_tgt.smile = 0.95f; g_tgt.squint = 0.45f; g_tgt.eyeOpen = 0.75f;
      g_tgt.glow = 0.95f; g_tgt.brow = 0.35f; break;
    case Emotion::Surprised:
      g_tgt.eyeSize = 1.35f; g_tgt.eyeOpen = 1.f; g_tgt.mouthOpen = 0.85f;
      g_tgt.smile = 0.1f; g_tgt.brow = 0.9f; g_tgt.glow = 1.f; break;
    case Emotion::Look:
      g_tgt.smile = 0.25f; g_tgt.glow = 0.85f; break;
    case Emotion::Sleepy:
      g_tgt.eyeOpen = 0.22f; g_tgt.smile = 0.05f; g_tgt.glow = 0.25f;
      g_tgt.brow = -0.2f; g_tgt.bounce = 2.f; break;
    case Emotion::Angry:
      g_tgt.smile = -0.7f; g_tgt.brow = -0.85f; g_tgt.squint = 0.35f;
      g_tgt.glow = 0.9f; g_tgt.eyeSize = 0.95f; break;
    case Emotion::Excited:
      g_tgt.smile = 0.8f; g_tgt.eyeSize = 1.2f; g_tgt.mouthOpen = 0.4f;
      g_tgt.glow = 1.f; g_tgt.brow = 0.55f; g_tgt.bounce = -4.f; break;
    case Emotion::Listening:
      g_tgt.eyeSize = 1.15f; g_tgt.mouthOpen = 0.15f; g_tgt.smile = 0.2f;
      g_tgt.brow = 0.5f; g_tgt.glow = 1.f; break;
    default: break;
  }
}

void faceBegin(Arduino_GFX* gfx) {
  g = gfx;
  g_nextBlink = millis() + 1800;
  g_lastMotionMs = millis();
  setTargetsFor(Emotion::Idle);
  g_cur = g_tgt;
}

static void pickEmotion(const ImuSample& imu, float micE, uint32_t now) {
  bool moving = imu.jerk > 0.08f || fabsf(imu.mag - 1.f) > 0.18f;
  if (moving) g_lastMotionMs = now;
  if (now < g_emoHoldUntil) return;

  Emotion next = Emotion::Idle;
  if (micE > 0.55f) { next = Emotion::Excited; g_emoHoldUntil = now + 400; }
  else if (micE > 0.22f) { next = Emotion::Listening; g_emoHoldUntil = now + 250; }
  else if (imu.jerk > 1.4f || imu.mag > 2.4f) { next = Emotion::Angry; g_emoHoldUntil = now + 600; }
  else if (imu.jerk > 0.55f) { next = Emotion::Surprised; g_emoHoldUntil = now + 450; }
  else if (imu.jerk > 0.22f || fabsf(imu.ax) > 0.35f || fabsf(imu.ay) > 0.35f) {
    next = (fabsf(imu.ax) > 0.25f || fabsf(imu.ay) > 0.25f) ? Emotion::Look : Emotion::Happy;
    g_emoHoldUntil = now + 200;
  } else if (now - g_lastMotionMs > 20000) {
    next = Emotion::Sleepy;
  }

  if (next != g_emo) {
    g_emo = next;
    setTargetsFor(g_emo);
    Serial.printf("[face] %s\n", emotionName(g_emo));
  }
}

static void animateIdleBits(uint32_t now, float dt) {
  g_phase += dt;
  float breath = sinf(g_phase * 2.2f) * 0.012f;
  g_tgt.breathe = breath;
  if (g_emo == Emotion::Idle || g_emo == Emotion::Look || g_emo == Emotion::Listening)
    g_tgt.bounce = sinf(g_phase * 2.2f) * 2.5f;
  else if (g_emo == Emotion::Excited) g_tgt.bounce = sinf(g_phase * 10.f) * 5.f;
  else if (g_emo == Emotion::Angry) g_tgt.bounce = sinf(g_phase * 18.f) * 2.f;

  if (g_emo != Emotion::Sleepy) {
    if (now >= g_nextBlink && g_blinkUntil == 0) {
      g_blinkUntil = now + 120;
      g_nextBlink = now + 1600 + (uint32_t)(esp_random() % 2500);
    }
    if (g_blinkUntil && now < g_blinkUntil) g_tgt.eyeOpen = 0.08f;
    else if (g_blinkUntil && now >= g_blinkUntil) {
      g_blinkUntil = 0;
      setTargetsFor(g_emo);
    }
  }
}

void faceUpdate(const ImuSample& imu, float micEnergy, uint32_t nowMs) {
  static uint32_t last = 0;
  float dt = last ? (nowMs - last) / 1000.f : 0.016f;
  if (dt > 0.1f) dt = 0.1f;
  last = nowMs;

  pickEmotion(imu, micEnergy, nowMs);

  float trackX = clampf(imu.ax * 1.4f, -1.f, 1.f);
  float trackY = clampf(-imu.ay * 1.4f, -1.f, 1.f);

  if (g_emo == Emotion::Look || g_emo == Emotion::Idle || g_emo == Emotion::Listening) {
    if (nowMs > g_lookAroundUntil && g_emo == Emotion::Idle) {
      if ((esp_random() % 100) < 3) {
        g_lookTx = ((int)(esp_random() % 200) - 100) / 100.f;
        g_lookTy = ((int)(esp_random() % 120) - 60) / 100.f;
        g_lookAroundUntil = nowMs + 700 + (esp_random() % 900);
      } else { g_lookTx = 0; g_lookTy = 0; }
    }
    float lx = (fabsf(trackX) > 0.12f || fabsf(trackY) > 0.12f) ? trackX : g_lookTx;
    float ly = (fabsf(trackX) > 0.12f || fabsf(trackY) > 0.12f) ? trackY : g_lookTy;
    g_tgt.pupilX = lx; g_tgt.pupilY = ly;
  } else if (g_emo == Emotion::Sleepy) {
    g_tgt.pupilX = 0; g_tgt.pupilY = 0.15f;
  } else {
    g_tgt.pupilX = trackX * 0.5f; g_tgt.pupilY = trackY * 0.5f;
  }

  if (micEnergy > 0.1f) {
    g_tgt.mouthOpen = clampf(micEnergy * 0.9f, g_tgt.mouthOpen, 1.f);
    g_tgt.glow = clampf(0.7f + micEnergy * 0.4f, 0.f, 1.f);
  }

  animateIdleBits(nowMs, dt);

  const float k = clampf(dt * 10.f, 0.05f, 0.45f);
  auto L = [&](float &c, float t) { c = lerpf(c, t, k); };
  L(g_cur.eyeOpen, g_tgt.eyeOpen); L(g_cur.eyeSize, g_tgt.eyeSize);
  L(g_cur.pupilX, g_tgt.pupilX); L(g_cur.pupilY, g_tgt.pupilY);
  L(g_cur.brow, g_tgt.brow); L(g_cur.smile, g_tgt.smile);
  L(g_cur.mouthOpen, g_tgt.mouthOpen); L(g_cur.bounce, g_tgt.bounce);
  L(g_cur.breathe, g_tgt.breathe); L(g_cur.glow, g_tgt.glow);
  L(g_cur.squint, g_tgt.squint);
}

// Geometria pensada para 240x284; SC la escala al panel real (368x448).
static const float SC = 1.5f;
static inline int px(float v) { return (int)(v * SC + (v >= 0 ? 0.5f : -0.5f)); }

static void drawEye(int cx, int cy, float open, float size, float px_, float py_, float squint, float glow) {
  int rx = px(28 * size);
  int ry = px(30 * size * open * (1.f - squint * 0.55f));
  if (ry < 3) ry = 3;
  uint16_t glowC = rgb565(COL_GLOW), eyeC = rgb565(COL_EYE), dimC = rgb565(COL_EYE_DIM), wh = rgb565(COL_WHITE);
  int glowR = rx + px(10 * glow);
  g->fillCircle(cx, cy, glowR, dimC);
  g->fillCircle(cx, cy, rx + px(4), eyeC);
  g->fillEllipse(cx, cy, rx, ry, rgb565(0x061018));
  g->fillEllipse(cx, cy, rx - px(2), ry - px(2), eyeC);
  int pr = px(10 * size);
  int ox = (int)(px_ * (rx - pr - px(4)));
  int oy = (int)(py_ * (ry - pr - px(3)));
  g->fillCircle(cx + ox, cy + oy, pr + px(2), rgb565(0x003344));
  g->fillCircle(cx + ox, cy + oy, pr, rgb565(0x021018));
  g->fillCircle(cx + ox - pr / 3, cy + oy - pr / 3, max(2, pr / 3), wh);
  g->fillCircle(cx + ox + pr / 4, cy + oy + pr / 5, max(1, pr / 5), glowC);
  if (open < 0.98f) {
    int lid = (int)((1.f - open) * ry * 2);
    g->fillRect(cx - rx - px(2), cy - ry - px(2), rx * 2 + px(4), lid + px(2), rgb565(COL_FACE));
  }
  if (squint > 0.05f) {
    int sq = (int)(squint * ry);
    g->fillRect(cx - rx - px(2), cy + ry - sq, rx * 2 + px(4), sq + px(3), rgb565(COL_FACE));
  }
}

static void drawBrow(int cx, int cy, float brow, bool left) {
  int baseY = cy - px(42);
  int innerY = baseY - px(brow * 10);
  int outerY = baseY + px(brow * 4);
  int x0 = left ? cx - px(18) : cx - px(16);
  int x1 = left ? cx + px(16) : cx + px(18);
  if (left) wideLine(x0, outerY, x1, innerY, px(4), rgb565(COL_MOUTH));
  else wideLine(x0, innerY, x1, outerY, px(4), rgb565(COL_MOUTH));
}

static void drawMouth(int cx, int cy, float smile, float open) {
  int w = px(54);
  int curve = px(smile * 18);
  int y = cy + px(48);
  uint16_t c = rgb565(COL_MOUTH);
  if (open > 0.12f) {
    int oh = px(8 + open * 22);
    int ow = px(18 + open * 16);
    g->fillEllipse(cx, y + px(2), ow, oh, rgb565(0x1A1020));
    g->drawEllipse(cx, y + px(2), ow, oh, c);
    if (open > 0.55f) g->fillCircle(cx, y + oh / 2, px(4), rgb565(COL_AMBER));
  } else {
    int steps = 12;
    int prevx = cx - w / 2, prevy = y - curve / 3;
    for (int i = 1; i <= steps; i++) {
      float t = i / (float)steps;
      float x = cx - w / 2 + t * w;
      float mid = 1.f - fabsf(2 * t - 1.f);
      float yy = (smile < 0) ? (y - fabsf((float)curve) * mid) : (y + curve * mid);
      wideLine(prevx, prevy, (int)x, (int)yy, px(3), c);
      prevx = (int)x; prevy = (int)yy;
    }
  }
}

void faceDraw() {
  if (!g) return;
  const int W = LCD_WIDTH, H = LCD_HEIGHT;
  int cx = W / 2;
  int cy = H / 2 + px(g_cur.bounce);
  float sc = 1.f + g_cur.breathe;

  g->fillScreen(rgb565(COL_BG));

  int hr = px(98 * sc);
  int hh = px(110 * sc);
  g->fillEllipse(cx, cy + px(4), hr + px(4), hh + px(4), rgb565(COL_FACE_EDGE));
  g->fillEllipse(cx, cy + px(4), hr, hh, rgb565(COL_FACE));

  g->fillCircle(cx - px(62), cy + px(28), px(4), rgb565(COL_AMBER));
  g->fillCircle(cx + px(62), cy + px(28), px(4), rgb565(COL_AMBER));
  g->fillCircle(cx - px(58), cy + px(22), px(10), rgb565(0x143040));
  g->fillCircle(cx + px(58), cy + px(22), px(10), rgb565(0x143040));

  int antX = cx + px(g_cur.pupilX * 4);
  int antY = cy - hh + px(8);
  wideLine(cx, cy - hh + px(18), antX, antY - px(18), px(3), rgb565(COL_FACE_EDGE));
  g->fillCircle(antX, antY - px(22), px(6), rgb565(COL_EYE));
  g->fillCircle(antX, antY - px(22), px(3), rgb565(COL_AMBER));

  int eyeY = cy - px(18);
  int eyeDX = px(40);
  drawBrow(cx - eyeDX, eyeY, g_cur.brow, true);
  drawBrow(cx + eyeDX, eyeY, g_cur.brow, false);
  drawEye(cx - eyeDX, eyeY, g_cur.eyeOpen, g_cur.eyeSize, g_cur.pupilX, g_cur.pupilY, g_cur.squint, g_cur.glow);
  drawEye(cx + eyeDX, eyeY, g_cur.eyeOpen, g_cur.eyeSize, g_cur.pupilX, g_cur.pupilY, g_cur.squint, g_cur.glow);
  drawMouth(cx, cy, g_cur.smile, g_cur.mouthOpen);

  if (g_emo == Emotion::Listening || g_emo == Emotion::Excited) {
    int r = px(70 + sinf(g_phase * 6.f) * 8);
    g->drawCircle(cx, cy + px(4), r, rgb565(COL_EYE_DIM));
  }
}
