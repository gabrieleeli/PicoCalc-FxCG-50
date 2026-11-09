#include "natural_editor.hpp"
#include "expression.hpp"
#include "limits.hpp"
#include <new>
#include "memory.hpp"
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <cstring>

namespace fx { namespace {
using NodePtr=std::shared_ptr<NaturalNode>;
NodePtr node(NaturalNodeKind k){auto n=ast::make(k);n->kind=k;return n;}
NodePtr row(){return node(NaturalNodeKind::Row);}
std::pair<std::size_t,NodePtr> takeTrailingOperand(NaturalNode& line,std::size_t cursor){
 if(cursor==0)throw std::runtime_error("Manca l'operando a sinistra");
 std::size_t begin=cursor;const auto& last=*line.children[cursor-1];
 if(last.kind==NaturalNodeKind::Token){
  const unsigned char c=static_cast<unsigned char>(last.text.empty()?0:last.text[0]);
  if(!std::isalnum(c)&&c!='.')throw std::runtime_error("Operando non valido prima del modello");
  while(begin>0){const auto& n=*line.children[begin-1];if(n.kind!=NaturalNodeKind::Token||n.text.empty())break;unsigned char ch=static_cast<unsigned char>(n.text[0]);if(!std::isalnum(ch)&&ch!='.')break;--begin;}
 }else --begin;
 auto operand=row();operand->children.insert(operand->children.end(),line.children.begin()+static_cast<std::ptrdiff_t>(begin),line.children.begin()+static_cast<std::ptrdiff_t>(cursor));
 line.children.erase(line.children.begin()+static_cast<std::ptrdiff_t>(begin),line.children.begin()+static_cast<std::ptrdiff_t>(cursor));
 return {begin,operand};
}

std::size_t countNodes(const NaturalNode& n){std::size_t total=1;for(const auto& c:n.children)if(c)total+=countNodes(*c);return total;}

struct Box {int w{},h{14},base{10};};
std::size_t glyphCount(const std::string& s){std::size_t n=0;for(unsigned char c:s)if((c&0xC0)!=0x80)++n;return n;}
int rootPrefixWidth(const NaturalNode& n){return n.text.size()>1?static_cast<int>(n.text.size())*8:(n.text=="3"||n.text=="4"?0:(n.text.empty()||n.text=="2"?0:8));}
std::string tokenRun(const NaturalNode& n,std::size_t begin,std::size_t end){std::string s;for(std::size_t i=begin;i<end;++i)s+=n.children[i]->text;return s;}
std::string shown(const std::string& s){return formatNatural(s);}
unsigned autoRootIndex(const std::string& s){
 if(s=="sqrt") return 2;
 if(s=="cbrt") return 3;
 if(s.size()>=5&&s.size()<=6&&s.compare(0,4,"root")==0){char* end=nullptr;unsigned long n=std::strtoul(s.c_str()+4,&end,10);if(end&&*end=='\0'&&n>=2&&n<=99)return static_cast<unsigned>(n);}
 return 0;
}

Box measure(const NaturalNode&n){
 if(n.kind==NaturalNodeKind::Token)return {static_cast<int>(glyphCount(shown(n.text)))*8,14,10};
 if(n.kind==NaturalNodeKind::Row){Box b{0,14,10};for(std::size_t i=0;i<n.children.size();){if(n.children[i]->kind==NaturalNodeKind::Token){std::size_t j=i;while(j<n.children.size()&&n.children[j]->kind==NaturalNodeKind::Token)++j;b.w+=static_cast<int>(glyphCount(shown(tokenRun(n,i,j))))*8;i=j;}else{auto q=measure(*n.children[i]);b.w+=q.w;b.h=std::max(b.h,q.h);++i;}}return b;}
 if(n.kind==NaturalNodeKind::Fraction){auto a=measure(*n.children[0]),b=measure(*n.children[1]);return {std::max(a.w,b.w)+8,a.h+b.h+7,a.h+4};}
 if(n.kind==NaturalNodeKind::Radical){auto a=measure(*n.children[0]);return {a.w+17+rootPrefixWidth(n),a.h+5,a.base+3};}
 if(n.kind==NaturalNodeKind::Power||n.kind==NaturalNodeKind::Subscript){auto a=measure(*n.children[0]),b=measure(*n.children[1]);return {a.w+b.w,a.h+std::max(0,b.h-7),a.base+std::max(0,b.h-7)};}
 if(n.kind==NaturalNodeKind::Calculus){int w=n.text=="derivative"?32:18;int h=14;for(const auto& c:n.children){auto q=measure(*c);w+=q.w+8;h=std::max(h,q.h);}return {w,h,10};}
 auto a=measure(*n.children[0]);return {a.w+16,a.h,a.base};
}
int rowPrefixWidth(const NaturalNode& rowNode,std::size_t end){int width=0;for(std::size_t i=0;i<end;){if(rowNode.children[i]->kind==NaturalNodeKind::Token){std::size_t j=i;while(j<end&&rowNode.children[j]->kind==NaturalNodeKind::Token)++j;width+=static_cast<int>(glyphCount(shown(tokenRun(rowNode,i,j))))*8;i=j;}else{width+=measure(*rowNode.children[i]).w;++i;}}return width;}
void draw(const NaturalNode&n,int x,int base,std::vector<NaturalDrawOp>&out){
 auto addText=[&](int xx,int yy,const std::string&s){out.push_back({NaturalDrawOp::Kind::Text,xx,yy,0,0,s});};
 if(n.kind==NaturalNodeKind::Token){addText(x,base,shown(n.text));return;}
 if(n.kind==NaturalNodeKind::Row){int xx=x;for(std::size_t i=0;i<n.children.size();){if(n.children[i]->kind==NaturalNodeKind::Token){std::size_t j=i;while(j<n.children.size()&&n.children[j]->kind==NaturalNodeKind::Token)++j;std::string raw=tokenRun(n,i,j);addText(xx,base,shown(raw));xx+=static_cast<int>(glyphCount(shown(raw)))*8;i=j;}else{draw(*n.children[i],xx,base,out);xx+=measure(*n.children[i]).w;++i;}}return;}
 if(n.kind==NaturalNodeKind::Fraction){auto a=measure(*n.children[0]),b=measure(*n.children[1]);int w=std::max(a.w,b.w)+8;draw(*n.children[0],x+(w-a.w)/2,base-a.h-2,out);out.push_back({NaturalDrawOp::Kind::Line,x,base-1,x+w,base-1,{}});draw(*n.children[1],x+(w-b.w)/2,base+b.h+3,out);return;}
 if(n.kind==NaturalNodeKind::Radical){auto a=measure(*n.children[0]);int prefix=0;if(rootPrefixWidth(n)>0){addText(x,base-5,n.text);prefix=rootPrefixWidth(n);}addText(x+prefix,base,n.text=="3"?"∛":(n.text=="4"?"∜":"√"));out.push_back({NaturalDrawOp::Kind::Line,x+prefix+13,base-a.h,x+prefix+15+a.w,base-a.h,{}});draw(*n.children[0],x+prefix+16,base,out);return;}
 if(n.kind==NaturalNodeKind::Power||n.kind==NaturalNodeKind::Subscript){auto a=measure(*n.children[0]);draw(*n.children[0],x,base,out);draw(*n.children[1],x+a.w,base-(n.kind==NaturalNodeKind::Power?8:-2),out);return;}
 if(n.kind==NaturalNodeKind::Calculus){const std::string symbol=n.text=="integral"?"∫":(n.text=="sum"?"Σ":"d/dx");addText(x,base,symbol);int xx=x+(n.text=="derivative"?32:18);for(std::size_t i=0;i<n.children.size();++i){if(i) addText(xx,base, i==1?",":";");if(i)xx+=8;draw(*n.children[i],xx,base,out);xx+=measure(*n.children[i]).w+4;}return;}
 addText(x,base,"(");draw(*n.children[0],x+8,base,out);addText(x+8+measure(*n.children[0]).w,base,")");
}
}
// Called before any structural change: enforces the nesting limit (recursive
// layout/serialisation run on the SRAM stack), the node budget (memory) and
// reserves every container that the change will grow, so that a
// std::bad_alloc can only happen before the tree is modified.
void NaturalEditor::prepareInsert(bool structural){
 if(structural&&rowPath_.size()/2>=fx::limits::kMaxEditorNesting)throw std::length_error("Annidamento massimo raggiunto");
 if(countNodes(*root_)+4>fx::limits::editorMaxTokens())throw std::length_error("Espressione troppo lunga per la memoria disponibile");
 auto& r=activeRow();
 r.children.reserve(r.children.size()+1);
 if(structural)rowPath_.reserve(rowPath_.size()+2);
}
namespace { void setError(std::string& dst,const char* text) noexcept { const std::size_t n=std::min(std::strlen(text),dst.capacity()); dst.assign(text,n); } }
NaturalEditor::NaturalEditor(){error_.reserve(96);clear();}
void NaturalEditor::setExpression(const std::string& expression){ auto parsed=parseExpression(expression); root_=ast::make(NaturalNodeKind::Row); root_->children.push_back(std::move(parsed)); rowPath_.clear(); cursor_=root_->children.size(); error_.clear(); autoRootParenPending_=false; autoRootParenOpened_=false; }
void NaturalEditor::clear(){root_=row();rowPath_.clear();cursor_=0;error_.clear();autoRootParenPending_=false;autoRootParenOpened_=false;}
NaturalNode& NaturalEditor::activeRow(){NaturalNode*n=root_.get();for(auto i:rowPath_){if(i>=n->children.size())throw std::logic_error("Percorso cursore non valido");n=n->children[i].get();}if(n->kind!=NaturalNodeKind::Row)throw std::logic_error("Il cursore non punta a una riga");return *n;}
const NaturalNode& NaturalEditor::activeRow()const{return const_cast<NaturalEditor*>(this)->activeRow();}
void NaturalEditor::insert(char c){
 if(c=='^'){insertPower();return;}
 // A function-style opening parenthesis is optional after an auto-recognized root alias.
 // Closing it returns the cursor to the expression row, keeping the root as one VPAM node.
 if(c=='(' && autoRootParenPending_){autoRootParenPending_=false;autoRootParenOpened_=true;return;}
 if(c==')' && autoRootParenOpened_){
  if(rowPath_.size()>=2){const auto at=rowPath_[rowPath_.size()-2];rowPath_.resize(rowPath_.size()-2);cursor_=at+1;}
  autoRootParenOpened_=false;error_.clear();return;
 }
 if(autoRootParenPending_)autoRootParenPending_=false;
 if(!(std::isalnum(static_cast<unsigned char>(c))||c=='.'||c=='+'||c=='-'||c=='*'||c=='/'||c=='%'||c=='!'||c==','||c=='('||c==')'))throw std::invalid_argument("Carattere non ammesso nell'espressione");
 prepareInsert(false);
 auto t=node(NaturalNodeKind::Token);t->text.assign(1,c);auto&r=activeRow();r.children.insert(r.children.begin()+static_cast<std::ptrdiff_t>(cursor_),t);++cursor_;error_.clear();
 if(std::isalnum(static_cast<unsigned char>(c))){std::string tail;std::size_t begin=cursor_;while(begin>0&&r.children[begin-1]->kind==NaturalNodeKind::Token&&r.children[begin-1]->text.size()==1&&(std::isalnum(static_cast<unsigned char>(r.children[begin-1]->text[0])))){tail.insert(tail.begin(),r.children[begin-1]->text[0]);--begin;}
  if(const unsigned index=autoRootIndex(tail);index&&rowPath_.size()/2<fx::limits::kMaxEditorNesting){r.children.erase(r.children.begin()+static_cast<std::ptrdiff_t>(begin),r.children.begin()+static_cast<std::ptrdiff_t>(cursor_));cursor_=begin;insertNthRoot(index);autoRootParenPending_=true;autoRootParenOpened_=false;}
 }
}
bool NaturalEditor::enterChild(std::size_t c){auto&r=activeRow();if(cursor_==0||cursor_>r.children.size())return false;auto p=r.children[cursor_-1];if(c>=p->children.size()||p->children[c]->kind!=NaturalNodeKind::Row)return false;rowPath_.push_back(cursor_-1);rowPath_.push_back(0);cursor_=0;return true;}
void NaturalEditor::insertFraction(){prepareInsert(true);auto f=node(NaturalNodeKind::Fraction);f->children={row(),row()};auto&r=activeRow();std::size_t at=cursor_;if(cursor_>0){auto operand=takeTrailingOperand(r,cursor_);at=operand.first;f->children[0]=operand.second;}r.children.insert(r.children.begin()+static_cast<std::ptrdiff_t>(at),f);rowPath_.push_back(at);rowPath_.push_back(0);cursor_=f->children[0]->children.size();}
void NaturalEditor::insertRadical(){insertNthRoot(2);}
void NaturalEditor::insertNthRoot(unsigned index){if(index<2||index>99)throw std::invalid_argument("Indice della radice: usare 2-99");prepareInsert(true);auto f=node(NaturalNodeKind::Radical);f->text=index==2?"2":std::to_string(index);f->children={row()};auto&r=activeRow();r.children.insert(r.children.begin()+static_cast<std::ptrdiff_t>(cursor_),f);rowPath_.push_back(cursor_);rowPath_.push_back(0);cursor_=0;}
void NaturalEditor::insertPower(){prepareInsert(true);auto&r=activeRow();auto operand=takeTrailingOperand(r,cursor_);auto p=node(NaturalNodeKind::Power);p->children={operand.second,row()};r.children.insert(r.children.begin()+static_cast<std::ptrdiff_t>(operand.first),p);rowPath_.push_back(operand.first);rowPath_.push_back(1);cursor_=0;}
void NaturalEditor::insertSubscript(){prepareInsert(true);auto&r=activeRow();auto operand=takeTrailingOperand(r,cursor_);auto p=node(NaturalNodeKind::Subscript);p->children={operand.second,row()};r.children.insert(r.children.begin()+static_cast<std::ptrdiff_t>(operand.first),p);rowPath_.push_back(operand.first);rowPath_.push_back(1);cursor_=0;}
void NaturalEditor::insertGroup(){prepareInsert(true);auto g=node(NaturalNodeKind::Group);g->children={row()};auto&r=activeRow();r.children.insert(r.children.begin()+static_cast<std::ptrdiff_t>(cursor_),g);rowPath_.push_back(cursor_);rowPath_.push_back(0);cursor_=0;}
void NaturalEditor::insertIntegral(){prepareInsert(true);auto n=node(NaturalNodeKind::Calculus);n->text="integral";n->children={row(),row(),row()};auto&r=activeRow();r.children.insert(r.children.begin()+static_cast<std::ptrdiff_t>(cursor_),n);rowPath_.push_back(cursor_);rowPath_.push_back(0);cursor_=0;}
void NaturalEditor::insertDerivative(){prepareInsert(true);auto n=node(NaturalNodeKind::Calculus);n->text="derivative";n->children={row(),row()};auto&r=activeRow();r.children.insert(r.children.begin()+static_cast<std::ptrdiff_t>(cursor_),n);rowPath_.push_back(cursor_);rowPath_.push_back(0);cursor_=0;}
void NaturalEditor::insertSummation(){prepareInsert(true);auto n=node(NaturalNodeKind::Calculus);n->text="sum";n->children={row(),row(),row()};auto&r=activeRow();r.children.insert(r.children.begin()+static_cast<std::ptrdiff_t>(cursor_),n);rowPath_.push_back(cursor_);rowPath_.push_back(0);cursor_=0;}
bool NaturalEditor::moveLeft(){if(cursor_){--cursor_;return true;}return leaveChild(false);}
bool NaturalEditor::moveRight(){auto&r=activeRow();if(cursor_<r.children.size()){++cursor_;return true;}return leaveChild(true);}
bool NaturalEditor::leaveChild(bool right){if(rowPath_.size()<2)return false;auto child=rowPath_[rowPath_.size()-2];rowPath_.resize(rowPath_.size()-2);(void)activeRow();cursor_=child+(right?1:0);return true;}
bool NaturalEditor::moveUp(){const std::size_t depth=rowPath_.size();if(depth<2)return false;const auto child=rowPath_[depth-2];NaturalNode* parent=root_.get();for(std::size_t i=0;i+2<depth;++i)parent=parent->children.at(rowPath_[i]).get();if(child>=parent->children.size())return false;auto box=parent->children[child];if(box->children.empty()||box->children[0]->kind!=NaturalNodeKind::Row)return false;rowPath_[depth-1]=box->kind==NaturalNodeKind::Calculus&&rowPath_[depth-1]>0?rowPath_[depth-1]-1:0;cursor_=box->children[rowPath_[depth-1]]->children.size();return true;}
bool NaturalEditor::moveDown(){const std::size_t depth=rowPath_.size();if(depth<2)return false;const auto child=rowPath_[depth-2];NaturalNode* parent=root_.get();for(std::size_t i=0;i+2<depth;++i)parent=parent->children.at(rowPath_[i]).get();if(child>=parent->children.size())return false;auto box=parent->children[child];if(box->children.size()<2||box->children[1]->kind!=NaturalNodeKind::Row)return false;std::size_t next=rowPath_[depth-1]+1;if(box->kind!=NaturalNodeKind::Calculus)next=1;if(next>=box->children.size())return false;rowPath_[depth-1]=next;cursor_=box->children[next]->children.size();return true;}
bool NaturalEditor::backspace(){if(!cursor_)return leaveChild(false);auto&r=activeRow();r.children.erase(r.children.begin()+static_cast<std::ptrdiff_t>(--cursor_));return true;}
bool NaturalEditor::eraseForward(){auto&r=activeRow();if(cursor_>=r.children.size())return false;r.children.erase(r.children.begin()+static_cast<std::ptrdiff_t>(cursor_));return true;}
void NaturalEditor::handle(const NaturalKeyEvent&e){error_.clear();try{switch(e.key){case NaturalKey::Left:moveLeft();break;case NaturalKey::Right:moveRight();break;case NaturalKey::Up:moveUp();break;case NaturalKey::Down:moveDown();break;case NaturalKey::Backspace:backspace();break;case NaturalKey::Delete:eraseForward();break;case NaturalKey::Enter:insertGroup();break;case NaturalKey::Character:insert(e.character);break;}}catch(const std::bad_alloc&){setError(error_,fx::limits::kOutOfMemoryMessage);}catch(const std::exception&x){setError(error_,x.what());}}
std::string NaturalEditor::toExpression()const{return ast::serialize(*root_);}
std::vector<NaturalDrawOp> NaturalEditor::layout(int x,int baseline,int maxWidth)const{std::vector<NaturalDrawOp> out;auto b=measure(*root_);if(b.w>maxWidth){out.push_back({NaturalDrawOp::Kind::Text,x,baseline,0,0,"ESPRESSIONE TROPPO LUNGA"});return out;}draw(*root_,x,baseline,out);const NaturalNode* current=root_.get();int cx=x,cy=baseline;for(std::size_t d=0;d+1<rowPath_.size();d+=2){const auto index=rowPath_[d],slot=rowPath_[d+1];if(current->kind!=NaturalNodeKind::Row||index>=current->children.size())break;cx+=rowPrefixWidth(*current,index);const auto& box=*current->children[index];if(slot>=box.children.size())break;auto a=measure(*box.children[0]);if(box.kind==NaturalNodeKind::Fraction){auto bw=measure(*box.children[1]);int w=std::max(a.w,bw.w)+8;if(slot==0){cx+=(w-a.w)/2;cy-=a.h+2;}else{cx+=(w-bw.w)/2;cy+=bw.h+3;}}else if(box.kind==NaturalNodeKind::Radical){cx+=16+rootPrefixWidth(box);}else if(box.kind==NaturalNodeKind::Power||box.kind==NaturalNodeKind::Subscript){if(slot==1){cx+=a.w;cy+=(box.kind==NaturalNodeKind::Power?-8:2);}}else if(box.kind==NaturalNodeKind::Group){cx+=8;}else if(box.kind==NaturalNodeKind::Calculus){cx+=(box.text=="derivative"?32:18);for(std::size_t k=0;k<slot;++k)cx+=measure(*box.children[k]).w+12;}current=box.children[slot].get();}if(current->kind==NaturalNodeKind::Row){cx+=rowPrefixWidth(*current,std::min(cursor_,current->children.size()));out.push_back({NaturalDrawOp::Kind::Cursor,cx,cy-10,cx,cy+4,{}});}return out;}
}
