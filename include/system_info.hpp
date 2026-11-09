#pragma once
// Snapshot of the memory state for SYSTEM > INFO / DIAG and the USB log:
// SRAM and PSRAM pools, PSRAM driver state, stacks of both cores, flash
// layout and the hardware facts supplied by the board port.

#include "hardware_info.hpp"
#include "mem_pools.hpp"
#include "psram.hpp"
#include "system_runtime.hpp"

#include <cstddef>

namespace fx::sysinfo {

struct Report {
    const char* board{};
    const char* version{};
    mem::PoolStats sram{};
    mem::PoolStats psram{};
    std::size_t outOfMemoryEvents{};
    std::size_t invalidFrees{};
    runtime::StackUsage core0{};
    runtime::StackUsage core1{};
    psram::Info psramInfo{};
    Hardware hw{};
};

Report collect(const Hardware& hw) noexcept;

inline constexpr std::size_t kLineChars = 52;
// Formats the report as fixed-width lines (small font, 52 columns).
std::size_t formatMemory(const Report& report, char (*lines)[kLineChars], std::size_t maxLines) noexcept;
// "512", "12.5K", "3.2M"
void formatSize(char* out, std::size_t capacity, std::size_t bytes) noexcept;

} // namespace fx::sysinfo
