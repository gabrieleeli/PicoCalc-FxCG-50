// RECURSION (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Funzioni minime delle sezioni 5-45...5-49 del manuale fx-CG50 (descrizione
// del comportamento, nessun codice copiato): tipi an+1 = f(n, an) e
// an+2 = f(n, an, an+1), termini iniziali a0 e a1, tabella per n da Start a End
// con Σan (calcolata a passi, AC = Break), grafico dei termini e diagramma a
// ragnatela per an+1, sulla V-Window di Graph. La formula an+1 e Start/End/a0/a1
// sono le variabili di modo del workspace (VARS › RECURSION di Run-Matrix).
#include "apps.hpp"
#include "graph2d.hpp"
#include "recursion.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Act : int { ActType = 0, ActEdit, ActSet, ActTable, ActGraph, ActWeb, ActOk, ActInsN, ActInsAn, ActInsAn1, ActOpen, ActSaveAs };
const FKeyPage kFilePage{{{"OPEN", ActOpen}, {"SAVE\xC2\xB7" "AS", ActSaveAs}, {}, {}, {}, {}}};
const FKeyMenu kFileMenu{&kFilePage, 1, nullptr};
const FKeyPage kMainPage{{{"TYPE", ActType}, {"EDIT", ActEdit}, {"FILE", -1, &kFileMenu}, {}, {"SET", ActSet}, {"TABLE", ActTable}}};
const FKeyMenu kMainMenu{&kMainPage, 1, nullptr};
const FKeyPage kAskPage{{{"n", ActInsN}, {"an", ActInsAn}, {"an+1", ActInsAn1}, {}, {}, {}}};
const FKeyMenu kAskMenu{&kAskPage, 1, nullptr};
const FKeyPage kSetPage{{{}, {}, {}, {}, {}, {"OK", ActOk}}};
const FKeyMenu kSetMenu{&kSetPage, 1, nullptr};
const FKeyPage kTablePage{{{}, {}, {}, {}, {"G-PLT", ActGraph}, {"WEB", ActWeb}}};
const FKeyMenu kTableMenu{&kTablePage, 1, nullptr};
enum Screen : int { Main, Set, Table, Plot, Web };
constexpr int AskFormula = 1;
} // namespace

struct RecursionApp::View {
    ModeShell shell;
    TypedFileMenu files{files::FileType::Recursion};
    Form set;
    Screen screen{Main};
    step::Runner runner;
    std::unique_ptr<rec::Table> task;
    std::vector<rec::Row> rows;
    int row{0}, first{0};
    std::unique_ptr<graph2d::PointsSource> points;
    std::unique_ptr<graph2d::Plotter> plot;
    std::unique_ptr<graph2d::FunctionSource> fn;
    std::unique_ptr<graph2d::Plotter> fnPlot, diag;
    std::unique_ptr<graph2d::FunctionSource> identity;
    graph::Viewport viewport;
};

RecursionApp::RecursionApp() = default;
RecursionApp::~RecursionApp() = default;

namespace {
const std::string& formula(Context& ctx) {
    return ctx.ws.recursion.type == rec::Type::An1 ? ctx.ws.modeVars.recFormula : ctx.ws.recursion.an2;
}
rec::Setup setupOf(Context& ctx) {
    const ModeVariables& m = ctx.ws.modeVars;
    rec::Setup s;
    s.type = ctx.ws.recursion.type;
    s.formula = formula(ctx);
    s.start = static_cast<int>(m.recStart);
    s.end = static_cast<int>(m.recEnd);
    s.a0 = m.recInitial[0];
    s.a1 = m.recInitial[1];
    return s;
}
void fillSet(Context& ctx, RecursionApp::View& v) {
    const ModeVariables& m = ctx.ws.modeVars;
    v.set.fields = {numberField("Start", formNumber(m.recStart)), numberField("End", formNumber(m.recEnd)),
                    numberField("a0", formNumber(m.recInitial[0])), numberField("a1", formNumber(m.recInitial[1])),
                    choiceField("\xCE\xA3" "an", {"On", "Off"}, ctx.ws.recursion.sigma ? 0 : 1)};
}
void applySet(Context& ctx, RecursionApp::View& v) {
    const double a = v.set.number(ctx, 0), b = v.set.number(ctx, 1);
    if (a != std::floor(a) || b != std::floor(b) || a < 0 || b > rec::MaxN || a > b) throw MathError("Range ERROR: 0 <= Start <= End <= 999 (interi)");
    ModeVariables& m = ctx.ws.modeVars;
    m.recStart = a; m.recEnd = b;
    m.recInitial[0] = v.set.number(ctx, 2);
    m.recInitial[1] = v.set.number(ctx, 3);
    ctx.ws.recursion.sigma = v.set.fields[4].choice == 0;
    ctx.markDirty();
}
// Testo mostrato di una formula (nomi legati come la calcolatrice: n, an, an+1).
std::string shown(const std::string& f) {
    std::string out;
    for (std::size_t i = 0; i < f.size();) {
        if (f.compare(i, 7, "rec_an1") == 0) { out += "an+1"; i += 7; }
        else if (f.compare(i, 6, "rec_an") == 0) { out += "an"; i += 6; }
        else if (f.compare(i, 5, "rec_n") == 0) { out += "n"; i += 5; }
        else out += f[i++];
    }
    return out;
}
void startTable(Context& ctx, RecursionApp::View& v) {
    v.rows.clear();
    v.runner.reset();
    v.task = std::make_unique<rec::Table>(setupOf(ctx), ctx.ws.evalContext());
    v.runner.start(*v.task, step::Runner::Config{200, 25});
    v.row = v.first = 0;
    v.screen = Table;
    v.shell.fkeys.set(&kTableMenu);
}
void buildGraph(Context& ctx, RecursionApp::View& v, bool web) {
    v.viewport = graph2d::layout(graph2d::Layout::Full, ctx.ws.view, ctx.ws.view).main;
    std::vector<double> xs, ys;
    if (web) {
        if (ctx.ws.recursion.type != rec::Type::An1) throw MathError("Argument ERROR: WEB solo per an+1");
        rec::cobweb(v.rows, xs, ys);
        // y = an+1(x) e y = x sotto la ragnatela.
        EvalContext ec = ctx.ws.evalContext();
        ec.complexMode = ComplexMode::Real;
        ec.bind("rec_n", 0);
        std::string f = ctx.ws.modeVars.recFormula;
        for (std::size_t p; (p = f.find("rec_an")) != std::string::npos;) f.replace(p, 6, "(x)");
        v.fn = std::make_unique<graph2d::FunctionSource>(f, ec);
        v.fnPlot = std::make_unique<graph2d::Plotter>(*v.fn, v.viewport);
        v.fnPlot->runAll();
        v.identity = std::make_unique<graph2d::FunctionSource>("x", ctx.ws.evalContext());
        v.diag = std::make_unique<graph2d::Plotter>(*v.identity, v.viewport);
        v.diag->runAll();
    } else {
        for (const rec::Row& r : v.rows) { xs.push_back(r.n); ys.push_back(r.a); }
        v.fnPlot.reset(); v.diag.reset();
    }
    v.points = std::make_unique<graph2d::PointsSource>(std::move(xs), std::move(ys), web);
    v.plot = std::make_unique<graph2d::Plotter>(*v.points, v.viewport);
    v.plot->runAll();
    v.screen = web ? Web : Plot;
}
} // namespace

void RecursionApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    v_->shell.fkeys.set(&kMainMenu);
    fillSet(ctx, *v_);
}
void RecursionApp::leave(Context&) { v_.reset(); }
std::string RecursionApp::screenName() const {
    if (!v_) return "closed";
    static const char* const n[] = {"main", "set", "table", "graph", "web"};
    return n[v_->screen];
}
bool RecursionApp::tick(Context& ctx) {
    if (!v_ || !v_->runner.running()) return false;
    v_->runner.tick([&ctx] { return ctx.now(); });
    if (!v_->runner.running() && v_->task) { v_->rows = v_->task->rows(); v_->task.reset(); }
    else if (v_->task) v_->rows = v_->task->rows();
    return true;
}

void RecursionApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    if (v.runner.running() && ev.is(Key::AC)) { v.runner.interrupt(); if (v.task) v.rows = v.task->rows(); v.task.reset(); throw MathError("Break: tabella interrotta"); }
    if (v.files.active()) {
        if (v.files.key(ev) != 1) return;
        if (v.files.purpose() == TypedFileMenu::Purpose::Open) {
            const auto data = v.files.load(ctx, v.files.file());
            rec::FileData f;
            if (!rec::decodeFile(data.data(), data.size(), f)) throw MathError("Data ERROR: file Recursion danneggiato");
            ctx.ws.recursion = f.state;
            ctx.ws.setRecursionFormula(f.an1);
            ModeVariables& m = ctx.ws.modeVars;
            m.recStart = f.start; m.recEnd = f.end; m.recInitial[0] = f.a0; m.recInitial[1] = f.a1;
            ctx.markDirty();
        } else {
            const ModeVariables& m = ctx.ws.modeVars;
            v.files.save(ctx, v.files.file(), rec::encodeFile({ctx.ws.recursion, m.recFormula, m.recStart, m.recEnd, m.recInitial[0], m.recInitial[1]}));
            ctx.toast("Salvato " + v.files.file());
        }
        return;
    }
    // Richiesta della formula: F1 n, F2 an, F3 an+1 (solo per an+2) inseriti come nomi legati.
    if (v.shell.ask.active()) {
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActInsN) { v.shell.ask.input().insertSymbol("rec_n"); return; }
        if (a == ActInsAn) { v.shell.ask.input().insertSymbol("rec_an"); return; }
        if (a == ActInsAn1 && ctx.ws.recursion.type == rec::Type::An2) { v.shell.ask.input().insertSymbol("rec_an1"); return; }
        if (a >= 0 || a == FKeyBar::Handled) return;
    }
    const bool modal = v.shell.modalKey(ev, [](int, bool) {},
        [&](int tag) {
            if (tag != AskFormula) return;
            const std::string f = v.shell.ask.text();
            if (!f.empty()) (void)parseExpression(f);              // Syntax ERROR: la formula resta com'era
            if (ctx.ws.recursion.type == rec::Type::An1) ctx.ws.setRecursionFormula(f);
            else ctx.ws.recursion.an2 = f;
            ctx.markDirty();
            v.shell.fkeys.set(&kMainMenu);
        },
        [](int, int) {});
    if (modal) { if (!v.shell.ask.active()) v.shell.fkeys.set(&kMainMenu); return; }
    if (ev.is(Key::Setup)) { fillSet(ctx, v); v.screen = Set; v.shell.fkeys.set(&kSetMenu); return; }
    switch (v.screen) {
    case Main: {
        const int a = v.shell.fkeys.key(ev, ctx.shift());
        if (a == FKeyBar::Handled) return;
        switch (a) {
        case ActType:
            ctx.ws.recursion.type = ctx.ws.recursion.type == rec::Type::An1 ? rec::Type::An2 : rec::Type::An1;
            ctx.markDirty();
            return;
        case ActEdit: {
            v.shell.ask.open(ctx.ws.recursion.type == rec::Type::An1 ? "an+1=" : "an+2=", AskFormula);
            if (!formula(ctx).empty()) v.shell.ask.input().setExpression(formula(ctx));
            v.shell.fkeys.set(&kAskMenu);
            return;
        }
        case ActSet: fillSet(ctx, v); v.screen = Set; v.shell.fkeys.set(&kSetMenu); return;
        case ActTable: startTable(ctx, v); return;
        case ActOpen: v.shell.fkeys.set(&kMainMenu); v.files.openList(ctx); return;
        case ActSaveAs: v.shell.fkeys.set(&kMainMenu); v.files.openSaveAs(ctx, ""); return;
        default: break;
        }
        if (ev.is(Key::Exe)) { v.shell.ask.open(ctx.ws.recursion.type == rec::Type::An1 ? "an+1=" : "an+2=", AskFormula);
                               if (!formula(ctx).empty()) v.shell.ask.input().setExpression(formula(ctx));
                               v.shell.fkeys.set(&kAskMenu); return; }
        if (ev.is(Key::Exit)) { if (v.shell.fkeys.inSubmenu()) v.shell.fkeys.back(); else ctx.home(); }
        return;
    }
    case Set: {
        const bool editing = v.set.editing();
        const int a = editing ? FKeyBar::None : v.shell.fkeys.key(ev, false);
        if (a == ActOk) { applySet(ctx, v); v.screen = Main; v.shell.fkeys.set(&kMainMenu); return; }
        if (!editing && ev.is(Key::Exit)) { v.screen = Main; v.shell.fkeys.set(&kMainMenu); return; }
        (void)v.set.key(ctx, ev);
        return;
    }
    case Table: {
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActGraph || a == ActWeb) { if (v.runner.running()) return; buildGraph(ctx, v, a == ActWeb); return; }
        const int n = static_cast<int>(v.rows.size());
        if (ev.is(Key::Up)) v.row = std::max(0, v.row - 1);
        else if (ev.is(Key::Down)) v.row = std::min(std::max(0, n - 1), v.row + 1);
        else if (ev.is(Key::Exit)) { v.screen = Main; v.shell.fkeys.set(&kMainMenu); }
        return;
    }
    case Plot: case Web:
        if (ev.is(Key::Exit) || ev.is(Key::AC)) { v.screen = Table; v.shell.fkeys.set(&kTableMenu); }
        return;
    }
}

void RecursionApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    ui::clear(c);
    ctx.drawStatus(c, "RECURSION");
    const int top = ui::ContentTop;
    switch (v.screen) {
    case Main: {
        drawModeHeader(c, ctx.ws.recursion.type == rec::Type::An1 ? "Type: an+1 = f(n, an)" : "Type: an+2 = f(n, an, an+1)");
        const std::string label = ctx.ws.recursion.type == rec::Type::An1 ? "an+1=" : "an+2=";
        c.rect(0, top + 20, ui::ScreenW, 22, SelectSoft, true);
        ui::text(c, 4, top + 23, label.c_str(), Ink);
        ui::textFit(c, 56, top + 23, shown(formula(ctx)), ui::ScreenW - 60, Ink);
        const ModeVariables& m = ctx.ws.modeVars;
        char info[96];
        std::snprintf(info, sizeof info, "Start=%s  End=%s  a0=%s  a1=%s", formNumber(m.recStart).c_str(), formNumber(m.recEnd).c_str(),
                      formNumber(m.recInitial[0]).c_str(), formNumber(m.recInitial[1]).c_str());
        ui::text(c, 4, top + 50, info, Muted, font::Face::Small);
        break;
    }
    case Set:
        drawModeHeader(c, "Table Setting");
        v.set.draw(c, top + 18, ui::ContentBottom);
        break;
    case Table: {
        const bool sigma = ctx.ws.recursion.sigma;
        c.rect(0, top, ui::ScreenW, 18, StatusBg, true);
        ui::textCenter(c, 40, top + 1, "n", Ink);
        ui::textCenter(c, sigma ? 140 : 200, top + 1, "an", Ink);
        if (sigma) ui::textCenter(c, 250, top + 1, "\xCE\xA3" "an", Ink);
        const int rowH = 18, rows = (ui::ContentBottom - top - 18 - 18) / rowH;
        if (v.row < v.first) v.first = v.row;
        if (v.row >= v.first + rows) v.first = v.row - rows + 1;
        for (int k = 0; k < rows && v.first + k < static_cast<int>(v.rows.size()); ++k) {
            const rec::Row& r = v.rows[static_cast<std::size_t>(v.first + k)];
            const int y = top + 18 + k * rowH;
            const bool sel = v.first + k == v.row;
            if (sel) c.rect(0, y, ui::ScreenW, rowH, SelectSoft, true);
            ui::textRight(c, 70, y + 1, std::to_string(r.n).c_str(), Ink);
            ui::textRight(c, sigma ? 196 : 300, y + 1, fmt(ctx, r.a).c_str(), Ink);
            if (sigma) ui::textRight(c, 312, y + 1, fmt(ctx, r.sum).c_str(), Ink);
        }
        if (v.runner.running()) ui::textRight(c, ui::ScreenW - 4, ui::ContentBottom - 16, (std::to_string(std::max(0, v.runner.percent())) + "%").c_str(), Warn, font::Face::Small);
        break;
    }
    case Plot: case Web:
        graph2d::drawAxes(c, v.viewport, Muted, Grid, false);
        if (v.fnPlot) v.fnPlot->draw(c, Graph[1]);
        if (v.diag) v.diag->draw(c, Graph[4]);
        if (v.plot) {
            v.plot->draw(c, Graph[0]);
            // i termini come punti (G-PLT)
            if (v.screen == Plot) {
                const auto& p = v.plot->points();
                for (std::size_t i = 0; i + 1 < p.size(); i += 2) if (p[i] != graph::PenUp) c.rect(p[i] - 1, p[i + 1] - 1, 3, 3, Graph[0], true);
            }
        }
        break;
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.files.draw(c);
}
} // namespace fx::app
