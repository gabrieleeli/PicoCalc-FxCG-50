#include "widget_system.hpp"
#include "font_renderer.hpp"
namespace ui::widgets {
void panel(gfx::Canvas& c,gfx::Rect r,std::uint16_t f,std::uint16_t b){c.roundedRect(r.x,r.y,r.w,r.h,5,f,true);c.roundedRect(r.x,r.y,r.w,r.h,5,b,false);}
void titleBar(gfx::Canvas& c,gfx::Rect r,const char* t){c.rect(r.x,r.y,r.w,r.h,gfx::palette::Header,true);font::drawText(c,r.x+7,r.y+7,t,gfx::palette::Cyan,1,r.w-14);}
void popup(gfx::Canvas& c,gfx::Rect r,const char* t,const char* b,const char* h){panel(c,r,gfx::palette::Panel,gfx::palette::Cyan);titleBar(c,{r.x+1,r.y+1,r.w-2,22},t);font::drawText(c,r.x+9,r.y+33,b,gfx::palette::White,1,r.w-18);font::drawText(c,r.x+9,r.y+r.h-14,h,gfx::palette::Orange,1,r.w-18);}
void selection(gfx::Canvas& c,gfx::Rect r,bool active){c.roundedRect(r.x,r.y,r.w,r.h,4,active?gfx::palette::Cyan:gfx::palette::Border,false);}
}
