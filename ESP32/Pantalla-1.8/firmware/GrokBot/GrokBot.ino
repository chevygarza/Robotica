// GrokBot — cara Grok en la Waveshare ESP32-S3-Touch-AMOLED-1.8
// Panel CO5300 por QSPI (Arduino_GFX 1.6.4, bundle arduino-v2 de Waveshare).
// Se dibuja en un canvas en PSRAM y se manda el cuadro completo: sin parpadeo.
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_XCA9554.h>
#include "board.h"
#include "imu.h"
#include "mic.h"
#include "face.h"

Adafruit_XCA9554 expander;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK,
                                             PIN_LCD_SDIO0, PIN_LCD_SDIO1,
                                             PIN_LCD_SDIO2, PIN_LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(bus, GFX_NOT_DEFINED /* RST via expansor */,
                                           0 /* rotation */, LCD_WIDTH, LCD_HEIGHT,
                                           LCD_COL_OFFSET, 0, 0, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);

static Emotion lastPrinted = Emotion::Idle;
static uint32_t lastPrintMs = 0;

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
  Serial.println("\n[GrokBot] boot (AMOLED-1.8 / CO5300 QSPI)");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  if (!panelPowerOn()) Serial.println("[GrokBot] XCA9554 no responde — panel sin power/reset");

  if (!gfx->begin()) {  // inicia canvas + panel
    Serial.println("[GrokBot] FATAL: gfx->begin failed");
  }
  panel->setBrightness(255);

  gfx->fillScreen(rgb565(COL_BG));
  gfx->setCursor(40, LCD_HEIGHT / 2 - 10);
  gfx->setTextColor(rgb565(COL_EYE));
  gfx->setTextSize(3);
  gfx->println("GrokBot...");
  gfx->flush();
  delay(300);

  imuBegin();
  micBegin();
  faceBegin(gfx);
  Serial.println("[GrokBot] ready");
}

void loop() {
  uint32_t now = millis();
  ImuSample imu = imuRead();
  float energy = micEnergy();
  faceUpdate(imu, energy, now);
  faceDraw();
  gfx->flush();

  Emotion e = faceEmotion();
  if (e != lastPrinted || now - lastPrintMs > 3000) {
    Serial.printf("[GrokBot] emo=%s jerk=%.2f mic=%.2f\n", emotionName(e), imu.jerk, energy);
    lastPrinted = e;
    lastPrintMs = now;
  }
  delay(16);
}
