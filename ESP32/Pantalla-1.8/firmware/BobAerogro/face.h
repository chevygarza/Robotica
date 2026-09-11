#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "board.h"
#include "imu.h"

// Bob Aerogro: un blob plano de un solo color con dos ojos blancos tipo pildora.
// Cada animo tiene su forma (funcion polar r(theta)) y su color; se morfea entre ellos.
enum class Emotion : uint8_t { Idle = 0, Listening, Talking, Surprised, Angry, Sleepy, COUNT };

const char* emotionName(Emotion e);

void faceBegin(Arduino_GFX* gfx);
void faceUpdate(const ImuSample& imu, float micEnergy, uint32_t nowMs);
void faceDraw();
Emotion faceEmotion();
