// Grafico 2D condiviso con schermo diviso (v1.15.0, F7): vedi graph2d.hpp.
#include "graph2d.hpp"
#include <algorithm>
#include <cmath>

namespace fx::graph2d {

Panes layout(Layout l, const ViewWindow& mainView, const ViewWindow& subView) {
    Panes p;
    const int top = graphgeo::Top, rows = graphgeo::Rows;
    if (l == Layout::Full) {
        p.main = graph::Viewport(mainView, graphgeo::Left, top, graphgeo::Columns, rows);
        p.mainRect = {graphgeo::Left, top, graphgeo::Columns, rows};
        return p;
    }
    const int half = (graphgeo::Columns - 1) / 2;                  // 159 + 1 di separazione + 160
    const int rightX = graphgeo::Left + half + 1, rightW = graphgeo::Columns - half - 1;
    p.split = true;
    p.subIsTable = l != Layout::GraphGraph;
    p.tableLeft = l == Layout::TableGraph;
    const gfx::Rect left{graphgeo::Left, top, half, rows}, right{rightX, top, rightW, rows};
    p.mainRect = p.tableLeft ? right : left;
    p.subRect = p.tableLeft ? left : right;
    p.main = graph::Viewport(mainView, p.mainRect.x, top, p.mainRect.w, rows);
    p.sub = graph::Viewport(subView, p.subRect.x, top, p.subRect.w, rows);
    return p;
}

FunctionSource::FunctionSource(const std::string& expression, EvalContext context) : body_(parseExpression(expression)), ctx_(context) {}
bool FunctionSource::eval(double x, double& ox, double& oy) const {
    try {
        EvalContext c = ctx_;
        c.bind("x", x);
        oy = evaluateReal(*body_, c);
        ox = x;
        return std::isfinite(oy);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (...) {
        return false;                                               // non definita: un buco nel grafico
    }
}
bool PointsSource::eval(double i, double& x, double& y) const {
    if (!(i >= 0)) return false;
    const auto k = static_cast<std::size_t>(i);
    if (k >= count()) return false;
    x = xs_[k]; y = ys_[k];
    return std::isfinite(x) && std::isfinite(y);
}

Plotter::Plotter(const PointSource& source, const graph::Viewport& viewport) : src_(source), vp_(viewport) {
    switch (src_.kind()) {
    case PointSource::Kind::Function: total_ = static_cast<std::size_t>(vp_.columns()) + 1; break;
    case PointSource::Kind::Parametric: {
        const double span = src_.end() - src_.start(), dt = src_.step();
        total_ = dt > 0 && span >= 0 ? static_cast<std::size_t>(std::floor(span / dt + 1e-9)) + 1 : 0;
        total_ = std::min<std::size_t>(total_, graph::MaxParamSteps + 1);
        break;
    }
    case PointSource::Kind::Points: total_ = src_.count(); break;
    }
}
double Plotter::parameter(std::size_t k) const noexcept {
    switch (src_.kind()) {
    case PointSource::Kind::Function: return vp_.xAt(static_cast<double>(k));
    case PointSource::Kind::Parametric: return k + 1 == total_ ? src_.end() : src_.start() + static_cast<double>(k) * src_.step();
    case PointSource::Kind::Points: return static_cast<double>(k);
    }
    return 0;
}
bool Plotter::step(step::Budget& budget) {
    const double limit = 4.0 * std::max(vp_.rows(), vp_.columns());
    while (next_ < total_) {
        double x = 0, y = 0;
        const bool ok = src_.eval(parameter(next_), x, y);
        ++next_;
        if (ok) {
            const double sx = vp_.screenX(x), sy = vp_.screenY(y);
            const bool inRange = std::abs(sx - vp_.left()) < limit && std::abs(sy - vp_.top()) < limit;
            // salto verticale enorme tra due colonne (asintoto): la linea si spezza
            const bool jump = src_.kind() == PointSource::Kind::Function && havePrev_ && std::abs(sy - prevSy_) > vp_.rows() * 2.0;
            const bool penUp = !src_.connected() || !havePrev_ || jump || !inRange;
            if (inRange) {
                if (penUp) { xy_.push_back(graph::PenUp); xy_.push_back(0); }
                xy_.push_back(static_cast<std::int16_t>(std::lround(sx)));
                xy_.push_back(static_cast<std::int16_t>(std::lround(sy)));
            }
            havePrev_ = inRange;
            prevSy_ = sy;
        } else {
            havePrev_ = false;
        }
        if (!budget.consume()) break;
    }
    return next_ >= total_;
}
void Plotter::runAll() {
    step::Budget unlimited;
    while (!step(unlimited)) {}
}
std::size_t Plotter::draw(gfx::Canvas& c, std::uint16_t color, std::size_t from) const {
    // Un segmento collega (x,y) al punto precedente se questo non e' un PenUp:
    // disegnare a pezzi produce gli stessi segmenti di un disegno completo.
    const gfx::Rect old = c.clip();
    c.setClip(gfx::Rect{vp_.left(), vp_.top(), vp_.columns(), vp_.rows()});
    const std::size_t n = xy_.size() / 2;
    for (std::size_t i = from; i < n; ++i) {
        const std::int16_t x = xy_[2 * i], y = xy_[2 * i + 1];
        if (x == graph::PenUp) continue;
        const bool startsPiece = i == 0 || xy_[2 * (i - 1)] == graph::PenUp;
        if (startsPiece) {
            const bool isolated = i + 1 >= n || xy_[2 * (i + 1)] == graph::PenUp;
            if (isolated) c.pixel(x, y, color);
            continue;
        }
        c.line(xy_[2 * (i - 1)], xy_[2 * (i - 1) + 1], x, y, color);
    }
    c.setClip(old);
    return n;
}

void drawAxes(gfx::Canvas& c, const graph::Viewport& vp, std::uint16_t axisColor, std::uint16_t gridColor, bool grid) {
    const gfx::Rect old = c.clip();
    c.setClip(gfx::Rect{vp.left(), vp.top(), vp.columns(), vp.rows()});
    const ViewWindow& v = vp.view();
    if (grid && v.xscl > 0 && v.yscl > 0) {
        double first = 0;
        int count = 0;
        if (graph::tickRange(v.xmin, v.xmax, v.xscl, 200, first, count))
            for (int i = 0; i < count; ++i) c.vLine(static_cast<int>(std::lround(vp.screenX(first + i * v.xscl))), vp.top(), vp.rows(), gridColor);
        if (graph::tickRange(v.ymin, v.ymax, v.yscl, 200, first, count))
            for (int i = 0; i < count; ++i) c.hLine(vp.left(), static_cast<int>(std::lround(vp.screenY(first + i * v.yscl))), vp.columns(), gridColor);
    }
    if (v.ymin <= 0 && v.ymax >= 0) c.hLine(vp.left(), static_cast<int>(std::lround(vp.screenY(0))), vp.columns(), axisColor);
    if (v.xmin <= 0 && v.xmax >= 0) c.vLine(static_cast<int>(std::lround(vp.screenX(0))), vp.top(), vp.rows(), axisColor);
    c.setClip(old);
}

TraceCursor::TraceCursor(const PointSource& source, const graph::Viewport& viewport) : src_(source), vp_(viewport) {
    switch (src_.kind()) {
    case PointSource::Kind::Function: v_ = vp_.xAt(vp_.columns() / 2.0); break;
    case PointSource::Kind::Parametric: v_ = src_.start(); break;
    case PointSource::Kind::Points: v_ = 0; break;
    }
}
void TraceCursor::left() noexcept {
    switch (src_.kind()) {
    case PointSource::Kind::Function: v_ = std::max(vp_.view().xmin, v_ - vp_.xdot()); break;
    case PointSource::Kind::Parametric: v_ = std::max(src_.start(), v_ - src_.step()); break;
    case PointSource::Kind::Points: v_ = std::max(0.0, v_ - 1); break;
    }
}
void TraceCursor::right() noexcept {
    switch (src_.kind()) {
    case PointSource::Kind::Function: v_ = std::min(vp_.view().xmax, v_ + vp_.xdot()); break;
    case PointSource::Kind::Parametric: v_ = std::min(src_.end(), v_ + src_.step()); break;
    case PointSource::Kind::Points: {
        const double last = src_.count() ? static_cast<double>(src_.count() - 1) : 0.0;
        v_ = std::min(last, v_ + 1);
        break;
    }
    }
}
bool TraceCursor::point(double& x, double& y) const { return src_.eval(v_, x, y); }

ViewWindow zoomIn(const ViewWindow& v, double cx, double cy, double xFactor, double yFactor) {
    return graph::zoomAt(v, cx, cy, xFactor, yFactor);                // fattore > 1: la finestra si restringe
}
ViewWindow zoomOut(const ViewWindow& v, double cx, double cy, double xFactor, double yFactor) {
    return graph::zoomAt(v, cx, cy, 1.0 / xFactor, 1.0 / yFactor);
}
ViewWindow zoomBox(const ViewWindow& v, double x0, double y0, double x1, double y1) { return graph::zoomBox(v, x0, y0, x1, y1); }

} // namespace fx::graph2d
