// SYSTEM (v1.9.0): DIAG runs the memory/PSRAM/allocator/flash/display tests
// next to the microSD, internal flash and GRAPH tests of v1.8.0; INFO shows
// the pools; the "Archivio file" setting selects microSD or internal flash.
// On the Pico Plus 2 W a missing PSRAM is reported as a failure (degraded
// mode); on the Pico 2 W it is simply "not present".
#include "app_rig.hpp"
#include "board_config.hpp"
#include "diagnostics.hpp"
#include "mem_pools.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include <string>

using namespace fx;
using SA = app::SystemApp;

namespace {
struct Summary { int fails{}; bool mem{}, psr{}, alc{}, fls{}, lcd{}, sd{}, intl{}, gph{}; std::string failText; };
Summary diagnose(testrig::Rig& r) {
    Summary s;
    r.home(); r.type("g");
    auto* sys = r.active<SA>();
    CHECK(sys != nullptr);
    if (!sys) return s;
    r.key(pckey::F2);                                                     // DIAG
    for (const auto& l : sys->diagnostics()) {
        if (!l.first) { ++s.fails; s.failText += l.second + " | "; }
        const std::string& t = l.second;
        s.mem = s.mem || t.rfind("MEM ", 0) == 0;
        s.psr = s.psr || t.rfind("PSR ", 0) == 0;
        s.alc = s.alc || (t.rfind("ALC ", 0) == 0 && t.find(" 0 corruzioni") != std::string::npos);
        s.fls = s.fls || (t.rfind("FLS ", 0) == 0 && t.find("verifica OK") != std::string::npos);
        s.lcd = s.lcd || t.rfind("LCD ", 0) == 0;
        s.sd = s.sd || (t.rfind("SD  ", 0) == 0 && t.find("dati identici") != std::string::npos);
        s.intl = s.intl || (t.rfind("INT ", 0) == 0 && t.find("dati identici") != std::string::npos);
        s.gph = s.gph || (t.rfind("GPH ", 0) == 0 && t.find("radici") != std::string::npos);
    }
    r.key(pckey::Esc);
    return s;
}
} // namespace

int main() {
    board::init();
    storage::RecordStore(board::storageFlash()).eraseAll();
    testrig::Rig r;
    // ---- complete DIAG with the board's memory as expected
    if (board::config::kHasPsram) mem::testing::simulatePsram(8u << 20);
    Summary s = diagnose(r);
    if (s.fails) std::printf("  unexpected failures: %s\n", s.failText.c_str());
    CHECK(s.fails == 0);
    CHECK(s.mem && s.psr && s.alc && s.fls && s.lcd && s.sd && s.intl && s.gph);
    // the test files are removed from both stores
    std::vector<files::Entry> list;
    if (board::host::ramCard().list("PICOCALC", list) == files::Status::Ok)
        for (const auto& e : list) CHECK(e.name != "DIAG");
    files::FileSystem* internal = r.platform.internalFiles();
    CHECK(internal != nullptr);
    if (internal && internal->list("PICOCALC", list) == files::Status::Ok)
        for (const auto& e : list) CHECK(e.name != "DIAG");

    // ---- Pico Plus 2 W whose PSRAM did not pass its self-test: reported, the rest works
    if (board::config::kHasPsram) {
        mem::testing::simulatePsram(0);
        s = diagnose(r);
        CHECK(s.fails >= 2 && s.failText.find("PSRAM") != std::string::npos);
        CHECK(s.alc && s.fls && s.sd && s.intl && s.gph);
        mem::testing::simulatePsram(8u << 20);
    }

    // ---- INFO page with the pools
    r.home(); r.type("g");
    r.key(pckey::F1);
    CHECK(!r.error());
    r.key(pckey::Esc);

    // ---- "Archivio file": Right on the choice switches to the internal flash
    auto* sys = r.active<SA>();
    CHECK(sys != nullptr);
    for (int i = 0; i < 7; ++i) r.key(pckey::Down);                       // Angolo .. Luce tastiera -> Archivio file
    r.key(pckey::Right);
    CHECK(r.ws().settings.fileStore == Settings::FileStore::Internal);
    r.key(pckey::Left);
    CHECK(r.ws().settings.fileStore == Settings::FileStore::MicroSd);

    // ---- the allocator stress kernel on its own (host: single thread)
    const auto st = diag::allocatorStress(1234, 5000, 2048);
    CHECK(st.operations == 5000 && st.corruptions == 0 && st.allocations > 1000);
    CHECK(mem::checkIntegrity());
    mem::testing::reset();
    return testutil::finish("system_diag_test");
}
