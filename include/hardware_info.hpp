#pragma once
// Facts about the running hardware that only the board port knows; reported
// by SYSTEM > INFO and DIAG and on the USB serial port (system_info.hpp).
#include <cstddef>
#include <cstdint>

namespace fx::sysinfo {
struct Hardware {
    std::size_t framebufferBytes{};
    bool framebufferInPsram{};
    bool framebufferRequestedPsram{};   // FXAI_FRAMEBUFFER_IN_PSRAM build option
    std::uint32_t lastPresentUs{};      // duration of the last LCD update
    std::uint32_t flashJobs{};          // flash_safe_execute() erase/program jobs
    std::uint32_t flashFailures{};
    std::size_t firmwareBytes{};        // flash occupied by the firmware image (0 on the host)
    void* framebufferPixels{};          // diagnostics only (device)
};
} // namespace fx::sysinfo
