// EQUATION: simultaneous linear equations (2-6 unknowns), polynomial equations
// (degree 2-6, complex roots), SOLVER (numerical f(x)=g(x)) and polynomial
// inequalities (degree 1-6).
#include "apps.hpp"
#include "solvers.hpp"
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
const char* kModeNames[] = {"Simultaneous (sistemi lineari)", "Polynomial (polinomi)", "Solver (f(x)=g(x))", "Inequality (disequazioni)"};
const char* kRel[] = {">", "<", "\xE2\x89\xA5", "\xE2\x89\xA4"};
InequalityRelation relation(std::uint8_t r) {
    switch (r) { case 0: return InequalityRelation::Greater; case 1: return InequalityRelation::Less; case 2: return InequalityRelation::GreaterEqual; default: return InequalityRelation::LessEqual; }
}
std::string letter(int i) { return std::string(1, static_cast<char>('a' + i)); }
} // namespace

void EquationApp::enter(Context&) { screen_ = Screen::Menu; }

void EquationApp::openGrid(Context& ctx) {
    EquationState& e = ctx.ws.equation;
    grid_.row = 0; grid_.col = 0; grid_.growRows = false; grid_.rowMajor = true;
    if (mode_ == Mode::Simultaneous) {
        const int n = e.unknowns;
        grid_.rows = n; grid_.cols = n + 1; grid_.visibleCols = std::min(n + 1, 5);
        grid_.header = [n](int c) { return c < n ? letter(c) + "n" : std::string("rhs"); };
        grid_.get = [&e, n](int r, int c, double& v) { v = e.simultaneous[static_cast<std::size_t>(r * (n + 1) + c)]; return true; };
        grid_.set = [&e, n, &ctx](int r, int c, double v) { e.simultaneous[static_cast<std::size_t>(r * (n + 1) + c)] = v; ctx.markDirty(); };
        grid_.erase = [&e, n, &ctx](int r, int c) { e.simultaneous[static_cast<std::size_t>(r * (n + 1) + c)] = 0; ctx.markDirty(); };
    } else {
        const bool ineq = mode_ == Mode::Inequality;
        const int deg = ineq ? e.ineqDegree : e.degree;
        auto* coeffs = ineq ? &e.inequality : &e.polynomial;
        grid_.rows = 1; grid_.cols = deg + 1; grid_.visibleCols = std::min(deg + 1, 5);
        grid_.header = [](int c) { return letter(c); };
        grid_.get = [coeffs](int, int c, double& v) { v = (*coeffs)[static_cast<std::size_t>(c)]; return true; };
        grid_.set = [coeffs, &ctx](int, int c, double v) { (*coeffs)[static_cast<std::size_t>(c)] = v; ctx.markDirty(); };
        grid_.erase = [coeffs, &ctx](int, int c) { (*coeffs)[static_cast<std::size_t>(c)] = 0; ctx.markDirty(); };
    }
    screen_ = Screen::Grid;
}

void EquationApp::solve(Context& ctx) {
    EquationState& e = ctx.ws.equation;
    const DisplayFormat f = ctx.ws.settings.display;
    results_.clear();
    if (mode_ == Mode::Simultaneous) {
        const int n = e.unknowns;
        std::vector<std::vector<double>> A(static_cast<std::size_t>(n), std::vector<double>(static_cast<std::size_t>(n)));
        std::vector<double> b(static_cast<std::size_t>(n));
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n; ++c) A[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] = e.simultaneous[static_cast<std::size_t>(r * (n + 1) + c)];
            b[static_cast<std::size_t>(r)] = e.simultaneous[static_cast<std::size_t>(r * (n + 1) + n)];
        }
        const auto res = solveLinearSystem(A, b, 1e-12);
        results_.title = "Sistema " + std::to_string(n) + "x" + std::to_string(n);
        if (res.kind == LinearResult::Inconsistent) results_.add("Risultato", "Nessuna soluzione");
        else if (res.kind == LinearResult::Dependent) results_.add("Risultato", "Infinite soluzioni");
        else for (int i = 0; i < n; ++i) results_.add("x" + std::to_string(i + 1) + " =", formatReal(res.solution[static_cast<std::size_t>(i)], f));
        results_.add("rango", std::to_string(res.rank));
    } else if (mode_ == Mode::Polynomial) {
        std::vector<double> c(e.polynomial.begin(), e.polynomial.begin() + e.degree + 1);
        if (c[0] == 0) throw MathError("Math ERROR: il coefficiente a deve essere diverso da 0");
        const auto roots = polynomialRoots(c);
        results_.title = "Grado " + std::to_string(e.degree) + ": " + std::to_string(roots.size()) + " radici";
        for (std::size_t i = 0; i < roots.size(); ++i) results_.add("x" + std::to_string(i + 1) + " =", formatValue(roots[i], ComplexMode::Rectangular, f));
    } else if (mode_ == Mode::Inequality) {
        std::vector<double> c(e.inequality.begin(), e.inequality.begin() + e.ineqDegree + 1);
        if (c[0] == 0) throw MathError("Math ERROR: il coefficiente a deve essere diverso da 0");
        const auto set = polynomialInequality(c, relation(e.ineqRelation));
        results_.title = std::string("P(x) ") + kRel[e.ineqRelation] + " 0";
        std::string lines[8];
        const std::size_t n = ui::wrapText(formatSolutionSet(set, f), 26, lines, 8);
        for (std::size_t i = 0; i < n; ++i) results_.add(i == 0 ? "Soluzione" : "", lines[i]);
        for (std::size_t i = 0; i < set.size() && i < 6; ++i) {
            const auto& s = set[i];
            const std::string lo = std::isinf(s.lower) ? "-\xE2\x88\x9E" : formatReal(s.lower, f);
            const std::string hi = std::isinf(s.upper) ? "+\xE2\x88\x9E" : formatReal(s.upper, f);
            results_.add("I" + std::to_string(i + 1), std::string(s.lowerInclusive ? "[" : "(") + lo + ", " + hi + (s.upperInclusive ? "]" : ")"));
        }
    } else {
        e.solverExpr = solverForm_.fields[0].text;
        e.solverGuess = solverForm_.number(ctx, 1);
        const auto r = solveEquation(e.solverExpr, e.solverGuess, &ctx.ws.memory);
        ctx.ws.memory.store('X', r.x);
        results_.title = e.solverExpr;
        results_.add("X =", formatReal(r.x, f));
        results_.add("Lft =", formatReal(r.left, f));
        results_.add("Rgt =", formatReal(r.right, f));
        results_.add("", "X memorizzato nella variabile X");
    }
    ctx.markDirty();
    screen_ = Screen::Result;
}

void EquationApp::key(Context& ctx, const KeyEvent& ev) {
    EquationState& e = ctx.ws.equation;
    switch (screen_) {
    case Screen::Menu:
        if (ev.is(Key::Up)) menuSel_ = (menuSel_ + 3) % 4;
        else if (ev.is(Key::Down)) menuSel_ = (menuSel_ + 1) % 4;
        else if (ev.is(Key::Exit)) ctx.home();
        else {
            int pick = -1;
            if (ev.is(Key::Exe)) pick = menuSel_;
            else if (ev.is(Key::F1)) pick = 0; else if (ev.is(Key::F2)) pick = 1; else if (ev.is(Key::F3)) pick = 2; else if (ev.is(Key::F4)) pick = 3;
            if (pick < 0) return;
            menuSel_ = pick;
            mode_ = static_cast<Mode>(pick);
            if (mode_ == Mode::Solver) {
                solverForm_.fields = {{"Equazione", e.solverExpr, FormField::Type::Text, {}, 0},
                                      {"X iniziale", formatReal(e.solverGuess, {DisplayMode::Norm2, 0}), FormField::Type::Number, {}, 0}};
                solverForm_.selected = 0;
                screen_ = Screen::Solver;
            } else screen_ = Screen::ChooseSize;
        }
        return;
    case Screen::ChooseSize: {
        if (ev.is(Key::Exit)) { screen_ = Screen::Menu; return; }
        int k = -1;
        if (ev.is(Key::F1)) k = 0; else if (ev.is(Key::F2)) k = 1; else if (ev.is(Key::F3)) k = 2; else if (ev.is(Key::F4)) k = 3; else if (ev.is(Key::F5)) k = 4; else if (ev.is(Key::F6)) k = 5;
        if (ev.is(Key::Char) && ev.ch >= '1' && ev.ch <= '6') k = ev.ch - '1' - (mode_ == Mode::Inequality ? 0 : 1);
        if (k < 0) return;
        if (mode_ == Mode::Simultaneous && k <= 4) e.unknowns = static_cast<std::uint8_t>(2 + k);
        else if (mode_ == Mode::Polynomial && k <= 4) e.degree = static_cast<std::uint8_t>(2 + k);
        else if (mode_ == Mode::Inequality) e.ineqDegree = static_cast<std::uint8_t>(1 + k);
        else return;
        openGrid(ctx);
        return;
    }
    case Screen::Grid:
        if (grid_.editing()) { (void)grid_.key(ctx, ev); return; }
        if (ev.is(Key::F1)) { solve(ctx); return; }
        if (ev.is(Key::F2)) { if (grid_.erase) grid_.erase(grid_.row, grid_.col); return; }
        if (ev.is(Key::F3)) {
            if (mode_ == Mode::Simultaneous) e.simultaneous.fill(0); else if (mode_ == Mode::Polynomial) e.polynomial.fill(0); else e.inequality.fill(0);
            ctx.markDirty(); return;
        }
        if (mode_ == Mode::Inequality && ev.is(Key::F4)) { e.ineqRelation = static_cast<std::uint8_t>((e.ineqRelation + 1) % 4); ctx.markDirty(); return; }
        if (ev.is(Key::Exit)) { screen_ = Screen::ChooseSize; return; }
        (void)grid_.key(ctx, ev);
        return;
    case Screen::Solver:
        if (solverForm_.editing()) { (void)solverForm_.key(ctx, ev); return; }
        if (ev.is(Key::F1) || ev.is(Key::F6)) { solve(ctx); return; }
        if (ev.is(Key::Exit)) { screen_ = Screen::Menu; return; }
        (void)solverForm_.key(ctx, ev);
        return;
    case Screen::Result:
        if (ev.is(Key::Exit) || ev.is(Key::F1) || ev.is(Key::Exe)) { screen_ = mode_ == Mode::Solver ? Screen::Solver : Screen::Grid; return; }
        (void)results_.key(ev);
        return;
    }
}

void EquationApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "EQUATION");
    const EquationState& e = ctx.ws.equation;
    const int top = ui::ContentTop + 2, bottom = ui::ContentBottom;
    switch (screen_) {
    case Screen::Menu: {
        for (int i = 0; i < 4; ++i) {
            char label[64]; std::snprintf(label, sizeof(label), "F%d: %s", i + 1, kModeNames[i]);
            ui::listRow(c, top + 8 + i * 26, 24, label, nullptr, i == menuSel_);
        }
        const char* keys[6] = {"SIML", "POLY", "SOLVER", "INEQ", nullptr, nullptr};
        ui::softKeys(c, keys);
        return;
    }
    case Screen::ChooseSize: {
        const char* what = mode_ == Mode::Simultaneous ? "Numero di incognite?" : mode_ == Mode::Polynomial ? "Grado del polinomio?" : "Grado della disequazione?";
        ui::textCenter(c, ui::ScreenW / 2, top + 40, what, Ink);
        ui::textCenter(c, ui::ScreenW / 2, top + 70, kModeNames[static_cast<int>(mode_)], Muted, font::Face::Small);
        if (mode_ == Mode::Inequality) { const char* k[6] = {"1", "2", "3", "4", "5", "6"}; ui::softKeys(c, k); }
        else { const char* k[6] = {"2", "3", "4", "5", "6", nullptr}; ui::softKeys(c, k); }
        return;
    }
    case Screen::Grid: {
        std::string title;
        if (mode_ == Mode::Simultaneous) title = "an\xC2\xB7x1+bn\xC2\xB7x2+... = rhs";
        else {
            const int deg = mode_ == Mode::Inequality ? e.ineqDegree : e.degree;
            for (int i = 0; i <= deg; ++i) {
                if (i) title += "+";
                title += letter(i);
                if (deg - i >= 1) title += "x";
                if (deg - i >= 2) { static const char* sup[] = {"", "", "\xC2\xB2", "\xC2\xB3", "\xE2\x81\xB4", "\xE2\x81\xB5", "\xE2\x81\xB6"}; title += sup[deg - i]; }
            }
            title += mode_ == Mode::Inequality ? std::string(" ") + kRel[e.ineqRelation] + " 0" : std::string("=0");
        }
        c.rect(0, top, ui::ScreenW, 20, SelectSoft, true);
        ui::textFit(c, 6, top + 2, title, ui::ScreenW - 12, Ink);
        grid_.draw(ctx, c, top + 22, bottom);
        if (mode_ == Mode::Inequality) { const char* k[6] = {"SOLVE", "DEL", "CLEAR", "REL", nullptr, nullptr}; ui::softKeys(c, k); }
        else { const char* k[6] = {"SOLVE", "DEL", "CLEAR", nullptr, nullptr, nullptr}; ui::softKeys(c, k); }
        return;
    }
    case Screen::Solver: {
        ui::text(c, 6, top + 4, "Equazione in X (es. x^2=2, cos(x)=x)", Muted, font::Face::Small);
        solverForm_.draw(c, top + 22, bottom - 4);
        const char* k[6] = {"SOLVE", nullptr, nullptr, nullptr, nullptr, "SOLVE"};
        ui::softKeys(c, k);
        return;
    }
    case Screen::Result: {
        results_.draw(c, top, bottom);
        const char* k[6] = {"REPEAT", nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, k);
        return;
    }
    }
}
} // namespace fx::app
