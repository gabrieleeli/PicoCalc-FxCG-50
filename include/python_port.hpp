#pragma once
// Nucleo della modalita' Python (v1.17.0, Base; solo Pimoroni Pico Plus 2 W).
//
// MicroPython v1.24.1 (porta "embed", licenza MIT, third_party/micropython)
// gira come un LAVORO A PASSI: l'interprete esegue su una propria pila (una
// coroutine: ucontext sul PC, cambio di contesto Cortex-M33 sul dispositivo)
// e l'hook della VM gli fa restituire il controllo al ciclo del firmware ogni
// fetta di tempo, cosi' la tastiera e il display restano vivi. AC chiede un
// KeyboardInterrupt; input() sospende lo script finche' arriva una riga.
// L'heap di MicroPython e' separato da quello del firmware: 512 KB nella PSRAM
// (48 KB in SRAM senza PSRAM) e viene liberato all'uscita dalla modalita'.
#include "step_task.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fx::py {

constexpr std::size_t PsramHeapBytes = 512u * 1024u;
constexpr std::size_t SramHeapBytes = 48u * 1024u;
// Pila della coroutine e margine sotto il limite di MicroPython (RecursionError):
// sul dispositivo il margine copre il lancio dell'eccezione e gli IRQ, che usano
// la pila attiva; sul PC i sanitizer usano molta piu' pila negli intercettori.
#if defined(PICO_ON_DEVICE) && PICO_ON_DEVICE
constexpr std::size_t StackBytes = 24u * 1024u;
constexpr std::size_t StackMarginBytes = 4u * 1024u;
#else
constexpr std::size_t StackBytes = 64u * 1024u;
constexpr std::size_t StackMarginBytes = 16u * 1024u;
#endif
constexpr std::size_t ConsoleLines = 200;

// Disegno di casioplot (coordinate dello schermo dell'area di disegno).
class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void setPixel(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) = 0;
    virtual bool getPixel(int x, int y, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) = 0;
    virtual void drawString(int x, int y, const std::string& s, std::uint8_t r, std::uint8_t g, std::uint8_t b, bool large) = 0;
    virtual void clear() = 0;
    virtual void show() = 0;
};

enum class Outcome : std::uint8_t { Ok, Exception, Interrupted, Exit };

// L'interprete. Uno solo alla volta (MicroPython ha uno stato globale).
class Session final : public step::Task {
public:
    using Clock = std::function<std::uint32_t()>;
    // Prepara heap e pila (std::bad_alloc se la memoria non basta).
    Session(Clock clock, Canvas* canvas);
    ~Session() override;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    static bool active() noexcept;                   // una sessione esiste
    std::size_t heapBytes() const noexcept { return heapBytes_; }
    // Avvia l'esecuzione di uno script (file) o di una riga della SHELL.
    void runScript(const std::string& source);
    void runLine(const std::string& source);
    // Una riga della SHELL e' completa o serve un'altra riga (blocchi con ':').
    bool needsMoreInput(const std::string& text) const;
    bool running() const noexcept { return state_ == State::Running || state_ == State::WaitingInput; }
    bool waitingInput() const noexcept { return state_ == State::WaitingInput; }
    void provideInput(const std::string& line);      // riga per input()
    void interrupt() noexcept;                       // AC: KeyboardInterrupt
    Outcome outcome() const noexcept { return outcome_; }
    // step::Task: esegue fino alla fine della fetta di tempo.
    bool step(step::Budget& budget) override;
    void cancelled() override { interrupt(); }
    // Console: testo scritto da print() e dagli errori.
    const std::vector<std::string>& console() const noexcept { return lines_; }
    void clearConsole() { lines_.assign(1, std::string()); }
    void write(const char* s, std::size_t n);
    // Interne (chiamate dalla colla C).
    struct Impl;
private:
    enum class State : std::uint8_t { Idle, Running, WaitingInput };
    std::unique_ptr<Impl> impl_;
    Clock clock_;
    std::size_t heapBytes_{0};
    State state_{State::Idle};
    Outcome outcome_{Outcome::Ok};
    std::vector<std::string> lines_;
    friend struct Impl;
};

// Stato della modalita' nel workspace (sezione 0x0114).
struct State {
    std::string lastScript;                          // ultimo script aperto
    std::vector<std::string> history;                // ultime righe della SHELL (al piu' 20)
    bool operator==(const State& o) const noexcept { return lastScript == o.lastScript && history == o.history; }
};
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
bool decode(State& s, const std::uint8_t* data, std::size_t size);

} // namespace fx::py
