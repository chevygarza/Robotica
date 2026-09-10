#pragma once
#include <Arduino.h>

struct ImuSample {
  float ax, ay, az;   // g
  float mag;          // |a|
  float jerk;         // |a - a_prev|
  bool  ok;
};

bool imuBegin();
ImuSample imuRead();
