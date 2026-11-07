// Single expression pipeline: UTF-8 text -> tokens -> canonical AST -> evaluator.
// The evaluator is a template over double (Real mode) and std::complex<double>
// (a+bi and r<theta modes), so complex numbers are part of the parser/AST rather
// than a separate API. Operator priority follows the fx-CG50 manual: implicit
// multiplication binds tighter than * and /, prefix functions without
// parentheses take an implicit-product argument (sin 2x = sin(2x)).
#include "expression.hpp"
#include "math_engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

namespace fx {
namespace {
constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- names
struct FnInfo { const char* name; std::uint8_t minArgs, maxArgs; bool lazy; };
constexpr FnInfo kFunctions[] = {
    {"sin",1,1,false},{"cos",1,1,false},{"tan",1,1,false},{"asin",1,1,false},{"acos",1,1,false},
    {"atan",1,1,false},{"sinh",1,1,false},{"cosh",1,1,false},{"tanh",1,1,false},{"asinh",1,1,false},
    {"acosh",1,1,false},{"atanh",1,1,false},{"sqrt",1,1,false},{"cbrt",1,1,false},{"abs",1,1,false},
    {"ln",1,1,false},{"log",1,2,false},{"log2",1,1,false},{"log10",1,1,false},{"exp",1,1,false},
    {"floor",1,1,false},{"ceil",1,1,false},{"round",1,2,false},{"trunc",1,1,false},{"int",1,1,false},
    {"intg",1,1,false},{"frac",1,2,false},{"sign",1,1,false},{"recip",1,1,false},{"percent",1,1,false},
    {"deg",1,1,false},{"rad",1,1,false},{"grad",1,1,false},{"fact",1,1,false},{"factorial",1,1,false},
    {"re",1,1,false},{"im",1,1,false},{"arg",1,1,false},{"conj",1,1,false},{"real",1,1,false},
    {"imag",1,1,false},{"min",2,2,false},{"max",2,2,false},{"root",2,2,false},{"ncr",2,2,false},
    {"npr",2,2,false},{"atan2",2,2,false},{"hypot",2,2,false},{"pow",2,2,false},{"mod",2,2,false},
    {"gcd",2,2,false},{"lcm",2,2,false},{"randint",2,2,false},{"rand",0,0,false},{"ran",0,0,false},
    {"dms",1,3,false},{"pol",2,2,false},{"rec",2,2,false},
    {"integral",3,3,true},{"integrate",3,3,true},{"derivative",2,2,true},{"diff",2,2,true},
    {"derivative2",2,2,true},{"sum",3,3,true},{"prod",3,3,true},{"solve",2,2,true},
};
struct ConstInfo { const char* name; double value; };
const ConstInfo kConstants[] = {
    {"pi",kPi},{"tau",2*kPi},{"phi",1.6180339887498948482},{"goldenratio",1.6180339887498948482},
    {"eulergamma",0.5772156649015328606},{"sqrt2",1.4142135623730950488},{"ln2",0.69314718055994530942},
    {"ln10",2.30258509299404568402},{"speedoflight",299792458.0},{"physc",299792458.0},
    {"gravitationalconstant",6.67430e-11},{"physg",6.67430e-11},{"planckconstant",6.62607015e-34},
    {"physh",6.62607015e-34},{"avogadro",6.02214076e23},{"na",6.02214076e23},{"boltzmann",1.380649e-23},
    {"kb",1.380649e-23},{"elementarycharge",1.602176634e-19},{"qe",1.602176634e-19},
    {"standardgravity",9.80665},{"g0",9.80665},
};
// Names resolved at evaluation time (not constants): memory values and bindings.
constexpr const char* kSpecialNames[] = {"ans","preans","theta"};

std::string lower(std::string s){for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
bool isRootN(const std::string& n){
    if(n.size()<5||n.compare(0,4,"root")!=0)return false;
    for(std::size_t i=4;i<n.size();++i)if(!std::isdigit(static_cast<unsigned char>(n[i])))return false;
    const long v=std::strtol(n.c_str()+4,nullptr,10);return v>=2&&v<=99;
}
const FnInfo* findFunction(const std::string& lowerName){
    for(const auto& f:kFunctions)if(lowerName==f.name)return &f;
    return nullptr;
}
bool isFunctionName(const std::string& lowerName){return findFunction(lowerName)||isRootN(lowerName);}
bool isConstantName(const std::string& lowerName){
    for(const auto& c:kConstants)if(lowerName==c.name)return true;
    for(const char* s:kSpecialNames)if(lowerName==s)return true;
    return false;
}

// ---------------------------------------------------------------- lexer
enum class TT { Num, Ident, Op, End };
struct Tok { TT type{TT::End}; std::string text; char op{}; };

class Lexer {
public:
    explicit Lexer(const std::string& s):s_(s){}
    std::vector<Tok> run(){
        std::vector<Tok> out;
        while(true){
            skipSpace();
            if(p_>=s_.size()){out.push_back({TT::End,{},0});return out;}
            const unsigned char c=static_cast<unsigned char>(s_[p_]);
            if(std::isdigit(c)||(c=='.'&&p_+1<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[p_+1])))){out.push_back(number());continue;}
            if(std::isalpha(c)||c=='_'){identifier(out);continue;}
            if(c>=0x80){utf8(out);continue;}
            if(std::strchr("+-*/%^!,()",static_cast<int>(c))){out.push_back({TT::Op,{},static_cast<char>(c)});++p_;continue;}
            throw SyntaxError(std::string("Syntax ERROR: carattere non valido '")+static_cast<char>(c)+"'");
        }
    }
private:
    const std::string& s_; std::size_t p_{};
    void skipSpace(){while(p_<s_.size()&&std::isspace(static_cast<unsigned char>(s_[p_])))++p_;}
    Tok number(){
        const std::size_t b=p_;
        if(s_[p_]=='0'&&p_+2<s_.size()&&(s_[p_+1]=='x'||s_[p_+1]=='X'||s_[p_+1]=='b'||s_[p_+1]=='B'||s_[p_+1]=='o'||s_[p_+1]=='O')){
            const char kind=static_cast<char>(std::tolower(static_cast<unsigned char>(s_[p_+1])));
            const unsigned base=kind=='x'?16:kind=='b'?2:8;
            std::size_t q=p_+2,start=q;
            auto digitOk=[&](char ch){const int d=std::isdigit(static_cast<unsigned char>(ch))?ch-'0':std::isalpha(static_cast<unsigned char>(ch))?std::tolower(static_cast<unsigned char>(ch))-'a'+10:99;return d<static_cast<int>(base);};
            while(q<s_.size()&&digitOk(s_[q]))++q;
            if(q>start){p_=q;const std::uint64_t v=fromBase(s_.substr(start,q-start),base);return {TT::Num,std::to_string(v),0};}
        }
        while(p_<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[p_])))++p_;
        if(p_<s_.size()&&s_[p_]=='.'){++p_;while(p_<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[p_])))++p_;}
        if(p_<s_.size()&&(s_[p_]=='e'||s_[p_]=='E')){
            std::size_t q=p_+1;if(q<s_.size()&&(s_[q]=='+'||s_[q]=='-'))++q;
            if(q<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[q]))){p_=q;while(p_<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[p_])))++p_;}
        }
        std::string t=s_.substr(b,p_-b);
        if(std::count(t.begin(),t.end(),'.')>1)throw SyntaxError("Syntax ERROR: numero non valido");
        return {TT::Num,t,0};
    }
    void identifier(std::vector<Tok>& out){
        const std::size_t b=p_;
        while(p_<s_.size()&&(std::isalnum(static_cast<unsigned char>(s_[p_]))||s_[p_]=='_'))++p_;
        const std::string raw=s_.substr(b,p_-b);
        splitIdentifier(raw,out);
    }
    // Resolves an identifier, splitting e.g. "sinx" -> sin x, "2pix" -> pi x, "ab" -> a b.
    static void splitIdentifier(const std::string& raw,std::vector<Tok>& out){
        const std::string low=lower(raw);
        if(raw.size()==1||isFunctionName(low)||isConstantName(low)){out.push_back({TT::Ident,raw.size()==1?raw:low,0});return;}
        std::size_t k=0;
        while(k<raw.size()){
            std::size_t best=0;
            for(std::size_t len=raw.size()-k;len>=2;--len){
                const std::string piece=lower(raw.substr(k,len));
                if(isFunctionName(piece)||isConstantName(piece)){best=len;break;}
            }
            if(best){out.push_back({TT::Ident,lower(raw.substr(k,best)),0});k+=best;continue;}
            if(std::isdigit(static_cast<unsigned char>(raw[k]))){
                std::size_t e=k;while(e<raw.size()&&std::isdigit(static_cast<unsigned char>(raw[e])))++e;
                out.push_back({TT::Num,raw.substr(k,e-k),0});k=e;continue;
            }
            if(!std::isalpha(static_cast<unsigned char>(raw[k])))throw SyntaxError("Syntax ERROR: nome non valido '"+raw+"'");
            out.push_back({TT::Ident,raw.substr(k,1),0});++k;
        }
    }
    void utf8(std::vector<Tok>& out){
        struct Map{const char* seq;TT type;char op;const char* name;};
        static const Map maps[]={
            {"\xCF\x80",TT::Ident,0,"pi"},{"\xCE\xB8",TT::Ident,0,"theta"},{"\xC3\x97",TT::Op,'*',nullptr},
            {"\xC3\xB7",TT::Op,'/',nullptr},{"\xE2\x88\x92",TT::Op,'-',nullptr},{"\xC2\xB0",TT::Op,'d',nullptr},
            {"\xE2\x88\x9A",TT::Ident,0,"sqrt"},{"\xE2\x88\x9B",TT::Ident,0,"cbrt"},{"\xE2\x8B\x85",TT::Op,'*',nullptr},
            {"\xC2\xB7",TT::Op,'*',nullptr},
        };
        for(const auto& m:maps){const std::size_t n=std::strlen(m.seq);
            if(s_.compare(p_,n,m.seq)==0){p_+=n;out.push_back({m.type,m.name?m.name:"",m.op});return;}}
        if(s_.compare(p_,3,"\xE2\x86\x92")==0)throw SyntaxError("Syntax ERROR: -> ammesso solo a fine espressione");
        throw SyntaxError("Syntax ERROR: simbolo non supportato");
    }
};

// ---------------------------------------------------------------- parser
class Parser {
public:
    explicit Parser(std::vector<Tok> t):t_(std::move(t)){}
    ast::NodePtr run(){
        if(t_.size()==1)throw SyntaxError("Syntax ERROR: espressione vuota");
        auto n=addSub();
        if(peek().type!=TT::End){
            if(isOp(')'))throw SyntaxError("Syntax ERROR: ) in eccesso");
            throw SyntaxError("Syntax ERROR: caratteri extra");
        }
        return n;
    }
private:
    std::vector<Tok> t_; std::size_t i_{};
    const Tok& peek() const {return t_[i_];}
    bool isOp(char c) const {return peek().type==TT::Op&&peek().op==c;}
    bool eat(char c){if(isOp(c)){++i_;return true;}return false;}
    static ast::NodePtr bin(char op,ast::NodePtr a,ast::NodePtr b){auto n=ast::make(ast::Kind::Binary,std::string(1,op));n->children={std::move(a),std::move(b)};return n;}
    bool startsAtom() const {
        const Tok& k=peek();
        return k.type==TT::Num||k.type==TT::Ident||(k.type==TT::Op&&k.op=='(');
    }
    bool atFunction() const {return peek().type==TT::Ident&&isFunctionName(peek().text);}
    ast::NodePtr addSub(){
        auto n=mulDiv();
        for(;;){if(eat('+'))n=bin('+',n,mulDiv());else if(eat('-'))n=bin('-',n,mulDiv());else return n;}
    }
    ast::NodePtr mulDiv(){
        auto n=implicitProduct();
        for(;;){
            if(eat('*'))n=bin('*',n,implicitProduct());
            else if(eat('/'))n=bin('/',n,implicitProduct());
            else if(eat('%'))n=bin('%',n,implicitProduct());
            else return n;
        }
    }
    // Juxtaposition (2x, 2(3), x sin x) has higher priority than * and /.
    // stopAtFunction: the argument of a prefix function ends before the next
    // function name, so "sin x cos x" is sin(x)*cos(x).
    ast::NodePtr implicitProduct(bool stopAtFunction=false){
        auto n=unary();
        while(startsAtom()&&!(stopAtFunction&&atFunction())){
            if(peek().type==TT::Num&&i_>0&&t_[i_-1].type==TT::Num)throw SyntaxError("Syntax ERROR: numeri adiacenti");
            n=bin('*',n,unary());
        }
        return n;
    }
    ast::NodePtr unary(){
        if(eat('+'))return unary();
        if(eat('-')){auto n=ast::make(ast::Kind::Unary,"-");n->children={unary()};return n;}
        return power();
    }
    ast::NodePtr power(){
        auto base=postfix();
        if(eat('^')){
            // Right-associative; the exponent may carry its own sign (2^-2).
            ast::NodePtr e;
            if(eat('-')){e=ast::make(ast::Kind::Unary,"-");e->children={powerOperand()};}
            else{(void)eat('+');e=powerOperand();}
            return bin('^',base,e);
        }
        return base;
    }
    ast::NodePtr powerOperand(){return power();}
    ast::NodePtr postfix(){
        auto n=atom();
        for(;;){
            if(eat('!')){auto c=ast::make(ast::Kind::Call,"factorial");c->children={n};n=c;}
            else if(eat('d')){auto c=ast::make(ast::Kind::Call,"fromdeg");c->children={n};n=c;}
            else return n;
        }
    }
    ast::NodePtr atom(){
        const Tok k=peek();
        if(k.type==TT::Num){++i_;return ast::make(ast::Kind::Token,k.text);}
        if(k.type==TT::Op&&k.op=='('){
            ++i_;
            if(isOp(')'))throw SyntaxError("Syntax ERROR: parentesi vuote");
            auto inner=addSub();
            // Casio allows omitting closing parentheses at the end of the input.
            if(!eat(')')&&peek().type!=TT::End)throw SyntaxError("Syntax ERROR: manca )");
            auto g=ast::make(ast::Kind::Group);g->children={inner};return g;
        }
        if(k.type==TT::Ident){
            ++i_;
            if(isFunctionName(k.text)){
                auto call=ast::make(ast::Kind::Call,k.text);
                const FnInfo* info=findFunction(k.text);
                const unsigned minArgs=info?info->minArgs:1,maxArgs=info?info->maxArgs:1;
                if(eat('(')){
                    if(!isOp(')')){
                        do{call->children.push_back(addSub());}while(eat(','));
                    }
                    if(!eat(')')&&peek().type!=TT::End)throw SyntaxError("Syntax ERROR: manca ) in "+k.text);
                }else{
                    if(minArgs==0){/* rand */}
                    else if(maxArgs>1&&minArgs>1)throw SyntaxError("Syntax ERROR: "+k.text+" richiede parentesi");
                    else call->children.push_back(implicitProduct(true));
                }
                if(call->children.size()<minArgs||call->children.size()>maxArgs)
                    throw SyntaxError("Argument ERROR: "+k.text+" numero di argomenti errato");
                return call;
            }
            return ast::make(ast::Kind::Token,k.text);
        }
        if(k.type==TT::End)throw SyntaxError("Syntax ERROR: operando mancante");
        if(k.type==TT::Op&&k.op==')')throw SyntaxError("Syntax ERROR: ) inattesa");
        throw SyntaxError(std::string("Syntax ERROR: operatore '")+k.op+"' inatteso");
    }
};

// ---------------------------------------------------------------- evaluation helpers
std::uint32_t g_rng=0x9E3779B9u;
std::uint32_t nextRandom(){std::uint32_t x=g_rng;x^=x<<13;x^=x>>17;x^=x<<5;g_rng=x?x:0x9E3779B9u;return g_rng;}

double snapTrig(double v){return std::abs(v)<1e-15?0.0:v;}
// Exact values at multiples of 90 deg (or 100 grad) avoid sin(180)=1.2e-16 artefacts.
bool exactAngle(double x,double& sinOut,double& cosOut){
    double quarter=0;
    switch(angleMode()){case AngleMode::Degrees:quarter=90.0;break;case AngleMode::Gradians:quarter=100.0;break;default:return false;}
    const double q=x/quarter;
    if(std::abs(q)>1e15||q!=std::floor(q))return false;
    const long long k=((static_cast<long long>(q)%4)+4)%4;
    static const double s[4]={0,1,0,-1},c[4]={1,0,-1,0};
    sinOut=s[k];cosOut=c[k];return true;
}
bool isInteger(double v){return std::isfinite(v)&&v==std::floor(v);}
double requireReal(const Complex& z,const char* what){
    if(std::abs(z.imag())>1e-12*std::max(1.0,std::abs(z.real())))throw MathError(std::string("Argument ERROR: ")+what+" richiede un valore reale");
    return z.real();
}
double factorialChecked(double a){
    if(a<0||!isInteger(a))throw MathError("Math ERROR: fattoriale di un non intero");
    if(a>170)throw MathError("Math ERROR: fattoriale troppo grande");
    return factorial(static_cast<unsigned>(a));
}
// Best rational approximation p/q (q <= maxQ) by continued fractions.
bool smallRational(double v,long long maxQ,long long& p,long long& q){
    if(!std::isfinite(v)||std::abs(v)>1e12)return false;
    const bool neg=v<0;double x=std::abs(v);
    long long h0=0,h1=1,k0=1,k1=0;double z=x;
    for(int i=0;i<40;++i){
        const double a=std::floor(z);
        const long long ai=static_cast<long long>(a);
        const long long h2=ai*h1+h0,k2=ai*k1+k0;
        if(k2>maxQ)break;
        h0=h1;h1=h2;k0=k1;k1=k2;
        if(std::abs(static_cast<double>(h1)/static_cast<double>(k1)-x)<=1e-12*std::max(1.0,x))break;
        const double rem=z-a;if(rem<1e-15)break;z=1.0/rem;
    }
    if(k1==0||std::abs(static_cast<double>(h1)/static_cast<double>(k1)-x)>1e-12*std::max(1.0,x))return false;
    p=neg?-h1:h1;q=k1;return true;
}
// Real power with the Casio behaviour for negative bases and rational exponents
// with odd denominators: (-8)^(1/3) = -2.
double realPow(double a,double b){
    if(a==0&&b<0)throw MathError("Math ERROR: divisione per zero");
    if(a<0&&!isInteger(b)){
        long long p=0,q=0;
        if(smallRational(b,999,p,q)&&q%2==1){const double m=std::pow(-a,b);return (p%2!=0)?-m:m;}
        throw NonRealError("Non-Real ERROR: potenza di base negativa");
    }
    return std::pow(a,b);
}
long long toIntArg(double v,const char* what){
    if(!isInteger(v)||std::abs(v)>9e15)throw MathError(std::string("Argument ERROR: ")+what+" richiede interi");
    return static_cast<long long>(v);
}

// Evaluation environment with a stack of calculus bindings that shadow the context.
struct Env {
    const EvalContext& ctx;
    Binding local[8]{};
    int localCount{};
    std::uint8_t depth{};
};
struct LocalBind {
    Env& env;
    LocalBind(Env& e,const char* name,double v):env(e){
        if(env.localCount>=8)throw MathError("Stack ERROR: troppi livelli di calcolo annidati");
        env.local[env.localCount++]={name,v};
    }
    ~LocalBind(){--env.localCount;}
    void set(double v){env.local[env.localCount-1].value=v;}
};
bool lookupBinding(const Env& env,const std::string& name,double& out){
    const std::string low=lower(name);
    for(int i=env.localCount-1;i>=0;--i)if(low==env.local[i].name){out=env.local[i].value;return true;}
    for(int i=0;i<env.ctx.bindingCount;++i)if(env.ctx.bindings[i].name&&low==env.ctx.bindings[i].name){out=env.ctx.bindings[i].value;return true;}
    return false;
}

template<typename T> T evalNode(const ast::Node& n,Env& env);

template<typename T> double asReal(const T& v,const char* what){
    if constexpr(std::is_same_v<T,double>){(void)what;return v;}
    else return requireReal(v,what);
}
template<typename T> T fromReal(double v){if constexpr(std::is_same_v<T,double>)return v;else return Complex(v,0.0);}
template<typename T> void checkFinite(const T& v){
    if constexpr(std::is_same_v<T,double>){if(!std::isfinite(v))throw MathError("Math ERROR: risultato non finito");}
    else{if(!std::isfinite(v.real())||!std::isfinite(v.imag()))throw MathError("Math ERROR: risultato non finito");}
}

// Adaptive Gauss-Kronrod G7/K15 quadrature.
template<typename F> double gk15(F& f,double a,double b,double& err){
    static const double xk[8]={0.991455371120812639,0.949107912342758525,0.864864423359769073,0.741531185599394440,0.586087235467691130,0.405845151377397167,0.207784955007898468,0.0};
    static const double wk[8]={0.022935322010529225,0.063092092629978553,0.104790010322250184,0.140653259715525919,0.169004726639267903,0.190350578064785410,0.204432940075298892,0.209482141084727828};
    static const double wg[4]={0.129484966168869693,0.279705391489276668,0.381830050505118945,0.417959183673469388};
    const double c=0.5*(a+b),h=0.5*(b-a);
    const double fc=f(c);double resK=fc*wk[7],resG=fc*wg[3];
    for(int j=0;j<7;++j){const double dx=h*xk[j];const double f1=f(c-dx),f2=f(c+dx);resK+=wk[j]*(f1+f2);if(j%2==1)resG+=wg[j/2]*(f1+f2);}
    err=std::abs((resK-resG)*h);return resK*h;
}
template<typename F> double adaptiveIntegrate(F& f,double a,double b,double tol,int depth,int& evals){
    double err=0;const double r=gk15(f,a,b,err);evals+=15;
    if(!std::isfinite(r))throw MathError("Math ERROR: integrale non convergente (singolarita)");
    if(err<=std::max(tol,1e-14*std::abs(r))||depth>=18||evals>20000)return r;
    const double m=0.5*(a+b);
    return adaptiveIntegrate(f,a,m,tol*0.5,depth+1,evals)+adaptiveIntegrate(f,m,b,tol*0.5,depth+1,evals);
}
template<typename F> double integrateFn(F f,double a,double b,double tol){
    if(!std::isfinite(a)||!std::isfinite(b))throw MathError("Math ERROR: limiti di integrazione non finiti");
    if(a==b)return 0.0;
    int evals=0;
    return adaptiveIntegrate(f,a,b,tol,0,evals);
}
// Ridders' extrapolation of central differences.
template<typename F> double ridders(F f,double x){
    double h=std::max(1e-3,std::abs(x)*1e-3);
    constexpr int N=10;double a[N][N];double best=0,err=std::numeric_limits<double>::max();
    a[0][0]=(f(x+h)-f(x-h))/(2*h);best=a[0][0];
    for(int i=1;i<N;++i){
        h/=1.4;a[0][i]=(f(x+h)-f(x-h))/(2*h);double fac=1.96;
        for(int j=1;j<=i;++j){a[j][i]=(a[j-1][i]*fac-a[j-1][i-1])/(fac-1);fac*=1.96;
            const double e=std::max(std::abs(a[j][i]-a[j-1][i]),std::abs(a[j][i]-a[j-1][i-1]));
            if(e<=err){err=e;best=a[j][i];}}
        if(std::abs(a[i][i]-a[i-1][i-1])>=2*err)break;
    }
    if(!std::isfinite(best))throw MathError("Math ERROR: derivata non definita");
    return best;
}

template<typename T> T callFunction(const ast::Node& n,Env& env){
    const std::string& f=n.text;
    auto arg=[&](std::size_t i)->T{return evalNode<T>(*n.children.at(i),env);};
    auto rarg=[&](std::size_t i)->double{return asReal<T>(arg(i),f.c_str());};
    auto R=[](double v){return fromReal<T>(v);};

    // ---- calculus (lazy: the first child is evaluated with x bound)
    if(f=="integral"||f=="integrate"||f=="derivative"||f=="diff"||f=="derivative2"||f=="sum"||f=="prod"||f=="solve"){
        if(++env.depth>4){--env.depth;throw MathError("Stack ERROR: calcoli annidati troppo profondi");}
        struct DepthGuard{Env& e;~DepthGuard(){--e.depth;}} guard{env};
        const ast::Node& body=*n.children.at(0);
        LocalBind bind(env,"x",0.0);
        auto fx=[&](double v)->double{bind.set(v);const T r=evalNode<T>(body,env);return asReal<T>(r,"funzione integranda");};
        if(f=="integral"||f=="integrate"){const double a=rarg(1),b=rarg(2);return R(integrateFn(fx,a,b,1e-10));}
        if(f=="derivative"||f=="diff"){return R(ridders(fx,rarg(1)));}
        if(f=="derivative2"){const double x0=rarg(1);auto d1=[&](double v){return ridders(fx,v);};return R(ridders(d1,x0));}
        if(f=="solve"){
            double x=rarg(1);
            for(int it=0;it<100;++it){
                const double y=fx(x);if(!std::isfinite(y))throw MathError("Math ERROR: SOLVE fuori dominio");
                if(std::abs(y)<1e-13)return R(x);
                const double d=ridders(fx,x);
                if(d==0||!std::isfinite(d))throw MathError("Math ERROR: SOLVE derivata nulla");
                double nx=x-y/d;
                if(!std::isfinite(nx))throw MathError("Math ERROR: SOLVE non convergente");
                if(std::abs(nx-x)<=1e-12*std::max(1.0,std::abs(x))){x=nx;if(std::abs(fx(x))<1e-9*std::max(1.0,std::abs(y)+1))return R(x);break;}
                x=nx;
            }
            throw MathError("Math ERROR: SOLVE non convergente");
        }
        // sum / prod over integer range, x bound to the index.
        const double a=rarg(1),b=rarg(2);
        if(!isInteger(a)||!isInteger(b))throw MathError("Argument ERROR: limiti di Σ/Π interi");
        if(std::abs(b-a)>100000)throw MathError("Argument ERROR: massimo 100001 termini");
        T acc=R(f=="sum"?0.0:1.0);
        const long long lo=static_cast<long long>(a),hi=static_cast<long long>(b);
        if(lo<=hi){for(long long k=lo;k<=hi;++k){bind.set(static_cast<double>(k));const T v=evalNode<T>(body,env);if(f=="sum")acc+=v;else acc*=v;}}
        return acc;
    }

    // ---- zero-argument
    if(f=="rand"||f=="ran")return R(static_cast<double>(nextRandom()>>8)/16777216.0);

    // ---- complex-specific
    if(f=="re"||f=="real"){if constexpr(std::is_same_v<T,double>)return arg(0);else return R(arg(0).real());}
    if(f=="im"||f=="imag"){if constexpr(std::is_same_v<T,double>){(void)arg(0);return 0.0;}else return R(arg(0).imag());}
    if(f=="conj"){if constexpr(std::is_same_v<T,double>)return arg(0);else return std::conj(arg(0));}
    if(f=="arg"){if constexpr(std::is_same_v<T,double>){const double v=arg(0);if(v==0)throw MathError("Math ERROR: argomento di zero");return fromRadians(v<0?kPi:0.0);}
        else{const Complex z=arg(0);if(std::abs(z)==0)throw MathError("Math ERROR: argomento di zero");return R(fromRadians(std::arg(z)));}}

    // ---- one argument
    if(n.children.size()==1){
        const T a=arg(0);
        if(f=="sin"||f=="cos"||f=="tan"){
            if constexpr(std::is_same_v<T,double>){
                double s=0,c=0;
                if(exactAngle(a,s,c)){if(f=="sin")return s;if(f=="cos")return c;if(c==0)throw MathError("Math ERROR: tan non definita");return s/c;}
                const double r=toRadians(a);
                if(f=="sin")return snapTrig(std::sin(r));
                if(f=="cos")return snapTrig(std::cos(r));
                const double cr=std::cos(r);if(std::abs(cr)<1e-15)throw MathError("Math ERROR: tan non definita");return snapTrig(std::tan(r));
            }else{
                const Complex r=a*toRadians(1.0);
                if(f=="sin")return std::sin(r);
                if(f=="cos")return std::cos(r);
                return std::tan(r);
            }
        }
        if(f=="asin"||f=="acos"){
            if constexpr(std::is_same_v<T,double>){if(a<-1-1e-15||a>1+1e-15)throw NonRealError("Non-Real ERROR: |x|>1");const double c=std::clamp(a,-1.0,1.0);return fromRadians(f=="asin"?std::asin(c):std::acos(c));}
            else{const Complex r=f=="asin"?std::asin(a):std::acos(a);return r/toRadians(1.0);}
        }
        if(f=="atan"){if constexpr(std::is_same_v<T,double>)return fromRadians(std::atan(a));else return std::atan(a)/toRadians(1.0);}
        if(f=="sinh")return std::sinh(a);
        if(f=="cosh")return std::cosh(a);
        if(f=="tanh")return std::tanh(a);
        if(f=="asinh")return std::asinh(a);
        if(f=="acosh"){if constexpr(std::is_same_v<T,double>){if(a<1)throw NonRealError("Non-Real ERROR: acosh(x<1)");}return std::acosh(a);}
        if(f=="atanh"){if constexpr(std::is_same_v<T,double>){if(a<=-1||a>=1)throw NonRealError("Non-Real ERROR: |x|>=1");}return std::atanh(a);}
        if(f=="sqrt"){if constexpr(std::is_same_v<T,double>){if(a<0)throw NonRealError("Non-Real ERROR: radice di un negativo");return std::sqrt(a);}else return std::sqrt(a);}
        if(f=="cbrt"){if constexpr(std::is_same_v<T,double>)return std::cbrt(a);else{if(std::abs(a.imag())<1e-15)return R(std::cbrt(a.real()));return std::pow(a,1.0/3.0);}}
        if(isRootN(f)){const int idx=static_cast<int>(std::strtol(f.c_str()+4,nullptr,10));
            if constexpr(std::is_same_v<T,double>){if(a<0&&idx%2==0)throw NonRealError("Non-Real ERROR: radice pari di un negativo");return nthRoot(a,idx);}
            else{if(std::abs(a.imag())<1e-15&&(a.real()>=0||idx%2==1))return R(nthRoot(a.real(),idx));return std::pow(a,1.0/idx);}}
        if(f=="abs"){if constexpr(std::is_same_v<T,double>)return std::abs(a);else return R(std::abs(a));}
        if(f=="ln"||f=="log"||f=="log10"||f=="log2"){
            if constexpr(std::is_same_v<T,double>){
                if(a==0)throw MathError("Math ERROR: logaritmo di zero");
                if(a<0)throw NonRealError("Non-Real ERROR: logaritmo di un negativo");
                return f=="ln"?std::log(a):f=="log2"?std::log2(a):std::log10(a);
            }else{
                if(std::abs(a)==0)throw MathError("Math ERROR: logaritmo di zero");
                const Complex l=std::log(a);return f=="ln"?l:f=="log2"?l/std::log(2.0):l/std::log(10.0);
            }
        }
        if(f=="exp")return std::exp(a);
        if(f=="recip"){if(a==T(0))throw MathError("Math ERROR: divisione per zero");return T(1)/a;}
        if(f=="percent")return a/T(100);
        if(f=="fromdeg")return a*T(fromRadians(kPi/180.0));
        // Real-only functions below.
        const double v=asReal<T>(a,f.c_str());
        if(f=="floor")return R(std::floor(v));
        if(f=="ceil")return R(std::ceil(v));
        if(f=="round")return R(std::round(v));
        if(f=="trunc"||f=="int")return R(std::trunc(v));
        if(f=="intg")return R(std::floor(v));
        if(f=="frac")return R(v-std::trunc(v));
        if(f=="sign")return R(static_cast<double>((v>0)-(v<0)));
        if(f=="deg")return R(v*180.0/kPi);
        if(f=="rad")return R(v*kPi/180.0);
        if(f=="grad")return R(v*200.0/kPi);
        if(f=="fact"||f=="factorial")return R(factorialChecked(v));
        if(f=="dms")return R(v);
        throw SyntaxError("Syntax ERROR: funzione sconosciuta "+f);
    }

    // ---- two or three arguments
    if(f=="pow"){const T a=arg(0),b=arg(1);if constexpr(std::is_same_v<T,double>)return realPow(a,b);else{if(a==T(0)&&b.real()<0)throw MathError("Math ERROR: divisione per zero");return std::pow(a,b);}}
    if(f=="log"){
        // fx-CG50 convention: log(a,b) = log base a of b.
        const T base=arg(0),value=arg(1);
        if constexpr(std::is_same_v<T,double>){
            if(base<=0||base==1||value<=0)throw MathError("Math ERROR: argomenti del logaritmo non validi");
            return std::log(value)/std::log(base);
        }else{
            if(std::abs(base)==0||std::abs(value)==0||base==T(1))throw MathError("Math ERROR: argomenti del logaritmo non validi");
            return std::log(value)/std::log(base);
        }
    }
    if(f=="root"){
        const double idx=rarg(0);
        if(idx<2||idx>99||!isInteger(idx))throw MathError("Argument ERROR: indice della radice tra 2 e 99");
        const T x=arg(1);
        if constexpr(std::is_same_v<T,double>){if(x<0&&static_cast<int>(idx)%2==0)throw NonRealError("Non-Real ERROR: radice pari di un negativo");return nthRoot(x,static_cast<int>(idx));}
        else{if(std::abs(x.imag())<1e-15&&(x.real()>=0||static_cast<int>(idx)%2==1))return R(nthRoot(x.real(),static_cast<int>(idx)));return std::pow(x,1.0/idx);}
    }
    if(f=="frac"){const T a=arg(0),b=arg(1);if(b==T(0))throw MathError("Math ERROR: denominatore nullo");return a/b;}
    if(f=="dms"){
        const double d=rarg(0),m=rarg(1),s=n.children.size()>2?rarg(2):0.0;
        const double sign=(d<0||m<0||s<0)?-1.0:1.0;
        return R(sign*(std::abs(d)+std::abs(m)/60.0+std::abs(s)/3600.0));
    }
    const double a=rarg(0),b=rarg(1);
    if(f=="min")return R(std::min(a,b));
    if(f=="max")return R(std::max(a,b));
    if(f=="round"){const long long dg=toIntArg(b,"round");if(dg<0||dg>14)throw MathError("Argument ERROR: cifre 0..14");const double p=std::pow(10.0,static_cast<double>(dg));return R(std::round(a*p)/p);}
    if(f=="ncr"||f=="npr"){
        if(a<0||b<0||!isInteger(a)||!isInteger(b)||a>100000)throw MathError("Argument ERROR: nCr/nPr richiedono interi non negativi");
        if(b>a)throw MathError("Math ERROR: r non puo superare n");
        // Computed in floating point to avoid 64-bit overflow for large n.
        double r=1;const auto nn=static_cast<unsigned long long>(a),rr=static_cast<unsigned long long>(b);
        if(f=="npr"){for(unsigned long long i=0;i<rr;++i)r*=static_cast<double>(nn-i);}
        else{const unsigned long long k=std::min(rr,nn-rr);for(unsigned long long i=1;i<=k;++i)r=r*static_cast<double>(nn-k+i)/static_cast<double>(i);r=std::round(r);}
        if(!std::isfinite(r))throw MathError("Math ERROR: overflow");
        return R(r);
    }
    if(f=="atan2")return R(fromRadians(std::atan2(a,b)));
    if(f=="pol")return R(std::hypot(a,b));
    if(f=="rec")return R(a*std::cos(toRadians(b)));
    if(f=="hypot")return R(std::hypot(a,b));
    if(f=="mod"){if(b==0)throw MathError("Math ERROR: modulo per zero");return R(std::fmod(a,b));}
    if(f=="gcd"||f=="lcm"){
        long long x=std::llabs(toIntArg(a,f.c_str())),y=std::llabs(toIntArg(b,f.c_str()));
        long long g=x,h=y;while(h){const long long t=g%h;g=h;h=t;}
        if(f=="gcd")return R(static_cast<double>(g));
        if(g==0)return R(0.0);
        return R(static_cast<double>(x/g)*static_cast<double>(y));
    }
    if(f=="randint"){
        const long long lo=toIntArg(a,"randint"),hi=toIntArg(b,"randint");
        if(hi<lo)throw MathError("Argument ERROR: randint(a,b) con a<=b");
        const unsigned long long span=static_cast<unsigned long long>(hi-lo)+1ULL;
        return R(static_cast<double>(lo+static_cast<long long>(nextRandom()%span)));
    }
    throw SyntaxError("Syntax ERROR: funzione sconosciuta "+f);
}

template<typename T> T evalNode(const ast::Node& n,Env& env){
    using ast::Kind;
    switch(n.kind){
    case Kind::Token:{
        if(n.text.empty())throw SyntaxError("Syntax ERROR: token vuoto");
        const unsigned char c0=static_cast<unsigned char>(n.text[0]);
        if(std::isdigit(c0)||c0=='.'){char* e=nullptr;const double v=std::strtod(n.text.c_str(),&e);if(e!=n.text.c_str()+n.text.size())throw SyntaxError("Syntax ERROR: numero non valido");return fromReal<T>(v);}
        double bound=0;
        if(lookupBinding(env,n.text,bound))return fromReal<T>(bound);
        if(n.text=="e")return fromReal<T>(std::exp(1.0));
        if(n.text=="i"){if constexpr(std::is_same_v<T,double>)throw NonRealError("Non-Real ERROR: i richiede il modo complesso");else return Complex(0,1);}
        const std::string low=lower(n.text);
        for(const auto& k:kConstants)if(low==k.name)return fromReal<T>(k.value);
        const CalculatorMemory* mem=env.ctx.memory;
        if(low=="ans"||low=="preans"){
            if(!mem)throw MathError("Math ERROR: Ans non disponibile");
            const double re=low=="ans"?mem->answer():mem->previousAnswer();
            const double im=low=="ans"?mem->answerImag():mem->previousAnswerImag();
            if constexpr(std::is_same_v<T,double>){if(im!=0)throw NonRealError("Non-Real ERROR: Ans e' complesso");return re;}
            else return Complex(re,im);
        }
        if(n.text.size()==1&&std::isalpha(c0)){
            if(mem)return fromReal<T>(mem->recall(static_cast<char>(c0)));
            throw MathError(std::string("Math ERROR: variabile ")+static_cast<char>(std::toupper(c0))+" non definita");
        }
        if(low=="theta")throw MathError("Math ERROR: theta non definito");
        throw SyntaxError("Syntax ERROR: nome sconosciuto "+n.text);
    }
    case Kind::Group: return evalNode<T>(*n.children.at(0),env);
    case Kind::Unary:{
        const T a=evalNode<T>(*n.children.at(0),env);
        if(n.text!="-")return a;
        if constexpr(std::is_same_v<T,double>)return -a;
        else{
            // Negating 4+0i must give -4+0i (not -4-0i): a signed zero imaginary
            // part would put sqrt(-4) and ln(-1) on the wrong side of the branch cut.
            const double re=-a.real(),im=-a.imag();
            return Complex(re==0.0?0.0:re,im==0.0?0.0:im);
        }
    }
    case Kind::Binary:{
        const T a=evalNode<T>(*n.children.at(0),env),b=evalNode<T>(*n.children.at(1),env);
        T r{};
        switch(n.text.empty()?'?':n.text[0]){
        case '+': r=a+b;break;
        case '-': r=a-b;break;
        case '*': r=a*b;break;
        case '/': if(b==T(0))throw MathError("Math ERROR: divisione per zero");r=a/b;break;
        case '%':{const double x=asReal<T>(a,"%"),y=asReal<T>(b,"%");if(y==0)throw MathError("Math ERROR: modulo per zero");r=fromReal<T>(std::fmod(x,y));break;}
        case '^':
            if constexpr(std::is_same_v<T,double>)r=realPow(a,b);
            else{
                if(a==T(0)){if(b.real()<0)throw MathError("Math ERROR: divisione per zero");r=(b==T(0))?T(1):T(0);}
                else if(b.imag()==0&&isInteger(b.real())&&std::abs(b.real())<=64){
                    // Exact repeated multiplication keeps i^2 = -1 free of rounding noise.
                    T acc(1);T base=b.real()<0?T(1)/a:a;for(int k=0;k<static_cast<int>(std::abs(b.real()));++k)acc*=base;r=acc;
                }else r=std::pow(a,b);
            }
            break;
        default: throw SyntaxError("Syntax ERROR: operatore sconosciuto");
        }
        checkFinite(r);
        return r;
    }
    case Kind::Call:{T r=callFunction<T>(n,env);checkFinite(r);return r;}
    default: break;
    }
    throw SyntaxError("Syntax ERROR: nodo non valutabile");
}

bool isEditorKind(ast::Kind k){
    using ast::Kind;
    return k==Kind::Row||k==Kind::Fraction||k==Kind::Radical||k==Kind::Power||k==Kind::Subscript||k==Kind::Calculus||k==Kind::Matrix;
}
bool containsEditorNodes(const ast::Node& n){
    if(isEditorKind(n.kind))return true;
    for(const auto& c:n.children)if(c&&containsEditorNodes(*c))return true;
    return false;
}
} // namespace

EvalContext& EvalContext::bind(const char* name,double value){
    for(std::uint8_t i=0;i<bindingCount;++i)if(std::strcmp(bindings[i].name,name)==0){bindings[i].value=value;return *this;}
    if(bindingCount>=MaxBindings)throw std::length_error("Troppe variabili vincolate");
    bindings[bindingCount++]={name,value};return *this;
}

ast::NodePtr parseExpression(const std::string& expression){
    return Parser(Lexer(expression).run()).run();
}
double evaluateReal(const ast::Node& parsed,const EvalContext& context){
    Env env{context};
    const double v=evalNode<double>(parsed,env);
    if(!std::isfinite(v))throw MathError("Math ERROR: risultato non finito");
    return v==0.0?0.0:v;   // normalise -0
}
Complex evaluateComplex(const ast::Node& parsed,const EvalContext& context){
    Env env{context};
    Complex v=evalNode<Complex>(parsed,env);
    const double scale=std::max(std::abs(v.real()),std::abs(v.imag()));
    double re=v.real(),im=v.imag();
    if(std::abs(re)<1e-14*scale)re=0;
    if(std::abs(im)<1e-14*scale)im=0;
    return {re==0.0?0.0:re,im==0.0?0.0:im};
}
Complex evaluateValue(const ast::Node& parsed,const EvalContext& context){
    if(context.complexMode==ComplexMode::Real)return {evaluateReal(parsed,context),0.0};
    return evaluateComplex(parsed,context);
}

CompiledExpression::CompiledExpression(const std::string& text,const char* variable)
    :root_(parseExpression(text)),text_(text),variable_(variable){}
double CompiledExpression::operator()(double v,const CalculatorMemory* memory) const{
    if(!root_)throw SyntaxError("Syntax ERROR: funzione vuota");
    EvalContext ctx;ctx.memory=memory;ctx.bind(variable_,v);
    return evaluateReal(*root_,ctx);
}
bool CompiledExpression::tryEval(double v,double& out,const CalculatorMemory* memory) const noexcept{
    try{out=(*this)(v,memory);return true;}catch(...){return false;}
}
double CompiledExpression::eval2(double a,const char* nameB,double b,const CalculatorMemory* memory) const{
    if(!root_)throw SyntaxError("Syntax ERROR: funzione vuota");
    EvalContext ctx;ctx.memory=memory;ctx.bind(variable_,a);ctx.bind(nameB,b);
    return evaluateReal(*root_,ctx);
}

double evaluate(const std::string& e,double x){EvalContext c;c.bind("x",x);c.bind("y",0.0);return evaluateReal(*parseExpression(e),c);}
double evaluate(const ast::Node& e,double x){
    EvalContext c;c.bind("x",x);c.bind("y",0.0);
    if(containsEditorNodes(e))return evaluateReal(*parseExpression(ast::serialize(e)),c);
    return evaluateReal(e,c);
}
double evaluate(const std::string& e,double x,double y){EvalContext c;c.bind("x",x);c.bind("y",y);return evaluateReal(*parseExpression(e),c);}
double evaluate(const std::string& e,const CalculatorMemory& m,double x,double y){EvalContext c;c.memory=&m;c.bind("x",x);c.bind("y",y);return evaluateReal(*parseExpression(e),c);}
double compose(const std::string& outer,const std::string& inner,double x){return evaluate(outer,evaluate(inner,x));}
double inverseValue(const std::string& e,double target,double lo,double hi,unsigned iterations){
    if(!(lo<hi))throw std::invalid_argument("Intervallo inversa non valido");
    const CompiledExpression f(e);
    auto g=[&](double v){return f(v)-target;};
    double fl=g(lo),fh=g(hi);
    if(fl==0)return lo;
    if(fh==0)return hi;
    if(fl*fh>0)throw MathError("Valore fuori dal ramo o funzione non monotona nell'intervallo");
    for(unsigned i=0;i<iterations;++i){const double mid=lo+(hi-lo)*.5,fm=g(mid);
        if(std::abs(fm)<1e-12)return mid;
        if((fl<0&&fm>0)||(fl>0&&fm<0)){hi=mid;fh=fm;}else{lo=mid;fl=fm;}}
    return lo+(hi-lo)*.5;
}
double integrateAdaptive(const CompiledExpression& f,double a,double b,double tolerance,const CalculatorMemory* memory){
    auto g=[&](double v){return f(v,memory);};
    return integrateFn(g,a,b,tolerance);
}
double differentiate(const CompiledExpression& f,double x,const CalculatorMemory* memory){
    auto g=[&](double v){return f(v,memory);};
    return ridders(g,x);
}
bool isReservedWord(const std::string& lowerName){return isFunctionName(lowerName)||isConstantName(lowerName);}
void seedRandom(std::uint32_t seed) noexcept{g_rng=seed?seed:0x9E3779B9u;}
} // namespace fx
