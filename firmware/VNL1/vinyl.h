// VNL-1 — El vinilo. Surcos concentricos estaticos + etiqueta central que rota.
#pragma once
#include <lvgl.h>

// Geometria, para que los overlays sepan donde termina el disco.
#define VINYL_DISC_D    336
// Subida 30% (era 132): a 132 el nombre del album quedaba en el limite de lo
// legible a un brazo de distancia, que es como se mira un objeto de escritorio.
#define VINYL_LABEL_D   172

bool vinyl_create(lv_obj_t* parent);

// Encoge y regresa la etiqueta en 180ms. Es el feedback inmediato al apretar,
// lo que tapa la ventana de 260ms del doble push.
void vinyl_bump();

// true mientras el disco todavia tiene inercia, aunque ya se pidio parar.
bool vinyl_moving();

// animate = cross-fade de la etiqueta al album nuevo.
void vinyl_set_album(uint8_t idx, bool animate);

// dir: -1 antihorario, +1 horario. Desplaza el highlight hacia ese lado.
void vinyl_nudge_sheen(int8_t dir);

void vinyl_set_spinning(bool on);
bool vinyl_spinning();

// Avanza rotacion y suavizado del highlight. Llamar en cada pasada del loop.
void vinyl_tick();
