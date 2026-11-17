// v1.14.0 — catalogo delle costanti (include/physical_constants.hpp): valori
// confrontati con la tabella NIST CODATA 2022, coerenza delle derivate esatte,
// nomi del parser (nuovi e della v1.13), collisioni e lettura a pezzi del
// tokenizzatore, glifi (font, editor, cronologia, formatNatural), point_a,
// esempi di SYMBOLS e menu OPTN › CONST di RUN-MATRIX.
#include "app_rig.hpp"
#include "calc_library.hpp"
#include "cheatsheet.hpp"
#include "expression.hpp"
#include "font_renderer.hpp"
#include "math_renderer.hpp"
#include "memory.hpp"
#include "natural_editor.hpp"
#include "physical_constants.hpp"
#include "point_a.hpp"
#include "test_util.hpp"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace fx;
using physconst::ConstDef;

namespace {
constexpr double kPi = 3.14159265358979323846;
const ConstDef& K(const char* name) {
    const ConstDef* k = physconst::findConstant(name);
    if (!k) { std::fprintf(stderr, "costante mancante: %s\n", name); std::abort(); }
    return *k;
}
double V(const char* name) { return K(name).value; }
bool rel(double a, double b, double tol) { return std::abs(a - b) <= tol * std::abs(b); }
#define CHECK_REL(a, b, tol) do { const double ra_ = (a), rb_ = (b); \
    ::testutil::report(rel(ra_, rb_, tol), #a " ~ " #b, __FILE__, __LINE__, \
    "(" + std::to_string(ra_) + " vs " + std::to_string(rb_) + ", rel " + std::to_string(std::abs(ra_ - rb_) / std::abs(rb_)) + ")"); } while (0)

std::vector<std::string> namesOf(const ConstDef& k) {
    std::vector<std::string> out{k.parserName};
    physconst::forEachAlias(k, [&](std::string_view a) { out.emplace_back(a); });
    return out;
}
bool drawable(font::Face face, const char* s) {
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    while (*p) if (!font::hasGlyph(face, font::nextCodepoint(p))) return false;
    return true;
}
std::string upper(std::string s) { for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); return s; }
// Identificatori letti dal lexer del parser (in ordine).
std::vector<std::string> identifiers(const std::string& expr) {
    std::vector<std::string> out;
    rewriteIdentifiers(expr, [&](const std::string& id) { out.push_back(id); return std::string(); });
    return out;
}
std::string join(const std::vector<std::string>& v) { std::string s; for (const auto& x : v) s += (s.empty() ? "" : " ") + x; return s; }

// Righe della tabella NIST "CODATA 2022 recommended values" (allascii.txt,
// physics.nist.gov/cuu/Constants/Table/allascii.txt), testo originale senza spazi.
struct Nist { const char* name; const char* quantity; const char* value; const char* uncertainty; bool truncated; };
const Nist kNist[] = {
    {"physc", "speed of light in vacuum", "299792458", "", false},
    {"physmu0", "vacuum mag. permeability", "1.25663706127e-6", "0.00000000020e-6", false},
    {"physeps0", "vacuum electric permittivity", "8.8541878188e-12", "0.0000000014e-12", false},
    {"physz0", "characteristic impedance of vacuum", "376.730313412", "0.000000059", false},
    {"physg", "Newtonian constant of gravitation", "6.67430e-11", "0.00015e-11", false},
    {"physh", "Planck constant", "6.62607015e-34", "", false},
    {"physhbar", "reduced Planck constant", "1.054571817e-34", "", true},
    {"physplmass", "Planck mass", "2.176434e-8", "0.000024e-8", false},
    {"physpllen", "Planck length", "1.616255e-35", "0.000018e-35", false},
    {"physpltime", "Planck time", "5.391247e-44", "0.000060e-44", false},
    {"physpltemp", "Planck temperature", "1.416784e32", "0.000016e32", false},
    {"qe", "elementary charge", "1.602176634e-19", "", false},
    {"physphi0", "mag. flux quantum", "2.067833848e-15", "", true},
    {"physgcond", "conductance quantum", "7.748091729e-5", "", true},
    {"physkj", "Josephson constant", "483597.8484e9", "", true},
    {"physrk", "von Klitzing constant", "25812.80745", "", true},
    {"physmub", "Bohr magneton", "9.2740100657e-24", "0.0000000029e-24", false},
    {"physmunuc", "nuclear magneton", "5.0507837393e-27", "0.0000000016e-27", false},
    {"physalpha", "fine-structure constant", "7.2973525643e-3", "0.0000000011e-3", false},
    {"physinvalpha", "inverse fine-structure constant", "137.035999177", "0.000000021", false},
    {"physrinf", "Rydberg constant", "10973731.568157", "0.000012", false},
    {"physa0", "Bohr radius", "5.29177210544e-11", "0.00000000082e-11", false},
    {"physeh", "Hartree energy", "4.3597447222060e-18", "0.0000000000048e-18", false},
    {"physme", "electron mass", "9.1093837139e-31", "0.0000000028e-31", false},
    {"physmmu", "muon mass", "1.883531627e-28", "0.000000042e-28", false},
    {"physmp", "proton mass", "1.67262192595e-27", "0.00000000052e-27", false},
    {"physmn", "neutron mass", "1.67492750056e-27", "0.00000000085e-27", false},
    {"physmd", "deuteron mass", "3.3435837768e-27", "0.0000000010e-27", false},
    {"physmalpha", "alpha particle mass", "6.6446573450e-27", "0.0000000021e-27", false},
    {"physmratio", "proton-electron mass ratio", "1836.152673426", "0.000000032", false},
    {"physre", "classical electron radius", "2.8179403205e-15", "0.0000000013e-15", false},
    {"physlambdac", "Compton wavelength", "2.42631023538e-12", "0.00000000076e-12", false},
    {"physthomson", "Thomson cross section", "6.6524587051e-29", "0.0000000062e-29", false},
    {"physmue", "electron mag. mom.", "-9.2847646917e-24", "0.0000000029e-24", false},
    {"physmup", "proton mag. mom.", "1.41060679545e-26", "0.00000000060e-26", false},
    {"physmuneu", "neutron mag. mom.", "-9.6623653e-27", "0.0000023e-27", false},
    {"physgammap", "proton gyromag. ratio", "2.6752218708e8", "0.0000000011e8", false},
    {"na", "Avogadro constant", "6.02214076e23", "", false},
    {"kb", "Boltzmann constant", "1.380649e-23", "", false},
    {"physrgas", "molar gas constant", "8.314462618", "", true},
    {"physf", "Faraday constant", "96485.33212", "", true},
    {"physu", "atomic mass constant", "1.66053906892e-27", "0.00000000052e-27", false},
    {"physmolu", "molar mass constant", "1.00000000105e-3", "0.00000000031e-3", false},
    {"physsigma", "Stefan-Boltzmann constant", "5.670374419e-8", "", true},
    {"physb", "Wien wavelength displacement law constant", "2.897771955e-3", "", true},
    {"physrad1", "first radiation constant", "3.741771852e-16", "", true},
    {"physrad2", "second radiation constant", "1.438776877e-2", "", true},
    {"physvmatm", "molar volume of ideal gas (273.15 K, 101.325 kPa)", "22.41396954e-3", "", true},
    {"physvmbar", "molar volume of ideal gas (273.15 K, 100 kPa)", "22.71095464e-3", "", true},
    {"physn0", "Loschmidt constant (273.15 K, 101.325 kPa)", "2.686780111e25", "", true},
    {"g0", "standard acceleration of gravity", "9.80665", "", false},
    {"physatm", "standard atmosphere", "101325", "", false},
    {"physdnucs", "hyperfine transition frequency of Cs-133", "9192631770", "", false},
    {"physkcd", "luminous efficacy", "683", "", false},
};
// Nomi di costante accettati dal parser fino alla v1.13.1 (devono restare invariati).
const char* const kOldNames[] = {"pi", "tau", "phi", "goldenratio", "eulergamma", "sqrt2", "ln2", "ln10", "speedoflight", "physc",
    "gravitationalconstant", "physg", "planckconstant", "physh", "avogadro", "na", "boltzmann", "kb", "elementarycharge", "qe",
    "standardgravity", "g0"};
bool oldName(const std::string& n) { for (const char* o : kOldNames) if (n == o) return true; return false; }
} // namespace

int main() {
    board::init();
    const auto all = physconst::allConstants();
    const auto groups = physconst::constGroups();
    // ================================================================ struttura della tabella
    {
        CHECK(groups.size() == 7);
        CHECK(all.size() == 73);
        std::size_t total = 0;
        for (std::size_t g = 0; g < groups.size(); ++g) {
            const auto part = physconst::constantsOf(g);
            CHECK(!part.empty());
            for (const auto& k : part) CHECK(k.group == g);
            total += part.size();
            CHECK(groups[g].name && groups[g].name[0] && groups[g].title && groups[g].title[0] && groups[g].example && groups[g].example[0]);
        }
        CHECK(total == all.size());
        CHECK(physconst::constantsOf(groups.size()).empty());
        CHECK(physconst::constantsOf(0).size() == 8 && physconst::constantsOf(1).size() == 11 && physconst::constantsOf(2).size() == 7 &&
              physconst::constantsOf(3).size() == 19 && physconst::constantsOf(4).size() == 13 && physconst::constantsOf(5).size() == 5 &&
              physconst::constantsOf(6).size() == 10);
        std::set<std::string> seen;
        for (const auto& k : all) {
            CHECK(k.glyph && k.glyph[0] && k.nameIT && k.nameIT[0] && k.unit && k.altNames);
            CHECK(std::isfinite(k.value) && k.relUncertainty >= 0 && k.relUncertainty < 1e-3);
            // parserName: minuscolo ASCII, lettere e cifre; nuovi nomi "phys" + sigla.
            const std::string p = k.parserName;
            bool ascii = !p.empty() && std::islower(static_cast<unsigned char>(p[0]));
            for (char c : p) ascii = ascii && (std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)));
            CHECK(ascii);
            CHECK(oldName(p) || p == "e" || p.compare(0, 4, "phys") == 0);
            for (const auto& n : namesOf(k)) {
                bool lowerAlpha = !n.empty();
                for (char c : n) lowerAlpha = lowerAlpha && (std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)));
                CHECK(lowerAlpha);
                CHECK(seen.insert(n).second);                                 // nessun nome duplicato
                CHECK(physconst::findConstant(n) == &k);
                CHECK(physconst::findConstantNoCase(upper(n)) == &k);
            }
        }
        CHECK(physconst::findConstant("") == nullptr && physconst::findConstant("physxyz") == nullptr && physconst::findConstant("PHYSC") == nullptr);
        CHECK(physconst::findByGlyph("c") == &K("physc") && physconst::findByGlyph("m\xE2\x82\x91") == &K("physme") && physconst::findByGlyph("?") == nullptr);
    }
    // ================================================================ valori: tabella NIST CODATA 2022
    for (const auto& n : kNist) {
        const ConstDef* k = physconst::findConstant(n.name);
        CHECK(k != nullptr);
        if (!k) continue;
        const double nist = std::strtod(n.value, nullptr);
        if (n.truncated) CHECK_REL(k->value, nist, 1e-9);                     // NIST: "1.054 571 817..."
        else { CHECK(k->value == nist); if (k->value != nist) std::fprintf(stderr, "  %s: %.17g vs %s\n", n.name, k->value, n.value); }
        if (!n.uncertainty[0]) CHECK(physconst::isExact(*k));
        else {
            const double ur = std::strtod(n.uncertainty, nullptr) / std::abs(nist);   // incertezza relativa NIST
            CHECK(!physconst::isExact(*k));
            CHECK_REL(k->relUncertainty, ur, 0.05);                           // 2 cifre significative
        }
    }
    // ================================================================ costanti esatte: valore definito
    {
        CHECK(V("physc") == 299792458.0 && V("physh") == 6.62607015e-34 && V("qe") == 1.602176634e-19 && V("kb") == 1.380649e-23);
        CHECK(V("na") == 6.02214076e23 && V("physdnucs") == 9192631770.0 && V("physkcd") == 683.0);
        CHECK(V("g0") == 9.80665 && V("physatm") == 101325.0 && V("physt0") == 273.15 && V("physau") == 149597870700.0);
        CHECK(V("physly") == 9460730472580800.0);                            // c * 365.25 d (IAU)
        for (const char* name : {"physc", "physh", "physhbar", "qe", "physphi0", "physgcond", "physkj", "physrk", "na", "kb", "physrgas", "physf",
                                 "physsigma", "physb", "physrad1", "physrad2", "physvmatm", "physvmbar", "physn0", "g0", "physatm",
                                 "physdnucs", "physkcd", "physt0", "physau", "physly", "physpc"})
            CHECK(physconst::isExact(K(name)));
        for (const auto& k : physconst::constantsOf(0)) CHECK(physconst::isExact(k));
    }
    // ================================================================ derivate esatte: coerenza 1e-12
    {
        const double h = V("physh"), c = V("physc"), e = V("qe"), kB = V("kb"), NA = V("na");
        CHECK_REL(V("physhbar"), h / (2 * kPi), 1e-12);
        CHECK_REL(V("physrgas"), NA * kB, 1e-12);
        CHECK_REL(V("physf"), NA * e, 1e-12);
        CHECK_REL(V("physphi0"), h / (2 * e), 1e-12);
        CHECK_REL(V("physgcond"), 2 * e * e / h, 1e-12);
        CHECK_REL(V("physkj"), 2 * e / h, 1e-12);
        CHECK_REL(V("physrk"), h / (e * e), 1e-12);
        CHECK_REL(V("physsigma"), 2 * std::pow(kPi, 5) * std::pow(kB, 4) / (15 * std::pow(h, 3) * c * c), 1e-12);
        CHECK_REL(V("physrad1"), 2 * kPi * h * c * c, 1e-12);
        CHECK_REL(V("physrad2"), h * c / kB, 1e-12);
        double x = 5;                                                         // Wien: x = 5(1 - e^-x)
        for (int i = 0; i < 50; ++i) x -= ((x - 5) * std::exp(x) + 5) / ((x - 4) * std::exp(x));
        CHECK_REL(V("physb"), h * c / (kB * x), 1e-12);
        CHECK_REL(V("physvmatm"), NA * kB * 273.15 / 101325, 1e-12);
        CHECK_REL(V("physvmbar"), NA * kB * 273.15 / 100000, 1e-12);
        CHECK_REL(V("physn0"), 101325 / (kB * 273.15), 1e-12);
        CHECK_REL(V("physpc"), V("physau") * 648000 / kPi, 1e-15);
        CHECK_REL(V("tau"), 2 * V("pi"), 1e-15);
        // Misurate: relazioni entro le incertezze CODATA
        CHECK_REL(V("physeps0"), 1 / (V("physmu0") * c * c), 1e-9);
        CHECK_REL(V("physz0"), V("physmu0") * c, 1e-9);
        CHECK_REL(V("physalpha"), e * e / (4 * kPi * V("physeps0") * V("physhbar") * c), 1e-9);
        CHECK_REL(V("physinvalpha"), 1 / V("physalpha"), 1e-9);
        CHECK_REL(V("physmratio"), V("physmp") / V("physme"), 1e-9);
        CHECK_REL(V("physeh"), 2 * V("physrinf") * h * c, 1e-9);
        CHECK_REL(V("physa0"), V("physhbar") / (V("physme") * c * V("physalpha")), 1e-9);
        CHECK_REL(V("physre"), V("physalpha") * V("physalpha") * V("physa0"), 1e-9);
        CHECK_REL(V("physu"), V("physmolu") / NA, 1e-9);
        // Astronomiche: IAU 2015 B3 (nominali) e masse = GM/G con la G CODATA 2022
        CHECK(V("physgmsun") == 1.3271244e20 && V("physrsun") == 6.957e8 && V("physlsun") == 3.828e26);
        CHECK(V("physgmterra") == 3.986004e14 && V("physrterra") == 6.3781e6);
        CHECK_REL(V("physmsun"), V("physgmsun") / V("physg"), 1e-15);
        CHECK_REL(V("physmterra"), V("physgmterra") / V("physg"), 1e-15);
        CHECK_REL(V("physmterra"), 5.9722e24, 1e-5);
        CHECK_REL(K("physmsun").relUncertainty, K("physg").relUncertainty, 1e-12);
    }
    // ================================================================ parser: ogni nome e alias
    {
        for (const auto& k : all) {
            for (const auto& n : namesOf(k)) {
                double v = 0;
                CHECK_NOTHROW(v = evaluate(n));
                CHECK(v == k.value);
                if (n.size() > 1) { CHECK_NOTHROW(v = evaluate(upper(n))); CHECK(v == k.value); }   // il parser ignora le maiuscole
                CHECK(isReservedWord(n) == (n.size() > 1));
                // Un solo identificatore per il lexer, anche accanto a un numero e a una variabile.
                if (n.size() > 1) {
                    CHECK_EQ_STR(join(identifiers(n)), n);
                    CHECK_EQ_STR(join(identifiers("2" + n + "*x")), n + " x");
                }
            }
        }
        CHECK_REL(evaluate("physme*physc^2"), 8.1871057880e-14, 1e-10);
        CHECK_REL(evaluate("2physme"), 2 * V("physme"), 1e-15);
        CHECK_REL(evaluate("electronmass*speedoflight^2"), 8.1871057880e-14, 1e-10);
        // Nomi della v1.13: stessi valori (anche quelli salvati con maiuscole in file e workspace)
        const struct { const char* name; double value; } old[] = {
            {"physc", 299792458.0}, {"physg", 6.67430e-11}, {"physh", 6.62607015e-34}, {"na", 6.02214076e23}, {"kb", 1.380649e-23},
            {"qe", 1.602176634e-19}, {"g0", 9.80665}, {"speedoflight", 299792458.0}, {"gravitationalconstant", 6.67430e-11},
            {"planckconstant", 6.62607015e-34}, {"avogadro", 6.02214076e23}, {"boltzmann", 1.380649e-23},
            {"elementarycharge", 1.602176634e-19}, {"standardgravity", 9.80665}, {"NA", 6.02214076e23}, {"kB", 1.380649e-23},
            {"speedOfLight", 299792458.0}, {"goldenRatio", 1.6180339887498948482}, {"eulerGamma", 0.5772156649015328606},
            {"pi", kPi}, {"tau", 2 * kPi}, {"phi", 1.6180339887498948482}, {"goldenratio", 1.6180339887498948482},
            {"eulergamma", 0.5772156649015328606}, {"sqrt2", 1.4142135623730950488}, {"ln2", 0.69314718055994530942},
            {"ln10", 2.30258509299404568402}};
        for (const auto& o : old) {
            double v = 0;
            CHECK_NOTHROW(v = evaluate(o.name));
            CHECK(v == o.value);
            if (v != o.value) std::fprintf(stderr, "  %s: %.17g\n", o.name, v);
        }
        CHECK(evaluate("e") == std::exp(1.0));
        // "E" resta la variabile, "e" il numero di Nepero
        CalculatorMemory mem;
        mem.store('E', 7);
        mem.store('T', 300);
        mem.store('F', 5e14);
        mem.store('X', 3);
        CHECK(evaluate("E", mem) == 7 && evaluate("e", mem) == std::exp(1.0));
        // Il tokenizzatore prende il pezzo piu' lungo: le espressioni della v1.13 non cambiano lettura.
        CHECK_REL(evaluate("physh*x", mem, 2.0), 2 * 6.62607015e-34, 1e-15);
        CHECK_REL(evaluate("na*2", mem), 2 * 6.02214076e23, 1e-15);
        CHECK_REL(evaluate("kbT", mem), 1.380649e-23 * 300, 1e-15);           // k_B·T, non un nome nuovo
        CHECK_REL(evaluate("physhF", mem), 6.62607015e-34 * 5e14, 1e-15);    // h·F
        CHECK_REL(evaluate("physc2", mem), 2 * 299792458.0, 1e-15);          // c·2
        CHECK(evaluate("physg0", mem) == 0);                                  // G·0 (la nuova G₀ e' physgcond)
        CHECK_REL(evaluate("g0^2", mem), 9.80665 * 9.80665, 1e-15);
        CHECK_EQ_STR(join(identifiers("physhx")), "physh x");
        CHECK_EQ_STR(join(identifiers("kbT")), "kb T");                     // la variabile T resta maiuscola
        CHECK_EQ_STR(join(identifiers("nax")), "na x");
        CHECK_EQ_STR(join(identifiers("physmex")), "physme x");
        CHECK_EQ_STR(join(identifiers("physc2")), "physc");
        CHECK_EQ_STR(join(identifiers("2physa0x")), "physa0 x");
    }
    // ================================================================ collisioni e prefissi
    {
        for (const auto& k : all) {
            for (const auto& n : namesOf(k)) {
                // Nessun nome e' una funzione, un operatore, ans/preans/theta/r_var, una variabile di sistema...
                CHECK(!isNonConstantName(n));
                if (isNonConstantName(n)) std::fprintf(stderr, "  collisione: %s\n", n.c_str());
                // ...ne' una variabile A-Z/θ/r (lettere singole): solo "e" (Nepero, come prima) ha una lettera.
                CHECK(n.size() > 1 || n == "e");
                CHECK(n != "theta" && n != "r_var" && n != "ans" && n != "preans");
                if (oldName(n) || n == "e") continue;
                // Un nome nuovo non allunga un nome gia' riservato (funzione, costante della v1.13)
                // con cifre o con una sola lettera: "physc2" = c·2, "kbT" = k_B·T restano letture valide.
                for (std::size_t len = 2; len < n.size(); ++len) {
                    const std::string prefix = n.substr(0, len), rest = n.substr(len);
                    if (!oldName(prefix) && !isNonConstantName(prefix)) continue;
                    const bool twoLetters = rest.size() >= 2 && std::isalpha(static_cast<unsigned char>(rest[0])) &&
                                            std::isalpha(static_cast<unsigned char>(rest[1]));
                    CHECK(twoLetters);
                    if (!twoLetters) std::fprintf(stderr, "  %s allunga %s\n", n.c_str(), prefix.c_str());
                }
                // Nessun nome nuovo e' l'inizio di un altro nome della tabella (lettura a pezzi univoca).
                for (const auto& other : all)
                    for (const auto& m : namesOf(other))
                        if (m != n && m.size() > n.size()) {
                            CHECK(m.compare(0, n.size(), n) != 0);
                            if (m.compare(0, n.size(), n) == 0) std::fprintf(stderr, "  %s e' l'inizio di %s\n", n.c_str(), m.c_str());
                        }
            }
        }
    }
    // ================================================================ glifi: font, renderer, editor, cronologia
    {
        for (const auto& k : all) {
            for (font::Face face : {font::Face::Small, font::Face::Medium}) {
                CHECK(drawable(face, k.glyph));
                CHECK(drawable(face, k.nameIT));
                CHECK(drawable(face, k.unit));
            }
            for (const auto& n : namesOf(k)) {
                if (n.size() < 2) continue;
                CHECK(calclib::displayName(n) != nullptr && std::strcmp(calclib::displayName(n), k.glyph) == 0);
                CHECK_EQ_STR(math::displayToken(n), k.glyph);
                CHECK_EQ_STR(formatNatural(n), k.glyph);
                // Cronologia: il testo salvato torna un solo token (mostrato con il glifo) e non cambia.
                NaturalEditor ed;
                ed.setExpression("2" + n + "+1");
                CHECK_EQ_STR(ed.toExpression(), "2" + n + "+1");
                const auto& row = ed.root().children;
                bool single = false;
                for (const auto& ch : row) single = single || (ch && ch->text == n);
                const bool functionPrefixed = n == "sqrt2" || n == "ln2" || n == "ln10";   // scritti come prima (ln 2, √ 2)
                CHECK(single || functionPrefixed);
            }
        }
        CHECK(calclib::displayName("e") == nullptr);                           // la lettera "e" resta "e"
        CHECK_EQ_STR(formatNatural("E+e"), "E+e");
        CHECK_EQ_STR(formatNatural("speedOfLight+gravitationalConstant+planckConstant+avogadro+boltzmann+elementaryCharge+standardGravity+NA+kB+qe+g0"),
                     "c+G+h+N\xE2\x82\x90+k_B+q\xE2\x82\x91+g\xE2\x82\x80+N\xE2\x82\x90+k_B+q\xE2\x82\x91+g\xE2\x82\x80");
        CHECK_EQ_STR(formatNatural("goldenRatio+eulerGamma+sqrt2+ln2+ln10+tau"), "\xCF\x86+\xCE\xB3+\xE2\x88\x9A" "2+ln(2)+ln(10)+\xCF\x84");
        // Scrittura a mano nell'editor: le lettere (e le cifre finali) diventano un token
        {
            NaturalEditor ed;
            for (char c : std::string("physa0")) ed.insert(c);
            CHECK(ed.root().children.size() == 1 && ed.root().children[0]->text == "physa0");
            for (char c : std::string("*g0")) ed.insert(c);
            CHECK(ed.root().children.size() == 3 && ed.root().children[2]->text == "g0");
            for (char c : std::string("+physhbar")) ed.insert(c);
            CHECK(ed.root().children.back()->text == "physhbar");
            CHECK_EQ_STR(ed.toExpression(), "physa0*g0+physhbar");
            NaturalEditor line;
            line.setLinear(true);
            for (char c : std::string("ln2")) line.insert(c);                 // come prima: "ln" "2"
            CHECK(line.root().children.size() == 2);
        }
        // v1.13.1: "g0^2" richiamato dalla cronologia diventava g(0)^(2) = 0
        {
            NaturalEditor ed;
            ed.setExpression("g0^2");
            CHECK_REL(evaluate(ed.toExpression()), 9.80665 * 9.80665, 1e-15);
        }
    }
    // ================================================================ point_a::physicalConstant
    {
        using pointa::physicalConstant;
        CHECK(physicalConstant("pi") == kPi && physicalConstant("PI") == kPi && physicalConstant("e") == std::exp(1.0));
        CHECK(physicalConstant("c") == 299792458.0 && physicalConstant("C") == 299792458.0 && physicalConstant("G") == 6.67430e-11);
        CHECK(physicalConstant("h") == 6.62607015e-34 && physicalConstant("NA") == 6.02214076e23 && physicalConstant("kB") == 1.380649e-23);
        CHECK(physicalConstant("qe") == 1.602176634e-19 && physicalConstant("g0") == 9.80665);
        CHECK(physicalConstant("physme") == V("physme") && physicalConstant("electronMass") == V("physme"));
        CHECK(physicalConstant("m\xE2\x82\x91") == V("physme") && physicalConstant("\xC4\xA7") == V("physhbar"));
        CHECK_THROWS(physicalConstant("g"));
        CHECK_THROWS(physicalConstant("nonesiste"));
        CHECK_THROWS(physicalConstant(""));
    }
    // ================================================================ esempi dei gruppi (SYMBOLS)
    for (const auto& g : groups) {
        const std::string ex = g.example;
        std::size_t at = ex.find(" \xE2\x89\x88 ");
        std::size_t skip = 5;
        if (at == std::string::npos) { at = ex.find(" = "); skip = 3; }
        CHECK(at != std::string::npos);
        if (at == std::string::npos) continue;
        const std::string expr = ex.substr(0, at), shown = ex.substr(at + skip);
        char* end = nullptr;
        const double expected = std::strtod(shown.c_str(), &end);
        CHECK(end != shown.c_str());
        // Il valore mostrato ha 4 cifre significative ("≈") o e' esatto ("=").
        double v = 0;
        CHECK_NOTHROW(v = evaluate(expr));
        CHECK_REL(v, expected, skip == 5 ? 5e-4 : 1e-12);
    }
    // ================================================================ RUN-MATRIX: OPTN › CONST
    {
        testrig::Rig r;
        auto& ws = r.ws();
        r.key(pckey::Tab); r.type("1");                                        // Run-Matrix
        CHECK(r.active<app::RunMatrixApp>() != nullptr);
        for (std::size_t g = 0; g < groups.size(); ++g) {
            const auto part = physconst::constantsOf(g);
            for (std::size_t i = 0; i < part.size(); ++i) {
                r.key(pckey::F8);                                               // OPTN
                r.key(pckey::Up);                                               // CONST: ultima voce
                r.key(pckey::Enter);
                for (std::size_t k = 0; k < g; ++k) r.key(pckey::Down);       // gruppo
                r.key(pckey::Enter);
                for (std::size_t k = 0; k < i; ++k) r.key(pckey::Down);       // costante
                r.key(pckey::Enter);
                r.key(pckey::Enter);                                            // EXE
                CHECK(!r.error());
                if (r.error()) { std::fprintf(stderr, "  CONST %s: %s\n", part[i].parserName, r.errorDetail().c_str()); r.closeError(); continue; }
                CHECK(!ws.history.empty() && ws.history.back().error.empty());
                CHECK_EQ_STR(ws.history.back().expression, part[i].parserName);
                CHECK(std::abs(ws.history.back().value.real() - part[i].value) <= 1e-12 * std::abs(part[i].value));
            }
        }
        // Le voci mostrano glifo e nome: il titolo del sottomenu e' il nome del gruppo.
        CHECK_EQ_STR(math::displayToken("physmsun"), "M\xE2\x98\x89");
    }
    // ================================================================ SYMBOLS: pagine delle costanti che scorrono
    {
        testrig::Rig r;
        r.key(pckey::Tab); r.type("N");
        auto* s = r.active<app::SymbolsApp>();
        CHECK(s != nullptr);
        if (s) {
            for (int i = 0; i < 6; ++i) r.key(pckey::Right);                   // pagina 7: ATOMICHE E NUCLEARI
            CHECK(s->page() == 6);
            CHECK_EQ_STR(help::pageTitle(help::pages[s->page()]), "COSTANTI FISICHE: ATOMICHE E NUCLEARI");
            for (int i = 0; i < 10; ++i) r.key(pckey::Down);
            CHECK(s->selected() == 10 && s->firstVisible() == 5);
            for (int i = 0; i < 11; ++i) r.key(pckey::Up);                     // dalla prima voce si torna all'ultima
            CHECK(s->selected() == 18 && s->firstVisible() == 13);
            r.key(pckey::Right);
            CHECK(s->page() == 7 && s->selected() == 0 && s->firstVisible() == 0);
            r.key(pckey::Esc);                                                  // EXIT: menu principale
            CHECK(r.active<app::SymbolsApp>() == nullptr);
        }
    }
    return testutil::finish("physical_constants_test");
}
