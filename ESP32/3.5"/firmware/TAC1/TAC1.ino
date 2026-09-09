// ─────────────────────────────────────────────────────────────────────────────
// Stuntech TAC-1 — TactOS
// Etapa 1: pantalla con DMA, tactil, wallpaper, red WiFi capturada en
//          pantalla, hora por NTP y reposo.
//
// Placa:  Guition JC3248W535 (ESP32-S3 N16R8, 3.5" 480x320, AXS15231B QSPI)
// Core:   esp32 3.3.11 en entorno aislado (tools/)   ·   LVGL 8.3.11
// Build:  tools/build.sh          Flash: tools/build.sh flash
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <lvgl.h>
#include "display.h"
#include "net.h"
#include "ui.h"
#include "ajustes.h"
#include "cloud.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nTAC-1  etapa 1");

  if (!display_begin()) {
    Serial.println("display_begin() fallo, me detengo");
    for (;;) delay(1000);
  }
  Serial.printf("PSRAM libre: %u KB   heap interno: %u KB\n",
                (unsigned)(ESP.getFreePsram() / 1024), (unsigned)(ESP.getFreeHeap() / 1024));

  ajustes_begin();      // antes de la UI: el brillo inicial sale de aqui
  ui_build();
  // La red arranca despues de la UI: la pantalla ya vive mientras el router
  // negocia, en vez de dejar el aparato en negro esperando.
  net_begin();
  cloud_begin();        // clima y mercados, en su task del core 0

  // Primer cuadro completo en el panel antes de encender la luz: el fade
  // revela la UI ya dibujada, no el barrido.
  lv_timer_handler();
  delay(40);
  display_backlight_fade(ui_brillo(), 600);
}

void loop() {
  net_tick();
  ui_tick();
  display_tick();
  lv_timer_handler();

  static uint32_t t = 0;
  if (millis() - t >= 3000) {
    Serial.printf("fps %.1f   heap %u KB   luz %d%%\n",
                  display_frames() * 1000.0f / (millis() - t),
                  (unsigned)(ESP.getFreeHeap() / 1024), display_backlight_level());
    t = millis();
  }
  delay(2);
}
