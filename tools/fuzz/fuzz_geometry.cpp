// v1.19.0 (solo Pimoroni Pico Plus 2 W) — Fuzzing di GEOMETRY.
//  * primo byte % 3 == 0: i dati come sezione 0x0110 (formato 1 o 2);
//  * primo byte % 3 == 1: i dati come file .pcgeo;
//    ogni dato accettato si ricodifica e si rilegge identico, e le misure dello
//    stato letto non vanno in crash;
//  * primo byte % 3 == 2: una SEQUENZA DI COMANDI decisa dai byte (disegna,
//    costruisci, trasforma, sposta, scrivi e blocca misure, cancella, Undo,
//    Clr Constraint, anima a passi, tabella, codifica). Dopo ogni comando le
//    invarianti: ordine topologico, definizioni coerenti, geometria finita,
//    ricalcolo incrementale = completo, andata e ritorno identica, residui dei
//    blocchi entro la tolleranza dopo un valore accettato.
#include "geometry.hpp"
#include "step_task.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>
using namespace fx;
using namespace fx::geo;
namespace {
[[noreturn]] void bad(const char* what) { std::fprintf(stderr, "INVARIANT: %s\n", what); std::abort(); }
void roundTrip(const State& s) {
    std::vector<std::uint8_t> b;
    encode(s, b);
    State t;
    if (!decode(t, b.data(), b.size()) || !(t == s)) bad("roundtrip della sezione");
    const auto f = encodeFile(s);
    State u;
    u.file = s.file;
    if (!decodeFile(f.data(), f.size(), u) || !(u == s)) bad("roundtrip del file");
}
void invariants(const State& s) {
    Id prev = NoId;
    for (const auto& o : s.objects) {
        if (o.id <= prev) bad("ordine degli id");
        prev = o.id;
        for (Id p : o.parents) if (p >= o.id || indexOf(s, p) < 0) bad("genitore");
        if (!validate(s, o).empty()) bad("definizione");
        if (o.g.ok) for (double v : o.g.a) if (!std::isfinite(v)) bad("geometria non finita");
    }
    if (s.nextId <= prev) bad("nextId");
    State full = s;
    recompute(full);
    for (std::size_t i = 0; i < s.objects.size(); ++i) {
        const Shape& a = s.objects[i].g;
        const Shape& b = full.objects[i].g;
        if (a.ok != b.ok) bad("incrementale != completo (definito)");
        if (a.ok) for (int k = 0; k < 6; ++k) if (!(a.a[k] == b.a[k])) bad("incrementale != completo (valori)");
    }
}
struct Bytes {
    const std::uint8_t* p;
    std::size_t n, i{0};
    bool more() const { return i < n; }
    std::uint8_t u8() { return i < n ? p[i++] : 0; }
    double coord() { return (static_cast<int>(u8()) - 128) / 8.0; }
    Id pick(const State& s) { return s.objects.empty() ? NoId : s.objects[u8() % s.objects.size()].id; }
    std::vector<Id> sel(const State& s) { std::vector<Id> v; const int k = 1 + u8() % 3; for (int j = 0; j < k; ++j) { const Id id = pick(s); if (id) v.push_back(id); } return v; }
};
const char* const kFunctions[] = {"x^2", "sin(x)", "1/x", "sqrt(x)", "x^3-2x", "ln(x)", "abs(x)-1", "2"};
// FXAI_GEO_TRACE=1: stampa i comandi e il blocco che non torna (per capire un crash).
const bool kTrace = std::getenv("FXAI_GEO_TRACE") != nullptr;
void command(State& s, Bytes& b, std::vector<std::uint8_t>& undo) {
    const std::uint8_t op = b.u8() % 26;
    if (kTrace) std::fprintf(stderr, "op %d (oggetti %zu, blocchi %zu)\n", op, s.objects.size(), s.locks.size());
    bool solved = false;
    switch (op) {
    case 0: addPoint(s, b.coord(), b.coord()); break;
    case 1: { const Id c = b.pick(s); addPointOn(s, c, b.coord(), b.coord()); break; }
    case 2: { const Kind k = static_cast<Kind>(1 + b.u8() % 4); const Id a = b.pick(s), c = b.pick(s); addLine(s, k, a, c); break; }
    case 3: { const Id a = b.pick(s), c = b.pick(s); addCircle(s, a, c); break; }
    case 4: { const Id a = b.pick(s), c = b.pick(s); addArc(s, a, c, b.coord(), b.coord()); break; }
    case 5: { const Id a = b.pick(s), c = b.pick(s); if (b.u8() & 1) addSemicircle(s, a, c); else addArc3(s, a, c, b.pick(s)); break; }
    case 6: { std::vector<Id> v; const int k = 2 + b.u8() % 6; for (int j = 0; j < k; ++j) v.push_back(b.pick(s)); addPolygon(s, v, b.u8() & 1); break; }
    case 7: { const Spec sp = static_cast<Spec>(b.u8() % 5); addSpec(s, sp, Box{b.coord(), b.coord(), b.coord(), b.coord()}, Box{-6, -5, 6, 5}, 3 + b.u8() % 10); break; }
    case 8: addFunction(s, kFunctions[b.u8() % 8]); break;
    case 9: { const Construct c = static_cast<Construct>(b.u8() % 8); construct(s, c, b.sel(s), b.coord(), b.coord()); break; }
    case 10: { const TType t = static_cast<TType>(b.u8() % 6); const auto sel = b.sel(s); transform(s, t, sel, b.pick(s), b.coord() * 10, b.coord() / 4); break; }
    case 11: remove(s, {b.pick(s)}); break;
    case 12: { const Id p = b.pick(s); dragPoint(s, p, b.coord(), b.coord()); solved = true; break; }
    case 13: { const auto sel = b.sel(s); translate(s, sel, b.coord() / 4, b.coord() / 4); solved = true; break; }
    case 14: case 15: {
        const auto sel = b.sel(s);
        const auto specs = available(s, sel);
        if (specs.empty()) break;
        const MSpec m = specs[b.u8() % specs.size()];
        if (kTrace) std::fprintf(stderr, "  misura tipo %d su %zu operandi\n", static_cast<int>(m.type), m.ops.size());
        if (op == 14) setValue(s, m, std::abs(b.coord()) + 0.5, b.coord(), "L"); else lock(s, m);
        solved = true;
        break;
    }
    case 16: if (!s.locks.empty()) unlock(s, s.locks[b.u8() % s.locks.size()].m); break;
    case 17: clearConstraints(s); break;
    case 18: { const Id p = b.pick(s), c = b.pick(s); addAnimation(s, p, c, b.u8() & 1); break; }
    case 19: {
        if (s.animations.empty()) break;
        setAnimation(s, b.u8() % s.animations.size(), 2 + b.u8() % 20, b.coord() / 16, b.coord() / 16, b.u8() % 3);
        break;
    }
    case 20: {
        if (s.animations.empty()) break;
        const State before = s;
        AnimationTask t(s, b.u8() & 1, !s.columns.empty());
        step::Budget few(1 + b.u8() % 8, 0, nullptr);
        const bool done = t.step(few);
        if (!done) t.cancelled();
        // i punti animati tornano dov'erano (solo le celle della tabella cambiano)
        for (std::size_t i = 0; i < s.objects.size(); ++i) if (s.objects[i].vars != before.objects[i].vars) bad("animazione: punti non rimessi");
        break;
    }
    case 21: { const auto sel = b.sel(s); const auto specs = available(s, sel); if (!specs.empty()) addTableColumn(s, specs[b.u8() % specs.size()]); break; }
    case 22: { undo.clear(); encode(s, undo); break; }
    case 23: { if (undo.empty()) break; State t; if (!decode(t, undo.data(), undo.size())) bad("istantanea di Undo"); s = std::move(t); break; }
    case 24: { const Id id = b.pick(s); if (Object* o = find(s, id)) { o->hidden = !o->hidden; o->style.area = b.u8() % 8 == 7 ? NoArea : b.u8() % 7; } break; }
    default: { const auto sel = b.sel(s); for (const auto& m : available(s, sel)) { (void)measure(s, m); (void)measureValueText(s, m, b.u8() % 21); } (void)fit(s, 320, 266); break; }
    }
    if (solved)
        for (const auto& l : s.locks)
            for (std::size_t i = 0; i < residualCount(l); ++i) {
                const double r = residual(s, l, i);
                if (!(std::abs(r) <= 1e-8 * std::max({1.0, std::abs(l.v[0]), std::abs(l.v[1])}))) {
                    if (kTrace) std::fprintf(stderr, "blocco tipo %d su %zu operandi: residuo %g, valore %g\n", static_cast<int>(l.m.type), l.m.ops.size(), r, l.v[0]);
                    bad("residuo di un blocco dopo il risolutore");
                }
            }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 1) return 0;
    const std::uint8_t* p = d + 1;
    const std::size_t m = n - 1;
    try {
        switch (d[0] % 3) {
        case 0: {
            State s;
            if (!decode(s, p, m)) return 0;
            invariants(s);
            roundTrip(s);
            for (const auto& o : s.objects) for (const auto& sp : available(s, {o.id})) (void)measure(s, sp);
            return 0;
        }
        case 1: {
            State s;
            if (!decodeFile(p, m, s)) return 0;
            invariants(s);
            roundTrip(s);
            return 0;
        }
        default: {
            State s;
            Bytes b{p, m};
            std::vector<std::uint8_t> undo;
            int ops = 0;
            while (b.more() && ops++ < 48) {
                try { command(s, b, undo); }
                catch (const std::bad_alloc&) { throw; }
                catch (const std::exception&) {}          // errori della fx-CG50: lo stato resta valido
                invariants(s);
            }
            roundTrip(s);
            return 0;
        }
        }
    } catch (const std::bad_alloc&) {
        return 0;
    }
}
