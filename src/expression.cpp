// Single expression pipeline: UTF-8 text -> tokens -> canonical AST -> evaluator.
// The evaluator is a template over double (Real mode) and std::complex<double>
// (a+bi and r<theta modes), so complex numbers are part of the parser/AST rather
// than a separate API. Operator priority follows the fx-CG50 manual: implicit
// multiplication binds tighter than * and /, prefix functions without
// parentheses take an implicit-product argument (sin 2x = sin(2x)).
#include "expression.hpp"
#include "limits.hpp"
#include "math_engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <numeric>
#include <type_traits>
#include <vector>

#if defined(__GNUC__)
#define FXAI_NOINLINE __attribute__((noinline))
#else
#define FXAI_NOINLINE
#endif

// Default evaluation stack budget: generous on the host, set by the firmware
// build for the 32 KB stack of the RP2350 (see CMakeLists.txt).
#ifndef FXAI_EVAL_STACK_BUDGET
#define FXAI_EVAL_STACK_BUDGET (1024u * 1024u)
#endif

namespace fx {
namespace {
constexpr double kPi = 3.14159265358979323846;
std::size_t gEvalStackBudget = FXAI_EVAL_STACK_BUDGET;

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
    {"imag",1,1,false},{"min",1,2,false},{"max",1,2,false},{"root",2,2,false},{"ncr",2,2,false},
    {"npr",2,2,false},{"atan2",2,2,false},{"hypot",2,2,false},{"pow",2,2,false},{"mod",2,2,false},
    {"gcd",2,2,false},{"lcm",2,2,false},{"randint",2,2,false},{"rand",0,0,false},{"ran",0,0,false},
    {"dms",1,3,false},{"pol",2,2,false},{"rec",2,2,false},
    {"mixed",3,3,false},{"modexp",3,3,false},{"rndfix",2,2,false},{"rannorm",2,2,false},{"ranbin",2,2,false},
    {"not",1,1,false},{"neg",1,1,false},
    {"integral",3,3,true},{"integrate",3,3,true},{"derivative",2,2,true},{"diff",2,2,true},
    {"derivative2",2,2,true},{"sum",1,3,true},{"prod",1,3,true},{"solve",2,2,true},
    // Matrici, vettori e liste (fx-CG50 OPTN > MAT/VCT e OPTN > LIST).
    {"det",1,1,false},{"trn",1,1,false},{"identity",1,1,false},{"dim",1,1,false},{"augment",2,2,false},
    {"fill",2,2,false},{"ref",1,1,false},{"rref",1,1,false},{"mattolist",2,2,false},{"listtomat",1,26,false},
    {"dotp",2,2,false},{"crossp",2,2,false},{"angle",2,2,false},{"unitv",1,1,false},{"norm",1,1,false},
    {"seq",5,5,true},{"mean",1,1,false},{"median",1,1,false},{"cuml",1,1,false},{"dlist",1,1,false},
    {"sorta",1,1,false},{"sortd",1,1,false},
};
// Funzioni con argomenti o risultato lista, matrice o vettore: i loro nodi
// li valuta il valutatore delle collezioni (vedi CollEval).
bool isCollectionFunction(const std::string& f,std::size_t argCount){
    static const char* const names[]={"det","trn","identity","dim","augment","fill","ref","rref","mattolist","listtomat",
        "dotp","crossp","angle","unitv","norm","seq","mean","median","cuml","dlist","sorta","sortd","elem"};
    for(const char* n:names)if(f==n)return true;
    return argCount==1&&(f=="sum"||f=="prod"||f=="min"||f=="max");
}
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
// Variables owned by other applications (VARS menu), resolved through EvalContext::names.
constexpr const char* kSystemNames[] = {"xmin","xmax","xscl","ymin","ymax","yscl","tmin","tmax","tptch",
    "tblstart","tblend","tblpitch","xdot","list1","list2","list3","list4","list5","list6","list7","list8","list9","list10",
    "list11","list12","list13","list14","list15","list16","list17","list18","list19","list20","list21","list22","list23",
    "list24","list25","list26",nullptr};
// Infix operator words (fx-CG50 And/Or/Xor, Int÷ and Rmdr) and their internal op codes.
struct OpWord { const char* name; char op; };
constexpr OpWord kOperatorWords[] = {{"and",'&'},{"or",'|'},{"xor",'x'},{"xnor",'X'},{"rmdr",'r'},{"intdiv",'q'}};
char operatorWord(const std::string& lowerName){for(const auto& w:kOperatorWords)if(lowerName==w.name)return w.op;return 0;}

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
bool isSystemName(const std::string& lowerName){
    for(const char* const* n=kSystemNames;*n;++n)if(lowerName==*n)return true;
    return false;
}
// "list1".."list26": le liste del workspace (Statistics, List 1-26). In GRAPH
// una lista in una funzione indica uno dei suoi elementi (un grafico per
// elemento, risolto dal NameResolver del motore dei grafici); altrove e'
// l'intera lista.
int listNumber(const std::string& lowerName){
    if(lowerName.size()<5||lowerName.size()>6||lowerName.compare(0,4,"list")!=0||lowerName[4]=='0')return 0;
    int v=0;
    for(std::size_t i=4;i<lowerName.size();++i){if(lowerName[i]<'0'||lowerName[i]>'9')return 0;v=v*10+(lowerName[i]-'0');}
    return v>=1&&v<=CollectionResolver::Slots?v:0;
}
bool isListName(const std::string& lowerName){return listNumber(lowerName)!=0;}
bool isConstantName(const std::string& lowerName){
    for(const auto& c:kConstants)if(lowerName==c.name)return true;
    for(const char* s:kSpecialNames)if(lowerName==s)return true;
    return isSystemName(lowerName);
}
// Graph function reference word without '@': y|r|xt|yt|x followed by 1..20.
bool referenceName(const std::string& low){
    std::size_t k=0;while(k<low.size()&&std::isalpha(static_cast<unsigned char>(low[k])))++k;
    const std::string kind=low.substr(0,k),num=low.substr(k);
    if(kind!="y"&&kind!="r"&&kind!="xt"&&kind!="yt"&&kind!="x")return false;
    if(num.empty()||num.size()>2||num[0]=='0')return false;
    for(char c:num)if(!std::isdigit(static_cast<unsigned char>(c)))return false;
    const int v=std::atoi(num.c_str());return v>=1&&v<=20;
}

// Simboli ingegneristici (fx-CG50 OPTN > ESYM): nel testo canonico "#k",
// "#M", "#u" (micro, mostrato µ)... Moltiplicano il valore che precedono.
struct EngSymbol { char symbol; const char* factor; };
constexpr EngSymbol kEngSymbols[] = {{'k',"1e3"},{'M',"1e6"},{'G',"1e9"},{'T',"1e12"},{'m',"1e-3"},{'u',"1e-6"},{'n',"1e-9"},
                                     {'p',"1e-12"},{'f',"1e-15"}};
const char* engFactor(char c){for(const auto& e:kEngSymbols)if(e.symbol==c)return e.factor;return nullptr;}
// Frazione lineare della modalita' Line: "2⌟3", "1⌟2⌟3" (U+231F).
const char* const kFractionMark="\xE2\x8C\x9F";

// ---------------------------------------------------------------- lexer
// Ogni token conserva il suo intervallo di byte nel testo: il parser segnala un
// errore di sintassi sul token che non puo' usare e registra l'intervallo di
// ogni sotto-espressione nel suo nodo (vedi ast::Node::pos/len).
enum class TT { Num, Ident, Op, Ref, End };
struct Tok { TT type{TT::End}; std::string text; char op{}; std::uint16_t pos{}, len{}; };

class Lexer {
public:
    // base: base dei numeri senza prefisso (10, oppure 16/2/8 e 10 intero nella
    // modalita' Base-N: allora integer e' vero, i numeri sono interi a 32 bit).
    explicit Lexer(const std::string& s,unsigned base=10,bool integer=false):s_(s),base_(base),integer_(integer){
        if(s_.size()>60000)throw MathError("Memory ERROR: espressione troppo lunga");
    }
    std::vector<Tok> run(){
        std::vector<Tok> out;
        while(true){
            skipSpace();
            if(p_>=s_.size()){out.push_back({TT::End,{},0,at(p_),0});return out;}
            const unsigned char c=static_cast<unsigned char>(s_[p_]);
            if(std::isdigit(c)||(c=='.'&&p_+1<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[p_+1])))){out.push_back(integer_?integerNumber():number());continue;}
            if(c=='#'&&p_+1<s_.size()&&engFactor(s_[p_+1])){
                if(integer_)throw SyntaxError("Syntax ERROR: simboli ingegneristici non disponibili in Base-N",static_cast<int>(p_),2);
                out.push_back({TT::Op,std::string("#")+engFactor(s_[p_+1]),'#',at(p_),2});p_+=2;continue;
            }
            if(std::isalpha(c)||c=='_'){
                // Base-N esadecimale: una parola di sole cifre A-F e' un numero.
                if(integer_&&base_==16&&hexWord()){out.push_back(integerNumber());continue;}
                identifier(out);continue;
            }
            if(c=='@'){reference(out);continue;}
            if(c>=0x80){utf8(out);continue;}
            if(c=='<'||c=='>'){
                const bool eq=p_+1<s_.size()&&s_[p_+1]=='=';
                out.push_back({TT::Op,{},c=='<'?(eq?'l':'<'):(eq?'g':'>'),at(p_),static_cast<std::uint16_t>(eq?2:1)});p_+=eq?2:1;continue;
            }
            if(std::strchr("+-*/%^!,()=[]{}",static_cast<int>(c))){out.push_back({TT::Op,{},static_cast<char>(c),at(p_),1});++p_;continue;}
            throw SyntaxError(std::string("Syntax ERROR: carattere non valido '")+static_cast<char>(c)+"'",static_cast<int>(p_),1);
        }
    }
private:
    const std::string& s_; std::size_t p_{};
    unsigned base_{10};
    bool integer_{false};
    static std::uint16_t at(std::size_t p){return static_cast<std::uint16_t>(p);}
    static int digitValue(char ch){
        const unsigned char u=static_cast<unsigned char>(ch);
        if(std::isdigit(u))return ch-'0';
        if(std::isalpha(u))return std::tolower(u)-'a'+10;
        return 99;
    }
    bool hexWord() const {
        std::size_t q=p_;
        while(q<s_.size()&&(std::isalnum(static_cast<unsigned char>(s_[q]))||s_[q]=='_'))++q;
        for(std::size_t k=p_;k<q;++k)if(digitValue(s_[k])>=16)return false;
        return q>p_;
    }
    // Base-N (fx-CG50 Mode Dec/Hex/Bin/Oct): numero intero a 32 bit nella base
    // corrente o con un prefisso 0d/0x/0b/0o; Hex, Bin e Oct sono in
    // complemento a due (FFFFFFFF = -1), Dec va da -2147483648 a 2147483647.
    Tok integerNumber(){
        const std::size_t b=p_;
        unsigned base=base_;
        if(s_[p_]=='0'&&p_+2<s_.size()){
            const char k=static_cast<char>(std::tolower(static_cast<unsigned char>(s_[p_+1])));
            const unsigned pb=k=='d'?10:k=='x'?16:k=='b'?2:k=='o'?8:0;
            if(pb&&digitValue(s_[p_+2])<static_cast<int>(pb)){base=pb;p_+=2;}
        }
        const std::size_t start=p_;
        std::uint64_t v=0;
        while(p_<s_.size()&&(std::isalnum(static_cast<unsigned char>(s_[p_]))||s_[p_]=='.')){
            const char ch=s_[p_];
            if(ch=='.')throw SyntaxError("Syntax ERROR: in Base-N i numeri sono interi",static_cast<int>(p_),1);
            const int d=digitValue(ch);
            if(d>=static_cast<int>(base)){
                if(std::isalpha(static_cast<unsigned char>(ch))&&base!=16)break;       // una variabile dopo il numero (2A)
                throw SyntaxError(std::string("Syntax ERROR: cifra '")+ch+"' non valida in base "+std::to_string(base),static_cast<int>(p_),1);
            }
            v=v*base+static_cast<unsigned>(d);
            if(v>0xFFFFFFFFull)throw MathError("Math ERROR: numero oltre i 32 bit",static_cast<int>(b),static_cast<int>(p_-b+1));
            ++p_;
        }
        if(p_==start)throw SyntaxError("Syntax ERROR: numero mancante dopo il prefisso",static_cast<int>(b),static_cast<int>(p_-b));
        long long signedValue=static_cast<long long>(v);
        if(base==10){if(v>0x7FFFFFFFull)throw MathError("Math ERROR: oltre 2147483647 in Base-N",static_cast<int>(b),static_cast<int>(p_-b));}
        else if(v>0x7FFFFFFFull)signedValue=static_cast<long long>(v)-0x100000000ll;   // complemento a due
        return Tok{TT::Num,std::to_string(signedValue),0,at(b),at(p_-b)};
    }
    void skipSpace(){while(p_<s_.size()&&std::isspace(static_cast<unsigned char>(s_[p_])))++p_;}
    Tok number(){
        const std::size_t b=p_;
        auto tok=[&](std::string text){return Tok{TT::Num,std::move(text),0,at(b),at(p_-b)};};
        if(s_[p_]=='0'&&p_+2<s_.size()&&(s_[p_+1]=='x'||s_[p_+1]=='X'||s_[p_+1]=='b'||s_[p_+1]=='B'||s_[p_+1]=='o'||s_[p_+1]=='O'||s_[p_+1]=='d'||s_[p_+1]=='D')){
            const char kind=static_cast<char>(std::tolower(static_cast<unsigned char>(s_[p_+1])));
            const unsigned base=kind=='x'?16:kind=='b'?2:kind=='d'?10:8;
            std::size_t q=p_+2,start=q;
            auto digitOk=[&](char ch){const int d=std::isdigit(static_cast<unsigned char>(ch))?ch-'0':std::isalpha(static_cast<unsigned char>(ch))?std::tolower(static_cast<unsigned char>(ch))-'a'+10:99;return d<static_cast<int>(base);};
            while(q<s_.size()&&digitOk(s_[q]))++q;
            if(q>start){p_=q;const std::uint64_t v=fromBase(s_.substr(start,q-start),base);return tok(std::to_string(v));}
        }
        while(p_<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[p_])))++p_;
        // Frazione lineare "2⌟3" / "1⌟2⌟3" (modalita' Line): un solo letterale.
        if(s_.compare(p_,3,kFractionMark)==0){
            int marks=0;
            while(s_.compare(p_,3,kFractionMark)==0){
                const std::size_t q=p_+3;std::size_t e=q;
                while(e<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[e])))++e;
                if(e==q)throw SyntaxError("Syntax ERROR: dopo \xE2\x8C\x9F serve un intero",static_cast<int>(p_),3);
                if(++marks>2)throw SyntaxError("Syntax ERROR: troppi \xE2\x8C\x9F",static_cast<int>(p_),3);
                p_=e;
            }
            if(p_<s_.size()&&(s_[p_]=='.'||s_[p_]=='_'))throw SyntaxError("Syntax ERROR: una frazione ha termini interi",static_cast<int>(p_),1);
            return tok(s_.substr(b,p_-b));
        }
        if(p_<s_.size()&&s_[p_]=='.'){++p_;while(p_<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[p_])))++p_;}
        // Repeating decimal: "0.1_6" = 0.1666... (written with an overline in the editor).
        if(p_<s_.size()&&s_[p_]=='_'){
            if(s_.find('.',b)>=p_)throw SyntaxError("Syntax ERROR: il periodo segue la virgola",static_cast<int>(p_),1);
            const std::size_t q=++p_;
            while(p_<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[p_])))++p_;
            if(p_==q)throw SyntaxError("Syntax ERROR: periodo vuoto",static_cast<int>(q-1),1);
            const std::string t=s_.substr(b,p_-b);
            exact::Rational r;
            if(!exact::parseDecimal(t,r))throw MathError("Memory ERROR: numero periodico troppo lungo",static_cast<int>(b),static_cast<int>(p_-b));
            return tok(t);
        }
        if(p_<s_.size()&&(s_[p_]=='e'||s_[p_]=='E')){
            std::size_t q=p_+1;if(q<s_.size()&&(s_[q]=='+'||s_[q]=='-'))++q;
            if(q<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[q]))){p_=q;while(p_<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[p_])))++p_;}
        }
        std::string t=s_.substr(b,p_-b);
        if(std::count(t.begin(),t.end(),'.')>1)throw SyntaxError("Syntax ERROR: numero non valido",static_cast<int>(b),static_cast<int>(p_-b));
        return tok(std::move(t));
    }
    // "@Y3", "@Xt2": reference to a graph function (VARS > GRAPH on the fx-CG50).
    void reference(std::vector<Tok>& out){
        std::size_t q=p_+1;
        while(q<s_.size()&&std::isalpha(static_cast<unsigned char>(s_[q])))++q;
        while(q<s_.size()&&std::isdigit(static_cast<unsigned char>(s_[q])))++q;
        const std::string low=lower(s_.substr(p_+1,q-p_-1));
        if(!referenceName(low))throw SyntaxError("Syntax ERROR: riferimento a funzione non valido",static_cast<int>(p_),static_cast<int>(q-p_));
        out.push_back({TT::Ident,"@"+low,0,at(p_),at(q-p_)});p_=q;
    }
    void identifier(std::vector<Tok>& out){
        const std::size_t b=p_;
        while(p_<s_.size()&&(std::isalnum(static_cast<unsigned char>(s_[p_]))||s_[p_]=='_'))++p_;
        const std::string raw=s_.substr(b,p_-b);
        // fx-CG50 "Int÷" (integer quotient) is written as Int followed by the division sign.
        if(lower(raw)=="int"&&s_.compare(p_,2,"\xC3\xB7")==0){p_+=2;out.push_back({TT::Op,{},'q',at(b),at(p_-b)});return;}
        // Mat / Vct / List (esattamente con queste maiuscole, come li inserisce il
        // menu OPTN): riferimento a una memoria di matrice, vettore o lista. Il nome
        // che segue puo' essere attaccato alla parola ("MatA", "List1", "MatAns").
        // Le lettere minuscole digitate mantengono il loro significato (m*a*t resta
        // un prodotto).
        if(!isFunctionName(lower(raw))){
            for(const char* word:{"Mat","Vct","List"}){
                const std::size_t n=std::strlen(word);
                if(raw.compare(0,n,word)!=0)continue;
                out.push_back({TT::Ref,lower(std::string(word)),0,at(b),at(n)});
                // "List12": il numero attaccato alla parola, poi il resto.
                std::size_t k=n;
                while(k<raw.size()&&std::isdigit(static_cast<unsigned char>(raw[k])))++k;
                if(k>n)out.push_back({TT::Num,raw.substr(n,k-n),0,at(b+n),at(k-n)});
                if(k<raw.size())splitIdentifier(raw.substr(k),b+k,out);
                return;
            }
        }
        splitIdentifier(raw,b,out);
    }
    // Resolves an identifier, splitting e.g. "sinx" -> sin x, "2pix" -> pi x, "ab" -> a b.
    static void splitIdentifier(const std::string& raw,std::size_t base,std::vector<Tok>& out){
        const std::string low=lower(raw);
        if(const char op=operatorWord(low)){out.push_back({TT::Op,{},op,at(base),at(raw.size())});return;}
        if(raw.size()==1||isFunctionName(low)||isConstantName(low)){out.push_back({TT::Ident,raw.size()==1?raw:low,0,at(base),at(raw.size())});return;}
        std::size_t k=0;
        while(k<raw.size()){
            std::size_t best=0;
            for(std::size_t len=raw.size()-k;len>=2;--len){
                const std::string piece=lower(raw.substr(k,len));
                if(isFunctionName(piece)||isConstantName(piece)||operatorWord(piece)){best=len;break;}
            }
            if(best){
                const std::string word=lower(raw.substr(k,best));
                if(const char op=operatorWord(word))out.push_back({TT::Op,{},op,at(base+k),at(best)});
                else out.push_back({TT::Ident,word,0,at(base+k),at(best)});
                k+=best;continue;
            }
            if(std::isdigit(static_cast<unsigned char>(raw[k]))){
                std::size_t e=k;while(e<raw.size()&&std::isdigit(static_cast<unsigned char>(raw[e])))++e;
                out.push_back({TT::Num,raw.substr(k,e-k),0,at(base+k),at(e-k)});k=e;continue;
            }
            if(!std::isalpha(static_cast<unsigned char>(raw[k])))throw SyntaxError("Syntax ERROR: nome non valido '"+raw+"'",static_cast<int>(base+k),1);
            out.push_back({TT::Ident,raw.substr(k,1),0,at(base+k),1});++k;
        }
    }
    void utf8(std::vector<Tok>& out){
        struct Map{const char* seq;TT type;char op;const char* name;};
        static const Map maps[]={
            {"\xCF\x80",TT::Ident,0,"pi"},{"\xCE\xB8",TT::Ident,0,"theta"},{"\xC3\x97",TT::Op,'*',nullptr},
            {"\xC3\xB7",TT::Op,'/',nullptr},{"\xE2\x88\x92",TT::Op,'-',nullptr},{"\xC2\xB0",TT::Op,'d',nullptr},
            {"\xE2\x88\x9A",TT::Ident,0,"sqrt"},{"\xE2\x88\x9B",TT::Ident,0,"cbrt"},{"\xE2\x8B\x85",TT::Op,'*',nullptr},
            {"\xC2\xB7",TT::Op,'*',nullptr},{"\xE2\x89\xA0",TT::Op,'n',nullptr},{"\xE2\x89\xA4",TT::Op,'l',nullptr},
            {"\xE2\x89\xA5",TT::Op,'g',nullptr},
        };
        for(const auto& m:maps){const std::size_t n=std::strlen(m.seq);
            if(s_.compare(p_,n,m.seq)==0){out.push_back({m.type,m.name?m.name:"",m.op,at(p_),at(n)});p_+=n;return;}}
        const unsigned char c=static_cast<unsigned char>(s_[p_]);
        const int n=(c&0xE0)==0xC0?2:(c&0xF0)==0xE0?3:(c&0xF8)==0xF0?4:1;
        if(s_.compare(p_,3,"\xE2\x86\x92")==0)throw SyntaxError("Syntax ERROR: -> ammesso solo a fine espressione",static_cast<int>(p_),3);
        throw SyntaxError("Syntax ERROR: simbolo non supportato",static_cast<int>(p_),n);
    }
};

// ---------------------------------------------------------------- parser
class Parser {
public:
    explicit Parser(std::vector<Tok> t):t_(std::move(t)){
        // t_ always ends with an End token.
        if(t_.size()>MaxExpressionTokens+1)throw MathError("Memory ERROR: espressione troppo lunga",t_[MaxExpressionTokens].pos,t_[MaxExpressionTokens].len);
    }
    ast::NodePtr run(){
        if(t_.size()==1)throw SyntaxError("Syntax ERROR: espressione vuota",0,0);
        auto n=expr();
        if(peek().type!=TT::End){
            if(isOp(')'))throw error("Syntax ERROR: ) in eccesso");
            if(isOp(']')||isOp('}'))throw error(std::string("Syntax ERROR: ")+peek().op+" in eccesso");
            throw error("Syntax ERROR: caratteri extra");
        }
        return n;
    }
private:
    std::vector<Tok> t_; std::size_t i_{};
    int depth_{};
    // Every nested level (parentheses, function arguments, signs, exponents)
    // passes through unary() or powerOperand(); bounding it bounds the
    // recursion of the parser and of the evaluator.
    struct Nest {
        int& d;
        Nest(int& depth,const Tok& where):d(depth){
            if(++d>MaxExpressionNesting){--d;throw MathError("Stack ERROR: troppi livelli annidati",where.pos,where.len);}
        }
        ~Nest(){--d;}
        Nest(const Nest&)=delete;
        Nest& operator=(const Nest&)=delete;
    };
    const Tok& peek() const {return t_[i_];}
    bool isOp(char c) const {return peek().type==TT::Op&&peek().op==c;}
    bool eat(char c){if(isOp(c)){++i_;return true;}return false;}
    // Errore di sintassi sul token che il parser sta esaminando (la fine del
    // testo quando l'input si ferma troppo presto).
    SyntaxError error(const std::string& what) const {return SyntaxError(what,peek().pos,peek().len);}
    static SyntaxError errorAt(const std::string& what,const Tok& k){return SyntaxError(what,k.pos,k.len);}
    // Intervallo di byte di un nodo: dal primo token della produzione (indice s)
    // alla fine dell'ultimo token consumato.
    ast::NodePtr span(ast::NodePtr n,std::size_t s) const {
        const std::size_t first=std::min(s,t_.size()-1);
        const std::uint16_t begin=t_[first].pos;
        const std::uint16_t end=i_>first?static_cast<std::uint16_t>(t_[i_-1].pos+t_[i_-1].len):begin;
        n->pos=begin;n->len=static_cast<std::uint16_t>(end>begin?end-begin:0);
        return n;
    }
    static std::uint8_t flagsOf(const ast::Node& n){
        std::uint8_t f=0;
        for(const auto& c:n.children)if(c)f|=c->flags;
        return f;
    }
    ast::NodePtr bin(char op,ast::NodePtr a,ast::NodePtr b,std::size_t s) const {
        auto n=ast::make(ast::Kind::Binary,std::string(1,op));
        n->children={std::move(a),std::move(b)};
        n->flags=flagsOf(*n);
        return span(n,s);
    }
    bool startsAtom() const {
        const Tok& k=peek();
        return k.type==TT::Num||k.type==TT::Ident||k.type==TT::Ref||(k.type==TT::Op&&(k.op=='('||k.op=='['||k.op=='{'));
    }
    bool atFunction() const {return peek().type==TT::Ident&&isFunctionName(peek().text);}
    // Lowest priorities (fx-CG50 order): Or/Xor < And < relational operators < + -.
    ast::NodePtr expr(){
        const std::size_t s=i_;
        auto n=logicAnd();
        for(;;){
            if(eat('|'))n=bin('|',n,logicAnd(),s);
            else if(eat('x'))n=bin('x',n,logicAnd(),s);
            else if(eat('X'))n=bin('X',n,logicAnd(),s);      // Xnor (Base-N)
            else return n;
        }
    }
    ast::NodePtr logicAnd(){
        const std::size_t s=i_;
        auto n=relation();
        while(eat('&'))n=bin('&',n,relation(),s);
        return n;
    }
    ast::NodePtr relation(){
        const std::size_t s=i_;
        auto n=addSub();
        for(;;){
            bool matched=false;
            for(char op:{'=','n','<','>','l','g'})if(eat(op)){n=bin(op,n,addSub(),s);matched=true;break;}
            if(!matched)return n;
        }
    }
    ast::NodePtr addSub(){
        const std::size_t s=i_;
        auto n=mulDiv();
        for(;;){if(eat('+'))n=bin('+',n,mulDiv(),s);else if(eat('-'))n=bin('-',n,mulDiv(),s);else return n;}
    }
    ast::NodePtr mulDiv(){
        const std::size_t s=i_;
        auto n=implicitProduct();
        for(;;){
            if(eat('*'))n=bin('*',n,implicitProduct(),s);
            else if(eat('/'))n=bin('/',n,implicitProduct(),s);
            else if(eat('%'))n=bin('%',n,implicitProduct(),s);
            else if(eat('q'))n=bin('q',n,implicitProduct(),s);
            else if(eat('r'))n=bin('r',n,implicitProduct(),s);
            else return n;
        }
    }
    // Juxtaposition (2x, 2(3), x sin x) has higher priority than * and /.
    // stopAtFunction: the argument of a prefix function ends before the next
    // function name, so "sin x cos x" is sin(x)*cos(x).
    ast::NodePtr implicitProduct(bool stopAtFunction=false){
        const std::size_t s=i_;
        auto n=unary();
        while(startsAtom()&&!(stopAtFunction&&atFunction())){
            if(peek().type==TT::Num&&i_>0&&t_[i_-1].type==TT::Num)throw error("Syntax ERROR: numeri adiacenti");
            n=bin('*',n,unary(),s);
        }
        return n;
    }
    ast::NodePtr unary(){
        const Nest nest(depth_,peek());
        const std::size_t s=i_;
        if(eat('+'))return unary();
        if(eat('-')){auto n=ast::make(ast::Kind::Unary,"-");n->children={unary()};n->flags=flagsOf(*n);return span(n,s);}
        return power();
    }
    ast::NodePtr power(){
        const std::size_t s=i_;
        auto base=postfix();
        if(eat('^')){
            // Right-associative; the exponent may carry its own sign (2^-2).
            ast::NodePtr e;
            const std::size_t es=i_;
            if(eat('-')){e=ast::make(ast::Kind::Unary,"-");e->children={powerOperand()};e->flags=flagsOf(*e);e=span(e,es);}
            else{(void)eat('+');e=powerOperand();}
            return bin('^',base,e,s);
        }
        return base;
    }
    ast::NodePtr powerOperand(){const Nest nest(depth_,peek());return power();}
    ast::NodePtr postfix(){
        const std::size_t s=i_;
        auto n=atom();
        for(;;){
            if(eat('!')){auto c=ast::make(ast::Kind::Call,"factorial");c->children={n};c->flags=flagsOf(*c);n=span(c,s);}
            else if(eat('d')){auto c=ast::make(ast::Kind::Call,"fromdeg");c->children={n};c->flags=flagsOf(*c);n=span(c,s);}
            else if(isOp('#')){
                // Simbolo ingegneristico: 2k = 2×10³ (il fattore e' un token "#1e3").
                const std::size_t fs=i_;
                auto factor=ast::make(ast::Kind::Token,peek().text);
                ++i_;
                n=bin('*',n,span(factor,fs),s);
            }
            else return n;
        }
    }
    // [[a,b][c,d]]: formato di input delle matrici (e dei vettori) della fx-CG50, un gruppo [ ] per riga.
    ast::NodePtr matrixLiteral(){
        const std::size_t s=i_;
        ++i_;                                    // '[' esterna
        if(!isOp('['))throw error("Syntax ERROR: una matrice si scrive [[a,b][c,d]]");
        auto m=ast::make(ast::Kind::Matrix);
        std::size_t cols=0;
        while(isOp('[')){
            const Tok rowStart=peek();
            ++i_;
            std::size_t n=0;
            do{m->children.push_back(expr());++n;}while(eat(','));
            if(!eat(']')&&peek().type!=TT::End)throw error("Syntax ERROR: manca ] in una riga della matrice");
            if(cols==0)cols=n;
            else if(n!=cols)throw DimensionError("Dimension ERROR: le righe della matrice hanno lunghezze diverse",rowStart.pos,
                                                 static_cast<int>(t_[i_-1].pos+t_[i_-1].len)-rowStart.pos);
        }
        // Come per le parentesi, la quadra di chiusura puo' mancare alla fine dell'input.
        if(!eat(']')&&peek().type!=TT::End)throw error("Syntax ERROR: manca ] alla fine della matrice");
        m->text=std::to_string(cols);
        m->flags=ast::FlagCollection;
        return span(m,s);
    }
    // {a,b,c}: lista letterale.
    ast::NodePtr listLiteral(){
        const std::size_t s=i_;
        ++i_;                                    // '{'
        if(isOp('}'))throw error("Syntax ERROR: lista vuota");
        auto l=ast::make(ast::Kind::List);
        do{l->children.push_back(expr());}while(eat(','));
        if(!eat('}')&&peek().type!=TT::End)throw error("Syntax ERROR: manca } alla fine della lista");
        l->flags=ast::FlagCollection;
        return span(l,s);
    }
    // Mat A, Vct B, List 3, Mat Ans, Vct Ans, List Ans, con un indice facoltativo
    // dell'elemento: Mat A[2,1], Vct B[3], List 1[4].
    ast::NodePtr memoryReference(){
        const std::size_t s=i_;
        const Tok word=peek();
        ++i_;
        const Tok& k=peek();
        std::string name=word.text=="mat"?"Mat ":word.text=="vct"?"Vct ":"List ";
        if(k.type==TT::Ident&&k.text=="ans"){name+="Ans";++i_;}
        else if(word.text=="list"){
            bool digits=k.type==TT::Num&&!k.text.empty()&&k.text.size()<=2;
            if(digits)for(char c:k.text)digits=digits&&std::isdigit(static_cast<unsigned char>(c));
            const int index=digits?std::atoi(k.text.c_str()):0;
            if(index<1||index>CollectionResolver::Slots)throw errorAt("Syntax ERROR: dopo List serve un numero 1-26 o Ans",k);
            // List 1-26 usano il token delle variabili di lista condivise ("list12"),
            // lo stesso usato da GRAPH, TABLE e dai nomi di VARS.
            name="list"+std::to_string(index);
            ++i_;
        }else{
            if(k.type!=TT::Ident||k.text.size()!=1||!std::isalpha(static_cast<unsigned char>(k.text[0])))
                throw errorAt("Syntax ERROR: dopo "+name.substr(0,3)+" serve una lettera A-Z o Ans",k);
            name+=static_cast<char>(std::toupper(static_cast<unsigned char>(k.text[0])));++i_;
        }
        auto ref=ast::make(ast::Kind::Token,name);
        ref->flags=ast::FlagCollection;
        ref=span(ref,s);
        // "[" seguita da "[" inizia una matrice letterale (prodotto implicito), non un indice.
        if(isOp('[')&&!(t_[i_+1].type==TT::Op&&t_[i_+1].op=='[')){
            ++i_;
            auto call=ast::make(ast::Kind::Call,"elem");
            call->children={ref,expr()};
            if(eat(','))call->children.push_back(expr());
            if(!eat(']')&&peek().type!=TT::End)throw error("Syntax ERROR: manca ] dopo l'indice");
            call->flags=ast::FlagCollection;
            return span(call,s);
        }
        return ref;
    }
    ast::NodePtr atom(){
        const std::size_t s=i_;
        const Tok k=peek();
        if(k.type==TT::Num){++i_;return span(ast::make(ast::Kind::Token,k.text),s);}
        if(k.type==TT::Op&&k.op=='('){
            ++i_;
            if(isOp(')'))throw error("Syntax ERROR: parentesi vuote");
            auto inner=expr();
            // Casio allows omitting closing parentheses at the end of the input.
            if(!eat(')')&&peek().type!=TT::End)throw error("Syntax ERROR: manca )");
            auto g=ast::make(ast::Kind::Group);g->children={inner};g->flags=flagsOf(*g);return span(g,s);
        }
        if(k.type==TT::Op&&k.op=='[')return matrixLiteral();
        if(k.type==TT::Op&&k.op=='{')return listLiteral();
        if(k.type==TT::Ref)return memoryReference();
        if(k.type==TT::Ident){
            ++i_;
            if(!k.text.empty()&&k.text[0]=='@'){
                // Y1, Y1(x), Y1(Y2), Y1(A=2): optional arguments in parentheses.
                auto call=ast::make(ast::Kind::Call,k.text);
                if(eat('(')){
                    if(!isOp(')')){do{call->children.push_back(expr());}while(eat(','));}
                    if(!eat(')')&&peek().type!=TT::End)throw error("Syntax ERROR: manca ) in un riferimento");
                }
                if(call->children.size()>3)throw errorAt("Argument ERROR: troppi argomenti nel riferimento",k);
                call->flags=flagsOf(*call);
                return span(call,s);
            }
            if(isFunctionName(k.text)){
                auto call=ast::make(ast::Kind::Call,k.text);
                const FnInfo* info=findFunction(k.text);
                const unsigned minArgs=info?info->minArgs:1,maxArgs=info?info->maxArgs:1;
                if(eat('(')){
                    if(!isOp(')')){
                        do{call->children.push_back(expr());}while(eat(','));
                    }
                    if(!eat(')')&&peek().type!=TT::End)throw error("Syntax ERROR: manca ) in "+k.text);
                }else{
                    if(minArgs==0){/* rand */}
                    else if(minArgs>1)throw errorAt("Syntax ERROR: "+k.text+" richiede parentesi",k);
                    else call->children.push_back(implicitProduct(true));
                }
                const std::size_t count=call->children.size();
                // Sum / Prod di una lista hanno un argomento, i calcoli Σ / Π tre.
                const bool sumLike=k.text=="sum"||k.text=="prod";
                if(count<minArgs||count>maxArgs||(sumLike&&count==2))
                    throw SyntaxError("Argument ERROR: "+k.text+" numero di argomenti errato",k.pos,
                                      static_cast<int>(t_[i_-1].pos+t_[i_-1].len)-k.pos);
                call->flags=flagsOf(*call);
                if(isCollectionFunction(k.text,count))call->flags|=ast::FlagCollection;
                return span(call,s);
            }
            auto token=ast::make(ast::Kind::Token,k.text);
            if(isListName(k.text))token->flags=ast::FlagCollection;
            return span(token,s);
        }
        if(k.type==TT::End)throw error("Syntax ERROR: operando mancante");
        if(k.type==TT::Op&&k.op==')')throw error("Syntax ERROR: ) inattesa");
        throw error(std::string("Syntax ERROR: operatore '")+k.op+"' inatteso");
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
// ---- Base-N (fx-CG50 Mode Dec/Hex/Bin/Oct): interi a 32 bit in complemento a due.
long long int32Arg(double v){
    if(!isInteger(v)||v<-2147483648.0||v>2147483647.0)throw MathError("Math ERROR: in Base-N i valori sono interi a 32 bit");
    return static_cast<long long>(v);
}
double int32Result(long long v){
    if(v<-2147483648ll||v>2147483647ll)throw MathError("Math ERROR: risultato oltre i 32 bit (Base-N)");
    return static_cast<double>(v);
}
double bitsValue(std::uint32_t u){return static_cast<double>(static_cast<std::int32_t>(u));}
double baseNBinary(char op,double a,double b){
    const long long x=int32Arg(a),y=int32Arg(b);
    const auto ux=static_cast<std::uint32_t>(static_cast<std::int32_t>(x)),uy=static_cast<std::uint32_t>(static_cast<std::int32_t>(y));
    switch(op){
    case '+': return int32Result(x+y);
    case '-': return int32Result(x-y);
    case '*': return int32Result(x*y);
    case '/': case 'q':
        if(y==0)throw MathError("Math ERROR: divisione per zero");
        return int32Result(x/y);                    // divisione intera: la parte frazionaria si scarta
    case 'r': if(y==0)throw MathError("Math ERROR: divisione per zero");return int32Result(x%y);
    case '&': return bitsValue(ux&uy);
    case '|': return bitsValue(ux|uy);
    case 'x': return bitsValue(ux^uy);
    case 'X': return bitsValue(~(ux^uy));
    case '=': case 'n': case '<': case '>': case 'l': case 'g':{
        const bool v=op=='='?x==y:op=='n'?x!=y:op=='<'?x<y:op=='>'?x>y:op=='l'?x<=y:x>=y;
        return v?1.0:0.0;
    }
    default: throw MathError("Math ERROR: operazione non disponibile in Base-N");
    }
}

// "a⌟b" = a/b, "a⌟b⌟c" = a + b/c (frazione mista della modalita' Line).
bool fractionLiteral(const std::string& t,exact::Rational& out){
    long long part[3]={0,0,0};int count=0;
    std::size_t i=0;
    while(i<=t.size()&&count<3){
        std::size_t e=i;long long v=0;
        while(e<t.size()&&std::isdigit(static_cast<unsigned char>(t[e]))){
            if(v>99999999999999ll)return false;
            v=v*10+(t[e]-'0');++e;
        }
        if(e==i)return false;
        part[count++]=v;
        if(e>=t.size())break;
        if(t.compare(e,3,kFractionMark)!=0)return false;
        i=e+3;
    }
    if(count==2)return exact::Rational::make(part[0],part[1],out);
    if(count==3){
        exact::Rational f;
        if(!exact::Rational::make(part[1],part[2],f))return false;
        return exact::add(exact::Rational::integer(part[0]),f,out);
    }
    return false;
}

// Evaluation environment with a stack of calculus bindings that shadow the context.
struct Env {
    static constexpr int MaxLocal=16;
    const EvalContext& ctx;
    Binding local[MaxLocal]{};
    int localCount{};
    std::uint8_t depth{};
    std::uint8_t refDepth{};   // nested graph-function references
    std::uintptr_t stackBase{};
    bool decimal{};            // exact evaluation met a plain decimal literal
    // Nodo dell'espressione analizzata la cui operazione e' in corso: quando
    // viene lanciato un errore il suo intervallo arriva a EvalInfo::error.
    const ast::Node* at{};
};
// Copia in EvalInfo l'intervallo della sotto-espressione che non riesce
// mentre l'eccezione risale (senza catch/rethrow: chi campiona funzioni in un
// ciclo, come GRAPH, non paga nulla).
struct ErrorLocationGuard {
    Env& env;
    int pending;
    explicit ErrorLocationGuard(Env& e):env(e),pending(std::uncaught_exceptions()){}
    ~ErrorLocationGuard(){
        if(env.ctx.info&&std::uncaught_exceptions()>pending&&env.at&&env.at->len>0)
            env.ctx.info->error={static_cast<int>(env.at->pos),static_cast<int>(env.at->len)};
    }
};
// Mentre si valuta il corpo di una funzione di GRAPH (un altro testo), un
// errore viene segnalato sul riferimento nell'espressione calcolata.
struct ErrorNodeGuard {
    Env& env;
    const ast::Node* node;
    int pending;
    ErrorNodeGuard(Env& e,const ast::Node& n):env(e),node(&n),pending(std::uncaught_exceptions()){}
    ~ErrorNodeGuard(){if(std::uncaught_exceptions()>pending)env.at=node;}
};
// Current stack position. The frame address (not a local's address) is used so
// that AddressSanitizer's fake stack does not distort the measurement.
FXAI_NOINLINE std::uintptr_t stackPosition() noexcept {
    return reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
}
void checkStack(const Env& env){
    const std::uintptr_t here=stackPosition();
    // The stack grows downwards on every supported target.
    if(env.stackBase>here&&env.stackBase-here>gEvalStackBudget)
        throw MathError("Stack ERROR: calcolo troppo annidato");
}
struct LocalBind {
    Env& env;
    LocalBind(Env& e,const char* name,double v):env(e){
        if(env.localCount>=Env::MaxLocal)throw MathError("Stack ERROR: troppi livelli di calcolo annidati");
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

// Calculus functions evaluate their body lazily with x bound; env.depth limits
// how many of them can be nested.
template<typename T> FXAI_NOINLINE T callCalculus(const ast::Node& n,Env& env){
    const std::string& f=n.text;
    auto arg=[&](std::size_t i)->T{return evalNode<T>(*n.children.at(i),env);};
    auto rarg=[&](std::size_t i)->double{return asReal<T>(arg(i),f.c_str());};
    auto R=[](double v){return fromReal<T>(v);};

    // ---- calculus (lazy: the first child is evaluated with x bound)
    if(f=="integral"||f=="integrate"||f=="derivative"||f=="diff"||f=="derivative2"||f=="sum"||f=="prod"||f=="solve"){
        if(++env.depth>4){--env.depth;throw MathError("Stack ERROR: calcoli annidati troppo profondi");}
        struct DepthGuard{Env& e;~DepthGuard(){--e.depth;}} guard{env};
        const ast::Node& body=*n.children.at(0);
        if(env.ctx.info&&f!="sum"&&f!="prod")env.ctx.info->numerical=true;
        LocalBind bind(env,"x",0.0);
        auto fx=[&](double v)->double{bind.set(v);const T r=evalNode<T>(body,env);env.at=&n;return asReal<T>(r,"funzione integranda");};
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
        env.at=&n;
        return acc;
    }
    throw SyntaxError("Syntax ERROR: funzione sconosciuta "+f);
}

// Non-recursive part of a function call: the arguments are already evaluated,
// so the large frame of this function is never live across the recursion.
template<typename T> FXAI_NOINLINE T applyFunction(const std::string& f,const T* args,std::size_t count){
    auto arg=[&](std::size_t i)->T{if(i>=count)throw SyntaxError("Argument ERROR: "+f+" numero di argomenti errato");return args[i];};
    auto rarg=[&](std::size_t i)->double{return asReal<T>(arg(i),f.c_str());};
    auto R=[](double v){return fromReal<T>(v);};

    // ---- zero-argument
    if(f=="rand"||f=="ran")return R(static_cast<double>(nextRandom()>>8)/16777216.0);

    // ---- complex-specific
    if(f=="re"||f=="real"){if constexpr(std::is_same_v<T,double>)return arg(0);else return R(arg(0).real());}
    if(f=="im"||f=="imag"){if constexpr(std::is_same_v<T,double>){(void)arg(0);return 0.0;}else return R(arg(0).imag());}
    if(f=="conj"){if constexpr(std::is_same_v<T,double>)return arg(0);else return std::conj(arg(0));}
    if(f=="arg"){if constexpr(std::is_same_v<T,double>){const double v=arg(0);if(v==0)throw MathError("Math ERROR: argomento di zero");return fromRadians(v<0?kPi:0.0);}
        else{const Complex z=arg(0);if(std::abs(z)==0)throw MathError("Math ERROR: argomento di zero");return R(fromRadians(std::arg(z)));}}

    // ---- one argument
    if(count==1){
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
        if(f=="neg")return T(0)-a;
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
        if(f=="not")return R(v==0?1.0:0.0);
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
    if(f=="mixed"){
        // Mixed fraction w n/d: the sign of the whole part applies to the fraction (-2 1/3 = -(2+1/3)).
        const T w=arg(0),num=arg(1),den=arg(2);
        if(den==T(0))throw MathError("Math ERROR: denominatore nullo");
        const double ws=asReal<T>(w,"frazione mista");
        return ws<0?w-num/den:w+num/den;
    }
    if(f=="modexp"){
        // fx-CG50 MOD_Exp(n,p,m) = n^p mod m by square-and-multiply on exact integers.
        const long long nb=toIntArg(rarg(0),"MOD_Exp"),pe=toIntArg(rarg(1),"MOD_Exp"),m=toIntArg(rarg(2),"MOD_Exp");
        if(m<=0||pe<0||nb<0)throw MathError("Argument ERROR: MOD_Exp(n,p,m) con n,p>=0 e m>0");
        auto mulmod=[m](unsigned long long x,unsigned long long y){unsigned long long res=0;x%=static_cast<unsigned long long>(m);
            while(y){if(y&1)res=(res+x)%static_cast<unsigned long long>(m);x=(x*2)%static_cast<unsigned long long>(m);y>>=1;}return res;};
        unsigned long long result=1%static_cast<unsigned long long>(m),base=static_cast<unsigned long long>(nb)%static_cast<unsigned long long>(m);
        for(unsigned long long e=static_cast<unsigned long long>(pe);e;e>>=1){if(e&1)result=mulmod(result,base);base=mulmod(base,base);}
        return R(static_cast<double>(result));
    }
    if(f=="dms"){
        const double d=rarg(0),m=rarg(1),s=count>2?rarg(2):0.0;
        const double sign=(d<0||m<0||s<0)?-1.0:1.0;
        return R(sign*(std::abs(d)+std::abs(m)/60.0+std::abs(s)/3600.0));
    }
    const double a=rarg(0),b=rarg(1);
    if(f=="min")return R(std::min(a,b));
    if(f=="max")return R(std::max(a,b));
    if(f=="round"){const long long dg=toIntArg(b,"round");if(dg<0||dg>14)throw MathError("Argument ERROR: cifre 0..14");const double p=std::pow(10.0,static_cast<double>(dg));return R(std::round(a*p)/p);}
    if(f=="rndfix"){const long long dg=toIntArg(b,"RndFix");if(dg<0||dg>9)throw MathError("Argument ERROR: RndFix cifre 0..9");const double p=std::pow(10.0,static_cast<double>(dg));return R(std::round(a*p)/p);}
    if(f=="rannorm"){
        // RanNorm#(sigma, mu): Box-Muller transform of two uniform numbers in (0,1].
        if(a<=0)throw MathError("Argument ERROR: RanNorm# richiede sigma>0");
        const double u1=(static_cast<double>(nextRandom()>>8)+1.0)/16777217.0,u2=static_cast<double>(nextRandom()>>8)/16777216.0;
        return R(b+a*std::sqrt(-2.0*std::log(u1))*std::cos(2.0*kPi*u2));
    }
    if(f=="ranbin"){
        const long long trials=toIntArg(a,"RanBin#");
        if(trials<1||trials>100000||b<0||b>1)throw MathError("Argument ERROR: RanBin#(n,p) con 1<=n<=100000, 0<=p<=1");
        long long hits=0;
        for(long long k=0;k<trials;++k)if(static_cast<double>(nextRandom()>>8)/16777216.0<b)++hits;
        return R(static_cast<double>(hits));
    }
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

bool isLazyFunction(const std::string& f){
    return f=="integral"||f=="integrate"||f=="derivative"||f=="diff"||f=="derivative2"||f=="sum"||f=="prod"||f=="solve";
}
// "Y1" as written by the user (for messages).
std::string referenceLabel(const std::string& text){
    std::string s=text.size()>1?text.substr(1):text;
    if(!s.empty())s[0]=static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}
// Graph function reference: positional argument -> the function's own variable,
// "A=v" arguments -> temporary value of a coefficient (memory is not changed).
template<typename T> FXAI_NOINLINE T callReference(const ast::Node& n,Env& env){
    const char* var=nullptr;
    const ast::Node* body=env.ctx.functions?env.ctx.functions->function(n.text.substr(1),var):nullptr;
    if(!body||!var)throw MathError("Math ERROR: "+referenceLabel(n.text)+" non definita");
    // Every reference counts, also those nested in the arguments: Y1(Y2(Y3(Y4(Y5)))) is the limit.
    if(env.refDepth>=MaxFunctionReferenceDepth)throw MathError("Stack ERROR: piu' di 5 funzioni composte");
    ++env.refDepth;
    struct DepthGuard{Env& e;~DepthGuard(){--e.refDepth;}} guard{env};
    const ErrorNodeGuard errorNode(env,n);
    char names[3][2]={};double values[3]={};int assigned=0;
    double positional=0;bool hasPositional=false;
    for(const auto& c:n.children){
        if(!c)throw SyntaxError("Syntax ERROR: argomento mancante");
        const ast::Node* a=c.get();
        while(a->kind==ast::Kind::Group&&a->children.size()==1&&a->children[0])a=a->children[0].get();
        const ast::Node* lhs=a->kind==ast::Kind::Binary&&a->text=="="&&a->children.size()==2?a->children[0].get():nullptr;
        if(lhs&&lhs->kind==ast::Kind::Token&&lhs->text.size()==1&&std::isalpha(static_cast<unsigned char>(lhs->text[0]))){
            names[assigned][0]=static_cast<char>(std::tolower(static_cast<unsigned char>(lhs->text[0])));
            values[assigned]=asReal<T>(evalNode<T>(*a->children[1],env),"coefficiente");
            ++assigned;continue;
        }
        if(hasPositional)throw SyntaxError("Argument ERROR: "+referenceLabel(n.text)+" ha un solo argomento");
        positional=asReal<T>(evalNode<T>(*c,env),"argomento");hasPositional=true;
    }
    if(!hasPositional&&!lookupBinding(env,var,positional))
        throw MathError(std::string("Math ERROR: variabile ")+var+" non definita per "+referenceLabel(n.text));
    LocalBind b0(env,var,positional);
    // Assignments shadow the memory variables only while the body is evaluated.
    if(assigned==0)return evalNode<T>(*body,env);
    LocalBind b1(env,names[0],values[0]);
    if(assigned==1)return evalNode<T>(*body,env);
    LocalBind b2(env,names[1],values[1]);
    if(assigned==2)return evalNode<T>(*body,env);
    LocalBind b3(env,names[2],values[2]);
    return evalNode<T>(*body,env);
}
template<typename T> FXAI_NOINLINE T callFunction(const ast::Node& n,Env& env){
    if(env.ctx.integerBase){
        // Base-N: solo Not (complemento a uno), Neg (complemento a due) e Abs.
        const std::string& f=n.text;
        if(n.children.size()!=1||!n.children[0]||(f!="not"&&f!="neg"&&f!="abs"))
            throw MathError("Math ERROR: "+f+" non disponibile in Base-N");
        const long long x=int32Arg(asReal<T>(evalNode<T>(*n.children[0],env),f.c_str()));
        env.at=&n;
        if(f=="not")return fromReal<T>(bitsValue(~static_cast<std::uint32_t>(static_cast<std::int32_t>(x))));
        return fromReal<T>(int32Result(f=="neg"?-x:(x<0?-x:x)));
    }
    if(!n.text.empty()&&n.text[0]=='@')return callReference<T>(n,env);
    if(isLazyFunction(n.text))return callCalculus<T>(n,env);
    const std::size_t count=n.children.size();
    if(count>3)throw SyntaxError("Argument ERROR: "+n.text+" numero di argomenti errato");
    T args[3]{};
    for(std::size_t i=0;i<count;++i){
        if(!n.children[i])throw SyntaxError("Syntax ERROR: argomento mancante");
        args[i]=evalNode<T>(*n.children[i],env);
    }
    env.at=&n;
    return applyFunction<T>(n.text,args,count);
}

template<typename T> FXAI_NOINLINE T evalToken(const ast::Node& n,Env& env){
        if(n.text.empty())throw SyntaxError("Syntax ERROR: token vuoto");
        const unsigned char c0=static_cast<unsigned char>(n.text[0]);
        if(c0=='#'){char* e=nullptr;const double v=std::strtod(n.text.c_str()+1,&e);return fromReal<T>(v);}   // simbolo ingegneristico
        if(c0=='-'&&n.text.size()>1&&std::isdigit(static_cast<unsigned char>(n.text[1])))return fromReal<T>(std::strtod(n.text.c_str(),nullptr));   // Base-N negativo
        if(std::isdigit(c0)&&n.text.find(kFractionMark)!=std::string::npos){
            exact::Rational r;
            if(!fractionLiteral(n.text,r))throw MathError("Math ERROR: denominatore nullo o frazione troppo grande");
            return fromReal<T>(r.toDouble());
        }
        if(std::isdigit(c0)||c0=='.'){
            if(n.text.find('_')!=std::string::npos){
                exact::Rational r;
                if(!exact::parseDecimal(n.text,r))throw SyntaxError("Syntax ERROR: numero periodico non valido");
                return fromReal<T>(r.toDouble());
            }
            char* e=nullptr;const double v=std::strtod(n.text.c_str(),&e);if(e!=n.text.c_str()+n.text.size())throw SyntaxError("Syntax ERROR: numero non valido");return fromReal<T>(v);
        }
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
        if(isSystemName(low)){
            double v=0;
            if(env.ctx.names&&env.ctx.names->resolve(low,v))return fromReal<T>(v);
            throw MathError("Math ERROR: "+n.text+" non disponibile qui");
        }
        throw SyntaxError("Syntax ERROR: nome sconosciuto "+n.text);
}

template<typename T> FXAI_NOINLINE T applyBinary(char op,const T& a,const T& b){
        T r{};
        switch(op){
        case '+': r=a+b;break;
        case '-': r=a-b;break;
        case '*': r=a*b;break;
        case '/': if(b==T(0))throw MathError("Math ERROR: divisione per zero");r=a/b;break;
        case '%':{const double x=asReal<T>(a,"%"),y=asReal<T>(b,"%");if(y==0)throw MathError("Math ERROR: modulo per zero");r=fromReal<T>(std::fmod(x,y));break;}
        case 'q': case 'r':{
            // fx-CG50 Int÷ / Rmdr: integer quotient and remainder (sign of the dividend).
            const double x=asReal<T>(a,"Int\xC3\xB7"),y=asReal<T>(b,"Rmdr");
            if(!isInteger(x)||!isInteger(y)||std::abs(x)>9e15||std::abs(y)>9e15)throw MathError("Argument ERROR: Int\xC3\xB7 e Rmdr richiedono interi");
            if(y==0)throw MathError("Math ERROR: divisione per zero");
            const double qv=std::trunc(x/y);
            r=fromReal<T>(op=='q'?qv:x-qv*y);break;
        }
        case '=': case 'n': case '<': case '>': case 'l': case 'g':{
            bool v=false;
            if(op=='='||op=='n'){v=(a==b);if(op=='n')v=!v;}
            else{
                const double x=asReal<T>(a,"confronto"),y=asReal<T>(b,"confronto");
                v=op=='<'?x<y:op=='>'?x>y:op=='l'?x<=y:x>=y;
            }
            r=fromReal<T>(v?1.0:0.0);break;
        }
        case '&': case '|': case 'x': case 'X':{
            const bool x=!(a==T(0)),y=!(b==T(0));
            r=fromReal<T>((op=='&'?(x&&y):op=='|'?(x||y):op=='x'?(x!=y):(x==y))?1.0:0.0);break;
        }
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

// Operazione binaria nel dominio del contesto: interi a 32 bit nella modalita'
// Base-N, altrimenti le regole numeriche di sempre.
template<typename T> T combine(const Env& env,char op,const T& a,const T& b){
    if constexpr(std::is_same_v<T,double>){if(env.ctx.integerBase)return baseNBinary(op,a,b);}
    else{if(env.ctx.integerBase)return Complex(baseNBinary(op,requireReal(a,"Base-N"),requireReal(b,"Base-N")),0.0);}
    return applyBinary<T>(op,a,b);
}

// Left-associative chains (1+2+3+..., 2*x*y) form left-deep trees. They are
// walked iteratively so that the recursion depth follows the nesting of the
// input, not its length.
bool isChainNode(const ast::Node& n) noexcept {
    return n.kind==ast::Kind::Binary&&n.text.size()==1&&std::strchr("+-*/%qr=n<>lg&|xX",n.text[0])!=nullptr&&n.children.size()==2&&n.children[0]&&n.children[1];
}
template<typename T> FXAI_NOINLINE T evalChain(const ast::Node& top,Env& env){
    constexpr std::size_t kChunk=16;
    const ast::Node* spine[kChunk];
    std::size_t count=0;
    const ast::Node* cur=&top;
    while(count<kChunk&&isChainNode(*cur)){spine[count++]=cur;cur=cur->children[0].get();}
    T acc=isChainNode(*cur)?evalChain<T>(*cur,env):evalNode<T>(*cur,env);
    while(count>0){
        const ast::Node& b=*spine[--count];
        const T rhs=evalNode<T>(*b.children[1],env);
        env.at=&b;
        acc=combine<T>(env,b.text[0],acc,rhs);
    }
    return acc;
}

// Sotto-espressione con matrici, vettori o liste che qui deve dare un numero
// (Det Mat A, Mat A[1,2], DotP(...)): la valuta il valutatore delle collezioni
// (definito sotto); TypeError quando il risultato non e' un numero.
Complex evalCollectionScalar(const ast::Node& n,Env& env);
bool exactCollectionScalar(const ast::Node& n,Env& env,exact::Value& out);

template<typename T> T evalNode(const ast::Node& node,Env& env){
    using ast::Kind;
    checkStack(env);
    const ast::Node* np=&node;
    while(np->kind==Kind::Group){
        if(np->children.size()!=1||!np->children[0])throw SyntaxError("Syntax ERROR: gruppo vuoto");
        np=np->children[0].get();
    }
    const ast::Node& n=*np;
    if(n.flags&ast::FlagCollection){
        const Complex z=evalCollectionScalar(n,env);
        if constexpr(std::is_same_v<T,double>)return requireReal(z,"il calcolo");
        else return z;
    }
    switch(n.kind){
    case Kind::Token:{
        env.at=&n;
        const T v=evalToken<T>(n,env);
        if(env.ctx.integerBase)(void)int32Arg(asReal<T>(v,"Base-N"));      // Base-N: solo interi a 32 bit
        return v;
    }
    case Kind::Unary:{
        const T a=evalNode<T>(*n.children.at(0),env);
        if(n.text!="-")return a;
        if(env.ctx.integerBase){env.at=&n;return fromReal<T>(int32Result(-int32Arg(asReal<T>(a,"Base-N"))));}
        if constexpr(std::is_same_v<T,double>)return -a;
        else{
            // Negating 4+0i must give -4+0i (not -4-0i): a signed zero imaginary
            // part would put sqrt(-4) and ln(-1) on the wrong side of the branch cut.
            const double re=-a.real(),im=-a.imag();
            return Complex(re==0.0?0.0:re,im==0.0?0.0:im);
        }
    }
    case Kind::Binary:{
        if(isChainNode(n))return evalChain<T>(n,env);
        const T a=evalNode<T>(*n.children.at(0),env);
        const T b=evalNode<T>(*n.children.at(1),env);
        env.at=&n;
        return combine<T>(env,n.text.empty()?'?':n.text[0],a,b);
    }
    case Kind::Call:{T r=callFunction<T>(n,env);env.at=&n;checkFinite(r);return r;}
    default: break;
    }
    throw SyntaxError("Syntax ERROR: nodo non valutabile");
}

// ---------------------------------------------------------------- exact evaluation
// Second walk over the same parsed AST with exact::Value. Every helper returns
// false as soon as a step has no exact form; the frames on the recursive path
// (exactNode, exactChain, exactCall) hold at most a few values, and the large
// function table (exactApply) is never live across the recursion.
using exact::Rational;
using exact::Value;

bool exactStackOk(const Env& env){
    const std::uintptr_t here=stackPosition();
    return !(env.stackBase>here&&env.stackBase-here>gEvalStackBudget);
}
bool exactInteger(const Value& v,long long& out){
    Rational r;
    if(!v.rational(r)||!r.isInteger())return false;
    out=r.num;return true;
}
Value exactInt(long long v){return Value::fromInteger(v);}
// Angle argument converted to degrees (Deg: rational, Rad: rational multiple of π, Gra: rational gon).
bool exactToDegrees(const Value& v,Rational& deg){
    if(v.isZero()){deg={};return true;}
    switch(angleMode()){
    case AngleMode::Degrees: return v.rational(deg);
    case AngleMode::Gradians:{Rational g;return v.rational(g)&&exact::mul(g,{9,10},deg);}
    case AngleMode::Radians:
        if(!v.isPi())return false;
        return exact::mul(v.term(0).coef,Rational::integer(180),deg);
    }
    return false;
}
bool exactFromDegrees(const Rational& deg,Value& out){
    switch(angleMode()){
    case AngleMode::Degrees: out=Value::fromRational(deg);return true;
    case AngleMode::Gradians:{Rational g;if(!exact::mul(deg,{10,9},g))return false;out=Value::fromRational(g);return true;}
    case AngleMode::Radians:{Rational c;if(!exact::div(deg,Rational::integer(180),c))return false;out=Value::piTimes(c);return true;}
    }
    return false;
}
Value surdTerm(Rational coef,long long radicand){
    const exact::Term t{coef,radicand};
    Value v;(void)Value::fromTerms(&t,1,v);return v;
}
// sin of an integer number of degrees that is a multiple of 30 or 45.
bool exactSinDegrees(const Rational& deg,Value& out){
    if(!deg.isInteger())return false;
    const long long d=((deg.num%360)+360)%360;
    switch(d){
    case 0: case 180: out=Value{};return true;
    case 90: out=exactInt(1);return true;
    case 270: out=exactInt(-1);return true;
    case 30: case 150: out=Value::fromRational({1,2});return true;
    case 210: case 330: out=Value::fromRational({-1,2});return true;
    case 45: case 135: out=surdTerm({1,2},2);return true;
    case 225: case 315: out=surdTerm({-1,2},2);return true;
    case 60: case 120: out=surdTerm({1,2},3);return true;
    case 240: case 300: out=surdTerm({-1,2},3);return true;
    default: return false;
    }
}
// Inverse sine / tangent of the special values, in degrees.
bool exactAsinDegrees(const Value& v,Rational& deg){
    const struct {Value value;long long degrees;} table[]={
        {Value{},0},{exactInt(1),90},{exactInt(-1),-90},{Value::fromRational({1,2}),30},{Value::fromRational({-1,2}),-30},
        {surdTerm({1,2},2),45},{surdTerm({-1,2},2),-45},{surdTerm({1,2},3),60},{surdTerm({-1,2},3),-60}};
    for(const auto& e:table)if(e.value==v){deg=Rational::integer(e.degrees);return true;}
    return false;
}
bool exactAtanDegrees(const Value& v,Rational& deg){
    const struct {Value value;long long degrees;} table[]={
        {Value{},0},{exactInt(1),45},{exactInt(-1),-45},{surdTerm({1,3},3),30},{surdTerm({-1,3},3),-30},
        {surdTerm({1,1},3),60},{surdTerm({-1,1},3),-60}};
    for(const auto& e:table)if(e.value==v){deg=Rational::integer(e.degrees);return true;}
    return false;
}
// floor() of an exact value: exact for rationals; for irrational surds the
// floating value is used only when it is far from an integer (an irrational
// surd is never an integer, so the floor is then certain).
bool exactFloor(const Value& v,long long& out){
    Rational r;
    if(v.rational(r)){
        long long q=r.num/r.den;
        if(r.num%r.den!=0&&r.num<0)--q;
        out=q;return true;
    }
    const double d=v.toDouble();
    if(!std::isfinite(d)||std::abs(d)>9e15||std::abs(d-std::nearbyint(d))<1e-6)return false;
    out=static_cast<long long>(std::floor(d));return true;
}
bool exactRoundDigits(const Value& v,long long digits,Value& out){
    Rational r,scaled,half{1,2};
    if(!v.rational(r)||digits<0||digits>14)return false;
    long long p10=1;for(long long k=0;k<digits;++k)p10*=10;
    const bool negative=r.num<0;
    if(negative)r.num=-r.num;
    if(!exact::mul(r,Rational::integer(p10),scaled)||!exact::add(scaled,half,scaled))return false;
    const long long q=scaled.num/scaled.den;
    Rational res;
    if(!Rational::make(negative?-q:q,p10,res))return false;
    out=Value::fromRational(res);return true;
}
bool exactCompare(const Value& a,const Value& b,int& sign){
    Value d;
    if(!exact::sub(a,b,d))return false;
    sign=d.sign();return true;
}
bool exactPow(const Value& a,const Value& b,Value& out){
    Rational e;
    if(!b.rational(e))return false;
    if(a.isZero()&&e.sign()<=0)return e.isZero()?(out=exactInt(1),true):false;
    return exact::powRational(a,e,out);
}
FXAI_NOINLINE bool exactBinary(char op,const Value& a,const Value& b,Value& out){
    switch(op){
    case '+': return exact::add(a,b,out);
    case '-': return exact::sub(a,b,out);
    case '*': return exact::mul(a,b,out);
    case '/': return exact::div(a,b,out);
    case '^': return exactPow(a,b,out);
    case '%': case 'q': case 'r':{
        Rational x,y,q;
        if(!a.rational(x)||!b.rational(y)||y.isZero())return false;
        if(op!='%'&&(!x.isInteger()||!y.isInteger()))return false;
        if(!exact::div(x,y,q))return false;
        const long long t=q.num/q.den;     // truncation towards zero (fmod semantics)
        if(op=='q'){out=exactInt(t);return true;}
        Rational prod,rem;
        if(!exact::mul(y,Rational::integer(t),prod)||!exact::sub(x,prod,rem))return false;
        out=Value::fromRational(rem);return true;
    }
    case '=': case 'n': case '<': case '>': case 'l': case 'g':{
        int c=0;
        if(!exactCompare(a,b,c))return false;
        const bool v=op=='='?c==0:op=='n'?c!=0:op=='<'?c<0:op=='>'?c>0:op=='l'?c<=0:c>=0;
        out=exactInt(v?1:0);return true;
    }
    case '&': out=exactInt(!a.isZero()&&!b.isZero());return true;
    case '|': out=exactInt(!a.isZero()||!b.isZero());return true;
    case 'x': out=exactInt(a.isZero()!=b.isZero());return true;
    case 'X': out=exactInt(a.isZero()==b.isZero());return true;
    default: return false;
    }
}
// Exact counterparts of the scalar functions; false for functions whose
// values are transcendental (or random) at the given argument.
FXAI_NOINLINE bool exactApply(const std::string& f,const Value* a,std::size_t count,Value& out){
    long long i0=0,i1=0,i2=0;
    Rational r0,r1,deg;
    if(count==1){
        const Value& x=a[0];
        if(f=="abs"){if(x.sign()<0)return exact::neg(x,out);out=x;return true;}
        if(f=="sqrt")return exact::sqrt(x,out);
        if(f=="cbrt")return exact::nthRoot(x,3,out);
        if(isRootN(f))return exact::nthRoot(x,static_cast<int>(std::strtol(f.c_str()+4,nullptr,10)),out);
        if(f=="recip")return exact::div(exactInt(1),x,out);
        if(f=="percent")return exact::div(x,exactInt(100),out);
        if(f=="re"||f=="real"||f=="conj"){out=x;return true;}
        if(f=="im"||f=="imag"){out=Value{};return true;}
        if(f=="arg"){const int s=x.sign();if(s==0)return false;return exactFromDegrees(Rational::integer(s>0?0:180),out);}
        if(f=="sin"||f=="cos"||f=="tan"){
            if(!exactToDegrees(x,deg))return false;
            Value sn,cs;
            if(f!="cos"&&!exactSinDegrees(deg,sn))return false;
            if(f!="sin"){Rational d90;if(!exact::add(deg,Rational::integer(90),d90)||!exactSinDegrees(d90,cs))return false;}
            if(f=="sin"){out=sn;return true;}
            if(f=="cos"){out=cs;return true;}
            return exact::div(sn,cs,out);
        }
        if(f=="asin"||f=="acos"||f=="atan"){
            if(!(f=="atan"?exactAtanDegrees(x,deg):exactAsinDegrees(x,deg)))return false;
            if(f=="acos"&&!exact::sub(Rational::integer(90),deg,deg))return false;
            return exactFromDegrees(deg,out);
        }
        if(f=="fromdeg"){Rational d;if(x.isZero()){out=x;return true;}if(!x.rational(d))return false;return exactFromDegrees(d,out);}
        if(f=="deg"){if(x.isZero()){out=x;return true;}if(!x.isPi())return false;Rational d;if(!exact::mul(x.term(0).coef,Rational::integer(180),d))return false;out=Value::fromRational(d);return true;}
        if(f=="grad"){if(x.isZero()){out=x;return true;}if(!x.isPi())return false;Rational d;if(!exact::mul(x.term(0).coef,Rational::integer(200),d))return false;out=Value::fromRational(d);return true;}
        if(f=="rad"){Rational d;if(!x.rational(d)||!exact::div(d,Rational::integer(180),d))return false;out=Value::piTimes(d);return true;}
        if(f=="ln"||f=="exp"){
            Rational v;
            if(!x.rational(v))return false;
            if(f=="ln"&&v==Rational::integer(1)){out=Value{};return true;}
            if(f=="exp"&&v.isZero()){out=exactInt(1);return true;}
            return false;
        }
        if(f=="log"||f=="log10"||f=="log2"){
            Rational v;
            if(!x.rational(v)||v.sign()<=0)return false;
            const long long base=f=="log2"?2:10;
            for(long long k=-60;k<=60;++k){
                Value p;
                if(exact::powInt(exactInt(base),k,p)&&p==x){out=exactInt(k);return true;}
            }
            return false;
        }
        if(f=="floor"||f=="intg"){if(!exactFloor(x,i0))return false;out=exactInt(i0);return true;}
        if(f=="ceil"){Value n;if(!exact::neg(x,n)||!exactFloor(n,i0))return false;out=exactInt(-i0);return true;}
        if(f=="trunc"||f=="int"||f=="frac"){
            if(!exactFloor(x,i0))return false;
            if(x.sign()<0&&!(x.isRational()&&x==exactInt(i0)))++i0;          // towards zero
            if(f=="frac")return exact::sub(x,exactInt(i0),out);
            out=exactInt(i0);return true;
        }
        if(f=="round"){Value h;if(!exactRoundDigits(x,0,h))return false;out=h;return true;}
        if(f=="sign"){out=exactInt(x.sign());return true;}
        if(f=="not"){out=exactInt(x.isZero()?1:0);return true;}
        if(f=="neg")return exact::neg(x,out);
        if(f=="fact"||f=="factorial"){
            if(!exactInteger(x,i0)||i0<0||i0>20)return false;
            long long p=1;for(long long k=2;k<=i0;++k)p*=k;
            out=exactInt(p);return true;
        }
        if(f=="dms"){out=x;return true;}
        return false;
    }
    if(count==2){
        const Value &x=a[0],&y=a[1];
        if(f=="pow")return exactPow(x,y,out);
        if(f=="frac")return exact::div(x,y,out);
        if(f=="root"){if(!exactInteger(x,i0)||i0<2||i0>99)return false;return exact::nthRoot(y,static_cast<int>(i0),out);}
        if(f=="log"){
            // log(a,b) = k when b = a^k exactly (rational base).
            if(!x.rational(r0)||!y.rational(r1)||r0.sign()<=0||r1.sign()<=0||r0==Rational::integer(1))return false;
            const double k=std::round(std::log(r1.toDouble())/std::log(r0.toDouble()));
            if(!std::isfinite(k)||std::abs(k)>64)return false;
            Value p;
            if(!exact::powInt(x,static_cast<long long>(k),p)||p!=y)return false;
            out=exactInt(static_cast<long long>(k));return true;
        }
        if(f=="min"||f=="max"){int c=0;if(!exactCompare(x,y,c))return false;out=(f=="min")==(c<=0)?x:y;return true;}
        if(f=="hypot"){Value s1,s2,s;return exact::mul(x,x,s1)&&exact::mul(y,y,s2)&&exact::add(s1,s2,s)&&exact::sqrt(s,out);}
        if(f=="mod")return exactBinary('%',x,y,out);
        if(f=="round"||f=="rndfix"){if(!exactInteger(y,i1))return false;return exactRoundDigits(x,i1,out);}
        if(f=="gcd"||f=="lcm"){
            if(!exactInteger(x,i0)||!exactInteger(y,i1))return false;
            long long g=std::llabs(i0),h=std::llabs(i1);while(h){const long long t=g%h;g=h;h=t;}
            if(f=="gcd"){out=exactInt(g);return true;}
            if(g==0){out=Value{};return true;}
            long long l=0;
            if(__builtin_mul_overflow(std::llabs(i0)/g,std::llabs(i1),&l))return false;
            out=exactInt(l);return true;
        }
        if(f=="ncr"||f=="npr"){
            if(!exactInteger(x,i0)||!exactInteger(y,i1)||i0<0||i1<0||i1>i0)return false;
            long long acc=1;
            if(f=="npr"){for(long long k=0;k<i1;++k)if(__builtin_mul_overflow(acc,i0-k,&acc))return false;}
            else{
                const long long k=std::min(i1,i0-i1);
                for(long long j=1;j<=k;++j){
                    // acc * (n-k+j) / j is always an integer; divide by the gcd first to delay overflow.
                    long long num=i0-k+j,den=j;
                    long long g=std::gcd(acc,den);acc/=g;den/=g;
                    g=std::gcd(num,den);num/=g;den/=g;
                    if(den!=1||__builtin_mul_overflow(acc,num,&acc))return false;
                }
            }
            out=exactInt(acc);return true;
        }
        return false;
    }
    if(count==3){
        if(f=="mixed"){
            Value fr;
            if(!exact::div(a[1],a[2],fr))return false;
            return a[0].sign()<0?exact::sub(a[0],fr,out):exact::add(a[0],fr,out);
        }
        if(f=="dms"){
            Value m,s,t;
            return exact::div(a[1],exactInt(60),m)&&exact::div(a[2],exactInt(3600),s)&&exact::add(a[0],m,t)&&exact::add(t,s,out);
        }
        if(f=="modexp"){
            if(!exactInteger(a[0],i0)||!exactInteger(a[1],i1)||!exactInteger(a[2],i2)||i0<0||i1<0||i2<=0)return false;
            return false;   // the floating-point path computes it exactly with integers; nothing to add
        }
    }
    return false;
}

bool exactNode(const ast::Node& n,Env& env,Value& out);

FXAI_NOINLINE bool exactToken(const ast::Node& n,Env& env,Value& out){
    if(n.text.empty())return false;
    const unsigned char c0=static_cast<unsigned char>(n.text[0]);
    if(env.ctx.integerBase)return false;                       // Base-N: valori interi, nessuna forma esatta
    if(c0=='#'){
        // Simbolo ingegneristico: valore esatto, mostrato come un decimale (come 2.5).
        Rational r;
        if(!exact::parseDecimal(n.text.substr(1),r))return false;
        env.decimal=true;
        out=Value::fromRational(r);return true;
    }
    if(std::isdigit(c0)&&n.text.find(kFractionMark)!=std::string::npos){
        Rational r;
        if(!fractionLiteral(n.text,r))return false;
        out=Value::fromRational(r);return true;
    }
    if(std::isdigit(c0)||c0=='.'){
        Rational r;
        if(!exact::parseDecimal(n.text,r))return false;
        if(exact::isPlainDecimal(n.text))env.decimal=true;
        out=Value::fromRational(r);return true;
    }
    double bound=0;
    if(lookupBinding(env,n.text,bound)){
        if(!isInteger(bound)||std::abs(bound)>9e15)return false;   // only exact integer indices (Σ, Π)
        out=exactInt(static_cast<long long>(bound));return true;
    }
    const std::string low=lower(n.text);
    if(low=="pi"){out=Value::piTimes(Rational::integer(1));return true;}
    if(low=="tau"){out=Value::piTimes(Rational::integer(2));return true;}
    const CalculatorMemory* mem=env.ctx.memory;
    bool decimal=false;
    if(low=="ans"||low=="preans"){
        if(!mem||!(low=="ans"?mem->exactAnswer(out,decimal):mem->exactPreviousAnswer(out,decimal)))return false;
        env.decimal=env.decimal||decimal;return true;
    }
    if(n.text.size()==1&&std::isalpha(c0)&&n.text!="e"&&n.text!="i"){
        if(!mem||!mem->exactRecall(static_cast<char>(c0),out,decimal))return false;
        env.decimal=env.decimal||decimal;return true;
    }
    return false;
}
// Σ / Π over integer ranges with an exact body (at most 10000 terms).
FXAI_NOINLINE bool exactSum(const ast::Node& n,Env& env,Value& out){
    if(n.children.size()!=3||!n.children[0]||!n.children[1]||!n.children[2])return false;
    Value lo,hi;
    long long a=0,b=0;
    if(!exactNode(*n.children[1],env,lo)||!exactNode(*n.children[2],env,hi)||!exactInteger(lo,a)||!exactInteger(hi,b))return false;
    if(b-a>10000)return false;
    if(++env.depth>4){--env.depth;return false;}
    struct DepthGuard{Env& e;~DepthGuard(){--e.depth;}} guard{env};
    const bool sum=n.text=="sum";
    Value acc=exactInt(sum?0:1);
    LocalBind bind(env,"x",0.0);
    for(long long k=a;k<=b;++k){
        bind.set(static_cast<double>(k));
        Value term;
        if(!exactNode(*n.children[0],env,term))return false;
        if(!(sum?exact::add(acc,term,acc):exact::mul(acc,term,acc)))return false;
    }
    out=acc;return true;
}
FXAI_NOINLINE bool exactCall(const ast::Node& n,Env& env,Value& out){
    const std::string& f=n.text;
    if(f=="sum"||f=="prod")return exactSum(n,env,out);
    if(!f.empty()&&f[0]=='@')return false;   // graph function references are evaluated numerically
    // Numerical calculus and random numbers have no exact value here.
    if(isLazyFunction(f)||f=="rand"||f=="ran"||f=="randint"||f=="rannorm"||f=="ranbin")return false;
    const std::size_t count=n.children.size();
    if(count>3)return false;
    Value args[3];
    for(std::size_t i=0;i<count;++i){
        if(!n.children[i]||!exactNode(*n.children[i],env,args[i]))return false;
    }
    if(!exactApply(f,args,count,out))return false;
    // Rounding to decimal places gives a decimal result (fx-CG50 RndFix).
    if(f=="rndfix"||(f=="round"&&count==2))env.decimal=true;
    return true;
}
FXAI_NOINLINE bool exactChain(const ast::Node& top,Env& env,Value& out){
    constexpr std::size_t kChunk=16;
    const ast::Node* spine[kChunk];
    std::size_t count=0;
    const ast::Node* cur=&top;
    while(count<kChunk&&isChainNode(*cur)){spine[count++]=cur;cur=cur->children[0].get();}
    if(!(isChainNode(*cur)?exactChain(*cur,env,out):exactNode(*cur,env,out)))return false;
    while(count>0){
        const ast::Node& b=*spine[--count];
        Value rhs;
        if(!exactNode(*b.children[1],env,rhs)||!exactBinary(b.text[0],out,rhs,out))return false;
    }
    return true;
}
bool exactNode(const ast::Node& node,Env& env,Value& out){
    using ast::Kind;
    if(!exactStackOk(env))return false;
    const ast::Node* np=&node;
    while(np->kind==Kind::Group){
        if(np->children.size()!=1||!np->children[0])return false;
        np=np->children[0].get();
    }
    const ast::Node& n=*np;
    if(n.flags&ast::FlagCollection)return exactCollectionScalar(n,env,out);
    switch(n.kind){
    case Kind::Token: return exactToken(n,env,out);
    case Kind::Unary:
        if(n.children.size()!=1||!n.children[0]||!exactNode(*n.children[0],env,out))return false;
        return n.text!="-"||exact::neg(out,out);
    case Kind::Binary:{
        if(n.children.size()!=2||!n.children[0]||!n.children[1]||n.text.size()!=1)return false;
        if(isChainNode(n))return exactChain(n,env,out);
        Value rhs;
        if(!exactNode(*n.children[0],env,out)||!exactNode(*n.children[1],env,rhs))return false;
        return exactBinary(n.text[0],out,rhs,out);
    }
    case Kind::Call: return exactCall(n,env,out);
    default: return false;
    }
}

// ---------------------------------------------------------------- matrici, vettori e liste
// Un solo valutatore per ogni tipo di valore. Una sotto-espressione senza
// collezioni passa ai valutatori scalari sopra, cosi' i numeri mantengono
// esattamente il comportamento che hanno ovunque; le regole delle collezioni
// (aritmetica elemento per elemento delle liste, prodotto di matrici, inversa,
// Det, DotP...) sono scritte una sola volta, come template sul dominio degli
// elementi: virgola mobile (double o complessi, secondo il Complex Mode) o
// valori esatti.
template<typename E> struct Coll {
    ValueType type{ValueType::Scalar};
    std::size_t rows{1},cols{1};
    E s{};                  // valore di un numero
    std::vector<E> v;       // elementi per righe di una lista, matrice o vettore
    bool scalar() const noexcept {return type==ValueType::Scalar;}
    bool list() const noexcept {return type==ValueType::List;}
    bool mat() const noexcept {return type==ValueType::Matrix||type==ValueType::Vector;}
    bool vectorShaped() const noexcept {return mat()&&(rows==1||cols==1);}
};
void checkMatrixSize(std::size_t rows,std::size_t cols){
    if(rows<1||cols<1)throw DimensionError("Dimension ERROR: una matrice ha almeno una riga e una colonna");
    if(rows>limits::kMatrixMaxDim||cols>limits::kMatrixMaxDim||rows*cols>limits::kMatrixElements)
        throw DimensionError("Dimension ERROR: matrice troppo grande (massimo "+std::to_string(limits::kMatrixMaxDim)+" per lato e "+
                             std::to_string(limits::kMatrixElements)+" elementi)");
}
void checkListSize(std::size_t n){
    if(n<1)throw DimensionError("Dimension ERROR: una lista ha almeno un elemento");
    if(n>limits::kListMaxLength)throw DimensionError("Dimension ERROR: massimo "+std::to_string(limits::kListMaxLength)+" elementi in una lista");
}

// Lanciata nel dominio esatto quando un passaggio non ha forma esatta.
struct NoExactForm {};

// Elementi in virgola mobile: aritmetica reale nel modo Real, complessa negli altri.
struct NumDomain {
    using Elem=Complex;
    static bool real(const Env& env){return env.ctx.complexMode==ComplexMode::Real;}
    static Elem node(const ast::Node& n,Env& env){
        if(real(env))return Complex(evalNode<double>(n,env),0.0);
        return evalNode<Complex>(n,env);
    }
    static Elem lazy(const ast::Node& n,Env& env){
        if(real(env)){const double r=callFunction<double>(n,env);checkFinite(r);return Complex(r,0.0);}
        const Complex r=callFunction<Complex>(n,env);checkFinite(r);return r;
    }
    static Elem binary(char op,const Elem& a,const Elem& b,Env& env){
        if(real(env))return Complex(combine<double>(env,op,a.real(),b.real()),0.0);
        return combine<Complex>(env,op,a,b);
    }
    static Elem negate(const Elem& a,Env&){const double re=-a.real(),im=-a.imag();return Complex(re==0.0?0.0:re,im==0.0?0.0:im);}
    static Elem apply(const std::string& f,const Elem* args,std::size_t count,Env& env){
        if(real(env)){
            double r[3]={};
            for(std::size_t i=0;i<count&&i<3;++i)r[i]=args[i].real();
            const double v=applyFunction<double>(f,r,count);checkFinite(v);return Complex(v,0.0);
        }
        const Complex v=applyFunction<Complex>(f,args,count);checkFinite(v);return v;
    }
    static Elem fromInt(long long v){return Complex(static_cast<double>(v),0.0);}
    static Elem fromDouble(double v){return Complex(v,0.0);}
    static bool isZero(const Elem& a){return a.real()==0.0&&a.imag()==0.0;}
    static double magnitude(const Elem& a){return std::abs(a);}
    static double toReal(const Elem& a,const char* what){return requireReal(a,what);}
    static int compare(const Elem& a,const Elem& b){
        const double x=requireReal(a,"il confronto"),y=requireReal(b,"il confronto");
        return (x>y)-(x<y);
    }
    static bool negligible(const Elem& pivot,double scale){return std::abs(pivot)<=1e-12*scale;}
    static void load(const CalcValue& src,Coll<Elem>& out,Env& env){
        if(real(env)&&src.complex())throw NonRealError("Non-Real ERROR: elementi complessi (serve il modo complesso)");
        out.rows=src.rows;out.cols=src.cols;
        out.v.resize(src.size());
        for(std::size_t i=0;i<src.size();++i)out.v[i]=src.at(i);
    }
};
// Elementi esatti (razionali, radicali, multipli di pi): ogni passaggio e'
// esatto oppure la valutazione esatta viene abbandonata.
struct ExactDomain {
    using Elem=exact::Value;
    static Elem node(const ast::Node& n,Env& env){Elem out;if(!exactNode(n,env,out))throw NoExactForm{};return out;}
    static Elem lazy(const ast::Node& n,Env& env){Elem out;if(!exactCall(n,env,out))throw NoExactForm{};return out;}
    static Elem binary(char op,const Elem& a,const Elem& b,Env&){Elem out;if(!exactBinary(op,a,b,out))throw NoExactForm{};return out;}
    static Elem negate(const Elem& a,Env&){Elem out;if(!exact::neg(a,out))throw NoExactForm{};return out;}
    static Elem apply(const std::string& f,const Elem* args,std::size_t count,Env& env){
        Elem out;
        if(count>3||!exactApply(f,args,count,out))throw NoExactForm{};
        if(f=="rndfix"||(f=="round"&&count==2))env.decimal=true;
        return out;
    }
    static Elem fromInt(long long v){return exactInt(v);}
    static Elem fromDouble(double v){
        if(!isInteger(v)||std::abs(v)>9e15)throw NoExactForm{};
        return exactInt(static_cast<long long>(v));
    }
    static bool isZero(const Elem& a){return a.isZero();}
    static double magnitude(const Elem& a){return std::abs(a.toDouble());}
    static double toReal(const Elem& a,const char*){Rational r;if(!a.rational(r))throw NoExactForm{};return r.toDouble();}
    static int compare(const Elem& a,const Elem& b){int c=0;if(!exactCompare(a,b,c))throw NoExactForm{};return c;}
    static bool negligible(const Elem& pivot,double){return pivot.isZero();}
    // Un elemento memorizzato e' esatto quando la memoria conserva la sua forma
    // esatta (celle dell'editor MAT/VCT, risultati esatti memorizzati: v1.11.0)
    // o quando e' intero (un double intero e' esattamente quell'intero).
    static void load(const CalcValue& src,Coll<Elem>& out,Env&){
        if(src.complex())throw NoExactForm{};
        out.rows=src.rows;out.cols=src.cols;
        if(src.hasExact()){out.v=src.exact;return;}
        out.v.resize(src.size());
        for(std::size_t i=0;i<src.size();++i){
            const double d=src.re[i];
            if(!isInteger(d)||std::abs(d)>9e15)throw NoExactForm{};
            out.v[i]=exactInt(static_cast<long long>(d));
        }
    }
};

template<class D> class CollEval {
public:
    using E=typename D::Elem;
    using C=Coll<E>;
    explicit CollEval(Env& e):env(e){}

    C eval(const ast::Node& node){
        using ast::Kind;
        checkStack(env);
        const ast::Node* np=&node;
        while(np->kind==Kind::Group){
            if(np->children.size()!=1||!np->children[0])throw SyntaxError("Syntax ERROR: gruppo vuoto");
            np=np->children[0].get();
        }
        const ast::Node& n=*np;
        if(!(n.flags&ast::FlagCollection))return scalarOf(D::node(n,env));
        switch(n.kind){
        case Kind::Token: return reference(n);
        case Kind::Matrix: return matrixLiteral(n);
        case Kind::List: return listLiteral(n);
        case Kind::Unary:{
            C a=eval(*n.children.at(0));
            if(n.text!="-")return a;
            env.at=&n;
            if(a.scalar())a.s=D::negate(a.s,env);
            else for(E& e:a.v)e=D::negate(e,env);
            return a;
        }
        case Kind::Binary:{
            if(isChainNode(n))return chain(n);
            C a=eval(*n.children.at(0));
            C b=eval(*n.children.at(1));
            env.at=&n;
            return binary(n.text.empty()?'?':n.text[0],a,b);
        }
        case Kind::Call: return call(n);
        default: break;
        }
        throw SyntaxError("Syntax ERROR: nodo non valutabile");
    }

private:
    Env& env;

    // Le catene associative a sinistra si percorrono in modo iterativo, come in
    // evalChain(): la profondita' della ricorsione segue l'annidamento
    // dell'input, non la sua lunghezza.
    C chain(const ast::Node& top){
        constexpr std::size_t kChunk=16;
        const ast::Node* spine[kChunk];
        std::size_t count=0;
        const ast::Node* cur=&top;
        auto walkable=[](const ast::Node& x){return isChainNode(x)&&(x.flags&ast::FlagCollection);};
        while(count<kChunk&&walkable(*cur)){spine[count++]=cur;cur=cur->children[0].get();}
        C acc=walkable(*cur)?chain(*cur):eval(*cur);
        while(count>0){
            const ast::Node& b=*spine[--count];
            const C rhs=eval(*b.children[1]);
            env.at=&b;
            acc=binary(b.text[0],acc,rhs);
        }
        return acc;
    }

    // ---- funzioni di costruzione
    static C scalarOf(const E& e){C c;c.s=e;return c;}
    static C shaped(ValueType type,std::size_t rows,std::size_t cols){
        C c;c.type=type;c.rows=rows;c.cols=cols;c.v.resize(rows*cols);return c;
    }
    static C matrixOf(std::size_t rows,std::size_t cols,ValueType type=ValueType::Matrix){checkMatrixSize(rows,cols);return shaped(type,rows,cols);}
    static C listOf(std::size_t n){checkListSize(n);return shaped(ValueType::List,n,1);}
    static std::string dims(const C& c){
        if(c.scalar())return "numero";
        if(c.list())return "lista di "+std::to_string(c.v.size());
        return std::to_string(c.rows)+"x"+std::to_string(c.cols);
    }
    [[noreturn]] static void typeError(const std::string& what){throw TypeError("Type ERROR: "+what);}
    E op(char o,const E& a,const E& b){return D::binary(o,a,b,env);}
    E fn(const char* name,const E& a){return D::apply(name,&a,1,env);}
    static double maxMagnitude(const C& c){double m=0;for(const E& e:c.v)m=std::max(m,D::magnitude(e));return m;}
    static void swapRows(C& m,std::size_t a,std::size_t b){
        if(a==b)return;
        for(std::size_t j=0;j<m.cols;++j)std::swap(m.v[a*m.cols+j],m.v[b*m.cols+j]);
    }
    E scalarArg(const C& c,const char* what){
        if(!c.scalar())typeError(std::string(what)+" deve essere un numero, non una "+valueTypeName(c.type));
        return c.s;
    }
    long long integerArg(const C& c,const char* what){
        const double d=D::toReal(scalarArg(c,what),what);
        if(!isInteger(d)||std::abs(d)>1e9)throw MathError(std::string("Argument ERROR: ")+what+" deve essere un intero");
        return static_cast<long long>(d);
    }
    const C& listArg(const C& c,const char* f){
        if(!c.list())typeError(std::string(f)+" richiede una lista, non "+article(c));
        return c;
    }
    const C& matrixArg(const C& c,const char* f){
        if(!c.mat())typeError(std::string(f)+" richiede una matrice, non "+article(c));
        return c;
    }
    const C& vectorArg(const C& c,const char* f){
        if(!c.mat())typeError(std::string(f)+" richiede un vettore, non "+article(c));
        if(!c.vectorShaped())throw DimensionError(std::string("Dimension ERROR: ")+f+" richiede un vettore (1xn o nx1), non una matrice "+dims(c));
        return c;
    }
    static std::string article(const C& c){
        switch(c.type){
        case ValueType::Scalar: return "un numero";
        case ValueType::List: return "una lista";
        case ValueType::Matrix: return "una matrice";
        case ValueType::Vector: return "un vettore";
        }
        return "?";
    }

    // ---- valori letterali e memorie
    E element(const ast::Node& cell,const char* what){
        C v=eval(cell);
        if(!v.scalar()){env.at=&cell;typeError(std::string(what)+" devono essere numeri");}
        return v.s;
    }
    C matrixLiteral(const ast::Node& n){
        const std::size_t cols=ast::matrixColumns(n);
        if(cols==0||n.children.empty()||n.children.size()%cols!=0)throw SyntaxError("Syntax ERROR: matrice non valida");
        env.at=&n;
        C out=matrixOf(n.children.size()/cols,cols);
        for(std::size_t i=0;i<n.children.size();++i){
            if(!n.children[i])throw SyntaxError("Syntax ERROR: elemento mancante");
            out.v[i]=element(*n.children[i],"gli elementi di una matrice");
        }
        return out;
    }
    C listLiteral(const ast::Node& n){
        env.at=&n;
        C out=listOf(n.children.size());
        for(std::size_t i=0;i<n.children.size();++i){
            if(!n.children[i])throw SyntaxError("Syntax ERROR: elemento mancante");
            out.v[i]=element(*n.children[i],"gli elementi di una lista");
        }
        return out;
    }
    C reference(const ast::Node& n){
        using Memory=CollectionResolver::Memory;
        env.at=&n;
        std::string t=n.text;                        // "Mat A", "Vct Ans", "List 12", "list3"
        Memory memory=Memory::List;
        int index=CollectionResolver::Answer;
        if(isListName(t)){
            // Una funzione di GRAPH con una lista si disegna una volta per elemento: li'
            // il nome e' l'elemento del grafico in corso, un numero.
            double element=0;
            if(env.ctx.names&&env.ctx.names->resolve(t,element))return scalarOf(D::fromDouble(element));
            index=listNumber(t)-1;
            t="List "+t.substr(4);
        }else{
            const std::size_t space=t.find(' ');
            if(space==std::string::npos||space+1>=t.size())throw SyntaxError("Syntax ERROR: nome sconosciuto "+t);
            memory=t[0]=='M'?Memory::Mat:t[0]=='V'?Memory::Vct:Memory::List;
            const std::string name=t.substr(space+1);
            if(name!="Ans")index=memory==Memory::List?std::atoi(name.c_str())-1:name[0]-'A';
        }
        if(!env.ctx.collections)throw MathError("Math ERROR: "+t+" non disponibile qui");
        CalcValue stored;
        if(!env.ctx.collections->collection(memory,index,stored)||stored.empty())
            throw DimensionError("Dimension ERROR: "+t+" non definita");
        C out;
        D::load(stored,out,env);
        out.type=memory==Memory::Mat?ValueType::Matrix:memory==Memory::Vct?ValueType::Vector:ValueType::List;
        return out;
    }

    // ---- aritmetica
    static ValueType resultType(const C& a,const C& b){
        if(a.scalar())return b.type;
        if(b.scalar())return a.type;
        if(a.list())return ValueType::List;
        return a.type==ValueType::Vector&&b.type==ValueType::Vector?ValueType::Vector:ValueType::Matrix;
    }
    // Elemento per elemento; un numero si combina con ogni elemento.
    C zip(char o,const C& a,const C& b){
        const C& shape=a.scalar()?b:a;
        if(!a.scalar()&&!b.scalar()&&a.v.size()!=b.v.size())
            throw DimensionError("Dimension ERROR: lunghezze diverse ("+std::to_string(a.v.size())+" e "+std::to_string(b.v.size())+")");
        C out=shaped(resultType(a,b),shape.rows,shape.cols);
        for(std::size_t i=0;i<out.v.size();++i)out.v[i]=op(o,a.scalar()?a.s:a.v[i],b.scalar()?b.s:b.v[i]);
        return out;
    }
    C binary(char o,const C& a,const C& b){
        if(a.scalar()&&b.scalar())return scalarOf(op(o,a.s,b.s));
        if(a.list()||b.list()){
            if(a.mat()||b.mat())typeError("operazione tra una lista e una matrice");
            return zip(o,a,b);
        }
        switch(o){
        case '+': case '-':
            if(a.scalar()||b.scalar())typeError(std::string(o=='+'?"somma":"differenza")+" tra un numero e una matrice");
            if(a.rows!=b.rows||a.cols!=b.cols)
                throw DimensionError("Dimension ERROR: "+dims(a)+" e "+dims(b)+" hanno dimensioni diverse");
            return zip(o,a,b);
        case '*':
            if(a.scalar()||b.scalar())return zip(o,a,b);
            return multiply(a,b);
        case '/':
            if(b.scalar())return zip(o,a,b);
            typeError("divisione per una matrice (usa l'inversa ^-1)");
        case '^': return power(a,b);
        default: break;
        }
        typeError("operatore non definito per matrici e vettori");
    }
    C multiply(const C& a,const C& b){
        if(a.cols!=b.rows)throw DimensionError("Dimension ERROR: il prodotto "+dims(a)+" per "+dims(b)+" non e' definito");
        C out=matrixOf(a.rows,b.cols);
        if(a.type==ValueType::Vector&&b.type==ValueType::Vector&&out.vectorShaped())out.type=ValueType::Vector;
        const E zero=D::fromInt(0);
        for(std::size_t i=0;i<a.rows;++i)for(std::size_t j=0;j<b.cols;++j){
            E acc=zero;
            for(std::size_t k=0;k<a.cols;++k)acc=op('+',acc,op('*',a.v[i*a.cols+k],b.v[k*b.cols+j]));
            out.v[i*b.cols+j]=acc;
        }
        return out;
    }
    C identity(std::size_t n){
        C out=matrixOf(n,n);
        const E zero=D::fromInt(0),one=D::fromInt(1);
        for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j)out.v[i*n+j]=i==j?one:zero;
        return out;
    }
    static void requireSquare(const C& a,const char* what){
        if(a.rows!=a.cols)throw DimensionError(std::string("Dimension ERROR: ")+what+" richiede una matrice quadrata, non "+dims(a));
    }
    // Eliminazione di Gauss-Jordan con pivot parziale.
    C inverse(const C& a){
        requireSquare(a,"l'inversa");
        const std::size_t n=a.rows;
        C m=a;
        C inv=identity(n);
        const double scale=maxMagnitude(a);
        for(std::size_t c=0;c<n;++c){
            std::size_t p=c;
            double best=D::magnitude(m.v[c*n+c]);
            for(std::size_t r=c+1;r<n;++r){const double g=D::magnitude(m.v[r*n+c]);if(g>best){best=g;p=r;}}
            if(D::negligible(m.v[p*n+c],scale))throw MathError("Math ERROR: matrice singolare (determinante nullo)");
            swapRows(m,p,c);swapRows(inv,p,c);
            const E d=m.v[c*n+c];
            for(std::size_t j=0;j<n;++j){m.v[c*n+j]=op('/',m.v[c*n+j],d);inv.v[c*n+j]=op('/',inv.v[c*n+j],d);}
            for(std::size_t r=0;r<n;++r){
                if(r==c)continue;
                const E f=m.v[r*n+c];
                if(D::isZero(f))continue;
                for(std::size_t j=0;j<n;++j){
                    m.v[r*n+j]=op('-',m.v[r*n+j],op('*',f,m.v[c*n+j]));
                    inv.v[r*n+j]=op('-',inv.v[r*n+j],op('*',f,inv.v[c*n+j]));
                }
            }
        }
        return inv;
    }
    C power(const C& a,const C& b){
        if(a.scalar())typeError("un numero elevato a una matrice non e' definito");
        if(!b.scalar())typeError("l'esponente di una matrice deve essere un numero");
        const double e=D::toReal(b.s,"l'esponente");
        if(!isInteger(e)||std::abs(e)>32766)throw MathError("Math ERROR: l'esponente di una matrice e' un intero tra -32766 e 32766");
        requireSquare(a,"la potenza");
        long long n=static_cast<long long>(e);
        C base=a;
        base.type=ValueType::Matrix;
        if(n<0){base=inverse(base);n=-n;}
        if(n==0)return identity(a.rows);
        // Quadrati e prodotti; il primo fattore si copia, non si moltiplica per l'identita'.
        C result;
        bool have=false;
        while(n){
            if(n&1){if(have)result=multiply(result,base);else{result=base;have=true;}}
            n>>=1;
            if(n)base=multiply(base,base);
        }
        return result;
    }
    E determinant(const C& a){
        requireSquare(a,"Det");
        const std::size_t n=a.rows;
        C m=a;
        const double scale=maxMagnitude(a);
        E det=D::fromInt(1);
        bool negative=false;
        for(std::size_t c=0;c<n;++c){
            std::size_t p=c;
            double best=D::magnitude(m.v[c*n+c]);
            for(std::size_t r=c+1;r<n;++r){const double g=D::magnitude(m.v[r*n+c]);if(g>best){best=g;p=r;}}
            if(D::negligible(m.v[p*n+c],scale))return D::fromInt(0);
            if(p!=c){swapRows(m,p,c);negative=!negative;}
            const E pivot=m.v[c*n+c];
            det=op('*',det,pivot);
            for(std::size_t r=c+1;r<n;++r){
                if(D::isZero(m.v[r*n+c]))continue;
                const E f=op('/',m.v[r*n+c],pivot);
                for(std::size_t j=c;j<n;++j)m.v[r*n+j]=op('-',m.v[r*n+j],op('*',f,m.v[c*n+j]));
            }
        }
        return negative?D::negate(det,env):det;
    }
    // Forma a scala (Ref) e forma a scala ridotta (Rref): eliminazione di Gauss
    // con pivot parziale, primi elementi delle righe portati a 1.
    C echelon(const C& a,bool reduced){
        C m=a;
        m.type=ValueType::Matrix;
        const std::size_t rows=m.rows,cols=m.cols;
        const double scale=maxMagnitude(a);
        const E zero=D::fromInt(0),one=D::fromInt(1);
        std::size_t row=0;
        for(std::size_t c=0;c<cols&&row<rows;++c){
            std::size_t p=row;
            double best=D::magnitude(m.v[row*cols+c]);
            for(std::size_t r=row+1;r<rows;++r){const double g=D::magnitude(m.v[r*cols+c]);if(g>best){best=g;p=r;}}
            if(D::negligible(m.v[p*cols+c],scale)){for(std::size_t r=row;r<rows;++r)m.v[r*cols+c]=zero;continue;}
            swapRows(m,p,row);
            const E d=m.v[row*cols+c];
            for(std::size_t j=c+1;j<cols;++j)m.v[row*cols+j]=op('/',m.v[row*cols+j],d);
            m.v[row*cols+c]=one;
            for(std::size_t r=reduced?0:row+1;r<rows;++r){
                if(r==row)continue;
                const E f=m.v[r*cols+c];
                if(D::isZero(f))continue;
                for(std::size_t j=c+1;j<cols;++j)m.v[r*cols+j]=op('-',m.v[r*cols+j],op('*',f,m.v[row*cols+j]));
                m.v[r*cols+c]=zero;
            }
            ++row;
        }
        return m;
    }
    C transpose(const C& a){
        C out=shaped(a.type,a.cols,a.rows);
        for(std::size_t i=0;i<a.rows;++i)for(std::size_t j=0;j<a.cols;++j)out.v[j*a.rows+i]=a.v[i*a.cols+j];
        return out;
    }
    E fold(const C& l,char o,long long start){
        E acc=D::fromInt(start);
        for(const E& e:l.v)acc=op(o,acc,e);
        return acc;
    }
    E dot(const C& a,const C& b){
        E acc=D::fromInt(0);
        for(std::size_t i=0;i<a.v.size();++i)acc=op('+',acc,op('*',a.v[i],b.v[i]));
        return acc;
    }
    E norm(const C& a){
        E acc=D::fromInt(0);
        for(const E& e:a.v){const E m=fn("abs",e);acc=op('+',acc,op('*',m,m));}
        return fn("sqrt",acc);
    }
    std::vector<E> sorted(const C& l,bool descending){
        std::vector<E> v=l.v;
        // Ordinamento per inserimento: le liste sono corte e il confronto puo' arrendersi (dominio esatto).
        for(std::size_t i=1;i<v.size();++i){
            E key=v[i];
            std::size_t j=i;
            while(j>0&&(descending?D::compare(v[j-1],key)<0:D::compare(v[j-1],key)>0)){v[j]=v[j-1];--j;}
            v[j]=key;
        }
        return v;
    }

    // ---- funzioni
    // Seq(espressione, variabile, inizio, fine, incremento)
    C sequence(const ast::Node& n){
        const ast::Node* var=n.children[1].get();
        while(var->kind==ast::Kind::Group&&var->children.size()==1&&var->children[0])var=var->children[0].get();
        if(var->kind!=ast::Kind::Token||var->text.size()!=1||!std::isalpha(static_cast<unsigned char>(var->text[0]))){
            env.at=var;
            throw SyntaxError("Syntax ERROR: il secondo argomento di Seq e' la variabile (es. X)");
        }
        const char name[2]={static_cast<char>(std::tolower(static_cast<unsigned char>(var->text[0]))),0};
        double range[3]={};
        for(std::size_t i=0;i<3;++i){
            const ast::Node& a=*n.children[i+2];
            const C v=eval(a);
            env.at=&a;
            range[i]=D::toReal(scalarArg(v,"un estremo di Seq"),"Seq");
        }
        env.at=&n;
        const double start=range[0],end=range[1],step=range[2];
        if(step==0||!std::isfinite(step)||!std::isfinite(start)||!std::isfinite(end))throw MathError("Math ERROR: incremento di Seq nullo o non finito");
        const double steps=(end-start)/step;
        if(steps<0)throw MathError("Math ERROR: Seq non raggiunge il valore finale con questo incremento");
        const double countValue=std::floor(steps+1e-9)+1;
        if(countValue>static_cast<double>(limits::kListMaxLength))checkListSize(limits::kListMaxLength+1);
        C out=listOf(static_cast<std::size_t>(countValue));
        if(++env.depth>4){--env.depth;throw MathError("Stack ERROR: calcoli annidati troppo profondi");}
        struct DepthGuard{Env& e;~DepthGuard(){--e.depth;}} guard{env};
        LocalBind bind(env,name,start);
        for(std::size_t k=0;k<out.v.size();++k){
            bind.set(start+static_cast<double>(k)*step);
            const C v=eval(*n.children[0]);
            if(!v.scalar()){env.at=n.children[0].get();typeError("l'espressione di Seq deve dare un numero");}
            out.v[k]=v.s;
        }
        env.at=&n;
        return out;
    }
    // Funzione scalare applicata alle liste elemento per elemento (e Abs, Int,
    // Frac, Intg e le parti complesse agli elementi di una matrice).
    C mapCall(const std::string& f,const std::vector<C>& args){
        const std::size_t count=args.size();
        if(count>3)throw SyntaxError("Argument ERROR: "+f+" numero di argomenti errato");
        const C* shape=nullptr;
        bool anyList=false,anyMatrix=false;
        for(const C& a:args){
            if(a.scalar())continue;
            anyList=anyList||a.list();anyMatrix=anyMatrix||a.mat();
            if(shape&&shape->v.size()!=a.v.size())
                throw DimensionError("Dimension ERROR: lunghezze diverse ("+std::to_string(shape->v.size())+" e "+std::to_string(a.v.size())+")");
            if(!shape)shape=&a;
        }
        E in[3]={};
        if(!shape){
            for(std::size_t i=0;i<count;++i)in[i]=args[i].s;
            return scalarOf(D::apply(f,in,count,env));
        }
        if(anyMatrix){
            static const char* const allowed[]={"abs","int","frac","intg","re","im","real","imag","conj"};
            bool ok=count==1&&!anyList;
            if(ok){ok=false;for(const char* a:allowed)ok=ok||f==a;}
            if(!ok)typeError(f+" non si applica a matrici e vettori");
        }
        C out=shaped(shape->type,shape->rows,shape->cols);
        for(std::size_t k=0;k<out.v.size();++k){
            for(std::size_t i=0;i<count;++i)in[i]=args[i].scalar()?args[i].s:args[i].v[k];
            out.v[k]=D::apply(f,in,count,env);
        }
        return out;
    }
    C elementOf(const std::vector<C>& a){
        const C& target=a[0];
        if(target.list()){
            if(a.size()!=2)throw SyntaxError("Argument ERROR: una lista ha un solo indice, es. List 1[3]");
            const long long i=integerArg(a[1],"l'indice");
            if(i<1||static_cast<std::size_t>(i)>target.v.size())
                throw DimensionError("Dimension ERROR: indice "+std::to_string(i)+" fuori dalla lista di "+std::to_string(target.v.size()));
            return scalarOf(target.v[static_cast<std::size_t>(i-1)]);
        }
        if(!target.mat())typeError("l'indice [ ] si applica a Mat, Vct e List");
        if(a.size()==2){
            if(!target.vectorShaped())throw SyntaxError("Argument ERROR: una matrice ha due indici, es. Mat A[1,2]");
            const long long i=integerArg(a[1],"l'indice");
            if(i<1||static_cast<std::size_t>(i)>target.v.size())
                throw DimensionError("Dimension ERROR: indice "+std::to_string(i)+" fuori dal vettore di "+std::to_string(target.v.size()));
            return scalarOf(target.v[static_cast<std::size_t>(i-1)]);
        }
        const long long r=integerArg(a[1],"l'indice di riga"),c=integerArg(a[2],"l'indice di colonna");
        if(r<1||c<1||static_cast<std::size_t>(r)>target.rows||static_cast<std::size_t>(c)>target.cols)
            throw DimensionError("Dimension ERROR: ["+std::to_string(r)+","+std::to_string(c)+"] fuori dalla matrice "+dims(target));
        return scalarOf(target.v[static_cast<std::size_t>(r-1)*target.cols+static_cast<std::size_t>(c-1)]);
    }
    C call(const ast::Node& n){
        const std::string& f=n.text;
        const std::size_t count=n.children.size();
        for(const auto& c:n.children)if(!c)throw SyntaxError("Syntax ERROR: argomento mancante");
        if(f=="seq"){if(count!=5)throw SyntaxError("Argument ERROR: Seq(espressione,variabile,inizio,fine,incremento)");return sequence(n);}
        // I riferimenti alle funzioni di GRAPH e le funzioni di calcolo mantengono
        // la loro valutazione (pigra); una collezione puo' stare nei loro argomenti.
        if((!f.empty()&&f[0]=='@')||(isLazyFunction(f)&&count!=1)){
            const E r=D::lazy(n,env);
            env.at=&n;
            return scalarOf(r);
        }
        std::vector<C> a;
        a.reserve(count);
        for(const auto& c:n.children)a.push_back(eval(*c));
        env.at=&n;
        if(f=="elem")return elementOf(a);
        // ---- matrici
        if(f=="det")return scalarOf(determinant(matrixArg(a[0],"Det")));
        if(f=="trn")return transpose(matrixArg(a[0],"Trn"));
        if(f=="ref"||f=="rref")return echelon(matrixArg(a[0],f=="ref"?"Ref":"Rref"),f=="rref");
        if(f=="identity"){
            const long long size=integerArg(a[0],"la dimensione di Identity");
            if(size<1)throw DimensionError("Dimension ERROR: Identity richiede una dimensione positiva");
            return identity(static_cast<std::size_t>(size));
        }
        if(f=="dim"){
            if(a[0].list())return scalarOf(D::fromInt(static_cast<long long>(a[0].v.size())));
            if(!a[0].mat())typeError("Dim richiede una matrice, un vettore o una lista");
            C out=listOf(2);
            out.v[0]=D::fromInt(static_cast<long long>(a[0].rows));out.v[1]=D::fromInt(static_cast<long long>(a[0].cols));
            return out;
        }
        if(f=="augment"){
            if(a[0].list()&&a[1].list()){
                C out=listOf(a[0].v.size()+a[1].v.size());
                std::copy(a[0].v.begin(),a[0].v.end(),out.v.begin());
                std::copy(a[1].v.begin(),a[1].v.end(),out.v.begin()+static_cast<std::ptrdiff_t>(a[0].v.size()));
                return out;
            }
            if(!a[0].mat()||!a[1].mat())typeError("Augment unisce due matrici (o due liste)");
            if(a[0].rows!=a[1].rows)throw DimensionError("Dimension ERROR: Augment richiede lo stesso numero di righe ("+dims(a[0])+" e "+dims(a[1])+")");
            C out=matrixOf(a[0].rows,a[0].cols+a[1].cols);
            for(std::size_t i=0;i<out.rows;++i){
                for(std::size_t j=0;j<a[0].cols;++j)out.v[i*out.cols+j]=a[0].v[i*a[0].cols+j];
                for(std::size_t j=0;j<a[1].cols;++j)out.v[i*out.cols+a[0].cols+j]=a[1].v[i*a[1].cols+j];
            }
            return out;
        }
        if(f=="fill"){
            const E value=scalarArg(a[0],"il valore di Fill");
            if(a[1].scalar())typeError("Fill(valore, matrice o lista)");
            C out=a[1];
            for(E& e:out.v)e=value;
            return out;
        }
        if(f=="mattolist"){
            const C& m=matrixArg(a[0],"Mat\xE2\x86\x92List");
            const long long col=integerArg(a[1],"la colonna");
            if(col<1||static_cast<std::size_t>(col)>m.cols)throw DimensionError("Dimension ERROR: colonna "+std::to_string(col)+" fuori dalla matrice "+dims(m));
            C out=listOf(m.rows);
            for(std::size_t i=0;i<m.rows;++i)out.v[i]=m.v[i*m.cols+static_cast<std::size_t>(col-1)];
            return out;
        }
        if(f=="listtomat"){
            const std::size_t length=listArg(a[0],"List\xE2\x86\x92Mat").v.size();
            for(const C& l:a)if(listArg(l,"List\xE2\x86\x92Mat").v.size()!=length)
                throw DimensionError("Dimension ERROR: List\xE2\x86\x92Mat richiede liste della stessa lunghezza");
            C out=matrixOf(length,count);
            for(std::size_t j=0;j<count;++j)for(std::size_t i=0;i<length;++i)out.v[i*count+j]=a[j].v[i];
            return out;
        }
        // ---- vettori
        if(f=="dotp"||f=="crossp"||f=="angle"){
            const char* label=f=="dotp"?"DotP":f=="crossp"?"CrossP":"Angle";
            const C& x=vectorArg(a[0],label);
            const C& y=vectorArg(a[1],label);
            if(x.v.size()!=y.v.size())throw DimensionError(std::string("Dimension ERROR: ")+label+" richiede vettori della stessa dimensione ("+
                                                           std::to_string(x.v.size())+" e "+std::to_string(y.v.size())+")");
            if(f=="dotp")return scalarOf(dot(x,y));
            if(f=="crossp"){
                if(x.v.size()!=2&&x.v.size()!=3)throw DimensionError("Dimension ERROR: CrossP richiede vettori di dimensione 2 o 3");
                const E zero=D::fromInt(0);
                const E x3=x.v.size()==3?x.v[2]:zero,y3=y.v.size()==3?y.v[2]:zero;
                C out=x.rows==1?shaped(ValueType::Vector,1,3):shaped(ValueType::Vector,3,1);
                out.v[0]=op('-',op('*',x.v[1],y3),op('*',x3,y.v[1]));
                out.v[1]=op('-',op('*',x3,y.v[0]),op('*',x.v[0],y3));
                out.v[2]=op('-',op('*',x.v[0],y.v[1]),op('*',x.v[1],y.v[0]));
                return out;
            }
            const E product=op('*',norm(x),norm(y));
            if(D::isZero(product))throw MathError("Math ERROR: Angle con un vettore nullo");
            return scalarOf(fn("acos",op('/',dot(x,y),product)));
        }
        if(f=="norm")return scalarOf(norm(matrixArg(a[0],"Norm")));
        if(f=="unitv"){
            const C& x=vectorArg(a[0],"UnitV");
            const E length=norm(x);
            if(D::isZero(length))throw MathError("Math ERROR: UnitV di un vettore nullo");
            C out=x;
            out.type=ValueType::Vector;
            for(E& e:out.v)e=op('/',e,length);
            return out;
        }
        // ---- liste
        if(count==1&&(f=="sum"||f=="prod"))return scalarOf(fold(listArg(a[0],f=="sum"?"Sum":"Prod"),f=="sum"?'+':'*',f=="sum"?0:1));
        if(f=="mean"){
            const C& l=listArg(a[0],"Mean");
            return scalarOf(op('/',fold(l,'+',0),D::fromInt(static_cast<long long>(l.v.size()))));
        }
        if(f=="median"){
            const std::vector<E> v=sorted(listArg(a[0],"Median"),false);
            const std::size_t n2=v.size();
            if(n2%2==1)return scalarOf(v[n2/2]);
            return scalarOf(op('/',op('+',v[n2/2-1],v[n2/2]),D::fromInt(2)));
        }
        if(count==1&&(f=="min"||f=="max")){
            const C& l=listArg(a[0],f=="min"?"Min":"Max");
            E best=l.v[0];
            for(const E& e:l.v){const int c=D::compare(e,best);if(f=="min"?c<0:c>0)best=e;}
            return scalarOf(best);
        }
        if(f=="cuml"){
            C out=listArg(a[0],"Cuml");
            for(std::size_t i=1;i<out.v.size();++i)out.v[i]=op('+',out.v[i-1],out.v[i]);
            return out;
        }
        if(f=="percent"&&a[0].list()){
            const E total=fold(a[0],'+',0);
            if(D::isZero(total))throw MathError("Math ERROR: Percent di una lista con somma nulla");
            C out=a[0];
            const E hundred=D::fromInt(100);
            for(E& e:out.v)e=op('*',op('/',e,total),hundred);
            return out;
        }
        if(f=="dlist"){
            const C& l=listArg(a[0],"\xCE\x94List");
            if(l.v.size()<2)throw DimensionError("Dimension ERROR: \xCE\x94List richiede almeno due elementi");
            C out=listOf(l.v.size()-1);
            for(std::size_t i=0;i+1<l.v.size();++i)out.v[i]=op('-',l.v[i+1],l.v[i]);
            return out;
        }
        if(f=="sorta"||f=="sortd"){
            C out=listArg(a[0],f=="sorta"?"SortA":"SortD");
            out.v=sorted(out,f=="sortd");
            return out;
        }
        return mapCall(f,a);
    }
};

Complex evalCollectionScalar(const ast::Node& n,Env& env){
    CollEval<NumDomain> evaluator(env);
    const Coll<Complex> c=evaluator.eval(n);
    if(!c.scalar()){
        env.at=&n;
        throw TypeError(std::string("Type ERROR: qui serve un numero, il risultato e' una ")+valueTypeName(c.type));
    }
    return c.s;
}
bool exactCollectionScalar(const ast::Node& n,Env& env,exact::Value& out){
    try{
        CollEval<ExactDomain> evaluator(env);
        const Coll<exact::Value> c=evaluator.eval(n);
        if(!c.scalar())return false;
        out=c.s;
        return true;
    }catch(const NoExactForm&){return false;}
    catch(const std::exception&){return false;}
}
double tidyReal(double v){
    if(!std::isfinite(v))throw MathError("Math ERROR: risultato non finito");
    return v==0.0?0.0:v;   // normalizza -0
}
Complex tidyComplex(Complex v){
    const double scale=std::max(std::abs(v.real()),std::abs(v.imag()));
    double re=v.real(),im=v.imag();
    if(std::abs(re)<1e-14*scale)re=0;
    if(std::abs(im)<1e-14*scale)im=0;
    return {re==0.0?0.0:re,im==0.0?0.0:im};
}
bool isEditorKind(ast::Kind k){
    using ast::Kind;
    // Un nodo Matrix e' un modello dell'editor quando le sue celle sono righe.
    return k==Kind::Row||k==Kind::Fraction||k==Kind::Radical||k==Kind::Power||k==Kind::Subscript||k==Kind::Calculus;
}
bool containsEditorNodes(const ast::Node& n){
    if(isEditorKind(n.kind))return true;
    if(n.kind==ast::Kind::Matrix&&!(n.flags&ast::FlagCollection))return true;
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
ast::NodePtr parseExpression(const std::string& expression,unsigned integerBase){
    if(integerBase==0)return parseExpression(expression);
    if(integerBase!=2&&integerBase!=8&&integerBase!=10&&integerBase!=16)throw std::invalid_argument("base non valida");
    return Parser(Lexer(expression,integerBase,true).run()).run();
}
void setEvalStackBudget(std::size_t bytes) noexcept{gEvalStackBudget=bytes;}
std::size_t evalStackBudget() noexcept{return gEvalStackBudget;}

double evaluateReal(const ast::Node& parsed,const EvalContext& context){
    Env env{context};
    env.stackBase=stackPosition();
    const ErrorLocationGuard location(env);
    const double v=evalNode<double>(parsed,env);
    env.at=&parsed;
    return tidyReal(v);
}
Complex evaluateComplex(const ast::Node& parsed,const EvalContext& context){
    Env env{context};
    env.stackBase=stackPosition();
    const ErrorLocationGuard location(env);
    return tidyComplex(evalNode<Complex>(parsed,env));
}
CalcValue evaluateAny(const ast::Node& parsed,const EvalContext& context){
    Env env{context};
    env.stackBase=stackPosition();
    const ErrorLocationGuard location(env);
    const bool real=context.complexMode==ComplexMode::Real;
    CollEval<NumDomain> evaluator(env);
    Coll<Complex> c=evaluator.eval(parsed);
    env.at=&parsed;
    auto tidy=[real](const Complex& z){return real?Complex(tidyReal(z.real()),0.0):tidyComplex(z);};
    if(c.scalar())return CalcValue::scalarOf(tidy(c.s));
    CalcValue out;
    out.type=c.type;
    out.rows=static_cast<std::uint16_t>(c.rows);out.cols=static_cast<std::uint16_t>(c.cols);
    out.re.assign(c.v.size(),0.0);
    for(std::size_t i=0;i<c.v.size();++i){
        const Complex z=tidy(c.v[i]);
        if(!std::isfinite(z.real())||!std::isfinite(z.imag()))throw MathError("Math ERROR: risultato non finito");
        out.set(i,z);
    }
    return out;
}
bool evaluateExactAny(const ast::Node& parsed,const EvalContext& context,ExactCollection& out){
    try{
        Env env{context};
        env.stackBase=stackPosition();
        CollEval<ExactDomain> evaluator(env);
        Coll<exact::Value> c=evaluator.eval(parsed);
        ExactCollection result;
        result.type=c.type;
        result.decimal=env.decimal;
        if(c.scalar()){result.rows=result.cols=1;result.elements.assign(1,c.s);}
        else{
            result.rows=static_cast<std::uint16_t>(c.rows);result.cols=static_cast<std::uint16_t>(c.cols);
            result.elements=std::move(c.v);
        }
        for(const exact::Value& e:result.elements)if(!std::isfinite(e.toDouble()))return false;
        out=std::move(result);
        return true;
    }catch(const NoExactForm&){return false;}
    catch(...){return false;}
}
bool evaluateExact(const ast::Node& parsed,const EvalContext& context,exact::Value& out,bool* decimalInput){
    try{
        Env env{context};
        env.stackBase=stackPosition();
        exact::Value v;
        if(!exactNode(parsed,env,v))return false;
        out=v;
        if(decimalInput)*decimalInput=env.decimal;
        return true;
    }catch(...){return false;}
}
const char* const* systemVariableNames() noexcept{return kSystemNames;}
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
double integrateFunction(const std::function<double(double)>& f,double a,double b,double tolerance){
    auto g=[&](double v){const double r=f(v);if(!std::isfinite(r))throw MathError("Math ERROR: funzione non definita nell'intervallo");return r;};
    return integrateFn(g,a,b,tolerance);
}
double differentiateFunction(const std::function<double(double)>& f,double x){
    auto g=[&](double v){return f(v);};
    return ridders(g,x);
}
std::string rewriteIdentifiers(const std::string& expression,const std::function<std::string(const std::string&)>& map){
    const std::vector<Tok> toks=Lexer(expression).run();
    std::string out;
    auto word=[](char c){return std::isalnum(static_cast<unsigned char>(c))||c=='_'||c=='@'||c=='.';};
    auto append=[&](const std::string& t){
        if(t.empty())return;
        // Keep adjacent words and numbers apart ("sin x", "2 y").
        if(!out.empty()&&word(out.back())&&word(t.front()))out+=' ';
        out+=t;
    };
    for(const Tok& t:toks){
        switch(t.type){
        case TT::End: break;
        case TT::Num: append(t.text); break;
        case TT::Ref: append(t.text=="mat"?"Mat":t.text=="vct"?"Vct":"List"); break;
        case TT::Ident:{const std::string r=map(t.text);append(r.empty()?t.text:r);break;}
        case TT::Op:
            switch(t.op){
            case 'l':append("<=");break; case 'g':append(">=");break; case 'n':append("\xE2\x89\xA0");break;
            case '&':append(" and ");break; case '|':append(" or ");break; case 'x':append(" xor ");break; case 'X':append(" xnor ");break;
            case '#':{
                for(const auto& e:kEngSymbols)if(t.text.compare(1,std::string::npos,e.factor)==0){append(std::string("#")+e.symbol);break;}
                break;
            }
            case 'r':append(" Rmdr ");break; case 'q':append("Int\xC3\xB7");break; case 'd':append("\xC2\xB0");break;
            default:append(std::string(1,t.op));break;
            }
            break;
        }
    }
    return out;
}
bool isFunctionReferenceName(const std::string& lowerName) noexcept{
    try{return referenceName(lowerName);}catch(...){return false;}
}
bool isReservedWord(const std::string& lowerName){return isFunctionName(lowerName)||isConstantName(lowerName)||operatorWord(lowerName)!=0;}
void seedRandom(std::uint32_t seed) noexcept{g_rng=seed?seed:0x9E3779B9u;}
} // namespace fx
