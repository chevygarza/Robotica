#include "app_ui.h"
#include "app_net.h"
#include "app_hue.h"
#include "app_markets.h"
#include "app_server.h"
#include <lvgl.h>
#include <math.h>
#include <string.h>
#include "time.h"

// GIFs embebidos (wallpapers_data.c)
extern "C" {
  extern const uint8_t dball_gif[];   extern const unsigned int dball_gif_len;
  extern const uint8_t pokemon_gif[]; extern const unsigned int pokemon_gif_len;
  extern const uint8_t zelda_gif[];   extern const unsigned int zelda_gif_len;
}

// ---------- Paleta ----------
#define COL_BG     lv_color_hex(0x0B1020)
#define COL_CARD   lv_color_hex(0x161C2E)
#define COL_TXT    lv_color_hex(0xFFFFFF)
#define COL_SUB    lv_color_hex(0x8A93A6)
#define COL_ACCENT lv_color_hex(0x4EA8FF)
#define COL_WARM   lv_color_hex(0xFFB454)
#define COL_X      lv_color_hex(0x1D9BF0)
#define COL_OK     lv_color_hex(0x3DD68C)

#define NUM_APPS 6   // 0=clima, 1=X, 2=Luces(Hue), 3=Mercados, 4=Servidor, 5=Wallpaper

// Pantallas: overview (lista) y detail (info ampliada) por cada app
static lv_obj_t *ovScr[NUM_APPS];
static lv_obj_t *dtScr[NUM_APPS];
static int  curApp   = 0;
static bool inDetail = false;

// --- Estado de navegación Hue ---
enum HueView { HV_NONE, HV_FAVS, HV_ROOMS, HV_LIGHTS, HV_CONTROL };
static HueView hueView = HV_NONE;
static int  hueSel = 0, hueRoomSel = 0, hueLightSel = 0;
static bool hueTargetRoom = false;
static int  hueTargetId = 0;
static char hueTargetName[24] = "";
// widgets Hue
static lv_obj_t *hueCoverInfo;
static lv_obj_t *hueMenuScr, *hueTitle, *hueBox, *hueHint;
static lv_obj_t *hueItems[HUE_MAX_LIGHTS + 4];
static int hueItemCount = 0;
static char hueBuf[HUE_MAX_LIGHTS + 4][28];
static const char *huePtrs[HUE_MAX_LIGHTS + 4];
static void hueEnterRooms();
static void hueEnterLights();

// --- widgets Mercados (app 3) ---
static lv_obj_t *mktSym[MKT_MAX], *mktPrice[MKT_MAX], *mktChg[MKT_MAX];

// --- widgets Servidor (app 4) ---
static lv_obj_t *srvDot, *srvStatus, *srvUptime, *srvRamBar, *srvRamLbl,
                *srvSsdBar, *srvSsdLbl, *srvNet, *srvThermal;

// --- Wallpaper (app 5) ---
static lv_obj_t *wallGif, *wallName;
static int wallCur = 0;
static lv_img_dsc_t wallDsc[3];
static const char *wallNames[3] = { "Dragon Ball", "Pokemon", "Zelda" };

// --- overview clima ---
static lv_obj_t *w_clock, *w_date, *w_temp, *w_cond, *w_extra, *w_wifi, *w_rain;
// --- overview X ---
static lv_obj_t *x_name, *x_user, *x_followers, *x_following, *x_posts;
// --- detail clima / X ---
static lv_obj_t *w_dtext;
static lv_obj_t *x_dname, *x_duser, *x_dbio, *x_dmetrics;
// --- dots ---
static lv_obj_t *dots[NUM_APPS][NUM_APPS];

static lv_obj_t* mkLabel(lv_obj_t *p, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

static lv_obj_t* mkCard(lv_obj_t *p, int w, int h) {
  lv_obj_t *c = lv_obj_create(p);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_color(c, COL_CARD, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_radius(c, 16, 0);
  lv_obj_set_style_pad_all(c, 6, 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

static lv_obj_t* newScreen() {
  lv_obj_t *s = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(s, COL_BG, 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

static const char* wxDesc(int code) {
  switch (code) {
    case 0:  return "Despejado";
    case 1:  return "Mayormente despejado";
    case 2:  return "Parcial nublado";
    case 3:  return "Nublado";
    case 45: case 48: return "Niebla";
    case 51: case 53: case 55: return "Llovizna";
    case 61: case 63: case 65: return "Lluvia";
    case 66: case 67: return "Lluvia helada";
    case 71: case 73: case 75: case 77: return "Nieve";
    case 80: case 81: case 82: return "Chubascos";
    case 95: case 96: case 99: return "Tormenta";
    default: return "--";
  }
}

static void fmtCount(long n, char *out, size_t cap) {
  if (n >= 1000000)   snprintf(out, cap, "%.1fM", n / 1000000.0);
  else if (n >= 1000) snprintf(out, cap, "%.1fK", n / 1000.0);
  else                snprintf(out, cap, "%ld", n);
}

static void fmtThousands(long n, char *out, size_t cap) {
  char b[24]; int l = snprintf(b, sizeof(b), "%ld", n);
  int o = 0;
  for (int i = 0; i < l && o < (int)cap - 1; i++) {
    if (i > 0 && (l - i) % 3 == 0) out[o++] = ',';
    out[o++] = b[i];
  }
  out[o] = 0;
}

static lv_obj_t* hintBack(lv_obj_t *p) {
  lv_obj_t *h = mkLabel(p, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(h, "push: volver");
  lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -22);
  return h;
}

static void buildDots(lv_obj_t *p, int idx) {
  for (int i = 0; i < NUM_APPS; i++) {
    lv_obj_t *d = lv_obj_create(p);
    lv_obj_set_size(d, 8, 8);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_set_style_bg_color(d, (i == idx) ? COL_ACCENT : COL_SUB, 0);
    lv_obj_set_style_bg_opa(d, (i == idx) ? LV_OPA_COVER : LV_OPA_40, 0);
    lv_obj_align(d, LV_ALIGN_BOTTOM_MID, (i * 2 - (NUM_APPS - 1)) * 8, -18);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    dots[idx][i] = d;
  }
}

// ---------- Overview 0: Reloj + Clima ----------
static void buildWeather() {
  lv_obj_t *s = newScreen();
  ovScr[0] = s;

  lv_obj_t *city = mkLabel(s, &lv_font_montserrat_16, COL_SUB);
  lv_label_set_text(city, "MONTERREY");
  lv_obj_align(city, LV_ALIGN_TOP_MID, 0, 46);

  w_wifi = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_label_set_text(w_wifi, LV_SYMBOL_WIFI);
  lv_obj_align(w_wifi, LV_ALIGN_TOP_MID, 0, 24);

  w_clock = mkLabel(s, &lv_font_montserrat_48, COL_TXT);
  lv_label_set_text(w_clock, "--:--");
  lv_obj_align(w_clock, LV_ALIGN_TOP_MID, 0, 74);

  w_date = mkLabel(s, &lv_font_montserrat_16, COL_SUB);
  lv_label_set_text(w_date, "");
  lv_obj_align(w_date, LV_ALIGN_TOP_MID, 0, 126);

  w_temp = mkLabel(s, &lv_font_montserrat_44, COL_WARM);
  lv_label_set_text(w_temp, "--");
  lv_obj_align(w_temp, LV_ALIGN_CENTER, 0, 22);

  w_cond = mkLabel(s, &lv_font_montserrat_18, COL_TXT);
  lv_label_set_text(w_cond, "Cargando...");
  lv_obj_align(w_cond, LV_ALIGN_CENTER, 0, 60);

  w_extra = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_label_set_text(w_extra, "");
  lv_obj_set_style_text_align(w_extra, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(w_extra, LV_ALIGN_CENTER, 0, 92);

  w_rain = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_label_set_text(w_rain, "");
  lv_obj_align(w_rain, LV_ALIGN_CENTER, 0, 116);

  buildDots(s, 0);
}

// ---------- Overview 1: Perfil X ----------
static void buildX() {
  lv_obj_t *s = newScreen();
  ovScr[1] = s;

  lv_obj_t *badge = lv_obj_create(s);
  lv_obj_set_size(badge, 46, 46);
  lv_obj_set_style_radius(badge, 12, 0);
  lv_obj_set_style_bg_color(badge, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(badge, 2, 0);
  lv_obj_set_style_border_color(badge, COL_X, 0);
  lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_t *xl = mkLabel(badge, &lv_font_montserrat_28, COL_TXT);
  lv_label_set_text(xl, "X");
  lv_obj_center(xl);

  x_name = mkLabel(s, &lv_font_montserrat_20, COL_TXT);
  lv_label_set_text(x_name, "stuntech");
  lv_obj_align(x_name, LV_ALIGN_TOP_MID, 0, 94);

  x_user = mkLabel(s, &lv_font_montserrat_16, COL_X);
  lv_label_set_text(x_user, "@stuntech");
  lv_obj_align(x_user, LV_ALIGN_TOP_MID, 0, 120);

  const int CW = 96, CH = 78, GAP = 6;
  int totalW = CW * 3 + GAP * 2;
  int startX = -(totalW / 2) + CW / 2;
  lv_obj_t **vals[3]  = { &x_followers, &x_following, &x_posts };
  const char *labels[3] = { "Seguidores", "Siguiendo", "Posts" };
  for (int i = 0; i < 3; i++) {
    lv_obj_t *c = mkCard(s, CW, CH);
    lv_obj_align(c, LV_ALIGN_CENTER, startX + i * (CW + GAP), 36);
    lv_obj_t *v = mkLabel(c, &lv_font_montserrat_24, COL_TXT);
    lv_label_set_text(v, "--");
    lv_obj_align(v, LV_ALIGN_CENTER, 0, -10);
    lv_obj_t *t = mkLabel(c, &lv_font_montserrat_12, COL_SUB);
    lv_label_set_text(t, labels[i]);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, 22);
    *vals[i] = v;
  }

  buildDots(s, 1);
}

// ---------- Detail 0: Clima ampliado ----------
static void buildWeatherDetail() {
  lv_obj_t *s = newScreen();
  dtScr[0] = s;

  lv_obj_t *title = mkLabel(s, &lv_font_montserrat_18, COL_ACCENT);
  lv_label_set_text(title, "Clima - Monterrey");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

  w_dtext = mkLabel(s, &lv_font_montserrat_14, COL_TXT);
  lv_obj_set_style_text_align(w_dtext, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(w_dtext, "Cargando...");
  lv_obj_align(w_dtext, LV_ALIGN_CENTER, 0, 4);

  hintBack(s);
}

// ---------- Detail 1: X ampliado ----------
static void buildXDetail() {
  lv_obj_t *s = newScreen();
  dtScr[1] = s;

  lv_obj_t *col = lv_obj_create(s);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, 250, 290);
  lv_obj_align(col, LV_ALIGN_CENTER, 0, -8);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 5, 0);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

  x_dname = mkLabel(col, &lv_font_montserrat_20, COL_TXT);
  lv_label_set_text(x_dname, "stuntech");

  x_duser = mkLabel(col, &lv_font_montserrat_14, COL_X);
  lv_label_set_text(x_duser, "@stuntech");

  x_dbio = mkLabel(col, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_long_mode(x_dbio, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(x_dbio, 230);
  lv_obj_set_style_text_align(x_dbio, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(x_dbio, "");

  x_dmetrics = mkLabel(col, &lv_font_montserrat_16, COL_TXT);
  lv_obj_set_style_text_align(x_dmetrics, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(x_dmetrics, "");

  hintBack(s);
}

// ---------- App 2: Luces (Hue) ----------
static lv_obj_t* mkCircle(lv_obj_t *p, int d, uint32_t hex, lv_opa_t opa) {
  lv_obj_t *c = lv_obj_create(p);
  lv_obj_remove_style_all(c);
  lv_obj_set_size(c, d, d);
  lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(hex), 0);
  lv_obj_set_style_bg_opa(c, opa, 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

static void buildHueCover() {
  lv_obj_t *s = newScreen();
  ovScr[2] = s;

  // Logo Hue: bombilla brillante (glow + bulbo + reflejo)
  lv_obj_align(mkCircle(s, 118, 0xFFD36B, LV_OPA_20), LV_ALIGN_CENTER, 0, -40);
  lv_obj_align(mkCircle(s, 84,  0xFFD36B, LV_OPA_40), LV_ALIGN_CENTER, 0, -40);
  lv_obj_align(mkCircle(s, 60,  0xFFE39A, LV_OPA_COVER), LV_ALIGN_CENTER, 0, -40);
  lv_obj_align(mkCircle(s, 16,  0xFFFFFF, LV_OPA_70), LV_ALIGN_CENTER, -12, -52);

  lv_obj_t *wm = mkLabel(s, &lv_font_montserrat_28, COL_TXT);
  lv_label_set_text(wm, "hue");
  lv_obj_align(wm, LV_ALIGN_CENTER, 0, 26);

  hueCoverInfo = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_obj_set_style_text_align(hueCoverInfo, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(hueCoverInfo, "Philips Hue");
  lv_obj_align(hueCoverInfo, LV_ALIGN_CENTER, 0, 54);

  lv_obj_t *h = mkLabel(s, &lv_font_montserrat_14, COL_ACCENT);
  lv_label_set_text(h, "push: entrar");
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 88);

  buildDots(s, 2);
}

static void buildHueMenuScreen() {
  lv_obj_t *s = newScreen();
  hueMenuScr = s;

  hueTitle = mkLabel(s, &lv_font_montserrat_18, COL_ACCENT);
  lv_obj_set_style_text_align(hueTitle, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(hueTitle, "Luces");
  lv_obj_align(hueTitle, LV_ALIGN_TOP_MID, 0, 28);

  hueBox = lv_obj_create(s);
  lv_obj_remove_style_all(hueBox);
  lv_obj_set_size(hueBox, 280, 222);
  lv_obj_align(hueBox, LV_ALIGN_CENTER, 0, 8);
  lv_obj_set_flex_flow(hueBox, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(hueBox, 3, 0);
  lv_obj_set_scroll_dir(hueBox, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(hueBox, LV_SCROLLBAR_MODE_OFF);

  hueHint = mkLabel(s, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(hueHint, "girar: mover   push: ok   manten: atras");
  lv_obj_align(hueHint, LV_ALIGN_BOTTOM_MID, 0, -12);
}

static void hueApplyHighlight() {
  for (int i = 0; i < hueItemCount; i++) {
    bool sel = (i == hueSel);
    lv_obj_set_style_bg_color(hueItems[i], COL_ACCENT, 0);
    lv_obj_set_style_bg_opa(hueItems[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(hueItems[i], sel ? COL_BG : COL_TXT, 0);
  }
  if (hueSel >= 0 && hueSel < hueItemCount)
    lv_obj_scroll_to_view(hueItems[hueSel], LV_ANIM_ON);
}

static void hueRenderMenu(const char *title, const char *const *items, int count, int sel) {
  lv_label_set_text(hueTitle, title);
  lv_obj_clean(hueBox);
  if (count > HUE_MAX_LIGHTS + 4) count = HUE_MAX_LIGHTS + 4;
  hueItemCount = count;
  hueSel = (sel < 0) ? 0 : (sel >= count ? count - 1 : sel);
  for (int i = 0; i < count; i++) {
    lv_obj_t *it = lv_label_create(hueBox);
    lv_obj_set_width(it, lv_pct(100));
    lv_obj_set_style_text_font(it, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_ver(it, 5, 0);
    lv_obj_set_style_pad_hor(it, 10, 0);
    lv_obj_set_style_radius(it, 8, 0);
    lv_label_set_long_mode(it, LV_LABEL_LONG_DOT);
    lv_label_set_text(it, items[i]);
    hueItems[i] = it;
  }
  hueApplyHighlight();
}

static void hueEnterRooms() {
  int n = 0;
  if (hue_lock(60)) {
    n = g_roomCount;
    for (int i = 0; i < n; i++) {
      snprintf(hueBuf[i], 28, "%s (%d)", g_rooms[i].name, g_rooms[i].nLights);
      huePtrs[i] = hueBuf[i];
    }
    hue_unlock();
  }
  if (n == 0) { snprintf(hueBuf[0], 28, "Cargando..."); huePtrs[0] = hueBuf[0]; n = 1; }
  hueRenderMenu("Cuartos", huePtrs, n, hueSel);
}

static void hueEnterLights() {
  int idx = 0;
  char title[28] = "Cuarto";
  if (hue_lock(60)) {
    if (hueRoomSel < g_roomCount) {
      HueRoom *r = &g_rooms[hueRoomSel];
      strncpy(title, r->name, sizeof(title) - 1); title[sizeof(title) - 1] = 0;
      snprintf(hueBuf[idx], 28, "Todo el cuarto"); huePtrs[idx] = hueBuf[idx]; idx++;
      for (int j = 0; j < r->nLights && idx < HUE_MAX_LIGHTS + 3; j++) {
        HueLight *l = hue_findLight(r->lightIds[j]);
        const char *nm = l ? l->name : "?";
        bool on = l ? l->on : false;
        snprintf(hueBuf[idx], 28, "%s %s", on ? "[ON]" : "[  ]", nm);
        huePtrs[idx] = hueBuf[idx]; idx++;
      }
    }
    hue_unlock();
  }
  if (idx == 0) { snprintf(hueBuf[0], 28, "Cargando..."); huePtrs[0] = hueBuf[0]; idx = 1; }
  hueRenderMenu(title, huePtrs, idx, hueSel);
}

static const char *CTRL_ITEMS[] = {
  "Encender", "Apagar", "Brillo 100%", "Brillo 50%", "Brillo 20%",
  "Blanco", "Azul", "Rojo", "Verde"
};
static const int CTRL_N = 9;

static void hueEnterControl() {
  hueRenderMenu(hueTargetName, CTRL_ITEMS, CTRL_N, hueSel);
}

// ===== Favoritos táctiles del Bunker (grupo 13) =====
struct Fav { const char *label; int group; int onCmd; int bri; HueColor color; uint32_t activeHex; };
static Fav FAVS[3] = {
  { "Relax",  13, 1, 120, HUE_WHITE, 0xFFB454 },   // 50% calido
  { "Blanco", 13, 1, 254, HUE_WHITE, 0xF0F0F0 },   // 100% blanco
  { "Fiesta", 13, 1, 254, HUE_BLUE,  0xB14CFF },   // 2 rojos + 2 azules
};
static int  favActive = -1;
static lv_obj_t *hueFavScr;
static lv_obj_t *favBtn[3], *favLbl[3];

static void updateFavColors() {
  for (int i = 0; i < 3; i++) {
    bool on = (favActive == i);
    lv_obj_set_style_bg_color(favBtn[i], on ? lv_color_hex(FAVS[i].activeHex) : COL_CARD, 0);
    lv_obj_set_style_text_color(favLbl[i], on ? COL_BG : COL_TXT, 0);
  }
}

static void favCb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (favActive == i) {                                   // tap de nuevo = apagar
    hue_set(true, FAVS[i].group, 0, -1, HUE_NONE);
    favActive = -1;
  } else if (i == 2) {                                    // Fiesta: 2 rojos + 2 azules
    int ids[HUE_MAX_ROOMLIGHTS]; int n = 0;
    if (hue_lock(50)) {
      for (int k = 0; k < g_roomCount; k++)
        if (g_rooms[k].id == FAVS[i].group) {
          n = g_rooms[k].nLights;
          for (int j = 0; j < n; j++) ids[j] = g_rooms[k].lightIds[j];
          break;
        }
      hue_unlock();
    }
    if (n == 0) {                                         // aún no carga: respaldo
      hue_set(true, FAVS[i].group, 1, 254, HUE_BLUE);
    } else {
      for (int j = 0; j < n; j++)
        hue_set(false, ids[j], 1, 254, (j % 2 == 0) ? HUE_RED : HUE_BLUE);
    }
    favActive = i;
  } else {
    hue_set(true, FAVS[i].group, FAVS[i].onCmd, FAVS[i].bri, FAVS[i].color);
    favActive = i;
  }
  updateFavColors();
}

static void otrosCb(lv_event_t *e) {
  hueView = HV_ROOMS; hueSel = 0; hueRoomSel = 0;
  hue_request_rooms();
  hueEnterRooms();
  lv_scr_load_anim(hueMenuScr, LV_SCR_LOAD_ANIM_OVER_LEFT, 250, 0, false);
}

static void buildHueFavs() {
  lv_obj_t *s = newScreen();
  hueFavScr = s;

  lv_obj_t *ttl = mkLabel(s, &lv_font_montserrat_28, COL_TXT);
  lv_label_set_text(ttl, "BUNKER");
  lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 40);

  const int BW = 84, BH = 84, GAP = 8;
  int total = BW * 3 + GAP * 2;
  int startX = -(total / 2) + BW / 2;
  for (int i = 0; i < 3; i++) {
    lv_obj_t *b = lv_btn_create(s);
    lv_obj_set_size(b, BW, BH);
    lv_obj_align(b, LV_ALIGN_CENTER, startX + i * (BW + GAP), -6);
    lv_obj_set_style_radius(b, 18, 0);
    lv_obj_set_style_bg_color(b, COL_CARD, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, favCb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *l = mkLabel(b, &lv_font_montserrat_16, COL_TXT);
    lv_label_set_text(l, FAVS[i].label);
    lv_obj_center(l);
    favBtn[i] = b; favLbl[i] = l;
  }

  lv_obj_t *otros = lv_btn_create(s);
  lv_obj_set_size(otros, 244, 44);
  lv_obj_align(otros, LV_ALIGN_CENTER, 0, 80);
  lv_obj_set_style_radius(otros, 14, 0);
  lv_obj_set_style_bg_color(otros, COL_BG, 0);
  lv_obj_set_style_border_width(otros, 1, 0);
  lv_obj_set_style_border_color(otros, COL_SUB, 0);
  lv_obj_set_style_shadow_width(otros, 0, 0);
  lv_obj_add_event_cb(otros, otrosCb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ol = mkLabel(otros, &lv_font_montserrat_16, COL_SUB);
  lv_label_set_text(ol, "Otros cuartos  " LV_SYMBOL_RIGHT);
  lv_obj_center(ol);

  updateFavColors();
}

// ---------- App 3: Mercados (cripto) ----------
static void buildMarketsScreen() {
  lv_obj_t *s = newScreen();
  ovScr[3] = s;

  lv_obj_t *t = mkLabel(s, &lv_font_montserrat_18, COL_ACCENT);
  lv_label_set_text(t, "MERCADOS");
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 38);

  for (int i = 0; i < 3; i++) {
    lv_obj_t *row = mkCard(s, 246, 50);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -34 + i * 58);
    lv_obj_t *sym = mkLabel(row, &lv_font_montserrat_20, COL_TXT);
    lv_label_set_text(sym, "--");
    lv_obj_align(sym, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_t *pr = mkLabel(row, &lv_font_montserrat_16, COL_TXT);
    lv_label_set_text(pr, "--");
    lv_obj_align(pr, LV_ALIGN_RIGHT_MID, -6, -9);
    lv_obj_t *ch = mkLabel(row, &lv_font_montserrat_12, COL_SUB);
    lv_label_set_text(ch, "--");
    lv_obj_align(ch, LV_ALIGN_RIGHT_MID, -6, 10);
    mktSym[i] = sym; mktPrice[i] = pr; mktChg[i] = ch;
  }

  buildDots(s, 3);
}

// ---------- App 4: Servidor (Mac Mini) ----------
static lv_obj_t* mkBar(lv_obj_t *p, int y) {
  lv_obj_t *b = lv_bar_create(p);
  lv_obj_set_size(b, 150, 12);
  lv_obj_align(b, LV_ALIGN_CENTER, 6, y);
  lv_obj_set_style_radius(b, 6, 0);
  lv_obj_set_style_bg_color(b, COL_CARD, 0);
  lv_obj_set_style_bg_color(b, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(b, 6, LV_PART_INDICATOR);
  lv_bar_set_range(b, 0, 100);
  lv_bar_set_value(b, 0, LV_ANIM_OFF);
  return b;
}

static void buildServerScreen() {
  lv_obj_t *s = newScreen();
  ovScr[4] = s;

  srvDot = lv_obj_create(s);
  lv_obj_set_size(srvDot, 12, 12);
  lv_obj_set_style_radius(srvDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(srvDot, 0, 0);
  lv_obj_set_style_bg_color(srvDot, COL_SUB, 0);
  lv_obj_clear_flag(srvDot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(srvDot, LV_ALIGN_TOP_MID, -54, 44);

  lv_obj_t *ttl = mkLabel(s, &lv_font_montserrat_18, COL_TXT);
  lv_label_set_text(ttl, "Mac Mini");
  lv_obj_align(ttl, LV_ALIGN_TOP_MID, 8, 38);

  srvStatus = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_label_set_text(srvStatus, "Conectando...");
  lv_obj_align(srvStatus, LV_ALIGN_TOP_MID, 0, 66);

  // RAM
  lv_obj_t *rl = mkLabel(s, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(rl, "RAM"); lv_obj_align(rl, LV_ALIGN_CENTER, -96, -36);
  srvRamBar = mkBar(s, -36);
  srvRamLbl = mkLabel(s, &lv_font_montserrat_12, COL_TXT);
  lv_label_set_text(srvRamLbl, "--"); lv_obj_align(srvRamLbl, LV_ALIGN_CENTER, 100, -36);

  // SSD
  lv_obj_t *sl = mkLabel(s, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(sl, "SSD"); lv_obj_align(sl, LV_ALIGN_CENTER, -96, -12);
  srvSsdBar = mkBar(s, -12);
  srvSsdLbl = mkLabel(s, &lv_font_montserrat_12, COL_TXT);
  lv_label_set_text(srvSsdLbl, "--"); lv_obj_align(srvSsdLbl, LV_ALIGN_CENTER, 100, -12);

  srvUptime = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_label_set_text(srvUptime, "");
  lv_obj_set_style_text_align(srvUptime, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(srvUptime, LV_ALIGN_CENTER, 0, 18);

  srvNet = mkLabel(s, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(srvNet, "");
  lv_obj_set_style_text_align(srvNet, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(srvNet, LV_ALIGN_CENTER, 0, 44);

  srvThermal = mkLabel(s, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(srvThermal, "");
  lv_obj_align(srvThermal, LV_ALIGN_CENTER, 0, 68);

  buildDots(s, 4);
}

// ---------- App 5: Wallpaper (GIFs) ----------
// El GIF solo existe mientras se está viendo: al salir de la app (o dormir
// la pantalla) se destruye, liberando el buffer de decodificación y el CPU.
static void wallShow(bool on) {
  if (on && !wallGif) {
    wallGif = lv_gif_create(ovScr[5]);
    lv_gif_set_src(wallGif, &wallDsc[wallCur]);
    lv_img_set_zoom(wallGif, 512);            // 2x: 180px -> 360px = full screen
    lv_img_set_antialias(wallGif, false);     // escala entera, nítido y barato
    lv_obj_center(wallGif);
    lv_obj_move_background(wallGif);          // nombre y dots quedan encima
  } else if (!on && wallGif) {
    lv_obj_del(wallGif);
    wallGif = nullptr;
  }
}

void ui_screen_off() { wallShow(false); }
void ui_screen_on()  { wallShow(curApp == 5); }

static void buildWallpaperScreen() {
  lv_obj_t *s = newScreen();
  ovScr[5] = s;

  const uint8_t *datas[3]   = { dball_gif, pokemon_gif, zelda_gif };
  const unsigned int lens[3] = { dball_gif_len, pokemon_gif_len, zelda_gif_len };
  for (int i = 0; i < 3; i++) {
    memset(&wallDsc[i], 0, sizeof(lv_img_dsc_t));
    wallDsc[i].header.cf = LV_IMG_CF_RAW;
    wallDsc[i].header.always_zero = 0;
    wallDsc[i].data_size = lens[i];
    wallDsc[i].data = datas[i];
  }

  // (el GIF se crea al entrar a la app via wallShow)

  wallName = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_label_set_text(wallName, wallNames[0]);
  lv_obj_align(wallName, LV_ALIGN_BOTTOM_MID, 0, -38);

  buildDots(s, 5);
}

void ui_build() {
  buildWeather();
  buildX();
  buildWeatherDetail();
  buildXDetail();
  buildHueCover();
  buildHueFavs();
  buildHueMenuScreen();
  buildMarketsScreen();
  buildServerScreen();
  buildWallpaperScreen();
  lv_scr_load(ovScr[0]);
}

// 🔄 Girar: dentro de Hue = mover en la lista; si no, cambiar de app
void ui_nav(int dir) {
  if (curApp == 2 && hueView != HV_NONE) {        // navegando listas de Hue
    if (hueView == HV_FAVS) return;               // favoritos = táctil, el giro no aplica
    if (hueItemCount <= 0) return;
    hueSel += dir;
    if (hueSel < 0) hueSel = 0;
    if (hueSel >= hueItemCount) hueSel = hueItemCount - 1;
    hueApplyHighlight();
    return;
  }
  if (inDetail) return;                            // en detalle, girar no hace nada
  int n = (curApp + dir) % NUM_APPS;
  if (n < 0) n += NUM_APPS;
  if (n == curApp) return;
  curApp = n;
  lv_scr_load_anim(ovScr[curApp],
                   dir > 0 ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                   250, 0, false);
  if (curApp == 1) net_request_x();
  wallShow(curApp == 5);   // GIF solo activo dentro de Wallpapers
}

// 👇 Push corto: entrar / activar
void ui_select() {
  // --- App Mercados: push = refrescar ---
  if (curApp == 3) { markets_request(); return; }
  // --- App Servidor: push = refrescar ---
  if (curApp == 4) { server_request(); return; }
  // --- App Wallpaper: push = siguiente GIF ---
  if (curApp == 5) {
    wallCur = (wallCur + 1) % 3;
    if (wallGif) {
      lv_gif_set_src(wallGif, &wallDsc[wallCur]);
      lv_obj_center(wallGif);
    }
    lv_label_set_text(wallName, wallNames[wallCur]);
    return;
  }
  // --- App Luces (Hue) ---
  if (curApp == 2) {
    if (hueView == HV_NONE) {                      // cover -> favoritos
      hueView = HV_FAVS;
      hue_request_rooms();                         // precarga focos (para Fiesta individual)
      lv_scr_load_anim(hueFavScr, LV_SCR_LOAD_ANIM_OVER_LEFT, 250, 0, false);
    } else if (hueView == HV_FAVS) {               // push = ir a otros cuartos
      hueView = HV_ROOMS; hueSel = 0; hueRoomSel = 0;
      hue_request_rooms();
      hueEnterRooms();
      lv_scr_load_anim(hueMenuScr, LV_SCR_LOAD_ANIM_OVER_LEFT, 250, 0, false);
    } else if (hueView == HV_ROOMS) {              // cuarto -> sus focos
      hueRoomSel = hueSel;
      hueView = HV_LIGHTS; hueSel = 0;
      hueEnterLights();
    } else if (hueView == HV_LIGHTS) {             // foco/"todo" -> control
      hueLightSel = hueSel;
      if (hue_lock(50)) {
        HueRoom *r = (hueRoomSel < g_roomCount) ? &g_rooms[hueRoomSel] : nullptr;
        if (hueSel == 0 && r) {                    // "Todo el cuarto"
          hueTargetRoom = true; hueTargetId = r->id;
          strncpy(hueTargetName, r->name, sizeof(hueTargetName) - 1);
        } else if (r) {                            // un foco
          HueLight *l = hue_findLight(r->lightIds[hueSel - 1]);
          hueTargetRoom = false; hueTargetId = l ? l->id : 0;
          strncpy(hueTargetName, l ? l->name : "?", sizeof(hueTargetName) - 1);
        }
        hueTargetName[sizeof(hueTargetName) - 1] = 0;
        hue_unlock();
      }
      hueView = HV_CONTROL; hueSel = 0;
      hueEnterControl();
    } else if (hueView == HV_CONTROL) {            // ejecutar acción
      int on = -1, bri = -1; HueColor col = HUE_NONE;
      switch (hueSel) {
        case 0: on = 1; break;
        case 1: on = 0; break;
        case 2: on = 1; bri = 254; break;
        case 3: on = 1; bri = 127; break;
        case 4: on = 1; bri = 50;  break;
        case 5: col = HUE_WHITE; break;
        case 6: col = HUE_BLUE;  break;
        case 7: col = HUE_RED;   break;
        case 8: col = HUE_GREEN; break;
      }
      hue_set(hueTargetRoom, hueTargetId, on, bri, col);
    }
    return;
  }
  // --- Clima / X: entrar al detalle ---
  if (!inDetail) {
    inDetail = true;
    lv_scr_load_anim(dtScr[curApp], LV_SCR_LOAD_ANIM_OVER_TOP, 250, 0, false);
    if (curApp == 1) net_request_x();
  } else {
    inDetail = false;
    lv_scr_load_anim(ovScr[curApp], LV_SCR_LOAD_ANIM_OVER_BOTTOM, 250, 0, false);
  }
}

// 👇⏳ Push largo: atrás / subir un nivel
void ui_back() {
  if (curApp == 2 && hueView != HV_NONE) {
    if (hueView == HV_CONTROL) {
      hueView = HV_LIGHTS; hueSel = hueLightSel; hueEnterLights();
    } else if (hueView == HV_LIGHTS) {
      hueView = HV_ROOMS; hueSel = hueRoomSel; hueEnterRooms();
    } else if (hueView == HV_ROOMS) {              // cuartos -> favoritos
      hueView = HV_FAVS;
      lv_scr_load_anim(hueFavScr, LV_SCR_LOAD_ANIM_OVER_RIGHT, 250, 0, false);
    } else {                                       // favoritos -> cover
      hueView = HV_NONE;
      lv_scr_load_anim(ovScr[2], LV_SCR_LOAD_ANIM_OVER_RIGHT, 250, 0, false);
    }
    return;
  }
  if (inDetail) {
    inDetail = false;
    lv_scr_load_anim(ovScr[curApp], LV_SCR_LOAD_ANIM_OVER_BOTTOM, 250, 0, false);
  }
}

void ui_tick() {
  // --- Reloj ---
  struct tm tm;
  static const char *dias[7]   = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};
  static const char *meses[12] = {"Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic"};
  if (getLocalTime(&tm, 0)) {
    int h12 = tm.tm_hour % 12; if (h12 == 0) h12 = 12;
    const char *ap = (tm.tm_hour < 12) ? "AM" : "PM";
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d %s", h12, tm.tm_min, ap);
    lv_label_set_text(w_clock, buf);
    char db[24];
    snprintf(db, sizeof(db), "%s %d %s", dias[tm.tm_wday % 7], tm.tm_mday, meses[tm.tm_mon % 12]);
    lv_label_set_text(w_date, db);
  }

  static uint32_t last = 0;
  if (millis() - last < 500) return;
  last = millis();

  if (!net_lock(50)) { Serial.println("[ui] net_lock FAIL"); return; }
  AppState st = g_state;
  net_unlock();

  static uint32_t dbg = 0;
  if (millis() - dbg > 3000) {
    dbg = millis();
    Serial.printf("[ui] wifi=%d wx.valid=%d x.valid=%d temp=%.1f foll=%ld status=%s\n",
                  st.wifiUp, st.weather.valid, st.x.valid,
                  (double)st.weather.tempC, st.x.followers, st.status);
  }

  lv_obj_set_style_text_color(w_wifi, st.wifiUp ? COL_OK : COL_SUB, 0);

  if (st.weather.valid) {
    char t[12]; snprintf(t, sizeof(t), "%d°", (int)lround(st.weather.tempC));
    lv_label_set_text(w_temp, t);
    lv_label_set_text(w_cond, wxDesc(st.weather.code));
    char e[64];
    snprintf(e, sizeof(e), LV_SYMBOL_UP "%d°  " LV_SYMBOL_DOWN "%d°   %d%%RH   %dkm/h",
             (int)lround(st.weather.tMax), (int)lround(st.weather.tMin),
             st.weather.humidity, (int)lround(st.weather.windKmh));
    lv_label_set_text(w_extra, e);

    // Aviso de lluvia en la pantalla principal
    if (st.weather.rainSoon) {
      int H = st.weather.rainHour; int h12 = H % 12; if (h12 == 0) h12 = 12;
      const char *ap = (H < 12) ? "am" : "pm";
      char r[48]; snprintf(r, sizeof(r), "Lluvia ~%d%s (%d%%)", h12, ap, st.weather.rainProb);
      lv_label_set_text(w_rain, r);
      lv_obj_set_style_text_color(w_rain, COL_X, 0);
    } else {
      lv_label_set_text(w_rain, "Sin lluvia pronto");
      lv_obj_set_style_text_color(w_rain, COL_SUB, 0);
    }

    // Detalle: resumen + pronóstico de las próximas 5 horas
    char wd[420]; int n = 0;
    n += snprintf(wd + n, sizeof(wd) - n,
             "Ahora %d°  Sensacion %d°\nHumedad %d%%  Viento %dkm/h\nPresion %d hPa  UV %d\n\nProximas horas:",
             (int)lround(st.weather.tempC), (int)lround(st.weather.feelsC),
             st.weather.humidity, (int)lround(st.weather.windKmh),
             (int)lround(st.weather.pressure), (int)lround(st.weather.uvMax));
    if (st.weather.hourlyValid) {
      for (int i = 0; i < 5; i++) {
        int H = st.weather.hrHour[i]; int h12 = H % 12; if (h12 == 0) h12 = 12;
        const char *ap = (H < 12) ? "am" : "pm";
        n += snprintf(wd + n, sizeof(wd) - n, "\n %d%s   %d°   %d%% lluvia",
                      h12, ap, st.weather.hrTemp[i], st.weather.hrProb[i]);
      }
    }
    lv_label_set_text(w_dtext, wd);
  }

  if (st.x.valid) {
    lv_label_set_text(x_name, st.x.name);
    char u[40]; snprintf(u, sizeof(u), "@%s", st.x.username);
    lv_label_set_text(x_user, u);
    char a[12], b[12], c[12];
    fmtCount(st.x.followers, a, sizeof(a));
    fmtCount(st.x.following, b, sizeof(b));
    fmtCount(st.x.tweets,    c, sizeof(c));
    lv_label_set_text(x_followers, a);
    lv_label_set_text(x_following, b);
    lv_label_set_text(x_posts,     c);

    lv_label_set_text(x_dname, st.x.name);
    lv_label_set_text(x_duser, u);
    lv_label_set_text(x_dbio, st.x.bio);
    char fa[20], fb[20], fc[20], fd[20], xd[200];
    fmtThousands(st.x.followers, fa, sizeof(fa));
    fmtThousands(st.x.following, fb, sizeof(fb));
    fmtThousands(st.x.tweets,    fc, sizeof(fc));
    fmtThousands(st.x.listed,    fd, sizeof(fd));
    snprintf(xd, sizeof(xd),
             "Seguidores: %s\nSiguiendo: %s\nPosts: %s\nListas: %s\nDesde: %.7s",
             fa, fb, fc, fd, st.x.created);
    lv_label_set_text(x_dmetrics, xd);
  }

  // --- Luces (Hue) ---
  lv_label_set_text(hueCoverInfo, hue_status());
  if (curApp == 2 && hueView != HV_NONE && hue_consumeDirty()) {
    if (hueView == HV_ROOMS)  hueEnterRooms();
    else if (hueView == HV_LIGHTS) hueEnterLights();
  }

  // --- Mercados ---
  if (markets_lock(10)) {
    for (int i = 0; i < g_coinCount && i < 3; i++) {
      if (!g_coins[i].valid) continue;
      lv_label_set_text(mktSym[i], g_coins[i].sym);
      char p[24];
      double pr = g_coins[i].price;
      if (pr >= 1000)     snprintf(p, sizeof(p), "$%.0f", pr);
      else if (pr >= 1)   snprintf(p, sizeof(p), "$%.2f", pr);
      else                snprintf(p, sizeof(p), "$%.4f", pr);
      lv_label_set_text(mktPrice[i], p);
      char c[16]; snprintf(c, sizeof(c), "%+.1f%%", g_coins[i].chg24);
      lv_label_set_text(mktChg[i], c);
      lv_obj_set_style_text_color(mktChg[i], g_coins[i].chg24 >= 0 ? COL_OK : lv_color_hex(0xFF5B6E), 0);
    }
    markets_unlock();
  }

  // --- Servidor (Mac Mini) ---
  if (server_lock(10)) {
    if (g_srv.valid) {
      lv_color_t sc = COL_OK; const char *sw = "Saludable";
      if (strcmp(g_srv.status, "warn") == 0)     { sc = COL_WARM; sw = "Alerta"; }
      else if (strcmp(g_srv.status, "critical") == 0) { sc = lv_color_hex(0xFF5B6E); sw = "Critico"; }
      lv_obj_set_style_bg_color(srvDot, sc, 0);
      lv_label_set_text(srvStatus, sw);
      lv_obj_set_style_text_color(srvStatus, sc, 0);

      lv_bar_set_value(srvRamBar, g_srv.ram_pct, LV_ANIM_OFF);
      char rb[16]; snprintf(rb, sizeof(rb), "%d%%", g_srv.ram_pct); lv_label_set_text(srvRamLbl, rb);
      lv_bar_set_value(srvSsdBar, g_srv.ssd_pct, LV_ANIM_OFF);
      char sb[16]; snprintf(sb, sizeof(sb), "%d%%", g_srv.ssd_pct); lv_label_set_text(srvSsdLbl, sb);

      char up[40]; snprintf(up, sizeof(up), "Up %s   Load %.1f", g_srv.uptime, g_srv.load[0]);
      lv_label_set_text(srvUptime, up);

      char nt[48];
      if (!g_srv.net_up)              snprintf(nt, sizeof(nt), "Red: caida");
      else if (g_srv.down_mbps < 0)   snprintf(nt, sizeof(nt), "Red: ok");
      else snprintf(nt, sizeof(nt), LV_SYMBOL_DOWN "%.0f " LV_SYMBOL_UP "%.0f Mbps  %.0fms",
                    g_srv.down_mbps, g_srv.up_mbps, g_srv.ping_ms);
      lv_label_set_text(srvNet, nt);

      char th[28]; snprintf(th, sizeof(th), "Thermal: %s", g_srv.thermal[0] ? g_srv.thermal : "--");
      lv_label_set_text(srvThermal, th);
    } else {
      lv_label_set_text(srvStatus, g_srv.err[0] ? g_srv.err : "Sin datos");
      lv_obj_set_style_bg_color(srvDot, COL_SUB, 0);
    }
    server_unlock();
  }
}
