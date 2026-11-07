#pragma once
#include <cstddef>
#include "expression_ast.hpp"
#include <string>
#include <vector>

namespace fx {
using NaturalNodeKind = ast::Kind;
using NaturalNode = ast::Node;
enum class NaturalKey { Left, Right, Up, Down, Backspace, Delete, Enter, Character };
struct NaturalKeyEvent { NaturalKey key{NaturalKey::Character}; char character{}; };
struct NaturalDrawOp { enum class Kind { Text, Line, Cursor }; Kind kind{Kind::Text}; int x{}, y{}, x2{}, y2{}; std::string text; };
class NaturalEditor {
public:
    NaturalEditor();
    void clear();
    void setExpression(const std::string& expression);
    void insert(char c);
    void insertFraction();
    void insertRadical();
    void insertNthRoot(unsigned index);
    void insertPower();
    void insertSubscript();
    void insertGroup();
    void insertIntegral();
    void insertDerivative();
    void insertSummation();
    void handle(const NaturalKeyEvent& event);
    bool moveLeft(); bool moveRight(); bool moveUp(); bool moveDown();
    bool backspace(); bool eraseForward();
    std::string toExpression() const;
    std::vector<NaturalDrawOp> layout(int x=0, int baseline=0, int maxWidth=300) const;
    std::size_t cursorIndex() const { return cursor_; }
    const NaturalNode& root() const { return *root_; }
    std::string error() const { return error_; }
    void clearError() { error_.clear(); }
private:
    std::shared_ptr<NaturalNode> root_;
    std::vector<std::size_t> rowPath_;
    std::size_t cursor_{};
    std::string error_;
    bool autoRootParenPending_{};
    bool autoRootParenOpened_{};
    NaturalNode& activeRow();
    const NaturalNode& activeRow() const;
    bool enterChild(std::size_t child);
    bool leaveChild(bool toRight);
};
}
