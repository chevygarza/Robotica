#include "mic.h"
#include <Wire.h>
#include "board.h"
#include <driver/i2s.h>

// TODO mic energy: ruta I2S + ES7210 ligera. Si la placa no responde,
// micOk()=false y el face sigue solo con IMU.

#define GROK_ENABLE_MIC 0  // off hasta que display funcione

#if GROK_ENABLE_MIC
static bool g_micOk = false;
static float g_energy = 0.f;

static bool axpEnableMicRail() {
  // AXP2101: encender ALDO1 @ 3.3V (mics), sin apagar el resto a lo bestia.
  auto wr = [](uint8_t reg, uint8_t val) -> bool {
    Wire.beginTransmission(AXP2101_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
  };
  auto rd = [](uint8_t reg, uint8_t &out) -> bool {
    Wire.beginTransmission(AXP2101_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)AXP2101_ADDR, 1) != 1) return false;
    out = Wire.read();
    return true;
  };

  uint8_t chip = 0;
  if (!rd(0x03, chip)) {
    Serial.println("[mic] AXP2101 no responde — sigo sin rail dedicado");
    return false;
  }
  // ALDO1 voltage 3.3V: reg 0x92 = (3300-500)/100 = 28
  wr(0x92, 28);
  uint8_t ldo = 0;
  rd(0x90, ldo);
  wr(0x90, (uint8_t)(ldo | 0x01)); // bit0 ALDO1
  Serial.printf("[mic] AXP2101 chip=0x%02X ALDO1 on (0x90=0x%02X)\n", chip, ldo | 1);
  return true;
}

static bool es7210Write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES7210_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool es7210InitLite() {
  // Secuencia minima inspirada en demos ESP-BOX / Waveshare (slave I2S, 16k).
  // No es un driver completo — si no hay energia util, el face ignora el mic.
  if (!es7210Write(0x00, 0xFF)) return false; // soft reset
  delay(10);
  es7210Write(0x00, 0x41); // take out of reset, enable
  es7210Write(0x01, 0x1F); // clock on
  es7210Write(0x02, 0x01); // master clock divider-ish
  es7210Write(0x07, 0x20); // SDP
  es7210Write(0x08, 0x10);
  es7210Write(0x09, 0x30); // ADC1/2 power
  es7210Write(0x0A, 0x30);
  es7210Write(0x0B, 0x00);
  es7210Write(0x11, 0x60);
  es7210Write(0x12, 0x02);
  es7210Write(0x22, 0x00);
  es7210Write(0x40, 0xC3); // ADC1
  es7210Write(0x41, 0x70);
  es7210Write(0x42, 0xC3); // ADC2
  es7210Write(0x43, 0x70);
  delay(20);
  Serial.println("[mic] ES7210 init lite ok");
  return true;
}

bool micBegin() {
  axpEnableMicRail();
  delay(30);
  if (!es7210InitLite()) {
    Serial.println("[mic] ES7210 no responde — TODO mic energy deshabilitado");
    g_micOk = false;
    return false;
  }

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("[mic] i2s_driver_install fail");
    g_micOk = false;
    return false;
  }

  i2s_pin_config_t pins = {};
  pins.mck_io_num   = PIN_I2S_MCLK;
  pins.bck_io_num   = PIN_I2S_BCLK;
  pins.ws_io_num    = PIN_I2S_LRCK;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num  = PIN_I2S_DIN;

  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    Serial.println("[mic] i2s_set_pin fail");
    i2s_driver_uninstall(I2S_NUM_0);
    g_micOk = false;
    return false;
  }

  // PA off — no queremos altavoz para energy
  pinMode(PIN_I2S_PA, OUTPUT);
  digitalWrite(PIN_I2S_PA, LOW);

  g_micOk = true;
  Serial.println("[mic] I2S RX listo (energy RMS)");
  return true;
}

float micEnergy() {
  if (!g_micOk) return 0.f;
  int16_t buf[256];
  size_t n = 0;
  if (i2s_read(I2S_NUM_0, buf, sizeof(buf), &n, 0) != ESP_OK || n < 4) {
    return g_energy * 0.9f;
  }
  size_t samples = n / sizeof(int16_t);
  double acc = 0;
  for (size_t i = 0; i < samples; i++) {
    double v = (double)buf[i];
    acc += v * v;
  }
  float rms = sqrtf((float)(acc / (double)samples)) / 32768.f;
  // Suavizado + ganancia practica
  float e = constrain(rms * 8.f, 0.f, 1.f);
  g_energy = g_energy * 0.7f + e * 0.3f;
  return g_energy;
}

bool micOk() { return g_micOk; }

#else
bool micBegin() { return false; }
float micEnergy() { return 0.f; }
bool micOk() { return false; }
#endif
