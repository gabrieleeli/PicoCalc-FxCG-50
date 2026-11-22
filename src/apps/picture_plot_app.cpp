// PICTURE PLOT (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Funzioni minime del capitolo 15 del manuale fx-CG50 (descrizione del
// comportamento, nessun codice o grafica copiati): un'immagine .pcpict (PICT o
// CAPT) come sfondo, punti messi con il cursore e numerati, coordinate in List 1
// e List 2, regressione col motore di Statistics (stats::regress) e la retta o
// la curva disegnata sopra lo sfondo; file .pcplot.
#include "apps.hpp"
#include "picture_plot.hpp"
#include "statistics.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Act : int { ActOpenPict = 0, ActPlot, ActUndo, ActList, ActReg, ActWindow, ActNew, ActOpen, ActSaveAs, ActOk };
const FKeyPage kFilePage{{{"NEW", ActNew}, {"OPEN", ActOpen}, {"SAVE\xC2\xB7" "AS", ActSaveAs}, {"PICT", ActOpenPict}, {}, {}}};
const FKeyMenu kFileMenu{&kFilePage, 1, nullptr};
const FKeyPage kMainPage{{{"FILE", -1, &kFileMenu}, {"PLOT", ActPlot}, {"UNDO", ActUndo}, {"LIST", ActList}, {"REG", ActReg}, {"V-WIN", ActWindow}}};
const FKeyMenu kMainMenu{&kMainPage, 1, nullptr};
const FKeyPage kWinPage{{{}, {}, {}, {}, {}, {"OK", ActOk}}};
const FKeyMenu kWinMenu{&kWinPage, 1, nullptr};
constexpr int MenuReg = 1, ConfirmNew = 1;
constexpr int AreaTop = ui::ContentTop + 16, AreaW = ui::ScreenW, AreaH = ui::ContentBottom - AreaTop;
const stats::RegType kRegs[] = {stats::RegType::Linear, stats::RegType::MedMed, stats::RegType::Quad, stats::RegType::Cubic, stats::RegType::Quart,
                                stats::RegType::Log, stats::RegType::Exp, stats::RegType::AbExp, stats::RegType::Power};
} // namespace

struct PicturePlotApp::View {
    ModeShell shell;
    TypedFileMenu files{files::FileType::PicturePlot};
    FileDialog pictDialog;
    std::vector<std::string> pictFiles;
    pict::Image picture;
    bool havePicture{false};
    std::string loaded;                          // immagine caricata (nome)
    int px{AreaW / 2}, py{AreaH / 2};            // cursore in pixel dell'area
    bool window{false};
    Form form;
    std::vector<int> curve;                      // punti di schermo della regressione (x, y)
    std::string regText;
};

PicturePlotApp::PicturePlotApp() = default;
PicturePlotApp::~PicturePlotApp() = default;

namespace {
void loadPicture(Context& ctx, PicturePlotApp::View& v) {
    const std::string& name = ctx.ws.pictplot.background;
    if (name == v.loaded) return;
    v.loaded = name;
    v.havePicture = false;
    v.picture = pict::Image{};
    files::FileSystem* fs = userFiles(ctx);
    if (name.empty() || !fs) return;
    for (const char* dir : {files::PictFolder, files::CaptureFolder}) {
        std::vector<std::uint8_t> data;
        if (files::readAll(*fs, files::join(dir, name), data, pict::maxEncodedBytes() + 256) != files::Status::Ok) continue;
        if (pict::parse(data.data(), data.size(), v.picture).empty()) { v.havePicture = true; return; }
    }
}
void buildCurve(Context& ctx, PicturePlotApp::View& v) {
    v.curve.clear();
    v.regText.clear();
    const pplot::State& s = ctx.ws.pictplot;
    if (s.regression < 0) return;
    std::vector<double> xs, ys;
    pplot::coordinates(s, xs, ys);
    const stats::Regression r = stats::regress(kRegs[s.regression], xs, ys);
    v.regText = std::string(stats::regFormula(r.type)) + "  a=" + formNumber(r.coef.empty() ? 0 : r.coef[0]) + (r.coef.size() > 1 ? "  b=" + formNumber(r.coef[1]) : "");
    if (r.hasR) v.regText += "  r=" + formNumber(r.r);
    for (int x = 0; x < AreaW; x += 2) {
        double mx, my;
        pplot::toPlane(s.window, AreaW, AreaH, x, 0, mx, my);
        double y = 0;
        try { y = r.predict(mx); } catch (...) { continue; }
        if (!std::isfinite(y)) continue;
        double sx, sy;
        pplot::toScreen(s.window, AreaW, AreaH, mx, y, sx, sy);
        if (sy < -2000 || sy > 2000) continue;
        v.curve.push_back(x);
        v.curve.push_back(static_cast<int>(std::lround(sy)));
    }
}
} // namespace

void PicturePlotApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    v_->shell.fkeys.set(&kMainMenu);
    v_->loaded = "\x01";                          // forza il caricamento
    loadPicture(ctx, *v_);
    try { buildCurve(ctx, *v_); } catch (...) { v_->curve.clear(); }
}
void PicturePlotApp::leave(Context&) { v_.reset(); }      // l'immagine (fino a 200 KB) si libera uscendo
std::string PicturePlotApp::screenName() const {
    if (!v_) return "closed";
    return v_->window ? "vwin" : v_->pictDialog.active() ? "pict" : v_->files.active() ? "file" : "plot";
}
bool PicturePlotApp::tick(Context&) { return false; }

void PicturePlotApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    pplot::State& s = ctx.ws.pictplot;
    if (v.pictDialog.active()) {
        if (v.pictDialog.key(ev) == 1) {
            s.background = v.pictDialog.name();
            ctx.markDirty();
            loadPicture(ctx, v);
            if (!v.havePicture) throw MathError("Data ERROR: immagine non leggibile");
        }
        return;
    }
    if (v.files.active()) {
        if (v.files.key(ev) != 1) return;
        if (v.files.purpose() == TypedFileMenu::Purpose::Open) {
            const auto data = v.files.load(ctx, v.files.file());
            pplot::State t;
            t.file = v.files.file();
            if (!pplot::decodeFile(data.data(), data.size(), t)) throw MathError("Data ERROR: file Picture Plot danneggiato");
            s = std::move(t);
            loadPicture(ctx, v);
            buildCurve(ctx, v);
        } else {
            v.files.save(ctx, v.files.file(), pplot::encodeFile(s));
            s.file = v.files.file();
            ctx.toast("Salvato " + s.file);
        }
        ctx.markDirty();
        return;
    }
    const bool modal = v.shell.modalKey(ev,
        [&](int tag, bool yes) { if (tag == ConfirmNew && yes) { pplot::reset(s); v.curve.clear(); loadPicture(ctx, v); ctx.markDirty(); } },
        [](int) {},
        [&](int tag, int choice) {
            if (tag != MenuReg || choice < 0) return;
            if (s.points.size() < 2) throw MathError("Dimension ERROR: almeno due punti");
            const int old = s.regression;
            s.regression = choice;
            try { buildCurve(ctx, v); } catch (...) { s.regression = old; buildCurve(ctx, v); throw; }
            ctx.markDirty();
        });
    if (modal) return;
    if (v.window) {
        const bool editing = v.form.editing();
        const int a = editing ? FKeyBar::None : v.shell.fkeys.key(ev, false);
        if (a == ActOk) {
            pplot::Window w{v.form.number(ctx, 0), v.form.number(ctx, 1), v.form.number(ctx, 2), v.form.number(ctx, 3)};
            const std::string e = pplot::validate(w);
            if (!e.empty()) throw MathError(e);
            s.window = w;
            ctx.markDirty();
            buildCurve(ctx, v);
            v.window = false;
            v.shell.fkeys.set(&kMainMenu);
            return;
        }
        if (!editing && ev.is(Key::Exit)) { v.window = false; v.shell.fkeys.set(&kMainMenu); return; }
        (void)v.form.key(ctx, ev);
        return;
    }
    const int a = v.shell.fkeys.key(ev, ctx.shift());
    switch (a) {
    case FKeyBar::Handled: return;
    case ActNew: v.shell.fkeys.set(&kMainMenu); v.shell.confirm.open("Nuovo Picture Plot?", ConfirmNew); return;
    case ActOpen: v.shell.fkeys.set(&kMainMenu); v.files.openList(ctx); return;
    case ActSaveAs: v.shell.fkeys.set(&kMainMenu); v.files.openSaveAs(ctx, TypedFileMenu::stem(s.file)); return;
    case ActOpenPict: {
        v.shell.fkeys.set(&kMainMenu);
        files::FileSystem* fs = userFiles(ctx);
        if (!fs) throw MathError("ERROR: archivio dei file non disponibile (microSD assente)");
        v.pictFiles.clear();
        for (const char* dir : {files::PictFolder, files::CaptureFolder}) {
            std::vector<files::Entry> es;
            if (fs->list(dir, es) != files::Status::Ok) continue;
            for (const auto& e : es) if (!e.directory && files::hasExtension(e.name, ".pcpict")) v.pictFiles.push_back(e.name);
        }
        v.pictDialog.setExtension("");
        v.pictDialog.setEmptyText("Nessuna immagine in PICT o CAPT");
        v.pictDialog.openList("Sfondo", v.pictFiles);
        return;
    }
    case ActPlot: case ActOk: {
        double x, y;
        pplot::toPlane(s.window, AreaW, AreaH, v.px, v.py, x, y);
        pplot::addPoint(s, x, y);
        ctx.markDirty();
        if (s.regression >= 0 && s.points.size() >= 2) { try { buildCurve(ctx, v); } catch (...) { v.curve.clear(); } }
        return;
    }
    case ActUndo:
        if (!s.points.empty()) { s.points.pop_back(); ctx.markDirty(); if (s.points.size() < 2) { v.curve.clear(); } else { try { buildCurve(ctx, v); } catch (...) { v.curve.clear(); } } }
        return;
    case ActList: {
        std::vector<double> xs, ys;
        pplot::coordinates(s, xs, ys);
        ctx.ws.setList(0, xs);
        ctx.ws.setList(1, ys);
        ctx.markDirty();
        ctx.toast(std::to_string(xs.size()) + " punti \xE2\x86\x92 List 1, List 2");
        return;
    }
    case ActReg: {
        std::vector<std::string> items;
        for (stats::RegType t : kRegs) items.push_back(stats::regName(t));
        v.shell.showMenu("Regressione", items, MenuReg);
        return;
    }
    case ActWindow:
        v.form.fields = {numberField("Xmin", formNumber(s.window.xmin)), numberField("Xmax", formNumber(s.window.xmax)),
                         numberField("Ymin", formNumber(s.window.ymin)), numberField("Ymax", formNumber(s.window.ymax))};
        v.window = true;
        v.shell.fkeys.set(&kWinMenu);
        return;
    default: break;
    }
    const int stepPx = ctx.shift() ? 12 : 3;
    if (ev.is(Key::Left)) v.px = std::max(0, v.px - stepPx);
    else if (ev.is(Key::Right)) v.px = std::min(AreaW - 1, v.px + stepPx);
    else if (ev.is(Key::Up)) v.py = std::max(0, v.py - stepPx);
    else if (ev.is(Key::Down)) v.py = std::min(AreaH - 1, v.py + stepPx);
    else if (ev.is(Key::Exe)) { double x, y; pplot::toPlane(s.window, AreaW, AreaH, v.px, v.py, x, y); pplot::addPoint(s, x, y); ctx.markDirty(); }
    else if (ev.is(Key::Exit)) { if (v.shell.fkeys.inSubmenu()) v.shell.fkeys.back(); else ctx.home(); }
}

void PicturePlotApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    const pplot::State& s = ctx.ws.pictplot;
    ui::clear(c);
    ctx.drawStatus(c, "PICTURE PLOT");
    c.setClip(gfx::Rect{0, AreaTop, AreaW, AreaH});
    if (v.havePicture) {
        // L'immagine alla larghezza dell'area (le immagini della PicoCalc sono 320 punti).
        (void)pict::forEachSpan(v.picture, [&](int x, int y, int n, std::uint16_t col) {
            if (col != pict::Transparent) c.hLine(x, AreaTop + y, n, col);
        });
    }
    for (std::size_t i = 0; i + 3 < v.curve.size(); i += 2) c.line(v.curve[i], AreaTop + v.curve[i + 1], v.curve[i + 2], AreaTop + v.curve[i + 3], Error);
    for (std::size_t i = 0; i < s.points.size(); ++i) {
        double sx, sy;
        pplot::toScreen(s.window, AreaW, AreaH, s.points[i].x, s.points[i].y, sx, sy);
        const int x = static_cast<int>(std::lround(sx)), y = AreaTop + static_cast<int>(std::lround(sy));
        c.rect(x - 2, y - 2, 5, 5, Graph[0], true);
        ui::text(c, x + 4, y - 12, std::to_string(i + 1).c_str(), Graph[0], font::Face::Small);
    }
    c.hLine(v.px - 6, AreaTop + v.py, 13, Ink);
    c.vLine(v.px, AreaTop + v.py - 6, 13, Ink);
    c.resetClip();
    double x, y;
    pplot::toPlane(s.window, AreaW, AreaH, v.px, v.py, x, y);
    std::string head = "X=" + formNumber(x) + " Y=" + formNumber(y) + "  #" + std::to_string(s.points.size());
    if (!v.regText.empty()) head = v.regText;
    else if (!v.havePicture) head += "  (FILE > PICT: sfondo)";
    drawModeHeader(c, head);
    if (v.window) {
        c.rect(0, AreaTop, ui::ScreenW, AreaH, Bg, true);
        v.form.draw(c, AreaTop + 2, ui::ContentBottom);
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.files.draw(c);
    v.pictDialog.draw(c);
}
} // namespace fx::app
