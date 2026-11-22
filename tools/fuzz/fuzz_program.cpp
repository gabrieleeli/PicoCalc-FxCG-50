// v1.15.0 (F9) — Fuzzing dell'interprete C/C++/C#: compilazione di sorgenti
// arbitrari ed esecuzione a budget limitato (passi, memoria, input finito).
// Primo byte: dialetto. Nessun crash, nessuna ricorsione oltre i limiti.
#include "program.hpp"
#include "workspace.hpp"
#include <cstdint>
#include <string>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 1) return 0;
    static Workspace* ws = new Workspace();
    const auto dialect = static_cast<prog::Dialect>(d[0] % 3);
    const std::string src(reinterpret_cast<const char*>(d + 1), n - 1);
    try {
        auto program = prog::compile(src, dialect);
        prog::WorkspaceHost host(*ws);
        host.input = {"1 2 3", "abc", "4.5"};
        prog::Machine m(program, host, 20000);
        step::Budget budget(20000, 0, nullptr);
        for (int i = 0; i < 4 && m.state() != prog::Machine::State::Done && m.state() != prog::Machine::State::Failed; ++i) {
            (void)m.step(budget);
            if (budget.exhausted()) break;
        }
    } catch (const std::exception&) {
    }
    return 0;
}
