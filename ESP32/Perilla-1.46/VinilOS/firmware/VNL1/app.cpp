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
#include "weather.h"
#include "especiales.h"
#include <Adafruit_NeoPixel.h>

LV_FONT_DECLARE(vnl_reloj_78);   // generada con lv_font_conv, solo 0-9 : -

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
#define AJ_CAMPOS      7      // ajustes
#define AJ_FILA_H      32     // alto de fila

// Ambar calido. Atravesando acrilico se lee como amplificador de bulbos; un
// color que cambia con el contenido se lee como periferico gamer. El objeto
// es lo primero.
#define COLOR_AMBAR  0xFF7A10

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
static bool     dormido    = false;   // el reposo ya entro; lo lee paintRing
static uint8_t  campo      = 0;        // campo seleccionado en la alarma
static bool     editando   = false;
static bool     alarmaSonando = false;

// ── Widgets ──────────────────────────────────────────────────────────────────
static lv_obj_t* selBox   = nullptr;   // info del disco mientras hojeas
static lv_obj_t* selName  = nullptr;
static lv_obj_t* selMeta  = nullptr;
static lv_obj_t* playBox  = nullptr;
static lv_obj_t* nowName  = nullptr;   // titulo de la pista en curso
static lv_obj_t* timeLbl  = nullptr;   // artista y tiempo, en la misma linea
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
static lv_obj_t* ajLista  = nullptr;   // contenedor que se desliza
static lv_obj_t* relojBox = nullptr;
static lv_obj_t* rjFecha  = nullptr;
static lv_obj_t* rjHora   = nullptr;
static lv_obj_t* rjClima  = nullptr;   // temperatura y cielo, reloj digital
static lv_obj_t* rjFechaA = nullptr;   // los mismos dos datos, cara analoga
static lv_obj_t* rjClimaA = nullptr;
static lv_obj_t* rjMarca[12] = { nullptr };
static lv_obj_t* rjAguja[3]  = { nullptr };
static lv_obj_t* rjNum[4]    = { nullptr };   // 12, 3, 6, 9
static lv_point_t rjPtMarca[12][2];
static lv_point_t rjPtAguja[3][2];
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

// Que suena ahora mismo. Ojo con el indice: player_track_index() es la
// posicion en la baraja y los nombres estan indexados por ARCHIVO, asi que hay
// que preguntar por el archivo. Con el album barajado nunca coinciden.
//
// Se consulta el album CARGADO, no el que estas hojeando: son distintos en
// cuanto sales a la biblioteca con la musica puesta.
static const char* const* pistaTabla(bool artistas) {
  if (!trackIx || loaded < 0) return nullptr;
  uint8_t src = (uint8_t)loaded;
  if (ES_ESPECIAL(src)) return nullptr;
  const Album& a = ALBUMS[src];
  return artistas ? a.track_artists : a.track_names;
}

static const char* pistaTexto(bool artistas) {
  const char* const* tabla = pistaTabla(artistas);
  if (!tabla) return "";
  uint8_t f = player_track_file();
  if (!f || f > ALBUMS[loaded].tracks) return "";
  const char* s = tabla[f - 1];
  return s ? s : "";
}

static const char* trackName()   { return pistaTexto(false); }
static const char* trackArtist() { return pistaTexto(true);  }

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
#if COVER_AJUSTES_OK
    vinyl_set_custom("AJUSTES", "Del Aparato", COLOR_AJUSTES_AUTO, animate,
                     COVER_AJUSTES);
#else
    vinyl_set_custom("AJUSTES", "Del Aparato", COLOR_AJUSTES, animate);
#endif
    return;
  }
  if (ES_ALARMA(album)) {
    AlarmCfg& c = alarm_cfg();
#if COVER_ALARMA_OK
    vinyl_set_custom("ALARMA", c.activa ? "Activada" : "Apagada",
                     COLOR_ALARMA_AUTO, animate, COVER_ALARMA);
#else
    vinyl_set_custom("ALARMA", c.activa ? "Activada" : "Apagada",
                     COLOR_ALARMA, animate);
#endif
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
    if (!c.activa)      snprintf(t, sizeof(t), "Apagada\nSon las %s", hhmm);
    else if (!clock_ready())
                        snprintf(t, sizeof(t), "%02d:%02d\n%s", c.hora,
                                 c.minuto, clock_status());
    else                snprintf(t, sizeof(t), "%02d:%02d\nSon las %s",
                                 c.hora, c.minuto, hhmm);
    lv_label_set_text(selMeta, t);
    return;
  }
  if (ES_AJUSTES(album)) {
    Ajustes& j = ajustes();
    lv_label_set_text(selName, "AJUSTES");
    char t[64], r[24];
    aj_texto_reposo(r, sizeof(r), j.reposoMin);
    snprintf(t, sizeof(t), "Reposo %s\nLEDs %s", r, aj_texto_leds(j.ledsModo));
    lv_label_set_text(selMeta, t);
    return;
  }
  const Album& a = ALBUMS[album];
  lv_label_set_text(selName, a.name);
  char t[48];
  if (a.tracks == 0) {
    snprintf(t, sizeof(t), "Vacio");
  } else {
    unsigned min = (a.seconds + 30) / 60;
    snprintf(t, sizeof(t), "%u %s\n%u %s", a.tracks,
             a.tracks == 1 ? "Cancion" : "Canciones",
             min, min == 1 ? "Minuto" : "Minutos");
  }
  lv_label_set_text(selMeta, t);
}

static void refreshDots() {
  // El titulo se refresca aqui y no en cada sitio que cambia de pista:
  // arrancar el album, el avance automatico y el doble push pasan los
  // tres por esta funcion. Puesto en el detector de cambio se perdia la
  // PRIMERA cancion, porque ahi no hay cambio que detectar.
  if (nowName) lv_label_set_text(nowName, trackName());
  vinyl_set_dots(ES_ESPECIAL(album) ? 0 : ALBUMS[album].tracks, trackIx,
                 ES_ESPECIAL(album) ? 0xFFFFFF : ALBUMS[album].color);
}

// ── Reloj ────────────────────────────────────────────────────────────────────
// En reposo, en vez de apagarse, la pantalla se vuelve reloj. Es lo que este
// objeto ya era en la practica: vive fijo en un mueble y se mira de pasada.

static void relojPunto(lv_point_t* p, float grados, lv_coord_t r0, lv_coord_t r1) {
  float a = (grados - 90.0f) * PI / 180.0f;
  p[0].x = (lv_coord_t)(180 + r0 * cosf(a));
  p[0].y = (lv_coord_t)(180 + r0 * sinf(a));
  p[1].x = (lv_coord_t)(180 + r1 * cosf(a));
  p[1].y = (lv_coord_t)(180 + r1 * sinf(a));
}

static void refreshReloj() {
  Ajustes& j = ajustes();
  bool analogo = (j.enReposo == REPOSO_ANALOGO);

  struct tm t;
  bool hay = clock_now(&t);

  // Fecha y clima se arman una sola vez y los usan las dos caras: es el mismo
  // dato, y duplicar el formateo es duplicar los errores.
  static const char* DIA[] = { "Domingo", "Lunes", "Martes", "Miercoles",
                               "Jueves", "Viernes", "Sabado" };
  static const char* MES[] = { "Enero", "Febrero", "Marzo", "Abril", "Mayo",
                               "Junio", "Julio", "Agosto", "Septiembre",
                               "Octubre", "Noviembre", "Diciembre" };
  char fecha[40] = "";
  char clima[40] = "";
  char hora[12]  = "--:--";
  if (hay) {
    snprintf(fecha, sizeof(fecha), "%s %d de %s",
             DIA[t.tm_wday % 7], t.tm_mday, MES[t.tm_mon % 12]);
    uint8_t h12 = t.tm_hour % 12; if (!h12) h12 = 12;
    snprintf(hora, sizeof(hora), "%d:%02d", h12, t.tm_min);
  } else {
    snprintf(fecha, sizeof(fecha), "Sin Hora");
  }
  // El grado existe en Montserrat (codigo 176), asi que se puede escribir de
  // verdad y no con una "C" pegada. Sin lectura buena, el renglon queda vacio
  // en vez de mentir: el reloj no depende del clima para servir.
  if (weather_ok())
    snprintf(clima, sizeof(clima), "%d\u00B0  %s", weather_temp(), weather_texto());

  // Digital
  lv_obj_t* dig[3] = { rjFecha, rjHora, rjClima };
  for (auto o : dig) {
    if (analogo) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
  if (!analogo) {
    lv_label_set_text(rjFecha, fecha);
    lv_label_set_text(rjHora, hora);
    lv_label_set_text(rjClima, clima);
  }

  for (lv_obj_t* o : { rjFechaA, rjClimaA }) {
    if (analogo) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
  if (analogo) {
    lv_label_set_text(rjFechaA, fecha);
    lv_label_set_text(rjClimaA, clima);
  }

  // Analogo
  for (uint8_t i = 0; i < 12; i++) {
    if (analogo) lv_obj_clear_flag(rjMarca[i], LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(rjMarca[i], LV_OBJ_FLAG_HIDDEN);
  }
  for (uint8_t i = 0; i < 4; i++) {
    if (analogo) lv_obj_clear_flag(rjNum[i], LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(rjNum[i], LV_OBJ_FLAG_HIDDEN);
  }
  for (uint8_t i = 0; i < 3; i++) {
    if (analogo && hay) lv_obj_clear_flag(rjAguja[i], LV_OBJ_FLAG_HIDDEN);
    else                lv_obj_add_flag(rjAguja[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (analogo && hay) {
    // La aguja de la hora avanza con los minutos: a las 3:30 no apunta al 3,
    // apunta a la mitad entre el 3 y el 4. Un reloj que no hace eso se ve mal
    // sin que uno sepa por que.
    float gh = (t.tm_hour % 12) * 30.0f + t.tm_min * 0.5f;
    float gm = t.tm_min * 6.0f + t.tm_sec * 0.1f;
    float gs = t.tm_sec * 6.0f;
    relojPunto(rjPtAguja[0], gh, 14, 86);
    relojPunto(rjPtAguja[1], gm, 14, 126);
    relojPunto(rjPtAguja[2], gs, -26, 132);
    for (uint8_t i = 0; i < 3; i++) lv_line_set_points(rjAguja[i], rjPtAguja[i], 2);
  }
}

static void goReloj() {
  st = ST_RELOJ;
  refreshReloj();
  fadeTo(selBox, 0, 300);
  fadeTo(playBox, 0, 300);
  fadeTo(vecinoIzq, 0, 300);
  fadeTo(vecinoDer, 0, 300);
  fadeTo(halo, 0, 300);
  fadeTo(relojBox, 255, 500);
  vinyl_zoom_to(20, 500);        // el disco se retira casi por completo
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
      default: snprintf(v, sizeof(v), "%s", c.activa ? "Activada" : "Apagada");
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

// Los seis de luz van juntos y en el orden en que ocurren: primero como se ve
// usandolo, luego cuando se retira, luego el anillo. Al Terminar no es luz y
// por eso queda al final, separado.
static const char* AJ_ROT[AJ_CAMPOS] = { "Brillo", "Reposo", "En Reposo",
                                         "Luz Reposo", "LEDs", "Brillo LEDs",
                                         "Al Terminar" };

static void refreshAjustes() {
  Ajustes& j = ajustes();

  // Solo lectura: no es un campo, es informacion. Por eso vive bajo el titulo
  // y no en la lista que se recorre con la perilla.
  char b[40];
  if (bat_presente()) snprintf(b, sizeof(b), "Bateria %u%%   %u.%02u V",
                               bat_pct(), bat_mv() / 1000, (bat_mv() % 1000) / 10);
  else                snprintf(b, sizeof(b), "Sin Bateria");
  lv_label_set_text(ajBat, b);
  char v[32];
  for (uint8_t i = 0; i < AJ_CAMPOS; i++) {
    switch (i) {
      case 0: snprintf(v, sizeof(v), "%u%%", j.brillo); break;
      case 1: aj_texto_reposo(v, sizeof(v), j.reposoMin); break;
      case 2: snprintf(v, sizeof(v), "%s", aj_texto_en_reposo(j.enReposo)); break;
      case 3: snprintf(v, sizeof(v), "%u%%", j.luzReposo); break;
      case 4: snprintf(v, sizeof(v), "%s", aj_texto_leds(j.ledsModo)); break;
      case 5: snprintf(v, sizeof(v), "%u%%", j.brilloLeds); break;
      default: snprintf(v, sizeof(v), "%s", aj_texto_fin(j.alFin));
    }
    lv_label_set_text(ajVal[i], v);
    bool sel = (i == campo);
    lv_obj_set_style_text_color(ajVal[i], (sel && editando)
        ? lv_color_hex(0xD4A017) : lv_color_white(), 0);
    // Un campo que hoy no hace nada se ve, pero apenas: dice "aqui hay algo,
    // no ahorita". Borrarlo de la lista seria peor, porque la lista cambiaria
    // de largo al mover otro campo.
    if (!aj_campo_activo(i)) {
      lv_obj_set_style_text_opa(ajVal[i], 60, 0);
      lv_obj_set_style_text_opa(ajRot[i], 60, 0);
      continue;
    }
    lv_obj_set_style_text_opa(ajVal[i], sel ? LV_OPA_COVER : 175, 0);
    lv_obj_set_style_text_opa(ajRot[i], sel ? 235 : 150, 0);
  }

  // Siete campos caben enteros en la pantalla, asi que la lista ya no se
  // desliza. Con diez habia que correrla, y esa era la peor parte de Ajustes:
  // el campo elegido se quedaba quieto y el mundo se movia detras.
  if (ajLista) lv_obj_set_y(ajLista, 0);
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
  // El anillo se apaga al dormir, SIEMPRE, tenga reloj la pantalla o no.
  //
  // Antes esto colgaba de que la pantalla llegara a cero, y con el reloj puesto
  // nunca llega: el anillo se quedaba encendido toda la noche a brillo 3 de
  // 255. El razonamiento estaba mal, no el codigo. El reloj es informacion —lo
  // pones para leerlo de noche— y el anillo es decoracion que pertenece al uso.
  // Compartir interruptor solo tiene sentido cuando los dos se apagan.
  //
  // Se corta la corriente de la tira (GPIO17), no solo el brillo: un LED en
  // brillo 0 sigue alimentado y sigue calentando dentro de una caja cerrada.
  if (dormido || blNivel == 0 || ajustes().ledsModo == LED_OFF) {
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

  // Con preprocesador y no con ternario: si la caratula especial no existe, su
  // simbolo tampoco, y el ternario ni siquiera compilaria.
  const uint32_t* pal = nullptr;
  if (ES_AJUSTES(src)) {
#if COVER_AJUSTES_OK
    pal = RING_AJUSTES;
#endif
  } else if (ES_ALARMA(src)) {
#if COVER_ALARMA_OK
    pal = RING_ALARMA;
#endif
  } else {
    pal = ALBUMS[src].anillo;
  }
  switch (ajustes().ledsModo) {

    case LED_RGB: {
      // Arcoiris repartido alrededor del anillo, girando una vuelta cada 30
      // segundos. Lento y continuo a proposito: los saltos de color son lo que
      // hace que el RGB se vea barato.
      uint16_t base = (uint16_t)((millis() % 30000UL) * 65536UL / 30000UL);
      for (int i = 0; i < NUM_LEDS; i++) {
        uint16_t h = (uint16_t)(base + (uint32_t)i * 65536UL / NUM_LEDS);
        ring.setPixelColor(i, ring.gamma32(ring.ColorHSV(h, 255, 255)));
      }
      break;
    }

    case LED_COVER:
      // Cada LED toma el color del sector de la caratula que le queda detras:
      // el anillo como reflejo del arte.
      if (pal) {
        for (int i = 0; i < NUM_LEDS; i++) ring.setPixelColor(i, pal[i % 8]);
      } else {
        uint32_t col = ES_AJUSTES(src) ? COLOR_AJUSTES
                     : ES_ALARMA(src)  ? COLOR_ALARMA : ALBUMS[src].color;
        for (int i = 0; i < NUM_LEDS; i++) ring.setPixelColor(i, col);
      }
      break;

    default:
      for (int i = 0; i < NUM_LEDS; i++) ring.setPixelColor(i, COLOR_AMBAR);
      break;
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
  // Que suena, sin tener que adivinar. El DFPlayer no entrega metadata: estos
  // nombres vienen compilados desde los tags de los MP3 originales.
  //
  // Van en capa fija sobre el disco, no en el disco: la etiqueta gira, y un
  // texto girando no se lee. Y el ancho es 260 y no 300 porque a esta altura
  // la cuerda del circulo ya no da para mas.
  nowName = mkLabel(playBox, &lv_font_montserrat_18, 235, 266);
  lv_obj_set_pos(nowName, 50, 266);
  lv_obj_set_width(nowName, 260);
  lv_label_set_long_mode(nowName, LV_LABEL_LONG_DOT);
  lv_label_set_text(nowName, "");

  timeLbl = mkLabel(playBox, &lv_font_montserrat_14, 165, 293);
  lv_obj_set_pos(timeLbl, 50, 293);
  lv_obj_set_width(timeLbl, 260);
  lv_label_set_long_mode(timeLbl, LV_LABEL_LONG_DOT);
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

  // Reloj de reposo. Digital y analogo comparten caja; se muestra uno u otro.
  relojBox = mkBox(scr);
  {
    lv_obj_t* velo = lv_obj_create(relojBox);
    lv_obj_remove_style_all(velo);
    lv_obj_set_size(velo, 360, 360);
    lv_obj_center(velo);
    lv_obj_set_style_radius(velo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(velo, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(velo, LV_OPA_COVER, 0);

    // La hora va en una Montserrat de 78 generada aparte: LVGL solo trae hasta
    // 48. Solo lleva digitos, dos puntos y guion, asi que pesa 76KB en vez de
    // los 400KB que costaria un juego completo de caracteres a ese tamano.
    //
    // Tres renglones: que dia es, que hora es, y que tiempo hace. Sin AM/PM —
    // en un reloj de escritorio nadie duda de si son las tres de la tarde o de
    // la madrugada, y ese texto solo restaba tamano a lo que si importa.
    rjFecha = mkLabel(relojBox, &lv_font_montserrat_20, 150, 116);
    rjHora  = mkLabel(relojBox, &vnl_reloj_78, LV_OPA_COVER, 152);
    rjClima = mkLabel(relojBox, &lv_font_montserrat_20, LV_OPA_COVER, 222);
    lv_obj_set_style_text_color(rjClima, lv_color_hex(COLOR_AMBAR), 0);

    // La cara analoga lleva los mismos dos datos, mas chicos y mas tenues: ahi
    // el protagonista son las agujas. Van entre los numerales y el centro, que
    // es donde un reloj de verdad pone su ventanilla.
    rjFechaA = mkLabel(relojBox, &lv_font_montserrat_14, 130, 116);
    rjClimaA = mkLabel(relojBox, &lv_font_montserrat_14, 170, 224);
    lv_obj_set_style_text_color(rjClimaA, lv_color_hex(COLOR_AMBAR), 0);

    // Indices: mas largos y claros en las cuatro horas cardinales.
    for (uint8_t i = 0; i < 12; i++) {
      bool cardinal = (i % 3 == 0);
      relojPunto(rjPtMarca[i], i * 30.0f, cardinal ? 132 : 140, 152);
      rjMarca[i] = lv_line_create(relojBox);
      lv_obj_set_pos(rjMarca[i], 0, 0);
      lv_obj_set_style_line_color(rjMarca[i], lv_color_white(), 0);
      lv_obj_set_style_line_width(rjMarca[i], cardinal ? 4 : 2, 0);
      lv_obj_set_style_line_opa(rjMarca[i], cardinal ? 230 : 120, 0);
      lv_obj_set_style_line_rounded(rjMarca[i], true, 0);
      lv_line_set_points(rjMarca[i], rjPtMarca[i], 2);
      lv_obj_add_flag(rjMarca[i], LV_OBJ_FLAG_HIDDEN);
    }
    // Horas y minutos en blanco, segundero en ambar: el unico elemento que se
    // mueve lleva el color de la caja.
    const lv_coord_t ANCHO[3] = { 7, 5, 2 };
    for (uint8_t i = 0; i < 3; i++) {
      rjAguja[i] = lv_line_create(relojBox);
      lv_obj_set_pos(rjAguja[i], 0, 0);
      lv_obj_set_style_line_color(rjAguja[i],
          i == 2 ? lv_color_hex(COLOR_AMBAR) : lv_color_white(), 0);
      lv_obj_set_style_line_width(rjAguja[i], ANCHO[i], 0);
      lv_obj_set_style_line_rounded(rjAguja[i], true, 0);
      lv_obj_add_flag(rjAguja[i], LV_OBJ_FLAG_HIDDEN);
    }
    // Los cuatro numerales del mockup, por dentro del anillo de indices.
    static const char* NUM[4] = { "12", "3", "6", "9" };
    for (uint8_t i = 0; i < 4; i++) {
      float a = (i * 90.0f - 90.0f) * PI / 180.0f;
      lv_coord_t cx = (lv_coord_t)(180 + 108 * cosf(a));
      lv_coord_t cy = (lv_coord_t)(180 + 108 * sinf(a));
      rjNum[i] = lv_label_create(relojBox);
      lv_obj_set_style_text_font(rjNum[i], &lv_font_montserrat_20, 0);
      lv_obj_set_style_text_color(rjNum[i], lv_color_white(), 0);
      lv_obj_set_style_text_opa(rjNum[i], 220, 0);
      lv_obj_set_style_text_align(rjNum[i], LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_width(rjNum[i], 44);
      lv_obj_set_pos(rjNum[i], cx - 22, cy - 13);
      lv_label_set_text(rjNum[i], NUM[i]);
      lv_obj_add_flag(rjNum[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t* eje = lv_obj_create(relojBox);
    lv_obj_remove_style_all(eje);
    lv_obj_set_size(eje, 10, 10);
    lv_obj_center(eje);
    lv_obj_set_style_radius(eje, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eje, lv_color_hex(COLOR_AMBAR), 0);
    lv_obj_set_style_bg_opa(eje, LV_OPA_COVER, 0);
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
    lv_obj_set_pos(tit, 30, 32);
    lv_label_set_text(tit, "AJUSTES");

    ajBat = lv_label_create(ajBox);
    lv_obj_set_style_text_font(ajBat, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ajBat, lv_color_white(), 0);
    lv_obj_set_style_text_opa(ajBat, 140, 0);
    lv_obj_set_style_text_align(ajBat, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ajBat, 300);
    lv_obj_set_pos(ajBat, 30, 60);
    lv_label_set_text(ajBat, "");

    // Contenedor de la lista. Ya no se desliza: los siete campos caben, y la
    // fila mas baja queda dentro de la cuerda del circulo a esa altura.
    ajLista = lv_obj_create(ajBox);
    lv_obj_remove_style_all(ajLista);
    lv_obj_set_size(ajLista, 360, 86 + AJ_CAMPOS * AJ_FILA_H);
    lv_obj_set_pos(ajLista, 0, 0);
    lv_obj_clear_flag(ajLista, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < AJ_CAMPOS; i++) {
      lv_coord_t y = 86 + i * AJ_FILA_H;
      ajRot[i] = lv_label_create(ajLista);
      lv_obj_set_style_text_font(ajRot[i], &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(ajRot[i], lv_color_white(), 0);
      lv_obj_set_width(ajRot[i], 130);
      lv_obj_set_pos(ajRot[i], 52, y);
      lv_label_set_text(ajRot[i], AJ_ROT[i]);

      ajVal[i] = lv_label_create(ajLista);
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

  // Del reloj se sale con cualquier gesto, y ese gesto SOLO saca: si no,
  // alcanzas la perilla para ver la hora de cerca y acabas cambiando de disco.
  if (st == ST_RELOJ) {
    fadeTo(relojBox, 0, 260);
    if (trackIx && !paused) goPlaying(false);
    else                    goSelector();
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
        if (!editando) {
          // Salta los campos que ahora mismo no controlan nada. El tope de
          // vueltas es por si algun dia todos quedaran inactivos: la perilla
          // se queda donde esta, nunca se cuelga.
          for (uint8_t n = 0; n < AJ_CAMPOS; n++) {
            campo = (uint8_t)((campo + AJ_CAMPOS + d) % AJ_CAMPOS);
            if (aj_campo_activo(campo)) break;
          }
        }
        else switch (campo) {
          case 0: j.brillo     = aj_ciclo_pct(j.brillo, d, 30);     break;
          case 1: j.reposoMin  = aj_ciclo_reposo(j.reposoMin, d);   break;
          case 2: j.enReposo   = (uint8_t)((j.enReposo + 3 + d) % 3); break;
          case 3: j.luzReposo  = aj_ciclo_pct(j.luzReposo, d, 20);  break;
          case 4: j.ledsModo   = (uint8_t)((j.ledsModo + 4 + d) % 4); break;
          case 5: j.brilloLeds = aj_ciclo_pct(j.brilloLeds, d, 10); break;
          default: j.alFin     = (uint8_t)((j.alFin + 3 + d) % 3);
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
        fadeTo(nowName, 0, 120);
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
    uint32_t umbral = j.reposoMin ? (uint32_t)j.reposoMin * 60000UL : 0xFFFFFFFF;
    bool enReposo = (idle >= umbral);

    // Reposo: UNA sola condicion, la perilla sin tocar, y vale desde donde
    // sea — biblioteca, reproduciendo, en pausa, dentro de Ajustes o de la
    // alarma. Antes dormia solo desde la biblioteca y en silencio, y por eso
    // un album en pausa se quedaba encendido toda la noche: el mismo minuto
    // de abandono hacia cosas distintas segun donde te hubieras quedado.
    //
    // El flag hace que esto ocurra en la transicion y no en cada pasada: sin
    // el, con la pantalla en Apagar se llamaria a ajustes_save() miles de
    // veces por minuto y eso desgasta la NVS.
    if (enReposo && !dormido) {
      dormido = true;
      // Ajustes y la alarma solo guardan al pulsar. Si te vas a medio editar,
      // el reposo confirma en vez de descartar: nadie deja un campo a la
      // mitad esperando perderlo.
      if (st == ST_AJUSTES) { editando = false; ajustes_save(); }
      if (st == ST_ALARMA)  { editando = false; alarm_save();   }
      if (j.enReposo != REPOSO_APAGAR && st != ST_RELOJ) goReloj();
      paintRing();          // el anillo se va de inmediato, no cuando cambie el brillo
    } else if (!enReposo && dormido) {
      dormido = false;
      paintRing();          // y vuelve al despertar, aunque el brillo no cambie
    }

    // Una sola regla: en uso mandas tu Brillo; en reposo se atenua a una
    // fraccion DE ese brillo. Al ser relativo, bajar el brillo baja las dos
    // cosas y el reposo nunca puede quedar mas claro que el uso — que es lo
    // que pasaba cuando los dos niveles eran absolutos e independientes.
    // Apagar apaga siempre, con musica o sin ella: quien lo elige quiere el
    // cuarto a oscuras, y una luz que respeta eso a medias no lo respeta.
    uint8_t quiero = j.brillo;
    if (enReposo) {
      if (j.enReposo == REPOSO_APAGAR) quiero = 0;
      else {
        quiero = (uint8_t)((uint32_t)j.brillo * j.luzReposo / 100);
        // Piso absoluto, no relativo: por debajo de esto el panel deja de
        // leerse, y eso es una propiedad del hardware, no del gusto. Sin el,
        // un brillo bajo multiplicado por una luz de reposo baja aterriza en
        // un reloj puesto que nadie puede ver — que es justo lo que paso.
        if (quiero < 15) quiero = 15;
      }
    }
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
    if (st == ST_PLAYING) { fadeTo(timeLbl, 165, 240); fadeTo(nowName, 235, 240); }
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

    if (st == ST_RELOJ) refreshReloj();

    if (st == ST_PLAYING && !volShownMs) {
      char t[12];
      fmtTime(t, sizeof(t), elapsedS());
      // El artista comparte renglon con el tiempo. Son dos datos chicos y
      // separarlos en dos lineas mas dejaria la banda con cuatro pisos.
      const char* ar = trackArtist();
      char linea[72];
      if (ar && *ar) snprintf(linea, sizeof(linea), "%s  -  %s", ar, t);
      else           snprintf(linea, sizeof(linea), "%s", t);
      lv_label_set_text(timeLbl, linea);
    }
  }

  // El arcoiris avanza solo, asi que necesita refresco constante aunque no
  // haya musica ni eventos.
  if ((trackIx && !paused) || ajustes().ledsModo == LED_RGB) paintRing();
}

AppState app_state() { return st; }

// Solo para depurar: que nivel de retroiluminacion cree el firmware que tiene,
// y si considera que hay musica sonando.
uint8_t app_bl_dbg()      { return blNivel; }
bool    app_sonando_dbg() { return trackIx && !paused; }
