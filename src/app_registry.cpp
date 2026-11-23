// The registry of modes (see app_registry.hpp). Keep it in sync with the
// sources: tests/architecture_test.cpp fails when a mode uses a service,
// a workspace section or a link that is not declared here.
#include "app_registry.hpp"
#include "board_config.hpp"
#include "features.hpp"
#include "graphics_engine.hpp"
#include <string_view>

namespace fx::app {
namespace {
using gfx::rgb888To565;
namespace s = service;
namespace d = data;
constexpr std::uint32_t AllData = (1u << data::Count) - 1;
constexpr std::uint32_t Ui = s::Widgets | s::Editor | s::Format;
// v1.15.0: guscio comune e file tipizzati, previsti per ogni modalita' nuova.
constexpr std::uint32_t Shell = s::ModeShell | s::TypedFiles;
constexpr std::uint8_t Plus = boards::PicoPlus2W;

const char* const kRunMatrixSrc[] = {"src/apps/run_matrix_app.cpp", "src/apps/matrix_editor.cpp", "src/apps/answer_view.cpp", "src/run_matrix.cpp", nullptr};
const char* const kStatisticsSrc[] = {"src/apps/statistics_app.cpp", nullptr};
const char* const kGraphSrc[] = {"src/apps/graph_app.cpp", "src/apps/graph_plot.cpp", "src/apps/graph_files.cpp", "src/apps/graph_view.cpp", "src/apps/graph_dual.cpp", nullptr};
const char* const kTableSrc[] = {"src/apps/table_app.cpp", "src/apps/graph_view.cpp", nullptr};
const char* const kEquationSrc[] = {"src/apps/equation_app.cpp", "src/apps/math_field.cpp", "src/equation.cpp", nullptr};
const char* const kMemorySrc[] = {"src/apps/memory_app.cpp", nullptr};
const char* const kSystemSrc[] = {"src/apps/system_app.cpp", nullptr};
const char* const kConversionSrc[] = {"src/apps/conversion_app.cpp", nullptr};
const char* const kAiChatSrc[] = {"src/apps/ai_chat_app.cpp", nullptr};
const char* const kSymbolsSrc[] = {"src/apps/symbols_app.cpp", nullptr};
const char* const kGraph3DSrc[] = {"src/apps/graph3d_app.cpp", "src/apps/graph3d_plot.cpp", nullptr};
const char* const kEActivitySrc[] = {"src/apps/eactivity_app.cpp", "src/eactivity.cpp", nullptr};
// v1.17.0: modalita' Base della Pimoroni Pico Plus 2 W (controller + nucleo proprio).
#if FXAI_BOARD_PICO_PLUS_2W
#define FXAI_SRC(name) kSrc##name
const char* const kSrcSpreadsheet[] = {"src/apps/spreadsheet_app.cpp", "src/spreadsheet.cpp", nullptr};
const char* const kSrcDyna[] = {"src/apps/dyna_app.cpp", "src/dyna.cpp", nullptr};
const char* const kSrcRecursion[] = {"src/apps/recursion_app.cpp", "src/recursion.cpp", nullptr};
const char* const kSrcConic[] = {"src/apps/conic_app.cpp", "src/conic.cpp", nullptr};
const char* const kSrcProgram[] = {"src/apps/program_app.cpp", "src/program_mode.cpp", nullptr};
const char* const kSrcFinancial[] = {"src/apps/financial_app.cpp", "src/financial.cpp", nullptr};
const char* const kSrcLink[] = {"src/apps/link_app.cpp", "src/link_protocol.cpp", nullptr};
const char* const kSrcGeometry[] = {"src/apps/geometry_app.cpp", "src/geometry.cpp", nullptr};
const char* const kSrcPicturePlot[] = {"src/apps/picture_plot_app.cpp", "src/picture_plot.cpp", nullptr};
const char* const kSrcPython[] = {"src/apps/python_app.cpp", "src/python_port.cpp", nullptr};
constexpr AppStatus BaseStatus = AppStatus::Base;
#else
#define FXAI_SRC(name) nullptr
constexpr AppStatus BaseStatus = AppStatus::Planned;   // Pico 2 W: risolte comunque in Planned (fuori scheda)
#endif
constexpr std::uint32_t Core = s::ModeCore;

constexpr AppInfo kApps[AppCount] = {
    {AppId::RunMatrix, "Run-Matrix", "1", rgb888To565(232, 96, 30), AppStatus::Available,
     "Calcoli e risultati: input Math o Line, risultati esatti e decimali, matrici, vettori e liste con l'editor MAT/VCT e i CSV, schermate MatAns/ListAns, cronologia modificabile, variabili, Base-N intera, simboli ingegneristici, Simp, DMS, comandi grafici disegnati da Graph",
     nullptr, s::Expression | s::Exact | Ui | s::MathRender | s::MathCore | s::RunCore | s::Storage | s::Files | s::GraphCmd,
     // View, Table e Graph: scritti solo dai comandi grafici (graph_commands.hpp) e da
     // storeSystemVariable (->Xmax, ->TblStart, ->Xfct), mai direttamente; Lists solo con
     // i metodi del workspace (List "QTY", n->Dim List, Fill); Strings: Str, f1-f20, Dyna/Recursion/TVM.
     d::Settings | d::Memory | d::History | d::Matrices | d::View | d::Table | d::Graph | d::Lists | d::Strings, linkTo(AppId::Graph), kRunMatrixSrc},
    {AppId::Statistics, "Statistics", "2", rgb888To565(30, 160, 90), AppStatus::Available,
     "Liste 1-26 con sottonomi e file CSV, statistiche 1/2 variabili, regressioni, grafici statistici, distribuzioni, test e intervalli",
     nullptr, s::Expression | Ui | s::Stats | s::Storage | s::Files, d::Settings | d::Lists | d::Functions, 0, kStatisticsSrc},
    {AppId::EActivity, "eActivity", "3", rgb888To565(220, 40, 140), AppStatus::Available,
     "Documenti .pceact con righe di testo (espressioni Math, colori, marcatori, memo), righe di calcolo valutate dal nucleo di Run-Matrix con ricalcolo fino alla riga di stop, immagini PICT, strisce delle modalita' con i propri dati, Notes, memorie Mat/Vct/List per file",
     nullptr,
     s::Expression | s::Exact | Ui | s::MathRender | s::RunCore | s::Storage | s::Files | Shell | s::TextEdit | s::StepTask | s::EActivityCore,
     // Matrices e Lists: le memorie del file (10-9); le parti delle strisce passano solo da
     // encodeParts/decodeParts/resetPart del workspace.
     d::Settings | d::Memory | d::Matrices | d::Lists | d::EActivity,
     linkTo(AppId::RunMatrix) | linkTo(AppId::Graph) | linkTo(AppId::Table) | linkTo(AppId::Statistics) | linkTo(AppId::Equation) | linkTo(AppId::Graph3D),
     kEActivitySrc},
#if FXAI_BOARD_PICO_PLUS_2W
    {AppId::Spreadsheet, "Spreadsheet", "4", rgb888To565(70, 150, 40), BaseStatus,
     "Foglio A..Z x 999 con numeri, testo e formule (=A1+B2*2, $A$1, A1:A10 con Sum/Mean/Min/Max/Median), ricalcolo automatico e Circular ERROR, CUT/COPY/PASTE e FILL, colonne <-> List, file .pcsheet",
     "Base v1.17.0: mancano per la parita' (cap. 9) SEQ, SORT, CellIf/CellProd, formattazione condizionale, GRAPH e CALC dalle celle, CSV e memorie verso matrici.",
     s::Expression | Ui | s::Stats | s::Storage | Shell | s::TextEdit | s::StepTask | s::Binding | Core, d::Settings | d::Memory | d::Lists | d::Sheet, 0, FXAI_SRC(Spreadsheet), Plus},
#else   // Pico 2 W congelato: la voce della v1.16.0, identica
    {AppId::Spreadsheet, "Spreadsheet", "4", rgb888To565(70, 150, 40), AppStatus::Planned,
     "Foglio A..Z x 999 righe con valori e formule (=A1+B2), statistiche sulle colonne",
     "Foglio di calcolo con formule e riferimenti non ancora implementato.",
     s::Expression | Ui | s::Stats | s::Storage | s::Models | Shell | s::TextEdit | s::StepTask | s::Graph2D | s::Binding, d::Settings | d::Memory | d::Lists, 0, nullptr, Plus},
#endif
    {AppId::Graph, "Graph", "5", rgb888To565(60, 70, 200), AppStatus::Available,
     "Grafici Y=, r=, parametrici, X=, disequazioni (Y1..Y20); trace, zoom, V-Window, sketch, G-Solve, modify, memorie, PICT, progetti .pcgraph su microSD",
     nullptr, s::Expression | Ui | s::MathRender | s::Storage | s::GraphCore | s::Files,
     d::Settings | d::Memory | d::Functions | d::View | d::Table | d::Lists | d::Graph,
     linkTo(AppId::Table) | linkTo(AppId::RunMatrix), kGraphSrc},
#if FXAI_BOARD_PICO_PLUS_2W
    {AppId::DynaGraph, "Dyna Graph", "6", rgb888To565(40, 110, 220), BaseStatus,
     "Y= con un coefficiente variabile (Start/End/Pitch), animazione a passi con velocita' e AC, ciclo continuo o passata singola, sulla V-Window di Graph",
     "Base v1.17.0: mancano per la parita' (5-42) le funzioni BUILT-IN, il Locus, la Dynamic Graph Memory e la tabella da formula.",
     s::Expression | Ui | s::Storage | Shell | s::StepTask | s::Graph2D | s::Binding | Core, d::Settings | d::Memory | d::View | d::Strings | d::Dyna, 0, FXAI_SRC(Dyna), Plus},
#else   // Pico 2 W congelato: la voce della v1.16.0, identica
    {AppId::DynaGraph, "Dyna Graph", "6", rgb888To565(40, 110, 220), AppStatus::Planned,
     "Famiglie di grafici al variare di un coefficiente (Start/End/Pitch), sulla V-Window di Graph",
     "Grafici dinamici con parametro variabile non ancora implementati.",
     s::Expression | Ui | s::MathRender | s::Storage | s::Models | Shell | s::StepTask | s::Graph2D | s::Binding, d::Settings | d::Memory | d::View, 0, nullptr, Plus},
#endif
    {AppId::Table, "Table", "7", rgb888To565(120, 70, 200), AppStatus::Available,
     "Tabelle di Y=, r= e parametriche (Y1..Y20) da range o lista, derivate, EDIT/ROW, LISTMEM; GPH-CON/GPH-PLT verso Graph",
     nullptr, s::Expression | Ui | s::MathRender | s::Storage | s::GraphCore | s::ModeShell, d::Settings | d::Memory | d::Functions | d::View | d::Table | d::Lists | d::Graph,
     linkTo(AppId::Graph), kTableSrc},
#if FXAI_BOARD_PICO_PLUS_2W
    {AppId::Recursion, "Recursion", "8", rgb888To565(40, 60, 160), BaseStatus,
     "Successioni an+1 = f(n, an) e an+2 = f(n, an, an+1) con a0 e a1, tabella con Sum an, grafico dei termini e ragnatela",
     "Base v1.17.0: mancano per la parita' (5-45) bn e cn, il tipo an = f(n), il grafico di fase e la convergenza della ragnatela.",
     s::Expression | Ui | s::Storage | Shell | s::StepTask | s::Graph2D | s::Binding | Core, d::Settings | d::Memory | d::View | d::Strings | d::Recursion, 0, FXAI_SRC(Recursion), Plus},
#else   // Pico 2 W congelato: la voce della v1.16.0, identica
    {AppId::Recursion, "Recursion", "8", rgb888To565(40, 60, 160), AppStatus::Planned,
     "Successioni an, an+1, an+2 (fino a tre), tabelle, grafici e diagrammi di fase",
     "Successioni ricorsive an+1 non ancora implementate.",
     s::Expression | Ui | s::Storage | s::Models | Shell | s::StepTask | s::Graph2D | s::Binding, d::Settings | d::Memory | d::View, 0, nullptr, Plus},
#endif
#if FXAI_BOARD_PICO_PLUS_2W
    {AppId::ConicGraphs, "Conic Graphs", "9", rgb888To565(30, 140, 170), BaseStatus,
     "Parabole, circonferenze, ellissi e iperboli in forma cartesiana da un elenco di modelli, coefficienti, grafico, TRACE e G-Solve (vertice, fuoco, centro, raggio, asintoti, eccentricita')",
     "Base v1.17.0: mancano per la parita' (5-50) le forme polari e parametriche, direttrice, asse, latus rectum, intercette e ZOOM.",
     s::Expression | Ui | s::Solvers | s::Storage | Shell | s::Graph2D | Core, d::Settings | d::Memory | d::View | d::Conic, 0, FXAI_SRC(Conic), Plus},
#else   // Pico 2 W congelato: la voce della v1.16.0, identica
    {AppId::ConicGraphs, "Conic Graphs", "9", rgb888To565(30, 140, 170), AppStatus::Planned,
     "Parabole, circonferenze, ellissi e iperboli in forma cartesiana, polare e parametrica",
     "Grafici di coniche non ancora implementati.",
     s::Expression | Ui | s::Solvers | s::Storage | s::Models | Shell | s::Graph2D, d::Settings | d::View, 0, nullptr, Plus},
#endif
    {AppId::Equation, "Equation", "A", rgb888To565(240, 140, 20), AppStatus::Available,
     "SIML (2-6 incognite) e POLY (grado 2-6) con coefficienti esatti e complessi, soluzioni esatte, molteplicita' e Infinitely Many Solutions; SOLVER su qualsiasi variabile con Lower/Upper; disequazioni (estensione)",
     nullptr, s::Expression | s::Exact | Ui | s::MathRender | s::RunCore | s::Solvers | s::Storage | s::EquationCore,
     d::Settings | d::Memory | d::Equation, 0, kEquationSrc},
#if FXAI_BOARD_PICO_PLUS_2W
    {AppId::Program, "Program", "B", rgb888To565(170, 160, 20), BaseStatus,
     "Programmi .pcprog in C, C++ e C# (sottoinsiemi interpretati): elenco NEW/EDIT/DEL/RUN, editor con evidenziazione, output, input, comandi grafici disegnati da Graph, A-Z e liste, AC = Break, errori con riga e colonna",
     "Base v1.17.0: mancano per la parita' (cap. 8) DEBUG passo passo, SEARCH, RENAME, password e la libreria fx completa (statistiche, finanza).",
     s::Expression | Ui | s::Storage | Shell | s::TextEdit | s::StepTask | s::Binding | s::Interp | s::GraphCmd | Core,
     d::Settings | d::Memory | d::Lists | d::Program, linkTo(AppId::Graph), FXAI_SRC(Program), Plus},
#else   // Pico 2 W congelato: la voce della v1.16.0, identica
    {AppId::Program, "Program", "B", rgb888To565(170, 160, 20), AppStatus::Planned,
     "Programmi in C, C++ e C# (sottoinsiemi interpretati, decisione della v1.15.0) che usano il motore, i comandi grafici e il workspace",
     "Interprete dei programmi C, C++ e C# non ancora integrato in una modalita'.",
     s::Expression | s::Exact | s::RunCore | Ui | s::Storage | s::Models | Shell | s::TextEdit | s::StepTask | s::Graph2D | s::Binding | s::Interp | s::GraphCmd,
     d::Settings | d::Memory | d::Lists | d::Functions | d::View, 0, nullptr, Plus},
#endif
#if FXAI_BOARD_PICO_PLUS_2W
    {AppId::Financial, "Financial", "C", rgb888To565(230, 190, 20), BaseStatus,
     "Interesse semplice, TVM (n, I%, PV, PMT, FV, P/Y, C/Y) con BGN/END, Cash Flow (NPV, IRR, PBP, NFV), ammortamento, APR<->EFF, giorni; risultato in Ans",
     "Base v1.17.0: mancano per la parita' (cap. 7) Cost/Sell/Margin, Depreciation, Bond, i grafici dei flussi e Date Mode 360.",
     s::Expression | Ui | s::Storage | Shell | s::StepTask | Core, d::Settings | d::Memory | d::Strings | d::Financial, 0, FXAI_SRC(Financial), Plus},
#else   // Pico 2 W congelato: la voce della v1.16.0, identica
    {AppId::Financial, "Financial", "C", rgb888To565(230, 190, 20), AppStatus::Planned,
     "Interesse semplice e composto (TVM), flussi di cassa, ammortamento, conversioni di tasso",
     "Calcoli finanziari (TVM, interessi) non ancora implementati.",
     s::Expression | Ui | s::Solvers | s::Storage | s::Models | Shell | s::Graph2D | s::Binding, d::Settings | d::Memory, 0, nullptr, Plus},
#endif
    {AppId::ECon4, "E-CON4", "D", rgb888To565(130, 40, 180), AppStatus::External,
     "Acquisizione dati da sensori esterni (data logger)",
     "Richiede un data logger esterno e il suo protocollo: non supportato dalla PicoCalc.",
     s::Widgets | s::Storage, d::Settings | d::Lists, 0, nullptr},
#if FXAI_BOARD_PICO_PLUS_2W
    {AppId::Link, "Link", "E", rgb888To565(70, 140, 140), BaseStatus,
     "Invio e ricezione del workspace o di singoli file sulla USB seriale (pacchetti propri con CRC-32), avanzamento e AC; pacchetti .pclink; strumento tools/picocalc_link.py",
     "Base v1.17.0: il protocollo Link Casio tra calcolatrici non e' supportato; mancano la selezione di piu' file e il trasferimento di immagini dello schermo.",
     s::Widgets | s::Storage | s::Files | Shell | s::StepTask | Core, AllData, 0, FXAI_SRC(Link), Plus},
#else   // Pico 2 W congelato: la voce della v1.16.0, identica
    {AppId::Link, "Link", "E", rgb888To565(70, 140, 140), AppStatus::Planned,
     "Trasferimento del workspace (formato proprio con CRC) via USB seriale verso un PC",
     "Trasferimento dati via USB/seriale non ancora implementato; il protocollo Link Casio non e' supportato.",
     s::Widgets | s::Storage | Shell | s::StepTask, AllData, 0, nullptr, Plus},
#endif
    {AppId::Memory, "Memory", "F", rgb888To565(70, 110, 170), AppStatus::Available,
     "Gestione della memoria: uso, cancellazione selettiva, salvataggio del workspace",
     nullptr, s::Widgets | s::Storage | s::Files | s::TypedFiles, AllData, 0, kMemorySrc},
    {AppId::System, "System", "G", rgb888To565(60, 130, 150), AppStatus::Available,
     "Impostazioni (SET UP), retroilluminazione, Wi-Fi, relay AI, informazioni su memoria e PSRAM, diagnostica e reset",
     nullptr, Ui | s::Network | s::Storage | s::Diag | s::Hardware, AllData /* RESET clears the whole workspace */, 0, kSystemSrc},
#if FXAI_BOARD_PICO_PLUS_2W
    {AppId::Geometry, "Geometry", "H", rgb888To565(50, 110, 210), BaseStatus,
     "Punti, segmenti, rette, circonferenze e poligoni messi con il cursore; selezione, spostamento, cancellazione; lunghezze, angoli, aree e coordinate; zoom e pan; file .pcgeo",
     "Base v1.17.0: mancano per la parita' (cap. 14) vincoli e costruzioni (Construct), trasformazioni, animazioni, testo ed etichette.",
     s::Expression | Ui | s::Storage | Shell | s::Graph2D | Core, d::Settings | d::Memory | d::Geometry, 0, FXAI_SRC(Geometry), Plus},
#else   // Pico 2 W congelato: la voce della v1.16.0, identica
    {AppId::Geometry, "Geometry", "H", rgb888To565(50, 110, 210), AppStatus::Planned,
     "Costruzioni geometriche (punti, rette, circonferenze, poligoni) e misure",
     "Costruzioni geometriche interattive non ancora implementate.",
     s::Expression | Ui | s::MathCore | s::Storage | s::Models | Shell | s::StepTask | s::Graph2D, d::Settings | d::Memory, 0, nullptr, Plus},
#endif
#if FXAI_BOARD_PICO_PLUS_2W
    {AppId::PicturePlot, "Picture Plot", "I", rgb888To565(40, 170, 80), BaseStatus,
     "Sfondo .pcpict, punti messi con il cursore e numerati, coordinate in List 1 e List 2, regressione col motore di Statistics disegnata sopra lo sfondo; file .pcplot",
     "Base v1.17.0: mancano per la parita' (cap. 15) i fotogrammi, AXTRANS, Plot List, Y=f(x) e Fade I/O.",
     s::Expression | Ui | s::Stats | s::Storage | s::Files | Shell | s::Graph2D | Core, d::Settings | d::Memory | d::Lists | d::PicturePlot, 0, FXAI_SRC(PicturePlot), Plus},
#else   // Pico 2 W congelato: la voce della v1.16.0, identica
    {AppId::PicturePlot, "Picture Plot", "I", rgb888To565(40, 170, 80), AppStatus::Planned,
     "Punti su un'immagine di sfondo, liste di coordinate e regressioni",
     "Picture Plot non ancora implementato.",
     s::Expression | Ui | s::Stats | s::Storage | s::Models | Shell | s::Graph2D, d::Settings | d::Lists | d::View, 0, nullptr, Plus},
#endif
    {AppId::Graph3D, "3D Graph", "J", rgb888To565(70, 60, 190), AppStatus::Available,
     "Fino a 3 grafici: modelli Line, Plane, Sphere, Cylinder, Cone, Z=, Param, Rot X/Y; V-Window 3D, Setup, FORMAT, 3D G-Mem e V-Mem, rotazione, zoom e viste, TRACE, G-Solve (CROSS, INTSECT, RELATION), SKETCH, PICTURE; z-buffer in PSRAM",
     nullptr, s::Expression | Ui | s::MathRender | s::Storage | s::Files | s::Graph3D | s::StepTask, d::Settings | d::Memory | d::Graph3D, 0, kGraph3DSrc},
    {AppId::Conversion, "Conversion", "K", rgb888To565(220, 90, 120), AppStatus::Available,
     "Conversioni di unita' (10 grandezze, prefissi SI); il risultato va in Ans",
     nullptr, s::Expression | Ui | s::Units, d::Settings | d::Memory, 0, kConversionSrc},
#if FXAI_BOARD_PICO_PLUS_2W
    {AppId::Python, "Python", "L", rgb888To565(40, 120, 180), BaseStatus,
     "MicroPython: script .py con editor, RUN, SHELL (REPL) e output sul display; moduli math e casioplot minimo; heap separato in PSRAM; AC = KeyboardInterrupt",
     "Base v1.17.0: mancano per la parita' (cap. 17) il menu delle funzioni, il CATALOG, il debug degli script e i moduli random/turtle/matplotl.",
     s::Widgets | s::Editor | s::Storage | Shell | s::TextEdit | s::StepTask | s::Hardware | Core, d::Settings | d::Python, 0, FXAI_SRC(Python), Plus},
#else   // Pico 2 W congelato: la voce della v1.16.0, identica
    {AppId::Python, "Python", "L", rgb888To565(40, 120, 180), AppStatus::Planned,
     "Script MicroPython con heap separato e output sul display",
     "Runtime MicroPython non integrato in questa build.",
     s::Widgets | s::Storage | s::Models | Shell | s::TextEdit | s::StepTask | s::Graph2D, d::Settings, 0, nullptr, Plus},
#endif
    {AppId::AiChat, "AI Chat", "M", rgb888To565(0, 150, 200), AppStatus::Available,
     "Conversazione con Claude o ChatGPT tramite relay HTTPS (nessuna chiave sul dispositivo)",
     nullptr, s::Widgets | s::Editor | s::Network | s::Storage, d::Settings | d::Chat, linkTo(AppId::System), kAiChatSrc},
    {AppId::Symbols, "Symbols", "N", rgb888To565(130, 60, 170), AppStatus::Available,
     "Guida di simboli e funzioni", nullptr, s::Widgets | s::Help, 0, 0, kSymbolsSrc},
};

const char* const kHExpression[] = {"expression.hpp", "expression_ast.hpp", "value.hpp", "calc_library.hpp", nullptr};
const char* const kHExact[] = {"exact.hpp", nullptr};
const char* const kHEditor[] = {"natural_editor.hpp", "line_editor.hpp", nullptr};
const char* const kHRender[] = {"math_renderer.hpp", nullptr};
const char* const kHFormat[] = {"number_format.hpp", nullptr};
const char* const kHSolvers[] = {"solvers.hpp", nullptr};
const char* const kHStats[] = {"statistics.hpp", nullptr};
const char* const kHMathCore[] = {"math_engine.hpp", "limits.hpp", nullptr};
const char* const kHWidgets[] = {"ui_kit.hpp", "app_widgets.hpp", "graphics_engine.hpp", "font_renderer.hpp", nullptr};
const char* const kHNetwork[] = {"net.hpp", "ai_protocol.hpp", "ai_client.hpp", nullptr};
const char* const kHStorage[] = {"storage.hpp", nullptr};
const char* const kHUnits[] = {"point_a.hpp", nullptr};
const char* const kHHelp[] = {"cheatsheet.hpp", nullptr};
const char* const kHRunCore[] = {"run_matrix.hpp", nullptr};
const char* const kHModels[] = {"app_models.hpp", nullptr};
const char* const kHGraphCore[] = {"graph_model.hpp", "graph_engine.hpp", "graph_solve.hpp", "table_model.hpp", "graph_view.hpp", "limits.hpp", nullptr};
const char* const kHFiles[] = {"file_store.hpp", "graph_file.hpp", "pict.hpp", "csv_io.hpp", nullptr};
const char* const kHDiag[] = {"diagnostics.hpp", nullptr};
const char* const kHGraph3D[] = {"graph3d.hpp", "graph3d_model.hpp", "limits.hpp", nullptr};
const char* const kHGraphCmd[] = {"graph_commands.hpp", nullptr};
const char* const kHEquation[] = {"equation.hpp", nullptr};
const char* const kHTypedFiles[] = {"typed_files.hpp", nullptr};
const char* const kHModeShell[] = {"mode_shell.hpp", nullptr};
const char* const kHTextEdit[] = {"text_editor.hpp", nullptr};
const char* const kHStepTask[] = {"step_task.hpp", nullptr};
const char* const kHGraph2D[] = {"graph2d.hpp", nullptr};
const char* const kHBinding[] = {"binding.hpp", nullptr};
const char* const kHInterp[] = {"program.hpp", nullptr};
const char* const kHEActivity[] = {"eactivity.hpp", "limits.hpp", nullptr};
#if FXAI_BOARD_PICO_PLUS_2W
// v1.17.0: tutti i nuclei delle modalita' Base (l'architecture_test permette a
// ogni modalita' solo il proprio, modeCoreHeaders).
const char* const kHModeCore[] = {"spreadsheet.hpp", "dyna.hpp", "recursion.hpp", "conic.hpp", "program_mode.hpp", "financial.hpp",
                                  "link_protocol.hpp", "geometry.hpp", "picture_plot.hpp", "python_port.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
#endif
const char* const kHHardware[] = {"board_config.hpp", "mem_pools.hpp", "psram.hpp", "limits.hpp", "system_info.hpp", "system_runtime.hpp", nullptr};
const ServiceInfo kServices[service::Count] = {
    {s::Expression, "Expression", kHExpression}, {s::Exact, "Exact", kHExact}, {s::Editor, "Editor", kHEditor},
    {s::MathRender, "MathRender", kHRender}, {s::Format, "Format", kHFormat}, {s::Solvers, "Solvers", kHSolvers},
    {s::Stats, "Stats", kHStats}, {s::MathCore, "MathCore", kHMathCore}, {s::Widgets, "Widgets", kHWidgets},
    {s::Network, "Network", kHNetwork}, {s::Storage, "Storage", kHStorage}, {s::Units, "Units", kHUnits},
    {s::Help, "Help", kHHelp}, {s::RunCore, "RunCore", kHRunCore}, {s::Models, "Models", kHModels},
    {s::GraphCore, "GraphCore", kHGraphCore}, {s::Files, "Files", kHFiles}, {s::Diag, "Diag", kHDiag},
    {s::Graph3D, "Graph3D", kHGraph3D}, {s::Hardware, "Hardware", kHHardware}, {s::GraphCmd, "GraphCmd", kHGraphCmd},
    {s::EquationCore, "EquationCore", kHEquation},
    {s::TypedFiles, "TypedFiles", kHTypedFiles}, {s::ModeShell, "ModeShell", kHModeShell},
    {s::TextEdit, "TextEdit", kHTextEdit}, {s::StepTask, "StepTask", kHStepTask},
    {s::Graph2D, "Graph2D", kHGraph2D, Plus}, {s::Binding, "Binding", kHBinding, Plus},
    {s::Interp, "Interp", kHInterp, Plus},
    {s::EActivityCore, "EActivityCore", kHEActivity},
#if FXAI_BOARD_PICO_PLUS_2W
    {s::ModeCore, "ModeCore", kHModeCore, Plus},
#endif
};
const char* const kDataNames[data::Count] = {"settings", "memory", "history", "functions", "view", "table", "lists", "equation", "chat", "graph", "graph3d", "matrices", "strings", "eactivity",
#if FXAI_BOARD_PICO_PLUS_2W
                                             "sheet", "dyna", "recursion", "conic", "financial", "program", "link", "geometry", "pictplot", "python",
#endif
};
// Membri e metodi del workspace che non hanno il nome di una sezione (v1.11.0).
struct MemberSection { const char* member; std::uint32_t section; };
const MemberSection kMembers[] = {
    {"listNames", d::Lists}, {"listName", d::Lists}, {"setListName", d::Lists}, {"setList", d::Lists},
    {"checkListLength", d::Lists}, {"listFits", d::Lists}, {"listElements", d::Lists},
    // v1.12.0: List "QTY", List File 1-6, Str, function memory, variabili di sistema
    {"listByName", d::Lists}, {"selectListFile", d::Lists}, {"listFiles", d::Lists},
    {"setString", d::Strings}, {"functionMemories", d::Strings}, {"setFunctionMemory", d::Strings}, {"modeVars", d::Strings}, {"setRecursionFormula", d::Strings}, {"functionMemory", d::Strings},
    // Una variabile di sistema puo' essere della V-Window, di TABLE, di GRAPH (Xfct) o di Dyna/Recursion/TVM.
    {"storeSystemVariable", d::View | d::Table | d::Graph | d::Strings},
    {"statisticVariable", d::Settings | d::Lists}, {"pixelProbe", d::Graph},
};
} // namespace

namespace {
// ---- v1.15.0 (F1): risoluzione per scheda e per opzioni della build
constexpr bool compiledFlag(AppId id) noexcept {
    switch (id) {
    case AppId::EActivity: return features::kEActivity;
    case AppId::Spreadsheet: return features::kSpreadsheet;
    case AppId::DynaGraph: return features::kDynaGraph;
    case AppId::Recursion: return features::kRecursion;
    case AppId::ConicGraphs: return features::kConicGraphs;
    case AppId::Program: return features::kProgram;
    case AppId::Financial: return features::kFinancial;
    case AppId::Link: return features::kLink;
    case AppId::Geometry: return features::kGeometry;
    case AppId::PicturePlot: return features::kPicturePlot;
    case AppId::Python: return features::kPython;
    default: return true;                  // modalita' della v1.14.0 ed estensioni: sempre compilate
    }
}
constexpr std::uint8_t kThisBoard = board::config::kIsPicoPlus2W ? boards::PicoPlus2W : boards::Pico2W;
constexpr const char* kBoardMissing = "Non disponibile sul Raspberry Pi Pico 2 W: richiede la Pimoroni Pico Plus 2 W (flash e RAM).";
constexpr AppInfo resolve(AppInfo a) noexcept {
    if ((a.boards & kThisBoard) == 0) {
        if (a.status != AppStatus::External) a.status = AppStatus::Planned;
        a.missing = kBoardMissing;
        a.sources = nullptr;
        a.links = 0;
    } else if (!compiledFlag(a.id) && isOpen(a.status)) {
        a.status = AppStatus::Planned;
        a.missing = "Modalita' esclusa da questa build (opzione FXAI_MODE disattivata).";
        a.sources = nullptr;
        a.links = 0;
    }
    return a;
}
struct ResolvedApps {
    AppInfo items[AppCount];
    constexpr ResolvedApps() : items{} { for (std::size_t i = 0; i < AppCount; ++i) items[i] = resolve(kApps[i]); }
};
constexpr ResolvedApps kResolved{};
} // namespace

const char* const kNotOnThisBoard = kBoardMissing;
std::uint8_t thisBoard() noexcept { return kThisBoard; }
bool modeCompiled(AppId id) noexcept { return compiledFlag(id); }
const AppInfo& declaredAppInfo(std::size_t index) noexcept { return kApps[index < AppCount ? index : 0]; }
bool serviceCompiled(std::uint32_t bit) noexcept {
    switch (bit) {
    case service::TypedFiles: return features::kTypedFiles;
    case service::ModeShell: return features::kModeShell;
    case service::TextEdit: return features::kTextEdit;
    case service::StepTask: return features::kStepTask;
    case service::Graph2D: return features::kGraph2D;
    case service::Binding: return features::kBinding;
    case service::Interp: return features::kInterp;
    default: return true;
    }
}

std::uint32_t dataSectionOfMember(const char* member) noexcept {
    if (!member) return 0;
    const std::string_view m(member);
    for (std::size_t i = 0; i < data::Count; ++i) if (m == kDataNames[i]) return 1u << i;
    for (const auto& e : kMembers) if (m == e.member) return e.section;
    return 0;
}

const AppInfo& appInfo(AppId id) noexcept {
    const auto i = static_cast<std::size_t>(id);
    return kResolved.items[i < AppCount ? i : 0];
}
const AppInfo& appInfoAt(std::size_t index) noexcept { return kResolved.items[index < AppCount ? index : 0]; }
const ServiceInfo& serviceInfo(std::size_t index) noexcept { return kServices[index < service::Count ? index : 0]; }
const char* dataSectionName(std::size_t index) noexcept { return kDataNames[index < data::Count ? index : 0]; }
#if FXAI_BOARD_PICO_PLUS_2W
const char* const* modeCoreHeaders(AppId id) noexcept {
    static const char* const sheet[] = {"spreadsheet.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
    static const char* const dyna[] = {"dyna.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
    static const char* const rec[] = {"recursion.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
    static const char* const conic[] = {"conic.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
    static const char* const prog[] = {"program_mode.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
    static const char* const fin[] = {"financial.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
    static const char* const link[] = {"link_protocol.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
    static const char* const geo[] = {"geometry.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
    static const char* const pp[] = {"picture_plot.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
    static const char* const py[] = {"python_port.hpp", "mode_codec.hpp", "limits.hpp", nullptr};
    static const char* const none[] = {nullptr};
    switch (id) {
    case AppId::Spreadsheet: return sheet;
    case AppId::DynaGraph: return dyna;
    case AppId::Recursion: return rec;
    case AppId::ConicGraphs: return conic;
    case AppId::Program: return prog;
    case AppId::Financial: return fin;
    case AppId::Link: return link;
    case AppId::Geometry: return geo;
    case AppId::PicturePlot: return pp;
    case AppId::Python: return py;
    default: return none;
    }
}
#endif
const char* statusName(AppStatus s) noexcept {
    switch (s) {
    case AppStatus::Available: return "Available";
    case AppStatus::Planned: return "Planned";
    case AppStatus::External: return "External";
    case AppStatus::Base: return "Base";
    }
    return "?";
}
} // namespace fx::app
