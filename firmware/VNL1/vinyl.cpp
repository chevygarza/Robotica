#include "vinyl.h"
#include "albums.h"

// Todo el layout respeta el circulo de 360x360: nada vive en las esquinas.
#define DISC_D    VINYL_DISC_D
#define LABEL_D   VINYL_LABEL_D
#define HOLE_R      5    // agujero del eje

// Velocidad estilizada, no los 33 1/3 RPM reales: a 200 grados/s la etiqueta
// avanza 8 grados por frame y el texto se ve escalonado. 22 RPM se lee como
// giro y se mantiene suave.
#define VINYL_RPM  22.0f

#define SHEEN_SPAN  54   // ancho angular del highlight, en grados
#define SHEEN_STEP  38   // cuanto se corre por muesca de la perilla

static lv_obj_t*   disc   = nullptr;   // canvas con disco + surcos (estatico)
static lv_obj_t*   sheen  = nullptr;   // arco de luz
static lv_obj_t*   label  = nullptr;   // canvas de la etiqueta (rota)
static lv_color_t* discBuf  = nullptr;
static uint8_t*    labelBuf = nullptr;

static uint8_t  curAlbum   = 0;
static bool     spinWanted = false;
static float    rpmCur     = 0.0f;     // inercia: el disco no arranca ni para en seco
static float    angle      = 0.0f;     // grados
static float    sheenCur   = 300.0f;   // luz entrando por arriba-izquierda
static float    sheenTgt   = 300.0f;
static uint32_t lastMs     = 0;
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
  lv_canvas_fill_bg(disc, lv_color_black(), LV_OPA_COVER);

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

  // Surcos desde el filo de la etiqueta hasta casi el borde: mas apretados y
  // tenues hacia afuera, como un vinilo real.
  for (lv_coord_t r = LABEL_D / 2 + 3; r < c - 3; r += 3) {
    float k = (float)(r - LABEL_D / 2) / (float)(c - LABEL_D / 2);
    g.opa = (lv_opa_t)(34 - 16 * k);
    lv_canvas_draw_arc(disc, c, c, r, 0, 360, &g);
  }

  // Canto del disco: un aro claro afuera y una sombra apenas adentro. Es lo que
  // le da volumen y marca donde termina el vinilo.
  g.opa   = 90;
  g.width = 2;
  lv_canvas_draw_arc(disc, c, c, c - 2, 0, 360, &g);
  g.color = lv_color_black();
  g.opa   = 70;
  g.width = 3;
  lv_canvas_draw_arc(disc, c, c, c - 6, 0, 360, &g);
}

// ── Etiqueta: se redibuja al cambiar de album ────────────────────────────────
static void drawLabel(uint8_t idx) {
  const Album& a = ALBUMS[idx];

  lv_canvas_fill_bg(label, lv_color_black(), LV_OPA_TRANSP);

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

  lv_draw_label_dsc_t t;
  lv_draw_label_dsc_init(&t);
  t.color        = lv_color_white();
  t.font         = &lv_font_montserrat_20;
  t.align        = LV_TEXT_ALIGN_CENTER;
  t.letter_space = 2;
  lv_canvas_draw_text(label, 0, c - 34, LABEL_D, &t, a.name);

  lv_draw_label_dsc_t s;
  lv_draw_label_dsc_init(&s);
  s.color        = lv_color_white();
  s.opa          = 200;          // al 60% se perdia sobre el verde
  s.font         = &lv_font_montserrat_12;
  s.align        = LV_TEXT_ALIGN_CENTER;
  s.letter_space = 1;
  lv_canvas_draw_text(label, 10, c + 12, LABEL_D - 20, &s, a.subtitle);

  // Agujero del eje, al final para que quede encima del texto.
  lv_draw_rect_dsc_t hole;
  lv_draw_rect_dsc_init(&hole);
  hole.radius   = LV_RADIUS_CIRCLE;
  hole.bg_opa   = LV_OPA_COVER;
  hole.bg_color = lv_color_black();
  lv_canvas_draw_rect(label, c - HOLE_R, c - HOLE_R, HOLE_R * 2, HOLE_R * 2, &hole);
}

bool vinyl_create(lv_obj_t* parent) {
  lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

  // El canvas del disco vive en PSRAM: son 226KB y se escribe una sola vez.
  discBuf = (lv_color_t*)heap_caps_malloc(
      LV_CANVAS_BUF_SIZE_TRUE_COLOR(DISC_D, DISC_D), MALLOC_CAP_SPIRAM);
  // La etiqueta va en RAM interna: LVGL la lee pixel por pixel en cada
  // rotacion, y desde PSRAM eso cuesta el doble.
  labelBuf = (uint8_t*)heap_caps_malloc(
      LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(LABEL_D, LABEL_D), MALLOC_CAP_INTERNAL);
  if (!discBuf || !labelBuf) {
    Serial.println("ERROR: no alcanzo la memoria para los canvas del vinilo");
    return false;
  }

  disc = lv_canvas_create(parent);
  lv_canvas_set_buffer(disc, discBuf, DISC_D, DISC_D, LV_IMG_CF_TRUE_COLOR);
  lv_obj_center(disc);
  drawDisc();

  // Highlight especular. Es un arco ancho a muy baja opacidad: se lee como luz
  // resbalando sobre los surcos. Se queda quieto mientras el disco gira (como
  // pasa de verdad con una lampara fija) y se corre al navegar con la perilla.
  sheen = lv_arc_create(parent);
  lv_obj_set_size(sheen, DISC_D - 8, DISC_D - 8);
  lv_obj_center(sheen);
  lv_obj_remove_style(sheen, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(sheen, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(sheen, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_opa(sheen, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(sheen, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_color(sheen, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(sheen, 18, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(sheen, (DISC_D - LABEL_D) / 2, LV_PART_INDICATOR);
  lv_arc_set_bg_angles(sheen, 0, 360);
  lv_arc_set_angles(sheen, (uint16_t)sheenCur, (uint16_t)sheenCur + SHEEN_SPAN);

  label = lv_canvas_create(parent);
  lv_canvas_set_buffer(label, labelBuf, LABEL_D, LABEL_D, LV_IMG_CF_TRUE_COLOR_ALPHA);
  lv_obj_center(label);
  lv_img_set_pivot(label, LABEL_D / 2, LABEL_D / 2);
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

void vinyl_set_album(uint8_t idx, bool animate) {
  if (idx >= ALBUM_COUNT) return;
  curAlbum = idx;
  drawLabel(idx);
  lv_obj_invalidate(label);
  if (animate) fadeInLabel();
}

void vinyl_nudge_sheen(int8_t dir) {
  sheenTgt += (dir >= 0 ? SHEEN_STEP : -SHEEN_STEP);
  while (sheenTgt < 0)    sheenTgt += 360.0f;
  while (sheenTgt >= 360) sheenTgt -= 360.0f;
  // Camino mas corto: sin esto, ir de 350 a 10 grados barre 340 grados.
  if (sheenCur - sheenTgt >  180.0f) sheenTgt += 360.0f;
  if (sheenTgt - sheenCur >  180.0f) sheenTgt -= 360.0f;
}

void vinyl_set_spinning(bool on) {
  spinWanted = on;
}

bool vinyl_spinning() { return spinWanted; }
bool vinyl_moving()   { return rpmCur > 0.15f; }

void vinyl_bump() {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, label);
  lv_anim_set_values(&a, 256, 244);          // 256 = 100% en lv_img_set_zoom
  lv_anim_set_time(&a, 90);
  lv_anim_set_playback_time(&a, 90);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
    lv_img_set_zoom((lv_obj_t*)obj, (uint16_t)v);
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

  // Suavizado exponencial: da easing sin manejar animaciones a mano.
  if (fabsf(sheenTgt - sheenCur) > 0.4f) {
    sheenCur += (sheenTgt - sheenCur) * 0.18f;
    float a = sheenCur;
    while (a < 0)    a += 360.0f;
    while (a >= 360) a -= 360.0f;
    lv_arc_set_angles(sheen, (uint16_t)a, (uint16_t)a + SHEEN_SPAN);
  }
}
