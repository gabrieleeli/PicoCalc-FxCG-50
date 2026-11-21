// 3D GRAPH graph screen (v1.14.0, fx-CG50 manual 16-14...16-19): progressive
// computation of the lattices (AC stops it), drawing with the z-buffer or the
// painter's algorithm, rotation with the cursor keys and Auto Rotate, ZOOM
// (IN, OUT, VIEW-X/Y/Z, ORIGINAL), TRACE with Next/Back and the coordinate
// pop-up, SKETCH (Cls, Plot, Text), G-Solve CROSS / INTSECT / RELATION,
// PICTURE STORE and CAPTURE.
//
// Keys (as GRAPH on the PicoCalc): F1 TRACE, F2 ZOOM, F3 V-WIN, F4 SKETCH,
// F5 G-SOLVE, F6 G<->T, also with SHIFT; OPTN: PICTURE, ROTATE, Z-AUTO
// (extension); cursor keys rotate; '.' cycles Axes/Box, ',' Label; AC or EXIT
// back to the list (AC stops the computation and the auto rotation).
#include "apps.hpp"
#include "file_store.hpp"
#include "graph3d.hpp"
#include "graph3d_model.hpp"
#include "limits.hpp"
#include "pict.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>

namespace fx::app {
using namespace ui::theme;

namespace {
constexpr std::size_t PointsPerTick = 600;          // evaluations per tick: the keyboard stays responsive
constexpr double RotateStep = 15.0;                 // (scelta) degrees per cursor key
constexpr double AutoStep = 10.0;                   // Auto Rotate: 36 steps per turn
constexpr int AutoSteps = 72;                       // stops after two turns (16-14)
constexpr std::uint32_t AutoInterval = 40;          // ms between two frames of Auto Rotate
constexpr float ZoomFactor = 1.5F;
constexpr double TwoPi = 6.283185307179586;
constexpr std::uint16_t PointerColor = gfx::rgb888To565(230, 0, 200);
constexpr std::uint16_t SectionPlaneColor = gfx::rgb888To565(40, 190, 70);

std::size_t viewportPixels() {
    return static_cast<std::size_t>(Graph3DApp::Viewport.w) * static_cast<std::size_t>(Graph3DApp::Viewport.h);
}
const char* axisName(g3d::Axis a) { return a == g3d::Axis::X ? "X" : a == g3d::Axis::Y ? "Y" : "Z"; }
void axisRange(const g3d::Window& w, g3d::Axis a, double& lo, double& hi) {
    lo = a == g3d::Axis::X ? w.xmin : a == g3d::Axis::Y ? w.ymin : w.zmin;
    hi = a == g3d::Axis::X ? w.xmax : a == g3d::Axis::Y ? w.ymax : w.zmax;
}
// Text on the graph with a white background (coordinates, results).
void label(gfx::Canvas& c, int x, int y, const std::string& s, std::uint16_t color, font::Face face = font::Face::Medium, bool right = false) {
    const font::TextMetrics m = font::measure(face, s.c_str());
    const int w = std::min(m.width, ui::ScreenW - 4);
    const int lx = right ? x - w : x;
    c.rect(lx - 1, y, w + 2, m.height, Bg, true);
    ui::text(c, lx, y, s.c_str(), color, face, ui::ScreenW - lx);
}
bool periodicU(g3d::Kind k) { return k == g3d::Kind::Sphere || k == g3d::Kind::Cylinder || k == g3d::Kind::Cone; }
bool periodicV(g3d::Kind k) { return k == g3d::Kind::RotX || k == g3d::Kind::RotY; }
} // namespace

// ---------------------------------------------------------------- computation
void Graph3DApp::stopSampling() noexcept {
    sampling_ = false;
    sampleGraph_ = samplePatch_ = sampleNext_ = 0;
}

void Graph3DApp::releaseScene() noexcept {
    stopSampling();
    for (auto& ms : meshes_) for (auto& m : ms) m.clear();
    layer_.clear(); layer_.shrink_to_fit();
    layerValid_ = false;
    samplesValid_ = false;
    picture_ = pict::Image{};
    pictPending_ = 0; pictReady_ = false;
}

std::size_t Graph3DApp::retainedBytes() const noexcept {
    std::size_t n = layer_.capacity() * sizeof(std::uint16_t);
    for (const auto& ms : meshes_) for (const auto& m : ms) n += m.bytes();
    if (!capture_.empty()) n += 1;
    if (!picture_.empty()) n += 1;
    return n;
}

void Graph3DApp::startDraw(Context& ctx) {
    Graph3DState& g3 = ctx.ws.graph3d;
    const g3d::Window& w = g3.set.window;
    const std::string werr = g3d::validate(w);
    if (!werr.empty()) throw MathError(werr);
    ctx.ws.apply();                                               // angle unit of the functions
    releaseScene();
    std::size_t graphs = 0;
    for (std::size_t i = 0; i < g3d::Functions; ++i) {
        const g3d::Function& f = g3.set.fns[i];
        active_[i] = f.selected && f.defined();
        for (auto& e : compiled_[i]) e = CompiledExpression{};
        patchCount_[i] = 0;
        lineVisible_[i] = false;
        shapes_[i] = g3d::Shape{};
        shapes_[i].kind = f.kind;                                 // the kind of every graph (templates: the whole shape)
        if (!active_[i]) continue;
        const std::string err = g3d::validate(f);
        if (!err.empty()) throw MathError(err);
        switch (f.kind) {
        case g3d::Kind::Z: compiled_[i][0] = CompiledExpression(f.expr[0], "x"); break;   // Syntax ERROR before drawing
        case g3d::Kind::Param: for (std::size_t k = 0; k < 3; ++k) compiled_[i][k] = CompiledExpression(f.expr[k], "s"); break;
        case g3d::Kind::RotX: case g3d::Kind::RotY: compiled_[i][0] = CompiledExpression(f.expr[0], "x"); break;
        default: {
            const std::string e = g3d::shapeOf(f, shapes_[i]);
            if (!e.empty()) throw MathError(e);
            break;
        }
        }
        ++graphs;
    }
    zbuffer_ = limits::graph3dZBuffer(viewportPixels());
    std::size_t limit = limits::graph3dPoints(viewportPixels(), std::max<std::size_t>(graphs, 1));
    for (;;) {
        try {
            for (std::size_t i = 0; i < g3d::Functions; ++i) {
                if (!active_[i]) continue;
                const g3d::Function& f = g3.set.fns[i];
                const auto nx = std::min<std::size_t>(static_cast<std::size_t>(w.xgrid), limit);
                const auto ny = std::min<std::size_t>(static_cast<std::size_t>(w.ygrid), limit);
                auto& spec = specs_[i];
                switch (f.kind) {
                case g3d::Kind::Z: spec[0] = {{w.xmin, w.xmax, w.ymin, w.ymax}, nx, ny}; patchCount_[i] = 1; break;
                case g3d::Kind::Param:
                    spec[0] = {{w.smin, w.smax, w.tmin, w.tmax}, std::min<std::size_t>(static_cast<std::size_t>(w.sgrid), limit),
                               std::min<std::size_t>(static_cast<std::size_t>(w.tgrid), limit)};
                    patchCount_[i] = 1;
                    break;
                case g3d::Kind::RotX: case g3d::Kind::RotY: spec[0] = {{w.xmin, w.xmax, 0, TwoPi}, nx, ny}; patchCount_[i] = 1; break;
                case g3d::Kind::Line: {
                    double t0, t1;
                    lineVisible_[i] = g3d::clipLine(w, shapes_[i].p, shapes_[i].d, t0, t1);
                    if (lineVisible_[i]) {
                        lineT0_[i] = t0; lineT1_[i] = t1;
                        lineA_[i] = shapes_[i].p + shapes_[i].d * t0;
                        lineB_[i] = shapes_[i].p + shapes_[i].d * t1;
                    }
                    patchCount_[i] = 0;
                    break;
                }
                default: patchCount_[i] = g3d::templatePatches(shapes_[i], w, nx, ny, spec.data()); break;
                }
                for (std::size_t p = 0; p < patchCount_[i]; ++p) g3d::beginSample(meshes_[i][p], spec[p].nu, spec[p].nv);
            }
            break;
        } catch (const std::bad_alloc&) {
            for (auto& ms : meshes_) for (auto& m : ms) m.clear();
            if (limit <= 2) throw;                                  // Memory ERROR, nothing kept
            limit = std::max<std::size_t>(2, limit / 2);
        }
    }
    points_ = limit;
    drawnWindow_ = w;
    originalTheta_ = w.theta; originalPhi_ = w.phi;               // ZOOM ORIGINAL
    sampling_ = true;
    samplesValid_ = false;
    sampleGraph_ = samplePatch_ = sampleNext_ = 0;
    mode_ = Mode::View; sub_ = Sub::None;
    marks_.clear();
    message_.clear();
    autoSteps_ = 0;
    memoryError_ = false;
    screen_ = Screen::Plot;
}

void Graph3DApp::showPlot(Context& ctx) {
    const g3d::Window& w = ctx.ws.graph3d.set.window;
    const bool sameBox = drawnWindow_.xmin == w.xmin && drawnWindow_.xmax == w.xmax && drawnWindow_.ymin == w.ymin &&
                         drawnWindow_.ymax == w.ymax && drawnWindow_.zmin == w.zmin && drawnWindow_.zmax == w.zmax;
    if ((samplesValid_ || sampling_) && sameBox) { screen_ = Screen::Plot; mode_ = Mode::View; sub_ = Sub::None; return; }
    startDraw(ctx);
}

bool Graph3DApp::pointOf(Context& ctx, int graph, double u, double v, g3d::Vec3& out) const {
    const bool ok = pointOfRaw(ctx, graph, u, v, out);
    if (ok) {
        // Rounding noise of cos/sin (2.4E-16 instead of 0) is not shown.
        const double scale = std::max({1.0, std::fabs(out.x), std::fabs(out.y), std::fabs(out.z)});
        for (double* c : {&out.x, &out.y, &out.z}) if (std::fabs(*c) < 1e-12 * scale) *c = 0;
    }
    return ok;
}
bool Graph3DApp::pointOfRaw(Context& ctx, int graph, double u, double v, g3d::Vec3& out) const {
    if (graph < 0 || graph >= static_cast<int>(g3d::Functions)) return false;
    const auto gi = static_cast<std::size_t>(graph);
    const CalculatorMemory* memory = &ctx.ws.memory;
    try {
        switch (shapes_[gi].kind) {
        case g3d::Kind::Z: out = {u, v, compiled_[gi][0].eval2(u, "y", v, memory)}; return true;
        case g3d::Kind::Param:
            out = {compiled_[gi][0].eval2(u, "t", v, memory), compiled_[gi][1].eval2(u, "t", v, memory), compiled_[gi][2].eval2(u, "t", v, memory)};
            return true;
        case g3d::Kind::RotX: { const double y = compiled_[gi][0](u, memory); out = {u, y * std::cos(v), y * std::sin(v)}; return true; }
        case g3d::Kind::RotY: { const double y = compiled_[gi][0](u, memory); out = {u * std::cos(v), y, u * std::sin(v)}; return true; }
        case g3d::Kind::Line: out = shapes_[gi].p + shapes_[gi].d * u; return true;
        case g3d::Kind::None: return false;
        default: return g3d::templatePoint(shapes_[gi], 0, u, v, out);
        }
    } catch (const std::bad_alloc&) {
        throw;
    } catch (...) {
        return false;                                               // undefined point: a hole
    }
}

void Graph3DApp::finishSampling(Context& ctx) {
    while (sampling_) (void)tick(ctx);
}

bool Graph3DApp::tick(Context& ctx) {
    bool redraw = false;
    if (memoryError_) {
        memoryError_ = false;
        releaseScene();
        screen_ = Screen::List;
        throw MathError("Memory ERROR: memoria insufficiente per il grafico 3D");
    }
    if (pictReady_) {                                             // PICTURE STORE (captured by drawPlot)
        pictReady_ = false;
        const int n = pictPending_;
        pictPending_ = 0;
        pict::Image img = std::move(picture_);
        picture_ = pict::Image{};
        files::FileSystem* fs = userFiles(ctx);
        if (!fs) throw MathError("Storage ERROR: archivio non disponibile");
        files::Status st = fs->makeDirs(files::PictFolder);
        if (st == files::Status::Ok) {
            const auto data = pict::serialize(img);
            st = files::writeAtomic(*fs, files::join(files::PictFolder, pict::memoryFileName(n)), data.data(), data.size());
        }
        if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
        ctx.toast("Pict " + std::to_string(n) + " memorizzata");
        redraw = true;
    }
    if (sampling_) {
        std::size_t budget = PointsPerTick;
        while (budget > 0 && sampleGraph_ < g3d::Functions) {
            const std::size_t g = sampleGraph_;
            if (!active_[g] || samplePatch_ >= patchCount_[g]) { ++sampleGraph_; samplePatch_ = 0; sampleNext_ = 0; continue; }
            const std::size_t patch = samplePatch_;
            const g3d::PointFn fn = [this, &ctx, g, patch](double u, double v, g3d::Vec3& out) {
                if (!g3d::isTemplate(shapes_[g].kind)) return pointOf(ctx, static_cast<int>(g), u, v, out);
                return g3d::templatePoint(shapes_[g], patch, u, v, out);
            };
            const std::size_t before = sampleNext_;
            const bool done = g3d::sampleStep(meshes_[g][patch], fn, specs_[g][patch].domain, sampleNext_, budget);
            budget -= std::min(budget, sampleNext_ - before);
            if (done) { ++samplePatch_; sampleNext_ = 0; }
        }
        if (sampleGraph_ >= g3d::Functions) {
            sampling_ = false;
            samplesValid_ = true;
            ++dataGen_;
            layerValid_ = false;
            bool anyFunction = false, anyPoint = false;
            for (std::size_t i = 0; i < g3d::Functions; ++i) {
                const g3d::Kind k = ctx.ws.graph3d.set.fns[i].kind;
                if (!active_[i] || g3d::isTemplate(k)) continue;
                anyFunction = true;
                anyPoint = anyPoint || meshes_[i][0].valid > 0;
            }
            if (anyFunction && !anyPoint) message_ = "Math ERROR: funzioni non definite nella finestra";
        }
        redraw = true;
    }
    if (autoSteps_ > 0 && screen_ == Screen::Plot && !sampling_) {
        const std::uint32_t now = ctx.now();
        if (now - autoLast_ >= AutoInterval) {
            autoLast_ = now;
            --autoSteps_;
            const double dt = autoDir_ == 0 ? AutoStep : autoDir_ == 1 ? -AutoStep : 0;
            const double dp = autoDir_ == 2 ? -AutoStep : autoDir_ == 3 ? AutoStep : 0;
            g3d::Window& w = ctx.ws.graph3d.set.window;
            w.theta = g3d::normalizeTheta(w.theta + dt);
            w.phi = g3d::normalizePhi(w.phi + dp);
            if (autoSteps_ == 0) ctx.markDirty();
            redraw = true;
        }
    }
    return redraw;
}

// ---------------------------------------------------------------- view changes
void Graph3DApp::setView(Context& ctx, double theta, double phi, float zoom) {
    g3d::Window& w = ctx.ws.graph3d.set.window;
    w.theta = g3d::normalizeTheta(theta);
    w.phi = g3d::normalizePhi(phi);
    ctx.ws.graph3d.zoom = std::clamp(zoom, 0.05F, 20.0F);
    marks_.clear();                                               // (scelta) sketches belong to one view
    ctx.markDirty();
}
void Graph3DApp::rotateBy(Context& ctx, double dTheta, double dPhi) {
    const g3d::Window& w = ctx.ws.graph3d.set.window;
    setView(ctx, w.theta + dTheta, w.phi + dPhi, ctx.ws.graph3d.zoom);
}
void Graph3DApp::autoZ(Context& ctx) {
    if (sampling_ || !samplesValid_) return;
    double lo = 1e300, hi = -1e300;
    for (std::size_t i = 0; i < g3d::Functions; ++i)
        for (std::size_t p = 0; p < patchCount_[i]; ++p)
            if (active_[i] && meshes_[i][p].valid) { lo = std::min(lo, meshes_[i][p].zmin); hi = std::max(hi, meshes_[i][p].zmax); }
    if (!(lo <= hi)) return;
    if (lo == hi) { lo -= 1; hi += 1; }
    const double pad = (hi - lo) * 0.05;
    g3d::Window w = ctx.ws.graph3d.set.window;
    w.zmin = lo - pad; w.zmax = hi + pad;
    if (!g3d::validate(w).empty()) return;
    ctx.ws.graph3d.set.window = w;
    ctx.markDirty();
    startDraw(ctx);
}

// ---------------------------------------------------------------- keys
void Graph3DApp::plotKey(Context& ctx, const KeyEvent& ev) {
    if (autoSteps_ > 0) {                                         // Auto Rotate: AC stops it (16-14)
        if (ev.is(Key::AC) || ev.is(Key::Exit)) { autoSteps_ = 0; ctx.markDirty(); }
        return;
    }
    if (sampling_) {
        if (ev.is(Key::AC)) {                                     // AC stops the computation: partial graph
            stopSampling();
            samplesValid_ = true;
            ++dataGen_;
            layerValid_ = false;
            ctx.toast("Calcolo interrotto (AC)");
        } else if (ev.is(Key::Exit)) {
            stopSampling();
            screen_ = Screen::List;
        }
        return;
    }
    if (sub_ != Sub::None) { subKey(ctx, ev); return; }
    switch (mode_) {
    case Mode::Trace: traceKey(ctx, ev); return;
    case Mode::Cross: crossKey(ctx, ev); return;
    case Mode::Intsect: case Mode::Relation: solveKey(ctx, ev); return;
    case Mode::SketchPlot: case Mode::SketchText: sketchKey(ctx, ev); return;
    case Mode::View: break;
    }
    switch (ev.key) {
    case Key::F1: startTrace(ctx); return;                                        // TRACE
    case Key::F2: sub_ = Sub::Zoom; return;                                       // ZOOM
    case Key::F3: openWindow(ctx, true); return;                                  // V-WIN
    case Key::F4: sub_ = Sub::Sketch; return;                                     // SKETCH
    case Key::F5: sub_ = Sub::GSolve; return;                                     // G-SOLVE
    case Key::F6: case Key::Exit: case Key::AC: screen_ = Screen::List; return;   // G<->T, back to the list (16-3)
    case Key::Optn: sub_ = Sub::Optn; return;
    case Key::Left: rotateBy(ctx, -RotateStep, 0); return;
    case Key::Right: rotateBy(ctx, RotateStep, 0); return;
    case Key::Up: rotateBy(ctx, 0, RotateStep); return;
    case Key::Down: rotateBy(ctx, 0, -RotateStep); return;
    case Key::Char: {
        g3d::Setup& s = ctx.ws.graph3d.set.setup;
        if (ev.ch == '.') {                                                       // Axes/Box cycle (16-1)
            if (s.axes && s.box) s.box = false;
            else if (s.axes) { s.axes = false; s.box = true; }
            else if (s.box) s.box = false;
            else { s.axes = true; s.box = true; }
            ctx.markDirty();
        } else if (ev.ch == ',') {                                                // Label (16-2)
            s.label = !s.label;
            ctx.markDirty();
        } else if (ev.ch == '+' || ev.ch == '-') {                                // zoom (as GRAPH)
            const float z = ctx.ws.graph3d.zoom;
            setView(ctx, ctx.ws.graph3d.set.window.theta, ctx.ws.graph3d.set.window.phi, ev.ch == '+' ? z * ZoomFactor : z / ZoomFactor);
        }
        return;
    }
    default: return;
    }
}

void Graph3DApp::subKey(Context& ctx, const KeyEvent& ev) {
    if (ev.is(Key::Exit) || ev.is(Key::AC)) {
        sub_ = sub_ == Sub::CrossAxis ? Sub::GSolve : sub_ == Sub::Picture || sub_ == Sub::AutoRotate ? Sub::Optn : Sub::None;
        return;
    }
    const Sub s = sub_;
    const int k = ev.key >= Key::F1 && ev.key <= Key::F6 ? static_cast<int>(ev.key) - static_cast<int>(Key::F1) + 1 : 0;
    if (k == 0) return;
    sub_ = Sub::None;
    const Graph3DState& g3 = ctx.ws.graph3d;
    switch (s) {
    case Sub::Zoom:                                                // 16-15
        switch (k) {
        case 1: setView(ctx, g3.set.window.theta, g3.set.window.phi, g3.zoom * ZoomFactor); return;
        case 2: setView(ctx, g3.set.window.theta, g3.set.window.phi, g3.zoom / ZoomFactor); return;
        case 3: setView(ctx, 0, 90, g3.zoom); return;              // VIEW-X: along the positive x axis
        case 4: setView(ctx, 90, 90, g3.zoom); return;             // VIEW-Y
        case 5: setView(ctx, -90, 0, g3.zoom); return;             // VIEW-Z: from above, x to the right
        case 6: setView(ctx, originalTheta_, originalPhi_, 1.0F); return;   // ORIGINAL
        default: return;
        }
    case Sub::Sketch:                                              // 16-15, 16-16
        if (k == 1) { marks_.clear(); return; }
        if (k == 2 || k == 3) {
            mode_ = k == 2 ? Mode::SketchPlot : Mode::SketchText;
            px_ = Viewport.x + Viewport.w / 2; py_ = Viewport.y + Viewport.h / 2;
        }
        return;
    case Sub::GSolve:                                              // 16-16...16-19
        if (k == 1) { sub_ = Sub::CrossAxis; return; }
        if (k == 2) { startSolve(ctx, Mode::Intsect); return; }
        if (k == 3) { startSolve(ctx, Mode::Relation); return; }
        return;
    case Sub::CrossAxis:
        if (k >= 1 && k <= 3) startCross(ctx, static_cast<g3d::Axis>(k - 1));
        return;
    case Sub::Optn:
        if (k == 1) sub_ = Sub::Picture;
        else if (k == 2) sub_ = Sub::AutoRotate;
        else if (k == 3) autoZ(ctx);
        return;
    case Sub::Picture:
        if (k == 1) {
            if (!userFiles(ctx)) throw MathError("Storage ERROR: archivio non disponibile");
            ask(Ask::Kind::PictStore, "Store In Pict (1-20):");
        }
        return;
    case Sub::AutoRotate:                                          // 16-14: L->R, R->L, T->B, B->T
        if (k >= 1 && k <= 4) { autoDir_ = k - 1; autoSteps_ = AutoSteps; autoLast_ = ctx.now() - AutoInterval; mode_ = Mode::View; marks_.clear(); }
        return;
    default:
        return;
    }
}

// ---------------------------------------------------------------- TRACE (16-14, 16-15)
bool Graph3DApp::traceDomain(int graph, double& u0, double& u1, double& v0, double& v1, std::size_t& nu, std::size_t& nv, bool& oneD) const {
    if (graph < 0 || graph >= static_cast<int>(g3d::Functions) || !active_[static_cast<std::size_t>(graph)]) return false;
    const auto gi = static_cast<std::size_t>(graph);
    oneD = false;
    if (shapes_[gi].kind == g3d::Kind::Line && patchCount_[gi] == 0) {
        if (!lineVisible_[gi]) return false;
        u0 = lineT0_[gi]; u1 = lineT1_[gi]; v0 = v1 = 0;
        nu = std::max<std::size_t>(2, std::min<std::size_t>(static_cast<std::size_t>(drawnWindow_.xgrid), points_));
        nv = 1; oneD = true;
        return true;
    }
    if (patchCount_[gi] == 0 || meshes_[gi][0].nu < 2) return false;
    const g3d::Domain& d = specs_[gi][0].domain;
    u0 = d.u0; u1 = d.u1; v0 = d.v0; v1 = d.v1;
    nu = meshes_[gi][0].nu; nv = meshes_[gi][0].nv;
    return true;
}
bool Graph3DApp::tracePoint(Context& ctx, g3d::Vec3& out) const {
    if (mode_ != Mode::Trace) return false;
    return pointOf(ctx, traceGraph_, traceU_, traceV_, out);
}
void Graph3DApp::startTrace(Context& ctx) {
    (void)ctx;
    if (!samplesValid_) return;
    for (int k = 0; k < static_cast<int>(g3d::Functions); ++k) {
        double u0, u1, v0, v1; std::size_t nu, nv; bool oneD;
        if (!traceDomain(k, u0, u1, v0, v1, nu, nv, oneD)) continue;
        traceGraph_ = k;
        // Centre of the lattice; on the angle of a closed surface the point in front (u = 0).
        traceU_ = periodicU(shapes_[static_cast<std::size_t>(k)].kind) ? u0 : g3d::latticeAt(u0, u1, (nu - 1) / 2, nu);
        traceV_ = oneD ? 0 : g3d::latticeAt(v0, v1, (nv - 1) / 2, nv);
        mode_ = Mode::Trace;
        return;
    }
    ctx.toast("Nessun grafico da tracciare");
}
void Graph3DApp::moveTrace(int du, int dv) {
    double u0, u1, v0, v1; std::size_t nu, nv; bool oneD;
    if (!traceDomain(traceGraph_, u0, u1, v0, v1, nu, nv, oneD)) return;
    const g3d::Kind kind = shapes_[static_cast<std::size_t>(traceGraph_)].kind;
    auto step = [](double& x, double a, double b, std::size_t n, int d, bool periodic) {
        if (d == 0 || n < 2) return;
        const double h = (b - a) / static_cast<double>(n - 1);
        x += d * h;
        const double lo = std::min(a, b), hi = std::max(a, b);
        if (periodic) { while (x > hi + 1e-12) x -= hi - lo; while (x < lo - 1e-12) x += hi - lo; }
        else x = std::clamp(x, lo, hi);
    };
    step(traceU_, u0, u1, nu, du, periodicU(kind));
    if (!oneD) step(traceV_, v0, v1, nv, dv, periodicV(kind));
}

void Graph3DApp::traceKey(Context& ctx, const KeyEvent& ev) {
    const g3d::Kind kind = traceGraph_ >= 0 ? shapes_[static_cast<std::size_t>(traceGraph_)].kind : g3d::Kind::None;
    if (ctx.shift() && ev.key >= Key::F2 && ev.key <= Key::F6) {   // the other tools leave TRACE
        mode_ = Mode::View;
        plotKey(ctx, ev);
        return;
    }
    switch (ev.key) {
    case Key::Left: moveTrace(ev.ctrl ? -5 : -1, 0); return;
    case Key::Right: moveTrace(ev.ctrl ? 5 : 1, 0); return;
    case Key::Up: moveTrace(0, ev.ctrl ? 5 : 1); return;
    case Key::Down: moveTrace(0, ev.ctrl ? -5 : -1); return;
    case Key::F1: case Key::F2: {                                  // Next / Back
        if (ctx.shift() && ev.is(Key::F1)) { mode_ = Mode::View; return; }   // SHIFT F1 ends TRACE (16-15)
        const int n = static_cast<int>(g3d::Functions);
        for (int k = 1; k <= n; ++k) {
            const int g = (traceGraph_ + (ev.is(Key::F1) ? k : n - k)) % n;
            double u0, u1, v0, v1; std::size_t nu, nv; bool oneD;
            if (!traceDomain(g, u0, u1, v0, v1, nu, nv, oneD)) continue;
            if (g == traceGraph_) return;
            traceGraph_ = g;
            traceU_ = periodicU(shapes_[static_cast<std::size_t>(g)].kind) ? u0 : g3d::latticeAt(u0, u1, (nu - 1) / 2, nu);
            traceV_ = oneD ? 0 : g3d::latticeAt(v0, v1, (nv - 1) / 2, nv);
            return;
        }
        return;
    }
    case Key::Exit: case Key::AC: mode_ = Mode::View; return;
    case Key::Char: {
        // X,θ,T (the `x` key) or a value: coordinates of the pointer (Z= and Param only, 16-15).
        if (ev.ctrl || (kind != g3d::Kind::Z && kind != g3d::Kind::Param)) return;
        const char* first = kind == g3d::Kind::Param ? "S:" : "X:";
        if (ev.ch == 'x' || ev.ch == 'X') ask(Ask::Kind::TraceU, first);
        else if (std::isdigit(static_cast<unsigned char>(ev.ch)) || ev.ch == '-' || ev.ch == '.' || ev.ch == '(') ask(Ask::Kind::TraceU, first, std::string(1, ev.ch));
        return;
    }
    default: return;
    }
}

// ---------------------------------------------------------------- CROSS (16-16, 16-17)
void Graph3DApp::startCross(Context& ctx, g3d::Axis axis) {
    if (!samplesValid_) return;
    double lo, hi;
    axisRange(ctx.ws.graph3d.set.window, axis, lo, hi);
    crossAxis_ = axis;
    crossValue_ = lo <= 0 && 0 <= hi ? 0.0 : (lo + hi) / 2;
    mode_ = Mode::Cross;
}
void Graph3DApp::crossKey(Context& ctx, const KeyEvent& ev) {
    const g3d::Window& w = ctx.ws.graph3d.set.window;
    double lo, hi;
    axisRange(w, crossAxis_, lo, hi);
    const int grid = crossAxis_ == g3d::Axis::Y ? w.ygrid : w.xgrid;
    const double step = (hi - lo) / std::max(1, grid - 1);
    switch (ev.key) {
    case Key::F1: crossValue_ = std::min(hi, crossValue_ + step); if (std::fabs(crossValue_) < step * 1e-9) crossValue_ = 0; return;   // UP
    case Key::F2: crossValue_ = std::max(lo, crossValue_ - step); if (std::fabs(crossValue_) < step * 1e-9) crossValue_ = 0; return;   // DOWN
    case Key::F3: setView(ctx, 0, 90, ctx.ws.graph3d.zoom); return;
    case Key::F4: setView(ctx, 90, 90, ctx.ws.graph3d.zoom); return;
    case Key::F5: setView(ctx, -90, 0, ctx.ws.graph3d.zoom); return;
    case Key::F6: setView(ctx, originalTheta_, originalPhi_, 1.0F); return;
    case Key::Left: rotateBy(ctx, -RotateStep, 0); return;
    case Key::Right: rotateBy(ctx, RotateStep, 0); return;
    case Key::Up: rotateBy(ctx, 0, RotateStep); return;
    case Key::Down: rotateBy(ctx, 0, -RotateStep); return;
    case Key::Exit: case Key::AC: mode_ = Mode::View; return;
    case Key::Char:
        if (!ev.ctrl && (std::isdigit(static_cast<unsigned char>(ev.ch)) || ev.ch == '-' || ev.ch == '.' || ev.ch == '('))
            ask(Ask::Kind::CrossValue, std::string(axisName(crossAxis_)) + ":", std::string(1, ev.ch));
        return;
    default: return;
    }
}

// ---------------------------------------------------------------- INTSECT / RELATION (16-18, 16-19)
void Graph3DApp::startSolve(Context& ctx, Mode mode) {
    if (!samplesValid_) return;
    int rows[g3d::Functions];
    int n = 0;
    for (int i = 0; i < static_cast<int>(g3d::Functions); ++i)
        if (active_[static_cast<std::size_t>(i)] && g3d::isLineOrPlane(ctx.ws.graph3d.set.fns[static_cast<std::size_t>(i)])) rows[n++] = i;
    if (n < 2)
        throw MathError(mode == Mode::Intsect ? "INTSECT requires: multiple lines or planes. (modelli Line o Plane)"
                                              : "RELATION requires: multiple lines or planes. (modelli Line o Plane)");
    pairCount_ = 0;
    for (int a = 0; a < n; ++a)
        for (int b = a + 1; b < n; ++b) pairs_[static_cast<std::size_t>(pairCount_++)] = {rows[a], rows[b]};
    pairIndex_ = 0;
    mode_ = mode;
}
std::pair<int, int> Graph3DApp::solvePair() const noexcept {
    if ((mode_ != Mode::Intsect && mode_ != Mode::Relation) || pairCount_ == 0) return {-1, -1};
    return pairs_[static_cast<std::size_t>(pairIndex_)];
}
void Graph3DApp::solveKey(Context& ctx, const KeyEvent& ev) {
    switch (ev.key) {
    case Key::F1: pairIndex_ = (pairIndex_ + 1) % std::max(1, pairCount_); return;                       // Next
    case Key::F2: pairIndex_ = (pairIndex_ + pairCount_ - 1) % std::max(1, pairCount_); return;          // Back
    case Key::Left: rotateBy(ctx, -RotateStep, 0); return;
    case Key::Right: rotateBy(ctx, RotateStep, 0); return;
    case Key::Up: rotateBy(ctx, 0, RotateStep); return;
    case Key::Down: rotateBy(ctx, 0, -RotateStep); return;
    case Key::Exit: case Key::AC: mode_ = Mode::View; return;                                            // EXIT clears the result
    default: return;
    }
}

// ---------------------------------------------------------------- SKETCH (16-15, 16-16)
void Graph3DApp::sketchKey(Context& ctx, const KeyEvent& ev) {
    const int step = ev.ctrl ? 8 : 1;
    switch (ev.key) {
    case Key::Left: px_ = std::max(Viewport.x, px_ - step); return;
    case Key::Right: px_ = std::min(Viewport.x + Viewport.w - 1, px_ + step); return;
    case Key::Up: py_ = std::max(Viewport.y, py_ - step); return;
    case Key::Down: py_ = std::min(Viewport.y + Viewport.h - 1, py_ + step); return;
    case Key::Exe:
        if (mode_ == Mode::SketchPlot) { if (marks_.size() < 200) marks_.push_back({px_, py_, false, {}}); }
        else ask(Ask::Kind::SketchText, "Text:");
        return;
    case Key::Exit: case Key::AC: mode_ = Mode::View; return;
    case Key::Char:
        if (mode_ == Mode::SketchText && !ev.ctrl && static_cast<unsigned char>(ev.ch) >= 0x20) ask(Ask::Kind::SketchText, "Text:", std::string(1, ev.ch));
        return;
    default: (void)ctx; return;
    }
}

// ---------------------------------------------------------------- PICTURE and CAPTURE
void Graph3DApp::storePicture(Context& ctx, int n) {
    if (!userFiles(ctx)) throw MathError("Storage ERROR: archivio non disponibile");
    pictPending_ = n;                                              // the graph area is captured by the next drawPlot()
    pictReady_ = false;
}
void Graph3DApp::takeCapture(Context& ctx) {
    capture_ = pict::Image{};
    const gfx::Canvas* screen = ctx.screen();
    if (!screen || !pict::encode(screen->pixels(), screen->width(), screen->width(), screen->height(), capture_)) {
        capture_ = pict::Image{};
        throw MathError("Memory ERROR: schermata troppo complessa");
    }
    ask(Ask::Kind::Capture, "CAPTURE (1-20):");
}
void Graph3DApp::writeCapture(Context& ctx, int n) {
    if (capture_.empty()) return;
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) { capture_ = pict::Image{}; throw MathError("Storage ERROR: archivio non disponibile"); }
    char name[32];
    std::snprintf(name, sizeof(name), "CAPT%02d%s", n, files::PictExtension);
    files::Status st = fs->makeDirs(files::CaptureFolder);
    if (st == files::Status::Ok) {
        const auto data = pict::serialize(capture_);
        st = files::writeAtomic(*fs, files::join(files::CaptureFolder, name), data.data(), data.size());
    }
    capture_ = pict::Image{};
    if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
    ctx.toast("Capture " + std::to_string(n) + " salvata");
}

// ---------------------------------------------------------------- drawing
std::string Graph3DApp::graphLabel(Context& ctx, int graph) const {
    const g3d::Function& f = ctx.ws.graph3d.set.fns[static_cast<std::size_t>(graph)];
    std::string s = std::to_string(graph + 1) + ":";
    switch (f.kind) {
    case g3d::Kind::Z: return s + "Z=" + f.expr[0];
    case g3d::Kind::RotX: case g3d::Kind::RotY: return s + g3d::kindName(f.kind) + " Y=" + f.expr[0];
    default: return s + g3d::kindName(f.kind);
    }
}

void Graph3DApp::drawPlot(Context& ctx, gfx::Canvas& c) {
    const gfx::Rect vp = Viewport;
    const Graph3DState& g3 = ctx.ws.graph3d;
    c.rect(vp.x, vp.y, vp.w, vp.h, Bg, true);
    const g3d::Camera cam{g3.set.window.theta, g3.set.window.phi, g3.zoom};
    const g3d::Projector proj(g3.set.window, cam, vp);
    if (sampling_) {
        std::size_t total = 0, done = 0;
        for (std::size_t i = 0; i < g3d::Functions; ++i)
            for (std::size_t p = 0; p < patchCount_[i]; ++p) {
                if (!active_[i]) continue;
                const std::size_t n = meshes_[i][p].verts.size();
                total += n;
                done += i < sampleGraph_ || (i == sampleGraph_ && p < samplePatch_) ? n : (i == sampleGraph_ && p == samplePatch_ ? sampleNext_ : 0);
            }
        char buf[64];
        std::snprintf(buf, sizeof buf, "Calcolo... %u%%  (AC: interrompe)", static_cast<unsigned>(total ? done * 100 / total : 0));
        ui::textCenter(c, vp.x + vp.w / 2, vp.y + vp.h / 2 - 8, buf, Muted);
    } else if (samplesValid_) {
        const LayerKey key{dataGen_, cam.theta, cam.phi, cam.zoom, g3.set.setup, zbuffer_};
        const std::size_t pixels = viewportPixels();
        if (layerValid_ && key == layerKey_ && layer_.size() == pixels) {
            c.copyFrom(vp, layer_.data(), vp.w);                   // TRACE / SKETCH: the scene is not drawn again
        } else {
            g3d::Item items[g3d::Functions];
            std::size_t n = 0;
            for (std::size_t i = 0; i < g3d::Functions; ++i) {
                if (!active_[i]) continue;
                g3d::Item& it = items[n];
                it = g3d::Item{};
                g3d::itemColors(g3.set.fns[i], it);
                if (shapes_[i].kind == g3d::Kind::Line) {
                    if (!lineVisible_[i]) continue;
                    it.line = true; it.a = lineA_[i]; it.b = lineB_[i];
                } else {
                    it.patchCount = patchCount_[i];
                    for (std::size_t p = 0; p < patchCount_[i]; ++p) it.patches[p] = &meshes_[i][p];
                }
                ++n;
            }
            try {
                stats_ = g3d::render(c, items, n, g3.set.window, g3.set.setup, cam, vp, zbuffer_);
            } catch (const std::bad_alloc&) {
                if (!zbuffer_) { memoryError_ = true; return; }
                zbuffer_ = false;                                  // no room for the depth buffer: painter's algorithm
                c.rect(vp.x, vp.y, vp.w, vp.h, Bg, true);
                try { stats_ = g3d::render(c, items, n, g3.set.window, g3.set.setup, cam, vp, false); }
                catch (const std::bad_alloc&) { memoryError_ = true; return; }
            }
            layerValid_ = false;
            if (limits::ram().graphLayerCache) {
                try {
                    layer_.resize(pixels);
                    c.copyTo(vp, layer_.data(), vp.w);
                    layerKey_ = LayerKey{dataGen_, cam.theta, cam.phi, cam.zoom, g3.set.setup, zbuffer_};
                    layerValid_ = true;
                } catch (const std::bad_alloc&) {
                    layer_.clear(); layer_.shrink_to_fit();
                }
            }
        }
        if (pictPending_ && !pictReady_) {
            // PICTURE STORE: the graph area (the V-Window is not stored, 16-14).
            picture_ = pict::Image{};
            if (pict::encode(c.pixels() + static_cast<std::size_t>(vp.y) * static_cast<std::size_t>(c.width()), c.width(), vp.w, vp.h, picture_)) pictReady_ = true;
            else { pictPending_ = 0; ctx.toast("Memory ERROR: immagine troppo complessa", 2500); }
        }
        drawOverlays(ctx, c, proj);
    }
    if (!message_.empty() && mode_ == Mode::View) label(c, 4, vp.y + vp.h - 16, message_, Error, font::Face::Small);
    // Function menu of the screen.
    const char* k[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    switch (sub_) {
    case Sub::Zoom: { const char* z[6] = {"IN", "OUT", "VIEW-X", "VIEW-Y", "VIEW-Z", "ORIGINAL"}; std::copy(z, z + 6, k); break; }
    case Sub::Sketch: { const char* z[6] = {"Cls", "Plot", "Text", nullptr, nullptr, nullptr}; std::copy(z, z + 6, k); break; }
    case Sub::GSolve: { const char* z[6] = {"CROSS", "INTSECT", "RELATION", nullptr, nullptr, nullptr}; std::copy(z, z + 6, k); break; }
    case Sub::CrossAxis: { const char* z[6] = {"X", "Y", "Z", nullptr, nullptr, nullptr}; std::copy(z, z + 6, k); break; }
    case Sub::Optn: { const char* z[6] = {"PICTURE", "ROTATE", "Z-AUTO", nullptr, nullptr, nullptr}; std::copy(z, z + 6, k); break; }
    case Sub::Picture: { const char* z[6] = {"STORE", nullptr, nullptr, nullptr, nullptr, nullptr}; std::copy(z, z + 6, k); break; }
    case Sub::AutoRotate: { const char* z[6] = {"L\xE2\x86\x92R", "R\xE2\x86\x92L", "T\xE2\x86\x92" "B", "B\xE2\x86\x92T", nullptr, nullptr}; std::copy(z, z + 6, k); break; }
    default:
        switch (mode_) {
        case Mode::View: { const char* z[6] = {"TRACE", "ZOOM", "V-WIN", "SKETCH", "G-SOLV", "G\xE2\x87\x94T"}; std::copy(z, z + 6, k); break; }
        case Mode::Trace: case Mode::Intsect: case Mode::Relation: { const char* z[6] = {"Next", "Back", nullptr, nullptr, nullptr, nullptr}; std::copy(z, z + 6, k); break; }
        case Mode::Cross: { const char* z[6] = {"UP", "DOWN", "VIEW-X", "VIEW-Y", "VIEW-Z", "ORIGINAL"}; std::copy(z, z + 6, k); break; }
        default: break;
        }
        break;
    }
    if (sampling_) { const char* z[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}; std::copy(z, z + 6, k); }
    ui::softKeys(c, k);
}

void Graph3DApp::drawOverlays(Context& ctx, gfx::Canvas& c, const g3d::Projector& proj) {
    const gfx::Rect vp = Viewport;
    const Graph3DState& g3 = ctx.ws.graph3d;
    const g3d::Window& w = g3.set.window;
    const gfx::Rect oldClip = c.clip();
    c.setClip(vp);
    // SKETCH: points and texts in the Plot/LineCol colour.
    const std::uint16_t plot = g3d::color565(g3.set.setup.plot);
    for (const Mark& m : marks_) {
        if (m.text) ui::text(c, m.x, m.y, m.s.c_str(), plot);
        else c.rect(m.x - 1, m.y - 1, 3, 3, plot, true);
    }
    const int bottom = vp.y + vp.h - 2;
    switch (mode_) {
    case Mode::Trace: {
        if (traceGraph_ < 0) break;
        const auto gi = static_cast<std::size_t>(traceGraph_);
        const g3d::Function& f = g3.set.fns[gi];
        label(c, 2, vp.y + 2, graphLabel(ctx, traceGraph_), g3d::color565(f.area != g3d::Color::Clear ? f.area : f.line));
        g3d::Vec3 p;
        const bool ok = pointOf(ctx, traceGraph_, traceU_, traceV_, p);
        label(c, 2, bottom - 48, "X=" + (ok ? fmt(ctx, p.x) : std::string()), Ink);
        label(c, 2, bottom - 32, "Y=" + (ok ? fmt(ctx, p.y) : std::string()), Ink);
        label(c, 2, bottom - 16, "Z=" + (ok ? fmt(ctx, p.z) : std::string()), Ink);
        label(c, vp.x + vp.w - 4, vp.y + vp.h / 2, "TRACE", Ink, font::Face::Medium, true);
        if (ok && g3d::inBox(w, p)) {
            const g3d::ScreenPoint sp = proj.project(p);
            g3d::drawPointer(c, static_cast<int>(std::lround(sp.x)), static_cast<int>(std::lround(sp.y)), PointerColor);
        }
        break;
    }
    case Mode::Cross: {
        g3d::drawSectionPlane(c, proj, w, crossAxis_, crossValue_, SectionPlaneColor);
        g3d::Polyline curves[g3d::MaxSectionCurves];
        for (std::size_t i = 0; i < g3d::Functions; ++i) {
            if (!active_[i] || !g3d::isTemplate(shapes_[i].kind)) continue;     // templates only (16-16)
            bool coincident = false;
            const std::size_t n = g3d::crossSection(shapes_[i], w, crossAxis_, crossValue_, curves, coincident);
            const std::uint16_t color = g3d::oppositeColor(g3.set.fns[i].area);
            for (std::size_t k = 0; k < n; ++k)
                g3d::drawPolyline(c, proj, w, curves[k].pts.data(), curves[k].pts.size(), curves[k].closed, color, coincident ? 3 : 2);
        }
        label(c, 2, vp.y + 2, "[F1]/[F2]:Move cross section", Ink, font::Face::Small);
        label(c, 2, bottom - 16, std::string(axisName(crossAxis_)) + "=" + fmt(ctx, crossValue_), Ink);
        label(c, vp.x + vp.w - 4, bottom - 16, std::string("CROSS ") + axisName(crossAxis_), Ink, font::Face::Medium, true);
        break;
    }
    case Mode::Intsect: case Mode::Relation: {
        if (pairCount_ == 0) break;
        const auto [a, b] = pairs_[static_cast<std::size_t>(pairIndex_)];
        for (int k = 0; k < 2; ++k) {
            const int gr = k == 0 ? a : b;
            const g3d::Function& f = g3.set.fns[static_cast<std::size_t>(gr)];
            label(c, 2, vp.y + 2 + k * 16, std::to_string(gr + 1) + ": " + g3d::kindName(f.kind), g3d::color565(f.area != g3d::Color::Clear ? f.area : f.line));
        }
        const g3d::Shape& sa = shapes_[static_cast<std::size_t>(a)];
        const g3d::Shape& sb = shapes_[static_cast<std::size_t>(b)];
        if (mode_ == Mode::Relation) {
            label(c, 2, vp.y + vp.h / 2, g3d::relationName(g3d::relation(sa, sb)), Ink);
            label(c, vp.x + vp.w - 4, vp.y + vp.h / 2, "RELATION", Ink, font::Face::Medium, true);
            break;
        }
        const g3d::Intersection r = g3d::intersect(sa, sb);
        switch (r.kind) {
        case g3d::Intersection::Kind::Point: {
            label(c, 2, bottom - 48, "X=" + fmt(ctx, r.point.x), Ink);
            label(c, 2, bottom - 32, "Y=" + fmt(ctx, r.point.y), Ink);
            label(c, 2, bottom - 16, "Z=" + fmt(ctx, r.point.z), Ink);
            if (g3d::inBox(w, r.point)) {
                const g3d::ScreenPoint sp = proj.project(r.point);
                g3d::drawPointer(c, static_cast<int>(std::lround(sp.x)), static_cast<int>(std::lround(sp.y)), PointerColor);
            }
            break;
        }
        case g3d::Intersection::Kind::Line: {
            // Line of intersection (16-18), as X=x0+at, Y=y0+bt, Z=z0+ct; drawn on the graph.
            const g3d::Vec3 p = r.point, d = r.direction;
            auto eq = [&](const char* n, double p0, double d0) {
                std::string s = std::string(n) + "=" + fmt(ctx, p0);
                if (d0 != 0) s += (d0 < 0 ? "-" : "+") + fmt(ctx, std::fabs(d0)) + "t";
                return s;
            };
            label(c, 2, bottom - 48, eq("X", p.x, d.x), Ink, font::Face::Small);
            label(c, 2, bottom - 34, eq("Y", p.y, d.y), Ink, font::Face::Small);
            label(c, 2, bottom - 20, eq("Z", p.z, d.z), Ink, font::Face::Small);
            double t0, t1;
            if (g3d::clipLine(w, p, d, t0, t1)) g3d::drawSegment(c, proj, w, p + d * t0, p + d * t1, PointerColor, 2);
            break;
        }
        case g3d::Intersection::Kind::NotFound: label(c, 2, vp.y + vp.h / 2, "NOT FOUND", Ink); break;
        case g3d::Intersection::Kind::Infinite: label(c, 2, vp.y + vp.h / 2, "INFINITE", Ink); break;
        }
        label(c, vp.x + vp.w - 4, vp.y + vp.h / 2, "INTSECT", Ink, font::Face::Medium, true);
        break;
    }
    case Mode::SketchPlot: case Mode::SketchText:
        g3d::drawPointer(c, px_, py_, PointerColor);
        break;
    case Mode::View:
        break;
    }
    c.setClip(oldClip);
}

} // namespace fx::app
