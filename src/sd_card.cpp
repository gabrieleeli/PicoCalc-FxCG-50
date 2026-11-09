#include "sd_card.hpp"

#include "board_config.hpp"

#if FXAI_ON_DEVICE && FXAI_ENABLE_SD
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"
#endif

namespace fx::sd {

#if FXAI_ON_DEVICE && FXAI_ENABLE_SD
namespace {
namespace cfg = board::config;
spi_inst_t* const kSpi = spi0;

std::uint8_t xfer(std::uint8_t out) {
    std::uint8_t in = 0xFF;
    spi_write_read_blocking(kSpi, &out, &in, 1);
    return in;
}
void select(bool on) { gpio_put(cfg::kSdCs, on ? 0 : 1); }

// Sends a command and returns the R1 response (0xFF on timeout).
std::uint8_t command(std::uint8_t cmd, std::uint32_t arg, std::uint8_t crc) {
    xfer(0xFF);
    const std::uint8_t frame[6] = {static_cast<std::uint8_t>(0x40u | cmd), static_cast<std::uint8_t>(arg >> 24),
                                   static_cast<std::uint8_t>(arg >> 16), static_cast<std::uint8_t>(arg >> 8),
                                   static_cast<std::uint8_t>(arg), crc};
    for (std::uint8_t b : frame) xfer(b);
    for (int i = 0; i < 10; ++i) {
        const std::uint8_t r = xfer(0xFF);
        if ((r & 0x80u) == 0) return r;
    }
    return 0xFF;
}

void release() {
    select(false);
    xfer(0xFF);
}
} // namespace

ProbeResult probe() noexcept {
    ProbeResult r;
    r.enabled = true;
    gpio_init(cfg::kSdDetect);
    gpio_set_dir(cfg::kSdDetect, GPIO_IN);
    gpio_pull_up(cfg::kSdDetect);
    sleep_us(50);
    r.cardDetected = !gpio_get(cfg::kSdDetect); // active low
    if (!r.cardDetected) { r.error = "nessuna scheda inserita"; return r; }

    spi_init(kSpi, 400'000); // identification mode: <= 400 kHz
    gpio_set_function(cfg::kSdSck, GPIO_FUNC_SPI);
    gpio_set_function(cfg::kSdMosi, GPIO_FUNC_SPI);
    gpio_set_function(cfg::kSdMiso, GPIO_FUNC_SPI);
    gpio_pull_up(cfg::kSdMiso);
    gpio_init(cfg::kSdCs);
    gpio_set_dir(cfg::kSdCs, GPIO_OUT);
    select(false);
    for (int i = 0; i < 10; ++i) xfer(0xFF); // >= 74 clocks with CS high

    select(true);
    std::uint8_t r1 = 0xFF;
    for (int i = 0; i < 10 && r1 != 0x01; ++i) r1 = command(0, 0, 0x95); // GO_IDLE_STATE
    if (r1 != 0x01) { release(); spi_deinit(kSpi); r.error = "nessuna risposta a CMD0"; return r; }

    r1 = command(8, 0x000001AAu, 0x87); // SEND_IF_COND
    bool v2 = false;
    if (r1 == 0x01) {
        std::uint8_t r7[4];
        for (auto& b : r7) b = xfer(0xFF);
        v2 = r7[2] == 0x01 && r7[3] == 0xAA;
        if (!v2) { release(); spi_deinit(kSpi); r.error = "tensione non supportata (CMD8)"; return r; }
    }
    r.version = v2 ? 2 : 1;

    const absolute_time_t until = make_timeout_time_ms(1000);
    do {
        command(55, 0, 0x65);                       // APP_CMD
        r1 = command(41, v2 ? 0x40000000u : 0u, 0x77); // SD_SEND_OP_COND (HCS)
    } while (r1 != 0x00 && !time_reached(until));
    if (r1 != 0x00) { release(); spi_deinit(kSpi); r.error = "timeout ACMD41"; return r; }
    r.initialized = true;

    if (v2 && command(58, 0, 0xFD) == 0x00) { // READ_OCR
        std::uint8_t ocr[4];
        for (auto& b : ocr) b = xfer(0xFF);
        r.highCapacity = (ocr[0] & 0x40u) != 0; // CCS
    }

    if (command(9, 0, 0xAF) == 0x00) { // SEND_CSD
        std::uint8_t token = 0xFF;
        for (int i = 0; i < 1000 && token == 0xFF; ++i) token = xfer(0xFF);
        if (token == 0xFE) {
            std::uint8_t csd[16];
            for (auto& b : csd) b = xfer(0xFF);
            xfer(0xFF); xfer(0xFF); // CRC16
            if ((csd[0] >> 6) == 1) { // CSD v2: C_SIZE[69:48]
                const std::uint32_t cSize = (static_cast<std::uint32_t>(csd[7] & 0x3Fu) << 16) |
                                            (static_cast<std::uint32_t>(csd[8]) << 8) | csd[9];
                r.capacityBytes = (static_cast<std::uint64_t>(cSize) + 1u) * 512u * 1024u;
            } else { // CSD v1
                const std::uint32_t readBlLen = csd[5] & 0x0Fu;
                const std::uint32_t cSize = (static_cast<std::uint32_t>(csd[6] & 0x03u) << 10) |
                                            (static_cast<std::uint32_t>(csd[7]) << 2) | (csd[8] >> 6);
                const std::uint32_t mult = ((csd[9] & 0x03u) << 1) | (csd[10] >> 7);
                r.capacityBytes = (static_cast<std::uint64_t>(cSize) + 1u) << (mult + 2u + readBlLen);
            }
        }
    }
    release();
    spi_deinit(kSpi);
    return r;
}

#else

ProbeResult probe() noexcept {
    ProbeResult r;
    r.enabled = FXAI_ENABLE_SD != 0;
    r.error = FXAI_ENABLE_SD ? "solo su dispositivo" : "SD disabilitata (FXAI_ENABLE_SD=OFF)";
    return r;
}

#endif

} // namespace fx::sd
