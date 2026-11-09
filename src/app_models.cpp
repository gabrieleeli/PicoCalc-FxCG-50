#include "app_models.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace fx::models {
namespace {
bool finite(double v) { return std::isfinite(v); }
} // namespace

std::string validate(const RecursionModel& m) {
    if (m.start < 0 || m.end > RecursionModel::MaxN || m.start > m.end) return "Argument ERROR: n tra 0 e 999, Start <= End";
    if (m.fromOne && m.start < 1) return "Argument ERROR: con a1 come termine iniziale Start >= 1";
    bool any = false;
    for (std::size_t i = 0; i < m.formula.size(); ++i) {
        if (!m.selected[i]) continue;
        if (m.formula[i].empty()) return "Argument ERROR: successione selezionata senza formula";
        any = true;
    }
    if (!any) return "Argument ERROR: nessuna successione selezionata";
    for (const auto& pair : m.initial) for (double v : pair) if (!finite(v)) return "Argument ERROR: termine iniziale non finito";
    return {};
}

const char* dynaBuiltinFunction(DynaBuiltin b) noexcept {
    switch (b) {
    case DynaBuiltin::Linear: return "A*x+B";
    case DynaBuiltin::Vertex: return "A(x-B)^2+C";
    case DynaBuiltin::Quadratic: return "A*x^2+B*x+C";
    case DynaBuiltin::Cubic: return "A*x^3+B*x^2+C*x+D";
    case DynaBuiltin::Sine: return "A*sin(B*x+C)";
    case DynaBuiltin::Cosine: return "A*cos(B*x+C)";
    case DynaBuiltin::Tangent: return "A*tan(B*x+C)";
    case DynaBuiltin::Custom: return "";
    }
    return "";
}
std::size_t frameCount(const DynaGraphModel& m) noexcept {
    if (!(m.pitch > 0) || !finite(m.start) || !finite(m.end)) return 0;
    const double span = std::abs(m.end - m.start) / m.pitch;
    if (span > 1e6) return static_cast<std::size_t>(1e6);
    return static_cast<std::size_t>(std::floor(span + 1e-9)) + 1;
}
std::string validate(const DynaGraphModel& m) {
    if (m.function.empty()) return "Argument ERROR: funzione dinamica vuota";
    if (m.variable < 'A' || m.variable > 'Z') return "Argument ERROR: coefficiente dinamico A-Z";
    if (!(m.pitch > 0) || !finite(m.start) || !finite(m.end) || m.start == m.end) return "Argument ERROR: Start != End e Pitch > 0";
    if (frameCount(m) > DynaGraphModel::MaxFrames) return "Argument ERROR: al massimo 100 grafici";
    return {};
}

ConicKind classify(const std::array<double, 6>& k) noexcept {
    const double a = k[0], b = k[1], c = k[2], d = k[3], e = k[4], f = k[5];
    for (double v : k) if (!finite(v)) return ConicKind::None;
    if (a == 0 && b == 0 && c == 0) return ConicKind::None;                 // a line, not a conic
    // Determinant of the symmetric matrix of the quadratic form in homogeneous coordinates.
    const double det = a * (c * f - e * e / 4) - b / 2 * (b / 2 * f - e * d / 4) + d / 2 * (b / 2 * e / 2 - c * d / 2);
    const double scale = std::max({std::abs(a), std::abs(b), std::abs(c), std::abs(d), std::abs(e), std::abs(f), 1e-300});
    if (std::abs(det) <= 1e-12 * scale * scale * scale) return ConicKind::Degenerate;
    const double disc = b * b - 4 * a * c;
    if (std::abs(disc) <= 1e-12 * scale * scale) return ConicKind::Parabola;
    if (disc > 0) return ConicKind::Hyperbola;
    if ((a + c) * det > 0) return ConicKind::None;                           // imaginary ellipse
    return (a == c && b == 0) ? ConicKind::Circle : ConicKind::Ellipse;
}
const char* conicName(ConicKind k) noexcept {
    switch (k) {
    case ConicKind::None: return "nessuna conica reale";
    case ConicKind::Parabola: return "parabola";
    case ConicKind::Circle: return "circonferenza";
    case ConicKind::Ellipse: return "ellisse";
    case ConicKind::Hyperbola: return "iperbole";
    case ConicKind::Degenerate: return "conica degenere";
    }
    return "?";
}

std::string validate(const Graph3DModel& m) {
    if (!(m.xmin < m.xmax) || !(m.ymin < m.ymax) || !(m.zmin < m.zmax)) return "Argument ERROR: min < max su ogni asse";
    // Up to 128 cells per side; the 3D Graph mode lowers it to what the memory
    // allows (24 without PSRAM, see limits.hpp).
    if (m.grid < 5 || m.grid > 128) return "Argument ERROR: griglia 5..128";
    for (std::size_t i = 0; i < Graph3DModel::Functions; ++i) if (m.selected[i] && m.z[i].empty()) return "Argument ERROR: funzione z selezionata vuota";
    return {};
}

std::string validate(const FinancialModel& m) {
    for (double v : {m.n, m.interest, m.pv, m.pmt, m.fv, m.py, m.cy}) if (!finite(v)) return "Argument ERROR: valore non finito";
    if (!(m.py > 0) || !(m.cy > 0)) return "Argument ERROR: P/Y e C/Y devono essere positivi";
    if (m.n < 0) return "Argument ERROR: n >= 0";
    return {};
}

bool parseCellRef(const std::string& t, CellRef& out) noexcept {
    if (t.size() < 2 || t.size() > 4 || !std::isalpha(static_cast<unsigned char>(t[0]))) return false;
    unsigned row = 0;
    for (std::size_t i = 1; i < t.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(t[i]))) return false;
        row = row * 10 + static_cast<unsigned>(t[i] - '0');
    }
    if (t[1] == '0' || row < 1 || row > SpreadsheetModel::Rows) return false;
    out.column = static_cast<std::uint8_t>(std::toupper(static_cast<unsigned char>(t[0])) - 'A');
    out.row = static_cast<std::uint16_t>(row);
    return true;
}
std::string cellName(const CellRef& r) { return std::string(1, static_cast<char>('A' + r.column)) + std::to_string(r.row); }
bool SpreadsheetModel::set(const CellRef& ref, std::string content) {
    if (ref.column >= Columns || ref.row < 1 || ref.row > Rows) return false;
    auto key = [](const CellRef& c) { return static_cast<unsigned>(c.row) * Columns + c.column; };
    auto it = std::lower_bound(cells.begin(), cells.end(), key(ref), [&](const Cell& c, unsigned k) { return key(c.ref) < k; });
    const bool exists = it != cells.end() && key(it->ref) == key(ref);
    if (content.empty()) { if (exists) cells.erase(it); return true; }
    if (exists) it->content = std::move(content);
    else cells.insert(it, Cell{ref, std::move(content)});
    return true;
}
const std::string* SpreadsheetModel::get(const CellRef& ref) const {
    for (const auto& c : cells) if (c.ref.column == ref.column && c.ref.row == ref.row) return &c.content;
    return nullptr;
}

std::string validateProgramName(const std::string& n) {
    if (n.empty() || n.size() > 8) return "Argument ERROR: nome di 1..8 caratteri";
    for (unsigned char c : n)
        if (!((c >= 'A' && c <= 'Z') || std::isdigit(c) || c == ' ' || c == '.' || c == '_')) return "Argument ERROR: usare A-Z, 0-9, spazio, . e _";
    return {};
}
std::string validateScriptName(const std::string& n) {
    if (n.size() < 4 || n.compare(n.size() - 3, 3, ".py") != 0) return "Argument ERROR: il nome deve finire con .py";
    const std::string base = n.substr(0, n.size() - 3);
    if (base.empty() || base.size() > 8) return "Argument ERROR: nome di 1..8 caratteri";
    for (unsigned char c : base) if (!(std::isalnum(c) || c == '_')) return "Argument ERROR: usare lettere, cifre e _";
    return {};
}

std::string validate(const GeometryObject& o) {
    const std::size_t n = o.coordinates.size();
    for (double v : o.coordinates) if (!finite(v)) return "Argument ERROR: coordinate non finite";
    switch (o.kind) {
    case GeometryObject::Kind::Point: return n == 2 ? std::string() : "Argument ERROR: un punto ha 2 coordinate";
    case GeometryObject::Kind::Segment: case GeometryObject::Kind::Line: case GeometryObject::Kind::Ray:
        if (n != 4) return "Argument ERROR: servono due punti";
        if (o.coordinates[0] == o.coordinates[2] && o.coordinates[1] == o.coordinates[3]) return "Argument ERROR: punti coincidenti";
        return {};
    case GeometryObject::Kind::Circle: return n == 3 && o.coordinates[2] > 0 ? std::string() : "Argument ERROR: centro e raggio > 0";
    case GeometryObject::Kind::Polygon: return n >= 6 && n % 2 == 0 ? std::string() : "Argument ERROR: almeno tre vertici";
    }
    return "Argument ERROR: oggetto sconosciuto";
}
} // namespace fx::models
