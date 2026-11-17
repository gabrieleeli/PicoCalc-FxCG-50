// v1.13.1 — // Confronto tra valutazione esatta e decimale: segnala (DIFF / EXACT-ONLY) le
// espressioni con forma esatta diversa dal valore decimale; ABORT_DIFF=1 si ferma.
// Le differenze dovute agli arrotondamenti del calcolo decimale (10^16+1-10^16)
// vanno valutate a mano.
#include "expression.hpp"
#include "workspace.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace fx;
static Workspace* W = nullptr;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (!W) { W = new Workspace(); W->memory.store('A', 3); W->memory.store('B', 0.5); }
    if (n < 1) return 0;
    std::string s(reinterpret_cast<const char*>(d + 1), n - 1);
    W->settings.angle = static_cast<AngleMode>(d[0] % 3); W->apply();
    try {
        auto ast = parseExpression(s);
        EvalContext c = W->evalContext();
        c.complexMode = (d[0] & 8) ? ComplexMode::Rectangular : ComplexMode::Real;
        exact::Value v; bool dec = false;
        if (!evaluateExact(*ast, c, v, &dec)) return 0;
        Complex z;
        try { z = evaluateValue(*ast, c); } catch (const std::exception& e) {
            std::fprintf(stderr, "EXACT-ONLY: [%s] exact=%.17g float threw %s\n", s.c_str(), v.toDouble(), e.what());
            if (std::getenv("ABORT_DIFF")) std::abort();
            return 0;
        }
        const double x = v.toDouble();
        if (!std::isfinite(z.real()) || !std::isfinite(x)) return 0;
        const double tol = 1e-9 * std::max({1.0, std::abs(x), std::abs(z.real())});
        if (std::abs(z.imag()) > tol || std::abs(z.real() - x) > tol) {
            std::fprintf(stderr, "DIFF: [%s] exact=%.17g float=%.17g%+.17gi\n", s.c_str(), x, z.real(), z.imag());
            if (std::getenv("ABORT_DIFF")) std::abort();
        }
    } catch (const std::exception&) {}
    return 0;
}
