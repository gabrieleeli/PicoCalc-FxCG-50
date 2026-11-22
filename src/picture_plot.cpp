// Nucleo di Picture Plot (v1.17.0): vedi picture_plot.hpp.
#include "picture_plot.hpp"
#include "expression.hpp"
#include "mode_codec.hpp"
#include <cmath>

namespace fx::pplot {
std::string validate(const Window& w) {
    for (double v : {w.xmin, w.xmax, w.ymin, w.ymax}) if (!std::isfinite(v) || std::abs(v) >= 1e100) return "Math ERROR: finestra";
    if (!(w.xmin < w.xmax) || !(w.ymin < w.ymax)) return "Range ERROR: min < max";
    return {};
}
void addPoint(State& s, double x, double y) {
    if (s.points.size() >= MaxPoints) throw MathError("Memory ERROR: oltre 999 punti");
    s.points.push_back(Point{x, y});
}
void toPlane(const Window& w, int width, int height, int px, int py, double& x, double& y) noexcept {
    x = w.xmin + (w.xmax - w.xmin) * px / (width - 1);
    y = w.ymax - (w.ymax - w.ymin) * py / (height - 1);
}
void toScreen(const Window& w, int width, int height, double x, double y, double& px, double& py) noexcept {
    px = (x - w.xmin) / (w.xmax - w.xmin) * (width - 1);
    py = (w.ymax - y) / (w.ymax - w.ymin) * (height - 1);
}
void coordinates(const State& s, std::vector<double>& xs, std::vector<double>& ys) {
    xs.clear(); ys.clear();
    for (const Point& p : s.points) { xs.push_back(p.x); ys.push_back(p.y); }
}
void reset(State& s) { s = State{}; }
namespace {
void write(codec::Writer& w, const State& s) {
    w.str(s.background);
    w.f64(s.window.xmin); w.f64(s.window.xmax); w.f64(s.window.ymin); w.f64(s.window.ymax);
    w.u8(static_cast<std::uint8_t>(s.regression + 1));
    w.u16(static_cast<std::uint16_t>(s.points.size()));
    for (const Point& p : s.points) { w.f64(p.x); w.f64(p.y); }
}
bool read(codec::Reader& r, State& t) {
    t.background = r.str(64);
    t.window.xmin = r.f64(); t.window.xmax = r.f64(); t.window.ymin = r.f64(); t.window.ymax = r.f64();
    t.regression = static_cast<int>(r.u8()) - 1;
    const std::size_t n = r.u16();
    if (!r.ok || n > MaxPoints || t.regression < -1 || t.regression > 8 || !validate(t.window).empty()) return false;
    for (std::size_t k = 0; k < n; ++k) {
        Point p{r.f64(), r.f64()};
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) return false;
        t.points.push_back(p);
    }
    return r.ok;
}
} // namespace
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1);
    write(w, s);
    w.str(s.file);
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    if (!read(r, t)) return false;
    t.file = r.str(64);
    if (!r.end()) return false;
    s = std::move(t);
    return true;
}
std::vector<std::uint8_t> encodeFile(const State& s) {
    std::vector<std::uint8_t> out;
    codec::Writer w{out};
    w.u8(1);
    write(w, s);
    return out;
}
bool decodeFile(const std::uint8_t* data, std::size_t size, State& out) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    if (!read(r, t) || !r.end()) return false;
    t.file = out.file;
    out = std::move(t);
    return true;
}
} // namespace fx::pplot
