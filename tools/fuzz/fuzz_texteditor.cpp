// v1.15.0 (F5) — Fuzzing dell'editor di testo: sequenze di tasti, testo,
// selezioni, ricerche e salti; dopo ogni operazione il conteggio dei byte deve
// coincidere con text() e restare nei limiti, il cursore dentro il testo.
#include "text_editor.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace fx;
static void check(const TextEditor& t, std::size_t maxBytes) {
    const std::string s = t.text();
    if (s.size() != t.bytes() || t.bytes() > maxBytes || t.cursorLine() >= t.lineCount() || t.cursorColumn() > t.line(t.cursorLine()).size()) {
        std::fprintf(stderr, "INVARIANT bytes=%zu text=%zu\n", t.bytes(), s.size());
        std::abort();
    }
}
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    const std::size_t maxBytes = 300, maxLine = 40;
    TextEditor t(maxBytes, maxLine);
    std::string clip;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t b = d[i];
        KeyEvent ev;
        switch (b % 16) {
        case 0: ev.key = Key::Char; ev.ch = static_cast<char>(32 + (b >> 4) * 6); break;
        case 1: ev.key = Key::Exe; break;
        case 2: ev.key = Key::Del; break;
        case 3: ev.key = Key::DelForward; break;
        case 4: ev.key = Key::Left; ev.ctrl = b & 0x80; break;
        case 5: ev.key = Key::Right; ev.ctrl = b & 0x80; break;
        case 6: ev.key = Key::Up; ev.ctrl = b & 0x80; break;
        case 7: ev.key = Key::Down; ev.ctrl = b & 0x80; break;
        case 8: ev.key = Key::Home; ev.ctrl = b & 0x80; break;
        case 9: ev.key = Key::End; ev.ctrl = b & 0x80; break;
        case 10: ev.key = Key::Char; ev.ctrl = true; ev.ch = "cxv"[(b >> 4) % 3]; break;
        case 11: ev.key = Key::AC; break;
        case 12: ev.key = Key::PageDown; break;
        case 13: (void)t.insert(std::string("\xCE\xB8\n", (b >> 4) % 4)); check(t, maxBytes); continue;
        case 14: (void)t.find(std::string(1, static_cast<char>('a' + (b >> 4)))); check(t, maxBytes); continue;
        default: (void)t.gotoLine(b >> 4); check(t, maxBytes); continue;
        }
        (void)t.key(ev, &clip);
        check(t, maxBytes);
    }
    return 0;
}
