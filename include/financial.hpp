#pragma once
// Nucleo di Financial (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// I calcoli sono quelli di calc_library (le funzioni di OPTN › TVM di
// Run-Matrix): un solo motore finanziario. Qui ci sono lo stato della
// modalita' (impostazioni, interesse semplice, flussi di cassa, ammortamento,
// conversione dei tassi, date) e le schermate risolte come funzioni pure. Le
// variabili TVM n, I%, PV, PMT, FV, P/Y, C/Y sono le variabili di modo del
// workspace (VARS › TVM in Run-Matrix).
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::fin {
enum class TvmVar : std::uint8_t { N = 0, I = 1, PV = 2, PMT = 3, FV = 4, PY = 5, CY = 6 };
const char* tvmName(TvmVar v) noexcept;              // "n", "I%", "PV"...
struct Tvm { double n{0}, i{0}, pv{0}, pmt{0}, fv{0}, py{12}, cy{12}; double& at(TvmVar v); double at(TvmVar v) const; };
// Risolve una delle variabili n, I%, PV, PMT, FV (MathError se non si puo').
double solve(TvmVar unknown, const Tvm& t, bool begin);
struct State {
    bool begin{false};                               // SET UP Payment: BGN / END
    double smplN{120}, smplI{7}, smplPV{-1000};      // interesse semplice (giorni)
    std::vector<double> cash{-1000, 300, 400, 500};  // flussi di cassa Csh0, Csh1...
    double cashI{5};
    double pm1{1}, pm2{12};                          // ammortamento
    double cnvN{4}, cnvI{12};                        // conversione dei tassi (n, I%)
    std::array<double, 6> dates{{1, 1, 2026, 12, 31, 2026}};   // m1 d1 y1 m2 d2 y2
    bool operator==(const State& o) const noexcept;
};
struct SimpleResult { double si, sfv; };
SimpleResult simple(const State& s);
struct CashResult { double npv, irr, pbp, nfv; bool irrOk, pbpOk; };
CashResult cashFlow(const State& s);
struct AmortResult { double bal, interest, principal, sumInterest, sumPrincipal; };
AmortResult amortization(const State& s, const Tvm& t);
double toEff(const State& s);                        // ▶EFF: tasso effettivo da APR
double toApr(const State& s);                        // ▶APR: nominale da effettivo
double days(const State& s);                         // giorni tra le due date (anno di 365 giorni)
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
bool decode(State& s, const std::uint8_t* data, std::size_t size);
// File .pcfin: la sezione e le variabili TVM (n, I%, PV, PMT, FV, P/Y, C/Y di
// VARS › TVM). decodeFile accetta solo numeri finiti.
struct FileData {
    State state;
    Tvm tvm;
    bool operator==(const FileData& o) const noexcept;
};
std::vector<std::uint8_t> encodeFile(const FileData& f);
bool decodeFile(const std::uint8_t* data, std::size_t size, FileData& out);
} // namespace fx::fin
