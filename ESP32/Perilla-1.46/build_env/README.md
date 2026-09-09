# Build environment reproducible

Versiones exactas que producen un binario funcional para la CrowPanel 1.46":

| Item | Versión |
|---|---|
| arduino-cli | 1.5.1 |
| esp32:esp32 core | **2.0.17** |
| LovyanGFX | **1.2.7** ⚠️ (NO 1.2.21 — ver nota) |
| lvgl | 8.3.6 |
| cst816t | 1.5.1 |
| ArduinoJson | 7.4.2 |
| IRremoteESP8266 | 2.8.6 (solo StuntHub, no usado pero presente) |

## Setup limpio
```bash
arduino-cli config init
arduino-cli core update-index
arduino-cli core install esp32:esp32@2.0.17
arduino-cli lib install "LovyanGFX@1.2.7" "lvgl@8.3.6" "cst816t@1.5.1" "ArduinoJson@7.4.2"
# Copiar el lv_conf.h custom a ~/Documents/Arduino/libraries/
cp tools/build_env/lv_conf.h ~/Documents/Arduino/libraries/lv_conf.h
```

## Nota crítica: LovyanGFX 1.2.7 vs 1.2.21
En 1.2.7 el panel ST77961 inicializa correctamente con la config de `board.h`
(LGFX). Versiones posteriores cambiaron la secuencia de init y la pantalla
queda en blanco con backlight encendido (firmware sí corre — WiFi/agente OK,
solo el display está negro). **Pinear a 1.2.7**.

`shasum Panel_ST77961.hpp` en 1.2.7 = `bed2b6a3f248e0f41d7c7876c34afc447fd693d9`.

## FQBN único válido
```
esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc
```

## Cosas a ignorar
- `~/Documents/Arduino/libraries/UI/` (ui_Screen0.c etc): es del repo de Elecrow.
  Ningún firmware del proyecto lo usa. Puede coexistir.
- `crowpanel_14M.csv` en `~/Library/Arduino15/packages/esp32/hardware/esp32/2.0.17/tools/partitions/`:
  fue un partition file custom de las primeras pruebas. Hoy usamos `huge_app`
  estándar (3MB app / 9MB SPIFFS) — sobra de espacio (StuntHub v2 = 70%,
  TMEhub = 39%). Puede borrarse, no hace falta.
