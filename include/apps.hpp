#pragma once
// Application classes of the PicoCalc FX firmware. Each mode owns its screen
// state, input handling and drawing; shared data lives in fx::Workspace.
#include "app.hpp"
#include "natural_editor.hpp"
#include "line_editor.hpp"
#include "app_widgets.hpp"
#include "file_store.hpp"
#include "typed_files.hpp"
#include "graph_engine.hpp"
#include "graph3d.hpp"
#include "table_model.hpp"
#include "pict.hpp"
#include "statistics.hpp"
#include "step_task.hpp"
#include "mode_shell.hpp"
#include "eactivity.hpp"
#include "text_editor.hpp"
#include <array>
#include <string>
#include <vector>

namespace fx::math { struct Size; }
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

// Editor MAT/VCT di Run-Matrix (fx-CG50 SHIFT 3 'MAT/VCT', manuale 2-42…2-48,
// 2-59…2-60): elenco di Mat A-Z (o Vct A-Z) con le dimensioni, DIM, modifica
// delle celle (EXE passa alla cella successiva), ROW-OP (SWAP, ×Rw, ×Rw+, Rw+),
// ROW e COLUMN DELETE/INSERT/ADD, DELETE/DEL-ALL, M⇔V. I valori stanno nel
// workspace (ws.matrices): sono quelli usati dai calcoli e vengono salvati con
// il resto dei dati.
class MatrixEditorView {
public:
    enum class Screen : std::uint8_t { List, Dim, Cells };
    static constexpr int AnsRow = 26;                    // "Mat Ans" / "Vct Ans" dopo Z (sola lettura)
    bool isOpen() const noexcept { return open_; }
    void open(Context& ctx, bool vectors = false);
    void close() noexcept { open_ = false; }
    void key(Context& ctx, const KeyEvent& ev);
    void draw(Context& ctx, gfx::Canvas& c);
    Screen screen() const noexcept { return screen_; }
    bool vectors() const noexcept { return vectors_; }
    int selected() const noexcept { return selected_; }
    const CellGrid& grid() const noexcept { return grid_; }
private:
    enum class Confirm : std::uint8_t { None, Delete, DeleteAll };
    enum class Menu : std::uint8_t { None, RowOp, Row, Column, Csv };
    CalcValue* current(Context& ctx) const;              // memoria della riga selezionata
    std::string name(int row) const;                     // "Mat A", "Vct Ans"
    void listKey(Context& ctx, const KeyEvent& ev);
    void dimKey(Context& ctx, const KeyEvent& ev);
    void cellsKey(Context& ctx, const KeyEvent& ev);
    void openDim(Context& ctx);
    void openCells(Context& ctx);
    void store(Context& ctx, CalcValue value);           // assegnazione controllata della memoria selezionata
    void menuChoice(Context& ctx, int choice);
    void askNext(Context& ctx);                          // raccoglie i numeri di una ROW-OP
    void writeCell(Context& ctx, int row, int col, const std::string& text);   // cella da un'espressione
    std::string cellEditText(Context& ctx, int row, int col) const;          // testo iniziale della modifica
    void csvMenu(Context& ctx, int choice);              // CSV: 0 LOAD, 1 SAVE·AS
    void csvChosen(Context& ctx);
    FileDialog dialog_;
    bool csvSave_{false};
    bool open_{false};
    bool vectors_{false};
    Screen screen_{Screen::List};
    int selected_{0}, first_{0};
    Confirm confirm_{Confirm::None};
    // DIM: righe e colonne in digitazione
    LineEditor dim_[2]{LineEditor{4}, LineEditor{4}};
    int dimField_{0};
    CellGrid grid_;
    PopupMenu menu_;
    Menu menuKind_{Menu::None};
    // Parametri di ROW-OP: domande ancora da rispondere e valori gia' dati
    int rowOp_{-1};
    std::vector<double> args_;
    LineEditor ask_{24};
    bool asking_{false};
};

// Schermate MatAns / VctAns / ListAns della fx-CG50 (v1.11.0): un risultato
// lista, matrice o vettore piu' grande della vista naturale (oltre 6x6 o 20
// elementi) si legge qui, scorrendo righe e colonne; la cella corrente e' indicata.
// I valori sono quelli delle memorie delle risposte (sola lettura).
class AnswerView {
public:
    bool isOpen() const noexcept { return open_; }
    void open(Context& ctx, ValueType type);
    void close() noexcept { open_ = false; }
    void key(Context& ctx, const KeyEvent& ev);
    void draw(Context& ctx, gfx::Canvas& c);
    ValueType type() const noexcept { return type_; }
    const CellGrid& grid() const noexcept { return grid_; }
private:
    const CalcValue* value(Context& ctx) const;
    bool open_{false};
    ValueType type_{ValueType::Matrix};
    CellGrid grid_;
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
    MatrixEditorView& matrixEditor() noexcept { return matrices_; }
    AnswerView& answerView() noexcept { return answers_; }
    int resultScroll() const noexcept { return resultScroll_; }
    bool menuOpen() const noexcept { return menu_.open; }     // OPTN, VARS, CATALOG... aperto (v1.12.0)
    enum class Menu : std::uint8_t { None, Math, MatVct, Action, MatRows, MatCols };
private:
    void syncInputMode(Context& ctx);                    // Setup Input/Output -> modalita' dell'editor
    bool clipKey(Context& ctx, const KeyEvent& ev);      // tasti durante una selezione CLIP
    void markError(const ErrorLocation& where);           // cursore sulla causa di un errore
    void evaluate(Context& ctx);
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
    std::vector<std::string> actions_;   // azioni delle voci del menu aperto (v1.12.0)
    std::string menuTitle_;
    pict::Image capture_;                // schermata catturata con SHIFT 7, in attesa del numero
    void showMenu(const std::string& title, const std::vector<std::pair<std::string, std::string>>& items);
    void openOptn(Context& ctx);
    void openVars(Context& ctx);
    void openCatalog(Context& ctx);
    void runAction(Context& ctx, const std::string& action, const std::string& label);
    void takeCapture(Context& ctx);
    void writeCapture(Context& ctx, int n);
    std::size_t matRows_{2};
    MatrixEditorView matrices_;
    AnswerView answers_;
    int resultScroll_{0};          // scorrimento del risultato selezionato piu' largo della riga
    int resultOverflow_{0};        // pixel nascosti di quel risultato (calcolati nel disegno)
};

class SymbolsApp final : public App {
public:
    AppId id() const override { return AppId::Symbols; }
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
private:
    unsigned page_{0}, selected_{0};
};

// Campo di input Math/Line (v1.12.0): lo stesso editor naturale di Run-Matrix
// (NaturalEditor, modalita' del SET UP "Input/Output") con i tasti dei modelli
// della PicoCalc e il disegno 2D di math_renderer. Usato dalle celle dei
// coefficienti di EQUATION e dall'equazione del Solver.
//  Tasti: caratteri, DEL, frecce, Home/End, F1 frazione (SHIFT: mista),
//  F2 √ (SHIFT: x²), F3 potenza (SHIFT: π), F4 menu MATH, F5 i, F6 Ans
//  (SHIFT: θ), STO →, Ctrl+C/X/V/Z/Y. EXE ed EXIT restano al chiamante.
class MathField {
public:
    void start(Context& ctx, const std::string& expression);
    bool key(Context& ctx, const KeyEvent& ev);           // true se il tasto e' stato usato
    std::string text() const { return editor_.toExpression(); }
    bool empty() const { return editor_.empty(); }
    bool menuOpen() const noexcept { return menu_.open; }
    void markError(const ErrorLocation& where);
    // Disegna l'input in [x, x+width) con la linea di base a `baseline`; scorre
    // orizzontalmente per tenere visibile il cursore.
    void draw(gfx::Canvas& c, int x, int baseline, int width, int height) const;
    void drawMenu(gfx::Canvas& c) const { if (menu_.open) menu_.draw(c); }
    static const char* const* softKeys(bool shift);
    NaturalEditor& editor() noexcept { return editor_; }
private:
    NaturalEditor editor_;
    PopupMenu menu_;
    mutable int scroll_{0};
};

// eActivity (v1.16.0, manuale fx-CG50 capitolo 10): elenco dei documenti
// (OPEN, NEW, DELETE, SEARCH, MEMO, cartelle) e workspace con righe di testo
// (con espressioni Math), di calcolo (nucleo di Run-Matrix), di stop,
// immagini e strisce che aprono le altre modalita' con i propri dati.
// Nucleo puro in eactivity.hpp; i documenti sono file .pceact (typed_files).
class EActivityApp final : public App {
public:
    AppId id() const override { return AppId::EActivity; }
    void enter(Context& ctx) override;
    void leave(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    bool tick(Context& ctx) override;
    enum class Screen : std::uint8_t { Files, Doc, Capacity, MemoList, MemoDetail, StripSize };
    struct Item { std::string name, file; bool folder{false}; std::uint32_t size{0}; };
    struct MemoRef { int line; std::uint8_t color; std::string text; };
    // ---- stato per i test
    Screen screen() const noexcept { return screen_; }
    bool documentOpen() const noexcept { return open_; }
    const eact::Document& document() const noexcept { return frames_.empty() ? empty_ : frames_.back().doc; }
    const eact::Document& mainDocument() const noexcept { return frames_.empty() ? empty_ : frames_.front().doc; }
    bool inNotes() const noexcept { return frames_.size() > 1; }
    int line() const noexcept { return frames_.empty() ? 0 : frames_.back().cur; }
    int firstLine() const noexcept { return frames_.empty() ? 0 : frames_.back().first; }
    std::size_t textCursor() const noexcept { return textPos_; }
    bool editingIsland() const noexcept { return island_; }
    bool editingTitle() const noexcept { return titleEdit_; }
    std::string inputText() const { return fieldLine_ || island_ ? (field_.empty() ? std::string() : field_.text()) : std::string(); }
    bool modified() const noexcept { return modified_; }
    bool lowerCase() const noexcept { return lower_; }
    const std::string& fileName() const noexcept { return fileName_; }
    const std::string& folder() const noexcept { return folder_; }
    const std::string& message() const noexcept { return message_; }
    const std::vector<Item>& items() const noexcept { return items_; }
    int selected() const noexcept { return sel_; }
    bool recalculating() const noexcept { return runner_.running(); }
    int marking() const noexcept { return markMode_; }
    bool memoOpen() const noexcept { return memoWindow_; }
    bool charsOpen() const noexcept { return charOpen_; }
    const std::vector<MemoRef>& memoList() const noexcept { return memos_; }
    std::size_t documentBytes() const;
private:
    struct Frame {
        eact::Document doc;
        int cur{0}, first{0};
        int stripLine{-1};                 // Notes: riga della striscia nel documento sotto
    };
    struct Row;
    struct Pict { std::string name; pict::Image img; bool ok{false}; };
    // memorie e limite
    void suspend(Context& ctx);
    void resume(Context& ctx);
    bool fits() const;
    [[noreturn]] static void memoryError();
    // file
    std::string directory() const;
    files::FileSystem& store(Context& ctx);
    void listFiles(Context& ctx);
    eact::Document readDocument(Context& ctx, const std::string& file);
    void writeDocument(Context& ctx, const std::string& file, const eact::Document& doc);
    void loadDocument(Context& ctx, eact::Document doc, const std::string& file);
    void openDocument(Context& ctx, const std::string& file);
    void newDocument(Context& ctx, const std::string& stem);
    void saveDocument(Context& ctx, const std::string& file);
    void closeDocument(Context& ctx);
    void exitDocument(Context& ctx);
    std::vector<std::string> fileStems() const;
    void nameDialog(const char* title, const std::string& initial, int purpose);
    void filesKey(Context& ctx, const KeyEvent& ev);
    void dialogKey(Context& ctx, const KeyEvent& ev);
    // memo
    void buildMemoList(const eact::Document& doc);
    eact::Document& memoDocument();
    void memoListKey(Context& ctx, const KeyEvent& ev);
    void storeMemo(Context& ctx, bool remove);
    void memoKey(Context& ctx, const KeyEvent& ev);
    void detailKey(Context& ctx, const KeyEvent& ev);
    // righe
    Frame& frame();
    eact::Document& doc();
    eact::Line& current();
    void enterLine(Context& ctx);
    void leaveLine(Context& ctx);
    void moveTo(Context& ctx, int line, bool atEnd);
    void insertLine(Context& ctx, eact::Line line, bool below = false);
    void deleteLine(Context& ctx);
    void insertText(Context& ctx, const std::string& s);
    void eraseText(std::size_t from, std::size_t to);
    void joinLines(Context& ctx, int index);
    void splitLine(Context& ctx);
    std::vector<Row> textRows(const eact::Line& l, bool current);
    math::Size islandSize(const std::string& text, std::size_t from, bool current);
    static int measurePiece(const std::string& t, std::size_t a, std::size_t b, bool island, void* user);
    ast::NodePtr tree(const std::string& expr);
    void caretAt(const std::vector<Row>& rows, const std::string& text, std::size_t pos, int& row, int& x) const;
    std::size_t positionAt(const Row& row, const std::string& text, int x) const;
    void visualMove(Context& ctx, int dir);
    void clearRow(Context& ctx);
    void textKey(Context& ctx, const KeyEvent& ev);
    void beginIsland(Context& ctx, std::size_t from, bool atEnd);
    void insertIsland(Context& ctx);
    void syncIsland(Context& ctx);
    void commitIsland(Context& ctx, bool after);
    bool fieldAtStart() const;
    bool fieldAtEnd() const;
    void islandKey(Context& ctx, const KeyEvent& ev);
    void evaluateLine(Context& ctx, eact::Line& l);
    void execute(Context& ctx);
    void startRecalc(Context& ctx, std::size_t from);
    void calcKey(Context& ctx, const KeyEvent& ev);
    void stripKey(Context& ctx, const KeyEvent& ev);
    void titleKey(Context& ctx, const KeyEvent& ev);
    void showStripMenu(int tag);
    void openStrip(Context& ctx, int index);
    void returnFromStrip(Context& ctx);
    void openMemoryEditor(Context& ctx, std::uint8_t entry);
    void openNotes(Context& ctx, int index);
    void closeNotes(Context& ctx);
    const pict::Image* picture(Context& ctx, const std::string& file);
    static void pictureSize(const pict::Image& img, int& w, int& h);
    void openPictureList(Context& ctx);
    void insertPicture(Context& ctx, const std::string& file);
    void openFormat();
    void startMark(Context& ctx, int mode, std::uint8_t value);
    void markRange(int& l0, std::size_t& p0, int& l1, std::size_t& p1) const;
    std::uint8_t shownAttr(int line, std::size_t byte, std::uint8_t stored) const;
    void applyMark(Context& ctx);
    void markKey(Context& ctx, const KeyEvent& ev);
    void page(Context& ctx, int dir);
    void ensureVisible(Context& ctx);
    void docAction(Context& ctx, int action);
    void menuChoice(Context& ctx, int tag, int choice);
    void confirmed(Context& ctx, int tag);
    void charKey(Context& ctx, const KeyEvent& ev);
    void updateBar();
    void docKey(Context& ctx, const KeyEvent& ev);
    // disegno
    int lineHeight(Context& ctx, int index, bool current);
    void drawText(Context& ctx, gfx::Canvas& c, int index, int y, bool current);
    void drawCalc(Context& ctx, gfx::Canvas& c, int index, int y, bool current);
    void drawLine(Context& ctx, gfx::Canvas& c, int index, int y, int h, bool current);
    void drawDoc(Context& ctx, gfx::Canvas& c);
    void drawFiles(Context& ctx, gfx::Canvas& c);
    void drawMemoList(Context& ctx, gfx::Canvas& c);
    void drawMemoWindow(gfx::Canvas& c);
    void drawChars(gfx::Canvas& c);

    Screen screen_{Screen::Files};
    std::vector<Frame> frames_;
    eact::Document empty_;
    std::string fileName_, folder_, message_;
    bool modified_{false}, open_{false}, suspended_{false};
    std::vector<Item> items_;
    int sel_{0}, first_{0};
    // riga corrente
    std::size_t textPos_{0};
    bool island_{false}, layoutCurrent_{false};
    std::size_t islandFrom_{0}, islandTo_{0};
    MathField field_;                      // riga di calcolo o espressione nel testo
    bool fieldLine_{false};                // field_ contiene la riga di calcolo corrente
    std::string fieldText_;                // testo della riga di calcolo quando vi si e' entrati
    LineEditor title_{16};                 // titolo della striscia (16 caratteri, 10-18)
    bool titleEdit_{false};
    bool lower_{false};                    // A<->a
    int markMode_{0};                      // 1 MARKER, 2 CHAR (10-10)
    std::uint8_t markValue_{0};
    int markLine_{0};
    std::size_t markPos_{0};
    // modali
    ModeShell shell_;
    const FKeyMenu* bar_{nullptr};
    FileDialog dialog_;
    int dialogFor_{0};
    bool memoWindow_{false};
    int memoLine_{-1};
    std::uint8_t memoColor_{0};
    TextEditor memoText_{eact::MaxMemoBytes, eact::MaxMemoBytes};
    std::vector<MemoRef> memos_;
    int memoSel_{0};
    eact::Document memoDoc_;               // MEMO dall'elenco dei file
    std::string memoFile_;
    Screen memoReturn_{Screen::Files}, detailReturn_{Screen::Doc};
    bool charOpen_{false};
    int charPage_{0}, charSel_{0};
    // strisce e memorie del documento
    int stripLine_{-1};                    // striscia aperta (-2: LIST o MAT/VCT del documento)
    std::vector<std::uint8_t> docMemories_;   // memorie del documento con eActivity lasciata (MENU)
    // ricalcolo a passi (10-9)
    struct Recalc final : step::Task {
        EActivityApp* app{nullptr};
        Context* ctx{nullptr};
        bool step(step::Budget& budget) override;
        std::size_t total() const override { return end - start; }
        std::size_t done() const override { return next - start; }
        std::size_t start{0}, next{0}, end{0};
    };
    Recalc recalc_;
    step::Runner runner_;
    // immagini e alberi delle espressioni gia' preparati
    std::vector<Pict> picts_;
    std::size_t pictBytes_{0};
    std::vector<std::pair<std::string, ast::NodePtr>> trees_;
};

// EQUATION (fx-CG50, manuale 4-1...4-5): SIML (2-6 incognite), POLY (grado
// 2-6), SOLVER su qualsiasi variabile, e l'estensione INEQ (disequazioni
// polinomiali, non presente sulla fx-CG50). Calcolo in equation.hpp.
class EquationApp final : public App {
public:
    AppId id() const override { return AppId::Equation; }
    void enter(Context& ctx) override;
    void leave(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    enum class Screen { Menu, ChooseSize, Coefficients, Solutions, SolverTable, SolverResult };
    enum class Mode { Simultaneous, Polynomial, Solver, Inequality };
    Screen screen() const noexcept { return screen_; }
    Mode mode() const noexcept { return mode_; }
    // ---- per i test e lo stato mostrato
    bool editing() const noexcept { return editing_; }
    int row() const noexcept { return row_; }
    int col() const noexcept { return col_; }
    const eq::SimulResult& simul() const noexcept { return simul_; }
    const std::vector<eq::Root>& roots() const noexcept { return shownRoots_; }
    const std::string& message() const noexcept { return message_; }   // "No Solution", "Infinitely Many Solutions"
    std::size_t solutionRows() const noexcept;
    std::string solutionText(std::size_t i, const Context& ctx) const;  // testo lineare della riga i (soluzione o formula)
    std::string selectedText(const Context& ctx) const;                 // valore completo in basso (testo lineare)
    const std::vector<char>& solverVars() const noexcept { return vars_; }
    const eq::SolverOutcome& solverOutcome() const noexcept { return outcome_; }
    const ResultList& inequalityResults() const noexcept { return ineqResults_; }
private:
    // schermate
    void chooseMode(Context& ctx, Mode m);
    void openCoefficients(Context& ctx);
    void coefficientsKey(Context& ctx, const KeyEvent& ev);
    void solutionsKey(Context& ctx, const KeyEvent& ev);
    void openSolver(Context& ctx);
    void solverKey(Context& ctx, const KeyEvent& ev);
    void solve(Context& ctx);
    void solveSolver(Context& ctx);
    // coefficienti
    int gridRows(const Context& ctx) const;
    int gridCols(const Context& ctx) const;
    eq::Number& cell(Context& ctx, int r, int c) const;
    void commitCell(Context& ctx);
    void clearResults(Context& ctx);
    eq::View view(const Context& ctx, std::size_t row) const;
    // Solver: righe della tabella (0 = Eq, poi le variabili, poi Lower e Upper).
    int solverRows() const noexcept { return static_cast<int>(vars_.size()) + 3; }
    void refreshVariables(Context& ctx);
    void commitSolverRow(Context& ctx);
    // disegno
    void drawHeader(Context& ctx, gfx::Canvas& c, int y) const;
    void drawCoefficients(Context& ctx, gfx::Canvas& c);
    void drawSolutions(Context& ctx, gfx::Canvas& c);
    void drawSolver(Context& ctx, gfx::Canvas& c);
    void drawSolverResult(Context& ctx, gfx::Canvas& c);
    void drawBottomValue(gfx::Canvas& c, const ast::Node& tree, int baseline);

    Screen screen_{Screen::Menu};
    Mode mode_{Mode::Simultaneous};
    int menuSel_{0};
    int row_{0}, col_{0}, firstCol_{0}, firstRow_{0};
    bool editing_{false};
    MathField field_;
    // risultati
    eq::SimulResult simul_;
    std::vector<eq::Root> shownRoots_;
    std::string message_;
    std::vector<std::uint8_t> views_;            // S⇔D per riga: 0 automatica, 1 decimale, 2 esatta
    int solRow_{0}, solFirst_{0};
    int scroll_{0};
    mutable int overflow_{0};                    // pixel del valore in basso oltre lo schermo
    ResultList ineqResults_;
    // Solver
    std::vector<char> vars_;
    int solverRow_{0}, solverFirst_{0};
    eq::SolverOutcome outcome_;
    char solvedVar_{0};
    std::array<double, 26> recall_{};            // valori prima dell'ultimo SOLVE (RECALL)
    std::array<bool, 26> recallUsed_{};
    bool recallValid_{false};
};

class StatisticsApp final : public App {
public:
    AppId id() const override { return AppId::Statistics; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    enum class Screen { Lists, Results, Graph, Form, Set };
    enum class GraphKind { Scatter, XYLine, Histogram, Box, Regression };
    Screen screen() const noexcept { return screen_; }
    const ResultList& results() const noexcept { return results_; }
    const CellGrid& grid() const noexcept { return grid_; }
    const FileDialog& fileDialog() const noexcept { return dialog_; }
private:
    void setupGrid(Context& ctx);
    void openSet(Context& ctx, bool csvFormat = false);  // SET XList/YList oppure formato CSV
    void listOption(Context& ctx, int choice);           // OPTN dell'editor delle liste: CSV, DEL-ALL, SET
    void fileChosen(Context& ctx);                       // LOAD / SAVE·AS del CSV dopo la scelta del nome
    FileDialog dialog_;
    bool csvSet_{false};
    bool saving_{false};
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
    int probePixel(Context& ctx, gfx::Canvas& c, int row, int column) override;

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
    bool layerCached() const noexcept { return layerValid_; }   // static layers kept in PSRAM (trace/pointer)
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
    struct InputBox { Ask ask{Ask::None}; std::string prompt; LineEditor editor{40}; bool text{false}; bool replace{false}; };

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
    // Pop-up showing the current value: the first typed character replaces it.
    void askValue(Ask a, const std::string& prompt, const std::string& current, bool text = false);
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
    bool built_{false}, renderDone_{false}, truncationShown_{false};
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
    // Static layers of the plot (background, regions, grid, axes, curves,
    // sketches, marks) kept while the trace/pointer cursor moves: with the
    // PSRAM budgets a screen may hold 400000 curve points, far slower to
    // redraw than to copy back (180 KB, PSRAM only, limits::ram()).
    std::vector<std::uint16_t> layer_;
    std::uint64_t layerKey_{0};
    bool layerValid_{false};
    std::uint64_t layerFingerprint(const Context& ctx) const;
    double factorX_{2};
    bool panGrab_{false};
    Screen formatReturn_{Screen::List};
    std::vector<graph::Plot> sketchPlots_;               // SKETCH Inverse graphs (x = f(y))
    std::vector<graph::Curve> sketchCurves_;
    std::uint32_t lastMove_{0};
    int speed_{1};                                       // pointer acceleration
    bool keepPointer_{false};                            // next pointer starts at the trace point
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
    bool ownsSetup() const override { return true; }   // SETUP: Variable, Derivative, Angle
    // Table relation list (manual 5-32), function editor, range form,
    // Setup, the number table (manual 5-33..5-35).
    enum class Screen : std::uint8_t { List, Edit, Set, Setup, Table };
    Screen screen() const noexcept { return static_cast<Screen>(shell_.screens.top()); }
    const std::vector<table::Column>& columns() const noexcept { return cols_; }
    int row() const noexcept { return row_; }
    int column() const noexcept { return col_; }        // -1: the variable column (x, θ, T)
    // Value of a cell of the generated table (column -1: the variable); false: "ERROR".
    bool cell(Context& ctx, int row, int column, double& out);
    std::string cellText(Context& ctx, int row, int column);
private:
    enum class Menu : std::uint8_t { None, Type, Style, Row, Optn };
    enum class Ask : std::uint8_t { None, EditValue, ListMem };
    enum class Confirm : std::uint8_t { None, DeleteFunction, DeleteTable };
    struct Line { int fn; int sub; };                    // relation list line (sub 1 = Yt of a parametric pair)
    std::vector<Line> lines(Context& ctx) const;
    GraphType lineType(Context& ctx, int fn) const;
    bool ensure(Context& ctx);                           // engine and columns up to date with the workspace
    std::uint64_t fingerprint(Context& ctx) const;
    void generate(Context& ctx);                         // TABLE (F6): rows from the range or the list
    void listKey(Context& ctx, const KeyEvent& ev);
    void editKey(Context& ctx, const KeyEvent& ev);
    void tableKey(Context& ctx, const KeyEvent& ev);
    void formKey(Context& ctx, const KeyEvent& ev);
    void beginEdit(Context& ctx, bool keep, char first = 0);
    void storeEdit(Context& ctx);
    void menuChoice(Context& ctx, int tag, int choice);
    void answer(Context& ctx);
    void openSet(Context& ctx);
    void openSetup(Context& ctx);
    void drawList(Context& ctx, gfx::Canvas& c);
    void drawTable(Context& ctx, gfx::Canvas& c);
    // v1.15.0 (F4): screens, F-key bar, confirmations, value prompts and
    // menus come from the shared mode shell (same drawing as before).
    ModeShell shell_;
    Screen current() const noexcept { return static_cast<Screen>(shell_.screens.top()); }
    void show(Screen s) noexcept { shell_.screens.reset(static_cast<int>(s)); }   // List <-> Table (root)
    int listRow_{0}, listFirst_{0};
    int row_{0}, col_{-1}, firstRow_{0}, firstCol_{0};
    // relation list editing
    NaturalEditor editor_;
    int editFn_{-1}, editSub_{0};
    bool editNew_{false};
    // engine and columns of the table type
    graph::GraphEngine engine_;
    std::vector<table::Column> cols_;
    std::uint64_t fingerprint_{0};
    bool built_{false};
    std::string buildError_;
    Form form_;
};

class SystemApp final : public App {
public:
    AppId id() const override { return AppId::System; }
    void enter(Context& ctx) override;
    void leave(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    const std::vector<std::pair<bool, std::string>>& diagnostics() const noexcept { return diagLines_; }
private:
    void apply(Context& ctx);
    void runDiagnostics(Context& ctx);
    Form form_;
    bool info_{false};
    bool confirmReset_{false};
    // DIAG: self-tests for the validation on the PicoCalc (diagnostics.hpp)
    bool diag_{false};
    int diagTop_{0};
    std::vector<std::pair<bool, std::string>> diagLines_;
};

class MemoryApp final : public App {
public:
    AppId id() const override { return AppId::Memory; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    // v1.15.0 (F3): vista dei file per tipo (F3 FILES): Data = dati del
    // workspace, Types = tipi di file della scheda, Files = file di un tipo.
    enum class Page : std::uint8_t { Data, Types, Files };
    Page page() const noexcept { return page_; }
    const std::vector<files::TypedEntry>& entries() const noexcept { return entries_; }
private:
    void openTypes(Context& ctx);
    void openFiles(Context& ctx);
    void filesKey(Context& ctx, const KeyEvent& ev);
    void drawFiles(Context& ctx, gfx::Canvas& c);
    int selected_{0};
    int confirm_{-1};
    Page page_{Page::Data};
    std::vector<files::FileType> types_;
    std::vector<std::pair<std::size_t, std::uint64_t>> typeUsage_;   // file e byte per tipo
    std::string typeStatus_;
    int typeSel_{0}, fileSel_{0};
    std::vector<files::TypedEntry> entries_;
    FileDialog dialog_;
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

// 3D GRAPH (v1.14.0, fx-CG50 manual chapter 16): up to three graphs of the
// templates Line, Plane, Sphere, Cylinder, Cone or of the directly input
// types Z=, Param, Rot X, Rot Y. List (SELECT, DELETE, TYPE, 3D-GMEM, DRAW,
// FORMAT), function selection screen, coefficient and function input, 3D
// View Window (3D-VMEM), Setup (Axes, Box, Label, Plot/LineCol...), graph
// screen with rotation (cursor keys, ROTATE), ZOOM (IN, OUT, VIEW-X/Y/Z,
// ORIGINAL), TRACE, SKETCH (Cls, Plot, Text), G-Solve (CROSS, INTSECT,
// RELATION), PICTURE and CAPTURE. The lattices are computed in tick() (a
// budget of points per tick, AC stops) and kept between redraws: rotating or
// zooming only re-projects. Meshes, z-buffer and the image of the scene
// (TRACE/SKETCH) live in PSRAM on the Pico Plus 2 W; without PSRAM the
// reduced profile is used (painter's algorithm, 25 points per side).
// List and input screens: graph3d_app.cpp; graph screen: graph3d_plot.cpp.
class Graph3DApp final : public App, private step::Task {
public:
    AppId id() const override { return AppId::Graph3D; }
    void enter(Context& ctx) override;
    void leave(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    bool tick(Context& ctx) override;
    bool ownsSetup() const override { return true; }

    enum class Screen : std::uint8_t { List, Select, Coef, Input, Window, Setup, Format, Plot };
    enum class Mode : std::uint8_t { View, Trace, Cross, Intsect, Relation, SketchPlot, SketchText };
    // Function-key submenus (the fx-CG50 replaces the function menu).
    enum class Sub : std::uint8_t { None, Rotate, GMem, VMem, Zoom, Sketch, GSolve, CrossAxis, Optn, Picture, AutoRotate };
    Screen screen() const noexcept { return screen_; }
    Mode mode() const noexcept { return mode_; }
    Sub sub() const noexcept { return sub_; }
    bool sampling() const noexcept { return sampling_; }
    bool drawn() const noexcept { return samplesValid_; }
    void finishSampling(Context& ctx);                       // completes the computation now (tests)
    const g3d::Mesh& mesh(std::size_t graph, std::size_t patch = 0) const { return meshes_.at(graph).at(patch); }
    std::size_t points() const noexcept { return points_; }  // points per side in use (after the memory limits)
    bool zbuffer() const noexcept { return zbuffer_; }
    const g3d::RenderStats& lastRender() const noexcept { return stats_; }
    const std::string& message() const noexcept { return message_; }
    // TRACE: graph (row 0..2) and coordinates of the pointer (false: undefined).
    int traceGraph() const noexcept { return traceGraph_; }
    bool tracePoint(Context& ctx, g3d::Vec3& out) const;
    // G-Solve: the pair of graphs shown (rows) and the CROSS position.
    std::pair<int, int> solvePair() const noexcept;
    g3d::Axis crossAxis() const noexcept { return crossAxis_; }
    double crossValue() const noexcept { return crossValue_; }
    std::size_t sketchMarks() const noexcept { return marks_.size(); }
    bool autoRotating() const noexcept { return autoSteps_ > 0; }
    const g3d::Function& editing() const noexcept { return edit_; }   // function on the input screens
    int coefCell() const noexcept { return coefCell_; }
    std::size_t retainedBytes() const noexcept;               // lattices, image cache and captures still allocated
    bool layerCached() const noexcept { return layerValid_; }
    static constexpr gfx::Rect Viewport{0, ui::StatusH, ui::ScreenW, ui::ContentBottom - ui::StatusH};   // area of GRAPH

private:
    struct Ask {
        enum class Kind : std::uint8_t { None, GMemStore, GMemRecall, VMemStore, VMemRecall, PictStore, Capture, TraceU, TraceV, CrossValue, SketchText };
        Kind kind{Kind::None};
        std::string prompt;
        LineEditor editor{60};
    };
    struct Mark { int x{}, y{}; bool text{}; std::string s; };
    struct LayerKey {
        std::uint32_t gen{}; double theta{}, phi{}; float zoom{}; g3d::Setup setup{}; bool zbuffer{};
        bool operator==(const LayerKey&) const = default;
    };
    // ---- graph3d_app.cpp: list, selection, coefficients, input, window, setup, format, memories
    void listKey(Context& ctx, const KeyEvent& ev);
    void selectKey(Context& ctx, const KeyEvent& ev);
    void coefKey(Context& ctx, const KeyEvent& ev);
    void inputKey(Context& ctx, const KeyEvent& ev);
    void windowKey(Context& ctx, const KeyEvent& ev);
    void setupKey(Context& ctx, const KeyEvent& ev);
    void formatKey(Context& ctx, const KeyEvent& ev);
    void askKey(Context& ctx, const KeyEvent& ev);
    void ask(Ask::Kind kind, std::string prompt, std::string initial = {});
    void answered(Context& ctx, Ask::Kind kind, const std::string& text);
    void openSelect(Context& ctx);
    void openTemplate(Context& ctx, g3d::Kind kind);
    void openInput(Context& ctx, g3d::Kind kind);
    void openWindow(Context& ctx, bool fromPlot);
    void loadWindowForm(Context& ctx, const g3d::Window& w);
    g3d::Window windowFromForm(Context& ctx) const;
    void applyWindow(Context& ctx);
    void openSetup(Context& ctx);
    void applySetup(Context& ctx);
    void openFormat(Context& ctx);
    void setFunction(Context& ctx);                           // SET: validates edit_ and stores it in the row
    void commitInputLine(Context& ctx);
    void beginInputEdit(Context& ctx, bool keep, char first = 0);
    void storeGraphMemory(Context& ctx, int n);
    void recallGraphMemory(Context& ctx, int n);
    void changed(Context& ctx);                               // data changed: samples are recomputed on the next DRAW
    void drawList(Context& ctx, gfx::Canvas& c);
    void drawSelect(Context& ctx, gfx::Canvas& c);
    void drawCoef(Context& ctx, gfx::Canvas& c);
    void drawInput(Context& ctx, gfx::Canvas& c);
    void drawAsk(gfx::Canvas& c) const;
    // ---- graph3d_plot.cpp: computation and graph screen
    void startDraw(Context& ctx);
    void showPlot(Context& ctx);                              // G<->T: draws again only if something changed
    void stopSampling() noexcept;
    void releaseScene() noexcept;
    void plotKey(Context& ctx, const KeyEvent& ev);
    void subKey(Context& ctx, const KeyEvent& ev);
    void traceKey(Context& ctx, const KeyEvent& ev);
    void crossKey(Context& ctx, const KeyEvent& ev);
    void solveKey(Context& ctx, const KeyEvent& ev);
    void sketchKey(Context& ctx, const KeyEvent& ev);
    void startTrace(Context& ctx);
    void moveTrace(int du, int dv);
    void startCross(Context& ctx, g3d::Axis axis);
    void startSolve(Context& ctx, Mode mode);
    void setView(Context& ctx, double theta, double phi, float zoom);
    void rotateBy(Context& ctx, double dTheta, double dPhi);
    void autoZ(Context& ctx);
    void storePicture(Context& ctx, int n);
    void takeCapture(Context& ctx);
    void writeCapture(Context& ctx, int n);
    bool traceDomain(int graph, double& u0, double& u1, double& v0, double& v1, std::size_t& nu, std::size_t& nv, bool& oneD) const;
    bool pointOf(Context& ctx, int graph, double u, double v, g3d::Vec3& out) const;
    bool pointOfRaw(Context& ctx, int graph, double u, double v, g3d::Vec3& out) const;
    void drawPlot(Context& ctx, gfx::Canvas& c);
    void drawOverlays(Context& ctx, gfx::Canvas& c, const g3d::Projector& proj);
    std::string graphLabel(Context& ctx, int graph) const;

    Screen screen_{Screen::List};
    Mode mode_{Mode::View};
    Sub sub_{Sub::None};
    int row_{0};
    bool confirmDelete_{false};
    Ask ask_;
    // selection and input screens
    int selectRow_{0};
    Screen inputReturn_{Screen::List};
    g3d::Function edit_{};
    int coefCell_{0};
    bool coefEditing_{false};
    LineEditor coefEditor_{120};
    int inputLine_{0};
    bool inputEditing_{false};
    NaturalEditor editor_;
    Form window_, setup_, format_;
    bool windowFromPlot_{false};
    Screen setupReturn_{Screen::List};
    // scene
    std::array<std::array<g3d::Mesh, g3d::MaxPatches>, g3d::Functions> meshes_{};
    std::array<std::array<g3d::PatchSpec, g3d::MaxPatches>, g3d::Functions> specs_{};
    std::array<std::size_t, g3d::Functions> patchCount_{};
    std::array<g3d::Shape, g3d::Functions> shapes_{};
    std::array<std::array<CompiledExpression, 3>, g3d::Functions> compiled_{};
    std::array<bool, g3d::Functions> active_{};
    std::array<bool, g3d::Functions> lineVisible_{};
    std::array<g3d::Vec3, g3d::Functions> lineA_{}, lineB_{};
    std::array<double, g3d::Functions> lineT0_{}, lineT1_{};
    g3d::Window drawnWindow_{};                               // window of the computed lattices
    bool sampling_{false}, samplesValid_{false}, zbuffer_{false};
    std::size_t sampleGraph_{0}, samplePatch_{0}, sampleNext_{0}, points_{0};
    // v1.15.0 (F6): the progressive computation runs on the shared StepTask
    // runner, 600 lattice points per step as in v1.14.0.
    step::Runner runner_;
    Context* stepCtx_{nullptr};                               // context of the running computation
    bool step(step::Budget& budget) override;
    std::size_t total() const override;
    std::size_t done() const override;
    double originalTheta_{20}, originalPhi_{70};
    std::uint32_t dataGen_{0};
    std::pmr::vector<std::uint16_t> layer_{mem::resource(mem::Policy::PREFER_PSRAM)};
    LayerKey layerKey_{};
    bool layerValid_{false};
    g3d::RenderStats stats_{};
    std::string message_;
    bool memoryError_{false};
    // TRACE, CROSS, INTSECT / RELATION, SKETCH, auto rotation, capture
    int traceGraph_{-1};
    double traceU_{0}, traceV_{0};
    double askedU_{0};
    g3d::Axis crossAxis_{g3d::Axis::X};
    double crossValue_{0};
    std::array<std::pair<int, int>, 3> pairs_{};
    int pairCount_{0}, pairIndex_{0};
    std::vector<Mark> marks_;
    int px_{0}, py_{0};
    int autoSteps_{0}, autoDir_{0};
    std::uint32_t autoLast_{0};
    pict::Image capture_{};
    int pictPending_{0};                                      // PICTURE STORE: Pict n captured by the next drawPlot()
    bool pictReady_{false};                                   // ... and written by tick()
    pict::Image picture_{};
};

} // namespace fx::app
