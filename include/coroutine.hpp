#pragma once
// Coroutine con una pila propria (v1.20.0, solo Pimoroni Pico Plus 2 W).
//
// Estratta dalla porta di MicroPython della v1.17.0 (python_port.cpp) per
// servire anche l'interprete dei programmi: un lavoro lungo gira su una pila
// sua e restituisce il controllo al ciclo del firmware con yield() da
// qualsiasi profondita' (anche dentro una funzione nativa che richiama il
// programma), cosi' tastiera e display restano vivi.
//  * PC: ucontext (makecontext/swapcontext), annotato per AddressSanitizer;
//  * dispositivo (Cortex-M33): fx_coro_switch salva r4-r11, lr, s16-s31 e
//    MSPLIM; il limite della pila segue la pila attiva.
// Una coroutine alla volta per core (quella attiva e' globale). Le eccezioni
// C++ non attraversano il cambio di contesto: il corpo le cattura tutte.
#include <cstddef>
#include <cstdint>

namespace fx::coro {

class Coroutine {
public:
    using Body = void (*)(void* arg);
    // Pila di `stackBytes` (SRAM sul dispositivo); std::bad_alloc se manca.
    Coroutine(std::size_t stackBytes, Body body, void* arg);
    ~Coroutine();                                // il corpo deve essere finito (o mai partito)
    Coroutine(const Coroutine&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;
    // Dal ciclo principale: entra (o rientra) nella coroutine; torna quando
    // il corpo chiama yield() o finisce.
    void resume();
    // Dal corpo: torna al ciclo principale.
    void yield();
    bool started() const noexcept { return started_; }
    bool finished() const noexcept { return finished_; }
    bool inside() const noexcept { return inside_; }
    std::size_t stackBytes() const noexcept { return size_; }
    // Byte della pila mai toccati (la pila e' riempita con un motivo all'inizio).
    std::size_t stackUntouched() const noexcept;
    // Indirizzi della pila (limite per il controllo della profondita').
    const std::uint8_t* stackBottom() const noexcept { return stack_; }
    struct Impl;
    static void entry();                         // interno: primo ingresso nella coroutine attiva
private:
    std::uint8_t* stack_{nullptr};
    std::size_t size_{0};
    Body body_;
    void* arg_;
    Impl* impl_{nullptr};
    bool started_{false}, finished_{false}, inside_{false};
};

// La coroutine in esecuzione sul core (nullptr fuori da ogni coroutine).
Coroutine* current() noexcept;

} // namespace fx::coro
