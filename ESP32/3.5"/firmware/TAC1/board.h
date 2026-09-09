// TAC-1 — Mapa de hardware. Guition JC3248W535 (en el listado venia como
// "JC4832W535": buscar siempre por JC3248W535).
//
// ESP32-S3-WROOM-1 N16R8: 16MB flash quad, 8MB PSRAM octal. USB-C al USB nativo
// del S3 (no hay puente serie): en el Mac aparece como /dev/cu.usbmodem*.
#pragma once

// ── Pantalla: AXS15231B, 320x480 nativa (retrato), bus QSPI ─────────────────
// Sin pin de reset ni de DC: todo va por comandos QSPI. Pines validados contra
// la config de ESPHome y el dev-device de Arduino_GFX para esta placa.
#define PIN_LCD_CS       45   // strapping pin; en reposo queda en HIGH, no molesta
#define PIN_LCD_SCK      47
#define PIN_LCD_D0       21
#define PIN_LCD_D1       48
#define PIN_LCD_D2       40
#define PIN_LCD_D3       39
#define PIN_LCD_BL        1   // backlight por PWM
#define LCD_QSPI_HZ  32000000

// Dos cosas que ninguna ficha dice y que mandan sobre el driver:
//
// 1. NO ROTA POR HARDWARE. MADCTL con el bit MV no hace nada en QSPI (ESPHome
//    lo excluye a proposito y Arduino_GFX lo resuelve con un framebuffer). El
//    lienzo apaisado de 480x320 se gira por software al volcarlo al panel.
//
// 2. NO ACEPTA VENTANAS PARCIALES. En QSPI el controlador ignora RASET: cada
//    escritura empieza en la fila 0. El driver de Espressif ni siquiera lo manda.
//    Por eso se vuelca SIEMPRE el cuadro completo, de arriba a abajo.
#define PANEL_W         320
#define PANEL_H         480
#define SCR_W           480   // lienzo de la UI (apaisado)
#define SCR_H           320

// Hacia que lado se gira. 0 y 1 son los dos apaisados posibles; se elige el que
// deja el USB-C del lado que se quiere. Cambia solo este numero: pantalla y
// tactil se giran juntos.
#define ROT_FLIP          1

// ── Tactil: dentro del mismo AXS15231B, por I2C ──────────────────────────────
#define PIN_TP_SDA        4
#define PIN_TP_SCL        8
#define TP_ADDR        0x3B

// ── Pendientes de validar en placa (no se usan todavia) ─────────────────────
// TF card (SPI) y altavoz (I2S) existen; sus GPIO no estan en ninguna fuente
// confiable en texto. Cuando toquen, se sacan del esquematico o a prueba.
