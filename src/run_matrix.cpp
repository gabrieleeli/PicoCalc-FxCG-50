#include "run_matrix.hpp"
#include "math_engine.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <numeric>

namespace fx {
namespace {
const char* const kArrow = "\xE2\x86\x92";   // U+2192

std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}
ast::NodePtr tok(const std::string& t) { return ast::make(ast::Kind::Token, t); }
ast::NodePtr rowOf(const std::string& text) {
    auto r = ast::make(ast::Kind::Row);
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        const std::size_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        r->children.push_back(tok(text.substr(i, len)));
        i += len;
    }
    return r;
}
void appendText(ast::Node& row, const std::string& text) {
    auto r = rowOf(text);
    for (auto& c : r->children) row.children.push_back(c);
}
ast::NodePtr fraction(ast::NodePtr top, ast::NodePtr bottom) {
    auto f = ast::make(ast::Kind::Fraction);
    f->children = {std::move(top), std::move(bottom)};
    return f;
}
ast::NodePtr radical(std::int64_t radicand) {
    auto r = ast::make(ast::Kind::Radical, "2");
    r->children = {rowOf(std::to_string(radicand))};
    return r;
}
// Parses the store target after the arrow: "A" or "A~F".
void storeTarget(const std::string& target, RunOutcome& out) {
    auto letter = [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); };
    if (target.size() == 1 && std::isalpha(static_cast<unsigned char>(target[0]))) { out.storedTo = out.storedLast = letter(target[0]); return; }
    if (target.size() == 3 && target[1] == '~' && std::isalpha(static_cast<unsigned char>(target[0])) && std::isalpha(static_cast<unsigned char>(target[2]))) {
        out.storedTo = letter(target[0]); out.storedLast = letter(target[2]);
        if (out.storedLast >= out.storedTo) return;
    }
    throw SyntaxError("Syntax ERROR: dopo -> serve una variabile A-Z o un intervallo A~F");
}
bool rationalOf(const HistoryEntry& e, exact::Rational& r) { return e.exact.valid && e.exact.value.rational(r); }
bool isIntegerExact(const HistoryEntry& e) { exact::Rational r; return rationalOf(e, r) && r.isInteger(); }
bool plainResult(const HistoryEntry& e) { return e.error.empty() && e.value.imag() == 0; }
} // namespace

RunOutcome evaluateEntry(Workspace& ws, const std::string& raw, HistoryEntry& entry) {
    std::string expr = trim(raw);
    RunOutcome out;
    std::size_t arrow = expr.rfind(kArrow);
    std::size_t arrowLen = 3;
    if (arrow == std::string::npos) { arrow = expr.rfind("->"); arrowLen = 2; }
    if (arrow != std::string::npos) {
        std::string target = trim(expr.substr(arrow + arrowLen));
        target.erase(std::remove(target.begin(), target.end(), ' '), target.end());
        storeTarget(target, out);
        expr = trim(expr.substr(0, arrow));
    }
    ws.apply();
    auto tree = parseExpression(expr);
    EvalInfo info;
    EvalContext ctx = ws.evalContext();
    ctx.info = &info;
    out.value = evaluateValue(*tree, ctx);
    // Pol / Rec at the top level produce a pair, like the fx-CG50.
    if (tree->kind == ast::Kind::Call && (tree->text == "pol" || tree->text == "rec") && tree->children.size() == 2) {
        const double a = evaluateReal(*tree->children[0], ctx), b = evaluateReal(*tree->children[1], ctx);
        const DisplayFormat f = ws.settings.display;
        if (tree->text == "pol") {
            const auto p = rectToPolar(a, b);
            out.pairText = "r=" + formatReal(p.first, f) + "  \xCE\xB8=" + formatReal(p.second, f);
        } else {
            const auto p = polarToRect(a, b);
            out.pairText = "x=" + formatReal(p.first, f) + "  y=" + formatReal(p.second, f);
        }
    }
    // Exact form, evaluated on the same AST. When it exists it is the value:
    // its floating-point image replaces the rounded one (1/3*3 is exactly 1).
    ExactSlot slot;
    if (out.value.imag() == 0 && out.pairText.empty()) {
        exact::Value v;
        bool decimal = false;
        if (evaluateExact(*tree, ctx, v, &decimal) && std::isfinite(v.toDouble())) {
            slot = ExactSlot{v, true, decimal};
            out.value = {v.toDouble(), 0.0};
            out.exact = true;
        }
    }
    if (out.storedTo) {
        if (out.value.imag() != 0) throw MathError("Argument ERROR: le variabili A-Z sono reali");
        for (char v = out.storedTo; v <= out.storedLast; ++v) {
            if (slot.valid) ws.memory.storeExact(v, out.value.real(), slot.value, slot.decimal);
            else ws.memory.store(v, out.value.real());
        }
    }
    if (slot.valid) ws.memory.setAnswerExact(out.value.real(), slot.value, slot.decimal);
    else ws.memory.setAnswer(out.value.real(), out.value.imag());
    entry = HistoryEntry{};
    entry.expression = raw;
    entry.value = out.value;
    entry.exact = slot;
    entry.numerical = info.numerical;
    if (!out.pairText.empty()) entry.error = std::string("=") + out.pairText;   // '=' prefix: informative text, not an error
    return out;
}

RunOutcome runCalculation(Workspace& ws, const std::string& raw) {
    HistoryEntry h;
    const RunOutcome out = evaluateEntry(ws, raw, h);
    ws.addHistory(std::move(h));
    return out;
}

std::size_t recalculateHistory(Workspace& ws, std::size_t from) {
    auto& hist = ws.history;
    if (from >= hist.size()) return 0;
    // Ans before the first recalculated entry is the result of the entry before
    // it (0 when there is none, as on the fx-CG50); PreAns the one before that.
    auto answerOf = [&](std::size_t count, double& re, double& im, ExactSlot& slot) {
        re = im = 0; slot = ExactSlot{exact::Value{}, true, false};
        for (std::size_t k = count; k-- > 0;) {
            const HistoryEntry& h = hist[k];
            if (!h.error.empty() && h.error[0] != '=') continue;     // failed entries do not change Ans
            re = h.value.real(); im = h.value.imag(); slot = h.exact;
            return k;
        }
        return static_cast<std::size_t>(-1);
    };
    double ar = 0, ai = 0, pr = 0, pi = 0;
    ExactSlot ae, pe;
    const std::size_t last = answerOf(from, ar, ai, ae);
    if (last != static_cast<std::size_t>(-1)) (void)answerOf(last, pr, pi, pe);
    else pe = ExactSlot{exact::Value{}, true, false};
    ws.memory.restoreAnswers(ar, ai, ae, pr, pi, pe);
    std::size_t failures = 0;
    for (std::size_t i = from; i < hist.size(); ++i) {
        HistoryEntry fresh;
        try {
            (void)evaluateEntry(ws, hist[i].expression, fresh);
            fresh.view = static_cast<std::uint8_t>(hist[i].view == ViewDecimal || hist[i].view == ViewExact ? hist[i].view : static_cast<std::uint8_t>(ViewAuto));
            fresh.flipMixed = hist[i].flipMixed;
        } catch (const std::exception& e) {
            fresh.expression = hist[i].expression;
            fresh.value = {std::nan(""), 0.0};
            fresh.error = e.what();
            if (fresh.error.empty() || fresh.error[0] == '=') fresh.error = "ERROR";
            ++failures;
        }
        hist[i] = std::move(fresh);
    }
    return failures;
}

std::size_t editHistoryEntry(Workspace& ws, std::size_t index, const std::string& expression) {
    if (index >= ws.history.size()) throw std::out_of_range("Voce della cronologia inesistente");
    // Copies kept on the heap: CalculatorMemory with its exact slots is ~1.7 KB,
    // too large for a frame on the 32 KB RP2350 stack.
    const std::vector<HistoryEntry> savedHistory = ws.history;
    const auto savedMemory = std::make_unique<CalculatorMemory>(ws.memory);
    ws.history[index].expression = expression;
    const std::size_t failures = recalculateHistory(ws, index);
    const HistoryEntry& edited = ws.history[index];
    if (!edited.error.empty() && edited.error[0] != '=') {
        const std::string what = edited.error;
        ws.history = savedHistory;
        ws.memory = *savedMemory;
        // Re-raise with the original class so the error box shows the right title.
        if (what.rfind("Syntax", 0) == 0) throw SyntaxError(what);
        throw MathError(what);
    }
    return failures;
}

// ---------------------------------------------------------------- views
bool showsExact(const HistoryEntry& e, const Settings& s) {
    if (!plainResult(e) || !e.exact.valid || isIntegerExact(e)) return false;
    if (e.view == ViewExact) return exact::fitsRelaxed(e.exact.value);
    if (e.view != ViewAuto || !s.fractionResults || e.exact.decimal) return false;
    return exact::fitsNatural(e.exact.value, s.mixedFractions != e.flipMixed);
}
bool showsMixed(const HistoryEntry& e, const Settings& s) {
    exact::Rational r;
    return showsExact(e, s) && rationalOf(e, r) && (s.mixedFractions != e.flipMixed) && std::llabs(r.num) > r.den;
}
bool isApproximation(const HistoryEntry& e, const Settings& s) {
    return plainResult(e) && e.numerical && !showsExact(e, s) && e.view <= ViewDecimal;
}

bool toggleExactDecimal(HistoryEntry& e, const Settings& s) {
    if (!e.error.empty()) return false;
    if (showsExact(e, s) || e.view == ViewPeriodic) { e.view = ViewDecimal; return true; }
    if (!e.exact.valid || isIntegerExact(e) || !exact::fitsRelaxed(e.exact.value)) {
        if (e.view != ViewAuto && e.view != ViewDecimal) { e.view = ViewAuto; return true; }   // back from Base-N / DMS
        return false;
    }
    HistoryEntry probe = e;
    probe.view = ViewAuto;
    e.view = showsExact(probe, s) ? ViewAuto : ViewExact;
    return true;
}
bool toggleMixedFraction(HistoryEntry& e, const Settings& s) {
    exact::Rational r;
    if (!showsExact(e, s) || !rationalOf(e, r) || std::llabs(r.num) <= r.den) return false;
    e.flipMixed = !e.flipMixed;
    // The other format may need more characters than the automatic view allows.
    if (!showsExact(e, s)) e.view = ViewExact;
    return true;
}
bool togglePeriodic(HistoryEntry& e) {
    exact::Rational r;
    if (!e.error.empty() || !rationalOf(e, r) || r.isInteger()) return false;
    e.view = e.view == ViewPeriodic ? ViewAuto : ViewPeriodic;
    return true;
}

namespace {
std::string periodicText(const exact::Rational& r) {
    const exact::Expansion x = exact::decimalExpansion(r);
    std::string s = (x.negative ? "-" : "") + x.integer;
    if (!x.fixed.empty() || !x.period.empty()) s += ".";
    s += x.fixed;
    if (!x.period.empty()) s += "(" + x.period + ")";
    if (x.truncated) s += "\xE2\x80\xA6";
    return s;
}
std::string baseText(const HistoryEntry& e) {
    const double v = e.value.real();
    if (v != std::floor(v) || std::abs(v) > 9.0e15) return "Math ERROR: Base-N richiede un intero";
    const unsigned base = e.view == ViewHex ? 16 : e.view == ViewBin ? 2 : 8;
    const long long iv = static_cast<long long>(v);
    const std::string digits = toBase(static_cast<std::uint64_t>(iv < 0 ? -iv : iv), base);
    const char* prefix = e.view == ViewHex ? "0x" : e.view == ViewBin ? "0b" : "0o";
    return (iv < 0 ? "-" : "") + std::string(prefix) + digits;
}
std::string dmsText(const HistoryEntry& e, const Settings& s) {
    const double v = e.value.real();
    const auto sx = decimalToSexagesimal(v);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d\xC2\xB0%d'%s\"", sx.degrees, sx.minutes, formatReal(std::round(sx.seconds * 1e6) / 1e6, s.display).c_str());
    return (v < 0 && sx.degrees == 0 ? "-" : "") + std::string(buf);
}
// 2D tree of an exact value (fractions stacked, radicals, pi).
ast::NodePtr exactTree(const exact::Value& v, bool mixed) {
    auto root = ast::make(ast::Kind::Row);
    if (v.isZero()) { appendText(*root, "0"); return root; }
    if (v.form() == exact::Value::Form::Pi) {
        const exact::Rational& c = v.term(0).coef;
        const std::int64_t a = std::llabs(c.num);
        if (c.num < 0) appendText(*root, "-");
        auto top = rowOf(a == 1 ? std::string() : std::to_string(a));
        top->children.push_back(tok("pi"));
        if (c.isInteger()) { for (auto& t : top->children) root->children.push_back(t); return root; }
        root->children.push_back(fraction(top, rowOf(std::to_string(c.den))));
        return root;
    }
    exact::Rational r;
    if (v.rational(r)) {
        const std::int64_t n = std::llabs(r.num);
        if (r.num < 0) appendText(*root, "-");
        if (mixed && n > r.den) {
            appendText(*root, std::to_string(n / r.den));
            root->children.push_back(fraction(rowOf(std::to_string(n % r.den)), rowOf(std::to_string(r.den))));
        } else {
            root->children.push_back(fraction(rowOf(std::to_string(n)), rowOf(std::to_string(r.den))));
        }
        return root;
    }
    // Surd: numerators over the least common denominator, as on the fx-CG50.
    std::int64_t lcd = 1;
    for (std::size_t i = 0; i < v.termCount(); ++i) { const std::int64_t d = v.term(i).coef.den; lcd = lcd / std::gcd(lcd, d) * d; }
    auto top = ast::make(ast::Kind::Row);
    bool leadingMinus = false;
    for (std::size_t i = 0; i < v.termCount(); ++i) {
        const exact::Term& t = v.term(i);
        const std::int64_t num = t.coef.num * (lcd / t.coef.den), a = std::llabs(num);
        if (num < 0) { if (i == 0 && v.termCount() == 1 && lcd > 1) leadingMinus = true; else appendText(*top, "-"); }
        else if (i > 0) appendText(*top, "+");
        if (t.radicand == 1) appendText(*top, std::to_string(a));
        else { if (a != 1) appendText(*top, std::to_string(a)); top->children.push_back(radical(t.radicand)); }
    }
    if (lcd == 1) return top;
    if (leadingMinus) appendText(*root, "-");
    root->children.push_back(fraction(top, rowOf(std::to_string(lcd))));
    return root;
}
} // namespace

std::string resultText(const HistoryEntry& e, const Settings& s) {
    if (!e.error.empty() && e.error[0] == '=') return e.error.substr(1);
    if (!e.error.empty()) return e.error;
    if (e.value.imag() == 0) {
        if (e.view >= ViewHex && e.view <= ViewOct) return baseText(e);
        if (e.view == ViewDms) return dmsText(e, s);
        exact::Rational r;
        if (e.view == ViewPeriodic && rationalOf(e, r)) return periodicText(r);
        if (showsExact(e, s)) return exact::toText(e.exact.value, showsMixed(e, s));
    }
    return formatValue(e.value, s.complex, s.display);
}

ast::NodePtr resultTree(const HistoryEntry& e, const Settings& s) {
    auto root = ast::make(ast::Kind::Row);
    if (plainResult(e)) {
        if (showsExact(e, s)) return exactTree(e.exact.value, showsMixed(e, s));
        exact::Rational r;
        if (e.view == ViewPeriodic && rationalOf(e, r)) {
            const exact::Expansion x = exact::decimalExpansion(r);
            appendText(*root, (x.negative ? "-" : "") + x.integer);
            if (!x.fixed.empty() || !x.period.empty()) appendText(*root, ".");
            appendText(*root, x.fixed);
            if (!x.period.empty()) {
                auto over = ast::make(ast::Kind::Call, "overline");
                over->children = {rowOf(x.period)};
                root->children.push_back(over);
            }
            if (x.truncated) appendText(*root, "\xE2\x80\xA6");
            return root;
        }
        if (e.view <= ViewDecimal || e.view == ViewExact) {
            if (isApproximation(e, s)) appendText(*root, "\xE2\x89\x88");      // ≈ numerical method
            const FormattedNumber fnum = formatNumber(e.value.real(), s.display);
            appendText(*root, fnum.mantissa);
            if (fnum.scientific) {
                root->children.push_back(tok("*"));
                auto p = ast::make(ast::Kind::Power);
                p->children = {rowOf("10"), rowOf(std::to_string(fnum.exponent))};
                root->children.push_back(p);
            }
            return root;
        }
    }
    // Complex, base-N, DMS and pair results are shown as plain text tokens.
    appendText(*root, resultText(e, s));
    return root;
}

std::string resultExpression(const HistoryEntry& e) {
    if (e.exact.valid && plainResult(e)) return exact::toExpression(e.exact.value);
    char buf[96];
    if (e.value.imag() == 0) { std::snprintf(buf, sizeof(buf), "%.15g", e.value.real()); return buf; }
    std::snprintf(buf, sizeof(buf), "(%.15g%+.15gi)", e.value.real(), e.value.imag());
    return buf;
}
} // namespace fx
