// GEOMETRY (v1.19.0, solo Pimoroni Pico Plus 2 W): controller della modalita'
// alla parita' del capitolo 14 del manuale fx-CG50 (descrizione del
// comportamento, nessun codice o grafica copiati; tabella di parita' in
// docs/reports/ANALISI_FONTI_v1.19.0.md). Tasti, menu a tendina F1-F6, strumenti
// di disegno, casella di misura (OPTN), moduli, animazioni e risolutore come
// step::Task con AC. Il nucleo puro e' in src/geometry*.cpp, il disegno in
// geometry_view.cpp.
#include "apps.hpp"
#include "file_store.hpp"
#include "geometry.hpp"
#include "geometry_ui.hpp"
#include "pict.hpp"
#include "step_task.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace geoui {
namespace {
const char* const kFile[] = {"New", "Open", "Save as", "Key Help", "Option"};
const char* const kView[] = {"Zoom Box", "Pan", "Scroll", "Zoom In", "Zoom Out", "Zoom to Fit", "Axes", "Grid"};
const char* const kEdit[] = {"Undo/Redo", "Select All", "Deselect All", "Select Figure", "Delete", "Clear All"};
const char* const kOption[] = {"Text", "Label", "Expression", "Number Format", "Clr Constraint", "Show All", "Hide", "Area Calc"};
const char* const kProps[] = {"to the front", "to the back", "All TEXT", "Fade I/O", "Store Picture", "Recall Picture", "Clear Picture", "Format"};
const char* const kDraw[] = {"Point", "Line Segment", "Infinite Line", "Ray", "Vector", "Circle", "Arc", "SemiCirc (Diam)", "3-Point Arc"};
const char* const kDrawSpec[] = {"Triangle", "Isosc Triangle", "Rectangle", "Square", "Polygon", "Regular n-gon", "Function f(x)"};
const char* const kConstruct[] = {"Perp Bisector", "Perpendicular", "Midpoint", "Intersection", "Angle Bisector", "Parallel", "Tangent", "Attached Angle"};
const char* const kTransform[] = {"Reflection", "Translation", "Trans(Sel Vec)", "Rotation", "Dilation", "Symmetry"};
const char* const kAnimate[] = {"Add Animation", "Replace Animation", "Trace", "Edit Animation", "Go (once)", "Go (repeat)", "Add Table", "Display Table"};
const char* const kStore[] = {"LIST", "STAT"};
const char* const kDelete[] = {"DELETE", "DEL-ALL"};
const char* const kFormats[] = {"Fix0", "Fix1", "Fix2", "Fix3", "Fix4", "Fix5", "Fix6", "Fix7", "Fix8", "Fix9", "Sci1", "Sci2", "Sci3", "Sci4",
                                "Sci5", "Sci6", "Sci7", "Sci8", "Sci9", "Sci10", "Norm"};
template <std::size_t N> constexpr int count(const char* const (&)[N]) { return static_cast<int>(N); }
const Tab kTFile[] = {{"File", kFile, count(kFile)}, {"View", kView, count(kView)}};
const Tab kTEdit[] = {{"Edit", kEdit, count(kEdit)}, {"Option", kOption, count(kOption)}, {"Properties", kProps, count(kProps)}};
const Tab kTDraw[] = {{"Draw", kDraw, count(kDraw)}, {"Draw Spec", kDrawSpec, count(kDrawSpec)}};
const Tab kTConstruct[] = {{"Construct", kConstruct, count(kConstruct)}};
const Tab kTTransform[] = {{"Transform", kTransform, count(kTransform)}};
const Tab kTAnimate[] = {{"Animate", kAnimate, count(kAnimate)}};
const Tab kTFormats[] = {{"Number Format", kFormats, count(kFormats)}};
const Tab kTStore[] = {{"STORE", kStore, count(kStore)}};
const Tab kTDelete[] = {{"DELETE", kDelete, count(kDelete)}};
const MenuDef kMenus[] = {{kTFile, 2}, {kTEdit, 3}, {kTDraw, 2}, {kTConstruct, 1}, {kTTransform, 1}, {kTAnimate, 1}, {kTFormats, 1}, {nullptr, 0}, {kTStore, 1}, {kTDelete, 1}};

// Barra F1-F6 della schermata principale, della casella di misura e delle schermate.
const FKeyPage kMainPage{{{"File", 0}, {"Edit", 1}, {"Draw", 2}, {"Constr", 3}, {"Transf", 4}, {"Anim", 5}}};
const FKeyMenu kMainBar{&kMainPage, 1, nullptr};
const FKeyPage kBoxPage{{{"TYPE", 10}, {"LOCK", 11}, {"PASTE", 12}, {"TABLE", 13}, {"LABEL", 14}, {"CLOSE", 15}}};
const FKeyMenu kBoxBar{&kBoxPage, 1, nullptr};

enum Ask : int { AskNone = 0, AskNgon, AskFunction, AskTransX, AskTransY, AskAngle, AskFactor, AskValue, AskFade, AskStorePict, AskRecallPict, AskList };
enum Confirm : int { ConfirmNone = 0, ConfirmNew, ConfirmOpen, ConfirmClearAll, ConfirmDeleteFile, ConfirmDelAnimations, ConfirmDelTable };

geo::State& st(Context& ctx) { return ctx.ws.geometry; }
// La vista si mappa sempre sull'area intera: la casella di misura copre solo il fondo.
int areaH(const View&) { return AreaH; }
bool isYesNo(geo::MType t) { return t == geo::MType::Incidence || t == geo::MType::Tangency || t == geo::MType::Congruence; }
// Passo del puntatore nel piano: un valore "rotondo" vicino a 6 pixel.
double niceStep(double scale) {
    const double raw = 6.0 / scale;
    const double p = std::pow(10.0, std::floor(std::log10(raw)));
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0}) if (m * p >= raw) return m * p;
    return 10 * p;
}
double snap(double v, double step) { const double r = std::round(v / step) * step; return std::abs(r) < step * 1e-9 ? 0.0 : r; }
geo::Box visibleBox(Context& ctx, const View& v) {
    double x0, y0, x1, y1;
    geo::toPlane(st(ctx).view, ui::ScreenW, areaH(v), 0, areaH(v), x0, y0);
    geo::toPlane(st(ctx).view, ui::ScreenW, areaH(v), ui::ScreenW, 0, x1, y1);
    return geo::Box{x0, y0, x1, y1};
}
// Istantanea per Undo/Redo, presa prima di ogni operazione che cambia la costruzione.
void before(Context& ctx, View& v) {
    v.undo.clear();
    geo::encode(st(ctx), v.undo);
    v.undoValid = true;
}
void changed(Context& ctx, View& v) {
    ctx.markDirty();
    v.traces.clear();
    v.traceIds.clear();
    // la selezione perde gli oggetti che non ci sono piu'
    v.sel.erase(std::remove_if(v.sel.begin(), v.sel.end(), [&](geo::Id id) { return !geo::find(st(ctx), id); }), v.sel.end());
}
void endTool(Context& ctx, View& v, bool removeCreated) {
    if (removeCreated && !v.created.empty()) { geo::remove(st(ctx), v.created); changed(ctx, v); }
    v.tool = Tool::None;
    v.picks.clear();
    v.created.clear();
    v.corner = false;
    v.pendingText.clear();
}
void startTool(Context& ctx, View& v, Tool t) {
    endTool(ctx, v, true);
    v.tool = t;
}
void select(View& v, geo::Id id) { if (id != geo::NoId && std::find(v.sel.begin(), v.sel.end(), id) == v.sel.end()) v.sel.push_back(id); }
void toggle(View& v, geo::Id id) {
    auto it = std::find(v.sel.begin(), v.sel.end(), id);
    if (it == v.sel.end()) v.sel.push_back(id); else v.sel.erase(it);
}
// Punto sotto il puntatore, oppure uno nuovo (legato alla curva sotto il puntatore, D01).
geo::Id pickPoint(Context& ctx, View& v) {
    geo::State& s = st(ctx);
    const double tol = pixelTolerance(ctx);
    const geo::Id hit = geo::hitTest(s, v.px, v.py, tol, true);
    if (hit != geo::NoId) return hit;
    const geo::Id curve = geo::hitCurve(s, v.px, v.py, tol);
    const geo::Id id = curve != geo::NoId ? geo::addPointOn(s, curve, v.px, v.py) : geo::addPoint(s, v.px, v.py);
    v.created.push_back(id);
    changed(ctx, v);
    return id;
}
geo::Id hitKind(Context& ctx, const View& v, bool (*pred)(geo::Kind)) {
    geo::State& s = st(ctx);
    const double tol = pixelTolerance(ctx);
    const geo::Id id = geo::hitTest(s, v.px, v.py, tol);
    const geo::Object* o = geo::find(s, id);
    if (o && pred(o->kind)) return id;
    const geo::Id c = geo::hitCurve(s, v.px, v.py, tol);
    o = geo::find(s, c);
    return o && pred(o->kind) ? c : geo::NoId;
}
bool isLineK(geo::Kind k) { return geo::isLineLike(k); }
bool isVectorK(geo::Kind k) { return k == geo::Kind::Vector; }
bool isTangentK(geo::Kind k) { return k == geo::Kind::Circle || k == geo::Kind::Arc || k == geo::Kind::Function; }
std::vector<geo::MSpec> boxSpecs(Context& ctx, const View& v) { return geo::available(st(ctx), v.sel); }
bool currentSpec(Context& ctx, const View& v, geo::MSpec& m) {
    const auto all = boxSpecs(ctx, v);
    if (all.empty()) return false;
    m = all[static_cast<std::size_t>(std::clamp(v.boxType, 0, static_cast<int>(all.size()) - 1))];
    return true;
}
std::string unitSuffix(const Context& ctx) { return ctx.ws.geometry.options.angle == 0 ? "gradi" : "radianti"; }

// ---- menu a tendina
void showMenu(View& v, int menu, int tab) {
    const MenuDef& d = menuDef(menu);
    if (!d.tabs) return;
    tab = std::clamp(tab, 0, d.count - 1);
    v.menu = menu;
    v.tab = tab;
    const Tab& t = d.tabs[tab];
    std::vector<std::string> items(t.items, t.items + t.count);
    // schede: il titolo le elenca, la corrente tra parentesi quadre (◀ ▶ per cambiarla)
    v.menuLabel.clear();
    for (int i = 0; i < d.count; ++i) v.menuLabel += (i ? "  " : "") + (i == tab ? "[" + std::string(d.tabs[i].name) + "]" : std::string(d.tabs[i].name));
    if (d.count > 1) v.menuLabel += "  \xE2\x97\x80\xE2\x96\xB6";
    v.shell.showMenu(v.menuLabel.c_str(), std::move(items), menu);
}
void openForm(View& v, FormKind kind, std::vector<FormField> fields) {
    v.form = Form{};
    v.form.fields = std::move(fields);
    v.formKind = kind;
    v.screen = Screen::Form;
}
void openTextForm(Context& ctx, View& v, FormKind kind, const std::string& label, const std::string& text) {
    openForm(v, kind, {textField(label, text)});
    (void)v.form.key(ctx, KeyEvent{Key::Exe});              // subito in modifica
}
std::vector<std::string> choicesOf(const char* const* names, int n) { return std::vector<std::string>(names, names + n); }
void openSetup(Context& ctx, View& v) {
    const geo::Options& o = st(ctx).options;
    std::vector<std::string> units;
    for (std::uint8_t u = 0; u < geo::LengthUnitCount; ++u) units.push_back(geo::lengthUnitName(u));
    openForm(v, FormKind::Setup, {choiceField("Number Format", choicesOf(kFormats, count(kFormats)), o.numFormat),
                                  choiceField("Angle", {"Deg", "Rad"}, o.angle),
                                  choiceField("Length Unit", units, o.lengthUnit),
                                  choiceField("Grid", {"Off", "On", "Line"}, static_cast<int>(o.grid)),
                                  numberField("Grid Space", formNumber(o.gridSpace)),
                                  choiceField("Axes", {"Off", "On", "Scale"}, static_cast<int>(o.axes))});
}
void openWindow(Context& ctx, View& v) {
    const geo::Box b = visibleBox(ctx, v);
    openForm(v, FormKind::Window, {numberField("Xmin", formNumber(b.x0)), numberField("Xmax", formNumber(b.x1)),
                                   numberField("Ymid", formNumber(st(ctx).view.cy))});
}
// FORMAT (SHIFT 5): solo le voci che valgono per la selezione.
void openFormat(Context& ctx, View& v) {
    if (v.sel.empty()) throw MathError("Argument ERROR: selezionare gli oggetti (FORMAT)");
    bool lines = false, areas = false, chars = false;
    const geo::Object* first = nullptr;
    for (geo::Id id : v.sel) {
        const geo::Object* o = geo::find(st(ctx), id);
        if (!o) continue;
        if (!first) first = o;
        lines = lines || geo::isCurve(o->kind) || o->kind == geo::Kind::Polygon || o->kind == geo::Kind::Point;
        areas = areas || o->kind == geo::Kind::Polygon || o->kind == geo::Kind::Circle;
        chars = true;
    }
    if (!first) return;
    std::vector<std::string> colors, areaColors{"Clear"};
    for (std::uint8_t i = 0; i < geo::ColorCount; ++i) { colors.push_back(paletteName(i)); areaColors.push_back(paletteName(i)); }
    std::vector<FormField> f;
    if (lines) {
        f.push_back(choiceField("Line Style", {"Normal", "Thick", "Thin", "Broken", "Dot"}, static_cast<int>(first->style.lineStyle)));
        f.push_back(choiceField("Line Color", colors, first->style.line));
    }
    if (areas) {
        f.push_back(choiceField("Area Color", areaColors, first->style.area == geo::NoArea ? 0 : first->style.area + 1));
        f.push_back(choiceField("Paint Style", {"Normal", "Lighter"}, first->style.paint));
    }
    if (chars) f.push_back(choiceField("Char Color", colors, first->style.chars));
    openForm(v, FormKind::Format, std::move(f));
}
int fieldChoice(const Form& f, const char* label, int fallback) {
    for (const auto& x : f.fields) if (x.label == label) return x.choice;
    return fallback;
}
// ---- lavori a passi
void clearJob(View& v) {
    v.anim.reset();
    v.solver.reset();
    v.job = Job::None;
    v.runner.reset();
}
void startJob(View& v, Job job, step::Task& task, step::Runner::Config cfg) {
    v.job = job;
    v.runner.reset();
    v.runner.start(task, cfg);
}
void startAnimation(Context& ctx, View& v, bool repeat) {
    geo::State& s = st(ctx);
    v.traces.clear();
    v.traceIds = s.traced;
    v.anim = std::make_unique<geo::AnimationTask>(s, repeat, !s.columns.empty());
    startJob(v, Job::Animate, *v.anim, step::Runner::Config{1, 0});
}
void startSolve(Context& ctx, View& v, const geo::Lock& l) {
    geo::State& s = st(ctx);
    v.jobStart.clear();
    geo::encode(s, v.jobStart);
    v.undo = v.jobStart;
    v.undoValid = true;
    geo::putLock(s, l);
    v.solver = std::make_unique<geo::SolveTask>(s, std::vector<geo::Id>{});
    startJob(v, Job::Solve, *v.solver, step::Runner::Config{400, 20});
}
void finishJob(Context& ctx, View& v) {
    geo::State& s = st(ctx);
    const Job job = v.job;
    if (job == Job::Animate || job == Job::Table) {
        if (v.anim) v.traces = v.anim->traces();
        if (!s.columns.empty()) ctx.markDirty();
        if (job == Job::Table) { v.screen = Screen::Table; v.tableRow = v.tableCol = v.tableFirst = 0; }
    } else if (job == Job::Solve) {
        if (!v.solver || !v.solver->ok()) {
            geo::State back;
            if (geo::decode(back, v.jobStart.data(), v.jobStart.size())) s = std::move(back);
            clearJob(v);
            throw MathError("Math ERROR: vincoli incompatibili");
        }
        changed(ctx, v);
        // D6: la figura che esce dalla finestra viene inquadrata
        const geo::Box b = visibleBox(ctx, v);
        bool outside = false;
        for (const auto& o : s.objects)
            if (o.g.ok && o.kind == geo::Kind::Point && (o.g.a[0] < b.x0 || o.g.a[0] > b.x1 || o.g.a[1] < b.y0 || o.g.a[1] > b.y1)) outside = true;
        if (outside) geo::fit(s, ui::ScreenW, areaH(v));
    }
    clearJob(v);
}
void stopJob(Context& ctx, View& v) {
    const Job job = v.job;
    v.runner.interrupt();                       // AnimationTask/SolveTask::cancelled rimette lo stato
    if (job == Job::Solve) {
        geo::State back;
        if (geo::decode(back, v.jobStart.data(), v.jobStart.size())) st(ctx) = std::move(back);
    }
    if (v.anim) v.traces = v.anim->traces();
    clearJob(v);
    if (job == Job::Solve || job == Job::Table) throw MathError("Break: calcolo interrotto (AC)");
}

// ---- Store Picture / sfondo
files::FileSystem* fileSystem(Context& ctx) {
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) throw MathError("Storage ERROR: archivio file non disponibile");
    return fs;
}
void checkStatus(files::Status st, const char* what) {
    if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + what + " (" + files::statusMessage(st) + ")");
}
void loadBackground(Context& ctx, View& v) {
    const geo::State& s = st(ctx);
    if (s.background.empty()) { v.background = pict::Image{}; v.backgroundPath.clear(); return; }
    if (s.background == v.backgroundPath && !v.background.empty()) { v.background.fade = s.fade; return; }
    v.background = pict::Image{};
    v.backgroundPath.clear();
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) return;
    std::vector<std::uint8_t> data;
    if (files::readAll(*fs, files::join(files::PictFolder, s.background), data, pict::maxEncodedBytes() + 256) != files::Status::Ok) return;
    pict::Image img;
    if (!pict::parse(data.data(), data.size(), img).empty()) return;
    v.background = std::move(img);
    v.background.fade = s.fade;
    v.backgroundPath = s.background;
}
void writeCapture(Context& ctx, View& v) {
    v.captureReady = false;
    const int n = v.pendingStore;
    v.pendingStore = 0;
    files::FileSystem* fs = fileSystem(ctx);
    checkStatus(fs->makeDirs(files::PictFolder), "cartella PICT");
    const auto data = pict::serialize(v.capture);
    checkStatus(files::writeAtomic(*fs, files::join(files::PictFolder, pict::memoryFileName(n)), data.data(), data.size()), "Store Picture");
    v.capture = pict::Image{};
    ctx.toast("Salvato in Pict " + std::to_string(n));
}

// ---- azioni dei menu
void menuAction(Context& ctx, View& v, int menu, int tab, int item);
void transformChosen(Context& ctx, View& v, geo::TType t) {
    if (v.sel.empty()) throw MathError("Argument ERROR: selezionare le figure da trasformare");
    v.ttype = t;
    switch (t) {
    case geo::TType::Reflection: startTool(ctx, v, Tool::Axis); break;
    case geo::TType::Translation: v.tool = Tool::None; v.shell.ask.open("Translation X:", AskTransX); break;
    case geo::TType::TransVec: startTool(ctx, v, Tool::Vector); break;
    default: startTool(ctx, v, Tool::Center); break;
    }
}
void constructChosen(Context& ctx, View& v, geo::Construct c) {
    geo::State& s = st(ctx);
    if (c == geo::Construct::Tangent) { startTool(ctx, v, Tool::Tangent); return; }
    if (c == geo::Construct::AttachedAngle) {
        int lines = 0;
        for (geo::Id id : v.sel) if (const geo::Object* o = geo::find(s, id)) lines += geo::isLineLike(o->kind);
        if (v.sel.size() != 2 || lines != 2) throw MathError("Argument ERROR: selezionare due lati");
        startTool(ctx, v, Tool::AnglePosition);
        return;
    }
    before(ctx, v);
    const auto made = geo::construct(s, c, v.sel, v.px, v.py);
    v.sel.clear();
    changed(ctx, v);
    (void)made;
}
void drawChosen(Context& ctx, View& v, int tab, int item) {
    if (tab == 0) {
        static const geo::Kind kinds[] = {geo::Kind::Segment, geo::Kind::Line, geo::Kind::Ray, geo::Kind::Vector};
        switch (item) {
        case 0: startTool(ctx, v, Tool::Point); break;
        case 1: case 2: case 3: case 4: startTool(ctx, v, Tool::Line); v.lineKind = kinds[item - 1]; break;
        case 5: startTool(ctx, v, Tool::Circle); break;
        case 6: startTool(ctx, v, Tool::Arc); break;
        case 7: startTool(ctx, v, Tool::Semicircle); break;
        default: startTool(ctx, v, Tool::Arc3); break;
        }
        return;
    }
    static const geo::Spec specs[] = {geo::Spec::Triangle, geo::Spec::Isosceles, geo::Spec::Rectangle, geo::Spec::Square};
    if (item <= 3) { startTool(ctx, v, Tool::Box); v.spec = specs[item]; return; }
    if (item == 4) { startTool(ctx, v, Tool::Polygon); return; }
    if (item == 5) { endTool(ctx, v, true); v.shell.ask.open("Number of sides (3-12):", AskNgon); return; }
    endTool(ctx, v, true);
    v.shell.ask.open("Y=", AskFunction);
}
std::vector<geo::Id> selectedOf(Context& ctx, const View& v, bool (*pred)(geo::Kind)) {
    std::vector<geo::Id> out;
    for (geo::Id id : v.sel) if (const geo::Object* o = geo::find(st(ctx), id)) if (pred(o->kind)) out.push_back(id);
    return out;
}
bool isPointK(geo::Kind k) { return k == geo::Kind::Point; }
bool isCurveK(geo::Kind k) { return geo::isCurve(k); }
bool isPastedK(geo::Kind k) { return k == geo::Kind::Measure || k == geo::Kind::Expression; }
bool isAreaK(geo::Kind k) { return k == geo::Kind::Polygon || k == geo::Kind::Circle; }
void animateChosen(Context& ctx, View& v, int item) {
    geo::State& s = st(ctx);
    switch (item) {
    case 0: case 1: {
        const auto pts = selectedOf(ctx, v, isPointK), cur = selectedOf(ctx, v, isCurveK);
        if (pts.size() != 1 || cur.size() != 1) throw MathError("Argument ERROR: selezionare un punto e il percorso");
        geo::addAnimation(s, pts[0], cur[0], item == 1);
        ctx.markDirty();
        ctx.toast("Animazione di " + geo::find(s, pts[0])->label);
        return;
    }
    case 2: geo::setTrace(s, selectedOf(ctx, v, isPointK)); ctx.markDirty(); return;
    case 3: v.screen = Screen::Animations; v.animRow = 0; return;
    case 4: case 5: startAnimation(ctx, v, item == 5); return;
    case 6: {
        geo::MSpec m;
        if (!currentSpec(ctx, v, m)) throw MathError("Argument ERROR: scegliere una misura nella casella (OPTN)");
        geo::addTableColumn(s, m);
        ctx.markDirty();
        ctx.toast("Colonna aggiunta alla tabella");
        return;
    }
    default:
        if (s.columns.empty()) throw MathError("Argument ERROR: nessuna colonna (Add Table)");
        if (s.tableRows == 0) {
            if (s.animations.empty()) throw MathError("Argument ERROR: nessuna animazione (Add Animation)");
            v.anim = std::make_unique<geo::AnimationTask>(s, false, true);
            startJob(v, Job::Table, *v.anim, step::Runner::Config{0, 20});
            return;
        }
        v.screen = Screen::Table;
        v.tableRow = v.tableCol = v.tableFirst = 0;
        return;
    }
}
void optionChosen(Context& ctx, View& v, int item) {
    geo::State& s = st(ctx);
    switch (item) {
    case 0: openTextForm(ctx, v, FormKind::Text, "Text", ""); return;
    case 1: {
        if (v.sel.size() != 1) throw MathError("Argument ERROR: selezionare un oggetto");
        openTextForm(ctx, v, FormKind::Label, "Label", geo::find(s, v.sel[0])->label);
        return;
    }
    case 2: openTextForm(ctx, v, FormKind::Expression, "Expression (@1, @2...)", ""); return;
    case 3: showMenu(v, MenuNumberFormat, 0); return;
    case 4: before(ctx, v); geo::clearConstraints(s); changed(ctx, v); ctx.toast("Vincoli tolti"); return;
    case 5: for (auto& o : s.objects) o.hidden = false; ctx.markDirty(); return;
    case 6: {
        if (v.sel.empty()) throw MathError("Argument ERROR: selezionare gli oggetti da nascondere");
        before(ctx, v);
        for (geo::Id id : v.sel) if (geo::Object* o = geo::find(s, id)) o->hidden = true;
        v.sel.clear();
        ctx.markDirty();
        return;
    }
    default: {
        // Area Calc (O08): le aree delle figure chiuse colorate scelte, incollate e sommate
        std::vector<geo::Id> figs;
        for (geo::Id id : selectedOf(ctx, v, isAreaK)) if (geo::find(s, id)->style.area != geo::NoArea) figs.push_back(id);
        if (figs.empty()) throw MathError("Argument ERROR: selezionare figure chiuse con Area Color (FORMAT)");
        before(ctx, v);
        std::string sum;
        double y = v.py;
        for (geo::Id id : figs) {
            const geo::Id m = geo::addMeasureText(s, geo::MSpec{geo::MType::Area, {id}}, v.px, y);
            y -= 16 / s.view.scale;
            sum += (sum.empty() ? "@" : "+@") + std::to_string(geo::find(s, m)->seq);
        }
        if (figs.size() > 1) geo::addExpression(s, v.px, y, sum);
        changed(ctx, v);
        return;
    }
    }
}
void propertiesChosen(Context& ctx, View& v, int item) {
    geo::State& s = st(ctx);
    switch (item) {
    case 0: case 1:
        if (v.sel.empty()) throw MathError("Argument ERROR: selezionare gli oggetti");
        before(ctx, v);
        if (item == 0) geo::toFront(s, v.sel); else geo::toBack(s, v.sel);
        ctx.markDirty();
        return;
    case 2: before(ctx, v); geo::textsToFront(s); ctx.markDirty(); return;
    case 3: v.shell.ask.open("Fade I/O (0-100%):", AskFade); return;
    case 4: v.shell.ask.open("Store In Picture Memory (1-20):", AskStorePict); return;
    case 5: v.shell.ask.open("Recall Picture (1-20):", AskRecallPict); return;
    case 6: s.background.clear(); loadBackground(ctx, v); ctx.markDirty(); return;
    default: openFormat(ctx, v); return;
    }
}
void editChosen(Context& ctx, View& v, int item) {
    geo::State& s = st(ctx);
    switch (item) {
    case 0: {
        if (!v.undoValid) throw MathError("Argument ERROR: nessuna operazione da annullare");
        std::vector<std::uint8_t> now;
        geo::encode(s, now);
        geo::State back;
        if (!geo::decode(back, v.undo.data(), v.undo.size())) throw MathError("Data ERROR: istantanea danneggiata");
        s = std::move(back);
        v.undo = std::move(now);               // di nuovo: Redo
        changed(ctx, v);
        return;
    }
    case 1: v.sel.clear(); for (const auto& o : s.objects) if (!o.hidden && o.kind != geo::Kind::Transform) v.sel.push_back(o.id); return;
    case 2: v.sel.clear(); return;
    case 3: {
        if (v.sel.empty()) throw MathError("Argument ERROR: selezionare un oggetto della figura");
        for (geo::Id id : geo::figureOf(s, v.sel)) select(v, id);
        return;
    }
    case 4: {
        if (v.sel.empty()) throw MathError("Argument ERROR: selezionare gli oggetti da cancellare");
        before(ctx, v);
        geo::remove(s, v.sel);
        v.sel.clear();
        changed(ctx, v);
        return;
    }
    default: v.shell.confirm.open("Cancellare tutto? (non si puo' annullare)", ConfirmClearAll); return;
    }
}
void newConstruction(Context& ctx, View& v) {
    geo::State& s = st(ctx);
    const geo::Options keep = s.options;
    geo::reset(s);
    s.options = keep;
    v.sel.clear();
    v.px = v.py = 0;
    v.undoValid = false;
    changed(ctx, v);
    loadBackground(ctx, v);
}
void fileChosen(Context& ctx, View& v, int tab, int item) {
    geo::State& s = st(ctx);
    if (tab == 1) {
        switch (item) {
        case 0: startTool(ctx, v, Tool::ZoomBox); return;
        case 1: startTool(ctx, v, Tool::Pan); return;
        case 2: startTool(ctx, v, Tool::Scroll); return;
        case 3: geo::zoom(s.view, 2); ctx.markDirty(); return;
        case 4: geo::zoom(s.view, 0.5); ctx.markDirty(); return;
        case 5: if (!geo::fit(s, ui::ScreenW, areaH(v))) ctx.toast("Nessun oggetto da inquadrare"); ctx.markDirty(); return;
        case 6: s.options.axes = static_cast<geo::Axes>((static_cast<int>(s.options.axes) + 1) % 3); ctx.markDirty(); return;
        default: s.options.grid = static_cast<geo::Grid>((static_cast<int>(s.options.grid) + 1) % 3); ctx.markDirty(); return;
        }
    }
    switch (item) {
    case 0:
        if (!s.objects.empty()) { v.shell.confirm.open("Nuova costruzione? (quella attuale si perde)", ConfirmNew); return; }
        newConstruction(ctx, v);
        return;
    case 1:
        if (!s.objects.empty()) { v.shell.confirm.open("Aprire un file? (la costruzione attuale si perde)", ConfirmOpen); return; }
        v.files.openList(ctx);
        return;
    case 2: v.files.openSaveAs(ctx, TypedFileMenu::stem(s.file)); return;
    case 3: v.screen = Screen::Help; v.helpPage = 0; return;
    default: openSetup(ctx, v); return;
    }
}
void menuAction(Context& ctx, View& v, int menu, int tab, int item) {
    switch (menu) {
    case MenuFile: fileChosen(ctx, v, tab, item); return;
    case MenuEdit:
        if (tab == 0) editChosen(ctx, v, item);
        else if (tab == 1) optionChosen(ctx, v, item);
        else propertiesChosen(ctx, v, item);
        return;
    case MenuDraw: drawChosen(ctx, v, tab, item); return;
    case MenuConstruct: constructChosen(ctx, v, static_cast<geo::Construct>(item)); return;
    case MenuTransform: transformChosen(ctx, v, static_cast<geo::TType>(item)); return;
    case MenuAnimate: animateChosen(ctx, v, item); return;
    case MenuNumberFormat: {
        geo::State& s = st(ctx);
        const auto pasted = selectedOf(ctx, v, isPastedK);
        if (pasted.empty()) s.options.numFormat = static_cast<std::uint8_t>(item);
        else for (geo::Id id : pasted) geo::find(s, id)->fmt = static_cast<std::uint8_t>(item);
        ctx.markDirty();
        return;
    }
    case MenuBoxType: v.boxType = item; v.boxSel = v.sel; return;
    case MenuStore: {
        geo::State& s = st(ctx);
        if (item == 0) { v.shell.ask.open("List (1-26):", AskList); return; }
        // STAT (A10): tutte le colonne in List 1... e Statistics
        const std::size_t cols = s.columns.size();
        for (std::size_t c = 0; c < cols && c < 26; ++c) {
            std::vector<double> col;
            for (std::size_t r = 0; r < s.tableRows; ++r) {
                const double x = s.table[r * cols + c];
                if (x == geo::NoValue) throw MathError("Math ERROR: valori indefiniti nella colonna " + std::to_string(c + 1));
                col.push_back(x);
            }
            ctx.ws.setList(c, std::move(col));
        }
        ctx.markDirty();
        ctx.open(AppId::Statistics);
        return;
    }
    case MenuDelete: {
        geo::State& s = st(ctx);
        if (item == 0) { geo::removeColumn(s, static_cast<std::size_t>(v.tableCol)); v.tableCol = std::max(0, std::min(v.tableCol, static_cast<int>(s.columns.size()) - 1)); ctx.markDirty(); }
        else v.shell.confirm.open("Cancellare tutte le colonne?", ConfirmDelTable);
        if (s.columns.empty()) v.screen = Screen::Main;
        return;
    }
    default: return;
    }
}

// ---- risposte alle richieste numeriche
std::vector<double> numbers(Context& ctx, const std::string& text) {
    std::vector<double> out;
    int depth = 0;
    std::string cur;
    for (char ch : text) {
        if (ch == '(' || ch == '[' || ch == '{') ++depth;
        if (ch == ')' || ch == ']' || ch == '}') --depth;
        if (ch == ',' && depth == 0) { out.push_back(evalNumber(ctx, cur)); cur.clear(); continue; }
        cur += ch;
    }
    if (!cur.empty()) out.push_back(evalNumber(ctx, cur));
    return out;
}
void answer(Context& ctx, View& v, int tag) {
    geo::State& s = st(ctx);
    const std::string text = v.shell.ask.text();
    if (text.empty()) return;
    switch (tag) {
    case AskNgon: {
        const double n = evalNumber(ctx, text);
        if (n != std::floor(n) || n < geo::MinNgon || n > geo::MaxNgon) throw MathError("Argument ERROR: da 3 a 12 lati");
        startTool(ctx, v, Tool::Box);
        v.spec = geo::Spec::Ngon;
        v.ngon = static_cast<int>(n);
        return;
    }
    case AskFunction: before(ctx, v); geo::addFunction(s, text); changed(ctx, v); return;
    case AskTransX: v.tp0 = evalNumber(ctx, text); v.shell.ask.open("Translation Y:", AskTransY); return;
    case AskTransY: {
        v.tp1 = evalNumber(ctx, text);
        before(ctx, v);
        geo::transform(s, geo::TType::Translation, v.sel, geo::NoId, v.tp0, v.tp1);
        v.sel.clear();
        changed(ctx, v);
        return;
    }
    case AskAngle: case AskFactor: {
        const double x = evalNumber(ctx, text);
        try { geo::transform(s, v.ttype, v.sel, v.picks.empty() ? geo::NoId : v.picks[0], x); }
        catch (...) { endTool(ctx, v, true); throw; }
        v.created.clear();
        endTool(ctx, v, false);
        v.sel.clear();
        changed(ctx, v);
        return;
    }
    case AskValue: {
        geo::MSpec m;
        if (!currentSpec(ctx, v, m)) return;
        const auto nums = numbers(ctx, text);
        const bool two = m.type == geo::MType::Coordinates || m.type == geo::MType::Components;
        if (nums.size() != (two ? 2u : 1u)) throw MathError(two ? "Syntax ERROR: scrivere x,y" : "Syntax ERROR: un numero");
        if (m.type == geo::MType::RotationAngle || m.type == geo::MType::DilationScale) {
            before(ctx, v);
            geo::setValue(s, m, nums[0]);
            changed(ctx, v);
            return;
        }
        startSolve(ctx, v, geo::prepareValue(s, m, nums[0], two ? nums[1] : 0));
        return;
    }
    case AskFade: {
        const double f = evalNumber(ctx, text);
        if (f != std::floor(f) || f < 0 || f > 100) throw MathError("Argument ERROR: da 0 a 100");
        s.fade = static_cast<std::uint8_t>(f);
        v.background.fade = s.fade;
        ctx.markDirty();
        return;
    }
    case AskStorePict: case AskRecallPict: {
        const double n = evalNumber(ctx, text);
        if (n != std::floor(n) || n < 1 || n > pict::PictMemories) throw MathError("Argument ERROR: Pict da 1 a 20");
        if (tag == AskStorePict) { (void)fileSystem(ctx); v.pendingStore = static_cast<int>(n); return; }
        files::FileSystem* fs = fileSystem(ctx);
        std::vector<std::uint8_t> data;
        checkStatus(files::readAll(*fs, files::join(files::PictFolder, pict::memoryFileName(static_cast<int>(n))), data, pict::maxEncodedBytes() + 256), "Pict");
        pict::Image img;
        if (!pict::parse(data.data(), data.size(), img).empty()) throw MathError("Data ERROR: immagine danneggiata");
        s.background = pict::memoryFileName(static_cast<int>(n));
        v.background = std::move(img);
        v.background.fade = s.fade;
        v.backgroundPath = s.background;
        ctx.markDirty();
        return;
    }
    case AskList: {
        const double n = evalNumber(ctx, text);
        if (n != std::floor(n) || n < 1 || n > 26) throw MathError("Argument ERROR: List da 1 a 26");
        const std::size_t cols = s.columns.size();
        if (v.tableCol < 0 || static_cast<std::size_t>(v.tableCol) >= cols) return;
        std::vector<double> col;
        for (std::size_t r = 0; r < s.tableRows; ++r) {
            const double x = s.table[r * cols + static_cast<std::size_t>(v.tableCol)];
            if (x == geo::NoValue) throw MathError("Math ERROR: valori indefiniti nella colonna");
            col.push_back(x);
        }
        ctx.ws.setList(static_cast<std::size_t>(n) - 1, std::move(col));
        ctx.markDirty();
        ctx.toast("Salvata in List " + std::to_string(static_cast<int>(n)));
        return;
    }
    default: return;
    }
}
void confirmed(Context& ctx, View& v, int tag, bool yes) {
    if (!yes) return;
    geo::State& s = st(ctx);
    switch (tag) {
    case ConfirmNew: newConstruction(ctx, v); return;
    case ConfirmOpen: v.files.openList(ctx); return;
    case ConfirmClearAll: {
        const geo::Options keep = s.options;
        const geo::View view = s.view;
        const std::string file = s.file;
        geo::reset(s);
        s.options = keep; s.view = view; s.file = file;
        v.sel.clear();
        v.undoValid = false;                 // Clear All non si annulla (E06)
        changed(ctx, v);
        return;
    }
    case ConfirmDeleteFile: v.files.remove(ctx, v.files.file()); ctx.toast("Cancellato " + v.files.file()); return;
    case ConfirmDelAnimations: s.animations.clear(); s.traced.clear(); ctx.markDirty(); return;
    case ConfirmDelTable: geo::clearTable(s); v.screen = Screen::Main; ctx.markDirty(); return;
    default: return;
    }
}

// ---- moduli
void formDone(Context& ctx, View& v) {
    geo::State& s = st(ctx);
    const Form& f = v.form;
    switch (v.formKind) {
    case FormKind::Setup: {
        const double gs = evalNumber(ctx, f.fields[4].text);
        if (!(gs >= 0.01 && gs <= 1000)) throw MathError("Argument ERROR: Grid Space da 0.01 a 1000");
        s.options.numFormat = static_cast<std::uint8_t>(f.fields[0].choice);
        s.options.angle = static_cast<std::uint8_t>(f.fields[1].choice);
        s.options.lengthUnit = static_cast<std::uint8_t>(f.fields[2].choice);
        s.options.grid = static_cast<geo::Grid>(f.fields[3].choice);
        s.options.gridSpace = gs;
        s.options.axes = static_cast<geo::Axes>(f.fields[5].choice);
        ctx.markDirty();
        break;
    }
    case FormKind::Window:
        geo::setWindow(s.view, ui::ScreenW, evalNumber(ctx, f.fields[0].text), evalNumber(ctx, f.fields[1].text), evalNumber(ctx, f.fields[2].text));
        ctx.markDirty();
        break;
    case FormKind::Format: {
        before(ctx, v);
        for (geo::Id id : v.sel) {
            geo::Object* o = geo::find(s, id);
            if (!o) continue;
            o->style.lineStyle = static_cast<geo::LineStyle>(fieldChoice(f, "Line Style", static_cast<int>(o->style.lineStyle)));
            o->style.line = static_cast<std::uint8_t>(fieldChoice(f, "Line Color", o->style.line));
            const int area = fieldChoice(f, "Area Color", o->style.area == geo::NoArea ? 0 : o->style.area + 1);
            if (o->kind == geo::Kind::Polygon || o->kind == geo::Kind::Circle) {
                o->style.area = area == 0 ? geo::NoArea : static_cast<std::uint8_t>(area - 1);
                o->style.paint = static_cast<std::uint8_t>(fieldChoice(f, "Paint Style", o->style.paint));
            }
            o->style.chars = static_cast<std::uint8_t>(fieldChoice(f, "Char Color", o->style.chars));
        }
        ctx.markDirty();
        break;
    }
    case FormKind::Animation: {
        const double steps = evalNumber(ctx, f.fields[0].text), times = evalNumber(ctx, f.fields[3].text);
        if (steps != std::floor(steps) || times != std::floor(times)) throw MathError("Argument ERROR: Steps e Times interi");
        geo::setAnimation(s, v.formIndex, static_cast<int>(steps), evalNumber(ctx, f.fields[1].text), evalNumber(ctx, f.fields[2].text), static_cast<int>(times));
        s.table.clear();
        s.tableRows = 0;
        ctx.markDirty();
        v.screen = Screen::Animations;
        v.formKind = FormKind::None;
        return;
    }
    case FormKind::Text: case FormKind::Expression: {
        const std::string t = f.fields[0].text;
        if (t.empty()) break;
        if (v.formKind == FormKind::Text && t.size() > geo::MaxText) throw MathError("Argument ERROR: al piu' 31 caratteri");
        startTool(ctx, v, v.formKind == FormKind::Text ? Tool::PlaceText : Tool::PlaceExpression);
        v.pendingText = t;
        break;
    }
    case FormKind::Label: {
        if (v.sel.size() != 1) break;
        before(ctx, v);
        geo::setValue(s, geo::MSpec{geo::MType::Label, {v.sel[0]}}, 0, 0, f.fields[0].text);
        ctx.markDirty();
        break;
    }
    default: break;
    }
    v.screen = Screen::Main;
    v.formKind = FormKind::None;
}
void formKey(Context& ctx, View& v, const KeyEvent& ev) {
    const bool single = v.formKind == FormKind::Text || v.formKind == FormKind::Label || v.formKind == FormKind::Expression;
    if (ev.is(Key::AC) && !v.form.editing()) { v.screen = v.formKind == FormKind::Animation ? Screen::Animations : Screen::Main; v.formKind = FormKind::None; return; }
    if (v.formKind == FormKind::Window && ev.is(Key::F1) && !v.form.editing()) {     // INITIAL
        geo::View init;
        st(ctx).view = init;
        openWindow(ctx, v);
        return;
    }
    if (ev.is(Key::Exit) && !v.form.editing()) { formDone(ctx, v); return; }
    if (single && ev.is(Key::Exit) && v.form.editing()) { v.screen = Screen::Main; v.formKind = FormKind::None; return; }
    const bool wasEditing = v.form.editing();
    (void)v.form.key(ctx, ev);
    if (single && wasEditing && ev.is(Key::Exe) && !v.form.editing()) formDone(ctx, v);
}

// ---- casella di misura (M01)
void boxKey(Context& ctx, View& v, const KeyEvent& ev) {
    geo::State& s = st(ctx);
    const auto specs = boxSpecs(ctx, v);
    const int n = static_cast<int>(specs.size());
    if (n > 0) v.boxType = std::clamp(v.boxType, 0, n - 1);
    const int a = v.shell.fkeys.key(ev, false);
    if (ev.is(Key::Exit)) { v.box = BoxState::Shown; v.shell.fkeys.set(&kMainBar); return; }
    if (ev.is(Key::Optn) || a == 15) { v.box = BoxState::Hidden; v.shell.fkeys.set(&kMainBar); return; }
    if (n == 0) return;
    const geo::MSpec m = specs[static_cast<std::size_t>(v.boxType)];
    if (ev.is(Key::Left)) { v.boxType = (v.boxType + n - 1) % n; return; }
    if (ev.is(Key::Right)) { v.boxType = (v.boxType + 1) % n; return; }
    switch (a) {
    case 10: {
        std::vector<std::string> items;
        for (const auto& x : specs) items.push_back(geo::mtypeName(x.type));
        v.menu = MenuBoxType;
        v.shell.showMenu("Measurement", std::move(items), MenuBoxType);
        return;
    }
    case 11:
        if (!geo::lockable(m.type)) throw MathError("Argument ERROR: misura non bloccabile");
        if (geo::isLocked(s, m)) { before(ctx, v); geo::unlock(s, m); ctx.markDirty(); return; }
        startSolve(ctx, v, geo::prepareLock(s, m));
        return;
    case 12: {
        if (m.type == geo::MType::Label || m.type == geo::MType::Equation || m.type == geo::MType::Expression) {
            if (m.type != geo::MType::Equation) throw MathError("Argument ERROR: misura da incollare");
        }
        before(ctx, v);
        const double off = 10 / s.view.scale;
        if (m.type == geo::MType::Equation) geo::addText(s, v.px + off, v.py - off, geo::measureValueText(s, m, s.options.numFormat).substr(0, geo::MaxText));
        else geo::addMeasureText(s, m, v.px + off, v.py - off);
        changed(ctx, v);
        return;
    }
    case 13: geo::addTableColumn(s, m); ctx.markDirty(); ctx.toast("Colonna aggiunta alla tabella"); return;
    case 14: {
        if (v.sel.size() != 1) throw MathError("Argument ERROR: selezionare un oggetto");
        openTextForm(ctx, v, FormKind::Label, "Label", geo::find(s, v.sel[0])->label);
        return;
    }
    default: break;
    }
    if (m.type == geo::MType::Label && (ev.is(Key::Exe) || ev.key == Key::Char)) {
        openTextForm(ctx, v, FormKind::Label, "Label", geo::find(s, m.ops[0])->label);
        return;
    }
    if (m.type == geo::MType::Expression && (ev.is(Key::Exe) || ev.key == Key::Char)) {
        if (v.sel.size() != 1) return;
        openTextForm(ctx, v, FormKind::Expression, "Expression", geo::find(s, v.sel[0])->text);
        v.formKind = FormKind::Expression;
        return;
    }
    if (ev.key == Key::Char || ev.is(Key::Exe)) {
        if (m.type == geo::MType::Equation || m.type == geo::MType::Perimeter || isYesNo(m.type)) return;
        v.shell.ask.open(std::string(geo::mtypeName(m.type)) + (m.type == geo::MType::Coordinates || m.type == geo::MType::Components ? " (x,y)=" : "="), AskValue,
                         ev.key == Key::Char ? ev.ch : 0);
    }
}

// ---- strumenti: EXE nella zona di disegno
void toolExe(Context& ctx, View& v) {
    geo::State& s = st(ctx);
    auto done = [&] { v.picks.clear(); v.created.clear(); changed(ctx, v); };
    auto addPick = [&](std::size_t need) {
        if (v.picks.empty()) before(ctx, v);
        const geo::Id p = pickPoint(ctx, v);
        if (!v.picks.empty() && v.picks.back() == p) return false;
        v.picks.push_back(p);
        return v.picks.size() == need;
    };
    switch (v.tool) {
    case Tool::Point: {
        const geo::Id hit = geo::hitTest(s, v.px, v.py, pixelTolerance(ctx), true);
        if (hit != geo::NoId) return;
        before(ctx, v);
        (void)pickPoint(ctx, v);
        v.created.clear();
        return;
    }
    case Tool::Line: if (addPick(2)) { geo::addLine(s, v.lineKind, v.picks[0], v.picks[1]); done(); } return;
    case Tool::Circle: if (addPick(2)) { geo::addCircle(s, v.picks[0], v.picks[1]); done(); } return;
    case Tool::Semicircle: if (addPick(2)) { geo::addSemicircle(s, v.picks[0], v.picks[1]); done(); } return;
    case Tool::Arc3: if (addPick(3)) { geo::addArc3(s, v.picks[0], v.picks[1], v.picks[2]); done(); } return;
    case Tool::Arc:
        if (v.picks.size() < 2) { (void)addPick(3); return; }
        geo::addArc(s, v.picks[0], v.picks[1], v.px, v.py);
        done();
        return;
    case Tool::Box: case Tool::ZoomBox:
        if (!v.corner) { v.corner = true; v.cx0 = v.px; v.cy0 = v.py; return; }
        v.corner = false;
        if (v.tool == Tool::ZoomBox) { geo::zoomBox(s.view, ui::ScreenW, areaH(v), v.cx0, v.cy0, v.px, v.py); ctx.markDirty(); endTool(ctx, v, false); return; }
        before(ctx, v);
        geo::addSpec(s, v.spec, geo::Box{v.cx0, v.cy0, v.px, v.py}, visibleBox(ctx, v), v.ngon);
        changed(ctx, v);
        return;
    case Tool::Polygon: {
        if (v.picks.empty()) before(ctx, v);
        const geo::Id hit = geo::hitTest(s, v.px, v.py, pixelTolerance(ctx), true);
        if (hit != geo::NoId && v.picks.size() >= 3 && hit == v.picks[0]) { geo::addPolygon(s, v.picks, true); done(); return; }
        if (hit != geo::NoId && std::find(v.picks.begin(), v.picks.end(), hit) != v.picks.end()) return;
        if (v.picks.size() >= geo::MaxVertices) throw MathError("Argument ERROR: al piu' 50 vertici");
        v.picks.push_back(pickPoint(ctx, v));
        return;
    }
    case Tool::Tangent: {
        geo::Id curve = geo::NoId;
        for (geo::Id id : v.sel) if (const geo::Object* o = geo::find(s, id)) if (isTangentK(o->kind)) curve = id;
        if (curve == geo::NoId) curve = hitKind(ctx, v, isTangentK);
        if (curve == geo::NoId) throw MathError("Argument ERROR: Select Curve (circonferenza, arco o funzione)");
        before(ctx, v);
        geo::construct(s, geo::Construct::Tangent, {curve}, v.px, v.py);
        endTool(ctx, v, false);
        v.sel.clear();
        changed(ctx, v);
        return;
    }
    case Tool::AnglePosition:
        before(ctx, v);
        geo::construct(s, geo::Construct::AttachedAngle, v.sel, v.px, v.py);
        endTool(ctx, v, false);
        v.sel.clear();
        changed(ctx, v);
        return;
    case Tool::Axis: case Tool::Vector: {
        const geo::Id ref = hitKind(ctx, v, v.tool == Tool::Axis ? isLineK : isVectorK);
        if (ref == geo::NoId) throw MathError(v.tool == Tool::Axis ? "Argument ERROR: Select Axis" : "Argument ERROR: Select Vector");
        before(ctx, v);
        geo::transform(s, v.ttype, v.sel, ref);
        endTool(ctx, v, false);
        v.sel.clear();
        changed(ctx, v);
        return;
    }
    case Tool::Center: {
        before(ctx, v);
        geo::Id c = geo::hitTest(s, v.px, v.py, pixelTolerance(ctx), true);
        if (c == geo::NoId) { c = geo::addPoint(s, v.px, v.py); v.created.push_back(c); changed(ctx, v); }
        v.picks = {c};
        if (v.ttype == geo::TType::Symmetry) {
            try { geo::transform(s, v.ttype, v.sel, c); } catch (...) { endTool(ctx, v, true); throw; }
            v.created.clear();
            endTool(ctx, v, false);
            v.sel.clear();
            changed(ctx, v);
            return;
        }
        if (v.ttype == geo::TType::Rotation) v.shell.ask.open("Rotation angle (" + unitSuffix(ctx) + "):", AskAngle);
        else v.shell.ask.open("Scale factor (0.1-10):", AskFactor);
        return;
    }
    case Tool::PlaceText: case Tool::PlaceExpression: {
        before(ctx, v);
        if (v.tool == Tool::PlaceText) geo::addText(s, v.px, v.py, v.pendingText);
        else geo::addExpression(s, v.px, v.py, v.pendingText);
        endTool(ctx, v, false);
        changed(ctx, v);
        return;
    }
    case Tool::Pan: case Tool::Scroll: endTool(ctx, v, false); return;
    case Tool::Move: v.moveStart.clear(); endTool(ctx, v, false); return;
    default: return;
    }
}
// Puntatore con la vista che lo segue.
void movePointer(Context& ctx, View& v, double dx, double dy) {
    geo::State& s = st(ctx);
    const double step = niceStep(s.view.scale);
    v.px = snap(v.px + dx, step);
    v.py = snap(v.py + dy, step);
    // il puntatore resta nella parte visibile: la vista lo segue
    double x0, y0, x1, y1;
    geo::toPlane(s.view, ui::ScreenW, AreaH, 0, areaBottom(v) - AreaTop, x0, y0);
    geo::toPlane(s.view, ui::ScreenW, AreaH, ui::ScreenW, 0, x1, y1);
    if (v.px < x0) s.view.cx += v.px - x0; else if (v.px > x1) s.view.cx += v.px - x1;
    if (v.py < y0) s.view.cy += v.py - y0; else if (v.py > y1) s.view.cy += v.py - y1;
}
} // namespace

// ---- definizioni condivise con geometry_view.cpp
const MenuDef& menuDef(int menu) noexcept {
    static const MenuDef none{nullptr, 0};
    return menu >= 0 && menu < static_cast<int>(sizeof kMenus / sizeof kMenus[0]) ? kMenus[menu] : none;
}
int areaBottom(const View& v) noexcept { return ui::ContentBottom - (v.box != BoxState::Hidden ? BoxH : 0); }
double pixelTolerance(const Context& ctx) noexcept { return 6.0 / ctx.ws.geometry.view.scale; }
void toScreen(const Context& ctx, double x, double y, int& sx, int& sy) {
    double fx, fy;
    geo::toScreen(ctx.ws.geometry.view, ui::ScreenW, AreaH, x, y, fx, fy);
    sx = static_cast<int>(std::lround(std::clamp(fx, -1e6, 1e6)));
    sy = AreaTop + static_cast<int>(std::lround(std::clamp(fy, -1e6, 1e6)));
}
std::string formatName(std::uint8_t fmt) { return fmt <= geo::FmtNorm ? kFormats[fmt] : "?"; }
} // namespace geoui

using namespace geoui;

// Lo stato della schermata e' quello di geometry_ui.hpp (condiviso con il disegno).
struct GeometryApp::View : geoui::View {};

GeometryApp::GeometryApp() = default;
GeometryApp::~GeometryApp() = default;

void GeometryApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    View& v = *v_;
    geo::State& s = ctx.ws.geometry;
    v.px = snap(s.view.cx, niceStep(s.view.scale));
    v.py = snap(s.view.cy, niceStep(s.view.scale));
    v.shell.fkeys.set(&kMainBar);
    geo::recompute(s);
    try { loadBackground(ctx, v); } catch (const std::exception&) {}
}
void GeometryApp::leave(Context&) { v_.reset(); }

bool GeometryApp::tick(Context& ctx) {
    if (!v_) return false;
    View& v = *v_;
    if (v.captureReady) { writeCapture(ctx, v); return true; }
    if (!v.busy()) return false;
    if (v.runner.running()) {
        // animazione: un fotogramma per tick (la velocita' e' quella del disegno)
        try { v.runner.tick([&ctx] { return ctx.now(); }); }
        catch (...) { clearJob(v); throw; }
        if (v.anim) v.traces = v.anim->traces();
    }
    if (v.runner.state() == step::State::Done || v.runner.state() == step::State::Failed) finishJob(ctx, v);
    return true;
}

void GeometryApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    geo::State& s = ctx.ws.geometry;
    // lavoro in corso: AC o EXIT lo fermano (A06, A12)
    if (v.busy()) {
        if (ev.is(Key::AC) || ev.is(Key::Exit)) stopJob(ctx, v);
        return;
    }
    if (v.files.active()) {
        // DELETE nell'elenco di Open (F1), con conferma
        if (v.files.purpose() == TypedFileMenu::Purpose::Open && ev.is(Key::F1)) { v.files.openList(ctx, TypedFileMenu::Purpose::Delete); return; }
        if (v.files.key(ev) != 1) return;
        if (v.files.purpose() == TypedFileMenu::Purpose::Delete) { v.shell.confirm.open("Cancellare " + v.files.file() + "?", ConfirmDeleteFile); return; }
        if (v.files.purpose() == TypedFileMenu::Purpose::Open) {
            const auto data = v.files.load(ctx, v.files.file());
            geo::State t;
            t.file = v.files.file();
            if (!geo::decodeFile(data.data(), data.size(), t)) throw MathError("Data ERROR: costruzione danneggiata o troppo grande");
            s = std::move(t);
            v.sel.clear();
            v.undoValid = false;
            v.px = snap(s.view.cx, niceStep(s.view.scale));
            v.py = snap(s.view.cy, niceStep(s.view.scale));
            loadBackground(ctx, v);
            changed(ctx, v);
        } else {
            v.files.save(ctx, v.files.file(), geo::encodeFile(s));
            s.file = v.files.file();
            ctx.toast("Salvato " + s.file);
            ctx.markDirty();
        }
        return;
    }
    if (v.shell.modalOpen()) {
        // schede dei menu a tendina (◀ ▶)
        if (v.shell.menu.open && v.menu >= 0 && (ev.is(Key::Left) || ev.is(Key::Right))) {
            const MenuDef& d = menuDef(v.menu);
            if (d.count > 1) { showMenu(v, v.menu, (v.tab + (ev.is(Key::Left) ? d.count - 1 : 1)) % d.count); return; }
            if (ev.is(Key::Left)) { v.shell.menu.close(); v.menu = MenuNone; return; }
        }
        (void)v.shell.modalKey(ev, [&](int tag, bool yes) { confirmed(ctx, v, tag, yes); }, [&](int tag) { answer(ctx, v, tag); },
                               [&](int tag, int choice) { const int tab = v.tab; v.menu = MenuNone; menuAction(ctx, v, tag, tab, choice); });
        if (!v.shell.menu.open) v.menu = MenuNone;
        if (!v.shell.ask.active() && (v.tool == Tool::Center) && !v.picks.empty() && v.ttype != geo::TType::Symmetry && ev.is(Key::Exit)) endTool(ctx, v, true);
        return;
    }
    switch (v.screen) {
    case Screen::Form: formKey(ctx, v, ev); return;
    case Screen::Help:
        if (ev.is(Key::Left) || ev.is(Key::Up)) v.helpPage = (v.helpPage + HelpPages - 1) % HelpPages;
        else if (ev.is(Key::Right) || ev.is(Key::Down)) v.helpPage = (v.helpPage + 1) % HelpPages;
        else if (ev.is(Key::Exit) || ev.is(Key::AC)) v.screen = Screen::Main;
        return;
    case Screen::Animations: {
        const int n = static_cast<int>(s.animations.size());
        if (ev.is(Key::Exit) || ev.is(Key::AC)) { v.screen = Screen::Main; return; }
        if (n == 0) return;
        if (ev.is(Key::Up)) v.animRow = (v.animRow + n - 1) % n;
        else if (ev.is(Key::Down)) v.animRow = (v.animRow + 1) % n;
        else if (ev.is(Key::Exe) || ev.is(Key::F1)) {
            const geo::Animation& a = s.animations[static_cast<std::size_t>(v.animRow)];
            v.formIndex = static_cast<std::size_t>(v.animRow);
            openForm(v, FormKind::Animation, {numberField("Steps", std::to_string(a.steps)), numberField("t0", formNumber(a.t0)),
                                              numberField("t1", formNumber(a.t1)), numberField("Times", std::to_string(a.times))});
        } else if (ev.is(Key::F5) || ev.is(Key::Del)) {
            geo::removeAnimation(s, static_cast<std::size_t>(v.animRow));
            v.animRow = std::max(0, std::min(v.animRow, n - 2));
            ctx.markDirty();
        } else if (ev.is(Key::F6)) v.shell.confirm.open("Cancellare tutte le animazioni?", ConfirmDelAnimations);
        return;
    }
    case Screen::Table: {
        const int cols = static_cast<int>(s.columns.size()), rows = s.tableRows;
        if (ev.is(Key::Exit) || ev.is(Key::AC)) { v.screen = Screen::Main; return; }
        if (cols == 0) { v.screen = Screen::Main; return; }
        if (ev.is(Key::Left)) v.tableCol = std::max(0, v.tableCol - 1);
        else if (ev.is(Key::Right)) v.tableCol = std::min(cols - 1, v.tableCol + 1);
        else if (ev.is(Key::Up)) v.tableRow = std::max(0, v.tableRow - 1);
        else if (ev.is(Key::Down)) v.tableRow = std::min(std::max(0, rows - 1), v.tableRow + 1);
        else if (ev.is(Key::F1)) showMenu(v, MenuStore, 0);
        else if (ev.is(Key::F2)) showMenu(v, MenuDelete, 0);
        return;
    }
    default: break;
    }
    if (v.sel != v.boxSel) { v.boxSel = v.sel; v.boxType = 0; }   // selezione nuova: il primo tipo di misura
    if (v.box == BoxState::Focused) { boxKey(ctx, v, ev); return; }
    // tasti con SHIFT: V-Window (SHIFT F3), FORMAT (SHIFT 5); SET UP (SHIFT MENU)
    if (ev.is(Key::Setup)) { endTool(ctx, v, true); openSetup(ctx, v); return; }
    if (ctx.shift() && ev.is(Key::F3)) { endTool(ctx, v, true); openWindow(ctx, v); return; }
    if (ctx.shift() && ev.isChar('5')) { openFormat(ctx, v); return; }
    const int a = v.shell.fkeys.key(ev, false);
    if (a >= 0 && a <= 5) {
        if (v.tool == Tool::Polygon && v.picks.size() >= 2) { geo::addPolygon(s, v.picks, false); v.picks.clear(); v.created.clear(); changed(ctx, v); }
        if (v.tool != Tool::Tangent && v.tool != Tool::AnglePosition) endTool(ctx, v, true);
        showMenu(v, a, 0);
        return;
    }
    if (a == FKeyBar::Handled) return;
    if (ev.is(Key::Optn)) { v.box = BoxState::Focused; v.shell.fkeys.set(&kBoxBar); return; }
    // frecce
    const double step = niceStep(s.view.scale) * (ctx.shift() ? 4 : 1);
    double dx = 0, dy = 0;
    if (ev.is(Key::Left)) dx = -step;
    else if (ev.is(Key::Right)) dx = step;
    else if (ev.is(Key::Up)) dy = step;
    else if (ev.is(Key::Down)) dy = -step;
    if (dx != 0 || dy != 0) {
        if (v.tool == Tool::Pan || v.tool == Tool::Scroll) { s.view.cx += dx; s.view.cy += dy; v.px += dx; v.py += dy; ctx.markDirty(); return; }
        if (v.tool == Tool::Move) {
            if (geo::translate(s, v.sel, dx, dy)) { v.px += dx; v.py += dy; changed(ctx, v); }
            return;
        }
        movePointer(ctx, v, dx, dy);
        return;
    }
    if (ev.key == Key::Char) {
        const char c = ev.ch;
        if (c >= '1' && c <= '9') {
            // salto al settore 3x3 (disposizione del tastierino: 7 8 9 in alto)
            const int k = c - '1', col = k % 3, row = 2 - k / 3;
            double x, y;
            geo::toPlane(s.view, ui::ScreenW, areaH(v), (col + 0.5) * ui::ScreenW / 3.0, (row + 0.5) * areaH(v) / 3.0, x, y);
            const double st0 = niceStep(s.view.scale);
            v.px = snap(x, st0);
            v.py = snap(y, st0);
            return;
        }
        switch (c) {
        case '+': geo::zoom(s.view, 2); ctx.markDirty(); return;
        case '-': geo::zoom(s.view, 0.5); ctx.markDirty(); return;
        case '.': startTool(ctx, v, Tool::Scroll); return;
        case 'x': case 'X': editChosen(ctx, v, 3); return;
        case '*': editChosen(ctx, v, 0); return;
        case 'v': case 'V':
            if (v.sel.empty()) { startTool(ctx, v, Tool::Pan); return; }
            before(ctx, v);
            v.moveStart = v.undo;
            startTool(ctx, v, Tool::Move);
            return;
        default: return;
        }
    }
    if (ev.is(Key::Exe)) {
        if (v.tool != Tool::None) { toolExe(ctx, v); return; }
        int sx, sy;
        toScreen(ctx, v.px, v.py, sx, sy);
        geo::Id hit = hitText(ctx, v, sx, sy);
        if (hit == geo::NoId) hit = geo::hitTest(s, v.px, v.py, pixelTolerance(ctx));
        if (hit != geo::NoId) toggle(v, hit);
        return;
    }
    if (ev.is(Key::Del) || ev.is(Key::DelForward)) { if (v.tool == Tool::None && !v.sel.empty()) editChosen(ctx, v, 4); return; }
    if (ev.is(Key::AC)) {
        if (v.tool != Tool::None) {
            if (v.tool == Tool::Move && !v.moveStart.empty()) { geo::State back; if (geo::decode(back, v.moveStart.data(), v.moveStart.size())) s = std::move(back); v.moveStart.clear(); changed(ctx, v); }
            endTool(ctx, v, true);
            return;
        }
        v.sel.clear();
        return;
    }
    if (ev.is(Key::Exit)) {
        if (v.tool == Tool::Polygon && v.picks.size() >= 2) {   // spezzata aperta (D14)
            geo::addPolygon(s, v.picks, false);
            v.picks.clear();
            v.created.clear();
            changed(ctx, v);
            endTool(ctx, v, false);
            return;
        }
        if (v.tool == Tool::Move) { v.moveStart.clear(); endTool(ctx, v, false); return; }
        if (v.tool != Tool::None) { endTool(ctx, v, true); return; }
        if (v.box == BoxState::Shown) { v.box = BoxState::Hidden; return; }
        // dalla striscia di eActivity EXIT torna al documento (la casella resta su OPTN)
        if (ctx.inStrip(AppId::Geometry) && v.sel.empty()) { ctx.home(); return; }
        v.box = BoxState::Focused;                       // EXIT a riposo: casella di misura (fx-CG50)
        v.shell.fkeys.set(&kBoxBar);
        return;
    }
}

void GeometryApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    switch (v.screen) {
    case Screen::Help: drawHelp(ctx, v, c); break;
    case Screen::Animations: drawAnimations(ctx, v, c); break;
    case Screen::Table: drawTable(ctx, v, c); break;
    case Screen::Form: {
        ui::clear(c);
        ctx.drawStatus(c, "GEOMETRY");
        const char* title = "Option";
        const char* const setupKeys[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        const char* const winKeys[6] = {"INITIAL", nullptr, nullptr, nullptr, nullptr, nullptr};
        switch (v.formKind) {
        case FormKind::Setup: title = "SET UP / Option (EXIT: OK, AC: annulla)"; break;
        case FormKind::Window: title = "V-Window (EXIT: OK, AC: annulla)"; break;
        case FormKind::Format: title = "FORMAT (EXIT: OK, AC: annulla)"; break;
        case FormKind::Animation: title = "Edit Animation (EXIT: OK)"; break;
        case FormKind::Text: title = "Text (EXE: posare)"; break;
        case FormKind::Label: title = "Label (al piu' 14 caratteri)"; break;
        case FormKind::Expression: title = "Expression (@n = misura incollata)"; break;
        default: break;
        }
        drawFormScreen(c, title, v.form, ui::ContentBottom - ui::ContentTop - 30, v.formKind == FormKind::Window ? winKeys : setupKeys);
        break;
    }
    default: drawMain(ctx, v, c); break;
    }
    v.shell.drawModals(c);
    v.files.draw(c);
}

std::string GeometryApp::screenName() const {
    if (!v_) return "closed";
    const View& v = *v_;
    if (v.busy()) return v.job == Job::Animate ? "animate" : v.job == Job::Table ? "table-calc" : "solve";
    if (v.files.active()) return "file";
    if (v.shell.menu.open) return "menu";
    if (v.shell.ask.active()) return "ask";
    if (v.shell.confirm.active()) return "confirm";
    switch (v.screen) {
    case Screen::Help: return "help";
    case Screen::Form: return "form";
    case Screen::Animations: return "animations";
    case Screen::Table: return "table";
    default: break;
    }
    if (v.box == BoxState::Focused) return "box";
    switch (v.tool) {
    case Tool::Point: return "point";
    case Tool::Line: return "line";
    case Tool::Circle: return "circle";
    case Tool::Arc: return "arc";
    case Tool::Semicircle: return "semicircle";
    case Tool::Arc3: return "arc3";
    case Tool::Box: return "box-figure";
    case Tool::Polygon: return "polygon";
    case Tool::Tangent: return "tangent";
    case Tool::AnglePosition: return "angle-position";
    case Tool::Axis: return "axis";
    case Tool::Vector: return "vector";
    case Tool::Center: return "center";
    case Tool::ZoomBox: return "zoom-box";
    case Tool::Pan: return "pan";
    case Tool::Scroll: return "scroll";
    case Tool::Move: return "move";
    case Tool::PlaceText: return "place-text";
    case Tool::PlaceExpression: return "place-expression";
    default: return "main";
    }
}
std::vector<std::uint16_t> GeometryApp::selection() const { return v_ ? v_->sel : std::vector<std::uint16_t>{}; }
double GeometryApp::pointerX() const noexcept { return v_ ? v_->px : 0; }
double GeometryApp::pointerY() const noexcept { return v_ ? v_->py : 0; }
std::string GeometryApp::menuTitle() const {
    if (!v_ || !v_->shell.menu.open) return "";
    const MenuDef& d = menuDef(v_->menu);
    if (d.tabs && v_->tab >= 0 && v_->tab < d.count) return d.tabs[v_->tab].name;     // la scheda aperta
    return v_->shell.menu.title ? v_->shell.menu.title : "";
}
std::vector<std::string> GeometryApp::menuItems() const { return v_ && v_->shell.menu.open ? v_->shell.menu.items : std::vector<std::string>{}; }
bool GeometryApp::boxFocused() const noexcept { return v_ && v_->box == BoxState::Focused; }
std::string GeometryApp::headerText() const { return v_ ? v_->header : std::string(); }
std::string GeometryApp::boxText() const { return v_ && v_->box != BoxState::Hidden ? v_->boxLine : std::string(); }
} // namespace fx::app
