// VNL-1 — Mapa de hardware. CrowPanel 1.46" HMI ESP32-S3 Rotary Display.
// Fuente: codigo oficial de Elecrow (RotaryScreen_1_46.h) + validacion en placa.
#pragma once

// ── Encoder rotativo (EC11 con push integrado) ─────────────────────────────
#define PIN_ENC_A        45   // CLK
#define PIN_ENC_B        42   // DT
#define PIN_ENC_SW       41   // push, activo en LOW

// ── Pantalla: IPS redonda 360x360, ST77961, SPI 3-wire a 80MHz ─────────────
// El wiki dice GC9A01: esta mal. El panel es ST77961.
#define PIN_LCD_SCLK     10
#define PIN_LCD_MOSI     11
#define PIN_LCD_DC        3
#define PIN_LCD_CS        9
#define PIN_LCD_RST      14
#define PIN_LCD_BL       46   // backlight por PWM (ledc) -> fade de auto-apagado

// CRITICO y no documentado en el wiki. Verificado en el esquematico oficial
// (Eagle_SCH&PCB/ESP32 Display-1.46-V1.0.pdf, bloque POWER):
//
//   GPIO2 -> R23 10k -> Q7 (S9013) -> compuerta de Q4 (PMOS-3401-4A) -> OUT_5V
//
// GPIO2 NO es "corriente de la pantalla" como dice el comentario del ejemplo de
// Elecrow: es el interruptor del riel de 5V que alimenta el pin 3 de los
// conectores UART e I2C. Sin ponerlo en HIGH, cualquier periferico colgado de
// esos conectores se queda sin alimentacion — y el sintoma es cruel, porque el
// pin flota y da lecturas que parecen cable roto.
//
// Cualquier sketch que hable con el DFPlayer TIENE que encender esto, aunque no
// use la pantalla para nada.
#define PIN_PERIPH_5V_EN  2   // 5V de los conectores UART / I2C
#define PIN_LCD_PWR       1   // corriente del panel

// ── Touch capacitivo CST816T ───────────────────────────────────────────────
// Bus I2C 0 (Wire) remapeado a 6/7. El header de expansion tiene su PROPIO bus
// en 38/39 (TwoWire(1) en el ejemplo oficial): son dos buses distintos, no uno
// compartido.
#define PIN_TP_SDA        6
#define PIN_TP_SCL        7
#define PIN_TP_INT        5
#define PIN_TP_RST       13

// ── I2C libre del header de expansion (bus 1) ──────────────────────────────
#define PIN_HDR_SDA      38
#define PIN_HDR_SCL      39

// ── Anillo RGB ambiental ───────────────────────────────────────────────────
#define PIN_RGB_DIN      48
#define PIN_RGB_PWR      17   // hay que ponerlo en HIGH o la tira no enciende
#define NUM_LEDS          8

#define PIN_POWER_LED    40
#define PIN_BULB_LED     43   // LED "bulb" con PWM propio en el demo de fabrica

// ── Bateria ────────────────────────────────────────────────────────────────
// IO4 quedo libre cuando el DFPlayer se fue al conector UART. Tiene convertidor
// analogico, asi que ahi entra el divisor que mide la celda.
#define PIN_BAT_ADC       4

// ── DFPlayer Mini ──────────────────────────────────────────────────────────
// Va en el conector UART de 4 hilos (RX·TX·5V·GND), que expone GPIO43/44.
//
// Esto NO se pelea con el monitor serial: con USB CDC On Boot, `Serial` es el
// USB nativo del S3, no UART0. Los pines 43/44 quedan libres y aqui se usa
// UART1 ruteado a ellos por la matriz de GPIO. Por eso no hace falta el cable
// FPC del header de expansion.
//
// Unico efecto secundario: el bootloader de ROM escupe sus mensajes de arranque
// por GPIO43 a 115200. El DFPlayer los ve como ruido a 9600 y los ignora — no
// forman una trama valida (las suyas abren en 0x7E y cierran en 0xEF).
#define PIN_DF_TX        43   // ESP32 TX -> [1k] -> RX del DFPlayer
#define PIN_DF_RX        44   // ESP32 RX <-------- TX del DFPlayer
