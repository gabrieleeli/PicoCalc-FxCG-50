// Nucleo di Geometry (v1.19.0): oggetti, grafo delle dipendenze, ricalcolo
// incrementale, intersezioni con le tolleranze dichiarate, f(x), selezione
// col puntatore, vista. Vedi geometry.hpp e docs/reports/ANALISI_FONTI_v1.19.0.md.
#include "geometry.hpp"
#include "expression.hpp"
#include "geometry_internal.hpp"
#include "limits.hpp"
#include "math_engine.hpp"
#include <algorithm>
#include <cstdio>

namespace fx::geo {
using namespace detail;

struct FunctionCache {
    std::string text;
    ast::NodePtr node;
};

std::size_t maxObjects() noexcept { return &limits::ram() == &limits::kPsramProfile ? 1000 : 250; }

const char* kindName(Kind k) noexcept {
    switch (k) {
    case Kind::Point: return "Punto";
    case Kind::Segment: return "Segmento";
    case Kind::Line: return "Retta";
    case Kind::Ray: return "Semiretta";
    case Kind::Vector: return "Vettore";
    case Kind::Circle: return "Circonferenza";
    case Kind::Arc: return "Arco";
    case Kind::Polygon: return "Poligono";
    case Kind::Function: return "Funzione";
    case Kind::Text: return "Testo";
    case Kind::Measure: return "Misura";
    case Kind::Expression: return "Espressione";
    case Kind::Transform: return "Trasformazione";
    default: return "?";
    }
}
const char* lengthUnitName(std::uint8_t u) noexcept {
    static const char* const n[LengthUnitCount] = {"Off", "mm", "cm", "m", "km", "inch", "feet", "yard", "mile"};
    return u < LengthUnitCount ? n[u] : "Off";
}
bool isLineLike(Kind k) noexcept { return k == Kind::Segment || k == Kind::Line || k == Kind::Ray || k == Kind::Vector; }
bool isCurve(Kind k) noexcept { return isLineLike(k) || k == Kind::Circle || k == Kind::Arc || k == Kind::Function; }

bool Object::sameData(const Object& o) const noexcept {
    return id == o.id && kind == o.kind && def == o.def && flag == o.flag && flag2 == o.flag2 && parents == o.parents && vars == o.vars &&
           label == o.label && text == o.text && style == o.style && hidden == o.hidden && showLabel == o.showLabel && figure == o.figure &&
           z == o.z && fmt == o.fmt && seq == o.seq;
}
bool State::operator==(const State& o) const noexcept {
    if (objects.size() != o.objects.size()) return false;
    for (std::size_t i = 0; i < objects.size(); ++i) if (!objects[i].sameData(o.objects[i])) return false;
    return locks == o.locks && animations == o.animations && traced == o.traced && columns == o.columns && table == o.table &&
           tableRows == o.tableRows && view == o.view && options == o.options && file == o.file && background == o.background &&
           fade == o.fade && nextId == o.nextId && nextFigure == o.nextFigure && nextLabel == o.nextLabel && nextSeq == o.nextSeq;
}

int indexOf(const State& s, Id id) noexcept {
    if (id == NoId) return -1;
    std::size_t lo = 0, hi = s.objects.size();
    while (lo < hi) {
        const std::size_t mid = (lo + hi) / 2;
        if (s.objects[mid].id < id) lo = mid + 1; else hi = mid;
    }
    return lo < s.objects.size() && s.objects[lo].id == id ? static_cast<int>(lo) : -1;
}
const Object* find(const State& s, Id id) noexcept { const int i = indexOf(s, id); return i < 0 ? nullptr : &s.objects[static_cast<std::size_t>(i)]; }
Object* find(State& s, Id id) noexcept { const int i = indexOf(s, id); return i < 0 ? nullptr : &s.objects[static_cast<std::size_t>(i)]; }
bool defined(const State& s, Id id) noexcept { const Object* o = find(s, id); return o && o->g.ok; }

namespace detail {
const Object* objectOf(const State& s, Id id) noexcept { return find(s, id); }
const Shape* shapeOf(const State& s, Id id) noexcept { const Object* o = find(s, id); return o && o->g.ok ? &o->g : nullptr; }
[[noreturn]] void fail(const std::string& message) { throw MathError(message); }
double toRadians(const Options& o, double v) noexcept { return o.angle == 0 ? v * Pi / 180 : v; }
double fromRadians(const Options& o, double v) noexcept { return o.angle == 0 ? v * 180 / Pi : v; }

bool curveOf(const State& s, const Object& o, Curve& c) noexcept {
    if (!o.g.ok) return false;
    const auto& a = o.g.a;
    if (isLineLike(o.kind)) {
        c.line = true;
        c.p = {a[0], a[1]};
        c.d = V2{a[2], a[3]} - c.p;
        c.tmin = o.kind == Kind::Line ? -HUGE_VAL : 0;
        c.tmax = o.kind == Kind::Segment || o.kind == Kind::Vector ? 1 : HUGE_VAL;
        return norm(c.d) > 0;
    }
    if (o.kind == Kind::Circle || o.kind == Kind::Arc) {
        c.line = false;
        c.c = {a[0], a[1]};
        c.r = a[2];
        c.full = o.kind == Kind::Circle;
        c.a0 = o.kind == Kind::Circle ? a[3] : a[3];
        c.sweep = o.kind == Kind::Circle ? TwoPi : a[4];
        return c.r > 0;
    }
    (void)s;
    return false;
}
double arcParam(const Curve& c, double angle) noexcept {
    if (c.sweep >= 0) return wrap2Pi(angle - c.a0);
    return wrap2Pi(c.a0 - angle);
}
bool onArc(const Curve& c, double angle, double eps) noexcept {
    if (c.full) return true;
    const double rel = arcParam(c, angle), sw = std::abs(c.sweep);
    return rel <= sw + eps || rel >= TwoPi - eps;
}
namespace {
bool inRange(const Curve& c, double t) noexcept { return t >= c.tmin - EpsParam && t <= c.tmax + EpsParam; }
int lineLine(const Curve& a, const Curve& b, V2 out[2]) noexcept {
    const double den = cross(a.d, b.d);
    if (std::abs(den) <= EpsParallel * norm(a.d) * norm(b.d)) return 0;   // parallele o coincidenti
    const V2 w = b.p - a.p;
    const double t = cross(w, b.d) / den, u = cross(w, a.d) / den;
    if (!inRange(a, t) || !inRange(b, u)) return 0;
    out[0] = a.p + t * a.d;
    return 1;
}
// Retta-circonferenza: radici ordinate per t sulla retta.
int lineCircle(const Curve& l, const Curve& c, V2 out[2], double ts[2]) noexcept {
    const double dd = dot(l.d, l.d);
    const double t0 = dot(c.c - l.p, l.d) / dd;
    const V2 f = l.p + t0 * l.d;
    const double h = norm(c.c - f);
    const double sc = scaleOf({l.p.x, l.p.y, c.c.x, c.c.y, c.r});
    int n = 0;
    auto take = [&](double t) {
        const V2 p = l.p + t * l.d;
        if (!inRange(l, t) || !onArc(c, angleOf(p - c.c), EpsAngle)) return;
        ts[n] = t;
        out[n++] = p;
    };
    if (std::abs(h - c.r) <= EpsTangent * sc) take(t0);
    else if (h < c.r) {
        const double dt = std::sqrt(c.r * c.r - h * h) / std::sqrt(dd);
        take(t0 - dt);
        take(t0 + dt);
    }
    return n;
}
int circleCircle(const Curve& a, const Curve& b, V2 out[2]) noexcept {
    const V2 dv = b.c - a.c;
    const double d = norm(dv);
    const double sc = scaleOf({a.c.x, a.c.y, b.c.x, b.c.y, a.r, b.r});
    if (d <= EpsConcentric * sc) return 0;                                   // concentriche
    const V2 u = (1 / d) * dv;
    V2 cand[2];
    int m = 0;
    if (std::abs(d - (a.r + b.r)) <= EpsTangent * sc) cand[m++] = a.c + a.r * u;           // tangenti esterne
    else if (std::abs(d - std::abs(a.r - b.r)) <= EpsTangent * sc) cand[m++] = a.c + (a.r > b.r ? a.r : -a.r) * u;   // interne
    else if (d < a.r + b.r && d > std::abs(a.r - b.r)) {
        const double x = (d * d + a.r * a.r - b.r * b.r) / (2 * d);
        const double h = std::sqrt(std::max(0.0, a.r * a.r - x * x));
        const V2 m0 = a.c + x * u;
        cand[m++] = m0 - h * rot90(u);
        cand[m++] = m0 + h * rot90(u);
    }
    int n = 0;
    for (int i = 0; i < m; ++i)
        if (onArc(a, angleOf(cand[i] - a.c), EpsAngle) && onArc(b, angleOf(cand[i] - b.c), EpsAngle)) out[n++] = cand[i];
    if (n == 2 && arcParam(a, angleOf(out[0] - a.c)) > arcParam(a, angleOf(out[1] - a.c))) std::swap(out[0], out[1]);
    return n;
}
} // namespace
int intersect(const Curve& c1, const Curve& c2, V2 out[2]) noexcept {
    if (c1.line && c2.line) return lineLine(c1, c2, out);
    double ts[2];
    if (c1.line) return lineCircle(c1, c2, out, ts);
    if (c2.line) {
        const int n = lineCircle(c2, c1, out, ts);
        if (n == 2 && arcParam(c1, angleOf(out[0] - c1.c)) > arcParam(c1, angleOf(out[1] - c1.c))) std::swap(out[0], out[1]);
        return n;
    }
    return circleCircle(c1, c2, out);
}

void checkRoom(const State& s, std::size_t more) {
    if (s.objects.size() + more > maxObjects())
        fail("Memory ERROR: oltre " + std::to_string(maxObjects()) + " oggetti");
}
namespace {
// Gli id sono a 16 bit: quando finiscono si rinumerano (stesso ordine, quindi stesso grafo).
void renumber(State& s) {
    std::vector<std::pair<Id, Id>> map;
    map.reserve(s.objects.size());
    Id next = 1;
    for (auto& o : s.objects) { map.emplace_back(o.id, next); o.id = next++; }
    auto remap = [&](Id& id) {
        auto it = std::lower_bound(map.begin(), map.end(), std::make_pair(id, Id{0}));
        id = it != map.end() && it->first == id ? it->second : NoId;
    };
    for (auto& o : s.objects) for (Id& p : o.parents) remap(p);
    for (auto& l : s.locks) for (Id& p : l.m.ops) remap(p);
    for (auto& a : s.animations) { remap(a.point); remap(a.path); }
    for (Id& t : s.traced) remap(t);
    for (auto& c : s.columns) for (Id& p : c.m.ops) remap(p);
    s.nextId = next;
}
} // namespace
Id append(State& s, Object o) {
    checkRoom(s, 1);
    if (s.nextId >= 0xFFF0) renumber(s);
    o.id = s.nextId++;
    for (Id p : o.parents) if (p == NoId || p >= o.id || indexOf(s, p) < 0) fail("Argument ERROR: genitore mancante");
    const std::string e = validate(s, o);
    if (!e.empty()) fail(e);
    s.objects.push_back(std::move(o));
    evaluate(s, s.objects.back());
    return s.objects.back().id;
}

bool solverVar(const Object& o, std::size_t k) noexcept {
    switch (o.def) {
    case Def::Free: return k < 2;
    case Def::On: case Def::IsoApex: case Def::ArcEnd: return k == 0;
    case Def::RectVertex: return o.flag2 == 0 && k == 0;
    default: return false;
    }
}
} // namespace detail

namespace {
struct RadiansGuard {
    AngleMode old;
    RadiansGuard() : old(angleMode()) { setAngleMode(AngleMode::Radians); }
    ~RadiansGuard() { setAngleMode(old); }
};
bool setPoint(Shape& g, V2 p) {
    g.a[0] = p.x; g.a[1] = p.y;
    return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) < 1e150 && std::abs(p.y) < 1e150;
}
bool setLine(Shape& g, V2 a, V2 b) {
    g.a[0] = a.x; g.a[1] = a.y; g.a[2] = b.x; g.a[3] = b.y;
    for (int i = 0; i < 4; ++i) if (!std::isfinite(g.a[i]) || std::abs(g.a[i]) >= 1e150) return false;
    return norm(b - a) > EpsCoincident * scaleOf({a.x, a.y, b.x, b.y});
}
bool setCircle(Shape& g, V2 c, double r, double a0, double sweep) {
    g.a[0] = c.x; g.a[1] = c.y; g.a[2] = r; g.a[3] = a0; g.a[4] = sweep;
    for (int i = 0; i < 5; ++i) if (!std::isfinite(g.a[i]) || std::abs(g.a[i]) >= 1e150) return false;
    return r > EpsCoincident * scaleOf({c.x, c.y});
}
V2 apply(const Shape& t, V2 p) noexcept { return {t.a[0] * p.x + t.a[1] * p.y + t.a[4], t.a[2] * p.x + t.a[3] * p.y + t.a[5]}; }
bool pointParent(const State& s, const Object& o, std::size_t i, V2& p) {
    const Shape* g = i < o.parents.size() ? shapeOf(s, o.parents[i]) : nullptr;
    if (!g) return false;
    p = pointOf(*g);
    return true;
}
bool lineParent(const State& s, const Object& o, std::size_t i, V2& a, V2& b) {
    const Shape* g = i < o.parents.size() ? shapeOf(s, o.parents[i]) : nullptr;
    if (!g) return false;
    a = startOf(*g); b = endOf(*g);
    return true;
}
// Due punti dai genitori: due punti, oppure un segmento/vettore (Midpoint, PerpBisector).
bool twoPoints(const State& s, const Object& o, V2& a, V2& b) {
    if (o.parents.size() == 1) return lineParent(s, o, 0, a, b);
    return pointParent(s, o, 0, a) && pointParent(s, o, 1, b);
}
bool evalShape(State& s, Object& o) {
    Shape& g = o.g;
    V2 a, b, c, p;
    switch (o.def) {
    case Def::Free: return setPoint(g, {o.vars[0], o.vars[1]});
    case Def::On: {
        double x, y;
        return curvePoint(s, o.parents[0], o.vars[0], x, y) && setPoint(g, {x, y});
    }
    case Def::Midpoint: return twoPoints(s, o, a, b) && setPoint(g, 0.5 * (a + b));
    case Def::Intersection: {
        const Object* c1 = find(s, o.parents[0]);
        const Object* c2 = find(s, o.parents[1]);
        Curve k1, k2;
        if (!c1 || !c2 || !curveOf(s, *c1, k1) || !curveOf(s, *c2, k2)) return false;
        V2 pts[2];
        const int n = intersect(k1, k2, pts);
        return o.flag < n && setPoint(g, pts[o.flag]);
    }
    case Def::Image: {
        const Shape* t = shapeOf(s, o.parents[1]);
        return t && pointParent(s, o, 0, p) && setPoint(g, apply(*t, p));
    }
    case Def::RectVertex:
        if (!pointParent(s, o, 0, a) || !pointParent(s, o, 1, b)) return false;
        if (o.flag2 == 0) {
            const double l = norm(b - a);
            if (l <= 0) return false;
            return setPoint(g, b + (o.vars[0] / l) * rot90(b - a));
        }
        return pointParent(s, o, 2, c) && setPoint(g, a + (c - b));
    case Def::SquareVertex:
        if (!pointParent(s, o, 0, a) || !pointParent(s, o, 1, b)) return false;
        return setPoint(g, (o.flag2 == 0 ? b : a) + rot90(b - a));
    case Def::IsoApex: {
        if (!pointParent(s, o, 0, a) || !pointParent(s, o, 1, b)) return false;
        const double l = norm(b - a);
        return l > 0 && setPoint(g, 0.5 * (a + b) + (o.vars[0] / l) * rot90(b - a));
    }
    case Def::NgonVertex:
        if (!pointParent(s, o, 0, c) || !pointParent(s, o, 1, a)) return false;
        return setPoint(g, c + rotate(a - c, TwoPi * o.flag2 / o.flag));
    case Def::ArcEnd:
        if (!pointParent(s, o, 0, c) || !pointParent(s, o, 1, a)) return false;
        return setPoint(g, c + norm(a - c) * dir(o.vars[0]));
    case Def::TwoPoints: return pointParent(s, o, 0, a) && pointParent(s, o, 1, b) && setLine(g, a, b);
    case Def::PerpBisector: {
        if (!twoPoints(s, o, a, b)) return false;
        const V2 m = 0.5 * (a + b);
        return setLine(g, m, m + rot90(b - a));
    }
    case Def::Perpendicular: case Def::Parallel: {
        if (!lineParent(s, o, 0, a, b) || !pointParent(s, o, 1, p)) return false;
        const V2 d = o.def == Def::Parallel ? b - a : rot90(b - a);
        return setLine(g, p, p + d);
    }
    case Def::AngleBisector: {
        const Object* l1 = find(s, o.parents[0]);
        const Object* l2 = find(s, o.parents[1]);
        Curve k1, k2;
        if (!l1 || !l2 || !curveOf(s, *l1, k1) || !curveOf(s, *l2, k2)) return false;
        k1.tmin = k2.tmin = -HUGE_VAL; k1.tmax = k2.tmax = HUGE_VAL;     // le rette che li contengono
        V2 x[2];
        if (lineLine(k1, k2, x) != 1) return false;
        const V2 u1 = (1 / norm(k1.d)) * k1.d, u2 = (1 / norm(k2.d)) * k2.d;
        V2 d = o.flag == 0 ? u1 + u2 : u1 - u2;
        if (norm(d) < 1e-12) d = rot90(u1);
        return setLine(g, x[0], x[0] + (norm(k1.d) / norm(d)) * d);
    }
    case Def::Tangent: {
        const Object* cu = find(s, o.parents[0]);
        if (!cu || !cu->g.ok || !pointParent(s, o, 1, p)) return false;
        if (cu->kind == Kind::Function) {
            double y0, y1, y2;
            const double h = 1e-6 * std::max(1.0, std::abs(p.x));
            if (!evalFunction(s, *cu, p.x, y0) || !evalFunction(s, *cu, p.x - h, y1) || !evalFunction(s, *cu, p.x + h, y2)) return false;
            const double m = (y2 - y1) / (2 * h);
            return setLine(g, {p.x, y0}, {p.x + 1, y0 + m});
        }
        const V2 cc{cu->g.a[0], cu->g.a[1]};
        return setLine(g, p, p + rot90(p - cc));
    }
    case Def::ImageCurve: {
        const Object* src = find(s, o.parents[0]);
        const Shape* t = shapeOf(s, o.parents[1]);
        if (!src || !src->g.ok || !t) return false;
        const auto& q = src->g.a;
        if (isLineLike(src->kind)) return setLine(g, apply(*t, {q[0], q[1]}), apply(*t, {q[2], q[3]}));
        const double det = t->a[0] * t->a[3] - t->a[1] * t->a[2];
        const double k = std::sqrt(std::abs(det));
        const V2 cc = apply(*t, {q[0], q[1]});
        const V2 s0 = apply(*t, V2{q[0], q[1]} + q[2] * dir(q[3]));
        const double sweep = src->kind == Kind::Circle ? TwoPi : (det < 0 ? -q[4] : q[4]);
        return setCircle(g, cc, q[2] * k, angleOf(s0 - cc), sweep);
    }
    case Def::CircleCP:
        if (!pointParent(s, o, 0, c) || !pointParent(s, o, 1, p)) return false;
        return setCircle(g, c, norm(p - c), angleOf(p - c), TwoPi);
    case Def::ArcCSE: {
        V2 e;
        if (!pointParent(s, o, 0, c) || !pointParent(s, o, 1, a) || !pointParent(s, o, 2, e)) return false;
        const double s0 = angleOf(a - c), s1 = angleOf(e - c);
        double sw = wrap2Pi(s1 - s0);
        if (sw <= EpsAngle) sw = TwoPi;
        if (o.flag) sw = -(TwoPi - sw >= EpsAngle ? TwoPi - sw : TwoPi);
        return setCircle(g, c, norm(a - c), s0, sw);
    }
    case Def::Semicircle: {
        if (!pointParent(s, o, 0, a) || !pointParent(s, o, 1, b)) return false;
        const V2 m = 0.5 * (a + b);
        return setCircle(g, m, 0.5 * norm(b - a), angleOf(a - m), o.flag ? -Pi : Pi);
    }
    case Def::Arc3: {
        V2 m;
        if (!pointParent(s, o, 0, a) || !pointParent(s, o, 1, m) || !pointParent(s, o, 2, b)) return false;
        const double d = 2 * cross(m - a, b - a);
        if (std::abs(d) <= EpsParallel * norm(m - a) * norm(b - a) * 2) return false;      // allineati
        const V2 ma = m - a, ba = b - a;
        const V2 cc = a + (1 / d) * V2{ba.y * dot(ma, ma) - ma.y * dot(ba, ba), ma.x * dot(ba, ba) - ba.x * dot(ma, ma)};
        const double s0 = angleOf(a - cc), sm = wrap2Pi(angleOf(m - cc) - s0), se = wrap2Pi(angleOf(b - cc) - s0);
        const double sweep = sm < se ? se : se - TwoPi;
        return setCircle(g, cc, norm(a - cc), s0, sweep);
    }
    case Def::PolygonV:
        for (Id v : o.parents) if (!shapeOf(s, v)) return false;
        return true;
    case Def::FunctionX: {
        if (!o.fn || o.fn->text != o.text) {
            auto cache = std::make_shared<FunctionCache>();
            cache->text = o.text;
            try { cache->node = parseExpression(o.text); } catch (const std::bad_alloc&) { throw; } catch (const std::exception&) { cache->node.reset(); }
            o.fn = std::move(cache);
        }
        return o.fn->node != nullptr;
    }
    case Def::TextAt: case Def::MeasureAt: case Def::ExpressionAt: return setPoint(g, {o.vars[0], o.vars[1]});
    case Def::Reflect: {
        if (!lineParent(s, o, 0, a, b)) return false;
        const V2 u = (1 / norm(b - a)) * (b - a);
        const double m00 = 2 * u.x * u.x - 1, m01 = 2 * u.x * u.y, m11 = 2 * u.y * u.y - 1;
        g.a[0] = m00; g.a[1] = m01; g.a[2] = m01; g.a[3] = m11;
        g.a[4] = a.x - (m00 * a.x + m01 * a.y); g.a[5] = a.y - (m01 * a.x + m11 * a.y);
        return true;
    }
    case Def::Translate: case Def::TranslateVec: {
        V2 d{0, 0};
        if (o.def == Def::Translate) d = {o.vars[0], o.vars[1]};
        else if (lineParent(s, o, 0, a, b)) d = b - a;
        else return false;
        g.a[0] = 1; g.a[1] = 0; g.a[2] = 0; g.a[3] = 1; g.a[4] = d.x; g.a[5] = d.y;
        return std::isfinite(d.x) && std::isfinite(d.y);
    }
    case Def::Rotate: case Def::Dilate: case Def::Symmetry: {
        if (!pointParent(s, o, 0, c)) return false;
        double m00, m01, m10, m11;
        if (o.def == Def::Rotate) { const double cs = std::cos(o.vars[0]), sn = std::sin(o.vars[0]); m00 = cs; m01 = -sn; m10 = sn; m11 = cs; }
        else { const double k = o.def == Def::Dilate ? o.vars[0] : -1.0; m00 = k; m01 = 0; m10 = 0; m11 = k; }
        g.a[0] = m00; g.a[1] = m01; g.a[2] = m10; g.a[3] = m11;
        g.a[4] = c.x - (m00 * c.x + m01 * c.y); g.a[5] = c.y - (m10 * c.x + m11 * c.y);
        return true;
    }
    default: return false;
    }
}
} // namespace

namespace detail {
void evaluate(State& s, Object& o) {
    Shape g{};
    o.g = g;
    o.g.ok = evalShape(s, o);
}
} // namespace detail

bool evalFunction(const State& s, const Object& f, double x, double& y) {
    (void)s;
    if (!f.fn || !f.fn->node) return false;
    try {
        RadiansGuard rad;
        EvalContext ctx;
        ctx.bind("x", x);
        y = evaluateReal(*f.fn->node, ctx);
        return std::isfinite(y);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception&) {
        return false;
    }
}

std::size_t recompute(State& s) {
    for (auto& o : s.objects) evaluate(s, o);
    return s.objects.size();
}
std::size_t recompute(State& s, const std::vector<Id>& changed) {
    const std::size_t n = s.objects.size();
    std::vector<char> dirty(n, 0);
    std::size_t first = n;
    for (Id id : changed) { const int i = indexOf(s, id); if (i >= 0) { dirty[static_cast<std::size_t>(i)] = 1; first = std::min(first, static_cast<std::size_t>(i)); } }
    std::size_t count = 0;
    for (std::size_t i = first; i < n; ++i) {
        Object& o = s.objects[i];
        if (!dirty[i])
            for (Id p : o.parents) { const int j = indexOf(s, p); if (j >= 0 && dirty[static_cast<std::size_t>(j)]) { dirty[i] = 1; break; } }
        if (dirty[i]) { evaluate(s, o); ++count; }
    }
    return count;
}
bool isAncestor(const State& s, Id ancestor, Id id) {
    const auto d = descendants(s, {ancestor});
    return std::find(d.begin(), d.end(), id) != d.end();
}
std::vector<Id> descendants(const State& s, const std::vector<Id>& ids) {
    std::vector<char> mark(s.objects.size(), 0);
    for (Id id : ids) { const int i = indexOf(s, id); if (i >= 0) mark[static_cast<std::size_t>(i)] = 1; }
    std::vector<Id> out;
    for (std::size_t i = 0; i < s.objects.size(); ++i) {
        if (!mark[i])
            for (Id p : s.objects[i].parents) { const int j = indexOf(s, p); if (j >= 0 && mark[static_cast<std::size_t>(j)]) { mark[i] = 1; break; } }
        if (mark[i]) out.push_back(s.objects[i].id);
    }
    return out;
}

namespace {
bool kindIs(const State& s, Id id, bool (*pred)(Kind)) { const Object* o = find(s, id); return o && pred(o->kind); }
bool isPointK(Kind k) { return k == Kind::Point; }
bool isLineK(Kind k) { return isLineLike(k); }
bool isSegOrVec(Kind k) { return k == Kind::Segment || k == Kind::Vector; }
bool isRound(Kind k) { return k == Kind::Circle || k == Kind::Arc; }
bool isCutting(Kind k) { return isLineLike(k) || isRound(k); }
bool isCurveK(Kind k) { return isCurve(k); }
bool isTangentCurve(Kind k) { return isRound(k) || k == Kind::Function; }
bool isTransformK(Kind k) { return k == Kind::Transform; }
bool isMeasureK(Kind k) { return k == Kind::Measure; }
bool isVectorK(Kind k) { return k == Kind::Vector; }
bool anyGeom(Kind k) { return k != Kind::Text && k != Kind::Expression && k != Kind::Transform && k != Kind::Measure; }
} // namespace

std::string validate(const State& s, const Object& o) {
    const std::size_t np = o.parents.size(), nv = o.vars.size();
    for (double v : o.vars) if (!std::isfinite(v) || std::abs(v) >= 1e150) return "Math ERROR: valore non valido";
    if (o.label.size() > MaxLabel) return "Argument ERROR: etichetta oltre 14 caratteri";
    if (o.style.line >= ColorCount || o.style.chars >= ColorCount || (o.style.area != NoArea && o.style.area >= ColorCount) ||
        o.style.paint > 1 || o.style.lineStyle >= LineStyle::Count)
        return "Argument ERROR: stile";
    if (o.fmt != 0xFF && o.fmt > FmtNorm) return "Argument ERROR: formato";
    auto parentsAre = [&](std::initializer_list<bool (*)(Kind)> preds) {
        if (np != preds.size()) return false;
        std::size_t i = 0;
        for (auto pr : preds) if (!kindIs(s, o.parents[i++], pr)) return false;
        return true;
    };
    auto need = [&](Kind k, bool ok, std::size_t vars) { return o.kind == k && ok && nv == vars; };
    bool ok = false;
    switch (o.def) {
    case Def::Free: ok = need(Kind::Point, np == 0, 2); break;
    case Def::On: ok = need(Kind::Point, parentsAre({isCurveK}), 1); break;
    case Def::Midpoint: ok = need(Kind::Point, parentsAre({isPointK, isPointK}) || parentsAre({isSegOrVec}), 0); break;
    case Def::Intersection: ok = need(Kind::Point, parentsAre({isCutting, isCutting}), 0) && o.flag <= 1 && o.parents[0] != o.parents[1]; break;
    case Def::Image: ok = need(Kind::Point, parentsAre({isPointK, isTransformK}), 0); break;
    case Def::RectVertex: ok = o.flag2 == 0 ? need(Kind::Point, parentsAre({isPointK, isPointK}), 1)
                                            : o.flag2 == 1 && need(Kind::Point, parentsAre({isPointK, isPointK, isPointK}), 0); break;
    case Def::SquareVertex: ok = need(Kind::Point, parentsAre({isPointK, isPointK}), 0) && o.flag2 <= 1; break;
    case Def::IsoApex: case Def::ArcEnd: ok = need(Kind::Point, parentsAre({isPointK, isPointK}), 1); break;
    case Def::NgonVertex: ok = need(Kind::Point, parentsAre({isPointK, isPointK}), 0) && o.flag >= MinNgon && o.flag <= MaxNgon && o.flag2 >= 1 && o.flag2 < o.flag; break;
    case Def::TwoPoints: ok = isLineLike(o.kind) && parentsAre({isPointK, isPointK}) && nv == 0 && o.parents[0] != o.parents[1]; break;
    case Def::PerpBisector: ok = need(Kind::Line, parentsAre({isPointK, isPointK}) || parentsAre({isSegOrVec}), 0); break;
    case Def::Perpendicular: case Def::Parallel: ok = need(Kind::Line, parentsAre({isLineK, isPointK}), 0); break;
    case Def::AngleBisector: ok = need(Kind::Line, parentsAre({isLineK, isLineK}), 0) && o.flag <= 1; break;
    case Def::Tangent: ok = need(Kind::Line, parentsAre({isTangentCurve, isPointK}), 0); break;
    case Def::ImageCurve: {
        const Object* src = np == 2 ? find(s, o.parents[0]) : nullptr;
        ok = src && src->kind == o.kind && (isLineLike(o.kind) || isRound(o.kind)) && kindIs(s, o.parents[1], isTransformK) && nv == 0;
        break;
    }
    case Def::CircleCP: ok = need(Kind::Circle, parentsAre({isPointK, isPointK}), 0) && o.parents[0] != o.parents[1]; break;
    case Def::ArcCSE: ok = need(Kind::Arc, parentsAre({isPointK, isPointK, isPointK}), 0) && o.flag <= 1; break;
    case Def::Semicircle: ok = need(Kind::Arc, parentsAre({isPointK, isPointK}), 0) && o.flag <= 1; break;
    case Def::Arc3: ok = need(Kind::Arc, parentsAre({isPointK, isPointK, isPointK}), 0); break;
    case Def::PolygonV: {
        ok = o.kind == Kind::Polygon && np >= 3 && np <= MaxVertices && nv == 0;
        for (Id p : o.parents) ok = ok && kindIs(s, p, isPointK);
        break;
    }
    case Def::FunctionX: ok = need(Kind::Function, np == 0, 0) && !o.text.empty() && o.text.size() <= MaxFormula; break;
    case Def::TextAt: ok = need(Kind::Text, np == 0, 2) && o.text.size() <= MaxText; break;
    case Def::MeasureAt: {
        ok = need(Kind::Measure, np >= 1 && np <= 3, 2) && o.flag < static_cast<std::uint8_t>(MType::Label) && o.flag2 <= 1;
        for (Id p : o.parents) ok = ok && kindIs(s, p, anyGeom);
        break;
    }
    case Def::ExpressionAt: {
        ok = need(Kind::Expression, np <= MaxColumns, 2) && o.text.size() <= MaxFormula;
        for (Id p : o.parents) ok = ok && kindIs(s, p, isMeasureK);
        break;
    }
    case Def::Reflect: ok = need(Kind::Transform, parentsAre({isLineK}), 0); break;
    case Def::Translate: ok = need(Kind::Transform, np == 0, 2); break;
    case Def::TranslateVec: ok = need(Kind::Transform, parentsAre({isVectorK}), 0); break;
    case Def::Rotate: ok = need(Kind::Transform, parentsAre({isPointK}), 1); break;
    case Def::Dilate: ok = need(Kind::Transform, parentsAre({isPointK}), 1) && std::abs(o.vars[0]) >= MinDilation && std::abs(o.vars[0]) <= MaxDilation; break;
    case Def::Symmetry: ok = need(Kind::Transform, parentsAre({isPointK}), 0); break;
    default: ok = false;
    }
    return ok ? std::string() : std::string("Argument ERROR: definizione non valida");
}

std::string nextPointLabel(State& s) {
    const unsigned n = s.nextLabel++;
    std::string l(1, static_cast<char>('A' + n % 26));
    if (n >= 26) l += std::to_string(n / 26);
    return l;
}

std::string shortName(const State& s, Id id) {
    const Object* o = find(s, id);
    if (!o) return {};
    if (!o->label.empty()) return o->label;
    std::string n;
    if (o->def == Def::TwoPoints || o->def == Def::CircleCP || o->def == Def::Semicircle || o->def == Def::Arc3 || o->def == Def::ArcCSE || o->def == Def::PolygonV)
        for (Id p : o->parents) if (const Object* q = find(s, p)) n += q->label;
    return n.size() > MaxLabel ? n.substr(0, MaxLabel) : n;
}

// ---- parametri delle curve -----------------------------------------------------------
bool curvePoint(const State& s, Id curve, double t, double& x, double& y) {
    const Object* o = find(s, curve);
    if (!o || !o->g.ok) return false;
    const auto& a = o->g.a;
    V2 p;
    if (isLineLike(o->kind)) p = V2{a[0], a[1]} + t * V2{a[2] - a[0], a[3] - a[1]};
    else if (o->kind == Kind::Circle) p = V2{a[0], a[1]} + a[2] * dir(a[3] + t);
    else if (o->kind == Kind::Arc) p = V2{a[0], a[1]} + a[2] * dir(a[3] + t * a[4]);
    else if (o->kind == Kind::Function) { double fy; if (!evalFunction(s, *o, t, fy)) return false; p = {t, fy}; }
    else return false;
    x = p.x; y = p.y;
    return std::isfinite(x) && std::isfinite(y);
}
bool project(const State& s, Id curve, double x, double y, double& t, double& px, double& py) {
    const Object* o = find(s, curve);
    if (!o || !o->g.ok) return false;
    const auto& a = o->g.a;
    const V2 q{x, y};
    if (isLineLike(o->kind)) {
        const V2 p0{a[0], a[1]}, d{a[2] - a[0], a[3] - a[1]};
        t = dot(q - p0, d) / dot(d, d);
        if (o->kind != Kind::Line) t = std::max(t, 0.0);
        if (o->kind == Kind::Segment || o->kind == Kind::Vector) t = std::min(t, 1.0);
    } else if (o->kind == Kind::Circle) {
        t = wrap2Pi(angleOf(q - V2{a[0], a[1]}) - a[3]);
    } else if (o->kind == Kind::Arc) {
        Curve c;
        curveOf(s, *o, c);
        const double rel = arcParam(c, angleOf(q - c.c)), sw = std::abs(c.sweep);
        double u = rel;
        if (rel > sw) u = (rel - sw < TwoPi - rel) ? sw : 0;      // fuori: l'estremo piu' vicino
        t = sw > 0 ? u / sw : 0;
    } else if (o->kind == Kind::Function) {
        t = x;
    } else return false;
    return curvePoint(s, curve, t, px, py);
}

// ---- selezione col puntatore ------------------------------------------------------------
namespace {
double distToCurve(const State& s, const Object& o, V2 q) {
    const auto& a = o.g.a;
    if (isLineLike(o.kind)) {
        const V2 p0{a[0], a[1]}, d{a[2] - a[0], a[3] - a[1]};
        double t = dot(q - p0, d) / dot(d, d);
        if (o.kind != Kind::Line) t = std::max(t, 0.0);
        if (o.kind == Kind::Segment || o.kind == Kind::Vector) t = std::min(t, 1.0);
        return norm(q - (p0 + t * d));
    }
    if (o.kind == Kind::Circle) return std::abs(norm(q - V2{a[0], a[1]}) - a[2]);
    if (o.kind == Kind::Arc) {
        double t, x, y;
        if (!project(s, o.id, q.x, q.y, t, x, y)) return HUGE_VAL;
        return norm(q - V2{x, y});
    }
    if (o.kind == Kind::Function) {
        double y;
        if (!evalFunction(s, o, q.x, y)) return HUGE_VAL;
        return std::abs(y - q.y);
    }
    return HUGE_VAL;
}
bool insidePolygon(const State& s, const Object& o, V2 q) {
    bool in = false;
    const std::size_t n = o.parents.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const Shape* a = shapeOf(s, o.parents[i]);
        const Shape* b = shapeOf(s, o.parents[j]);
        if (!a || !b) return false;
        const V2 pi = pointOf(*a), pj = pointOf(*b);
        if ((pi.y > q.y) != (pj.y > q.y) && q.x < (pj.x - pi.x) * (q.y - pi.y) / (pj.y - pi.y) + pi.x) in = !in;
    }
    return in;
}
} // namespace
Id hitCurve(const State& s, double x, double y, double tol) {
    Id best = NoId;
    double bestD = tol;
    for (const auto& o : s.objects) {
        if (o.hidden || !o.g.ok || !isCurve(o.kind)) continue;
        const double d = distToCurve(s, o, {x, y});
        if (d <= bestD) { bestD = d; best = o.id; }
    }
    return best;
}
Id hitTest(const State& s, double x, double y, double tol, bool pointsOnly) {
    Id best = NoId;
    double bestD = tol;
    for (const auto& o : s.objects) {
        if (o.hidden || !o.g.ok || o.kind != Kind::Point) continue;
        const double d = norm(V2{x, y} - pointOf(o.g));
        if (d <= bestD) { bestD = d; best = o.id; }
    }
    if (best != NoId || pointsOnly) return best;
    best = hitCurve(s, x, y, tol);
    if (best != NoId) return best;
    for (auto it = s.objects.rbegin(); it != s.objects.rend(); ++it)
        if (!it->hidden && it->g.ok && it->kind == Kind::Polygon && insidePolygon(s, *it, {x, y})) return it->id;
    return NoId;
}

// ---- vista ---------------------------------------------------------------------------
void toScreen(const View& v, int w, int h, double x, double y, double& sx, double& sy) noexcept {
    sx = w / 2.0 + (x - v.cx) * v.scale;
    sy = h / 2.0 - (y - v.cy) * v.scale;
}
void toPlane(const View& v, int w, int h, double sx, double sy, double& x, double& y) noexcept {
    x = v.cx + (sx - w / 2.0) / v.scale;
    y = v.cy - (sy - h / 2.0) / v.scale;
}
void zoom(View& v, double factor) noexcept { v.scale = std::clamp(v.scale * factor, MinScale, MaxScale); }
void zoomBox(View& v, int w, int h, double x0, double y0, double x1, double y1) noexcept {
    const double bw = std::abs(x1 - x0), bh = std::abs(y1 - y0);
    if (!(bw > 0) && !(bh > 0)) return;
    v.cx = (x0 + x1) / 2;
    v.cy = (y0 + y1) / 2;
    const double sx = bw > 0 ? w / bw : HUGE_VAL, sy = bh > 0 ? h / bh : HUGE_VAL;
    v.scale = std::clamp(std::min(sx, sy), MinScale, MaxScale);
}
bool fit(State& s, int w, int h) {
    double x0 = HUGE_VAL, y0 = HUGE_VAL, x1 = -HUGE_VAL, y1 = -HUGE_VAL;
    auto add = [&](V2 p) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); };
    for (const auto& o : s.objects) {
        if (o.hidden || !o.g.ok) continue;
        const auto& a = o.g.a;
        switch (o.kind) {
        case Kind::Point: case Kind::Text: case Kind::Measure: case Kind::Expression: add({a[0], a[1]}); break;
        case Kind::Segment: case Kind::Line: case Kind::Ray: case Kind::Vector: add({a[0], a[1]}); add({a[2], a[3]}); break;
        case Kind::Circle: case Kind::Arc: add({a[0] - a[2], a[1] - a[2]}); add({a[0] + a[2], a[1] + a[2]}); break;
        default: break;       // poligoni (dai vertici), funzioni escluse (V06), trasformazioni
        }
    }
    if (x0 > x1) return false;
    const double pad = 0.1 * std::max({x1 - x0, y1 - y0, 1e-9});
    if (x1 - x0 < 1e-9 && y1 - y0 < 1e-9) { s.view.cx = x0; s.view.cy = y0; return true; }
    zoomBox(s.view, w, h, x0 - pad, y0 - pad, x1 + pad, y1 + pad);
    return true;
}
void setWindow(View& v, int w, double xmin, double xmax, double ymid) {
    if (!(xmin < xmax) || !std::isfinite(xmin) || !std::isfinite(xmax) || !std::isfinite(ymid)) fail("Range ERROR: Xmin < Xmax");
    const double scale = w / (xmax - xmin);
    if (scale < MinScale || scale > MaxScale) fail("Range ERROR: finestra fuori dai limiti");
    v.cx = (xmin + xmax) / 2;
    v.cy = ymid;
    v.scale = scale;
}
} // namespace fx::geo
