// Diagnóstico: parpadea GPIO43 (el pin del IR) cada 0.5s
void setup() {
  pinMode(43, OUTPUT);
}
void loop() {
  digitalWrite(43, HIGH);   // LED ON
  delay(500);
  digitalWrite(43, LOW);    // LED OFF
  delay(500);
}
