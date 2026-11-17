// v1.13.1 — // Fuzzing del lettore CSV e del caricamento in liste e matrici. Primo byte:
// separatore, simbolo decimale, lista di partenza, liste o matrice.
#include "csv_io.hpp"
#include "workspace.hpp"
#include "expression.hpp"
#include <cstdint>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 1) return 0;
    csv::Format f;
    f.separator = (d[0] & 1) ? ';' : ',';
    f.decimal = (d[0] & 2) ? ',' : '.';
    if (f.separator == f.decimal) f.decimal = '.';
    try {
        auto t = csv::decode(d + 1, n - 1, f, 999, 26);
        Workspace ws;
        try { csv::loadLists(ws, (d[0] >> 2) % 26, t); } catch (const std::exception&) {}
        try { csv::loadMatrix(ws, (d[0] >> 7) & 1, 0, t); } catch (const std::exception&) {}
        try { auto s = csv::encodeLists(ws); (void)s; } catch (const std::exception&) {}
    } catch (const std::exception&) {}
    return 0;
}
