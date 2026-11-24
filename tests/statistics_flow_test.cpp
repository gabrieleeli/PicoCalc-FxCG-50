// v1.18.0 (solo Pimoroni Pico Plus 2 W): STATISTICS alla parita' del capitolo 6,
// un caso per voce della tabella di docs/reports/ANALISI_FONTI_v1.18.0.md (§3):
// tasti dal firmware simulato, schermate, liste, variabili di VARS › STAT,
// pixel dei grafici. I numeri del nucleo sono confrontati con scipy in
// statistics_test; qui si controlla che l'interfaccia li produca e li salvi.
#include "app_rig.hpp"
#include "dist_core.hpp"
#include "expression.hpp"
#include "stat_engine.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace fx;
using namespace fx::app;
namespace pk = fx::pckey;
using testrig::Rig;

namespace {
#define T(id, cond) ::testutil::report(static_cast<bool>(cond), id, __FILE__, __LINE__, #cond)
StatisticsApp* openStat(Rig& r) { r.home(); r.type("2"); return r.active<StatisticsApp>(); }
std::string label(Rig& r, StatisticsApp* st, int i) {
    const char* const* l = st->softKeyLabels(r.fw.apps().context());
    return l[i] ? l[i] : "";
}
int fieldIndex(StatisticsApp* st, const std::string& l) {
    const auto& f = st->form().fields;
    for (std::size_t i = 0; i < f.size(); ++i) if (f[i].label == l) return static_cast<int>(i);
    return -1;
}
void selectField(Rig& r, StatisticsApp* st, const std::string& l) {
    const int target = fieldIndex(st, l);
    if (target < 0) { ::testutil::report(false, "campo del modulo", __FILE__, __LINE__, l); return; }
    for (int k = 0; k < 40 && st->form().selected != target; ++k) r.key(pk::Down);
}
void setChoice(Rig& r, StatisticsApp* st, const std::string& l, int choice) {
    selectField(r, st, l);
    const int i = fieldIndex(st, l);
    if (i < 0) return;
    for (int k = 0; k < 60 && st->form().fields[static_cast<std::size_t>(i)].choice != choice; ++k) r.key(pk::Right);
}
void setNumber(Rig& r, StatisticsApp* st, const std::string& l, const std::string& text) {
    selectField(r, st, l);
    r.key(pk::Backspace);                                   // campo vuoto in modifica
    r.enter(text);
}
double rowValue(StatisticsApp* st, const std::string& l) {
    for (const auto& row : st->results().rows) if (row.first == l) return std::stod(row.second);
    ::testutil::report(false, "riga del risultato", __FILE__, __LINE__, l);
    return std::nan("");
}
bool near(double a, double b, double rel = 1e-9) { return std::abs(a - b) <= rel * std::max(1.0, std::abs(b)); }
std::size_t colorPixels(std::uint16_t color) {
    std::vector<std::uint16_t> px(static_cast<std::size_t>(ui::ScreenW) * ui::ScreenH);
    board::graphics().copyTo(gfx::Rect{0, 0, ui::ScreenW, ui::ScreenH}, px.data(), ui::ScreenW);
    std::size_t n = 0;
    for (std::size_t i = static_cast<std::size_t>(ui::ScreenW) * ui::ContentTop; i < static_cast<std::size_t>(ui::ScreenW) * ui::ContentBottom; ++i) n += px[i] == color;
    return n;
}
std::string errText(Rig& r) { return r.errorTitle() + ": " + r.errorDetail(); }
void redraw(Rig& r) { r.fw.draw(board::graphics()); board::present(); }
double resolve(Workspace& ws, const char* name) { double v = 0; return ws.resolve(name, v) ? v : std::nan(""); }
double eval(Workspace& ws, const std::string& text) { return evaluateReal(*parseExpression(text), ws.evalContext()); }
void clearLists(Workspace& ws) { for (auto& l : ws.lists) l.clear(); for (auto& f : ws.statistics.listFormula) f.clear(); }
// Tasto F della pagina p (0-2) della barra dell'editor delle liste.
void pageKey(Rig& r, StatisticsApp* st, int page, std::uint8_t fkey) {
    for (int k = 0; k < 3 && label(r, st, 0) != (page == 0 ? "GRAPH" : page == 1 ? "SRT-A" : "ADD"); ++k) r.key(pk::F6);
    r.key(fkey);
}
} // namespace

int main() {
    board::init();
    Rig r;
    Workspace& ws = r.ws();
    // ================================================================ 3.1 editor delle liste
    {
        StatisticsApp* st = openStat(r);
        // E01: tre pagine della barra
        T("E01 pagina 1", label(r, st, 0) == "GRAPH" && label(r, st, 1) == "CALC" && label(r, st, 2) == "TEST" && label(r, st, 3) == "INTR" &&
                          label(r, st, 4) == "DIST" && label(r, st, 5) == "\xE2\x96\xB6");
        r.key(pk::F6);
        T("E01 pagina 2", label(r, st, 0) == "SRT-A" && label(r, st, 1) == "SRT-D" && label(r, st, 2) == "DEL" && label(r, st, 3) == "DEL-ALL" && label(r, st, 4) == "INS");
        r.key(pk::F6);
        T("E01 pagina 3", label(r, st, 0) == "ADD" && label(r, st, 1) == "JUMP" && label(r, st, 2) == "COLOR" && label(r, st, 3) == "FILE" && label(r, st, 4) == "CSV");
        r.key(pk::F6);
        T("E01 ritorno alla pagina 1", label(r, st, 0) == "GRAPH");
        // E02: SRT-A su tre liste con la base al centro
        clearLists(ws);
        ws.lists[0] = {3, 1, 2}; ws.lists[1] = {30, 10, 20}; ws.lists[2] = {300, 100, 200};
        pageKey(r, st, 1, pk::F1);
        r.enter("3"); r.enter("2"); r.enter("1"); r.enter("3");
        T("E02 SRT-A tre liste", !r.error() && ws.lists[1] == std::vector<double>({10, 20, 30}) && ws.lists[0] == std::vector<double>({1, 2, 3}) &&
                                 ws.lists[2] == std::vector<double>({100, 200, 300}));
        ws.lists[0] = {5, 4, 6};
        pageKey(r, st, 1, pk::F1); r.enter("1"); r.enter("1");
        T("E02 SRT-A una lista", ws.lists[0] == std::vector<double>({4, 5, 6}));
        ws.lists[3] = {1, 2};
        pageKey(r, st, 1, pk::F1); r.enter("2"); r.enter("1"); r.enter("4");
        T("E02 lunghezze diverse: Dimension ERROR", r.error() && errText(r).find("lunghezza") != std::string::npos);
        r.closeError();
        pageKey(r, st, 1, pk::F1); r.enter("7");
        T("E02 How Many fuori 1-6", r.error());
        r.closeError();
        // E03: SRT-D su due liste
        ws.lists[0] = {1, 3, 2}; ws.lists[1] = {10, 30, 20};
        pageKey(r, st, 1, pk::F2); r.enter("2"); r.enter("1"); r.enter("2");
        T("E03 SRT-D", ws.lists[0] == std::vector<double>({3, 2, 1}) && ws.lists[1] == std::vector<double>({30, 20, 10}));
        // E04: DEL (tasto F e Backspace)
        clearLists(ws);
        ws.lists[0] = {1, 2, 3};
        r.key(pk::Home); r.key(pk::Down);
        pageKey(r, st, 1, pk::F3);
        T("E04 DEL", ws.lists[0] == std::vector<double>({1, 3}));
        r.key(pk::Backspace);
        T("E04 Backspace", ws.lists[0] == std::vector<double>({1}));
        // E05: DEL-ALL con la conferma; il sottonome resta
        ws.lists[0] = {1, 2, 3}; ws.setListName(0, "QTY");
        pageKey(r, st, 1, pk::F4); r.key(pk::F6);
        T("E05 DEL-ALL No", ws.lists[0].size() == 3);
        pageKey(r, st, 1, pk::F4); r.key(pk::F1);
        T("E05 DEL-ALL Si'", ws.lists[0].empty() && ws.listName(0) == "QTY");
        ws.setListName(0, "");
        // E06: INS sopra il cursore, E07: ADD sotto
        ws.lists[0] = {1, 2, 3};
        r.key(pk::Home); r.key(pk::Down);
        pageKey(r, st, 1, pk::F5);
        T("E06 INS", ws.lists[0] == std::vector<double>({1, 0, 2, 3}) && st->cursorRow() == 1);
        pageKey(r, st, 2, pk::F1);
        T("E07 ADD", ws.lists[0] == std::vector<double>({1, 0, 0, 2, 3}) && st->cursorRow() == 2);
        ws.lists[0] = {1, 2, 3};
        // E08: JUMP TOP, BOTTOM, ROW
        pageKey(r, st, 2, pk::F2); r.key(pk::F2);
        T("E08 BOTTOM", st->cursorRow() == 2);
        r.key(pk::F1);
        T("E08 TOP", st->cursorRow() == 0);
        r.key(pk::F3); r.enter("2");
        T("E08 ROW", st->cursorRow() == 1);
        r.key(pk::Esc);
        // E09: riga SUB; con Sub Name Off la testa e' subito sopra la prima cella
        r.key(pk::Home); r.key(pk::Up);
        T("E09 riga SUB", st->cursorRow() == StatisticsApp::SubRow);
        r.enter("PESO");
        T("E09 sottonome", ws.listName(0) == "PESO" && st->cursorRow() == 0);
        ws.settings.subName = false;
        r.key(pk::Up);
        T("E09 Sub Name Off", st->cursorRow() == StatisticsApp::HeadRow);
        r.key(pk::Down);
        ws.settings.subName = true;
        // E10: COLOR della lista, dati disegnati nel colore
        pageKey(r, st, 2, pk::F3); r.key(pk::Down); r.key(pk::Enter);        // Red
        redraw(r);
        T("E10 colore della lista", ws.statistics.listColor[0] == 1 && colorPixels(statplot::paletteColor(1)) > 0);
        // E11: formula nella testa di List 2, ricalcolo, ciclo, modifica a mano
        r.key(pk::Right); r.key(pk::Home); r.key(pk::Up); r.key(pk::Up);
        T("E11 testa della lista", st->cursorRow() == StatisticsApp::HeadRow && st->cursorColumn() == 1);
        r.enter("List 1*2");
        T("E11 formula", !r.error() && ws.statistics.listFormula[1] == "List 1*2" && ws.lists[1] == std::vector<double>({2, 4, 6}));
        r.key(pk::Left); r.key(pk::Down); r.key(pk::Down); r.enter("10");                 // List 1[1] = 10
        T("E11 ricalcolo", ws.lists[0][0] == 10 && ws.lists[1][0] == 20);
        r.key(pk::Home); r.key(pk::Up); r.key(pk::Up); r.enter("List 2+1");                // List 1 = List 2 + 1: ciclo
        T("E11 Circular ERROR", r.error() && errText(r).find("Circular") != std::string::npos && ws.statistics.listFormula[0].empty());
        r.closeError();
        {
            Workspace back;
            const auto blob = serializeWorkspace(ws);
            T("E11 formula nel record", deserializeWorkspace(blob.data(), blob.size(), back) && back.statistics.listFormula[1] == "List 1*2");
        }
        r.key(pk::Right); r.key(pk::Down); r.key(pk::Down); r.enter("7");                // a mano in List 2
        T("E11 modifica a mano toglie la formula", ws.statistics.listFormula[1].empty() && ws.lists[1][0] == 7);
        // E12: List File dall'editor
        pageKey(r, st, 2, pk::F4); r.key(pk::Down); r.key(pk::Enter);         // File 2
        T("E12 File 2", ws.settings.listFile == 1 && ws.lists[0].empty());
        redraw(r);
        pageKey(r, st, 2, pk::F4); r.key(pk::Enter);                          // File 1
        T("E12 File 1", ws.settings.listFile == 0 && ws.lists[0].size() == 3);
        // E13: CSV SAVE·AS e LOAD dalla terza pagina (archivio interno)
        ws.settings.fileStore = Settings::FileStore::Internal;
        clearLists(ws);
        ws.lists[0] = {1, 2}; ws.lists[1] = {3};
        for (int i = 0; i < 26; ++i) r.key(pk::Left);
        pageKey(r, st, 2, pk::F5); r.key(pk::F2); r.enter("V118");
        T("E13 CSV SAVE·AS", !r.error());
        clearLists(ws);
        pageKey(r, st, 2, pk::F5); r.key(pk::F1); r.key(pk::Enter);
        T("E13 CSV LOAD", !r.error() && ws.lists[0] == std::vector<double>({1, 2}) && ws.lists[1] == std::vector<double>({3}));
        r.key(pk::Esc);
        r.home();
    }
    // ================================================================ 3.2 grafici
    {
        clearLists(ws);
        ws.statistics = stats::PlusState{};
        ws.lists[0] = {1, 2, 3, 4, 5, 6}; ws.lists[1] = {2.1, 3.9, 6.2, 7.8, 10.1, 12.2}; ws.lists[2] = {1, 2, 0, 1, 1, 3};
        StatisticsApp* st = openStat(r);
        // G01: menu GRAPH e GRAPH1
        r.key(pk::F1);
        T("G01 menu", label(r, st, 0) == "GRAPH1" && label(r, st, 1) == "GRAPH2" && label(r, st, 2) == "GRAPH3" && label(r, st, 3) == "SELECT" && label(r, st, 5) == "SET");
        r.key(pk::F1);
        T("G01 GRAPH1", !r.error() && st->screen() == StatisticsApp::Screen::Graph && st->drawnPlots() == 1);
        const std::size_t blue = colorPixels(statplot::paletteColor(0));
        T("G04 Scatter disegnato", blue > 0);
        r.key(pk::Esc);
        // G02 SET: voci secondo il tipo; G12 Pie; R16 DefG
        r.key(pk::F1); r.key(pk::F6);
        T("G02 SET", st->screen() == StatisticsApp::Screen::Set && fieldIndex(st, "YList") >= 0 && fieldIndex(st, "Mark Type") >= 0 && fieldIndex(st, "Color Link") >= 0);
        setChoice(r, st, "Graph Type", static_cast<int>(stats::GraphType::Pie));
        T("G02 voci di Pie", ws.statistics.graphs[0].type == stats::GraphType::Pie && fieldIndex(st, "Display") >= 0 && fieldIndex(st, "YList") < 0);
        setChoice(r, st, "% Sto List", 5);
        r.key(pk::Esc);
        r.key(pk::F1); r.key(pk::F1);
        double sum = 0; for (double v : ws.lists[0]) sum += v;
        T("G12 Pie e % Sto List", !r.error() && ws.lists[4].size() == 6 && near(ws.lists[4][0], 100 / sum));
        r.key(pk::Esc);
        ws.lists[5] = {1, -1};
        ws.statistics.graphs[0].pieData = 5;
        r.key(pk::F1); r.key(pk::F1);
        T("G12 Pie con dato negativo", r.error());
        r.closeError();
        r.key(pk::F1); r.key(pk::F6); r.key(pk::F5);
        T("R16 DefG", ws.statistics.graphs[0] == stats::defaultGraph(0));
        r.key(pk::Esc);
        // G03 SELECT: tre grafici accesi e DRAW insieme
        r.key(pk::F1); r.key(pk::F4);
        T("G03 SELECT", st->screen() == StatisticsApp::Screen::Select);
        r.key(pk::Down); r.key(pk::F1); r.key(pk::Down); r.key(pk::F1);
        T("G03 DrawOn", ws.statistics.graphs[1].on && ws.statistics.graphs[2].on);
        r.key(pk::F6);
        T("G03 DRAW di tre grafici", !r.error() && st->drawnPlots() == 3);
        r.key(pk::Esc);
        ws.statistics.graphs[1].on = ws.statistics.graphs[2].on = false;
        // G04 frequenza 0: il punto non si disegna; G18 Graph Color
        ws.statistics.graphs[0].freq = 3;                                      // List 3 con uno 0
        r.key(pk::F1); r.key(pk::F1);
        T("G04 frequenza 0 non disegnata", colorPixels(statplot::paletteColor(0)) < blue);
        r.key(pk::Esc);
        ws.statistics.graphs[0].freq = 0;
        ws.statistics.graphs[0].color = 1;
        r.key(pk::F1); r.key(pk::F1);
        T("G18 Graph Color", colorPixels(statplot::paletteColor(1)) > 0 && colorPixels(statplot::paletteColor(0)) == 0);
        r.key(pk::Esc);
        ws.statistics.graphs[0].color = 0;
        ws.statistics.graphs[0].colorLink = 2; ws.statistics.listColor[0] = 3;  // OnData: colore di List 1
        r.key(pk::F1); r.key(pk::F1);
        T("G18 Color Link OnData", colorPixels(statplot::paletteColor(3)) > 0);
        r.key(pk::Esc);
        ws.statistics.graphs[0].colorLink = 0;
        // G05 xyLine, G06 NPPlot, G09 N-Dist, G13 regressione come tipo, G11 Bar: un tipo per volta
        auto drawType = [&](stats::GraphType t) {
            ws.statistics.graphs[0].type = t;
            r.key(pk::F1); r.key(pk::F1);
            const bool ok = !r.error() && st->screen() == StatisticsApp::Screen::Graph;
            redraw(r);
            const std::size_t px = colorPixels(statplot::paletteColor(0));
            r.closeError();
            if (st->screen() == StatisticsApp::Screen::Graph) r.key(pk::Esc);
            return ok ? px : 0;
        };
        T("G05 xyLine", drawType(stats::GraphType::XYLine) > blue);
        T("G06 NPPlot", drawType(stats::GraphType::NPPlot) > 0);
        T("G09 N-Dist", drawType(stats::GraphType::NDist) > 0);
        for (int t = static_cast<int>(stats::GraphType::Linear); t <= static_cast<int>(stats::GraphType::Logistic); ++t)
            ::testutil::report(drawType(static_cast<stats::GraphType>(t)) > blue, "G13 regressione come tipo di grafico", __FILE__, __LINE__,
                               stats::graphTypeName(static_cast<stats::GraphType>(t)));
        ws.statistics.graphs[0].bar = {1, 2, 0};
        T("G11 Bar", drawType(stats::GraphType::Bar) > 0);
        ws.statistics.graphs[0].stick = 1;
        T("G11 Bar orizzontale", drawType(stats::GraphType::Bar) > 0);
        // G08 MedBox con gli outlier, G16 TRACE dei cinque valori
        ws.lists[3] = {1, 2, 3, 4, 5, 6, 7, 30};
        ws.statistics.graphs[0] = stats::defaultGraph(0);
        ws.statistics.graphs[0].type = stats::GraphType::MedBox; ws.statistics.graphs[0].xList = 3; ws.statistics.graphs[0].outliers = true;
        r.key(pk::F1); r.key(pk::F1); r.key(pk::F2);
        T("G08 MedBox", !r.error() && st->traceText().find("minX=1") != std::string::npos);
        r.key(pk::Right); r.key(pk::Right); r.key(pk::Right); r.key(pk::Right);
        T("G16 TRACE MedBox maxX", st->traceText().find("maxX=30") != std::string::npos);
        r.key(pk::Esc); r.key(pk::Esc);
        // G07 Histogram: Start e Width chiesti prima, classi nel TRACE; G10 Broken; VARS H-Start/H-pitch
        ws.statistics.graphs[0] = stats::defaultGraph(0);
        ws.statistics.graphs[0].type = stats::GraphType::Hist;
        r.key(pk::F1); r.key(pk::F1);
        T("G07 Start/Width", st->screen() == StatisticsApp::Screen::HistAsk && st->form().fields[0].text == "1" && st->form().fields[1].text == "0.5");
        setNumber(r, st, "Start", "0"); setNumber(r, st, "Width", "2");
        r.key(pk::Enter);
        T("G07 Histogram", !r.error() && st->screen() == StatisticsApp::Screen::Graph && ws.statistics.graphs[0].hWidth == 2);
        r.key(pk::F2);
        T("G07 TRACE della classe", st->traceText().find("X=0") != std::string::npos && st->traceText().find("f=1") != std::string::npos);
        T("G07 H-Start, H-pitch", resolve(ws, "stat_hstart") == 0 && resolve(ws, "stat_hpitch") == 2);
        r.key(pk::Esc); r.key(pk::Esc);
        ws.statistics.graphs[0].type = stats::GraphType::Broken;
        r.key(pk::F1); r.key(pk::F1); r.key(pk::Enter); r.key(pk::Enter);    // EXE su Start, EXE su Width
        T("G10 Broken", !r.error() && st->screen() == StatisticsApp::Screen::Graph);
        r.key(pk::Esc);
        // G14 Stat Wind Manual (V-Window di Graph), G19 V-Window dal grafico
        ws.statistics.graphs[0] = stats::defaultGraph(0);
        ws.view.xmin = -10; ws.view.xmax = 20;
        ws.statistics.statWindManual = true;
        r.key(pk::F1); r.key(pk::F1); r.key(pk::F3);
        T("G14 Stat Wind Manual", st->screen() == StatisticsApp::Screen::VWindow && st->form().fields[0].text == "-10" && st->form().fields[1].text == "20");
        setNumber(r, st, "Xmax", "15");
        r.key(pk::Esc);
        T("G19 V-Window", st->screen() == StatisticsApp::Screen::Graph && ws.view.xmax == 15);
        r.key(pk::Esc);
        ws.statistics.statWindManual = false;
        r.key(pk::F1); r.key(pk::F1); r.key(pk::F3);
        T("G14 Stat Wind Auto", std::stod(st->form().fields[0].text) > 0 && std::stod(st->form().fields[1].text) < 7);
        r.key(pk::Esc);
        // G16 TRACE su Scatter
        r.key(pk::F2);
        T("G16 TRACE", st->traceText().find("X=1") != std::string::npos && st->traceText().find("Y=2.1") != std::string::npos);
        r.key(pk::Right);
        T("G16 TRACE punto seguente", st->traceText().find("X=2") != std::string::npos);
        r.key(pk::Esc); r.key(pk::Esc);
        // G17 CALC dal grafico con le liste di StatGraph2 e DRAW della curva
        ws.lists[6] = {1, 2, 3, 4}; ws.lists[7] = {3, 5, 7, 9};
        ws.statistics.graphs[1].xList = 6; ws.statistics.graphs[1].yList = 7;
        r.key(pk::F1); r.key(pk::F2);
        r.key(pk::F1); r.key(pk::F3); r.key(pk::F1);                           // CALC > X > ax+b
        r.settle();
        T("G17 CALC dal grafico", !r.error() && st->screen() == StatisticsApp::Screen::Results && near(rowValue(st, "a"), 2) && near(rowValue(st, "b"), 1));
        r.key(pk::F6);
        T("G17 DRAW della regressione", st->screen() == StatisticsApp::Screen::Graph && colorPixels(statplot::paletteColor(1)) > 0);
        r.key(pk::Esc); r.key(pk::Esc);
        r.home();
    }
    // ================================================================ 3.3 CALC e regressioni
    {
        clearLists(ws);
        ws.statistics = stats::PlusState{};
        ws.settings.statXList = 0; ws.settings.statYList = 1;
        ws.lists[0] = {1, 2, 3, 4, 5}; ws.lists[1] = {2, 4.1, 5.9, 8.2, 9.8}; ws.lists[2] = {1, 2, 1, 1, 1};
        StatisticsApp* st = openStat(r);
        // C01 menu CALC e REG su due pagine
        r.key(pk::F2);
        T("C01 CALC", label(r, st, 0) == "1-VAR" && label(r, st, 1) == "2-VAR" && label(r, st, 2) == "REG" && label(r, st, 5) == "SET");
        r.key(pk::F3);
        T("C01 REG pagina 1", label(r, st, 0) == "X" && label(r, st, 1) == "Med" && label(r, st, 4) == "X\xE2\x81\xB4");
        r.key(pk::F6);
        T("C01 REG pagina 2", label(r, st, 0) == "Log" && label(r, st, 1) == "Exp" && label(r, st, 2) == "Pwr" && label(r, st, 3) == "Sin" && label(r, st, 4) == "Lgst");
        r.key(pk::Esc); r.key(pk::Esc);
        // C03 1-VAR (Mod:n compreso), G15 Q1Q3 Type
        r.key(pk::F2); r.key(pk::F1);
        T("C03 1-VAR", st->results().rows.size() == 14 && near(rowValue(st, "Q1"), 1.5) && near(rowValue(st, "Mod:n"), 5) && near(rowValue(st, "n"), 5));
        r.key(pk::Esc);
        ws.statistics.quartiles = stats::QuartileRule::OnData;
        r.key(pk::F2); r.key(pk::F1);
        T("G15 Q1Q3 OnData", near(rowValue(st, "Q1"), 2) && near(rowValue(st, "Q3"), 4));
        r.key(pk::Esc);
        ws.statistics.quartiles = stats::QuartileRule::Std;
        // C02 SET di CALC: 1Var Freq = List 3 e 2Var Freq
        r.key(pk::F2); r.key(pk::F6);
        T("C02 SET", st->screen() == StatisticsApp::Screen::CalcSet);
        setChoice(r, st, "1Var Freq", 3); setChoice(r, st, "2Var Freq", 3);
        r.key(pk::Esc);
        T("C02 SET salvato", ws.statistics.var1Freq == 3 && ws.statistics.var2Freq == 3);
        r.key(pk::F2); r.key(pk::F1);
        T("C03 1-VAR con le frequenze", near(rowValue(st, "n"), 6) && near(rowValue(st, "\xEE\x80\x81"), 17.0 / 6));
        r.key(pk::Esc);
        // C04 2-VAR con le frequenze
        r.key(pk::F2); r.key(pk::F2);
        T("C04 2-VAR", st->results().rows.size() == 16 && near(rowValue(st, "n"), 6) && near(rowValue(st, "\xCE\xA3xy"), 1 * 2 + 2 * 2 * 4.1 + 3 * 5.9 + 4 * 8.2 + 5 * 9.8));
        r.key(pk::Esc);
        // R18 regressione con 2Var Freq = dati ripetuti
        r.key(pk::F2); r.key(pk::F3); r.key(pk::F1); r.key(pk::F1); r.settle();
        const stats::Fit rep = stats::fit(stats::Reg::LinearAx, {1, 2, 2, 3, 4, 5}, {2, 4.1, 4.1, 5.9, 8.2, 9.8});
        T("R18 frequenze", near(rowValue(st, "a"), rep.coef[0]) && near(rowValue(st, "b"), rep.coef[1]));
        r.key(pk::Esc);
        ws.statistics.var1Freq = ws.statistics.var2Freq = 0;
        // R13 righe per tipo, R14 residui (Resid List), R01/R02 ax+b e a+bx
        ws.statistics.residList = 6;
        r.key(pk::F2); r.key(pk::F3); r.key(pk::F1); r.key(pk::F1); r.settle();
        const stats::Fit lin = stats::fit(stats::Reg::LinearAx, ws.lists[0], ws.lists[1]);
        T("R01 ax+b", st->results().rows.size() == 5 && near(rowValue(st, "a"), lin.coef[0]) && near(rowValue(st, "r"), lin.r) && near(rowValue(st, "MSe"), lin.mse));
        const auto res = stats::residuals(lin, ws.lists[0], ws.lists[1]);
        T("R14 Resid List", ws.lists[5].size() == 5 && near(ws.lists[5][2], res[2]));
        r.key(pk::Esc);
        r.key(pk::F2); r.key(pk::F3); r.key(pk::F1); r.key(pk::F2); r.settle();
        T("R02 a+bx", near(rowValue(st, "a"), lin.coef[1]) && near(rowValue(st, "b"), lin.coef[0]) && ws.settings.statRegType == 11);
        r.key(pk::Esc);
        ws.statistics.residList = 0;
        const struct { std::uint8_t f1, f2, f3; std::size_t rows; stats::Reg type; } regs[] = {
            {pk::F3, pk::F2, 0, 2, stats::Reg::MedMed}, {pk::F3, pk::F3, 0, 5, stats::Reg::Quad}, {pk::F3, pk::F4, 0, 6, stats::Reg::Cubic},
            {pk::F3, pk::F5, 0, 7, stats::Reg::Quart}};
        for (const auto& g : regs) {
            r.key(pk::F2); r.key(g.f1); r.key(g.f2); r.settle();
            ::testutil::report(!r.error() && st->results().rows.size() == g.rows && ws.settings.statRegType == static_cast<std::uint8_t>(g.type),
                               "R13 righe della regressione", __FILE__, __LINE__, stats::regTitle(g.type));
            r.closeError(); r.key(pk::Esc);
        }
        // pagina 2: Log, Exp (ae^bx, ab^x), Pwr, Sin, Lgst
        ws.lists[0] = {1, 2, 3, 4, 5, 6, 7, 8}; ws.lists[1] = {2.0, 3.1, 4.9, 7.2, 11.8, 17.0, 27.5, 41.0};
        const struct { std::uint8_t a, b; std::size_t rows; stats::Reg type; } regs2[] = {
            {pk::F1, 0, 5, stats::Reg::Log}, {pk::F2, pk::F1, 5, stats::Reg::ExpE}, {pk::F2, pk::F2, 5, stats::Reg::ExpAb}, {pk::F3, 0, 5, stats::Reg::Power},
            {pk::F4, 0, 5, stats::Reg::Sin}, {pk::F5, 0, 4, stats::Reg::Logistic}};
        for (const auto& g : regs2) {
            r.key(pk::F2); r.key(pk::F3); r.key(pk::F6); r.key(g.a); if (g.b) r.key(g.b);
            r.settle();
            const stats::Fit f = stats::fit(g.type, ws.lists[0], ws.lists[1]);
            ::testutil::report(!r.error() && st->results().rows.size() == g.rows && near(rowValue(st, "a"), f.coef[0], 1e-8),
                               "R07-R12 regressione", __FILE__, __LINE__, stats::regTitle(g.type));
            r.closeError(); r.key(pk::Esc);
        }
        // R11 Sin: lavoro a passi (schermata in corso), AC = Break
        ws.lists[0].clear(); ws.lists[1].clear();
        for (int i = 0; i < 400; ++i) { ws.lists[0].push_back(i * 0.05); ws.lists[1].push_back(std::sin(0.9 * i * 0.05)); }
        r.key(pk::F2); r.key(pk::F3); r.key(pk::F6); r.key(pk::F4);
        T("R11 Sin in step::Task", st->screen() == StatisticsApp::Screen::Running);
        r.key(pk::Break);
        T("R11 AC = Break", r.error() && errText(r).find("Break") != std::string::npos);
        r.closeError();
        T("R11 dopo Break", st->screen() != StatisticsApp::Screen::Running);
        // R15 COPY in Y7, R17 x̂ e ŷ in Run-Matrix per i tipi nuovi
        ws.lists[0] = {1, 2, 3, 4, 5, 6, 7, 8}; ws.lists[1] = {2.0, 3.1, 4.9, 7.2, 11.8, 17.0, 27.5, 41.0};
        r.key(pk::F2); r.key(pk::F3); r.key(pk::F6); r.key(pk::F5); r.settle();     // Logistic
        const stats::Fit lg = stats::fit(stats::Reg::Logistic, ws.lists[0], ws.lists[1]);
        for (auto& fn : ws.functions) fn.expr.clear();                         // l'elenco parte da Y1 (prima vuota)
        r.key(pk::F5);
        T("R15 COPY", st->screen() == StatisticsApp::Screen::Copy);
        for (int k = 0; k < 6; ++k) r.key(pk::Down);
        r.key(pk::Enter);
        T("R15 COPY in Y7", ws.functions[6].expr == lg.expression() && st->screen() == StatisticsApp::Screen::Results);
        T("R17 y^ Logistic", near(eval(ws, "y_hat(3.5)"), lg.predict(3.5), 1e-9));
        T("R17 x^ Logistic", near(eval(ws, "x_hat(" + std::to_string(lg.predict(3.5)) + ")"), 3.5, 1e-6));
        r.key(pk::Esc);
        r.key(pk::F2); r.key(pk::F3); r.key(pk::F6); r.key(pk::F4); r.settle();     // Sin
        const stats::Fit sn = stats::fit(stats::Reg::Sin, ws.lists[0], ws.lists[1]);
        ws.settings.angle = AngleMode::Degrees; ws.apply();
        T("R17 y^ Sin in radianti anche in Deg", near(eval(ws, "y_hat(2)"), sn.predict(2), 1e-9));
        ws.settings.angle = AngleMode::Radians; ws.apply();
        bool sinErr = false;
        try { (void)eval(ws, "x_hat(1)"); } catch (const MathError&) { sinErr = true; }
        T("R17 x^ Sin: Math ERROR", sinErr);
        r.key(pk::Esc);
        r.key(pk::F2); r.key(pk::F3); r.key(pk::F1); r.key(pk::F2); r.settle();     // a+bx
        const stats::Fit ab = stats::fit(stats::Reg::LinearA, ws.lists[0], ws.lists[1]);
        T("R17 y^ a+bx", near(eval(ws, "y_hat(2)"), ab.coef[0] + 2 * ab.coef[1]));
        r.key(pk::Esc);
        // R03 Med-Med: punti di VARS › STAT › PTS
        r.key(pk::F2); r.key(pk::F3); r.key(pk::F2); r.settle();
        const stats::Fit md = stats::fit(stats::Reg::MedMed, ws.lists[0], ws.lists[1]);
        T("R03 Med-Med PTS", near(resolve(ws, "stat_x1"), md.pts[0]) && near(resolve(ws, "stat_y3"), md.pts[5]));
        r.key(pk::Esc);
        // C05 / A01: VARS › STAT con i valori dell'ultimo calcolo (non ricalcolati), anche dopo il riavvio
        r.key(pk::F2); r.key(pk::F1);
        const double mean = resolve(ws, "stat_meanx");
        r.key(pk::Esc);
        ws.lists[0][0] = 100;
        T("A01 valori dell'ultimo calcolo", mean == 4.5 && resolve(ws, "stat_meanx") == 4.5);
        {
            Workspace back;
            const auto blob = serializeWorkspace(ws);
            double v = 0;
            T("C05 VARS dopo il riavvio", deserializeWorkspace(blob.data(), blob.size(), back) && back.resolve("stat_meanx", v) && v == 4.5);
        }
        r.home();
    }
    // ================================================================ 3.4 TEST e 3.5 INTR
    {
        clearLists(ws);
        ws.statistics = stats::PlusState{};
        ws.lists[0] = {1.2, 2.3, 2.3, 3.1, 4.8, 5.0, 5.5, 6.7, 7.1, 8.4, 9.9, 10.2};
        ws.lists[1] = {3.4, 4.1, 2.9, 5.6, 6.2, 4.4, 3.8, 5.1, 4.9};
        StatisticsApp* st = openStat(r);
        stats::Sample a; a.list = true; a.x = ws.lists[0];
        stats::Sample b; b.list = true; b.x = ws.lists[1];
        auto runForm = [&]() { r.key(pk::F1); r.settle(); };
        // T01 1-Sample Z (List), poi Var; T13 DRAW; T14 Save Res e VARS RESULT
        r.key(pk::F3); r.key(pk::F1); r.key(pk::F1);
        T("T01 modulo", st->screen() == StatisticsApp::Screen::Form && fieldIndex(st, "List") >= 0 && fieldIndex(st, "Freq") >= 0);
        setNumber(r, st, "\xCE\xBC" "0", "5"); setNumber(r, st, "\xCF\x83", "2.5"); setChoice(r, st, "Save Res", 5);
        runForm();
        const stats::Outcome z1 = stats::zTest1S(a, 5, 2.5, 0);
        T("T01 1-Sample Z", !r.error() && near(rowValue(st, "z"), z1.rows[0].value) && near(rowValue(st, "p"), z1.rows[1].value));
        T("T14 Save Res", ws.lists[4].size() == z1.rows.size() && near(ws.lists[4][1], z1.rows[1].value));
        T("T14 VARS RESULT", near(resolve(ws, "stat_res_p"), z1.rows[1].value) && near(resolve(ws, "stat_in_mean"), z1.rows[2].value));
        r.key(pk::F6);
        T("T13 DRAW", st->screen() == StatisticsApp::Screen::DistGraph && colorPixels(ui::theme::SelectSoft) > 0);
        r.key(pk::Esc); r.key(pk::Esc);
        setChoice(r, st, "Data", 1);
        T("T01 Data: Variable", fieldIndex(st, "\xEE\x80\x81") >= 0 && fieldIndex(st, "List") < 0);
        setNumber(r, st, "\xEE\x80\x81", "6"); setNumber(r, st, "n", "12");
        runForm();
        T("T01 Var", near(rowValue(st, "z"), (6 - 5) / (2.5 / std::sqrt(12.0))));
        r.key(pk::Esc); r.key(pk::Esc);
        // Gli altri test: un modulo per test, valori iniziali o liste, contro il nucleo
        struct Case { const char* id; std::uint8_t k1, k2, k3; std::function<void(StatisticsApp*)> setup; std::function<bool(StatisticsApp*)> check; };
        const std::vector<Case> cases = {
            {"T02 2-Sample Z", pk::F1, pk::F2, 0, [&](StatisticsApp* s) { setNumber(r, s, "\xCF\x83" "1", "2.5"); setNumber(r, s, "\xCF\x83" "2", "1.2"); },
             [&](StatisticsApp* s) { return near(rowValue(s, "z"), stats::zTest2S(a, b, 2.5, 1.2, 0).rows[0].value); }},
            {"T03 1-Prop Z", pk::F1, pk::F3, 0, [&](StatisticsApp* s) { setNumber(r, s, "x", "55"); setNumber(r, s, "n", "100"); },
             [&](StatisticsApp* s) { return near(rowValue(s, "z"), stats::zTest1P(0.5, 55, 100, 0).rows[0].value) && near(rowValue(s, "p\xCC\x82"), 0.55); }},
            {"T04 2-Prop Z", pk::F1, pk::F4, 0, [&](StatisticsApp* s) { setNumber(r, s, "x1", "45"); setNumber(r, s, "n1", "100"); setNumber(r, s, "x2", "60"); setNumber(r, s, "n2", "120"); },
             [&](StatisticsApp* s) { return near(rowValue(s, "z"), stats::zTest2P(45, 100, 60, 120, 0).rows[0].value); }},
            {"T05 1-Sample t", pk::F2, pk::F1, 0, [&](StatisticsApp* s) { setNumber(r, s, "\xCE\xBC" "0", "5"); setChoice(r, s, "\xCE\xBC", 2); },
             [&](StatisticsApp* s) { return near(rowValue(s, "t"), stats::tTest1S(a, 5, 1).rows[0].value) && near(rowValue(s, "p"), stats::tTest1S(a, 5, 1).rows[1].value); }},
            {"T06 2-Sample t Pooled On", pk::F2, pk::F2, 0, [&](StatisticsApp* s) { setChoice(r, s, "Pooled", 1); },
             [&](StatisticsApp* s) { return near(rowValue(s, "df"), 19) && near(rowValue(s, "sp"), stats::tTest2S(a, b, true, 0).rows[7].value); }},
            {"T06 2-Sample t Pooled Off", pk::F2, pk::F2, 0, [&](StatisticsApp* s) { setChoice(r, s, "Pooled", 0); },
             [&](StatisticsApp* s) { return near(rowValue(s, "df"), stats::tTest2S(a, b, false, 0).rows[2].value); }},
            {"T07 LinearReg t", pk::F2, pk::F3, 0, [&](StatisticsApp* s) { setChoice(r, s, "XList", 2); setChoice(r, s, "YList", 3); },
             [&](StatisticsApp* s) { return near(rowValue(s, "t"), stats::tTestReg(ws.lists[2], ws.lists[3], nullptr, 0).rows[0].value) && s->results().rows.size() == 8; }},
            {"T08 chi^2 GOF", pk::F3, pk::F1, 0, [&](StatisticsApp* s) { setChoice(r, s, "Observed", 4); setChoice(r, s, "Expected", 5); setNumber(r, s, "df", "3"); setChoice(r, s, "CNTRB", 7); },
             [&](StatisticsApp* s) { return near(rowValue(s, "\xCF\x87\xC2\xB2"), 4.32) && ws.lists[6].size() == 4 && near(ws.lists[6][0], 49.0 / 25); }},
            {"T09 chi^2 2-way", pk::F3, pk::F2, 0, [&](StatisticsApp*) {},
             [&](StatisticsApp* s) { CalcValue e; return near(rowValue(s, "df"), 2) && ws.collection(CollectionResolver::Memory::Mat, 1, e) && e.rows == 2 && e.cols == 3 && near(e.re[0], 25); }},
            {"T10 2-Sample F", pk::F4, 0, 0, [&](StatisticsApp*) {},
             [&](StatisticsApp* s) { return near(rowValue(s, "F"), stats::fTest2S(a, b, 0).rows[0].value) && near(rowValue(s, "p"), stats::fTest2S(a, b, 0).rows[1].value); }},
        };
        ws.lists[2] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        ws.lists[3] = {1.5, 1.9, 2.8, 4.1, 6.0, 9.2, 13.1, 20.4, 29.8, 45.1, 66.7, 99.0};
        ws.lists[4] = {18, 22, 30, 30}; ws.lists[5] = {25, 25, 25, 25};
        {
            CalcValue m = CalcValue::matrix(2, 3);
            m.re = {20, 30, 25, 30, 20, 25};
            ws.storeMatrix(0, m);
        }
        for (const Case& c : cases) {
            r.key(pk::F3); r.key(c.k1); if (c.k2) r.key(c.k2);
            c.setup(st);
            runForm();
            ::testutil::report(!r.error() && st->screen() == StatisticsApp::Screen::Results && c.check(st), c.id, __FILE__, __LINE__, st->results().title);
            r.closeError();
            r.key(pk::Esc); r.key(pk::Esc);
        }
        // T11 ANOVA a una via in step::Task; AC interrompe; T12 due vie con il grafico d'interazione
        ws.lists[0] = {1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3};
        ws.lists[1] = {12.1, 13.4, 11.8, 12.9, 14.2, 15.1, 13.9, 14.8, 15.4, 11.2, 10.8, 11.9};
        r.key(pk::F3); r.key(pk::F5);
        setChoice(r, st, "Dependent", 1);
        r.key(pk::F1);
        r.settle();
        const stats::Outcome an = stats::anova(ws.lists[0], {}, ws.lists[1]);
        T("T11 ANOVA una via", !r.error() && near(rowValue(st, "A  F"), an.rows[3].value) && near(resolve(ws, "stat_res_fa"), an.rows[3].value));
        r.key(pk::Esc);
        {
            std::vector<double> big(999), grp(999);
            for (int i = 0; i < 999; ++i) { grp[static_cast<std::size_t>(i)] = i % 3; big[static_cast<std::size_t>(i)] = i * 0.37; }
            ws.setList(8, grp); ws.setList(9, big);
            setChoice(r, st, "Factor A", 8); setChoice(r, st, "Dependent", 9);
            r.key(pk::F1);
            T("T11 ANOVA in step::Task", st->screen() == StatisticsApp::Screen::Running);
            r.key(pk::Break);
            T("T11 AC durante l'ANOVA", r.error() && errText(r).find("Break") != std::string::npos);
            r.closeError();
            T("T11 dopo Break: modulo", st->screen() == StatisticsApp::Screen::Form);
        }
        ws.lists[0] = {1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2};
        ws.lists[1] = {1, 1, 2, 2, 3, 3, 1, 1, 2, 2, 3, 3};
        ws.lists[2] = {5.2, 5.8, 6.9, 7.3, 8.1, 7.7, 6.0, 6.6, 7.1, 7.9, 9.8, 10.4};
        setChoice(r, st, "How Many", 1);
        T("T12 Factor B", fieldIndex(st, "Factor B") >= 0);
        setChoice(r, st, "Factor A", 0); setChoice(r, st, "Factor B", 1); setChoice(r, st, "Dependent", 2);
        r.key(pk::F1); r.settle();
        const stats::Outcome an2 = stats::anova(ws.lists[0], ws.lists[1], ws.lists[2]);
        T("T12 ANOVA due vie", !r.error() && near(rowValue(st, "AB F"), an2.rows[13].value) && st->results().rows.size() == 18);
        r.key(pk::F6);
        T("T12 grafico d'interazione", st->screen() == StatisticsApp::Screen::Interaction);
        redraw(r);
        T("T12 linee per livello di B", colorPixels(statplot::paletteColor(0)) > 0 && colorPixels(statplot::paletteColor(1)) > 0);
        r.key(pk::Esc); r.key(pk::Esc); r.key(pk::Esc);
        // I01-I06 intervalli
        ws.lists[0] = {1.2, 2.3, 2.3, 3.1, 4.8, 5.0, 5.5, 6.7, 7.1, 8.4, 9.9, 10.2};
        ws.lists[1] = {3.4, 4.1, 2.9, 5.6, 6.2, 4.4, 3.8, 5.1, 4.9};
        a.x = ws.lists[0]; b.x = ws.lists[1];
        const struct { const char* id; std::uint8_t k1, k2; stats::Outcome want; std::function<void(StatisticsApp*)> setup; } ints[] = {
            {"I01 1-Sample Z Int", pk::F1, pk::F1, stats::zInt1S(a, 0.95, 2.5), [&](StatisticsApp* s) { setNumber(r, s, "\xCF\x83", "2.5"); }},
            {"I02 2-Sample Z Int", pk::F1, pk::F2, stats::zInt2S(a, b, 0.95, 2.5, 1.2), [&](StatisticsApp* s) { setNumber(r, s, "\xCF\x83" "1", "2.5"); setNumber(r, s, "\xCF\x83" "2", "1.2"); }},
            {"I03 1-Prop Z Int", pk::F1, pk::F3, stats::zInt1P(0.95, 55, 100), [&](StatisticsApp* s) { setNumber(r, s, "x", "55"); setNumber(r, s, "n", "100"); }},
            {"I04 2-Prop Z Int", pk::F1, pk::F4, stats::zInt2P(0.95, 45, 100, 60, 120),
             [&](StatisticsApp* s) { setNumber(r, s, "x1", "45"); setNumber(r, s, "n1", "100"); setNumber(r, s, "x2", "60"); setNumber(r, s, "n2", "120"); }},
            {"I05 1-Sample t Int", pk::F2, pk::F1, stats::tInt1S(a, 0.95), [&](StatisticsApp*) {}},
            {"I06 2-Sample t Int", pk::F2, pk::F2, stats::tInt2S(a, b, 0.95, true), [&](StatisticsApp* s) { setChoice(r, s, "Pooled", 1); }},
        };
        for (const auto& c : ints) {
            r.key(pk::F4); r.key(c.k1); r.key(c.k2);
            c.setup(st);
            r.key(pk::F1); r.settle();
            ::testutil::report(!r.error() && near(rowValue(st, "Left"), c.want.rows[0].value) && near(rowValue(st, "Right"), c.want.rows[1].value) &&
                               near(resolve(ws, "stat_int_left"), c.want.rows[0].value), c.id, __FILE__, __LINE__);
            r.closeError();
            r.key(pk::Esc); r.key(pk::Esc);
        }
        r.home();
    }
    // ================================================================ 3.6 DIST
    {
        clearLists(ws);
        StatisticsApp* st = openStat(r);
        // D01 Normal: Ncd con i valori iniziali (Var), Ans (D12), DRAW (D10)
        r.key(pk::F5); r.key(pk::F1);
        T("D01 menu NORM", label(r, st, 0) == "Npd" && label(r, st, 1) == "Ncd" && label(r, st, 2) == "InvN");
        r.key(pk::F2);
        T("D01 modulo Ncd", st->screen() == StatisticsApp::Screen::Form && fieldIndex(st, "Lower") >= 0 && fieldIndex(st, "\xCF\x83") >= 0);
        r.key(pk::F1);
        r.settle();
        T("D01 Ncd", !r.error() && near(rowValue(st, "p"), 0.6826894921, 1e-9) && near(rowValue(st, "z:Low"), -1));
        T("D12 Ans", near(ws.memory.answer(), 0.682689492137, 1e-11));
        T("A02 VARS DIST", near(resolve(ws, "stat_dist_p"), ws.memory.answer()) && near(resolve(ws, "stat_dist_zup"), 1));
        r.key(pk::F6);
        T("D10 DRAW", st->screen() == StatisticsApp::Screen::DistGraph && colorPixels(ui::theme::SelectSoft) > 100);
        r.key(pk::Esc); r.key(pk::Esc); r.key(pk::Esc);
        // InvN Central
        r.key(pk::F5); r.key(pk::F1); r.key(pk::F3);
        setChoice(r, st, "Tail", 2); setNumber(r, st, "Area", "0.8");
        r.key(pk::F1); r.settle();
        T("D01 InvN Central", near(rowValue(st, "x2Inv"), 1.281551565545, 1e-9) && near(rowValue(st, "x1Inv"), -1.281551565545, 1e-9));
        r.key(pk::Esc); r.key(pk::Esc);
        // D02-D08: una funzione per famiglia con i valori iniziali, contro dist_core
        const struct { const char* id; bool page2; std::uint8_t fam, fn; dist::Kind kind; } kinds[] = {
            {"D02 tpd", false, pk::F2, pk::F1, {dist::Family::T, dist::Func::PD}}, {"D02 Invt", false, pk::F2, pk::F3, {dist::Family::T, dist::Func::Inv}},
            {"D03 Cpd", false, pk::F3, pk::F1, {dist::Family::Chi, dist::Func::PD}}, {"D03 InvC", false, pk::F3, pk::F3, {dist::Family::Chi, dist::Func::Inv}},
            {"D04 Fcd", false, pk::F4, pk::F2, {dist::Family::F, dist::Func::CD}}, {"D04 InvF", false, pk::F4, pk::F3, {dist::Family::F, dist::Func::Inv}},
            {"D05 Bcd", false, pk::F5, pk::F2, {dist::Family::Binomial, dist::Func::CD}}, {"D05 InvB", false, pk::F5, pk::F3, {dist::Family::Binomial, dist::Func::Inv}},
            {"D06 Ppd", true, pk::F1, pk::F1, {dist::Family::Poisson, dist::Func::PD}}, {"D06 InvP", true, pk::F1, pk::F3, {dist::Family::Poisson, dist::Func::Inv}},
            {"D07 Gcd", true, pk::F2, pk::F2, {dist::Family::Geometric, dist::Func::CD}}, {"D07 InvG", true, pk::F2, pk::F3, {dist::Family::Geometric, dist::Func::Inv}},
            {"D08 Hpd", true, pk::F3, pk::F1, {dist::Family::Hypergeometric, dist::Func::PD}}, {"D08 InvH", true, pk::F3, pk::F3, {dist::Family::Hypergeometric, dist::Func::Inv}},
        };
        for (const auto& k : kinds) {
            r.key(pk::F5); if (k.page2) r.key(pk::F6);
            r.key(k.fam); r.key(k.fn);
            r.key(pk::F1); r.settle();
            dist::Input in;
            for (const auto& p : dist::paramSpecs(k.kind)) in.values.push_back(p.def);
            const double want = dist::compute(k.kind, in).main;
            ::testutil::report(!r.error() && !st->results().rows.empty() && near(std::stod(st->results().rows[0].second), want, 1e-9) && near(ws.memory.answer(), want, 1e-12),
                               k.id, __FILE__, __LINE__, st->results().title);
            r.closeError();
            r.key(pk::Esc); r.key(pk::Esc);
        }
        // D09 List: Npd con List 1, risultati in ListAns e Save Res; D05 Bcd con U.List (Lower 0)
        ws.lists[0] = {-1, 0, 0.5, 2, 3.5};
        r.key(pk::F5); r.key(pk::F1); r.key(pk::F1);
        setChoice(r, st, "Data", 0);
        T("D09 campi List", fieldIndex(st, "List") >= 0 && fieldIndex(st, "x") < 0);
        setChoice(r, st, "Save Res", 3);
        r.key(pk::F1); r.settle();
        CalcValue la;
        T("D09 ListAns e Save Res", !r.error() && ws.collection(CollectionResolver::Memory::List, CollectionResolver::Answer, la) && la.size() == 5 &&
                                    ws.lists[2].size() == 5 && near(ws.lists[2][1], 0.3989422804014327, 1e-12));
        r.key(pk::Esc); r.key(pk::Esc);
        ws.lists[0] = {0, 3, 7};
        r.key(pk::F5); r.key(pk::F5); r.key(pk::F2);
        setChoice(r, st, "Data", 0);
        T("D05 U.List", fieldIndex(st, "Upper List") >= 0 && fieldIndex(st, "Lower List") < 0);
        r.key(pk::F1); r.settle();
        T("D05 Bcd con lista", !r.error() && st->results().rows.size() == 3 && near(std::stod(st->results().rows[2].second), 0.9453125, 1e-9));
        r.key(pk::Esc); r.key(pk::Esc);
        // D11: 999 inverse della t in lista, AC a meta'
        std::vector<double> areas(999);
        for (int i = 0; i < 999; ++i) areas[static_cast<std::size_t>(i)] = 0.001 + 0.998 * i / 998.0;
        ws.setList(0, areas);
        r.key(pk::F5); r.key(pk::F2); r.key(pk::F3);
        setChoice(r, st, "Data", 0);
        r.key(pk::F1);
        for (int i = 0; i < 2; ++i) { board::host::advance(30); if (r.fw.step()) redraw(r); }
        T("D11 lista in corso", st->screen() == StatisticsApp::Screen::Running);
        r.key(pk::Break);
        T("D11 AC = Break", r.error() && errText(r).find("Break") != std::string::npos);
        r.closeError();
        r.home();
    }
    // ================================================================ 3.7 SET UP e record
    {
        StatisticsApp* st = openStat(r);
        r.key(pk::F10);
        T("U01 SET UP", st->screen() == StatisticsApp::Screen::Setup && fieldIndex(st, "Stat Wind") >= 0 && fieldIndex(st, "Resid List") >= 0 &&
                        fieldIndex(st, "Q1Q3 Type") >= 0 && fieldIndex(st, "List File") >= 0 && fieldIndex(st, "Sub Name") >= 0);
        setChoice(r, st, "Stat Wind", 1); setChoice(r, st, "Resid List", 4); setChoice(r, st, "Q1Q3 Type", 1);
        r.key(pk::Esc);
        T("U01 valori", ws.statistics.statWindManual && ws.statistics.residList == 4 && ws.statistics.quartiles == stats::QuartileRule::OnData);
        // U02: la sezione 0x0101 nel record v12; danneggiata -> isolata
        auto blob = serializeWorkspace(ws);
        Workspace back;
        T("U02 sezione nel record", deserializeWorkspace(blob.data(), blob.size(), back) && back.statistics == ws.statistics);
        bool found = false;
        for (std::size_t i = 0; i + 4 < blob.size(); ++i) {
            if (blob[i] == 0x01 && blob[i + 1] == 0x01 && blob[i + 2] == 0x01) {   // id 0x0101 (LE), versione 1
                std::vector<std::uint8_t> bad = blob;
                bad[i + 12] ^= 0x5A;
                Workspace t;
                if (deserializeWorkspace(bad.data(), bad.size(), t) && t.statistics == stats::PlusState{} &&
                    std::find(t.damagedSections.begin(), t.damagedSections.end(), 0x0101) != t.damagedSections.end()) found = true;
                break;
            }
        }
        T("U02 sezione danneggiata isolata", found);
        r.home();
    }
    return testutil::finish("statistics_flow_test");
}
