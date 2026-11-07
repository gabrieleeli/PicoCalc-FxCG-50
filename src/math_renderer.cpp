#include "math_renderer.hpp"
#include "memory.hpp"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>

namespace fx::math { namespace {
constexpr int GlyphW=6, GlyphH=9, GlyphBase=7, MaxDepth=32;
std::size_t glyphCount(const std::string& s) noexcept {std::size_t n=0;for(unsigned char c:s)if((c&0xC0)!=0x80)++n;return n;}
std::size_t byteOffset(const std::string& s,std::size_t glyphs) noexcept {std::size_t i=0,n=0;while(i<s.size()&&n<glyphs){unsigned char c=static_cast<unsigned char>(s[i]);i+=(c<0x80?1:(c&0xE0)==0xC0?2:(c&0xF0)==0xE0?3:4);++n;}return std::min(i,s.size());}
std::string tokenRun(const NaturalNode& n,std::size_t begin,std::size_t end){std::string s;for(std::size_t i=begin;i<end;++i)s+=n.children[i]->text;return s;}
std::string shown(const std::string& s){return formatNatural(s);}
int radicalPrefixWidth(const NaturalNode& n){if(n.text.empty()||n.text=="2"||n.text=="3"||n.text=="4")return 0;return static_cast<int>(glyphCount(n.text))*GlyphW;}
int satAdd(int a,int b) noexcept { const auto v=static_cast<std::int64_t>(a)+b; return static_cast<int>(std::clamp<std::int64_t>(v,0,1000000)); }
Size measureNode(const NaturalNode& n,unsigned depth) noexcept {
    if(depth>MaxDepth)return {0,GlyphH,GlyphBase};
    if(n.kind==NaturalNodeKind::Token){const auto w=static_cast<std::int64_t>(glyphCount(shown(n.text)))*GlyphW;return {static_cast<int>(std::min<std::int64_t>(w,1000000)),GlyphH,GlyphBase};}
    if(n.kind==NaturalNodeKind::Row){Size s{0,GlyphH,GlyphBase};for(std::size_t i=0;i<n.children.size();){if(!n.children[i]){++i;continue;}if(n.children[i]->kind==NaturalNodeKind::Token){std::size_t j=i;while(j<n.children.size()&&n.children[j]&&n.children[j]->kind==NaturalNodeKind::Token)++j;s.width=satAdd(s.width,static_cast<int>(glyphCount(shown(tokenRun(n,i,j))))*GlyphW);i=j;}else{auto q=measureNode(*n.children[i],depth+1);s.width=satAdd(s.width,q.width);s.height=std::max(s.height,q.height);s.baseline=std::max(s.baseline,q.baseline);++i;}}return s;}
    if(n.kind==NaturalNodeKind::Fraction&&n.children.size()>=2&&n.children[0]&&n.children[1]){auto a=measureNode(*n.children[0],depth+1),b=measureNode(*n.children[1],depth+1);return {satAdd(std::max(a.width,b.width),8),satAdd(satAdd(a.height,b.height),7),satAdd(a.height,3)};}
    if(n.kind==NaturalNodeKind::Radical&&!n.children.empty()&&n.children[0]){auto a=measureNode(*n.children[0],depth+1);const int prefix=radicalPrefixWidth(n);return {satAdd(a.width,14+prefix),satAdd(a.height,4),satAdd(a.baseline,2)};}
    if((n.kind==NaturalNodeKind::Power||n.kind==NaturalNodeKind::Subscript)&&n.children.size()>=2&&n.children[0]&&n.children[1]){auto a=measureNode(*n.children[0],depth+1),b=measureNode(*n.children[1],depth+1);return {satAdd(a.width,b.width),std::max(a.height,satAdd(b.height,2)),satAdd(a.baseline,n.kind==NaturalNodeKind::Power?std::max(0,b.height-3):0)};}
    if(n.kind==NaturalNodeKind::Binary&&n.children.size()==2&&n.children[0]&&n.children[1]){auto a=measureNode(*n.children[0],depth+1),b=measureNode(*n.children[1],depth+1);return {satAdd(satAdd(a.width,static_cast<int>(glyphCount(n.text))*GlyphW+4),b.width),std::max(a.height,b.height),std::max(a.baseline,b.baseline)};}
    if(n.kind==NaturalNodeKind::Unary&&n.children.size()==1&&n.children[0]){auto a=measureNode(*n.children[0],depth+1);return {satAdd(a.width,static_cast<int>(glyphCount(n.text))*GlyphW),a.height,a.baseline};}
    if(n.kind==NaturalNodeKind::Call&&!n.children.empty()){int w=static_cast<int>(glyphCount(n.text))*GlyphW+12,h=GlyphH,base=GlyphBase;for(const auto& c:n.children)if(c){auto a=measureNode(*c,depth+1);w=satAdd(w,a.width+6);h=std::max(h,a.height);base=std::max(base,a.baseline);}return {w,h,base};}
    if(n.kind==NaturalNodeKind::Group&&!n.children.empty()&&n.children[0]){auto a=measureNode(*n.children[0],depth+1);return {satAdd(a.width,12),a.height,a.baseline};}
    if(n.kind==NaturalNodeKind::Matrix&&!n.children.empty()){int w=0,h=0;for(const auto& r:n.children)if(r){auto q=measureNode(*r,depth+1);w=std::max(w,q.width);h=satAdd(h,satAdd(q.height,3));}h=std::max(GlyphH,h-3);return {satAdd(w,16),h,std::max(GlyphBase,h/2)};}
    return {0,GlyphH,GlyphBase};
}
void addText(std::vector<NaturalDrawOp>& o,int x,int y,const std::string& s){o.push_back({NaturalDrawOp::Kind::Text,x,y,0,0,s});}
void addLine(std::vector<NaturalDrawOp>& o,int x,int y,int x2,int y2){o.push_back({NaturalDrawOp::Kind::Line,x,y,x2,y2,{}});}
void drawNode(const NaturalNode& n,int x,int base,std::vector<NaturalDrawOp>& out,unsigned depth){
    if(depth>MaxDepth)return;
    if(n.kind==NaturalNodeKind::Token){addText(out,x,base,shown(n.text));return;}
    if(n.kind==NaturalNodeKind::Row){int px=x;for(std::size_t i=0;i<n.children.size();){if(!n.children[i]){++i;continue;}if(n.children[i]->kind==NaturalNodeKind::Token){std::size_t j=i;while(j<n.children.size()&&n.children[j]&&n.children[j]->kind==NaturalNodeKind::Token)++j;std::string raw=tokenRun(n,i,j),display=shown(raw);addText(out,px,base,display);px=satAdd(px,static_cast<int>(glyphCount(display))*GlyphW);i=j;}else{drawNode(*n.children[i],px,base,out,depth+1);px=satAdd(px,measureNode(*n.children[i],depth+1).width);++i;}}return;}
    if(n.kind==NaturalNodeKind::Fraction&&n.children.size()>=2&&n.children[0]&&n.children[1]){auto a=measureNode(*n.children[0],depth+1),b=measureNode(*n.children[1],depth+1);int w=std::max(a.width,b.width)+8;drawNode(*n.children[0],x+(w-a.width)/2,base-a.height-3,out,depth+1);addLine(out,x,base-1,satAdd(x,w),base-1);drawNode(*n.children[1],x+(w-b.width)/2,base+b.height+3,out,depth+1);return;}
    if(n.kind==NaturalNodeKind::Radical&&!n.children.empty()&&n.children[0]){auto a=measureNode(*n.children[0],depth+1);int prefix=radicalPrefixWidth(n);if(prefix>0)addText(out,x,base-4,n.text);addText(out,x+prefix,base,n.text=="3"?"∛":(n.text=="4"?"∜":"√"));addLine(out,x+prefix+9,base-a.height-2,satAdd(x+prefix+12,a.width),base-a.height-2);drawNode(*n.children[0],x+prefix+13,base,out,depth+1);return;}
    if((n.kind==NaturalNodeKind::Power||n.kind==NaturalNodeKind::Subscript)&&n.children.size()>=2&&n.children[0]&&n.children[1]){auto a=measureNode(*n.children[0],depth+1);drawNode(*n.children[0],x,base,out,depth+1);drawNode(*n.children[1],satAdd(x,a.width),base+(n.kind==NaturalNodeKind::Power?-5:3),out,depth+1);return;}
    if(n.kind==NaturalNodeKind::Binary&&n.children.size()==2&&n.children[0]&&n.children[1]){auto a=measureNode(*n.children[0],depth+1);drawNode(*n.children[0],x,base,out,depth+1);int ox=satAdd(x,a.width+2);addText(out,ox,base,n.text);drawNode(*n.children[1],satAdd(ox,static_cast<int>(glyphCount(n.text))*GlyphW+2),base,out,depth+1);return;}
    if(n.kind==NaturalNodeKind::Unary&&n.children.size()==1&&n.children[0]){addText(out,x,base,n.text);drawNode(*n.children[0],satAdd(x,static_cast<int>(glyphCount(n.text))*GlyphW),base,out,depth+1);return;}
    if(n.kind==NaturalNodeKind::Call&&!n.children.empty()){addText(out,x,base,n.text+"(");int px=satAdd(x,static_cast<int>(glyphCount(n.text)+1)*GlyphW);for(std::size_t i=0;i<n.children.size();++i){if(i)addText(out,px,base,",");if(n.children[i]){drawNode(*n.children[i],px,base,out,depth+1);px=satAdd(px,measureNode(*n.children[i],depth+1).width);}if(i+1<n.children.size())px=satAdd(px,GlyphW);}addText(out,px,base,")");return;}
    if(n.kind==NaturalNodeKind::Group&&!n.children.empty()&&n.children[0]){auto a=measureNode(*n.children[0],depth+1);addText(out,x,base,"(");drawNode(*n.children[0],x+6,base,out,depth+1);addText(out,satAdd(x+6,a.width),base,")");return;}
    if(n.kind==NaturalNodeKind::Matrix&&!n.children.empty()){auto sz=measureNode(n,depth+1);int yy=base-sz.baseline;addLine(out,x+5,yy,x+1,yy+sz.height/2);addLine(out,x+1,yy+sz.height/2,x+5,yy+sz.height);addLine(out,x+sz.width-5,yy,x+sz.width-1,yy+sz.height/2);addLine(out,x+sz.width-1,yy+sz.height/2,x+sz.width-5,yy+sz.height);for(const auto& r:n.children)if(r){auto q=measureNode(*r,depth+1);drawNode(*r,x+8,yy+q.baseline,out,depth+1);yy=satAdd(yy,q.height+3);}}
}
void clipOps(Layout& r,int left,int top,int right,int bottom){
    std::vector<NaturalDrawOp> clipped;clipped.reserve(r.ops.size());
    for(auto op:r.ops){
        if(op.kind==NaturalDrawOp::Kind::Text){if(op.y<top||op.y+2>bottom||op.x>=right||op.x+GlyphW<=left){r.clipped=true;continue;}if(op.x<left){const int skip=(left-op.x+GlyphW-1)/GlyphW;if(skip>=static_cast<int>(glyphCount(op.text))){r.clipped=true;continue;}op.text.erase(0,byteOffset(op.text,static_cast<std::size_t>(skip)));op.x+=skip*GlyphW;r.clipped=true;}const int fit=std::max(0,(right-op.x)/GlyphW);if(fit<static_cast<int>(glyphCount(op.text))){op.text.resize(byteOffset(op.text,static_cast<std::size_t>(fit)));r.clipped=true;}if(!op.text.empty())clipped.push_back(std::move(op));
        }else if(op.kind==NaturalDrawOp::Kind::Cursor){if(op.x<left||op.x>right||op.y<top||op.y2>bottom){r.clipped=true;continue;}clipped.push_back(std::move(op));
        }else{
            double x0=op.x,y0=op.y,x1=op.x2,y1=op.y2;const double xmin=left,xmax=right,ymin=top,ymax=bottom;
            auto code=[&](double xx,double yy){return (xx<xmin?1:xx>xmax?2:0)|(yy<ymin?4:yy>ymax?8:0);};
            int c0=code(x0,y0),c1=code(x1,y1);bool accept=false;
            for(int guard=0;guard<8;++guard){if(!(c0|c1)){accept=true;break;}if(c0&c1)break;const int c=c0?c0:c1;double xx=0,yy=0;
                if(c&8){if(y1==y0)break;xx=x0+(x1-x0)*(ymax-y0)/(y1-y0);yy=ymax;}
                else if(c&4){if(y1==y0)break;xx=x0+(x1-x0)*(ymin-y0)/(y1-y0);yy=ymin;}
                else if(c&2){if(x1==x0)break;yy=y0+(y1-y0)*(xmax-x0)/(x1-x0);xx=xmax;}
                else {if(x1==x0)break;yy=y0+(y1-y0)*(xmin-x0)/(x1-x0);xx=xmin;}
                if(c==c0){x0=xx;y0=yy;c0=code(x0,y0);}else{x1=xx;y1=yy;c1=code(x1,y1);}
            }
            if(accept){if(x0!=op.x||y0!=op.y||x1!=op.x2||y1!=op.y2)r.clipped=true;op.x=static_cast<int>(std::lround(x0));op.y=static_cast<int>(std::lround(y0));op.x2=static_cast<int>(std::lround(x1));op.y2=static_cast<int>(std::lround(y1));clipped.push_back(std::move(op));}
            else r.clipped=true;
        }
    }r.ops=std::move(clipped);
}
}
Size measure(const NaturalNode& n) noexcept{return measureNode(n,0);}
Layout layout(const NaturalNode& n,int x,int baseline,int maxWidth,int maxHeight,Align align){
    Layout r;r.size=measure(n);r.width=std::max(0,maxWidth);r.height=std::max(0,maxHeight);r.top=baseline-r.size.baseline;
    if(maxWidth<=0||maxHeight<=0){r.clipped=true;return r;}
    int drawX=x;if(align==Align::Center)drawX+=std::max(0,(maxWidth-r.size.width)/2);else if(align==Align::Right)drawX+=std::max(0,maxWidth-r.size.width);
    r.x=drawX;drawNode(n,drawX,baseline,r.ops,0);
    const int right=x+maxWidth,bottom=r.top+maxHeight;
    if(r.size.width>maxWidth||r.size.height>maxHeight)r.clipped=true;
    clipOps(r,x,r.top,right,bottom);return r;
}
}
