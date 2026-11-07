#include "math_engine.hpp"
#include "point_a.hpp"
#include "expression.hpp"
#include "graph.hpp"
#include "memory.hpp"
#include "editors.hpp"
#include "natural_editor.hpp"
#include "keyboard_input.hpp"
#include "run_matrix.hpp"
#include <cassert>
#include <cmath>
int main(){
 auto surface=fx::sample3DSurface("x+y",-1,1,-1,1,-2,2,320,320,12); assert(!surface.empty());
 auto curve=fx::sample2D("x^2",-2,2,0,4,320,320); assert(!curve.empty());
 auto ineq=fx::solvePolynomialInequality(1,-3,2,fx::InequalityRelation::LessEqual);
 assert(ineq.size()==1&&std::abs(ineq[0].lower-1)<1e-9&&std::abs(ineq[0].upper-2)<1e-9&&ineq[0].lowerInclusive&&ineq[0].upperInclusive);
 auto linineq=fx::solvePolynomialInequality(0,2,-4,fx::InequalityRelation::Greater);
 assert(linineq.size()==1&&std::abs(linineq[0].lower-2)<1e-9&&!linineq[0].lowerInclusive);
 auto le=fx::solveLinearEquation(2,-6); assert(le.kind==fx::QuadraticResult::Linear&&std::abs(le.x1-3)<1e-9);
 auto joined=fx::unionSolutionSets({{0,1,true,false}},{{1,2,true,true}});
 assert(joined.size()==1&&joined[0].lower==0&&joined[0].upper==2&&joined[0].upperInclusive);
 auto common=fx::intersectSolutionSets({{0,3,true,false}},{{2,4,true,true}});
 assert(common.size()==1&&common[0].lower==2&&common[0].upper==3&&!common[0].upperInclusive);
 auto sysineq=fx::solveInequalitySystem({{{0,1,-1,2}},{{0,1,-3,0}}});
 assert(sysineq.size()==1&&std::abs(sysineq[0].lower-1)<1e-9&&std::abs(sysineq[0].upper-3)<1e-9);
 auto sys=fx::solveLinearSystem({{1,1},{1,-1}},{5,1});
 assert(sys.kind==fx::LinearResult::Unique&&std::abs(sys.solution[0]-3)<1e-9&&std::abs(sys.solution[1]-2)<1e-9);
 auto q=fx::solveQuadratic(1,-3,2); assert(q.kind==fx::QuadraticResult::TwoReal); assert(std::abs(q.x1-1)<1e-9&&std::abs(q.x2-2)<1e-9);
 assert(std::abs(fx::evaluate("sin(pi/2)")-1)<1e-9);
 assert(std::abs(fx::evaluate("tau/2")-std::acos(-1.0))<1e-12);
 assert(std::abs(fx::evaluate("speedOfLight")-299792458.0)<1e-6);
 assert(std::abs(fx::evaluate("physc")-299792458.0)<1e-6);
 assert(std::abs(fx::evaluate("physg")-6.67430e-11)<1e-22);
 assert(std::abs(fx::evaluate("physh")-6.62607015e-34)<1e-45);
 assert(std::abs(fx::evaluate("NA")-6.02214076e23)<1e10);
 assert(std::abs(fx::evaluate("kB")-1.380649e-23)<1e-34);
 assert(std::abs(fx::evaluate("qe")-1.602176634e-19)<1e-30);
 assert(std::abs(fx::evaluate("g0")-9.80665)<1e-12);
 assert(std::abs(fx::evaluate("goldenRatio")-(1.0+std::sqrt(5.0))/2.0)<1e-12);
 assert(std::abs(fx::evaluate("root(5,32)")-2.0)<1e-12);
 assert(std::abs(fx::evaluate("root(4,16)")-2.0)<1e-12);
 assert(std::abs(fx::evaluate("2^3^2")-512.0)<1e-12);
 assert(std::abs(fx::evaluate("(x+1)^2",2.0)-9.0)<1e-12);
 assert(fx::formatNatural("root(4,16)")=="⁴√(16)");
 assert(std::abs(fx::evaluate("frac(1,2)")-0.5)<1e-12);
 assert(std::abs(fx::evaluate("factorial(5)")-120.0)<1e-12);
 assert(std::abs(fx::evaluate("nCr(5,2)")-10.0)<1e-12);
 assert(std::abs(fx::evaluate("nPr(5,2)")-20.0)<1e-12);
 bool badRoot=false;try{(void)fx::evaluate("root(2.5,16)");}catch(const std::domain_error&){badRoot=true;}assert(badRoot);
 bool badFraction=false;try{(void)fx::evaluate("frac(1,0)");}catch(const std::domain_error&){badFraction=true;}assert(badFraction);
 assert(std::abs(fx::evaluate("sqrt(81)+cbrt(8)")-11)<1e-9);
 assert(std::abs(fx::evaluate("2pi")-2*std::acos(-1.0))<1e-9);
 assert(std::abs(fx::evaluate("5!")-120)<1e-9);
 assert(std::abs(fx::evaluate("-2^2")+4)<1e-9);
 assert(std::abs(fx::evaluate("2^-2")-0.25)<1e-9);
 assert(std::abs(fx::evaluate("log(100)")-2)<1e-9);
 assert(std::abs(fx::evaluate("log(8,2)")-3)<1e-9);
 assert(std::abs(fx::evaluate("max(2,7)+min(4,1)")-8)<1e-9);
 assert(std::abs(fx::evaluate("round(2.6)+frac(3.25)")-3.25)<1e-9);
 assert(std::abs(fx::evaluate("atan2(1,1)")-std::acos(-1.0)/4)<1e-9);
 assert(std::abs(fx::evaluate("x+y",2,3)-5)<1e-12);
 assert(std::abs(fx::compose("x^2+1", "2*x", 3)-37)<1e-12);
 assert(std::abs(fx::inverseValue("x^2",9,0,5)-3)<1e-8); fx::setAngleMode(fx::AngleMode::Degrees); assert(std::abs(fx::evaluate("sin(90)")-1)<1e-9); fx::setAngleMode(fx::AngleMode::Radians); assert(fx::nCr(5,2)==10);
 auto r=fx::linearRegression({1,2,3},{3,5,7}); assert(std::abs(r.slope-2)<1e-9);
 auto f=fx::addFractions(fx::makeFraction(1,3),fx::makeFraction(1,6)); assert(f.numerator==1&&f.denominator==2);
 assert(fx::toBase(255,16)=="FF"&&fx::fromBase("FF",16)==255);
 auto factors=fx::primeFactors(84); assert((factors==std::vector<std::uint64_t>{2,2,3,7}));
 assert(std::abs(fx::logBase(8,2)-3)<1e-9); assert(std::abs(fx::nthRoot(27,3)-3)<1e-9);
 auto p=fx::polarToRect(2,0); assert(std::abs(p.first-2)<1e-9&&std::abs(p.second)<1e-9);
 fx::Matrix m{{4,7},{2,6}}; assert(std::abs(fx::matrixDeterminant(m)-10)<1e-9); auto mi=fx::matrixInverse(m); assert(std::abs(mi[0][0]-.6)<1e-9);
 assert(std::abs(fx::vectorNorm({3,4})-5)<1e-9);
 auto ds=fx::describe({1,2,3,4,5}); assert(ds.count==5&&std::abs(ds.median-3)<1e-9&&std::abs(ds.q1-1.5)<1e-9);
 assert(std::abs(fx::normalCdf(0)-0.5)<1e-12);
 assert(std::abs(fx::inverseNormalCdf(0.5))<1e-7);
 assert(std::abs(fx::binomialPmf(4,2,0.5)-0.375)<1e-12);
 assert(std::abs(fx::poissonPmf(0,2)-std::exp(-2))<1e-12);
 std::vector<double> coeff; fx::polynomialRegression({0,1,2,3},{1,6,17,34},2,&coeff);
 assert(coeff.size()==3&&std::abs(coeff[0]-1)<1e-8&&std::abs(coeff[1]-2)<1e-8&&std::abs(coeff[2]-3)<1e-8);
 fx::Spreadsheet sheet; sheet.set(0,0,2); sheet.set(1,0,4); assert(std::abs(sheet.average(0,0,1)-3)<1e-12);
 fx::CalculatorMemory mem;assert(std::abs(fx::evaluate("e",mem)-std::exp(1.0))<1e-12);mem.store('A',12.5);assert(std::abs(mem.recall('a')-12.5)<1e-12);assert(std::abs(fx::evaluate("A+2",mem)-14.5)<1e-12);mem.setAnswer(4);mem.setAnswer(9);assert(mem.answer()==9&&mem.previousAnswer()==4);mem.addToIndependentMemory(3);assert(mem.independentMemory()==3);assert(fx::formatNatural("sqrt(9)+pi*2^3")=="√(9)+π×2³");
 assert(fx::formatNatural("alpha+beta+gamma+delta+epsilon+zeta+eta+theta+iota+kappa+lambda+mu+nu+xi+omicron+pi+rho+sigma+tau+upsilon+phi+chi+psi+omega")=="α+β+γ+δ+ε+ζ+η+θ+ι+κ+λ+μ+ν+ξ+ο+π+ρ+σ+τ+υ+φ+χ+ψ+ω");
 assert(fx::formatNatural("Alpha+Beta+Gamma+Delta+Epsilon+Zeta+Eta+Theta+Iota+Kappa+Lambda+Mu+Nu+Xi+Omicron+Pi+Rho+Sigma+Tau+Upsilon+Phi+Chi+Psi+Omega")=="Α+Β+Γ+Δ+Ε+Ζ+Η+Θ+Ι+Κ+Λ+Μ+Ν+Ξ+Ο+Π+Ρ+Σ+Τ+Υ+Φ+Χ+Ψ+Ω");
 assert(fx::formatNatural("root(5,32)")=="⁵√(32)");
 assert(fx::formatNatural("speedOfLight+gravitationalConstant+planckConstant+avogadro+boltzmann+elementaryCharge+standardGravity+NA+kB+qe+g0")=="c+G+h+Nₐ+k_B+qₑ+g₀+Nₐ+k_B+qₑ+g₀");
 fx::MatrixEditor me;me.resize(0,2,2);me.set(0,0,0,4);me.set(0,1,1,7);assert(me.get(0,0,0)==4&&me.value(0).size()==2);
 fx::StatisticsEditor se;se.set(0,0,1);se.set(0,1,2);se.set(0,2,3);assert(se.summary(0).count==3);
 auto an=fx::oneWayAnova({{2,3,4},{5,6,7},{8,9,10}});assert(std::abs(an.f-27)<1e-9&&an.pValue<0.01);
 auto tt=fx::oneSampleTTest({2,3,4,5,6},3);assert(std::abs(tt.t-1.41421356237)<1e-8&&tt.pTwoTailed>0.2);
 auto t2=fx::twoSampleTTest({2,3,4,5},{4,5,6,7});assert(std::abs(t2.t+2.19089023002)<1e-8&&t2.df>5);
 auto ci=fx::meanConfidenceInterval({2,3,4,5,6},0.95);assert(ci.lower<4&&ci.upper>4&&ci.critical>2);
 auto cz=fx::complexSqrt(fx::Complex(-4,0)); assert(std::abs(cz.real())<1e-10&&std::abs(std::abs(cz.imag())-2)<1e-10);
 // Point A: Run-Matrix session, matrix/vector operations, exact fractions, complex, Base-N, calculus and conversions.
 using namespace fx::pointa;
 RunMatrixSession session; assert(std::abs(session.execute("2+3")-5)<1e-12); session.store('A',7); assert(std::abs(session.execute("A+Ans")-12)<1e-12); assert(session.history().size()==2&&session.memory().previousAnswer()==5);
 auto mt=transpose({{1,2,3},{4,5,6}}); assert(mt.size()==3&&mt[0].size()==2&&mt[2][1]==6);
 assert(std::abs(trace({{2,1},{0,3}})-5)<1e-12);
 auto rr=rref({{1,2,3},{2,4,6}}); assert(std::abs(rr[0][0]-1)<1e-12&&std::abs(rr[1][1])<1e-12);
 auto sx=solve({{2,1},{1,-1}},{5,1}); assert(std::abs(sx[0]-2)<1e-12&&std::abs(sx[1]-1)<1e-12);
 fx::setAngleMode(fx::AngleMode::Degrees); assert(std::abs(vectorAngle({1,0},{0,1})-90)<1e-10); fx::setAngleMode(fx::AngleMode::Radians);
 auto fd=subtractFractions(fx::makeFraction(3,4),fx::makeFraction(1,4)); assert(fd.numerator==1&&fd.denominator==2);
 auto fq=divideFractions(fx::makeFraction(2,3),fx::makeFraction(4,5)); assert(fq.numerator==5&&fq.denominator==6);
 auto approx=decimalToFraction(0.125); assert(approx.numerator==1&&approx.denominator==8);
 assert(formatFraction(fx::makeFraction(7,3),true)=="2 1/3");
 assert(std::abs(complexDivide({1,1},{1,-1}).real())<1e-12);
 assert(formatComplex({0,-1})=="-i");
 assert(toBaseSigned(-255,16)=="-FF"&&fromBaseSigned("-FF",16)==-255);
 assert(bitNot(0x0F,8)==0xF0&&shiftLeft(3,2,8)==12&&bitXor(6,3)==5);
 assert(std::abs(derivative("x^3",2)-12)<1e-7);
 assert(std::abs(integrate("x^2",0,3)-9)<1e-9);
 auto rt=roots("x^2-4",-3,3); assert(rt.size()==2&&std::abs(rt[0]+2)<1e-7&&std::abs(rt[1]-2)<1e-7);
 assert(std::abs(solve("x^2-9",2.5)-3)<1e-7);
 assert(std::abs(convert(100,Unit::Celsius,Unit::Fahrenheit)-212)<1e-10);
 assert(std::abs(convert(1,Unit::Kilometer,Unit::Meter)-1000)<1e-12);
 assert(engineeringPrefix("k")==1000&&std::abs(physicalConstant("c")-299792458)<1e-6);
 assert(formatNaturalExpression("2*x/3")=="2 × x ÷ 3");
 // Natural V.P.A.M. structured editor: fraction/radical/power nesting and keyboard events.
 fx::NaturalEditor ne; ne.insertFraction(); ne.insert('1'); assert(ne.moveDown()); ne.insert('3'); assert(ne.toExpression()=="(1)/(3)"); assert(std::abs(fx::evaluate(ne.toExpression())-1.0/3.0)<1e-12);
 ne.clear(); ne.insertRadical(); ne.insert('9'); assert(ne.toExpression()=="sqrt(9)"&&fx::evaluate(ne.toExpression())==3);
 ne.clear(); ne.insert('2'); ne.insertPower(); ne.insert('3'); assert(ne.toExpression()=="(2)^(3)"&&fx::evaluate(ne.toExpression())==8); assert(ne.moveUp());
 ne.clear(); ne.insert('2'); ne.insert('^'); ne.insert('3'); assert(ne.toExpression()=="(2)^(3)"&&fx::evaluate(ne.toExpression())==8);
 ne.clear(); for(char c:std::string("sqrt"))ne.insert(c); assert(ne.toExpression()=="sqrt()"); ne.insert('9'); assert(ne.toExpression()=="sqrt(9)"&&fx::evaluate(ne.toExpression())==3);
 ne.clear(); for(char c:std::string("cbrt"))ne.insert(c); ne.insert('2'); ne.insert('7'); assert(ne.toExpression()=="root(3,27)"&&fx::evaluate(ne.toExpression())==3);
 ne.clear(); for(char c:std::string("root5"))ne.insert(c); ne.insert('3'); ne.insert('2'); assert(ne.toExpression()=="root(5,32)"&&fx::evaluate(ne.toExpression())==2);
 ne.clear(); for(char c:std::string("root10"))ne.insert(c); ne.insert('1'); ne.insert('0'); assert(ne.toExpression()=="root(10,10)"&&std::abs(fx::evaluate(ne.toExpression())-std::pow(10.0,0.1))<1e-12);
 ne.clear(); for(char c:std::string("sqrt(3)"))ne.insert(c); assert(ne.toExpression()=="sqrt(3)"&&std::abs(fx::evaluate(ne.toExpression())-std::sqrt(3.0))<1e-12);
 ne.clear(); for(char c:std::string("root3(27)"))ne.insert(c); assert(ne.toExpression()=="root(3,27)"&&fx::evaluate(ne.toExpression())==3);
 ne.clear(); for(char c:std::string("sin(30)"))ne.insert(c); fx::setAngleMode(fx::AngleMode::Degrees); assert(ne.toExpression()=="sin(30)"&&std::abs(fx::evaluate(ne.toExpression())-0.5)<1e-12); fx::setAngleMode(fx::AngleMode::Radians);
 assert(std::abs(fx::evaluate("root3(27)")-3.0)<1e-12);
 assert(std::abs(fx::evaluate("root5(32)")-2.0)<1e-12);
 assert(std::abs(fx::evaluate("integral(x^2,0,3)")-9.0)<1e-9);
 assert(std::abs(fx::evaluate("derivative(x^2,3)")-6.0)<1e-6);
 assert(fx::formatNatural("integral(x^2,0,3)").find("∫")!=std::string::npos);
 assert(fx::formatNatural("sum(x,1,5)").find("Σ")!=std::string::npos);
 assert(std::abs(fx::evaluate("sum(x,1,5)")-15.0)<1e-12);
 assert(std::abs(fx::evaluate("sum(x^2,1,3)")-14.0)<1e-12);
 // Calculus templates are real editable VPAM nodes with separate argument rows.
 ne.clear(); ne.insertIntegral(); for(char c:std::string("x^2")) ne.insert(c); assert(ne.moveRight());
 assert(ne.moveDown()); ne.insert('0'); assert(ne.moveDown()); ne.insert('3');
 assert(ne.toExpression()=="integral((x)^(2),0,3)"); assert(std::abs(fx::evaluate(ne.toExpression())-9.0)<1e-9);
 auto integralOps=ne.layout(0,30,300); bool integralGlyph=false; for(const auto& op:integralOps) if(op.kind==fx::NaturalDrawOp::Kind::Text&&op.text=="∫") integralGlyph=true; assert(integralGlyph);
 ne.clear(); ne.insertDerivative(); for(char c:std::string("x^2")) ne.insert(c); assert(ne.moveRight()); assert(ne.moveDown()); ne.insert('3');
 assert(ne.toExpression()=="derivative((x)^(2),3)"); assert(std::abs(fx::evaluate(ne.toExpression())-6.0)<1e-6);
 ne.clear(); ne.insertSummation(); ne.insert('x'); assert(ne.moveDown()); ne.insert('1'); assert(ne.moveDown()); ne.insert('5');
 assert(ne.toExpression()=="sum(x,1,5)"); assert(fx::evaluate(ne.toExpression())==15.0);

 ne.clear(); for(char c:std::string("alpha"))ne.insert(c); auto greekOps=ne.layout(4,20,300); bool greekVisible=false; for(const auto& op:greekOps)if(op.kind==fx::NaturalDrawOp::Kind::Text&&op.text=="α")greekVisible=true; assert(greekVisible&&ne.toExpression()=="alpha");
 ne.clear(); ne.insert('1'); ne.insert('2'); ne.insertPower(); ne.insert('2'); assert(ne.toExpression()=="(12)^(2)"&&fx::evaluate(ne.toExpression())==144);
 ne.clear(); ne.insert('1'); ne.insert('2'); ne.insertFraction(); assert(ne.toExpression()=="(12)/()"); assert(ne.moveDown()); ne.insert('3'); assert(ne.toExpression()=="(12)/(3)"&&std::abs(fx::evaluate(ne.toExpression())-4)<1e-12);
 ne.clear(); ne.insertFraction(); ne.insert('1'); assert(ne.moveDown()); ne.insert('3'); assert(ne.moveUp()); ne.insert('+'); ne.insert('1'); assert(ne.moveDown()); assert(ne.toExpression()=="(1+1)/(3)"); assert(std::abs(fx::evaluate(ne.toExpression())-2.0/3.0)<1e-12);
 ne.clear(); ne.handle({fx::NaturalKey::Character,'4'}); ne.handle({fx::NaturalKey::Character,'2'}); assert(ne.toExpression()=="42"); ne.handle({fx::NaturalKey::Backspace,0}); assert(ne.toExpression()=="4");
 ne.clear(); ne.insertGroup(); ne.insert('5'); ne.insert('+'); ne.insert('1'); assert(ne.toExpression()=="(5+1)"&&fx::evaluate(ne.toExpression())==6);
 auto naturalOps=ne.layout(4,20,300); assert(!naturalOps.empty()); bool cursorDrawn=false; for(const auto& op:naturalOps) cursorDrawn|=op.kind==fx::NaturalDrawOp::Kind::Cursor; assert(cursorDrawn);
 ne.clear(); ne.insert('x'); ne.insertSubscript(); ne.insert('1'); bool subscriptRejected=false; try{(void)ne.toExpression();}catch(const std::runtime_error&){subscriptRejected=true;} assert(subscriptRejected);
 bool fractionOverflow=false; try { (void)fx::makeFraction(INT64_MIN,-1); } catch(const std::overflow_error&) { fractionOverflow=true; } assert(fractionOverflow);
 // PicoCalc STM32 FIFO decoding: state in MSB, physical key code in LSB.
 assert(fx::decodePicoCalcKey(0).action==fx::InputAction::None);
 assert(fx::decodePicoCalcKey(0x0301).action==fx::InputAction::None); // release ignored
 assert(fx::decodePicoCalcKey(0x0131).action==fx::InputAction::Character && fx::decodePicoCalcKey(0x0131).character=='1');
 assert(fx::decodePicoCalcKey(0x015E).action==fx::InputAction::Character && fx::decodePicoCalcKey(0x015E).character=='^');
 assert(fx::decodePicoCalcKey(0x02B5).action==fx::InputAction::Up); // held arrow repeat
 assert(fx::decodePicoCalcKey(0x01B4).action==fx::InputAction::Left);
 assert(fx::decodePicoCalcKey(0x0108).action==fx::InputAction::Backspace);
 assert(fx::decodePicoCalcKey(0x01D4).action==fx::InputAction::Delete);
 assert(fx::decodePicoCalcKey(0x010A).action==fx::InputAction::Evaluate);
 assert(fx::decodePicoCalcKey(0x01B1).action==fx::InputAction::Escape);
 assert(fx::decodePicoCalcKey(0x0181).action==fx::InputAction::Fraction);
 assert(fx::decodePicoCalcKey(0x0182).action==fx::InputAction::Radical);
 assert(fx::decodePicoCalcKey(0x0183).action==fx::InputAction::Power);
 assert(fx::decodePicoCalcKey(0x0184).action==fx::InputAction::Subscript);
 assert(fx::decodePicoCalcKey(0x0185).action==fx::InputAction::Group);
 assert(fx::decodePicoCalcKey(0x0186).action==fx::InputAction::Clear);
 // End-to-end host flow: launcher -> Run-Matrix -> edit -> evaluate -> return.
 fx::RunMatrixController app;
 assert(app.screen()==fx::AppScreen::Home);
 app.handle(fx::decodePicoCalcKey(0x0181));
 assert(app.screen()==fx::AppScreen::RunMatrix);
 app.handle(fx::decodePicoCalcKey(0x0131));
 app.handle(fx::decodePicoCalcKey(0x012B));
 app.handle(fx::decodePicoCalcKey(0x0132));
 app.handle(fx::decodePicoCalcKey(0x010A));
 assert(app.expression()=="1+2" && app.result()=="3" && app.message().empty());
 assert(app.session().history().size()==1);
 app.handle(fx::decodePicoCalcKey(0x01B1));
 assert(app.screen()==fx::AppScreen::Home);
 // Natural fraction workflow: F1, numerator, DOWN, denominator, EXE.
 app.handle(fx::decodePicoCalcKey(0x0181));
 app.handle(fx::decodePicoCalcKey(0x0181));
 app.handle(fx::decodePicoCalcKey(0x0131));
 app.handle(fx::decodePicoCalcKey(0x02B6));
 app.handle(fx::decodePicoCalcKey(0x0133));
 app.handle(fx::decodePicoCalcKey(0x010A));
 assert(app.expression()=="(1)/(3)" && app.result()=="0.3333333333");
 // Radical and power templates are editable through their physical function-key events.
 app.handle(fx::decodePicoCalcKey(0x0186));
 app.handle(fx::decodePicoCalcKey(0x0182));
 app.handle(fx::decodePicoCalcKey(0x0139));
 app.handle(fx::decodePicoCalcKey(0x010A));
 assert(app.expression()=="sqrt(9)" && app.result()=="3");
 app.handle(fx::decodePicoCalcKey(0x0186));
 app.handle(fx::decodePicoCalcKey(0x0132));
 app.handle(fx::decodePicoCalcKey(0x0183));
 app.handle(fx::decodePicoCalcKey(0x0133));
 app.handle(fx::decodePicoCalcKey(0x010A));
 assert(app.expression()=="(2)^(3)" && app.result()=="8");
 app.handle(fx::decodePicoCalcKey(0x0186)); app.handle(fx::decodePicoCalcKey(0x0134)); app.handle(fx::decodePicoCalcKey(0x015E)); app.handle(fx::decodePicoCalcKey(0x0132)); app.handle(fx::decodePicoCalcKey(0x010A)); assert(app.expression()=="(4)^(2)" && app.result()=="16");
 // F5 creates a nested group; F4 creates a visible but not yet parser-evaluable subscript.
 app.handle(fx::decodePicoCalcKey(0x0186));
 app.handle(fx::decodePicoCalcKey(0x0185));
 app.handle(fx::decodePicoCalcKey(0x0132));
 app.handle(fx::decodePicoCalcKey(0x012B));
 app.handle(fx::decodePicoCalcKey(0x0133));
 app.handle(fx::decodePicoCalcKey(0x010A));
 assert(app.expression()=="(2+3)" && app.result()=="5");
 fx::pointa::RunMatrixSession limitedHistory; for(int i=0;i<35;++i)limitedHistory.execute("1+1"); assert(limitedHistory.history().size()==30 && limitedHistory.history().front().expression=="1+1");
 app.handle(fx::decodePicoCalcKey(0x0186));
 app.handle(fx::decodePicoCalcKey(0x0178));
 app.handle(fx::decodePicoCalcKey(0x0184));
 app.handle(fx::decodePicoCalcKey(0x0131));
 app.handle(fx::decodePicoCalcKey(0x010A));
 assert(!app.message().empty() && app.session().history().size()==6);
}
