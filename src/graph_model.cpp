#include "graph_model.hpp"
#include <cmath>

namespace fx {
namespace {
struct TypeName { GraphType type; const char* label; const char* name; };
// label: UTF-8 shown on screen; name: ASCII used in files.
constexpr TypeName kTypes[GraphTypeCount] = {
    {GraphType::Y, "Y=", "Y="}, {GraphType::Polar, "r=", "r="},
    {GraphType::YGreater, "Y>", "Y>"}, {GraphType::YLess, "Y<", "Y<"},
    {GraphType::YGreaterEq, "Y\xE2\x89\xA5", "Y>="}, {GraphType::YLessEq, "Y\xE2\x89\xA4", "Y<="},
    {GraphType::X, "X=", "X="}, {GraphType::Param, "Param", "Param"},
    {GraphType::XGreater, "X>", "X>"}, {GraphType::XLess, "X<", "X<"},
    {GraphType::XGreaterEq, "X\xE2\x89\xA5", "X>="}, {GraphType::XLessEq, "X\xE2\x89\xA4", "X<="},
};
constexpr const char* kStyles[LineStyleCount] = {"Normal", "Thick", "Broken", "Dot", "Thin"};
constexpr const char* kColors[GraphColorCount] = {"Blue", "Red", "Green", "Magenta", "Black", "Cyan", "Yellow"};
constexpr const char* kSketchKinds[] = {"Plot", "PlotOff", "Line", "Tangent", "Normal", "Circle", "Vertical", "Horizontal", "Text", "Pen", "Inverse"};
constexpr std::size_t SketchKindCount = sizeof(kSketchKinds) / sizeof(kSketchKinds[0]);
bool finite(double v) { return std::isfinite(v) && std::abs(v) <= MaxViewMagnitude; }
} // namespace

const char* graphTypeLabel(GraphType t) noexcept {
    for (const auto& k : kTypes) if (k.type == t) return k.label;
    return "Y=";
}
const char* graphTypeName(GraphType t) noexcept {
    for (const auto& k : kTypes) if (k.type == t) return k.name;
    return "Y=";
}
bool parseGraphTypeName(const std::string& name, GraphType& out) noexcept {
    for (const auto& k : kTypes) if (name == k.name) { out = k.type; return true; }
    return false;
}
const char* graphVariable(GraphType t) noexcept {
    if (t == GraphType::Polar) return "theta";
    if (t == GraphType::Param) return "t";
    if (isXType(t)) return "y";
    return "x";
}
const char* lineStyleName(LineStyle s) noexcept {
    const auto i = static_cast<std::size_t>(s);
    return i < LineStyleCount ? kStyles[i] : kStyles[0];
}
bool parseLineStyleName(const std::string& name, LineStyle& out) noexcept {
    for (std::size_t i = 0; i < LineStyleCount; ++i) if (name == kStyles[i]) { out = static_cast<LineStyle>(i); return true; }
    return false;
}
const char* graphColorName(std::uint8_t c) noexcept { return c < GraphColorCount ? kColors[c] : kColors[0]; }
bool parseGraphColorName(const std::string& name, std::uint8_t& out) noexcept {
    for (std::uint8_t i = 0; i < GraphColorCount; ++i) if (name == kColors[i]) { out = i; return true; }
    return false;
}
GraphFunction emptyGraphFunction(std::size_t index) {
    GraphFunction f;
    f.color = defaultGraphColor(index);
    return f;
}

double viewXdot(const ViewWindow& v) noexcept { return (v.xmax - v.xmin) / (graphgeo::Columns - 1); }
double viewYdot(const ViewWindow& v) noexcept { return (v.ymax - v.ymin) / (graphgeo::Rows - 1); }

std::string validateView(const ViewWindow& v) {
    for (double d : {v.xmin, v.xmax, v.xscl, v.ymin, v.ymax, v.yscl, v.tmin, v.tmax, v.tstep})
        if (!finite(d)) return "Argument ERROR: valore fuori intervallo (|v| < 1E+98)";
    if (!(v.xmin < v.xmax)) return "Argument ERROR: Xmin deve essere minore di Xmax";
    if (!(v.ymin < v.ymax)) return "Argument ERROR: Ymin deve essere minore di Ymax";
    // Pixel coordinates must stay representable after the transform.
    if (!(viewXdot(v) > 0) || !(viewYdot(v) > 0) || !std::isfinite(viewXdot(v)) || !std::isfinite(viewYdot(v)))
        return "Argument ERROR: finestra troppo piccola";
    if (v.xscl < 0 || v.yscl < 0) return "Argument ERROR: Xscale e Yscale non negativi";
    if (v.tstep == 0) return "Argument ERROR: T\xCE\xB8ptch non puo' essere 0";
    if (v.tmin == v.tmax) return "Argument ERROR: T\xCE\xB8min = T\xCE\xB8max";
    if ((v.tmax - v.tmin) / v.tstep < 0) return "Argument ERROR: segno di T\xCE\xB8ptch errato";
    return {};
}

const char* sketchKindName(SketchItem::Kind k) noexcept {
    const auto i = static_cast<std::size_t>(k);
    return i < SketchKindCount ? kSketchKinds[i] : "?";
}
bool parseSketchKindName(const std::string& name, SketchItem::Kind& out) noexcept {
    for (std::size_t i = 0; i < SketchKindCount; ++i) if (name == kSketchKinds[i]) { out = static_cast<SketchItem::Kind>(i); return true; }
    return false;
}
std::string validateSketch(const SketchItem& s) {
    if (static_cast<std::size_t>(s.kind) >= SketchKindCount) return "tipo di elemento sconosciuto";
    for (double d : {s.x0, s.y0, s.x1, s.y1}) if (!finite(d)) return "coordinate non valide";
    if (s.color >= GraphColorCount) return "colore non valido";
    if (static_cast<std::uint8_t>(s.style) >= LineStyleCount) return "stile non valido";
    if (s.kind == SketchItem::Kind::Circle && !(s.x1 > 0)) return "raggio non valido";
    if ((s.kind == SketchItem::Kind::Tangent || s.kind == SketchItem::Kind::Normal) && s.x1 == 0 && s.y1 == 0) return "direzione nulla";
    if (s.kind == SketchItem::Kind::Text && (s.text.empty() || s.text.size() > MaxSketchText)) return "testo vuoto o troppo lungo";
    if (s.kind == SketchItem::Kind::Inverse && (s.text.empty() || s.text.size() > MaxGraphExpression)) return "funzione mancante";
    if (s.kind == SketchItem::Kind::Pen) {
        if (s.path.size() < 2 || s.path.size() % 2 || s.path.size() > 2 * MaxPenPoints) return "tracciato non valido";
        for (float f : s.path) if (!std::isfinite(f)) return "tracciato non valido";
    } else if (!s.path.empty()) return "tracciato inatteso";
    return {};
}
std::size_t graphMemoryBytes(const GraphState& g) noexcept {
    std::size_t n = 0;
    for (const auto& m : g.gmem) n += m.size();
    return n;
}
} // namespace fx
