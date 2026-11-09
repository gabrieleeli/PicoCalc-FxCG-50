#pragma once
// files::FileSystem on the internal flash: LittleFS v2.11.3 (third_party/
// littlefs, BSD-3-Clause) in the LittleFS partition of board_config.hpp
// (about 11 MB on the Pimoroni Pico Plus 2 W, 1.9 MB on the Pico 2 W).
//
// GRAPH projects and pictures can be saved here instead of the microSD card
// (SYSTEM > "Archivio file"). The partition is mounted on demand; a blank or
// foreign partition is formatted on the first mount (it holds nothing else).
// Names are matched case-insensitively and keep their case, like FAT; rename
// refuses an existing target and remove refuses a non-empty folder, as the
// FatFs backend does, so writeAtomic()/recover() behave the same on both.
// LittleFS is copy-on-write and power-loss resilient; every erase/program goes
// through the board FlashDevice (flash_safe_execute, SRAM bounce buffer,
// PSRAM QMI check on the RP2350).
#include "file_store.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace storage { class FlashDevice; }

namespace fx::files {

class LittleFsFileSystem final : public FileSystem {
public:
    explicit LittleFsFileSystem(storage::FlashDevice& device);
    ~LittleFsFileSystem() override;
    LittleFsFileSystem(const LittleFsFileSystem&) = delete;
    LittleFsFileSystem& operator=(const LittleFsFileSystem&) = delete;
    const char* name() const noexcept override { return "flash interna"; }
    std::string describe() override;
    Status status() override;
    Status list(const std::string& dir, std::vector<Entry>& out) override;
    Status stat(const std::string& path, Entry& out) override;
    Status openRead(const std::string& path, std::unique_ptr<Reader>& out) override;
    Status openWrite(const std::string& path, std::unique_ptr<Writer>& out) override;
    Status remove(const std::string& path) override;
    Status rename(const std::string& from, const std::string& to) override;
    Status makeDirs(const std::string& path) override;
    Status freeSpace(std::uint64_t& bytes) override;
    // Erases the file system (a new empty volume is mounted).
    Status format();
    bool mounted() const noexcept;
    bool formattedAtMount() const noexcept;     // the last mount had to format the partition
    std::uint64_t totalBytes() const noexcept;
    int lastError() const noexcept;             // last LittleFS error code (0 = none)
    struct Impl;
private:
    std::unique_ptr<Impl> impl_;
};

} // namespace fx::files
