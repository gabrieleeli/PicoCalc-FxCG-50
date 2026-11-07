#include "graph_renderer.hpp"
#include "expression.hpp"
#include <algorithm>
#include <cmath>

namespace fx {
void GraphRenderer::draw(gfx::Canvas& c,const std::string& expression,const GraphView& v){
    const auto old=c.clip(); c.setClip({8,34,std::max(0,c.width()-16),std::max(0,c.height()-78)});
    const int w=c.width(),h=c.height(),left=12,right=w-12,top=42,bottom=h-52;
    c.rect(left,top,right-left,bottom-top,gfx::palette::Border,false);
    auto sx=[&](double x){return left+static_cast<int>(std::lround((x-v.xmin)*(right-left)/(v.xmax-v.xmin)));};
    auto sy=[&](double y){return bottom-static_cast<int>(std::lround((y-v.ymin)*(bottom-top)/(v.ymax-v.ymin)));};
    const double dx=(v.xmax-v.xmin)/10.0,dy=(v.ymax-v.ymin)/10.0;
    if(dx>0&&dy>0){for(int i=0;i<=10;++i){int x=sx(v.xmin+i*dx),y=sy(v.ymin+i*dy);c.vLine(x,top,bottom-top,gfx::palette::Panel);c.hLine(left,y,right-left,gfx::palette::Panel);}}
    if(v.xmin<=0&&v.xmax>=0)c.vLine(sx(0),top,bottom-top,gfx::palette::White);
    if(v.ymin<=0&&v.ymax>=0)c.hLine(left,sy(0),right-left,gfx::palette::White);
    bool previous=false;int px=0,py=0;
    for(int x=left;x<=right;++x){double xv=v.xmin+(v.xmax-v.xmin)*(x-left)/(right-left);try{double yv=evaluate(expression,xv);if(!std::isfinite(yv)||yv<v.ymin||yv>v.ymax){previous=false;continue;}int yy=sy(yv);if(previous&&std::abs(yy-py)<(bottom-top))c.line(px,py,x,yy,gfx::palette::Cyan);px=x;py=yy;previous=true;}catch(...){previous=false;}}
    if(v.trace){const int cx=std::clamp(v.cursorX,left,right);double xv=v.xmin+(v.xmax-v.xmin)*(cx-left)/(right-left);try{double yv=evaluate(expression,xv);int cy=sy(yv);c.vLine(cx,top,bottom-top,gfx::palette::Orange);if(cy>=top&&cy<=bottom)c.circle(cx,cy,3,gfx::palette::Orange,true);}catch(...){}}
    c.resetClip();c.setClip(old);
}
void GraphRenderer::zoom(GraphView& v,double f) noexcept {if(!(f>0.05&&f<20))return;double cx=(v.xmin+v.xmax)*0.5,cy=(v.ymin+v.ymax)*0.5;double hw=(v.xmax-v.xmin)*0.5*f,hh=(v.ymax-v.ymin)*0.5*f;v.xmin=cx-hw;v.xmax=cx+hw;v.ymin=cy-hh;v.ymax=cy+hh;}
void GraphRenderer::pan(GraphView& v,double dx,double dy) noexcept {v.xmin+=dx;v.xmax+=dx;v.ymin+=dy;v.ymax+=dy;}
}
