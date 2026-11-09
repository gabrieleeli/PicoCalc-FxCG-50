#include "table_model.hpp"
#include <cmath>
#include <cstdio>

namespace fx::table {
bool tableType(GraphType t) noexcept { return t == GraphType::Y || t == GraphType::Polar || t == GraphType::Param; }
const char* variableName(GraphType funcType) noexcept {
    return funcType == GraphType::Polar ? "\xCE\xB8" : funcType == GraphType::Param ? "T" : "X";
}
const char* derivativeName(GraphType funcType) noexcept { return funcType == GraphType::Polar ? "dr/d\xCE\xB8" : "dY/dX"; }

std::vector<double> rangeRows(double start, double end, double step) {
    if (!std::isfinite(start) || !std::isfinite(end) || !std::isfinite(step) || !(step > 0) || end < start)
        throw MathError("Argument ERROR: Start<=End e Step>0");
    const double count = std::floor((end - start) / step + 1e-9) + 1;
    if (count > static_cast<double>(MaxRows))
        throw MathError("Memory ERROR: massimo 999 righe (aumentare Step)");
    std::vector<double> rows(static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < rows.size(); ++i) {
        rows[i] = start + static_cast<double>(i) * step;
        // -0.1+0.1 is 0, not 2.8E-17: snap values within rounding of a multiple of Step
        if (std::abs(rows[i]) < 1e-9 * step) rows[i] = 0;
    }
    return rows;
}

std::vector<double> listRows(const std::vector<double>& list) {
    if (list.empty()) throw MathError("Dimension ERROR: lista vuota");
    if (list.size() > MaxRows) throw MathError("Memory ERROR: massimo 999 righe");
    return list;
}

std::vector<Column> columns(const graph::GraphEngine& engine, GraphType funcType, bool derivative) {
    std::vector<Column> out;
    const auto& plots = engine.plots();
    for (int i = 0; i < static_cast<int>(plots.size()); ++i) {
        const graph::Plot& p = plots[static_cast<std::size_t>(i)];
        if (p.type != funcType || p.pasted || p.variant != 0) continue;
        if (derivative && (p.hasRange || p.variants > 1))
            throw MathError("Math ERROR: Derivative On con un intervallo [a,b] o [A=...] in " + p.label);
        if (out.size() + 3 > MaxColumns) break;
        Column c;
        c.plot = i;
        c.color = p.color;
        c.title = p.label;
        out.push_back(c);
        if (funcType == GraphType::Param) {
            Column y = c;
            y.kind = Column::Kind::Second;
            y.title = "Yt" + p.label.substr(p.label.size() > 2 ? 2 : p.label.size());
            out.push_back(y);
        }
        if (derivative) {
            Column d = c;
            d.kind = Column::Kind::Derivative;
            d.title = (funcType == GraphType::Param ? "Yt" + p.label.substr(p.label.size() > 2 ? 2 : p.label.size()) : p.label) + "'";
            out.push_back(d);
        }
    }
    return out;
}

bool cell(const graph::GraphEngine& engine, const Column& c, double v, double& out) noexcept {
    const auto& plots = engine.plots();
    if (c.plot < 0 || c.plot >= static_cast<int>(plots.size())) return false;
    const graph::Plot& p = plots[static_cast<std::size_t>(c.plot)];
    if (c.kind == Column::Kind::Derivative) return engine.derivative(p, v, out) && std::isfinite(out);
    if (p.type == GraphType::Param) {
        double x, y;
        if (!engine.point(p, v, x, y)) return false;
        out = c.kind == Column::Kind::Second ? y : x;
        return std::isfinite(out);
    }
    return engine.eval(p, v, out) && std::isfinite(out);
}

bool point(const graph::GraphEngine& engine, const graph::Plot& p, double v, double& x, double& y) noexcept {
    if (p.type == GraphType::Y) { x = v; return engine.eval(p, v, y) && std::isfinite(y); }
    return engine.point(p, v, x, y) && std::isfinite(x) && std::isfinite(y);
}

namespace {
// "1.23e+15" -> "1.23E15", "1e-07" -> "1E-7"
std::string compactExponent(std::string s) {
    const auto e = s.find_first_of("eE");
    if (e == std::string::npos) return s;
    std::string mant = s.substr(0, e), exp = s.substr(e + 1);
    bool neg = false;
    if (!exp.empty() && (exp[0] == '+' || exp[0] == '-')) { neg = exp[0] == '-'; exp.erase(0, 1); }
    while (exp.size() > 1 && exp[0] == '0') exp.erase(0, 1);
    return mant + "E" + (neg ? "-" : "") + exp;
}
} // namespace

std::string cellText(double v) {
    if (!std::isfinite(v)) return "ERROR";
    if (v == 0) return "0";
    // The most significant digits that fit in 6 characters, plain or with an exponent.
    char buf[48];
    std::string shortest;
    for (int p = 6; p >= 1; --p) {
        std::snprintf(buf, sizeof(buf), "%.*g", p, v);
        std::string plain = compactExponent(buf);
        if (plain.size() <= 6) return plain;
        std::snprintf(buf, sizeof(buf), "%.*e", p - 1, v);
        std::string sci = buf;
        const auto e = sci.find('e');
        std::string mant = sci.substr(0, e);
        if (mant.find('.') != std::string::npos) {                 // 1.2500 -> 1.25
            while (!mant.empty() && mant.back() == '0') mant.pop_back();
            if (!mant.empty() && mant.back() == '.') mant.pop_back();
        }
        sci = compactExponent(mant + sci.substr(e));
        if (sci.size() <= 6) return sci;
        if (shortest.empty() || sci.size() < shortest.size()) shortest = sci;
    }
    return shortest;                               // e.g. -1E-100: 7 characters, nothing shorter exists
}
} // namespace fx::table
