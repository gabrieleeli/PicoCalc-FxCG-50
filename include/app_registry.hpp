#pragma once
// Registry of the fx-CG50 style modes: one table that the launcher, the
// AppManager, the documentation (docs/APPLICATIONS_ARCHITECTURE.md) and the
// architecture test share. For every mode it declares its status, its
// responsibility, the shared services it is built on, the workspace sections
// it may access and the modes it may open directly. tests/architecture_test.cpp
// checks the sources against these declarations, so the separation between
// applications (graphs only in GRAPH, statistics only in STATISTICS, ...) is
// enforced rather than just documented.
#include <cstddef>
#include <cstdint>

namespace fx::app {

// Main-menu entries in fx-CG50 order (keys 1..9, A..K) plus project extensions.
enum class AppId : std::uint8_t {
    RunMatrix, Statistics, EActivity, Spreadsheet, Graph, DynaGraph, Table, Recursion,
    ConicGraphs, Equation, Program, Financial, ECon4, Link, Memory, System, Geometry,
    PicturePlot, Graph3D, Conversion, Python, AiChat, Symbols, Count, MainMenu = 0xFF
};
constexpr std::size_t AppCount = static_cast<std::size_t>(AppId::Count);

enum class AppStatus : std::uint8_t {
    Available,      // controller registered in the firmware (functions: docs/FEATURE_MATRIX.md)
    Planned,        // responsibilities, services and data model defined; controller not written yet
    External,       // depends on hardware or a proprietary protocol the PicoCalc does not have
};
const char* statusName(AppStatus status) noexcept;

// Shared services: each exists once and every mode uses the same instance of the engine.
namespace service {
constexpr std::uint32_t Expression = 1u << 0;    // expression.hpp  parser -> AST -> valutatori (numeri, liste, matrici, vettori)
constexpr std::uint32_t Exact      = 1u << 1;    // exact.hpp       fractions, surds, multiples of pi
constexpr std::uint32_t Editor     = 1u << 2;    // natural_editor.hpp, line_editor.hpp
constexpr std::uint32_t MathRender = 1u << 3;    // math_renderer.hpp  2D layout of expressions/results
constexpr std::uint32_t Format     = 1u << 4;    // number_format.hpp  Norm/Fix/Sci/Eng, complex forms
constexpr std::uint32_t Solvers    = 1u << 5;    // solvers.hpp     polynomials, inequalities, f(x)=g(x)
constexpr std::uint32_t Stats      = 1u << 6;    // statistics.hpp  1/2-var, regressions, distributions, tests
constexpr std::uint32_t MathCore   = 1u << 7;    // math_engine.hpp matrices, vectors, bases, DMS, angle unit
constexpr std::uint32_t Widgets    = 1u << 8;    // ui_kit.hpp, app_widgets.hpp (forms, grids, lists)
constexpr std::uint32_t Network    = 1u << 9;    // net.hpp, ai_protocol.hpp (Wi-Fi/TLS relay)
constexpr std::uint32_t Storage    = 1u << 10;   // workspace record in flash (storage.hpp via Firmware)
constexpr std::uint32_t Units      = 1u << 11;   // units.hpp (unica tabella delle unita'), unit_formats.hpp (conversioni di formato)
constexpr std::uint32_t Help       = 1u << 12;   // symbol guide (cheatsheet.hpp)
constexpr std::uint32_t RunCore    = 1u << 13;   // run_matrix.hpp  Run-Matrix calculation/history core
constexpr std::uint32_t Models     = 1u << 14;   // app_models.hpp  data models of the planned modes
constexpr std::uint32_t GraphCore  = 1u << 15;   // graph_model/graph_engine/graph_solve/table_model.hpp  plots, sampling, G-Solve, zoom, tables
constexpr std::uint32_t Files      = 1u << 16;   // file_store/graph_file/pict.hpp  microSD files, .pcgraph, .pcpict
constexpr std::uint32_t Diag       = 1u << 17;   // diagnostics.hpp  hardware self-tests (microSD, GRAPH timing, memory)
constexpr std::uint32_t Graph3D    = 1u << 18;   // graph3d.hpp     3D surfaces z=f(x,y) (meshes and z-buffer in PSRAM)
constexpr std::uint32_t Hardware   = 1u << 19;   // board_config/mem_pools/psram/limits/system_info.hpp  board and memory state
// graph_commands.hpp: comandi grafici di Run-Matrix (Plot, F-Line, Graph Y=...)
// eseguiti sui dati di GRAPH (V-Window, SKETCH) che GRAPH poi disegna (v1.11.0).
constexpr std::uint32_t GraphCmd   = 1u << 20;
// equation.hpp: motore di EQUATION (v1.12.0): coefficienti esatti, SIML, POLY, SOLVER.
constexpr std::uint32_t EquationCore = 1u << 21;
constexpr std::uint32_t Count      = 22;
} // namespace service

// Sections of fx::Workspace, the only channel through which modes exchange data.
namespace data {
constexpr std::uint32_t Settings  = 1u << 0;     // angle, display, complex, Frac Result, network
constexpr std::uint32_t Memory    = 1u << 1;     // A-Z, Ans, PreAns (with exact forms)
constexpr std::uint32_t History   = 1u << 2;     // Run-Matrix history
constexpr std::uint32_t Functions = 1u << 3;     // Y1..Y6 and their types
constexpr std::uint32_t View      = 1u << 4;     // V-Window
constexpr std::uint32_t Table     = 1u << 5;     // Table range
constexpr std::uint32_t Lists     = 1u << 6;     // List 1..26 e i loro sottonomi
constexpr std::uint32_t Equation  = 1u << 7;     // Equation coefficients and solver
constexpr std::uint32_t Chat      = 1u << 8;     // AI conversation
constexpr std::uint32_t Graph     = 1u << 9;     // GRAPH Setup, V-Window/graph memories, sketches, project
constexpr std::uint32_t Graph3D   = 1u << 10;    // 3D Graph functions, 3D V-Window and view
constexpr std::uint32_t Matrices  = 1u << 11;    // Mat A-Z, Vct A-Z, MatAns, VctAns, ListAns
constexpr std::uint32_t Strings   = 1u << 12;    // v1.12.0: Str 1-20, function memory f1-f20, variabili di Dyna/Recursion/TVM
constexpr std::uint32_t Count     = 13;
} // namespace data

constexpr std::uint32_t linkTo(AppId id) noexcept { return 1u << static_cast<unsigned>(id); }

// Sezione dei dati (bit di data::) a cui appartiene un membro o un metodo del
// workspace usato dalle applicazioni ("lists", "listNames", "setList"...);
// 0 per i nomi che non sono una sezione. Il test di architettura confronta
// ogni accesso `ws.<nome>` dei sorgenti con le sezioni dichiarate.
std::uint32_t dataSectionOfMember(const char* member) noexcept;

struct AppInfo {
    AppId id;
    const char* name;           // main menu caption
    const char* key;            // direct access key on the main menu
    std::uint16_t color;        // icon colour
    AppStatus status;
    const char* role;           // responsibility of the mode
    const char* missing;        // what is missing, non-null while the mode is not available
    std::uint32_t services;     // service:: bits (planned services for modes not available yet)
    std::uint32_t data;         // data:: sections the sources may access
    std::uint32_t links;        // linkTo() bits: modes this one may open directly
    const char* const* sources; // implementation files relative to the repository (nullptr-terminated)
};
const AppInfo& appInfo(AppId id) noexcept;
const AppInfo& appInfoAt(std::size_t index) noexcept;

struct ServiceInfo {
    std::uint32_t bit;
    const char* name;
    const char* const* headers;  // headers that provide the service (nullptr-terminated)
};
const ServiceInfo& serviceInfo(std::size_t index) noexcept;   // index < service::Count
const char* dataSectionName(std::size_t index) noexcept;       // index < data::Count, "ws.<name>"
} // namespace fx::app
