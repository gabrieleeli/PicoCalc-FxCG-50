#include "run_matrix.hpp"
#include "math_engine.hpp"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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
    for (char c : text) r->children.push_back(tok(std::string(1, c)));
    return r;
}
} // namespace

RunOutcome runCalculation(Workspace& ws, const std::string& raw) {
    std::string expr = trim(raw);
    RunOutcome out;
    // Store arrow: "expr -> A" (A-Z, case-insensitive).
    std::size_t arrow = expr.rfind(kArrow);
    std::size_t arrowLen = 3;
    if (arrow == std::string::npos) { arrow = expr.rfind("->"); arrowLen = 2; }
    if (arrow != std::string::npos) {
        const std::string target = trim(expr.substr(arrow + arrowLen));
        if (target.size() != 1 || !std::isalpha(static_cast<unsigned char>(target[0])))
            throw SyntaxError("Syntax ERROR: dopo -> serve una variabile A-Z");
        out.storedTo = static_cast<char>(std::toupper(static_cast<unsigned char>(target[0])));
        expr = trim(expr.substr(0, arrow));
    }
    ws.apply();
    auto tree = parseExpression(expr);
    const EvalContext ctx = ws.evalContext();
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
    if (out.storedTo) {
        if (out.value.imag() != 0) throw MathError("Argument ERROR: le variabili A-Z sono reali");
        ws.memory.store(out.storedTo, out.value.real());
    }
    ws.memory.setAnswer(out.value.real(), out.value.imag());
    HistoryEntry h;
    h.expression = raw;
    h.value = out.value;
    if (!out.pairText.empty()) h.error = std::string("=") + out.pairText;   // '=' prefix: informative text, not an error
    ws.addHistory(std::move(h));
    return out;
}

bool showsAsFraction(const HistoryEntry& e, const Settings& s, long long& num, long long& den) {
    if (e.view != ViewAuto || !s.fractionResults || e.value.imag() != 0 || !e.error.empty()) return false;
    if (e.expression.find('.') != std::string::npos) return false;
    const double v = e.value.real();
    if (v == std::floor(v)) return false;
    return exactFraction(v, num, den, 9999) && den > 1;
}

std::string resultText(const HistoryEntry& e, const Settings& s) {
    if (!e.error.empty() && e.error[0] == '=') return e.error.substr(1);
    if (!e.error.empty()) return e.error;
    long long n = 0, d = 0;
    if (showsAsFraction(e, s, n, d)) return std::to_string(n) + "/" + std::to_string(d);
    const double v = e.value.real();
    if (e.view >= ViewHex && e.view <= ViewOct && e.value.imag() == 0) {
        if (v != std::floor(v) || std::abs(v) > 9.0e15) return "Math ERROR: Base-N richiede un intero";
        const unsigned base = e.view == ViewHex ? 16 : e.view == ViewBin ? 2 : 8;
        const long long iv = static_cast<long long>(v);
        const std::string digits = toBase(static_cast<std::uint64_t>(iv < 0 ? -iv : iv), base);
        const char* prefix = e.view == ViewHex ? "0x" : e.view == ViewBin ? "0b" : "0o";
        return (iv < 0 ? "-" : "") + std::string(prefix) + digits;
    }
    if (e.view == ViewDms && e.value.imag() == 0) {
        const auto sx = decimalToSexagesimal(v);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d\xC2\xB0%d'%s\"", sx.degrees, sx.minutes, formatReal(std::round(sx.seconds * 1e6) / 1e6, s.display).c_str());
        return (v < 0 && sx.degrees == 0 ? "-" : "") + std::string(buf);
    }
    return formatValue(e.value, s.complex, s.display);
}

ast::NodePtr resultTree(const HistoryEntry& e, const Settings& s) {
    auto root = ast::make(ast::Kind::Row);
    long long n = 0, d = 0;
    if (showsAsFraction(e, s, n, d)) {
        if (n < 0) { root->children.push_back(tok("-")); n = -n; }
        auto f = ast::make(ast::Kind::Fraction);
        f->children = {rowOf(std::to_string(n)), rowOf(std::to_string(d))};
        root->children.push_back(f);
        return root;
    }
    if (e.error.empty() && e.value.imag() == 0 && e.view <= ViewDecimal) {
        const FormattedNumber fnum = formatNumber(e.value.real(), s.display);
        for (char c : fnum.mantissa) root->children.push_back(tok(std::string(1, c)));
        if (fnum.scientific) {
            root->children.push_back(tok("*"));
            auto p = ast::make(ast::Kind::Power);
            char exp[16]; std::snprintf(exp, sizeof(exp), "%d", fnum.exponent);
            p->children = {rowOf("10"), rowOf(exp)};
            root->children.push_back(p);
        }
        return root;
    }
    // Complex, base-N, DMS and pair results are shown as plain text tokens.
    const std::string t = resultText(e, s);
    std::size_t i = 0;
    while (i < t.size()) {
        const unsigned char c = static_cast<unsigned char>(t[i]);
        const std::size_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        root->children.push_back(tok(t.substr(i, len)));
        i += len;
    }
    return root;
}
} // namespace fx
