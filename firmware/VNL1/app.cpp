#include "app.h"
#include "pins.h"
#include "albums.h"
#include "vinyl.h"
#include "player.h"
#include <Adafruit_NeoPixel.h>

#define MAX_DOTS       16
#define DOT_R         160    // radio donde viven los puntos de pista
#define VOL_HOLD_MS  1400    // cuanto se queda el overlay de volumen
#define VOL_STEP        1
#define HOLD_SHOW_MS  140    // a partir de aqui se ve el aro de "mantener"

static Adafruit_NeoPixel ring(NUM_LEDS, PIN_RGB_DIN, NEO_GRB + NEO_KHZ800);

static AppState st      = ST_SELECTOR;
static uint8_t  album   = 0;
static int8_t   loaded  = -1;      // album cargado en el DFPlayer
static uint8_t  vol     = 20;
static uint8_t  trackIx = 0;       // 1-based; 0 = nada cargado
static bool     paused  = false;

static uint32_t volShownMs = 0;
static uint32_t trackStart = 0;    // reloj propio del tiempo transcurrido
static uint32_t pausedAt   = 0;
static uint32_t lastSecond = 0;
static bool     holdShown  = false;

// ── Widgets ──────────────────────────────────────────────────────────────────
static lv_obj_t* selBox   = nullptr;   // info del disco mientras hojeas
static lv_obj_t* selMeta  = nullptr;
static lv_obj_t* playBox  = nullptr;
static lv_obj_t* dots[MAX_DOTS] = { nullptr };
static lv_obj_t* timeLbl  = nullptr;
static lv_obj_t* volBox   = nullptr;
static lv_obj_t* volArc   = nullptr;
static lv_obj_t* volLbl   = nullptr;
static lv_obj_t* holdArc  = nullptr;   // progreso del mantener

// Un solo helper para todas las transiciones: nada de cortes duros.
static void fadeTo(lv_obj_t* o, lv_opa_t to, uint16_t ms) {
  if (!o) return;
  if (to > 0) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);

  lv_anim_del(o, nullptr);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_values(&a, lv_obj_get_style_opa(o, 0), to);
  lv_anim_set_time(&a, ms);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
  });
  if (to == 0) {
    // Al llegar a 0 se oculta de verdad: un objeto invisible sigue costando
    // redibujo en cada invalidacion.
    lv_anim_set_ready_cb(&a, [](lv_anim_t* an) {
      lv_obj_add_flag((lv_obj_t*)an->var, LV_OBJ_FLAG_HIDDEN);
    });
  }
  lv_anim_start(&a);
}

static lv_obj_t* mkLabel(lv_obj_t* parent, const lv_font_t* font, lv_opa_t opa,
                         lv_coord_t y) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_white(), 0);
  lv_obj_set_style_text_opa(l, opa, 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(l, 300);
  lv_obj_set_pos(l, 30, y);
  return l;
}

static lv_obj_t* mkBox(lv_obj_t* parent) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, 360, 360);
  lv_obj_set_pos(b, 0, 0);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
  return b;
}

static lv_obj_t* mkRimArc(lv_obj_t* parent, lv_coord_t size, lv_coord_t width,
                          lv_opa_t bgOpa) {
  lv_obj_t* a = lv_arc_create(parent);
  lv_obj_set_size(a, size, size);
  lv_obj_center(a);
  lv_obj_remove_style(a, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_color(a, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(a, bgOpa, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
  lv_obj_set_style_arc_color(a, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
  lv_arc_set_rotation(a, 270);          // el cero arriba, no a las 3
  lv_arc_set_bg_angles(a, 0, 360);
  return a;
}

// ── Contenido ────────────────────────────────────────────────────────────────
static void fmtTime(char* out, size_t n, uint32_t secs) {
  snprintf(out, n, "%lu:%02lu", (unsigned long)(secs / 60),
           (unsigned long)(secs % 60));
}

// La info del disco vive en el selector, no en una pantalla aparte: se lee
// mientras hojeas y no cuesta un push extra.
static void refreshSelMeta() {
  const Album& a = ALBUMS[album];
  char t[48];
  if (a.tracks == 0) {
    snprintf(t, sizeof(t), "vacio");
  } else {
    snprintf(t, sizeof(t), "%u %s - %u min", a.tracks,
             a.tracks == 1 ? "cancion" : "canciones",
             (unsigned)((a.seconds + 30) / 60));
  }
  lv_label_set_text(selMeta, t);
}

// Los puntos se colocan una vez por album: uno por cancion, arrancando arriba.
static void layoutDots(uint8_t n) {
  if (n > MAX_DOTS) n = MAX_DOTS;
  for (uint8_t i = 0; i < MAX_DOTS; i++) {
    if (!dots[i]) continue;
    if (i >= n) { lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN); continue; }
    lv_obj_clear_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
    float ang = (-90.0f + 360.0f * i / n) * PI / 180.0f;
    lv_obj_set_pos(dots[i], (lv_coord_t)(180 + DOT_R * cosf(ang)) - 3,
                            (lv_coord_t)(180 + DOT_R * sinf(ang)) - 3);
  }
}

static void refreshDots() {
  uint8_t n = ALBUMS[album].tracks;
  for (uint8_t i = 0; i < MAX_DOTS && i < n; i++) {
    if (!dots[i]) continue;
    bool played  = (trackIx > 0 && i < trackIx - 1);
    bool current = (trackIx > 0 && i == trackIx - 1);
    lv_obj_set_style_bg_color(dots[i],
        current ? lv_color_hex(ALBUMS[album].color) : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dots[i], current ? LV_OPA_COVER
                                             : (played ? 120 : 40), 0);
    lv_obj_set_size(dots[i], current ? 8 : 6, current ? 8 : 6);
  }
}

// ── Transiciones ─────────────────────────────────────────────────────────────
static uint32_t elapsedS() {
  if (!trackIx) return 0;
  uint32_t ref = paused ? pausedAt : millis();
  return (ref - trackStart) / 1000;
}

static void startAlbum() {
  loaded  = (int8_t)album;
  trackIx = 1;
  paused  = false;
  trackStart = millis();
  pausedAt = 0;
  if (player_available()) player_play_album(ALBUMS[album].folder,
                                           ALBUMS[album].tracks);
  layoutDots(ALBUMS[album].tracks);
  refreshDots();
}

static void goSelector() {
  st = ST_SELECTOR;
  refreshSelMeta();
  fadeTo(selBox, 255, 260);
  fadeTo(playBox, 0, 240);
  fadeTo(volBox, 0, 160);
  // El disco frena con inercia; la musica sigue sonando.
  vinyl_set_spinning(false);
  vinyl_set_album(album, false);
}

static void goPlaying(bool restart) {
  st = ST_PLAYING;
  fadeTo(selBox, 0, 200);
  fadeTo(playBox, 255, 320);
  vinyl_set_album(album, false);
  vinyl_set_spinning(true);

  if (restart || loaded != (int8_t)album || !trackIx) {
    startAlbum();
  } else if (paused) {
    paused = false;
    if (pausedAt) trackStart += millis() - pausedAt;
    pausedAt = 0;
    if (player_available()) player_resume();
  }
  refreshDots();
}

// ── Anillo de LEDs ───────────────────────────────────────────────────────────
static void paintRing() {
  // Manda el album que SUENA, no el que estas hojeando: asi la biblioteca te
  // dice de que disco viene la musica sin necesidad de leer nada.
  uint8_t src = (loaded >= 0 && trackIx) ? (uint8_t)loaded : album;
  bool    breathe = (trackIx && !paused);

  uint8_t br = 85;
  if (breathe) {
    float phase = (millis() % 2600) / 2600.0f;
    br = (uint8_t)(38 + 72 * (0.5f - 0.5f * cosf(phase * 2 * PI)));
  }
  ring.setBrightness(br);
  for (int i = 0; i < NUM_LEDS; i++) ring.setPixelColor(i, ALBUMS[src].color);
  ring.show();
}

// ── API ──────────────────────────────────────────────────────────────────────
bool app_begin() {
  pinMode(PIN_RGB_PWR, OUTPUT);
  digitalWrite(PIN_RGB_PWR, HIGH);
  ring.begin();

  lv_obj_t* scr = lv_scr_act();
  if (!vinyl_create(scr)) return false;

  selBox  = mkBox(scr);
  selMeta = mkLabel(selBox, &lv_font_montserrat_14, 130, 254);

  playBox = mkBox(scr);
  for (uint8_t i = 0; i < MAX_DOTS; i++) {
    dots[i] = lv_obj_create(playBox);
    lv_obj_remove_style_all(dots[i]);
    lv_obj_set_size(dots[i], 6, 6);
    lv_obj_set_style_radius(dots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dots[i], lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dots[i], 40, 0);
    lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
  }
  timeLbl = mkLabel(playBox, &lv_font_montserrat_16, 190, 254);
  lv_label_set_text(timeLbl, "0:00");

  // Overlay de volumen: aro sobre el canto del disco + el numero. Aparece al
  // girar y se va solo.
  volBox = mkBox(scr);
  volArc = mkRimArc(volBox, 296, 4, 30);
  lv_arc_set_range(volArc, 0, PLAYER_VOL_MAX);
  lv_arc_set_value(volArc, vol);
  volLbl = mkLabel(volBox, &lv_font_montserrat_20, LV_OPA_COVER, 250);

  // Aro del mantener: crece mientras sostienes y completa la vuelta justo
  // cuando el gesto se dispara. Sin esto, mantener 900ms se siente identico a
  // no hacer nada, y el usuario suelta antes de tiempo creyendo que no sirve.
  holdArc = mkRimArc(scr, 330, 4, 0);
  lv_arc_set_range(holdArc, 0, 100);
  lv_arc_set_value(holdArc, 0);
  lv_obj_set_style_opa(holdArc, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(holdArc, LV_OBJ_FLAG_HIDDEN);

  vinyl_set_album(album, false);
  refreshSelMeta();
  lv_obj_clear_flag(selBox, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_opa(selBox, LV_OPA_COVER, 0);
  paintRing();
  return true;
}

void app_event(KnobEvent e) {
  if (e == KNOB_DOWN) { vinyl_bump(); return; }

  switch (st) {
    case ST_SELECTOR:
      if (e == KNOB_CW || e == KNOB_CCW) {
        if (e == KNOB_CW && album < ALBUM_COUNT - 1)  album++;
        else if (e == KNOB_CCW && album > 0)          album--;
        vinyl_set_album(album, true);
        vinyl_nudge_sheen(e == KNOB_CW ? +1 : -1);
        refreshSelMeta();
      } else if (e == KNOB_PRESS) {
        // Sin escalas: un push y suena. Si este disco es el que ya suena,
        // regresas a el sin reiniciarlo.
        bool mismo = (loaded == (int8_t)album && trackIx);
        goPlaying(!mismo);
      }
      break;

    case ST_PLAYING:
      if (e == KNOB_CW || e == KNOB_CCW) {
        int16_t v = (int16_t)vol + (e == KNOB_CW ? VOL_STEP : -VOL_STEP);
        if (v < 0) v = 0;
        if (v > PLAYER_VOL_MAX) v = PLAYER_VOL_MAX;
        vol = (uint8_t)v;
        player_set_volume(vol);      // la cola colapsa las rafagas de la perilla
        char t[8];
        snprintf(t, sizeof(t), "%u", vol);
        lv_label_set_text(volLbl, t);
        lv_arc_set_value(volArc, vol);
        fadeTo(volBox, 255, 120);
        fadeTo(timeLbl, 0, 120);
        volShownMs = millis();
      } else if (e == KNOB_PRESS) {
        paused = !paused;
        if (paused) {
          pausedAt = millis();
          if (player_available()) player_pause();
          vinyl_set_spinning(false);
        } else {
          if (pausedAt) trackStart += millis() - pausedAt;
          pausedAt = 0;
          if (player_available()) player_resume();
          vinyl_set_spinning(true);
        }
      } else if (e == KNOB_DOUBLE_PRESS) {
        trackIx = (uint8_t)(trackIx % ALBUMS[album].tracks) + 1;
        trackStart = millis();
        pausedAt = 0;
        paused = false;
        if (player_available()) player_next();
        vinyl_set_spinning(true);
        refreshDots();
      } else if (e == KNOB_LONG_PRESS) {
        goSelector();
      }
      break;
  }
  paintRing();
}

void app_tick() {
  vinyl_tick();

  // Aro de progreso del mantener. Solo tiene sentido en reproduccion, que es
  // donde el gesto hace algo.
  uint32_t h = (st == ST_PLAYING) ? knob::holdMs() : 0;
  if (h > HOLD_SHOW_MS) {
    int32_t pct = (int32_t)((h * 100) / KNOB_LONG_PRESS_MS);
    lv_arc_set_value(holdArc, pct > 100 ? 100 : pct);
    if (!holdShown) { holdShown = true; fadeTo(holdArc, 220, 90); }
  } else if (holdShown) {
    holdShown = false;
    fadeTo(holdArc, 0, 180);
  }

  // El overlay de volumen se retira solo y devuelve el tiempo a su lugar.
  if (volShownMs && millis() - volShownMs > VOL_HOLD_MS) {
    volShownMs = 0;
    fadeTo(volBox, 0, 240);
    if (st == ST_PLAYING) fadeTo(timeLbl, 190, 240);
  }

  if (millis() - lastSecond >= 250) {
    lastSecond = millis();

    // Con el DFPlayer conectado, el avance de pista lo manda el modulo.
    if (player_available() && player_track_index() &&
        player_track_index() != trackIx) {
      trackIx = player_track_index();
      trackStart = millis();
      refreshDots();
    }

    if (st == ST_PLAYING && !volShownMs) {
      char t[12];
      fmtTime(t, sizeof(t), elapsedS());
      lv_label_set_text(timeLbl, t);
    }
  }

  if (trackIx && !paused) paintRing();     // la respiracion necesita refresco
}

AppState app_state() { return st; }
