// VNL-1 · Barrido de pines: encuentra a que GPIO esta conectado el cable.
// Baja un pin candidato a 0V durante 4s, lo suelta 2s, y pasa al siguiente.
// Mide un cable contra el negro: cuando caiga a 0V, el serial te dice cual es.
const int CANDIDATOS[] = { 43, 44, 38, 39, 4, 12 };
const int N = sizeof(CANDIDATOS) / sizeof(CANDIDATOS[0]);

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("VNL-1 · barrido de pines del conector");
  Serial.println("mide un cable contra el negro y espera a que caiga a 0V");
  Serial.println("-----------------------------------------------------");
}

void loop() {
  for (int i = 0; i < N; i++) {
    int p = CANDIDATOS[i];
    Serial.printf(">>> bajando GPIO %d  (4 segundos)\n", p);
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
    delay(4000);
    pinMode(p, INPUT);          // se suelta para no estorbar al siguiente
    Serial.println("    ... suelto");
    delay(2000);
  }
  Serial.println("--- vuelta completa, repito ---");
}
