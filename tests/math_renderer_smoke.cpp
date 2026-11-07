#include "math_renderer.hpp"
#include <cassert>
#include <memory>
int main(){
 using namespace fx;
 auto root=std::make_shared<NaturalNode>();root->kind=NaturalNodeKind::Row;
 auto frac=std::make_shared<NaturalNode>();frac->kind=NaturalNodeKind::Fraction;
 auto n=std::make_shared<NaturalNode>();n->kind=NaturalNodeKind::Row;auto t=std::make_shared<NaturalNode>();t->kind=NaturalNodeKind::Token;t->text="x+1";n->children.push_back(t);
 auto d=std::make_shared<NaturalNode>();d->kind=NaturalNodeKind::Row;auto u=std::make_shared<NaturalNode>();u->kind=NaturalNodeKind::Token;u->text="2";d->children.push_back(u);frac->children={n,d};root->children.push_back(frac);
 auto m=math::measure(*root);assert(m.width>0&&m.height>14);
 auto l=math::layout(*root,10,100,200,180);assert(!l.clipped);bool hasRule=false;for(const auto&o:l.ops)if(o.kind==NaturalDrawOp::Kind::Line)hasRule=true;assert(hasRule);
 auto clipped=math::layout(*root,0,0,1,1);assert(clipped.clipped);
 auto centered=math::layout(*root,0,100,200,180,math::Align::Center);assert(centered.x==(200-m.width)/2);
 auto right=math::layout(*root,0,100,200,180,math::Align::Right);assert(right.x==200-m.width);
 auto partial=math::layout(*root,0,100,8,180);assert(partial.clipped);bool textFits=true;for(const auto&o:partial.ops)if(o.kind==NaturalDrawOp::Kind::Text)textFits &= o.x>=0 && o.x+static_cast<int>(o.text.size())*6<=8;assert(textFits);
 auto crossing=std::make_shared<NaturalNode>();crossing->kind=NaturalNodeKind::Fraction;crossing->children={n,d};auto narrow=math::layout(*crossing,0,40,8,30);for(const auto&o:narrow.ops)if(o.kind==NaturalDrawOp::Kind::Line){assert(o.x>=0&&o.x2<=8);}
 // The actual on-device layout path converts a typed Greek alias to one glyph.
 NaturalEditor greekEditor;for(char c:std::string("alpha"))greekEditor.insert(c);
 auto greekLayout=math::layout(greekEditor.root(),0,20,200,40);bool alphaGlyph=false;
 for(const auto& op:greekLayout.ops)if(op.kind==NaturalDrawOp::Kind::Text&&op.text=="α")alphaGlyph=true;
 assert(alphaGlyph);
 NaturalEditor exponentEditor;exponentEditor.insert('x');exponentEditor.insert('^');exponentEditor.insert('3');
 auto exponentLayout=math::layout(exponentEditor.root(),0,30,200,50);bool exponentRaised=false;
 for(const auto& op:exponentLayout.ops)if(op.kind==NaturalDrawOp::Kind::Text&&op.text=="3"&&op.y<30)exponentRaised=true;
 assert(exponentRaised);
 NaturalEditor fifthRootEditor;for(char c:std::string("root5"))fifthRootEditor.insert(c);fifthRootEditor.insert('3');fifthRootEditor.insert('2');
 auto fifthRootLayout=math::layout(fifthRootEditor.root(),0,30,200,50);bool degreeFive=false;for(const auto& op:fifthRootLayout.ops)if(op.kind==NaturalDrawOp::Kind::Text&&op.text=="5")degreeFive=true;assert(degreeFive);
 auto empty=std::make_shared<NaturalNode>();empty->kind=NaturalNodeKind::Fraction;auto invalid=math::measure(*empty);assert(invalid.width==0);
}
