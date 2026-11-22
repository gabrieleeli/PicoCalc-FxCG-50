#pragma once
// Nucleo di Dyna Graph (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Una funzione Y= con un coefficiente variabile (una lettera A-Z): per ogni
// valore da Start a End con passo Pitch (VARS › DYNA, nel workspace) si disegna
// un fotogramma sulla V-Window di Graph. Il coefficiente e' legato
// nell'EvalContext (EvalContext::bind): la variabile A-Z della memoria non cambia.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::dyna {
constexpr std::size_t MaxFrames = 100;                 // fx-CG50: al piu' 100 grafici per animazione
enum class Speed : std::uint8_t { Slow = 0, Normal = 1, Fast = 2 };
// Millisecondi tra due fotogrammi.
std::uint32_t frameDelayMs(Speed s) noexcept;
const char* speedName(Speed s) noexcept;
struct State {
    std::string function{"A(X-1)^2-2"};               // testo lineare canonico, variabile X
    char variable{'A'};                              // coefficiente che cambia
    bool loop{true};                                 // true: ciclo continuo; false: passata singola
    Speed speed{Speed::Normal};
    bool operator==(const State& o) const noexcept { return function == o.function && variable == o.variable && loop == o.loop && speed == o.speed; }
};
// Valori del coefficiente da start a end con passo pitch (anche decrescenti con
// pitch negativo). Lancia MathError: "Range ERROR" oltre MaxFrames, "Argument
// ERROR" con pitch nullo o di segno sbagliato.
std::vector<double> frameValues(double start, double end, double pitch);
// "" se lo stato e' valido, altrimenti il messaggio.
std::string validate(const State& s);
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
bool decode(State& s, const std::uint8_t* data, std::size_t size);
// File .pcdyna: la sezione e Start, End, Pitch (le variabili di VARS › DYNA).
// decodeFile accetta solo dati validi (validate e frameValues senza errori).
struct FileData {
    State state;
    double start{1}, end{5}, pitch{1};
    bool operator==(const FileData& o) const noexcept { return state == o.state && start == o.start && end == o.end && pitch == o.pitch; }
};
std::vector<std::uint8_t> encodeFile(const FileData& f);
bool decodeFile(const std::uint8_t* data, std::size_t size, FileData& out);
} // namespace fx::dyna
