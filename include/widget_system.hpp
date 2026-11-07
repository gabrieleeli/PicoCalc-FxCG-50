#pragma once
#include "graphics_engine.hpp"
namespace ui::widgets {
void panel(gfx::Canvas&,gfx::Rect,std::uint16_t fill,std::uint16_t border);
void titleBar(gfx::Canvas&,gfx::Rect,const char* title);
void popup(gfx::Canvas&,gfx::Rect,const char* title,const char* body,const char* hint);
void selection(gfx::Canvas&,gfx::Rect,bool active);
}
