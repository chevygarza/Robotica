#pragma once
// ====================================================================
//  Hardware de la CrowPanel 1.46" redonda (ESP32-S3R8)
//  Init de pantalla tomada del ejemplo oficial (panel ST77961, SPI).
// ====================================================================
#include <LovyanGFX.h>

// ---- Pines (esquemático V1.0) ----
#define PIN_LCD_SCLK   10
#define PIN_LCD_MOSI   11
#define PIN_LCD_CS      9
#define PIN_LCD_DC      3
#define PIN_LCD_RST    14
#define PIN_LCD_BL     46

#define PIN_TOUCH_SDA   6   // bus Wire del táctil
#define PIN_TOUCH_SCL   7
#define PIN_TOUCH_RST  13
#define PIN_TOUCH_INT   5

#define PIN_ENC_A      45
#define PIN_ENC_B      42
#define PIN_ENC_SW     41

#define PIN_PWR_LED    40   // LED de encendido (activo en LOW)
#define PIN_RGB_DIN    48   // NeoPixel x8 (fase posterior)

// Pines de alimentación interna que el ejemplo activa para encender la pantalla
#define PIN_SCR_PWR_A   1
#define PIN_SCR_PWR_B   2
#define PIN_RGB_PWR    17

static const uint32_t SCREEN_W = 360;
static const uint32_t SCREEN_H = 360;

// PWM del backlight
static const int BL_PWM_CH   = 0;
static const int BL_PWM_FREQ = 5000;
static const int BL_PWM_RES  = 8;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST77961 _panel;
  lgfx::Bus_SPI       _bus;
public:
  LGFX(void) {
    {
      auto cfg = _bus.config();
      cfg.spi_host   = SPI2_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read  = 20000000;
      cfg.spi_3wire  = true;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk   = PIN_LCD_SCLK;
      cfg.pin_mosi   = PIN_LCD_MOSI;
      cfg.pin_miso   = -1;
      cfg.pin_dc     = PIN_LCD_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = PIN_LCD_CS;
      cfg.pin_rst  = PIN_LCD_RST;
      cfg.pin_busy = -1;
      cfg.memory_width  = SCREEN_W;
      cfg.memory_height = SCREEN_H;
      cfg.panel_width   = SCREEN_W;
      cfg.panel_height  = SCREEN_H;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable  = false;
      cfg.invert    = false;
      cfg.rgb_order = true;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};
