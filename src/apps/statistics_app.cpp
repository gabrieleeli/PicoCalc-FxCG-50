// STATISTICS: list editor (List 1-6), 1-Var / 2-Var / regressions, statistical
// graphs (scatter, xyLine, histogram, box plot, regression curve), DIST
// calculators and Z/t tests and intervals.
#include "apps.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace fx::app {
using namespace ui::theme;
using stats::RegType;

namespace {
const std::vector<std::string> kGraphMenu = {"Scatter (List1, List2)", "xyLine (List1, List2)", "Histogram (List1)", "Box plot (List1)"};
const std::vector<std::string> kCalcMenu = {"1-Var (List1)", "2-Var (List1, List2)", "Reg ax+b", "Reg Med-Med", "Reg X\xC2\xB2", "Reg X\xC2\xB3", "Reg X\xE2\x81\xB4", "Reg Log", "Reg Exp a\xC2\xB7" "e^bx", "Reg ab^x", "Reg Power"};
const std::vector<std::string> kDistMenu = {"Normal PD", "Normal CD", "Inverse Normal", "Binomial PD", "Binomial CD", "Poisson PD", "Poisson CD", "Student-t CD", "\xCF\x87\xC2\xB2 CD", "F CD"};
const std::vector<std::string> kTestMenu = {"1-Sample Z Test (List1)", "1-Sample t Test (List1)", "1-Sample Z Interval (List1)", "1-Sample t Interval (List1)"};
FormField num(const char* label, const char* value) { return {label, value, FormField::Type::Number, {}, 0}; }
FormField tail() { return {"\xCE\xBC", "\xE2\x89\xA0", FormField::Type::Choice, {"\xE2\x89\xA0", "<", ">"}, 0}; }
} // namespace

void StatisticsApp::enter(Context& ctx) { setupGrid(ctx); screen_ = Screen::Lists; menu_.close(); }

void StatisticsApp::setupGrid(Context& ctx) {
    auto& lists = ctx.ws.lists;
    grid_.cols = static_cast<int>(Workspace::ListCount);
    grid_.visibleCols = 3;
    grid_.growRows = true;
    std::size_t longest = 0; for (const auto& l : lists) longest = std::max(longest, l.size());
    grid_.rows = static_cast<int>(std::max<std::size_t>(1, longest));
    grid_.header = [](int c) { return "List " + std::to_string(c + 1); };
    grid_.columnLength = [&lists](int c) { return static_cast<int>(lists[static_cast<std::size_t>(c)].size()); };
    grid_.get = [&lists](int r, int c, double& v) { const auto& l = lists[static_cast<std::size_t>(c)]; if (static_cast<std::size_t>(r) >= l.size()) return false; v = l[static_cast<std::size_t>(r)]; return true; };
    grid_.set = [&lists, &ctx](int r, int c, double v) {
        auto& l = lists[static_cast<std::size_t>(c)];
        if (static_cast<std::size_t>(r) >= Workspace::MaxListLength) throw MathError("Dimension ERROR: massimo 255 elementi");
        if (static_cast<std::size_t>(r) >= l.size()) l.resize(static_cast<std::size_t>(r) + 1, 0.0);
        l[static_cast<std::size_t>(r)] = v; ctx.markDirty();
    };
    grid_.erase = [&lists, &ctx](int r, int c) {
        auto& l = lists[static_cast<std::size_t>(c)];
        if (static_cast<std::size_t>(r) < l.size()) { l.erase(l.begin() + r); ctx.markDirty(); }
    };
}

void StatisticsApp::runCalc(Context& ctx, int choice) {
    const auto& L = ctx.ws.lists;
    const DisplayFormat f = ctx.ws.settings.display;
    auto F = [&](double v) { return formatReal(v, f); };
    results_.clear(); hasReg_ = false;
    if (choice == 0) {
        const auto o = stats::oneVariable(L[0]);
        results_.title = "1-Variable (List1)";
        results_.add("x\xCC\x84", F(o.mean)); results_.add("\xCE\xA3x", F(o.sum)); results_.add("\xCE\xA3x\xC2\xB2", F(o.sumSq));
        results_.add("\xCF\x83x", F(o.popSd)); results_.add("sx", F(o.sampleSd)); results_.add("n", F(o.n));
        results_.add("minX", F(o.min)); results_.add("Q1", F(o.q1)); results_.add("Med", F(o.median)); results_.add("Q3", F(o.q3)); results_.add("maxX", F(o.max));
        std::string modes; for (std::size_t i = 0; i < o.modes.size() && i < 4; ++i) { if (i) modes += ", "; modes += F(o.modes[i]); }
        if (o.modes.size() > 4) modes += ", ...";
        results_.add("Mod", modes); results_.add("Mod:F", F(o.modeCount));
    } else if (choice == 1) {
        const auto t = stats::twoVariable(L[0], L[1]);
        results_.title = "2-Variable (List1, List2)";
        results_.add("x\xCC\x84", F(t.meanX)); results_.add("\xCE\xA3x", F(t.sumX)); results_.add("\xCE\xA3x\xC2\xB2", F(t.sumX2)); results_.add("\xCF\x83x", F(t.popSdX)); results_.add("sx", F(t.sampleSdX)); results_.add("n", F(t.n));
        results_.add("y\xCC\x84", F(t.meanY)); results_.add("\xCE\xA3y", F(t.sumY)); results_.add("\xCE\xA3y\xC2\xB2", F(t.sumY2)); results_.add("\xCF\x83y", F(t.popSdY)); results_.add("sy", F(t.sampleSdY));
        results_.add("\xCE\xA3xy", F(t.sumXY)); results_.add("minX", F(t.minX)); results_.add("maxX", F(t.maxX)); results_.add("minY", F(t.minY)); results_.add("maxY", F(t.maxY));
    } else {
        const auto type = static_cast<RegType>(choice - 2);
        reg_ = stats::regress(type, L[0], L[1]);
        hasReg_ = true;
        results_.title = std::string("Regressione ") + stats::regFormula(type);
        static const char* names[] = {"a", "b", "c", "d", "e"};
        for (std::size_t i = 0; i < reg_.coef.size() && i < 5; ++i) results_.add(names[i], F(reg_.coef[i]));
        if (reg_.hasR) results_.add("r", F(reg_.r));
        results_.add("r\xC2\xB2", F(reg_.r2));
        results_.add("MSe", F(reg_.mse));
    }
    screen_ = Screen::Results;
}

void StatisticsApp::openDist(int k) {
    formKind_ = 100 + k;
    switch (k) {
    case 0: form_.fields = {num("x", "0"), num("\xCF\x83", "1"), num("\xCE\xBC", "0")}; break;
    case 1: form_.fields = {num("Lower", "-1"), num("Upper", "1"), num("\xCF\x83", "1"), num("\xCE\xBC", "0")}; break;
    case 2: form_.fields = {num("Area", "0.95"), num("\xCF\x83", "1"), num("\xCE\xBC", "0")}; break;
    case 3: case 4: form_.fields = {num("x", "2"), num("Numtrial n", "10"), num("p", "0.5")}; break;
    case 5: case 6: form_.fields = {num("x", "2"), num("\xCE\xBB", "1")}; break;
    case 7: form_.fields = {num("Lower", "-1"), num("Upper", "1"), num("df", "10")}; break;
    case 8: form_.fields = {num("Lower", "0"), num("Upper", "3.841"), num("df", "1")}; break;
    default: form_.fields = {num("Lower", "0"), num("Upper", "4.96"), num("n:df", "1"), num("d:df", "10")}; break;
    }
    form_.selected = 0; results_.clear(); screen_ = Screen::Form;
}
void StatisticsApp::openTest(int k) {
    formKind_ = 200 + k;
    switch (k) {
    case 0: form_.fields = {tail(), num("\xCE\xBC" "0", "0"), num("\xCF\x83", "1")}; break;
    case 1: form_.fields = {tail(), num("\xCE\xBC" "0", "0")}; break;
    case 2: form_.fields = {num("C-Level", "0.95"), num("\xCF\x83", "1")}; break;
    default: form_.fields = {num("C-Level", "0.95")}; break;
    }
    form_.selected = 0; results_.clear(); screen_ = Screen::Form;
}
void StatisticsApp::computeForm(Context& ctx) {
    auto n = [&](std::size_t i) { return form_.number(ctx, i); };
    const DisplayFormat f = ctx.ws.settings.display;
    auto F = [&](double v) { return formatReal(v, f); };
    const double inf = std::numeric_limits<double>::infinity();
    auto bound = [&](double v) { return std::abs(v) >= 1e99 ? (v < 0 ? -inf : inf) : v; };
    results_.clear();
    double p = 0;
    switch (formKind_) {
    case 100: p = stats::normalPd(n(0), n(1), n(2)); results_.add("p", F(p)); break;
    case 101: p = stats::normalCd(bound(n(0)), bound(n(1)), n(2), n(3)); results_.add("p", F(p)); results_.add("z:Low", F((n(0) - n(3)) / n(2))); results_.add("z:Up", F((n(1) - n(3)) / n(2))); break;
    case 102: results_.add("x", F(stats::inverseNormal(n(0), n(1), n(2)))); break;
    case 103: results_.add("p", F(stats::binomialPd(n(0), n(1), n(2)))); break;
    case 104: results_.add("p", F(stats::binomialCd(n(0), n(1), n(2)))); break;
    case 105: results_.add("p", F(stats::poissonPd(n(0), n(1)))); break;
    case 106: results_.add("p", F(stats::poissonCd(n(0), n(1)))); break;
    case 107: results_.add("p", F(stats::tCd(bound(n(0)), bound(n(1)), n(2)))); break;
    case 108: results_.add("p", F(stats::chiSquareCd(bound(n(0)), bound(n(1)), n(2)))); break;
    case 109: results_.add("p", F(stats::fCd(bound(n(0)), bound(n(1)), n(2), n(3)))); break;
    case 200: case 201: {
        const int t = form_.fields[0].choice == 0 ? 0 : form_.fields[0].choice == 1 ? -1 : 1;
        const auto r = formKind_ == 200 ? stats::zTest1(ctx.ws.lists[0], n(1), n(2), t) : stats::tTest1(ctx.ws.lists[0], n(1), t);
        results_.add(formKind_ == 200 ? "z" : "t", F(r.statistic)); results_.add("p", F(r.p));
        if (formKind_ == 201) results_.add("df", F(r.df));
        results_.add("x\xCC\x84", F(r.mean)); results_.add("sx", F(r.sd)); results_.add("n", F(r.n));
        break;
    }
    case 202: case 203: {
        const auto iv = formKind_ == 202 ? stats::zInterval(ctx.ws.lists[0], n(1), n(0)) : stats::tInterval(ctx.ws.lists[0], n(0));
        results_.add("Left", F(iv.lower)); results_.add("Right", F(iv.upper)); results_.add("x\xCC\x84", F(iv.mean)); results_.add("n", F(iv.n));
        break;
    }
    default: break;
    }
}

void StatisticsApp::key(Context& ctx, const KeyEvent& ev) {
    if (menu_.open) {
        const int choice = menu_.key(ev);
        if (choice < 0) return;
        if (menuKind_ == 1) {
            graph_ = static_cast<GraphKind>(choice);
            if (ctx.ws.lists[0].empty()) throw MathError("Dimension ERROR: List1 vuota");
            if ((graph_ == GraphKind::Scatter || graph_ == GraphKind::XYLine) && ctx.ws.lists[0].size() != ctx.ws.lists[1].size())
                throw MathError("Dimension ERROR: List1 e List2 di lunghezza diversa");
            screen_ = Screen::Graph;
        } else if (menuKind_ == 2) runCalc(ctx, choice);
        else if (menuKind_ == 3) openDist(choice);
        else if (menuKind_ == 4) openTest(choice);
        return;
    }
    switch (screen_) {
    case Screen::Lists:
        if (grid_.editing()) { (void)grid_.key(ctx, ev); return; }
        switch (ev.key) {
        case Key::F1: menu_.show("GRAPH", kGraphMenu); menuKind_ = 1; return;
        case Key::F2: menu_.show("CALC", kCalcMenu); menuKind_ = 2; return;
        case Key::F3: menu_.show("TEST / INTR", kTestMenu); menuKind_ = 4; return;
        case Key::F4: menu_.show("DIST", kDistMenu); menuKind_ = 3; return;
        case Key::F5: {   // sort ascending (SHIFT: descending)
            auto& l = ctx.ws.lists[static_cast<std::size_t>(grid_.col)];
            if (ctx.shift()) std::sort(l.begin(), l.end(), std::greater<>()); else std::sort(l.begin(), l.end());
            ctx.markDirty(); return;
        }
        case Key::F6: ctx.ws.lists[static_cast<std::size_t>(grid_.col)].clear(); grid_.row = 0; ctx.markDirty(); return;
        case Key::Exit: ctx.home(); return;
        default: break;
        }
        (void)grid_.key(ctx, ev);
        setupGrid(ctx);
        return;
    case Screen::Results:
        if (ev.is(Key::Exit) || ev.is(Key::F6)) { screen_ = formKind_ >= 100 && results_.title.empty() ? Screen::Form : Screen::Lists; return; }
        if (hasReg_ && ev.is(Key::F1)) { graph_ = GraphKind::Regression; screen_ = Screen::Graph; return; }
        if (hasReg_ && ev.is(Key::F2)) {
            // Copy the regression model into the first empty GRAPH function (Y6 if none).
            auto& fns = ctx.ws.functions;
            std::size_t slot = fns.size() - 1;
            for (std::size_t i = 0; i < fns.size(); ++i) if (fns[i].expr.empty()) { slot = i; break; }
            fns[slot] = emptyGraphFunction(slot); fns[slot].expr = reg_.expression();
            ctx.markDirty();
            ctx.toast("Copiato in Y" + std::to_string(slot + 1));
            return;
        }
        (void)results_.key(ev);
        return;
    case Screen::Graph:
        if (ev.is(Key::Exit) || ev.is(Key::F6) || ev.is(Key::Exe)) screen_ = graph_ == GraphKind::Regression ? Screen::Results : Screen::Lists;
        return;
    case Screen::Form:
        if (form_.editing()) { (void)form_.key(ctx, ev); return; }
        if (ev.is(Key::F1) || ev.is(Key::F6)) { computeForm(ctx); return; }
        if (ev.is(Key::Exit)) { screen_ = Screen::Lists; return; }
        (void)form_.key(ctx, ev);
        return;
    }
}

void StatisticsApp::drawGraph(Context& ctx, gfx::Canvas& c) {
    const auto& L = ctx.ws.lists;
    const int left = 52, right = ui::ScreenW - 8, top = ui::ContentTop + 8, bottom = ui::ContentBottom - 26;
    const std::vector<double>& xs = L[0];
    std::vector<double> ys = (graph_ == GraphKind::Scatter || graph_ == GraphKind::XYLine || graph_ == GraphKind::Regression) ? L[1] : std::vector<double>{};
    double xmin = *std::min_element(xs.begin(), xs.end()), xmax = *std::max_element(xs.begin(), xs.end());
    if (xmin == xmax) { xmin -= 1; xmax += 1; }
    double ymin = 0, ymax = 1;
    if (!ys.empty()) { ymin = *std::min_element(ys.begin(), ys.end()); ymax = *std::max_element(ys.begin(), ys.end()); if (ymin == ymax) { ymin -= 1; ymax += 1; } }
    // Histogram bins.
    std::vector<int> bins;
    if (graph_ == GraphKind::Histogram) {
        const int nb = std::clamp(static_cast<int>(std::ceil(std::sqrt(static_cast<double>(xs.size())))), 3, 12);
        bins.assign(static_cast<std::size_t>(nb), 0);
        for (double v : xs) { int b = static_cast<int>((v - xmin) / (xmax - xmin) * nb); bins[static_cast<std::size_t>(std::clamp(b, 0, nb - 1))]++; }
        ymin = 0; ymax = *std::max_element(bins.begin(), bins.end()) * 1.1;
    }
    const double padX = (xmax - xmin) * 0.08, padY = (ymax - ymin) * 0.1;
    if (graph_ != GraphKind::Histogram) { xmin -= padX; xmax += padX; ymin -= padY; ymax += padY; }
    auto sx = [&](double v) { return left + static_cast<int>(std::lround((v - xmin) / (xmax - xmin) * (right - left))); };
    auto sy = [&](double v) { return bottom - static_cast<int>(std::lround((v - ymin) / (ymax - ymin) * (bottom - top))); };
    c.rect(left, top, right - left + 1, bottom - top + 1, Faint, false);
    if (xmin < 0 && xmax > 0) c.vLine(sx(0), top, bottom - top, Muted);
    if (ymin < 0 && ymax > 0) c.hLine(left, sy(0), right - left, Muted);
    const DisplayFormat lf{DisplayMode::Sci, 3};
    ui::text(c, left, bottom + 4, formatReal(xmin, lf).c_str(), Muted, font::Face::Small);
    ui::textRight(c, right, bottom + 4, formatReal(xmax, lf).c_str(), Muted, font::Face::Small);
    if (graph_ != GraphKind::Box) {
        ui::text(c, 2, top, formatReal(ymax, lf).c_str(), Muted, font::Face::Small);
        ui::text(c, 2, bottom - 10, formatReal(ymin, lf).c_str(), Muted, font::Face::Small);
    }
    const std::uint16_t col = Graph[0], col2 = Graph[1];
    switch (graph_) {
    case GraphKind::Scatter: case GraphKind::Regression:
        for (std::size_t i = 0; i < xs.size() && i < ys.size(); ++i) c.rect(sx(xs[i]) - 2, sy(ys[i]) - 2, 5, 5, col, true);
        if (graph_ == GraphKind::Regression && hasReg_) {
            int px = 0, py = 0; bool prev = false;
            for (int x = left; x <= right; ++x) {
                const double xv = xmin + (xmax - xmin) * (x - left) / (right - left);
                const double yv = reg_.predict(xv);
                if (!std::isfinite(yv)) { prev = false; continue; }
                const int y = sy(yv);
                if (prev && std::abs(y - py) < 400) c.line(px, py, x, y, col2);
                px = x; py = y; prev = true;
            }
            ui::textRight(c, right - 4, bottom - 14, stats::regFormula(reg_.type), col2, font::Face::Small);
        }
        break;
    case GraphKind::XYLine: {
        std::vector<std::pair<double, double>> pts;
        for (std::size_t i = 0; i < xs.size() && i < ys.size(); ++i) pts.emplace_back(xs[i], ys[i]);
        for (std::size_t i = 0; i < pts.size(); ++i) {
            c.rect(sx(pts[i].first) - 2, sy(pts[i].second) - 2, 5, 5, col, true);
            if (i) c.line(sx(pts[i - 1].first), sy(pts[i - 1].second), sx(pts[i].first), sy(pts[i].second), col);
        }
        break;
    }
    case GraphKind::Histogram: {
        const int nb = static_cast<int>(bins.size());
        for (int b = 0; b < nb; ++b) {
            const double x0 = xmin + (xmax - xmin) * b / nb, x1 = xmin + (xmax - xmin) * (b + 1) / nb;
            const int X0 = sx(x0), X1 = sx(x1), Y = sy(bins[static_cast<std::size_t>(b)]);
            c.rect(X0, Y, X1 - X0, bottom - Y, SelectSoft, true);
            c.rect(X0, Y, X1 - X0 + 1, bottom - Y + 1, col, false);
        }
        break;
    }
    case GraphKind::Box: {
        const auto o = stats::oneVariable(xs);
        const int yc = (top + bottom) / 2, h = 40;
        c.hLine(sx(o.min), yc, sx(o.q1) - sx(o.min), col);
        c.hLine(sx(o.q3), yc, sx(o.max) - sx(o.q3), col);
        c.vLine(sx(o.min), yc - 10, 21, col); c.vLine(sx(o.max), yc - 10, 21, col);
        c.rect(sx(o.q1), yc - h / 2, sx(o.q3) - sx(o.q1) + 1, h, SelectSoft, true);
        c.rect(sx(o.q1), yc - h / 2, sx(o.q3) - sx(o.q1) + 1, h, col, false);
        c.vLine(sx(o.median), yc - h / 2, h, col2);
        char info[96]; std::snprintf(info, sizeof(info), "Q1=%s Med=%s Q3=%s", formatReal(o.q1, lf).c_str(), formatReal(o.median, lf).c_str(), formatReal(o.q3, lf).c_str());
        ui::text(c, left, top + 4, info, Ink, font::Face::Small);
        break;
    }
    }
}

void StatisticsApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "STATISTICS");
    const int top = ui::ContentTop + 1, bottom = ui::ContentBottom;
    switch (screen_) {
    case Screen::Lists: {
        grid_.draw(ctx, c, top, bottom);
        const bool shift = ctx.shift();
        const char* keys[6] = {"GRAPH", "CALC", "TEST", "DIST", shift ? "SORT\xE2\x86\x93" : "SORT\xE2\x86\x91", "DEL-L"};
        ui::softKeys(c, keys);
        break;
    }
    case Screen::Results: {
        results_.draw(c, top, bottom);
        if (hasReg_) { const char* k[6] = {"DRAW", "COPY\xE2\x86\x92Y", nullptr, nullptr, nullptr, "EXIT"}; ui::softKeys(c, k); }
        else { const char* k[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, "EXIT"}; ui::softKeys(c, k); }
        break;
    }
    case Screen::Graph: {
        drawGraph(ctx, c);
        const char* k[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, "EXIT"};
        ui::softKeys(c, k);
        break;
    }
    case Screen::Form: {
        const std::vector<std::string>& names = formKind_ >= 200 ? kTestMenu : kDistMenu;
        const int idx = formKind_ % 100;
        c.rect(0, top, ui::ScreenW, 20, SelectSoft, true);
        ui::textFit(c, 6, top + 2, names[static_cast<std::size_t>(idx)], ui::ScreenW - 12, Ink);
        const int formBottom = top + 24 + static_cast<int>(form_.fields.size()) * 20;
        form_.draw(c, top + 24, formBottom);
        if (!results_.rows.empty()) { c.hLine(0, formBottom + 4, ui::ScreenW, Faint); results_.draw(c, formBottom + 8, bottom); }
        const char* k[6] = {"CALC", nullptr, nullptr, nullptr, nullptr, "CALC"};
        ui::softKeys(c, k);
        break;
    }
    }
    menu_.draw(c);
}
} // namespace fx::app
