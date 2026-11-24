#pragma once
// Archivio dei programmi sulla microSD (v1.20.0, solo Pimoroni Pico Plus 2 W).
//
// I programmi sono file sorgente standard (nessuna riga aggiunta) in
// PICOCALC/CODE/<LING> (C, CPP, CS, JAVA, PY); un progetto e' una cartella
// (in Java le sottocartelle seguono i package). La flash tiene solo le
// impostazioni, i file recenti e il record del workspace. Ogni scrittura e'
// atomica (file temporaneo verificato, poi rinomina: files::writeAtomic):
// una SD tolta durante il salvataggio lascia il file vecchio intatto. Senza
// SD, SD piena o non scrivibile i messaggi sono chiari ("Inserire la scheda
// SD", "SD piena") e non c'e' mai un crash.
//
// Funzioni del gestore dei file (capitolo 8 della fx-CG50 ampliato): elenco
// con cartelle e ordinamento, NEW file/progetto dai modelli, RENAME, COPY,
// MOVE, DEL, DEL-ALL, SEARCH per nome (inizio del nome come la fx-CG50),
// password facoltativa come sulla fx-CG50 (protegge la modifica, non RUN).
// La migrazione porta i programmi della v1.17.0 (PICOCALC/PROG *.pcprog,
// .c/.cpp/.cs, PICOCALC/PY *.py, dalla flash o dalla SD) in sorgenti standard.
#include "file_store.hpp"
#include "program.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fx::code {

enum class Lang : std::uint8_t { C = 0, Cpp, CSharp, Java, Python, Count };
constexpr std::size_t LangCount = static_cast<std::size_t>(Lang::Count);
constexpr const char* CodeRoot = "PICOCALC/CODE";
constexpr std::size_t MaxEditBytes = 256u * 1024u;       // un file aperto per intero nell'editor
constexpr std::size_t MaxFileBytes = 4u * 1024u * 1024u; // un file aperto a blocchi (limite dichiarato e provato)
constexpr std::size_t MaxProjectFiles = 64;

const char* folderOf(Lang l) noexcept;                   // "C", "CPP", "CS", "JAVA", "PY"
const char* extensionOf(Lang l) noexcept;                // ".c", ".cpp", ".cs", ".java", ".py"
const char* nameOf(Lang l) noexcept;                     // "C", "C++", "C#", "Java", "Python"
const char* libLanguage(Lang l) noexcept;                // "c", "cpp", "cs", "java", "py"
std::string rootOf(Lang l);                              // PICOCALC/CODE/CPP
bool langOfFile(const std::string& fileName, Lang& out) noexcept;   // dall'estensione
bool isSource(const std::string& fileName) noexcept;     // sorgente o intestazione di un linguaggio
prog::Dialect dialectOf(Lang l) noexcept;
// Messaggio per l'utente di uno stato dell'archivio.
std::string message(files::Status st);
// Nome di file o cartella valido sulla SD (1..40 caratteri A-Z a-z 0-9 _ - . , niente '.' iniziale).
bool validItemName(const std::string& name) noexcept;

struct Item {
    std::string name;
    bool dir{false};
    std::uint32_t size{0};
    bool locked{false};                                  // password
};
enum class SortBy : std::uint8_t { Name = 0, Type, Size };

class Store {
public:
    explicit Store(files::FileSystem* fs) noexcept : fs_(fs) {}
    files::FileSystem* fileSystem() const noexcept { return fs_; }
    // SD presente e montata, cartelle CODE create; altrimenti NoCard/IoError...
    files::Status ready();
    files::Status list(const std::string& dir, std::vector<Item>& out, SortBy by = SortBy::Name);
    files::Status readText(const std::string& path, std::string& out, std::size_t maxBytes = MaxEditBytes);
    files::Status readBlock(const std::string& path, std::uint64_t offset, std::size_t length, std::string& out, std::uint64_t* fileSize = nullptr);
    files::Status fileSize(const std::string& path, std::uint64_t& size);
    files::Status writeText(const std::string& path, const std::string& text);
    // Salvataggio di un file grande: il blocco [offset, offset+oldLength) del file
    // sulla SD diventa `block` (il resto si copia a pezzi), poi la rinomina atomica.
    files::Status spliceBlock(const std::string& path, std::uint64_t offset, std::uint64_t oldLength, const std::string& block);
    files::Status makeFolder(const std::string& path);
    files::Status rename(const std::string& path, const std::string& newName);
    files::Status copy(const std::string& from, const std::string& toDir, std::string* newPath = nullptr);
    files::Status move(const std::string& from, const std::string& toDir);
    files::Status remove(const std::string& path);       // file o cartella (con il contenuto)
    files::Status removeAll(const std::string& dir);     // il contenuto della cartella
    bool exists(const std::string& path);
    bool isFolder(const std::string& path);
    // Ricerca come la fx-CG50: i nomi che iniziano con `prefix` (senza maiuscole), nella cartella e sotto.
    std::vector<std::string> search(const std::string& dir, const std::string& prefix, std::size_t maxResults = 64);
    // ---- password (fx-CG50: chiesta per EDIT, non per RUN)
    bool locked(const std::string& path);
    bool checkPassword(const std::string& path, const std::string& password);
    files::Status setPassword(const std::string& path, const std::string& password);   // vuota = toglie
    // ---- nuovi file e progetti dai modelli
    files::Status newFile(Lang l, const std::string& dir, const std::string& name, std::string& path);
    files::Status newProject(Lang l, const std::string& dir, const std::string& name, std::string& path);
    static std::string templateFor(Lang l, const std::string& name, bool project);
    // ---- migrazione della v1.17.0
    struct Migration {
        int converted{0}, skipped{0}, failed{0};
        std::vector<std::string> notes;                  // "HELLO.pcprog -> CODE/CPP/HELLO.cpp"
    };
    // Ci sono programmi della v1.17.0 in `old` (flash interna o SD)?
    static bool hasLegacy(files::FileSystem& old);
    Migration migrate(files::FileSystem& old, bool removeOld);
private:
    files::Status copyTree(const std::string& from, const std::string& to);
    files::Status loadPasswords(std::vector<std::pair<std::string, std::string>>& out);
    files::Status savePasswords(const std::vector<std::pair<std::string, std::string>>& in);
    void movePasswords(const std::string& from, const std::string& to, bool keep);
    files::FileSystem* fs_;
};

// Fonti del compilatore: la cartella del programma (o del progetto) e le
// librerie installate in PICOCALC/LIB/<LING> (prima il progetto, poi LIB).
class ProjectSources final : public prog::SourceProvider {
public:
    ProjectSources(files::FileSystem& fs, std::string projectRoot) : fs_(fs), root_(std::move(projectRoot)) {}
    bool read(const std::string& path, std::string& text, std::string& error) override;
    bool exists(const std::string& path) override;
    bool readLibrary(const prog::LibraryRef& lib, const std::string& path, std::string& text, std::string& error) override;
    std::vector<prog::LibraryRef> libraries(const std::string& language) override;
private:
    files::FileSystem& fs_;
    std::string root_;
};
// Progetto da compilare per un file o una cartella: le unita' di traduzione
// (il file aperto per primo; in una cartella tutti i sorgenti del linguaggio,
// anche nelle sottocartelle), la cartella del programma per i file del
// programma e le fonti. False con il messaggio (SD assente, troppi file...).
// `sources` resta vivo finche' si compila (prog::Project ne tiene il puntatore).
bool loadProject(Store& store, const std::string& path, prog::Project& out, std::unique_ptr<ProjectSources>& sources,
                 std::string& folder, std::string& error, const std::string& editedPath = {}, const std::string* editedText = nullptr);

} // namespace fx::code
