#include "knob.h"
#include "pins.h"

#define KNOB_QUEUE_LEN 16

namespace knob {

// Tabla de transicion de cuadratura: indice = (estado_previo << 2) | estado_actual.
// 0 = sin cambio o transicion invalida (rebote), ±1 = un sub-paso.
static const int8_t QUAD_LUT[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

static volatile uint8_t  prevState = 0;
static volatile int8_t   subSteps  = 0;
static volatile int32_t  rawCount  = 0;

static volatile KnobEvent queue[KNOB_QUEUE_LEN];
static volatile uint8_t   qHead = 0, qTail = 0;

static uint32_t lastActivityMs = 0;

static inline void push(KnobEvent e) {
  uint8_t next = (uint8_t)((qHead + 1) % KNOB_QUEUE_LEN);
  if (next == qTail) return;           // cola llena: se descarta el mas nuevo
  queue[qHead] = e;
  qHead = next;
}

static void IRAM_ATTR onEncoderEdge() {
  uint8_t s = (uint8_t)((digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B));
  int8_t  d = QUAD_LUT[(prevState << 2) | s];
  prevState = s;
  if (d == 0) return;
#if KNOB_INVERT
  d = -d;
#endif

  rawCount += d;
  subSteps += d;

  if (subSteps >= KNOB_COUNTS_PER_DETENT) {
    subSteps = 0;
    push(KNOB_CW);
  } else if (subSteps <= -KNOB_COUNTS_PER_DETENT) {
    subSteps = 0;
    push(KNOB_CCW);
  }
}

void begin() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  prevState = (uint8_t)((digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B));
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), onEncoderEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), onEncoderEdge, CHANGE);

  lastActivityMs = millis();
}

void update() {
  static bool     pressed    = false;
  static bool     longFired  = false;
  static uint32_t downMs     = 0;
  static uint32_t lastEdgeMs = 0;
  static bool     awaiting   = false;   // hay un push corto sin resolver
  static uint32_t upMs       = 0;

  bool now = (digitalRead(PIN_ENC_SW) == LOW);
  uint32_t t = millis();

  if (now != pressed && (t - lastEdgeMs) > KNOB_DEBOUNCE_MS) {
    lastEdgeMs = t;
    pressed = now;
    if (pressed) {
      downMs = t;
      longFired = false;
      push(KNOB_DOWN);                  // feedback inmediato, sin compromiso
    } else if (!longFired) {
      if (awaiting && (t - upMs) <= KNOB_DOUBLE_MS) {
        awaiting = false;
        push(KNOB_DOUBLE_PRESS);
      } else {
        awaiting = true;                // esperar por si viene el segundo
        upMs = t;
      }
    } else {
      awaiting = false;                 // el largo ya se atendio
    }
  }

  // Cierra la ventana: si nadie mas presiono, era un push corto.
  if (awaiting && (t - upMs) > KNOB_DOUBLE_MS) {
    awaiting = false;
    push(KNOB_PRESS);
  }

  // Largo: se emite al cumplir el tiempo, sin esperar a que suelte.
  if (pressed && !longFired && (t - downMs) >= KNOB_LONG_PRESS_MS) {
    longFired = true;
    awaiting = false;
    push(KNOB_LONG_PRESS);
  }
}

KnobEvent read() {
  if (qTail == qHead) return KNOB_NONE;
  KnobEvent e = queue[qTail];
  qTail = (uint8_t)((qTail + 1) % KNOB_QUEUE_LEN);
  lastActivityMs = millis();
  return e;
}

uint32_t idleMs()    { return millis() - lastActivityMs; }
void     touch()     { lastActivityMs = millis(); }
int32_t  diagCount() { return rawCount; }

} // namespace knob
