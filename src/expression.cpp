#include "expression.hpp"
#include "math_engine.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace fx {
namespace {
class Parser {
 const std::string& s; size_t p=0; double x,y; const CalculatorMemory* memory=nullptr;
 void ws(){while(p<s.size()&&std::isspace(static_cast<unsigned char>(s[p])))++p;}
 bool eat(char c){ws();
   if(p<s.size()&&s[p]==c){++p;return true;}return false;}
 bool startsAtom(){ws();return p<s.size()&&(s[p]=='('||std::isalpha(static_cast<unsigned char>(s[p]))||std::isdigit(static_cast<unsigned char>(s[p]))||s[p]=='.');}
 double expr(){double v=term();for(;;){if(eat('+'))v+=term();else if(eat('-'))v-=term();else return v;}}
 double term(){double v=unary();for(;;){if(eat('*'))v*=unary();else if(eat('/')){double d=unary();
   if(d==0)throw std::domain_error("Divisione per zero");
   v/=d;}else if(eat('%')){double d=unary();
   if(d==0)throw std::domain_error("Modulo per zero");
   v=std::fmod(v,d);}else {size_t save=p;ws();
   bool implicit=p<s.size()&&(s[p]=='('||std::isalpha(static_cast<unsigned char>(s[p])));
   if(implicit)v*=unary();else {p=save;return v;}}}}
 double power(){double v=postfix();
   if(eat('^'))v=std::pow(v,unary());
  return v;}
 double unary(){if(eat('+'))return unary();
   if(eat('-'))return-unary();
  return power();}
 double postfix(){double v=atom();while(eat('!')){if(v<0||v>170||std::floor(v)!=v)throw std::domain_error("Fattoriale: serve un intero tra 0 e 170");v=factorial(static_cast<unsigned>(v));}return v;}
 double atom(){
  ws(); if(eat('(')){double v=expr();
   if(!eat(')'))throw std::runtime_error("Manca )");
   return v;}
  if(p<s.size()&&(std::isdigit(static_cast<unsigned char>(s[p]))||s[p]=='.')){char*e=nullptr;
   double v=std::strtod(s.c_str()+p,&e);
   if(e==s.c_str()+p)throw std::runtime_error("Numero non valido");
   p=static_cast<size_t>(e-s.c_str());return v;}
  if(p<s.size()&&std::isalpha(static_cast<unsigned char>(s[p]))){size_t b=p;while(p<s.size()&&std::isalnum(static_cast<unsigned char>(s[p])))++p;std::string n=s.substr(b,p-b);std::transform(n.begin(),n.end(),n.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
   if(n=="x")return x;
   if(n=="y")return y;
   if(n=="pi")return M_PI;
   if(n=="e")return M_E;
   if(memory){if(n=="ans")return memory->answer();if(n=="preans")return memory->previousAnswer();if(n=="m")return memory->independentMemory();if(n.size()==1&&n[0]>='a'&&n[0]<='z')return memory->recall(n[0]);}
   if(n=="tau")return 2.0*M_PI;
   if(n=="phi"||n=="goldenratio")return (1.0+std::sqrt(5.0))/2.0;
   if(n=="eulergamma")return 0.5772156649015328606;
   if(n=="sqrt2")return std::sqrt(2.0);
   if(n=="ln2")return std::log(2.0);
   if(n=="ln10")return std::log(10.0);
   if(n=="speedoflight"||n=="physc")return 299792458.0;
   if(n=="gravitationalconstant"||n=="physg")return 6.67430e-11;
   if(n=="planckconstant"||n=="physh")return 6.62607015e-34;
   if(n=="avogadro"||n=="na")return 6.02214076e23;
   if(n=="boltzmann"||n=="kb")return 1.380649e-23;
   if(n=="elementarycharge"||n=="qe")return 1.602176634e-19;
   if(n=="standardgravity"||n=="g0")return 9.80665;
   
   if(!eat('('))throw std::runtime_error("Funzione: usare parentesi");
   if(n=="integral"||n=="integrate"||n=="derivative"||n=="diff"||n=="sum"){
    const std::size_t begin=p;int depth=0;std::size_t comma=std::string::npos;
    for(std::size_t q=p;q<s.size();++q){if(s[q]=='(')++depth;else if(s[q]==')'){if(depth==0)break;--depth;}else if(s[q]==','&&depth==0){comma=q;break;}}
    if(comma==std::string::npos||comma==begin)throw std::runtime_error("Uso: integral(f(x),a,b), derivative(f(x),x) oppure sum(f(x),inizio,fine)");
    const std::string integrand=s.substr(begin,comma-begin);p=comma+1;
    const double first=expr();
    double lo=0.0,hi=first;
    if(n=="integral"||n=="integrate"||n=="sum"){
     if(!eat(','))throw std::runtime_error("Manca il secondo limite");
     hi=expr();
    }
    if(!eat(')'))throw std::runtime_error("Manca )");
    auto f=[&](double t){return Parser(integrand,t,y,memory).run();};
    if(n=="derivative"||n=="diff"){const double h=1e-5*std::max(1.0,std::abs(hi));return (f(hi+h)-f(hi-h))/(2*h);}
    if(n=="sum"){if(std::floor(first)!=first||std::floor(hi)!=hi)throw std::domain_error("Limiti della sommatoria: usare interi");if(std::abs(hi-first)>10000)throw std::domain_error("Sommatoria troppo ampia (massimo 10001 termini)");double total=0;const long long a=static_cast<long long>(first),b=static_cast<long long>(hi);if(a<=b){for(long long k=a;k<=b;++k)total+=f(static_cast<double>(k));}else{for(long long k=a;k>=b;--k)total+=f(static_cast<double>(k));}return total;}
    lo=first;
    constexpr unsigned intervals=400;const double step=(hi-lo)/intervals;
    double total=f(lo)+f(hi);for(unsigned i=1;i<intervals;++i)total+=(i%2?4.0:2.0)*f(lo+i*step);
    return total*step/3.0;
   }
   double a=expr();
   double bval=0;
   bool two=eat(',');
   if(two)bval=expr();
   if(!eat(')'))throw std::runtime_error("Manca )");
   if(two){if(n=="min")return std::min(a,bval);
   if(n=="frac"){if(bval==0)throw std::domain_error("Denominatore della frazione nullo");return a/bval;}
   if(n=="max")return std::max(a,bval);
   if(n=="log")return logBase(a,bval);
   if(n=="root"){if(a<2||a>99||std::floor(a)!=a)throw std::domain_error("Indice della radice: usare un intero tra 2 e 99");return nthRoot(bval,static_cast<int>(a));}
   if(n=="ncr"||n=="npr"){
    if(a<0||bval<0||std::floor(a)!=a||std::floor(bval)!=bval||a>100000||bval>100000)throw std::domain_error("nCr/nPr: servono interi non negativi");
    const auto nn=static_cast<unsigned>(a),rr=static_cast<unsigned>(bval);
    if(rr>nn)throw std::domain_error("nCr/nPr: r non puo superare n");
    return static_cast<double>(n=="ncr"?fx::nCr(nn,rr):fx::nPr(nn,rr));
   }
   if(n=="atan2")return fromRadians(std::atan2(a,bval));
   if(n=="hypot")return std::hypot(a,bval);
   if(n=="pow")return std::pow(a,bval);
   if(n=="mod")return std::fmod(a,bval);
   throw std::runtime_error("Funzione a due argomenti sconosciuta: "+n);}
   if(n=="sin")return std::sin(toRadians(a));
   if(n=="cos")return std::cos(toRadians(a));
   if(n=="tan")return std::tan(toRadians(a));
   if(n=="asin")return fromRadians(std::asin(a));
   if(n=="acos")return fromRadians(std::acos(a));
   if(n=="atan")return fromRadians(std::atan(a));
   if(n=="sinh")return std::sinh(a);
   if(n=="cosh")return std::cosh(a);
   if(n=="tanh")return std::tanh(a);
   if(n=="asinh")return std::asinh(a);
   if(n=="acosh")return std::acosh(a);
   if(n=="atanh")return std::atanh(a);
   if(n=="sqrt")return std::sqrt(a);
   if(n=="cbrt")return std::cbrt(a);
   if(n.size()>4 && n.compare(0,4,"root")==0){
    const std::string degree=n.substr(4);char* end=nullptr;long index=std::strtol(degree.c_str(),&end,10);
    if(end && *end=='\0' && index>=2 && index<=99)return nthRoot(a,static_cast<int>(index));
   }
   if(n=="abs")return std::abs(a);
   if(n=="ln")return std::log(a);
   if(n=="log")return std::log10(a);
   if(n=="log10")return std::log10(a);
   if(n=="exp")return std::exp(a);
   if(n=="log2")return std::log2(a);
   if(n=="floor")return std::floor(a);
   if(n=="ceil")return std::ceil(a);
   if(n=="round")return std::round(a);
   if(n=="trunc"||n=="int")return std::trunc(a);
   if(n=="frac")return a-std::trunc(a);
   if(n=="sign")return (a>0)-(a<0);
   if(n=="recip")return reciprocal(a);
   if(n=="percent")return a/100.0;
   if(n=="deg")return a*180.0/M_PI;
   if(n=="rad")return a*M_PI/180.0;
   if(n=="grad")return a*200.0/M_PI;
   if(n=="fact"||n=="factorial"){if(a<0||a>170||std::floor(a)!=a)throw std::domain_error("Argomento fattoriale non valido");return factorial(static_cast<unsigned>(a));}
   throw std::runtime_error("Funzione sconosciuta: "+n);
  }
  throw std::runtime_error("Espressione non valida");
 }
public: Parser(const std::string&v,double xv,double yv,const CalculatorMemory* mem=nullptr):s(v),x(xv),y(yv),memory(mem){} double run(){double v=expr();ws();
   if(p!=s.size())throw std::runtime_error("Caratteri extra");
   if(!std::isfinite(v))throw std::domain_error("Risultato non finito");
  return v;}
};

class AstParser {
 const std::string& s; std::size_t p{};
 void ws(){while(p<s.size()&&std::isspace(static_cast<unsigned char>(s[p])))++p;}
 bool eat(char c){ws();if(p<s.size()&&s[p]==c){++p;return true;}return false;}
 std::string identifier(){ws();std::size_t b=p;while(p<s.size()&&std::isalnum(static_cast<unsigned char>(s[p])))++p;if(b==p)throw std::runtime_error("Identificatore atteso");std::string n=s.substr(b,p-b);std::transform(n.begin(),n.end(),n.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return n;}
 ast::NodePtr binary(std::string op,ast::NodePtr a,ast::NodePtr b){auto n=ast::make(ast::Kind::Binary,std::move(op));n->children={std::move(a),std::move(b)};return n;}
 ast::NodePtr expression(){auto n=term();for(;;){if(eat('+'))n=binary("+",n,term());else if(eat('-'))n=binary("-",n,term());else return n;}}
 ast::NodePtr term(){auto n=unary();for(;;){if(eat('*'))n=binary("*",n,unary());else if(eat('/'))n=binary("/",n,unary());else if(eat('%'))n=binary("%",n,unary());else{ws();if(p<s.size()&&(s[p]=='('||std::isalpha(static_cast<unsigned char>(s[p]))))n=binary("*",n,unary());else return n;}}}
 ast::NodePtr unary(){if(eat('+')){auto n=ast::make(ast::Kind::Unary,"+");n->children={unary()};return n;}if(eat('-')){auto n=ast::make(ast::Kind::Unary,"-");n->children={unary()};return n;}return power();}
 ast::NodePtr power(){auto n=postfix();if(eat('^'))n=binary("^",n,unary());return n;}
 ast::NodePtr postfix(){auto n=atom();while(eat('!')){auto c=ast::make(ast::Kind::Call,"factorial");c->children={n};n=c;}return n;}
 ast::NodePtr atom(){ws();if(eat('(')){auto n=expression();if(!eat(')'))throw std::runtime_error("Manca )");auto g=ast::make(ast::Kind::Group);g->children={n};return g;}
  if(p<s.size()&&(std::isdigit(static_cast<unsigned char>(s[p]))||s[p]=='.')){char* e=nullptr;(void)std::strtod(s.c_str()+p,&e);if(e==s.c_str()+p)throw std::runtime_error("Numero non valido");auto n=ast::make(ast::Kind::Token,s.substr(p,static_cast<std::size_t>(e-(s.c_str()+p))));p=static_cast<std::size_t>(e-s.c_str());return n;}
  auto name=identifier();if(!eat('('))return ast::make(ast::Kind::Token,name);auto call=ast::make(ast::Kind::Call,name);if(!eat(')')){do{call->children.push_back(expression());}while(eat(','));if(!eat(')'))throw std::runtime_error("Manca ) nella funzione");}if(call->children.empty())throw std::runtime_error("Argomenti funzione mancanti");return call;
 }
public: explicit AstParser(const std::string& text):s(text){} ast::NodePtr run(){auto n=expression();ws();if(p!=s.size())throw std::runtime_error("Caratteri extra nell'espressione");return n;}
};
double evalAst(const ast::Node& n,double x,double y){using ast::Kind;auto ev=[&](std::size_t i){if(i>=n.children.size()||!n.children[i])throw std::runtime_error("Figlio AST mancante");return evalAst(*n.children[i],x,y);};
 if(!ast::validShape(n))throw std::runtime_error("Nodo AST non valido");
 switch(n.kind){case Kind::Token:{if(n.text.empty())throw std::runtime_error("Token vuoto");char* e=nullptr;double v=std::strtod(n.text.c_str(),&e);if(e==n.text.c_str()+n.text.size())return v;if(n.text=="x")return x;if(n.text=="y")return y;if(n.text=="pi")return M_PI;if(n.text=="e")return M_E;throw std::runtime_error("Variabile AST sconosciuta: "+n.text);}
 case Kind::Row:{if(n.children.empty())return 0.0;std::string tokenText;double value=0.0;bool haveValue=false;auto flush=[&](){if(!tokenText.empty()){value=evalAst(*parseExpression(tokenText),x,y);haveValue=true;tokenText.clear();}};for(const auto& c:n.children){if(!c)throw std::runtime_error("Figlio AST nullo");if(c->kind==Kind::Token)tokenText+=c->text;else{flush();value=evalAst(*c,x,y);haveValue=true;}}flush();return haveValue?value:0.0;}
 case Kind::Binary:{double a=ev(0),b=ev(1);if(n.text=="+")return a+b;if(n.text=="-")return a-b;if(n.text=="*")return a*b;if(n.text=="/"){if(b==0)throw std::domain_error("Divisione per zero");return a/b;}if(n.text=="%"){if(b==0)throw std::domain_error("Modulo per zero");return std::fmod(a,b);}if(n.text=="^")return std::pow(a,b);throw std::runtime_error("Operatore AST sconosciuto");}
 case Kind::Unary:{double a=ev(0);return n.text=="-"?-a:a;}
 case Kind::Fraction:{double b=ev(1);if(b==0)throw std::domain_error("Divisione per zero");return ev(0)/b;}
 case Kind::Power:return std::pow(ev(0),ev(1));case Kind::Group:return ev(0);
 case Kind::Radical:{double a=ev(0);return n.text.empty()||n.text=="2"?std::sqrt(a):nthRoot(a,std::stoi(n.text));}
 case Kind::Call:{double a=ev(0);if(n.text=="sqrt")return std::sqrt(a);if(n.text=="abs")return std::abs(a);if(n.text=="sin")return std::sin(toRadians(a));if(n.text=="cos")return std::cos(toRadians(a));if(n.text=="tan")return std::tan(toRadians(a));if(n.text=="ln")return std::log(a);if(n.text=="log")return std::log10(a);if(n.text=="exp")return std::exp(a);if(n.text=="factorial"){if(a<0||a>170||std::floor(a)!=a)throw std::domain_error("Fattoriale non valido");return factorial(static_cast<unsigned>(a));}if(n.text=="pow"&&n.children.size()==2)return std::pow(a,ev(1));throw std::runtime_error("Funzione AST non supportata: "+n.text);}
 default:throw std::runtime_error("Nodo AST non valutabile in modalita scalare");}
}
}
ast::NodePtr parseExpression(const std::string& expression){return AstParser(expression).run();}
double evaluate(const std::string&e,double x){return Parser(e,x,0.0).run();}
double evaluate(const ast::Node& e,double x){return evalAst(e,x,0.0);}
double evaluate(const std::string&e,double x,double y){return Parser(e,x,y).run();}
double evaluate(const std::string&e,const CalculatorMemory& m,double x,double y){return Parser(e,x,y,&m).run();}
double compose(const std::string&outer,const std::string&inner,double x){return evaluate(outer,evaluate(inner,x));}
double inverseValue(const std::string&e,double target,double lo,double hi,unsigned iterations){if(!(lo<hi))throw std::invalid_argument("Intervallo inversa non valido");auto f=[&](double v){return evaluate(e,v)-target;};
   double fl=f(lo),fh=f(hi);
   if(fl==0)return lo;
   if(fh==0)return hi;
   if(!std::isfinite(fl)||!std::isfinite(fh)||fl*fh>0)throw std::domain_error("Valore fuori dal ramo o funzione non monotona nell'intervallo");
 for(unsigned i=0;i<iterations;++i){double mid=lo+(hi-lo)*.5,fm=f(mid);
   if(!std::isfinite(fm))throw std::domain_error("Dominio non valido durante l'inversa");
   if(std::abs(fm)<1e-12)return mid;
   if((fl<0&&fm>0)||(fl>0&&fm<0)){hi=mid;fh=fm;}else{lo=mid;fl=fm;}}return lo+(hi-lo)*.5;}
}
