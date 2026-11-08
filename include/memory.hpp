#pragma once
#include "exact.hpp"
#include <array>
#include <string>
#include <stdexcept>
namespace fx {
// Exact companion of a stored value (fraction, surd or multiple of pi) so that
// results keep their exact form when reused through Ans or A-Z. `decimal`
// records that the value came from decimal input (shown as a decimal).
struct ExactSlot { exact::Value value; bool valid{false}; bool decimal{false}; };

class CalculatorMemory {
public:
 static constexpr std::size_t VariableCount=26;
 // Stores an approximate value (the exact companion is dropped).
 void store(char name,double value);
 // Stores a value together with its exact form.
 void storeExact(char name,double value,const exact::Value& exactValue,bool decimal);
 // Exact form of a variable: never-assigned variables are exactly 0.
 bool exactRecall(char name,exact::Value& out,bool& decimal) const;
 const ExactSlot& exactSlot(char name) const;
 double recall(char name) const;
 void clear(char name);
 void clearAll();
 void setAnswer(double value, double imag = 0.0);
 // Like setAnswer() for a real value that also has an exact form.
 void setAnswerExact(double value,const exact::Value& exactValue,bool decimal);
 bool exactAnswer(exact::Value& out,bool& decimal) const;
 bool exactPreviousAnswer(exact::Value& out,bool& decimal) const;
 const ExactSlot& exactAnswerSlot() const { return exactAns_; }
 const ExactSlot& exactPreviousAnswerSlot() const { return exactPreAns_; }
 // Restores Ans/PreAns with their exact forms (history recalculation, storage).
 void restoreAnswers(double ans,double ansImag,const ExactSlot& ansExact,double pre,double preImag,const ExactSlot& preExact);
 bool used(char name) const;
 double answer() const { return answer_; }
 double answerImag() const { return answerImag_; }
 double previousAnswer() const { return previousAnswer_; }
 double previousAnswerImag() const { return previousAnswerImag_; }
 void setIndependentMemory(double value) { independent_=value; }
 double independentMemory() const { return independent_; }
 void addToIndependentMemory(double value) { independent_+=value; }
 void subtractFromIndependentMemory(double value) { independent_-=value; }
private:
 std::array<double,VariableCount> vars_{};
 std::array<bool,VariableCount> used_{};
 std::array<ExactSlot,VariableCount> exact_{};
 ExactSlot exactAns_{}, exactPreAns_{};
 double answer_{}, previousAnswer_{}, independent_{};
 double answerImag_{}, previousAnswerImag_{};
};
// Compact natural-display formatter: Unicode operators, roots, pi, powers and grouped fractions.
std::string formatNatural(const std::string& expression);
}
