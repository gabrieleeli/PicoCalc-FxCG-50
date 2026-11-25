// v1.20.0: flusso dell'interfaccia di Program con il firmware vero (app_rig,
// solo Pimoroni Pico Plus 2 W): migrazione dei programmi della v1.17.0 al
// primo avvio, gestore dei file (NEW, RENAME, COPY, MOVE, DEL, DEL-ALL,
// SEARCH, SORT, PASS), editor (FIND, REPLACE, UNDO/REDO, JUMP, SAVE), RUN con
// l'input e la fine dell'input (Ctrl+D), DEBUG con un punto d'interruzione,
// LIB (LOCAL dal repository copiato sulla SD, INFO, VERIFY, DEL) e SD assente.
#include "app_rig.hpp"
#include "file_store.hpp"
#include "code_store.hpp"
#include "program_mode.hpp"
#include "test_util.hpp"
#include <filesystem>
#include <fstream>

using namespace fx;
using namespace fx::app;
namespace pk = fx::pckey;

namespace {
template <class A> std::string screen(testrig::Rig& r) { A* a = r.active<A>(); return a ? a->screenName() : "-"; }
files::RamFileSystem& card() { return static_cast<files::RamFileSystem&>(board::host::ramCard()); }
void put(files::FileSystem& fs, const std::string& path, const std::string& text) {
    const std::size_t slash = path.rfind('/');
    if (slash != std::string::npos) (void)fs.makeDirs(path.substr(0, slash));
    CHECK(files::writeAtomic(fs, path, reinterpret_cast<const std::uint8_t*>(text.data()), text.size()) == files::Status::Ok);
}
std::string get(const std::string& path) {
    std::vector<std::uint8_t> d;
    if (files::readAll(card(), path, d, 8u << 20) != files::Status::Ok) return "<assente>";
    return std::string(d.begin(), d.end());
}
bool exists(const std::string& path) { files::Entry e; return card().stat(path, e) == files::Status::Ok; }
void ticks(testrig::Rig& r, int n, std::uint32_t ms = 30) {
    for (int i = 0; i < n; ++i) { board::host::advance(ms); if (r.fw.step()) { r.fw.draw(board::graphics()); board::present(); } }
}
std::string toast(testrig::Rig& r) { return r.fw.apps().context().toastText(); }
void ctrl(testrig::Rig& r, char c) {
    r.raw(testrig::press(pk::ModCtrl, 1));
    r.raw(testrig::press(static_cast<std::uint8_t>(c)));
    r.raw(testrig::press(pk::ModCtrl, 3));
}
void clearField(testrig::Rig& r, int n = 48) { for (int i = 0; i < n; ++i) r.key(pk::Backspace); }
void page(testrig::Rig& r, int pages) { for (int i = 0; i < pages; ++i) r.key(pk::F6); }   // ▷
void copyRepository(const std::string& to) {
    for (const auto& e : std::filesystem::recursive_directory_iterator(FXAI_LIBRARIES_DIR)) {
        if (!e.is_regular_file()) continue;
        std::ifstream in(e.path(), std::ios::binary);
        put(card(), to + "/" + std::filesystem::relative(e.path(), FXAI_LIBRARIES_DIR).generic_string(),
            std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
    }
}
} // namespace

int main() {
    // ---- migrazione: programmi della v1.17.0 nella flash interna, proposta al primo ingresso
    auto r = std::make_unique<testrig::Rig>();
    files::FileSystem* flash = board::internalFileSystem();
    CHECK(flash != nullptr);
    put(*flash, "PICOCALC/PROG/VECCHIO.pcprog", progmode::encodeProgram(prog::Dialect::C, "int main(void) { return 0; }\n"));
    put(*flash, "PICOCALC/PY/grafico.py", "print(1)\n");
    r->type("b");
    CHECK(r->fw.apps().activeId() == AppId::Program && screen<ProgramApp>(*r) == "list");
    CHECK(!r->ws().program.migrationAsked);
    r->key(pk::Enter);                                                      // menu: converti, originali tenuti
    CHECK(get("PICOCALC/CODE/C/VECCHIO.c") == "int main(void) { return 0; }\n" && exists("PICOCALC/CODE/PY/grafico.py"));
    CHECK(r->ws().program.migrationAsked && toast(*r).find("Convertiti 2") != std::string::npos);
    files::Entry fe;
    CHECK(flash->stat("PICOCALC/PROG/VECCHIO.pcprog", fe) == files::Status::Ok);   // originali tenuti
    r->key(pk::Esc);
    r->type("b");                                                           // riaperta: niente seconda proposta
    CHECK(screen<ProgramApp>(*r) == "list");

    // ---- NEW > File in CODE/CPP, editor: FIND, REPLACE, UNDO, REDO, JUMP, SAVE, RUN
    r->key(pk::Home); r->key(pk::Down); r->key(pk::Enter);                  // CODE/CPP
    r->key(pk::F3); r->key(pk::Enter);                                      // NEW > File
    r->enter("CONTI");
    CHECK(screen<ProgramApp>(*r) == "edit" && exists("PICOCALC/CODE/CPP/CONTI.cpp"));
    r->key(pk::F4); r->enter("Ciao"); r->enter("Salve");                    // REPL Ciao -> Salve
    CHECK(toast(*r) == "1 sostituzioni");
    r->key(pk::F2);                                                         // SAVE
    CHECK(get("PICOCALC/CODE/CPP/CONTI.cpp").find("Salve") != std::string::npos);
    page(*r, 1); r->key(pk::F1);                                            // UNDO
    page(*r, 2); r->key(pk::F2);                                            // SAVE
    CHECK(get("PICOCALC/CODE/CPP/CONTI.cpp").find("Ciao") != std::string::npos);
    page(*r, 1); r->key(pk::F2);                                            // REDO
    page(*r, 2); r->key(pk::F2);
    CHECK(get("PICOCALC/CODE/CPP/CONTI.cpp").find("Salve") != std::string::npos);
    r->key(pk::F3); r->enter("nessuno");                                    // FIND non trovato
    CHECK(toast(*r).find("non trovato") != std::string::npos);
    r->key(pk::F5); r->enter("99");                                         // JUMP oltre la fine
    CHECK(toast(*r).find("riga 99") != std::string::npos);
    // testo scritto con la tastiera: rientro automatico dopo '{'
    r->key(pk::F5); r->enter("3");                                          // int main() {
    r->key(pk::End); r->key(pk::Enter);
    r->type("fx::setVar(\"A\", 7);");
    r->key(pk::F1);                                                         // RUN (salva e esegue)
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "done" && r->ws().memory.recall('A') == 7);
    CHECK(get("PICOCALC/CODE/CPP/CONTI.cpp").find("    fx::setVar(\"A\", 7);") != std::string::npos);
    r->key(pk::Esc);
    CHECK(screen<ProgramApp>(*r) == "edit");
    r->key(pk::Esc);
    CHECK(screen<ProgramApp>(*r) == "list");

    // ---- file grande (3 MB, oltre i 256 KB dell'editor): a blocchi di 128 KB, modifica e salvataggio del solo blocco
    {
        std::string big;
        for (int i = 0; big.size() < 3u * 1024u * 1024u; ++i) big += "// riga " + std::to_string(i) + " del file grande\n";
        big += "// ultima riga senza a capo";
        put(card(), "PICOCALC/CODE/CPP/GRANDE.cpp", big);
        r->key(pk::Esc); r->key(pk::Enter);                                 // l'elenco si rilegge
        r->type("G"); r->key(pk::F2);                                       // EDIT
        CHECK(screen<ProgramApp>(*r) == "edit" && toast(*r).find("blocchi") != std::string::npos);
        page(*r, 2); r->key(pk::F2);                                        // BTM del blocco
        r->key(pk::PageDown);                                               // blocco seguente
        r->type("@");
        ctrl(*r, 's');                                                      // Ctrl+S
        const std::size_t first = big.rfind('\n', 128u * 1024u) + 1;        // il primo blocco finisce a fine riga
        const std::string want = big.substr(0, first) + "@" + big.substr(first);
        CHECK(get("PICOCALC/CODE/CPP/GRANDE.cpp") == want);
        // fino all'ultimo blocco: l'ultima riga resta senza a capo
        for (int i = 0; i < 40; ++i) { r->key(pk::F2); r->key(pk::PageDown); }
        r->key(pk::End);
        r->type("!");
        ctrl(*r, 's');
        CHECK(get("PICOCALC/CODE/CPP/GRANDE.cpp") == want + "!");
        r->key(pk::PageUp); r->key(pk::Home);
        page(*r, 1);                                                        // pagina 1
        r->key(pk::Esc);
        CHECK(screen<ProgramApp>(*r) == "list");
        CHECK(card().remove("PICOCALC/CODE/CPP/GRANDE.cpp") == files::Status::Ok);
        r->key(pk::Esc); r->key(pk::Enter);
    }

    // ---- input e fine dell'input (Ctrl+D)
    put(card(), "PICOCALC/CODE/CPP/SOMMA.cpp", "#include <iostream>\nint main() { int x, s = 0; while (std::cin >> x) s += x; fx::setVar(\"C\", s); return 0; }\n");
    r->key(pk::Esc); r->key(pk::Enter);                                     // l'elenco si rilegge
    r->type("S"); r->key(pk::Enter);                                        // salto alla lettera S, EXE = RUN
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "input");
    r->enter("4 5");
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "input");
    ctrl(*r, 'd');
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "done" && r->ws().memory.recall('C') == 9);
    r->key(pk::Esc);
    if (screen<ProgramApp>(*r) == "edit") r->key(pk::Esc);

    // ---- gestore dei file: RENAME, COPY, MOVE, SEARCH, SORT, DEL, DEL-ALL
    CHECK(screen<ProgramApp>(*r) == "list");
    r->type("C");                                                           // CONTI.cpp
    page(*r, 1); r->key(pk::F2);                                            // REN
    clearField(*r);
    r->enter("CASSA.cpp");
    CHECK(exists("PICOCALC/CODE/CPP/CASSA.cpp") && !exists("PICOCALC/CODE/CPP/CONTI.cpp"));
    page(*r, 2);                                                            // pagina 1
    r->key(pk::F3); r->key(pk::Down); r->key(pk::Down); r->key(pk::Enter);  // NEW > Cartella
    r->enter("Archivio");
    CHECK(exists("PICOCALC/CODE/CPP/Archivio"));
    r->type("C");                                                           // CASSA.cpp
    page(*r, 1); r->key(pk::F3);                                            // COPY
    r->key(pk::Up); r->key(pk::Enter);                                      // in CPP/Archivio (l'elenco e' circolare: Su = ultima)
    CHECK(exists("PICOCALC/CODE/CPP/Archivio/CASSA.cpp") && exists("PICOCALC/CODE/CPP/CASSA.cpp"));
    r->type("S");                                                           // SOMMA.cpp
    r->key(pk::F4);                                                         // MOVE
    r->key(pk::Up); r->key(pk::Enter);
    CHECK(exists("PICOCALC/CODE/CPP/Archivio/SOMMA.cpp") && !exists("PICOCALC/CODE/CPP/SOMMA.cpp"));
    r->key(pk::F1); r->enter("som");                                        // SEARCH "som"
    r->key(pk::Enter);                                                      // CPP/Archivio/SOMMA.cpp
    r->key(pk::F5);                                                         // SORT: tipo
    CHECK(toast(*r) == "Ordine: tipo" && r->ws().program.sort == 1);
    r->key(pk::F5); r->key(pk::F5);                                         // dimensione, nome
    CHECK(r->ws().program.sort == 0);
    page(*r, 2);                                                            // pagina 1
    r->type("S"); r->key(pk::F4);                                           // DEL SOMMA.cpp: No
    r->key(pk::Esc);
    CHECK(exists("PICOCALC/CODE/CPP/Archivio/SOMMA.cpp"));
    r->key(pk::F4); r->key(pk::F1);                                         // DEL: Si'
    CHECK(!exists("PICOCALC/CODE/CPP/Archivio/SOMMA.cpp") && exists("PICOCALC/CODE/CPP/Archivio/CASSA.cpp"));
    r->key(pk::F5); r->key(pk::F1);                                         // DEL-ALL in Archivio
    CHECK(!exists("PICOCALC/CODE/CPP/Archivio/CASSA.cpp") && exists("PICOCALC/CODE/CPP/Archivio"));
    r->key(pk::Esc);                                                        // su: CODE/CPP

    // ---- password: chiesta per EDIT, non per RUN
    r->type("C");                                                           // CASSA.cpp
    page(*r, 2); r->key(pk::F1);                                            // PASS
    r->enter("abc");
    CHECK(toast(*r).find("Password impostata") != std::string::npos);
    page(*r, 1);                                                            // pagina 1
    r->key(pk::F2); r->enter("zzz");                                        // EDIT con la password sbagliata
    CHECK(r->error() && r->errorDetail().find("password errata") != std::string::npos);
    r->closeError();
    r->key(pk::F2); r->enter("abc");
    CHECK(screen<ProgramApp>(*r) == "edit");
    r->key(pk::Esc);
    r->key(pk::F1);                                                         // RUN senza password
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "done");
    r->key(pk::Esc);
    if (screen<ProgramApp>(*r) == "edit") r->key(pk::Esc);

    // ---- DEBUG: punto d'interruzione, pausa, VARS, CONT
    put(card(), "PICOCALC/CODE/C/DBG.c", "#include <stdio.h>\nint main(void) {\n    int a = 2;\n    int b = a * 21;\n    fx_setvar(\"D\", b);\n    return 0;\n}\n");
    r->key(pk::Esc);                                                        // CODE
    r->key(pk::Home); r->key(pk::Enter);                                    // CODE/C
    r->type("D"); r->key(pk::F2);                                           // EDIT DBG.c
    CHECK(screen<ProgramApp>(*r) == "edit");
    r->key(pk::F5); r->enter("5");                                          // JUMP alla riga 5
    page(*r, 1); r->key(pk::F4);                                            // BRK
    CHECK(toast(*r).find("riga 5") != std::string::npos);
    r->key(pk::F5);                                                         // DEBUG
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "debug");
    r->key(pk::F4);                                                         // CONT fino al punto d'interruzione
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "debug");
    r->key(pk::F5);                                                         // VARS
    CHECK(screen<ProgramApp>(*r) == "vars");
    r->key(pk::Esc);
    CHECK(screen<ProgramApp>(*r) == "debug");
    r->key(pk::F1);                                                         // STEP
    r->settle();
    r->key(pk::F4);                                                         // CONT fino alla fine
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "done" && r->ws().memory.recall('D') == 42);
    r->key(pk::Esc);
    if (screen<ProgramApp>(*r) == "edit") r->key(pk::Esc);

    // ---- LIB: LOCAL dal repository copiato in LIB/IMPORT, INFO, VERIFY, DEL
    copyRepository("PICOCALC/LIB/IMPORT");
    r->key(pk::Esc);                                                        // CODE: tutti i linguaggi
    CHECK(screen<ProgramApp>(*r) == "list");
    page(*r, 2); r->key(pk::F2);                                            // LIB
    CHECK(screen<ProgramApp>(*r) == "lib");
    page(*r, 1); r->key(pk::F1);                                            // LOCAL
    r->key(pk::Down); r->key(pk::Down); r->key(pk::Enter);                  // c/trasformazioni
    r->key(pk::F1);                                                         // installare matrici e trasformazioni? Si'
    ticks(*r, 200, 5);
    r->settle();
    CHECK(exists("PICOCALC/LIB/C/trasformazioni/library.json") && exists("PICOCALC/LIB/C/matrici/matrici.c"));
    CHECK(!exists("PICOCALC/LIB/.tmp/journal"));
    page(*r, 1);                                                            // pagina 1
    r->key(pk::Home); r->key(pk::F1);                                       // INFO di matrici
    CHECK(screen<ProgramApp>(*r) == "info");
    r->key(pk::Esc);
    r->key(pk::F5);                                                         // VERIFY
    CHECK(screen<ProgramApp>(*r) == "info");
    r->key(pk::Esc);
    // la libreria si usa da un programma (prima la cartella del programma, poi LIB)
    put(card(), "PICOCALC/CODE/C/TRA.c", "#include <trasformazioni.h>\nint main(void) { double x = 1, y = 0; tr_applica(tr_traslazione(2, 3), &x, &y); fx_setvar(\"E\", x + y); return 0; }\n");
    r->key(pk::Esc);                                                        // gestore dei file
    r->key(pk::Home); r->key(pk::Enter);                                    // CODE/C
    r->type("T"); r->key(pk::Enter);
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "done" && r->ws().memory.recall('E') == 6);
    r->key(pk::Esc);
    if (screen<ProgramApp>(*r) == "edit") r->key(pk::Esc);
    // DEL di trasformazioni
    r->key(pk::Esc);
    page(*r, 2); r->key(pk::F2);                                            // LIB
    r->key(pk::End); r->key(pk::F4); r->key(pk::F1);                        // DEL dell'ultima (trasformazioni): Si'
    CHECK(!exists("PICOCALC/LIB/C/trasformazioni") && exists("PICOCALC/LIB/C/matrici"));
    r->key(pk::Esc);

    // ---- SD assente: messaggio chiaro, nessun crash
    card().setPresent(false);
    r->key(pk::Esc);
    r->type("b");
    CHECK(screen<ProgramApp>(*r) == "list");
    r->key(pk::F3);                                                         // NEW senza SD
    CHECK(r->error() && r->errorDetail().find("Inserire la scheda SD") != std::string::npos);
    r->closeError();
    card().setPresent(true);
    r->key(pk::Esc);
    r->type("b");
    CHECK(screen<ProgramApp>(*r) == "list" && !r->error());
    return testutil::finish("program_flow_test");
}
