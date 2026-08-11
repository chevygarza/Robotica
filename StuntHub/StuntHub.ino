// ====================================================================
//  StuntHub - CrowPanel 1.46" (ESP32-S3R8)
//  FASE 1: UI propia + reloj + clima Monterrey + perfil de X (@stuntech)
//  (sin IR ni BME280 todavía)
// ====================================================================
#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <cst816t.h>
#include "time.h"

#include "board.h"
#include "app_net.h"
#include "app_ui.h"
#include "app_hue.h"
#include "app_markets.h"
#include "app_wol.h"
#include "player.h"

LGFX gfx;
cst816t touch(Wire, PIN_TOUCH_RST, PIN_TOUCH_INT);

// ---- LVGL ----
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf  = nullptr;
static lv_color_t *buf1 = nullptr;

// --- Suspensión de pantalla ---
volatile uint32_t g_lastActivity = 0;
static bool       g_asleep = false;
static uint32_t   g_wakeGuardUntil = 0;
static const uint32_t SLEEP_MS = 30000;   // 30s sin actividad -> apaga

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  if (gfx.getStartCount() > 0) gfx.endWrite();
  gfx.pushImageDMA(area->x1, area->y1,
                   area->x2 - area->x1 + 1, area->y2 - area->y1 + 1,
                   (lgfx::rgb565_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

static void my_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  if (touch.available() && !(touch.x == 0 && touch.y == 0)) {
    g_lastActivity = millis();
    // Si está dormido (o recién despertó), el toque solo despierta, no actúa
    if (g_asleep || millis() < g_wakeGuardUntil) { data->state = LV_INDEV_STATE_REL; return; }
    data->state   = LV_INDEV_STATE_PR;
    data->point.x = touch.x;
    data->point.y = touch.y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ---- Perilla (encoder + pulsador) ----
volatile int  g_encDelta = 0;
volatile bool g_btnShort = false;
volatile bool g_btnLong  = false;
static int lastStateA = 0;

static void encTask(void *pv) {
  lastStateA = digitalRead(PIN_ENC_A);
  int lastBtn = HIGH;
  uint32_t pressT = 0;
  bool longFired = false;
  for (;;) {
    // --- Encoder (giro) ---
    int a = digitalRead(PIN_ENC_A);
    if (a != lastStateA && a == 1) {            // un paso por detente
      if (digitalRead(PIN_ENC_B) != a) g_encDelta++;   // sentido horario = bajar
      else                             g_encDelta--;
      g_lastActivity = millis();
    }
    lastStateA = a;

    // --- Botón: corto vs largo (mantener) ---
    int b = digitalRead(PIN_ENC_SW);
    if (b == LOW && lastBtn == HIGH) { pressT = millis(); longFired = false; g_lastActivity = millis(); }
    if (b == LOW && !longFired && millis() - pressT > 600) { g_btnLong = true; longFired = true; }
    if (b == HIGH && lastBtn == LOW && !longFired && millis() - pressT > 30) g_btnShort = true;
    lastBtn = b;

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

static void powerUpScreen() {
  // Secuencia de reset/encendido tomada del ejemplo oficial
  pinMode(PIN_LCD_RST, OUTPUT);
  digitalWrite(PIN_LCD_RST, HIGH); delay(10);
  digitalWrite(PIN_LCD_RST, LOW);  delay(10);
  digitalWrite(PIN_LCD_RST, HIGH);

  pinMode(PIN_PWR_LED, OUTPUT); digitalWrite(PIN_PWR_LED, LOW);  // LED encendido (activo LOW)
  pinMode(PIN_SCR_PWR_A, OUTPUT); digitalWrite(PIN_SCR_PWR_A, HIGH);
  pinMode(PIN_SCR_PWR_B, OUTPUT); digitalWrite(PIN_SCR_PWR_B, HIGH);
  pinMode(PIN_RGB_PWR, OUTPUT);   digitalWrite(PIN_RGB_PWR, HIGH);
}

// Brillo según horario: día 70%, noche (10pm-7am) 25%. Sin hora aún -> día.
static uint8_t briPct() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return 70;
  return (t.tm_hour >= 22 || t.tm_hour < 7) ? 25 : 70;
}

static void initBacklight() {
  ledcSetup(BL_PWM_CH, BL_PWM_FREQ, BL_PWM_RES);
  ledcAttachPin(PIN_LCD_BL, BL_PWM_CH);
  ledcWrite(BL_PWM_CH, (briPct() * 255) / 100);
}

static void screenSleep() {
  ledcWrite(BL_PWM_CH, 0);            // apaga pantalla
  digitalWrite(PIN_PWR_LED, HIGH);   // apaga LED de encendido (activo LOW)
  digitalWrite(PIN_RGB_PWR, LOW);    // corta LEDs RGB
  ui_screen_off();                   // suelta el GIF (CPU/RAM)
  g_asleep = true;
}

static void screenWake() {
  ledcWrite(BL_PWM_CH, (briPct() * 255) / 100);
  digitalWrite(PIN_PWR_LED, LOW);
  digitalWrite(PIN_RGB_PWR, HIGH);
  ui_screen_on();
  g_asleep = false;
  g_lastActivity = millis();
  g_wakeGuardUntil = millis() + 350;  // ignora el toque/giro que despertó
}

void setup() {
  Serial.begin(115200);            // sin while(!Serial): debe bootear sin PC

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
  if (!buf || !buf1) Serial.println("ERR: no se pudo reservar buffers LVGL en PSRAM");
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

  // Perilla
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  xTaskCreatePinnedToCore(encTask, "enc", 3072, nullptr, 1, nullptr, 1);

  net_begin();   // WiFi + NTP + clima + X en su propio task (core 0)
  hue_begin();   // Philips Hue en su propio task (core 0)
  markets_begin(); // Mercados (cripto) en su propio task (core 0)
  pc_status_begin(); // Estado directo de la PC gamer (task persistente, core 0)
  player_begin();    // DFPlayer por UART1. Sin modulo -> todo no-op, la UI sigue.

  g_lastActivity = millis();   // arranca el contador de inactividad
}

void loop() {
  lv_timer_handler();
  ui_tick();
  player_tick();   // vacia la cola del DFPlayer y atiende sus avisos

  // Consumir eventos de la perilla en el hilo de LVGL
  uint32_t now = millis();
  if (g_asleep) {
    if (now - g_lastActivity < 400) screenWake();            // actividad reciente -> despierta
    g_encDelta = 0; g_btnShort = false; g_btnLong = false;   // ignora eventos mientras duerme
  } else {
    if (g_encDelta != 0) { int d = g_encDelta; g_encDelta = 0; ui_nav(d > 0 ? 1 : -1); }
    if (g_btnShort) { g_btnShort = false; ui_select(); }
    if (g_btnLong)  { g_btnLong  = false; ui_back();   }
    if (now - g_lastActivity > SLEEP_MS) screenSleep();      // 30s sin actividad -> apaga
    static uint32_t briT = 0;                                // brillo día/noche
    if (now - briT > 60000) { briT = now; ledcWrite(BL_PWM_CH, (briPct() * 255) / 100); }
  }

  delay(5);
}
