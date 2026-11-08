#include "graph_engine.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace fx::graph {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double JumpPx = 4.0;        // pixel distance that triggers the continuity test
constexpr int JumpDepth = 12;         // bisections of the continuity test (1/4096 of a step)
constexpr int EdgeDepth = 12;         // bisections that locate the edge of the domain
constexpr double ScreenLimit = 1e9;   // screen coordinates are clamped to this magnitude
const double NaN = std::numeric_limits<double>::quiet_NaN();

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}
// Splits at top-level commas (not inside parentheses).
std::vector<std::string> splitTop(const std::string& s) {
    std::vector<std::string> out;
    int depth = 0; std::size_t start = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(') ++depth;
        else if (s[i] == ')') --depth;
        else if (s[i] == ',' && depth == 0) { out.push_back(trim(s.substr(start, i - start))); start = i + 1; }
    }
    out.push_back(trim(s.substr(start)));
    return out;
}
double clampScreen(double v) {
    if (std::isnan(v)) return v;
    return std::clamp(v, -ScreenLimit, ScreenLimit);
}
double toRadians(double t) {
    switch (angleMode()) {
    case AngleMode::Degrees: return t * kPi / 180.0;
    case AngleMode::Gradians: return t * kPi / 200.0;
    default: return t;
    }
}
bool finiteValue(double v) { return std::isfinite(v); }
void collectTokens(const ast::Node& n, std::string& letters, const char* variable) {
    if (n.kind == ast::Kind::Token && n.text.size() == 1 && std::isalpha(static_cast<unsigned char>(n.text[0]))) {
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(n.text[0])));
        const bool own = variable && std::string(variable).size() == 1 && variable[0] == c;
        // x, y, r, t are graph variables; e and i are constants.
        if (!own && c != 'x' && c != 'y' && c != 'r' && c != 't' && c != 'e' && c != 'i') {
            const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (letters.find(up) == std::string::npos) letters.push_back(up);
        }
    }
    for (const auto& c : n.children) if (c) collectTokens(*c, letters, variable);
}
void collectLists(const ast::Node& n, unsigned& mask) {
    if (n.kind == ast::Kind::Token && n.text.size() == 5 && n.text.compare(0, 4, "list") == 0 && n.text[4] >= '1' && n.text[4] <= '6')
        mask |= 1u << (n.text[4] - '1');
    for (const auto& c : n.children) if (c) collectLists(*c, mask);
}
std::string typePrefix(GraphType t) {
    if (t == GraphType::Polar) return "r";
    if (t == GraphType::Param) return "Xt";
    if (isXType(t)) return "X";
    return "Y";
}
} // namespace

// ---------------------------------------------------------------- viewport
Viewport::Viewport(const ViewWindow& v, int left, int top, int columns, int rows) noexcept
    : view_(v), left_(left), top_(top), columns_(columns), rows_(rows),
      xdot_((v.xmax - v.xmin) / std::max(1, columns - 1)), ydot_((v.ymax - v.ymin) / std::max(1, rows - 1)) {}

bool Viewport::inside(double sx, double sy, double margin) const noexcept {
    return sx >= left_ - margin && sx <= left_ + columns_ - 1 + margin && sy >= top_ - margin && sy <= top_ + rows_ - 1 + margin;
}
bool Viewport::clip(double& x0, double& y0, double& x1, double& y1, double margin) const noexcept {
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) || !std::isfinite(y1)) return false;
    const double xmin = left_ - margin, xmax = left_ + columns_ - 1 + margin, ymin = top_ - margin, ymax = top_ + rows_ - 1 + margin;
    const double dx = x1 - x0, dy = y1 - y0;
    double t0 = 0, t1 = 1;
    const double p[4] = {-dx, dx, -dy, dy};
    const double q[4] = {x0 - xmin, xmax - x0, y0 - ymin, ymax - y0};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0) { if (q[i] < 0) return false; continue; }
        const double r = q[i] / p[i];
        if (p[i] < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
        else { if (r < t0) return false; if (r < t1) t1 = r; }
    }
    const double nx0 = x0 + t0 * dx, ny0 = y0 + t0 * dy, nx1 = x0 + t1 * dx, ny1 = y0 + t1 * dy;
    x0 = nx0; y0 = ny0; x1 = nx1; y1 = ny1;
    return true;
}

// ---------------------------------------------------------------- function texts
FunctionText splitFunctionText(const std::string& text) {
    FunctionText out;
    int depth = 0;
    std::size_t open = std::string::npos;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '(') ++depth;
        else if (c == ')') --depth;
        else if (c == '[' && depth == 0) { open = i; break; }
        else if (c == ']') throw SyntaxError("Syntax ERROR: ] senza [");
    }
    if (open == std::string::npos) {
        out.body = trim(text);
        if (out.body.empty()) throw SyntaxError("Syntax ERROR: funzione vuota");
        return out;
    }
    std::size_t comma = open;
    while (comma > 0 && std::isspace(static_cast<unsigned char>(text[comma - 1]))) --comma;
    if (comma == 0 || text[comma - 1] != ',') throw SyntaxError("Syntax ERROR: usare funzione,[a,b] o funzione,[A=...]");
    out.body = trim(text.substr(0, comma - 1));
    if (out.body.empty()) throw SyntaxError("Syntax ERROR: funzione vuota");
    std::string inner = text.substr(open + 1);
    const std::size_t close = inner.find(']');
    if (close != std::string::npos) {
        if (!trim(inner.substr(close + 1)).empty()) throw SyntaxError("Syntax ERROR: testo dopo ]");
        inner.erase(close);
    }
    if (inner.find('[') != std::string::npos) throw SyntaxError("Syntax ERROR: [ annidate");
    auto parts = splitTop(inner);
    const std::size_t eq = parts[0].find('=');
    if (eq != std::string::npos) {
        const std::string var = trim(parts[0].substr(0, eq));
        const unsigned char v = var.size() == 1 ? static_cast<unsigned char>(var[0]) : 0;
        const char low = static_cast<char>(std::tolower(v));
        if (!std::isalpha(v) || low == 'x' || low == 'y' || low == 't' || low == 'r')
            throw SyntaxError("Syntax ERROR: variabile di sovrapposizione non valida (non X, Y, r, T)");
        out.overwrite = static_cast<char>(std::toupper(v));
        parts[0] = trim(parts[0].substr(eq + 1));
        for (const auto& p : parts) if (p.empty()) throw SyntaxError("Syntax ERROR: valore mancante in [ ]");
        if (parts.size() > MaxOverwriteValues) throw MathError("Argument ERROR: al massimo 10 valori in [A=...]");
        out.values = std::move(parts);
        return out;
    }
    if (parts.size() != 2 || parts[0].empty() || parts[1].empty()) throw SyntaxError("Syntax ERROR: intervallo ,[inizio,fine]");
    out.hasRange = true; out.rangeLo = parts[0]; out.rangeHi = parts[1];
    return out;
}

// ---------------------------------------------------------------- engine
class GraphEngine::Names final : public NameResolver {
public:
    const GraphSource* src{};
    mutable int index{-1};
    bool resolve(const std::string& n, double& out) const override {
        if (n.size() == 5 && n.compare(0, 4, "list") == 0 && n[4] >= '1' && n[4] <= '6') {
            if (!src || !src->lists || index < 0) return false;
            const auto& l = (*src->lists)[static_cast<std::size_t>(n[4] - '1')];
            if (static_cast<std::size_t>(index) >= l.size()) return false;
            out = l[static_cast<std::size_t>(index)];
            return true;
        }
        return src && src->names && src->names->resolve(n, out);
    }
};

GraphEngine::GraphEngine() : names_(std::make_unique<Names>()) {}
GraphEngine::~GraphEngine() = default;

const std::string& GraphEngine::error(std::size_t f) const {
    static const std::string none;
    return f < compiled_.size() ? compiled_[f].error : none;
}
const ast::Node* GraphEngine::body(std::size_t f) const { return f < compiled_.size() ? compiled_[f].body.get() : nullptr; }

void GraphEngine::build(const GraphSource& source, const std::vector<std::string>& pasted) {
    src_ = source;
    names_->src = &src_;
    plots_.clear();
    if (!src_.functions) return;
    auto evalNumber = [&](const std::string& text) {
        EvalContext ctx; ctx.memory = src_.memory; ctx.names = src_.names; ctx.functions = this;
        return evaluateReal(*parseExpression(text), ctx);
    };
    // 1) compile every defined function (references may point to unselected ones)
    for (std::size_t i = 0; i < MaxGraphFunctions; ++i) {
        Compiled& c = compiled_[i];
        c = Compiled{};
        const GraphFunction& f = (*src_.functions)[i];
        if (f.empty()) continue;
        try {
            if (f.type == GraphType::Param && (f.expr.empty() || f.expr2.empty())) throw SyntaxError("Syntax ERROR: inserire Xt e Yt");
            c.text = splitFunctionText(f.expr);
            c.body = parseExpression(c.text.body);
            if (f.type == GraphType::Param) {
                const FunctionText y = splitFunctionText(f.expr2);
                if (y.hasRange || y.overwrite) throw SyntaxError("Syntax ERROR: intervallo e [A=] vanno scritti dopo Xt");
                c.body2 = parseExpression(y.body);
            }
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& e) {
            c.body.reset(); c.body2.reset(); c.error = e.what();
        }
    }
    // 2) plots of the selected functions, with their variants
    for (std::size_t i = 0; i < MaxGraphFunctions && plots_.size() < MaxPlots; ++i) {
        Compiled& c = compiled_[i];
        const GraphFunction& f = (*src_.functions)[i];
        if (!f.selected || !c.body || !c.error.empty()) continue;
        try {
            Plot base;
            base.function = static_cast<int>(i);
            base.type = f.type;
            base.color = f.color < GraphColorCount ? f.color : defaultGraphColor(i);
            base.style = f.style;
            base.label = typePrefix(f.type) + std::to_string(i + 1);
            base.text = c.text.body;
            base.body = c.body; base.body2 = c.body2;
            base.variable = graphVariable(f.type);
            if (c.text.hasRange) {
                base.lo = evalNumber(c.text.rangeLo); base.hi = evalNumber(c.text.rangeHi);
                if (!(base.lo < base.hi)) throw MathError("Argument ERROR: intervallo [inizio,fine] con inizio<fine");
                base.hasRange = true;
            }
            unsigned lists = 0;
            collectLists(*c.body, lists);
            if (c.body2) collectLists(*c.body2, lists);
            if (lists && c.text.overwrite) throw MathError("Argument ERROR: Lista e [A=...] insieme");
            // Overwrite / List graphs: colours continue the blue, red, green, magenta, black sequence.
            const std::uint8_t start = base.color <= 4 ? base.color : defaultGraphColor(i);
            if (c.text.overwrite) {
                const int n = static_cast<int>(c.text.values.size());
                for (int k = 0; k < n && plots_.size() < MaxPlots; ++k) {
                    Plot p = base;
                    p.variant = k; p.variants = n;
                    p.bindName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(c.text.overwrite)));
                    p.bindValue = evalNumber(c.text.values[static_cast<std::size_t>(k)]);
                    p.detail = std::string(1, c.text.overwrite) + "=" + c.text.values[static_cast<std::size_t>(k)];
                    if (k > 0) { p.color = kOverwriteColors[(start + k) % 5]; p.style = LineStyle::Normal; }
                    plots_.push_back(std::move(p));
                }
                continue;
            }
            if (lists) {
                std::size_t n = 0; bool first = true;
                for (int l = 0; l < 6; ++l) {
                    if (!(lists & (1u << l))) continue;
                    const std::size_t len = src_.lists ? (*src_.lists)[static_cast<std::size_t>(l)].size() : 0;
                    if (first) { n = len; first = false; }
                    else if (len != n) throw MathError("Dimension ERROR: liste di lunghezza diversa");
                }
                if (n == 0) throw MathError("Dimension ERROR: lista vuota");
                for (std::size_t k = 0; k < n && plots_.size() < MaxPlots; ++k) {
                    Plot p = base;
                    p.variant = static_cast<int>(k); p.variants = static_cast<int>(n);
                    p.listIndex = static_cast<int>(k);
                    p.detail = "List[" + std::to_string(k + 1) + "]";
                    if (k > 0) { p.color = kOverwriteColors[(start + k) % 5]; p.style = LineStyle::Normal; }
                    plots_.push_back(std::move(p));
                }
                continue;
            }
            plots_.push_back(std::move(base));
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& e) {
            c.error = e.what();
            while (!plots_.empty() && plots_.back().function == static_cast<int>(i)) plots_.pop_back();
        }
    }
    // 3) pasted graphs: blue, normal line (manual 5-31)
    std::size_t pastedCount = 0;
    for (const auto& text : pasted) {
        if (plots_.size() >= MaxPlots || pastedCount >= MaxPasted) break;
        try {
            Plot p = makePlot(text, GraphType::Y, 0, LineStyle::Normal);
            p.pasted = true;
            p.label = "Y";
            plots_.push_back(std::move(p));
            ++pastedCount;
        } catch (const std::bad_alloc&) { throw; } catch (const std::exception&) {}
    }
}

Plot GraphEngine::makePlot(const std::string& text, GraphType type, std::uint8_t color, LineStyle style) const {
    Plot p;
    p.type = type; p.color = color; p.style = style;
    std::string body = trim(text);
    // A pasted "Y=expression" is graphed as Y= expression (manual 5-30).
    if (body.size() > 2 && (body[0] == 'Y' || body[0] == 'y') && body[1] == '=') body = trim(body.substr(2));
    p.text = body;
    p.body = parseExpression(body);
    p.variable = graphVariable(type);
    p.label = typePrefix(type);
    return p;
}

double GraphEngine::evalBody(const Plot& p, const ast::Node& b, double v) const {
    EvalContext ctx;
    ctx.memory = src_.memory;
    ctx.names = names_.get();
    ctx.functions = this;
    ctx.bind(p.variable, v);
    if (p.bindName[0]) ctx.bind(p.bindName, p.bindValue);
    names_->index = p.listIndex;
    return evaluateReal(b, ctx);
}
bool GraphEngine::eval(const Plot& p, double v, double& out) const noexcept {
    if (!p.body || !std::isfinite(v)) return false;
    if (p.hasRange && (v < p.lo || v > p.hi)) return false;
    try { out = evalBody(p, *p.body, v); return finiteValue(out); } catch (...) { return false; }
}
double GraphEngine::evalOrThrow(const Plot& p, double v) const {
    if (!p.body) throw SyntaxError("Syntax ERROR: funzione vuota");
    if (p.hasRange && (v < p.lo || v > p.hi)) throw MathError("Math ERROR: fuori dall'intervallo del grafico");
    return evalBody(p, *p.body, v);
}
bool GraphEngine::polarRadius(const Plot& p, double theta, double& r) const noexcept { return eval(p, theta, r); }
bool GraphEngine::point(const Plot& p, double t, double& x, double& y) const noexcept {
    if (!p.body || !std::isfinite(t)) return false;
    if (p.hasRange && (t < p.lo || t > p.hi)) return false;
    try {
        if (p.type == GraphType::Param) {
            if (!p.body2) return false;
            x = evalBody(p, *p.body, t);
            y = evalBody(p, *p.body2, t);
        } else {
            const double r = evalBody(p, *p.body, t);
            const double th = toRadians(t);
            x = r * std::cos(th); y = r * std::sin(th);
        }
        return finiteValue(x) && finiteValue(y);
    } catch (...) { return false; }
}
bool GraphEngine::derivative(const Plot& p, double v, double& out) const noexcept {
    try {
        if (p.type == GraphType::Param) {
            const double dx = differentiateFunction([&](double t) { return evalBody(p, *p.body, t); }, v);
            const double dy = differentiateFunction([&](double t) { return evalBody(p, *p.body2, t); }, v);
            if (dx == 0) return false;
            out = dy / dx;
        } else {
            out = differentiateFunction([&](double t) { return evalOrThrow(p, t); }, v);
        }
        return finiteValue(out);
    } catch (...) { return false; }
}

const ast::Node* GraphEngine::function(const std::string& name, const char*& variable) const {
    if (!src_.functions || !isFunctionReferenceName(name)) return nullptr;
    std::size_t k = 0;
    while (k < name.size() && std::isalpha(static_cast<unsigned char>(name[k]))) ++k;
    const std::string kind = name.substr(0, k);
    const int index = std::atoi(name.c_str() + k) - 1;
    if (index < 0 || index >= static_cast<int>(MaxGraphFunctions)) return nullptr;
    const GraphFunction& f = (*src_.functions)[static_cast<std::size_t>(index)];
    const Compiled& c = compiled_[static_cast<std::size_t>(index)];
    if (!c.body || !c.error.empty()) return nullptr;
    bool ok = false;
    const ast::Node* b = c.body.get();
    if (kind == "y") ok = isYType(f.type);
    else if (kind == "r") ok = f.type == GraphType::Polar;
    else if (kind == "x") ok = isXType(f.type);
    else if (kind == "xt") ok = f.type == GraphType::Param;
    else if (kind == "yt") { ok = f.type == GraphType::Param; b = c.body2.get(); }
    if (!ok || !b) return nullptr;
    variable = graphVariable(f.type);
    return b;
}

std::string GraphEngine::coefficients(const ast::Node& b, const char* variable) {
    std::string letters;
    collectTokens(b, letters, variable);
    std::sort(letters.begin(), letters.end());
    return letters;
}

// ---------------------------------------------------------------- sampling
std::size_t sampleCount(const Plot& p, const Viewport& vp) {
    if (isYType(p.type)) return static_cast<std::size_t>(vp.columns());
    if (isXType(p.type)) return static_cast<std::size_t>(vp.rows());
    const ViewWindow& v = vp.view();
    const double span = (v.tmax - v.tmin) / v.tstep;
    if (!std::isfinite(span) || span <= 0) return 0;
    if (span > static_cast<double>(MaxParamSteps)) return MaxParamSteps + 1;
    const auto n = static_cast<std::size_t>(std::floor(span + 1e-9));
    return n + 1 + (static_cast<double>(n) < span - 1e-9 ? 1 : 0);
}
double sampleParameter(const Plot& p, const Viewport& vp, std::size_t k) {
    if (isYType(p.type)) return vp.xAt(static_cast<double>(k));
    if (isXType(p.type)) return vp.yAt(static_cast<double>(k));
    const ViewWindow& v = vp.view();
    const double span = (v.tmax - v.tmin) / v.tstep;
    if (span > static_cast<double>(MaxParamSteps)) {
        // Very small pitch: the same range with MaxParamSteps steps.
        return k >= MaxParamSteps ? v.tmax : v.tmin + (v.tmax - v.tmin) * static_cast<double>(k) / MaxParamSteps;
    }
    const double t = v.tmin + static_cast<double>(k) * v.tstep;
    return (v.tstep > 0 ? t > v.tmax : t < v.tmax) ? v.tmax : t;
}

namespace {
struct Sampler {
    const GraphEngine& e;
    const Plot& p;
    const Viewport& vp;
    Curve& c;
    int& budget;
    // Screen position of parameter v.
    bool map(double v, double& sx, double& sy) {
        --budget;
        double a = 0, b = 0;
        bool ok;
        if (isYType(p.type)) { ok = e.eval(p, v, b); a = v; }
        else if (isXType(p.type)) { ok = e.eval(p, v, a); b = v; }
        else ok = e.point(p, v, a, b);
        if (!ok) { sx = sy = NaN; return false; }
        sx = clampScreen(vp.screenX(a)); sy = clampScreen(vp.screenY(b));
        return true;
    }
    bool full() const { return c.xy.size() + 6 >= 2 * MaxCurvePoints; }
    void penUp() {
        if (!c.xy.empty() && c.xy[c.xy.size() - 2] != PenUp) { c.xy.push_back(PenUp); c.xy.push_back(0); }
    }
    void push(double x, double y) {
        const auto ix = static_cast<std::int16_t>(std::lround(x)), iy = static_cast<std::int16_t>(std::lround(y));
        if (c.xy.size() >= 2 && c.xy[c.xy.size() - 2] == ix && c.xy[c.xy.size() - 1] == iy) return;
        c.xy.push_back(ix); c.xy.push_back(iy);
    }
    bool lastIs(double x, double y) const {
        if (c.xy.size() < 2 || c.xy[c.xy.size() - 2] == PenUp) return false;
        return c.xy[c.xy.size() - 2] == static_cast<std::int16_t>(std::lround(x)) && c.xy[c.xy.size() - 1] == static_cast<std::int16_t>(std::lround(y));
    }
    void segment(double ax, double ay, double bx, double by) {
        double x0 = ax, y0 = ay, x1 = bx, y1 = by;
        if (!vp.clip(x0, y0, x1, y1)) { penUp(); return; }
        if (!lastIs(x0, y0)) { penUp(); push(x0, y0); }
        push(x1, y1);
        // The polyline continues only if b itself is inside the clip rectangle.
        if (!vp.inside(bx, by, 3)) penUp();
    }
    void dot(double x, double y) {
        if (!vp.inside(x, y, 0.5)) return;
        penUp(); push(x, y); penUp();
    }
    static double dist(double ax, double ay, double bx, double by) { return std::max(std::abs(bx - ax), std::abs(by - ay)); }
    // True when the curve breaks between va and vb (pole, jump or undefined point):
    // the half with the longer chord is bisected; a continuous curve's chord
    // shrinks below JumpPx, a jump keeps it.
    bool jump(double va, double ax, double ay, double vb, double bx, double by) {
        for (int d = 0; d < JumpDepth; ++d) {
            if (budget <= 0) return false;
            const double vm = 0.5 * (va + vb);
            if (vm == va || vm == vb) return dist(ax, ay, bx, by) > JumpPx;
            double mx, my;
            if (!map(vm, mx, my)) return true;
            const double da = dist(ax, ay, mx, my), db = dist(mx, my, bx, by);
            if (std::max(da, db) <= JumpPx) return false;
            if (da >= db) { vb = vm; bx = mx; by = my; } else { va = vm; ax = mx; ay = my; }
        }
        return true;
    }
    bool offSameSide(double ax, double ay, double bx, double by) const {
        const double m = 4;
        const double l = vp.left() - m, r = vp.left() + vp.columns() - 1 + m, t = vp.top() - m, b = vp.top() + vp.rows() - 1 + m;
        return (ax < l && bx < l) || (ax > r && bx > r) || (ay < t && by < t) || (ay > b && by > b);
    }
    void connect(double v0, double x0, double y0, bool ok0, double v1, double x1, double y1, bool ok1) {
        if (!ok0 && !ok1) { penUp(); return; }
        if (ok0 != ok1) {
            // Edge of the domain (sqrt(x) at 0, a graph range...): bisect towards it
            // so that the curve ends where the function stops being defined.
            double good = ok0 ? v0 : v1, bad = ok0 ? v1 : v0;
            double gx = ok0 ? x0 : x1, gy = ok0 ? y0 : y1, ex = gx, ey = gy;
            for (int d = 0; d < EdgeDepth && budget > 0; ++d) {
                const double vm = 0.5 * (good + bad);
                double mx, my;
                if (map(vm, mx, my)) { good = vm; ex = mx; ey = my; } else bad = vm;
            }
            const bool edgeJump = dist(gx, gy, ex, ey) > JumpPx && jump(ok0 ? v0 : good, ok0 ? x0 : ex, ok0 ? y0 : ey, ok0 ? good : v1, ok0 ? ex : x1, ok0 ? ey : y1);
            if (ok0) { if (!edgeJump && !offSameSide(x0, y0, ex, ey)) segment(x0, y0, ex, ey); penUp(); }
            else { penUp(); if (!edgeJump && !offSameSide(ex, ey, x1, y1)) segment(ex, ey, x1, y1); }
            return;
        }
        if (offSameSide(x0, y0, x1, y1)) { penUp(); return; }
        if (dist(x0, y0, x1, y1) > JumpPx && jump(v0, x0, y0, v1, x1, y1)) { penUp(); return; }
        segment(x0, y0, x1, y1);
    }
};
} // namespace

bool sampleStep(const GraphEngine& engine, const Plot& p, const Viewport& vp, bool plotOnly, Curve& c, int& budget) {
    if (c.done) return true;
    if (c.steps == 0 && c.next == 0) {
        c.steps = sampleCount(p, vp);
        if (isInequality(p.type)) c.boundary.assign(c.steps, std::numeric_limits<float>::quiet_NaN());
        c.xy.reserve(std::min<std::size_t>(2 * MaxCurvePoints, 2 * c.steps + 16));
    }
    Sampler s{engine, p, vp, c, budget};
    while (c.next < c.steps && budget > 0) {
        if (s.full()) { c.truncated = true; break; }
        const double v = sampleParameter(p, vp, c.next);
        double sx, sy;
        const bool ok = s.map(v, sx, sy);
        if (ok && !c.boundary.empty()) c.boundary[c.next] = static_cast<float>(isYType(p.type) ? sy : sx);
        if (plotOnly) {
            if (ok) s.dot(sx, sy);
        } else if (c.havePrev) {
            s.connect(c.prevV, c.prevSx, c.prevSy, c.prevOk, v, sx, sy, ok);
        } else if (ok && vp.inside(sx, sy, 3)) {
            s.penUp(); s.push(sx, sy);
        }
        c.prevV = v; c.prevSx = sx; c.prevSy = sy; c.prevOk = ok; c.havePrev = true;
        ++c.next;
    }
    if (c.next >= c.steps || c.truncated) c.done = true;
    return c.done;
}

// ---------------------------------------------------------------- V-Window / ZOOM
ViewWindow initialWindow(AngleMode a) {
    ViewWindow v;
    if (a == AngleMode::Degrees) { v.tmax = 360; v.tstep = 3; }
    else if (a == AngleMode::Gradians) { v.tmax = 400; v.tstep = 400.0 / 120.0; }
    return v;
}
ViewWindow trigWindow(const ViewWindow& in, AngleMode a) {
    ViewWindow v = in;
    const double half = a == AngleMode::Degrees ? 180.0 : a == AngleMode::Gradians ? 200.0 : kPi;
    v.xmin = -3 * half; v.xmax = 3 * half; v.xscl = half / 2;
    v.ymin = -1.6; v.ymax = 1.6; v.yscl = 0.5;
    return v;
}
ViewWindow standardWindow(const ViewWindow& in) {
    ViewWindow v = in;
    v.xmin = -10; v.xmax = 10; v.xscl = 1; v.ymin = -10; v.ymax = 10; v.yscl = 1;
    return v;
}
namespace {
ViewWindow checked(const ViewWindow& v) {
    const std::string e = validateView(v);
    if (!e.empty()) throw MathError(e);
    return v;
}
} // namespace
ViewWindow withXdot(const ViewWindow& in, double xdot) {
    if (!(xdot > 0) || !std::isfinite(xdot)) throw MathError("Argument ERROR: Xdot deve essere positivo");
    ViewWindow v = in;
    v.xmax = v.xmin + xdot * (graphgeo::Columns - 1);
    return checked(v);
}
ViewWindow zoomAt(const ViewWindow& in, double cx, double cy, double fx, double fy) {
    if (!(fx > 0) || !(fy > 0) || !std::isfinite(fx) || !std::isfinite(fy)) throw MathError("Argument ERROR: fattore di zoom non valido");
    ViewWindow v = in;
    const double hw = 0.5 * (in.xmax - in.xmin) / fx, hh = 0.5 * (in.ymax - in.ymin) / fy;
    v.xmin = cx - hw; v.xmax = cx + hw; v.ymin = cy - hh; v.ymax = cy + hh;
    return checked(v);
}
ViewWindow zoomBox(const ViewWindow& in, double x0, double y0, double x1, double y1) {
    if (x0 == x1 || y0 == y1) throw MathError("Argument ERROR: il riquadro deve avere larghezza e altezza");
    ViewWindow v = in;
    v.xmin = std::min(x0, x1); v.xmax = std::max(x0, x1);
    v.ymin = std::min(y0, y1); v.ymax = std::max(y0, y1);
    return checked(v);
}
ViewWindow squareWindow(const ViewWindow& in) {
    ViewWindow v = in;
    const double c = 0.5 * (in.xmin + in.xmax), half = 0.5 * viewYdot(in) * (graphgeo::Columns - 1);
    v.xmin = c - half; v.xmax = c + half;
    return checked(v);
}
ViewWindow squareXBase(const ViewWindow& in) {
    ViewWindow v = in;
    const double c = 0.5 * (in.ymin + in.ymax), half = 0.5 * viewXdot(in) * (graphgeo::Rows - 1);
    v.ymin = c - half; v.ymax = c + half;
    return checked(v);
}
namespace {
double roundDot(double d) {
    const double e = std::pow(10.0, std::floor(std::log10(d)));
    const double r = std::round(d / e) * e;
    return r > 0 ? r : e;
}
} // namespace
ViewWindow roundWindow(const ViewWindow& in, double cx, double cy) {
    ViewWindow v = in;
    const double dx = roundDot(viewXdot(in)), dy = roundDot(viewYdot(in));
    v.xmin = std::round((cx - 0.5 * dx * (graphgeo::Columns - 1)) / dx) * dx;
    v.xmax = v.xmin + dx * (graphgeo::Columns - 1);
    v.ymin = std::round((cy - 0.5 * dy * (graphgeo::Rows - 1)) / dy) * dy;
    v.ymax = v.ymin + dy * (graphgeo::Rows - 1);
    return checked(v);
}
ViewWindow integerWindow(const ViewWindow& in, double cx, double cy) {
    ViewWindow v = in;
    v.xmin = std::round(cx - 0.5 * (graphgeo::Columns - 1));
    v.xmax = v.xmin + (graphgeo::Columns - 1);
    v.ymin = std::round(cy - 0.5 * (graphgeo::Rows - 1));
    v.ymax = v.ymin + (graphgeo::Rows - 1);
    return checked(v);
}
bool autoWindow(ViewWindow& v, std::vector<double> values) {
    values.erase(std::remove_if(values.begin(), values.end(), [](double d) { return !std::isfinite(d); }), values.end());
    if (values.empty()) return false;
    std::sort(values.begin(), values.end());
    double lo = values.front(), hi = values.back();
    if (values.size() >= 20) {
        // Near a pole a few huge values would flatten the graph: use the 2..98 % range.
        const double qlo = values[values.size() * 2 / 100], qhi = values[values.size() - 1 - values.size() * 2 / 100];
        if (hi - lo > 20 * (qhi - qlo)) { lo = qlo; hi = qhi; }
    }
    if (hi - lo <= 1e-10 * std::max(1.0, std::abs(hi))) { lo -= 1; hi += 1; }
    const double pad = (hi - lo) * 0.05;
    ViewWindow n = v;
    n.ymin = lo - pad; n.ymax = hi + pad;
    if (!validateView(n).empty()) return false;
    v = n;
    return true;
}
bool tickRange(double lo, double hi, double scale, int maxTicks, double& first, int& count) {
    if (!(scale > 0) || !(hi > lo)) return false;
    const double a = std::ceil(lo / scale), b = std::floor(hi / scale);
    if (!std::isfinite(a) || !std::isfinite(b) || b - a + 1 > maxTicks) return false;
    if (b < a) return false;
    first = a * scale; count = static_cast<int>(b - a) + 1;
    return true;
}
} // namespace fx::graph
