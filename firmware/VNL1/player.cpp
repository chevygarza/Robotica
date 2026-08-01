#include "player.h"
#include "pins.h"
#include <DFRobotDFPlayerMini.h>

static HardwareSerial       dfSerial(1);
static DFRobotDFPlayerMini  df;
static bool                 ready = false;

// ── Cola de comandos ─────────────────────────────────────────────────────────
enum CmdKind : uint8_t { CMD_NONE = 0, CMD_PLAY_FOLDER, CMD_PAUSE, CMD_RESUME,
                         CMD_STOP, CMD_VOLUME };

struct Cmd { CmdKind kind; uint8_t a, b; };

#define QLEN 12
static Cmd      queue[QLEN];
static uint8_t  qHead = 0, qTail = 0;
static uint32_t lastSentMs = 0;

static void enqueue(CmdKind k, uint8_t a = 0, uint8_t b = 0) {
  // El volumen se colapsa: si ya hay uno pendiente, se sobreescribe en vez de
  // encolar otro. Es lo que evita que 12 muescas de perilla = 12 comandos.
  if (k == CMD_VOLUME) {
    for (uint8_t i = qTail; i != qHead; i = (i + 1) % QLEN) {
      if (queue[i].kind == CMD_VOLUME) { queue[i].a = a; return; }
    }
  }
  uint8_t next = (qHead + 1) % QLEN;
  if (next == qTail) return;              // cola llena: se descarta
  queue[qHead] = { k, a, b };
  qHead = next;
}

// ── Estado de reproduccion ───────────────────────────────────────────────────
static uint8_t  order[32];                // orden barajado de pistas
static uint8_t  orderCount = 0;
static uint8_t  orderPos   = 0;
static uint8_t  curFolder  = 0;
static uint8_t  volume     = 20;
static bool     playing    = false;
static uint32_t trackStart = 0;
static uint32_t pausedAt   = 0;

static void shuffle(uint8_t tracks) {
  if (tracks > sizeof(order)) tracks = sizeof(order);
  orderCount = tracks;
  for (uint8_t i = 0; i < tracks; i++) order[i] = i + 1;
  // Fisher-Yates con el generador de hardware del ESP32.
  for (uint8_t i = tracks - 1; i > 0; i--) {
    uint8_t j = esp_random() % (i + 1);
    uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
  }
  orderPos = 0;
}

bool player_begin() {
  dfSerial.begin(9600, SERIAL_8N1, PIN_DF_RX, PIN_DF_TX);
  delay(100);

  // isACK = false NO ES NEGOCIABLE con este modulo. Verificado en placa el
  // 2026-07-31: con isACK=true la libreria se queda dentro de sendStack() en
  //     while (_isSending) { delay(0); available(); }
  // esperando la confirmacion de cada comando, y este clon MH2024K confirma
  // unos si y otros no. El sketch se cuelga en seco, sin timeout que lo salve.
  //
  // No perdemos nada que importe: los avisos utiles ("termino la pista", que es
  // lo que dispara el avance del album) el modulo los manda por su cuenta, sin
  // que nadie se los pida, y se leen igual en player_tick().
  df.begin(dfSerial, /*isACK=*/false, /*doReset=*/true);
  delay(500);
  ready = true;

  // Sin ACK no hay forma de preguntarle nada, asi que tampoco tiene caso pedir
  // readFileCounts(): devolveria -1 y solo confundiria al depurar.
  df.volume(volume);
  delay(PLAYER_MIN_GAP_MS);
  df.EQ(DFPLAYER_EQ_NORMAL);
  delay(PLAYER_MIN_GAP_MS);
  df.outputDevice(DFPLAYER_DEVICE_SD);
  lastSentMs = millis();
  Serial.println("DFPlayer: iniciado (sin ACK, a proposito)");
  return true;
}

bool player_available() { return ready; }

static void sendNext() {
  if (qTail == qHead) return;
  Cmd c = queue[qTail];
  qTail = (qTail + 1) % QLEN;

  switch (c.kind) {
    case CMD_PLAY_FOLDER: df.playFolder(c.a, c.b); break;
    case CMD_PAUSE:       df.pause();              break;
    case CMD_RESUME:      df.start();              break;
    case CMD_STOP:        df.stop();               break;
    case CMD_VOLUME:      df.volume(c.a);          break;
    default: break;
  }
  lastSentMs = millis();
}

static void playCurrent() {
  if (!orderCount) return;
  enqueue(CMD_PLAY_FOLDER, curFolder, order[orderPos]);
  trackStart = millis();
  playing = true;
}

void player_tick() {
  if (!ready) return;

  if (millis() - lastSentMs >= PLAYER_MIN_GAP_MS) sendNext();

  // El modulo avisa cuando termina una pista: ahi avanzamos en el barajado.
  if (df.available()) {
    uint8_t type = df.readType();
    if (type == DFPlayerPlayFinished) {
      player_next();
    } else if (type == DFPlayerError) {
      Serial.printf("DFPlayer error: %d\n", df.read());
    }
  }
}

void player_play_album(uint8_t folder, uint8_t tracks) {
  if (!ready) return;
  curFolder = folder;
  shuffle(tracks);
  playCurrent();
}

void player_pause() {
  if (!ready || !playing) return;
  enqueue(CMD_PAUSE);
  pausedAt = millis();
  playing = false;
}

void player_resume() {
  if (!ready || playing) return;
  enqueue(CMD_RESUME);
  // Se corre el arranque para que el tiempo transcurrido no cuente la pausa.
  if (pausedAt) trackStart += millis() - pausedAt;
  pausedAt = 0;
  playing = true;
}

void player_stop() {
  if (!ready) return;
  enqueue(CMD_STOP);
  playing = false;
  orderCount = 0;
}

bool player_playing() { return playing; }

void player_next() {
  if (!ready || !orderCount) return;
  orderPos = (orderPos + 1) % orderCount;
  // Al dar la vuelta se rebaraja: dos pasadas seguidas no repiten el orden.
  if (orderPos == 0) shuffle(orderCount);
  playCurrent();
}

void player_set_volume(uint8_t vol) {
  if (vol > PLAYER_VOL_MAX) vol = PLAYER_VOL_MAX;
  volume = vol;
  if (ready) enqueue(CMD_VOLUME, vol);
}

uint8_t player_volume()      { return volume; }
uint8_t player_track_index() { return orderCount ? orderPos + 1 : 0; }
uint8_t player_track_count() { return orderCount; }

uint32_t player_elapsed_s() {
  if (!orderCount) return 0;
  uint32_t ref = playing ? millis() : (pausedAt ? pausedAt : millis());
  return (ref - trackStart) / 1000;
}
