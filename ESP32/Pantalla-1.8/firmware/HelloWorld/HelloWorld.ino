// Prueba base: 01_HelloWorld oficial (arduino-v2, ESP32-S3-Touch-AMOLED-1.8)
// + encendido/reset del panel por el expansor XCA9554 (como 02_Drawing_board y fabrica).
#include <Arduino.h>
#include <Wire.h>
#include "Arduino_GFX_Library.h"
#include <Adafruit_XCA9554.h>
#include "pin_config.h"

Adafruit_XCA9554 expander;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS /* CS */, LCD_SCLK /* SCK */, LCD_SDIO0 /* SDIO0 */, LCD_SDIO1 /* SDIO1 */,
  LCD_SDIO2 /* SDIO2 */, LCD_SDIO3 /* SDIO3 */);

Arduino_CO5300 *gfx = new Arduino_CO5300(
    bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */, LCD_WIDTH /* width */, LCD_HEIGHT /* height */, 16, 0, 0, 0);

void setup(void) {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  Serial.println("Arduino_GFX Hello World example (AMOLED-1.8)");

  Wire.begin(IIC_SDA, IIC_SCL);
  if (!expander.begin(0x20, &Wire)) {
    Serial.println("Failed to find XCA9554 chip");
  } else {
    for (uint8_t p = 0; p < 3; p++) expander.pinMode(p, OUTPUT);
    for (uint8_t p = 0; p < 3; p++) expander.digitalWrite(p, LOW);
    delay(20);
    for (uint8_t p = 0; p < 3; p++) expander.digitalWrite(p, HIGH);
  }

  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->setBrightness(255);

  gfx->setCursor(10, 10);
  gfx->setTextColor(RGB565_RED);
  gfx->println("Hello World!");
  delay(5000);
}

void loop() {
  gfx->setCursor(random(gfx->width()), random(gfx->height()));
  gfx->setTextColor(random(0xffff), random(0xffff));
  gfx->setTextSize(random(6), random(6), random(2));
  gfx->println("Hello World!");
  delay(200);
}
