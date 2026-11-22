#pragma once
// Nucleo dell'interprete dei programmi (v1.15.0, F9; solo Pimoroni Pico Plus 2 W).
//
// Decisione della v1.15.0 (docs/reports/ANALISI_FONTI_v1.15.0.md §6): Program
// usa sottoinsiemi interpretati di C, C++ e C#, scritti da zero. Il sorgente
// e' compilato (compile()) in un bytecode per una macchina a pila ITERATIVA:
// nessuna ricorsione nativa durante l'esecuzione (le chiamate del programma
// sono frame in un vettore, al piu' MaxCallDepth), l'analisi e' a discesa
// ricorsiva con profondita' limitata (MaxNesting). Machine e' un step::Task:
// la modalita' la esegue a passi con step::Runner e AC la interrompe
// ("Break"). Tutto l'I/O passa da Host (testo, tasti, comandi grafici della
// calcolatrice, espressioni della calcolatrice): WorkspaceHost lo collega al
// workspace (graph_commands per i grafici, il parser unico per fx::calc).
// Gli errori hanno il nome della fx-CG50 e la riga e colonna del sorgente.
#include "step_task.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fx {
struct Workspace;
}

namespace fx::prog {

enum class Dialect : std::uint8_t { C = 0, Cpp = 1, CSharp = 2 };
// Dall'estensione del file: .c, .cpp, .cs (altrimenti C++).
Dialect dialectOf(const std::string& fileName) noexcept;
const char* dialectName(Dialect d) noexcept;           // "C", "C++", "C#"

// Limiti (stack di 32 KB, heap della scheda).
constexpr int MaxNesting = 48;                          // annidamento di blocchi ed espressioni nell'analisi
constexpr std::size_t MaxCallDepth = 64;                // chiamate annidate (oltre: "Nesting ERROR")
constexpr std::size_t MaxSourceBytes = 256u * 1024u;
constexpr std::size_t MaxCode = 200000;                 // istruzioni del bytecode
constexpr std::size_t MaxStack = 4096;                  // valori sulla pila della macchina
constexpr std::size_t DefaultMemoryCells = 200000;      // elementi di array + caratteri di stringhe

struct Error : std::runtime_error {
    Error(const std::string& message, int line, int column) : std::runtime_error(message), line(line), column(column) {}
    int line, column;                                   // 1-based; 0 = sconosciute
};

// Servizi del programma verso la calcolatrice.
class Host {
public:
    virtual ~Host() = default;
    virtual void print(const std::string& text) = 0;                    // testo sul display (con '\n')
    // Una riga digitata: false se non c'e' ancora (la macchina aspetta l'input).
    virtual bool readLine(std::string& out) = 0;
    virtual void clearText() {}
    virtual void locate(int column, int row, const std::string& text) { (void)column; (void)row; print(text); }
    virtual int getKey() { return 0; }                                  // 0 = nessun tasto
    // Comando grafico della calcolatrice ("Plot 1,2", "F-Line 0,0,1,1"...).
    virtual void graphics(const std::string& command) { (void)command; }
    // Espressione della calcolatrice con le variabili del programma legate.
    virtual double calc(const std::string& expression, const std::vector<std::pair<std::string, double>>& locals) {
        (void)expression; (void)locals; return 0;
    }
    virtual double getVariable(char letter) { (void)letter; return 0; }   // A-Z
    virtual void setVariable(char letter, double value) { (void)letter; (void)value; }
};

class Program;                                          // bytecode compilato (opaco)
std::shared_ptr<const Program> compile(const std::string& source, Dialect dialect);   // lancia Error
std::size_t codeSize(const Program& program) noexcept;

class Machine final : public step::Task {
public:
    enum class State : std::uint8_t { Ready, Running, WaitingInput, Done, Failed };
    Machine(std::shared_ptr<const Program> program, Host& host, std::size_t memoryCells = DefaultMemoryCells);
    ~Machine() override;
    // step::Task: ogni unita' e' un'istruzione. Gli errori di esecuzione
    // lanciano Error (con riga e colonna) e lasciano la macchina in Failed.
    bool step(step::Budget& budget) override;
    std::size_t done() const override { return executed_; }
    void cancelled() override;
    State state() const noexcept { return state_; }
    int exitCode() const noexcept { return exitCode_; }
    std::size_t executed() const noexcept { return executed_; }
    // Esegue fino alla fine o fino a `maxSteps` istruzioni (test).
    void runToEnd(std::size_t maxSteps = 100000000);
    int currentLine() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    State state_{State::Ready};
    int exitCode_{0};
    std::size_t executed_{0};
};

// Host collegato al workspace: i grafici passano da graphcmd::execute, le
// espressioni dal parser unico con le variabili del programma legate da
// bind::BindingResolver, A-Z dalla memoria. Il testo va in `output`, l'input
// viene da `input` (una riga per elemento).
class WorkspaceHost : public Host {
public:
    explicit WorkspaceHost(Workspace& ws) : ws_(ws) {}
    void print(const std::string& text) override { output += text; }
    bool readLine(std::string& out) override;
    void clearText() override { output.clear(); }
    void graphics(const std::string& command) override;
    double calc(const std::string& expression, const std::vector<std::pair<std::string, double>>& locals) override;
    double getVariable(char letter) override;
    void setVariable(char letter, double value) override;
    std::string output;
    std::vector<std::string> input;
    std::vector<std::string> commands;                  // comandi grafici eseguiti
private:
    Workspace& ws_;
};

} // namespace fx::prog
