// v1.11.0 — Run-Matrix: modalita' Line (INS/sovrascrittura, ⌟, scorrimento
// dei risultati lunghi), SHIFT+INS in Math, schermate MatAns/ListAns, comandi
// grafici eseguiti in Run-Matrix e disegnati da GRAPH, nuove voci del SET UP.
#include "app_rig.hpp"
#include "graph_commands.hpp"
#include "graph_engine.hpp"
#include "run_matrix.hpp"
#include "test_util.hpp"
#include <string>

using namespace fx;

namespace {
const std::string kMark = "\xE2\x8C\x9F";
std::string last(Workspace& w) { return resultText(w.history.back(), w.settings); }
void type(NaturalEditor& e, const std::string& s) { for (char c : s) e.insert(c); }
std::size_t count(const Workspace& w, SketchItem::Kind k) {
    std::size_t n = 0;
    for (const auto& s : w.graph.sketch) n += s.kind == k;
    return n;
}
} // namespace

int main() {
    board::init();
    // ================================================================ INS: sovrascrittura in Line
    {
        NaturalEditor e;
        e.setLinear(true);
        type(e, "1234");
        e.moveHome();
        e.setOverwrite(true);
        CHECK(e.overwrite());
        type(e, "9");
        CHECK_EQ_STR(e.toExpression(), "9234");
        e.moveRight(); e.insertFraction();                       // anche ⌟ sovrascrive
        CHECK_EQ_STR(e.toExpression(), "92" + kMark + "4");
        e.moveEnd(); type(e, "5");                               // in fondo aggiunge
        CHECK_EQ_STR(e.toExpression(), "92" + kMark + "45");
        e.setOverwrite(false);
        e.moveHome(); type(e, "7");
        CHECK_EQ_STR(e.toExpression(), "792" + kMark + "45");
        // In Math non esiste la sovrascrittura
        NaturalEditor m;
        m.setOverwrite(true);
        CHECK(!m.overwrite());
        e.setOverwrite(true); e.setLinear(false);
        CHECK(!e.overwrite());
    }
    // ================================================================ SHIFT+INS: argomento di un modello (Math)
    {
        NaturalEditor e;
        type(e, "1+(2+3)+4");
        for (int i = 0; i < 7; ++i) e.moveLeft();                // prima di "("
        e.beginArgument();
        CHECK(e.argumentPending());
        e.insertRadical();
        CHECK(!e.argumentPending());
        CHECK_EQ_STR(e.toExpression(), "1+sqrt(2+3)+4");
        CHECK_NEAR(evaluate(e.toExpression()), 5 + std::sqrt(5.0), 1e-12);
        e.clear(); type(e, "25+1"); e.moveHome(); e.beginArgument(); e.insertRadical();
        CHECK_EQ_STR(e.toExpression(), "sqrt(25)+1");                // un numero
        e.clear(); type(e, "sin(30)"); e.moveHome(); e.beginArgument(); e.insertAbs();
        CHECK_EQ_STR(e.toExpression(), "abs(sin(30))");              // una funzione con la sua parentesi
        e.clear(); type(e, "12"); e.moveHome(); e.beginArgument(); e.insertFraction(); type(e, "4");
        CHECK_EQ_STR(e.toExpression(), "(12)/(4)");                  // numeratore, poi il denominatore
        e.clear(); type(e, "5"); e.moveHome(); e.beginArgument(); e.moveRight();
        CHECK(!e.argumentPending());                                 // una freccia annulla
        NaturalEditor l; l.setLinear(true); l.beginArgument();
        CHECK(!l.argumentPending());                                 // solo in Math
    }
    // ================================================================ tasti in Run-Matrix
    {
        testrig::Rig r;
        auto& ws = r.ws();
        ws.history.clear();
        ws.settings.inputMode = Settings::InputMode::Line;
        r.type("1");
        auto* rm = r.active<app::RunMatrixApp>();
        CHECK(rm && rm->editor().linear());
        r.type("1234"); r.key(pckey::Home);
        r.key(pckey::F7); r.key(pckey::Backspace);                     // SHIFT DEL = INS
        CHECK(rm && rm->editor().overwrite());
        r.type("5"); r.key(pckey::Enter);
        CHECK_EQ_STR(ws.history.back().expression, "5234");
        CHECK(rm && rm->editor().overwrite());                          // resta attiva
        r.key(pckey::F7); r.key(pckey::Backspace);
        CHECK(rm && !rm->editor().overwrite());
        // F1 in Line: ⌟; il risultato in Line con ⌟
        r.type("2"); r.key(pckey::F1); r.type("6"); r.key(pckey::Enter);
        CHECK_EQ_STR(ws.history.back().expression, "2" + kMark + "6");
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings, true), "1" + kMark + "3");
        // Risultato piu' lungo della riga: ←/→ lo scorrono quando e' selezionato
        r.enter("Seq(X^2,X,1,20,1)");
        r.key(pckey::Up);
        CHECK(rm && rm->browsing() == static_cast<int>(ws.history.size()) - 1 && rm->resultScroll() == 0);
        r.key(pckey::Right);
        CHECK(rm && rm->resultScroll() > 0 && rm->browsing() >= 0);    // scorre, resta selezionato
        r.key(pckey::Left);
        CHECK(rm && rm->resultScroll() == 0);
        r.key(pckey::Up);                                               // riga corta: → copia l'espressione
        r.key(pckey::Right);
        CHECK(rm && rm->browsing() < 0 && rm->editor().toExpression() == "2" + kMark + "6");
        r.key(pckey::Esc);
        // SHIFT+INS in Math dalla tastiera
        ws.settings.inputMode = Settings::InputMode::Math;
        r.type("16"); r.key(pckey::Home);
        r.key(pckey::F7); r.key(pckey::Backspace);
        CHECK(rm && rm->editor().argumentPending());
        r.key(pckey::F2);                                               // √
        CHECK(rm && rm->editor().toExpression() == "sqrt(16)");
        r.key(pckey::Enter);
        CHECK_EQ_STR(last(ws), "4");
        // ================================================================ MatAns / ListAns
        r.enter("Identity(7)");
        CHECK(rm && rm->answerView().isOpen() && rm->answerView().type() == ValueType::Matrix);
        CHECK(rm && rm->answerView().grid().rows == 7 && rm->answerView().grid().cols == 7);
        for (int i = 0; i < 6; ++i) r.key(pckey::Right);
        r.key(pckey::Down);
        CHECK(rm && rm->answerView().grid().col == 6 && rm->answerView().grid().row == 1 && rm->answerView().grid().firstColumn() > 0);
        r.type("5");                                                    // sola lettura: i tasti non cambiano nulla
        CHECK(ws.matrices.matAns.re[1 * 7 + 6] == 0);
        r.key(pckey::Esc);
        CHECK(rm && !rm->answerView().isOpen());
        r.key(pckey::Up); r.key(pckey::F4);                             // dalla cronologia: F4 MatAns
        CHECK(rm && rm->answerView().isOpen());
        r.key(pckey::Esc); r.key(pckey::Esc);
        r.enter("Seq(X,X,1,30,1)");
        CHECK(rm && rm->answerView().isOpen() && rm->answerView().type() == ValueType::List && rm->answerView().grid().rows == 30);
        for (int i = 0; i < 29; ++i) r.key(pckey::Down);
        CHECK(rm && rm->answerView().grid().row == 29);
        r.key(pckey::Esc);
        r.enter("Identity(3)");                                         // vista naturale: nessuna schermata
        CHECK(rm && !rm->answerView().isOpen());
        r.home();
    }
    // ================================================================ comandi grafici: dati di GRAPH
    {
        Workspace w;
        CHECK(graphcmd::isCommand("Plot 1,2") && graphcmd::isCommand("F-Line 1,2,3,4") && graphcmd::isCommand("Graph Y=x^2"));
        CHECK(graphcmd::isCommand("Graph(X,Y)=(cos(T),sin(T))") && graphcmd::isCommand("Cls") && graphcmd::isCommand("Plot1,2"));
        CHECK(!graphcmd::isCommand("Plotx") && !graphcmd::isCommand("1+2") && !graphcmd::isCommand("line"));
        runCalculation(w, "ViewWindow -5,5,1,-4,4,1");
        CHECK(w.view.xmin == -5 && w.view.xmax == 5 && w.view.ymin == -4 && w.view.ymax == 4);
        CHECK(w.history.back().command && last(w) == "Done" && w.graph.commandScreen);
        runCalculation(w, "1\xE2\x86\x92" "A");
        runCalculation(w, "Plot A,A*2");
        runCalculation(w, "Plot 3,4");
        CHECK(count(w, SketchItem::Kind::Point) == 2 && w.graph.sketch[0].x0 == 1 && w.graph.sketch[0].y0 == 2);
        runCalculation(w, "Line");                                      // unisce gli ultimi due punti
        const SketchItem& seg = w.graph.sketch.back();
        CHECK(seg.kind == SketchItem::Kind::Segment && seg.x0 == 1 && seg.y0 == 2 && seg.x1 == 3 && seg.y1 == 4);
        runCalculation(w, "F-Line 0,0,1,1");
        runCalculation(w, "Circle 0,0,2");
        runCalculation(w, "Vertical 1");
        runCalculation(w, "Horizontal -1");
        runCalculation(w, "Text 1,1,\"Ciao\"");
        CHECK(w.graph.sketch.back().kind == SketchItem::Kind::Text && w.graph.sketch.back().text == "Ciao");
        CHECK(w.graph.sketch.back().x0 == -5 && w.graph.sketch.back().y0 == 4);   // pixel (1,1): in alto a sinistra
        runCalculation(w, "PxlOn 282,320");
        CHECK_NEAR(w.graph.sketch.back().x0, 5, 1e-9);
        CHECK_NEAR(w.graph.sketch.back().y0, -4, 1e-9);
        runCalculation(w, "PlotOff 1,2");
        CHECK(w.graph.sketch.back().kind == SketchItem::Kind::PointOff);
        runCalculation(w, "PlotChg 5,5");
        CHECK(w.graph.sketch.back().kind == SketchItem::Kind::Point);
        runCalculation(w, "PlotChg 5,5");
        CHECK(w.graph.sketch.back().kind == SketchItem::Kind::PointOff);
        runCalculation(w, "Tangent x^2,1");
        CHECK(w.graph.sketch.back().kind == SketchItem::Kind::Tangent && w.graph.sketch.back().y0 == 1);
        CHECK_NEAR(w.graph.sketch.back().y1, 2, 1e-6);
        runCalculation(w, "Normal x^2,1");
        CHECK_NEAR(w.graph.sketch.back().x1, -2, 1e-6);
        runCalculation(w, "Inverse x^2");
        CHECK(w.graph.sketch.back().kind == SketchItem::Kind::Inverse && w.graph.sketch.back().text == "x^2");
        runCalculation(w, "Graph Y=x^2-1");
        runCalculation(w, "Graph r=2");
        runCalculation(w, "Graph X=y^2");
        runCalculation(w, "Graph(X,Y)=(cos(T),sin(T))");
        CHECK(w.graph.commandGraphs.size() == 4);
        CHECK(w.graph.commandGraphs[0].type == GraphType::Y && w.graph.commandGraphs[0].expr == "x^2-1");
        CHECK(w.graph.commandGraphs[3].type == GraphType::Param && w.graph.commandGraphs[3].expr == "cos(T)" && w.graph.commandGraphs[3].expr2 == "sin(T)");
        CHECK(w.graph.commandGraphs[1].color != w.graph.commandGraphs[0].color);
        // Il motore di GRAPH disegna i grafici dei comandi
        w.apply();
        graph::GraphEngine e;
        e.build({&w.functions, &w.memory, &w, &w.lists, &w.graph.commandGraphs});
        std::size_t fromCommands = 0;
        for (const auto& p : e.plots()) fromCommands += p.detail == "Run-Matrix";
        CHECK(fromCommands == 4);
        // Errori: tutto o niente
        const std::size_t items = w.graph.sketch.size();
        CHECK_THROWS_AS(runCalculation(w, "Plot 1"), SyntaxError);
        CHECK_THROWS_AS(runCalculation(w, "Circle 0,0,-1"), MathError);
        CHECK_THROWS_AS(runCalculation(w, "ViewWindow 5,1,1,-1,1,1"), MathError);
        CHECK(w.view.xmin == -5);
        CHECK_THROWS_AS(runCalculation(w, "Text 1,1,Ciao"), SyntaxError);
        CHECK_THROWS_AS(runCalculation(w, "Text 0,1,\"x\""), MathError);   // riga 0 fuori dallo schermo
        CHECK_THROWS_AS(runCalculation(w, "Graph Y="), SyntaxError);
        CHECK_THROWS_AS(runCalculation(w, "Graph Y=x+"), SyntaxError);
        CHECK_THROWS_AS(runCalculation(w, "Plot 1/0,1"), MathError);
        CHECK_THROWS_AS(runCalculation(w, "Cls 1"), SyntaxError);
        CHECK(w.graph.sketch.size() == items);
        // Posizione dell'errore nel testo del comando
        try { runCalculation(w, "Plot 1,1/0"); CHECK(false); }
        catch (const MathError& err) { CHECK(err.where.known() && err.where.position >= 7); }
        // Limite di 64 elementi SKETCH
        while (w.graph.sketch.size() < MaxSketchItems) runCalculation(w, "Plot 0,0");
        CHECK_THROWS_AS(runCalculation(w, "Plot 1,1"), MathError);
        CHECK(w.graph.sketch.size() == MaxSketchItems);
        // Cls cancella SKETCH e grafici dei comandi
        runCalculation(w, "Cls");
        CHECK(w.graph.sketch.empty() && w.graph.commandGraphs.empty());
        CHECK_THROWS_AS(runCalculation(w, "Line"), MathError);          // nessun punto da unire
        // Ricalcolo della cronologia: i comandi successivi non si rieseguono
        Workspace h;
        runCalculation(h, "2\xE2\x86\x92" "B");
        runCalculation(h, "Plot B,B");
        runCalculation(h, "B+1");
        CHECK(h.graph.sketch.size() == 1);
        CHECK(editHistoryEntry(h, 0, "3\xE2\x86\x92" "B") == 0);
        CHECK(h.graph.sketch.size() == 1 && resultText(h.history[1], h.settings) == "Done" && resultText(h.history[2], h.settings) == "4");
        CHECK(editHistoryEntry(h, 1, "Plot B,1") == 0);                  // la riga modificata si esegue
        CHECK(h.graph.sketch.size() == 2 && h.graph.sketch.back().x0 == 3);
        // Persistenza (record v7): grafici dei comandi e righe "Done"
        runCalculation(h, "Graph Y=2x");
        const auto blob = serializeWorkspace(h);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
        CHECK(back.graph.commandGraphs.size() == 1 && back.graph.commandGraphs[0].expr == "2x");
        CHECK(back.history[1].command && resultText(back.history[1], back.settings) == "Done");
        CHECK(!back.graph.commandScreen);                                // la richiesta non si salva
    }
    // ================================================================ comandi grafici: schermata di GRAPH ed EXIT
    {
        testrig::Rig r;
        auto& ws = r.ws();
        ws.graph.sketch.clear(); ws.graph.commandGraphs.clear();
        ws.settings.inputMode = Settings::InputMode::Line;
        r.type("1");
        r.enter("Graph Y=x^2");
        CHECK(!r.error());
        auto* g = r.active<app::GraphApp>();
        CHECK(g && g->screen() == app::GraphApp::Screen::Plot);       // il grafico, non la lista
        r.settle();
        bool drawn = false;
        if (g) for (const auto& p : g->engine().plots()) drawn = drawn || p.detail == "Run-Matrix";
        CHECK(drawn);
        r.key(pckey::Esc);                                              // EXIT: di nuovo i calcoli
        CHECK(r.active<app::RunMatrixApp>() != nullptr && !ws.graph.commandScreen);
        CHECK_EQ_STR(last(ws), "Done");
        // OPTN > SKETCH inserisce il nome del comando
        auto* rm = r.active<app::RunMatrixApp>();
        r.key(pckey::F8);
        for (int i = 0; i < 3; ++i) r.key(pckey::Up);                  // SKETCH: prima di RISULTATO e BASE-N
        r.key(pckey::Enter);
        for (int i = 0; i < 6; ++i) r.key(pckey::Down);                // "Plot "
        r.key(pckey::Enter);
        CHECK(rm && rm->editor().toExpression() == "Plot ");
        r.type("1,1"); r.key(pckey::Enter);
        CHECK(r.active<app::GraphApp>() != nullptr && ws.graph.sketch.size() == 1);
        r.key(pckey::Esc);
        // Errore: resta in Run-Matrix con il cursore sulla causa
        r.enter("Circle 0,0,0");
        CHECK(r.error() && r.active<app::RunMatrixApp>() != nullptr);
        r.closeError();
        // Dal menu principale GRAPH apre come sempre la lista delle relazioni
        r.home(); r.type("5");
        CHECK(r.active<app::GraphApp>() && r.active<app::GraphApp>()->screen() == app::GraphApp::Screen::List);
        r.home();
    }
    // ================================================================ SET UP: Mode, Simplify, CSV
    {
        testrig::Rig r;
        auto& s = r.ws().settings;
        s.baseMode = Settings::BaseMode::Comp; s.simplifyManual = false; s.csvSeparator = ','; s.csvDecimal = '.';
        r.home(); r.type("g");
        for (int i = 0; i < 18; ++i) r.key(pckey::Down);
        r.key(pckey::Right);                                            // Mode: Dec
        CHECK(s.baseMode == Settings::BaseMode::Dec);
        r.key(pckey::Right);
        CHECK(s.baseMode == Settings::BaseMode::Hex);
        r.key(pckey::Down); r.key(pckey::Right);                       // Simplify: Manual
        CHECK(s.simplifyManual);
        r.key(pckey::Down); r.key(pckey::Right);                       // CSV Separator ;
        CHECK(s.csvSeparator == ';');
        r.key(pckey::Down); r.key(pckey::Right);                       // CSV Decimal ,
        CHECK(s.csvDecimal == ',');
        r.key(pckey::Up); r.key(pckey::Right);                         // Separator , con Decimal , : rifiutato
        CHECK(r.error());
        r.closeError();
        CHECK(s.csvSeparator == ';');
        r.key(pckey::Esc);
        r.closeError();
        // La barra di stato di Run-Matrix indica la base
        r.home(); r.type("1");
        r.enter("FF");
        CHECK_EQ_STR(last(r.ws()), "FF");
        r.ws().settings.baseMode = Settings::BaseMode::Comp;
        r.ws().settings.simplifyManual = false;
        r.ws().settings.csvSeparator = ','; r.ws().settings.csvDecimal = '.';
        r.home();
    }
    return testutil::finish("run_matrix_v111_test");
}
