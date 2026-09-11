#pragma once
// Waveshare ESP32-S3-Touch-AMOLED-1.8 — pines oficiales (arduino-v2/libraries/Mylibrary/pin_config.h)
// OJO: NO es la Touch-LCD-1.83. Panel AMOLED CO5300 368x448 por QSPI; encendido y
// reset del panel van por el expansor I2C XCA9554/TCA9534 (@0x20), no por GPIO.
#include <Arduino.h>

#define PIN_LCD_SDIO0  4
#define PIN_LCD_SDIO1  5
#define PIN_LCD_SDIO2  6
#define PIN_LCD_SDIO3  7
#define PIN_LCD_SCLK  11
#define PIN_LCD_CS    12

#define LCD_WIDTH    368
#define LCD_HEIGHT   448
#define LCD_COL_OFFSET 16   // como el demo oficial: Arduino_CO5300(..., 16, 0, 0, 0)

#define PIN_I2C_SDA   15
#define PIN_I2C_SCL   14
#define PIN_TP_INT    21

#define PIN_I2S_MCLK  16
#define PIN_I2S_BCLK   9
#define PIN_I2S_LRCK  45
#define PIN_I2S_DIN   10
#define PIN_I2S_DOUT   8
#define PIN_I2S_PA    46

#define EXPANDER_ADDR 0x20  // XCA9554: P0/P1/P2 = power/reset del AMOLED
#define AXP2101_ADDR  0x34
#define QMI8658_ADDR  0x6B
#define ES8311_ADDR   0x18

#define COL_BG        0x000000u   // AMOLED: negro puro = pixeles apagados
#define COL_EYE       0x00D4FFu
#define COL_EYE_DIM   0x007A99u
#define COL_GLOW      0x4FC3F7u
#define COL_WHITE     0xFFFFFFu
#define COL_FACE      0x121A2Eu
#define COL_FACE_EDGE 0x1C2740u
#define COL_MOUTH     0xA8C8D8u
#define COL_AMBER     0xFF7A10u
#define COL_SLEEP     0x3A4A66u

static inline uint16_t rgb565(uint32_t rgb) {
  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >> 8) & 0xFF;
  uint8_t b = rgb & 0xFF;
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
