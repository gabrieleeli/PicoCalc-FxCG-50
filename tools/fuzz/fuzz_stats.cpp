// v1.18.0 — Fuzzing del nucleo di Statistics e delle distribuzioni (solo Pico Plus 2 W):
// sezioni 0x0101 (Statistics) e 0x0117 (Distribution) del record v12 (ogni dato
// accettato si ricodifica identico), statistiche, regressioni (Sin e Logistic
// comprese, a passi), test, intervalli, ANOVA, modelli dei grafici e le 24
// funzioni di distribuzione con dati e parametri casuali: solo MathError ammessi,
// risultati finiti quando il calcolo riesce.
#include "dist_core.hpp"
#include "distribution.hpp"
#include "expression.hpp"
#include "stat_engine.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace fx;
namespace {
struct Bytes {
    const std::uint8_t* p; std::size_t n, i{0};
    std::uint8_t u8() { return i < n ? p[i++] : 0; }
    // Numero "ragionevole" (piccoli interi, decimali, a volte grandi o negativi).
    double num() {
        const std::uint8_t k = u8();
        switch (k % 8) {
        case 0: return static_cast<double>(static_cast<std::int8_t>(u8()));
        case 1: return u8() / 16.0;
        case 2: return (u8() - 128) * 0.37;
        case 3: return std::ldexp(1.0 + u8() / 256.0, static_cast<int>(u8() % 40) - 20);
        case 4: return u8() % 2 ? 1e-300 : 1e300;
        default: return u8() % 32;
        }
    }
    std::vector<double> list(std::size_t max) { std::size_t len = u8() % (max + 1); std::vector<double> v; while (len--) v.push_back(num()); return v; }
};
void finite(double v, const char* what) { if (!std::isfinite(v)) { std::fprintf(stderr, "NON FINITO %s\n", what); std::abort(); } }
template <class F> void guard(F&& f) { try { f(); } catch (const MathError&) {} }
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 1) return 0;
    Bytes b{d + 1, n - 1};
    switch (d[0] % 7) {
    case 0: {                                                       // sezione 0x0101
        stats::PlusState s;
        if (stats::decode(s, b.p, b.n)) {
            std::vector<std::uint8_t> e; stats::encode(s, e);
            stats::PlusState t;
            if (!stats::decode(t, e.data(), e.size()) || !(t == s)) { std::fprintf(stderr, "ROUNDTRIP 0x0101\n"); std::abort(); }
            for (std::uint8_t v = 0; v < stats::ResultVarCount; ++v) { double x = 0; if (s.value(v, x)) finite(x, "risultato"); }
        }
        break;
    }
    case 1: {                                                       // sezione 0x0117 e calcolo della funzione salvata
        dmode::State s;
        if (dmode::decode(s, b.p, b.n)) {
            std::vector<std::uint8_t> e; dmode::encode(s, e);
            dmode::State t;
            if (!dmode::decode(t, e.data(), e.size()) || !(t == s)) { std::fprintf(stderr, "ROUNDTRIP 0x0117\n"); std::abort(); }
            dist::Input in; in.values = dmode::paramsOf(s, s.kind); in.tail = static_cast<dist::Tail>(s.tail);
            guard([&] { const dist::Result r = dist::compute(dist::kindAt(s.kind), in); finite(r.main, "dist"); (void)dist::graphModel(dist::kindAt(s.kind), in, r); });
        }
        break;
    }
    case 2: {                                                       // statistiche e regressioni
        const auto x = b.list(40), y = b.list(40);
        std::vector<double> f; for (std::size_t i = 0; i < x.size(); ++i) f.push_back(b.u8() % 4);
        const bool useF = b.u8() % 2;
        guard([&] { const auto o = stats::oneVarEx(x, useF ? &f : nullptr, static_cast<stats::QuartileRule>(b.u8() % 2)); finite(o.mean, "media"); });
        guard([&] { const auto t = stats::twoVarEx(x, y, useF ? &f : nullptr); finite(t.sumXY, "Sxy"); });
        const auto type = static_cast<stats::Reg>(b.u8() % static_cast<int>(stats::Reg::Count));
        guard([&] {
            stats::FitTask task(type, x, y, useF ? f : std::vector<double>{});
            for (int k = 0; k < 4000; ++k) { step::Budget bud(1 + b.u8() % 50, 0, nullptr); if (task.step(bud)) break; }
            const stats::Fit& r = task.result();
            for (std::size_t i = 0; i < r.ncoef; ++i) finite(r.coef[i], "coefficiente");
            (void)stats::fitRows(r);
            guard([&] { (void)stats::residuals(r, x, y); });
            guard([&] { (void)stats::estimateX(r, b.num()); });
            (void)r.expression();
        });
        guard([&] { (void)stats::histogram(x, useF ? &f : nullptr, b.num(), b.num()); });
        guard([&] { (void)stats::boxModel(x, useF ? &f : nullptr, stats::QuartileRule::Std, true); });
        guard([&] { std::vector<double> xs, zs; stats::normalProbability(x, useF ? &f : nullptr, xs, zs); });
        guard([&] { (void)stats::piePercent(x); });
        break;
    }
    case 3: {                                                       // test e intervalli
        stats::Sample a, c;
        a.list = b.u8() % 2; c.list = b.u8() % 2;
        a.x = b.list(30); c.x = b.list(30);
        a.mean = b.num(); a.sd = b.num(); a.n = b.num(); c.mean = b.num(); c.sd = b.num(); c.n = b.num();
        const int tail = static_cast<int>(b.u8() % 3) - 1;
        const double p1 = b.num(), p2 = b.num(), p3 = b.num(), p4 = b.num();
        auto check = [](const stats::Outcome& o) { for (const auto& r : o.rows) finite(r.value, r.label.c_str()); };
        switch (b.u8() % 16) {
        case 0: guard([&] { check(stats::zTest1S(a, p1, p2, tail)); }); break;
        case 1: guard([&] { check(stats::zTest2S(a, c, p1, p2, tail)); }); break;
        case 2: guard([&] { check(stats::zTest1P(p1, p2, p3, tail)); }); break;
        case 3: guard([&] { check(stats::zTest2P(p1, p2, p3, p4, tail)); }); break;
        case 4: guard([&] { check(stats::tTest1S(a, p1, tail)); }); break;
        case 5: guard([&] { check(stats::tTest2S(a, c, p1 > 0, tail)); }); break;
        case 6: guard([&] { check(stats::tTestReg(a.x, c.x, nullptr, tail)); }); break;
        case 7: guard([&] { check(stats::chiGof(a.x, c.x, p1)); }); break;
        case 8: guard([&] { std::vector<double> e; const std::size_t r = b.u8() % 5, k = b.u8() % 5; check(stats::chi2Way(a.x, r, k, e)); }); break;
        case 9: guard([&] { check(stats::fTest2S(a, c, tail)); }); break;
        case 10: guard([&] { check(stats::zInt1S(a, p1, p2)); }); break;
        case 11: guard([&] { check(stats::zInt2S(a, c, p1, p2, p3)); }); break;
        case 12: guard([&] { check(stats::zInt1P(p1, p2, p3)); }); break;
        case 13: guard([&] { check(stats::zInt2P(p1, p2, p3, p4, b.num())); }); break;
        case 14: guard([&] { check(stats::tInt1S(a, p1)); }); break;
        default: guard([&] { check(stats::tInt2S(a, c, p1, p2 > 0)); }); break;
        }
        guard([&] { const auto o = stats::zTest1S(a, p1, 1, tail); (void)dist::testGraph(o); });
        break;
    }
    case 4: {                                                       // ANOVA a passi
        std::vector<double> fa, fb, y;
        const std::size_t len = b.u8() % 60;
        const bool two = b.u8() % 2;
        for (std::size_t i = 0; i < len; ++i) { fa.push_back(b.u8() % 4); if (two) fb.push_back(b.u8() % 3); y.push_back(b.num()); }
        guard([&] {
            stats::AnovaTask t(fa, fb, y);
            for (int k = 0; k < 1000; ++k) { step::Budget bud(1 + b.u8() % 8, 0, nullptr); if (t.step(bud)) break; }
            for (const auto& r : t.outcome().rows) finite(r.value, r.label.c_str());
        });
        break;
    }
    default: {                                                      // le 24 funzioni, singole e in lista
        const dist::Kind k = dist::kindAt(b.u8());
        dist::Input in;
        for (std::size_t i = 0; i < dist::paramSpecs(k).size(); ++i) in.values.push_back(b.num());
        in.tail = static_cast<dist::Tail>(b.u8() % 3);
        // Parametri enormi delle discrete limitati come sulla calcolatrice (calcoli fino a 1E6 termini).
        if (!dist::continuous(k.family)) for (double& v : in.values) if (std::abs(v) > 5000) v = std::fmod(v, 5000);
        guard([&] { const dist::Result r = dist::compute(k, in); finite(r.main, "dist"); const auto g = dist::graphModel(k, in, r); if (g.available) (void)dist::density(g, g.xmin); });
        guard([&] {
            const auto l1 = b.list(12), l2 = b.list(12);
            dist::ListTask t(k, in, l1, l2);
            for (int s = 0; s < 100; ++s) { step::Budget bud(1 + b.u8() % 4, 0, nullptr); if (t.step(bud)) break; }
            for (double v : t.results()) finite(v, "lista");
        });
        break;
    }
    }
    return 0;
}
