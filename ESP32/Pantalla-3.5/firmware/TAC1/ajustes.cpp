#include "ajustes.h"
#include <Preferences.h>

static Ajustes aj = { BRILLO_AUTO };

void ajustes_begin() {
  Preferences p;
  p.begin("aj", true);
  aj.brillo = p.getUChar("brillo", BRILLO_AUTO);
  p.end();
}

void ajustes_save() {
  Preferences p;
  p.begin("aj", false);
  p.putUChar("brillo", aj.brillo);
  p.end();
}

Ajustes& ajustes() { return aj; }
