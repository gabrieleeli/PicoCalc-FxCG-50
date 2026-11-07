#include "memory.hpp"
#include <cctype>
namespace fx {
void CalculatorMemory::store(char n,double v){n=static_cast<char>(std::toupper(static_cast<unsigned char>(n)));if(n<'A'||n>'Z')throw std::invalid_argument("Variabile ammessa: A-Z");vars_[n-'A']=v;used_[n-'A']=true;}
double CalculatorMemory::recall(char n)const{n=static_cast<char>(std::toupper(static_cast<unsigned char>(n)));if(n<'A'||n>'Z')throw std::invalid_argument("Variabile ammessa: A-Z");if(!used_[n-'A'])throw std::domain_error("Variabile non inizializzata");return vars_[n-'A'];}
void CalculatorMemory::clear(char n){n=static_cast<char>(std::toupper(static_cast<unsigned char>(n)));if(n<'A'||n>'Z')throw std::invalid_argument("Variabile ammessa: A-Z");vars_[n-'A']=0;used_[n-'A']=false;}
void CalculatorMemory::clearAll(){vars_.fill(0);used_.fill(false);answer_=previousAnswer_=independent_=0;}
void CalculatorMemory::setAnswer(double v){previousAnswer_=answer_;answer_=v;}
namespace {
std::string superscript(char c){switch(c){case '0':return "⁰";case '1':return "¹";case '2':return "²";case '3':return "³";case '4':return "⁴";case '5':return "⁵";case '6':return "⁶";case '7':return "⁷";case '8':return "⁸";case '9':return "⁹";case '-':return "⁻";case '+':return "⁺";default:return std::string(1,c);}}
struct Symbol {const char* name; const char* glyph;};
constexpr Symbol greek[]={
 {"alpha","α"},{"beta","β"},{"gamma","γ"},{"delta","δ"},{"epsilon","ε"},{"zeta","ζ"},{"eta","η"},{"theta","θ"},{"iota","ι"},{"kappa","κ"},{"lambda","λ"},{"mu","μ"},{"nu","ν"},{"xi","ξ"},{"omicron","ο"},{"pi","π"},{"rho","ρ"},{"sigma","σ"},{"tau","τ"},{"upsilon","υ"},{"phi","φ"},{"chi","χ"},{"psi","ψ"},{"omega","ω"},
 {"Alpha","Α"},{"Beta","Β"},{"Gamma","Γ"},{"Delta","Δ"},{"Epsilon","Ε"},{"Zeta","Ζ"},{"Eta","Η"},{"Theta","Θ"},{"Iota","Ι"},{"Kappa","Κ"},{"Lambda","Λ"},{"Mu","Μ"},{"Nu","Ν"},{"Xi","Ξ"},{"Omicron","Ο"},{"Pi","Π"},{"Rho","Ρ"},{"Sigma","Σ"},{"Tau","Τ"},{"Upsilon","Υ"},{"Phi","Φ"},{"Chi","Χ"},{"Psi","Ψ"},{"Omega","Ω"}};
constexpr Symbol constants[]={
 {"speedOfLight","c"},{"physc","c"},{"gravitationalConstant","G"},{"physg","G"},{"planckConstant","h"},{"physh","h"},{"avogadro","Nₐ"},{"NA","Nₐ"},{"boltzmann","k_B"},{"kB","k_B"},{"elementaryCharge","qₑ"},{"qe","qₑ"},{"standardGravity","g₀"},{"g0","g₀"},{"goldenRatio","φ"},{"tau","τ"},{"eulerGamma","γ"},{"sqrt2","√2"},{"ln2","ln(2)"},{"ln10","ln(10)"}};
bool wordChar(char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';}
}
std::string formatNatural(const std::string& in){
 std::string s;s.reserve(in.size()*2);
 for(std::size_t i=0;i<in.size();){
  if(in[i]=='^'){++i;if(i<in.size()&&in[i]=='('){++i;while(i<in.size()&&in[i]!=')')s+=superscript(in[i++]);if(i<in.size())++i;}else{if(i<in.size()&&(in[i]=='-'||in[i]=='+'))s+=superscript(in[i++]);while(i<in.size()&&std::isdigit(static_cast<unsigned char>(in[i])))s+=superscript(in[i++]);}continue;}
  if(in.compare(i,5,"root(")==0){std::size_t comma=in.find(',',i+5),close=in.find(')',comma==std::string::npos?i+5:comma+1);if(comma!=std::string::npos&&close!=std::string::npos){std::string degree=in.substr(i+5,comma-(i+5)),radicand=in.substr(comma+1,close-(comma+1));while(!degree.empty()&&degree.front()==' ')degree.erase(degree.begin());while(!degree.empty()&&degree.back()==' ')degree.pop_back();if(degree.size()==1&&degree[0]>='2'&&degree[0]<='9')s+=superscript(degree[0]);else s+=degree;s+="√("+formatNatural(radicand)+")";i=close+1;continue;}}
  if(wordChar(in[i])){std::size_t j=i;while(j<in.size()&&wordChar(in[j]))++j;std::string word=in.substr(i,j-i);bool replaced=false;
   for(const auto& item:greek)if(word==item.name){s+=item.glyph;replaced=true;break;}
   if(!replaced)for(const auto& item:constants)if(word==item.name){s+=item.glyph;replaced=true;break;}
   if(!replaced){if(word=="sqrt")s+="√";else if(word=="cbrt")s+="∛";else if(word=="integral"||word=="integrate")s+="∫";else if(word=="derivative"||word=="diff")s+="d/dx";else if(word=="sum")s+="Σ";else if(word=="pi")s+="π";else if(word=="ans"||word=="Ans")s+="Ans";else s+=word;}i=j;continue;
  }
  char c=in[i++];if(c=='*')s+="×";else if(c=='/')s+="÷";else if(c=='-')s+="−";else s+=c;
 }
 return s;
}
}
