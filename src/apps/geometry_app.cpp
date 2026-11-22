// GEOMETRY (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Funzioni minime del capitolo 14 del manuale fx-CG50 (descrizione del
// comportamento, nessun codice o grafica copiati): punti, segmenti, rette,
// circonferenze e poligoni messi con il cursore; selezione, spostamento e
// cancellazione; misure di lunghezza, angolo, area e coordinate; zoom e pan;
// file .pcgeo. Il nucleo (geometria e misure) e' in geometry.cpp.
#include "apps.hpp"
#include "geometry.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Act : int { ActPoint = 0, ActSegment, ActLine, ActCircle, ActPolygon, ActSelect, ActMove, ActMeasure, ActZoomIn, ActZoomOut, ActPan, ActFit,
                 ActNew, ActOpen, ActSaveAs, ActDone };
const FKeyPage kDrawPage{{{"POINT", ActPoint}, {"SEGMENT", ActSegment}, {"LINE", ActLine}, {"CIRCLE", ActCircle}, {"POLYGON", ActPolygon}, {}}};
const FKeyMenu kDrawMenu{&kDrawPage, 1, nullptr};
const FKeyPage kViewPage{{{"ZOOM+", ActZoomIn}, {"ZOOM-", ActZoomOut}, {"PAN", ActPan}, {"RESET", ActFit}, {}, {}}};
const FKeyMenu kViewMenu{&kViewPage, 1, nullptr};
const FKeyPage kFilePage{{{"NEW", ActNew}, {"OPEN", ActOpen}, {"SAVE\xC2\xB7" "AS", ActSaveAs}, {}, {}, {}}};
const FKeyMenu kFileMenu{&kFilePage, 1, nullptr};
const FKeyPage kMainPage{{{"DRAW", -1, &kDrawMenu}, {"SELECT", ActSelect}, {"MOVE", ActMove}, {"MEASURE", ActMeasure}, {"VIEW", -1, &kViewMenu}, {"FILE", -1, &kFileMenu}}};
const FKeyMenu kMainMenu{&kMainPage, 1, nullptr};
const FKeyPage kPolyPage{{{}, {}, {}, {}, {}, {"DONE", ActDone}}};
const FKeyMenu kPolyMenu{&kPolyPage, 1, nullptr};
enum class Tool : std::uint8_t { None, Draw, Move, Pan };
constexpr int ConfirmNew = 1;
constexpr int AreaTop = ui::ContentTop + 16, AreaH = ui::ContentBottom - AreaTop;
// Passo del cursore nel piano: un valore "rotondo" vicino a 6 pixel.
double niceStep(double scale) {
    const double raw = 6.0 / scale;
    const double p = std::pow(10.0, std::floor(std::log10(raw)));
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0}) if (m * p >= raw) return m * p;
    return 10 * p;
}
double snap(double v, double step) { return std::round(v / step) * step; }
} // namespace

struct GeometryApp::View {
    ModeShell shell;
    TypedFileMenu files{files::FileType::Geometry};
    double cx{0}, cy{0};                         // cursore nel piano
    Tool tool{Tool::None};
    geo::Kind drawing{geo::Kind::Point};
    std::vector<double> pending;                 // punti dell'oggetto in costruzione
    int selected{-1};
    bool measure{false};
};

GeometryApp::GeometryApp() = default;
GeometryApp::~GeometryApp() = default;

namespace {
void addPending(Context& ctx, GeometryApp::View& v) {
    v.pending.push_back(v.cx);
    v.pending.push_back(v.cy);
    const std::size_t have = v.pending.size() / 2;
    if (v.drawing == geo::Kind::Polygon) {
        // EXE sul primo vertice chiude il poligono.
        if (have >= 4 && std::abs(v.pending[0] - v.cx) < 1e-9 && std::abs(v.pending[1] - v.cy) < 1e-9) {
            v.pending.resize(v.pending.size() - 2);
        } else return;
    } else if (have < geo::pointsNeeded(v.drawing)) {
        return;
    }
    geo::Object o;
    o.kind = v.drawing;
    o.xy = std::move(v.pending);
    v.pending.clear();
    geo::add(ctx.ws.geometry, std::move(o));          // Argument/Memory ERROR: nulla aggiunto
    v.selected = static_cast<int>(ctx.ws.geometry.objects.size()) - 1;
    ctx.markDirty();
    if (v.drawing == geo::Kind::Polygon) { v.tool = Tool::None; v.shell.fkeys.set(&kMainMenu); }
}
} // namespace

void GeometryApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    v_->cx = ctx.ws.geometry.view.cx;
    v_->cy = ctx.ws.geometry.view.cy;
    v_->shell.fkeys.set(&kMainMenu);
}
void GeometryApp::leave(Context&) { v_.reset(); }
std::string GeometryApp::screenName() const {
    if (!v_) return "closed";
    if (v_->files.active()) return "file";
    if (v_->measure) return "measure";
    switch (v_->tool) {
    case Tool::Draw: return "draw";
    case Tool::Move: return "move";
    case Tool::Pan: return "pan";
    default: return "main";
    }
}
bool GeometryApp::tick(Context&) { return false; }

void GeometryApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    geo::State& s = ctx.ws.geometry;
    if (v.files.active()) {
        if (v.files.key(ev) != 1) return;
        if (v.files.purpose() == TypedFileMenu::Purpose::Open) {
            const auto data = v.files.load(ctx, v.files.file());
            geo::State t;
            t.file = v.files.file();
            if (!geo::decodeFile(data.data(), data.size(), t)) throw MathError("Data ERROR: costruzione danneggiata");
            s = std::move(t);
            v.selected = -1;
            v.cx = s.view.cx; v.cy = s.view.cy;
        } else {
            v.files.save(ctx, v.files.file(), geo::encodeFile(s));
            s.file = v.files.file();
            ctx.toast("Salvato " + s.file);
        }
        ctx.markDirty();
        return;
    }
    const bool modal = v.shell.modalKey(ev,
        [&](int tag, bool yes) { if (tag == ConfirmNew && yes) { geo::reset(s); v.selected = -1; v.cx = v.cy = 0; ctx.markDirty(); } },
        [](int) {}, [](int, int) {});
    if (modal) return;
    if (v.measure) { if (ev.is(Key::Exit) || ev.is(Key::Exe) || ev.is(Key::AC)) v.measure = false; return; }
    const double step = niceStep(s.view.scale) * (ctx.shift() ? 4 : 1);
    const int a = v.shell.fkeys.key(ev, false);
    switch (a) {
    case FKeyBar::Handled: return;
    case ActPoint: case ActSegment: case ActLine: case ActCircle: case ActPolygon:
        v.drawing = static_cast<geo::Kind>(a - ActPoint);
        v.pending.clear();
        v.tool = Tool::Draw;
        v.shell.fkeys.set(v.drawing == geo::Kind::Polygon ? &kPolyMenu : &kMainMenu);
        return;
    case ActDone:
        if (v.pending.size() >= 6) { geo::Object o; o.kind = geo::Kind::Polygon; o.xy = std::move(v.pending); v.pending.clear(); geo::add(s, std::move(o)); ctx.markDirty(); v.selected = static_cast<int>(s.objects.size()) - 1; }
        v.tool = Tool::None;
        v.shell.fkeys.set(&kMainMenu);
        return;
    case ActSelect: {
        v.tool = Tool::None;
        v.selected = geo::hitTest(s, v.cx, v.cy, 8.0 / s.view.scale);
        if (v.selected < 0) ctx.toast("Nessun oggetto vicino al cursore");
        return;
    }
    case ActMove:
        if (v.selected < 0) throw MathError("Argument ERROR: selezionare un oggetto (SELECT)");
        v.tool = Tool::Move;
        return;
    case ActMeasure:
        if (v.selected < 0) throw MathError("Argument ERROR: selezionare un oggetto (SELECT)");
        v.measure = true;
        return;
    case ActZoomIn: geo::zoom(s.view, 1.5); s.view.cx = v.cx; s.view.cy = v.cy; v.shell.fkeys.set(&kMainMenu); ctx.markDirty(); return;
    case ActZoomOut: geo::zoom(s.view, 1 / 1.5); s.view.cx = v.cx; s.view.cy = v.cy; v.shell.fkeys.set(&kMainMenu); ctx.markDirty(); return;
    case ActPan: v.tool = Tool::Pan; v.shell.fkeys.set(&kMainMenu); return;
    case ActFit: s.view = geo::View{}; v.cx = v.cy = 0; v.shell.fkeys.set(&kMainMenu); ctx.markDirty(); return;
    case ActNew: v.shell.fkeys.set(&kMainMenu); v.shell.confirm.open("Nuova costruzione? (gli oggetti non salvati si perdono)", ConfirmNew); return;
    case ActOpen: v.shell.fkeys.set(&kMainMenu); v.files.openList(ctx); return;
    case ActSaveAs: v.shell.fkeys.set(&kMainMenu); v.files.openSaveAs(ctx, TypedFileMenu::stem(s.file)); return;
    default: break;
    }
    double dx = 0, dy = 0;
    if (ev.is(Key::Left)) dx = -step;
    else if (ev.is(Key::Right)) dx = step;
    else if (ev.is(Key::Up)) dy = step;
    else if (ev.is(Key::Down)) dy = -step;
    if (dx != 0 || dy != 0) {
        if (v.tool == Tool::Move && v.selected >= 0) { geo::move(s.objects[static_cast<std::size_t>(v.selected)], dx, dy); ctx.markDirty(); }
        if (v.tool == Tool::Pan) { s.view.cx += dx; s.view.cy += dy; ctx.markDirty(); }
        v.cx = snap(v.cx + dx, niceStep(s.view.scale));
        v.cy = snap(v.cy + dy, niceStep(s.view.scale));
        // il cursore resta nella finestra: la vista lo segue
        const double halfW = ui::ScreenW / 2.0 / s.view.scale, halfH = AreaH / 2.0 / s.view.scale;
        if (v.cx < s.view.cx - halfW || v.cx > s.view.cx + halfW || v.cy < s.view.cy - halfH || v.cy > s.view.cy + halfH) { s.view.cx = v.cx; s.view.cy = v.cy; }
        return;
    }
    if (ev.is(Key::Exe)) {
        if (v.tool == Tool::Draw) { addPending(ctx, v); return; }
        if (v.tool == Tool::Move || v.tool == Tool::Pan) { v.tool = Tool::None; return; }
        v.selected = geo::hitTest(s, v.cx, v.cy, 8.0 / s.view.scale);
        return;
    }
    if ((ev.is(Key::Del) || ev.is(Key::DelForward)) && v.selected >= 0 && v.tool == Tool::None) {
        s.objects.erase(s.objects.begin() + v.selected);
        v.selected = -1;
        ctx.markDirty();
        return;
    }
    if (ev.is(Key::Exit) || ev.is(Key::AC)) {
        if (v.shell.fkeys.inSubmenu()) { v.shell.fkeys.back(); return; }
        if (v.tool != Tool::None || !v.pending.empty()) { v.tool = Tool::None; v.pending.clear(); v.shell.fkeys.set(&kMainMenu); return; }
        if (v.selected >= 0) { v.selected = -1; return; }
        if (ev.is(Key::Exit)) ctx.home();
    }
}

void GeometryApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    const geo::State& s = ctx.ws.geometry;
    ui::clear(c);
    ctx.drawStatus(c, "GEOMETRY");
    const int w = ui::ScreenW, h = AreaH;
    auto sx = [&](double x, double y, int& px, int& py) {
        double fx, fy;
        geo::toScreen(s.view, w, h, x, y, fx, fy);
        px = static_cast<int>(std::lround(fx));
        py = AreaTop + static_cast<int>(std::lround(fy));
    };
    c.setClip(gfx::Rect{0, AreaTop, w, h});
    // assi
    int ox, oy;
    sx(0, 0, ox, oy);
    if (oy >= AreaTop && oy < AreaTop + h) c.hLine(0, oy, w, Faint);
    if (ox >= 0 && ox < w) c.vLine(ox, AreaTop, h, Faint);
    for (std::size_t k = 0; k < s.objects.size(); ++k) {
        const geo::Object& o = s.objects[k];
        const std::uint16_t col = static_cast<int>(k) == v.selected ? Error : Graph[0];
        const auto& p = o.xy;
        int x0, y0, x1, y1;
        switch (o.kind) {
        case geo::Kind::Point: sx(p[0], p[1], x0, y0); c.circle(x0, y0, 3, col, true); break;
        case geo::Kind::Segment: sx(p[0], p[1], x0, y0); sx(p[2], p[3], x1, y1); c.line(x0, y0, x1, y1, col); break;
        case geo::Kind::Line: {
            // la retta attraversa tutta la finestra
            const double dx = p[2] - p[0], dy = p[3] - p[1], far = 4000.0 / s.view.scale, len = std::hypot(dx, dy);
            sx(p[0] - dx / len * far, p[1] - dy / len * far, x0, y0); sx(p[0] + dx / len * far, p[1] + dy / len * far, x1, y1);
            c.line(x0, y0, x1, y1, col);
            break;
        }
        case geo::Kind::Circle: {
            sx(p[0], p[1], x0, y0);
            const int r = static_cast<int>(std::lround(geo::distance(p[0], p[1], p[2], p[3]) * s.view.scale));
            if (r < 4000) c.circle(x0, y0, r, col, false);
            c.rect(x0 - 1, y0 - 1, 3, 3, col, true);
            break;
        }
        case geo::Kind::Polygon:
            for (std::size_t i = 0; i < o.count(); ++i) {
                const std::size_t j = (i + 1) % o.count();
                sx(p[2 * i], p[2 * i + 1], x0, y0); sx(p[2 * j], p[2 * j + 1], x1, y1);
                c.line(x0, y0, x1, y1, col);
            }
            break;
        }
    }
    // oggetto in costruzione
    for (std::size_t i = 0; i + 1 < v.pending.size(); i += 2) {
        int px, py;
        sx(v.pending[i], v.pending[i + 1], px, py);
        c.rect(px - 2, py - 2, 5, 5, Warn, true);
        if (i + 3 < v.pending.size()) { int qx, qy; sx(v.pending[i + 2], v.pending[i + 3], qx, qy); c.line(px, py, qx, qy, Warn); }
    }
    int cx, cy;
    sx(v.cx, v.cy, cx, cy);
    c.hLine(cx - 6, cy, 13, Ink); c.vLine(cx, cy - 6, 13, Ink);
    c.resetClip();
    char head[96];
    const char* tool = v.tool == Tool::Draw ? geo::kindName(v.drawing) : v.tool == Tool::Move ? "MOVE" : v.tool == Tool::Pan ? "PAN" : "";
    std::snprintf(head, sizeof head, "(%s, %s)  %s  %u oggetti", formNumber(v.cx).c_str(), formNumber(v.cy).c_str(), tool, static_cast<unsigned>(s.objects.size()));
    drawModeHeader(c, head);
    if (v.measure && v.selected >= 0 && v.selected < static_cast<int>(s.objects.size())) {
        const auto lines = geo::measure(s.objects[static_cast<std::size_t>(v.selected)]);
        const int ph = 20 + static_cast<int>(lines.size()) * 16;
        const gfx::Rect r{16, 80, ui::ScreenW - 32, ph};
        ui::panel(c, r, Bg, Ink);
        ui::text(c, r.x + 6, r.y + 3, geo::kindName(s.objects[static_cast<std::size_t>(v.selected)].kind), Accent, font::Face::Small);
        for (std::size_t i = 0; i < lines.size(); ++i) ui::textFit(c, r.x + 6, r.y + 18 + static_cast<int>(i) * 16, lines[i], r.w - 12, Ink, font::Face::Small);
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.files.draw(c);
}
} // namespace fx::app
