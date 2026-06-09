// Diagnóstico: GPIO43 FIJO en alto (IR siempre encendido)
void setup() { pinMode(43, OUTPUT); digitalWrite(43, HIGH); }
void loop() { digitalWrite(43, HIGH); delay(1000); }
