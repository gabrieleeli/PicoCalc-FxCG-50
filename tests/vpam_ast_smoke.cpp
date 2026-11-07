#include "expression.hpp"
#include "natural_editor.hpp"
#include "math_renderer.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    using fx::ast::Kind;
    // Parser creates typed nodes (not a serialized string wrapper).
    auto parsed = fx::parseExpression("(1+2)*3^2");
    assert(parsed && parsed->kind == Kind::Binary && parsed->text == "*");
    assert(parsed->children[0]->kind == Kind::Group);
    assert(parsed->children[1]->kind == Kind::Binary);
    assert(std::abs(fx::evaluate(*parsed) - 27.0) < 1e-12);
    assert(!fx::ast::serialize(*parsed).empty());

    auto incomplete = fx::ast::make(Kind::Fraction);
    bool rejected = false;
    try { (void)fx::evaluate(*incomplete); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);

    // Editor imports the parsed tree into its editable shared AST.
    fx::NaturalEditor editor;
    editor.setExpression("sqrt(16)+x^2");
    assert(editor.root().kind == Kind::Row);
    assert(editor.root().children.size() == 1);
    assert(std::abs(fx::evaluate(editor.root(), 3.0) - 13.0) < 1e-12);

    // Renderer consumes that exact editor tree and emits drawing operations.
    auto rendered = fx::math::layout(editor.root(), 2, 20, 300, 100);
    assert(!rendered.ops.empty());
    assert(rendered.size.width > 0);

    // Fraction/power nodes remain directly evaluable without serialize().
    auto row = fx::ast::make(Kind::Row);
    auto frac = fx::ast::make(Kind::Fraction);
    auto numerator = fx::ast::make(Kind::Row);
    auto denominator = fx::ast::make(Kind::Row);
    numerator->children.push_back(fx::ast::make(Kind::Token, "1"));
    denominator->children.push_back(fx::ast::make(Kind::Token, "2"));
    frac->children = {numerator, denominator};
    row->children.push_back(frac);
    assert(std::abs(fx::evaluate(*row) - 0.5) < 1e-12);
    std::cout << "VPAM AST end-to-end tests: PASS\n";
}
