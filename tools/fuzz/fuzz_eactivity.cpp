// v1.16.0 (M2) — Fuzzing di eActivity.
//  * primo byte pari: contenuto di un file .pceact (eact::decode): ogni
//    documento accettato si ricodifica identico; testo, a capo e unita' del
//    testo restano coerenti; le parti del workspace (strisce) si decodificano
//    con la garanzia forte;
//  * primo byte dispari: un documento qualunque (u16 lunghezza + byte) aperto
//    nel controller e modificato con una sequenza di tasti (come fuzz_ui):
//    nessun crash, il documento resta entro il limite della scheda.
// Con FXAI_FUZZ_EACT_DOCS=1 ogni input e' un documento (la parte lenta del
// controller, che riavvia il firmware a ogni input, e' saltata).
#include "app_rig.hpp"
#include "eactivity.hpp"
#include "limits.hpp"
#include "storage.hpp"
#include "typed_files.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace fx;
namespace {
int measure(const std::string& t, std::size_t a, std::size_t b, bool island, void*) {
    return island ? 40 : static_cast<int>(b - a) * 8 + (t.empty() ? 0 : 0);
}
const std::uint8_t kKeys[] = {
    '0', '1', '2', '+', '/', 'x', 'A', 'a', ' ', '\'', ',', 'Z',
    pckey::F1, pckey::F2, pckey::F3, pckey::F4, pckey::F5, pckey::F6, pckey::F7, pckey::F8,
    pckey::Up, pckey::Down, pckey::Left, pckey::Right, pckey::Enter, pckey::Esc, pckey::Backspace, pckey::Del,
    pckey::Break, pckey::Insert, pckey::Home, pckey::End, pckey::PageUp, pckey::PageDown, pckey::Enter, pckey::Down};
}
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 1) return 0;
    static const bool docsOnly = std::getenv("FXAI_FUZZ_EACT_DOCS") != nullptr;
    if ((d[0] & 1) == 0 || docsOnly) {
        eact::Document doc;
        if (!eact::decode(d + 1, n - 1, doc)) return 0;
        const auto again = eact::encode(doc);
        eact::Document back;
        if (!eact::decode(again.data(), again.size(), back) || eact::encode(back) != again) { std::fprintf(stderr, "ROUNDTRIP\n"); std::abort(); }
        for (const eact::Line& l : doc.lines) {
            if (l.kind != eact::LineKind::Text) continue;
            if (!eact::validLineText(l.text)) { std::fprintf(stderr, "INVALID TEXT\n"); std::abort(); }
            for (std::size_t p = 0; p < l.text.size();) {
                const std::size_t q = eact::nextUnit(l.text, p);
                if (q <= p || eact::prevUnit(l.text, q) != p) { std::fprintf(stderr, "UNITS\n"); std::abort(); }
                p = q;
            }
            std::size_t covered = 0;
            for (const auto& row : eact::wrap(l.text, 64 + d[0], measure, nullptr))
                for (const auto& piece : row) { if (piece.from != covered) { std::fprintf(stderr, "WRAP GAP\n"); std::abort(); } covered = piece.to; }
            if (covered != l.text.size()) { std::fprintf(stderr, "WRAP END\n"); std::abort(); }
        }
        if (!doc.memories.empty()) {
            Workspace ws;
            const auto before = serializeWorkspace(ws);
            if (!decodeParts(ws, doc.memories.data(), doc.memories.size()) && serializeWorkspace(ws) != before) { std::fprintf(stderr, "PARTS\n"); std::abort(); }
        }
        return 0;
    }
    if (n < 3 || n > 1500) return 0;
    const std::size_t len = std::min<std::size_t>(n - 3, static_cast<std::size_t>(d[1] | (d[2] << 8)));
    static files::RamFileSystem card(4u * 1024u * 1024u);
    board::host::setFileSystem(&card);
    (void)card.remove("PICOCALC/EACT/FUZZ.pceact");
    (void)card.makeDirs("PICOCALC/EACT");
    const std::vector<std::uint8_t> payload(d + 3, d + 3 + len);
    const auto raw = files::encodeTypedFile(files::FileType::EActivity, payload);
    (void)files::writeAtomic(card, "PICOCALC/EACT/FUZZ.pceact", raw.data(), raw.size());
    { storage::RecordStore rs(board::storageFlash()); (void)rs.eraseAll(); }
    testrig::Rig r;
    r.fw.apps().open(app::AppId::EActivity);
    r.key(pckey::Enter);                                     // OPEN del solo file
    for (std::size_t i = 3 + len; i < n && i < 3 + len + 300; ++i) {
        const std::uint8_t k = kKeys[d[i] % sizeof(kKeys)];
        if (k >= 0x80 || k < 0x20) r.key(k); else r.raw(testrig::press(k));
        if (r.fw.apps().activeId() == app::AppId::MainMenu) r.fw.apps().open(app::AppId::EActivity);
    }
    r.settle(200);
    if (auto* ea = r.active<app::EActivityApp>())
        if (ea->documentBytes() > limits::kEActivityDocumentBytes) { std::fprintf(stderr, "LIMIT\n"); std::abort(); }
    board::host::setFileSystem(&board::host::ramCard());
    return 0;
}
