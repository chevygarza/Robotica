// TAC-1 — El carrusel de apps y sus pantallas.
//
// CARRUSEL  Inicio <-> Clima <-> Mercados, circular. Deslizar a la izquierda
//           trae la siguiente app (entra por la derecha), como el giro horario
//           en StuntHub. Los puntos abajo dicen donde estas.
// INICIO    wallpaper, la hora grande, la fecha, y una pastilla con el estado
//           de la red arriba a la derecha. Tocar la pastilla abre la Red.
// RED       lista de redes -> clave con teclado -> conectando -> vuelve solo.
//           Es un nivel abajo de Inicio: la flecha regresa, sin puntos.
// CLIMA     Monterrey ahora y las proximas cinco horas.
// MERCADOS  BTC, ETH y SOL en tres tarjetas.
// AJUSTES   deslizar hacia abajo desde cualquier app lo baja como una hoja;
//           hacia arriba (o la flecha) lo cierra. Wi-Fi, Brillo, Volumen,
//           Apagar y Reiniciar. Lo irreversible pide un segundo toque.
//
// Reposo: sin tocar 3 minutos la luz baja a un resplandor. El primer toque
// solo la trae de vuelta, no actua.
#include "ui.h"
#include "ui_theme.h"
#include "display.h"
#include "net.h"
#include "cloud.h"
#include "ajustes.h"
#include "version.h"
#include "board.h"
#include <time.h>

LV_FONT_DECLARE(tac_reloj_96);
extern "C" const lv_img_dsc_t wallpaper;

#define IDLE_MS        180000   // 3 minutos
#define BRILLO_DIA         70
#define BRILLO_NOCHE       25
#define BRILLO_REPOSO      15   // piso: mas abajo el panel deja de leerse
#define KB_H              188
#define SCAN_EVERY_MS   15000

// ── Carrusel ─────────────────────────────────────────────────────────────────
enum : uint8_t { APP_INICIO, APP_CLIMA, APP_MERCADOS, APP_N };
static lv_obj_t* apps[APP_N];
static uint8_t   app;

// ── Inicio ───────────────────────────────────────────────────────────────────
static lv_obj_t *scrHome, *lbHora, *lbFecha, *pill, *pillDot, *pillTxt;

// ── Clima ────────────────────────────────────────────────────────────────────
static lv_obj_t *cxState, *cxTemp, *cxCond, *cxL1, *cxL2, *cxHr[5], *cxHrT[5], *cxHrP[5];

// ── Mercados ─────────────────────────────────────────────────────────────────
static lv_obj_t *mkState, *mkSym[COIN_N], *mkPrice[COIN_N], *mkChg[COIN_N], *mkGlobal;
static int       lastMin = -1;
static NetState  lastNet = (NetState)255;
static bool      dormido;
static uint8_t   brilloAct;

// ── Ajustes ──────────────────────────────────────────────────────────────────
static lv_obj_t *scrAj, *ajWifi, *ajBrilloVal, *ajSlider, *ajApagar, *ajReiniciar;
static lv_obj_t *ajFrom;                 // a donde vuelve al cerrarse
static lv_obj_t *confirmRow;             // fila que espera el segundo toque
static uint32_t  confirmAt;
static bool      apagada;                // Apagar: la luz se va y luego el panel duerme
static bool      panelOff;               // el panel ya recibio SLPIN
#define CONFIRM_MS 4000

// ── Red ──────────────────────────────────────────────────────────────────────
static lv_obj_t *redFrom;                // a donde vuelve al cerrarse
enum RedVista : uint8_t { RV_LISTA, RV_CLAVE, RV_CONECTANDO };
static lv_obj_t *scrRed, *redState, *redList, *redTa, *redKb, *redSpin, *redMsg;
static RedVista  redVista;
static bool      enRed;
static char      redSsid[33];
static int       redScanN = -1;
static uint32_t  redScanAt, redDoneAt;

static const char* DIAS[]  = { "Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado" };
static const char* MESES[] = { "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio",
                               "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre" };

uint8_t ui_brillo() {
  if (ajustes().brillo != BRILLO_AUTO) return ajustes().brillo;
  struct tm t;
  if (net_time(&t) && (t.tm_hour >= 22 || t.tm_hour < 7)) return BRILLO_NOCHE;
  return BRILLO_DIA;
}

// ── Carrusel: deslizar y los puntos ──────────────────────────────────────────
static void appGoto(uint8_t to, bool left) {
  app = to;
  lv_scr_load_anim(apps[to], left ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                   UI_MS_SCREEN, 0, false);
}

static void ajEnter();
static void appSwipe(lv_event_t* e) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (d == LV_DIR_TOP) return;
  lv_indev_wait_release(lv_indev_get_act());     // el gesto no es un toque
  if (d == LV_DIR_BOTTOM) { ajEnter(); return; }
  bool left = d == LV_DIR_LEFT;
  appGoto((app + (left ? 1 : APP_N - 1)) % APP_N, left);
}

// Cada pantalla del carrusel lleva sus puntos: el encendido es el suyo.
static void carousel(lv_obj_t* scr, uint8_t idx) {
  apps[idx] = scr;
  lv_obj_add_event_cb(scr, appSwipe, LV_EVENT_GESTURE, nullptr);
  lv_obj_t* row = lv_obj_create(scr);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, UI_DOT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, UI_DOT_GAP, 0);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, UI_DOTS_Y);
  for (uint8_t i = 0; i < APP_N; i++) {
    lv_obj_t* d = lv_obj_create(row);
    lv_obj_remove_style_all(d);
    lv_obj_set_size(d, UI_DOT, UI_DOT);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(d, i == idx ? COL_ACCENT : COL_TXT, 0);
    lv_obj_set_style_bg_opa(d, i == idx ? OPA_MAIN : OPA_INACTIVE, 0);
  }
}

// ── Red: vistas ──────────────────────────────────────────────────────────────
static void redShow(RedVista v) {
  redVista = v;
  uiFade(redList, v == RV_LISTA       ? OPA_MAIN : 0, v == RV_LISTA       ? UI_MS_ENTER : UI_MS_EXIT);
  uiFade(redTa,   v == RV_CLAVE       ? OPA_MAIN : 0, v == RV_CLAVE       ? UI_MS_ENTER : UI_MS_EXIT);
  uiFade(redKb,   v == RV_CLAVE       ? OPA_MAIN : 0, v == RV_CLAVE       ? UI_MS_ENTER : UI_MS_EXIT);
  uiFade(redSpin, v == RV_CONECTANDO  ? OPA_MAIN : 0, v == RV_CONECTANDO  ? UI_MS_ENTER : UI_MS_EXIT);
  uiFade(redMsg,  v == RV_CONECTANDO  ? OPA_MAIN : 0, v == RV_CONECTANDO  ? UI_MS_ENTER : UI_MS_EXIT);
  if (v == RV_CLAVE) {
    char ph[48];
    snprintf(ph, sizeof(ph), "Clave de %s", redSsid);
    lv_textarea_set_placeholder_text(redTa, ph);
    lv_textarea_set_text(redTa, "");
    lv_label_set_text(redState, redSsid);
  }
}

static void redConnect() {
  net_connect(redSsid, lv_textarea_get_text(redTa));
  char m[64];
  snprintf(m, sizeof(m), "Conectando a %s", redSsid);
  lv_label_set_text(redMsg, m);
  lv_label_set_text(redState, "");
  redShow(RV_CONECTANDO);
}

static void redRow(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  strlcpy(redSsid, net_scan_ssid(i), sizeof(redSsid));
  if (net_scan_open(i)) { lv_textarea_set_text(redTa, ""); redConnect(); }
  else redShow(RV_CLAVE);
}

// La senal: barras encendidas y opacidad de toda la fila, juntas.
static uint8_t  rssiBars(int rssi) { return rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1; }
static lv_opa_t barsOpa(uint8_t b)  { return b == 4 ? OPA_AVAIL : b == 3 ? OPA_CONTEXT : b == 2 ? OPA_STATE : OPA_INACTIVE; }

static lv_obj_t* redRowMake(const char* ssid, int rssi, bool current, int idx) {
  lv_obj_t* r = lv_obj_create(redList);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, UI_CONTENT_W, UI_ROW_H);
  lv_obj_set_style_border_color(r, COL_TXT, 0);
  lv_obj_set_style_border_opa(r, OPA_SEPARATOR, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_bg_color(r, COL_TXT, 0);
  lv_obj_set_style_bg_opa(r, 0, 0);
  lv_obj_set_style_bg_opa(r, OPA_KEY, LV_STATE_PRESSED);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(r, redRow, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
  uint8_t bars = rssiBars(rssi);
  lv_obj_t* l = uiLabel(r, UI_FONT_ROW, OPA_MAIN);
  lv_label_set_text(l, ssid);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  lv_obj_set_width(l, UI_CONTENT_W - 40);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t* sig = uiSignal(r, bars, current ? COL_OK : COL_TXT);
  lv_obj_align(sig, LV_ALIGN_RIGHT_MID, 0, 0);
  if (current) lv_obj_set_style_text_color(l, COL_OK, 0);   // la actual, en verde entera
  else lv_obj_set_style_opa(r, barsOpa(bars), 0);          // las demas, por senal
  return r;
}

static void redFill() {
  lv_obj_clean(redList);
  int n = net_scan_count(), shown = 0;
  for (int i = 0; i < n; i++) {
    char s[33];
    strlcpy(s, net_scan_ssid(i), sizeof(s));       // copia: net_scan_ssid reusa su buffer
    if (!s[0]) continue;
    bool dup = false;                              // varios AP de la misma red
    for (int j = 0; j < i && !dup; j++) dup = strcmp(net_scan_ssid(j), s) == 0;
    if (dup) continue;
    redRowMake(s, net_scan_rssi(i), net_state() == NET_UP && strcmp(net_ssid(), s) == 0, i);
    shown++;
  }
  if (net_state() == NET_UP) lv_label_set_text_fmt(redState, "Conectado a %s", net_ssid());
  else lv_label_set_text_fmt(redState, shown == 1 ? "%d Red" : "%d Redes", shown);
}

static void redScan() {
  net_scan_start();
  redScanN  = -1;
  redScanAt = millis();
  if (!lv_obj_get_child_cnt(redList)) lv_label_set_text(redState, "Buscando Redes");
}

static void redEnter() {
  redFrom = lv_scr_act();
  enRed = true;
  strlcpy(redSsid, "", sizeof(redSsid));
  redShow(RV_LISTA);
  lv_obj_clean(redList);
  redScan();
  lv_scr_load_anim(scrRed, LV_SCR_LOAD_ANIM_FADE_IN, UI_MS_SCREEN, 0, false);
}

static void redLeave() {
  enRed = false;
  lv_scr_load_anim(redFrom, LV_SCR_LOAD_ANIM_FADE_IN, UI_MS_SCREEN, 0, false);
}

static void redBack(lv_event_t*) {
  if (redVista == RV_CLAVE) redShow(RV_LISTA);    // sube UN nivel, siempre
  else redLeave();
}

static void redKbEvent(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_READY)       redConnect();
  else if (c == LV_EVENT_CANCEL) redShow(RV_LISTA);
}

static void buildRed() {
  scrRed = uiScreen();
  uiTitle(scrRed, "Red WiFi");
  redState = uiState(scrRed);
  uiTap(scrRed, LV_SYMBOL_LEFT, LV_ALIGN_TOP_LEFT, UI_MARGIN - 12, 0, redBack);
  redList = uiListBox(scrRed);

  // La clave: un campo y el teclado. Se estilan desde cero, sin el tema.
  redTa = lv_textarea_create(scrRed);
  lv_obj_remove_style_all(redTa);
  lv_obj_set_size(redTa, UI_CONTENT_W, UI_TAP_MIN);
  lv_obj_align(redTa, LV_ALIGN_TOP_MID, 0, UI_Y_ROW0);
  lv_obj_set_style_bg_color(redTa, COL_TXT, 0);
  lv_obj_set_style_bg_opa(redTa, OPA_KEY, 0);
  lv_obj_set_style_radius(redTa, UI_RADIUS, 0);
  lv_obj_set_style_pad_hor(redTa, 16, 0);
  lv_obj_set_style_pad_ver(redTa, 12, 0);
  lv_obj_set_style_text_font(redTa, UI_FONT_DATA, 0);
  lv_obj_set_style_text_color(redTa, COL_TXT, 0);
  lv_obj_set_style_text_opa(redTa, OPA_INACTIVE, LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_set_style_border_side(redTa, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
  lv_obj_set_style_border_width(redTa, 2, LV_PART_CURSOR);
  lv_obj_set_style_border_color(redTa, COL_ACCENT, LV_PART_CURSOR);
  lv_textarea_set_one_line(redTa, true);
  lv_textarea_set_password_mode(redTa, true);
  lv_textarea_set_max_length(redTa, 64);

  redKb = lv_keyboard_create(scrRed);
  lv_obj_remove_style_all(redKb);
  lv_obj_set_size(redKb, SCR_W, KB_H);
  lv_obj_align(redKb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_pad_all(redKb, 8, 0);
  lv_obj_set_style_pad_gap(redKb, 6, 0);
  lv_obj_set_style_bg_color(redKb, COL_TXT, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(redKb, OPA_KEY, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(redKb, 60, (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_PRESSED);
  lv_obj_set_style_radius(redKb, 10, LV_PART_ITEMS);
  lv_obj_set_style_text_font(redKb, UI_FONT_DATA, LV_PART_ITEMS);
  lv_obj_set_style_text_color(redKb, COL_TXT, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(redKb, COL_ACCENT, (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(redKb, LV_OPA_COVER, (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED);
  lv_keyboard_set_textarea(redKb, redTa);
  lv_obj_add_event_cb(redKb, redKbEvent, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(redKb, redKbEvent, LV_EVENT_CANCEL, nullptr);

  redSpin = lv_spinner_create(scrRed, 1000, 60);
  lv_obj_remove_style_all(redSpin);
  lv_obj_set_size(redSpin, 44, 44);
  lv_obj_align(redSpin, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_arc_width(redSpin, 4, LV_PART_MAIN);
  lv_obj_set_style_arc_color(redSpin, COL_TXT, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(redSpin, OPA_SURFACE, LV_PART_MAIN);
  lv_obj_set_style_arc_width(redSpin, 4, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(redSpin, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(redSpin, true, LV_PART_INDICATOR);

  redMsg = uiLabel(scrRed, UI_FONT_DATA, OPA_AVAIL);
  lv_obj_set_style_text_align(redMsg, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(redMsg, UI_CONTENT_W);
  lv_obj_align(redMsg, LV_ALIGN_CENTER, 0, 36);

  // Todo lo que no es la lista arranca invisible; uiFade lo trae.
  for (lv_obj_t* o : { redTa, redKb, redSpin, redMsg }) {
    lv_obj_set_style_opa(o, 0, 0);
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}

// ── Clima ────────────────────────────────────────────────────────────────────
static void buildClima() {
  lv_obj_t* s = uiScreen();
  uiTitle(s, "Clima");
  cxState = uiState(s);
  lv_label_set_text(cxState, "Sin Datos");

  cxTemp = uiLabel(s, UI_FONT_FIG_XL, OPA_MAIN);
  lv_label_set_text(cxTemp, "--°");
  lv_obj_set_style_text_letter_space(cxTemp, -1, 0);
  lv_obj_set_pos(cxTemp, UI_MARGIN, UI_Y_ROW0 - 3);
  cxCond = uiLabel(s, UI_FONT_ROW, OPA_AVAIL);      // a la linea base de la cifra
  lv_label_set_text(cxCond, "");
  cxL1 = uiLabel(s, UI_FONT_STATE, OPA_CONTEXT);
  lv_label_set_text(cxL1, "");
  lv_obj_set_pos(cxL1, UI_MARGIN, 142);
  cxL2 = uiLabel(s, UI_FONT_STATE, OPA_CONTEXT);
  lv_label_set_text(cxL2, "");
  lv_obj_set_pos(cxL2, UI_MARGIN, 164);

  // Las proximas cinco horas, en cinco columnas.
  lv_coord_t colW = UI_CONTENT_W / 5;
  for (int i = 0; i < 5; i++) {
    lv_coord_t x = UI_MARGIN + colW * i, cw = colW;
    cxHr[i] = uiLabel(s, UI_FONT_STATE, OPA_STATE);
    cxHrT[i] = uiLabel(s, UI_FONT_DATA, OPA_MAIN);
    cxHrP[i] = uiLabel(s, UI_FONT_STATE, OPA_CONTEXT);
    lv_obj_t* col[3] = { cxHr[i], cxHrT[i], cxHrP[i] };
    lv_coord_t y[3]  = { 206, 228, 254 };
    for (int k = 0; k < 3; k++) {
      lv_label_set_text(col[k], "");
      lv_obj_set_width(col[k], cw);
      lv_obj_set_style_text_align(col[k], LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(col[k], x, y[k]);
    }
  }
  carousel(s, APP_CLIMA);
}

static void paintClima() {
  Weather w;
  if (!cloud_weather(&w)) return;
  lv_label_set_text_fmt(cxState, "Actualizado %s", w.updated);
  lv_label_set_text_fmt(cxTemp, "%d°", (int)lroundf(w.tempC));
  lv_label_set_text(cxCond, weather_text(w.code));
  lv_obj_align_to(cxCond, cxTemp, LV_ALIGN_OUT_RIGHT_BOTTOM, 16, -8);
  lv_label_set_text_fmt(cxL1, "Sensacion %d° \u2022 Humedad %d%%", (int)lroundf(w.feelsC), w.humidity);
  lv_label_set_text_fmt(cxL2, "Viento %d km/h \u2022 Max %d° / Min %d°",
                        (int)lroundf(w.windKmh), (int)lroundf(w.tMax), (int)lroundf(w.tMin));
  for (int i = 0; i < 5; i++) {
    lv_label_set_text_fmt(cxHr[i], "%dh", w.hrHour[i]);
    lv_label_set_text_fmt(cxHrT[i], "%d°", w.hrTemp[i]);
    lv_label_set_text_fmt(cxHrP[i], "%d%%", w.hrProb[i]);
    // Lluvia probable es un ESTADO: se dice con el color de atencion.
    bool rain = w.hrProb[i] >= 50;
    lv_obj_set_style_text_color(cxHrP[i], rain ? COL_WARN : COL_TXT, 0);
    lv_obj_set_style_text_opa(cxHrP[i], rain ? OPA_MAIN : OPA_CONTEXT, 0);
  }
}

// ── Mercados ─────────────────────────────────────────────────────────────────
static void buildMercados() {
  lv_obj_t* s = uiScreen();
  uiTitle(s, "Mercados");
  mkState = uiState(s);
  lv_label_set_text(mkState, "Sin Datos");

  // Tres tarjetas: cada una agrupa simbolo, precio y cambio, por eso llevan
  // superficie.
  lv_coord_t gap = 12, cw = (UI_CONTENT_W - gap * (COIN_N - 1)) / COIN_N, ch = 132;
  for (int i = 0; i < COIN_N; i++) {
    lv_obj_t* c = lv_obj_create(s);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, cw, ch);
    lv_obj_set_pos(c, UI_MARGIN + (cw + gap) * i, UI_Y_ROW0);
    lv_obj_set_style_bg_color(c, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, UI_RADIUS, 0);
    lv_obj_set_style_pad_ver(c, 16, 0);
    lv_obj_set_style_pad_hor(c, 14, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    // Todo al ras izquierdo: simbolo arriba, precio al centro, cambio abajo.
    mkSym[i] = uiLabel(c, UI_FONT_TITLE, OPA_MAIN);
    lv_label_set_text(mkSym[i], "");
    lv_obj_align(mkSym[i], LV_ALIGN_TOP_LEFT, 0, 0);
    mkPrice[i] = uiLabel(c, UI_FONT_FIG_MD, OPA_MAIN);
    lv_label_set_text(mkPrice[i], "--");
    lv_obj_set_style_text_letter_space(mkPrice[i], -1, 0);
    lv_obj_align(mkPrice[i], LV_ALIGN_LEFT_MID, 0, 0);
    mkChg[i] = uiLabel(c, UI_FONT_STATE, OPA_MAIN);
    lv_label_set_text(mkChg[i], "");
    lv_obj_align(mkChg[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
  }
  mkGlobal = uiLabel(s, UI_FONT_STATE, OPA_CONTEXT);
  lv_label_set_text(mkGlobal, "");
  lv_obj_set_pos(mkGlobal, UI_MARGIN, 236);
  carousel(s, APP_MERCADOS);
}

// Como el mockup: entero de mil para arriba ("104238"), un decimal abajo
// ("214.6"). Sin simbolo: el estado ya dice USD.
static void fmtPrice(char* out, size_t n, double p) {
  if (p >= 1000) snprintf(out, n, "%ld", lround(p));
  else           snprintf(out, n, "%.1f", p);
}

static void paintMercados() {
  Coin c[COIN_N];
  int n = cloud_coins(c);
  if (!n) return;
  lv_label_set_text_fmt(mkState, "USD \u2022 %s", cloud_markets_updated());
  if (cloud_mcap() > 0)
    lv_label_set_text_fmt(mkGlobal, "Capitalizacion total %.2f T \u2022 Dominancia BTC %d%%",
                          cloud_mcap() / 1e12, (int)lroundf(cloud_btc_dom()));
  for (int i = 0; i < n; i++) {
    char p[20];
    fmtPrice(p, sizeof(p), c[i].price);
    lv_label_set_text(mkSym[i], c[i].sym);
    lv_label_set_text(mkPrice[i], p);
    lv_label_set_text_fmt(mkChg[i], "%+.1f%%", c[i].chg24);
    lv_obj_set_style_text_color(mkChg[i], c[i].chg24 >= 0 ? COL_OK : COL_BAD, 0);
  }
}

// ── Ajustes ──────────────────────────────────────────────────────────────────
static void ajPaint() {
  NetState n = net_state();
  lv_label_set_text(lv_obj_get_child(ajWifi, 1), n == NET_UP ? net_ssid() : n == NET_CONNECTING ? "Conectando" : "Sin Red");
  uint8_t b = ajustes().brillo;
  if (b == BRILLO_AUTO) lv_label_set_text(ajBrilloVal, "Auto");
  else lv_label_set_text_fmt(ajBrilloVal, "%d%%", b);
  lv_slider_set_value(ajSlider, b == BRILLO_AUTO ? ui_brillo() : b, LV_ANIM_OFF);
}

static void ajLeave() {
  confirmRow = nullptr;
  lv_scr_load_anim(ajFrom, LV_SCR_LOAD_ANIM_OUT_TOP, UI_MS_SCREEN, 0, false);
}

static void ajEnter() {
  ajFrom = lv_scr_act();
  ajPaint();
  lv_scr_load_anim(scrAj, LV_SCR_LOAD_ANIM_OVER_BOTTOM, UI_MS_SCREEN, 0, false);
}

static void ajSwipe(lv_event_t*) {
  if (lv_indev_get_gesture_dir(lv_indev_get_act()) != LV_DIR_TOP) return;
  lv_indev_wait_release(lv_indev_get_act());
  ajLeave();
}

// Lo irreversible: el primer toque pide, el segundo confirma. La fila cambia
// a ambar y dice que va a pasar. Si no llega en 4 s, vuelve sola.
static void ajConfirmReset() {
  if (!confirmRow) return;
  lv_obj_t* v = lv_obj_get_child(confirmRow, 1);
  lv_label_set_text(v, "");
  lv_obj_set_style_text_color(v, COL_TXT, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(confirmRow, 0), COL_TXT, 0);
  confirmRow = nullptr;
}

static void ajDanger(lv_event_t* e) {
  lv_obj_t* row = lv_event_get_current_target(e);
  if (confirmRow == row) {
    ajConfirmReset();
    if (row == ajReiniciar) { ajustes_save(); ESP.restart(); }
    // Apagar: el panel duerme y la luz se va; el primer toque lo trae.
    apagada = true;
    dormido = true;
    display_sleep(true);
    ajLeave();
    display_backlight_fade(0, UI_MS_DUSK);
    return;
  }
  ajConfirmReset();
  confirmRow = row;
  confirmAt  = millis();
  lv_obj_t* v = lv_obj_get_child(row, 1);
  lv_label_set_text(v, row == ajReiniciar ? "Tocar para reiniciar" : "Tocar para apagar");
  lv_obj_set_style_text_font(v, UI_FONT_STATE, 0);
  lv_obj_set_style_text_color(v, COL_ACCENT, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(row, 0), COL_ACCENT, 0);
}

static void ajBrilloEvent(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  uint8_t v = lv_slider_get_value(ajSlider);
  if (c == LV_EVENT_VALUE_CHANGED) {
    ajustes().brillo = v;
    lv_label_set_text_fmt(ajBrilloVal, "%d%%", v);
    display_backlight(v);                       // sin fade: sigue al dedo
  } else if (c == LV_EVENT_RELEASED) {
    brilloAct = v;
    ajustes_save();
  }
}

static void ajAuto(lv_event_t*) {
  ajustes().brillo = BRILLO_AUTO;
  ajustes_save();
  brilloAct = ui_brillo();
  display_backlight_fade(brilloAct, UI_MS_WAKE);
  ajPaint();
}

static void buildAjustes() {
  scrAj = uiScreen();
  uiTitle(scrAj, "Ajustes");
  lv_label_set_text(uiState(scrAj), "TAC-1 \u2022 TacOS " TACOS_VERSION);
  lv_obj_add_event_cb(scrAj, ajSwipe, LV_EVENT_GESTURE, nullptr);   // hacia arriba cierra

  lv_obj_t* g = uiGroup(scrAj, UI_Y_ROW0, 5);
  ajWifi = uiGroupRow(g, "Wi-Fi", "", true, [](lv_event_t*) { redEnter(); }, nullptr);

  // Brillo: el valor a la derecha es tocable y vuelve a Auto; el slider va
  // en medio, entre la etiqueta y el valor.
  lv_obj_t* rb = uiGroupRow(g, "Brillo", "Auto", false, nullptr, nullptr);
  ajBrilloVal = lv_obj_get_child(rb, 1);
  lv_obj_set_style_text_color(ajBrilloVal, COL_ACCENT, 0);
  lv_obj_add_flag(ajBrilloVal, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(ajBrilloVal, 12);
  lv_obj_add_event_cb(ajBrilloVal, ajAuto, LV_EVENT_CLICKED, nullptr);
  ajSlider = uiSlider(rb, 196, BRILLO_MIN, 100);
  lv_obj_align(ajSlider, LV_ALIGN_RIGHT_MID, -60, 0);
  lv_obj_add_event_cb(ajSlider, ajBrilloEvent, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(ajSlider, ajBrilloEvent, LV_EVENT_RELEASED, nullptr);

  // Volumen: existe el altavoz, no existe el audio todavia. Se ve, no actua.
  lv_obj_t* rv = uiGroupRow(g, "Volumen", "Sin Audio", false, nullptr, nullptr);
  uiRowInactive(rv);

  ajApagar    = uiGroupRow(g, "Apagar", "", false, ajDanger, nullptr);
  ajReiniciar = uiGroupRow(g, "Reiniciar", "", false, ajDanger, nullptr);
  uiRowLast(ajReiniciar);
}

// ── Inicio ───────────────────────────────────────────────────────────────────
static void buildHome() {
  scrHome = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scrHome);
  lv_obj_set_style_bg_img_src(scrHome, &wallpaper, 0);
  lv_obj_clear_flag(scrHome, LV_OBJ_FLAG_SCROLLABLE);

  // La hora ocupa de y=135 a 204 (los digitos de 96 px miden 69), la fecha
  // va a 228, como en el mockup.
  lbHora = uiLabel(scrHome, UI_FONT_CLOCK, OPA_MAIN);
  lv_label_set_text(lbHora, "--:--");
  lv_obj_set_style_text_letter_space(lbHora, -4, 0);
  lv_obj_set_pos(lbHora, UI_MARGIN - 2, 135);

  lbFecha = uiLabel(scrHome, UI_FONT_DATA, OPA_AVAIL);
  lv_label_set_text(lbFecha, "Sin Hora");
  lv_obj_set_pos(lbFecha, UI_MARGIN, 229);

  // El estado de la red, en una pastilla. Tocarla abre la Red.
  pill = lv_obj_create(scrHome);
  lv_obj_remove_style_all(pill);
  lv_obj_set_style_bg_color(pill, COL_TXT, 0);
  lv_obj_set_style_bg_opa(pill, OPA_SURFACE, 0);
  lv_obj_set_style_radius(pill, UI_PILL_RADIUS, 0);
  lv_obj_set_style_pad_ver(pill, 10, 0);            // 36 de alto con texto de 14
  lv_obj_set_style_pad_hor(pill, 14, 0);
  lv_obj_set_style_pad_column(pill, 8, 0);
  lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_align(pill, LV_ALIGN_TOP_RIGHT, -UI_MARGIN, 22);
  lv_obj_add_flag(pill, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(pill, [](lv_event_t*) { redEnter(); }, LV_EVENT_CLICKED, nullptr);
  pillDot = lv_obj_create(pill);                     // un punto de 8: el color es el estado
  lv_obj_remove_style_all(pillDot);
  lv_obj_set_size(pillDot, 8, 8);
  lv_obj_set_style_radius(pillDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(pillDot, COL_TXT, 0);
  lv_obj_set_style_bg_opa(pillDot, OPA_INACTIVE, 0);
  pillTxt = uiLabel(pill, UI_FONT_STATE, OPA_AVAIL);
  lv_label_set_text(pillTxt, "Sin Red");
  carousel(scrHome, APP_INICIO);
}

static void homeTick() {
  struct tm t;
  if (net_time(&t)) {
    if (t.tm_min != lastMin) {
      lastMin = t.tm_min;
      lv_label_set_text_fmt(lbHora, "%02d:%02d", t.tm_hour, t.tm_min);
      lv_label_set_text_fmt(lbFecha, "%s %d de %s", DIAS[t.tm_wday], t.tm_mday, MESES[t.tm_mon]);
    }
  }
  NetState n = net_state();
  if (n != lastNet) {
    lastNet = n;
    bool up = n == NET_UP;
    lv_label_set_text(pillTxt, up ? net_ssid() : n == NET_CONNECTING ? "Conectando" : "Sin Red");
    // Verde = "esta bien" (estado); ambar = "esta corriendo" (conectando).
    // Sin red no hay color: solo se ve que existe.
    lv_obj_set_style_bg_color(pillDot, up ? COL_OK : n == NET_CONNECTING ? COL_ACCENT : COL_TXT, 0);
    lv_obj_set_style_bg_opa(pillDot, up || n == NET_CONNECTING ? LV_OPA_COVER : OPA_INACTIVE, 0);
  }
}

static void redTick() {
  uint32_t now = millis();
  if (redVista == RV_LISTA) {
    int n = net_scan_count();
    if (redScanN < 0 && n >= 0) { redScanN = n; redFill(); }
    if (redScanN >= 0 && now - redScanAt > SCAN_EVERY_MS) redScan();
    return;
  }
  if (redVista != RV_CONECTANDO) return;
  NetState n = net_state();
  if (!redDoneAt) {
    if (n == NET_UP && strcmp(net_ssid(), redSsid) == 0) {
      lv_label_set_text_fmt(redMsg, "Conectado\n%s", net_ip());
      uiFade(redSpin, 0, UI_MS_EXIT);
      redDoneAt = now;
    } else if (n == NET_FAILED) {
      lv_label_set_text(redMsg, "No se pudo conectar");
      uiFade(redSpin, 0, UI_MS_EXIT);
      redDoneAt = now;
    }
    return;
  }
  if (now - redDoneAt > UI_MS_DAWN) {
    bool ok = n == NET_UP;
    redDoneAt = 0;
    if (ok) redLeave(); else redShow(RV_CLAVE);
  }
}

void ui_build() {
  buildHome();
  buildClima();
  buildMercados();
  buildAjustes();
  buildRed();
  lv_scr_load(scrHome);
  brilloAct = ui_brillo();
}

void ui_tick() {
  static uint32_t lastS = 0;
  uint32_t now = millis();

  // Reposo: la luz se retira sola; el primer toque solo la trae de vuelta.
  if (!dormido && lv_disp_get_inactive_time(nullptr) > IDLE_MS) {
    dormido = true;
    display_sleep(true);
    display_backlight_fade(BRILLO_REPOSO, UI_MS_DUSK);
  }
  if (dormido && display_woke()) {
    dormido = false;
    display_sleep(false);
    lv_disp_trig_activity(nullptr);
    if (panelOff) { panelOff = false; display_on(); }
    apagada = false;
    brilloAct = ui_brillo();
    display_backlight_fade(brilloAct, UI_MS_WAKE);
  }
  // Apagar de verdad cuando la luz ya llego a cero: el panel se duerme.
  if (apagada && !panelOff && display_backlight_level() == 0) { panelOff = true; display_off(); }
  if (confirmRow && now - confirmAt > CONFIRM_MS) ajConfirmReset();

  if (enRed) redTick();

  if (now - lastS < 1000) return;                  // lo de abajo va a 1 fps
  lastS = now;
  homeTick();
  if (cloud_weather_dirty()) paintClima();
  if (cloud_markets_dirty()) paintMercados();
  uint8_t b = ui_brillo();                         // dia / noche
  if (!dormido && b != brilloAct) { brilloAct = b; display_backlight_fade(b, UI_MS_DAWN); }
}
