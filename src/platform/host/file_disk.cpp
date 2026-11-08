#include "file_disk.hpp"

namespace board::host {
bool FileDisk::init() {
    if (f_) return true;
    f_ = std::fopen(path_.c_str(), "r+b");
    if (!f_) return false;
    if (std::fseek(f_, 0, SEEK_END) != 0) { std::fclose(f_); f_ = nullptr; return false; }
    const long size = std::ftell(f_);
    sectors_ = size > 0 ? static_cast<std::uint64_t>(size) / SectorSize : 0;
    if (sectors_ == 0) { std::fclose(f_); f_ = nullptr; return false; }
    return true;
}
bool FileDisk::read(std::uint64_t lba, std::uint8_t* dst, std::uint32_t count) {
    if (!f_ || lba + count > sectors_) return false;
    if (std::fseek(f_, static_cast<long>(lba * SectorSize), SEEK_SET) != 0) return false;
    return std::fread(dst, SectorSize, count, f_) == count;
}
bool FileDisk::write(std::uint64_t lba, const std::uint8_t* src, std::uint32_t count) {
    if (!f_ || lba + count > sectors_) return false;
    if (std::fseek(f_, static_cast<long>(lba * SectorSize), SEEK_SET) != 0) return false;
    return std::fwrite(src, SectorSize, count, f_) == count;
}
} // namespace board::host
