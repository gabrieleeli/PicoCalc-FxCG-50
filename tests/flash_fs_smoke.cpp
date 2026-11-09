// Host tests for the flash partition layout and the LittleFS glue, running on
// an emulated NOR flash with the RP2350 rules: 4 KiB erase, 256 B program,
// program can only clear bits, writes outside [diag sector, end of LittleFS)
// are forbidden (firmware and BTstack areas are never touched).
#include "board_config.hpp"
#include "flash_fs.hpp"
#include "test_support.hpp"

#include <cstring>
#include <vector>

namespace {
namespace cfg = board::config;

class EmulatedNor final : public fx::storage::FlashDevice {
public:
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(cfg::kFlashSize, 0xFF);
    std::size_t erases{}, programs{}, violations{};
    bool read(std::size_t offset, void* dst, std::size_t n) override {
        if (offset + n > bytes.size()) return false;
        std::memcpy(dst, bytes.data() + offset, n);
        return true;
    }
    bool erase(std::size_t offset, std::size_t n) override {
        if (!allowed(offset, n) || offset % 4096 || n % 4096) { ++violations; return false; }
        std::memset(bytes.data() + offset, 0xFF, n);
        ++erases;
        return true;
    }
    bool program(std::size_t offset, const void* src, std::size_t n) override {
        if (!allowed(offset, n) || offset % 256 || n % 256) { ++violations; return false; }
        const auto* in = static_cast<const std::uint8_t*>(src);
        for (std::size_t i = 0; i < n; ++i) bytes[offset + i] &= in[i]; // NOR: 1 -> 0 only
        ++programs;
        return true;
    }
private:
    static bool allowed(std::size_t offset, std::size_t n) {
        return offset >= cfg::kFlashDiagOffset && offset + n <= cfg::kFlashFsOffset + cfg::kFlashFsSize;
    }
};
} // namespace

int main() {
    const auto l = fx::storage::layout();
    FX_CHECK(l.flashSize == cfg::kFlashSize);
    FX_CHECK(l.diagOffset == l.firmwarePartition);
    FX_CHECK(l.fsOffset == l.diagOffset + 4096);
    FX_CHECK(l.btReservedOffset == l.flashSize - 8192);
    FX_CHECK(l.fsOffset + l.fsSize == l.btReservedOffset);
    if (cfg::kIsPicoPlus2W) FX_CHECK(l.fsSize > 12u * 1024u * 1024u - 16384u);
    else FX_CHECK(l.fsSize > 2u * 1024u * 1024u - 16384u && l.flashSize == 4u * 1024u * 1024u);

    EmulatedNor nor;
    fx::storage::testing::useDevice(&nor);
    FX_CHECK(fx::storage::mount());
    auto s = fx::storage::stats();
    FX_CHECK(s.mounted && s.formattedAtBoot && s.bootCount == 1);
    FX_CHECK(s.blockSize == 4096 && s.blockCount == cfg::kFlashFsSize / 4096);

    const char text[] = "angle=DEG;history=on";
    FX_CHECK(fx::storage::writeFile("/settings", text, sizeof text));
    char back[64] = {};
    FX_CHECK(fx::storage::readFile("/settings", back, sizeof back) == static_cast<long>(sizeof text));
    FX_CHECK(std::strcmp(back, text) == 0);
    FX_CHECK(fx::storage::readFile("/missing", back, sizeof back) == -1);

    // Persistence across a remount (simulated reboot).
    fx::storage::unmount();
    FX_CHECK(fx::storage::mount());
    s = fx::storage::stats();
    FX_CHECK(!s.formattedAtBoot && s.bootCount == 2);
    std::memset(back, 0, sizeof back);
    FX_CHECK(fx::storage::readFile("/settings", back, sizeof back) == static_cast<long>(sizeof text));

    // Fill the file system: writes eventually fail cleanly, nothing crashes.
    std::vector<std::uint8_t> blob(256u * 1024u, 0x5A);
    int written = 0;
    char name[32];
    for (int i = 0; i < 200; ++i) {
        std::snprintf(name, sizeof name, "/blob%03d", i);
        if (!fx::storage::writeFile(name, blob.data(), blob.size())) break;
        ++written;
    }
    s = fx::storage::stats();
    FX_CHECK(written > 0 && s.usedBytes > 0 && s.peakUsedBytes >= s.usedBytes);
    FX_CHECK(written < 200 || cfg::kFlashFsSize > 200u * 256u * 1024u);
    for (int i = 0; i < written; ++i) {
        std::snprintf(name, sizeof name, "/blob%03d", i);
        FX_CHECK(fx::storage::removeFile(name));
    }
    FX_CHECK(fx::storage::stats().usedBytes < s.usedBytes);
    FX_CHECK(nor.violations == 0); // never touched firmware or BTstack areas

    // A corrupted partition is reformatted on the next mount.
    fx::storage::unmount();
    for (std::size_t i = cfg::kFlashFsOffset; i < cfg::kFlashFsOffset + 64u * 1024u; ++i)
        nor.bytes[i] = static_cast<std::uint8_t>(i * 31u);
    FX_CHECK(fx::storage::mount());
    FX_CHECK(fx::storage::stats().formattedAtBoot);

    // The flash/PSRAM interaction test needs the real chip.
    FX_CHECK(!fx::storage::testPsramAfterFlashWrite().ran);
    fx::storage::testing::useDevice(nullptr);
    std::puts("Flash layout + LittleFS on emulated NOR: PASS");
    return 0;
}
