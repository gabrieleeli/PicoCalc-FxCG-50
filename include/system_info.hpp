#pragma once
// Snapshot of the memory state for the System -> Memoria screen and the USB log.

#include "flash_fs.hpp"
#include "mem_pools.hpp"
#include "psram.hpp"
#include "system_runtime.hpp"

#include <cstddef>

namespace fx::sysinfo {

struct FramebufferPlacement {
    std::size_t bytes{};
    bool inPsram{};
    bool requestedPsram{};
};
void setFramebuffer(const FramebufferPlacement& placement) noexcept;

struct Report {
    const char* board{};
    const char* version{};
    mem::PoolStats sram{};
    mem::PoolStats psram{};
    mem::PoolStats psramPicocalc{};
    std::size_t outOfMemoryEvents{};
    runtime::StackUsage core0{};
    runtime::StackUsage core1{};
    storage::Layout flash{};
    storage::FsStats fs{};
    FramebufferPlacement framebuffer{};
    psram::Info psramInfo{};
};

Report collect() noexcept;

inline constexpr std::size_t kLineChars = 52;
// Formats the report as fixed-width lines for the 320x320 screen (5x7 font).
std::size_t formatMemory(const Report& report, char (*lines)[kLineChars], std::size_t maxLines) noexcept;
// "512", "12.5K", "3.2M"
void formatSize(char* out, std::size_t capacity, std::size_t bytes) noexcept;

} // namespace fx::sysinfo
