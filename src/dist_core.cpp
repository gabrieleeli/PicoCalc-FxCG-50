// Nucleo delle distribuzioni (v1.18.0, solo Pimoroni Pico Plus 2 W): vedi dist_core.hpp.
#include "dist_core.hpp"
#include "calc_library.hpp"
#include "expression.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace fx::dist {
namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();
void need(bool cond, const char* msg) { if (!cond) throw MathError(msg); }
double finite(double v) { need(std::isfinite(v), "Math ERROR: dato non finito"); return v; }
// Limiti come nel DIST della v1.16.0: |v| >= 1E99 vale ±infinito.
double bound(double v) { return std::abs(v) >= 1e99 ? (v < 0 ? -kInf : kInf) : v; }
using stats::ResultVar;
} // namespace

std::size_t indexOf(Kind k) noexcept { return static_cast<std::size_t>(k.family) * 3 + static_cast<std::size_t>(k.func); }
Kind kindAt(std::size_t i) noexcept { i %= KindCount; return {static_cast<Family>(i / 3), static_cast<Func>(i % 3)}; }
const char* familyName(Family f) noexcept {
    static const char* const n[] = {"Normal", "Student-t", "\xCF\x87\xC2\xB2", "F", "Binomial", "Poisson", "Geometric", "Hypergeometric"};
    return static_cast<std::size_t>(f) < FamilyCount ? n[static_cast<std::size_t>(f)] : "?";
}
const char* familyKey(Family f) noexcept {
    static const char* const n[] = {"NORM", "t", "CHI", "F", "BINOMIAL", "POISSON", "GEO", "H.GEO"};
    return static_cast<std::size_t>(f) < FamilyCount ? n[static_cast<std::size_t>(f)] : "?";
}
const char* kindName(Kind k) noexcept {
    static const char* const n[KindCount] = {
        "Normal P.D", "Normal C.D", "Inverse Normal", "Student-t P.D", "Student-t C.D", "Inverse Student-t",
        "\xCF\x87\xC2\xB2 P.D", "\xCF\x87\xC2\xB2 C.D", "Inverse \xCF\x87\xC2\xB2", "F P.D", "F C.D", "Inverse F",
        "Binomial P.D", "Binomial C.D", "Inverse Binomial", "Poisson P.D", "Poisson C.D", "Inverse Poisson",
        "Geometric P.D", "Geometric C.D", "Inverse Geometric", "Hypergeometric P.D", "Hypergeometric C.D", "Inverse Hypergeometric"};
    return n[indexOf(k) % KindCount];
}
const char* kindKey(Kind k) noexcept {
    static const char* const n[KindCount] = {"Npd", "Ncd", "InvN", "tpd", "tcd", "Invt", "Cpd", "Ccd", "InvC", "Fpd", "Fcd", "InvF",
                                             "Bpd", "Bcd", "InvB", "Ppd", "Pcd", "InvP", "Gpd", "Gcd", "InvG", "Hpd", "Hcd", "InvH"};
    return n[indexOf(k) % KindCount];
}
bool continuous(Family f) noexcept { return f == Family::Normal || f == Family::T || f == Family::Chi || f == Family::F; }
const char* tailName(Tail t) noexcept { return t == Tail::Right ? "Right" : t == Tail::Central ? "Central" : "Left"; }

std::vector<ParamSpec> paramSpecs(Kind k) {
    std::vector<ParamSpec> args;
    switch (k.func) {
    case Func::PD: args = {{"x", k.family == Family::Chi || k.family == Family::F ? 1.0 : k.family == Family::Binomial || k.family == Family::Poisson || k.family == Family::Geometric ? 2.0 : k.family == Family::Hypergeometric ? 1.0 : 0.0}}; break;
    case Func::CD: {
        double lo = -1, hi = 1;
        switch (k.family) {
        case Family::Chi: lo = 0; hi = 3.841; break;
        case Family::F: lo = 0; hi = 4.96; break;
        case Family::Binomial: case Family::Poisson: lo = 0; hi = 2; break;
        case Family::Geometric: lo = 1; hi = 2; break;
        case Family::Hypergeometric: lo = 0; hi = 1; break;
        default: break;
        }
        args = {{"Lower", lo}, {"Upper", hi}};
        break;
    }
    case Func::Inv: args = {{"Area", k.family == Family::Normal ? 0.95 : continuous(k.family) ? 0.05 : 0.5}}; break;
    }
    switch (k.family) {
    case Family::Normal: args.push_back({"\xCF\x83", 1}); args.push_back({"\xCE\xBC", 0}); break;
    case Family::T: case Family::Chi: args.push_back({"df", 1}); break;
    case Family::F: args.push_back({"n:df", 1}); args.push_back({"d:df", 10}); break;
    case Family::Binomial: args.push_back({"Numtrial", 10}); args.push_back({"p", 0.5}); break;
    case Family::Poisson: args.push_back({"\xCE\xBC", 1}); break;
    case Family::Geometric: args.push_back({"p", 0.5}); break;
    case Family::Hypergeometric: args.push_back({"n", 5}); args.push_back({"M", 10}); args.push_back({"N", 20}); break;
    default: break;
    }
    return args;
}
std::size_t argCount(Kind k) noexcept { return k.func == Func::CD ? 2 : 1; }
bool hasTail(Kind k) noexcept { return k.family == Family::Normal && k.func == Func::Inv; }
std::vector<const char*> listLabels(Kind k) {
    if (k.func == Func::CD) return continuous(k.family) ? std::vector<const char*>{"Lower List", "Upper List"} : std::vector<const char*>{"Upper List"};
    return {k.func == Func::Inv ? "Area List" : "List"};
}

Result compute(Kind k, const Input& in) {
    const std::vector<ParamSpec> spec = paramSpecs(k);
    need(in.values.size() == spec.size(), "Argument ERROR: parametri mancanti");
    std::vector<double> v = in.values;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (k.func == Func::CD && i < 2) { need(!std::isnan(v[i]), "Math ERROR: dato non finito"); v[i] = bound(v[i]); }
        else finite(v[i]);
    }
    Result r;
    auto put = [&](const char* label, double value, int var) {
        r.rows.emplace_back(label, value);
        if (var >= 0) r.vars.emplace_back(static_cast<std::uint8_t>(var), value);
    };
    const std::size_t a = argCount(k);
    const double* p = v.data() + a;                        // parametri della distribuzione
    if (k.func == Func::CD) need(v[0] <= v[1], "Argument ERROR: Lower <= Upper");
    // CD discreta P(Lower <= X <= Upper) = CD(Upper) - CD(Lower - 1).
    auto discreteRange = [&](auto&& cd) {
        const double hi = std::floor(std::min(v[1], 1e15)), lo = std::ceil(std::max(v[0], -1e15));
        if (hi < lo) return 0.0;
        const double up = cd(hi), down = lo - 1 >= 0 ? cd(lo - 1) : 0.0;
        return std::clamp(up - down, 0.0, 1.0);
    };
    switch (k.family) {
    case Family::Normal:
        if (k.func == Func::PD) put("p", calclib::normPD(v[0], p[0], p[1]), stats::VDistP);
        else if (k.func == Func::CD) {
            put("p", calclib::normCD(v[0], v[1], p[0], p[1]), stats::VDistP);
            need(p[0] > 0, "Argument ERROR: \xCF\x83>0");
            put("z:Low", (v[0] - p[1]) / p[0], stats::VZLow);
            put("z:Up", (v[1] - p[1]) / p[0], stats::VZUp);
        } else if (in.tail == Tail::Central) {
            double lo = 0, hi = 0;
            calclib::invNormCentral(v[0], p[0], p[1], lo, hi);
            put("x1Inv", lo, stats::VX1Inv); put("x2Inv", hi, stats::VX2Inv);
        } else put("xInv", calclib::invNormCD(in.tail == Tail::Right ? 1 : -1, v[0], p[0], p[1]), stats::VXInv);
        break;
    case Family::T:
        if (k.func == Func::PD) put("p", calclib::tPD(v[0], p[0]), stats::VDistP);
        else if (k.func == Func::CD) { put("p", calclib::tCD(v[0], v[1], p[0]), stats::VDistP); put("t:Low", v[0], stats::VTLow); put("t:Up", v[1], stats::VTUp); }
        else put("xInv", calclib::invTCD(v[0], p[0]), stats::VXInv);
        break;
    case Family::Chi:
        if (k.func == Func::PD) put("p", calclib::chiPD(v[0], p[0]), stats::VDistP);
        else if (k.func == Func::CD) put("p", calclib::chiCD(v[0], v[1], p[0]), stats::VDistP);
        else put("xInv", calclib::invChiCD(v[0], p[0]), stats::VXInv);
        break;
    case Family::F:
        if (k.func == Func::PD) put("p", calclib::fPD(v[0], p[0], p[1]), stats::VDistP);
        else if (k.func == Func::CD) put("p", calclib::fCD(v[0], v[1], p[0], p[1]), stats::VDistP);
        else put("xInv", calclib::invFCD(v[0], p[0], p[1]), stats::VXInv);
        break;
    case Family::Binomial:
        if (k.func == Func::PD) put("p", calclib::binomialPD(v[0], p[0], p[1]), stats::VDistP);
        else if (k.func == Func::CD) { (void)calclib::binomialPD(0, p[0], p[1]); put("p", discreteRange([&](double x) { return calclib::binomialCD(x, p[0], p[1]); }), stats::VDistP); }
        else put("xInv", calclib::invBinomialCD(v[0], p[0], p[1]), stats::VXInv);
        break;
    case Family::Poisson:
        if (k.func == Func::PD) put("p", calclib::poissonPD(v[0], p[0]), stats::VDistP);
        else if (k.func == Func::CD) { (void)calclib::poissonPD(0, p[0]); put("p", discreteRange([&](double x) { return calclib::poissonCD(x, p[0]); }), stats::VDistP); }
        else put("xInv", calclib::invPoissonCD(v[0], p[0]), stats::VXInv);
        break;
    case Family::Geometric:
        if (k.func == Func::PD) put("p", calclib::geoPD(v[0], p[0]), stats::VDistP);
        else if (k.func == Func::CD) { (void)calclib::geoPD(1, p[0]); put("p", discreteRange([&](double x) { return calclib::geoCD(x, p[0]); }), stats::VDistP); }
        else put("xInv", calclib::invGeoCD(v[0], p[0]), stats::VXInv);
        break;
    case Family::Hypergeometric:
        if (k.func == Func::PD) put("p", calclib::hypergeoPD(v[0], p[0], p[1], p[2]), stats::VDistP);
        else if (k.func == Func::CD) { (void)calclib::hypergeoPD(0, p[0], p[1], p[2]); put("p", discreteRange([&](double x) { return calclib::hypergeoCD(x, p[0], p[1], p[2]); }), stats::VDistP); }
        else put("xInv", calclib::invHypergeoCD(v[0], p[0], p[1], p[2]), stats::VXInv);
        break;
    default: throw MathError("Argument ERROR: distribuzione");
    }
    for (const auto& row : r.rows) need(std::isfinite(row.second) || k.func == Func::CD, "Math ERROR: risultato non finito");
    r.main = r.rows.front().second;
    return r;
}

ListTask::ListTask(Kind k, Input base, std::vector<double> first, std::vector<double> second)
    : k_(k), base_(std::move(base)), first_(std::move(first)), second_(std::move(second)) {
    if (first_.empty()) throw DimensionError("Dimension ERROR: lista vuota");
    need(!(hasTail(k_) && base_.tail == Tail::Central), "Argument ERROR: Tail Central solo con Data: Variable");
    const bool twoLists = k_.func == Func::CD && continuous(k_.family);
    if (twoLists && second_.size() != first_.size()) throw DimensionError("Dimension ERROR: Lower List e Upper List di lunghezza diversa");
    out_.reserve(first_.size());
}
bool ListTask::step(step::Budget& budget) {
    const bool twoLists = k_.func == Func::CD && continuous(k_.family);
    while (next_ < first_.size()) {
        Input in = base_;
        if (k_.func == Func::CD) {
            if (twoLists) { in.values[0] = first_[next_]; in.values[1] = second_[next_]; }
            else { in.values[0] = 0; in.values[1] = first_[next_]; }     // fx-CG50: U.List con Lower 0
        } else in.values[0] = first_[next_];
        try {
            out_.push_back(compute(k_, in).main);
        } catch (const MathError& e) {
            // L'elemento fuori dominio e' indicato nel messaggio (fx-CG50: errore con il dato).
            throw MathError(std::string(e.what()) + " (elemento " + std::to_string(next_ + 1) + ")");
        }
        ++next_;
        if (!budget.consume()) return false;
    }
    return true;
}
std::vector<double> computeList(Kind k, const Input& base, const std::vector<double>& first, const std::vector<double>& second) {
    ListTask t(k, base, first, second);
    step::Budget unlimited;
    while (!t.step(unlimited)) {}
    return t.results();
}

// ---------------------------------------------------------------- grafico
double density(const GraphModel& g, double x) noexcept {
    try {
        switch (g.family) {
        case Family::Normal: return calclib::normPD(x, g.p2, g.p1);
        case Family::T: return calclib::tPD(x, g.p1);
        case Family::Chi: return x <= 0 ? 0 : calclib::chiPD(x, g.p1);
        case Family::F: return x <= 0 ? 0 : calclib::fPD(x, g.p1, g.p2);
        default: return 0;
        }
    } catch (...) { return 0; }
}
namespace {
// Finestra della curva: centro e ampiezza tipici (window), allargati per
// includere i punti notevoli (expand), poi l'altezza dal picco (finishFrame).
void window(GraphModel& g) {
    switch (g.family) {
    case Family::Normal: g.xmin = g.p1 - 3.5 * g.p2; g.xmax = g.p1 + 3.5 * g.p2; break;
    case Family::T: g.xmin = -4; g.xmax = 4; break;
    case Family::Chi: g.xmin = 0; g.xmax = g.p1 + 4 * std::sqrt(2 * g.p1) + 2; break;
    case Family::F: g.xmin = 0; g.xmax = 5; break;
    default: break;
    }
}
void expand(GraphModel& g, double mark) {
    if (!std::isfinite(mark)) return;
    const double span = g.xmax - g.xmin, lim = 8 * span;
    if (mark < g.xmin && g.family != Family::Chi && g.family != Family::F) g.xmin = std::max(mark - 0.1 * span, g.xmin - lim);
    if (mark > g.xmax) g.xmax = std::min(mark + 0.1 * span, g.xmax + lim);
}
void finishFrame(GraphModel& g) {
    double peak = 0;
    for (int i = 0; i <= 200; ++i) {
        const double x = g.xmin + (g.xmax - g.xmin) * i / 200.0;
        const double d = density(g, x);
        if (std::isfinite(d)) peak = std::max(peak, d);
    }
    if (!(peak > 0)) peak = 1;
    // chi^2 e F con df piccoli tendono all'infinito vicino a 0: altezza limitata.
    if (g.family == Family::Chi || g.family == Family::F) peak = std::min(peak, 1.2);
    g.ymax = peak * 1.15;
}
GraphModel base(Family f, double p1, double p2) {
    GraphModel g; g.available = continuous(f); g.family = f; g.p1 = p1; g.p2 = p2;
    return g;
}
} // namespace
GraphModel graphModel(Kind k, const Input& in, const Result& r) {
    if (!continuous(k.family)) return {};
    const std::vector<double>& v = in.values;
    const std::size_t a = argCount(k);
    GraphModel g;
    switch (k.family) {
    case Family::Normal: g = base(k.family, v[a + 1], v[a]); break;          // mu, sigma
    case Family::T: case Family::Chi: g = base(k.family, v[a], 0); break;
    case Family::F: g = base(k.family, v[a], v[a + 1]); break;
    default: return {};
    }
    double mark = std::numeric_limits<double>::quiet_NaN();
    if (k.func == Func::PD) { mark = v[0]; g.hasMark = true; g.markX = v[0]; g.markY = r.main; }
    else if (k.func == Func::CD) { g.areas.emplace_back(bound(v[0]), bound(v[1])); mark = std::isfinite(bound(v[1])) ? v[1] : v[0]; }
    else if (hasTail(k) && in.tail == Tail::Central) { g.areas.emplace_back(r.rows[0].second, r.rows[1].second); mark = r.rows[1].second; }
    else if (hasTail(k) && in.tail == Tail::Left) { g.areas.emplace_back(-kInf, r.main); mark = r.main; }
    else { g.areas.emplace_back(r.main, kInf); mark = r.main; }
    window(g);
    expand(g, mark);
    if (k.func == Func::CD) expand(g, bound(v[0]));
    finishFrame(g);
    return g;
}
GraphModel testGraph(const stats::Outcome& o) {
    GraphModel g;
    switch (o.curve) {
    case stats::Curve::Normal: g = base(Family::Normal, 0, 1); break;
    case stats::Curve::T: g = base(Family::T, o.df1, 0); break;
    case stats::Curve::Chi: g = base(Family::Chi, o.df1, 0); break;
    case stats::Curve::F: g = base(Family::F, o.df1, o.df2); break;
    default: return {};
    }
    const double s = o.stat;
    const bool symmetric = o.curve == stats::Curve::Normal || o.curve == stats::Curve::T;
    if (o.tail < 0) g.areas.emplace_back(-kInf, s);
    else if (o.tail > 0) g.areas.emplace_back(s, kInf);
    else if (symmetric) { g.areas.emplace_back(-kInf, -std::abs(s)); g.areas.emplace_back(std::abs(s), kInf); }
    else {
        // chi^2/F a due code: l'area dalla parte della statistica.
        const double lower = o.curve == stats::Curve::F ? stats::fLowerTail(s, o.df1, o.df2) : 1 - stats::chiUpperTail(s, o.df1);
        if (lower < 0.5) g.areas.emplace_back(-kInf, s); else g.areas.emplace_back(s, kInf);
    }
    g.hasMark = true; g.markX = s; g.markY = density(g, s);
    window(g);
    expand(g, symmetric ? std::abs(s) : s);
    if (symmetric) { expand(g, -std::abs(s)); const double m = std::max(-g.xmin, g.xmax); g.xmin = -m; g.xmax = m; }
    finishFrame(g);
    return g;
}
} // namespace fx::dist
