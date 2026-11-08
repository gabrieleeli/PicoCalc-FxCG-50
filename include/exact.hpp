#pragma once
// Exact values for Run-Matrix results, modelled on the fx-CG50 Math
// input/output mode: rationals p/q, quadratic surds with at most two terms
// (a√b/c ± d√e/f) and rational multiples of π.
//
// Every operation either produces the exact result or returns false ("no
// exact form"); the caller then keeps the floating-point value. Nothing in
// this module approximates: a value is exact or it is not represented.
// All integer arithmetic is overflow-checked (64-bit), so the module behaves
// identically on the RP2350 and on the host.
#include <cstddef>
#include <cstdint>
#include <string>

namespace fx::exact {

struct Rational {
    std::int64_t num{0};
    std::int64_t den{1};          // > 0, gcd(|num|, den) == 1
    // Normalises n/d; false on d == 0 or out-of-range values.
    static bool make(std::int64_t n, std::int64_t d, Rational& out) noexcept;
    static Rational integer(std::int64_t n) noexcept { return {n, 1}; }
    bool isZero() const noexcept { return num == 0; }
    bool isInteger() const noexcept { return den == 1; }
    int sign() const noexcept { return (num > 0) - (num < 0); }
    double toDouble() const noexcept;
    bool operator==(const Rational& o) const noexcept { return num == o.num && den == o.den; }
};
bool add(const Rational& a, const Rational& b, Rational& out) noexcept;
bool sub(const Rational& a, const Rational& b, Rational& out) noexcept;
bool mul(const Rational& a, const Rational& b, Rational& out) noexcept;
bool div(const Rational& a, const Rational& b, Rational& out) noexcept;
// -1, 0, 1 (exact, no overflow).
int compare(const Rational& a, const Rational& b) noexcept;

// coef * sqrt(radicand); radicand is square-free, 1 for a rational term.
struct Term {
    Rational coef;
    std::int64_t radicand{1};
    bool operator==(const Term& o) const noexcept { return coef == o.coef && radicand == o.radicand; }
};

class Value {
public:
    static constexpr std::size_t MaxTerms = 2;
    static constexpr std::int64_t MaxRadicand = 1000000;
    enum class Form : std::uint8_t { Surd = 0, Pi = 1 };   // Surd also covers rationals

    Value() noexcept = default;
    static Value fromRational(const Rational& r) noexcept;
    static Value fromInteger(std::int64_t n) noexcept { return fromRational(Rational::integer(n)); }
    static Value piTimes(const Rational& coef) noexcept;    // coef * pi
    // Builds a canonical surd value from up to two terms; false when it needs more.
    static bool fromTerms(const Term* terms, std::size_t count, Value& out) noexcept;

    Form form() const noexcept { return form_; }
    std::size_t termCount() const noexcept { return count_; }
    const Term& term(std::size_t i) const noexcept { return terms_[i]; }
    bool isZero() const noexcept { return count_ == 0; }
    bool isPi() const noexcept { return form_ == Form::Pi && count_ == 1; }
    // True (and r set) when the value is rational (zero included).
    bool rational(Rational& r) const noexcept;
    bool isRational() const noexcept { Rational r; return rational(r); }
    double toDouble() const noexcept;
    int sign() const noexcept;     // from the exact representation
    bool operator==(const Value& o) const noexcept;
    bool operator!=(const Value& o) const noexcept { return !(*this == o); }

private:
    Form form_{Form::Surd};
    std::uint8_t count_{0};
    Term terms_[MaxTerms]{};
};

bool add(const Value& a, const Value& b, Value& out) noexcept;
bool sub(const Value& a, const Value& b, Value& out) noexcept;
bool mul(const Value& a, const Value& b, Value& out) noexcept;
bool div(const Value& a, const Value& b, Value& out) noexcept;   // false also for b == 0
bool neg(const Value& a, Value& out) noexcept;
bool powInt(const Value& base, std::int64_t exponent, Value& out) noexcept;
// Principal square root of a non-negative rational (√12 = 2√3).
bool sqrt(const Value& a, Value& out) noexcept;
// n-th root (n >= 2) of a rational that is a perfect n-th power; odd n accepts negatives.
bool nthRoot(const Value& a, int n, Value& out) noexcept;
// a^(p/q) for rational exponents: integer powers, square roots and perfect roots.
bool powRational(const Value& base, const Rational& exponent, Value& out) noexcept;

// Decimal literal "12.375", "1.5e3" or periodic "0.1_6" (digits after '_'
// repeat forever: 0.1666... = 1/6). False for malformed or out-of-range text.
bool parseDecimal(const std::string& text, Rational& out) noexcept;
// True when the literal text has a decimal point but no repeating part.
bool isPlainDecimal(const std::string& text) noexcept;

// Decimal expansion of a rational: integer part, non-repeating digits and the
// repeating block (empty when the expansion terminates). At most maxDigits
// digits after the point are produced; truncated is set when the period is longer.
struct Expansion {
    bool negative{false};
    std::string integer;
    std::string fixed;
    std::string period;
    bool truncated{false};
};
Expansion decimalExpansion(const Rational& r, std::size_t maxDigits = 40);

// Display limits. The automatic (natural) view follows the fx-CG50 rules: a
// fraction is shown when it needs at most 10 characters (digits and fraction
// bars, mixed or improper as selected); surd terms need 1<=|a|<100, 1<b<1000,
// c<100 (rational term: |d|<100, f<100); π forms as described in the manual.
// The relaxed limits are used when the user explicitly asks for the exact form.
bool fitsNatural(const Value& v, bool mixed) noexcept;
bool fitsRelaxed(const Value& v) noexcept;
std::size_t fractionChars(const Rational& r, bool mixed) noexcept;

// Linear text of an exact value: "1/3", "2 1/3" (mixed), "2√3/3", "(3+√5)/2", "π/2", "3π".
std::string toText(const Value& v, bool mixed);
// Canonical expression accepted by the parser and by NaturalEditor::setExpression.
std::string toExpression(const Value& v);
} // namespace fx::exact
