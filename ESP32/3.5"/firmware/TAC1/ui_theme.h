// ============================================================================
//  TAC-1 — INTERFAZ.md de StuntHub hecho codigo, para un lienzo apaisado
// ============================================================================
// Mismo ADN que StuntHub (~/Desktop/CROWN32/StuntHub/ui_theme.h) y VinilOS:
// un acento, jerarquia por opacidad, Montserrat en una escala cerrada, tiempos
// fijos. Lo que cambia es el lienzo: 480x320 rectangular y TACTIL, asi que la
// reticula tiene filas mas altas (dedo, no perilla) y aqui si existen las
// esquinas.
//
// Las pantallas NO escriben fuentes, alturas, radios ni hex. Si hace falta un
// numero nuevo, se agrega AQUI y se documenta en el INTERFAZ.
#pragma once
#include <lvgl.h>

// ---------------------------------------------------------------- Color ----
#define COL_BG      lv_color_hex(0x0B1020)   // fondo de toda pantalla de datos
#define COL_TXT     lv_color_hex(0xFFFFFF)   // siempre blanco: jerarquia = opacidad
#define COL_ACCENT  lv_color_hex(0xFF7A10)   // ambar. Lo vivo: lo que editas, lo que corre
#define COL_OK      lv_color_hex(0x3DD68C)   // ESTADO, nunca decoracion
#define COL_BAD     lv_color_hex(0xFF5B6E)   // ESTADO, nunca decoracion
#define COL_WARN    lv_color_hex(0xFFB454)   // ESTADO, nunca decoracion
#define COL_SURFACE lv_color_hex(0x161C2E)   // tarjeta, SOLO si agrupa algo
// El inicio es la unica pantalla con wallpaper: es contenido inmersivo, como
// la Musica en StuntHub. Toda pantalla que muestre datos va sobre COL_BG.

// ------------------------------------------------------------- Opacidad ----
#define OPA_MAIN        255   // el dato principal
#define OPA_AVAIL       175   // dato disponible, no elegido
#define OPA_CONTEXT     150   // contexto
#define OPA_STATE       140   // renglon de estado permanente
#define OPA_INACTIVE     60   // existe, pero hoy no hace nada
#define OPA_SURFACE      20   // fondo de una superficie tactil sobre el wallpaper
#define OPA_KEY          15   // fondo de una tecla
#define OPA_SEPARATOR    15   // la linea entre filas de un grupo

// ----------------------------------------------------------- Tipografia ----
// Cuatro de familia + dos de cifra, como StuntHub. La hora grande es una
// fuente generada aparte (tools/make_font.sh), solo digitos y dos puntos.
#define UI_FONT_TITLE   &lv_font_montserrat_22
#define UI_FONT_DATA    &lv_font_montserrat_18
#define UI_FONT_ROW     &lv_font_montserrat_16
#define UI_FONT_STATE   &lv_font_montserrat_14
#define UI_FONT_FIG_MD  &lv_font_montserrat_28
#define UI_FONT_FIG_XL  &lv_font_montserrat_48  // la cifra protagonista: UNA por pantalla
#define UI_FONT_CLOCK   &tac_reloj_96
// Todo en ASCII: Montserrat de LVGL no trae acentos. Capitalizado, no VERSALITAS.

// --------------------------------------------------------------- Layout ----
#define UI_MARGIN         24   // aire contra el borde de la pantalla
#define UI_Y_TITLE        10   // caja del titulo: su centro queda en y=22
#define UI_Y_STATE        44   // caja del estado: su centro queda en y=52
#define UI_Y_ROW0         80   // primera fila de lista
#define UI_ROW_H          44   // alto de fila: es un objetivo para el dedo
#define UI_ROWS_MAX        5   // caben 5 sin deslizar
#define UI_CONTENT_W     432   // ancho de contenido, centrado (480 - 2*24)
#define UI_TAP_MIN        44   // ningun objetivo tactil mas chico que esto
#define UI_RADIUS         14
#define UI_PILL_RADIUS    18
#define UI_DOTS_Y        -11   // indicador de apps: solo en las pantallas del carrusel
#define UI_DOT            6
#define UI_DOT_GAP       10

// ------------------------------------------------------------ Movimiento ----
// Todo ease_in_out, nada lineal. Se va lento, vuelve rapido.
#define UI_MS_EXPECTED   120   // aparecer algo que ya esperabas
#define UI_MS_EXIT       180   // salidas rapidas
#define UI_MS_SCREEN     250   // cambio de pantalla: EL ESTANDAR
#define UI_MS_WAKE       260   // la luz volviendo a tu mano
#define UI_MS_ENTER      300   // entradas suaves
#define UI_MS_DAWN       900   // amanecer
#define UI_MS_DUSK      1200   // la pantalla retirandose

// ======================================================== Componentes =======

static inline lv_obj_t* uiLabel(lv_obj_t* p, const lv_font_t* font, lv_opa_t opa) {
  lv_obj_t* l = lv_label_create(p);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, COL_TXT, 0);
  lv_obj_set_style_text_opa(l, opa, 0);
  return l;
}

// Pantalla de datos: fondo solido, sin scroll.
static inline lv_obj_t* uiScreen() {
  lv_obj_t* s = lv_obj_create(nullptr);
  lv_obj_remove_style_all(s);
  lv_obj_set_style_bg_color(s, COL_BG, 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

// Titulo: y=22, centrado, blanco al 100%, capitalizado.
static inline lv_obj_t* uiTitle(lv_obj_t* p, const char* txt) {
  lv_obj_t* t = uiLabel(p, UI_FONT_TITLE, OPA_MAIN);
  lv_label_set_text(t, txt);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, UI_Y_TITLE);
  return t;
}

// Estado permanente: y=52, un renglon que dice COMO ESTA la pantalla.
static inline lv_obj_t* uiState(lv_obj_t* p) {
  lv_obj_t* s = uiLabel(p, UI_FONT_STATE, OPA_STATE);
  lv_label_set_text(s, "");
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, UI_Y_STATE);
  return s;
}

// Un objetivo tactil sin marco: solo el simbolo, con area de 44x44 alrededor.
static inline lv_obj_t* uiTap(lv_obj_t* p, const char* sym, lv_align_t al,
                              lv_coord_t x, lv_coord_t y, lv_event_cb_t cb) {
  lv_obj_t* b = lv_obj_create(p);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, UI_TAP_MIN, UI_TAP_MIN);
  lv_obj_align(b, al, x, y);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* l = uiLabel(b, UI_FONT_DATA, OPA_AVAIL);
  lv_label_set_text(l, sym);
  lv_obj_center(l);
  return b;
}

// Lista: el componente principal. Cada fila es un objetivo para el dedo.
static inline lv_obj_t* uiListBox(lv_obj_t* p) {
  lv_obj_t* b = lv_obj_create(p);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, UI_CONTENT_W, UI_ROW_H * UI_ROWS_MAX);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, UI_Y_ROW0);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(b, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(b, LV_SCROLLBAR_MODE_OFF);
  return b;
}

// Un solo helper para todas las transiciones de opacidad: nada de cortes duros.
static inline void uiFade(lv_obj_t* o, lv_opa_t to, uint16_t ms) {
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
    lv_anim_set_ready_cb(&a, [](lv_anim_t* an) {
      lv_obj_add_flag((lv_obj_t*)an->var, LV_OBJ_FLAG_HIDDEN);
    });
  }
  lv_anim_start(&a);
}

// Grupo: filas agrupadas en una superficie, como una lista de iOS. Lleva
// superficie porque agrupa. Las filas van con uiGroupRow.
static inline lv_obj_t* uiGroup(lv_obj_t* p, lv_coord_t y, uint8_t rows) {
  lv_obj_t* g = lv_obj_create(p);
  lv_obj_remove_style_all(g);
  lv_obj_set_size(g, UI_CONTENT_W, UI_ROW_H * rows);
  lv_obj_align(g, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_bg_color(g, COL_SURFACE, 0);
  lv_obj_set_style_bg_opa(g, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g, UI_RADIUS, 0);
  lv_obj_set_style_clip_corner(g, true, 0);
  lv_obj_set_flex_flow(g, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);
  return g;
}

// Una fila de grupo: etiqueta, valor a la derecha y, si navega, su chevron.
// Hijo 0 etiqueta, hijo 1 valor, hijo 2 chevron (si lo hay). La ultima fila
// del grupo va sin separador: se le quita con uiRowLast.
static inline lv_obj_t* uiGroupRow(lv_obj_t* g, const char* label, const char* value,
                                   bool chevron, lv_event_cb_t cb, void* user) {
  lv_obj_t* r = lv_obj_create(g);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, UI_CONTENT_W, UI_ROW_H);
  lv_obj_set_style_pad_hor(r, 16, 0);
  lv_obj_set_style_border_color(r, COL_TXT, 0);
  lv_obj_set_style_border_opa(r, OPA_SEPARATOR, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_bg_color(r, COL_TXT, 0);
  lv_obj_set_style_bg_opa(r, 0, 0);
  lv_obj_set_style_bg_opa(r, OPA_KEY, LV_STATE_PRESSED);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  if (cb) { lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(r, cb, LV_EVENT_CLICKED, user); }

  lv_obj_t* l = uiLabel(r, UI_FONT_ROW, OPA_MAIN);
  lv_label_set_text(l, label);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t* v = uiLabel(r, UI_FONT_ROW, OPA_AVAIL);
  lv_label_set_text(v, value ? value : "");
  lv_obj_align(v, LV_ALIGN_RIGHT_MID, chevron ? -22 : 0, 0);
  if (chevron) {
    lv_obj_t* c = uiLabel(r, UI_FONT_STATE, OPA_INACTIVE);
    lv_label_set_text(c, LV_SYMBOL_RIGHT);
    lv_obj_align(c, LV_ALIGN_RIGHT_MID, 0, 0);
  }
  return r;
}
static inline void uiRowLast(lv_obj_t* r) { lv_obj_set_style_border_width(r, 0, 0); }

// Un control que hoy no controla nada: tenue, y el dedo pasa de largo.
static inline void uiRowInactive(lv_obj_t* r) {
  lv_obj_clear_flag(r, LV_OBJ_FLAG_CLICKABLE);
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(r); i++)
    lv_obj_set_style_text_opa(lv_obj_get_child(r, i), OPA_INACTIVE, 0);
}

// Slider: pista tenue, lo recorrido en ambar (es lo que editas), perilla blanca.
static inline lv_obj_t* uiSlider(lv_obj_t* p, lv_coord_t w, int32_t min, int32_t max) {
  lv_obj_t* s = lv_slider_create(p);
  lv_obj_remove_style_all(s);
  lv_obj_set_size(s, w, 4);
  lv_slider_set_range(s, min, max);
  lv_obj_set_style_bg_color(s, COL_TXT, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s, OPA_SURFACE, LV_PART_MAIN);
  lv_obj_set_style_radius(s, 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(s, 2, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s, COL_TXT, LV_PART_KNOB);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_pad_all(s, 9, LV_PART_KNOB);           // perilla de 22
  lv_obj_set_ext_click_area(s, 16);                        // el dedo no es preciso
  return s;
}

// Senal Wi-Fi: cuatro barras de 3 px (alturas 5/8/11/14, separadas 2). Las
// encendidas van del color que se pida; las apagadas, blanco al 24%.
static inline lv_obj_t* uiSignal(lv_obj_t* p, uint8_t bars, lv_color_t on) {
  lv_obj_t* box = lv_obj_create(p);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, 18, 14);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  for (int i = 0; i < 4; i++) {
    lv_obj_t* b = lv_obj_create(box);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 3, 5 + 3 * i);
    lv_obj_set_pos(b, i * 5, 14 - (5 + 3 * i));
    lv_obj_set_style_bg_color(b, i < bars ? on : COL_TXT, 0);
    lv_obj_set_style_bg_opa(b, i < bars ? LV_OPA_COVER : OPA_INACTIVE, 0);
  }
  return box;
}
