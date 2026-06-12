#pragma once

void ui_build();
void ui_tick();         // llamar en loop
void ui_push();         // push corto de la perilla
void ui_long();         // push largo
bool ui_can_sleep();    // true solo en reposo (no dormir durante un reseteo)
