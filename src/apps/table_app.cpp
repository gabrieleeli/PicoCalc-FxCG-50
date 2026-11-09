// TABLE (manual 5-32..5-35): the table relation list (functions shared with
// GRAPH, Y1..Y20), the table range or a list as the variable, number tables of
// the Y=, r= or Param functions (one type per table) with derivative columns,
// editing of the variable column (EDIT, ROW DELETE/INSERT/ADD), DELETE,
// LISTMEM and GPH-CON / GPH-PLT drawn by GRAPH. Every value comes from the
// GRAPH engine (the shared evaluator) through table_model.
#include "apps.hpp"
#include "math_renderer.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
std::uint16_t lineColor(std::uint8_t c) {
    static const std::uint16_t pal[GraphColorCount] = {Graph[0], Graph[1], Graph[2], Graph[3], Graph[4], Graph[5], rgb888To565(225, 180, 0)};
    return pal[c % GraphColorCount];
}
std::string prefix(GraphType t, int fn, int sub) {
    const std::string n = std::to_string(fn + 1);
    switch (t) {
    case GraphType::Polar: return "r" + n;
    case GraphType::Param: return (sub ? "Yt" : "Xt") + n;
    case GraphType::X: case GraphType::XGreater: case GraphType::XLess: case GraphType::XGreaterEq: case GraphType::XLessEq: return "X" + n;
    default: return "Y" + n;
    }
}
std::string relation(GraphType t) {
    if (t == GraphType::Polar || t == GraphType::Param) return "=";
    return std::string(graphTypeLabel(t)).substr(1);
}
const GraphType kTableTypes[] = {GraphType::Y, GraphType::Polar, GraphType::Param};
} // namespace

// ---------------------------------------------------------------- lifecycle
void TableApp::enter(Context& ctx) {
    menu_.close(); ask_ = Ask::None; confirm_ = Confirm::None;
    built_ = false;
    ctx.ws.table.draw = TableRange::Draw::None;
    // A generated table is shown again (also after GPH-CON / GPH-PLT and G<->T).
    screen_ = ctx.ws.table.generated && !ctx.ws.table.rows.empty() ? Screen::Table : Screen::List;
    row_ = std::clamp(row_, 0, std::max(0, static_cast<int>(ctx.ws.table.rows.size()) - 1));
}
void TableApp::leave(Context& ctx) {
    (void)ctx;
    cols_.clear(); cols_.shrink_to_fit();
    built_ = false;
}

GraphType TableApp::lineType(Context& ctx, int fn) const {
    const GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(fn)];
    return f.empty() ? ctx.ws.table.funcType : f.type;
}
std::vector<TableApp::Line> TableApp::lines(Context& ctx) const {
    std::vector<Line> out;
    for (int i = 0; i < static_cast<int>(ctx.ws.functions.size()); ++i) {
        out.push_back({i, 0});
        if (lineType(ctx, i) == GraphType::Param) out.push_back({i, 1});
    }
    return out;
}

// FNV-1a of everything the table depends on (functions, A-Z, lists, settings).
std::uint64_t TableApp::fingerprint(Context& ctx) const {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* p, std::size_t n) { const auto* b = static_cast<const std::uint8_t*>(p); for (std::size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; } };
    for (const auto& f : ctx.ws.functions) {
        mix(f.expr.data(), f.expr.size()); mix(f.expr2.data(), f.expr2.size());
        const std::uint8_t meta[4] = {static_cast<std::uint8_t>(f.type), static_cast<std::uint8_t>(f.selected), f.color, static_cast<std::uint8_t>(f.style)};
        mix(meta, sizeof(meta));
    }
    for (char v = 'A'; v <= 'Z'; ++v) { const double d = ctx.ws.memory.recall(v); mix(&d, sizeof(d)); }
    for (const auto& l : ctx.ws.lists) { const std::size_t n = l.size(); mix(&n, sizeof(n)); for (double d : l) mix(&d, sizeof(d)); }
    const std::uint8_t s[3] = {static_cast<std::uint8_t>(ctx.ws.settings.angle), static_cast<std::uint8_t>(ctx.ws.graph.settings.derivative),
                               static_cast<std::uint8_t>(ctx.ws.table.funcType)};
    mix(s, sizeof(s));
    return h;
}

bool TableApp::ensure(Context& ctx) {
    const std::uint64_t fp = fingerprint(ctx);
    if (built_ && fp == fingerprint_) return buildError_.empty();
    built_ = true; fingerprint_ = fp;
    buildError_.clear(); cols_.clear();
    ctx.ws.apply();
    engine_.build({&ctx.ws.functions, &ctx.ws.memory, &ctx.ws, &ctx.ws.lists});
    try {
        cols_ = table::columns(engine_, ctx.ws.table.funcType, ctx.ws.graph.settings.derivative);
    } catch (const std::exception& e) {
        buildError_ = e.what();
    }
    col_ = std::clamp(col_, -1, static_cast<int>(cols_.size()) - 1);
    return buildError_.empty();
}

void TableApp::generate(Context& ctx) {
    TableRange& t = ctx.ws.table;
    built_ = false;
    if (!ensure(ctx)) throw MathError(buildError_);
    if (cols_.empty()) throw MathError(std::string("Math ERROR: nessuna funzione ") + graphTypeLabel(t.funcType) + " selezionata");
    t.rows = t.variable == TableRange::Variable::List ? table::listRows(ctx.ws.lists[t.list]) : table::rangeRows(t.start, t.end, t.step);
    t.generated = true;
    ctx.markDirty();                    // the rows are workspace data, not part of the GRAPH project
    row_ = 0; col_ = cols_.empty() ? -1 : 0; firstRow_ = 0; firstCol_ = 0;
    screen_ = Screen::Table;
}

bool TableApp::cell(Context& ctx, int row, int column, double& out) {
    const auto& rows = ctx.ws.table.rows;
    if (row < 0 || row >= static_cast<int>(rows.size())) return false;
    const double v = rows[static_cast<std::size_t>(row)];
    if (column < 0) { out = v; return true; }
    if (!ensure(ctx) || column >= static_cast<int>(cols_.size())) return false;
    return table::cell(engine_, cols_[static_cast<std::size_t>(column)], v, out);
}
std::string TableApp::cellText(Context& ctx, int row, int column) {
    double v;
    return cell(ctx, row, column, v) ? table::cellText(v) : std::string("ERROR");
}

// ---------------------------------------------------------------- keys
void TableApp::key(Context& ctx, const KeyEvent& ev) {
    if (confirm_ != Confirm::None) {
        if (ev.is(Key::F1) || ev.is(Key::Exe)) {
            if (confirm_ == Confirm::DeleteFunction) {
                const auto ls = lines(ctx);
                const Line line = ls[static_cast<std::size_t>(std::clamp(listRow_, 0, static_cast<int>(ls.size()) - 1))];
                ctx.ws.functions[static_cast<std::size_t>(line.fn)] = emptyGraphFunction(static_cast<std::size_t>(line.fn));
                ctx.ws.graph.projectModified = true;
            } else {
                ctx.ws.table.rows.clear(); ctx.ws.table.generated = false;
                screen_ = Screen::List;
            }
            ctx.markDirty();
        }
        if (ev.is(Key::F1) || ev.is(Key::Exe) || ev.is(Key::F6) || ev.is(Key::Exit)) confirm_ = Confirm::None;
        return;
    }
    if (ask_ != Ask::None) {
        switch (ev.key) {
        case Key::Exe: answer(ctx); return;
        case Key::Exit: ask_ = Ask::None; return;
        case Key::Left: input_.moveLeft(); return;
        case Key::Right: input_.moveRight(); return;
        case Key::Del: input_.backspace(); return;
        case Key::AC: input_.clear(); return;
        case Key::Char: if (!ev.ctrl) input_.insert(ev.ch); return;
        default: return;
        }
    }
    if (menu_.open) {
        const int c = menu_.key(ev);
        if (c == -2) { menuKind_ = Menu::None; return; }
        if (c >= 0) { menu_.close(); menuChoice(ctx, c); }
        return;
    }
    if (ev.is(Key::Setup) && screen_ != Screen::Edit) { openSetup(ctx); return; }
    switch (screen_) {
    case Screen::List: listKey(ctx, ev); return;
    case Screen::Edit: editKey(ctx, ev); return;
    case Screen::Set: case Screen::Setup: formKey(ctx, ev); return;
    case Screen::Table: tableKey(ctx, ev); return;
    }
}

void TableApp::listKey(Context& ctx, const KeyEvent& ev) {
    const auto ls = lines(ctx);
    const int n = static_cast<int>(ls.size());
    listRow_ = std::clamp(listRow_, 0, n - 1);
    const Line line = ls[static_cast<std::size_t>(listRow_)];
    GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(line.fn)];
    switch (ev.key) {
    case Key::Up: listRow_ = (listRow_ + n - 1) % n; return;
    case Key::Down: listRow_ = (listRow_ + 1) % n; return;
    case Key::PageUp: listRow_ = std::max(0, listRow_ - 6); return;
    case Key::PageDown: listRow_ = std::min(n - 1, listRow_ + 6); return;
    case Key::Home: listRow_ = 0; return;
    case Key::End: listRow_ = n - 1; return;
    case Key::F1:                                                  // SELECT
        if (f.defined()) { f.selected = !f.selected; ctx.markDirty(); ctx.ws.graph.projectModified = true; }
        return;
    case Key::F2: case Key::Del: case Key::DelForward:            // DELETE
        if (f.empty()) return;
        confirm_ = Confirm::DeleteFunction;
        confirmText_ = "Eliminare " + prefix(f.type, line.fn, 0) + (f.type == GraphType::Param ? " e Yt" + std::to_string(line.fn + 1) : std::string()) + "?";
        return;
    case Key::F3: menu_.show("TYPE (tabella)", {"Y=", "r=", "Param"}); menuKind_ = Menu::Type; return;
    case Key::F4: {
        if (f.empty()) return;
        std::vector<std::string> items;
        for (std::uint8_t i = 0; i < GraphColorCount; ++i) items.emplace_back(graphColorName(i));
        menu_.show("STYLE (colore)", items); menuKind_ = Menu::Style;
        menu_.selected = f.color % GraphColorCount;
        return;
    }
    case Key::F5: openSet(ctx); return;
    case Key::F6: generate(ctx); return;                          // TABLE
    case Key::Exe: case Key::Right: beginEdit(ctx, true); return;
    case Key::Exit: ctx.home(); return;
    case Key::Char:
        if (!ev.ctrl) beginEdit(ctx, false, ev.ch);
        return;
    default: return;
    }
}

void TableApp::beginEdit(Context& ctx, bool keep, char first) {
    const auto ls = lines(ctx);
    const Line line = ls[static_cast<std::size_t>(std::clamp(listRow_, 0, static_cast<int>(ls.size()) - 1))];
    const GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(line.fn)];
    editFn_ = line.fn; editSub_ = line.sub;
    editNew_ = f.empty();
    editor_.clear();
    const std::string& text = line.sub ? f.expr2 : f.expr;
    if (keep && !text.empty()) editor_.setExpression(text);
    if (first) editor_.insert(first);
    screen_ = Screen::Edit;
}

void TableApp::storeEdit(Context& ctx) {
    GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(editFn_)];
    const std::string text = editor_.empty() ? std::string() : editor_.toExpression();
    if (!text.empty()) {
        if (text.size() > MaxGraphExpression) throw MathError("Memory ERROR: espressione troppo lunga");
        const graph::FunctionText t = graph::splitFunctionText(text);
        (void)parseExpression(t.body);                                // Syntax ERROR before storing
    }
    if (f.empty()) f.type = ctx.ws.table.funcType;
    if (editSub_ == 1) f.expr2 = text; else f.expr = text;
    if (f.type != GraphType::Param) f.expr2.clear();
    if (!text.empty()) f.selected = true;
    ctx.markDirty(); ctx.ws.graph.projectModified = true;
    screen_ = Screen::List;
    listRow_ = std::min(listRow_ + 1, static_cast<int>(lines(ctx).size()) - 1);
}

void TableApp::editKey(Context& ctx, const KeyEvent& ev) {
    const GraphType t = lineType(ctx, editFn_);
    switch (ev.key) {
    case Key::Exe: storeEdit(ctx); return;
    case Key::Exit: screen_ = Screen::List; return;
    case Key::Char:
        if (ev.ctrl) {
            if (ev.ch == 'c' || ev.ch == 'C') { ctx.clipboard = editor_.copyTree(); ctx.toast("Copiato"); }
            else if ((ev.ch == 'v' || ev.ch == 'V') && ctx.clipboard) editor_.paste(*ctx.clipboard);
            else if (ev.ch == 'z' || ev.ch == 'Z') editor_.undo();
            else if (ev.ch == 'y' || ev.ch == 'Y') editor_.redo();
            return;
        }
        editor_.insert(ev.ch);
        return;
    case Key::Left: editor_.moveLeft(); return;
    case Key::Right: editor_.moveRight(); return;
    case Key::Up: editor_.moveUp(); return;
    case Key::Down: editor_.moveDown(); return;
    case Key::Home: editor_.moveHome(); return;
    case Key::End: editor_.moveEnd(); return;
    case Key::Del: editor_.backspace(); return;
    case Key::DelForward: editor_.eraseForward(); return;
    case Key::AC: editor_.clear(); return;
    case Key::F1: editor_.insertFraction(); return;
    case Key::F2: editor_.insertRadical(); return;
    case Key::F3: editor_.insertPower(); return;
    case Key::F4: editor_.insertText(t == GraphType::Polar ? "theta" : t == GraphType::Param ? "t" : "x"); return;
    case Key::F5: editor_.insertAbs(); return;
    default: return;
    }
}

void TableApp::openSet(Context& ctx) {
    const TableRange& t = ctx.ws.table;
    auto f = [](const char* l, double v) { return FormField{l, formatReal(v, {DisplayMode::Norm2, 0}), FormField::Type::Number, {}, 0}; };
    form_.fields = {f("Start", t.start), f("End", t.end), f("Step", t.step)};
    form_.selected = 0;
    if (screen_ != Screen::Set) formReturn_ = screen_;
    screen_ = Screen::Set;
}
void TableApp::openSetup(Context& ctx) {
    const TableRange& t = ctx.ws.table;
    std::vector<std::string> vars = {"Range"};
    for (int i = 1; i <= 6; ++i) vars.push_back("List " + std::to_string(i));
    auto choice = [](const char* l, std::vector<std::string> c, int sel) {
        const std::string text = c[static_cast<std::size_t>(sel)];
        return FormField{l, text, FormField::Type::Choice, std::move(c), sel};
    };
    form_.fields = {
        choice("Variable", vars, t.variable == TableRange::Variable::List ? 1 + t.list : 0),
        choice("Derivative", {"On", "Off"}, ctx.ws.graph.settings.derivative ? 0 : 1),
        choice("Angle", {"Deg", "Rad", "Gra"}, ctx.ws.settings.angle == AngleMode::Degrees ? 0 : ctx.ws.settings.angle == AngleMode::Radians ? 1 : 2),
    };
    form_.selected = 0;
    if (screen_ != Screen::Setup && screen_ != Screen::Set) formReturn_ = screen_;
    screen_ = Screen::Setup;
}

void TableApp::formKey(Context& ctx, const KeyEvent& ev) {
    if (form_.editing()) { (void)form_.key(ctx, ev); return; }
    if (ev.is(Key::Exit) || ev.is(Key::F6) || (screen_ == Screen::Set && ev.is(Key::F1))) {
        TableRange& t = ctx.ws.table;
        if (screen_ == Screen::Set) {
            try {
                const double start = form_.number(ctx, 0), end = form_.number(ctx, 1), step = form_.number(ctx, 2);
                (void)table::rangeRows(start, end, step);              // the same checks as generating the table
                t.start = start; t.end = end; t.step = step;
            } catch (...) {
                openSet(ctx);                                          // back to the stored values: EXIT leaves next time
                throw;
            }
        } else {
            const int var = form_.fields[0].choice;
            t.variable = var == 0 ? TableRange::Variable::Range : TableRange::Variable::List;
            if (var > 0) t.list = static_cast<std::uint8_t>(var - 1);
            ctx.ws.graph.settings.derivative = form_.fields[1].choice == 0;
            const int a = form_.fields[2].choice;
            ctx.ws.settings.angle = a == 0 ? AngleMode::Degrees : a == 1 ? AngleMode::Radians : AngleMode::Gradians;
            ctx.ws.apply();
        }
        ctx.markDirty(); ctx.ws.graph.projectModified = true;
        // back where SET / SETUP was opened (the table only if it still exists)
        screen_ = formReturn_ == Screen::Table && t.generated && !t.rows.empty() ? Screen::Table : Screen::List;
        if (screen_ == Screen::Table && !ensure(ctx)) { const std::string e = buildError_; screen_ = Screen::List; throw MathError(e); }
        return;
    }
    (void)form_.key(ctx, ev);
}

void TableApp::tableKey(Context& ctx, const KeyEvent& ev) {
    TableRange& t = ctx.ws.table;
    if (ev.is(Key::F1) || ev.is(Key::Exit)) { screen_ = Screen::List; return; }   // FORMULA
    if (!ensure(ctx)) { screen_ = Screen::List; throw MathError(buildError_); }
    const int rows = static_cast<int>(t.rows.size()), cols = static_cast<int>(cols_.size());
    if (rows == 0) { screen_ = Screen::List; return; }
    row_ = std::clamp(row_, 0, rows - 1);
    switch (ev.key) {
    case Key::Up: row_ = std::max(0, row_ - 1); return;
    case Key::Down: row_ = std::min(rows - 1, row_ + 1); return;
    case Key::PageUp: row_ = std::max(0, row_ - 10); return;
    case Key::PageDown: row_ = std::min(rows - 1, row_ + 10); return;
    case Key::Home: row_ = 0; return;
    case Key::End: row_ = rows - 1; return;
    case Key::Left: col_ = std::max(-1, col_ - 1); return;
    case Key::Right: col_ = std::min(cols - 1, col_ + 1); return;
    case Key::F2: confirm_ = Confirm::DeleteTable; confirmText_ = "Eliminare la tabella?"; return;
    case Key::F3: menu_.show("ROW", {"DELETE", "INSERT", "ADD"}); menuKind_ = Menu::Row; return;
    case Key::F4: case Key::Exe:                                       // EDIT: a new value of the variable
        ask_ = Ask::EditValue; input_.clear();
        askPrompt_ = std::string(table::variableName(t.funcType)) + " (riga " + std::to_string(row_ + 1) + "):";
        return;
    case Key::F5: case Key::F6:
        // GPH-CON / GPH-PLT: GRAPH draws the points of the table (manual 5-35).
        if (cols_.empty()) throw MathError("Math ERROR: nessuna funzione selezionata");
        t.draw = ev.is(Key::F5) ? TableRange::Draw::Connect : TableRange::Draw::Plot;
        ctx.open(AppId::Graph);
        return;
    case Key::Optn: menu_.show("OPTN", {"LISTMEM (colonna -> List)"}); menuKind_ = Menu::Optn; return;
    case Key::Char:
        // A number typed in the table replaces the variable of the row (the other columns cannot be changed).
        if (!ev.ctrl && (std::isdigit(static_cast<unsigned char>(ev.ch)) || ev.ch == '-' || ev.ch == '.' || ev.ch == '(')) {
            ask_ = Ask::EditValue; input_.clear(); input_.insert(ev.ch);
            askPrompt_ = std::string(table::variableName(t.funcType)) + " (riga " + std::to_string(row_ + 1) + "):";
        }
        return;
    default: return;
    }
}

void TableApp::menuChoice(Context& ctx, int c) {
    TableRange& t = ctx.ws.table;
    const Menu m = menuKind_;
    menuKind_ = Menu::None;
    switch (m) {
    case Menu::Type: {
        const GraphType type = kTableTypes[std::clamp(c, 0, 2)];
        if (type != t.funcType) { t.funcType = type; t.generated = false; t.rows.clear(); }
        ctx.markDirty(); ctx.ws.graph.projectModified = true;
        return;
    }
    case Menu::Style: {
        const auto ls = lines(ctx);
        const Line line = ls[static_cast<std::size_t>(std::clamp(listRow_, 0, static_cast<int>(ls.size()) - 1))];
        ctx.ws.functions[static_cast<std::size_t>(line.fn)].color = static_cast<std::uint8_t>(c % GraphColorCount);
        ctx.markDirty(); ctx.ws.graph.projectModified = true;
        return;
    }
    case Menu::Row: {
        auto& rows = t.rows;
        if (rows.empty()) return;
        const auto at = rows.begin() + std::clamp(row_, 0, static_cast<int>(rows.size()) - 1);
        if (c == 0) {                                                  // DELETE
            rows.erase(at);
            if (rows.empty()) { t.generated = false; screen_ = Screen::List; }
            row_ = std::min(row_, std::max(0, static_cast<int>(rows.size()) - 1));
        } else {
            // INSERT: a copy of the row above it; ADD: a copy below it (then EDIT changes the value).
            if (rows.size() >= table::MaxRows) throw MathError("Memory ERROR: massimo 999 righe");
            const double v = *at;
            if (c == 1) rows.insert(at, v); else { rows.insert(at + 1, v); ++row_; }
        }
        ctx.markDirty();
        return;
    }
    case Menu::Optn:
        ask_ = Ask::ListMem; input_.clear();
        askPrompt_ = "LISTMEM: colonna " + (col_ < 0 ? std::string(table::variableName(t.funcType)) : cols_[static_cast<std::size_t>(col_)].title) + " in List (1-6):";
        return;
    case Menu::None: return;
    }
}

void TableApp::answer(Context& ctx) {
    TableRange& t = ctx.ws.table;
    const Ask a = ask_;
    const std::string text = input_.empty() ? std::string() : input_.toExpression();
    ask_ = Ask::None;
    if (text.empty()) return;
    EvalContext ec;
    ec.memory = &ctx.ws.memory;
    ec.names = &ctx.ws;
    // An illegal value (1/0, undefined) is an error and the original stays (manual 5-34).
    const double v = evaluateReal(*parseExpression(text), ec);
    if (!std::isfinite(v)) throw MathError("Math ERROR: valore non finito");
    if (a == Ask::EditValue) {
        if (row_ < 0 || row_ >= static_cast<int>(t.rows.size())) return;
        t.rows[static_cast<std::size_t>(row_)] = v;
        ctx.markDirty();
        return;
    }
    // LISTMEM (manual 5-35): the column under the cursor into List n.
    if (v != std::floor(v) || v < 1 || v > static_cast<double>(ctx.ws.lists.size())) throw MathError("Argument ERROR: List da 1 a 6");
    if (t.rows.size() > Workspace::MaxListLength) throw MathError("Dimension ERROR: una lista contiene al massimo 255 elementi");
    std::vector<double> values;
    values.reserve(t.rows.size());
    for (int r = 0; r < static_cast<int>(t.rows.size()); ++r) {
        double x;
        if (!cell(ctx, r, col_, x)) throw MathError("Math ERROR: valore non definito alla riga " + std::to_string(r + 1));
        values.push_back(x);
    }
    ctx.ws.lists[static_cast<std::size_t>(v) - 1] = std::move(values);
    ctx.markDirty();
    ctx.toast("Copiata in List " + std::to_string(static_cast<int>(v)));
}

// ---------------------------------------------------------------- drawing
void TableApp::drawList(Context& ctx, gfx::Canvas& c) {
    (void)ensure(ctx);
    const auto ls = lines(ctx);
    listRow_ = std::clamp(listRow_, 0, static_cast<int>(ls.size()) - 1);
    const bool editing = screen_ == Screen::Edit;
    const int rowH = 28, top = ui::ContentTop + 1, panelH = editing ? 72 : 0;
    const int visible = (ui::ContentBottom - top - 16 - panelH) / rowH;
    if (listRow_ < listFirst_) listFirst_ = listRow_;
    if (listRow_ >= listFirst_ + visible) listFirst_ = listRow_ - visible + 1;
    // Table type and variable on the first line (Table Func, Setup Variable).
    const TableRange& tr = ctx.ws.table;
    const std::string head = std::string("Table Func: ") + graphTypeLabel(tr.funcType) + "   " + table::variableName(tr.funcType) + ": " +
                             (tr.variable == TableRange::Variable::List ? "List " + std::to_string(tr.list + 1) : std::string("Range"));
    c.rect(0, top, ui::ScreenW, 16, StatusBg, true);
    ui::text(c, 4, top + 2, head.c_str(), Muted, font::Face::Small);
    for (int k = 0; k < visible && listFirst_ + k < static_cast<int>(ls.size()); ++k) {
        const Line line = ls[static_cast<std::size_t>(listFirst_ + k)];
        const GraphFunction& f = ctx.ws.functions[static_cast<std::size_t>(line.fn)];
        const GraphType t = lineType(ctx, line.fn);
        const bool tabulated = t == tr.funcType;
        const int y = top + 16 + k * rowH, sy = y + rowH / 2;
        if (listFirst_ + k == listRow_) c.rect(0, y, ui::ScreenW, rowH, SelectSoft, true);
        c.rect(2, sy - 7, 3, 14, lineColor(f.color), true);
        const std::string p = prefix(t, line.fn, line.sub), rel = relation(t);
        const int pw = font::measure(font::Face::Medium, p.c_str()).width, rw = font::measure(font::Face::Medium, rel.c_str()).width;
        const std::uint16_t ink = tabulated || f.empty() ? Ink : Muted;
        ui::text(c, 10, sy - 8, p.c_str(), ink);
        const bool on = f.selected && f.defined() && tabulated;
        if (on) c.rect(10 + pw, sy - 9, rw + 2, 18, Accent, true);    // highlighted "=": in the table
        ui::text(c, 11 + pw, sy - 8, rel.c_str(), on ? 0xFFFF : ink);
        const std::string& text = line.sub ? f.expr2 : f.expr;
        if (!text.empty()) {
            NaturalEditor view;
            try { view.setExpression(text); } catch (...) { view.clear(); view.insertText("?"); }
            const auto sz = math::measure(view.root());
            const int ex = 16 + pw + rw;
            auto l = math::layout(view.root(), ex, sy - sz.height / 2 + sz.baseline, ui::ScreenW - ex - 4, rowH);
            math::draw(c, l, ink, Cursor, Placeholder);
            if (tabulated && f.defined() && !engine_.error(static_cast<std::size_t>(line.fn)).empty())
                ui::textRight(c, ui::ScreenW - 3, y + 1, "ERROR", Error, font::Face::Small);
        }
        c.hLine(0, y + rowH - 1, ui::ScreenW, Grid);
    }
    ui::scrollBar(c, ui::ScreenW - 3, top + 16, visible * rowH, static_cast<int>(ls.size()), visible, listFirst_);
    if (editing) {
        const int ey = ui::ContentBottom - panelH;
        c.rect(0, ey, ui::ScreenW, panelH, StatusBg, true);
        c.hLine(0, ey, ui::ScreenW, Faint);
        const GraphType t = lineType(ctx, editFn_);
        ui::text(c, 4, ey + 4, (prefix(t, editFn_, editSub_) + relation(t)).c_str(), Accent, font::Face::Small);
        const math::CursorTarget cur{editor_.activeRowPtr(), editor_.cursorIndex()};
        const auto sz = math::measure(editor_.root());
        auto l = math::layout(editor_.root(), 6, ey + 20 + sz.baseline, ui::ScreenW - 12, panelH - 22, math::Align::Left, &cur);
        math::draw(c, l, Ink, Cursor, Placeholder);
        const char* keys[6] = {"a/b", "\xE2\x88\x9A\xE2\x96\xA1", "\xE2\x96\xA1^", t == GraphType::Polar ? "\xCE\xB8" : t == GraphType::Param ? "T" : "X", "|\xE2\x96\xA1|", nullptr};
        ui::softKeys(c, keys);
        return;
    }
    const char* keys[6] = {"SELECT", "DELETE", "TYPE", "STYLE", "SET", "TABLE"};
    ui::softKeys(c, keys);
}

void TableApp::drawTable(Context& ctx, gfx::Canvas& c) {
    const TableRange& t = ctx.ws.table;
    const int top = ui::ContentTop + 1, bottom = ui::ContentBottom;
    if (!ensure(ctx)) {
        ui::textCenter(c, ui::ScreenW / 2, 120, buildError_.c_str(), Error, font::Face::Small);
        const char* keys[6] = {"FORMULA", nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, keys);
        return;
    }
    const int rows = static_cast<int>(t.rows.size()), cols = static_cast<int>(cols_.size());
    row_ = std::clamp(row_, 0, std::max(0, rows - 1));
    col_ = std::clamp(col_, -1, cols - 1);
    const int visibleCols = 4, xW = 58, colW = (ui::ScreenW - xW - 4) / visibleCols, rowH = 18;
    if (col_ >= 0 && col_ < firstCol_) firstCol_ = col_;
    if (col_ >= firstCol_ + visibleCols) firstCol_ = col_ - visibleCols + 1;
    firstCol_ = std::clamp(firstCol_, 0, std::max(0, cols - visibleCols));
    const int visibleRows = (bottom - top - 18 - 22) / rowH;
    if (row_ < firstRow_) firstRow_ = row_;
    if (row_ >= firstRow_ + visibleRows) firstRow_ = row_ - visibleRows + 1;
    // header: variable, then the function columns with their colour
    c.rect(0, top, ui::ScreenW, 18, StatusBg, true);
    ui::textCenter(c, xW / 2, top + 1, table::variableName(t.funcType), Ink);
    for (int k = 0; k < visibleCols && firstCol_ + k < cols; ++k) {
        const table::Column& col = cols_[static_cast<std::size_t>(firstCol_ + k)];
        const int x = xW + 2 + k * colW;
        c.rect(x + 2, top + 5, 3, 9, lineColor(col.color), true);
        ui::textCenter(c, x + colW / 2 + 2, top + 1, col.title.c_str(), Ink);
    }
    for (int r = 0; r < visibleRows && firstRow_ + r < rows; ++r) {
        const int rr = firstRow_ + r, y = top + 18 + r * rowH;
        const bool selRow = rr == row_;
        if (selRow && col_ < 0) c.rect(1, y, xW - 2, rowH, Select, true);
        ui::textRight(c, xW - 4, y + 1, cellText(ctx, rr, -1).c_str(), selRow && col_ < 0 ? SelectInk : Ink);
        for (int k = 0; k < visibleCols && firstCol_ + k < cols; ++k) {
            const int x = xW + 2 + k * colW, ci = firstCol_ + k;
            const bool sel = selRow && ci == col_;
            if (sel) c.rect(x + 1, y, colW - 2, rowH, Select, true);
            const std::string text = cellText(ctx, rr, ci);
            ui::textRight(c, x + colW - 4, y + 1, text.c_str(), sel ? SelectInk : text == "ERROR" ? Error : lineColor(cols_[static_cast<std::size_t>(ci)].color));
        }
        c.hLine(0, y + rowH - 1, ui::ScreenW, Grid);
    }
    c.vLine(xW, top, 18 + std::min(visibleRows, rows) * rowH, Faint);
    ui::scrollBar(c, ui::ScreenW - 3, top + 18, visibleRows * rowH, rows, visibleRows, firstRow_);
    // full value of the selected cell (the cells show at most 6 characters)
    double v = 0;
    std::string full;
    if (col_ < 0) full = std::string(table::variableName(t.funcType)) + "=";
    else {
        const table::Column& col = cols_[static_cast<std::size_t>(col_)];
        full = (col.kind == table::Column::Kind::Derivative ? std::string(table::derivativeName(t.funcType)) : col.title) + "=";
    }
    full += cell(ctx, row_, col_, v) ? formatReal(v, ctx.ws.settings.display) : std::string("ERROR");
    c.hLine(0, bottom - 22, ui::ScreenW, Faint);
    ui::textFit(c, 6, bottom - 19, full, ui::ScreenW - 12, Ink);
    const char* keys[6] = {"FORMULA", "DELETE", "ROW", "EDIT", "GPH-CON", "GPH-PLT"};
    ui::softKeys(c, keys);
}

void TableApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "TABLE");
    const int top = ui::ContentTop + 1;
    switch (screen_) {
    case Screen::List: case Screen::Edit: drawList(ctx, c); break;
    case Screen::Table: drawTable(ctx, c); break;
    case Screen::Set: case Screen::Setup: {
        c.rect(0, top, ui::ScreenW, 20, SelectSoft, true);
        const bool set = screen_ == Screen::Set;
        ui::text(c, 6, top + 2, set ? (std::string("Table Setting (") + table::variableName(ctx.ws.table.funcType) + ")").c_str() : "SET UP (Table)", Ink);
        form_.draw(c, top + 22, top + 22 + (set ? 60 : 80));
        if (set && ctx.ws.table.variable == TableRange::Variable::List)
            ui::text(c, 6, top + 112, ("Variable = List " + std::to_string(ctx.ws.table.list + 1) + ": il range non e' usato").c_str(), Muted, font::Face::Small);
        const char* keys[6] = {set ? "OK" : nullptr, nullptr, nullptr, nullptr, nullptr, "OK"};
        ui::softKeys(c, keys);
        break;
    }
    }
    if (menu_.open) menu_.draw(c);
    if (ask_ != Ask::None) {
        const gfx::Rect r{20, 120, ui::ScreenW - 40, 58};
        ui::panel(c, r, 0xFFFF, Accent);
        ui::textFit(c, r.x + 8, r.y + 6, askPrompt_, r.w - 16, Accent, font::Face::Small);
        const math::CursorTarget cur{input_.activeRowPtr(), input_.cursorIndex()};
        const auto sz = math::measure(input_.root());
        auto l = math::layout(input_.root(), r.x + 8, r.y + 24 + sz.baseline, r.w - 16, 30, math::Align::Left, &cur);
        math::draw(c, l, Ink, Cursor, Placeholder);
    }
    if (confirm_ != Confirm::None) {
        const gfx::Rect r{16, 110, ui::ScreenW - 32, 80};
        ui::panel(c, r, 0xFFFF, Warn);
        ui::text(c, r.x + 8, r.y + 8, confirmText_.c_str(), Ink);
        ui::text(c, r.x + 8, r.y + r.h - 20, "F1/EXE: S\xC3\xAC   F6/EXIT: No", Muted, font::Face::Small);
    }
}
} // namespace fx::app
