#include "vinyl.h"
#include "albums.h"
#include <string.h>

// Todo el layout respeta el circulo de 360x360: nada vive en las esquinas.
#define DISC_D    VINYL_DISC_D
#define LABEL_D   VINYL_LABEL_D
#define HOLE_R      6    // agujero del eje

// Velocidad estilizada, no los 33 1/3 RPM reales: a 200 grados/s la etiqueta
// avanza 8 grados por frame y el texto se ve escalonado. 22 RPM se lee como
// giro y se mantiene suave.
#define VINYL_RPM  22.0f

static lv_obj_t*   disc   = nullptr;   // canvas con disco + surcos (estatico)
static lv_obj_t*   label  = nullptr;   // canvas de la etiqueta (rota)
static uint8_t*    discBuf  = nullptr;
static uint8_t*    labelBuf = nullptr;

static uint8_t  curAlbum   = 0;
static bool     spinWanted = false;
static float    rpmCur     = 0.0f;     // inercia: el disco no arranca ni para en seco
static float    angle      = 0.0f;     // grados
static uint32_t lastMs     = 0;
static uint16_t baseZoom   = 256;      // 256 = 100%
static uint16_t bumpZoom   = 256;
static int16_t  lastSent   = -1;       // ultimo angulo enviado a LVGL, en 0.1 grados

// En LVGL 8.3 el gradiente de un draw_dsc va en bg_grad (con stops), no en los
// bg_grad_color / bg_grad_dir que si existen en los estilos de objeto.
static void setGrad(lv_draw_rect_dsc_t& d, lv_color_t from, lv_color_t to) {
  d.bg_color                = from;
  d.bg_grad.dir             = LV_GRAD_DIR_VER;
  d.bg_grad.stops_count     = 2;
  d.bg_grad.stops[0].color  = from;
  d.bg_grad.stops[0].frac   = 0;
  d.bg_grad.stops[1].color  = to;
  d.bg_grad.stops[1].frac   = 255;
}

// ── Disco: se dibuja UNA sola vez ────────────────────────────────────────────
// Los surcos son circulos concentricos, y un circulo se ve identico girado. Por
// eso nada de esto se redibuja por frame: solo la etiqueta rota.
static void drawDisc() {
  lv_canvas_fill_bg(disc, lv_color_black(), LV_OPA_TRANSP);

  lv_draw_rect_dsc_t body;
  lv_draw_rect_dsc_init(&body);
  // El cuerpo tiene que separarse del fondo negro o el disco no se lee como
  // objeto: con #141414 sobre negro puro la silueta desaparece y el ojo solo
  // ve la banda de surcos, que se siente un disco chico flotando.
  body.radius = LV_RADIUS_CIRCLE;
  body.bg_opa = LV_OPA_COVER;
  setGrad(body, lv_color_hex(0x262626), lv_color_hex(0x131313));
  lv_canvas_draw_rect(disc, 0, 0, DISC_D, DISC_D, &body);

  const lv_coord_t c = DISC_D / 2;

  lv_draw_arc_dsc_t g;
  lv_draw_arc_dsc_init(&g);
  g.color = lv_color_white();
  g.width = 1;

  // Un vinilo tiene tres zonas: borde liso por donde entra la aguja, la zona
  // grabada con los surcos, y otra banda lisa antes de la etiqueta. Los surcos
  // van finos y parejos; el disco real no tiene brillos ni manchas.
  const lv_coord_t rOut = c - 15;
  const lv_coord_t rIn  = LABEL_D / 2 + 11;
  for (lv_coord_t r = rIn; r <= rOut; r += 2) {
    float k = (float)(r - rIn) / (float)(rOut - rIn);
    g.opa = (lv_opa_t)(24 - 10 * k);
    lv_canvas_draw_arc(disc, c, c, r, 0, 360, &g);
  }

  // Canto: filo claro afuera y sombra apenas adentro, para darle grosor.
  g.opa = 95; g.width = 2;
  lv_canvas_draw_arc(disc, c, c, c - 2, 0, 360, &g);
  g.color = lv_color_black(); g.opa = 80; g.width = 3;
  lv_canvas_draw_arc(disc, c, c, c - 7, 0, 360, &g);

  // Surco de salida: el aro marcado que rodea la etiqueta en cualquier disco.
  g.color = lv_color_white(); g.opa = 55; g.width = 1;
  lv_canvas_draw_arc(disc, c, c, LABEL_D / 2 + 6, 0, 360, &g);
  g.opa = 30;
  lv_canvas_draw_arc(disc, c, c, LABEL_D / 2 + 4, 0, 360, &g);
}

// ── Etiqueta: se redibuja al cambiar de album ────────────────────────────────
// Cuando el disco trae caratula, la imagen ES la etiqueta: se vuelca pixel a
// pixel con recorte circular. Es lo unico que se dibuja distinto; el agujero
// del eje se pone despues igual que siempre.
static void drawCover(const uint16_t* cover) {
  const lv_coord_t c = LABEL_D / 2;
  const int32_t r2 = (int32_t)(c - 1) * (c - 1);
  for (lv_coord_t y = 0; y < LABEL_D; y++) {
    int32_t dy = y - c;
    for (lv_coord_t x = 0; x < LABEL_D; x++) {
      int32_t dx = x - c;
      if (dx * dx + dy * dy > r2) continue;      // fuera del circulo: transparente
      lv_color_t col;
      col.full = cover[y * LABEL_D + x];
      lv_canvas_set_px_color(label, x, y, col);
      lv_canvas_set_px_opa(label, x, y, LV_OPA_COVER);
    }
  }
}

// El dibujo de la etiqueta no necesita saber de albumes: solo texto y color.
static void drawLabelRaw(const char* name, const char* sub, uint32_t color,
                         const uint16_t* cover) {
  struct { const char* name; const char* subtitle; uint32_t color;
           const uint16_t* cover; } a = { name, sub, color, cover };

  lv_canvas_fill_bg(label, lv_color_black(), LV_OPA_TRANSP);

  if (a.cover) {
    drawCover(a.cover);
    lv_draw_rect_dsc_t h2;
    lv_draw_rect_dsc_init(&h2);
    h2.radius   = LV_RADIUS_CIRCLE;
    h2.bg_opa   = LV_OPA_COVER;
    h2.bg_color = lv_color_black();
    const lv_coord_t cc = LABEL_D / 2;
    lv_canvas_draw_rect(label, cc - HOLE_R, cc - HOLE_R,
                        HOLE_R * 2, HOLE_R * 2, &h2);
    return;
  }

  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  d.bg_opa = LV_OPA_COVER;
  setGrad(d, lv_color_hex(a.color), lv_color_darken(lv_color_hex(a.color), 70));
  lv_canvas_draw_rect(label, 0, 0, LABEL_D, LABEL_D, &d);

  const lv_coord_t c = LABEL_D / 2;

  lv_draw_arc_dsc_t ring;
  lv_draw_arc_dsc_init(&ring);
  ring.color = lv_color_white();
  ring.opa   = 40;
  ring.width = 1;
  lv_canvas_draw_arc(label, c, c, c - 5, 0, 360, &ring);

  // El nombre manda: nombres cortos respiran a 26, los largos bajan a 22 para
  // no partirse en dos renglones dentro de un circulo.
  lv_draw_label_dsc_t t;
  lv_draw_label_dsc_init(&t);
  t.color        = lv_color_white();
  t.font         = strlen(a.name) > 7 ? &lv_font_montserrat_22
                                      : &lv_font_montserrat_26;
  t.align        = LV_TEXT_ALIGN_CENTER;
  t.letter_space = 2;
  lv_canvas_draw_text(label, 0, c - 46, LABEL_D, &t, a.name);

  lv_draw_label_dsc_t s;
  lv_draw_label_dsc_init(&s);
  s.color        = lv_color_white();
  s.opa          = 200;          // al 60% se perdia sobre el verde
  s.font         = &lv_font_montserrat_16;
  s.align        = LV_TEXT_ALIGN_CENTER;
  s.letter_space = 1;
  lv_canvas_draw_text(label, 12, c + 18, LABEL_D - 24, &s, a.subtitle);

  // Agujero del eje, al final para que quede encima del texto.
  lv_draw_rect_dsc_t hole;
  lv_draw_rect_dsc_init(&hole);
  hole.radius   = LV_RADIUS_CIRCLE;
  hole.bg_opa   = LV_OPA_COVER;
  hole.bg_color = lv_color_black();
  lv_canvas_draw_rect(label, c - HOLE_R, c - HOLE_R, HOLE_R * 2, HOLE_R * 2, &hole);
}

static void drawLabel(uint8_t idx);   // definida mas abajo

bool vinyl_create(lv_obj_t* parent) {
  lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

  // El canvas del disco vive en PSRAM: son 226KB y se escribe una sola vez.
  // Con alfa, no en color plano: el lienzo es cuadrado y el disco redondo, asi
  // que las esquinas TIENEN que ser transparentes o se ve el recuadro encima
  // del fondo en cuanto la camara se aleja.
  discBuf = (uint8_t*)heap_caps_malloc(
      LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(DISC_D, DISC_D), MALLOC_CAP_SPIRAM);
  // La etiqueta va en RAM interna: LVGL la lee pixel por pixel en cada
  // rotacion, y desde PSRAM eso cuesta el doble.
  labelBuf = (uint8_t*)heap_caps_malloc(
      LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(LABEL_D, LABEL_D), MALLOC_CAP_INTERNAL);
  if (!discBuf || !labelBuf) {
    Serial.println("ERROR: no alcanzo la memoria para los canvas del vinilo");
    return false;
  }

  disc = lv_canvas_create(parent);
  lv_canvas_set_buffer(disc, discBuf, DISC_D, DISC_D, LV_IMG_CF_TRUE_COLOR_ALPHA);
  lv_obj_center(disc);
  drawDisc();


  label = lv_canvas_create(parent);
  lv_canvas_set_buffer(label, labelBuf, LABEL_D, LABEL_D, LV_IMG_CF_TRUE_COLOR_ALPHA);
  lv_obj_center(label);
  lv_img_set_pivot(label, LABEL_D / 2, LABEL_D / 2);
  lv_img_set_pivot(disc, DISC_D / 2, DISC_D / 2);
  lv_img_set_antialias(label, true);
  drawLabel(curAlbum);

  lastMs = millis();
  return true;
}

static void fadeInLabel() {
  lv_obj_set_style_img_opa(label, LV_OPA_40, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, label);
  lv_anim_set_values(&a, LV_OPA_40, LV_OPA_COVER);
  lv_anim_set_time(&a, 220);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
    lv_obj_set_style_img_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
  });
  lv_anim_start(&a);
}

static void drawLabel(uint8_t idx) {
  const Album& a = ALBUMS[idx];
  drawLabelRaw(a.name, a.subtitle, a.color, a.cover);
}

void vinyl_set_custom(const char* name, const char* sub, uint32_t color,
                      bool animate) {
  drawLabelRaw(name, sub, color, nullptr);
  lv_obj_invalidate(label);
  if (animate) fadeInLabel();
}

void vinyl_set_album(uint8_t idx, bool animate) {
  if (idx >= ALBUM_COUNT) return;
  curAlbum = idx;
  drawLabel(idx);
  lv_obj_invalidate(label);
  if (animate) fadeInLabel();
}

void vinyl_set_spinning(bool on) {
  spinWanted = on;
}

bool vinyl_spinning() { return spinWanted; }
bool vinyl_moving()   { return rpmCur > 0.15f; }

// Escala el conjunto disco + luz + etiqueta. Es lo que separa visualmente
// "estoy dentro de un disco" de "estoy hojeando la biblioteca": en el selector
// la camara se aleja y caben los vecinos; al reproducir se acerca y el vinilo
// llena el cuadro, solo.
static void applyZoom() {
  if (!disc) return;
  lv_img_set_zoom(disc, baseZoom);
  lv_img_set_zoom(label, (uint16_t)((uint32_t)baseZoom * bumpZoom / 256));
}

void vinyl_zoom_to(uint8_t pct, uint16_t ms) {
  uint16_t tgt = (uint16_t)(256UL * pct / 100);
  lv_anim_del(&baseZoom, nullptr);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, &baseZoom);
  lv_anim_set_values(&a, baseZoom, tgt);
  lv_anim_set_time(&a, ms);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_set_exec_cb(&a, [](void* v, int32_t val) {
    baseZoom = (uint16_t)val;
    applyZoom();
  });
  lv_anim_start(&a);
}

uint8_t vinyl_zoom() { return (uint8_t)(baseZoom * 100UL / 256); }

void vinyl_bump() {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, label);
  lv_anim_set_values(&a, 256, 244);          // 256 = 100% en lv_img_set_zoom
  lv_anim_set_time(&a, 90);
  lv_anim_set_playback_time(&a, 90);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
    bumpZoom = (uint16_t)v;
    applyZoom();
  });
  lv_anim_start(&a);
}

void vinyl_tick() {
  uint32_t now = millis();
  float dt = (now - lastMs) / 1000.0f;
  lastMs = now;
  if (dt > 0.25f) dt = 0.25f;              // tras una pausa larga, no saltes

  // Rampa de velocidad: un vinilo real tarda en tomar vuelo y en frenar. El
  // suavizado exponencial da esa inercia con una linea.
  float rpmTgt = spinWanted ? VINYL_RPM : 0.0f;
  rpmCur += (rpmTgt - rpmCur) * (1.0f - expf(-dt / 0.45f));
  if (!spinWanted && rpmCur < 0.05f) rpmCur = 0.0f;

  if (rpmCur > 0.0f) {
    angle += rpmCur * 6.0f * dt;           // RPM -> grados/segundo
    while (angle >= 360.0f) angle -= 360.0f;
    int16_t sent = (int16_t)(angle * 10.0f);
    if (sent != lastSent) {
      lastSent = sent;
      lv_img_set_angle(label, sent);
    }
  }

}
