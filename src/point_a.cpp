#include "point_a.hpp"
#include "expression.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"
#include <new>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
namespace fx::pointa { namespace {
void shape(const Matrix&a){if(a.empty()||a[0].empty())throw std::invalid_argument("Matrice vuota");for(auto&r:a)if(r.size()!=a[0].size())throw std::invalid_argument("Matrice non rettangolare");}
void same(const Matrix&a,const Matrix&b){shape(a);shape(b);if(a.size()!=b.size()||a[0].size()!=b[0].size())throw std::invalid_argument("Dimensioni incompatibili");}
std::int64_t safeSub(std::int64_t a,std::int64_t b){if((b<0&&a>INT64_MAX+b)||(b>0&&a<INT64_MIN+b))throw std::overflow_error("Overflow frazione");return a-b;}
std::int64_t safeMul(std::int64_t a,std::int64_t b){if(a==0||b==0)return 0;if(a==-1&&b==INT64_MIN)throw std::overflow_error("Overflow frazione");if(b==-1&&a==INT64_MIN)throw std::overflow_error("Overflow frazione");if(a>0?(b>0?a>INT64_MAX/b:b<INT64_MIN/a):(b>0?a<INT64_MIN/b:a!=0&&b<INT64_MAX/a))throw std::overflow_error("Overflow frazione");return a*b;}
std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");if(a==s.npos)return {};return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
int dim(Unit u){switch(u){case Unit::Meter:case Unit::Kilometer:case Unit::Centimeter:case Unit::Millimeter:case Unit::Inch:case Unit::Foot:case Unit::Mile:return 1;case Unit::SquareMeter:case Unit::SquareCentimeter:case Unit::Hectare:case Unit::Acre:return 2;case Unit::CubicMeter:case Unit::Liter:case Unit::Milliliter:case Unit::GallonUS:return 3;case Unit::Second:case Unit::Minute:case Unit::Hour:case Unit::Day:return 4;case Unit::MeterPerSecond:case Unit::KilometerPerHour:case Unit::MilePerHour:return 5;case Unit::Kilogram:case Unit::Gram:case Unit::Milligram:case Unit::Pound:return 6;case Unit::Pascal:case Unit::Kilopascal:case Unit::Bar:case Unit::Atmosphere:return 7;case Unit::Joule:case Unit::Kilojoule:case Unit::Calorie:case Unit::Kilocalorie:return 8;case Unit::Watt:case Unit::Kilowatt:case Unit::Horsepower:return 9;default:return 0;}}
double scaleOf(Unit u){switch(u){case Unit::Meter:case Unit::SquareMeter:case Unit::CubicMeter:case Unit::Second:case Unit::MeterPerSecond:case Unit::Kilogram:case Unit::Pascal:case Unit::Joule:case Unit::Watt:return 1;case Unit::Kilometer:return 1e3;case Unit::Centimeter:return 1e-2;case Unit::Millimeter:return 1e-3;case Unit::Inch:return .0254;case Unit::Foot:return .3048;case Unit::Mile:return 1609.344;case Unit::SquareCentimeter:return 1e-4;case Unit::Hectare:return 1e4;case Unit::Acre:return 4046.8564224;case Unit::Liter:return 1e-3;case Unit::Milliliter:return 1e-6;case Unit::GallonUS:return .003785411784;case Unit::Minute:return 60;case Unit::Hour:return 3600;case Unit::Day:return 86400;case Unit::KilometerPerHour:return 1/3.6;case Unit::MilePerHour:return .44704;case Unit::Gram:return 1e-3;case Unit::Milligram:return 1e-6;case Unit::Pound:return .45359237;case Unit::Kilopascal:return 1e3;case Unit::Bar:return 1e5;case Unit::Atmosphere:return 101325;case Unit::Kilojoule:return 1e3;case Unit::Calorie:return 4.184;case Unit::Kilocalorie:return 4184;case Unit::Kilowatt:return 1e3;case Unit::Horsepower:return 745.6998715822702;default:return 1;}}
double toK(double v,Unit u){return u==Unit::Kelvin?v:u==Unit::Celsius?v+273.15:u==Unit::Fahrenheit?(v-32)*5/9+273.15:throw std::invalid_argument("Temperatura non valida");}
double fromK(double v,Unit u){return u==Unit::Kelvin?v:u==Unit::Celsius?v-273.15:u==Unit::Fahrenheit?(v-273.15)*9/5+32:throw std::invalid_argument("Temperatura non valida");}
}
double RunMatrixSession::execute(const std::string& expression){
 const double value=fx::evaluate(expression,memory_);
 // History is bulk data: on the Pico Plus 2 W it lives in PSRAM.
 mem::PlacementScope heavy(mem::Policy::PREFER_PSRAM);
 HistoryEntry entry{expression,value}; // may throw std::bad_alloc: nothing changed yet
 try{history_.push_back(std::move(entry));}
 catch(const std::bad_alloc&){
  // Growing the vector failed (entry left intact by the strong guarantee).
  // History is a convenience: drop the oldest quarter, then the push cannot
  // reallocate and therefore cannot fail.
  if(history_.empty())throw;
  history_.erase(history_.begin(),history_.begin()+static_cast<std::ptrdiff_t>(std::max<std::size_t>(1,history_.size()/4)));
  history_.push_back(std::move(entry));
 }
 memory_.setAnswer(value); // only after the entry is safely stored
 // Capacity: 30 entries on the Pico 2 W (fx-CG50 parity), up to 10000 with PSRAM,
 // lowered at runtime when the pools are short of memory.
 const std::size_t capacity=fx::limits::historyCapacity();
 if(history_.size()>capacity)history_.erase(history_.begin(),history_.begin()+static_cast<std::ptrdiff_t>(history_.size()-capacity));
 return value;
}
void RunMatrixSession::store(char variable,double value){memory_.store(variable,value);}
double RunMatrixSession::recall(char variable)const{return memory_.recall(variable);}
Matrix subtract(const Matrix&a,const Matrix&b){same(a,b);Matrix r=a;for(size_t i=0;i<r.size();++i)for(size_t j=0;j<r[0].size();++j)r[i][j]-=b[i][j];return r;}
Matrix scale(const Matrix&a,double k){shape(a);Matrix r=a;for(auto&row:r)for(double&v:row)v*=k;return r;}
Matrix transpose(const Matrix&a){shape(a);Matrix r(a[0].size(),std::vector<double>(a.size()));for(size_t i=0;i<a.size();++i)for(size_t j=0;j<a[0].size();++j)r[j][i]=a[i][j];return r;}
double trace(const Matrix&a){shape(a);if(a.size()!=a[0].size())throw std::invalid_argument("Traccia: matrice non quadrata");double s=0;for(size_t i=0;i<a.size();++i)s+=a[i][i];return s;}
Matrix rref(Matrix a,double eps){shape(a);size_t lead=0;for(size_t row=0;row<a.size()&&lead<a[0].size();++row){size_t p=row;for(size_t i=row+1;i<a.size();++i)if(std::abs(a[i][lead])>std::abs(a[p][lead]))p=i;if(std::abs(a[p][lead])<=eps){++lead;--row;continue;}std::swap(a[p],a[row]);double d=a[row][lead];for(double&v:a[row])v/=d;for(size_t i=0;i<a.size();++i)if(i!=row){double f=a[i][lead];for(size_t j=0;j<a[i].size();++j)a[i][j]-=f*a[row][j];}++lead;}for(auto&r:a)for(double&v:r)if(std::abs(v)<eps)v=0;return a;}
std::vector<double> solve(const Matrix&A,const std::vector<double>&b,double eps){shape(A);if(A.size()!=b.size()||A.size()!=A[0].size())throw std::invalid_argument("Sistema quadrato richiesto");Matrix m=A;for(size_t i=0;i<m.size();++i)m[i].push_back(b[i]);m=rref(m,eps);std::vector<double>x(A.size());for(size_t i=0;i<A.size();++i){size_t p=A.size();for(size_t j=0;j<A.size();++j)if(std::abs(m[i][j])>eps){p=j;break;}if(p==A.size()){if(std::abs(m[i].back())>eps)throw std::domain_error("Sistema incompatibile");throw std::domain_error("Sistema indeterminato");}x[p]=m[i].back();}return x;}
std::vector<double> vectorSubtract(const std::vector<double>&a,const std::vector<double>&b){if(a.size()!=b.size())throw std::invalid_argument("Dimensioni diverse");std::vector<double>r(a.size());for(size_t i=0;i<a.size();++i)r[i]=a[i]-b[i];return r;}
std::vector<double> vectorScale(const std::vector<double>&a,double k){auto r=a;for(auto&v:r)v*=k;return r;}
double vectorAngle(const std::vector<double>&a,const std::vector<double>&b){double na=vectorNorm(a),nb=vectorNorm(b);if(a.size()!=b.size()||a.empty()||na==0||nb==0)throw std::domain_error("Angolo vettoriale non definito");double c=std::max(-1.0,std::min(1.0,vectorDot(a,b)/(na*nb)));return fromRadians(std::acos(c));}
Fraction subtractFractions(Fraction a,Fraction b){auto n=safeSub(safeMul(a.numerator,b.denominator),safeMul(b.numerator,a.denominator));auto d=safeMul(a.denominator,b.denominator);return makeFraction(n,d);}
Fraction divideFractions(Fraction a,Fraction b){if(!b.numerator)throw std::domain_error("Divisione per zero");return multiplyFractions(a,makeFraction(b.denominator,b.numerator));}
Fraction decimalToFraction(double x,std::int64_t maxD,double tol){if(!std::isfinite(x)||maxD<1||tol<0)throw std::invalid_argument("Valore frazionario non valido");bool neg=x<0;x=std::abs(x);std::int64_t h0=0,h1=1,k0=1,k1=0;double z=x;for(int i=0;i<64;++i){double af=std::floor(z);if(af>9e18)break;auto a=static_cast<std::int64_t>(af);long double hn=static_cast<long double>(a)*h1+h0,kn=static_cast<long double>(a)*k1+k0;if(kn>maxD||hn>INT64_MAX)break;auto h=static_cast<std::int64_t>(hn),k=static_cast<std::int64_t>(kn);h0=h1;h1=h;k0=k1;k1=k;if(std::abs(static_cast<double>(h1)/k1-x)<=tol)break;double rem=z-af;if(rem<1e-18)break;z=1/rem;}return makeFraction(neg?-h1:h1,k1);}
std::string formatFraction(Fraction f,bool mixed){f=makeFraction(f.numerator,f.denominator);std::ostringstream o;const std::uint64_t mag=f.numerator<0?std::uint64_t(-(f.numerator+1))+1:std::uint64_t(f.numerator);if(mixed&&mag>=static_cast<std::uint64_t>(f.denominator)){const std::int64_t whole=f.numerator/f.denominator;const std::uint64_t rem=static_cast<std::uint64_t>(f.numerator%f.denominator<0?-(f.numerator%f.denominator):f.numerator%f.denominator);o<<whole;if(rem)o<<" "<<rem<<"/"<<f.denominator;}else o<<f.numerator<<"/"<<f.denominator;return o.str();}
Complex complexAdd(Complex a,Complex b){return a+b;}Complex complexSubtract(Complex a,Complex b){return a-b;}Complex complexMultiply(Complex a,Complex b){return a*b;}Complex complexDivide(Complex a,Complex b){if(std::abs(b)==0)throw std::domain_error("Divisione complessa per zero");return a/b;}Complex complexExp(Complex z){return std::exp(z);}Complex complexLog(Complex z){if(std::abs(z)==0)throw std::domain_error("Logaritmo complesso di zero");return std::log(z);}Complex complexSin(Complex z){return std::sin(z);}Complex complexCos(Complex z){return std::cos(z);}Complex complexTan(Complex z){return std::tan(z);}double complexArgument(Complex z){if(std::abs(z)==0)throw std::domain_error("Argomento di zero");return fromRadians(std::arg(z));}
std::string formatComplex(Complex z,unsigned p){std::ostringstream o;o<<std::setprecision(p)<<std::defaultfloat;double a=z.real(),b=z.imag();if(std::abs(a)<1e-14)a=0;if(std::abs(b)<1e-14)b=0;if(!b)o<<a;else{if(a)o<<a<<(b<0?"-":"+");else if(b<0)o<<"-";if(std::abs(b)!=1)o<<std::abs(b);o<<"i";}return o.str();}
std::string toBaseSigned(std::int64_t v,unsigned b){if(v>=0)return toBase((uint64_t)v,b);uint64_t n=uint64_t(-(v+1))+1;return "-"+toBase(n,b);}
std::int64_t fromBaseSigned(const std::string&s,unsigned b){auto t=trim(s);if(t.empty())throw std::invalid_argument("Numero vuoto");bool neg=t[0]=='-';size_t p=(neg||t[0]=='+')?1:0;if(p==t.size())throw std::invalid_argument("Numero non valido");uint64_t n=fromBase(t.substr(p),b);if(neg){if(n>uint64_t(INT64_MAX)+1)throw std::overflow_error("Overflow");return n==uint64_t(INT64_MAX)+1?INT64_MIN:-int64_t(n);}if(n>INT64_MAX)throw std::overflow_error("Overflow");return int64_t(n);}
uint64_t bitAnd(uint64_t a,uint64_t b){return a&b;}uint64_t bitOr(uint64_t a,uint64_t b){return a|b;}uint64_t bitXor(uint64_t a,uint64_t b){return a^b;}uint64_t bitNot(uint64_t a,unsigned w){if(!w||w>64)throw std::invalid_argument("Larghezza non valida");uint64_t m=w==64?UINT64_MAX:(uint64_t(1)<<w)-1;return ~a&m;}uint64_t shiftLeft(uint64_t a,unsigned n,unsigned w){if(!w||w>64||n>=w)throw std::invalid_argument("Shift non valido");uint64_t m=w==64?UINT64_MAX:(uint64_t(1)<<w)-1;return (a<<n)&m;}uint64_t shiftRight(uint64_t a,unsigned n,unsigned w){if(!w||w>64||n>=w)throw std::invalid_argument("Shift non valido");uint64_t m=w==64?UINT64_MAX:(uint64_t(1)<<w)-1;return (a&m)>>n;}
double derivative(const std::string&e,double x,double h){if(!(h>0))throw std::invalid_argument("Passo non valido");auto f=[&](double z){return fx::evaluate(e,z);};return (f(x-2*h)+8*f(x+h)-8*f(x-h)-f(x+2*h))/(12*h);}
double integrate(const std::string&e,double a,double b,unsigned n){if(n<2)n=2;if(n%2)++n;auto f=[&](double x){return fx::evaluate(e,x);};double h=(b-a)/n,s=f(a)+f(b);for(unsigned i=1;i<n;++i)s+=(i%2?4:2)*f(a+i*h);return s*h/3;}
std::vector<double> roots(const std::string&e,double lo,double hi,unsigned n,double tol){if(!(lo<hi)||!n)throw std::invalid_argument("Intervallo non valido");auto f=[&](double x){return fx::evaluate(e,x);};std::vector<double>r;double x0=lo,y0=f(x0);for(unsigned i=1;i<=n;++i){double x1=lo+(hi-lo)*i/n,y1=f(x1);if(y0==0){if(r.empty()||std::abs(r.back()-x0)>tol*10)r.push_back(x0);}else if(std::isfinite(y0)&&std::isfinite(y1)&&((y0<0&&y1>0)||(y0>0&&y1<0)||y1==0)){double a=x0,b=x1,fa=y0;for(int k=0;k<100&&b-a>tol;++k){double m=(a+b)/2,fm=f(m);if(fm==0){a=b=m;break;}if((fa<0&&fm>0)||(fa>0&&fm<0))b=m;else{a=m;fa=fm;}}double q=(a+b)/2;if(r.empty()||std::abs(r.back()-q)>tol*10)r.push_back(q);}x0=x1;y0=y1;}return r;}
double solve(const std::string&e,double x,unsigned n,double tol){auto f=[&](double z){return fx::evaluate(e,z);};for(unsigned i=0;i<n;++i){double y=f(x);if(std::abs(y)<=tol)return x;double d=derivative(e,x,1e-5);if(std::abs(d)<1e-14)throw std::domain_error("Derivata nulla in SOLVE");double nx=x-y/d;if(!std::isfinite(nx))throw std::domain_error("SOLVE non convergente");if(std::abs(nx-x)<=tol&&std::abs(f(nx))<=tol*10)return nx;x=nx;}throw std::runtime_error("SOLVE: iterazioni esaurite");}
double convert(double v,Unit a,Unit b){bool ta=a==Unit::Kelvin||a==Unit::Celsius||a==Unit::Fahrenheit,tb=b==Unit::Kelvin||b==Unit::Celsius||b==Unit::Fahrenheit;if(ta||tb){if(!ta||!tb)throw std::invalid_argument("Dimensioni incompatibili");return fromK(toK(v,a),b);}if(dim(a)!=dim(b))throw std::invalid_argument("Dimensioni incompatibili");return v*scaleOf(a)/scaleOf(b);}
double engineeringPrefix(const std::string&p){if(p=="da")return 1e1;if(p=="h")return 1e2;if(p=="k"||p=="K")return 1e3;if(p=="M")return 1e6;if(p=="G")return 1e9;if(p=="T")return 1e12;if(p=="d")return 1e-1;if(p=="c")return 1e-2;if(p=="m")return 1e-3;if(p=="u"||p=="µ")return 1e-6;if(p=="n")return 1e-9;if(p=="p")return 1e-12;if(p=="f")return 1e-15;throw std::invalid_argument("Prefisso SI sconosciuto");}
double physicalConstant(const std::string&n){if(n=="pi"||n=="PI")return M_PI;if(n=="e")return M_E;if(n=="c"||n=="C")return 299792458.;if(n=="G")return 6.67430e-11;if(n=="h")return 6.62607015e-34;if(n=="NA")return 6.02214076e23;if(n=="kB")return 1.380649e-23;if(n=="qe")return 1.602176634e-19;if(n=="g0")return 9.80665;throw std::invalid_argument("Costante non riconosciuta");}
std::string formatEngineering(double v,unsigned p){if(!std::isfinite(v))throw std::invalid_argument("Valore non finito");if(v==0)return "0";int e=int(std::floor(std::log10(std::abs(v))/3))*3;std::ostringstream o;o<<std::setprecision(p)<<std::defaultfloat<<v/std::pow(10.,e)<<"e"<<(e>=0?"+":"")<<e;return o.str();}
std::string formatNaturalExpression(const std::string&e){std::string o;for(size_t i=0;i<e.size();++i){if(e[i]=='*'&&i+1<e.size()&&e[i+1]=='*'){o+='^';++i;}else if(e[i]=='*')o+=" × ";else if(e[i]=='/')o+=" ÷ ";else o+=e[i];}return o;}
}
