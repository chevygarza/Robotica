#include "app_ui.h"
#include "app_net.h"
#include "app_hue.h"
#include "app_markets.h"
#include "app_wol.h"
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
#define COL_OK     lv_color_hex(0x3DD68C)
#define COL_BAD    lv_color_hex(0xFF5B6E)

// ---------- Orden del menu (lo que recorre el giro) ----------
// Indices con nombre: insertar/mover una app es cambiar esta lista, no cazar
// numeros por todo el archivo. VinilOS entra aqui en la etapa 4.
#define APP_LUCES     0
#define APP_MERCADOS  1
#define APP_PC        2
#define APP_FOTOS     3
#define NUM_APPS      4

// Pantallas: una overview por app
static lv_obj_t *ovScr[NUM_APPS];
static int  curApp = 0;

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

// --- widgets Mercados ---
static lv_obj_t *mktSym[MKT_MAX], *mktPrice[MKT_MAX], *mktChg[MKT_MAX];

// --- Fotos (wallpapers) ---
static lv_obj_t *wallGif, *wallName;
static int wallCur = 0;
static lv_img_dsc_t wallDsc[3];
static const char *wallNames[3] = { "Dragon Ball", "Pokemon", "Zelda" };

// --- PC Gamer: cover + menu navegable con la perilla ---
enum PgView { PG_COVER, PG_MENU, PG_WAKING, PG_OFFING };
static PgView pgView = PG_COVER;
static uint32_t pgT = 0, pgInfoUntil = 0, pgLastReq = 0;
static lv_obj_t *pgIcon, *pgDot, *pgStatus, *pgHint;   // cover
// menu (lista navegable: girar mueve, push activa, manten = atras)
static lv_obj_t *pgMenuScr, *pgMenuTitle, *pgMenuBox, *pgMenuHint;
static lv_obj_t *pgItems[5];
static int pgItemCount = 0, pgSel = 0;
enum PgAction { PA_ON, PA_OFF, PA_NORMAL, PA_SIM, PA_TV };
static PgAction pgActions[5];
static bool pgConfirming = false;       // 1er push en Prender/Apagar pide confirmar
static uint32_t pgBootT = 0;            // momento del WoL (lockout de arranque)
static const char *PG_PATHS[3] = { "normal", "sim", "tv" };  // indexado por action-PA_NORMAL

// Estado de la PC: consulta DIRECTA al agente (task persistente, ~3s).
// Antes habia fallback al health.json del Mac Mini; esa app ya no existe.
static bool pcOnlineNow() { return pc_direct_state() == 1; }

// LOCKOUT de arranque: los perfiles (Normal/Sim/TV) solo cuando la PC esta
// encendida Y ESTABLE (uptime>=2min). Asi un perfil no se dispara en pleno boot.
// Fallback: si tras 150s del WoL no hay uptime>=2, habilitar de todos modos.
static bool pcReady() {
  if (pc_direct_state() != 1) return false;
  if (g_pcUptimeMin >= 2) return true;
  if (pgBootT && millis() - pgBootT > 150000) return true;   // fallback duro
  return false;
}

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

// ---------- App: Luces (Hue) ----------
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
  ovScr[APP_LUCES] = s;

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

  buildDots(s, APP_LUCES);
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

  // Sin hint en la curva inferior (ilegible en pantalla redonda): los gestos
  // perilla/push/manten son convencion global del CROWN. Ver design system.
  hueHint = mkLabel(s, &lv_font_montserrat_12, COL_SUB);
  lv_label_set_text(hueHint, "");
  lv_obj_align(hueHint, LV_ALIGN_TOP_MID, 0, 40);
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

// ---------- App: Mercados (cripto) ----------
static void buildMarketsScreen() {
  lv_obj_t *s = newScreen();
  ovScr[APP_MERCADOS] = s;

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

  buildDots(s, APP_MERCADOS);
}

// ---------- App: Fotos (GIFs) ----------
// El GIF solo existe mientras se está viendo: al salir de la app (o dormir
// la pantalla) se destruye, liberando el buffer de decodificación y el CPU.
static void wallShow(bool on) {
  if (on && !wallGif) {
    wallGif = lv_gif_create(ovScr[APP_FOTOS]);
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
void ui_screen_on()  { wallShow(curApp == APP_FOTOS); }

static void buildWallpaperScreen() {
  lv_obj_t *s = newScreen();
  ovScr[APP_FOTOS] = s;

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

  buildDots(s, APP_FOTOS);
}

// ---------- App: PC Gamer (cover + menu navegable) ----------
// Cover: estado de la PC; push = entra al menu. Menu: girar mueve, push activa,
// manten = atras. Modos (Normal/Sim/TV) solo cuando la PC esta encendida.
static int pgMenuOnline = -1;             // estado con que se construyo el menu
static int pgReadyShown = -1;             // ultimo "ready" pintado en el menu
static int pgProfileItem = -1;            // item de perfil esperando confirmacion

static void pgApplyHighlight() {
  bool ready = pcReady();
  for (int i = 0; i < pgItemCount; i++) {
    bool sel = (i == pgSel);
    bool isMode = (pgActions[i] == PA_NORMAL || pgActions[i] == PA_SIM || pgActions[i] == PA_TV);
    bool locked = isMode && !ready;          // perfil bloqueado durante el boot
    lv_obj_set_style_bg_color(pgItems[i], COL_ACCENT, 0);
    lv_obj_set_style_bg_opa(pgItems[i], (sel && !locked) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(pgItems[i], locked ? COL_SUB : (sel ? COL_BG : COL_TXT), 0);
  }
}

// Apagada -> [Prender]. Encendida -> [Apagar, Normal, Sim, TV].
static void pgRenderMenu() {
  lv_obj_clean(pgMenuBox);
  pgConfirming = false;
  bool online = pcOnlineNow();
  pgMenuOnline = online ? 1 : 0;
  const char *labels[5]; int n = 0;
  if (online) {
    labels[n] = "Apagar PC";   pgActions[n] = PA_OFF;    n++;
    labels[n] = "Modo Normal"; pgActions[n] = PA_NORMAL; n++;
    labels[n] = "Modo Sim";    pgActions[n] = PA_SIM;    n++;
    labels[n] = "Modo TV";     pgActions[n] = PA_TV;     n++;
  } else {
    labels[n] = "Prender PC";  pgActions[n] = PA_ON;     n++;
  }
  pgItemCount = n;
  if (pgSel >= n) pgSel = 0;
  for (int i = 0; i < n; i++) {
    lv_obj_t *it = lv_label_create(pgMenuBox);
    lv_obj_set_width(it, lv_pct(100));
    lv_obj_set_style_text_font(it, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_ver(it, 9, 0);
    lv_obj_set_style_radius(it, 10, 0);
    lv_obj_set_style_text_align(it, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(it, labels[i]);
    pgItems[i] = it;
  }
  pgApplyHighlight();
}

static void pgEnterMenu() {
  pgView = PG_MENU; pgSel = 0;
  pgRenderMenu();
  lv_scr_load_anim(pgMenuScr, LV_SCR_LOAD_ANIM_OVER_LEFT, 250, 0, false);
}

static void buildGamerMenu() {
  lv_obj_t *s = newScreen();
  pgMenuScr = s;

  pgMenuTitle = mkLabel(s, &lv_font_montserrat_18, COL_ACCENT);
  lv_obj_set_style_text_align(pgMenuTitle, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(pgMenuTitle, "PC GAMER");
  lv_obj_align(pgMenuTitle, LV_ALIGN_TOP_MID, 0, 28);

  pgMenuBox = lv_obj_create(s);
  lv_obj_remove_style_all(pgMenuBox);
  lv_obj_set_size(pgMenuBox, 280, 190);
  lv_obj_align(pgMenuBox, LV_ALIGN_CENTER, 0, 20);
  lv_obj_set_flex_flow(pgMenuBox, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(pgMenuBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(pgMenuBox, 8, 0);
  lv_obj_clear_flag(pgMenuBox, LV_OBJ_FLAG_SCROLLABLE);

  // Feedback/estado: debajo del titulo (zona visible del circulo), vacio en reposo.
  pgMenuHint = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_label_set_text(pgMenuHint, "");
  lv_obj_align(pgMenuHint, LV_ALIGN_TOP_MID, 0, 52);
}

static void buildGamerScreen() {
  lv_obj_t *s = newScreen();
  ovScr[APP_PC] = s;

  lv_obj_t *t = mkLabel(s, &lv_font_montserrat_18, COL_ACCENT);
  lv_label_set_text(t, "PC GAMER");
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);

  lv_obj_t *ring = lv_obj_create(s);
  lv_obj_set_size(ring, 110, 110);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ring, COL_CARD, 0);
  lv_obj_set_style_border_width(ring, 3, 0);
  lv_obj_set_style_border_color(ring, COL_ACCENT, 0);
  lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(ring, LV_ALIGN_CENTER, 0, -24);
  pgIcon = mkLabel(ring, &lv_font_montserrat_48, COL_SUB);
  lv_label_set_text(pgIcon, LV_SYMBOL_POWER);
  lv_obj_center(pgIcon);

  pgDot = lv_obj_create(s);
  lv_obj_set_size(pgDot, 10, 10);
  lv_obj_set_style_radius(pgDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(pgDot, 0, 0);
  lv_obj_set_style_bg_color(pgDot, COL_SUB, 0);
  lv_obj_clear_flag(pgDot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(pgDot, LV_ALIGN_CENTER, -56, 56);

  pgStatus = mkLabel(s, &lv_font_montserrat_16, COL_TXT);
  lv_label_set_text(pgStatus, "Consultando...");
  lv_obj_align(pgStatus, LV_ALIGN_CENTER, 8, 56);

  pgHint = mkLabel(s, &lv_font_montserrat_14, COL_SUB);
  lv_label_set_long_mode(pgHint, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(pgHint, 250);
  lv_obj_set_style_text_align(pgHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(pgHint, "push: opciones");
  lv_obj_align(pgHint, LV_ALIGN_CENTER, 0, 96);

  buildDots(s, APP_PC);
}

void ui_build() {
  buildHueCover();
  buildHueFavs();
  buildHueMenuScreen();
  buildGamerMenu();
  buildMarketsScreen();
  buildWallpaperScreen();
  buildGamerScreen();
  lv_scr_load(ovScr[APP_LUCES]);
}

// 🔄 Girar: dentro de Hue/PC = mover en la lista; si no, cambiar de app
void ui_nav(int dir) {
  if (curApp == APP_PC && pgView == PG_MENU) {    // navegando el menu de PC Gamer
    if (pgItemCount <= 0) return;
    pgSel += dir;
    if (pgSel < 0) pgSel = 0;
    if (pgSel >= pgItemCount) pgSel = pgItemCount - 1;
    pgConfirming = false;                          // moverse cancela la confirmacion
    lv_label_set_text(pgMenuHint, "");
    pgApplyHighlight();
    return;
  }
  if (curApp == APP_LUCES && hueView != HV_NONE) { // navegando listas de Hue
    if (hueView == HV_FAVS) return;               // favoritos = táctil, el giro no aplica
    if (hueItemCount <= 0) return;
    hueSel += dir;
    if (hueSel < 0) hueSel = 0;
    if (hueSel >= hueItemCount) hueSel = hueItemCount - 1;
    hueApplyHighlight();
    return;
  }
  int n = (curApp + dir) % NUM_APPS;
  if (n < 0) n += NUM_APPS;
  if (n == curApp) return;
  curApp = n;
  lv_scr_load_anim(ovScr[curApp],
                   dir > 0 ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                   250, 0, false);
  wallShow(curApp == APP_FOTOS);   // GIF solo activo dentro de Fotos
}

// 👇 Push corto: entrar / activar
void ui_select() {
  // --- App Mercados: push = refrescar ---
  if (curApp == APP_MERCADOS) { markets_request(); return; }
  // --- App PC Gamer: cover -> menu -> activar opcion ---
  if (curApp == APP_PC) {
    if (pgView == PG_COVER) {            // cover: entrar al menu
      pgEnterMenu();
      return;
    }
    if (pgView == PG_MENU) {
      if (pgItemCount <= 0) return;
      PgAction a = pgActions[pgSel];
      if (a == PA_NORMAL || a == PA_SIM || a == PA_TV) {   // modo: solo si PC lista
        if (!pcReady()) {                                  // lockout de arranque
          lv_label_set_text(pgMenuHint, "Arrancando PC, espera...");
          pgInfoUntil = millis() + 2500;
          return;
        }
        pc_profile_async(PG_PATHS[a - PA_NORMAL]);
        pgProfileItem = pgSel;                             // item en curso (espera resultado)
        lv_obj_set_style_bg_opa(pgItems[pgSel], LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(pgItems[pgSel], COL_WARM, 0);
        lv_label_set_text(pgMenuHint, "Enviando...");
        return;
      }
      // Prender / Apagar: pide confirmacion (2do push)
      if (!pgConfirming) {
        pgConfirming = true;
        lv_label_set_text(pgMenuHint, (a == PA_ON) ? "Prender? push de nuevo"
                                                   : "Apagar? push de nuevo");
        return;
      }
      pgConfirming = false;
      if (a == PA_ON) {
        wol_send();
        pgBootT = millis();                  // arranca el lockout de perfiles
        pgView = PG_WAKING; pgT = millis(); pgLastReq = millis();
        lv_label_set_text(pgHint, "Despertando... 1-2 min");
      } else {
        pc_shutdown_async();
        pgView = PG_OFFING; pgT = millis(); pgLastReq = millis();
        lv_label_set_text(pgHint, "Apagando...");
      }
      lv_scr_load_anim(ovScr[APP_PC], LV_SCR_LOAD_ANIM_OVER_RIGHT, 250, 0, false);  // al cover
      return;
    }
    // PG_WAKING / PG_OFFING: pushes ignorados
    return;
  }
  // --- App Fotos: push = siguiente GIF ---
  if (curApp == APP_FOTOS) {
    wallCur = (wallCur + 1) % 3;
    if (wallGif) {
      lv_gif_set_src(wallGif, &wallDsc[wallCur]);
      lv_obj_center(wallGif);
    }
    lv_label_set_text(wallName, wallNames[wallCur]);
    return;
  }
  // --- App Luces (Hue) ---
  if (curApp == APP_LUCES) {
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
}

// 👇⏳ Push largo: atrás / subir un nivel
void ui_back() {
  if (curApp == APP_PC && pgView == PG_MENU) {     // menu -> volver al cover
    pgView = PG_COVER; pgConfirming = false;
    lv_label_set_text(pgHint, "push: opciones");
    lv_scr_load_anim(ovScr[APP_PC], LV_SCR_LOAD_ANIM_OVER_RIGHT, 250, 0, false);
    return;
  }
  if (curApp == APP_LUCES && hueView != HV_NONE) {
    if (hueView == HV_CONTROL) {
      hueView = HV_LIGHTS; hueSel = hueLightSel; hueEnterLights();
    } else if (hueView == HV_LIGHTS) {
      hueView = HV_ROOMS; hueSel = hueRoomSel; hueEnterRooms();
    } else if (hueView == HV_ROOMS) {              // cuartos -> favoritos
      hueView = HV_FAVS;
      lv_scr_load_anim(hueFavScr, LV_SCR_LOAD_ANIM_OVER_RIGHT, 250, 0, false);
    } else {                                       // favoritos -> cover
      hueView = HV_NONE;
      lv_scr_load_anim(ovScr[APP_LUCES], LV_SCR_LOAD_ANIM_OVER_RIGHT, 250, 0, false);
    }
    return;
  }
}

void ui_tick() {
  // PC Gamer: el task de estado solo consulta mientras se ve la app 6
  pc_status_active(curApp == APP_PC);

  // El throttle de 500ms protege el resto del tick (Hue/Mercados/PC).
  static uint32_t last = 0;
  if (millis() - last < 500) return;
  last = millis();

  // NOTA: el reloj y el clima (app_net) siguen corriendo pero ya no se pintan:
  // la app Clima se fue y su reloj vuelve como REPOSO del aparato en la etapa 5.

  // --- Luces (Hue) ---
  lv_label_set_text(hueCoverInfo, hue_status());
  if (curApp == APP_LUCES && hueView != HV_NONE && hue_consumeDirty()) {
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

  // --- PC Gamer ---
  // Antes este bloque vivia ANIDADO dentro de if(server_lock(10)): el estado de
  // la PC venia del health.json del Mac Mini. Hoy el poll es DIRECTO al agente
  // (task persistente), asi que va suelto y no depende de la app Servidor.
  int dstate    = pc_direct_state();      // 1=encendida 0=apagada -1=desconocido
  bool pcKnown  = (dstate >= 0);
  bool pcOnline = (dstate == 1);

  // Estado en el cover
  if (pcKnown) {
    lv_obj_set_style_bg_color(pgDot, pcOnline ? COL_OK : COL_SUB, 0);
    lv_label_set_text(pgStatus, pcOnline ? "Encendida" : "Apagada");
    lv_obj_set_style_text_color(pgIcon, pcOnline ? COL_OK : COL_SUB, 0);
  } else {
    lv_label_set_text(pgStatus, "Consultando...");
  }

  // En el menu: confirmacion del perfil (item verde si llego, rojo si no)
  if (pgView == PG_MENU && pgProfileItem >= 0 && pgProfileItem < pgItemCount) {
    if (g_pcProfileResult == 2) {                       // POST respondio OK
      lv_obj_set_style_text_color(pgItems[pgProfileItem], COL_OK, 0);
      lv_label_set_text(pgMenuHint, LV_SYMBOL_OK " Listo");
      lv_obj_set_style_text_color(pgMenuHint, COL_OK, 0);
      pgProfileItem = -1; pgInfoUntil = millis() + 1800;
    } else if (g_pcProfileResult == -1) {               // no respondio
      lv_obj_set_style_text_color(pgItems[pgProfileItem], COL_BAD, 0);
      lv_label_set_text(pgMenuHint, "No respondio");
      lv_obj_set_style_text_color(pgMenuHint, COL_BAD, 0);
      pgProfileItem = -1; pgInfoUntil = millis() + 2500;
    }
  }
  // reconstruir si el online cambio; re-pintar si el "ready" cambio
  if (pgView == PG_MENU) {
    if (pcKnown && pgMenuOnline != (pcOnline ? 1 : 0)) { pgRenderMenu(); pgReadyShown = -1; }
    int r = pcReady() ? 1 : 0;
    if (pgReadyShown != r) {
      pgReadyShown = r;
      pgApplyHighlight();                 // desbloquea/bloquea los perfiles
      if (!pgInfoUntil) lv_label_set_text(pgMenuHint, r ? "" : "Arrancando PC...");
    }
  }

  // El poll directo corre solo cada 3s: aqui ya no hay que pedir refrescos.
  if (pgView == PG_WAKING) {
    if (pcOnline) {
      pgView = PG_COVER;
      lv_label_set_text(pgHint, "Encendida! A jugar :)");
      pgInfoUntil = millis() + 6000;
    } else if (millis() - pgT > 180000) {
      pgView = PG_COVER;
      lv_label_set_text(pgHint, "No confirmo en 3 min - revisa la PC");
      pgInfoUntil = millis() + 8000;
    }
  }
  if (pgView == PG_OFFING) {
    if (g_pcShutdownResult == -1) {
      pgView = PG_COVER;
      lv_label_set_text(pgHint, "No pude apagarla (agente instalado?)");
      pgInfoUntil = millis() + 8000;
    } else if (!pcOnline) {
      pgView = PG_COVER;
      lv_label_set_text(pgHint, "Apagada. Buenas noches, gamer");
      pgInfoUntil = millis() + 6000;
    } else if (millis() - pgT > 120000) {
      pgView = PG_COVER;
      lv_label_set_text(pgHint, "Sigue encendida - revisa la PC");
      pgInfoUntil = millis() + 8000;
    }
  }
  // Restaurar hints tras un mensaje temporal
  if (pgInfoUntil && millis() > pgInfoUntil) {
    pgInfoUntil = 0;
    if (pgView == PG_MENU) {
      lv_label_set_text(pgMenuHint, "");
      lv_obj_set_style_text_color(pgMenuHint, COL_SUB, 0);
      pgApplyHighlight();                   // restaura el color normal de los items
    }
    else lv_label_set_text(pgHint, "push: opciones");
  }
}
