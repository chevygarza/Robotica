// VNL-1 · ¿Puede el DFPlayer decirnos que hay en la tarjeta?
//
// Si contesta cuantas carpetas hay y cuantas pistas tiene cada una, el firmware
// puede armar la biblioteca solo y albums.h deja de existir. Eso significaria
// que cambiar musica = tocar la SD, sin recompilar ni reflashear nada.
#include <DFRobotDFPlayerMini.h>
#include "pins.h"

HardwareSerial      dfSerial(1);
DFRobotDFPlayerMini df;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nVNL-1 · consulta de estructura al DFPlayer");

  pinMode(PIN_PERIPH_5V_EN, OUTPUT);
  digitalWrite(PIN_PERIPH_5V_EN, HIGH);
  delay(300);

  dfSerial.begin(9600, SERIAL_8N1, PIN_DF_RX, PIN_DF_TX);
  delay(2000);

  df.begin(dfSerial, /*isACK=*/false, /*doReset=*/true);
  delay(1500);
  Serial.println("modulo iniciado (sin ACK)\n");

  Serial.printf("archivos totales .......... %d\n", df.readFileCounts());
  delay(200);
  Serial.printf("carpetas .................. %d\n", df.readFolderCounts());
  delay(200);

  for (uint8_t f = 1; f <= 6; f++) {
    int n = df.readFileCountsInFolder(f);
    Serial.printf("  carpeta %02d .............. %d pista(s)\n", f, n);
    delay(200);
  }

  Serial.println("\nSi los numeros cuadran con la tarjeta, la biblioteca");
  Serial.println("se puede armar sola y albums.h sale sobrando.");
}

void loop() { delay(1000); }
