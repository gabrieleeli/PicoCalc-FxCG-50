#pragma once
// Lavoro a passi interrompibile con AC (v1.15.0, F6).
//
// Generalizza il calcolo progressivo del 3D Graph (v1.14.0): un lavoro lungo
// (ricalcolo di un foglio o di un'eActivity, esecuzione di un programma,
// animazione, test statistici) e' diviso in passi; ogni tick() della modalita'
// ne esegue uno entro un budget di unita' di lavoro E di tempo, cosi' la
// tastiera resta pronta e AC lo interrompe con "Break" come sulla fx-CG50.
// Nessuna interfaccia qui: la barra di avanzamento e' in mode_shell.hpp.
// Il tempo viene da un orologio passato dal chiamante (Platform::millis sul
// dispositivo, un orologio simulato nei test): il comportamento e' riproducibile.
#include <cstddef>
#include <cstdint>
#include <functional>

namespace fx::step {

using Clock = std::function<std::uint32_t()>;

// Budget di un passo: unita' di lavoro (0 = nessun limite) e scadenza in ms
// sull'orologio (0 = nessun limite di tempo). Il lavoro chiama consume() per
// ogni unita' e si ferma appena restituisce false.
class Budget {
public:
    Budget() = default;
    Budget(std::size_t units, std::uint32_t sliceMs, const Clock* clock) noexcept;
    // Una unita' di lavoro: false quando il passo deve finire (unita' esaurite o tempo scaduto).
    bool consume(std::size_t units = 1) noexcept;
    bool exhausted() const noexcept;
    std::size_t used() const noexcept { return used_; }
    std::size_t remaining() const noexcept;          // unita' ancora disponibili (SIZE_MAX senza limite)
private:
    std::size_t limit_{0}, used_{0};
    std::uint32_t start_{0}, slice_{0};
    const Clock* clock_{nullptr};
    mutable bool timeUp_{false};
};

// Un lavoro lungo: step() esegue al piu' il budget e dice se e' finito.
class Task {
public:
    virtual ~Task() = default;
    virtual bool step(Budget& budget) = 0;           // true: lavoro completato
    // Avanzamento per la barra (done <= total; total 0 = sconosciuto).
    virtual std::size_t total() const { return 0; }
    virtual std::size_t done() const { return 0; }
    // Chiamato quando AC interrompe il lavoro (per liberare la memoria, lasciare i dati parziali...).
    virtual void cancelled() {}
};

enum class State : std::uint8_t { Idle, Running, Done, Break, Failed };

// Esegue un Task un passo per tick. Le eccezioni del lavoro (Math ERROR,
// Memory ERROR) passano al chiamante dopo aver messo lo stato a Failed, cosi'
// AppManager mostra la finestra d'errore e il runner non resta "in corso".
class Runner {
public:
    struct Config {
        std::size_t unitsPerStep{0};                 // 0 = nessun limite di unita'
        std::uint32_t sliceMs{20};                   // 0 = nessun limite di tempo
    };
    static constexpr const char* BreakMessage = "Break";
    void start(Task& task, Config config);
    void start(Task& task) { start(task, Config{}); }
    // Un passo: true se e' stato eseguito (va ridisegnato).
    bool tick(const Clock& clock);
    // AC durante il lavoro: lo interrompe (stato Break). Vero se il tasto e' stato usato.
    bool interrupt();
    void reset() noexcept { task_ = nullptr; state_ = State::Idle; steps_ = 0; }
    State state() const noexcept { return state_; }
    bool running() const noexcept { return state_ == State::Running; }
    std::size_t steps() const noexcept { return steps_; }
    // 0..100 dall'avanzamento del lavoro; -1 se il totale non e' noto.
    int percent() const;
private:
    Task* task_{nullptr};
    Config config_{};
    State state_{State::Idle};
    std::size_t steps_{0};
};

} // namespace fx::step
