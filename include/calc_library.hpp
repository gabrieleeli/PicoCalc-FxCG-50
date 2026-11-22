#pragma once
#include "board_config.hpp"
// Libreria di funzioni di Run-Matrix (v1.12.0) usata dall'unico valutatore
// delle espressioni (expression.cpp): conversioni metriche (OPTN › CONVERT),
// distribuzioni di probabilita' (OPTN › STAT › DIST, PROB › P( Q( R( ),
// funzioni finanziarie (OPTN › TVM) e nomi mostrati dei comandi.
// Solo calcolo: nessuna interfaccia, nessun accesso al workspace.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::calclib {

// ---------------------------------------------------------------- CONVERT
// Unita' delle conversioni metriche della fx-CG50 (manuale 2-64…): il valore
// in unita' SI e' (v + offset) × factor; le temperature hanno l'offset.
struct UnitInfo {
    const char* name;        // nome scritto tra [ ] ("cm", "inch", "°C", "kgf/cm²")
    std::uint8_t category;   // indice in unitCategories()
    double factor;
    double offset;
};
// Categorie nell'ordine del menu CONVERT (terminato da nullptr).
const char* const* unitCategories() noexcept;
// Unita' di una categoria, nell'ordine del menu.
std::vector<const UnitInfo*> unitsOf(std::size_t category);
// Unita' con il nome esatto, nullptr se sconosciuta.
const UnitInfo* findUnit(const std::string& name) noexcept;
// Converte tra due unita' della stessa categoria; Argument ERROR (MathError)
// per categorie diverse.
double convertUnit(double value, const UnitInfo& from, const UnitInfo& to);

// ---------------------------------------------------------------- distribuzioni
// Funzioni della fx-CG50 (OPTN › STAT › DIST e PROB). Gli argomenti fuori
// dominio lanciano MathError ("Argument ERROR" / "Math ERROR").
double normPD(double x, double sigma, double mu);
double normCD(double lower, double upper, double sigma, double mu);
// tail: -1 sinistra ("L"), +1 destra ("R"); per "C" si usa invNormCentral.
double invNormCD(int tail, double area, double sigma, double mu);
void invNormCentral(double area, double sigma, double mu, double& lower, double& upper);
double tPD(double x, double df);
double tCD(double lower, double upper, double df);
double invTCD(double upperArea, double df);           // P(X >= x) = area
double chiPD(double x, double df);
double chiCD(double lower, double upper, double df);
double invChiCD(double upperArea, double df);
double fPD(double x, double n, double d);
double fCD(double lower, double upper, double n, double d);
double invFCD(double upperArea, double n, double d);
double binomialPD(double x, double n, double p);
double binomialCD(double x, double n, double p);
double invBinomialCD(double prob, double n, double p);
double poissonPD(double x, double lambda);
double poissonCD(double x, double lambda);
double invPoissonCD(double prob, double lambda);
double geoPD(double x, double p);
double geoCD(double x, double p);
double invGeoCD(double prob, double p);
double hypergeoPD(double x, double n, double m, double N);
double hypergeoCD(double x, double n, double m, double N);
double invHypergeoCD(double prob, double n, double m, double N);
// PROB › P( Q( R(: normale standard (P = sinistra, Q = tra 0 e |t|, R = destra).
double probP(double t);
double probQ(double t);
double probR(double t);

// ---------------------------------------------------------------- TVM
// Funzioni finanziarie della fx-CG50 (OPTN › TVM, manuale cap. 7): anno di
// 365 giorni, pagamento a fine periodo (le impostazioni predefinite della
// modalita' Financial). I flussi di cassa sono una lista.
double smplSI(double n, double rate, double pv);
double smplSFV(double n, double rate, double pv);
double cmpdPV(double n, double rate, double pmt, double fv, double py, double cy);
double cmpdFV(double n, double rate, double pv, double pmt, double py, double cy);
double cmpdPMT(double n, double rate, double pv, double fv, double py, double cy);
double cmpdN(double rate, double pv, double pmt, double fv, double py, double cy);
double cmpdI(double n, double pv, double pmt, double fv, double py, double cy);
double cashNPV(double rate, const std::vector<double>& cash);
double cashNFV(double rate, const std::vector<double>& cash);
double cashIRR(const std::vector<double>& cash);
double cashPBP(double rate, const std::vector<double>& cash);
// Ammortamento: kind 0 BAL (dopo PM2), 1 INT (rata PM1), 2 PRN (rata PM1), 3 ΣINT, 4 ΣPRN (PM1..PM2).
double amortization(int kind, double pm1, double pm2, double rate, double pv, double pmt, double py, double cy);
double cnvtEFF(double n, double apr);
double cnvtAPR(double n, double eff);
double cost(double sell, double margin);
double sell(double cost, double margin);
double margin(double cost, double sell);
double daysPrd(double m1, double d1, double y1, double m2, double d2, double y2);
#if FXAI_BOARD_PICO_PLUS_2W
// v1.17.0 (Financial, solo Plus): le stesse equazioni con il pagamento
// all'inizio (BGN) o alla fine (END) del periodo. unknown: 0 n, 1 I%, 2 PV, 3 PMT, 4 FV.
double cmpdSolve(int unknown, double n, double rate, double pv, double pmt, double fv, double py, double cy, bool begin);
double amortizationAt(int kind, double pm1, double pm2, double rate, double pv, double pmt, double py, double cy, bool begin);
#endif

// ---------------------------------------------------------------- nomi
// Nome mostrato di un comando del testo canonico ("norm_pd" -> "NormPD",
// "solve_n" -> "SolveN", "ran_list" -> "RanList#"), nullptr se il nome non
// e' nella tabella. L'editor e il renderer lo usano per disegnare il token.
const char* displayName(const std::string& canonical) noexcept;
// Tutti i comandi del CATALOG (nome mostrato, testo canonico inserito),
// terminato da {nullptr, nullptr}; la finestra CATALOG li ordina per nome.
struct CatalogEntry { const char* shown; const char* insert; };
const CatalogEntry* catalog() noexcept;

} // namespace fx::calclib
