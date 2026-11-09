// Board profile and flash layout (v1.9.0): partitions computed from the
// flash size never overlap, the last flash sector is never written (RP2350-E10
// UF2 workaround), the Pico 2 W keeps the v1.8.0 record location, records
// written by v1.8.0 (format 4) load unchanged, and on the Pico Plus 2 W a
// v1.8.0 workspace found at the old location is migrated once.
#include "app_rig.hpp"
#include "board_config.hpp"
#include "limits.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include <cstring>
#include <vector>

using namespace fx;
namespace cfg = board::config;

namespace {
const std::uint8_t kV180Record[] = {
#include "fixtures/workspace_v1_8_0.inc"
};
void checkV180(const Workspace& ws) {
    CHECK(ws.settings.angle == AngleMode::Degrees);
    CHECK(ws.memory.recall('A') == 5.0 && ws.memory.answer() == 42.0);
    CHECK(ws.history.size() == 1 && ws.history[0].expression == "40+2");
    CHECK(ws.functions[0].expr == "(x)^(2)-1" && ws.functions[1].expr == "2x+1");
    CHECK(ws.lists[0].size() == 3 && ws.lists[0][2] == 3.5);
    CHECK(ws.table.start == -2 && ws.table.step == 0.5);
    CHECK(ws.chat.size() == 1 && ws.chat[0].text == "Ciao");
    // fields added in v1.9.0 (format 5) take their defaults
    CHECK(ws.settings.fileStore == Settings::FileStore::MicroSd);
    CHECK(ws.graph3d.model.z[0] == "sin(x)cos(y)" && ws.graph3d.model.grid == 40);
}
} // namespace

int main() {
    // ================================================================ flash layout
    CHECK(cfg::kFirmwarePartitionBytes == (cfg::kIsPicoPlus2W ? 4u : 2u) * 1024u * 1024u);
    CHECK(cfg::kFlashDiagOffset == cfg::kFirmwarePartitionBytes);
    CHECK(cfg::kFlashFsOffset == cfg::kFlashDiagOffset + cfg::kFlashSectorBytes);
    CHECK(cfg::kFlashFsOffset + cfg::kFlashFsSize == cfg::kFlashBtOffset);
    CHECK(cfg::kFlashBtOffset + cfg::kFlashBtBytes == cfg::kFlashRecordOffset);
    CHECK(cfg::kFlashRecordOffset % cfg::kFlashSectorBytes == 0 && cfg::kFlashFsSize % cfg::kFlashSectorBytes == 0);
    if (cfg::kIsPicoPlus2W) {
        CHECK(cfg::kFlashSize == 16u * 1024u * 1024u && cfg::kHasPsram);
        CHECK(cfg::kFlashRecordOffset + cfg::kFlashRecordSize == cfg::kFlashSize - cfg::kFlashTailGuardBytes);   // E10 guard
        CHECK(cfg::kFlashFsSize > 10u * 1024u * 1024u);                                   // ~11 MB of LittleFS
        CHECK(cfg::kLegacyRecordOffset + cfg::kLegacyRecordSize <= cfg::kFirmwarePartitionBytes);   // never written
        CHECK(cfg::kPsramCsPin == 47 && cfg::kGpioCount == 48 && cfg::kAdcBasePin == 40);
    } else {
        CHECK(cfg::kFlashSize == 4u * 1024u * 1024u && !cfg::kHasPsram);
        CHECK(cfg::kFlashRecordOffset == cfg::kFlashSize - 128u * 1024u);                 // v1.8.0 location
        CHECK(cfg::kFlashFsSize > 1800u * 1024u);
        CHECK(cfg::kPsramCsPin == -1 && cfg::kGpioCount == 30 && cfg::kAdcBasePin == 26);
    }
    // ================================================================ flash chip check (wrong UF2)
    CHECK(cfg::flashBytesFromJedec(0xEF, 0x16) == 4u * 1024u * 1024u);     // W25Q32 (Pico 2 W)
    CHECK(cfg::flashBytesFromJedec(0xEF, 0x18) == 16u * 1024u * 1024u);    // W25Q128 class (Pico Plus 2 W)
    CHECK(cfg::flashBytesFromJedec(0xC8, 0x18) == 16u * 1024u * 1024u);    // GigaDevice, same encoding
    CHECK(cfg::flashBytesFromJedec(0xFF, 0x18) == 0 && cfg::flashBytesFromJedec(0x00, 0x00) == 0);   // no reply
    CHECK(cfg::flashBytesFromJedec(0x1F, 0x01) == 0);                      // other encoding: unknown
    CHECK(cfg::usableFlashBytes(0) == cfg::kFlashSize);                    // unknown chip: layout of the build
    CHECK(cfg::usableFlashBytes(64u * 1024u * 1024u) == cfg::kFlashSize);  // larger chip: layout of the build
    CHECK(cfg::usableFlashBytes(cfg::kFlashSize) == cfg::kFlashSize);
    if (cfg::kIsPicoPlus2W) {
        // The Plus image on a 4 MB chip: every storage region lies beyond it and
        // is disabled; only the (read-only) v1.8.0 location stays inside.
        const std::size_t limit = cfg::usableFlashBytes(4u * 1024u * 1024u);
        CHECK(limit == 4u * 1024u * 1024u);
        CHECK(cfg::kFlashDiagOffset + cfg::kFlashSectorBytes > limit && cfg::kFlashFsOffset + cfg::kFlashFsSize > limit &&
              cfg::kFlashRecordOffset + cfg::kFlashRecordSize > limit);
        CHECK(cfg::kLegacyRecordOffset + cfg::kLegacyRecordSize <= limit);
    }
    {
        const auto chip = board::flashChip();                              // simulated module: its own chip
        CHECK(chip.bytes == cfg::kFlashSize && chip.manufacturer == 0xEF);
    }
    // PicoCalc wiring identical on both modules (v1.8.0 driver values)
    CHECK(cfg::kLcdSck == 10 && cfg::kLcdMosi == 11 && cfg::kLcdMiso == 12 && cfg::kLcdCs == 13 && cfg::kLcdDc == 14 && cfg::kLcdReset == 15);
    CHECK(cfg::kKeyboardSda == 6 && cfg::kKeyboardScl == 7 && cfg::kKeyboardAddress == 0x1F && cfg::kKeyboardI2cHz == 10000);
    CHECK(cfg::kSdSck == 18 && cfg::kSdMosi == 19 && cfg::kSdMiso == 16 && cfg::kSdCs == 17 && cfg::kSdDetect == 22);
    CHECK(!cfg::gpioHasAdc(26) == cfg::kIsPicoPlus2W);                                  // GP26 is analog only on the RP2350A

    // ================================================================ record store and the reserved tail
    {
        // Pico 2 W geometry: 2 x 64 KB at the end of the flash, last 4 KB reserved.
        storage::RamFlash flash(128 * 1024);
        flash.setReservedTail(4096);
        storage::RecordStore store(flash);
        CHECK(store.capacity() == 64u * 1024u - 256u - 4096u);
        std::vector<std::uint8_t> big(store.capacity(), 0xA5), back;
        // Both slots are written alternately: neither save touches the last sector.
        for (int i = 0; i < 4; ++i) { big[0] = static_cast<std::uint8_t>(i); CHECK(store.save(big)); }
        bool tailBlank = true;
        for (std::size_t i = 128 * 1024 - 4096; i < 128 * 1024; ++i) tailBlank = tailBlank && flash.raw()[i] == 0xFF;
        CHECK(tailBlank);
        CHECK(store.load(back) && back == big);
        big.push_back(1);
        CHECK(!store.save(big));                                              // one byte too many
        // A record that an older firmware stored into the tail (v1.8.0 saves were
        // up to 65280 bytes) is still read.
        storage::RamFlash old(128 * 1024);
        std::vector<std::uint8_t> legacyBig(65000, 0x3C);
        {
            storage::RecordStore v180(old);                                   // no reserved tail: v1.8.0 behaviour
            CHECK(v180.save({1, 2, 3}) && v180.save(legacyBig));              // the second save goes to slot B
        }
        old.setReservedTail(4096);
        storage::RecordStore now(old);
        std::vector<std::uint8_t> got;
        CHECK(now.load(got) && got == legacyBig && now.activeSlot() == 1);
        // the device refuses writes into the tail even when asked directly
        CHECK(!old.erase(128 * 1024 - 4096, 4096));
        std::uint8_t page[256] = {};
        CHECK(!old.program(128 * 1024 - 256, page, 256));
    }

    // ================================================================ v1.8.0 record (format 4)
    {
        Workspace ws;
        CHECK(deserializeWorkspace(kV180Record, sizeof kV180Record, ws));
        checkV180(ws);
        // saved again in format 5 and read back with the new fields
        ws.settings.fileStore = Settings::FileStore::Internal;
        ws.graph3d.model.z[2] = "x*y"; ws.graph3d.model.selected[2] = true; ws.graph3d.yaw = 100;
        const auto blob = serializeWorkspace(ws);
        CHECK(blob[4] == 5 && blob[5] == 0);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
        CHECK(back.settings.fileStore == Settings::FileStore::Internal && back.graph3d.model.z[2] == "x*y" && back.graph3d.yaw == 100);
        CHECK(back.lists[0] == ws.lists[0] && back.history.size() == 1);
        // corrupt 3D data is rejected like any other field
        Workspace bad = ws;
        bad.graph3d.model.grid = 200;
        const auto badBlob = serializeWorkspace(bad);
        Workspace sink;
        CHECK(!deserializeWorkspace(badBlob.data(), badBlob.size(), sink));
    }

    // ================================================================ migration of a v1.8.0 workspace
    board::init();
    {
        storage::RamFlash legacy(128 * 1024);
        {
            storage::RecordStore old(legacy);
            CHECK(old.save(std::vector<std::uint8_t>(kV180Record, kV180Record + sizeof kV180Record)));
        }
        const auto legacyBefore = legacy.raw();
        const std::size_t legacyWrites = legacy.writes();
        storage::RecordStore(board::storageFlash()).eraseAll();               // new store empty: first boot of v1.9.0
        {
            testrig::Rig r(&legacy);
            CHECK(r.fw.loadedFromFlash() && r.fw.migrated());
            checkV180(r.ws());
            r.settle();
            board::host::advance(3000);
            r.settle();                                                       // autosave to the new store
            CHECK(r.fw.saves() >= 1);
        }
        CHECK(legacy.raw() == legacyBefore && legacy.writes() == legacyWrites);   // the old store is never written
        {
            testrig::Rig r(&legacy);                                          // next boot: the new store wins
            CHECK(r.fw.loadedFromFlash() && !r.fw.migrated());
            checkV180(r.ws());
        }
        // an existing record is never replaced by the legacy one
        {
            testrig::Rig r(&legacy);
            r.ws().memory.store('A', 9.0);
            r.fw.apps().context().markDirty();
            board::host::advance(3000);
            r.settle();
        }
        {
            testrig::Rig r(&legacy);
            CHECK(!r.fw.migrated() && r.ws().memory.recall('A') == 9.0);
        }
        storage::RecordStore(board::storageFlash()).eraseAll();
    }
    return testutil::finish("board_profile_test");
}
