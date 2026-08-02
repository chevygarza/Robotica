// VNL-1 — La alarma. Un disco mas al final de la biblioteca.
//
// Se guarda en NVS, asi que sobrevive al desconectar el aparato. Lo unico que
// no sobrevive es la hora: eso lo resuelve netclock con NTP.
#pragma once
#include <Arduino.h>
#include <time.h>

// Mascara de dias, bit 0 = domingo (igual que tm_wday).
#define DIAS_TODOS   0x7F
#define DIAS_LAV     0x3E    // lunes a viernes
#define DIAS_FIN     0x41    // sabado y domingo

struct AlarmCfg {
  bool    activa;
  uint8_t hora;      // 0..23
  uint8_t minuto;    // 0..59
  uint8_t dias;      // mascara
  uint8_t album;     // indice en ALBUMS[]
};

void      alarm_begin();
void      alarm_save();
AlarmCfg& alarm_cfg();

// true exactamente una vez cuando el minuto coincide y el dia aplica.
bool alarm_debe_sonar(const struct tm& t);

// Texto de los dias para la UI: "todos", "L a V", "fin de semana", "algunos".
const char* alarm_texto_dias(uint8_t dias);
