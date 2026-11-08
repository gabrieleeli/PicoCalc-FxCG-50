// Natural V.P.A.M. editor + 2D math layout tests.
#include "expression.hpp"
#include "math_engine.hpp"
#include "math_renderer.hpp"
#include "natural_editor.hpp"
#include "test_util.hpp"
#include <cmath>
#include <string>

using namespace fx;

static void type(NaturalEditor& e, const std::string& s) { for (char c : s) e.insert(c); }
static double value(const NaturalEditor& e) { return evaluate(e.toExpression()); }
static bool hasText(const std::vector<NaturalDrawOp>& ops, const std::string& t) {
    for (const auto& op : ops) if (op.kind == NaturalDrawOp::Kind::Text && op.text == t) return true;
    return false;
}
static int cursorX(const std::vector<NaturalDrawOp>& ops) {
    for (const auto& op : ops) if (op.kind == NaturalDrawOp::Kind::Cursor) return op.x;
    return -1;
}

int main() {
    NaturalEditor ne;
    // --- templates (ported from the v1.5.4 suite)
    ne.insertFraction(); ne.insert('1'); CHECK(ne.moveDown()); ne.insert('3');
    CHECK_EQ_STR(ne.toExpression(), "(1)/(3)"); CHECK_NEAR(value(ne), 1.0 / 3.0, 1e-12);
    ne.clear(); ne.insertRadical(); ne.insert('9'); CHECK_EQ_STR(ne.toExpression(), "sqrt(9)"); CHECK(value(ne) == 3);
    ne.clear(); ne.insert('2'); ne.insertPower(); ne.insert('3'); CHECK_EQ_STR(ne.toExpression(), "(2)^(3)"); CHECK(value(ne) == 8); CHECK(ne.moveUp());
    ne.clear(); type(ne, "2^3"); CHECK_EQ_STR(ne.toExpression(), "(2)^(3)");
    ne.clear(); type(ne, "sqrt"); CHECK_EQ_STR(ne.toExpression(), "sqrt()"); ne.insert('9'); CHECK_EQ_STR(ne.toExpression(), "sqrt(9)");
    ne.clear(); type(ne, "cbrt27"); CHECK_EQ_STR(ne.toExpression(), "root(3,27)"); CHECK(value(ne) == 3);
    ne.clear(); type(ne, "root532"); CHECK_EQ_STR(ne.toExpression(), "root(5,32)"); CHECK(value(ne) == 2);
    ne.clear(); type(ne, "root1010"); CHECK_EQ_STR(ne.toExpression(), "root(10,10)");
    ne.clear(); type(ne, "sqrt(3)"); CHECK_EQ_STR(ne.toExpression(), "sqrt(3)");
    ne.clear(); type(ne, "root3(27)"); CHECK_EQ_STR(ne.toExpression(), "root(3,27)");
    ne.clear(); type(ne, "2sqrt9"); CHECK_EQ_STR(ne.toExpression(), "2sqrt(9)"); CHECK(value(ne) == 6);
    ne.clear(); type(ne, "sin(30)"); setAngleMode(AngleMode::Degrees); CHECK_EQ_STR(ne.toExpression(), "sin(30)"); CHECK_NEAR(value(ne), 0.5, 1e-12); setAngleMode(AngleMode::Radians);
    ne.clear(); ne.insertIntegral(); type(ne, "x^2"); CHECK(ne.moveRight()); CHECK(ne.moveDown()); ne.insert('0'); CHECK(ne.moveDown()); ne.insert('3');
    CHECK_EQ_STR(ne.toExpression(), "integral((x)^(2),0,3)"); CHECK_NEAR(value(ne), 9, 1e-9);
    CHECK(hasText(ne.layout(0, 30, 300), "∫"));
    ne.clear(); ne.insertDerivative(); type(ne, "x^2"); CHECK(ne.moveRight()); CHECK(ne.moveDown()); ne.insert('3');
    CHECK_EQ_STR(ne.toExpression(), "derivative((x)^(2),3)"); CHECK_NEAR(value(ne), 6, 1e-7);
    ne.clear(); ne.insertSummation(); ne.insert('x'); CHECK(ne.moveDown()); ne.insert('1'); CHECK(ne.moveDown()); ne.insert('5');
    CHECK_EQ_STR(ne.toExpression(), "sum(x,1,5)"); CHECK(value(ne) == 15);
    ne.clear(); type(ne, "alpha"); CHECK(hasText(ne.layout(4, 20, 300), "α")); CHECK_EQ_STR(ne.toExpression(), "alpha");
    ne.clear(); type(ne, "12"); ne.insertPower(); ne.insert('2'); CHECK_EQ_STR(ne.toExpression(), "(12)^(2)"); CHECK(value(ne) == 144);
    // fx-CG50 behaviour: an operand before the fraction key becomes the numerator, cursor goes to the denominator.
    ne.clear(); type(ne, "12"); ne.insertFraction(); CHECK_EQ_STR(ne.toExpression(), "(12)/()"); ne.insert('3'); CHECK_EQ_STR(ne.toExpression(), "(12)/(3)"); CHECK(value(ne) == 4);
    ne.clear(); ne.insertFraction(); ne.insert('1'); CHECK(ne.moveDown()); ne.insert('3'); CHECK(ne.moveUp()); type(ne, "+1"); CHECK(ne.moveDown());
    CHECK_EQ_STR(ne.toExpression(), "(1+1)/(3)");
    ne.clear(); ne.handle({NaturalKey::Character, '4'}); ne.handle({NaturalKey::Character, '2'}); CHECK_EQ_STR(ne.toExpression(), "42");
    ne.handle({NaturalKey::Backspace, 0}); CHECK_EQ_STR(ne.toExpression(), "4");
    ne.clear(); ne.insertGroup(); type(ne, "5+1"); CHECK_EQ_STR(ne.toExpression(), "(5+1)"); CHECK(value(ne) == 6);
    ne.clear(); ne.insert('x'); ne.insertSubscript(); ne.insert('1'); CHECK_THROWS_AS(ne.toExpression(), std::runtime_error);
    // --- new templates
    ne.clear(); ne.insertAbs(); type(ne, "-3"); CHECK_EQ_STR(ne.toExpression(), "abs(-3)"); CHECK(value(ne) == 3);
    ne.clear(); ne.insertLogBase(); ne.insert('2'); CHECK(ne.moveRight()); ne.insert('8'); CHECK_EQ_STR(ne.toExpression(), "log(2,8)"); CHECK_NEAR(value(ne), 3, 1e-12);
    ne.clear(); ne.insertExp(); ne.insert('0'); CHECK_EQ_STR(ne.toExpression(), "(e)^(0)"); CHECK(value(ne) == 1);
    ne.clear(); ne.insertTenPower(); ne.insert('3'); CHECK(value(ne) == 1000);
    ne.clear(); ne.insert('5'); ne.insertSquare(); ne.insert('+'); ne.insert('1'); CHECK_EQ_STR(ne.toExpression(), "(5)^(2)+1");
    ne.clear(); ne.insertProduct(); ne.insert('x'); ne.moveDown(); ne.insert('1'); ne.moveDown(); ne.insert('4'); CHECK(value(ne) == 24);
    // x^ without an operand creates an empty base box (CG50 behaviour).
    ne.clear(); ne.insert('1'); ne.insert('+'); ne.insertPower(); ne.insert('2'); ne.moveDown(); ne.insert('3');
    CHECK_EQ_STR(ne.toExpression(), "1+(2)^(3)"); CHECK(value(ne) == 9);
    // --- word tokens: one cursor step / one delete per function name
    ne.clear(); type(ne, "sin"); CHECK(ne.cursorIndex() == 1); CHECK(ne.root().children.size() == 1);
    ne.insert('h'); CHECK(ne.root().children.size() == 1); CHECK_EQ_STR(ne.root().children[0]->text, "sinh");
    ne.clear(); type(ne, "2pi"); CHECK(ne.root().children.size() == 2); CHECK(hasText(ne.layout(0, 20, 300), "π"));
    ne.backspace(); CHECK_EQ_STR(ne.toExpression(), "2");
    ne.clear(); type(ne, "Ans"); CHECK(ne.root().children.size() == 1);
    // --- cursor uses the renderer metrics (8 px per Medium glyph)
    ne.clear(); type(ne, "123");
    CHECK(cursorX(ne.layout(10, 30, 300)) == 10 + 3 * 8);
    ne.moveLeft(); CHECK(cursorX(ne.layout(10, 30, 300)) == 10 + 2 * 8);
    ne.clear(); type(ne, "2*pi"); CHECK(cursorX(ne.layout(0, 30, 300)) == 3 * 8);   // "2×π" = 3 glyphs
    // --- navigation enters templates like the CG50
    ne.clear(); ne.insertFraction(); ne.insert('1'); ne.moveDown(); ne.insert('2'); ne.moveRight();   // leave fraction
    ne.insert('+'); ne.moveLeft(); ne.moveLeft();          // into the denominator from the right
    ne.insert('3'); CHECK_EQ_STR(ne.toExpression(), "(1)/(23)+");
    ne.clear(); ne.insertFraction(); ne.insert('1'); ne.moveDown(); ne.insert('4'); ne.moveHome(); ne.moveRight(); ne.insert('9');
    CHECK_EQ_STR(ne.toExpression(), "(91)/(4)");
    // --- backspace: empty template is removed, a non-empty one is entered
    ne.clear(); ne.insertFraction(); ne.backspace(); CHECK(ne.empty());
    ne.clear(); type(ne, "7"); ne.insertPower(); ne.backspace(); CHECK_EQ_STR(ne.toExpression(), "7");
    ne.clear(); ne.insertRadical(); ne.insert('4'); ne.moveRight(); ne.backspace(); CHECK_EQ_STR(ne.toExpression(), "sqrt(4)");
    // --- undo / redo
    ne.clear(); type(ne, "12"); CHECK(ne.undo()); CHECK_EQ_STR(ne.toExpression(), "1"); CHECK(ne.redo()); CHECK_EQ_STR(ne.toExpression(), "12");
    ne.clear(); CHECK(ne.empty()); CHECK(ne.undo()); CHECK_EQ_STR(ne.toExpression(), "12");
    // --- clipboard
    auto clip = ne.copyTree(); ne.clear(); ne.paste(*clip); ne.insert('+'); ne.paste(*clip);
    CHECK_EQ_STR(ne.toExpression(), "12+12");
    // --- structural import (history recall / stored functions) round-trips
    const char* samples[] = {"(1)/(3)", "sqrt(9)+1", "root(5,32)", "(x)^(2)+(2)^(x)", "integral((x)^(2),0,3)",
        "derivative(sin(x),0)", "sum(x,1,10)", "abs(-2)", "log(2,8)", "((1)/(2))^(2)", "sin(x)^2", "2pi",
        "(2+3)^(2)", "sinh(1)", "Ans*2", "prod(x,1,4)"};
    for (const char* s : samples) {
        NaturalEditor a; a.setExpression(s);
        const std::string first = a.toExpression();
        NaturalEditor b; b.setExpression(first);
        CHECK_EQ_STR(b.toExpression(), first);
        double va = 0, vb = 0; bool okA = true, okB = true;
        try { CalculatorMemory m; m.setAnswer(3); EvalContext c; c.memory = &m; c.bind("x", 0.7); va = evaluateReal(*parseExpression(s), c); } catch (...) { okA = false; }
        try { CalculatorMemory m; m.setAnswer(3); EvalContext c; c.memory = &m; c.bind("x", 0.7); vb = evaluateReal(*parseExpression(first), c); } catch (...) { okB = false; }
        CHECK(okA == okB);
        if (okA && okB) CHECK_NEAR(va, vb, 1e-9 * std::max(1.0, std::abs(va)));
    }
    ne.setExpression("(1)/(3)"); CHECK(ne.root().children.size() == 1 && ne.root().children[0]->kind == ast::Kind::Fraction);
    ne.setExpression("x^2+1"); CHECK_EQ_STR(ne.toExpression(), "(x)^(2)+1");
    ne.setExpression("2^-3"); CHECK_NEAR(value(ne), 0.125, 1e-12);
    // --- renderer: fraction rule, raised exponent, root index, clipping, alignment
    {
        NaturalEditor fr; fr.insertFraction(); type(fr, "x+1"); fr.moveDown(); fr.insert('2');
        const auto m = math::measure(fr.root());
        CHECK(m.width > 0 && m.height > 16);
        auto l = math::layout(fr.root(), 10, 100, 200, 180);
        bool rule = false; for (const auto& o : l.ops) if (o.kind == NaturalDrawOp::Kind::Line) rule = true;
        CHECK(rule && !l.clipped);
        CHECK(math::layout(fr.root(), 0, 0, 1, 1).clipped);
        CHECK(math::layout(fr.root(), 0, 100, 200, 180, math::Align::Center).x == (200 - m.width) / 2);
        CHECK(math::layout(fr.root(), 0, 100, 200, 180, math::Align::Right).x == 200 - m.width);
        auto narrow = math::layout(fr.root(), 0, 100, 8, 180);
        CHECK(narrow.clipped);
        for (const auto& o : narrow.ops) {
            if (o.kind == NaturalDrawOp::Kind::Text) CHECK(o.x >= 0 && o.x + static_cast<int>(o.text.size()) * 8 <= 8);
            if (o.kind == NaturalDrawOp::Kind::Line) CHECK(o.x >= 0 && o.x2 <= 8);
        }
    }
    {
        NaturalEditor ex; type(ex, "x^3");
        auto l = math::layout(ex.root(), 0, 30, 200, 50);
        bool raised = false; for (const auto& o : l.ops) if (o.kind == NaturalDrawOp::Kind::Text && o.text == "3" && o.y < 30 - 14 && o.face == math::FaceSmall) raised = true;
        CHECK(raised);
        NaturalEditor r5; type(r5, "root532"); CHECK(hasText(math::layout(r5.root(), 0, 30, 200, 50).ops, "5"));
        auto empty = ast::make(ast::Kind::Fraction); CHECK(math::measure(*empty).width == 0);
        // Empty template slots get a placeholder box.
        NaturalEditor ph; ph.insertFraction();
        int placeholders = 0; for (const auto& o : ph.layout(0, 40, 200)) if (o.kind == NaturalDrawOp::Kind::Placeholder) ++placeholders;
        CHECK(placeholders == 2);
    }
    // --- input limits (bounded recursion and memory on the RP2350)
    {
        auto errorStarts = [](auto&& action, const char* prefix) {
            try { action(); } catch (const MathError& e) { return std::string(e.what()).rfind(prefix, 0) == 0; } catch (...) { return false; }
            return false;
        };
        NaturalEditor deep;
        for (std::size_t i = 0; i < NaturalEditor::MaxTemplateDepth; ++i) deep.insertFraction();
        CHECK(deep.cursorPath().size() == 2 * NaturalEditor::MaxTemplateDepth);
        const std::string before = deep.toExpression();
        CHECK(errorStarts([&] { deep.insertFraction(); }, "Stack ERROR"));
        CHECK(errorStarts([&] { deep.insertRadical(); }, "Stack ERROR"));
        CHECK_EQ_STR(deep.toExpression(), before);                  // the refused edit was rolled back
        CHECK(deep.cursorPath().size() == 2 * NaturalEditor::MaxTemplateDepth);
        deep.insert('1');                                           // plain input still works at the limit
        CHECK(deep.toExpression() != before);
        // A power that takes a nested operand also counts the operand's depth.
        NaturalEditor powDeep;
        for (std::size_t i = 0; i < NaturalEditor::MaxTemplateDepth; ++i) powDeep.insertRadical();
        powDeep.insert('2');
        powDeep.moveEnd();
        CHECK(errorStarts([&] { powDeep.insertPower(); }, "Stack ERROR"));
        NaturalEditor longInput;
        bool full = false;
        for (std::size_t i = 0; i < NaturalEditor::MaxNodes + 10 && !full; ++i)
            full = errorStarts([&] { longInput.insert(i % 2 ? '+' : '1'); }, "Memory ERROR");
        CHECK(full);
        const std::string longText = longInput.toExpression();
        CHECK(longText.size() == NaturalEditor::MaxNodes - 1);       // root row + one node per character
        CHECK(errorStarts([&] { longInput.insertSymbol("pi"); }, "Memory ERROR"));
        CHECK_EQ_STR(longInput.toExpression(), longText);
        CHECK(longInput.undo());                                    // history still consistent after refusals
        CHECK(longInput.toExpression().size() == longText.size() - 1);
        NaturalEditor paster; type(paster, "12+34");
        bool pasteRefused = false;
        for (int i = 0; i < 12 && !pasteRefused; ++i) { auto copy = paster.copyTree(); pasteRefused = errorStarts([&] { paster.paste(*copy); }, "Memory ERROR"); }
        CHECK(pasteRefused);
        NaturalEditor imported;
        std::string nested; for (int i = 0; i < 13; ++i) nested += "sqrt("; nested += "2"; for (int i = 0; i < 13; ++i) nested += ")";
        CHECK(errorStarts([&] { imported.setExpression(nested); }, "Stack ERROR"));
        CHECK_NOTHROW(imported.setExpression("sqrt(sqrt(2))+(1)/(3)"));
    }
    // v1.7.0 templates: mixed fraction, repeating digits, root with an editable index.
    {
        NaturalEditor m;
        m.insertMixedFraction(); type(m, "2"); m.moveRight(); type(m, "1"); m.moveDown(); type(m, "3");
        CHECK_EQ_STR(m.toExpression(), "mixed(2,1,3)");
        CHECK_NEAR(value(m), 7.0 / 3.0, 1e-15);
        CHECK(m.moveUp() && m.cursorPath().back() == 1);                 // denominator -> numerator
        NaturalEditor back; back.setExpression(m.toExpression());
        CHECK_EQ_STR(back.toExpression(), "mixed(2,1,3)");
        CHECK(math::measure(back.root()).height > 20);                   // drawn as a stacked fraction
        NaturalEditor p; type(p, "0.1"); p.insertPeriod(); type(p, "6");
        CHECK_EQ_STR(p.toExpression(), "0.1_6");
        CHECK_NEAR(value(p), 1.0 / 6.0, 1e-15);
        NaturalEditor pBack; pBack.setExpression("2+0.1_6");
        CHECK_EQ_STR(pBack.toExpression(), "2+0.1_6");
        NaturalEditor noPoint; type(noPoint, "12");
        CHECK_THROWS_AS(noPoint.insertPeriod(), SyntaxError);
        NaturalEditor r; r.insertRootTemplate(); type(r, "3"); r.moveRight(); type(r, "8");
        CHECK_EQ_STR(r.toExpression(), "root(3,8)");
        CHECK_NEAR(value(r), 2.0, 1e-15);
        NaturalEditor rel; type(rel, "2<=3");
        CHECK_NEAR(value(rel), 1.0, 0);
        NaturalEditor range; type(range, "1->A~C");                      // '~' accepted for A~F stores
        CHECK_EQ_STR(range.toExpression(), "1->A~C");
    }
    return testutil::finish("editor_test");
}
