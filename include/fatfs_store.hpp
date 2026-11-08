#pragma once
// files::FileSystem on a FAT/exFAT volume (FatFs R0.16, third_party/fatfs) of
// a 512-byte sector device: the PicoCalc microSD card on the RP2350, a RAM
// disk or a simulated SD card in the host tests. The card is mounted on
// demand and re-mounted after it is removed and inserted again.
#include "file_store.hpp"
#include "sd_card.hpp"
#include <memory>

namespace fx::files {

class FatFsFileSystem final : public FileSystem {
public:
    explicit FatFsFileSystem(sd::BlockDevice& device);
    ~FatFsFileSystem() override;
    FatFsFileSystem(const FatFsFileSystem&) = delete;
    FatFsFileSystem& operator=(const FatFsFileSystem&) = delete;
    const char* name() const noexcept override { return "microSD"; }
    Status status() override;
    Status list(const std::string& dir, std::vector<Entry>& out) override;
    Status stat(const std::string& path, Entry& out) override;
    Status openRead(const std::string& path, std::unique_ptr<Reader>& out) override;
    Status openWrite(const std::string& path, std::unique_ptr<Writer>& out) override;
    Status remove(const std::string& path) override;
    Status rename(const std::string& from, const std::string& to) override;
    Status makeDirs(const std::string& path) override;
    Status freeSpace(std::uint64_t& bytes) override;
    // Creates a new FAT/exFAT volume (host builds with FXAI_FATFS_MKFS only).
    Status format();
    bool mounted() const noexcept { return mounted_; }
private:
    struct Volume;
    Status ensureMounted();
    void unmount();
    sd::BlockDevice& device_;
    std::unique_ptr<Volume> volume_;
    bool mounted_{false};
};
} // namespace fx::files
