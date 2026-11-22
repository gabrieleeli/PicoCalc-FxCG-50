// v1.15.0 (F8, solo Pimoroni Pico Plus 2 W): espressioni con variabili legate:
// BindingResolver componibile con i resolver del workspace, celle A1..Z999 e
// intervalli dallo stesso lexer, nomi legati delle successioni e dei
// programmi, dipendenze circolari rifiutate con "Circular ERROR".
#include "binding.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <map>

using namespace fx;
using namespace fx::bind;

int main() {
    Workspace ws;
    ws.memory.store('A', 10);
    ws.view.xmax = 7;
    // ---------------------------------------------------------------- F8.1 nomi legati e resolver padre
    {
        BindingResolver r(&ws, &ws);
        r.bind("rec_n", 3);
        r.bind("rec_an", 5);
        r.bind("rec_an1", 8);
        r.bind("tvm_pv", -1000);
        r.bind("rec_n", 4);                                     // aggiorna
        CHECK(r.bound("rec_n") && !r.bound("rec_bn"));
        const EvalContext ec = r.context(ws.evalContext());
        CHECK(evaluateReal(*parseExpression("rec_an1+rec_an*rec_n"), ec) == 28);
        CHECK(evaluateReal(*parseExpression("A+Xmax"), ec) == 17);                  // memoria e V-Window dal padre
        CHECK(evaluateReal(*parseExpression("tvm_pv"), ec) == -1000);              // il legato prima del padre
        CHECK(r.unbind("tvm_pv") && !r.unbind("tvm_pv"));
        CHECK(evaluateReal(*parseExpression("tvm_pv"), ec) == ws.modeVars.tvmPV);  // ora dal workspace
        CHECK_THROWS(evaluateReal(*parseExpression("rec_bn"), ec));                  // non legato: Math ERROR
        for (std::size_t i = r.bound("rec_n") ? 3 : 0; i < MaxBoundNames; ++i) r.bind("var_v" + std::to_string(i), 1);
        CHECK_THROWS(r.bind("var_overflow", 1));                                     // Memory ERROR
        // senza resolver legato gli stessi nomi non sono disponibili (un solo sistema di variabili)
        CHECK_THROWS(evaluateReal(*parseExpression("rec_an1"), ws.evalContext()));
    }
    // ---------------------------------------------------------------- F8.2 celle e intervalli dallo stesso lexer
    {
        std::map<std::pair<int, int>, double> sheet = {{{0, 1}, 2}, {{0, 2}, 3}, {{1, 1}, 10}, {{25, 999}, 1}};
        BindingResolver r(&ws, &ws);
        r.setCells([&](CellRef c, double& out) {
            const auto it = sheet.find({c.col, c.row});
            if (it == sheet.end()) return false;
            out = it->second;
            return true;
        });
        ParseOptions cells;
        cells.cellReferences = true;
        const EvalContext ec = r.context(ws.evalContext());
        CHECK(evaluateReal(*parseExpression("A1+A2*B1", cells), ec) == 32);
        CHECK(evaluateReal(*parseExpression("$A$1+A$2+$B1", cells), ec) == 15);                   // riferimenti assoluti
        CHECK(evaluateReal(*parseExpression("Z999", cells), ec) == 1);
        CHECK(evaluateReal(*parseExpression("Sum(A1:B2)", cells), ec) == 15);                     // intervallo come lista
        CHECK(evaluateReal(*parseExpression("Mean(A1:A2)", cells), ec) == 2.5);
        CHECK(evaluateReal(*parseExpression("A+A1", cells), ec) == 12);                           // la variabile A resta A
        CHECK_THROWS(evaluateReal(*parseExpression("C5", cells), ec));                             // cella vuota
        CHECK(evaluateReal(*parseExpression("A0", cells), ec) == 0);                               // A0 non e' una cella: A x 0
        CHECK_THROWS(parseExpression("A1:", cells));
        // senza l'opzione A1 resta A x 1 come in Run-Matrix
        CHECK(evaluateReal(*parseExpression("A1"), ws.evalContext()) == 10);
        CellRef a, b;
        CHECK(parseCellVariable("cell_b12", a) && a.col == 1 && a.row == 12 && cellVariable(a) == "cell_b12");
        CHECK(!parseCellVariable("cell_b0", a) && !parseCellVariable("cell_b1000", a) && !parseCellVariable("cellb1", a));
        CHECK(parseRangeVariable("range_a1_c3", a, b) && a.col == 0 && b.col == 2 && b.row == 3);
        const auto refs = cellReferences(*parseExpression("A1+Sum(B1:B3)*$C$2", cells));
        CHECK(refs.size() == 5);
    }
    // ---------------------------------------------------------------- nomi legati dei programmi (fx::calc)
    {
        BindingResolver r(&ws, &ws);
        r.bind("var_raggio", 2);
        r.bind("var_x", 3);
        std::vector<std::string> names = {"raggio", "x"};
        ParseOptions opt;
        opt.boundNames = &names;
        const EvalContext ec = r.context(ws.evalContext());
        CHECK(std::abs(evaluateReal(*parseExpression("\xCF\x80raggio^2", opt), ec) - 4 * 3.141592653589793) < 1e-12);
        CHECK(evaluateReal(*parseExpression("x^2+A", opt), ec) == 19);                             // x del programma, A della memoria
        CHECK(evaluateReal(*parseExpression("x"), ws.evalContext()) == ws.memory.recall('X'));     // senza: la memoria X
    }
    // ---------------------------------------------------------------- F8.3 dipendenze e Circular ERROR
    {
        DependencyGraph g(5);
        g.depend(0, 1); g.depend(1, 2); g.depend(3, 2);
        const auto order = g.order();
        auto pos = [&](std::size_t n) { return std::find(order.begin(), order.end(), n) - order.begin(); };
        CHECK(order.size() == 5 && pos(2) < pos(1) && pos(1) < pos(0) && pos(2) < pos(3));
        const auto aff = g.affected(2);
        CHECK(aff.size() == 4 && aff.front() == 2);                                                // 2, poi chi ne dipende
        g.depend(2, 0);                                                                            // ciclo 0 -> 1 -> 2 -> 0
        bool circular = false;
        try { (void)g.order(); } catch (const MathError& e) { circular = std::string(e.what()) == "Circular ERROR"; }
        CHECK(circular && g.cycleNode() <= 2);
        DependencyGraph self(1);
        self.depend(0, 0);                                                                         // "=A1" in A1 (manuale α)
        CHECK_THROWS_AS(self.order(), MathError);
        // grande e senza ricorsione
        DependencyGraph chain(20000);
        for (std::size_t i = 1; i < 20000; ++i) chain.depend(i, i - 1);
        const auto big = chain.order();
        CHECK(big.size() == 20000 && big.front() == 0 && big.back() == 19999);
        CHECK_THROWS(chain.depend(0, 20000));
    }
    return testutil::finish("binding_test");
}
