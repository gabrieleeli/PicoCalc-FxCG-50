// v1.12.0 — comandi grafici di Run-Matrix (manuale cap. 5 SKETCH e cap. 8):
// Graph Y>/Y</Y≥/Y≤ e X>/X</X≥/X≤, G∫dx, prefissi SketchNormal/Thick/Broken/
// Dot/Thin, StoPict/RclPict/BG-Pict/RclCapt, DrawGraph, DrawStat, DrawDyna,
// DrawR-Con/Plt, DrawWeb, DrawFTG-Con/Plt, G-SelOn/Off, PxlTest( (schermata di
// GRAPH come VRAM) e i grafici dei comandi nei progetti .pcgraph.
#include "app_rig.hpp"
#include "graph_commands.hpp"
#include "graph_engine.hpp"
#include "graph_file.hpp"
#include "run_matrix.hpp"
#include "test_util.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace fx;

namespace {
const std::string kArrow = "\xE2\x86\x92";
std::string last(Workspace& w) { return resultText(w.history.back(), w.settings); }
std::size_t count(const Workspace& w, SketchItem::Kind k) {
    std::size_t n = 0;
    for (const auto& s : w.graph.sketch) n += s.kind == k;
    return n;
}
template <typename E> bool fails(Workspace& w, const std::string& t) {
    try { runCalculation(w, t); } catch (const E&) { return true; } catch (...) { return false; }
    return false;
}
} // namespace

int main() {
    board::init();
    // ================================================================ disequazioni, G∫dx, stili
    {
        Workspace w;
        const struct { const char* text; GraphType type; } ineq[] = {
            {"Graph Y>x", GraphType::YGreater}, {"Graph Y<x", GraphType::YLess}, {"Graph Y\xE2\x89\xA5x", GraphType::YGreaterEq},
            {"Graph Y<=x", GraphType::YLessEq}, {"Graph X>y", GraphType::XGreater}, {"Graph X<y", GraphType::XLess},
            {"Graph X>=y", GraphType::XGreaterEq}, {"Graph X\xE2\x89\xA4y", GraphType::XLessEq}};
        for (const auto& e : ineq) {
            runCalculation(w, e.text);
            CHECK(last(w) == "Done" && w.graph.commandGraphs.back().type == e.type);
        }
        CHECK(w.graph.commandGraphs.size() == 8);
        runCalculation(w, "Cls");
        CHECK(w.graph.commandGraphs.empty() && w.graph.sketch.empty());
        runCalculation(w, "G\xE2\x88\xAB" "dx x^2,0,3");
        CHECK(count(w, SketchItem::Kind::Integral) == 1 && w.graph.commandGraphs.size() == 1);
        CHECK(w.graph.sketch[0].x0 == 0 && w.graph.sketch[0].x1 == 3 && w.graph.sketch[0].text == "x^2");
        CHECK(fails<MathError>(w, "G\xE2\x88\xAB" "dx x,3,0"));
        CHECK(fails<SyntaxError>(w, "G\xE2\x88\xAB" "dx x,1"));
        runCalculation(w, "SketchThick F-Line 0,0,1,1");
        CHECK(w.graph.sketch.back().style == LineStyle::Thick);
        runCalculation(w, "SketchDot Graph Y=x");
        CHECK(w.graph.commandGraphs.back().style == LineStyle::Dot);
        runCalculation(w, "SketchBroken");                                           // da solo: SET UP Sketch Line
        CHECK(w.graph.settings.sketchLine == LineStyle::Broken);
        runCalculation(w, "Vertical 1");
        CHECK(w.graph.sketch.back().style == LineStyle::Broken);
        runCalculation(w, "SketchThin Circle 0,0,1");
        CHECK(w.graph.sketch.back().style == LineStyle::Thin);
        runCalculation(w, "SketchNormal Plot 1,1");
        CHECK(w.graph.sketch.back().style == LineStyle::Normal);
    }
    // ================================================================ Pict, G-SelOn/Off, Draw...
    {
        Workspace w;
        runCalculation(w, "StoPict 3");
        CHECK(w.graph.storePict == 3);
        runCalculation(w, "RclPict 4");
        CHECK(w.graph.overlay.find("PICT04") != std::string::npos);
        runCalculation(w, "BG-Pict 2");
        CHECK(w.graph.settings.background == 2);
        runCalculation(w, "RclCapt 5");
        CHECK(w.graph.overlay.find("CAPT05") != std::string::npos);
        CHECK(fails<MathError>(w, "StoPict 21"));
        runCalculation(w, "G-SelOff 1");
        CHECK(!w.functions[0].selected);
        runCalculation(w, "G-SelOn 1");
        CHECK(w.functions[0].selected);
        CHECK(fails<MathError>(w, "G-SelOn 21"));
        runCalculation(w, "DrawGraph");
        CHECK(last(w) == "Done" && w.graph.commandScreen);
        // DrawStat: Scatter di XList/YList
        CHECK(fails<DimensionError>(w, "DrawStat"));
        w.setList(0, {1, 2, 3}); w.setList(1, {2, 4, 6});
        runCalculation(w, "DrawStat");
        CHECK(count(w, SketchItem::Kind::Data) == 1 && w.graph.sketch.back().path.size() == 6 && w.graph.sketch.back().y1 == 0);
        // DrawR / DrawWeb: an+1 = 2an+1 da a0 = 1
        CHECK(fails<MathError>(w, "DrawR-Con"));
        runCalculation(w, "\"2rec_an+1\"" + kArrow + "rec_an1");
        runCalculation(w, "1" + kArrow + "rec_a0");
        runCalculation(w, "4" + kArrow + "r_end");
        runCalculation(w, "DrawR-Con");
        const SketchItem& rs = w.graph.sketch.back();
        CHECK(rs.kind == SketchItem::Kind::Data && rs.y1 == 1 && rs.path.size() == 10 && rs.path[9] == 31);   // 1,3,7,15,31
        runCalculation(w, "DrawR-Plt");
        CHECK(w.graph.sketch.back().y1 == 0);
        runCalculation(w, "DrawWeb");
        CHECK(w.graph.sketch.back().kind == SketchItem::Kind::Data);
        // DrawFTG: la tabella di Y1 = x²
        w.functions[0].expr = "x^2"; w.functions[0].type = GraphType::Y;
        for (std::size_t i = 1; i < w.functions.size(); ++i) w.functions[i].selected = false;
        w.table.start = 0; w.table.end = 2; w.table.step = 1;
        runCalculation(w, "DrawFTG-Con");
        CHECK(w.graph.sketch.back().path == std::vector<float>({0, 0, 1, 1, 2, 4}));
        runCalculation(w, "DrawFTG-Plt");
        CHECK(w.graph.sketch.back().y1 == 0);
        // DrawDyna: Y1 = A x per A da D Start a D End
        runCalculation(w, "Cls");
        w.functions[0].expr = "Ax";
        runCalculation(w, "1" + kArrow + "d_start");
        runCalculation(w, "3" + kArrow + "d_end");
        runCalculation(w, "DrawDyna");
        CHECK(w.graph.commandGraphs.size() == 3);
        double y = 0;
        graph::GraphEngine engine;
        engine.build({&w.functions, &w.memory, &w, &w.lists, &w.graph.commandGraphs});
        int found = 0;
        for (const auto& p : engine.plots()) if (p.detail == "Run-Matrix" && engine.eval(p, 2, y) && (y == 2 || y == 4 || y == 6)) ++found;
        CHECK(found == 3);
        runCalculation(w, "DrawDyna");                                               // 6 grafici
        runCalculation(w, "DrawDyna");                                               // 9 grafici
        CHECK(fails<MathError>(w, "DrawDyna"));                                      // oltre 10 grafici dei comandi
        CHECK(w.graph.commandGraphs.size() == 9);                                     // tutto o niente
        // Limite dei punti di DrawStat/DrawR/DrawFTG della scheda
        runCalculation(w, "Cls");
        // v1.18.0: two lists that fit the store of the board (999 per list on both boards, 1 530 in all on the Pico 2 W)
        std::vector<double> big(std::min(limits::kListMaxLength, limits::kListStoreElements / 2), 1.0);
        w.setList(0, big); w.setList(1, big);
        bool limited = false;
        for (int k = 0; k < 30 && !limited; ++k) limited = fails<MathError>(w, "DrawStat");
        CHECK(limited);
    }
    // ================================================================ grafici dei comandi nei progetti .pcgraph
    {
        Workspace w;
        runCalculation(w, "Graph Y\xE2\x89\xA5x^2");
        runCalculation(w, "Graph(X,Y)=(cos(t),sin(t))");
        runCalculation(w, "G\xE2\x88\xAB" "dx x,0,1");
        graphfile::Project p;
        p.commands = w.graph.commandGraphs;
        p.sketch = w.graph.sketch;
        const std::string text = graphfile::encode(p);
        CHECK(text.find("\nc=Y>=|") != std::string::npos);
        graphfile::Project back;
        CHECK(graphfile::decode(text, back).empty());
        CHECK(back.commands.size() == 3 && back.commands[0].type == GraphType::YGreaterEq && back.commands[1].expr2 == "sin(t)");
        CHECK(back.sketch.size() == 1 && back.sketch[0].kind == SketchItem::Kind::Integral);
        CHECK(graphfile::encode(back) == text);
    }
    // ================================================================ PxlTest( e la schermata di GRAPH
    {
        testrig::Rig r;
        auto& ws = r.ws();
        r.key(pckey::Tab); r.type("1");
        CHECK(r.active<app::RunMatrixApp>() != nullptr);
        for (auto& f : ws.functions) f.selected = false;
        ws.graph.settings.axes = AxesMode::Off;
        r.enter("Cls");
        r.key(pckey::Esc);
        auto pxl = [&](const std::string& args) {
            // PxlTest( dal menu SKETCH: il token pxl_test, poi gli argomenti
            r.active<app::RunMatrixApp>()->editor().insertSymbol("pxl_test");
            r.enter("(" + args);
        };
        pxl("10,10");
        CHECK_EQ_STR(last(ws), "0");
        r.enter("PxlOn 10,10");
        r.key(pckey::Esc);
        pxl("10,10");
        CHECK_EQ_STR(last(ws), "1");                                                  // il punto acceso da PxlOn
        pxl("200,300");
        CHECK_EQ_STR(last(ws), "0");
        ws.graph.settings.axes = AxesMode::On;
        pxl("142,160");                                                               // gli assi sono pixel accesi
        CHECK_EQ_STR(last(ws), "1");
        pxl("0,1");
        CHECK(r.error());
        r.closeError();
        r.key(pckey::Esc);                                                            // EXIT cancella l'input
        // Run-Matrix resta la modalita' attiva e si ridisegna
        CHECK(r.active<app::RunMatrixApp>() != nullptr);
        // StoPict n da Run-Matrix: GRAPH salva la schermata disegnata
        r.active<app::RunMatrixApp>()->editor().insertSymbol("StoPict ");             // dal menu PICTURE
        r.enter("7");
        CHECK(r.active<app::GraphApp>() != nullptr);
        r.settle();
        CHECK(ws.graph.storePict == 0);
        r.key(pckey::Esc);
    }
    return testutil::finish("graph_cmd_v112_test");
}
