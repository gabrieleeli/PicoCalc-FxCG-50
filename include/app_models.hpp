#pragma once
// Initial data models of the modes that are not available yet (status
// Planned in app_registry.hpp). They fix what each mode will store and the
// limits taken from the fx-CG50 manual, with validation and the few pure
// helpers the models need (conic classification, cell references, frame
// counts). A model becomes part of fx::Workspace (and of the versioned
// record) in the session that implements its mode, so no format change is
// made for data nobody edits: Graph3DModel is persisted since v1.9.0 (record
// version 5, 3D Graph mode); the others are not persisted yet.
// All expressions are linear text evaluated by the shared engine.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::models {

// Every validate() returns an empty string when the model is valid,
// otherwise an fx-CG50 style message ("Argument ERROR: ...").

// ---------------------------------------------------------------- Recursion (manual 5-45)
enum class RecursionType : std::uint8_t { General /* an = f(n) */, Linear2 /* an+1 = f(n, an) */, Linear3 /* an+2 = f(n, an, an+1) */ };
struct RecursionModel {
    static constexpr int MaxN = 999;
    RecursionType type{RecursionType::Linear2};
    std::array<std::string, 3> formula{};        // an / bn / cn; names n, an, an1, bn, bn1, cn, cn1
    std::array<bool, 3> selected{{true, false, false}};
    int start{1}, end{5};                        // table range of n
    bool fromOne{true};                          // initial terms a1, a2 (true) or a0, a1
    std::array<std::array<double, 2>, 3> initial{};
};
std::string validate(const RecursionModel& m);

// ---------------------------------------------------------------- Dyna Graph (manual 5-42)
enum class DynaBuiltin : std::uint8_t { Linear, Vertex, Quadratic, Cubic, Sine, Cosine, Tangent, Custom };
const char* dynaBuiltinFunction(DynaBuiltin b) noexcept;    // "A*x+B", "A(x-B)^2+C", ...
struct DynaGraphModel {
    static constexpr std::size_t MaxFrames = 100;
    std::string function{"A(x-B)^2+C"};
    char variable{'A'};                          // the coefficient that changes
    double start{1}, end{5}, pitch{1};
    bool stopAfterTen{true};                     // Setup "Dynamic Type": Stop / Cont
    bool locus{false};
    enum class Speed : std::uint8_t { StopAndGo, Slow, Normal, Fast } speed{Speed::Normal};
};
std::size_t frameCount(const DynaGraphModel& m) noexcept;   // graphs drawn for start..end by pitch
std::string validate(const DynaGraphModel& m);

// ---------------------------------------------------------------- Conic Graphs (manual 5-50)
enum class ConicKind : std::uint8_t { None, Parabola, Circle, Ellipse, Hyperbola, Degenerate };
enum class ConicInput : std::uint8_t { Rectangular, Polar, Parametric };
struct ConicModel {
    ConicInput input{ConicInput::Rectangular};
    // General form A x² + B xy + C y² + D x + E y + F = 0 of the selected pattern.
    std::array<double, 6> coefficients{{1, 0, 1, 0, 0, -1}};
};
// Type of the general conic from the discriminant B²-4AC and the 3x3 determinant.
ConicKind classify(const std::array<double, 6>& coefficients) noexcept;
const char* conicName(ConicKind k) noexcept;

// ---------------------------------------------------------------- 3D Graph (chapter 16)
struct Graph3DModel {
    static constexpr std::size_t Functions = 3;
    std::array<std::string, Functions> z{};      // z = f(x, y)
    std::array<bool, Functions> selected{{true, false, false}};
    double xmin{-3}, xmax{3}, ymin{-3}, ymax{3}, zmin{-3}, zmax{3};
    int grid{40};                                 // cells per axis (5..128; limited by the free memory)
};
std::string validate(const Graph3DModel& m);

// ---------------------------------------------------------------- Financial (chapter 7)
struct FinancialModel {
    double n{0}, interest{0}, pv{0}, pmt{0}, fv{0};   // TVM variables (I% annual)
    double py{12}, cy{12};                            // payments / compoundings per year
    bool paymentAtBegin{false};                       // Setup "Payment": BEGIN / END
    bool days365{true};                               // Setup "Date Mode"
};
std::string validate(const FinancialModel& m);

// ---------------------------------------------------------------- Spreadsheet (chapter 9)
struct CellRef { std::uint8_t column{}; std::uint16_t row{}; };   // column 0 = A, row 1..999
bool parseCellRef(const std::string& text, CellRef& out) noexcept; // "B12", "z999"
std::string cellName(const CellRef& ref);
struct SpreadsheetModel {
    static constexpr std::uint8_t Columns = 26;
    static constexpr std::uint16_t Rows = 999;
    struct Cell { CellRef ref; std::string content; };       // number or formula starting with '='
    std::vector<Cell> cells;                                  // sparse, sorted by (row, column)
    bool autoCalc{true};
    bool set(const CellRef& ref, std::string content);       // false for out-of-range refs
    const std::string* get(const CellRef& ref) const;
};

// ---------------------------------------------------------------- Program / Python / eActivity files
struct SourceFile {
    std::string name;
    std::string text;
};
// Program names: 1..8 characters A-Z, 0-9, space, '.', '_' (fx-CG50 file names).
std::string validateProgramName(const std::string& name);
// Python scripts: 1..8 characters + ".py".
std::string validateScriptName(const std::string& name);
struct EActivityStrip {
    enum class Kind : std::uint8_t { Text, Calculation };
    Kind kind{Kind::Text};
    std::string content;                          // text, or linear expression evaluated by Run-Matrix core
};
struct EActivityDocument { std::string name; std::vector<EActivityStrip> strips; };

// ---------------------------------------------------------------- Geometry / Picture Plot
struct GeometryObject {
    enum class Kind : std::uint8_t { Point, Segment, Line, Ray, Circle, Polygon };
    Kind kind{Kind::Point};
    std::vector<double> coordinates;              // point list (x0, y0, x1, y1, ...); circle: cx, cy, r
};
std::string validate(const GeometryObject& o);
struct PicturePlotModel {
    struct Point { double x{}, y{}; std::uint16_t frame{}; };
    std::string background;                       // image name
    std::vector<Point> points;                    // copied to lists for regressions
};
} // namespace fx::models
