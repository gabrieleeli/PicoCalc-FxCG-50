// DYNA GRAPH (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Funzioni minime della sezione 5-42 del manuale fx-CG50 (descrizione del
// comportamento, nessun codice copiato): una funzione Y= con un coefficiente
// variabile, Start/End/Pitch (le variabili di VARS › DYNA), animazione a passi
// sulla V-Window di Graph con velocita' regolabile, ciclo continuo o passata
// singola, AC per fermarla. Il coefficiente e' legato nell'EvalContext (la
// memoria A-Z non cambia); ogni fotogramma e' campionato da graph2d::Plotter.
#include "apps.hpp"
#include "dyna.hpp"
#include "graph2d.hpp"
#include <algorithm>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Act : int { ActDyna = 0, ActVwin, ActNew, ActOpen, ActSaveAs, ActOk, ActSlower, ActFaster };
const FKeyPage kFilePage{{{"OPEN", ActOpen}, {"SAVE\xC2\xB7" "AS", ActSaveAs}, {}, {}, {}, {}}};
const FKeyMenu kFileMenu{&kFilePage, 1, nullptr};
const FKeyPage kMainPage{{{"DYNA", ActDyna}, {"V-WIN", ActVwin}, {}, {}, {"FILE", -1, &kFileMenu}, {}}};
const FKeyMenu kMainMenu{&kMainPage, 1, nullptr};
const FKeyPage kAnimPage{{{"SLOWER", ActSlower}, {"FASTER", ActFaster}, {}, {}, {}, {}}};
const FKeyMenu kAnimMenu{&kAnimPage, 1, nullptr};
const FKeyPage kWinPage{{{}, {}, {}, {}, {}, {"OK", ActOk}}};
const FKeyMenu kWinMenu{&kWinPage, 1, nullptr};
const char* const kVarNames[26] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
                                   "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"};
enum Screen : int { Main = 0, Window = 1, Anim = 2 };
std::string num(double v) { return formNumber(v); }
} // namespace

struct DynaGraphApp::View {
    ModeShell shell;
    TypedFileMenu files{files::FileType::DynaGraph};
    Form form;
    Form window;
    Screen screen{Main};
    std::vector<double> frames;
    std::size_t frame{0};
    int direction{1};
    std::uint32_t frameAt{0};
    bool stopped{false};
    std::unique_ptr<graph2d::FunctionSource> source;
    std::unique_ptr<graph2d::Plotter> plotter;
    graph::Viewport viewport;
};

DynaGraphApp::DynaGraphApp() = default;
DynaGraphApp::~DynaGraphApp() = default;

namespace {
void fillForm(Context& ctx, DynaGraphApp::View& v) {
    const dyna::State& s = ctx.ws.dyna;
    const ModeVariables& m = ctx.ws.modeVars;
    v.form.fields.clear();
    v.form.fields.push_back(textField("Y=", s.function));
    std::vector<std::string> letters;
    for (char c = 'A'; c <= 'Z'; ++c) letters.push_back(std::string(1, c));
    v.form.fields.push_back(choiceField("Dynamic Var", letters, s.variable - 'A'));
    v.form.fields.push_back(numberField("Start", num(m.dynaStart)));
    v.form.fields.push_back(numberField("End", num(m.dynaEnd)));
    v.form.fields.push_back(numberField("Pitch", num(m.dynaPitch)));
    v.form.fields.push_back(choiceField("Dynamic Type", {"Cont", "Stop"}, s.loop ? 0 : 1));
    v.form.fields.push_back(choiceField("Speed", {"Slow", "Normal", "Fast"}, static_cast<int>(s.speed)));
}
// Il modulo nel workspace (dopo ogni modifica): validazione come la calcolatrice.
void applyForm(Context& ctx, DynaGraphApp::View& v) {
    dyna::State s = ctx.ws.dyna;
    s.function = v.form.fields[0].text;
    s.variable = static_cast<char>('A' + v.form.fields[1].choice);
    s.loop = v.form.fields[5].choice == 0;
    s.speed = static_cast<dyna::Speed>(v.form.fields[6].choice);
    const std::string err = dyna::validate(s);
    if (!err.empty()) throw MathError(err);
    const double a = v.form.number(ctx, 2), b = v.form.number(ctx, 3), p = v.form.number(ctx, 4);
    (void)dyna::frameValues(a, b, p);                  // Range ERROR / Argument ERROR prima di scrivere
    ctx.ws.modeVars.dynaStart = a; ctx.ws.modeVars.dynaEnd = b; ctx.ws.modeVars.dynaPitch = p;
    ctx.ws.dyna = s;
    ctx.markDirty();
}
void fillWindow(Context& ctx, DynaGraphApp::View& v) {
    const ViewWindow& w = ctx.ws.view;
    v.window.fields = {numberField("Xmin", num(w.xmin)), numberField("Xmax", num(w.xmax)), numberField("Xscale", num(w.xscl)),
                       numberField("Ymin", num(w.ymin)), numberField("Ymax", num(w.ymax)), numberField("Yscale", num(w.yscl))};
}
void applyWindow(Context& ctx, DynaGraphApp::View& v) {
    ViewWindow w = ctx.ws.view;
    w.xmin = v.window.number(ctx, 0); w.xmax = v.window.number(ctx, 1); w.xscl = v.window.number(ctx, 2);
    w.ymin = v.window.number(ctx, 3); w.ymax = v.window.number(ctx, 4); w.yscl = v.window.number(ctx, 5);
    if (!(w.xmin < w.xmax) || !(w.ymin < w.ymax) || w.xscl < 0 || w.yscl < 0) throw MathError("Range ERROR: V-Window");
    ctx.ws.view = w;                                    // la V-Window di Graph, condivisa
    ctx.markDirty();
}
void startFrame(Context& ctx, DynaGraphApp::View& v) {
    EvalContext ec = ctx.ws.evalContext();
    ec.complexMode = ComplexMode::Real;
    ec.bind(kVarNames[ctx.ws.dyna.variable - 'A'], v.frames[v.frame]);
    v.source = std::make_unique<graph2d::FunctionSource>(ctx.ws.dyna.function, ec);
    v.plotter = std::make_unique<graph2d::Plotter>(*v.source, v.viewport);
}
} // namespace

void DynaGraphApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    v_->shell.fkeys.set(&kMainMenu);
    fillForm(ctx, *v_);
    // Striscia "Dynamic Graph" di eActivity: si apre sull'animazione.
    if (ctx.inStrip(AppId::DynaGraph)) {
        try {
            v_->frames = dyna::frameValues(ctx.ws.modeVars.dynaStart, ctx.ws.modeVars.dynaEnd, ctx.ws.modeVars.dynaPitch);
            v_->viewport = graph2d::layout(graph2d::Layout::Full, ctx.ws.view, ctx.ws.view).main;
            v_->screen = Anim;
            v_->shell.fkeys.set(&kAnimMenu);
            startFrame(ctx, *v_);
        } catch (const std::exception&) { v_->screen = Main; v_->shell.fkeys.set(&kMainMenu); }
    }
}
void DynaGraphApp::leave(Context&) { v_.reset(); }
std::string DynaGraphApp::screenName() const {
    if (!v_) return "closed";
    return v_->screen == Anim ? (v_->stopped ? "stopped" : "anim") : v_->screen == Window ? "vwin" : "main";
}

bool DynaGraphApp::tick(Context& ctx) {
    if (!v_ || v_->screen != Anim || v_->stopped || !v_->plotter) return false;
    View& v = *v_;
    if (!v.plotter->finished()) {
        // Un fotogramma a passi (budget di valutazioni e di tempo): la tastiera resta pronta.
        step::Clock clock = [&ctx] { return ctx.now(); };
        step::Budget b(160, 20, &clock);
        (void)v.plotter->step(b);
        if (v.plotter->finished()) v.frameAt = ctx.now();
        return true;
    }
    if (ctx.now() - v.frameAt < dyna::frameDelayMs(ctx.ws.dyna.speed)) return false;
    // Fotogramma seguente: ciclo continuo avanti e indietro, o una passata sola.
    if (v.frames.size() <= 1) { v.stopped = true; return true; }
    if (v.frame + 1 >= v.frames.size() && v.direction > 0) {
        if (!ctx.ws.dyna.loop) { v.stopped = true; return true; }
        v.direction = -1;
    } else if (v.frame == 0 && v.direction < 0) {
        v.direction = 1;
    }
    v.frame = static_cast<std::size_t>(static_cast<int>(v.frame) + v.direction);
    startFrame(ctx, v);
    return true;
}

void DynaGraphApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    if (v.files.active()) {
        if (v.files.key(ev) != 1) return;
        if (v.files.purpose() == TypedFileMenu::Purpose::Open) {
            const auto data = v.files.load(ctx, v.files.file());
            dyna::FileData f;
            if (!dyna::decodeFile(data.data(), data.size(), f)) throw MathError("Data ERROR: file Dyna Graph danneggiato");
            ctx.ws.dyna = f.state;
            ctx.ws.modeVars.dynaStart = f.start; ctx.ws.modeVars.dynaEnd = f.end; ctx.ws.modeVars.dynaPitch = f.pitch;
            ctx.markDirty();
            fillForm(ctx, v);
        } else {
            const ModeVariables& m = ctx.ws.modeVars;
            v.files.save(ctx, v.files.file(), dyna::encodeFile({ctx.ws.dyna, m.dynaStart, m.dynaEnd, m.dynaPitch}));
            ctx.toast("Salvato " + v.files.file());
        }
        return;
    }
    if (v.screen == Anim) {
        const int a = v.shell.fkeys.key(ev, ctx.shift());
        int speed = static_cast<int>(ctx.ws.dyna.speed);
        if (a == ActSlower || ev.is(Key::Left)) speed = std::max(0, speed - 1);
        else if (a == ActFaster || ev.is(Key::Right)) speed = std::min(2, speed + 1);
        else if (ev.is(Key::AC)) { v.stopped = true; return; }                       // AC ferma l'animazione
        else if (ev.is(Key::Exe) && v.stopped) { v.stopped = false; v.frameAt = ctx.now(); return; }
        else if (ev.is(Key::Exit)) { v.screen = Main; v.plotter.reset(); v.source.reset(); v.shell.fkeys.set(&kMainMenu); return; }
        else return;
        ctx.ws.dyna.speed = static_cast<dyna::Speed>(speed);
        v.form.fields[6].choice = speed;
        ctx.markDirty();
        return;
    }
    Form& f = v.screen == Window ? v.window : v.form;
    const bool editing = f.editing();
    if (!editing) {
        const int a = v.shell.fkeys.key(ev, ctx.shift());
        if (a == FKeyBar::Handled) return;
        if (v.screen == Window) {
            if (a == ActOk || ev.is(Key::Exit)) { if (a == ActOk) applyWindow(ctx, v); v.screen = Main; v.shell.fkeys.set(&kMainMenu); return; }
        } else {
            switch (a) {
            case ActDyna:
                applyForm(ctx, v);
                v.frames = dyna::frameValues(ctx.ws.modeVars.dynaStart, ctx.ws.modeVars.dynaEnd, ctx.ws.modeVars.dynaPitch);
                v.frame = 0; v.direction = 1; v.stopped = false;
                v.viewport = graph2d::layout(graph2d::Layout::Full, ctx.ws.view, ctx.ws.view).main;
                v.screen = Anim;
                v.shell.fkeys.set(&kAnimMenu);
                startFrame(ctx, v);
                return;
            case ActVwin: fillWindow(ctx, v); v.screen = Window; v.shell.fkeys.set(&kWinMenu); return;
            case ActOpen: v.shell.fkeys.set(&kMainMenu); v.files.openList(ctx); return;
            case ActSaveAs: v.shell.fkeys.set(&kMainMenu); applyForm(ctx, v); v.files.openSaveAs(ctx, ""); return;
            default: break;
            }
            if (ev.is(Key::Exit)) { if (v.shell.fkeys.inSubmenu()) v.shell.fkeys.back(); else ctx.home(); return; }
        }
    }
    if (f.key(ctx, ev) && editing && !f.editing() && v.screen == Main) applyForm(ctx, v);
    else if (v.screen == Main && (ev.is(Key::Left) || ev.is(Key::Right)) && !f.editing()) applyForm(ctx, v);
}

void DynaGraphApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    ui::clear(c);
    ctx.drawStatus(c, "DYNA GRAPH");
    if (v.screen == Anim) {
        graph2d::drawAxes(c, v.viewport, Muted, Grid, false);
        if (v.plotter) v.plotter->draw(c, Graph[0]);
        char label[64];
        std::snprintf(label, sizeof label, "%c=%s%s", ctx.ws.dyna.variable, num(v.frames.empty() ? 0 : v.frames[v.frame]).c_str(),
                      v.stopped ? "  [EXE]" : "");
        ui::text(c, 4, ui::ContentTop + 2, label, Ink, font::Face::Small);
        ui::textRight(c, ui::ScreenW - 4, ui::ContentTop + 2, dyna::speedName(ctx.ws.dyna.speed), Muted, font::Face::Small);
    } else {
        drawModeHeader(c, v.screen == Window ? "V-Window (Graph)" : "Dynamic Func: " + std::string(1, ctx.ws.dyna.variable) + " varia");
        (v.screen == Window ? v.window : v.form).draw(c, ui::ContentTop + 18, ui::ContentBottom);
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.files.draw(c);
}
} // namespace fx::app
