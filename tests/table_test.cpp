// TABLE (manual 5-32..5-35): the table core (rows, columns, cells, 6-character
// cells) against closed-form values, and the TABLE mode from the keyboard:
// relation list, SET, Setup Variable/Derivative, Y=, r= and Param tables,
// EDIT, ROW DELETE/INSERT/ADD, DELETE, LISTMEM, GPH-CON/PLT, limits,
// persistence (workspace v4, v3 records, .pcgraph).
#include "apps.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "firmware.hpp"
#include "graph_file.hpp"
#include "table_model.hpp"
#include "test_util.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace fx;
using app::AppId;
using TA = app::TableApp;

namespace {
constexpr double kPi = 3.14159265358979323846;
std::uint16_t press(std::uint8_t code, std::uint8_t state = 1) { return static_cast<std::uint16_t>((state << 8) | code); }
struct Rig {
    BoardPlatform platform;
    Firmware fw{platform, board::storageFlash()};
    Rig() { fw.boot(); settle(); }
    void settle(int max = 2000) {
        for (int i = 0; i < max; ++i) { if (!fw.step()) break; fw.draw(board::graphics()); board::present(); board::host::advance(5); }
    }
    void raw(std::uint16_t r) { fw.rawKey(r); board::host::advance(20); if (fw.step()) { fw.draw(board::graphics()); board::present(); } }
    void key(std::uint8_t code) { raw(press(code)); raw(press(code, 3)); }
    void keys(std::uint8_t code, int n) { for (int i = 0; i < n; ++i) key(code); }
    void type(const std::string& s) { for (char c : s) raw(press(static_cast<std::uint8_t>(c))); }
    void enter(const std::string& s) { type(s); key(pckey::Enter); }
    bool error() { return fw.apps().errorShown(); }
    std::string errorTitle() { return fw.apps().errorTitle(); }
    void closeError() { if (error()) key(pckey::Esc); }
    Workspace& ws() { return fw.workspace(); }
    app::Context& ctx() { return fw.apps().context(); }
    TA* table() { return dynamic_cast<TA*>(fw.apps().active()); }
    void home() { key(pckey::Tab); }
    // TABLE with the given Y= functions in Y1, Y2, ... and the range Start..End Step.
    TA* tableWith(std::initializer_list<const char*> ys, double start, double end, double step) {
        home();
        for (std::size_t i = 0; i < ws().functions.size(); ++i) ws().functions[i] = emptyGraphFunction(i);
        std::size_t i = 0;
        for (const char* y : ys) { ws().functions[i].expr = y; ws().functions[i].type = GraphType::Y; ++i; }
        ws().table = TableRange{};
        ws().table.start = start; ws().table.end = end; ws().table.step = step;
        type("7");
        return table();
    }
};
bool near(double a, double b, double tol) { return std::abs(a - b) <= tol * std::max(1.0, std::abs(b)); }
// Engine with functions for the core tests.
struct Core {
    Workspace ws;
    graph::GraphEngine engine;
    Core() { for (std::size_t i = 0; i < ws.functions.size(); ++i) ws.functions[i] = emptyGraphFunction(i); }
    void set(std::size_t i, const char* e, GraphType t = GraphType::Y, const char* e2 = "") {
        ws.functions[i].expr = e; ws.functions[i].type = t; ws.functions[i].expr2 = e2; ws.functions[i].selected = true;
    }
    void build() { engine.build({&ws.functions, &ws.memory, &ws, &ws.lists}); }
};
} // namespace

int main() {
    board::init();
    // ================================================================ rows
    {
        auto r = table::rangeRows(-3, 3, 1);
        CHECK(r.size() == 7 && r.front() == -3 && r.back() == 3);
        r = table::rangeRows(0, 1, 0.1);                                     // positive step
        CHECK(r.size() == 11 && r[0] == 0 && near(r[10], 1, 1e-15));
        r = table::rangeRows(-10, -2, 2);                                    // negative interval
        CHECK(r.size() == 5 && r[0] == -10 && r[4] == -2);
        r = table::rangeRows(-0.3, 0.3, 0.1);                                // mixed interval: exactly 0 in the middle
        CHECK(r.size() == 7 && r[3] == 0 && near(r[6], 0.3, 1e-15));
        r = table::rangeRows(1, 1, 5);
        CHECK(r.size() == 1 && r[0] == 1);
        r = table::rangeRows(0, 998, 1);
        CHECK(r.size() == table::MaxRows);
        auto throws = [](double a, double b, double s) { try { (void)table::rangeRows(a, b, s); } catch (const MathError&) { return true; } return false; };
        CHECK(throws(0, 1, 0) && throws(0, 1, -1) && throws(2, 1, 1) && throws(0, 999, 1) && throws(0, NAN, 1));
        CHECK(table::listRows({4, 1, 9}) == std::vector<double>({4, 1, 9}));
        bool empty = false;
        try { (void)table::listRows({}); } catch (const MathError&) { empty = true; }
        CHECK(empty);
    }
    // ================================================================ 6-character cells (manual 5-33)
    {
        CHECK(table::cellText(9) == "9" && table::cellText(-2.625) == "-2.625" && table::cellText(0) == "0");
        CHECK(table::cellText(1.0 / 3) == "0.3333" && table::cellText(-1.0 / 3) == "-0.333");
        CHECK(table::cellText(123456) == "123456" && table::cellText(1234567) == "1.23E6");
        CHECK(table::cellText(1e15) == "1E15" && table::cellText(-2.5e-7) == "-2E-7");   // 2.5e-7 is 2.4999..e-7 in binary
        CHECK(table::cellText(0.000125) == "1.3E-4" && table::cellText(std::sqrt(2.0)) == "1.4142");
        CHECK(table::cellText(NAN) == "ERROR" && table::cellText(INFINITY) == "ERROR");
        for (double v : {1.0, -1.0, 3.14159, -99999.9, 1e-300, -1e300, 5e-5, 12345.678}) CHECK(table::cellText(v).size() <= 6);
    }
    // ================================================================ columns and values against closed forms
    {
        Core c;
        c.set(0, "x^3-4x");                    // polynomial
        c.set(1, "sin(x)");                    // trigonometric
        c.set(2, "e^(x)");                     // exponential
        c.set(3, "ln(x)");                     // logarithmic: undefined for x <= 0
        c.set(4, "(x^2+1)/(x-1)");             // rational: pole at 1
        c.set(5, "sqrt(4-x^2)");               // undefined for |x| > 2
        c.set(6, "2x", GraphType::YGreater);   // another type: not in a Y= table
        c.build();
        setAngleMode(AngleMode::Radians);
        const auto cols = table::columns(c.engine, GraphType::Y, false);
        CHECK(cols.size() == 6 && cols[0].title == "Y1" && cols[5].title == "Y6");
        const auto rows = table::rangeRows(-3, 3, 0.5);
        int defined[6] = {};
        for (double x : rows) {
            double v;
            if (table::cell(c.engine, cols[0], x, v)) { ++defined[0]; CHECK(near(v, x * x * x - 4 * x, 1e-14)); }
            if (table::cell(c.engine, cols[1], x, v)) { ++defined[1]; CHECK(near(v, std::sin(x), 1e-14)); }
            if (table::cell(c.engine, cols[2], x, v)) { ++defined[2]; CHECK(near(v, std::exp(x), 1e-14)); }
            if (table::cell(c.engine, cols[3], x, v)) { ++defined[3]; CHECK(x > 0 && near(v, std::log(x), 1e-14)); }
            if (table::cell(c.engine, cols[4], x, v)) { ++defined[4]; CHECK(x != 1 && near(v, (x * x + 1) / (x - 1), 1e-14)); }
            if (table::cell(c.engine, cols[5], x, v)) { ++defined[5]; CHECK(std::abs(x) <= 2 && near(v, std::sqrt(4 - x * x), 1e-14)); }
        }
        CHECK(defined[0] == 13 && defined[1] == 13 && defined[2] == 13);
        CHECK(defined[3] == 6 && defined[4] == 12 && defined[5] == 9);   // ln: 0.5..3; pole at 1; |x|<=2
        // degrees
        setAngleMode(AngleMode::Degrees);
        double v;
        CHECK(table::cell(c.engine, cols[1], 30, v) && near(v, 0.5, 1e-14));
        setAngleMode(AngleMode::Radians);
        // derivative columns
        const auto dcols = table::columns(c.engine, GraphType::Y, true);
        CHECK(dcols.size() == 12 && dcols[1].title == "Y1'" && dcols[1].kind == table::Column::Kind::Derivative);
        CHECK(table::cell(c.engine, dcols[1], 2, v) && near(v, 3 * 4 - 4, 1e-8));
        CHECK(table::cell(c.engine, dcols[5], 1, v) && near(v, std::exp(1.0), 1e-8));
        CHECK(table::cell(c.engine, dcols[7], 2, v) && near(v, 0.5, 1e-8));
        CHECK(!table::cell(c.engine, dcols[7], -1, v));                   // ln' outside the domain
        // Derivative On with a range or an overwrite: error (manual 5-34)
        c.set(7, "x+A,[A=1,2]");
        c.build();
        bool threw = false;
        try { (void)table::columns(c.engine, GraphType::Y, true); } catch (const MathError&) { threw = true; }
        CHECK(threw);
        CHECK(table::columns(c.engine, GraphType::Y, false).size() == 7);
        // r= and Param tables
        Core p;
        p.set(0, "2cos(theta)", GraphType::Polar);
        p.set(1, "3cos(t)", GraphType::Param, "2sin(t)");
        p.set(2, "x");                                                    // Y=: not in these tables
        p.build();
        setAngleMode(AngleMode::Degrees);
        const auto rc = table::columns(p.engine, GraphType::Polar, false);
        CHECK(rc.size() == 1 && rc[0].title == "r1");
        CHECK(table::cell(p.engine, rc[0], 60, v) && near(v, 1, 1e-14));
        double x, y;
        CHECK(table::point(p.engine, p.engine.plots()[0], 60, x, y) && near(x, 0.5, 1e-14) && near(y, std::sqrt(3.0) / 2, 1e-14));
        const auto pc = table::columns(p.engine, GraphType::Param, true);
        CHECK(pc.size() == 3 && pc[0].title == "Xt2" && pc[1].title == "Yt2" && pc[2].kind == table::Column::Kind::Derivative);
        setAngleMode(AngleMode::Radians);
        CHECK(table::cell(p.engine, pc[0], kPi / 3, v) && near(v, 1.5, 1e-14));
        CHECK(table::cell(p.engine, pc[1], kPi / 3, v) && near(v, std::sqrt(3.0), 1e-14));
        CHECK(table::cell(p.engine, pc[2], kPi / 4, v) && near(v, -2.0 / 3.0, 1e-7));   // dY/dX = 2cos t / (-3 sin t)
    }
    // ================================================================ TABLE from the keyboard
    Rig r;
    Workspace& ws = r.ws();
    {
        r.home();
        for (std::size_t i = 0; i < ws.functions.size(); ++i) ws.functions[i] = emptyGraphFunction(i);
        ws.table = TableRange{};
        r.type("7");
        TA* t = r.table();
        CHECK(t && t->screen() == TA::Screen::List);
        r.type("x^2"); r.key(pckey::Right); r.enter("-3x");          // typed on Y1 (-> leaves the power): stored and selected
        CHECK(ws.functions[0].type == GraphType::Y && ws.functions[0].selected && !ws.functions[0].empty());
        // SET: Start -2, End 2, Step 0.5
        r.key(pckey::F5);
        CHECK(t->screen() == TA::Screen::Set);
        r.enter("-2"); r.enter("2"); r.enter("0.5");
        r.key(pckey::Esc);
        CHECK(ws.table.start == -2 && ws.table.end == 2 && ws.table.step == 0.5);
        r.key(pckey::F6);                                             // TABLE
        CHECK(t->screen() == TA::Screen::Table && ws.table.rows.size() == 9 && ws.table.generated);
        double v = 0;
        CHECK(t->cell(r.ctx(), 0, 0, v) && v == 10 && t->cell(r.ctx(), 8, 0, v) && v == -2);
        CHECK(t->cellText(r.ctx(), 3, 0) == "1.75");                  // x = -0.5: 0.25 + 1.5
        // the function is changed: the table follows
        r.key(pckey::F1);                                             // FORMULA
        CHECK(t->screen() == TA::Screen::List);
        r.key(pckey::Home); r.enter("2x+1");
        r.key(pckey::Down);
        r.enter("1/x");                                               // Y2 with a pole at 0
        r.key(pckey::F6);
        CHECK(t->columns().size() == 2 && t->cell(r.ctx(), 0, 0, v) && v == -3);
        CHECK(t->cellText(r.ctx(), 4, 1) == "ERROR" && t->cellText(r.ctx(), 5, 1) == "2");   // 1/0 undefined, 1/0.5
        // scrolling and the cursor
        r.keys(pckey::Down, 20);
        CHECK(t->row() == 8);
        r.key(pckey::PageUp);
        CHECK(t->row() == 0);
        r.key(pckey::Left);
        CHECK(t->column() == -1);
        r.keys(pckey::Right, 5);
        CHECK(t->column() == 1);
        ws.graph.projectModified = false;
        // EDIT: a new x value; an illegal value is an error and nothing changes
        r.key(pckey::Left); r.key(pckey::Left);
        r.key(pckey::F4); r.enter("pi/4");
        CHECK(near(ws.table.rows[0], kPi / 4, 1e-15) && t->cell(r.ctx(), 0, 0, v) && near(v, kPi / 2 + 1, 1e-14));
        r.key(pckey::F4); r.enter("1/0");
        CHECK(r.error()); r.closeError();
        CHECK(near(ws.table.rows[0], kPi / 4, 1e-15));
        r.type("7"); r.key(pckey::Enter);                             // a number typed in the table edits x
        CHECK(ws.table.rows[0] == 7);
        CHECK(!ws.graph.projectModified);                             // table rows are not part of the GRAPH project
        // ROW: DELETE, INSERT (copy above), ADD (copy below)
        r.key(pckey::Down);                                           // row 1: x = -1.5
        r.key(pckey::F3); r.type("1");
        CHECK(ws.table.rows.size() == 8 && ws.table.rows[1] == -1);
        r.key(pckey::F3); r.type("2");
        CHECK(ws.table.rows.size() == 9 && ws.table.rows[1] == -1 && ws.table.rows[2] == -1);
        r.key(pckey::F3); r.type("3");
        CHECK(ws.table.rows.size() == 10 && ws.table.rows[2] == -1 && t->row() == 2);
        // LISTMEM: the Y1 column into List 2
        r.key(pckey::Right);
        r.key(pckey::F8); r.type("1"); r.enter("2");
        CHECK(!r.error() && ws.lists[1].size() == 10 && ws.lists[1][0] == 15 && ws.lists[1][1] == -1);   // 2*7+1, 2*(-1)+1
        // the Y2 column has an undefined value: error, the list is not changed
        ws.table.rows[3] = 0;
        r.key(pckey::Right);
        r.key(pckey::F8); r.type("1"); r.enter("3");
        CHECK(r.error()); r.closeError();
        CHECK(ws.lists[2].empty());
        r.key(pckey::F8); r.type("1"); r.enter("9");                  // List 9 does not exist
        CHECK(r.error()); r.closeError();
        // GPH-CON: GRAPH draws the rows of this (edited) table
        r.key(pckey::F5);
        CHECK(r.fw.apps().activeId() == AppId::Graph && ws.table.draw == TableRange::Draw::Connect);
        r.settle();
        auto* g = dynamic_cast<app::GraphApp*>(r.fw.apps().active());
        CHECK(g && g->curves().size() >= 2 && !g->curves()[0].xy.empty());
        r.key(pckey::Esc);                                            // back to the table
        CHECK(r.fw.apps().activeId() == AppId::Table && r.table()->screen() == TA::Screen::Table);
        t = r.table();
        // DELETE the table
        r.key(pckey::F2); r.key(pckey::F1);
        CHECK(t->screen() == TA::Screen::List && !ws.table.generated && ws.table.rows.empty());
    }
    // ================================================================ Variable = List, Setup
    {
        TA* t = r.tableWith({"x^2"}, -3, 3, 1);
        ws.lists[3] = {4, -1.5, 9, 0.25};
        r.key(pckey::F10);                                            // SETUP of TABLE
        CHECK(t && t->screen() == TA::Screen::Setup);
        r.keys(pckey::Right, 4);                                      // Variable: List 4
        r.key(pckey::Esc);
        CHECK(ws.table.variable == TableRange::Variable::List && ws.table.list == 3);
        r.key(pckey::F6);
        double v;
        CHECK(ws.table.rows == ws.lists[3] && t->cell(r.ctx(), 1, 0, v) && v == 2.25);
        ws.lists[3].clear();                                          // an empty list: Dimension ERROR
        r.key(pckey::F1); r.key(pckey::F6);
        CHECK(r.error() && r.errorTitle() == "Dimension ERROR"); r.closeError();
        r.key(pckey::F10); r.keys(pckey::Left, 4); r.key(pckey::Esc);   // back to Range
        CHECK(ws.table.variable == TableRange::Variable::Range);
        // Derivative On from the Setup: dY/dX columns
        r.key(pckey::F10); r.key(pckey::Down); r.key(pckey::Left); r.key(pckey::Esc);
        CHECK(ws.graph.settings.derivative);
        if (t->screen() == TA::Screen::Table) r.key(pckey::F1);       // FORMULA
        r.key(pckey::F6);
        CHECK(t->columns().size() == 2 && t->cell(r.ctx(), 0, 1, v) && near(v, -6, 1e-8));
        // a function with an overwrite and Derivative On: error (manual 5-34)
        ws.functions[1].expr = "x+A,[A=1,2]"; ws.functions[1].type = GraphType::Y; ws.functions[1].selected = true;
        r.key(pckey::F1);                                             // FORMULA: back to the list, no error yet
        CHECK(!r.error() && t->screen() == TA::Screen::List);
        r.key(pckey::F6);
        CHECK(r.error() && r.errorTitle() == "Math ERROR"); r.closeError();
        ws.functions[1] = emptyGraphFunction(1);
        ws.graph.settings.derivative = false;
    }
    // ================================================================ r= and Param tables
    {
        TA* t = r.tableWith({}, 0, 90, 30);
        ws.functions[0] = {"2cos(theta)", GraphType::Polar, true, 0, LineStyle::Normal, {}};
        ws.functions[1] = {"3cos(t)", GraphType::Param, true, 1, LineStyle::Normal, "2sin(t)"};
        ws.settings.angle = AngleMode::Degrees;
        r.key(pckey::F3); r.type("2");                                // TYPE: r=
        CHECK(ws.table.funcType == GraphType::Polar);
        r.key(pckey::F6);
        double v;
        CHECK(t && t->columns().size() == 1 && ws.table.rows.size() == 4 && t->cell(r.ctx(), 2, 0, v) && near(v, 1, 1e-14));
        r.key(pckey::F1);
        r.key(pckey::F3); r.type("3");                                // TYPE: Param
        CHECK(ws.table.funcType == GraphType::Param && !ws.table.generated);
        r.key(pckey::F6);
        CHECK(t->columns().size() == 2 && t->cell(r.ctx(), 3, 0, v) && std::abs(v) < 1e-14 && t->cell(r.ctx(), 3, 1, v) && near(v, 2, 1e-14));
        // a new line takes the table type: Xt / Yt
        r.key(pckey::F1);
        r.key(pckey::Home); r.keys(pckey::Down, 3);                   // r1, Xt2, Yt2, then line 3
        r.enter("t"); r.enter("t^2");
        CHECK(ws.functions[2].type == GraphType::Param && ws.functions[2].expr == "t" && ws.functions[2].expr2 == "(t)^(2)");
        r.key(pckey::F3); r.type("1");                                // back to Y=
        ws.settings.angle = AngleMode::Radians;
    }
    // ================================================================ STYLE, SELECT, DELETE function, limits
    {
        TA* t = r.tableWith({"x", "x^2"}, 0, 998, 1);
        r.key(pckey::Home); r.key(pckey::Down);
        r.key(pckey::F4); r.type("3");                                // STYLE: Green
        CHECK(ws.functions[1].color == 2);
        r.key(pckey::F1);                                             // SELECT: Y2 off
        CHECK(!ws.functions[1].selected);
        r.key(pckey::F6);
        CHECK(t && t->columns().size() == 1 && ws.table.rows.size() == 999);   // the largest table
        r.key(pckey::End);
        double v;
        CHECK(t->row() == 998 && t->cell(r.ctx(), 998, 0, v) && v == 998);
        r.key(pckey::F3); r.type("3");                                // ADD: over 999 rows
        CHECK(r.error() && r.errorTitle() == "Memory ERROR"); r.closeError();
        r.key(pckey::F8); r.type("1"); r.enter("1");                  // LISTMEM: lists hold 255 values
        CHECK(r.error() && r.errorTitle() == "Dimension ERROR"); r.closeError();
        r.key(pckey::F1);
        r.key(pckey::F5); r.enter("0"); r.enter("999"); r.enter("1"); r.key(pckey::Esc);   // 1000 rows
        CHECK(r.error() && r.errorTitle() == "Memory ERROR"); r.closeError();
        CHECK(t->screen() == TA::Screen::Set);
        r.key(pckey::Esc);                                            // the stored range is back: EXIT leaves
        CHECK(!r.error() && t->screen() == TA::Screen::List && ws.table.end == 998);
        r.key(pckey::Home); r.key(pckey::Down);
        r.key(pckey::F2); r.key(pckey::F1);                           // DELETE Y2
        CHECK(ws.functions[1].empty());
        r.key(pckey::Up);
        r.key(pckey::F2); r.key(pckey::F1);                           // DELETE Y1
        r.key(pckey::F6);                                             // no function: error
        CHECK(r.error()); r.closeError();
    }
    // ================================================================ persistence
    {
        TA* t = r.tableWith({"x^2+1"}, -1, 1, 0.5);
        r.key(pckey::F6);
        r.key(pckey::F4); r.enter("10");                              // edited first row
        ws.table.variable = TableRange::Variable::Range;
        CHECK(t && ws.table.rows.front() == 10);
        r.fw.saveNow();
        {
            Rig again;
            const Workspace& w = again.ws();
            CHECK(w.table.generated && w.table.rows.size() == 5 && w.table.rows.front() == 10 && w.table.start == -1 && w.table.step == 0.5);
            again.type("7");
            TA* t2 = again.table();
            double v;
            CHECK(t2 && t2->screen() == TA::Screen::Table && t2->cell(again.ctx(), 0, 0, v) && v == 101);
        }
        // a version 3 record (v1.8.0 before TABLE): the table block without the new fields
        Workspace w3;
        w3.table.start = -3.25; w3.table.end = 3.75; w3.table.step = 0.125;
        auto blob = serializeWorkspace(w3);
        std::uint8_t pattern[24];
        std::memcpy(pattern, &w3.table.start, 8); std::memcpy(pattern + 8, &w3.table.end, 8); std::memcpy(pattern + 16, &w3.table.step, 8);
        const auto at = std::search(blob.begin(), blob.end(), pattern, pattern + 24);
        CHECK(at != blob.end());
        if (at != blob.end()) {
            blob.erase(at + 24, at + 24 + 6);                         // variable, list, type, generated, row count
            const std::uint16_t three = 3;
            std::memcpy(blob.data() + 4, &three, 2);
            Workspace back;
            CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
            CHECK(back.table.start == -3.25 && back.table.step == 0.125 && back.table.variable == TableRange::Variable::Range);
            CHECK(back.table.funcType == GraphType::Y && back.table.rows.empty() && !back.table.generated);
        }
        // .pcgraph: the table settings travel with the project
        graphfile::Project p;
        p.tableFromList = true; p.tableList = 4; p.tableFunc = GraphType::Param;
        graphfile::Project q;
        CHECK(graphfile::decode(graphfile::encode(p), q).empty() && q.tableFromList && q.tableList == 4 && q.tableFunc == GraphType::Param);
        std::string text = graphfile::encode(graphfile::Project{});
        CHECK(text.find("tablevar=Range") != std::string::npos && text.find("tablefunc=Y=") != std::string::npos);
    }
    return testutil::finish("table_test");
}
