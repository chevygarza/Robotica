// ─────────────────────────────────────────────────────────────────────────────
// VNL-1 · Barrido de volumen — hasta donde llega la bocina sin romperse
//
// Sube el volumen de 8 a 30 en pasos de 2, tocando el mismo tono en cada uno.
// Un tono puro delata el recorte del amplificador mucho antes que una cancion:
// cuando empieza a rasgar, ese es el techo util.
//
// Que escuchar:
//   · a que numero deja de crecer el volumen percibido (ahi satura el ampli)
//   · a que numero empieza a rasgar o vibrar (ahi esta el limite de la bocina)
//
// Que vigilar en el serial:
//   · si reaparece el encabezado "VNL-1 · barrido de volumen", la placa se
//     reinicio: el pico de corriente tumbo el riel. Eso es limite de la fuente
//     USB, no del modulo — se resuelve alimentando por power bank.
// ─────────────────────────────────────────────────────────────────────────────
#include <DFRobotDFPlayerMini.h>
#include "pins.h"

HardwareSerial      dfSerial(1);
DFRobotDFPlayerMini df;

#define VOL_MIN   8
#define VOL_MAX  30
#define PASO      2
#define HOLD_MS 4500

uint8_t vol = VOL_MIN;

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("VNL-1 · barrido de volumen");

  pinMode(PIN_PERIPH_5V_EN, OUTPUT);
  digitalWrite(PIN_PERIPH_5V_EN, HIGH);
  delay(300);

  dfSerial.begin(9600, SERIAL_8N1, PIN_DF_RX, PIN_DF_TX);
  delay(2000);

  // isACK = false A PROPOSITO. Con true, la libreria se queda en un bucle
  // infinito dentro de sendStack() esperando la confirmacion de cada comando
  // (while (_isSending) ...), y este clon confirma unos si y otros no: cuelga
  // el sketch en seco. Sin ACK los comandos se mandan y ya. Los avisos que
  // importan ("termino la pista") el modulo los manda solos de todas formas.
  df.begin(dfSerial, /*isACK=*/false, /*doReset=*/true);
  Serial.println("modulo iniciado (sin ACK, a proposito)");
  delay(500);

  df.volume(VOL_MIN);
  delay(60);
  df.playFolder(1, 1);              // arranca la musica una sola vez

  Serial.println("cada paso dura 4.5s. escucha donde empieza a rasgar.");
  Serial.println("-----------------------------------------------");
}

void loop() {
  // La musica NO se reinicia en cada paso: se deja correr y solo sube el
  // volumen. Comparar el mismo pasaje creciendo es como se juzga distorsion;
  // reiniciando siempre oirias la entrada, que suele ser la parte mas suave.
  df.volume(vol);

  Serial.printf("volumen %2d / 30", vol);
  if (vol >= 26) Serial.print("   <- zona de riesgo");
  Serial.println();

  delay(HOLD_MS);

  vol += PASO;
  if (vol > VOL_MAX) {
    vol = VOL_MIN;
    Serial.println("--- vuelvo a empezar ---");
  }

  // Si la cancion se acabo, arranca la siguiente y sigue el barrido.
  if (df.available() && df.readType() == DFPlayerPlayFinished) {
    static uint8_t t = 1;
    t = (t % 3) + 1;
    df.playFolder(1, t);
    Serial.printf("   (siguiente cancion: %03d.mp3)\n", t);
  }
}
