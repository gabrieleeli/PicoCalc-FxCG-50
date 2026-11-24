// v1.17.0, fase 1: inizializzazione di TUTTE le modalita' aperte del registro
// (Available e Base) sul profilo di scheda della build (la suite gira sui due
// profili). Per ogni modalita' e per ogni caso di avvio:
//   C1 avvio a freddo (flash vuota)          C2 dopo RESET di System
//   C3 record v11 valido (fixture v1.16.0)   C4 record v11 corrotto / C5 troncato
//   C6 solo SRAM (PSRAM assente o self-test fallito: stessi pool)
//   C7 memoria quasi esaurita ("Memory ERROR" senza crash)
//   C8 microSD assente                       C9 microSD presente
// controlla (K1) che si apra da MENU con il tasto di accesso diretto, (K2) che
// disegni la schermata e la barra F1-F6, (K3) che accetti la sequenza tipica,
// (K4) che esca con MENU e, se ha una striscia, con SHIFT ' e MENU dalla
// striscia di eActivity, (K5) che entrare e uscire non lasci memoria negli
// allocatori, (K6) che lo stato resti dopo uscita e rientro e dopo un riavvio.
// Tutto l'operator new passa dai pool simulati (src/mem_new.cpp), come sul
// dispositivo: le perdite si vedono nelle statistiche dei pool.
#include "app_rig.hpp"
#include "eactivity.hpp"
#include "mem_pools.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include <functional>
#include <map>
#include <sstream>

using namespace fx;
using namespace fx::app;
namespace pk = fx::pckey;

namespace {
const std::uint8_t kV1160[] = {
#include "fixtures/workspace_v1_16_0.inc"
};
using Rig = testrig::Rig;

struct Mode {
    AppId id;
    std::function<void(Rig&)> sequence;            // tasti tipici (dopo l'apertura)
    std::function<std::string(Workspace&)> probe;   // stato che la sequenza cambia e che deve restare
    int strip{-1};                                  // eact::StripKind della striscia della modalita' (-1: nessuna)
};
template <class T> std::string str(const T& v) { std::ostringstream o; o.precision(15); o << v; return o.str(); }
std::string listText(const std::vector<double>& l) { std::string s; for (double d : l) s += str(d) + ","; return s; }
[[maybe_unused]] void ticks(Rig& r, int n, std::uint32_t ms = 30) {
    for (int i = 0; i < n; ++i) { board::host::advance(ms); if (r.fw.step()) { r.fw.draw(board::graphics()); board::present(); } }
}

std::vector<Mode> modes() {
    std::vector<Mode> m;
    m.push_back({AppId::RunMatrix, [](Rig& r) { r.enter("1+2"); r.key(pk::Up); r.key(pk::Esc); },
                 [](Workspace& w) { return str(w.history.size()) + (w.history.empty() ? "" : w.history.back().expression); }, 0});
#if FXAI_BOARD_PICO_PLUS_2W
    // v1.18.0: CALC › 1-VAR con la barra della fx-CG50; la sonda comprende un risultato di VARS › STAT.
    m.push_back({AppId::Statistics, [](Rig& r) { r.enter("1"); r.enter("2"); r.enter("3"); r.key(pk::F2); r.key(pk::F1); r.key(pk::Esc); },
                 [](Workspace& w) { double n = 0; return listText(w.lists[0]) + (w.statistics.value(stats::VN, n) ? str(n) : std::string("-")); }, 7});
#else
    m.push_back({AppId::Statistics, [](Rig& r) { r.enter("1"); r.enter("2"); r.enter("3"); r.key(pk::F2); r.type("1"); r.key(pk::Esc); },
                 [](Workspace& w) { return listText(w.lists[0]); }, 7});
#endif
    m.push_back({AppId::EActivity, [](Rig& r) { r.key(pk::F2); r.enter("INIT"); r.type("Prova"); r.key(pk::Esc); r.key(pk::F1); },
                 [](Workspace& w) { return w.eactivity.lastFile + "|" + w.eactivity.folder; }, -1});
    m.push_back({AppId::Graph, [](Rig& r) { r.key(pk::F6); r.settle(); r.key(pk::Esc); },
                 [](Workspace& w) { return w.functions[0].expr + str(w.view.xmax); }, 1});
    m.push_back({AppId::Table, [](Rig& r) { r.key(pk::F6); r.settle(); r.key(pk::Esc); },
                 [](Workspace& w) { return str(w.table.generated) + str(w.table.rows.size()); }, 3});
    m.push_back({AppId::Equation, [](Rig& r) { r.key(pk::F2); r.key(pk::F1); r.enter("1"); r.enter("-3"); r.enter("2"); r.key(pk::F1); r.key(pk::Esc); r.key(pk::Esc); r.key(pk::Esc); },
                 [](Workspace& w) { return str(w.equation.degree) + str(w.equation.polynomial[1].value.real()); }, 8});
    m.push_back({AppId::Memory, [](Rig& r) { r.key(pk::Down); r.key(pk::Down); r.key(pk::Up); },
                 [](Workspace& w) { return str(w.memory.recall('B')); }, -1});
    m.push_back({AppId::System, [](Rig& r) { r.key(pk::Down); r.key(pk::Right); },
                 [](Workspace& w) { return str(static_cast<int>(w.settings.angle)); }, -1});
    m.push_back({AppId::Graph3D, [](Rig& r) { r.key(pk::F6); r.settle(); r.key(pk::Esc); },
                 [](Workspace& w) { return w.graph3d.set.fns[0].expr[0]; }, 20});
    m.push_back({AppId::Conversion, [](Rig& r) { r.key(pk::Break); r.enter("2.5"); r.key(pk::F1); },
                 [](Workspace& w) { return str(w.memory.answer()); }, -1});
    // Senza Wi-Fi (PC, dispositivo non configurato) l'invio dice che la rete non c'e': e' il comportamento atteso.
    m.push_back({AppId::AiChat, [](Rig& r) { r.type("ciao"); r.key(pk::Enter); r.settle(); if (r.error() && r.fw.apps().errorDetail().find("rete") != std::string::npos) r.closeError(); },
                 [](Workspace& w) { return str(w.chat.size()); }, -1});
    m.push_back({AppId::Symbols, [](Rig& r) { r.key(pk::Right); r.key(pk::Down); },
                 [](Workspace&) { return std::string("symbols"); }, -1});
#if FXAI_MODE_SPREADSHEET
    m.push_back({AppId::Spreadsheet, [](Rig& r) { r.enter("7"); r.enter("=A1*2"); r.settle(); },
                 [](Workspace& w) { const sheet::Cell* c = w.sheet.sheet.cell(sheet::Ref{0, 2}); return c ? c->input : std::string(); },
                 static_cast<int>(eact::StripKind::SpreadSheet)});
#endif
#if FXAI_MODE_DYNAGRAPH
    m.push_back({AppId::DynaGraph, [](Rig& r) { r.key(pk::F1); ticks(r, 20); r.key(pk::F2); r.key(pk::Break); r.key(pk::Esc); },
                 [](Workspace& w) { return w.dyna.function + str(static_cast<int>(w.dyna.speed)); }, static_cast<int>(eact::StripKind::DynamicGraph)});
#endif
#if FXAI_MODE_RECURSION
    m.push_back({AppId::Recursion, [](Rig& r) { r.key(pk::F2); r.type("2"); r.key(pk::F2); r.type("+1"); r.key(pk::Enter); r.key(pk::F6); r.settle(); r.key(pk::Esc); },
                 [](Workspace& w) { return w.modeVars.recFormula; }, static_cast<int>(eact::StripKind::RecurEditor)});
#endif
#if FXAI_MODE_CONICGRAPHS
    m.push_back({AppId::ConicGraphs, [](Rig& r) { r.key(pk::Down); r.key(pk::Enter); r.key(pk::F6); r.settle(); r.key(pk::Esc); r.key(pk::Esc); },
                 [](Workspace& w) { return str(static_cast<int>(w.conic.model)); }, static_cast<int>(eact::StripKind::ConicsGraph)});
#endif
#if FXAI_MODE_PROGRAM
    m.push_back({AppId::Program, [](Rig& r) { r.key(pk::F3); r.enter("INIT"); r.key(pk::F1); r.settle(); r.key(pk::Esc); },
                 [](Workspace& w) { return w.program.lastFile; }, -1});
#endif
#if FXAI_MODE_FINANCIAL
    m.push_back({AppId::Financial, [](Rig& r) { r.key(pk::Enter); r.enter("90"); r.key(pk::F1); r.key(pk::F6); r.key(pk::Esc); },
                 [](Workspace& w) { return str(w.financial.smplN); }, static_cast<int>(eact::StripKind::Financial)});
#endif
#if FXAI_MODE_LINK
    m.push_back({AppId::Link, [](Rig& r) { r.key(pk::F3); r.closeError(); r.key(pk::Esc); },
                 [](Workspace& w) { return str(w.link.sent); }, -1});
#endif
#if FXAI_MODE_GEOMETRY
    // v1.19.0: F3 Draw > 2 Line Segment, due punti, EXIT (tre oggetti: due punti e il segmento)
    m.push_back({AppId::Geometry, [](Rig& r) { r.key(pk::F3); r.type("2"); r.key(pk::Enter); for (int i = 0; i < 4; ++i) r.key(pk::Right); r.key(pk::Enter); r.key(pk::Esc); },
                 [](Workspace& w) { return str(w.geometry.objects.size()); }, static_cast<int>(eact::StripKind::Geometry)});
#endif
#if FXAI_MODE_PICTUREPLOT
    m.push_back({AppId::PicturePlot, [](Rig& r) { r.key(pk::F2); r.key(pk::Right); r.key(pk::F2); },
                 [](Workspace& w) { return str(w.pictplot.points.size()); }, static_cast<int>(eact::StripKind::PicturePlot)});
#endif
#if FXAI_MODE_DISTRIBUTION
    // v1.18.0: Distribution (Base): Normal C.D con i valori iniziali, CALC, risultato in Ans.
    m.push_back({AppId::Distribution, [](Rig& r) { r.key(pk::Enter); r.key(pk::F1); r.settle(); r.key(pk::Esc); r.key(pk::Esc); },
                 [](Workspace& w) { return str(w.distribution.kind) + str(w.distribution.hasResult) + str(w.distribution.lastResult); }, -1});
#endif
#if FXAI_MODE_PYTHON
    m.push_back({AppId::Python, [](Rig& r) { r.key(pk::F5); r.enter("1+2"); r.settle(); r.key(pk::Esc); },
                 [](Workspace& w) { return str(w.python.history.size()); }, -1});
#endif
    return m;
}

enum Case : int { Cold, AfterReset, V11Valid, V11Corrupt, V11Truncated, SramOnly, NearOom, NoCard, Card, CaseCount };
const char* const kCaseNames[] = {"cold", "after-reset", "v11", "v11-corrupt", "v11-truncated", "sram-only", "near-oom", "no-card", "card"};

std::vector<void*>* g_hog;
// La microSD di ogni caso vive nei pool simulati del caso: si crea dopo la
// simulazione e si distrugge prima della successiva.
std::unique_ptr<files::RamFileSystem> g_card;
void exhaust() {
    for (std::size_t chunk : {65536u, 4096u, 512u, 64u, 16u})
        while (g_hog->size() < g_hog->capacity()) {
            void* p = mem::tryAlloc(mem::Policy::PREFER_SRAM, chunk);
            if (!p) break;
            g_hog->push_back(p);
        }
}
void release() { for (void* p : *g_hog) mem::free(p); g_hog->clear(); }
// Allocazioni vive e byte usati dei pool. Una perdita aumenta le allocazioni o
// i byte a ogni visita; un buffer conservato che si rialloca con una capacita'
// diversa cambia i byte di un blocco (TLSF arrotonda a 16 B): fino a 64 B di
// differenza tra due visite non sono una perdita.
struct Live { std::size_t allocations, bytes; };
Live live() {
    const auto s = mem::stats(mem::Pool::SRAM), p = mem::stats(mem::Pool::PSRAM_BOARD);
    return {s.allocations + p.allocations, s.used + p.used};
}
bool sameLive(const Live& a, const Live& b) { return a.allocations == b.allocations && b.bytes <= a.bytes + 64; }
// Pixel della barra F1-F6 (sfondo dei tasti) nelle ultime 20 righe.
bool fkeyBarDrawn() {
    std::vector<std::uint16_t> px(static_cast<std::size_t>(ui::ScreenW * ui::SoftKeyH));
    board::graphics().copyTo(gfx::Rect{0, ui::ScreenH - ui::SoftKeyH, ui::ScreenW, ui::SoftKeyH}, px.data(), ui::ScreenW);
    std::size_t keyBg = 0;
    for (std::uint16_t p : px) keyBg += p == ui::theme::KeyBg;
    return keyBg >= 400;                                         // almeno un tasto disegnato (~700 pixel l'uno)
}
bool openedController(Rig& r, AppId id) {
    App* a = r.fw.apps().active();
    return r.fw.apps().activeId() == id && a && dynamic_cast<UnavailableApp*>(a) == nullptr && a->id() == id;
}
void openByKey(Rig& r, AppId id) {
    r.home();
    r.closeError();
    r.type(std::string(1, static_cast<char>(std::tolower(static_cast<unsigned char>(appInfo(id).key[0])))));
}
std::string label(const Mode& m, int c, const char* what) { return std::string(appInfo(m.id).name) + " [" + kCaseNames[c] + "] " + what; }
#define KCHECK(cond, what) ::testutil::report(static_cast<bool>(cond), #cond, __FILE__, __LINE__, label(mode, c, what))

void prepare(int c) {
    storage::RecordStore store(board::storageFlash());
    store.eraseAll();
    board::host::setFileSystem(nullptr);
    g_card.reset();
    if (c == SramOnly || c == NearOom) { mem::testing::simulateSram(c == NearOom ? 768u * 1024u : 2u * 1024u * 1024u); mem::testing::simulatePsram(0); }
    else { mem::testing::simulateSram(4u * 1024u * 1024u); mem::testing::simulatePsram(board::config::kHasPsram ? 8u * 1024u * 1024u : 0); }
    g_card = std::make_unique<files::RamFileSystem>();
    board::host::setFileSystem(c == NoCard ? nullptr : g_card.get());
    if (c == V11Valid || c == V11Corrupt || c == V11Truncated) {
        std::vector<std::uint8_t> rec(kV1160, kV1160 + sizeof kV1160);
        if (c == V11Corrupt) for (std::size_t i = rec.size() / 3; i < rec.size() / 3 + 24; ++i) rec[i] ^= 0xA5;
        if (c == V11Truncated) rec.resize(rec.size() / 2);
        CHECK(store.save(rec));                                  // il CRC dello slot e' giusto: e' il contenuto a non valere
    }
    if (c == AfterReset) {
        Rig r;
        r.ws().memory.store('B', 9);
        r.ws().functions[0].expr = "x^3";
        r.type("g"); r.key(pk::F5); r.key(pk::Enter);             // SYSTEM > RESET > EXE
        CHECK(r.ws().memory.recall('B') == 0 && r.ws().functions[0].expr == "sin(x)");
        r.home();
        CHECK(r.fw.saveNow());
    }
}
void finishCase() {
    board::host::setFileSystem(nullptr);
    g_card.reset();
}

void runCase(const Mode& mode, int c) {
    prepare(c);
    {
        auto r = std::make_unique<Rig>();
        if (c == V11Valid) KCHECK(r->fw.loadedFromFlash() && r->ws().modeVars.recFormula == "2rec_an+1", "record v11 letto");
        if (c == V11Corrupt || c == V11Truncated) KCHECK(!r->fw.loadedFromFlash() && !r->error(), "record rifiutato senza crash, workspace vuoto");
        r->closeError();
        // K1 + K2
        openByKey(*r, mode.id);
        KCHECK(openedController(*r, mode.id), "K1 si apre da MENU");
        r->fw.draw(board::graphics());
        KCHECK(!r->error(), "K2 schermata iniziale senza errori");
        KCHECK(fkeyBarDrawn(), "K2 barra F1-F6");
        // K5: entrare e uscire non lascia memoria (la prima visita crea le cache durevoli;
        // i salvataggi in sospeso si fanno prima della misura)
        r->home();
        openByKey(*r, mode.id);
        r->home();
        (void)r->fw.saveNow();
        const Live a = live();
        openByKey(*r, mode.id);
        r->fw.draw(board::graphics());
        r->home();
        const Live a2 = live();                                // prima del messaggio (che alloca)
        KCHECK(sameLive(a, a2), "K5 entrata/uscita senza perdite");
        // K3: sequenza tipica
        openByKey(*r, mode.id);
        if (c == NearOom) exhaust();
        mode.sequence(*r);
        r->settle();
        if (c == NearOom) {
            // con la memoria esaurita: al piu' un Memory ERROR, mai un crash; poi la modalita' torna usabile
            // (il controllo si registra dopo aver liberato la memoria: anche il test alloca)
            const bool memoryError = !r->error() || r->errorTitle() == "Memory ERROR" || r->errorTitle() == "ERROR" || r->errorTitle() == "Data ERROR";
            r->closeError();
            release();
            KCHECK(memoryError, "K3 Memory ERROR senza crash");
            r->fw.draw(board::graphics());
            r->home();
            openByKey(*r, mode.id);
            r->fw.draw(board::graphics());
            KCHECK(openedController(*r, mode.id) && !r->error(), "K3 usabile dopo la memoria esaurita");
            r->home();
            KCHECK(mem::checkIntegrity() && mem::invalidFrees() == 0, "K5 allocatori integri");
            return;
        }
        if (c == NoCard) {
            // senza microSD le modalita' con i file dicono che l'archivio manca; nient'altro
            KCHECK(!r->error() || r->errorDetail().find("microSD") != std::string::npos || r->errorDetail().find("archivio") != std::string::npos ||
                   r->errorDetail().find("No SD") != std::string::npos || r->errorDetail().find("non disponibile") != std::string::npos, "K3 senza microSD solo l'avviso dell'archivio");
            r->closeError();
        } else {
            KCHECK(!r->error(), (std::string("K3 sequenza tipica senza errori: ") + r->errorTitle() + " " + r->errorDetail()).c_str());
            r->closeError();
        }
        r->fw.draw(board::graphics());
        const std::string probe = mode.probe(r->ws());
        // K4: MENU esce
        r->key(pk::Tab);
        r->closeError();
        KCHECK(r->fw.apps().activeId() == AppId::MainMenu, "K4 MENU torna al menu principale");
        // K6 (salvataggio): l'autosalvataggio del firmware (cambio di modalita' o 2,5 s
        // senza tasti) deve bastare: la modalita' segna i dati come modificati. Il
        // record nella flash, letto subito, ha lo stato della sequenza.
        board::host::advance(3000);
        r->settle();
        {
            std::vector<std::uint8_t> blob;
            auto saved = std::make_unique<Workspace>();
            const bool ok = storage::RecordStore(board::storageFlash()).load(blob) && deserializeWorkspace(blob.data(), blob.size(), *saved);
            KCHECK(ok && mode.probe(*saved) == probe, "K6 lo stato e' nel record dopo l'autosalvataggio");
        }
        // K5 dopo la sequenza: un'altra visita non lascia memoria
        openByKey(*r, mode.id);
        r->home();
        (void)r->fw.saveNow();
        const Live b = live();
        openByKey(*r, mode.id);
        r->fw.draw(board::graphics());
        r->home();
        const Live b2 = live();
        KCHECK(sameLive(b, b2), "K5 visita dopo la sequenza senza perdite");
        KCHECK(mem::checkIntegrity() && mem::invalidFrees() == 0, "K5 allocatori integri");
        // K6: lo stato resta dopo uscita e rientro ...
        openByKey(*r, mode.id);
        KCHECK(mode.probe(r->ws()) == probe, "K6 stato dopo uscita e rientro");
        r->home();
        // K4 striscia di eActivity: MENU e SHIFT ' tornano al documento
        if (mode.strip >= 0 && c != NoCard && isOpen(appInfo(AppId::EActivity).status)) {
            openByKey(*r, AppId::EActivity);
            r->key(pk::F2); r->enter("STRIP");                    // NEW
            r->key(pk::F2);                                      // STRIP
            for (int i = 0; i < mode.strip; ++i) r->key(pk::Down);
            r->key(pk::Enter); r->key(pk::Enter); r->key(pk::Enter);   // tipo, titolo vuoto, apri
            r->settle();
            KCHECK(r->fw.apps().activeId() == mode.id && r->fw.apps().context().inStrip(mode.id) && !r->error(), "K4 la striscia apre la modalita'");
            r->fw.draw(board::graphics());
            r->key(pk::F7); r->type("'");                       // SHIFT '
            KCHECK(r->fw.apps().activeId() == AppId::EActivity && !r->fw.apps().context().strip().active, "K4 SHIFT ' torna a eActivity");
            r->key(pk::Enter);
            r->settle();
            KCHECK(r->fw.apps().activeId() == mode.id, "K4 la striscia si riapre");
            r->key(pk::Tab);                                     // MENU
            KCHECK(r->fw.apps().activeId() == AppId::EActivity && !r->fw.apps().context().strip().active, "K4 MENU torna a eActivity");
            r->key(pk::Esc); r->key(pk::F6);                     // chiude senza salvare
            r->home();
            KCHECK(mode.probe(r->ws()) == probe, "K4 i dati del menu principale restano dopo la striscia");
        }
        // ... e dopo un riavvio simulato: l'autosalvataggio del firmware (cambio di
        // modalita' o 2,5 s senza tasti) deve bastare, la modalita' deve aver segnato
        // i dati come modificati; poi un nuovo firmware sulla stessa flash.
        r->home();
        board::host::advance(3000);
        r->settle();
        KCHECK(!r->fw.apps().context().dirty(), "K6 autosalvataggio fatto");
        r.reset();
        r = std::make_unique<Rig>();
        r->closeError();
        openByKey(*r, mode.id);
        KCHECK(openedController(*r, mode.id) && mode.probe(r->ws()) == probe, "K6 stato dopo il riavvio");
        r->home();
    }
    finishCase();
}
} // namespace

int main() {
    board::init();
    std::vector<void*> hog;
    hog.reserve(1u << 16);
    g_hog = &hog;
    const auto all = modes();
    // Ogni modalita' aperta del registro ha il suo caso (una modalita' nuova non puo' mancare).
    std::size_t open = 0;
    for (std::size_t i = 0; i < AppCount; ++i) {
        const AppInfo& info = appInfoAt(i);
        if (!isOpen(info.status)) continue;
        ++open;
        bool found = false;
        for (const Mode& m : all) found = found || m.id == info.id;
        ::testutil::report(found, "every open mode has an init case", __FILE__, __LINE__, info.name);
    }
    CHECK(open == all.size());
    // v1.18.0: sulla Plus anche Distribution (Base).
    CHECK(open == (board::config::kIsPicoPlus2W ? 12u + 10u + (features::kDistribution ? 1u : 0u) : 12u) || !board::config::kIsPicoPlus2W);
    // Le cache statiche delle schermate nascono nell'heap vero, prima dei pool simulati.
    {
        storage::RecordStore(board::storageFlash()).eraseAll();
        Rig warm;
        for (const Mode& mode : all) {
            openByKey(warm, mode.id);
            warm.fw.draw(board::graphics());
            mode.sequence(warm);
            warm.settle();
            warm.closeError();
            warm.fw.draw(board::graphics());
            warm.home();
            warm.closeError();
        }
    }
    for (const Mode& mode : all)
        for (int c = 0; c < CaseCount; ++c) runCase(mode, c);
    mem::testing::reset();
    board::host::setFileSystem(&board::host::ramCard());
    return testutil::finish("apps_init_test");
}
