// v1.18.0: GRAPH alla parita' completa con la fx-CG50 (manuale 5-1...5-41,
// 5-52...5-63; Setup 1-35...1-37). Un gruppo per ogni voce della tabella di
// parita' chiusa nella v1.18.0 (docs/reports/ANALISI_FONTI_v1.18.0.md, §3):
// Dual Screen G+G e GtoT, voci piccole, errori E1-E5, immagini a due livelli,
// cartelle, funzioni predefinite di Run-Matrix. Gira su entrambi i profili
// (lo stesso codice sulle due schede).
#include "apps.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "file_store.hpp"
#include "firmware.hpp"
#include "graph_commands.hpp"
#include "graph_engine.hpp"
#include "graph_view.hpp"
#include "pict.hpp"
#include "test_util.hpp"
#include <cmath>
#include <cstdio>
#include <string>

using namespace fx;
using app::AppId;
using GA = app::GraphApp;

namespace {
std::uint16_t press(std::uint8_t code, std::uint8_t state = 1) { return static_cast<std::uint16_t>((state << 8) | code); }
struct Rig {
    BoardPlatform platform;
    Firmware fw{platform, board::storageFlash()};
    Rig() { fw.boot(); settle(); }
    void settle(int max = 4000) {
        for (int i = 0; i < max; ++i) {
            if (!fw.step()) break;
            fw.draw(board::graphics()); board::present();
            board::host::advance(5);
        }
    }
    void raw(std::uint16_t r) { fw.rawKey(r); board::host::advance(20); if (fw.step()) { fw.draw(board::graphics()); board::present(); } }
    void key(std::uint8_t code) { raw(press(code)); raw(press(code, 3)); }
    void keys(std::uint8_t code, int n) { for (int i = 0; i < n; ++i) key(code); }
    void steps(std::uint8_t code, int n) { for (int i = 0; i < n; ++i) { board::host::advance(250); key(code); } }
    void type(const std::string& s) { for (char c : s) raw(press(static_cast<std::uint8_t>(c))); }
    void enter(const std::string& s) { type(s); key(pckey::Enter); }
    void ctrl(char c) { raw(press(pckey::ModCtrl)); raw(press(static_cast<std::uint8_t>(c))); raw(press(pckey::ModCtrl, 3)); }
    void shift() { key(pckey::F7); }
    GA* graph() { return dynamic_cast<GA*>(fw.apps().active()); }
    app::TableApp* table() { return dynamic_cast<app::TableApp*>(fw.apps().active()); }
    bool error() { return fw.apps().errorShown(); }
    std::string errorTitle() { return fw.apps().errorTitle(); }
    void closeError() { if (error()) key(pckey::Esc); }
    Workspace& ws() { return fw.workspace(); }
    app::Context& ctx() { return fw.apps().context(); }
    void home() { key(pckey::Tab); }
    // GRAPH on its relation list with the given Y= functions and the INITIAL window.
    GA* graphWith(std::initializer_list<const char*> ys) {
        home();
        for (std::size_t i = 0; i < ws().functions.size(); ++i) ws().functions[i] = emptyGraphFunction(i);
        std::size_t i = 0;
        for (const char* y : ys) { ws().functions[i].expr = y; ws().functions[i].type = GraphType::Y; ++i; }
        ws().view = ViewWindow{};
        ws().graph.dual = GraphDual{};
        ws().graph.sketch.clear();
        ws().graph.settings = GraphSettings{};
        ws().settings.angle = AngleMode::Radians;
        type("5");
        key(pckey::Home);                                            // the cursor on Y1
        return graph();
    }
    void draw() { key(pckey::F6); settle(); }
    // Setup of GRAPH, "Dual Screen" (row 13): Right = G+G, Left = GtoT, from Off.
    void dualScreen(int rights) { key(pckey::F10); keys(pckey::Down, 12); for (int i = 0; i < (rights > 0 ? rights : -rights); ++i) key(rights > 0 ? pckey::Right : pckey::Left); key(pckey::Esc); }
    std::uint16_t pixel(int x, int y) { return board::graphics().getPixel(x, y); }
};
bool near(double a, double b, double tol) { return std::abs(a - b) <= tol; }
bool hasInk(Rig& r, int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) if (r.pixel(x, y) != 0xFFFF) return true;
    return false;
}
} // namespace

int main() {
    board::init();
    files::RamFileSystem card(4u * 1024u * 1024u);
    board::host::setFileSystem(&card);
    Rig r;
    Workspace& ws = r.ws();

    // ================================================================ G02, G03: EXE draws, -> edits, AC returns (5-2, 5-17)
    {
        GA* g = r.graphWith({"x^2"});
        CHECK(g && g->screen() == GA::Screen::List);
        r.key(pckey::Enter);                                         // EXE on the relation list: DRAW
        CHECK(g->screen() == GA::Screen::Plot);
        r.key(pckey::Break);                                         // AC: back to the relation list
        CHECK(g->screen() == GA::Screen::List);
        r.key(pckey::Right);                                         // ->: the cursor in the expression
        CHECK(g->screen() == GA::Screen::Edit);
        r.key(pckey::Esc);
    }
    // ================================================================ G08: OPTN > ANGLE in the editor, Y1 = sin x^r with Deg (5-4)
    {
        GA* g = r.graphWith({});
        ws.settings.angle = AngleMode::Degrees;
        r.type("sin(x");
        r.key(pckey::F8); r.keys(pckey::Down, 12); r.key(pckey::Enter);   // INSERISCI > ANGLE
        r.type("2");                                                 // r
        r.key(pckey::Right); r.key(pckey::Enter);
        CHECK(ws.functions[0].expr.find("\xCA\xB3") != std::string::npos);
        r.draw();
        double y = 0;
        const auto& plots = g->engine().plots();
        CHECK(!plots.empty() && g->engine().eval(plots[0], 3.14159265358979 / 2, y) && near(y, 1, 1e-12));
        // G-Solve ROOT in a window around pi: the radian root, not 180 degrees
        ws.view.xmin = 2; ws.view.xmax = 4;
        r.key(pckey::F5); r.type("1"); r.settle();
        CHECK(!g->solutions().empty() && near(g->solutions()[0].first, 3.14159265358979, 1e-6));
        r.key(pckey::Esc);
        ws.settings.angle = AngleMode::Radians;
    }
    // ================================================================ G12: FORMAT with the number keys (5-16)
    {
        GA* g = r.graphWith({"x"});
        r.key(pckey::F8); r.type("1");                               // OPTN > FORMAT
        CHECK(g->screen() == GA::Screen::Format);
        r.type("3");                                                 // Line Style 3: Broken
        r.key(pckey::Down); r.type("2");                             // Line Color 2: Red
        r.key(pckey::Esc);
        CHECK(ws.functions[0].style == LineStyle::Broken && ws.functions[0].color == 1);
    }
    // ================================================================ G14 / E3: FORMAT of pasted graphs, not of overwrite graphs (5-28, 5-31)
    {
        GA* g = r.graphWith({"Ax^2,[A=1,2]"});
        r.draw();
        r.ctx().clipboard = std::make_shared<ast::Node>(*parseExpression("x+1"));
        r.ctrl('v');                                                 // paste Y=x+1: blue, Normal
        r.settle();
        const auto& plots = g->engine().plots();
        CHECK(plots.size() == 3 && plots[2].pasted && plots[2].color == 0 && plots[2].style == LineStyle::Normal);
        r.key(pckey::F8); r.type("5");                               // OPTN > FORMAT
        // the overwrite graphs are not offered: only the pasted graph, chosen at once (5-16, one graph)
        CHECK(g->screen() == GA::Screen::Format);
        r.type("2"); r.key(pckey::Down); r.type("3"); r.key(pckey::Esc);   // Thick, Green
        r.settle();
        const auto& after = g->engine().plots();
        CHECK(after.size() == 3 && after[2].pasted && after[2].style == LineStyle::Thick && after[2].color == 2);
        CHECK(after[1].style == LineStyle::Normal);                  // A=2 unchanged
    }
    // ================================================================ G32: scrolling with the cursor keys
    {
        r.graphWith({"x"});
        r.draw();
        const double w = ws.view.xmax - ws.view.xmin, h = ws.view.ymax - ws.view.ymin;
        r.key(pckey::Right);
        CHECK(near(ws.view.xmin, -6.3 + w / 8, 1e-9));
        r.key(pckey::Up);
        CHECK(near(ws.view.ymin, -3.1 + h / 8, 1e-9));
    }
    // ================================================================ G35: Fade I/O slider in steps of 5 %, stored with the background (5-12)
    {
        GA* g = r.graphWith({"x"});
        r.draw();
        r.key(pckey::F8); r.type("1"); r.type("1"); r.enter("1");    // PICTURE > STORE Pict 1
        r.settle();
        CHECK(card.exists("PICOCALC/PICT/PICT01.pcpict"));
        ws.graph.settings.background = 1;
        r.home(); r.type("5"); r.draw();
        r.key(pckey::F8); r.type("3");                               // FadeI/O
        CHECK(g->fadeSliderOpen());
        r.key(pckey::Right); r.key(pckey::Right); r.key(pckey::Right);
        CHECK(ws.graph.settings.fade == 15);
        r.key(pckey::Left);
        CHECK(ws.graph.settings.fade == 10);
        r.key(pckey::Esc);
        CHECK(!g->fadeSliderOpen());
        std::vector<std::uint8_t> bytes;
        pict::Image img;
        CHECK(files::readAll(card, "PICOCALC/PICT/PICT01.pcpict", bytes, 1 << 20) == files::Status::Ok);
        CHECK(pict::parse(bytes.data(), bytes.size(), img).empty() && img.fade == 10);
        ws.graph.settings.background = 0; ws.graph.settings.fade = 0;
    }
    // ================================================================ G36: two layers in a picture (5-20), Fade only on the background layer
    {
        r.graphWith({"0"});
        ws.graph.settings.grid = GridMode::Line;
        r.draw();
        r.key(pckey::F8); r.type("1"); r.type("1"); r.enter("2");    // STORE Pict 2
        r.settle();
        std::vector<std::uint8_t> bytes;
        pict::Image img;
        CHECK(files::readAll(card, "PICOCALC/PICT/PICT02.pcpict", bytes, 1 << 20) == files::Status::Ok);
        CHECK(bytes.size() > 10 && bytes[8] == 2);                   // version 2
        CHECK(pict::parse(bytes.data(), bytes.size(), img).empty() && img.layered());
        // y = 0 is in the graph layer, the grid lines in the background layer
        int graphInk = 0, backInk = 0;
        pict::forEachRun(img.graphLayer, img.width, img.height, [&](int, int y, int n, std::uint16_t c) { if (c != 0xFFFF && y >= 138 && y <= 144) graphInk += n; });
        pict::forEachRun(img.backLayer, img.width, img.height, [&](int, int, int n, std::uint16_t c) { if (c != 0xFFFF) backInk += n; });
        CHECK(graphInk > 200 && backInk > 200);
        // as background with Fade 100 %: the background layer disappears, the graph stays
        ws.graph.settings.grid = GridMode::Off; ws.graph.settings.axes = AxesMode::Off;
        ws.functions[0] = emptyGraphFunction(0);
        ws.graph.settings.background = 2; ws.graph.settings.fade = 100;
        r.home(); r.type("5"); r.draw();
        const int row = graphgeo::Top + 140;
        CHECK(hasInk(r, 100, row - 1, 110, row + 1));                // the curve of the picture, not faded
        bool gridVisible = false;
        for (int y = graphgeo::Top + 2; y < graphgeo::Top + 100; ++y) if (r.pixel(5, y) != 0xFFFF) gridVisible = true;
        CHECK(!gridVisible);
        ws.graph.settings.background = 0; ws.graph.settings.fade = 0; ws.graph.settings.axes = AxesMode::On;
        // single-layer images keep the version 1 bytes
        pict::Image one;
        std::vector<std::uint16_t> px(4, 0x1234);
        CHECK(pict::encode(px.data(), 2, 2, 2, one));
        const auto v1 = pict::serialize(one);
        CHECK(v1[8] == 1 && v1[14] == 1);
        // a damaged layer is refused
        auto bad = bytes;
        bad[100] ^= 0x55;
        pict::Image refused;
        CHECK(!pict::parse(bad.data(), bad.size(), refused).empty());
    }
    // ================================================================ G38: folders of pictures (5-10, 5-21, 5-22)
    {
        GA* g = r.graphWith({"x"});
        r.draw();
        r.key(pckey::F8); r.type("1"); r.type("2");                  // PICTURE > SAVE•AS: folder list
        CHECK(g->screen() == GA::Screen::Files && g->filesFolder() == files::PictFolder);
        r.key(pckey::F5); r.enter("LAB");                            // new folder
        CHECK(card.exists("PICOCALC/PICT/LAB"));
        r.key(pckey::Home); r.key(pckey::Enter);                     // the folder is listed first: open it
        CHECK(g->filesFolder() == "PICOCALC/PICT/LAB");
        r.key(pckey::F1); r.enter("WAVE");                           // SAVE•AS in the folder
        r.settle();
        CHECK(card.exists("PICOCALC/PICT/LAB/WAVE.pcpict"));
        r.key(pckey::F8); r.type("1"); r.type("4");                  // OPEN: root
        r.key(pckey::Home); r.key(pckey::Enter);                     // into LAB
        CHECK(g->filesFolder() == "PICOCALC/PICT/LAB");
        r.key(pckey::Enter);                                         // RECALL WAVE
        CHECK(ws.graph.overlay == "PICOCALC/PICT/LAB/WAVE.pcpict");
        r.key(pckey::F4); r.type("1"); r.key(pckey::F1);             // Cls
    }
    // ================================================================ G41-G48: Dual Screen G+G (5-23, 5-24), D1/D2
    {
        GA* g = r.graphWith({"x(x+1)(x-1)", "x^2"});
        r.dualScreen(1);
        CHECK(ws.graph.dual.mode == DualScreen::GraphGraph);
        // G42: V-Window of the main screen, then F6 SOTTO for the sub-screen
        r.shift(); r.key(pckey::F3);
        CHECK(g->screen() == GA::Screen::Window && !g->windowEditsSub());
        r.enter("-2"); r.enter("2");
        r.key(pckey::F6);
        CHECK(g->windowEditsSub());
        r.enter("-4"); r.enter("4");
        r.key(pckey::Esc);
        CHECK(ws.view.xmin == -2 && ws.view.xmax == 2 && ws.graph.dual.sub.xmin == -4 && ws.graph.dual.sub.xmax == 4);
        r.draw();
        CHECK(g->builtLayout() == GA::Layout::GraphGraph);
        CHECK(!g->curves()[0].xy.empty() && g->subCurves().size() == 2 && g->subCurves()[0].xy.empty());
        // the main graph is drawn above the separator, nothing below yet
        for (std::size_t i = 0; i + 1 < g->curves()[0].xy.size(); i += 2)
            if (g->curves()[0].xy[i] != graph::PenUp) CHECK(g->curves()[0].xy[i + 1] <= graphgeo::Top + graphgeo::SplitRows + 3);
        // G43: COPY: both screens, "E"
        r.key(pckey::F8); r.type("1"); r.settle();
        CHECK(ws.graph.dual.side[0] == DualSide::Both && ws.graph.dual.side[1] == DualSide::Both);
        CHECK(!g->subCurves()[0].xy.empty());
        for (std::size_t i = 0; i + 1 < g->subCurves()[0].xy.size(); i += 2)
            if (g->subCurves()[0].xy[i] != graph::PenUp) CHECK(g->subCurves()[0].xy[i + 1] >= graphgeo::SubTop - 3);
        // G45: SELECT on an "E" line clears the indicator, the graph stays selected
        r.key(pckey::Break);                                         // G48: AC -> relation list
        CHECK(g->screen() == GA::Screen::List);
        r.key(pckey::Home); r.key(pckey::Down);
        r.key(pckey::F1);
        CHECK(ws.graph.dual.side[1] == DualSide::Main && ws.functions[1].selected);
        // G44: SWAP: Y2 (main) goes to the sub-screen, the V-Windows are exchanged
        r.draw();
        r.key(pckey::F8); r.type("2"); r.settle();
        CHECK(ws.graph.dual.side[1] == DualSide::Sub && ws.graph.dual.side[0] == DualSide::Both);
        CHECK(ws.view.xmin == -4 && ws.graph.dual.sub.xmin == -2);
        // G46: FORMAT is refused on an "S" line of the list
        r.key(pckey::Esc);
        r.key(pckey::Home); r.key(pckey::Down);
        r.key(pckey::F8); r.type("1");
        CHECK(r.error()); r.closeError();
        // G47: TRACE on the main screen only (Y2 is in the sub-screen)
        r.draw();
        r.key(pckey::F1);
        CHECK(g->mode() == GA::Mode::Trace);
        r.key(pckey::Down);
        double x = 0, y = 0;
        CHECK(g->cursor(x, y) && near(y, x * (x + 1) * (x - 1), 1e-9));   // still Y1
        r.key(pckey::F1);
        // D2: ZOOM IN starts from the main V-Window and writes the sub-screen one
        const ViewWindow mainBefore = ws.view;
        r.key(pckey::F2); r.type("3"); r.key(pckey::Enter);          // ZOOM > IN at the centre
        CHECK(ws.view == mainBefore);
        CHECK(near(ws.graph.dual.sub.xmax - ws.graph.dual.sub.xmin, (mainBefore.xmax - mainBefore.xmin) / 2, 1e-9));
        r.key(pckey::F2); r.type("6"); r.type("5");                   // ZOOM > PREVIOUS: the sub-screen again
        // G31: PAN refused in G+G (5-9); G39: no picture of a split screen (5-21); G53: no paste (5-31)
        r.key(pckey::F8); r.type("4");
        CHECK(r.error()); r.closeError();
        r.key(pckey::F8); r.type("3"); r.type("1");
        CHECK(r.error()); r.closeError();
        r.ctx().clipboard = std::make_shared<ast::Node>(*parseExpression("x"));
        r.ctrl('v');
        CHECK(r.error()); r.closeError();
        // G57: MODIFY needs Dual Screen Off (5-39)
        r.key(pckey::Esc);
        ws.functions[2].expr = "Ax"; ws.functions[2].type = GraphType::Y;
        r.key(pckey::F5);
        CHECK(r.error()); r.closeError();
        ws.functions[2] = emptyGraphFunction(2);
        // S10: the Setup item returns to Off
        r.dualScreen(1);                                             // G+G -> GtoT -> Off: Right twice from G+G
        r.dualScreen(1);
        CHECK(ws.graph.dual.mode == DualScreen::Off);
    }
    // ================================================================ heap of the Dual Screen only while Graph is open
    {
        GA* g = r.graphWith({"x", "x^2"});
        r.dualScreen(1);
        r.draw();
        r.key(pckey::F8); r.type("1"); r.settle();                    // COPY: curves in the sub-screen
        CHECK(g->builtLayout() == GA::Layout::GraphGraph && !g->subCurves().empty() && !g->subCurves()[0].xy.empty());
        r.home();
        CHECK(g->subCurves().empty() && g->subCurves().capacity() == 0 && g->curves().capacity() == 0);
        ws.graph.dual = GraphDual{};
    }
    // ================================================================ G72: Graph to Table (5-55), S03 Derivative in the table
    {
        GA* g = r.graphWith({"x^2-3", "-x+1"});
        r.dualScreen(-1);
        CHECK(ws.graph.dual.mode == DualScreen::GraphTable);
        r.draw();
        CHECK(g->builtLayout() == GA::Layout::GraphTable);
        r.key(pckey::F1);                                            // TRACE on Y1
        r.key(pckey::Enter);                                         // store the point
        r.steps(pckey::Right, 10);
        r.key(pckey::Enter);
        CHECK(ws.graph.dual.gtot.size() == 2 && ws.graph.dual.gtotType == GraphType::Y);
        const double x0 = ws.graph.dual.gtot[0];
        r.key(pckey::F1);
        r.key(pckey::F8); r.type("1");                               // OPTN > CHANGE: the table is active
        CHECK(g->tableActive());
        r.key(pckey::Right);                                         // column Y1
        r.key(pckey::F8); r.type("2"); r.enter("3");                 // LISTMEM column Y1 -> List 3
        CHECK(ws.lists[2].size() == 2 && near(ws.lists[2][0], x0 * x0 - 3, 1e-9));
        r.key(pckey::Up);
        r.key(pckey::F1);                                            // R•DEL
        CHECK(ws.graph.dual.gtot.size() == 1);
        r.key(pckey::F2); r.key(pckey::F1);                          // DEL•ALL, Yes
        CHECK(ws.graph.dual.gtot.empty());
        r.key(pckey::Esc);
        CHECK(!g->tableActive());
        // a point of another table type while the table holds Y= points: Argument ERROR
        ws.graph.dual.gtot = {1.0};
        ws.functions[0].type = GraphType::Polar; ws.functions[0].expr = "2";
        ws.functions[1] = emptyGraphFunction(1);
        r.home(); r.type("5"); r.draw();
        r.key(pckey::F1); r.key(pckey::Enter);
        CHECK(r.error()); r.closeError();
        // Derivative On: dY/dX columns in the GtoT table
        ws.functions[0] = emptyGraphFunction(0); ws.functions[0].expr = "x^2";
        ws.graph.dual.gtot = {2.0};
        ws.graph.settings.derivative = true;
        r.home(); r.type("5"); r.draw();
        r.key(pckey::F8); r.type("1");
        CHECK(g->tableActive());
        r.keys(pckey::Right, 2);                                     // Y1'
        CHECK(g->gtotCursor().col == 1);
        CHECK(!g->gtotTable().empty());
        // the Dual Screen buffers are released when Graph is left (heap only while open),
        // and the graph is active again when Graph is entered
        r.home();
        CHECK(g->gtotTable().empty() && g->gtotTable().capacity() == 0 && !g->tableActive());
        ws.graph.settings.derivative = false;
        ws.graph.dual = GraphDual{};
    }
    // ================================================================ G50: built-in function graphs of Run-Matrix (5-26)
    {
        ws.graph.commandGraphs.clear();
        graphcmd::execute(ws, "Graph Y=sin");
        graphcmd::execute(ws, "Graph r=cos");
        graphcmd::execute(ws, "Graph Y=^2");
        CHECK(ws.graph.commandGraphs.size() == 3);
        CHECK(ws.graph.commandGraphs[0].expr == "sin(x)" && ws.graph.commandGraphs[1].expr == "cos(theta)" && ws.graph.commandGraphs[2].expr == "(x)^(2)");
        bool threw = false;
        try { graphcmd::execute(ws, "Graph Y=3+"); } catch (const std::exception&) { threw = true; }
        CHECK(threw);                                                // not a built-in: still a Syntax ERROR
        ws.graph.commandGraphs.clear();
        ws.graph.commandScreen = false;                              // as GRAPH does when it is left
    }
    // ================================================================ G51 / E2, G52: cyan / yellow in overwrite and List graphs (5-28, 5-30)
    {
        Workspace w;
        for (std::size_t i = 0; i < w.functions.size(); ++i) w.functions[i] = emptyGraphFunction(i);
        w.functions[2].expr = "Ax,[A=1,2,3]"; w.functions[2].color = 5;      // cyan on line 3
        w.functions[3].expr = "List1x"; w.functions[3].color = 6;            // yellow on line 4
        w.lists[0] = {1, 2};
        graph::GraphEngine e;
        e.build({&w.functions, &w.memory, &w, &w.lists});
        const auto& p = e.plots();
        CHECK(p.size() == 5);
        CHECK(p[0].color == defaultGraphColor(2) && p[1].color == (defaultGraphColor(2) + 1) % 5);
        CHECK(p[3].color == defaultGraphColor(3) && p[4].color == (defaultGraphColor(3) + 1) % 5);
        // the variable of an overwrite cannot be X, Y, r, θ or T (5-28)
        w.functions[2].expr = "Tx,[T=1,2]";
        graph::GraphEngine e2;
        e2.build({&w.functions, &w.memory, &w, &w.lists});
        CHECK(!e2.error(2).empty());
    }
    // ================================================================ G61 / E1, G62: Tangent with the Setup style; FORMAT during SKETCH (5-52, 5-53)
    {
        GA* g = r.graphWith({"x^2"});
        ws.graph.settings.plotColor = 4; ws.graph.settings.sketchLine = LineStyle::Thin;
        r.draw();
        r.key(pckey::F4); r.type("2");                               // SKETCH > Tangent
        CHECK(g->mode() == GA::Mode::Trace);
        r.key(pckey::Enter);
        CHECK(ws.graph.sketch.size() == 1 && ws.graph.sketch[0].color == 4 && ws.graph.sketch[0].style == LineStyle::Thin);
        r.key(pckey::F8);                                            // FORMAT during the operation
        CHECK(g->screen() == GA::Screen::Format);
        r.type("3"); r.key(pckey::Down); r.type("2"); r.key(pckey::Esc);   // Broken, Red
        CHECK(g->screen() == GA::Screen::Plot && g->mode() == GA::Mode::Trace);
        r.steps(pckey::Right, 5);
        r.key(pckey::Enter);                                         // next tangent in succession
        CHECK(ws.graph.sketch.size() == 2 && ws.graph.sketch[1].color == 1 && ws.graph.sketch[1].style == LineStyle::Broken);
        r.key(pckey::Esc);
        // Plot: FORMAT in the pointer mode
        r.key(pckey::F4); r.type("5"); r.type("1");                  // SKETCH > PLOT > Plot
        CHECK(g->mode() == GA::Mode::Pointer);
        r.key(pckey::F8); r.key(pckey::Down); r.type("3"); r.key(pckey::Esc);   // colour Green
        r.key(pckey::Enter);
        CHECK(ws.graph.sketch.size() == 3 && ws.graph.sketch[2].color == 2);
        r.key(pckey::Esc);
        r.key(pckey::F4); r.type("5"); r.type("1"); r.key(pckey::Enter);   // a new operation starts from the Setup colour
        CHECK(ws.graph.sketch.size() == 4 && ws.graph.sketch[3].color == 4);
        r.key(pckey::Esc);
        ws.graph.settings = GraphSettings{};
        ws.graph.sketch.clear();
    }
    // ================================================================ G65, G66, G67, G69: TRACE (5-54)
    {
        GA* g = r.graphWith({"x^3-4x"});
        r.draw();
        r.key(pckey::F1);
        r.settle();
        CHECK(!g->pointerLabel().empty() && g->pointerLabel().front() == '(');   // coordinates next to the pointer
        CHECK(g->traceLines());
        r.shift(); r.key(pckey::Down);                               // SHIFT down hides the supplementary lines
        CHECK(!g->traceLines());
        r.shift(); r.key(pckey::Up);
        CHECK(g->traceLines());
        r.type("x");                                                 // the X,θ,T key opens the pop-up
        r.enter("1.5");
        double x = 0, y = 0;
        CHECK(g->cursor(x, y) && x == 1.5 && near(y, -2.625, 1e-12));
        ws.graph.settings.coord = false;
        r.key(pckey::Right);
        CHECK(g->pointerLabel().empty());                            // Coord Off
        ws.graph.settings.coord = true;
        r.key(pckey::F1);
        // Y> : "X:" and "Y:"
        ws.functions[0].type = GraphType::YGreater;
        r.home(); r.type("5"); r.draw();
        r.key(pckey::F1);
        CHECK(g->infoText().find("X:") == 0 && g->infoText().find("Y:") != std::string::npos);
        r.key(pckey::F1);
        ws.functions[0].type = GraphType::YGreaterEq;
        r.home(); r.type("5"); r.draw();
        r.key(pckey::F1);
        CHECK(g->infoText().find("X=") == 0);
        r.key(pckey::F1);
    }
    // ================================================================ G79, G80: ∫dx ROOT / INTSECT with 21 or more roots (5-60)
    {
        GA* g = r.graphWith({"sin(10x)"});
        ws.view.xmin = -6.3; ws.view.xmax = 6.3;
        r.draw();
        r.key(pckey::F5); r.type("8"); r.type("2");                  // ∫dx > ROOT
        CHECK(g->mode() == GA::Mode::Integral);
        r.key(pckey::Enter);                                         // leftmost root
        r.keys(pckey::Right, 22);                                    // 22 roots further right
        r.key(pckey::Enter);
        CHECK(r.error() && r.errorTitle() == "Math ERROR"); r.closeError();
        r.key(pckey::Esc);
        // ∫dx INTSECT: 21 or more intersections of sin(10x) and 0
        ws.functions[1].expr = "0"; ws.functions[1].type = GraphType::Y; ws.functions[1].selected = true;
        r.home(); r.type("5"); r.draw();
        r.key(pckey::F5); r.type("8"); r.type("3");                  // ∫dx > INTSECT (two graphs: chosen at once)
        CHECK(g->mode() == GA::Mode::Integral);
        r.key(pckey::Enter);
        r.keys(pckey::Right, 22);
        r.key(pckey::Enter);
        CHECK(r.error() && r.errorTitle() == "Math ERROR"); r.closeError();
        r.key(pckey::Esc);
        // with 3 intersections only, the value and the area are computed
        r.key(pckey::F5); r.type("8"); r.type("3");
        r.key(pckey::Enter); r.keys(pckey::Right, 2); r.key(pckey::Enter);
        CHECK(!r.error() && std::isfinite(g->integralValue()) && g->areaValue() > 0);
        r.key(pckey::Esc);
    }
    return testutil::finish("graph_v118_test");
}
