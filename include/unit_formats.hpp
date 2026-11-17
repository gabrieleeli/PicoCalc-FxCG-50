#pragma once
// Conversioni di formato di CONVERSION (v1.14.0): testo -> testo con
// validazione dell'input (basi numeriche, complemento a due, BCD, Gray,
// IEEE-754, numeri romani, Unicode, DMS/HMS, tempo, date, coordinate,
// complessi, notazioni, colori, tassi APR/EFF, misure degli anelli).
//
// Ogni convertitore ha un elenco di rappresentazioni: il testo nella
// rappresentazione "da" viene letto (Syntax ERROR se malformato, Argument
// ERROR se fuori intervallo) e scritto nella rappresentazione "a". Riusa le
// funzioni di Run-Matrix: Base-N (toBase/fromBase), DMS, Cnvt_EFF/Cnvt_APR,
// Days_Prd, frazioni e periodi esatti (exact.hpp).
// Solo calcolo: nessuna interfaccia, nessun accesso al workspace.
#include "math_engine.hpp"
#include "number_format.hpp"
#include "units.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace fx::unitfmt {

struct Converter {
    const char* name;            // "Basi numeriche"
    units::Group group;          // Formati o Taglie
    const char* const* reps;     // rappresentazioni, terminate da nullptr
    std::uint32_t inputMask;     // bit i: la rappresentazione i si puo' leggere
    const char* paramName;       // nullptr: nessun parametro
    double paramDefault, paramMin, paramMax;
    const char* note;            // mostrata a schermo (nullptr: nessuna)
};

struct Options {
    double param{0};                         // base, bit, capitalizzazioni (Converter::paramName)
    AngleMode angle{AngleMode::Degrees};     // angoli di coordinate e complessi
    DisplayFormat display{};                 // numeri decimali (SET UP Display)
};

std::size_t converterCount() noexcept;
const Converter& converter(std::size_t index) noexcept;
std::size_t repCount(const Converter& c) noexcept;
bool readable(const Converter& c, std::size_t rep) noexcept;

// Converte `input` dalla rappresentazione `from` alla `to`.
std::string convert(std::size_t conv, std::size_t from, std::size_t to, const std::string& input, const Options& opt);
// Colore RGB565 del testo (convertitore dei colori), per l'anteprima a schermo.
bool colorOf(std::size_t conv, std::size_t from, const std::string& input, const Options& opt, std::uint16_t& rgb565);
// Indice del convertitore dei colori (per l'anteprima).
std::size_t colorConverter() noexcept;

} // namespace fx::unitfmt
