#include "weather.h"
#include "netclock.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#define REINTENTO_MS   (5UL  * 60UL * 1000UL)   // fallo: se vuelve a intentar pronto
#define REFRESCO_MS    (30UL * 60UL * 1000UL)   // exito: el clima no cambia por minuto

static volatile bool  hay   = false;
static volatile int   grados = 0;
static volatile int   codigo = -1;

// Codigos WMO, que es lo que devuelve Open-Meteo. Los agrupo: la diferencia
// entre llovizna ligera y llovizna moderada no cabe en la pantalla y a nadie
// le cambia el dia. Todo en ASCII porque Montserrat no trae acentos.
static const char* texto_wmo(int c) {
  switch (c) {
    case 0:  return "Despejado";
    case 1:  return "Casi Despejado";
    case 2:  return "Parcial Nublado";
    case 3:  return "Nublado";
    case 45: case 48: return "Niebla";
    case 51: case 53: case 55: return "Llovizna";
    case 56: case 57: return "Llovizna Helada";
    case 61: case 63: case 65: return "Lluvia";
    case 66: case 67: return "Lluvia Helada";
    case 71: case 73: case 75: case 77: return "Nieve";
    case 80: case 81: case 82: return "Chubascos";
    case 85: case 86: return "Nieve";
    case 95: return "Tormenta";
    case 96: case 99: return "Tormenta y Granizo";
    default: return "";
  }
}

// Sin libreria de JSON: la respuesta es de forma fija y conocida, y arrastrar
// ArduinoJson solo para leer dos numeros seria pagar de mas.
//
// El "desde" NO es un detalle. La respuesta trae los mismos nombres dos veces:
//
//     "current_units": { "temperature_2m": "\u00B0C", "weather_code": "wmo code" }
//     "current":       { "temperature_2m": 31.5,  "weather_code": 0 }
//
// y el bloque de unidades va primero. Buscar a secas encuentra el texto "C",
// que convertido a numero da cero — un cero perfectamente creible en invierno
// y absurdo en agosto. Por eso se empieza a buscar despues de "current":.
static bool campo(const String& s, const char* clave, float* out, int desde) {
  int i = s.indexOf(clave, desde);
  if (i < 0) return false;
  i = s.indexOf(':', i + strlen(clave));
  if (i < 0) return false;
  // Si lo que sigue es una comilla, caimos en el bloque de unidades.
  int j = i + 1;
  while (j < (int)s.length() && s[j] == ' ') j++;
  if (j < (int)s.length() && s[j] == '"') return false;
  *out = s.substring(i + 1).toFloat();
  return true;
}

static bool consultar() {
  WiFiClientSecure cli;
  // Sin verificar certificado: es un dato publico de solo lectura y no hay
  // nada que proteger. Guardar y renovar una CA en un reproductor de musica
  // seria mantenimiento sin beneficio.
  cli.setInsecure();
  cli.setTimeout(8);

  HTTPClient http;
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=")
             + CLIMA_LAT + "&longitude=" + CLIMA_LON
             + "&current=temperature_2m,weather_code";
  if (!http.begin(cli, url)) return false;
  http.setTimeout(8000);

  bool bien = false;
  if (http.GET() == HTTP_CODE_OK) {
    String body = http.getString();
    float t, c;
    // "current_units" no coincide con "current": por la llave de dos puntos.
    int base = body.indexOf("\"current\":");
    if (base < 0) base = 0;
    if (campo(body, "\"temperature_2m\"", &t, base) &&
        campo(body, "\"weather_code\"", &c, base)) {
      grados = (int)lroundf(t);
      codigo = (int)c;
      hay    = (texto_wmo(codigo)[0] != '\0');
      bien   = hay;
      Serial.printf("clima: %d C, codigo %d (%s)\n", grados, codigo,
                    texto_wmo(codigo));
    }
  }
  http.end();
  if (!bien) Serial.println("clima: sin respuesta util");
  return bien;
}

static void tareaClima(void*) {
  for (;;) {
    // Nada que intentar sin red. Se revisa seguido porque el WiFi puede tardar
    // en asociar, pero cada revision cuesta nada.
    if (!clock_wifi()) { vTaskDelay(pdMS_TO_TICKS(5000)); continue; }
    bool bien = consultar();
    vTaskDelay(pdMS_TO_TICKS(bien ? REFRESCO_MS : REINTENTO_MS));
  }
}

void weather_begin() {
  xTaskCreatePinnedToCore(tareaClima, "clima", 8192, nullptr, 1, nullptr, 0);
}

bool        weather_ok()    { return hay; }
int         weather_temp()  { return grados; }
const char* weather_texto() { return hay ? texto_wmo(codigo) : ""; }
