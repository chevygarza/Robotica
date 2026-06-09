#include "tme_ui.h"
#include "tme_net.h"
#include <lvgl.h>
#include <stdio.h>

// ---------- Paleta (familia StuntHub) ----------
#define COL_BG     lv_color_hex(0x0B1020)
#define COL_CARD   lv_color_hex(0x161C2E)
#define COL_TXT    lv_color_hex(0xFFFFFF)
#define COL_SUB    lv_color_hex(0x8A93A6)
#define COL_ACCENT lv_color_hex(0x4EA8FF)
#define COL_WARM   lv_color_hex(0xFFB454)
#define COL_OK     lv_color_hex(0x3DD68C)
#define COL_BAD    lv_color_hex(0xFF5B6E)

// ---------- Frases para el operador (rotan cada 6s) ----------
static const char *PHRASES[] = {
  "Descargando datos del motor...",
  "Gracias, operador",
  "Tu familia te espera, maneja seguro",
  "Eres pieza clave de este equipo",
  "Revisa espejos, luces y llantas",
  "Hidratate bien antes de salir",
  "La carga llega porque tu llegas",
  "Con calma se llega lejos",
  "Excelente viaje",
};
static const int N_PHRASES = sizeof(PHRASES) / sizeof(PHRASES[0]);

// ---------- Vistas ----------
enum View { V_IDLE, V_CONFIRM, V_RUN, V_DONE, V_ERROR };
static View view = V_IDLE;
static uint32_t confirmT = 0;        // timeout de confirmacion
static bool dismissed = false;       // done/error ya visto (push)
static bool sawRun = false;          // presenciamos el proceso

// ---------- Widgets ----------
static lv_obj_t *scr[5];
static lv_obj_t *idleStatus, *idleDot;
static lv_obj_t *runStep, *runBar, *runEta, *runPhrase;

static lv_obj_t* mkLabel(lv_obj_t *p, const lv_font_t *f, lv_color_t c) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, c, 0);
  return l;
}

static lv_obj_t* newScreen(lv_color_t topColor) {
  lv_obj_t *s = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(s, COL_BG, 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  // arco superior de color (estado)
  lv_obj_t *band = lv_obj_create(s);
  lv_obj_remove_style_all(band);
  lv_obj_set_size(band, 120, 6);
  lv_obj_set_style_radius(band, 3, 0);
  lv_obj_set_style_bg_color(band, topColor, 0);
  lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
  lv_obj_align(band, LV_ALIGN_TOP_MID, 0, 26);
  return s;
}

// ---------- V_IDLE ----------
static void buildIdle() {
  lv_obj_t *s = newScreen(COL_ACCENT);
  scr[V_IDLE] = s;

  lv_obj_t *t = mkLabel(s, &lv_font_montserrat_48, COL_TXT);
  lv_label_set_text(t, "TME");
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -70);

  lv_obj_t *st = mkLabel(s, &lv_font_montserrat_20, COL_SUB);
  lv_label_set_text(st, "Reseteo PC26");
  lv_obj_align(st, LV_ALIGN_CENTER, 0, -26);

  idleDot = lv_obj_create(s);
  lv_obj_set_size(idleDot, 10, 10);
  lv_obj_set_style_radius(idleDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(idleDot, 0, 0);
  lv_obj_set_style_bg_color(idleDot, COL_SUB, 0);
  lv_obj_clear_flag(idleDot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(idleDot, LV_ALIGN_CENTER, -72, 12);

  idleStatus = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_label_set_text(idleStatus, "Buscando maquina...");
  lv_obj_align(idleStatus, LV_ALIGN_CENTER, 8, 12);

  lv_obj_t *btn = lv_obj_create(s);
  lv_obj_set_size(btn, 220, 54);
  lv_obj_set_style_radius(btn, 27, 0);
  lv_obj_set_style_bg_color(btn, COL_CARD, 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, 70);
  lv_obj_t *bl = mkLabel(btn, &lv_font_montserrat_18, COL_TXT);
  lv_label_set_text(bl, LV_SYMBOL_PLAY "  Iniciar reseteo");
  lv_obj_center(bl);

  lv_obj_t *h = mkLabel(s, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(h, "presiona la perilla");
  lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -34);
}

// ---------- V_CONFIRM ----------
static void buildConfirm() {
  lv_obj_t *s = newScreen(COL_WARM);
  scr[V_CONFIRM] = s;

  lv_obj_t *q = mkLabel(s, &lv_font_montserrat_28, COL_TXT);
  lv_label_set_text(q, "Iniciar reseteo?");
  lv_obj_align(q, LV_ALIGN_CENTER, 0, -40);

  lv_obj_t *a = mkLabel(s, &lv_font_montserrat_18, COL_WARM);
  lv_label_set_text(a, "presiona otra vez = SI");
  lv_obj_align(a, LV_ALIGN_CENTER, 0, 10);

  lv_obj_t *c = mkLabel(s, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(c, "se cancela solo en 10 seg");
  lv_obj_align(c, LV_ALIGN_CENTER, 0, 48);
}

// ---------- V_RUN ----------
static void buildRun() {
  lv_obj_t *s = newScreen(COL_OK);
  scr[V_RUN] = s;

  lv_obj_t *t = mkLabel(s, &lv_font_montserrat_16, COL_SUB);
  lv_label_set_text(t, "RESETEO EN CURSO");
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 46);

  runStep = mkLabel(s, &lv_font_montserrat_18, COL_TXT);
  lv_label_set_text(runStep, "Iniciando...");
  lv_obj_align(runStep, LV_ALIGN_CENTER, 0, -56);

  runBar = lv_bar_create(s);
  lv_obj_set_size(runBar, 240, 22);
  lv_obj_align(runBar, LV_ALIGN_CENTER, 0, -16);
  lv_obj_set_style_radius(runBar, 11, 0);
  lv_obj_set_style_bg_color(runBar, COL_CARD, 0);
  lv_obj_set_style_bg_color(runBar, COL_OK, LV_PART_INDICATOR);
  lv_obj_set_style_radius(runBar, 11, LV_PART_INDICATOR);
  lv_bar_set_range(runBar, 0, 100);
  lv_bar_set_value(runBar, 0, LV_ANIM_OFF);

  runEta = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_label_set_text(runEta, "");
  lv_obj_align(runEta, LV_ALIGN_CENTER, 0, 14);

  runPhrase = mkLabel(s, &lv_font_montserrat_16, COL_WARM);
  lv_label_set_long_mode(runPhrase, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(runPhrase, 250);
  lv_obj_set_style_text_align(runPhrase, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(runPhrase, PHRASES[0]);
  lv_obj_align(runPhrase, LV_ALIGN_CENTER, 0, 64);
}

// ---------- V_DONE ----------
static void buildDone() {
  lv_obj_t *s = newScreen(COL_OK);
  scr[V_DONE] = s;

  lv_obj_t *ic = mkLabel(s, &lv_font_montserrat_48, COL_OK);
  lv_label_set_text(ic, LV_SYMBOL_OK);
  lv_obj_align(ic, LV_ALIGN_CENTER, 0, -76);

  lv_obj_t *t = mkLabel(s, &lv_font_montserrat_24, COL_TXT);
  lv_label_set_text(t, "Listo! Gracias, operador");
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -16);

  lv_obj_t *m = mkLabel(s, &lv_font_montserrat_16, COL_SUB);
  lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(m, 260);
  lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(m, "Excelente viaje.\nDescansa, te esperan en casa.");
  lv_obj_align(m, LV_ALIGN_CENTER, 0, 38);

  lv_obj_t *h = mkLabel(s, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(h, "presiona para volver");
  lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -34);
}

// ---------- V_ERROR ----------
static void buildError() {
  lv_obj_t *s = newScreen(COL_BAD);
  scr[V_ERROR] = s;

  lv_obj_t *ic = mkLabel(s, &lv_font_montserrat_48, COL_BAD);
  lv_label_set_text(ic, LV_SYMBOL_CLOSE);
  lv_obj_align(ic, LV_ALIGN_CENTER, 0, -76);

  lv_obj_t *t = mkLabel(s, &lv_font_montserrat_22, COL_TXT);
  lv_label_set_text(t, "Algo fallo en el reseteo");
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -16);

  lv_obj_t *m = mkLabel(s, &lv_font_montserrat_16, COL_SUB);
  lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(m, 260);
  lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(m, "Intenta de nuevo o\navisa a sistemas.");
  lv_obj_align(m, LV_ALIGN_CENTER, 0, 38);

  lv_obj_t *h = mkLabel(s, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(h, "presiona para volver");
  lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -34);
}

static void show(View v, lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_FADE_ON) {
  if (view == v) return;
  view = v;
  lv_scr_load_anim(scr[v], anim, 220, 0, false);
}

void ui_build() {
  buildIdle();
  buildConfirm();
  buildRun();
  buildDone();
  buildError();
  lv_scr_load(scr[V_IDLE]);
}

bool ui_can_sleep() { return view == V_IDLE; }

// 👇 Push corto
void ui_push() {
  switch (view) {
    case V_IDLE:
      show(V_CONFIRM); confirmT = millis();
      break;
    case V_CONFIRM:
      tme_request_reset();
      sawRun = true; dismissed = false;
      lv_label_set_text(runStep, "Iniciando...");
      lv_bar_set_value(runBar, 3, LV_ANIM_OFF);
      lv_label_set_text(runEta, "");
      show(V_RUN);
      break;
    case V_RUN:
      break;                       // idempotente: ignorado
    case V_DONE:
    case V_ERROR:
      dismissed = true; sawRun = false;
      show(V_IDLE);
      break;
  }
}

void ui_long() {
  if (view == V_CONFIRM) show(V_IDLE);   // cancelar confirmacion
  if (view == V_DONE || view == V_ERROR) { dismissed = true; sawRun = false; show(V_IDLE); }
}

void ui_tick() {
  // timeout de confirmacion
  if (view == V_CONFIRM && millis() - confirmT > 10000) show(V_IDLE);

  static uint32_t last = 0;
  if (millis() - last < 300) return;
  last = millis();

  if (!tme_lock(20)) return;
  TmeStatus st = g_tme;
  tme_unlock();

  bool isRunning = (st.state == TME_STARTING || st.state == TME_CHECK1 || st.state == TME_CHECK2);

  // estado nuevo -> limpia el "visto"
  if (isRunning) dismissed = false;

  // proceso corriendo (incluso iniciado desde el escritorio) -> mostrar barra
  if (isRunning && (view == V_IDLE || view == V_CONFIRM)) { sawRun = true; show(V_RUN); }

  // fin del proceso
  if (st.state == TME_DONE && view == V_RUN)  show(V_DONE);
  if (st.state == TME_ERROR && !dismissed && (view == V_RUN || view == V_IDLE) && sawRun) show(V_ERROR);

  // refresco de la vista RUN
  if (view == V_RUN) {
    if (st.label[0]) lv_label_set_text(runStep, st.label);
    if (st.pct >= 0) lv_bar_set_value(runBar, st.pct, LV_ANIM_ON);
    if (st.eta_s >= 0) {
      char e[20]; snprintf(e, sizeof(e), "faltan %02d:%02d", st.eta_s / 60, st.eta_s % 60);
      lv_label_set_text(runEta, e);
    } else {
      lv_label_set_text(runEta, "");
    }
    static uint32_t phT = 0; static int ph = 0;
    if (millis() - phT > 6000) {
      phT = millis();
      ph = (ph + 1) % N_PHRASES;
      lv_label_set_text(runPhrase, PHRASES[ph]);
    }
  }

  // status en reposo
  if (view == V_IDLE) {
    bool online = (st.state != TME_OFFLINE);
    lv_obj_set_style_bg_color(idleDot, online ? COL_OK : COL_SUB, 0);
    lv_label_set_text(idleStatus, online ? "Maquina en linea" : "Sin conexion con maquina");
  }
}
