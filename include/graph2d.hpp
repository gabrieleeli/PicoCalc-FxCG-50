#pragma once
// Grafico 2D condiviso con schermo diviso (v1.15.0, F7; solo Pimoroni Pico Plus 2 W).
//
// Generalizza GraphCore per le modalita' che disegnano sul piano cartesiano
// (Dual Screen di Graph/Table, Recursion, Dyna Graph, Conic Graphs,
// Financial, Picture Plot, Geometry):
//  * Layout: schermo pieno o diviso come la fx-CG50 (G+G, GtoT, T+G): due
//    graph::Viewport affiancati, il grafico principale a sinistra;
//  * PointSource: sorgenti di punti generiche (funzione y=f(x), successione,
//    curva parametrica come le coniche, liste di punti, oggetti geometrici);
//  * Plotter: campionamento a passi con un budget (StepTask) in polilinee di
//    schermo; il disegno incrementale coincide al pixel con un ridisegno completo;
//  * TraceCursor e ZOOM comuni sulla V-Window unica di `View` (ViewWindow).
#include "graph_engine.hpp"
#include "graphics_engine.hpp"
#include "step_task.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace fx::graph2d {

// ---- schermo diviso (manuale 5-23, 5-36: Dual Screen G+G, GtoT, T+G)
enum class Layout : std::uint8_t { Full = 0, GraphGraph = 1, GraphTable = 2, TableGraph = 3 };
struct Panes {
    graph::Viewport main;                       // grafico principale
    graph::Viewport sub;                        // secondo grafico (G+G) o zona della tabella
    gfx::Rect mainRect{}, subRect{};
    bool split{false};
    bool subIsTable{false};                     // GtoT / T+G: la parte "sub" e' una tabella
    bool tableLeft{false};                      // T+G: tabella a sinistra
};
// Le due meta' sono larghe (Columns - 1) / 2 pixel con una linea di separazione.
Panes layout(Layout l, const ViewWindow& mainView, const ViewWindow& subView);

// ---- sorgenti di punti
class PointSource {
public:
    enum class Kind : std::uint8_t { Function, Parametric, Points };
    virtual ~PointSource() = default;
    virtual Kind kind() const noexcept = 0;
    // Function: v = x, punto (x, f(x)); Parametric: v = t; Points: v = indice.
    virtual bool eval(double v, double& x, double& y) const = 0;
    // Dominio del parametro (Parametric) o numero di punti (Points).
    virtual double start() const noexcept { return 0; }
    virtual double end() const noexcept { return 0; }
    virtual double step() const noexcept { return 1; }
    virtual std::size_t count() const noexcept { return 0; }
    virtual bool connected() const noexcept { return true; }   // Points: polilinea (true) o punti isolati
};
// y = f(x) con il valutatore unico (la variabile x legata nell'EvalContext).
class FunctionSource final : public PointSource {
public:
    FunctionSource(const std::string& expression, EvalContext context);
    Kind kind() const noexcept override { return Kind::Function; }
    bool eval(double x, double& ox, double& oy) const override;
private:
    ast::NodePtr body_;
    EvalContext ctx_;
};
// Curva parametrica da una funzione C++ (coniche, circonferenze di Geometry...).
class ParametricSource final : public PointSource {
public:
    using Fn = std::function<bool(double t, double& x, double& y)>;
    ParametricSource(Fn fn, double t0, double t1, double dt) : fn_(std::move(fn)), t0_(t0), t1_(t1), dt_(dt) {}
    Kind kind() const noexcept override { return Kind::Parametric; }
    bool eval(double t, double& x, double& y) const override { return fn_(t, x, y); }
    double start() const noexcept override { return t0_; }
    double end() const noexcept override { return t1_; }
    double step() const noexcept override { return dt_; }
private:
    Fn fn_;
    double t0_, t1_, dt_;
};
// Punti (successioni, liste, segmenti): coordinate in due vettori.
class PointsSource final : public PointSource {
public:
    PointsSource(std::vector<double> xs, std::vector<double> ys, bool connected) : xs_(std::move(xs)), ys_(std::move(ys)), connected_(connected) {}
    Kind kind() const noexcept override { return Kind::Points; }
    bool eval(double i, double& x, double& y) const override;
    std::size_t count() const noexcept override { return xs_.size() < ys_.size() ? xs_.size() : ys_.size(); }
    bool connected() const noexcept override { return connected_; }
private:
    std::vector<double> xs_, ys_;
    bool connected_;
};

// ---- campionamento a passi in punti di schermo (graph::PenUp spezza la linea)
class Plotter final : public step::Task {
public:
    Plotter(const PointSource& source, const graph::Viewport& viewport);
    bool step(step::Budget& budget) override;    // un'unita' = una valutazione
    std::size_t total() const override { return total_; }
    std::size_t done() const override { return next_; }
    bool finished() const noexcept { return next_ >= total_; }
    const std::vector<std::int16_t>& points() const noexcept { return xy_; }
    // Disegna i segmenti dal punto `from` (numero di coordinate x,y gia' disegnate) a oggi.
    std::size_t draw(gfx::Canvas& c, std::uint16_t color, std::size_t from = 0) const;
    void runAll();                                // senza budget (ridisegno completo)
private:
    double parameter(std::size_t k) const noexcept;
    const PointSource& src_;
    graph::Viewport vp_;
    std::size_t total_{0}, next_{0};
    std::vector<std::int16_t> xy_;
    bool havePrev_{false};
    double prevSy_{0};
};

// ---- assi e griglia di un riquadro (per le modalita' nuove)
void drawAxes(gfx::Canvas& c, const graph::Viewport& vp, std::uint16_t axisColor, std::uint16_t gridColor, bool grid);

// ---- TRACE comune: il cursore si sposta di un pixel (funzioni), di un passo
// del parametro (parametriche) o di un punto (punti).
class TraceCursor {
public:
    TraceCursor(const PointSource& source, const graph::Viewport& viewport);
    void left() noexcept;
    void right() noexcept;
    void jump(double v) noexcept { v_ = v; }
    double parameter() const noexcept { return v_; }
    bool point(double& x, double& y) const;      // false: fuori dominio
private:
    const PointSource& src_;
    graph::Viewport vp_;
    double v_{0};
};

// ---- ZOOM comuni (manuale 5-15): fattori e riquadro sulla V-Window condivisa.
ViewWindow zoomIn(const ViewWindow& v, double cx, double cy, double xFactor, double yFactor);
ViewWindow zoomOut(const ViewWindow& v, double cx, double cy, double xFactor, double yFactor);
ViewWindow zoomBox(const ViewWindow& v, double x0, double y0, double x1, double y1);

} // namespace fx::graph2d
