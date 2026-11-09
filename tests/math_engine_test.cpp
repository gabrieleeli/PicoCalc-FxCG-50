// Math engine regression suite (ported from the v1.5.4 math_smoke test).
#include "math_engine.hpp"
#include "point_a.hpp"
#include "expression.hpp"
#include "graph.hpp"
#include "memory.hpp"
#include "editors.hpp"
#include "test_util.hpp"
#include "board_config.hpp"
#include "limits.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#undef assert
#define assert(x) CHECK(x)
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
 assert(std::abs(fx::evaluate("log(2,8)")-3)<1e-9); // fx-CG50 convention: log(base,value)
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

 bool fractionOverflow=false; try { (void)fx::makeFraction(INT64_MIN,-1); } catch(const std::overflow_error&) { fractionOverflow=true; } assert(fractionOverflow);
 // Legacy session history: 30 entries without PSRAM (fx-CG50), memory-bound up to 10000 with it.
 {const std::size_t cap=fx::limits::historyCapacity();fx::pointa::RunMatrixSession limitedHistory;const std::size_t runs=std::min<std::size_t>(cap+5,20000);for(std::size_t i=0;i<runs;++i)limitedHistory.execute("1+1");assert(limitedHistory.history().size()==std::min(cap,runs) && limitedHistory.history().front().expression=="1+1");if(!board::config::kHasPsram)assert(cap==30);}
 return testutil::finish("math_engine_test");
}
