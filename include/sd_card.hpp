#pragma once
// PicoCalc micro-SD probe (FXAI_ENABLE_SD): SPI0 on GP16 (MISO) / GP18 (SCK) /
// GP19 (MOSI), CS on GP17, card-detect on GP22 (active low), pins from
// board_config.hpp. Performs the SD SPI-mode initialisation (CMD0, CMD8,
// ACMD41, CMD58, CMD9) and reports the card type and capacity. There is no
// file system on the SD card yet; this is the hardware layer + diagnostics.

#include <cstdint>

namespace fx::sd {

struct ProbeResult {
    bool enabled{};        // compiled with FXAI_ENABLE_SD
    bool cardDetected{};   // card-detect switch closed
    bool initialized{};    // card left the idle state (ACMD41)
    bool highCapacity{};   // SDHC/SDXC (block addressing)
    std::uint8_t version{};// 1 or 2 (CMD8 accepted)
    std::uint64_t capacityBytes{};
    const char* error{};
};

ProbeResult probe() noexcept;

} // namespace fx::sd
