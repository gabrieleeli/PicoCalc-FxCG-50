#pragma once
// Nucleo dell'interprete dei programmi (solo Pimoroni Pico Plus 2 W).
//
// v1.15.0 (F9): sottoinsiemi interpretati di C, C++ e C#, scritti da zero.
// v1.20.0: nucleo nuovo e condiviso (docs/reports/ANALISI_FONTI_v1.20.0.md §5):
// i front-end C/C++ e C# (Java dalla v1.21.0) producono lo stesso albero, la
// stessa analisi assegna classi, campi, vtable e tipi statici e genera le
// istruzioni di una macchina a pila ITERATIVA (le chiamate del programma sono
// cornici in un vettore, nessuna ricorsione nativa). Il programma si analizza
// dal sorgente a ogni RUN: la rappresentazione interna vive solo in RAM.
// Un solo modello a oggetti (array, istanze, stringhe, contenitori, chiusure),
// un heap con budget e GC mark-and-sweep, gli stessi errori con file, riga e
// colonna, la stessa libreria fx per tutti i linguaggi.
//
// Machine e' un step::Task: compilazione gia' fatta, esecuzione a passi; il
// lavoro gira su una pila propria (coroutine, include/coroutine.hpp) cosi' le
// funzioni native possono richiamare il programma e la fetta di tempo puo'
// finire ovunque. AC = cancelled() o interrupt(): "Break". Tutto l'I/O passa
// da Host (testo, tasti, grafici, calcolatrice, file nella cartella permessa).
#include "step_task.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fx {
struct Workspace;
namespace files { class FileSystem; }
}

namespace fx::prog {

enum class Dialect : std::uint8_t { C = 0, Cpp = 1, CSharp = 2, Java = 3 };
constexpr int DialectCount = 4;
// Dall'estensione: .c/.h C, .cpp/.cc/.cxx/.hpp/.hh C++, .cs C#, .java Java (altrimenti C++).
Dialect dialectOf(const std::string& fileName) noexcept;
const char* dialectName(Dialect d) noexcept;            // "C", "C++", "C#", "Java"
// File sorgente compilato (non solo incluso) per il dialetto: .c, .cpp, .cs, .java.
bool isTranslationUnit(const std::string& fileName, Dialect d) noexcept;

// Limiti di un'esecuzione (docs/reports/ANALISI_FONTI_v1.20.0.md §5.7).
struct Limits {
    std::size_t heapBytes{2u * 1024u * 1024u};          // heap degli oggetti del programma
    std::size_t stackValues{65536};                     // pila dei valori (variabili e operandi)
    std::size_t callDepth{2000};                        // chiamate annidate
    std::size_t sourceBytes{256u * 1024u};              // un file
    std::size_t projectBytes{1024u * 1024u};            // progetto con le librerie
    std::uint32_t timeLimitMs{0};                       // 0 = nessun limite (AC sempre attivo)
};
Limits defaultLimits(bool psram) noexcept;
// Compatibilita' con la v1.15.0: limiti con l'heap di `memoryCells` elementi di 4 byte.
Limits limitsForCells(std::size_t memoryCells) noexcept;
constexpr int MaxNesting = 48;                          // annidamento nel sorgente ("Stack ERROR")
constexpr std::size_t MaxCallDepth = 2000;
constexpr std::size_t MaxSourceBytes = 256u * 1024u;
constexpr std::size_t DefaultMemoryCells = 200000;      // v1.15.0: elementi (heap di 800 KB)

// Errore di compilazione o di esecuzione: messaggio col nome dell'errore della
// fx-CG50 o dell'eccezione del linguaggio, file, riga e colonna (1-based; 0 =
// sconosciute) e la pila delle chiamate ("funzione (file:riga)").
struct Error : std::runtime_error {
    Error(const std::string& message, int line, int column, std::string file = {})
        : std::runtime_error(message), line(line), column(column), file(std::move(file)) {}
    int line, column;
    std::string file;
    std::vector<std::string> trace;
};

// Un file del progetto o di una libreria.
struct SourceFile {
    std::string path;                                   // relativo alla radice del progetto ("main.cpp", "lib/util.h")
    std::string text;
    int library{-1};                                    // -1 = programma, altrimenti indice in Project::libraries
};
struct LibraryRef {
    std::string name;                                   // "matrici"
    std::string language;                               // "c", "cpp", "cs", "java", "py"
    std::string namespaceName;                          // C#: nome di using; Java: package; Python: modulo
    std::string root;                                   // cartella sulla SD (PICOCALC/LIB/C/matrici)
    std::string dataFolder;                             // PICOCALC/LIBDATA/C/matrici
    std::vector<std::pair<std::string, std::string>> files;   // percorso relativo -> SHA-256 atteso
};
// Fornisce i file che il progetto include (intestazioni, file di libreria):
// il caricatore (program_project.cpp) chiede i file per nome mentre analizza.
class SourceProvider {
public:
    virtual ~SourceProvider() = default;
    // Testo di `path` (relativo alla radice del progetto); false se manca (error spiega perche').
    virtual bool read(const std::string& path, std::string& text, std::string& error) = 0;
    virtual bool exists(const std::string& path) = 0;
    // File di una libreria installata (percorso relativo alla sua cartella).
    virtual bool readLibrary(const LibraryRef& lib, const std::string& path, std::string& text, std::string& error) {
        (void)lib; (void)path; (void)text; error = "libreria non disponibile"; return false;
    }
    // Librerie installate di un linguaggio ("c", "cpp", "cs", "java").
    virtual std::vector<LibraryRef> libraries(const std::string& language) { (void)language; return {}; }
};
struct Project {
    Dialect dialect{Dialect::Cpp};
    std::vector<SourceFile> files;                      // unita' di traduzione del programma (main incluso)
    std::string entry;                                  // file con main (vuoto: il primo che lo definisce)
    SourceProvider* provider{nullptr};                  // intestazioni e librerie (nullptr: solo `files`)
    std::vector<LibraryRef> libraries;                  // librerie usate (riempito dal caricatore)
    std::size_t projectBytes{1024u * 1024u};
};

class Program;                                          // programma analizzato (opaco)
// Un solo file (v1.15.0): lancia Error.
std::shared_ptr<const Program> compile(const std::string& source, Dialect dialect);
// Progetto con piu' file, intestazioni del progetto e librerie: lancia Error.
std::shared_ptr<const Program> compile(Project& project);
std::size_t codeSize(const Program& program) noexcept;  // istruzioni generate
std::vector<std::string> programFiles(const Program& program);   // file usati (programma e librerie)

// Servizi del programma verso la calcolatrice e la PicoCalc.
class Host {
public:
    virtual ~Host() = default;
    virtual void print(const std::string& text) = 0;                    // testo sul display (con '\n')
    // Una riga digitata: false se non c'e' ancora (la macchina aspetta).
    virtual bool readLine(std::string& out) = 0;
    virtual void clearText() {}
    virtual void locate(int column, int row, const std::string& text) { (void)column; (void)row; print(text); }
    virtual int getKey() { return 0; }                                  // codice del Getkey, 0 = nessun tasto
    // Comando grafico della calcolatrice ("Plot 1,2", "F-Line 0,0,1,1", ...).
    virtual void graphics(const std::string& command) { (void)command; }
    virtual int pixelTest(int row, int column) { (void)row; (void)column; return 0; }
    // Espressione della calcolatrice con le variabili del programma legate.
    virtual double calc(const std::string& expression, const std::vector<std::pair<std::string, double>>& locals) {
        (void)expression; (void)locals; return 0;
    }
    virtual std::string calcExact(const std::string& expression, const std::vector<std::pair<std::string, double>>& locals) {
        (void)expression; (void)locals; return {};
    }
    virtual double getVariable(char letter) { (void)letter; return 0; }   // A-Z, r (='r'), theta (='@'), Ans (='$')
    virtual void setVariable(char letter, double value) { (void)letter; (void)value; }
    // List 1-26 (indice da 1, come List 1[n]); errori come MathError.
    virtual double getListItem(int list, int index) { (void)list; (void)index; return 0; }
    virtual void setListItem(int list, int index, double value) { (void)list; (void)index; (void)value; }
    virtual int listSize(int list) { (void)list; return 0; }
    virtual std::vector<double> getList(int list) { (void)list; return {}; }
    virtual void setList(int list, const std::vector<double>& values) { (void)list; (void)values; }
    // Mat A-Z (lettera), righe per colonne.
    virtual bool getMatrix(char name, std::vector<double>& cells, int& rows, int& cols) { (void)name; (void)cells; rows = cols = 0; return false; }
    virtual void setMatrix(char name, const std::vector<double>& cells, int rows, int cols) { (void)name; (void)cells; (void)rows; (void)cols; }
    // Attesa di EXE (come il comando ◢): false finche' non arriva.
    virtual bool pauseDone() { return true; }
    // File del programma: il file system e la cartella permessa (vuota = nessun file).
    virtual files::FileSystem* files() { return nullptr; }
    virtual std::string programFolder() { return {}; }
    virtual std::uint32_t millis() { return 0; }
};

// Debug (DB-01...DB-07).
enum class StepMode : std::uint8_t { Run, Into, Over, Out };
struct FrameInfo { std::string function, file; int line{0}, column{0}; };
struct VarInfo {
    std::string name, type, value;                      // valore come lo stamperebbe il linguaggio
    bool expandable{false};                             // struct, oggetti, array, contenitori
    std::uint32_t handle{0};                            // per children()
};

class Machine final : public step::Task {
public:
    enum class State : std::uint8_t { Ready, Running, WaitingInput, Paused, Done, Failed };
    Machine(std::shared_ptr<const Program> program, Host& host, Limits limits);
    Machine(std::shared_ptr<const Program> program, Host& host, std::size_t memoryCells = DefaultMemoryCells);
    ~Machine() override;
    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;
    // step::Task: un'unita' = un'istruzione. Gli errori lanciano Error (file,
    // riga, colonna, pila) e lasciano la macchina in Failed.
    bool step(step::Budget& budget) override;
    std::size_t done() const override { return executed_; }
    void cancelled() override;                          // AC: Break, file chiusi, heap liberato
    State state() const noexcept { return state_; }
    int exitCode() const noexcept { return exitCode_; }
    std::size_t executed() const noexcept { return executed_; }
    // Esegue fino alla fine, all'attesa dell'input o alla pausa del debug, o
    // fino a `maxSteps` istruzioni ("Time Out").
    void runToEnd(std::size_t maxSteps = 100000000);
    int currentLine() const noexcept;
    std::string currentFile() const;
    // ---- debug
    void setDebug(bool on);                             // prima del primo step: parte in pausa
    void setBreakpoints(const std::vector<std::pair<std::string, int>>& fileLines);
    void resume(StepMode mode);                         // da Paused
    std::vector<FrameInfo> callStack() const;           // dalla cornice piu' interna
    std::vector<VarInfo> variables(std::size_t frame) const;   // locali della cornice e globali
    std::vector<VarInfo> children(std::uint32_t handle) const;
    // Memoria dell'heap in uso (byte) e cicli del GC (test).
    std::size_t heapUsed() const noexcept;
    std::size_t gcCycles() const noexcept;
    std::size_t openFiles() const noexcept;             // file aperti dal programma
    struct Impl;
private:
    std::unique_ptr<Impl> impl_;
    State state_{State::Ready};
    int exitCode_{0};
    std::size_t executed_{0};
    friend struct Impl;
};

// Host collegato al workspace: i grafici passano da graphcmd::execute, le
// espressioni dal parser unico con le variabili del programma legate da
// bind::BindingResolver, A-Z, liste e matrici dalla memoria. Il testo va in
// `output`, l'input viene da `input` (una riga per elemento).
class WorkspaceHost : public Host {
public:
    explicit WorkspaceHost(Workspace& ws) : ws_(ws) {}
    void print(const std::string& text) override { output += text; }
    bool readLine(std::string& out) override;
    void clearText() override { output.clear(); }
    void graphics(const std::string& command) override;
    double calc(const std::string& expression, const std::vector<std::pair<std::string, double>>& locals) override;
    std::string calcExact(const std::string& expression, const std::vector<std::pair<std::string, double>>& locals) override;
    double getVariable(char letter) override;
    void setVariable(char letter, double value) override;
    double getListItem(int list, int index) override;
    void setListItem(int list, int index, double value) override;
    int listSize(int list) override;
    std::vector<double> getList(int list) override;
    void setList(int list, const std::vector<double>& values) override;
    bool getMatrix(char name, std::vector<double>& cells, int& rows, int& cols) override;
    void setMatrix(char name, const std::vector<double>& cells, int rows, int cols) override;
    files::FileSystem* files() override { return fs; }
    std::string programFolder() override { return folder; }
    std::uint32_t millis() override { return clock; }
    std::string output;
    std::vector<std::string> input;
    std::vector<std::string> commands;                  // comandi grafici eseguiti
    files::FileSystem* fs{nullptr};                     // file del programma (test: RamFileSystem)
    std::string folder;                                 // cartella del programma
    std::uint32_t clock{0};
    Workspace& workspace() noexcept { return ws_; }
private:
    Workspace& ws_;
};

} // namespace fx::prog
