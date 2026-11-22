// v1.17.0 — Fuzzing delle formule del foglio (Spreadsheet): righe "CELLA=testo"
// scritte nel foglio, ricalcolo (cicli, intervalli, errori), spostamento dei
// riferimenti, COPY/PASTE/FILL, e la decodifica dei file .pcsheet e dello
// stato. Primo byte: modo. Un file accettato si ricodifica identico.
#include "spreadsheet.hpp"
#include "workspace.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    static Workspace* ws = new Workspace();
    if (n < 1) return 0;
    const std::uint8_t mode = d[0] % 3;
    if (mode == 1) {
        sheet::Sheet s;
        if (!sheet::decodeFile(d + 1, n - 1, s)) return 0;
        const auto again = sheet::encodeFile(s);
        sheet::Sheet t;
        if (!sheet::decodeFile(again.data(), again.size(), t) || !(t == s)) { std::fprintf(stderr, "ROUNDTRIP\n"); std::abort(); }
        try { sheet::recalculate(s, ws->evalContext()); } catch (const std::bad_alloc&) {}
        return 0;
    }
    if (mode == 2) {
        sheet::State st;
        if (!sheet::decode(st, d + 1, n - 1)) return 0;
        std::vector<std::uint8_t> out;
        sheet::encode(st, out);
        sheet::State back;
        if (!sheet::decode(back, out.data(), out.size()) || !(back == st)) { std::fprintf(stderr, "ROUNDTRIP\n"); std::abort(); }
        return 0;
    }
    // Righe "REF=testo"; una riga "!dc,dr" sposta e incolla il blocco A1:C3.
    const std::string text(reinterpret_cast<const char*>(d + 1), n - 1);
    sheet::Sheet s;
    std::size_t p = 0;
    int lines = 0;
    while (p < text.size() && lines++ < 64) {
        std::size_t e = text.find('\n', p);
        if (e == std::string::npos) e = text.size();
        const std::string line = text.substr(p, e - p);
        p = e + 1;
        const std::size_t eq = line.find('=');
        try {
            if (!line.empty() && line[0] == '!') {
                sheet::Clipboard c;
                sheet::copyBlock(s, sheet::Block{sheet::Ref{0, 1}, sheet::Ref{2, 3}}, line.size() > 1 && line[1] == 'x', c);
                const int dc = line.size() > 2 ? (line[2] & 7) : 1, dr = line.size() > 3 ? (line[3] & 15) : 1;
                sheet::paste(s, c, sheet::Ref{dc, 1 + dr});
                continue;
            }
            if (!line.empty() && line[0] == '#') { sheet::fill(s, sheet::Block{sheet::Ref{0, 1}, sheet::Ref{3, 4}}, line.substr(1)); continue; }
            if (eq == std::string::npos) continue;
            sheet::Ref r;
            if (!sheet::parseRef(line.substr(0, eq), r)) continue;
            s.set(r, line.substr(eq + 1));
            std::string shifted;
            const sheet::Cell* c = s.cell(r);
            if (c && c->kind == sheet::Kind::Formula) {
                (void)sheet::shiftFormula(c->input, (line.size() & 3) - 1, static_cast<int>(line.size() % 5) - 2, shifted);
                (void)sheet::formulaReferences(c->input);
            }
        } catch (const std::bad_alloc&) {
            return 0;
        } catch (const std::exception&) {
        }
    }
    try {
        sheet::Recalc task(s, ws->evalContext());
        step::Budget b(7, 0, nullptr);
        (void)task.step(b);
        task.runAll();
        (void)sheet::columnValues(s, 0, 1, sheet::Rows);
    } catch (const std::bad_alloc&) {
    } catch (const std::exception&) {
    }
    return 0;
}
