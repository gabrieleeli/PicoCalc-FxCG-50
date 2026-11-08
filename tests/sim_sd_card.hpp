#pragma once
// SD card in SPI mode simulated byte by byte as seen on MISO/MOSI, for the
// host tests of the microSD stack (sd_card.cpp, FatFs, files::FileSystem):
// SDHC, byte-addressed SDv2 and SDv1 cards, rejected writes, busy cards, read
// errors, missing card and power cycles.
#include "sd_card.hpp"
#include <cstdint>
#include <cstring>
#include <deque>
#include <utility>
#include <vector>

namespace test {
using namespace fx;
// SD card in SPI mode, simulated byte by byte as seen on MISO/MOSI.
class SimCard final : public sd::SpiBus {
public:
    enum class Kind { SDHC, SDv2, SDv1 };
    SimCard(Kind k, std::size_t sectors) : kind_(k), data_(sectors * 512, 0), sectors_(sectors) {}
    // Card with existing content (a disk image), sectors = size / 512.
    SimCard(Kind k, std::vector<std::uint8_t> image) : kind_(k), data_(std::move(image)), sectors_(data_.size() / 512) {}
    bool present{true};
    bool rejectWrites{false};
    int busyBytes{3};
    int readyPolls{3};                       // ACMD41 answers "idle" this many times
    std::uint32_t clock{0};
    std::size_t commands{0};
    int failReads{0};                        // the next n CMD17 answer with a data error token
    // Card removed and inserted again without the firmware noticing: it lost
    // power and ignores every command until CMD0 puts it back in SPI mode.
    void powerCycle() { spi_ = false; idle_ = true; out_.clear(); cmdLen_ = 0; rx_ = Rx::None; }
    void setClock(std::uint32_t hz) override { clock = hz; }
    void select(bool s) override { selected_ = s; if (!s) { out_.clear(); cmdLen_ = 0; } }
    bool cardPresent() override { return present; }
    std::uint32_t millis() override { return ++now_; }
    std::uint8_t transfer(std::uint8_t in) override {
        if (!present || !selected_) return 0xFF;
        // Pending response bytes (R1 of CMD24 included) go out before the card waits for data.
        if (rx_ != Rx::Data && cmdLen_ == 0 && !out_.empty()) { const std::uint8_t b = out_.front(); out_.pop_front(); return b; }
        if (rx_ == Rx::Token) { if (in == 0xFE) { rx_ = Rx::Data; wbuf_.clear(); } return 0xFF; }
        if (rx_ == Rx::Data) {
            wbuf_.push_back(in);
            if (wbuf_.size() == 514) {           // 512 data bytes + CRC
                rx_ = Rx::None;
                if (rejectWrites) out_.push_back(0x0D);
                else {
                    std::memcpy(data_.data() + waddr_ * 512, wbuf_.data(), 512);
                    out_.push_back(0x05);
                    for (int i = 0; i < busyBytes; ++i) out_.push_back(0x00);
                }
            }
            return 0xFF;
        }
        if (cmdLen_ == 0 && !out_.empty()) { const std::uint8_t b = out_.front(); out_.pop_front(); return b; }
        if (cmdLen_ == 0) { if ((in & 0xC0) == 0x40) cmd_[cmdLen_++] = in; return 0xFF; }
        cmd_[cmdLen_++] = in;
        if (cmdLen_ == 6) { cmdLen_ = 0; execute(); }
        return 0xFF;
    }
    std::vector<std::uint8_t>& data() { return data_; }
private:
    enum class Rx { None, Token, Data };
    void r1(std::uint8_t v) { out_.push_back(0xFF); out_.push_back(v); }
    void execute() {
        ++commands;
        const std::uint8_t c = cmd_[0] & 0x3F;
        const std::uint32_t arg = (static_cast<std::uint32_t>(cmd_[1]) << 24) | (cmd_[2] << 16) | (cmd_[3] << 8) | cmd_[4];
        const bool app = app_;
        app_ = false;
        const std::uint8_t idle = idle_ ? 0x01 : 0x00;
        if (!spi_ && c != 0) return;             // not in SPI mode: no answer (MISO stays high)
        switch (c) {
        case 0: if (cmd_[5] != 0x95) { r1(0x09); return; } spi_ = true; idle_ = true; polls_ = readyPolls; r1(0x01); return;
        case 8:
            if (kind_ == Kind::SDv1) { r1(0x05); return; }
            if (cmd_[5] != 0x87) { r1(0x09); return; }
            r1(idle); out_.insert(out_.end(), {0x00, 0x00, static_cast<std::uint8_t>((arg >> 8) & 0x0F), static_cast<std::uint8_t>(arg)});
            return;
        case 55: app_ = true; r1(idle); return;
        case 41:
            if (!app) { r1(0x04 | idle); return; }
            if (kind_ == Kind::SDHC && !(arg & 0x40000000)) { r1(0x01); return; }   // SDHC needs HCS
            if (polls_ > 0) { --polls_; r1(0x01); return; }
            idle_ = false; r1(0x00); return;
        case 58: r1(idle); out_.insert(out_.end(), {static_cast<std::uint8_t>(0x80 | (kind_ == Kind::SDHC ? 0x40 : 0)), 0xFF, 0x80, 0x00}); return;
        case 16: r1(arg == 512 ? idle : static_cast<std::uint8_t>(0x40 | idle)); return;
        case 9: {
            if (idle_) { r1(0x01); return; }
            std::uint8_t csd[16]{};
            if (kind_ == Kind::SDHC) {
                const std::uint32_t csize = static_cast<std::uint32_t>(sectors_ / 1024 - 1);
                csd[0] = 0x40; csd[7] = static_cast<std::uint8_t>((csize >> 16) & 0x3F); csd[8] = static_cast<std::uint8_t>(csize >> 8); csd[9] = static_cast<std::uint8_t>(csize);
            } else {
                // capacity = (C_SIZE+1) * 2^(C_SIZE_MULT+2) * 2^READ_BL_LEN with READ_BL_LEN = 9, MULT = 7
                const std::uint32_t csize = static_cast<std::uint32_t>(sectors_ / 512 - 1);
                csd[5] = 9; csd[6] = static_cast<std::uint8_t>((csize >> 10) & 0x03); csd[7] = static_cast<std::uint8_t>(csize >> 2);
                csd[8] = static_cast<std::uint8_t>((csize & 0x03) << 6); csd[9] = 0x03; csd[10] = 0x80;
            }
            r1(0x00); out_.push_back(0xFF); out_.push_back(0xFE);
            out_.insert(out_.end(), csd, csd + 16); out_.push_back(0xAB); out_.push_back(0xCD);
            return;
        }
        case 17: {
            const std::uint64_t lba = kind_ == Kind::SDHC ? arg : arg / 512;
            if (idle_ || lba >= sectors_ || (kind_ != Kind::SDHC && arg % 512)) { r1(0x20); return; }
            if (failReads > 0) { --failReads; r1(0x00); out_.push_back(0xFF); out_.push_back(0x08); return; }   // data error token (ECC)
            r1(0x00); out_.push_back(0xFF); out_.push_back(0xFF); out_.push_back(0xFE);
            out_.insert(out_.end(), data_.begin() + static_cast<std::ptrdiff_t>(lba * 512), data_.begin() + static_cast<std::ptrdiff_t>(lba * 512 + 512));
            out_.push_back(0x12); out_.push_back(0x34);
            return;
        }
        case 24: {
            const std::uint64_t lba = kind_ == Kind::SDHC ? arg : arg / 512;
            if (idle_ || lba >= sectors_) { r1(0x20); return; }
            waddr_ = lba; rx_ = Rx::Token; r1(0x00);
            return;
        }
        case 13: r1(0x00); out_.push_back(0x00); return;
        default: r1(0x04 | idle); return;
        }
    }
    Kind kind_;
    std::vector<std::uint8_t> data_;
    std::size_t sectors_;
    std::deque<std::uint8_t> out_;
    std::uint8_t cmd_[6]{};
    int cmdLen_{0};
    bool selected_{false}, idle_{true}, app_{false}, spi_{false};
    int polls_{0};
    Rx rx_{Rx::None};
    std::vector<std::uint8_t> wbuf_;
    std::uint64_t waddr_{0};
    std::uint32_t now_{0};
};
} // namespace test
