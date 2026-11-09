#pragma once
// Host only: a 512-byte sector device backed by a disk image file (for example
// an image of a real microSD card, or one made with mkfs.fat / mkfs.exfat).
// Used by the simulator (PICOCALC_SD_IMAGE) and by tools/tests that check the
// FatFs volume written by the firmware code with independent tools.
#include "sd_card.hpp"
#include <cstdio>
#include <string>

namespace board::host {
class FileDisk final : public fx::sd::BlockDevice {
public:
    explicit FileDisk(std::string path) : path_(std::move(path)) {}
    ~FileDisk() override { if (f_) std::fclose(f_); }
    FileDisk(const FileDisk&) = delete;
    FileDisk& operator=(const FileDisk&) = delete;
    bool present() override { return true; }
    bool init() override;
    bool ready() const override { return f_ != nullptr; }
    std::uint64_t sectors() const override { return sectors_; }
    bool read(std::uint64_t lba, std::uint8_t* dst, std::uint32_t count) override;
    bool write(std::uint64_t lba, const std::uint8_t* src, std::uint32_t count) override;
    bool sync() override { return f_ && std::fflush(f_) == 0; }
    const char* kind() const override { return "immagine"; }
private:
    std::string path_;
    std::FILE* f_{nullptr};
    std::uint64_t sectors_{0};
};
} // namespace board::host
