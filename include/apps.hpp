#pragma once
// Application classes of the PicoCalc FX firmware. Each mode owns its screen
// state, input handling and drawing; shared data lives in fx::Workspace.
#include "app.hpp"
#include "natural_editor.hpp"
#include "line_editor.hpp"
#include "app_widgets.hpp"
#include "file_store.hpp"
#include "graph_engine.hpp"
#include "pict.hpp"
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
    void leave(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    NaturalEditor& editor() noexcept { return editor_; }
    int browsing() const noexcept { return browse_; }      // selected history entry, -1 = input line
    int editing() const noexcept { return editIndex_; }    // history entry edited in place, -1 = none
    enum class Menu : std::uint8_t { None, Math, OptnCategories, OptnItems, VarsCategories, VarsItems };
private:
    void evaluate(Context& ctx);
    void insertCatalogItem(Context& ctx, const std::string& item);
    void beginEdit(Context& ctx, int index);
    void endEdit(bool restoreInput);
    bool browseKey(Context& ctx, const KeyEvent& ev);    // false: the key leaves browsing and is handled normally
    void menuChoice(Context& ctx, int choice);
    HistoryEntry* target(Context& ctx);                  // entry the result keys act on
    void resultKey(Context& ctx, int kind);              // 0 S<->D, 1 ab/c<->d/c, 2 period, 3..7 views
    NaturalEditor editor_;
    ast::NodePtr pendingInput_;     // input line kept aside while a history entry is edited
    int browse_{-1};
    int editIndex_{-1};
    int scrollX_{0};
    PopupMenu menu_;
    Menu menuKind_{Menu::None};
    int optnCategory_{0};
    int varsCategory_{0};
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

// GRAPH (fx-CG50 chapter 5): relation list Y1..Y20 with every graph type,
// V-Window, Setup, Trace, Zoom, Sketch, G-Solve, Modify, graph and V-Window
// memories, pictures and .pcgraph projects on the microSD card. The numerical
// work is done by graph_engine.hpp / graph_solve.hpp; the curves are sampled
// incrementally in tick() so the keyboard is never blocked.
class GraphApp final : public App {
public:
    AppId id() const override { return AppId::Graph; }
    void enter(Context& ctx) override;
    void leave(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    bool tick(Context& ctx) override;
    bool ownsSetup() const override { return true; }

    enum class Screen : std::uint8_t { List, Edit, Plot, Window, Setup, Format, Files };
    // Plot screen states: plain graph, Trace (cursor on a curve), free pointer,
    // choice of a graph, G-Solve results, integral limits, Modify.
    enum class Mode : std::uint8_t { View, Trace, Pointer, Select, Results, Integral, Modify };
    Screen screen() const noexcept { return screen_; }
    Mode mode() const noexcept { return mode_; }
    void showPlot() noexcept { screen_ = Screen::Plot; mode_ = Mode::View; }
    // G-Solve results (x, y) of the last analysis.
    const std::vector<std::pair<double, double>>& solutions() const noexcept { return solutionXY_; }
    double integralValue() const noexcept { return integral_.value; }
    double areaValue() const noexcept { return integral_.area; }
    bool rendering() const noexcept { return !renderDone_; }
    void finishRendering(Context& ctx);                  // completes the sampling now (tests, PICT capture)
    const graph::GraphEngine& engine() const noexcept { return engine_; }
    const std::vector<graph::Curve>& curves() const noexcept { return curves_; }
    bool cursor(double& x, double& y) const noexcept;    // trace/pointer position in graph coordinates
    const std::string& infoText() const noexcept { return bottomText_; }   // coordinates line shown last
    static constexpr int PlotLeft = graphgeo::Left, PlotRight = graphgeo::Right, PlotTop = graphgeo::Top, PlotBottom = graphgeo::Bottom;

    // What the pointer, the trace cursor, the graph choice or a pop-up is for.
    enum class Action : std::uint8_t {
        None, ZoomBox, ZoomIn, ZoomOut, Pan,
        Plot, PlotOn, PlotOff, PlotChg, Line, FLine, Circle, Vertical, Horizontal, Text, Pen,
        Tangent, Normal, Inverse,
        Root, Max, Min, YIcept, Isect, YCal, XCal, Integral, IntRoot, IntIsect, IntMixed,
        Format, ModifyStart,
    };
    enum class Menu : std::uint8_t {
        None, Type, Convert, Tool, Style, GMem, BuiltIn, File, ListOptn, EditMore, EditVars, EditList,
        Zoom, Sketch, SketchPlot, SketchLine, SketchItems, SketchItem, Solve, SolveIntegral, PlotOptn,
        Picture, PictureStore, PictureRecall, BgvWin, VMem, Square, Color, StyleSketch,
    };
    // Number / text pop-up purposes.
    enum class Ask : std::uint8_t {
        None, TraceValue, XValue, YValue, IntLower, IntUpper, ModifyValue, ModifyStep, GMemStore, GMemRecall,
        VMemStore, VMemRecall, PictStore, PictRecall, PictSaveAs, BmpSaveAs, ProjectSaveAs, ProjectRename, SketchText,
        RefY, RefR, RefXt, RefYt, RefX, Factor, FactorY, Fade, BgvSaveAs,
    };
    enum class Browse : std::uint8_t { Projects, Pictures, Background };
    enum class Confirm : std::uint8_t { None, DeleteFunction, DeleteFile, Overwrite, NewProject, OpenProject, BgvLoad, BgvSave, ClearSketch };

private:
    struct Line { int fn; int sub; };                    // relation list line (sub 1 = Yt of a parametric pair)
    struct Mark { double x, y; bool on; };               // dot placed with EXE during Trace/G-Solve
    struct Solution { double x, y; std::string extra; };
    struct IntegralState {
        int stage{0};                                    // 0 lower limit, 1 upper limit, 2 result
        int f{-1}, g{-1};                                // plots (g >= 0: between two graphs)
        double lo{0}, hi{0}, value{0}, area{0};
        bool hasArea{false};
        std::vector<double> stops;                       // roots / intersections the pointer jumps between
        int stop{0};
    };
    struct ModifyState { int plot{-1}; int function{-1}; std::string vars; int selected{0}; double step{1}; };
    struct InputBox { Ask ask{Ask::None}; std::string prompt; LineEditor editor{40}; bool text{false}; };

    // graph_app.cpp: screens other than the plot
    std::vector<Line> lines(Context& ctx) const;
    GraphType lineType(Context& ctx, int fn) const;
    void listKey(Context& ctx, const KeyEvent& ev);
    void editKey(Context& ctx, const KeyEvent& ev);
    void storeEdit(Context& ctx);
    void beginEdit(Context& ctx, bool keep, char first = 0);
    void windowKey(Context& ctx, const KeyEvent& ev);
    void openWindow(Context& ctx);
    void loadWindowForm(const ViewWindow& v);
    bool applyWindow(Context& ctx);
    void setupKey(Context& ctx, const KeyEvent& ev);
    void openSetup(Context& ctx);
    void applySetup(Context& ctx);
    void formatKey(Context& ctx, const KeyEvent& ev);
    void openFormat(Context& ctx, int function, int sketchItem);
    void menuChoice(Context& ctx, Menu menu, int choice);
    void showMenu(Menu m, const char* title, std::vector<std::string> items);
    void ask(Ask a, const std::string& prompt, const std::string& initial = {}, bool text = false);
    void answer(Context& ctx, Ask a, const std::string& text);
    void confirm(Confirm c, const std::string& message);
    void confirmed(Context& ctx, Confirm c);
    void changed(Context& ctx);                          // functions/view/settings changed: redraw everything
    void drawList(Context& ctx, gfx::Canvas& c);
    void drawWindow(Context& ctx, gfx::Canvas& c);
    void drawPopups(Context& ctx, gfx::Canvas& c);
    void startModify(Context& ctx, int plot);
    void modifyKey(Context& ctx, const KeyEvent& ev);
    void copyModified(Context& ctx, int fn);

    // graph_plot.cpp: rendering and the plot screen
    void ensureBuilt(Context& ctx);
    bool renderStep(Context& ctx, int budget);
    void drawPlot(Context& ctx, gfx::Canvas& c);
    void drawLayers(Context& ctx, gfx::Canvas& c);       // background ... sketches (what PICT saves)
    void drawOverlays(Context& ctx, gfx::Canvas& c);     // cursors, texts, results
    void plotKey(Context& ctx, const KeyEvent& ev);
    void traceKey(Context& ctx, const KeyEvent& ev);
    void pointerKey(Context& ctx, const KeyEvent& ev);
    void selectKey(Context& ctx, const KeyEvent& ev);
    void resultsKey(Context& ctx, const KeyEvent& ev);
    void integralKey(Context& ctx, const KeyEvent& ev);
    void startTrace(Context& ctx, Action purpose);
    void startPointer(Action purpose);
    void startSelect(Context& ctx, Action purpose);
    void selected(Context& ctx, int plot);
    void solve(Context& ctx, Action what, int f, int g, double value);
    void beginIntegral(Context& ctx, Action what, int f, int g);
    void finishIntegral(Context& ctx);
    void zoomTo(Context& ctx, const ViewWindow& v);
    void zoomAction(Context& ctx, int choice);
    void plotMenu(Context& ctx, Menu menu, int choice);
    bool plotAnswer(Context& ctx, Ask a, const std::string& text);
    void bgvSave(Context& ctx, const std::string& path);
    void sketchAction(Context& ctx, Action a);
    void pointerDone(Context& ctx);
    void addSketch(Context& ctx, SketchItem item);
    void pasteGraph(Context& ctx);
    void traceMove(Context& ctx, int steps);
    void traceSet(Context& ctx, double v);
    bool tracePoint(double& x, double& y) const;
    std::vector<int> drawablePlots(bool analysable) const;
    bool tableDraw(Context& ctx) const;
    void buildTableCurves(Context& ctx);

    // graph_files.cpp: projects, memories, pictures
    files::FileSystem* fileSystem(Context& ctx, bool required);
    void newProject(Context& ctx);
    void saveProject(Context& ctx, const std::string& name);
    void openProject(Context& ctx, const std::string& name);
    void openFiles(Context& ctx, Browse purpose);
    void filesKey(Context& ctx, const KeyEvent& ev);
    void drawFiles(Context& ctx, gfx::Canvas& c);
    void storeGraphMemory(Context& ctx, int n);
    void recallGraphMemory(Context& ctx, int n);
    void storePicture(Context& ctx, const std::string& path);
    void recallPicture(Context& ctx, const std::string& path, bool background);
    void loadBackground(Context& ctx);
    void writeCapture(Context& ctx);
    void deleteFile(Context& ctx);

    Screen screen_{Screen::List};
    Screen returnScreen_{Screen::List};
    Mode mode_{Mode::View};
    Action action_{Action::None};
    int row_{0}, firstRow_{0};
    NaturalEditor editor_;
    int editFn_{0}, editSub_{0};
    bool editNew_{false};
    Form window_, setup_, format_;
    ViewWindow windowStart_{};
    int formatFn_{-1}, formatSketch_{-1};
    PopupMenu menu_;
    Menu menuKind_{Menu::None};
    InputBox input_;
    Confirm confirm_{Confirm::None};
    std::string confirmText_;
    int sketchSelected_{-1};
    int copyTarget_{-1};                                 // Modify COPY: choosing the destination line

    // rendering
    graph::GraphEngine engine_;
    std::vector<graph::Curve> curves_;
    std::vector<std::string> pasted_;
    bool built_{false}, renderDone_{false};
    std::size_t renderPlot_{0};
    ViewWindow builtView_{};
    std::vector<ViewWindow> zoomStack_;                  // ZOOM PREVIOUS
    ViewWindow original_{};                              // ZOOM ORIGINAL
    bool haveOriginal_{false};
    pict::Image overlay_, background_;
    std::string overlayPath_, backgroundPath_;
    bool capturePending_{false};
    std::string capturePath_;
    pict::Image capture_;
    bool captureReady_{false}, captureBmp_{false};

    // plot screen state
    int cursorPlot_{0};
    double cursorV_{0};
    bool traceLines_{true};
    int px_{160}, py_{159};                              // free pointer (screen pixels)
    bool havePoint1_{false};
    double p1x_{0}, p1y_{0};
    std::vector<float> pen_;
    bool penDown_{false};
    std::vector<int> selectable_;
    int selectIndex_{0}, firstChoice_{-1};
    std::vector<Solution> solutions_;
    std::vector<std::pair<double, double>> solutionXY_;
    int solutionIndex_{0};
    std::string solveLabel_;
    int solvePlot_{-1};
    IntegralState integral_;
    bool shadeIntegral_{false};
    std::vector<Mark> marks_;
    ModifyState modify_;
    std::string topText_, bottomText_;
    std::uint32_t blinkStart_{0};

    // files
    struct FileList { std::vector<files::Entry> entries; int selected{0}, first{0}; Browse purpose{Browse::Projects}; std::string folder; std::string status; } files_;
    std::string pendingName_;
    std::vector<std::string> windowText_;                // V-Window texts when the form was filled
    std::uint32_t lastBlink_{0};
    std::vector<std::int16_t> shade_;                    // integral shading: column, y0, y1 triples
    double factorX_{2};
    bool panGrab_{false};
    Screen formatReturn_{Screen::List};
    std::vector<graph::Plot> sketchPlots_;               // SKETCH Inverse graphs (x = f(y))
    std::vector<graph::Curve> sketchCurves_;
    std::uint32_t lastMove_{0};
    int speed_{1};                                       // pointer acceleration
    std::uint64_t fingerprint_{0};                       // data the plots were built from
    std::uint64_t sourceFingerprint(Context& ctx) const;
};

// TABLE: values of the Y= functions of the shared relation list (Y1..Y20,
// evaluated by the GRAPH engine, so composite references and List graphs give
// the same values as the graphs), with dY/dX columns when the Setup item
// Derivative is On; GPH-CON / GPH-PLT draw the table in GRAPH.
class TableApp final : public App {
public:
    AppId id() const override { return AppId::Table; }
    void enter(Context& ctx) override;
    void leave(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    struct Column { int plot; bool derivative; };
    const std::vector<Column>& columns() const noexcept { return cols_; }
    const std::vector<double>& xs() const noexcept { return xs_; }
    // Value shown in a cell (false: "ERROR"), from the shared evaluator.
    bool cell(int row, int column, double& out) const;
private:
    void rebuild(Context& ctx);
    bool setup_{false};
    Form form_;
    std::vector<double> xs_;
    std::vector<Column> cols_;
    graph::GraphEngine engine_;
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

class AiChatApp final : public App {
public:
    AppId id() const override { return AppId::AiChat; }
    void enter(Context& ctx) override;
    void leave(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    bool tick(Context& ctx) override;
    bool busy() const noexcept { return sending_; }
    const std::string& status() const noexcept { return status_; }
private:
    void send(Context& ctx);
    LineEditor input_{800};
    bool sending_{false};
    bool statusError_{false};
    std::string status_;
    int scroll_{0};                    // lines scrolled up from the newest text
    std::uint32_t lastDraw_{0};
};

} // namespace fx::app
