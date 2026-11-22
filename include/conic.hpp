#pragma once
// Nucleo di Conic Graphs (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Coniche in forma cartesiana scelte da un elenco di modelli (manuale 5-50),
// coefficienti, curve parametriche per il grafico 2D condiviso e i punti
// notevoli di G-Solve. Nessuna interfaccia e nessun parser qui: i coefficienti
// arrivano gia' valutati dal motore unico.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::conic {
enum class Model : std::uint8_t {
    ParabolaX = 0,      // X = A(Y-K)^2 + H
    ParabolaY = 1,      // Y = A(X-H)^2 + K
    Circle = 2,         // (X-H)^2 + (Y-K)^2 = R^2
    Ellipse = 3,        // (X-H)^2/A^2 + (Y-K)^2/B^2 = 1
    HyperbolaX = 4,     // (X-H)^2/A^2 - (Y-K)^2/B^2 = 1
    HyperbolaY = 5,     // (Y-K)^2/B^2 - (X-H)^2/A^2 = 1
    Count = 6
};
constexpr std::size_t ModelCount = static_cast<std::size_t>(Model::Count);
const char* modelName(Model m) noexcept;           // "Parabola", "Circle"...
const char* modelFormula(Model m) noexcept;        // "Y=A(X-H)^2+K"
// Nomi dei coefficienti del modello, nell'ordine dei valori ("AHK", "HKR", "HKAB").
const char* coefficientNames(Model m) noexcept;
struct State {
    Model model{Model::Circle};
    std::array<double, 5> coef{{1, 1, 0, 0, 1}};   // A, B, H, K, R (per nome, vedi valueOf)
    bool operator==(const State& o) const noexcept { return model == o.model && coef == o.coef; }
};
// Indice in coef del coefficiente con il nome `c` ('A', 'B', 'H', 'K', 'R').
int coefIndex(char c) noexcept;
// "" se i coefficienti sono validi (A != 0, R > 0, A e B > 0 dove servono).
std::string validate(const State& s);
// Rami della curva per la finestra [xmin,xmax] x [ymin,ymax]: ogni ramo e' una
// curva parametrica x(t), y(t) con t in [t0, t1] (le iperboli hanno due rami).
struct Branch { double t0, t1, dt; };
std::vector<Branch> branches(const State& s, double xmin, double xmax, double ymin, double ymax);
bool point(const State& s, std::size_t branch, double t, double& x, double& y) noexcept;
// G-Solve (manuale 5-51).
enum class Solve : std::uint8_t { Vertex, Focus, Center, Radius, Asymptote, Eccentricity };
const char* solveName(Solve s) noexcept;
struct Result {
    std::vector<std::array<double, 2>> points;     // vertici, fuochi, centro
    std::vector<std::array<double, 2>> lines;      // asintoti y = m x + q: (m, q)
    double value{0};                                // raggio, eccentricita'
    bool hasValue{false};
    std::string text;                               // testo mostrato
};
// Lancia MathError("Math ERROR: ...") se la voce non vale per il modello.
Result solve(const State& s, Solve what);
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
bool decode(State& s, const std::uint8_t* data, std::size_t size);
} // namespace fx::conic
