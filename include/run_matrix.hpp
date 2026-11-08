#pragma once
// Run-Matrix calculation core (no UI): evaluation of one input line, history
// with in-place editing and recalculation, and the result views (exact form,
// decimal, mixed/improper fraction, repeating decimal, Base-N, DMS).
#include "expression_ast.hpp"
#include "workspace.hpp"
#include <string>

namespace fx {

// Result of one RUN-MATRIX calculation.
struct RunOutcome {
    Complex value{};
    char storedTo{};            // first variable letter for "expr->A" / "expr->A~F", 0 otherwise
    char storedLast{};          // last letter of an A~F range (== storedTo for one variable)
    std::string pairText;       // "r=5  θ=53.13" for Pol(), "x=..  y=.." for Rec()
    bool exact{};               // the value has an exact form (fraction, surd, multiple of pi)
};

// Evaluates a linear expression exactly like the RUN-MATRIX mode into `entry`:
// store arrow (U+2192 or "->", one variable or a range "A~F"), Pol/Rec pairs,
// exact evaluation next to the floating-point one, Ans/PreAns update.
// Throws Syntax/Math errors (the entry is then left untouched).
RunOutcome evaluateEntry(Workspace& ws, const std::string& expression, HistoryEntry& entry);
// evaluateEntry() + append to the workspace history.
RunOutcome runCalculation(Workspace& ws, const std::string& expression);

// fx-CG50 history editing: replaces the expression of history[index] and
// recalculates it and every later entry in order (Ans chains and stores are
// replayed). If the edited entry itself fails, history and memory are
// restored and the error is thrown; later entries that fail keep their error
// text. Returns the number of later entries that failed.
std::size_t editHistoryEntry(Workspace& ws, std::size_t index, const std::string& expression);
// Recalculates history[from..] with Ans/PreAns taken from the entries before it.
std::size_t recalculateHistory(Workspace& ws, std::size_t from);

// Result views stored in HistoryEntry::view.
enum ResultView : std::uint8_t {
    ViewAuto = 0,       // exact form when the fx-CG50 would show it, otherwise decimal
    ViewDecimal = 1,
    ViewHex = 2, ViewBin = 3, ViewOct = 4,
    ViewDms = 5,
    ViewExact = 6,      // exact form requested with S<->D (also for decimal input)
    ViewPeriodic = 7,   // repeating decimal of a rational: 0.1(6)
};

// True when the entry currently shows its exact form (fraction, surd, pi).
bool showsExact(const HistoryEntry& entry, const Settings& settings);
// True when an exact fraction is shown in mixed form (2 1/3).
bool showsMixed(const HistoryEntry& entry, const Settings& settings);
// True when the decimal shown is the result of a numerical method (shown with ≈).
bool isApproximation(const HistoryEntry& entry, const Settings& settings);

// fx-CG50 S<->D: exact form <-> decimal. Returns false when the value has no
// other form (irrational or numerical result, integer, error).
bool toggleExactDecimal(HistoryEntry& entry, const Settings& settings);
// a b/c <-> d/c on an exact fraction result. False when no fraction is shown.
bool toggleMixedFraction(HistoryEntry& entry, const Settings& settings);
// Repeating-decimal view of an exact rational (toggles back to the automatic view).
bool togglePeriodic(HistoryEntry& entry);

// Linear text of a result in its current view ("1/3", "2 1/3", "2√3/3",
// "π/2", "0.3333333333", "0.1(6)", "0x1F", "30°15'0\"").
std::string resultText(const HistoryEntry& entry, const Settings& settings);
// 2D tree used to draw the result (stacked fraction, radicals, ×10^n, overline).
ast::NodePtr resultTree(const HistoryEntry& entry, const Settings& settings);
// Expression that reproduces the stored value exactly when possible (exact
// form, else 15 significant digits); used to reuse a result in a new input.
std::string resultExpression(const HistoryEntry& entry);
} // namespace fx
