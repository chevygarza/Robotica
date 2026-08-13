#include "app_ui.h"
#include "ui_theme.h"
#include "app_net.h"
#include "app_hue.h"
#include "app_markets.h"
#include "app_wol.h"
#include "player.h"
#include "vinyl.h"
#include "albums.h"
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

#define APP_LUCES     0
#define APP_MERCADOS  1
#define APP_MUSICA    2
#define APP_PC        3
#define APP_FOTOS     4
#define NUM_APPS      5

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

// --- Musica (VinilOS): portada -> biblioteca -> reproduciendo ---
// EXCEPCION documentada a la regla 8 (fondos brillantes): dentro de esta app el
// fondo es negro. La caratula llena el disco, asi que la mura del panel casi no
// se ve y el negro es lo que hace que el vinilo se lea como objeto.
enum MusView { MV_COVER, MV_LIB, MV_PLAY };
static bool mktInMenu = false;      // Mercados: portada vs menu
static MusView musView = MV_COVER;
static lv_obj_t *musScr = nullptr;            // pantalla del vinilo (lib + play)
static lv_obj_t *musName, *musSub;            // biblioteca: album y subtitulo
static lv_obj_t *musCard, *musTrack, *musArtist;  // reproduciendo: pista
static lv_obj_t *musVol;                      // overlay de volumen
static uint8_t  musSel = 0;                   // disco que se hojea
static int8_t   musLoaded = -1;               // disco que suena (-1 = ninguno)
static uint8_t  musTrackIx = 0;
static bool     musPaused = false;
static uint32_t musVolUntil = 0;
static void musVinylOn();     // reserva perezosa: definidas mas abajo, pero
static void musVinylOff();    // ui_screen_off/on (arriba) ya las necesita

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

// Envoltorio historico sobre uiLabel: opacidad completa. Para texto secundario
// NO se usa un color gris (COL_SUB murio con INTERFAZ.md §7): se usa uiLabel
// con OPA_AVAIL_LABEL.
static lv_obj_t* mkLabel(lv_obj_t *p, const lv_font_t *font, lv_color_t color) {
  return uiLabel(p, font, color, LV_OPA_COVER);
}

static lv_obj_t* mkCard(lv_obj_t *p, int w, int h) {
  lv_obj_t *c = lv_obj_create(p);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_color(c, COL_SURFACE, 0);
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
    lv_obj_set_style_bg_color(d, (i == idx) ? COL_ACCENT : COL_TXT, 0);
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

  lv_obj_t *wm = mkLabel(s, UI_FONT_TITLE, COL_TXT);
  lv_label_set_text(wm, "hue");
  lv_obj_align(wm, LV_ALIGN_CENTER, 0, 26);

  hueCoverInfo = uiLabel(s, UI_FONT_STATE, COL_TXT, OPA_AVAIL_LABEL);
  lv_obj_set_style_text_align(hueCoverInfo, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(hueCoverInfo, "Philips Hue");
  lv_obj_align(hueCoverInfo, LV_ALIGN_CENTER, 0, 54);

  lv_obj_t *h = mkLabel(s, UI_FONT_STATE, COL_ACCENT);
  lv_label_set_text(h, "push: entrar");
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 88);

  buildDots(s, APP_LUCES);
}

static void buildHueMenuScreen() {
  lv_obj_t *s = newScreen();
  hueMenuScr = s;

  hueTitle = mkLabel(s, UI_FONT_TITLE, COL_ACCENT);
  lv_obj_set_style_text_align(hueTitle, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(hueTitle, "Luces");
  lv_obj_align(hueTitle, LV_ALIGN_TOP_MID, 0, 28);

  hueBox = uiListBox(s);

  hueHint = uiLabel(s, UI_FONT_STATE, COL_TXT, OPA_AVAIL_LABEL);
  lv_label_set_text(hueHint, "");
  lv_obj_align(hueHint, LV_ALIGN_TOP_MID, 0, 40);
}

static void hueApplyHighlight() {
  for (int i = 0; i < hueItemCount; i++)
    uiRowFocus(hueItems[i], i == hueSel, false, false);
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
    hueItems[i] = uiRow(hueBox, items[i], "");
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
static void updateFavColors();   // definida con la lista de favoritos

static void favActivate(int i) {
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

// Favoritos del Bunker. ERAN tres botones tactiles; INTERFAZ.md apaga el dedo
// (§3) y exige que todo menu sea una lista vertical (§4, regla 3). El estado
// activo, que antes se veia pintando el boton, ahora es la columna de valor.
static const int FAV_ROWS = 4;                 // 3 escenas + "Otros Cuartos"
static lv_obj_t *favRow[FAV_ROWS];

static void updateFavColors() {
  for (int i = 0; i < 3; i++)
    lv_label_set_text(uiRowValue(favRow[i]), (favActive == i) ? "Encendido" : "");
}

static void hueFavHighlight() {
  for (int i = 0; i < FAV_ROWS; i++) uiRowFocus(favRow[i], i == hueSel, false, false);
}

static void buildHueFavs() {
  lv_obj_t *s = newScreen();
  hueFavScr = s;

  uiTitle(s, "Luces");
  lv_obj_t *box = uiListBox(s);
  for (int i = 0; i < 3; i++) favRow[i] = uiRow(box, FAVS[i].label, "");
  favRow[3] = uiRow(box, "Otros Cuartos", LV_SYMBOL_RIGHT);

  updateFavColors();
  hueFavHighlight();
}

// ---------- App: Mercados (cripto) ----------
// La portada MUESTRA (§4 regla 1: la portada nunca actua). Refrescar dejo de
// ser un push escondido en la portada y es un renglon del menu.
// Las filas usan uiRow: por eso ya no hay LEFT_MID/RIGHT_MID sueltos aqui —
// alinear a los costados solo es valido DENTRO de una caja acotada y centrada,
// nunca contra el borde de la pantalla, que se curva (§2).
static lv_obj_t *mktState;
static lv_obj_t *mktMenuScr, *mktMenuRow;

static void buildMarketsScreen() {
  lv_obj_t *s = newScreen();
  ovScr[APP_MERCADOS] = s;

  uiTitle(s, "Mercados");
  mktState = uiState(s);

  lv_obj_t *box = uiListBox(s);
  for (int i = 0; i < 3; i++) {
    lv_obj_t *r = uiRow(box, "--", "--");
    mktSym[i] = uiRowLabel(r); mktPrice[i] = uiRowValue(r);
    // El cambio de 24h va bajo el precio: es estado, y su color lo dice.
    mktChg[i] = uiLabel(r, UI_FONT_STATE, COL_TXT, OPA_AVAIL_LABEL);
    lv_label_set_text(mktChg[i], "");
    lv_obj_align(mktChg[i], LV_ALIGN_RIGHT_MID, 0, 11);
    lv_obj_align(mktPrice[i], LV_ALIGN_RIGHT_MID, 0, -6);
  }

  buildDots(s, APP_MERCADOS);
}

// Menu de Mercados: una lista, como todo menu (§4 regla 3).
static void buildMarketsMenu() {
  lv_obj_t *s = newScreen();
  mktMenuScr = s;
  uiTitle(s, "Mercados");
  lv_obj_t *box = uiListBox(s);
  mktMenuRow = uiRow(box, "Refrescar", "");
  uiRowFocus(mktMenuRow, true, false, false);
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

// Dormir la pantalla suelta el trabajo visual caro (el GIF y la rotacion del
// disco). CONDICION QUE NO SE NEGOCIA: la musica NO se detiene — duerme la
// pantalla, no el aparato. Por eso aqui no se toca el player.
void ui_screen_off() {
  wallShow(false);
  musVinylOff();          // suelta los canvas del vinilo (120KB internos)
}
void ui_screen_on() {
  wallShow(curApp == APP_FOTOS);
  // Rehace el vinilo y restaura la vista (incluido el giro si seguia sonando).
  if (curApp == APP_MUSICA && musView != MV_COVER) musVinylOn();
}

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

  wallName = uiLabel(s, UI_FONT_STATE, COL_TXT, OPA_AVAIL_LABEL);
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
    uiRowFocus(pgItems[i], sel, locked, false);
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
    pgItems[i] = uiRow(pgMenuBox, labels[i], "");
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

  pgMenuTitle = mkLabel(s, UI_FONT_TITLE, COL_ACCENT);
  lv_obj_set_style_text_align(pgMenuTitle, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(pgMenuTitle, "Pc Gamer");
  lv_obj_align(pgMenuTitle, LV_ALIGN_TOP_MID, 0, 28);

  pgMenuBox = uiListBox(s);

  // Feedback/estado: debajo del titulo (zona visible del circulo), vacio en reposo.
  pgMenuHint = uiState(s);
}

static void buildGamerScreen() {
  lv_obj_t *s = newScreen();
  ovScr[APP_PC] = s;

  lv_obj_t *t = mkLabel(s, UI_FONT_TITLE, COL_ACCENT);
  lv_label_set_text(t, "Pc Gamer");
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);

  lv_obj_t *ring = lv_obj_create(s);
  lv_obj_set_size(ring, 110, 110);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ring, COL_SURFACE, 0);
  lv_obj_set_style_border_width(ring, 3, 0);
  lv_obj_set_style_border_color(ring, COL_ACCENT, 0);
  lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(ring, LV_ALIGN_CENTER, 0, -24);
  pgIcon = uiLabel(ring, UI_FONT_FIG_XL, COL_TXT, OPA_AVAIL_LABEL);
  lv_label_set_text(pgIcon, LV_SYMBOL_POWER);
  lv_obj_center(pgIcon);

  pgDot = lv_obj_create(s);
  lv_obj_set_size(pgDot, 10, 10);
  lv_obj_set_style_radius(pgDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(pgDot, 0, 0);
  lv_obj_set_style_bg_color(pgDot, COL_TXT, 0);
  lv_obj_set_style_bg_opa(pgDot, OPA_INACTIVE, 0);
  lv_obj_clear_flag(pgDot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(pgDot, LV_ALIGN_CENTER, -56, 56);

  pgStatus = mkLabel(s, UI_FONT_ROW, COL_TXT);
  lv_label_set_text(pgStatus, "Consultando...");
  lv_obj_align(pgStatus, LV_ALIGN_CENTER, 8, 56);

  pgHint = uiLabel(s, UI_FONT_STATE, COL_TXT, OPA_AVAIL_LABEL);
  lv_label_set_long_mode(pgHint, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(pgHint, 250);
  lv_obj_set_style_text_align(pgHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(pgHint, "push: opciones");
  lv_obj_align(pgHint, LV_ALIGN_CENTER, 0, 96);

  buildDots(s, APP_PC);
}

// ---------- App: Musica (VinilOS) ----------
// Girar en la portada = cambiar de app. Push = biblioteca (girar hojea discos,
// push reproduce). Dentro de reproduccion el giro es VOLUMEN y manten regresa.
// La pantalla es el indicador de modo: el giro no significa dos cosas en la
// misma vista (regla central de VinilOS).
static void buildMusicCover() {
  lv_obj_t *s = newScreen();
  ovScr[APP_MUSICA] = s;

  // Disco insinuado: aro exterior + etiqueta al centro
  lv_obj_align(mkCircle(s, 150, 0x101010, LV_OPA_COVER), LV_ALIGN_CENTER, 0, -34);
  lv_obj_align(mkCircle(s, 132, 0x2A2A2A, LV_OPA_COVER), LV_ALIGN_CENTER, 0, -34);
  lv_obj_align(mkCircle(s, 118, 0x101010, LV_OPA_COVER), LV_ALIGN_CENTER, 0, -34);
  lv_obj_align(mkCircle(s, 58,  0xFFB454, LV_OPA_COVER), LV_ALIGN_CENTER, 0, -34);
  lv_obj_align(mkCircle(s, 12,  0x000000, LV_OPA_COVER), LV_ALIGN_CENTER, 0, -34);

  lv_obj_t *wm = mkLabel(s, UI_FONT_TITLE, COL_TXT);
  lv_label_set_text(wm, "VinilOS");
  lv_obj_align(wm, LV_ALIGN_CENTER, 0, 60);

  lv_obj_t *info = uiLabel(s, UI_FONT_STATE, COL_TXT, OPA_AVAIL_LABEL);
  char b[40]; snprintf(b, sizeof(b), "%d discos", (int)ALBUM_COUNT);
  lv_label_set_text(info, b);
  lv_obj_align(info, LV_ALIGN_CENTER, 0, 88);

  lv_obj_t *h = mkLabel(s, UI_FONT_STATE, COL_ACCENT);
  lv_label_set_text(h, "push: entrar");
  lv_obj_align(h, LV_ALIGN_CENTER, 0, 116);

  buildDots(s, APP_MUSICA);
}

// Pantalla del vinilo: la comparten biblioteca y reproduccion. Lo que cambia
// entre las dos es la ESCALA del disco (62% vs 100%) y que capa se ve.
static void buildMusicScreen() {
  lv_obj_t *s = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(s, lv_color_hex(0x000000), 0);   // negro a proposito
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  musScr = s;
  // El vinilo NO se crea aqui: sus canvas piden 120KB de RAM INTERNA (la misma
  // que el TLS de clima/mercados). Se reserva al entrar y se suelta al salir o
  // al dormir — mismo patron que el GIF de Fotos.

  // Biblioteca: nombre arriba (el disco al 62% deja libre esa franja y respeta
  // el safe area; abajo la curva se come el texto).
  musName = mkLabel(s, UI_FONT_TITLE, COL_TXT);
  lv_obj_set_style_text_align(musName, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(musName, "");
  lv_obj_align(musName, LV_ALIGN_TOP_MID, 0, 42);

  musSub = uiLabel(s, UI_FONT_STATE, COL_TXT, OPA_AVAIL_LABEL);
  lv_obj_set_style_text_align(musSub, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(musSub, "");
  lv_obj_align(musSub, LV_ALIGN_TOP_MID, 0, 68);

  // Reproduccion: la pista va en capa FIJA sobre el disco (la etiqueta gira, y
  // un texto girando no se lee). Card translucida para que se lea sobre el arte.
  musCard = lv_obj_create(s);
  lv_obj_remove_style_all(musCard);
  lv_obj_set_size(musCard, 300, 62);
  lv_obj_set_style_bg_color(musCard, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(musCard, 170, 0);
  lv_obj_set_style_radius(musCard, 16, 0);
  lv_obj_align(musCard, LV_ALIGN_CENTER, 0, 96);
  lv_obj_clear_flag(musCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(musCard, LV_OBJ_FLAG_HIDDEN);

  musTrack = mkLabel(musCard, UI_FONT_ROW, COL_TXT);
  lv_label_set_long_mode(musTrack, LV_LABEL_LONG_DOT);
  lv_obj_set_width(musTrack, 280);
  lv_obj_set_style_text_align(musTrack, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(musTrack, "");
  lv_obj_align(musTrack, LV_ALIGN_TOP_MID, 0, 6);

  musArtist = uiLabel(musCard, UI_FONT_STATE, COL_TXT, OPA_AVAIL_LABEL);
  lv_label_set_long_mode(musArtist, LV_LABEL_LONG_DOT);
  lv_obj_set_width(musArtist, 280);
  lv_obj_set_style_text_align(musArtist, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(musArtist, "");
  lv_obj_align(musArtist, LV_ALIGN_TOP_MID, 0, 32);

  musVol = mkLabel(s, UI_FONT_DATA, COL_ACCENT);
  lv_label_set_text(musVol, "");
  lv_obj_align(musVol, LV_ALIGN_TOP_MID, 0, 46);
  lv_obj_add_flag(musVol, LV_OBJ_FLAG_HIDDEN);
}

// Nombre/artista de lo que SUENA (no de lo que hojeas: son distintos en cuanto
// sales a la biblioteca con la musica puesta). El indice de la baraja no sirve
// aqui: los nombres estan indexados por ARCHIVO.
static const char* musPista(bool artista) {
  if (musLoaded < 0 || !musTrackIx) return "";
  const Album &a = ALBUMS[musLoaded];
  uint8_t f = player_track_file();
  if (!f || f > a.tracks) return "";
  const char *const *t = artista ? a.track_artists : a.track_names;
  const char *v = t ? t[f - 1] : nullptr;
  return v ? v : "";
}

static void musRefreshTrack() {
  lv_label_set_text(musTrack,  musPista(false));
  char b[96];
  const char *ar = musPista(true);
  if (musLoaded >= 0 && musTrackIx)
    snprintf(b, sizeof(b), "%s%s%d/%d", ar, ar[0] ? "  -  " : "",
             musTrackIx, ALBUMS[musLoaded].tracks);
  else b[0] = 0;
  lv_label_set_text(musArtist, b);
}

static void musRefreshLib() {
  const Album &a = ALBUMS[musSel];
  lv_label_set_text(musName, a.name);
  char b[64];
  int mins = a.seconds / 60;
  if (a.subtitle && a.subtitle[0])
    snprintf(b, sizeof(b), "%s  -  %d temas  -  %d min", a.subtitle, a.tracks, mins);
  else
    snprintf(b, sizeof(b), "%d temas  -  %d min", a.tracks, mins);
  lv_label_set_text(musSub, b);
  vinyl_set_album(musSel, true);
}

// Reserva perezosa del vinilo. Devuelve la vista a como estaba: al recrear, el
// modulo arranca en cero (zoom 100, sin girar), no a media vuelta.
static void musVinylOn() {
  if (vinyl_alive()) return;
  if (!vinyl_create(musScr)) { Serial.println("[mus] sin memoria para el vinilo"); return; }
  // vinyl_create cuelga sus canvas al final de la pantalla: quedarian ENCIMA de
  // las etiquetas, que se crearon antes. Se suben las capas de texto.
  lv_obj_move_foreground(musName);
  lv_obj_move_foreground(musSub);
  lv_obj_move_foreground(musCard);
  lv_obj_move_foreground(musVol);

  Serial.printf("[mus] vinilo creado  heap=%u\n", (unsigned)ESP.getFreeHeap());
  uint8_t disco = (musView == MV_PLAY && musLoaded >= 0) ? (uint8_t)musLoaded : musSel;
  vinyl_set_album(disco, false);
  if (musView == MV_PLAY) {
    vinyl_zoom_to(100, 1);
    vinyl_cover_mode(false);
    vinyl_set_dots(ALBUMS[disco].tracks, musTrackIx, ALBUMS[disco].color);
    vinyl_set_spinning(!musPaused);
  } else {
    vinyl_zoom_to(62, 1);
    vinyl_cover_mode(true);
    vinyl_set_spinning(false);
  }
}

static void musVinylOff() {
  if (!vinyl_alive()) return;
  vinyl_destroy();
  Serial.printf("[mus] vinilo liberado heap=%u\n", (unsigned)ESP.getFreeHeap());
}

// Biblioteca: la camara se aleja y el disco muestra la caratula completa.
static void musGoLib() {
  musView = MV_LIB;
  vinyl_zoom_to(62, 420);
  vinyl_cover_mode(true);
  vinyl_set_spinning(false);      // frena con inercia; la musica NO se detiene
  lv_obj_add_flag(musCard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(musVol,  LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(musName, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(musSub,  LV_OBJ_FLAG_HIDDEN);
  musRefreshLib();
}

// Reproduccion: la camara se acerca, el disco llena el cuadro y gira.
static void musGoPlay(bool arrancar) {
  musView = MV_PLAY;
  vinyl_zoom_to(100, 420);
  vinyl_cover_mode(false);
  vinyl_set_album(musSel, false);
  vinyl_set_spinning(true);
  lv_obj_add_flag(musName, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(musSub,  LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(musCard, LV_OBJ_FLAG_HIDDEN);

  if (arrancar || musLoaded != (int8_t)musSel || !musTrackIx) {
    musLoaded = (int8_t)musSel;
    musTrackIx = 1;
    musPaused = false;
    if (player_available()) player_play_album(ALBUMS[musSel].folder, ALBUMS[musSel].tracks);
  } else if (musPaused) {
    musPaused = false;
    if (player_available()) player_resume();
  }
  vinyl_set_dots(ALBUMS[musSel].tracks, musTrackIx, ALBUMS[musSel].color);
  musRefreshTrack();
}

void ui_build() {
  buildHueCover();
  buildHueFavs();
  buildHueMenuScreen();
  buildGamerMenu();
  buildMarketsScreen();
  buildMarketsMenu();
  buildMusicCover();
  buildMusicScreen();
  buildWallpaperScreen();
  buildGamerScreen();
  lv_scr_load(ovScr[APP_LUCES]);
}

// 🔄 Girar: dentro de Hue/PC = mover en la lista; si no, cambiar de app
void ui_nav(int dir) {
  if (curApp == APP_PC && pgView == PG_MENU) {    // navegando el menu de PC Gamer
    if (pgItemCount <= 0) return;
    // Un renglon inactivo se ve (al 60) pero EL GIRO PASA DE LARGO (§5.3):
    // no se esconde, porque la lista cambiaria de largo y perderias referencia.
    bool ready = pcReady();
    int n = pgSel;
    for (int step = 0; step < pgItemCount; step++) {
      n += dir;
      if (n < 0) { n = 0; break; }
      if (n >= pgItemCount) { n = pgItemCount - 1; break; }
      bool isMode = (pgActions[n] == PA_NORMAL || pgActions[n] == PA_SIM || pgActions[n] == PA_TV);
      if (!(isMode && !ready)) break;              // llegamos a uno usable
    }
    pgSel = n;
    pgConfirming = false;                          // moverse cancela la confirmacion
    lv_label_set_text(pgMenuHint, "");
    pgApplyHighlight();
    return;
  }
  if (curApp == APP_MUSICA && musView != MV_COVER) {
    if (musView == MV_LIB) {                      // biblioteca: hojear discos
      int n = (int)musSel + dir;                  // circular, como en VinilOS
      if (n < 0) n = ALBUM_COUNT - 1;
      if (n >= (int)ALBUM_COUNT) n = 0;
      musSel = (uint8_t)n;
      musRefreshLib();
    } else {                                      // reproduciendo: VOLUMEN
      int v = (int)player_volume() + dir;
      if (v < 0) v = 0;
      if (v > PLAYER_VOL_MAX) v = PLAYER_VOL_MAX;
      player_set_volume((uint8_t)v);              // la cola colapsa las rafagas
      char b[16]; snprintf(b, sizeof(b), "Vol %d", v);
      lv_label_set_text(musVol, b);
      lv_obj_clear_flag(musVol, LV_OBJ_FLAG_HIDDEN);
      musVolUntil = millis() + 1200;
    }
    return;
  }
  if (curApp == APP_LUCES && hueView != HV_NONE) { // navegando listas de Hue
    if (hueView == HV_FAVS) {                     // ahora es lista navegable
      hueSel += dir;
      if (hueSel < 0) hueSel = FAV_ROWS - 1;
      if (hueSel >= FAV_ROWS) hueSel = 0;
      hueFavHighlight();
      return;
    }
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
  if (curApp == APP_MERCADOS) {
    if (!mktInMenu) {                             // portada -> entra al menu
      mktInMenu = true;
      lv_scr_load_anim(mktMenuScr, LV_SCR_LOAD_ANIM_OVER_LEFT, UI_MS_SCREEN, 0, false);
    } else {                                      // menu -> la accion
      markets_request();
      lv_label_set_text(uiRowValue(mktMenuRow), "Pedido");
    }
    return;
  }
  // --- App Musica (VinilOS): portada -> biblioteca -> reproducir/pausa ---
  if (curApp == APP_MUSICA) {
    if (musView == MV_COVER) {                    // portada -> biblioteca
      musView = MV_LIB;                           // musVinylOn lee la vista
      musVinylOn();
      musGoLib();
      lv_scr_load_anim(musScr, LV_SCR_LOAD_ANIM_OVER_LEFT, 250, 0, false);
    } else if (musView == MV_LIB) {               // disco -> reproducir
      musGoPlay(true);
    } else {                                      // reproduciendo: pausa/reanuda
      vinyl_bump();                               // feedback inmediato al apretar
      if (musPaused) {
        musPaused = false;
        if (player_available()) player_resume();
        vinyl_set_spinning(true);
      } else {
        musPaused = true;
        if (player_available()) player_pause();
        vinyl_set_spinning(false);                // frena derecho, no torcido
      }
    }
    return;
  }
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
        lv_obj_set_style_text_color(pgItems[pgSel], COL_WARN, 0);
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
    } else if (hueView == HV_FAVS) {               // escena, o entrar a cuartos
      if (hueSel < 3) { favActivate(hueSel); return; }
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
  if (curApp == APP_MERCADOS && mktInMenu) {
    mktInMenu = false;
    lv_label_set_text(uiRowValue(mktMenuRow), "");
    lv_scr_load_anim(ovScr[APP_MERCADOS], LV_SCR_LOAD_ANIM_OVER_RIGHT, UI_MS_SCREEN, 0, false);
    return;
  }
  if (curApp == APP_PC && pgView == PG_MENU) {     // menu -> volver al cover
    pgView = PG_COVER; pgConfirming = false;
    lv_label_set_text(pgHint, "push: opciones");
    lv_scr_load_anim(ovScr[APP_PC], LV_SCR_LOAD_ANIM_OVER_RIGHT, 250, 0, false);
    return;
  }
  if (curApp == APP_MUSICA && musView != MV_COVER) {
    if (musView == MV_PLAY) {                     // reproduccion -> biblioteca
      musGoLib();                                 // la musica SIGUE sonando
    } else {                                      // biblioteca -> portada
      musView = MV_COVER;
      musVinylOff();                              // devuelve la RAM interna
      lv_scr_load_anim(ovScr[APP_MUSICA], LV_SCR_LOAD_ANIM_OVER_RIGHT, 250, 0, false);
    }
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

  // --- Musica (VinilOS) ---
  // vinyl_tick mueve la rotacion: solo mientras se ve el disco (es lo mas caro
  // de dibujar del firmware). Va ANTES del throttle de 500ms para que gire fluido.
  if (curApp == APP_MUSICA && musView != MV_COVER) {
    vinyl_tick();
    if (musVolUntil && millis() > musVolUntil) {
      musVolUntil = 0;
      lv_obj_add_flag(musVol, LV_OBJ_FLAG_HIDDEN);
    }
    // El DFPlayer avisa solo cuando cambia de pista; la UI lo refleja.
    if (player_available() && player_track_index() && player_track_index() != musTrackIx) {
      musTrackIx = player_track_index();
      if (musLoaded >= 0)
        vinyl_set_dots(ALBUMS[musLoaded].tracks, musTrackIx, ALBUMS[musLoaded].color);
      musRefreshTrack();
    }
    // Fin del album: por ahora repite el mismo disco (Ajustes llega en etapa 5).
    if (player_available() && player_album_fin() && musLoaded >= 0) {
      musTrackIx = 1;
      player_play_album(ALBUMS[musLoaded].folder, ALBUMS[musLoaded].tracks);
      musRefreshTrack();
    }
  }

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
    lv_obj_set_style_bg_color(pgDot, pcOnline ? COL_OK : COL_TXT, 0);
    lv_obj_set_style_bg_opa(pgDot, pcOnline ? LV_OPA_COVER : OPA_INACTIVE, 0);
    lv_label_set_text(pgStatus, pcOnline ? "Encendida" : "Apagada");
    lv_obj_set_style_text_color(pgIcon, pcOnline ? COL_OK : COL_TXT, 0);
    lv_obj_set_style_text_opa(pgIcon, pcOnline ? LV_OPA_COVER : OPA_INACTIVE, 0);
  } else {
    lv_label_set_text(pgStatus, "Consultando...");
  }

  // En el menu: confirmacion del perfil (item verde si llego, rojo si no)
  if (pgView == PG_MENU && pgProfileItem >= 0 && pgProfileItem < pgItemCount) {
    if (g_pcProfileResult == 2) {                       // POST respondio OK
      lv_obj_set_style_text_color(uiRowLabel(pgItems[pgProfileItem]), COL_OK, 0);
      lv_label_set_text(pgMenuHint, LV_SYMBOL_OK " Listo");
      lv_obj_set_style_text_color(pgMenuHint, COL_OK, 0);
      pgProfileItem = -1; pgInfoUntil = millis() + 1800;
    } else if (g_pcProfileResult == -1) {               // no respondio
      lv_obj_set_style_text_color(uiRowLabel(pgItems[pgProfileItem]), COL_BAD, 0);
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
      lv_obj_set_style_text_color(pgMenuHint, COL_TXT, 0);
      lv_obj_set_style_text_opa(pgMenuHint, OPA_STATE, 0);
      pgApplyHighlight();                   // restaura el color normal de los items
    }
    else lv_label_set_text(pgHint, "push: opciones");
  }
}
