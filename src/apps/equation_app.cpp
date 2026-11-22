// EQUATION (fx-CG50, manuale 4-1...4-5, 1-29): schermata "Select Type",
// SIML (sistemi lineari con 2-6 incognite), POLY (equazioni di grado 2-6),
// SOLVER (qualsiasi variabile, tabella delle variabili, Lower/Upper, Lft/Rgt)
// e l'estensione INEQ (disequazioni polinomiali di grado 1-6, non presente
// sulla fx-CG50). I coefficienti sono espressioni dello stesso parser di
// Run-Matrix conservate in forma esatta; le soluzioni si mostrano con gli
// stessi modelli 2D di Run-Matrix. Il calcolo e' in equation.hpp.
#include "apps.hpp"
#include "equation.hpp"
#include "math_renderer.hpp"
#include "run_matrix.hpp"
#include "solvers.hpp"
#include <algorithm>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
const char* const kRel[] = {">", "<", "\xE2\x89\xA5", "\xE2\x89\xA4"};
InequalityRelation relation(std::uint8_t r) {
    switch (r) { case 0: return InequalityRelation::Greater; case 1: return InequalityRelation::Less; case 2: return InequalityRelation::GreaterEqual; default: return InequalityRelation::LessEqual; }
}
constexpr std::uint16_t kTitleInk = gfx::rgb888To565(0, 70, 200);        // intestazioni blu (manuale 4-2)
constexpr std::uint16_t kMultiplicity = gfx::rgb888To565(200, 0, 160);   // "×3" magenta (manuale 4-3)
const char* const kSup[] = {"", "", "\xC2\xB2", "\xC2\xB3", "\xE2\x81\xB4", "\xE2\x81\xB5", "\xE2\x81\xB6"};
// Nomi delle incognite: X Y (Z) fino a 3, X1...X6 oltre (manuale 4-2; scelta per 4-6).
const char* const* unknownNames(int n) {
    static const char* const small[] = {"X", "Y", "Z"};
    static const char* const large[] = {"X1", "X2", "X3", "X4", "X5", "X6"};
    return n <= 3 ? small : large;
}
const char* const* rootNames() {
    static const char* const names[] = {"X1", "X2", "X3", "X4", "X5", "X6"};
    return names;
}
std::string letter(int i) { return std::string(1, static_cast<char>('a' + i)); }
// Testo con pedici: "a_nX" -> a, n piccola in basso, X.
int drawSubscripted(gfx::Canvas& c, int x, int y, const std::string& s, std::uint16_t ink) {
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '_' && i + 1 < s.size()) {
            x += ui::text(c, x, y + 6, s.substr(i + 1, 1).c_str(), ink, font::Face::Small);
            i += 2; continue;
        }
        const unsigned char ch = static_cast<unsigned char>(s[i]);
        const std::size_t len = ch < 0x80 ? 1 : (ch & 0xE0) == 0xC0 ? 2 : (ch & 0xF0) == 0xE0 ? 3 : 4;
        x += ui::text(c, x, y, s.substr(i, len).c_str(), ink);
        i += len;
    }
    return x;
}
// Disegna un albero 2D; restituisce la larghezza.
int drawTree(gfx::Canvas& c, const ast::Node& tree, int x, int baseline, int maxWidth, std::uint16_t ink, math::Align align = math::Align::Left) {
    const math::Layout l = math::layout(tree, x, baseline, maxWidth, 120, align);
    math::draw(c, l, ink, ink, Placeholder);
    return l.size.width;
}
ast::NodePtr labelled(const std::string& label, const ast::NodePtr& value) {
    auto row = ast::make(ast::Kind::Row);
    appendTextTokens(*row, label);
    for (auto& ch : value->children) row->children.push_back(ch);
    return row;
}
// Albero di un'equazione del Solver (testo lineare) come la mostra l'editor.
ast::NodePtr equationTree(const std::string& text, bool linear) {
    if (linear) { auto r = ast::make(ast::Kind::Row); appendTextTokens(*r, text); return r; }
    NaturalEditor ed;
    try { ed.setExpression(text); } catch (const std::exception&) { auto r = ast::make(ast::Kind::Row); appendTextTokens(*r, text); return r; }
    return cloneTree(ed.root());
}
std::string cut(const std::string& s, std::size_t glyphs) {
    if (font::glyphCount(s.c_str()) <= glyphs) return s;
    std::string lines[1];
    (void)ui::wrapText(s, glyphs > 1 ? glyphs - 1 : 1, lines, 1);
    return lines[0] + "\xE2\x80\xA6";
}
} // namespace

// ---------------------------------------------------------------- stato
void EquationApp::enter(Context& ctx) {
    editing_ = false;
    // v1.16.0: strisce Solver, Simul Equation e Poly Equation di eActivity.
    if (ctx.inStrip(AppId::Equation) && !ctx.resuming()) {
        const std::uint8_t e = ctx.strip().entry;
        if (e == stripentry::Solver) { chooseMode(ctx, Mode::Solver); return; }
        if (e == stripentry::SimulEquation || e == stripentry::PolyEquation) {
            mode_ = e == stripentry::SimulEquation ? Mode::Simultaneous : Mode::Polynomial;
            menuSel_ = static_cast<int>(mode_);
            openCoefficients(ctx);
            return;
        }
    }
    if (!ctx.resuming()) { screen_ = Screen::Menu; return; }
    // Ritorno dal SET UP: stessa schermata, con le nuove impostazioni.
    if (screen_ == Screen::SolverResult) screen_ = Screen::SolverTable;
    if (screen_ == Screen::Solutions) {
        try { solve(ctx); } catch (const std::exception&) { screen_ = Screen::Coefficients; }
    }
    if (screen_ == Screen::SolverTable) refreshVariables(ctx);
}
void EquationApp::leave(Context&) { editing_ = false; }

std::size_t EquationApp::solutionRows() const noexcept {
    if (mode_ == Mode::Simultaneous) return simul_.kind == eq::SimulResult::Kind::None ? 0 : static_cast<std::size_t>(simul_.unknowns);
    if (mode_ == Mode::Polynomial) return shownRoots_.size();
    return ineqResults_.rows.size();
}
eq::View EquationApp::view(const Context& ctx, std::size_t row) const {
    const Settings& s = ctx.ws.settings;
    eq::View v;
    v.complex = s.complex; v.display = s.display; v.mixed = s.mixedFractions;
    v.natural = s.inputMode == Settings::InputMode::Math; v.fractionResults = s.fractionResults; v.angle = s.angle;
    v.mode = row < views_.size() ? views_[row] : 0;
    return v;
}
std::string EquationApp::solutionText(std::size_t i, const Context& ctx) const {
    const eq::View v = view(ctx, i);
    if (mode_ == Mode::Simultaneous) {
        if (i >= static_cast<std::size_t>(simul_.unknowns) || simul_.kind == eq::SimulResult::Kind::None) return {};
        return eq::formulaText(simul_, static_cast<int>(i), unknownNames(simul_.unknowns), v);
    }
    if (mode_ == Mode::Polynomial) {
        if (i >= shownRoots_.size()) return {};
        std::string t = std::string(rootNames()[i]) + "=" + eq::numberText(shownRoots_[i].value, v);
        if (shownRoots_[i].multiplicity > 1) t += " \xC3\x97" + std::to_string(shownRoots_[i].multiplicity);
        return t;
    }
    return i < ineqResults_.rows.size() ? ineqResults_.rows[i].first + " " + ineqResults_.rows[i].second : std::string();
}
std::string EquationApp::selectedText(const Context& ctx) const {
    const std::size_t i = static_cast<std::size_t>(solRow_);
    const eq::View v = view(ctx, i);
    if (mode_ == Mode::Simultaneous && simul_.kind == eq::SimulResult::Kind::Unique && i < simul_.solution.size()) return eq::numberText(simul_.solution[i], v);
    if (mode_ == Mode::Polynomial && i < shownRoots_.size()) return eq::numberText(shownRoots_[i].value, v);
    return solutionText(i, ctx);
}

int EquationApp::gridRows(const Context& ctx) const { return mode_ == Mode::Simultaneous ? ctx.ws.equation.unknowns : 1; }
int EquationApp::gridCols(const Context& ctx) const {
    const EquationState& e = ctx.ws.equation;
    if (mode_ == Mode::Simultaneous) return e.unknowns + 1;
    return (mode_ == Mode::Inequality ? e.ineqDegree : e.degree) + 1;
}
eq::Number& EquationApp::cell(Context& ctx, int r, int c) const {
    EquationState& e = ctx.ws.equation;
    if (mode_ == Mode::Simultaneous) return e.sim(static_cast<std::size_t>(r), static_cast<std::size_t>(c));
    return (mode_ == Mode::Inequality ? e.inequality : e.polynomial)[static_cast<std::size_t>(c)];
}
// Un coefficiente cambiato rende obsolete le soluzioni memorizzate (VARS › SimRes, PlyRes).
void EquationApp::clearResults(Context& ctx) {
    if (mode_ == Mode::Simultaneous) ctx.ws.equation.simResult = CalcValue{};
    else if (mode_ == Mode::Polynomial) ctx.ws.equation.polyResult = CalcValue{};
    ctx.markDirty();
}

// ---------------------------------------------------------------- menu e dimensioni
void EquationApp::chooseMode(Context& ctx, Mode m) {
    mode_ = m;
    menuSel_ = static_cast<int>(m);
    editing_ = false;
    if (m == Mode::Solver) openSolver(ctx);
    else screen_ = Screen::ChooseSize;
}
void EquationApp::openCoefficients(Context&) {
    row_ = col_ = firstCol_ = firstRow_ = 0;
    editing_ = false;
    screen_ = Screen::Coefficients;
}

void EquationApp::key(Context& ctx, const KeyEvent& ev) {
    EquationState& e = ctx.ws.equation;
    switch (screen_) {
    case Screen::Menu: {
        if (ev.is(Key::Up)) { menuSel_ = (menuSel_ + 3) % 4; return; }
        if (ev.is(Key::Down)) { menuSel_ = (menuSel_ + 1) % 4; return; }
        if (ev.is(Key::Exit)) { ctx.home(); return; }
        int pick = -1;
        if (ev.is(Key::Exe)) pick = menuSel_;
        else if (ev.is(Key::F1)) pick = 0; else if (ev.is(Key::F2)) pick = 1; else if (ev.is(Key::F3)) pick = 2; else if (ev.is(Key::F4)) pick = 3;
        else if (ev.is(Key::Char) && ev.ch >= '1' && ev.ch <= '4') pick = ev.ch - '1';
        if (pick >= 0) chooseMode(ctx, static_cast<Mode>(pick));
        return;
    }
    case Screen::ChooseSize: {
        if (ev.is(Key::Exit)) { screen_ = Screen::Menu; return; }
        int k = -1;
        if (ev.is(Key::F1)) k = 0; else if (ev.is(Key::F2)) k = 1; else if (ev.is(Key::F3)) k = 2; else if (ev.is(Key::F4)) k = 3; else if (ev.is(Key::F5)) k = 4; else if (ev.is(Key::F6)) k = 5;
        const bool ineq = mode_ == Mode::Inequality;
        if (ev.is(Key::Char) && ev.ch >= '1' && ev.ch <= '6') k = ev.ch - '1' - (ineq ? 0 : 1);
        if (k < 0) return;
        if (mode_ == Mode::Simultaneous && k <= 4) { if (e.unknowns != 2 + k) e.simResult = CalcValue{}; e.unknowns = static_cast<std::uint8_t>(2 + k); }
        else if (mode_ == Mode::Polynomial && k <= 4) { if (e.degree != 2 + k) e.polyResult = CalcValue{}; e.degree = static_cast<std::uint8_t>(2 + k); }
        else if (ineq && k <= 5) e.ineqDegree = static_cast<std::uint8_t>(1 + k);
        else return;
        ctx.markDirty();
        openCoefficients(ctx);
        return;
    }
    case Screen::Coefficients: coefficientsKey(ctx, ev); return;
    case Screen::Solutions: solutionsKey(ctx, ev); return;
    case Screen::SolverTable: solverKey(ctx, ev); return;
    case Screen::SolverResult:
        if (ev.is(Key::F1) || ev.is(Key::Exit) || ev.is(Key::Exe)) screen_ = Screen::SolverTable;   // REPEAT
        return;
    }
}

// ---------------------------------------------------------------- coefficienti
// Valore digitato: espressione dello stesso parser (frazioni, radici, π, A-Z,
// Ans, i), memorizzata con la forma esatta. In caso di errore il cursore va
// sulla causa e l'immissione resta aperta, come in Run-Matrix.
void EquationApp::commitCell(Context& ctx) {
    const std::string text = field_.text();
    auto advance = [&] {
        // a1 -> b1 -> c1 -> ... -> an -> bn -> cn (manuale 4-1)
        if (col_ + 1 < gridCols(ctx)) ++col_;
        else if (row_ + 1 < gridRows(ctx)) { col_ = 0; ++row_; }
    };
    if (text.empty()) { editing_ = false; advance(); return; }     // EXE senza input: il valore resta
    ctx.ws.apply();
    eq::Number n;
    try {
        n = eq::evaluateNumber(text, ctx.ws.evalContext());
    } catch (const SyntaxError& ex) { field_.markError(ex.where); throw; }
    catch (const MathError& ex) { field_.markError(ex.where); throw; }
    if (mode_ == Mode::Inequality && !n.isReal()) throw MathError("Ma ERROR: le disequazioni hanno coefficienti reali");
    cell(ctx, row_, col_) = n;
    clearResults(ctx);
    editing_ = false;
    advance();
}

void EquationApp::coefficientsKey(Context& ctx, const KeyEvent& ev) {
    if (editing_) {
        if (field_.menuOpen()) { (void)field_.key(ctx, ev); return; }
        if (ev.is(Key::Exe)) { commitCell(ctx); return; }
        if (ev.is(Key::Exit)) { editing_ = false; return; }     // il coefficiente torna com'era (manuale 4-1)
        (void)field_.key(ctx, ev);
        return;
    }
    const int rows = gridRows(ctx), cols = gridCols(ctx);
    switch (ev.key) {
    case Key::Up: row_ = std::max(0, row_ - 1); return;
    case Key::Down: row_ = std::min(rows - 1, row_ + 1); return;
    case Key::Left: col_ = std::max(0, col_ - 1); return;
    case Key::Right: col_ = std::min(cols - 1, col_ + 1); return;
    case Key::Home: col_ = 0; return;
    case Key::End: col_ = cols - 1; return;
    case Key::F1: solve(ctx); return;                                      // SOLVE
    case Key::F2: case Key::Del: case Key::DelForward:                    // DELETE
        cell(ctx, row_, col_) = eq::Number{}; clearResults(ctx); return;
    case Key::F3: {                                                        // CLEAR: tutti i coefficienti a zero
        EquationState& e = ctx.ws.equation;
        auto& v = mode_ == Mode::Simultaneous ? e.simultaneous : mode_ == Mode::Polynomial ? e.polynomial : e.inequality;
        for (eq::Number& n : v) n = eq::Number{};
        row_ = col_ = 0;                                                   // si riparte da a1
        clearResults(ctx);
        return;
    }
    case Key::F4:                                                          // EDIT
        field_.start(ctx, cell(ctx, row_, col_).isZero() ? std::string() : eq::editText(cell(ctx, row_, col_)));
        editing_ = true;
        return;
    case Key::Exe:                                                         // il valore resta, cella successiva
        if (col_ + 1 < cols) ++col_;
        else if (row_ + 1 < rows) { col_ = 0; ++row_; }
        return;
    case Key::F5:
        if (mode_ == Mode::Inequality) { EquationState& e = ctx.ws.equation; e.ineqRelation = static_cast<std::uint8_t>((e.ineqRelation + 1) % 4); ctx.markDirty(); }
        return;
    case Key::Exit: screen_ = Screen::ChooseSize; return;
    case Key::Char:
        if (ev.ctrl) return;
        field_.start(ctx, std::string());
        editing_ = true;
        (void)field_.key(ctx, ev);
        return;
    default: return;
    }
}

void EquationApp::solve(Context& ctx) {
    EquationState& e = ctx.ws.equation;
    views_.clear(); solRow_ = 0; solFirst_ = 0; scroll_ = 0;
    message_.clear();
    if (mode_ == Mode::Simultaneous) {
        simul_ = eq::solveSimultaneous(e.simCoefficients(), e.unknowns);
        const int n = e.unknowns;
        if (simul_.kind == eq::SimulResult::Kind::None) message_ = "No Solution";
        else if (simul_.kind == eq::SimulResult::Kind::Infinite) message_ = "Infinitely Many Solutions";
        // VARS › SimRes: soluzioni, o la Rref senza soluzione unica (manuale 1-33).
        e.simResult = simul_.kind == eq::SimulResult::Kind::Unique ? eq::toMatrix(simul_.solution, static_cast<std::size_t>(n), 1)
                                                                   : eq::toMatrix(simul_.rref, static_cast<std::size_t>(n), static_cast<std::size_t>(n + 1));
        views_.assign(static_cast<std::size_t>(n), 0);
    } else if (mode_ == Mode::Polynomial) {
        const auto all = eq::solvePolynomial(e.polyCoefficients());
        shownRoots_ = eq::visibleRoots(all, ctx.ws.settings.complex);
        if (shownRoots_.empty()) { message_ = "No Solution"; e.polyResult = CalcValue{}; }
        else {
            std::vector<eq::Number> values;
            for (const eq::Root& r : shownRoots_) values.push_back(r.value);
            e.polyResult = eq::toMatrix(values, values.size(), 1);
        }
        views_.assign(shownRoots_.size(), 0);
    } else {
        std::vector<double> c;
        for (std::size_t i = 0; i <= e.ineqDegree; ++i) {
            const eq::Number& n = e.inequality[i];
            if (!n.isReal()) throw MathError("Ma ERROR: le disequazioni hanno coefficienti reali");
            c.push_back(n.value.real());
        }
        if (c[0] == 0) throw MathError("Ma ERROR: il coefficiente a deve essere diverso da 0");
        const auto set = polynomialInequality(c, relation(e.ineqRelation));
        const DisplayFormat f = ctx.ws.settings.display;
        ineqResults_.clear();
        ineqResults_.title = std::string("P(X) ") + kRel[e.ineqRelation] + " 0";
        std::string lines[8];
        const std::size_t n = ui::wrapText(formatSolutionSet(set, f), 26, lines, 8);
        for (std::size_t i = 0; i < n; ++i) ineqResults_.add(i == 0 ? "Soluzione" : "", lines[i]);
        for (std::size_t i = 0; i < set.size() && i < 6; ++i) {
            const auto& s = set[i];
            const std::string lo = std::isinf(s.lower) ? "-\xE2\x88\x9E" : formatReal(s.lower, f);
            const std::string hi = std::isinf(s.upper) ? "+\xE2\x88\x9E" : formatReal(s.upper, f);
            ineqResults_.add("I" + std::to_string(i + 1), std::string(s.lowerInclusive ? "[" : "(") + lo + ", " + hi + (s.upperInclusive ? "]" : ")"));
        }
    }
    ctx.markDirty();
    screen_ = Screen::Solutions;
}

// ---------------------------------------------------------------- soluzioni
void EquationApp::solutionsKey(Context& ctx, const KeyEvent& ev) {
    if (ev.is(Key::F1) || ev.is(Key::Exit)) { screen_ = Screen::Coefficients; return; }   // REPEAT
    if (mode_ == Mode::Inequality) { (void)ineqResults_.key(ev); return; }
    const int n = static_cast<int>(solutionRows());
    switch (ev.key) {
    case Key::Up: if (solRow_ > 0) { --solRow_; scroll_ = 0; } return;
    case Key::Down: if (solRow_ + 1 < n) { ++solRow_; scroll_ = 0; } return;
    case Key::Left: scroll_ = std::max(0, scroll_ - 48); return;
    case Key::Right: scroll_ = std::min(overflow_, scroll_ + 48); return;
    case Key::F5: {                                                        // S⇔D
        if (solRow_ >= n || static_cast<std::size_t>(solRow_) >= views_.size()) return;
        std::uint8_t& m = views_[static_cast<std::size_t>(solRow_)];
        const eq::View v = view(ctx, static_cast<std::size_t>(solRow_));
        bool exactShown = false, other = false;
        auto probe = [&](const eq::Number& x) { exactShown = exactShown || eq::showsExact(x, v); other = other || eq::hasOtherForm(x, v); };
        if (mode_ == Mode::Polynomial) probe(shownRoots_[static_cast<std::size_t>(solRow_)].value);
        else if (simul_.kind == eq::SimulResult::Kind::Unique) probe(simul_.solution[static_cast<std::size_t>(solRow_)]);
        else {
            const std::size_t F = simul_.freeVars.size(), stride = 1 + F;
            for (std::size_t j = 0; j < stride; ++j) probe(simul_.formula[static_cast<std::size_t>(solRow_) * stride + j]);
        }
        if (m != 0) m = 0;
        else if (exactShown) m = 1;
        else if (other) m = 2;
        else ctx.toast("Nessuna altra forma: valore decimale o intero");
        scroll_ = 0;
        return;
    }
    default: return;
    }
}

// ---------------------------------------------------------------- Solver
void EquationApp::refreshVariables(Context& ctx) {
    try { vars_ = eq::solverVariables(ctx.ws.equation.solverExpr); } catch (const std::exception&) { vars_.clear(); }
}
void EquationApp::openSolver(Context& ctx) {
    screen_ = Screen::SolverTable;
    refreshVariables(ctx);
    solverRow_ = 0; solverFirst_ = 0;
    const EquationState& e = ctx.ws.equation;
    // Si riparte dalla variabile risolta l'ultima volta (fx-CG50: la tabella resta com'era).
    for (std::size_t i = 0; i < vars_.size(); ++i) if (vars_[i] == e.solverVariable) solverRow_ = static_cast<int>(i) + 1;
    editing_ = e.solverExpr.empty();
    if (editing_) field_.start(ctx, std::string());
}
void EquationApp::commitSolverRow(Context& ctx) {
    EquationState& e = ctx.ws.equation;
    const std::string text = field_.text();
    const int nv = static_cast<int>(vars_.size());
    if (solverRow_ == 0) {
        if (text.empty()) { e.solverExpr.clear(); vars_.clear(); ctx.markDirty(); return; }
        if (text.size() > limits::kEquationExpression) throw MathError("Memory ERROR: equazione troppo lunga");
        std::vector<char> found;
        try { found = eq::solverVariables(text); }
        catch (const SyntaxError& ex) { field_.markError(ex.where); throw; }
        e.solverExpr = text;
        vars_ = found;
        editing_ = false;
        solverRow_ = vars_.empty() ? 0 : 1;      // la tabella delle variabili (manuale 4-5)
        ctx.markDirty();
        return;
    }
    if (text.empty()) { editing_ = false; return; }
    ctx.ws.apply();
    EvalContext ec = ctx.ws.evalContext();
    ec.complexMode = ComplexMode::Real;
    double value = 0;
    try { value = evaluateReal(*parseExpression(text), ec); }
    catch (const SyntaxError& ex) { field_.markError(ex.where); throw; }
    catch (const MathError& ex) { field_.markError(ex.where); throw; }
    if (!std::isfinite(value)) throw MathError("Ma ERROR: valore non finito");
    if (solverRow_ <= nv) {
        const char v = vars_[static_cast<std::size_t>(solverRow_ - 1)];
        const eq::Number n = eq::evaluateNumber(text, ec);
        if (n.hasExact && n.exact.isReal()) ctx.ws.memory.storeExact(v, value, n.exact.re, n.decimal);
        else ctx.ws.memory.store(v, value);
    } else if (solverRow_ == nv + 1) e.solverLower = value;
    else e.solverUpper = value;
    editing_ = false;
    ctx.markDirty();
    if (solverRow_ + 1 < solverRows()) ++solverRow_;
}
void EquationApp::solveSolver(Context& ctx) {
    EquationState& e = ctx.ws.equation;
    if (e.solverExpr.empty()) throw SyntaxError("Syntax ERROR: manca l'equazione");
    refreshVariables(ctx);
    if (vars_.empty()) {
        (void)eq::solverVariables(e.solverExpr);           // errore di sintassi, se c'e'
        throw MathError("No Variable: l'equazione non contiene variabili");
    }
    // La variabile evidenziata (manuale 4-5); sulle altre righe l'ultima risolta o la prima.
    char v = 0;
    if (solverRow_ >= 1 && solverRow_ <= static_cast<int>(vars_.size())) v = vars_[static_cast<std::size_t>(solverRow_ - 1)];
    else if (std::find(vars_.begin(), vars_.end(), e.solverVariable) != vars_.end()) v = e.solverVariable;
    else v = vars_.front();
    for (int k = 0; k < 26; ++k) { const char name = static_cast<char>('A' + k); recall_[static_cast<std::size_t>(k)] = ctx.ws.memory.recall(name); recallUsed_[static_cast<std::size_t>(k)] = ctx.ws.memory.used(name); }
    ctx.ws.apply();
    outcome_ = eq::solveFor(e.solverExpr, v, e.solverLower, e.solverUpper, ctx.ws.evalContext());
    // La variabile risolta resta memorizzata (manuale 4-5: V=16.8 nella tabella).
    ctx.ws.memory.store(v, outcome_.value);
    e.solverVariable = v;
    solvedVar_ = v;
    recallValid_ = true;
    ctx.markDirty();
    screen_ = Screen::SolverResult;
}
void EquationApp::solverKey(Context& ctx, const KeyEvent& ev) {
    EquationState& e = ctx.ws.equation;
    if (editing_) {
        if (field_.menuOpen()) { (void)field_.key(ctx, ev); return; }
        if (ev.is(Key::Exe)) { commitSolverRow(ctx); return; }
        if (ev.is(Key::Exit)) {
            editing_ = false;
            if (e.solverExpr.empty() && solverRow_ == 0) screen_ = Screen::Menu;
            return;
        }
        (void)field_.key(ctx, ev);
        return;
    }
    const int rows = solverRows(), nv = static_cast<int>(vars_.size());
    switch (ev.key) {
    case Key::Up: solverRow_ = std::max(0, solverRow_ - 1); return;
    case Key::Down: solverRow_ = std::min(rows - 1, solverRow_ + 1); return;
    case Key::F6: solveSolver(ctx); return;                                // SOLVE
    case Key::F1:                                                          // RECALL
        if (!recallValid_) { ctx.toast("Nessun valore da richiamare"); return; }
        for (int k = 0; k < 26; ++k) {
            const char name = static_cast<char>('A' + k);
            if (recallUsed_[static_cast<std::size_t>(k)]) ctx.ws.memory.store(name, recall_[static_cast<std::size_t>(k)]);
            else ctx.ws.memory.clear(name);
        }
        recallValid_ = false;
        ctx.markDirty();
        ctx.toast("Valori prima dell'ultimo SOLVE");
        return;
    case Key::F2: case Key::Del: case Key::DelForward:                    // DELETE
        if (solverRow_ == 0) { e.solverExpr.clear(); vars_.clear(); field_.start(ctx, std::string()); editing_ = true; }
        else if (solverRow_ <= nv) ctx.ws.memory.store(vars_[static_cast<std::size_t>(solverRow_ - 1)], 0);
        else if (solverRow_ == nv + 1) e.solverLower = EquationState::DefaultLower;
        else e.solverUpper = EquationState::DefaultUpper;
        ctx.markDirty();
        return;
    case Key::Exe: case Key::F4: {                                         // modifica della riga
        std::string start;
        char buf[40];
        if (solverRow_ == 0) start = e.solverExpr;
        else if (solverRow_ <= nv) {
            const char v = vars_[static_cast<std::size_t>(solverRow_ - 1)];
            const ExactSlot& slot = ctx.ws.memory.exactSlot(v);
            if (slot.valid && !slot.decimal) start = exact::toExpression(slot.value);
            else { std::snprintf(buf, sizeof(buf), "%.15g", ctx.ws.memory.recall(v)); start = buf; }
        } else { std::snprintf(buf, sizeof(buf), "%.15g", solverRow_ == nv + 1 ? e.solverLower : e.solverUpper); start = buf; }
        field_.start(ctx, start);
        editing_ = true;
        return;
    }
    case Key::Exit: screen_ = Screen::Menu; return;
    case Key::Char:
        if (ev.ctrl) return;
        field_.start(ctx, std::string());
        editing_ = true;
        (void)field_.key(ctx, ev);
        return;
    default: return;
    }
}

// ---------------------------------------------------------------- disegno
void EquationApp::drawHeader(Context& ctx, gfx::Canvas& c, int y) const {
    const EquationState& e = ctx.ws.equation;
    std::string t;
    if (mode_ == Mode::Simultaneous) {
        // a_nX+b_nY=c_n (manuale 4-2); X1...X6 da 4 incognite.
        const int n = e.unknowns;
        const char* const* names = unknownNames(n);
        for (int i = 0; i < n; ++i) {
            if (n == 6 && i >= 2 && i < n - 1) { if (i == 2) t += "+\xE2\x80\xA6"; continue; }
            if (i) t += "+";
            t += letter(i) + "_n" + names[i];
        }
        t += "=" + letter(n) + "_n";
    } else {
        const int deg = mode_ == Mode::Inequality ? e.ineqDegree : e.degree;
        for (int i = 0; i <= deg; ++i) {
            if (i) t += "+";
            t += letter(i);
            if (deg - i >= 1) t += "X";
            if (deg - i >= 2) t += kSup[deg - i];
        }
        t += mode_ == Mode::Inequality ? std::string(kRel[e.ineqRelation]) + "0" : std::string("=0");
    }
    drawSubscripted(c, 4, y, t, kTitleInk);
    c.hLine(0, y + 19, ui::ScreenW, Faint);
}

void EquationApp::drawBottomValue(gfx::Canvas& c, const ast::Node& tree, int baseline) {
    const math::Size sz = math::measure(tree);
    const int area = ui::ScreenW - 12;
    overflow_ = std::max(0, sz.width - area);
    scroll_ = std::min(scroll_, overflow_);
    if (overflow_ == 0) { drawTree(c, tree, 6, baseline, area, Ink, math::Align::Right); return; }
    // Valore piu' largo dello schermo: scorre con ◀ ▶.
    const math::Layout l = math::layout(tree, 6 - scroll_, baseline, area + scroll_, 120);
    c.setClip({6, baseline - 60, area, 120});
    math::draw(c, l, Ink, Ink, Placeholder);
    c.resetClip();
    if (scroll_ > 0) ui::text(c, 0, baseline - 8, "\xE2\x97\x80", Accent, font::Face::Small);
    if (scroll_ < overflow_) ui::textRight(c, ui::ScreenW, baseline - 8, "\xE2\x96\xB6", Accent, font::Face::Small);
}

void EquationApp::drawCoefficients(Context& ctx, gfx::Canvas& c) {
    const int top = ui::ContentTop + 2;
    drawHeader(ctx, c, top);
    const int rows = gridRows(ctx), cols = gridCols(ctx);
    const int labelW = 22, visibleCols = std::min(cols, 4);
    const int colW = (ui::ScreenW - labelW - 12) / visibleCols;
    if (col_ < firstCol_) firstCol_ = col_;
    if (col_ >= firstCol_ + visibleCols) firstCol_ = col_ - visibleCols + 1;
    const int headY = top + 24, rowH = 22, gridTop = headY + 16;
    for (int k = 0; k < visibleCols; ++k) {
        const int x = labelW + 4 + k * colW;
        ui::textCenter(c, x + colW / 2, headY, letter(firstCol_ + k).c_str(), Ink, font::Face::Small);
        c.hLine(x + 4, headY + 13, colW - 8, Ink);
    }
    if (firstCol_ > 0) ui::text(c, labelW - 6, headY, "\xE2\x97\x80", Accent, font::Face::Small);
    if (firstCol_ + visibleCols < cols) ui::textRight(c, ui::ScreenW - 1, headY, "\xE2\x96\xB6", Accent, font::Face::Small);
    // Parentesi di matrice e numeri di riga (manuale 4-2).
    const int left = labelW + 1, right = labelW + 6 + visibleCols * colW;
    const int bottom = gridTop + rows * rowH;
    c.vLine(left, gridTop, rows * rowH, Ink); c.hLine(left, gridTop, 4, Ink); c.hLine(left, bottom - 1, 4, Ink);
    c.vLine(right, gridTop, rows * rowH, Ink); c.hLine(right - 3, gridTop, 4, Ink); c.hLine(right - 3, bottom - 1, 4, Ink);
    eq::View cellView = view(ctx, 0);
    cellView.mode = 1;                                    // celle: decimali (manuale 4-2, 1-29)
    for (int r = 0; r < rows; ++r) {
        const int y = gridTop + r * rowH;
        if (mode_ == Mode::Simultaneous) { char lbl[12]; std::snprintf(lbl, sizeof(lbl), "%d", r + 1); ui::textRight(c, labelW - 3, y + 4, lbl, Ink, font::Face::Small); }
        for (int k = 0; k < visibleCols; ++k) {
            const int cc = firstCol_ + k, x = labelW + 4 + k * colW;
            const bool sel = r == row_ && cc == col_;
            if (sel) c.rect(x + 1, y + 1, colW - 2, rowH - 2, editing_ ? SelectSoft : Select, true);
            const std::string s = cut(eq::numberText(cell(ctx, r, cc), cellView), static_cast<std::size_t>((colW - 6) / 6));
            ui::textRight(c, x + colW - 4, y + 5, s.c_str(), sel && !editing_ ? SelectInk : Ink, font::Face::Small);
        }
    }
    // Valore della cella corrente in forma naturale (in basso a destra), o l'immissione.
    const int lineY = ui::ContentBottom - 46;
    c.hLine(0, lineY, ui::ScreenW, Faint);
    if (editing_) {
        field_.draw(c, 10, ui::ContentBottom - 18, ui::ScreenW - 20, 40);
    } else {
        const auto tree = eq::numberTree(cell(ctx, row_, col_), view(ctx, 0));
        scroll_ = 0;
        drawBottomValue(c, *tree, ui::ContentBottom - 16);
    }
    if (editing_) ui::softKeys(c, MathField::softKeys(ctx.shift()));
    else if (mode_ == Mode::Inequality) { const char* k[6] = {"SOLVE", "DELETE", "CLEAR", "EDIT", "REL", nullptr}; ui::softKeys(c, k); }
    else { const char* k[6] = {"SOLVE", "DELETE", "CLEAR", "EDIT", nullptr, nullptr}; ui::softKeys(c, k); }
    if (editing_) field_.drawMenu(c);
}

void EquationApp::drawSolutions(Context& ctx, gfx::Canvas& c) {
    const int top = ui::ContentTop + 2;
    const char* keys[6] = {"REPEAT", nullptr, nullptr, nullptr, "S\xE2\x87\x94" "D", nullptr};
    if (mode_ == Mode::Inequality) {
        ineqResults_.draw(c, top, ui::ContentBottom);
        keys[4] = nullptr;
        ui::softKeys(c, keys);
        return;
    }
    drawHeader(ctx, c, top);
    int y = top + 26;
    const int rowH = 22;
    if (!message_.empty() && (mode_ == Mode::Polynomial || simul_.kind == eq::SimulResult::Kind::None)) {
        ui::textCenter(c, ui::ScreenW / 2, top + 100, message_.c_str(), Ink);
        keys[4] = nullptr;
        ui::softKeys(c, keys);
        return;
    }
    const int n = static_cast<int>(solutionRows());
    if (simul_.kind == eq::SimulResult::Kind::Infinite && mode_ == Mode::Simultaneous) {
        // "Infinitely Many Solutions" con la formula di ogni incognita (manuale 4-2).
        ui::text(c, 8, y, "Infinitely", Ink);
        ui::text(c, 8, y + 18, "Many Solutions", Ink);
        y += 42;
        const int visible = std::max(1, (ui::ContentBottom - 52 - y) / 26);
        if (solRow_ < solFirst_) solFirst_ = solRow_;
        if (solRow_ >= solFirst_ + visible) solFirst_ = solRow_ - visible + 1;
        for (int i = 0; i < visible && solFirst_ + i < n; ++i) {
            const int k = solFirst_ + i, ry = y + i * 26;
            const bool sel = k == solRow_;
            if (sel) c.rect(4, ry, ui::ScreenW - 8, 24, Select, true);
            const auto tree = eq::formulaTree(simul_, k, unknownNames(simul_.unknowns), view(ctx, static_cast<std::size_t>(k)));
            drawTree(c, *tree, 10, ry + 17, ui::ScreenW - 20, sel ? SelectInk : Ink);
        }
        c.hLine(0, ui::ContentBottom - 46, ui::ScreenW, Faint);
        const auto tree = eq::formulaTree(simul_, solRow_, unknownNames(simul_.unknowns), view(ctx, static_cast<std::size_t>(solRow_)));
        drawBottomValue(c, *tree, ui::ContentBottom - 16);
        ui::softKeys(c, keys);
        return;
    }
    // Soluzioni in colonna tra parentesi, cella evidenziata (manuale 4-2, 4-3).
    const char* const* names = mode_ == Mode::Simultaneous ? unknownNames(simul_.unknowns) : rootNames();
    const int left = 36, cellW = 200;
    c.vLine(left, y, n * rowH, Ink); c.hLine(left, y, 4, Ink); c.hLine(left, y + n * rowH - 1, 4, Ink);
    c.vLine(left + cellW + 4, y, n * rowH, Ink); c.hLine(left + cellW + 1, y, 4, Ink); c.hLine(left + cellW + 1, y + n * rowH - 1, 4, Ink);
    for (int i = 0; i < n; ++i) {
        const int ry = y + i * rowH;
        const bool sel = i == solRow_;
        ui::text(c, 6, ry + 3, names[i], Ink);
        if (sel) c.rect(left + 3, ry + 1, cellW - 2, rowH - 2, Select, true);
        const eq::Number& value = mode_ == Mode::Simultaneous ? simul_.solution[static_cast<std::size_t>(i)] : shownRoots_[static_cast<std::size_t>(i)].value;
        eq::View cellView = view(ctx, static_cast<std::size_t>(i));
        cellView.mode = 1;                                // la cella mostra il decimale (manuale 1-29)
        const std::string s = cut(eq::numberText(value, cellView), static_cast<std::size_t>((cellW - 10) / 8));
        ui::textRight(c, left + cellW - 3, ry + 3, s.c_str(), sel ? SelectInk : Ink);
        if (mode_ == Mode::Polynomial && shownRoots_[static_cast<std::size_t>(i)].multiplicity > 1) {
            const std::string m = "\xC3\x97" + std::to_string(shownRoots_[static_cast<std::size_t>(i)].multiplicity);
            ui::text(c, left + cellW + 10, ry + 3, m.c_str(), kMultiplicity);
        }
    }
    c.hLine(0, ui::ContentBottom - 46, ui::ScreenW, Faint);
    const eq::Number& selected = mode_ == Mode::Simultaneous ? simul_.solution[static_cast<std::size_t>(solRow_)] : shownRoots_[static_cast<std::size_t>(solRow_)].value;
    const auto tree = eq::numberTree(selected, view(ctx, static_cast<std::size_t>(solRow_)));
    drawBottomValue(c, *tree, ui::ContentBottom - 16);
    ui::softKeys(c, keys);
}

void EquationApp::drawSolver(Context& ctx, gfx::Canvas& c) {
    const EquationState& e = ctx.ws.equation;
    const int top = ui::ContentTop + 4;
    const bool linear = ctx.ws.settings.inputMode == Settings::InputMode::Line;
    const int rows = solverRows(), nv = static_cast<int>(vars_.size());
    const int eqH = 30, rowH = 22;
    const int listBottom = ui::ContentBottom - (editing_ ? 50 : 4);
    const int visible = std::max(1, (listBottom - top - eqH) / rowH + 1);
    if (solverRow_ < solverFirst_) solverFirst_ = solverRow_;
    if (solverRow_ >= solverFirst_ + visible) solverFirst_ = solverRow_ - visible + 1;
    int y = top;
    for (int r = solverFirst_; r < rows && y < listBottom; ++r) {
        const int h = r == 0 ? eqH : rowH;
        const bool sel = r == solverRow_;
        if (sel) c.rect(0, y, ui::ScreenW, h, editing_ ? SelectSoft : Select, true);
        const std::uint16_t ink = sel && !editing_ ? SelectInk : Ink;
        const int baseline = y + h - 7;
        if (r == 0) {
            ui::text(c, 4, baseline - 15, "Eq:", ink);
            if (!e.solverExpr.empty()) drawTree(c, *equationTree(e.solverExpr, linear), 34, baseline, ui::ScreenW - 40, ink);
        } else {
            std::string label;
            double value = 0;
            ast::NodePtr tree;
            if (r <= nv) {
                const char v = vars_[static_cast<std::size_t>(r - 1)];
                label = std::string(1, v) + "=";
                const ExactSlot& slot = ctx.ws.memory.exactSlot(v);
                eq::View vw = view(ctx, 0);
                const eq::Number n = slot.valid ? eq::Number::fromExact(eq::ExactComplex::real(slot.value), slot.decimal)
                                                : eq::Number::fromComplex({ctx.ws.memory.recall(v), 0});
                tree = labelled(label, eq::numberTree(n, vw));
            } else {
                label = r == nv + 1 ? "Lower=" : "Upper=";
                value = r == nv + 1 ? e.solverLower : e.solverUpper;
                auto row = ast::make(ast::Kind::Row);
                appendTextTokens(*row, label);
                if (linear) appendTextTokens(*row, formatReal(value, ctx.ws.settings.display));
                else appendNumberTree(*row, value, ctx.ws.settings.display);
                tree = row;
            }
            drawTree(c, *tree, r <= nv ? 30 : 14, baseline, ui::ScreenW - 36, ink);
        }
        y += h;
    }
    if (editing_) {
        c.hLine(0, ui::ContentBottom - 46, ui::ScreenW, Faint);
        field_.draw(c, 10, ui::ContentBottom - 18, ui::ScreenW - 20, 40);
        ui::softKeys(c, MathField::softKeys(ctx.shift()));
        field_.drawMenu(c);
        return;
    }
    const char* k[6] = {"RECALL", "DELETE", nullptr, nullptr, nullptr, "SOLVE"};
    ui::softKeys(c, k);
}

void EquationApp::drawSolverResult(Context& ctx, gfx::Canvas& c) {
    const EquationState& e = ctx.ws.equation;
    const bool linear = ctx.ws.settings.inputMode == Settings::InputMode::Line;
    const DisplayFormat f = ctx.ws.settings.display;
    int y = ui::ContentTop + 26;
    ui::text(c, 4, y - 15, "Eq:", kTitleInk);
    drawTree(c, *equationTree(e.solverExpr, linear), 34, y, ui::ScreenW - 40, kTitleInk);
    y += 30;
    auto line = [&](const std::string& label, double value, int x) {
        auto row = ast::make(ast::Kind::Row);
        appendTextTokens(*row, label);
        if (linear) appendTextTokens(*row, formatReal(value, f)); else appendNumberTree(*row, value, f);
        drawTree(c, *row, x, y, ui::ScreenW - x - 4, Ink);
        y += 24;
    };
    // "V=16.8", "Lft=14", "Rgt=14" (manuale 4-5).
    line(std::string(1, solvedVar_) + "=", outcome_.value, 26);
    line("Lft=", outcome_.left, 8);
    line("Rgt=", outcome_.right, 8);
    if (outcome_.retry) ui::text(c, 8, y - 10, "Retry", Error);
    const char* k[6] = {"REPEAT", nullptr, nullptr, nullptr, nullptr, nullptr};
    ui::softKeys(c, k);
}

void EquationApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "EQUATION");
    const int top = ui::ContentTop + 2;
    switch (screen_) {
    case Screen::Menu: {
        // "Equation / Select Type / F1:Simultaneous / F2:Polynomial / F3:Solver" (manuale 4-1).
        ui::text(c, 8, top + 6, "Equation", kTitleInk);
        ui::text(c, 8, top + 40, "Select Type", Ink);
        static const char* const items[4] = {"F1:Simultaneous", "F2:Polynomial", "F3:Solver", "F4:Inequality"};
        for (int i = 0; i < 4; ++i) ui::listRow(c, top + 62 + i * 24, 22, items[i], i == 3 ? "estensione" : nullptr, i == menuSel_, 4, ui::ScreenW - 8);
        const char* keys[6] = {"SIMUL", "POLY", "SOLVER", "INEQ", nullptr, nullptr};
        ui::softKeys(c, keys);
        return;
    }
    case Screen::ChooseSize: {
        const char* what = mode_ == Mode::Simultaneous ? "Number of Unknowns?" : "Degree?";
        ui::text(c, 8, top + 6, mode_ == Mode::Simultaneous ? "Simultaneous" : mode_ == Mode::Polynomial ? "Polynomial" : "Inequality (estensione)", kTitleInk);
        ui::text(c, 8, top + 40, what, Ink);
        if (mode_ == Mode::Inequality) { const char* k[6] = {"1", "2", "3", "4", "5", "6"}; ui::softKeys(c, k); }
        else { const char* k[6] = {"2", "3", "4", "5", "6", nullptr}; ui::softKeys(c, k); }
        return;
    }
    case Screen::Coefficients: drawCoefficients(ctx, c); return;
    case Screen::Solutions: drawSolutions(ctx, c); return;
    case Screen::SolverTable: drawSolver(ctx, c); return;
    case Screen::SolverResult: drawSolverResult(ctx, c); return;
    }
}
} // namespace fx::app
