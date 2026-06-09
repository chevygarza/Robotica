# Plan — CrowPanel 1.46" "Hub All-in-One" (clima + news + sensores + IR)

## ROADMAP DEL BUNKER (decidido 2026-06-07)
Modelo: cada pieza hace lo suyo, no una placa lo hace todo.
- CrowPanel = panel bonito: dashboard + A/C IR + sensores.
- Mac Mini (24/7) = cerebro: LLM/Claude, voz, automatizaciones (puente).
- Voz = dispositivo dedicado futuro (Atom Echo/S3-Box) que le habla a la Mac.

Fases:
- FASE 1 ✅ HECHA: dashboard (reloj MTY + clima + X stats), nav perilla+detalle.
- FASE 2 🔜 (al llegar hardware jueves): A/C Mirage por IR (UART GPIO43/44) + BME280 (I2C GPIO38/39). Guiar cableado en protoboard paso a paso.
- FASE 3 🔭 futuro sin prisa: voz + Claude vía Mac Mini (whisper.cpp local + Claude API + Piper/say local, ~$1-3/mes), en placa con audio (NO la CrowPanel por pines). 
- BONUS opcional cualquier momento (sin hardware): "Claude en pantalla" = CrowPanel pregunta a Mac/Claude y muestra texto.
NO comprar nada nuevo para voz aún. Prioridad = Fase 2.

## Objetivo
Hub de escritorio con UI LVGL hermosa (perilla + táctil):
1. Internet: clima de Monterrey + noticias (RSS/API, solo software).
2. Sensores reales a bordo: temp + humedad (BME280 I2C).
3. IR de alto alcance: emisor potente (A/C Mirage + todo lo IR) + receptor IR (aprender controles).
4. Reloj NTP.

## Mapa de pines (esquemático V1.0 + código de fábrica)
| Función | GPIO |
|---|---|
| LCD (ST77961, SPI, LovyanGFX) | SCLK=10, MOSI=11, CS=9, DC=3, RST=14, BL=46 |
| Táctil (CST816T, I2C "Wire") | SDA=38, SCL=39, INT=5, RST=13 |
| Encoder/perilla | A=45, B=42, botón=41 |
| LED ambiental (NeoPixel x8) | DIN=48 ; power-LED=40 |
| **IR emisor (conector UART)** | **GPIO43 (U0TXD)** vía transistor |
| **IR receptor (conector UART)** | **GPIO44 (U0RXD)** |
| **Sensor BME280 (conector I2C)** | comparte bus GPIO38/39 (addr 0x76/0x77; touch 0x15, no chocan) |

## Hardware (BOM) — todo en protoboard, SIN soldar
YA TIENE (kit ELEGOO Most Complete Mega2560): transistor PN2222 & S8050 (x10), capacitor 100µF, resistencias (10R,100R,220R,330R,1K,2K,5K1,10K,100K,1M x10 c/u), MÓDULO RECEPTOR IR (VS1838B), 2x control remoto IR (kit + carrito), protoboard + jumpers, DHT11, y bonus: PIR HC-SR501, ultrasónico, RTC, GY-521.
COMPRAR (solo 3): 
- LEDs IR 940nm 5mm x3-5 (lo único de IR que NO trae el kit; trae receptor+control pero no emisor)
- Sensor BME280 (I2C, 3.3V) — preferido sobre el DHT11 del kit porque DHT11 es 1-wire y pide GPIO dedicado escaso; BME280 va en el bus I2C sin conflicto
- 1 cable EXTRA 1.25mm 4-pin (PicoBlade-compat); la caja del CrowPanel trae 1, se necesita otro para usar UART+I2C a la vez
Resistencia IR: usar 10Ω (del kit) por LED, 2-3 LEDs (PN2222 aguanta el pulso).
NO comprar KY-005.

## Circuito IR (alta potencia)
- GPIO43 -> 220Ω -> base S8050; emisor -> GND.
- 3V3 -> 22Ω -> ánodo cada LED IR; cátodos -> colector S8050. (LEDs a ~120° para cobertura amplia.)
- VS1838B: OUT->GPIO44, VCC->3V3, GND->GND. Cap 100µF entre 3V3 y GND.
- (Opcional más alcance: alimentar riel de LEDs desde 5V del conector USB.)

## Pantallas / iconos (navegar con perilla)
1. Reloj NTP (hora Monterrey).
2. Clima Monterrey (Open-Meteo, sin API key): temp, condición, máx/mín, humedad.
3. Control A/C Mirage (perilla = °C, push = on/off, táctil = modo/fan) por IR.
4. Noticias (ticker, RSS/API).
5. 𝕏 @stuntech: últimos posts + métricas públicas (likes/reposts/respuestas).
6. Sensores a bordo (BME280: temp/humedad/presión interior).

## Integración X (decidido: API oficial pago-por-uso)
- 2026: X sin tier gratis. Pago por uso ~$0.005/lectura ($0.001 "owned reads"). ~$1-2 USD/mes a bajo refresco.
- Endpoints v2 (app-only Bearer):
  - `GET https://api.x.com/2/users/by/username/stuntech` -> user id
  - `GET https://api.x.com/2/users/{id}/tweets?max_results=5&tweet.fields=public_metrics,created_at&exclude=retweets,replies`
  - Header: `Authorization: Bearer <BEARER_TOKEN>`
- ESP32: WiFiClientSecure (setInsecure() para empezar, o bundlear CA de x.com) + ArduinoJson. Token en config local.
- Métricas públicas (likes/reposts/replies/quotes) ✅. Impresiones (privadas) = requieren OAuth de dueño, fase 2.
- SETUP que hará Jose: developer.x.com -> crear Project+App -> copiar Bearer Token -> registrar tarjeta (pago por uso) -> pegar token en config.

## Software (Arduino + LVGL, autónomo, sin Home Assistant)
- WiFi + NTP (zona Monterrey, CST6).
- HTTPClient + ArduinoJson -> Open-Meteo (clima, sin API key).
- Noticias: RSS/API por WiFi (ticker en pantalla).
- BME280 (Adafruit_BME280 o similar) en Wire (GPIO38/39).
- IRremoteESP8266: IRMirageAc en GPIO43 (emisor) + IRrecv en GPIO44 (aprender).
- LovyanGFX (ST77961) + lvgl 8.3 + cst816t + encoder como lv_indev.

## Notas
- Self-heating: poner BME280 a unos cm en cable, o aplicar offset por software.
- ROM puede emitir un blip en GPIO43 al boot (inofensivo).

## Estado entorno (LISTO Y PROBADO, sin tocar la placa)
- arduino-cli 1.5.1 + core esp32:esp32@2.0.17. Libs en ~/Documents/Arduino/libraries.
- FQBN: esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc
- Partición 14MB: tools/partitions/crowpanel_14M.csv + `--build-property build.partitions=crowpanel_14M --build-property upload.maximum_size=14680064`
- Demo de fábrica compila en verde. IR Mirage compila en verde. Falta: instalar Adafruit_BME280; escribir el sketch del hub.
- Respaldo de fábrica íntegro en backup/factory_full_16MB.bin. NO se ha flasheado nada.
