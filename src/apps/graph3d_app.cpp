// 3D GRAPH (v1.14.0, fx-CG50 manual chapter 16): function list, function
// selection screen, template coefficients, direct function input (Z=, Param,
// Rot X, Rot Y), 3D View Window with 3D-VMEM, Setup, FORMAT, 3D-GMEM and the
// pop-ups shared by every screen. The graph screen (computation, rotation,
// ZOOM, TRACE, SKETCH, G-Solve, PICTURE) is in graph3d_plot.cpp.
//
// Keys (PicoCalc adaptation, as in GRAPH): SHIFT = F7, OPTN = F8, SET UP = F10;
//   list:   F1 SELECT, F2 DELETE (or DEL), F3 TYPE (or ←/→), F4 3D-GMEM,
//           F6 DRAW (or EXE), SHIFT F3 V-WIN, SHIFT 5 / OPTN FORMAT,
//           SHIFT F1/F2/F4/F5 draw and open TRACE/ZOOM/SKETCH/G-SOLVE, SHIFT F6 G<->T;
//   select: F1 Template, F2 Z=, F3 Param, F4 Rotate (F1 Rot X, F2 Rot Y), EXE on a template;
//   coefficients: F1-F4 input forms, EDIT, F6 SET; input: F6 SET, while typing F1 X/S, F2 Y/T.
#include "apps.hpp"
#include "graph3d.hpp"
#include "graph3d_model.hpp"
#include "limits.hpp"
#include "math_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
enum WindowField { WXmin, WXmax, WXgrid, WYmin, WYmax, WYgrid, WZmin, WZmax, WSmin, WSmax, WSgrid, WTmin, WTmax, WTgrid, WTheta, WPhi, WCount };
enum SetupField { SInput, SAngle, SDisplay, SPlotCol, SAxes, SBox, SLabel, SCount };
constexpr g3d::Kind kTemplates[] = {g3d::Kind::Line, g3d::Kind::Plane, g3d::Kind::Sphere, g3d::Kind::Cylinder, g3d::Kind::Cone};
constexpr int TemplateCount = 5;

FormField choice(const char* l, std::vector<std::string> c, int sel) {
    sel = std::clamp(sel, 0, static_cast<int>(c.size()) - 1);
    const std::string t = c[static_cast<std::size_t>(sel)];
    return {l, t, FormField::Type::Choice, std::move(c), sel};
}
std::vector<std::string> colorChoices(std::uint8_t count) {
    std::vector<std::string> v;
    for (std::uint8_t i = 0; i < count; ++i) v.emplace_back(g3d::colorName(static_cast<g3d::Color>(i)));
    return v;
}
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
// Short number for the list and the coefficient cells.
std::string shortNumber(double v) {
    if (std::fabs(v) < 1e-13) v = 0;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}
std::string triple(const g3d::Function& f, std::size_t i) {
    return "(" + shortNumber(f.coef[i]) + "," + shortNumber(f.coef[i + 1]) + "," + shortNumber(f.coef[i + 2]) + ")";
}
std::string signedTerm(double v, const char* var) {
    return (v < 0 ? "(" + shortNumber(v) + ")" : shortNumber(v)) + var;
}

// Coefficient input screens (16-9...16-12): cells laid out as on the
// calculator, with the index of each cell in Function::coef.
struct CoefLayout {
    const char* title{};
    const char* title2{};
    int rows{1}, cols{1};
    const char* rowLabel[5]{};
    const char* header[3][4]{};
    int headerRows{1};
    bool byColumn{false};          // coef index = column * rows + row (vectors in columns)
    bool vertical{false};          // Cylinder / Cone: one value per line
    int index(int r, int c) const { return byColumn ? c * rows + r : r * cols + c; }
};
CoefLayout layoutOf(const g3d::Function& f) {
    CoefLayout l;
    switch (f.kind) {
    case g3d::Kind::Line:
        switch (static_cast<g3d::LineForm>(f.form)) {
        case g3d::LineForm::Express:
            l = {"(X-X0)/a=(Y-Y0)/b=(Z-Z0)/c", nullptr, 2, 3, {}, {{"X0", "Y0", "Z0"}, {"a", "b", "c"}}, 2, false, false};
            break;
        case g3d::LineForm::Vector:
            l = {"r = r0 + t v", nullptr, 3, 2, {"X", "Y", "Z"}, {{"r0", "v"}}, 1, true, false};
            break;
        case g3d::LineForm::PointVector:
            l = {"Through point (X0,Y0,Z0)", "Direction vector [a,b,c]", 2, 3, {}, {{"X0", "Y0", "Z0"}, {"a", "b", "c"}}, 2, false, false};
            break;
        case g3d::LineForm::Points:
            l = {"Line through 2 points", nullptr, 2, 3, {"P1", "P2"}, {{"X", "Y", "Z"}}, 1, false, false};
            break;
        }
        break;
    case g3d::Kind::Plane:
        switch (static_cast<g3d::PlaneForm>(f.form)) {
        case g3d::PlaneForm::Express: l = {"aX+bY+cZ+d=0", nullptr, 1, 4, {}, {{"a", "b", "c", "d"}}, 1, false, false}; break;
        case g3d::PlaneForm::Vector: l = {"r = r0 + s u + t v", nullptr, 3, 3, {"X", "Y", "Z"}, {{"r0", "u", "v"}}, 1, true, false}; break;
        case g3d::PlaneForm::Points: l = {"Plane through 3 points", nullptr, 3, 3, {"P1", "P2", "P3"}, {{"X", "Y", "Z"}}, 1, false, false}; break;
        }
        break;
    case g3d::Kind::Sphere:
        if (static_cast<g3d::SphereForm>(f.form) == g3d::SphereForm::Factor)
            l = {"(X-a)\xC2\xB2+(Y-b)\xC2\xB2+(Z-c)\xC2\xB2=r\xC2\xB2", nullptr, 1, 4, {}, {{"a", "b", "c", "r"}}, 1, false, false};
        else
            l = {"X\xC2\xB2+Y\xC2\xB2+Z\xC2\xB2+aX+bY+cZ+d=0", nullptr, 1, 4, {}, {{"a", "b", "c", "d"}}, 1, false, false};
        break;
    case g3d::Kind::Cylinder: case g3d::Kind::Cone:
        l = {f.kind == g3d::Kind::Cylinder ? "Cylinder" : "Cone", nullptr, 5, 1, {"Radius", "Zmin", "Zmax", "Center X", "Center Y"}, {}, 0, false, true};
        break;
    default:
        break;
    }
    return l;
}
// Lines of the function list for a template (16-2, 16-6, 16-18, 16-19).
std::vector<std::string> templateLines(const g3d::Function& f) {
    using namespace g3d;
    const auto& c = f.coef;
    switch (f.kind) {
    case Kind::Line:
        switch (static_cast<LineForm>(f.form)) {
        case LineForm::Express:
            return {"(X-" + shortNumber(c[0]) + ")/" + shortNumber(c[3]) + "=(Y-" + shortNumber(c[1]) + ")/" + shortNumber(c[4]) + "=",
                    "(Z-" + shortNumber(c[2]) + ")/" + shortNumber(c[5])};
        case LineForm::Vector: return {"r=" + triple(f, 0) + "+t" + triple(f, 3)};
        case LineForm::PointVector: return {"P=" + triple(f, 0) + "  d=[" + shortNumber(c[3]) + "," + shortNumber(c[4]) + "," + shortNumber(c[5]) + "]"};
        case LineForm::Points: return {"P1=" + triple(f, 0), "P2=" + triple(f, 3)};
        }
        break;
    case Kind::Plane:
        switch (static_cast<PlaneForm>(f.form)) {
        case PlaneForm::Express: return {signedTerm(c[0], "X") + "+" + signedTerm(c[1], "Y") + "+" + signedTerm(c[2], "Z") + "+" + signedTerm(c[3], "") + "=0"};
        case PlaneForm::Vector: return {"r=" + triple(f, 0), "+s" + triple(f, 3) + "+t" + triple(f, 6)};
        case PlaneForm::Points: return {"P1=" + triple(f, 0) + " P2=" + triple(f, 3), "P3=" + triple(f, 6)};
        }
        break;
    case Kind::Sphere:
        if (static_cast<SphereForm>(f.form) == SphereForm::Factor)
            return {"(X-" + shortNumber(c[0]) + ")\xC2\xB2+(Y-" + shortNumber(c[1]) + ")\xC2\xB2+(Z-" + shortNumber(c[2]) + ")\xC2\xB2=" + shortNumber(c[3]) + "\xC2\xB2"};
        return {"X\xC2\xB2+Y\xC2\xB2+Z\xC2\xB2+" + signedTerm(c[0], "X") + "+" + signedTerm(c[1], "Y") + "+" + signedTerm(c[2], "Z") + "+" + signedTerm(c[3], "") + "=0"};
    case Kind::Cylinder: case Kind::Cone:
        return {"Radius=" + shortNumber(c[0]) + "  Z:" + shortNumber(c[1]) + "~" + shortNumber(c[2]), "Center (" + shortNumber(c[3]) + "," + shortNumber(c[4]) + ")"};
    default:
        break;
    }
    return {};
}
const char* variableName(g3d::Kind k, int which) {
    if (k == g3d::Kind::Param) return which == 0 ? "s" : "t";
    return which == 0 ? "x" : "y";
}
std::uint16_t graphColor(const g3d::Function& f) {
    return g3d::color565(f.area != g3d::Color::Clear ? f.area : f.line);
}
int askNumber(Context& ctx, const std::string& text, int max) {
    const double v = evalNumber(ctx, text);
    if (!(v >= 1 && v <= max) || v != std::floor(v)) throw MathError("Argument ERROR: numero da 1 a " + std::to_string(max));
    return static_cast<int>(v);
}
} // namespace

// ---------------------------------------------------------------- lifecycle
void Graph3DApp::enter(Context& ctx) {
    (void)ctx;
    if (ctx.resuming() && screen_ == Screen::Setup) screen_ = setupReturn_;
    if (!ctx.resuming()) screen_ = Screen::List;
    if (screen_ == Screen::Plot) screen_ = Screen::List;     // A-Z or the angle unit may have changed elsewhere
    mode_ = Mode::View; sub_ = Sub::None;
    ask_.kind = Ask::Kind::None; confirmDelete_ = false;
    samplesValid_ = false; layerValid_ = false;
    message_.clear();
    // v1.16.0: striscia 3D Graph di eActivity: la schermata del grafico (calcolo progressivo).
    if (ctx.inStrip(AppId::Graph3D) && ctx.strip().entry == stripentry::Graph3D && !ctx.resuming()) {
        try { startDraw(ctx); } catch (const std::bad_alloc&) { throw; } catch (const std::exception&) { screen_ = Screen::List; }
    }
}

void Graph3DApp::leave(Context&) {
    // The lattices, the image cache and the capture are large (PSRAM on the
    // Pico Plus 2 W): released when the mode is left, recomputed on DRAW.
    releaseScene();
    capture_ = pict::Image{};
    marks_.clear(); marks_.shrink_to_fit();
    autoSteps_ = 0;
}

void Graph3DApp::changed(Context& ctx) {
    ctx.markDirty();
    stopSampling();
    samplesValid_ = false;
    layerValid_ = false;
}

// ---------------------------------------------------------------- key dispatch
void Graph3DApp::key(Context& ctx, const KeyEvent& ev) {
    if (ask_.kind != Ask::Kind::None) { askKey(ctx, ev); return; }
    if (confirmDelete_) {
        if (ev.is(Key::F1) || ev.is(Key::Exe) || ev.isChar('y')) {
            auto& f = ctx.ws.graph3d.set.fns[static_cast<std::size_t>(row_)];
            const g3d::Color line = f.line, area = f.area;
            f = g3d::emptyFunction(static_cast<std::size_t>(row_));
            f.line = line; f.area = area;                            // FORMAT stays with the row
            changed(ctx);
            confirmDelete_ = false;
        } else if (ev.is(Key::F6) || ev.is(Key::Exit) || ev.isChar('n') || ev.is(Key::AC)) {
            confirmDelete_ = false;
        }
        return;
    }
    if (ev.is(Key::Setup)) {
        if (screen_ == Screen::Setup) { applySetup(ctx); screen_ = setupReturn_; }
        else if (screen_ != Screen::Coef && screen_ != Screen::Input) openSetup(ctx);
        return;
    }
    if (ctx.shift() && ev.isChar('7') && (screen_ == Screen::Plot || screen_ == Screen::List)) { takeCapture(ctx); return; }   // CAPTURE
    switch (screen_) {
    case Screen::List: listKey(ctx, ev); return;
    case Screen::Select: selectKey(ctx, ev); return;
    case Screen::Coef: coefKey(ctx, ev); return;
    case Screen::Input: inputKey(ctx, ev); return;
    case Screen::Window: windowKey(ctx, ev); return;
    case Screen::Setup: setupKey(ctx, ev); return;
    case Screen::Format: formatKey(ctx, ev); return;
    case Screen::Plot: plotKey(ctx, ev); return;
    }
}

// ---------------------------------------------------------------- list (16-6)
void Graph3DApp::listKey(Context& ctx, const KeyEvent& ev) {
    auto& set = ctx.ws.graph3d.set;
    const int rows = static_cast<int>(g3d::Functions);
    auto& f = set.fns[static_cast<std::size_t>(row_)];
    if (sub_ == Sub::GMem) {
        if (ev.is(Key::F1)) { sub_ = Sub::None; ask(Ask::Kind::GMemStore, "STORE 3D G-Mem (1-20):"); }
        else if (ev.is(Key::F2)) { sub_ = Sub::None; ask(Ask::Kind::GMemRecall, "RECALL 3D G-Mem (1-20):"); }
        else if (ev.is(Key::Exit) || ev.is(Key::AC)) sub_ = Sub::None;
        return;
    }
    if (ctx.shift()) {
        // SHIFT layer: TRACE ZOOM V-WIN SKETCH G-SOLVE G<->T (16-3, 16-14...16-16); SHIFT 5 FORMAT.
        switch (ev.key) {
        case Key::F1: showPlot(ctx); if (screen_ == Screen::Plot) { finishSampling(ctx); startTrace(ctx); } return;
        case Key::F2: showPlot(ctx); if (screen_ == Screen::Plot) sub_ = Sub::Zoom; return;
        case Key::F3: openWindow(ctx, false); return;
        case Key::F4: showPlot(ctx); if (screen_ == Screen::Plot) sub_ = Sub::Sketch; return;
        case Key::F5: showPlot(ctx); if (screen_ == Screen::Plot) sub_ = Sub::GSolve; return;
        case Key::F6: showPlot(ctx); return;
        case Key::Char: if (ev.ch == '5') { openFormat(ctx); return; } break;
        default: break;
        }
    }
    switch (ev.key) {
    case Key::Up: row_ = (row_ + rows - 1) % rows; return;
    case Key::Down: row_ = (row_ + 1) % rows; return;
    case Key::F1: f.selected = !f.selected; changed(ctx); return;                          // SELECT
    case Key::F2: case Key::Del: case Key::DelForward: if (f.kind != g3d::Kind::None) confirmDelete_ = true; return;
    case Key::F3: openSelect(ctx); return;                                                 // TYPE
    case Key::Left: case Key::Right:
        // On a registered function the arrows open its input screen (16-8).
        if (f.kind == g3d::Kind::None) openSelect(ctx);
        else if (g3d::isTemplate(f.kind)) openTemplate(ctx, f.kind);
        else openInput(ctx, f.kind);
        return;
    case Key::F4: sub_ = Sub::GMem; return;                                                // 3D-GMEM
    case Key::F6: case Key::Exe: startDraw(ctx); return;                                   // DRAW
    case Key::Optn: openFormat(ctx); return;
    case Key::Exit: ctx.home(); return;
    case Key::Char:
        // Extension: typing on an empty or Z= row starts its Z= input.
        if (!ev.ctrl && (f.kind == g3d::Kind::None || f.kind == g3d::Kind::Z) && static_cast<unsigned char>(ev.ch) >= 0x20) {
            openInput(ctx, g3d::Kind::Z);
            beginInputEdit(ctx, false, ev.ch);
        }
        return;
    default: return;
    }
}

// ---------------------------------------------------------------- selection screen (16-8, 16-9)
void Graph3DApp::openSelect(Context& ctx) {
    const auto& f = ctx.ws.graph3d.set.fns[static_cast<std::size_t>(row_)];
    selectRow_ = 0;
    for (int i = 0; i < TemplateCount; ++i) if (kTemplates[i] == f.kind) selectRow_ = i;
    sub_ = Sub::None;
    screen_ = Screen::Select;
}
void Graph3DApp::selectKey(Context& ctx, const KeyEvent& ev) {
    if (sub_ == Sub::Rotate) {
        if (ev.is(Key::F1)) { sub_ = Sub::None; openInput(ctx, g3d::Kind::RotX); }
        else if (ev.is(Key::F2)) { sub_ = Sub::None; openInput(ctx, g3d::Kind::RotY); }
        else if (ev.is(Key::Exit) || ev.is(Key::AC)) sub_ = Sub::None;
        return;
    }
    switch (ev.key) {
    case Key::Up: selectRow_ = (selectRow_ + TemplateCount - 1) % TemplateCount; return;
    case Key::Down: selectRow_ = (selectRow_ + 1) % TemplateCount; return;
    case Key::Exe: openTemplate(ctx, kTemplates[selectRow_]); return;
    case Key::F1: return;                                       // Template: this screen
    case Key::F2: openInput(ctx, g3d::Kind::Z); return;
    case Key::F3: openInput(ctx, g3d::Kind::Param); return;
    case Key::F4: sub_ = Sub::Rotate; return;
    case Key::Exit: case Key::AC: screen_ = Screen::List; return;
    default: return;
    }
}

// ---------------------------------------------------------------- template coefficients (16-9...16-12)
void Graph3DApp::openTemplate(Context& ctx, g3d::Kind kind) {
    const auto& f = ctx.ws.graph3d.set.fns[static_cast<std::size_t>(row_)];
    if (f.kind == kind) {
        edit_ = f;                                              // re-selecting shows the previous coefficients
    } else {
        edit_ = g3d::emptyFunction(static_cast<std::size_t>(row_));
        edit_.line = f.line; edit_.area = f.area;
        edit_.kind = kind;
    }
    coefCell_ = 0; coefEditing_ = false; sub_ = Sub::None;
    screen_ = Screen::Coef;
}
void Graph3DApp::coefKey(Context& ctx, const KeyEvent& ev) {
    const CoefLayout l = layoutOf(edit_);
    const int cells = l.rows * l.cols;
    coefCell_ = std::clamp(coefCell_, 0, cells - 1);
    const int r = coefCell_ / l.cols, c = coefCell_ % l.cols;
    auto& value = edit_.coef[static_cast<std::size_t>(l.index(r, c))];
    if (coefEditing_) {
        if (ev.is(Key::Exe)) {
            value = evalNumber(ctx, coefEditor_.text());           // Syntax / Math ERROR keep the editor open
            coefEditing_ = false;
            coefCell_ = (coefCell_ + 1) % cells;
        } else if (ev.is(Key::Exit)) {
            coefEditing_ = false;
        } else if (ev.is(Key::F6)) {
            value = evalNumber(ctx, coefEditor_.text());
            coefEditing_ = false;
            setFunction(ctx);
        } else {
            (void)coefEditor_.key(ev);
        }
        return;
    }
    const std::uint8_t forms = g3d::formCount(edit_.kind);
    const int editKey = forms > 1 ? forms + 1 : 0;                // EDIT after the forms (F5 / F4 / F3)
    switch (ev.key) {
    case Key::Left: if (c > 0) --coefCell_; return;
    case Key::Right: if (c + 1 < l.cols) ++coefCell_; return;
    case Key::Up: if (r > 0) coefCell_ -= l.cols; return;
    case Key::Down: if (r + 1 < l.rows) coefCell_ += l.cols; return;
    case Key::F1: case Key::F2: case Key::F3: case Key::F4: case Key::F5: {
        const int k = static_cast<int>(ev.key) - static_cast<int>(Key::F1) + 1;
        if (forms > 1 && k <= forms) { g3d::convertForm(edit_, static_cast<std::uint8_t>(k - 1)); coefCell_ = 0; return; }
        if (k == editKey) { coefEditor_.set(fmt(ctx, value)); coefEditing_ = true; }
        return;
    }
    case Key::F6: setFunction(ctx); return;                       // SET
    case Key::Exe:
        if (l.vertical || forms == 1) { coefEditor_.set(fmt(ctx, value)); coefEditing_ = true; }
        else coefCell_ = (coefCell_ + 1) % cells;
        return;
    case Key::Exit: case Key::AC: screen_ = Screen::List; return;
    case Key::Char:
        if (!ev.ctrl && static_cast<unsigned char>(ev.ch) >= 0x20) {
            coefEditor_.clear();
            (void)coefEditor_.key(ev);
            coefEditing_ = true;
        }
        return;
    default: return;
    }
}

// ---------------------------------------------------------------- direct input (16-12, 16-13)
void Graph3DApp::openInput(Context& ctx, g3d::Kind kind) {
    const auto& f = ctx.ws.graph3d.set.fns[static_cast<std::size_t>(row_)];
    if (f.kind == kind) {
        edit_ = f;
    } else {
        edit_ = g3d::emptyFunction(static_cast<std::size_t>(row_));
        edit_.line = f.line; edit_.area = f.area;
        edit_.kind = kind;
    }
    inputLine_ = 0; inputEditing_ = false; sub_ = Sub::None;
    screen_ = Screen::Input;
}
void Graph3DApp::beginInputEdit(Context& ctx, bool keep, char first) {
    editor_.setLinear(ctx.ws.settings.inputMode == Settings::InputMode::Line);
    editor_.clear();
    const std::string& text = edit_.expr[static_cast<std::size_t>(inputLine_)];
    if (keep && !text.empty()) editor_.setExpression(text);
    if (first) editor_.insert(first);
    inputEditing_ = true;
}
void Graph3DApp::commitInputLine(Context& ctx) {
    (void)ctx;
    const std::string text = editor_.empty() ? std::string() : editor_.toExpression();
    if (text.size() > g3d::MaxExpression) throw MathError("Memory ERROR: espressione troppo lunga");
    if (!text.empty()) (void)CompiledExpression(text, variableName(edit_.kind, 0));   // Syntax ERROR keeps the editor open
    edit_.expr[static_cast<std::size_t>(inputLine_)] = text;
    inputEditing_ = false;
    if (edit_.kind == g3d::Kind::Param && inputLine_ < 2) ++inputLine_;
}
void Graph3DApp::inputKey(Context& ctx, const KeyEvent& ev) {
    const int lines = edit_.kind == g3d::Kind::Param ? 3 : 1;
    if (inputEditing_) {
        if (ctx.shift() && ev.isChar('7')) return;
        switch (ev.key) {
        case Key::Exe: commitInputLine(ctx); return;
        case Key::F6: commitInputLine(ctx); setFunction(ctx); return;
        case Key::Exit: inputEditing_ = false; return;
        case Key::F1: editor_.insertText(edit_.kind == g3d::Kind::Param ? "S" : "X"); return;
        case Key::F2: if (edit_.kind == g3d::Kind::Z || edit_.kind == g3d::Kind::Param) editor_.insertText(edit_.kind == g3d::Kind::Param ? "T" : "Y"); return;
        case Key::F3: editor_.insertFraction(); return;
        case Key::F4: editor_.insertRadical(); return;
        case Key::F5: editor_.insertPower(); return;
        case Key::Left: editor_.moveLeft(); return;
        case Key::Right: editor_.moveRight(); return;
        case Key::Up: editor_.moveUp(); return;
        case Key::Down: editor_.moveDown(); return;
        case Key::Home: editor_.moveHome(); return;
        case Key::End: editor_.moveEnd(); return;
        case Key::Del: editor_.backspace(); return;
        case Key::DelForward: editor_.eraseForward(); return;
        case Key::AC: editor_.clear(); return;
        case Key::Char:
            if (ev.ctrl) {
                if ((ev.ch == 'v' || ev.ch == 'V') && ctx.clipboard) editor_.paste(*ctx.clipboard);
                else if (ev.ch == 'c' || ev.ch == 'C') { ctx.clipboard = editor_.copyTree(); ctx.toast("Copiato"); }
                return;
            }
            // X,θ,T: the variable of the graph (T in Param, 16-13).
            editor_.insert(ev.ch);
            return;
        default: return;
        }
    }
    if (sub_ == Sub::Rotate) {
        if (ev.is(Key::F1)) { sub_ = Sub::None; openInput(ctx, g3d::Kind::RotX); }
        else if (ev.is(Key::F2)) { sub_ = Sub::None; openInput(ctx, g3d::Kind::RotY); }
        else if (ev.is(Key::Exit) || ev.is(Key::AC)) sub_ = Sub::None;
        return;
    }
    switch (ev.key) {
    case Key::Up: inputLine_ = (inputLine_ + lines - 1) % lines; return;
    case Key::Down: inputLine_ = (inputLine_ + 1) % lines; return;
    case Key::Exe: case Key::Right: case Key::Left: beginInputEdit(ctx, true); return;
    case Key::F1: openSelect(ctx); return;
    case Key::F2: openInput(ctx, g3d::Kind::Z); return;
    case Key::F3: openInput(ctx, g3d::Kind::Param); return;
    case Key::F4: sub_ = Sub::Rotate; return;
    case Key::F6: setFunction(ctx); return;                       // SET
    case Key::Del: case Key::DelForward: edit_.expr[static_cast<std::size_t>(inputLine_)].clear(); return;
    case Key::Exit: case Key::AC: screen_ = Screen::List; return;
    case Key::Char:
        if (!ev.ctrl && static_cast<unsigned char>(ev.ch) >= 0x20) beginInputEdit(ctx, false, ev.ch);
        return;
    default: return;
    }
}

// SET (16-9, 16-12): the function goes to the list after the checks of the manual.
void Graph3DApp::setFunction(Context& ctx) {
    g3d::Function f = edit_;
    if (!g3d::isTemplate(f.kind)) {
        const bool empty = std::all_of(f.expr.begin(), f.expr.end(), [](const std::string& e) { return e.empty(); });
        if (empty) {                                              // nothing typed: the row stays empty
            const g3d::Color line = f.line, area = f.area;
            f = g3d::emptyFunction(static_cast<std::size_t>(row_));
            f.line = line; f.area = area;
        } else {
            for (std::size_t i = 0; i < 3; ++i)
                if (!f.expr[i].empty()) (void)CompiledExpression(f.expr[i], variableName(f.kind, 0));
        }
    }
    const std::string err = g3d::validate(f);
    if (!err.empty()) throw MathError(err);
    if (f.kind != g3d::Kind::None && ctx.ws.graph3d.set.fns[static_cast<std::size_t>(row_)].kind != f.kind) f.selected = true;
    ctx.ws.graph3d.set.fns[static_cast<std::size_t>(row_)] = f;
    changed(ctx);
    sub_ = Sub::None;
    screen_ = Screen::List;
}

// ---------------------------------------------------------------- 3D View Window (16-4, 16-5)
void Graph3DApp::loadWindowForm(Context& ctx, const g3d::Window& w) {
    auto num = [&](const char* label, double v) { return FormField{label, fmt(ctx, v), FormField::Type::Number, {}, 0}; };
    window_.fields = {num("Xmin", w.xmin), num(" max", w.xmax), num(" grid", w.xgrid), num("Ymin", w.ymin), num(" max", w.ymax),
                      num(" grid", w.ygrid), num("Zmin", w.zmin), num(" max", w.zmax), num("Smin", w.smin), num(" max", w.smax),
                      num(" grid", w.sgrid), num("Tmin", w.tmin), num(" max", w.tmax), num(" grid", w.tgrid),
                      num("Angle\xCE\xB8", w.theta), num("Angle\xCF\x86", w.phi)};
}
void Graph3DApp::openWindow(Context& ctx, bool fromPlot) {
    loadWindowForm(ctx, ctx.ws.graph3d.set.window);
    window_.selected = 0;
    windowFromPlot_ = fromPlot;
    sub_ = Sub::None;
    stopSampling();
    screen_ = Screen::Window;
}
g3d::Window Graph3DApp::windowFromForm(Context& ctx) const {
    g3d::Window w;
    double* values[] = {&w.xmin, &w.xmax, nullptr, &w.ymin, &w.ymax, nullptr, &w.zmin, &w.zmax, &w.smin, &w.smax, nullptr,
                        &w.tmin, &w.tmax, nullptr, &w.theta, &w.phi};
    int* grids[] = {nullptr, nullptr, &w.xgrid, nullptr, nullptr, &w.ygrid, nullptr, nullptr, nullptr, nullptr, &w.sgrid,
                    nullptr, nullptr, &w.tgrid, nullptr, nullptr};
    for (int i = 0; i < WCount; ++i) {
        const double v = window_.number(ctx, static_cast<std::size_t>(i));
        if (values[i]) *values[i] = v;
        else {
            if (!(v >= g3d::MinGrid && v <= g3d::MaxGrid) || v != std::floor(v)) throw MathError("Out of Domain: grid deve essere un intero da 2 a 50");
            *grids[i] = static_cast<int>(v);
        }
    }
    const std::string err = g3d::validate(w);
    if (!err.empty()) throw MathError(err);
    return w;
}
void Graph3DApp::applyWindow(Context& ctx) {
    const g3d::Window w = windowFromForm(ctx);
    g3d::Window& cur = ctx.ws.graph3d.set.window;
    const bool onlyView = w.xmin == cur.xmin && w.xmax == cur.xmax && w.ymin == cur.ymin && w.ymax == cur.ymax && w.zmin == cur.zmin &&
                          w.zmax == cur.zmax && w.smin == cur.smin && w.smax == cur.smax && w.tmin == cur.tmin && w.tmax == cur.tmax &&
                          w.xgrid == cur.xgrid && w.ygrid == cur.ygrid && w.sgrid == cur.sgrid && w.tgrid == cur.tgrid;
    if (w == cur) return;
    cur = w;
    ctx.markDirty();
    layerValid_ = false;
    if (!onlyView) samplesValid_ = false;                         // angles only: the lattices stay
    marks_.clear();
}
void Graph3DApp::windowKey(Context& ctx, const KeyEvent& ev) {
    if (sub_ == Sub::VMem) {
        if (ev.is(Key::F1)) { sub_ = Sub::None; ask(Ask::Kind::VMemStore, "STORE 3D V-Win (1-6):"); }
        else if (ev.is(Key::F2)) { sub_ = Sub::None; ask(Ask::Kind::VMemRecall, "RECALL 3D V-Win (1-6):"); }
        else if (ev.is(Key::Exit) || ev.is(Key::AC)) sub_ = Sub::None;
        return;
    }
    if (!window_.editing()) {
        if (ev.is(Key::Exit)) {
            applyWindow(ctx);                                     // Out of Domain / Invalid Setting keep the screen open
            if (windowFromPlot_) showPlot(ctx);
            else screen_ = Screen::List;
            return;
        }
        if (ev.is(Key::F1)) { loadWindowForm(ctx, g3d::Window{}); return; }   // INITIAL
        if (ev.is(Key::F4)) { sub_ = Sub::VMem; return; }                       // 3D-VMEM
    }
    (void)window_.key(ctx, ev);
}

// ---------------------------------------------------------------- Setup (16-1, 16-2, 16-15)
void Graph3DApp::openSetup(Context& ctx) {
    const Settings& s = ctx.ws.settings;
    const g3d::Setup& g = ctx.ws.graph3d.set.setup;
    setup_.fields = {
        choice("Input/Output", {"Math", "Line"}, s.inputMode == Settings::InputMode::Line ? 1 : 0),
        choice("Angle", {"Deg", "Rad", "Gra"}, s.angle == AngleMode::Degrees ? 0 : s.angle == AngleMode::Radians ? 1 : 2),
        choice("Display", displayChoices(), displayIndex(s.display)),
        choice("Plot/LineCol", colorChoices(7), static_cast<int>(g.plot)),
        choice("Axes", {"On", "Off"}, g.axes ? 0 : 1),
        choice("Box", {"On", "Off"}, g.box ? 0 : 1),
        choice("Label", {"On", "Off"}, g.label ? 0 : 1),
    };
    setup_.selected = 0;
    if (screen_ != Screen::Setup) setupReturn_ = screen_ == Screen::Window || screen_ == Screen::Format ? Screen::List : screen_;
    sub_ = Sub::None;
    screen_ = Screen::Setup;
}
void Graph3DApp::applySetup(Context& ctx) {
    Settings& s = ctx.ws.settings;
    g3d::Setup& g = ctx.ws.graph3d.set.setup;
    const auto& f = setup_.fields;
    const AngleMode angle = s.angle;
    s.inputMode = f[SInput].choice == 1 ? Settings::InputMode::Line : Settings::InputMode::Math;
    s.angle = f[SAngle].choice == 0 ? AngleMode::Degrees : f[SAngle].choice == 1 ? AngleMode::Radians : AngleMode::Gradians;
    s.display = displayFrom(f[SDisplay].choice);
    g.plot = static_cast<g3d::Color>(f[SPlotCol].choice);
    g.axes = f[SAxes].choice == 0;
    g.box = f[SBox].choice == 0;
    g.label = f[SLabel].choice == 0;
    ctx.ws.apply();
    ctx.markDirty();
    layerValid_ = false;
    if (s.angle != angle) samplesValid_ = false;                  // trigonometric functions change
}
void Graph3DApp::setupKey(Context& ctx, const KeyEvent& ev) {
    if (setup_.editing()) { (void)setup_.key(ctx, ev); return; }
    if (ev.is(Key::Exit) || ev.is(Key::Exe)) {
        applySetup(ctx);
        screen_ = setupReturn_;
        if (screen_ == Screen::Plot) showPlot(ctx);
        return;
    }
    (void)setup_.key(ctx, ev);
}

// ---------------------------------------------------------------- FORMAT (16-6, 16-7)
void Graph3DApp::openFormat(Context& ctx) {
    const auto& f = ctx.ws.graph3d.set.fns[static_cast<std::size_t>(row_)];
    format_.fields = {choice("Line Color", colorChoices(g3d::ColorCount), static_cast<int>(f.line)),
                      choice("Area Color", colorChoices(g3d::ColorCount), static_cast<int>(f.area))};
    format_.selected = 0;
    sub_ = Sub::None;
    screen_ = Screen::Format;
}
void Graph3DApp::formatKey(Context& ctx, const KeyEvent& ev) {
    if (ev.is(Key::Exit) || ev.is(Key::Exe)) {
        const auto line = static_cast<g3d::Color>(format_.fields[0].choice), area = static_cast<g3d::Color>(format_.fields[1].choice);
        if (line == g3d::Color::Clear && area == g3d::Color::Clear)
            throw MathError("Invalid Setting: Line Color e Area Color non possono essere entrambi Clear");
        auto& f = ctx.ws.graph3d.set.fns[static_cast<std::size_t>(row_)];
        if (f.line != line || f.area != area) { f.line = line; f.area = area; ctx.markDirty(); ++dataGen_; layerValid_ = false; }
        screen_ = Screen::List;
        return;
    }
    if (ev.is(Key::AC)) { screen_ = Screen::List; return; }
    (void)format_.key(ctx, ev);
}

// ---------------------------------------------------------------- 3D graph memory (16-7, 16-8)
void Graph3DApp::storeGraphMemory(Context& ctx, int n) {
    Graph3DState& g3 = ctx.ws.graph3d;
    std::string data = g3d::encode(g3.set);
    const std::size_t others = g3.memoryBytes() - g3.gmem[static_cast<std::size_t>(n - 1)].size();
    if (others + data.size() > limits::kGraph3DMemoryBytes) throw MathError("Memory ERROR: 3D G-Mem piene");
    g3.gmem[static_cast<std::size_t>(n - 1)] = std::move(data);   // an existing memory is replaced (16-8)
    ctx.markDirty();
    ctx.toast("3D G-Mem " + std::to_string(n) + " memorizzata");
}
void Graph3DApp::recallGraphMemory(Context& ctx, int n) {
    const std::string& data = ctx.ws.graph3d.gmem[static_cast<std::size_t>(n - 1)];
    if (data.empty()) throw MathError("Memory ERROR: 3D G-Mem " + std::to_string(n) + " vuota");
    g3d::GraphSet set;
    if (!g3d::decode(data, set)) throw MathError("Memory ERROR: 3D G-Mem " + std::to_string(n) + " danneggiata");
    ctx.ws.graph3d.set = std::move(set);                          // replaces the list, window and setup (16-8)
    changed(ctx);
    ctx.toast("3D G-Mem " + std::to_string(n) + " richiamata");
}

// ---------------------------------------------------------------- pop-up input
void Graph3DApp::ask(Ask::Kind kind, std::string prompt, std::string initial) {
    ask_.kind = kind;
    ask_.prompt = std::move(prompt);
    ask_.editor.set(std::move(initial));
}
void Graph3DApp::askKey(Context& ctx, const KeyEvent& ev) {
    if (ev.is(Key::Exit)) { ask_.kind = Ask::Kind::None; return; }
    if (ev.is(Key::Exe)) {
        const Ask::Kind kind = ask_.kind;
        const std::string text = ask_.editor.text();
        ask_.kind = Ask::Kind::None;
        if (text.empty() && kind != Ask::Kind::SketchText) return;
        answered(ctx, kind, text);
        return;
    }
    (void)ask_.editor.key(ev);
}
void Graph3DApp::answered(Context& ctx, Ask::Kind kind, const std::string& text) {
    switch (kind) {
    case Ask::Kind::GMemStore: storeGraphMemory(ctx, askNumber(ctx, text, 20)); return;
    case Ask::Kind::GMemRecall: recallGraphMemory(ctx, askNumber(ctx, text, 20)); return;
    case Ask::Kind::VMemStore: {
        const int n = askNumber(ctx, text, 6);
        Graph3DState& g3 = ctx.ws.graph3d;
        g3.vmem[static_cast<std::size_t>(n - 1)] = windowFromForm(ctx);
        g3.vmemUsed = static_cast<std::uint8_t>(g3.vmemUsed | (1u << (n - 1)));
        ctx.markDirty();
        ctx.toast("3D V-Win " + std::to_string(n) + " memorizzata");
        return;
    }
    case Ask::Kind::VMemRecall: {
        const int n = askNumber(ctx, text, 6);
        const Graph3DState& g3 = ctx.ws.graph3d;
        if (!((g3.vmemUsed >> (n - 1)) & 1u)) throw MathError("Memory ERROR: 3D V-Win " + std::to_string(n) + " vuota");
        loadWindowForm(ctx, g3.vmem[static_cast<std::size_t>(n - 1)]);
        return;
    }
    case Ask::Kind::PictStore: storePicture(ctx, askNumber(ctx, text, 20)); return;
    case Ask::Kind::Capture: writeCapture(ctx, askNumber(ctx, text, 20)); return;
    case Ask::Kind::TraceU: {
        askedU_ = evalNumber(ctx, text);
        const auto& f = ctx.ws.graph3d.set.fns[static_cast<std::size_t>(traceGraph_)];
        ask(Ask::Kind::TraceV, f.kind == g3d::Kind::Param ? "T:" : "Y:");
        return;
    }
    case Ask::Kind::TraceV: {
        double u0, u1, v0, v1; std::size_t nu, nv; bool oneD;
        if (!traceDomain(traceGraph_, u0, u1, v0, v1, nu, nv, oneD)) return;
        traceU_ = std::clamp(askedU_, std::min(u0, u1), std::max(u0, u1));
        traceV_ = std::clamp(evalNumber(ctx, text), std::min(v0, v1), std::max(v0, v1));
        return;
    }
    case Ask::Kind::CrossValue: {
        const g3d::Window& w = ctx.ws.graph3d.set.window;
        const double lo = crossAxis_ == g3d::Axis::X ? w.xmin : crossAxis_ == g3d::Axis::Y ? w.ymin : w.zmin;
        const double hi = crossAxis_ == g3d::Axis::X ? w.xmax : crossAxis_ == g3d::Axis::Y ? w.ymax : w.zmax;
        crossValue_ = std::clamp(evalNumber(ctx, text), lo, hi);
        return;
    }
    case Ask::Kind::SketchText:
        if (!text.empty()) marks_.push_back({px_, py_, true, text});
        return;
    case Ask::Kind::None: return;
    }
}
void Graph3DApp::drawAsk(gfx::Canvas& c) const {
    const int w = 260, h = 54, x = (ui::ScreenW - w) / 2, y = 120;
    c.rect(x + 3, y + 3, w, h, Faint, true);
    ui::panel(c, {x, y, w, h}, Bg, Ink);
    ui::text(c, x + 8, y + 6, ask_.prompt.c_str(), Accent, font::Face::Small);
    c.rect(x + 8, y + 22, w - 16, 22, SelectSoft, true);
    ask_.editor.draw(c, x + 12, y + 25, w - 24, Ink);
}

// ---------------------------------------------------------------- drawing
void Graph3DApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "3D GRAPH");
    switch (screen_) {
    case Screen::List: drawList(ctx, c); break;
    case Screen::Select: drawSelect(ctx, c); break;
    case Screen::Coef: drawCoef(ctx, c); break;
    case Screen::Input: drawInput(ctx, c); break;
    case Screen::Window: {
        ui::text(c, 8, ui::ContentTop + 4, "View Window", Accent);
        window_.draw(c, ui::ContentTop + 22, ui::ContentBottom - 4);
        if (sub_ == Sub::VMem) { const char* k[6] = {"STORE", "RECALL", nullptr, nullptr, nullptr, nullptr}; ui::softKeys(c, k); }
        else { const char* k[6] = {"INITIAL", nullptr, nullptr, "3D-VMEM", nullptr, nullptr}; ui::softKeys(c, k); }
        break;
    }
    case Screen::Setup: {
        ui::text(c, 8, ui::ContentTop + 4, "SET UP (3D Graph)", Accent);
        setup_.draw(c, ui::ContentTop + 22, ui::ContentBottom - 4);
        const char* k[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, k);
        break;
    }
    case Screen::Format: {
        char title[24];
        std::snprintf(title, sizeof title, "FORMAT %d:", row_ + 1);
        ui::text(c, 8, ui::ContentTop + 4, title, Accent);
        format_.draw(c, ui::ContentTop + 22, ui::ContentTop + 66);
        // Sample of the two colours (16-6, 16-7).
        const auto line = static_cast<g3d::Color>(format_.fields[0].choice), area = static_cast<g3d::Color>(format_.fields[1].choice);
        const int sy = ui::ContentTop + 80;
        if (area != g3d::Color::Clear) c.rect(40, sy, 240, 60, g3d::color565(area), true);
        if (line != g3d::Color::Clear)
            for (int k = 0; k <= 6; ++k) { c.vLine(40 + k * 40, sy, 60, g3d::color565(line)); if (k <= 3) c.hLine(40, sy + k * 20, 240, g3d::color565(line)); }
        ui::text(c, 8, ui::ContentBottom - 18, "OK:[EXIT]  Cancel:[AC]   \xE2\x97\x80\xE2\x96\xB6 colore", Muted, font::Face::Small);
        const char* k[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, k);
        break;
    }
    case Screen::Plot: drawPlot(ctx, c); break;
    }
    if (confirmDelete_) {
        ui::errorBox(c, "Delete?", "Eliminare la funzione?  F1:Yes  F6:No");
        const char* k[6] = {"Yes", nullptr, nullptr, nullptr, nullptr, "No"};
        ui::softKeys(c, k);
    }
    if (ask_.kind != Ask::Kind::None) drawAsk(c);
}

void Graph3DApp::drawList(Context& ctx, gfx::Canvas& c) {
    const auto& set = ctx.ws.graph3d.set;
    ui::text(c, 6, ui::ContentTop + 2, "3D Graph", Ink);
    const int top = ui::ContentTop + 20, rowH = 72;
    for (int i = 0; i < static_cast<int>(g3d::Functions); ++i) {
        const auto& f = set.fns[static_cast<std::size_t>(i)];
        const int y = top + i * rowH;
        const bool sel = i == row_;
        if (sel) c.rect(0, y, ui::ScreenW, rowH - 2, SelectSoft, true);
        const std::uint16_t color = graphColor(f);
        // "1:" with the colon highlighted when the graph is drawn (16-6).
        char num[4];
        std::snprintf(num, sizeof num, "%d", i + 1);
        ui::text(c, 4, y + 3, num, color);
        const bool on = f.selected && f.defined();
        if (on) c.rect(12, y + 2, 8, 17, color, true);
        ui::text(c, 12, y + 3, ":", on ? 0xFFFF : color);
        // [—] in the colour of the graph.
        ui::text(c, ui::ScreenW - 34, y + 3, "[", color);
        c.rect(ui::ScreenW - 26, y + 10, 12, 3, color, true);
        ui::text(c, ui::ScreenW - 12, y + 3, "]", color);
        const int x = 24, w = ui::ScreenW - x - 40;
        switch (f.kind) {
        case g3d::Kind::None: break;
        case g3d::Kind::Z: {
            ui::text(c, x, y + 3, "Z=", Ink);
            if (!f.expr[0].empty()) {
                NaturalEditor view;
                try { view.setExpression(f.expr[0]); } catch (...) { view.clear(); view.insertText("?"); }
                const auto sz = math::measure(view.root());
                auto l = math::layout(view.root(), x + 18, y + 11 - sz.height / 2 + sz.baseline, w - 18, rowH - 8);   // centred on "Z="
                math::draw(c, l, Ink, Cursor, Placeholder);
            }
            break;
        }
        case g3d::Kind::Param: {
            static const char* const names[3] = {"Xst=", "Yst=", "Zst="};
            for (int k = 0; k < 3; ++k) ui::textFit(c, x, y + 4 + k * 20, std::string(names[k]) + f.expr[static_cast<std::size_t>(k)], w, Ink);
            break;
        }
        case g3d::Kind::RotX: case g3d::Kind::RotY:
            ui::text(c, x, y + 3, g3d::kindName(f.kind), Ink);
            ui::textFit(c, x + 4, y + 24, "Y=" + f.expr[0], w - 4, Ink);
            break;
        default: {
            ui::text(c, x, y + 3, g3d::kindName(f.kind), Ink);
            const auto lines = templateLines(f);
            for (std::size_t k = 0; k < lines.size() && k < 3; ++k)
                ui::textFit(c, x + 4, y + 23 + static_cast<int>(k) * 14, lines[k], w + 30, Ink, font::Face::Small);
            break;
        }
        }
        c.hLine(0, y + rowH - 2, ui::ScreenW, Grid);
    }
    const g3d::Window& wv = set.window;
    char buf[200];
    std::snprintf(buf, sizeof buf, "X %g~%g  Y %g~%g  Z %g~%g  \xCE\xB8 %g \xCF\x86 %g", wv.xmin, wv.xmax, wv.ymin, wv.ymax, wv.zmin, wv.zmax,
                  wv.theta, wv.phi);
    ui::textFit(c, 6, ui::ContentBottom - 14, buf, ui::ScreenW - 12, Muted, font::Face::Small);
    if (sub_ == Sub::GMem) { const char* k[6] = {"STORE", "RECALL", nullptr, nullptr, nullptr, nullptr}; ui::softKeys(c, k); return; }
    const char* k[6] = {"SELECT", "DELETE", "TYPE", "3D-GMEM", nullptr, "DRAW"};
    ui::softKeys(c, k);
}

void Graph3DApp::drawSelect(Context& ctx, gfx::Canvas& c) {
    (void)ctx;
    ui::text(c, 6, ui::ContentTop + 2, "Select Template", Accent);
    const int top = ui::ContentTop + 22, rowH = 50;
    for (int i = 0; i < TemplateCount; ++i) {
        const int y = top + i * rowH;
        const bool sel = i == selectRow_;
        if (sel) c.rect(0, y, ui::ScreenW, rowH - 2, Select, true);
        ui::text(c, 10, y + 16, g3d::kindName(kTemplates[i]), sel ? SelectInk : Ink);
        // Icon drawn by the project (not the calculator's).
        const int ix = ui::ScreenW - 64, iy = y + 4, iw = 52, ih = 40;
        c.rect(ix, iy, iw, ih, Bg, true);
        c.rect(ix, iy, iw, ih, Faint, false);
        const std::uint16_t blue = gfx::rgb888To565(40, 90, 220), dark = gfx::rgb888To565(20, 40, 120);
        switch (kTemplates[i]) {
        case g3d::Kind::Line: c.line(ix + 6, iy + ih - 6, ix + iw - 6, iy + 6, gfx::rgb888To565(0, 160, 200)); c.line(ix + 6, iy + 8, ix + iw - 8, iy + ih - 8, dark); break;
        case g3d::Kind::Plane: {
            const gfx::Point p[4] = {{ix + 8, iy + 26}, {ix + 30, iy + 34}, {ix + 44, iy + 14}, {ix + 22, iy + 6}};
            c.polygon(p, 4, blue, true); c.polygon(p, 4, dark, false);
            break;
        }
        case g3d::Kind::Sphere:
            c.circle(ix + iw / 2, iy + ih / 2, 15, blue, true); c.circle(ix + iw / 2, iy + ih / 2, 15, dark, false);
            c.ellipse(ix + iw / 2, iy + ih / 2, 15, 5, dark, false); c.ellipse(ix + iw / 2, iy + ih / 2, 6, 15, dark, false);
            break;
        case g3d::Kind::Cylinder:
            c.rect(ix + 14, iy + 9, 24, 24, blue, true);
            c.ellipse(ix + 26, iy + 33, 12, 4, blue, true); c.ellipse(ix + 26, iy + 9, 12, 4, gfx::rgb888To565(120, 160, 240), true);
            c.ellipse(ix + 26, iy + 9, 12, 4, dark, false);
            break;
        case g3d::Kind::Cone: {
            const gfx::Point p[3] = {{ix + 26, iy + 4}, {ix + 12, iy + 32}, {ix + 40, iy + 32}};
            c.polygon(p, 3, blue, true);
            c.ellipse(ix + 26, iy + 32, 14, 4, blue, true); c.ellipse(ix + 26, iy + 32, 14, 4, dark, false);
            break;
        }
        default: break;
        }
    }
    if (sub_ == Sub::Rotate) { const char* k[6] = {"Rot X", "Rot Y", nullptr, nullptr, nullptr, nullptr}; ui::softKeys(c, k); return; }
    const char* k[6] = {"Template", "Z=", "Param", "Rotate", nullptr, nullptr};
    ui::softKeys(c, k);
}

void Graph3DApp::drawCoef(Context& ctx, gfx::Canvas& c) {
    const CoefLayout l = layoutOf(edit_);
    ui::text(c, 6, ui::ContentTop + 2, l.title, Accent);
    if (l.title2) ui::text(c, 6, ui::ContentTop + 20, l.title2, Accent);
    const int cells = l.rows * l.cols;
    coefCell_ = std::clamp(coefCell_, 0, cells - 1);
    const int top = ui::ContentTop + (l.title2 ? 44 : 28);
    if (l.vertical) {
        for (int r = 0; r < l.rows; ++r) {
            const int y = top + r * 26;
            const bool sel = r == coefCell_;
            if (sel) c.rect(0, y - 2, ui::ScreenW, 22, Select, true);
            ui::text(c, 10, y, l.rowLabel[r], sel ? SelectInk : Ink);
            ui::textFit(c, 110, y, ":" + fmt(ctx, edit_.coef[static_cast<std::size_t>(r)]), ui::ScreenW - 120, sel ? SelectInk : Ink);
        }
    } else {
        const int labelW = l.rowLabel[0] ? 26 : 8;
        const int cellW = std::min(74, (ui::ScreenW - labelW - 12) / l.cols);
        for (int r = 0; r < l.rows; ++r) {
            const int hr = l.headerRows > 1 ? r : 0;
            const int y = top + r * (l.headerRows > 1 ? 44 : 26) + (l.headerRows > 1 ? 16 : 18);
            if (r == 0 || l.headerRows > 1)
                for (int col = 0; col < l.cols; ++col) {
                    const int hx = labelW + col * cellW;
                    if (l.header[hr][col]) ui::textCenter(c, hx + cellW / 2, y - 18, l.header[hr][col], Ink, font::Face::Small);
                    c.hLine(hx + 4, y - 4, cellW - 8, Ink);
                }
            if (l.rowLabel[r]) ui::text(c, 4, y + 2, l.rowLabel[r], Ink);
            ui::text(c, labelW - 6, y + 1, "[", Muted);
            ui::text(c, labelW + l.cols * cellW - 2, y + 1, "]", Muted);
            for (int col = 0; col < l.cols; ++col) {
                const int cell = r * l.cols + col;
                const int hx = labelW + col * cellW;
                const bool sel = cell == coefCell_;
                if (sel) c.rect(hx + 2, y, cellW - 4, 18, Select, true);
                std::string v = shortNumber(edit_.coef[static_cast<std::size_t>(l.index(r, col))]);
                const int maxChars = std::max(1, (cellW - 8) / 8);
                if (static_cast<int>(v.size()) > maxChars) v = v.substr(0, static_cast<std::size_t>(maxChars - 1)) + "\xE2\x80\xA6";
                ui::textRight(c, hx + cellW - 4, y + 1, v.c_str(), sel ? SelectInk : Ink);   // right-aligned as on the calculator
            }
        }
    }
    // Value of the selected cell (bottom right), or the edit line.
    const int r = coefCell_ / l.cols, col = coefCell_ % l.cols;
    const double value = edit_.coef[static_cast<std::size_t>(l.vertical ? coefCell_ : l.index(r, col))];
    const int ey = ui::ContentBottom - 24;
    c.hLine(0, ey - 2, ui::ScreenW, Faint);
    if (coefEditing_) {
        c.rect(0, ey, ui::ScreenW, 22, StatusBg, true);
        coefEditor_.draw(c, 6, ey + 3, ui::ScreenW - 12, Ink);
    } else {
        ui::textRight(c, ui::ScreenW - 6, ey + 3, fmt(ctx, value).c_str(), Ink);
    }
    const std::uint8_t forms = g3d::formCount(edit_.kind);
    const char* k[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, "SET"};
    for (std::uint8_t i = 0; i < forms && forms > 1; ++i) k[i] = g3d::formName(edit_.kind, i);
    if (forms > 1) k[forms] = "EDIT";
    ui::softKeys(c, k);
    // The form in use is marked in the function menu.
    if (forms > 1) {
        const int tw = ui::ScreenW / 6;
        c.rect(edit_.form * tw + 2, ui::ScreenH - 3, tw - 4, 2, Shift, true);
    }
}

void Graph3DApp::drawInput(Context& ctx, gfx::Canvas& c) {
    (void)ctx;
    std::string title = "Graph Func :";
    title += g3d::kindName(edit_.kind);
    ui::text(c, 6, ui::ContentTop + 2, title.c_str(), Accent);
    static const char* const paramNames[3] = {"Xst=", "Yst=", "Zst="};
    const int lines = edit_.kind == g3d::Kind::Param ? 3 : 1;
    const int top = ui::ContentTop + 24, rowH = 34;
    for (int i = 0; i < lines; ++i) {
        const int y = top + i * rowH;
        const bool sel = i == inputLine_;
        if (sel) c.rect(0, y, ui::ScreenW, rowH - 2, inputEditing_ ? SelectSoft : Select, true);
        const char* name = edit_.kind == g3d::Kind::Param ? paramNames[i] : edit_.kind == g3d::Kind::Z ? "Z=" : "Y=";
        const std::uint16_t ink = sel && !inputEditing_ ? SelectInk : Ink;
        ui::text(c, 6, y + 8, name, ink);
        const int ex = 6 + font::measure(font::Face::Medium, name).width + 4;
        if (sel && inputEditing_) {
            const math::CursorTarget cur{editor_.activeRowPtr(), editor_.cursorIndex()};
            const auto sz = math::measure(editor_.root());
            auto l = math::layout(editor_.root(), ex, y + rowH / 2 - sz.height / 2 + sz.baseline, ui::ScreenW - ex - 4, rowH - 2, math::Align::Left, &cur);
            math::draw(c, l, Ink, Cursor, Placeholder);
        } else if (!edit_.expr[static_cast<std::size_t>(i)].empty()) {
            NaturalEditor view;
            try { view.setExpression(edit_.expr[static_cast<std::size_t>(i)]); } catch (...) { view.clear(); view.insertText("?"); }
            const auto sz = math::measure(view.root());
            auto l = math::layout(view.root(), ex, y + rowH / 2 - sz.height / 2 + sz.baseline, ui::ScreenW - ex - 4, rowH - 2);
            math::draw(c, l, ink, Cursor, Placeholder);
        }
    }
    const char* hint = edit_.kind == g3d::Kind::Param ? "Variabili S e T" : edit_.kind == g3d::Kind::Z ? "Variabili X e Y" : "Y=f(X): solido di rotazione";
    ui::text(c, 6, ui::ContentBottom - 18, hint, Muted, font::Face::Small);
    if (inputEditing_) {
        const bool param = edit_.kind == g3d::Kind::Param;
        const bool two = param || edit_.kind == g3d::Kind::Z;
        const char* k[6] = {param ? "S" : "X", two ? (param ? "T" : "Y") : nullptr, "a/b", "\xE2\x88\x9A\xE2\x96\xA1", "\xE2\x96\xA1^", "SET"};
        ui::softKeys(c, k);
        return;
    }
    if (sub_ == Sub::Rotate) { const char* k[6] = {"Rot X", "Rot Y", nullptr, nullptr, nullptr, nullptr}; ui::softKeys(c, k); return; }
    const char* k[6] = {"Template", "Z=", "Param", "Rotate", nullptr, "SET"};
    ui::softKeys(c, k);
}

} // namespace fx::app
