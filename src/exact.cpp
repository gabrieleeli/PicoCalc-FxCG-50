// Exact rational / surd / π arithmetic for Run-Matrix results (see exact.hpp).
// All operations are bounded loops on 64-bit integers with explicit overflow
// checks; nothing recurses.
#include "exact.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace fx::exact {
namespace {
using i64 = std::int64_t;
using u64 = std::uint64_t;
// Values are kept below 2^62 so that negation and abs never overflow.
constexpr i64 Limit = static_cast<i64>(1) << 62;
constexpr double kPi = 3.14159265358979323846;

bool inRange(i64 v) noexcept { return v > -Limit && v < Limit; }
bool mulOk(i64 a, i64 b, i64& out) noexcept { return !__builtin_mul_overflow(a, b, &out) && inRange(out); }
bool addOk(i64 a, i64 b, i64& out) noexcept { return !__builtin_add_overflow(a, b, &out) && inRange(out); }
i64 iabs(i64 v) noexcept { return v < 0 ? -v : v; }
i64 gcd(i64 a, i64 b) noexcept { a = iabs(a); b = iabs(b); while (b) { const i64 t = a % b; a = b; b = t; } return a; }
std::size_t digits(i64 v) noexcept { std::size_t n = 1; v = iabs(v); while (v >= 10) { v /= 10; ++n; } return n; }

// Exact comparison of two positive fractions n1/d1 and n2/d2 by continued fractions.
int comparePositive(i64 n1, i64 d1, i64 n2, i64 d2) noexcept {
    for (int guard = 0; guard < 200; ++guard) {
        const i64 q1 = n1 / d1, q2 = n2 / d2;
        if (q1 != q2) return q1 < q2 ? -1 : 1;
        const i64 r1 = n1 % d1, r2 = n2 % d2;
        if (r1 == 0 && r2 == 0) return 0;
        if (r1 == 0) return -1;
        if (r2 == 0) return 1;
        // r1/d1 < r2/d2  <=>  d2/r2 < d1/r1
        const i64 a = d2, b = r2, c = d1, d = r1;
        n1 = a; d1 = b; n2 = c; d2 = d;
    }
    return 0;
}
bool isSquare(u64 n, u64& root) noexcept {
    u64 r = static_cast<u64>(std::sqrt(static_cast<double>(n)));
    while (r > 0 && r * r > n) --r;
    while ((r + 1) * (r + 1) <= n) ++r;
    root = r;
    return r * r == n;
}
// n = k^2 * m with m square-free. False when n cannot be factored within the bound.
bool squareFree(u64 n, u64& k, u64& m) noexcept {
    k = 1; m = 1;
    if (n == 0) { k = 0; return true; }
    u64 d = 2;
    for (; d <= 100000 && d * d <= n; d += (d == 2 ? 1 : 2)) {
        int e = 0;
        while (n % d == 0) { n /= d; ++e; }
        for (int i = 0; i < e / 2; ++i) k *= d;
        if (e % 2) m *= d;
        if (m > static_cast<u64>(Value::MaxRadicand)) return false;
    }
    if (n > 1) {
        if (d * d > n) { m *= n; }                        // the cofactor is prime
        else { u64 r = 0; if (!isSquare(n, r)) return false; k *= r; }
    }
    return m <= static_cast<u64>(Value::MaxRadicand);
}
bool intRoot(i64 value, int n, i64& root) noexcept {
    if (value < 0) return false;
    const double est = std::round(std::pow(static_cast<double>(value), 1.0 / n));
    for (i64 c = static_cast<i64>(est) - 1; c <= static_cast<i64>(est) + 1; ++c) {
        if (c < 0) continue;
        i64 p = 1; bool ok = true;
        for (int i = 0; i < n && ok; ++i) ok = mulOk(p, c, p);
        if (ok && p == value) { root = c; return true; }
    }
    return false;
}
std::string sqrtText(i64 r) { return "\xE2\x88\x9A" + std::to_string(r); }   // U+221A
} // namespace

// ---------------------------------------------------------------- rationals
bool Rational::make(i64 n, i64 d, Rational& out) noexcept {
    if (d == 0 || !inRange(n) || !inRange(d)) return false;
    if (d < 0) { n = -n; d = -d; }
    const i64 g = gcd(n, d);
    out.num = g ? n / g : 0; out.den = g ? d / g : 1;
    if (out.num == 0) out.den = 1;
    return true;
}
double Rational::toDouble() const noexcept { return static_cast<double>(num) / static_cast<double>(den); }
bool add(const Rational& a, const Rational& b, Rational& out) noexcept {
    const i64 g = gcd(a.den, b.den);
    i64 den = 0, x = 0, y = 0, num = 0;
    if (!mulOk(a.den / g, b.den, den) || !mulOk(a.num, b.den / g, x) || !mulOk(b.num, a.den / g, y) || !addOk(x, y, num)) return false;
    return Rational::make(num, den, out);
}
bool sub(const Rational& a, const Rational& b, Rational& out) noexcept { return add(a, {-b.num, b.den}, out); }
bool mul(const Rational& a, const Rational& b, Rational& out) noexcept {
    const i64 g1 = gcd(a.num, b.den), g2 = gcd(b.num, a.den);
    const i64 an = g1 ? a.num / g1 : 0, bd = g1 ? b.den / g1 : b.den, bn = g2 ? b.num / g2 : 0, ad = g2 ? a.den / g2 : a.den;
    i64 num = 0, den = 0;
    if (!mulOk(an, bn, num) || !mulOk(ad, bd, den)) return false;
    return Rational::make(num, den, out);
}
bool div(const Rational& a, const Rational& b, Rational& out) noexcept {
    if (b.num == 0) return false;
    return mul(a, {b.num < 0 ? -b.den : b.den, iabs(b.num)}, out);
}
int compare(const Rational& a, const Rational& b) noexcept {
    const int sa = a.sign(), sb = b.sign();
    if (sa != sb) return sa < sb ? -1 : 1;
    if (sa == 0) return 0;
    const int c = comparePositive(iabs(a.num), a.den, iabs(b.num), b.den);
    return sa > 0 ? c : -c;
}

// ---------------------------------------------------------------- values
Value Value::fromRational(const Rational& r) noexcept {
    Value v;
    if (r.num != 0) { v.count_ = 1; v.terms_[0] = {r, 1}; }
    return v;
}
Value Value::piTimes(const Rational& coef) noexcept {
    Value v;
    if (coef.num != 0) { v.form_ = Form::Pi; v.count_ = 1; v.terms_[0] = {coef, 1}; }
    return v;
}
bool Value::fromTerms(const Term* in, std::size_t count, Value& out) noexcept {
    Term merged[4]{};
    std::size_t n = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (in[i].coef.num == 0) continue;
        std::size_t j = 0;
        while (j < n && merged[j].radicand != in[i].radicand) ++j;
        if (j == n) { if (n == 4) return false; merged[n++] = in[i]; }
        else if (!add(merged[j].coef, in[i].coef, merged[j].coef)) return false;
    }
    Value v;
    for (std::size_t i = 0; i < n; ++i) {
        if (merged[i].coef.num == 0) continue;
        if (v.count_ == MaxTerms) return false;
        v.terms_[v.count_++] = merged[i];
    }
    if (v.count_ == 2 && v.terms_[0].radicand > v.terms_[1].radicand) std::swap(v.terms_[0], v.terms_[1]);
    out = v;
    return true;
}
bool Value::rational(Rational& r) const noexcept {
    if (count_ == 0) { r = {}; return true; }
    if (form_ == Form::Surd && count_ == 1 && terms_[0].radicand == 1) { r = terms_[0].coef; return true; }
    return false;
}
double Value::toDouble() const noexcept {
    if (form_ == Form::Pi) return count_ ? terms_[0].coef.toDouble() * kPi : 0.0;
    double s = 0;
    for (std::size_t i = 0; i < count_; ++i) s += terms_[i].coef.toDouble() * std::sqrt(static_cast<double>(terms_[i].radicand));
    return s;
}
int Value::sign() const noexcept {
    if (count_ == 0) return 0;
    if (count_ == 1) return terms_[0].coef.sign();
    const int s0 = terms_[0].coef.sign(), s1 = terms_[1].coef.sign();
    if (s0 == s1) return s0;
    // Opposite signs: compare the squares c^2 * r, both rational.
    Rational a, b;
    if (mul(terms_[0].coef, terms_[0].coef, a) && mul(a, Rational::integer(terms_[0].radicand), a) &&
        mul(terms_[1].coef, terms_[1].coef, b) && mul(b, Rational::integer(terms_[1].radicand), b)) {
        const int c = compare(a, b);
        return c > 0 ? s0 : c < 0 ? s1 : 0;
    }
    const double d = toDouble();
    return (d > 0) - (d < 0);
}
bool Value::operator==(const Value& o) const noexcept {
    if (count_ != o.count_) return false;
    if (count_ == 0) return true;
    if (form_ != o.form_) return false;
    for (std::size_t i = 0; i < count_; ++i) if (!(terms_[i] == o.terms_[i])) return false;
    return true;
}

bool neg(const Value& a, Value& out) noexcept {
    Value v = a;
    Term t[Value::MaxTerms]{};
    for (std::size_t i = 0; i < a.termCount(); ++i) { t[i] = a.term(i); t[i].coef.num = -t[i].coef.num; }
    if (a.form() == Value::Form::Pi) { out = Value::piTimes(t[0].coef); return true; }
    if (!Value::fromTerms(t, a.termCount(), v)) return false;
    out = v;
    return true;
}
bool add(const Value& a, const Value& b, Value& out) noexcept {
    if (a.isZero()) { out = b; return true; }
    if (b.isZero()) { out = a; return true; }
    if (a.form() == Value::Form::Pi || b.form() == Value::Form::Pi) {
        if (a.form() != b.form()) return false;               // π + rational has no exact form here
        Rational c;
        if (!add(a.term(0).coef, b.term(0).coef, c)) return false;
        out = Value::piTimes(c);
        return true;
    }
    Term t[4]{};
    std::size_t n = 0;
    for (std::size_t i = 0; i < a.termCount(); ++i) t[n++] = a.term(i);
    for (std::size_t i = 0; i < b.termCount(); ++i) t[n++] = b.term(i);
    return Value::fromTerms(t, n, out);
}
bool sub(const Value& a, const Value& b, Value& out) noexcept {
    Value nb;
    return neg(b, nb) && add(a, nb, out);
}
namespace {
bool mulTerm(const Term& x, const Term& y, Term& out) noexcept {
    const i64 g = gcd(x.radicand, y.radicand);
    i64 rad = 0;
    if (!mulOk(x.radicand / g, y.radicand / g, rad) || rad > Value::MaxRadicand) return false;
    Rational c;
    if (!mul(x.coef, y.coef, c) || !mul(c, Rational::integer(g), c)) return false;
    out = {c, rad};
    return true;
}
} // namespace
bool mul(const Value& a, const Value& b, Value& out) noexcept {
    if (a.isZero() || b.isZero()) { out = Value{}; return true; }
    const bool pa = a.form() == Value::Form::Pi, pb = b.form() == Value::Form::Pi;
    if (pa || pb) {
        if (pa && pb) return false;                           // π² is not represented
        const Value& p = pa ? a : b;
        Rational r, c;
        if (!(pa ? b : a).rational(r) || !mul(p.term(0).coef, r, c)) return false;
        out = Value::piTimes(c);
        return true;
    }
    Term t[4]{};
    std::size_t n = 0;
    for (std::size_t i = 0; i < a.termCount(); ++i)
        for (std::size_t j = 0; j < b.termCount(); ++j)
            if (!mulTerm(a.term(i), b.term(j), t[n++])) return false;
    return Value::fromTerms(t, n, out);
}
bool div(const Value& a, const Value& b, Value& out) noexcept {
    if (b.isZero()) return false;
    if (a.isZero()) { out = Value{}; return true; }
    const bool pa = a.form() == Value::Form::Pi, pb = b.form() == Value::Form::Pi;
    if (pb) {
        if (!pa) return false;                                 // 1/π
        Rational c;
        if (!div(a.term(0).coef, b.term(0).coef, c)) return false;
        out = Value::fromRational(c);
        return true;
    }
    if (pa) {
        Rational r, c;
        if (!b.rational(r) || !div(a.term(0).coef, r, c)) return false;
        out = Value::piTimes(c);
        return true;
    }
    // Rationalise the denominator: 1/(c√r) = √r/(c·r); 1/(t1+t2) = (t1-t2)/(t1²-t2²).
    Value inverse;
    if (b.termCount() == 1) {
        const Term& t = b.term(0);
        Rational c;
        if (!mul(t.coef, Rational::integer(t.radicand), c) || !div(Rational::integer(1), c, c)) return false;
        const Term inv{c, t.radicand};
        if (!Value::fromTerms(&inv, 1, inverse)) return false;
    } else {
        const Term& t1 = b.term(0); const Term& t2 = b.term(1);
        Rational s1, s2, d;
        if (!mul(t1.coef, t1.coef, s1) || !mul(s1, Rational::integer(t1.radicand), s1) ||
            !mul(t2.coef, t2.coef, s2) || !mul(s2, Rational::integer(t2.radicand), s2) || !sub(s1, s2, d) || d.isZero()) return false;
        Term conj[2] = {t1, t2};
        conj[1].coef.num = -conj[1].coef.num;
        if (!div(conj[0].coef, d, conj[0].coef) || !div(conj[1].coef, d, conj[1].coef)) return false;
        if (!Value::fromTerms(conj, 2, inverse)) return false;
    }
    return mul(a, inverse, out);
}
bool powInt(const Value& base, std::int64_t e, Value& out) noexcept {
    if (e == 0) { out = Value::fromInteger(1); return true; }
    if (base.isZero()) { if (e < 0) return false; out = Value{}; return true; }
    if (e > (1 << 20) || e < -(1 << 20)) return false;
    if (base.form() == Value::Form::Pi && e != 1) return false;
    Value result = Value::fromInteger(1), sq = base;
    std::int64_t n = e < 0 ? -e : e;
    while (n > 0) {
        if (n & 1) { if (!mul(result, sq, result)) return false; }
        n >>= 1;
        if (n > 0 && !mul(sq, sq, sq)) return false;
    }
    if (e < 0) return div(Value::fromInteger(1), result, out);
    out = result;
    return true;
}
bool sqrt(const Value& a, Value& out) noexcept {
    Rational r;
    if (!a.rational(r) || r.num < 0) return false;
    if (r.num == 0) { out = Value{}; return true; }
    // √(p/q) = √(p·q)/q
    i64 pq = 0;
    if (!mulOk(r.num, r.den, pq)) return false;
    u64 k = 0, m = 0;
    if (!squareFree(static_cast<u64>(pq), k, m)) return false;
    Rational c;
    if (!Rational::make(static_cast<i64>(k), r.den, c)) return false;
    const Term t{c, static_cast<i64>(m)};
    return Value::fromTerms(&t, 1, out);
}
bool nthRoot(const Value& a, int n, Value& out) noexcept {
    if (n == 2) return sqrt(a, out);
    Rational r;
    if (n < 2 || !a.rational(r)) return false;
    if (r.num < 0 && n % 2 == 0) return false;
    i64 p = 0, q = 0;
    if (!intRoot(iabs(r.num), n, p) || !intRoot(r.den, n, q)) return false;
    Rational res;
    if (!Rational::make(r.num < 0 ? -p : p, q, res)) return false;
    out = Value::fromRational(res);
    return true;
}
bool powRational(const Value& base, const Rational& e, Value& out) noexcept {
    if (e.isInteger()) return powInt(base, e.num, out);
    if (e.den > 99) return false;
    Value root;
    if (!nthRoot(base, static_cast<int>(e.den), root)) return false;
    return powInt(root, e.num, out);
}

// ---------------------------------------------------------------- literals
bool isPlainDecimal(const std::string& t) noexcept {
    return t.find('.') != std::string::npos && t.find('_') == std::string::npos && t.compare(0, 2, "0x") != 0;
}
bool parseDecimal(const std::string& t, Rational& out) noexcept {
    std::size_t i = 0;
    i64 mant = 0; int scale = 0; std::size_t sig = 0;
    auto digit = [&](char c) -> bool {
        if (mant == 0 && c == '0') return true;               // leading zeros do not count
        if (++sig > 18) return false;
        return mulOk(mant, 10, mant) && addOk(mant, c - '0', mant);
    };
    bool any = false;
    while (i < t.size() && t[i] >= '0' && t[i] <= '9') { if (!digit(t[i])) return false; ++i; any = true; }
    std::string period;
    if (i < t.size() && t[i] == '.') {
        ++i;
        while (i < t.size() && t[i] >= '0' && t[i] <= '9') { if (!digit(t[i])) return false; ++scale; ++i; any = true; }
        if (i < t.size() && t[i] == '_') {
            ++i;
            while (i < t.size() && t[i] >= '0' && t[i] <= '9') period.push_back(t[i++]);
            if (period.empty() || period.size() > 15) return false;
        }
    }
    if (!any && period.empty()) return false;
    int exponent = 0;
    if (i < t.size() && (t[i] == 'e' || t[i] == 'E')) {
        ++i;
        bool negExp = false;
        if (i < t.size() && (t[i] == '+' || t[i] == '-')) negExp = t[i++] == '-';
        if (i >= t.size()) return false;
        while (i < t.size() && t[i] >= '0' && t[i] <= '9') { exponent = exponent * 10 + (t[i++] - '0'); if (exponent > 400) return false; }
        if (negExp) exponent = -exponent;
    }
    if (i != t.size()) return false;
    // value = mant / 10^scale + period / (10^scale * (10^len - 1))
    i64 pow10 = 1;
    for (int k = 0; k < scale; ++k) if (!mulOk(pow10, 10, pow10)) return false;
    Rational v;
    if (!Rational::make(mant, pow10, v)) return false;
    if (!period.empty()) {
        i64 rep = 0, nines = 0;
        for (char c : period) { if (!mulOk(rep, 10, rep) || !addOk(rep, c - '0', rep) || !mulOk(nines, 10, nines) || !addOk(nines, 9, nines)) return false; }
        i64 den = 0;
        Rational p;
        if (!mulOk(pow10, nines, den) || !Rational::make(rep, den, p) || !add(v, p, v)) return false;
    }
    for (int k = 0; k < (exponent < 0 ? -exponent : exponent); ++k)
        if (!(exponent < 0 ? div(v, Rational::integer(10), v) : mul(v, Rational::integer(10), v))) return false;
    out = v;
    return true;
}

Expansion decimalExpansion(const Rational& r, std::size_t maxDigits) {
    Expansion e;
    e.negative = r.num < 0;
    const i64 n = iabs(r.num), d = r.den;
    e.integer = std::to_string(n / d);
    i64 rem = n % d;
    if (rem == 0) return e;
    std::string digitsOut;
    i64 seen[64]{};
    const std::size_t limit = std::min<std::size_t>(maxDigits, 64);
    for (std::size_t pos = 0; pos < limit && rem != 0; ++pos) {
        for (std::size_t j = 0; j < pos; ++j) {
            if (seen[j] == rem) { e.fixed = digitsOut.substr(0, j); e.period = digitsOut.substr(j); return e; }
        }
        seen[pos] = rem;
        i64 x = 0;
        if (!mulOk(rem, 10, x)) { e.fixed = digitsOut; e.truncated = true; return e; }
        digitsOut.push_back(static_cast<char>('0' + x / d));
        rem = x % d;
    }
    e.fixed = digitsOut;
    e.truncated = rem != 0;
    return e;
}

// ---------------------------------------------------------------- display rules
std::size_t fractionChars(const Rational& r, bool mixed) noexcept {
    if (r.isInteger()) return digits(r.num);
    const i64 n = iabs(r.num);
    if (mixed && n > r.den) return digits(n / r.den) + 1 + digits(n % r.den) + 1 + digits(r.den);
    return digits(n) + 1 + digits(r.den);
}
bool fitsNatural(const Value& v, bool mixed) noexcept {
    if (v.isZero()) return true;
    if (v.form() == Value::Form::Pi) {
        const Rational& c = v.term(0).coef;
        if (c.isInteger()) return iabs(c.num) < 1000000;
        const i64 b = iabs(c.num);
        const std::size_t count = b > c.den ? digits(b / c.den) + digits(b % c.den) + digits(c.den) : digits(b) + digits(c.den);
        return count <= 8 && digits(c.den) <= 3;
    }
    Rational r;
    if (v.rational(r)) return fractionChars(r, mixed) <= 10;
    for (std::size_t i = 0; i < v.termCount(); ++i) {
        const Term& t = v.term(i);
        if (iabs(t.coef.num) >= 100 || t.coef.den >= 100) return false;
        if (t.radicand > 1 && t.radicand >= 1000) return false;
    }
    return true;
}
bool fitsRelaxed(const Value& v) noexcept {
    for (std::size_t i = 0; i < v.termCount(); ++i) {
        const Term& t = v.term(i);
        const i64 lim = v.termCount() == 1 && t.radicand == 1 ? 1000000000000LL : 10000000LL;
        if (iabs(t.coef.num) >= lim || t.coef.den >= lim) return false;
    }
    return true;
}

namespace {
std::string mixedText(const Rational& r) {
    const i64 n = iabs(r.num);
    std::string s = r.num < 0 ? "-" : "";
    if (n > r.den) return s + std::to_string(n / r.den) + " " + std::to_string(n % r.den) + "/" + std::to_string(r.den);
    return s + std::to_string(n) + "/" + std::to_string(r.den);
}
// Numerators over the least common denominator of the terms.
bool commonDenominator(const Value& v, i64& lcd, i64 nums[Value::MaxTerms]) noexcept {
    lcd = 1;
    for (std::size_t i = 0; i < v.termCount(); ++i) {
        const i64 d = v.term(i).coef.den;
        if (!mulOk(lcd / gcd(lcd, d), d, lcd)) return false;
    }
    for (std::size_t i = 0; i < v.termCount(); ++i)
        if (!mulOk(v.term(i).coef.num, lcd / v.term(i).coef.den, nums[i])) return false;
    return true;
}
std::string termText(i64 num, i64 rad, bool first) {
    std::string s;
    if (num < 0) s = "-"; else if (!first) s = "+";
    const i64 a = iabs(num);
    if (rad == 1) return s + std::to_string(a);
    if (a != 1) s += std::to_string(a);
    return s + sqrtText(rad);
}
} // namespace

std::string toText(const Value& v, bool mixed) {
    if (v.isZero()) return "0";
    if (v.form() == Value::Form::Pi) {
        const Rational& c = v.term(0).coef;
        const i64 a = iabs(c.num);
        std::string s = c.num < 0 ? "-" : "";
        s += (a == 1 ? std::string() : std::to_string(a)) + "\xCF\x80";
        if (!c.isInteger()) s += "/" + std::to_string(c.den);
        return s;
    }
    Rational r;
    if (v.rational(r)) {
        if (r.isInteger()) return std::to_string(r.num);
        return mixed ? mixedText(r) : std::to_string(r.num) + "/" + std::to_string(r.den);
    }
    i64 lcd = 1, nums[Value::MaxTerms]{};
    if (!commonDenominator(v, lcd, nums)) return "?";
    std::string top;
    for (std::size_t i = 0; i < v.termCount(); ++i) top += termText(nums[i], v.term(i).radicand, i == 0);
    if (lcd == 1) return top;
    if (v.termCount() > 1) top = "(" + top + ")";
    return top + "/" + std::to_string(lcd);
}

std::string toExpression(const Value& v) {
    if (v.isZero()) return "0";
    if (v.form() == Value::Form::Pi) {
        const Rational& c = v.term(0).coef;
        const i64 a = iabs(c.num);
        const std::string top = (a == 1 ? std::string() : std::to_string(a)) + "pi";
        const std::string sign = c.num < 0 ? "-" : "";
        if (c.isInteger()) return sign + top;
        return sign + "(" + top + ")/(" + std::to_string(c.den) + ")";
    }
    Rational r;
    if (v.rational(r)) {
        if (r.isInteger()) return std::to_string(r.num);
        return std::string(r.num < 0 ? "-" : "") + "(" + std::to_string(iabs(r.num)) + ")/(" + std::to_string(r.den) + ")";
    }
    i64 lcd = 1, nums[Value::MaxTerms]{};
    if (!commonDenominator(v, lcd, nums)) return std::to_string(v.toDouble());
    std::string top;
    for (std::size_t i = 0; i < v.termCount(); ++i) {
        const i64 a = iabs(nums[i]), rad = v.term(i).radicand;
        top += nums[i] < 0 ? "-" : (i ? "+" : "");
        if (rad == 1) top += std::to_string(a);
        else top += (a == 1 ? std::string() : std::to_string(a)) + "sqrt(" + std::to_string(rad) + ")";
    }
    if (lcd == 1) return top;
    return "(" + top + ")/(" + std::to_string(lcd) + ")";
}
} // namespace fx::exact
