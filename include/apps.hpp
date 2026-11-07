#pragma once
// Application classes of the PicoCalc FX firmware. Each mode owns its screen
// state, input handling and drawing; shared data lives in fx::Workspace.
#include "app.hpp"
#include "natural_editor.hpp"
#include "line_editor.hpp"
#include "app_widgets.hpp"
#include "statistics.hpp"
#include <array>
#include <string>
#include <vector>

namespace fx::app {

void drawAppIcon(gfx::Canvas& c, AppId id, int x, int y, int w, int h, bool available);

class LauncherApp final : public App {
public:
    AppId id() const override { return AppId::MainMenu; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    std::size_t selection() const noexcept { return selected_; }
    void select(std::size_t index) noexcept { selected_ = index < AppCount ? index : 0; }
    bool available(std::size_t index) const noexcept;
    void setAvailability(std::size_t index, bool on) noexcept { if (index < AppCount) available_[index] = on; }
private:
    std::size_t selected_{0};
    int firstRow_{0};
    std::array<bool, AppCount> available_{};
};

class UnavailableApp final : public App {
public:
    explicit UnavailableApp(AppManager& m) : manager_(m) {}
    AppId id() const override { return manager_.requestedUnavailable(); }
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
private:
    AppManager& manager_;
};

// Generic modal popup list used by OPTN / MATH / ZOOM / G-SOLVE menus.
struct PopupMenu {
    const char* title{};
    std::vector<std::string> items;
    int selected{0}, first{0};
    bool open{false};
    void show(const char* t, std::vector<std::string> list) { title = t; items = std::move(list); selected = 0; first = 0; open = true; }
    void close() { open = false; }
    // Returns the chosen index (>=0), -1 when still open, -2 when cancelled.
    int key(const KeyEvent& ev);
    void draw(gfx::Canvas& c) const;
};

class RunMatrixApp final : public App {
public:
    AppId id() const override { return AppId::RunMatrix; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    NaturalEditor& editor() noexcept { return editor_; }
    int browsing() const noexcept { return browse_; }
private:
    void evaluate(Context& ctx);
    void insertCatalogItem(Context& ctx, const std::string& item);
    NaturalEditor editor_;
    int browse_{-1};                // selected history entry, -1 = editing
    int scrollX_{0};
    PopupMenu menu_;
    int menuKind_{0};               // 1 MATH, 2 OPTN categories, 3 OPTN items, 4 VARS
    int optnCategory_{0};
};

class SymbolsApp final : public App {
public:
    AppId id() const override { return AppId::Symbols; }
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
private:
    unsigned page_{0}, selected_{0};
};

class EquationApp final : public App {
public:
    AppId id() const override { return AppId::Equation; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    enum class Screen { Menu, ChooseSize, Grid, Solver, Result };
    enum class Mode { Simultaneous, Polynomial, Solver, Inequality };
    Screen screen() const noexcept { return screen_; }
    const ResultList& results() const noexcept { return results_; }
private:
    void openGrid(Context& ctx);
    void solve(Context& ctx);
    Screen screen_{Screen::Menu};
    Mode mode_{Mode::Simultaneous};
    int menuSel_{0};
    CellGrid grid_;
    Form solverForm_;
    ResultList results_;
};

class StatisticsApp final : public App {
public:
    AppId id() const override { return AppId::Statistics; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    enum class Screen { Lists, Results, Graph, Form };
    enum class GraphKind { Scatter, XYLine, Histogram, Box, Regression };
    Screen screen() const noexcept { return screen_; }
    const ResultList& results() const noexcept { return results_; }
private:
    void setupGrid(Context& ctx);
    void runCalc(Context& ctx, int choice);
    void openDist(int choice);
    void openTest(int choice);
    void computeForm(Context& ctx);
    void drawGraph(Context& ctx, gfx::Canvas& c);
    Screen screen_{Screen::Lists};
    CellGrid grid_;
    PopupMenu menu_;
    int menuKind_{0};          // 1 GRAPH, 2 CALC, 3 DIST, 4 TEST
    ResultList results_;
    Form form_;
    int formKind_{0};          // 100+ dist, 200+ test
    GraphKind graph_{GraphKind::Scatter};
    stats::Regression reg_{};
    bool hasReg_{false};
};

class GraphApp final : public App {
public:
    AppId id() const override { return AppId::Graph; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    enum class Screen { List, Edit, Plot, Window, Integral };
    Screen screen() const noexcept { return screen_; }
    // G-Solve results (x, y) for tests.
    const std::vector<std::pair<double, double>>& solutions() const noexcept { return solutions_; }
    void showPlot() noexcept { screen_ = Screen::Plot; }
    static constexpr int PlotLeft = 0, PlotRight = 319, PlotTop = 18, PlotBottom = 299;
private:
    struct Cache {
        bool valid{false};
        ViewWindow view{};
        std::array<std::string, 6> expr{};
        std::array<GraphType, 6> type{};
        std::array<bool, 6> on{};
        std::array<std::vector<float>, 6> y{};         // per pixel column (NaN = undefined)
        std::array<std::vector<std::pair<float, float>>, 6> polar{};
    };
    void refresh(Context& ctx);
    void drawPlot(Context& ctx, gfx::Canvas& c);
    int currentFunction(Context& ctx) const;
    void gsolve(Context& ctx, int kind);
    void zoom(Context& ctx, int kind);
    double pixelToX(const ViewWindow& v, int px) const;
    Screen screen_{Screen::List};
    int row_{0};
    NaturalEditor editor_;
    Form window_;
    Form integral_;
    PopupMenu menu_;
    int menuKind_{0};                 // 1 ZOOM, 2 G-SOLVE, 3 SKETCH
    bool trace_{false};
    int traceX_{160};
    int traceFn_{0};
    int sketch_{0};                   // 1 tangent, 2 normal
    std::vector<std::pair<double, double>> solutions_;
    int solutionIndex_{0};
    std::string solveLabel_;
    bool shadeIntegral_{false};
    double intA_{0}, intB_{0}, intValue_{0};
    Cache cache_;
};

class TableApp final : public App {
public:
    AppId id() const override { return AppId::Table; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
private:
    void rebuild(Context& ctx);
    bool setup_{false};
    Form form_;
    std::vector<double> xs_;
    std::vector<int> fns_;
    int row_{0}, firstRow_{0}, col_{0};
};

class SystemApp final : public App {
public:
    AppId id() const override { return AppId::System; }
    void enter(Context& ctx) override;
    void leave(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
private:
    void apply(Context& ctx);
    Form form_;
    bool info_{false};
    bool confirmReset_{false};
};

class MemoryApp final : public App {
public:
    AppId id() const override { return AppId::Memory; }
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
private:
    int selected_{0};
    int confirm_{-1};
};

class ConversionApp final : public App {
public:
    AppId id() const override { return AppId::Conversion; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
private:
    void rebuildUnits();
    Form form_;
    std::string result_;
};

} // namespace fx::app
