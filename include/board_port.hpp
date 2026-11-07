#pragma once
#include <cstdint>
#include "graphics_engine.hpp"
namespace board {
void init();
gfx::Canvas& graphics() noexcept;
void clear(uint16_t rgb565);
void text(int x,int y,const char* s,uint16_t rgb565);
void rect(int x,int y,int w,int h,uint16_t rgb565,bool filled=false);
void pixel(int x,int y,uint16_t rgb565);
void present();
int readKey();
bool wifiConnect(const char* ssid,const char* password);
}
