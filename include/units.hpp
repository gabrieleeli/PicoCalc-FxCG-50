#pragma once
// Unita' di misura (v1.14.0): l'unica fonte dati delle conversioni numeriche.
//
// Una sola tabella in flash (const, nessuna copia in RAM) usata da:
//  - CONVERSION (tutte le categorie, tutte le unita', prefissi SI/IEC);
//  - Run-Matrix OPTN › CONVERT (calc_library.hpp): solo le 103 unita' marcate
//    flag::FxCG50, nelle 11 categorie e nell'ordine del manuale della fx-CG50;
//  - la sintassi valore[unita']▶[unita'] di Run-Matrix (anche le unita' nuove).
//
// Modello di conversione (valore SI = s):
//  - Linear   s = v × factor
//  - Affine   s = (v + offset) × factor        (temperature, taglie)
//  - Inverse  s = factor / v                    (L/100 km, min/km, lunghezza d'onda)
//  - Function s = f(v), v = f⁻¹(s)              (dB, pH, AWG, magnitudo...)
// Le costanti sono quelle esatte del SI 2019 (BIPM) e di NIST SP 811 (2008);
// le costanti fisiche misurate sono CODATA 2018 (le stesse della fx-CG50 per
// l'unita' di massa atomica). Ogni fattore non esatto ha la fonte in units.cpp
// e in docs/CONVERSION.md (generato dal test conversion_v114_test).
#include "exact.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace fx::units {

enum class Kind : std::uint8_t { Linear = 0, Affine = 1, Inverse = 2, Function = 3 };

namespace flag {
constexpr std::uint8_t FxCG50 = 1;      // unita' del menu CONVERT della fx-CG50
constexpr std::uint8_t PrefixSI = 2;    // ammette i prefissi SI (q … Q)
constexpr std::uint8_t PrefixIEC = 4;   // ammette i prefissi binari (Ki … Yi)
constexpr std::uint8_t Approx = 8;      // tabella/formula convenzionale approssimata
constexpr std::uint8_t Param = 16;      // s = v × factor / parametro della categoria (DPI, massa molare)
constexpr std::uint8_t Historic = 64;   // unita' storica (sottocategoria in fondo all'elenco)
constexpr std::uint8_t ExactDecimal = 128;  // il fattore e' esatto ed e' il decimale scritto
} // namespace flag

// Gruppi del primo livello di CONVERSION.
enum class Group : std::uint8_t { BaseSI, Mechanics, Electricity, Thermal, LightRadiation, Chemistry, Data,
                                  Logarithmic, Derived, Formats, Sizes, Currency, Count };
const char* groupName(Group g) noexcept;

enum class Param : std::uint8_t { None = 0, Dpi = 1, MolarMass = 2 };

struct Category {
    const char* name;      // nome italiano
    const char* fxName;    // nome nel menu CONVERT della fx-CG50 (nullptr: categoria nuova)
    const char* si;        // unita' di riferimento (SI o coerente)
    const char* note;      // costanti o formula mostrate a schermo (nullptr: nessuna)
    Group group;
    Param param;
};

struct Unit {
    double factor;          // vedi Kind; per Function e' il parametro della funzione
    const char* name;       // simbolo scritto tra [ ] ("cm", "°C", "kgf/cm²")
    const char* label;      // nome italiano
    const char* exact;      // fattore esatto ("1/3.6", "pi/180") o nullptr
    std::uint8_t cat;       // indice della categoria
    std::uint8_t flags;
    std::uint8_t kindSrc;   // bit 0-3 Kind, bit 4-7 fonte (sourceName)
    std::uint8_t aux;       // Affine: indice dell'offset; Function: funzione
    Kind kind() const noexcept { return static_cast<Kind>(kindSrc & 0x0F); }
    unsigned source() const noexcept { return kindSrc >> 4; }
    bool has(std::uint8_t f) const noexcept { return (flags & f) != 0; }
};

struct Prefix {
    const char* sym;        // "k", "µ", "Ki"
    const char* name;       // "kilo"
    double factor;
    std::int8_t exp;        // potenza di 10 (SI) o di 2 (IEC)
    bool iec;
};

// Un'unita' con l'eventuale prefisso (-1: nessuno).
struct Ref {
    const Unit* unit{nullptr};
    int prefix{-1};
    explicit operator bool() const noexcept { return unit != nullptr; }
};

struct Params {
    double dpi{96};             // pixel: punti per pollice (CSS: 96)
    double molarMass{18.015};   // g/mol (acqua, H2O)
};

std::size_t categoryCount() noexcept;
const Category& category(std::size_t index) noexcept;
std::size_t unitCount() noexcept;
const Unit& unit(std::size_t index) noexcept;
std::size_t indexOf(const Unit& u) noexcept;
// Le unita' di una categoria sono contigue nella tabella: [first, first+count).
void unitsOf(std::size_t cat, std::size_t& first, std::size_t& count) noexcept;
// Nomi delle 11 categorie del menu CONVERT della fx-CG50 (terminati da nullptr):
// sono le prime 11 categorie della tabella, nello stesso ordine.
const char* const* fxCategoryNames() noexcept;
std::size_t prefixCount() noexcept;
const Prefix& prefix(std::size_t index) noexcept;
bool prefixAllowed(const Unit& u, std::size_t prefixIndex) noexcept;
// Offset delle unita' affini (valore e testo esatto, nullptr se non esatto).
double affineOffset(const Unit& u) noexcept;
const char* affineOffsetText(const Unit& u) noexcept;
// Fonte del fattore (indice Unit::source()).
const char* sourceName(unsigned source) noexcept;
std::size_t sourceCount() noexcept;
// Formula di un'unita' Function, per lo schermo e la documentazione.
const char* functionFormula(const Unit& u) noexcept;

// Simbolo mostrato ("k" + "Pa" = "kPa").
std::string symbol(const Ref& r);
// Unita' (con prefisso) dal simbolo, nella categoria data.
Ref find(std::size_t cat, const std::string& sym) noexcept;
// Coppia da/a dai simboli: la prima categoria che le contiene entrambe.
// fxOnly: solo le unita' fx-CG50. runMatrix esclude le unita' con parametro.
// Ritorna 0 se trovata, 1 se i simboli esistono ma in categorie diverse,
// 2 se un simbolo e' sconosciuto.
int resolve(const std::string& from, const std::string& to, Ref& a, Ref& b, bool fxOnly, bool runMatrix) noexcept;
// True se il simbolo (anche con prefisso) e' un'unita' della tabella.
bool known(const std::string& sym, bool fxOnly, bool runMatrix) noexcept;

// Conversioni. Lanciano MathError ("Math ERROR: ...", "Argument ERROR: ...")
// per valori non finiti, fuori dominio o categorie diverse.
double toSI(double v, const Ref& r, const Params& p = {});
double fromSI(double s, const Ref& r, const Params& p = {});
double convert(double v, const Ref& from, const Ref& to, const Params& p = {});
// Formula identica a quella della fx-CG50 (v + oa)·fa/fb − ob per le unita'
// lineari e affini: Run-Matrix ottiene gli stessi risultati bit per bit.
double convertLinear(double v, const Unit& from, const Unit& to);

// Fattore esatto: r × π^pi. False se l'unita' non e' esatta o non e' rappresentabile.
struct ExactFactor { exact::Rational r; int pi{0}; };
bool exactFactor(const Ref& r, ExactFactor& out) noexcept;
// Testo "a*b/c" con decimali e "pi" -> ExactFactor (false se non rappresentabile).
bool parseExactText(const char* text, ExactFactor& out) noexcept;
// Conversione esatta (unita' lineari e affini esatte, senza parametro).
bool convertExact(const exact::Value& v, const Ref& from, const Ref& to, exact::Value& out) noexcept;

// Ricerca: true se la query (ASCII, senza maiuscole) compare nel simbolo o nel nome.
bool matches(const Unit& u, const std::string& lowerQuery) noexcept;
bool matchesText(const char* text, const std::string& lowerQuery) noexcept;

} // namespace fx::units
