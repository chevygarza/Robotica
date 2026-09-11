#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "board.h"
#include "imu.h"

// Bob Aerogro: un blob plano de un solo color con dos ojos blancos tipo pildora.
// Cada animo tiene su forma (funcion polar r(theta)) y su color; se morfea entre ellos.
enum class Emotion : uint8_t { Idle = 0, Listening, Talking, Surprised, Angry, Sleepy, COUNT };

const char* emotionName(Emotion e);

// Reacciones cortas a cuidados del tamagotchi (no cambian el animo/color)
enum class FaceAction : uint8_t { None = 0, Caress, Eat, Tickle };

void faceBegin(Arduino_GFX* gfx);
void faceSetVitality(float v);          // 0..1: color vivo -> apagado/gris
void faceSetScale(float s);             // tamano del cuerpo (edad)
void faceAction(FaceAction a);          // dispara un gesto de ~1 s
void faceForce(Emotion e, uint32_t ms); // fuerza un animo (ej. dormido boca abajo)

// Actos que se pueden pedir desde fuera (puente con la Mac)
enum class FaceMove : uint8_t { Hop, Dance, Wink, Curious, Peekaboo };
void faceDo(FaceMove m);
void faceSay(const char* text, uint32_t ms);   // globo de texto sobre Bob
void faceFocus(uint32_t ms);                    // "concentrado": azul, sin travesuras
void faceUpdate(const ImuSample& imu, float micEnergy, uint32_t nowMs);
void faceDraw();
Emotion faceEmotion();
