#include "cheatsheet.hpp"
#include "test_util.hpp"
#undef assert
#define assert(x) CHECK(x)
#include <cstring>
int main() {
    using namespace fx::help;
    static_assert(PageCount == 16);
    assert(std::strcmp(pages[0].title, "ARITMETICA") == 0);
    assert(std::strcmp(pages[2].title, "LETTERE GRECHE") == 0);
    assert(std::strcmp(pages[3].title, "COSTANTI MATEMATICHE") == 0);
    assert(std::strcmp(pages[10].title, "ALTRE FUNZIONI") == 0);
    assert(std::strcmp(pages[11].title, "COMBINATORIA") == 0);
    assert(std::strcmp(pages[14].title, "GRECHE MAIUSCOLE") == 0);
    assert(std::strcmp(pages[1].entries[5].symbol, "rootN(x)") == 0);
    for (const auto& page : pages) {
        assert(page.title != nullptr);
        assert(page.entries != nullptr);
        assert(page.count > 0 && page.count <= 6);
        for (unsigned i = 0; i < page.count; ++i) {
            assert(page.entries[i].symbol && page.entries[i].symbol[0]);
            assert(page.entries[i].meaning && page.entries[i].meaning[0]);
            assert(page.entries[i].example && page.entries[i].example[0]);
        }
    }
    return testutil::finish("cheatsheet_smoke");
}
