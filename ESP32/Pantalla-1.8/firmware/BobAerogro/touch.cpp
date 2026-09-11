#include "touch.h"
#include <Wire.h>
#include "board.h"

static const uint8_t ADDR = 0x15;
static bool g_ok = false;

static bool rd(uint8_t reg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(ADDR); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ADDR, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}
static void wr(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(ADDR); Wire.write(reg); Wire.write(v); Wire.endTransmission();
}

bool touchBegin() {
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  uint8_t id = 0;
  g_ok = rd(0xA7, &id, 1);
  if (g_ok) {
    wr(0xFE, 0x01);   // sin auto-sleep: se puede leer en cualquier momento
    wr(0xFA, 0x60);   // IRQ periodica mientras hay dedo + en cambios
    Serial.printf("[touch] CST816 ok id=0x%02X\n", id);
  } else {
    Serial.println("[touch] CST816 no responde — sin tactil");
  }
  return g_ok;
}

// Maquina de gestos: tap corto, doble tap (decidido tras 300 ms), swipe, hold.
TouchInfo touchPoll(uint32_t now) {
  TouchInfo out;
  if (!g_ok) return out;
  static bool wasDown = false, holdSent = false, pendingTap = false, swiped = false;
  static uint32_t downMs = 0, upMs = 0, lastRead = 0;
  static int16_t x0 = 0, y0 = 0, xl = 0, yl = 0, tapX = 0, tapY = 0;

  bool down = wasDown;
  int16_t x = xl, y = yl;
  if (now - lastRead >= 15) {               // ~66 Hz basta y no satura el I2C
    lastRead = now;
    uint8_t b[5];
    if (rd(0x02, b, 5)) {
      down = (b[0] & 0x0F) > 0;
      if (down) { x = ((b[1] & 0x0F) << 8) | b[2]; y = ((b[3] & 0x0F) << 8) | b[4]; }
    }
  }
  out.down = down; out.x = x; out.y = y;

  if (down && !wasDown) {                   // dedo baja
    downMs = now; x0 = x; y0 = y; holdSent = false; swiped = false;
  } else if (down && wasDown) {
    int16_t dx = x - x0, dy = y - y0;
    if (!swiped && !holdSent && (abs(dx) > 40 || abs(dy) > 40)) {
      swiped = true; out.ev = TouchEvent::Swipe; out.dx = dx; out.dy = dy; pendingTap = false;
    } else if (!swiped && !holdSent && now - downMs > 600) {
      holdSent = true; out.ev = TouchEvent::Hold; pendingTap = false;
    }
  } else if (!down && wasDown) {            // dedo sube
    if (holdSent) out.ev = TouchEvent::HoldEnd;
    else if (!swiped && now - downMs < 300) {
      if (pendingTap && now - upMs < 350) { pendingTap = false; out.ev = TouchEvent::DoubleTap; out.x = tapX; out.y = tapY; }
      else { pendingTap = true; upMs = now; tapX = x0; tapY = y0; }
    }
  } else if (pendingTap && now - upMs >= 350) {   // no llego segundo tap: era simple
    pendingTap = false; out.ev = TouchEvent::Tap; out.x = tapX; out.y = tapY;
  }
  wasDown = down; xl = x; yl = y;
  return out;
}
