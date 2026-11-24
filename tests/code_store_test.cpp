// v1.20.0: archivio dei programmi sulla microSD (PICOCALC/CODE/<LING>) e
// gestore dei file di Program. Una prova per voce delle tabelle SD/FM di
// docs/reports/ANALISI_FONTI_v1.20.0.md: cartelle per linguaggio, elenco con
// ordinamento, NEW file/progetto dai modelli, salvataggio atomico (file
// temporaneo, verifica, rinomina) con la SD tolta a meta' o piena, RENAME,
// COPY, MOVE, DEL, DEL-ALL, SEARCH, password, file grandi a blocchi,
// migrazione dei programmi della v1.17.0 e progetti per il compilatore.
#include "code_store.hpp"
#include "file_store.hpp"
#include "program.hpp"
#include "program_mode.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <memory>

using namespace fx;
using fx::code::Item;
using fx::code::Lang;
using fx::code::Store;
using fx::files::RamFileSystem;
using fx::files::Status;

namespace {
std::string text(RamFileSystem& fs, const std::string& path) {
    const auto* d = fs.data(path);
    return d ? std::string(d->begin(), d->end()) : std::string("<assente>");
}
std::vector<std::string> names(const std::vector<Item>& items) {
    std::vector<std::string> v;
    for (const Item& i : items) v.push_back(i.name);
    return v;
}
bool has(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }
const std::string Cpp = "PICOCALC/CODE/CPP";

// Esegue un progetto caricato dall'archivio (come RUN del gestore dei file).
std::string runProject(Store& st, const std::string& path, std::string* error = nullptr) {
    prog::Project p;
    std::unique_ptr<code::ProjectSources> src;
    std::string folder, err;
    if (!code::loadProject(st, path, p, src, folder, err)) { if (error) *error = err; return "LOAD[" + err + "]"; }
    Workspace ws;
    prog::WorkspaceHost host(ws);
    host.fs = st.fileSystem();
    host.folder = folder;
    try {
        auto prog = prog::compile(p);
        prog::Machine m(prog, host);
        m.runToEnd(5000000);
    } catch (const prog::Error& e) {
        if (error) *error = e.what();
        return host.output + "ERR[" + e.what() + "]";
    }
    return host.output;
}

// ---------------------------------------------------------------- SD-01..03
void testFolders() {
    RamFileSystem fs;
    Store st(&fs);
    CHECK(st.ready() == Status::Ok);
    for (const char* d : {"C", "CPP", "CS", "JAVA", "PY"}) CHECK(fs.exists(std::string("PICOCALC/CODE/") + d));
    CHECK_EQ_STR(code::rootOf(Lang::CSharp), "PICOCALC/CODE/CS");
    Lang l{};
    CHECK(code::langOfFile("a.CPP", l) && l == Lang::Cpp);
    CHECK(code::langOfFile("x.h", l) && l == Lang::C);
    CHECK(code::langOfFile("Main.java", l) && l == Lang::Java);
    CHECK(!code::langOfFile("note.txt", l));
    // nomi: niente '..', niente '/' o ':', niente '.' iniziale, al massimo 40 caratteri
    CHECK(code::validItemName("Gioco_2.cpp"));
    CHECK(!code::validItemName("../fuori.cpp"));
    CHECK(!code::validItemName("a/b.cpp"));
    CHECK(!code::validItemName(".nascosto"));
    CHECK(!code::validItemName("C:x"));
    CHECK(!code::validItemName(std::string(41, 'a')));
    // senza SD: messaggio chiaro, nessun crash
    RamFileSystem none;
    none.setPresent(false);
    Store ns(&none);
    CHECK(ns.ready() == Status::NoCard);
    CHECK_EQ_STR(code::message(ns.ready()), "Inserire la scheda SD");
    std::vector<Item> items;
    CHECK(ns.list(Cpp, items) == Status::NoCard);
    std::string path;
    CHECK(ns.newFile(Lang::Cpp, Cpp, "a", path) == Status::NoCard);
    Store nul(nullptr);
    CHECK(nul.ready() == Status::NoCard);
    CHECK(nul.writeText(Cpp + "/a.cpp", "x") == Status::NoCard);
    CHECK_EQ_STR(code::message(Status::Full), "SD piena");
}

// ---------------------------------------------------------------- FM-01 elenco e ordinamento
void testListAndSort() {
    RamFileSystem fs;
    Store st(&fs);
    (void)st.ready();
    CHECK(st.writeText(Cpp + "/zeta.cpp", std::string(10, 'z')) == Status::Ok);
    CHECK(st.writeText(Cpp + "/Alfa.cpp", std::string(300, 'a')) == Status::Ok);
    CHECK(st.writeText(Cpp + "/beta.h", std::string(50, 'b')) == Status::Ok);
    CHECK(st.makeFolder(Cpp + "/Progetto") == Status::Ok);
    CHECK(st.makeFolder(Cpp + "/Progetto") == Status::Exists);
    CHECK(st.makeFolder(Cpp + "/no..pe") == Status::InvalidName);
    // file temporanei e copie di sicurezza del salvataggio atomico non si vedono
    CHECK(st.writeText(Cpp + "/resto.cpp.tmp", "t") == Status::Ok);
    std::vector<Item> items;
    CHECK(st.list(Cpp, items, code::SortBy::Name) == Status::Ok);
    std::vector<std::string> n = names(items);
    CHECK(n.size() == 4);
    CHECK(!n.empty() && n[0] == "Progetto" && items[0].dir);            // cartelle prima
    CHECK(n.size() == 4 && n[1] == "Alfa.cpp" && n[2] == "beta.h" && n[3] == "zeta.cpp");
    CHECK(st.list(Cpp, items, code::SortBy::Size) == Status::Ok);
    n = names(items);
    CHECK(n.size() == 4 && n[1] == "Alfa.cpp" && n[2] == "beta.h" && n[3] == "zeta.cpp" && items[1].size == 300);
    CHECK(st.list(Cpp, items, code::SortBy::Type) == Status::Ok);
    n = names(items);
    CHECK(n.size() == 4 && n[1] == "Alfa.cpp" && n[2] == "zeta.cpp" && n[3] == "beta.h");   // .cpp < .h
}

// ---------------------------------------------------------------- FM-02 NEW dai modelli, progetti
void testNewAndTemplates() {
    RamFileSystem fs;
    Store st(&fs);
    (void)st.ready();
    std::string path;
    CHECK(st.newFile(Lang::Cpp, Cpp, "ciao", path) == Status::Ok);
    CHECK_EQ_STR(path, Cpp + "/ciao.cpp");
    CHECK(text(fs, path).find("int main()") != std::string::npos);
    CHECK(text(fs, path).find("// pcprog") == std::string::npos);       // sorgente standard, nessuna riga aggiunta
    CHECK(st.newFile(Lang::Cpp, Cpp, "ciao", path) == Status::Exists);
    CHECK(st.newFile(Lang::Cpp, Cpp, "../x", path) == Status::InvalidName);
    CHECK_EQ_STR(runProject(st, Cpp + "/ciao.cpp"), "Ciao\n");
    // C#: la classe prende il nome del file (con l'iniziale maiuscola)
    CHECK(st.newFile(Lang::CSharp, "PICOCALC/CODE/CS", "saluto", path) == Status::Ok);
    CHECK(text(fs, path).find("class Saluto") != std::string::npos);
    CHECK_EQ_STR(runProject(st, path), "Ciao\n");
    CHECK(st.newFile(Lang::C, "PICOCALC/CODE/C", "primo", path) == Status::Ok);
    CHECK_EQ_STR(runProject(st, path), "Ciao\n");
    CHECK(Store::templateFor(Lang::Java, "9vite", false).find("class P9vite") != std::string::npos);
    CHECK(Store::templateFor(Lang::Python, "x", false) == "print(\"Ciao\")\n");
    // progetti: una cartella con piu' file che si compilano insieme
    CHECK(st.newProject(Lang::Cpp, Cpp, "Gioco", path) == Status::Ok);
    CHECK(fs.exists(Cpp + "/Gioco/main.cpp") && fs.exists(Cpp + "/Gioco/util.h") && fs.exists(Cpp + "/Gioco/util.cpp"));
    CHECK_EQ_STR(runProject(st, Cpp + "/Gioco"), "Ciao da Gioco\n");
    CHECK(st.newProject(Lang::C, "PICOCALC/CODE/C", "Calcolo", path) == Status::Ok);
    CHECK_EQ_STR(runProject(st, path), "Ciao da Calcolo\n");
    CHECK(st.newProject(Lang::CSharp, "PICOCALC/CODE/CS", "Conti", path) == Status::Ok);
    CHECK_EQ_STR(runProject(st, path), "Ciao da Conti\n");
    CHECK(st.newProject(Lang::Cpp, Cpp, "Gioco", path) == Status::Exists);
    CHECK(st.newProject(Lang::Cpp, Cpp, "a.b", path) == Status::InvalidName);
    // un progetto che non entra nella SD non lascia cartelle a meta'
    RamFileSystem small(fs.used() + 2000);
    Store ss(&small);
    (void)ss.ready();
    CHECK(ss.newProject(Lang::Cpp, Cpp, "Grosso", path) == Status::Ok || !small.exists(Cpp + "/Grosso"));
}

// ---------------------------------------------------------------- SD-04/05 salvataggio atomico, SD tolta, SD piena
void testAtomicSave() {
    RamFileSystem fs;
    Store st(&fs);
    (void)st.ready();
    const std::string f = Cpp + "/dati.cpp";
    const std::string v1 = "int main() { return 1; }\n";
    CHECK(st.writeText(f, v1) == Status::Ok);
    // SD tolta durante il salvataggio (dopo 0, 5, 20... byte): il file vecchio resta intero
    const std::string v2 = "int main() { /* nuova versione piu' lunga */ return 2; }\n";
    for (std::size_t cut : {std::size_t(0), std::size_t(5), std::size_t(20), v2.size() - 1, v2.size() + 3, v2.size() * 2 + 10}) {
        fs.failAfterBytes(cut);
        const Status s = st.writeText(f, v2);
        fs.powerOn();
        std::vector<Item> items;
        (void)st.list(Cpp, items);                         // l'elenco completa o annulla l'operazione rimasta a meta'
        const std::string now = text(fs, f);
        CHECK(now == v1 || (now == v2 && s == Status::Ok));
        if (now == v2) CHECK(st.writeText(f, v1) == Status::Ok);
    }
    // ogni operazione che modifica la SD puo' fallire: mai un file troncato
    for (int n = 1; n <= 8; ++n) {
        fs.failOperation(n);
        const Status s = st.writeText(f, v2);
        fs.powerOn();
        std::vector<Item> items;
        (void)st.list(Cpp, items);
        const std::string now = text(fs, f);
        CHECK(now == v1 || now == v2);
        if (s != Status::Ok) CHECK(!code::message(s).empty());
        if (now == v2) CHECK(st.writeText(f, v1) == Status::Ok);
    }
    // SD piena: "SD piena", il file vecchio resta
    RamFileSystem full(1024 * 1024);
    Store sf(&full);
    (void)sf.ready();
    CHECK(sf.writeText(Cpp + "/p.cpp", v1) == Status::Ok);
    full.setCapacity(full.used() + 40);
    const Status s = sf.writeText(Cpp + "/p.cpp", std::string(4000, 'x'));
    CHECK(s == Status::Full);
    CHECK_EQ_STR(code::message(s), "SD piena");
    CHECK_EQ_STR(text(full, Cpp + "/p.cpp"), v1);
    // SD tolta: lettura e scrittura danno "Inserire la scheda SD"
    fs.setPresent(false);
    std::string out;
    CHECK(st.readText(f, out) == Status::NoCard);
    CHECK(st.writeText(f, v2) == Status::NoCard);
    fs.setPresent(true);
    CHECK(st.readText(f, out) == Status::Ok && out == v1);
}

// ---------------------------------------------------------------- FM-03..07 RENAME COPY MOVE DEL DEL-ALL
void testFileOperations() {
    RamFileSystem fs;
    Store st(&fs);
    (void)st.ready();
    std::string path;
    CHECK(st.newFile(Lang::Cpp, Cpp, "uno", path) == Status::Ok);
    CHECK(st.newFile(Lang::Cpp, Cpp, "due", path) == Status::Ok);
    CHECK(st.rename(Cpp + "/uno.cpp", "primo.cpp") == Status::Ok);
    CHECK(!fs.exists(Cpp + "/uno.cpp") && fs.exists(Cpp + "/primo.cpp"));
    CHECK(st.rename(Cpp + "/primo.cpp", "due.cpp") == Status::Exists);
    CHECK(st.rename(Cpp + "/primo.cpp", "a/b") == Status::InvalidName);
    CHECK(st.rename(Cpp + "/primo.cpp", "PRIMO.cpp") == Status::Ok);     // solo maiuscole: ammesso
    // COPY nella stessa cartella: NOME_1
    std::string copied;
    CHECK(st.copy(Cpp + "/due.cpp", Cpp, &copied) == Status::Ok);
    CHECK_EQ_STR(copied, Cpp + "/due_1.cpp");
    CHECK_EQ_STR(text(fs, copied), text(fs, Cpp + "/due.cpp"));
    CHECK(st.copy(Cpp + "/manca.cpp", Cpp) == Status::NotFound);
    // COPY e MOVE di una cartella (progetto) con il contenuto
    CHECK(st.newProject(Lang::Cpp, Cpp, "Prog", path) == Status::Ok);
    CHECK(st.makeFolder(Cpp + "/Archivio") == Status::Ok);
    CHECK(st.copy(Cpp + "/Prog", Cpp + "/Archivio") == Status::Ok);
    CHECK(fs.exists(Cpp + "/Archivio/Prog/main.cpp") && fs.exists(Cpp + "/Prog/main.cpp"));
    CHECK(st.copy(Cpp + "/Prog", Cpp + "/Prog") == Status::InvalidName);  // dentro se stessa
    CHECK(st.move(Cpp + "/Prog", Cpp + "/Prog") == Status::InvalidName);
    CHECK(st.move(Cpp + "/due_1.cpp", Cpp + "/Archivio") == Status::Ok);
    CHECK(!fs.exists(Cpp + "/due_1.cpp") && fs.exists(Cpp + "/Archivio/due_1.cpp"));
    CHECK(st.copy(Cpp + "/due.cpp", Cpp + "/Archivio") == Status::Ok);
    CHECK(st.move(Cpp + "/due.cpp", Cpp + "/Archivio") == Status::Exists); // non sovrascrive
    // MOVE tra linguaggi (la cartella di destinazione decide solo la posizione)
    CHECK(st.move(Cpp + "/Archivio/due.cpp", "PICOCALC/CODE/C") == Status::Ok);
    CHECK(fs.exists("PICOCALC/CODE/C/due.cpp"));
    // DEL e DEL-ALL
    CHECK(st.remove(Cpp + "/Archivio/Prog") == Status::Ok);
    CHECK(!fs.exists(Cpp + "/Archivio/Prog") && !fs.exists(Cpp + "/Archivio/Prog/main.cpp"));
    CHECK(st.removeAll(Cpp + "/Archivio") == Status::Ok);
    std::vector<Item> items;
    CHECK(st.list(Cpp + "/Archivio", items) == Status::Ok && items.empty());
    CHECK(fs.exists(Cpp + "/Archivio"));                                   // DEL-ALL svuota, la cartella resta
    // una COPY che non entra nella SD non lascia mezze copie
    std::vector<std::uint8_t> big(60000, 'q');
    CHECK(st.writeText(Cpp + "/grande.cpp", std::string(big.begin(), big.end())) == Status::Ok);
    fs.setCapacity(fs.used() + 1000);
    CHECK(st.copy(Cpp + "/grande.cpp", Cpp + "/Archivio") == Status::Full);
    CHECK(!fs.exists(Cpp + "/Archivio/grande.cpp"));
}

// ---------------------------------------------------------------- FM-08 SEARCH
void testSearch() {
    RamFileSystem fs;
    Store st(&fs);
    (void)st.ready();
    std::string path;
    for (const char* n : {"Pianeti", "piano", "Retta", "PIXEL"}) CHECK(st.newFile(Lang::Cpp, Cpp, n, path) == Status::Ok);
    CHECK(st.newProject(Lang::Cpp, Cpp, "Pista", path) == Status::Ok);
    CHECK(st.writeText(Cpp + "/Pista/pila.cpp", "int x;\n") == Status::Ok);
    const std::vector<std::string> r = st.search(Cpp, "pi");
    CHECK(r.size() == 5);
    CHECK(has(r, Cpp + "/Pianeti.cpp") && has(r, Cpp + "/piano.cpp") && has(r, Cpp + "/PIXEL.cpp") && has(r, Cpp + "/Pista") && has(r, Cpp + "/Pista/pila.cpp"));
    CHECK(!has(r, Cpp + "/Retta.cpp"));
    CHECK(st.search(Cpp, "").empty());
    CHECK(st.search(Cpp, "zz").empty());
    CHECK(st.search(Cpp, "p", 2).size() == 2);
}

// ---------------------------------------------------------------- FM-09 password
void testPasswords() {
    RamFileSystem fs;
    Store st(&fs);
    (void)st.ready();
    std::string path;
    CHECK(st.newFile(Lang::Cpp, Cpp, "segreto", path) == Status::Ok);
    CHECK(!st.locked(path));
    CHECK(st.checkPassword(path, "qualsiasi"));                           // senza password: libero
    CHECK(st.setPassword(path, "123456789") == Status::InvalidName);      // fx-CG50: al piu' 8 caratteri
    CHECK(st.setPassword(path, "abc") == Status::Ok);
    CHECK(st.locked(path));
    CHECK(st.checkPassword(path, "abc"));
    CHECK(!st.checkPassword(path, "abd"));
    CHECK(text(fs, "PICOCALC/CODE/.passwords").find("abc") == std::string::npos);   // solo l'impronta
    std::vector<Item> items;
    (void)st.list(Cpp, items);
    CHECK(items.size() == 1 && items[0].locked);
    // RUN non chiede la password (come la fx-CG50): si esegue
    CHECK_EQ_STR(runProject(st, path), "Ciao\n");
    // la password segue il file: RENAME, MOVE, COPY
    CHECK(st.rename(path, "riservato.cpp") == Status::Ok);
    CHECK(!st.locked(path) && st.locked(Cpp + "/riservato.cpp") && st.checkPassword(Cpp + "/riservato.cpp", "abc"));
    CHECK(st.makeFolder(Cpp + "/Cassaforte") == Status::Ok);
    CHECK(st.move(Cpp + "/riservato.cpp", Cpp + "/Cassaforte") == Status::Ok);
    CHECK(st.locked(Cpp + "/Cassaforte/riservato.cpp") && st.checkPassword(Cpp + "/Cassaforte/riservato.cpp", "abc"));
    CHECK(st.copy(Cpp + "/Cassaforte/riservato.cpp", Cpp) == Status::Ok);
    CHECK(st.locked(Cpp + "/riservato.cpp") && !st.checkPassword(Cpp + "/riservato.cpp", "zzz"));
    // la cartella rinominata porta con se' le password dei file
    CHECK(st.rename(Cpp + "/Cassaforte", "Forziere") == Status::Ok);
    CHECK(st.locked(Cpp + "/Forziere/riservato.cpp") && !st.locked(Cpp + "/Cassaforte/riservato.cpp"));
    // DEL della cartella: le password dei file dentro spariscono (un file nuovo con lo stesso nome e' libero)
    CHECK(st.remove(Cpp + "/Forziere") == Status::Ok);
    CHECK(st.makeFolder(Cpp + "/Forziere") == Status::Ok);
    CHECK(st.writeText(Cpp + "/Forziere/riservato.cpp", "int main() { return 0; }\n") == Status::Ok);
    CHECK(!st.locked(Cpp + "/Forziere/riservato.cpp"));
    // togliere la password
    CHECK(st.setPassword(Cpp + "/riservato.cpp", "") == Status::Ok);
    CHECK(!st.locked(Cpp + "/riservato.cpp"));
    CHECK(!fs.exists("PICOCALC/CODE/.passwords"));                        // nessuna password: niente file
    // file delle password guasto: ignorato senza crash
    CHECK(st.writeText("PICOCALC/CODE/.passwords", "riga senza tab\nx\tnon-esadecimale\n") == Status::Ok);
    CHECK(!st.locked(Cpp + "/riservato.cpp"));
}

// ---------------------------------------------------------------- ED-10 file grandi a blocchi
void testBlocks() {
    RamFileSystem fs;
    Store st(&fs);
    (void)st.ready();
    std::string big;
    for (int i = 0; i < 20000; ++i) big += "// riga " + std::to_string(i) + "\n";     // circa 300 KB
    const std::string f = Cpp + "/lungo.cpp";
    CHECK(st.writeText(f, big) == Status::Ok);
    std::uint64_t size = 0;
    CHECK(st.fileSize(f, size) == Status::Ok && size == big.size());
    std::string out;
    CHECK(st.readText(f, out) == Status::TooLarge);                      // oltre MaxEditBytes: a blocchi
    std::uint64_t total = 0;
    CHECK(st.readBlock(f, 100000, 4096, out, &total) == Status::Ok && total == big.size());
    CHECK(out == big.substr(100000, 4096));
    CHECK(st.readBlock(f, big.size() - 10, 4096, out) == Status::Ok && out == big.substr(big.size() - 10));
    // modifica di un blocco: il resto si copia, la rinomina e' atomica
    const std::string block = "// BLOCCO NUOVO\n";
    CHECK(st.spliceBlock(f, 50000, 4096, block) == Status::Ok);
    const std::string want = big.substr(0, 50000) + block + big.substr(54096);
    CHECK(text(fs, f) == want);
    // interruzione durante il salvataggio del blocco: il file resta quello di prima
    fs.failAfterBytes(120000);
    CHECK(st.spliceBlock(f, 0, 10, "X") != Status::Ok);
    fs.powerOn();
    std::vector<Item> items;
    (void)st.list(Cpp, items);
    CHECK(text(fs, f) == want);
    // blocco in fondo e blocco vuoto
    CHECK(st.spliceBlock(f, want.size(), 0, "// fine\n") == Status::Ok);
    CHECK(text(fs, f) == want + "// fine\n");
}

// ---------------------------------------------------------------- SD-07 migrazione della v1.17.0
void testMigration() {
    RamFileSystem flash;                                                 // flash interna della v1.17.0
    (void)flash.makeDirs("PICOCALC/PROG");
    (void)flash.makeDirs("PICOCALC/PY");
    auto put = [&](RamFileSystem& fs, const std::string& p, const std::string& t) {
        return files::writeAtomic(fs, p, reinterpret_cast<const std::uint8_t*>(t.data()), t.size());
    };
    CHECK(put(flash, "PICOCALC/PROG/HELLO.pcprog", progmode::encodeProgram(prog::Dialect::C, "int main(void) { printf(\"hi\"); return 0; }\n")) == Status::Ok);
    CHECK(put(flash, "PICOCALC/PROG/CONTI.pcprog", progmode::encodeProgram(prog::Dialect::CSharp, "class P { static void Main() { System.Console.Write(2); } }\n")) == Status::Ok);
    CHECK(put(flash, "PICOCALC/PROG/VECCHIO.pcprog", "int main() { return 0; }\n") == Status::Ok);   // senza riga: C++
    CHECK(put(flash, "PICOCALC/PROG/extra.cpp", "int main() { return 3; }\n") == Status::Ok);
    CHECK(put(flash, "PICOCALC/PY/grafico.py", "print(1)\n") == Status::Ok);
    CHECK(put(flash, "PICOCALC/PROG/leggimi.txt", "non e' un programma") == Status::Ok);
    CHECK(Store::hasLegacy(flash));
    RamFileSystem empty;
    CHECK(!Store::hasLegacy(empty));
    RamFileSystem sd;
    Store st(&sd);
    Store::Migration m = st.migrate(flash, false);
    CHECK(m.converted == 5 && m.failed == 0 && m.skipped == 0);
    CHECK_EQ_STR(text(sd, "PICOCALC/CODE/C/HELLO.c"), "int main(void) { printf(\"hi\"); return 0; }\n");   // la riga // pcprog e' tolta
    CHECK(sd.exists("PICOCALC/CODE/CS/CONTI.cs") && sd.exists("PICOCALC/CODE/CPP/VECCHIO.cpp") && sd.exists("PICOCALC/CODE/CPP/extra.cpp"));
    CHECK(sd.exists("PICOCALC/CODE/PY/grafico.py"));
    CHECK(!sd.exists("PICOCALC/CODE/CPP/leggimi.txt"));
    CHECK(flash.exists("PICOCALC/PROG/HELLO.pcprog"));                   // senza removeOld gli originali restano
    CHECK_EQ_STR(runProject(st, "PICOCALC/CODE/C/HELLO.c"), "hi");
    CHECK_EQ_STR(runProject(st, "PICOCALC/CODE/CS/CONTI.cs"), "2");
    // seconda volta: gia' migrati (stesso testo), niente doppioni
    m = st.migrate(flash, false);
    CHECK(m.converted == 0 && m.skipped == 5);
    // un file diverso con lo stesso nome non si sovrascrive: NOME_1
    CHECK(put(flash, "PICOCALC/PROG/extra.cpp", "int main() { return 4; }\n") == Status::Ok);
    m = st.migrate(flash, true);
    CHECK(m.converted == 1 && sd.exists("PICOCALC/CODE/CPP/extra_1.cpp"));
    CHECK(text(sd, "PICOCALC/CODE/CPP/extra.cpp") == "int main() { return 3; }\n");
    CHECK(!flash.exists("PICOCALC/PROG/extra.cpp"));                    // removeOld: via dalla flash
    // senza SD la migrazione non parte e lo dice
    RamFileSystem noSd;
    noSd.setPresent(false);
    Store sn(&noSd);
    m = sn.migrate(flash, false);
    CHECK(m.failed == 1 && m.converted == 0 && !m.notes.empty() && m.notes[0] == "Inserire la scheda SD");
    // SD piena a meta': i programmi gia' copiati restano, gli altri sono segnalati
    RamFileSystem tiny(1);
    Store sm(&tiny);
    tiny.setCapacity(64 * 1024);
    (void)sm.ready();
    tiny.setCapacity(tiny.used() + 60);
    m = sm.migrate(flash, false);
    CHECK(m.failed > 0 && m.converted + m.failed + m.skipped == 4);          // extra.cpp e' gia' stato tolto dalla flash
}

// ---------------------------------------------------------------- RN-03/LB-11 progetti e librerie per il compilatore
void testProjectSources() {
    RamFileSystem fs;
    Store st(&fs);
    (void)st.ready();
    // progetto C++ con un'intestazione e due file
    CHECK(st.makeFolder(Cpp + "/Geo") == Status::Ok);
    CHECK(st.writeText(Cpp + "/Geo/main.cpp", "#include <iostream>\n#include \"area.h\"\nint main() { std::cout << area(3, 4); return 0; }\n") == Status::Ok);
    CHECK(st.writeText(Cpp + "/Geo/area.h", "#pragma once\nint area(int a, int b);\n") == Status::Ok);
    CHECK(st.writeText(Cpp + "/Geo/area.cpp", "#include \"area.h\"\nint area(int a, int b) { return a * b; }\n") == Status::Ok);
    CHECK_EQ_STR(runProject(st, Cpp + "/Geo"), "12");
    // errore in un file del progetto: file e riga giusti
    CHECK(st.writeText(Cpp + "/Geo/area.cpp", "#include \"area.h\"\nint area(int a, int b) {\n  return a * c;\n}\n") == Status::Ok);
    std::string err;
    const std::string r = runProject(st, Cpp + "/Geo", &err);
    CHECK(r.find("ERR[") != std::string::npos && err.find("'c'") != std::string::npos);
    // intestazione mancante: suggerimento di LIB
    CHECK(st.writeText(Cpp + "/usa.cpp", "#include \"matrici.h\"\nint main() { return 0; }\n") == Status::Ok);
    err.clear();
    (void)runProject(st, Cpp + "/usa.cpp", &err);
    CHECK(err.find("matrici.h") != std::string::npos);
    // un file dell'utente con lo stesso nome di una libreria vince (prima il progetto, poi LIB)
    CHECK(st.writeText(Cpp + "/matrici.h", "#pragma once\ninline int traccia() { return 7; }\n") == Status::Ok);
    CHECK(st.writeText(Cpp + "/usa.cpp", "#include <iostream>\n#include \"matrici.h\"\nint main() { std::cout << traccia(); return 0; }\n") == Status::Ok);
    CHECK_EQ_STR(runProject(st, Cpp + "/usa.cpp"), "7");
    // file del programma: solo nella cartella del programma
    CHECK(st.writeText(Cpp + "/scrivi.cpp", "#include <fstream>\nint main() { std::ofstream f(\"out.txt\"); f << 42; return 0; }\n") == Status::Ok);
    CHECK_EQ_STR(runProject(st, Cpp + "/scrivi.cpp"), "");
    CHECK_EQ_STR(text(fs, Cpp + "/out.txt"), "42");
    // intestazioni non si eseguono; Python e Java hanno i loro messaggi
    err.clear();
    CHECK(runProject(st, Cpp + "/Geo/area.h", &err).rfind("LOAD[", 0) == 0);
    CHECK(st.writeText("PICOCALC/CODE/PY/a.py", "print(1)\n") == Status::Ok);
    err.clear();
    (void)runProject(st, "PICOCALC/CODE/PY/a.py", &err);
    CHECK(err.find("Python") != std::string::npos);
    // troppi file in un progetto: messaggio chiaro
    CHECK(st.makeFolder(Cpp + "/Enorme") == Status::Ok);
    for (std::size_t i = 0; i <= code::MaxProjectFiles; ++i) (void)st.writeText(Cpp + "/Enorme/f" + std::to_string(i) + ".cpp", "int f" + std::to_string(i) + "() { return 0; }\n");
    err.clear();
    CHECK(runProject(st, Cpp + "/Enorme", &err).rfind("LOAD[", 0) == 0 && err.find("troppi file") != std::string::npos);
    // SD tolta prima di RUN
    fs.setPresent(false);
    err.clear();
    CHECK(runProject(st, Cpp + "/Geo", &err).rfind("LOAD[", 0) == 0 && err == "Inserire la scheda SD");
}
} // namespace

int main() {
    testFolders();
    testListAndSort();
    testNewAndTemplates();
    testAtomicSave();
    testFileOperations();
    testSearch();
    testPasswords();
    testBlocks();
    testMigration();
    testProjectSources();
    return testutil::finish("code_store_test");
}
