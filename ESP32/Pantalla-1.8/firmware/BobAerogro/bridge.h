#pragma once
#include <Arduino.h>

// Puente con la Mac por USB serial. Lineas de texto:
//   EV hello | working | done | notify | error | bye
//   SAY <texto>            (globo de texto ~5 s)
//   ROT 0|1|2|3|auto       (rotacion manual del canvas / volver a automatica)
//   PING                   -> responde PONG
enum class BridgeEvent : uint8_t { None = 0, Hello, Working, Done, Notify, Error, Bye, Say, Rot };
struct BridgeMsg { BridgeEvent ev = BridgeEvent::None; char text[64] = ""; };
BridgeMsg bridgePoll();
