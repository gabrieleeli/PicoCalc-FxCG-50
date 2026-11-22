#pragma once
// Nucleo di Recursion (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Successioni an+1 = f(n, an) e an+2 = f(n, an, an+1) con i termini iniziali
// a0 e a1, tabella per n da Start a End con Σan, punti del grafico e segmenti
// del diagramma a ragnatela (solo an+1). Le formule sono testo lineare con i
// nomi legati del lexer unico (rec_n, rec_an, rec_an1, mostrati n, an, an+1),
// valutate con EvalContext::bind: la memoria A-Z non cambia. La formula an+1 e
// Start/End/a0/a1 sono le variabili di modo del workspace (VARS › RECURSION).
#include "expression.hpp"
#include "step_task.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::rec {
constexpr int MaxN = 999;
enum class Type : std::uint8_t { An1 = 0, An2 = 1 };   // an+1 = f(n, an); an+2 = f(n, an, an+1)
struct State {
    Type type{Type::An1};
    std::string an2;                                   // formula an+2 (testo lineare); an+1 e' in modeVars.recFormula
    bool sigma{true};                                  // colonna Σan nella tabella
    bool operator==(const State& o) const noexcept { return type == o.type && an2 == o.an2 && sigma == o.sigma; }
};
struct Row { int n; double a; double sum; };
// Parametri della generazione (dal workspace).
struct Setup {
    Type type{Type::An1};
    std::string formula;                               // an+1 o an+2
    int start{0}, end{10};
    double a0{0}, a1{1};
};
// Tabella da Start a End (termini calcolati da n = 0). Lavoro a passi: un'unita'
// = un termine. Errori (Math ERROR...) lanciati con il numero del termine.
class Table final : public step::Task {
public:
    Table(Setup setup, EvalContext base);
    bool step(step::Budget& budget) override;
    std::size_t total() const override { return static_cast<std::size_t>(setup_.end) + 1; }
    std::size_t done() const override { return static_cast<std::size_t>(n_); }
    const std::vector<Row>& rows() const noexcept { return rows_; }
    void runAll();
private:
    Setup setup_;
    EvalContext base_;
    ast::NodePtr f_;
    int n_{0};
    double prev_{0}, cur_{0}, sum_{0};
    std::vector<Row> rows_;
};
std::vector<Row> table(const Setup& setup, const EvalContext& base);
// "" se Start/End/a0/a1 sono validi.
std::string validate(const Setup& s);
// Ragnatela: (an, an) -> (an, an+1) -> (an+1, an+1)...: coordinate per Graph2D.
void cobweb(const std::vector<Row>& rows, std::vector<double>& xs, std::vector<double>& ys);
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
bool decode(State& s, const std::uint8_t* data, std::size_t size);
// File .pcrecur: la sezione, la formula an+1 (variabile di modo, testo
// lineare) e Start, End, a0, a1. decodeFile accetta solo dati validi.
struct FileData {
    State state;
    std::string an1;
    double start{0}, end{10}, a0{0}, a1{0};
    bool operator==(const FileData& o) const noexcept {
        return state == o.state && an1 == o.an1 && start == o.start && end == o.end && a0 == o.a0 && a1 == o.a1;
    }
};
std::vector<std::uint8_t> encodeFile(const FileData& f);
bool decodeFile(const std::uint8_t* data, std::size_t size, FileData& out);
} // namespace fx::rec
