#include "mic.h"
#include <Wire.h>
#include <ESP_I2S.h>
#include "board.h"
#include "es8311.h"

// Mic integrado de la AMOLED-1.8: codec ES8311 (I2C 0x18) + I2S estandar.
// Secuencia de init tomada del demo oficial 15_ES8311 (sin reproduccion).

#define GROK_ENABLE_MIC 1

#if GROK_ENABLE_MIC
static const int      MIC_RATE   = 16000;
static const size_t   MIC_CHUNK  = 512;        // bytes: 128 frames estereo = 8 ms
static const float    MIC_GAIN   = 6.f;        // ganancia practica sobre el RMS normalizado
static const float    MIC_FLOOR  = 0.01f;      // ruido de fondo que se descuenta

static I2SClass g_i2s;
static bool g_micOk = false;
static volatile float g_energy = 0.f;

static bool codecInit() {
  es8311_handle_t h = es8311_create(0 /* Wire */, ES8311_ADDRRES_0);
  if (!h) return false;
  const es8311_clock_config_t clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = MIC_RATE * 256,
    .sample_frequency = MIC_RATE,
  };
  if (es8311_init(h, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return false;
  if (es8311_sample_frequency_config(h, clk.mclk_frequency, clk.sample_frequency) != ESP_OK) return false;
  if (es8311_microphone_config(h, false) != ESP_OK) return false;
  es8311_voice_volume_set(h, 0, NULL);                    // DAC en silencio
  es8311_microphone_gain_set(h, ES8311_MIC_GAIN_24DB);
  return true;
}

static void micTask(void *) {
  static uint8_t buf[MIC_CHUNK];
  static int16_t peakL = 0, peakR = 0;
  static uint32_t lastDiag = 0, reads = 0;
  for (;;) {
    size_t n = g_i2s.readBytes((char *)buf, sizeof(buf));
    if (n < 4) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
    reads++;
    const int16_t *s = (const int16_t *)buf;
    size_t count = n / 2;
    double acc = 0;
    for (size_t i = 0; i < count; i++) {
      double v = s[i]; acc += v * v;
      int16_t a = s[i] < 0 ? -s[i] : s[i];
      if (i & 1) { if (a > peakR) peakR = a; } else { if (a > peakL) peakL = a; }
    }
    if (millis() - lastDiag > 2000) {
      Serial.printf("[mic] diag reads=%lu bytes=%u peakL=%d peakR=%d s0=%d s1=%d\n",
                    (unsigned long)reads, (unsigned)n, peakL, peakR, s[0], s[1]);
      peakL = peakR = 0; lastDiag = millis();
    }
    float rms = sqrtf((float)(acc / (double)count)) / 32768.f;
    float e = (rms - MIC_FLOOR) * MIC_GAIN;
    e = e < 0.f ? 0.f : (e > 1.f ? 1.f : e);
    // ataque rapido, caida lenta: la boca abre al instante y cierra suave
    float cur = g_energy;
    g_energy = (e > cur) ? (cur * 0.4f + e * 0.6f) : (cur * 0.85f + e * 0.15f);
  }
}

static uint8_t rdReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xEE;
  if (Wire.requestFrom((int)addr, 1) != 1) return 0xEE;
  return Wire.read();
}

static void dumpDiag() {
  // AXP2101: 0x90 = LDO enable bits (ALDO1..4, BLDO1..2, CPUSLDO, DLDO1); 0x92 = ALDO1 V
  Serial.printf("[mic] AXP2101 id=0x%02X ldo_en(0x90)=0x%02X aldo1(0x92)=%u dcdc_en(0x80)=0x%02X\n",
                rdReg(AXP2101_ADDR, 0x03), rdReg(AXP2101_ADDR, 0x90),
                rdReg(AXP2101_ADDR, 0x92), rdReg(AXP2101_ADDR, 0x80));
  // ES8311: 0x00 reset/pwr, 0x01 clk, 0x14 sys/mic, 0x16 adc pga, 0x17 adc vol, 0x0A sdp out, 0xFD id
  Serial.printf("[mic] ES8311 id=0x%02X%02X r00=0x%02X r01=0x%02X r0A=0x%02X r14=0x%02X r16=0x%02X r17=0x%02X\n",
                rdReg(ES8311_ADDR, 0xFD), rdReg(ES8311_ADDR, 0xFE), rdReg(ES8311_ADDR, 0x00),
                rdReg(ES8311_ADDR, 0x01), rdReg(ES8311_ADDR, 0x0A), rdReg(ES8311_ADDR, 0x14),
                rdReg(ES8311_ADDR, 0x16), rdReg(ES8311_ADDR, 0x17));
}

bool micBegin() {
  pinMode(PIN_I2S_PA, OUTPUT);
  digitalWrite(PIN_I2S_PA, LOW);   // bocina apagada

  g_i2s.setPins(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT, PIN_I2S_DIN, PIN_I2S_MCLK);
  if (!g_i2s.begin(I2S_MODE_STD, MIC_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("[mic] I2S begin fallo");
    return false;
  }
  if (!codecInit()) {
    Serial.println("[mic] ES8311 no responde — cara solo con IMU");
    return false;
  }
  dumpDiag();
  xTaskCreatePinnedToCore(micTask, "mic", 4096, nullptr, 1, nullptr, 0);
  g_micOk = true;
  Serial.println("[mic] ES8311 + I2S listos (tarea en core 0)");
  return true;
}

float micEnergy() { return g_micOk ? g_energy : 0.f; }
bool micOk() { return g_micOk; }

#else
bool micBegin() { return false; }
float micEnergy() { return 0.f; }
bool micOk() { return false; }
#endif
