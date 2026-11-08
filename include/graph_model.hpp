#pragma once
// Data model of the GRAPH mode (fx-CG50 chapter 5): relation list of 20
// functions with type, colour and line style, View Window, the Setup items of
// the graph screen, V-Window and graph memories and the SKETCH elements.
// Pure data with validation helpers: no UI and no evaluation here (see
// graph_engine.hpp). The workspace owns one instance; .pcgraph projects
// (graph_file.hpp) store the same structures.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx {

// Plot area of the GRAPH screen on the 320x320 panel: below the 18 px status
// bar and above the 20 px function-key bar. Column c shows x = Xmin + c*Xdot,
// row r shows y = Ymax - r*Ydot (Xmin on the first column, Xmax on the last).
namespace graphgeo {
constexpr int Left = 0, Top = 18, Columns = 320, Rows = 282;
constexpr int Right = Left + Columns - 1, Bottom = Top + Rows - 1;
} // namespace graphgeo

// Values 0..5 keep the encoding of the v1.6/v1.7 records.
enum class GraphType : std::uint8_t {
    Y = 0, Polar = 1, YGreater = 2, YLess = 3, YGreaterEq = 4, YLessEq = 5,
    X = 6, Param = 7, XGreater = 8, XLess = 9, XGreaterEq = 10, XLessEq = 11,
};
constexpr std::uint8_t GraphTypeCount = 12;
const char* graphTypeLabel(GraphType t) noexcept;    // "Y=", "r=", "Param", "X=", "Y>", "Y≥", ...
const char* graphTypeName(GraphType t) noexcept;     // ASCII key used by the file formats: "Y=", "r=", "Param", "X=", "Y>", "Y>=", ...
bool parseGraphTypeName(const std::string& name, GraphType& out) noexcept;
// Y=f(x) and the Y inequalities: functions of x drawn column by column.
constexpr bool isYType(GraphType t) noexcept { return t == GraphType::Y || (t >= GraphType::YGreater && t <= GraphType::YLessEq); }
// X=f(y) and the X inequalities: functions of y drawn row by row.
constexpr bool isXType(GraphType t) noexcept { return t == GraphType::X || (t >= GraphType::XGreater && t <= GraphType::XLessEq); }
constexpr bool isInequality(GraphType t) noexcept { return (t >= GraphType::YGreater && t <= GraphType::YLessEq) || (t >= GraphType::XGreater && t <= GraphType::XLessEq); }
constexpr bool isStrictInequality(GraphType t) noexcept { return t == GraphType::YGreater || t == GraphType::YLess || t == GraphType::XGreater || t == GraphType::XLess; }
// Y>, Y>=, X>, X>=: the region lies above (right of) the boundary.
constexpr bool isGreaterInequality(GraphType t) noexcept { return t == GraphType::YGreater || t == GraphType::YGreaterEq || t == GraphType::XGreater || t == GraphType::XGreaterEq; }
// CONVERT is allowed between rectangular expressions and inequalities only (manual 5-17).
constexpr bool isConvertible(GraphType t) noexcept { return isYType(t) || isXType(t); }
// Name of the variable the function body uses: "x", "y" (X=f(y)), "theta" (r=), "t" (Param).
const char* graphVariable(GraphType t) noexcept;

enum class LineStyle : std::uint8_t { Normal = 0, Thick = 1, Broken = 2, Dot = 3, Thin = 4 };
constexpr std::uint8_t LineStyleCount = 5;
const char* lineStyleName(LineStyle s) noexcept;     // "Normal", "Thick", "Broken", "Dot", "Thin"
bool parseLineStyleName(const std::string& name, LineStyle& out) noexcept;

// fx-CG50 colours. Indices 0..5 keep the encoding of the v1.6/v1.7 records.
enum class GraphColor : std::uint8_t { Blue = 0, Red = 1, Green = 2, Magenta = 3, Black = 4, Cyan = 5, Yellow = 6 };
constexpr std::uint8_t GraphColorCount = 7;
const char* graphColorName(std::uint8_t color) noexcept;   // "Blue", "Red", ...
bool parseGraphColorName(const std::string& name, std::uint8_t& out) noexcept;
// Default colour of relation-list line i: blue, red, green, magenta, black, repeated.
constexpr std::uint8_t defaultGraphColor(std::size_t index) noexcept { return static_cast<std::uint8_t>(index % 5); }

constexpr std::size_t MaxGraphFunctions = 20;
constexpr std::size_t MaxGraphExpression = 1000;   // bytes of one stored expression

struct GraphFunction {
    // Y=, r=, X= and inequalities: f(x) / f(theta) / f(y), optionally followed by
    // ",[a,b]" (graph range) or ",[A=v1,v2,...]" (overwrite). Param: Xt(T).
    std::string expr;
    GraphType type{GraphType::Y};
    bool selected{true};                 // draw / non-draw ("=" highlighted)
    std::uint8_t color{0};               // GraphColor
    LineStyle style{LineStyle::Normal};
    std::string expr2;                   // Param: Yt(T)
    bool defined() const noexcept { return type == GraphType::Param ? !expr.empty() && !expr2.empty() : !expr.empty(); }
    bool empty() const noexcept { return expr.empty() && expr2.empty(); }
};
GraphFunction emptyGraphFunction(std::size_t index);

// View Window. Xdot/Ydot are derived from the plot size (graphgeo).
struct ViewWindow {
    double xmin{-6.3}, xmax{6.3}, xscl{1}, ymin{-3.1}, ymax{3.1}, yscl{1};
    double tmin{0}, tmax{6.283185307179586}, tstep{0.05235987755982988};
    bool operator==(const ViewWindow&) const = default;
};
double viewXdot(const ViewWindow& v) noexcept;       // (Xmax-Xmin)/(Columns-1)
double viewYdot(const ViewWindow& v) noexcept;
constexpr double MaxViewMagnitude = 9.999999999e97;  // input range of the V-Window parameters (manual 5-6)
// Empty when the window can be drawn, otherwise an fx-CG50 style error text.
// Tθmax < Tθmin is allowed: Tθptch then has to be negative (manual 5-6).
std::string validateView(const ViewWindow& v);

enum class GridMode : std::uint8_t { Off = 0, On = 1, Line = 2 };
enum class AxesMode : std::uint8_t { On = 0, Off = 1, Scale = 2 };

// Setup items of the Graph mode (manual 1-35..1-38) plus the zoom factors.
struct GraphSettings {
    GraphType funcType{GraphType::Y};    // Func Type: type given to new entries
    bool plotDraw{false};                // Draw Type: Connect (false) / Plot
    bool derivative{false};              // Derivative: dY/dX in Trace, roots and Table
    bool coord{true};                    // Coord: pointer coordinates
    GridMode grid{GridMode::Off};
    AxesMode axes{AxesMode::On};
    bool label{false};                   // axis labels x, y, O
    bool graphFunc{true};                // Graph Func: show the function while drawing/tracing
    bool simulGraph{false};              // Simul Graph: draw all graphs at the same time
    bool ineqUnion{true};                // Ineq Type: Union (true) / Intsect
    LineStyle sketchLine{LineStyle::Normal};
    std::uint8_t plotColor{0};           // Plot/LineCol (GraphColor) for sketches
    double xFactor{2}, yFactor{2};       // ZOOM FACTOR
    std::uint8_t background{0};          // 0 None, 1..20 Pict n, 21 = backgroundFile
    std::string backgroundFile;          // picture used when background == 21
    std::uint8_t fade{0};                // background lightness 0..100 %
};
constexpr std::uint8_t BackgroundFile = 21;

// SKETCH element in graph coordinates, so it is redrawn after zoom and pan and
// can be saved in a project.
struct SketchItem {
    enum class Kind : std::uint8_t {
        Point = 0,       // Plot / PlotOn: (x0,y0)
        PointOff = 1,    // PlotOff: erased dot at (x0,y0)
        Segment = 2,     // Line / F-Line: (x0,y0)-(x1,y1)
        Tangent = 3,     // infinite line through (x0,y0) with direction (x1,y1)
        Normal = 4,      // idem, normal line
        Circle = 5,      // centre (x0,y0), radius x1
        Vertical = 6,    // x = x0
        Horizontal = 7,  // y = y0
        Text = 8,        // text at (x0,y0)
        Pen = 9,         // freehand: path = x,y pairs
        Inverse = 10,    // inverse of y=f(x): text holds f, drawn as x=f(y)
    };
    Kind kind{Kind::Point};
    double x0{}, y0{}, x1{}, y1{};
    std::uint8_t color{0};
    LineStyle style{LineStyle::Normal};
    std::string text;
    std::vector<float> path;
};
constexpr std::size_t MaxSketchItems = 64;
constexpr std::size_t MaxPenPoints = 400;            // x,y pairs per freehand stroke
constexpr std::size_t MaxSketchText = 32;
const char* sketchKindName(SketchItem::Kind k) noexcept;   // "Plot", "PlotOff", "Line", ...
bool parseSketchKindName(const std::string& name, SketchItem::Kind& out) noexcept;
std::string validateSketch(const SketchItem& s);

constexpr std::size_t VWindowMemories = 6;           // V-Win1..V-Win6
constexpr std::size_t GraphMemories = 20;            // G-Mem1..G-Mem20
constexpr std::size_t MaxGraphMemoryBytes = 24 * 1024;   // all graph memories together

// Everything the GRAPH mode keeps besides the functions, the V-Window and the
// global angle unit (owned by Settings).
struct GraphState {
    GraphSettings settings;
    std::array<ViewWindow, VWindowMemories> vmem{};
    std::uint8_t vmemUsed{};                         // bit i: V-Win(i+1) stored
    std::array<std::string, GraphMemories> gmem{};   // encoded graph memories (graph_file.hpp), empty = free
    std::vector<SketchItem> sketch;
    std::string overlay;                             // picture recalled behind the graphs ("" = none)
    std::string project;                             // name of the open .pcgraph project ("" = not saved yet)
    bool projectModified{};
};
std::size_t graphMemoryBytes(const GraphState& g) noexcept;
} // namespace fx
