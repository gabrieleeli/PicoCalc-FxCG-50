// files::FileSystem on FatFs (third_party/fatfs) and the FatFs disk I/O
// functions for one 512-byte sector device.
#include "fatfs_store.hpp"
#include "ff.h"
#include "diskio.h"
#include <cstring>
#include <vector>

namespace {
fx::sd::BlockDevice* g_device = nullptr;     // the device of drive 0 (one volume)
}

// ---------------------------------------------------------------- FatFs disk I/O
extern "C" {
DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0 || !g_device) return STA_NOINIT;
    if (!g_device->present()) return STA_NODISK | STA_NOINIT;
    return g_device->ready() ? 0 : STA_NOINIT;
}
DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0 || !g_device) return STA_NOINIT;
    if (!g_device->present()) return STA_NODISK | STA_NOINIT;
    if (!g_device->ready() && !g_device->init()) return STA_NOINIT;
    return 0;
}
DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !g_device || !g_device->ready()) return RES_NOTRDY;
    return g_device->read(sector, buff, count) ? RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !g_device || !g_device->ready()) return RES_NOTRDY;
    return g_device->write(sector, buff, count) ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv != 0 || !g_device || !g_device->ready()) return RES_NOTRDY;
    switch (cmd) {
    case CTRL_SYNC: return g_device->sync() ? RES_OK : RES_ERROR;
    case GET_SECTOR_COUNT: *static_cast<LBA_t*>(buff) = static_cast<LBA_t>(g_device->sectors()); return RES_OK;
    case GET_SECTOR_SIZE: *static_cast<WORD*>(buff) = static_cast<WORD>(fx::sd::BlockDevice::SectorSize); return RES_OK;
    case GET_BLOCK_SIZE: *static_cast<DWORD*>(buff) = 1; return RES_OK;   // erase block size unknown
    default: return RES_PARERR;
    }
}
}

namespace fx::files {
namespace {
Status map(FRESULT r) {
    switch (r) {
    case FR_OK: return Status::Ok;
    case FR_NOT_READY: return Status::NoCard;
    case FR_NO_FILE: case FR_NO_PATH: return Status::NotFound;
    case FR_EXIST: return Status::Exists;
    case FR_INVALID_NAME: case FR_INVALID_DRIVE: return Status::InvalidName;
    case FR_NO_FILESYSTEM: return Status::Corrupt;
    case FR_DENIED: return Status::Full;                // directory or volume full when creating
    default: return Status::IoError;
    }
}
std::string fatPath(const std::string& p) {
    std::string out = "/";
    for (char c : p) {
        if (c == '/' && out.back() == '/') continue;
        out.push_back(c);
    }
    if (out.size() > 1 && out.back() == '/') out.pop_back();
    return out;
}
class FatReader final : public Reader {
public:
    FIL fil{};
    bool open{false};
    ~FatReader() override { if (open) f_close(&fil); }
    Status read(void* dst, std::size_t n, std::size_t& got) override {
        UINT br = 0;
        const FRESULT r = f_read(&fil, dst, static_cast<UINT>(n), &br);
        got = br;
        return map(r);
    }
};
class FatWriter final : public Writer {
public:
    FIL fil{};
    bool open{false};
    ~FatWriter() override { if (open) f_close(&fil); }
    Status write(const void* data, std::size_t n) override {
        UINT bw = 0;
        const FRESULT r = f_write(&fil, data, static_cast<UINT>(n), &bw);
        if (r != FR_OK) return map(r);
        return bw < n ? Status::Full : Status::Ok;      // FatFs writes less when the volume is full
    }
    Status close() override {
        if (!open) return Status::Ok;
        open = false;
        return map(f_close(&fil));
    }
};
} // namespace

struct FatFsFileSystem::Volume { FATFS fs{}; };

FatFsFileSystem::FatFsFileSystem(sd::BlockDevice& device) : device_(device), volume_(std::make_unique<Volume>()) {}
FatFsFileSystem::~FatFsFileSystem() {
    unmount();
    if (g_device == &device_) g_device = nullptr;
}
void FatFsFileSystem::unmount() {
    if (mounted_) f_mount(nullptr, "", 0);
    mounted_ = false;
}
Status FatFsFileSystem::ensureMounted() {
    g_device = &device_;
    if (!device_.present()) { unmount(); return Status::NoCard; }
    if (mounted_ && device_.ready()) return Status::Ok;
    unmount();
    // A card that was removed and inserted again is initialised from scratch.
    if (!device_.init()) return device_.present() ? Status::IoError : Status::NoCard;
    const FRESULT r = f_mount(&volume_->fs, "", 1);
    if (r != FR_OK) { f_mount(nullptr, "", 0); return map(r); }
    mounted_ = true;
    return Status::Ok;
}
Status FatFsFileSystem::status() { return ensureMounted(); }

Status FatFsFileSystem::list(const std::string& dir, std::vector<Entry>& out) {
    Status st = ensureMounted();
    if (st != Status::Ok) return st;
    DIR d{};
    FRESULT r = f_opendir(&d, fatPath(dir).c_str());
    if (r != FR_OK) return map(r);
    out.clear();
    for (;;) {
        FILINFO fno{};
        r = f_readdir(&d, &fno);
        if (r != FR_OK) { f_closedir(&d); return map(r); }
        if (fno.fname[0] == 0) break;
        if (std::strcmp(fno.fname, ".") == 0 || std::strcmp(fno.fname, "..") == 0) continue;
        out.push_back({fno.fname, static_cast<std::uint32_t>(fno.fsize), (fno.fattrib & AM_DIR) != 0});
    }
    f_closedir(&d);
    return Status::Ok;
}
Status FatFsFileSystem::stat(const std::string& path, Entry& out) {
    Status st = ensureMounted();
    if (st != Status::Ok) return st;
    const std::string p = fatPath(path);
    if (p == "/") { out = {"", 0, true}; return Status::Ok; }
    FILINFO fno{};
    const FRESULT r = f_stat(p.c_str(), &fno);
    if (r != FR_OK) return map(r);
    out = {fno.fname, static_cast<std::uint32_t>(fno.fsize), (fno.fattrib & AM_DIR) != 0};
    return Status::Ok;
}
Status FatFsFileSystem::openRead(const std::string& path, std::unique_ptr<Reader>& out) {
    Status st = ensureMounted();
    if (st != Status::Ok) return st;
    auto r = std::make_unique<FatReader>();
    const FRESULT res = f_open(&r->fil, fatPath(path).c_str(), FA_READ);
    if (res != FR_OK) return map(res);
    r->open = true;
    out = std::move(r);
    return Status::Ok;
}
Status FatFsFileSystem::openWrite(const std::string& path, std::unique_ptr<Writer>& out) {
    Status st = ensureMounted();
    if (st != Status::Ok) return st;
    auto w = std::make_unique<FatWriter>();
    const FRESULT res = f_open(&w->fil, fatPath(path).c_str(), FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) return map(res);
    w->open = true;
    out = std::move(w);
    return Status::Ok;
}
Status FatFsFileSystem::remove(const std::string& path) {
    Status st = ensureMounted();
    if (st != Status::Ok) return st;
    const FRESULT r = f_unlink(fatPath(path).c_str());
    return r == FR_DENIED ? Status::IoError : map(r);   // non-empty directory or read-only file
}
Status FatFsFileSystem::rename(const std::string& from, const std::string& to) {
    Status st = ensureMounted();
    if (st != Status::Ok) return st;
    return map(f_rename(fatPath(from).c_str(), fatPath(to).c_str()));
}
Status FatFsFileSystem::makeDirs(const std::string& path) {
    Status st = ensureMounted();
    if (st != Status::Ok) return st;
    const std::string p = fatPath(path);
    for (std::size_t i = 1; i <= p.size(); ++i) {
        if (i != p.size() && p[i] != '/') continue;
        const std::string part = p.substr(0, i);
        const FRESULT r = f_mkdir(part.c_str());
        if (r == FR_EXIST) {
            FILINFO fno{};
            if (f_stat(part.c_str(), &fno) != FR_OK || !(fno.fattrib & AM_DIR)) return Status::Exists;
            continue;
        }
        if (r != FR_OK) return map(r);
    }
    return Status::Ok;
}
Status FatFsFileSystem::freeSpace(std::uint64_t& bytes) {
    Status st = ensureMounted();
    if (st != Status::Ok) return st;
    DWORD clusters = 0;
    FATFS* fs = nullptr;
    const FRESULT r = f_getfree("", &clusters, &fs);
    if (r != FR_OK) return map(r);
    bytes = static_cast<std::uint64_t>(clusters) * fs->csize * sd::BlockDevice::SectorSize;
    return Status::Ok;
}
Status FatFsFileSystem::format() {
#if FF_USE_MKFS
    g_device = &device_;
    unmount();
    if (!device_.present()) return Status::NoCard;
    if (!device_.ready() && !device_.init()) return Status::IoError;
    std::vector<BYTE> work(4096);
    MKFS_PARM opt{FM_ANY | FM_SFD, 0, 0, 0, 0};
    const FRESULT r = f_mkfs("", &opt, work.data(), static_cast<UINT>(work.size()));
    if (r != FR_OK) return map(r);
    return ensureMounted();
#else
    return Status::NotSupported;
#endif
}
} // namespace fx::files
