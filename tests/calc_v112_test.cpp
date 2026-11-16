// v1.12.0 — calcolo di Run-Matrix: CALC (Solve con limiti, SolveN, FMin, FMax,
// Σ con variabile e passo, ∫ con tolleranza, d²/dx², funzioni vietate), PROB
// (Ran# n, RanInt#/RanNorm#/RanBin# con n, RanList#, RanSamp#, P( Q( R( t( ),
// NUMERIC (Rnd), ANGLE (ʳ ᵍ ° ′ ″, Pol/Rec in List Ans), complessi (∠, ►r∠θ),
// STAT (distribuzioni, S-Dev, Var, Mean/Median con frequenze, x̂ ŷ), TVM,
// CONVERT, ESYM P/E ed ENG/ENG←, overflow, Simplify Manual (m.c.m.), Base-N
// (Bin a 16 bit, variabili A-F in Hex, liste nella base) e messaggi d'errore.
// I valori attesi sono gli esempi del manuale fx-CG50 dove il manuale ne da'.
#include "calc_library.hpp"
#include "run_matrix.hpp"
#include "statistics.hpp"
#include "test_util.hpp"
#include <cmath>
#include <string>

using namespace fx;

namespace {
const std::string kArrow = "\xE2\x86\x92";
const std::string kSimp = "\xE2\x96\xB6Simp";
std::string calc(Workspace& w, const std::string& t) { runCalculation(w, t); return resultText(w.history.back(), w.settings); }
double num(Workspace& w, const std::string& t) { runCalculation(w, t); return w.history.back().value.real(); }
template <typename E> bool fails(Workspace& w, const std::string& t) {
    try { runCalculation(w, t); } catch (const E&) { return true; } catch (...) { return false; }
    return false;
}
std::string failText(Workspace& w, const std::string& t) {
    try { runCalculation(w, t); } catch (const std::exception& e) { return e.what(); }
    return "(nessun errore)";
}
Workspace deg() { Workspace w; w.settings.angle = AngleMode::Degrees; return w; }
} // namespace

int main() {
    // ================================================================ CALC (manuale 2-25…2-35)
    {
        Workspace w = deg();
        CHECK_EQ_STR(calc(w, "derivative(x^3+4x^2+x-6,3)"), "52");                 // 2-29
        CHECK_EQ_STR(calc(w, "derivative2(x^3+4x^2+x-6,3)"), "26");                // 2-30
        CHECK_NEAR(num(w, "integral(2x^2+3x+4,1,5)"), 404.0 / 3.0, 1e-9);          // 2-31
        CHECK_NEAR(num(w, "integral(2x^2+3x+4,1,5,1e-5)"), 404.0 / 3.0, 1e-6);     // tolleranza
        CHECK(fails<MathError>(w, "integral(x,0,1,2)"));
        CHECK_EQ_STR(calc(w, "sum(k^2-3k+5,k,3,6)"), "52");                        // 2-34, variabile k
        CHECK_EQ_STR(calc(w, "sum(k,k,1,10,2)"), "25");                            // passo 2: 1+3+5+7+9
        CHECK_EQ_STR(calc(w, "sum(x^2,1,10)"), "385");                             // forma storica (x)
        CHECK_EQ_STR(calc(w, "prod(n,n,1,5)"), "120");
        CHECK_EQ_STR(calc(w, "sum(1/k,k,1,3)"), "11/6");                           // resta esatto
        CHECK(fails<SyntaxError>(w, "sum(k,2,1,5)"));                              // la variabile e' una lettera
        // Solve(f, stima[, inferiore, superiore]) e la forma equazione a=b
        CHECK_EQ_STR(calc(w, "solve(x^2-4,1,0,3)"), "2");
        CHECK_EQ_STR(calc(w, "solve(x^2=9,1)"), "3");
        CHECK_NEAR(num(w, "solve(x^2-4,-5,-3,-1)"), -2, 1e-10);
        CHECK(fails<MathError>(w, "solve(x^2+1,1,0,3)"));
        CHECK(failText(w, "solve(x^2+1,1,0,3)").rfind("Iteration ERROR", 0) == 0);
        // SolveN: fino a 10 soluzioni in List Ans (manuale 2-33)
        CHECK_EQ_STR(calc(w, "solve_n(x^2-x-6)"), "{-2,3}");
        CHECK_EQ_STR(calc(w, "solve_n(x^2=4,x,0,5)"), "{2}");
        CHECK_EQ_STR(calc(w, "solve_n((x-1)^2)"), "{1}");                          // radice doppia
        CHECK(failText(w, "solve_n(1/x)").rfind("Can't Solve!", 0) == 0);              // un polo non e' una soluzione
        CHECK(failText(w, "solve_n(x^2+1)").rfind("Can't Solve!", 0) == 0);
        CHECK_EQ_STR(calc(w, "Dim(solve_n(x^3-6x^2+11x-6))"), "3");
        CHECK(w.history.back().error.empty());
        // FMin / FMax: {x, f(x)} (manuale 2-35)
        CHECK_EQ_STR(calc(w, "f_min(x^2-4x+9,1,4)"), "{2,5}");
        CHECK_EQ_STR(calc(w, "f_max(-x^2+2x+2,0,3)"), "{1,3}");
        CHECK_EQ_STR(calc(w, "f_max(-x^2+2x+2,0,3,6)"), "{1,3}");                  // precisione n
        CHECK(fails<MathError>(w, "f_min(x,1,4,12)"));
        CHECK_EQ_STR(calc(w, "List Ans[1]"), "1");
        // Funzioni vietate negli argomenti di Solve/SolveN/FMin/FMax (manuale 2-28)
        CHECK(fails<SyntaxError>(w, "solve(sum(k,k,1,x)-6,1)"));
        CHECK(fails<SyntaxError>(w, "solve_n(f_min(x,0,1)[1]+x)"));
        CHECK(fails<SyntaxError>(w, "f_min(solve(x-1,0)+x,0,1)"));
    }
    // ================================================================ PROB (manuale 2-18…2-21)
    {
        Workspace w = deg();
        seedRandom(12345);
        const double a = num(w, "ran 1"), b = num(w, "ran 1");
        CHECK(a > 0 && a < 1 && b > 0 && b < 1 && a != b);
        CHECK_EQ_STR(calc(w, "ran 0"), "0");                                       // reinizializza
        CHECK(num(w, "ran 1") == a && num(w, "ran 1") == b);                       // stessa sequenza
        CHECK(fails<MathError>(w, "ran 11"));
        runCalculation(w, "randint(1,6,50)");
        const auto& l = w.matrices.listAns;
        CHECK(l.size() == 50);
        bool inRange = true; for (double d : l.re) inRange = inRange && d >= 1 && d <= 6 && d == std::floor(d);
        CHECK(inRange);
        runCalculation(w, "ran_list(4)");
        CHECK(w.matrices.listAns.size() == 4);
        runCalculation(w, "ran_samp({1,2,3,4,5},5)");                              // senza ripetizione
        double sum = 0; for (double d : w.matrices.listAns.re) sum += d;
        CHECK(w.matrices.listAns.size() == 5 && sum == 15);
        runCalculation(w, "ran_samp({7},3,1)");                                    // con ripetizione
        CHECK(w.matrices.listAns.size() == 3 && w.matrices.listAns.re[2] == 7);
        CHECK(fails<MathError>(w, "ran_samp({1,2},3)"));
        runCalculation(w, "rannorm(1,10,5)");
        CHECK(w.matrices.listAns.size() == 5);
        runCalculation(w, "ranbin(10,0.5,3)");
        CHECK(w.matrices.listAns.size() == 3);
        // P( Q( R( della normale standard
        CHECK_NEAR(num(w, "prob_p(0.5)"), 0.6914624613, 1e-10);
        CHECK_NEAR(num(w, "prob_q(-1)"), 0.3413447461, 1e-10);
        CHECK_NEAR(num(w, "prob_r(1)"), 0.1586552539, 1e-10);
        // t(x) = (x - x̄)/σx dei dati di Statistics
        w.setList(0, {1, 2, 3, 4, 5});
        CHECK_NEAR(num(w, "prob_t(3)"), 0, 1e-12);
        CHECK_NEAR(num(w, "prob_t(5)"), 2 / std::sqrt(2.0), 1e-12);
        CHECK_EQ_STR(calc(w, "ncr(5,2)"), "10");
    }
    // ================================================================ NUMERIC, ANGLE, coordinate (2-15, 2-17, 2-20)
    {
        Workspace w = deg();
        w.settings.display = {DisplayMode::Fix, 3};
        CHECK_EQ_STR(calc(w, "num_rnd(10/3)\xC3\x97" "3"), "9.999");               // Rnd arrotonda alle cifre di Fix
        w.settings.display = {DisplayMode::Norm1, 0};
        CHECK_NEAR(num(w, "num_rnd(1/3)"), 0.3333333333, 1e-15);
        CHECK_EQ_STR(calc(w, "pi\xCA\xB3"), "180");                                // π radianti in Deg
        CHECK_EQ_STR(calc(w, "100\xE1\xB5\x8D"), "90");                            // 100 gradi centesimali
        CHECK_NEAR(num(w, "2\xC2\xB0" "20\xE2\x80\xB2" "30\xE2\x80\xB3"), 2 + 20 / 60.0 + 30 / 3600.0, 1e-12);   // 2°20′30″
        CHECK_EQ_STR(calc(w, "2\xC2\xB0" "30\xE2\x80\xB2"), "2.5");                // sessagesimale: decimale
        CHECK_EQ_STR(calc(w, "pol(1,sqrt(3))"), "{2,60}");                         // List Ans (2-20)
        CHECK_EQ_STR(calc(w, "rec(2,30)"), "{1.732050808,1}");
        CHECK_EQ_STR(calc(w, "List Ans[2]"), "1");
        w.settings.angle = AngleMode::Radians;
        CHECK_NEAR(num(w, "180\xC2\xB0"), 3.14159265358979, 1e-12);
    }
    // ================================================================ complessi (2-36)
    {
        Workspace w = deg();
        w.settings.complex = ComplexMode::Rectangular;
        CHECK_EQ_STR(calc(w, "2\xE2\x88\xA0" "90"), "2i");
        CHECK_EQ_STR(calc(w, "1+2\xE2\x88\xA0" "0"), "3");
        runCalculation(w, "1+i");
        HistoryEntry& h = w.history.back();
        CHECK(setComplexView(h, true));
        CHECK_EQ_STR(resultText(h, w.settings), "1.414213562\xE2\x88\xA0" "45");
        CHECK(setComplexView(h, false));
        CHECK_EQ_STR(resultText(h, w.settings), "1+i");
        runCalculation(w, "5");
        CHECK(!setComplexView(w.history.back(), true));                          // non complesso
        CHECK_EQ_STR(calc(w, "arg({i,1})"), "{90,0}");                           // Arg elemento per elemento
        w.settings.complex = ComplexMode::Real;
        CHECK(fails<NonRealError>(w, "2\xE2\x88\xA0" "30"));
    }
    // ================================================================ STAT: distribuzioni (cap. 6 in Run-Matrix)
    {
        Workspace w = deg();
        CHECK_NEAR(num(w, "norm_pd(0)"), 0.3989422804, 1e-10);
        CHECK_NEAR(num(w, "norm_cd(-1,1)"), 0.6826894921, 1e-10);
        CHECK_NEAR(num(w, "norm_cd(-1,1,2,0)"), 0.3829249225, 1e-10);
        CHECK_NEAR(num(w, "invnorm_cd(0.7)"), 0.5244005127, 1e-10);
        CHECK_NEAR(num(w, "invnorm_cd(\"L\",0.7)"), 0.5244005127, 1e-10);
        CHECK_NEAR(num(w, "invnorm_cd(\"R\",0.3)"), 0.5244005127, 1e-10);
        CHECK_EQ_STR(calc(w, "invnorm_cd(\"C\",0.95)"), "{-1.959963985,1.959963985}");
        CHECK(fails<MathError>(w, "invnorm_cd(\"X\",0.5)"));
        CHECK_NEAR(num(w, "t_pd(1,5)"), 0.2196797973, 1e-10);
        CHECK_NEAR(num(w, "t_cd(-1,1,5)"), 0.6367825323, 1e-9);
        CHECK_NEAR(num(w, "invt_cd(0.05,10)"), 1.812461123, 1e-8);
        CHECK_NEAR(num(w, "chi_pd(2,3)"), 0.2075537487, 1e-10);
        CHECK_NEAR(num(w, "invchi_cd(0.05,4)"), 9.487729037, 1e-8);
        CHECK_NEAR(num(w, "f_pd(1,2,3)"), 0.2075537487, 1e-1);
        CHECK_NEAR(num(w, "invf_cd(0.05,2,3)"), 9.552094496, 1e-7);
        CHECK_EQ_STR(calc(w, "binomial_pd(3,5,0.5)"), "0.3125");
        CHECK_EQ_STR(calc(w, "binomial_pd(3,0.5)"), "{0.125,0.375,0.375,0.125}");
        CHECK_EQ_STR(calc(w, "binomial_cd(2,3,0.5)"), "0.875");
        CHECK_EQ_STR(calc(w, "binomial_pd({0,1},2,0.5)"), "{0.25,0.5}");          // elemento per elemento
        CHECK_EQ_STR(calc(w, "invbinomial_cd(0.5,10,0.5)"), "5");
        CHECK_NEAR(num(w, "poisson_pd(2,3)"), 0.2240418077, 1e-10);
        CHECK_NEAR(num(w, "poisson_cd(2,3)"), 0.4231900811, 1e-10);
        CHECK_EQ_STR(calc(w, "invpoisson_cd(0.5,3)"), "3");
        CHECK_EQ_STR(calc(w, "geo_pd(2,0.5)"), "0.25");
        CHECK_EQ_STR(calc(w, "geo_cd(3,0.5)"), "0.875");
        CHECK_EQ_STR(calc(w, "invgeo_cd(0.875,0.5)"), "3");
        CHECK_EQ_STR(calc(w, "hypergeo_pd(1,3,4,10)"), "0.5");
        CHECK_NEAR(num(w, "hypergeo_cd(1,3,4,10)"), 1.0 / 6.0 + 0.5, 1e-12);
        CHECK_EQ_STR(calc(w, "invhypergeo_cd(0.6,3,4,10)"), "1");
        // S-Dev, Var, Mean e Median con le frequenze (3-10)
        CHECK_NEAR(num(w, "s_dev({1,2,3,4})"), 1.290994449, 1e-9);
        CHECK_NEAR(num(w, "stat_var({1,2,3,4})"), 5.0 / 3.0, 1e-12);
        CHECK_EQ_STR(calc(w, "Mean({1,2,3},{1,1,2})"), "9/4");
        CHECK_EQ_STR(calc(w, "Median({1,2,3,4},{1,1,1,1})"), "2.5");
        CHECK_EQ_STR(calc(w, "Median({10,20,30},{1,5,1})"), "20");
        CHECK_EQ_STR(calc(w, "Min({1,5,3},{4,2,6})"), "{1,2,3}");                 // due liste (3-9)
        CHECK_EQ_STR(calc(w, "Max({1,5,3},{4,2,6})"), "{4,5,6}");
        CHECK(fails<DimensionError>(w, "Mean({1,2},{1,2,3})"));
        // x̂ e ŷ: regressione dell'ultimo calcolo di Statistics
        CHECK(fails<MathError>(w, "y_hat(1)"));
        w.setList(0, {1, 2, 3, 4}); w.setList(1, {3, 5, 7, 9});
        w.settings.statRegType = static_cast<std::uint8_t>(stats::RegType::Linear);
        CHECK_EQ_STR(calc(w, "y_hat(10)"), "21");                                  // y = 2x + 1
        CHECK_EQ_STR(calc(w, "x_hat(21)"), "10");
        CHECK_EQ_STR(calc(w, "reg_a"), "2");
        CHECK_EQ_STR(calc(w, "reg_r"), "1");
        w.setList(1, {1, 4, 9, 16});
        w.settings.statRegType = static_cast<std::uint8_t>(stats::RegType::Quad);
        CHECK_EQ_STR(calc(w, "x_hat(9)"), "{-3,3}");                               // Quad: due valori
        CHECK_EQ_STR(calc(w, "stat_n+stat_sumx"), "14");                           // VARS STAT
        CHECK_EQ_STR(calc(w, "stat_maxy"), "16");
    }
    // ================================================================ TVM (cap. 7, funzioni in OPTN)
    {
        Workspace w = deg();
        CHECK_NEAR(num(w, "smpl_si(120,7,-300)"), 6.904109589, 1e-9);
        CHECK_NEAR(num(w, "smpl_sfv(120,7,-300)"), 306.9041096, 1e-7);
        CHECK_NEAR(num(w, "cmpd_fv(10,5,-100,0,1,1)"), 162.8894627, 1e-7);
        CHECK_NEAR(num(w, "cmpd_pv(10,5,0,162.8894627,1,1)"), -100, 1e-6);
        CHECK_NEAR(num(w, "cmpd_pmt(12,6,1000,0,12,12)"), -86.06642971, 1e-7);
        CHECK_NEAR(num(w, "cmpd_n(5,-100,0,162.8894627,1,1)"), 10, 1e-7);
        CHECK_NEAR(num(w, "cmpd_i(10,-100,0,162.8894627,1,1)"), 5, 1e-6);
        CHECK_NEAR(num(w, "cash_npv(10,{-100,50,60})"), -4.958677686, 1e-9);
        CHECK_NEAR(num(w, "cash_irr({-100,50,60})"), 6.39410298, 1e-7);
        CHECK_NEAR(num(w, "cash_nfv(10,{-100,50,60})"), -6, 1e-9);
        CHECK_NEAR(num(w, "cash_pbp(0,{-100,50,60})"), 1 + 50.0 / 60.0, 1e-12);
        CHECK_NEAR(num(w, "cnvt_eff(4,12)"), 12.55088100, 1e-8);
        CHECK_NEAR(num(w, "cnvt_apr(4,12.550881)"), 12, 1e-6);
        CHECK_NEAR(num(w, "fin_cost(200,25)"), 150, 1e-12);
        CHECK_NEAR(num(w, "fin_sell(150,25)"), 200, 1e-12);
        CHECK_NEAR(num(w, "fin_margin(150,200)"), 25, 1e-12);
        CHECK_EQ_STR(calc(w, "days_prd(1,1,2020,3,1,2020)"), "60");               // 2020 bisestile
        CHECK(fails<MathError>(w, "days_prd(2,30,2021,3,1,2021)"));
        const double bal = num(w, "amt_bal(1,12,12,1000,-88.84878868,12,12)");
        CHECK(std::abs(bal) < 1e-6);                                              // prestito estinto
        CHECK_NEAR(num(w, "amt_int(1,1,12,1000,-88.84878868,12,12)"), -10, 1e-9);
        CHECK_NEAR(num(w, "amt_sint(1,12,12,1000,-88.84878868,12,12)") + num(w, "amt_sprn(1,12,12,1000,-88.84878868,12,12)"),
                   12 * -88.84878868, 1e-6);
    }
    // ================================================================ CONVERT (2-64…)
    {
        Workspace w = deg();
        CHECK_NEAR(num(w, "10[cm]\xE2\x96\xB6[in]"), 3.937007874, 1e-9);
        CHECK_EQ_STR(calc(w, "100[\xC2\xB0" "C]\xE2\x96\xB6[\xC2\xB0" "F]"), "212");
        CHECK_EQ_STR(calc(w, "0[\xC2\xB0" "C]\xE2\x96\xB6[K]"), "273.15");
        CHECK_EQ_STR(calc(w, "1[km]\xE2\x96\xB6[m]+1"), "1001");
        CHECK_EQ_STR(calc(w, "2[h]\xE2\x96\xB6[min]"), "120");
        CHECK_NEAR(num(w, "1[atm]\xE2\x96\xB6[mmHg]"), 760, 1e-6);
        CHECK(fails<MathError>(w, "1[m]\xE2\x96\xB6[kg]"));                       // grandezze diverse
        CHECK(fails<SyntaxError>(w, "1[m]"));                                       // manca ▶[...]
        CHECK_EQ_STR(calc(w, "{1,2}[m]\xE2\x96\xB6[cm]"), "{100,200}");
        // Matrici: [m] dopo un nome di matrice resta un indice
        runCalculation(w, "[[5,6]]" + kArrow + "Mat A");
        runCalculation(w, "2" + kArrow + "M");
        CHECK_EQ_STR(calc(w, "Mat A[1,2]"), "6");
        const char* const* cats = calclib::unitCategories();
        std::size_t total = 0, k = 0;
        for (; cats[k]; ++k) total += calclib::unitsOf(k).size();
        CHECK(k == 11 && total > 100);
    }
    // ================================================================ ESYM P/E, ENG/ENG←, overflow (2-23, 2-4)
    {
        Workspace w = deg();
        CHECK_EQ_STR(calc(w, "3#P"), "3E+15");
        CHECK_EQ_STR(calc(w, "2#E"), "2E+18");
        w.settings.display = {DisplayMode::Eng, 0};
        CHECK_EQ_STR(calc(w, "3#P\xC3\x97" "2"), "6P");
        CHECK_EQ_STR(calc(w, "1.5#E"), "1.5E");
        w.settings.display = {DisplayMode::Norm1, 0};
        runCalculation(w, "1234");
        HistoryEntry& h = w.history.back();
        CHECK(shiftEngineering(h, 1));
        CHECK_EQ_STR(resultText(h, w.settings), "1.234E+03");
        CHECK(shiftEngineering(h, 1));
        CHECK_EQ_STR(resultText(h, w.settings), "0.001234E+06");
        CHECK(shiftEngineering(h, -1) && shiftEngineering(h, -1));
        CHECK_EQ_STR(resultText(h, w.settings), "1234E+00");
        CHECK(shiftEngineering(h, -1));
        CHECK_EQ_STR(resultText(h, w.settings), "1234000E-03");
        CHECK_NEAR(w.memory.answer(), 1234, 0);                                    // il valore non cambia
        runCalculation(w, "{1,2}");
        CHECK(!shiftEngineering(w.history.back(), 1));
        CHECK(fails<MathError>(w, "1E99\xC3\x97" "10"));
        CHECK(failText(w, "70!").rfind("Math ERROR", 0) == 0);                      // 70! > 10^100
        CHECK_EQ_STR(calc(w, "69!>1E98"), "1");
        CHECK(fails<MathError>(w, "{1E99}\xC3\x97" "10"));
    }
    // ================================================================ Simplify Manual (2-22, 2-26)
    {
        Workspace w = deg();
        w.settings.simplifyManual = true;
        CHECK_EQ_STR(calc(w, "15/60"), "15/60");                                   // come scritta
        CHECK_EQ_STR(calc(w, "1/2+1/3"), "5/6");                                   // m.c.m. 6
        CHECK_EQ_STR(calc(w, "2/4+1/4"), "3/4");                                   // m.c.m. 4, non ridotta
        CHECK_EQ_STR(calc(w, "1/6+1/6"), "2/6");
        CHECK_EQ_STR(calc(w, "3/4-1/4"), "2/4");
        CHECK_EQ_STR(calc(w, "1/2\xC3\x97" "2/3"), "2/6");                          // prodotti
        CHECK_EQ_STR(calc(w, "(2/4)\xC3\xB7(1/2)"), "1");                            // 4/4: un intero si mostra intero
        CHECK_EQ_STR(calc(w, "(1/2)^2"), "1/4");
        CHECK_EQ_STR(calc(w, "1+1/2"), "3/2");
        CHECK_EQ_STR(calc(w, "2/6" + kSimp), "1/3  F=2");
        CHECK_EQ_STR(calc(w, kSimp), "1/3");                                       // irriducibile: senza F=
        CHECK_EQ_STR(calc(w, "27/63" + kSimp + "9"), "3/7  F=9");
        CHECK(fails<MathError>(w, "27/63" + kSimp + "2"));                         // divisore non valido
        CHECK_EQ_STR(calc(w, "0.5+1/4"), "0.75");                                  // decimali: decimale
        w.settings.complex = ComplexMode::Rectangular;
        CHECK_EQ_STR(calc(w, "15/60"), "1/4");                                     // Manual solo nel modo Real
        w.settings.complex = ComplexMode::Real;
        w.settings.simplifyManual = false;
        CHECK_EQ_STR(calc(w, "1/6+1/6"), "1/3");
        std::int64_t n = 0, d = 0;
        CHECK(evaluateUnreducedFraction(*parseExpression("2" + std::string("\xE2\x8C\x9F") + "4+1" + "\xE2\x8C\x9F" + "4"), n, d) && n == 3 && d == 4);
        CHECK(!evaluateUnreducedFraction(*parseExpression("1/2+x"), n, d));
    }
    // ================================================================ Base-N (2-39…2-42)
    {
        Workspace w = deg();
        w.settings.baseMode = Settings::BaseMode::Bin;
        CHECK_EQ_STR(calc(w, "111+1"), "1000");
        CHECK_EQ_STR(calc(w, "0-1"), "1111111111111111");                          // 16 bit
        CHECK(fails<MathError>(w, "0111111111111111+1"));                          // oltre 16 bit
        CHECK(fails<MathError>(w, "11111111111111111"));                           // 17 cifre
        CHECK_EQ_STR(calc(w, "{1,10,11}+1"), "{10,11,100}");                       // lista nella base
        CHECK_EQ_STR(calc(w, "not(0)"), "1111111111111111");
        w.settings.baseMode = Settings::BaseMode::Hex;
        CHECK_EQ_STR(calc(w, "A+1"), "B");                                         // A-F sono cifre
        runCalculation(w, "5" + kArrow + "v_a");                                   // la variabile A
        CHECK(w.memory.recall('A') == 5);
        CHECK_EQ_STR(calc(w, "v_a+A"), "F");
        CHECK_EQ_STR(calc(w, "[[FF,1][2,3]]"), "[[FF,1][2,3]]");                    // matrice nella base
        CHECK_EQ_STR(calc(w, "FFFFFFFF"), "FFFFFFFF");
        CHECK(fails<MathError>(w, "FFFFFFFF+0d1+7FFFFFFF+7FFFFFFF"));
        CHECK(fails<SyntaxError>(w, "1.5"));
        CHECK(fails<MathError>(w, "sin(1)"));
        w.settings.baseMode = Settings::BaseMode::Oct;
        CHECK_EQ_STR(calc(w, "7+1"), "10");
        CHECK_EQ_STR(calc(w, "0-1"), "37777777777");
        w.settings.baseMode = Settings::BaseMode::Comp;
        CHECK_EQ_STR(baseNText(-1, 2), "1111111111111111");
        CHECK_EQ_STR(baseNText(40000, 2), "Math ERROR");
        CHECK_EQ_STR(calc(w, "v_a"), "5");                                          // v_a vale anche fuori da Base-N
    }
    // ================================================================ messaggi d'errore (Appendice α)
    {
        Workspace w = deg();
        CHECK(failText(w, "Det({1,2})").rfind("Argument ERROR", 0) == 0);           // niente Type ERROR
        w.settings.complex = ComplexMode::Rectangular;
        CHECK(failText(w, "{1+i}" + kArrow + "List 1").rfind("Complex Number In List", 0) == 0);
        runCalculation(w, "[[1+i]]" + kArrow + "Mat B");
        w.settings.complex = ComplexMode::Real;
        CHECK(failText(w, "Mat B").rfind("Complex Number In Matrix", 0) == 0);
        CHECK(failText(w, "-10" + kArrow + "xmax").rfind("Range ERROR", 0) == 0);  // Xmax < Xmin
        CHECK(failText(w, "ViewWindow 1,0,1,0,1,1").rfind("Range ERROR", 0) == 0);
        CHECK(failText(w, "1+").rfind("Syntax ERROR", 0) == 0);
        CHECK(failText(w, "1/0").rfind("Math ERROR", 0) == 0);
    }
    return testutil::finish("calc_v112_test");
}
