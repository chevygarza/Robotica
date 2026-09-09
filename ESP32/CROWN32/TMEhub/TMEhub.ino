// ====================================================================
//  TMEhub - CrowPanel 1.46" (ESP32-S3R8)
//  Perilla industrial para reseteos de camiones (Cummins PC26).
//  Habla con tme_agent.ps1 en la maquina Windows via LAN.
//  Flujo: push = "Iniciar reseteo?" -> push = arranca -> barra en vivo
//         con frases para el operador -> verde OK / rojo error.
// ====================================================================
#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <cst816t.h>

#include "board.h"
#include "tme_net.h"
#include "tme_ui.h"

LGFX gfx;
cst816t touch(Wire, PIN_TOUCH_RST, PIN_TOUCH_INT);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf  = nullptr;
static lv_color_t *buf1 = nullptr;

// --- Suspension de pantalla (solo en reposo) ---
volatile uint32_t g_lastActivity = 0;
static bool       g_asleep = false;
static uint32_t   g_wakeGuardUntil = 0;
static const uint32_t SLEEP_MS = 60000;   // 60s sin actividad (en reposo)

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  if (gfx.getStartCount() > 0) gfx.endWrite();
  gfx.pushImageDMA(area->x1, area->y1,
                   area->x2 - area->x1 + 1, area->y2 - area->y1 + 1,
                   (lgfx::rgb565_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

// El touch (cst816t) dispara TOQUES FANTASMA (defecto electrico de la linea
// INT): por eso ya NO refresca la actividad -> el sleep de 60s si se alcanza.
// La unica fuente de "despertar" es la perilla (girar/push), que es como el
// operador usa el equipo. Se sigue leyendo el chip solo para consumir el evento.
static void my_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  touch.available();
  data->state = LV_INDEV_STATE_REL;
}

// --- Perilla: push corto / largo ---
volatile bool g_btnShort = false;
volatile bool g_btnLong  = false;
volatile int  g_encDelta = 0;
static int lastStateA = 0;

static void encTask(void *pv) {
  lastStateA = digitalRead(PIN_ENC_A);
  int lastBtn = HIGH;
  uint32_t pressT = 0;
  bool longFired = false;
  for (;;) {
    int a = digitalRead(PIN_ENC_A);
    if (a != lastStateA && a == 1) {           // girar = solo actividad (despierta)
      g_encDelta++;
      g_lastActivity = millis();
    }
    lastStateA = a;

    int b = digitalRead(PIN_ENC_SW);
    if (b == LOW && lastBtn == HIGH) { pressT = millis(); longFired = false; g_lastActivity = millis(); }
    if (b == LOW && !longFired && millis() - pressT > 600) { g_btnLong = true; longFired = true; }
    if (b == HIGH && lastBtn == LOW && !longFired && millis() - pressT > 30) g_btnShort = true;
    lastBtn = b;

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

static void powerUpScreen() {
  pinMode(PIN_LCD_RST, OUTPUT);
  digitalWrite(PIN_LCD_RST, HIGH); delay(10);
  digitalWrite(PIN_LCD_RST, LOW);  delay(10);
  digitalWrite(PIN_LCD_RST, HIGH);

  pinMode(PIN_PWR_LED, OUTPUT);   digitalWrite(PIN_PWR_LED, LOW);
  pinMode(PIN_SCR_PWR_A, OUTPUT); digitalWrite(PIN_SCR_PWR_A, HIGH);
  pinMode(PIN_SCR_PWR_B, OUTPUT); digitalWrite(PIN_SCR_PWR_B, HIGH);
  pinMode(PIN_RGB_PWR, OUTPUT);   digitalWrite(PIN_RGB_PWR, HIGH);
}

static const int BL_PCT = 55;   // brillo fijo (menos calor/desgaste, prendida 24/7)

static void initBacklight() {
  ledcSetup(BL_PWM_CH, BL_PWM_FREQ, BL_PWM_RES);
  ledcAttachPin(PIN_LCD_BL, BL_PWM_CH);
  ledcWrite(BL_PWM_CH, (BL_PCT * 255) / 100);
}

static void screenSleep() {
  ledcWrite(BL_PWM_CH, 0);
  digitalWrite(PIN_PWR_LED, HIGH);
  digitalWrite(PIN_RGB_PWR, LOW);
  g_asleep = true;
}

static void screenWake() {
  ledcWrite(BL_PWM_CH, (BL_PCT * 255) / 100);
  digitalWrite(PIN_PWR_LED, LOW);
  digitalWrite(PIN_RGB_PWR, HIGH);
  g_asleep = false;
  g_lastActivity = millis();
  g_wakeGuardUntil = millis() + 400;   // el push/giro que despierta no actua
}

void setup() {
  Serial.begin(115200);

  powerUpScreen();

  Wire.setPins(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  Wire.begin();

  gfx.init();
  gfx.initDMA();
  gfx.startWrite();
  gfx.fillScreen(TFT_BLACK);

  touch.begin(mode_touch);

  lv_init();
  size_t bufSize = sizeof(lv_color_t) * SCREEN_W * SCREEN_H;
  buf  = (lv_color_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
  buf1 = (lv_color_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
  lv_disp_draw_buf_init(&draw_buf, buf, buf1, SCREEN_W * SCREEN_H);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = SCREEN_W;
  disp_drv.ver_res  = SCREEN_H;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touch_read;
  lv_indev_drv_register(&indev_drv);

  ui_build();
  initBacklight();

  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  xTaskCreatePinnedToCore(encTask, "enc", 3072, nullptr, 1, nullptr, 1);

  tme_begin();   // WiFi + polling del agente (core 0)

  g_lastActivity = millis();
}

void loop() {
  lv_timer_handler();
  ui_tick();

  uint32_t now = millis();

  // si hay un reseteo corriendo (aunque lo iniciaran del escritorio), despierta
  bool processActive = false;
  if (tme_lock(5)) {
    processActive = (g_tme.state == TME_STARTING || g_tme.state == TME_CHECK1 || g_tme.state == TME_CHECK2);
    tme_unlock();
  }

  if (g_asleep) {
    if (now - g_lastActivity < 400 || processActive) screenWake();
    g_btnShort = false; g_btnLong = false; g_encDelta = 0;
  } else {
    bool guarded = now < g_wakeGuardUntil;
    if (g_btnShort) { g_btnShort = false; if (!guarded) ui_push(); }
    if (g_btnLong)  { g_btnLong  = false; if (!guarded) ui_long(); }
    g_encDelta = 0;
    if (ui_can_sleep() && !processActive && now - g_lastActivity > SLEEP_MS) screenSleep();
  }

  delay(5);
}
