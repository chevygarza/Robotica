#include "display.h"
#include "board.h"
#include "axs_init.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ─── El bus ──────────────────────────────────────────────────────────────────
// El AXS15231B en QSPI recibe 32 bits antes de cualquier cosa: un opcode y el
// comando, en UNA linea: 0x02 = "escribe registro", 0x32 = "escribe pixeles"
// (y solo los pixeles van por las cuatro lineas). Se calca el camino de
// Arduino_GFX, que es el probado en esta placa: opcode en la fase de comando
// del SPI, el registro en la de direccion, y el CS a mano por GPIO para que
// un cuadro entero, en varios trozos, viva bajo una sola bajada de CS.
#define CHUNK_ROWS   40
#define CHUNK_PX     (CHUNK_ROWS * PANEL_W)
#define CHUNKS       (PANEL_H / CHUNK_ROWS)

static spi_device_handle_t dev;
static uint16_t*          chunk[2];
static spi_transaction_ext_t tx[2];   // una por buffer de trozo
static SemaphoreHandle_t  txDone;     // una ficha por trozo que termino de salir
static SemaphoreHandle_t  busMtx;     // un cuadro entero o un comando suelto, nunca los dos
static TaskHandle_t       flushTask;
static lv_disp_drv_t      drv;
static lv_disp_draw_buf_t drawBuf;
static const uint16_t* volatile pending;   // el cuadro que espera salir
static volatile uint32_t  frames;

static inline void csLow()  { gpio_set_level((gpio_num_t)PIN_LCD_CS, 0); }
static inline void csHigh() { gpio_set_level((gpio_num_t)PIN_LCD_CS, 1); }

// Solo cuentan los trozos de pixeles: el post_cb tambien se dispara con los
// comandos cortos, y contarlos hacia que el CS subiera antes de que saliera
// el ultimo trozo (el panel cortaba las ultimas filas).
static void IRAM_ATTR onTxDone(spi_transaction_t* t) {
  if (!t->user) return;
  BaseType_t hp = pdFALSE;
  xSemaphoreGiveFromISR(txDone, &hp);
  if (hp) portYIELD_FROM_ISR();
}

// Escribe un registro: 0x02, registro, y sus parametros, todo en una linea.
static void cmd(uint8_t c, const uint8_t* p = nullptr, size_t n = 0) {
  spi_transaction_ext_t t = {};
  t.base.flags     = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  t.base.cmd       = 0x02;
  t.base.addr      = (uint32_t)c << 8;
  t.base.length    = n * 8;
  t.base.tx_buffer = n ? p : nullptr;
  csLow();
  esp_err_t e = spi_device_polling_transmit(dev, &t.base);
  csHigh();
  if (e != ESP_OK) Serial.printf("[lcd] cmd %02X: %s\n", c, esp_err_to_name(e));
}

// Gira 90 grados CHUNK_ROWS filas del panel, a partir de la fila p0, leyendo
// del lienzo apaisado. Se recorre el lienzo por FILAS para que cada lectura
// de PSRAM sea contigua; las escrituras saltan, pero caen en RAM interna.
static void rotateChunk(const uint16_t* src, int p0, uint16_t* dst) {
  for (int ly = 0; ly < SCR_H; ly++) {
#if ROT_FLIP
    // panel (px, py)  <-  lienzo (lx = py, ly = SCR_H-1-px)
    const uint16_t* s = src + ly * SCR_W + p0;
    uint16_t*       d = dst + (SCR_H - 1 - ly);
    for (int j = 0; j < CHUNK_ROWS; j++) d[j * PANEL_W] = s[j];
#else
    // panel (px, py)  <-  lienzo (lx = SCR_W-1-py, ly = px)
    const uint16_t* s = src + ly * SCR_W + (SCR_W - 1 - p0);
    uint16_t*       d = dst + ly;
    for (int j = 0; j < CHUNK_ROWS; j++) d[j * PANEL_W] = s[-j];
#endif
  }
}

// Vive en el core 0 (el de WiFi), para que girar y mandar el cuadro no le
// quite tiempo a LVGL, que dibuja el siguiente en el core 1.
static void flushLoop(void*) {
  static const uint8_t caset[4] = { 0, 0, (PANEL_W - 1) >> 8, (PANEL_W - 1) & 0xFF };
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    const uint16_t* src = pending;
    xSemaphoreTake(busMtx, portMAX_DELAY);
    static const uint8_t raset[4] = { 0, 0, (PANEL_H - 1) >> 8, (PANEL_H - 1) & 0xFF };
    cmd(0x2A, caset, 4);
    cmd(0x2B, raset, 4);
    cmd(0x2C);                                 // RAMWR: desde la fila 0
    csLow();
    for (int k = 0; k < CHUNKS; k++) {
      int b = k & 1;
      if (k >= 2) xSemaphoreTake(txDone, portMAX_DELAY);   // el buffer b ya salio
      rotateChunk(src, k * CHUNK_ROWS, chunk[b]);
      spi_transaction_ext_t* t = &tx[b];
      if (k == 0) {                            // el primer trozo lleva 0x32 3C: "sigue escribiendo"
        t->base.flags = SPI_TRANS_MODE_QIO;
        t->base.cmd   = 0x32;
        t->base.addr  = 0x003C00;
      } else {                                 // los demas son datos a secas, sin fases
        t->base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                        SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
        t->command_bits = 0; t->address_bits = 0; t->dummy_bits = 0;
      }
      t->base.length    = CHUNK_PX * 16;
      t->base.tx_buffer = chunk[b];
      t->base.user      = (void*)1;            // "soy pixeles": cuenta al terminar
      esp_err_t e = spi_device_queue_trans(dev, &t->base, portMAX_DELAY);
      if (e != ESP_OK) { Serial.printf("[lcd] trozo %d: %s\n", k, esp_err_to_name(e)); xSemaphoreGive(txDone); }
    }
    xSemaphoreTake(txDone, portMAX_DELAY);
    xSemaphoreTake(txDone, portMAX_DELAY);
    csHigh();
    xSemaphoreGive(busMtx);
    frames = frames + 1;
    lv_disp_flush_ready(&drv);
  }
}

// LVGL en modo directo: dibuja solo lo que cambio, pero el buffer siempre tiene
// el cuadro completo. Al terminar el ultimo pedazo se manda el cuadro entero,
// que es lo unico que el panel acepta.
static void flushCb(lv_disp_drv_t* d, const lv_area_t*, lv_color_t* px) {
  if (!lv_disp_flush_is_last(d)) { lv_disp_flush_ready(d); return; }
  pending = (const uint16_t*)px;
  xTaskNotifyGive(flushTask);
}

// ─── Tactil ──────────────────────────────────────────────────────────────────
static bool asleep = false, woke = false;

static bool touchRaw(uint16_t& x, uint16_t& y) {
  static const uint8_t rd[11] = { 0xB5, 0xAB, 0xA5, 0x5A, 0, 0, 0, 8, 0, 0, 0 };
  Wire.beginTransmission(TP_ADDR);
  Wire.write(rd, sizeof(rd));
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)8) != 8) return false;
  uint8_t d[8];
  for (uint8_t& b : d) b = Wire.read();
  if (d[0] != 0 || d[1] == 0) return false;          // sin dedo
  uint16_t px = ((d[2] & 0x0F) << 8) | d[3];         // en coordenadas del panel
  uint16_t py = ((d[4] & 0x0F) << 8) | d[5];
#if ROT_FLIP
  x = py;              y = SCR_H - 1 - px;
#else
  x = SCR_W - 1 - py;  y = px;
#endif
  return true;
}

static void touchRead(lv_indev_drv_t*, lv_indev_data_t* data) {
  uint16_t x, y;
  bool down = touchRaw(x, y);
  if (asleep) {                       // dormida: el toque despierta, no actua
    if (down) woke = true;
    data->state = LV_INDEV_STATE_REL;
    return;
  }
  data->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  if (down) { data->point.x = x; data->point.y = y; }
}

// ─── Backlight ───────────────────────────────────────────────────────────────
static uint8_t  blCur = 0, blFrom = 0, blTo = 0;
static uint32_t blStart = 0, blDur = 0;

static void blApply(uint8_t pct) {
  blCur = pct > 100 ? 100 : pct;
  // Curva cuadratica: el ojo no es lineal, y un fade lineal en PWM se ve como
  // un salton al final.
  ledcWrite(PIN_LCD_BL, (uint32_t)blCur * blCur * 255 / 10000);
}

// ─── Arranque ────────────────────────────────────────────────────────────────
bool display_begin() {
  ledcAttach(PIN_LCD_BL, 5000, 8);
  blApply(0);

  gpio_reset_pin((gpio_num_t)PIN_LCD_CS);
  gpio_set_direction((gpio_num_t)PIN_LCD_CS, GPIO_MODE_OUTPUT);
  csHigh();

  spi_bus_config_t bus = {};
  bus.sclk_io_num     = PIN_LCD_SCK;
  bus.data0_io_num    = PIN_LCD_D0;
  bus.data1_io_num    = PIN_LCD_D1;
  bus.data2_io_num    = PIN_LCD_D2;
  bus.data3_io_num    = PIN_LCD_D3;
  bus.max_transfer_sz = CHUNK_PX * 2;
  bus.flags           = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;
  esp_err_t e = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
  if (e != ESP_OK) { Serial.printf("[lcd] bus: %s\n", esp_err_to_name(e)); return false; }

  spi_device_interface_config_t dcfg = {};
  dcfg.command_bits   = 8;
  dcfg.address_bits   = 24;
  dcfg.mode           = 0;
  dcfg.clock_speed_hz = LCD_QSPI_HZ;
  dcfg.spics_io_num   = -1;                    // el CS lo llevamos a mano
  dcfg.flags          = SPI_DEVICE_HALFDUPLEX;
  dcfg.queue_size     = 2;
  dcfg.post_cb        = onTxDone;
  txDone = xSemaphoreCreateCounting(CHUNKS, 0);
  busMtx = xSemaphoreCreateMutex();
  e = spi_bus_add_device(SPI2_HOST, &dcfg, &dev);
  if (e != ESP_OK) { Serial.printf("[lcd] device: %s\n", esp_err_to_name(e)); return false; }

  // Arranque como lo hace Arduino_GFX en esta placa: SWRESET (no hay pin de
  // reset), el blob del fabricante para el panel de 320x480, y los DCS de
  // siempre. La secuencia minima de ESPHome no basto aqui.
  static const uint8_t colmod[1] = { 0x55 };           // RGB565
  static const uint8_t madctl[1] = { 0x00 };           // sin rotacion: se hace en software
  cmd(0x01);  delay(200);                              // SWRESET
  for (size_t i = 0; i < sizeof(AXS_INIT);) {
    uint8_t c = AXS_INIT[i], n = AXS_INIT[i + 1];
    if (c == 0xFF) { delay(n); i += 2; continue; }
    cmd(c, &AXS_INIT[i + 2], n);
    i += 2 + n;
  }
  cmd(0x20);                                           // INVOFF
  cmd(0x36, madctl, 1);
  cmd(0x3A, colmod, 1);

  // Tactil, en el mismo chip.
  Wire.begin(PIN_TP_SDA, PIN_TP_SCL, 400000);

  // LVGL: dos cuadros completos en PSRAM (300KB cada uno) y los dos trozos de
  // DMA en RAM interna. LVGL dibuja el cuadro N+1 mientras sale el N.
  lv_init();
  size_t bytes = SCR_W * SCR_H * sizeof(lv_color_t);
  lv_color_t* b0 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  lv_color_t* b1 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  chunk[0] = (uint16_t*)heap_caps_malloc(CHUNK_PX * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  chunk[1] = (uint16_t*)heap_caps_malloc(CHUNK_PX * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!b0 || !b1 || !chunk[0] || !chunk[1]) return false;
  lv_disp_draw_buf_init(&drawBuf, b0, b1, SCR_W * SCR_H);

  lv_disp_drv_init(&drv);
  drv.hor_res     = SCR_W;
  drv.ver_res     = SCR_H;
  drv.flush_cb    = flushCb;
  drv.draw_buf    = &drawBuf;
  drv.direct_mode = 1;
  lv_disp_drv_register(&drv);

  static lv_indev_drv_t indev;
  lv_indev_drv_init(&indev);
  indev.type    = LV_INDEV_TYPE_POINTER;
  indev.read_cb = touchRead;
  lv_indev_drv_register(&indev);

  xTaskCreatePinnedToCore(flushLoop, "lcd", 3072, nullptr, 3, &flushTask, 0);
  return true;
}

void display_tick() {
  if (!blDur) return;
  uint32_t t = millis() - blStart;
  if (t >= blDur) { blDur = 0; blApply(blTo); return; }
  float k = (float)t / (float)blDur;
  k = k * k * (3.0f - 2.0f * k);                       // ease in-out
  blApply((uint8_t)(blFrom + (blTo - blFrom) * k));
}

void display_backlight(uint8_t pct) { blDur = 0; blApply(pct); }

void display_off() {
  display_backlight(0);
  xSemaphoreTake(busMtx, portMAX_DELAY);               // que no haya un cuadro a medias
  cmd(0x28);                                           // DISPOFF
  cmd(0x10);  delay(10);                               // SLPIN
  xSemaphoreGive(busMtx);
}

void display_on() {
  xSemaphoreTake(busMtx, portMAX_DELAY);
  cmd(0x11);  delay(120);                              // SLPOUT
  cmd(0x29);                                           // DISPON: el GRAM sigue ahi
  xSemaphoreGive(busMtx);
}

void display_backlight_fade(uint8_t pct, uint16_t ms) {
  if (pct > 100) pct = 100;
  if (!ms) { display_backlight(pct); return; }
  blFrom = blCur; blTo = pct; blStart = millis(); blDur = ms;
}

uint8_t  display_backlight_level() { return blCur; }
void     display_sleep(bool a)     { asleep = a; woke = false; }
bool     display_woke()            { bool w = woke; woke = false; return w; }
uint32_t display_frames()          { uint32_t f = frames; frames = 0; return f; }
