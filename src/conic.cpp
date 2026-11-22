// Nucleo di Conic Graphs (v1.17.0): vedi conic.hpp.
#include "conic.hpp"
#include "expression.hpp"
#include "mode_codec.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::conic {
namespace {
constexpr double Pi = 3.14159265358979323846;
double A(const State& s) { return s.coef[0]; }
double B(const State& s) { return s.coef[1]; }
double H(const State& s) { return s.coef[2]; }
double K(const State& s) { return s.coef[3]; }
double R(const State& s) { return s.coef[4]; }
std::string fmt(double v) { char b[32]; std::snprintf(b, sizeof b, "%.10G", std::abs(v) < 1e-13 ? 0.0 : v); return b; }
std::string pt(const std::array<double, 2>& p) { return "(" + fmt(p[0]) + ", " + fmt(p[1]) + ")"; }
} // namespace

const char* modelName(Model m) noexcept {
    switch (m) {
    case Model::ParabolaX: case Model::ParabolaY: return "Parabola";
    case Model::Circle: return "Circle";
    case Model::Ellipse: return "Ellipse";
    default: return "Hyperbola";
    }
}
const char* modelFormula(Model m) noexcept {
    switch (m) {
    case Model::ParabolaX: return "X=A(Y-K)\xC2\xB2+H";
    case Model::ParabolaY: return "Y=A(X-H)\xC2\xB2+K";
    case Model::Circle: return "(X-H)\xC2\xB2+(Y-K)\xC2\xB2=R\xC2\xB2";
    case Model::Ellipse: return "(X-H)\xC2\xB2/A\xC2\xB2+(Y-K)\xC2\xB2/B\xC2\xB2=1";
    case Model::HyperbolaX: return "(X-H)\xC2\xB2/A\xC2\xB2-(Y-K)\xC2\xB2/B\xC2\xB2=1";
    default: return "(Y-K)\xC2\xB2/B\xC2\xB2-(X-H)\xC2\xB2/A\xC2\xB2=1";
    }
}
const char* coefficientNames(Model m) noexcept {
    switch (m) {
    case Model::ParabolaX: case Model::ParabolaY: return "AHK";
    case Model::Circle: return "HKR";
    default: return "ABHK";
    }
}
int coefIndex(char c) noexcept {
    switch (c) { case 'A': return 0; case 'B': return 1; case 'H': return 2; case 'K': return 3; case 'R': return 4; default: return -1; }
}
std::string validate(const State& s) {
    for (double v : s.coef) if (!std::isfinite(v)) return "Math ERROR: coefficiente non valido";
    switch (s.model) {
    case Model::ParabolaX: case Model::ParabolaY: if (A(s) == 0) return "Math ERROR: A=0"; break;
    case Model::Circle: if (R(s) <= 0) return "Math ERROR: R deve essere >0"; break;
    case Model::Count: return "Argument ERROR: modello";
    default: if (A(s) <= 0 || B(s) <= 0) return "Math ERROR: A e B devono essere >0"; break;
    }
    return {};
}
std::vector<Branch> branches(const State& s, double xmin, double xmax, double ymin, double ymax) {
    if (!validate(s).empty()) return {};
    const double steps = 400;
    switch (s.model) {
    case Model::ParabolaY: return {Branch{xmin - H(s), xmax - H(s), (xmax - xmin) / steps}};
    case Model::ParabolaX: return {Branch{ymin - K(s), ymax - K(s), (ymax - ymin) / steps}};
    case Model::Circle: case Model::Ellipse: return {Branch{0, 2 * Pi, 2 * Pi / steps}};
    case Model::HyperbolaX: case Model::HyperbolaY: {
        // u con |sinh u| abbastanza grande da uscire dalla finestra su entrambi i lati.
        const double span = s.model == Model::HyperbolaX ? std::max(std::abs(ymin - K(s)), std::abs(ymax - K(s))) / B(s)
                                                         : std::max(std::abs(xmin - H(s)), std::abs(xmax - H(s))) / A(s);
        const double u = std::asinh(span) + 0.2;
        return {Branch{-u, u, 2 * u / steps}, Branch{-u, u, 2 * u / steps}};
    }
    default: return {};
    }
}
bool point(const State& s, std::size_t branch, double t, double& x, double& y) noexcept {
    switch (s.model) {
    case Model::ParabolaY: x = H(s) + t; y = A(s) * t * t + K(s); return true;
    case Model::ParabolaX: y = K(s) + t; x = A(s) * t * t + H(s); return true;
    case Model::Circle: x = H(s) + R(s) * std::cos(t); y = K(s) + R(s) * std::sin(t); return true;
    case Model::Ellipse: x = H(s) + A(s) * std::cos(t); y = K(s) + B(s) * std::sin(t); return true;
    case Model::HyperbolaX: { const double sg = branch == 0 ? 1 : -1; x = H(s) + sg * A(s) * std::cosh(t); y = K(s) + B(s) * std::sinh(t); return true; }
    case Model::HyperbolaY: { const double sg = branch == 0 ? 1 : -1; y = K(s) + sg * B(s) * std::cosh(t); x = H(s) + A(s) * std::sinh(t); return true; }
    default: return false;
    }
}
const char* solveName(Solve s) noexcept {
    switch (s) {
    case Solve::Vertex: return "VERTEX";
    case Solve::Focus: return "FOCUS";
    case Solve::Center: return "CENTER";
    case Solve::Radius: return "RADIUS";
    case Solve::Asymptote: return "ASYMPT";
    default: return "e";
    }
}
Result solve(const State& s, Solve what) {
    const std::string err = validate(s);
    if (!err.empty()) throw MathError(err);
    Result r;
    const bool parabola = s.model == Model::ParabolaX || s.model == Model::ParabolaY;
    const bool hyper = s.model == Model::HyperbolaX || s.model == Model::HyperbolaY;
    auto notFor = [&]() -> Result { throw MathError(std::string("Math ERROR: ") + solveName(what) + " non vale per " + modelName(s.model)); };
    switch (what) {
    case Solve::Vertex:
        if (parabola) r.points.push_back({H(s), K(s)});
        else if (s.model == Model::Ellipse) { r.points = {{H(s) - A(s), K(s)}, {H(s) + A(s), K(s)}, {H(s), K(s) - B(s)}, {H(s), K(s) + B(s)}}; }
        else if (s.model == Model::HyperbolaX) r.points = {{H(s) - A(s), K(s)}, {H(s) + A(s), K(s)}};
        else if (s.model == Model::HyperbolaY) r.points = {{H(s), K(s) - B(s)}, {H(s), K(s) + B(s)}};
        else return notFor();
        break;
    case Solve::Focus:
        if (s.model == Model::ParabolaY) r.points.push_back({H(s), K(s) + 1 / (4 * A(s))});
        else if (s.model == Model::ParabolaX) r.points.push_back({H(s) + 1 / (4 * A(s)), K(s)});
        else if (s.model == Model::Ellipse) {
            if (A(s) >= B(s)) { const double c = std::sqrt(A(s) * A(s) - B(s) * B(s)); r.points = {{H(s) - c, K(s)}, {H(s) + c, K(s)}}; }
            else { const double c = std::sqrt(B(s) * B(s) - A(s) * A(s)); r.points = {{H(s), K(s) - c}, {H(s), K(s) + c}}; }
        } else if (hyper) {
            const double c = std::sqrt(A(s) * A(s) + B(s) * B(s));
            if (s.model == Model::HyperbolaX) r.points = {{H(s) - c, K(s)}, {H(s) + c, K(s)}};
            else r.points = {{H(s), K(s) - c}, {H(s), K(s) + c}};
        } else return notFor();
        break;
    case Solve::Center:
        if (parabola) return notFor();
        r.points.push_back({H(s), K(s)});
        break;
    case Solve::Radius:
        if (s.model != Model::Circle) return notFor();
        r.value = R(s); r.hasValue = true;
        break;
    case Solve::Asymptote:
        if (!hyper) return notFor();
        // y - K = ±(B/A)(x - H)
        r.lines = {{B(s) / A(s), K(s) - B(s) / A(s) * H(s)}, {-B(s) / A(s), K(s) + B(s) / A(s) * H(s)}};
        break;
    case Solve::Eccentricity:
        r.hasValue = true;
        if (parabola) r.value = 1;
        else if (s.model == Model::Circle) r.value = 0;
        else if (s.model == Model::Ellipse) { const double a = std::max(A(s), B(s)), b = std::min(A(s), B(s)); r.value = std::sqrt(1 - (b * b) / (a * a)); }
        else r.value = std::sqrt(A(s) * A(s) + B(s) * B(s)) / (s.model == Model::HyperbolaX ? A(s) : B(s));
        break;
    }
    for (const auto& p : r.points) r.text += (r.text.empty() ? "" : "  ") + pt(p);
    for (const auto& l : r.lines) r.text += std::string(r.text.empty() ? "" : "  ") + "Y=" + fmt(l[0]) + "X" + (l[1] >= 0 ? "+" : "") + fmt(l[1]);
    if (r.hasValue) r.text = std::string(what == Solve::Radius ? "R=" : "e=") + fmt(r.value);
    return r;
}
void reset(State& s) { s = State{}; }
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1);
    w.u8(static_cast<std::uint8_t>(s.model));
    for (double v : s.coef) w.f64(v);
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    const std::uint8_t m = r.u8();
    for (double& v : t.coef) v = r.f64();
    if (!r.end() || m >= ModelCount) return false;
    for (double v : t.coef) if (!std::isfinite(v)) return false;
    t.model = static_cast<Model>(m);
    s = t;
    return true;
}
} // namespace fx::conic
