// v1.10.0 — editor e interfaccia di Run-Matrix: modelli MAT/VCT, modalita'
// Math/Line, selezione parziale (CLIP) con copia/taglia/incolla, parentesi
// colorate per livello, cursore sulla causa di un errore, editor MAT/VCT e
// persistenza delle impostazioni, provati sul firmware reale (Rig).
#include "app_rig.hpp"
#include "math_renderer.hpp"
#include "natural_editor.hpp"
#include "run_matrix.hpp"
#include "test_util.hpp"
#include "ui_kit.hpp"
#include <cmath>
#include <random>
#include <string>
#include <vector>

using namespace fx;
using testrig::Rig;

namespace {
void type(NaturalEditor& e, const std::string& s) { for (char c : s) e.insert(c); }
double value(const NaturalEditor& e) { return evaluate(e.toExpression()); }
std::vector<std::uint8_t> parenColors(const NaturalEditor& e) {
    std::vector<std::uint8_t> out;
    for (const auto& op : e.layout(0, 20, 300))
        if (op.kind == NaturalDrawOp::Kind::Text && (op.text == "(" || op.text == ")")) out.push_back(op.color);
    return out;
}
int highlights(const std::vector<NaturalDrawOp>& ops, std::uint8_t kind) {
    int n = 0;
    for (const auto& op : ops) if (op.kind == NaturalDrawOp::Kind::Highlight && op.color == kind) ++n;
    return n;
}
// Rapporto di contrasto WCAG 2 tra due colori RGB565.
double contrast(std::uint16_t a, std::uint16_t b) {
    auto lum = [](std::uint16_t c) {
        const gfx::RGB888 rgb = gfx::rgb565To888(c);
        auto ch = [](std::uint8_t v) { const double x = v / 255.0; return x <= 0.03928 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4); };
        return 0.2126 * ch(rgb.r) + 0.7152 * ch(rgb.g) + 0.0722 * ch(rgb.b);
    };
    const double la = lum(a), lb = lum(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}
std::string clip(const ast::NodePtr& n) { return n ? ast::serialize(*n) : std::string("<vuoto>"); }
} // namespace

int main() {
    // ---------------------------------------------------------------- modelli MAT/VCT (modalita' Math)
    {
        NaturalEditor e;
        e.insertMatrix(2, 2);
        type(e, "1"); CHECK(e.moveRight()); type(e, "2"); CHECK(e.moveRight()); type(e, "3"); CHECK(e.moveRight()); type(e, "4");
        CHECK_EQ_STR(e.toExpression(), "[[1,2][3,4]]");
        CHECK(e.moveUp()); type(e, "0");                                          // dalla cella (2,2) alla (1,2)
        CHECK_EQ_STR(e.toExpression(), "[[1,20][3,4]]");
        CHECK(e.moveDown() && e.cursorPath().size() == 2 && e.cursorPath()[1] == 3);
        CHECK(e.moveRight());                                                     // esce dal modello
        type(e, "*2");
        CHECK_EQ_STR(e.toExpression(), "[[1,20][3,4]]*2");
        // Importazione strutturale e disegno a griglia
        e.setExpression("Det([[1,2][3,4]])");
        CHECK(e.root().children.size() == 4 && e.root().children[2]->kind == ast::Kind::Matrix);
        CHECK_EQ_STR(e.toExpression(), "Det([[1,2][3,4]])");
        CHECK_NEAR(value(e), -2, 1e-12);
        int x1 = -1, x2 = -1, y1 = -1, y3 = -1;
        for (const auto& op : e.layout(0, 40, 300)) {
            if (op.kind != NaturalDrawOp::Kind::Text) continue;
            if (op.text == "1") { x1 = op.x; y1 = op.y; }
            if (op.text == "2") x2 = op.x;
            if (op.text == "3") y3 = op.y;
        }
        CHECK(x2 > x1 && y3 > y1);                                                // colonne affiancate, righe sovrapposte
        // Un testo che non e' una matrice rettangolare resta come token.
        e.setExpression("[[1,2][3]]");
        CHECK(e.root().children[0]->kind == ast::Kind::Token);
        // Mat / Vct / List sono un solo token ciascuno
        e.setExpression("MatA+List1");
        CHECK(e.root().children.size() == 5 && e.root().children[0]->text == "Mat" && e.root().children[3]->text == "List");
        CHECK_EQ_STR(math::displayToken("Mat"), "Mat ");
        CHECK_EQ_STR(math::displayToken("mattolist"), "Mat\xE2\x86\x92List");
        // Liste e parentesi quadre si digitano
        e.clear(); type(e, "{1,2}+[[3,4]]");
        CHECK_EQ_STR(e.toExpression(), "{1,2}+[[3,4]]");
        // ^ dopo una lista o una matrice digitate: l'intero gruppo e' la base
        e.clear(); type(e, "{1,2}^2");
        CHECK_EQ_STR(e.toExpression(), "({1,2})^(2)");
        e.clear(); type(e, "[[1,1][0,1]]"); e.insertFraction(); type(e, "2");
        CHECK_EQ_STR(e.toExpression(), "([[1,1][0,1]])/(2)");
        // Backspace all'inizio di un modello vuoto lo elimina
        e.clear(); e.insertMatrix(1, 3);
        CHECK(e.backspace() && e.empty());
        CHECK_THROWS_AS(e.insertMatrix(7, 1), std::invalid_argument);
    }
    // ---------------------------------------------------------------- modalita' Line
    {
        NaturalEditor e;
        e.setLinear(true);
        CHECK(e.linear());
        type(e, "1"); e.insertFraction(); type(e, "3");
        CHECK_EQ_STR(e.toExpression(), "1/3");
        CHECK(e.root().children.size() == 3);                                     // soltanto token
        e.clear(); e.insertRadical(); type(e, "16)"); e.insertPower(); type(e, "2");
        CHECK_EQ_STR(e.toExpression(), "\xE2\x88\x9A(16)^2");
        CHECK_NEAR(value(e), 16, 1e-12);
        e.clear(); e.insertMatrix(2, 2); type(e, "1");
        CHECK_EQ_STR(e.toExpression(), "[[1,][,]]");
        CHECK_THROWS_AS(e.insertMixedFraction(), std::invalid_argument);       // solo in modalita' Math
        e.clear(); type(e, "sqrt9");                                              // nessun modello automatico
        CHECK_EQ_STR(e.toExpression(), "sqrt9");
        // Importazione lineare: nessun modello
        e.setExpression("(1)/(3)+[[1,2]]");
        for (const auto& c : e.root().children) CHECK(c->kind == ast::Kind::Token);
        CHECK_EQ_STR(e.toExpression(), "(1)/(3)+[[1,2]]");
        // Conversione Math <-> Line della riga di input
        NaturalEditor m;
        m.insertFraction(); type(m, "1"); CHECK(m.moveDown()); type(m, "3");
        CHECK(m.root().children[0]->kind == ast::Kind::Fraction);
        m.setLinear(true);
        CHECK(m.root().children.size() == 7 && m.toExpression() == "(1)/(3)");
        m.setLinear(false);
        CHECK(m.root().children.size() == 1 && m.root().children[0]->kind == ast::Kind::Fraction);
        // Incolla in Line: il modello diventa testo lineare
        NaturalEditor l; l.setLinear(true);
        l.paste(m.root());
        CHECK(l.root().children.size() == 7 && l.toExpression() == "(1)/(3)");
    }
    // ---------------------------------------------------------------- CLIP: selezione, copia, taglia, incolla
    {
        NaturalEditor e;
        type(e, "12+34");
        CHECK(e.moveLeft() && e.moveLeft());
        e.beginSelection();
        CHECK(e.selecting() && e.selection().empty());
        CHECK(e.selectRight() && e.selectRight());
        CHECK(e.selection().begin == 3 && e.selection().end == 5);
        CHECK_EQ_STR(clip(e.copySelection()), "34");
        CHECK(highlights(e.layout(0, 20, 300), 0) == 1);                          // evidenziazione
        auto cut = e.cutSelection();
        CHECK(!e.selecting() && clip(cut) == "34" && e.toExpression() == "12+");
        e.moveHome(); e.paste(*cut);
        CHECK_EQ_STR(e.toExpression(), "3412+");
        CHECK(e.undo() && e.toExpression() == "12+" && e.undo() && e.toExpression() == "12+34");
        // Parentesi bilanciate: la selezione si estende alla parentesi corrispondente.
        e.clear(); type(e, "(1+2)*3");
        e.moveHome(); e.beginSelection(); CHECK(e.selectRight() && e.selectRight());   // "(1"
        CHECK_EQ_STR(clip(e.copySelection()), "(1+2)");
        e.moveHome(); CHECK(e.moveRight() && e.moveRight() && e.moveRight());
        e.beginSelection(); for (int i = 0; i < 4; ++i) (void)e.selectRight();       // "2)*3"
        CHECK_EQ_STR(clip(e.copySelection()), "(1+2)*3");
        e.moveHome(); CHECK(e.moveRight()); e.beginSelection(); CHECK(e.selectRight() && e.selectRight() && e.selectRight());
        CHECK_EQ_STR(clip(e.copySelection()), "1+2");                             // dentro le parentesi
        // Il nome della funzione segue la sua parentesi
        e.clear(); type(e, "sin(30)+1");
        e.moveHome(); CHECK(e.moveRight());
        e.beginSelection(); for (int i = 0; i < 4; ++i) (void)e.selectRight();   // "(30)"
        CHECK_EQ_STR(clip(e.copySelection()), "sin(30)");
        // Dentro un modello la selezione resta nel campo; oltre il bordo prende tutto il modello.
        e.clear(); e.insertFraction(); type(e, "12"); CHECK(e.moveDown()); type(e, "5");
        e.beginSelection(); CHECK(e.selectLeft());
        CHECK_EQ_STR(clip(e.copySelection()), "5");
        CHECK(e.selectLeft());                                                    // oltre l'inizio del denominatore
        CHECK(e.cursorPath().empty() && e.selection().begin == 0 && e.selection().end == 1);
        CHECK_EQ_STR(clip(e.copySelection()), "(12)/(5)");
        CHECK(e.deleteSelection() && e.empty());
        // Una selezione vuota non copia nulla; un movimento la annulla.
        e.clear(); type(e, "7"); e.beginSelection();
        CHECK(e.copySelection() == nullptr);
        CHECK(e.moveLeft() && !e.selecting());
    }
    // ---------------------------------------------------------------- parentesi colorate
    {
        NaturalEditor e;
        type(e, "((1)+(2))");
        CHECK(parenColors(e) == std::vector<std::uint8_t>({1, 2, 2, 2, 2, 1}));   // blu, rosso ... rosso, blu
        e.clear(); type(e, "((((((1))))))");
        CHECK(parenColors(e) == std::vector<std::uint8_t>({1, 2, 3, 4, 5, 1, 1, 5, 4, 3, 2, 1}));   // dopo il nero di nuovo blu
        // Un modello aperto dentro una parentesi e' al livello successivo; le parentesi
        // lasciate aperte in un campo non cambiano i colori dopo il modello.
        e.clear(); type(e, "("); e.insertFraction(); type(e, "(1"); CHECK(e.moveDown()); type(e, "2"); CHECK(e.moveRight()); type(e, ")");
        CHECK(parenColors(e) == std::vector<std::uint8_t>({1, 2, 1}));
        // Il gruppo ( ) del menu MATH segue le stesse regole
        e.clear(); type(e, "("); e.insertGroup(); type(e, "1");
        std::vector<std::uint8_t> group;
        for (const auto& op : e.layout(0, 20, 300)) if (op.kind == NaturalDrawOp::Kind::Text && (op.text == "(" || op.text == ")")) group.push_back(op.color);
        CHECK(group.size() == 3 && group[0] == 1 && group[1] == 2 && group[2] == 2);
        // Dopo l'esecuzione (cronologia) le parentesi tornano nere: nessun colore senza colorParens.
        auto plain = math::layout(e.root(), 0, 20, 300);
        for (const auto& op : plain.ops) CHECK(op.color == 0);
        // Leggibilita': contrasto di almeno 4,5:1 su ogni sfondo dell'input.
        for (std::uint16_t color : ui::theme::Paren)
            for (std::uint16_t bg : {ui::theme::Bg, ui::theme::SelectSoft, ui::theme::Highlight, ui::theme::ErrorHighlight})
                CHECK(contrast(color, bg) >= 4.5);
        CHECK(contrast(ui::theme::Ink, ui::theme::Highlight) >= 7 && contrast(ui::theme::Ink, ui::theme::ErrorHighlight) >= 7);
    }
    // ---------------------------------------------------------------- cursore sulla causa di un errore
    {
        NaturalEditor e;
        type(e, "14/0*2.3");
        CHECK(e.markError(0, 4));                                                 // 14÷0: cursore dopo lo 0 (manuale 1-10)
        CHECK(e.cursorPath().empty() && e.cursorIndex() == 4);
        CHECK(e.errorMark().begin == 0 && e.errorMark().end == 4);
        CHECK(highlights(e.layout(0, 20, 300), 1) == 1);
        CHECK(e.moveLeft()); type(e, "1");
        CHECK_EQ_STR(e.toExpression(), "14/10*2.3");                              // e' gia' modificabile
        CHECK(e.errorMark().empty());                                            // la modifica toglie l'evidenziazione
        // Dentro un modello: il cursore va nel campo che contiene l'errore.
        e.clear(); e.insertFraction(); type(e, "1"); CHECK(e.moveDown()); type(e, "2+*3");
        const std::string text = e.toExpression();                                // (1)/(2+*3)
        try { (void)parseExpression(text); CHECK(false); }
        catch (const SyntaxError& s) {
            CHECK(s.where.position == 7);
            CHECK(e.markError(static_cast<std::size_t>(s.where.position), static_cast<std::size_t>(s.where.length)));
        }
        CHECK(e.cursorPath() == std::vector<std::size_t>({0, 1}) && e.cursorIndex() == 3);
        // L'intera frazione quando e' lei la causa
        e.clear(); e.insertFraction(); type(e, "1"); CHECK(e.moveDown()); type(e, "0"); CHECK(e.moveRight()); type(e, "+1");
        CHECK(e.markError(0, 7) && e.cursorPath().empty() && e.cursorIndex() == 1);
        CHECK(e.errorMark().begin == 0 && e.errorMark().end == 1);
        // Errore alla fine del testo (operando mancante): cursore in fondo, nulla da evidenziare.
        e.clear(); type(e, "2+");
        CHECK(e.markError(2, 0) && e.cursorIndex() == 2 && e.errorMark().empty());
        CHECK(!e.markError(99, 1));
    }
    // ---------------------------------------------------------------- robustezza: sequenze casuali
    {
        std::mt19937 rng(20261114);
        int completed = 0;
        for (int round = 0; round < 600; ++round) {
            NaturalEditor e;
            if (round % 3 == 0) e.setLinear(true);
            for (int k = 0; k < 40; ++k) {
                try {
                    switch (rng() % 18) {
                    case 0: e.insert("0123456789+-*/(),[]{}xA"[rng() % 23]); break;
                    case 1: e.insertMatrix(1 + rng() % 3, 1 + rng() % 3); break;
                    case 2: e.insertFraction(); break;
                    case 3: e.insertPower(); break;
                    case 4: (void)e.moveLeft(); break;
                    case 5: (void)e.moveRight(); break;
                    case 6: (void)e.moveUp(); break;
                    case 7: (void)e.moveDown(); break;
                    case 8: (void)e.backspace(); break;
                    case 9: (void)e.selectLeft(); break;
                    case 10: (void)e.selectRight(); break;
                    case 11: { auto c = e.cutSelection(); if (c) e.paste(*c); break; }
                    case 12: (void)e.undo(); break;
                    case 13: { const auto t = e.toExpression(); (void)e.markError(rng() % (t.size() + 1), rng() % 4); break; }
                    case 14: e.setLinear(!e.linear()); break;
                    case 15: e.insertGroup(); break;
                    case 16: e.insertSymbol("Mat"); break;
                    default: { const auto t = e.toExpression(); e.setExpression(t); break; }
                    }
                    (void)e.toExpression();
                    (void)e.layout(0, 20, 300);
                } catch (const std::exception&) {}
            }
            ++completed;
        }
        CHECK(completed == 600);
    }
    // ---------------------------------------------------------------- Run-Matrix sul firmware
    {
        Rig r;
        auto& ws = r.ws();
        r.type("1");                                                              // menu principale -> Run-Matrix
        CHECK(r.fw.apps().activeId() == app::AppId::RunMatrix);
        auto* rm = r.active<app::RunMatrixApp>();
        // Errore: il cursore va sulla causa e la causa e' evidenziata.
        r.enter("14/0*2.3");
        CHECK(r.error() && r.errorTitle() == "Math ERROR");
        CHECK(rm->editor().cursorIndex() == 4 && rm->editor().errorMark().end == 4);
        r.closeError();
        r.key(pckey::Left); r.type("1"); r.key(pckey::Enter);
        CHECK(!r.error());
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "3.22");
        r.enter("2+*3");
        CHECK(r.error() && r.errorTitle() == "Syntax ERROR" && rm->editor().cursorIndex() == 3);
        r.closeError();
        r.key(pckey::Break);
        // MAT/VCT: SHIFT 3, DIM 2×2, celle con EXE verso destra
        r.key(pckey::F7); r.type("3");
        CHECK(rm->matrixEditor().isOpen() && rm->matrixEditor().screen() == app::MatrixEditorView::Screen::List);
        r.key(pckey::F3);
        CHECK(rm->matrixEditor().screen() == app::MatrixEditorView::Screen::Dim);
        r.key(pckey::Backspace); r.type("2"); r.key(pckey::Enter); r.type("2"); r.key(pckey::Enter);
        CHECK(rm->matrixEditor().screen() == app::MatrixEditorView::Screen::Cells);
        r.enter("1"); r.enter("2"); r.enter("3"); r.enter("4");
        CHECK(ws.matrices.mat[0].size() == 4 && ws.matrices.mat[0].re[3] == 4 && ws.matrices.mat[0].re[1] == 2);
        // ROW-OP SWAP delle righe 1 e 2, poi ROW ADD
        r.key(pckey::F1); r.key(pckey::Enter);                                    // SWAP
        r.key(pckey::Backspace); r.enter("1"); r.key(pckey::Backspace); r.enter("2");
        CHECK(ws.matrices.mat[0].re[0] == 3 && ws.matrices.mat[0].re[2] == 1);
        r.key(pckey::F2); r.type("3");                                            // ROW > ADD
        CHECK(ws.matrices.mat[0].rows == 3 && ws.matrices.mat[0].re[4] == 0);
        r.key(pckey::F2); r.type("1");                                            // ROW > DELETE (la riga aggiunta)
        CHECK(ws.matrices.mat[0].rows == 2);
        r.key(pckey::Esc);                                                        // celle -> elenco
        r.type("b"); CHECK(rm->matrixEditor().selected() == 1);                   // lettera: Mat B
        r.key(pckey::F6); CHECK(rm->matrixEditor().vectors());                    // M<->V
        r.key(pckey::Enter); r.key(pckey::Backspace); r.type("3"); r.key(pckey::Enter);
        r.key(pckey::Backspace); r.type("2"); r.key(pckey::Enter);                // 3×2 non e' un vettore
        CHECK(r.error() && r.errorTitle() == "Dimension ERROR");
        r.closeError();
        r.key(pckey::Backspace); r.type("1"); r.key(pckey::Enter);
        CHECK(ws.matrices.vct[1].rows == 3 && ws.matrices.vct[1].cols == 1);
        r.key(pckey::Esc); r.key(pckey::Esc);
        CHECK(!rm->matrixEditor().isOpen());
        // La matrice si usa nei calcoli: Det(Mat A) = 3·2 - 4·1 = 2 (dopo lo SWAP)
        r.enter("Det(MatA)");
        CHECK(!r.error());
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "2");
        r.enter("MatA^-1");
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "[[1,-2][-1/2,3/2]]");
        // CLIP: SHIFT 8, frecce, F1 COPY; SHIFT 9 incolla
        r.type("12+34");
        r.key(pckey::F7); r.type("8");
        CHECK(rm->editor().selecting());
        r.key(pckey::Left); r.key(pckey::Left);
        r.key(pckey::F1);
        CHECK(!rm->editor().selecting() && clip(r.fw.apps().context().clipboard) == "34");
        r.key(pckey::F7); r.type("9");
        CHECK_EQ_STR(rm->editor().toExpression(), "12+3434");
        // F2 CUT
        r.key(pckey::F7); r.type("8"); r.key(pckey::Left); r.key(pckey::Left); r.key(pckey::F2);
        CHECK_EQ_STR(rm->editor().toExpression(), "12+34");
        r.key(pckey::Break);
        // Modalita' Line da SET UP: il primo campo e' Input/Output
        r.home(); r.type("g");
        r.key(pckey::Right);
        CHECK(ws.settings.inputMode == Settings::InputMode::Line);
        r.home(); r.type("1");
        rm = r.active<app::RunMatrixApp>();
        CHECK(rm->editor().linear());
        r.type("1"); r.key(pckey::F1); r.type("4"); r.key(pckey::Enter);          // F1 in Line: "/"
        CHECK_EQ_STR(ws.history.back().expression, "1/4");
        CHECK_EQ_STR(resultText(ws.history.back(), ws.settings), "1/4");
        r.home();
        r.settle();
    }
    // Riavvio: matrici, vettori e impostazione Input/Output restano.
    {
        Rig again;
        auto& ws = again.ws();
        CHECK(ws.settings.inputMode == Settings::InputMode::Line);
        CHECK(ws.matrices.mat[0].rows == 2 && ws.matrices.mat[0].re[0] == 3 && ws.matrices.vct[1].rows == 3);
        CHECK(!ws.history.empty() && isCollection(ws.history[ws.history.size() - 2]));
        again.type("1");
        auto* rm = again.active<app::RunMatrixApp>();
        CHECK(rm->editor().linear());
        again.home(); again.type("g"); again.key(pckey::Left);                     // di nuovo Math
        CHECK(ws.settings.inputMode == Settings::InputMode::Math);
        again.home(); again.settle();
    }
    return testutil::finish("editor_v110_test");
}
