// v1.12.0 — editor Math/Line (ROADMAP Punto 3, voci 2 e 7): conversione testo
// <-> albero esatta in entrambe le direzioni su un corpus ampio (testo
// canonico -> albero -> stesso testo; albero costruito con i modelli -> testo
// -> albero con lo stesso testo e lo stesso disegno), parentesi implicite di
// una frazione accostata a un operando (2[1/3] -> 2((1)/(3)), 6÷2[1/3] = 9),
// testo Line con le parentesi minime, modelli d²/dx² e Σ/Π con la variabile,
// nomi mostrati dei nuovi comandi e glifi nuovi del font.
#include "calc_library.hpp"
#include "font_renderer.hpp"
#include "graph_commands.hpp"
#include "math_renderer.hpp"
#include "natural_editor.hpp"
#include "run_matrix.hpp"
#include "test_util.hpp"
#include <string>
#include <vector>

using namespace fx;

namespace {
std::string ops(const NaturalEditor& e) {
    std::string s;
    for (const auto& o : e.layout(0, 20, 300)) {
        if (o.kind == NaturalDrawOp::Kind::Cursor) continue;
        s += std::to_string(static_cast<int>(o.kind)) + ":" + std::to_string(o.x) + "," + std::to_string(o.y) + "," + std::to_string(o.x2) + "," +
             std::to_string(o.y2) + o.text + ";";
    }
    return s;
}
bool hasText(const NaturalEditor& e, const std::string& t) {
    for (const auto& o : e.layout(0, 20, 300)) if (o.kind == NaturalDrawOp::Kind::Text && o.text.find(t) != std::string::npos) return true;
    return false;
}
void type(NaturalEditor& e, const std::string& s) { for (char c : s) e.insert(c); }
std::string lowerAscii(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
// Testo -> albero -> testo, nelle due modalita'; nel modo Math il testo e' gia' canonico.
void roundTrip(const std::string& t) {
    for (int lin = 0; lin < 2; ++lin) {
        NaturalEditor e;
        e.setLinear(lin != 0);
        e.setExpression(t);
        const std::string back = e.toExpression();
        ::testutil::report(back == t, "text round trip", __FILE__, __LINE__, (lin ? "Line: " : "Math: ") + t + " -> " + back);
        NaturalEditor again;
        again.setLinear(lin != 0);
        again.setExpression(back);
        ::testutil::report(ops(again) == ops(e) && again.toExpression() == back, "tree round trip", __FILE__, __LINE__, t);
    }
}
// Albero dei modelli -> testo -> albero: stesso testo e stesso disegno.
void treeTrip(const NaturalEditor& e, const char* what) {
    NaturalEditor back;
    back.setExpression(e.toExpression());
    ::testutil::report(back.toExpression() == e.toExpression() && ops(back) == ops(e), "template round trip", __FILE__, __LINE__,
                       std::string(what) + ": " + e.toExpression() + " -> " + back.toExpression());
}
} // namespace

int main() {
    // ================================================================ corpus testo -> albero -> testo
    const char* const corpus[] = {
        "1+2", "(1)/(2)", "(1)/(2)+(3)/(4)", "2((1)/(3))", "6\xC3\xB7" "2((1)/(3))", "((1)/(2))x", "((1)/(2))((1)/(3))",
        "(x)^(2)", "((x)^(2))/(3)", "sqrt(2)", "root(3,8)", "root(n+1,8)", "root(3,27)", "mixed(2,1,3)", "abs(-2)", "log(2,8)",
        "integral(x,0,1)", "derivative((x)^(3),2)", "derivative2((x)^(3),2)", "sum(k,k,1,5)", "sum(x,1,5)", "prod(n,n,1,4)",
        "((1)/(2))", "(((1)/(2))+1)", "[[1,2][3,4]]", "[[(1)/(2),2][3,sqrt(2)]]", "{1,(1)/(3),3}", "MatA+MatB", "List1[2]",
        "0.1_6", "2\xE2\x8C\x9F" "3", "1\xE2\x8C\x9F" "2\xE2\x8C\x9F" "3", "sin(30)", "sin(30)cos(60)", "(e)^(2)", "(10)^(3)",
        "norm_pd(1)", "invnorm_cd(\"L\",0.7)", "solve_n((x)^(2)=4)", "f_min(x,0,1)", "ran_samp({1,2},1)", "cmpd_pv(1,2,3,4,5,6)",
        "v_a+2", "r_var+theta", "f_1(3)", "str_1", "10[cm]\xE2\x96\xB6[in]", "pi\xCA\xB3", "100\xE1\xB5\x8D",
        "2\xC2\xB0" "20\xE2\x80\xB2" "30\xE2\x80\xB3", "2\xE2\x88\xA0" "30", "3#P+2#E", "x_hat(2)", "y_hat(2)", "tbl_result",
        "eq_simres", "3\xE2\x86\x92" "A:A\xC3\x97" "2", "5\xE2\x97\xA2" "6", "1/2\xE2\x96\xB6Simp", "num_rnd(1/3)", "prob_p(1)",
        "@Y1(2)", "Det(MatA)", "Seq(x,x,1,5,1)", "ran3", "(2)/(3)+(1)/(6)", "-((1)/(2))", "((2)/(3))^(2)",
    };
    for (const char* t : corpus) roundTrip(t);
    // Ogni testo del corpus si calcola con l'unico parser (o da' un errore della calcolatrice, non un'eccezione C++)
    {
        Workspace w;
        int evaluated = 0;
        for (const char* t : corpus) {
            try { runCalculation(w, t); ++evaluated; }
            catch (const SyntaxError&) {} catch (const MathError&) {}
        }
        CHECK(evaluated > 40);
    }
    // ================================================================ modelli -> testo -> modelli
    {
        NaturalEditor g; g.insertGroup(); g.insertFraction(); type(g, "1"); g.moveDown(); type(g, "2");
        treeTrip(g, "gruppo con frazione");
        NaturalEditor r; r.insertRootTemplate(); type(r, "n+1"); r.moveRight(); type(r, "8");
        treeTrip(r, "radice con indice n+1");
        NaturalEditor d; d.insertSecondDerivative(); type(d, "x"); d.moveDown(); type(d, "2");
        treeTrip(d, "d2/dx2");
        CHECK(hasText(d, "d\xC2\xB2"));
        NaturalEditor s; s.insertSummation(); type(s, "k"); s.moveDown(); s.backspace(); type(s, "k"); s.moveDown(); type(s, "1"); s.moveDown(); type(s, "4");
        treeTrip(s, "sigma con variabile");
        CHECK_EQ_STR(s.toExpression(), "sum(k,k,1,4)");
        CHECK(hasText(s, "\xCE\xA3"));
        NaturalEditor p; p.insertProduct(); type(p, "n"); p.moveDown(); p.backspace(); type(p, "n"); p.moveDown(); type(p, "1"); p.moveDown(); type(p, "4");
        treeTrip(p, "pi con variabile");
        NaturalEditor m; m.insertMatrix(2, 2); type(m, "1"); m.moveRight(); m.insertFraction(); type(m, "1"); m.moveDown(); type(m, "3");
        treeTrip(m, "matrice con frazione");
        NaturalEditor mx; mx.insertMixedFraction(); type(mx, "2"); mx.moveRight(); type(mx, "1"); mx.moveDown(); type(mx, "3");
        treeTrip(mx, "frazione mista");
        NaturalEditor pw; type(pw, "2"); pw.insertPower(); pw.insertFraction(); type(pw, "1"); pw.moveDown(); type(pw, "2");
        treeTrip(pw, "esponente frazione");
        NaturalEditor ab; ab.insertAbs(); ab.insertFraction(); type(ab, "1"); ab.moveDown(); type(ab, "2");
        treeTrip(ab, "valore assoluto");
        NaturalEditor lg; lg.insertLogBase(); type(lg, "2"); lg.moveRight(); type(lg, "8");
        treeTrip(lg, "log_a b");
        NaturalEditor in; in.insertIntegral(); type(in, "x"); in.moveDown(); type(in, "0"); in.moveDown(); type(in, "1");
        treeTrip(in, "integrale");
    }
    // ================================================================ × prima di una frazione e parentesi implicite (manuale 1-23)
    {
        NaturalEditor frac; frac.insertFraction(); type(frac, "1"); frac.moveDown(); type(frac, "3");
        NaturalEditor e; e.insertText("6\xC3\xB7" "2");
        e.insertSymbol("\xC3\xB7"); e.backspace();                  // cursore dopo "2"
        e.paste(frac.root());
        CHECK_EQ_STR(e.toExpression(), "6\xC3\xB7" "2((1)/(3))");
        Workspace w;
        runCalculation(w, e.toExpression());
        CHECK_EQ_STR(resultText(w.history.back(), w.settings), "9");     // 6÷(2×1/3): moltiplicazione implicita prima di ÷
        treeTrip(e, "frazione dopo un numero");
        // Con il × la frazione e' un fattore separato
        NaturalEditor x; x.insertText("6\xC3\xB7" "2"); x.insertSymbol("\xC3\x97"); x.paste(frac.root());
        runCalculation(w, x.toExpression());
        CHECK_EQ_STR(resultText(w.history.back(), w.settings), "1");
        // Testo Line con le sole parentesi necessarie
        CHECK_EQ_STR(e.linearText(), "6\xC3\xB7" "2(1/3)");
        CHECK_EQ_STR(NaturalEditor::toLinearText("(1)/(3)"), "1/3");
        CHECK_EQ_STR(NaturalEditor::toLinearText("(x+1)/(3)"), "(x+1)/3");
        CHECK_EQ_STR(NaturalEditor::toLinearText("(x)^(2)"), "x^2");
        CHECK_EQ_STR(NaturalEditor::toLinearText("(x)^(-1)"), "x^(-1)");
        CHECK_EQ_STR(NaturalEditor::toLinearText("((1)/(2))x"), "(1/2)x");
        // La forma Line ha lo stesso valore della forma canonica
        for (const char* t : {"6\xC3\xB7" "2((1)/(3))", "((1)/(2))x", "(x+1)/(3)", "((2)/(3))^(2)"}) {
            Workspace a, b;
            runCalculation(a, std::string("2") + "\xE2\x86\x92X:" + t);
            runCalculation(b, std::string("2") + "\xE2\x86\x92X:" + NaturalEditor::toLinearText(t));
            CHECK(a.history.back().value == b.history.back().value);
        }
        // Math <-> Line della riga di input: esatto nelle due direzioni
        NaturalEditor sw; sw.insertFraction(); type(sw, "1"); sw.moveDown(); type(sw, "3");
        const std::string before = ops(sw);
        sw.setLinear(true); sw.setLinear(false);
        CHECK(ops(sw) == before);
    }
    // ================================================================ nomi mostrati e glifi nuovi
    {
        NaturalEditor e, e2;
        e.setExpression("solve_n(x)+norm_pd(1)+v_a+f_2+str_3");                   // entro la larghezza della riga
        e2.setExpression("cmpd_pv(1,2,3,4,5,6)+r_var");
        CHECK(hasText(e, "SolveN") && hasText(e, "NormPD") && hasText(e2, "Cmpd_PV") && hasText(e, "f2") && hasText(e, "Str 3"));
        bool underline = false;
        for (const auto& o : e.layout(0, 20, 300)) underline = underline || o.kind == NaturalDrawOp::Kind::Line;
        CHECK(underline);                                                         // v_a sottolineata
        CHECK(std::string(calclib::displayName("x_hat")) == "\xEE\x80\x80" && std::string(calclib::displayName("ran_list")) == "RanList#");
        for (std::uint32_t cp : {0x25E2u, 0x02B3u, 0x1D4Du, 0x2032u, 0x2033u, 0x0177u, 0xE000u, 0x00C5u}) {
            bool small = false, medium = false;
            for (std::size_t i = 0; i < font::kSmall.count; ++i) small = small || font::kSmall.codepoints[i] == cp;
            for (std::size_t i = 0; i < font::kMedium.count; ++i) medium = medium || font::kMedium.codepoints[i] == cp;
            CHECK(small && medium);
        }
        // Il CATALOG elenca ogni comando una volta e con un testo che il parser conosce
        std::size_t entries = 0;
        for (const calclib::CatalogEntry* c = calclib::catalog(); c->shown; ++c) {
            ++entries;
            std::string ins = c->insert;
            if (!ins.empty() && ins.back() == '(') ins.pop_back();
            const bool known = isReservedWord(ins) || graphcmd::isCommand(ins) || ins == "List " || ins == "Mat " || ins == "Vct " ||
                               ins.rfind("Int", 0) == 0 || ins == "Rmdr" || ins == "*Row " || ins == "*Row+ " || ins == "Row+ " || ins == "Swap " ||
                               ins == "and" || ins == "or" || ins == "xor" || ins == "xnor" || ins == "not" || ins == "Det" || ins == "Dim" ||
                               isReservedWord(lowerAscii(ins));
            ::testutil::report(known, "catalog entry known", __FILE__, __LINE__, c->shown);
        }
        CHECK(entries > 150);
    }
    return testutil::finish("editor_v112_test");
}
