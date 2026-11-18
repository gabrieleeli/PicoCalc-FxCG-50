// 3D Graph model (v1.14.0): validation of the manual, canonical shapes of the
// templates, analytic INTSECT / RELATION and the compact binary form used by
// the 3D G-Mem and by the workspace record v10. See graph3d_model.hpp.
#include "graph3d_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fx::g3d {
namespace {
constexpr double Eps = 1e-10;        // tolerance on unit vectors (parallel, perpendicular)
constexpr double DistEps = 1e-9;     // relative tolerance on distances

bool zero(Vec3 a) noexcept { return a.x == 0 && a.y == 0 && a.z == 0; }
bool same(Vec3 a, Vec3 b) noexcept { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool finite(Vec3 a) noexcept { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
Vec3 unit(Vec3 a) noexcept { const double n = norm(a); return n > 0 ? a * (1.0 / n) : a; }
Vec3 at(const Function& f, std::size_t i) noexcept { return {f.coef[i], f.coef[i + 1], f.coef[i + 2]}; }
void put(Function& f, std::size_t i, Vec3 v) noexcept { f.coef[i] = v.x; f.coef[i + 1] = v.y; f.coef[i + 2] = v.z; }
double scaleOf(Vec3 a, Vec3 b) noexcept {
    return std::max({1.0, std::fabs(a.x), std::fabs(a.y), std::fabs(a.z), std::fabs(b.x), std::fabs(b.y), std::fabs(b.z)});
}
// Removes the rounding noise of the analytic results (1e-17 instead of 0).
double clean(double v, double scale) noexcept { return std::fabs(v) < 1e-12 * scale ? 0.0 : v; }
Vec3 clean(Vec3 v, double scale) noexcept { return {clean(v.x, scale), clean(v.y, scale), clean(v.z, scale)}; }

// In-plane orthonormal axes: the first one follows the coordinate axis that is
// least aligned with the normal, so the plane mesh is close to the box axes.
void planeAxes(Vec3 n, Vec3& u, Vec3& v) noexcept {
    const double ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);
    const Vec3 axis = ax <= ay && ax <= az ? Vec3{1, 0, 0} : ay <= az ? Vec3{0, 1, 0} : Vec3{0, 0, 1};
    u = unit(axis - n * dot(n, axis));
    v = cross(n, u);
}
Shape planeFrom(Vec3 p, Vec3 normal) {
    Shape s;
    s.kind = Kind::Plane;
    s.n = unit(normal);
    s.h = dot(s.n, p);
    s.p = p;
    planeAxes(s.n, s.u, s.v);
    return s;
}
// A plane as a point and two spanning vectors, in the user's own numbers.
struct RawPlane { Vec3 p, a, b; bool fromExpress; double ea, eb, ec, ed; };
RawPlane rawPlane(const Function& f) {
    RawPlane r{};
    switch (static_cast<PlaneForm>(f.form)) {
    case PlaneForm::Express: {
        r.fromExpress = true;
        r.ea = f.coef[0]; r.eb = f.coef[1]; r.ec = f.coef[2]; r.ed = f.coef[3];
        const Vec3 n{r.ea, r.eb, r.ec};
        const double nn = dot(n, n);
        r.p = nn > 0 ? n * (-r.ed / nn) : Vec3{};
        Vec3 u, v;
        planeAxes(unit(n), u, v);
        r.a = u; r.b = v;
        break;
    }
    case PlaneForm::Vector: r.p = at(f, 0); r.a = at(f, 3); r.b = at(f, 6); break;
    case PlaneForm::Points: r.p = at(f, 0); r.a = at(f, 3) - r.p; r.b = at(f, 6) - r.p; break;
    }
    return r;
}

// ---- binary form
struct Out {
    std::string s;
    void u8(std::uint8_t v) { s.push_back(static_cast<char>(v)); }
    void u16(std::uint16_t v) { u8(static_cast<std::uint8_t>(v)); u8(static_cast<std::uint8_t>(v >> 8)); }
    void f64(double v) { std::uint64_t b; std::memcpy(&b, &v, 8); for (int i = 0; i < 8; ++i) u8(static_cast<std::uint8_t>(b >> (8 * i))); }
    void str(const std::string& t) { const auto n = static_cast<std::uint16_t>(std::min(t.size(), MaxExpression)); u16(n); s.append(t, 0, n); }
};
struct In {
    const std::string& s;
    std::size_t i{0};
    bool ok{true};
    std::uint8_t u8() { if (i >= s.size()) { ok = false; return 0; } return static_cast<std::uint8_t>(s[i++]); }
    std::uint16_t u16() { const std::uint16_t lo = u8(); return static_cast<std::uint16_t>(lo | (u8() << 8)); }
    double f64() {
        std::uint64_t b = 0;
        for (int k = 0; k < 8; ++k) b |= static_cast<std::uint64_t>(u8()) << (8 * k);
        double v; std::memcpy(&v, &b, 8); return v;
    }
    std::string str() {
        const std::size_t n = u16();
        if (!ok || n > MaxExpression || i + n > s.size()) { ok = false; return {}; }
        std::string t = s.substr(i, n); i += n; return t;
    }
};
constexpr std::uint8_t SetFormat = 1, WindowFormat = 1;
void writeWindow(Out& o, const Window& w) {
    for (double d : {w.xmin, w.xmax, w.ymin, w.ymax, w.zmin, w.zmax, w.smin, w.smax, w.tmin, w.tmax, w.theta, w.phi}) o.f64(d);
    for (int g : {w.xgrid, w.ygrid, w.sgrid, w.tgrid}) o.u8(static_cast<std::uint8_t>(std::clamp(g, 0, 255)));
}
bool readWindow(In& in, Window& w) {
    for (double* d : {&w.xmin, &w.xmax, &w.ymin, &w.ymax, &w.zmin, &w.zmax, &w.smin, &w.smax, &w.tmin, &w.tmax, &w.theta, &w.phi}) *d = in.f64();
    for (int* g : {&w.xgrid, &w.ygrid, &w.sgrid, &w.tgrid}) *g = in.u8();
    return in.ok && validate(w).empty();
}
} // namespace

double norm(Vec3 a) noexcept { return std::sqrt(dot(a, a)); }

bool isTemplate(Kind k) noexcept {
    return k == Kind::Line || k == Kind::Plane || k == Kind::Sphere || k == Kind::Cylinder || k == Kind::Cone;
}
const char* kindName(Kind k) noexcept {
    switch (k) {
    case Kind::Z: return "Z=";
    case Kind::Param: return "Param";
    case Kind::RotX: return "Rot X";
    case Kind::RotY: return "Rot Y";
    case Kind::Line: return "Line";
    case Kind::Plane: return "Plane";
    case Kind::Sphere: return "Sphere";
    case Kind::Cylinder: return "Cylinder";
    case Kind::Cone: return "Cone";
    case Kind::None: break;
    }
    return "";
}
std::uint8_t formCount(Kind k) noexcept {
    return k == Kind::Line ? 4 : k == Kind::Plane ? 3 : k == Kind::Sphere ? 2 : 1;
}
const char* formName(Kind k, std::uint8_t form) noexcept {
    if (k == Kind::Line) { static const char* const n[] = {"EXPRESS", "VECTOR", "P&V", "POINTS"}; return form < 4 ? n[form] : ""; }
    if (k == Kind::Plane) { static const char* const n[] = {"EXPRESS", "VECTOR", "POINTS"}; return form < 3 ? n[form] : ""; }
    if (k == Kind::Sphere) { static const char* const n[] = {"FACTOR", "EXPAND"}; return form < 2 ? n[form] : ""; }
    return "";
}
std::size_t coefficientCount(Kind k, std::uint8_t form) noexcept {
    switch (k) {
    case Kind::Line: return 6;
    case Kind::Plane: return form == static_cast<std::uint8_t>(PlaneForm::Express) ? 4 : 9;
    case Kind::Sphere: return 4;
    case Kind::Cylinder: case Kind::Cone: return 5;
    default: return 0;
    }
}
const char* colorName(Color c) noexcept {
    static const char* const n[ColorCount] = {"Black", "Blue", "Red", "Magenta", "Green", "Cyan", "Yellow", "White", "Clear"};
    const auto i = static_cast<std::uint8_t>(c);
    return i < ColorCount ? n[i] : "";
}
std::uint32_t colorRgb(Color c) noexcept {
    switch (c) {
    case Color::Black: return 0x000000;
    case Color::Blue: return 0x1E3CE6;
    case Color::Red: return 0xE01E1E;
    case Color::Magenta: return 0xD200D2;
    case Color::Green: return 0x009B3C;
    case Color::Cyan: return 0x00AADC;
    case Color::Yellow: return 0xE6BE00;
    case Color::White: case Color::Clear: return 0xFFFFFF;
    }
    return 0x000000;
}
Color defaultAreaColor(std::size_t row) noexcept {
    static const Color c[Functions] = {Color::Blue, Color::Red, Color::Green};
    return c[row % Functions];
}
bool Function::defined() const noexcept {
    switch (kind) {
    case Kind::None: return false;
    case Kind::Z: case Kind::RotX: case Kind::RotY: return !expr[0].empty();
    case Kind::Param: return !expr[0].empty() && !expr[1].empty() && !expr[2].empty();
    default: return true;
    }
}
Function emptyFunction(std::size_t row) {
    Function f;
    f.area = defaultAreaColor(row);
    return f;
}

// ---------------------------------------------------------------- window
double normalizeTheta(double d) noexcept {
    if (!std::isfinite(d)) return 0;
    d = std::fmod(d, 360.0);                 // (-360, 360)
    if (d <= -180.0) d += 360.0;
    if (d > 180.0) d -= 360.0;
    return d;
}
double normalizePhi(double d) noexcept {
    if (!std::isfinite(d)) return 0;
    d = std::fmod(d, 360.0);
    if (d < 0) d += 360.0;
    if (d >= 360.0) d = 0;
    return d;
}
std::string validate(const Window& w) {
    struct R { const char* name; double lo, hi; } ranges[] = {
        {"X", w.xmin, w.xmax}, {"Y", w.ymin, w.ymax}, {"Z", w.zmin, w.zmax}, {"S", w.smin, w.smax}, {"T", w.tmin, w.tmax}};
    for (const R& r : ranges)
        if (!std::isfinite(r.lo) || !std::isfinite(r.hi) || std::fabs(r.lo) >= MaxCoordinate || std::fabs(r.hi) >= MaxCoordinate)
            return std::string("Out of Domain: ") + r.name + "min e " + r.name + "max devono essere minori di 1E98 in valore assoluto";
    struct G { const char* name; int g; } grids[] = {{"Xgrid", w.xgrid}, {"Ygrid", w.ygrid}, {"Sgrid", w.sgrid}, {"Tgrid", w.tgrid}};
    for (const G& g : grids)
        if (g.g < MinGrid || g.g > MaxGrid) return std::string("Out of Domain: ") + g.name + " deve essere un intero da 2 a 50";
    if (!std::isfinite(w.theta) || !(w.theta > -180.0 && w.theta <= 180.0)) return "Out of Domain: Angle\xCE\xB8 deve essere -180 < \xCE\xB8 <= 180";
    if (!std::isfinite(w.phi) || !(w.phi >= 0.0 && w.phi < 360.0)) return "Out of Domain: Angle\xCF\x86 deve essere 0 <= \xCF\x86 < 360";
    for (const R& r : ranges)
        if (!(r.lo < r.hi)) return std::string("Invalid Setting: ") + r.name + "min deve essere minore di " + r.name + "max";
    return {};
}

// ---------------------------------------------------------------- shapes
std::string shapeOf(const Function& f, Shape& out) {
    for (std::size_t i = 0; i < coefficientCount(f.kind, f.form); ++i)
        if (!std::isfinite(f.coef[i])) return "Invalid Setting: coefficiente non finito";
    Shape s;
    s.kind = f.kind;
    switch (f.kind) {
    case Kind::Line:
        if (f.form >= formCount(f.kind)) return "Invalid Setting: modulo della retta";
        switch (static_cast<LineForm>(f.form)) {
        case LineForm::Express:
            if (f.coef[3] == 0 || f.coef[4] == 0 || f.coef[5] == 0) return "Invalid Setting: a, b e c devono essere diversi da 0";
            s.p = at(f, 0); s.d = at(f, 3);
            break;
        case LineForm::Vector:
            if (zero(at(f, 3))) return "Invalid Setting: il vettore v non puo' essere nullo";
            s.p = at(f, 0); s.d = at(f, 3);
            break;
        case LineForm::PointVector:
            if (zero(at(f, 3))) return "Invalid Setting: il vettore direzione non puo' essere nullo";
            s.p = at(f, 0); s.d = at(f, 3);
            break;
        case LineForm::Points:
            if (same(at(f, 0), at(f, 3))) return "Invalid Setting: P1 e P2 coincidono";
            s.p = at(f, 0); s.d = at(f, 3) - at(f, 0);
            break;
        }
        if (!finite(s.d) || zero(s.d)) return "Invalid Setting: direzione della retta";
        break;
    case Kind::Plane: {
        if (f.form >= formCount(f.kind)) return "Invalid Setting: modulo del piano";
        Vec3 normal{};
        Vec3 p{};
        switch (static_cast<PlaneForm>(f.form)) {
        case PlaneForm::Express:
            if (f.coef[0] == 0 && f.coef[1] == 0 && f.coef[2] == 0) return "Invalid Setting: a, b e c non possono essere tutti 0";
            normal = at(f, 0);
            p = normal * (-f.coef[3] / dot(normal, normal));
            break;
        case PlaneForm::Vector: {
            const Vec3 u = at(f, 3), v = at(f, 6);
            if (zero(u) || zero(v)) return "Invalid Setting: i vettori u e v non possono essere nulli";
            normal = cross(u, v);
            if (!(norm(normal) > 1e-12 * norm(u) * norm(v))) return "Invalid Setting: u e v hanno la stessa direzione";
            p = at(f, 0);
            break;
        }
        case PlaneForm::Points: {
            const Vec3 a = at(f, 0), b = at(f, 3), c = at(f, 6);
            if (same(a, b) || same(a, c) || same(b, c)) return "Invalid Setting: due punti coincidono";
            normal = cross(b - a, c - a);
            if (!(norm(normal) > 1e-12 * norm(b - a) * norm(c - a))) return "Invalid Setting: i tre punti sono allineati";
            p = a;
            break;
        }
        }
        if (!finite(normal) || !finite(p)) return "Invalid Setting: piano non valido";
        s = planeFrom(p, normal);
        break;
    }
    case Kind::Sphere:
        if (f.form >= formCount(f.kind)) return "Invalid Setting: modulo della sfera";
        if (static_cast<SphereForm>(f.form) == SphereForm::Factor) {
            if (!(f.coef[3] > 0)) return "Invalid Setting: r deve essere maggiore di 0";
            s.p = at(f, 0); s.r = f.coef[3];
        } else {
            const double a = f.coef[0], b = f.coef[1], c = f.coef[2], d = f.coef[3];
            const double q = a * a + b * b + c * c;
            if (!(q > 4 * d)) return "Invalid Setting: deve essere a\xC2\xB2+b\xC2\xB2+c\xC2\xB2 > 4d";
            s.p = {-a / 2, -b / 2, -c / 2};
            s.r = std::sqrt(q / 4 - d);
            if (!(s.r > 0) || !std::isfinite(s.r)) return "Invalid Setting: raggio non valido";
        }
        break;
    case Kind::Cylinder: case Kind::Cone:
        if (!(f.coef[0] > 0)) return "Invalid Setting: Radius deve essere maggiore di 0";
        if (f.coef[1] == f.coef[2]) return "Invalid Setting: Zmin e Zmax devono essere diversi";
        s.r = f.coef[0]; s.cx = f.coef[3]; s.cy = f.coef[4];
        if (f.kind == Kind::Cylinder) { s.z0 = std::min(f.coef[1], f.coef[2]); s.z1 = std::max(f.coef[1], f.coef[2]); }
        else { s.z0 = f.coef[1]; s.z1 = f.coef[2]; }   // base, apex
        break;
    default:
        return "Invalid Setting: non e' un modello";
    }
    out = s;
    return {};
}

std::string validate(const Function& f) {
    if (static_cast<std::uint8_t>(f.kind) >= KindCount) return "Invalid Setting: tipo di grafico";
    if (static_cast<std::uint8_t>(f.line) >= ColorCount || static_cast<std::uint8_t>(f.area) >= ColorCount) return "Invalid Setting: colore";
    if (f.line == Color::Clear && f.area == Color::Clear) return "Invalid Setting: Line Color e Area Color non possono essere entrambi Clear";
    for (const auto& e : f.expr) if (e.size() > MaxExpression) return "Memory ERROR: espressione troppo lunga";
    if (f.form >= formCount(f.kind)) return "Invalid Setting: modulo dei coefficienti";
    switch (f.kind) {
    case Kind::None: return {};
    case Kind::Z: case Kind::RotX: case Kind::RotY:
        return f.expr[0].empty() ? "Syntax ERROR: funzione vuota" : std::string();
    case Kind::Param:
        for (std::size_t i = 0; i < 3; ++i)
            if (f.expr[i].empty()) return std::string("Syntax ERROR: manca ") + (i == 0 ? "Xst" : i == 1 ? "Yst" : "Zst");
        return {};
    default: {
        Shape s;
        return shapeOf(f, s);
    }
    }
}

void convertForm(Function& f, std::uint8_t form) {
    if (!isTemplate(f.kind) || form >= formCount(f.kind) || form == f.form) return;
    Shape s;
    const bool valid = shapeOf(f, s).empty();
    Function g = f;
    g.form = form;
    g.coef.fill(0.0);
    if (valid) {
        if (f.kind == Kind::Line) {
            switch (static_cast<LineForm>(form)) {
            case LineForm::Express:
                if (s.d.x != 0 && s.d.y != 0 && s.d.z != 0) { put(g, 0, s.p); put(g, 3, s.d); }
                break;
            case LineForm::Vector: case LineForm::PointVector: put(g, 0, s.p); put(g, 3, s.d); break;
            case LineForm::Points: put(g, 0, s.p); put(g, 3, s.p + s.d); break;
            }
        } else if (f.kind == Kind::Plane) {
            const RawPlane r = rawPlane(f);
            switch (static_cast<PlaneForm>(form)) {
            case PlaneForm::Express:
                if (r.fromExpress) { g.coef[0] = r.ea; g.coef[1] = r.eb; g.coef[2] = r.ec; g.coef[3] = r.ed; }
                else { const Vec3 n = cross(r.a, r.b); put(g, 0, n); g.coef[3] = -dot(n, r.p); }
                break;
            case PlaneForm::Vector: put(g, 0, r.p); put(g, 3, r.a); put(g, 6, r.b); break;
            case PlaneForm::Points: put(g, 0, r.p); put(g, 3, r.p + r.a); put(g, 6, r.p + r.b); break;
            }
            const double sc = std::max({1.0, std::fabs(r.p.x), std::fabs(r.p.y), std::fabs(r.p.z)});
            for (double& c : g.coef) c = clean(c, sc);
        } else if (f.kind == Kind::Sphere) {
            if (static_cast<SphereForm>(form) == SphereForm::Factor) { put(g, 0, s.p); g.coef[3] = s.r; }
            else { put(g, 0, s.p * -2.0); g.coef[3] = dot(s.p, s.p) - s.r * s.r; }
        }
        if (!validate(g).empty()) g.coef.fill(0.0);
    }
    f = g;
}

// ---------------------------------------------------------------- INTSECT / RELATION
bool isLineOrPlane(const Function& f) noexcept { return f.kind == Kind::Line || f.kind == Kind::Plane; }

Intersection intersect(const Shape& a0, const Shape& b0) {
    const Shape& a = a0.kind == Kind::Plane && b0.kind == Kind::Line ? b0 : a0;
    const Shape& b = a0.kind == Kind::Plane && b0.kind == Kind::Line ? a0 : b0;
    Intersection r;
    const double scale = scaleOf(a.p, b.p);
    if (a.kind == Kind::Line && b.kind == Kind::Line) {
        const Vec3 d1 = unit(a.d), d2 = unit(b.d), c = cross(d1, d2), w = b.p - a.p;
        if (norm(c) < Eps) {
            r.kind = norm(cross(w, d1)) <= DistEps * scale ? Intersection::Kind::Infinite : Intersection::Kind::NotFound;
            return r;
        }
        if (std::fabs(dot(w, c)) / norm(c) > DistEps * scale) { r.kind = Intersection::Kind::NotFound; return r; }   // skew
        const double t = dot(cross(w, d2), c) / dot(c, c);
        r.kind = Intersection::Kind::Point;
        r.point = clean(a.p + d1 * t, scale);
        return r;
    }
    if (a.kind == Kind::Line && b.kind == Kind::Plane) {
        const Vec3 d = unit(a.d);
        const double den = dot(b.n, d), dist = dot(b.n, a.p) - b.h;
        if (std::fabs(den) < Eps) {
            r.kind = std::fabs(dist) <= DistEps * scale ? Intersection::Kind::Infinite : Intersection::Kind::NotFound;
            return r;
        }
        r.kind = Intersection::Kind::Point;
        r.point = clean(a.p - d * (dist / den), scale);
        return r;
    }
    if (a.kind == Kind::Plane && b.kind == Kind::Plane) {
        const Vec3 dir = cross(a.n, b.n);
        if (norm(dir) < Eps) {
            const double h2 = dot(a.n, b.n) < 0 ? -b.h : b.h;
            r.kind = std::fabs(a.h - h2) <= DistEps * scale ? Intersection::Kind::Infinite : Intersection::Kind::NotFound;
            return r;
        }
        r.kind = Intersection::Kind::Line;
        r.point = clean((cross(b.n, dir) * a.h + cross(dir, a.n) * b.h) * (1.0 / dot(dir, dir)), scale);
        r.direction = clean(unit(dir), 1.0);
        return r;
    }
    return r;
}

Relation relation(const Shape& a0, const Shape& b0) {
    const Shape& a = a0.kind == Kind::Plane && b0.kind == Kind::Line ? b0 : a0;
    const Shape& b = a0.kind == Kind::Plane && b0.kind == Kind::Line ? a0 : b0;
    const double scale = scaleOf(a.p, b.p);
    if (a.kind == Kind::Line && b.kind == Kind::Line) {
        const Vec3 d1 = unit(a.d), d2 = unit(b.d), c = cross(d1, d2), w = b.p - a.p;
        if (norm(c) < Eps) return norm(cross(w, d1)) <= DistEps * scale ? Relation::SameLine : Relation::Parallel;
        if (std::fabs(dot(w, c)) / norm(c) > DistEps * scale) return Relation::Skew;
        return std::fabs(dot(d1, d2)) < Eps ? Relation::Perpendicular : Relation::Intersect;
    }
    if (a.kind == Kind::Line && b.kind == Kind::Plane) {
        const Vec3 d = unit(a.d);
        if (std::fabs(dot(b.n, d)) < Eps) return std::fabs(dot(b.n, a.p) - b.h) <= DistEps * scale ? Relation::OnPlane : Relation::Parallel;
        return norm(cross(d, b.n)) < Eps ? Relation::Perpendicular : Relation::Intersect;
    }
    const Vec3 dir = cross(a.n, b.n);
    if (norm(dir) < Eps) {
        const double h2 = dot(a.n, b.n) < 0 ? -b.h : b.h;
        return std::fabs(a.h - h2) <= DistEps * scale ? Relation::SamePlane : Relation::Parallel;
    }
    return std::fabs(dot(a.n, b.n)) < Eps ? Relation::Perpendicular : Relation::Intersect;
}

const char* relationName(Relation r) noexcept {
    switch (r) {
    case Relation::Intersect: return "INTERSECT";
    case Relation::Perpendicular: return "PERPENDICULAR";
    case Relation::Parallel: return "PARALLEL";
    case Relation::Skew: return "SKEW";
    case Relation::SameLine: return "SAME LINE";
    case Relation::OnPlane: return "ON THE PLANE";
    case Relation::SamePlane: return "SAME PLANE";
    }
    return "";
}

// ---------------------------------------------------------------- binary form
std::string encodeWindow(const Window& w) {
    Out o;
    o.u8(WindowFormat);
    writeWindow(o, w);
    return std::move(o.s);
}
bool decodeWindow(const std::string& data, Window& out) {
    In in{data};
    if (in.u8() != WindowFormat) return false;
    Window w;
    if (!readWindow(in, w) || in.i != data.size()) return false;
    out = w;
    return true;
}
std::string encode(const GraphSet& set) {
    Out o;
    o.u8(SetFormat);
    for (const Function& f : set.fns) {
        o.u8(static_cast<std::uint8_t>(f.kind)); o.u8(f.selected ? 1 : 0); o.u8(f.form);
        o.u8(static_cast<std::uint8_t>(f.line)); o.u8(static_cast<std::uint8_t>(f.area));
        for (const auto& e : f.expr) o.str(e);
        for (double c : f.coef) o.f64(c);
    }
    writeWindow(o, set.window);
    o.u8(static_cast<std::uint8_t>((set.setup.axes ? 1 : 0) | (set.setup.box ? 2 : 0) | (set.setup.label ? 4 : 0)));
    o.u8(static_cast<std::uint8_t>(set.setup.plot));
    return std::move(o.s);
}
bool decode(const std::string& data, GraphSet& out) {
    In in{data};
    if (in.u8() != SetFormat) return false;
    GraphSet g;
    for (Function& f : g.fns) {
        const auto kind = in.u8(), sel = in.u8(), form = in.u8(), line = in.u8(), area = in.u8();
        if (!in.ok || kind >= KindCount || sel > 1 || line >= ColorCount || area >= ColorCount) return false;
        f.kind = static_cast<Kind>(kind); f.selected = sel != 0; f.form = form;
        f.line = static_cast<Color>(line); f.area = static_cast<Color>(area);
        for (auto& e : f.expr) e = in.str();
        for (double& c : f.coef) c = in.f64();
        if (!in.ok) return false;
        if (f.kind != Kind::None && !validate(f).empty()) return false;
        if (f.kind == Kind::None && (f.form != 0 || (f.line == Color::Clear && f.area == Color::Clear))) return false;
    }
    if (!readWindow(in, g.window)) return false;
    const auto flags = in.u8(), plot = in.u8();
    if (!in.ok || flags > 7 || plot >= ColorCount || plot == static_cast<std::uint8_t>(Color::Clear)) return false;
    g.setup.axes = flags & 1; g.setup.box = (flags & 2) != 0; g.setup.label = (flags & 4) != 0;
    g.setup.plot = static_cast<Color>(plot);
    if (in.i != data.size()) return false;
    out = std::move(g);
    return true;
}

} // namespace fx::g3d
