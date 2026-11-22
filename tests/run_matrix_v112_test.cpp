// v1.12.0 — Run-Matrix e workspace (record v8): istruzioni multiple ":" e "◢",
// stringhe e Str 1-20, function memory f1-f20, variabili r e θ, variabili di
// sistema assegnabili (V-Window, FACTOR, TABLE, DYNA, RECURSION, TVM), List
// "nome", List File 1-6 con il budget comune, n->Dim List, Fill sulle memorie,
// Swap/*Row/*Row+/Row+, forme esatte delle matrici oltre 64 elementi e celle
// decimali, VARS EQUATION/TABLE, migrazione dei record v8 (fixture della
// v1.12.0) e v7 (fixture della v1.11.0) e interfaccia (OPTN nell'ordine della fx-CG50, VARS, CATALOG,
// FUNCMEM, CONVERT, CAPTURE, ENG).
#include "app_rig.hpp"
#include "run_matrix.hpp"
#include "test_util.hpp"
#include <cmath>
#include <string>

using namespace fx;

namespace {
const std::uint8_t kV1110Record[] = {
#include "fixtures/workspace_v1_11_0_run_matrix.inc"
};
// Record v8 della v1.12.0 (rilascio di EQUATION su main), generato con quei sorgenti.
const std::uint8_t kV1120Record[] = {
#include "fixtures/workspace_v1_12_0.inc"
};
const std::string kArrow = "\xE2\x86\x92";
const std::string kDisp = "\xE2\x97\xA2";
std::string calc(Workspace& w, const std::string& t) { runCalculation(w, t); return resultText(w.history.back(), w.settings); }
std::string last(Workspace& w) { return resultText(w.history.back(), w.settings); }
template <typename E> bool fails(Workspace& w, const std::string& t) {
    try { runCalculation(w, t); } catch (const E&) { return true; } catch (...) { return false; }
    return false;
}
} // namespace

int main() {
    board::init();
    // ================================================================ ":" e "◢" (manuale 2-9)
    {
        Workspace w;
        CHECK_EQ_STR(calc(w, "3" + kArrow + "A:A\xC3\x97" "2"), "6");
        CHECK(w.memory.recall('A') == 3 && w.history.size() == 1);
        CHECK_EQ_STR(calc(w, "5" + kDisp + "Ans+1"), "6");
        CHECK(w.history.back().displays.size() == 1 && w.history.back().displays[0] == "5");
        CHECK_EQ_STR(calc(w, "1:2:3"), "3");
        // L'errore indica la posizione nella riga intera
        try { runCalculation(w, "1+2:3+"); CHECK(false); }
        catch (const SyntaxError& e) { CHECK(e.where.position == 6); }
        CHECK_EQ_STR(calc(w, "{1,2}:Sum(List Ans)"), "3");
        CHECK_EQ_STR(calc(w, "Text 1,1,\"a:b\""), "Done");                         // ':' tra virgolette
        // Ricalcolo della cronologia con istruzioni multiple
        runCalculation(w, "2" + kArrow + "B:B+1");
        CHECK(editHistoryEntry(w, w.history.size() - 1, "4" + kArrow + "B:B+1") == 0);
        CHECK_EQ_STR(last(w), "5");
    }
    // ================================================================ stringhe, Str, function memory
    {
        Workspace w;
        CHECK_EQ_STR(calc(w, "\"HELLO\""), "HELLO");
        CHECK_EQ_STR(calc(w, "\"AB\"" + kArrow + "str_1"), "AB");
        CHECK(w.strings[0] == "AB");
        CHECK_EQ_STR(calc(w, "str_1"), "AB");
        CHECK(fails<TypeError>(w, "\"X\"" + kArrow + "A"));
        CHECK(fails<TypeError>(w, "3" + kArrow + "str_2"));
        CHECK_EQ_STR(calc(w, "x^2+1" + kArrow + "f_1"), "Done");                     // FUNCMEM STORE
        CHECK(w.functionMemories[0] == "x^2+1");
        runCalculation(w, "2" + kArrow + "X");
        CHECK_EQ_STR(calc(w, "f_1"), "5");                                           // con X
        CHECK_EQ_STR(calc(w, "f_1(3)"), "10");                                       // con l'argomento
        CHECK_EQ_STR(calc(w, "f_1+f_1(0)"), "6");
        CHECK(fails<MathError>(w, "f_2"));
        CHECK(fails<SyntaxError>(w, "x^" + kArrow + "f_3"));                          // sintassi verificata
        w.setFunctionMemory(1, "f_2+1");                                              // ricorsione infinita
        CHECK(fails<MathError>(w, "f_2"));
    }
    // ================================================================ r, θ e variabili di sistema (VARS)
    {
        Workspace w;
        CHECK_EQ_STR(calc(w, "4" + kArrow + "theta"), "4");
        CHECK_EQ_STR(calc(w, "theta+1"), "5");
        CHECK_EQ_STR(calc(w, "1/3" + kArrow + "r_var"), "1/3");
        CHECK_EQ_STR(calc(w, "3r_var"), "1");                                        // forma esatta conservata
        CHECK(w.memory.recall('R') == 0);                                            // r non e' R
        CHECK_EQ_STR(calc(w, "10" + kArrow + "xmax"), "10");
        CHECK(w.view.xmax == 10);
        CHECK(fails<MathError>(w, "-100" + kArrow + "xmax"));                        // Range ERROR, nulla cambia
        CHECK(w.view.xmax == 10);
        CHECK_EQ_STR(calc(w, "3" + kArrow + "xfct"), "3");
        CHECK(w.graph.settings.xFactor == 3);
        CHECK_EQ_STR(calc(w, "-2" + kArrow + "tblstart"), "-2");
        CHECK(w.table.start == -2);
        CHECK_EQ_STR(calc(w, "2" + kArrow + "d_start"), "2");
        CHECK_EQ_STR(calc(w, "d_start+d_end"), "7");                                 // D End predefinito 5
        CHECK_EQ_STR(calc(w, "5" + kArrow + "r_end"), "5");
        CHECK(fails<MathError>(w, "2.5" + kArrow + "r_end"));
        CHECK_EQ_STR(calc(w, "3" + kArrow + "rec_a0"), "3");
        CHECK_EQ_STR(calc(w, "1000" + kArrow + "tvm_pv"), "1000");
        CHECK_EQ_STR(calc(w, "tvm_py"), "12");
        CHECK(fails<MathError>(w, "0" + kArrow + "tvm_cy"));
        CHECK(fails<SyntaxError>(w, "1" + kArrow + "stat_n"));                       // i risultati STAT non si assegnano
        CHECK_EQ_STR(calc(w, "\"2rec_an+1\"" + kArrow + "rec_an1"), "2rec_an+1");    // formula an+1
        CHECK(w.modeVars.recFormula == "2rec_an+1");
    }
    // ================================================================ List "nome", List File, Dim, Fill
    {
        Workspace w;
        w.setListName(2, "QTY");
        runCalculation(w, "{1,2,3}" + kArrow + "List 3");
        CHECK_EQ_STR(calc(w, "List \"QTY\"\xC3\x97" "2"), "{2,4,6}");
        CHECK_EQ_STR(calc(w, "List \"QTY\"[2]"), "2");
        CHECK_EQ_STR(calc(w, "{9}" + kArrow + "List \"QTY\""), "{9}");
        CHECK(w.lists[2] == std::vector<double>({9}));
        CHECK(fails<DimensionError>(w, "List \"NONE\""));
        CHECK(fails<MathError>(w, "{1}" + kArrow + "List \"NONE\""));
        CHECK_EQ_STR(calc(w, "4" + kArrow + "Dim List 5"), "4");                     // n->Dim List: n zeri
        CHECK(w.lists[4] == std::vector<double>(4, 0.0));
        CHECK(fails<DimensionError>(w, "0" + kArrow + "Dim List 5"));
        CHECK_EQ_STR(calc(w, "Fill(7,List 5)"), "{7,7,7,7}");
        CHECK(w.lists[4] == std::vector<double>(4, 7.0));                            // Fill cambia la lista
        runCalculation(w, "[[1,2][3,4]]" + kArrow + "Mat A");
        runCalculation(w, "Fill(0,Mat A)");
        CHECK(w.matrices.mat[0].re == std::vector<double>(4, 0.0));
        // List File 1-6: ogni file ha 26 liste e i suoi sottonomi
        w.selectListFile(1);
        CHECK(w.settings.listFile == 1 && w.lists[2].empty() && w.listName(2).empty());
        runCalculation(w, "{5,6}" + kArrow + "List 1");
        w.selectListFile(0);
        CHECK(w.lists[2] == std::vector<double>({9}) && w.listName(2) == "QTY" && w.lists[0].empty());
        CHECK(w.listFiles[1].lists[0] == std::vector<double>({5, 6}));
        CHECK(w.listElements() == 1 + 4 + 2);                                         // budget comune ai file
        CHECK(fails<SyntaxError>(w, "1" + kArrow + "Dim List 0"));
        CHECK_THROWS(w.selectListFile(6));
        w.selectListFile(1);
        CHECK(w.lists[0] == std::vector<double>({5, 6}));
        // Il budget di tutte le liste vale per tutti i file insieme
        CHECK(!w.listFits(9, Workspace::MaxListElements));
    }
    // ================================================================ Swap, *Row, *Row+, Row+
    {
        Workspace w;
        runCalculation(w, "[[1,2][3,4]]" + kArrow + "Mat A");
        CHECK_EQ_STR(calc(w, "Swap A,1,2"), "Done");
        CHECK_EQ_STR(calc(w, "Mat A"), "[[3,4][1,2]]");
        runCalculation(w, "*Row 2,A,1");
        CHECK_EQ_STR(calc(w, "Mat A"), "[[6,8][1,2]]");
        runCalculation(w, "*Row+ -6,A,2,1");
        CHECK_EQ_STR(calc(w, "Mat A"), "[[0,-4][1,2]]");
        runCalculation(w, "Row+ A,2,1");
        CHECK_EQ_STR(calc(w, "Mat A"), "[[1,-2][1,2]]");
        CHECK(fails<DimensionError>(w, "Swap A,1,3"));
        CHECK(fails<DimensionError>(w, "Swap Q,1,2"));
        CHECK(fails<SyntaxError>(w, "Swap A,1"));
    }
    // ================================================================ forme esatte delle matrici (oltre 64 elementi)
    {
        Workspace w;
        runCalculation(w, "Fill(1/3,Identity(9))" + kArrow + "Mat A");               // 81 elementi
        CHECK(w.matrices.mat[0].hasExact());
        runCalculation(w, "Mat A\xC3\x97" "3");
        CHECK(w.matrices.matAns.hasExact() && w.matrices.matAns.re[80] == 1);
        CHECK_EQ_STR(calc(w, "Mat A[9,9]"), "1/3");
        // Celle digitate come decimali restano decimali (manuale 2-43)
        runCalculation(w, "{1,2}" + kArrow + "Dim Mat B");
        CalcValue& b = w.matrices.mat[1];
        b.setExact(0, exact::Value::fromRational({1, 4}), true);
        b.setExact(1, exact::Value::fromRational({1, 3}), false);
        CHECK(b.decimalAt(0) && !b.decimalAt(1));
        const auto blob = serializeWorkspace(w);
        Workspace back;
        CHECK(blob[4] == 11 && deserializeWorkspace(blob.data(), blob.size(), back));
        CHECK(back.matrices.mat[1].decimalAt(0) && !back.matrices.mat[1].decimalAt(1));
        CHECK(back.matrices.mat[0].hasExact() && back.matrices.mat[0].size() == 81);
        CHECK_EQ_STR(calc(back, "Mat B[1,1]\xC3\x97" "4"), "1");
        CHECK_EQ_STR(calc(back, "Mat B"), "[[0.25,0.3333333333]]");                  // un decimale rende decimale il risultato
        CHECK(limits::kExactStoreElements >= 400);
    }
    // ================================================================ VARS EQUATION e TABLE Result
    {
        Workspace w;
        // VARS › EQUATION dal menu di Run-Matrix (i valori li verifica equation_v112_test):
        // senza coefficienti la fx-CG50 da' un errore; il token e' quello di EQUATION.
        CHECK(fails<MathError>(w, "SimCoef"));
        CHECK(fails<MathError>(w, "PlyRes"));
        CHECK(std::string(equationMatrixName(0)) == "SimRes");
        w.functions[0].expr = "x^2"; w.functions[0].type = GraphType::Y; w.functions[0].selected = true;
        for (std::size_t i = 1; i < w.functions.size(); ++i) w.functions[i].selected = false;
        w.table.start = 1; w.table.end = 3; w.table.step = 1;
        CHECK_EQ_STR(calc(w, "tbl_result"), "[[1,1][2,4][3,9]]");
    }
    // ================================================================ migrazione del record v8 (v1.12.0)
    {
        Workspace w;
        CHECK(kV1120Record[4] == 8);
        CHECK(deserializeWorkspace(kV1120Record, sizeof(kV1120Record), w));
        CHECK(w.settings.angle == AngleMode::Degrees);
        exact::Value a; bool dec = false;
        CHECK(w.memory.exactRecall('A', a, dec) && a == exact::Value::fromRational({1, 3}));
        CHECK(w.matrices.mat[0].rows == 2 && w.matrices.mat[0].re[3] == 4);
        CHECK(w.lists[2] == std::vector<double>({1, 2, 3}) && w.listName(2) == "QTY");
        CHECK(w.history.size() == 5 && w.functions[0].expr == "x^2-1");
        CHECK(w.equation.unknowns == 2 && w.equation.degree == 2 && w.equation.solverExpr == "x^2=2");
        // VARS › EQUATION dal record migrato
        CHECK_EQ_STR(calc(w, "SimRes"), "[[2][1]]");
        CHECK_EQ_STR(calc(w, "SimCoef"), "[[1,1,3][1,-1,1]]");
        CHECK_EQ_STR(calc(w, "PlyCoef"), "[[1,0,-4]]");
        // I dati nuovi della v1.13.0 hanno i valori iniziali
        CHECK(w.settings.listFile == 0 && w.strings[0].empty() && w.functionMemories[0].empty());
        CHECK(w.listFiles[1].elements() == 0 && w.modeVars.tvmPY == 12 && w.modeVars.tvmCY == 12);
        // Salvato di nuovo: versione 9, si rilegge con gli stessi dati
        const auto blob = serializeWorkspace(w);
        Workspace again;
        CHECK(blob[4] == 11 && deserializeWorkspace(blob.data(), blob.size(), again));
        CHECK(again.equation.simResult.size() == 2 && again.lists[2].size() == 3 && again.history.size() == w.history.size());
    }
    // ================================================================ migrazione del record v7 (v1.11.0)
    {
        Workspace w;
        CHECK(kV1110Record[4] == 7);
        CHECK(deserializeWorkspace(kV1110Record, sizeof(kV1110Record), w));
        CHECK(w.settings.angle == AngleMode::Degrees && w.settings.simplifyManual);
        CHECK(w.settings.csvSeparator == ';' && w.settings.csvDecimal == ',');
        CHECK(w.settings.statXList == 2 && w.settings.statYList == 3);
        CHECK(w.settings.listFile == 0 && w.settings.subName && w.settings.statRegType == 0xFF);   // predefiniti v8
        CHECK(w.listName(2) == "QTY" && w.listName(25) == "PRICE");
        CHECK(w.lists[2] == std::vector<double>({1, 2, 3}) && w.lists[25] == std::vector<double>({10, 20, 30}));
        CHECK(w.matrices.mat[0].hasExact() && !w.matrices.mat[0].anyDecimal());
        CHECK(w.memory.recall('C') == 7);
        CHECK(w.history.size() == 10);
        CHECK_EQ_STR(resultText(w.history[0], w.settings), "15/60");
        CHECK_EQ_STR(resultText(w.history[1], w.settings), "5/20  F=3");
        CHECK_EQ_STR(resultText(w.history[6], w.settings), "100");                    // Hex FF+1
        CHECK(w.history[7].command && w.graph.sketch.size() == 1 && w.graph.commandGraphs.size() == 1);
        CHECK(w.functions[0].expr == "x^2-1");
        CHECK(w.strings[0].empty() && w.functionMemories[0].empty() && w.modeVars.tvmPY == 12);
        // Riscritto come v8 e riletto senza perdite
        const auto blob = serializeWorkspace(w);
        Workspace again;
        CHECK(blob[4] == 11 && deserializeWorkspace(blob.data(), blob.size(), again));
        CHECK(serializeWorkspace(again) == blob);
    }
    // ================================================================ record v8: tutti i campi nuovi
    {
        Workspace w;
        w.settings.subName = false;
        w.settings.statRegType = 3;
        runCalculation(w, "2" + kArrow + "theta");
        runCalculation(w, "1/7" + kArrow + "r_var");
        runCalculation(w, "\"PICO\"" + kArrow + "str_20");
        runCalculation(w, "sin(x)" + kArrow + "f_20");
        runCalculation(w, "9" + kArrow + "tvm_n");
        runCalculation(w, "\"rec_an+rec_n\"" + kArrow + "rec_an1");
        runCalculation(w, "5" + kDisp + "6");
        runCalculation(w, "\"TXT\"");
        runCalculation(w, "1234");
        CHECK(shiftEngineering(w.history.back(), 1));
        w.selectListFile(4);
        runCalculation(w, "{4,4}" + kArrow + "List 2");
        w.setListName(1, "F5");
        w.selectListFile(0);
        const auto blob = serializeWorkspace(w);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
        CHECK(!back.settings.subName && back.settings.statRegType == 3);
        CHECK(back.memory.recall(CalculatorMemory::ThetaSlot) == 2);
        exact::Value r; bool d = false;
        CHECK(back.memory.exactRecall(CalculatorMemory::RSlot, r, d) && r == exact::Value::fromRational({1, 7}));
        CHECK(back.strings[19] == "PICO" && back.functionMemories[19] == "sin(x)" && back.modeVars.tvmN == 9);
        CHECK(back.modeVars.recFormula == "rec_an+rec_n");
        CHECK(back.history[6].displays.size() == 1 && back.history[7].text == "TXT");
        CHECK_EQ_STR(resultText(back.history[8], back.settings), "1.234E+03");
        CHECK(back.listFiles[4].lists[1] == std::vector<double>({4, 4}) && back.listFiles[4].names[1] == "F5");
        // Record corrotti: file attivo con dati nello slot, versione futura
        auto bad = blob; bad[4] = 12;                      // v1.15.0: la versione corrente e' 11
        CHECK(!deserializeWorkspace(bad.data(), bad.size(), back));
        for (std::size_t cut : {blob.size() - 1, blob.size() - 40, blob.size() / 2}) {
            Workspace t;
            CHECK(!deserializeWorkspace(blob.data(), cut, t));
        }
    }
    // ================================================================ interfaccia di Run-Matrix
    {
        testrig::Rig r;
        auto& ws = r.ws();
        r.key(pckey::Tab); r.type("1");                                               // Run-Matrix
        auto* rm = r.active<app::RunMatrixApp>();
        CHECK(rm != nullptr);
        // OPTN: prima voce LIST, seconda MAT/VCT... (ordine della fx-CG50)
        r.key(pckey::F8);
        r.type("4");                                                                  // CALC
        for (int i = 0; i < 4; ++i) r.key(pckey::Down);                               // SolveN(
        r.key(pckey::Enter);
        CHECK_EQ_STR(rm->editor().toExpression(), "solve_n(");
        r.type("x^2"); r.key(pckey::Right); r.type("-1"); r.key(pckey::Enter);   // ^ apre il modello della potenza
        CHECK_EQ_STR(last(ws), "{-1,1}");
        // OPTN > ESYM > ENG sul risultato
        r.enter("1234");
        r.key(pckey::F8); for (int i = 0; i < 10; ++i) { r.key(pckey::Down); }
        r.key(pckey::Enter);   // ESYM
        for (int i = 0; i < 11; ++i) { r.key(pckey::Down); }
        r.key(pckey::Enter);                    // ENG
        CHECK_EQ_STR(last(ws), "1.234E+03");
        // OPTN > CONVERT > LENGTH > [cm] e poi [in]
        r.type("10");
        r.key(pckey::F8); r.type("6"); r.type("1");                                   // CONVERT, LENGTH
        for (int i = 0; i < 4; ++i) { r.key(pckey::Down); }
        r.key(pckey::Enter);          // [cm]
        r.key(pckey::F8); r.type("6"); r.type("1");
        for (int i = 0; i < 11; ++i) { r.key(pckey::Down); }
        r.key(pckey::Enter);         // [in]
        CHECK_EQ_STR(rm->editor().toExpression(), "10[cm]\xE2\x96\xB6[in]");
        r.key(pckey::Enter);
        CHECK(std::abs(ws.history.back().value.real() - 3.937007874) < 1e-9);
        // VARS > STAT, VARS > Str, VARS > A~Z in Hex (variabile v_a)
        ws.setList(0, {2, 4});
        r.key(pckey::F9); r.type("5"); r.type("2"); r.key(pckey::Enter);            // STAT > x̄
        CHECK_EQ_STR(last(ws), "3");
        ws.settings.baseMode = Settings::BaseMode::Hex;
        r.key(pckey::F9); r.type("1"); r.key(pckey::Enter);                          // A
        CHECK_EQ_STR(rm->editor().toExpression(), "v_a");
        r.key(pckey::Esc);
        ws.settings.baseMode = Settings::BaseMode::Comp;
        // FUNCMEM STORE f1 dall'input, poi fn
        r.type("x+1");
        r.key(pckey::F8); for (int i = 0; i < 12; ++i) { r.key(pckey::Down); }
        r.key(pckey::Enter);  // FUNCMEM
        r.type("1"); r.type("1");                                                      // STORE, f1
        CHECK(ws.functionMemories[0] == "x+1");
        r.key(pckey::Esc);
        // CATALOG (SHIFT 4): elenco alfabetico, inserisce il comando
        r.key(pckey::F7); r.type("4");
        CHECK(rm->menuOpen());
        r.key(pckey::Enter);                                                          // prima voce: *Row (ordine alfabetico)
        CHECK_EQ_STR(rm->editor().toExpression(), "*Row ");
        r.key(pckey::Esc);
        // CAPTURE (SHIFT 7) e RclCapt
        r.key(pckey::F7); r.type("7");
        CHECK(rm->menuOpen());
        r.type("3");                                                                  // Capture 3
        CHECK(!r.error());
        r.enter("RclCapt 3");
        CHECK(r.active<app::GraphApp>() != nullptr && ws.graph.overlay.find("CAPT03") != std::string::npos);
        r.key(pckey::Esc);
    }
    return testutil::finish("run_matrix_v112_test");
}
