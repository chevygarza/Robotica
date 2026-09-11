// Bob Aerogro — tamagotchi blob en la Waveshare ESP32-S3-Touch-AMOLED-1.8
// Panel CO5300 por QSPI (Arduino_GFX 1.6.4, bundle arduino-v2 de Waveshare).
// Se dibuja en un canvas en PSRAM y se manda el cuadro completo: sin parpadeo.
//
// Cuidados: tap = mimo (carino), doble tap = comer (hambre), deslizar = cosquillas
// (diversion), sacudir = jugar, boca abajo = dormir (sueno). Mantener el dedo
// muestra las barras. Las necesidades bajan con la hora real (RTC) y se guardan en NVS.
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_XCA9554.h>
#include "board.h"
#include "imu.h"
#include "mic.h"
#include "face.h"
#include "touch.h"
#include "rtc.h"
#include "pet.h"
#include "bridge.h"
#include "net.h"

Adafruit_XCA9554 expander;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK,
                                             PIN_LCD_SDIO0, PIN_LCD_SDIO1,
                                             PIN_LCD_SDIO2, PIN_LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(bus, GFX_NOT_DEFINED /* RST via expansor */,
                                           0 /* rotation */, LCD_WIDTH, LCD_HEIGHT,
                                           LCD_COL_OFFSET, 0, 0, 0);
// Rotacion inicial 0 = vertical tipo iPhone (USB a la derecha); despues manda el acelerometro.
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel, 0, 0, 0);

static Emotion lastPrinted = Emotion::Idle;
static uint32_t lastPrintMs = 0, lastLoopMs = 0, lastRtcMs = 0, nowUnix = 0;
static bool hudOn = false;

// Orientacion automatica: la gravedad en el plano de la pantalla dice donde es "abajo".
// QUAD_ROT[q] = rotacion del canvas para el cuadrante q de atan2(ay, ax) (0=+X, 1=+Y, 2=-X, 3=-Y).
// Calibrado en vivo con Jose (2026-09-11): vertical/iPhone (USB derecha) -> ax=+1 (q=0) -> rot 0;
// de lado/pelicula -> ay=-1 (q=3) -> rot 3. Patron: rotacion = cuadrante.
static const uint8_t QUAD_ROT[4] = { 0, 1, 2, 3 };
static uint8_t g_rot = 0;
static bool g_rotAuto = true;
static void updateOrientation(const ImuSample& imu, uint32_t now) {
  static int cand = -1; static uint32_t candSince = 0;
  if (!imu.ok || !g_rotAuto) return;
  float mag = sqrtf(imu.ax * imu.ax + imu.ay * imu.ay);
  if (mag < 0.75f) { cand = -1; return; }                // acostado o poco inclinado: se queda como esta
  float ang = atan2f(imu.ay, imu.ax) * 57.2958f;          // -180..180
  int q = (int)floorf((ang + 45.f) / 90.f) & 3;           // cuadrante mas cercano
  float center = q * 90.f; float d = fabsf(fmodf(ang - center + 540.f, 360.f) - 180.f);
  if (d > 30.f) { cand = -1; return; }                    // zona muerta entre cuadrantes
  if (q != cand) { cand = q; candSince = now; return; }
  if (now - candSince < 800) return;                      // estable 800 ms
  uint8_t rot = QUAD_ROT[q];
  if (rot != g_rot) { g_rot = rot; gfx->setRotation(rot); Serial.printf("[bob] orientacion q=%d rot=%d\n", q, rot); }
}

// Igual que el demo oficial 02_Drawing_board y que el firmware de fabrica
// ("Power and reset AMOLED panel through TCAL9534"): P0..P2 bajo, pausa, alto.
static bool panelPowerOn() {
  if (!expander.begin(EXPANDER_ADDR, &Wire)) return false;
  for (uint8_t p = 0; p < 3; p++) expander.pinMode(p, OUTPUT);
  for (uint8_t p = 0; p < 3; p++) expander.digitalWrite(p, LOW);
  delay(20);
  for (uint8_t p = 0; p < 3; p++) expander.digitalWrite(p, HIGH);
  delay(50);
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // no bloquear si nadie escucha el USB
  delay(300);
  Serial.println("\n[bob] boot (AMOLED-1.8 / CO5300 QSPI)");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  if (!panelPowerOn()) Serial.println("[bob] XCA9554 no responde — panel sin power/reset");

  if (!gfx->begin()) {  // inicia canvas + panel
    Serial.println("[bob] FATAL: gfx->begin failed");
  }
  panel->setBrightness(255);

  gfx->fillScreen(rgb565(COL_BG));
  gfx->setCursor(40, LCD_HEIGHT / 2 - 10);
  gfx->setTextColor(rgb565(COL_EYE));
  gfx->setTextSize(3);
  gfx->println("Bob Aerogro...");
  gfx->flush();
  delay(300);

  imuBegin();
  micBegin();
  touchBegin();
  rtcBegin();
  nowUnix = rtcNow();
  petBegin();
  faceBegin(gfx);
  netBegin();
  Serial.println("[bob] ready");
}

void loop() {
  uint32_t now = millis();
  float dt = lastLoopMs ? (now - lastLoopMs) / 1000.f : 0.016f;
  lastLoopMs = now;
  if (now - lastRtcMs > 10000) { lastRtcMs = now; uint32_t u = rtcNow(); if (u) nowUnix = u; }

  ImuSample imu = imuRead();
  if (now < 800) { imu.jerk = 0; imu.ok = false; }   // primera lectura del IMU es basura: no cuenta
  float energy = micEnergy();

  // Cuidados por tactil
  TouchInfo t = touchPoll(now);
  switch (t.ev) {
    case TouchEvent::Tap:       petAction(PetAction::Caress); faceAction(FaceAction::Caress); break;
    case TouchEvent::DoubleTap: petAction(PetAction::Feed);   faceAction(FaceAction::Eat);
                                if (petState().need[(int)Need::Hunger] >= 100) faceForce(Emotion::Angry, 900); // empachado
                                break;
    case TouchEvent::Swipe:     petAction(PetAction::Tickle); faceAction(FaceAction::Tickle); break;
    case TouchEvent::Hold:      hudOn = true; break;
    case TouchEvent::HoldEnd:   hudOn = false; break;
    default: break;
  }
  // Eventos de la Mac (hooks de Claude Code via bobd)
  BridgeMsg bm = bridgePoll();
  switch (bm.ev) {
    case BridgeEvent::Hello:   faceDo(FaceMove::Hop); faceSay("Hola Jose!", 3000); break;
    case BridgeEvent::Working: faceFocus(12000); break;                       // se renueva con cada evento
    case BridgeEvent::Done:    faceDo(FaceMove::Dance); petAction(PetAction::Tickle); break;
    case BridgeEvent::Notify:  faceForce(Emotion::Surprised, 4000); faceSay("Te necesito!", 4000); break;
    case BridgeEvent::Error:   faceForce(Emotion::Angry, 1500); break;
    case BridgeEvent::Bye:     faceDo(FaceMove::Wink); faceSay("Bye!", 2000); break;
    case BridgeEvent::Say:     faceDo(FaceMove::Curious); faceSay(bm.text, 5000); break;
    case BridgeEvent::Rot:
      if (!strcmp(bm.text, "auto")) { g_rotAuto = true; faceSay("rotacion auto", 2000); }
      else { g_rotAuto = false; g_rot = atoi(bm.text) & 3; gfx->setRotation(g_rot); char t[24]; snprintf(t, sizeof(t), "rot %d", g_rot); faceSay(t, 2500); }
      break;
    default: break;
  }

  // Novedades por Wi-Fi (hitos del mundo Claude)
  NetNews news = netPoll();
  if (news.fresh) {
    if (news.celebrate) { faceDo(FaceMove::Dance); petAction(PetAction::Tickle); }
    else faceDo(FaceMove::Curious);
    faceSay(news.text, 6000);
  }

  // Sacudida = jugar (una vez por sacudida)
  static uint32_t lastPlayMs = 0;
  if (imu.jerk > 0.55f && now - lastPlayMs > 1500) { lastPlayMs = now; petAction(PetAction::Play); faceAction(FaceAction::Play); }
  // Boca abajo = a dormir. La orientacion de reposo al arrancar cuenta como "pantalla arriba".
  static float azUp = 0;
  if (azUp == 0 && imu.ok && fabsf(imu.az) > 0.5f) azUp = imu.az > 0 ? 1.f : -1.f;
  bool faceDown = imu.ok && azUp != 0 && imu.az * azUp < -0.5f;
  if (faceDown) faceForce(Emotion::Sleepy, 300);

  updateOrientation(imu, now);
  faceUpdate(imu, energy, now);
  bool asleep = faceEmotion() == Emotion::Sleepy;
  petUpdate(dt, asleep, nowUnix);
  faceSetVitality(petVitality());
  { Need w = petWorstNeed(); faceSetNeedHint((petState().need[(int)w] < 20 && w != Need::Sleep) ? (int8_t)w : -1); }
  faceSetScale(petSizeScale());

  if (hudOn) petDrawHud(gfx); else faceDraw();
  gfx->flush();

  Emotion e = faceEmotion();
  if (e != lastPrinted || now - lastPrintMs > 3000) {
    Serial.printf("[bob] emo=%s jerk=%.2f mic=%.2f vit=%.2f dia=%lu energia=%.0f%s ax=%.2f ay=%.2f az=%.2f rot=%d\n", emotionName(e), imu.jerk, energy,
                  petVitality(), (unsigned long)petAgeDays(), petState().need[(int)Need::Sleep], asleep ? " zzz" : "",
                  imu.ax, imu.ay, imu.az, g_rot);
    lastPrinted = e;
    lastPrintMs = now;
  }
  delay(16);
}
