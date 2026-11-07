#include "graphics_engine.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace gfx {
Transform2D Transform2D::translation(float x, float y) noexcept {
    Transform2D t; t.m02=x; t.m12=y; return t;
}
Transform2D Transform2D::scale(float x, float y) noexcept {
    Transform2D t; t.m00=x; t.m11=y; return t;
}
Transform2D Transform2D::rotation(float radians) noexcept {
    Transform2D t; const float c=std::cos(radians), sn=std::sin(radians);
    t.m00=c; t.m01=-sn; t.m10=sn; t.m11=c; return t;
}
PointF Transform2D::apply(PointF p) const noexcept {
    return {m00*p.x+m01*p.y+m02, m10*p.x+m11*p.y+m12};
}
Canvas::Canvas(std::uint16_t* pixels, int width, int height) noexcept { attach(pixels,width,height); }
void Canvas::attach(std::uint16_t* pixels,int width,int height) noexcept {
    pixels_=pixels; width_=std::max(0,width); height_=std::max(0,height); resetClip(); dirty_={}; dirtyRegions_={};
}
void Canvas::setClip(Rect r) noexcept {
    const int x0=std::clamp(r.x,0,width_), y0=std::clamp(r.y,0,height_);
    const long long rx=static_cast<long long>(r.x)+std::max(0,r.w);
    const long long ry=static_cast<long long>(r.y)+std::max(0,r.h);
    const int x1=static_cast<int>(std::clamp<long long>(rx,0,width_));
    const int y1=static_cast<int>(std::clamp<long long>(ry,0,height_));
    clip_={x0,y0,std::max(0,x1-x0),std::max(0,y1-y0)};
}
void Canvas::resetClip() noexcept { clip_={0,0,width_,height_}; }
void Canvas::markDirty(Rect r) noexcept {
    if(r.w<=0||r.h<=0)return;
    const auto unite=[](Rect a, Rect b) noexcept {
        const int x0=std::min(a.x,b.x), y0=std::min(a.y,b.y);
        const int x1=std::max(a.x+a.w,b.x+b.w), y1=std::max(a.y+a.h,b.y+b.h);
        return Rect{x0,y0,x1-x0,y1-y0};
    };
    dirty_ = dirty_.w<=0 || dirty_.h<=0 ? r : unite(dirty_,r);
    // Merge overlapping or edge-adjacent regions to avoid redundant LCD transfers.
    for(std::size_t i=0;i<dirtyRegions_.count;) {
        const Rect a=dirtyRegions_.rects[i];
        const bool separated = r.x > a.x+a.w || a.x > r.x+r.w ||
                               r.y > a.y+a.h || a.y > r.y+r.h;
        if(!separated) {
            r=unite(r,a);
            for(std::size_t j=i+1;j<dirtyRegions_.count;++j) dirtyRegions_.rects[j-1]=dirtyRegions_.rects[j];
            --dirtyRegions_.count; i=0;
        } else ++i;
    }
    if(dirtyRegions_.count<DirtyRegions::Capacity) dirtyRegions_.rects[dirtyRegions_.count++]=r;
    else { Rect all=r; for(std::size_t i=0;i<dirtyRegions_.count;++i) all=unite(all,dirtyRegions_.rects[i]);
        dirtyRegions_.rects[0]=all; dirtyRegions_.count=1; }
}
Rect Canvas::consumeDirty() noexcept {Rect r=dirty_;dirty_={};dirtyRegions_={};return r;}
DirtyRegions Canvas::consumeDirtyRegions() noexcept {DirtyRegions out=dirtyRegions_;dirtyRegions_={};dirty_={};return out;}
bool Canvas::inside(int x,int y) const noexcept {
    return pixels_ && x>=clip_.x && y>=clip_.y && x<clip_.x+clip_.w && y<clip_.y+clip_.h;
}
void Canvas::clear(std::uint16_t c) noexcept {
    if(!pixels_) return;
    for(int y=clip_.y;y<clip_.y+clip_.h;++y)
        std::fill(pixels_+static_cast<std::size_t>(y)*width_+clip_.x,
                  pixels_+static_cast<std::size_t>(y)*width_+clip_.x+clip_.w,c);
    markDirty(clip_);
}
void Canvas::pixel(int x,int y,std::uint16_t c) noexcept { if(inside(x,y)){pixels_[static_cast<std::size_t>(y)*width_+x]=c;markDirty({x,y,1,1});} }
std::uint16_t Canvas::getPixel(int x,int y) const noexcept {
    if(!inside(x,y)) return 0;
    return pixels_[static_cast<std::size_t>(y)*width_+x];
}
void Canvas::hLine(int x,int y,int w,std::uint16_t c) noexcept {
    if(w<=0 || y<clip_.y || y>=clip_.y+clip_.h || !pixels_) return;
    const long long end=static_cast<long long>(x)+w;
    const int x0=std::max(x,clip_.x), x1=static_cast<int>(std::min<long long>(end,clip_.x+clip_.w));
    if(x0<x1){std::fill(pixels_+static_cast<std::size_t>(y)*width_+x0,pixels_+static_cast<std::size_t>(y)*width_+x1,c);markDirty({x0,y,x1-x0,1});}
}
void Canvas::vLine(int x,int y,int h,std::uint16_t c) noexcept {
    if(h<=0 || x<clip_.x || x>=clip_.x+clip_.w || !pixels_) return;
    const long long end=static_cast<long long>(y)+h;
    const int y0=std::max(y,clip_.y), y1=static_cast<int>(std::min<long long>(end,clip_.y+clip_.h));
    for(int yy=y0;yy<y1;++yy) pixels_[static_cast<std::size_t>(yy)*width_+x]=c;
    if(y0<y1)markDirty({x,y0,1,y1-y0});
}
void Canvas::line(int x0,int y0,int x1,int y1,std::uint16_t c) noexcept {
    // Cohen-Sutherland clipping prevents work proportional to off-screen coordinates.
    if(!pixels_||clip_.w<=0||clip_.h<=0)return;
    const double xmin=clip_.x, ymin=clip_.y;
    const double xmax=static_cast<double>(clip_.x)+clip_.w-1.0;
    const double ymax=static_cast<double>(clip_.y)+clip_.h-1.0;
    double ax=x0,ay=y0,bx=x1,by=y1;
    auto code=[&](double x,double y) noexcept {
        unsigned out=0; if(x<xmin)out|=1U;else if(x>xmax)out|=2U;
        if(y<ymin)out|=4U;else if(y>ymax)out|=8U; return out;
    };
    unsigned ca=code(ax,ay),cb=code(bx,by);
    while(true){
        if((ca|cb)==0U)break;
        if((ca&cb)!=0U)return;
        const unsigned out=ca?ca:cb; double x=0.0,y=0.0;
        if(out&8U){if(by==ay)return;x=ax+(bx-ax)*(ymax-ay)/(by-ay);y=ymax;}
        else if(out&4U){if(by==ay)return;x=ax+(bx-ax)*(ymin-ay)/(by-ay);y=ymin;}
        else if(out&2U){if(bx==ax)return;y=ay+(by-ay)*(xmax-ax)/(bx-ax);x=xmax;}
        else {if(bx==ax)return;y=ay+(by-ay)*(xmin-ax)/(bx-ax);x=xmin;}
        if(out==ca){ax=x;ay=y;ca=code(ax,ay);}else{bx=x;by=y;cb=code(bx,by);}
    }
    int ix0=static_cast<int>(std::lround(ax)),iy0=static_cast<int>(std::lround(ay));
    const int ix1=static_cast<int>(std::lround(bx)),iy1=static_cast<int>(std::lround(by));
    const long long dx=std::llabs(static_cast<long long>(ix1)-ix0), sx=ix0<ix1?1:-1;
    const long long dy=-std::llabs(static_cast<long long>(iy1)-iy0), sy=iy0<iy1?1:-1;
    long long err=dx+dy;
    for(;;){pixel(ix0,iy0,c);if(ix0==ix1&&iy0==iy1)break;const long long e2=2*err;
        if(e2>=dy){err+=dy;ix0+=static_cast<int>(sx);}if(e2<=dx){err+=dx;iy0+=static_cast<int>(sy);}}
}
void Canvas::dashedLine(int x0,int y0,int x1,int y1,std::uint16_t c,int dash,int gap) noexcept {
    if(dash<=0 || gap<0) return;
    const int dx=x1-x0, dy=y1-y0;
    const int steps=std::max(std::abs(dx),std::abs(dy));
    if(steps==0){pixel(x0,y0,c);return;}
    const int period=dash+gap;
    for(int i=0;i<=steps;++i) if((i%period)<dash) {
        const int x=x0+static_cast<int>((static_cast<long long>(dx)*i)/steps);
        const int y=y0+static_cast<int>((static_cast<long long>(dy)*i)/steps);
        pixel(x,y,c);
    }
}
void Canvas::ellipse(int cx,int cy,int rx,int ry,std::uint16_t c,bool filled) noexcept {
    if(rx<0||ry<0)return;
    if(rx==0){vLine(cx,cy-ry,2*ry+1,c);return;}
    if(ry==0){hLine(cx-rx,cy,2*rx+1,c);return;}
    // Midpoint ellipse algorithm using 64-bit intermediates to avoid overflow.
    const long long rx2=static_cast<long long>(rx)*rx, ry2=static_cast<long long>(ry)*ry;
    long long x=0,y=ry,px=0,py=2*rx2*y;
    long long p=ry2-rx2*ry+rx2/4;
    auto plot=[&](int xx,int yy){
        if(filled){hLine(cx-xx,cy+yy,2*xx+1,c);if(yy)hLine(cx-xx,cy-yy,2*xx+1,c);}
        else {pixel(cx+xx,cy+yy,c);pixel(cx-xx,cy+yy,c);pixel(cx+xx,cy-yy,c);pixel(cx-xx,cy-yy,c);}
    };
    while(px<py){plot(static_cast<int>(x),static_cast<int>(y));++x;px+=2*ry2;
        if(p<0)p+=ry2+px;else{--y;py-=2*rx2;p+=ry2+px-py;}}
    p=ry2*(x*x+x)+rx2*(y-1)*(y-1)-rx2*ry2;
    while(y>=0){plot(static_cast<int>(x),static_cast<int>(y));--y;py-=2*rx2;
        if(p>0)p+=rx2-py;else{++x;px+=2*ry2;p+=rx2-py+px;}}
}
void Canvas::polyline(const Point* p,std::size_t n,std::uint16_t c) noexcept {
    if(!p||n<2)return;
    for(std::size_t i=1;i<n;++i)line(p[i-1].x,p[i-1].y,p[i].x,p[i].y,c);
}
void Canvas::bezier(const Point* cp,std::size_t n,std::uint16_t c,unsigned segments) noexcept {
    if(!cp||n<2||n>8||segments==0)return;
    Point previous=cp[0];
    for(unsigned step=1;step<=segments;++step){
        const double t=static_cast<double>(step)/segments, u=1.0-t;
        Point current{};
        // De Casteljau evaluation is stable for quadratic through degree-seven curves.
        PointF work[8]{};
        for(std::size_t i=0;i<n;++i)work[i]={static_cast<float>(cp[i].x),static_cast<float>(cp[i].y)};
        for(std::size_t level=n-1;level>0;--level)
            for(std::size_t i=0;i<level;++i){work[i].x=static_cast<float>(u*work[i].x+t*work[i+1].x);work[i].y=static_cast<float>(u*work[i].y+t*work[i+1].y);}
        current={static_cast<int>(std::lround(work[0].x)),static_cast<int>(std::lround(work[0].y))};
        line(previous.x,previous.y,current.x,current.y,c); previous=current;
    }
}
void Canvas::transformedPolyline(const PointF* p,std::size_t n,const Transform2D& tr,std::uint16_t c) noexcept {
    if(!p||n<2)return;
    Point previous{static_cast<int>(std::lround(tr.apply(p[0]).x)),static_cast<int>(std::lround(tr.apply(p[0]).y))};
    for(std::size_t i=1;i<n;++i){const PointF q=tr.apply(p[i]);const Point current{static_cast<int>(std::lround(q.x)),static_cast<int>(std::lround(q.y))};line(previous.x,previous.y,current.x,current.y,c);previous=current;}
}
void Canvas::rect(int x,int y,int w,int h,std::uint16_t c,bool filled) noexcept {
    if(w<=0||h<=0) return;
    if(filled){const int y0=std::max(y,clip_.y),y1=static_cast<int>(std::min<long long>(static_cast<long long>(y)+h,clip_.y+clip_.h));
        for(int yy=y0;yy<y1;++yy) hLine(x,yy,w,c);
        return;
    }
    hLine(x,y,w,c); if(h>1) hLine(x,y+h-1,w,c);
    if(h>2){vLine(x,y+1,h-2,c);if(w>1)vLine(x+w-1,y+1,h-2,c);}
}
void Canvas::roundedRect(int x,int y,int w,int h,int r,std::uint16_t c,bool filled) noexcept {
    if(w<=0||h<=0) return;
    r=std::clamp(r,0,std::min(w,h)/2);
    if(r==0){rect(x,y,w,h,c,filled);return;}
    if(filled){
        for(int yy=0;yy<h;++yy){int inset=0;
            if(yy<r){const double dy=r-yy-0.5;inset=r-static_cast<int>(std::floor(std::sqrt(std::max(0.0,r*r-dy*dy))));}
            else if(yy>=h-r){const double dy=yy-(h-r)+0.5;inset=r-static_cast<int>(std::floor(std::sqrt(std::max(0.0,r*r-dy*dy))));}
            hLine(x+inset,y+yy,w-2*inset,c);
        }
    } else {
        line(x+r,y,x+w-r-1,y,c); line(x+r,y+h-1,x+w-r-1,y+h-1,c);
        line(x,y+r,x,y+h-r-1,c); line(x+w-1,y+r,x+w-1,y+h-r-1,c);
        arc(x+r,y+r,r,180,270,c); arc(x+w-r-1,y+r,r,270,360,c);
        arc(x+w-r-1,y+h-r-1,r,0,90,c); arc(x+r,y+h-r-1,r,90,180,c);
    }
}
void Canvas::circle(int cx,int cy,int r,std::uint16_t c,bool filled) noexcept {
    if(r<0) return;
    int x=r,y=0,err=1-r;
    while(x>=y){
        if(filled){hLine(cx-x,cy+y,2*x+1,c);hLine(cx-x,cy-y,2*x+1,c);hLine(cx-y,cy+x,2*y+1,c);hLine(cx-y,cy-x,2*y+1,c);}
        else {pixel(cx+x,cy+y,c);pixel(cx+y,cy+x,c);pixel(cx-y,cy+x,c);pixel(cx-x,cy+y,c);pixel(cx-x,cy-y,c);pixel(cx-y,cy-x,c);pixel(cx+y,cy-x,c);pixel(cx+x,cy-y,c);}
        ++y; if(err<0) err+=2*y+1; else {--x;err+=2*(y-x)+1;}
    }
}
void Canvas::arc(int cx,int cy,int r,int start,int end,std::uint16_t c) noexcept {
    if(r<0) return;
    if(end<start) end+=360;
    const int span=end-start;
    const int steps=std::max(1,span*std::max(1,r)/45);
    int px=cx+static_cast<int>(std::lround(r*std::cos(start*3.14159265358979323846/180.0)));
    int py=cy+static_cast<int>(std::lround(r*std::sin(start*3.14159265358979323846/180.0)));
    for(int i=1;i<=steps;++i){const double a=(start+span*static_cast<double>(i)/steps)*3.14159265358979323846/180.0;
        const int nx=cx+static_cast<int>(std::lround(r*std::cos(a))),ny=cy+static_cast<int>(std::lround(r*std::sin(a)));line(px,py,nx,ny,c);px=nx;py=ny;}
}
void Canvas::polygon(const Point* p,std::size_t n,std::uint16_t c,bool filled) noexcept {
    if(!p||n<2) return;
    if(!filled){
        for(std::size_t i=0;i<n;++i){const Point a=p[i],b=p[(i+1)%n];line(a.x,a.y,b.x,b.y,c);}
        return;
    }
    constexpr std::size_t Max=32; if(n>Max)return;
    int minY=p[0].y,maxY=p[0].y; for(std::size_t i=1;i<n;++i){minY=std::min(minY,p[i].y);maxY=std::max(maxY,p[i].y);}
    minY=std::max(minY,clip_.y);maxY=std::min(maxY,clip_.y+clip_.h-1);
    int intersections[Max];
    for(int y=minY;y<=maxY;++y){std::size_t count=0;
        for(std::size_t i=0,j=n-1;i<n;j=i++) {const Point a=p[i],b=p[j];
            if((a.y<=y&&b.y>y)||(b.y<=y&&a.y>y)) intersections[count++]=a.x+(y-a.y)*(b.x-a.x)/(b.y-a.y);}
        std::sort(intersections,intersections+count);
        for(std::size_t i=0;i+1<count;i+=2) hLine(intersections[i],y,intersections[i+1]-intersections[i]+1,c);
    }
}
void Canvas::bitmap(int x,int y,int w,int h,const std::uint16_t* src,std::uint16_t key,bool transparent) noexcept {
    if(!src||w<=0||h<=0)return;
    for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx){const auto c=src[static_cast<std::size_t>(yy)*w+xx];if(!transparent||c!=key)pixel(x+xx,y+yy,c);}
}
std::uint16_t Canvas::blend(std::uint16_t fg,std::uint16_t bg,std::uint8_t a) noexcept {
    const unsigned inv=255-a;
    const unsigned fr=(fg>>11)&31,fgc=(fg>>5)&63,fb=fg&31;
    const unsigned br=(bg>>11)&31,bgc=(bg>>5)&63,bb=bg&31;
    const unsigned r=(fr*a+br*inv+127)/255,g=(fgc*a+bgc*inv+127)/255,b=(fb*a+bb*inv+127)/255;
    return static_cast<std::uint16_t>((r<<11)|(g<<5)|b);
}
void Canvas::drawLayer(const Layer& layer) noexcept {
    if(!layer.visible || !layer.pixels) return;
    bitmap(layer.x,layer.y,layer.width,layer.height,layer.pixels,layer.transparentKey,layer.useTransparency);
}
void Canvas::blendPixel(int x,int y,std::uint16_t c,std::uint8_t a) noexcept {if(a==255)pixel(x,y,c);else if(a)pixel(x,y,blend(c,getPixel(x,y),a));}
void Canvas::verticalGradient(int x,int y,int w,int h,std::uint16_t top,std::uint16_t bottom) noexcept {
    if(w<=0||h<=0)return;
    for(int i=0;i<h;++i){
        const unsigned a=h>1?static_cast<unsigned>((i*255)/(h-1)):0U;
        hLine(x,y+i,w,blend(bottom,top,static_cast<std::uint8_t>(a)));
    }
}
void Canvas::dottedRect(int x,int y,int w,int h,std::uint16_t c) noexcept {
    if(w<=0||h<=0)return;
    for(int i=0;i<w;i+=2){pixel(x+i,y,c);pixel(x+i,y+h-1,c);}
    for(int j=0;j<h;j+=2){pixel(x,y+j,c);pixel(x+w-1,y+j,c);}
}
std::size_t TileDiff::diff(const std::uint16_t* px,int width,int height,Rect* out,std::size_t maxOut) noexcept {
    if(!px||!out||maxOut==0||width<=0||height<=0)return 0;
    const int tx=std::min(MaxTilesX,(width+Tile-1)/Tile),ty=std::min(MaxTilesY,(height+Tile-1)/Tile);
    bool changed[MaxTilesX*MaxTilesY]{};
    bool any=false;
    for(int r=0;r<ty;++r)for(int c=0;c<tx;++c){
        std::uint32_t h=2166136261u;
        const int y1=std::min(height,(r+1)*Tile),x0=c*Tile,x1=std::min(width,x0+Tile);
        for(int y=r*Tile;y<y1;++y){const std::uint16_t* row=px+static_cast<std::size_t>(y)*width;
            for(int x=x0;x<x1;++x){h^=row[x];h*=16777619u;}}
        const int i=r*MaxTilesX+c;
        if(!valid_||hashes_[i]!=h){changed[i]=true;any=true;}
        hashes_[i]=h;
    }
    valid_=true;
    if(!any)return 0;
    std::size_t n=0;bool overflow=false;
    for(int r=0;r<ty&&!overflow;++r){
        int c=0;
        while(c<tx){
            if(!changed[r*MaxTilesX+c]){++c;continue;}
            const int start=c;while(c<tx&&changed[r*MaxTilesX+c])++c;
            Rect rect{start*Tile,r*Tile,std::min(width,c*Tile)-start*Tile,std::min(height,(r+1)*Tile)-r*Tile};
            // Merge with a rectangle directly above that has the same horizontal span.
            bool merged=false;
            for(std::size_t k=0;k<n;++k){Rect& o=out[k];
                if(o.x==rect.x&&o.w==rect.w&&o.y+o.h==rect.y){o.h+=rect.h;merged=true;break;}}
            if(merged)continue;
            if(n==maxOut){overflow=true;break;}
            out[n++]=rect;
        }
    }
    if(overflow){
        int x0=width,y0=height,x1=0,y1=0;
        for(int r=0;r<ty;++r)for(int c=0;c<tx;++c)if(changed[r*MaxTilesX+c]){
            x0=std::min(x0,c*Tile);y0=std::min(y0,r*Tile);
            x1=std::max(x1,std::min(width,(c+1)*Tile));y1=std::max(y1,std::min(height,(r+1)*Tile));}
        out[0]={x0,y0,x1-x0,y1-y0};return 1;
    }
    return n;
}
} // namespace gfx
