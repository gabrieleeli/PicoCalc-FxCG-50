#pragma once
#include "math_engine.hpp"
#include "limits.hpp"
#include <array>
#include <vector>
namespace fx {
class MatrixEditor {
public:
 // Mat A..Z. Per-board dimension caps (40 on Pico 2 W, 999 with PSRAM); the
 // storage is allocated in PSRAM when available and resize() fails cleanly
 // (std::bad_alloc, previous matrix kept) when memory is short.
 static constexpr std::size_t Slots=limits::kProfile.matrixSlots;
 static constexpr std::size_t MaxRows=limits::kProfile.matrixMaxDim, MaxCols=limits::kProfile.matrixMaxDim;
 void resize(std::size_t slot,std::size_t rows,std::size_t cols);
 void set(std::size_t slot,std::size_t row,std::size_t col,double value);
 double get(std::size_t slot,std::size_t row,std::size_t col) const;
 const Matrix& value(std::size_t slot) const;
 void clear(std::size_t slot);
private: std::array<Matrix,Slots> matrices_;
};
class StatisticsEditor {
public:
 // List 1..26; maximum length = fx::limits::listMaxLength() (999 on Pico 2 W,
 // memory-bound up to 1,000,000 elements with PSRAM).
 static constexpr std::size_t Lists=limits::kProfile.statLists;
 void set(std::size_t list,std::size_t index,double value);
 void erase(std::size_t list,std::size_t index);
 const std::vector<double>& list(std::size_t list) const;
 DescriptiveStats summary(std::size_t list) const;
 Regression linearFit(std::size_t xList,std::size_t yList) const;
private: std::array<std::vector<double>,Lists> lists_;
};
struct AnovaResult { double ssBetween{},ssWithin{},dfBetween{},dfWithin{},msBetween{},msWithin{},f{},pValue{}; std::size_t groups{},observations{}; };
AnovaResult oneWayAnova(const std::vector<std::vector<double>>& groups);
struct TTestResult { double t{},df{},pTwoTailed{},meanDifference{},standardError{}; };
TTestResult oneSampleTTest(const std::vector<double>& sample,double hypothesizedMean);
TTestResult pairedTTest(const std::vector<double>& a,const std::vector<double>& b);
TTestResult twoSampleTTest(const std::vector<double>& a,const std::vector<double>& b,bool equalVariances=false);
struct ConfidenceInterval { double lower{},upper{},critical{}; };
ConfidenceInterval meanConfidenceInterval(const std::vector<double>& sample,double confidence=0.95);
}
