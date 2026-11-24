// Nucleo di Geometry (v1.19.0): casella di misura (tipi secondo la selezione,
// valori, testi), blocchi e risolutore di Gauss-Newton (ANALISI_FONTI §4.3).
#include "geometry.hpp"
#include "expression.hpp"
#include "geometry_internal.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace fx::geo {
using namespace detail;

const char* mtypeName(MType t) noexcept {
    switch (t) {
    case MType::Coordinates: return "Coordinate";
    case MType::Distance: return "Lunghezza";
    case MType::Slope: return "Pendenza";
    case MType::Direction: return "Direzione";
    case MType::Equation: return "Equazione";
    case MType::Components: return "Vettore";
    case MType::Radius: return "Raggio";
    case MType::Circumference: return "Circonferenza";
    case MType::Area: return "Area";
    case MType::Perimeter: return "Perimetro";
    case MType::Angle: return "Angolo";
    case MType::Supplementary: return "Supplementare";
    case MType::Incidence: return "Incidenza";
    case MType::Tangency: return "Tangenza";
    case MType::Congruence: return "Congruenza";
    case MType::RotationAngle: return "Ang. rotazione";
    case MType::DilationScale: return "Fattore";
    case MType::Label: return "Etichetta";
    case MType::Expression: return "Espressione";
    default: return "?";
    }
}
bool lockable(MType t) noexcept {
    switch (t) {
    case MType::Coordinates: case MType::Distance: case MType::Slope: case MType::Direction: case MType::Components: case MType::Radius:
    case MType::Circumference: case MType::Area: case MType::Angle: case MType::Supplementary: case MType::Incidence: case MType::Tangency:
    case MType::Congruence: return true;
    default: return false;
    }
}

namespace {
const Object* o(const State& s, Id id) { return find(s, id); }
Kind k(const State& s, Id id) { const Object* x = find(s, id); return x ? x->kind : Kind::Count; }
bool isRoundK(Kind x) { return x == Kind::Circle || x == Kind::Arc; }
bool isSegK(Kind x) { return x == Kind::Segment || x == Kind::Vector; }
// Vertici di segmenti che formano un percorso chiuso (Area/Perimetro di "3 lati selezionati").
bool loop(const State& s, const std::vector<Id>& segs, std::vector<V2>& out) {
    if (segs.size() < 3) return false;
    std::vector<std::pair<Id, Id>> e;
    for (Id id : segs) {
        const Object* x = o(s, id);
        if (!x || x->kind != Kind::Segment || x->def != Def::TwoPoints) return false;
        e.emplace_back(x->parents[0], x->parents[1]);
    }
    std::vector<char> used(e.size(), 0);
    std::vector<Id> chain{e[0].first, e[0].second};
    used[0] = 1;
    for (std::size_t step = 1; step < e.size(); ++step) {
        bool found = false;
        for (std::size_t i = 0; i < e.size() && !found; ++i) {
            if (used[i]) continue;
            if (e[i].first == chain.back()) { chain.push_back(e[i].second); used[i] = 1; found = true; }
            else if (e[i].second == chain.back()) { chain.push_back(e[i].first); used[i] = 1; found = true; }
        }
        if (!found) return false;
    }
    if (chain.back() != chain.front()) return false;
    chain.pop_back();
    for (Id v : chain) { const Shape* g = shapeOf(s, v); if (!g) return false; out.push_back(pointOf(*g)); }
    return true;
}
bool polyVertices(const State& s, const Object& p, std::vector<V2>& out) {
    for (Id v : p.parents) { const Shape* g = shapeOf(s, v); if (!g) return false; out.push_back(pointOf(*g)); }
    return true;
}
double area(const std::vector<V2>& v) {
    double a = 0;
    for (std::size_t i = 0; i < v.size(); ++i) a += cross(v[i], v[(i + 1) % v.size()]);
    return std::abs(a) / 2;
}
double perimeter(const std::vector<V2>& v) {
    double p = 0;
    for (std::size_t i = 0; i < v.size(); ++i) p += norm(v[(i + 1) % v.size()] - v[i]);
    return p;
}
// Due vettori dei lati di un angolo: dal vertice comune se i segmenti ne hanno uno.
bool sideVectors(const State& s, Id l1, Id l2, V2& u, V2& v) {
    const Object* a = o(s, l1);
    const Object* b = o(s, l2);
    if (!a || !b || !a->g.ok || !b->g.ok) return false;
    const V2 a0 = startOf(a->g), a1 = endOf(a->g), b0 = startOf(b->g), b1 = endOf(b->g);
    u = a1 - a0;
    v = b1 - b0;
    if (a->def == Def::TwoPoints && b->def == Def::TwoPoints) {
        // lati con un vertice comune: i due vettori partono da quel vertice
        const Id pa0 = a->parents[0], pa1 = a->parents[1], pb0 = b->parents[0], pb1 = b->parents[1];
        if (pa0 == pb1) v = b0 - b1;
        else if (pa1 == pb0) u = a0 - a1;
        else if (pa1 == pb1) { u = a0 - a1; v = b0 - b1; }
    }
    return norm(u) > 0 && norm(v) > 0;
}
double angleBetween(V2 u, V2 v) { return std::atan2(std::abs(cross(u, v)), dot(u, v)); }
bool lineOf(const State& s, Id id, V2& p, V2& d) {
    const Object* x = o(s, id);
    if (!x || !x->g.ok || !isLineLike(x->kind)) return false;
    p = startOf(x->g);
    d = endOf(x->g) - p;
    return true;
}
bool roundOf(const State& s, Id id, V2& c, double& r) {
    const Object* x = o(s, id);
    if (!x || !x->g.ok || !isRoundK(x->kind)) return false;
    c = {x->g.a[0], x->g.a[1]};
    r = x->g.a[2];
    return true;
}
// Distanza con segno di un punto da una curva (incidenza).
bool signedDistance(const State& s, Id point, Id curve, double& d) {
    const Shape* p = shapeOf(s, point);
    const Object* c = o(s, curve);
    if (!p || !c || !c->g.ok) return false;
    const V2 q = pointOf(*p);
    V2 a, dv, cc;
    double r;
    if (lineOf(s, curve, a, dv)) { d = cross(dv, q - a) / norm(dv); return true; }
    if (roundOf(s, curve, cc, r)) { d = norm(q - cc) - r; return true; }
    if (c->kind == Kind::Function) { double y; if (!evalFunction(s, *c, q.x, y)) return false; d = q.y - y; return true; }
    return false;
}
// Trasformazione di cui `image` e' l'immagine di `src` (RotationAngle, DilationScale).
const Object* transformBetween(const State& s, Id src, Id image, Def def) {
    const Object* im = o(s, image);
    if (!im || im->def != Def::Image || im->parents[0] != src) return nullptr;
    const Object* t = o(s, im->parents[1]);
    return t && t->def == def ? t : nullptr;
}
bool tangentPair(const State& s, Id a, Id b, double aux, double& r) {
    V2 p, d, c1, c2;
    double r1, r2;
    if (lineOf(s, a, p, d) && roundOf(s, b, c1, r1)) { r = std::abs(cross(d, c1 - p)) / norm(d) - r1; return true; }
    if (lineOf(s, b, p, d) && roundOf(s, a, c1, r1)) { r = std::abs(cross(d, c1 - p)) / norm(d) - r1; return true; }
    if (roundOf(s, a, c1, r1) && roundOf(s, b, c2, r2)) {
        const double dd = norm(c2 - c1);
        r = aux == 0 ? dd - (r1 + r2) : dd - std::abs(r1 - r2);
        return true;
    }
    return false;
}
double tangencyAux(const State& s, Id a, Id b) {
    V2 c1, c2;
    double r1, r2;
    if (roundOf(s, a, c1, r1) && roundOf(s, b, c2, r2)) {
        const double dd = norm(c2 - c1);
        return std::abs(dd - (r1 + r2)) <= std::abs(dd - std::abs(r1 - r2)) ? 0 : 1;
    }
    return 0;
}
double scaleOfOps(const State& s, const std::vector<Id>& ops) {
    double sc = 1;
    for (Id id : ops) if (const Object* x = o(s, id)) if (x->g.ok) for (int i = 0; i < 5; ++i) sc = std::max(sc, std::abs(x->g.a[i]));
    return sc;
}
// Valore canonico di una misura (angoli in radianti): false se indefinita.
bool raw(const State& s, const MSpec& m, double v[2], int& n, bool& yes, std::string& text) {
    text.clear();
    n = 1;
    yes = false;
    const auto& ops = m.ops;
    auto sh = [&](std::size_t i) { return i < ops.size() ? shapeOf(s, ops[i]) : nullptr; };
    switch (m.type) {
    case MType::Coordinates: {
        const Shape* g = sh(0);
        if (!g || k(s, ops[0]) != Kind::Point) return false;
        v[0] = g->a[0]; v[1] = g->a[1]; n = 2;
        return true;
    }
    case MType::Distance: {
        if (ops.size() == 2) {
            const Shape* a = sh(0);
            const Shape* b = sh(1);
            if (!a || !b) return false;
            v[0] = norm(pointOf(*b) - pointOf(*a));
            return true;
        }
        V2 p, d;
        if (!lineOf(s, ops[0], p, d) || !isSegK(k(s, ops[0]))) return false;
        v[0] = norm(d);
        return true;
    }
    case MType::Slope: case MType::Direction: case MType::Components: {
        V2 p, d;
        if (!lineOf(s, ops[0], p, d)) return false;
        if (m.type == MType::Components) { v[0] = d.x; v[1] = d.y; n = 2; return true; }
        if (m.type == MType::Direction) { v[0] = angleOf(d); return true; }
        if (std::abs(d.x) <= 1e-15 * std::abs(d.y)) return false;     // verticale: pendenza indefinita
        v[0] = d.y / d.x;
        return true;
    }
    case MType::Radius: case MType::Circumference: {
        V2 c;
        double r;
        if (!roundOf(s, ops[0], c, r)) return false;
        const Object* x = o(s, ops[0]);
        v[0] = m.type == MType::Radius ? r : (x->kind == Kind::Circle ? TwoPi * r : std::abs(x->g.a[4]) * r);
        return true;
    }
    case MType::Area: case MType::Perimeter: {
        std::vector<V2> pts;
        if (ops.size() == 1) {
            const Object* x = o(s, ops[0]);
            if (!x || !x->g.ok) return false;
            if (x->kind == Kind::Circle) { v[0] = m.type == MType::Area ? Pi * x->g.a[2] * x->g.a[2] : TwoPi * x->g.a[2]; return true; }
            if (x->kind != Kind::Polygon || !polyVertices(s, *x, pts)) return false;
        } else if (!loop(s, ops, pts)) return false;
        v[0] = m.type == MType::Area ? area(pts) : perimeter(pts);
        return true;
    }
    case MType::Angle: case MType::Supplementary: {
        double a;
        if (ops.size() == 3) {
            const Shape* p0 = sh(0);
            const Shape* p1 = sh(1);
            const Shape* p2 = sh(2);
            if (!p0 || !p1 || !p2) return false;
            const V2 u = pointOf(*p0) - pointOf(*p1), w = pointOf(*p2) - pointOf(*p1);
            if (!(norm(u) > 0) || !(norm(w) > 0)) return false;
            a = angleBetween(u, w);
        } else {
            V2 u, w;
            if (ops.size() != 2 || !sideVectors(s, ops[0], ops[1], u, w)) return false;
            a = angleBetween(u, w);
        }
        v[0] = m.type == MType::Angle ? a : Pi - a;
        return true;
    }
    case MType::Incidence: {
        if (ops.size() != 2) return false;
        const bool firstPoint = k(s, ops[0]) == Kind::Point;
        double d;
        if (!signedDistance(s, firstPoint ? ops[0] : ops[1], firstPoint ? ops[1] : ops[0], d)) return false;
        v[0] = d; n = 0;
        yes = std::abs(d) <= EpsYesNo * scaleOfOps(s, ops);
        return true;
    }
    case MType::Tangency: {
        double r;
        if (ops.size() != 2 || !tangentPair(s, ops[0], ops[1], tangencyAux(s, ops[0], ops[1]), r)) return false;
        v[0] = r; n = 0;
        yes = std::abs(r) <= EpsYesNo * scaleOfOps(s, ops);
        return true;
    }
    case MType::Congruence: {
        V2 p, d1, d2;
        if (ops.size() != 2 || !lineOf(s, ops[0], p, d1) || !lineOf(s, ops[1], p, d2)) return false;
        v[0] = norm(d1) - norm(d2); n = 0;
        yes = std::abs(v[0]) <= EpsYesNo * scaleOfOps(s, ops);
        return true;
    }
    case MType::RotationAngle: case MType::DilationScale: {
        if (ops.size() != 2) return false;
        const Def d = m.type == MType::RotationAngle ? Def::Rotate : Def::Dilate;
        const Object* t = transformBetween(s, ops[0], ops[1], d);
        if (!t) t = transformBetween(s, ops[1], ops[0], d);
        if (!t) return false;
        v[0] = t->vars[0];
        return true;
    }
    case MType::Equation: case MType::Label: case MType::Expression:
        n = 0;
        return true;
    default: return false;
    }
}
bool isAngle(MType t) { return t == MType::Angle || t == MType::Supplementary || t == MType::Direction || t == MType::RotationAngle; }
} // namespace

std::vector<MSpec> available(const State& s, const std::vector<Id>& sel) {
    std::vector<MSpec> out;
    auto add = [&](MType t, std::vector<Id> ops) { out.push_back(MSpec{t, std::move(ops)}); };
    std::vector<Id> pts, lines, rounds, polys, funcs, others;
    for (Id id : sel) {
        switch (k(s, id)) {
        case Kind::Point: pts.push_back(id); break;
        case Kind::Segment: case Kind::Line: case Kind::Ray: case Kind::Vector: lines.push_back(id); break;
        case Kind::Circle: case Kind::Arc: rounds.push_back(id); break;
        case Kind::Polygon: polys.push_back(id); break;
        case Kind::Function: funcs.push_back(id); break;
        default: others.push_back(id); break;
        }
    }
    const std::size_t n = sel.size();
    if (n == 1) {
        const Id id = sel[0];
        switch (k(s, id)) {
        case Kind::Point: add(MType::Coordinates, {id}); break;
        case Kind::Segment: add(MType::Distance, {id}); add(MType::Slope, {id}); add(MType::Direction, {id}); add(MType::Equation, {id}); break;
        case Kind::Vector: add(MType::Distance, {id}); add(MType::Components, {id}); add(MType::Slope, {id}); add(MType::Direction, {id}); add(MType::Equation, {id}); break;
        case Kind::Line: case Kind::Ray: add(MType::Slope, {id}); add(MType::Direction, {id}); add(MType::Equation, {id}); break;
        case Kind::Circle: add(MType::Radius, {id}); add(MType::Circumference, {id}); add(MType::Area, {id}); add(MType::Equation, {id}); break;
        case Kind::Arc: add(MType::Radius, {id}); add(MType::Circumference, {id}); add(MType::Equation, {id}); break;
        case Kind::Polygon: add(MType::Area, {id}); add(MType::Perimeter, {id}); break;
        case Kind::Function: add(MType::Equation, {id}); break;
        case Kind::Expression: add(MType::Expression, {id}); break;
        default: break;
        }
        if (k(s, id) != Kind::Function && k(s, id) != Kind::Expression && k(s, id) != Kind::Transform && k(s, id) != Kind::Count) add(MType::Label, {id});
        return out;
    }
    if (n == pts.size()) {
        if (n == 2) {
            add(MType::Distance, sel);
            for (Def d : {Def::Rotate, Def::Dilate})
                if (transformBetween(s, sel[0], sel[1], d) || transformBetween(s, sel[1], sel[0], d))
                    add(d == Def::Rotate ? MType::RotationAngle : MType::DilationScale, sel);
        } else if (n == 3) add(MType::Angle, sel);
        return out;
    }
    if (n == 2 && lines.size() == 2) {
        add(MType::Angle, sel);
        add(MType::Supplementary, sel);
        if (isSegK(k(s, sel[0])) && isSegK(k(s, sel[1]))) add(MType::Congruence, sel);
        return out;
    }
    if (n == 2 && pts.size() == 1 && (lines.size() + rounds.size() + funcs.size()) == 1) { add(MType::Incidence, sel); return out; }
    if (n == 2 && ((lines.size() == 1 && rounds.size() == 1) || rounds.size() == 2)) { add(MType::Tangency, sel); return out; }
    if (n >= 3 && lines.size() == n) {
        std::vector<V2> v;
        if (loop(s, sel, v)) { add(MType::Area, sel); add(MType::Perimeter, sel); }
    }
    return out;
}

Measurement measure(const State& s, const MSpec& m) {
    Measurement r;
    double v[2]{};
    int n = 0;
    bool yes = false;
    std::string text;
    r.ok = raw(s, m, v, n, yes, text);
    if (!r.ok) return r;
    r.count = n;
    r.yesNo = m.type == MType::Incidence || m.type == MType::Tangency || m.type == MType::Congruence;
    r.yes = yes;
    r.v[0] = isAngle(m.type) && m.type != MType::DilationScale ? fromRadians(s.options, v[0]) : v[0];
    r.v[1] = v[1];
    const Object* first = m.ops.empty() ? nullptr : find(s, m.ops[0]);
    if (m.type == MType::Label) {
        if (!first) { r.ok = false; return r; }
        r.text = first->label;
    } else if (m.type == MType::Expression) {
        if (!first) { r.ok = false; return r; }
        r.text = first->text;
        // valore: @n sostituiti dai valori delle misure incollate
        std::string expr;
        const std::string& f = first->text;
        for (std::size_t i = 0; i < f.size(); ++i) {
            if (f[i] != '@') { expr += f[i]; continue; }
            std::size_t j = i + 1;
            unsigned num = 0;
            while (j < f.size() && std::isdigit(static_cast<unsigned char>(f[j]))) num = num * 10 + static_cast<unsigned>(f[j++] - '0');
            int comp = 0;
            if (j < f.size() && (f[j] == 'X' || f[j] == 'Y')) { comp = f[j] == 'X' ? 1 : 2; ++j; }
            const Object* mo = nullptr;
            for (Id p : first->parents) if (const Object* x = find(s, p)) if (x->seq == num) mo = x;
            if (!mo) { r.ok = false; return r; }
            const Measurement mv = measure(s, MSpec{static_cast<MType>(mo->flag), mo->parents});
            if (!mv.ok || mv.count == 0) { r.ok = false; return r; }
            char b[40];
            std::snprintf(b, sizeof b, "(%.17g)", comp == 2 && mv.count == 2 ? mv.v[1] : mv.v[0]);
            expr += b;
            i = j - 1;
        }
        try {
            r.v[0] = evaluateReal(*parseExpression(expr), EvalContext{});
            r.count = std::isfinite(r.v[0]) ? 1 : 0;
            r.ok = r.count == 1;
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception&) {
            r.ok = false;
        }
    } else if (m.type == MType::Equation) {
        if (!first || !first->g.ok) { r.ok = false; return r; }
        const std::uint8_t fmt = s.options.numFormat;
        auto num = [&](double x) { return formatNumber(x, fmt); };
        auto signed_ = [&](double x) { return std::string(x < 0 ? "-" : "+") + num(std::abs(x)); };
        const auto& a = first->g.a;
        if (isLineLike(first->kind)) {
            const double dx = a[2] - a[0], dy = a[3] - a[1];
            if (std::abs(dx) <= 1e-15 * std::abs(dy)) r.text = "x=" + num(a[0]);
            else { const double mm = dy / dx; r.text = "y=" + num(mm) + "x" + signed_(a[1] - mm * a[0]); }
        } else if (isRoundK(first->kind)) {
            auto square = [&](const char* v, double c) { return c == 0 ? std::string(v) + "\xC2\xB2" : "(" + std::string(v) + signed_(-c) + ")\xC2\xB2"; };
            r.text = square("x", a[0]) + "+" + square("y", a[1]) + "=" + num(a[2] * a[2]);
        } else if (first->kind == Kind::Function) {
            r.text = "y=" + first->text;
        } else r.ok = false;
    }
    return r;
}

std::string formatNumber(double v, std::uint8_t fmt) {
    if (!std::isfinite(v)) return "Undefined";
    if (std::abs(v) < 1e-300) v = 0;
    char b[48];
    const double rv = std::round(v);
    // interi senza decimali (a meno dell'arrotondamento dei calcoli: 1e-9 relativo)
    if (std::abs(v) < 1e10 && std::abs(v - rv) <= 1e-9 * std::max(1.0, std::abs(v))) { std::snprintf(b, sizeof b, "%.0f", rv == 0 ? 0.0 : rv); return b; }
    if (fmt <= 9) {
        std::snprintf(b, sizeof b, "%.*f", static_cast<int>(fmt), v);
        std::string out = b;
        if (out == "-0" || out.find_first_not_of("-0.") == std::string::npos) { if (out[0] == '-') out.erase(0, 1); }
        return out;
    }
    if (fmt >= FmtSci1 && fmt < FmtNorm) { std::snprintf(b, sizeof b, "%.*E", static_cast<int>(fmt - FmtSci1), v); return b; }
    std::snprintf(b, sizeof b, "%.10G", v);
    return b;
}
std::string measureValueText(const State& s, const MSpec& m, std::uint8_t fmt) {
    const Measurement r = measure(s, m);
    if (!r.ok) return "Undefined";
    if (r.yesNo) return r.yes ? "S\xC3\xAC" : "No";
    if (m.type == MType::Label || m.type == MType::Equation) return r.text;
    if (r.count == 2) return "(" + formatNumber(r.v[0], fmt) + ", " + formatNumber(r.v[1], fmt) + ")";
    std::string t = formatNumber(r.v[0], fmt);
    if (isAngle(m.type) && s.options.angle == 0) t += "\xC2\xB0";
    return t;
}
std::string measureText(const State& s, const Object& p) {
    if (p.kind == Kind::Text) return p.text;
    const std::uint8_t fmt = p.fmt == 0xFF ? s.options.numFormat : p.fmt;
    if (p.kind == Kind::Expression) {
        const Measurement r = measure(s, MSpec{MType::Expression, {p.id}});
        return p.text + "=" + (r.ok ? formatNumber(r.v[0], fmt) : std::string("Undefined"));
    }
    if (p.kind != Kind::Measure) return p.label;
    const MType t = static_cast<MType>(p.flag);
    std::string out = p.label + "=" + measureValueText(s, MSpec{t, p.parents}, fmt);
    if (s.options.lengthUnit != 0 && (t == MType::Distance || t == MType::Radius || t == MType::Circumference || t == MType::Perimeter))
        out += std::string(" ") + lengthUnitName(s.options.lengthUnit);
    if (s.options.lengthUnit != 0 && t == MType::Area) out += std::string(" ") + lengthUnitName(s.options.lengthUnit) + "\xC2\xB2";
    return out;
}

// ---- blocchi ----------------------------------------------------------------------------
std::size_t residualCount(const Lock& l) noexcept { return l.m.type == MType::Coordinates || l.m.type == MType::Components ? 2 : 1; }
double residual(const State& s, const Lock& l, std::size_t i) noexcept {
    double v[2]{};
    int n = 0;
    bool yes = false;
    std::string text;
    try {
        if (l.m.type == MType::Tangency) {
            double r;
            if (l.m.ops.size() != 2 || !tangentPair(s, l.m.ops[0], l.m.ops[1], l.aux, r)) return 0;
            return r;
        }
        if (!raw(s, l.m, v, n, yes, text)) return 0;    // indefinita: il blocco non agisce
    } catch (...) {
        return 0;
    }
    switch (l.m.type) {
    case MType::Slope: {
        V2 p, d;
        if (!lineOf(s, l.m.ops[0], p, d)) return 0;
        return (d.x * l.v[0] - d.y) / (std::sqrt(1 + l.v[0] * l.v[0]) * norm(d));
    }
    case MType::Direction: return wrapPi(v[0] - l.v[0]);
    case MType::Incidence: case MType::Congruence: return v[0];
    default: return v[i] - l.v[i];
    }
}
bool isLocked(const State& s, const MSpec& m) noexcept {
    for (const auto& l : s.locks) if (l.m == m) return true;
    return false;
}
namespace {
std::vector<std::vector<double>> snapVars(const State& s) {
    std::vector<std::vector<double>> v;
    v.reserve(s.objects.size());
    for (const auto& x : s.objects) v.push_back(x.vars);
    return v;
}
void restoreVars(State& s, const std::vector<std::vector<double>>& v) {
    for (std::size_t i = 0; i < s.objects.size() && i < v.size(); ++i) s.objects[i].vars = v[i];
    recompute(s);
}
void checkSpec(const State& s, const MSpec& m) {
    if (m.ops.empty()) fail("Argument ERROR: misura senza oggetti");
    for (Id id : m.ops) if (!find(s, id)) fail("Argument ERROR: oggetto inesistente");
    bool ok = false;
    for (const auto& a : available(s, m.ops)) ok = ok || a == m;
    if (!ok) fail("Argument ERROR: misura non disponibile per la selezione");
}
// Aggiunge (o sostituisce) il blocco e risolve; con un errore tutto torna com'era.
void applyLock(State& s, const Lock& l) {
    const auto vars = snapVars(s);
    const auto locks = s.locks;
    putLock(s, l);
    if (!solve(s, {})) {
        s.locks = locks;
        restoreVars(s, vars);
        fail("Math ERROR: vincoli incompatibili");
    }
}
} // namespace
void setValue(State& s, const MSpec& m, double v0, double v1, const std::string& text) {
    checkSpec(s, m);
    Object& first = s.objects[at(s, m.ops[0])];
    switch (m.type) {
    case MType::Label:
        if (text.size() > MaxLabel) fail("Argument ERROR: al piu' 14 caratteri");
        first.label = text;
        first.showLabel = !text.empty();
        return;
    case MType::Expression: {
        if (text.empty() || text.size() > MaxFormula) fail("Argument ERROR: espressione");
        // stessa verifica dei riferimenti @n di addExpression
        const double x = first.vars[0], y = first.vars[1];
        State t = s;
        const Id id = addExpression(t, x, y, text);
        Object& o = s.objects[at(s, m.ops[0])];
        const Object* made = find(t, id);
        if (!made) fail("Argument ERROR: espressione");
        o.text = text;
        o.parents = made->parents;
        recompute(s, {o.id});
        return;
    }
    case MType::RotationAngle: case MType::DilationScale: {
        const Def d = m.type == MType::RotationAngle ? Def::Rotate : Def::Dilate;
        const Object* t = transformBetween(s, m.ops[0], m.ops[1], d);
        if (!t) t = transformBetween(s, m.ops[1], m.ops[0], d);
        if (!t) fail("Argument ERROR: misura non disponibile");
        if (!std::isfinite(v0)) fail("Math ERROR: valore");
        const double val = d == Def::Rotate ? toRadians(s.options, v0) : v0;
        if (d == Def::Dilate && !(std::abs(val) >= MinDilation && std::abs(val) <= MaxDilation)) fail("Argument ERROR: fattore con 0.1 <= |k| <= 10");
        Object& tm = s.objects[at(s, t->id)];
        tm.vars[0] = val;
        recompute(s, {tm.id});
        return;
    }
    default: break;
    }
    if (!lockable(m.type)) fail("Argument ERROR: misura non modificabile");
    if (m.type == MType::Incidence || m.type == MType::Tangency || m.type == MType::Congruence) { lock(s, m); return; }
    applyLock(s, prepareValue(s, m, v0, v1));
}
Lock prepareValue(const State& s, const MSpec& m, double v0, double v1) {
    checkSpec(s, m);
    if (!lockable(m.type) || m.type == MType::Incidence || m.type == MType::Tangency || m.type == MType::Congruence) fail("Argument ERROR: misura non modificabile");
    if (!std::isfinite(v0) || !std::isfinite(v1) || std::abs(v0) >= 1e100 || std::abs(v1) >= 1e100) fail("Math ERROR: valore");
    Lock l;
    l.m = m;
    l.v[0] = isAngle(m.type) ? toRadians(s.options, v0) : v0;
    l.v[1] = v1;
    if ((m.type == MType::Distance || m.type == MType::Radius || m.type == MType::Circumference || m.type == MType::Area) && !(l.v[0] > 0))
        fail("Math ERROR: valore non positivo");
    if ((m.type == MType::Angle || m.type == MType::Supplementary) && !(l.v[0] >= 0 && l.v[0] <= Pi)) fail("Math ERROR: angolo da 0 a 180");
    if (s.locks.size() >= MaxLocks && !isLocked(s, m)) fail("Memory ERROR: oltre 64 blocchi");
    return l;
}
Lock prepareLock(const State& s, const MSpec& m) {
    checkSpec(s, m);
    if (!lockable(m.type)) fail("Argument ERROR: misura non bloccabile");
    if (s.locks.size() >= MaxLocks && !isLocked(s, m)) fail("Memory ERROR: oltre 64 blocchi");
    Lock l;
    l.m = m;
    double v[2]{};
    int n = 0;
    bool yes = false;
    std::string text;
    if (!raw(s, m, v, n, yes, text)) fail("Math ERROR: misura indefinita");
    l.v[0] = v[0];
    l.v[1] = v[1];
    if (m.type == MType::Incidence || m.type == MType::Congruence) l.v[0] = l.v[1] = 0;
    if (m.type == MType::Tangency) { l.v[0] = 0; l.aux = tangencyAux(s, m.ops[0], m.ops[1]); }
    return l;
}
void putLock(State& s, const Lock& l) {
    s.locks.erase(std::remove_if(s.locks.begin(), s.locks.end(), [&](const Lock& x) { return x.m == l.m; }), s.locks.end());
    // coordinate di un punto libero: il punto va subito al valore
    if (l.m.type == MType::Coordinates) {
        Object* p = find(s, l.m.ops[0]);
        if (p && p->def == Def::Free) { p->vars = {l.v[0], l.v[1]}; recompute(s, {p->id}); }
    }
    s.locks.push_back(l);
}
void lock(State& s, const MSpec& m) { applyLock(s, prepareLock(s, m)); }
void unlock(State& s, const MSpec& m) {
    s.locks.erase(std::remove_if(s.locks.begin(), s.locks.end(), [&](const Lock& x) { return x.m == m; }), s.locks.end());
}

// ---- risolutore ------------------------------------------------------------------------
SolveTask::SolveTask(State& s, std::vector<Id> pinned) : s_(s) {
    // scalari liberi antenati degli operandi dei blocchi
    std::vector<char> anc(s.objects.size(), 0);
    for (const auto& l : s.locks) for (Id id : l.m.ops) { const int i = indexOf(s, id); if (i >= 0) anc[static_cast<std::size_t>(i)] = 1; }
    for (std::size_t i = s.objects.size(); i-- > 0;) {
        if (!anc[i]) continue;
        for (Id p : s.objects[i].parents) { const int j = indexOf(s, p); if (j >= 0) anc[static_cast<std::size_t>(j)] = 1; }
    }
    auto locked = [&](Id id) {
        for (const auto& l : s.locks) if (l.m.type == MType::Coordinates && l.m.ops[0] == id && find(s, id)->def == Def::Free) return true;
        return false;
    };
    for (std::size_t i = 0; i < s.objects.size(); ++i) {
        if (!anc[i]) continue;
        const Object& o = s.objects[i];
        if (o.def == Def::Free && locked(o.id)) continue;
        const bool pin = std::find(pinned.begin(), pinned.end(), o.id) != pinned.end();
        for (std::size_t k = 0; k < o.vars.size(); ++k)
            if (solverVar(o, k)) { vars_.push_back(Var{i, k, pin ? 1e4 : 1.0}); start_.push_back(o.vars[k]); }
    }
}
void SolveTask::setVar(std::size_t j, double v) {
    Object& o = s_.objects[vars_[j].obj];
    o.vars[vars_[j].k] = v;
    recompute(s_, {o.id});
}
double SolveTask::eval(std::vector<double>& r) {
    r.clear();
    double worst = 0;
    for (const auto& l : s_.locks) {
        const double sc = std::max({1.0, std::abs(l.v[0]), std::abs(l.v[1])});
        for (std::size_t i = 0; i < residualCount(l); ++i) {
            const double x = residual(s_, l, i) / sc;
            r.push_back(std::isfinite(x) ? x : 1e30);
            worst = std::max(worst, std::abs(r.back()));
        }
    }
    return worst;
}
bool SolveTask::step(step::Budget& budget) {
    if (ok_ || failed_) return true;
    const std::size_t n = vars_.size();
    std::vector<double> r, rj;
    for (;;) {
        const double worst = eval(r);
        budget.consume(1);
        if (worst <= EpsSolve) { ok_ = true; return true; }
        if (n == 0 || iter_ >= 60) { failed_ = true; cancelled(); return true; }
        ++iter_;
        const std::size_t m = r.size();
        // Jacobiana per differenze in avanti
        std::vector<double> J(m * n);
        for (std::size_t j = 0; j < n; ++j) {
            const double x = s_.objects[vars_[j].obj].vars[vars_[j].k];
            const double h = 1e-7 * std::max(1.0, std::abs(x));
            setVar(j, x + h);
            eval(rj);
            setVar(j, x);
            for (std::size_t i = 0; i < m; ++i) J[i * n + j] = (rj[i] - r[i]) / h;
        }
        budget.consume(2 * n);
        // (J W^-1 J^T + mu I) y = r
        std::vector<double> A(m * m, 0.0);
        double tr = 0;
        for (std::size_t a = 0; a < m; ++a)
            for (std::size_t b = 0; b < m; ++b) {
                double sum = 0;
                for (std::size_t j = 0; j < n; ++j) sum += J[a * n + j] * J[b * n + j] / vars_[j].weight;
                A[a * m + b] = sum;
            }
        for (std::size_t a = 0; a < m; ++a) tr += A[a * m + a];
        const double mu = 1e-12 * tr + 1e-300;
        for (std::size_t a = 0; a < m; ++a) A[a * m + a] += mu;
        std::vector<double> y = r;
        for (std::size_t c = 0; c < m; ++c) {              // Gauss con pivot parziale
            std::size_t piv = c;
            for (std::size_t a = c + 1; a < m; ++a) if (std::abs(A[a * m + c]) > std::abs(A[piv * m + c])) piv = a;
            if (std::abs(A[piv * m + c]) < 1e-300) { failed_ = true; cancelled(); return true; }
            if (piv != c) { for (std::size_t b = 0; b < m; ++b) std::swap(A[c * m + b], A[piv * m + b]); std::swap(y[c], y[piv]); }
            for (std::size_t a = c + 1; a < m; ++a) {
                const double f = A[a * m + c] / A[c * m + c];
                if (f == 0) continue;
                for (std::size_t b = c; b < m; ++b) A[a * m + b] -= f * A[c * m + b];
                y[a] -= f * y[c];
            }
        }
        for (std::size_t c = m; c-- > 0;) {
            double sum = y[c];
            for (std::size_t b = c + 1; b < m; ++b) sum -= A[c * m + b] * y[b];
            y[c] = sum / A[c * m + c];
        }
        std::vector<double> dx(n, 0.0), x0(n);
        for (std::size_t j = 0; j < n; ++j) {
            x0[j] = s_.objects[vars_[j].obj].vars[vars_[j].k];
            double sum = 0;
            for (std::size_t a = 0; a < m; ++a) sum += J[a * n + j] * y[a];
            dx[j] = -sum / vars_[j].weight;
        }
        // ricerca lineare sulla somma dei quadrati
        double f0 = 0;
        for (double v : r) f0 += v * v;
        double alpha = 1;
        bool better = false;
        for (int ls = 0; ls < 12 && !better; ++ls, alpha /= 2) {
            for (std::size_t j = 0; j < n; ++j) s_.objects[vars_[j].obj].vars[vars_[j].k] = x0[j] + alpha * dx[j];
            recompute(s_);
            eval(rj);
            double f1 = 0;
            for (double v : rj) f1 += v * v;
            budget.consume(1);
            better = f1 < f0;
        }
        if (!better) {
            for (std::size_t j = 0; j < n; ++j) s_.objects[vars_[j].obj].vars[vars_[j].k] = x0[j];
            recompute(s_);
            failed_ = true;
            cancelled();
            return true;
        }
        if (budget.exhausted()) return false;
    }
}
void SolveTask::cancelled() {
    for (std::size_t j = 0; j < vars_.size(); ++j) s_.objects[vars_[j].obj].vars[vars_[j].k] = start_[j];
    recompute(s_);
}
bool solve(State& s, const std::vector<Id>& pinned) {
    if (s.locks.empty()) return true;
    SolveTask t(s, pinned);
    step::Budget b;
    while (!t.step(b)) {}
    return t.ok();
}
} // namespace fx::geo
