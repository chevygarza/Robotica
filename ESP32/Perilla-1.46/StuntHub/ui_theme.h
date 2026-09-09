// ============================================================================
//  StuntHub — INTERFAZ.md hecho codigo
// ============================================================================
// La espec vive en ./INTERFAZ.md y MANDA. Este archivo solo la vuelve
// ejecutable: si el documento y este archivo se contradicen, gana el documento
// y este archivo esta mal.
//
// Existe porque un lineamiento que solo vive en prosa no se puede obedecer:
// asi acabamos con siete tamanos de letra y el titulo a cinco alturas. Las apps
// NO escriben fuentes, alturas, radios ni hex. Si necesitas un numero nuevo,
// se agrega AQUI y se documenta ALLA.
#pragma once
#include <lvgl.h>

// ---------------------------------------------------------------- Color ----
// INTERFAZ.md §7. Fondo oscuro, a conciencia. UN acento: ambar.
#define COL_BG      lv_color_hex(0x0B1020)   // fondo de toda pantalla
#define COL_SURFACE lv_color_hex(0x161C2E)   // tarjeta, SOLO si agrupa algo
#define COL_TXT     lv_color_hex(0xFFFFFF)   // siempre blanco: jerarquia = opacidad
#define COL_ACCENT  lv_color_hex(0xFF7A10)   // ambar. Lo que editas, lo que corre
#define COL_OK      lv_color_hex(0x3DD68C)   // ESTADO, nunca decoracion
#define COL_BAD     lv_color_hex(0xFF5B6E)   // ESTADO, nunca decoracion
#define COL_WARN    lv_color_hex(0xFFB454)   // ESTADO, nunca decoracion
// El ambar es el ADN de la familia (mismo de VinilOS): atravesando acrilico se
// lee como amplificador de bulbos, no como periferico gamer. NO existe COL_SUB:
// el texto secundario es blanco con opacidad.

// ------------------------------------------------------------- Opacidad ----
// INTERFAZ.md §5.3. La jerarquia va por aqui, no por color.
#define OPA_SEL_LABEL   235
#define OPA_SEL_VALUE   255
#define OPA_AVAIL_LABEL 150
#define OPA_AVAIL_VALUE 175
#define OPA_INACTIVE     60   // se ve que existe; el giro pasa de largo
#define OPA_STATE       140   // renglon de estado permanente

// ----------------------------------------------------------- Tipografia ----
// INTERFAZ.md §6. CUATRO de familia + DOS de cifra. El 12 y el 20 se eliminaron:
// el 12 esta bajo el piso legible a un brazo de distancia.
#define UI_FONT_TITLE   &lv_font_montserrat_22  // titulo de app o pantalla
#define UI_FONT_DATA    &lv_font_montserrat_18  // dato principal dentro de una app
#define UI_FONT_ROW     &lv_font_montserrat_16  // filas de lista y campos
#define UI_FONT_STATE   &lv_font_montserrat_14  // estado, datos secundarios
#define UI_FONT_FIG_XL  &lv_font_montserrat_48  // la cifra protagonista: UNA por pantalla
#define UI_FONT_FIG_MD  &lv_font_montserrat_28  // valor principal de una tarjeta
// Todo en ASCII: Montserrat de LVGL no trae acentos. El grado (176) si existe.
// Capitalizado, no VERSALITAS: "Pc Gamer", no "PC GAMER".

// --------------------------------------------------------------- Layout ----
// INTERFAZ.md §2. Pantalla REDONDA: el cuadrado existe en memoria, el circulo
// es lo unico que se ve. Prohibido LEFT_MID y RIGHT_MID.
#define UI_SAFE_TOP       30   // nada por encima
#define UI_SAFE_BOTTOM   320   // nada por debajo
#define UI_Y_TITLE        32   // UNICA altura de titulo en toda la UI
#define UI_Y_STATE        60   // subtitulo / estado permanente
#define UI_Y_ROW0         86   // primera fila de lista
#define UI_ROW_H          32   // alto de fila
#define UI_ROWS_MAX        7   // con mas, la lista se desliza
#define UI_CONTENT_W     280   // ancho de contenido, centrado
#define UI_BOX_H         200   // alto maximo de una caja
#define UI_ROW_PAD_H      12   // etiqueta en x=52, valor termina en x=308
#define UI_CARD_RADIUS    16
#define UI_DOTS_Y        -18   // indicador de apps: SOLO en el home

// ------------------------------------------------------------ Movimiento ----
// INTERFAZ.md §8. Todo ease_in_out, nada lineal.
#define UI_MS_EXPECTED   120   // aparecer algo que ya esperabas
#define UI_MS_EXIT       180   // salidas rapidas
#define UI_MS_SCREEN     250   // cambio de pantalla: EL ESTANDAR
#define UI_MS_ENTER      300   // entradas suaves
#define UI_MS_DAWN       900   // amanecer
#define UI_MS_DUSK      1200   // la pantalla retirandose a negro
// Asimetria deliberada: se va lento (1200), vuelve rapido (260).

// ======================================================== Componentes =======
// INTERFAZ.md §5. Cinco. No se inventan mas sin quitar uno.

static inline lv_obj_t* uiLabel(lv_obj_t *p, const lv_font_t *font,
                                lv_color_t c, lv_opa_t opa) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, c, 0);
  lv_obj_set_style_text_opa(l, opa, 0);
  return l;
}

// 5.1 Titulo de app: y=32, blanco al 100%, capitalizado.
static inline lv_obj_t* uiTitle(lv_obj_t *p, const char *txt) {
  lv_obj_t *t = uiLabel(p, UI_FONT_TITLE, COL_TXT, LV_OPA_COVER);
  lv_label_set_text(t, txt);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, UI_Y_TITLE);
  return t;
}

// 5.2 Estado permanente: y=60, un renglon que dice COMO ESTA la app.
// Es informacion ("4 de 44 Encendidos"), NUNCA un hint de gesto.
static inline lv_obj_t* uiState(lv_obj_t *p) {
  lv_obj_t *s = uiLabel(p, UI_FONT_STATE, COL_TXT, OPA_STATE);
  lv_label_set_text(s, "");
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, UI_Y_STATE);
  return s;
}

// 5.3 Lista: el componente principal. Todo menu es esto.
static inline lv_obj_t* uiListBox(lv_obj_t *p) {
  lv_obj_t *b = lv_obj_create(p);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, UI_CONTENT_W, UI_ROW_H * UI_ROWS_MAX);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, UI_Y_ROW0);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(b, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(b, LV_SCROLLBAR_MODE_OFF);
  return b;
}

// Una fila. Etiqueta a la izquierda, valor opcional a la derecha; el valor se
// alinea a la derecha para que una cifra que se actualiza no baile.
// Devuelve la fila; la etiqueta es su hijo 0 y el valor su hijo 1.
static inline lv_obj_t* uiRow(lv_obj_t *box, const char *label, const char *value) {
  lv_obj_t *r = lv_obj_create(box);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, UI_CONTENT_W, UI_ROW_H);
  lv_obj_set_style_pad_hor(r, UI_ROW_PAD_H, 0);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *l = uiLabel(r, UI_FONT_ROW, COL_TXT, OPA_AVAIL_LABEL);
  lv_label_set_text(l, label);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  lv_obj_set_width(l, UI_CONTENT_W - UI_ROW_PAD_H * 2 - 70);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *v = uiLabel(r, UI_FONT_ROW, COL_TXT, OPA_AVAIL_VALUE);
  lv_label_set_text(v, value ? value : "");
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
  return r;
}

static inline lv_obj_t* uiRowLabel(lv_obj_t *row) { return lv_obj_get_child(row, 0); }
static inline lv_obj_t* uiRowValue(lv_obj_t *row) { return lv_obj_get_child(row, 1); }

// Foco de una fila. SIN marcos ni recuadros (§5.3): solo opacidad y color.
// locked = existe pero no se puede usar ahora; el giro debe pasar de largo.
// editing = se esta cambiando su valor: el valor va en ambar.
static inline void uiRowFocus(lv_obj_t *row, bool sel, bool locked, bool editing) {
  lv_obj_t *l = uiRowLabel(row), *v = uiRowValue(row);
  lv_opa_t ol = locked ? OPA_INACTIVE : (sel ? OPA_SEL_LABEL : OPA_AVAIL_LABEL);
  lv_opa_t ov = locked ? OPA_INACTIVE : (sel ? OPA_SEL_VALUE : OPA_AVAIL_VALUE);
  lv_obj_set_style_text_opa(l, ol, 0);
  lv_obj_set_style_text_opa(v, ov, 0);
  lv_obj_set_style_text_color(v, (editing && !locked) ? COL_ACCENT : COL_TXT, 0);
}
