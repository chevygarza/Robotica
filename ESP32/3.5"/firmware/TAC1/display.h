// TAC-1 — Pantalla: AXS15231B por QSPI con DMA + LVGL + tactil + backlight.
// Todo lo que toca el panel vive aqui; el resto del firmware solo habla LVGL.
#pragma once
#include <lvgl.h>

// Inicializa bus, panel, LVGL y el tactil. Deja el backlight en 0: usa
// display_backlight_fade() despues de armar la primera pantalla para que
// arranque con un fade y no con un golpe de luz.
bool display_begin();

// En cada pasada del loop: corre el fade pendiente del backlight.
void display_tick();

void    display_backlight(uint8_t pct);                    // inmediato
void    display_backlight_fade(uint8_t pct, uint16_t ms);  // no bloqueante
uint8_t display_backlight_level();

// Reposo. Dormida, la pantalla NO reporta toques a LVGL: el primer toque solo
// despierta (display_woke() lo entrega una vez), no actua.
void display_sleep(bool asleep);
bool display_woke();

// Apagar de verdad: el panel a dormir y la luz a cero. display_on() lo trae.
void display_off();
void display_on();

// Diagnostico: cuadros volcados al panel desde la ultima consulta.
uint32_t display_frames();
