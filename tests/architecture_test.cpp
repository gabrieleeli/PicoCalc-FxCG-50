// Architecture test: the application registry (app_registry.hpp) is checked
// against the real sources and the real firmware:
//  - every Available mode has a registered controller, every other mode opens
//    the explicit "not available" screen (no fake applications);
//  - each mode's sources include only the headers of the shared services it
//    declares, access only the workspace sections it declares and open only
//    the modes it declares (data is exchanged through the workspace);
//  - Run-Matrix shows calculations and results only: no graph, statistics,
//    table or conversion code;
//  - engine headers never depend on the application/UI layer, and the
//    header graph has no cycles;
//  - the data models of the planned modes validate as specified.
#include "app_models.hpp"
#include "app_registry.hpp"
#include "apps.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "firmware.hpp"
#include "test_util.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef FXAI_SOURCE_ROOT
#error "FXAI_SOURCE_ROOT must point to the repository root"
#endif

using namespace fx;
using namespace fx::app;

namespace {
const std::string kRoot = FXAI_SOURCE_ROOT;
const char* const kIdNames[AppCount] = {"RunMatrix", "Statistics", "EActivity", "Spreadsheet", "Graph", "DynaGraph", "Table",
    "Recursion", "ConicGraphs", "Equation", "Program", "Financial", "ECon4", "Link", "Memory", "System", "Geometry",
    "PicturePlot", "Graph3D", "Conversion", "Python", "AiChat", "Symbols"};

std::string readFile(const std::string& relative) {
    std::ifstream in(kRoot + "/" + relative, std::ios::binary);
    std::stringstream ss; ss << in.rdbuf();
    return ss.str();
}
// Removes // and /* */ comments and string literals so that words in comments do not count.
std::string codeOnly(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s.compare(i, 2, "//") == 0) { while (i < s.size() && s[i] != '\n') ++i; out += '\n'; continue; }
        if (s.compare(i, 2, "/*") == 0) { const auto e = s.find("*/", i + 2); i = e == std::string::npos ? s.size() : e + 1; continue; }
        if (s[i] == '"' && (i == 0 || s[i - 1] != '\'')) {
            // keep #include "x" intact, drop other string contents
            const auto lineStart = s.rfind('\n', i);
            const bool include = s.compare(lineStart == std::string::npos ? 0 : lineStart + 1, 8, "#include") == 0;
            std::size_t j = i + 1;
            while (j < s.size() && s[j] != '"') { if (s[j] == '\\') ++j; ++j; }
            out += include ? s.substr(i, j - i + 1) : "\"\"";
            i = j; continue;
        }
        out += s[i];
    }
    return out;
}
std::vector<std::string> includesOf(const std::string& code) {
    std::vector<std::string> out;
    std::size_t p = 0;
    while ((p = code.find("#include \"", p)) != std::string::npos) {
        const auto b = p + 10, e = code.find('"', b);
        out.push_back(code.substr(b, e - b));
        p = e;
    }
    return out;
}
bool identChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
std::set<std::string> workspaceSections(const std::string& code) {
    std::set<std::string> out;
    std::size_t p = 0;
    while ((p = code.find("ws.", p)) != std::string::npos) {
        if (p > 0 && identChar(code[p - 1])) { p += 3; continue; }
        std::size_t e = p + 3;
        while (e < code.size() && identChar(code[e])) ++e;
        out.insert(code.substr(p + 3, e - p - 3));
        p = e;
    }
    return out;
}
std::set<std::string> openedApps(const std::string& code) {
    std::set<std::string> out;
    std::size_t p = 0;
    while ((p = code.find("open(AppId::", p)) != std::string::npos) {
        std::size_t e = p + 12;
        while (e < code.size() && identChar(code[e])) ++e;
        out.insert(code.substr(p + 12, e - p - 12));
        p = e;
    }
    return out;
}
std::set<std::string> allowedHeaders(const AppInfo& info) {
    std::set<std::string> h = {"apps.hpp", "app.hpp", "app_widgets.hpp"};
    for (std::size_t i = 0; i < service::Count; ++i) {
        const ServiceInfo& s = serviceInfo(i);
        if (info.services & s.bit) for (const char* const* p = s.headers; *p; ++p) h.insert(*p);
    }
    return h;
}
std::uint16_t press(std::uint8_t code, std::uint8_t state = 1) { return static_cast<std::uint16_t>((state << 8) | code); }
} // namespace

int main() {
    // ---------------------------------------------------------------- registry consistency
    std::set<std::string> keys, names;
    for (std::size_t i = 0; i < AppCount; ++i) {
        const AppInfo& a = appInfoAt(i);
        CHECK(static_cast<std::size_t>(a.id) == i);
        CHECK(keys.insert(a.key).second && names.insert(a.name).second);
        CHECK(a.role && std::strlen(a.role) > 10);
        CHECK(a.services != 0 && (a.data >> data::Count) == 0);
        const bool available = a.status == AppStatus::Available;
        CHECK(available == (a.sources != nullptr));
        CHECK(available == (a.missing == nullptr));
        if (!available) CHECK(a.links == 0);
        for (std::size_t j = 0; j < AppCount; ++j)
            if (a.links & linkTo(static_cast<AppId>(j))) CHECK(appInfoAt(j).status == AppStatus::Available);
        if (a.sources) for (const char* const* s = a.sources; *s; ++s) CHECK(std::filesystem::exists(kRoot + "/" + *s));
    }
    // Every service is backed by existing headers.
    for (std::size_t i = 0; i < service::Count; ++i)
        for (const char* const* h = serviceInfo(i).headers; *h; ++h) CHECK(std::filesystem::exists(kRoot + "/include/" + *h));

    // ---------------------------------------------------------------- firmware registration
    board::init();
    {
        BoardPlatform platform;
        Firmware fw{platform, board::storageFlash()};
        fw.boot();
        for (std::size_t i = 0; i < AppCount; ++i) {
            const auto id = static_cast<AppId>(i);
            fw.apps().open(id);
            App* active = fw.apps().active();
            const bool unavailable = dynamic_cast<UnavailableApp*>(active) != nullptr;
            CHECK(active != nullptr && fw.apps().activeId() == id);
            CHECK(unavailable == (appInfoAt(i).status != AppStatus::Available));
            if (!unavailable) CHECK(active->id() == id);
            fw.draw(board::graphics());                                    // every screen draws without errors
            CHECK(!fw.apps().errorShown());
            fw.apps().home();
        }
        // The main menu reaches every mode with its direct-access key.
        for (std::size_t i = 0; i < AppCount; ++i) {
            fw.rawKey(press(static_cast<std::uint8_t>(std::tolower(static_cast<unsigned char>(appInfoAt(i).key[0])))));
            CHECK(fw.apps().activeId() == static_cast<AppId>(i));
            fw.apps().home();
        }
    }

    // ---------------------------------------------------------------- sources against the registry
    // Ogni sezione ha il suo nome; i membri e i metodi delle liste (v1.11.0)
    // appartengono alla sezione Lists.
    for (std::size_t i = 0; i < data::Count; ++i) CHECK(dataSectionOfMember(dataSectionName(i)) == (1u << i));
    CHECK(dataSectionOfMember("listNames") == data::Lists && dataSectionOfMember("setList") == data::Lists);
    CHECK(dataSectionOfMember("nonexistent") == 0);
    for (std::size_t i = 0; i < AppCount; ++i) {
        const AppInfo& a = appInfoAt(i);
        if (!a.sources) continue;
        const auto allowed = allowedHeaders(a);
        for (const char* const* src = a.sources; *src; ++src) {
            const std::string code = codeOnly(readFile(*src));
            CHECK(!code.empty());
            for (const auto& inc : includesOf(code))
                ::testutil::report(allowed.count(inc) == 1, "include allowed by declared services", *src, 0, std::string(a.name) + " -> " + inc);
            for (const auto& sec : workspaceSections(code)) {
                if (sec == "resolve" || sec == "apply" || sec == "evalContext" || sec == "addHistory") continue;   // workspace methods
                // Mat/Vct/List: letti tramite l'interfaccia CollectionResolver e assegnati
                // con metodi controllati (le liste restano di Statistics: nessun accesso
                // diretto a ws.lists)
                if (sec == "collection" || sec == "storeMatrix" || sec == "storeVector" || sec == "storeList" ||
                    sec == "storeCollectionAnswer" || sec == "snapshotCollections" || sec == "restoreCollections") continue;
                const std::uint32_t section = dataSectionOfMember(sec.c_str());
                bool declared = section != 0 && (a.data & section) == section;
                if (sec == "resetAll") declared = a.data == (1u << data::Count) - 1;                       // touches every section
                ::testutil::report(declared, "workspace section declared", *src, 0, std::string(a.name) + " -> ws." + sec);
            }
            for (const auto& target : openedApps(code)) {
                bool ok = false;
                for (std::size_t j = 0; j < AppCount; ++j) if (target == kIdNames[j]) ok = (a.links & linkTo(static_cast<AppId>(j))) != 0;
                ::testutil::report(ok, "link declared", *src, 0, std::string(a.name) + " -> " + target);
            }
        }
    }
    // Run-Matrix shows calculations and results only. I comandi grafici (v1.11.0)
    // passano dall'interprete graph_commands (graphcmd::) e li disegna GRAPH:
    // Run-Matrix non tocca direttamente i dati di GRAPH e non disegna.
    {
        const char* forbidden[] = {"GraphApp", "TableApp", "StatisticsApp", "ConversionApp", "EquationApp", "stats::", "sample2D",
                                   "sample3DSurface", "GraphFunction", "ViewWindow", "TableRange", "ws.view", "ws.table", "ws.lists",
                                   "ws.functions", "ws.equation", "ws.graph", "SketchItem", "CommandGraph", "graph::", "pointa::",
                                   "drawPlot", "canvas.line", "c.line(", "c.circle(", "mathSegment",
                                   // v1.12.0: List File, Str, function memory, variabili di Dyna/Recursion/TVM e
                                   // PxlTest( solo tramite i metodi del workspace e i servizi del valutatore
                                   "ws.listFiles", "ws.listNames", "ws.modeVars", "ws.functionMemories", "pixelProbe",
                                   "selectListFile", "ws.settings.listFile", "probePixel", "class Lexer", "class Parser",
                                   "statistics.hpp", "solvers.hpp", "table_model.hpp"};
        bool usesInterpreter = false, opensGraph = false;
        for (const char* const* src = appInfo(AppId::RunMatrix).sources; *src; ++src) {
            const std::string code = codeOnly(readFile(*src));
            for (const char* f : forbidden) ::testutil::report(code.find(f) == std::string::npos, "Run-Matrix separation", *src, 0, f);
            usesInterpreter = usesInterpreter || code.find("graphcmd::execute") != std::string::npos;
            opensGraph = opensGraph || code.find("open(AppId::Graph)") != std::string::npos;
        }
        CHECK(usesInterpreter && opensGraph);
        // L'interprete dei comandi e' un modulo del motore (nessuna dipendenza dall'interfaccia)
        // e GRAPH disegna i suoi risultati.
        const std::string graphCode = codeOnly(readFile("src/apps/graph_plot.cpp")) + codeOnly(readFile("src/apps/graph_app.cpp"));
        CHECK(graphCode.find("commandGraphs") != std::string::npos && graphCode.find("commandScreen") != std::string::npos);
        CHECK(codeOnly(readFile("src/graph_engine.cpp")).find("commands") != std::string::npos);
    }
    // EQUATION (v1.12.0): un solo parser e un solo valutatore. I coefficienti e le
    // equazioni passano dal motore di Run-Matrix (parseExpression, evaluateAny,
    // evaluateExact) attraverso equation.hpp; nessuna lettura propria dei numeri,
    // nessun percorso numerico separato (la griglia CellGrid e i vecchi solutori in double).
    {
        const char* noParsing[] = {"strtod", "atof", "stod(", "stof(", "sscanf", "istringstream", "evalNumber("};
        const char* legacy[] = {"CellGrid", "solveLinearSystem", "polynomialRoots", "solveEquation(", "Form "};
        const char* required[] = {"eq::evaluateNumber", "eq::solveSimultaneous", "eq::solvePolynomial", "eq::solveFor",
                                  "eq::numberTree", "eq::solverVariables", "MathField"};
        std::string appCode;
        for (const char* const* src = appInfo(AppId::Equation).sources; *src; ++src) {
            const std::string code = codeOnly(readFile(*src));
            for (const char* f : noParsing) ::testutil::report(code.find(f) == std::string::npos, "Equation: no separate number parsing", *src, 0, f);
            if (std::string(*src).find("/apps/") != std::string::npos) {
                appCode += code;
                for (const char* f : legacy) ::testutil::report(code.find(f) == std::string::npos, "Equation: no legacy numeric path", *src, 0, f);
            }
        }
        for (const char* r : required) ::testutil::report(appCode.find(r) != std::string::npos, "Equation uses the equation engine", "src/apps/equation_app.cpp", 0, r);
        const std::string engine = codeOnly(readFile("src/equation.cpp"));
        for (const char* r : {"parseExpression(", "evaluateAny(", "evaluateExact(", "evaluateReal("})
            ::testutil::report(engine.find(r) != std::string::npos, "equation engine evaluates with the shared evaluator", "src/equation.cpp", 0, r);
        // Run-Matrix legge le matrici di EQUATION solo come CollectionResolver (VARS › EQUATION),
        // senza il motore di EQUATION ne' i dati di ws.equation.
        std::string runCode;
        for (const char* const* src = appInfo(AppId::RunMatrix).sources; *src; ++src) runCode += codeOnly(readFile(*src));
        CHECK(runCode.find("equationMatrixName") != std::string::npos && runCode.find("Memory::Equation") != std::string::npos);
        CHECK(runCode.find("eq::") == std::string::npos && runCode.find("equation.hpp") == std::string::npos);
        const std::string wsCode = codeOnly(readFile("src/workspace.cpp"));
        CHECK(wsCode.find("Memory::Equation") != std::string::npos && wsCode.find("eq::toMatrix") != std::string::npos);
        // v1.13.0: VARS › EQUATION ha un solo percorso (le matrici di EQUATION via
        // CollectionResolver); nessun secondo calcolo dei risultati nel workspace o nel valutatore.
        for (const char* src : {"src/expression.cpp", "src/workspace.cpp", "src/calc_library.cpp", "src/apps/run_matrix_app.cpp"}) {
            const std::string code = codeOnly(readFile(src));
            ::testutil::report(code.find("eq_sim") == std::string::npos && code.find("eq_ply") == std::string::npos,
                               "VARS EQUATION has a single path (Memory::Equation)", src, 0, "eq_sim/eq_ply");
        }
        CHECK(wsCode.find("solveLinearSystem") == std::string::npos && wsCode.find("polynomialRoots") == std::string::npos);
        // I nomi SimRes... sono token dello stesso lexer.
        CHECK(codeOnly(readFile("src/expression.cpp")).find("equationMatrixName") != std::string::npos);
        // Nessun'altra modalita' disponibile usa il motore di EQUATION.
        for (std::size_t i = 0; i < AppCount; ++i) {
            const AppInfo& a = appInfoAt(i);
            if (!a.sources || a.id == AppId::Equation) continue;
            ::testutil::report((a.services & service::EquationCore) == 0, "only Equation declares EquationCore", a.name, 0, a.name);
        }
    }
    // Graphs of functions live in Graph: no other available mode draws function curves.
    for (std::size_t i = 0; i < AppCount; ++i) {
        const AppInfo& a = appInfoAt(i);
        if (!a.sources || a.id == AppId::Graph || a.id == AppId::Statistics) continue;   // Statistics draws statistical plots
        for (const char* const* src = a.sources; *src; ++src)
            ::testutil::report(codeOnly(readFile(*src)).find("ws.view.") == std::string::npos || a.id == AppId::Table,
                               "only Graph/Table use the V-Window", *src, 0, a.name);
    }

    // ---------------------------------------------------------------- layering and include cycles
    {
        const std::set<std::string> appLayer = {"app.hpp", "apps.hpp", "app_widgets.hpp", "firmware.hpp", "ui_kit.hpp",
                                                "board_port.hpp", "board_host.hpp", "board_platform.hpp", "net.hpp", "math_renderer.hpp",
                                                "graphics_engine.hpp", "font_renderer.hpp"};
        const char* engine[] = {"expression.hpp", "exact.hpp", "expression_ast.hpp", "memory.hpp", "math_engine.hpp", "number_format.hpp",
                                "solvers.hpp", "statistics.hpp", "run_matrix.hpp", "workspace.hpp", "storage.hpp", "app_models.hpp",
                                "graph_model.hpp", "graph_engine.hpp", "graph_solve.hpp", "file_store.hpp", "graph_file.hpp", "pict.hpp",
                                "sd_card.hpp", "fatfs_store.hpp", "value.hpp", "csv_io.hpp", "graph_commands.hpp", "calc_library.hpp", "equation.hpp", "limits.hpp"};
        const char* engineSrc[] = {"src/expression.cpp", "src/exact.cpp", "src/memory.cpp", "src/math_engine.cpp", "src/number_format.cpp",
                                   "src/solvers.cpp", "src/statistics.cpp", "src/run_matrix.cpp", "src/workspace.cpp", "src/storage.cpp", "src/app_models.cpp",
                                   "src/graph_model.cpp", "src/graph_engine.cpp", "src/graph_solve.cpp", "src/file_store.cpp", "src/graph_file.cpp", "src/pict.cpp",
                                   "src/sd_card.cpp", "src/fatfs_store.cpp", "src/value.cpp", "src/csv_io.cpp", "src/graph_commands.cpp",
                                   "src/calc_library.cpp", "src/equation.cpp"};
        for (const char* h : engine)
            for (const auto& inc : includesOf(codeOnly(readFile(std::string("include/") + h))))
                ::testutil::report(appLayer.count(inc) == 0, "engine header independent of the UI", h, 0, inc);
        for (const char* s : engineSrc)
            for (const auto& inc : includesOf(codeOnly(readFile(s))))
                ::testutil::report(appLayer.count(inc) == 0, "engine source independent of the UI", s, 0, inc);
        // v1.12.0: un solo parser, un solo AST, un solo valutatore. Nessun altro
        // sorgente definisce un lexer o un parser di espressioni, e la libreria
        // delle funzioni (calc_library) e' puro calcolo: niente workspace ne' parser.
        for (const auto& entry : std::filesystem::recursive_directory_iterator(kRoot + "/src")) {
            if (entry.path().extension() != ".cpp") continue;
            const std::string rel = entry.path().lexically_relative(kRoot).string();
            if (rel == "src/expression.cpp") continue;
            const std::string code = codeOnly(readFile(rel));
            for (const char* f : {"class Lexer", "class Parser", "struct Lexer", "struct Parser", "evalNode<", "CollEval<"})
                ::testutil::report(code.find(f) == std::string::npos, "single parser and evaluator", rel.c_str(), 0, f);
        }
        {
            const std::string lib = codeOnly(readFile("src/calc_library.cpp")) + codeOnly(readFile("include/calc_library.hpp"));
            for (const char* f : {"workspace.hpp", "parseExpression", "evaluateReal", "evaluateAny", "Workspace"})
                ::testutil::report(lib.find(f) == std::string::npos, "calc_library is pure computation", "src/calc_library.cpp", 0, f);
            // I comandi grafici valutano gli argomenti con il parser unico.
            const std::string cmd = codeOnly(readFile("src/graph_commands.cpp"));
            CHECK(cmd.find("parseExpression(") != std::string::npos && cmd.find("evaluateReal(") != std::string::npos);
            // PxlTest(: solo l'AppManager collega la schermata di GRAPH al valutatore.
            CHECK(codeOnly(readFile("src/app_manager.cpp")).find("pixelProbe") != std::string::npos);
            CHECK(codeOnly(readFile("src/apps/graph_plot.cpp")).find("probePixel") != std::string::npos);
        }
        // Include graph of include/*.hpp is acyclic.
        std::map<std::string, std::vector<std::string>> graph;
        for (const auto& entry : std::filesystem::directory_iterator(kRoot + "/include")) {
            const std::string name = entry.path().filename().string();
            if (entry.path().extension() == ".hpp") graph[name] = includesOf(codeOnly(readFile("include/" + name)));
        }
        CHECK(graph.size() > 20);
        std::map<std::string, int> state;   // 0 new, 1 on stack, 2 done
        std::string cycle;
        std::function<bool(const std::string&)> dfs = [&](const std::string& n) {
            state[n] = 1;
            for (const auto& m : graph[n]) {
                if (!graph.count(m)) continue;
                if (state[m] == 1) { cycle = n + " -> " + m; return false; }
                if (state[m] == 0 && !dfs(m)) return false;
            }
            state[n] = 2;
            return true;
        };
        bool acyclic = true;
        for (const auto& g : graph) if (state[g.first] == 0 && !dfs(g.first)) { acyclic = false; break; }
        ::testutil::report(acyclic, "no include cycles", "include/", 0, cycle);
    }

    // ---------------------------------------------------------------- data models of the planned modes
    {
        using namespace fx::models;
        RecursionModel rec;
        CHECK(!validate(rec).empty());                                 // no formula yet
        rec.formula[0] = "2an+1";
        CHECK(validate(rec).empty());
        rec.end = 1000; CHECK(!validate(rec).empty());
        rec.end = 6; rec.start = 0; CHECK(!validate(rec).empty());     // a1 needs n >= 1
        rec.fromOne = false; CHECK(validate(rec).empty());
        DynaGraphModel dyna;
        CHECK(validate(dyna).empty() && frameCount(dyna) == 5);        // A = 1..5 step 1
        dyna.pitch = 0.01; CHECK(!validate(dyna).empty());              // 401 graphs > 100
        dyna.pitch = 0; CHECK(frameCount(dyna) == 0 && !validate(dyna).empty());
        CHECK(std::string(dynaBuiltinFunction(DynaBuiltin::Vertex)) == "A(x-B)^2+C");
        CHECK(classify({1, 0, 1, 0, 0, -1}) == ConicKind::Circle);      // x²+y²=1
        CHECK(classify({1, 0, 4, 0, 0, -4}) == ConicKind::Ellipse);
        CHECK(classify({1, 0, -1, 0, 0, -1}) == ConicKind::Hyperbola);
        CHECK(classify({0, 0, 2, -1, 1, -1}) == ConicKind::Parabola);    // x = 2y²+y-1 (manual 5-51)
        CHECK(classify({1, 0, 1, 0, 0, 1}) == ConicKind::None);         // x²+y²=-1
        CHECK(classify({1, 0, -1, 0, 0, 0}) == ConicKind::Degenerate);  // two lines
        Graph3DModel g3; g3.z[0] = "x^2-y^2";
        CHECK(validate(g3).empty());
        g3.grid = 128; CHECK(validate(g3).empty());
        g3.grid = 129; CHECK(!validate(g3).empty());
        FinancialModel fin;
        CHECK(validate(fin).empty());
        fin.py = 0; CHECK(!validate(fin).empty());
        CellRef ref;
        CHECK(parseCellRef("B12", ref) && ref.column == 1 && ref.row == 12 && cellName(ref) == "B12");
        CHECK(parseCellRef("z999", ref) && ref.column == 25);
        CHECK(!parseCellRef("A0", ref) && !parseCellRef("A1000", ref) && !parseCellRef("1A", ref) && !parseCellRef("A01", ref));
        SpreadsheetModel sheet;
        CHECK(sheet.set({0, 1}, "=B1*2") && sheet.set({1, 1}, "21") && sheet.get({0, 1}) && *sheet.get({0, 1}) == "=B1*2");
        CHECK(sheet.set({0, 1}, "") && sheet.get({0, 1}) == nullptr && sheet.cells.size() == 1);
        CHECK(!sheet.set({26, 1}, "1"));
        CHECK(validateProgramName("FIBO 2").empty() && !validateProgramName("fibo").empty() && !validateProgramName("TOOLONGNAME").empty());
        CHECK(validateScriptName("hello.py").empty() && !validateScriptName("hello.txt").empty() && !validateScriptName(".py").empty());
        GeometryObject seg{GeometryObject::Kind::Segment, {0, 0, 1, 1}};
        CHECK(validate(seg).empty());
        GeometryObject bad{GeometryObject::Kind::Circle, {0, 0, -1}};
        CHECK(!validate(bad).empty());
    }
    return testutil::finish("architecture_test");
}
