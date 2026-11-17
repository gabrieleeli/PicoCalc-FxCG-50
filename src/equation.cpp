// Motore della modalita' EQUATION (v1.12.0): vedi equation.hpp.
#include "equation.hpp"
#include "run_matrix.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <numeric>

namespace fx::eq {
using exact::Rational;
using exact::Value;

namespace {
constexpr double kPi = 3.14159265358979323846;
// Intervallo di calcolo della fx-CG50: |x| < 1e100, valori non nulli >= 1e-99.
constexpr double kOverflow = 1e100;
constexpr double kUnderflow = 1e-99;

bool finiteComplex(Complex z) { return std::isfinite(z.real()) && std::isfinite(z.imag()); }

// Segno "ordinato" di un numero esatto complesso: quello della parte reale, o
// dell'immaginaria quando la reale e' nulla.
int leadingSign(const ExactComplex& c) { return !c.re.isZero() ? c.re.sign() : c.im.sign(); }
int leadingSign(Complex z) { return z.real() != 0 ? (z.real() > 0 ? 1 : -1) : (z.imag() > 0 ? 1 : z.imag() < 0 ? -1 : 0); }
} // namespace

// ---------------------------------------------------------------- aritmetica esatta complessa
bool add(const ExactComplex& a, const ExactComplex& b, ExactComplex& out) noexcept {
    ExactComplex r;
    if (!exact::add(a.re, b.re, r.re) || !exact::add(a.im, b.im, r.im)) return false;
    out = r; return true;
}
bool sub(const ExactComplex& a, const ExactComplex& b, ExactComplex& out) noexcept {
    ExactComplex r;
    if (!exact::sub(a.re, b.re, r.re) || !exact::sub(a.im, b.im, r.im)) return false;
    out = r; return true;
}
bool neg(const ExactComplex& a, ExactComplex& out) noexcept {
    ExactComplex r;
    if (!exact::neg(a.re, r.re) || !exact::neg(a.im, r.im)) return false;
    out = r; return true;
}
bool mul(const ExactComplex& a, const ExactComplex& b, ExactComplex& out) noexcept {
    ExactComplex r;
    if (a.isReal() && b.isReal()) { if (!exact::mul(a.re, b.re, r.re)) return false; out = r; return true; }
    if (b.isReal()) { if (!exact::mul(a.re, b.re, r.re) || !exact::mul(a.im, b.re, r.im)) return false; out = r; return true; }
    if (a.isReal()) { if (!exact::mul(a.re, b.re, r.re) || !exact::mul(a.re, b.im, r.im)) return false; out = r; return true; }
    Value ac, bd, ad, bc;
    if (!exact::mul(a.re, b.re, ac) || !exact::mul(a.im, b.im, bd) || !exact::mul(a.re, b.im, ad) || !exact::mul(a.im, b.re, bc)) return false;
    if (!exact::sub(ac, bd, r.re) || !exact::add(ad, bc, r.im)) return false;
    out = r; return true;
}
bool div(const ExactComplex& a, const ExactComplex& b, ExactComplex& out) noexcept {
    if (b.isZero()) return false;
    ExactComplex r;
    if (b.isReal()) {
        if (!exact::div(a.re, b.re, r.re) || !exact::div(a.im, b.re, r.im)) return false;
        out = r; return true;
    }
    // a / b = a * conj(b) / |b|^2
    ExactComplex conj{b.re, {}}, num;
    if (!exact::neg(b.im, conj.im) || !mul(a, conj, num)) return false;
    Value c2, d2, den;
    if (!exact::mul(b.re, b.re, c2) || !exact::mul(b.im, b.im, d2) || !exact::add(c2, d2, den)) return false;
    if (!exact::div(num.re, den, r.re) || !exact::div(num.im, den, r.im)) return false;
    out = r; return true;
}

// ---------------------------------------------------------------- Number
Number Number::fromExact(const ExactComplex& e, bool decimal) noexcept {
    Number n; n.exact = e; n.hasExact = true; n.decimal = decimal; n.value = e.toComplex(); return n;
}
Number Number::fromComplex(Complex z) noexcept {
    Number n; n.value = z; n.hasExact = false; n.exact = {}; return n;
}
Number Number::fromRational(const Rational& r, bool decimal) noexcept { return fromExact(ExactComplex::real(Value::fromRational(r)), decimal); }
bool Number::operator==(const Number& o) const noexcept {
    if (hasExact != o.hasExact || decimal != o.decimal) return false;
    return hasExact ? exact == o.exact : value == o.value;
}

namespace {
bool hasToken(const ast::Node& n, const char* text) {
    if (n.kind == ast::Kind::Token && n.text == text) return true;
    for (const auto& c : n.children) if (c && hasToken(*c, text)) return true;
    return false;
}
void renameToken(ast::Node& n, const char* from, const char* to) {
    if (n.kind == ast::Kind::Token && n.text == from) n.text = to;
    for (auto& c : n.children) if (c) renameToken(*c, from, to);
}
// Esponente naturale scritto come numero (eventualmente tra parentesi): 0..64.
bool naturalLiteral(const ast::Node& n) {
    if (n.kind == ast::Kind::Group) return n.children.size() == 1 && n.children[0] && naturalLiteral(*n.children[0]);
    if (n.kind != ast::Kind::Token || n.text.empty() || n.text.size() > 2) return false;
    for (char c : n.text) if (c < '0' || c > '9') return false;
    return std::atoi(n.text.c_str()) <= 64;
}
// v1.13.1 (E04): true quando l'albero e' un polinomio in `marker`: i sotto-alberi
// che lo contengono usano solo +, −, ×, il segno meno, le parentesi, la divisione
// per una costante e le potenze con esponente naturale. Funzioni come Abs, Conjg,
// ReP coincidono con un polinomio sugli interi sostituiti ma non in i
// (1E10+Abs(i) dava 1E+10+i): con la variabile dentro una funzione la forma
// esatta non si calcola per sostituzione.
bool polynomialIn(const ast::Node& n, const char* marker) {
    if (!hasToken(n, marker)) return true;
    switch (n.kind) {
    case ast::Kind::Token: return true;
    case ast::Kind::Group: return n.children.size() == 1 && n.children[0] && polynomialIn(*n.children[0], marker);
    case ast::Kind::Unary: return n.text == "-" && n.children.size() == 1 && n.children[0] && polynomialIn(*n.children[0], marker);
    case ast::Kind::Binary: {
        if (n.children.size() != 2 || !n.children[0] || !n.children[1]) return false;
        const ast::Node& a = *n.children[0];
        const ast::Node& b = *n.children[1];
        if (n.text == "+" || n.text == "-" || n.text == "*") return polynomialIn(a, marker) && polynomialIn(b, marker);
        if (n.text == "/") return polynomialIn(a, marker) && !hasToken(b, marker);
        if (n.text == "^") return naturalLiteral(b) && polynomialIn(a, marker);
        return false;
    }
    default: return false;
    }
}
// Forma esatta di un'espressione complessa polinomiale in i (a+bi, (1+√3i)/2,
// (1+i)^2...). L'unico valutatore esatto (evaluateExact) calcola f(t) con i
// sostituita da un intero t = 0..4; le differenze finite danno il polinomio
// in t (grado <= 3, controllato con la quarta) e i^2 = -1 lo riduce ad A + Bi.
// Il risultato e' accettato solo se coincide con il valore complesso calcolato.
// Le tabelle (15 valori esatti, ~840 byte) stanno sull'heap: il frame resta
// piccolo anche sul core 0 (32 KB di stack).
__attribute__((noinline)) bool exactComplex(ast::Node& tree, const EvalContext& base, Complex z, ExactComplex& out, bool& decimal) {
    if (base.bindingCount >= EvalContext::MaxBindings) return false;
    static const char* const kMarker = "_i";              // nessun nome digitato inizia con '_'
    if (!polynomialIn(tree, "i")) return false;
    renameToken(tree, "i", kMarker);
    EvalContext ec = base;
    ec.bind(kMarker, 0.0);
    struct Tables { Value f[5], d[5], row[5]; };
    const auto tables = std::make_unique<Tables>();
    Value* f = tables->f;
    for (int t = 0; t < 5; ++t) {
        ec.bindings[ec.bindingCount - 1].value = t;
        bool dec = false;
        if (!evaluateExact(tree, ec, f[t], &dec)) return false;
        decimal = decimal || dec;
    }
    // Differenze finite d0..d4.
    Value* d = tables->d;
    Value* row = tables->row;
    for (int k = 0; k < 5; ++k) row[k] = f[k];
    for (int k = 0; k < 5; ++k) {
        d[k] = row[0];
        for (int j = 0; j + 1 < 5 - k; ++j) if (!exact::sub(row[j + 1], row[j], row[j])) return false;
    }
    if (!d[4].isZero()) return false;
    const Value two = Value::fromInteger(2), three = Value::fromInteger(3), six = Value::fromInteger(6);
    Value h2, h3, t2, t3, c1, c2, c3;
    // c1 = d1 - d2/2 + d3/3, c2 = d2/2 - d3/2, c3 = d3/6
    if (!exact::div(d[2], two, h2) || !exact::div(d[3], three, t3) || !exact::sub(d[1], h2, c1) || !exact::add(c1, t3, c1)) return false;
    if (!exact::div(d[3], two, t2) || !exact::sub(h2, t2, c2) || !exact::div(d[3], six, c3)) return false;
    ExactComplex r;
    if (!exact::sub(d[0], c2, r.re) || !exact::sub(c1, c3, r.im)) return false;
    // Parte reale e immaginaria si confrontano ciascuna con la propria scala (v1.13.1, E04).
    const Complex v = r.toComplex();
    if (!(std::abs(v.real() - z.real()) <= 1e-9 * std::max(1.0, std::abs(z.real()))) ||
        !(std::abs(v.imag() - z.imag()) <= 1e-9 * std::max(1.0, std::abs(z.imag())))) return false;
    out = r;
    return true;
}
} // namespace

Number evaluateNumber(const std::string& text, const EvalContext& context) {
    if (text.empty()) throw SyntaxError("Syntax ERROR: valore mancante");
    const ast::NodePtr tree = parseExpression(text);
    const bool imaginary = hasToken(*tree, "i");
    EvalContext ec = context;
    if (ec.complexMode == ComplexMode::Real && imaginary) ec.complexMode = ComplexMode::Rectangular;
    const CalcValue v = evaluateAny(*tree, ec);
    if (!v.isScalar()) throw TypeError(std::string("Ma ERROR: un coefficiente e' un numero, non una ") + valueTypeName(v.type));
    const Complex z = v.scalar();
    if (!finiteComplex(z) || std::abs(z.real()) >= kOverflow || std::abs(z.imag()) >= kOverflow)
        throw MathError("Ma ERROR: valore fuori dall'intervallo di calcolo");
    if (!imaginary || z.imag() == 0) {
        Value e; bool decimal = false;
        if (z.imag() == 0 && evaluateExact(*tree, ec, e, &decimal) && std::isfinite(e.toDouble())) return Number::fromExact(ExactComplex::real(e), decimal);
        if (!imaginary) return Number::fromComplex(z);
    }
    ExactComplex c; bool decimal = false;
    if (exactComplex(*tree, ec, z, c, decimal)) return Number::fromExact(c, decimal);
    return Number::fromComplex(z);
}

Number fromStoredDouble(double value) {
    if (!std::isfinite(value)) return Number::fromComplex({value, 0});
    if (value == 0) return Number{};
    char buf[40];
    for (int digits = 15; digits <= 17; ++digits) {
        std::snprintf(buf, sizeof(buf), "%.*g", digits, value);
        // Il decimale si legge con il lettore esatto dei numeri (exact::parseDecimal):
        // va bene solo se torna esattamente lo stesso double.
        Rational r;
        const bool negative = buf[0] == '-';
        if (!exact::parseDecimal(negative ? buf + 1 : buf, r)) break;
        if (negative && !exact::sub(Rational{}, r, r)) break;
        if (r.toDouble() != value) continue;
        // Gli interi restano interi; un valore non intero era un decimale (la
        // v1.11.0 memorizzava il risultato della valutazione).
        return Number::fromRational(r, !r.isInteger());
    }
    return Number::fromComplex({value, 0});
}

std::string editText(const Number& n) {
    char buf[96];
    if (!n.hasExact) {
        const Complex z = n.value;
        if (z.imag() == 0) std::snprintf(buf, sizeof(buf), "%.15g", z.real());
        else if (z.real() == 0) std::snprintf(buf, sizeof(buf), "%.15gi", z.imag());
        else std::snprintf(buf, sizeof(buf), "%.15g%+.15gi", z.real(), z.imag());
        return buf;
    }
    if (n.decimal) {
        // Un decimale torna nell'editor come decimale.
        const Complex z = n.value;
        if (z.imag() == 0) { std::snprintf(buf, sizeof(buf), "%.15g", z.real()); return buf; }
    }
    const ExactComplex& e = n.exact;
    if (e.isReal()) return exact::toExpression(e.re);
    std::string im = exact::toExpression(e.im);
    std::string imText;
    Rational r;
    if (e.im.rational(r) && r.num == 1 && r.den == 1) imText = "i";
    else if (e.im.rational(r) && r.num == -1 && r.den == 1) imText = "-i";
    else imText = "(" + im + ")i";
    if (e.re.isZero()) return imText;
    std::string out = exact::toExpression(e.re);
    if (imText[0] != '-') out += "+";
    return out + imText;
}

// ---------------------------------------------------------------- controlli dell'intervallo
namespace {
void checkRange(const Number& n) {
    const Complex z = n.value;
    for (double part : {z.real(), z.imag()}) {
        if (!std::isfinite(part) || std::abs(part) >= kOverflow) throw MathError("Ma ERROR: soluzione fuori dall'intervallo di calcolo (|x| >= 1E+100)");
        if (part != 0 && std::abs(part) < kUnderflow) throw MathError("Underflow: soluzione troppo piccola (|x| < 1E-99)");
    }
}
void checkCoefficients(const std::vector<Number>& c) {
    for (const Number& n : c)
        if (!finiteComplex(n.value) || std::abs(n.value.real()) >= kOverflow || std::abs(n.value.imag()) >= kOverflow)
            throw MathError("Ma ERROR: coefficiente fuori dall'intervallo di calcolo");
}
bool allExact(const std::vector<Number>& c) {
    for (const Number& n : c) if (!n.hasExact) return false;
    return true;
}
bool anyDecimal(const std::vector<Number>& c) {
    for (const Number& n : c) if (n.decimal) return true;
    return false;
}
} // namespace

// ---------------------------------------------------------------- SIML
namespace {
// Gauss-Jordan esatto su una matrice rows x cols (per righe). pivots[r] = colonna del pivot.
// dec: per ogni elemento, se dipende da un numero decimale digitato (la
// fx-CG50 mostra decimali solo quei risultati: manuale 4-2, X=−½i accanto a
// Y=1.4−1.8i−(2+i)Z).
bool rrefExact(std::vector<ExactComplex>& m, std::vector<bool>& dec, int rows, int cols, int pivotCols, std::vector<int>& pivots) {
    pivots.clear();
    int r = 0;
    for (int c = 0; c < pivotCols && r < rows; ++c) {
        int p = -1;
        for (int k = r; k < rows; ++k) if (!m[static_cast<std::size_t>(k * cols + c)].isZero()) { p = k; break; }
        if (p < 0) continue;
        if (p != r) for (int j = 0; j < cols; ++j) {
            std::swap(m[static_cast<std::size_t>(p * cols + j)], m[static_cast<std::size_t>(r * cols + j)]);
            const bool t = dec[static_cast<std::size_t>(p * cols + j)];
            dec[static_cast<std::size_t>(p * cols + j)] = dec[static_cast<std::size_t>(r * cols + j)];
            dec[static_cast<std::size_t>(r * cols + j)] = t;
        }
        const ExactComplex pivot = m[static_cast<std::size_t>(r * cols + c)];
        const bool pivotDec = dec[static_cast<std::size_t>(r * cols + c)];
        for (int j = c; j < cols; ++j) {
            ExactComplex& x = m[static_cast<std::size_t>(r * cols + j)];
            if (!div(x, pivot, x)) return false;
            if (!x.isZero() && pivotDec) dec[static_cast<std::size_t>(r * cols + j)] = true;
        }
        for (int k = 0; k < rows; ++k) {
            if (k == r) continue;
            const ExactComplex factor = m[static_cast<std::size_t>(k * cols + c)];
            if (factor.isZero()) continue;
            const bool factorDec = dec[static_cast<std::size_t>(k * cols + c)];
            for (int j = c; j < cols; ++j) {
                ExactComplex t;
                const ExactComplex& rv = m[static_cast<std::size_t>(r * cols + j)];
                if (!mul(factor, rv, t)) return false;
                ExactComplex& x = m[static_cast<std::size_t>(k * cols + j)];
                if (!sub(x, t, x)) return false;
                if (!rv.isZero() && (factorDec || dec[static_cast<std::size_t>(r * cols + j)])) dec[static_cast<std::size_t>(k * cols + j)] = true;
            }
        }
        pivots.push_back(c);
        ++r;
    }
    return true;
}
// Gauss-Jordan in virgola mobile con pivoting parziale. Un pivot e' nullo
// sotto una soglia relativa alla norma della matrice dei coefficienti.
void rrefNumeric(std::vector<Complex>& m, int rows, int cols, int coefCols, std::vector<int>& pivots) {
    pivots.clear();
    // Equilibratura: ogni equazione divisa per il suo coefficiente piu' grande,
    // cosi' la soglia dei pivot nulli vale anche per righe di scala molto diversa
    // (1E-60 X = 1E60). Le soluzioni non cambiano.
    for (int k = 0; k < rows; ++k) {
        double big = 0;
        for (int j = 0; j < coefCols; ++j) big = std::max(big, std::abs(m[static_cast<std::size_t>(k * cols + j)]));
        if (big > 0 && std::isfinite(big)) for (int j = 0; j < cols; ++j) m[static_cast<std::size_t>(k * cols + j)] /= big;
    }
    // Soglia dei pivot nulli per colonna: relativa alla colonna piu' grande dei
    // coefficienti, e per i termini noti alla loro scala.
    double coefNorm = 0, rhsNorm = 0;
    for (int k = 0; k < rows; ++k)
        for (int j = 0; j < cols; ++j) {
            const double a = std::abs(m[static_cast<std::size_t>(k * cols + j)]);
            if (j < coefCols) coefNorm = std::max(coefNorm, a); else rhsNorm = std::max(rhsNorm, a);
        }
    const double tiny = std::numeric_limits<double>::min();
    int r = 0;
    for (int c = 0; c < cols && r < rows; ++c) {
        const double tol = 1e-11 * std::max(1, rows) * std::max(c < coefCols ? coefNorm : rhsNorm, tiny);
        int p = r; double best = std::abs(m[static_cast<std::size_t>(r * cols + c)]);
        for (int k = r + 1; k < rows; ++k) { const double a = std::abs(m[static_cast<std::size_t>(k * cols + c)]); if (a > best) { best = a; p = k; } }
        if (!(best > tol)) { for (int k = r; k < rows; ++k) m[static_cast<std::size_t>(k * cols + c)] = 0; continue; }
        if (p != r) for (int j = 0; j < cols; ++j) std::swap(m[static_cast<std::size_t>(p * cols + j)], m[static_cast<std::size_t>(r * cols + j)]);
        const Complex pivot = m[static_cast<std::size_t>(r * cols + c)];
        for (int j = c; j < cols; ++j) m[static_cast<std::size_t>(r * cols + j)] /= pivot;
        m[static_cast<std::size_t>(r * cols + c)] = 1;
        for (int k = 0; k < rows; ++k) {
            if (k == r) continue;
            const Complex factor = m[static_cast<std::size_t>(k * cols + c)];
            if (factor == Complex{}) continue;
            for (int j = c; j < cols; ++j) m[static_cast<std::size_t>(k * cols + j)] -= factor * m[static_cast<std::size_t>(r * cols + j)];
            m[static_cast<std::size_t>(k * cols + c)] = 0;
        }
        pivots.push_back(c);
        ++r;
    }
    // Residui dell'eliminazione nei coefficienti: le parti trascurabili diventano
    // zero (i termini noti, cioe' le soluzioni, restano come sono).
    for (int k = 0; k < rows; ++k)
        for (int j = 0; j < coefCols; ++j) {
            Complex& z = m[static_cast<std::size_t>(k * cols + j)];
            const double clean = 1e-12 * std::max(1.0, coefNorm);
            if (std::abs(z.real()) < clean) z.real(0.0);
            if (std::abs(z.imag()) < clean) z.imag(0.0);
        }
}
} // namespace

SimulResult solveSimultaneous(const std::vector<Number>& coefficients, int n) {
    if (n < 1 || n > 6 || coefficients.size() < static_cast<std::size_t>(n * (n + 1))) throw DimensionError("Dimension ERROR: sistema non valido");
    checkCoefficients(coefficients);
    const int cols = n + 1;
    const std::size_t cells = static_cast<std::size_t>(n * cols);
    std::vector<Number> rref(cells);
    std::vector<int> pivots;
    const std::vector<Number> used(coefficients.begin(), coefficients.begin() + static_cast<std::ptrdiff_t>(cells));
    bool exactDone = false;
    if (allExact(used)) {
        std::vector<ExactComplex> m(cells);
        std::vector<bool> dec(cells);
        for (std::size_t i = 0; i < cells; ++i) { m[i] = used[i].exact; dec[i] = used[i].decimal; }
        if (rrefExact(m, dec, n, cols, cols, pivots)) {
            for (std::size_t i = 0; i < cells; ++i) rref[i] = Number::fromExact(m[i], dec[i] && !m[i].isZero());
            exactDone = true;
        }
    }
    if (!exactDone) {
        std::vector<Complex> m(cells);
        for (std::size_t i = 0; i < cells; ++i) m[i] = used[i].value;
        rrefNumeric(m, n, cols, n, pivots);   // Rref della matrice completa (SimRes, manuale 1-33)
        for (std::size_t i = 0; i < cells; ++i) rref[i] = Number::fromComplex(m[i]);
    }
    SimulResult out;
    out.unknowns = n;
    out.exactPath = exactDone;
    // Un pivot nella colonna dei termini noti: nessuna soluzione.
    const bool inconsistent = !pivots.empty() && pivots.back() == n;
    if (inconsistent) pivots.pop_back();
    const int rank = static_cast<int>(pivots.size());
    out.rref = rref;
    if (inconsistent) { out.kind = SimulResult::Kind::None; return out; }
    if (rank == n) {
        out.kind = SimulResult::Kind::Unique;
        out.solution.resize(static_cast<std::size_t>(n));
        for (int r = 0; r < n; ++r) {
            out.solution[static_cast<std::size_t>(pivots[static_cast<std::size_t>(r)])] = rref[static_cast<std::size_t>(r * cols + n)];
        }
        for (const Number& x : out.solution) checkRange(x);
        return out;
    }
    // Infinite soluzioni: le colonne senza pivot sono incognite libere.
    out.kind = SimulResult::Kind::Infinite;
    out.free.assign(static_cast<std::size_t>(n), true);
    for (int p : pivots) out.free[static_cast<std::size_t>(p)] = false;
    for (int k = 0; k < n; ++k) if (out.free[static_cast<std::size_t>(k)]) out.freeVars.push_back(static_cast<std::uint8_t>(k));
    const std::size_t F = out.freeVars.size(), stride = 1 + F;
    out.formula.assign(static_cast<std::size_t>(n) * stride, Number{});
    for (std::size_t j = 0; j < F; ++j) {
        const std::size_t k = out.freeVars[j];
        out.formula[k * stride + 1 + j] = Number::fromRational(Rational::integer(1));
    }
    for (int r = 0; r < rank; ++r) {
        const std::size_t p = static_cast<std::size_t>(pivots[static_cast<std::size_t>(r)]);
        out.formula[p * stride] = rref[static_cast<std::size_t>(r * cols + n)];
        for (std::size_t j = 0; j < F; ++j) {
            const Number& a = rref[static_cast<std::size_t>(r * cols) + out.freeVars[j]];
            Number c;
            if (a.hasExact) { ExactComplex e; if (neg(a.exact, e)) c = Number::fromExact(e, a.decimal); else c = Number::fromComplex(-a.value); }
            else c = Number::fromComplex(-a.value);
            out.formula[p * stride + 1 + j] = c;
        }
    }
    return out;
}

// ---------------------------------------------------------------- POLY
namespace {
// Polinomio razionale, coefficiente di x^k all'indice k.
using RPoly = std::vector<Rational>;
void trimPoly(RPoly& p) { while (p.size() > 1 && p.back().isZero()) p.pop_back(); }
int degreeOf(const RPoly& p) { return p.empty() || (p.size() == 1 && p[0].isZero()) ? -1 : static_cast<int>(p.size()) - 1; }
bool makeMonic(RPoly& p) {
    trimPoly(p);
    if (degreeOf(p) < 0) return true;
    const Rational lead = p.back();
    for (Rational& c : p) if (!exact::div(c, lead, c)) return false;
    return true;
}
bool derivative(const RPoly& p, RPoly& out) {
    out.assign(p.size() > 1 ? p.size() - 1 : 1, Rational{});
    for (std::size_t k = 1; k < p.size(); ++k) if (!exact::mul(p[k], Rational::integer(static_cast<std::int64_t>(k)), out[k - 1])) return false;
    trimPoly(out);
    return true;
}
// a = q*b + r (b non nullo)
bool divide(const RPoly& a, const RPoly& b, RPoly& q, RPoly& r) {
    r = a; trimPoly(r);
    const int db = degreeOf(b);
    if (db < 0) return false;
    int dr = degreeOf(r);
    q.assign(dr >= db ? static_cast<std::size_t>(dr - db + 1) : 1, Rational{});
    while (dr >= db && dr >= 0) {
        Rational f;
        if (!exact::div(r[static_cast<std::size_t>(dr)], b[static_cast<std::size_t>(db)], f)) return false;
        q[static_cast<std::size_t>(dr - db)] = f;
        for (int k = 0; k <= db; ++k) {
            Rational t;
            if (!exact::mul(f, b[static_cast<std::size_t>(k)], t)) return false;
            Rational& x = r[static_cast<std::size_t>(dr - db + k)];
            if (!exact::sub(x, t, x)) return false;
        }
        r[static_cast<std::size_t>(dr)] = Rational{};
        trimPoly(r);
        dr = degreeOf(r);
    }
    trimPoly(q);
    return true;
}
bool gcdPoly(RPoly a, RPoly b, RPoly& g) {
    trimPoly(a); trimPoly(b);
    while (degreeOf(b) >= 0) {
        RPoly q, r;
        if (!divide(a, b, q, r)) return false;
        a = std::move(b); b = std::move(r);
        if (!makeMonic(b)) return false;
    }
    g = std::move(a);
    return makeMonic(g);
}
bool exactQuotient(const RPoly& a, const RPoly& b, RPoly& q) {
    RPoly r;
    return divide(a, b, q, r) && degreeOf(r) < 0;
}
// Scomposizione senza quadrati di Yun: p = prodotto di a_i^i (a_i senza
// fattori ripetuti). factors[i-1] = a_i.
bool squareFree(const RPoly& p, std::vector<RPoly>& factors) {
    factors.clear();
    RPoly f = p;
    if (!makeMonic(f)) return false;
    RPoly fd, a0, b, c, d;
    if (!derivative(f, fd) || !gcdPoly(f, fd, a0) || !exactQuotient(f, a0, b) || !exactQuotient(fd, a0, c)) return false;
    RPoly bd;
    if (!derivative(b, bd)) return false;
    d = c;
    d.resize(std::max(c.size(), bd.size()));
    for (std::size_t k = 0; k < d.size(); ++k) {
        const Rational x = k < c.size() ? c[k] : Rational{}, y = k < bd.size() ? bd[k] : Rational{};
        if (!exact::sub(x, y, d[k])) return false;
    }
    trimPoly(d);
    for (int guard = 0; degreeOf(b) > 0 && guard < 8; ++guard) {
        RPoly a;
        if (!gcdPoly(b, d, a)) return false;
        factors.push_back(a);
        RPoly nb, nc;
        if (!exactQuotient(b, a, nb) || !exactQuotient(d, a, nc)) return false;
        RPoly nbd;
        if (!derivative(nb, nbd)) return false;
        RPoly nd(std::max(nc.size(), nbd.size()));
        for (std::size_t k = 0; k < nd.size(); ++k) {
            const Rational x = k < nc.size() ? nc[k] : Rational{}, y = k < nbd.size() ? nbd[k] : Rational{};
            if (!exact::sub(x, y, nd[k])) return false;
        }
        trimPoly(nd);
        b = std::move(nb); d = std::move(nd);
    }
    return degreeOf(b) <= 0;
}
bool evalRational(const RPoly& p, const Rational& x, Rational& out) {
    Rational acc{};
    for (std::size_t k = p.size(); k-- > 0;) {
        if (!exact::mul(acc, x, acc) || !exact::add(acc, p[k], acc)) return false;
    }
    out = acc; return true;
}
std::int64_t gcd64(std::int64_t a, std::int64_t b) { a = a < 0 ? -a : a; b = b < 0 ? -b : b; while (b) { const std::int64_t t = a % b; a = b; b = t; } return a; }
// Divisori positivi di n (n <= 1e12), al piu' `limit`.
bool divisors(std::int64_t n, std::vector<std::int64_t>& out, std::size_t limit) {
    out.clear();
    if (n <= 0) return false;
    std::vector<std::int64_t> high;
    for (std::int64_t d = 1; d <= n / d; ++d) {
        if (n % d != 0) continue;
        out.push_back(d);
        if (d != n / d) high.push_back(n / d);
        if (out.size() + high.size() > limit) return false;
    }
    out.insert(out.end(), high.rbegin(), high.rend());
    return true;
}
// Coefficienti interi primitivi di un polinomio razionale.
bool integerCoefficients(const RPoly& p, std::vector<std::int64_t>& out) {
    std::int64_t l = 1;
    for (const Rational& c : p) {
        const std::int64_t g = gcd64(l, c.den);
        const std::int64_t m = c.den / g;
        if (l > std::numeric_limits<std::int64_t>::max() / m) return false;
        l *= m;
    }
    out.clear();
    std::int64_t content = 0;
    for (const Rational& c : p) {
        const std::int64_t f = l / c.den;
        if (c.num != 0 && std::llabs(c.num) > std::numeric_limits<std::int64_t>::max() / f) return false;
        out.push_back(c.num * f);
        content = gcd64(content, out.back());
    }
    if (content > 1) for (auto& v : out) v /= content;
    return true;
}
// Radici razionali di un polinomio senza quadrati (teorema delle radici razionali); p viene deflazionato.
bool rationalRoots(RPoly& p, std::vector<Rational>& roots) {
    std::vector<std::int64_t> ic;
    if (!integerCoefficients(p, ic) || ic.empty()) return false;
    const std::int64_t c0 = ic.front(), lead = ic.back();
    if (c0 == 0) return false;
    if (std::llabs(c0) > 1000000000000LL || std::llabs(lead) > 100000000LL) return false;
    std::vector<std::int64_t> ps, qs;
    if (!divisors(std::llabs(c0), ps, 4096) || !divisors(std::llabs(lead), qs, 1024)) return false;
    if (ps.size() * qs.size() > 40000) return false;
    for (std::int64_t q : qs) {
        for (std::int64_t pp : ps) {
            if (gcd64(pp, q) != 1) continue;
            for (int sign : {1, -1}) {
                if (degreeOf(p) < 1) return true;
                Rational x, v;
                if (!Rational::make(sign * pp, q, x)) continue;
                if (!evalRational(p, x, v)) return false;
                if (!v.isZero()) continue;
                roots.push_back(x);
                RPoly lin{Rational{}, Rational::integer(1)}, q2;
                if (!exact::sub(Rational{}, x, lin[0])) return false;
                if (!exactQuotient(p, lin, q2)) return false;
                p = std::move(q2);
            }
        }
    }
    return true;
}
// Radici numeriche (Aberth-Ehrlich + Newton) di un polinomio complesso, coefficienti dal grado piu' alto.
Complex horner(const std::vector<Complex>& a, Complex z, Complex* derivativeOut = nullptr) {
    Complex p = a[0], d = 0;
    for (std::size_t i = 1; i < a.size(); ++i) { d = d * z + p; p = p * z + a[i]; }
    if (derivativeOut) *derivativeOut = d;
    return p;
}
std::vector<Complex> numericRoots(const std::vector<Complex>& a) {
    const std::size_t n = a.size() - 1;
    std::vector<Complex> z(n);
    if (n == 0) return z;
    if (n == 1) { z[0] = -a[1] / a[0]; return z; }
    std::vector<Complex> m(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) m[i] = a[i] / a[0];
    double radius = 0;
    for (std::size_t i = 1; i < m.size(); ++i) radius = std::max(radius, std::pow(std::abs(m[i]), 1.0 / static_cast<double>(i)));
    double r0 = std::abs(m.back()) > 0 ? std::pow(std::abs(m.back()), 1.0 / static_cast<double>(n)) : 0.5;
    if (!(r0 > 0) || !std::isfinite(r0)) r0 = 1;
    r0 = std::min(r0, 2 * radius + 1);
    for (std::size_t k = 0; k < n; ++k) z[k] = std::polar(r0, 2.0 * kPi * static_cast<double>(k) / static_cast<double>(n) + 0.4);
    for (int it = 0; it < 800; ++it) {
        double maxStep = 0;
        for (std::size_t i = 0; i < n; ++i) {
            Complex d;
            const Complex p = horner(m, z[i], &d);
            if (p == Complex(0, 0)) continue;
            const Complex ratio = p / d;
            Complex sum = 0;
            for (std::size_t j = 0; j < n; ++j) if (j != i) sum += 1.0 / (z[i] - z[j]);
            const Complex w = ratio / (1.0 - ratio * sum);
            if (!finiteComplex(w)) continue;
            z[i] -= w;
            maxStep = std::max(maxStep, std::abs(w) / std::max(1.0, std::abs(z[i])));
        }
        if (maxStep < 1e-16) break;
    }
    for (auto& r : z) {
        for (int k = 0; k < 4; ++k) {
            Complex d; const Complex p = horner(a, r, &d);
            if (std::abs(d) == 0) break;
            const Complex nr = r - p / d;
            if (!finiteComplex(nr)) break;
            if (std::abs(horner(a, nr)) <= std::abs(p)) r = nr; else break;
        }
    }
    return z;
}
// Radici esatte di un fattore senza quadrati di grado 1 o 2.
bool exactLowDegree(const RPoly& p, std::vector<ExactComplex>& out) {
    const int d = degreeOf(p);
    if (d == 1) {
        Rational x;
        if (!exact::div(p[0], p[1], x) || !exact::sub(Rational{}, x, x)) return false;
        out.push_back(ExactComplex::real(Value::fromRational(x)));
        return true;
    }
    if (d != 2) return false;
    const Rational& a = p[2]; const Rational& b = p[1]; const Rational& c = p[0];
    Rational b2, ac, ac4, disc, twoA, mb;
    if (!exact::mul(b, b, b2) || !exact::mul(a, c, ac) || !exact::mul(ac, Rational::integer(4), ac4) || !exact::sub(b2, ac4, disc)) return false;
    if (!exact::mul(a, Rational::integer(2), twoA) || !exact::sub(Rational{}, b, mb)) return false;
    Rational center;
    if (!exact::div(mb, twoA, center)) return false;
    const bool negative = disc.sign() < 0;
    Rational absDisc = disc;
    if (negative && !exact::sub(Rational{}, disc, absDisc)) return false;
    Value root, half;
    if (!exact::sqrt(Value::fromRational(absDisc), root)) return false;
    if (!exact::div(root, Value::fromRational(twoA), half)) return false;
    if (half.sign() < 0 && !exact::neg(half, half)) return false;
    const Value centerV = Value::fromRational(center);
    if (negative) {
        ExactComplex z1{centerV, half}, z2{centerV, {}};
        if (!exact::neg(half, z2.im)) return false;
        out.push_back(z1); out.push_back(z2);
        return true;
    }
    ExactComplex z1, z2;
    if (!exact::add(centerV, half, z1.re) || !exact::sub(centerV, half, z2.re)) return false;
    out.push_back(z1); out.push_back(z2);
    return true;
}
// Radici esatte di un fattore senza quadrati senza radici razionali.
// Biquadratico (solo potenze pari): y = x^2, radici razionali y -> x = ±√y.
bool exactEven(const RPoly& p, std::vector<ExactComplex>& out) {
    const int d = degreeOf(p);
    if (d < 4 || d % 2 != 0) return false;
    for (int k = 1; k <= d; k += 2) if (!p[static_cast<std::size_t>(k)].isZero()) return false;
    if (d == 4) {
        // x^4 + b x^2 + c (monico) = (x^2 + s)^2 - (2s - b) x^2 con s^2 = c e
        // 2s - b = t^2 razionali: (x^2 - t x + s)(x^2 + t x + s). Esempio x^4+x^2+1.
        RPoly m = p;
        if (makeMonic(m)) {
            Value sv;
            if (exact::sqrt(Value::fromRational(m[0]), sv)) {
                Rational s0;
                if (sv.rational(s0)) {
                    for (int sign : {1, -1}) {
                        Rational s = s0, twoS, t2;
                        if (sign < 0 && !exact::sub(Rational{}, s0, s)) continue;
                        if (!exact::mul(s, Rational::integer(2), twoS) || !exact::sub(twoS, m[2], t2) || t2.sign() <= 0) continue;
                        Value tv; Rational t;
                        if (!exact::sqrt(Value::fromRational(t2), tv) || !tv.rational(t)) continue;
                        Rational mt;
                        if (!exact::sub(Rational{}, t, mt)) continue;
                        std::vector<ExactComplex> found;
                        if (exactLowDegree(RPoly{s, mt, Rational::integer(1)}, found) && exactLowDegree(RPoly{s, t, Rational::integer(1)}, found)) {
                            out.insert(out.end(), found.begin(), found.end());
                            return true;
                        }
                    }
                }
            }
        }
    }
    RPoly y;
    for (int k = 0; k <= d; k += 2) y.push_back(p[static_cast<std::size_t>(k)]);
    // Radici razionali di q(y) e quadratiche esatte del resto.
    std::vector<Rational> ys;
    RPoly rest = y;
    if (!makeMonic(rest) || !rationalRoots(rest, ys)) return false;
    std::vector<ExactComplex> yRoots;
    for (const Rational& r : ys) yRoots.push_back(ExactComplex::real(Value::fromRational(r)));
    if (degreeOf(rest) > 2) return false;
    if (degreeOf(rest) >= 1 && !exactLowDegree(rest, yRoots)) return false;
    for (const ExactComplex& yr : yRoots) {
        Rational r;
        if (!yr.isReal() || !yr.re.rational(r)) return false;    // √ di un radicale: nessuna forma esatta
        const bool negative = r.sign() < 0;
        Rational a = r;
        if (negative && !exact::sub(Rational{}, r, a)) return false;
        Value s, ms;
        if (!exact::sqrt(Value::fromRational(a), s) || !exact::neg(s, ms)) return false;
        if (negative) { out.push_back({Value{}, s}); out.push_back({Value{}, ms}); }
        else { out.push_back(ExactComplex::real(s)); out.push_back(ExactComplex::real(ms)); }
    }
    return true;
}
// Ordine della fx-CG50 (manuale 4-3, 4-4): reali decrescenti, poi complesse.
void sortRoots(std::vector<Root>& roots) {
    std::stable_sort(roots.begin(), roots.end(), [](const Root& x, const Root& y) {
        const bool rx = x.value.isReal(), ry = y.value.isReal();
        if (rx != ry) return rx;
        const Complex a = x.value.value, b = y.value.value;
        if (a.real() != b.real()) return a.real() > b.real();
        return a.imag() > b.imag();
    });
}
// Radici in virgola mobile di un polinomio qualsiasi, raggruppate per molteplicita'.
void numericPolynomial(const std::vector<Complex>& a, bool realCoefficients, std::uint8_t multiplicity, std::vector<Root>& out) {
    std::vector<Complex> z = numericRoots(a);
    auto residualSmall = [&](Complex r) {
        double s = 0, rp = 1;
        for (std::size_t i = a.size(); i-- > 0;) { s += std::abs(a[i]) * rp; rp *= std::abs(r); }
        return std::abs(horner(a, r)) <= 1e-9 * s;
    };
    if (realCoefficients) {
        for (auto& r : z) {
            const double scale = std::max(1.0, std::abs(r));
            if (std::abs(r.imag()) < 1e-7 * scale && (residualSmall({r.real(), 0}) || std::abs(r.imag()) < 1e-13 * scale)) r = {r.real(), 0.0};
        }
        // Coppie coniugate esatte.
        std::vector<bool> used(z.size(), false);
        for (std::size_t i = 0; i < z.size(); ++i) {
            if (used[i] || z[i].imag() == 0) continue;
            std::size_t best = z.size(); double bestD = std::numeric_limits<double>::max();
            for (std::size_t j = 0; j < z.size(); ++j) {
                if (j == i || used[j] || z[j].imag() == 0) continue;
                const double dd = std::abs(z[j] - std::conj(z[i]));
                if (dd < bestD) { bestD = dd; best = j; }
            }
            if (best < z.size()) {
                const Complex avg = 0.5 * (z[i] + std::conj(z[best]));
                z[i] = avg; z[best] = std::conj(avg); used[i] = used[best] = true;
            }
        }
    }
    // Radici multiple: Aberth le separa di circa eps^(1/m); si raggruppano
    // quando la media annulla anche la derivata.
    std::vector<bool> taken(z.size(), false);
    for (std::size_t i = 0; i < z.size(); ++i) {
        if (taken[i]) continue;
        std::vector<std::size_t> group{i};
        taken[i] = true;
        for (std::size_t j = i + 1; j < z.size(); ++j) {
            if (taken[j]) continue;
            const double scale = std::max(1.0, std::abs(z[i]));
            if (std::abs(z[j] - z[i]) < 1e-3 * scale) { group.push_back(j); taken[j] = true; }
        }
        Complex mean = 0;
        for (std::size_t k : group) mean += z[k];
        mean /= static_cast<double>(group.size());
        if (group.size() > 1) {
            // Conferma: la derivata (m-1)-esima deve annullarsi nella media.
            std::vector<Complex> d = a;
            bool ok = true;
            for (std::size_t m = 1; m < group.size() && ok; ++m) {
                std::vector<Complex> nd(d.size() - 1);
                const std::size_t deg = d.size() - 1;
                for (std::size_t k = 0; k + 1 < d.size(); ++k) nd[k] = d[k] * static_cast<double>(deg - k);
                d = std::move(nd);
                double s = 0, rp = 1;
                for (std::size_t k = d.size(); k-- > 0;) { s += std::abs(d[k]) * rp; rp *= std::abs(mean); }
                ok = std::abs(horner(d, mean)) <= 1e-5 * std::max(s, 1e-300);
            }
            if (!ok) {
                // Radici vicine ma distinte.
                for (std::size_t k : group) out.push_back({Number::fromComplex(z[k]), multiplicity});
                continue;
            }
            if (realCoefficients && std::abs(mean.imag()) < 1e-7 * std::max(1.0, std::abs(mean))) mean.imag(0.0);
        }
        // Residui numerici: una parte trascurabile rispetto al modulo diventa 0
        // (radici multiple: errore circa eps^(1/m)).
        const double tiny = (group.size() > 1 ? 1e-6 : 1e-13) * std::abs(mean);
        if (std::abs(mean.real()) < tiny) mean.real(0.0);
        if (std::abs(mean.imag()) < tiny) mean.imag(0.0);
        out.push_back({Number::fromComplex(mean), static_cast<std::uint8_t>(multiplicity * group.size())});
    }
}
} // namespace

std::vector<Root> solvePolynomial(const std::vector<Number>& coefficients) {
    if (coefficients.size() < 2) throw DimensionError("Dimension ERROR: grado non valido");
    checkCoefficients(coefficients);
    if (coefficients.front().isZero()) throw MathError("Ma ERROR: il coefficiente a deve essere diverso da 0");
    const bool decimal = anyDecimal(coefficients);
    std::vector<Root> roots;
    // Radici nulle: x^k a destra.
    std::size_t end = coefficients.size();
    std::uint8_t zeros = 0;
    while (end > 1 && coefficients[end - 1].isZero()) { --end; ++zeros; }
    if (zeros) roots.push_back({Number::fromRational(Rational::integer(0)), zeros});
    const std::vector<Number> c(coefficients.begin(), coefficients.begin() + static_cast<std::ptrdiff_t>(end));
    bool done = false;
    // Percorso esatto: coefficienti razionali.
    bool rational = true;
    RPoly p(c.size());
    for (std::size_t i = 0; i < c.size() && rational; ++i) {
        Rational r;
        if (!c[i].hasExact || !c[i].exact.isReal() || !c[i].exact.re.rational(r)) rational = false;
        else p[c.size() - 1 - i] = r;
    }
    if (rational && c.size() > 1) {
        std::vector<RPoly> factors;
        std::vector<Root> exactRoots;
        bool ok = squareFree(p, factors);
        for (std::size_t m = 0; ok && m < factors.size(); ++m) {
            RPoly f = factors[m];
            if (degreeOf(f) < 1) continue;
            const auto mult = static_cast<std::uint8_t>(m + 1);
            std::vector<Rational> rr;
            if (!makeMonic(f) || !rationalRoots(f, rr)) {
                // Coefficienti troppo grandi per la ricerca: radici numeriche del fattore.
                rr.clear(); f = factors[m];
                if (!makeMonic(f)) { ok = false; break; }
            }
            for (const Rational& r : rr) exactRoots.push_back({Number::fromRational(r, decimal), mult});
            std::vector<ExactComplex> ex;
            const int d = degreeOf(f);
            if (d < 1) continue;
            if ((d <= 2 && exactLowDegree(f, ex)) || exactEven(f, ex)) {
                for (const ExactComplex& e : ex) exactRoots.push_back({Number::fromExact(e, decimal), mult});
                continue;
            }
            // Fattore irriducibile di grado >= 3: radici in virgola mobile (ben separate).
            std::vector<Complex> a(f.size());
            for (std::size_t k = 0; k < f.size(); ++k) a[k] = f[f.size() - 1 - k].toDouble();
            std::vector<Root> part;
            numericPolynomial(a, true, mult, part);
            exactRoots.insert(exactRoots.end(), part.begin(), part.end());
        }
        if (ok) { roots.insert(roots.end(), exactRoots.begin(), exactRoots.end()); done = true; }
    }
    if (!done && c.size() > 1) {
        std::vector<Complex> a(c.size());
        bool real = true;
        for (std::size_t i = 0; i < c.size(); ++i) { a[i] = c[i].value; real = real && a[i].imag() == 0; }
        numericPolynomial(a, real, 1, roots);
    }
    for (const Root& r : roots) checkRange(r.value);
    sortRoots(roots);
    return roots;
}

std::vector<Root> visibleRoots(const std::vector<Root>& roots, ComplexMode mode) {
    if (mode != ComplexMode::Real) return roots;
    std::vector<Root> out;
    for (const Root& r : roots) if (r.value.isReal()) out.push_back(r);
    return out;
}

// ---------------------------------------------------------------- SOLVER
namespace {
std::size_t countEquals(const std::string& s) { return static_cast<std::size_t>(std::count(s.begin(), s.end(), '=')); }
void collectVariables(const ast::Node& n, int offset, std::vector<std::pair<int, char>>& out) {
    if (n.kind == ast::Kind::Token && n.text.size() == 1 && std::isalpha(static_cast<unsigned char>(n.text[0])) && n.text != "e" && n.text != "i") {
        const char v = static_cast<char>(std::toupper(static_cast<unsigned char>(n.text[0])));
        out.emplace_back(offset + n.pos, v);
    }
    for (const auto& c : n.children) if (c) collectVariables(*c, offset, out);
}
struct Sides { ast::NodePtr lhs, rhs; bool hasEquals{false}; };
Sides parseSides(const std::string& equation) {
    const std::size_t equals = countEquals(equation);
    if (equals > 1) throw SyntaxError("Syntax ERROR: un solo \"=\" ammesso", static_cast<int>(equation.find('=', equation.find('=') + 1)), 1);
    Sides s;
    if (equals == 0) { s.lhs = parseExpression(equation); s.rhs = parseExpression("0"); return s; }
    const std::size_t at = equation.find('=');
    if (at == 0 || at + 1 >= equation.size()) throw SyntaxError("Syntax ERROR: manca un membro dell'equazione", static_cast<int>(at), 1);
    s.lhs = parseExpression(equation.substr(0, at));
    s.rhs = parseExpression(equation.substr(at + 1));
    s.hasEquals = true;
    return s;
}
} // namespace

std::vector<char> solverVariables(const std::string& equation) {
    if (equation.empty()) return {};
    const Sides s = parseSides(equation);
    std::vector<std::pair<int, char>> found;
    collectVariables(*s.lhs, 0, found);
    collectVariables(*s.rhs, static_cast<int>(equation.find('=') == std::string::npos ? 0 : equation.find('=') + 1), found);
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<char> out;
    for (const auto& f : found) if (std::find(out.begin(), out.end(), f.second) == out.end()) out.push_back(f.second);
    return out;
}

SolverOutcome solveFor(const std::string& equation, char variable, double lower, double upper, const EvalContext& context) {
    if (equation.empty()) throw SyntaxError("Syntax ERROR: equazione vuota");
    const Sides s = parseSides(equation);
    const std::vector<char> vars = solverVariables(equation);
    variable = static_cast<char>(std::toupper(static_cast<unsigned char>(variable)));
    if (vars.empty() || std::find(vars.begin(), vars.end(), variable) == vars.end())
        throw MathError("No Variable: l'equazione non contiene la variabile da risolvere");
    if (!(lower <= upper) || !std::isfinite(lower) || !std::isfinite(upper)) throw MathError("Ma ERROR: Lower deve essere minore di Upper");
    if (!context.memory) throw MathError("Ma ERROR: memoria delle variabili non disponibile");
    // Copia delle variabili sull'heap: la variabile risolta cambia a ogni prova.
    auto memory = std::make_unique<CalculatorMemory>(*context.memory);
    EvalContext ec = context;
    ec.memory = memory.get();
    ec.complexMode = ComplexMode::Real;
    double lastL = 0, lastR = 0;
    std::size_t evaluations = 0;
    auto f = [&](double t, bool& ok) {
        // Limite di calcolo per scheda (limits.hpp): oltre, Time Out come la fx-CG50.
        if (++evaluations > limits::kSolverEvaluations) throw MathError("Time Out: troppe valutazioni dell'equazione (cambia il valore iniziale o i limiti)");
        memory->store(variable, t);
        ok = false;
        try {
            lastL = evaluateReal(*s.lhs, ec);
            lastR = evaluateReal(*s.rhs, ec);
            ok = std::isfinite(lastL) && std::isfinite(lastR);
        } catch (const MathError&) {
            return 0.0;
        }
        return lastL - lastR;
    };
    auto scaleAt = [&](double) { return std::max({1.0, std::abs(lastL), std::abs(lastR)}); };
    double x0 = context.memory->recall(variable);
    if (x0 < lower) x0 = lower;
    if (x0 > upper) x0 = upper;
    bool outsideFound = false;
    auto accept = [&](double x, SolverOutcome& out) -> int {
        // 2 = soluzione, 1 = Retry, 0 = no.
        bool ok = false;
        const double y = f(x, ok);
        if (!ok) return 0;
        const double sc = scaleAt(x);
        int verdict = std::abs(y) <= 1e-11 * sc ? 2 : std::abs(y) <= 1e-6 * sc ? 1 : 0;
        if (verdict == 0) return 0;
        if (verdict == 1) {
            // "Retry" solo vicino a un vero cambio di segno (non per 1/x = 0 a x grande).
            const double delta = 1e-6 * std::max(1.0, std::abs(x));
            bool o1 = false, o2 = false;
            const double a = f(x - delta, o1), b = f(x + delta, o2);
            if (!o1 || !o2 || (a > 0) == (b > 0)) return 0;
            (void)f(x, ok);
        }
        if (x < lower || x > upper) { outsideFound = true; return 0; }
        out.value = x; out.left = lastL; out.right = lastR; out.retry = verdict == 1;
        return verdict;
    };
    SolverOutcome best;
    bool haveRetry = false;
    // 1) Newton con differenze centrali dal valore attuale della variabile.
    {
        double x = x0;
        const double escape = std::max(1e3, 1e8 * std::max(1.0, std::abs(x0)));
        for (int i = 0; i < 80; ++i) {
            if (!(std::abs(x) <= escape)) break;
            bool ok = false;
            const double y = f(x, ok);
            if (!ok) break;
            if (std::abs(y) <= 1e-13 * scaleAt(x)) break;
            const double h = 1e-6 * std::max(1.0, std::abs(x));
            bool ok1 = false, ok2 = false;
            const double y1 = f(x + h, ok1), y2 = f(x - h, ok2);
            double d = 0;
            if (ok1 && ok2) d = (y1 - y2) / (2 * h);
            else if (ok1) d = (y1 - y) / h;
            else if (ok2) d = (y - y2) / h;
            if (d == 0 || !std::isfinite(d)) break;
            const double nx = x - y / d;
            if (!std::isfinite(nx)) break;
            const bool stalled = std::abs(nx - x) <= 1e-15 * std::max(1.0, std::abs(x));
            x = nx;
            if (stalled) break;
        }
        SolverOutcome o;
        const int v = accept(x, o);
        if (v == 2) return o;
        if (v == 1) { best = o; haveRetry = true; }
    }
    // 2) Ricerca di un cambio di segno attorno al valore iniziale, entro i
    // limiti, con passi crescenti su ogni lato; bisezione sul primo tratto
    // con un cambio di segno. I cambi di segno dei poli (1/x) hanno un
    // residuo grande e sono scartati.
    {
        double prev[2] = {x0, x0}, fprev[2] = {0, 0};
        bool okPrev[2] = {false, false};
        {
            bool ok = false;
            const double y = f(x0, ok);
            for (int s2 = 0; s2 < 2; ++s2) { fprev[s2] = y; okPrev[s2] = ok; }
        }
        double step = std::max(0.5, std::abs(x0) * 0.1);
        for (int k = 0; k < 160; ++k, step *= (k < 60 ? 1.6 : 6.0)) {
            bool moved = false;
            for (int s2 = 0; s2 < 2; ++s2) {
                double b = x0 + (s2 == 0 ? step : -step);
                b = std::clamp(b, lower, upper);
                if (b == prev[s2]) continue;
                moved = true;
                bool ob = false;
                const double fb = f(b, ob);
                if (okPrev[s2] && ob && ((fprev[s2] > 0) != (fb > 0) || fb == 0)) {
                    double lo = std::min(prev[s2], b), hi = std::max(prev[s2], b);
                    bool ol = false;
                    double flo = f(lo, ol);
                    for (int it = 0; ol && it < 220 && hi - lo > 4e-16 * std::max(1.0, std::abs(lo)); ++it) {
                        const double mid = 0.5 * (lo + hi);
                        bool om = false; const double fm = f(mid, om);
                        if (!om) break;
                        if (fm == 0) { lo = hi = mid; break; }
                        if ((fm < 0) == (flo < 0)) { lo = mid; flo = fm; } else hi = mid;
                    }
                    SolverOutcome o;
                    const int v = accept(0.5 * (lo + hi), o);
                    if (v == 2) return o;
                    if (v == 1 && !haveRetry) { best = o; haveRetry = true; }
                }
                prev[s2] = b; fprev[s2] = fb; okPrev[s2] = ob;
            }
            if (!moved) break;
        }
    }
    if (haveRetry) return best;
    if (outsideFound) throw MathError("Can't Solve!: soluzione fuori da Lower-Upper (cambia il valore iniziale o i limiti)");
    throw MathError("Time Out: il metodo di Newton non converge (cambia il valore iniziale)");
}

// ---------------------------------------------------------------- visualizzazione
namespace {
bool integerExact(const Value& v) { Rational r; return v.rational(r) && r.isInteger(); }
// Albero 2D di un valore esatto: gli interi come testo (exactValueTree e' per le
// forme non intere: 1 diventerebbe 1/1).
ast::NodePtr valueTree(const Value& v, bool mixed) {
    Rational r;
    if (v.rational(r) && r.isInteger()) { auto row = ast::make(ast::Kind::Row); appendTextTokens(*row, std::to_string(r.num)); return row; }
    return exactValueTree(v, mixed);
}
bool fitsNaturalC(const ExactComplex& e, bool mixed) { return exact::fitsNatural(e.re, mixed) && (e.im.isZero() || exact::fitsNatural(e.im, mixed)); }
bool fitsRelaxedC(const ExactComplex& e) { return exact::fitsRelaxed(e.re) && (e.im.isZero() || exact::fitsRelaxed(e.im)); }
bool allInteger(const ExactComplex& e) { return integerExact(e.re) && integerExact(e.im); }

// Forma polare esatta: modulo radicale e argomento multiplo di π/12 (o gradi /
// gradianti razionali). L'argomento si riconosce dalla tangente esatta.
bool exactPolar(const ExactComplex& e, AngleMode angle, Value& modulus, Value& argument) {
    Value r2, i2, m2;
    if (!exact::mul(e.re, e.re, r2) || !exact::mul(e.im, e.im, i2) || !exact::add(r2, i2, m2)) return false;
    if (!m2.isRational() || !exact::sqrt(m2, modulus)) return false;
    const double theta = std::atan2(e.im.toDouble(), e.re.toDouble());
    const double k = theta / kPi * 12.0;
    const double kr = std::round(k);
    if (std::abs(k - kr) > 1e-9) return false;
    const int twelfths = static_cast<int>(kr);                // -12..12
    // Tangente esatta attesa per k*15 gradi (k mod 12).
    const int km = ((twelfths % 12) + 12) % 12;
    if (km == 6) { if (!e.re.isZero()) return false; }
    else {
        if (e.re.isZero()) return false;
        Value t;
        if (!exact::div(e.im, e.re, t)) return false;
        Value expected;
        const Value three = Value::fromInteger(3);
        Value s3; if (!exact::sqrt(three, s3)) return false;
        switch (km) {
        case 0: expected = Value{}; break;
        case 3: case 9: expected = Value::fromInteger(km == 3 ? 1 : -1); break;
        case 2: case 8: case 4: case 10: {
            Value v;
            if (km == 4 || km == 10) v = s3;
            else if (!exact::div(s3, three, v)) return false;
            if (km == 8 || km == 10) { if (!exact::neg(v, v)) return false; }
            expected = v; break;
        }
        default: {   // 15°, 75° (2 ∓ √3) e i loro opposti
            Value two = Value::fromInteger(2), v;
            if (km == 1 || km == 7) { if (!exact::sub(two, s3, v)) return false; }
            else if (km == 5 || km == 11) { if (!exact::add(two, s3, v) || !exact::neg(v, v)) return false; }
            expected = v; break;
        }
        }
        if (t != expected) return false;
    }
    switch (angle) {
    case AngleMode::Radians: { Rational c; if (!Rational::make(twelfths, 12, c)) return false; argument = Value::piTimes(c); return true; }
    case AngleMode::Degrees: argument = Value::fromInteger(twelfths * 15); return true;
    case AngleMode::Gradians: { Rational c; if (!Rational::make(twelfths * 50, 3, c)) return false; argument = Value::fromRational(c); return true; }
    }
    return false;
}
double angleIn(double radians, AngleMode a) {
    switch (a) {
    case AngleMode::Degrees: return radians * 180.0 / kPi;
    case AngleMode::Gradians: return radians * 200.0 / kPi;
    default: return radians;
    }
}
bool polarView(const Number&, const View& v) { return v.complex == ComplexMode::Polar; }
std::string realText(double x, const View& v) { return formatReal(x, v.display); }
// Numeratori su denominatore comune: termini (coef * √rad) delle due parti.
struct Scaled { std::int64_t num; std::int64_t rad; };
bool commonDenominator(const ExactComplex& e, std::int64_t& lcd, std::vector<Scaled>& re, std::vector<Scaled>& im) {
    if (e.re.form() == Value::Form::Pi || e.im.form() == Value::Form::Pi) return false;
    lcd = 1;
    for (const Value* v : {&e.re, &e.im})
        for (std::size_t i = 0; i < v->termCount(); ++i) {
            const std::int64_t d = v->term(i).coef.den;
            const std::int64_t g = std::gcd(lcd, d);
            if (lcd / g > std::numeric_limits<std::int64_t>::max() / d) return false;
            lcd = lcd / g * d;
        }
    auto scale = [&](const Value& v, std::vector<Scaled>& out) {
        for (std::size_t i = 0; i < v.termCount(); ++i) {
            const exact::Term& t = v.term(i);
            const std::int64_t f = lcd / t.coef.den;
            if (t.coef.num != 0 && std::llabs(t.coef.num) > std::numeric_limits<std::int64_t>::max() / f) return false;
            out.push_back({t.coef.num * f, t.radicand});
        }
        return true;
    };
    return scale(e.re, re) && scale(e.im, im);
}
std::string termsText(const std::vector<Scaled>& terms, bool first, bool imaginary) {
    std::string out;
    for (std::size_t i = 0; i < terms.size(); ++i) {
        const Scaled& t = terms[i];
        const std::int64_t a = std::llabs(t.num);
        if (t.num < 0) out += "-"; else if (!first || i > 0) out += "+";
        const bool one = a == 1 && (t.rad != 1 || imaginary);
        if (!one) out += std::to_string(a);
        if (t.rad != 1) out += "\xE2\x88\x9A" + std::to_string(t.rad);
        if (imaginary && terms.size() == 1) out += "i";
    }
    if (imaginary && terms.size() > 1) out = (out[0] == '+' ? "+(" + out.substr(1) : "(" + out) + ")i";
    return out;
}
std::string exactComplexText(const ExactComplex& e, const View& v, bool lineStyle) {
    if (e.isReal()) return lineStyle ? exactLineText(e.re, v.mixed) : exact::toText(e.re, v.mixed);
    std::int64_t lcd = 1;
    std::vector<Scaled> re, im;
    if (commonDenominator(e, lcd, re, im)) {
        std::string top = termsText(re, true, false) + termsText(im, re.empty(), true);
        if (lcd == 1) return top;
        const std::string bar = lineStyle ? "\xE2\x8C\x9F" : "/";
        if (re.empty() && im.size() == 1) return top + bar + std::to_string(lcd);      // √6i/3
        return "(" + top + ")" + bar + std::to_string(lcd);
    }
    std::string out = e.re.isZero() ? std::string() : exact::toText(e.re, false);
    std::string imt = exact::toText(e.im, false);
    if (!out.empty() && imt[0] != '-') out += "+";
    return out + "(" + imt + ")i";
}
void appendTerms(ast::Node& row, const std::vector<Scaled>& terms, bool first, bool imaginary) {
    for (std::size_t i = 0; i < terms.size(); ++i) {
        const Scaled& t = terms[i];
        const std::int64_t a = std::llabs(t.num);
        if (t.num < 0) appendTextTokens(row, "-"); else if (!first || i > 0) appendTextTokens(row, "+");
        const bool one = a == 1 && (t.rad != 1 || imaginary);
        if (!one) appendTextTokens(row, std::to_string(a));
        if (t.rad != 1) {
            auto r = ast::make(ast::Kind::Radical, "2");
            auto inner = ast::make(ast::Kind::Row);
            appendTextTokens(*inner, std::to_string(t.rad));
            r->children = {inner};
            row.children.push_back(r);
        }
    }
    if (imaginary && !terms.empty()) appendTextTokens(row, "i");
}
ast::NodePtr exactComplexTree(const ExactComplex& e, const View& v) {
    if (e.isReal()) return valueTree(e.re, v.mixed);
    auto root = ast::make(ast::Kind::Row);
    std::int64_t lcd = 1;
    std::vector<Scaled> re, im;
    const bool common = commonDenominator(e, lcd, re, im) && im.size() == 1;
    if (common && lcd == 1) { appendTerms(*root, re, true, false); appendTerms(*root, im, re.empty(), true); return root; }
    if (common && !re.empty()) {
        // (−3+√11 i)/2 (manuale 1-29): numeratori sul denominatore comune.
        auto top = ast::make(ast::Kind::Row);
        appendTerms(*top, re, true, false);
        appendTerms(*top, im, false, true);
        auto bottom = ast::make(ast::Kind::Row);
        appendTextTokens(*bottom, std::to_string(lcd));
        auto f = ast::make(ast::Kind::Fraction);
        f->children = {top, bottom};
        root->children.push_back(f);
        return root;
    }
    // Parte reale (se c'e'), segno, modulo della parte immaginaria e i:
    // −½i (manuale 4-2), 1+(√2+√3)i, π/2+πi.
    if (!e.re.isZero()) { auto t = valueTree(e.re, false); for (auto& c : t->children) root->children.push_back(c); }
    const bool negative = e.im.sign() < 0;
    Value mag = e.im;
    if (negative) (void)exact::neg(e.im, mag);
    if (negative) appendTextTokens(*root, "-"); else if (!e.re.isZero()) appendTextTokens(*root, "+");
    const bool multi = mag.termCount() > 1;
    Rational r;
    const bool unit = mag.rational(r) && r.num == 1 && r.den == 1;
    if (multi) appendTextTokens(*root, "(");
    if (!unit) { auto t = valueTree(mag, false); for (auto& c : t->children) root->children.push_back(c); }
    if (multi) appendTextTokens(*root, ")");
    appendTextTokens(*root, "i");
    return root;
}
void appendDecimalComplex(ast::Node& row, Complex z, const View& v) {
    if (z.imag() == 0) { appendNumberTree(row, z.real(), v.display); return; }
    if (z.real() != 0) appendNumberTree(row, z.real(), v.display);
    const double im = z.imag();
    appendTextTokens(row, im < 0 ? "-" : (z.real() != 0 ? "+" : ""));
    if (std::abs(im) != 1) appendNumberTree(row, std::abs(im), v.display);
    appendTextTokens(row, "i");
}
} // namespace

bool showsExact(const Number& n, const View& v) {
    if (!n.hasExact || v.mode == 1) return false;
    if (polarView(n, v)) {
        // r∠θ: 1∠π (manuale 4-4) anche per un intero; 2∠0 resta decimale.
        Value m, a;
        if (!exactPolar(n.exact, v.angle, m, a) || (integerExact(m) && integerExact(a))) return false;
        return v.mode == 2 || (v.natural && v.fractionResults && !n.decimal && v.display.mode != DisplayMode::Eng);
    }
    if (allInteger(n.exact)) return false;
    if (v.mode == 2) return fitsRelaxedC(n.exact);
    if (!v.natural || !v.fractionResults || n.decimal || v.display.mode == DisplayMode::Eng) return false;
    return fitsNaturalC(n.exact, v.mixed);
}
bool hasOtherForm(const Number& n, const View& v) {
    if (!n.hasExact) return false;
    View probe = v; probe.mode = 2;
    return showsExact(n, probe);
}

std::string numberText(const Number& n, const View& v, bool lineStyle) {
    const ComplexMode mode = n.isReal() ? v.complex : (v.complex == ComplexMode::Real ? ComplexMode::Rectangular : v.complex);
    if (showsExact(n, v)) {
        if (polarView(n, v)) {
            Value m, a;
            (void)exactPolar(n.exact, v.angle, m, a);
            return exact::toText(m, false) + "\xE2\x88\xA0" + exact::toText(a, false);
        }
        return exactComplexText(n.exact, v, lineStyle);
    }
    const Complex z = n.value;
    if (z.imag() == 0 && !polarView(n, v)) return realText(z.real(), v);
    if (mode == ComplexMode::Polar) {
        return formatReal(std::abs(z), v.display) + "\xE2\x88\xA0" + formatReal(angleIn(std::atan2(z.imag(), z.real()), v.angle), v.display);
    }
    return formatValue(z, ComplexMode::Rectangular, v.display);
}

ast::NodePtr numberTree(const Number& n, const View& v) {
    if (!v.natural) {
        auto row = ast::make(ast::Kind::Row);
        appendTextTokens(*row, numberText(n, v, true));
        return row;
    }
    if (showsExact(n, v)) {
        if (polarView(n, v)) {
            Value m, a;
            (void)exactPolar(n.exact, v.angle, m, a);
            auto row = valueTree(m, false);
            appendTextTokens(*row, "\xE2\x88\xA0");
            auto at = valueTree(a, false);
            for (auto& c : at->children) row->children.push_back(c);
            return row;
        }
        return exactComplexTree(n.exact, v);
    }
    auto row = ast::make(ast::Kind::Row);
    const Complex z = n.value;
    if (polarView(n, v)) {
        appendNumberTree(*row, std::abs(z), v.display);
        appendTextTokens(*row, "\xE2\x88\xA0");
        appendNumberTree(*row, angleIn(std::atan2(z.imag(), z.real()), v.angle), v.display);
        return row;
    }
    appendDecimalComplex(*row, z, v);
    return row;
}

namespace {
// Coefficiente di una incognita libera nella formula: segno e modulo ("−(2+i)").
struct SignedPart { bool negative; Number magnitude; bool unit; };
SignedPart signedPart(const Number& c) {
    SignedPart s{false, c, false};
    const int sign = c.hasExact ? leadingSign(c.exact) : leadingSign(c.value);
    if (sign < 0) {
        s.negative = true;
        if (c.hasExact) { ExactComplex e; if (neg(c.exact, e)) s.magnitude = Number::fromExact(e, c.decimal); else s.magnitude = Number::fromComplex(-c.value); }
        else s.magnitude = Number::fromComplex(-c.value);
    }
    const Complex m = s.magnitude.value;
    s.unit = s.magnitude.hasExact ? (s.magnitude.exact.isReal() && s.magnitude.exact.re == Value::fromInteger(1)) : (m == Complex(1, 0));
    return s;
}
} // namespace

std::string formulaText(const SimulResult& r, int k, const char* const* names, const View& v) {
    std::string out = std::string(names[k]) + "=";
    if (r.kind != SimulResult::Kind::Infinite) return out + (r.kind == SimulResult::Kind::Unique ? numberText(r.solution[static_cast<std::size_t>(k)], v) : std::string());
    if (r.free[static_cast<std::size_t>(k)]) return out + names[k];
    const std::size_t F = r.freeVars.size(), stride = 1 + F;
    const Number& c0 = r.formula[static_cast<std::size_t>(k) * stride];
    bool any = false;
    if (!c0.isZero()) { out += numberText(c0, v); any = true; }
    for (std::size_t j = 0; j < F; ++j) {
        const Number& c = r.formula[static_cast<std::size_t>(k) * stride + 1 + j];
        if (c.isZero()) continue;
        const SignedPart s = signedPart(c);
        if (s.negative) out += "-"; else if (any) out += "+";
        if (!s.unit) {
            const std::string t = numberText(s.magnitude, v);
            out += s.magnitude.isReal() ? t : "(" + t + ")";
        }
        out += names[r.freeVars[j]];
        any = true;
    }
    if (!any) out += "0";
    return out;
}

ast::NodePtr formulaTree(const SimulResult& r, int k, const char* const* names, const View& v) {
    auto row = ast::make(ast::Kind::Row);
    appendTextTokens(*row, std::string(names[k]) + "=");
    auto append = [&](const ast::NodePtr& t) { for (auto& c : t->children) row->children.push_back(c); };
    if (r.kind == SimulResult::Kind::Unique) { append(numberTree(r.solution[static_cast<std::size_t>(k)], v)); return row; }
    if (r.kind != SimulResult::Kind::Infinite) return row;
    if (r.free[static_cast<std::size_t>(k)]) { appendTextTokens(*row, names[k]); return row; }
    const std::size_t F = r.freeVars.size(), stride = 1 + F;
    const Number& c0 = r.formula[static_cast<std::size_t>(k) * stride];
    bool any = false;
    if (!c0.isZero()) { append(numberTree(c0, v)); any = true; }
    for (std::size_t j = 0; j < F; ++j) {
        const Number& c = r.formula[static_cast<std::size_t>(k) * stride + 1 + j];
        if (c.isZero()) continue;
        const SignedPart s = signedPart(c);
        appendTextTokens(*row, s.negative ? "-" : (any ? "+" : ""));
        if (!s.unit) {
            const bool paren = !s.magnitude.isReal();
            if (paren) appendTextTokens(*row, "(");
            append(numberTree(s.magnitude, v));
            if (paren) appendTextTokens(*row, ")");
        }
        appendTextTokens(*row, names[r.freeVars[j]]);
        any = true;
    }
    if (!any) appendTextTokens(*row, "0");
    return row;
}

CalcValue toMatrix(const std::vector<Number>& values, std::size_t rows, std::size_t cols) {
    CalcValue m = CalcValue::matrix(rows, cols);
    bool exactAll = values.size() <= CalcValue::ExactElements;
    for (const Number& n : values) exactAll = exactAll && n.hasExact && n.exact.isReal() && !n.decimal;
    for (std::size_t i = 0; i < rows * cols && i < values.size(); ++i) {
        if (exactAll) m.setExact(i, values[i].exact.re);
        else m.set(i, values[i].value);
    }
    m.normalize();
    return m;
}
} // namespace fx::eq
