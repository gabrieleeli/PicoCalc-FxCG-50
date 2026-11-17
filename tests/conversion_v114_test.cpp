// v1.14.0: CONVERSION completo.
//  - tabella unica (units.hpp): integrita', fattori esatti, prefissi SI e IEC;
//  - per ogni categoria: un valore di riferimento da fonte ufficiale (NIST SP 811,
//    BIPM SI 2019, CODATA 2018), il round trip A->B->A entro 1E-12 relativo,
//    l'errore per categorie incompatibili e, per le non lineari, i casi limite;
//  - Run-Matrix OPTN › CONVERT identico alla v1.13.1 (103 unita', ordine, risultati);
//  - conversioni di formato (unit_formats.hpp) con input non validi;
//  - risultati esatti; record v10 (preferenze di CONVERSION) e compatibilita' v9;
//  - docs/CONVERSION.md generato dalla tabella (FXAI_UPDATE_DOCS=1 lo riscrive).
#include "app_rig.hpp"
#include "calc_library.hpp"
#include "exact.hpp"
#include "expression.hpp"
#include "run_matrix.hpp"
#include "test_util.hpp"
#include "unit_formats.hpp"
#include "units.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef FXAI_SOURCE_ROOT
#error "FXAI_SOURCE_ROOT must point to the repository root"
#endif

using namespace fx;
namespace u = fx::units;

namespace {
// ---------------------------------------------------------------- v1.13.1 (calc_library.cpp, kUnits)
struct Legacy { const char* name; int cat; double factor; double offset; };
const Legacy kLegacy[] = {
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
const char* const kLegacyCategories[] = {"LENGTH", "AREA", "VOLUME", "TIME", "TMPR", "VELOCITY", "MASS", "FORCE", "PRESSURE", "ENERGY", "POWER"};

const std::string kArrow = "\xE2\x86\x92";
const std::string kConv = "\xE2\x96\xB6";
std::string calc(Workspace& w, const std::string& t) { runCalculation(w, t); return resultText(w.history.back(), w.settings); }
double num(Workspace& w, const std::string& t) { runCalculation(w, t); return w.history.back().value.real(); }
template <typename E> bool fails(Workspace& w, const std::string& t) {
    try { runCalculation(w, t); } catch (const E&) { return true; } catch (...) { return false; }
    return false;
}

std::size_t cat(const char* name) {
    for (std::size_t c = 0; c < u::categoryCount(); ++c) if (std::strcmp(u::category(c).name, name) == 0) return c;
    std::fprintf(stderr, "categoria sconosciuta %s\n", name);
    return 0;
}
u::Ref R(std::size_t c, const char* sym) {
    const u::Ref r = u::find(c, sym);
    if (!r) std::fprintf(stderr, "unita' sconosciuta [%s] in %s\n", sym, u::category(c).name);
    return r;
}
double conv(const char* category, double v, const char* from, const char* to, u::Params p = {}) {
    const std::size_t c = cat(category);
    return u::convert(v, R(c, from), R(c, to), p);
}
bool rel(double a, double b, double tol) { return a == b || std::fabs(a - b) <= tol * std::max(std::fabs(a), std::fabs(b)); }
#define CHECK_REL(a, b, tol) do { const double ra_ = (a), rb_ = (b); \
    ::testutil::report(rel(ra_, rb_, tol), #a " ~ " #b, __FILE__, __LINE__, "(" + std::to_string(ra_) + " vs " + std::to_string(rb_) + ")"); } while (0)
std::string errorTitle(const std::exception& e) { const char* w = e.what(); const char* c = std::strchr(w, ':'); return c ? std::string(w, c) : w; }
template <typename F> std::string errorOf(F f) {
    try { f(); } catch (const std::exception& e) { return errorTitle(e); }
    return "";
}
std::string shortest(double v) {
    char b[40];
    for (int d = 1; d <= 17; ++d) { std::snprintf(b, sizeof b, "%.*g", d, v); if (std::strtod(b, nullptr) == v) break; }
    return b;
}
std::string fmtc(std::size_t conv, std::size_t from, std::size_t to, const std::string& in, double param = 0) {
    unitfmt::Options o; o.param = param;
    return unitfmt::convert(conv, from, to, in, o);
}
std::size_t converterIndex(const char* name) {
    for (std::size_t i = 0; i < unitfmt::converterCount(); ++i) if (std::strcmp(unitfmt::converter(i).name, name) == 0) return i;
    std::fprintf(stderr, "convertitore sconosciuto %s\n", name);
    return 0;
}
double factorValue(const u::ExactFactor& f) { return f.r.toDouble() * std::pow(3.14159265358979323846, f.pi); }

// ---------------------------------------------------------------- docs/CONVERSION.md
std::string kindText(const u::Unit& x) {
    const std::string k = shortest(x.factor);
    switch (x.kind()) {
    case u::Kind::Linear: return x.has(u::flag::Param) ? "v × " + k + " / " + (u::category(x.cat).param == u::Param::Dpi ? "DPI" : "M") : "v × " + k;
    case u::Kind::Affine: return "(v + " + shortest(u::affineOffset(x)) + ") × " + k;
    case u::Kind::Inverse: return k + " / v";
    default: return std::string(u::functionFormula(x)) + (x.factor != 0 ? ", k = " + k : "");
    }
}
std::string escape(std::string s) {
    for (std::size_t p = 0; (p = s.find('|', p)) != std::string::npos; p += 2) s.replace(p, 1, "\\|");
    return s;
}
std::string conversionDoc() {
    std::ostringstream o;
    std::size_t fx = 0;
    for (std::size_t i = 0; i < u::unitCount(); ++i) fx += u::unit(i).has(u::flag::FxCG50);
    o << "# CONVERSION — categorie, unita', fattori e fonti (v1.14.0)\n\n"
      << "<!-- Generato da tests/conversion_v114_test.cpp dalla tabella di src/units.cpp e src/unit_formats.cpp:\n"
      << "     il test fallisce se questo file non coincide. Per rigenerarlo: FXAI_UPDATE_DOCS=1 build/host-pico_plus_2w/conversion_v114_test -->\n\n"
      << "CONVERSION e Run-Matrix (OPTN › CONVERT e la sintassi `valore[unita']▶[unita']`) usano **una sola tabella**\n"
      << "(`include/units.hpp`, `src/units.cpp`), in flash. Totale: **" << u::categoryCount() << " categorie numeriche, "
      << u::unitCount() << " unita'** (di cui " << fx << " della fx-CG50, le sole del menu CONVERT di Run-Matrix), "
      << u::prefixCount() << " prefissi, " << unitfmt::converterCount() << " conversioni di formato e le valute.\n\n"
      << "Modello: lineare `s = v × fattore`; affine `s = (v + offset) × fattore`; inverso `s = fattore / v`;\n"
      << "funzione `s = f(v)` con inversa. `s` e' il valore nell'unita' di riferimento della categoria (SI o coerente).\n"
      << "Costanti: SI 2019 esatte (c, h, e, k, N_A), NIST SP 811 (2008) per le unita' anglosassoni, **CODATA 2018** per le\n"
      << "costanti misurate (la stessa base della fx-CG50 per u), IAU 2012/2015 per le grandezze astronomiche.\n"
      << "Colonna *Esatto*: testo razionale usato per i risultati esatti (`pi` = π), `decimale` se il fattore scritto e' esatto.\n\n"
      << "## Fonti\n\n| # | Fonte |\n|---|---|\n";
    for (std::size_t s = 0; s < u::sourceCount(); ++s) o << "| " << s << " | " << u::sourceName(static_cast<unsigned>(s)) << " |\n";
    o << "\n## Prefissi\n\n| Simbolo | Nome | Fattore |\n|---|---|---|\n";
    for (std::size_t p = 0; p < u::prefixCount(); ++p) {
        const u::Prefix& x = u::prefix(p);
        o << "| " << x.sym << " | " << x.name << " | " << (x.iec ? "2^" : "10^") << int(x.exp) << " |\n";
    }
    o << "\nI prefissi si applicano (F4 PREFIX, o scritti nel simbolo in Run-Matrix: `[nPa]`, `[TiB]`) alle unita' marcate\n"
      << "*SI* o *IEC* nella colonna Note.\n";
    for (std::size_t g = 0; g < static_cast<std::size_t>(u::Group::Count); ++g) {
        bool header = false;
        for (std::size_t c = 0; c < u::categoryCount(); ++c) {
            const u::Category& k = u::category(c);
            if (static_cast<std::size_t>(k.group) != g) continue;
            if (!header) { o << "\n## " << u::groupName(k.group) << "\n"; header = true; }
            std::size_t first = 0, count = 0;
            u::unitsOf(c, first, count);
            o << "\n### " << k.name << (k.fxName ? std::string(" (fx-CG50: ") + k.fxName + ")" : "") << "\n\n"
              << "Riferimento: `" << k.si << "`, " << count << " unita'." << (k.note ? std::string(" ") + k.note + "." : "") << "\n\n"
              << "| Simbolo | Nome | Conversione in " << escape(k.si) << " | Esatto | Fonte | Note |\n|---|---|---|---|---|---|\n";
            for (std::size_t i = first; i < first + count; ++i) {
                const u::Unit& x = u::unit(i);
                std::string notes;
                auto add = [&](const char* t) { if (!notes.empty()) notes += ", "; notes += t; };
                if (x.has(u::flag::FxCG50)) add("fx-CG50");
                if (x.has(u::flag::PrefixSI)) add("SI");
                if (x.has(u::flag::PrefixIEC)) add("IEC");
                if (x.has(u::flag::Approx)) add("approssimata");
                if (x.has(u::flag::Historic)) add("storica");
                const std::string exactText = x.exact ? x.exact : x.has(u::flag::ExactDecimal) ? "decimale" : "—";
                o << "| `" << escape(x.name) << "` | " << escape(x.label) << " | " << escape(kindText(x)) << " | " << escape(exactText) << " | "
                  << x.source() << " | " << notes << " |\n";
            }
        }
        for (std::size_t f = 0; f < unitfmt::converterCount(); ++f) {
            const unitfmt::Converter& cv = unitfmt::converter(f);
            if (static_cast<std::size_t>(cv.group) != g) continue;
            if (!header) { o << "\n## " << u::groupName(cv.group) << "\n"; header = true; }
            o << "\n### " << cv.name << "\n\nRappresentazioni (testo -> testo):";
            for (std::size_t r = 0; cv.reps[r]; ++r) o << (r ? "; " : " ") << cv.reps[r] << (unitfmt::readable(cv, r) ? "" : " (solo uscita)");
            o << ".";
            if (cv.paramName) o << " Parametro: " << cv.paramName << " (" << shortest(cv.paramMin) << "-" << shortest(cv.paramMax) << ", predefinito " << shortest(cv.paramDefault) << ").";
            if (cv.note) o << " " << cv.note << ".";
            o << "\n";
        }
        if (static_cast<u::Group>(g) == u::Group::Currency)
            o << "\n## Valute\n\n16 valute (EUR USD GBP CHF JPY CNY CAD AUD SEK NOK DKK PLN CZK HUF INR BRL) con i tassi inseriti\n"
              << "dall'utente (F4 TASSI: unita' per 1 EUR) e salvati nel workspace (record v10). Nessuna rete richiesta;\n"
              << "l'aggiornamento via Wi-Fi e' in docs/ROADMAP.md.\n";
    }
    return o.str();
}
} // namespace

int main() {
    board::init();
    // ================================================================ tabella
    {
        CHECK(u::categoryCount() == 77);
        CHECK(u::unitCount() == 771);
        std::size_t total = 0;
        for (std::size_t c = 0; c < u::categoryCount(); ++c) {
            std::size_t first = 0, count = 0;
            u::unitsOf(c, first, count);
            CHECK(count >= 2);
            total += count;
            std::set<std::string> names;
            const u::Category& k = u::category(c);
            CHECK(k.name && *k.name && k.si && *k.si);
            CHECK((c < 11) == (k.fxName != nullptr));
            for (std::size_t i = first; i < first + count; ++i) {
                const u::Unit& x = u::unit(i);
                ::testutil::report(names.insert(x.name).second, "simbolo unico nella categoria", k.name, 0, x.name);
                CHECK(x.cat == c && x.label && *x.label && x.source() < u::sourceCount());
                CHECK(std::isfinite(x.factor) && (x.factor != 0 || x.kind() == u::Kind::Function));
                const std::string n = x.name;
                CHECK(n.find(']') == std::string::npos && n.find("\xE2\x96\xB6") == std::string::npos && n.find('[') == std::string::npos);
                // Un decimale esatto deve essere scritto con al massimo 15 cifre (il testo e' il valore).
                if (x.has(u::flag::ExactDecimal)) {
                    char b[40]; std::snprintf(b, sizeof b, "%.15g", x.factor);
                    ::testutil::report(std::strtod(b, nullptr) == x.factor, "decimale esatto <= 15 cifre", k.name, 0, x.name);
                    CHECK(x.source() == 0 && x.exact == nullptr);
                }
                if (x.exact) {
                    u::ExactFactor f;
                    ::testutil::report(u::parseExactText(x.exact, f), "testo esatto rappresentabile", k.name, 0, x.name);
                    ::testutil::report(rel(factorValue(f), x.factor, 4e-16), "testo esatto = fattore", k.name, 0, x.name);
                }
                if (x.kind() == u::Kind::Affine && u::affineOffsetText(x)) {
                    exact::Rational o;
                    u::ExactFactor f;
                    CHECK(u::parseExactText(u::affineOffsetText(x), f) && f.r.toDouble() == u::affineOffset(x));
                    (void)o;
                }
                if (x.has(u::flag::FxCG50)) CHECK(c < 11);
            }
            const u::Ref si = u::find(c, k.si);
            if (si) CHECK(u::toSI(1, si) == 1);
            else CHECK(std::strcmp(k.name, "Consumo di carburante") == 0 || std::strcmp(k.name, "Scarpe (adulti)") == 0);
        }
        CHECK(total == u::unitCount());
        // Prefissi SI completi (q ... Q, anche ronto e ronna) e IEC (Ki ... Yi).
        CHECK(u::prefixCount() == 32);
        int si = 0, iec = 0;
        for (std::size_t p = 0; p < u::prefixCount(); ++p) {
            const u::Prefix& x = u::prefix(p);
            if (x.iec) { ++iec; CHECK(x.factor == std::ldexp(1.0, x.exp)); }
            else { ++si; CHECK(rel(x.factor, std::pow(10.0, x.exp), 1e-15)); }
        }
        CHECK(si == 24 && iec == 8);
        CHECK(std::strcmp(u::prefix(0).sym, "q") == 0 && std::strcmp(u::prefix(23).sym, "Q") == 0 && std::strcmp(u::prefix(1).name, "ronto") == 0);
        const std::size_t data = cat("Dati digitali");
        const u::Ref tib = u::find(data, "TiB"), kib = u::find(data, "KiB"), qbit = u::find(data, "Qbit"), nbit = u::find(data, "Kibit");
        CHECK(tib && kib && qbit && nbit && qbit.prefix == 23);
        CHECK(u::convert(1, tib, kib) == 1073741824.0);
        CHECK(!u::find(cat("Temperatura"), "kK"));                        // prefissi solo dove ammessi
        CHECK(u::find(cat("Pressione"), "uPa") && u::find(cat("Pressione"), "uPa").prefix == 8);
    }
    // ================================================================ Run-Matrix CONVERT identico alla v1.13.1
    {
        const char* const* cats = calclib::unitCategories();
        std::size_t k = 0;
        for (; cats[k]; ++k) CHECK(k < 11 && std::strcmp(cats[k], kLegacyCategories[k]) == 0);
        CHECK(k == 11);
        std::size_t idx = 0, fxCount = 0;
        for (std::size_t c = 0; c < 11; ++c) {
            for (const calclib::UnitInfo* x : calclib::unitsOf(c)) {
                ++fxCount;
                if (idx >= sizeof(kLegacy) / sizeof(kLegacy[0])) { CHECK(false); break; }
                const Legacy& l = kLegacy[idx++];
                ::testutil::report(std::strcmp(x->name, l.name) == 0 && static_cast<int>(c) == l.cat, "stesso nome e ordine", l.name, 0, x->name);
                CHECK(x->factor == l.factor && u::affineOffset(*x) == l.offset && x->has(u::flag::FxCG50));
                CHECK(calclib::findUnit(l.name) == x);
            }
        }
        CHECK(fxCount == 106 && idx == 106);   // la v1.13.1 ne aveva 106 (non 103): 18+9+15+11+4+5+10+5+10+14+5
        for (std::size_t i = 0; i < u::unitCount(); ++i) fxCount -= u::unit(i).has(u::flag::FxCG50);
        CHECK(fxCount == 0);
        CHECK(calclib::unitsOf(11).empty());
        // Risultati bit per bit uguali alla formula della v1.13.1, per ogni coppia di unita' di ogni categoria.
        const double values[] = {1, 0, -40, 123.456, 1e-7, 3.5e12, -0.0};
        std::size_t pairs = 0, same = 0;
        for (const Legacy& a : kLegacy)
            for (const Legacy& b : kLegacy) {
                if (a.cat != b.cat) continue;
                for (double v : values) {
                    const double old = (v + a.offset) * a.factor / b.factor - b.offset;
                    const double now = calclib::convertUnit(v, *calclib::findUnit(a.name), *calclib::findUnit(b.name));
                    ++pairs;
                    same += std::memcmp(&old, &now, sizeof old) == 0;
                }
            }
        CHECK(pairs > 7000 && same == pairs);
        // Nomi non fx-CG50: non sono nel menu e non cambiano le espressioni esistenti (solo prima di ▶[).
        CHECK(!calclib::findUnit("nm") && !calclib::unitToken("nm", false) && calclib::unitToken("nm", true));
        CHECK(!calclib::unitToken("A", false) && !calclib::unitToken("px", true));
        Workspace w; w.settings.angle = AngleMode::Degrees;
        CHECK_EQ_STR(calc(w, "100[\xC2\xB0" "C]" + kConv + "[\xC2\xB0" "F]"), "212");
        CHECK_NEAR(num(w, "1[atm]" + kConv + "[mmHg]"), 760, 1e-9);
        CHECK(fails<MathError>(w, "1[eV]" + kConv + "[K]"));                       // due nomi fx: come prima
        CHECK(fails<MathError>(w, "1[m]" + kConv + "[kg]"));
        CHECK(fails<SyntaxError>(w, "1[m]"));
        // Unita' nuove con la sintassi [ ]▶[ ] (anche derivate e prefissi).
        CHECK_NEAR(num(w, "10[nm]" + kConv + "[eV]"), 123.984198, 1e-6);           // h·c/e = 1239.84198 eV·nm
        CHECK_NEAR(num(w, "1[mile]" + kConv + "[league]"), 1.0 / 3, 1e-15);
        CHECK_NEAR(num(w, "2[kWb]" + kConv + "[Mx]"), 2e11, 1e-3);
        CHECK_NEAR(num(w, "1[cm]" + kConv + "[nm]"), 1e7, 1e-6);                  // fx + nuova
        CHECK_NEAR(num(w, "30[dBm]" + kConv + "[W]"), 1, 1e-12);
        CHECK(fails<SyntaxError>(w, "1[px]" + kConv + "[in]"));                    // unita' con parametro: solo CONVERSION
        CHECK(fails<MathError>(w, "1[nm]" + kConv + "[kg]"));
        CHECK(fails<MathError>(w, "0[L/100km]" + kConv + "[km/L]"));
        CHECK_EQ_STR(calc(w, "{1,2}[in]" + kConv + "[thou]"), "{1000,2000}");
        // pointa::convert e' un wrapper della stessa tabella.
        CHECK(calclib::convertUnitNames(1, "km", "m") == 1000);
    }
    // ================================================================ valori di riferimento, per categoria
    {
        // Lunghezza (NIST SP 811 B.8; IAU 2015 B2; CSS 96 px/in)
        CHECK(conv("Lunghezza", 1, "in", "m") == 0.0254);
        CHECK_REL(conv("Lunghezza", 1, "mile", "km"), 1.609344, 1e-15);
        CHECK_REL(conv("Lunghezza", 1, "ft(US)", "m"), 0.3048006096012192, 1e-15);
        CHECK_REL(conv("Lunghezza", 72, "pt", "in"), 1, 1e-15);
        CHECK_REL(conv("Lunghezza", 96, "px", "in"), 1, 1e-15);
        { u::Params p; p.dpi = 300; CHECK_REL(conv("Lunghezza", 300, "px", "in", p), 1, 1e-15); }
        CHECK_REL(conv("Lunghezza", 1, "Mpc", "l.y."), 3.2615637771674336e6, 1e-12);
        CHECK_REL(conv("Lunghezza", 1, "a0", "\xC3\x85"), 0.529177210903, 1e-12);
        CHECK_REL(conv("Lunghezza", 1, "m.p.", "km"), 1.48, 1e-15);
        // Area
        CHECK(conv("Area", 1, "acre", "m\xC2\xB2") == 4046.8564224);
        CHECK_REL(conv("Area", 1, "cmil", "m\xC2\xB2"), 5.067075e-10, 1e-6);                 // NIST: 5,067 075 E-10
        CHECK_REL(conv("Area", 1, "section", "m\xC2\xB2"), 2.589998e6, 1e-6);                // NIST: 2,589 998 E+06
        CHECK_REL(conv("Area", 1, "b", "m\xC2\xB2"), 1e-28, 1e-15);
        // Volume
        CHECK_REL(conv("Volume", 1, "gal(US)", "L"), 3.785411784, 1e-15);
        CHECK_REL(conv("Volume", 1, "bbl", "L"), 158.987294928, 1e-15);
        CHECK_REL(conv("Volume", 1, "ac\xE2\x80\xA2" "ft", "m\xC2\xB3"), 1233.48183754752, 1e-15);
        CHECK_REL(conv("Volume", 1, "bu(US)", "L"), 35.23907016688, 1e-15);
        CHECK_REL(conv("Volume", 8, "pt(UK)", "gal(UK)"), 1, 1e-15);
        // Tempo
        CHECK_REL(conv("Tempo", 1, "yr(greg)", "day"), 365.2425, 1e-15);
        CHECK_REL(conv("Tempo", 12, "mese", "yr(greg)"), 1, 1e-15);
        CHECK_REL(conv("Tempo", 1, "week", "h"), 168, 1e-15);
        CHECK_REL(conv("Tempo", 1, "mese(sin)", "day"), 29.530588853, 1e-12);
        // Temperatura
        CHECK_REL(conv("Temperatura", -40, "\xC2\xB0" "C", "\xC2\xB0" "F"), -40, 1e-13);
        CHECK_REL(conv("Temperatura", 0, "K", "\xC2\xB0" "C"), -273.15, 1e-15);
        CHECK_REL(conv("Temperatura", 80, "\xC2\xB0" "R\xC3\xA9", "\xC2\xB0" "C"), 100, 1e-13);
        CHECK_REL(conv("Temperatura", 60, "\xC2\xB0" "R\xC3\xB8", "\xC2\xB0" "C"), 100, 1e-13);
        CHECK(std::fabs(conv("Temperatura", 0, "\xC2\xB0" "De", "\xC2\xB0" "C") - 100) < 1e-12);
        CHECK(std::fabs(conv("Temperatura", 150, "\xC2\xB0" "De", "\xC2\xB0" "C")) < 1e-12);
        CHECK_REL(conv("Temperatura", 33, "\xC2\xB0" "N", "\xC2\xB0" "C"), 100, 1e-13);
        CHECK_REL(conv("Temperatura", 491.67, "\xC2\xB0" "R", "K"), 273.15, 1e-13);
        // Intervalli di temperatura
        CHECK_REL(conv("Intervallo di temperatura", 9, "\xCE\x94\xC2\xB0" "F", "\xCE\x94K"), 5, 1e-15);
        CHECK_REL(conv("Intervallo di temperatura", 10, "\xCE\x94\xC2\xB0" "C", "\xCE\x94K"), 10, 1e-15);
        // Velocita' (mach: ISA 15 °C; ritmo inverso)
        CHECK_REL(conv("Velocita'", 1, "knot", "m/s"), 1852.0 / 3600, 1e-15);
        CHECK_REL(conv("Velocita'", 1, "mach", "m/s"), 340.294, 1e-15);
        CHECK_REL(conv("Velocita'", 5, "min/km", "km/h"), 12, 1e-14);
        CHECK_REL(conv("Velocita'", 12, "km/h", "min/km"), 5, 1e-14);
        CHECK_REL(conv("Velocita'", 1, "c", "km/s"), 299792.458, 1e-15);
        CHECK(errorOf([] { conv("Velocita'", 0, "min/km", "m/s"); }) == "Math ERROR");
        CHECK(errorOf([] { conv("Velocita'", 0, "m/s", "min/km"); }) == "Math ERROR");
        // Accelerazione
        CHECK(conv("Accelerazione", 1, "g0", "m/s\xC2\xB2") == 9.80665);
        CHECK_REL(conv("Accelerazione", 1, "Gal", "m/s\xC2\xB2"), 0.01, 1e-15);
        // Massa (NIST, CODATA 2018)
        CHECK(conv("Massa", 1, "lb", "kg") == 0.45359237);
        CHECK_REL(conv("Massa", 1, "oz t", "g"), 31.1034768, 1e-15);
        CHECK_REL(conv("Massa", 1, "gr", "mg"), 64.79891, 1e-15);
        CHECK_REL(conv("Massa", 1, "m_p", "u"), 1.007276466621, 1e-12);            // CODATA 2018
        CHECK_REL(conv("Massa", 1, "stone", "lb"), 14, 1e-15);
        // Forza
        CHECK(conv("Forza", 1, "lbf", "N") == 4.4482216152605);
        CHECK_REL(conv("Forza", 1, "kip", "lbf"), 1000, 1e-15);
        CHECK_REL(conv("Forza", 1, "pdl", "N"), 0.138254954376, 1e-15);
        // Pressione
        CHECK_REL(conv("Pressione", 1, "psi", "Pa"), 6894.757293168361, 1e-15);         // NIST: 6,894 757 E+03
        CHECK_REL(conv("Pressione", 760, "Torr", "atm"), 1, 1e-15);
        CHECK_REL(conv("Pressione", 1, "at", "kgf/cm\xC2\xB2"), 1, 1e-15);
        CHECK_REL(conv("Pressione", 1, "cmHg", "Pa"), 1333.22387415, 1e-15);
        // Energia
        CHECK_REL(conv("Energia", 1, "Eh", "eV"), 27.211386245988, 1e-12);              // CODATA 2018
        CHECK_REL(conv("Energia", 1, "kWh", "MJ"), 3.6, 1e-15);
        CHECK_REL(conv("Energia", 1, "Btu(th)", "J"), 1054.3502644888889, 1e-15);
        CHECK_REL(conv("Energia", 1, "tep", "GJ"), 41.868, 1e-15);
        CHECK_REL(conv("Energia", 1, "quad", "Btu"), 1e15, 1e-15);
        // Potenza (dBm, dBW: non lineari)
        CHECK_REL(conv("Potenza", 1, "CV", "W"), 735.49875, 1e-15);
        CHECK_REL(conv("Potenza", 1, "TR", "W"), 3516.8528420666667, 1e-14);           // NIST: 3,516 853 E+03
        CHECK_REL(conv("Potenza", 0, "dBm", "mW"), 1, 1e-15);
        CHECK_REL(conv("Potenza", 30, "dBm", "W"), 1, 1e-14);
        CHECK_REL(conv("Potenza", -30, "dBW", "mW"), 1, 1e-14);
        CHECK_REL(conv("Potenza", 1, "W", "dBm"), 30, 1e-14);
        CHECK(errorOf([] { conv("Potenza", 0, "W", "dBm"); }) == "Math ERROR");
        CHECK(errorOf([] { conv("Potenza", -1, "W", "dBW"); }) == "Math ERROR");
        // Coppia
        CHECK_REL(conv("Coppia / momento", 1, "lbf\xE2\x80\xA2" "ft", "N\xE2\x80\xA2m"), 1.3558179483314004, 1e-15);
        // Angoli
        CHECK_REL(conv("Angolo piano", 180, "deg", "rad"), 3.141592653589793, 1e-15);
        CHECK_REL(conv("Angolo piano", 6400, "mil(NATO)", "giro"), 1, 1e-15);
        CHECK_REL(conv("Angolo piano", 1, "gon", "deg"), 0.9, 1e-15);
        CHECK_REL(conv("Angolo piano", 1, "h(ang)", "deg"), 15, 1e-15);
        CHECK_REL(conv("Angolo solido", 1, "sfera", "sr"), 4 * 3.141592653589793, 1e-15);
        CHECK_REL(conv("Angolo solido", 1, "sr", "deg\xC2\xB2"), 3282.806350011744, 1e-13);
        // Frequenza e velocita' angolare
        CHECK_REL(conv("Frequenza", 60, "rpm", "Hz"), 1, 1e-15);
        CHECK_REL(conv("Frequenza", 2 * 3.141592653589793, "rad/s", "Hz"), 1, 1e-15);
        CHECK_REL(conv("Velocita' angolare", 60, "rpm", "rad/s"), 2 * 3.141592653589793, 1e-15);
        // Densita', portate, consumo
        CHECK_REL(conv("Densita'", 1, "lb/ft\xC2\xB3", "kg/m\xC2\xB3"), 16.018463373960138, 1e-14);   // NIST: 1,601 846 E+01
        CHECK_REL(conv("Densita'", 1, "g/cm\xC2\xB3", "kg/m\xC2\xB3"), 1000, 1e-15);
        CHECK_REL(conv("Portata volumetrica", 1, "cfm", "m\xC2\xB3/s"), 4.719474432e-4, 1e-15);
        CHECK_REL(conv("Portata volumetrica", 1, "gpm(US)", "L/min"), 3.785411784, 1e-15);
        CHECK_REL(conv("Portata massica", 1, "lb/h", "kg/s"), 1.2599788055555556e-4, 1e-14);   // NIST: 1,259 979 E-04
        CHECK_REL(conv("Consumo di carburante", 5, "L/100km", "km/L"), 20, 1e-15);
        CHECK_REL(conv("Consumo di carburante", 30, "mpg(US)", "L/100km"), 7.840486111111111, 1e-12);
        CHECK_REL(conv("Consumo di carburante", 7.840486111111111, "L/100km", "mpg(US)"), 30, 1e-12);
        CHECK_REL(conv("Consumo di carburante", 4, "gal/100mi(US)", "mpg(US)"), 25, 1e-14);
        CHECK(errorOf([] { conv("Consumo di carburante", 0, "L/100km", "km/L"); }) == "Math ERROR");
        CHECK_REL(conv("Consumo di carburante", -5, "L/100km", "km/L"), -20, 1e-15);  // negativo: inverso coerente
        // Dati
        CHECK(conv("Dati digitali", 1, "KiB", "B") == 1024);
        CHECK(conv("Dati digitali", 1, "GB", "B") == 1e9);
        CHECK(conv("Velocita' di trasmissione", 1, "MB/s", "Mbit/s") == 8);
        // Elettricita' e magnetismo
        CHECK_REL(conv("Carica elettrica", 1, "Ah", "C"), 3600, 1e-15);
        CHECK_REL(conv("Carica elettrica", 1, "faraday", "C"), 96485.33212331001, 1e-15);
        CHECK_REL(conv("Carica elettrica", 1, "statC", "C"), 3.3356409519815204e-10, 1e-15);
        CHECK_REL(conv("Corrente elettrica", 1, "abA", "A"), 10, 1e-15);
        CHECK_REL(conv("Tensione elettrica", 1, "statV", "V"), 299.792458, 1e-15);
        CHECK_REL(conv("Resistenza", 1, "k\xCE\xA9", "\xCE\xA9"), 1000, 1e-15);
        CHECK_REL(conv("Conduttanza", 1, "mho", "S"), 1, 1e-15);
        CHECK_REL(conv("Capacita'", 1, "\xC2\xB5" "F", "nF"), 1000, 1e-15);
        CHECK_REL(conv("Induttanza", 1, "mH", "\xC2\xB5H"), 1000, 1e-15);
        CHECK_REL(conv("Resistivita'", 1, "\xCE\xA9\xE2\x80\xA2" "cm", "\xCE\xA9\xE2\x80\xA2m"), 0.01, 1e-15);
        CHECK_REL(conv("Conducibilita' elettrica", 1, "mS/cm", "S/m"), 0.1, 1e-15);
        CHECK_REL(conv("Flusso magnetico", 1, "Mx", "Wb"), 1e-8, 1e-15);
        CHECK_REL(conv("Induzione magnetica", 1, "G", "\xC2\xB5T"), 100, 1e-15);
        CHECK_REL(conv("Campo magnetico H", 1, "Oe", "A/m"), 79.57747154594767, 1e-15);
        // Fotometria e radiazioni
        CHECK_REL(conv("Intensita' luminosa", 1, "kcd", "cd"), 1000, 1e-15);
        CHECK_REL(conv("Flusso luminoso", 1, "klm", "lm"), 1000, 1e-15);
        CHECK_REL(conv("Illuminamento", 1, "fc", "lx"), 10.763910416709722, 1e-15);
        CHECK_REL(conv("Luminanza", 1, "fL", "cd/m\xC2\xB2"), 3.4262590996353905, 1e-14);
        CHECK_REL(conv("Attivita' radioattiva", 1, "Ci", "GBq"), 37, 1e-15);
        CHECK_REL(conv("Dose assorbita", 1, "rad", "mGy"), 10, 1e-15);
        CHECK_REL(conv("Dose equivalente", 1, "rem", "mSv"), 10, 1e-15);
        CHECK_REL(conv("Esposizione", 1, "R", "C/kg"), 2.58e-4, 1e-15);
        // Chimica
        CHECK_REL(conv("Quantita' di sostanza", 1, "mol", "part."), 6.02214076e23, 1e-15);
        CHECK_REL(conv("Concentrazione molare", 1, "M", "mol/m\xC2\xB3"), 1000, 1e-15);
        CHECK_REL(conv("Concentrazione in massa", 1, "mg/dL", "mg/L"), 10, 1e-15);
        CHECK_REL(conv("Rapporti adimensionali", 1, "%", "ppm"), 1e4, 1e-15);
        CHECK_REL(conv("Rapporti adimensionali", 1, "bp", "%"), 0.01, 1e-15);
        // Viscosita', tensione superficiale, termica, meccanica (NIST SP 811 B.9)
        CHECK_REL(conv("Viscosita' dinamica", 1, "cP", "mPa\xE2\x80\xA2s"), 1, 1e-15);
        CHECK_REL(conv("Viscosita' cinematica", 1, "cSt", "mm\xC2\xB2/s"), 1, 1e-15);
        CHECK_REL(conv("Tensione superficiale", 1, "dyn/cm", "mN/m"), 1, 1e-15);
        CHECK_REL(conv("Conducibilita' termica", 1, "Btu/(h\xE2\x80\xA2" "ft\xE2\x80\xA2\xC2\xB0" "F)", "W/(m\xE2\x80\xA2K)"), 1.730735, 1e-6);
        CHECK_REL(conv("Coeff. di scambio termico", 1, "Btu/(h\xE2\x80\xA2" "ft\xC2\xB2\xE2\x80\xA2\xC2\xB0" "F)", "W/(m\xC2\xB2\xE2\x80\xA2K)"), 5.678263, 1e-6);
        CHECK_REL(conv("Calore specifico", 1, "Btu/(lb\xE2\x80\xA2\xC2\xB0" "F)", "J/(kg\xE2\x80\xA2K)"), 4186.8, 1e-15);
        CHECK_REL(conv("Flusso termico", 1, "Btu/(h\xE2\x80\xA2" "ft\xC2\xB2)", "W/m\xC2\xB2"), 3.154591, 1e-6);
        CHECK_REL(conv("Entropia / capacita' termica", 1, "cal/K", "J/K"), 4.1868, 1e-15);
        CHECK_REL(conv("Resistenza termica", 1, "\xC2\xB0" "F\xE2\x80\xA2h/Btu", "K/W"), 1.895634, 1e-6);
        CHECK_REL(conv("Isolamento termico (R)", 1, "ft\xC2\xB2\xE2\x80\xA2\xC2\xB0" "F\xE2\x80\xA2h/Btu", "m\xC2\xB2\xE2\x80\xA2K/W"), 0.1761102, 1e-6);
        CHECK_REL(conv("Quantita' di moto / impulso", 1, "lbf\xE2\x80\xA2s", "N\xE2\x80\xA2s"), 4.4482216152605, 1e-15);
        CHECK_REL(conv("Momento angolare", 1, "\xC4\xA7", "J\xE2\x80\xA2s"), 1.054571817e-34, 1e-9);   // CODATA 2018
        CHECK_REL(conv("Momento d'inerzia", 1, "lb\xE2\x80\xA2" "ft\xC2\xB2", "kg\xE2\x80\xA2m\xC2\xB2"), 0.04214011, 1e-6);
        CHECK_REL(conv("Rigidezza", 1, "lbf/in", "N/m"), 175.1268, 1e-6);
        // Logaritmiche
        CHECK_REL(conv("Rapporto di potenza", 10, "dB", "P/P0"), 10, 1e-15);
        CHECK_REL(conv("Rapporto di potenza", 1, "B", "dB"), 10, 1e-15);
        CHECK_REL(conv("Rapporto di potenza", 1, "Np", "dB"), 8.685889638065037, 1e-14);
        CHECK_REL(conv("Rapporto di ampiezza", 20, "dB", "A/A0"), 10, 1e-15);
        CHECK_REL(conv("Rapporto di ampiezza", 1, "Np", "dB"), 8.685889638065037, 1e-14);
        CHECK_REL(conv("Pressione sonora", 0, "dB SPL", "\xC2\xB5Pa"), 20, 1e-14);
        CHECK_REL(conv("Pressione sonora", 94, "dB SPL", "Pa"), 1.0023744672545447, 1e-13);
        CHECK_REL(conv("Livello di tensione", 0, "dBu", "V"), 0.7745966692414834, 1e-15);
        CHECK_REL(conv("Livello di tensione", 1, "V", "dBV"), 0, 1e-15);
        CHECK_REL(conv("Rapporto di frequenza", 1, "ottava", "cent"), 1200, 1e-13);
        CHECK_REL(conv("Rapporto di frequenza", 12, "semitono", "ottava"), 1, 1e-14);
        CHECK_REL(conv("Rapporto di frequenza", 1, "decade", "ottava"), 3.321928094887362, 1e-14);
        CHECK_REL(conv("Acidita' (pH)", 7, "pH", "mol/L"), 1e-7, 1e-14);
        CHECK_REL(conv("Acidita' (pH)", 0, "pH", "mol/L"), 1, 1e-15);
        CHECK_REL(conv("Acidita' (pH)", -1, "pH", "mol/L"), 10, 1e-14);
        CHECK_REL(conv("Acidita' (pH)", 4, "pOH", "pH"), 10, 1e-13);
        CHECK_REL(conv("Acidita' (pH)", 1e-3, "OH-", "pH"), 11, 1e-13);
        CHECK(errorOf([] { conv("Acidita' (pH)", 0, "mol/L", "pH"); }) == "Math ERROR");
        CHECK(errorOf([] { conv("Acidita' (pH)", -1e-7, "mol/L", "pH"); }) == "Math ERROR");
        CHECK_REL(conv("Calibro dei fili (AWG)", 36, "AWG", "mm"), 0.127, 1e-14);
        CHECK_REL(conv("Calibro dei fili (AWG)", -3, "AWG", "mm"), 11.684, 1e-4);     // 0000 = 0,46 in
        CHECK_REL(conv("Calibro dei fili (AWG)", 10, "AWG", "mm"), 2.588, 1e-3);
        CHECK_REL(conv("Calibro dei fili (AWG)", 10, "AWG", "mm\xC2\xB2"), 5.26, 1e-3);
        CHECK_REL(conv("Calibro dei fili (AWG)", 0, "AWG", "in"), 0.3249, 1e-3);
        CHECK(errorOf([] { conv("Calibro dei fili (AWG)", 61, "AWG", "mm"); }) == "Argument ERROR");
        CHECK(errorOf([] { conv("Calibro dei fili (AWG)", 50, "mm", "AWG"); }) == "Argument ERROR");
        CHECK(errorOf([] { conv("Calibro dei fili (AWG)", -1, "mm\xC2\xB2", "mm"); }) == "Argument ERROR");
        CHECK_REL(conv("Magnitudo sismica", 0, "Mw", "J"), 63095.734448019344, 1e-14);
        CHECK_REL(conv("Magnitudo sismica", 6, "Mw", "J"), 6.309573444801943e13, 1e-13);
        CHECK(errorOf([] { conv("Magnitudo sismica", 0, "J", "Mw"); }) == "Math ERROR");
        CHECK_REL(conv("Magnitudo stellare", 5, "mag", "F/F0"), 0.01, 1e-14);
        CHECK_REL(conv("Magnitudo stellare", -5, "mag", "F/F0"), 100, 1e-14);
        // Derivate (c, h, e, k_B, N_A esatte; CODATA 2018)
        CHECK_REL(conv("Fotone: \xCE\xBB \xE2\x86\x94 \xCE\xBD \xE2\x86\x94 E \xE2\x86\x94 \xCF\x83", 500, "nm", "THz"), 599.584916, 1e-15);
        CHECK_REL(conv("Fotone: \xCE\xBB \xE2\x86\x94 \xCE\xBD \xE2\x86\x94 E \xE2\x86\x94 \xCF\x83", 1, "eV", "nm"), 1239.8419843320026, 1e-14);
        CHECK_REL(conv("Fotone: \xCE\xBB \xE2\x86\x94 \xCE\xBD \xE2\x86\x94 E \xE2\x86\x94 \xCF\x83", 1, "cm\xE2\x81\xBB\xC2\xB9", "GHz"), 29.9792458, 1e-15);
        CHECK(errorOf([] { conv("Fotone: \xCE\xBB \xE2\x86\x94 \xCE\xBD \xE2\x86\x94 E \xE2\x86\x94 \xCF\x83", 0, "nm", "eV"); }) == "Math ERROR");
        CHECK_REL(conv("Massa \xE2\x86\x94 energia (E = mc\xC2\xB2)", 1, "u", "MeV"), 931.49410242, 1e-10);    // CODATA 2018
        CHECK_REL(conv("Massa \xE2\x86\x94 energia (E = mc\xC2\xB2)", 1, "kg", "J"), 89875517873681764.0, 1e-15);
        CHECK_REL(conv("Temperatura \xE2\x86\x94 energia (kT)", 1, "eV", "K"), 11604.51812, 1e-9);           // CODATA 2018
        CHECK_REL(conv("Temperatura \xE2\x86\x94 energia (kT)", 25, "\xC2\xB0" "C", "K"), 298.15, 1e-14);
        CHECK_REL(conv("Temperatura \xE2\x86\x94 energia (kT)", 300, "K", "meV"), 25.85199, 1e-6);
        CHECK_REL(conv("Moli \xE2\x86\x94 massa \xE2\x86\x94 particelle", 18.015, "g", "mol"), 1, 1e-15);
        { u::Params p; p.molarMass = 58.44; CHECK_REL(conv("Moli \xE2\x86\x94 massa \xE2\x86\x94 particelle", 2, "mol", "g", p), 116.88, 1e-14); }
        { u::Params p; p.molarMass = 0; CHECK(errorOf([&] { conv("Moli \xE2\x86\x94 massa \xE2\x86\x94 particelle", 1, "g", "mol", p); }) == "Argument ERROR"); }
        CHECK_REL(conv("Moli \xE2\x86\x94 massa \xE2\x86\x94 particelle", 1, "mol", "L(STP)"), 22.413969545014137, 1e-14);
        CHECK_REL(conv("Peso \xE2\x86\x94 massa (g\xE2\x82\x80)", 1, "kg", "N"), 9.80665, 1e-15);
        CHECK_REL(conv("Peso \xE2\x86\x94 massa (g\xE2\x82\x80)", 1, "lb", "lbf"), 1, 1e-15);
        // Taglie (approssimate)
        CHECK_REL(conv("Scarpe (adulti)", 42, "EU", "cm"), 26.5, 1e-14);
        CHECK(std::fabs(conv("Scarpe (adulti)", 8, "UK", "US(M)") - 9) < 1e-12);
        CHECK(std::fabs(conv("Scarpe (adulti)", 8, "UK", "US(W)") - 10) < 1e-12);
        CHECK(std::fabs(conv("Scarpe (adulti)", 26.5, "cm", "UK") - 8.07) < 0.01);
        CHECK(u::find(cat("Scarpe (adulti)"), "EU").unit->has(u::flag::Approx));
    }
    // ================================================================ round trip A->B->A, categorie incompatibili
    {
        const double values[] = {0.7, 3, 250, -2.5};
        std::size_t trips = 0, bad = 0;
        for (std::size_t c = 0; c < u::categoryCount(); ++c) {
            std::size_t first = 0, count = 0;
            u::unitsOf(c, first, count);
            for (std::size_t i = first; i < first + count; ++i)
                for (std::size_t j = first; j < first + count; ++j)
                    for (double v : values) {
                        const u::Ref a{&u::unit(i), -1}, b{&u::unit(j), -1};
                        double there = 0;
                        try { there = u::convert(v, a, b); } catch (const MathError&) { continue; }   // fuori dominio
                        double back = 0;
                        try { back = u::convert(there, b, a); } catch (const MathError&) { ++bad; continue; }
                        ++trips;
                        if (!(std::fabs(back - v) <= 1e-12 * std::max(1.0, std::fabs(v)))) {
                            ++bad;
                            std::fprintf(stderr, "round trip %s -> %s -> %s: %.17g -> %.17g\n", u::unit(i).name, u::unit(j).name, u::unit(i).name, v, back);
                        }
                    }
            // Categorie incompatibili: Argument ERROR.
            const std::size_t next = (c + 1) % u::categoryCount();
            std::size_t f2 = 0, n2 = 0;
            u::unitsOf(next, f2, n2);
            const std::string e = errorOf([&] { u::convert(1, {&u::unit(first), -1}, {&u::unit(f2), -1}); });
            ::testutil::report(e == "Argument ERROR", "categorie incompatibili", u::category(c).name, 0, e);
        }
        CHECK(trips > 40000 && bad == 0);
        // Con i prefissi
        const std::size_t pres = cat("Pressione");
        const u::Ref npa = u::find(pres, "nPa"), ksi = u::find(pres, "ksi");
        CHECK(npa && npa.prefix == 7 && rel(u::convert(u::convert(3, npa, ksi), ksi, npa), 3, 1e-12));
        CHECK_REL(u::convert(1, u::find(pres, "QPa"), u::find(pres, "Pa")), 1e30, 1e-15);
        // NaN e infiniti
        CHECK(errorOf([] { conv("Lunghezza", NAN, "m", "cm"); }) == "Math ERROR");
        CHECK(errorOf([] { conv("Lunghezza", INFINITY, "m", "cm"); }) == "Math ERROR");
        CHECK(errorOf([] { conv("Lunghezza", 1e300, "Gpc", "l_P"); }) == "Math ERROR");
    }
    // ================================================================ risultati esatti
    {
        auto ex = [](const char* category, const exact::Value& v, const char* from, const char* to) {
            const std::size_t c = cat(category);
            exact::Value out;
            if (!u::convertExact(v, R(c, from), R(c, to), out)) return std::string("(no)");
            return exact::toText(out, false);
        };
        const exact::Value third = exact::Value::fromRational({1, 3});
        CHECK_EQ_STR(ex("Lunghezza", third, "m", "cm"), "100/3");
        CHECK_EQ_STR(ex("Lunghezza", exact::Value::fromInteger(1), "in", "cm"), "127/50");
        CHECK_EQ_STR(ex("Lunghezza", exact::Value::fromInteger(1), "ft(US)", "m"), "1200/3937");
        CHECK_EQ_STR(ex("Velocita'", exact::Value::fromInteger(1), "km/h", "m/s"), "5/18");
        CHECK_EQ_STR(ex("Temperatura", exact::Value::fromInteger(100), "\xC2\xB0" "C", "\xC2\xB0" "F"), "212");
        CHECK_EQ_STR(ex("Temperatura", exact::Value::fromRational({493, 5}), "\xC2\xB0" "F", "\xC2\xB0" "C"), "37");
        CHECK_EQ_STR(ex("Angolo piano", exact::Value::fromInteger(180), "deg", "rad"), "\xCF\x80");
        CHECK_EQ_STR(ex("Angolo piano", exact::Value::piTimes({1, 2}), "rad", "deg"), "90");
        CHECK_EQ_STR(ex("Pressione", exact::Value::fromInteger(1), "atm", "mmHg"), "760");
        CHECK_EQ_STR(ex("Massa \xE2\x86\x94 energia (E = mc\xC2\xB2)", exact::Value::fromInteger(1), "kg", "J"), "89875517873681764");
        CHECK_EQ_STR(ex("Massa", exact::Value::fromInteger(1), "u", "kg"), "(no)");                     // CODATA: non esatta
        CHECK_EQ_STR(ex("Dati digitali", exact::Value::fromInteger(3), "MiB", "KiB"), "3072");
        // Prefisso esatto
        exact::Value out;
        const std::size_t len = cat("Lunghezza");
        CHECK(u::convertExact(exact::Value::fromInteger(5), u::find(len, "nm"), u::find(len, "pm"), out) && exact::toText(out, false) == "5000");
    }
    // ================================================================ conversioni di formato
    {
        const std::size_t base = converterIndex("Basi numeriche");
        CHECK_EQ_STR(fmtc(base, 1, 0, "FF", 12), "255");
        CHECK_EQ_STR(fmtc(base, 2, 0, "-101.1", 12), "-5.5");
        CHECK_EQ_STR(fmtc(base, 0, 1, "255.625", 12), "FF.A");
        CHECK_EQ_STR(fmtc(base, 4, 0, "Z", 36), "35");
        CHECK_EQ_STR(fmtc(base, 0, 2, "0.1", 12).substr(0, 12), "0.0001100110");
        CHECK(errorOf([&] { fmtc(base, 1, 0, "12G", 12); }) == "Syntax ERROR");
        CHECK(errorOf([&] { fmtc(base, 4, 0, "1", 1); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(base, 1, 0, "1" + std::string(17, '0'), 12); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(base, 0, 1, "", 12); }) == "Syntax ERROR");
        const std::size_t twos = converterIndex("Complemento a due");
        CHECK_EQ_STR(fmtc(twos, 0, 1, "-1", 16), "FFFF");
        CHECK_EQ_STR(fmtc(twos, 1, 0, "8000", 16), "-32768");
        CHECK_EQ_STR(fmtc(twos, 0, 2, "-2", 8), "11111110");
        CHECK_EQ_STR(fmtc(twos, 1, 0, "8000000000000000", 64), "-9223372036854775808");
        CHECK(errorOf([&] { fmtc(twos, 0, 1, "40000", 16); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(twos, 0, 1, "1", 12); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(twos, 1, 0, "1FFFF", 16); }) == "Argument ERROR");
        const std::size_t bcd = converterIndex("BCD");
        CHECK_EQ_STR(fmtc(bcd, 0, 1, "1234"), "0001 0010 0011 0100");
        CHECK_EQ_STR(fmtc(bcd, 1, 0, "1 0010 1001"), "129");
        CHECK(errorOf([&] { fmtc(bcd, 1, 0, "1010"); }) == "Argument ERROR");
        const std::size_t gray = converterIndex("Codice Gray");
        CHECK_EQ_STR(fmtc(gray, 0, 2, "13"), "1011");
        CHECK_EQ_STR(fmtc(gray, 2, 0, "1011"), "13");
        const std::size_t ieee = converterIndex("IEEE-754");
        CHECK_EQ_STR(fmtc(ieee, 0, 2, "1").substr(0, 8), "3F800000");
        CHECK_EQ_STR(fmtc(ieee, 0, 3, "0.1").substr(0, 16), "3FB999999999999A");
        CHECK_EQ_STR(fmtc(ieee, 0, 1, "65504").substr(0, 4), "7BFF");
        CHECK_EQ_STR(fmtc(ieee, 0, 1, "1e5").substr(0, 4), "7C00");
        CHECK_EQ_STR(fmtc(ieee, 0, 1, "-0.5").substr(0, 4), "B800");
        CHECK_EQ_STR(fmtc(ieee, 1, 0, "0001"), "5.9604644775390625e-08");
        CHECK_EQ_STR(fmtc(ieee, 2, 0, "7FC00000"), "NaN");
        CHECK_EQ_STR(fmtc(ieee, 3, 0, "FFF0000000000000"), "-Inf");
        CHECK_EQ_STR(fmtc(ieee, 0, 2, "1").substr(8), "  s=0 e=127 m=000000");
        CHECK(errorOf([&] { fmtc(ieee, 2, 0, "zz"); }) == "Syntax ERROR");
        CHECK(errorOf([&] { fmtc(ieee, 1, 0, "12345"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(ieee, 0, 2, "1e39"); }) == "Argument ERROR");
        const std::size_t roman = converterIndex("Numeri romani");
        CHECK_EQ_STR(fmtc(roman, 0, 1, "1994"), "MCMXCIV");
        CHECK_EQ_STR(fmtc(roman, 1, 0, "mmmcmxcix"), "3999");
        CHECK_EQ_STR(fmtc(roman, 0, 2, "4234"), "(IV)CCXXXIV");
        CHECK_EQ_STR(fmtc(roman, 2, 0, "(IV)CCXXXIV"), "4234");
        CHECK_EQ_STR(fmtc(roman, 2, 0, "(MMMCMXCIX)CMXCIX"), "3999999");
        CHECK(errorOf([&] { fmtc(roman, 0, 1, "0"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(roman, 0, 1, "4000"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(roman, 0, 2, "4000000"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(roman, 1, 0, "IIII"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(roman, 1, 0, "IM"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(roman, 1, 0, "ABC"); }) == "Syntax ERROR");
        CHECK(errorOf([&] { fmtc(roman, 0, 1, "12.5"); }) == "Argument ERROR");
        const std::size_t text = converterIndex("Testo \xE2\x86\x94 Unicode");
        CHECK_EQ_STR(fmtc(text, 0, 1, "\xE2\x82\xAC"), "U+20AC");
        CHECK_EQ_STR(fmtc(text, 0, 3, "\xE2\x82\xAC"), "E2 82 AC");
        CHECK_EQ_STR(fmtc(text, 2, 0, "67 105 97 111"), "Ciao");
        CHECK_EQ_STR(fmtc(text, 1, 0, "U+1F600"), "\xF0\x9F\x98\x80");
        CHECK(errorOf([&] { fmtc(text, 3, 0, "C3 28"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(text, 3, 0, "C0 80"); }) == "Argument ERROR");          // overlong
        CHECK(errorOf([&] { fmtc(text, 1, 0, "D800"); }) == "Argument ERROR");           // surrogato
        CHECK(errorOf([&] { fmtc(text, 0, 1, std::string("\xE2\x82", 2)); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(text, 3, 0, "ABC"); }) == "Syntax ERROR");
        const std::size_t ang = converterIndex("Angoli (DMS, HMS)");
        CHECK_EQ_STR(fmtc(ang, 1, 0, "12\xC2\xB0" "30\xE2\x80\xB2" "15.5\xE2\x80\xB3"), "12.50430556");
        CHECK_EQ_STR(fmtc(ang, 1, 0, "12 30 15.5"), "12.50430556");
        CHECK_EQ_STR(fmtc(ang, 0, 1, "-0.5"), "-0\xC2\xB0" "30\xE2\x80\xB2" "0\xE2\x80\xB3");
        CHECK_EQ_STR(fmtc(ang, 2, 0, "1h"), "15");
        CHECK(errorOf([&] { fmtc(ang, 1, 0, "12 75 0"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(ang, 1, 0, "12 -3 0"); }) == "Syntax ERROR");
        CHECK(errorOf([&] { fmtc(ang, 0, 1, "abc"); }) == "Syntax ERROR");
        CHECK(errorOf([&] { fmtc(ang, 0, 1, "inf"); }) == "Syntax ERROR");
        CHECK(errorOf([&] { fmtc(ang, 0, 1, "1e999"); }) == "Argument ERROR");
        const std::size_t tim = converterIndex("Tempo (h:m:s)");
        CHECK_EQ_STR(fmtc(tim, 0, 1, "1.5"), "1:30:00");
        CHECK_EQ_STR(fmtc(tim, 2, 4, "90061"), "1d 1:01:01");
        CHECK_EQ_STR(fmtc(tim, 1, 2, "1:02:03.5"), "3723.5");
        CHECK(errorOf([&] { fmtc(tim, 1, 2, "1:75:00"); }) == "Argument ERROR");
        const std::size_t date = converterIndex("Date");
        CHECK_EQ_STR(fmtc(date, 0, 1, "2000-01-01"), "2451544.5");
        CHECK_EQ_STR(fmtc(date, 0, 2, "2000-01-01"), "51544");
        CHECK_EQ_STR(fmtc(date, 0, 5, "2000-01-01"), "946684800");
        CHECK_EQ_STR(fmtc(date, 0, 4, "2000-01-01"), "1999-W52-6");
        CHECK_EQ_STR(fmtc(date, 0, 3, "2024-12-31"), "2024-366");
        CHECK_EQ_STR(fmtc(date, 0, 6, "2026-11-17"), "martedi'");
        CHECK_EQ_STR(fmtc(date, 4, 0, "2020-W53-1"), "2020-12-28");
        CHECK_EQ_STR(fmtc(date, 1, 0, "2460384.5"), "2024-03-15");
        CHECK_EQ_STR(fmtc(date, 5, 0, "0"), "1970-01-01");
        CHECK_EQ_STR(fmtc(date, 0, 0, "15/03/2024"), "2024-03-15");
        CHECK_EQ_STR(fmtc(date, 0, 1, "0001-01-01"), "1721425.5");
        CHECK(errorOf([&] { fmtc(date, 0, 1, "2023-02-29"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(date, 0, 1, "2024-13-01"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(date, 0, 1, "10000-01-01"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(date, 4, 0, "2021-W53-1"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(date, 0, 1, "ciao"); }) == "Syntax ERROR");
        CHECK(errorOf([&] { fmtc(date, 6, 0, "lunedi'"); }) == "Argument ERROR");       // solo uscita
        CHECK(errorOf([&] { fmtc(date, 5, 0, "1e300"); }) == "Argument ERROR");
        const std::size_t diff = converterIndex("Differenza tra date");
        CHECK_EQ_STR(fmtc(diff, 0, 1, "2024-01-01 2024-12-25"), "359");
        CHECK(calclib::daysPrd(1, 1, 2024, 12, 25, 2024) == 359);                          // stesso algoritmo di Days_Prd
        CHECK_EQ_STR(fmtc(diff, 0, 1, "1800-01-01 2000-01-01"), "73048");
        CHECK_EQ_STR(fmtc(diff, 0, 1, "2024-03-01 2024-02-01"), "-29");
        CHECK_EQ_STR(fmtc(diff, 0, 3, "2020-02-29 2024-03-15"), "4 a 0 m 15 g");
        CHECK_EQ_STR(fmtc(diff, 0, 2, "2024-01-01 2024-01-20"), "2 sett. 5 g");
        CHECK(errorOf([&] { fmtc(diff, 0, 1, "2024-01-01"); }) == "Syntax ERROR");
        const std::size_t c2 = converterIndex("Coordinate 2D");
        CHECK_EQ_STR(fmtc(c2, 0, 1, "3, 4"), "5, 53.13010235\xC2\xB0");
        CHECK_EQ_STR(fmtc(c2, 1, 0, "2, 90"), "0, 2");
        CHECK(errorOf([&] { fmtc(c2, 1, 0, "-1, 30"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(c2, 0, 1, "3"); }) == "Syntax ERROR");
        const std::size_t c3 = converterIndex("Coordinate 3D");
        CHECK_EQ_STR(fmtc(c3, 0, 2, "0, 0, 2"), "2, 0\xC2\xB0, 0\xC2\xB0");
        CHECK_EQ_STR(fmtc(c3, 2, 0, "1, 90, 90"), "0, 1, 0");
        CHECK_EQ_STR(fmtc(c3, 1, 0, "2, 0, 5"), "2, 0, 5");
        const std::size_t geo = converterIndex("Coordinate geografiche");
        CHECK_EQ_STR(fmtc(geo, 0, 1, "45.4642, -9.19"), "45\xC2\xB0" "27\xE2\x80\xB2" "51.12\xE2\x80\xB3N, 9\xC2\xB0" "11\xE2\x80\xB2" "24\xE2\x80\xB3W");
        CHECK_EQ_STR(fmtc(geo, 1, 0, "45 27 51.12 N, 9 11 24 W"), "45.4642, -9.19");
        CHECK(errorOf([&] { fmtc(geo, 0, 1, "91, 0"); }) == "Argument ERROR");
        const std::size_t cpx = converterIndex("Numeri complessi");
        CHECK_EQ_STR(fmtc(cpx, 0, 1, "3+4i"), "5\xE2\x88\xA0" "53.13010235");
        CHECK_EQ_STR(fmtc(cpx, 1, 0, "2\xE2\x88\xA0" "90"), "2i");
        CHECK_EQ_STR(fmtc(cpx, 0, 0, "-i"), "-i");
        CHECK(errorOf([&] { fmtc(cpx, 0, 1, "1+"); }) == "Syntax ERROR");
        const std::size_t nt = converterIndex("Notazioni numeriche");
        CHECK_EQ_STR(fmtc(nt, 3, 1, "0.1(6)"), "1/6");
        CHECK_EQ_STR(fmtc(nt, 1, 3, "1/7"), "0.(142857)");
        CHECK_EQ_STR(fmtc(nt, 2, 1, "2 1/3"), "7/3");
        CHECK_EQ_STR(fmtc(nt, 1, 2, "-7/3"), "-2 1/3");
        CHECK_EQ_STR(fmtc(nt, 5, 0, "4.7k"), "4700");
        CHECK_EQ_STR(fmtc(nt, 0, 4, "0.000123"), "1.23E-04");
        CHECK_EQ_STR(fmtc(nt, 0, 5, "0.000123"), "123\xC2\xB5");
        CHECK(errorOf([&] { fmtc(nt, 1, 0, "7/0"); }) == "Math ERROR");
        CHECK(errorOf([&] { fmtc(nt, 1, 0, "7/"); }) == "Syntax ERROR");
        const std::size_t col = converterIndex("Colori");
        CHECK_EQ_STR(fmtc(col, 0, 2, "#FF0000"), "0\xC2\xB0, 100%, 50%");
        CHECK_EQ_STR(fmtc(col, 5, 0, "F800"), "#FF0000");
        CHECK_EQ_STR(fmtc(col, 0, 5, "#E8601E"), "0xEB03");
        CHECK_EQ_STR(fmtc(col, 4, 0, "0, 100, 100, 0"), "#FF0000");
        CHECK_EQ_STR(fmtc(col, 3, 1, "120, 100, 100"), "0, 255, 0");
        CHECK_EQ_STR(fmtc(col, 0, 0, "#f80"), "#FF8800");
        CHECK(errorOf([&] { fmtc(col, 1, 0, "300, 0, 0"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(col, 0, 1, "#GG0000"); }) == "Syntax ERROR");
        CHECK(errorOf([&] { fmtc(col, 5, 0, "1FFFF"); }) == "Argument ERROR");
        std::uint16_t rgb = 0;
        CHECK(unitfmt::colorOf(col, 0, "#FF0000", {}, rgb) && rgb == 0xF800 && !unitfmt::colorOf(col, 0, "nope", {}, rgb));
        const std::size_t rate = converterIndex("Tassi APR \xE2\x86\x94 EFF");
        CHECK_EQ_STR(fmtc(rate, 0, 1, "12", 12), "12.68250301");
        CHECK_NEAR(std::stod(fmtc(rate, 1, 0, "12.68250301", 12)), 12, 1e-8);
        CHECK_EQ_STR(fmtc(rate, 0, 2, "12", 12), "1");
        CHECK(errorOf([&] { fmtc(rate, 0, 1, "12", 0); }) == "Argument ERROR");
        const std::size_t ring = converterIndex("Anelli");
        CHECK_EQ_STR(fmtc(ring, 3, 1, "M"), "52.5");
        CHECK_EQ_STR(fmtc(ring, 2, 3, "6.5"), "M\xC2\xBD");
        CHECK_EQ_STR(fmtc(ring, 2, 3, "6"), "L\xC2\xBD");
        CHECK(errorOf([&] { fmtc(ring, 3, 0, "7"); }) == "Syntax ERROR");
        CHECK(errorOf([&] { fmtc(ring, 2, 0, "40"); }) == "Argument ERROR");
        // Indici fuori intervallo e testo troppo lungo: nessun crash.
        CHECK(errorOf([&] { fmtc(99, 0, 0, "1"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(base, 9, 0, "1"); }) == "Argument ERROR");
        CHECK(errorOf([&] { fmtc(text, 0, 1, std::string(300, 'a')); }) == "Argument ERROR");
    }
    // ================================================================ record v10 e compatibilita' v9
    {
        static const std::uint8_t kV9[] = {
#include "fixtures/workspace_v1_13_1.inc"
        };
        Workspace w;
        CHECK(deserializeWorkspace(kV9, sizeof kV9, w));
        // Contenuto scritto dalla v1.13.1 (vedi il commento del fixture): nessuna perdita.
        CHECK(w.settings.angle == AngleMode::Degrees && w.memory.recall('A') == 2.5);
        CHECK(w.history.size() == 2 && w.history[0].expression == "10[cm]\xE2\x96\xB6[in]");
        CHECK(w.strings[0] == "CONV" && w.listName(0) == "LEN" && w.lists[0].size() == 3);
        CHECK(w.settings.conversion.recent.empty() && w.settings.conversion.dpi == 96 && w.settings.conversion.rates[1] == 0);
        // Preferenze di CONVERSION nel record v10.
        auto& cv = w.settings.conversion;
        cv.rates[1] = 1.08; cv.dpi = 300; cv.molarMass = 58.44;
        cv.favorites = {0x0000, 0x0105, 0x0200};
        cv.recent.push_back({0x0001, 2, 3, -1, 14, "1/3"});
        const auto blob = serializeWorkspace(w);
        CHECK(blob[4] == 10 && blob[5] == 0);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
        const auto& b = back.settings.conversion;
        CHECK(b.rates[1] == 1.08 && b.dpi == 300 && b.molarMass == 58.44 && b.favorites.size() == 3 && b.favorites[1] == 0x0105);
        CHECK(b.recent.size() == 1 && b.recent[0].value == "1/3" && b.recent[0].toPrefix == 14 && b.recent[0].fromPrefix == -1);
        CHECK(back.history.size() == 2 && back.strings[0] == "CONV");
        // Record v10 corrotti: tasso negativo, tipo di voce sconosciuto, troncato.
        auto bad = blob;
        const double rate = 1.08, minus = -1;
        std::uint8_t pattern[8]; std::memcpy(pattern, &rate, 8);
        const auto at = std::search(bad.begin(), bad.end(), pattern, pattern + 8);
        CHECK(at != bad.end());
        if (at != bad.end()) std::memcpy(&*at, &minus, 8);
        CHECK(!deserializeWorkspace(bad.data(), bad.size(), back));
        auto bad2 = blob; bad2[bad2.size() - 1] = 3;                                     // ultimo preferito: tipo 3
        CHECK(!deserializeWorkspace(bad2.data(), bad2.size(), back));
        CHECK(!deserializeWorkspace(blob.data(), blob.size() - 1, back));
    }
    // ================================================================ documentazione
    {
        const std::string doc = conversionDoc();
        const std::string path = std::string(FXAI_SOURCE_ROOT) + "/docs/CONVERSION.md";
        if (std::getenv("FXAI_UPDATE_DOCS")) { std::ofstream(path, std::ios::binary) << doc; }
        std::ifstream in(path, std::ios::binary);
        std::stringstream ss; ss << in.rdbuf();
        ::testutil::report(ss.str() == doc, "docs/CONVERSION.md generato dalla tabella", path.c_str(), 0,
                           "rigenerare con FXAI_UPDATE_DOCS=1");
    }
    return testutil::finish("conversion_v114_test");
}
