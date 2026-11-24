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
#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef FXAI_SOURCE_ROOT
#error "FXAI_SOURCE_ROOT must point to the repository root"
#endif
#ifndef FXAI_COMPILED_SOURCES
#error "FXAI_COMPILED_SOURCES must list the sources compiled for this board (tests/CMakeLists.txt)"
#endif

using namespace fx;
using namespace fx::app;

namespace {
const std::string kRoot = FXAI_SOURCE_ROOT;
const char* const kIdNames[AppCount] = {"RunMatrix", "Statistics", "EActivity", "Spreadsheet", "Graph", "DynaGraph", "Table",
    "Recursion", "ConicGraphs", "Equation", "Program", "Financial", "ECon4", "Link", "Memory", "System", "Geometry",
    "PicturePlot", "Graph3D", "Conversion", "Python", "AiChat", "Symbols",
#if FXAI_BOARD_PICO_PLUS_2W
    "Distribution",                                           // v1.18.0, solo Plus
#endif
};

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
        if ((info.services & s.bit) == 0) continue;
#if FXAI_BOARD_PICO_PLUS_2W
        // v1.17.0: ModeCore permette solo il nucleo della modalita' stessa.
        if (s.bit == service::ModeCore) { for (const char* const* p = modeCoreHeaders(info.id); *p; ++p) h.insert(*p); continue; }
#endif
        for (const char* const* p = s.headers; *p; ++p) h.insert(*p);
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
        // v1.17.0: Available e Base hanno un controller e i sorgenti; solo
        // Available (parita' completa) non ha la voce `missing`.
        const bool open = isOpen(a.status);
        CHECK(open == (a.sources != nullptr));
        CHECK((a.status == AppStatus::Available) == (a.missing == nullptr));
        if (!open) CHECK(a.links == 0);
        for (std::size_t j = 0; j < AppCount; ++j)
            if (a.links & linkTo(static_cast<AppId>(j))) CHECK(isOpen(appInfoAt(j).status));
        if (a.sources) for (const char* const* s = a.sources; *s; ++s) CHECK(std::filesystem::exists(kRoot + "/" + *s));
    }
    // Every service is backed by existing headers.
    for (std::size_t i = 0; i < service::Count; ++i)
        for (const char* const* h = serviceInfo(i).headers; *h; ++h) CHECK(std::filesystem::exists(kRoot + "/include/" + *h));

    // ---------------------------------------------------------------- v1.15.0 (F1): modalita' e servizi per scheda
    {
        std::set<std::string> compiled;
        {
            const std::string list = FXAI_COMPILED_SOURCES;
            std::size_t p = 0;
            while (p <= list.size()) {
                const std::size_t e = list.find(',', p);
                const std::string item = list.substr(p, e == std::string::npos ? std::string::npos : e - p);
                if (!item.empty()) compiled.insert(item);
                if (e == std::string::npos) break;
                p = e + 1;
            }
        }
        CHECK(compiled.count("src/app_registry.cpp") == 1 && compiled.size() > 60);
        const std::uint8_t board = thisBoard();
        CHECK(board == boards::PicoPlus2W || board == boards::Pico2W);
        for (std::size_t i = 0; i < AppCount; ++i) {
            const AppInfo& decl = declaredAppInfo(i);
            const AppInfo& a = appInfoAt(i);
            CHECK(decl.boards != 0 && (decl.boards & ~boards::Both) == 0);
            const bool onBoard = (decl.boards & board) != 0;
            if (!onBoard) {
                ::testutil::report(!isOpen(a.status) && a.sources == nullptr && a.links == 0 &&
                                   a.missing && std::string(a.missing) == kNotOnThisBoard,
                                   "mode not on this board is Planned with the board text", decl.name, 0, decl.name);
                ::testutil::report(!modeCompiled(decl.id), "FXAI_MODE off on a board the registry excludes", decl.name, 0, decl.name);
            }
            if (isOpen(a.status)) {
                ::testutil::report(onBoard && modeCompiled(a.id), "Available/Base only where compiled", a.name, 0, a.name);
                for (const char* const* src = a.sources; *src; ++src)
                    ::testutil::report(compiled.count(*src) == 1, "sources of an Available mode are compiled", *src, 0, a.name);
                for (std::size_t k = 0; k < service::Count; ++k) {
                    const ServiceInfo& sv = serviceInfo(k);
                    if (a.services & sv.bit)
                        ::testutil::report(serviceCompiled(sv.bit) && (sv.boards & board) != 0, "services of an Available mode exist on the board", a.name, 0, sv.name);
                }
            } else if (decl.sources) {
                // Una modalita' non disponibile qui non porta i propri controller nell'immagine.
                for (const char* const* src = decl.sources; *src; ++src)
                    if (std::string(*src).find("src/apps/") == 0)
                        ::testutil::report(compiled.count(*src) == 0, "controllers of an excluded mode are not compiled", *src, 0, decl.name);
            }
        }
        // Servizi: compilati esattamente sulle schede del registro, con i loro sorgenti.
        const std::map<std::uint32_t, std::vector<std::string>> serviceSources = {
            {service::TypedFiles, {"src/typed_files.cpp"}}, {service::ModeShell, {"src/apps/mode_shell.cpp"}},
            {service::TextEdit, {"src/text_editor.cpp"}}, {service::StepTask, {"src/step_task.cpp"}},
            {service::Graph2D, {"src/graph2d.cpp"}}, {service::Binding, {"src/binding.cpp"}},
            {service::Interp, {"src/program_lang.cpp", "src/program_vm.cpp"}}};
        for (std::size_t k = 0; k < service::Count; ++k) {
            const ServiceInfo& sv = serviceInfo(k);
            CHECK(sv.boards != 0 && (sv.boards & ~boards::Both) == 0);
            ::testutil::report(serviceCompiled(sv.bit) == ((sv.boards & board) != 0), "service compiled exactly on its boards", sv.name, 0, sv.name);
            const auto it = serviceSources.find(sv.bit);
            if (it == serviceSources.end()) continue;
            for (const std::string& src : it->second) {
                CHECK(std::filesystem::exists(kRoot + "/" + src));
                ::testutil::report((compiled.count(src) == 1) == serviceCompiled(sv.bit), "service sources compiled with the service", src.c_str(), 0, sv.name);
            }
        }
        // v1.16.0: le modalita' della v1.14.0 piu' eActivity su entrambe le schede.
        // Il Raspberry Pi Pico 2 W e' CONGELATO alla v1.16.0: questo controllo
        // fallisce se una versione successiva cambia le sue modalita' Available.
        // Sulla Pimoroni Pico Plus 2 W le milestone M3-M14 ne aggiungono altre.
        const std::set<AppId> v116 = {AppId::RunMatrix, AppId::Statistics, AppId::EActivity, AppId::Graph, AppId::Table, AppId::Equation,
                                      AppId::Memory, AppId::System, AppId::Graph3D, AppId::Conversion, AppId::AiChat, AppId::Symbols};
        std::set<AppId> available;
        // v1.17.0: l'insieme congelato e' quello delle modalita' APERTE (Available
        // o Base): sul Pico 2 W nessuna modalita' Base, nessuna Available nuova.
        for (std::size_t i = 0; i < AppCount; ++i) if (isOpen(appInfoAt(i).status)) available.insert(appInfoAt(i).id);
        if (thisBoard() == boards::Pico2W) {
            ::testutil::report(available == v116, "Pico 2 W frozen at v1.16.0 (open modes)", "tests/architecture_test.cpp", 0, "Pico2W");
            for (std::size_t i = 0; i < AppCount; ++i)
                ::testutil::report(appInfoAt(i).status != AppStatus::Base, "no Base mode on the frozen Pico 2 W", appInfoAt(i).name, 0, "Pico2W");
        } else {
            CHECK(std::includes(available.begin(), available.end(), v116.begin(), v116.end()));
            // Sulla Plus le modalita' della v1.16.0 restano Available (Statistics compresa:
            // la sua parita' completa e' la v1.18.0) e le modalita' nuove sono Base.
            for (AppId id : v116) CHECK(appInfo(id).status == AppStatus::Available);
            // v1.19.0: Geometry esce dalle Base (parita' completa con il capitolo 14).
            const std::set<AppId> v117Base = {AppId::Spreadsheet, AppId::DynaGraph, AppId::Recursion, AppId::ConicGraphs, AppId::Program,
                                              AppId::Financial, AppId::Link, AppId::PicturePlot, AppId::Python};
            for (AppId id : v117Base)
                ::testutil::report(appInfo(id).status == AppStatus::Base || !modeCompiled(id), "v1.17.0 modes are Base on the Plus", appInfo(id).name, 0, "Plus");
            ::testutil::report(appInfo(AppId::Geometry).status == AppStatus::Available || !modeCompiled(AppId::Geometry), "v1.19.0 Geometry e' Available", "Geometry", 0, "Plus");
        }
#if FXAI_BOARD_PICO_PLUS_2W
        // v1.18.0: Statistics alla parita' (Available, guscio comune e lavori a passi),
        // Distribution Base in coda al registro; un solo motore delle distribuzioni
        // (dist_core su calc_library) usato dal DIST di Statistics e da Distribution.
        {
            const AppInfo& st = appInfo(AppId::Statistics);
            CHECK(st.status == AppStatus::Available && (st.services & service::ModeShell) && (st.services & service::StepTask) && (st.data & data::StatData));
            CHECK(static_cast<std::size_t>(AppId::Distribution) == AppCount - 1 && std::string(appInfo(AppId::Distribution).key) == "O");
            ::testutil::report(appInfo(AppId::Distribution).status == AppStatus::Base || !modeCompiled(AppId::Distribution), "v1.18.0 Distribution e' Base", "Distribution", 0, "Plus");
            const std::string distCore = codeOnly(readFile("src/dist_core.cpp"));
            CHECK(distCore.find("calclib::normCD(") != std::string::npos && distCore.find("calclib::invHypergeoCD(") != std::string::npos);
            CHECK(codeOnly(readFile("src/apps/statistics_plus_app.cpp")).find("dist::compute(") != std::string::npos);
            CHECK(codeOnly(readFile("src/apps/distribution_app.cpp")).find("dist::compute(") != std::string::npos);
            // Le funzioni di distribuzione non sono riscritte nei controller.
            for (const char* src : {"src/apps/statistics_plus_app.cpp", "src/apps/distribution_app.cpp", "src/apps/stat_plot.cpp"})
                ::testutil::report(codeOnly(readFile(src)).find("calclib::") == std::string::npos, "DIST solo da dist_core", src, 0, "calclib");
            // statistics_app.cpp: sulla Plus il codice della v1.16.0 e' tutto nell'#else.
            const std::string old = readFile("src/apps/statistics_app.cpp");
            CHECK(old.find("#if FXAI_BOARD_PICO_PLUS_2W") != std::string::npos && old.find("#else") != std::string::npos);
        }
        // v1.19.0: Geometry alla parita' (Available, nessun `missing`), con i lavori a passi, le
        // immagini Pict e il collegamento verso Statistics (STORE > STAT della tabella); un nucleo
        // puro (geo::) separato dal controller: il nucleo non conosce l'interfaccia e il
        // controller non tocca le forme calcolate se non attraverso geometry.hpp.
        if (modeCompiled(AppId::Geometry)) {
            const AppInfo& g = appInfo(AppId::Geometry);
            CHECK(g.status == AppStatus::Available && g.missing == nullptr && (g.services & service::StepTask) && (g.services & service::ModeCore));
            CHECK((g.data & data::Geometry) && (g.data & data::Lists) && (g.links & linkTo(AppId::Statistics)));
            for (const char* src : {"src/geometry.cpp", "src/geometry_build.cpp", "src/geometry_measure.cpp", "src/geometry_anim.cpp", "src/geometry_file.cpp"}) {
                const std::string code = codeOnly(readFile(src));
                for (const char* f : {"apps.hpp", "gfx::", "app::", "Context& ", "ui::", "Workspace"})
                    ::testutil::report(code.find(f) == std::string::npos, "nucleo di Geometry senza interfaccia", src, 0, f);
            }
            for (const char* src : {"src/apps/geometry_app.cpp", "src/apps/geometry_view.cpp"})
                ::testutil::report(codeOnly(readFile(src)).find("geometry_internal.hpp") == std::string::npos, "il controller usa solo geometry.hpp", src, 0, "internal");
        }
#else
        // v1.18.0: sul Pico 2 W congelato nessuna modalita' nuova (23 modalita', come la v1.16.0).
        CHECK(AppCount == 23);
#endif
        // E-CON4 resta External su entrambe le schede.
        CHECK(appInfo(AppId::ECon4).status == AppStatus::External);
        // Il controllo sulla mappa del linker usa questi nomi (cmake/fxai_sources.cmake).
        const std::string cmakeFeatures = readFile("cmake/fxai_sources.cmake");
        for (const char* opt : {"MODE EACTIVITY", "MODE SPREADSHEET", "MODE PROGRAM", "MODE PYTHON", "SERVICE GRAPH2D",
                                "SERVICE BINDING", "SERVICE INTERP", "SERVICE TYPEDFILES", "SERVICE MODESHELL", "SERVICE TEXTEDIT", "SERVICE STEPTASK"})
            ::testutil::report(cmakeFeatures.find(std::string("fxai_feature(") + opt) != std::string::npos, "option declared in fxai_sources.cmake", "cmake/fxai_sources.cmake", 0, opt);
        CHECK(readFile("cmake/check_firmware_size.cmake").find("FXAI_EXCLUDED_OBJECTS") != std::string::npos);
    }

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
            CHECK(unavailable == !isOpen(appInfoAt(i).status));
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
    // v1.14.0, 3D Graph: one parser and evaluator (CompiledExpression for the
    // functions, evalNumber for the coefficients), the variables of the graph
    // bound with EvalContext (no write to A-Z), the geometry in the pure model,
    // controller and drawing separated from the model.
    {
        std::string code3d;
        for (const char* const* src = appInfo(AppId::Graph3D).sources; *src; ++src) {
            const std::string code = codeOnly(readFile(*src));
            code3d += code;
            for (const char* f : {"strtod", "atof", "stod(", "stof(", "sscanf", "istringstream", "parseExpression(", "memory.store(", "storeExact("})
                ::testutil::report(code.find(f) == std::string::npos, "3D Graph: no own number parsing, no write to A-Z", *src, 0, f);
        }
        for (const char* r : {"CompiledExpression(", "evalNumber(", "g3d::shapeOf(", "g3d::intersect(", "g3d::relation(", "g3d::crossSection(",
                              "g3d::render(", "g3d::validate(", "g3d::encode("})
            ::testutil::report(code3d.find(r) != std::string::npos, "3D Graph uses the shared engine and the 3D library", "src/apps/graph3d_*.cpp", 0, r);
        const std::string model = codeOnly(readFile("src/graph3d_model.cpp"));
        for (const auto& inc : includesOf(model)) {
            const bool allowed = inc == "graph3d_model.hpp" || inc.find('.') == std::string::npos;   // standard headers only
            ::testutil::report(allowed, "graph3d_model is pure (no parser, workspace or graphics)", "src/graph3d_model.cpp", 0, inc);
        }
        const std::string lib = codeOnly(readFile("src/graph3d.cpp"));
        for (const auto& inc : includesOf(lib))
            ::testutil::report(inc == "graph3d.hpp" || inc == "font_renderer.hpp" || inc.find('.') == std::string::npos,
                               "graph3d drawing library: no UI, app or workspace", "src/graph3d.cpp", 0, inc);
        CHECK(codeOnly(readFile("src/workspace.cpp")).find("g3d::decode(") != std::string::npos);   // record v10 validated by the model
    }
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
    // v1.17.0: le modalita' Base della Plus che dichiarano il grafico 2D condiviso
    // (Graph2D) e la sezione View disegnano sulla V-Window di Graph (Dyna, Recursion, Conic...).
    for (std::size_t i = 0; i < AppCount; ++i) {
        const AppInfo& a = appInfoAt(i);
        if (!a.sources || a.id == AppId::Graph || a.id == AppId::Statistics) continue;   // Statistics draws statistical plots
        if (a.status == AppStatus::Base && (a.services & service::Graph2D) && (a.data & data::View)) continue;
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
                                "sd_card.hpp", "fatfs_store.hpp", "value.hpp", "csv_io.hpp", "graph_commands.hpp", "calc_library.hpp", "equation.hpp", "limits.hpp",
                                "graph3d_model.hpp",
                                // v1.15.0: servizi puri delle fondamenta (F2, F3, F6, F8, F9)
                                "step_task.hpp", "typed_files.hpp", "features.hpp", "binding.hpp", "program.hpp"};
        const char* engineSrc[] = {"src/expression.cpp", "src/exact.cpp", "src/memory.cpp", "src/math_engine.cpp", "src/number_format.cpp",
                                   "src/solvers.cpp", "src/statistics.cpp", "src/run_matrix.cpp", "src/workspace.cpp", "src/storage.cpp", "src/app_models.cpp",
                                   "src/graph_model.cpp", "src/graph_engine.cpp", "src/graph_solve.cpp", "src/file_store.cpp", "src/graph_file.cpp", "src/pict.cpp",
                                   "src/sd_card.cpp", "src/fatfs_store.cpp", "src/value.cpp", "src/csv_io.cpp", "src/graph_commands.cpp",
                                   "src/calc_library.cpp", "src/equation.cpp", "src/graph3d_model.cpp",
                                   "src/step_task.cpp", "src/typed_files.cpp", "src/workspace_sections.cpp", "src/binding.cpp",
                                   "src/program_lang.cpp", "src/program_vm.cpp"};
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
            // v1.15.0 (F9): il linguaggio dei programmi (C, C++, C#) ha il suo analizzatore
            // solo in program_lang.cpp; le espressioni della calcolatrice passano dal parser
            // unico (fx::calc) e i grafici dall'interprete dei comandi di Run-Matrix.
            const std::string lang = codeOnly(readFile("src/program_lang.cpp"));
            const std::string vm = codeOnly(readFile("src/program_vm.cpp"));
            CHECK(lang.find("class SourceLexer") != std::string::npos && lang.find("class Compiler") != std::string::npos);
            CHECK(vm.find("parseExpression(") != std::string::npos && vm.find("graphcmd::execute(") != std::string::npos);
            CHECK(vm.find("bind::BindingResolver") != std::string::npos);
            for (const char* f : {"evaluateAny(", "evaluateExact(", "class Lexer", "class Parser"})
                ::testutil::report(lang.find(f) == std::string::npos && vm.find(f) == std::string::npos, "program core uses the calculator engine only through parseExpression", "src/program_*.cpp", 0, f);
            for (const auto& entry : std::filesystem::recursive_directory_iterator(kRoot + "/src")) {
                const std::string rel = entry.path().lexically_relative(kRoot).string();
                if (entry.path().extension() != ".cpp" || rel == "src/program_lang.cpp") continue;
                ::testutil::report(codeOnly(readFile(rel)).find("SourceLexer") == std::string::npos, "only program_lang parses program sources", rel.c_str(), 0, "SourceLexer");
            }
            // F8: celle e nomi legati sono opzioni dello stesso lexer.
            const std::string expr = codeOnly(readFile("src/expression.cpp"));
            CHECK(expr.find("cellReference(") != std::string::npos && expr.find("bound_") != std::string::npos);
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
        // v1.14.0: il modello del 3D Graph e' in graph3d_model.hpp (griglia 2..50 come la fx-CG50)
        g3d::Window g3;
        CHECK(g3d::validate(g3).empty());
        g3.xgrid = 50; CHECK(g3d::validate(g3).empty());
        g3.xgrid = 51; CHECK(!g3d::validate(g3).empty());
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
