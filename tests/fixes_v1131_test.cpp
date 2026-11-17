// v1.13.1 — versione di correzione: un gruppo di controlli per ogni errore di
// docs/reports/ERRORI_v1.13.1.md (E01...E09). Ogni gruppo riproduce l'errore
// della v1.13.0 (fallisce con quei sorgenti) e verifica la correzione.
#include "calc_library.hpp"
#include "csv_io.hpp"
#include "equation.hpp"
#include "file_store.hpp"
#include "run_matrix.hpp"
#include "statistics.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

// ---------------------------------------------------------------- byte richiesti all'allocatore
// operator new contato (E06, E09): byte chiesti mentre `g_tracking` e' attivo.
namespace {
bool g_tracking = false;
std::size_t g_requested = 0;
} // namespace
void* operator new(std::size_t n) {
    if (g_tracking) g_requested += n;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace fx;

namespace {
const std::uint8_t kV1110Equation[] = {
#include "fixtures/workspace_v1_11_0.inc"
};
using Bytes = std::vector<std::uint8_t>;
// Posizione dell'unica occorrenza di `pattern` (npos se assente o ripetuto).
std::size_t findOnce(const Bytes& data, const Bytes& pattern) {
    std::size_t found = std::string::npos, count = 0;
    for (std::size_t i = 0; i + pattern.size() <= data.size(); ++i)
        if (std::equal(pattern.begin(), pattern.end(), data.begin() + static_cast<std::ptrdiff_t>(i))) { found = i; ++count; }
    return count == 1 ? found : std::string::npos;
}
Bytes doubleBytes(double v) { Bytes b(8); std::memcpy(b.data(), &v, 8); return b; }
// Un record accettato deve restare leggibile dopo il salvataggio (anche dopo un cambio di List File).
bool reloadable(const Workspace& w) {
    const Bytes again = serializeWorkspace(w);
    Workspace back;
    return deserializeWorkspace(again.data(), again.size(), back);
}
double millis(const std::chrono::steady_clock::time_point& t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}
double runValue(Workspace& w, const std::string& text) {
    try { runCalculation(w, text); } catch (const std::exception&) { return std::nan(""); }
    return w.history.back().value.real();
}
template <typename E> bool runFails(Workspace& w, const std::string& text) {
    try { runCalculation(w, text); } catch (const E&) { return true; } catch (...) { return false; }
    return false;
}
} // namespace

int main() {
    // ================================================================ E01 record precedenti non piu' rileggibili
    {
        // (a) Coefficiente di EQUATION non finito in un record v7 (v1.11.0): la v1.13.0
        // lo accettava e il record v9 salvato subito dopo veniva rifiutato all'avvio
        // successivo (workspace azzerato). Ora il record e' rifiutato alla lettura.
        Bytes rec(kV1110Equation, kV1110Equation + sizeof(kV1110Equation));
        const std::size_t at = findOnce(rec, doubleBytes(0.1));       // cella SIML in piu' (0.1)
        CHECK(at != std::string::npos);
        if (at != std::string::npos) {
            const Bytes nan = doubleBytes(std::nan(""));
            std::copy(nan.begin(), nan.end(), rec.begin() + static_cast<std::ptrdiff_t>(at));
            Workspace w;
            const bool loaded = deserializeWorkspace(rec.data(), rec.size(), w);
            CHECK(!loaded || reloadable(w));
            CHECK(!loaded);
            CHECK(w.equation.unknowns == 2);                        // workspace intatto
        }
        // Il record originale resta leggibile e rileggibile.
        {
            Workspace w;
            CHECK(deserializeWorkspace(kV1110Equation, sizeof(kV1110Equation), w));
            CHECK(reloadable(w));
        }
        // (b) Equazione del Solver piu' lunga del limite in un record v7.
        Bytes rec2(kV1110Equation, kV1110Equation + sizeof(kV1110Equation));
        const std::string eq = "x^3-2x=1.5";
        Bytes pattern{static_cast<std::uint8_t>(eq.size()), 0};
        pattern.insert(pattern.end(), eq.begin(), eq.end());
        const std::size_t sp = findOnce(rec2, pattern);
        CHECK(sp != std::string::npos);
        if (sp != std::string::npos) {
            std::string longEq;
            while (longEq.size() < limits::kEquationExpression + 20) longEq += "x+";
            longEq += "1=0";
            Bytes repl{static_cast<std::uint8_t>(longEq.size() & 0xFF), static_cast<std::uint8_t>(longEq.size() >> 8)};
            repl.insert(repl.end(), longEq.begin(), longEq.end());
            rec2.erase(rec2.begin() + static_cast<std::ptrdiff_t>(sp), rec2.begin() + static_cast<std::ptrdiff_t>(sp + pattern.size()));
            rec2.insert(rec2.begin() + static_cast<std::ptrdiff_t>(sp), repl.begin(), repl.end());
            Workspace w;
            const bool loaded = deserializeWorkspace(rec2.data(), rec2.size(), w);
            CHECK(!loaded || reloadable(w));
            CHECK(!loaded);
        }
        // (c) Elemento non finito in una lista del file attivo: dopo un cambio di
        // List File la lista passa nel blocco dei List File, che lo rifiuta.
        Workspace src;
        src.lists[0] = {1.0, 1234.5678, 3.0};
        Bytes rec3 = serializeWorkspace(src);
        const std::size_t lp = findOnce(rec3, doubleBytes(1234.5678));
        CHECK(lp != std::string::npos);
        if (lp != std::string::npos) {
            const Bytes inf = doubleBytes(INFINITY);
            std::copy(inf.begin(), inf.end(), rec3.begin() + static_cast<std::ptrdiff_t>(lp));
            Workspace w;
            const bool loaded = deserializeWorkspace(rec3.data(), rec3.size(), w);
            if (loaded) w.selectListFile(1);
            CHECK(!loaded || reloadable(w));
            CHECK(!loaded);
        }
    }

    // ================================================================ E02 HypergeoCD con x grande
    {
        // La somma andava da 0 a x anche oltre min(n, M): con x = 3E8 secondi sul PC,
        // minuti sulla PicoCalc (nessun AC durante il calcolo).
        const auto t0 = std::chrono::steady_clock::now();
        CHECK_NEAR(calclib::hypergeoCD(3e8, 5, 5, 10), 1.0, 1e-12);
        CHECK(millis(t0) < 200);
        CHECK_NEAR(calclib::hypergeoCD(2, 5, 5, 10), 0.5, 1e-12);          // 1+25+100 su 252 = 0.5
        CHECK_NEAR(calclib::hypergeoCD(2.5, 5, 5, 10), 0.5, 1e-12);
        CHECK_NEAR(calclib::hypergeoCD(-1, 5, 5, 10), 0.0, 1e-15);
        CHECK_THROWS(calclib::hypergeoCD(1e12, 5, 5, 4));                   // n > N: Argument ERROR anche con x grande
    }

    // ================================================================ E03 inverse discrete in tempo quadratico
    {
        // Ogni candidato ricalcolava la somma da 0: InvBinomialCD(0.5,20000,0.5) 2,5 s sul PC.
        const auto t0 = std::chrono::steady_clock::now();
        CHECK(calclib::invBinomialCD(0.5, 20000, 0.5) == 10000);
        CHECK(calclib::invPoissonCD(0.5, 20000) == 20000);
        CHECK(calclib::invHypergeoCD(0.5, 4000, 4000, 8000) == 2000);
        CHECK(millis(t0) < 300);
        // Stesso risultato della definizione (il piu' piccolo x con CD(x) >= prob) su casi piccoli.
        bool same = true;
        for (double n : {1.0, 7.0, 30.0})
            for (double p : {0.1, 0.5, 0.93})
                for (double prob : {0.0, 0.05, 0.5, 0.99, 1.0}) {
                    double x = 0;
                    while (x < n && calclib::binomialCD(x, n, p) < prob - 1e-10) ++x;
                    same = same && calclib::invBinomialCD(prob, n, p) == x;
                    double y = 0;
                    while (calclib::poissonCD(y, n) < prob - 1e-10) ++y;
                    same = same && calclib::invPoissonCD(prob, n) == y;
                    double z = 0;
                    const double N = n + 10, M = std::floor(n / 2), k = std::min(n, M);
                    while (z < k && calclib::hypergeoCD(z, n, M, N) < prob - 1e-10) ++z;
                    same = same && calclib::invHypergeoCD(prob, n, M, N) == z;
                }
        CHECK(same);
        CHECK_THROWS(calclib::invBinomialCD(1.5, 10, 0.5));
        CHECK_THROWS(calclib::invPoissonCD(0.5, 0));
    }

    // ================================================================ E04 forma esatta complessa sbagliata
    {
        Workspace w;
        const EvalContext c = w.evalContext();
        // Abs, Conjg, ReP... non sono polinomi in i: 1E10+Abs(i) valeva 1E+10+i.
        const eq::Number a = eq::evaluateNumber("1E10+abs(i)", c);
        CHECK_NEAR(a.value.real(), 1e10 + 1, 1e-6);
        CHECK_NEAR(a.value.imag(), 0.0, 1e-9);
        const eq::Number b = eq::evaluateNumber("1E10+conj(i)", c);
        CHECK_NEAR(b.value.real(), 1e10, 1e-6);
        CHECK_NEAR(b.value.imag(), -1.0, 1e-9);
        const eq::Number d = eq::evaluateNumber("1E10+1E10i+abs(i)", c);
        CHECK_NEAR(d.value.real(), 1e10 + 1, 1e-6);
        CHECK_NEAR(d.value.imag(), 1e10, 1e-6);
        const eq::Number r = eq::evaluateNumber("1E10+re(i)", c);
        CHECK_NEAR(r.value.imag(), 0.0, 1e-9);
        // Le forme esatte dei polinomi in i restano.
        const eq::Number p = eq::evaluateNumber("(1+i)^2", c);
        CHECK(p.hasExact && p.exact.re.isZero() && p.exact.im == exact::Value::fromInteger(2));
        const eq::Number q = eq::evaluateNumber("(1+sqrt(3)i)/2", c);
        CHECK(q.hasExact);
        CHECK_NEAR(q.value.imag(), std::sqrt(3.0) / 2, 1e-12);
        const eq::Number s = eq::evaluateNumber("3+4i", c);
        CHECK(s.hasExact && s.exact.re == exact::Value::fromInteger(3) && s.exact.im == exact::Value::fromInteger(4));
        const eq::Number t = eq::evaluateNumber("2i^3-i", c);        // -3i
        CHECK(t.hasExact && t.exact.im == exact::Value::fromInteger(-3));
    }

    // ================================================================ E05 nCr / nPr con n > 100000
    {
        // fx-CG50 (Appendice, intervalli di input): n, r interi, 0 <= r <= n < 1E10, risultato < 1E100.
        Workspace w;
        CHECK_NEAR(runValue(w, "nCr(1000000,2)"), 499999500000.0, 0.5);
        CHECK_NEAR(runValue(w, "nPr(200000,2)"), 39999800000.0, 0.5);
        CHECK_NEAR(runValue(w, "nCr(9999999999,1)"), 9999999999.0, 0.5);
        CHECK_NEAR(runValue(w, "nPr(100001,3)") / 999999999900000.0, 1.0, 1e-12);
        CHECK_NEAR(runValue(w, "nCr(10,3)"), 120.0, 0.0);
        CHECK(runFails<MathError>(w, "nCr(10000000000,1)"));     // n >= 1E10
        CHECK(runFails<MathError>(w, "nCr(1000000,100000)"));    // risultato >= 1E100
        CHECK(runFails<MathError>(w, "nPr(1000,500)"));
        CHECK(runFails<MathError>(w, "nCr(5,6)"));
        CHECK(runFails<MathError>(w, "nCr(2.5,1)"));
    }

    // ================================================================ E06 CSV oltre i limiti: memoria prima del controllo
    {
        // Un file entro maxFileBytes con troppe colonne o righe allocava una stringa per
        // campo e per riga (centinaia di KB sul Pico 2 W, piu' dello heap) prima di
        // dare Dimension ERROR.
        const csv::Format f;
        const std::string wide(40000, ',');
        g_tracking = true; g_requested = 0;
        bool dimension = false;
        try { (void)csv::decode(reinterpret_cast<const std::uint8_t*>(wide.data()), wide.size(), f, 999, 26); }
        catch (const DimensionError&) { dimension = true; }
        g_tracking = false;
        CHECK(dimension);
        CHECK(g_requested < 64 * 1024);
        std::string tall;
        for (int i = 0; i < 20000; ++i) tall += "1\n";
        g_tracking = true; g_requested = 0;
        dimension = false;
        try { (void)csv::decode(reinterpret_cast<const std::uint8_t*>(tall.data()), tall.size(), f, 255, 26); }
        catch (const DimensionError&) { dimension = true; }
        g_tracking = false;
        CHECK(dimension);
        CHECK(g_requested < 64 * 1024);
        // Una matrice dal file: anche il numero di celle si controlla prima di allocarle.
        std::string cells;
        for (int r = 0; r < 300; ++r) { for (int k = 0; k < 300; ++k) cells += k ? ",1" : "1"; cells += "\n"; }
        g_tracking = true; g_requested = 0;
        dimension = false;
        try { (void)csv::decode(reinterpret_cast<const std::uint8_t*>(cells.data()), cells.size(), f, 999, 999, 10000); }
        catch (const DimensionError&) { dimension = true; }
        g_tracking = false;
        CHECK(dimension);
        CHECK(g_requested < 64 * 1024);
        // I file validi si leggono come prima.
        const std::string ok = "\"A\",\"B\"\r\n1,2\r\n3,\r\n5,6\n";
        const csv::Table t = csv::decode(reinterpret_cast<const std::uint8_t*>(ok.data()), ok.size(), f, 999, 26);
        CHECK(t.rows == 3 && t.cols == 2);
        CHECK(t.has(0, 0) && t.at(0, 0) == 1 && t.at(2, 1) == 6 && !t.has(1, 1));
        const std::string semi = "1,5;2\n3;4,25\n";
        const csv::Table u = csv::decode(reinterpret_cast<const std::uint8_t*>(semi.data()), semi.size(), {';', ','}, 999, 26);
        CHECK(u.rows == 2 && u.cols == 2 && u.at(0, 0) == 1.5 && u.at(1, 1) == 4.25);
        CHECK_THROWS_AS(csv::decode(reinterpret_cast<const std::uint8_t*>("1,2\n3,x\n"), 8, f, 999, 26), MathError);
        CHECK_THROWS_AS(csv::decode(reinterpret_cast<const std::uint8_t*>("1,2,3\n"), 6, f, 999, 2), DimensionError);
        CHECK_THROWS_AS(csv::decode(reinterpret_cast<const std::uint8_t*>("\n\n"), 2, f, 999, 2), MathError);
    }

    // ================================================================ E07 salvataggio atomico dopo un'interruzione
    {
        // Stato lasciato da un salvataggio interrotto dopo "file -> .bak": nuovo .tmp
        // completo e vecchio .bak, nessun file. Un secondo salvataggio interrotto
        // mentre scrive il .tmp faceva poi promuovere da recover() un .tmp parziale
        // (il file vecchio era perso).
        files::RamFileSystem fs(1 << 20);
        CHECK(fs.makeDirs(csv::Folder) == files::Status::Ok);
        const std::string base = csv::pathOf("DATA");
        const std::string oldText = "1,2,3\r\n", newer = "4,5,6\r\n", newest = "7,8,9\r\n10,11,12\r\n";
        CHECK(files::writeAtomic(fs, base + ".bak", reinterpret_cast<const std::uint8_t*>(oldText.data()), oldText.size()) == files::Status::Ok);
        CHECK(files::writeAtomic(fs, base + ".tmp", reinterpret_cast<const std::uint8_t*>(newer.data()), newer.size()) == files::Status::Ok);
        CHECK(!fs.exists(base));
        fs.failAfterBytes(5);                                    // corrente persa durante la scrittura del nuovo .tmp
        CHECK(csv::save(fs, "DATA", newest) != files::Status::Ok);
        fs.powerOn();
        Bytes data;
        CHECK(csv::load(fs, "DATA", data) == files::Status::Ok);
        const std::string got(data.begin(), data.end());
        CHECK(got == oldText || got == newer);                   // mai il .tmp parziale
        CHECK(!fs.exists(base + ".tmp"));
        // Un salvataggio normale dopo il ripristino.
        CHECK(csv::save(fs, "DATA", newest) == files::Status::Ok);
        CHECK(csv::load(fs, "DATA", data) == files::Status::Ok);
        CHECK(std::string(data.begin(), data.end()) == newest);
        CHECK(!fs.exists(base + ".bak") && !fs.exists(base + ".tmp"));
    }

    // ================================================================ E08 quantili t oltre la finestra di bisezione
    {
        // df = 1: t(0.999995) = 1/tan(5E-6·π) = 63661.977...; la ricerca era limitata a ±1000.
        const stats::Interval iv = stats::tInterval({1, 2}, 0.99999);
        const double t = 1.0 / std::tan(5e-6 * 3.14159265358979323846);
        CHECK_NEAR(iv.upper - iv.lower, 2 * t * std::sqrt(0.5) / std::sqrt(2.0), 1e-4 * t);
        // InvTCD(area, df) = x con P(X >= x) = area: l'intervallo [-1E4, 1E4] tagliava le code.
        CHECK_NEAR(calclib::invTCD(0.9999999, 1) / -3183098.8618, 1.0, 1e-8);
        CHECK_NEAR(calclib::invTCD(0.0000001, 1) / 3183098.8618, 1.0, 1e-8);
        CHECK(calclib::invTCD(0.5, 1) == 0);
        CHECK(calclib::invTCD(0.5, 7) == 0);
        CHECK_NEAR(calclib::invTCD(0.05, 10), 1.812461123, 1e-8);
        CHECK_NEAR(calclib::invTCD(0.95, 10), -1.812461123, 1e-8);
    }

    // ================================================================ E09 1-Variable con le frequenze
    {
        // Le frequenze (fino a 100000 per dato) moltiplicavano i dati in memoria:
        // 3 dati da 100000 = 300 000 double copiati e ordinati.
        const std::vector<double> x{1, 2, 3}, f{100000, 100000, 100000};
        g_tracking = true; g_requested = 0;
        const stats::OneVar o = stats::oneVariable(x, &f);
        g_tracking = false;
        CHECK(g_requested < 64 * 1024);
        CHECK(o.n == 300000 && o.mean == 2 && o.median == 2 && o.q1 == 1 && o.q3 == 3 && o.min == 1 && o.max == 3);
        CHECK(o.modes.size() == 3 && o.modeCount == 100000);
        // Stessi risultati dei dati ripetuti.
        const std::vector<double> xs{4, 1, 7, 1, 3}, fs{2, 3, 0, 1, 5};
        std::vector<double> expanded;
        for (std::size_t i = 0; i < xs.size(); ++i) for (int k = 0; k < static_cast<int>(fs[i]); ++k) expanded.push_back(xs[i]);
        const stats::OneVar a = stats::oneVariable(xs, &fs), b = stats::oneVariable(expanded);
        CHECK(a.n == b.n && a.min == b.min && a.max == b.max && a.median == b.median && a.q1 == b.q1 && a.q3 == b.q3);
        CHECK_NEAR(a.mean, b.mean, 1e-12);
        CHECK_NEAR(a.sum, b.sum, 1e-12);
        CHECK_NEAR(a.sumSq, b.sumSq, 1e-12);
        CHECK_NEAR(a.popSd, b.popSd, 1e-12);
        CHECK_NEAR(a.sampleSd, b.sampleSd, 1e-12);
        CHECK(a.modes == b.modes && a.modeCount == b.modeCount);
        const std::vector<double> odd{5, 9}, oddF{2, 1};          // 5 5 9: Q1 = 5, Q3 = 9
        const stats::OneVar c = stats::oneVariable(odd, &oddF);
        CHECK(c.q1 == 5 && c.q3 == 9 && c.median == 5);
        const std::vector<double> zero{0, 0};
        CHECK_THROWS(stats::oneVariable(odd, &zero));
    }
    // ================================================================ E10 Ran# n fuori intervallo
    {
        // n = 2E13 veniva convertito in int prima del controllo (comportamento indefinito).
        Workspace w;
        CHECK(runFails<MathError>(w, "ran(20000000000000)"));
        CHECK(runFails<MathError>(w, "ran(-3E15)"));
        CHECK(runFails<MathError>(w, "ran(11)"));
        CHECK(runFails<MathError>(w, "ran(2.5)"));
        const double r1 = runValue(w, "ran(3)");
        CHECK(r1 >= 0 && r1 < 1);
        CHECK(runValue(w, "ran(0)") == 0);
    }

    // ================================================================ E12 Σ/Π con estremi oltre 9.2E18
    {
        // Gli estremi si convertivano in long long (indefinito oltre 9.2E18; sulla
        // PicoCalc il contatore traboccava in un ciclo senza fine).
        Workspace w;
        const auto t0 = std::chrono::steady_clock::now();
        CHECK_NEAR(runValue(w, "sum(x,x,1E19,1E19)") / 1e19, 1.0, 1e-12);
        CHECK_NEAR(runValue(w, "prod(2,x,-1E99,-1E99)"), 2.0, 0.0);
        CHECK(millis(t0) < 500);
        CHECK_NEAR(runValue(w, "sum(x,x,1,100)"), 5050.0, 0.0);
        CHECK_NEAR(runValue(w, "sum(x^2,x,-3,3)"), 28.0, 0.0);
        CHECK_NEAR(runValue(w, "sum(x,x,5,1)"), 0.0, 0.0);              // nessun termine
        CHECK_NEAR(runValue(w, "prod(x,x,1,6)"), 720.0, 0.0);
        CHECK(runFails<MathError>(w, "sum(x,x,1,200000)"));
        CHECK(runFails<MathError>(w, "sum(x,x,1.5,3)"));
    }
    return testutil::finish("fixes_v1131_test");
}
