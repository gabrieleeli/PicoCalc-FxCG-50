// CONIC GRAPHS (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Funzioni minime delle sezioni 5-50...5-51 del manuale fx-CG50 (descrizione
// del comportamento, nessun codice copiato): parabola, circonferenza, ellisse e
// iperbole in forma cartesiana scelte da un elenco di modelli, coefficienti
// valutati dal motore unico, grafico sulla V-Window di Plot con TRACE e
// G-Solve (vertice, fuoco, centro, raggio, asintoti, eccentricita').
#include "apps.hpp"
#include "conic.hpp"
#include "graph2d.hpp"
#include "mode_codec.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Act : int { ActSelect = 0, ActDraw, ActTrace, ActSolve, ActOk, ActOpen, ActSaveAs };
const FKeyPage kFilePage{{{"OPEN", ActOpen}, {"SAVE\xC2\xB7" "AS", ActSaveAs}, {}, {}, {}, {}}};
const FKeyMenu kFileMenu{&kFilePage, 1, nullptr};
const FKeyPage kListPage{{{"SELECT", ActSelect}, {}, {}, {}, {"FILE", -1, &kFileMenu}, {}}};
const FKeyMenu kListMenu{&kListPage, 1, nullptr};
const FKeyPage kCoefPage{{{}, {}, {}, {}, {}, {"DRAW", ActDraw}}};
const FKeyMenu kCoefMenu{&kCoefPage, 1, nullptr};
const FKeyPage kGraphPage{{{"TRACE", ActTrace}, {}, {}, {}, {"G-SOLVE", ActSolve}, {}}};
const FKeyMenu kGraphMenu{&kGraphPage, 1, nullptr};
enum Screen : int { List, Coef, Plot };
constexpr int MenuSolve = 1;
const conic::Solve kSolves[] = {conic::Solve::Vertex, conic::Solve::Focus, conic::Solve::Center, conic::Solve::Radius,
                                conic::Solve::Asymptote, conic::Solve::Eccentricity};
} // namespace

struct ConicGraphsApp::View {
    ModeShell shell;
    TypedFileMenu files{files::FileType::Conic};
    SelectList models;
    Form coef;
    Screen screen{List};
    graph::Viewport viewport;
    std::vector<std::unique_ptr<graph2d::ParametricSource>> sources;
    std::vector<std::unique_ptr<graph2d::Plotter>> plots;
    std::unique_ptr<graph2d::TraceCursor> trace;
    std::size_t traceBranch{0};
    bool tracing{false};
    conic::Result result;
    bool showResult{false};
};

ConicGraphsApp::ConicGraphsApp() = default;
ConicGraphsApp::~ConicGraphsApp() = default;

namespace {
void fillCoef(Context& ctx, ConicGraphsApp::View& v) {
    const conic::State& s = ctx.ws.conic;
    v.coef.fields.clear();
    for (const char* n = conic::coefficientNames(s.model); *n; ++n)
        v.coef.fields.push_back(numberField(std::string(1, *n), formNumber(s.coef[static_cast<std::size_t>(conic::coefIndex(*n))])));
}
void applyCoef(Context& ctx, ConicGraphsApp::View& v) {
    conic::State s = ctx.ws.conic;
    std::size_t k = 0;
    for (const char* n = conic::coefficientNames(s.model); *n; ++n, ++k) s.coef[static_cast<std::size_t>(conic::coefIndex(*n))] = v.coef.number(ctx, k);
    const std::string e = conic::validate(s);
    if (!e.empty()) throw MathError(e);
    ctx.ws.conic = s;
    ctx.markDirty();
}
void buildGraph(Context& ctx, ConicGraphsApp::View& v) {
    const ViewWindow& w = ctx.ws.view;
    v.viewport = graph2d::layout(graph2d::Layout::Full, w, w).main;
    v.sources.clear(); v.plots.clear();
    const conic::State s = ctx.ws.conic;
    const auto bs = conic::branches(s, w.xmin, w.xmax, w.ymin, w.ymax);
    for (std::size_t b = 0; b < bs.size(); ++b) {
        v.sources.push_back(std::make_unique<graph2d::ParametricSource>(
            [s, b](double t, double& x, double& y) { return conic::point(s, b, t, x, y); }, bs[b].t0, bs[b].t1, bs[b].dt));
        v.plots.push_back(std::make_unique<graph2d::Plotter>(*v.sources.back(), v.viewport));
    }
    v.trace.reset();
    v.tracing = false;
    v.showResult = false;
    v.screen = Plot;
    v.shell.fkeys.set(&kGraphMenu);
}
} // namespace

void ConicGraphsApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    for (std::size_t i = 0; i < conic::ModelCount; ++i) v_->models.items.push_back(conic::modelFormula(static_cast<conic::Model>(i)));
    v_->models.selected = static_cast<int>(ctx.ws.conic.model);
    v_->shell.fkeys.set(&kListMenu);
    // Striscia di eActivity: Conics Plot apre il grafico (Conics Editor l'elenco).
    if (ctx.inStrip(AppId::ConicGraphs) && ctx.strip().entry == 4 && conic::validate(ctx.ws.conic).empty()) buildGraph(ctx, *v_);
}
void ConicGraphsApp::leave(Context&) { v_.reset(); }
std::string ConicGraphsApp::screenName() const {
    if (!v_) return "closed";
    return v_->screen == List ? "list" : v_->screen == Coef ? "coef" : v_->tracing ? "trace" : "graph";
}
bool ConicGraphsApp::tick(Context& ctx) {
    (void)ctx;
    if (!v_ || v_->screen != Plot) return false;
    bool work = false;
    for (auto& p : v_->plots) {
        if (p->finished()) continue;
        step::Budget b(400, 0, nullptr);
        (void)p->step(b);
        work = true;
        break;
    }
    return work;
}

void ConicGraphsApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    if (v.files.active()) {
        if (v.files.key(ev) != 1) return;
        if (v.files.purpose() == TypedFileMenu::Purpose::Open) {
            const auto data = v.files.load(ctx, v.files.file());
            conic::State s;
            if (!conic::decode(s, data.data(), data.size())) throw MathError("Data ERROR: file Conic Graphs danneggiato");
            ctx.ws.conic = s;
            v.models.selected = static_cast<int>(s.model);
            ctx.markDirty();
        } else {
            std::vector<std::uint8_t> out;
            conic::encode(ctx.ws.conic, out);
            v.files.save(ctx, v.files.file(), out);
            ctx.toast("Salvato " + v.files.file());
        }
        return;
    }
    const bool modal = v.shell.modalKey(ev, [](int, bool) {}, [](int) {},
        [&](int tag, int choice) {
            if (tag != MenuSolve || choice < 0) return;
            v.result = conic::solve(ctx.ws.conic, kSolves[choice]);
            v.showResult = true;
            v.tracing = false;
        });
    if (modal) return;
    switch (v.screen) {
    case List: {
        const int a = v.shell.fkeys.key(ev, ctx.shift());
        if (a == FKeyBar::Handled) return;
        if (a == ActOpen) { v.shell.fkeys.set(&kListMenu); v.files.openList(ctx); return; }
        if (a == ActSaveAs) { v.shell.fkeys.set(&kListMenu); v.files.openSaveAs(ctx, ""); return; }
        if (a == ActSelect || ev.is(Key::Exe)) {
            const auto m = static_cast<conic::Model>(v.models.selected);
            if (ctx.ws.conic.model != m) { ctx.ws.conic.model = m; ctx.markDirty(); }
            fillCoef(ctx, v);
            v.screen = Coef;
            v.shell.fkeys.set(&kCoefMenu);
            return;
        }
        if (ev.is(Key::Exit)) { if (v.shell.fkeys.inSubmenu()) v.shell.fkeys.back(); else ctx.home(); return; }
        (void)v.models.key(ev);
        return;
    }
    case Coef: {
        const bool editing = v.coef.editing();
        const int a = editing ? FKeyBar::None : v.shell.fkeys.key(ev, false);
        if (a == ActDraw) { applyCoef(ctx, v); buildGraph(ctx, v); return; }
        if (!editing && ev.is(Key::Exit)) { v.screen = List; v.shell.fkeys.set(&kListMenu); return; }
        if (v.coef.key(ctx, ev) && editing && !v.coef.editing()) applyCoef(ctx, v);
        return;
    }
    case Plot: {
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActTrace && !v.sources.empty()) {
            v.tracing = !v.tracing;
            v.showResult = false;
            v.traceBranch = 0;
            v.trace = std::make_unique<graph2d::TraceCursor>(*v.sources[0], v.viewport);
            const double mid = (v.sources[0]->start() + v.sources[0]->end()) / 2;
            v.trace->jump(mid);
            return;
        }
        if (a == ActSolve) {
            std::vector<std::string> items;
            for (conic::Solve s : kSolves) items.push_back(conic::solveName(s));
            v.shell.showMenu("G-SOLVE", items, MenuSolve);
            return;
        }
        if (v.tracing && v.trace) {
            if (ev.is(Key::Left)) { for (int k = 0; k < 4; ++k) v.trace->left(); return; }
            if (ev.is(Key::Right)) { for (int k = 0; k < 4; ++k) v.trace->right(); return; }
            if ((ev.is(Key::Up) || ev.is(Key::Down)) && v.sources.size() > 1) {
                v.traceBranch = (v.traceBranch + 1) % v.sources.size();
                const double t = v.trace->parameter();
                v.trace = std::make_unique<graph2d::TraceCursor>(*v.sources[v.traceBranch], v.viewport);
                v.trace->jump(t);
                return;
            }
        }
        if (ev.is(Key::Exit) || ev.is(Key::AC)) {
            if (v.tracing || v.showResult) { v.tracing = false; v.showResult = false; return; }
            v.screen = Coef; v.shell.fkeys.set(&kCoefMenu);
        }
        return;
    }
    }
}

void ConicGraphsApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    ui::clear(c);
    ctx.drawStatus(c, "CONIC GRAPHS");
    const int top = ui::ContentTop;
    switch (v.screen) {
    case List:
        drawModeHeader(c, "Conic Graphs: scegli il modello");
        v.models.draw(c, gfx::Rect{0, top + 18, ui::ScreenW, ui::ContentBottom - top - 18}, 22);
        break;
    case Coef:
        drawModeHeader(c, conic::modelFormula(ctx.ws.conic.model));
        v.coef.draw(c, top + 18, ui::ContentBottom);
        break;
    case Plot: {
        graph2d::drawAxes(c, v.viewport, Muted, Grid, false);
        for (const auto& p : v.plots) p->draw(c, Graph[0]);
        ui::text(c, 4, top + 2, conic::modelFormula(ctx.ws.conic.model), Ink, font::Face::Small);
        if (v.tracing && v.trace) {
            double x, y;
            if (v.trace->point(x, y)) {
                const int sx = static_cast<int>(std::lround(v.viewport.screenX(x))), sy = static_cast<int>(std::lround(v.viewport.screenY(y)));
                c.hLine(sx - 5, sy, 11, Error); c.vLine(sx, sy - 5, 11, Error);
                const std::string t = "X=" + formNumber(x) + "  Y=" + formNumber(y);
                c.rect(0, ui::ContentBottom - 18, ui::ScreenW, 18, Bg, true);
                ui::text(c, 4, ui::ContentBottom - 16, t.c_str(), Ink, font::Face::Small);
            }
        }
        if (v.showResult) {
            for (const auto& p : v.result.points) {
                const int sx = static_cast<int>(std::lround(v.viewport.screenX(p[0]))), sy = static_cast<int>(std::lround(v.viewport.screenY(p[1])));
                c.circle(sx, sy, 3, Error, true);
            }
            for (const auto& l : v.result.lines) {
                const ViewWindow& w = v.viewport.view();
                double x0 = v.viewport.screenX(w.xmin), y0 = v.viewport.screenY(l[0] * w.xmin + l[1]);
                double x1 = v.viewport.screenX(w.xmax), y1 = v.viewport.screenY(l[0] * w.xmax + l[1]);
                if (v.viewport.clip(x0, y0, x1, y1)) c.line(static_cast<int>(x0), static_cast<int>(y0), static_cast<int>(x1), static_cast<int>(y1), Graph[1]);
            }
            c.rect(0, ui::ContentBottom - 18, ui::ScreenW, 18, Bg, true);
            ui::textFit(c, 4, ui::ContentBottom - 16, v.result.text, ui::ScreenW - 8, Ink, font::Face::Small);
        }
        break;
    }
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.files.draw(c);
}
} // namespace fx::app
