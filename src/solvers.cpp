#include "solvers.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace fx {
namespace {
Complex horner(const std::vector<double>& a, Complex z, Complex* derivative = nullptr) {
    Complex p = a[0], d = 0;
    for (std::size_t i = 1; i < a.size(); ++i) { d = d * z + p; p = p * z + a[i]; }
    if (derivative) *derivative = d;
    return p;
}
double magnitudeScale(const std::vector<double>& a, double x) {
    double s = 0, xp = 1;
    for (std::size_t i = a.size(); i-- > 0;) { s += std::abs(a[i]) * xp; xp *= std::abs(x); }
    return s;
}
} // namespace

double polynomialValue(const std::vector<double>& a, double x) noexcept {
    double p = 0;
    for (double c : a) p = p * x + c;
    return p;
}

std::vector<Complex> polynomialRoots(std::vector<double> a) {
    for (double c : a) if (!std::isfinite(c)) throw MathError("Math ERROR: coefficiente non finito");
    while (!a.empty() && a.front() == 0) a.erase(a.begin());
    if (a.empty()) throw MathError("Math ERROR: polinomio nullo (infinite soluzioni)");
    std::vector<Complex> roots;
    // Zero roots.
    while (a.size() > 1 && a.back() == 0) { roots.emplace_back(0.0, 0.0); a.pop_back(); }
    const std::size_t n = a.size() - 1;
    if (n == 0) return roots;
    if (n == 1) { roots.emplace_back(-a[1] / a[0], 0.0); }
    else if (n == 2) {
        const auto q = solveQuadratic(a[0], a[1], a[2]);
        if (q.kind == QuadraticResult::TwoReal) { roots.emplace_back(q.x1, 0); roots.emplace_back(q.x2, 0); }
        else if (q.kind == QuadraticResult::Double) { roots.emplace_back(q.x1, 0); roots.emplace_back(q.x1, 0); }
        else { roots.emplace_back(q.real, q.imag); roots.emplace_back(q.real, -q.imag); }
    } else {
        // Aberth-Ehrlich on the monic polynomial.
        std::vector<double> m(a.size());
        for (std::size_t i = 0; i < a.size(); ++i) m[i] = a[i] / a[0];
        double radius = 0;   // Cauchy bound
        for (std::size_t i = 1; i < m.size(); ++i) radius = std::max(radius, std::abs(m[i]));
        radius = 1 + radius;
        double r0 = std::pow(std::abs(m.back()), 1.0 / static_cast<double>(n));
        if (!(r0 > 0) || r0 > radius) r0 = std::min(radius, 1.0);
        std::vector<Complex> z(n);
        for (std::size_t k = 0; k < n; ++k) z[k] = std::polar(r0, 2.0 * 3.14159265358979323846 * static_cast<double>(k) / static_cast<double>(n) + 0.4);
        for (int it = 0; it < 500; ++it) {
            double maxStep = 0;
            for (std::size_t i = 0; i < n; ++i) {
                Complex d;
                const Complex p = horner(m, z[i], &d);
                if (p == Complex(0, 0)) continue;
                const Complex ratio = p / d;
                Complex sum = 0;
                for (std::size_t j = 0; j < n; ++j) if (j != i) sum += 1.0 / (z[i] - z[j]);
                const Complex w = ratio / (1.0 - ratio * sum);
                if (!std::isfinite(w.real()) || !std::isfinite(w.imag())) continue;
                z[i] -= w;
                maxStep = std::max(maxStep, std::abs(w) / std::max(1.0, std::abs(z[i])));
            }
            if (maxStep < 1e-15) break;
        }
        // Newton polishing on the original coefficients.
        for (auto& r : z) {
            for (int k = 0; k < 3; ++k) {
                Complex d; const Complex p = horner(a, r, &d);
                if (std::abs(d) == 0) break;
                const Complex nr = r - p / d;
                if (!std::isfinite(nr.real()) || !std::isfinite(nr.imag())) break;
                if (std::abs(horner(a, nr)) <= std::abs(p)) r = nr; else break;
            }
        }
        // Snap nearly-real roots (multiple real roots converge with tiny imaginary noise).
        for (auto& r : z) {
            const double scale = std::max(1.0, std::abs(r));
            if (std::abs(r.imag()) < 1e-6 * scale) {
                const double x = r.real();
                if (std::abs(polynomialValue(a, x)) <= 1e-8 * magnitudeScale(a, x) || std::abs(r.imag()) < 1e-12 * scale) r = {x, 0.0};
            }
        }
        // Exact conjugate pairs.
        std::vector<bool> used(n, false);
        for (std::size_t i = 0; i < n; ++i) {
            if (used[i] || z[i].imag() == 0) continue;
            std::size_t best = n; double bestD = std::numeric_limits<double>::max();
            for (std::size_t j = 0; j < n; ++j) {
                if (j == i || used[j] || z[j].imag() == 0) continue;
                const double dd = std::abs(z[j] - std::conj(z[i]));
                if (dd < bestD) { bestD = dd; best = j; }
            }
            if (best < n) {
                const Complex avg = 0.5 * (z[i] + std::conj(z[best]));
                z[i] = avg; z[best] = std::conj(avg); used[i] = used[best] = true;
            }
        }
        roots.insert(roots.end(), z.begin(), z.end());
    }
    for (auto& r : roots) {
        if (r.real() == 0.0) r.real(0.0);
        if (std::abs(r.real()) < 1e-14 * std::max(1.0, std::abs(r))) r.real(0.0);
    }
    // Real roots first (ascending), then complex by real part, positive imaginary first.
    std::stable_sort(roots.begin(), roots.end(), [](const Complex& x, const Complex& y) {
        const bool rx = x.imag() == 0, ry = y.imag() == 0;
        if (rx != ry) return rx;
        if (x.real() != y.real()) return x.real() < y.real();
        return x.imag() > y.imag();
    });
    return roots;
}

std::vector<SolutionInterval> polynomialInequality(const std::vector<double>& coeffsIn, InequalityRelation rel) {
    std::vector<double> a = coeffsIn;
    while (!a.empty() && a.front() == 0) a.erase(a.begin());
    const double inf = std::numeric_limits<double>::infinity();
    const bool strict = rel == InequalityRelation::Less || rel == InequalityRelation::Greater;
    const bool wantPositive = rel == InequalityRelation::Greater || rel == InequalityRelation::GreaterEqual;
    auto ok = [&](double v) { return wantPositive ? (strict ? v > 0 : v >= 0) : (strict ? v < 0 : v <= 0); };
    if (a.empty()) return ok(0.0) ? std::vector<SolutionInterval>{{-inf, inf, false, false}} : std::vector<SolutionInterval>{};
    if (a.size() == 1) return ok(a[0]) ? std::vector<SolutionInterval>{{-inf, inf, false, false}} : std::vector<SolutionInterval>{};
    std::vector<double> xs;
    for (const auto& r : polynomialRoots(a)) if (r.imag() == 0) xs.push_back(r.real());
    std::sort(xs.begin(), xs.end());
    std::vector<double> distinct;
    for (double x : xs) if (distinct.empty() || std::abs(x - distinct.back()) > 1e-9 * std::max(1.0, std::abs(x))) distinct.push_back(x);
    std::vector<SolutionInterval> out;
    if (distinct.empty()) return ok(polynomialValue(a, 0.0)) ? std::vector<SolutionInterval>{{-inf, inf, false, false}} : out;
    // Open intervals between consecutive roots: test the sign at an interior point.
    auto sample = [&](double lo, double hi) {
        if (!std::isfinite(lo)) return hi - std::max(1.0, std::abs(hi));
        if (!std::isfinite(hi)) return lo + std::max(1.0, std::abs(lo));
        return 0.5 * (lo + hi);
    };
    std::vector<double> bounds; bounds.push_back(-inf); bounds.insert(bounds.end(), distinct.begin(), distinct.end()); bounds.push_back(inf);
    for (std::size_t i = 0; i + 1 < bounds.size(); ++i) {
        const double lo = bounds[i], hi = bounds[i + 1];
        if (ok(polynomialValue(a, sample(lo, hi)))) out.push_back({lo, hi, false, false});
    }
    // Roots themselves satisfy the non-strict relations.
    if (!strict) for (double r : distinct) out.push_back({r, r, true, true});
    return unionSolutionSets(out, {});
}

std::string formatSolutionSet(const std::vector<SolutionInterval>& set, DisplayFormat f) {
    if (set.empty()) return "Nessuna soluzione";
    std::string out;
    for (const auto& s : set) {
        if (!out.empty()) out += " , ";
        const bool loInf = std::isinf(s.lower), hiInf = std::isinf(s.upper);
        if (loInf && hiInf) return "Tutti i reali";
        if (!loInf && !hiInf && s.lower == s.upper) { out += "x=" + formatReal(s.lower, f); continue; }
        if (!loInf) out += formatReal(s.lower, f) + (s.lowerInclusive ? "\xE2\x89\xA4" : "<");
        out += "x";
        if (!hiInf) out += std::string(s.upperInclusive ? "\xE2\x89\xA4" : "<") + formatReal(s.upper, f);
    }
    return out;
}

SolverResult solveEquation(const std::string& equation, double guess, const CalculatorMemory* memory) {
    const auto eq = equation.find('=');
    if (eq != std::string::npos && equation.find('=', eq + 1) != std::string::npos) throw SyntaxError("Syntax ERROR: un solo '=' ammesso");
    const CompiledExpression lhs(eq == std::string::npos ? equation : equation.substr(0, eq));
    const CompiledExpression rhs(eq == std::string::npos ? std::string("0") : equation.substr(eq + 1));
    auto f = [&](double x, bool& ok) {
        double l = 0, r = 0;
        ok = lhs.tryEval(x, l, memory) && rhs.tryEval(x, r, memory);
        return l - r;
    };
    auto finish = [&](double x) { SolverResult res{x, lhs(x, memory), rhs(x, memory)}; return res; };
    bool ok = false;
    // 1) Newton with central differences from the initial guess.
    double x = guess;
    // Iterates that run away (asymptotes such as 1/x = 0) are not roots.
    const double escape = 1e8 * std::max(1.0, std::abs(guess));
    for (int i = 0; i < 60; ++i) {
        if (std::abs(x) > escape) break;
        const double y = f(x, ok);
        if (!ok) break;
        if (std::abs(y) < 1e-12 * std::max(1.0, std::abs(lhs(x, memory)))) return finish(x);
        const double h = 1e-6 * std::max(1.0, std::abs(x));
        bool ok1 = false, ok2 = false;
        const double d = (f(x + h, ok1) - f(x - h, ok2)) / (2 * h);
        if (!ok1 || !ok2 || d == 0 || !std::isfinite(d)) break;
        const double nx = x - y / d;
        if (!std::isfinite(nx)) break;
        if (std::abs(nx - x) <= 1e-13 * std::max(1.0, std::abs(x))) {
            bool okn = false; const double yn = f(nx, okn);
            if (okn && std::abs(yn) < 1e-8 * std::max(1.0, std::abs(lhs(nx, memory)))) return finish(nx);
            break;
        }
        x = nx;
    }
    // 2) Expanding bracket search around the guess, then bisection.
    double step = std::max(0.5, std::abs(guess) * 0.1);
    for (int k = 0; k < 60; ++k, step *= 1.6) {
        for (int side = -1; side <= 1; side += 2) {
            const double a = guess, b = guess + side * step;
            bool oa = false, ob = false;
            double fa = f(a, oa), fb = f(b, ob);
            if (!oa || !ob || fa * fb > 0) continue;
            double lo = std::min(a, b), hi = std::max(a, b);
            double flo = lo == a ? fa : fb;
            for (int it = 0; it < 200 && hi - lo > 1e-15 * std::max(1.0, std::abs(lo)); ++it) {
                const double mid = 0.5 * (lo + hi);
                bool om = false; const double fm = f(mid, om);
                if (!om) break;
                if ((fm < 0) == (flo < 0)) { lo = mid; flo = fm; } else hi = mid;
            }
            const double root = 0.5 * (lo + hi);
            bool orr = false; const double fr = f(root, orr);
            // Reject sign changes caused by poles (e.g. 1/x): the residual must be small.
            if (orr && std::abs(fr) < 1e-6 * std::max(1.0, std::abs(lhs(root, memory)))) return finish(root);
        }
    }
    throw MathError("Math ERROR: SOLVE non converge (prova un altro valore iniziale)");
}
} // namespace fx
