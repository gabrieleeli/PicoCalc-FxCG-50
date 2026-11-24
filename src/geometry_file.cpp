// Nucleo di Geometry (v1.19.0): sezione 0x0110 del record v12 e file .pcgeo
// versione 2, con la lettura (e la migrazione) del formato 1 della v1.17.0.
// La decodifica ha la garanzia forte: o tutto valido, o nulla cambia.
#include "geometry.hpp"
#include "expression.hpp"
#include "geometry_internal.hpp"
#include "mode_codec.hpp"
#include <algorithm>

namespace fx::geo {
using namespace detail;

void reset(State& s) { s = State{}; }

namespace {
constexpr std::size_t MaxBackground = 64;
void writeSpec(codec::Writer& w, const MSpec& m) {
    w.u8(static_cast<std::uint8_t>(m.type));
    w.u8(static_cast<std::uint8_t>(m.ops.size()));
    for (Id id : m.ops) w.u16(id);
}
void writeBody(codec::Writer& w, const State& s) {
    w.u16(static_cast<std::uint16_t>(s.objects.size()));
    for (const Object& o : s.objects) {
        w.u16(o.id);
        w.u8(static_cast<std::uint8_t>(o.kind));
        w.u8(static_cast<std::uint8_t>(o.def));
        w.u8(o.flag);
        w.u8(o.flag2);
        w.u8(static_cast<std::uint8_t>(o.parents.size()));
        for (Id p : o.parents) w.u16(p);
        w.u8(static_cast<std::uint8_t>(o.vars.size()));
        for (double v : o.vars) w.f64(v);
        w.str(o.label);
        w.str(o.text);
        w.u8(o.style.line); w.u8(static_cast<std::uint8_t>(o.style.lineStyle)); w.u8(o.style.area); w.u8(o.style.paint); w.u8(o.style.chars);
        w.u8(static_cast<std::uint8_t>((o.hidden ? 1 : 0) | (o.showLabel ? 2 : 0)));
        w.u16(o.figure);
        w.u16(static_cast<std::uint16_t>(o.z));
        w.u8(o.fmt);
        w.u16(o.seq);
    }
    w.u8(static_cast<std::uint8_t>(s.locks.size()));
    for (const Lock& l : s.locks) { writeSpec(w, l.m); w.f64(l.v[0]); w.f64(l.v[1]); w.f64(l.aux); }
    w.u8(static_cast<std::uint8_t>(s.animations.size()));
    for (const Animation& a : s.animations) { w.u16(a.point); w.u16(a.path); w.u8(a.steps); w.f64(a.t0); w.f64(a.t1); w.u16(a.times); }
    w.u16(static_cast<std::uint16_t>(s.traced.size()));
    for (Id id : s.traced) w.u16(id);
    w.u8(static_cast<std::uint8_t>(s.columns.size()));
    for (const TableColumn& c : s.columns) { writeSpec(w, c.m); w.u8(c.comp); w.str(c.name); }
    w.u16(s.tableRows);
    for (double v : s.table) w.f64(v);
    const Options& op = s.options;
    w.u8(op.numFormat); w.u8(op.angle); w.u8(op.lengthUnit); w.u8(static_cast<std::uint8_t>(op.grid)); w.f64(op.gridSpace); w.u8(static_cast<std::uint8_t>(op.axes));
    w.f64(s.view.cx); w.f64(s.view.cy); w.f64(s.view.scale);
    w.str(s.background);
    w.u8(s.fade);
    w.u16(s.nextId); w.u16(s.nextFigure); w.u16(s.nextLabel); w.u16(s.nextSeq);
}
bool finiteValue(double v) { return std::isfinite(v) && std::abs(v) < 1e150; }
bool readSpec(codec::Reader& r, const State& t, MSpec& m) {
    const std::uint8_t type = r.u8(), n = r.u8();
    if (!r.ok || type >= static_cast<std::uint8_t>(MType::Count) || n == 0 || n > 3) return false;
    m.type = static_cast<MType>(type);
    for (std::uint8_t i = 0; i < n; ++i) { const Id id = r.u16(); if (!find(t, id)) return false; m.ops.push_back(id); }
    return r.ok;
}
bool readBody(codec::Reader& r, State& t) {
    const std::size_t n = r.u16();
    if (!r.ok || n > maxObjects()) return false;
    t.objects.reserve(n);
    Id prev = NoId;
    for (std::size_t i = 0; i < n; ++i) {
        Object o;
        o.id = r.u16();
        const std::uint8_t kind = r.u8(), def = r.u8();
        o.flag = r.u8();
        o.flag2 = r.u8();
        if (!r.ok || o.id == NoId || o.id <= prev || kind >= static_cast<std::uint8_t>(Kind::Count) || def >= static_cast<std::uint8_t>(Def::Count)) return false;
        o.kind = static_cast<Kind>(kind);
        o.def = static_cast<Def>(def);
        const std::size_t np = r.u8();
        if (np > MaxVertices) return false;
        for (std::size_t k = 0; k < np; ++k) { const Id p = r.u16(); if (p == NoId || p >= o.id) return false; o.parents.push_back(p); }
        const std::size_t nv = r.u8();
        if (nv > 4) return false;
        for (std::size_t k = 0; k < nv; ++k) o.vars.push_back(r.f64());
        o.label = r.str(MaxLabel);
        o.text = r.str(MaxFormula);
        o.style.line = r.u8();
        const std::uint8_t ls = r.u8();
        o.style.area = r.u8();
        o.style.paint = r.u8();
        o.style.chars = r.u8();
        if (ls >= static_cast<std::uint8_t>(LineStyle::Count)) return false;
        o.style.lineStyle = static_cast<LineStyle>(ls);
        const std::uint8_t bits = r.u8();
        if (bits > 3) return false;
        o.hidden = (bits & 1) != 0;
        o.showLabel = (bits & 2) != 0;
        o.figure = r.u16();
        o.z = static_cast<std::int16_t>(r.u16());
        o.fmt = r.u8();
        o.seq = r.u16();
        if (!r.ok || !validate(t, o).empty()) return false;
        prev = o.id;
        t.objects.push_back(std::move(o));
    }
    const std::size_t nl = r.u8();
    if (nl > MaxLocks) return false;
    for (std::size_t i = 0; i < nl && r.ok; ++i) {
        Lock l;
        if (!readSpec(r, t, l.m) || !lockable(l.m.type)) return false;
        l.v[0] = r.f64(); l.v[1] = r.f64(); l.aux = r.f64();
        if (!finiteValue(l.v[0]) || !finiteValue(l.v[1]) || !(l.aux == 0 || l.aux == 1)) return false;
        t.locks.push_back(std::move(l));
    }
    const std::size_t na = r.u8();
    if (na > MaxAnimations) return false;
    for (std::size_t i = 0; i < na && r.ok; ++i) {
        Animation a;
        a.point = r.u16(); a.path = r.u16(); a.steps = r.u8(); a.t0 = r.f64(); a.t1 = r.f64(); a.times = r.u16();
        const Object* p = find(t, a.point);
        const Object* c = find(t, a.path);
        if (!r.ok || !p || !c || p->kind != Kind::Point || !isCurve(c->kind) || a.steps < MinSteps || a.steps > MaxSteps || a.times > 9999) return false;
        const double lim = c->kind == Kind::Function ? 1e6 : MaxT;
        if (!(a.t0 >= -lim && a.t0 <= lim) || !(a.t1 >= -lim && a.t1 <= lim)) return false;
        for (const auto& x : t.animations) if (x.point == a.point) return false;
        t.animations.push_back(a);
    }
    const std::size_t nt = r.u16();
    if (nt > t.objects.size()) return false;
    Id last = NoId;
    for (std::size_t i = 0; i < nt && r.ok; ++i) {
        const Id id = r.u16();
        const Object* p = find(t, id);
        if (!p || p->kind != Kind::Point || id <= last) return false;
        last = id;
        t.traced.push_back(id);
    }
    const std::size_t nc = r.u8();
    if (nc > MaxColumns) return false;
    for (std::size_t i = 0; i < nc && r.ok; ++i) {
        TableColumn c;
        if (!readSpec(r, t, c.m)) return false;
        c.comp = r.u8();
        c.name = r.str(MaxLabel);
        if (!r.ok || c.comp > 1 || c.m.type == MType::Label || c.m.type == MType::Equation || c.m.type == MType::Expression) return false;
        t.columns.push_back(std::move(c));
    }
    t.tableRows = r.u16();
    if (t.tableRows > MaxRows || (nc == 0 && t.tableRows != 0)) return false;
    t.table.resize(static_cast<std::size_t>(t.tableRows) * nc);
    for (double& v : t.table) { v = r.f64(); if (!(finiteValue(v) || v == NoValue)) return false; }
    Options& op = t.options;
    op.numFormat = r.u8(); op.angle = r.u8(); op.lengthUnit = r.u8();
    const std::uint8_t grid = r.u8();
    op.gridSpace = r.f64();
    const std::uint8_t axes = r.u8();
    if (!r.ok || op.numFormat > FmtNorm || op.angle > 1 || op.lengthUnit >= LengthUnitCount || grid > 2 || axes > 2 ||
        !(op.gridSpace >= 0.01 && op.gridSpace <= 1000))
        return false;
    op.grid = static_cast<Grid>(grid);
    op.axes = static_cast<Axes>(axes);
    t.view.cx = r.f64(); t.view.cy = r.f64(); t.view.scale = r.f64();
    if (!finiteValue(t.view.cx) || !finiteValue(t.view.cy) || !(t.view.scale >= MinScale && t.view.scale <= MaxScale)) return false;
    t.background = r.str(MaxBackground);
    t.fade = r.u8();
    t.nextId = r.u16(); t.nextFigure = r.u16(); t.nextLabel = r.u16(); t.nextSeq = r.u16();
    if (!r.ok || t.fade > 100 || t.nextId <= prev || t.nextFigure == 0 || t.nextSeq == 0) return false;
    recompute(t);
    return true;
}

// ---- formato 1 (v1.17.0) ------------------------------------------------------------------
struct OldObject { std::uint8_t kind; std::vector<double> xy; };
bool readOld(codec::Reader& r, std::vector<OldObject>& objs, View& view) {
    const std::size_t n = r.u16();
    if (!r.ok || n > 500) return false;
    for (std::size_t k = 0; k < n && r.ok; ++k) {
        OldObject o;
        o.kind = r.u8();
        const std::uint8_t count = r.u8();
        if (o.kind > 4) return false;
        o.xy.resize(2u * count);
        for (double& v : o.xy) v = r.f64();
        if (!r.ok || o.xy.empty()) return false;
        for (double v : o.xy) if (!std::isfinite(v) || std::abs(v) >= 1e100) return false;
        const std::size_t pts = o.xy.size() / 2;
        if (o.kind == 0 && pts != 1) return false;
        if ((o.kind == 1 || o.kind == 2 || o.kind == 3) && (pts != 2 || (o.xy[0] == o.xy[2] && o.xy[1] == o.xy[3]))) return false;
        if (o.kind == 4 && (pts < 3 || pts > 50)) return false;
        objs.push_back(std::move(o));
    }
    view.cx = r.f64(); view.cy = r.f64(); view.scale = r.f64();
    return r.ok && std::isfinite(view.cx) && std::isfinite(view.cy) && view.scale >= 0.01 && view.scale <= 10000;
}
// Migrazione: ogni coordinata diventa un punto libero, le figure restano legate ai loro punti.
bool migrate(const std::vector<OldObject>& objs, const View& view, State& t) {
    try {
        for (const auto& o : objs) {
            std::vector<Id> pts;
            for (std::size_t i = 0; i + 1 < o.xy.size(); i += 2) pts.push_back(addPoint(t, o.xy[i], o.xy[i + 1]));
            switch (o.kind) {
            case 1: addLine(t, Kind::Segment, pts[0], pts[1]); break;
            case 2: addLine(t, Kind::Line, pts[0], pts[1]); break;
            case 3: addCircle(t, pts[0], pts[1]); break;
            case 4: {
                // vertici consecutivi uguali (possibili nella v1.17.0) si uniscono
                std::vector<Id> v;
                for (std::size_t i = 0; i < pts.size(); ++i) {
                    const std::size_t j = i * 2;
                    if (!v.empty()) {
                        const Object* q = find(t, v.back());
                        if (q && q->vars[0] == o.xy[j] && q->vars[1] == o.xy[j + 1]) continue;
                    }
                    v.push_back(pts[i]);
                }
                if (v.size() > 1) { const Object* a = find(t, v.front()); const Object* b = find(t, v.back()); if (a->vars == b->vars) v.pop_back(); }
                if (v.size() >= 3) addPolygon(t, v, true);
                else if (v.size() == 2) addPolygon(t, v, false);
                break;
            }
            default: break;
            }
        }
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception&) {
        return false;
    }
    t.view = view;
    return true;
}
} // namespace

void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(FormatVersion);
    writeBody(w, s);
    w.str(s.file);
}
namespace detail {
bool decodeObjects(State& t, const std::uint8_t* data, std::size_t size, bool withFile) {
    codec::Reader r{data, size};
    const std::uint8_t version = r.u8();
    if (version == 1) {
        std::vector<OldObject> objs;
        View view;
        if (!readOld(r, objs, view)) return false;
        std::string file;
        if (withFile) file = r.str(64);
        if (!r.end()) return false;
        if (!migrate(objs, view, t)) return false;
        t.file = file;
        return true;
    }
    if (version != FormatVersion || !readBody(r, t)) return false;
    if (withFile) t.file = r.str(64);
    return r.end();
}
} // namespace detail
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    State t;
    if (!decodeObjects(t, data, size, true)) return false;
    s = std::move(t);
    return true;
}
std::vector<std::uint8_t> encodeFile(const State& s) {
    std::vector<std::uint8_t> out;
    codec::Writer w{out};
    w.u8(FormatVersion);
    writeBody(w, s);
    return out;
}
bool decodeFile(const std::uint8_t* data, std::size_t size, State& out) {
    State t;
    if (!decodeObjects(t, data, size, false)) return false;
    t.file = out.file;
    out = std::move(t);
    return true;
}
} // namespace fx::geo
