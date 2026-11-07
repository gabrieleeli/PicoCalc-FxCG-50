#pragma once
#include "expression_ast.hpp"
#include "workspace.hpp"
#include <string>

namespace fx {

// Result of one RUN-MATRIX calculation.
struct RunOutcome {
    Complex value{};
    char storedTo{};            // variable letter for "expr->A", 0 otherwise
    std::string pairText;       // "r=5  θ=53.13" for Pol(), "x=..  y=.." for Rec()
};

// Evaluates a linear expression exactly like the RUN-MATRIX mode: handles the
// store arrow (U+2192 or "->"), Pol(x,y)/Rec(r,θ) pairs, updates Ans/PreAns and
// appends the entry to the workspace history. Throws on Syntax/Math errors.
RunOutcome runCalculation(Workspace& ws, const std::string& expression);

// Result views stored in HistoryEntry::view.
enum ResultView : std::uint8_t { ViewAuto = 0, ViewDecimal = 1, ViewHex = 2, ViewBin = 3, ViewOct = 4, ViewDms = 5 };

// True when the auto view shows the value as an exact fraction (rational
// value, no decimal point typed in the expression, fraction display enabled).
bool showsAsFraction(const HistoryEntry& entry, const Settings& settings, long long& num, long long& den);
// Linear text of a result in its current view ("1/3", "0.3333333333", "0x1F", "30°15'0\"").
std::string resultText(const HistoryEntry& entry, const Settings& settings);
// 2D tree used to draw the result (stacked fraction, ×10^n exponent).
ast::NodePtr resultTree(const HistoryEntry& entry, const Settings& settings);
} // namespace fx
