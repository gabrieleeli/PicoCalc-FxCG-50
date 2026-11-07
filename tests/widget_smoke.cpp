#include "widget_system.hpp"
#include <array>
#include <cassert>
int main(){std::array<unsigned short,320*320> fb{};gfx::Canvas c(fb.data(),320,320);ui::widgets::panel(c,{5,5,100,60},gfx::palette::Panel,gfx::palette::Cyan);ui::widgets::titleBar(c,{6,6,98,20},"TEST");ui::widgets::popup(c,{20,80,180,100},"DIALOG","CONTENUTO","EXE OK");ui::widgets::selection(c,{5,5,100,60},true);assert(c.getPixel(30,90)!=0);}
