# Apps nuevas para StuntHub — Plan de implementación

## ✅ App 1 — Mercados (cripto) — HECHA (pre-construida)
- Archivos: `app_markets.cpp/.h` + integrada en UI como app 4 (5ª).
- Datos: CoinGecko API (gratis, sin key). Valida con ISRG Root X1 (ya lo tenemos).
- Sigue BTC/ETH/SOL (configurable en COIN_IDS/COIN_SYMS). Refresca cada 5 min; push = refrescar.
- UI: 3 tarjetas (símbolo · precio · %24h verde/rojo).
- Pendiente: flashear + probar. Fácil agregar más monedas o acciones (otra API).

## 📋 App 2 — Mac Mini (server health) — PLAN
Jose ya tiene un cron que manda reporte a Telegram diario 5pm (uptime, load, procs, red, thermal, SSD, RAM, SMART, top CPU/RAM, estado).

**Arquitectura (todo LAN, sin nube, sin TLS):**
1. En el Mac Mini: un script (puede ser el mismo del cron) que escribe `health.json` con los stats, y lo sirve por HTTP en la red local. Correrlo cada ~1-5 min (no solo 5pm) para datos frescos.
   - Servir: `python3 -m http.server 8080` en la carpeta del json, o un mini Flask/Node, o meterlo en un server web existente.
2. ESP32 hace GET `http://<macmini-ip>:8080/health.json` cada ~60s (LAN, rápido, gratis, HTTP simple sin certificados).
3. Parse con ArduinoJson + UI.

**Schema JSON que debe generar el Mac Mini:**
```json
{
  "status": "healthy",            // healthy|warn|critical
  "uptime": "12:50",
  "load": [1.17, 1.11, 1.11],
  "procs": 700,
  "net": {"up": true, "down_mbps": 415, "up_mbps": 372, "ping_ms": 49},
  "thermal": "normal",
  "ssd": {"total_gi": 460, "used_gi": 12, "free_gi": 169, "pct": 7, "smart": "verified"},
  "ram": {"total_gb": 16, "used_gb": 7.8, "free_gb": 8.2, "pct": 49},
  "top_cpu": [{"name":"trustd","cpu":6.1}],
  "top_ram": [{"name":"Google","ram":5.9}]
}
```

**UI app "Servidor":** punto 🟢/🟡/🔴 + "Mac Mini"; uptime; load; barra RAM (49%); barra SSD (7%); red ↓415 ↑372 ping 49; thermal. Push = detalle con top procesos.
**Falta de Jose:** IP del Mac Mini en LAN + adaptar el script del cron a JSON + servirlo (yo le ayudo con el script).

## 🎮 App 3 — Wallpapers animados (Zelda / Dragon Ball / Pokemon) — PLAN
**Enfoque técnico:**
- Usar el decodificador GIF de LVGL: `LV_USE_GIF 1` en lv_conf.h (módulo gifdec).
- GIFs PEQUEÑOS y optimizados (pixel-art se ve increíble en pantalla redonda): ~150-220px, pocos colores, loop corto. Full 360x360 es muy pesado.
- Almacenamiento: cada GIF como arreglo C embebido en flash (convertir con el image converter de LVGL), o en partición LittleFS subida aparte. Probable necesitar partición de app grande (ya tenemos crowpanel_14M.csv) o LittleFS para assets.
- Decodificación usa PSRAM (8MB) — ok para tamaños modestos.
- UI: app "Wallpaper" donde girar la perilla cicla entre los 3 GIFs (objetos lv_gif centrados).

**Pipeline de assets (lo hago yo en la Mac cuando Jose pase los GIFs):**
1. Jose pasa 3 GIFs que le gusten (o busco pixel-art fan-art).
2. Optimizar con gifsicle/ImageMagick: bajar tamaño, colores, frames (cada uno < ~300KB).
3. Convertir a formato LVGL (C array o LittleFS).
4. Habilitar LV_USE_GIF + app "Wallpaper" con la perilla ciclando los 3.

**Nota copyright:** Zelda/DBZ/Pokemon son marcas; para uso personal en tu propio device está bien. Uso pixel-art fan-art (no assets oficiales). Jose puede pasar los GIFs específicos que quiera.
