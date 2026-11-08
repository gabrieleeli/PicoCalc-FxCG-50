#include "graph_solve.hpp"
#include "expression.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace fx::graph {
namespace {
constexpr double Eps = std::numeric_limits<double>::epsilon();
const double NaN = std::numeric_limits<double>::quiet_NaN();

double value(const RealFunction& f, double x) {
    double y;
    return f(x, y) && std::isfinite(y) ? y : NaN;
}
// Version that throws, for the shared numerical methods.
double strict(const RealFunction& f, double x) {
    double y;
    if (!f(x, y) || !std::isfinite(y)) throw MathError("Math ERROR: funzione non definita nell'intervallo");
    return y;
}
struct Samples { std::vector<double> x, y; double scale{0}; };
Samples sample(const RealFunction& f, double a, double b, int n) {
    n = std::max(n, 8);
    Samples s;
    s.x.resize(static_cast<std::size_t>(n) + 1); s.y.resize(static_cast<std::size_t>(n) + 1);
    for (int k = 0; k <= n; ++k) {
        const double x = k == n ? b : a + (b - a) * k / n;
        s.x[static_cast<std::size_t>(k)] = x;
        const double y = value(f, x);
        s.y[static_cast<std::size_t>(k)] = y;
        if (std::isfinite(y)) s.scale = std::max(s.scale, std::abs(y));
    }
    return s;
}
void sortUnique(std::vector<double>& v, double span) {
    std::sort(v.begin(), v.end());
    const double tol = std::max(1e-12, 1e-9 * span);
    std::vector<double> out;
    for (double x : v) if (out.empty() || std::abs(x - out.back()) > tol * std::max(1.0, std::abs(x))) out.push_back(x);
    if (out.size() > MaxSolutions) out.resize(MaxSolutions);
    v.swap(out);
}
// Extremum inside [lo, hi] (sign +1 minimum, -1 maximum): zero of the derivative
// when it changes sign in the bracket, otherwise Brent's minimisation.
bool refineExtremum(const RealFunction& f, double lo, double hi, double sign, double guess, double& out) {
    double best = guess, fbest = sign * value(f, guess);
    try {
        auto d = [&](double x, double& r) {
            try { r = differentiateFunction([&](double t) { return strict(f, t); }, x); return std::isfinite(r); }
            catch (...) { return false; }
        };
        double dlo, dhi;
        if (d(lo, dlo) && d(hi, dhi) && sign * dlo < 0 && sign * dhi > 0) {
            double r;
            if (brentRoot(d, lo, hi, dlo, dhi, r)) {
                const double fr = sign * value(f, r);
                if (std::isfinite(fr) && fr <= fbest + 1e-12 * std::max(1.0, std::abs(fbest))) { best = r; fbest = fr; }
            }
        }
    } catch (...) {}
    double x;
    auto g = [&](double t, double& r) { const double v = value(f, t); r = sign * v; return std::isfinite(v); };
    if (brentExtremum(g, lo, hi, 1.0, x)) {
        const double fx = sign * value(f, x);
        if (std::isfinite(fx) && fx < fbest) { best = x; fbest = fx; }
    }
    if (!std::isfinite(fbest)) return false;
    out = best;
    return true;
}
} // namespace

bool brentRoot(const RealFunction& f, double a, double b, double fa, double fb, double& root) {
    if (fa == 0) { root = a; return true; }
    if (fb == 0) { root = b; return true; }
    if ((fa > 0) == (fb > 0)) return false;
    double c = a, fc = fa, d = b - a, e = d;
    for (int it = 0; it < 200; ++it) {
        if ((fb > 0) == (fc > 0)) { c = a; fc = fa; d = e = b - a; }
        if (std::abs(fc) < std::abs(fb)) { a = b; b = c; c = a; fa = fb; fb = fc; fc = fa; }
        const double tol = 2 * Eps * std::abs(b) + 1e-300;
        const double m = 0.5 * (c - b);
        if (std::abs(m) <= tol || fb == 0) { root = b; return true; }
        if (std::abs(e) >= tol && std::abs(fa) > std::abs(fb)) {
            // Inverse quadratic interpolation (secant when only two points differ).
            double p, q;
            const double s = fb / fa;
            if (a == c) { p = 2 * m * s; q = 1 - s; }
            else {
                const double qq = fa / fc, r = fb / fc;
                p = s * (2 * m * qq * (qq - r) - (b - a) * (r - 1));
                q = (qq - 1) * (r - 1) * (s - 1);
            }
            if (p > 0) q = -q; else p = -p;
            if (2 * p < std::min(3 * m * q - std::abs(tol * q), std::abs(e * q))) { e = d; d = p / q; }
            else { d = m; e = m; }
        } else { d = m; e = m; }
        a = b; fa = fb;
        b += std::abs(d) > tol ? d : (m > 0 ? tol : -tol);
        double y;
        if (!f(b, y) || !std::isfinite(y)) return false;
        fb = y;
    }
    root = b;
    return true;
}

bool brentExtremum(const RealFunction& f, double a, double b, double sign, double& xmin) {
    const double golden = 0.3819660112501051;
    if (a > b) std::swap(a, b);
    double x = a + golden * (b - a), w = x, v = x;
    double fx;
    if (!f(x, fx) || !std::isfinite(fx)) return false;
    fx *= sign;
    double fw = fx, fv = fx, d = 0, e = 0;
    for (int it = 0; it < 200; ++it) {
        const double m = 0.5 * (a + b);
        const double tol = 1e-10 * std::abs(x) + 1e-14, t2 = 2 * tol;
        if (std::abs(x - m) <= t2 - 0.5 * (b - a)) break;
        bool parabolic = false;
        if (std::abs(e) > tol) {
            double r = (x - w) * (fx - fv), q = (x - v) * (fx - fw), p = (x - v) * q - (x - w) * r;
            q = 2 * (q - r);
            if (q > 0) p = -p; else q = -q;
            if (std::abs(p) < std::abs(0.5 * q * e) && p > q * (a - x) && p < q * (b - x)) {
                e = d; d = p / q; parabolic = true;
                const double u = x + d;
                if (u - a < t2 || b - u < t2) d = x < m ? tol : -tol;
            }
        }
        if (!parabolic) { e = (x < m ? b : a) - x; d = golden * e; }
        const double u = x + (std::abs(d) >= tol ? d : (d > 0 ? tol : -tol));
        double fu;
        if (!f(u, fu) || !std::isfinite(fu)) return false;
        fu *= sign;
        if (fu <= fx) {
            if (u < x) b = x; else a = x;
            v = w; fv = fw; w = x; fw = fx; x = u; fx = fu;
        } else {
            if (u < x) a = u; else b = u;
            if (fu <= fw || w == x) { v = w; fv = fw; w = u; fw = fu; }
            else if (fu <= fv || v == x || v == w) { v = u; fv = fu; }
        }
    }
    xmin = x;
    return true;
}

std::vector<double> findRoots(const RealFunction& f, double a, double b, int n) {
    std::vector<double> roots;
    if (!(a < b)) return roots;
    const Samples s = sample(f, a, b, n);
    const std::size_t count = s.x.size();
    for (std::size_t k = 0; k < count; ++k) {
        const double yk = s.y[k];
        if (!std::isfinite(yk)) continue;
        if (yk == 0) { roots.push_back(s.x[k]); continue; }
        if (k + 1 < count && std::isfinite(s.y[k + 1]) && s.y[k + 1] != 0 && (yk > 0) != (s.y[k + 1] > 0)) {
            double r;
            if (brentRoot(f, s.x[k], s.x[k + 1], yk, s.y[k + 1], r)) {
                // A pole or a jump also changes sign: the residual must be small.
                const double fr = value(f, r);
                if (std::isfinite(fr) && std::abs(fr) <= 1e-7 * std::max({1.0, std::abs(yk), std::abs(s.y[k + 1])})) roots.push_back(r);
            }
        }
        // Touching zero (x^2 at 0): local minimum of |f| without a sign change.
        if (k > 0 && k + 1 < count && std::isfinite(s.y[k - 1]) && std::isfinite(s.y[k + 1]) &&
            (s.y[k - 1] > 0) == (yk > 0) && (s.y[k + 1] > 0) == (yk > 0) && s.y[k - 1] != 0 && s.y[k + 1] != 0 &&
            std::abs(yk) <= std::abs(s.y[k - 1]) && std::abs(yk) <= std::abs(s.y[k + 1]) &&
            std::abs(yk) < std::abs(s.y[k - 1]) + std::abs(s.y[k + 1])) {
            double x;
            if (refineExtremum(f, s.x[k - 1], s.x[k + 1], yk > 0 ? 1.0 : -1.0, s.x[k], x)) {
                const double fx = value(f, x);
                if (std::isfinite(fx) && std::abs(fx) <= 1e-10 * std::max(1.0, s.scale)) roots.push_back(x);
            }
        }
    }
    sortUnique(roots, b - a);
    return roots;
}

std::vector<double> findExtrema(const RealFunction& f, double a, double b, bool maximum, int n) {
    std::vector<double> out;
    if (!(a < b)) return out;
    const Samples s = sample(f, a, b, n);
    const double sign = maximum ? -1.0 : 1.0;
    for (std::size_t k = 1; k + 1 < s.x.size(); ++k) {
        const double y0 = sign * s.y[k - 1], y1 = sign * s.y[k], y2 = sign * s.y[k + 1];
        if (!std::isfinite(y0) || !std::isfinite(y1) || !std::isfinite(y2)) continue;
        // Local minimum of sign*f; a flat run counts once (strict on one side).
        if (!(y1 <= y0 && y1 < y2)) continue;
        // Extend left over a plateau so that the bracket contains the extremum.
        std::size_t lo = k - 1;
        while (lo > 0 && sign * s.y[lo] == y1 && std::isfinite(s.y[lo - 1])) --lo;
        double x;
        if (refineExtremum(f, s.x[lo], s.x[k + 1], sign, s.x[k], x)) {
            if (x > a && x < b) out.push_back(x);
        }
    }
    sortUnique(out, b - a);
    return out;
}

std::vector<double> findIntersections(const RealFunction& f, const RealFunction& g, double a, double b, int n) {
    auto d = [&](double x, double& r) {
        double u, v;
        if (!f(x, u) || !g(x, v) || !std::isfinite(u) || !std::isfinite(v)) return false;
        r = u - v;
        return true;
    };
    return findRoots(d, a, b, n);
}

std::vector<double> findValue(const RealFunction& f, double y, double a, double b, int n) {
    auto d = [&](double x, double& r) { double u; if (!f(x, u) || !std::isfinite(u)) return false; r = u - y; return true; };
    return findRoots(d, a, b, n);
}

double integrate(const RealFunction& f, double a, double b) {
    if (a == b) return 0;
    // An undefined point or a pole found by a coarse scan makes the integral undefined.
    const double lo = std::min(a, b), hi = std::max(a, b);
    for (int k = 0; k <= 64; ++k) (void)strict(f, lo + (hi - lo) * k / 64);
    return integrateFunction([&](double x) { return strict(f, x); }, a, b, 1e-10);
}

double area(const RealFunction& f, double a, double b) {
    if (a == b) return 0;
    const double lo = std::min(a, b), hi = std::max(a, b);
    const std::vector<double> roots = findRoots(f, lo, hi, 400);
    std::vector<double> cuts{lo};
    for (double r : roots) if (r > lo && r < hi) cuts.push_back(r);
    cuts.push_back(hi);
    double total = 0;
    for (std::size_t i = 1; i < cuts.size(); ++i) total += std::abs(integrate(f, cuts[i - 1], cuts[i]));
    return total;
}
} // namespace fx::graph
