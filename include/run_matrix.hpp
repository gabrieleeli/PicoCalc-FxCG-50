#pragma once
#include "keyboard_input.hpp"
#include "natural_editor.hpp"
#include "point_a.hpp"
#include "graph_renderer.hpp"
#include <string>

namespace fx {
enum class AppScreen { Home, RunMatrix, Graph, Cheatsheet, Unavailable };

class RunMatrixController {
public:
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
    const std::string& graphExpression() const noexcept { return graphExpression_; }
private:
    void evaluate();
    AppScreen screen_{AppScreen::Home};
    unsigned selected_{};
    unsigned cheatsheetPage_{};
    unsigned cheatsheetSelection_{};
    GraphView graphView_{};
    std::string graphExpression_{"x^2"};
    NaturalEditor editor_;
    pointa::RunMatrixSession session_;
    std::string expression_;
    std::string result_;
    std::string message_;
};
}
