// 3D GRAPH: surfaces z = f(x, y) (Z1..Z3) in the 3D V-Window.
//   list:  up/down choose Z1..Z3, EXE or typing edits, F1 SELECT, F2 DELETE,
//          F3 V-WIN, F6 DRAW, EXIT main menu;
//   plot:  arrows rotate, F1 ZOOM+, F2 ZOOM-, F3 V-WIN, F4 AUTO (Z range from
//          the samples), F5 reset the view, F6/EXIT back to the list.
// The surfaces are sampled in tick() (a budget of points per tick) and kept:
// rotation and zoom only re-project. Memory: meshes and the z-buffer go to the
// PSRAM of the Pico Plus 2 W (grid up to 128, depth buffer); without PSRAM the
// grid is capped at 24 and the painter's algorithm is used (limits.hpp).
#include "apps.hpp"
#include "graph3d.hpp"
#include "limits.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>

namespace fx::app {
using namespace ui::theme;

namespace {
constexpr std::size_t PointsPerTick = 600;    // evaluations per tick (keyboard stays responsive)
constexpr float ZoomStep = 1.25F, RotateStep = 15.0F;
enum WindowField { WXmin, WXmax, WYmin, WYmax, WZmin, WZmax, WGrid };

g3d::Window windowOf(const models::Graph3DModel& m) {
    return {m.xmin, m.xmax, m.ymin, m.ymax, m.zmin, m.zmax};
}
std::size_t viewportPixels() {
    return static_cast<std::size_t>(Graph3DApp::Viewport.w) * static_cast<std::size_t>(Graph3DApp::Viewport.h);
}
} // namespace

void Graph3DApp::enter(Context& ctx) {
    (void)ctx;
    screen_ = Screen::List;
    message_.clear();
    samplesValid_ = false;   // A..Z or the angle unit may have changed elsewhere
}

void Graph3DApp::leave(Context&) {
    stopSampling();
    // The meshes are large (PSRAM on the Pico Plus 2 W): release them when the
    // mode is left; they are recomputed on the next DRAW.
    for (auto& m : meshes_) m.clear();
    samplesValid_ = false;
}

void Graph3DApp::stopSampling() noexcept {
    sampling_ = false;
    sampleSurface_ = sampleNext_ = 0;
}

void Graph3DApp::startSampling(Context& ctx) {
    auto& model = ctx.ws.graph3d.model;
    const std::string err = models::validate(model);
    if (!err.empty()) throw MathError(err);
    ctx.ws.apply();                        // angle unit of the trigonometric functions
    bool any = false;
    for (std::size_t i = 0; i < models::Graph3DModel::Functions; ++i) {
        active_[i] = model.selected[i] && !model.z[i].empty();
        meshes_[i].clear();
        if (!active_[i]) { compiled_[i] = CompiledExpression{}; continue; }
        compiled_[i] = CompiledExpression(model.z[i], "x");   // throws Syntax ERROR before anything is drawn
        any = true;
    }
    if (!any) throw MathError("Argument ERROR: nessuna funzione Z selezionata");
    // Grid and renderer from the memory that is free now.
    grid_ = std::min<std::size_t>(static_cast<std::size_t>(model.grid), limits::graph3dGrid(viewportPixels()));
    zbuffer_ = limits::graph3dZBuffer(viewportPixels());
    for (;;) {
        try {
            for (std::size_t i = 0; i < models::Graph3DModel::Functions; ++i)
                if (active_[i]) g3d::beginSample(meshes_[i], grid_);
            break;
        } catch (const std::bad_alloc&) {
            for (auto& m : meshes_) m.clear();
            if (grid_ <= 5) throw;          // reported as Memory ERROR by the AppManager
            grid_ = std::max<std::size_t>(5, grid_ / 2);
        }
    }
    sampling_ = true;
    samplesValid_ = false;
    sampleSurface_ = sampleNext_ = 0;
    message_.clear();
}

bool Graph3DApp::tick(Context& ctx) {
    if (!sampling_) return false;
    const auto window = windowOf(ctx.ws.graph3d.model);
    std::size_t budget = PointsPerTick;
    while (budget > 0 && sampleSurface_ < meshes_.size()) {
        if (!active_[sampleSurface_]) { ++sampleSurface_; sampleNext_ = 0; continue; }
        const CompiledExpression& f = compiled_[sampleSurface_];
        const CalculatorMemory* memory = &ctx.ws.memory;
        const g3d::Function fn = [&f, memory](double x, double y, double& z) {
            try { z = f.eval2(x, "y", y, memory); return true; }
            catch (const std::bad_alloc&) { throw; }
            catch (...) { return false; }   // undefined point: a hole in the surface
        };
        const std::size_t before = sampleNext_;
        const bool done = g3d::sampleStep(meshes_[sampleSurface_], fn, window, sampleNext_, budget);
        budget -= std::min(budget, sampleNext_ - before);
        if (done) { ++sampleSurface_; sampleNext_ = 0; }
    }
    if (sampleSurface_ >= meshes_.size()) {
        sampling_ = false;
        samplesValid_ = true;
        std::size_t valid = 0;
        for (std::size_t i = 0; i < meshes_.size(); ++i) if (active_[i]) valid += meshes_[i].valid;
        if (valid == 0) message_ = "Math ERROR: funzioni non definite nella finestra";
    }
    return true;
}

void Graph3DApp::finishSampling(Context& ctx) {
    while (sampling_) (void)tick(ctx);
}

void Graph3DApp::openWindow(Context& ctx) {
    const auto& m = ctx.ws.graph3d.model;
    auto num = [&](const char* label, double v) { return FormField{label, fmt(ctx, v), FormField::Type::Number, {}, 0}; };
    window_.fields = {num("Xmin", m.xmin), num("Xmax", m.xmax), num("Ymin", m.ymin), num("Ymax", m.ymax),
                      num("Zmin", m.zmin), num("Zmax", m.zmax), num("Griglia", m.grid)};
    window_.selected = 0;
    windowFromPlot_ = screen_ == Screen::Plot;
    stopSampling();
    screen_ = Screen::Window;
}

void Graph3DApp::applyWindow(Context& ctx) {
    models::Graph3DModel m = ctx.ws.graph3d.model;
    m.xmin = window_.number(ctx, WXmin); m.xmax = window_.number(ctx, WXmax);
    m.ymin = window_.number(ctx, WYmin); m.ymax = window_.number(ctx, WYmax);
    m.zmin = window_.number(ctx, WZmin); m.zmax = window_.number(ctx, WZmax);
    const double g = window_.number(ctx, WGrid);
    if (!(g >= 5 && g <= 128) || g != std::floor(g)) throw MathError("Argument ERROR: griglia 5..128");
    m.grid = static_cast<int>(g);
    const std::string err = models::validate(m);
    if (!err.empty()) throw MathError(err);
    ctx.ws.graph3d.model = m;
    ctx.markDirty();
    samplesValid_ = false;
}

void Graph3DApp::key(Context& ctx, const KeyEvent& ev) {
    auto& g3 = ctx.ws.graph3d;
    switch (screen_) {
    case Screen::Edit:
        if (ev.is(Key::Exe)) {
            const std::string text = editor_.text();
            if (text.size() > MaxGraphExpression) throw MathError("Memory ERROR: espressione troppo lunga");
            if (!text.empty()) (void)CompiledExpression(text, "x");   // Syntax ERROR keeps the editor open
            g3.model.z[static_cast<std::size_t>(row_)] = text;
            if (!text.empty()) g3.model.selected[static_cast<std::size_t>(row_)] = true;
            ctx.markDirty();
            samplesValid_ = false;
            screen_ = Screen::List;
        } else if (ev.is(Key::Exit)) {
            screen_ = Screen::List;
        } else {
            (void)editor_.key(ev);
        }
        return;
    case Screen::Window:
        if (!window_.editing() && (ev.is(Key::Exit) || ev.is(Key::F6))) {
            applyWindow(ctx);   // throws on invalid values: the form stays open
            if (ev.is(Key::F6) || windowFromPlot_) { startSampling(ctx); screen_ = Screen::Plot; }
            else screen_ = Screen::List;
            return;
        }
        if (!window_.editing() && ev.is(Key::F1)) {   // INITIAL
            const models::Graph3DModel d;
            window_.fields[WXmin].text = fmt(ctx, d.xmin); window_.fields[WXmax].text = fmt(ctx, d.xmax);
            window_.fields[WYmin].text = fmt(ctx, d.ymin); window_.fields[WYmax].text = fmt(ctx, d.ymax);
            window_.fields[WZmin].text = fmt(ctx, d.zmin); window_.fields[WZmax].text = fmt(ctx, d.zmax);
            window_.fields[WGrid].text = fmt(ctx, d.grid);
            return;
        }
        (void)window_.key(ctx, ev);
        return;
    case Screen::Plot:
        if (ev.is(Key::Exit) || ev.is(Key::F6)) { stopSampling(); screen_ = Screen::List; return; }
        if (ev.is(Key::Left)) { g3d::View v{g3.yaw, g3.pitch, g3.zoom}; g3d::rotate(v, -RotateStep, 0); g3.yaw = v.yawDeg; }
        else if (ev.is(Key::Right)) { g3d::View v{g3.yaw, g3.pitch, g3.zoom}; g3d::rotate(v, RotateStep, 0); g3.yaw = v.yawDeg; }
        else if (ev.is(Key::Up)) { g3d::View v{g3.yaw, g3.pitch, g3.zoom}; g3d::rotate(v, 0, RotateStep); g3.pitch = v.pitchDeg; }
        else if (ev.is(Key::Down)) { g3d::View v{g3.yaw, g3.pitch, g3.zoom}; g3d::rotate(v, 0, -RotateStep); g3.pitch = v.pitchDeg; }
        else if (ev.is(Key::F1) || ev.isChar('+')) g3.zoom = std::min(g3.zoom * ZoomStep, 8.0F);
        else if (ev.is(Key::F2) || ev.isChar('-')) g3.zoom = std::max(g3.zoom / ZoomStep, 0.2F);
        else if (ev.is(Key::F3)) { openWindow(ctx); return; }
        else if (ev.is(Key::F4)) {   // AUTO: Z range of the samples
            if (sampling_) return;
            double lo = 1e300, hi = -1e300;
            for (std::size_t i = 0; i < meshes_.size(); ++i)
                if (active_[i] && meshes_[i].valid) { lo = std::min(lo, meshes_[i].zmin); hi = std::max(hi, meshes_[i].zmax); }
            if (!(lo < hi)) { if (lo == hi && lo < 1e300) { lo -= 1; hi += 1; } else return; }
            const double pad = (hi - lo) * 0.05;
            g3.model.zmin = lo - pad; g3.model.zmax = hi + pad;
        } else if (ev.is(Key::F5)) { const Graph3DState d; g3.yaw = d.yaw; g3.pitch = d.pitch; g3.zoom = d.zoom; }
        else return;
        ctx.markDirty();
        return;
    case Screen::List:
        break;
    }
    // List
    const int rows = static_cast<int>(models::Graph3DModel::Functions);
    if (ev.is(Key::Exit)) { ctx.home(); return; }
    if (ev.is(Key::Up)) { row_ = (row_ + rows - 1) % rows; return; }
    if (ev.is(Key::Down)) { row_ = (row_ + 1) % rows; return; }
    const auto r = static_cast<std::size_t>(row_);
    if (ev.is(Key::F1)) { g3.model.selected[r] = !g3.model.selected[r]; ctx.markDirty(); samplesValid_ = false; return; }
    if (ev.is(Key::F2) || ev.is(Key::DelForward)) { g3.model.z[r].clear(); g3.model.selected[r] = false; ctx.markDirty(); samplesValid_ = false; return; }
    if (ev.is(Key::F3)) { openWindow(ctx); return; }
    if (ev.is(Key::F6)) { startSampling(ctx); screen_ = Screen::Plot; return; }
    if (ev.is(Key::Exe) || ev.is(Key::Char)) {
        editor_.set(ev.is(Key::Exe) ? g3.model.z[r] : std::string());
        if (ev.is(Key::Char)) (void)editor_.key(ev);
        screen_ = Screen::Edit;
    }
}

void Graph3DApp::drawList(Context& ctx, gfx::Canvas& c) {
    const auto& m = ctx.ws.graph3d.model;
    const int top = ui::ContentTop + 6;
    ui::text(c, 8, top, "Superfici z = f(x, y)", Muted, font::Face::Small);
    for (int i = 0; i < static_cast<int>(models::Graph3DModel::Functions); ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const int y = top + 18 + i * 30;
        const bool sel = i == row_;
        c.rect(4, y, ui::ScreenW - 8, 26, sel ? SelectSoft : Bg, true);
        // Selection square in the colour of the surface (as the GRAPH relation list).
        const std::uint16_t color = g3d::surfaceColor(idx, 0.55F, 1.0F);
        c.rect(10, y + 7, 12, 12, color, m.selected[idx]);
        c.rect(10, y + 7, 12, 12, Ink, false);
        char label[8];
        std::snprintf(label, sizeof label, "Z%d=", i + 1);
        ui::text(c, 28, y + 5, label, Ink);
        if (screen_ == Screen::Edit && sel) editor_.draw(c, 64, y + 5, ui::ScreenW - 76, Ink);
        else ui::textFit(c, 64, y + 5, m.z[idx].empty() ? std::string("--") : m.z[idx], ui::ScreenW - 76, m.z[idx].empty() ? Faint : Ink);
    }
    const int infoY = top + 18 + 3 * 30 + 10;
    char buf[96];
    std::snprintf(buf, sizeof buf, "X %g..%g  Y %g..%g  Z %g..%g", m.xmin, m.xmax, m.ymin, m.ymax, m.zmin, m.zmax);
    ui::text(c, 8, infoY, buf, Muted, font::Face::Small);
    const std::size_t maxGrid = limits::graph3dGrid(viewportPixels());
    std::snprintf(buf, sizeof buf, "Griglia %d (massimo ora %u), %s", m.grid, static_cast<unsigned>(maxGrid),
                  limits::graph3dZBuffer(viewportPixels()) ? "z-buffer in PSRAM" : "algoritmo del pittore (SRAM)");
    ui::text(c, 8, infoY + 14, buf, Muted, font::Face::Small);
    if (screen_ == Screen::Edit) {
        ui::text(c, 8, ui::ContentBottom - 22, "EXE conferma  EXIT annulla  (variabili x, y)", Muted, font::Face::Small);
        const char* k[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, k);
        return;
    }
    const char* k[6] = {"SELECT", "DELETE", "V-WIN", nullptr, nullptr, "DRAW"};
    ui::softKeys(c, k);
}

void Graph3DApp::drawPlot(Context& ctx, gfx::Canvas& c) {
    const auto& g3 = ctx.ws.graph3d;
    const gfx::Rect vp = Viewport;
    c.rect(vp.x, vp.y, vp.w, vp.h, Bg, true);
    char buf[112];
    if (sampling_) {
        std::size_t total = 0, done = 0;
        for (std::size_t i = 0; i < meshes_.size(); ++i) {
            if (!active_[i]) continue;
            total += meshes_[i].verts.size();
            done += i < sampleSurface_ ? meshes_[i].verts.size() : i == sampleSurface_ ? sampleNext_ : 0;
        }
        std::snprintf(buf, sizeof buf, "Calcolo delle superfici... %u%%", static_cast<unsigned>(total ? done * 100 / total : 0));
        ui::textCenter(c, vp.x + vp.w / 2, vp.y + vp.h / 2 - 8, buf, Muted);
    } else if (samplesValid_) {
        const g3d::Mesh* list[models::Graph3DModel::Functions] = {};
        const std::size_t n = meshes_.size();
        for (std::size_t i = 0; i < n; ++i) list[i] = active_[i] ? &meshes_[i] : nullptr;
        const g3d::View view{g3.yaw, g3.pitch, g3.zoom};
        try {
            stats_ = g3d::render(c, list, n, windowOf(g3.model), view, vp, zbuffer_);
        } catch (const std::bad_alloc&) {
            // No room for the depth buffer: fall back to the painter's algorithm.
            zbuffer_ = false;
            stats_ = g3d::render(c, list, n, windowOf(g3.model), view, vp, false);
        }
        const gfx::Rect full{0, 0, ui::ScreenW, ui::ScreenH};
        c.setClip(full);
    }
    // Header: profile and orientation.
    std::snprintf(buf, sizeof buf, "griglia %u  %s  rot %d/%d  zoom %.2g", static_cast<unsigned>(grid_),
                  zbuffer_ ? "z-buffer" : "pittore", static_cast<int>(g3.yaw), static_cast<int>(g3.pitch),
                  static_cast<double>(g3.zoom));
    ui::text(c, 6, ui::ContentTop + 4, buf, Muted, font::Face::Small);
    if (!message_.empty()) ui::text(c, 6, vp.y + vp.h - 14, message_.c_str(), Error, font::Face::Small);
    const char* k[6] = {"ZOOM+", "ZOOM-", "V-WIN", "AUTO", "VIEW", "LIST"};
    ui::softKeys(c, k);
}

void Graph3DApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "3D GRAPH");
    switch (screen_) {
    case Screen::List:
    case Screen::Edit:
        drawList(ctx, c);
        return;
    case Screen::Window: {
        ui::text(c, 8, ui::ContentTop + 6, "V-Window 3D", Muted, font::Face::Small);
        window_.draw(c, ui::ContentTop + 22, ui::ContentBottom - 4);
        const char* k[6] = {"INITIAL", nullptr, nullptr, nullptr, nullptr, "DRAW"};
        ui::softKeys(c, k);
        return;
    }
    case Screen::Plot:
        drawPlot(ctx, c);
        return;
    }
}
} // namespace fx::app
