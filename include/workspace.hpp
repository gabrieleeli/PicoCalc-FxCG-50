#pragma once
#include "expression.hpp"
#include "math_engine.hpp"
#include "memory.hpp"
#include "number_format.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fx {

struct Settings {
    AngleMode angle{AngleMode::Radians};      // fx-CG50 factory default
    DisplayFormat display{};
    ComplexMode complex{ComplexMode::Real};
    bool fractionResults{true};          // natural output: show exact forms (fractions, surds, pi) automatically
    bool mixedFractions{false};          // fx-CG50 "Frac Result": false = d/c (improper), true = ab/c (mixed)
    std::uint8_t lcdBacklight{160};      // STM32 REG_ID_BKL (0..255)
    std::uint8_t kbdBacklight{0};        // STM32 REG_ID_BK2 (0..255)
    std::string wifiSsid;
    std::string wifiPassword;
    std::string relayHost;               // HTTPS relay for AI chat (no provider keys on device)
    std::uint16_t relayPort{443};
    std::string relayPath{"/v1/chat"};
    std::string relayToken;              // per-device token issued by the relay
    std::uint8_t aiProvider{0};          // 0 = Claude (Anthropic), 1 = ChatGPT (OpenAI)
    std::string aiModel;                 // empty = relay default
    std::uint16_t aiMaxChars{1200};           // reply length requested from the relay (200..4000)
};

struct HistoryEntry {
    std::string expression;              // canonical linear text (editor round-trip)
    Complex value{};
    std::string error;                   // non-empty for failed evaluations ('=' prefix: Pol/Rec pair text)
    std::uint8_t view{0};                // ResultView (run_matrix.hpp)
    ExactSlot exact;                     // exact form of the value, when one exists
    bool numerical{false};               // produced by a numerical method (integral, d/dx, SOLVE)
    bool flipMixed{false};               // this result shows the other fraction format (ab/c <-> d/c)
};

enum class GraphType : std::uint8_t { Y = 0, Polar = 1, YGreater = 2, YLess = 3, YGreaterEq = 4, YLessEq = 5 };
const char* graphTypeLabel(GraphType t) noexcept;   // "Y=", "r=", "Y>", ...

struct GraphFunction {
    std::string expr;
    GraphType type{GraphType::Y};
    bool selected{true};
    std::uint8_t color{0};
};

struct ViewWindow {
    double xmin{-6.3}, xmax{6.3}, xscl{1}, ymin{-3.1}, ymax{3.1}, yscl{1};
    double tmin{0}, tmax{6.283185307179586}, tstep{0.05235987755982988};
    bool grid{true};
};

struct TableRange { double start{-3}, end{3}, step{1}; };

struct EquationState {
    std::uint8_t unknowns{2};
    std::array<double, 6 * 7> simultaneous{};    // row-major n x (n+1)
    std::uint8_t degree{2};
    std::array<double, 7> polynomial{};           // highest power first
    std::string solverExpr{"x^2-2=0"};
    double solverGuess{1};
    std::uint8_t ineqDegree{2};
    std::uint8_t ineqRelation{0};                 // 0 >, 1 <, 2 >=, 3 <=
    std::array<double, 7> inequality{};
};

struct ChatMessage { bool fromUser{}; std::string text; };

// Shared data of every application. It also resolves the variables that
// Run-Matrix reads from other modes through VARS (V-Window, Table range).
struct Workspace final : NameResolver {
    static constexpr std::size_t MaxHistory = 40;
    static constexpr std::size_t ListCount = 6;
    static constexpr std::size_t MaxListLength = 255;
    static constexpr std::size_t MaxChatMessages = 30;

    Settings settings;
    CalculatorMemory memory;
    std::vector<HistoryEntry> history;
    std::array<GraphFunction, 6> functions{};
    ViewWindow view;
    TableRange table;
    std::array<std::vector<double>, ListCount> lists{};
    EquationState equation;
    std::vector<ChatMessage> chat;

    Workspace();
    void resetAll();
    // Applies the global angle mode used by the evaluator.
    void apply() const;
    EvalContext evalContext() const;            // memory + complex mode + shared names
    void addHistory(HistoryEntry entry);
    bool resolve(const std::string& lowerName, double& out) const override;
};

// Binary, versioned serialisation used by the flash record store.
std::vector<std::uint8_t> serializeWorkspace(const Workspace& ws);
// Returns false (and leaves ws untouched) for corrupt or incompatible data.
bool deserializeWorkspace(const std::uint8_t* data, std::size_t size, Workspace& ws);
} // namespace fx
