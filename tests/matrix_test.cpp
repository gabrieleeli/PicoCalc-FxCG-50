// v1.10.0 — matrici, vettori e liste come valori del parser e dell'AST:
// parsing, valutazione (numerica ed esatta), controllo di dimensioni e tipi,
// posizione degli errori, memorie Mat/Vct/List del workspace, nucleo di
// Run-Matrix (memorizzazione, risultati, cronologia) e formato del record v6
// con la lettura dei record delle versioni precedenti.
#include "run_matrix.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <cmath>
#include <functional>
#include <string>
#include <vector>

using namespace fx;

namespace {
const std::uint8_t kV190Record[] = {
#include "fixtures/workspace_v1_9_0.inc"
};
const std::uint8_t kV180Record[] = {
#include "fixtures/workspace_v1_8_0.inc"
};

CalcValue any(Workspace& ws, const std::string& text, ComplexMode mode = ComplexMode::Real) {
    EvalContext c = ws.evalContext();
    c.complexMode = mode;
    return evaluateAny(*parseExpression(text), c);
}
bool same(const CalcValue& v, ValueType type, std::size_t rows, std::size_t cols, const std::vector<double>& re, double tol = 1e-12) {
    if (v.type != type || v.rows != rows || v.cols != cols || v.size() != re.size() || v.complex()) return false;
    for (std::size_t i = 0; i < re.size(); ++i) if (std::abs(v.re[i] - re[i]) > tol) return false;
    return true;
}
// Errore della classe attesa e intervallo del testo indicato da EvalInfo.
template <typename E> bool failsAt(Workspace& ws, const std::string& text, int pos, int len) {
    EvalInfo info;
    EvalContext c = ws.evalContext();
    c.info = &info;
    try { (void)evaluateAny(*parseExpression(text), c); }
    catch (const E&) { return info.error.position == pos && info.error.length == len; }
    catch (...) { return false; }
    return false;
}
template <typename E> ErrorLocation entryError(Workspace& ws, const std::string& text) {
    try { (void)runCalculation(ws, text); }
    catch (const E& e) { return e.where; }
    catch (...) { return ErrorLocation{-2, 0}; }
    return ErrorLocation{-3, 0};
}
std::string last(Workspace& ws) { return resultText(ws.history.back(), ws.settings); }
} // namespace

int main() {
    setAngleMode(AngleMode::Radians);
    // ---------------------------------------------------------------- parsing
    {
        auto m = parseExpression("[[1,2][3,4]]");
        CHECK(m->kind == ast::Kind::Matrix && m->text == "2" && m->children.size() == 4 && ast::involvesCollections(*m));
        CHECK(m->pos == 0 && m->len == 12);
        auto l = parseExpression("{1,2,3}");
        CHECK(l->kind == ast::Kind::List && l->children.size() == 3 && ast::involvesCollections(*l));
        CHECK(parseExpression("Mat A")->text == "Mat A" && parseExpression("MatA")->text == "Mat A");
        CHECK(parseExpression("Mat x")->text == "Mat X");                       // fx-CG50: Mat x = Mat X
        CHECK(parseExpression("Vct B")->text == "Vct B" && parseExpression("Mat Ans")->text == "Mat Ans");
        CHECK(parseExpression("List 3")->text == "list3" && parseExpression("List12")->text == "List 12");
        CHECK(parseExpression("List Ans")->text == "List Ans");
        auto e = parseExpression("Mat A[1,2]");
        CHECK(e->kind == ast::Kind::Call && e->text == "elem" && e->children.size() == 3);
        CHECK(parseExpression("List 1[2]")->children.size() == 2);
        // Le parentesi di chiusura possono mancare alla fine, come per ( ).
        CHECK(parseExpression("[[1,2][3,4")->children.size() == 4 && parseExpression("{1,2")->children.size() == 2);
        // Un prodotto implicito con una matrice letterale non e' un indice.
        CHECK(parseExpression("Mat A[[1]]")->kind == ast::Kind::Binary);
        // mat, vct e list scritti in minuscolo restano lettere (m×a×t).
        CHECK(!ast::involvesCollections(*parseExpression("mat")));
        // Errori di sintassi con la posizione del token
        auto syntaxAt = [](const char* t, int pos) {
            try { (void)parseExpression(t); } catch (const SyntaxError& e) { return e.where.position == pos; } catch (...) { return false; }
            return false;
        };
        CHECK(syntaxAt("[1,2]", 1));
        CHECK(syntaxAt("Mat 1", 4));
        CHECK(syntaxAt("List 27", 5));
        CHECK(syntaxAt("{}", 1));
        CHECK(syntaxAt("2+*3", 2));
        CHECK(syntaxAt("1+2)", 3));
        CHECK(syntaxAt("sin(", 4));                                             // operando mancante alla fine
        CHECK(syntaxAt("3#", 1));
        try { (void)parseExpression("[[1,2][3]]"); CHECK(false); }
        catch (const DimensionError& d) { CHECK(d.where.position == 6 && d.where.length == 3); }
        // Le posizioni dei sotto-nodi
        auto b = parseExpression("1+Det([[1,2]])");
        CHECK(b->kind == ast::Kind::Binary && b->children[1]->pos == 2 && b->children[1]->len == 12);
        // Riscrittura degli identificatori (GRAPH) con un riferimento a una memoria
        CHECK_EQ_STR(rewriteIdentifiers("Mat A+x", [](const std::string& n) { return n == "x" ? std::string("y") : std::string(); }), "Mat A+y");
    }
    // ---------------------------------------------------------------- matrici
    Workspace ws;
    {
        CHECK(same(any(ws, "[[1,2][3,4]]+[[5,6][7,8]]"), ValueType::Matrix, 2, 2, {6, 8, 10, 12}));
        CHECK(same(any(ws, "[[1,2][3,4]]-[[1,1][1,1]]"), ValueType::Matrix, 2, 2, {0, 1, 2, 3}));
        CHECK(same(any(ws, "[[1,2][3,4]]*[[5,6][7,8]]"), ValueType::Matrix, 2, 2, {19, 22, 43, 50}));
        CHECK(same(any(ws, "[[1,2][3,4]][[5,6][7,8]]"), ValueType::Matrix, 2, 2, {19, 22, 43, 50}));   // prodotto implicito
        CHECK(same(any(ws, "2[[1,2][3,4]]"), ValueType::Matrix, 2, 2, {2, 4, 6, 8}));
        CHECK(same(any(ws, "[[1,2][3,4]]/2"), ValueType::Matrix, 2, 2, {0.5, 1, 1.5, 2}));
        CHECK(same(any(ws, "-[[1,2]]"), ValueType::Matrix, 1, 2, {-1, -2}));
        CHECK(same(any(ws, "[[1,2][3,4]]^-1"), ValueType::Matrix, 2, 2, {-2, 1, 1.5, -0.5}));
        CHECK(same(any(ws, "[[1,2][3,4]]^2"), ValueType::Matrix, 2, 2, {7, 10, 15, 22}));
        CHECK(same(any(ws, "[[1,2][3,4]]^3"), ValueType::Matrix, 2, 2, {37, 54, 81, 118}));      // manuale 2-57
        CHECK(same(any(ws, "[[1,2][3,4]]^0"), ValueType::Matrix, 2, 2, {1, 0, 0, 1}));
        CHECK_NEAR(evaluateReal(*parseExpression("Det([[1,2,3][4,5,6][-1,-2,0]])"), ws.evalContext()), -9, 1e-12);   // manuale 2-54
        CHECK_NEAR(evaluateReal(*parseExpression("Det([[1,2][2,4]])"), ws.evalContext()), 0, 1e-15);
        CHECK(same(any(ws, "Trn([[1,2][3,4][5,6]])"), ValueType::Matrix, 2, 3, {1, 3, 5, 2, 4, 6}));
        CHECK(same(any(ws, "Identity(3)"), ValueType::Matrix, 3, 3, {1, 0, 0, 0, 1, 0, 0, 0, 1}));
        CHECK(same(any(ws, "Dim([[1,2,3][4,5,6]])"), ValueType::List, 2, 1, {2, 3}));
        CHECK(same(any(ws, "Augment([[1][2]],[[3][4]])"), ValueType::Matrix, 2, 2, {1, 3, 2, 4}));
        CHECK(same(any(ws, "Fill(3,[[1,2][3,4]])"), ValueType::Matrix, 2, 2, {3, 3, 3, 3}));
        CHECK(same(any(ws, "Rref([[2,-1,3,19][1,1,-5,-21][0,4,3,0]])"), ValueType::Matrix, 3, 4, {1, 0, 0, 2, 0, 1, 0, -3, 0, 0, 1, 4}, 1e-12));
        const CalcValue ref = any(ws, "Ref([[1,2,3][4,5,6]])");
        CHECK(ref.rows == 2 && ref.cols == 3 && ref.re[0] == 1 && ref.re[3] == 0 && ref.re[4] == 1);   // forma a scala, pivot 1
        CHECK(same(any(ws, "mattolist([[1,2][3,4][5,6]],2)"), ValueType::List, 3, 1, {2, 4, 6}));
        CHECK(same(any(ws, "listtomat({1,2,3},{4,5,6})"), ValueType::Matrix, 3, 2, {1, 4, 2, 5, 3, 6}));
        CHECK(same(any(ws, "abs([[1,-2][-3,4]])"), ValueType::Matrix, 2, 2, {1, 2, 3, 4}));       // manuale 2-57
        CHECK(same(any(ws, "Int([[1.5,-2.5]])"), ValueType::Matrix, 1, 2, {1, -2}));
        CHECK(same(any(ws, "[[1,2][3,4]][[1,2][3,4]]^-1"), ValueType::Matrix, 2, 2, {1, 0, 0, 1}, 1e-12));
        CHECK_NEAR(evaluateReal(*parseExpression("Norm([[1,2][3,4]])"), ws.evalContext()), std::sqrt(30.0), 1e-12);
    }
    // ---------------------------------------------------------------- vettori
    {
        CHECK_NEAR(evaluateReal(*parseExpression("DotP([[1,2]],[[3,4]])"), ws.evalContext()), 11, 1e-12);
        CHECK(same(any(ws, "CrossP([[1,2]],[[3,4]])"), ValueType::Vector, 1, 3, {0, 0, -2}));
        CHECK(same(any(ws, "CrossP([[1][2][3]],[[4][5][6]])"), ValueType::Vector, 3, 1, {-3, 6, -3}));
        CHECK_NEAR(evaluateReal(*parseExpression("Norm([[1,3]])"), ws.evalContext()), std::sqrt(10.0), 1e-12);
        CHECK(same(any(ws, "UnitV([[5,5]])"), ValueType::Vector, 1, 2, {std::sqrt(0.5), std::sqrt(0.5)}, 1e-12));
        setAngleMode(AngleMode::Degrees);
        CHECK_NEAR(evaluateReal(*parseExpression("Angle([[1,2]],[[3,4]])"), ws.evalContext()), 10.30484646876603, 1e-9);
        setAngleMode(AngleMode::Radians);
        CHECK(same(any(ws, "[[1,2][2,1]][[1][2]]"), ValueType::Matrix, 2, 1, {5, 4}));         // manuale 2-62
    }
    // ---------------------------------------------------------------- liste
    {
        CHECK(same(any(ws, "{1,2,3}+{4,5,6}"), ValueType::List, 3, 1, {5, 7, 9}));
        CHECK(same(any(ws, "{1,2,3}*2"), ValueType::List, 3, 1, {2, 4, 6}));
        CHECK(same(any(ws, "10-{1,2}"), ValueType::List, 2, 1, {9, 8}));
        CHECK(same(any(ws, "{1,4,9}^0.5"), ValueType::List, 3, 1, {1, 2, 3}));
        CHECK(same(any(ws, "sqrt({4,9})"), ValueType::List, 2, 1, {2, 3}));
        CHECK(same(any(ws, "sin({0})"), ValueType::List, 1, 1, {0}));
        CHECK(same(any(ws, "{1,2}={1,3}"), ValueType::List, 2, 1, {1, 0}));
        CHECK(same(any(ws, "max({1,5},{3,2})"), ValueType::List, 2, 1, {3, 5}));
        auto num = [&](const char* t) { return evaluateReal(*parseExpression(t), ws.evalContext()); };
        CHECK(num("Sum({1,2,3})") == 6 && num("Prod({1,2,3,4})") == 24);
        CHECK(num("Mean({1,2,3,4})") == 2.5 && num("Median({3,1,2})") == 2 && num("Median({4,1,3,2})") == 2.5);
        CHECK(num("Min({3,1,2})") == 1 && num("Max({3,1,2})") == 3 && num("Dim({7,8,9})") == 3);
        CHECK(num("sum(x,1,4)") == 10);                                         // Σ invariato
        CHECK(same(any(ws, "Cuml({1,2,3})"), ValueType::List, 3, 1, {1, 3, 6}));
        CHECK(same(any(ws, "Percent({1,1,2})"), ValueType::List, 3, 1, {25, 25, 50}));
        CHECK(same(any(ws, "dlist({1,4,9})"), ValueType::List, 2, 1, {3, 5}));
        CHECK(same(any(ws, "SortA({3,1,2})"), ValueType::List, 3, 1, {1, 2, 3}));
        CHECK(same(any(ws, "SortD({3,1,2})"), ValueType::List, 3, 1, {3, 2, 1}));
        CHECK(same(any(ws, "Seq(x^2,x,1,5,1)"), ValueType::List, 5, 1, {1, 4, 9, 16, 25}));
        CHECK(same(any(ws, "Seq(K,K,0,1,0.25)"), ValueType::List, 5, 1, {0, 0.25, 0.5, 0.75, 1}));
        CHECK(same(any(ws, "Augment({1},{2,3})"), ValueType::List, 3, 1, {1, 2, 3}));
    }
    // ---------------------------------------------------------------- numeri complessi
    {
        const CalcValue z = any(ws, "[[1,i][0,1]][[1][i]]", ComplexMode::Rectangular);
        CHECK(z.rows == 2 && z.cols == 1 && z.complex() && z.at(0) == Complex(0, 0) && z.at(1) == Complex(0, 1));
        CHECK_THROWS_AS(any(ws, "[[i]]"), NonRealError);
        CHECK(same(any(ws, "abs([[3+4i]])", ComplexMode::Rectangular), ValueType::Matrix, 1, 1, {5}));
    }
    // ---------------------------------------------------------------- errori di dimensione e di tipo
    {
        CHECK_THROWS_AS(any(ws, "[[1,2]]+[[1,2,3]]"), DimensionError);
        CHECK_THROWS_AS(any(ws, "{1,2}+{1,2,3}"), DimensionError);
        CHECK_THROWS_AS(any(ws, "[[1,2][3,4]]*[[1,2,3]]"), DimensionError);
        CHECK_THROWS_AS(any(ws, "Det([[1,2,3]])"), DimensionError);
        CHECK_THROWS_AS(any(ws, "[[1,2,3]]^2"), DimensionError);
        CHECK_THROWS_AS(any(ws, "CrossP([[1,2,3,4]],[[1,2,3,4]])"), DimensionError);
        CHECK_THROWS_AS(any(ws, "DotP([[1,2]],[[1,2,3]])"), DimensionError);
        CHECK_THROWS_AS(any(ws, "DotP([[1,2][3,4]],[[1,2][3,4]])"), DimensionError);
        CHECK_THROWS_AS(any(ws, "Augment([[1,2]],[[1][2]])"), DimensionError);
        CHECK_THROWS_AS(any(ws, "mattolist([[1,2]],3)"), DimensionError);
        CHECK_THROWS_AS(any(ws, "Identity(1000)"), DimensionError);
        CHECK_THROWS_AS(any(ws, "Mat Z"), DimensionError);                       // non definita
        CHECK_THROWS_AS(any(ws, "Seq(x,x,1,2000,1)"), DimensionError);         // lista oltre il limite
        CHECK_THROWS_AS(any(ws, "[[1,2][2,4]]^-1"), MathError);                  // singolare
        CHECK_THROWS_AS(any(ws, "{1,2}+[[1,2]]"), TypeError);
        CHECK_THROWS_AS(any(ws, "sin([[1]])"), TypeError);
        CHECK_THROWS_AS(any(ws, "DotP({1},{2})"), TypeError);
        CHECK_THROWS_AS(any(ws, "1/[[1]]"), TypeError);
        CHECK_THROWS_AS(any(ws, "2^[[1]]"), TypeError);
        CHECK_THROWS_AS(any(ws, "1+[[1]]"), TypeError);
        CHECK_THROWS_AS(any(ws, "[[{1}]]"), TypeError);
        CHECK_THROWS_AS(any(ws, "Sum([[1,2]])"), TypeError);
        CHECK_THROWS_AS(any(ws, "Det(3)"), TypeError);
        CHECK_THROWS_AS(evaluateReal(*parseExpression("[[1,2]]"), ws.evalContext()), TypeError);   // serve un numero
        CHECK_THROWS_AS(evaluateReal(*parseExpression("{1,2}+1"), ws.evalContext()), TypeError);
        // Posizione dell'errore: il sotto-calcolo che non riesce
        CHECK(failsAt<DimensionError>(ws, "1+Det([[1,2]])", 2, 12));
        CHECK(failsAt<DimensionError>(ws, "{1,2}+{1,2,3}", 0, 13));
        CHECK(failsAt<TypeError>(ws, "3*(1/[[1]])", 3, 7));
        CHECK(failsAt<MathError>(ws, "14/0*2.3", 0, 4));
        CHECK(failsAt<NonRealError>(ws, "1+ln(-1)+2", 2, 6));
        CHECK(failsAt<DimensionError>(ws, "2+Mat Q", 2, 5));
    }
    // ---------------------------------------------------------------- stack: annidamento profondo di collezioni
    {
        std::string deep = "[[1,2][3,4]]";
        for (int i = 0; i < 20; ++i) deep = "Trn(" + deep + ")";
        CHECK(same(any(ws, deep), ValueType::Matrix, 2, 2, {1, 2, 3, 4}));         // 20 livelli: entro i limiti
        const std::size_t budget = evalStackBudget();
        setEvalStackBudget(2048);                                                   // budget ridotto: Stack ERROR, mai un crash
        bool stackError = false;
        try { (void)any(ws, deep); } catch (const MathError& e) { stackError = std::string(e.what()).rfind("Stack ERROR", 0) == 0; }
        CHECK(stackError);
        exact::Value v;
        CHECK(!evaluateExact(*parseExpression("Det(" + deep + ")"), ws.evalContext(), v));   // l'esatto si arrende
        setEvalStackBudget(budget);
        CHECK(evaluateExact(*parseExpression("Det(" + deep + ")"), ws.evalContext(), v) && exact::toText(v, false) == "-2");
    }
    // ---------------------------------------------------------------- forme esatte
    {
        ExactCollection ex;
        CHECK(evaluateExactAny(*parseExpression("[[1,2][3,4]]^-1"), ws.evalContext(), ex));
        CHECK(ex.type == ValueType::Matrix && ex.rows == 2 && ex.elements.size() == 4);
        CHECK(exact::toText(ex.elements[2], false) == "3/2" && exact::toText(ex.elements[3], false) == "-1/2");
        CHECK(evaluateExactAny(*parseExpression("{1/3,1/6}*3"), ws.evalContext(), ex) && exact::toText(ex.elements[1], false) == "1/2");
        CHECK(evaluateExactAny(*parseExpression("UnitV([[1,1]])"), ws.evalContext(), ex) && exact::toText(ex.elements[0], false) == "\xE2\x88\x9A" "2/2");
        CHECK(!evaluateExactAny(*parseExpression("sin({1})"), ws.evalContext(), ex));          // nessuna forma esatta
        exact::Value v;
        CHECK(evaluateExact(*parseExpression("Det([[1,2][3,4]])"), ws.evalContext(), v) && exact::toText(v, false) == "-2");
        CHECK(evaluateExact(*parseExpression("Mean({1,2})"), ws.evalContext(), v) && exact::toText(v, false) == "3/2");
    }
    // ---------------------------------------------------------------- Run-Matrix: memorie e risultati
    {
        Workspace w;
        runCalculation(w, "[[1,2][3,4]]\xE2\x86\x92Mat A");
        CHECK(same(w.matrices.mat[0], ValueType::Matrix, 2, 2, {1, 2, 3, 4}) && w.matrices.matAns == w.matrices.mat[0]);
        CHECK(isCollection(w.history.back()) && last(w) == "[[1,2][3,4]]");
        runCalculation(w, "Mat A^-1");
        CHECK_EQ_STR(last(w), "[[-2,1][3/2,-1/2]]");                            // forme esatte negli elementi
        CHECK(toggleExactDecimal(w.history.back(), w.settings));
        CHECK_EQ_STR(last(w), "[[-2,1][1.5,-0.5]]");
        CHECK(toggleExactDecimal(w.history.back(), w.settings));
        CHECK_EQ_STR(last(w), "[[-2,1][3/2,-1/2]]");
        CHECK(!togglePeriodic(w.history.back()));
        // Il risultato di una matrice non cambia Ans (che resta un numero).
        runCalculation(w, "3"); runCalculation(w, "Mat A+Mat A"); runCalculation(w, "Ans+1");
        CHECK_EQ_STR(last(w), "4");
        // Vettori: Vct e VctAns
        runCalculation(w, "[[1,2,3]]\xE2\x86\x92Vct B");
        CHECK(w.matrices.vct[1].type == ValueType::Vector && w.matrices.vct[1].cols == 3);
        CHECK(w.matrices.vctAns.size() == 3 && w.matrices.matAns.size() == 3);   // 1×n: MatAns e VctAns
        CHECK(entryError<DimensionError>(w, "[[1,2][3,4]]\xE2\x86\x92Vct C").position == 12);   // segnalato alla freccia
        CHECK(w.matrices.vct[2].empty());
        runCalculation(w, "Vct B\xE2\x86\x92Mat D");
        CHECK(w.matrices.mat[3].type == ValueType::Matrix && w.matrices.mat[3].rows == 1);
        // Liste: List 1-6 sono quelle di Statistics
        runCalculation(w, "{1,2,3}\xE2\x86\x92List 1");
        CHECK(w.lists[0] == std::vector<double>({1, 2, 3}));
        runCalculation(w, "List 1*2");
        CHECK_EQ_STR(last(w), "{2,4,6}");
        CHECK(w.matrices.listAns.size() == 3 && w.matrices.listAns.re[2] == 6);
        runCalculation(w, "List Ans+1");
        CHECK_EQ_STR(last(w), "{3,5,7}");
        runCalculation(w, "7\xE2\x86\x92List 2[1]"); runCalculation(w, "8\xE2\x86\x92List 2[2]");
        CHECK(w.lists[1] == std::vector<double>({7, 8}));
        CHECK(entryError<DimensionError>(w, "9\xE2\x86\x92List 2[4]").known());   // una cella alla volta
        CHECK(entryError<MathError>(w, "{1}\xE2\x86\x92List 7").known());          // List 7-26 non disponibili
        // Elementi e dimensioni
        runCalculation(w, "5\xE2\x86\x92Mat A[1,2]");
        CHECK(w.matrices.mat[0].re[1] == 5);
        runCalculation(w, "Mat A[1,2]");
        CHECK_EQ_STR(last(w), "5");
        runCalculation(w, "{2,3}\xE2\x86\x92" "Dim Mat C");
        CHECK(same(w.matrices.mat[2], ValueType::Matrix, 2, 3, {0, 0, 0, 0, 0, 0}));
        CHECK(entryError<TypeError>(w, "[[1]]\xE2\x86\x92" "A").known());          // una matrice non va in A-Z
        CHECK(entryError<TypeError>(w, "2\xE2\x86\x92Mat E").known());             // un numero non va in Mat
        CHECK(entryError<SyntaxError>(w, "1\xE2\x86\x92Mat Ans").known());
        // Risultato oltre la vista naturale (6×6): solo tipo e dimensioni nella cronologia.
        runCalculation(w, "Identity(7)");
        CHECK(isCollection(w.history.back()) && w.history.back().collection->value.empty());
        CHECK_EQ_STR(last(w), "Mat 7\xC3\x97" "7 \xE2\x86\x92 Mat Ans");
        CHECK(w.matrices.matAns.size() == 49);
        CHECK_EQ_STR(resultExpression(w.history.back()), "Mat Ans");
        // Vista naturale: albero con la matrice
        runCalculation(w, "[[1/2,1][0,1]]");
        const auto tree = resultTree(w.history.back(), w.settings);
        CHECK(tree->children.size() == 1 && tree->children[0]->kind == ast::Kind::Matrix && tree->children[0]->children.size() == 4);
        CHECK(tree->children[0]->children[0]->children[0]->kind == ast::Kind::Fraction);   // 1/2 come frazione
        CHECK_EQ_STR(resultExpression(w.history.back()), "[[(1)/(2),1][0,1]]");
        runCalculation(w, "{1/2,3}");
        const auto listTree = resultTree(w.history.back(), w.settings);         // { 1/2 , 3 }
        CHECK(listTree->children.size() == 5 && listTree->children[0]->text == "{" && listTree->children[1]->kind == ast::Kind::Fraction &&
              listTree->children[4]->text == "}");
        // Posizione degli errori in evaluateEntry (spazi iniziali compresi)
        CHECK(entryError<DimensionError>(w, "  1+Det([[1,2]])").position == 4);
        CHECK(entryError<SyntaxError>(w, "1+)").position == 2);
        const ErrorLocation arrow = entryError<SyntaxError>(w, "1\xE2\x86\x92Q~A");
        CHECK(arrow.position == 1 && arrow.length == 6);
        // Modifica della cronologia: in caso di errore matrici e liste tornano come prima.
        Workspace h;
        runCalculation(h, "[[1]]\xE2\x86\x92Mat A");
        runCalculation(h, "Mat A*2");
        CHECK_THROWS_AS(editHistoryEntry(h, 0, "[[9]]+Det(Mat Z)\xE2\x86\x92Mat A"), MathError);
        CHECK(h.matrices.mat[0].re[0] == 1 && h.matrices.matAns.re[0] == 2);
        CHECK_EQ_STR(h.history[0].expression, "[[1]]\xE2\x86\x92Mat A");
        CHECK(editHistoryEntry(h, 0, "[[2]]\xE2\x86\x92Mat A") == 0);
        CHECK_EQ_STR(resultText(h.history[1], h.settings), "[[4]]");
        try { (void)editHistoryEntry(h, 1, "Mat A+Det([[1,2]])"); CHECK(false); }
        catch (const MathError& e) { CHECK(e.where.position == 6 && e.where.length == 12); }
    }
    // ---------------------------------------------------------------- limiti per scheda
    {
        Workspace w;
        CHECK_THROWS_AS(w.storeMatrix(0, CalcValue::matrix(limits::kMatrixMaxDim + 1, 1)), DimensionError);
        CHECK_THROWS_AS(w.storeVector(0, CalcValue::matrix(2, 2, ValueType::Vector)), DimensionError);
        CHECK_THROWS_AS(w.storeMatrix(0, CalcValue::list(3)), TypeError);
        std::size_t side = 1;
        while ((side + 1) * (side + 1) <= limits::kMatrixElements && side + 1 <= limits::kMatrixMaxDim) ++side;
        std::size_t stored = 0;
        bool full = false;
        for (std::size_t slot = 0; slot < CollectionMemory::Slots && !full; ++slot) {
            try { w.storeMatrix(slot, CalcValue::matrix(side, side)); stored += side * side; }
            catch (const DimensionError&) { CHECK(false); }
            catch (const MathError&) { full = true; }
        }
        CHECK(full && stored <= limits::kMatrixStoreElements && w.matrices.elements() <= limits::kMatrixStoreElements);
    }
    // ---------------------------------------------------------------- persistenza (record v6)
    {
        Workspace w;
        w.settings.inputMode = Settings::InputMode::Line;
        runCalculation(w, "[[1/2,2][3,4]]\xE2\x86\x92Mat A");
        runCalculation(w, "[[1,2,3]]\xE2\x86\x92Vct C");
        runCalculation(w, "{1,2}\xE2\x86\x92List 3");
        runCalculation(w, "List 3+1");
        CHECK_THROWS_AS(runCalculation(w, "[[1+i]]"), NonRealError);            // Real: errore, nessuna riga
        w.settings.complex = ComplexMode::Rectangular;
        runCalculation(w, "[[1+i,2]]\xE2\x86\x92Mat B");
        runCalculation(w, "Identity(8)");
        runCalculation(w, "2+2");
        const auto bytes = serializeWorkspace(w);
        CHECK(bytes[4] == 6 && bytes[5] == 0);
        Workspace back;
        CHECK(deserializeWorkspace(bytes.data(), bytes.size(), back));
        CHECK(back.settings.inputMode == Settings::InputMode::Line);
        CHECK(back.matrices.mat[0] == w.matrices.mat[0] && back.matrices.vct[2] == w.matrices.vct[2]);
        CHECK(back.matrices.mat[1].complex() && back.matrices.mat[1].at(0) == Complex(1, 1));
        CHECK(back.matrices.matAns.size() == 64 && back.matrices.listAns == w.matrices.listAns);
        CHECK(back.lists[2] == std::vector<double>({1, 2}));
        CHECK(back.history.size() == w.history.size());
        for (std::size_t i = 0; i < w.history.size(); ++i) {
            CHECK(isCollection(back.history[i]) == isCollection(w.history[i]));
            CHECK(resultText(back.history[i], back.settings) == resultText(w.history[i], w.settings));
        }
        CHECK_EQ_STR(resultText(back.history[0], back.settings), "[[1/2,2][3,4]]");   // forme esatte salvate
        // Dati troncati o alterati: rifiutati, destinazione invariata.
        Workspace target; target.memory.store('B', 5);
        for (std::size_t cut = 0; cut < bytes.size(); cut += 7) CHECK(!deserializeWorkspace(bytes.data(), cut, target));
        CHECK(target.memory.recall('B') == 5 && target.matrices.mat[0].empty());
        // Il secondo salvataggio e' identico (formato stabile)
        CHECK(serializeWorkspace(back) == bytes);
    }
    // ---------------------------------------------------------------- record delle versioni precedenti
    {
        Workspace w;
        CHECK(deserializeWorkspace(kV190Record, sizeof kV190Record, w));      // v1.9.0, formato 5
        CHECK(w.settings.angle == AngleMode::Degrees && w.settings.mixedFractions && w.settings.fileStore == Settings::FileStore::Internal);
        CHECK(w.settings.inputMode == Settings::InputMode::Math);              // valore predefinito
        CHECK(w.history.size() == 3 && w.memory.recall('B') == 7 && w.lists[0].size() == 3 && w.lists[1].size() == 3);
        CHECK(w.functions[0].expr == "(x)^(2)-1" && w.graph3d.model.z[1] == "x*y" && w.chat.size() == 1);
        CHECK(w.matrices.elements() == 0 && w.matrices.listAns.empty());
        for (const auto& h : w.history) CHECK(!isCollection(h));
        CHECK_EQ_STR(resultText(w.history[0], w.settings), "1/2");
        // Le liste della v1.9.0 sono subito utilizzabili nei calcoli.
        runCalculation(w, "Sum(List 1)+Sum(List 2)");
        CHECK_EQ_STR(resultText(w.history.back(), w.settings), "22.5");
        const auto bytes = serializeWorkspace(w);
        Workspace again;
        CHECK(bytes[4] == 6 && deserializeWorkspace(bytes.data(), bytes.size(), again) && again.history.size() == 4);
        Workspace old;
        CHECK(deserializeWorkspace(kV180Record, sizeof kV180Record, old) && old.matrices.elements() == 0);   // v1.8.0, formato 4
    }
    return testutil::finish("matrix_test");
}
