// v1.13.1 — // Sequenze di tasti sull'intero firmware (porta host della scheda, come i test):
// ogni byte e' un tasto della tabella kKeys; il record in flash si azzera a ogni input.
// Key-sequence fuzzer of the whole firmware (host board port, as in the tests).
#include "app_rig.hpp"
#include "storage.hpp"
#include <cstdint>
using namespace fx;
static const std::uint8_t kKeys[] = {
    '0','1','2','3','4','5','6','7','8','9','.','+','-','*','/','^','(',')',',','=','x','A','B','X','i','e','s','q','[',']','{','}',
    pckey::F1, pckey::F2, pckey::F3, pckey::F4, pckey::F5, pckey::F6, pckey::F7, pckey::F8, pckey::F9, pckey::F10,
    pckey::Up, pckey::Down, pckey::Left, pckey::Right, pckey::Enter, pckey::Esc, pckey::Backspace, pckey::Del,
    pckey::Break, pckey::Insert, pckey::Home, pckey::End, pckey::PageUp, pckey::PageDown, pckey::Enter, pckey::Enter,
    pckey::Up, pckey::Down, pckey::F1, pckey::Tab, ' ', '!'};
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n > 400) return 0;
    { storage::RecordStore rs(board::storageFlash()); (void)rs.eraseAll(); }
    testrig::Rig r;
    // First byte: which application to start from (launcher digit).
    for (size_t i = 0; i < n; ++i) {
        const std::uint8_t k = kKeys[d[i] % sizeof(kKeys)];
        if (k == pckey::Tab && (d[i] & 0x80)) continue;
        if (k >= 0x80 || k < 0x20) r.key(k); else r.raw(testrig::press(k));
    }
    r.settle(200);
    return 0;
}
