// Libreria di funzioni di Run-Matrix (vedi calc_library.hpp).
#include "calc_library.hpp"
#include "expression.hpp"
#include "math_engine.hpp"
#include "statistics.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace fx::calclib {
namespace {
constexpr double kPi = 3.14159265358979323846;
[[noreturn]] void argumentError(const char* what) { throw MathError(std::string("Argument ERROR: ") + what); }
void need(bool ok, const char* what) { if (!ok) argumentError(what); }
bool integral(double v) { return std::isfinite(v) && v == std::floor(v); }

// ---------------------------------------------------------------- unita'
const char* const kCategories[] = {"LENGTH", "AREA", "VOLUME", "TIME", "TMPR", "VELOCITY", "MASS", "FORCE", "PRESSURE", "ENERGY", "POWER", nullptr};
// Fattori verso le unita' SI (m, m², m³, s, K, m/s, kg, N, Pa, J, W).
const UnitInfo kUnits[] = {
    {"fm", 0, 1e-15, 0}, {"\xC3\x85", 0, 1e-10, 0}, {"\xC2\xB5m", 0, 1e-6, 0}, {"mm", 0, 1e-3, 0}, {"cm", 0, 1e-2, 0},
    {"m", 0, 1, 0}, {"km", 0, 1e3, 0}, {"AU", 0, 1.495978707e11, 0}, {"l.y.", 0, 9.4607304725808e15, 0},
    {"pc", 0, 3.0856775814913673e16, 0}, {"Mil", 0, 2.54e-5, 0}, {"in", 0, 0.0254, 0}, {"ft", 0, 0.3048, 0},
    {"yd", 0, 0.9144, 0}, {"fath", 0, 1.8288, 0}, {"rd", 0, 5.0292, 0}, {"mile", 0, 1609.344, 0}, {"n mile", 0, 1852, 0},
    {"cm\xC2\xB2", 1, 1e-4, 0}, {"m\xC2\xB2", 1, 1, 0}, {"ha", 1, 1e4, 0}, {"km\xC2\xB2", 1, 1e6, 0}, {"in\xC2\xB2", 1, 6.4516e-4, 0},
    {"ft\xC2\xB2", 1, 0.09290304, 0}, {"yd\xC2\xB2", 1, 0.83612736, 0}, {"acre", 1, 4046.8564224, 0}, {"mile\xC2\xB2", 1, 2589988.110336, 0},
    {"cm\xC2\xB3", 2, 1e-6, 0}, {"mL", 2, 1e-6, 0}, {"L", 2, 1e-3, 0}, {"m\xC2\xB3", 2, 1, 0}, {"in\xC2\xB3", 2, 1.6387064e-5, 0},
    {"ft\xC2\xB3", 2, 0.028316846592, 0}, {"fl_oz(UK)", 2, 2.84130625e-5, 0}, {"fl_oz(US)", 2, 2.95735295625e-5, 0},
    {"gal(US)", 2, 3.785411784e-3, 0}, {"gal(UK)", 2, 4.54609e-3, 0}, {"pt", 2, 4.73176473e-4, 0}, {"qt", 2, 9.46352946e-4, 0},
    {"tsp", 2, 4.92892159375e-6, 0}, {"tbsp", 2, 1.478676478125e-5, 0}, {"cup", 2, 2.365882365e-4, 0},
    {"ns", 3, 1e-9, 0}, {"\xC2\xB5s", 3, 1e-6, 0}, {"ms", 3, 1e-3, 0}, {"s", 3, 1, 0}, {"min", 3, 60, 0}, {"h", 3, 3600, 0},
    {"day", 3, 86400, 0}, {"week", 3, 604800, 0}, {"yr", 3, 31536000, 0}, {"s-yr", 3, 31556925.9747, 0}, {"t-yr", 3, 31558149.76, 0},
    {"\xC2\xB0" "C", 4, 1, 273.15}, {"K", 4, 1, 0}, {"\xC2\xB0" "F", 4, 5.0 / 9.0, 459.67}, {"\xC2\xB0" "R", 4, 5.0 / 9.0, 0},
    {"m/s", 5, 1, 0}, {"km/h", 5, 1 / 3.6, 0}, {"knot", 5, 1852.0 / 3600.0, 0}, {"ft/s", 5, 0.3048, 0}, {"mile/h", 5, 0.44704, 0},
    {"u", 6, 1.66053906660e-27, 0}, {"mg", 6, 1e-6, 0}, {"g", 6, 1e-3, 0}, {"kg", 6, 1, 0}, {"mton", 6, 1000, 0},
    {"oz", 6, 0.028349523125, 0}, {"lb", 6, 0.45359237, 0}, {"slug", 6, 14.593902937206, 0}, {"ton(short)", 6, 907.18474, 0},
    {"ton(long)", 6, 1016.0469088, 0},
    {"N", 7, 1, 0}, {"lbf", 7, 4.4482216152605, 0}, {"tonf", 7, 9964.01641818352, 0}, {"dyne", 7, 1e-5, 0}, {"kgf", 7, 9.80665, 0},
    {"Pa", 8, 1, 0}, {"kPa", 8, 1e3, 0}, {"mmH2O", 8, 9.80665, 0}, {"mmHg", 8, 101325.0 / 760.0, 0}, {"atm", 8, 101325, 0},
    {"inH2O", 8, 249.08891, 0}, {"inHg", 8, 3386.388640341, 0}, {"lbf/in\xC2\xB2", 8, 6894.757293168, 0}, {"bar", 8, 1e5, 0},
    {"kgf/cm\xC2\xB2", 8, 98066.5, 0},
    {"eV", 9, 1.602176634e-19, 0}, {"J", 9, 1, 0}, {"calth", 9, 4.184, 0}, {"cal15", 9, 4.1855, 0}, {"calIT", 9, 4.1868, 0},
    {"kcalth", 9, 4184, 0}, {"kcal15", 9, 4185.5, 0}, {"kcalIT", 9, 4186.8, 0}, {"l-atm", 9, 101.325, 0},
    {"kW\xE2\x80\xA2h", 9, 3.6e6, 0}, {"ft\xE2\x80\xA2lbf", 9, 1.3558179483314, 0}, {"Btu", 9, 1055.05585262, 0}, {"erg", 9, 1e-7, 0},
    {"kgf\xE2\x80\xA2m", 9, 9.80665, 0},
    {"W", 10, 1, 0}, {"calth/s", 10, 4.184, 0}, {"hp", 10, 745.69987158227, 0}, {"ft\xE2\x80\xA2lbf/s", 10, 1.3558179483314, 0},
    {"Btu/min", 10, 17.584264210, 0},
};

// ---------------------------------------------------------------- distribuzioni
// Inversa monotona per bisezione: il piu' piccolo x con cdf(x) >= target
// (cdf crescente) su [lo, hi], allargando hi finche' serve.
template <typename F> double bisectIncreasing(F cdf, double target, double lo, double hi) {
    for (int k = 0; k < 200 && cdf(hi) < target; ++k) { lo = hi; hi = hi * 2 + 1; }
    for (int k = 0; k < 200; ++k) {
        const double mid = 0.5 * (lo + hi);
        if (cdf(mid) < target) lo = mid; else hi = mid;
        if (hi - lo <= 1e-13 * std::max(1.0, std::abs(mid))) break;
    }
    return 0.5 * (lo + hi);
}
// log C(n, k). v1.13.1 (E16): con n grande la differenza dei tre lgamma (circa
// n·ln n) perdeva ~1E-5 (HypergeoPD oltre 1); con min(k, n-k) <= 64 il prodotto
// si calcola direttamente, termine per termine.
double logChoose(double n, double k) {
    const double j = std::min(k, n - k);
    if (j >= 0 && j <= 64) {
        double s = 0;
        for (double i = 1; i <= j; ++i) s += std::log((n - j + i) / i);
        return s;
    }
    return std::lgamma(n + 1) - std::lgamma(k + 1) - std::lgamma(n - k + 1);
}
// Termini sommati al massimo da una funzione di ripartizione discreta (come il
// limite n <= 1E6 di BinomialCD e x <= 1E6 di PoissonCD): il calcolo resta
// limitato anche senza AC (v1.13.1, E02).
constexpr double kMaxDiscreteTerms = 1e6;
// Distribuzione discreta: il piu' piccolo intero x in [lo, hi) con CD(x) >= prob
// (hi se nessuno). pd e' nulla sotto lo; CD(x) e' la somma di pd(lo..x) limitata
// a 1, come in stats::binomialCd/poissonCd e hypergeoCD: la si accumula una sola
// volta (v1.13.1, E03: ricalcolarla per ogni x costava O(x²), minuti sulla
// PicoCalc), con le stesse somme nello stesso ordine, quindi lo stesso risultato.
template <typename F> double discreteInverse(F pd, double prob, double lo, double hi) {
    need(prob >= 0 && prob <= 1, "0<=prob<=1");
    // Tolleranza della calcolatrice sugli arrotondamenti delle somme.
    const double target = prob - 1e-10;
    if (target <= 0) return 0;                               // CD(0) >= 0: come la ricerca da 0
    double sum = 0;
    for (double x = lo; x < hi; ++x) {
        sum += pd(x);
        if (std::min(1.0, sum) >= target) return x;
    }
    return hi;
}

// ---------------------------------------------------------------- finanza
// Tasso per periodo di pagamento dal tasso annuo I% e dai periodi P/Y, C/Y.
double periodRate(double rate, double py, double cy) {
    need(py > 0 && cy > 0, "P/Y>0, C/Y>0");
    if (py == cy) return rate / (100.0 * cy);
    return std::pow(1 + rate / (100.0 * cy), cy / py) - 1;
}
// Pagamento a fine periodo: PV + α·PMT + β·FV = 0.
void tvmFactors(double n, double i, double& alpha, double& beta) {
    if (i == 0) { alpha = n; beta = 1; return; }
    beta = std::pow(1 + i, -n);
    alpha = (1 - beta) / i;
}
double rateFromPeriod(double i, double py, double cy) {
    if (py == cy) return i * 100.0 * cy;
    return (std::pow(1 + i, py / cy) - 1) * cy * 100.0;
}
bool leap(long y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
long dayNumber(double m, double d, double y) {
    need(integral(m) && integral(d) && integral(y) && m >= 1 && m <= 12 && d >= 1 && y >= 1902 && y <= 2097, "data 1/1/1902-31/12/2097");
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const long yy = static_cast<long>(y), mm = static_cast<long>(m), dd = static_cast<long>(d);
    need(dd <= days[mm - 1] + (mm == 2 && leap(yy) ? 1 : 0), "giorno non valido");
    long n = dd;
    for (long k = 1; k < mm; ++k) n += days[k - 1] + (k == 2 && leap(yy) ? 1 : 0);
    for (long k = 1902; k < yy; ++k) n += leap(k) ? 366 : 365;
    return n;
}
} // namespace

const char* const* unitCategories() noexcept { return kCategories; }
std::vector<const UnitInfo*> unitsOf(std::size_t category) {
    std::vector<const UnitInfo*> out;
    for (const auto& u : kUnits) if (u.category == category) out.push_back(&u);
    return out;
}
const UnitInfo* findUnit(const std::string& name) noexcept {
    for (const auto& u : kUnits) if (name == u.name) return &u;
    return nullptr;
}
double convertUnit(double value, const UnitInfo& from, const UnitInfo& to) {
    if (from.category != to.category) throw MathError("Argument ERROR: [" + std::string(from.name) + "] e [" + to.name + "] misurano grandezze diverse");
    return (value + from.offset) * from.factor / to.factor - to.offset;
}

// ---------------------------------------------------------------- distribuzioni
double normPD(double x, double sigma, double mu) { return stats::normalPd(x, sigma, mu); }
double normCD(double lower, double upper, double sigma, double mu) { return stats::normalCd(lower, upper, sigma, mu); }
// Quantile della normale standard raffinato con Newton sulla funzione di
// ripartizione (precisione di ~1e-15 invece di ~1e-9 dell'approssimazione razionale).
double standardQuantile(double p) {
    double z = inverseNormalCdf(p, 0.0, 1.0);
    for (int k = 0; k < 3; ++k) {
        const double pdf = std::exp(-0.5 * z * z) / std::sqrt(2 * kPi);
        if (pdf <= 0) break;
        z -= (normalCdf(z) - p) / pdf;
    }
    return z;
}
double invNormCD(int tail, double area, double sigma, double mu) {
    need(area > 0 && area < 1 && sigma > 0, "0<Area<1, sigma>0");
    return mu + sigma * standardQuantile(tail > 0 ? 1 - area : area);
}
void invNormCentral(double area, double sigma, double mu, double& lower, double& upper) {
    need(area > 0 && area < 1 && sigma > 0, "0<Area<1, sigma>0");
    upper = mu + sigma * standardQuantile(0.5 + area / 2);
    lower = 2 * mu - upper;
}
double tPD(double x, double df) {
    need(df > 0, "df>0");
    return std::exp(std::lgamma((df + 1) / 2) - std::lgamma(df / 2)) / std::sqrt(df * kPi) * std::pow(1 + x * x / df, -(df + 1) / 2);
}
double tCD(double lower, double upper, double df) { return stats::tCd(lower, upper, df); }
double invTCD(double area, double df) {
    need(area > 0 && area < 1 && df > 0, "0<Area<1, df>0");
    // P(X >= x) = area: cdf(x) = 1 - area. v1.13.1 (E08): la distribuzione e'
    // simmetrica (x(1 - area) = -x(area), x(0.5) = 0); si cerca la coda superiore
    // da 0, allargando l'intervallo (prima [-1E4, 1E4] tagliava la coda inferiore).
    if (area == 0.5) return 0;
    if (area > 0.5) return -invTCD(1 - area, df);
    auto cdf = [&](double v) { return stats::tCd(-std::numeric_limits<double>::infinity(), v, df); };
    return bisectIncreasing(cdf, 1 - area, 0, 1e4);
}
double chiPD(double x, double df) {
    need(df > 0 && integral(df), "df intero >0");
    if (x <= 0) return 0;
    return std::exp((df / 2 - 1) * std::log(x) - x / 2 - (df / 2) * std::log(2.0) - std::lgamma(df / 2));
}
double chiCD(double lower, double upper, double df) { need(integral(df), "df intero >0"); return stats::chiSquareCd(lower, upper, df); }
double invChiCD(double area, double df) {
    need(area > 0 && area < 1 && df > 0 && integral(df), "0<Area<1, df intero >0");
    auto cdf = [&](double v) { return stats::chiSquareCd(0, v, df); };
    return bisectIncreasing(cdf, 1 - area, 0, df + 10);
}
double fPD(double x, double n, double d) {
    need(n > 0 && d > 0 && integral(n) && integral(d), "gradi di liberta' interi >0");
    if (x <= 0) return 0;
    const double lg = std::lgamma((n + d) / 2) - std::lgamma(n / 2) - std::lgamma(d / 2);
    return std::exp(lg + (n / 2) * std::log(n / d) + (n / 2 - 1) * std::log(x) - ((n + d) / 2) * std::log1p(n * x / d));
}
double fCD(double lower, double upper, double n, double d) { need(integral(n) && integral(d), "gradi di liberta' interi >0"); return stats::fCd(lower, upper, n, d); }
double invFCD(double area, double n, double d) {
    need(area > 0 && area < 1 && n > 0 && d > 0 && integral(n) && integral(d), "0<Area<1, gradi di liberta' interi >0");
    auto cdf = [&](double v) { return stats::fCd(0, v, n, d); };
    return bisectIncreasing(cdf, 1 - area, 0, 10);
}
double binomialPD(double x, double n, double p) { return stats::binomialPd(x, n, p); }
double binomialCD(double x, double n, double p) { return x < 0 ? 0 : stats::binomialCd(x, n, p); }
double invBinomialCD(double prob, double n, double p) {
    need(integral(n) && n >= 0 && p >= 0 && p <= 1, "n intero >=0, 0<=p<=1");
    need(n <= 1e6, "n intero 0..1e6");                       // limite di BinomialCD
    return discreteInverse([&](double x) { return stats::binomialPd(x, n, p); }, prob, 0, n);
}
double poissonPD(double x, double lambda) { return stats::poissonPd(x, lambda); }
double poissonCD(double x, double lambda) { return x < 0 ? 0 : stats::poissonCd(x, lambda); }
double invPoissonCD(double prob, double lambda) {
    need(lambda > 0 && std::isfinite(lambda), "lambda>0");
    // Si cerca in [0, 1E6] (il limite di PoissonCD). v1.13.1 (E15): senza un x
    // valido si restituiva 1000000 (risultato errato per lambda grandi).
    const double x = discreteInverse([&](double v) { return stats::poissonPd(v, lambda); }, prob, 0, kMaxDiscreteTerms + 1);
    if (x > kMaxDiscreteTerms) argumentError("InvPoissonCD oltre 1E6 (lambda troppo grande)");
    return x;
}
// v1.13.1 (E14): (1-p)^k con il logaritmo (log1p): con p < 1E-16 1-p vale 1 in
// double e GeoCD era sempre 0 (InvGeoCD non terminava piu').
double geoPD(double x, double p) {
    need(p > 0 && p <= 1, "0<p<=1");
    if (x < 1 || !integral(x)) return 0;
    if (p == 1) return x == 1 ? 1 : 0;
    return p * std::exp((x - 1) * std::log1p(-p));
}
double geoCD(double x, double p) {
    need(p > 0 && p <= 1, "0<p<=1");
    if (x < 1) return 0;
    if (p == 1) return 1;
    return -std::expm1(std::floor(x) * std::log1p(-p));
}
double invGeoCD(double prob, double p) {
    need(p > 0 && p <= 1 && prob >= 0 && prob <= 1, "0<=prob<=1, 0<p<=1");
    if (prob <= 0) return 1;
    if (p == 1 || prob >= 1) return p == 1 ? 1 : std::numeric_limits<double>::infinity();
    // Il piu' piccolo x >= 1 con GeoCD(x) >= prob - 1E-10, dalla formula chiusa;
    // pochi passi correggono gli arrotondamenti (oltre 2^53 x non cambia piu').
    const double target = prob - 1e-10;
    if (target <= 0) return 1;
    double x = std::ceil(std::log1p(-target) / std::log1p(-p));
    if (!(x >= 1)) x = 1;
    for (int k = 0; k < 64 && x > 1 && geoCD(x - 1, p) >= target; ++k) --x;
    for (int k = 0; k < 64 && geoCD(x, p) < target; ++k) ++x;
    return x;
}
double hypergeoPD(double x, double n, double m, double N) {
    need(integral(n) && integral(m) && integral(N) && n >= 1 && m >= 0 && N >= 1 && n <= N && m <= N, "n, M, N interi con n<=N, M<=N");
    if (!integral(x) || x < std::max(0.0, n + m - N) || x > std::min(n, m)) return 0;
    return std::exp(logChoose(m, x) + logChoose(N - m, n - x) - logChoose(N, n));
}
double hypergeoCD(double x, double n, double m, double N) {
    (void)hypergeoPD(0, n, m, N);                            // Argument ERROR anche quando la somma e' vuota
    // v1.13.1 (E02): si sommano solo i termini non nulli, da max(0, n+M-N) a
    // min(x, n, M), al piu' 1E6 (la somma andava da 0 a x: con x = 1E8 o n = M = 1E9
    // minuti o ore, senza AC).
    const double lo = std::max(0.0, n + m - N), top = std::min(std::floor(x), std::min(n, m));
    need(top - lo < kMaxDiscreteTerms, "HypergeoCD con al piu' 1E6 valori possibili");
    double s = 0;
    for (double k = lo; k <= top; ++k) s += hypergeoPD(k, n, m, N);
    return std::min(1.0, s);
}
double invHypergeoCD(double prob, double n, double m, double N) {
    (void)hypergeoPD(0, n, m, N);
    const double lo = std::max(0.0, n + m - N), hi = std::min(n, m);
    need(hi - lo < kMaxDiscreteTerms, "InvHypergeoCD con al piu' 1E6 valori possibili");
    return discreteInverse([&](double x) { return hypergeoPD(x, n, m, N); }, prob, lo, hi);
}
double probP(double t) { return normalCdf(t); }
double probQ(double t) { return normalCdf(std::abs(t)) - 0.5; }
double probR(double t) { return 1 - normalCdf(t); }

// ---------------------------------------------------------------- finanza
double smplSI(double n, double rate, double pv) { return -(n / 365.0) * pv * (rate / 100.0); }
double smplSFV(double n, double rate, double pv) { return -(pv + (n / 365.0) * pv * (rate / 100.0)); }
double cmpdPV(double n, double rate, double pmt, double fv, double py, double cy) {
    double a = 0, b = 0;
    tvmFactors(n, periodRate(rate, py, cy), a, b);
    return -(a * pmt + b * fv);
}
double cmpdFV(double n, double rate, double pv, double pmt, double py, double cy) {
    double a = 0, b = 0;
    tvmFactors(n, periodRate(rate, py, cy), a, b);
    return -(pmt * a + pv) / b;
}
double cmpdPMT(double n, double rate, double pv, double fv, double py, double cy) {
    double a = 0, b = 0;
    tvmFactors(n, periodRate(rate, py, cy), a, b);
    if (a == 0) throw MathError("Math ERROR: n=0");
    return -(pv + fv * b) / a;
}
double cmpdN(double rate, double pv, double pmt, double fv, double py, double cy) {
    const double i = periodRate(rate, py, cy);
    if (i == 0) { if (pmt == 0) throw MathError("Math ERROR: PMT=0 con I%=0"); return -(pv + fv) / pmt; }
    const double num = pmt - fv * i, den = pmt + pv * i;
    if (num / den <= 0) throw MathError("Math ERROR: nessun numero di periodi");
    return std::log(num / den) / std::log1p(i);
}
double cmpdI(double n, double pv, double pmt, double fv, double py, double cy) {
    need(n > 0, "n>0");
    auto f = [&](double i) { double a = 0, b = 0; tvmFactors(n, i, a, b); return pv + a * pmt + b * fv; };
    // Newton con derivata numerica, partenza 1% per periodo; poi bisezione di riserva.
    double i = 0.01;
    for (int k = 0; k < 100; ++k) {
        const double y = f(i), h = std::max(1e-7, std::abs(i) * 1e-6);
        const double d = (f(i + h) - f(i - h)) / (2 * h);
        if (!std::isfinite(d) || d == 0) break;
        const double next = i - y / d;
        if (!std::isfinite(next) || next <= -1) break;
        if (std::abs(next - i) < 1e-14) { i = next; return rateFromPeriod(i, py, cy); }
        i = next;
    }
    if (std::abs(f(i)) < 1e-8 * std::max(1.0, std::abs(pv) + std::abs(fv) + std::abs(pmt))) return rateFromPeriod(i, py, cy);
    throw MathError("Iteration ERROR: I% non trovato");
}
double cashNPV(double rate, const std::vector<double>& cash) {
    need(!cash.empty(), "lista dei flussi vuota");
    const double i = rate / 100.0;
    double s = 0;
    for (std::size_t k = 0; k < cash.size(); ++k) s += cash[k] / std::pow(1 + i, static_cast<double>(k));
    return s;
}
double cashNFV(double rate, const std::vector<double>& cash) {
    return cashNPV(rate, cash) * std::pow(1 + rate / 100.0, static_cast<double>(cash.size() - 1));
}
double cashIRR(const std::vector<double>& cash) {
    need(cash.size() >= 2, "almeno due flussi");
    auto f = [&](double i) { double s = 0; for (std::size_t k = 0; k < cash.size(); ++k) s += cash[k] / std::pow(1 + i, static_cast<double>(k)); return s; };
    double i = 0.1;
    for (int k = 0; k < 200; ++k) {
        const double y = f(i), h = 1e-7;
        const double d = (f(i + h) - f(i - h)) / (2 * h);
        if (!std::isfinite(d) || d == 0) break;
        const double next = i - y / d;
        if (!std::isfinite(next) || next <= -1) break;
        if (std::abs(next - i) < 1e-14) return next * 100;
        i = next;
    }
    if (std::abs(f(i)) < 1e-8) return i * 100;
    throw MathError("Iteration ERROR: IRR non trovato");
}
double cashPBP(double rate, const std::vector<double>& cash) {
    need(!cash.empty(), "lista dei flussi vuota");
    if (cash[0] >= 0) return 0;
    const double i = rate / 100.0;
    double pv = cash[0];
    for (std::size_t k = 1; k < cash.size(); ++k) {
        const double next = pv + cash[k] / std::pow(1 + i, static_cast<double>(k));
        if (pv <= 0 && next > 0) return static_cast<double>(k - 1) - pv / (next - pv);
        pv = next;
    }
    throw MathError("Math ERROR: il periodo di recupero supera i flussi");
}
double amortization(int kind, double pm1, double pm2, double rate, double pv, double pmt, double py, double cy) {
    need(integral(pm1) && integral(pm2) && pm1 >= 1 && pm2 >= pm1 && pm2 <= 9999, "1<=PM1<=PM2<=9999");
    const double i = periodRate(rate, py, cy);
    double bal = pv, sumInt = 0, sumPrn = 0, int1 = 0, prn1 = 0;
    for (int k = 1; k <= static_cast<int>(pm2); ++k) {
        const double in = -bal * i;            // interesse della rata k (segno della calcolatrice)
        const double pr = pmt - in;            // capitale della rata k
        bal = bal * (1 + i) + pmt;
        if (k == static_cast<int>(pm1)) { int1 = in; prn1 = pr; }
        if (k >= static_cast<int>(pm1)) { sumInt += in; sumPrn += pr; }
    }
    switch (kind) {
    case 0: return bal;
    case 1: return int1;
    case 2: return prn1;
    case 3: return sumInt;
    default: return sumPrn;
    }
}
double cnvtEFF(double n, double apr) { need(n > 0, "n>0"); return (std::pow(1 + apr / (100.0 * n), n) - 1) * 100.0; }
double cnvtAPR(double n, double eff) { need(n > 0, "n>0"); return n * (std::pow(1 + eff / 100.0, 1.0 / n) - 1) * 100.0; }
double cost(double sellPrice, double marginPct) { need(marginPct < 100, "Margin<100"); return sellPrice * (1 - marginPct / 100.0); }
double sell(double costPrice, double marginPct) { need(marginPct < 100, "Margin<100"); return costPrice / (1 - marginPct / 100.0); }
double margin(double costPrice, double sellPrice) { need(sellPrice != 0, "Sell<>0"); return (1 - costPrice / sellPrice) * 100.0; }
double daysPrd(double m1, double d1, double y1, double m2, double d2, double y2) {
    return static_cast<double>(dayNumber(m2, d2, y2) - dayNumber(m1, d1, y1));
}
#if FXAI_BOARD_PICO_PLUS_2W
// v1.17.0: BGN = END con il pagamento capitalizzato di un periodo, PMT·(1+i).
double cmpdSolve(int unknown, double n, double rate, double pv, double pmt, double fv, double py, double cy, bool begin) {
    if (unknown == 1) {
        if (!begin) return cmpdI(n, pv, pmt, fv, py, cy);
        need(n > 0, "n>0");
        auto f = [&](double i) { double a = 0, b = 0; tvmFactors(n, i, a, b); return pv + a * (1 + i) * pmt + b * fv; };
        double i = 0.01;
        for (int k = 0; k < 100; ++k) {
            const double y = f(i), h = std::max(1e-7, std::abs(i) * 1e-6);
            const double d = (f(i + h) - f(i - h)) / (2 * h);
            if (!std::isfinite(d) || d == 0) break;
            const double next = i - y / d;
            if (!std::isfinite(next) || next <= -1) break;
            if (std::abs(next - i) < 1e-14) return rateFromPeriod(next, py, cy);
            i = next;
        }
        if (std::abs(f(i)) < 1e-8 * std::max(1.0, std::abs(pv) + std::abs(fv) + std::abs(pmt))) return rateFromPeriod(i, py, cy);
        throw MathError("Iteration ERROR: I% non trovato");
    }
    const double g = begin ? 1 + periodRate(rate, py, cy) : 1;
    switch (unknown) {
    case 0: return cmpdN(rate, pv, pmt * g, fv, py, cy);
    case 2: return cmpdPV(n, rate, pmt * g, fv, py, cy);
    case 3: return cmpdPMT(n, rate, pv, fv, py, cy) / g;
    case 4: return cmpdFV(n, rate, pv, pmt * g, py, cy);
    default: throw MathError("Argument ERROR: variabile TVM");
    }
}
double amortizationAt(int kind, double pm1, double pm2, double rate, double pv, double pmt, double py, double cy, bool begin) {
    if (!begin) return amortization(kind, pm1, pm2, rate, pv, pmt, py, cy);
    need(integral(pm1) && integral(pm2) && pm1 >= 1 && pm2 >= pm1 && pm2 <= 9999, "1<=PM1<=PM2<=9999");
    const double i = periodRate(rate, py, cy);
    // Pagamento all'inizio: la prima rata non ha interessi (tutta capitale).
    double bal = pv, sumInt = 0, sumPrn = 0, int1 = 0, prn1 = 0;
    for (int k = 1; k <= static_cast<int>(pm2); ++k) {
        const double in = k == 1 ? 0.0 : -bal * i;
        const double pr = pmt - in;
        bal = k == 1 ? bal + pmt : bal * (1 + i) + pmt;
        if (k == static_cast<int>(pm1)) { int1 = in; prn1 = pr; }
        if (k >= static_cast<int>(pm1)) { sumInt += in; sumPrn += pr; }
    }
    switch (kind) {
    case 0: return bal;
    case 1: return int1;
    case 2: return prn1;
    case 3: return sumInt;
    default: return sumPrn;
    }
}
#endif

// ---------------------------------------------------------------- nomi
namespace {
struct Name { const char* canonical; const char* shown; };
const Name kNames[] = {
    {"solve", "Solve"}, {"solve_n", "SolveN"}, {"f_min", "FMin"}, {"f_max", "FMax"}, {"derivative", "d/dx"}, {"derivative2", "d\xC2\xB2/dx\xC2\xB2"},
    {"integral", "\xE2\x88\xAB"}, {"sum", "\xCE\xA3"}, {"prod", "\xCE\xA0"},
    {"ran", "Ran# "}, {"rand", "Ran# "}, {"ran_list", "RanList#"}, {"ran_samp", "RanSamp#"},
    {"prob_p", "P"}, {"prob_q", "Q"}, {"prob_r", "R"}, {"prob_t", "t"}, {"num_rnd", "Rnd "},
    {"norm_pd", "NormPD"}, {"norm_cd", "NormCD"}, {"invnorm_cd", "InvNormCD"}, {"t_pd", "tPD"}, {"t_cd", "tCD"}, {"invt_cd", "InvTCD"},
    {"chi_pd", "ChiPD"}, {"chi_cd", "ChiCD"}, {"invchi_cd", "InvChiCD"}, {"f_pd", "FPD"}, {"f_cd", "FCD"}, {"invf_cd", "InvFCD"},
    {"binomial_pd", "BinomialPD"}, {"binomial_cd", "BinomialCD"}, {"invbinomial_cd", "InvBinomialCD"},
    {"poisson_pd", "PoissonPD"}, {"poisson_cd", "PoissonCD"}, {"invpoisson_cd", "InvPoissonCD"},
    {"geo_pd", "GeoPD"}, {"geo_cd", "GeoCD"}, {"invgeo_cd", "InvGeoCD"},
    {"hypergeo_pd", "HypergeoPD"}, {"hypergeo_cd", "HypergeoCD"}, {"invhypergeo_cd", "InvHypergeoCD"},
    {"x_hat", "\xEE\x80\x80"}, {"y_hat", "\xC5\xB7"}, {"s_dev", "S-Dev"}, {"stat_var", "Var"},
    {"smpl_si", "Smpl_SI"}, {"smpl_sfv", "Smpl_SFV"}, {"cmpd_pv", "Cmpd_PV"}, {"cmpd_fv", "Cmpd_FV"}, {"cmpd_pmt", "Cmpd_PMT"},
    {"cmpd_n", "Cmpd_n"}, {"cmpd_i", "Cmpd_I%"}, {"cash_npv", "Cash_NPV"}, {"cash_irr", "Cash_IRR"}, {"cash_pbp", "Cash_PBP"},
    {"cash_nfv", "Cash_NFV"}, {"amt_bal", "Amt_BAL"}, {"amt_int", "Amt_INT"}, {"amt_prn", "Amt_PRN"}, {"amt_sint", "Amt_\xCE\xA3INT"},
    {"amt_sprn", "Amt_\xCE\xA3PRN"}, {"cnvt_eff", "Cnvt_EFF"}, {"cnvt_apr", "Cnvt_APR"}, {"fin_cost", "Cost"}, {"fin_sell", "Sell"},
    {"fin_margin", "Margin"}, {"days_prd", "Days_Prd"},
    {"pxl_test", "PxlTest"}, {"r_var", "r"}, {"theta", "\xCE\xB8"},
    // v1.13.1 (E17): VARS › V-WIN e TABLE con i nomi della fx-CG50 (non "xmin", "tblstart").
    {"xmin", "Xmin"}, {"xmax", "Xmax"}, {"xscl", "Xscl"}, {"ymin", "Ymin"}, {"ymax", "Ymax"}, {"yscl", "Yscl"},
    {"tmin", "Tmin"}, {"tmax", "Tmax"}, {"tptch", "Tptch"}, {"tblstart", "TblStart"}, {"tblend", "TblEnd"}, {"tblpitch", "TblPitch"},
    {"xfct", "Xfct"}, {"yfct", "Yfct"}, {"d_start", "D Start"}, {"d_end", "D End"}, {"d_pitch", "D pitch"},
    {"r_start", "R Start"}, {"r_end", "R End"}, {"rec_a0", "a0"}, {"rec_a1", "a1"}, {"rec_a2", "a2"}, {"rec_b0", "b0"}, {"rec_b1", "b1"},
    {"rec_b2", "b2"}, {"rec_c0", "c0"}, {"rec_c1", "c1"}, {"rec_c2", "c2"}, {"rec_anstart", "anStart"}, {"rec_bnstart", "bnStart"},
    {"rec_cnstart", "cnStart"}, {"rec_an1", "an+1"}, {"tbl_result", "Result"}, {"rec_an", "an"}, {"rec_n", "n"},
    {"tvm_n", "n"}, {"tvm_i", "I%"}, {"tvm_pv", "PV"}, {"tvm_pmt", "PMT"}, {"tvm_fv", "FV"}, {"tvm_py", "P/Y"}, {"tvm_cy", "C/Y"},
    {"stat_n", "n"}, {"stat_meanx", "\xEE\x80\x81"}, {"stat_sumx", "\xCE\xA3x"}, {"stat_sumx2", "\xCE\xA3x\xC2\xB2"}, {"stat_popsdx", "\xCF\x83x"},
    {"stat_sdx", "sx"}, {"stat_minx", "minX"}, {"stat_maxx", "maxX"}, {"stat_meany", "\xEE\x80\x82"}, {"stat_sumy", "\xCE\xA3y"},
    {"stat_sumy2", "\xCE\xA3y\xC2\xB2"}, {"stat_sumxy", "\xCE\xA3xy"}, {"stat_popsdy", "\xCF\x83y"}, {"stat_sdy", "sy"}, {"stat_miny", "minY"},
    {"stat_maxy", "maxY"}, {"reg_a", "a"}, {"reg_b", "b"}, {"reg_c", "c"}, {"reg_d", "d"}, {"reg_e", "e"}, {"reg_r", "r"},
    {"reg_r2", "r\xC2\xB2"}, {"reg_mse", "MSe"}, {"stat_q1", "Q1"}, {"stat_q3", "Q3"}, {"stat_med", "Med"}, {"stat_mod", "Mod"},
    {"stat_hstart", "H-Start"}, {"stat_hpitch", "H-pitch"},
};
const CatalogEntry kCatalog[] = {
    {"Abs", "abs("}, {"Amt_BAL(", "amt_bal("}, {"Amt_INT(", "amt_int("}, {"Amt_PRN(", "amt_prn("}, {"Amt_\xCE\xA3INT(", "amt_sint("},
    {"Amt_\xCE\xA3PRN(", "amt_sprn("}, {"And", "and"}, {"Angle(", "Angle("}, {"Arg", "arg("}, {"Augment(", "Augment("},
    {"BinomialCD(", "binomial_cd("}, {"BinomialPD(", "binomial_pd("}, {"Cash_IRR(", "cash_irr("}, {"Cash_NFV(", "cash_nfv("},
    {"Cash_NPV(", "cash_npv("}, {"Cash_PBP(", "cash_pbp("}, {"ChiCD(", "chi_cd("}, {"ChiPD(", "chi_pd("}, {"Cls", "Cls"},
    {"Cmpd_FV(", "cmpd_fv("}, {"Cmpd_I%(", "cmpd_i("}, {"Cmpd_n(", "cmpd_n("}, {"Cmpd_PMT(", "cmpd_pmt("}, {"Cmpd_PV(", "cmpd_pv("},
    {"Cnvt_APR(", "cnvt_apr("}, {"Cnvt_EFF(", "cnvt_eff("}, {"Conjg", "conj("}, {"Cost(", "fin_cost("}, {"CrossP(", "CrossP("},
    {"Cuml", "Cuml("}, {"Days_Prd(", "days_prd("}, {"Det", "Det("}, {"Dim", "Dim("}, {"DotP(", "DotP("}, {"DrawDyna", "DrawDyna"},
    {"DrawFTG-Con", "DrawFTG-Con"}, {"DrawFTG-Plt", "DrawFTG-Plt"}, {"DrawGraph", "DrawGraph"}, {"DrawR-Con", "DrawR-Con"},
    {"DrawR-Plt", "DrawR-Plt"}, {"DrawStat", "DrawStat"}, {"d/dx(", "derivative("}, {"d\xC2\xB2/dx\xC2\xB2(", "derivative2("},
    {"F-Line", "F-Line "}, {"FCD(", "f_cd("}, {"Fill(", "Fill("}, {"FMax(", "f_max("}, {"FMin(", "f_min("}, {"FPD(", "f_pd("},
    {"Frac", "frac("}, {"G-SelOff", "G-SelOff "}, {"G-SelOn", "G-SelOn "}, {"G\xE2\x88\xAB" "dx", "G\xE2\x88\xAB" "dx "},
    {"GCD(", "gcd("}, {"GeoCD(", "geo_cd("}, {"GeoPD(", "geo_pd("}, {"Graph X=", "Graph X="}, {"Graph Y=", "Graph Y="},
    {"Graph Y>", "Graph Y>"}, {"Graph Y<", "Graph Y<"}, {"Graph Y\xE2\x89\xA5", "Graph Y\xE2\x89\xA5"}, {"Graph Y\xE2\x89\xA4", "Graph Y\xE2\x89\xA4"},
    {"Graph X>", "Graph X>"}, {"Graph X<", "Graph X<"}, {"Graph X\xE2\x89\xA5", "Graph X\xE2\x89\xA5"}, {"Graph X\xE2\x89\xA4", "Graph X\xE2\x89\xA4"},
    {"Graph r=", "Graph r="}, {"Graph(X,Y)=(", "Graph(X,Y)=("}, {"Horizontal", "Horizontal "}, {"HypergeoCD(", "hypergeo_cd("},
    {"HypergeoPD(", "hypergeo_pd("}, {"Identity", "Identity("}, {"Int", "int("}, {"Int\xC3\xB7", "Int\xC3\xB7"}, {"Intg", "intg("},
    {"InvBinomialCD(", "invbinomial_cd("}, {"InvChiCD(", "invchi_cd("}, {"InvFCD(", "invf_cd("}, {"InvGeoCD(", "invgeo_cd("},
    {"InvHypergeoCD(", "invhypergeo_cd("}, {"InvNormCD(", "invnorm_cd("}, {"InvPoissonCD(", "invpoisson_cd("}, {"InvTCD(", "invt_cd("},
    {"Inverse", "Inverse "}, {"LCM(", "lcm("}, {"Line", "Line"}, {"List", "List "}, {"List\xE2\x86\x92Mat(", "listtomat("},
    {"Margin(", "fin_margin("}, {"Mat", "Mat "}, {"Mat\xE2\x86\x92List(", "mattolist("}, {"Max(", "Max("}, {"Mean(", "Mean("},
    {"Median(", "Median("}, {"Min(", "Min("}, {"MOD(", "mod("}, {"MOD_Exp(", "modexp("}, {"Neg", "neg("}, {"Norm(", "Norm("},
    {"NormCD(", "norm_cd("}, {"NormPD(", "norm_pd("}, {"Normal", "Normal "}, {"Not", "not"}, {"Or", "or"}, {"P(", "prob_p("},
    {"Percent", "Percent("}, {"Plot", "Plot "}, {"PlotChg", "PlotChg "}, {"PlotOff", "PlotOff "}, {"PlotOn", "PlotOn "},
    {"PoissonCD(", "poisson_cd("}, {"PoissonPD(", "poisson_pd("}, {"Pol(", "pol("}, {"Prod", "Prod("}, {"PxlChg", "PxlChg "},
    {"PxlOff", "PxlOff "}, {"PxlOn", "PxlOn "}, {"PxlTest(", "pxl_test("}, {"Q(", "prob_q("}, {"R(", "prob_r("}, {"Ran#", "ran"},
    {"RanBin#(", "ranbin("}, {"RanInt#(", "randint("}, {"RanList#(", "ran_list("}, {"RanNorm#(", "rannorm("}, {"RanSamp#(", "ran_samp("},
    {"RclPict", "RclPict "}, {"Rec(", "rec("}, {"Ref", "Ref("}, {"ReP", "re("}, {"ImP", "im("}, {"Rmdr", "Rmdr"}, {"Rnd", "num_rnd("},
    {"RndFix(", "rndfix("}, {"Rref", "Rref("}, {"*Row", "*Row "}, {"*Row+", "*Row+ "}, {"Row+", "Row+ "}, {"S-Dev(", "s_dev("},
    {"Sell(", "fin_sell("}, {"Seq(", "Seq("}, {"SketchBroken", "SketchBroken "}, {"SketchDot", "SketchDot "},
    {"SketchNormal", "SketchNormal "}, {"SketchThick", "SketchThick "}, {"SketchThin", "SketchThin "}, {"Smpl_SFV(", "smpl_sfv("},
    {"Smpl_SI(", "smpl_si("}, {"Solve(", "solve("}, {"SolveN(", "solve_n("}, {"SortA(", "SortA("}, {"SortD(", "SortD("},
    {"StoPict", "StoPict "}, {"BG-Pict", "BG-Pict "}, {"Sum", "Sum("}, {"Swap", "Swap "}, {"t(", "prob_t("}, {"tCD(", "t_cd("},
    {"Tangent", "Tangent "}, {"Text", "Text "}, {"tPD(", "t_pd("}, {"Trn", "Trn("}, {"UnitV(", "UnitV("}, {"Var(", "stat_var("},
    {"Vct", "Vct "}, {"Vertical", "Vertical "}, {"ViewWindow", "ViewWindow "}, {"Xnor", "xnor"}, {"Xor", "xor"},
    {"\xEE\x80\x80(", "x_hat("}, {"\xC5\xB7(", "y_hat("}, {"\xCE\x94List", "dlist("}, {"\xCE\xA3(", "sum("}, {"\xE2\x88\xAB(", "integral("},
    {nullptr, nullptr},
};
} // namespace

const char* displayName(const std::string& canonical) noexcept {
    for (const auto& n : kNames) if (canonical == n.canonical) return n.shown;
    return nullptr;
}
const CatalogEntry* catalog() noexcept { return kCatalog; }
} // namespace fx::calclib
