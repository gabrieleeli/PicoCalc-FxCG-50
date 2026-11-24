// DISTRIBUTION (v1.18.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Funzioni del capitolo 18 del manuale fx-CG50 (descrizione del comportamento,
// nessun codice copiato): scelta della distribuzione e della funzione (PD, CD,
// inversa) fra le 24 della fx-CG50, parametri con Data Variable o List,
// CALC con il risultato in Ans (Variable) o in ListAns (List) e Save Res,
// DRAW della curva con l'area per le distribuzioni continue. I calcoli sono
// quelli di dist_core (lo stesso nucleo del DIST di Statistics); le liste e le
// inverse girano in step::Task con AC. Stato nella sezione 0x0117 del record v12.
#include "apps.hpp"
#include "dist_core.hpp"
#include "distribution.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Page : int { Menu = 0, Input, Results, Graph, Running };
enum Act : int { ActCalc = 1, ActDraw, ActSelect };
const FKeyPage kMenuPage{{{"SELECT", ActSelect}, {}, {}, {}, {}, {}}};
const FKeyMenu kMenuMenu{&kMenuPage, 1, nullptr};
const FKeyPage kFormPage{{{"CALC", ActCalc}, {}, {}, {}, {}, {"DRAW", ActDraw}}};
const FKeyMenu kFormMenu{&kFormPage, 1, nullptr};
const FKeyPage kCalcPage{{{"CALC", ActCalc}, {}, {}, {}, {}, {}}};
const FKeyMenu kCalcMenu{&kCalcPage, 1, nullptr};
const FKeyPage kDrawPage{{{}, {}, {}, {}, {}, {"DRAW", ActDraw}}};
const FKeyMenu kDrawMenu{&kDrawPage, 1, nullptr};
const FKeyPage kEmptyPage{{{}, {}, {}, {}, {}, {}}};
const FKeyMenu kEmptyMenu{&kEmptyPage, 1, nullptr};
std::vector<std::string> lists(const char* none) {
    std::vector<std::string> v;
    if (none) v.emplace_back(none);
    for (int i = 1; i <= 26; ++i) v.push_back("List " + std::to_string(i));
    return v;
}
class OneStep final : public step::Task {
public:
    explicit OneStep(std::function<void()> fn) : fn_(std::move(fn)) {}
    bool step(step::Budget&) override { fn_(); done_ = 1; return true; }
    std::size_t total() const override { return 1; }
    std::size_t done() const override { return done_; }
private:
    std::function<void()> fn_;
    std::size_t done_{0};
};
} // namespace

struct DistributionApp::View {
    ModeShell shell;
    SelectList kinds;
    Form form;
    int page{Menu}, before{Menu};
    ResultList results;
    dist::GraphModel graph;
    std::vector<std::string> lines;
    bool drawAfter{false};
    step::Runner runner;
    std::unique_ptr<step::Task> task;
    std::function<void(Context&)> onDone;
};

DistributionApp::DistributionApp() = default;
DistributionApp::~DistributionApp() = default;

namespace {
using View = DistributionApp::View;
dist::Kind kindOf(const Context& ctx) { return dist::kindAt(ctx.ws.distribution.kind); }
int fieldAt(const Form& f, const std::string& label) {
    for (std::size_t i = 0; i < f.fields.size(); ++i) if (f.fields[i].label == label) return static_cast<int>(i);
    return -1;
}
int choice(const Form& f, const std::string& label) { const int i = fieldAt(f, label); return i < 0 ? 0 : f.fields[static_cast<std::size_t>(i)].choice; }
void setBar(View& v) {
    v.shell.fkeys.set(v.page == Input ? &kFormMenu : v.page == Results && v.graph.available ? &kDrawMenu : &kEmptyMenu);
}
void buildForm(Context& ctx, View& v) {
    const dmode::State& s = ctx.ws.distribution;
    const dist::Kind k = kindOf(ctx);
    const auto spec = dist::paramSpecs(k);
    const std::vector<double> values = dmode::paramsOf(s, s.kind);
    const std::size_t args = dist::argCount(k);
    std::vector<FormField> f;
    f.push_back(choiceField("Data", {"List", "Variable"}, s.listData ? 0 : 1));
    if (dist::hasTail(k)) f.push_back(choiceField("Tail", {"Left", "Right", "Central"}, s.tail));
    if (s.listData) {
        const auto labels = dist::listLabels(k);
        f.push_back(choiceField(labels[0], lists(nullptr), s.list1));
        if (labels.size() > 1) f.push_back(choiceField(labels[1], lists(nullptr), s.list2));
    }
    for (std::size_t i = s.listData ? args : 0; i < spec.size(); ++i) f.push_back(numberField(spec[i].label, formNumber(values[i])));
    f.push_back(choiceField("Save Res", lists("None"), s.saveRes));
    const int sel = v.form.selected;
    v.form.fields = std::move(f);
    v.form.selected = std::min(std::max(0, sel), static_cast<int>(v.form.fields.size()) - 1);
    v.shell.fkeys.set(dist::continuous(k.family) ? &kFormMenu : &kCalcMenu);
}
// Il modulo nello stato (scelte subito, numeri validi soltanto): garanzia forte.
void applyForm(Context& ctx, View& v, bool numbers) {
    dmode::State s = ctx.ws.distribution;
    const dist::Kind k = kindOf(ctx);
    const auto spec = dist::paramSpecs(k);
    const std::size_t args = dist::argCount(k);
    const bool list = choice(v.form, "Data") == 0;
    s.tail = static_cast<std::uint8_t>(choice(v.form, "Tail"));
    s.saveRes = static_cast<std::uint8_t>(choice(v.form, "Save Res"));
    if (s.listData && list) {
        const auto labels = dist::listLabels(k);
        s.list1 = static_cast<std::uint8_t>(choice(v.form, labels[0]));
        if (labels.size() > 1) s.list2 = static_cast<std::uint8_t>(choice(v.form, labels[1]));
    }
    if (numbers) {
        std::vector<double> values = dmode::paramsOf(s, s.kind);
        for (std::size_t i = s.listData ? args : 0; i < spec.size(); ++i) {
            const int fi = fieldAt(v.form, spec[i].label);
            if (fi >= 0) values[i] = evalNumber(ctx, v.form.fields[static_cast<std::size_t>(fi)].text);
        }
        s.params[s.kind] = values;
    }
    const bool rebuild = s.listData != list;
    s.listData = list;
    if (!(s == ctx.ws.distribution)) { ctx.ws.distribution = s; ctx.markDirty(); }
    if (rebuild) buildForm(ctx, v);
}
void start(View& v, std::unique_ptr<step::Task> t, std::function<void(Context&)> done) {
    v.task = std::move(t);
    v.onDone = std::move(done);
    v.runner.reset();
    v.runner.start(*v.task, step::Runner::Config{200, 25});
    v.before = v.page;
    v.page = Running;
    v.shell.fkeys.set(&kEmptyMenu);
}
void saveRes(Context& ctx, int choiceIndex, const std::vector<double>& values) {
    if (choiceIndex <= 0 || values.empty()) return;
    ctx.ws.setList(static_cast<std::size_t>(choiceIndex - 1), values);
    ctx.markDirty();
}
void calculate(Context& ctx, View& v, bool draw) {
    applyForm(ctx, v, true);
    const dmode::State& s = ctx.ws.distribution;
    const dist::Kind k = kindOf(ctx);
    dist::Input in;
    in.values = dmode::paramsOf(s, s.kind);
    in.tail = static_cast<dist::Tail>(s.tail);
    const int save = s.saveRes;
    v.drawAfter = draw;
    View* self = &v;
    if (s.listData) {
        const auto labels = dist::listLabels(k);
        std::vector<double> first = ctx.ws.lists[s.list1];
        std::vector<double> second = labels.size() > 1 ? ctx.ws.lists[s.list2] : std::vector<double>{};
        auto* task = new dist::ListTask(k, in, std::move(first), std::move(second));
        start(v, std::unique_ptr<step::Task>(task), [self, task, k, save](Context& c) {
            View& w = *self;
            const std::vector<double>& out = task->results();
            CalcValue lv = CalcValue::list(out.size());
            lv.re = out;
            c.ws.storeCollectionAnswer(lv);
            saveRes(c, save, out);
            w.results.clear();
            w.results.title = std::string(dist::kindName(k)) + (k.func == dist::Func::Inv ? "  xInv" : "  p");
            for (std::size_t i = 0; i < out.size(); ++i) w.results.add(std::to_string(i + 1), formatReal(out[i], c.ws.settings.display));
            w.graph = {};
            c.ws.distribution.hasResult = true;
            c.ws.distribution.lastResult = out.empty() ? 0 : out.front();
            c.markDirty();
            w.page = Results;
            setBar(w);
        });
        return;
    }
    auto result = std::make_shared<dist::Result>();
    start(v, std::make_unique<OneStep>([result, k, in] { *result = dist::compute(k, in); }), [self, result, k, in, save](Context& c) {
        View& w = *self;
        w.results.clear();
        w.results.title = dist::kindName(k);
        std::vector<double> values;
        w.lines.clear();
        for (const auto& [label, value] : result->rows) {
            w.results.add(label, formatReal(value, c.ws.settings.display));
            values.push_back(value);
            w.lines.push_back(label + "=" + formatReal(value, c.ws.settings.display));
        }
        c.ws.memory.setAnswer(result->main);
        saveRes(c, save, values);
        c.ws.distribution.hasResult = true;
        c.ws.distribution.lastResult = result->main;
        c.markDirty();
        w.graph = dist::graphModel(k, in, *result);
        w.page = w.drawAfter && w.graph.available ? Graph : Results;
        setBar(w);
    });
}
} // namespace

void DistributionApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    for (std::size_t i = 0; i < dist::KindCount; ++i) v_->kinds.items.push_back(std::string(dist::kindKey(dist::kindAt(i))) + "  " + dist::kindName(dist::kindAt(i)));
    v_->kinds.selected = ctx.ws.distribution.kind;
    v_->shell.fkeys.set(&kMenuMenu);
}
void DistributionApp::leave(Context&) { v_.reset(); }
std::string DistributionApp::screenName() const {
    if (!v_) return "closed";
    static const char* const n[] = {"menu", "input", "results", "graph", "running"};
    return n[v_->page];
}
bool DistributionApp::tick(Context& ctx) {
    if (!v_ || !v_->task) return false;
    View& v = *v_;
    if (v.runner.running()) {
        try { v.runner.tick([&ctx] { return ctx.now(); }); }
        catch (...) { v.task.reset(); v.onDone = nullptr; v.page = v.before; setBar(v); if (v.page == Input) buildForm(ctx, v); throw; }
    }
    if (v.runner.state() == step::State::Done) {
        auto done = std::move(v.onDone);
        v.onDone = nullptr;
        if (done) done(ctx);
        v.task.reset();
        v.runner.reset();
    }
    return true;
}

void DistributionApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    if (v.runner.running()) {
        if (ev.is(Key::AC)) {
            v.runner.interrupt(); v.task.reset(); v.onDone = nullptr; v.runner.reset();
            v.page = v.before; buildForm(ctx, v);
            throw MathError("Break: calcolo interrotto (AC)");
        }
        return;
    }
    switch (v.page) {
    case Menu:
        if (ev.is(Key::Exe) || v.shell.fkeys.key(ev, false) == ActSelect) {
            ctx.ws.distribution.kind = static_cast<std::uint8_t>(v.kinds.selected);
            ctx.markDirty();
            v.form.selected = 0;
            buildForm(ctx, v);
            v.page = Input;
            return;
        }
        if (ev.is(Key::Exit)) { ctx.home(); return; }
        (void)v.kinds.key(ev, 10);
        return;
    case Input: {
        if (v.form.editing()) { (void)v.form.key(ctx, ev); if (!v.form.editing()) applyForm(ctx, v, true); return; }
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActCalc || a == ActDraw) { calculate(ctx, v, a == ActDraw); return; }
        if (ev.is(Key::Exit)) { v.page = Menu; v.shell.fkeys.set(&kMenuMenu); return; }
        (void)v.form.key(ctx, ev);
        applyForm(ctx, v, false);
        return;
    }
    case Results: {
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActDraw && v.graph.available) { v.page = Graph; v.shell.fkeys.set(&kEmptyMenu); return; }
        if (ev.is(Key::Exit)) { v.page = Input; buildForm(ctx, v); return; }
        (void)v.results.key(ev);
        return;
    }
    case Graph:
        if (ev.is(Key::Exit) || ev.is(Key::Exe)) { v.page = Results; setBar(v); }
        return;
    default: return;
    }
}

void DistributionApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    ui::clear(c);
    ctx.drawStatus(c, "DISTRIBUTION");
    const int top = ui::ContentTop;
    const dist::Kind k = kindOf(ctx);
    switch (v.page) {
    case Menu:
        drawModeHeader(c, "Distribution  (24 funzioni)");
        v.kinds.draw(c, gfx::Rect{0, top + 18, ui::ScreenW, ui::ContentBottom - top - 18}, 20);
        break;
    case Input:
        drawModeHeader(c, dist::kindName(k));
        v.form.draw(c, top + 20, ui::ContentBottom);
        break;
    case Results: v.results.draw(c, top + 1, ui::ContentBottom); break;
    case Graph: drawDistributionGraph(c, gfx::Rect{0, top, ui::ScreenW, ui::ContentBottom - top}, v.graph, v.lines); break;
    case Running: {
        drawModeHeader(c, "Calcolo in corso");
        const int pct = v.runner.percent();
        ui::textCenter(c, ui::ScreenW / 2, top + 100, (pct >= 0 ? std::to_string(pct) + " %" : std::string("...")).c_str(), Ink);
        ui::textCenter(c, ui::ScreenW / 2, top + 130, "AC: Break", Muted, font::Face::Small);
        break;
    }
    default: break;
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
}
} // namespace fx::app
