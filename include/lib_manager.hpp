#pragma once
// Gestore delle librerie dei programmi (v1.20.0, solo Pimoroni Pico Plus 2 W).
// Progetto: docs/reports/ANALISI_FONTI_v1.20.0.md §6, uso: docs/LIBRARIES.md.
//
// Una libreria e' una cartella PICOCALC/LIB/<LING>/<nome> con i sorgenti e
// library.json (nome, versione semver, linguaggio, descrizione, autore,
// licenza, namespace, file con SHA-256 e dimensione, dipendenze con gli
// intervalli di versione, firmware minimo). I dati che scrive a tempo di
// esecuzione stanno in PICOCALC/LIBDATA/<LING>/<nome>. L'indice di un
// repository (index.json) elenca le versioni con il percorso, l'impronta del
// manifest e le dipendenze: il piano d'installazione si calcola dall'indice
// (cicli e dipendenze mancanti sono errori chiari), poi ogni file si scarica
// in PICOCALC/LIB/.tmp controllando dimensione e SHA-256 e solo alla fine le
// cartelle si spostano al loro posto con un giornale (.tmp/journal): dopo
// un'interruzione (SD tolta, AC, batteria) all'avvio si completa o si annulla,
// mai una libreria a meta'. L'installazione non esegue codice; percorsi
// assoluti, "..", nomi non validi e file troppo grandi sono rifiutati.
#include "file_store.hpp"
#include "program.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fx::libs {

constexpr const char* LibRoot = "PICOCALC/LIB";
constexpr const char* DataRoot = "PICOCALC/LIBDATA";
constexpr const char* TmpRoot = "PICOCALC/LIB/.tmp";
constexpr const char* CacheRoot = "PICOCALC/LIB/.cache";
constexpr const char* ManifestName = "library.json";
constexpr std::size_t MaxManifestBytes = 16 * 1024;
constexpr std::size_t MaxIndexBytes = 64 * 1024;
constexpr std::size_t MaxFileBytes = 256 * 1024;
constexpr std::size_t MaxLibraryBytes = 1024 * 1024;
constexpr std::size_t MaxFiles = 64;
constexpr std::size_t MaxDependencies = 16;
constexpr std::size_t MaxPlan = 32;

// ---------------------------------------------------------------- versioni
struct Version {
    int major{0}, minor{0}, patch{0};
    static bool parse(std::string_view text, Version& out) noexcept;   // "1.2.3"
    std::string text() const;
    int compare(const Version& o) const noexcept;
    bool operator==(const Version& o) const noexcept { return compare(o) == 0; }
    bool operator<(const Version& o) const noexcept { return compare(o) < 0; }
};
// Intervalli: "1.2.3" (esatta), "^1.2.3", "~1.2.3", ">=1.0.0", ">1.0.0", "<=2.0.0",
// "<2.0.0", "*", e combinazioni separate da spazi (">=1.0.0 <2.0.0").
bool validRange(std::string_view range) noexcept;
bool rangeMatches(std::string_view range, const Version& v) noexcept;

// ---------------------------------------------------------------- manifest e indice
struct LibFile {
    std::string path;                                    // relativo alla cartella della libreria
    std::string sha256;
    std::uint32_t size{0};
};
struct Dependency {
    std::string name;
    std::string range;
};
struct Manifest {
    std::string name;
    Version version;
    std::string language;                                // "c", "cpp", "cs", "java", "py"
    std::string description, author, license;
    std::string ns;                                      // C#: namespace; Java: package; Python: modulo
    std::vector<LibFile> files;
    std::vector<Dependency> dependencies;
    Version minFirmware;
};
bool parseManifest(const std::string& json, Manifest& out, std::string& error);
std::string manifestJson(const Manifest& m);             // testo canonico (tools e test)
struct IndexEntry {
    std::string name, language, description;
    Version version;
    std::string path;                                    // cartella relativa all'indice ("c/matrici/1.0.0")
    std::string manifestSha256;
    std::uint32_t manifestSize{0};
    std::vector<Dependency> dependencies;
    Version minFirmware;                                 // facoltativo: le versioni per firmware piu' nuovi si saltano
};
bool parseIndex(const std::string& json, std::vector<IndexEntry>& out, std::string& error);

bool validLibName(std::string_view name) noexcept;      // [a-z0-9_-], 1..32, inizia con una lettera
bool validLanguage(std::string_view lang) noexcept;
// Percorso relativo sicuro: niente '/' iniziale, "..", ":", "\\", componenti vuoti o nomi strani.
bool safeRelativePath(std::string_view path) noexcept;
std::string langFolder(std::string_view language);      // "c" -> "C", "cpp" -> "CPP", "cs" -> "CS"...
std::string libraryRoot(std::string_view language, std::string_view name);
std::string dataRoot(std::string_view language, std::string_view name);

// ---------------------------------------------------------------- librerie installate
struct Installed {
    Manifest manifest;
    std::string root;
    bool manifestOk{false};
    std::string error;                                   // manifest assente o non valido
};
// Librerie di un linguaggio ("" = tutte), in ordine di nome.
std::vector<Installed> listInstalled(files::FileSystem& fs, std::string_view language);
prog::LibraryRef toRef(const Installed& lib);
struct VerifyReport {
    bool ok{true};
    std::vector<std::string> problems;                  // "file mancante: x.h", "file modificato: y.c"...
};
VerifyReport verify(files::FileSystem& fs, const Installed& lib, const std::function<void()>& poll = nullptr);
// Librerie installate che dipendono da `name`.
std::vector<std::string> dependents(files::FileSystem& fs, std::string_view language, std::string_view name);
files::Status removeLibrary(files::FileSystem& fs, std::string_view language, std::string_view name, std::string& error);
// Copia manuale (PC -> SD): una cartella con library.json messa in LIB/<LING>/:
// la registra controllando nome, file e impronte (VERIFY).
// Cancellazione ricorsiva di una cartella (anche per il gestore dei file).
files::Status removeTree(files::FileSystem& fs, const std::string& path);
// Giornale dell'installazione: completa o annulla un'installazione interrotta.
bool recoverInstall(files::FileSystem& fs, std::string& note);

// ---------------------------------------------------------------- piano
struct PlanStep {
    IndexEntry entry;
    std::string fromVersion;                            // vuota: installazione nuova
    bool downgrade{false};
    bool upToDate{false};                               // gia' installata (niente da fare)
};
// Piano per installare `name` (con le dipendenze) dall'indice: in ordine di
// dipendenza (prima le dipendenze). Errori: libreria sconosciuta, nessuna
// versione compatibile, dipendenza mancante, ciclo (con il percorso), troppe
// librerie, firmware troppo vecchio.
bool makePlan(const std::vector<IndexEntry>& index, const std::vector<Installed>& installed, std::string_view name,
              std::string_view language, const Version& firmware, std::vector<PlanStep>& out, std::string& error,
              std::string_view wantVersion = {});

// ---------------------------------------------------------------- sorgenti dei file
// Una sorgente da' i file di un repository per percorso relativo, a pezzi e
// senza bloccare (HTTPS dal Wi-Fi, una cartella della SD, il collegamento al PC).
class Source {
public:
    virtual ~Source() = default;
    enum class Poll : std::uint8_t { Busy, Data, Done, Failed };
    virtual bool begin(const std::string& relPath, std::string& error) = 0;
    // Nuovi byte in `chunk` (che il chiamante svuota). Done = file completo.
    virtual Poll poll(std::string& chunk, std::string& error) = 0;
    virtual void cancel() = 0;
};
// File da una cartella (copia manuale sulla SD, test).
class FolderSource final : public Source {
public:
    FolderSource(files::FileSystem& fs, std::string base) : fs_(fs), base_(std::move(base)) {}
    bool begin(const std::string& relPath, std::string& error) override;
    Poll poll(std::string& chunk, std::string& error) override;
    void cancel() override { reader_.reset(); }
private:
    files::FileSystem& fs_;
    std::string base_;
    std::unique_ptr<files::Reader> reader_;
};

// ---------------------------------------------------------------- installazione a passi
class Installer {
public:
    enum class State : std::uint8_t { Idle, Fetching, Committing, Done, Failed, Cancelled };
    Installer(files::FileSystem& fs, Source& src);
    ~Installer();
    // `firmware`: versione di questo firmware (minFirmware dei manifest).
    void start(const std::vector<PlanStep>& plan, const Version& firmware);
    // Un passo (fino a ~budget byte scaricati): false quando ha finito.
    bool tick(std::size_t budgetBytes = 8192);
    void cancel();                                      // AC: niente resta in LIB, .tmp ripulita
    State state() const noexcept { return state_; }
    const std::string& error() const noexcept { return error_; }
    std::string progress() const;                       // "matrici 1.2.0: matrici.c 3/5"
    std::size_t bytesDone() const noexcept { return bytes_; }
private:
    void fail(const std::string& why);
    bool openNext();
    bool commit();
    files::FileSystem& fs_;
    Source& src_;
    Version firmware_;
    std::vector<PlanStep> plan_;
    std::vector<Manifest> manifests_;
    State state_{State::Idle};
    std::string error_;
    std::size_t step_{0}, file_{0};
    bool inManifest_{true}, open_{false};
    std::unique_ptr<files::Writer> writer_;
    std::string chunk_;
    std::string manifestText_;
    std::uint64_t got_{0};
    std::size_t bytes_{0};
    struct Hash;
    std::unique_ptr<Hash> hash_;
};

} // namespace fx::libs
