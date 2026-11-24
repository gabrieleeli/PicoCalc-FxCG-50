// v1.17.0: record del workspace v12 (solo Pimoroni Pico Plus 2 W).
//  - firma (scheda e versione del firmware) e CRC-32 per sezione;
//  - le sezioni delle dieci modalita' Base senza perdite;
//  - migrazione dalla v11 (fixture scritta dalla v1.16.0): le modalita' Base
//    ripartono dalle variabili di modo che la v11 aveva gia';
//  - una sezione di modalita' danneggiata viene isolata (la modalita' riparte
//    dai valori iniziali, il resto del record si legge) e il firmware lo dice;
//  - sezioni di un firmware piu' nuovo conservate; record troncati rifiutati;
//  - le sezioni come parti del workspace per le strisce di eActivity.
#include "app_rig.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <cmath>
#include <cstring>

using namespace fx;

namespace {
const std::uint8_t kV1160[] = {
#include "fixtures/workspace_v1_16_0.inc"
};
std::uint16_t get16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
std::uint32_t get32(const std::uint8_t* p) { return get16(p) | (static_cast<std::uint32_t>(get16(p + 2)) << 16); }
// Posizione della coda TLV di un record v12: si cerca la sezione con l'id dato
// partendo dalla fine (le sezioni sono in fondo al record, in ordine di id).
struct SectionAt { std::size_t header{0}, data{0}, length{0}; bool found{false}; };
SectionAt findSection(const std::vector<std::uint8_t>& rec, std::uint16_t id) {
    // la coda: u16 numero, poi per sezione id, versione, lunghezza, CRC, dati
    for (std::size_t start = rec.size(); start-- > 6;) {
        std::size_t p = start;
        const std::uint16_t n = get16(&rec[p]);
        if (n == 0 || n > 64) continue;
        p += 2;
        SectionAt out;
        bool ok = true;
        int previous = -1;
        for (std::uint16_t k = 0; k < n && ok; ++k) {
            if (p + 12 > rec.size()) { ok = false; break; }
            const std::uint16_t sid = get16(&rec[p]);
            const std::uint32_t len = get32(&rec[p + 4]);
            if (static_cast<int>(sid) <= previous || p + 12 + len > rec.size()) { ok = false; break; }
            previous = sid;
            if (sid == id) { out.header = p; out.data = p + 12; out.length = len; out.found = true; }
            p += 12 + len;
        }
        if (ok && p == rec.size() && out.found) return out;
    }
    return {};
}
void fillAll(Workspace& ws) {
    ws.memory.store('B', 7);
    ws.sheet.sheet.set(sheet::Ref{0, 1}, "2");
    ws.sheet.sheet.set(sheet::Ref{1, 1}, "=A1*3");
    ws.sheet.file = "CONTI.pcsheet";
    ws.dyna.function = "A sin(X)"; ws.dyna.speed = dyna::Speed::Slow;
    ws.recursion.type = rec::Type::An2; ws.recursion.an2 = "rec_an1+rec_an";
    ws.conic.model = conic::Model::Ellipse; ws.conic.coef = {5, 3, 1, 1, 1};
    ws.financial.begin = true; ws.financial.cash = {-100, 60, 60};
    ws.program.lastFile = "HELLO.pcprog"; ws.program.cursorLine = 3;
    ws.link.sent = 4; ws.link.lastName = "WORKSPACE";
    geo::addLine(ws.geometry, geo::Kind::Segment, geo::addPoint(ws.geometry, 0, 0), geo::addPoint(ws.geometry, 3, 4));
    ws.pictplot.points.push_back(pplot::Point{1, 2}); ws.pictplot.background = "PICT01.pcpict";
    ws.python.lastScript = "DEMO.py"; ws.python.history = {"1+2"};
}
bool sameModes(const Workspace& a, const Workspace& b) {
    return a.sheet == b.sheet && a.dyna == b.dyna && a.recursion == b.recursion && a.conic == b.conic && a.financial == b.financial &&
           a.program == b.program && a.link == b.link && a.geometry == b.geometry && a.pictplot == b.pictplot && a.python == b.python;
}
} // namespace

int main() {
    board::init();
    // ---------------------------------------------------------------- versione e firma
    CHECK(workspaceRecordVersion() == 12);
    auto ws = std::make_unique<Workspace>();
    fillAll(*ws);
    const auto rec = serializeWorkspace(*ws);
    CHECK(rec[4] == 12 && rec[5] == 0);
    {
        const std::size_t n = get16(&rec[6]);
        const std::string origin(reinterpret_cast<const char*>(&rec[8]), n);
        CHECK(origin.rfind("pico_plus_2w ", 0) == 0 && origin.size() > 13);
        auto back = std::make_unique<Workspace>();
        CHECK(deserializeWorkspace(rec.data(), rec.size(), *back));
        CHECK(back->recordOrigin == origin && back->damagedSections.empty());
        CHECK(sameModes(*ws, *back) && back->memory.recall('B') == 7);
        CHECK(serializeWorkspace(*back) == rec);                                     // riscritto identico
    }
    // un workspace vuoto non scrive le sezioni delle modalita' (valori iniziali)
    {
        Workspace empty;
        const auto e = serializeWorkspace(empty);
        CHECK(get16(&e[e.size() - 2]) == 0);
    }
    // ---------------------------------------------------------------- migrazione v11 -> v12
    {
        auto v11 = std::make_unique<Workspace>();
        CHECK(kV1160[4] == 11 && deserializeWorkspace(kV1160, sizeof kV1160, *v11));
        const ModeVariables& m = v11->modeVars;
        CHECK(m.dynaStart == 2 && m.dynaEnd == 6 && m.dynaPitch == 2 && m.recFormula == "2rec_an+1" && m.recEnd == 8 && m.recInitial[0] == 3);
        CHECK(m.tvmN == 24 && m.tvmI == 4.5 && m.tvmPV == -5000);
        CHECK(v11->eactivity.folder == "SCUOLA" && v11->memory.recall('B') == 7 && v11->lists[0].size() == 3);
        // le modalita' Base partono dai valori iniziali e leggono le variabili di modo della v11
        Workspace fresh;
        CHECK(sameModes(*v11, fresh) && v11->recordOrigin.empty());
        const fin::Tvm before{m.tvmN, m.tvmI, m.tvmPV, m.tvmPMT, m.tvmFV, m.tvmPY, m.tvmCY};
        CHECK(std::abs(fin::solve(fin::TvmVar::FV, before, false) - 5000 * std::pow(1.00375, 24)) < 1e-6);
        rec::Setup s;
        s.formula = m.recFormula; s.start = static_cast<int>(m.recStart); s.end = static_cast<int>(m.recEnd); s.a0 = m.recInitial[0];
        CHECK(rec::table(s, v11->evalContext()).back().a == 1023);         // 3, 7, 15, ... (n = 8)
        // il primo salvataggio scrive la v12; la lettura e' identica
        const auto v12 = serializeWorkspace(*v11);
        auto again = std::make_unique<Workspace>();
        CHECK(v12[4] == 12 && deserializeWorkspace(v12.data(), v12.size(), *again));
        CHECK(again->modeVars.recFormula == "2rec_an+1" && again->eactivity.folder == "SCUOLA" && again->modeVars.tvmI == 4.5);
        // e la Plus scrive ancora la v11 a richiesta (test, firmware precedenti)
        const auto old = serializeWorkspaceV11(*again);
        auto back = std::make_unique<Workspace>();
        CHECK(old[4] == 11 && deserializeWorkspace(old.data(), old.size(), *back) && back->modeVars.recFormula == "2rec_an+1");
        // una v11 con le sezioni delle modalita' Base (senza CRC) si legge
        auto full = std::make_unique<Workspace>();
        fillAll(*full);
        const auto full11 = serializeWorkspaceV11(*full);
        auto full11back = std::make_unique<Workspace>();
        CHECK(deserializeWorkspace(full11.data(), full11.size(), *full11back) && sameModes(*full, *full11back));
    }
    // ---------------------------------------------------------------- sezioni danneggiate isolate
    {
        const std::uint16_t sheetId = section::ModeBase + static_cast<std::uint16_t>(app::AppId::Spreadsheet);
        const SectionAt at = findSection(rec, sheetId);
        CHECK(at.found && at.length > 4);
        // 1) CRC sbagliato: il foglio riparte vuoto, il resto resta
        auto bad = rec;
        bad[at.data + at.length / 2] ^= 0x5A;
        auto back = std::make_unique<Workspace>();
        CHECK(deserializeWorkspace(bad.data(), bad.size(), *back));
        CHECK(back->damagedSections == std::vector<std::uint16_t>({sheetId}) && back->sheet.sheet.empty());
        CHECK(back->memory.recall('B') == 7 && back->geometry == ws->geometry && back->python == ws->python);
        CHECK(std::string(sectionModeName(sheetId)) == "Spreadsheet");
        // 2) CRC giusto ma dati non validi (versione dei dati 9): isolata anche questa
        auto junk = rec;
        junk[at.data] = 9;
        const std::uint32_t crc = storage::crc32(&junk[at.data], at.length);
        for (int k = 0; k < 4; ++k) junk[at.header + 8 + static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(crc >> (8 * k));
        auto back2 = std::make_unique<Workspace>();
        CHECK(deserializeWorkspace(junk.data(), junk.size(), *back2) && back2->damagedSections.size() == 1 && back2->dyna == ws->dyna);
        // 3) il firmware lo dice all'avvio e il record si riscrive pulito
        storage::RecordStore store(board::storageFlash());
        store.eraseAll();
        CHECK(store.save(bad));
        {
            testrig::Rig r;
            CHECK(r.error() && r.errorTitle() == "Data ERROR" && r.errorDetail().find("Spreadsheet") != std::string::npos);
            CHECK(r.ws().memory.recall('B') == 7 && r.ws().sheet.sheet.empty());
            r.closeError();
            CHECK(r.fw.saveNow());
        }
        {
            testrig::Rig r;
            CHECK(!r.error() && r.ws().damagedSections.empty() && r.ws().memory.recall('B') == 7);
        }
        store.eraseAll();
        // 4) un errore di struttura (lunghezza oltre il record, troncamento) rifiuta il record
        auto cut = rec;
        cut.resize(cut.size() - 3);
        auto t = std::make_unique<Workspace>();
        CHECK(!deserializeWorkspace(cut.data(), cut.size(), *t));
        auto longer = rec;
        longer[at.header + 4] = 0xFF; longer[at.header + 5] = 0xFF;
        CHECK(!deserializeWorkspace(longer.data(), longer.size(), *t));
        auto future = rec;
        future[4] = 13;
        CHECK(!deserializeWorkspace(future.data(), future.size(), *t));
    }
    // ---------------------------------------------------------------- sezioni di un firmware piu' nuovo
    {
        auto w = std::make_unique<Workspace>();
        RecordSection future;
        future.id = 0x0150; future.version = 9; future.data = {1, 2, 3};
        RecordSection newerSheet;
        newerSheet.id = section::ModeBase + static_cast<std::uint16_t>(app::AppId::Spreadsheet); newerSheet.version = 7; newerSheet.data = {4, 5};
        w->preservedSections = {newerSheet, future};
        const auto r1 = serializeWorkspace(*w);
        auto b = std::make_unique<Workspace>();
        CHECK(deserializeWorkspace(r1.data(), r1.size(), *b) && b->preservedSections.size() == 2 && b->preservedSections[1] == future);
        CHECK(serializeWorkspace(*b) == r1);
    }
    // ---------------------------------------------------------------- parti per le strisce
    {
        const std::uint8_t sheetPart = part::ModeBase + static_cast<std::uint8_t>(app::AppId::Spreadsheet);
        const std::uint8_t conicPart = part::ModeBase + static_cast<std::uint8_t>(app::AppId::ConicGraphs);
        const auto parts = encodeParts(*ws, {sheetPart, conicPart, part::Lists});
        auto w = std::make_unique<Workspace>();
        CHECK(decodeParts(*w, parts.data(), parts.size()) && w->sheet == ws->sheet && w->conic == ws->conic);
        resetPart(*w, sheetPart);
        CHECK(w->sheet.sheet.empty() && w->conic == ws->conic);
        auto dup = encodeParts(*ws, {sheetPart, sheetPart});
        CHECK(!decodeParts(*w, dup.data(), dup.size()));
        auto unknown = encodeParts(*ws, {sheetPart});
        unknown[1] = part::ModeBase + static_cast<std::uint8_t>(app::AppId::Symbols);   // nessuna sezione per Symbols
        CHECK(!decodeParts(*w, unknown.data(), unknown.size()));
        CHECK_THROWS(encodePart(*ws, part::ModeBase + static_cast<std::uint8_t>(app::AppId::Graph)));
    }
    return testutil::finish("workspace_v117_test");
}
