#include "app.h"
#include "pins.h"
#include "albums.h"
#include "vinyl.h"
#include "player.h"
#include "display.h"
#include "netclock.h"
#include "alarm.h"
#include <Adafruit_NeoPixel.h>

#define MAX_DOTS       16
#define DOT_R         160    // radio donde viven los puntos de pista
#define VOL_HOLD_MS  1400    // cuanto se queda el overlay de volumen
#define VOL_STEP        1
#define HOLD_SHOW_MS  140    // a partir de aqui se ve el aro de "mantener"

// Reposo: a los 30s sin tocar nada, la pantalla se retira. Si hay musica no se
// apaga del todo — bajarla a la mitad deja ver que sigue sonando sin alumbrar
// el cuarto. Si no hay musica, no hay nada que mirar y se apaga completa.
#define IDLE_MS     30000
#define DIM_PCT        50

// La alarma es un disco mas al final de la fila. No es un menu escondido ni un
// gesto secreto: se llega girando, igual que a cualquier album.
#define ITEM_COUNT   (ALBUM_COUNT + 1)
#define ES_ALARMA(i) ((i) >= ALBUM_COUNT)
#define COLOR_ALARMA 0x6C7A89
#define CAMPOS         5

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
static uint8_t  blNivel    = 100;    // ultimo nivel pedido al backlight
static bool     apagada    = false;
static uint8_t  campo      = 0;        // campo seleccionado en la alarma
static bool     editando   = false;
static bool     alarmaSonando = false;

// ── Widgets ──────────────────────────────────────────────────────────────────
static lv_obj_t* selBox   = nullptr;   // info del disco mientras hojeas
static lv_obj_t* selName  = nullptr;
static lv_obj_t* selMeta  = nullptr;
static lv_obj_t* fondo    = nullptr;   // plinto de madera
static lv_color_t* fondoBuf = nullptr;
static lv_obj_t* playBox  = nullptr;
static lv_obj_t* dots[MAX_DOTS] = { nullptr };
static lv_obj_t* timeLbl  = nullptr;
static lv_obj_t* volBox   = nullptr;
static lv_obj_t* volArc   = nullptr;
static lv_obj_t* volLbl   = nullptr;
static lv_obj_t* holdArc  = nullptr;   // progreso del mantener
static lv_obj_t* alarmBox = nullptr;
static lv_obj_t* alRot[CAMPOS] = { nullptr };
static lv_obj_t* alVal[CAMPOS] = { nullptr };
static lv_obj_t* vecinoIzq = nullptr;  // discos de al lado, solo en biblioteca
static lv_obj_t* vecinoDer = nullptr;

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

// Los vecinos solo existen en la biblioteca, y son lo que hace inconfundible
// donde estas: con compania estas hojeando, solo estas dentro de un disco.
// Van detras del vinilo, asomandose por los costados como discos en una caja.
static void refreshVecinos() {
  // Con la biblioteca circular siempre hay disco a los dos lados.
  bool hay = ITEM_COUNT > 1;
  uint8_t izq = (uint8_t)((album + ITEM_COUNT - 1) % ITEM_COUNT);
  uint8_t der = (uint8_t)((album + 1) % ITEM_COUNT);
  lv_obj_set_style_bg_color(vecinoIzq, lv_color_hex(
      ES_ALARMA(izq) ? COLOR_ALARMA : ALBUMS[izq].color), 0);
  lv_obj_set_style_bg_color(vecinoDer, lv_color_hex(
      ES_ALARMA(der) ? COLOR_ALARMA : ALBUMS[der].color), 0);
  fadeTo(vecinoIzq, hay ? 225 : 0, 240);
  fadeTo(vecinoDer, hay ? 225 : 0, 240);
}

// ── Contenido ────────────────────────────────────────────────────────────────
static void fmtTime(char* out, size_t n, uint32_t secs) {
  snprintf(out, n, "%lu:%02lu", (unsigned long)(secs / 60),
           (unsigned long)(secs % 60));
}

// La info del disco vive en el selector, no en una pantalla aparte: se lee
// mientras hojeas y no cuesta un push extra.
// El plinto: la madera sobre la que descansa el disco. Se dibuja UNA vez.
// Veta horizontal tenue y viNeta hacia el borde, para que el vinilo se recorte
// contra algo con cuerpo en vez de flotar en negro.
static void drawFondo() {
  lv_canvas_fill_bg(fondo, lv_color_hex(0x5A3E28), LV_OPA_COVER);

  lv_draw_line_dsc_t ln;
  lv_draw_line_dsc_init(&ln);
  ln.width = 1;

  uint32_t rnd = 0x1234ABCD;                 // veta reproducible, no aleatoria
  for (lv_coord_t y = 0; y < 360; y++) {
    rnd = rnd * 1664525u + 1013904223u;
    uint8_t v = (rnd >> 16) & 0x1F;
    if (v > 22) continue;                    // no todas las lineas llevan veta
    ln.color = (v & 1) ? lv_color_hex(0x7A5636) : lv_color_hex(0x3E2A1B);
    ln.opa   = (lv_opa_t)(30 + (v & 7) * 12);
    lv_point_t p[2] = { {0, y}, {359, y} };
    lv_canvas_draw_line(fondo, p, 2, &ln);
  }

  // ViNeta: oscurece hacia afuera para que la esquina redonda no compita.
  lv_draw_arc_dsc_t v;
  lv_draw_arc_dsc_init(&v);
  v.color = lv_color_black();
  v.width = 3;
  for (lv_coord_t r = 148; r < 182; r += 2) {
    v.opa = (lv_opa_t)(((r - 148) * 150) / 34);
    lv_canvas_draw_arc(fondo, 180, 180, r, 0, 360, &v);
  }
}

static void refreshSelMeta() {
  if (ES_ALARMA(album)) {
    AlarmCfg& c = alarm_cfg();
    lv_label_set_text(selName, "ALARMA");
    char t[64], hhmm[8];
    clock_hhmm(hhmm, sizeof(hhmm));
    if (!c.activa)      snprintf(t, sizeof(t), "apagada\nson las %s", hhmm);
    else if (!clock_ready())
                        snprintf(t, sizeof(t), "%02d:%02d\n%s", c.hora,
                                 c.minuto, clock_status());
    else                snprintf(t, sizeof(t), "%02d:%02d\nson las %s",
                                 c.hora, c.minuto, hhmm);
    lv_label_set_text(selMeta, t);
    vinyl_set_custom("ALARMA", c.activa ? "activada" : "apagada",
                     COLOR_ALARMA, false);
    return;
  }
  const Album& a = ALBUMS[album];
  vinyl_set_album(album, false);
  lv_label_set_text(selName, a.name);
  char t[48];
  if (a.tracks == 0) {
    snprintf(t, sizeof(t), "vacio");
  } else {
    unsigned min = (a.seconds + 30) / 60;
    snprintf(t, sizeof(t), "%u %s\n%u %s", a.tracks,
             a.tracks == 1 ? "cancion" : "canciones",
             min, min == 1 ? "minuto" : "minutos");
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

// ── Alarma ───────────────────────────────────────────────────────────────────
static const char* CAMPO_ROT[CAMPOS] = { "Hora", "Minuto", "Dias", "Disco",
                                         "Alarma" };

static void refreshAlarma() {
  AlarmCfg& c = alarm_cfg();
  char v[40];
  for (uint8_t i = 0; i < CAMPOS; i++) {
    switch (i) {
      case 0: snprintf(v, sizeof(v), "%02d", c.hora);   break;
      case 1: snprintf(v, sizeof(v), "%02d", c.minuto); break;
      case 2: snprintf(v, sizeof(v), "%s", alarm_texto_dias(c.dias)); break;
      case 3: snprintf(v, sizeof(v), "%s",
                       ALBUMS[c.album % ALBUM_COUNT].name); break;
      default: snprintf(v, sizeof(v), "%s", c.activa ? "activada" : "apagada");
    }
    lv_label_set_text(alVal[i], v);

    bool sel = (i == campo);
    // Editando: el valor toma color, para que se vea que el giro le pertenece
    // a ese campo y no a la navegacion.
    lv_color_t colVal = (sel && editando) ? lv_color_hex(0xD4A017)
                                          : lv_color_white();
    lv_obj_set_style_text_color(alVal[i], colVal, 0);
    lv_obj_set_style_text_opa(alVal[i], sel ? LV_OPA_COVER : 90, 0);
    lv_obj_set_style_text_opa(alRot[i], sel ? 200 : 70, 0);
  }
}

static void goAlarma() {
  st = ST_ALARMA;
  campo = 0;
  editando = false;
  refreshAlarma();
  fadeTo(selBox, 0, 180);
  fadeTo(vecinoIzq, 0, 180);
  fadeTo(vecinoDer, 0, 180);
  fadeTo(alarmBox, 255, 280);
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
  vinyl_zoom_to(62, 420);        // la camara se aleja
  refreshVecinos();
  fadeTo(selBox, 255, 260);
  fadeTo(playBox, 0, 240);
  fadeTo(volBox, 0, 160);
  // El disco frena con inercia; la musica sigue sonando.
  vinyl_set_spinning(false);
  vinyl_set_album(album, false);
}

static void goPlaying(bool restart) {
  st = ST_PLAYING;
  vinyl_zoom_to(100, 420);       // la camara se acerca: el disco llena el cuadro
  fadeTo(vecinoIzq, 0, 200);
  fadeTo(vecinoDer, 0, 200);
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
  // El anillo sigue a la pantalla: si ella se retira, el tambien. Con la
  // pantalla apagada se corta la corriente de la tira (GPIO17), no solo el
  // brillo: un LED en brillo 0 sigue alimentado y sigue calentando.
  if (blNivel == 0) {
    ring.clear();
    ring.show();
    digitalWrite(PIN_RGB_PWR, LOW);
    return;
  }
  digitalWrite(PIN_RGB_PWR, HIGH);

  // Manda el album que SUENA, no el que estas hojeando: asi la biblioteca te
  // dice de que disco viene la musica sin necesidad de leer nada.
  uint8_t src = (loaded >= 0 && trackIx) ? (uint8_t)loaded : album;
  bool    breathe = (trackIx && !paused);

  uint8_t br = 85;
  if (breathe) {
    float phase = (millis() % 2600) / 2600.0f;
    br = (uint8_t)(38 + 72 * (0.5f - 0.5f * cosf(phase * 2 * PI)));
  }
  // El nivel de la pantalla escala el del anillo: al 50% de brillo, la luz
  // ambiental baja igual y el objeto entero se atenua como una sola cosa.
  uint32_t col = ES_ALARMA(src) ? COLOR_ALARMA : ALBUMS[src].color;
  ring.setBrightness((uint8_t)((uint16_t)br * blNivel / 100));
  for (int i = 0; i < NUM_LEDS; i++) ring.setPixelColor(i, col);
  ring.show();
}

// ── API ──────────────────────────────────────────────────────────────────────
bool app_begin() {
  pinMode(PIN_RGB_PWR, OUTPUT);
  digitalWrite(PIN_RGB_PWR, HIGH);
  ring.begin();

  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  fondoBuf = (lv_color_t*)heap_caps_malloc(
      LV_CANVAS_BUF_SIZE_TRUE_COLOR(360, 360), MALLOC_CAP_SPIRAM);
  if (fondoBuf) {
    fondo = lv_canvas_create(scr);
    lv_canvas_set_buffer(fondo, fondoBuf, 360, 360, LV_IMG_CF_TRUE_COLOR);
    lv_obj_center(fondo);
    drawFondo();
  }

  // Se crean primero para que el vinilo quede encima de ellos.
  for (int i = 0; i < 2; i++) {
    lv_obj_t* v = lv_obj_create(scr);
    lv_obj_remove_style_all(v);
    lv_obj_set_size(v, 168, 168);
    lv_obj_set_pos(v, i == 0 ? -78 : 270, 96);
    lv_obj_set_style_radius(v, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(v, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(v, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(v, LV_OBJ_FLAG_HIDDEN);
    if (i == 0) vecinoIzq = v; else vecinoDer = v;
  }

  if (!vinyl_create(scr)) return false;

  selBox  = mkBox(scr);
  // Titulo arriba y datos abajo, con el disco en medio: apilados los dos
  // debajo, el segundo renglon caia contra el borde redondo y se perdia.
  selName = mkLabel(selBox, &lv_font_montserrat_22, LV_OPA_COVER, 36);
  selMeta = mkLabel(selBox, &lv_font_montserrat_16, 155, 282);

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

  // Pantalla de la alarma: fondo propio para que el texto se lea, y filas de
  // rotulo + valor. Se navega con el mismo vocabulario de siempre.
  alarmBox = mkBox(scr);
  {
    lv_obj_t* velo = lv_obj_create(alarmBox);
    lv_obj_remove_style_all(velo);
    lv_obj_set_size(velo, 360, 360);
    lv_obj_center(velo);
    lv_obj_set_style_radius(velo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(velo, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(velo, 230, 0);

    lv_obj_t* tit = lv_label_create(alarmBox);
    lv_obj_set_style_text_font(tit, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(tit, lv_color_hex(COLOR_ALARMA), 0);
    lv_obj_set_style_text_align(tit, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(tit, 300);
    lv_obj_set_pos(tit, 30, 44);
    lv_label_set_text(tit, "ALARMA");

    for (uint8_t i = 0; i < CAMPOS; i++) {
      lv_coord_t y = 96 + i * 38;
      alRot[i] = lv_label_create(alarmBox);
      lv_obj_set_style_text_font(alRot[i], &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(alRot[i], lv_color_white(), 0);
      lv_obj_set_width(alRot[i], 110);
      lv_obj_set_pos(alRot[i], 62, y);
      lv_label_set_text(alRot[i], CAMPO_ROT[i]);

      alVal[i] = lv_label_create(alarmBox);
      lv_obj_set_style_text_font(alVal[i], &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(alVal[i], lv_color_white(), 0);
      lv_obj_set_style_text_align(alVal[i], LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_width(alVal[i], 130);
      lv_obj_set_pos(alVal[i], 168, y);
    }
  }

  // Aro del mantener: crece mientras sostienes y completa la vuelta justo
  // cuando el gesto se dispara. Sin esto, mantener 900ms se siente identico a
  // no hacer nada, y el usuario suelta antes de tiempo creyendo que no sirve.
  holdArc = mkRimArc(scr, 330, 4, 0);
  lv_arc_set_range(holdArc, 0, 100);
  lv_arc_set_value(holdArc, 0);
  lv_obj_set_style_opa(holdArc, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(holdArc, LV_OBJ_FLAG_HIDDEN);

  vinyl_set_album(album, false);
  vinyl_zoom_to(62, 1);          // arranca en la biblioteca, ya alejado
  refreshSelMeta();
  refreshVecinos();
  lv_obj_clear_flag(selBox, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_opa(selBox, LV_OPA_COVER, 0);
  paintRing();
  return true;
}

void app_event(KnobEvent e) {
  // Con la pantalla apagada, el gesto que despierta SOLO despierta. Si no,
  // alcanzas la perilla a ciegas para ver la hora y acabas cambiando de disco
  // o pausando la musica sin querer.
  if (apagada) {
    apagada = false;
    blNivel = 100;
    display_backlight_fade(100, 260);
    return;
  }
  if (e == KNOB_DOWN) { vinyl_bump(); return; }

  // Con la alarma sonando, cualquier push la apaga y devuelve a la biblioteca.
  // Es lo primero que hace una mano dormida, y no debe requerir punteria.
  if (alarmaSonando && (e == KNOB_PRESS || e == KNOB_DOUBLE_PRESS ||
                        e == KNOB_LONG_PRESS)) {
    alarmaSonando = false;
    if (player_available()) player_stop();
    trackIx = 0;
    paused = false;
    loaded = -1;
    goSelector();
    paintRing();
    return;
  }

  switch (st) {
    case ST_SELECTOR:
      if (e == KNOB_CW || e == KNOB_CCW) {
        // Circular: pasado el ultimo vuelve el primero. Topar contra un
        // extremo se siente como una falla; girar sin fin se siente como
        // hojear una caja de discos.
        if (e == KNOB_CW) album = (uint8_t)((album + 1) % ITEM_COUNT);
        else              album = (uint8_t)((album + ITEM_COUNT - 1) % ITEM_COUNT);
        refreshSelMeta();
        if (!ES_ALARMA(album)) vinyl_set_album(album, true);
        refreshVecinos();
      } else if (e == KNOB_PRESS) {
        if (ES_ALARMA(album)) { goAlarma(); break; }
        // Sin escalas: un push y suena. Si este disco es el que ya suena,
        // regresas a el sin reiniciarlo.
        bool mismo = (loaded == (int8_t)album && trackIx);
        goPlaying(!mismo);
      }
      break;

    case ST_ALARMA: {
      AlarmCfg& c = alarm_cfg();
      int8_t d = (e == KNOB_CW) ? +1 : (e == KNOB_CCW ? -1 : 0);
      if (d != 0) {
        if (!editando) {
          campo = (uint8_t)((campo + CAMPOS + d) % CAMPOS);
        } else {
          switch (campo) {
            case 0: c.hora   = (uint8_t)((c.hora + 24 + d) % 24); break;
            // El minuto va de cinco en cinco: de uno en uno serian 60 muescas
            // para cruzar la hora, y nadie pone una alarma a las 7:23.
            case 1: c.minuto = (uint8_t)((c.minuto + 60 + d * 5) % 60); break;
            case 2: {
              const uint8_t ciclo[3] = { DIAS_LAV, DIAS_FIN, DIAS_TODOS };
              uint8_t k = 0;
              for (uint8_t i = 0; i < 3; i++) if (ciclo[i] == c.dias) k = i;
              c.dias = ciclo[(k + 3 + d) % 3];
              break;
            }
            case 3: c.album = (uint8_t)((c.album + ALBUM_COUNT + d) % ALBUM_COUNT);
                    break;
            default: c.activa = !c.activa;
          }
        }
        refreshAlarma();
      } else if (e == KNOB_PRESS) {
        editando = !editando;
        if (!editando) alarm_save();     // se guarda al confirmar el campo
        refreshAlarma();
      } else if (e == KNOB_LONG_PRESS) {
        editando = false;
        alarm_save();
        fadeTo(alarmBox, 0, 220);
        goSelector();
      }
      break;
    }

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
  clock_tick();

  // Disparo de la alarma. Solo con hora buena: sin NTP el reloj arranca en
  // 1970 y dispararia en cuanto encendieras el aparato.
  {
    struct tm t;
    if (clock_now(&t) && alarm_debe_sonar(t)) {
      Serial.printf("alarma: %02d:%02d, suena %s\n", t.tm_hour, t.tm_min,
                    ALBUMS[alarm_cfg().album % ALBUM_COUNT].name);
      album = (uint8_t)(alarm_cfg().album % ALBUM_COUNT);
      alarmaSonando = true;
      blNivel = 100;
      apagada = false;
      display_backlight_fade(100, 900);   // amanecer, no golpe de luz
      goPlaying(true);
      paintRing();
    }
  }

  // Auto-retiro de la pantalla. El fade es lento al irse y rapido al volver:
  // apagarse debe sentirse como algo que se retira solo, encenderse como una
  // respuesta inmediata a tu mano.
  {
    uint32_t idle = knob::idleMs();
    bool sonando = (trackIx && !paused);
    uint8_t quiero = (idle < IDLE_MS) ? 100 : (sonando ? DIM_PCT : 0);
    if (quiero != blNivel) {
      blNivel = quiero;
      apagada = (quiero == 0);
      display_backlight_fade(quiero, quiero == 100 ? 260
                                   : (quiero == 0 ? 1200 : 800));
      paintRing();   // el anillo cambia con ella, aunque no haya musica
    }
  }

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
