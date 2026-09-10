#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "board.h"
#include "imu.h"

enum class Emotion : uint8_t {
  Idle = 0, Happy, Surprised, Look, Sleepy, Angry, Excited, Listening, COUNT
};

const char* emotionName(Emotion e);

struct FaceState {
  Emotion emotion = Emotion::Idle;
  float eyeOpen = 1.f;
  float eyeSize = 1.f;
  float pupilX = 0.f;
  float pupilY = 0.f;
  float brow = 0.f;
  float smile = 0.3f;
  float mouthOpen = 0.f;
  float bounce = 0.f;
  float breathe = 0.f;
  float glow = 0.7f;
  float squint = 0.f;
};

void faceBegin(Arduino_GFX* gfx);
void faceUpdate(const ImuSample& imu, float micEnergy, uint32_t nowMs);
void faceDraw();
Emotion faceEmotion();
