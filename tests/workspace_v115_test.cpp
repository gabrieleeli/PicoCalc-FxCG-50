// v1.15.0 (F2): workspace a sezioni estensibili, record v11.
//  - lettura invariata dei record v1-v10 (fixture dalla v1.6.0 alla v1.14.0);
//  - coda TLV: sezioni conosciute decodificate, sconosciute o piu' nuove
//    conservate byte per byte e riscritte intatte;
//  - sezioni corrotte, troncate, duplicate, fuori ordine: record rifiutato e
//    workspace di destinazione intatto;
//  - RESET (resetAll) cancella anche le sezioni conservate.
#include "run_matrix.hpp"
#include "test_util.hpp"
#include "features.hpp"
#include "workspace.hpp"
#include <cstring>
#include <memory>

using namespace fx;

// v1.17.0: questa suite prova il formato v11. La Plus scrive la v12 dalla
// v1.17.0: qui si usa il suo scrittore v11 (tests/workspace_v117_test.cpp prova la v12).
namespace {
#if FXAI_BOARD_PICO_PLUS_2W
std::vector<std::uint8_t> writeV11(const Workspace& w) { return serializeWorkspaceV11(w); }
std::vector<std::uint8_t> writeV11(const Workspace& w, const SectionCodec* c, std::size_t n) { return serializeWorkspaceV11(w, c, n); }
#else
std::vector<std::uint8_t> writeV11(const Workspace& w) { return serializeWorkspace(w); }
std::vector<std::uint8_t> writeV11(const Workspace& w, const SectionCodec* c, std::size_t n) { return serializeWorkspace(w, c, n); }
#endif
} // namespace

namespace {
const std::uint8_t kV160[] = {
#include "fixtures/workspace_v1_6_0.inc"
};
const std::uint8_t kV180[] = {
#include "fixtures/workspace_v1_8_0.inc"
};
const std::uint8_t kV190[] = {
#include "fixtures/workspace_v1_9_0.inc"
};
const std::uint8_t kV1100[] = {
#include "fixtures/workspace_v1_10_0.inc"
};
const std::uint8_t kV1110[] = {
#include "fixtures/workspace_v1_11_0.inc"
};
const std::uint8_t kV1120[] = {
#include "fixtures/workspace_v1_12_0.inc"
};
const std::uint8_t kV1131[] = {
#include "fixtures/workspace_v1_13_1_graph3d.inc"
};
// Record v10 scritto dai sorgenti della v1.14.0 (tests/fixtures/gen_workspace_v1_14_0.cpp.txt).
const std::uint8_t kV1140[] = {
#include "fixtures/workspace_v1_14_0.inc"
};

// Una sezione di prova "conosciuta": un contatore e un testo.
struct Probe { int counter{0}; std::string text; };
Probe gProbe;
bool probePresent(const Workspace&) { return gProbe.counter != 0 || !gProbe.text.empty(); }
void probeEncode(const Workspace&, std::vector<std::uint8_t>& out) {
    out.push_back(static_cast<std::uint8_t>(gProbe.counter));
    out.insert(out.end(), gProbe.text.begin(), gProbe.text.end());
}
bool probeDecode(Workspace&, std::uint16_t version, const std::uint8_t* d, std::size_t n) {
    if (version == 0 || n < 1) return false;
    gProbe.counter = d[0];
    gProbe.text.assign(reinterpret_cast<const char*>(d + 1), n - 1);
    return true;
}
const SectionCodec kProbe[] = {{0x0150, 2, probePresent, probeEncode, probeDecode}};

std::vector<std::uint8_t> withTail(std::vector<std::uint8_t> rec, const std::vector<RecordSection>& sections) {
    // sostituisce la coda vuota (u16 0) del record v11 con le sezioni date
    rec.resize(rec.size() - 2);
    rec.push_back(static_cast<std::uint8_t>(sections.size())); rec.push_back(static_cast<std::uint8_t>(sections.size() >> 8));
    for (const auto& s : sections) {
        rec.push_back(static_cast<std::uint8_t>(s.id)); rec.push_back(static_cast<std::uint8_t>(s.id >> 8));
        rec.push_back(static_cast<std::uint8_t>(s.version)); rec.push_back(static_cast<std::uint8_t>(s.version >> 8));
        const auto n = static_cast<std::uint32_t>(s.data.size());
        for (int k = 0; k < 4; ++k) rec.push_back(static_cast<std::uint8_t>(n >> (8 * k)));
        rec.insert(rec.end(), s.data.begin(), s.data.end());
    }
    return rec;
}
RecordSection sec(std::uint16_t id, std::uint16_t version, std::string bytes) {
    RecordSection s; s.id = id; s.version = version; s.data.assign(bytes.begin(), bytes.end());
    return s;
}
} // namespace

int main() {
    // ---------------------------------------------------------------- F2.4 lettura v1-v10 invariata
    {
        struct Fixture { const std::uint8_t* data; std::size_t size; int version; };
        const Fixture fixtures[] = {{kV160, sizeof kV160, 1}, {kV180, sizeof kV180, 4}, {kV190, sizeof kV190, 5}, {kV1100, sizeof kV1100, 6},
                                    {kV1110, sizeof kV1110, 7}, {kV1120, sizeof kV1120, 8}, {kV1131, sizeof kV1131, 9}, {kV1140, sizeof kV1140, 10}};
        for (const Fixture& f : fixtures) {
            CHECK(f.data[4] == f.version);
            auto ws = std::make_unique<Workspace>();
            CHECK(deserializeWorkspace(f.data, f.size, *ws));
            CHECK(ws->preservedSections.empty());
            // riscritto nel formato v11 e riletto: identico byte per byte alla seconda scrittura
            const auto v11 = writeV11(*ws);
            CHECK(v11[4] == 11 && v11[5] == 0);
            auto back = std::make_unique<Workspace>();
            CHECK(deserializeWorkspace(v11.data(), v11.size(), *back));
            CHECK(writeV11(*back) == v11);
        }
        // il contenuto della fixture della v1.14.0
        auto ws = std::make_unique<Workspace>();
        CHECK(deserializeWorkspace(kV1140, sizeof kV1140, *ws));
        CHECK(ws->settings.angle == AngleMode::Degrees && ws->settings.mixedFractions);
        CHECK(ws->history.size() == 5 && ws->memory.recall('B') == 7);
        CHECK(ws->functions[0].expr == "x^2-1" && ws->functions[0].selected);
        CHECK(ws->listName(0) == "QTY" && ws->lists[0] == std::vector<double>({1.5, 2.5, 3.5}));
        CHECK(ws->strings[0] == "HELLO");
        CHECK(ws->graph3d.set.fns[0].kind == g3d::Kind::Z && ws->graph3d.set.fns[0].expr[0] == "x^2-y^2" && ws->graph3d.set.window.theta == 30);
        CHECK(ws->equation.solverExpr == "x^2=2" && ws->modeVars.dynaStart == 2);
        CHECK(workspaceRecordVersion() == (FXAI_BOARD_PICO_PLUS_2W ? 12 : 11));
    }

    // ---------------------------------------------------------------- F2.1/F2.2 coda TLV
    Workspace base;
    base.memory.store('C', 3);
    const auto empty = writeV11(base);
    CHECK(empty[empty.size() - 2] == 0 && empty[empty.size() - 1] == 0);          // nessuna sezione: u16 0
    {
        // sezioni sconosciute: conservate in ordine e riscritte intatte
        const auto rec = withTail(empty, {sec(0x0010, 1, "abc"), sec(0x0105, 3, std::string("\0\1\2", 3)), sec(0x0200, 9, "")});
        Workspace ws;
        CHECK(deserializeWorkspace(rec.data(), rec.size(), ws));
        CHECK(ws.preservedSections.size() == 3 && ws.preservedSections[1].id == 0x0105 && ws.preservedSections[1].version == 3);
        CHECK(ws.preservedSections[1].data == std::vector<std::uint8_t>({0, 1, 2}) && ws.preservedSections[2].data.empty());
        CHECK(ws.memory.recall('C') == 3);
        CHECK(writeV11(ws) == rec);                                         // riscritto identico
        // dopo una modifica del resto del workspace le sezioni restano
        ws.memory.store('D', 4);
        Workspace again;
        const auto rec2 = writeV11(ws);
        CHECK(deserializeWorkspace(rec2.data(), rec2.size(), again) && again.preservedSections == ws.preservedSections && again.memory.recall('D') == 4);
        // RESET cancella tutto, anche le sezioni dei firmware successivi
        again.resetAll();
        CHECK(again.preservedSections.empty());
    }
    {
        // sezione conosciuta: decodificata; scritta solo se presente
        gProbe = Probe{7, "eact"};
        Workspace ws;
        const auto rec = writeV11(ws, kProbe, 1);
        gProbe = Probe{};
        Workspace back;
        CHECK(deserializeWorkspace(rec.data(), rec.size(), back, kProbe, 1));
        CHECK(gProbe.counter == 7 && gProbe.text == "eact" && back.preservedSections.empty());
        // la stessa sezione letta da un firmware che non la conosce: conservata e riscritta intatta
        Workspace older;
        CHECK(deserializeWorkspace(rec.data(), rec.size(), older));
        CHECK(older.preservedSections.size() == 1 && older.preservedSections[0].id == 0x0150 && older.preservedSections[0].version == 2);
        CHECK(writeV11(older) == rec);
        // nulla da salvare: nessuna sezione
        gProbe = Probe{};
        const auto none = writeV11(ws, kProbe, 1);
        CHECK(none == writeV11(ws));
        // versione piu' nuova di quella conosciuta: conservata, non decodificata, e vince sulla codifica
        const auto newer = withTail(empty, {sec(0x0150, 5, std::string("\x09" "future", 7))});
        gProbe = Probe{1, "now"};
        Workspace nw;
        CHECK(deserializeWorkspace(newer.data(), newer.size(), nw, kProbe, 1));
        CHECK(gProbe.counter == 1 && gProbe.text == "now");                           // non toccata
        CHECK(nw.preservedSections.size() == 1 && nw.preservedSections[0].version == 5);
        const auto out = writeV11(nw, kProbe, 1);
        CHECK(out == newer);                                                          // i dati nuovi non si perdono
        // versione piu' vecchia leggibile: decodificata
        const auto older2 = withTail(empty, {sec(0x0150, 1, std::string("\x03" "old", 4))});
        Workspace ow;
        CHECK(deserializeWorkspace(older2.data(), older2.size(), ow, kProbe, 1) && gProbe.counter == 3 && gProbe.text == "old");
        // dati non validi per la codifica conosciuta: record rifiutato
        const auto badKnown = withTail(empty, {sec(0x0150, 2, "")});
        Workspace bk;
        bk.memory.store('Z', 26);
        CHECK(!deserializeWorkspace(badKnown.data(), badKnown.size(), bk, kProbe, 1));
        CHECK(bk.memory.recall('Z') == 26);
        gProbe = Probe{};
    }

    // ---------------------------------------------------------------- F2.3 sezioni corrotte
    {
        Workspace target;
        target.memory.store('Q', 17);
        auto rejected = [&](const std::vector<std::uint8_t>& rec) {
            const bool ok = deserializeWorkspace(rec.data(), rec.size(), target);
            return !ok && target.memory.recall('Q') == 17 && target.preservedSections.empty();
        };
        // duplicate e fuori ordine
        CHECK(rejected(withTail(empty, {sec(0x0300, 1, "a"), sec(0x0300, 1, "b")})));
        CHECK(rejected(withTail(empty, {sec(0x0301, 1, "a"), sec(0x0300, 1, "b")})));
        // troncata: lunghezza oltre la fine del record, e ogni taglio della coda
        auto rec = withTail(empty, {sec(0x0300, 1, "abcdef"), sec(0x0400, 1, "xyz")});
        for (std::size_t cut = empty.size() - 2; cut < rec.size(); ++cut) {
            Workspace t;
            CHECK(!deserializeWorkspace(rec.data(), cut, t));
        }
        auto longLen = rec;
        longLen[empty.size() - 2 + 2 + 4] = 0xFF;                                      // lunghezza della prima sezione
        CHECK(rejected(longLen));
        // byte in piu' dopo l'ultima sezione
        auto extra = rec; extra.push_back(0);
        CHECK(rejected(extra));
        // numero di sezioni oltre il limite
        auto many = empty;
        many[many.size() - 2] = static_cast<std::uint8_t>(section::MaxSections + 1);
        CHECK(rejected(many));
        // lunghezza oltre MaxSectionBytes
        auto huge = withTail(empty, {sec(0x0300, 1, "a")});
        const std::uint32_t big = static_cast<std::uint32_t>(section::MaxSectionBytes + 1);
        std::memcpy(huge.data() + empty.size() - 2 + 2 + 4, &big, 4);
        CHECK(rejected(huge));
        // un record valido con sezioni diventa il workspace solo quando e' tutto valido
        Workspace ok;
        CHECK(deserializeWorkspace(rec.data(), rec.size(), ok) && ok.preservedSections.size() == 2);
    }
    // ---------------------------------------------------------------- versioni future del formato principale
    {
        auto future = empty;
        future[4] = static_cast<std::uint8_t>(workspaceRecordVersion() + 1);
        Workspace t;
        CHECK(!deserializeWorkspace(future.data(), future.size(), t));
        // v1.16.0: la prima sezione di produzione e' quella di eActivity (0x0102)
        std::size_t n = 99;
        (void)workspaceSectionCodecs(n);
        // v1.17.0: sulla Plus anche le sezioni delle modalita' Base compilate.
        const std::size_t base = features::kSpreadsheet + features::kDynaGraph + features::kRecursion + features::kConicGraphs + features::kProgram +
                                 features::kFinancial + features::kLink + features::kGeometry + features::kPicturePlot + features::kPython;
        CHECK(n == (features::kEActivity ? 1u : 0u) + base);
    }
    return testutil::finish("workspace_v115_test");
}
