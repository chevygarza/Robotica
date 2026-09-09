// ─────────────────────────────────────────────────────────────────────────────
// VNL-1 · Prueba del DFPlayer en terreno conocido — Arduino Mega 2560
//
// Sirve para una sola pregunta: ¿el modulo y la microSD funcionan?
// Si suena aqui, el problema esta en como sale la corriente de la perilla.
// Si no suena ni aqui, el sospechoso es el modulo o la tarjeta.
//
// Cableado (el mismo cable, sin desoldar nada):
//   rojo    -> 5V
//   negro   -> GND
//   blanco  -> pin 18 (TX1)   ya trae la resistencia de 1kOhm
//   amarillo-> pin 19 (RX1)
//
// No necesita que hagas nada: cambia de pista sola cada 10 segundos y va
// anunciando por el monitor que esta tocando. Los tonos se identifican de
// oido: numero de bips = pista, grave/medio/agudo = carpeta 1/2/3.
// ─────────────────────────────────────────────────────────────────────────────
#include <DFRobotDFPlayerMini.h>

DFRobotDFPlayerMini df;

uint8_t folder = 1, track = 1;
unsigned long lastChange = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println(F("\nVNL-1 · DFPlayer en Mega 2560"));
  Serial.println(F("TX1=18 (blanco, con 1k)   RX1=19 (amarillo)   9600 baudios"));

  Serial1.begin(9600);

  // Escucha cruda: al energizarse, el modulo manda solo una trama de estado
  // que abre en 0x7E y cierra en 0xEF. Si aparece, hay comunicacion real.
  Serial.println(F("escuchando la linea 2.5s antes de hablarle..."));
  unsigned long t0 = millis();
  byte raw[64]; byte n = 0;
  while (millis() - t0 < 2500) {
    while (Serial1.available() && n < sizeof(raw)) raw[n++] = Serial1.read();
  }
  if (n) {
    Serial.print(F("el modulo hablo solo: "));
    for (byte i = 0; i < n; i++) { Serial.print(raw[i], HEX); Serial.print(' '); }
    Serial.println();
    if (raw[0] == 0x7E) Serial.println(F("   trama valida: HAY COMUNICACION"));
  } else {
    Serial.println(F("silencio en la linea"));
  }

  bool ok = df.begin(Serial1, true, true);
  Serial.println(ok ? F("OK: el modulo responde con ACK")
                    : F("sin ACK: sigo a ciegas, los comandos se mandan igual"));
  if (ok) {
    Serial.print(F("archivos en la SD: "));
    Serial.println(df.readFileCounts());
  }

  df.volume(15);              // moderado: a todo volumen puede caer el riel
  delay(100);
  df.playFolder(folder, track);
  Serial.println(F("--- /01/001.mp3 ---"));
  lastChange = millis();
}

void loop() {
  if (millis() - lastChange > 10000) {
    lastChange = millis();
    track++;
    if (track > 3) { track = 1; folder = (folder % 3) + 1; }
    df.playFolder(folder, track);
    Serial.print(F("--- /0")); Serial.print(folder);
    Serial.print(F("/00")); Serial.print(track); Serial.println(F(".mp3 ---"));
  }

  if (df.available()) {
    uint8_t type = df.readType();
    int val = df.read();
    Serial.print(F("[modulo] tipo ")); Serial.print(type);
    Serial.print(F("  valor ")); Serial.println(val);
  }
}
