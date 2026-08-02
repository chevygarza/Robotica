// ─────────────────────────────────────────────────────────────────────────────
// Stuntech VNL-1 — Reproductor de vinilos digital
// Etapa 4: maquina de estados completa. Selector, detalle y reproduccion.
//
// Placa:  Elecrow CrowPanel 1.46" HMI ESP32-S3 Rotary Display
// Core:   esp32 2.0.17   ·   LVGL 8.3   ·   LovyanGFX 1.2.7 (no la 1.2.21)
// FQBN:   esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,
//         USBMode=hwcdc,CDCOnBoot=cdc
//
// SELECTOR  girar = disco      · push = entrar (o volver a lo que suena)
// DETALLE   push  = reproducir · push 3s = atras
// TOCANDO   girar = volumen    · push = pausa · doble = siguiente · 3s = atras
// ─────────────────────────────────────────────────────────────────────────────
#include "pins.h"
#include "knob.h"
#include "display.h"
#include "player.h"
#include "app.h"
#include "netclock.h"
#include "alarm.h"

// Etapa 3: en 1 el DFPlayer entra en juego. Sin el modulo conectado la UI
// funciona completa, solo sin sonido.
#define STAGE3_AUDIO 1

// Prueba automatica: inyecta la secuencia completa de gestos para validar las
// transiciones sin manos. Se deja en 0 para uso normal.
#define SELFTEST 0

static uint32_t frames = 0;
static uint32_t fpsMs  = 0;

#if SELFTEST
static void selftest() {
  struct Step { uint16_t atS; KnobEvent e; const char* what; };
  static const Step SEQ[] = {
    {  3, KNOB_PRESS,        "selector: entrar al detalle" },
    {  6, KNOB_PRESS,        "detalle: reproducir" },
    {  9, KNOB_CW,           "tocando: volumen +" },
    { 10, KNOB_CW,           "tocando: volumen +" },
    { 12, KNOB_DOUBLE_PRESS, "tocando: siguiente cancion" },
    { 15, KNOB_PRESS,        "tocando: pausa" },
    { 17, KNOB_PRESS,        "tocando: reanudar" },
    { 20, KNOB_LONG_PRESS,   "tocando: volver a la biblioteca" },
    { 23, KNOB_CW,           "selector: siguiente disco" },
    { 25, KNOB_PRESS,        "selector: push en disco nuevo" },
  };
  static uint8_t i = 0;
  if (i >= sizeof(SEQ) / sizeof(SEQ[0])) return;
  if (millis() / 1000 < SEQ[i].atS) return;
  Serial.printf("[test] %s\n", SEQ[i].what);
  app_event(SEQ[i].e);
  i++;
}
#endif

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_POWER_LED, OUTPUT);
  digitalWrite(PIN_POWER_LED, LOW);    // el LED de power es activo en LOW

  knob::begin();
  alarm_begin();

  Serial.println();
  Serial.println("VNL-1  etapa 4 — maquina de estados");

  if (!display_begin()) {
    Serial.println("display_begin() fallo, me detengo");
    while (true) delay(1000);
  }
  Serial.printf("PSRAM libre: %u KB   heap interno: %u KB\n",
                (unsigned)(ESP.getFreePsram() / 1024),
                (unsigned)(ESP.getFreeHeap() / 1024));

  if (!app_begin()) {
    Serial.println("app_begin() fallo, me detengo");
    while (true) delay(1000);
  }

#if STAGE3_AUDIO
  if (player_begin()) player_set_volume(20);
  else Serial.println("sigo sin audio: la pantalla y la perilla van igual");
#endif

  // El WiFi arranca despues de la UI: asi la pantalla ya esta viva mientras la
  // red negocia, en vez de dejar el aparato en negro esperando a un router.
  clock_begin();

  // Primer render completo antes de encender la luz: el fade revela la UI ya
  // dibujada en vez de mostrar el barrido de LVGL.
  lv_timer_handler();
  display_backlight_fade(100, 600);

  Serial.println("---------------------------------------------");
  fpsMs = millis();
}

void loop() {
  knob::update();
  if (display_touched()) knob::touch();

  KnobEvent e;
  while ((e = knob::read()) != KNOB_NONE) {
    app_event(e);

    static const char* NAMES[] = { "-", "CW", "CCW", "PRESS", "DOBLE", "LONG",
                                   "DOWN" };
    static const char* STATES[] = { "SELECTOR", "TOCANDO", "ALARMA" };
    if (e != KNOB_DOWN) {
      Serial.printf("%-6s -> %s\n", NAMES[e], STATES[app_state()]);
    }
  }

#if SELFTEST
  selftest();
#endif

  app_tick();
  display_tick();
#if STAGE3_AUDIO
  player_tick();
#endif
  lv_timer_handler();

  frames++;
  if (millis() - fpsMs >= 3000) {
    Serial.printf("fps %.1f   estado %d\n", frames * 1000.0f / (millis() - fpsMs),
                  (int)app_state());
    frames = 0;
    fpsMs  = millis();
  }

  delay(2);
}
