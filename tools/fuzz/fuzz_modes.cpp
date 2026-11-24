// v1.17.0 — Fuzzing dei decoder delle modalita' Base (sezioni del record v12,
// file .pcdyna/.pcrecur/.pcconic/.pcfin/.pcgeo/.pcplot, .pcprog; v1.18.0: Distribution; v1.19.0: .pcgeo v2): ogni dato
// accettato si ricodifica e si rilegge identico, e i calcoli sullo stato letto
// (coniche, TVM, misure, evidenziazione) non vanno in crash.
#include "conic.hpp"
#include "distribution.hpp"
#include "dyna.hpp"
#include "financial.hpp"
#include "geometry.hpp"
#include "link_protocol.hpp"
#include "picture_plot.hpp"
#include "program_mode.hpp"
#include "python_port.hpp"
#include "recursion.hpp"
#include "spreadsheet.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace fx;
namespace {
template <class S, class Dec, class Enc> void roundtrip(const std::uint8_t* p, std::size_t m, Dec dec, Enc enc, S& out, bool& ok) {
    ok = dec(out, p, m);
    if (!ok) return;
    std::vector<std::uint8_t> b;
    enc(out, b);
    S again;
    if (!dec(again, b.data(), b.size()) || !(again == out)) { std::fprintf(stderr, "ROUNDTRIP\n"); std::abort(); }
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 1) return 0;
    const std::uint8_t* p = d + 1;
    const std::size_t m = n - 1;
    bool ok = false;
    try {
        switch (d[0] % 16) {
        case 0: { dyna::State s; roundtrip(p, m, dyna::decode, dyna::encode, s, ok); if (ok) (void)dyna::validate(s); break; }
        case 1: { rec::State s; roundtrip(p, m, rec::decode, rec::encode, s, ok); break; }
        // v1.18.0: sezione 0x0117 di Distribution (valida per costruzione dopo la lettura).
        case 15: { dmode::State s; roundtrip(p, m, dmode::decode, dmode::encode, s, ok); if (ok && !dmode::validate(s).empty()) std::abort(); break; }
        // File .pcdyna, .pcrecur, .pcfin: accettati solo validi, ricodificati identici.
        case 12: {
            dyna::FileData f;
            if (dyna::decodeFile(p, m, f)) {
                const auto b = dyna::encodeFile(f);
                dyna::FileData g;
                if (!dyna::decodeFile(b.data(), b.size(), g) || !(g == f)) std::abort();
                if (dyna::frameValues(f.start, f.end, f.pitch).size() > dyna::MaxFrames) std::abort();
            }
            break;
        }
        case 13: {
            rec::FileData f;
            if (rec::decodeFile(p, m, f)) { const auto b = rec::encodeFile(f); rec::FileData g; if (!rec::decodeFile(b.data(), b.size(), g) || !(g == f)) std::abort(); }
            break;
        }
        case 14: {
            fin::FileData f;
            if (fin::decodeFile(p, m, f)) {
                const auto b = fin::encodeFile(f);
                fin::FileData g;
                if (!fin::decodeFile(b.data(), b.size(), g) || !(g == f)) std::abort();
                for (int k = 0; k < 5; ++k) { try { (void)fin::solve(static_cast<fin::TvmVar>(k), f.tvm, f.state.begin); } catch (const std::exception&) {} }
                try { (void)fin::amortization(f.state, f.tvm); } catch (const std::exception&) {}
            }
            break;
        }
        case 2: {
            conic::State s; roundtrip(p, m, conic::decode, conic::encode, s, ok);
            if (ok && conic::validate(s).empty()) {
                for (const auto& b : conic::branches(s, -10, 10, -10, 10)) { double x, y; (void)conic::point(s, 0, b.t0, x, y); }
                for (int k = 0; k < 6; ++k) { try { (void)conic::solve(s, static_cast<conic::Solve>(k)); } catch (const std::exception&) {} }
            }
            break;
        }
        case 3: {
            fin::State s; roundtrip(p, m, fin::decode, fin::encode, s, ok);
            if (ok) {
                try { (void)fin::simple(s); } catch (const std::exception&) {}
                try { (void)fin::cashFlow(s); } catch (const std::exception&) {}
                try { (void)fin::toEff(s); (void)fin::days(s); } catch (const std::exception&) {}
            }
            break;
        }
        case 4: { progmode::State s; roundtrip(p, m, progmode::decode, progmode::encode, s, ok); break; }
        case 5: {
            geo::State s; roundtrip(p, m, geo::decode, geo::encode, s, ok);
            // v1.19.0: formato 2 (e 1 migrato); misure disponibili per ogni oggetto
            if (ok) { for (const auto& o : s.objects) for (const auto& sp : geo::available(s, {o.id})) (void)geo::measure(s, sp); (void)geo::hitTest(s, 0, 0, 1); }
            geo::State f;
            if (geo::decodeFile(p, m, f)) { const auto b = geo::encodeFile(f); geo::State g; if (!geo::decodeFile(b.data(), b.size(), g) || !(g == f)) std::abort(); }
            break;
        }
        case 6: {
            pplot::State s; roundtrip(p, m, pplot::decode, pplot::encode, s, ok);
            pplot::State f;
            if (pplot::decodeFile(p, m, f)) { const auto b = pplot::encodeFile(f); pplot::State g; if (!pplot::decodeFile(b.data(), b.size(), g) || !(g == f)) std::abort(); }
            break;
        }
        case 7: { py::State s; roundtrip(p, m, py::decode, py::encode, s, ok); break; }
        case 8: { link::State s; roundtrip(p, m, link::decode, link::encode, s, ok); break; }
        case 9: { sheet::State s; roundtrip(p, m, sheet::decode, sheet::encode, s, ok); break; }
        default: {
            // .pcprog: riga del dialetto ed evidenziazione di ogni riga
            const std::string text(reinterpret_cast<const char*>(p), m);
            prog::Dialect dl = prog::Dialect::C;
            const std::string src = progmode::decodeProgram(text, static_cast<prog::Dialect>(d[0] % 3), dl);
            bool inComment = false;
            std::size_t a = 0;
            while (a <= src.size()) {
                const std::size_t e = std::min(src.find('\n', a), src.size());
                const std::string line = src.substr(a, e - a);
                if (progmode::highlight(line, dl, inComment).size() != line.size()) std::abort();
                a = e + 1;
            }
            const std::string again = progmode::encodeProgram(dl, src);
            prog::Dialect d2 = prog::Dialect::C;
            if (progmode::decodeProgram(again, prog::Dialect::C, d2) != src || d2 != dl) std::abort();
            break;
        }
        }
    } catch (const std::bad_alloc&) {
    }
    return 0;
}
