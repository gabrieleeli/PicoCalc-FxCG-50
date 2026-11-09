#pragma once
#include "keyboard_input.hpp"
#include "natural_editor.hpp"
#include "point_a.hpp"
#include "graph_renderer.hpp"
#include "graph3d.hpp"
#include "diagnostics.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace fx {
enum class AppScreen { Home, RunMatrix, Graph, Cheatsheet, Unavailable, GraphFunctions, Graph3D, System, Memory, Diagnostics };

class RunMatrixController {
public:
    RunMatrixController();
    void handle(const InputEvent& event);
    AppScreen screen() const noexcept { return screen_; }
    const NaturalEditor& editor() const noexcept { return editor_; }
    const std::string& result() const noexcept { return result_; }
    const std::string& message() const noexcept { return message_; }
    const std::string& expression() const noexcept { return expression_; }
    const pointa::RunMatrixSession& session() const noexcept { return session_; }
    unsigned selection() const noexcept { return selected_; }
    const char* selectedAppName() const noexcept;
    unsigned cheatsheetPage() const noexcept { return cheatsheetPage_; }
    unsigned cheatsheetSelection() const noexcept { return cheatsheetSelection_; }
    const GraphView& graphView() const noexcept { return graphView_; }
    // First Y= function (kept for API compatibility with the single-graph UI).
    const std::string& graphExpression() const noexcept { return graphFunctions_.front(); }
    // All Y= functions plotted together (count limited by fx::limits).
    const std::vector<std::string>& graphFunctions() const noexcept { return graphFunctions_; }
    std::size_t graphSelected() const noexcept { return graphSelected_; }
    const std::string& graph3dExpression() const noexcept { return graph3dExpression_; }
    const g3d::View& graph3dView() const noexcept { return graph3dView_; }
    unsigned systemSelection() const noexcept { return systemSelection_; }
    unsigned memoryPage() const noexcept { return memoryPage_; }
    unsigned diagnosticSelection() const noexcept { return diagSelection_; }
    const diag::Result* lastDiagnostic() const noexcept { return haveDiag_ ? &lastDiag_ : nullptr; }
    bool diagnosticPending() const noexcept { return diagPending_; }
    void runPendingDiagnostic();
    // Error reporting used by the main loop when an operation fails.
    void reportOutOfMemory();
    void reportError(const char* text);
    // Pop-up message to show once over the current screen. Stored in a fixed
    // buffer: reporting an out-of-memory condition never allocates.
    bool hasToast() const noexcept { return toast_[0] != '\0'; }
    const char* toast() const noexcept { return toast_; }
    void clearToast() noexcept { toast_[0] = '\0'; }
    // Changes whenever the given screen would draw differently (window snapshots).
    std::uint32_t stamp(AppScreen screen) const noexcept;

    static constexpr unsigned kSystemEntries = 3;
    static constexpr unsigned kMemoryPages = 2;

private:
    void evaluate();
    void handleHome(const InputEvent& event);
    void handleGraph(const InputEvent& event);
    void handleGraphFunctions(const InputEvent& event);
    void handleGraph3D(const InputEvent& event);
    void handleSystem(const InputEvent& event);
    void openRunMatrix();
    // Sets message_ without allocating (capacity reserved in the constructor,
    // longer texts are truncated): safe inside std::bad_alloc handlers.
    void setMessage(const char* text) noexcept;
    void setMessage(const std::string& text) noexcept { setMessage(text.c_str()); }
    AppScreen screen_{AppScreen::Home};
    unsigned selected_{};
    unsigned cheatsheetPage_{};
    unsigned cheatsheetSelection_{};
    GraphView graphView_{};
    std::vector<std::string> graphFunctions_{std::string("x^2")};
    std::size_t graphSelected_{};
    std::string graph3dExpression_{"sin(x)*cos(y)"};
    g3d::View graph3dView_{};
    unsigned systemSelection_{};
    unsigned memoryPage_{};
    unsigned diagSelection_{};
    bool diagPending_{};
    bool haveDiag_{};
    diag::Result lastDiag_{};
    NaturalEditor editor_;
    pointa::RunMatrixSession session_;
    std::string expression_;
    std::string result_;
    std::string message_;
    char toast_[64]{};
};
}
