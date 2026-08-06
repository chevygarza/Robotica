#include "app.h"
#include "pins.h"
#include "albums.h"
#include "vinyl.h"
#include "player.h"
#include "display.h"
#include "netclock.h"
#include "alarm.h"
#include "settings.h"
#include "battery.h"
#include <Adafruit_NeoPixel.h>

#define MAX_DOTS       16
#define DOT_R         160    // radio donde viven los puntos de pista
#define VOL_HOLD_MS  1400    // cuanto se queda el overlay de volumen
#define VOL_STEP        1
#define HOLD_SHOW_MS  140    // a partir de aqui se ve el aro de "mantener"

// Reposo. Los valores de arranque cambiaron tras vivir con el aparato: 30
// segundos y apagado total se leia como aparato descompuesto, no como aparato
// dormido — pasa que lo miras, esta negro, y crees que se colgo.
//
// Ahora tarda mas en retirarse y nunca se apaga del todo: en reposo queda un
// resplandor bajo que dice "aqui estoy". Los tres valores seran ajustables
// desde la pantalla de Ajustes.
#define IDLE_MS    180000    // 3 minutos
#define DIM_PCT         50   // con musica
#define REPOSO_PCT      12   // sin musica: tenue, pero vivo

// La alarma es un disco mas al final de la fila. No es un menu escondido ni un
// gesto secreto: se llega girando, igual que a cualquier album.
// Dos discos especiales al final de la fila: alarma y ajustes. Ninguno es un
// menu escondido — se llega girando, igual que a cualquier album.
#define ITEM_COUNT    (ALBUM_COUNT + 2)
#define ES_ALARMA(i)  ((i) == ALBUM_COUNT)
#define ES_AJUSTES(i) ((i) == ALBUM_COUNT + 1)
#define ES_ESPECIAL(i) ((i) >= ALBUM_COUNT)
#define COLOR_ALARMA  0x6C7A89
#define COLOR_AJUSTES 0x9AA0A6
#define CAMPOS          5     // alarma
#define AJ_CAMPOS       7     // ajustes

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
static lv_obj_t* playBox  = nullptr;
static lv_obj_t* timeLbl  = nullptr;
static lv_obj_t* volBox   = nullptr;
static lv_obj_t* volArc   = nullptr;
static lv_obj_t* volLbl   = nullptr;
static lv_obj_t* holdArc  = nullptr;   // progreso del mantener
static lv_obj_t* alarmBox = nullptr;
static lv_obj_t* alRot[CAMPOS] = { nullptr };
static lv_obj_t* alVal[CAMPOS] = { nullptr };
static lv_obj_t* ajBox    = nullptr;
static lv_obj_t* ajRot[AJ_CAMPOS] = { nullptr };
static lv_obj_t* ajVal[AJ_CAMPOS] = { nullptr };
static lv_obj_t* ajBat    = nullptr;   // lectura de la celda, solo informativa
static lv_obj_t* halo     = nullptr;   // resplandor del album, solo al tocar
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
  auto colorDe = [](uint8_t i) -> uint32_t {
    if (ES_AJUSTES(i)) return COLOR_AJUSTES;
    if (ES_ALARMA(i))  return COLOR_ALARMA;
    return ALBUMS[i].color;
  };
  lv_obj_set_style_bg_color(vecinoIzq, lv_color_hex(colorDe(izq)), 0);
  lv_obj_set_style_bg_color(vecinoDer, lv_color_hex(colorDe(der)), 0);
  // Si ya estan a la vista, basta con cambiarles el color: lanzar una
  // animacion por muesca solo agrega trabajo y se siente como arrastre.
  for (lv_obj_t* v : { vecinoIzq, vecinoDer }) {
    if (!hay) { fadeTo(v, 0, 240); continue; }
    if (lv_obj_has_flag(v, LV_OBJ_FLAG_HIDDEN)) fadeTo(v, 225, 240);
    else lv_obj_set_style_opa(v, 225, 0);
  }
}

// ── Contenido ────────────────────────────────────────────────────────────────
static void fmtTime(char* out, size_t n, uint32_t secs) {
  snprintf(out, n, "%lu:%02lu", (unsigned long)(secs / 60),
           (unsigned long)(secs % 60));
}

// La info del disco vive en el selector, no en una pantalla aparte: se lee
// mientras hojeas y no cuesta un push extra.
// El disco se pinta UNA vez por cambio. Antes lo hacian dos caminos distintos
// y con caratula eso se sentia como que la perilla se atoraba.
static void refreshDisco(bool animate) {
  if (ES_AJUSTES(album)) {
    vinyl_set_custom("AJUSTES", "del aparato", COLOR_AJUSTES, animate);
    return;
  }
  if (ES_ALARMA(album)) {
    AlarmCfg& c = alarm_cfg();
    vinyl_set_custom("ALARMA", c.activa ? "activada" : "apagada",
                     COLOR_ALARMA, animate);
  } else {
    vinyl_set_album(album, animate);
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
    return;
  }
  if (ES_AJUSTES(album)) {
    Ajustes& j = ajustes();
    lv_label_set_text(selName, "AJUSTES");
    char t[64], r[24];
    aj_texto_reposo(r, sizeof(r), j.reposoMin);
    snprintf(t, sizeof(t), "Reposo %s\nLEDs %s", r, j.leds ? "si" : "no");
    lv_label_set_text(selMeta, t);
    return;
  }
  const Album& a = ALBUMS[album];
  lv_label_set_text(selName, a.name);
  char t[48];
  if (a.tracks == 0) {
    snprintf(t, sizeof(t), "vacio");
  } else {
    unsigned min = (a.seconds + 30) / 60;
    snprintf(t, sizeof(t), "%u %s\n%u %s", a.tracks,
             a.tracks == 1 ? "Cancion" : "Canciones",
             min, min == 1 ? "Minuto" : "Minutos");
  }
  lv_label_set_text(selMeta, t);
}

static void refreshDots() {
  vinyl_set_dots(ES_ESPECIAL(album) ? 0 : ALBUMS[album].tracks, trackIx,
                 ES_ESPECIAL(album) ? 0xFFFFFF : ALBUMS[album].color);
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
    lv_obj_set_style_text_opa(alVal[i], sel ? LV_OPA_COVER : 175, 0);
    lv_obj_set_style_text_opa(alRot[i], sel ? 235 : 150, 0);
  }
}

static const char* AJ_ROT[AJ_CAMPOS] = { "Al terminar", "Reposo", "Luz reposo",
                                         "Luz musica", "Brillo", "LEDs",
                                         "Brillo LEDs" };

static void refreshAjustes() {
  Ajustes& j = ajustes();

  // Solo lectura: no es un campo, es informacion. Por eso vive bajo el titulo
  // y no en la lista que se recorre con la perilla.
  char b[40];
  if (bat_presente()) snprintf(b, sizeof(b), "Bateria %u%%   %u.%02u V",
                               bat_pct(), bat_mv() / 1000, (bat_mv() % 1000) / 10);
  else                snprintf(b, sizeof(b), "sin bateria");
  lv_label_set_text(ajBat, b);
  char v[32];
  for (uint8_t i = 0; i < AJ_CAMPOS; i++) {
    switch (i) {
      case 0: snprintf(v, sizeof(v), "%s", aj_texto_fin(j.alFin)); break;
      case 1: aj_texto_reposo(v, sizeof(v), j.reposoMin); break;
      case 2: snprintf(v, sizeof(v), "%u%%", j.luzReposo); break;
      case 3: snprintf(v, sizeof(v), "%u%%", j.luzMusica); break;
      case 4: snprintf(v, sizeof(v), "%u%%", j.brillo);    break;
      case 5: snprintf(v, sizeof(v), "%s", j.leds ? "si" : "no"); break;
      default: snprintf(v, sizeof(v), "%u%%", j.brilloLeds);
    }
    lv_label_set_text(ajVal[i], v);
    bool sel = (i == campo);
    lv_obj_set_style_text_color(ajVal[i], (sel && editando)
        ? lv_color_hex(0xD4A017) : lv_color_white(), 0);
    lv_obj_set_style_text_opa(ajVal[i], sel ? LV_OPA_COVER : 175, 0);
    lv_obj_set_style_text_opa(ajRot[i], sel ? 235 : 150, 0);
  }
}

static void goAjustes() {
  st = ST_AJUSTES;
  campo = 0;
  editando = false;
  refreshAjustes();
  fadeTo(selBox, 0, 180);
  fadeTo(vecinoIzq, 0, 180);
  fadeTo(vecinoDer, 0, 180);
  fadeTo(ajBox, 255, 280);
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
  refreshDots();
}

static void goSelector() {
  st = ST_SELECTOR;
  refreshSelMeta();
  vinyl_zoom_to(62, 420);        // la camara se aleja
  vinyl_cover_mode(true);        // hojeando portadas
  fadeTo(halo, 0, 400);
  refreshVecinos();
  fadeTo(selBox, 255, 260);
  fadeTo(playBox, 0, 240);
  fadeTo(volBox, 0, 160);
  // El disco frena con inercia; la musica sigue sonando.
  vinyl_set_spinning(false);
  refreshDisco(false);
}

static void goPlaying(bool restart) {
  st = ST_PLAYING;
  vinyl_zoom_to(100, 420);       // la camara se acerca: el disco llena el cuadro
  vinyl_cover_mode(false);       // en el plato: vuelve a ser un vinilo
  if (!ES_ALARMA(album)) {
    lv_obj_set_style_bg_color(halo, lv_color_hex(ALBUMS[album].color), 0);
    fadeTo(halo, 130, 500);
  }
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
  // Los LEDs apagados por ajuste, o la pantalla en cero: en ambos casos se
  // corta la corriente de la tira, no solo el brillo.
  if (blNivel == 0 || !ajustes().leds) {
    ring.clear();
    ring.show();
    digitalWrite(PIN_RGB_PWR, LOW);
    // El LED de encendido tambien: dejarlo prendido junto a una pantalla negra
    // hace que el aparato parezca colgado en vez de dormido. Es activo en LOW.
    digitalWrite(PIN_POWER_LED, HIGH);
    return;
  }
  digitalWrite(PIN_RGB_PWR, HIGH);
  digitalWrite(PIN_POWER_LED, LOW);

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
  ring.setBrightness((uint8_t)((uint32_t)br * blNivel * ajustes().brilloLeds
                               / 10000));

  // Con caratula, cada LED toma el color del sector de la imagen que le queda
  // detras: el anillo es un reflejo del arte, no ocho copias del mismo tono.
  const uint32_t* pal = ES_ESPECIAL(src) ? nullptr : ALBUMS[src].anillo;
  if (pal) {
    for (int i = 0; i < NUM_LEDS; i++) ring.setPixelColor(i, pal[i % 8]);
  } else {
    uint32_t col = ES_AJUSTES(src) ? COLOR_AJUSTES
                 : ES_ALARMA(src)  ? COLOR_ALARMA : ALBUMS[src].color;
    for (int i = 0; i < NUM_LEDS; i++) ring.setPixelColor(i, col);
  }
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


  // Halo: un circulo del color del album, apenas mas grande que el disco. Al
  // reproducir asoma como un borde de luz alrededor del vinilo. Es un solo
  // objeto plano, no un lienzo: no cuesta nada por frame.
  halo = lv_obj_create(scr);
  lv_obj_remove_style_all(halo);
  lv_obj_set_size(halo, 358, 358);
  lv_obj_center(halo);
  lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(halo, LV_OPA_COVER, 0);
  lv_obj_set_style_opa(halo, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(halo, LV_OBJ_FLAG_HIDDEN);

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
  selMeta = mkLabel(selBox, &lv_font_montserrat_16, 155, 292);

  playBox = mkBox(scr);
  timeLbl = mkLabel(playBox, &lv_font_montserrat_16, 190, 300);
  lv_label_set_text(timeLbl, "0:00");

  // Overlay de volumen: aro sobre el canto del disco + el numero. Aparece al
  // girar y se va solo.
  volBox = mkBox(scr);
  volArc = mkRimArc(volBox, 296, 4, 30);
  lv_arc_set_range(volArc, 0, PLAYER_VOL_MAX);
  lv_arc_set_value(volArc, vol);
  volLbl = mkLabel(volBox, &lv_font_montserrat_20, LV_OPA_COVER, 296);

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

  // Pantalla de Ajustes: misma estructura que la de alarma, mismo vocabulario
  // de gestos. No hay nada nuevo que aprender.
  ajBox = mkBox(scr);
  {
    lv_obj_t* velo = lv_obj_create(ajBox);
    lv_obj_remove_style_all(velo);
    lv_obj_set_size(velo, 360, 360);
    lv_obj_center(velo);
    lv_obj_set_style_radius(velo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(velo, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(velo, 230, 0);

    lv_obj_t* tit = lv_label_create(ajBox);
    lv_obj_set_style_text_font(tit, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(tit, lv_color_hex(COLOR_AJUSTES), 0);
    lv_obj_set_style_text_align(tit, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(tit, 300);
    lv_obj_set_pos(tit, 30, 40);
    lv_label_set_text(tit, "AJUSTES");

    ajBat = lv_label_create(ajBox);
    lv_obj_set_style_text_font(ajBat, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ajBat, lv_color_white(), 0);
    lv_obj_set_style_text_opa(ajBat, 140, 0);
    lv_obj_set_style_text_align(ajBat, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ajBat, 300);
    lv_obj_set_pos(ajBat, 30, 68);
    lv_label_set_text(ajBat, "");

    for (uint8_t i = 0; i < AJ_CAMPOS; i++) {
      lv_coord_t y = 98 + i * 31;      // siete filas piden pasos mas cortos
      ajRot[i] = lv_label_create(ajBox);
      lv_obj_set_style_text_font(ajRot[i], &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(ajRot[i], lv_color_white(), 0);
      lv_obj_set_width(ajRot[i], 130);
      lv_obj_set_pos(ajRot[i], 52, y);
      lv_label_set_text(ajRot[i], AJ_ROT[i]);

      ajVal[i] = lv_label_create(ajBox);
      lv_obj_set_style_text_font(ajVal[i], &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(ajVal[i], lv_color_white(), 0);
      lv_obj_set_style_text_align(ajVal[i], LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_width(ajVal[i], 120);
      lv_obj_set_pos(ajVal[i], 188, y);
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

  refreshDisco(false);
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
        refreshDisco(true);
        refreshSelMeta();
        refreshVecinos();
      } else if (e == KNOB_PRESS) {
        if (ES_ALARMA(album))  { goAlarma();  break; }
        if (ES_AJUSTES(album)) { goAjustes(); break; }
        // Sin escalas: un push y suena. Si este disco es el que ya suena,
        // regresas a el sin reiniciarlo.
        bool mismo = (loaded == (int8_t)album && trackIx);
        goPlaying(!mismo);
      }
      break;

    case ST_AJUSTES: {
      Ajustes& j = ajustes();
      int8_t d = (e == KNOB_CW) ? +1 : (e == KNOB_CCW ? -1 : 0);
      if (d != 0) {
        if (!editando) campo = (uint8_t)((campo + AJ_CAMPOS + d) % AJ_CAMPOS);
        else switch (campo) {
          case 0: j.alFin      = (uint8_t)((j.alFin + 3 + d) % 3); break;
          case 1: j.reposoMin  = aj_ciclo_reposo(j.reposoMin, d);  break;
          case 2: j.luzReposo  = aj_ciclo_pct(j.luzReposo, d, 0);  break;
          case 3: j.luzMusica  = aj_ciclo_pct(j.luzMusica, d, 0);  break;
          case 4: j.brillo     = aj_ciclo_pct(j.brillo, d, 30);    break;
          case 5: j.leds       = !j.leds;                          break;
          default: j.brilloLeds = aj_ciclo_pct(j.brilloLeds, d, 20);
        }
        refreshAjustes();
        // Brillo y LEDs se aplican al instante: ajustar a ciegas y ver el
        // resultado hasta salir seria adivinar.
        blNivel = 0;
        paintRing();
      } else if (e == KNOB_PRESS) {
        editando = !editando;
        if (!editando) ajustes_save();
        refreshAjustes();
      } else if (e == KNOB_LONG_PRESS) {
        editando = false;
        ajustes_save();
        fadeTo(ajBox, 0, 220);
        goSelector();
      }
      break;
    }

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

  // Disparo de la alarma, una vez por segundo. Consultarlo en cada vuelta del
  // loop metia una espera de 5ms unas 500 veces por segundo, y eso se sentia
  // en la perilla. Solo con hora buena: sin NTP el reloj arranca en 1970 y
  // dispararia en cuanto encendieras el aparato.
  static uint32_t ultimaRevision = 0;
  if (millis() - ultimaRevision >= 1000) {
    ultimaRevision = millis();
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
    Ajustes& j = ajustes();
    uint32_t idle = knob::idleMs();
    bool sonando = (trackIx && !paused);
    uint32_t umbral = j.reposoMin ? (uint32_t)j.reposoMin * 60000UL : 0xFFFFFFFF;
    uint8_t quiero = (idle < umbral) ? j.brillo
                                     : (sonando ? j.luzMusica : j.luzReposo);
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

  // Se acabo el album. Antes el reproductor rebarajaba solo y no paraba nunca;
  // ahora manda el ajuste.
  if (player_available() && player_album_fin()) {
    Ajustes& j = ajustes();
    if (j.alFin == FIN_REPETIR) {
      player_play_album(ALBUMS[album].folder, ALBUMS[album].tracks);
      trackIx = 1;
      trackStart = millis();
      refreshDots();
    } else if (j.alFin == FIN_INFINITO) {
      // Salta los discos especiales y los vacios: infinito significa que la
      // musica no para, no que se atore en una carpeta sin canciones.
      uint8_t sig = album;
      for (uint8_t k = 0; k < ALBUM_COUNT; k++) {
        sig = (uint8_t)((sig + 1) % ALBUM_COUNT);
        if (ALBUMS[sig].tracks) break;
      }
      album = sig;
      refreshDisco(false);
      startAlbum();
      Serial.printf("[audio] infinito: sigue %s\n", ALBUMS[album].name);
    } else {
      // Detener: el disco se asienta derecho y regresas a la biblioteca.
      if (player_available()) player_stop();
      trackIx = 0;
      paused = false;
      loaded = -1;
      goSelector();
    }
    paintRing();
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

// Solo para depurar: que nivel de retroiluminacion cree el firmware que tiene,
// y si considera que hay musica sonando.
uint8_t app_bl_dbg()      { return blNivel; }
bool    app_sonando_dbg() { return trackIx && !paused; }
