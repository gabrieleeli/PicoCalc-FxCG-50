#pragma once
// GRAPH engine (no drawing, no UI):
//  - Viewport: graph <-> screen coordinate transform and segment clipping;
//  - function texts with the fx-CG50 suffixes ",[a,b]" (graph range) and
//    ",[A=v1,v2,...]" (overwrite);
//  - GraphEngine: compiles the relation list with the shared parser, expands
//    overwrite and List graphs into plots, evaluates every value with the shared
//    evaluator (also for Trace and G-Solve) and resolves the references Y1(..)
//    of composite functions;
//  - incremental sampling of a plot into screen polylines with detection of
//    poles, jumps and domain edges, and inequality boundaries;
//  - V-Window and ZOOM arithmetic.
// The renderer (src/apps/graph_plot.cpp) only draws what this module produces.
#include "expression.hpp"
#include "graph_model.hpp"
#include "math_engine.hpp"
#include "memory.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fx::graph {

// ---------------------------------------------------------------- viewport
class Viewport {
public:
    Viewport() : Viewport(ViewWindow{}) {}
    explicit Viewport(const ViewWindow& v, int left = graphgeo::Left, int top = graphgeo::Top,
                      int columns = graphgeo::Columns, int rows = graphgeo::Rows) noexcept;
    const ViewWindow& view() const noexcept { return view_; }
    int left() const noexcept { return left_; }
    int top() const noexcept { return top_; }
    int columns() const noexcept { return columns_; }
    int rows() const noexcept { return rows_; }
    double xdot() const noexcept { return xdot_; }
    double ydot() const noexcept { return ydot_; }
    double xAt(double column) const noexcept { return view_.xmin + column * xdot_; }   // column relative to left()
    double yAt(double row) const noexcept { return view_.ymax - row * ydot_; }         // row relative to top()
    double columnOf(double x) const noexcept { return (x - view_.xmin) / xdot_; }
    double rowOf(double y) const noexcept { return (view_.ymax - y) / ydot_; }
    double screenX(double x) const noexcept { return left_ + columnOf(x); }
    double screenY(double y) const noexcept { return top_ + rowOf(y); }
    double mathX(double sx) const noexcept { return xAt(sx - left_); }
    double mathY(double sy) const noexcept { return yAt(sy - top_); }
    bool inside(double sx, double sy, double margin = 0) const noexcept;
    // Liang-Barsky clipping of a screen-space segment against the plot
    // rectangle grown by margin. False when nothing is visible.
    bool clip(double& x0, double& y0, double& x1, double& y1, double margin = 3) const noexcept;
private:
    ViewWindow view_;
    int left_, top_, columns_, rows_;
    double xdot_, ydot_;
};

// ---------------------------------------------------------------- function texts
struct FunctionText {
    std::string body;
    bool hasRange{};
    std::string rangeLo, rangeHi;              // expressions of the graph range
    char overwrite{};                          // 'A'..'Z' of ",[A=...]", 0 = none
    std::vector<std::string> values;           // overwrite values (expressions)
};
// Splits a stored function at the top-level ",[" suffix. Throws SyntaxError.
FunctionText splitFunctionText(const std::string& text);
constexpr std::size_t MaxOverwriteValues = 10;

// ---------------------------------------------------------------- plots
constexpr std::size_t MaxPlots = 30;          // fx-CG50: 20 functions + pasted graphs, 30 in total
constexpr std::size_t MaxPasted = 10;
// Colour sequence of overwrite/List graphs after the first one (manual 5-28).
constexpr std::uint8_t kOverwriteColors[5] = {0, 1, 2, 3, 4};   // blue, red, green, magenta, black

struct Plot {
    int function{-1};                          // relation list index; -1 pasted graph or sketch inverse
    int variant{0}, variants{1};
    GraphType type{GraphType::Y};
    std::uint8_t color{0};
    LineStyle style{LineStyle::Normal};
    std::string label;                         // "Y1", "r2", "Xt3", "Y4"
    std::string detail;                        // "A=2" (overwrite), "List1[2]"
    std::string text;                          // expression without suffix (Graph Func)
    bool hasRange{};
    double lo{}, hi{};                         // graph range of the variable
    std::shared_ptr<const ast::Node> body, body2;   // Param: Xt and Yt
    const char* variable{"x"};
    char bindName[2]{};                        // overwrite variable (lower case), "" = none
    double bindValue{};
    int listIndex{-1};                         // List graph: element used for list1..list6
    bool pasted{};
};

// Copy of everything an evaluation needs (the engine never touches the workspace).
struct GraphSource {
    const std::array<GraphFunction, MaxGraphFunctions>* functions{};
    const CalculatorMemory* memory{};
    const NameResolver* names{};               // Xmin, TblStart... (the workspace)
    const std::array<std::vector<double>, 6>* lists{};
};

class GraphEngine final : public FunctionResolver {
public:
    GraphEngine();
    ~GraphEngine() override;
    GraphEngine(const GraphEngine&) = delete;
    GraphEngine& operator=(const GraphEngine&) = delete;
    // Compiles every defined function (for references) and builds the plots of
    // the selected ones plus the pasted expressions. Errors of single functions
    // are recorded and the function is skipped; build() itself only throws
    // std::bad_alloc.
    void build(const GraphSource& source, const std::vector<std::string>& pasted = {});
    const std::vector<Plot>& plots() const noexcept { return plots_; }
    const std::string& error(std::size_t function) const;      // "" = no error
    // Plot of an arbitrary expression (sketch Inverse, Modify preview); throws on syntax errors.
    Plot makePlot(const std::string& text, GraphType type, std::uint8_t color, LineStyle style) const;

    // Values from the shared evaluator; false where the function is undefined.
    bool eval(const Plot& p, double v, double& out) const noexcept;               // Y/X types
    bool point(const Plot& p, double t, double& x, double& y) const noexcept;    // r= (t = theta) and Param
    bool polarRadius(const Plot& p, double theta, double& r) const noexcept;
    // dY/dX (Y types, Param), dX/dY (X types), dr/dtheta (r=).
    bool derivative(const Plot& p, double v, double& out) const noexcept;
    // Same as eval() but throws the evaluator's error (for messages).
    double evalOrThrow(const Plot& p, double v) const;

    // FunctionResolver: "y3" -> body of Y3 (Y= or Y inequality) ...
    const ast::Node* function(const std::string& lowerName, const char*& variable) const override;

    // Single-letter memory variables of an expression other than its own
    // variable (Modify, manual 5-38), in alphabetical order.
    static std::string coefficients(const ast::Node& body, const char* variable);
    const ast::Node* body(std::size_t function) const;           // parsed body (Xt for Param)
private:
    struct Compiled { std::shared_ptr<const ast::Node> body, body2; FunctionText text; std::string error; };
    class Names;
    double evalBody(const Plot& p, const ast::Node& body, double v) const;
    GraphSource src_{};
    std::array<Compiled, MaxGraphFunctions> compiled_{};
    std::vector<Plot> plots_;
    std::unique_ptr<Names> names_;
};

// ---------------------------------------------------------------- sampling
constexpr std::int16_t PenUp = INT16_MIN;     // x value that starts a new polyline piece
constexpr std::size_t MaxCurvePoints = 6000;   // points kept per plot (24 KB)
constexpr std::size_t MaxParamSteps = 20000;   // T/theta steps of one polar/parametric graph

struct Curve {
    std::vector<std::int16_t> xy;              // screen points (x,y); x == PenUp breaks the line
    std::vector<float> boundary;               // inequality: screen y per column (Y types) / x per row (X types), NaN = none
    std::size_t next{0}, steps{0};
    bool done{false}, truncated{false};
    // previous sample (connection to the next one)
    double prevV{}, prevSx{}, prevSy{};
    bool prevOk{false}, havePrev{false};
    void reset() { *this = Curve{}; }
};
// Number of samples of a plot: columns (Y types), rows (X types) or T/theta steps.
std::size_t sampleCount(const Plot& p, const Viewport& vp);
// Parameter of sample k (x, y, theta or T).
double sampleParameter(const Plot& p, const Viewport& vp, std::size_t k);
// Samples the next part of a plot. budget = evaluations allowed in this call
// (decremented); returns true when the curve is complete. Plot draw type
// (plotOnly) stores isolated points instead of connected lines.
bool sampleStep(const GraphEngine& engine, const Plot& p, const Viewport& vp, bool plotOnly, Curve& curve, int& budget);

// ---------------------------------------------------------------- V-Window / ZOOM
ViewWindow initialWindow(AngleMode angle);                       // INITIAL
ViewWindow trigWindow(const ViewWindow& v, AngleMode angle);     // TRIG (T range kept)
ViewWindow standardWindow(const ViewWindow& v);                  // STANDRD: -10..10
ViewWindow withXdot(const ViewWindow& v, double xdot);           // Xdot edit: Xmax follows
// Factor zoom centred on (cx, cy): fx, fy > 1 enlarge (IN), < 1 reduce.
ViewWindow zoomAt(const ViewWindow& v, double cx, double cy, double fx, double fy);
// Box zoom; throws Argument ERROR when the box has no width or height (manual 5-9).
ViewWindow zoomBox(const ViewWindow& v, double x0, double y0, double x1, double y1);
ViewWindow squareWindow(const ViewWindow& v);                    // SQUARE / Y-BASE: Xdot = Ydot, x centre kept
ViewWindow squareXBase(const ViewWindow& v);                     // X-BASE: x kept, y changed
ViewWindow roundWindow(const ViewWindow& v, double cx, double cy);   // ROUND: dots of one significant digit
ViewWindow integerWindow(const ViewWindow& v, double cx, double cy); // INTEGER: Xdot = Ydot = 1
// AUTO: Y range from the finite values (outliers of poles ignored). False if none.
bool autoWindow(ViewWindow& v, std::vector<double> values);
// Tick positions of an axis (Xscale/Yscale), limited to maxTicks.
bool tickRange(double lo, double hi, double scale, int maxTicks, double& first, int& count);
} // namespace fx::graph
