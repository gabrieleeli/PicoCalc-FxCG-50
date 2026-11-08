// The registry of modes (see app_registry.hpp). Keep it in sync with the
// sources: tests/architecture_test.cpp fails when a mode uses a service,
// a workspace section or a link that is not declared here.
#include "app_registry.hpp"
#include "graphics_engine.hpp"

namespace fx::app {
namespace {
using gfx::rgb888To565;
namespace s = service;
namespace d = data;
constexpr std::uint32_t AllData = (1u << data::Count) - 1;
constexpr std::uint32_t Ui = s::Widgets | s::Editor | s::Format;

const char* const kRunMatrixSrc[] = {"src/apps/run_matrix_app.cpp", "src/run_matrix.cpp", nullptr};
const char* const kStatisticsSrc[] = {"src/apps/statistics_app.cpp", nullptr};
const char* const kGraphSrc[] = {"src/apps/graph_app.cpp", "src/apps/graph_plot.cpp", "src/apps/graph_files.cpp", nullptr};
const char* const kTableSrc[] = {"src/apps/table_app.cpp", nullptr};
const char* const kEquationSrc[] = {"src/apps/equation_app.cpp", nullptr};
const char* const kMemorySrc[] = {"src/apps/memory_app.cpp", nullptr};
const char* const kSystemSrc[] = {"src/apps/system_app.cpp", nullptr};
const char* const kConversionSrc[] = {"src/apps/conversion_app.cpp", nullptr};
const char* const kAiChatSrc[] = {"src/apps/ai_chat_app.cpp", nullptr};
const char* const kSymbolsSrc[] = {"src/apps/symbols_app.cpp", nullptr};

const AppInfo kApps[AppCount] = {
    {AppId::RunMatrix, "Run-Matrix", "1", rgb888To565(232, 96, 30), AppStatus::Available,
     "Calcoli e risultati: input naturale, risultati esatti e decimali, cronologia modificabile, variabili, Base-N, DMS",
     nullptr, s::Expression | s::Exact | Ui | s::MathRender | s::MathCore | s::RunCore | s::Storage,
     d::Settings | d::Memory | d::History, 0, kRunMatrixSrc},
    {AppId::Statistics, "Statistics", "2", rgb888To565(30, 160, 90), AppStatus::Available,
     "Liste, statistiche 1/2 variabili, regressioni, grafici statistici, distribuzioni, test e intervalli",
     nullptr, s::Expression | Ui | s::Stats | s::Storage, d::Settings | d::Lists | d::Functions, 0, kStatisticsSrc},
    {AppId::EActivity, "eActivity", "3", rgb888To565(220, 40, 140), AppStatus::Planned,
     "Documenti con righe di testo e righe di calcolo valutate dal nucleo di Run-Matrix",
     "Documenti misti testo/formule non ancora implementati.",
     s::Expression | s::Exact | Ui | s::MathRender | s::RunCore | s::Storage | s::Models, d::Settings | d::Memory, 0, nullptr},
    {AppId::Spreadsheet, "Spreadsheet", "4", rgb888To565(70, 150, 40), AppStatus::Planned,
     "Foglio A..Z x 999 righe con valori e formule (=A1+B2), statistiche sulle colonne",
     "Foglio di calcolo con formule e riferimenti non ancora implementato.",
     s::Expression | Ui | s::Stats | s::Storage | s::Models, d::Settings | d::Memory | d::Lists, 0, nullptr},
    {AppId::Graph, "Graph", "5", rgb888To565(60, 70, 200), AppStatus::Available,
     "Grafici Y=, r=, parametrici, X=, disequazioni (Y1..Y20); trace, zoom, V-Window, sketch, G-Solve, modify, memorie, PICT, progetti .pcgraph su microSD",
     nullptr, s::Expression | Ui | s::MathRender | s::Storage | s::GraphCore | s::Files,
     d::Settings | d::Memory | d::Functions | d::View | d::Table | d::Lists | d::Graph,
     linkTo(AppId::Table), kGraphSrc},
    {AppId::DynaGraph, "Dyna Graph", "6", rgb888To565(40, 110, 220), AppStatus::Planned,
     "Famiglie di grafici al variare di un coefficiente (Start/End/Pitch), sulla V-Window di Graph",
     "Grafici dinamici con parametro variabile non ancora implementati.",
     s::Expression | Ui | s::MathRender | s::Storage | s::Models, d::Settings | d::Memory | d::View, 0, nullptr},
    {AppId::Table, "Table", "7", rgb888To565(120, 70, 200), AppStatus::Available,
     "Tabelle di Y=, r= e parametriche (Y1..Y20) da range o lista, derivate, EDIT/ROW, LISTMEM; GPH-CON/GPH-PLT verso Graph",
     nullptr, s::Expression | Ui | s::MathRender | s::Storage | s::GraphCore, d::Settings | d::Memory | d::Functions | d::View | d::Table | d::Lists | d::Graph,
     linkTo(AppId::Graph), kTableSrc},
    {AppId::Recursion, "Recursion", "8", rgb888To565(40, 60, 160), AppStatus::Planned,
     "Successioni an, an+1, an+2 (fino a tre), tabelle, grafici e diagrammi di fase",
     "Successioni ricorsive an+1 non ancora implementate.",
     s::Expression | Ui | s::Storage | s::Models, d::Settings | d::Memory | d::View, 0, nullptr},
    {AppId::ConicGraphs, "Conic Graphs", "9", rgb888To565(30, 140, 170), AppStatus::Planned,
     "Parabole, circonferenze, ellissi e iperboli in forma cartesiana, polare e parametrica",
     "Grafici di coniche non ancora implementati.",
     s::Expression | Ui | s::Solvers | s::Storage | s::Models, d::Settings | d::View, 0, nullptr},
    {AppId::Equation, "Equation", "A", rgb888To565(240, 140, 20), AppStatus::Available,
     "Sistemi lineari, polinomi di grado 2..6, solver f(x)=g(x), disequazioni",
     nullptr, s::Expression | Ui | s::Solvers | s::Storage, d::Settings | d::Memory | d::Equation, 0, kEquationSrc},
    {AppId::Program, "Program", "B", rgb888To565(170, 160, 20), AppStatus::Planned,
     "Programmi con un linguaggio proprio (non il codice Casio) che usano il motore e il workspace",
     "Linguaggio di programmazione non ancora implementato.",
     s::Expression | s::Exact | s::RunCore | Ui | s::Storage | s::Models,
     d::Settings | d::Memory | d::Lists | d::Functions | d::View, 0, nullptr},
    {AppId::Financial, "Financial", "C", rgb888To565(230, 190, 20), AppStatus::Planned,
     "Interesse semplice e composto (TVM), flussi di cassa, ammortamento, conversioni di tasso",
     "Calcoli finanziari (TVM, interessi) non ancora implementati.",
     s::Expression | Ui | s::Solvers | s::Storage | s::Models, d::Settings | d::Memory, 0, nullptr},
    {AppId::ECon4, "E-CON4", "D", rgb888To565(130, 40, 180), AppStatus::External,
     "Acquisizione dati da sensori esterni (data logger)",
     "Richiede un data logger esterno e il suo protocollo: non supportato dalla PicoCalc.",
     s::Widgets | s::Storage, d::Settings | d::Lists, 0, nullptr},
    {AppId::Link, "Link", "E", rgb888To565(70, 140, 140), AppStatus::Planned,
     "Trasferimento del workspace (formato proprio con CRC) via USB seriale verso un PC",
     "Trasferimento dati via USB/seriale non ancora implementato; il protocollo Link Casio non e' supportato.",
     s::Widgets | s::Storage, AllData, 0, nullptr},
    {AppId::Memory, "Memory", "F", rgb888To565(70, 110, 170), AppStatus::Available,
     "Gestione della memoria: uso, cancellazione selettiva, salvataggio del workspace",
     nullptr, s::Widgets | s::Storage, AllData, 0, kMemorySrc},
    {AppId::System, "System", "G", rgb888To565(60, 130, 150), AppStatus::Available,
     "Impostazioni (SET UP), retroilluminazione, Wi-Fi, relay AI, informazioni e reset",
     nullptr, Ui | s::Network | s::Storage | s::Diag, AllData /* RESET clears the whole workspace */, 0, kSystemSrc},
    {AppId::Geometry, "Geometry", "H", rgb888To565(50, 110, 210), AppStatus::Planned,
     "Costruzioni geometriche (punti, rette, circonferenze, poligoni) e misure",
     "Costruzioni geometriche interattive non ancora implementate.",
     s::Expression | Ui | s::MathCore | s::Storage | s::Models, d::Settings | d::Memory, 0, nullptr},
    {AppId::PicturePlot, "Picture Plot", "I", rgb888To565(40, 170, 80), AppStatus::Planned,
     "Punti su un'immagine di sfondo, liste di coordinate e regressioni",
     "Picture Plot non ancora implementato.",
     s::Expression | Ui | s::Stats | s::Storage | s::Models, d::Settings | d::Lists | d::View, 0, nullptr},
    {AppId::Graph3D, "3D Graph", "J", rgb888To565(70, 60, 190), AppStatus::Planned,
     "Superfici z=f(x,y), sfere, piani, rette nello spazio con finestra 3D",
     "Grafici 3D z=f(x,y) non ancora implementati nell'interfaccia.",
     s::Expression | Ui | s::Storage | s::Models, d::Settings | d::Memory, 0, nullptr},
    {AppId::Conversion, "Conversion", "K", rgb888To565(220, 90, 120), AppStatus::Available,
     "Conversioni di unita' (10 grandezze, prefissi SI); il risultato va in Ans",
     nullptr, s::Expression | Ui | s::Units, d::Settings | d::Memory, 0, kConversionSrc},
    {AppId::Python, "Python", "L", rgb888To565(40, 120, 180), AppStatus::Planned,
     "Script MicroPython con heap separato e output sul display",
     "Runtime MicroPython non integrato in questa build.",
     s::Widgets | s::Storage | s::Models, d::Settings, 0, nullptr},
    {AppId::AiChat, "AI Chat", "M", rgb888To565(0, 150, 200), AppStatus::Available,
     "Conversazione con Claude o ChatGPT tramite relay HTTPS (nessuna chiave sul dispositivo)",
     nullptr, s::Widgets | s::Editor | s::Network | s::Storage, d::Settings | d::Chat, linkTo(AppId::System), kAiChatSrc},
    {AppId::Symbols, "Symbols", "N", rgb888To565(130, 60, 170), AppStatus::Available,
     "Guida di simboli e funzioni", nullptr, s::Widgets | s::Help, 0, 0, kSymbolsSrc},
};

const char* const kHExpression[] = {"expression.hpp", "expression_ast.hpp", nullptr};
const char* const kHExact[] = {"exact.hpp", nullptr};
const char* const kHEditor[] = {"natural_editor.hpp", "line_editor.hpp", nullptr};
const char* const kHRender[] = {"math_renderer.hpp", nullptr};
const char* const kHFormat[] = {"number_format.hpp", nullptr};
const char* const kHSolvers[] = {"solvers.hpp", nullptr};
const char* const kHStats[] = {"statistics.hpp", nullptr};
const char* const kHMathCore[] = {"math_engine.hpp", nullptr};
const char* const kHWidgets[] = {"ui_kit.hpp", "app_widgets.hpp", "graphics_engine.hpp", "font_renderer.hpp", nullptr};
const char* const kHNetwork[] = {"net.hpp", "ai_protocol.hpp", "ai_client.hpp", nullptr};
const char* const kHStorage[] = {"storage.hpp", nullptr};
const char* const kHUnits[] = {"point_a.hpp", nullptr};
const char* const kHHelp[] = {"cheatsheet.hpp", nullptr};
const char* const kHRunCore[] = {"run_matrix.hpp", nullptr};
const char* const kHModels[] = {"app_models.hpp", nullptr};
const char* const kHGraphCore[] = {"graph_model.hpp", "graph_engine.hpp", "graph_solve.hpp", "table_model.hpp", nullptr};
const char* const kHFiles[] = {"file_store.hpp", "graph_file.hpp", "pict.hpp", nullptr};
const char* const kHDiag[] = {"diagnostics.hpp", nullptr};
const ServiceInfo kServices[service::Count] = {
    {s::Expression, "Expression", kHExpression}, {s::Exact, "Exact", kHExact}, {s::Editor, "Editor", kHEditor},
    {s::MathRender, "MathRender", kHRender}, {s::Format, "Format", kHFormat}, {s::Solvers, "Solvers", kHSolvers},
    {s::Stats, "Stats", kHStats}, {s::MathCore, "MathCore", kHMathCore}, {s::Widgets, "Widgets", kHWidgets},
    {s::Network, "Network", kHNetwork}, {s::Storage, "Storage", kHStorage}, {s::Units, "Units", kHUnits},
    {s::Help, "Help", kHHelp}, {s::RunCore, "RunCore", kHRunCore}, {s::Models, "Models", kHModels},
    {s::GraphCore, "GraphCore", kHGraphCore}, {s::Files, "Files", kHFiles}, {s::Diag, "Diag", kHDiag},
};
const char* const kDataNames[data::Count] = {"settings", "memory", "history", "functions", "view", "table", "lists", "equation", "chat", "graph"};
} // namespace

const AppInfo& appInfo(AppId id) noexcept {
    const auto i = static_cast<std::size_t>(id);
    return kApps[i < AppCount ? i : 0];
}
const AppInfo& appInfoAt(std::size_t index) noexcept { return kApps[index < AppCount ? index : 0]; }
const ServiceInfo& serviceInfo(std::size_t index) noexcept { return kServices[index < service::Count ? index : 0]; }
const char* dataSectionName(std::size_t index) noexcept { return kDataNames[index < data::Count ? index : 0]; }
const char* statusName(AppStatus s) noexcept {
    switch (s) {
    case AppStatus::Available: return "Available";
    case AppStatus::Planned: return "Planned";
    case AppStatus::External: return "External";
    }
    return "?";
}
} // namespace fx::app
