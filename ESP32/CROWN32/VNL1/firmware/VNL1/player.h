// VNL-1 — Audio: DFPlayer Mini por UART1 (IO4 = TX, IO12 = RX).
//
// Dos cosas que este modulo resuelve y que no son opcionales:
//
// 1) THROTTLING. El encoder genera muchos pulsos por vuelta y el DFPlayer habla
//    a 9600 baudios. Mandarle un comando por pulso lo satura y deja de
//    responder. Aqui todo pasa por una cola que se vacia a un comando cada
//    PLAYER_MIN_GAP_MS, y los cambios de volumen se colapsan: si giras la
//    perilla 12 muescas, se manda UN comando con el volumen final, no 12.
//
// 2) TOLERANCIA A NO TENER HARDWARE. Si el modulo no contesta, player_begin()
//    devuelve false y todo lo demas se vuelve no-op. La UI sigue corriendo
//    completa, solo sin sonido: asi se puede trabajar la pantalla sin el
//    DFPlayer conectado.
#pragma once
#include <Arduino.h>

#define PLAYER_MIN_GAP_MS   60
#define PLAYER_VOL_MAX      30

bool player_begin();
bool player_available();          // false = no hay modulo, todo es no-op

// Llamar en cada pasada del loop: vacia la cola y atiende avisos del modulo.
void player_tick();

// Arranca un album en orden aleatorio (barajado con esp_random).
void player_play_album(uint8_t folder, uint8_t tracks);

void player_pause();
void player_resume();
void player_stop();
bool player_playing();

void    player_next();

// true UNA vez cuando se acabo la ultima pista del album. Antes el reproductor
// rebarajaba y seguia para siempre por su cuenta; ahora avisa y deja que la
// maquina de estados decida, segun lo que el usuario haya puesto en Ajustes.
bool    player_album_fin();
void    player_set_volume(uint8_t vol);   // 0..30, se colapsa en la cola
uint8_t player_volume();

// Posicion dentro del album barajado, para la UI.
uint8_t player_track_index();             // 1-based sobre el orden barajado
uint8_t player_track_count();
uint8_t player_track_file();              // 1-based sobre los archivos reales

// Segundos desde que arranco la pista actual. El DFPlayer no reporta posicion,
// asi que la barra de progreso se estima con reloj propio contra la duracion
// del manifiesto.
uint32_t player_elapsed_s();
