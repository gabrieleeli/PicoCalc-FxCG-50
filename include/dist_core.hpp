#pragma once
// Nucleo delle distribuzioni di probabilita' condiviso dal DIST di Statistics e
// dalla modalita' Distribution (v1.18.0, solo Pimoroni Pico Plus 2 W). Una
// sola tabella delle 24 funzioni della fx-CG50 (PD, CD e inverse di Normale,
// t, chi^2, F, Binomiale, Poisson, Geometrica, Ipergeometrica) e un solo
// calcolo: le funzioni di calc_library, le stesse di OPTN › STAT › DIST di
// Run-Matrix. L'ingresso List gira a passi (ListTask, interrompibile con AC).
// Il modello del grafico (densita' e aree evidenziate) serve anche al DRAW dei
// test di Statistics.
#include "stat_engine.hpp"
#include "step_task.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace fx::dist {

enum class Family : std::uint8_t { Normal = 0, T, Chi, F, Binomial, Poisson, Geometric, Hypergeometric, Count };
enum class Func : std::uint8_t { PD = 0, CD = 1, Inv = 2 };
constexpr std::size_t FamilyCount = static_cast<std::size_t>(Family::Count);
constexpr std::size_t KindCount = FamilyCount * 3;
struct Kind {
    Family family{Family::Normal};
    Func func{Func::PD};
    bool operator==(const Kind& o) const noexcept { return family == o.family && func == o.func; }
};
std::size_t indexOf(Kind k) noexcept;
Kind kindAt(std::size_t index) noexcept;                // index % KindCount
const char* familyName(Family f) noexcept;              // "Normal", "Student-t", "\xCF\x87\xC2\xB2", "F", ...
const char* familyKey(Family f) noexcept;               // tasti del menu DIST: "NORM", "t", "CHI", "F", "BINOMIAL", ...
const char* kindName(Kind k) noexcept;                  // "Normal P.D", "Normal C.D", "Inverse Normal", ...
const char* kindKey(Kind k) noexcept;                   // "Npd", "Ncd", "InvN", "tpd", ...
bool continuous(Family f) noexcept;
enum class Tail : std::uint8_t { Left = 0, Right = 1, Central = 2 };
const char* tailName(Tail t) noexcept;

// Campi del modulo: prima gli argomenti (x | Lower, Upper | Area), poi i parametri
// della distribuzione, nell'ordine del manuale; valori iniziali.
struct ParamSpec { const char* label; double def; };
std::vector<ParamSpec> paramSpecs(Kind k);
std::size_t argCount(Kind k) noexcept;                  // 1 (x o Area) o 2 (Lower, Upper)
bool hasTail(Kind k) noexcept;                          // Inverse Normal
// Ingresso List: le etichette dei campi lista (al posto degli argomenti).
// Continue CD: Lower List e Upper List; discrete CD: solo Upper List con Lower 0 (fx-CG50).
std::vector<const char*> listLabels(Kind k);

struct Input {
    std::vector<double> values;                         // nell'ordine di paramSpecs
    Tail tail{Tail::Left};
};
struct Result {
    std::vector<std::pair<std::string, double>> rows;   // "p", "z:Low", ... | "xInv" | "x1Inv", "x2Inv"
    std::vector<std::pair<std::uint8_t, double>> vars;  // variabili di VARS › STAT › DIST (stats::ResultVar)
    double main{0};                                     // p, o x per le inverse (x1Inv per Central)
};
// Calcolo di una funzione (MathError con il messaggio della fx-CG50 fuori dominio).
Result compute(Kind k, const Input& in);

// Ingresso List: un elemento per unita' di lavoro. `first` sostituisce
// l'argomento (x, Area, Upper per le CD discrete, Lower per le CD continue),
// `second` l'Upper delle CD continue.
class ListTask final : public step::Task {
public:
    ListTask(Kind k, Input base, std::vector<double> first, std::vector<double> second = {});
    bool step(step::Budget& budget) override;
    std::size_t total() const override { return first_.size(); }
    std::size_t done() const override { return next_; }
    const std::vector<double>& results() const noexcept { return out_; }
private:
    Kind k_;
    Input base_;
    std::vector<double> first_, second_, out_;
    std::size_t next_{0};
};
std::vector<double> computeList(Kind k, const Input& base, const std::vector<double>& first, const std::vector<double>& second = {});

// ---- grafico (continue): densita', finestra, aree evidenziate, punto
struct GraphModel {
    bool available{false};
    Family family{Family::Normal};
    double p1{0}, p2{1};                                // parametri della densita' (Normal: mu, sigma; t/chi^2: df; F: n, d)
    double xmin{-4}, xmax{4}, ymax{0.5};
    std::vector<std::pair<double, double>> areas;       // intervalli evidenziati (anche infiniti)
    bool hasMark{false};
    double markX{0}, markY{0};
};
double density(const GraphModel& g, double x) noexcept;
GraphModel graphModel(Kind k, const Input& in, const Result& r);
// Grafico di un test (DRAW): curva della statistica con l'area del p-value.
GraphModel testGraph(const stats::Outcome& o);
} // namespace fx::dist
