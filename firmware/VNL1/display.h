// VNL-1 — Pantalla: LovyanGFX (panel ST77961) + LVGL + backlight con PWM.
// Todo lo que toca el panel vive aqui; el resto del firmware solo habla LVGL.
#pragma once
#include <lvgl.h>

// Inicializa panel, DMA, LVGL y el tactil. Deja el backlight en 0: usa
// display_backlight_fade() despues de armar la primera pantalla para que
// arranque con un fade y no con un golpe de luz.
bool display_begin();

// En cada pasada del loop: corre el fade pendiente del backlight.
void display_tick();

void    display_backlight(uint8_t pct);                    // inmediato
void    display_backlight_fade(uint8_t pct, uint16_t ms);  // no bloqueante
uint8_t display_backlight_level();
bool    display_backlight_busy();

// El tactil aqui solo sirve para despertar la pantalla: devuelve true (y se
// limpia) si hubo un toque desde la ultima consulta.
bool display_touched();
