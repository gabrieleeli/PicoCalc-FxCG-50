// v1.17.0 — Fuzzing del protocollo di Link: decodificatore a flusso (rumore,
// trame troncate, CRC sbagliati), pacchetti .pclink e un trasferimento
// guidato dai byte del "PC" (ricevitore e mittente). Primo byte: modo.
#include "link_protocol.hpp"
#include <cstdio>
#include <cstdlib>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 1) return 0;
    const std::uint8_t mode = d[0] % 4;
    const std::uint8_t* p = d + 1;
    const std::size_t m = n - 1;
    if (mode == 0) {
        link::Decoder dec;
        std::size_t i = 0, step = 1 + (n % 7);
        link::Frame f;
        while (i < m) {
            const std::size_t k = std::min(step, m - i);
            dec.push(p + i, k);
            i += k;
            while (dec.next(f)) {
                const auto again = link::encodeFrame(f);
                link::Decoder d2;
                d2.push(again.data(), again.size());
                link::Frame g;
                if (!d2.next(g) || g.type != f.type || g.seq != f.seq || g.payload != f.payload) { std::fprintf(stderr, "FRAME\n"); std::abort(); }
            }
        }
        return 0;
    }
    if (mode == 1) {
        std::vector<link::Entry> es;
        if (!link::decodePackage(p, m, es)) return 0;
        const auto again = link::encodePackage(es);
        if (again != std::vector<std::uint8_t>(p, p + m)) { std::fprintf(stderr, "PACKAGE\n"); std::abort(); }
        for (const auto& e : es) if (e.kind == link::Entry::Kind::File && !link::validPath(e.path)) std::abort();
        return 0;
    }
    // modo 2: il ricevitore legge i byte; modo 3: il mittente riceve i byte come risposte
    link::LoopPort dev, pc;
    link::connect(dev, pc);
    std::uint32_t now = 0;
    dev.inbox.assign(p, p + m);
    if (mode == 2) {
        link::Receiver rx(dev, [&now] { return now; }, 64 * 1024);
        for (int k = 0; k < 64 && rx.status() == link::Status::Running; ++k) { step::Budget b(32, 0, nullptr); (void)rx.step(b); now += 400; }
        if (rx.status() == link::Status::Done && rx.package().size() != rx.total()) std::abort();
    } else {
        std::vector<std::uint8_t> pkg(3000, 0x5A);
        link::Sender tx(dev, [&now] { return now; }, pkg, "F");
        for (int k = 0; k < 64 && tx.status() == link::Status::Running; ++k) { step::Budget b(32, 0, nullptr); (void)tx.step(b); now += 700; }
        tx.cancelled();
    }
    return 0;
}
