#pragma once
// Internal flash partitioning + LittleFS file system.
//
// Layout (offsets from the start of flash, computed in cmake/fxai_board.cmake
// from the board flash size):
//   [0, FW)            firmware image (post-build size check)
//   [FW, FW+4K)        diagnostic scratch sector (flash/PSRAM interaction test)
//   [FW+4K, END-8K)    LittleFS
//   [END-8K, END)      reserved for BTstack TLV storage
//
// Flash and PSRAM share the QSPI bus (QMI): while a sector is erased or a page
// programmed, XIP is disabled for BOTH chips. Therefore every erase/program
// runs through flash_safe_execute() (other core locked out, IRQs masked on the
// caller), the source data is first copied into an SRAM bounce buffer (a
// source in PSRAM or flash would be unreadable during the operation), and the
// QMI M1 (PSRAM) configuration is verified/repaired before leaving the safe zone.

#include <cstddef>
#include <cstdint>

namespace fx::storage {

struct Layout {
    std::size_t flashSize{};
    std::size_t firmwarePartition{};
    std::size_t firmwareUsed{};     // bytes of flash occupied by the running image (0 on host)
    std::size_t diagOffset{};
    std::size_t fsOffset{};
    std::size_t fsSize{};
    std::size_t btReservedOffset{};
};
Layout layout() noexcept;

struct FsStats {
    bool mounted{};
    bool formattedAtBoot{};
    std::size_t blockSize{};
    std::size_t blockCount{};
    std::size_t usedBytes{};
    std::size_t freeBytes{};
    std::size_t peakUsedBytes{};
    std::uint32_t bootCount{};
    int lastError{};
};

bool mount() noexcept;     // mounts, formatting on first boot or if corrupt
void unmount() noexcept;
FsStats stats() noexcept;
bool writeFile(const char* path, const void* data, std::size_t bytes) noexcept;
long readFile(const char* path, void* buffer, std::size_t capacity) noexcept; // bytes read or -1
bool removeFile(const char* path) noexcept;

// Diagnostic: PSRAM content and configuration must survive a flash erase +
// program of the diagnostic sector (System -> Test).
struct FlashPsramReport {
    bool ran{};
    bool flashVerified{};
    bool psramIntactCached{};
    bool psramIntactUncached{};
    bool psramWritableAfter{};
    bool m1ConfigIntact{};
    std::uint32_t eraseUs{};
    std::uint32_t programUs{};
    const char* error{};
};
FlashPsramReport testPsramAfterFlashWrite() noexcept;

// Low-level flash access used by LittleFS (exposed for host tests).
class FlashDevice {
public:
    virtual ~FlashDevice() = default;
    virtual bool read(std::size_t offset, void* dst, std::size_t bytes) = 0;
    virtual bool erase(std::size_t offset, std::size_t bytes) = 0;        // 4 KiB aligned
    virtual bool program(std::size_t offset, const void* src, std::size_t bytes) = 0; // 256 B aligned
};

namespace testing {
// Host tests: route LittleFS to an emulated NOR flash of `flashSize` bytes
// (the default device layout is applied to it). nullptr restores the default.
void useDevice(FlashDevice* device) noexcept;
} // namespace testing

} // namespace fx::storage
