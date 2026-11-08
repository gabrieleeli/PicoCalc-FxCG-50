// GRAPH: relation list Y1..Y20 (Y=, r=, Param, X=, Y/X inequalities), function
// editor, TYPE/CONVERT, TOOL (STYLE, GPH-MEM, BUILT-IN, FORMAT, FILE), MODIFY,
// V-Window with V-MEM/SQUARE, the Setup items of the graph screen, and the
// pop-ups shared by every GRAPH screen. The plot screen is in graph_plot.cpp,
// projects and pictures in graph_files.cpp.
#include "apps.hpp"
#include "math_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>

namespace fx::app {
using namespace ui::theme;

namespace {
constexpr GraphType kTypeMenu[] = {GraphType::Y, GraphType::Polar, GraphType::Param, GraphType::X, GraphType::Y /* CONVERT */,
                                   GraphType::YGreater, GraphType::YLess, GraphType::YGreaterEq, GraphType::YLessEq,
                                   GraphType::XGreater, GraphType::XLess, GraphType::XGreaterEq, GraphType::XLessEq};
constexpr GraphType kConvert[] = {GraphType::Y, GraphType::YGreater, GraphType::YLess, GraphType::YGreaterEq, GraphType::YLessEq,
                                  GraphType::X, GraphType::XGreater, GraphType::XLess, GraphType::XGreaterEq, GraphType::XLessEq};
// fx-CG50 built-in function types (shared with Dyna Graph, manual 5-42).
constexpr const char* kBuiltIn[] = {"Ax+B", "A(x-B)^(2)+C", "Ax^(2)+Bx+C", "Ax^(3)+Bx^(2)+Cx+D", "Asin(Bx+C)", "Acos(Bx+C)", "Atan(Bx+C)"};
constexpr const char* kBuiltInLabel[] = {"Y=AX+B", "Y=A(X-B)\xC2\xB2+C", "Y=AX\xC2\xB2+BX+C", "Y=AX\xC2\xB3+BX\xC2\xB2+CX+D", "Y=Asin(BX+C)", "Y=Acos(BX+C)", "Y=Atan(BX+C)"};
enum WindowField { WXmin, WXmax, WXscl, WXdot, WYmin, WYmax, WYscl, WTmin, WTmax, WTstep, WCount };
enum SetupField { SFunc, SDraw, SDeriv, SAngle, SCoord, SGrid, SAxes, SLabel, SDisplay, SGraphFunc, SSimul, SBack, SFade, SPlotCol, SSketch, SIneq, SCount };

std::string numberText(double v) { return formatReal(v, {DisplayMode::Norm2, 0}); }
FormField num(const char* l, double v) { return {l, numberText(v), FormField::Type::Number, {}, 0}; }
FormField choice(const char* l, std::vector<std::string> c, int sel) {
    sel = std::clamp(sel, 0, static_cast<int>(c.size()) - 1);
    const std::string t = c[static_cast<std::size_t>(sel)];
    return {l, t, FormField::Type::Choice, std::move(c), sel};
}
std::vector<std::string> colorNames() { std::vector<std::string> v; for (std::uint8_t i = 0; i < GraphColorCount; ++i) v.emplace_back(graphColorName(i)); return v; }
std::vector<std::string> styleNames() { std::vector<std::string> v; for (std::uint8_t i = 0; i < LineStyleCount; ++i) v.emplace_back(lineStyleName(static_cast<LineStyle>(i))); return v; }
std::vector<std::string> displayChoices() {
    std::vector<std::string> v = {"Norm1", "Norm2"};
    for (int i = 0; i <= 9; ++i) v.push_back("Fix" + std::to_string(i));
    for (int i = 1; i <= 10; ++i) v.push_back("Sci" + std::to_string(i));
    v.push_back("Eng");
    return v;
}
int displayIndex(DisplayFormat f) {
    switch (f.mode) {
    case DisplayMode::Norm1: return 0;
    case DisplayMode::Norm2: return 1;
    case DisplayMode::Fix: return 2 + std::min<int>(f.digits, 9);
    case DisplayMode::Sci: return 12 + std::clamp<int>(f.digits == 0 ? 10 : f.digits, 1, 10) - 1;
    case DisplayMode::Eng: return 22;
    }
    return 0;
}
DisplayFormat displayFrom(int i) {
    if (i <= 0) return {DisplayMode::Norm1, 0};
    if (i == 1) return {DisplayMode::Norm2, 0};
    if (i <= 11) return {DisplayMode::Fix, static_cast<std::uint8_t>(i - 2)};
    if (i <= 21) return {DisplayMode::Sci, static_cast<std::uint8_t>(i - 11)};
    return {DisplayMode::Eng, 0};
}
// Prefix of a relation-list line: "Y3", "r3", "Xt3"/"Yt3", "X3".
std::string linePrefix(GraphType t, int fn, int sub) {
    const std::string n = std::to_string(fn + 1);
    if (t == GraphType::Polar) return "r" + n;
    if (t == GraphType::Param) return (sub ? "Yt" : "Xt") + n;
    if (isXType(t)) return "X" + n;
    return "Y" + n;
}
// The relation sign shown after the prefix ("=", ">", "≥"...).
std::string relation(GraphType t) {
    const std::string l = graphTypeLabel(t);
    return t == GraphType::Param ? "=" : l.substr(1);
}
const char* variableKey(GraphType t) {
    if (t == GraphType::Polar) return "\xCE\xB8";
    if (t == GraphType::Param) return "T";
    if (isXType(t)) return "Y";
    return "X";
}
} // namespace

// ---------------------------------------------------------------- lifecycle
void GraphApp::enter(Context& ctx) {
    built_ = false;                                  // functions and memory may have changed in other modes
    menu_.close(); menuKind_ = Menu::None;
    input_.ask = Ask::None; confirm_ = Confirm::None; copyTarget_ = -1;
    loadBackground(ctx);
    if (!ctx.ws.graph.overlay.empty() && overlayPath_ != ctx.ws.graph.overlay) {
        try { recallPicture(ctx, ctx.ws.graph.overlay, false); } catch (const std::exception&) { overlay_ = {}; overlayPath_.clear(); }
    }
    mode_ = Mode::View; action_ = Action::None;
    // From the main menu GRAPH opens on the relation list (fx-CG50); TABLE's
    // GPH-CON/GPH-PLT open the graph screen directly.
    screen_ = tableDraw(ctx) ? Screen::Plot : Screen::List;
}
void GraphApp::leave(Context& ctx) {
    ctx.ws.table.draw = TableRange::Draw::None;
    // Release the sampled curves while other modes run (heap for TLS, lists...).
    curves_.clear(); curves_.shrink_to_fit();
    shade_.clear(); built_ = false;
    capture_ = pict::Image{}; captureReady_ = capturePending_ = false;
}
bool GraphApp::tick(Context& ctx) {
    bool redraw = false;
    if (captureReady_) {
        try { writeCapture(ctx); } catch (const std::exception& e) { captureReady_ = false; capture_ = pict::Image{}; ctx.toast(e.what(), 2500); }
        redraw = true;
    }
    if (screen_ == Screen::Plot && !renderDone_) {
        try {
            redraw = renderStep(ctx, 600) || redraw;
        } catch (const std::bad_alloc&) {
            // Heap exhausted (pictures + curves): keep what is drawn and stop
            // sampling, so that the next tick does not fail again.
            for (auto& c : curves_) if (!c.done) c.done = c.truncated = true;
            for (auto& c : sketchCurves_) if (!c.done) c.done = c.truncated = true;
            renderDone_ = true;
            throw MathError("Memory ERROR: memoria insufficiente, disegno parziale");
        }
    }
    if (screen_ == Screen::Plot && mode_ == Mode::Select) {
        const std::uint32_t phase = (ctx.now() - blinkStart_) / 300;
        if (phase != lastBlink_) { lastBlink_ = phase; redraw = true; }
    }
    return redraw;
}
void GraphApp::finishRendering(Context& ctx) {
    ensureBuilt(ctx);
    for (int guard = 0; !renderDone_ && guard < 100000; ++guard) (void)renderStep(ctx, 1 << 20);
}
void GraphApp::changed(Context& ctx) {
    built_ = false;
    ctx.markDirty();
    ctx.ws.graph.projectModified = true;
}

// ---------------------------------------------------------------- relation list
GraphType GraphApp::lineType(Context& ctx, int fn) const {
    const GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(fn)];
    return f.empty() ? ctx.ws.graph.settings.funcType : f.type;   // empty lines take the Func Type
}
std::vector<GraphApp::Line> GraphApp::lines(Context& ctx) const {
    std::vector<Line> out;
    out.reserve(MaxGraphFunctions + 4);
    for (int i = 0; i < static_cast<int>(MaxGraphFunctions); ++i) {
        out.push_back({i, 0});
        if (lineType(ctx, i) == GraphType::Param) out.push_back({i, 1});
    }
    return out;
}

void GraphApp::beginEdit(Context& ctx, bool keep, char first) {
    const auto ls = lines(ctx);
    row_ = std::clamp(row_, 0, static_cast<int>(ls.size()) - 1);
    const Line l = ls[static_cast<std::size_t>(row_)];
    editFn_ = l.fn; editSub_ = l.sub; editNew_ = !keep;
    editor_.clear();
    const GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(l.fn)];
    const std::string& text = l.sub ? f.expr2 : f.expr;
    if (keep && !text.empty()) editor_.setExpression(text);
    if (first) editor_.insert(first);
    screen_ = Screen::Edit;
}

void GraphApp::storeEdit(Context& ctx) {
    GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(editFn_)];
    const GraphType wanted = ctx.ws.graph.settings.funcType;
    const std::string text = editor_.empty() ? std::string() : editor_.toExpression();
    if (!text.empty()) {
        if (text.size() > MaxGraphExpression) throw MathError("Memory ERROR: espressione troppo lunga");
        const graph::FunctionText t = graph::splitFunctionText(text);   // Syntax ERROR on a bad suffix
        (void)parseExpression(t.body);                                   // syntax check before storing
        if (editSub_ == 1 && (t.hasRange || t.overwrite)) throw SyntaxError("Syntax ERROR: intervallo e [A=] vanno dopo Xt");
    }
    // A new function cannot replace one of a different type (manual 5-13).
    if (editNew_ && !f.empty() && f.type != wanted)
        throw MathError(std::string("Argument ERROR: ") + linePrefix(f.type, editFn_, 0) + " ha un altro tipo (DELETE o CONVERT)");
    if (f.empty()) f.type = wanted;
    if (editSub_ == 1) f.expr2 = text; else f.expr = text;
    if (f.type != GraphType::Param) f.expr2.clear();
    if (!text.empty()) f.selected = true;
    changed(ctx);
    screen_ = Screen::List;
    const auto ls = lines(ctx);
    row_ = std::min(row_ + 1, static_cast<int>(ls.size()) - 1);
}

void GraphApp::listKey(Context& ctx, const KeyEvent& ev) {
    auto ls = lines(ctx);
    row_ = std::clamp(row_, 0, static_cast<int>(ls.size()) - 1);
    const Line line = ls[static_cast<std::size_t>(row_)];
    GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(line.fn)];
    const int n = static_cast<int>(ls.size());
    // Modify COPY: choose the destination line.
    if (copyTarget_ >= 0) {
        if (ev.is(Key::Up)) row_ = (row_ + n - 1) % n;
        else if (ev.is(Key::Down)) row_ = (row_ + 1) % n;
        else if (ev.is(Key::Exe)) { copyModified(ctx, line.fn); }
        else if (ev.is(Key::Exit)) { copyTarget_ = -1; screen_ = Screen::Plot; mode_ = Mode::Modify; }
        return;
    }
    if (ctx.shift()) {
        // SHIFT layer: TRACE ZOOM V-WIN SKETCH G-SOLV G<->T (manual 5-2).
        switch (ev.key) {
        case Key::F1: showPlot(); haveOriginal_ = false; startTrace(ctx, Action::None); return;
        case Key::F2: showPlot(); haveOriginal_ = false; key(ctx, {Key::F2}); return;
        case Key::F3: returnScreen_ = Screen::List; openWindow(ctx); return;
        case Key::F4: showPlot(); haveOriginal_ = false; key(ctx, {Key::F4}); return;
        case Key::F5: showPlot(); haveOriginal_ = false; key(ctx, {Key::F5}); return;
        case Key::F6: showPlot(); haveOriginal_ = false; return;
        default: break;
        }
    }
    switch (ev.key) {
    case Key::Up: row_ = (row_ + n - 1) % n; return;
    case Key::Down: row_ = (row_ + 1) % n; return;
    case Key::PageUp: row_ = std::max(0, row_ - 6); return;
    case Key::PageDown: row_ = std::min(n - 1, row_ + 6); return;
    case Key::Home: row_ = 0; return;
    case Key::End: row_ = n - 1; return;
    case Key::F1: f.selected = !f.selected; changed(ctx); return;
    case Key::F2: case Key::DelForward: case Key::Del:
        if (f.empty()) return;
        confirm(Confirm::DeleteFunction, "Eliminare " + linePrefix(f.type, line.fn, 0) + (f.type == GraphType::Param ? " e Yt" + std::to_string(line.fn + 1) : std::string()) + "?");
        return;
    case Key::F3: {
        std::vector<std::string> items;
        for (std::size_t i = 0; i < std::size(kTypeMenu); ++i) items.emplace_back(i == 4 ? "CONVERT \xE2\x96\xB6" : graphTypeLabel(kTypeMenu[i]));
        showMenu(Menu::Type, "TYPE", items);
        return;
    }
    case Key::F4: showMenu(Menu::Tool, "TOOL", {"STYLE \xE2\x96\xB6", "GPH-MEM \xE2\x96\xB6", "BUILT-IN \xE2\x96\xB6", "FORMAT", "FILE \xE2\x96\xB6"}); return;
    case Key::F5: startModify(ctx, -1); return;
    case Key::F6: showPlot(); haveOriginal_ = false; marks_.clear(); return;
    case Key::Optn: showMenu(Menu::ListOptn, "OPTN", {"FORMAT", "FILE \xE2\x96\xB6", "V-WIN", "\xE2\x86\x92 TABLE"}); return;
    case Key::Exe: case Key::Right: beginEdit(ctx, true); return;
    case Key::Char:
        if (ev.ctrl) {
            if (ev.ch == 'v' || ev.ch == 'V') { if (ctx.clipboard) { beginEdit(ctx, false); editor_.paste(*ctx.clipboard); } }
            return;
        }
        beginEdit(ctx, false, ev.ch);
        return;
    case Key::Exit: ctx.home(); return;
    default: return;
    }
}

void GraphApp::editKey(Context& ctx, const KeyEvent& ev) {
    const GraphType t = lineType(ctx, editFn_);
    switch (ev.key) {
    case Key::Exe: storeEdit(ctx); return;
    case Key::Exit: screen_ = Screen::List; return;
    case Key::Char:
        if (ev.ctrl) {
            switch (ev.ch) {
            case 'c': case 'C': ctx.clipboard = editor_.copyTree(); ctx.toast("Copiato"); return;
            case 'v': case 'V': if (ctx.clipboard) editor_.paste(*ctx.clipboard); return;
            case 'z': case 'Z': editor_.undo(); return;
            case 'y': case 'Y': editor_.redo(); return;
            default: return;
            }
        }
        editor_.insert(ev.ch);
        return;
    case Key::Left: editor_.moveLeft(); return;
    case Key::Right: editor_.moveRight(); return;
    case Key::Up: editor_.moveUp(); return;
    case Key::Down: editor_.moveDown(); return;
    case Key::Home: editor_.moveHome(); return;
    case Key::End: editor_.moveEnd(); return;
    case Key::Del: editor_.backspace(); return;
    case Key::DelForward: editor_.eraseForward(); return;
    case Key::AC: editor_.clear(); return;
    case Key::F1: editor_.insertFraction(); return;
    case Key::F2: editor_.insertRadical(); return;
    case Key::F3: editor_.insertPower(); return;
    case Key::F4:
        if (t == GraphType::Polar) editor_.insertText("theta");
        else if (t == GraphType::Param) editor_.insert('t');
        else if (isXType(t)) editor_.insert('y');
        else editor_.insert('x');
        return;
    case Key::F5: editor_.insertAbs(); return;
    case Key::F6: case Key::Optn:
        showMenu(Menu::EditMore, "INSERISCI", {"[ ]  intervallo ,[a,b]", "[A=]  sovrapposizione", "List 1-6", "\xCF\x80", "\xCE\xB8", "x\xC2\xB2", "log\xE2\x82\x90" "b", "e^\xE2\x96\xA1", "d/dx", "\xE2\x88\xAB" "dx", "\xE2\x89\xA5  \xE2\x89\xA4  =", "GRAPH: Y r Xt Yt X"});
        return;
    case Key::Vars: showMenu(Menu::EditVars, "VARS GRAPH", {"Y  (Y=, Y>...)", "r  (polare)", "Xt (parametrica)", "Yt (parametrica)", "X  (X=, X>...)"}); return;
    default: return;
    }
}

// ---------------------------------------------------------------- V-Window
void GraphApp::loadWindowForm(const ViewWindow& v) {
    window_.fields = {num("Xmin", v.xmin), num("Xmax", v.xmax), num("Xscale", v.xscl), num("Xdot", viewXdot(v)),
                      num("Ymin", v.ymin), num("Ymax", v.ymax), num("Yscale", v.yscl),
                      num("T\xCE\xB8min", v.tmin), num("T\xCE\xB8max", v.tmax), num("T\xCE\xB8ptch", v.tstep)};
    windowStart_ = v;
    windowText_.clear();
    for (const auto& f : window_.fields) windowText_.push_back(f.text);
}
void GraphApp::openWindow(Context& ctx) {
    loadWindowForm(ctx.ws.view);
    window_.selected = 0;
    screen_ = Screen::Window;
}
bool GraphApp::applyWindow(Context& ctx) {
    auto edited = [&](int i) { return window_.fields[static_cast<std::size_t>(i)].text != windowText_[static_cast<std::size_t>(i)]; };
    auto val = [&](int i, double original) { return edited(i) ? window_.number(ctx, static_cast<std::size_t>(i)) : original; };
    const ViewWindow s = windowStart_;
    ViewWindow v = s;
    v.xmin = val(WXmin, s.xmin); v.xmax = val(WXmax, s.xmax); v.xscl = val(WXscl, s.xscl);
    v.ymin = val(WYmin, s.ymin); v.ymax = val(WYmax, s.ymax); v.yscl = val(WYscl, s.yscl);
    v.tmin = val(WTmin, s.tmin); v.tmax = val(WTmax, s.tmax); v.tstep = val(WTstep, s.tstep);
    // Changing Xdot moves Xmax (manual 5-6).
    if (edited(WXdot) && !edited(WXmax)) v = graph::withXdot(v, window_.number(ctx, WXdot));
    // Tθmax < Tθmin makes Tθptch negative (manual 5-6).
    if (v.tstep != 0 && (v.tmax - v.tmin) * v.tstep < 0) v.tstep = -v.tstep;
    const std::string err = validateView(v);
    if (!err.empty()) throw MathError(err);
    if (!(v == ctx.ws.view)) {
        ctx.ws.view = v;
        haveOriginal_ = false; zoomStack_.clear(); marks_.clear(); shade_.clear(); shadeIntegral_ = false;
        ctx.markDirty(); ctx.ws.graph.projectModified = true;
    }
    loadWindowForm(v);
    return true;
}
void GraphApp::windowKey(Context& ctx, const KeyEvent& ev) {
    if (window_.editing()) {
        const int field = window_.selected;
        (void)window_.key(ctx, ev);
        if (!window_.editing() && (field == WXmin || field == WXmax || field == WXdot)) {
            // Keep Xdot and Xmax consistent while editing.
            try {
                const double xmin = window_.number(ctx, WXmin);
                if (field == WXdot) {
                    const double d = window_.number(ctx, WXdot);
                    if (d > 0) window_.fields[WXmax].text = numberText(xmin + d * (graphgeo::Columns - 1));
                } else {
                    const double xmax = window_.number(ctx, WXmax);
                    window_.fields[WXdot].text = numberText((xmax - xmin) / (graphgeo::Columns - 1));
                    windowText_[WXdot] = window_.fields[WXdot].text;   // derived, not an edit of Xdot
                }
            } catch (const std::exception&) {}
        }
        return;
    }
    const AngleMode angle = ctx.ws.settings.angle;
    auto current = [&]() { ViewWindow keep = ctx.ws.view; try { (void)applyWindow(ctx); } catch (...) { ctx.ws.view = keep; throw; } return ctx.ws.view; };
    switch (ev.key) {
    case Key::F1: loadWindowForm(graph::initialWindow(angle));  return;
    case Key::F2: { ViewWindow v = graph::trigWindow(windowStart_, angle); loadWindowForm(v);  return; }
    case Key::F3: { ViewWindow v = graph::standardWindow(windowStart_); loadWindowForm(v);  return; }
    case Key::F4: showMenu(Menu::VMem, "V-MEM", {"STORE", "RECALL"}); return;
    case Key::F5: showMenu(Menu::Square, "SQUARE", {"Y-BASE (X corretto)", "X-BASE (Y corretto)"}); return;
    case Key::F6:
        if (!background_.empty()) confirm(Confirm::BgvLoad, "Caricare la V-Window dello sfondo?");
        return;
    case Key::Exit:
        (void)current();
        screen_ = returnScreen_;
        if (screen_ == Screen::Plot) mode_ = Mode::View;
        return;
    default: break;
    }
    (void)window_.key(ctx, ev);
}

// ---------------------------------------------------------------- Setup
void GraphApp::openSetup(Context& ctx) {
    const GraphSettings& g = ctx.ws.graph.settings;
    const Settings& s = ctx.ws.settings;
    std::vector<std::string> types;
    for (std::uint8_t i = 0; i < GraphTypeCount; ++i) types.emplace_back(graphTypeLabel(static_cast<GraphType>(i)));
    std::vector<std::string> back = {"None"};
    for (int i = 1; i <= pict::PictMemories; ++i) back.push_back("PICT " + std::to_string(i));
    if (!g.backgroundFile.empty()) back.push_back(files::stem(g.backgroundFile.substr(g.backgroundFile.rfind('/') + 1)));
    std::vector<std::string> fades;
    for (int i = 0; i <= 100; i += 5) fades.push_back(std::to_string(i) + "%");
    setup_.fields = {
        choice("Func Type", types, static_cast<int>(g.funcType)),
        choice("Draw Type", {"Connect", "Plot"}, g.plotDraw ? 1 : 0),
        choice("Derivative", {"On", "Off"}, g.derivative ? 0 : 1),
        choice("Angle", {"Deg", "Rad", "Gra"}, s.angle == AngleMode::Degrees ? 0 : s.angle == AngleMode::Radians ? 1 : 2),
        choice("Coord", {"On", "Off"}, g.coord ? 0 : 1),
        choice("Grid", {"On", "Off", "Line"}, g.grid == GridMode::On ? 0 : g.grid == GridMode::Off ? 1 : 2),
        choice("Axes", {"On", "Off", "Scale"}, g.axes == AxesMode::On ? 0 : g.axes == AxesMode::Off ? 1 : 2),
        choice("Label", {"On", "Off"}, g.label ? 0 : 1),
        choice("Display", displayChoices(), displayIndex(s.display)),
        choice("Graph Func", {"On", "Off"}, g.graphFunc ? 0 : 1),
        choice("Simul Graph", {"On", "Off"}, g.simulGraph ? 0 : 1),
        choice("Background", back, g.background == BackgroundFile ? static_cast<int>(back.size()) - 1 : g.background),
        choice("Fade I/O", fades, g.fade / 5),
        choice("Plot/LineCol", colorNames(), g.plotColor),
        choice("Sketch Line", styleNames(), static_cast<int>(g.sketchLine)),
        choice("Ineq Type", {"Intsect", "Union"}, g.ineqUnion ? 1 : 0),
    };
    setup_.selected = 0;
    returnScreen_ = screen_ == Screen::Setup ? returnScreen_ : screen_;
    screen_ = Screen::Setup;
}
void GraphApp::applySetup(Context& ctx) {
    GraphSettings& g = ctx.ws.graph.settings;
    Settings& s = ctx.ws.settings;
    const auto& f = setup_.fields;
    const std::uint8_t oldBack = g.background, oldFade = g.fade;
    g.funcType = static_cast<GraphType>(f[SFunc].choice);
    g.plotDraw = f[SDraw].choice == 1;
    g.derivative = f[SDeriv].choice == 0;
    s.angle = f[SAngle].choice == 0 ? AngleMode::Degrees : f[SAngle].choice == 1 ? AngleMode::Radians : AngleMode::Gradians;
    g.coord = f[SCoord].choice == 0;
    g.grid = f[SGrid].choice == 0 ? GridMode::On : f[SGrid].choice == 1 ? GridMode::Off : GridMode::Line;
    g.axes = f[SAxes].choice == 0 ? AxesMode::On : f[SAxes].choice == 1 ? AxesMode::Off : AxesMode::Scale;
    g.label = f[SLabel].choice == 0;
    s.display = displayFrom(f[SDisplay].choice);
    g.graphFunc = f[SGraphFunc].choice == 0;
    g.simulGraph = f[SSimul].choice == 0;
    const int b = f[SBack].choice;
    g.background = b == 0 ? 0 : b <= pict::PictMemories ? static_cast<std::uint8_t>(b) : BackgroundFile;
    g.fade = static_cast<std::uint8_t>(f[SFade].choice * 5);
    g.plotColor = static_cast<std::uint8_t>(f[SPlotCol].choice);
    g.sketchLine = static_cast<LineStyle>(f[SSketch].choice);
    g.ineqUnion = f[SIneq].choice == 1;
    ctx.ws.apply();
    changed(ctx);
    if (g.background != oldBack || g.fade != oldFade) {
        background_ = pict::Image{}; backgroundPath_.clear();
        try { loadBackground(ctx); } catch (const std::exception& e) { ctx.toast(e.what(), 2500); }
        if (!background_.empty() && g.background != oldBack) confirm(Confirm::BgvLoad, "Caricare la V-Window dello sfondo?");
    }
}
void GraphApp::setupKey(Context& ctx, const KeyEvent& ev) {
    if (setup_.editing()) { (void)setup_.key(ctx, ev); return; }
    if (ev.is(Key::Exit) || ev.is(Key::Setup) || ev.is(Key::Exe)) {
        applySetup(ctx);
        screen_ = returnScreen_;
        if (screen_ == Screen::Plot) mode_ = Mode::View;
        return;
    }
    if (ev.is(Key::F1) && setup_.selected == SBack) { applySetup(ctx); openFiles(ctx, Browse::Background); return; }
    (void)setup_.key(ctx, ev);
}

// ---------------------------------------------------------------- FORMAT (line style and colour)
void GraphApp::openFormat(Context& ctx, int function, int sketchItem) {
    formatFn_ = function; formatSketch_ = sketchItem;
    std::uint8_t color = 0; LineStyle style = LineStyle::Normal;
    if (function >= 0) { const auto& f = ctx.ws.functions[static_cast<std::size_t>(function)]; color = f.color; style = f.style; }
    else if (sketchItem >= 0 && sketchItem < static_cast<int>(ctx.ws.graph.sketch.size())) { const auto& s = ctx.ws.graph.sketch[static_cast<std::size_t>(sketchItem)]; color = s.color; style = s.style; }
    else return;
    format_.fields = {choice("Line Style", styleNames(), static_cast<int>(style)), choice("Line Color", colorNames(), color)};
    format_.selected = 0;
    formatReturn_ = screen_ == Screen::Edit ? Screen::List : screen_;
    screen_ = Screen::Format;
}
void GraphApp::formatKey(Context& ctx, const KeyEvent& ev) {
    if (ev.is(Key::Exit) || ev.is(Key::Exe)) {
        const auto style = static_cast<LineStyle>(format_.fields[0].choice);
        const auto color = static_cast<std::uint8_t>(format_.fields[1].choice);
        if (formatFn_ >= 0) { auto& f = ctx.ws.functions[static_cast<std::size_t>(formatFn_)]; f.style = style; f.color = color; changed(ctx); }
        else if (formatSketch_ >= 0 && formatSketch_ < static_cast<int>(ctx.ws.graph.sketch.size())) {
            auto& s = ctx.ws.graph.sketch[static_cast<std::size_t>(formatSketch_)]; s.style = style; s.color = color;
            ctx.markDirty(); ctx.ws.graph.projectModified = true;
        }
        screen_ = formatReturn_;
        if (screen_ == Screen::Plot) mode_ = Mode::View;
        return;
    }
    (void)format_.key(ctx, ev);
}

// ---------------------------------------------------------------- MODIFY (manual 5-38)
void GraphApp::startModify(Context& ctx, int plot) {
    ensureBuilt(ctx);
    const auto& plots = engine_.plots();
    int chosen = plot, fn = -1, count = 0;
    std::string vars;
    for (int i = 0; i < static_cast<int>(plots.size()); ++i) {
        const auto& p = plots[static_cast<std::size_t>(i)];
        if (p.pasted || p.function < 0 || p.variant != 0) continue;
        std::string v = graph::GraphEngine::coefficients(*p.body, p.variable);
        if (p.body2) for (char c : graph::GraphEngine::coefficients(*p.body2, p.variable)) if (v.find(c) == std::string::npos) v.push_back(c);
        if (p.bindName[0]) v.erase(std::remove(v.begin(), v.end(), static_cast<char>(std::toupper(static_cast<unsigned char>(p.bindName[0])))), v.end());
        if (v.empty()) continue;
        ++count;
        if (plot < 0 || plot == i) { chosen = i; fn = p.function; std::sort(v.begin(), v.end()); vars = v; }
    }
    if (count == 0) throw MathError("Argument ERROR: nessuna funzione con variabili (A..Z)");
    if (count > 1 && plot < 0) throw MathError("Argument ERROR: piu' di una funzione con variabili");
    if (vars.size() > 5) throw MathError("Argument ERROR: al massimo 5 variabili");
    modify_.plot = chosen; modify_.function = fn; modify_.vars = vars; modify_.selected = 0;
    if (!(modify_.step > 0)) modify_.step = 1;
    screen_ = Screen::Plot; mode_ = Mode::Modify;
    haveOriginal_ = false;
}
void GraphApp::modifyKey(Context& ctx, const KeyEvent& ev) {
    const int n = static_cast<int>(modify_.vars.size());
    switch (ev.key) {
    case Key::Up: modify_.selected = (modify_.selected + n) % (n + 1); return;
    case Key::Down: modify_.selected = (modify_.selected + 1) % (n + 1); return;
    case Key::Left: case Key::Right: {
        if (modify_.selected >= n) return;
        const char v = modify_.vars[static_cast<std::size_t>(modify_.selected)];
        const double d = (ev.is(Key::Left) ? -1 : 1) * modify_.step;
        double nv = ctx.ws.memory.recall(v) + d;
        if (std::abs(nv) < 1e-12 * std::max(1.0, std::abs(modify_.step))) nv = 0;
        ctx.ws.memory.store(v, nv);
        changed(ctx);
        return;
    }
    case Key::F1: case Key::Optn: {
        // COPY: choose the destination line, starting on the first empty one.
        copyTarget_ = 0;
        const auto ls = lines(ctx);
        for (int i = 0; i < static_cast<int>(ls.size()); ++i)
            if (ctx.ws.functions[static_cast<std::size_t>(ls[static_cast<std::size_t>(i)].fn)].empty()) { row_ = i; break; }
        screen_ = Screen::List;
        return;
    }
    case Key::Exit: mode_ = Mode::View; return;
    case Key::Char:
        if (!ev.ctrl && (std::isdigit(static_cast<unsigned char>(ev.ch)) || ev.ch == '-' || ev.ch == '.' || ev.ch == '(')) {
            if (modify_.selected >= n) ask(Ask::ModifyStep, "Step:", std::string(1, ev.ch));
            else ask(Ask::ModifyValue, std::string(1, modify_.vars[static_cast<std::size_t>(modify_.selected)]) + "=", std::string(1, ev.ch));
        }
        return;
    case Key::Exe:
        if (modify_.selected >= n) askValue(Ask::ModifyStep, "Step:", numberText(modify_.step));
        else askValue(Ask::ModifyValue, std::string(1, modify_.vars[static_cast<std::size_t>(modify_.selected)]) + "=",
                      numberText(ctx.ws.memory.recall(modify_.vars[static_cast<std::size_t>(modify_.selected)])));
        return;
    default: return;
    }
}
void GraphApp::copyModified(Context& ctx, int fn) {
    GraphFunction& dst = ctx.ws.functions[static_cast<std::size_t>(fn)];
    if (dst.selected && dst.defined() && fn != modify_.function) throw MathError("Argument ERROR: Expression in use (espressione in uso)");
    if (fn == modify_.function && dst.selected) throw MathError("Argument ERROR: Expression in use (espressione in uso)");
    const GraphFunction& src = ctx.ws.functions[static_cast<std::size_t>(modify_.function)];
    auto substitute = [&](const std::string& text) {
        const graph::FunctionText t = graph::splitFunctionText(text);
        return rewriteIdentifiers(t.body, [&](const std::string& id) -> std::string {
            if (id.size() != 1) return {};
            const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(id[0])));
            if (modify_.vars.find(up) == std::string::npos) return {};
            char buf[40];
            std::snprintf(buf, sizeof(buf), "(%.15g)", ctx.ws.memory.recall(up));
            return buf;
        });
    };
    GraphFunction copy = src;
    copy.expr = substitute(src.expr);
    if (src.type == GraphType::Param) copy.expr2 = substitute(src.expr2);
    copy.selected = false;
    dst = copy;
    copyTarget_ = -1;
    changed(ctx);
    screen_ = Screen::Plot; mode_ = Mode::Modify;
    ctx.toast("Copiato in " + linePrefix(copy.type, fn, 0));
}

// ---------------------------------------------------------------- menus, pop-ups, confirmations
void GraphApp::showMenu(Menu m, const char* title, std::vector<std::string> items) {
    menu_.show(title, std::move(items));
    menuKind_ = m;
}
void GraphApp::ask(Ask a, const std::string& prompt, const std::string& initial, bool text) {
    input_.ask = a; input_.prompt = prompt; input_.text = text; input_.replace = false;
    input_.editor.clear();
    if (!initial.empty()) input_.editor.insert(initial);
}
void GraphApp::askValue(Ask a, const std::string& prompt, const std::string& current, bool text) {
    ask(a, prompt, current, text);
    input_.replace = !current.empty();
}
void GraphApp::confirm(Confirm c, const std::string& message) { confirm_ = c; confirmText_ = message; }

void GraphApp::menuChoice(Context& ctx, Menu menu, int c) {
    auto ls = lines(ctx);
    const Line line = ls[static_cast<std::size_t>(std::clamp(row_, 0, static_cast<int>(ls.size()) - 1))];
    GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(line.fn)];
    switch (menu) {
    case Menu::Type:
        if (c == 4) {
            std::vector<std::string> items;
            for (GraphType t : kConvert) items.push_back(std::string("'") + graphTypeLabel(t));
            showMenu(Menu::Convert, "CONVERT", items);
            return;
        }
        ctx.ws.graph.settings.funcType = kTypeMenu[c];
        changed(ctx);
        return;
    case Menu::Convert: {
        const GraphType to = kConvert[c];
        if (!f.defined()) throw MathError("Argument ERROR: riga vuota");
        if (!isConvertible(f.type)) throw MathError("Argument ERROR: CONVERT solo per funzioni rettangolari e disequazioni");
        if (isYType(f.type) != isYType(to)) {
            // Y=f(x) <-> X=f(y): the variable changes with the type.
            const graph::FunctionText t = graph::splitFunctionText(f.expr);
            std::string body = rewriteIdentifiers(t.body, [](const std::string& id) -> std::string { return id == "x" ? "y" : id == "y" ? "x" : std::string(); });
            const auto open = f.expr.find('[');
            if (open != std::string::npos) { auto comma = f.expr.rfind(',', open); body += f.expr.substr(comma); }
            f.expr = body;
        }
        f.type = to;
        changed(ctx);
        return;
    }
    case Menu::Tool:
        switch (c) {
        case 0: showMenu(Menu::Style, "STYLE", styleNames()); return;
        case 1: showMenu(Menu::GMem, "GPH-MEM", {"STORE", "RECALL"}); return;
        case 2: { std::vector<std::string> items(std::begin(kBuiltInLabel), std::end(kBuiltInLabel)); showMenu(Menu::BuiltIn, "BUILT-IN", items); return; }
        case 3: openFormat(ctx, line.fn, -1); return;
        default: showMenu(Menu::File, "FILE (progetto)", {"NEW", "OPEN", "SAVE", "SAVE AS", "RENAME", "DELETE"}); return;
        }
    case Menu::ListOptn:
        switch (c) {
        case 0: openFormat(ctx, line.fn, -1); return;
        case 1: showMenu(Menu::File, "FILE (progetto)", {"NEW", "OPEN", "SAVE", "SAVE AS", "RENAME", "DELETE"}); return;
        case 2: returnScreen_ = screen_; openWindow(ctx); return;
        default: ctx.open(AppId::Table); return;
        }
    case Menu::Style: f.style = static_cast<LineStyle>(c); changed(ctx); return;
    case Menu::GMem: ask(c == 0 ? Ask::GMemStore : Ask::GMemRecall, c == 0 ? "STORE G-Mem (1-20):" : "RECALL G-Mem (1-20):"); return;
    case Menu::BuiltIn: {
        if (!f.empty() && f.type != GraphType::Y) throw MathError("Argument ERROR: la riga contiene un altro tipo");
        f.type = GraphType::Y; f.expr = kBuiltIn[c]; f.expr2.clear(); f.selected = true;
        changed(ctx);
        return;
    }
    case Menu::File:
        switch (c) {
        case 0:
            if (ctx.ws.graph.projectModified) confirm(Confirm::NewProject, "Modifiche non salvate. Nuovo progetto?");
            else newProject(ctx);
            return;
        case 1:
            if (ctx.ws.graph.projectModified) confirm(Confirm::OpenProject, "Modifiche non salvate. Aprire un altro progetto?");
            else openFiles(ctx, Browse::Projects);
            return;
        case 2:
            if (ctx.ws.graph.project.empty()) ask(Ask::ProjectSaveAs, "SAVE AS (max 16):", {}, true);
            else saveProject(ctx, ctx.ws.graph.project);
            return;
        case 3: askValue(Ask::ProjectSaveAs, "SAVE AS (max 16):", ctx.ws.graph.project, true); return;
        default: openFiles(ctx, Browse::Projects); return;      // RENAME / DELETE from the file list
        }
    case Menu::EditMore:
        switch (c) {
        case 0: editor_.insertText(",["); editor_.insertText(","); editor_.insertText("]"); editor_.moveLeft(); editor_.moveLeft(); return;
        case 1: editor_.insertText(",[A="); editor_.insertText("]"); editor_.moveLeft(); return;
        case 2: showMenu(Menu::EditList, "LIST", {"List 1", "List 2", "List 3", "List 4", "List 5", "List 6"}); return;
        case 3: editor_.insertText("pi"); return;
        case 4: editor_.insertText("theta"); return;
        case 5: editor_.insertSquare(); return;
        case 6: editor_.insertLogBase(); return;
        case 7: editor_.insertExp(); return;
        case 8: editor_.insertDerivative(); return;
        case 9: editor_.insertIntegral(); return;
        case 10: editor_.insertText(">="); return;
        default: showMenu(Menu::EditVars, "VARS GRAPH", {"Y  (Y=, Y>...)", "r  (polare)", "Xt (parametrica)", "Yt (parametrica)", "X  (X=, X>...)"}); return;
        }
    case Menu::EditList: editor_.insertSymbol("List" + std::to_string(c + 1)); return;
    case Menu::EditVars: {
        const Ask refs[] = {Ask::RefY, Ask::RefR, Ask::RefXt, Ask::RefYt, Ask::RefX};
        const char* names[] = {"Y", "r", "Xt", "Yt", "X"};
        ask(refs[c], std::string(names[c]) + " numero (1-20):");
        return;
    }
    case Menu::VMem: ask(c == 0 ? Ask::VMemStore : Ask::VMemRecall, c == 0 ? "STORE V-Win (1-6):" : "RECALL V-Win (1-6):"); return;
    case Menu::Square: {
        ViewWindow cur = ctx.ws.view;
        try { (void)applyWindow(ctx); cur = ctx.ws.view; } catch (...) { throw; }
        loadWindowForm(c == 0 ? graph::squareWindow(cur) : graph::squareXBase(cur));
        
        return;
    }
    default:
        plotMenu(ctx, menu, c);              // plot-screen menus (graph_plot.cpp)
        return;
    }
}

void GraphApp::answer(Context& ctx, Ask a, const std::string& text) {
    auto number = [&]() { return evalNumber(ctx, text); };
    auto index = [&](int max) {
        const double v = number();
        if (v != std::floor(v) || v < 1 || v > max) throw MathError("Argument ERROR: numero da 1 a " + std::to_string(max));
        return static_cast<int>(v);
    };
    switch (a) {
    case Ask::GMemStore: storeGraphMemory(ctx, index(static_cast<int>(GraphMemories))); return;
    case Ask::GMemRecall: recallGraphMemory(ctx, index(static_cast<int>(GraphMemories))); return;
    case Ask::VMemStore: {
        const int n = index(static_cast<int>(VWindowMemories));
        (void)applyWindow(ctx);
        ctx.ws.graph.vmem[static_cast<std::size_t>(n - 1)] = ctx.ws.view;
        ctx.ws.graph.vmemUsed = static_cast<std::uint8_t>(ctx.ws.graph.vmemUsed | (1u << (n - 1)));
        ctx.markDirty(); ctx.ws.graph.projectModified = true;
        ctx.toast("V-Win" + std::to_string(n) + " memorizzata");
        return;
    }
    case Ask::VMemRecall: {
        const int n = index(static_cast<int>(VWindowMemories));
        if (!(ctx.ws.graph.vmemUsed & (1u << (n - 1)))) throw MathError("Memory ERROR: V-Win" + std::to_string(n) + " vuota");
        loadWindowForm(ctx.ws.graph.vmem[static_cast<std::size_t>(n - 1)]);
        
        return;
    }
    case Ask::RefY: case Ask::RefR: case Ask::RefXt: case Ask::RefYt: case Ask::RefX: {
        const int n = index(static_cast<int>(MaxGraphFunctions));
        const char* kind = a == Ask::RefY ? "Y" : a == Ask::RefR ? "R" : a == Ask::RefXt ? "XT" : a == Ask::RefYt ? "YT" : "X";
        editor_.insertSymbol(std::string("@") + kind + std::to_string(n));
        return;
    }
    case Ask::ModifyValue: {
        const double v = number();
        ctx.ws.memory.store(modify_.vars[static_cast<std::size_t>(modify_.selected)], v);
        changed(ctx);
        return;
    }
    case Ask::ModifyStep: {
        const double v = number();
        if (!(v > 0)) throw MathError("Argument ERROR: Step deve essere positivo");
        modify_.step = v;
        return;
    }
    default: break;
    }
    // Plot-screen and file questions.
    pendingName_ = text;
    if (plotAnswer(ctx, a, text)) return;
    switch (a) {
    case Ask::ProjectSaveAs: {
        if (!files::validName(text, 16)) throw MathError("Argument ERROR: nome di 1-16 caratteri A-Z 0-9 _ -");
        files::FileSystem* fs = fileSystem(ctx, true);
        files::Entry e;
        if (text != ctx.ws.graph.project && fs->stat(files::join(files::GraphFolder, text + files::ProjectExtension), e) == files::Status::Ok) {
            confirm(Confirm::Overwrite, text + " esiste. Sovrascrivere?");
            return;
        }
        saveProject(ctx, text);
        return;
    }
    case Ask::ProjectRename: {
        if (!files::validName(text, 16)) throw MathError("Argument ERROR: nome di 1-16 caratteri A-Z 0-9 _ -");
        files::FileSystem* fs = fileSystem(ctx, true);
        if (files_.entries.empty()) return;
        const std::string from = files_.entries[static_cast<std::size_t>(files_.selected)].name;
        const std::string ext = files_.purpose == Browse::Projects ? files::ProjectExtension : files::PictExtension;
        const files::Status st = fs->rename(files::join(files_.folder, from), files::join(files_.folder, text + ext));
        if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
        if (files_.purpose == Browse::Projects && files::stem(from) == ctx.ws.graph.project) { ctx.ws.graph.project = text; ctx.markDirty(); }
        openFiles(ctx, files_.purpose);
        return;
    }
    default: return;
    }
}

void GraphApp::confirmed(Context& ctx, Confirm c) {
    switch (c) {
    case Confirm::DeleteFunction: {
        const auto ls = lines(ctx);
        const Line line = ls[static_cast<std::size_t>(std::clamp(row_, 0, static_cast<int>(ls.size()) - 1))];
        GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(line.fn)];
        const std::uint8_t color = f.color; const LineStyle style = f.style;
        f = emptyGraphFunction(static_cast<std::size_t>(line.fn));
        f.color = color; f.style = style;
        changed(ctx);
        return;
    }
    case Confirm::NewProject: newProject(ctx); return;
    case Confirm::OpenProject: openFiles(ctx, Browse::Projects); return;
    case Confirm::Overwrite: saveProject(ctx, pendingName_); return;
    case Confirm::DeleteFile: deleteFile(ctx); return;
    case Confirm::ClearSketch:
        ctx.ws.graph.sketch.clear(); ctx.ws.graph.overlay.clear(); overlay_ = pict::Image{}; overlayPath_.clear();
        pasted_.clear(); marks_.clear(); changed(ctx);
        return;
    case Confirm::BgvLoad:
        if (!background_.empty()) {
            ViewWindow v = background_.view;
            const ViewWindow cur = screen_ == Screen::Window ? windowStart_ : ctx.ws.view;
            v.tmin = cur.tmin; v.tmax = cur.tmax; v.tstep = cur.tstep;    // T values are not taken (manual 5-11)
            if (screen_ == Screen::Window) { loadWindowForm(v);  }
            else if (validateView(v).empty()) { ctx.ws.view = v; ctx.markDirty(); ctx.ws.graph.projectModified = true; }
        }
        return;
    case Confirm::BgvSave: bgvSave(ctx, backgroundPath_); return;
    default: return;
    }
}

// ---------------------------------------------------------------- key routing
void GraphApp::key(Context& ctx, const KeyEvent& ev) {
    if (input_.ask != Ask::None) {
        if (ev.is(Key::Exit)) { input_.ask = Ask::None; return; }
        if (ev.is(Key::Exe)) {
            const Ask a = input_.ask;
            const std::string text = input_.editor.text();
            input_.ask = Ask::None;
            if (text.empty()) return;
            answer(ctx, a, text);
            return;
        }
        if (input_.replace && ((ev.is(Key::Char) && !ev.ctrl) || ev.is(Key::Del))) input_.editor.clear();
        input_.replace = false;
        (void)input_.editor.key(ev);
        return;
    }
    if (confirm_ != Confirm::None) {
        const Confirm c = confirm_;
        if (ev.is(Key::F1) || ev.is(Key::Exe) || ev.isChar('y') || ev.isChar('s')) { confirm_ = Confirm::None; confirmed(ctx, c); }
        else if (ev.is(Key::F6) || ev.is(Key::Exit) || ev.isChar('n')) confirm_ = Confirm::None;
        return;
    }
    if (menu_.open) {
        const int choice = menu_.key(ev);
        if (choice >= 0) { const Menu m = menuKind_; menuKind_ = Menu::None; menuChoice(ctx, m, choice); }
        else if (choice == -2) menuKind_ = Menu::None;
        return;
    }
    if (ev.is(Key::Setup)) {
        if (screen_ == Screen::Setup) { applySetup(ctx); screen_ = returnScreen_; return; }
        if (screen_ == Screen::Edit) screen_ = Screen::List;
        openSetup(ctx);
        return;
    }
    switch (screen_) {
    case Screen::List: listKey(ctx, ev); return;
    case Screen::Edit: editKey(ctx, ev); return;
    case Screen::Window: windowKey(ctx, ev); return;
    case Screen::Setup: setupKey(ctx, ev); return;
    case Screen::Format: formatKey(ctx, ev); return;
    case Screen::Files: filesKey(ctx, ev); return;
    case Screen::Plot: plotKey(ctx, ev); return;
    }
}

// ---------------------------------------------------------------- drawing (non-plot screens)
void GraphApp::drawList(Context& ctx, gfx::Canvas& c) {
    ensureBuilt(ctx);
    const auto ls = lines(ctx);
    row_ = std::clamp(row_, 0, static_cast<int>(ls.size()) - 1);
    const bool editing = screen_ == Screen::Edit;
    const int rowH = 28, top = ui::ContentTop + 1;
    const int panelH = editing ? 72 : 0;
    const int visible = (ui::ContentBottom - top - panelH) / rowH;
    if (row_ < firstRow_) firstRow_ = row_;
    if (row_ >= firstRow_ + visible) firstRow_ = row_ - visible + 1;
    for (int k = 0; k < visible && firstRow_ + k < static_cast<int>(ls.size()); ++k) {
        const Line line = ls[static_cast<std::size_t>(firstRow_ + k)];
        const GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(line.fn)];
        const GraphType t = lineType(ctx, line.fn);
        const int y = top + k * rowH;
        const bool sel = firstRow_ + k == row_;
        if (sel) c.rect(0, y, ui::ScreenW, rowH, copyTarget_ >= 0 ? rgb888To565(255, 220, 230) : SelectSoft, true);
        const std::uint16_t col = [&] { const std::uint16_t pal[GraphColorCount] = {Graph[0], Graph[1], Graph[2], Graph[3], Graph[4], Graph[5], rgb888To565(225, 180, 0)}; return pal[f.color % GraphColorCount]; }();
        c.rect(2, y + rowH / 2 - 7, 3, 14, col, true);
        // Line style sample.
        const int sy = y + rowH / 2;
        switch (f.style) {
        case LineStyle::Thick: c.rect(7, sy - 2, 9, 4, col, true); break;
        case LineStyle::Thin: c.hLine(7, sy, 9, col); break;
        case LineStyle::Broken: c.rect(7, sy - 1, 3, 3, col, true); c.rect(13, sy - 1, 3, 3, col, true); break;
        case LineStyle::Dot: c.rect(7, sy, 2, 2, col, true); c.rect(11, sy, 2, 2, col, true); c.rect(15, sy, 1, 2, col, true); break;
        default: c.rect(7, sy - 1, 9, 2, col, true); break;
        }
        const std::string prefix = linePrefix(t, line.fn, line.sub);
        const std::string rel = relation(t);
        const int pw = font::measure(font::Face::Medium, prefix.c_str()).width;
        const int rw = font::measure(font::Face::Medium, rel.c_str()).width;
        ui::text(c, 20, sy - 8, prefix.c_str(), Ink);
        const bool on = f.selected && f.defined();
        if (on) c.rect(20 + pw, sy - 9, rw + 2, 18, Accent, true);     // highlighted "=" = drawn (CG50)
        ui::text(c, 21 + pw, sy - 8, rel.c_str(), on ? 0xFFFF : Ink);
        const std::string& text = line.sub ? f.expr2 : f.expr;
        const int ex = 26 + pw + rw;
        if (!text.empty()) {
            NaturalEditor view;
            try { view.setExpression(text); } catch (...) { view.clear(); view.insertText("?"); }
            const auto sz = math::measure(view.root());
            auto l = math::layout(view.root(), ex, sy - sz.height / 2 + sz.baseline, ui::ScreenW - ex - 4, rowH);
            math::draw(c, l, Ink, Cursor, Placeholder);
            if (!engine_.error(static_cast<std::size_t>(line.fn)).empty() && f.defined()) ui::textRight(c, ui::ScreenW - 3, y + 1, "ERROR", Error, font::Face::Small);
        }
        c.hLine(0, y + rowH - 1, ui::ScreenW, Grid);
    }
    ui::scrollBar(c, ui::ScreenW - 3, top, visible * rowH, static_cast<int>(ls.size()), visible, firstRow_);
    if (editing) {
        const int ey = ui::ContentBottom - panelH;
        c.rect(0, ey, ui::ScreenW, panelH, StatusBg, true);
        c.hLine(0, ey, ui::ScreenW, Faint);
        const GraphType t = lineType(ctx, editFn_);
        const std::string head = linePrefix(t, editFn_, editSub_) + relation(t);
        ui::text(c, 4, ey + 4, head.c_str(), Accent, font::Face::Small);
        const math::CursorTarget cur{editor_.activeRowPtr(), editor_.cursorIndex()};
        const auto sz = math::measure(editor_.root());
        auto l = math::layout(editor_.root(), 6, ey + 20 + sz.baseline, ui::ScreenW - 12, panelH - 22, math::Align::Left, &cur);
        math::draw(c, l, Ink, Cursor, Placeholder);
        const char* keys[6] = {"a/b", "\xE2\x88\x9A\xE2\x96\xA1", "\xE2\x96\xA1^", variableKey(t), "|\xE2\x96\xA1|", "INS \xE2\x96\xB6"};
        ui::softKeys(c, keys);
        return;
    }
    if (copyTarget_ >= 0) {
        ui::toast(c, "COPY: scegli la riga, EXE");
        const char* keys[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, keys);
        return;
    }
    if (ctx.shift()) {
        const char* keys[6] = {"TRACE", "ZOOM", "V-WIN", "SKETCH", "G-SOLV", "G\xE2\x87\x94T"};
        ui::softKeys(c, keys);
    } else {
        const char* keys[6] = {"SELECT", "DELETE", "TYPE", "TOOL", "MODIFY", "DRAW"};
        ui::softKeys(c, keys);
    }
}

void GraphApp::drawWindow(Context& ctx, gfx::Canvas& c) {
    (void)ctx;
    c.rect(0, ui::ContentTop, ui::ScreenW, 20, SelectSoft, true);
    ui::text(c, 6, ui::ContentTop + 2, "View Window", Ink);
    window_.draw(c, ui::ContentTop + 22, ui::ContentBottom - 2);
    const char* keys[6] = {"INITIAL", "TRIG", "STANDRD", "V-MEM", "SQUARE", background_.empty() ? nullptr : "BGV-WIN"};
    ui::softKeys(c, keys);
}

void GraphApp::drawPopups(Context& ctx, gfx::Canvas& c) {
    (void)ctx;
    if (input_.ask != Ask::None) {
        const gfx::Rect r{20, 120, ui::ScreenW - 40, 58};
        ui::panel(c, r, 0xFFFF, Accent);
        ui::textFit(c, r.x + 8, r.y + 6, input_.prompt, r.w - 16, Accent, font::Face::Small);
        c.rect(r.x + 6, r.y + 24, r.w - 12, 22, StatusBg, true);
        input_.editor.draw(c, r.x + 10, r.y + 27, r.w - 20, Ink);
    }
    if (confirm_ != Confirm::None) {
        const gfx::Rect r{16, 110, ui::ScreenW - 32, 80};
        ui::panel(c, r, 0xFFFF, Warn);
        std::string lines[3];
        const std::size_t n = ui::wrapText(confirmText_, 34, lines, 3);
        for (std::size_t i = 0; i < n; ++i) ui::text(c, r.x + 8, r.y + 6 + static_cast<int>(i) * 16, lines[i].c_str(), Ink);
        ui::text(c, r.x + 8, r.y + r.h - 20, "F1/EXE: S\xC3\xAC   F6/EXIT: No", Muted, font::Face::Small);
    }
}

void GraphApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "GRAPH");
    switch (screen_) {
    case Screen::List: case Screen::Edit: drawList(ctx, c); break;
    case Screen::Plot: drawPlot(ctx, c); break;
    case Screen::Window: drawWindow(ctx, c); break;
    case Screen::Setup: {
        c.rect(0, ui::ContentTop, ui::ScreenW, 20, SelectSoft, true);
        ui::text(c, 6, ui::ContentTop + 2, "SET UP (Graph)", Ink);
        setup_.draw(c, ui::ContentTop + 22, ui::ContentBottom - 2);
        const char* keys[6] = {setup_.selected == SBack ? "OPEN" : nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, keys);
        break;
    }
    case Screen::Format: {
        c.rect(0, ui::ContentTop, ui::ScreenW, 20, SelectSoft, true);
        const std::string title = formatFn_ >= 0 ? "FORMAT " + linePrefix(ctx.ws.functions[static_cast<std::size_t>(formatFn_)].type, formatFn_, 0) : std::string("FORMAT SKETCH");
        ui::text(c, 6, ui::ContentTop + 2, title.c_str(), Ink);
        format_.draw(c, ui::ContentTop + 24, ui::ContentTop + 70);
        ui::text(c, 8, ui::ContentTop + 84, "\xE2\x97\x80 \xE2\x96\xB6 cambia, EXIT applica", Muted, font::Face::Small);
        const char* keys[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, keys);
        break;
    }
    case Screen::Files: drawFiles(ctx, c); break;
    }
    menu_.draw(c);
    drawPopups(ctx, c);
}
} // namespace fx::app
