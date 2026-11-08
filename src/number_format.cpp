#include "number_format.hpp"
#include "math_engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace fx {
namespace {
void stripZeros(std::string& s){
    if(s.find('.')==std::string::npos)return;
    while(!s.empty()&&s.back()=='0')s.pop_back();
    if(!s.empty()&&s.back()=='.')s.pop_back();
}
// Calculator rounding: |v| is first reduced to the 15-digit decimal mantissa
// the fx-CG50 uses internally, then rounded half away from zero to the digits
// shown. printf would round the binary value half-to-even instead
// (0.125 -> "0.12", 2.675 -> "2.67"; the calculator shows 0.13 and 2.68).
void mantissa15(double v,std::string& digits,int& exponent){
    char buf[64];
    std::snprintf(buf,sizeof(buf),"%.14e",std::abs(v));
    const std::string s(buf);
    const auto e=s.find('e');
    exponent=std::atoi(s.c_str()+e+1);
    digits.clear();
    for(std::size_t i=0;i<e;++i)if(s[i]!='.')digits.push_back(s[i]);
}
// Keeps the first `keep` digits, rounding half up; returns true on carry out
// of the first digit (the digit string then starts with an extra '1').
bool roundKeep(std::string& d,std::size_t keep){
    if(keep>=d.size()){d.append(keep-d.size(),'0');return false;}
    const bool up=d[keep]>='5';
    d.resize(keep);
    if(!up)return false;
    for(std::size_t i=keep;i-->0;){if(d[i]=='9')d[i]='0';else{++d[i];return false;}}
    d.insert(d.begin(),'1');
    return true;
}
// Splits |v| rounded to `sig` significant digits into digit string and exponent.
void sigDigits(double v,int sig,std::string& digits,int& exponent){
    mantissa15(v,digits,exponent);
    if(roundKeep(digits,static_cast<std::size_t>(sig))){digits.pop_back();++exponent;}
}
// |v| with exactly `decimals` digits after the point (Fix), calculator rounding.
std::string fixedText(double v,int decimals){
    std::string d;int e=0;
    mantissa15(v,d,e);
    const int keep=e+1+decimals;
    if(keep<=0){
        // Below the last shown decimal: 0, or one unit of it when the value reaches half of it.
        const bool up=keep==0&&d[0]>='5';
        d=up?"1":"";e=up?-decimals:0;
        if(!up){std::string z="0";if(decimals>0)z+="."+std::string(static_cast<std::size_t>(decimals),'0');return z;}
    }else if(roundKeep(d,static_cast<std::size_t>(keep))){++e;}
    std::string intPart,frac;
    if(e>=0){
        const std::size_t n=static_cast<std::size_t>(e+1);
        intPart=d.substr(0,std::min(n,d.size()));
        if(intPart.size()<n)intPart.append(n-intPart.size(),'0');
        frac=d.size()>n?d.substr(n):std::string();
    }else{
        intPart="0";
        frac=std::string(static_cast<std::size_t>(-e-1),'0')+d;
    }
    if(frac.size()<static_cast<std::size_t>(decimals))frac.append(static_cast<std::size_t>(decimals)-frac.size(),'0');
    frac.resize(static_cast<std::size_t>(decimals));
    return decimals>0?intPart+"."+frac:intPart;
}
std::string sciMantissa(const std::string& digits,bool strip){
    std::string m=digits.substr(0,1);
    if(digits.size()>1){m+=".";m+=digits.substr(1);}
    if(strip)stripZeros(m);
    return m;
}
} // namespace

std::string FormattedNumber::text() const {
    if(!scientific)return mantissa;
    char e[16];std::snprintf(e,sizeof(e),"E%c%02d",exponent<0?'-':'+',std::abs(exponent));
    return mantissa+e;
}

FormattedNumber formatNumber(double v,DisplayFormat f){
    FormattedNumber out;
    if(std::isnan(v)){out.mantissa="NaN";return out;}
    if(std::isinf(v)){out.mantissa=v<0?"-Inf":"Inf";return out;}
    const std::string sign=v<0?"-":"";
    if(v==0){
        if(f.mode==DisplayMode::Fix&&f.digits>0){out.mantissa="0."+std::string(f.digits,'0');}
        else if(f.mode==DisplayMode::Sci){const int sig=f.digits==0?10:std::clamp<int>(f.digits,1,10);out.mantissa=sig>1?"0."+std::string(static_cast<std::size_t>(sig-1),'0'):"0";out.scientific=true;}
        else out.mantissa="0";
        return out;
    }
    std::string digits;int e=0;
    switch(f.mode){
    case DisplayMode::Sci:{
        const int sig=f.digits==0?10:std::clamp<int>(f.digits,1,10);
        sigDigits(v,sig,digits,e);
        out.mantissa=sign+sciMantissa(digits,false);out.exponent=e;out.scientific=true;return out;
    }
    case DisplayMode::Eng:{
        sigDigits(v,10,digits,e);
        const int e3=static_cast<int>(std::floor(static_cast<double>(e)/3.0))*3;
        const int intDigits=e-e3+1;            // 1..3
        std::string m=digits.substr(0,static_cast<std::size_t>(intDigits))+"."+digits.substr(static_cast<std::size_t>(intDigits));
        stripZeros(m);
        out.mantissa=sign+m;out.exponent=e3;out.scientific=true;return out;
    }
    case DisplayMode::Fix:{
        if(std::abs(v)>=1e10){sigDigits(v,10,digits,e);out.mantissa=sign+sciMantissa(digits,true);out.exponent=e;out.scientific=true;return out;}
        std::string m=fixedText(v,static_cast<int>(std::min<std::uint8_t>(f.digits,9)));
        // Avoid "-0.00" for tiny negatives.
        bool allZero=true;for(char c:m)if(c!='0'&&c!='.')allZero=false;
        out.mantissa=(allZero?"":sign)+m;return out;
    }
    default:break;
    }
    sigDigits(v,10,digits,e);
    const bool sci=e>=10||(f.mode==DisplayMode::Norm1?e< -2:e< -9);
    if(sci){out.mantissa=sign+sciMantissa(digits,true);out.exponent=e;out.scientific=true;return out;}
    std::string m;
    if(e>=0){
        const std::size_t intLen=static_cast<std::size_t>(e+1);
        m=digits.substr(0,intLen);
        if(digits.size()>intLen)m+="."+digits.substr(intLen);
    }else{
        m="0."+std::string(static_cast<std::size_t>(-e-1),'0')+digits;
    }
    stripZeros(m);
    out.mantissa=sign+m;
    return out;
}

std::string formatReal(double v,DisplayFormat f){return formatNumber(v,f).text();}

std::string formatValue(Complex z,ComplexMode mode,DisplayFormat f){
    const double re=z.real(),im=z.imag();
    if(mode==ComplexMode::Polar&&im!=0){
        const double r=std::abs(z),theta=fromRadians(std::arg(z));
        return formatReal(r,f)+"\xE2\x88\xA0"+formatReal(theta,f);   // U+2220 angle
    }
    if(im==0)return formatReal(re,f);
    std::string out;
    if(re!=0)out=formatReal(re,f);
    const double a=std::abs(im);
    if(re!=0)out+=im<0?"-":"+";else if(im<0)out+="-";
    if(a!=1)out+=formatReal(a,f);
    out+="i";
    return out;
}

bool exactFraction(double v,long long& p,long long& q,long long maxQ) noexcept{
    if(!std::isfinite(v)||std::abs(v)>=1e10)return false;
    const bool neg=v<0;const double x=std::abs(v);
    long long h0=0,h1=1,k0=1,k1=0;double z=x;
    for(int i=0;i<48;++i){
        const double a=std::floor(z);
        if(a>1e12)break;
        const long long ai=static_cast<long long>(a);
        const long long h2=ai*h1+h0,k2=ai*k1+k0;
        if(k2>maxQ)break;
        h0=h1;h1=h2;k0=k1;k1=k2;
        if(std::abs(static_cast<double>(h1)/static_cast<double>(k1)-x)<=1e-11*std::max(1.0,x))break;
        const double rem=z-a;if(rem<1e-13)break;z=1.0/rem;
    }
    if(k1<=0)return false;
    if(std::abs(static_cast<double>(h1)/static_cast<double>(k1)-x)>1e-11*std::max(1.0,x))return false;
    p=neg?-h1:h1;q=k1;return true;
}

const char* displayModeName(DisplayFormat f) noexcept{
    static const char* fix[]={"Fix0","Fix1","Fix2","Fix3","Fix4","Fix5","Fix6","Fix7","Fix8","Fix9"};
    static const char* sci[]={"Sci10","Sci1","Sci2","Sci3","Sci4","Sci5","Sci6","Sci7","Sci8","Sci9","Sci10"};
    switch(f.mode){
    case DisplayMode::Norm1:return "Norm1";
    case DisplayMode::Norm2:return "Norm2";
    case DisplayMode::Fix:return fix[std::min<std::uint8_t>(f.digits,9)];
    case DisplayMode::Sci:return sci[std::min<std::uint8_t>(f.digits,10)];
    case DisplayMode::Eng:return "Eng";
    }
    return "?";
}
} // namespace fx
