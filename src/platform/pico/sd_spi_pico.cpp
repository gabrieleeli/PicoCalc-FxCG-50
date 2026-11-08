// PicoCalc microSD slot on the Pico 2 W: hardware SPI0 and the card-detect
// switch. Pins from the official ClockworkPi sources
// (github.com/clockworkpi/PicoCalc, Code/MP3Player/config.h and
// Code/pico_multi_booter/boot.c): SCK GP18, MOSI GP19, MISO GP16, CS GP17,
// card detect GP22 (low = card inserted, internal pull-up).
// The SD protocol and the FAT file system are portable (src/sd_card.cpp,
// src/fatfs_store.cpp) and tested on the host; this file still needs a test
// on a physical PicoCalc.
#include "fatfs_store.hpp"
#include "sd_card.hpp"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace board {
namespace {
constexpr uint SD_SCK = 18, SD_MOSI = 19, SD_MISO = 16, SD_CS = 17, SD_DETECT = 22;
spi_inst_t* const SD_SPI = spi0;

class PicoSdBus final : public fx::sd::SpiBus {
public:
    PicoSdBus() {
        spi_init(SD_SPI, fx::sd::SdCard::InitClockHz);
        spi_set_format(SD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        gpio_set_function(SD_SCK, GPIO_FUNC_SPI);
        gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);
        gpio_set_function(SD_MISO, GPIO_FUNC_SPI);
        gpio_pull_up(SD_MISO);                                   // MISO idles high between transfers
        gpio_init(SD_CS); gpio_set_dir(SD_CS, GPIO_OUT); gpio_put(SD_CS, 1);
        gpio_init(SD_DETECT); gpio_set_dir(SD_DETECT, GPIO_IN); gpio_pull_up(SD_DETECT);
    }
    void setClock(std::uint32_t hz) override { spi_set_baudrate(SD_SPI, hz); }
    void select(bool on) override { gpio_put(SD_CS, on ? 0 : 1); }
    std::uint8_t transfer(std::uint8_t out) override {
        std::uint8_t in = 0xFF;
        spi_write_read_blocking(SD_SPI, &out, &in, 1);
        return in;
    }
    void read(std::uint8_t* dst, std::size_t n) override { spi_read_blocking(SD_SPI, 0xFF, dst, n); }
    void write(const std::uint8_t* src, std::size_t n) override { spi_write_blocking(SD_SPI, src, n); }
    bool cardPresent() override { return !gpio_get(SD_DETECT); }
    std::uint32_t millis() override { return to_ms_since_boot(get_absolute_time()); }
};
} // namespace

fx::files::FileSystem* fileSystem() {
    static PicoSdBus bus;
    static fx::sd::SdCard card(bus);
    static fx::files::FatFsFileSystem fs(card);
    return &fs;
}
} // namespace board
