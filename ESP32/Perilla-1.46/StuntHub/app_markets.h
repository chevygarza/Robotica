#pragma once
#include <Arduino.h>

#define MKT_MAX 6
struct Coin {
  char   sym[8] = "";
  double price  = 0;
  double chg24  = 0;
  bool   valid  = false;
};
extern Coin g_coins[MKT_MAX];
extern int  g_coinCount;

void markets_begin();
void markets_request();              // refetch a demanda
bool markets_lock(uint32_t ms = 30);
void markets_unlock();
bool markets_consumeDirty();
