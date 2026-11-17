#include "cheatsheet.hpp"
#include "font_renderer.hpp"
#include "physical_constants.hpp"
#include "test_util.hpp"
#undef assert
#define assert(x) CHECK(x)
#include <cstring>
#include <string>
namespace {
bool same(const char* a, const char* b) { return a && b && std::strcmp(a, b) == 0; }
// Ogni carattere del testo ha un glifo nella faccia (niente '?' sullo schermo).
bool drawable(font::Face face, const char* s) {
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    while (*p) {
        const std::uint32_t cp = font::nextCodepoint(p);
        if (!font::hasGlyph(face, cp)) return false;
    }
    return true;
}
} // namespace
int main() {
    using namespace fx::help;
    // v1.14.0: dopo LETTERE GRECHE le 7 pagine delle costanti (un gruppo della tabella ciascuna).
    static_assert(PageCount == 23);
    assert(same(pageTitle(pages[22]), "CONFRONTI E FRAZIONI"));
    assert(same(pageTitle(pages[0]), "ARITMETICA"));
    assert(same(pageTitle(pages[2]), "LETTERE GRECHE"));
    assert(same(pageTitle(pages[3]), "COSTANTI MATEMATICHE"));
    assert(same(pageTitle(pages[4]), "COSTANTI FISICHE: UNIVERSALI"));
    assert(same(pageTitle(pages[5]), "COSTANTI FISICHE: ELETTROMAGNETICHE"));
    assert(same(pageTitle(pages[6]), "COSTANTI FISICHE: ATOMICHE E NUCLEARI"));
    assert(same(pageTitle(pages[7]), "COSTANTI FISICHE: FISICO-CHIMICHE"));
    assert(same(pageTitle(pages[8]), "COSTANTI FISICHE: VALORI ADOTTATI"));
    assert(same(pageTitle(pages[9]), "COSTANTI ASTRONOMICHE (IAU)"));
    assert(same(pageTitle(pages[16]), "ALTRE FUNZIONI"));
    assert(same(pageTitle(pages[17]), "COMBINATORIA"));
    assert(same(pageTitle(pages[20]), "GRECHE MAIUSCOLE"));
    assert(same(pages[1].entries[5].symbol, "rootN(x)"));
    std::size_t constantPages = 0, constantEntries = 0;
    for (const auto& page : pages) {
        assert(pageTitle(page) != nullptr && pageTitle(page)[0]);
        assert(drawable(font::Face::Medium, pageTitle(page)) && drawable(font::Face::Small, pageTitle(page)));
        const std::size_t n = entryCount(page);
        if (isConstantPage(page)) {
            // Voci dalla tabella: nessuna copia, il numero e' quello del gruppo; scorrono oltre VisibleRows.
            ++constantPages; constantEntries += n;
            assert(page.entries == nullptr && page.count == 0);
            assert(n == fx::physconst::constantsOf(static_cast<std::size_t>(page.constGroup)).size());
            assert(n > 0);
        } else {
            assert(page.entries != nullptr);
            assert(page.count > 0 && page.count <= VisibleRows);
        }
        for (std::size_t i = 0; i < n; ++i) {
            assert(entrySymbol(page, i) && entrySymbol(page, i)[0]);
            assert(entryMeaning(page, i) && entryMeaning(page, i)[0]);
            if (!isConstantPage(page)) assert(page.entries[i].example && page.entries[i].example[0]);
            else {
                assert(drawable(font::Face::Medium, entrySymbol(page, i)));
                assert(drawable(font::Face::Small, entryMeaning(page, i)));
            }
        }
    }
    assert(constantPages == fx::physconst::constGroups().size());
    assert(constantEntries == fx::physconst::allConstants().size());
    return testutil::finish("cheatsheet_smoke");
}
