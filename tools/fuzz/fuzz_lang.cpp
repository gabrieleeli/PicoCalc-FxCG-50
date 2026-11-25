// v1.20.0 — Fuzzing del nucleo condiviso per un linguaggio (FUZZ_DIALECT: C, C++
// o C#; un eseguibile per linguaggio): preprocessore, lexer, parser, analisi,
// generazione del codice ed esecuzione con limiti stretti (heap, pila,
// chiamate, istruzioni), file confinati su una SD in RAM, input finito con la
// fine dell'input. Nessun crash, nessuna lettura fuori dai limiti (ASan/UBSan).
#include "file_store.hpp"
#include "program.hpp"
#include "workspace.hpp"
#include <cstdint>
#include <string>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n > 20000) return 0;
    static Workspace* ws = new Workspace();
    static files::RamFileSystem* fs = new files::RamFileSystem(1u << 20);
    const std::string src(reinterpret_cast<const char*>(d), n);
    try {
        auto program = prog::compile(src, static_cast<prog::Dialect>(FUZZ_DIALECT));
        prog::WorkspaceHost host(*ws);
        host.fs = fs;
        host.folder = "PICOCALC/CODE/FUZZ";
        (void)fs->makeDirs(host.folder);
        host.input = {"1 2 3", "abc", "4.5", "-7"};
        host.endOfInput = true;
        prog::Limits lim = prog::defaultLimits(false);
        lim.heapBytes = 256u * 1024u;
        lim.stackValues = 4096;
        lim.callDepth = 200;
        prog::Machine m(program, host, lim);
        m.runToEnd(300000);
    } catch (const std::exception&) {
    }
    return 0;
}
