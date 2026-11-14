#pragma once
#include "expression_ast.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx {
using NaturalNodeKind = ast::Kind;
using NaturalNode = ast::Node;

enum class NaturalKey { Left, Right, Up, Down, Backspace, Delete, Enter, Character };
struct NaturalKeyEvent { NaturalKey key{NaturalKey::Character}; char character{}; };

// Drawing operation emitted by the math layout engine. Text ops carry the font
// face (0 = Medium 8x16, 1 = Small 6x12); y is the TOP of the text cell.
// color: 0 = inchiostro; per le parentesi 1..5 = colore del livello (blu,
// rosso, verde, magenta, nero, come sulla fx-CG50); per Highlight 0 = selezione
// (CLIP), 1 = causa di un errore. Le operazioni Highlight sono rettangoli pieni
// disegnati sotto il testo.
struct NaturalDrawOp {
    enum class Kind { Text, Line, Cursor, Placeholder, Highlight };
    Kind kind{Kind::Text};
    int x{}, y{}, x2{}, y2{};
    std::string text;
    std::uint8_t face{};
    std::uint8_t color{};
};

// Deep copy of an editor tree (history entries, clipboard, undo snapshots).
ast::NodePtr cloneTree(const NaturalNode& node);

// Editor strutturato in stile fx-CG50. L'albero usa il tipo condiviso
// ast::Node: i nodi Row contengono foglie Token e modelli (Fraction, Radical,
// Power, Subscript, Group, Calculus, Call, Matrix). toExpression() produce il
// testo lineare canonico letto dall'unico parser delle espressioni.
//
// Due modalita' di input, come la voce "Input/Output" del Setup della fx-CG50:
//  * Math (predefinita): i modelli sono riquadri 2D (frazioni, radici, potenze,
//    matrici) e il testo si importa in modo strutturale;
//  * Line: ogni tasto inserisce testo lineare ("/", "^", "√(", "[["), l'input e'
//    una riga di token e il testo lineare si importa cosi' com'e'.
class NaturalEditor {
public:
    // Input limits (bounded recursion and memory on the RP2350): an edit that
    // would exceed them is undone and raises "Stack ERROR" / "Memory ERROR".
    static constexpr std::size_t MaxTemplateDepth = 12;   // nested template boxes
    static constexpr std::size_t MaxNodes = 400;          // nodes in the whole input tree

    NaturalEditor();
    void clear();
    // Structural import of linear text: "(a)/(b)" and frac(a,b) become fractions,
    // ^ becomes a power box, sqrt/root/integral/derivative/sum/abs become templates.
    void setExpression(const std::string& expression);
    void setTree(const NaturalNode& root);                // deep copy, cursor at end
    bool empty() const noexcept;

    void insert(char c);
    void insertText(const std::string& text);           // inserts each ASCII char / UTF-8 symbol
    void insertSymbol(const std::string& utf8);          // single atomic token (pi, theta, ->, ...)
    void insertFraction();
    void insertMixedFraction();                            // a b/c template (whole, numerator, denominator)
    // Repeating digits after a decimal number (0.1 + period 6 = 0.1666...).
    // Throws SyntaxError when the cursor does not follow a decimal number.
    void insertPeriod();
    void insertRadical();
    void insertNthRoot(unsigned index);
    void insertRootTemplate();                             // x√ template with an editable index: root(n, x)
    void insertPower();
    void insertSquare();                                  // x^2 template
    void insertSubscript();
    void insertGroup();
    void insertAbs();
    void insertLogBase();                                  // log_a(b)
    void insertExp();                                      // e^[]
    void insertTenPower();                                 // 10^[]
    void insertIntegral();
    void insertDerivative();
    void insertSummation();
    void insertProduct();
    // Modello MAT/VCT: rows x cols celle (1..6), cursore nella prima cella.
    // Modalita' Line: "[[,][,]]" con il cursore dopo "[[".
    void insertMatrix(std::size_t rows, std::size_t cols);

    // Modalita' di input (Setup "Input/Output" della fx-CG50); il cambio converte l'input.
    void setLinear(bool linear);
    bool linear() const noexcept { return linear_; }

    // ---- CLIP: selezione di una parte dell'input (fx-CG50 SHIFT 8).
    // La selezione resta in una riga dell'albero, come il cursore della modalita'
    // Math; oltre l'inizio o la fine di un campo di un modello seleziona l'intero
    // modello. Copia e taglia estendono l'intervallo in modo che parentesi tonde,
    // quadre e graffe restino bilanciate: un'espressione valida non viene spezzata.
    struct Range { const NaturalNode* row{}; std::size_t begin{}, end{}; bool empty() const noexcept { return begin >= end; } };
    void beginSelection();
    void cancelSelection() noexcept { selecting_ = false; }
    bool selecting() const noexcept { return selecting_; }
    bool selectLeft();
    bool selectRight();
    Range selection() const;                       // intervallo normalizzato (bilanciato) della riga attiva
    ast::NodePtr copySelection() const;            // Row con le copie dei nodi selezionati (nullptr se vuota)
    ast::NodePtr cutSelection();                   // copia + elimina (annullabile); termina la selezione
    bool deleteSelection();                        // elimina senza copiare; termina la selezione

    // ---- Causa di un errore: posizione/lunghezza sono un intervallo di byte di
    // toExpression() (li indicano il parser e il valutatore). Il cursore va alla
    // fine della causa, come sulla fx-CG50 (esempio 14÷0×2.3: dopo lo 0), e
    // l'intervallo resta evidenziato fino a clearErrorMark(). False quando il
    // testo non corrisponde all'input.
    bool markError(std::size_t position, std::size_t length);
    Range errorMark() const noexcept { return errorMark_; }
    void clearErrorMark() noexcept { errorMark_ = Range{}; }

    void handle(const NaturalKeyEvent& event);
    bool moveLeft(); bool moveRight(); bool moveUp(); bool moveDown();
    void moveHome(); void moveEnd();
    bool backspace(); bool eraseForward();

    bool undo(); bool redo();
    ast::NodePtr copyTree() const;                        // clipboard copy of the whole input
    void paste(const NaturalNode& tree);                  // inserts the tree's row content at the cursor

    std::string toExpression() const;
    // Layout with the cursor, using the same metrics as the math renderer.
    std::vector<NaturalDrawOp> layout(int x = 0, int baseline = 0, int maxWidth = 300) const;

    std::size_t cursorIndex() const noexcept { return cursor_; }
    const std::vector<std::size_t>& cursorPath() const noexcept { return rowPath_; }
    const NaturalNode* activeRowPtr() const;
    const NaturalNode& root() const { return *root_; }
    std::string error() const { return error_; }
    void clearError() { error_.clear(); }

private:
    struct Snapshot { ast::NodePtr root; std::vector<std::size_t> path; std::size_t cursor; };
    std::shared_ptr<NaturalNode> root_;
    std::vector<std::size_t> rowPath_;
    std::size_t cursor_{};
    std::string error_;
    bool autoRootParenPending_{};
    bool autoRootParenOpened_{};
    bool linear_{};
    bool selecting_{};
    std::size_t anchor_{};          // altro estremo della selezione, nella riga attiva
    Range errorMark_;
    std::vector<Snapshot> undo_, redo_;

    NaturalNode& activeRow();
    const NaturalNode& activeRow() const;
    NaturalNode* parentBox();
    bool enterChild(std::size_t boxIndex, std::size_t child, bool atEnd);
    bool leaveChild(bool toRight);
    void insertBox(ast::NodePtr box, std::size_t focusChild);
    void insertNode(ast::NodePtr node);
    void insertLinear(const std::string& text, std::size_t cursorBack = 0);   // modello in modalita' Line
    void mergeWord();
    void pushUndo();
    void checkLimits();   // rolls back the edit recorded by the last pushUndo() and throws
};
} // namespace fx
