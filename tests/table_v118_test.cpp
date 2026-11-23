// v1.18.0: TABLE alla parita' completa con la fx-CG50 (manuale 5-32...5-37,
// 5-9; Setup 1-35...1-37): T+G (Dual Screen di TABLE), FORMAT e STYLE della
// lista (E6), AC dopo GPH-CON, TRACE sulle righe della tabella (E4), PAN sul
// grafico di TABLE, ANGLE nell'editor, LISTMEM a 999 su entrambe le schede.
// Un gruppo per voce della tabella di parita' (docs/reports/ANALISI_FONTI_v1.18.0.md).
#include "apps.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "firmware.hpp"
#include "graph_view.hpp"
#include "limits.hpp"
#include "test_util.hpp"
#include <cmath>
#include <string>

using namespace fx;
using app::AppId;
using TA = app::TableApp;
using GA = app::GraphApp;

namespace {
std::uint16_t press(std::uint8_t code, std::uint8_t state = 1) { return static_cast<std::uint16_t>((state << 8) | code); }
struct Rig {
    BoardPlatform platform;
    Firmware fw{platform, board::storageFlash()};
    Rig() { fw.boot(); settle(); }
    void settle(int max = 4000) {
        for (int i = 0; i < max; ++i) { if (!fw.step()) break; fw.draw(board::graphics()); board::present(); board::host::advance(5); }
    }
    void raw(std::uint16_t r) { fw.rawKey(r); board::host::advance(20); if (fw.step()) { fw.draw(board::graphics()); board::present(); } }
    void key(std::uint8_t code) { raw(press(code)); raw(press(code, 3)); }
    void keys(std::uint8_t code, int n) { for (int i = 0; i < n; ++i) key(code); }
    void type(const std::string& s) { for (char c : s) raw(press(static_cast<std::uint8_t>(c))); }
    void enter(const std::string& s) { type(s); key(pckey::Enter); }
    bool error() { return fw.apps().errorShown(); }
    void closeError() { if (error()) key(pckey::Esc); }
    Workspace& ws() { return fw.workspace(); }
    TA* table() { return dynamic_cast<TA*>(fw.apps().active()); }
    GA* graph() { return dynamic_cast<GA*>(fw.apps().active()); }
    void home() { key(pckey::Tab); }
    std::uint16_t pixel(int x, int y) { return board::graphics().getPixel(x, y); }
    TA* tableWith(std::initializer_list<const char*> ys, double start, double end, double step) {
        home();
        for (std::size_t i = 0; i < ws().functions.size(); ++i) ws().functions[i] = emptyGraphFunction(i);
        std::size_t i = 0;
        for (const char* y : ys) { ws().functions[i].expr = y; ws().functions[i].type = GraphType::Y; ++i; }
        ws().table = TableRange{};
        ws().table.start = start; ws().table.end = end; ws().table.step = step;
        ws().view = ViewWindow{};
        type("7");
        key(pckey::Home);
        return table();
    }
};
bool near(double a, double b, double tol) { return std::abs(a - b) <= tol; }
bool inkIn(Rig& r, int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) if (r.pixel(x, y) != 0xFFFF) return true;
    return false;
}
} // namespace

int main() {
    board::init();
    Rig r;
    Workspace& ws = r.ws();

    // ================================================================ T03 / E6: STYLE = line style, FORMAT = style and colour (5-15, 5-33)
    {
        TA* t = r.tableWith({"x^2"}, -3, 3, 1);
        CHECK(t && t->screen() == TA::Screen::List);
        r.key(pckey::F4); r.type("4");                               // STYLE: Dot
        CHECK(ws.functions[0].style == LineStyle::Dot && ws.functions[0].color == 0);
        r.key(pckey::F8); r.type("1"); r.type("2");                  // FORMAT > Line Style > Thick
        CHECK(ws.functions[0].style == LineStyle::Thick);
        r.key(pckey::F8); r.type("2"); r.type("4");                  // FORMAT > Line Color > Magenta
        CHECK(ws.functions[0].color == 3);
        r.key(pckey::F6);                                            // the column takes the colour
        CHECK(t->screen() == TA::Screen::Table && t->columns().size() == 1 && t->columns()[0].color == 3);
    }
    // ================================================================ G08 (TABLE): OPTN > ANGLE in the editor; cell of sin x° with Rad
    {
        TA* t = r.tableWith({}, 0, 90, 90);
        ws.settings.angle = AngleMode::Radians;
        r.type("sin(x");
        r.key(pckey::F8); r.type("1");                               // ANGLE > degrees
        r.key(pckey::Right); r.key(pckey::Enter);
        CHECK(ws.functions[0].expr.find("\xC2\xB0") != std::string::npos);
        r.key(pckey::F6);
        double v = 0;
        CHECK(t->cell(r.fw.apps().context(), 1, 0, v) && near(v, 1, 1e-12));   // sin 90° = 1 in Rad mode
    }
    // ================================================================ T10: AC after GPH-CON returns to the table (5-35); T11 / E4: TRACE on the rows
    {
        TA* t = r.tableWith({"x^2"}, -3, 3, 1);
        r.key(pckey::F6);
        ws.table.rows = {-3, -1, 0.5, 2};                            // edited rows (EDIT / ROW)
        r.key(pckey::F5);                                            // GPH-CON
        GA* g = r.graph();
        CHECK(g && g->screen() == GA::Screen::Plot && g->builtLayout() == GA::Layout::Full);
        r.settle();
        r.key(pckey::F1);                                            // TRACE: from one row to the next
        double x = 0, y = 0;
        CHECK(g->cursor(x, y) && x == -3);
        r.key(pckey::Right);
        CHECK(g->cursor(x, y) && x == -1);
        r.key(pckey::Right);
        CHECK(g->cursor(x, y) && x == 0.5 && near(y, 0.25, 1e-12));
        r.keys(pckey::Right, 3);
        CHECK(g->cursor(x, y) && x == 2);                            // stays on the last row
        r.key(pckey::F1);
        // T12 (5-9): PAN works on the graph of TABLE
        const double xmin = ws.view.xmin;
        r.key(pckey::F8); r.type("2");                               // OPTN > PAN
        CHECK(g->mode() == GA::Mode::Pointer);
        r.key(pckey::Enter); r.key(pckey::Right); r.key(pckey::Enter); r.key(pckey::Esc);
        CHECK(ws.view.xmin < xmin);
        r.key(pckey::Break);                                         // AC: back to the table
        CHECK(r.fw.apps().activeId() == AppId::Table && r.table() && r.table()->screen() == TA::Screen::Table);
        CHECK(ws.table.draw == TableRange::Draw::None);
        (void)t;
    }
    // ================================================================ T12, S17: T+G (5-36, 5-37)
    {
        TA* t = r.tableWith({"3x^2-2"}, -3, 3, 1);
        ws.view.xmin = 0; ws.view.xmax = 6; ws.view.ymin = -2; ws.view.ymax = 10;
        r.key(pckey::F10); r.keys(pckey::Down, 3); r.key(pckey::Left); r.key(pckey::Esc);   // SET UP: Dual Screen T+G
        CHECK(ws.table.dual);
        r.key(pckey::F6);                                            // TABLE: the table in the sub-screen
        CHECK(t->screen() == TA::Screen::Table);
        CHECK(!inkIn(r, 20, graphgeo::Top + 5, 300, graphgeo::Top + graphgeo::SplitRows - 5));   // no graph yet
        CHECK(r.pixel(160, graphgeo::SubTop + 5) != 0xFFFF);        // header of the table below
        r.key(pckey::F5);                                            // GPH-CON: the graph above, active
        GA* g = r.graph();
        CHECK(g && g->builtLayout() == GA::Layout::TableGraph);
        r.settle();
        CHECK(!g->curves()[0].xy.empty());
        for (std::size_t i = 0; i + 1 < g->curves()[0].xy.size(); i += 2)
            if (g->curves()[0].xy[i] != graph::PenUp) CHECK(g->curves()[0].xy[i + 1] <= graphgeo::Top + graphgeo::SplitRows + 3);
        r.key(pckey::F1);                                            // TRACE on the table rows
        r.key(pckey::Right);
        double x = 0, y = 0;
        CHECK(g->cursor(x, y) && x == -2 && y == 10);
        r.key(pckey::F1);
        r.key(pckey::F8); r.type("1");                               // OPTN > CHANGE: the table is active again
        CHECK(r.fw.apps().activeId() == AppId::Table && ws.table.shown == TableRange::Draw::Connect);
        r.settle();
        CHECK(inkIn(r, 1, graphgeo::Top + 2, 300, graphgeo::Top + graphgeo::SplitRows - 2));   // the graph stays drawn
        r.key(pckey::F6);                                            // GPH-PLT, then AC
        r.key(pckey::Break);
        CHECK(r.fw.apps().activeId() == AppId::Table && ws.table.shown == TableRange::Draw::Plot);
        r.key(pckey::F1);                                            // FORMULA: the graph is gone
        CHECK(ws.table.shown == TableRange::Draw::None);
        r.key(pckey::F10); r.keys(pckey::Down, 3); r.key(pckey::Right); r.key(pckey::Esc);  // Dual Screen Off
        CHECK(!ws.table.dual);
    }
    // ================================================================ T09: LISTMEM of 999 values on both boards (D4)
    {
        CHECK(limits::kListMaxLength == 999);
        TA* t = r.tableWith({"x"}, 1, 999, 1);
        r.key(pckey::F6);
        CHECK(t->screen() == TA::Screen::Table && ws.table.rows.size() == 999);
        r.key(pckey::F8); r.type("1"); r.enter("7");                 // LISTMEM: variable column -> List 7
        CHECK(!r.error() && ws.lists[6].size() == 999 && ws.lists[6][998] == 999);
        ws.lists[6].clear();
    }
    return testutil::finish("table_v118_test");
}
