#include "alarm.h"
#include <Preferences.h>

static Preferences prefs;
static AlarmCfg cfg = { false, 7, 30, DIAS_LAV, 0 };
static int16_t ultimoDisparo = -1;   // minuto del dia ya disparado

void alarm_begin() {
  prefs.begin("vnl1", false);
  cfg.activa = prefs.getBool("al_on", false);
  cfg.hora   = prefs.getUChar("al_h", 7);
  cfg.minuto = prefs.getUChar("al_m", 30);
  cfg.dias   = prefs.getUChar("al_d", DIAS_LAV);
  cfg.album  = prefs.getUChar("al_a", 0);
  prefs.end();
}

void alarm_save() {
  prefs.begin("vnl1", false);
  prefs.putBool("al_on", cfg.activa);
  prefs.putUChar("al_h", cfg.hora);
  prefs.putUChar("al_m", cfg.minuto);
  prefs.putUChar("al_d", cfg.dias);
  prefs.putUChar("al_a", cfg.album);
  prefs.end();
}

AlarmCfg& alarm_cfg() { return cfg; }

bool alarm_debe_sonar(const struct tm& t) {
  if (!cfg.activa) return false;
  if (!(cfg.dias & (1 << t.tm_wday))) return false;

  int16_t minutoDelDia = t.tm_hour * 60 + t.tm_min;
  if (t.tm_hour != cfg.hora || t.tm_min != cfg.minuto) {
    // Fuera de la hora: se limpia la marca para que manana vuelva a poder.
    if (ultimoDisparo != minutoDelDia) ultimoDisparo = -1;
    return false;
  }
  if (ultimoDisparo == minutoDelDia) return false;   // ya sono en este minuto
  ultimoDisparo = minutoDelDia;
  return true;
}

const char* alarm_texto_dias(uint8_t dias) {
  switch (dias) {
    case DIAS_TODOS: return "Todos los Dias";
    case DIAS_LAV:   return "Lunes a Viernes";
    case DIAS_FIN:   return "Fin de Semana";
    default:         return "Algunos Dias";
  }
}
