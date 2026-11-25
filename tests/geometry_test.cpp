// v1.19.0 (solo Pimoroni Pico Plus 2 W): nucleo puro di GEOMETRY alla parita'
// del capitolo 14. Un caso per voce della tabella di
// docs/reports/ANALISI_FONTI_v1.19.0.md (§3, colonna GT) con l'id nel nome,
// piu' i test di proprieta' del §8.2 su costruzioni casuali con seme fisso e la
// migrazione dei file della v1.17.0 (fixture prodotta da quel codice).
#include "expression.hpp"
#include "geometry.hpp"
#include "step_task.hpp"
#include "test_util.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace {
#include "fixtures/geometry_v1_17_0.inc"
}

using namespace fx;
using namespace fx::geo;

namespace {
#define T(id, cond) ::testutil::report(static_cast<bool>(cond), id, __FILE__, __LINE__, #cond)
#define TN(id, a, b, tol) do { const double va_ = (a), vb_ = (b); \
    ::testutil::report(std::abs(va_ - vb_) <= (tol) * std::max(1.0, std::abs(vb_)), id, __FILE__, __LINE__, \
    std::string(#a " ~ " #b " (") + std::to_string(va_) + " vs " + std::to_string(vb_) + ")"); } while (0)
constexpr double Pi = 3.14159265358979323846;
struct P2 { double x, y; };
P2 pt(const State& s, Id id) { const Object* o = find(s, id); return o && o->g.ok ? P2{o->g.a[0], o->g.a[1]} : P2{NAN, NAN}; }
double dist(P2 a, P2 b) { return std::hypot(a.x - b.x, a.y - b.y); }
double val(const State& s, MType t, std::vector<Id> ops, int i = 0) { const Measurement m = measure(s, MSpec{t, std::move(ops)}); return m.ok ? m.v[i] : NAN; }
bool throws(const std::function<void()>& f, const std::string& what = {}) {
    try { f(); } catch (const std::exception& e) { return what.empty() || std::string(e.what()).find(what) != std::string::npos; }
    return false;
}
bool roundTrip(const State& s) {
    std::vector<std::uint8_t> b;
    encode(s, b);
    State back;
    if (!decode(back, b.data(), b.size()) || !(back == s)) return false;
    const auto f = encodeFile(s);
    State fs;
    fs.file = s.file;
    return decodeFile(f.data(), f.size(), fs) && fs == s;
}
Id kindAt(const State& s, Kind k, std::size_t n = 0) { for (const auto& o : s.objects) if (o.kind == k && n-- == 0) return o.id; return NoId; }
std::vector<Id> ofKind(const State& s, Kind k) { std::vector<Id> v; for (const auto& o : s.objects) if (o.kind == k) v.push_back(o.id); return v; }
struct Rng { std::uint64_t x{88172645463325252ull}; double next() { x ^= x << 13; x ^= x >> 7; x ^= x << 17; return static_cast<double>(x >> 11) / 9007199254740992.0; } double in(double a, double b) { return a + (b - a) * next(); } };
double polyArea(const State& s, Id poly) { return val(s, MType::Area, {poly}); }
Box fitBox() { return Box{-6.6, -5.3, 6.6, 5.3}; }

// ---------------------------------------------------------------- Draw e Draw Spec
void draw() {
    {   // D01 punto libero e punto legato
        State s;
        const Id a = addPoint(s, 1, 2);
        T("D01 punto libero", pt(s, a).x == 1 && pt(s, a).y == 2 && find(s, a)->label == "A");
        const Id b = addPoint(s, 5, 2);
        const Id seg = addLine(s, Kind::Segment, a, b);
        const Id p = addPointOn(s, seg, 3, 7);
        T("D01 punto sul segmento", std::abs(pt(s, p).x - 3) < 1e-12 && std::abs(pt(s, p).y - 2) < 1e-12 && find(s, p)->def == Def::On);
        dragPoint(s, b, 9, 2);
        T("D01 il punto segue il segmento", std::abs(pt(s, p).x - 5) < 1e-12);
        T("D01 si sposta solo lungo l'oggetto", dragPoint(s, p, 100, 50) && std::abs(pt(s, p).x - 9) < 1e-12 && std::abs(pt(s, p).y - 2) < 1e-12);
        const Id c = addPoint(s, 0, 0), q = addPoint(s, 2, 0);
        const Id circ = addCircle(s, c, q);
        const Id pc = addPointOn(s, circ, 0, 5);
        T("D01 punto sulla circonferenza", std::abs(pt(s, pc).y - 2) < 1e-12 && std::abs(pt(s, pc).x) < 1e-12);
        dragPoint(s, q, 4, 0);
        T("D01 segue il raggio", std::abs(pt(s, pc).y - 4) < 1e-12);
        const Id f = addFunction(s, "x^2");
        const Id pf = addPointOn(s, f, 3, 0);
        T("D01 punto sulla funzione", pt(s, pf).x == 3 && std::abs(pt(s, pf).y - 9) < 1e-12);
        T("D01 nessun oggetto: errore", throws([&] { addPointOn(s, a, 0, 0); }));
    }
    {   // D02-D05 linee
        State s;
        const Id a = addPoint(s, 0, 0), b = addPoint(s, 3, 4);
        const Id seg = addLine(s, Kind::Segment, a, b), line = addLine(s, Kind::Line, a, b), ray = addLine(s, Kind::Ray, a, b), vec = addLine(s, Kind::Vector, a, b);
        TN("D02 segmento", val(s, MType::Distance, {seg}), 5, 1e-12);
        T("D02 punti coincidenti", throws([&] { addLine(s, Kind::Segment, a, a); }, "coincidenti"));
        T("D03 retta infinita oltre i punti", hitTest(s, 30, 40, 0.01) == line || hitCurve(s, 30, 40, 0.01) != NoId);
        T("D04 semiretta: oltre B si', prima di A no", hitCurve(s, -3, -4, 0.01) == line && hitCurve(s, 6, 8, 0.01) != NoId);
        TN("D05 vettore: componenti", val(s, MType::Components, {vec}, 1), 4, 1e-12);
        T("D05 tipi", find(s, ray)->kind == Kind::Ray && find(s, vec)->kind == Kind::Vector);
    }
    {   // D06-D09 circonferenze e archi
        State s;
        const Id c = addPoint(s, 1, 1), p = addPoint(s, 4, 5);
        const Id circ = addCircle(s, c, p);
        TN("D06 circonferenza", val(s, MType::Radius, {circ}), 5, 1e-12);
        const Id o = addPoint(s, 0, 0), st = addPoint(s, 2, 0);
        const Id arc = addArc(s, o, st, 0, 7);
        TN("D07 arco antiorario di 90 gradi", val(s, MType::Circumference, {arc}), Pi, 1e-12);
        const Id end = find(s, arc)->parents[2];
        T("D07 la fine sta sull'arco", std::abs(pt(s, end).y - 2) < 1e-12 && find(s, end)->def == Def::ArcEnd);
        const Id d1 = addPoint(s, -1, -3), d2 = addPoint(s, 1, -3);
        const Id semi = addSemicircle(s, d1, d2);
        TN("D08 semicirconferenza", val(s, MType::Circumference, {semi}), Pi, 1e-12);
        T("D08 sotto il diametro (antiorario da A a B)", hitCurve(s, 0, -4, 0.01) == semi && hitCurve(s, 0, -2, 0.01) != semi);
        const Id a = addPoint(s, 10, 0), m = addPoint(s, 10 + std::sqrt(2.0), std::sqrt(2.0)), b = addPoint(s, 10, 2);
        (void)m;
        const Id a3 = addArc3(s, addPoint(s, 12, 0), addPoint(s, 10, 2), addPoint(s, 8, 0));
        TN("D09 arco per tre punti", val(s, MType::Radius, {a3}), 2, 1e-12);
        TN("D09 meta' circonferenza", val(s, MType::Circumference, {a3}), 2 * Pi, 1e-12);
        T("D09 allineati: errore", throws([&] { addArc3(s, a, addPoint(s, 11, 0), addPoint(s, 12, 0)); }, "allineati"));
        (void)b;
    }
    {   // D10-D15 Draw Spec
        State s;
        const Id tri = addSpec(s, Spec::Triangle, Box{0, 0, 4, 3}, fitBox());
        TN("D10 triangolo nel riquadro", polyArea(s, tri), 6, 1e-12);
        T("D10 tre lati", ofKind(s, Kind::Segment).size() == 3 && find(s, tri)->parents.size() == 3);
        State s2;
        const Id iso = addSpec(s2, Spec::Isosceles, Box{0, 0, 4, 3}, fitBox());
        const auto& v = find(s2, iso)->parents;
        TN("D11 isoscele", dist(pt(s2, v[2]), pt(s2, v[0])), dist(pt(s2, v[2]), pt(s2, v[1])), 1e-12);
        dragPoint(s2, v[1], 7, 2);
        TN("D11 resta isoscele spostando un vertice", dist(pt(s2, v[2]), pt(s2, v[0])), dist(pt(s2, v[2]), pt(s2, v[1])), 1e-12);
        dragPoint(s2, v[2], 1, 9);
        TN("D11 resta isoscele spostando l'apice", dist(pt(s2, v[2]), pt(s2, v[0])), dist(pt(s2, v[2]), pt(s2, v[1])), 1e-12);
        State s3;
        const Id rect = addSpec(s3, Spec::Rectangle, Box{0, 0, 4, 2}, fitBox());
        const auto rv = find(s3, rect)->parents;
        TN("D12 rettangolo", polyArea(s3, rect), 8, 1e-12);
        dragPoint(s3, rv[1], 3, 3);
        auto rightAngle = [&](const State& st, Id a, Id b, Id c) {
            const P2 pa = pt(st, a), pb = pt(st, b), pc = pt(st, c);
            return std::abs((pa.x - pb.x) * (pc.x - pb.x) + (pa.y - pb.y) * (pc.y - pb.y)) < 1e-9;
        };
        T("D12 resta rettangolo", rightAngle(s3, rv[0], rv[1], rv[2]) && rightAngle(s3, rv[1], rv[2], rv[3]) && rightAngle(s3, rv[2], rv[3], rv[0]));
        dragPoint(s3, rv[3], -5, 5);
        T("D12 resta rettangolo spostando D", rightAngle(s3, rv[0], rv[1], rv[2]) && rightAngle(s3, rv[2], rv[3], rv[0]));
        State s4;
        const Id sq = addSpec(s4, Spec::Square, Box{0, 0, 4, 3}, fitBox());
        TN("D13 quadrato con il lato minore", polyArea(s4, sq), 9, 1e-12);
        const auto qv = find(s4, sq)->parents;
        dragPoint(s4, qv[2], 5, 5);
        TN("D13 resta quadrato", dist(pt(s4, qv[0]), pt(s4, qv[1])), dist(pt(s4, qv[1]), pt(s4, qv[2])), 1e-9);
        TN("D13 il vertice va dove indicato", dist(pt(s4, qv[2]), P2{5, 5}), 0, 1e-9);
        State s5;
        const Id a = addPoint(s5, 0, 0), b = addPoint(s5, 2, 0), c = addPoint(s5, 2, 2), d = addPoint(s5, 0, 2);
        const Id poly = addPolygon(s5, {a, b, c, d}, true);
        TN("D14 poligono chiuso", polyArea(s5, poly), 4, 1e-12);
        const std::size_t before = s5.objects.size();
        addPolygon(s5, {addPoint(s5, 5, 5), addPoint(s5, 6, 6), addPoint(s5, 7, 5)}, false);
        T("D14 spezzata aperta: solo due lati", s5.objects.size() == before + 3 + 2);
        State s6;
        const Id ng = addSpec(s6, Spec::Ngon, Box{-2, -2, 2, 2}, fitBox(), 6);
        TN("D15 esagono regolare", polyArea(s6, ng), 3 * std::sqrt(3.0) / 2 * 4, 1e-12);
        const auto nv = find(s6, ng)->parents;
        dragPoint(s6, nv[3], 0, -5);
        TN("D15 resta regolare", dist(pt(s6, nv[0]), pt(s6, nv[1])), dist(pt(s6, nv[3]), pt(s6, nv[4])), 1e-9);
        TN("D15 vertice spostato", dist(pt(s6, nv[3]), P2{0, -5}), 0, 1e-9);
        T("D15 lati da 3 a 12", throws([&] { addSpec(s6, Spec::Ngon, Box{0, 0, 1, 1}, fitBox(), 13); }) && throws([&] { addSpec(s6, Spec::Ngon, Box{0, 0, 1, 1}, fitBox(), 2); }));
        State s7;
        const Id big = addSpec(s7, Spec::Rectangle, Box{1, 1, 1.001, 1.001}, fitBox());
        TN("D17 riquadro troppo piccolo: la figura piu' grande della finestra", polyArea(s7, big), 0.8 * 13.2 * 0.8 * 10.6, 1e-9);
    }
    {   // D16 funzione
        State s;
        const Id f = addFunction(s, "sin(x)");
        double y = 0;
        T("D16 f(x) in radianti", evalFunction(s, *find(s, f), Pi / 2, y) && std::abs(y - 1) < 1e-12);
        T("D16 testo non valido: Syntax ERROR", throws([&] { addFunction(s, "x+*"); }));
        T("D16 vuota", throws([&] { addFunction(s, ""); }));
        T("D16 equazione", measure(s, MSpec{MType::Equation, {f}}).text == "y=sin(x)");
    }
}

// ---------------------------------------------------------------- Construct
void constructs() {
    State s;
    const Id a = addPoint(s, 0, 0), b = addPoint(s, 4, 0), c = addPoint(s, 1, 3);
    const Id ab = addLine(s, Kind::Segment, a, b);
    {   // C01 asse
        const Id pb = construct(s, Construct::PerpBisector, {ab})[0];
        const Id on = addPointOn(s, pb, 2, 7);
        TN("C01 equidistante", dist(pt(s, on), pt(s, a)), dist(pt(s, on), pt(s, b)), 1e-12);
        const Id pb2 = construct(s, Construct::PerpBisector, {a, c})[0];
        T("C01 da due punti", defined(s, pb2));
        T("C01 selezione sbagliata", throws([&] { construct(s, Construct::PerpBisector, {a}); }, "Argument"));
    }
    {   // C02 perpendicolare, C06 parallela
        const Id per = construct(s, Construct::Perpendicular, {ab, c})[0];
        const Object& o = *find(s, per);
        TN("C02 perpendicolare", (o.g.a[2] - o.g.a[0]) * 4 + (o.g.a[3] - o.g.a[1]) * 0, 0, 1e-12);
        T("C02 passa per il punto", dist(P2{o.g.a[0], o.g.a[1]}, pt(s, c)) < 1e-12);
        const Id par = construct(s, Construct::Parallel, {c, ab})[0];
        const Object& q = *find(s, par);
        TN("C06 parallela", q.g.a[3] - q.g.a[1], 0, 1e-12);
        T("C06 selezione sbagliata", throws([&] { construct(s, Construct::Parallel, {a, c}); }));
    }
    {   // C03 punto medio
        const Id m = construct(s, Construct::Midpoint, {ab})[0];
        T("C03 punto medio del segmento", pt(s, m).x == 2 && pt(s, m).y == 0);
        const Id m2 = construct(s, Construct::Midpoint, {a, c})[0];
        T("C03 di due punti", pt(s, m2).x == 0.5 && pt(s, m2).y == 1.5);
    }
    {   // C04 intersezioni per ogni coppia di tipi
        State t;
        const Id p0 = addPoint(t, -5, 0), p1 = addPoint(t, 5, 0), q0 = addPoint(t, 0, -5), q1 = addPoint(t, 0, 5);
        const Id hx = addLine(t, Kind::Line, p0, p1), vy = addLine(t, Kind::Segment, q0, q1);
        const auto ll = construct(t, Construct::Intersection, {hx, vy});
        T("C04 retta-segmento", ll.size() == 1 && pt(t, ll[0]).x == 0 && pt(t, ll[0]).y == 0);
        const Id cc = addPoint(t, 0, 0), cp = addPoint(t, 3, 0);
        const Id circ = addCircle(t, cc, cp);
        const auto lc = construct(t, Construct::Intersection, {hx, circ});
        T("C04 retta-circonferenza: due punti ordinati sulla retta", lc.size() == 2 && std::abs(pt(t, lc[0]).x + 3) < 1e-12 && std::abs(pt(t, lc[1]).x - 3) < 1e-12);
        const Id c2 = addCircle(t, addPoint(t, 4, 0), addPoint(t, 4, 3));
        const auto cc2 = construct(t, Construct::Intersection, {circ, c2});
        T("C04 circonferenza-circonferenza", cc2.size() == 2 && std::abs(pt(t, cc2[0]).x - 2) < 1e-12 && std::abs(dist(pt(t, cc2[0]), P2{0, 0}) - 3) < 1e-12);
        const Id arc = addArc(t, cc, cp, 0, 3);              // quarto di circonferenza nel primo quadrante
        const auto la = construct(t, Construct::Intersection, {vy, arc});
        T("C04 segmento-arco: solo il punto sull'arco", defined(t, la[0]) != defined(t, la[1]) || (defined(t, la[0]) && !defined(t, la[1])));
        const Id ray = addLine(t, Kind::Ray, addPoint(t, 10, 10), addPoint(t, 11, 10));
        T("C04 nessuna intersezione: errore", throws([&] { construct(t, Construct::Intersection, {ray, circ}); }, "nessuna intersezione"));
        const Id vec = addLine(t, Kind::Vector, addPoint(t, -4, -4), addPoint(t, 4, 4));
        T("C04 vettore-circonferenza (sul vettore)", construct(t, Construct::Intersection, {vec, circ}).size() == 2);
    }
    {   // C05 bisettrici
        State t;
        const Id o = addPoint(t, 0, 0), x = addPoint(t, 4, 0), y = addPoint(t, 0, 4);
        const Id ox = addLine(t, Kind::Segment, o, x), oy = addLine(t, Kind::Segment, o, y);
        const auto bis = construct(t, Construct::AngleBisector, {ox, oy});
        T("C05 due bisettrici", bis.size() == 2 && defined(t, bis[0]) && defined(t, bis[1]));
        TN("C05 angoli uguali", val(t, MType::Angle, {ox, bis[0]}), val(t, MType::Angle, {bis[0], oy}), 1e-12);
        const Id par = addLine(t, Kind::Segment, addPoint(t, 0, 1), addPoint(t, 4, 1));
        T("C05 parallele: errore", throws([&] { construct(t, Construct::AngleBisector, {ox, par}); }, "non si incontrano"));
    }
    {   // C07 tangenti
        State t;
        const Id cc = addPoint(t, 0, 0), cp = addPoint(t, 2, 0);
        const Id circ = addCircle(t, cc, cp);
        const auto tg = construct(t, Construct::Tangent, {circ}, 0, 3);
        const Object& l = *find(t, tg.back());
        const P2 tp = pt(t, tg[0]);
        TN("C07 tangente perpendicolare al raggio", (l.g.a[2] - l.g.a[0]) * tp.x + (l.g.a[3] - l.g.a[1]) * tp.y, 0, 1e-12);
        TN("C07 punto sulla curva", dist(tp, P2{0, 0}), 2, 1e-12);
        const Id f = addFunction(t, "x^3");
        const auto tf = construct(t, Construct::Tangent, {f}, 1, 0);
        const Object& lf = *find(t, tf.back());
        TN("C07 pendenza = f'(x)", (lf.g.a[3] - lf.g.a[1]) / (lf.g.a[2] - lf.g.a[0]), 3, 1e-6);
        T("C07 selezione sbagliata", throws([&] { construct(t, Construct::Tangent, {cc}); }));
    }
    {   // C08 angolo posato
        State t;
        const Id tri = addSpec(t, Spec::Triangle, Box{0, 0, 4, 3}, fitBox());
        const auto sides = ofKind(t, Kind::Segment);
        const Id m = construct(t, Construct::AttachedAngle, {sides[0], sides[2]}, 0.5, 0.5)[0];
        T("C08 misura dell'angolo posata", find(t, m)->kind == Kind::Measure && find(t, m)->flag2 == 1);
        TN("C08 angolo al vertice A", val(t, MType::Angle, {sides[0], sides[2]}), std::atan2(3, 1) * 180 / Pi, 1e-9);
        (void)tri;
    }
    {   // C09 dipendenze e ricalcolo incrementale
        State t;
        const Id p = addPoint(t, 0, 0), q = addPoint(t, 2, 0);
        const Id seg = addLine(t, Kind::Segment, p, q);
        const Id mid = construct(t, Construct::Midpoint, {seg})[0];
        const Id far = addPoint(t, 50, 50);
        const Id far2 = addPoint(t, 60, 50);
        addLine(t, Kind::Segment, far, far2);
        find(t, q)->vars = {6, 0};
        const std::size_t n = recompute(t, {q});
        T("C09 incrementale: solo i discendenti", n == 3);                       // q, segmento, punto medio
        T("C09 il punto medio segue", pt(t, mid).x == 3);
        State full = t;
        recompute(full);
        bool same = true;
        for (std::size_t i = 0; i < t.objects.size(); ++i) for (int k = 0; k < 6; ++k) same = same && t.objects[i].g.a[k] == full.objects[i].g.a[k];
        T("C09 incrementale = completo", same);
        T("C09 antenati", isAncestor(t, q, mid) && !isAncestor(t, far, mid));
    }
    {   // C10 casi degeneri
        State t;
        const Id a = addPoint(t, 0, 0), b = addPoint(t, 4, 0), c = addPoint(t, 0, 1), d = addPoint(t, 4, 2);
        const Id l1 = addLine(t, Kind::Line, a, b), l2 = addLine(t, Kind::Line, c, d);
        const Id x = construct(t, Construct::Intersection, {l1, l2})[0];
        T("C10 rette incidenti", defined(t, x));
        dragPoint(t, d, 4, 1);
        T("C10 rette parallele: indefinito", !defined(t, x));
        dragPoint(t, d, 4, 3);
        T("C10 torna definito", defined(t, x) && std::abs(pt(t, x).x + 2) < 1e-12);
        const Id c1 = addCircle(t, addPoint(t, 10, 10), addPoint(t, 12, 10));
        const Id cen = addPoint(t, 11, 10);
        const Id c2 = addCircle(t, cen, addPoint(t, 11, 12));
        const auto cx = construct(t, Construct::Intersection, {c1, c2});
        dragPoint(t, cen, 10, 10);
        T("C10 cerchi concentrici: indefinito", !defined(t, cx[0]) && !defined(t, cx[1]));
        const Id seg = addLine(t, Kind::Segment, a, b);
        const Id mid = construct(t, Construct::Midpoint, {seg})[0];
        dragPoint(t, b, 0, 0);
        T("C10 punti coincidenti: segmento indefinito", !defined(t, seg) && !defined(t, mid));
        dragPoint(t, b, 2, 0);
        T("C10 torna", defined(t, seg) && pt(t, mid).x == 1);
        // tangenza con tolleranza dichiarata: una sola radice
        State u;
        const Id k = addCircle(u, addPoint(u, 0, 0), addPoint(u, 1, 0));
        const Id tl = addLine(u, Kind::Line, addPoint(u, -2, 1 + 1e-12), addPoint(u, 2, 1 + 1e-12));
        const auto tp = construct(u, Construct::Intersection, {k, tl});
        T("C10 tangente entro EpsTangent: un punto", defined(u, tp[0]) && !defined(u, tp[1]));
    }
}

// ---------------------------------------------------------------- Transform
void transforms() {
    {   // T01-T08
        State s;
        const Id tri = addSpec(s, Spec::Triangle, Box{1, 1, 3, 2}, fitBox());
        const auto v = find(s, tri)->parents;
        const Id ax0 = addPoint(s, 0, -10), ax1 = addPoint(s, 0, 10);
        const Id axis = addLine(s, Kind::Line, ax0, ax1);
        const auto img = transform(s, TType::Reflection, {tri}, axis);
        T("T01 riflessione: punti, lati e poligono", img.size() == 7);
        const Id a1 = img[0];
        T("T01 A' = riflesso di A", pt(s, a1).x == -1 && pt(s, a1).y == 1 && find(s, a1)->label == "A'");
        T("T07 legato all'originale", dragPoint(s, v[0], 2, 1) && pt(s, a1).x == -2);
        T("T07 legato all'asse", dragPoint(s, ax1, 2, 10) && std::abs(pt(s, a1).x - (-2)) > 1e-3);
        const auto tr = transform(s, TType::Translation, {v[1]}, NoId, 2, -1);
        T("T02 traslazione", tr.size() == 1 && pt(s, tr[0]).x == pt(s, v[1]).x + 2 && pt(s, tr[0]).y == pt(s, v[1]).y - 1);
        T("T08 selezione parziale: solo il vertice", tr.size() == 1 && find(s, tr[0])->kind == Kind::Point);
        const Id vec = addLine(s, Kind::Vector, addPoint(s, 0, 0), addPoint(s, 1, 2));
        const auto tv = transform(s, TType::TransVec, {tri}, vec);
        TN("T03 traslazione del vettore", pt(s, tv[0]).x, pt(s, v[0]).x + 1, 1e-12);
        TN("T03 area conservata", polyArea(s, tv.back()), polyArea(s, tri), 1e-12);
        T("T03 serve un vettore", throws([&] { transform(s, TType::TransVec, {tri}, axis); }));
        const Id o = addPoint(s, 0, 0);
        const auto rot = transform(s, TType::Rotation, {v[1]}, o, 90);
        TN("T04 rotazione di 90 gradi (x)", pt(s, rot[0]).x, -pt(s, v[1]).y, 1e-12);
        TN("T04 rotazione di 90 gradi (y)", pt(s, rot[0]).y, pt(s, v[1]).x, 1e-12);
        const auto dil = transform(s, TType::Dilation, {tri}, o, 2);
        TN("T05 omotetia k=2: area x4", polyArea(s, dil.back()), 4 * polyArea(s, tri), 1e-12);
        T("T05 fattore fuori intervallo", throws([&] { transform(s, TType::Dilation, {tri}, o, 0.05); }) && throws([&] { transform(s, TType::Dilation, {tri}, o, 11); }));
        T("T05 fattore negativo ammesso", !throws([&] { transform(s, TType::Dilation, {v[0]}, o, -0.5); }));
        const auto sym = transform(s, TType::Symmetry, {v[2]}, o);
        T("T06 simmetria centrale", pt(s, sym[0]).x == -pt(s, v[2]).x && pt(s, sym[0]).y == -pt(s, v[2]).y);
        const Id f = addFunction(s, "x^2");
        T("T09 f(x) non trasformabile", throws([&] { transform(s, TType::Translation, {f}, NoId, 1, 1); }, "f(x)"));
        T("T01 asse mancante", throws([&] { transform(s, TType::Reflection, {tri}, o); }));
        // immagini di archi e di rette costruite
        const Id arc = addArc(s, addPoint(s, 5, 5), addPoint(s, 7, 5), 5, 9);
        const auto ia = transform(s, TType::Reflection, {arc}, axis);
        TN("T01 arco riflesso: stessa lunghezza", val(s, MType::Circumference, {ia.back()}), val(s, MType::Circumference, {arc}), 1e-12);
        const Id per = construct(s, Construct::Perpendicular, {axis, o})[0];
        const auto ip = transform(s, TType::Translation, {per}, NoId, 0, 3);
        T("T07 retta costruita: immagine della curva", ip.size() == 1 && find(s, ip[0])->def == Def::ImageCurve && defined(s, ip[0]));
    }
}

// ---------------------------------------------------------------- misure, blocchi
void measures() {
    State s;
    s.options.angle = 0;
    const Id a = addPoint(s, 0, 0), b = addPoint(s, 3, 4), c = addPoint(s, 3, 0);
    const Id ab = addLine(s, Kind::Segment, a, b), ac = addLine(s, Kind::Segment, a, c), bc = addLine(s, Kind::Segment, b, c);
    T("M02 coordinate", val(s, MType::Coordinates, {b}, 0) == 3 && val(s, MType::Coordinates, {b}, 1) == 4);
    TN("M03 lunghezza", val(s, MType::Distance, {ab}), 5, 1e-15);
    TN("M03 distanza di due punti", val(s, MType::Distance, {a, c}), 3, 1e-15);
    TN("M04 pendenza", val(s, MType::Slope, {ab}), 4.0 / 3, 1e-15);
    T("M04 verticale: indefinita", !measure(s, MSpec{MType::Slope, {bc}}).ok);
    TN("M05 direzione", val(s, MType::Direction, {ab}), std::atan2(4, 3) * 180 / Pi, 1e-12);
    TN("M06 angolo dal vertice comune", val(s, MType::Angle, {ab, ac}), std::atan2(4, 3) * 180 / Pi, 1e-12);
    TN("M06 angolo con tre punti", val(s, MType::Angle, {b, c, a}), 90, 1e-12);
    TN("M07 supplementare", val(s, MType::Supplementary, {ab, ac}), 180 - std::atan2(4, 3) * 180 / Pi, 1e-12);
    s.options.angle = 1;
    TN("M06 in radianti", val(s, MType::Angle, {b, c, a}), Pi / 2, 1e-12);
    s.options.angle = 0;
    const Id circ = addCircle(s, a, c);
    const Measurement inc = measure(s, MSpec{MType::Incidence, {c, circ}});
    T("M08 incidenza", inc.ok && inc.yesNo && inc.yes && measureValueText(s, MSpec{MType::Incidence, {c, circ}}, 2) == "S\xC3\xAC");
    T("M08 non incidente", !measure(s, MSpec{MType::Incidence, {b, ac}}).yes);
    const Id tl = addLine(s, Kind::Line, addPoint(s, -5, 3), addPoint(s, 5, 3));
    T("M09 tangenza retta-circonferenza", measure(s, MSpec{MType::Tangency, {tl, circ}}).yes);
    const Id c2 = addCircle(s, addPoint(s, 5, 0), addPoint(s, 7, 0));
    T("M09 tangenza tra circonferenze", measure(s, MSpec{MType::Tangency, {circ, c2}}).yes);
    const Id de = addLine(s, Kind::Segment, addPoint(s, 10, 0), addPoint(s, 10, 5));
    T("M10 congruenza", measure(s, MSpec{MType::Congruence, {ab, de}}).yes && !measure(s, MSpec{MType::Congruence, {ab, ac}}).yes);
    TN("M11 area della circonferenza", val(s, MType::Area, {circ}), 9 * Pi, 1e-12);
    TN("M11 area di tre lati che chiudono", val(s, MType::Area, {ab, bc, ac}), 6, 1e-12);
    TN("M12 perimetro", val(s, MType::Perimeter, {ab, bc, ac}), 12, 1e-12);
    T("M13 equazione della retta", measure(s, MSpec{MType::Equation, {ab}}).text == "y=1.33x+0");
    T("M13 equazione della circonferenza", measure(s, MSpec{MType::Equation, {circ}}).text == "x\xC2\xB2+y\xC2\xB2=9");
    T("M13 centro non nullo", measure(s, MSpec{MType::Equation, {c2}}).text == "(x-5)\xC2\xB2+y\xC2\xB2=4");
    T("M13 verticale", measure(s, MSpec{MType::Equation, {bc}}).text == "x=3");
    const Id vec = addLine(s, Kind::Vector, a, b);
    T("M14 componenti", val(s, MType::Components, {vec}, 0) == 3 && val(s, MType::Components, {vec}, 1) == 4);
    TN("M15 raggio", val(s, MType::Radius, {circ}), 3, 1e-15);
    TN("M16 circonferenza", val(s, MType::Circumference, {circ}), 6 * Pi, 1e-12);
    const Id o = addPoint(s, 20, 20);
    const auto r = transform(s, TType::Rotation, {b}, o, 30);
    TN("M17 angolo di rotazione", val(s, MType::RotationAngle, {b, r[0]}), 30, 1e-12);
    const auto d = transform(s, TType::Dilation, {b}, o, 1.5);
    TN("M17 fattore di dilatazione", val(s, MType::DilationScale, {b, d[0]}), 1.5, 1e-15);
    setValue(s, MSpec{MType::RotationAngle, {b, r[0]}}, 60);
    TN("M17 nuovo angolo scritto", val(s, MType::RotationAngle, {b, r[0]}), 60, 1e-12);
    // tipi disponibili secondo la selezione
    auto has = [&](std::vector<Id> sel, MType t) { for (const auto& m : available(s, sel)) if (m.type == t) return true; return false; };
    T("M01 tipi per un punto", has({a}, MType::Coordinates) && has({a}, MType::Label));
    T("M01 tipi per due lati", has({ab, ac}, MType::Angle) && has({ab, ac}, MType::Supplementary) && has({ab, ac}, MType::Congruence));
    T("M01 tipi per punto e curva", has({a, circ}, MType::Incidence));
    T("M01 tipi per retta e circonferenza", has({tl, circ}, MType::Tangency));
    T("M01 tre lati chiusi", has({ab, bc, ac}, MType::Area) && has({ab, bc, ac}, MType::Perimeter));
}
void locks() {
    {   // M18, M19: valore scritto con spostamento minimo e blocco
        State s;
        const Id a = addPoint(s, 0, 0), b = addPoint(s, 3, 4);
        const Id ab = addLine(s, Kind::Segment, a, b);
        setValue(s, MSpec{MType::Distance, {ab}}, 10);
        TN("M18 lunghezza scritta", val(s, MType::Distance, {ab}), 10, 1e-9);
        TN("M18 spostamento minimo: direzione invariata", val(s, MType::Direction, {ab}), std::atan2(4, 3) * 180 / Pi, 1e-6);
        TN("M18 i due estremi si muovono in modo simmetrico", pt(s, a).x + pt(s, b).x, 3, 1e-6);
        T("M18 bloccata", isLocked(s, MSpec{MType::Distance, {ab}}) && s.locks.size() == 1);
        T("M19 spostando un estremo la lunghezza resta", dragPoint(s, b, 20, 0));
        TN("M19 lunghezza dopo lo spostamento", val(s, MType::Distance, {ab}), 10, 1e-9);
        TN("M19 il punto trascinato va quasi dove indicato", pt(s, b).y, 0, 1e-3);
        unlock(s, MSpec{MType::Distance, {ab}});
        T("M19 sblocco", s.locks.empty() && dragPoint(s, b, 1, 0) && std::abs(val(s, MType::Distance, {ab}) - dist(pt(s, a), P2{1, 0})) < 1e-12);
    }
    {   // M18 su altri tipi
        State s;
        s.options.angle = 0;
        const Id o = addPoint(s, 0, 0), x = addPoint(s, 4, 0), y = addPoint(s, 1, 3);
        const Id ox = addLine(s, Kind::Segment, o, x), oy = addLine(s, Kind::Segment, o, y);
        setValue(s, MSpec{MType::Angle, {ox, oy}}, 90);
        TN("M18 angolo scritto", val(s, MType::Angle, {ox, oy}), 90, 1e-8);
        setValue(s, MSpec{MType::Coordinates, {x}}, 5, 1);
        T("M18 coordinate bloccate", pt(s, x).x == 5 && pt(s, x).y == 1);
        TN("M18 l'angolo resta 90", val(s, MType::Angle, {ox, oy}), 90, 1e-8);
        T("M19 un punto con le coordinate bloccate non si sposta", !dragPoint(s, x, 7, 7) && pt(s, x).x == 5);
        const Id c = addCircle(s, addPoint(s, 10, 10), addPoint(s, 12, 10));
        setValue(s, MSpec{MType::Radius, {c}}, 5);
        TN("M18 raggio", val(s, MType::Radius, {c}), 5, 1e-9);
        T("M21 raggio bloccato e area diversa: incompatibili", throws([&] { setValue(s, MSpec{MType::Area, {c}}, 4 * Pi); }, "incompatibili"));
        unlock(s, MSpec{MType::Radius, {c}});
        setValue(s, MSpec{MType::Area, {c}}, 4 * Pi);
        TN("M18 area della circonferenza", val(s, MType::Radius, {c}), 2, 1e-8);
        const Id v = addLine(s, Kind::Vector, addPoint(s, -5, -5), addPoint(s, -4, -4));
        setValue(s, MSpec{MType::Components, {v}}, 3, -2);
        T("M18 componenti", std::abs(val(s, MType::Components, {v}, 0) - 3) < 1e-9 && std::abs(val(s, MType::Components, {v}, 1) + 2) < 1e-9);
        const Id l = addLine(s, Kind::Line, addPoint(s, 0, -8), addPoint(s, 1, -7));
        setValue(s, MSpec{MType::Slope, {l}}, -2);
        TN("M18 pendenza", val(s, MType::Slope, {l}), -2, 1e-8);
        T("M21 componenti bloccate e direzione diversa", throws([&] { setValue(s, MSpec{MType::Direction, {v}}, 45); }, "incompatibili"));
        const Id v2 = addLine(s, Kind::Vector, addPoint(s, 5, -5), addPoint(s, 6, -3));
        setValue(s, MSpec{MType::Direction, {v2}}, 45);
        TN("M18 direzione", val(s, MType::Direction, {v2}), 45, 1e-8);
        T("M18 valore non positivo", throws([&] { setValue(s, MSpec{MType::Radius, {c}}, -1); }));
        T("M18 misura non modificabile", throws([&] { setValue(s, MSpec{MType::Equation, {l}}, 1); }));
    }
    {   // M08-M10 bloccate: Si' forzato
        State s;
        const Id c = addCircle(s, addPoint(s, 0, 0), addPoint(s, 2, 0));
        const Id p = addPoint(s, 5, 5);
        lock(s, MSpec{MType::Incidence, {p, c}});
        T("M08 incidenza bloccata: il punto va sulla curva", measure(s, MSpec{MType::Incidence, {p, c}}).yes);
        dragPoint(s, p, 9, 0);
        T("M08 resta sulla curva", measure(s, MSpec{MType::Incidence, {p, c}}).yes);
        const Id l = addLine(s, Kind::Line, addPoint(s, -5, 4), addPoint(s, 5, 4.5));
        lock(s, MSpec{MType::Tangency, {l, c}});
        T("M09 tangenza bloccata", measure(s, MSpec{MType::Tangency, {l, c}}).yes);
        const Id s1 = addLine(s, Kind::Segment, addPoint(s, 10, 0), addPoint(s, 13, 0));
        const Id s2 = addLine(s, Kind::Segment, addPoint(s, 10, 5), addPoint(s, 11, 5));
        lock(s, MSpec{MType::Congruence, {s1, s2}});
        TN("M10 congruenza bloccata", val(s, MType::Distance, {s1}), val(s, MType::Distance, {s2}), 1e-9);
    }
    {   // fuzz_geometry (fuzz_geometry_rotation_locks.bin): cambiare l'angolo di una rotazione
        // risolve di nuovo i blocchi sulle immagini; se non si puo', nulla cambia
        State s;
        const Id o = addPoint(s, 0, 0), a = addPoint(s, 2, 0);
        const auto img = transform(s, TType::Rotation, {a}, o, 90);
        const Id q = addPoint(s, 0, 5);
        const Id seg = addLine(s, Kind::Segment, img[0], q);
        setValue(s, MSpec{MType::Distance, {seg}}, 3);
        setValue(s, MSpec{MType::RotationAngle, {a, img[0]}}, 180);
        TN("M17 nuovo angolo con un blocco: il blocco resta vero", val(s, MType::Distance, {seg}), 3, 1e-9);
        TN("M17 l'angolo e' 180", val(s, MType::RotationAngle, {a, img[0]}), 180, 1e-12);
        // tutti i punti liberi fissati: un angolo diverso non puo' tenere la lunghezza
        State f;
        const Id o2 = addPoint(f, 0, 0), a2 = addPoint(f, 2, 0);
        const auto im2 = transform(f, TType::Rotation, {a2}, o2, 90);
        const Id q2 = addPoint(f, 0, 5);
        const Id s2 = addLine(f, Kind::Segment, im2[0], q2);
        setValue(f, MSpec{MType::Coordinates, {o2}}, 0, 0);
        setValue(f, MSpec{MType::Coordinates, {a2}}, 2, 0);
        setValue(f, MSpec{MType::Coordinates, {q2}}, 0, 5);
        setValue(f, MSpec{MType::Distance, {s2}}, 3);
        const State before = f;
        T("M21 nuovo angolo impossibile con i blocchi: errore", throws([&] { setValue(f, MSpec{MType::RotationAngle, {a2, im2[0]}}, 30); }, "incompatibili"));
        T("M21 nessun cambiamento", f == before);
    }
    {   // M21 incompatibili
        State s;
        const Id a = addPoint(s, 0, 0), b = addPoint(s, 3, 0);
        const Id ab = addLine(s, Kind::Segment, a, b);
        setValue(s, MSpec{MType::Coordinates, {a}}, 0, 0);
        setValue(s, MSpec{MType::Coordinates, {b}}, 3, 0);
        const State before = s;
        T("M21 vincoli incompatibili: errore", throws([&] { setValue(s, MSpec{MType::Distance, {ab}}, 7); }, "incompatibili"));
        T("M21 nessun cambiamento", s == before);
    }
    {   // M20 misura incollata e aggiornata; O02 etichette; O04 formato
        State s;
        const Id a = addPoint(s, 0, 0), b = addPoint(s, 3, 4);
        const Id ab = addLine(s, Kind::Segment, a, b);
        const Id m = addMeasureText(s, MSpec{MType::Distance, {ab}}, 1, 1);
        T("M20 testo incollato", measureText(s, *find(s, m)) == "Lunghezza=5");
        dragPoint(s, b, 6, 8);
        T("M20 aggiornato", measureText(s, *find(s, m)) == "Lunghezza=10");
        dragPoint(s, b, 1, 1);
        T("O04 Fix2 iniziale", measureText(s, *find(s, m)) == "Lunghezza=1.41");
        find(s, m)->fmt = 4;
        T("O04 Fix4 della misura", measureText(s, *find(s, m)) == "Lunghezza=1.4142");
        find(s, m)->fmt = FmtSci1 + 2;
        T("O04 Sci3", measureText(s, *find(s, m)) == "Lunghezza=1.41E+00");
        find(s, m)->fmt = FmtNorm;
        T("O04 Norm", measureText(s, *find(s, m)) == "Lunghezza=1.414213562");
        T("O04 interi senza decimali", formatNumber(5, 3) == "5" && formatNumber(-0.0001, 2) == "0.00");
        setValue(s, MSpec{MType::Label, {m}}, 0, 0, "AB");
        T("M20 nome modificabile", measureText(s, *find(s, m)) == "AB=1.414213562");
        s.options.lengthUnit = 2;
        T("S01 unita' di lunghezza", measureText(s, *find(s, m)) == "AB=1.414213562 cm");
        setValue(s, MSpec{MType::Label, {a}}, 0, 0, "O");
        T("O02 etichetta modificata", find(s, a)->label == "O");
        T("O02 al piu' 14 caratteri", throws([&] { setValue(s, MSpec{MType::Label, {a}}, 0, 0, "ABCDEFGHIJKLMNO"); }));
        State l;
        for (int i = 0; i < 28; ++i) addPoint(l, i, 0);
        T("O02 etichette automatiche", l.objects[0].label == "A" && l.objects[25].label == "Z" && l.objects[26].label == "A1" && l.objects[27].label == "B1");
        // O03 Expression
        const Id m2 = addMeasureText(s, MSpec{MType::Coordinates, {b}}, 2, 2);
        const Id e = addExpression(s, 3, 3, "@1*2+@2Y");
        const Measurement ev = measure(s, MSpec{MType::Expression, {e}});
        TN("O03 espressione con @n e @nY", ev.v[0], 2 * std::sqrt(2.0) + 1, 1e-9);
        T("O03 misura inesistente", throws([&] { addExpression(s, 0, 0, "@9+1"); }));
        remove(s, {m2});
        T("O03 cancellando la misura sparisce l'espressione", !find(s, e));
    }
}

// ---------------------------------------------------------------- Edit, opzioni, vista
void edits() {
    State s;
    const Id tri = addSpec(s, Spec::Triangle, Box{0, 0, 4, 3}, fitBox());
    const auto v = find(s, tri)->parents;
    const Id lone = addPoint(s, 9, 9);
    {   // E04 figura
        const auto fig = figureOf(s, {v[0]});
        T("E04 tutta la figura del vertice", fig.size() == 7);
        T("E04 un punto da solo", figureOf(s, {lone}).size() == 1);
    }
    {   // E05 cancellazione con i dipendenti
        State t = s;
        const Id m = construct(t, Construct::Midpoint, {v[0], v[1]})[0];
        const std::size_t n = remove(t, {v[0]});
        T("E05 vertice: lati, poligono e punto medio", n == 5 && !find(t, m) && !find(t, tri));
        T("E05 ordine topologico conservato", std::is_sorted(t.objects.begin(), t.objects.end(), [](const Object& a, const Object& b) { return a.id < b.id; }));
    }
    {   // E01 Undo: istantanea codificata
        std::vector<std::uint8_t> snap;
        encode(s, snap);
        State t = s;
        addPoint(t, 1, 1);
        State back;
        T("E01 istantanea", decode(back, snap.data(), snap.size()) && back == s && !(t == s));
    }
    {   // E08 spostamento della selezione
        State t = s;
        T("E08 sposta la figura", translate(t, {tri}, 1, 2) && pt(t, v[0]).x == 1 && pt(t, v[0]).y == 2 && pt(t, v[2]).y == 5);
        T("E08 un vertice vincolato si muove sul suo grado di liberta'", true);
        const Id m = construct(t, Construct::Midpoint, {v[0], v[1]})[0];
        T("E08 un punto costruito non si muove da solo", !movable(t, m) && !translate(t, {m}, 1, 1));
        const Id txt = addText(t, 0, 0, "Ciao");
        T("E08 testo spostabile", translate(t, {txt}, 2, 3) && find(t, txt)->vars[0] == 2);
    }
    {   // O05 Clr Constraint
        State t;
        const Id rect = addSpec(t, Spec::Rectangle, Box{0, 0, 4, 2}, fitBox());
        const auto rv = find(t, rect)->parents;
        const Id seg = ofKind(t, Kind::Segment)[0];
        setValue(t, MSpec{MType::Distance, {seg}}, 5);
        clearConstraints(t);
        T("O05 blocchi tolti", t.locks.empty());
        T("O05 vertici liberi", find(t, rv[2])->def == Def::Free && find(t, rv[3])->def == Def::Free);
        dragPoint(t, rv[2], 10, 10);
        T("O05 il rettangolo non e' piu' vincolato", pt(t, rv[3]).x != pt(t, rv[0]).x + (10 - pt(t, rv[1]).x) || pt(t, rv[2]).x == 10);
    }
    {   // O07 Hide, O06 Show All (dati), O09/O10 ordine
        State t = s;
        find(t, lone)->hidden = true;
        T("O07 nascosto: non selezionabile", hitTest(t, 9, 9, 0.1) == NoId);
        toFront(t, {v[0]});
        toBack(t, {lone});
        T("O09 davanti e dietro", find(t, v[0])->z > 0 && find(t, lone)->z < 0);
        const Id txt = addText(t, 0, 0, "T");
        textsToFront(t);
        T("O10 testi davanti", find(t, txt)->z > find(t, v[0])->z);
        find(t, tri)->style.area = 4;
        find(t, tri)->style.lineStyle = LineStyle::Broken;
        T("O14 stile salvato", roundTrip(t));
    }
    {   // V06, V09 vista
        State t = s;
        T("V06 Zoom to Fit", fit(t, 320, 250) && std::abs(t.view.cx - 4.5) < 1e-9);
        double x, y;
        toPlane(t.view, 320, 250, 0, 0, x, y);
        T("V06 tutti gli oggetti visibili", x <= 0 && y >= 9);
        State e;
        T("V06 nulla da inquadrare", !fit(e, 320, 250));
        View w;
        setWindow(w, 320, -8, 8, 1);
        T("V09 V-Window", w.scale == 20 && w.cx == 0 && w.cy == 1);
        T("V09 Xmin >= Xmax", throws([&] { setWindow(w, 320, 3, 3, 0); }));
        View z;
        zoom(z, 2);
        T("V04 zoom x2", z.scale == 48);
        zoomBox(z, 320, 240, 0, 0, 4, 3);
        T("V01 riquadro", z.cx == 2 && z.cy == 1.5 && z.scale == 80);
    }
}

// ---------------------------------------------------------------- animazioni
void animations() {
    State s;
    const Id a = addPoint(s, 0, 0), b = addPoint(s, 10, 0);
    const Id ab = addLine(s, Kind::Segment, a, b);
    const Id p = addPoint(s, 3, 3);
    const Id q = addPoint(s, -4, 4);
    const Id pq = addLine(s, Kind::Segment, p, q);
    addAnimation(s, p, ab, false);
    T("A01 aggiunta", s.animations.size() == 1 && s.animations[0].steps == 20 && s.animations[0].t0 == 0 && s.animations[0].t1 == 1);
    T("A01 il punto non puo' definire il percorso", throws([&] { addAnimation(s, a, ab, false); }));
    const Id c = addCircle(s, addPoint(s, 20, 0), addPoint(s, 22, 0));
    addAnimation(s, q, c, false);
    T("A01 piu' punti", s.animations.size() == 2);
    addAnimation(s, q, c, true);
    T("A02 Replace", s.animations.size() == 1 && s.animations[0].point == q);
    addAnimation(s, p, ab, false);
    setTrace(s, {p, ab});
    T("A03 solo i punti hanno la traccia", s.traced.size() == 1 && s.traced[0] == p);
    setAnimation(s, 0, 5, 0, 1, 2);
    setAnimation(s, 1, 11, 0, 0.5, 0);
    T("A04 limiti", throws([&] { setAnimation(s, 0, 1, 0, 1, 0); }) && throws([&] { setAnimation(s, 0, 101, 0, 1, 0); }) && throws([&] { setAnimation(s, 0, 20, -11, 1, 0); }));
    addTableColumn(s, MSpec{MType::Distance, {pq}});
    addTableColumn(s, MSpec{MType::Coordinates, {p}});
    T("A07 colonne (le coordinate due)", s.columns.size() == 3);
    const State before = s;
    {
        AnimationTask task(s, false, true);
        step::Budget one(1, 0, nullptr);
        T("A05 primo fotogramma", !task.step(one) && task.done() == 1 && pt(s, p).x == 0);
        step::Budget all;
        T("A05 Go (once) finisce", task.step(all));
        T("A12 i punti tornano dov'erano", pt(s, p).x == 3 && pt(s, q).x == -4);
        T("A08 tabella: una riga per passo", s.tableRows == 11 && s.table.size() == 33);
        TN("A08 ultima riga: x di P", s.table[10 * 3 + 1], 5, 1e-12);            // t1 = 0.5 sul segmento
        TN("A08 prima riga: lunghezza PQ (Q sulla circonferenza a t=0)", s.table[0], 22, 1e-12);
        T("A03 traccia di P", task.traces().size() == 1 && task.traces()[0].size() == 2 * 11);
    }
    {   // circonferenza: un giro, e animazione su una funzione
        State t;
        const Id cc = addCircle(t, addPoint(t, 0, 0), addPoint(t, 1, 0));
        const Id m = addPoint(t, 5, 5);
        addAnimation(t, m, cc, false);
        double x, y;
        T("A01 circonferenza: t=0.25 un quarto di giro", pathPoint(t, t.animations[0], 0.25, x, y) && std::abs(x) < 1e-12 && std::abs(y - 1) < 1e-12);
        const Id f = addFunction(t, "x^2");
        const Id m2 = addPoint(t, 1, 1);
        addAnimation(t, m2, f, false);
        T("A01 funzione: t = x", t.animations[1].t0 == -5 && pathPoint(t, t.animations[1], 3, x, y) && x == 3 && y == 9);
        const Id l = addLine(t, Kind::Line, addPoint(t, 0, -1), addPoint(t, 1, -1));
        const Id m3 = addPoint(t, 9, 9);
        addAnimation(t, m3, l, false);
        T("A01 retta oltre B", pathPoint(t, t.animations[2], 3, x, y) && x == 3 && y == -1);
    }
    {   // A06 Go (repeat) e AC
        State t = before;
        setAnimation(t, 0, 3, 0, 1, 0);
        setAnimation(t, 1, 3, 0, 1, 0);
        AnimationTask task(t, true, false);
        step::Budget b(50, 0, nullptr);
        T("A06 Times 0: non finisce", !task.step(b) && task.cycle() >= 10);
        task.cancelled();
        T("A12 AC: punti a posto", pt(t, p).x == 3 && pt(t, q).x == -4);
        State u = before;
        setAnimation(u, 0, 4, 0, 1, 3);
        setAnimation(u, 1, 4, 0, 1, 3);
        AnimationTask t3(u, true, false);
        step::Budget all;
        T("A06 Times 3", t3.step(all) && t3.cycle() == 3);
    }
    {   // A11 colonne
        State t = before;
        removeColumn(t, 0);
        T("A11 DELETE di una colonna", t.columns.size() == 2);
        clearTable(t);
        T("A11 DEL-ALL", t.columns.empty() && t.tableRows == 0);
        removeAnimation(t, 0);
        T("A04 DELETE di una riga", t.animations.size() == 1);
    }
    {   // animazione con un blocco: il vincolo resta vero a ogni fotogramma
        State t;
        const Id o = addPoint(t, 0, 0), x = addPoint(t, 3, 0), y = addPoint(t, 0, 3);
        const Id xy = addLine(t, Kind::Segment, x, y);
        addLine(t, Kind::Segment, o, x);
        setValue(t, MSpec{MType::Distance, {xy}}, 5);
        const Id path = addLine(t, Kind::Segment, addPoint(t, 1, -5), addPoint(t, 4, -5));
        const Id mover = addPoint(t, 9, 9);
        addAnimation(t, mover, path, false);
        addTableColumn(t, MSpec{MType::Distance, {xy}});
        AnimationTask task(t, false, true);
        step::Budget all;
        task.step(all);
        bool ok = true;
        for (std::size_t r = 0; r < t.tableRows; ++r) ok = ok && std::abs(t.table[r] - 5) < 1e-9;
        T("A12 blocco rispettato durante l'animazione", ok && t.tableRows == 20);
    }
    {   // fuzz_geometry (tests/fixtures/fuzz/fuzz_geometry_anim_locks.bin): con un blocco sul punto
        // animato il risolutore muove anche l'altro punto; alla fine tutto torna com'era
        State t;
        const Id p = addPoint(t, 0, 0), q = addPoint(t, 3, 4);
        const Id pq = addLine(t, Kind::Segment, p, q);
        setValue(t, MSpec{MType::Distance, {pq}}, 5);
        const Id path = addLine(t, Kind::Segment, addPoint(t, -2, -2), addPoint(t, 2, -2));
        addAnimation(t, p, path, false);
        const State before = t;
        AnimationTask task(t, false, false);
        step::Budget all;
        task.step(all);
        T("A12 con i blocchi: tutti i punti tornano dov'erano", t == before && pt(t, q).x == 3 && pt(t, q).y == 4);
        AnimationTask t2(t, true, false);
        step::Budget few(3, 0, nullptr);
        (void)t2.step(few);
        t2.cancelled();
        T("A12 con i blocchi: AC rimette tutto", t == before);
    }
}

// ---------------------------------------------------------------- persistenza e limiti
void persistence() {
    State s;
    const Id tri = addSpec(s, Spec::Isosceles, Box{0, 0, 4, 3}, fitBox());
    const auto v = find(s, tri)->parents;
    const Id ax = addLine(s, Kind::Line, addPoint(s, -1, -1), addPoint(s, -1, 1));
    transform(s, TType::Reflection, {tri}, ax);
    addFunction(s, "x^2-1");
    addText(s, 2, 2, "Triangolo");
    const Id seg = ofKind(s, Kind::Segment)[0];
    setValue(s, MSpec{MType::Distance, {seg}}, 6);
    addMeasureText(s, MSpec{MType::Distance, {seg}}, 0, -1);
    addAnimation(s, addPoint(s, 7, 7), seg, false);
    setTrace(s, {v[0]});
    addTableColumn(s, MSpec{MType::Distance, {seg}});
    s.options.numFormat = 4;
    s.options.grid = Grid::Line;
    s.options.gridSpace = 0.5;
    s.options.axes = Axes::Scale;
    s.view.scale = 30;
    s.background = "PICT03.pcpict";
    s.fade = 40;
    s.file = "TRI.pcgeo";
    T("P01 andata e ritorno identica (sezione e file)", roundTrip(s));
    std::vector<std::uint8_t> b;
    encode(s, b);
    T("P01 versione 2 nell'intestazione", b[0] == 2);
    bool cuts = true;
    for (std::size_t cut = 0; cut < b.size(); cut += 5) { State t; cuts = cuts && !decode(t, b.data(), cut); }
    T("P01 dati troncati rifiutati", cuts);
    auto bad = b;
    bad[0] = 3;
    State t;
    T("P01 versione piu' nuova rifiutata", !decode(t, bad.data(), bad.size()));
    // un genitore con id maggiore (grafo non topologico) e' rifiutato
    State g;
    const Id p0 = addPoint(g, 0, 0), p1 = addPoint(g, 1, 0);
    addLine(g, Kind::Segment, p0, p1);
    g.objects[2].parents[1] = 9;
    std::vector<std::uint8_t> gb;
    encode(g, gb);
    State gt;
    T("P01 genitore non valido rifiutato", !decode(gt, gb.data(), gb.size()));

    // P02, P03: file e sezione della v1.17.0
    State m;
    T("P03 sezione v1 letta", decode(m, kGeometryV1Section, sizeof kGeometryV1Section));
    T("P02 migrazione: punti liberi e figure", ofKind(m, Kind::Point).size() == 1 + 2 + 2 + 2 + 4 && ofKind(m, Kind::Segment).size() == 1 + 4 &&
                                               ofKind(m, Kind::Line).size() == 1 && ofKind(m, Kind::Circle).size() == 1 && ofKind(m, Kind::Polygon).size() == 1);
    T("P02 vista e file", m.view.cx == 1.5 && m.view.cy == -0.5 && m.view.scale == 36 && m.file == "SCUOLA.pcgeo");
    TN("P02 misure conservate", val(m, MType::Area, {kindAt(m, Kind::Polygon)}), 12, 1e-12);
    TN("P02 raggio", val(m, MType::Radius, {kindAt(m, Kind::Circle)}), 2, 1e-12);
    const Id first = ofKind(m, Kind::Point)[1];
    dragPoint(m, first, 1, 1);
    T("P02 le figure migrate sono legate ai loro punti", find(m, kindAt(m, Kind::Segment))->g.a[0] == 1);
    T("P02 la versione 2 ricodificata si rilegge", roundTrip(m));
    // file con l'intestazione dei file tipizzati: i dati iniziano dopo 16 byte
    const std::size_t hdr = 16;
    State fm;
    T("P02 file .pcgeo della v1.17.0", decodeFile(kGeometryV1File + hdr, sizeof kGeometryV1File - hdr, fm) && fm.objects.size() == m.objects.size());
    // v1.20.0 (fuzz_modes): vista della v1.17.0 con il centro oltre 1e150 (finito,
    // accettato dalla v1): migrata all'origine, il formato 2 si rilegge
    {
        std::vector<std::uint8_t> v1{1, 0, 0};                  // versione 1, nessun oggetto
        auto f64 = [&](double d) { std::uint64_t u; std::memcpy(&u, &d, 8); for (int i = 0; i < 8; ++i) v1.push_back(static_cast<std::uint8_t>(u >> (8 * i))); };
        f64(1e200); f64(-3); f64(36);
        State far;
        T("P02 centro fuori scala della v1.17.0", decodeFile(v1.data(), v1.size(), far) && far.view.cx == 0 && far.view.cy == -3);
        const auto again = encodeFile(far);
        State back;
        T("P02 ... e il formato 2 si rilegge", decodeFile(again.data(), again.size(), back) && back == far);
    }

    // P04 limiti
    State full;
    const std::size_t cap = maxObjects();
    for (std::size_t i = 0; i < cap; ++i) addPoint(full, static_cast<double>(i), 0);
    T("P04 Memory ERROR oltre il limite", throws([&] { addPoint(full, 0, 1); }, "Memory ERROR"));
    const std::size_t n = full.objects.size();
    T("P04 nulla aggiunto", throws([&] { addSpec(full, Spec::Triangle, Box{0, 0, 1, 1}, fitBox()); }) && full.objects.size() == n);
    State vv;
    std::vector<Id> many;
    for (int i = 0; i < 51; ++i) many.push_back(addPoint(vv, std::cos(i * 0.1), std::sin(i * 0.1)));
    T("P04 al piu' 50 vertici", throws([&] { addPolygon(vv, many, true); }));
    T("P04 testo di 31 caratteri", throws([&] { addText(vv, 0, 0, std::string(32, 'a')); }) && !throws([&] { addText(vv, 0, 0, std::string(31, 'a')); }));
    State an;
    const Id path = addLine(an, Kind::Segment, addPoint(an, 0, 0), addPoint(an, 1, 0));
    for (std::size_t i = 0; i < MaxAnimations; ++i) addAnimation(an, addPoint(an, 5, static_cast<double>(i)), path, false);
    T("P04 al piu' 16 animazioni", throws([&] { addAnimation(an, addPoint(an, 9, 9), path, false); }, "Memory"));
    State cl;
    const Id cp = addPoint(cl, 0, 0);
    for (std::size_t i = 0; i < 13; ++i) addTableColumn(cl, MSpec{MType::Coordinates, {cp}});
    T("P04 al piu' 26 colonne", throws([&] { addTableColumn(cl, MSpec{MType::Coordinates, {cp}}); }));
}

// ---------------------------------------------------------------- proprieta' (§8.2)
void properties() {
    Rng rng;
    int okReflect = 0, okRot = 0, okDil = 0, okTrans = 0, okSym = 0, okTwo = 0, okMid = 0, okInter = 0, okTan = 0, okBis = 0, okInc = 0, okCodec = 0, okSolve = 0;
    const int N = 60;
    for (int it = 0; it < N; ++it) {
        State s;
        s.options.angle = 0;
        std::vector<Id> v;
        for (int k = 0; k < 3; ++k) v.push_back(addPoint(s, rng.in(-5, 5), rng.in(-5, 5)));
        Id poly = NoId;
        try { poly = addPolygon(s, v, true); } catch (...) { continue; }
        const Id l0 = addPoint(s, rng.in(-5, 5), rng.in(-5, 5)), l1 = addPoint(s, rng.in(-5, 5), rng.in(-5, 5));
        const Id axis = addLine(s, Kind::Line, l0, l1);
        // G01 riflessione due volte = identita'
        const auto r1 = transform(s, TType::Reflection, {poly}, axis);
        const auto r2 = transform(s, TType::Reflection, {r1.back()}, axis);
        bool id = true;
        for (int k = 0; k < 3; ++k) id = id && dist(pt(s, r2[static_cast<std::size_t>(k)]), pt(s, v[static_cast<std::size_t>(k)])) < 1e-9;
        okReflect += id;
        // G02 rotazione: distanze conservate
        const Id c = addPoint(s, rng.in(-5, 5), rng.in(-5, 5));
        const double ang = rng.in(-360, 360);
        const auto ro = transform(s, TType::Rotation, {poly}, c, ang);
        okRot += std::abs(dist(pt(s, ro[0]), pt(s, ro[1])) - dist(pt(s, v[0]), pt(s, v[1]))) < 1e-9 &&
                 std::abs(dist(pt(s, ro[0]), pt(s, c)) - dist(pt(s, v[0]), pt(s, c))) < 1e-9;
        // G03 dilatazione: lunghezze x|k|, area x k^2
        const double k = (rng.next() < 0.5 ? -1 : 1) * rng.in(0.1, 10);
        const auto di = transform(s, TType::Dilation, {poly}, c, k);
        okDil += std::abs(polyArea(s, di.back()) - k * k * polyArea(s, poly)) <= 1e-9 * std::max(1.0, k * k * polyArea(s, poly)) &&
                 std::abs(dist(pt(s, di[0]), pt(s, di[1])) - std::abs(k) * dist(pt(s, v[0]), pt(s, v[1]))) < 1e-8 * std::max(1.0, std::abs(k));
        // G04 traslazione: vettori conservati
        const auto tr = transform(s, TType::Translation, {poly}, NoId, rng.in(-5, 5), rng.in(-5, 5));
        okTrans += std::abs((pt(s, tr[1]).x - pt(s, tr[0]).x) - (pt(s, v[1]).x - pt(s, v[0]).x)) < 1e-12 &&
                   std::abs((pt(s, tr[1]).y - pt(s, tr[0]).y) - (pt(s, v[1]).y - pt(s, v[0]).y)) < 1e-12;
        // G05 Symmetry = rotazione di 180
        const auto sy = transform(s, TType::Symmetry, {v[0]}, c);
        const auto r180 = transform(s, TType::Rotation, {v[0]}, c, 180);
        okSym += dist(pt(s, sy[0]), pt(s, r180[0])) < 1e-9;
        // G06 due riflessioni su rette per C con angolo theta = rotazione di 2 theta
        const double th = rng.in(0.1, 3.0);
        const Id d1 = addPoint(s, pt(s, c).x + 1, pt(s, c).y), d2 = addPoint(s, pt(s, c).x + std::cos(th), pt(s, c).y + std::sin(th));
        const Id m1 = addLine(s, Kind::Line, c, d1), m2 = addLine(s, Kind::Line, c, d2);
        const auto f1 = transform(s, TType::Reflection, {v[1]}, m1);
        const auto f2 = transform(s, TType::Reflection, {f1[0]}, m2);
        const auto rr = transform(s, TType::Rotation, {v[1]}, c, 2 * th * 180 / Pi);
        okTwo += dist(pt(s, f2[0]), pt(s, rr[0])) < 1e-9;
        // G07 punto medio equidistante
        const Id mid = construct(s, Construct::Midpoint, {v[0], v[1]})[0];
        okMid += std::abs(dist(pt(s, mid), pt(s, v[0])) - dist(pt(s, mid), pt(s, v[1]))) < 1e-12;
        // G08 intersezioni sopra entrambe le curve (retta e circonferenza)
        const Id circ = addCircle(s, c, v[2]);
        try {
            const auto xs = construct(s, Construct::Intersection, {axis, circ});
            bool on = true;
            for (Id x : xs) if (defined(s, x)) on = on && measure(s, MSpec{MType::Incidence, {x, axis}}).yes && measure(s, MSpec{MType::Incidence, {x, circ}}).yes;
            okInter += on;
        } catch (...) { ++okInter; }
        // G10 tangente perpendicolare al raggio
        const auto tg = construct(s, Construct::Tangent, {circ}, rng.in(-5, 5), rng.in(-5, 5));
        const P2 tp = pt(s, tg[0]), cc = pt(s, c);
        const Object& tl = *find(s, tg.back());
        okTan += std::abs((tl.g.a[2] - tl.g.a[0]) * (tp.x - cc.x) + (tl.g.a[3] - tl.g.a[1]) * (tp.y - cc.y)) < 1e-9 * std::max(1.0, dist(tp, cc) * dist(tp, cc));
        // G11 bisettrici ad angoli uguali
        const auto sides = ofKind(s, Kind::Segment);
        try {
            const auto bis = construct(s, Construct::AngleBisector, {sides[0], sides[1]});
            okBis += std::abs(val(s, MType::Angle, {sides[0], bis[0]}) - val(s, MType::Angle, {bis[0], sides[1]})) < 1e-8 ||
                     std::abs(val(s, MType::Angle, {sides[0], bis[1]}) - val(s, MType::Angle, {bis[1], sides[1]})) < 1e-8;
        } catch (...) { ++okBis; }
        // G12 incrementale = completo dopo uno spostamento
        dragPoint(s, v[0], rng.in(-5, 5), rng.in(-5, 5));
        State full = s;
        recompute(full);
        bool same = true;
        for (std::size_t i = 0; i < s.objects.size(); ++i) {
            same = same && s.objects[i].g.ok == full.objects[i].g.ok;
            if (s.objects[i].g.ok) for (int q = 0; q < 6; ++q) same = same && s.objects[i].g.a[q] == full.objects[i].g.a[q];
        }
        okInc += same;
        // G13 codifica e decodifica identiche
        okCodec += roundTrip(s);
        // G14 dopo il risolutore i residui sono entro la tolleranza
        try {
            setValue(s, MSpec{MType::Distance, {sides[0]}}, rng.in(1, 8));
            bool res = true;
            for (const auto& l : s.locks) for (std::size_t i = 0; i < residualCount(l); ++i) res = res && std::abs(residual(s, l, i)) <= 1e-9 * std::max(1.0, std::abs(l.v[0]));
            okSolve += res;
        } catch (...) { ++okSolve; }
    }
    T("G01 riflessione due volte = identita'", okReflect == N);
    T("G02 la rotazione conserva le distanze", okRot == N);
    T("G03 dilatazione: lunghezze x|k|, area x k^2", okDil == N);
    T("G04 la traslazione conserva i vettori", okTrans == N);
    T("G05 Symmetry = rotazione di 180", okSym == N);
    T("G06 due riflessioni = rotazione di 2 theta", okTwo == N);
    T("G07 punto medio equidistante", okMid == N);
    T("G08 intersezioni sopra le due curve", okInter == N);
    T("G10 tangente perpendicolare al raggio", okTan == N);
    T("G11 bisettrici ad angoli uguali", okBis == N);
    T("G12 ricalcolo incrementale = completo", okInc == N);
    T("G13 codifica identica", okCodec == N);
    T("G14 residui dei blocchi entro la tolleranza", okSolve == N);
}

// ---------------------------------------------------------------- risolutore come step::Task
void solverTask() {
    State s;
    std::vector<Id> pts;
    for (int i = 0; i < 12; ++i) pts.push_back(addPoint(s, std::cos(i * 0.5) * 3, std::sin(i * 0.5) * 3));
    std::vector<Id> segs;
    for (int i = 0; i < 11; ++i) segs.push_back(addLine(s, Kind::Segment, pts[static_cast<std::size_t>(i)], pts[static_cast<std::size_t>(i + 1)]));
    for (int i = 0; i < 10; ++i) s.locks.push_back(Lock{MSpec{MType::Distance, {segs[static_cast<std::size_t>(i)]}}, {2.0, 0}, 0});
    const State before = s;
    SolveTask t(s, {});
    step::Budget small(10, 0, nullptr);
    T("A12 risolutore a passi: un passo non basta", !t.step(small) && t.variables() == 22);
    t.cancelled();
    T("A12 AC sul risolutore: stato iniziale", s == before);
    SolveTask t2(s, {});
    step::Budget all;
    T("A12 risolutore completo", t2.step(all) && t2.ok());
    TN("A12 vincolo soddisfatto", val(s, MType::Distance, {segs[3]}), 2, 1e-9);
}
} // namespace

int main() {
    // un'eccezione inattesa in un gruppo e' un errore del gruppo (gli altri girano comunque)
    const std::pair<const char*, void (*)()> groups[] = {{"draw", draw}, {"constructs", constructs}, {"transforms", transforms}, {"measures", measures},
                                                         {"locks", locks}, {"edits", edits}, {"animations", animations}, {"persistence", persistence},
                                                         {"properties", properties}, {"solverTask", solverTask}};
    for (const auto& g : groups) {
        try { g.second(); } catch (const std::exception& e) { ::testutil::report(false, g.first, __FILE__, __LINE__, std::string("eccezione: ") + e.what()); }
    }
    return testutil::finish("geometry_test");
}
