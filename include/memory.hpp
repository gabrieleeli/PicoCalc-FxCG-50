#pragma once
#include <array>
#include <string>
#include <stdexcept>
namespace fx {
class CalculatorMemory {
public:
 static constexpr std::size_t VariableCount=26;
 void store(char name,double value);
 double recall(char name) const;
 void clear(char name);
 void clearAll();
 void setAnswer(double value);
 double answer() const { return answer_; }
 double previousAnswer() const { return previousAnswer_; }
 void setIndependentMemory(double value) { independent_=value; }
 double independentMemory() const { return independent_; }
 void addToIndependentMemory(double value) { independent_+=value; }
 void subtractFromIndependentMemory(double value) { independent_-=value; }
private:
 std::array<double,VariableCount> vars_{};
 std::array<bool,VariableCount> used_{};
 double answer_{}, previousAnswer_{}, independent_{};
};
// Compact natural-display formatter: Unicode operators, roots, pi, powers and grouped fractions.
std::string formatNatural(const std::string& expression);
}
