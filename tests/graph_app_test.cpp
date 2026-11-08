// GRAPH mode end to end, driven by PicoCalc key codes through the real
// Firmware: relation list and types, drawing, Trace, ZOOM, V-Window, Setup,
// G-Solve, Sketch, Modify, memories, PICT, TABLE integration and the complete
// save / close / reopen / modify / save cycle of a .pcgraph project.
#include "apps.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "file_store.hpp"
#include "firmware.hpp"
#include "graph_engine.hpp"
#include "graph_file.hpp"
#include "pict.hpp"
#include "test_util.hpp"
#include <cmath>
#include <string>

using namespace fx;
using app::AppId;
using GA = app::GraphApp;

namespace {
constexpr double kPi = 3.14159265358979323846;
std::uint16_t press(std::uint8_t code, std::uint8_t state = 1) { return static_cast<std::uint16_t>((state << 8) | code); }
struct Rig {
    BoardPlatform platform;
    Firmware fw{platform, board::storageFlash()};
    Rig() { fw.boot(); settle(); }
    // Main loop: steps until nothing needs to be redrawn (the incremental drawing is complete).
    void settle(int max = 2000) {
        for (int i = 0; i < max; ++i) {
            if (!fw.step()) break;
            fw.draw(board::graphics()); board::present();
            board::host::advance(5);
        }
    }
    void raw(std::uint16_t r) { fw.rawKey(r); board::host::advance(20); if (fw.step()) { fw.draw(board::graphics()); board::present(); } }
    void key(std::uint8_t code) { raw(press(code)); raw(press(code, 3)); }
    void keys(std::uint8_t code, int n) { for (int i = 0; i < n; ++i) key(code); }
    // Single pointer steps (a held arrow accelerates: keep presses apart).
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
    void home() { key(pckey::Tab); }
    void clearFunctions() { for (std::size_t i = 0; i < ws().functions.size(); ++i) ws().functions[i] = emptyGraphFunction(i); }
    // Opens GRAPH on the relation list with the given Y= functions (set through the workspace).
    GA* graphWith(std::initializer_list<const char*> ys) {
        home(); type("5");
        clearFunctions();
        std::size_t i = 0;
        for (const char* y : ys) { ws().functions[i].expr = y; ws().functions[i].type = GraphType::Y; ++i; }
        ws().view = ViewWindow{};
        return graph();
    }
    void draw() { key(pckey::F6); settle(); }
};
bool near(double a, double b, double tol) { return std::abs(a - b) <= tol; }
} // namespace

int main() {
    board::init();
    files::RamFileSystem card(4u * 1024u * 1024u);
    board::host::setFileSystem(&card);
    Rig r;
    Workspace& ws = r.ws();
    r.type("5");
    GA* g = r.graph();
    CHECK(g && g->screen() == GA::Screen::List);

    // ================================================================ relation list, editor, types
    {
        r.key(pckey::F2); CHECK(!r.error()); r.key(pckey::F1);                          // DELETE Y1 (sin x) > Yes
        CHECK(ws.functions[0].empty());
        r.enter("x+1");                                                                  // typing starts a new Y1
        CHECK_EQ_STR(ws.functions[0].expr, "x+1");
        CHECK(ws.functions[0].type == GraphType::Y && ws.functions[0].selected);
        r.key(pckey::F3); r.type("3");                                                   // TYPE > Param
        CHECK(ws.graph.settings.funcType == GraphType::Param);
        r.enter("2cos(t)");                                                              // Xt2
        r.enter("2sin(t)");                                                              // Yt2
        CHECK(ws.functions[1].type == GraphType::Param);
        CHECK_EQ_STR(ws.functions[1].expr, "2cos(t)");
        CHECK_EQ_STR(ws.functions[1].expr2, "2sin(t)");
        r.key(pckey::F3); r.type("4");                                                   // TYPE > X=
        r.type("y"); r.type("^2"); r.key(pckey::Right); r.key(pckey::Enter);             // X3 = y^2
        CHECK(ws.functions[2].type == GraphType::X && ws.functions[2].expr == "(y)^(2)");
        r.key(pckey::F3); r.type("2");                                                   // TYPE > r=
        r.enter("2");
        CHECK(ws.functions[3].type == GraphType::Polar && ws.functions[3].expr == "2");
        r.key(pckey::F3); r.type("9");                                                   // TYPE > Y<=
        r.enter("x-3");
        CHECK(ws.functions[4].type == GraphType::YLessEq);
        // CONVERT Y1 (x+1) to 'X= : the variable follows the type
        r.key(pckey::Home);
        r.key(pckey::F3); r.type("5"); r.type("6");                                      // CONVERT > 'X=
        CHECK(ws.functions[0].type == GraphType::X);
        CHECK_EQ_STR(ws.functions[0].expr, "y+1");
        r.key(pckey::F3); r.type("5"); r.type("2");                                      // CONVERT > 'Y>
        CHECK(ws.functions[0].type == GraphType::YGreater && ws.functions[0].expr == "x+1");
        // A new function cannot replace one of another type (manual 5-13).
        r.key(pckey::F3); r.type("1");                                                   // Func Type Y=
        r.key(pckey::Down);                                                              // Xt2
        r.enter("x");
        CHECK(r.error());
        r.closeError(); r.key(pckey::Esc);
        CHECK_EQ_STR(ws.functions[1].expr, "2cos(t)");
        // CONVERT is refused for parametric functions.
        r.key(pckey::F3); r.type("5"); r.type("1");
        CHECK(r.error()); r.closeError();
        // SELECT, STYLE, FORMAT colour
        r.key(pckey::Home);
        r.key(pckey::F1); CHECK(!ws.functions[0].selected);
        r.key(pckey::F1); CHECK(ws.functions[0].selected);
        r.key(pckey::F4); r.type("1"); r.type("2");                                      // TOOL > STYLE > Thick
        CHECK(ws.functions[0].style == LineStyle::Thick);
        r.key(pckey::F4); r.type("4");                                                   // TOOL > FORMAT
        CHECK(g->screen() == GA::Screen::Format);
        r.key(pckey::Down); r.key(pckey::Right); r.key(pckey::Right); r.key(pckey::Esc);   // colour +2
        CHECK(ws.functions[0].color == static_cast<std::uint8_t>(GraphColor::Green) && g->screen() == GA::Screen::List);
        // Syntax errors are reported and nothing is stored.
        r.key(pckey::End);
        r.enter("x+*");
        CHECK(r.error() && r.errorTitle() == "Syntax ERROR");
        r.closeError(); r.key(pckey::Esc);
        // Range and overwrite suffixes are accepted by the editor.
        r.key(pckey::F3); r.type("1");
        r.key(pckey::Home); r.keys(pckey::Down, 6);                                      // Y6 (after the Yt2 line)
        r.enter("x,[-2,4]");
        CHECK_EQ_STR(ws.functions[5].expr, "x,[-2,4]");
        r.enter("Ax,[A=1,2,3]");                                                         // storing moved to Y7
        CHECK_EQ_STR(ws.functions[6].expr, "Ax,[A=1,2,3]");
        r.draw();
        CHECK(g->screen() == GA::Screen::Plot && !g->rendering());
        // Y1 (Y>), Param, X=, r=, Y<=, range, 3 overwrite variants
        CHECK(g->engine().plots().size() == 9);
        for (const auto& c : g->curves()) CHECK(c.done);
    }
    // ================================================================ incremental drawing and Trace
    {
        g = r.graphWith({"x^2-2"});
        r.key(pckey::F6);                                         // DRAW without letting the loop finish
        CHECK(g->rendering() || g->curves().size() == 1);
        r.key(pckey::F1);                                         // Trace works while drawing
        CHECK(g->mode() == GA::Mode::Trace);
        r.settle();
        CHECK(!g->rendering());
        double x = 0, y = 0;
        CHECK(g->cursor(x, y));
        const double xdot = 12.6 / 319;
        CHECK_NEAR(x, -6.3 + 160 * xdot, 1e-12);
        CHECK_NEAR(y, x * x - 2, 1e-12);
        r.keys(pckey::Right, 5);
        CHECK(g->cursor(x, y)); CHECK_NEAR(x, -6.3 + 165 * xdot, 1e-12);
        r.enter("1");                                              // X value typed in the pop-up
        CHECK(g->cursor(x, y) && x == 1 && y == -1);
        CHECK(g->infoText().find("X=1") != std::string::npos && g->infoText().find("Y=-1") != std::string::npos);
        ws.graph.settings.derivative = true;
        r.key(pckey::Left); r.key(pckey::Right);
        CHECK(g->infoText().find("dY/dX=2") != std::string::npos);
        ws.graph.settings.derivative = false;
        r.key(pckey::Enter);                                       // dot with coordinates
        r.key(pckey::Esc);
        CHECK(g->mode() == GA::Mode::View);
        // Two graphs: up/down switch at the same x
        g = r.graphWith({"x^2-2", "2x"});
        r.draw();
        r.key(pckey::F1); r.enter("1.5");
        r.key(pckey::Down);
        CHECK(g->cursor(x, y) && x == 1.5 && y == 3);
        r.key(pckey::Esc);
    }
    // ================================================================ ZOOM
    {
        g = r.graphWith({"(x+5)(x+4)(x+3)"});
        ws.view = {-8, 8, 2, -4, 2, 1, 0, 2 * kPi, kPi / 60};      // manual 5-9
        r.draw();
        const ViewWindow start = ws.view;
        r.key(pckey::F2); r.type("1");                             // BOX
        CHECK(g->mode() == GA::Mode::Pointer);
        r.steps(pckey::Left, 40);
        r.key(pckey::Enter);
        r.steps(pckey::Left, 30);                                  // the second corner differs in x and y
        r.steps(pckey::Down, 30);
        r.key(pckey::Enter);
        CHECK(g->mode() == GA::Mode::View && ws.view.xmax - ws.view.xmin < 16 && !(ws.view == start));
        r.settle();
        r.key(pckey::F2); r.type("6");                             // ORIGINAL
        CHECK(ws.view == start);
        r.key(pckey::F2); r.type("2"); r.enter("4"); r.enter("2"); // FACTOR 4 x 2
        CHECK(ws.graph.settings.xFactor == 4 && ws.graph.settings.yFactor == 2);
        r.key(pckey::F2); r.type("3"); r.key(pckey::Enter);        // IN at the pointer
        CHECK_NEAR(ws.view.xmax - ws.view.xmin, 4, 1e-12);
        CHECK_NEAR(ws.view.ymax - ws.view.ymin, 3, 1e-12);
        r.key(pckey::F2); r.type("4"); r.key(pckey::Enter);        // OUT
        CHECK_NEAR(ws.view.xmax - ws.view.xmin, 16, 1e-12);
        r.type("+");                                               // +/- keys: factor zoom on the centre
        CHECK_NEAR(ws.view.xmax - ws.view.xmin, 4, 1e-12);
        r.type("-");
        r.key(pckey::F2); r.keys(pckey::Down, 9); r.key(pckey::Enter);   // PREVIOUS
        CHECK_NEAR(ws.view.xmax - ws.view.xmin, 4, 1e-12);
        r.key(pckey::F2); r.type("5");                             // AUTO: y range of the curve
        CHECK(ws.view.ymin < 0 && ws.view.ymax > 0);
        r.key(pckey::F2); r.type("7");                             // SQUARE
        CHECK_NEAR(viewXdot(ws.view), viewYdot(ws.view), 1e-12);
        r.key(pckey::F2); r.type("8");                             // ROUND
        const double xr = viewXdot(ws.view) / std::pow(10.0, std::floor(std::log10(viewXdot(ws.view))));
        CHECK(near(xr, std::round(xr), 1e-9));
        r.key(pckey::F2); r.type("9");                             // INTEGER
        CHECK(viewXdot(ws.view) == 1 && viewYdot(ws.view) == 1);
        r.key(pckey::F2); r.keys(pckey::Down, 10); r.key(pckey::Enter);  // TRIG
        CHECK_NEAR(ws.view.xmax, 3 * kPi, 1e-12);
        r.key(pckey::F2); r.keys(pckey::Down, 11); r.key(pckey::Enter);  // STANDRD
        CHECK(ws.view.xmin == -10 && ws.view.ymax == 10);
        r.key(pckey::F2); r.keys(pckey::Down, 12); r.key(pckey::Enter);  // INITIAL
        CHECK(ws.view.xmin == -6.3 && ws.view.ymax == 3.1);
        // Box with two points on the same row is refused (manual 5-9)
        r.key(pckey::F2); r.type("1"); r.key(pckey::Enter); r.steps(pckey::Right, 10); r.key(pckey::Enter);
        CHECK(r.error()); r.closeError(); r.key(pckey::Esc);
        // Arrows scroll the graph
        const double x0 = ws.view.xmin;
        r.key(pckey::Right);
        CHECK_NEAR(ws.view.xmin - x0, 12.6 / 8, 1e-12);
        // PAN: grab and move
        r.key(pckey::F8); r.type("2");
        CHECK(g->mode() == GA::Mode::Pointer);
        r.key(pckey::Enter); r.steps(pckey::Left, 3); r.key(pckey::Esc);
        CHECK(ws.view.xmin > x0 + 12.6 / 8);
    }
    // ================================================================ V-Window, V-MEM, Setup
    {
        g = r.graphWith({"x"});
        r.shift(); r.key(pckey::F3);                               // SHIFT+F3 = V-WIN from the list
        CHECK(g->screen() == GA::Screen::Window);
        r.enter("-2");                                             // Xmin; Xdot follows
        r.keys(pckey::Down, 2);                                    // Xdot
        r.enter("0.1");                                            // Xmax follows Xdot
        r.key(pckey::F4); r.type("1"); r.enter("2");               // V-MEM STORE 2
        r.key(pckey::Esc);
        CHECK(ws.view.xmin == -2);
        CHECK_NEAR(ws.view.xmax, -2 + 31.9, 1e-9);
        CHECK(ws.graph.vmemUsed == 2 && ws.graph.vmem[1].xmin == -2);
        r.shift(); r.key(pckey::F3);
        r.key(pckey::F1);                                          // INITIAL
        r.key(pckey::F4); r.type("2"); r.enter("2");               // V-MEM RECALL 2
        r.key(pckey::Esc);
        CHECK(ws.view.xmin == -2);
        r.shift(); r.key(pckey::F3);
        r.key(pckey::F4); r.type("2"); r.enter("5");               // empty memory
        CHECK(r.error()); r.closeError();
        r.key(pckey::F5); r.type("1");                             // SQUARE (Y-BASE)
        r.key(pckey::Esc);
        CHECK_NEAR(viewXdot(ws.view), viewYdot(ws.view), 1e-12);
        // T range with Tmax < Tmin: the pitch becomes negative
        r.shift(); r.key(pckey::F3);
        r.keys(pckey::Down, 7); r.enter("6"); r.enter("0"); r.enter("0.5");
        r.key(pckey::Esc);
        CHECK(ws.view.tmin == 6 && ws.view.tmax == 0 && ws.view.tstep == -0.5);
        // Invalid window: error, nothing applied
        const ViewWindow before = ws.view;
        r.shift(); r.key(pckey::F3);
        r.key(pckey::Down); r.enter("-50");                        // Xmax < Xmin
        r.key(pckey::Esc);
        CHECK(r.error()); r.closeError();
        CHECK(ws.view == before);
        r.key(pckey::Up); r.enter("Xmin+20"); r.key(pckey::Esc);  // valid again (expressions are accepted)
        // Setup inside GRAPH (not the SYSTEM mode)
        r.key(pckey::F10);
        CHECK(r.fw.apps().activeId() == AppId::Graph && g->screen() == GA::Screen::Setup);
        r.keys(pckey::Down, 5); r.key(pckey::Right);                         // Grid: Off (default) -> Line
        r.key(pckey::Down); r.key(pckey::Right); r.key(pckey::Right);        // Axes: Scale
        r.key(pckey::Down); r.key(pckey::Left);                              // Label: On
        r.key(pckey::Esc);
        CHECK(ws.graph.settings.grid == GridMode::Line && ws.graph.settings.axes == AxesMode::Scale && ws.graph.settings.label);
        r.key(pckey::F10); r.keys(pckey::Down, 3); r.key(pckey::Left); r.key(pckey::Esc);   // Angle: Rad -> Deg
        CHECK(ws.settings.angle == AngleMode::Degrees);
        r.key(pckey::F10); r.keys(pckey::Down, 3); r.key(pckey::Right); r.key(pckey::Esc);
        CHECK(ws.settings.angle == AngleMode::Radians);
        ws.graph.settings = GraphSettings{};
    }
    // ================================================================ BUILT-IN, Draw Type, Simul Graph
    {
        g = r.graphWith({});
        r.key(pckey::Home);
        r.key(pckey::F4); r.type("3"); r.type("2");                // TOOL > BUILT-IN > Y=A(X-B)^2+C
        CHECK(ws.functions[0].type == GraphType::Y && ws.functions[0].expr == "A(x-B)^(2)+C" && ws.functions[0].selected);
        ws.functions[0].type = GraphType::Polar; ws.functions[0].expr = "2";
        r.key(pckey::F4); r.type("3"); r.type("1");                // a line of another type is not replaced
        CHECK(r.error()); r.closeError();
        CHECK(ws.functions[0].type == GraphType::Polar && ws.functions[0].expr == "2");
        // Draw Type Plot: isolated points, one per column (manual 1-35)
        g = r.graphWith({"x"});
        r.key(pckey::F10); r.key(pckey::Down); r.key(pckey::Right); r.key(pckey::Esc);
        CHECK(ws.graph.settings.plotDraw);
        r.draw();
        {
            const auto& xy = g->curves().at(0).xy;
            int points = 0, joined = 0;
            for (std::size_t i = 0; i + 1 < xy.size(); i += 2) {
                if (xy[i] == graph::PenUp) continue;
                ++points;
                if (i >= 2 && xy[i - 2] != graph::PenUp) ++joined;
            }
            CHECK(points > 100 && joined == 0);
        }
        r.key(pckey::Esc);
        r.key(pckey::F10); r.key(pckey::Down); r.key(pckey::Left); r.key(pckey::Esc);
        CHECK(!ws.graph.settings.plotDraw);
        // Simul Graph On: the graphs advance together; Off: one after the other
        auto twoCircles = [&](double tstep) {
            g = r.graphWith({});
            for (std::size_t i = 0; i < 2; ++i) {
                ws.functions[i].type = GraphType::Param;
                ws.functions[i].expr = i ? "2cos(t)" : "cos(t)";
                ws.functions[i].expr2 = i ? "2sin(t)" : "sin(t)";
            }
            ws.view.tstep = tstep;                                  // thousands of steps per graph
        };
        twoCircles(0.001);
        r.key(pckey::F10); r.keys(pckey::Down, 10); r.key(pckey::Left); r.key(pckey::Esc);
        CHECK(ws.graph.settings.simulGraph);
        r.key(pckey::F6);
        CHECK(g->rendering() && g->curves().size() == 2);
        if (g->curves().size() == 2) CHECK(g->curves()[0].next > 0 && g->curves()[1].next > 0 && !g->curves()[0].done);
        r.settle();
        CHECK(!g->rendering() && g->curves()[0].done && g->curves()[1].done);
        r.key(pckey::Esc);
        twoCircles(0.0011);
        r.key(pckey::F10); r.keys(pckey::Down, 10); r.key(pckey::Right); r.key(pckey::Esc);
        CHECK(!ws.graph.settings.simulGraph);
        r.key(pckey::F6);
        CHECK(g->rendering() && g->curves().size() == 2);
        if (g->curves().size() == 2) CHECK(g->curves()[0].next > 0 && g->curves()[1].next == 0);
        r.settle();
        r.key(pckey::Esc);
        ws.graph.settings = GraphSettings{};
        // RAM: 20 dense Lissajous curves share MaxTotalCurvePoints (heap of the RP2350)
        g = r.graphWith({});
        for (std::size_t i = 0; i < MaxGraphFunctions; ++i) {
            ws.functions[i].type = GraphType::Param;
            ws.functions[i].expr = "6sin(" + std::to_string(37 + i) + "t)";
            ws.functions[i].expr2 = "3cos(" + std::to_string(41 + i) + "t)";
        }
        ws.view.tstep = 0.0005;
        r.draw();
        {
            std::size_t points = 0, truncated = 0;
            for (const auto& c : g->curves()) { points += c.xy.size() / 2; truncated += c.truncated ? 1 : 0; }
            CHECK(!g->rendering() && !r.error() && g->curves().size() == MaxGraphFunctions);
            CHECK(points <= graph::MaxTotalCurvePoints && points > graph::MaxTotalCurvePoints / 2 && truncated > 0);
        }
        r.key(pckey::Esc);
        ws.view = ViewWindow{};
    }
    // ================================================================ G-SOLVE
    {
        g = r.graphWith({"x^3-4x"});                               // manual 5-57
        r.draw();
        r.key(pckey::F5); r.type("1");                             // ROOT
        CHECK(g->mode() == GA::Mode::Results && g->solutions().size() == 3);
        if (g->solutions().size() == 3) { CHECK_NEAR(g->solutions()[0].first, -2, 1e-12); CHECK_NEAR(g->solutions()[2].first, 2, 1e-12); }
        r.key(pckey::Right);
        CHECK(g->infoText().find("X=0") != std::string::npos);
        r.key(pckey::Esc);
        r.key(pckey::F5); r.type("2");                             // MAX
        CHECK(g->solutions().size() == 1 && near(g->solutions()[0].first, -2 / std::sqrt(3.0), 1e-9));
        r.key(pckey::Esc);
        r.key(pckey::F5); r.type("3");                             // MIN
        CHECK(g->solutions().size() == 1 && near(g->solutions()[0].second, -16 / (3 * std::sqrt(3.0)), 1e-12));
        r.key(pckey::Esc);
        r.key(pckey::F5); r.type("4");                             // Y-ICEPT
        CHECK(g->solutions().size() == 1 && g->solutions()[0].second == 0);
        r.key(pckey::Esc);
        r.key(pckey::F5); r.type("6"); r.enter("0.5");             // Y-CAL
        CHECK(g->solutions().size() == 1 && near(g->solutions()[0].second, 0.125 - 2, 1e-12));
        r.key(pckey::Esc);
        r.key(pckey::F5); r.type("7"); r.enter("2.2");             // X-CAL: three x (manual 5-58)
        CHECK(g->solutions().size() == 3);
        for (const auto& s : g->solutions()) CHECK_NEAR(s.first * s.first * s.first - 4 * s.first, 2.2, 1e-9);
        r.key(pckey::Esc);
        r.key(pckey::F5); r.type("8"); r.type("1"); r.enter("-2"); r.enter("0");   // ∫dx from -2 to 0 (manual 5-59)
        CHECK_NEAR(g->integralValue(), 4, 1e-10);
        CHECK_NEAR(ws.memory.answer(), 4, 1e-10);
        // The shaded region stays on the graph after EXIT; the next G-Solve
        // redraws the graph without it.
        auto shadePixel = [&](double x, double y) {
            const graph::Viewport vp(ws.view);
            const int sx = static_cast<int>(std::lround(vp.screenX(x))), sy = static_cast<int>(std::lround(vp.screenY(y)));
            return board::graphics().pixels()[sy * ui::ScreenW + sx];
        };
        r.key(pckey::Esc); r.settle();
        CHECK(g->mode() == GA::Mode::View && shadePixel(-1, 1) != 0xFFFF);      // under the curve, inside [-2, 0]
        CHECK(shadePixel(1, 1) == 0xFFFF);                                        // outside the interval
        r.key(pckey::F5); r.type("2"); r.settle();                               // MAX
        CHECK(g->mode() == GA::Mode::Results && shadePixel(-1, 1) == 0xFFFF);
        r.key(pckey::Esc);
        // ∫dx with the pointer: lower then upper, EXE
        r.key(pckey::F5); r.type("8"); r.type("1");
        r.key(pckey::Enter); r.keys(pckey::Right, 10); r.key(pckey::Enter);
        CHECK(g->mode() == GA::Mode::Integral && g->integralValue() != 0);
        r.key(pckey::Esc);
        // Two graphs: INTSECT asks which ones when there are three (manual 5-57)
        g = r.graphWith({"x+1", "x^2", "-x"});
        r.draw();
        r.key(pckey::F5); r.type("5");
        CHECK(g->mode() == GA::Mode::Select);
        r.key(pckey::Enter); r.key(pckey::Enter);                  // Y1, then Y2
        CHECK(g->solutions().size() == 2 && near(g->solutions()[1].first, (1 + std::sqrt(5.0)) / 2, 1e-12));
        r.key(pckey::Esc);
        r.key(pckey::F5); r.type("1");
        CHECK(g->mode() == GA::Mode::Select);                      // three graphs: choose one
        r.key(pckey::Down); r.key(pckey::Down); r.key(pckey::Enter);   // -x
        CHECK(g->solutions().size() == 1 && g->solutions()[0].first == 0);
        r.key(pckey::Esc);
        // ∫dx ROOT on sin x between -pi and pi: integral 0, area 4 (manual 5-60)
        g = r.graphWith({"sin(x)"});
        r.draw();
        r.key(pckey::F5); r.type("8"); r.type("2");
        CHECK(g->mode() == GA::Mode::Integral);
        r.key(pckey::Right); r.key(pckey::Enter);                  // lower: -pi
        r.key(pckey::Right); r.key(pckey::Enter);                  // upper: next root after 0 = pi
        CHECK_NEAR(g->integralValue(), 0, 1e-10);
        CHECK_NEAR(g->areaValue(), 4, 1e-9);
        r.key(pckey::Esc);
        // ∫dx INTSECT: sin x and sin(x-pi/2): integral and area 2*sqrt(2)
        g = r.graphWith({"sin(x)", "sin(x-pi/2)"});
        r.draw();
        r.key(pckey::F5); r.type("8"); r.type("3");
        r.key(pckey::Right); r.key(pckey::Enter); r.key(pckey::Enter);
        CHECK_NEAR(std::abs(g->integralValue()), 2 * std::sqrt(2.0), 1e-9);
        CHECK_NEAR(g->areaValue(), 2 * std::sqrt(2.0), 1e-9);
        r.key(pckey::Esc);
        // ∫dx MIXED (manual 5-61): stops are roots and intersections; a typed x is allowed
        g = r.graphWith({"x^3+7x^2+2x-15", "5x+20"});
        ws.view = {-8, 4, 1, -20, 40, 5, 0, 2 * kPi, kPi / 60};
        r.draw();
        r.key(pckey::F5); r.type("8"); r.type("4");
        CHECK(g->mode() == GA::Mode::Integral);
        r.enter("-5"); r.enter("1");
        // ∫(Y1-Y2) from -5 to 1
        auto F = [](double x) { return x * x * x * x / 4 + 7 * x * x * x / 3 + x * x - 15 * x - (2.5 * x * x + 20 * x); };
        CHECK_NEAR(g->integralValue(), F(1) - F(-5), 1e-9);
        CHECK(g->areaValue() >= std::abs(g->integralValue()) - 1e-9);
        r.key(pckey::Esc);
        // No solution: Not Found
        g = r.graphWith({"x^2+1"});
        r.draw();
        r.key(pckey::F5); r.type("1");
        CHECK(r.error()); r.closeError();
    }
    // ================================================================ SKETCH
    {
        g = r.graphWith({"x(x+2)(x-2)"});                          // manual 5-53
        r.draw();
        r.key(pckey::F4); r.type("2");                             // Tangent
        CHECK(g->mode() == GA::Mode::Trace);
        r.enter("2"); r.key(pckey::Enter);
        CHECK(ws.graph.sketch.size() == 1 && ws.graph.sketch[0].kind == SketchItem::Kind::Tangent);
        if (!ws.graph.sketch.empty()) { CHECK_NEAR(ws.graph.sketch[0].x0, 2, 1e-12); CHECK_NEAR(ws.graph.sketch[0].y1 / ws.graph.sketch[0].x1, 8, 1e-8); }
        r.key(pckey::Esc);
        r.key(pckey::F4); r.type("3"); r.enter("1"); r.key(pckey::Enter); r.key(pckey::Esc);   // Normal at x=1: slope 1
        CHECK(ws.graph.sketch.size() == 2 && ws.graph.sketch[1].kind == SketchItem::Kind::Normal);
        if (ws.graph.sketch.size() == 2) CHECK_NEAR(ws.graph.sketch[1].x1 / ws.graph.sketch[1].y1, -(3 - 4), 1e-8);
        r.key(pckey::F4); r.type("4");                             // Inverse (one graph: no choice)
        CHECK(ws.graph.sketch.size() == 3 && ws.graph.sketch[2].kind == SketchItem::Kind::Inverse);
        r.settle();
        r.key(pckey::F4); r.type("5"); r.type("1"); r.key(pckey::Enter); r.key(pckey::Esc);     // Plot
        r.key(pckey::F4); r.type("5"); r.type("1"); r.steps(pckey::Right, 20); r.key(pckey::Enter); r.key(pckey::Esc);
        r.key(pckey::F4); r.type("6"); r.type("1");                // LINE > Line: joins the last two points
        CHECK(ws.graph.sketch.size() == 6 && ws.graph.sketch[5].kind == SketchItem::Kind::Segment);
        r.key(pckey::F4); r.type("5"); r.type("4"); r.steps(pckey::Right, 20); r.key(pckey::Enter); r.key(pckey::Esc);   // PlotChg removes it
        CHECK(ws.graph.sketch.size() == 5);
        r.key(pckey::F4); r.type("6"); r.type("2"); r.key(pckey::Enter); r.steps(pckey::Up, 10); r.key(pckey::Enter); r.key(pckey::Esc);   // F-Line
        r.key(pckey::F4); r.type("7"); r.key(pckey::Enter); r.steps(pckey::Right, 15); r.key(pckey::Enter); r.key(pckey::Esc);           // Circle
        ws.graph.settings.plotColor = 2; ws.graph.settings.sketchLine = LineStyle::Thick;      // Setup: Plot/LineCol, Sketch Line
        r.key(pckey::F4); r.type("8"); r.key(pckey::Enter); r.key(pckey::Esc);                                                          // Vertical
        ws.graph.settings.plotColor = 0; ws.graph.settings.sketchLine = LineStyle::Normal;
        r.key(pckey::F4); r.type("9"); r.key(pckey::Enter); r.key(pckey::Esc);                                                          // Horz
        r.key(pckey::F4); r.keys(pckey::Down, 10); r.key(pckey::Enter); r.key(pckey::Enter); r.enter("Max"); r.key(pckey::Esc);        // Text
        r.key(pckey::F4); r.keys(pckey::Down, 9); r.key(pckey::Enter); r.key(pckey::Enter); r.steps(pckey::Right, 5); r.steps(pckey::Up, 5); r.key(pckey::Enter); r.key(pckey::Esc);   // PEN
        CHECK(ws.graph.sketch.size() == 11);
        const SketchItem::Kind kinds[] = {SketchItem::Kind::Segment, SketchItem::Kind::Circle, SketchItem::Kind::Vertical, SketchItem::Kind::Horizontal, SketchItem::Kind::Text, SketchItem::Kind::Pen};
        for (std::size_t i = 0; i < 6 && 5 + i < ws.graph.sketch.size(); ++i) CHECK(ws.graph.sketch[5 + i].kind == kinds[i]);
        CHECK(ws.graph.sketch.size() > 9 && ws.graph.sketch[9].text == "Max");
        CHECK(ws.graph.sketch.size() > 8 && ws.graph.sketch[7].color == 2 && ws.graph.sketch[7].style == LineStyle::Thick && ws.graph.sketch[8].color == 0);
        // Elements: delete one, change the format of another
        r.key(pckey::F4); r.keys(pckey::Down, 11); r.key(pckey::Enter); r.type("1"); r.type("1");   // delete item 1 (tangent)
        CHECK(ws.graph.sketch.size() == 10 && ws.graph.sketch[0].kind == SketchItem::Kind::Normal);
        r.key(pckey::F4); r.keys(pckey::Down, 11); r.key(pckey::Enter); r.type("1"); r.type("2");   // FORMAT of item 1
        CHECK(g->screen() == GA::Screen::Format);
        r.key(pckey::Down); r.key(pckey::Right); r.key(pckey::Esc);
        CHECK(ws.graph.sketch[0].color == 1);
        r.key(pckey::F4); r.type("5"); r.type("3"); r.key(pckey::Enter); r.key(pckey::Esc);     // PlotOff
        CHECK(ws.graph.sketch.size() == 11 && ws.graph.sketch[10].kind == SketchItem::Kind::PointOff);
        r.key(pckey::F4); r.type("1"); r.key(pckey::F1);          // Cls
        CHECK(ws.graph.sketch.empty());
    }
    // ================================================================ MODIFY (manual 5-39)
    {
        g = r.graphWith({"x^2-Ax"});
        ws.memory.store('A', 0);
        r.key(pckey::F5);
        CHECK(g->mode() == GA::Mode::Modify && g->screen() == GA::Screen::Plot);
        r.key(pckey::Down); r.enter("0.5");                        // Step
        r.key(pckey::Up);
        r.keys(pckey::Right, 4);
        CHECK_NEAR(ws.memory.recall('A'), 2, 1e-12);
        r.enter("-2");
        CHECK(ws.memory.recall('A') == -2);
        r.settle();
        r.key(pckey::F1);                                          // COPY to the relation list
        CHECK(g->screen() == GA::Screen::List);
        r.key(pckey::Enter);                                       // into the first empty line: Y2
        CHECK(!ws.functions[1].empty() && !ws.functions[1].selected);
        ws.memory.store('A', 7);                                   // the copy keeps the value -2
        double v = 0;
        {
            Workspace probe = ws;
            graph::GraphEngine e;
            probe.functions[1].selected = true;
            e.build({&probe.functions, &probe.memory, &probe, &probe.lists});
            for (const auto& p : e.plots()) if (p.function == 1) CHECK(e.eval(p, 1, v) && v == 3);
        }
        CHECK(g->mode() == GA::Mode::Modify);
        r.key(pckey::Esc);
        // Error: two functions with variables
        g = r.graphWith({"Ax", "Bx"});
        r.key(pckey::F5);
        CHECK(r.error()); r.closeError();
        g = r.graphWith({"x"});
        r.key(pckey::F5);
        CHECK(r.error()); r.closeError();
        // Copying onto a drawn function: Expression in use
        g = r.graphWith({"Ax", "x"});
        r.key(pckey::F5); r.key(pckey::F1); r.key(pckey::Up); r.key(pckey::Enter);   // Y3 -> Y2 (drawn)
        CHECK(r.error()); r.closeError();
        r.key(pckey::Esc); r.key(pckey::Esc);
    }
    // ================================================================ graph memory
    {
        g = r.graphWith({"x^2", "3"});
        ws.functions[1].color = 3; ws.functions[1].style = LineStyle::Dot;
        r.key(pckey::F4); r.type("2"); r.type("1"); r.enter("3");  // GPH-MEM STORE 3
        CHECK(!ws.graph.gmem[2].empty());
        r.clearFunctions(); ws.view.xmin = -1;
        r.key(pckey::F4); r.type("2"); r.type("2"); r.enter("3");  // RECALL 3
        CHECK(ws.functions[0].expr == "x^2" && ws.functions[1].expr == "3" && ws.functions[1].style == LineStyle::Dot && ws.functions[1].color == 3);
        CHECK(ws.view.xmin == -6.3);
        r.key(pckey::F4); r.type("2"); r.type("2"); r.enter("9");  // empty
        CHECK(r.error()); r.closeError();
        r.key(pckey::F4); r.type("2"); r.type("1"); r.enter("21"); // out of range
        CHECK(r.error()); r.closeError();
    }
    // ================================================================ PICT
    {
        g = r.graphWith({"sin(x)"});
        r.draw();
        r.key(pckey::F8); r.type("1"); r.type("1"); r.enter("1");  // PICTURE > STORE > 1
        r.settle();
        CHECK(!r.error());
        std::vector<std::uint8_t> data;
        CHECK(files::readAll(card, "PICOCALC/PICT/PICT01.pcpict", data, 1 << 20) == files::Status::Ok);
        pict::Image img;
        CHECK_EQ_STR(pict::parse(data.data(), data.size(), img), "");
        CHECK(img.width == graphgeo::Columns && img.height == graphgeo::Rows && img.view == ws.view);
        r.key(pckey::F8); r.type("1"); r.type("2"); r.enter("WAVE");   // SAVE•AS
        r.settle();
        CHECK(card.exists("PICOCALC/PICT/WAVE.pcpict"));
        r.key(pckey::F8); r.type("1"); r.type("5"); r.enter("WAVE");   // BMP export
        r.settle();
        const auto* bmp = card.data("PICOCALC/PICT/WAVE.bmp");
        CHECK(bmp && bmp->size() == 54u + 960u * static_cast<unsigned>(graphgeo::Rows));
        r.key(pckey::F8); r.type("1"); r.type("3"); r.enter("1");  // RECALL 1: behind the graphs
        CHECK_EQ_STR(ws.graph.overlay, "PICOCALC/PICT/PICT01.pcpict");
        r.key(pckey::F8); r.type("1"); r.type("3"); r.enter("7");  // missing picture
        CHECK(r.error()); r.closeError();
        r.key(pckey::F8); r.type("1"); r.type("4");                // OPEN: file list
        CHECK(g->screen() == GA::Screen::Files);
        r.key(pckey::Enter);
        CHECK(g->screen() == GA::Screen::Plot);
        // Background (Setup) and its V-Window
        ws.view.xmin = -1;
        r.key(pckey::F10); r.keys(pckey::Down, 11); r.key(pckey::Right); r.key(pckey::Esc);   // Background: PICT 1
        CHECK(ws.graph.settings.background == 1);
        r.key(pckey::F1);                                          // load its V-Window: Yes
        CHECK(ws.view.xmin == -6.3);
        r.key(pckey::F8); r.type("3"); r.enter("40");              // Fade 40 %
        CHECK(ws.graph.settings.fade == 40);
        // BGV-WIN: the background file takes the current V-Window (manual 5-10)
        ws.view.xmin = -3; ws.view.xmax = 3;
        r.key(pckey::F8); r.type("4"); r.type("1"); r.key(pckey::F1);   // BGV-WIN > SAVE > Yes
        CHECK(!r.error());
        {
            std::vector<std::uint8_t> bytes;
            pict::Image back;
            CHECK(files::readAll(card, "PICOCALC/PICT/PICT01.pcpict", bytes, 1 << 20) == files::Status::Ok);
            CHECK(pict::parse(bytes.data(), bytes.size(), back).empty() && back.view.xmin == -3 && back.view.xmax == 3 && back.fade == 40);
        }
        r.key(pckey::F8); r.type("4"); r.type("2"); r.enter("BACK");    // BGV-WIN > SAVE•AS
        CHECK(card.exists("PICOCALC/PICT/BACK.pcpict") && ws.graph.settings.background == BackgroundFile);
        CHECK_EQ_STR(ws.graph.settings.backgroundFile, "PICOCALC/PICT/BACK.pcpict");
        r.key(pckey::F10); r.keys(pckey::Down, 11); r.keys(pckey::Left, 21); r.key(pckey::Esc);    // Background: None
        CHECK(ws.graph.settings.background == 0);
        // Cls removes the recalled picture
        r.key(pckey::F4); r.type("1"); r.key(pckey::F1);
        CHECK(ws.graph.overlay.empty());
        // No card
        card.setPresent(false);
        r.key(pckey::F8); r.type("1"); r.type("1"); r.enter("2");
        CHECK(r.error() && r.errorTitle() == "Storage ERROR"); r.closeError();
        card.setPresent(true);
    }
    // ================================================================ TABLE integration
    {
        g = r.graphWith({"x^2", "2x"});
        ws.table = TableRange{};
        r.key(pckey::F8); r.type("4");                             // OPTN > TABLE
        auto* t = r.table();
        CHECK(t && t->columns().size() == 2 && t->xs().size() == 7);
        double v = 0;
        CHECK(t && t->cell(0, 0, v) && v == 9 && t->cell(6, 1, v) && v == 6);
        r.home(); r.type("5");
        ws.graph.settings.derivative = true;
        r.home(); r.type("7");
        t = r.table();
        CHECK(t && t->columns().size() == 4 && t->cell(1, 1, v) && near(v, -4, 1e-9));   // d(x^2)/dx at -2
        ws.graph.settings.derivative = false;
        r.key(pckey::F5);                                          // GPH-CON
        g = r.graph();
        CHECK(g && g->screen() == GA::Screen::Plot && ws.table.draw == TableRange::Draw::Connect);
        r.settle();
        CHECK(g && !g->curves().empty() && !g->curves()[0].xy.empty());
        r.key(pckey::Esc);                                         // back to the table
        CHECK(r.fw.apps().activeId() == AppId::Table && ws.table.draw == TableRange::Draw::None);
        r.key(pckey::F6);                                          // GPH-PLT
        CHECK(ws.table.draw == TableRange::Draw::Plot);
        r.key(pckey::F6);                                          // G<->T back
        CHECK(r.fw.apps().activeId() == AppId::Table);
    }
    // ================================================================ paste a function on the graph screen (manual 5-30)
    {
        r.home(); r.type("1");                                     // Run-Matrix: Y=X copied to the clipboard
        r.type("Y=X"); r.ctrl('c');
        r.key(pckey::Break);
        g = r.graphWith({"x^2"});
        r.draw();
        r.ctrl('v');
        r.settle();
        CHECK(g->engine().plots().size() == 2 && g->engine().plots()[1].pasted);
        double v = 0;
        if (g->engine().plots().size() == 2) CHECK(g->engine().eval(g->engine().plots()[1], 2, v) && v == 2);
    }
    // ================================================================ .pcgraph project: full cycle (12.4)
    {
        g = r.graphWith({});
        r.key(pckey::F4); r.type("5"); r.type("1");               // FILE > NEW
        if (ws.graph.projectModified) r.key(pckey::F1);           // unsaved changes: confirm
        CHECK(ws.graph.project.empty() && !ws.graph.projectModified);
        // 2-3. functions of several types, colours and styles
        r.enter("x^2-2");
        r.key(pckey::F3); r.type("3"); r.enter("3cos(t)"); r.enter("3sin(t)");
        r.key(pckey::F3); r.type("2"); r.enter("2sin(2theta)");
        r.key(pckey::F3); r.type("7"); r.enter("x+1");            // Y<
        r.key(pckey::Home); r.key(pckey::F4); r.type("1"); r.type("3");   // Y1 Broken
        ws.functions[0].color = static_cast<std::uint8_t>(GraphColor::Red);
        // 4. V-Window
        ws.view = {-5, 5, 1, -4, 4, 2, 0, 2 * kPi, kPi / 30};
        ws.graph.settings.grid = GridMode::On; ws.graph.settings.axes = AxesMode::Scale;
        ws.settings.angle = AngleMode::Radians;
        // 5. sketches
        r.draw();
        r.key(pckey::F4); r.type("8"); r.key(pckey::Enter); r.key(pckey::Esc);
        r.key(pckey::F4); r.keys(pckey::Down, 10); r.key(pckey::Enter); r.key(pckey::Enter); r.enter("P1"); r.key(pckey::Esc);
        CHECK(ws.graph.sketch.size() == 2 && ws.graph.projectModified);
        // 6. save
        r.key(pckey::F8); r.keys(pckey::Down, 6); r.key(pckey::Enter); r.type("4"); r.enter("DEMO");   // OPTN > FILE > SAVE AS
        CHECK(!r.error() && ws.graph.project == "DEMO" && !ws.graph.projectModified);
        CHECK(card.exists("PICOCALC/GRAPH/DEMO.pcgraph"));
        const Workspace saved = ws;
        // 7. close: a new empty project
        r.key(pckey::F6);
        r.key(pckey::F4); r.type("5"); r.type("1");
        CHECK(ws.functions[0].empty() && ws.graph.sketch.empty());
        // 8. reopen
        r.key(pckey::F4); r.type("5"); r.type("2");
        CHECK(g->screen() == GA::Screen::Files);
        r.key(pckey::F1);
        CHECK(!r.error() && g->screen() == GA::Screen::List);
        // 9. every item is back
        for (std::size_t i = 0; i < MaxGraphFunctions; ++i) {
            const auto &a = saved.functions[i], &b = ws.functions[i];
            CHECK(a.expr == b.expr && a.expr2 == b.expr2 && a.type == b.type && a.selected == b.selected && a.color == b.color && a.style == b.style);
        }
        CHECK(ws.view == saved.view && ws.graph.settings.grid == GridMode::On && ws.graph.settings.axes == AxesMode::Scale);
        CHECK(ws.graph.sketch.size() == 2 && ws.graph.sketch[1].text == "P1");
        CHECK(ws.graph.project == "DEMO" && !ws.graph.projectModified);
        // 10. modify (still editable) and 11. save again
        r.keys(pckey::Down, 6);
        r.enter("x/2");
        CHECK(ws.graph.projectModified);
        r.key(pckey::F4); r.type("5"); r.type("3");               // FILE > SAVE (same name, no question)
        CHECK(!ws.graph.projectModified);
        std::vector<std::uint8_t> data;
        CHECK(files::readAll(card, "PICOCALC/GRAPH/DEMO.pcgraph", data, 1 << 20) == files::Status::Ok);
        graphfile::Project p;
        CHECK_EQ_STR(graphfile::decode(std::string(data.begin(), data.end()), p), "");
        bool found = false;
        for (const auto& f : p.functions) if (f.expr == "x/2" || f.expr == "(x)/(2)") found = true;
        CHECK(found);
        // Save As over an existing name asks first
        r.key(pckey::F4); r.type("5"); r.type("4"); r.enter("DEMO2");
        r.key(pckey::F4); r.type("5"); r.type("4");
        r.key(pckey::Break); r.enter("DEMO");
        CHECK(!r.error());                                         // confirmation shown
        r.key(pckey::F6);                                          // No
        CHECK(ws.graph.project == "DEMO2");
        // Unsaved changes are protected
        r.enter("x+9");
        r.key(pckey::F4); r.type("5"); r.type("1");               // NEW > confirmation
        r.key(pckey::F6);
        CHECK(!ws.functions[0].empty() || ws.graph.projectModified);
        // Rename and delete in the file list
        r.key(pckey::F4); r.type("5"); r.type("5");               // FILE > RENAME: the file list
        CHECK(g->screen() == GA::Screen::Files);
        r.key(pckey::F3); r.key(pckey::Break); r.enter("OLD");     // DEMO -> OLD
        CHECK(card.exists("PICOCALC/GRAPH/OLD.pcgraph") && !card.exists("PICOCALC/GRAPH/DEMO.pcgraph"));
        r.key(pckey::F2); r.key(pckey::F1);                        // delete DEMO2 (first in the sorted list)
        CHECK(!r.error() && !card.exists("PICOCALC/GRAPH/DEMO2.pcgraph") && card.exists("PICOCALC/GRAPH/OLD.pcgraph"));
        CHECK(ws.graph.project.empty());                           // it was the open project
        r.key(pckey::Esc);
        // Damaged file: reported, the open project is not touched
        auto* raw = card.data("PICOCALC/GRAPH/OLD.pcgraph");
        if (raw && raw->size() > 40) (*raw)[30] ^= 0x01;
        const std::string before = ws.functions[0].expr;
        r.key(pckey::F4); r.type("5"); r.type("2");
        if (ws.graph.projectModified) r.key(pckey::F1);
        r.key(pckey::F1);
        CHECK(r.error() && r.errorTitle() == "Storage ERROR");
        r.closeError();
        CHECK_EQ_STR(ws.functions[0].expr, before);
        r.key(pckey::Esc);
        // Full card: error, the previous file stays complete
        card.setCapacity(card.used() + 100);
        r.key(pckey::F4); r.type("5"); r.type("4"); r.key(pckey::Break); r.enter("BIG");
        CHECK(r.error()); r.closeError();
        CHECK(!card.exists("PICOCALC/GRAPH/BIG.pcgraph"));
        card.setCapacity(4u * 1024u * 1024u);
        // No card
        card.setPresent(false);
        r.key(pckey::F4); r.type("5"); r.type("4"); r.enter("NOCARD");
        CHECK(r.error() && r.errorTitle() == "Storage ERROR"); r.closeError();
        card.setPresent(true);
        CHECK(!card.exists("PICOCALC/GRAPH/NOCARD.pcgraph"));
    }
    // ================================================================ flash workspace keeps the GRAPH state
    {
        g = r.graphWith({"x^3"});
        ws.functions[1] = {"t", GraphType::Param, true, 2, LineStyle::Thin, "t^2"};
        ws.graph.settings.grid = GridMode::Line;
        SketchItem s; s.kind = SketchItem::Kind::Vertical; s.x0 = 1.5;
        ws.graph.sketch = {s};
        ws.graph.vmem[0] = ws.view; ws.graph.vmemUsed = 1;
        r.fw.saveNow();
        Rig again;
        const Workspace& w2 = again.fw.workspace();
        CHECK(w2.functions[0].expr == "x^3" && w2.functions[1].type == GraphType::Param && w2.functions[1].expr2 == "t^2" && w2.functions[1].style == LineStyle::Thin);
        CHECK(w2.graph.settings.grid == GridMode::Line && w2.graph.sketch.size() == 1 && w2.graph.vmemUsed == 1);
    }
    board::host::setFileSystem(&board::host::ramCard());
    return testutil::finish("graph_app_test");
}
