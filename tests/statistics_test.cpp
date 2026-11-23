// STATISTICS numerics against textbook reference values.
#include "statistics.hpp"
#include "math_engine.hpp"
#include "test_util.hpp"
#include <cmath>
#include <limits>
#if FXAI_BOARD_PICO_PLUS_2W
#include "dist_core.hpp"
#include "expression.hpp"
#include "memory.hpp"
#include "stat_engine.hpp"
#include <cstring>
#include <string>
#include <vector>
#endif

using namespace fx::stats;
#if FXAI_BOARD_PICO_PLUS_2W
// v1.18.0 (solo Plus): nucleo di Statistics e di Distribution contro i riferimenti
// indipendenti di scipy/numpy/statsmodels (tools/stat_reference.py), con le
// tolleranze dichiarate in docs/reports/ANALISI_FONTI_v1.18.0.md §6.
namespace ref {
struct RefValue { const char* key; double value; };
#include "fixtures/stat_reference.inc"
} // namespace ref
namespace {
double R(const std::string& key) {
    for (const auto& r : ref::kRefs) if (key == r.key) return r.value;
    ::testutil::report(false, "chiave di riferimento", __FILE__, __LINE__, key);
    return std::numeric_limits<double>::quiet_NaN();
}
// |got - ref| <= max(rel*|ref|, abs)
void near(const std::string& key, double got, double rel, double abs = 1e-14) {
    const double want = R(key);
    const bool ok = std::abs(got - want) <= std::max(rel * std::abs(want), abs);
    char extra[160];
    std::snprintf(extra, sizeof extra, "%s: %.17g vs %.17g", key.c_str(), got, want);
    ::testutil::report(ok, "valore contro scipy", __FILE__, __LINE__, extra);
}
constexpr double kDesc = 1e-12, kReg = 1e-9, kIterCoef = 1e-6, kCont = 1e-9, kDisc = 1e-10, kInv = 1e-8, kAnova = 1e-9;
void oneVarCase(const std::string& p, const std::vector<double>& x, const std::vector<double>* f) {
    for (QuartileRule rule : {QuartileRule::Std, QuartileRule::OnData}) {
        const OneVar o = oneVarEx(x, f, rule);
        near(p + ".n", o.n, 0, 0); near(p + ".mean", o.mean, kDesc); near(p + ".sum", o.sum, kDesc); near(p + ".sumsq", o.sumSq, kDesc);
        near(p + ".popsd", o.popSd, kDesc);
        if (o.n > 1) near(p + ".sd", o.sampleSd, kDesc);
        near(p + ".min", o.min, 0, 0); near(p + ".max", o.max, 0, 0); near(p + ".med", o.median, kDesc);
        near(p + (rule == QuartileRule::Std ? ".q1std" : ".q1ondata"), o.q1, kDesc);
        near(p + (rule == QuartileRule::Std ? ".q3std" : ".q3ondata"), o.q3, kDesc);
        near(p + ".modcount", o.modeCount, 0, 0); near(p + ".modn", static_cast<double>(o.modes.size()), 0, 0);
        near(p + ".mod", o.modes.front(), 0, 0);
    }
}
void plusEngine() {
    using namespace ref;
    // ---- C03 1-VAR (con e senza frequenze), G15 Q1Q3 Type Std/OnData
    oneVarCase("desc.ds1", ds1_x, nullptr);
    oneVarCase("desc.ds1f", ds1_x, &ds1_f);
    oneVarCase("desc.qa", q_a, nullptr); oneVarCase("desc.qb", q_b, nullptr); oneVarCase("desc.qc", q_c, nullptr);
    oneVarCase("desc.qd", q_d, nullptr); oneVarCase("desc.qe", q_e, nullptr); oneVarCase("desc.qef", q_e, &q_e_f);
    oneVarCase("desc.qg", q_g, nullptr); oneVarCase("desc.ds3", ds3_x, nullptr);
    // La v1.16.0 (Std) resta identica: stesso risultato di oneVariable.
    { const OneVar a = oneVariable(ds1_x, &ds1_f), b = oneVarEx(ds1_x, &ds1_f, QuartileRule::Std); CHECK(a.q1 == b.q1 && a.q3 == b.q3 && a.mean == b.mean); }
    CHECK_THROWS_AS(oneVarEx(ds1_x, &q_b, QuartileRule::Std), fx::DimensionError);       // frequenze di lunghezza diversa
    { std::vector<double> bad = ds1_f; bad[0] = 1.5; CHECK_THROWS_AS(oneVarEx(ds1_x, &bad, QuartileRule::Std), fx::MathError); }
    // ---- C04 2-VAR con le frequenze
    for (const char* p : {"desc2.ds2", "desc2.ds2f"}) {
        const bool f = std::strcmp(p, "desc2.ds2f") == 0;
        const TwoVar t = twoVarEx(ds2_x, ds2_y, f ? &ds2_f : nullptr);
        const std::string k = p;
        near(k + ".n", t.n, 0, 0); near(k + ".meanx", t.meanX, kDesc); near(k + ".meany", t.meanY, kDesc); near(k + ".sumx", t.sumX, kDesc);
        near(k + ".sumy", t.sumY, kDesc); near(k + ".sumx2", t.sumX2, kDesc); near(k + ".sumy2", t.sumY2, kDesc); near(k + ".sumxy", t.sumXY, kDesc);
        near(k + ".popsdx", t.popSdX, kDesc); near(k + ".popsdy", t.popSdY, kDesc); near(k + ".sdx", t.sampleSdX, kDesc); near(k + ".sdy", t.sampleSdY, kDesc);
    }
    // ---- R01-R10, R18: regressioni in forma chiusa, con e senza frequenze
    for (const std::string sfx : {"", "f"}) {
        const std::vector<double>* f = sfx.empty() ? nullptr : &ds2_f;
        auto coef = [&](const std::string& p, const Fit& r, const char* names) { for (std::size_t i = 0; names[i]; ++i) near(p + "." + names[i], r.coef[i], kReg, 1e-12); };
        Fit r = fit(Reg::LinearAx, ds2_x, ds2_y, f);
        coef("reg.ax" + sfx, r, "ab"); near("reg.ax" + sfx + ".r", r.r, kReg); near("reg.ax" + sfx + ".r2", r.r2, kReg); near("reg.ax" + sfx + ".mse", r.mse, kReg);
        r = fit(Reg::LinearA, ds2_x, ds2_y, f);
        coef("reg.a+bx" + sfx, r, "ab"); near("reg.a+bx" + sfx + ".r", r.r, kReg); near("reg.a+bx" + sfx + ".mse", r.mse, kReg);
        r = fit(Reg::MedMed, ds2_x, ds2_y, f);
        coef("reg.med" + sfx, r, "ab");
        CHECK(r.hasPts);
        for (int i = 0; i < 3; ++i) { near("reg.med" + sfx + ".x" + std::to_string(i + 1), r.pts[2 * i], kReg); near("reg.med" + sfx + ".y" + std::to_string(i + 1), r.pts[2 * i + 1], kReg); }
        const char* polys[] = {"quad", "cubic", "quart"};
        const Reg ptypes[] = {Reg::Quad, Reg::Cubic, Reg::Quart};
        for (int k = 0; k < 3; ++k) {
            r = fit(ptypes[k], ds2_x, ds4_y, f);
            const std::string p = std::string("reg.") + polys[k] + sfx;
            coef(p, r, k == 0 ? "abc" : k == 1 ? "abcd" : "abcde");
            near(p + ".r2", r.r2, kReg); near(p + ".mse", r.mse, kReg);
            CHECK(!r.hasR && r.hasR2);
        }
        const char* tr[] = {"log", "exp", "abexp", "power"};
        const Reg ttypes[] = {Reg::Log, Reg::ExpE, Reg::ExpAb, Reg::Power};
        for (int k = 0; k < 4; ++k) {
            r = fit(ttypes[k], ds2_x, k == 0 ? ds2_y : ds4_y, f);
            const std::string p = std::string("reg.") + tr[k] + sfx;
            coef(p, r, "ab"); near(p + ".r", r.r, kReg); near(p + ".r2", r.r2, kReg); near(p + ".mse", r.mse, kReg);
        }
    }
    // R18: frequenza 2 = dato ripetuto (ogni tipo)
    {
        const std::vector<double> x = {1, 2, 3, 4, 5, 6}, y = {2.2, 4.1, 6.5, 7.9, 10.4, 12.1}, f = {1, 2, 1, 1, 1, 1};
        const std::vector<double> xr = {1, 2, 2, 3, 4, 5, 6}, yr = {2.2, 4.1, 4.1, 6.5, 7.9, 10.4, 12.1};
        for (Reg t : {Reg::LinearAx, Reg::LinearA, Reg::MedMed, Reg::Quad, Reg::Cubic, Reg::Quart, Reg::Log, Reg::ExpE, Reg::ExpAb, Reg::Power}) {
            const Fit a = fit(t, x, y, &f), b = fit(t, xr, yr);
            bool same = a.ncoef == b.ncoef && std::abs(a.mse - b.mse) <= 1e-9 * std::max(1.0, std::abs(b.mse));
            for (std::size_t i = 0; i < a.ncoef; ++i) same = same && std::abs(a.coef[i] - b.coef[i]) <= 1e-9 * std::max(1.0, std::abs(b.coef[i]));
            ::testutil::report(same, "R18 frequenza 2 = dato ripetuto", __FILE__, __LINE__, regTitle(t));
        }
    }
    // ---- R11 Sin, R12 Logistic: SSres e coefficienti contro least_squares
    {
        Fit s = fit(Reg::Sin, sin_x, sin_y);
        near("regit.sin.a", s.coef[0], kIterCoef); near("regit.sin.b", s.coef[1], kIterCoef); near("regit.sin.c", s.coef[2], kIterCoef); near("regit.sin.d", s.coef[3], kIterCoef);
        near("regit.sin.mse", s.mse, kReg);
        double ss = 0; for (std::size_t i = 0; i < sin_x.size(); ++i) ss += std::pow(sin_y[i] - s.predict(sin_x[i]), 2);
        near("regit.sin.ss", ss, kReg);
        CHECK(!s.hasR && !s.hasR2 && s.ncoef == 4);
        const Fit l = fit(Reg::Logistic, lgs_x, lgs_y);
        near("regit.lgs.a", l.coef[0], kIterCoef); near("regit.lgs.b", l.coef[1], kIterCoef); near("regit.lgs.c", l.coef[2], kIterCoef);
        near("regit.lgs.mse", l.mse, kReg);
        CHECK_THROWS_AS(fit(Reg::Sin, {1, 2, 3}, {1, 2, 1}), fx::DimensionError);
        CHECK_THROWS_AS(fit(Reg::Logistic, {1, 2, 3}, {-1, -2, -3}), fx::MathError);
        // FitTask a passi: con un budget di 7 unita' serve piu' di un passo, stesso risultato.
        FitTask task(Reg::Sin, sin_x, sin_y, {});
        int steps = 0;
        for (;;) { fx::step::Budget b(7, 0, nullptr); ++steps; if (task.step(b)) break; }
        CHECK(steps > 10 && task.done() == task.total());
        CHECK(std::abs(task.result().coef[1] - s.coef[1]) < 1e-12);
    }
    // ---- R13 righe dei risultati per tipo, R14 residui, R17 x̂
    {
        const Fit m = fit(Reg::MedMed, ds2_x, ds2_y);
        CHECK(fitRows(m).size() == 2);                                     // Med-Med: solo a e b
        const Fit q = fit(Reg::Quad, ds2_x, ds4_y);
        auto rows = fitRows(q);
        CHECK(rows.size() == 5 && rows[3].first == "R\xC2\xB2" && rows[4].first == "MSe");
        const Fit l = fit(Reg::LinearAx, ds2_x, ds2_y);
        rows = fitRows(l);
        CHECK(rows.size() == 5 && rows[2].first == "r" && rows[3].first == "r\xC2\xB2");
        const Fit s = fit(Reg::Sin, sin_x, sin_y);
        CHECK(fitRows(s).size() == 5 && fitRows(s)[4].first == "MSe");
        const auto res = residuals(l, ds2_x, ds2_y);
        CHECK(res.size() == ds2_x.size() && std::abs(res[3] - (ds2_y[3] - l.predict(ds2_x[3]))) < 1e-15);
        double sum = 0; for (double v : res) sum += v;
        CHECK(std::abs(sum) < 1e-9);                                       // residui della retta a somma nulla
        for (Reg t : {Reg::LinearAx, Reg::LinearA, Reg::MedMed, Reg::Log, Reg::ExpE, Reg::ExpAb, Reg::Power}) {
            const Fit f = fit(t, ds2_x, t == Reg::Log || t == Reg::LinearAx || t == Reg::LinearA || t == Reg::MedMed ? ds2_y : ds4_y);
            const double y = f.predict(4.5);
            const auto xs = estimateX(f, y);
            ::testutil::report(xs.size() == 1 && std::abs(xs[0] - 4.5) < 1e-9, "R17 x^(y^(4.5)) = 4.5", __FILE__, __LINE__, regTitle(t));
        }
        const Fit c = fit(Reg::Cubic, {-2, -1, 0, 1, 2, 3}, {-6, 0, 0, 0, 6, 24});    // x^3 - x
        const auto roots = estimateX(c, 0);
        CHECK(roots.size() == 3 && std::abs(roots[0] + 1) < 1e-9 && std::abs(roots[1]) < 1e-9 && std::abs(roots[2] - 1) < 1e-9);
        const auto qr = estimateX(q, q.predict(2.0));
        CHECK(qr.size() == 2);
        const Fit lg = fit(Reg::Logistic, lgs_x, lgs_y);
        CHECK(std::abs(estimateX(lg, lg.predict(5.5))[0] - 5.5) < 1e-9);
        CHECK_THROWS_AS(estimateX(s, 2), fx::MathError);                  // Q14
        // COPY: la formula valutata da GRAPH coincide con ŷ (Sin in radianti anche in Deg).
        fx::CalculatorMemory mem;
        for (const Fit* f : {&l, &q, &s, &lg}) {
            fx::EvalContext ctx;
            ctx.memory = &mem;
            ctx.bind("x", 2.75);
            fx::setAngleMode(fx::AngleMode::Degrees);
            const double v = fx::evaluateReal(*fx::parseExpression(f->expression()), ctx);
            fx::setAngleMode(fx::AngleMode::Radians);
            ::testutil::report(std::abs(v - f->predict(2.75)) < 1e-9 * std::max(1.0, std::abs(v)), "R15 COPY: espressione = predict", __FILE__, __LINE__, f->expression());
        }
    }
    // ---- T01-T12 test contro scipy/statsmodels
    {
        Sample a; a.list = true; a.x = ds1_x;
        Sample b; b.list = true; b.x = ds3_x;
        for (int tail : {0, -1, 1}) {
            const std::string t = std::to_string(tail);
            Outcome o = zTest1S(a, 5, 2.5, tail);
            near("test.z1s." + t + ".z", o.rows[0].value, kCont); near("test.z1s." + t + ".p", o.rows[1].value, kCont);
            o = zTest2S(a, b, 2.5, 1.2, tail);
            near("test.z2s." + t + ".z", o.rows[0].value, kCont); near("test.z2s." + t + ".p", o.rows[1].value, kCont);
            o = tTest1S(a, 5, tail);
            near("test.t1s." + t + ".t", o.rows[0].value, kCont); near("test.t1s." + t + ".p", o.rows[1].value, kCont);
            for (bool pooled : {true, false}) {
                o = tTest2S(a, b, pooled, tail);
                const std::string k = "test.t2s." + std::to_string(pooled ? 1 : 0) + "." + t;
                near(k + ".t", o.rows[0].value, kCont); near(k + ".p", o.rows[1].value, kCont); near(k + ".df", o.rows[2].value, kCont);
            }
            o = zTest1P(0.5, 55, 100, tail);
            near("test.z1p." + t + ".z", o.rows[0].value, kCont); near("test.z1p." + t + ".p", o.rows[1].value, kCont);
            o = zTest2P(45, 100, 60, 120, tail);
            near("test.z2p." + t + ".z", o.rows[0].value, kCont); near("test.z2p." + t + ".p", o.rows[1].value, kCont);
            o = fTest2S(a, b, tail);
            near("test.f2s." + t + ".F", o.rows[0].value, kCont); near("test.f2s." + t + ".p", o.rows[1].value, kCont);
            o = tTestReg(ds2_x, ds4_y, nullptr, tail);
            near("test.treg." + t + ".t", o.rows[0].value, kCont); near("test.treg." + t + ".p", o.rows[1].value, kCont);
        }
        Outcome o = tTestReg(ds2_x, ds4_y, nullptr, 0);
        near("test.treg.a", o.rows[3].value, kReg); near("test.treg.b", o.rows[4].value, kReg); near("test.treg.s", o.rows[5].value, kReg); near("test.treg.r", o.rows[6].value, kReg);
        o = tTest2S(a, b, true, 0);
        CHECK(o.rows[7].label == "sp"); near("test.t2s.1.sp", o.rows[7].value, kDesc);
        // List e Var danno lo stesso test (T01, T05): x̄, sx, n dei dati
        const OneVar s1 = oneVarEx(ds1_x, nullptr, QuartileRule::Std);
        Sample v; v.mean = s1.mean; v.sd = s1.sampleSd; v.n = s1.n;
        CHECK(std::abs(tTest1S(v, 5, 0).rows[1].value - tTest1S(a, 5, 0).rows[1].value) < 1e-14);
        CHECK(std::abs(zTest1S(v, 5, 2.5, 1).rows[0].value - zTest1S(a, 5, 2.5, 1).rows[0].value) < 1e-14);
        CHECK(zTest1S(v, 5, 2.5, 0).rows.size() == 4 && zTest1S(a, 5, 2.5, 0).rows.size() == 5);   // sx solo con List
        CHECK_THROWS(zTest1P(0.5, 101, 100, 0));
        CHECK_THROWS(zTest1P(1.5, 10, 100, 0));
        // T08 chi^2 GOF con CNTRB, T09 2-way con la matrice attesa
        o = chiGof(gof_o, gof_e, 3);
        near("test.gof.chi", o.rows[0].value, kCont); near("test.gof.p", o.rows[1].value, kCont);
        CHECK(o.listOut.size() == 4);
        for (std::size_t i = 0; i < 4; ++i) near("test.gof.cntrb" + std::to_string(i), o.listOut[i], kCont);
        std::vector<double> e;
        o = chi2Way(way2_o, 2, 3, e);
        near("test.way2.chi", o.rows[0].value, kCont); near("test.way2.p", o.rows[1].value, kCont); near("test.way2.df", o.rows[2].value, 0, 0);
        for (std::size_t i = 0; i < 6; ++i) near("test.way2.e" + std::to_string(i), e[i], kCont);
        CHECK_THROWS_AS(chi2Way({1, 2}, 1, 2, e), fx::DimensionError);
        // T11, T12 ANOVA (una via, due vie con e senza repliche)
        o = anova(an1_a, {}, an1_y);
        near("anova1.ssa", o.rows[1].value, kAnova); near("anova1.f", o.rows[3].value, kAnova); near("anova1.p", o.rows[4].value, kAnova);
        near("anova1.sse", o.rows[6].value, kAnova);
        AnovaTask t2(an2_a, an2_b, an2_y);
        int steps = 0;
        for (;;) { fx::step::Budget bud(3, 0, nullptr); ++steps; if (t2.step(bud)) break; }
        CHECK(steps >= 4);                                                // a passi (AC possibile tra un passo e l'altro)
        const AnovaTable& tb = t2.table();
        CHECK(tb.twoWay && tb.interaction);
        near("anova2.ssa", tb.ssA, kAnova); near("anova2.fa", tb.fA, kAnova); near("anova2.pa", tb.pA, kAnova);
        near("anova2.ssb", tb.ssB, kAnova); near("anova2.fb", tb.fB, kAnova); near("anova2.pb", tb.pB, kAnova);
        near("anova2.ssab", tb.ssAB, kAnova); near("anova2.fab", tb.fAB, kAnova); near("anova2.pab", tb.pAB, kAnova);
        near("anova2.sse", tb.ssE, kAnova); near("anova2.dfe", tb.dfE, 0, 0); near("anova2.dfab", tb.dfAB, 0, 0);
        const Outcome o3 = anova(an3_a, an3_b, an3_y);
        AnovaTask t3(an3_a, an3_b, an3_y); { fx::step::Budget u; while (!t3.step(u)) {} }
        CHECK(!t3.table().interaction && o3.rows.size() == 13);
        near("anova3.ssa", t3.table().ssA, kAnova); near("anova3.fb", t3.table().fB, kAnova); near("anova3.pb", t3.table().pB, kAnova);
        near("anova3.sse", t3.table().ssE, kAnova); near("anova3.dfe", t3.table().dfE, 0, 0);
        CHECK_THROWS_AS(anova({1, 1, 2, 2, 2}, {1, 2, 1, 2, 2}, {1, 2, 3, 4, 5}), fx::DimensionError);   // Q6 non bilanciato
        // I01-I06 intervalli
        auto lr = [&](const char* k, const Outcome& x) { near(std::string(k) + ".left", x.rows[0].value, kCont); near(std::string(k) + ".right", x.rows[1].value, kCont); };
        lr("int.z1s", zInt1S(a, 0.95, 2.5)); lr("int.z2s", zInt2S(a, b, 0.95, 2.5, 1.2)); lr("int.z1p", zInt1P(0.95, 55, 100));
        lr("int.z2p", zInt2P(0.95, 45, 100, 60, 120)); lr("int.t1s", tInt1S(a, 0.95));
        lr("int.t2s.1", tInt2S(a, b, 0.95, true)); lr("int.t2s.0", tInt2S(a, b, 0.95, false));
    }
    // ---- D01-D08 le 24 funzioni (nucleo condiviso dist_core), D09 liste
    {
        using namespace fx::dist;
        auto run = [](Family f, Func fn, std::vector<double> v, Tail t = Tail::Left) { Input in; in.values = std::move(v); in.tail = t; return compute({f, fn}, in); };
        near("dist.npd", run(Family::Normal, Func::PD, {1.3, 1.7, 0.5}).main, kCont);
        Result r = run(Family::Normal, Func::CD, {-0.4, 2.1, 1.7, 0.5});
        near("dist.ncd", r.main, kCont);
        CHECK(r.rows.size() == 3 && std::abs(r.rows[1].second - (-0.9 / 1.7)) < 1e-15);   // z:Low
        near("dist.invn.left", run(Family::Normal, Func::Inv, {0.3, 1.7, 0.5}).main, kInv);
        near("dist.invn.right", run(Family::Normal, Func::Inv, {0.3, 1.7, 0.5}, Tail::Right).main, kInv);
        r = run(Family::Normal, Func::Inv, {0.8, 1.7, 0.5}, Tail::Central);
        near("dist.invn.c1", r.rows[0].second, kInv); near("dist.invn.c2", r.rows[1].second, kInv);
        near("dist.tpd", run(Family::T, Func::PD, {0.7, 6}).main, kCont);
        near("dist.tcd", run(Family::T, Func::CD, {-0.8, 1.9, 6}).main, kCont);
        near("dist.invt", run(Family::T, Func::Inv, {0.025, 6}).main, kInv);
        near("dist.cpd", run(Family::Chi, Func::PD, {2.5, 4}).main, kCont);
        near("dist.ccd", run(Family::Chi, Func::CD, {1.1, 5.2, 4}).main, kCont);
        near("dist.invc", run(Family::Chi, Func::Inv, {0.05, 4}).main, kInv);
        near("dist.fpd", run(Family::F, Func::PD, {1.4, 4, 9}).main, kCont);
        near("dist.fcd", run(Family::F, Func::CD, {0.5, 3.0, 4, 9}).main, kCont);
        near("dist.invf", run(Family::F, Func::Inv, {0.05, 4, 9}).main, kInv);
        near("dist.bpd", run(Family::Binomial, Func::PD, {7, 20, 0.35}).main, kDisc);
        near("dist.bcd", run(Family::Binomial, Func::CD, {4, 9, 20, 0.35}).main, kDisc);
        near("dist.invb", run(Family::Binomial, Func::Inv, {0.6, 20, 0.35}).main, 0, 0);
        near("dist.ppd", run(Family::Poisson, Func::PD, {4, 3.2}).main, kDisc);
        near("dist.pcd", run(Family::Poisson, Func::CD, {2, 6, 3.2}).main, kDisc);
        near("dist.invp", run(Family::Poisson, Func::Inv, {0.75, 3.2}).main, 0, 0);
        near("dist.gpd", run(Family::Geometric, Func::PD, {3, 0.3}).main, kDisc);
        near("dist.gcd", run(Family::Geometric, Func::CD, {2, 5, 0.3}).main, kDisc);
        near("dist.invg", run(Family::Geometric, Func::Inv, {0.8, 0.3}).main, 0, 0);
        near("dist.hpd", run(Family::Hypergeometric, Func::PD, {2, 6, 8, 25}).main, kDisc);
        near("dist.hcd", run(Family::Hypergeometric, Func::CD, {1, 3, 6, 8, 25}).main, kDisc);
        near("dist.invh", run(Family::Hypergeometric, Func::Inv, {0.7, 6, 8, 25}).main, 0, 0);
        CHECK_THROWS(run(Family::Normal, Func::CD, {2, 1, 1, 0}));                       // Lower > Upper
        // D09: ingresso List (Npd; Bcd con U.List e Lower 0; InvT) come ListTask a passi
        Input in; in.values = {0, 1.7, 0.5};
        auto l = computeList({Family::Normal, Func::PD}, in, {-1.0, 0.0, 0.5, 2.0, 3.5});
        for (std::size_t i = 0; i < 5; ++i) near("dist.list.npd" + std::to_string(i), l[i], kCont);
        in.values = {0, 0, 20, 0.35};
        l = computeList({Family::Binomial, Func::CD}, in, {0, 3, 7, 12, 20});
        for (std::size_t i = 0; i < 5; ++i) near("dist.list.bcd" + std::to_string(i), l[i], kDisc);
        in.values = {0.05, 6};
        ListTask task({Family::T, Func::Inv}, in, {0.01, 0.05, 0.1, 0.5, 0.9});
        int steps = 0;
        for (;;) { fx::step::Budget b(2, 0, nullptr); ++steps; if (task.step(b)) break; }
        CHECK(steps == 3 && task.results().size() == 5);
        for (std::size_t i = 0; i < 5; ++i) near("dist.list.invt" + std::to_string(i), task.results()[i], kInv);
        bool named = false;
        try { (void)computeList({Family::Binomial, Func::PD}, {{0, 10, 1.5}}, {1, 2}); } catch (const fx::MathError& e) { named = std::string(e.what()).find("elemento 1") != std::string::npos; }
        CHECK(named);
        // Tabella: 24 funzioni con nomi e campi
        for (std::size_t i = 0; i < KindCount; ++i) {
            const Kind k = kindAt(i);
            CHECK(indexOf(k) == i && std::strlen(kindKey(k)) >= 3 && !paramSpecs(k).empty());
            Input d; for (const ParamSpec& p : paramSpecs(k)) d.values.push_back(p.def);
            ::testutil::report(std::isfinite(compute(k, d).main), "valori iniziali calcolabili", __FILE__, __LINE__, kindName(k));
        }
        // D10 modello del grafico: area evidenziata e finestra con il punto
        in.values = {-0.4, 2.1, 1.7, 0.5};
        const Result cr = compute({Family::Normal, Func::CD}, in);
        const GraphModel g = graphModel({Family::Normal, Func::CD}, in, cr);
        CHECK(g.available && g.areas.size() == 1 && g.areas[0].first == -0.4 && g.xmin < -0.4 && g.xmax > 2.1);
        CHECK(std::abs(density(g, 0.5) - 1 / (1.7 * std::sqrt(2 * 3.14159265358979323846))) < 1e-12);
        CHECK(!graphModel({Family::Binomial, Func::PD}, {{7, 20, 0.35}}, compute({Family::Binomial, Func::PD}, {{7, 20, 0.35}})).available);
        // T13: grafico del test a due code (due aree simmetriche)
        Sample a; a.list = true; a.x = ds1_x;
        const GraphModel tg = testGraph(zTest1S(a, 5, 2.5, 0));
        CHECK(tg.available && tg.areas.size() == 2 && tg.areas[0].second == -tg.areas[1].first);
    }
    // ---- G06 NPP, G09 N-Dist, G07 istogramma, G08 baffi e outlier, G12 torta
    {
        std::vector<double> xs, zs;
        normalProbability(ref::ds1_x, nullptr, xs, zs);
        CHECK(xs.size() == ref::ds1_x.size() && std::is_sorted(xs.begin(), xs.end()));
        for (std::size_t i = 0; i < zs.size(); ++i) near("graph.npp.z" + std::to_string(i), zs[i], kInv);
        const OneVar o = oneVarEx(ref::ds1_x, nullptr, QuartileRule::Std);
        near("graph.ndist.peak", 1 / (o.popSd * std::sqrt(2 * 3.14159265358979323846)), kCont);
        const Histogram h = histogram({1, 2, 2, 3, 4.5, 5}, nullptr, 1, 1);
        CHECK(h.counts.size() == 5 && h.counts[0] == 1 && h.counts[1] == 2 && h.counts[2] == 1 && h.counts[3] == 1 && h.counts[4] == 1);
        const std::vector<double> hf = {1, 1, 1, 1, 1, 3};
        CHECK(histogram({1, 2, 2, 3, 4.5, 5}, &hf, 1, 2).counts == std::vector<double>({3, 2, 3}));
        CHECK_THROWS(histogram({1, 2}, nullptr, 0, 0));
        const BoxModel b = boxModel({1, 2, 3, 4, 5, 6, 7, 30}, nullptr, QuartileRule::Std, true);
        CHECK(b.outliers == std::vector<double>({30}) && b.highWhisker == 7 && b.lowWhisker == 1 && b.q1 == 2.5 && b.q3 == 6.5);
        const BoxModel b2 = boxModel({1, 2, 3, 4, 5, 6, 7, 30}, nullptr, QuartileRule::Std, false);
        CHECK(b2.outliers.empty() && b2.highWhisker == 30);
        const auto pct = piePercent({1, 3});
        CHECK(pct.size() == 2 && pct[0] == 25 && pct[1] == 75);
        CHECK_THROWS(piePercent({1, -1}));
    }
    // ---- U02 sezione 0x0101: andata e ritorno, valori rifiutati
    {
        PlusState s;
        s.graphs[1].type = GraphType::Pie; s.graphs[1].on = true; s.graphs[2].hStart = 0.5; s.graphs[2].hWidth = 2; s.graphs[2].hSet = true;
        s.var1X = 4; s.var1Freq = 5; s.var2Freq = 6; s.statWindManual = true; s.residList = 9; s.quartiles = QuartileRule::OnData;
        s.listColor[3] = 6; s.listFormula[2] = "List 1\xC3\x97" "2";
        s.store({{VP, 0.25}, {VRegA, -3}, {VDistP, 1e-30}});
        std::vector<std::uint8_t> blob;
        encode(s, blob);
        PlusState back;
        CHECK(decode(back, blob.data(), blob.size()) && back == s);
        double v = 0;
        CHECK(back.value(VP, v) && v == 0.25 && !back.value(VZ, v));
        PlusState d; reset(d);
        CHECK(d == PlusState{} && d.graphs[0].on && !d.graphs[1].on && d.graphs[2].color == 3);
        // Ogni byte troncato o un NaN fa fallire la decodifica senza toccare lo stato.
        for (std::size_t n = 0; n < blob.size(); ++n) { PlusState t = d; CHECK(!decode(t, blob.data(), n) && t == d); }
        PlusState nanState = s;
        nanState.store({{VZ, std::numeric_limits<double>::quiet_NaN()}});
        blob.clear(); encode(nanState, blob);
        PlusState t = d;
        CHECK(!decode(t, blob.data(), blob.size()) && t == d);
        CHECK(resultVarByName("stat_res_p") == VP && std::string(resultVarName(VDistP)) == "stat_dist_p" && resultVarByName("nope") == -1);
    }
}
} // namespace
#endif

int main() {
    const double inf = std::numeric_limits<double>::infinity();
    auto o = oneVariable({1, 2, 3, 4, 5});
    CHECK_NEAR(o.mean, 3, 1e-15); CHECK_NEAR(o.sum, 15, 0); CHECK_NEAR(o.sumSq, 55, 0);
    CHECK_NEAR(o.popSd, std::sqrt(2.0), 1e-15); CHECK_NEAR(o.sampleSd, std::sqrt(2.5), 1e-15);
    CHECK_NEAR(o.q1, 1.5, 0); CHECK_NEAR(o.median, 3, 0); CHECK_NEAR(o.q3, 4.5, 0);
    CHECK(o.modes.size() == 5 && o.modeCount == 1);
    o = oneVariable({6, 1, 5, 2, 4, 3});
    CHECK_NEAR(o.q1, 2, 0); CHECK_NEAR(o.median, 3.5, 0); CHECK_NEAR(o.q3, 5, 0); CHECK(o.min == 1 && o.max == 6);
    std::vector<double> f = {1, 3, 1};
    o = oneVariable({10, 20, 30}, &f);
    CHECK_NEAR(o.n, 5, 0); CHECK_NEAR(o.mean, 20, 1e-12); CHECK(o.modes.size() == 1 && o.modes[0] == 20 && o.modeCount == 3);
    CHECK_THROWS(oneVariable({}));
    auto t = twoVariable({1, 2, 3}, {2, 4, 7});
    CHECK_NEAR(t.sumXY, 31, 0); CHECK_NEAR(t.meanY, 13.0 / 3, 1e-15); CHECK(t.maxY == 7);
    CHECK_THROWS(twoVariable({1, 2}, {1}));
    // Regressions on exactly generated data.
    std::vector<double> x = {1, 2, 3, 4, 5, 6, 7, 8, 9}, y;
    for (double v : x) y.push_back(2 * v + 1);
    auto r = regress(RegType::Linear, x, y); CHECK_NEAR(r.coef[0], 2, 1e-12); CHECK_NEAR(r.coef[1], 1, 1e-12); CHECK_NEAR(r.r, 1, 1e-12);
    r = regress(RegType::MedMed, x, y); CHECK_NEAR(r.coef[0], 2, 1e-12); CHECK_NEAR(r.coef[1], 1, 1e-12);
    y.clear(); for (double v : x) y.push_back(3 * v * v + 2 * v + 1);
    r = regress(RegType::Quad, x, y); CHECK_NEAR(r.coef[0], 3, 1e-8); CHECK_NEAR(r.coef[1], 2, 1e-7); CHECK_NEAR(r.coef[2], 1, 1e-6); CHECK_NEAR(r.r2, 1, 1e-12);
    y.clear(); for (double v : x) y.push_back(v * v * v - v);
    r = regress(RegType::Cubic, x, y); CHECK_NEAR(r.coef[0], 1, 1e-8); CHECK_NEAR(r.coef[2], -1, 1e-6);
    y.clear(); for (double v : x) y.push_back(1 + 2 * std::log(v));
    r = regress(RegType::Log, x, y); CHECK_NEAR(r.coef[0], 1, 1e-12); CHECK_NEAR(r.coef[1], 2, 1e-12);
    y.clear(); for (double v : x) y.push_back(2 * std::exp(0.5 * v));
    r = regress(RegType::Exp, x, y); CHECK_NEAR(r.coef[0], 2, 1e-10); CHECK_NEAR(r.coef[1], 0.5, 1e-12);
    y.clear(); for (double v : x) y.push_back(2 * std::pow(3.0, v));
    r = regress(RegType::AbExp, x, y); CHECK_NEAR(r.coef[0], 2, 1e-9); CHECK_NEAR(r.coef[1], 3, 1e-12);
    y.clear(); for (double v : x) y.push_back(3 * v * v);
    r = regress(RegType::Power, x, y); CHECK_NEAR(r.coef[0], 3, 1e-10); CHECK_NEAR(r.coef[1], 2, 1e-12);
    CHECK_NEAR(r.predict(10), 300, 1e-8);
    CHECK_THROWS(regress(RegType::Log, {0, 1}, {1, 2}));
    CHECK_THROWS(regress(RegType::Linear, {1, 1, 1}, {1, 2, 3}));
    // Distributions (reference: standard statistical tables).
    CHECK_NEAR(normalCd(-1.96, 1.96, 1, 0), 0.9500042097, 1e-9);
    CHECK_NEAR(inverseNormal(0.975, 1, 0), 1.959963985, 1e-7);
    CHECK_NEAR(normalPd(0, 1, 0), 0.3989422804, 1e-10);
    CHECK_NEAR(binomialCd(2, 4, 0.5), 0.6875, 1e-12);
    CHECK_NEAR(binomialPd(50, 100, 0.5), 0.0795892374, 1e-9);
    CHECK_NEAR(poissonCd(2, 1), 0.9196986029, 1e-9);
    CHECK_NEAR(tCd(-inf, 2.228138852, 10), 0.975, 1e-8);
    CHECK_NEAR(tCd(-2.776445105, 2.776445105, 4), 0.95, 1e-8);
    CHECK_NEAR(chiSquareCd(0, 3.841458821, 1), 0.95, 1e-8);
    CHECK_NEAR(chiSquareCd(0, 18.30703805, 10), 0.95, 1e-8);
    CHECK_NEAR(fCd(0, 4.964602744, 1, 10), 0.95, 1e-8);
    CHECK_NEAR(fCd(0, 3.325834530, 5, 10), 0.95, 1e-8);
    CHECK_NEAR(fx::binomialPmf(100, 50, 0.5), 0.0795892374, 1e-9);   // fixed: no 64-bit overflow
    // Tests and intervals.
    auto tt = tTest1({2, 3, 4, 5, 6}, 3, 0);
    CHECK_NEAR(tt.statistic, std::sqrt(2.0), 1e-12); CHECK_NEAR(tt.df, 4, 0); CHECK_NEAR(tt.p, 0.2301996, 1e-6);
    auto zt = zTest1({2, 3, 4, 5, 6}, 3, 1, 1);
    CHECK_NEAR(zt.statistic, std::sqrt(5.0), 1e-12); CHECK_NEAR(zt.p, 0.01267366, 1e-7);
    auto zi = zInterval({2, 3, 4, 5, 6}, 1, 0.95);
    CHECK_NEAR(zi.lower, 4 - 1.959963985 / std::sqrt(5.0), 1e-7);
    auto ti = tInterval({2, 3, 4, 5, 6}, 0.95);
    CHECK_NEAR(ti.upper, 4 + 2.776445105 * std::sqrt(2.5) / std::sqrt(5.0), 1e-7);
#if FXAI_BOARD_PICO_PLUS_2W
    plusEngine();
#endif
    return testutil::finish("statistics_test");
}
