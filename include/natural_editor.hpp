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
// color: 0 = ink; for parentheses 1..5 = nesting level colour (blue, red,
// green, magenta, black, as on the fx-CG50); for Highlight 0 = selection
// (CLIP), 1 = cause of an error. Highlight ops are filled rectangles drawn
// below the text.
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

// Structured fx-CG50 style input editor. The tree uses the shared ast::Node
// type: Row nodes hold Token leaves and template boxes (Fraction, Radical,
// Power, Subscript, Group, Calculus, Call, Matrix). toExpression() produces the
// canonical linear text consumed by the single expression parser.
//
// Two input modes, as the fx-CG50 Setup "Input/Output" item:
//  * Math (default): templates are 2D boxes (fractions, roots, powers,
//    matrices) and text is imported structurally;
//  * Line: every key inserts linear text ("/", "^", "√(", "[["), the input is
//    one line of tokens and linear text is imported as it is.
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
    // MAT/VCT template: rows x cols cells (1..6), cursor in the first cell.
    // Line mode: "[[,][,]]" with the cursor after "[[".
    void insertMatrix(std::size_t rows, std::size_t cols);

    // Input mode (fx-CG50 Setup "Input/Output"); switching converts the input.
    void setLinear(bool linear);
    bool linear() const noexcept { return linear_; }

    // ---- CLIP: selection of part of the input (fx-CG50 SHIFT 8).
    // The selection stays in one row of the tree, as the cursor of the Math
    // mode does; moving past the start or the end of a template field selects
    // the whole template. Copy and cut extend the range so that parentheses,
    // brackets and braces stay balanced: a valid expression is never split.
    struct Range { const NaturalNode* row{}; std::size_t begin{}, end{}; bool empty() const noexcept { return begin >= end; } };
    void beginSelection();
    void cancelSelection() noexcept { selecting_ = false; }
    bool selecting() const noexcept { return selecting_; }
    bool selectLeft();
    bool selectRight();
    Range selection() const;                       // normalised (balanced) range of the active row
    ast::NodePtr copySelection() const;            // Row with copies of the selected nodes (nullptr when empty)
    ast::NodePtr cutSelection();                   // copy + delete (undoable); ends the selection
    bool deleteSelection();                        // delete without copying; ends the selection

    // ---- Cause of an error: position/length are a byte range of toExpression()
    // (the parser and the evaluator report it). The cursor moves to the end of
    // the cause, as on the fx-CG50 (example 14÷0×2.3: after the 0), and the
    // range is highlighted until clearErrorMark(). False when the text does
    // not match the input.
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
    std::size_t anchor_{};          // other end of the selection, in the active row
    Range errorMark_;
    std::vector<Snapshot> undo_, redo_;

    NaturalNode& activeRow();
    const NaturalNode& activeRow() const;
    NaturalNode* parentBox();
    bool enterChild(std::size_t boxIndex, std::size_t child, bool atEnd);
    bool leaveChild(bool toRight);
    void insertBox(ast::NodePtr box, std::size_t focusChild);
    void insertNode(ast::NodePtr node);
    void insertLinear(const std::string& text, std::size_t cursorBack = 0);   // Line-mode template
    void mergeWord();
    void pushUndo();
    void checkLimits();   // rolls back the edit recorded by the last pushUndo() and throws
};
} // namespace fx
