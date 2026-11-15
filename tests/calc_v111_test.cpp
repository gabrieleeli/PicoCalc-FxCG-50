// v1.11.0 — calcolo: simboli ingegneristici in input e visualizzazione Eng con
// i simboli, frazioni lineari ⌟, Simp manuale delle frazioni (con e senza
// fattore, Simplify Manual) e modalita' Base-N intera a 32 bit con gli
// operatori bit a bit (And, Or, Xor, Xnor, Not, Neg). Tutto passa dall'unico
// parser e dall'unico valutatore.
#include "run_matrix.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <cmath>
#include <string>

using namespace fx;

namespace {
const std::string kMark = "\xE2\x8C\x9F";      // ⌟
const std::string kSimp = "\xE2\x96\xB6Simp";  // ▶Simp
std::string last(Workspace& w) { return resultText(w.history.back(), w.settings); }
std::string lastLine(Workspace& w) { return resultText(w.history.back(), w.settings, true); }
double value(const std::string& text) { return evaluate(text); }
double baseValue(const std::string& text, unsigned base) {
    EvalContext c;
    c.integerBase = static_cast<std::uint8_t>(base);
    return evaluateReal(*parseExpression(text, base), c);
}
template <typename E> bool baseFails(const std::string& text, unsigned base) {
    try { (void)baseValue(text, base); } catch (const E&) { return true; } catch (...) { return false; }
    return false;
}
} // namespace

int main() {
    // ================================================================ simboli ingegneristici
    {
        CHECK_NEAR(value("2#k"), 2000, 0);
        CHECK_NEAR(value("3#M+1#G"), 1.003e9, 1e-3);
        CHECK_NEAR(value("5#m"), 0.005, 1e-18);
        CHECK_NEAR(value("4#u"), 4e-6, 1e-21);
        CHECK_NEAR(value("7#n"), 7e-9, 1e-24);
        CHECK_NEAR(value("8#p"), 8e-12, 1e-27);
        CHECK_NEAR(value("9#f") * 1e15, 9, 1e-12);
        CHECK_NEAR(value("1#T"), 1e12, 0);
        CHECK_NEAR(value("1/2#k"), 0.0005, 1e-18);           // postfisso: 1/(2k)
        CHECK_NEAR(value("(1+1)#k"), 2000, 0);
        CHECK_NEAR(value("2#k^2"), 4e6, 0);
        CHECK_THROWS_AS(parseExpression("#k"), SyntaxError);     // manca il valore
        CHECK_THROWS(parseExpression("2#q"));                    // simbolo sconosciuto
        // Valore esatto ma mostrato come un decimale (come un numero con la virgola)
        exact::Value v; bool decimal = false;
        EvalContext ctx;
        CHECK(evaluateExact(*parseExpression("5#m"), ctx, v, &decimal) && decimal && v == exact::Value::fromRational({1, 200}));
        CHECK(rewriteIdentifiers("2#k+x", [](const std::string&) { return std::string(); }) == "2#k+x");
        Workspace w;
        runCalculation(w, "999#k+25#k");
        CHECK_EQ_STR(last(w), "1024000");
        runCalculation(w, "5#m");
        CHECK_EQ_STR(last(w), "5E-03");                         // decimale (Norm1), S⇔D -> 1/200
        CHECK(toggleExactDecimal(w.history.back(), w.settings) && last(w) == "1/200");
    }
    // ================================================================ visualizzazione Eng con i simboli
    {
        const DisplayFormat eng{DisplayMode::Eng, 0};
        CHECK_EQ_STR(formatReal(1024000, eng), "1.024M");
        CHECK_EQ_STR(formatReal(0.9, eng), "900m");
        CHECK_EQ_STR(formatReal(4.7e-6, eng), "4.7\xC2\xB5");
        CHECK_EQ_STR(formatReal(3.3e-12, eng), "3.3p");
        CHECK_EQ_STR(formatReal(1e-15, eng), "1f");
        CHECK_EQ_STR(formatReal(2.5e12, eng), "2.5T");
        CHECK_EQ_STR(formatReal(25, eng), "25");                // 10^0: nessun simbolo
        CHECK_EQ_STR(formatReal(-1500, eng), "-1.5k");
        CHECK_EQ_STR(formatReal(1e-18, eng), "1E-18");          // oltre f..T resta ×10ⁿ
        CHECK_EQ_STR(formatReal(5e15, eng), "5E+15");
        Workspace w;
        w.settings.display = eng;
        runCalculation(w, "999#k+25#k");
        CHECK_EQ_STR(last(w), "1.024M");                        // esempio del manuale
        runCalculation(w, "9/10");
        CHECK_EQ_STR(last(w), "900m");                          // Eng: decimale
        CHECK(toggleExactDecimal(w.history.back(), w.settings) && last(w) == "9/10");   // S⇔D: forma esatta
        const auto tree = resultTree(w.history[0], w.settings);
        CHECK(tree && ast::serialize(*tree) == "1.024M");
    }
    // ================================================================ frazioni lineari ⌟
    {
        CHECK_NEAR(value("2" + kMark + "3"), 2.0 / 3.0, 1e-15);
        CHECK_NEAR(value("1" + kMark + "2" + kMark + "3"), 1 + 2.0 / 3.0, 1e-15);
        CHECK_NEAR(value("-1" + kMark + "4+1"), 0.75, 1e-15);
        CHECK_THROWS_AS(value("1" + kMark + "0"), MathError);
        CHECK_THROWS_AS(parseExpression("1" + kMark), SyntaxError);
        CHECK_THROWS_AS(parseExpression("1" + kMark + "2" + kMark + "3" + kMark + "4"), SyntaxError);
        CHECK_THROWS_AS(parseExpression("1.5" + kMark + "2"), SyntaxError);
        Workspace w;
        runCalculation(w, "1" + kMark + "3+1" + kMark + "6");
        CHECK_EQ_STR(last(w), "1/2");
        CHECK_EQ_STR(lastLine(w), "1" + kMark + "2");           // output Line
        runCalculation(w, "7" + kMark + "3");
        CHECK_EQ_STR(lastLine(w), "7" + kMark + "3");
        w.settings.mixedFractions = true;
        CHECK_EQ_STR(lastLine(w), "2" + kMark + "1" + kMark + "3");
        CHECK_EQ_STR(last(w), "2 1/3");
        runCalculation(w, "[[1/2,1][3,1/4]]");
        CHECK_EQ_STR(lastLine(w), "[[1" + kMark + "2,1][3,1" + kMark + "4]]");
        runCalculation(w, "\xE2\x88\x9A" "8");
        CHECK_EQ_STR(lastLine(w), "2\xE2\x88\x9A" "2");         // i radicali restano come sono
    }
    // ================================================================ Simp
    {
        Workspace w;
        runCalculation(w, "15/60" + kSimp);                    // senza fattore: il piu' piccolo primo comune
        CHECK_EQ_STR(last(w), "5/20  F=3");
        CHECK_EQ_STR(lastLine(w), "5" + kMark + "20  F=3");
        CHECK_NEAR(w.memory.answer(), 0.25, 1e-15);            // il valore non cambia
        runCalculation(w, "Ans" + kSimp);
        CHECK_EQ_STR(last(w), "1/4  F=5");
        runCalculation(w, kSimp);                              // ▶Simp da solo: Ans
        CHECK_EQ_STR(last(w), "1/4  F=1");                     // gia' irriducibile
        runCalculation(w, "15/60" + kSimp + "5");              // con fattore
        CHECK_EQ_STR(last(w), "3/12  F=5");
        runCalculation(w, "Ans" + kSimp + "3");
        CHECK_EQ_STR(last(w), "1/4  F=3");
        CHECK_THROWS_AS(runCalculation(w, "15/60" + kSimp + "7"), MathError);   // 7 non divide
        CHECK_THROWS_AS(runCalculation(w, "3" + kSimp), MathError);             // non e' una frazione
        CHECK_THROWS_AS(runCalculation(w, "\xE2\x88\x9A" "2" + kSimp), MathError);
        CHECK_THROWS_AS(runCalculation(w, "1/2" + kSimp + "x"), SyntaxError);
        runCalculation(w, "-(14/21)" + kSimp);
        CHECK_EQ_STR(last(w), "-2/3  F=7");
        CHECK_THROWS_AS(runCalculation(w, "35/49" + kSimp + "1"), MathError);   // il fattore e' almeno 2
        // S⇔D mostra il decimale e torna alla frazione
        runCalculation(w, "15/60" + kSimp);
        CHECK(toggleExactDecimal(w.history.back(), w.settings) && last(w) == "0.25");
        CHECK(toggleExactDecimal(w.history.back(), w.settings) && last(w) == "5/20  F=3");
        // Simplify Manual: la frazione resta come e' stata scritta
        Workspace m;
        m.settings.simplifyManual = true;
        runCalculation(m, "15/60");
        CHECK_EQ_STR(last(m), "15/60");
        runCalculation(m, "(15)/(60)");                         // modello del Math
        CHECK_EQ_STR(last(m), "15/60");
        runCalculation(m, "15" + kMark + "60");
        CHECK_EQ_STR(last(m), "15/60");
        runCalculation(m, "Ans" + kSimp);
        CHECK_EQ_STR(last(m), "5/20  F=3");
        runCalculation(m, "1/6+1/6");                           // altre operazioni: ridotte
        CHECK_EQ_STR(last(m), "1/3");
        m.settings.simplifyManual = false;
        runCalculation(m, "15/60");
        CHECK_EQ_STR(last(m), "1/4");
        // Persistenza e ricalcolo della cronologia
        const auto blob = serializeWorkspace(m);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
        CHECK(back.history[3].fracNum == 5 && back.history[3].fracDen == 20 && back.history[3].simpFactor == 3);
        CHECK_EQ_STR(resultText(back.history[3], back.settings), "5/20  F=3");
        CHECK(editHistoryEntry(m, 0, "30/60") == 0);
        CHECK_EQ_STR(resultText(m.history[0], m.settings), "1/2");    // Manual tornato Auto: ridotto
        CHECK_EQ_STR(resultText(m.history[3], m.settings), "1/4  F=1");   // Ans▶Simp ricalcolato
    }
    // ================================================================ Base-N: letterali e limiti
    {
        CHECK(baseValue("FF", 16) == 255 && baseValue("ff", 16) == 255 && baseValue("7FFFFFFF", 16) == 2147483647);
        CHECK(baseValue("FFFFFFFF", 16) == -1 && baseValue("80000000", 16) == -2147483648.0);
        CHECK(baseValue("1010", 2) == 10 && baseValue("11111111111111111111111111111111", 2) == -1);
        CHECK(baseValue("17", 8) == 15 && baseValue("37777777777", 8) == -1);
        CHECK(baseValue("2147483647", 10) == 2147483647 && baseValue("-2147483647-1", 10) == -2147483648.0);
        CHECK(baseFails<MathError>("2147483648", 10));                  // oltre il massimo decimale
        CHECK(baseFails<MathError>("100000000", 16));                   // oltre 32 bit
        CHECK(baseFails<SyntaxError>("12", 2));                         // cifra non valida
        CHECK(baseFails<SyntaxError>("8", 8));
        CHECK(baseFails<SyntaxError>("1.5", 10));                       // niente decimali
        CHECK(baseValue("0d10+0x10+0b10+0o10", 16) == 10 + 16 + 2 + 8);   // prefissi d h b o
        CHECK(baseValue("0d255", 2) == 255);
        // Aritmetica intera: divisione troncata, intervallo a 32 bit
        CHECK(baseValue("7/2", 10) == 3 && baseValue("-7/2", 10) == -3);
        CHECK(baseValue("7Rmdr 2", 10) == 1);
        CHECK(baseValue("2*3+4", 10) == 10);
        CHECK(baseFails<MathError>("7FFFFFFF+1", 16));
        CHECK(baseFails<MathError>("10000*1000000", 10));
        CHECK(baseFails<MathError>("1/0", 10));
        CHECK(baseFails<MathError>("-(-2147483647-1)", 10));
        // Operatori bit a bit (manuale: And, Or, Xor, Xnor, Not, Neg)
        CHECK(baseValue("1010 and 0110", 2) == 2);
        CHECK(baseValue("1010 or 0110", 2) == 14);
        CHECK(baseValue("1010 xor 0110", 2) == 12);
        CHECK(baseValue("1010 xnor 0110", 2) == -13);                  // ~(1100) a 32 bit
        CHECK(baseValue("not(0)", 16) == -1 && baseValue("not(FFFF)", 16) == -65536);
        CHECK(baseValue("neg(1)", 16) == -1 && baseValue("neg(FFFFFFFF)", 16) == 1);
        CHECK(baseFails<MathError>("neg(80000000)", 16));               // -(-2^31) oltre l'intervallo
        CHECK(baseFails<MathError>("sin(1)", 10));                     // funzioni scientifiche escluse
        CHECK(baseFails<SyntaxError>("2#k", 10));
        // Testo dei risultati
        CHECK_EQ_STR(baseNText(-1, 16), "FFFFFFFF");
        CHECK_EQ_STR(baseNText(-1, 2), "11111111111111111111111111111111");
        CHECK_EQ_STR(baseNText(-1, 8), "37777777777");
        CHECK_EQ_STR(baseNText(-1, 10), "-1");
        CHECK_EQ_STR(baseNText(255, 16), "FF");
        CHECK_EQ_STR(baseNText(0, 2), "0");
        // Nel calcolo normale And/Or/Xor/Xnor restano logici e Neg e' il segno
        CHECK(value("3 and 2") == 1 && value("0 xnor 0") == 1 && value("1 xnor 0") == 0 && value("neg(5)") == -5);
    }
    // ================================================================ Base-N in Run-Matrix
    {
        Workspace w;
        w.settings.baseMode = Settings::BaseMode::Hex;
        runCalculation(w, "FF+1");
        CHECK_EQ_STR(last(w), "100");
        CHECK(w.history.back().base == static_cast<std::uint8_t>(Settings::BaseMode::Hex));
        runCalculation(w, "0-1");
        CHECK_EQ_STR(last(w), "FFFFFFFF");
        // ▶Dec ▶Bin ▶Oct ▶Hex: la stessa riga in un'altra base
        HistoryEntry& h = w.history.back();
        h.view = ViewBin; CHECK_EQ_STR(last(w), "11111111111111111111111111111111");
        h.view = ViewOct; CHECK_EQ_STR(last(w), "37777777777");
        h.view = ViewDecimal; CHECK_EQ_STR(last(w), "-1");
        h.view = ViewAuto;
        runCalculation(w, "A\xE2\x86\x92" "G");                         // A = 10 e' una cifra; G una variabile
        CHECK(w.memory.recall('G') == 10);
        runCalculation(w, "G*2");
        CHECK_EQ_STR(last(w), "14");
        CHECK_THROWS_AS(runCalculation(w, "1.5"), SyntaxError);
        w.settings.baseMode = Settings::BaseMode::Bin;
        runCalculation(w, "1100 and 1010");
        CHECK_EQ_STR(last(w), "1000");
        w.settings.baseMode = Settings::BaseMode::Dec;
        runCalculation(w, "7/2");
        CHECK_EQ_STR(last(w), "3");
        // Ans resta un intero; il riuso di un risultato usa il prefisso decimale
        CHECK(w.memory.answer() == 3);
        CHECK_EQ_STR(resultExpression(w.history[1]), "-0d1");
        // Comp: la riga Base-N resta nella sua base e il ricalcolo la rilegge cosi'
        w.settings.baseMode = Settings::BaseMode::Comp;
        CHECK_EQ_STR(resultText(w.history[0], w.settings), "100");
        CHECK(editHistoryEntry(w, 3, "G*3") == 0);
        CHECK_EQ_STR(resultText(w.history[3], w.settings), "30");       // la riga modificata usa la modalita' corrente (Comp)
        CHECK_EQ_STR(resultText(w.history[4], w.settings), "1000");     // le successive restano nella loro base (Bin)
        CHECK_EQ_STR(resultText(w.history[5], w.settings), "3");
        runCalculation(w, "7/2");
        CHECK_EQ_STR(last(w), "7/2");                                   // di nuovo calcolo normale
        // Persistenza (record v7)
        const auto blob = serializeWorkspace(w);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
        CHECK(back.history[0].base == static_cast<std::uint8_t>(Settings::BaseMode::Hex) && resultText(back.history[1], back.settings) == "FFFFFFFF");
        w.settings.baseMode = Settings::BaseMode::Oct;
        const auto blob2 = serializeWorkspace(w);
        CHECK(deserializeWorkspace(blob2.data(), blob2.size(), back) && back.settings.baseMode == Settings::BaseMode::Oct);
    }
    return testutil::finish("calc_v111_test");
}
