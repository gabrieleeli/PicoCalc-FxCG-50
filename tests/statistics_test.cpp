// STATISTICS numerics against textbook reference values.
#include "statistics.hpp"
#include "math_engine.hpp"
#include "test_util.hpp"
#include <cmath>
#include <limits>

using namespace fx::stats;
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
    return testutil::finish("statistics_test");
}
