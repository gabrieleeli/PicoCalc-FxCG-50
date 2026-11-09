#pragma once
#include "app.hpp"
#include "line_editor.hpp"
#include <functional>
#include <string>
#include <vector>

namespace fx::app {

// Evaluates a numeric entry with the workspace memory (A-Z, Ans) in Real mode.
double evalNumber(Context& ctx, const std::string& text);
std::string fmt(Context& ctx, double v);

// Vertical form of labelled fields (V-Window, DIST/TEST parameters, settings).
struct FormField {
    std::string label;
    std::string text;                    // numeric expression or free text
    enum class Type { Number, Text, Choice, Secret } type{Type::Number};
    std::vector<std::string> choices;    // Choice fields
    int choice{0};
};
class Form {
public:
    std::vector<FormField> fields;
    int selected{0};
    bool editing() const noexcept { return editing_; }
    // Up/Down select, Left/Right change choices, characters or EXE start editing,
    // EXE confirms (numbers are validated by evaluation). Returns true if consumed.
    bool key(Context& ctx, const KeyEvent& ev);
    void draw(gfx::Canvas& c, int top, int bottom) const;
    double number(Context& ctx, std::size_t i) const { return evalNumber(ctx, fields.at(i).text); }
private:
    bool editing_{false};
    LineEditor editor_;
    mutable int first_{0};
};

// Scrollable "label = value" result screen.
class ResultList {
public:
    std::string title;
    std::vector<std::pair<std::string, std::string>> rows;
    void clear() { rows.clear(); first_ = 0; }
    void add(std::string label, std::string value) { rows.emplace_back(std::move(label), std::move(value)); }
    bool key(const KeyEvent& ev);
    void draw(gfx::Canvas& c, int top, int bottom) const;
private:
    int first_{0};
};

// Numeric cell grid with column headers and an edit line at the bottom.
class CellGrid {
public:
    int rows{1}, cols{1};
    int visibleCols{4};
    bool growRows{false};                 // statistics lists grow when typing below the last row
    bool rowMajor{false};                 // EXE advances to the next column (coefficient entry)
    std::function<int(int)> columnLength; // optional: cursor may not go past the first empty cell
    std::function<std::string(int)> header;
    std::function<bool(int, int, double&)> get;     // false = empty cell
    std::function<void(int, int, double)> set;
    std::function<void(int, int)> erase;
    int row{0}, col{0};
    bool editing() const noexcept { return editing_; }
    bool key(Context& ctx, const KeyEvent& ev);
    void draw(Context& ctx, gfx::Canvas& c, int top, int bottom) const;
private:
    bool editing_{false};
    LineEditor editor_;
    mutable int firstRow_{0}, firstCol_{0};
};

} // namespace fx::app
