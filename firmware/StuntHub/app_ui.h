#pragma once

void ui_build();        // crea las pantallas y widgets
void ui_tick();         // refresca desde g_state (llamar en loop)
void ui_nav(int dir);   // -1 / +1 : cambia de pantalla (perilla)
void ui_select();       // pulsación corta de la perilla
void ui_back();         // pulsación larga (atrás / subir nivel)
void ui_screen_off();   // pantalla durmió: suelta trabajo visual (GIF)
void ui_screen_on();    // pantalla despertó: lo retoma si aplica
