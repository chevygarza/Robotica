#include "imu.h"
#include <Wire.h>
#include "board.h"

// QMI8658 — lecturas minimas por registros (sin SensorLib).
static const uint8_t REG_WHOAMI = 0x00;
static const uint8_t REG_CTRL1  = 0x02;
static const uint8_t REG_CTRL2  = 0x03; // accel
static const uint8_t REG_CTRL7  = 0x08;
static const uint8_t REG_AX_L   = 0x35;

static uint8_t g_addr = QMI8658_ADDR;
static float g_prevAx = 0, g_prevAy = 0, g_prevAz = 1.0f;
static bool g_ok = false;

static bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(g_addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool readRegs(uint8_t reg, uint8_t *buf, size_t n) {
  Wire.beginTransmission(g_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)g_addr, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

bool imuBegin() {
  // Probar 0x6B y 0x6A
  const uint8_t addrs[] = {0x6B, 0x6A};
  for (uint8_t a : addrs) {
    g_addr = a;
    uint8_t who = 0;
    if (!readRegs(REG_WHOAMI, &who, 1)) continue;
    // QMI8658 WHO_AM_I suele ser 0x05 (a veces 0x7C en clones)
    if (who == 0x05 || who == 0x7C || who != 0x00) {
      // Accel ±4g, ODR ~250Hz (CTRL2: aODR/aFS tipicos)
      writeReg(REG_CTRL1, 0x40);       // address auto increment / little endian-ish
      writeReg(REG_CTRL2, 0x05);       // aFS=±2g, aODR ~250Hz
      writeReg(REG_CTRL7, 0x01);       // enable accelerometer
      delay(20);
      g_ok = true;
      Serial.printf("[imu] QMI8658 ok addr=0x%02X who=0x%02X\n", g_addr, who);
      return true;
    }
  }
  Serial.println("[imu] QMI8658 no encontrado — emociones usaran idle");
  g_ok = false;
  return false;
}

ImuSample imuRead() {
  ImuSample s{};
  s.ok = g_ok;
  if (!g_ok) {
    s.ax = 0; s.ay = 0; s.az = 1; s.mag = 1; s.jerk = 0;
    return s;
  }
  uint8_t raw[6];
  if (!readRegs(REG_AX_L, raw, 6)) {
    s.ok = false;
    return s;
  }
  auto s16 = [](uint8_t lo, uint8_t hi) -> int16_t {
    return (int16_t)((uint16_t)hi << 8 | lo);
  };
  // CTRL2=0x05 => aFS=±2g (bits 6:4 = 0): 2/32768 g/LSB. Con 4/32768 leia 2 g en reposo.
  const float scale = 2.0f / 32768.0f;
  s.ax = s16(raw[0], raw[1]) * scale;
  s.ay = s16(raw[2], raw[3]) * scale;
  s.az = s16(raw[4], raw[5]) * scale;
  s.mag = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
  float dx = s.ax - g_prevAx, dy = s.ay - g_prevAy, dz = s.az - g_prevAz;
  s.jerk = sqrtf(dx * dx + dy * dy + dz * dz);
  g_prevAx = s.ax; g_prevAy = s.ay; g_prevAz = s.az;
  return s;
}
