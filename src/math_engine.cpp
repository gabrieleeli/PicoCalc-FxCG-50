#include "math_engine.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <numeric>
#include <sstream>
#include <cctype>
#include <cstdint>
namespace fx {
static AngleMode g_mode=AngleMode::Radians;
void setAngleMode(AngleMode m){g_mode=m;}
double toRadians(double x){ if(g_mode==AngleMode::Degrees)return x*M_PI/180.0; if(g_mode==AngleMode::Gradians)return x*M_PI/200.0; return x; }
double fromRadians(double x){ if(g_mode==AngleMode::Degrees)return x*180.0/M_PI; if(g_mode==AngleMode::Gradians)return x*200.0/M_PI; return x; }
double factorial(unsigned n){ if(n>170) throw std::overflow_error("n! troppo grande"); double r=1; for(unsigned i=2;i<=n;++i)r*=i; return r; }
unsigned long long nPr(unsigned n,unsigned r){ if(r>n) return 0; unsigned long long v=1; for(unsigned i=0;i<r;++i)v*=n-i; return v; }
unsigned long long nCr(unsigned n,unsigned r){ if(r>n)return 0; r=std::min(r,n-r); unsigned long long v=1; for(unsigned i=1;i<=r;++i)v=v*(n-r+i)/i; return v; }
QuadraticResult solveLinearEquation(double a,double b){ return solveQuadratic(0.0,a,b); }
QuadraticResult solveQuadratic(double a,double b,double c){ constexpr double e=1e-12; QuadraticResult q{}; if(std::abs(a)<e){ if(std::abs(b)<e){q.kind=std::abs(c)<e?QuadraticResult::Identity:QuadraticResult::Impossible;return q;} q.kind=QuadraticResult::Linear;q.x1=-c/b;return q;} q.delta=b*b-4*a*c; if(q.delta>e){double s=std::sqrt(q.delta), z=-.5*(b+std::copysign(s,b)); q.kind=QuadraticResult::TwoReal; if(std::abs(z)<e){q.x1=(-b+s)/(2*a);q.x2=(-b-s)/(2*a);}else{q.x1=z/a;q.x2=c/z;} if(q.x1>q.x2)std::swap(q.x1,q.x2);return q;} if(std::abs(q.delta)<=e){q.kind=QuadraticResult::Double;q.x1=-b/(2*a);return q;} q.kind=QuadraticResult::Complex;q.real=-b/(2*a);q.imag=std::sqrt(-q.delta)/(2*std::abs(a));return q; }
std::vector<SolutionInterval> solvePolynomialInequality(double a,double b,double c,InequalityRelation rel){
 const bool inclusive=(rel==InequalityRelation::LessEqual||rel==InequalityRelation::GreaterEqual);
 const bool wantPositive=(rel==InequalityRelation::Greater||rel==InequalityRelation::GreaterEqual);
 const double inf=std::numeric_limits<double>::infinity();
 auto accepts=[&](double v){return wantPositive?(inclusive?v>=-1e-12:v>1e-12):(inclusive?v<=1e-12:v< -1e-12);};
 constexpr double eps=1e-12;
 if(std::abs(a)<eps){
  if(std::abs(b)<eps)return accepts(c)?std::vector<SolutionInterval>{{-inf,inf,false,false}}:std::vector<SolutionInterval>{};
  double r=-c/b; bool left=accepts(b*(-1.0));
  if(left)return {{-inf,r,false,inclusive}};
  return {{r,inf,inclusive,false}};
 }
 auto q=solveQuadratic(a,b,c);
 if(q.kind==QuadraticResult::Complex){return accepts(a)?std::vector<SolutionInterval>{{-inf,inf,false,false}}:std::vector<SolutionInterval>{};}
 if(q.kind==QuadraticResult::Double){
  bool sides=accepts(a); if(sides){if(inclusive)return {{-inf,inf,false,false}};return {{-inf,q.x1,false,false},{q.x1,inf,false,false}};}
  if(inclusive)return {{q.x1,q.x1,true,true}};
  return {};
 }
 double r1=q.x1,r2=q.x2; bool left=accepts(a),middle=accepts(-a),right=left;
 std::vector<SolutionInterval> out;
 if(left)out.push_back({-inf,r1,false,inclusive});
 if(middle)out.push_back({r1,r2,inclusive,inclusive});
 if(right)out.push_back({r2,inf,inclusive,false});
 return out;
}
std::vector<SolutionInterval> unionSolutionSets(const std::vector<SolutionInterval>& a,
                                                const std::vector<SolutionInterval>& b){
 std::vector<SolutionInterval> v=a; v.insert(v.end(),b.begin(),b.end());
 for(const auto& x:v) if(x.lower>x.upper) throw std::invalid_argument("Intervallo non valido");
 std::sort(v.begin(),v.end(),[](const auto& x,const auto& y){
  if(x.lower!=y.lower)return x.lower<y.lower;
  return x.lowerInclusive&&!y.lowerInclusive;
 });
 std::vector<SolutionInterval> out;
 for(const auto& x:v){
  if(out.empty()){out.push_back(x);continue;}
  auto& z=out.back();
  const bool touches=x.lower<z.upper || (x.lower==z.upper&&(z.upperInclusive||x.lowerInclusive));
  if(!touches){out.push_back(x);continue;}
  if(x.upper>z.upper){z.upper=x.upper;z.upperInclusive=x.upperInclusive;}
  else if(x.upper==z.upper) z.upperInclusive=z.upperInclusive||x.upperInclusive;
 }
 return out;
}
std::vector<SolutionInterval> intersectSolutionSets(const std::vector<SolutionInterval>& a,
                                                    const std::vector<SolutionInterval>& b){
 std::vector<SolutionInterval> out;
 for(const auto& x:a) for(const auto& y:b){
  SolutionInterval z;
  if(x.lower>y.lower){z.lower=x.lower;z.lowerInclusive=x.lowerInclusive;}
  else if(y.lower>x.lower){z.lower=y.lower;z.lowerInclusive=y.lowerInclusive;}
  else {z.lower=x.lower;z.lowerInclusive=x.lowerInclusive&&y.lowerInclusive;}
  if(x.upper<y.upper){z.upper=x.upper;z.upperInclusive=x.upperInclusive;}
  else if(y.upper<x.upper){z.upper=y.upper;z.upperInclusive=y.upperInclusive;}
  else {z.upper=x.upper;z.upperInclusive=x.upperInclusive&&y.upperInclusive;}
  if(z.lower<z.upper || (z.lower==z.upper&&z.lowerInclusive&&z.upperInclusive))out.push_back(z);
 }
 return unionSolutionSets(out,{});
}
std::vector<SolutionInterval> solveInequalitySystem(const std::vector<std::array<double,4>>& inequalities){
 const auto inf=std::numeric_limits<double>::infinity();
 std::vector<SolutionInterval> result{{-inf,inf,false,false}};
 for(const auto& item:inequalities){
  const double raw=item[3];
  if(raw<0||raw>3||std::floor(raw)!=raw)throw std::invalid_argument("Relazione non valida: usare 0,1,2,3");
  auto next=solvePolynomialInequality(item[0],item[1],item[2],static_cast<InequalityRelation>(static_cast<int>(raw)));
  result=intersectSolutionSets(result,next);
  if(result.empty())break;
 }
 return result;
}
LinearResult solveLinearSystem(std::vector<std::vector<double>> A,std::vector<double>b,double eps){size_t n=A.size(); if(!n||b.size()!=n)throw std::invalid_argument("Dimensioni non valide"); for(auto&r:A)if(r.size()!=n)throw std::invalid_argument("Matrice deve essere quadrata"); for(size_t i=0;i<n;++i)A[i].push_back(b[i]); size_t rank=0; for(size_t c=0;c<n&&rank<n;++c){size_t p=rank;for(size_t r=rank+1;r<n;++r)if(std::abs(A[r][c])>std::abs(A[p][c]))p=r; if(std::abs(A[p][c])<eps)continue;std::swap(A[p],A[rank]);double d=A[rank][c];for(size_t j=c;j<=n;++j)A[rank][j]/=d;for(size_t r=0;r<n;++r)if(r!=rank){double f=A[r][c];for(size_t j=c;j<=n;++j)A[r][j]-=f*A[rank][j];}++rank;} for(auto&r:A){bool zero=true;for(size_t c=0;c<n;++c)zero&=std::abs(r[c])<eps;if(zero&&std::abs(r[n])>=eps)return {LinearResult::Inconsistent,{},rank};} if(rank<n)return {LinearResult::Dependent,{},rank};std::vector<double>x(n);for(size_t i=0;i<n;++i)x[i]=A[i][n];return {LinearResult::Unique,x,rank};}
double integrateSimpson(double(*f)(double),double a,double b,unsigned n){if(n<2)n=2;if(n%2)++n;double h=(b-a)/n,s=f(a)+f(b);for(unsigned i=1;i<n;++i)s+=(i%2?4:2)*f(a+i*h);return s*h/3;}
double bisectRoot(double(*f)(double),double a,double b,unsigned it){double fa=f(a),fb=f(b);if(fa==0)return a;if(fb==0)return b;if(fa*fb>0)throw std::invalid_argument("Intervallo senza cambio di segno");for(unsigned i=0;i<it;++i){double m=(a+b)/2,fm=f(m);if(std::abs(fm)<1e-12)return m;if(fa*fm<=0){b=m;fb=fm;}else{a=m;fa=fm;}}return(a+b)/2;}
double mean(const std::vector<double>&v){if(v.empty())throw std::invalid_argument("Lista vuota");double s=0;for(double x:v)s+=x;return s/v.size();}
double variance(const std::vector<double>&v,bool sample){if(v.size()<(sample?2u:1u))throw std::invalid_argument("Dati insufficienti");double m=mean(v),s=0;for(double x:v)s+=(x-m)*(x-m);return s/(v.size()-(sample?1:0));}
double stddev(const std::vector<double>&v,bool sample){return std::sqrt(variance(v,sample));}
Regression linearRegression(const std::vector<double>&x,const std::vector<double>&y){if(x.size()!=y.size()||x.size()<2)throw std::invalid_argument("Servono almeno due coppie");double mx=mean(x),my=mean(y),xx=0,xy=0,yy=0;for(size_t i=0;i<x.size();++i){xx+=(x[i]-mx)*(x[i]-mx);xy+=(x[i]-mx)*(y[i]-my);yy+=(y[i]-my)*(y[i]-my);}if(xx==0)throw std::invalid_argument("Ascisse costanti");return {xy/xx,my-(xy/xx)*mx,(xx*yy>0?xy/std::sqrt(xx*yy):0)};}
std::vector<double> vectorAdd(const std::vector<double>&a,const std::vector<double>&b){if(a.size()!=b.size())throw std::invalid_argument("Dimensioni diverse");std::vector<double>r(a.size());for(size_t i=0;i<a.size();++i)r[i]=a[i]+b[i];return r;}
double vectorDot(const std::vector<double>&a,const std::vector<double>&b){if(a.size()!=b.size())throw std::invalid_argument("Dimensioni diverse");double s=0;for(size_t i=0;i<a.size();++i)s+=a[i]*b[i];return s;}
std::vector<double> vectorCross3(const std::vector<double>&a,const std::vector<double>&b){if(a.size()!=3||b.size()!=3)throw std::invalid_argument("Servono vettori 3D");return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}

namespace {
std::int64_t fractionMulChecked(std::int64_t a,std::int64_t b){
 if(a==0||b==0)return 0;
 if((a==-1&&b==INT64_MIN)||(b==-1&&a==INT64_MIN))throw std::overflow_error("Overflow frazione");
 if(a>0?(b>0?a>INT64_MAX/b:b<INT64_MIN/a):(b>0?a<INT64_MIN/b:b<INT64_MAX/a))throw std::overflow_error("Overflow frazione");
 return a*b;
}
std::int64_t fractionAddChecked(std::int64_t a,std::int64_t b){if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))throw std::overflow_error("Overflow frazione");return a+b;}
std::uint64_t fractionMagnitude(std::int64_t x){return x<0?std::uint64_t(-(x+1))+1:std::uint64_t(x);}
}
Fraction makeFraction(std::int64_t n,std::int64_t d){
 if(d==0) throw std::invalid_argument("Denominatore nullo");
 if(d==INT64_MIN)throw std::overflow_error("Denominatore fuori intervallo");
 if(d<0){if(n==INT64_MIN)throw std::overflow_error("Numeratore fuori intervallo");n=-n;d=-d;}
 const std::uint64_t g=std::gcd(fractionMagnitude(n),static_cast<std::uint64_t>(d));
 return {n/static_cast<std::int64_t>(g),d/static_cast<std::int64_t>(g)};
}
Fraction addFractions(Fraction a,Fraction b){return makeFraction(fractionAddChecked(fractionMulChecked(a.numerator,b.denominator),fractionMulChecked(b.numerator,a.denominator)),fractionMulChecked(a.denominator,b.denominator));}
Fraction multiplyFractions(Fraction a,Fraction b){return makeFraction(fractionMulChecked(a.numerator,b.numerator),fractionMulChecked(a.denominator,b.denominator));}
double percent(double v,double p){return v*p/100.0;}
double reciprocal(double v){if(v==0)throw std::domain_error("Divisione per zero");return 1.0/v;}
double nthRoot(double v,int n){if(n==0)throw std::domain_error("Indice della radice nullo");if(v<0&&n%2==0)throw std::domain_error("Radice pari di numero negativo");double a=std::pow(std::abs(v),1.0/std::abs(n));if(v<0)a=-a;if(n<0){if(a==0)throw std::domain_error("Divisione per zero");a=1.0/a;}return a;}
double logBase(double v,double b){if(v<=0||b<=0||b==1)throw std::domain_error("Argomenti logaritmo non validi");return std::log(v)/std::log(b);}
std::pair<double,double> polarToRect(double r,double a){double t=toRadians(a);return {r*std::cos(t),r*std::sin(t)};}
std::pair<double,double> rectToPolar(double x,double y){double a=std::atan2(y,x);return {std::hypot(x,y),fromRadians(a)};}
Sexagesimal decimalToSexagesimal(double v){int sign=v<0?-1:1;double a=std::abs(v);int d=static_cast<int>(a);double m=(a-d)*60;int mi=static_cast<int>(m);double sec=(m-mi)*60;return {sign*d,mi,sec};}
double sexagesimalToDecimal(Sexagesimal v){double sign=v.degrees<0?-1.0:1.0;return sign*(std::abs(v.degrees)+v.minutes/60.0+v.seconds/3600.0);}
std::string toBase(std::uint64_t v,unsigned b){if(b<2||b>36)throw std::invalid_argument("Base deve essere tra 2 e 36");const char*d="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";if(!v)return "0";std::string s;while(v){s.push_back(d[v%b]);v/=b;}std::reverse(s.begin(),s.end());return s;}
std::uint64_t fromBase(const std::string&s,unsigned b){if(b<2||b>36||s.empty())throw std::invalid_argument("Base o numero non valido");std::uint64_t v=0;for(char c:s){char u=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));unsigned d=(u>='0'&&u<='9')?u-'0':(u>='A'&&u<='Z')?u-'A'+10:99;if(d>=b)throw std::invalid_argument("Cifra non valida per la base");if(v>(UINT64_MAX-d)/b)throw std::overflow_error("Numero fuori intervallo");v=v*b+d;}return v;}
std::vector<std::uint64_t> primeFactors(std::uint64_t n){if(n<2)throw std::invalid_argument("Inserire un intero >= 2");std::vector<std::uint64_t> f;while(n%2==0){f.push_back(2);n/=2;}for(std::uint64_t p=3;p<=n/p;p+=2)while(n%p==0){f.push_back(p);n/=p;}if(n>1)f.push_back(n);return f;}
static void checkMatrix(const Matrix&a){if(a.empty()||a[0].empty())throw std::invalid_argument("Matrice vuota");for(const auto&r:a)if(r.size()!=a[0].size())throw std::invalid_argument("Matrice irregolare");}
Matrix matrixAdd(const Matrix&a,const Matrix&b){checkMatrix(a);checkMatrix(b);if(a.size()!=b.size()||a[0].size()!=b[0].size())throw std::invalid_argument("Dimensioni incompatibili");Matrix r=a;for(size_t i=0;i<a.size();++i)for(size_t j=0;j<a[0].size();++j)r[i][j]+=b[i][j];return r;}
Matrix matrixMultiply(const Matrix&a,const Matrix&b){checkMatrix(a);checkMatrix(b);if(a[0].size()!=b.size())throw std::invalid_argument("Dimensioni incompatibili");Matrix r(a.size(),std::vector<double>(b[0].size()));for(size_t i=0;i<a.size();++i)for(size_t k=0;k<b.size();++k)for(size_t j=0;j<b[0].size();++j)r[i][j]+=a[i][k]*b[k][j];return r;}
double matrixDeterminant(Matrix a){checkMatrix(a);size_t n=a.size();if(a[0].size()!=n)throw std::invalid_argument("Serve una matrice quadrata");double det=1;for(size_t c=0;c<n;++c){size_t p=c;for(size_t r=c+1;r<n;++r)if(std::abs(a[r][c])>std::abs(a[p][c]))p=r;if(std::abs(a[p][c])<1e-12)return 0;if(p!=c){std::swap(a[p],a[c]);det=-det;}double pivot=a[c][c];det*=pivot;for(size_t r=c+1;r<n;++r){double f=a[r][c]/pivot;for(size_t j=c+1;j<n;++j)a[r][j]-=f*a[c][j];}}return det;}
Matrix matrixInverse(Matrix a){checkMatrix(a);size_t n=a.size();if(a[0].size()!=n)throw std::invalid_argument("Serve una matrice quadrata");Matrix inv(n,std::vector<double>(2*n));for(size_t i=0;i<n;++i){for(size_t j=0;j<n;++j)inv[i][j]=a[i][j];inv[i][n+i]=1;}for(size_t c=0;c<n;++c){size_t p=c;for(size_t r=c+1;r<n;++r)if(std::abs(inv[r][c])>std::abs(inv[p][c]))p=r;if(std::abs(inv[p][c])<1e-12)throw std::domain_error("Matrice singolare");std::swap(inv[p],inv[c]);double d=inv[c][c];for(double&x:inv[c])x/=d;for(size_t r=0;r<n;++r)if(r!=c){double f=inv[r][c];for(size_t j=0;j<2*n;++j)inv[r][j]-=f*inv[c][j];}}Matrix out(n,std::vector<double>(n));for(size_t i=0;i<n;++i)for(size_t j=0;j<n;++j)out[i][j]=inv[i][n+j];return out;}
double vectorNorm(const std::vector<double>&a){double s=0;for(double x:a)s+=x*x;return std::sqrt(s);}


Complex complexPower(Complex z,Complex w){return std::pow(z,w);}
Complex complexSqrt(Complex z){return std::sqrt(z);}
double derivativeCentral(double(*f)(double),double x,double h){if(!(h>0))throw std::invalid_argument("Passo derivata non valido");return (f(x+h)-f(x-h))/(2*h);}
double solveNewton(double(*f)(double),double x,unsigned it,double tol){for(unsigned i=0;i<it;++i){double y=f(x);if(std::abs(y)<=tol)return x;double d=derivativeCentral(f,x);if(std::abs(d)<1e-14)throw std::domain_error("Derivata nulla nel metodo SOLVE");double nx=x-y/d;if(!std::isfinite(nx))throw std::domain_error("Metodo SOLVE non convergente");if(std::abs(nx-x)<=tol)return nx;x=nx;}if(std::abs(f(x))>tol*10)throw std::runtime_error("Metodo SOLVE: iterazioni esaurite");return x;}
double median(std::vector<double> v){if(v.empty())throw std::invalid_argument("Lista vuota");std::sort(v.begin(),v.end());size_t n=v.size();return n%2?v[n/2]:(v[n/2-1]+v[n/2])/2.0;}
std::pair<double,double> quartiles(std::vector<double> v){if(v.size()<2)throw std::invalid_argument("Servono almeno due dati");std::sort(v.begin(),v.end());size_t n=v.size(),m=n/2;std::vector<double> lo(v.begin(),v.begin()+m),hi(v.begin()+(n%2?m+1:m),v.end());return {median(lo),median(hi)};}
DescriptiveStats describe(const std::vector<double>&v){if(v.empty())throw std::invalid_argument("Lista vuota");auto mm=std::minmax_element(v.begin(),v.end());auto q=quartiles(v);DescriptiveStats d;d.count=v.size();d.sum=std::accumulate(v.begin(),v.end(),0.0);d.mean=d.sum/v.size();d.minimum=*mm.first;d.maximum=*mm.second;d.median=median(v);d.q1=q.first;d.q3=q.second;d.population_sd=stddev(v,false);d.sample_sd=v.size()>1?stddev(v,true):0;return d;}
Regression polynomialRegression(const std::vector<double>&x,const std::vector<double>&y,unsigned degree,std::vector<double>*out){if(x.size()!=y.size()||x.size()<=degree||degree<1||degree>4)throw std::invalid_argument("Regressione: dati o grado non validi");size_t n=degree+1;Matrix a(n,std::vector<double>(n));std::vector<double>b(n);for(size_t r=0;r<n;++r){for(size_t c=0;c<n;++c)for(double xv:x)a[r][c]+=std::pow(xv,static_cast<int>(r+c));for(size_t i=0;i<x.size();++i)b[r]+=y[i]*std::pow(x[i],static_cast<int>(r));}auto sol=solveLinearSystem(a,b);if(sol.kind!=LinearResult::Unique)throw std::domain_error("Regressione singolare");if(out)*out=sol.solution;double ss=0,tot=0,my=mean(y);for(size_t i=0;i<x.size();++i){double yh=0;for(size_t j=0;j<n;++j)yh+=sol.solution[j]*std::pow(x[i],static_cast<int>(j));ss+=(y[i]-yh)*(y[i]-yh);tot+=(y[i]-my)*(y[i]-my);}double r2=tot>0?1-ss/tot:1;return {0,sol.solution[0],std::sqrt(std::max(0.0,r2))};}
double normalPdf(double x,double m,double s){if(!(s>0))throw std::invalid_argument("Deviazione standard non valida");double z=(x-m)/s;return std::exp(-0.5*z*z)/(s*std::sqrt(2*M_PI));}
double normalCdf(double x,double m,double s){if(!(s>0))throw std::invalid_argument("Deviazione standard non valida");return 0.5*std::erfc(-(x-m)/(s*std::sqrt(2.0)));}
double inverseNormalCdf(double p,double m,double s){if(!(p>0&&p<1)||!(s>0))throw std::invalid_argument("Probabilita o deviazione standard non valida");static const double a[]={-3.969683028665376e+01,2.209460984245205e+02,-2.759285104469687e+02,1.383577518672690e+02,-3.066479806614716e+01,2.506628277459239e+00};static const double b[]={-5.447609879822406e+01,1.615858368580409e+02,-1.556989798598866e+02,6.680131188771972e+01,-1.328068155288572e+01};static const double c[]={-7.784894002430293e-03,-3.223964580411365e-01,-2.400758277161838e+00,-2.549732539343734e+00,4.374664141464968e+00,2.938163982698783e+00};static const double d[]={7.784695709041462e-03,3.224671290700398e-01,2.445134137142996e+00,3.754408661907416e+00};double q,r,z;if(p<0.02425){q=std::sqrt(-2*std::log(p));z=(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5])/((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);}else if(p>1-0.02425){q=std::sqrt(-2*std::log(1-p));z=-(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5])/((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);}else{q=p-.5;r=q*q;z=(((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q/(((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1);}return m+s*z;}
double binomialPmf(unsigned n,unsigned k,double p){if(!(p>=0&&p<=1)||k>n) return 0;return static_cast<double>(nCr(n,k))*std::pow(p,static_cast<int>(k))*std::pow(1-p,static_cast<int>(n-k));}
double binomialCdf(unsigned n,unsigned k,double p){if(!(p>=0&&p<=1))throw std::invalid_argument("p deve essere tra 0 e 1");double s=0;for(unsigned i=0;i<=std::min(k,n);++i)s+=binomialPmf(n,i,p);return s;}
double poissonPmf(unsigned k,double l){if(l<0||!std::isfinite(l))throw std::invalid_argument("Lambda non valida");return std::exp(-l+k*std::log(l==0?1:l)-std::lgamma(k+1.0))*(l==0&&k>0?0:1);}
double poissonCdf(unsigned k,double l){if(l<0||!std::isfinite(l))throw std::invalid_argument("Lambda non valida");double s=0;for(unsigned i=0;i<=k;++i)s+=poissonPmf(i,l);return s;}
void Spreadsheet::set(size_t r,size_t c,double v){if(r>=Rows||c>=Cols)throw std::out_of_range("Cella fuori intervallo");cells[r][c]=v;}
double Spreadsheet::get(size_t r,size_t c)const{if(r>=Rows||c>=Cols)throw std::out_of_range("Cella fuori intervallo");return cells[r][c];}
double Spreadsheet::sum(size_t c,size_t first,size_t last)const{if(c>=Cols||first>last||last>=Rows)throw std::out_of_range("Intervallo foglio non valido");double s=0;for(size_t r=first;r<=last;++r)s+=cells[r][c];return s;}
double Spreadsheet::average(size_t c,size_t first,size_t last)const{return sum(c,first,last)/static_cast<double>(last-first+1);}

}
