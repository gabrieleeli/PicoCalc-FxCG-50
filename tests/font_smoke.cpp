#include "font_renderer.hpp"
#include <array>
#include <cassert>
#include <cstdint>
int main(){
    std::array<std::uint16_t,80*32> fb{};
    gfx::Canvas canvas(fb.data(),80,32);
    const auto small=font::measure("ABC",1);
    const auto large=font::measure("ABC",2);
    assert(small.width==18 && small.height==7);
    assert(large.width==36 && large.height==14);
    auto drawn=font::drawText(canvas,2,2,"A\nB",gfx::palette::White,1,20);
    assert(drawn.width==6 && drawn.height==16);
    bool found=false;
    for(auto p:fb) if(p==gfx::palette::White){found=true;break;}
    assert(found);
    canvas.clear(gfx::palette::Black);
    font::drawText(canvas,0,0,"pi=π sqrt=√2 αβγδεζηθικλμνξοπρστυφχψω ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩ",gfx::palette::Cyan,1,80);
    found=false;for(auto p:fb)if(p==gfx::palette::Cyan){found=true;break;}assert(found);
    canvas.setClip({0,0,3,3});
    font::drawText(canvas,0,0,"MMMM",gfx::palette::Red,1,80);
    for(int y=0;y<32;++y)for(int x=0;x<80;++x)
        if(x>=3||y>=3) assert(fb[static_cast<std::size_t>(y)*80+x]!=gfx::palette::Red);
}
