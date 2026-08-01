#include "display.h"
#include "pins.h"
#include <LovyanGFX.h>
#include <Wire.h>
#include <cst816t.h>

// ─── Panel ───────────────────────────────────────────────────────────────────
// Valores tomados del codigo oficial de Elecrow. Dos que NO se pueden dejar en
// default: memory_height (LovyanGFX asume 390 para el ST77961 y la imagen
// queda corrida 30px) y rgb_order (en false salen los colores invertidos).
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST77961 _panel;
  lgfx::Bus_SPI       _bus;

public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 80000000;
      cfg.freq_read   = 20000000;
      cfg.spi_3wire   = true;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = PIN_LCD_SCLK;
      cfg.pin_mosi    = PIN_LCD_MOSI;
      cfg.pin_miso    = -1;
      cfg.pin_dc      = PIN_LCD_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs           = PIN_LCD_CS;
      cfg.pin_rst          = PIN_LCD_RST;
      cfg.pin_busy         = -1;
      cfg.memory_width     = 360;
      cfg.memory_height    = 360;
      cfg.panel_width      = 360;
      cfg.panel_height     = 360;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = false;
      cfg.rgb_order        = true;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

static LGFX      gfx;
static cst816t   touch(Wire, PIN_TP_RST, PIN_TP_INT);
static bool      touchFlag = false;

// ─── LVGL ────────────────────────────────────────────────────────────────────
#define SCR_W 360
#define SCR_H 360

static lv_disp_draw_buf_t draw_buf;
static lv_color_t*        buf0 = nullptr;
static lv_color_t*        buf1 = nullptr;

// Doble buffer de pantalla completa en PSRAM (259KB cada uno). Es lo que hace
// el ejemplo oficial y es lo correcto aqui: pushImageDMA es asincrono, asi que
// LVGL necesita un segundo buffer para dibujar mientras el DMA lee el primero.
static void flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* px) {
  if (gfx.getStartCount() > 0) gfx.endWrite();
  gfx.pushImageDMA(area->x1, area->y1,
                   area->x2 - area->x1 + 1,
                   area->y2 - area->y1 + 1,
                   (lgfx::rgb565_t*)&px->full);
  lv_disp_flush_ready(disp);
}

static void touch_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  if (touch.available() && !(touch.x == 0 && touch.y == 0)) {
    data->state   = LV_INDEV_STATE_PR;
    data->point.x = touch.x;
    data->point.y = touch.y;
    touchFlag = true;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ─── Backlight ───────────────────────────────────────────────────────────────
#define BL_CHANNEL    0
#define BL_FREQ    5000
#define BL_BITS       8

static uint8_t  blCur = 0, blFrom = 0, blTo = 0;
static uint32_t blStart = 0, blDur = 0;

static void blApply(uint8_t pct) {
  blCur = pct > 100 ? 100 : pct;
  // Curva cuadratica: el ojo percibe la luz de forma no lineal, y un fade
  // lineal en PWM se ve como un salton al final.
  uint32_t duty = (uint32_t)blCur * blCur * 255 / 10000;
  ledcWrite(BL_CHANNEL, duty);
}

bool display_begin() {
  // 1) Corriente del panel. Sin estos dos pines en HIGH la pantalla queda
  //    negra aunque el SPI funcione. No aparece en el wiki, solo en el codigo
  //    de fabrica de Elecrow.
  pinMode(PIN_LCD_PWR_A, OUTPUT);
  digitalWrite(PIN_LCD_PWR_A, HIGH);
  pinMode(PIN_LCD_PWR_B, OUTPUT);
  digitalWrite(PIN_LCD_PWR_B, HIGH);

  // 2) Pulso de reset a mano antes de init: el VDD sube en alto al arrancar y
  //    el panel necesita el ciclo para quedar en un estado conocido.
  pinMode(PIN_LCD_RST, OUTPUT);
  digitalWrite(PIN_LCD_RST, HIGH);
  delay(10);
  digitalWrite(PIN_LCD_RST, LOW);
  delay(10);
  digitalWrite(PIN_LCD_RST, HIGH);

  ledcSetup(BL_CHANNEL, BL_FREQ, BL_BITS);
  ledcAttachPin(PIN_LCD_BL, BL_CHANNEL);
  blApply(0);

  gfx.init();
  gfx.initDMA();
  gfx.startWrite();
  gfx.fillScreen(TFT_BLACK);

  // 3) Tactil en el bus 0 remapeado a 6/7.
  Wire.setPins(PIN_TP_SDA, PIN_TP_SCL);
  Wire.begin();
  touch.begin(mode_touch);

  lv_init();

  size_t bytes = sizeof(lv_color_t) * SCR_W * SCR_H;
  buf0 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  buf1 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  if (!buf0 || !buf1) {
    Serial.println("ERROR: no alcanzo la PSRAM para los buffers de LVGL");
    return false;
  }
  lv_disp_draw_buf_init(&draw_buf, buf0, buf1, SCR_W * SCR_H);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = SCR_W;
  disp_drv.ver_res  = SCR_H;
  disp_drv.flush_cb = flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_cb;
  lv_indev_drv_register(&indev_drv);

  return true;
}

void display_tick() {
  if (!blDur) return;
  uint32_t t = millis() - blStart;
  if (t >= blDur) {
    blDur = 0;
    blApply(blTo);
    return;
  }
  float k = (float)t / (float)blDur;
  k = k * k * (3.0f - 2.0f * k);            // ease in-out
  blApply((uint8_t)(blFrom + (blTo - blFrom) * k));
}

void display_backlight(uint8_t pct) { blDur = 0; blApply(pct); }

void display_backlight_fade(uint8_t pct, uint16_t ms) {
  if (pct > 100) pct = 100;
  if (!ms) { display_backlight(pct); return; }
  blFrom = blCur; blTo = pct; blStart = millis(); blDur = ms;
}

uint8_t display_backlight_level() { return blCur; }
bool    display_backlight_busy()  { return blDur != 0; }

bool display_touched() {
  bool t = touchFlag;
  touchFlag = false;
  return t;
}
