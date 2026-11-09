#include "graphics_engine.hpp"
#include <array>
#include "test_util.hpp"
#undef assert
#define assert(x) CHECK(x)
#include <cstdint>
#include <initializer_list>

int main() {
    constexpr int W=32,H=24;
    std::array<std::uint16_t,W*H> fb{};
    gfx::Canvas c(fb.data(),W,H);
    c.clear(gfx::palette::Black);
    c.pixel(2,3,gfx::palette::White);
    assert(c.getPixel(2,3)==gfx::palette::White);
    c.line(0,0,7,7,gfx::palette::Red);
    for(int i=0;i<8;++i) assert(c.getPixel(i,i)==gfx::palette::Red);
    c.hLine(1,10,8,gfx::palette::Green);
    c.vLine(5,8,6,gfx::palette::Cyan);
    assert(c.getPixel(3,10)==gfx::palette::Green);
    assert(c.getPixel(5,9)==gfx::palette::Cyan);
    c.rect(10,2,8,6,gfx::palette::Orange,true);
    assert(c.getPixel(12,4)==gfx::palette::Orange);
    c.rect(20,2,7,6,gfx::palette::Purple,false);
    assert(c.getPixel(20,2)==gfx::palette::Purple);
    assert(c.getPixel(23,4)!=gfx::palette::Purple);
    c.roundedRect(1,14,12,8,3,gfx::palette::White,true);
    assert(c.getPixel(6,17)==gfx::palette::White);
    c.circle(22,16,4,gfx::palette::Red,false);
    assert(c.getPixel(26,16)==gfx::palette::Red);
    const gfx::Point tri[]={{14,12},{20,12},{17,18}};
    c.polygon(tri,3,gfx::palette::Green,true);
    assert(c.getPixel(17,14)==gfx::palette::Green);
    c.setClip({0,0,4,4});
    c.clear(gfx::palette::Navy);
    assert(c.getPixel(0,0)==gfx::palette::Navy);
    assert(c.getPixel(5,5)!=gfx::palette::Navy);
    c.pixel(7,7,gfx::palette::White);
    assert(c.getPixel(7,7)!=gfx::palette::White);
    c.resetClip();
    c.consumeDirty();
    c.pixel(9,9,gfx::palette::Orange);
    const auto dirty=c.consumeDirty();
    assert(dirty.x==9&&dirty.y==9&&dirty.w==1&&dirty.h==1);
    assert(c.dirtyRect().w==0);
    c.blendPixel(30,20,gfx::palette::White,128);
    assert(c.getPixel(30,20)!=gfx::palette::Black);
    assert(gfx::Canvas::blend(gfx::palette::Red,gfx::palette::Black,255)==gfx::palette::Red);
    const std::uint16_t image[4]={gfx::palette::Red,gfx::palette::Green,gfx::palette::Blue,gfx::palette::White};
    c.bitmap(28,0,2,2,image,gfx::palette::Black,false);
    assert(c.getPixel(28,0)==gfx::palette::Red && c.getPixel(29,1)==gfx::palette::White);
    c.arc(16,6,3,0,90,gfx::palette::Cyan);
    assert(c.getPixel(19,6)==gfx::palette::Cyan);
    const gfx::Layer layer{image,24,20,2,2,gfx::palette::Black,true,false};
    c.drawLayer(layer);
    assert(c.getPixel(24,20)==gfx::palette::Red);
    const auto before=c.getPixel(24,20);
    c.drawLayer({image,24,20,2,2,gfx::palette::Black,false,false});
    assert(c.getPixel(24,20)==before);

    // Dashed line: dash/gap pattern is deterministic and clipped by the canvas.
    c.clear(gfx::palette::Black);
    c.dashedLine(0,2,13,2,gfx::palette::White,3,2);
    for (int x : {0,1,2,5,6,7,10,11,12}) assert(c.getPixel(x,2)==gfx::palette::White);
    for (int x : {3,4,8,9,13}) assert(c.getPixel(x,2)==gfx::palette::Black);

    c.ellipse(16,12,6,3,gfx::palette::Green,false);
    assert(c.getPixel(22,12)==gfx::palette::Green);
    assert(c.getPixel(16,9)==gfx::palette::Green);
    c.ellipse(16,12,4,2,gfx::palette::Orange,true);
    assert(c.getPixel(16,12)==gfx::palette::Orange);

    const gfx::Point path[]={{1,1},{5,1},{5,5},{9,5}};
    c.polyline(path,4,gfx::palette::Cyan);
    assert(c.getPixel(3,1)==gfx::palette::Cyan && c.getPixel(5,4)==gfx::palette::Cyan);
    const gfx::Point curve[]={{1,20},{8,10},{15,20}};
    c.bezier(curve,3,gfx::palette::Purple,24);
    assert(c.getPixel(1,20)==gfx::palette::Purple && c.getPixel(15,20)==gfx::palette::Purple);

    const gfx::PointF local[]={{0,0},{2,0},{2,2}};
    const auto transform=gfx::Transform2D::translation(10,10);
    c.transformedPolyline(local,3,transform,gfx::palette::Red);
    assert(c.getPixel(11,10)==gfx::palette::Red && c.getPixel(12,11)==gfx::palette::Red);
    const auto rotated=gfx::Transform2D::rotation(3.14159265358979323846/2.0).apply({1.0F,0.0F});
    assert(rotated.x>-0.001F && rotated.x<0.001F && rotated.y>0.999F);

    const auto red=gfx::rgb888To565(255,0,0);
    assert(red==gfx::palette::Red);
    const auto rgb=gfx::rgb565To888(gfx::palette::White);
    assert(rgb.r==255 && rgb.g==255 && rgb.b==255);
    assert(gfx::swapRgb565Bytes(0x1234)==0x3412);

    c.consumeDirtyRegions();
    c.pixel(1,1,gfx::palette::White);
    c.pixel(30,22,gfx::palette::White);
    const auto regions=c.consumeDirtyRegions();
    assert(regions.count==2);
    assert(regions.rects[0].x==1 && regions.rects[1].x==30);
    assert(c.dirtyRect().w==0);

    // Huge off-screen endpoints must be clipped before rasterization.
    c.line(-1000000000,12,1000000000,12,gfx::palette::Blue);
    assert(c.getPixel(0,12)==gfx::palette::Blue);
    assert(c.getPixel(W-1,12)==gfx::palette::Blue);
    return testutil::finish("graphics_smoke");
}
