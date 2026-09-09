// VNL-1 — El vinilo. Surcos concentricos estaticos + etiqueta central que rota.
#pragma once
#include <lvgl.h>

// Geometria, para que los overlays sepan donde termina el disco.
#define VINYL_DISC_D    336
// Subida 30% (era 132): a 132 el nombre del album quedaba en el limite de lo
// legible a un brazo de distancia, que es como se mira un objeto de escritorio.
#define VINYL_LABEL_D   200

bool vinyl_create(lv_obj_t* parent);

// Encoge y regresa la etiqueta en 180ms. Es el feedback inmediato al apretar,
// lo que tapa la ventana de 260ms del doble push.
void vinyl_bump();

// Aleja o acerca el vinilo. 100 = llena el cuadro (reproduciendo),
// ~62 = se encoge y deja ver los discos vecinos (biblioteca).
void    vinyl_zoom_to(uint8_t pct, uint16_t ms);

// true = la caratula llena el disco (biblioteca). false = vinilo con etiqueta
// al centro, que es el unico que puede girar sin hundir los fps.
void    vinyl_cover_mode(bool on);

// Puntos de pista pintados DENTRO del disco. Como objetos sueltos parpadeaban:
// son dieciseis piezas chicas y brillantes que LVGL redibuja sobre el disco, y
// sin sincronia vertical cada redibujo se ve como un destello.
void    vinyl_set_dots(uint8_t total, uint8_t actual, uint32_t color);
uint8_t vinyl_zoom();

// true mientras el disco todavia tiene inercia, aunque ya se pidio parar.
bool vinyl_moving();

// animate = cross-fade de la etiqueta al album nuevo.
void vinyl_set_album(uint8_t idx, bool animate);

// Etiqueta con texto y color propios, para discos que no son un album
// (hoy: la alarma). Misma pieza, contenido distinto.
// cover = nullptr dibuja la etiqueta de color con el texto; con imagen, la
// imagen ocupa el disco igual que en cualquier album.
void vinyl_set_custom(const char* name, const char* sub, uint32_t color,
                      bool animate, const uint16_t* cover = nullptr);

void vinyl_set_spinning(bool on);
bool vinyl_spinning();

// Avanza rotacion y suavizado del highlight. Llamar en cada pasada del loop.
void vinyl_tick();
