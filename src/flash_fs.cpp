#include "flash_fs.hpp"

#include "board_config.hpp"
#include "mem_pools.hpp"
#include "psram.hpp"

#include "lfs.h"

#include <cstdio>
#include <cstring>

#if FXAI_ON_DEVICE
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"
#include "pico/platform.h"
#include "pico/time.h"
extern "C" char __flash_binary_end; // end of the image in flash (SDK linker script)
#endif

namespace fx::storage {
namespace {
namespace cfg = board::config;
constexpr std::size_t kPageBytes = 256;
constexpr std::size_t kSectorBytes = 4096;
constexpr std::size_t kCacheBytes = 512;
constexpr std::size_t kLookaheadBytes = 128;
constexpr std::size_t kBounceBytes = 4096;
constexpr const char* kBootCounterFile = "/boot_count";

static_assert(cfg::kFlashDiagOffset % kSectorBytes == 0, "diagnostic sector must be sector aligned");

#if FXAI_ON_DEVICE
// --- RP2350 internal flash through flash_safe_execute() -----------------------
alignas(4) std::uint8_t g_bounce[kBounceBytes]; // SRAM: readable while XIP is off

struct FlashJob {
    std::uint32_t offset;
    std::size_t bytes;
    bool erase;
    bool m1Intact;
};

void __no_inline_not_in_flash_func(runFlashJob)(void* param) {
    auto* job = static_cast<FlashJob*>(param);
    if (job->erase) flash_range_erase(job->offset, job->bytes);
    else flash_range_program(job->offset, g_bounce, job->bytes);
    // The SDK restores M1 timing/rcmd/rfmt (FLASH_DEVINFO has no CS1 entry);
    // verify every M1 register and XIP_CTRL.WRITABLE_M1 before XIP resumes.
    job->m1Intact = psram::verifyConfigurationFromSafeZone();
}

class DeviceFlash final : public FlashDevice {
public:
    bool read(std::size_t offset, void* dst, std::size_t bytes) override {
        if (offset + bytes > cfg::kFlashSize) return false;
        // flash_range_program/erase flush the XIP cache, so cached reads are fresh.
        std::memcpy(dst, reinterpret_cast<const void*>(cfg::kFlashXipBase + offset), bytes);
        return true;
    }
    bool erase(std::size_t offset, std::size_t bytes) override {
        if (!writable(offset, bytes) || offset % kSectorBytes || bytes % kSectorBytes) return false;
        FlashJob job{static_cast<std::uint32_t>(offset), bytes, true, true};
        return execute(job);
    }
    bool program(std::size_t offset, const void* src, std::size_t bytes) override {
        if (!writable(offset, bytes) || offset % kPageBytes || bytes % kPageBytes) return false;
        const auto* in = static_cast<const std::uint8_t*>(src);
        while (bytes) {
            const std::size_t chunk = bytes < kBounceBytes ? bytes : kBounceBytes;
            std::memcpy(g_bounce, in, chunk); // copy out of PSRAM/flash before XIP stops
            FlashJob job{static_cast<std::uint32_t>(offset), chunk, false, true};
            if (!execute(job)) return false;
            offset += chunk;
            in += chunk;
            bytes -= chunk;
        }
        return true;
    }
    bool lastM1Intact{true};
    std::uint32_t lastDurationUs{};

private:
    static bool writable(std::size_t offset, std::size_t bytes) {
        // Never touch the firmware partition or the BTstack area.
        return offset >= cfg::kFlashDiagOffset && offset + bytes <= cfg::kFlashFsOffset + cfg::kFlashFsSize;
    }
    bool execute(FlashJob& job) {
        // Dirty PSRAM lines are committed by the SDK (xip_cache_clean_all) before
        // the ROM flushes the cache; cleaning here as well keeps that explicit.
        psram::cleanCache();
        const std::uint64_t start = time_us_64();
        const int rc = flash_safe_execute(runFlashJob, &job, 500);
        lastDurationUs = static_cast<std::uint32_t>(time_us_64() - start);
        lastM1Intact = job.m1Intact;
        if (rc != PICO_OK) std::printf("[flash] flash_safe_execute failed (%d)\n", rc);
        return rc == PICO_OK;
    }
};
DeviceFlash g_deviceFlash;
FlashDevice* g_device = &g_deviceFlash;
#else
FlashDevice* g_device = nullptr;
#endif

lfs_t g_lfs{};
lfs_config g_cfg{};
alignas(4) std::uint8_t g_readBuffer[kCacheBytes];
alignas(4) std::uint8_t g_progBuffer[kCacheBytes];
alignas(4) std::uint8_t g_lookahead[kLookaheadBytes];
alignas(4) std::uint8_t g_fileBuffer[kCacheBytes];
bool g_mounted = false;
bool g_formatted = false;
int g_lastError = 0;
std::size_t g_peakUsed = 0;
std::uint32_t g_bootCount = 0;

int bdRead(const lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size) {
    if (!g_device) return LFS_ERR_IO;
    const std::size_t addr = cfg::kFlashFsOffset + static_cast<std::size_t>(block) * c->block_size + off;
    return g_device->read(addr, buffer, size) ? 0 : LFS_ERR_IO;
}
int bdProg(const lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size) {
    if (!g_device) return LFS_ERR_IO;
    const std::size_t addr = cfg::kFlashFsOffset + static_cast<std::size_t>(block) * c->block_size + off;
    return g_device->program(addr, buffer, size) ? 0 : LFS_ERR_IO;
}
int bdErase(const lfs_config* c, lfs_block_t block) {
    if (!g_device) return LFS_ERR_IO;
    const std::size_t addr = cfg::kFlashFsOffset + static_cast<std::size_t>(block) * c->block_size;
    return g_device->erase(addr, c->block_size) ? 0 : LFS_ERR_IO;
}
int bdSync(const lfs_config*) { return 0; }

void configure() {
    g_cfg = lfs_config{};
    g_cfg.read = bdRead;
    g_cfg.prog = bdProg;
    g_cfg.erase = bdErase;
    g_cfg.sync = bdSync;
    g_cfg.read_size = 16;
    g_cfg.prog_size = kPageBytes;
    g_cfg.block_size = kSectorBytes;
    g_cfg.block_count = static_cast<lfs_size_t>(cfg::kFlashFsSize / kSectorBytes);
    g_cfg.block_cycles = 500;
    g_cfg.cache_size = kCacheBytes;
    g_cfg.lookahead_size = kLookaheadBytes;
    g_cfg.read_buffer = g_readBuffer;
    g_cfg.prog_buffer = g_progBuffer;
    g_cfg.lookahead_buffer = g_lookahead;
}

void updatePeak() {
    if (!g_mounted) return;
    const lfs_ssize_t blocks = lfs_fs_size(&g_lfs);
    if (blocks >= 0) {
        const std::size_t used = static_cast<std::size_t>(blocks) * kSectorBytes;
        if (used > g_peakUsed) g_peakUsed = used;
    }
}
} // namespace

Layout layout() noexcept {
    Layout l;
    l.flashSize = cfg::kFlashSize;
    l.firmwarePartition = cfg::kFirmwarePartitionBytes;
#if FXAI_ON_DEVICE
    l.firmwareUsed = static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(&__flash_binary_end) - cfg::kFlashXipBase);
#endif
    l.diagOffset = cfg::kFlashDiagOffset;
    l.fsOffset = cfg::kFlashFsOffset;
    l.fsSize = cfg::kFlashFsSize;
    l.btReservedOffset = cfg::kFlashFsOffset + cfg::kFlashFsSize;
    return l;
}

bool mount() noexcept {
    if (g_mounted) return true;
    if (!g_device) { g_lastError = LFS_ERR_IO; return false; }
    configure();
    int err = lfs_mount(&g_lfs, &g_cfg);
    g_formatted = false;
    if (err) {
        // Blank or corrupt partition (first boot, or layout change): format.
        err = lfs_format(&g_lfs, &g_cfg);
        if (!err) err = lfs_mount(&g_lfs, &g_cfg);
        g_formatted = err == 0;
    }
    g_lastError = err;
    g_mounted = err == 0;
    if (!g_mounted) {
        std::printf("[fs] LittleFS mount failed (%d): internal storage disabled\n", err);
        return false;
    }
    std::uint32_t count = 0;
    if (readFile(kBootCounterFile, &count, sizeof(count)) != static_cast<long>(sizeof(count))) count = 0;
    ++count;
    writeFile(kBootCounterFile, &count, sizeof(count));
    g_bootCount = count;
    updatePeak();
    return true;
}

void unmount() noexcept {
    if (g_mounted) lfs_unmount(&g_lfs);
    g_mounted = false;
}

FsStats stats() noexcept {
    FsStats s;
    s.mounted = g_mounted;
    s.formattedAtBoot = g_formatted;
    s.blockSize = kSectorBytes;
    s.blockCount = cfg::kFlashFsSize / kSectorBytes;
    s.lastError = g_lastError;
    s.bootCount = g_bootCount;
    if (g_mounted) {
        const lfs_ssize_t blocks = lfs_fs_size(&g_lfs);
        if (blocks >= 0) {
            s.usedBytes = static_cast<std::size_t>(blocks) * kSectorBytes;
            const std::size_t total = s.blockCount * kSectorBytes;
            s.freeBytes = total > s.usedBytes ? total - s.usedBytes : 0;
        }
        updatePeak();
    }
    s.peakUsedBytes = g_peakUsed;
    return s;
}

bool writeFile(const char* path, const void* data, std::size_t bytes) noexcept {
    if (!g_mounted || !path) return false;
    lfs_file_t file{};
    lfs_file_config fcfg{};
    fcfg.buffer = g_fileBuffer;
    int err = lfs_file_opencfg(&g_lfs, &file, path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC, &fcfg);
    if (err) { g_lastError = err; return false; }
    const lfs_ssize_t written = lfs_file_write(&g_lfs, &file, data, static_cast<lfs_size_t>(bytes));
    err = lfs_file_close(&g_lfs, &file); // commits atomically
    g_lastError = err;
    updatePeak();
    return err == 0 && written == static_cast<lfs_ssize_t>(bytes);
}

long readFile(const char* path, void* buffer, std::size_t capacity) noexcept {
    if (!g_mounted || !path) return -1;
    lfs_file_t file{};
    lfs_file_config fcfg{};
    fcfg.buffer = g_fileBuffer;
    int err = lfs_file_opencfg(&g_lfs, &file, path, LFS_O_RDONLY, &fcfg);
    if (err) { g_lastError = err; return -1; }
    const lfs_ssize_t got = lfs_file_read(&g_lfs, &file, buffer, static_cast<lfs_size_t>(capacity));
    lfs_file_close(&g_lfs, &file);
    return got < 0 ? -1 : static_cast<long>(got);
}

bool removeFile(const char* path) noexcept {
    if (!g_mounted || !path) return false;
    const int err = lfs_remove(&g_lfs, path);
    g_lastError = err;
    return err == 0;
}

FlashPsramReport testPsramAfterFlashWrite() noexcept {
    FlashPsramReport r;
#if FXAI_ON_DEVICE
    r.ran = true;
    constexpr std::size_t kWords = 16384; // 64 KiB: more than the 16 KiB XIP cache
    std::uint32_t* buffer = nullptr;
    const bool havePsram = psram::available();
    if (havePsram) {
        buffer = static_cast<std::uint32_t*>(mem::tryAlloc(mem::Pool::PSRAM_BOARD, kWords * 4, 64));
        if (!buffer) { r.error = "PSRAM insufficiente per il test"; return r; }
        std::uint32_t s = 0x1234567u;
        for (std::size_t i = 0; i < kWords; ++i) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; buffer[i] = s; }
        // No explicit clean here: the flash routines must commit dirty lines themselves.
    }
    // Erase + program the diagnostic sector (outside LittleFS).
    const std::uint64_t t0 = time_us_64();
    const bool erased = g_deviceFlash.erase(cfg::kFlashDiagOffset, kSectorBytes);
    r.eraseUs = static_cast<std::uint32_t>(time_us_64() - t0);
    bool m1 = g_deviceFlash.lastM1Intact;
    alignas(4) std::uint8_t page[kPageBytes];
    for (std::size_t i = 0; i < kPageBytes; ++i) page[i] = static_cast<std::uint8_t>(i * 7u + 3u);
    const std::uint64_t t1 = time_us_64();
    const bool programmed = erased && g_deviceFlash.program(cfg::kFlashDiagOffset, page, kPageBytes);
    r.programUs = static_cast<std::uint32_t>(time_us_64() - t1);
    m1 = m1 && g_deviceFlash.lastM1Intact;
    r.flashVerified = programmed &&
        std::memcmp(reinterpret_cast<const void*>(cfg::kFlashXipBase + cfg::kFlashDiagOffset), page, kPageBytes) == 0;
    r.m1ConfigIntact = m1;
    if (havePsram) {
        const auto uncached = reinterpret_cast<volatile std::uint32_t*>(
            reinterpret_cast<std::uintptr_t>(buffer) - cfg::kPsramCachedBase + cfg::kPsramUncachedBase);
        std::uint32_t s = 0x1234567u;
        bool okCached = true, okUncached = true;
        for (std::size_t i = 0; i < kWords; ++i) {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            if (uncached[i] != s) okUncached = false;
            if (buffer[i] != s) okCached = false;
        }
        r.psramIntactCached = okCached;
        r.psramIntactUncached = okUncached;
        // The window must still accept writes (QPI mode + WRITABLE_M1 intact).
        for (std::size_t i = 0; i < 256; ++i) buffer[i] = ~static_cast<std::uint32_t>(i);
        psram::cleanCache();
        bool writable = true;
        for (std::size_t i = 0; i < 256; ++i) if (uncached[i] != ~static_cast<std::uint32_t>(i)) writable = false;
        r.psramWritableAfter = writable;
        mem::free(buffer);
    } else {
        r.psramIntactCached = r.psramIntactUncached = r.psramWritableAfter = true; // not applicable
    }
    if (!r.flashVerified) r.error = "verifica flash fallita";
#else
    r.error = "solo su dispositivo";
#endif
    return r;
}

namespace testing {
void useDevice(FlashDevice* device) noexcept {
    unmount();
#if FXAI_ON_DEVICE
    g_device = device ? device : &g_deviceFlash;
#else
    g_device = device;
#endif
    g_peakUsed = 0;
}
} // namespace testing

} // namespace fx::storage
