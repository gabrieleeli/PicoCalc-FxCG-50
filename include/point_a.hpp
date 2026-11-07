#pragma once
#include "math_engine.hpp"
#include "memory.hpp"
#include <string>
#include <vector>
#include <cstdint>
namespace fx::pointa {
struct HistoryEntry { std::string expression; double result{}; };
class RunMatrixSession {
public:
 double execute(const std::string& expression);
 void store(char variable,double value);
 double recall(char variable) const;
 const CalculatorMemory& memory() const noexcept { return memory_; }
 const std::vector<HistoryEntry>& history() const noexcept { return history_; }
 void clearHistory() noexcept { history_.clear(); }
 void clearMemory() noexcept { memory_.clearAll(); }
private:
 CalculatorMemory memory_;
 std::vector<HistoryEntry> history_;
};
Matrix subtract(const Matrix&,const Matrix&); Matrix scale(const Matrix&,double); Matrix transpose(const Matrix&); double trace(const Matrix&); Matrix rref(Matrix,double eps=1e-12); std::vector<double> solve(const Matrix&,const std::vector<double>&,double eps=1e-12);
std::vector<double> vectorSubtract(const std::vector<double>&,const std::vector<double>&); std::vector<double> vectorScale(const std::vector<double>&,double); double vectorAngle(const std::vector<double>&,const std::vector<double>&);
Fraction subtractFractions(Fraction,Fraction); Fraction divideFractions(Fraction,Fraction); Fraction decimalToFraction(double,std::int64_t maxDenominator=1000000,double tolerance=1e-12); std::string formatFraction(Fraction,bool mixed=false);
Complex complexAdd(Complex,Complex); Complex complexSubtract(Complex,Complex); Complex complexMultiply(Complex,Complex); Complex complexDivide(Complex,Complex); Complex complexExp(Complex); Complex complexLog(Complex); Complex complexSin(Complex); Complex complexCos(Complex); Complex complexTan(Complex); double complexArgument(Complex); std::string formatComplex(Complex,unsigned significantDigits=10);
std::string toBaseSigned(std::int64_t,unsigned); std::int64_t fromBaseSigned(const std::string&,unsigned); std::uint64_t bitAnd(std::uint64_t,std::uint64_t); std::uint64_t bitOr(std::uint64_t,std::uint64_t); std::uint64_t bitXor(std::uint64_t,std::uint64_t); std::uint64_t bitNot(std::uint64_t,unsigned width=32); std::uint64_t shiftLeft(std::uint64_t,unsigned,unsigned width=32); std::uint64_t shiftRight(std::uint64_t,unsigned,unsigned width=32);
double derivative(const std::string&,double,double h=1e-5); double integrate(const std::string&,double,double,unsigned intervals=400); std::vector<double> roots(const std::string&,double,double,unsigned scanIntervals=512,double tolerance=1e-10); double solve(const std::string&,double initialGuess,unsigned iterations=100,double tolerance=1e-10);
enum class Unit { Meter,Kilometer,Centimeter,Millimeter,Inch,Foot,Mile,SquareMeter,SquareCentimeter,Hectare,Acre,CubicMeter,Liter,Milliliter,GallonUS,Second,Minute,Hour,Day,MeterPerSecond,KilometerPerHour,MilePerHour,Kilogram,Gram,Milligram,Pound,Pascal,Kilopascal,Bar,Atmosphere,Joule,Kilojoule,Calorie,Kilocalorie,Watt,Kilowatt,Horsepower,Kelvin,Celsius,Fahrenheit };
double convert(double,Unit,Unit); double engineeringPrefix(const std::string&); double physicalConstant(const std::string&); std::string formatEngineering(double,unsigned significantDigits=6); std::string formatNaturalExpression(const std::string&);
}
