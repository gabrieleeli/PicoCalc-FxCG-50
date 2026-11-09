#include "font_renderer.hpp"
#include <algorithm>
#include <cstddef>
namespace font { namespace {
const uint8_t* glyph(char c){
    static const uint8_t blank[5]={0,0,0,0,0};
    static const uint8_t digits[10][5]={
      {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
      {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
      {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
      {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
      {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E}};
    static const uint8_t letters[26][5]={
      {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
      {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
      {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
      {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
      {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
      {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
      {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
      {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
      {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
      {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
      {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
      {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
      {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}};
    static const uint8_t punct[][5]={
      {0,0,0,0,0}, {0,0,0x5F,0,0}, {0,0x07,0,0x07,0},
      {0x14,0x7F,0x14,0x7F,0x14}, {0x24,0x2A,0x7F,0x2A,0x12},
      {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50},
      {0,0x05,0x03,0,0}, {0,0x1C,0x22,0x41,0}, {0,0x41,0x22,0x1C,0},
      {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
      {0,0x50,0x30,0,0}, {0x08,0x08,0x08,0x08,0x08},
      {0,0x60,0x60,0,0}, {0x20,0x10,0x08,0x04,0x02},
      {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
      {0x00,0x00,0x60,0x60,0x00}, {0x08,0x14,0x22,0x41,0x00},
      {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
      {0x32,0x49,0x79,0x41,0x3E}, {0x00,0x06,0x09,0x09,0x06}};
    if(c>='0'&&c<='9')return digits[c-'0'];
    if(c>='A'&&c<='Z')return letters[c-'A'];
    if(c>='a'&&c<='z')return letters[c-'a'];
    switch(c){case ' ':return blank;case '!':return punct[1];case '"':return punct[2];case '#':return punct[3];case '$':return punct[4];case '%':return punct[5];case '&':return punct[6];case '\'':return punct[7];case '(':return punct[8];case ')':return punct[9];case '*':return punct[10];case '+':return punct[11];case ',':return punct[12];case '-':return punct[13];case '.':return punct[14];case '/':return punct[15];case ':':return punct[16];case ';':return punct[17];case '<':return punct[18];case '=':return punct[13];case '>':return punct[19];case '?':return punct[20];case '@':return punct[21];case '^':return punct[22];case '_':return punct[23];default:return blank;}
}
void drawGlyph(gfx::Canvas& canvas,int x,int y,char c,std::uint16_t color,int scale){
    const auto* g=glyph(c);
    for(int col=0;col<5;++col) for(int row=0;row<7;++row) if(g[col]&(1u<<row))
        canvas.rect(x+col*scale,y+row*scale,scale,scale,color,true);
}
}
std::uint32_t nextCodepoint(const unsigned char*& p) noexcept {
    const unsigned char c=*p++; if(c<0x80)return c;
    if((c&0xE0)==0xC0){const unsigned char b=*p; if((b&0xC0)==0x80){++p;return ((c&0x1F)<<6)|(b&0x3F);} return '?';}
    if((c&0xF0)==0xE0){if(!p[0]||!p[1])return '?';const unsigned char b=p[0],d=p[1];if((b&0xC0)==0x80&&(d&0xC0)==0x80){p+=2;return ((c&0x0F)<<12)|((b&0x3F)<<6)|(d&0x3F);}return '?';}
    if((c&0xF8)==0xF0){for(int i=0;i<3&&(*p&0xC0)==0x80;++i)++p;return '?';}
    return '?';
}
namespace {
const std::uint8_t* mathGlyph(std::uint32_t cp){
    static const std::uint8_t pi[5]={0x7F,0x40,0x40,0x40,0x7F};
    static const std::uint8_t root[5]={0x38,0x70,0x7F,0x01,0x01};
    static const std::uint8_t multiply[5]={0x22,0x14,0x08,0x14,0x22};
    static const std::uint8_t divide[5]={0x08,0x08,0x2A,0x08,0x08};
    static const std::uint8_t sigma[5]={0x7F,0x49,0x49,0x41,0x41};
    static const std::uint8_t theta[5]={0x3E,0x49,0x7F,0x49,0x3E};
    static const std::uint8_t integral[5]={0x00,0x41,0x7F,0x40,0x00};
    static const std::uint8_t infinity[5]={0x14,0x2A,0x55,0x2A,0x14};
    static const std::uint8_t le[5]={0x08,0x14,0x22,0x7F,0x08};
    static const std::uint8_t ge[5]={0x22,0x14,0x08,0x7F,0x08};
    static const std::uint8_t cbrt[5]={0x38,0x70,0x7F,0x01,0x01};
    static const std::uint8_t fourthRoot[5]={0x38,0x70,0x7F,0x01,0x01};
    switch(cp){case 0x03C0:return pi;case 0x221A:return root;case 0x00D7:return multiply;
    case 0x00F7:return divide;case 0x03A3:return sigma;case 0x03B8:return theta;
    case 0x222B:return integral;case 0x221E:return infinity;case 0x2264:return le;
    case 0x2265:return ge;case 0x221B:return cbrt;case 0x221C:return fourthRoot;default:return nullptr;}
}
char mapMath(std::uint32_t cp){
    switch(cp){case 0x221A:return 'V'; case 0x00D7:return 'x';
    case 0x00F7:return '/';case 0x2264:return '<';case 0x2265:return '>';
    case 0x221E:return '8';
    case 0x222B:return 'S';case 0x00B2:return '2';case 0x00B3:return '3';
    case 0x00B0:return 'o';case 0x2070:return '0';case 0x00B9:return '1';case 0x2074:return '4';case 0x2075:return '5';case 0x2076:return '6';case 0x2077:return '7';case 0x2078:return '8';case 0x2079:return '9';case 0x207A:return '+';case 0x207B:return '-';case 0x2090:return 'a';case 0x2091:return 'e';case 0x2080:return '0';case 0x2081:return '1';case 0x2082:return '2';case 0x2083:return '3';case 0x2084:return '4';case 0x2085:return '5';case 0x2086:return '6';case 0x2087:return '7';case 0x2088:return '8';case 0x2089:return '9';
    // Greek alphabet: compact 5x7 approximations keep the full alphabet legible on-device.
    case 0x03B1:return 'a';case 0x03B2:return 'b';case 0x03B3:return 'y';case 0x03B4:return 'd';case 0x03B5:return 'e';case 0x03B6:return 'z';case 0x03B7:return 'n';case 0x03B8:return 'O';case 0x03B9:return 'i';case 0x03BA:return 'k';case 0x03BB:return 'A';case 0x03BC:return 'u';case 0x03BD:return 'v';case 0x03BE:return 'E';case 0x03BF:return 'o';case 0x03C0:return 'P';case 0x03C1:return 'p';case 0x03C2:case 0x03C3:return 's';case 0x03C4:return 't';case 0x03C5:return 'u';case 0x03C6:return 'O';case 0x03C7:return 'x';case 0x03C8:return 'y';case 0x03C9:return 'w';
    case 0x0391:return 'A';case 0x0392:return 'B';case 0x0393:return 'G';case 0x0394:return 'D';case 0x0395:return 'E';case 0x0396:return 'Z';case 0x0397:return 'H';case 0x0398:return 'O';case 0x0399:return 'I';case 0x039A:return 'K';case 0x039B:return 'A';case 0x039C:return 'M';case 0x039D:return 'N';case 0x039E:return 'E';case 0x039F:return 'O';case 0x03A0:return 'P';case 0x03A1:return 'P';case 0x03A3:return 'E';case 0x03A4:return 'T';case 0x03A5:return 'Y';case 0x03A6:return 'O';case 0x03A7:return 'X';case 0x03A8:return 'Y';case 0x03A9:return 'W';
    default:return cp>=0x20&&cp<=0x7E?static_cast<char>(cp):'?';}
}
}
TextMetrics measure(const char* text,int scale) noexcept {
    if(!text) return {};
    scale=std::clamp(scale,1,3);
    int line=0,maxLine=0,lines=1;
    const auto* p=reinterpret_cast<const unsigned char*>(text);
    while(*p){std::uint32_t cp=nextCodepoint(p);if(cp=='\n'){maxLine=std::max(maxLine,line);line=0;++lines;}else line+=6*scale;}
    maxLine=std::max(maxLine,line);return {maxLine,lines*9*scale-2*scale};
}
TextMetrics drawText(gfx::Canvas& canvas,int x,int y,const char* text,std::uint16_t color,int scale,int maxWidth) noexcept {
    if(!text) return {};
    scale=std::clamp(scale,1,3);
    const int origin=x,limit=maxWidth>0?x+maxWidth:canvas.width();
    int maxLine=0,lines=1;
    const auto* p=reinterpret_cast<const unsigned char*>(text);
    while(*p){std::uint32_t cp=nextCodepoint(p);if(cp=='\n'){maxLine=std::max(maxLine,x-origin);x=origin;y+=9*scale;++lines;continue;}
        if(x+5*scale>limit) break;
        if(const auto* special=mathGlyph(cp)){
            for(int col=0;col<5;++col)for(int row=0;row<7;++row)if(special[col]&(1u<<row))
                canvas.rect(x+col*scale,y+row*scale,scale,scale,color,true);
        }else drawGlyph(canvas,x,y,mapMath(cp),color,scale);
        x+=6*scale;
    }
    maxLine=std::max(maxLine,x-origin);return {maxLine,lines*9*scale-2*scale};
}

namespace {
const BitmapFont* fontFor(Face face) noexcept {
    switch(face){case Face::Small:return &kSmall;case Face::Medium:return &kMedium;default:return nullptr;}
}
const std::uint8_t* lookup(const BitmapFont& f,std::uint32_t cp) noexcept {
    std::size_t lo=0,hi=f.count;
    while(lo<hi){const std::size_t mid=(lo+hi)/2;if(f.codepoints[mid]<cp)lo=mid+1;else hi=mid;}
    if(lo<f.count&&f.codepoints[lo]==cp)return f.bits+lo*static_cast<std::size_t>(f.height);
    return nullptr;
}
// Draws one code point in the requested face, falling back to the legacy 5x7
// math glyphs (and finally to '?') when the face has no glyph for it.
void drawCodepoint(gfx::Canvas& c,Face face,int x,int y,std::uint32_t cp,std::uint16_t color) noexcept {
    if(face==Face::Tiny){
        if(const auto* special=mathGlyph(cp)){for(int col=0;col<5;++col)for(int row=0;row<7;++row)if(special[col]&(1u<<row))c.pixel(x+col,y+row,color);}
        else drawGlyph(c,x,y,mapMath(cp),color,1);
        return;
    }
    const BitmapFont& f=*fontFor(face);
    const std::uint8_t* rows=lookup(f,cp);
    if(!rows&&face==Face::Small){
        // Legacy glyphs are 5x7: align their bottom row with the Small baseline.
        const int top=y+f.baseline-7;
        if(const auto* special=mathGlyph(cp)){for(int col=0;col<5;++col)for(int row=0;row<7;++row)if(special[col]&(1u<<row))c.pixel(x+col,top+row,color);return;}
        const char mapped=mapMath(cp);
        if(mapped!='?'||cp=='?'){rows=lookup(f,static_cast<unsigned char>(mapped));}
    }
    if(!rows)rows=lookup(f,'?');
    if(!rows)return;
    for(int r=0;r<f.height;++r){const std::uint8_t bits=rows[r];if(!bits)continue;
        int run=-1;
        for(int b=0;b<=f.width;++b){const bool on=b<f.width&&(bits&(0x80u>>b));
            if(on&&run<0)run=b;else if(!on&&run>=0){c.hLine(x+run,y+r,b-run,color);run=-1;}}
    }
}
}
std::size_t glyphCount(const char* text) noexcept {
    if(!text)return 0;
    std::size_t n=0;
    const auto* p=reinterpret_cast<const unsigned char*>(text);
    while(*p){(void)nextCodepoint(p);++n;}
    return n;
}
int advance(Face face) noexcept {const auto* f=fontFor(face);return f?f->width:6;}
int lineHeight(Face face) noexcept {const auto* f=fontFor(face);return f?f->height:9;}
int baseline(Face face) noexcept {const auto* f=fontFor(face);return f?f->baseline:7;}
bool hasGlyph(Face face,std::uint32_t cp) noexcept {
    if(face==Face::Tiny)return mathGlyph(cp)!=nullptr||(cp>=0x20&&cp<0x7F);
    return lookup(*fontFor(face),cp)!=nullptr;
}
TextMetrics measure(Face face,const char* text) noexcept {
    if(!text)return {};
    int line=0,maxLine=0,lines=1;const int adv=advance(face);
    const auto* p=reinterpret_cast<const unsigned char*>(text);
    while(*p){const std::uint32_t cp=nextCodepoint(p);if(cp=='\n'){maxLine=std::max(maxLine,line);line=0;++lines;}else line+=adv;}
    maxLine=std::max(maxLine,line);return {maxLine,lines*lineHeight(face)};
}
std::size_t drawClipped(gfx::Canvas& canvas,Face face,int x,int y,const char* text,std::uint16_t color,int maxWidth) noexcept {
    if(!text)return 0;
    const int adv=advance(face),origin=x,limit=maxWidth>0?x+maxWidth:canvas.width();
    std::size_t drawn=0;const auto* p=reinterpret_cast<const unsigned char*>(text);
    while(*p){const std::uint32_t cp=nextCodepoint(p);
        if(cp=='\n'){x=origin;y+=lineHeight(face);continue;}
        if(x+adv>limit)break;
        if(cp!=' ')drawCodepoint(canvas,face,x,y,cp,color);
        x+=adv;++drawn;}
    return drawn;
}
TextMetrics draw(gfx::Canvas& canvas,Face face,int x,int y,const char* text,std::uint16_t color,int maxWidth) noexcept {
    if(!text)return {};
    (void)drawClipped(canvas,face,x,y,text,color,maxWidth);
    const auto m=measure(face,text);
    return {maxWidth>0?std::min(m.width,maxWidth):m.width,m.height};
}
} // namespace font
