// Nucleo di Geometry (v1.17.0): vedi geometry.hpp.
#include "geometry.hpp"
#include "expression.hpp"
#include "mode_codec.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::geo {
namespace {
constexpr double Pi = 3.14159265358979323846;
std::string fmt(double v) { char b[32]; std::snprintf(b, sizeof b, "%.10G", std::abs(v) < 1e-12 ? 0.0 : v); return b; }
// Distanza del punto P dal segmento AB (o dalla retta se `infinite`).
double segmentDistance(double px, double py, double ax, double ay, double bx, double by, bool infinite) {
    const double dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    if (l2 == 0) return distance(px, py, ax, ay);
    double t = ((px - ax) * dx + (py - ay) * dy) / l2;
    if (!infinite) t = std::clamp(t, 0.0, 1.0);
    return distance(px, py, ax + t * dx, ay + t * dy);
}
} // namespace

const char* kindName(Kind k) noexcept {
    switch (k) { case Kind::Point: return "Punto"; case Kind::Segment: return "Segmento"; case Kind::Line: return "Retta"; case Kind::Circle: return "Circonferenza"; default: return "Poligono"; }
}
std::size_t pointsNeeded(Kind k) noexcept { return k == Kind::Point ? 1 : k == Kind::Polygon ? 3 : 2; }
std::string validate(const Object& o) {
    if (o.xy.size() % 2 != 0 || o.xy.empty()) return "Argument ERROR: coordinate";
    for (double v : o.xy) if (!std::isfinite(v) || std::abs(v) >= 1e100) return "Math ERROR: coordinata non valida";
    const std::size_t n = o.count();
    switch (o.kind) {
    case Kind::Point: if (n != 1) return "Argument ERROR: un punto"; break;
    case Kind::Segment: case Kind::Line: case Kind::Circle:
        if (n != 2) return "Argument ERROR: due punti";
        if (o.xy[0] == o.xy[2] && o.xy[1] == o.xy[3]) return "Math ERROR: punti coincidenti";
        break;
    case Kind::Polygon: if (n < 3 || n > 50) return "Argument ERROR: da 3 a 50 vertici"; break;
    default: return "Argument ERROR: tipo";
    }
    return {};
}
void add(State& s, Object o) {
    const std::string e = validate(o);
    if (!e.empty()) throw MathError(e);
    if (s.objects.size() >= MaxObjects) throw MathError("Memory ERROR: oltre 500 oggetti");
    s.objects.push_back(std::move(o));
}
double distance(double x0, double y0, double x1, double y1) noexcept { return std::hypot(x1 - x0, y1 - y0); }
double angleDegrees(double ax, double ay, double vx, double vy, double bx, double by) noexcept {
    const double a1 = std::atan2(ay - vy, ax - vx), a2 = std::atan2(by - vy, bx - vx);
    double d = std::abs(a1 - a2) * 180 / Pi;
    if (d > 180) d = 360 - d;
    return d;
}
double polygonArea(const Object& o) noexcept {
    const std::size_t n = o.count();
    double a = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        a += o.xy[2 * i] * o.xy[2 * j + 1] - o.xy[2 * j] * o.xy[2 * i + 1];
    }
    return std::abs(a) / 2;
}
int hitTest(const State& s, double x, double y, double tol) {
    int best = -1;
    double bestD = tol;
    for (std::size_t k = 0; k < s.objects.size(); ++k) {
        const Object& o = s.objects[k];
        double d = 1e300;
        const auto& p = o.xy;
        switch (o.kind) {
        case Kind::Point: d = distance(x, y, p[0], p[1]); break;
        case Kind::Segment: d = segmentDistance(x, y, p[0], p[1], p[2], p[3], false); break;
        case Kind::Line: d = segmentDistance(x, y, p[0], p[1], p[2], p[3], true); break;
        case Kind::Circle: d = std::abs(distance(x, y, p[0], p[1]) - distance(p[0], p[1], p[2], p[3])); break;
        case Kind::Polygon:
            for (std::size_t i = 0; i < o.count(); ++i) {
                const std::size_t j = (i + 1) % o.count();
                d = std::min(d, segmentDistance(x, y, p[2 * i], p[2 * i + 1], p[2 * j], p[2 * j + 1], false));
            }
            break;
        }
        // A parita' vince l'oggetto aggiunto per ultimo (quello disegnato sopra).
        if (d <= bestD) { bestD = d; best = static_cast<int>(k); }
    }
    return best;
}
void move(Object& o, double dx, double dy) noexcept {
    for (std::size_t i = 0; i + 1 < o.xy.size(); i += 2) { o.xy[i] += dx; o.xy[i + 1] += dy; }
}
std::vector<std::string> measure(const Object& o) {
    std::vector<std::string> out;
    const auto& p = o.xy;
    switch (o.kind) {
    case Kind::Point: out.push_back("(" + fmt(p[0]) + ", " + fmt(p[1]) + ")"); break;
    case Kind::Segment:
        out.push_back("Lunghezza=" + fmt(distance(p[0], p[1], p[2], p[3])));
        out.push_back("Angolo=" + fmt(std::atan2(p[3] - p[1], p[2] - p[0]) * 180 / Pi) + "\xC2\xB0");
        out.push_back("(" + fmt(p[0]) + ", " + fmt(p[1]) + ") (" + fmt(p[2]) + ", " + fmt(p[3]) + ")");
        break;
    case Kind::Line: {
        const double dx = p[2] - p[0];
        if (dx == 0) out.push_back("X=" + fmt(p[0]));
        else { const double m = (p[3] - p[1]) / dx; out.push_back("Y=" + fmt(m) + "X" + (p[1] - m * p[0] >= 0 ? "+" : "") + fmt(p[1] - m * p[0])); }
        out.push_back("Angolo=" + fmt(std::atan2(p[3] - p[1], dx) * 180 / Pi) + "\xC2\xB0");
        break;
    }
    case Kind::Circle: {
        const double r = distance(p[0], p[1], p[2], p[3]);
        out.push_back("Centro=(" + fmt(p[0]) + ", " + fmt(p[1]) + ")");
        out.push_back("Raggio=" + fmt(r));
        out.push_back("Circonferenza=" + fmt(2 * Pi * r));
        out.push_back("Area=" + fmt(Pi * r * r));
        break;
    }
    case Kind::Polygon: {
        double per = 0;
        const std::size_t n = o.count();
        for (std::size_t i = 0; i < n; ++i) { const std::size_t j = (i + 1) % n; per += distance(p[2 * i], p[2 * i + 1], p[2 * j], p[2 * j + 1]); }
        out.push_back("Area=" + fmt(polygonArea(o)));
        out.push_back("Perimetro=" + fmt(per));
        std::string angles = "Angoli:";
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t a = (i + n - 1) % n, b = (i + 1) % n;
            angles += " " + fmt(angleDegrees(p[2 * a], p[2 * a + 1], p[2 * i], p[2 * i + 1], p[2 * b], p[2 * b + 1]));
        }
        out.push_back(angles);
        break;
    }
    }
    return out;
}
void toScreen(const View& v, int w, int h, double x, double y, double& sx, double& sy) noexcept {
    sx = w / 2.0 + (x - v.cx) * v.scale;
    sy = h / 2.0 - (y - v.cy) * v.scale;
}
void toPlane(const View& v, int w, int h, double sx, double sy, double& x, double& y) noexcept {
    x = v.cx + (sx - w / 2.0) / v.scale;
    y = v.cy - (sy - h / 2.0) / v.scale;
}
void zoom(View& v, double factor) noexcept { v.scale = std::clamp(v.scale * factor, 0.01, 10000.0); }
void reset(State& s) { s = State{}; }
namespace {
void writeObjects(codec::Writer& w, const State& s) {
    w.u16(static_cast<std::uint16_t>(s.objects.size()));
    for (const Object& o : s.objects) {
        w.u8(static_cast<std::uint8_t>(o.kind));
        w.u8(static_cast<std::uint8_t>(o.count()));
        for (double v : o.xy) w.f64(v);
    }
    w.f64(s.view.cx); w.f64(s.view.cy); w.f64(s.view.scale);
}
bool readObjects(codec::Reader& r, State& t) {
    const std::size_t n = r.u16();
    if (n > MaxObjects) return false;
    for (std::size_t k = 0; k < n && r.ok; ++k) {
        Object o;
        const std::uint8_t kind = r.u8(), count = r.u8();
        if (kind > 4) return false;
        o.kind = static_cast<Kind>(kind);
        o.xy.resize(2u * count);
        for (double& v : o.xy) v = r.f64();
        if (!r.ok || !validate(o).empty()) return false;
        t.objects.push_back(std::move(o));
    }
    t.view.cx = r.f64(); t.view.cy = r.f64(); t.view.scale = r.f64();
    return r.ok && std::isfinite(t.view.cx) && std::isfinite(t.view.cy) && t.view.scale >= 0.01 && t.view.scale <= 10000;
}
} // namespace
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1);
    writeObjects(w, s);
    w.str(s.file);
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    if (!readObjects(r, t)) return false;
    t.file = r.str(64);
    if (!r.end()) return false;
    s = std::move(t);
    return true;
}
std::vector<std::uint8_t> encodeFile(const State& s) {
    std::vector<std::uint8_t> out;
    codec::Writer w{out};
    w.u8(1);
    writeObjects(w, s);
    return out;
}
bool decodeFile(const std::uint8_t* data, std::size_t size, State& out) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    if (!readObjects(r, t) || !r.end()) return false;
    t.file = out.file;
    out = std::move(t);
    return true;
}
} // namespace fx::geo
