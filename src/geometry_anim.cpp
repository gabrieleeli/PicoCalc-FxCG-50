// Nucleo di Geometry (v1.19.0): animazioni (Add/Replace Animation, Trace,
// Edit Animation, Go once/repeat) e tabella delle misure (Add/Display Table).
// L'animazione e' un step::Task: un fotogramma per unita', AC la interrompe;
// alla fine (o con AC) i punti tornano dov'erano (ANALISI_FONTI §4.5, Q13).
#include "geometry.hpp"
#include "geometry_internal.hpp"
#include <algorithm>

namespace fx::geo {
using namespace detail;

void addAnimation(State& s, Id point, Id path, bool replace) {
    const Object* p = find(s, point);
    const Object* c = find(s, path);
    if (!p || !c || p->kind != Kind::Point || !isCurve(c->kind)) fail("Argument ERROR: selezionare un punto e un segmento, una retta, una circonferenza, un arco o una funzione");
    if (!(p->def == Def::Free || (p->def == Def::On && p->parents[0] == path))) fail("Argument ERROR: il punto deve essere libero o sul percorso");
    if (isAncestor(s, point, path)) fail("Argument ERROR: il punto definisce il percorso");
    if (replace) s.animations.clear();
    Animation a;
    a.point = point;
    a.path = path;
    if (c->kind == Kind::Function) { a.t0 = -5; a.t1 = 5; }
    auto it = std::find_if(s.animations.begin(), s.animations.end(), [&](const Animation& x) { return x.point == point; });
    if (it != s.animations.end()) { *it = a; return; }
    if (s.animations.size() >= MaxAnimations) fail("Memory ERROR: oltre 16 animazioni");
    s.animations.push_back(a);
}
void setTrace(State& s, const std::vector<Id>& points) {
    std::vector<Id> t;
    for (Id id : points) { const Object* o = find(s, id); if (o && o->kind == Kind::Point && std::find(t.begin(), t.end(), id) == t.end()) t.push_back(id); }
    std::sort(t.begin(), t.end());
    s.traced = std::move(t);
}
bool pathPoint(const State& s, const Animation& a, double t, double& x, double& y) {
    const Object* c = find(s, a.path);
    if (!c || !c->g.ok) return false;
    return curvePoint(s, a.path, c->kind == Kind::Circle ? TwoPi * t : t, x, y);
}
void addTableColumn(State& s, const MSpec& m) {
    if (m.ops.empty()) fail("Argument ERROR: scegliere una misura nella casella");
    const Measurement r = measure(s, m);
    if (m.type == MType::Label || m.type == MType::Equation || r.yesNo) fail("Argument ERROR: misura non numerica");
    const std::size_t add = (m.type == MType::Coordinates || m.type == MType::Components) ? 2 : 1;
    if (s.columns.size() + add > MaxColumns) fail("Memory ERROR: al piu' 26 colonne");
    std::string base;
    for (Id id : m.ops) base += shortName(s, id);
    for (std::size_t c = 0; c < add; ++c) {
        TableColumn col;
        col.m = m;
        col.comp = static_cast<std::uint8_t>(c);
        std::string prefix = add == 2 ? (c == 0 ? "x " : "y ") : "";
        col.name = prefix + mtypeName(m.type);
        if (!base.empty()) col.name = prefix + base + " " + mtypeName(m.type);
        if (col.name.size() > MaxLabel) col.name.resize(MaxLabel);
        s.columns.push_back(std::move(col));
    }
    s.table.clear();
    s.tableRows = 0;
}

AnimationTask::AnimationTask(State& s, bool repeat, bool recordTable) : s_(s), repeat_(repeat), record_(recordTable) {
    if (s.animations.empty()) fail("Argument ERROR: nessuna animazione (Add Animation)");
    for (const auto& a : s.animations) {
        frames_ = std::max<std::size_t>(frames_, a.steps);
        if (indexOf(s, a.point) < 0) fail("Argument ERROR: animazione senza punto");
    }
    // Tutti gli scalari: con dei blocchi il risolutore muove anche altri punti a ogni
    // fotogramma, e alla fine tutto deve tornare com'era (trovato da fuzz_geometry).
    saved_.reserve(s.objects.size());
    for (std::size_t i = 0; i < s.objects.size(); ++i) saved_.emplace_back(i, s.objects[i].vars);
    cycles_ = 1;
    if (repeat_) {
        std::size_t times = 0;
        bool forever = false;
        for (const auto& a : s.animations) { if (a.times == 0) forever = true; times = std::max<std::size_t>(times, a.times); }
        cycles_ = forever ? 0 : times;
    }
    traces_.assign(s.traced.size(), {});
    if (record_) {
        s.tableRows = static_cast<std::uint16_t>(std::min<std::size_t>(frames_, MaxRows));
        s.table.assign(static_cast<std::size_t>(s.tableRows) * s.columns.size(), 0.0);
    }
}
void AnimationTask::apply(std::size_t frame) {
    std::vector<Id> changed;
    for (const auto& a : s_.animations) {
        const int i = indexOf(s_, a.point);
        if (i < 0) continue;
        const std::size_t f = std::min<std::size_t>(frame, a.steps - 1u);
        const double t = a.t0 + (a.t1 - a.t0) * static_cast<double>(f) / static_cast<double>(a.steps - 1u);
        double x, y;
        if (!pathPoint(s_, a, t, x, y)) continue;
        Object& p = s_.objects[static_cast<std::size_t>(i)];
        if (p.def == Def::Free) p.vars = {x, y};
        else {
            double tt, px, py;
            if (!project(s_, p.parents[0], x, y, tt, px, py)) continue;
            p.vars[0] = tt;
        }
        changed.push_back(p.id);
    }
    recompute(s_, changed);
    if (!s_.locks.empty()) solve(s_, changed);
}
bool AnimationTask::step(step::Budget& budget) {
    if (restored_) return true;
    for (;;) {
        apply(frame_);
        for (std::size_t k = 0; k < s_.traced.size(); ++k)
            if (const Shape* g = shapeOf(s_, s_.traced[k])) { traces_[k].push_back(g->a[0]); traces_[k].push_back(g->a[1]); }
        if (record_ && cycle_ == 0 && frame_ < s_.tableRows) {
            const std::size_t cols = s_.columns.size();
            for (std::size_t c = 0; c < cols; ++c) {
                const Measurement r = measure(s_, s_.columns[c].m);
                const double v = r.ok && r.count > s_.columns[c].comp ? r.v[s_.columns[c].comp] : NoValue;
                s_.table[frame_ * cols + c] = v;
            }
        }
        if (++frame_ >= frames_) {
            frame_ = 0;
            ++cycle_;
            if (cycles_ != 0 && cycle_ >= cycles_) { restore(); return true; }
            // le tracce di un nuovo giro ripartono
            for (auto& t : traces_) t.clear();
        }
        if (!budget.consume(1)) return false;           // un fotogramma = un'unita'
    }
}
void AnimationTask::restore() {
    if (restored_) return;
    restored_ = true;
    for (const auto& sv : saved_) if (sv.first < s_.objects.size()) s_.objects[sv.first].vars = sv.second;
    recompute(s_);
}
void AnimationTask::cancelled() { restore(); }
} // namespace fx::geo

namespace fx::geo {
using namespace detail;
void setAnimation(State& s, std::size_t index, int steps, double t0, double t1, int times) {
    if (index >= s.animations.size()) fail("Argument ERROR: animazione inesistente");
    if (steps < MinSteps || steps > MaxSteps) fail("Argument ERROR: Steps da 2 a 100");
    const Object* path = find(s, s.animations[index].path);
    const double lim = path && path->kind == Kind::Function ? 1e6 : MaxT;
    if (!(t0 >= -lim && t0 <= lim) || !(t1 >= -lim && t1 <= lim)) fail(lim == MaxT ? "Argument ERROR: t0 e t1 da -10 a 10" : "Argument ERROR: t0 e t1 fuori intervallo");
    if (times < 0 || times > 9999) fail("Argument ERROR: Times da 0 a 9999");
    Animation& a = s.animations[index];
    a.steps = static_cast<std::uint8_t>(steps);
    a.t0 = t0;
    a.t1 = t1;
    a.times = static_cast<std::uint16_t>(times);
}
void removeAnimation(State& s, std::size_t index) {
    if (index < s.animations.size()) s.animations.erase(s.animations.begin() + static_cast<std::ptrdiff_t>(index));
}
void removeColumn(State& s, std::size_t column) {
    const std::size_t cols = s.columns.size();
    if (column >= cols) return;
    std::vector<double> nt;
    for (std::size_t r = 0; r < s.tableRows; ++r)
        for (std::size_t c = 0; c < cols; ++c) if (c != column) nt.push_back(s.table[r * cols + c]);
    s.columns.erase(s.columns.begin() + static_cast<std::ptrdiff_t>(column));
    s.table = std::move(nt);
    if (s.columns.empty()) { s.table.clear(); s.tableRows = 0; }
}
void clearTable(State& s) { s.columns.clear(); s.table.clear(); s.tableRows = 0; }
} // namespace fx::geo
