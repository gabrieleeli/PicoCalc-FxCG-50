// SD card SPI-mode protocol (SD Physical Layer Simplified Specification).
#include "sd_card.hpp"
#include <cstring>

namespace fx::sd {
namespace {
constexpr std::uint8_t CMD0 = 0, CMD8 = 8, CMD9 = 9, CMD12 = 12, CMD13 = 13, CMD16 = 16, CMD17 = 17, CMD24 = 24, CMD55 = 55, CMD58 = 58, ACMD41 = 41;
constexpr std::uint8_t TokenStart = 0xFE;
} // namespace

void SdCard::deselect() {
    bus_.select(false);
    (void)bus_.transfer(0xFF);             // the card releases MISO after one more byte
}
bool SdCard::fail() {
    ++errors_;
    deselect();
    return false;
}
bool SdCard::waitReady(std::uint32_t timeoutMs) {
    const std::uint32_t start = bus_.millis();
    do {
        if (bus_.transfer(0xFF) == 0xFF) return true;
    } while (bus_.millis() - start < timeoutMs);
    return false;
}
std::uint8_t SdCard::command(std::uint8_t cmd, std::uint32_t arg) {
    bus_.select(true);
    if (cmd != CMD0 && !waitReady(500)) return 0xFF;
    // CRC is only checked for CMD0 and CMD8 in SPI mode.
    const std::uint8_t crc = cmd == CMD0 ? 0x95 : cmd == CMD8 ? 0x87 : 0x01;
    const std::uint8_t frame[6] = {static_cast<std::uint8_t>(0x40 | cmd), static_cast<std::uint8_t>(arg >> 24), static_cast<std::uint8_t>(arg >> 16),
                                   static_cast<std::uint8_t>(arg >> 8), static_cast<std::uint8_t>(arg), crc};
    bus_.write(frame, sizeof(frame));
    if (cmd == CMD12) (void)bus_.transfer(0xFF);    // stuff byte after STOP_TRANSMISSION
    for (int i = 0; i < 10; ++i) {                  // NCR: up to 8 bytes before R1
        const std::uint8_t r = bus_.transfer(0xFF);
        if (!(r & 0x80)) return r;
    }
    return 0xFF;
}
std::uint8_t SdCard::appCommand(std::uint8_t cmd, std::uint32_t arg) {
    const std::uint8_t r = command(CMD55, 0);
    if (r > 1) return r;
    deselect();
    return command(cmd, arg);
}
bool SdCard::receive(std::uint8_t* dst, std::size_t n, std::uint32_t timeoutMs) {
    const std::uint32_t start = bus_.millis();
    std::uint8_t token;
    do {
        token = bus_.transfer(0xFF);
    } while (token == 0xFF && bus_.millis() - start < timeoutMs);
    if (token != TokenStart) return false;          // timeout or data error token
    bus_.read(dst, n);
    (void)bus_.transfer(0xFF);                      // CRC (not checked: CRC mode off)
    (void)bus_.transfer(0xFF);
    return true;
}
bool SdCard::transmit(const std::uint8_t* src, std::uint8_t token) {
    if (!waitReady(500)) return false;
    (void)bus_.transfer(token);
    bus_.write(src, SectorSize);
    (void)bus_.transfer(0xFF);                      // dummy CRC
    (void)bus_.transfer(0xFF);
    const std::uint8_t response = bus_.transfer(0xFF);
    if ((response & 0x1F) != 0x05) return false;   // data rejected (CRC or write error)
    return waitReady(500);                          // programming (busy)
}

bool SdCard::init() {
    reset();
    if (!bus_.cardPresent()) return false;
    bus_.setClock(InitClockHz);
    bus_.select(false);
    for (int i = 0; i < 10; ++i) (void)bus_.transfer(0xFF);   // >= 74 clocks with CS high
    std::uint8_t r = 0xFF;
    for (int i = 0; i < 10 && r != 0x01; ++i) { r = command(CMD0, 0); deselect(); }
    if (r != 0x01) return fail();
    Type type = Type::None;
    const std::uint32_t start = bus_.millis();
    r = command(CMD8, 0x1AA);
    if (r == 0x01) {
        std::uint8_t r7[4];
        bus_.read(r7, 4);
        deselect();
        if (r7[2] != 0x01 || r7[3] != 0xAA) return fail();   // voltage not accepted / bad echo
        // ACMD41 with HCS until the card leaves the idle state (up to 1 s).
        do { r = appCommand(ACMD41, 0x40000000); deselect(); } while (r == 0x01 && bus_.millis() - start < 1000);
        if (r != 0x00) return fail();
        r = command(CMD58, 0);
        if (r != 0x00) return fail();
        std::uint8_t ocr[4];
        bus_.read(ocr, 4);
        deselect();
        type = (ocr[0] & 0x40) ? Type::SDHC : Type::SDv2;
    } else {
        deselect();
        // SD version 1 (MMC cards are not supported).
        do { r = appCommand(ACMD41, 0); deselect(); } while (r == 0x01 && bus_.millis() - start < 1000);
        if (r != 0x00) return fail();
        type = Type::SDv1;
    }
    if (type != Type::SDHC) {
        r = command(CMD16, 512);
        deselect();
        if (r != 0x00) return fail();
    }
    bus_.setClock(FastClockHz);
    // Capacity from the CSD register.
    r = command(CMD9, 0);
    std::uint8_t csd[16];
    if (r != 0x00 || !receive(csd, sizeof(csd), 200)) return fail();
    deselect();
    std::uint64_t sectors = 0;
    if ((csd[0] >> 6) == 1) {                       // CSD version 2.0 (SDHC/SDXC)
        const std::uint32_t csize = (static_cast<std::uint32_t>(csd[7] & 0x3F) << 16) | (static_cast<std::uint32_t>(csd[8]) << 8) | csd[9];
        sectors = (static_cast<std::uint64_t>(csize) + 1) * 1024;
    } else {                                        // CSD version 1.0
        const std::uint32_t readBl = csd[5] & 0x0F;
        const std::uint32_t csize = (static_cast<std::uint32_t>(csd[6] & 0x03) << 10) | (static_cast<std::uint32_t>(csd[7]) << 2) | (csd[8] >> 6);
        const std::uint32_t mult = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);
        sectors = (static_cast<std::uint64_t>(csize + 1) << (mult + 2 + readBl)) / SectorSize;
    }
    if (sectors == 0) return fail();
    type_ = type;
    sectors_ = sectors;
    return true;
}

bool SdCard::read(std::uint64_t lba, std::uint8_t* dst, std::uint32_t count) {
    if (!ready() || lba + count > sectors_) return false;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (command(CMD17, address(lba + i)) != 0x00) return fail();
        if (!receive(dst + static_cast<std::size_t>(i) * SectorSize, SectorSize, 200)) return fail();
        deselect();
    }
    return true;
}

bool SdCard::write(std::uint64_t lba, const std::uint8_t* src, std::uint32_t count) {
    if (!ready() || lba + count > sectors_) return false;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (command(CMD24, address(lba + i)) != 0x00) return fail();
        if (!transmit(src + static_cast<std::size_t>(i) * SectorSize, TokenStart)) return fail();
        deselect();
        // CMD13: the card reports programming errors in R2.
        const std::uint8_t r1 = command(CMD13, 0);
        const std::uint8_t r2 = bus_.transfer(0xFF);
        deselect();
        if (r1 != 0x00 || r2 != 0x00) { ++errors_; return false; }
    }
    return true;
}

bool SdCard::sync() {
    if (!ready()) return false;
    bus_.select(true);
    const bool ok = waitReady(500);
    deselect();
    return ok;
}

bool RamDisk::read(std::uint64_t lba, std::uint8_t* dst, std::uint32_t count) {
    if (!ready_ || (lba + count) * SectorSize > data_.size()) return false;
    std::memcpy(dst, data_.data() + lba * SectorSize, static_cast<std::size_t>(count) * SectorSize);
    return true;
}
bool RamDisk::write(std::uint64_t lba, const std::uint8_t* src, std::uint32_t count) {
    if (!ready_ || failWrites_ || (lba + count) * SectorSize > data_.size()) return false;
    std::memcpy(data_.data() + lba * SectorSize, src, static_cast<std::size_t>(count) * SectorSize);
    return true;
}
} // namespace fx::sd
