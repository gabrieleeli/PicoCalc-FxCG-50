// v1.14.0 — Fuzzing delle conversioni di CONVERSION: i parser di testo di
// unit_formats.hpp (basi, IEEE-754, romani, Unicode, DMS, date, coordinate,
// complessi, notazioni, colori, anelli) e la ricerca dei simboli di units.hpp
// (prefissi compresi). Byte 0: convertitore; byte 1: rappresentazioni da/a;
// byte 2: parametro; il resto e' il testo. Solo eccezioni attese, nessun crash.
#include "unit_formats.hpp"
#include "units.hpp"
#include "calc_library.hpp"
#include "expression.hpp"
#include <cstdint>
#include <string>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 3) return 0;
    const std::string text(reinterpret_cast<const char*>(d + 3), n - 3);
    const std::size_t conv = d[0] % unitfmt::converterCount();
    const std::size_t reps = unitfmt::repCount(unitfmt::converter(conv));
    unitfmt::Options o;
    static const double params[] = {2, 8, 10, 12, 16, 32, 36, 64, 1, 0, 365, 1e6, -1, 3.5};
    o.param = params[d[2] % (sizeof params / sizeof params[0])];
    o.angle = static_cast<AngleMode>(d[2] % 3);
    try { (void)unitfmt::convert(conv, (d[1] & 0x0F) % reps, (d[1] >> 4) % reps, text, o); } catch (const std::exception&) {}
    std::uint16_t rgb = 0;
    (void)unitfmt::colorOf(conv, (d[1] & 0x0F) % reps, text, o, rgb);
    // Simboli di unita' (con prefisso) e conversione tra due simboli come in Run-Matrix.
    const std::size_t cut = text.find(' ');
    const std::string a = text.substr(0, cut), b = cut == std::string::npos ? std::string("m") : text.substr(cut + 1);
    (void)units::known(a, false, true);
    try { (void)calclib::convertUnitNames(static_cast<double>(d[2]) - 100.0, a, b); } catch (const std::exception&) {}
    return 0;
}
