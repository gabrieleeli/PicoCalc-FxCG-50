// v1.13.1 — // Fuzzing di EQUATION: SIML e POLY con coefficienti dai byte (interi, frazioni,
// complessi decimali), visualizzazione dei risultati; SOLVER su testo.
#include "equation.hpp"
#include "workspace.hpp"
#include <cstdint>
#include <cstring>
#include <cmath>
using namespace fx;
static Workspace* W = nullptr;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (!W) W = new Workspace();
    if (n < 2) return 0;
    const int kind = d[0] % 3;
    const uint8_t sel = d[1];
    d += 2; n -= 2;
    // Coefficients: small integers / fractions / decimals from bytes
    std::vector<eq::Number> c;
    auto next = [&](std::size_t i) -> eq::Number {
        if (i * 3 + 2 >= n) return eq::Number::fromRational(exact::Rational{0, 1});
        const int8_t a = static_cast<int8_t>(d[i * 3]); const uint8_t b = d[i * 3 + 1]; const uint8_t k = d[i * 3 + 2];
        switch (k % 4) {
        case 0: return eq::Number::fromRational(exact::Rational{a, 1});
        case 1: return eq::Number::fromRational(exact::Rational{a, (b % 9) + 1});
        case 2: return eq::Number::fromComplex({a / 7.0, (b % 3) ? 0.0 : b / 13.0});
        default: { exact::Rational r; exact::Rational::make(a, (b % 5) + 1, r); return eq::Number::fromRational(r, true); }
        }
    };
    eq::View view; view.complex = static_cast<ComplexMode>(sel % 3); view.mixed = sel & 8; view.natural = sel & 16; view.mode = (sel >> 5) % 3;
    try {
        if (kind == 0) {
            const int u = 2 + (sel % 5);
            for (int i = 0; i < u * (u + 1); ++i) c.push_back(next(i));
            auto r = eq::solveSimultaneous(c, u);
            for (auto& s : r.solution) { (void)eq::numberText(s, view); (void)eq::numberTree(s, view); }
            const char* names[] = {"X","Y","Z","T","U","V"};
            if (r.kind == eq::SimulResult::Kind::Infinite) for (int k = 0; k < u; ++k) { (void)eq::formulaText(r, k, names, view); (void)eq::formulaTree(r, k, names, view); }
        } else if (kind == 1) {
            const int g = 2 + (sel % 5);
            for (int i = 0; i <= g; ++i) c.push_back(next(i));
            auto roots = eq::solvePolynomial(c);
            for (auto& r : eq::visibleRoots(roots, view.complex)) { (void)eq::numberText(r.value, view); (void)eq::numberTree(r.value, view); (void)eq::editText(r.value); }
        } else {
            std::string s(reinterpret_cast<const char*>(d), n);
            auto vars = eq::solverVariables(s);
            if (!vars.empty()) {
                EvalContext ctx = W->evalContext();
                (void)eq::solveFor(s, vars[0], -1e10, 1e10, ctx);
            }
        }
    } catch (const std::exception&) {}
    return 0;
}
