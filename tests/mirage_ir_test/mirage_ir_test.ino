#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Mirage.h>

const uint16_t kIrLedPin = 43;   // UART0 TXD del conector UART (pin libre)
IRMirageAc ac(kIrLedPin);

void setup() {
  ac.begin();
  ac.setPower(true);
  ac.setMode(kMirageAcCool);
  ac.setTemp(24);
  ac.setFan(kMirageAcFanAuto);
  ac.send();
}

void loop() {}
