// The editor, the parser, the evaluator and the renderer share one AST type.
#include "expression.hpp"
#include "math_renderer.hpp"
#include "natural_editor.hpp"
#include "test_util.hpp"
#include <cmath>
#include <stdexcept>

int main() {
    using fx::ast::Kind;
    // Parser creates typed nodes (not a serialized string wrapper).
    auto parsed = fx::parseExpression("(1+2)*3^2");
    CHECK(parsed && parsed->kind == Kind::Binary && parsed->text == "*");
    CHECK(parsed->children[0]->kind == Kind::Group);
    CHECK(parsed->children[1]->kind == Kind::Binary);
    CHECK_NEAR(fx::evaluate(*parsed), 27.0, 1e-12);
    CHECK(!fx::ast::serialize(*parsed).empty());
    // Incomplete template nodes are rejected.
    auto incomplete = fx::ast::make(Kind::Fraction);
    CHECK_THROWS_AS(fx::evaluate(*incomplete), std::runtime_error);
    // Editor imports text into structured template boxes.
    fx::NaturalEditor editor;
    editor.setExpression("sqrt(16)+x^2");
    CHECK(editor.root().kind == Kind::Row);
    CHECK(editor.root().children.size() == 3);
    CHECK(editor.root().children[0]->kind == Kind::Radical);
    CHECK(editor.root().children[2]->kind == Kind::Power);
    CHECK_NEAR(fx::evaluate(editor.root(), 3.0), 13.0, 1e-12);
    // Renderer consumes that exact editor tree.
    auto rendered = fx::math::layout(editor.root(), 2, 20, 300, 100);
    CHECK(!rendered.ops.empty() && rendered.size.width > 0);
    // Fraction nodes are directly evaluable.
    auto row = fx::ast::make(Kind::Row);
    auto frac = fx::ast::make(Kind::Fraction);
    auto numerator = fx::ast::make(Kind::Row), denominator = fx::ast::make(Kind::Row);
    numerator->children.push_back(fx::ast::make(Kind::Token, "1"));
    denominator->children.push_back(fx::ast::make(Kind::Token, "2"));
    frac->children = {numerator, denominator};
    row->children.push_back(frac);
    CHECK_NEAR(fx::evaluate(*row), 0.5, 1e-12);
    return testutil::finish("vpam_ast_smoke");
}
