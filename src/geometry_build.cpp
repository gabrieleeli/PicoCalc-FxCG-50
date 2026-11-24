// Nucleo di Geometry (v1.19.0): Draw, Draw Spec, Construct, Transform,
// cancellazione, Clr Constraint, ordine di disegno e spostamenti. Ogni
// operazione ha la garanzia forte: con un errore lo stato resta quello di prima.
#include "geometry.hpp"
#include "expression.hpp"
#include "geometry_internal.hpp"
#include <algorithm>
#include <cctype>

namespace fx::geo {
using namespace detail;

namespace {
// Transazione: con un'eccezione toglie gli oggetti aggiunti e rimette i contatori.
struct Txn {
    State& s;
    std::size_t size;
    Id nextId;
    std::uint16_t fig, label, seq;
    bool done{false};
    explicit Txn(State& st) : s(st), size(st.objects.size()), nextId(st.nextId), fig(st.nextFigure), label(st.nextLabel), seq(st.nextSeq) {}
    ~Txn() {
        if (done) return;
        s.objects.resize(size);
        s.nextId = nextId; s.nextFigure = fig; s.nextLabel = label; s.nextSeq = seq;
    }
    void commit() noexcept { done = true; }
};
const Object& obj(const State& s, Id id) {
    const Object* o = find(s, id);
    if (!o) fail("Argument ERROR: oggetto inesistente");
    return *o;
}
Kind kindOf(const State& s, Id id) { return obj(s, id).kind; }
bool isPoint(const State& s, Id id) { const Object* o = find(s, id); return o && o->kind == Kind::Point; }
Object point(Def def, std::vector<Id> parents, std::vector<double> vars) {
    Object o;
    o.kind = Kind::Point;
    o.def = def;
    o.parents = std::move(parents);
    o.vars = std::move(vars);
    return o;
}
Id addLabeledPoint(State& s, Object o, std::uint16_t figure) {
    o.label = nextPointLabel(s);
    o.figure = figure;
    return append(s, std::move(o));
}
// La figura nuova di un comando di disegno; i punti senza figura ne entrano a far parte.
std::uint16_t newFigure(State& s, const std::vector<Id>& points) {
    const std::uint16_t f = s.nextFigure++;
    if (s.nextFigure == 0) s.nextFigure = 1;
    for (Id p : points) if (Object* o = find(s, p)) if (o->figure == 0) o->figure = f;
    return f;
}
void requireDistinct(const State& s, Id a, Id b) {
    if (a == b) fail("Math ERROR: punti coincidenti");
    const Shape* ga = shapeOf(s, a);
    const Shape* gb = shapeOf(s, b);
    if (ga && gb && norm(pointOf(*ga) - pointOf(*gb)) <= EpsCoincident * scaleOf({ga->a[0], ga->a[1], gb->a[0], gb->a[1]}))
        fail("Math ERROR: punti coincidenti");
}
Id addShape(State& s, Kind kind, Def def, std::vector<Id> parents, std::uint8_t flag, std::uint16_t figure) {
    Object o;
    o.kind = kind;
    o.def = def;
    o.parents = std::move(parents);
    o.flag = flag;
    o.figure = figure;
    return append(s, std::move(o));
}
} // namespace

// ---- Draw -----------------------------------------------------------------------------
Id addPoint(State& s, double x, double y) {
    Txn t(s);
    const Id id = addLabeledPoint(s, point(Def::Free, {}, {x, y}), 0);
    t.commit();
    return id;
}
Id addPointOn(State& s, Id curve, double x, double y) {
    Txn t(s);
    if (!isCurve(kindOf(s, curve))) fail("Argument ERROR: selezionare una linea o una curva");
    double tt, px, py;
    if (!project(s, curve, x, y, tt, px, py)) fail("Math ERROR: curva indefinita");
    const Id id = addLabeledPoint(s, point(Def::On, {curve}, {tt}), 0);
    t.commit();
    return id;
}
Id addLine(State& s, Kind kind, Id a, Id b) {
    if (!isLineLike(kind)) fail("Argument ERROR: tipo di linea");
    if (!isPoint(s, a) || !isPoint(s, b)) fail("Argument ERROR: due punti");
    requireDistinct(s, a, b);
    Txn t(s);
    const Id id = addShape(s, kind, Def::TwoPoints, {a, b}, 0, newFigure(s, {a, b}));
    t.commit();
    return id;
}
Id addCircle(State& s, Id center, Id through) {
    if (!isPoint(s, center) || !isPoint(s, through)) fail("Argument ERROR: centro e punto della circonferenza");
    requireDistinct(s, center, through);
    Txn t(s);
    const Id id = addShape(s, Kind::Circle, Def::CircleCP, {center, through}, 0, newFigure(s, {center, through}));
    t.commit();
    return id;
}
Id addArc(State& s, Id center, Id start, double ex, double ey) {
    if (!isPoint(s, center) || !isPoint(s, start)) fail("Argument ERROR: centro, inizio e fine dell'arco");
    requireDistinct(s, center, start);
    const Shape* c = shapeOf(s, center);
    if (!c) fail("Math ERROR: centro indefinito");
    Txn t(s);
    const std::uint16_t f = newFigure(s, {center, start});
    const Id end = addLabeledPoint(s, point(Def::ArcEnd, {center, start}, {angleOf(V2{ex, ey} - pointOf(*c))}), f);
    const Id id = addShape(s, Kind::Arc, Def::ArcCSE, {center, start, end}, 0, f);
    t.commit();
    return id;
}
Id addSemicircle(State& s, Id a, Id b) {
    if (!isPoint(s, a) || !isPoint(s, b)) fail("Argument ERROR: estremi del diametro");
    requireDistinct(s, a, b);
    Txn t(s);
    const Id id = addShape(s, Kind::Arc, Def::Semicircle, {a, b}, 0, newFigure(s, {a, b}));
    t.commit();
    return id;
}
Id addArc3(State& s, Id a, Id m, Id b) {
    if (!isPoint(s, a) || !isPoint(s, m) || !isPoint(s, b)) fail("Argument ERROR: tre punti");
    requireDistinct(s, a, m); requireDistinct(s, m, b); requireDistinct(s, a, b);
    Txn t(s);
    const Id id = addShape(s, Kind::Arc, Def::Arc3, {a, m, b}, 0, newFigure(s, {a, m, b}));
    if (!defined(s, id)) fail("Math ERROR: punti allineati");
    t.commit();
    return id;
}
Id addPolygon(State& s, const std::vector<Id>& v, bool closed) {
    const std::size_t n = v.size();
    if (n < (closed ? 3u : 2u)) fail(closed ? "Argument ERROR: almeno 3 vertici" : "Argument ERROR: almeno 2 vertici");
    if (n > MaxVertices) fail("Argument ERROR: al piu' 50 vertici");
    for (Id p : v) if (!isPoint(s, p)) fail("Argument ERROR: vertici");
    const std::size_t sides = closed ? n : n - 1;
    for (std::size_t i = 0; i < sides; ++i) requireDistinct(s, v[i], v[(i + 1) % n]);
    checkRoom(s, sides + (closed ? 1 : 0));
    Txn t(s);
    const std::uint16_t f = newFigure(s, v);
    Id last = NoId;
    for (std::size_t i = 0; i < sides; ++i) last = addShape(s, Kind::Segment, Def::TwoPoints, {v[i], v[(i + 1) % n]}, 0, f);
    if (closed) last = addShape(s, Kind::Polygon, Def::PolygonV, v, 0, f);
    t.commit();
    return last;
}
Id addSpec(State& s, Spec spec, Box box, Box fitBox, int n) {
    if (spec == Spec::Ngon && (n < MinNgon || n > MaxNgon)) fail("Argument ERROR: da 3 a 12 lati");
    double l = std::min(box.x0, box.x1), r = std::max(box.x0, box.x1), b = std::min(box.y0, box.y1), tp = std::max(box.y0, box.y1);
    const double fw = std::abs(fitBox.x1 - fitBox.x0), fh = std::abs(fitBox.y1 - fitBox.y0);
    // Riquadro troppo piccolo (D17): la figura piu' grande che sta nella finestra.
    if (r - l < 0.02 * fw || tp - b < 0.02 * fh || !(r > l) || !(tp > b)) {
        const double cx = (fitBox.x0 + fitBox.x1) / 2, cy = (fitBox.y0 + fitBox.y1) / 2;
        l = cx - 0.4 * fw; r = cx + 0.4 * fw; b = cy - 0.4 * fh; tp = cy + 0.4 * fh;
        if (!(r > l) || !(tp > b)) fail("Argument ERROR: riquadro vuoto");
    }
    const double w = r - l, h = tp - b;
    checkRoom(s, spec == Spec::Ngon ? 2u * static_cast<std::size_t>(n) + 2 : 9u);
    Txn t(s);
    const std::uint16_t f = newFigure(s, {});
    std::vector<Id> v;
    auto freePt = [&](double x, double y) { return addLabeledPoint(s, point(Def::Free, {}, {x, y}), f); };
    switch (spec) {
    case Spec::Triangle:
        v = {freePt(l, b), freePt(r, b), freePt(l + 0.25 * w, tp)};
        break;
    case Spec::Isosceles: {
        const Id a = freePt(l, b), bb = freePt(r, b);
        v = {a, bb, addLabeledPoint(s, point(Def::IsoApex, {a, bb}, {h}), f)};
        break;
    }
    case Spec::Rectangle: {
        const Id a = freePt(l, b), bb = freePt(r, b);
        Object c = point(Def::RectVertex, {a, bb}, {h});
        const Id cc = addLabeledPoint(s, std::move(c), f);
        Object d = point(Def::RectVertex, {a, bb, cc}, {});
        d.flag2 = 1;
        v = {a, bb, cc, addLabeledPoint(s, std::move(d), f)};
        break;
    }
    case Spec::Square: {
        const double side = std::min(w, h);
        const Id a = freePt(l, b), bb = freePt(l + side, b);
        Object c = point(Def::SquareVertex, {a, bb}, {});
        Object d = point(Def::SquareVertex, {a, bb}, {});
        d.flag2 = 1;
        const Id cc = addLabeledPoint(s, std::move(c), f);
        v = {a, bb, cc, addLabeledPoint(s, std::move(d), f)};
        break;
    }
    case Spec::Ngon: {
        const double rad = std::min(w, h) / 2;
        const Id o = freePt((l + r) / 2, (b + tp) / 2);
        const Id v1 = freePt((l + r) / 2, (b + tp) / 2 + rad);
        v.push_back(v1);
        for (int k = 1; k < n; ++k) {
            Object p = point(Def::NgonVertex, {o, v1}, {});
            p.flag = static_cast<std::uint8_t>(n);
            p.flag2 = static_cast<std::uint8_t>(k);
            v.push_back(addLabeledPoint(s, std::move(p), f));
        }
        break;
    }
    }
    for (std::size_t i = 0; i < v.size(); ++i) addShape(s, Kind::Segment, Def::TwoPoints, {v[i], v[(i + 1) % v.size()]}, 0, f);
    const Id poly = addShape(s, Kind::Polygon, Def::PolygonV, v, 0, f);
    t.commit();
    return poly;
}
Id addFunction(State& s, const std::string& expression) {
    if (expression.empty()) fail("Syntax ERROR: funzione vuota");
    if (expression.size() > MaxFormula) fail("Argument ERROR: funzione troppo lunga");
    (void)parseExpression(expression);                 // Syntax ERROR con la posizione
    Txn t(s);
    Object o;
    o.kind = Kind::Function;
    o.def = Def::FunctionX;
    o.text = expression;
    o.showLabel = false;
    o.figure = newFigure(s, {});
    const Id id = append(s, std::move(o));
    t.commit();
    return id;
}
void setFunction(State& s, Id f, const std::string& expression) {
    Object* o = find(s, f);
    if (!o || o->kind != Kind::Function) fail("Argument ERROR: selezionare una funzione");
    if (expression.empty() || expression.size() > MaxFormula) fail("Argument ERROR: funzione");
    (void)parseExpression(expression);
    o->text = expression;
    recompute(s, {f});
}
Id addText(State& s, double x, double y, const std::string& text) {
    if (text.empty()) fail("Argument ERROR: testo vuoto");
    if (text.size() > MaxText) fail("Argument ERROR: al piu' 31 caratteri");
    Txn t(s);
    Object o;
    o.kind = Kind::Text;
    o.def = Def::TextAt;
    o.vars = {x, y};
    o.text = text;
    const Id id = append(s, std::move(o));
    t.commit();
    return id;
}
Id addMeasureText(State& s, const MSpec& m, double x, double y) {
    if (m.type >= MType::Label || m.ops.empty() || m.ops.size() > 3) fail("Argument ERROR: misura da incollare");
    std::vector<Id> parents = m.ops;
    Txn t(s);
    Object o;
    o.kind = Kind::Measure;
    o.def = Def::MeasureAt;
    o.flag = static_cast<std::uint8_t>(m.type);
    o.parents = std::move(parents);
    o.vars = {x, y};
    o.label = mtypeName(m.type);
    if (o.label.size() > MaxLabel) o.label.resize(MaxLabel);
    o.seq = s.nextSeq++;
    // i genitori devono avere un id minore: l'oggetto e' sempre l'ultimo
    const Id id = append(s, std::move(o));
    t.commit();
    return id;
}
namespace {
// "@3", "@3X", "@3Y" nel testo dell'espressione -> le misure incollate con quel numero.
std::vector<Id> expressionParents(const State& s, const std::string& f) {
    std::vector<Id> out;
    for (std::size_t i = 0; i < f.size(); ++i) {
        if (f[i] != '@') continue;
        std::size_t j = i + 1;
        unsigned n = 0;
        while (j < f.size() && std::isdigit(static_cast<unsigned char>(f[j])) && n < 100000) n = n * 10 + static_cast<unsigned>(f[j++] - '0');
        if (j == i + 1) fail("Syntax ERROR: @ senza numero");
        Id found = NoId;
        for (const auto& o : s.objects) if (o.kind == Kind::Measure && o.seq == n) found = o.id;
        if (found == NoId) fail("Argument ERROR: misura @" + std::to_string(n) + " inesistente");
        if (std::find(out.begin(), out.end(), found) == out.end()) out.push_back(found);
    }
    std::sort(out.begin(), out.end());
    if (out.size() > MaxColumns) fail("Argument ERROR: troppe misure");
    return out;
}
} // namespace
Id addExpression(State& s, double x, double y, const std::string& formula) {
    if (formula.empty() || formula.size() > MaxFormula) fail("Argument ERROR: espressione");
    Txn t(s);
    Object o;
    o.kind = Kind::Expression;
    o.def = Def::ExpressionAt;
    o.parents = expressionParents(s, formula);
    o.vars = {x, y};
    o.text = formula;
    o.label = "Expr";
    const Id id = append(s, std::move(o));
    t.commit();
    return id;
}

// ---- Construct ------------------------------------------------------------------------
std::vector<Id> construct(State& s, Construct c, const std::vector<Id>& sel, double x, double y) {
    for (Id id : sel) (void)obj(s, id);
    auto count = [&](bool (*pred)(Kind)) { std::size_t n = 0; for (Id id : sel) n += pred(kindOf(s, id)); return n; };
    auto pick = [&](bool (*pred)(Kind), std::size_t k = 0) { for (Id id : sel) if (pred(kindOf(s, id)) && k-- == 0) return id; return NoId; };
    auto isPt = [](Kind k) { return k == Kind::Point; };
    auto isLn = [](Kind k) { return isLineLike(k); };
    auto isSeg = [](Kind k) { return k == Kind::Segment || k == Kind::Vector; };
    auto isCut = [](Kind k) { return isLineLike(k) || k == Kind::Circle || k == Kind::Arc; };
    auto isTan = [](Kind k) { return k == Kind::Circle || k == Kind::Arc || k == Kind::Function; };
    Txn t(s);
    std::vector<Id> out;
    switch (c) {
    case Construct::PerpBisector: case Construct::Midpoint: {
        std::vector<Id> parents;
        if (sel.size() == 1 && count(isSeg) == 1) parents = {sel[0]};
        else if (sel.size() == 2 && count(isPt) == 2) { requireDistinct(s, sel[0], sel[1]); parents = {std::min(sel[0], sel[1]), std::max(sel[0], sel[1])}; }
        else fail("Argument ERROR: selezionare un segmento (o un lato) oppure due punti");
        if (c == Construct::Midpoint) out.push_back(addLabeledPoint(s, point(Def::Midpoint, parents, {}), 0));
        else out.push_back(addShape(s, Kind::Line, Def::PerpBisector, parents, 0, 0));
        break;
    }
    case Construct::Perpendicular: case Construct::Parallel: {
        if (sel.size() != 2 || count(isLn) != 1 || count(isPt) != 1) fail("Argument ERROR: selezionare una linea e un punto");
        out.push_back(addShape(s, Kind::Line, c == Construct::Parallel ? Def::Parallel : Def::Perpendicular, {pick(isLn), pick(isPt)}, 0, 0));
        break;
    }
    case Construct::Intersection: {
        if (sel.size() != 2 || count(isCut) != 2) fail("Argument ERROR: selezionare due linee, circonferenze o archi");
        const Id a = std::min(sel[0], sel[1]), b = std::max(sel[0], sel[1]);
        const bool twoLines = isLineLike(kindOf(s, a)) && isLineLike(kindOf(s, b));
        bool any = false;
        for (std::uint8_t k = 0; k < (twoLines ? 1 : 2); ++k) {
            Object p = point(Def::Intersection, {a, b}, {});
            p.flag = k;
            const Id id = addLabeledPoint(s, std::move(p), 0);
            out.push_back(id);
            any = any || defined(s, id);
        }
        if (!any) fail("Math ERROR: nessuna intersezione");
        break;
    }
    case Construct::AngleBisector: {
        if (sel.size() != 2 || count(isLn) != 2) fail("Argument ERROR: selezionare due linee");
        for (std::uint8_t k = 0; k < 2; ++k) out.push_back(addShape(s, Kind::Line, Def::AngleBisector, {sel[0] < sel[1] ? sel[0] : sel[1], sel[0] < sel[1] ? sel[1] : sel[0]}, k, 0));
        if (!defined(s, out[0])) fail("Math ERROR: le linee non si incontrano");
        break;
    }
    case Construct::Tangent: {
        const Id cu = pick(isTan);
        if (cu == NoId || sel.size() > 2 || count(isTan) != 1 || (sel.size() == 2 && count(isPt) != 1)) fail("Argument ERROR: selezionare una circonferenza, un arco o una funzione");
        Id p = pick(isPt);
        if (p != NoId) {
            // il punto scelto deve stare sulla curva: se e' gia' legato a lei lo si usa
            const Object& po = obj(s, p);
            if (!(po.def == Def::On && po.parents[0] == cu)) {
                const Shape* g = shapeOf(s, p);
                if (!g) fail("Math ERROR: punto indefinito");
                x = g->a[0]; y = g->a[1];
                p = NoId;
            }
        }
        if (p == NoId) { p = addPointOn(s, cu, x, y); out.push_back(p); }
        out.push_back(addShape(s, Kind::Line, Def::Tangent, {cu, p}, 0, 0));
        if (!defined(s, out.back())) fail("Math ERROR: tangente non definita in questo punto");
        break;
    }
    case Construct::AttachedAngle: {
        if (sel.size() != 2 || count(isLn) != 2) fail("Argument ERROR: selezionare due lati");
        Object m;
        m.kind = Kind::Measure;
        m.def = Def::MeasureAt;
        m.flag = static_cast<std::uint8_t>(MType::Angle);
        m.flag2 = 1;
        m.parents = sel;
        m.vars = {x, y};
        m.label = mtypeName(MType::Angle);
        m.seq = s.nextSeq++;
        out.push_back(append(s, std::move(m)));
        break;
    }
    }
    t.commit();
    return out;
}

// ---- Transform ------------------------------------------------------------------------
namespace {
bool pointBased(Def d) { return d == Def::TwoPoints || d == Def::CircleCP || d == Def::ArcCSE || d == Def::Semicircle || d == Def::Arc3 || d == Def::PolygonV; }
} // namespace
std::vector<Id> transform(State& s, TType tt, const std::vector<Id>& sel, Id ref, double p0, double p1) {
    for (Id id : sel) (void)obj(s, id);
    Object tr;
    tr.kind = Kind::Transform;
    tr.showLabel = false;
    switch (tt) {
    case TType::Reflection:
        if (ref == NoId || !isLineLike(kindOf(s, ref))) fail("Argument ERROR: selezionare l'asse (segmento, retta, semiretta o lato)");
        tr.def = Def::Reflect; tr.parents = {ref}; break;
    case TType::Translation:
        if (!std::isfinite(p0) || !std::isfinite(p1)) fail("Math ERROR: valori della traslazione");
        tr.def = Def::Translate; tr.vars = {p0, p1}; break;
    case TType::TransVec:
        if (ref == NoId || kindOf(s, ref) != Kind::Vector) fail("Argument ERROR: selezionare un vettore");
        tr.def = Def::TranslateVec; tr.parents = {ref}; break;
    case TType::Rotation:
        if (ref == NoId || kindOf(s, ref) != Kind::Point) fail("Argument ERROR: selezionare il centro di rotazione");
        if (!std::isfinite(p0)) fail("Math ERROR: angolo");
        tr.def = Def::Rotate; tr.parents = {ref}; tr.vars = {toRadians(s.options, p0)}; break;
    case TType::Dilation:
        if (ref == NoId || kindOf(s, ref) != Kind::Point) fail("Argument ERROR: selezionare il centro della dilatazione");
        if (!(std::abs(p0) >= MinDilation && std::abs(p0) <= MaxDilation)) fail("Argument ERROR: fattore con 0.1 <= |k| <= 10");
        tr.def = Def::Dilate; tr.parents = {ref}; tr.vars = {p0}; break;
    case TType::Symmetry:
        if (ref == NoId || kindOf(s, ref) != Kind::Point) fail("Argument ERROR: selezionare il centro");
        tr.def = Def::Symmetry; tr.parents = {ref}; break;
    }
    // Oggetti da trasformare: la selezione, le figure dei poligoni scelti e i vertici delle figure.
    std::vector<char> mark(s.objects.size(), 0);
    bool onlyFunctions = !sel.empty();
    for (Id id : sel) {
        const Object& o = obj(s, id);
        if (o.kind == Kind::Function) continue;
        if (o.kind == Kind::Text || o.kind == Kind::Measure || o.kind == Kind::Expression || o.kind == Kind::Transform) continue;
        onlyFunctions = false;
        mark[at(s, id)] = 1;
        if (o.kind == Kind::Polygon && o.figure != 0)
            for (std::size_t i = 0; i < s.objects.size(); ++i)
                if (s.objects[i].figure == o.figure && (isLineLike(s.objects[i].kind) || s.objects[i].kind == Kind::Point)) mark[i] = 1;
    }
    if (onlyFunctions && !sel.empty()) fail("Argument ERROR: f(x) non trasformabile");
    for (std::size_t i = 0; i < s.objects.size(); ++i) {
        if (!mark[i] || !pointBased(s.objects[i].def)) continue;
        for (Id p : s.objects[i].parents) mark[at(s, p)] = 1;
    }
    if (std::find(mark.begin(), mark.end(), 1) == mark.end()) fail("Argument ERROR: selezionare le figure da trasformare");
    std::size_t need = 1;
    for (char m : mark) need += m != 0;
    checkRoom(s, need);
    Txn t(s);
    const Id tid = append(s, std::move(tr));
    if (!defined(s, tid)) fail("Math ERROR: trasformazione indefinita");
    const std::uint16_t f = newFigure(s, {});
    std::vector<std::pair<Id, Id>> img;
    auto imageOf = [&](Id id) { for (auto& p : img) if (p.first == id) return p.second; return NoId; };
    std::vector<Id> out;
    const std::size_t n = mark.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (!mark[i]) continue;
        const Object src = s.objects[i];                 // copia: append puo' riallocare
        Object o;
        o.kind = src.kind;
        o.style = src.style;
        o.figure = f;
        if (!src.label.empty()) { o.label = src.label + "'"; if (o.label.size() > MaxLabel) o.label.resize(MaxLabel); }
        o.showLabel = src.showLabel;
        if (src.kind == Kind::Point) {
            o.def = Def::Image;
            o.parents = {src.id, tid};
        } else if (pointBased(src.def)) {
            bool all = true;
            for (Id p : src.parents) all = all && imageOf(p) != NoId;
            if (!all) continue;
            o.def = src.def;
            for (Id p : src.parents) o.parents.push_back(imageOf(p));
            o.flag = src.flag;
            if (tt == TType::Reflection && (src.def == Def::ArcCSE || src.def == Def::Semicircle)) o.flag ^= 1;
        } else if (isLineLike(src.kind) || src.kind == Kind::Circle || src.kind == Kind::Arc) {
            o.def = Def::ImageCurve;
            o.parents = {src.id, tid};
        } else continue;
        const Id id = append(s, std::move(o));
        img.emplace_back(src.id, id);
        out.push_back(id);
    }
    t.commit();
    return out;
}

// ---- Edit -----------------------------------------------------------------------------
namespace detail {
void removeReferences(State& s, const std::vector<char>& gone) {
    (void)gone;
    auto missing = [&](Id id) { return indexOf(s, id) < 0; };
    auto specGone = [&](const MSpec& m) { for (Id id : m.ops) if (missing(id)) return true; return false; };
    s.locks.erase(std::remove_if(s.locks.begin(), s.locks.end(), [&](const Lock& l) { return specGone(l.m); }), s.locks.end());
    s.animations.erase(std::remove_if(s.animations.begin(), s.animations.end(), [&](const Animation& a) { return missing(a.point) || missing(a.path); }), s.animations.end());
    s.traced.erase(std::remove_if(s.traced.begin(), s.traced.end(), missing), s.traced.end());
    const std::size_t cols = s.columns.size();
    std::vector<char> keep(cols, 1);
    bool changed = false;
    for (std::size_t c = 0; c < cols; ++c) if (specGone(s.columns[c].m)) { keep[c] = 0; changed = true; }
    if (changed) {
        std::vector<TableColumn> nc;
        std::vector<double> nt;
        for (std::size_t c = 0; c < cols; ++c) if (keep[c]) nc.push_back(s.columns[c]);
        for (std::size_t r = 0; r < s.tableRows; ++r)
            for (std::size_t c = 0; c < cols; ++c) if (keep[c]) nt.push_back(s.table[r * cols + c]);
        s.columns = std::move(nc);
        s.table = std::move(nt);
        if (s.columns.empty()) { s.table.clear(); s.tableRows = 0; }
    }
}
} // namespace detail
std::size_t remove(State& s, const std::vector<Id>& ids) {
    const auto gone = descendants(s, ids);
    if (gone.empty()) return 0;
    std::vector<char> mark(s.objects.size(), 0);
    for (Id id : gone) mark[at(s, id)] = 1;
    // trasformazioni rimaste senza immagini
    for (std::size_t i = 0; i < s.objects.size(); ++i) {
        if (mark[i] || s.objects[i].kind != Kind::Transform) continue;
        bool used = false;
        for (std::size_t j = i + 1; j < s.objects.size() && !used; ++j)
            if (!mark[j]) for (Id p : s.objects[j].parents) used = used || p == s.objects[i].id;
        if (!used) mark[i] = 1;
    }
    std::size_t removed = 0;
    std::vector<Object> keep;
    keep.reserve(s.objects.size());
    for (std::size_t i = 0; i < s.objects.size(); ++i) {
        if (mark[i]) ++removed; else keep.push_back(std::move(s.objects[i]));
    }
    s.objects = std::move(keep);
    removeReferences(s, mark);
    // le espressioni citano le misure come genitori: sono gia' tra i discendenti
    return removed;
}
std::vector<Id> figureOf(const State& s, const std::vector<Id>& ids) {
    std::vector<Id> out;
    for (Id id : ids) {
        const Object* o = find(s, id);
        if (!o) continue;
        if (o->figure == 0) { out.push_back(id); continue; }
        for (const auto& x : s.objects) if (x.figure == o->figure && x.kind != Kind::Transform) out.push_back(x.id);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}
void clearConstraints(State& s) {
    s.locks.clear();
    for (auto& o : s.objects) {
        if (o.kind != Kind::Point || !o.g.ok) continue;
        if (o.def == Def::RectVertex || o.def == Def::SquareVertex || o.def == Def::IsoApex || o.def == Def::NgonVertex) {
            o.def = Def::Free;
            o.parents.clear();
            o.vars = {o.g.a[0], o.g.a[1]};
            o.flag = o.flag2 = 0;
        }
    }
    recompute(s);
}
void toFront(State& s, const std::vector<Id>& ids) {
    std::int16_t top = 0;
    for (const auto& o : s.objects) top = std::max(top, o.z);
    if (top < 32000) ++top;
    for (Id id : ids) if (Object* o = find(s, id)) o->z = top;
}
void toBack(State& s, const std::vector<Id>& ids) {
    std::int16_t low = 0;
    for (const auto& o : s.objects) low = std::min(low, o.z);
    if (low > -32000) --low;
    for (Id id : ids) if (Object* o = find(s, id)) o->z = low;
}
void textsToFront(State& s) {
    std::vector<Id> t;
    for (const auto& o : s.objects) if (o.kind == Kind::Text || o.kind == Kind::Measure || o.kind == Kind::Expression) t.push_back(o.id);
    toFront(s, t);
}

// ---- spostamenti -----------------------------------------------------------------------
namespace {
bool coordinateLocked(const State& s, Id p) {
    for (const auto& l : s.locks) if (l.m.type == MType::Coordinates && l.m.ops.size() == 1 && l.m.ops[0] == p) return true;
    return false;
}
bool freePoint(const State& s, Id p) { const Object* o = find(s, p); return o && o->kind == Kind::Point && o->def == Def::Free && !coordinateLocked(s, p); }
std::vector<std::vector<double>> snapshotVars(const State& s) {
    std::vector<std::vector<double>> v;
    v.reserve(s.objects.size());
    for (const auto& o : s.objects) v.push_back(o.vars);
    return v;
}
void restoreVars(State& s, const std::vector<std::vector<double>>& v) {
    for (std::size_t i = 0; i < s.objects.size() && i < v.size(); ++i) s.objects[i].vars = v[i];
    recompute(s);
}
bool afterMove(State& s, const std::vector<Id>& moved, const std::vector<std::vector<double>>& before) {
    recompute(s, moved);
    if (s.locks.empty() || solve(s, moved)) return true;
    restoreVars(s, before);
    return false;
}
} // namespace
bool movable(const State& s, Id p) noexcept {
    const Object* o = find(s, p);
    if (!o) return false;
    if (o->kind == Kind::Text || o->kind == Kind::Measure || o->kind == Kind::Expression) return true;
    if (o->kind != Kind::Point) return false;
    switch (o->def) {
    case Def::Free: return !coordinateLocked(s, p);
    case Def::On: case Def::IsoApex: case Def::ArcEnd: return true;
    case Def::RectVertex: return true;
    case Def::SquareVertex: return freePoint(s, o->parents[1]);
    case Def::NgonVertex: return freePoint(s, o->parents[1]);
    default: return false;
    }
}
bool dragPoint(State& s, Id p, double x, double y) {
    if (!movable(s, p) || !std::isfinite(x) || !std::isfinite(y)) return false;
    Object& o = s.objects[at(s, p)];
    const auto before = snapshotVars(s);
    const V2 q{x, y};
    std::vector<Id> changed{p};
    if (o.kind != Kind::Point) { o.vars = {x, y}; recompute(s, changed); return true; }   // testi
    auto pt = [&](std::size_t i) { const Shape* g = shapeOf(s, o.parents[i]); return g ? pointOf(*g) : V2{NAN, NAN}; };
    switch (o.def) {
    case Def::Free: o.vars = {x, y}; break;
    case Def::On: {
        double t, px, py;
        if (!project(s, o.parents[0], x, y, t, px, py)) return false;
        o.vars[0] = t;
        break;
    }
    case Def::IsoApex: {
        const V2 a = pt(0), b = pt(1), d = b - a;
        if (!(norm(d) > 0)) return false;
        o.vars[0] = dot(q - 0.5 * (a + b), rot90(d)) / norm(d);
        break;
    }
    case Def::ArcEnd: o.vars[0] = angleOf(q - pt(0)); break;
    case Def::RectVertex: {
        const V2 a = pt(0), b = pt(1), d = b - a;
        if (!(norm(d) > 0)) return false;
        const double w = dot(q - (o.flag2 == 0 ? b : a), rot90(d)) / norm(d);
        if (o.flag2 == 0) o.vars[0] = w;
        else { Object& c = s.objects[at(s, o.parents[2])]; c.vars[0] = w; changed = {c.id}; }
        break;
    }
    case Def::SquareVertex: {
        const V2 a = pt(0);
        const V2 rel = q - a;
        // C = B + R90(B - A) -> B - A = (I + R90)^-1 (C - A); D = A + R90(B - A) -> B - A = R(-90)(D - A)
        const V2 ba = o.flag2 == 0 ? V2{0.5 * (rel.x + rel.y), 0.5 * (rel.y - rel.x)} : V2{rel.y, -rel.x};
        Object& b = s.objects[at(s, o.parents[1])];
        b.vars = {a.x + ba.x, a.y + ba.y};
        changed = {b.id};
        break;
    }
    case Def::NgonVertex: {
        const V2 c = pt(0);
        const V2 v1 = c + rotate(q - c, -TwoPi * o.flag2 / o.flag);
        Object& b = s.objects[at(s, o.parents[1])];
        b.vars = {v1.x, v1.y};
        changed = {b.id};
        break;
    }
    default: return false;
    }
    return afterMove(s, changed, before);
}
std::vector<Id> freePoints(const State& s, const std::vector<Id>& sel) {
    std::vector<char> mark(s.objects.size(), 0);
    std::vector<Id> stack(sel.begin(), sel.end());
    std::vector<Id> out;
    while (!stack.empty()) {
        const Id id = stack.back();
        stack.pop_back();
        const int i = indexOf(s, id);
        if (i < 0 || mark[static_cast<std::size_t>(i)]) continue;
        mark[static_cast<std::size_t>(i)] = 1;
        const Object& o = s.objects[static_cast<std::size_t>(i)];
        if (o.kind == Kind::Point) {
            if (o.def == Def::Free) { if (!coordinateLocked(s, id)) out.push_back(id); }
            else if (o.def == Def::RectVertex || o.def == Def::SquareVertex || o.def == Def::IsoApex || o.def == Def::NgonVertex || o.def == Def::ArcEnd)
                for (Id p : o.parents) stack.push_back(p);
        } else if (pointBased(o.def)) {
            for (Id p : o.parents) stack.push_back(p);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}
bool translate(State& s, const std::vector<Id>& sel, double dx, double dy) {
    if (!std::isfinite(dx) || !std::isfinite(dy)) return false;
    // un solo vertice vincolato: lungo il suo grado di liberta'
    if (sel.size() == 1) {
        const Object* o = find(s, sel[0]);
        if (o && o->kind == Kind::Point && o->def != Def::Free) {
            const Shape* g = shapeOf(s, sel[0]);
            return g && dragPoint(s, sel[0], g->a[0] + dx, g->a[1] + dy);
        }
    }
    const auto before = snapshotVars(s);
    std::vector<Id> moved = freePoints(s, sel);
    for (Id id : sel) {
        Object* o = find(s, id);
        if (o && (o->kind == Kind::Text || o->kind == Kind::Measure || o->kind == Kind::Expression)) { o->vars[0] += dx; o->vars[1] += dy; moved.push_back(id); }
    }
    if (moved.empty()) return false;
    for (Id id : moved) {
        Object& o = s.objects[at(s, id)];
        if (o.kind == Kind::Point) { o.vars[0] += dx; o.vars[1] += dy; }
    }
    return afterMove(s, moved, before);
}
} // namespace fx::geo
