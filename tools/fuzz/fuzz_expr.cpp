// v1.13.1 — // Fuzzing del parser e dei valutatori (evaluateAny, evaluateValue, evaluateExact,
// evaluateExactAny) nei modi Comp, Base-N e complessi. Primo byte: modo.
#include "expression.hpp"
#include "workspace.hpp"
#include "run_matrix.hpp"
#include <cstdint>
#include <string>
using namespace fx;
static Workspace* W = nullptr;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (!W) { W = new Workspace(); W->memory.store('A', 3); W->lists[0] = {1,2,3}; }
    if (n < 1) return 0;
    const unsigned mode = d[0];
    std::string s(reinterpret_cast<const char*>(d + 1), n - 1);
    unsigned base = 0;
    switch (mode % 5) { case 1: base = 16; break; case 2: base = 2; break; case 3: base = 8; break; case 4: base = 10; break; default: break; }
    try {
        auto ast = parseExpression(s, base);
        EvalContext c = W->evalContext();
        c.complexMode = static_cast<ComplexMode>((mode >> 3) % 3);
        c.integerBase = static_cast<std::uint8_t>(base);
        if (base) c.complexMode = ComplexMode::Real;
        try { (void)evaluateAny(*ast, c); } catch (const std::exception&) {}
        if (!base) {
            try { (void)evaluateValue(*ast, c); } catch (const std::exception&) {}
            exact::Value v; bool dec = false;
            (void)evaluateExact(*ast, c, v, &dec);
            ExactCollection ec; (void)evaluateExactAny(*ast, c, ec);
        }
    } catch (const std::exception&) {}
    return 0;
}
