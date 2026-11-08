#include "memory.hpp"
#include <cctype>
namespace fx {
namespace {
std::size_t slotIndex(char n){n=static_cast<char>(std::toupper(static_cast<unsigned char>(n)));if(n<'A'||n>'Z')throw std::invalid_argument("Variabile ammessa: A-Z");return static_cast<std::size_t>(n-'A');}
}
void CalculatorMemory::store(char n,double v){const auto i=slotIndex(n);vars_[i]=v;used_[i]=true;exact_[i]=ExactSlot{};}
void CalculatorMemory::storeExact(char n,double v,const exact::Value& e,bool decimal){const auto i=slotIndex(n);vars_[i]=v;used_[i]=true;exact_[i]=ExactSlot{e,true,decimal};}
bool CalculatorMemory::exactRecall(char n,exact::Value& out,bool& decimal) const{
 const auto i=slotIndex(n);
 if(!used_[i]){out=exact::Value{};decimal=false;return true;}
 if(!exact_[i].valid)return false;
 out=exact_[i].value;decimal=exact_[i].decimal;return true;
}
const ExactSlot& CalculatorMemory::exactSlot(char n) const{return exact_[slotIndex(n)];}
bool CalculatorMemory::used(char n) const{return used_[slotIndex(n)];}
void CalculatorMemory::setAnswerExact(double v,const exact::Value& e,bool decimal){setAnswer(v,0.0);exactAns_=ExactSlot{e,true,decimal};}
bool CalculatorMemory::exactAnswer(exact::Value& out,bool& decimal) const{if(!exactAns_.valid)return false;out=exactAns_.value;decimal=exactAns_.decimal;return true;}
bool CalculatorMemory::exactPreviousAnswer(exact::Value& out,bool& decimal) const{if(!exactPreAns_.valid)return false;out=exactPreAns_.value;decimal=exactPreAns_.decimal;return true;}
void CalculatorMemory::restoreAnswers(double a,double ai,const ExactSlot& ae,double p,double pi,const ExactSlot& pe){answer_=a;answerImag_=ai;exactAns_=ae;previousAnswer_=p;previousAnswerImag_=pi;exactPreAns_=pe;}
double CalculatorMemory::recall(char n)const{n=static_cast<char>(std::toupper(static_cast<unsigned char>(n)));if(n<'A'||n>'Z')throw std::invalid_argument("Variabile ammessa: A-Z");// fx-CG50 behaviour: variables that were never assigned read as 0.
return used_[n-'A']?vars_[n-'A']:0.0;}
void CalculatorMemory::clear(char n){const auto i=slotIndex(n);vars_[i]=0;used_[i]=false;exact_[i]=ExactSlot{};}
void CalculatorMemory::clearAll(){vars_.fill(0);used_.fill(false);exact_.fill(ExactSlot{});answer_=previousAnswer_=independent_=0;answerImag_=previousAnswerImag_=0;exactAns_=exactPreAns_=ExactSlot{};}
void CalculatorMemory::setAnswer(double v,double im){previousAnswer_=answer_;previousAnswerImag_=answerImag_;exactPreAns_=exactAns_;answer_=v;answerImag_=im;exactAns_=ExactSlot{};}
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
