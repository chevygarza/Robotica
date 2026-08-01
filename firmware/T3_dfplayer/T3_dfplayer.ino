// ─────────────────────────────────────────────────────────────────────────────
// VNL-1 · Etapa 3 aislada — prueba del DFPlayer Mini
//
// Sin pantalla, sin LVGL: solo la perilla, el serial y el modulo de audio. Si
// algo falla aqui, el problema es cableado / SD / modulo, y no la UI.
//
// Cableado (cable UART de 4 hilos de la CrowPanel):
//   rojo   5V   ──────────────► VCC
//   negro  GND  ──────────────► GND
//   blanco TX   ──[1kΩ]───────► RX del DFPlayer
//   amarillo RX ◄────────────── TX del DFPlayer
//   bocina 4Ω entre SPK1 y SPK2
//
// Con la perilla:
//   girar  -> volumen
//   push   -> siguiente pista de la carpeta actual
//   doble  -> cambia de carpeta (01 -> 02 -> 03)
//   3 seg  -> detener
//
// Las pistas de prueba se anuncian solas: numero de bips = pista,
// tono grave/medio/agudo = carpeta 1/2/3.
// ─────────────────────────────────────────────────────────────────────────────
#include <DFRobotDFPlayerMini.h>
#include "pins.h"
#include "knob.h"

HardwareSerial       dfSerial(1);
DFRobotDFPlayerMini  df;

static bool    ready   = false;
static bool    ackMode = true;   // false = clon que no confirma comandos
static uint8_t folder  = 1;
static uint8_t track   = 1;
static uint8_t vol     = 18;

static const char* errText(uint8_t v) {
  switch (v) {
    case Busy:            return "modulo ocupado / sin SD";
    case Sleeping:        return "dormido";
    case SerialWrongStack: return "trama serial mal formada";
    case CheckSumNotMatch: return "checksum no coincide (ruido en la linea)";
    case FileIndexOut:    return "indice de archivo fuera de rango";
    case FileMismatch:    return "no encuentra el archivo";
    case Advertise:       return "en modo anuncio";
    default:              return "desconocido";
  }
}

static void dumpStatus() {
  Serial.printf("   volumen %d   carpeta %02d   pista %03d\n", vol, folder, track);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("VNL-1 · prueba aislada del DFPlayer");
  Serial.printf("UART1  TX=%d (blanco)  RX=%d (amarillo)  9600 baudios\n",
                PIN_DF_TX, PIN_DF_RX);

  knob::begin();

  // LO PRIMERO Y LO MAS IMPORTANTE: encender el riel de 5V de los conectores.
  // El pin 3 del conector UART cuelga de un PMOS (Q4) gobernado por GPIO2. Si
  // esto no se enciende, el modulo no recibe alimentacion, no arranca, no
  // contesta — y el pin de 5V flota dando lecturas que parecen cable roto.
  pinMode(PIN_PERIPH_5V_EN, OUTPUT);
  digitalWrite(PIN_PERIPH_5V_EN, HIGH);
  Serial.println("riel de 5V de los conectores: ENCENDIDO (GPIO2)");
  delay(300);   // que el riel suba antes de hablarle al modulo

  dfSerial.begin(9600, SERIAL_8N1, PIN_DF_RX, PIN_DF_TX);

  // Los clones MH2024K tardan en despertar tras energizarse. Con 100ms el
  // begin() falla aunque todo este bien conectado.
  Serial.println("esperando a que el modulo arranque...");

  // Escucha cruda antes de hablarle. Al energizarse, el DFPlayer manda solo una
  // trama de estado (abre en 0x7E, cierra en 0xEF). Si aparece aqui, la linea
  // TX del modulo -> RX de la placa esta bien y el modulo vive. Es la unica
  // prueba de comunicacion que NO depende de que haya microSD.
  uint32_t t0 = millis();
  uint8_t  raw[64];
  uint8_t  n = 0;
  while (millis() - t0 < 2500) {
    while (dfSerial.available() && n < sizeof(raw)) raw[n++] = dfSerial.read();
    delay(5);
  }
  if (n) {
    Serial.printf("el modulo hablo solo: %d bytes ->", n);
    for (uint8_t i = 0; i < n; i++) Serial.printf(" %02X", raw[i]);
    Serial.println();
    if (raw[0] == 0x7E) Serial.println("   trama valida: HAY COMUNICACION");
  } else {
    Serial.println("silencio en la linea (normal en algunos clones, seguimos)");
  }

  // Primer intento: protocolo completo, con confirmacion de cada comando.
  if (df.begin(dfSerial, /*isACK=*/true, /*doReset=*/true)) {
    ready = true; ackMode = true;
    Serial.println("OK: responde con ACK (protocolo completo)");
  } else {
    // Segundo intento: sin ACK y sin reset. Es como hablan muchos clones.
    delay(800);
    if (df.begin(dfSerial, /*isACK=*/false, /*doReset=*/false)) {
      ready = true; ackMode = false;
      Serial.println("OK: responde SIN ACK (modo clon, a ciegas)");
    }
  }

  if (!ready) {
    Serial.println();
    Serial.println("NO RESPONDE. En orden de probabilidad:");
    Serial.println("  1. TX y RX cruzados  -> blanco va al RX del modulo,");
    Serial.println("     amarillo al TX. Si los tienes derechos, no hay dialogo.");
    Serial.println("  2. La microSD no esta en FAT32, o trae metadata de macOS.");
    Serial.println("  3. VCC flojo: el modulo necesita 5V firmes.");
    Serial.println("  4. Falta el GND comun entre placa y modulo.");
    Serial.println("Aun asi sigo: los comandos se mandan a ciegas por si acaso.");
  } else {
    int files = df.readFileCounts();
    Serial.printf("archivos que ve en la SD: %d%s\n", files,
                  ackMode ? "" : "  (sin ACK este dato no es confiable)");
  }

  df.volume(vol);
  delay(100);
  df.playFolder(folder, track);
  Serial.println("--- reproduciendo /01/001.mp3 ---");
  Serial.println("girar=volumen  push=siguiente  doble=carpeta  3s=parar");
  dumpStatus();
}

void loop() {
  knob::update();

  KnobEvent e;
  while ((e = knob::read()) != KNOB_NONE) {
    switch (e) {
      case KNOB_CW:
      case KNOB_CCW: {
        int v = vol + (e == KNOB_CW ? 1 : -1);
        vol = (uint8_t)constrain(v, 0, 30);
        df.volume(vol);
        Serial.printf("volumen %d\n", vol);
        break;
      }
      case KNOB_PRESS:
        track = (track % 3) + 1;          // 3 pistas de prueba por carpeta
        df.playFolder(folder, track);
        Serial.printf("-> /%02d/%03d.mp3\n", folder, track);
        dumpStatus();
        break;

      case KNOB_DOUBLE_PRESS:
        folder = (folder % 3) + 1;
        track  = 1;
        df.playFolder(folder, track);
        Serial.printf("-> carpeta %02d, pista 001\n", folder);
        dumpStatus();
        break;

      case KNOB_LONG_PRESS:
        df.stop();
        Serial.println("detenido");
        break;

      default: break;
    }
    delay(60);   // throttling: el modulo se satura si se le habla de golpe
  }

  // Todo lo que el modulo tenga que decir, se imprime.
  if (df.available()) {
    uint8_t type = df.readType();
    int     val  = df.read();
    switch (type) {
      case DFPlayerPlayFinished:
        Serial.printf("[modulo] termino la pista %d\n", val);
        break;
      case DFPlayerError:
        Serial.printf("[modulo] ERROR: %s (%d)\n", errText(val), val);
        break;
      case DFPlayerCardInserted: Serial.println("[modulo] SD insertada"); break;
      case DFPlayerCardRemoved:  Serial.println("[modulo] SD retirada");  break;
      case DFPlayerCardOnline:   Serial.println("[modulo] SD lista");     break;
      case DFPlayerUSBInserted:  Serial.println("[modulo] USB insertado");break;
      default:
        Serial.printf("[modulo] mensaje tipo %d, valor %d\n", type, val);
        break;
    }
  }

  delay(4);
}
