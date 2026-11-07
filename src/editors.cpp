#include "editors.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
namespace fx {
void MatrixEditor::resize(std::size_t s,std::size_t r,std::size_t c){if(s>=Slots||r<1||r>MaxRows||c<1||c>MaxCols)throw std::out_of_range("Dimensioni matrice non valide (1..4)");matrices_[s].assign(r,std::vector<double>(c));}
void MatrixEditor::set(std::size_t s,std::size_t r,std::size_t c,double v){if(s>=Slots||r>=matrices_[s].size()||c>=matrices_[s][r].size())throw std::out_of_range("Cella matrice fuori intervallo");matrices_[s][r][c]=v;}
double MatrixEditor::get(std::size_t s,std::size_t r,std::size_t c)const{if(s>=Slots||r>=matrices_[s].size()||c>=matrices_[s][r].size())throw std::out_of_range("Cella matrice fuori intervallo");return matrices_[s][r][c];}
const Matrix& MatrixEditor::value(std::size_t s)const{if(s>=Slots||matrices_[s].empty())throw std::out_of_range("Matrice non definita");return matrices_[s];}
void MatrixEditor::clear(std::size_t s){if(s>=Slots)throw std::out_of_range("Slot matrice non valido");matrices_[s].clear();}
void StatisticsEditor::set(std::size_t l,std::size_t i,double v){if(l>=Lists||i>255)throw std::out_of_range("Lista o indice non valido");auto& a=lists_[l];if(i>=a.size())a.resize(i+1);a[i]=v;}
void StatisticsEditor::erase(std::size_t l,std::size_t i){if(l>=Lists||i>=lists_[l].size())throw std::out_of_range("Elemento lista non valido");lists_[l].erase(lists_[l].begin()+static_cast<std::ptrdiff_t>(i));}
const std::vector<double>& StatisticsEditor::list(std::size_t l)const{if(l>=Lists)throw std::out_of_range("Lista non valida");return lists_[l];}
DescriptiveStats StatisticsEditor::summary(std::size_t l)const{return describe(list(l));}
Regression StatisticsEditor::linearFit(std::size_t x,std::size_t y)const{return linearRegression(list(x),list(y));}
namespace {
double betaCF(double a,double b,double x){const int maxIt=200;const double eps=3e-14,fpmin=1e-300;double qab=a+b,qap=a+1,qam=a-1,c=1,d=1-qab*x/qap;if(std::abs(d)<fpmin)d=fpmin;d=1/d;double h=d;for(int m=1;m<=maxIt;++m){int m2=2*m;double aa=m*(b-m)*x/((qam+m2)*(a+m2));d=1+aa*d;if(std::abs(d)<fpmin)d=fpmin;c=1+aa/c;if(std::abs(c)<fpmin)c=fpmin;d=1/d;h*=d*c;aa=-(a+m)*(qab+m)*x/((a+m2)*(qap+m2));d=1+aa*d;if(std::abs(d)<fpmin)d=fpmin;c=1+aa/c;if(std::abs(c)<fpmin)c=fpmin;d=1/d;double del=d*c;h*=del;if(std::abs(del-1)<eps)break;}return h;}
double regBeta(double x,double a,double b){if(x<=0)return 0;if(x>=1)return 1;double bt=std::exp(std::lgamma(a+b)-std::lgamma(a)-std::lgamma(b)+a*std::log(x)+b*std::log1p(-x));if(x<(a+1)/(a+b+2))return bt*betaCF(a,b,x)/a;return 1-bt*betaCF(b,a,1-x)/b;}
double tCdf(double t,double df){double x=df/(df+t*t),ib=regBeta(x,df/2,0.5);return t>=0?1-0.5*ib:0.5*ib;}
double tQuantile(double p,double df){if(!(p>0&&p<1)||df<=0)throw std::invalid_argument("Quantile t non valido");double lo=-100,hi=100;for(int i=0;i<180;++i){double m=(lo+hi)/2;if(tCdf(m,df)<p)lo=m;else hi=m;}return (lo+hi)/2;}
TTestResult tFromDifferences(const std::vector<double>& d){if(d.size()<2)throw std::invalid_argument("Servono almeno due osservazioni");double m=mean(d),sd=stddev(d,true),se=sd/std::sqrt(static_cast<double>(d.size()));if(se==0)throw std::domain_error("Errore standard nullo");double t=m/se,df=static_cast<double>(d.size()-1);return {t,df,2*(1-tCdf(std::abs(t),df)),m,se};}
}
AnovaResult oneWayAnova(const std::vector<std::vector<double>>& g){if(g.size()<2)throw std::invalid_argument("ANOVA: servono almeno due gruppi");std::size_t n=0;double total=0;for(const auto& a:g){if(a.empty())throw std::invalid_argument("ANOVA: gruppo vuoto");n+=a.size();total+=std::accumulate(a.begin(),a.end(),0.0);}if(n<=g.size())throw std::invalid_argument("ANOVA: gradi di liberta insufficienti");double grand=total/n,ssb=0,ssw=0;for(const auto& a:g){double m=mean(a);ssb+=a.size()*(m-grand)*(m-grand);for(double v:a)ssw+=(v-m)*(v-m);}double dfb=g.size()-1,dfw=n-g.size(),msb=ssb/dfb,msw=ssw/dfw;if(msw==0)throw std::domain_error("ANOVA: varianza interna nulla");double f=msb/msw;double x=(dfb*f)/(dfb*f+dfw);double p=1-regBeta(x,dfb/2,dfw/2);return {ssb,ssw,dfb,dfw,msb,msw,f,p,g.size(),n};}
TTestResult oneSampleTTest(const std::vector<double>& s,double mu){auto d=s;for(double& v:d)v-=mu;return tFromDifferences(d);}
TTestResult pairedTTest(const std::vector<double>& a,const std::vector<double>& b){if(a.size()!=b.size())throw std::invalid_argument("Campioni appaiati: dimensioni diverse");std::vector<double>d(a.size());for(std::size_t i=0;i<a.size();++i)d[i]=a[i]-b[i];return tFromDifferences(d);}
TTestResult twoSampleTTest(const std::vector<double>& a,const std::vector<double>& b,bool equal){if(a.size()<2||b.size()<2)throw std::invalid_argument("Test t: servono almeno due dati per campione");double ma=mean(a),mb=mean(b),va=variance(a,true),vb=variance(b,true),na=static_cast<double>(a.size()),nb=static_cast<double>(b.size()),se2,df;if(equal){df=na+nb-2;double pooled=((na-1)*va+(nb-1)*vb)/df;se2=pooled*(1/na+1/nb);}else{double x=va/na,y=vb/nb;se2=x+y;df=se2*se2/(x*x/(na-1)+y*y/(nb-1));}if(se2<=0)throw std::domain_error("Test t: errore standard nullo");double t=(ma-mb)/std::sqrt(se2);return {t,df,2*(1-tCdf(std::abs(t),df)),ma-mb,std::sqrt(se2)};}
ConfidenceInterval meanConfidenceInterval(const std::vector<double>& s,double c){if(s.size()<2||!(c>0&&c<1))throw std::invalid_argument("Intervallo: campione o confidenza non validi");double alpha=(1-c)/2,crit=tQuantile(1-alpha,s.size()-1),se=stddev(s,true)/std::sqrt(static_cast<double>(s.size())),m=mean(s);return {m-crit*se,m+crit*se,crit};}
}
