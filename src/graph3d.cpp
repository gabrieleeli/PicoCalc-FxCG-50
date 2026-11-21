// 3D Graph drawing library (v1.14.0): lattices, exact clipping to the box,
// projection with the V-Window angles θ/φ, z-buffer and painter's renderers,
// box, labels, axes indicator, cross sections and overlays. Derived from the
// v1.9.0 surface renderer, extended to every graph type of the fx-CG50.
#include "graph3d.hpp"

#include "font_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>

namespace fx::g3d {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double DepthMin = -1.8, DepthRange = 3.6;      // depth of the clipped cube is within ±sqrt(3)
constexpr double DepthQ = 65000.0 / DepthRange;
constexpr int WireBias = 160;                           // lines drawn on a surface stay visible
constexpr double Huge = 1e30;                           // larger coordinates are treated as undefined

struct P3 { double x, y, z; };
struct Proj { float x, y, depth; std::uint8_t code; bool valid; };

std::uint8_t outcode(double x, double y, double z) noexcept {
    constexpr double e = 1e-9;
    return static_cast<std::uint8_t>((x < -1 - e ? 1 : 0) | (x > 1 + e ? 2 : 0) | (y < -1 - e ? 4 : 0) | (y > 1 + e ? 8 : 0) |
                                     (z < -1 - e ? 16 : 0) | (z > 1 + e ? 32 : 0));
}
double coord(const P3& p, int axis) noexcept { return axis == 0 ? p.x : axis == 1 ? p.y : p.z; }

// Sutherland-Hodgman against the six faces of the cube [-1, 1]^3.
std::size_t clipPolygon(P3* poly, std::size_t n, P3* tmp) noexcept {
    for (int axis = 0; axis < 3 && n > 0; ++axis)
        for (int sign = -1; sign <= 1 && n > 0; sign += 2) {
            std::size_t m = 0;
            for (std::size_t k = 0; k < n; ++k) {
                const P3& a = poly[k];
                const P3& b = poly[(k + 1) % n];
                const double da = 1.0 - sign * coord(a, axis), db = 1.0 - sign * coord(b, axis);   // >= 0 inside
                if (da >= 0) tmp[m++] = a;
                if ((da >= 0) != (db >= 0)) {
                    const double t = da / (da - db);
                    tmp[m++] = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
                }
            }
            for (std::size_t k = 0; k < m; ++k) poly[k] = tmp[k];
            n = m;
        }
    return n;
}
// Liang-Barsky of the segment a-b against the cube; false if outside.
bool clipSegmentCube(P3& a, P3& b) noexcept {
    double t0 = 0, t1 = 1;
    const double d[3] = {b.x - a.x, b.y - a.y, b.z - a.z};
    const double p[3] = {a.x, a.y, a.z};
    for (int k = 0; k < 3; ++k) {
        if (d[k] == 0) { if (p[k] < -1 - 1e-9 || p[k] > 1 + 1e-9) return false; continue; }
        double ta = (-1 - p[k]) / d[k], tb = (1 - p[k]) / d[k];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta); t1 = std::min(t1, tb);
        if (t0 > t1) return false;
    }
    const P3 a0 = a;
    a = {a0.x + d[0] * t0, a0.y + d[1] * t0, a0.z + d[2] * t0};
    b = {a0.x + d[0] * t1, a0.y + d[1] * t1, a0.z + d[2] * t1};
    return true;
}
bool clipSegmentBox(const Window& w, Vec3& a, Vec3& b) noexcept {
    double t0 = 0, t1 = 1;
    const double d[3] = {b.x - a.x, b.y - a.y, b.z - a.z}, p[3] = {a.x, a.y, a.z};
    const double lo[3] = {w.xmin, w.ymin, w.zmin}, hi[3] = {w.xmax, w.ymax, w.zmax};
    for (int k = 0; k < 3; ++k) {
        const double tol = 1e-9 * (hi[k] - lo[k]);
        if (d[k] == 0) { if (p[k] < lo[k] - tol || p[k] > hi[k] + tol) return false; continue; }
        double ta = (lo[k] - p[k]) / d[k], tb = (hi[k] - p[k]) / d[k];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta); t1 = std::min(t1, tb);
        if (t0 > t1) return false;
    }
    const Vec3 a0 = a, dv = b - a;
    a = a0 + dv * t0; b = a0 + dv * t1;
    return true;
}

std::uint16_t quant(double depth) noexcept {
    const double q = std::clamp((depth - DepthMin) * DepthQ, 0.0, 65534.0);
    return static_cast<std::uint16_t>(q);
}
float edge(const ScreenPoint& a, const ScreenPoint& b, float px, float py) noexcept {
    return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}
// Flat triangle with per-pixel depth test; visible runs become horizontal lines.
bool sane(const ScreenPoint& p) noexcept {   // finite and small enough to be converted to int
    return std::fabs(p.x) < 1e6F && std::fabs(p.y) < 1e6F && std::isfinite(p.depth);
}
void fillTriangleZ(gfx::Canvas& c, std::uint16_t* zb, const gfx::Rect& vp, const ScreenPoint& a, const ScreenPoint& b,
                   const ScreenPoint& d, std::uint16_t color) noexcept {
    if (!sane(a) || !sane(b) || !sane(d)) return;
    const float area = edge(a, b, d.x, d.y);
    if (!(std::fabs(area) >= 1e-6F)) return;
    const int minX = std::max(vp.x, static_cast<int>(std::floor(std::min({a.x, b.x, d.x}))));
    const int maxX = std::min(vp.x + vp.w - 1, static_cast<int>(std::ceil(std::max({a.x, b.x, d.x}))));
    const int minY = std::max(vp.y, static_cast<int>(std::floor(std::min({a.y, b.y, d.y}))));
    const int maxY = std::min(vp.y + vp.h - 1, static_cast<int>(std::ceil(std::max({a.y, b.y, d.y}))));
    for (int y = minY; y <= maxY; ++y) {
        int runStart = -1;
        const float py = static_cast<float>(y) + 0.5F;
        for (int x = minX; x <= maxX + 1; ++x) {
            bool draw = false;
            if (x <= maxX) {
                const float px = static_cast<float>(x) + 0.5F;
                const float w0 = edge(b, d, px, py) / area, w1 = edge(d, a, px, py) / area, w2 = 1.0F - w0 - w1;
                if (w0 >= -1e-4F && w1 >= -1e-4F && w2 >= -1e-4F) {
                    const std::uint16_t zq = quant(w0 * a.depth + w1 * b.depth + w2 * d.depth);
                    std::uint16_t& slot = zb[static_cast<std::size_t>(y - vp.y) * static_cast<std::size_t>(vp.w) + static_cast<std::size_t>(x - vp.x)];
                    if (zq < slot) { slot = zq; draw = true; }
                }
            }
            if (draw) { if (runStart < 0) runStart = x; }
            else if (runStart >= 0) { c.hLine(runStart, y, x - runStart, color); runStart = -1; }
        }
    }
}
// Line with an optional depth test (zb == nullptr: always drawn); width 1 or 2.
void lineZ(gfx::Canvas& c, const std::uint16_t* zb, const gfx::Rect& vp, ScreenPoint a, ScreenPoint b, std::uint16_t color,
           int width, int bias) noexcept {
    if (!sane(a) || !sane(b)) return;
    const float dx = b.x - a.x, dy = b.y - a.y;
    const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::fabs(dx), std::fabs(dy)))));
    if (steps > 4096) return;
    const bool steep = std::fabs(dy) > std::fabs(dx);
    for (int k = 0; k <= steps; ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(steps);
        const int px = static_cast<int>(std::floor(a.x + dx * t)), py = static_cast<int>(std::floor(a.y + dy * t));
        const std::uint16_t q = quant(a.depth + (b.depth - a.depth) * t);
        for (int w = 0; w < width; ++w) {
            const int x = px + (steep ? w : 0), y = py + (steep ? 0 : w);
            if (x < vp.x || y < vp.y || x >= vp.x + vp.w || y >= vp.y + vp.h) continue;
            if (zb && static_cast<int>(q) - bias > static_cast<int>(zb[static_cast<std::size_t>(y - vp.y) * static_cast<std::size_t>(vp.w) + static_cast<std::size_t>(x - vp.x)])) continue;
            c.pixel(x, y, color);
        }
    }
}
std::uint16_t shade(std::uint16_t c, double k) noexcept {
    if (!std::isfinite(k)) k = 1.0;                       // degenerate normal (fuzzing: huge coordinates)
    k = std::clamp(k, 0.0, 1.0);
    const auto r = static_cast<int>(((c >> 11) & 31) * k), g = static_cast<int>(((c >> 5) & 63) * k), b = static_cast<int>((c & 31) * k);
    return static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
}
gfx::Point toPoint(const ScreenPoint& p) noexcept {
    return {static_cast<int>(std::lround(std::clamp(p.x, -30000.0F, 30000.0F))), static_cast<int>(std::lround(std::clamp(p.y, -30000.0F, 30000.0F)))};
}
// Point with the coordinate `axis` = value and the other two (b, c) in x, y, z order.
Vec3 onPlane(Axis axis, double value, double b, double c) noexcept {
    switch (axis) {
    case Axis::X: return {value, b, c};
    case Axis::Y: return {b, value, c};
    case Axis::Z: break;
    }
    return {b, c, value};
}
double component(Vec3 v, Axis axis) noexcept { return axis == Axis::X ? v.x : axis == Axis::Y ? v.y : v.z; }
Vec3 axisVector(Axis axis) noexcept { return axis == Axis::X ? Vec3{1, 0, 0} : axis == Axis::Y ? Vec3{0, 1, 0} : Vec3{0, 0, 1}; }
void otherRanges(const Window& w, Axis axis, double& b0, double& b1, double& c0, double& c1) noexcept {
    switch (axis) {
    case Axis::X: b0 = w.ymin; b1 = w.ymax; c0 = w.zmin; c1 = w.zmax; return;
    case Axis::Y: b0 = w.xmin; b1 = w.xmax; c0 = w.zmin; c1 = w.zmax; return;
    case Axis::Z: break;
    }
    b0 = w.xmin; b1 = w.xmax; c0 = w.ymin; c1 = w.ymax;
}
void range(const Window& w, Axis axis, double& lo, double& hi) noexcept {
    lo = axis == Axis::X ? w.xmin : axis == Axis::Y ? w.ymin : w.zmin;
    hi = axis == Axis::X ? w.xmax : axis == Axis::Y ? w.ymax : w.zmax;
}
void formatValue(char* buf, std::size_t n, double v) noexcept {
    if (std::fabs(v) < 1e-12) v = 0;
    std::snprintf(buf, n, "%.6g", v);
}
} // namespace

// ---------------------------------------------------------------- lattices
double latticeAt(double a, double b, std::size_t i, std::size_t n) noexcept {
    if (n < 2) return a;
    if (i + 1 >= n) return b;
    return a + (b - a) * static_cast<double>(i) / static_cast<double>(n - 1);
}

void beginSample(Mesh& mesh, std::size_t nu, std::size_t nv) {
    mesh.clear();
    nu = std::max<std::size_t>(nu, 2);
    nv = std::max<std::size_t>(nv, 2);
    try {
        mesh.verts.resize(nu * nv, Vertex{0.0F, 0.0F, 0.0F, false});
    } catch (...) {
        mesh.clear();
        throw;
    }
    mesh.nu = nu;
    mesh.nv = nv;
}

bool sampleStep(Mesh& mesh, const PointFn& f, const Domain& d, std::size_t& next, std::size_t budget) {
    const std::size_t total = mesh.verts.size();
    if (mesh.nu == 0) return true;
    for (; next < total && budget > 0; ++next, --budget) {
        const std::size_t i = next % mesh.nu, j = next / mesh.nu;
        Vec3 p;
        Vertex v{0.0F, 0.0F, 0.0F, false};
        if (f(latticeAt(d.u0, d.u1, i, mesh.nu), latticeAt(d.v0, d.v1, j, mesh.nv), p) && std::isfinite(p.x) && std::isfinite(p.y) &&
            std::isfinite(p.z) && std::fabs(p.x) < Huge && std::fabs(p.y) < Huge && std::fabs(p.z) < Huge) {
            v = {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z), true};
            if (mesh.valid == 0) { mesh.zmin = mesh.zmax = p.z; }
            else { mesh.zmin = std::min(mesh.zmin, p.z); mesh.zmax = std::max(mesh.zmax, p.z); }
            ++mesh.valid;
        }
        mesh.verts[next] = v;
    }
    return next >= total;
}

void sample(Mesh& mesh, const PointFn& f, const Domain& d, std::size_t nu, std::size_t nv) {
    beginSample(mesh, nu, nv);
    std::size_t next = 0;
    (void)sampleStep(mesh, f, d, next, mesh.verts.size());
}

bool inBox(const Window& w, Vec3 p) noexcept {
    const double ex = 1e-9 * (w.xmax - w.xmin), ey = 1e-9 * (w.ymax - w.ymin), ez = 1e-9 * (w.zmax - w.zmin);
    return p.x >= w.xmin - ex && p.x <= w.xmax + ex && p.y >= w.ymin - ey && p.y <= w.ymax + ey && p.z >= w.zmin - ez && p.z <= w.zmax + ez;
}

bool clipLine(const Window& w, Vec3 p, Vec3 d, double& t0, double& t1) noexcept {
    t0 = -1e300; t1 = 1e300;
    const double dd[3] = {d.x, d.y, d.z}, pp[3] = {p.x, p.y, p.z};
    const double lo[3] = {w.xmin, w.ymin, w.zmin}, hi[3] = {w.xmax, w.ymax, w.zmax};
    bool any = false;
    for (int k = 0; k < 3; ++k) {
        if (dd[k] == 0) { if (pp[k] < lo[k] || pp[k] > hi[k]) return false; continue; }
        any = true;
        double ta = (lo[k] - pp[k]) / dd[k], tb = (hi[k] - pp[k]) / dd[k];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta); t1 = std::min(t1, tb);
        if (t0 > t1) return false;
    }
    return any && std::isfinite(t0) && std::isfinite(t1);
}

std::size_t templatePatches(const Shape& s, const Window& w, std::size_t nu, std::size_t nv, PatchSpec out[MaxPatches]) {
    const double two = 2 * kPi;
    switch (s.kind) {
    case Kind::Plane: {
        // Bounding rectangle, in the plane axes, of the plane's section of the box.
        double u0 = 1e300, u1 = -1e300, v0 = 1e300, v1 = -1e300;
        bool any = false;
        auto add = [&](Vec3 q) {
            const Vec3 r = q - s.p;
            const double u = dot(r, s.u), v = dot(r, s.v);
            u0 = std::min(u0, u); u1 = std::max(u1, u); v0 = std::min(v0, v); v1 = std::max(v1, v);
            any = true;
        };
        auto corner = [&](int k) { return Vec3{k & 1 ? w.xmax : w.xmin, k & 2 ? w.ymax : w.ymin, k & 4 ? w.zmax : w.zmin}; };
        for (int a = 0; a < 8; ++a)
            for (int bit = 1; bit <= 4; bit <<= 1) {
                if (a & bit) continue;
                const Vec3 A = corner(a), B = corner(a | bit);
                const double fa = dot(s.n, A) - s.h, fb = dot(s.n, B) - s.h;
                if (fa == 0) add(A);
                if (fb == 0) add(B);
                if ((fa < 0 && fb > 0) || (fa > 0 && fb < 0)) add(A + (B - A) * (fa / (fa - fb)));
            }
        if (!any || !(u1 >= u0) || !(v1 >= v0)) return 0;
        out[0] = {{u0, u1, v0, v1}, nu, nv};                         // exact section; the clipping keeps the boundary
        return 1;
    }
    case Kind::Sphere:
        out[0] = {{0, two, -kPi / 2, kPi / 2}, nu, nv};
        return 1;
    case Kind::Cylinder:
        out[0] = {{0, two, s.z0, s.z1}, nu, nv};
        out[1] = {{0, two, 0, s.r}, nu, 2};
        out[2] = {{0, two, 0, s.r}, nu, 2};
        return 3;
    case Kind::Cone:
        out[0] = {{0, two, 0, 1}, nu, nv};
        out[1] = {{0, two, 0, s.r}, nu, 2};
        return 2;
    default:
        return 0;
    }
}

bool templatePoint(const Shape& s, std::size_t patch, double u, double v, Vec3& out) noexcept {
    switch (s.kind) {
    case Kind::Plane: out = s.p + s.u * u + s.v * v; return true;
    case Kind::Sphere: out = {s.p.x + s.r * std::cos(v) * std::cos(u), s.p.y + s.r * std::cos(v) * std::sin(u), s.p.z + s.r * std::sin(v)}; return true;
    case Kind::Cylinder:
        if (patch == 0) out = {s.cx + s.r * std::cos(u), s.cy + s.r * std::sin(u), v};
        else out = {s.cx + v * std::cos(u), s.cy + v * std::sin(u), patch == 1 ? s.z0 : s.z1};
        return true;
    case Kind::Cone:
        if (patch == 0) {
            const double rad = s.r * (1 - v);
            out = {s.cx + rad * std::cos(u), s.cy + rad * std::sin(u), s.z0 + v * (s.z1 - s.z0)};
        } else {
            out = {s.cx + v * std::cos(u), s.cy + v * std::sin(u), s.z0};
        }
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------- projection
Projector::Projector(const Window& w, const Camera& cam, gfx::Rect viewport) noexcept : w_(w), vp_(viewport) {
    const double th = cam.theta * kPi / 180.0, e = (90.0 - cam.phi) * kPi / 180.0;
    right_ = {-std::sin(th), std::cos(th), 0};
    up_ = {-std::cos(th) * std::sin(e), -std::sin(th) * std::sin(e), std::cos(e)};
    toward_ = {std::cos(th) * std::cos(e), std::sin(th) * std::cos(e), std::sin(e)};
    scale_ = 0.28 * std::min(vp_.w, vp_.h) * static_cast<double>(cam.zoom);
    cx_ = vp_.x + vp_.w * 0.5;
    cy_ = vp_.y + vp_.h * 0.5;
    sx_ = 2.0 / (w.xmax - w.xmin); sy_ = 2.0 / (w.ymax - w.ymin); sz_ = 2.0 / (w.zmax - w.zmin);
    ox_ = w.xmin; oy_ = w.ymin; oz_ = w.zmin;
}
void Projector::normalize(Vec3 p, double& nx, double& ny, double& nz) const noexcept {
    // Far outside the box (or not a number after an overflow): a finite point
    // in the same direction, which the clipping discards or cuts exactly.
    auto fix = [](double v) { return std::isnan(v) ? 1e12 : std::clamp(v, -1e12, 1e12); };
    nx = fix((p.x - ox_) * sx_ - 1.0); ny = fix((p.y - oy_) * sy_ - 1.0); nz = fix((p.z - oz_) * sz_ - 1.0);
}
ScreenPoint Projector::fromNormalized(double nx, double ny, double nz) const noexcept {
    const Vec3 n{nx, ny, nz};
    const double h = dot(n, right_), u = dot(n, up_), depth = -dot(n, toward_);
    return {static_cast<float>(cx_ + h * scale_), static_cast<float>(cy_ - u * scale_), static_cast<float>(depth)};
}
ScreenPoint Projector::project(Vec3 p) const noexcept {
    double x, y, z;
    normalize(p, x, y, z);
    return fromNormalized(x, y, z);
}

// ---------------------------------------------------------------- colours
std::uint16_t color565(Color c) noexcept {
    const std::uint32_t rgb = colorRgb(c);
    return gfx::rgb888To565(static_cast<std::uint8_t>(rgb >> 16), static_cast<std::uint8_t>(rgb >> 8), static_cast<std::uint8_t>(rgb));
}
void itemColors(const Function& f, Item& item) noexcept {
    const bool area = f.area != Color::Clear, line = f.line != Color::Clear;
    if (f.kind == Kind::Line) {
        item.hasFill = true;
        item.fill = color565(area ? f.area : f.line);
        item.hasWire = false;
        return;
    }
    item.hasFill = area;
    item.fill = color565(area ? f.area : f.line);
    item.hasWire = line;
    item.wire = color565(f.line);
}
std::uint16_t oppositeColor(Color area) noexcept {
    const std::uint32_t rgb = colorRgb(area == Color::Clear ? Color::Black : area);
    const std::uint32_t inv = 0xFFFFFFu ^ rgb;
    return gfx::rgb888To565(static_cast<std::uint8_t>(inv >> 16), static_cast<std::uint8_t>(inv >> 8), static_cast<std::uint8_t>(inv));
}

void rotate(Camera& cam, double dTheta, double dPhi) noexcept {
    cam.theta = normalizeTheta(cam.theta + dTheta);
    cam.phi = normalizePhi(cam.phi + dPhi);
}

// ---------------------------------------------------------------- render
RenderStats render(gfx::Canvas& canvas, const Item* items, std::size_t count, const Window& w, const Setup& setup,
                   const Camera& cam, gfx::Rect vp, bool zbuffer) {
    RenderStats st;
    st.zbuffer = zbuffer;
    if (vp.w <= 0 || vp.h <= 0) return st;
    std::pmr::memory_resource* heavy = mem::resource(mem::Policy::PREFER_PSRAM);
    const Projector proj(w, cam, vp);
    // Clipped polygon: a quadrilateral cut by the six faces has at most 4 + 6 = 10
    // vertices (Sutherland-Hodgman adds at most one per plane); one pair per frame.
    P3 poly[12], tmp[12];
    const gfx::Rect oldClip = canvas.clip();
    canvas.setClip(vp);
    const std::uint16_t boxColor = gfx::rgb888To565(40, 40, 40);
    const Vec3 toward = proj.towardViewer();
    const Vec3 light = [&] {
        Vec3 l = toward + proj.screenUp() * 0.6 - proj.screenRight() * 0.4;
        return l * (1.0 / norm(l));
    }();

    // Box corners (normalized cube) and the front / back edges for the painter.
    ScreenPoint corner[8];
    for (int k = 0; k < 8; ++k) corner[k] = proj.fromNormalized(k & 1 ? 1 : -1, k & 2 ? 1 : -1, k & 4 ? 1 : -1);
    struct EdgeRef { int a, b; bool front; };
    EdgeRef edges[12];
    {
        int n = 0;
        for (int a = 0; a < 8; ++a)
            for (int bit = 1; bit <= 4; bit <<= 1) {
                if (a & bit) continue;
                bool front = false;
                for (int other = 1; other <= 4; other <<= 1) {   // the two faces that contain the edge
                    if (other == bit) continue;
                    const int axis = other == 1 ? 0 : other == 2 ? 1 : 2;
                    const double sign = (a & other) ? 1.0 : -1.0;
                    const double t = axis == 0 ? toward.x : axis == 1 ? toward.y : toward.z;
                    front = front || sign * t > 1e-9;
                }
                edges[n++] = {a, a | bit, front};
            }
    }
    auto drawBox = [&](const std::uint16_t* zb, bool front, bool all) {
        if (!setup.box) return;
        for (const EdgeRef& e : edges)
            if (all || e.front == front) lineZ(canvas, zb, vp, corner[e.a], corner[e.b], boxColor, 1, WireBias);
    };

    // Project every lattice point.
    std::size_t total = 0;
    for (std::size_t s = 0; s < count; ++s)
        for (std::size_t p = 0; p < items[s].patchCount; ++p)
            if (items[s].patches[p] && !items[s].line) total += items[s].patches[p]->verts.size();
    std::pmr::vector<Proj> pv(heavy);
    pv.resize(total);
    st.bufferBytes = pv.size() * sizeof(Proj);
    std::pmr::vector<std::size_t> base(heavy);
    base.resize(count * MaxPatches, 0);
    {
        std::size_t k = 0;
        for (std::size_t s = 0; s < count; ++s)
            for (std::size_t p = 0; p < items[s].patchCount; ++p) {
                const Mesh* m = items[s].patches[p];
                base[s * MaxPatches + p] = k;
                if (!m || items[s].line) continue;
                for (const Vertex& v : m->verts) {
                    double x, y, z;
                    proj.normalize({v.x, v.y, v.z}, x, y, z);
                    const ScreenPoint sp = proj.fromNormalized(x, y, z);
                    pv[k++] = {sp.x, sp.y, sp.depth, outcode(x, y, z), v.valid};
                }
            }
    }
    auto normalized = [&](const Vertex& v) {
        P3 q;
        proj.normalize({v.x, v.y, v.z}, q.x, q.y, q.z);
        return q;
    };
    auto shaded = [&](std::uint16_t color, const P3& a, const P3& b, const P3& c) {
        const Vec3 u{b.x - a.x, b.y - a.y, b.z - a.z}, v{c.x - a.x, c.y - a.y, c.z - a.z};
        const Vec3 n = cross(u, v);
        const double len = norm(n);
        const double k = len > 0 ? std::fabs(dot(n, light)) / len : 1.0;
        return shade(color, 0.42 + 0.58 * k);
    };
    // Lattice edge of a patch clipped to the cube, as two screen points.
    auto wireEdge = [&](const Vertex& va, const Vertex& vb, ScreenPoint& sa, ScreenPoint& sb) {
        P3 a = normalized(va), b = normalized(vb);
        if (!clipSegmentCube(a, b)) return false;
        sa = proj.fromNormalized(a.x, a.y, a.z);
        sb = proj.fromNormalized(b.x, b.y, b.z);
        return true;
    };
    auto lineSegment = [&](const Item& it, double t0, double t1, ScreenPoint& sa, ScreenPoint& sb) {
        const Vec3 a = it.a + (it.b - it.a) * t0, b = it.a + (it.b - it.a) * t1;
        P3 na, nb;
        proj.normalize(a, na.x, na.y, na.z);
        proj.normalize(b, nb.x, nb.y, nb.z);
        if (!clipSegmentCube(na, nb)) return false;
        sa = proj.fromNormalized(na.x, na.y, na.z);
        sb = proj.fromNormalized(nb.x, nb.y, nb.z);
        return true;
    };

    if (zbuffer) {
        std::pmr::vector<std::uint16_t> zb(heavy);
        zb.assign(static_cast<std::size_t>(vp.w) * static_cast<std::size_t>(vp.h), 0xFFFF);
        st.bufferBytes += zb.size() * sizeof(std::uint16_t);
        std::uint16_t* depth = zb.data();
        for (std::size_t s = 0; s < count; ++s) {
            const Item& it = items[s];
            if (it.line) continue;
            for (std::size_t p = 0; p < it.patchCount; ++p) {
                const Mesh* m = it.patches[p];
                if (!m || m->nu < 2 || m->nv < 2) continue;
                const std::size_t b0 = base[s * MaxPatches + p], nu = m->nu;
                for (std::size_t j = 0; j + 1 < m->nv; ++j)
                    for (std::size_t i = 0; i + 1 < nu; ++i) {
                        const std::size_t q[4] = {j * nu + i, j * nu + i + 1, (j + 1) * nu + i + 1, (j + 1) * nu + i};
                        if (!pv[b0 + q[0]].valid || !pv[b0 + q[1]].valid || !pv[b0 + q[2]].valid || !pv[b0 + q[3]].valid) continue;
                        if (!it.hasFill) continue;
                        for (int t = 0; t < 2; ++t) {
                            const std::size_t tri[3] = {q[0], q[1 + t], q[2 + t]};
                            const Proj& a = pv[b0 + tri[0]];
                            const Proj& b = pv[b0 + tri[1]];
                            const Proj& c = pv[b0 + tri[2]];
                            if (a.code & b.code & c.code) continue;
                            poly[0] = normalized(m->verts[tri[0]]); poly[1] = normalized(m->verts[tri[1]]); poly[2] = normalized(m->verts[tri[2]]);
                            const std::uint16_t color = shaded(it.fill, poly[0], poly[1], poly[2]);
                            if ((a.code | b.code | c.code) == 0) {
                                fillTriangleZ(canvas, depth, vp, {a.x, a.y, a.depth}, {b.x, b.y, b.depth}, {c.x, c.y, c.depth}, color);
                                ++st.triangles;
                                continue;
                            }
                            const std::size_t n = clipPolygon(poly, 3, tmp);
                            if (n < 3) continue;
                            const ScreenPoint s0 = proj.fromNormalized(poly[0].x, poly[0].y, poly[0].z);
                            for (std::size_t k = 1; k + 1 < n; ++k) {
                                fillTriangleZ(canvas, depth, vp, s0, proj.fromNormalized(poly[k].x, poly[k].y, poly[k].z),
                                              proj.fromNormalized(poly[k + 1].x, poly[k + 1].y, poly[k + 1].z), color);
                                ++st.triangles;
                            }
                        }
                    }
            }
        }
        // Lattice lines and lines of the Line template, with the depth test.
        for (std::size_t s = 0; s < count; ++s) {
            const Item& it = items[s];
            if (it.line) {
                ScreenPoint a, b;
                if (lineSegment(it, 0, 1, a, b)) { lineZ(canvas, depth, vp, a, b, it.fill, 2, WireBias); ++st.lines; }
                continue;
            }
            if (!it.hasWire) continue;
            for (std::size_t p = 0; p < it.patchCount; ++p) {
                const Mesh* m = it.patches[p];
                if (!m) continue;
                const std::size_t nu = m->nu;
                for (std::size_t j = 0; j < m->nv; ++j)
                    for (std::size_t i = 0; i < nu; ++i) {
                        const Vertex& v = m->verts[j * nu + i];
                        if (!v.valid) continue;
                        ScreenPoint a, b;
                        if (i + 1 < nu && m->verts[j * nu + i + 1].valid && wireEdge(v, m->verts[j * nu + i + 1], a, b)) {
                            lineZ(canvas, depth, vp, a, b, it.wire, 1, WireBias); ++st.lines;
                        }
                        if (j + 1 < m->nv && m->verts[(j + 1) * nu + i].valid && wireEdge(v, m->verts[(j + 1) * nu + i], a, b)) {
                            lineZ(canvas, depth, vp, a, b, it.wire, 1, WireBias); ++st.lines;
                        }
                    }
            }
        }
        drawBox(depth, false, true);
    } else {
        // Painter's algorithm over the quadrilaterals of every patch and the
        // pieces of the lines: far primitives first.
        struct Order { float depth; std::uint32_t index; std::uint16_t i, j; std::uint8_t item, patch; };
        constexpr std::uint16_t LinePieces = 16;
        std::size_t prims = 0;
        for (std::size_t s = 0; s < count; ++s) {
            if (items[s].line) { prims += LinePieces; continue; }
            for (std::size_t p = 0; p < items[s].patchCount; ++p)
                if (const Mesh* m = items[s].patches[p]) prims += (m->nu > 0 ? m->nu - 1 : 0) * (m->nv > 0 ? m->nv - 1 : 0);
        }
        std::pmr::vector<Order> order(heavy);
        order.reserve(prims);
        st.bufferBytes += prims * sizeof(Order);
        for (std::size_t s = 0; s < count; ++s) {
            const Item& it = items[s];
            if (it.line) {
                for (std::uint16_t k = 0; k < LinePieces; ++k) {
                    const double t = (k + 0.5) / LinePieces;
                    order.push_back({proj.project(it.a + (it.b - it.a) * t).depth, 0, k, 0, static_cast<std::uint8_t>(s), 0xFF});
                }
                continue;
            }
            for (std::size_t p = 0; p < it.patchCount; ++p) {
                const Mesh* m = it.patches[p];
                if (!m) continue;
                const std::size_t b0 = base[s * MaxPatches + p], nu = m->nu;
                for (std::size_t j = 0; j + 1 < m->nv; ++j)
                    for (std::size_t i = 0; i + 1 < nu; ++i) {
                        const Proj* q[4] = {&pv[b0 + j * nu + i], &pv[b0 + j * nu + i + 1], &pv[b0 + (j + 1) * nu + i + 1], &pv[b0 + (j + 1) * nu + i]};
                        if (!q[0]->valid || !q[1]->valid || !q[2]->valid || !q[3]->valid) continue;
                        if (q[0]->code & q[1]->code & q[2]->code & q[3]->code) continue;
                        order.push_back({q[0]->depth + q[1]->depth + q[2]->depth + q[3]->depth, static_cast<std::uint32_t>(b0),
                                         static_cast<std::uint16_t>(i), static_cast<std::uint16_t>(j), static_cast<std::uint8_t>(s),
                                         static_cast<std::uint8_t>(p)});
                    }
            }
        }
        for (Order& o : order) if (o.patch == 0xFF) o.depth *= 4.0F;   // same scale as the sums of four depths
        std::sort(order.begin(), order.end(), [](const Order& a, const Order& b) { return a.depth > b.depth; });
        drawBox(nullptr, false, false);
        for (const Order& o : order) {
            const Item& it = items[o.item];
            if (o.patch == 0xFF) {
                ScreenPoint a, b;
                if (lineSegment(it, static_cast<double>(o.i) / LinePieces, static_cast<double>(o.i + 1) / LinePieces, a, b)) {
                    lineZ(canvas, nullptr, vp, a, b, it.fill, 2, 0); ++st.lines;
                }
                continue;
            }
            const Mesh& m = *it.patches[o.patch];
            const std::size_t nu = m.nu;
            const std::size_t idx[4] = {o.j * nu + o.i, o.j * nu + o.i + 1, (o.j + 1u) * nu + o.i + 1, (o.j + 1u) * nu + o.i};
            if (it.hasFill) {
                for (int k = 0; k < 4; ++k) poly[k] = normalized(m.verts[idx[k]]);
                const std::uint16_t color = shaded(it.fill, poly[0], poly[1], poly[2]);
                const std::size_t n = clipPolygon(poly, 4, tmp);
                if (n >= 3) {
                    gfx::Point pts[12];
                    for (std::size_t k = 0; k < n; ++k) pts[k] = toPoint(proj.fromNormalized(poly[k].x, poly[k].y, poly[k].z));
                    canvas.polygon(pts, n, color, true);
                    st.triangles += n - 2;
                }
            }
            if (it.hasWire) {
                for (int k = 0; k < 4; ++k) {
                    ScreenPoint a, b;
                    if (wireEdge(m.verts[idx[k]], m.verts[idx[(k + 1) % 4]], a, b)) { lineZ(canvas, nullptr, vp, a, b, it.wire, 1, 0); ++st.lines; }
                }
            }
        }
        drawBox(nullptr, true, false);
    }

    // Labels (Setup Label): axis names and the window values on three edges.
    if (setup.label) {
        const std::uint16_t ink = gfx::rgb888To565(20, 20, 20);
        const double cxs = vp.x + vp.w * 0.5, cys = vp.y + vp.h * 0.5;
        const double lo[3] = {w.xmin, w.ymin, w.zmin}, hi[3] = {w.xmax, w.ymax, w.zmax};
        static const char* const names[3] = {"X", "Y", "Z"};
        for (int axis = 0; axis < 3; ++axis) {
            const int bit = 1 << axis;
            int best = -1;
            double bestScore = -1e300;
            for (int a = 0; a < 8; ++a) {
                if (a & bit) continue;
                const ScreenPoint &p0 = corner[a], &p1 = corner[a | bit];
                const double mx = (p0.x + p1.x) * 0.5, my = (p0.y + p1.y) * 0.5;
                const double score = axis == 2 ? -mx : my;   // Z on the left edge, X and Y on the lowest edges
                if (score > bestScore) { bestScore = score; best = a; }
            }
            const ScreenPoint &p0 = corner[best], &p1 = corner[best | bit];
            double ox = (p0.x + p1.x) * 0.5 - cxs, oy = (p0.y + p1.y) * 0.5 - cys;
            const double ol = std::sqrt(ox * ox + oy * oy);
            if (ol > 1e-6) { ox /= ol; oy /= ol; } else { ox = 0; oy = 1; }
            auto label = [&](double x, double y, const char* text) {
                const font::TextMetrics m = font::measure(font::Face::Small, text);
                const int tx = static_cast<int>(std::lround(x + ox * 9 - m.width * 0.5)), ty = static_cast<int>(std::lround(y + oy * 9 - m.height * 0.5));
                font::draw(canvas, font::Face::Small, tx, ty, text, ink);
            };
            label((p0.x + p1.x) * 0.5, (p0.y + p1.y) * 0.5, names[axis]);
            char buf[24];
            formatValue(buf, sizeof buf, lo[axis]); label(p0.x, p0.y, buf);
            formatValue(buf, sizeof buf, hi[axis]); label(p1.x, p1.y, buf);
        }
    }
    // Axes indicator (Setup Axes): x, y, z directions in the upper right corner.
    if (setup.axes) {
        const std::uint16_t ink = gfx::rgb888To565(60, 60, 60);
        const int cx = vp.x + vp.w - 26, cy = vp.y + 26;
        static const char* const names[3] = {"x", "y", "z"};
        const Vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        for (int k = 0; k < 3; ++k) {
            const double dx = dot(axes[k], proj.screenRight()), dy = -dot(axes[k], proj.screenUp());
            const int ex = cx + static_cast<int>(std::lround(dx * 14)), ey = cy + static_cast<int>(std::lround(dy * 14));
            canvas.line(cx, cy, ex, ey, ink);
            font::draw(canvas, font::Face::Small, cx + static_cast<int>(std::lround(dx * 20)) - 3, cy + static_cast<int>(std::lround(dy * 20)) - 6, names[k], ink);
        }
    }
    canvas.setClip(oldClip);
    st.ok = true;
    return st;
}

// ---------------------------------------------------------------- overlays
void drawSegment(gfx::Canvas& c, const Projector& p, const Window& w, Vec3 a, Vec3 b, std::uint16_t color, int width) {
    if (!clipSegmentBox(w, a, b)) return;
    lineZ(c, nullptr, p.viewport(), p.project(a), p.project(b), color, width, 0);
}
void drawPolyline(gfx::Canvas& c, const Projector& p, const Window& w, const Vec3* pts, std::size_t n, bool closed,
                  std::uint16_t color, int width) {
    if (n == 0) return;
    if (n == 1) {
        if (!inBox(w, pts[0])) return;
        const gfx::Point q = toPoint(p.project(pts[0]));
        c.rect(q.x - 2, q.y - 2, 5, 5, color, true);
        return;
    }
    for (std::size_t k = 0; k + 1 < n; ++k) drawSegment(c, p, w, pts[k], pts[k + 1], color, width);
    if (closed && n > 2) drawSegment(c, p, w, pts[n - 1], pts[0], color, width);
}
void drawPointer(gfx::Canvas& c, int x, int y, std::uint16_t color) {
    c.hLine(x - 5, y, 4, color); c.hLine(x + 2, y, 4, color);
    c.vLine(x, y - 5, 4, color); c.vLine(x, y + 2, 4, color);
    c.rect(x - 1, y - 1, 3, 3, color, false);
}

std::size_t crossSection(const Shape& s, const Window& w, Axis axis, double value, Polyline out[MaxSectionCurves], bool& coincident) {
    coincident = false;
    std::size_t n = 0;
    const Vec3 e = axisVector(axis);
    double b0, b1, c0, c1;
    otherRanges(w, axis, b0, b1, c0, c1);
    auto curve = [&]() -> Polyline& { out[n].pts.clear(); out[n].closed = false; return out[n++]; };
    const double scale = std::max({1.0, std::fabs(value), std::fabs(s.p.x), std::fabs(s.p.y), std::fabs(s.p.z)});
    constexpr int Circle = 64;
    switch (s.kind) {
    case Kind::Line: {
        const double d = component(s.d, axis), p = component(s.p, axis);
        if (std::fabs(d) > 1e-12 * norm(s.d)) {
            const Vec3 q = s.p + s.d * ((value - p) / d);
            if (inBox(w, q)) curve().pts.push_back(q);
        } else if (std::fabs(p - value) <= 1e-9 * scale) {
            double t0, t1;
            if (clipLine(w, s.p, s.d, t0, t1)) { Polyline& l = curve(); l.pts.push_back(s.p + s.d * t0); l.pts.push_back(s.p + s.d * t1); }
        }
        break;
    }
    case Kind::Plane: {
        const Vec3 dir = cross(s.n, e);
        if (norm(dir) < 1e-10) {
            const double h = dot(s.n, e) < 0 ? -s.h : s.h;
            if (std::fabs(h - value) <= 1e-9 * scale) {
                coincident = true;
                Polyline& l = curve();
                l.closed = true;
                l.pts.push_back(onPlane(axis, value, b0, c0)); l.pts.push_back(onPlane(axis, value, b1, c0));
                l.pts.push_back(onPlane(axis, value, b1, c1)); l.pts.push_back(onPlane(axis, value, b0, c1));
            }
            break;
        }
        const Vec3 point = (cross(e, dir) * s.h + cross(dir, s.n) * value) * (1.0 / dot(dir, dir));
        double t0, t1;
        if (clipLine(w, point, dir, t0, t1)) { Polyline& l = curve(); l.pts.push_back(point + dir * t0); l.pts.push_back(point + dir * t1); }
        break;
    }
    case Kind::Sphere: {
        const double d = value - component(s.p, axis);
        if (std::fabs(d) > s.r) break;
        const double rho = std::sqrt(std::max(0.0, s.r * s.r - d * d));
        const double cb = axis == Axis::X ? s.p.y : s.p.x, cc = axis == Axis::Z ? s.p.y : s.p.z;
        Polyline& l = curve();
        if (rho <= 1e-12 * s.r) { l.pts.push_back(onPlane(axis, value, cb, cc)); break; }
        l.closed = true;
        for (int k = 0; k < Circle; ++k) {
            const double t = 2 * kPi * k / Circle;
            l.pts.push_back(onPlane(axis, value, cb + rho * std::cos(t), cc + rho * std::sin(t)));
        }
        break;
    }
    case Kind::Cylinder: {
        if (axis == Axis::Z) {
            if (value < s.z0 || value > s.z1) break;
            Polyline& l = curve();
            l.closed = true;
            for (int k = 0; k < Circle; ++k) {
                const double t = 2 * kPi * k / Circle;
                l.pts.push_back({s.cx + s.r * std::cos(t), s.cy + s.r * std::sin(t), value});
            }
            break;
        }
        const double d = value - (axis == Axis::X ? s.cx : s.cy), c = axis == Axis::X ? s.cy : s.cx;
        if (std::fabs(d) > s.r) break;
        const double h = std::sqrt(std::max(0.0, s.r * s.r - d * d));
        Polyline& l = curve();
        l.closed = true;
        l.pts.push_back(onPlane(axis, value, c - h, s.z0)); l.pts.push_back(onPlane(axis, value, c + h, s.z0));
        l.pts.push_back(onPlane(axis, value, c + h, s.z1)); l.pts.push_back(onPlane(axis, value, c - h, s.z1));
        break;
    }
    case Kind::Cone: {
        const double zlo = std::min(s.z0, s.z1), zhi = std::max(s.z0, s.z1);
        auto radius = [&](double z) { return s.r * (s.z1 - z) / (s.z1 - s.z0); };
        if (axis == Axis::Z) {
            if (value < zlo || value > zhi) break;
            const double rho = radius(value);
            Polyline& l = curve();
            if (rho <= 1e-12 * s.r) { l.pts.push_back({s.cx, s.cy, value}); break; }
            l.closed = true;
            for (int k = 0; k < Circle; ++k) {
                const double t = 2 * kPi * k / Circle;
                l.pts.push_back({s.cx + rho * std::cos(t), s.cy + rho * std::sin(t), value});
            }
            break;
        }
        const double d = value - (axis == Axis::X ? s.cx : s.cy), c = axis == Axis::X ? s.cy : s.cx;
        if (std::fabs(d) > s.r) break;
        // Conic section: from the base up to the height where the radius is |d|.
        const double zs = s.z1 - std::fabs(d) / s.r * (s.z1 - s.z0);
        constexpr int Steps = 32;
        Polyline& l = curve();
        l.closed = true;
        for (int k = 0; k <= Steps; ++k) {
            const double z = s.z0 + (zs - s.z0) * k / Steps, rho = radius(z);
            l.pts.push_back(onPlane(axis, value, c + std::sqrt(std::max(0.0, rho * rho - d * d)), z));
        }
        for (int k = Steps; k >= 0; --k) {
            const double z = s.z0 + (zs - s.z0) * k / Steps, rho = radius(z);
            const double h = std::sqrt(std::max(0.0, rho * rho - d * d));
            if (k == Steps && h <= 1e-12 * s.r) continue;   // apex of the section already added
            l.pts.push_back(onPlane(axis, value, c - h, z));
        }
        break;
    }
    default:
        break;
    }
    return n;
}

void drawSectionPlane(gfx::Canvas& c, const Projector& p, const Window& w, Axis axis, double value, std::uint16_t color) {
    double lo, hi;
    range(w, axis, lo, hi);
    if (value < lo || value > hi) return;
    double b0, b1, c0, c1;
    otherRanges(w, axis, b0, b1, c0, c1);
    constexpr int Lines = 10;
    for (int k = 0; k <= Lines; ++k) {
        const double b = b0 + (b1 - b0) * k / Lines, cc = c0 + (c1 - c0) * k / Lines;
        drawSegment(c, p, w, onPlane(axis, value, b, c0), onPlane(axis, value, b, c1), color, 1);
        drawSegment(c, p, w, onPlane(axis, value, b0, cc), onPlane(axis, value, b1, cc), color, 1);
    }
}

} // namespace fx::g3d
