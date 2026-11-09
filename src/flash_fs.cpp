#include "flash_fs.hpp"

#include "storage.hpp"

#include "lfs.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace fx::files {
namespace {
constexpr std::size_t kReadSize = 16;
constexpr std::size_t kCacheBytes = 512;      // multiple of the 256-byte flash page
constexpr std::size_t kLookaheadBytes = 128;

Status map(int err) {
    switch (err) {
    case LFS_ERR_OK: return Status::Ok;
    case LFS_ERR_NOENT: return Status::NotFound;
    case LFS_ERR_EXIST: return Status::Exists;
    case LFS_ERR_NOSPC: return Status::Full;
    case LFS_ERR_CORRUPT: return Status::Corrupt;
    case LFS_ERR_NAMETOOLONG: return Status::InvalidName;
    case LFS_ERR_FBIG: return Status::TooLarge;
    case LFS_ERR_NOTDIR: return Status::NotFound;
    default: return Status::IoError;
    }
}
bool sameName(const char* a, const std::string& b) {
    const std::size_t n = std::strlen(a);
    if (n != b.size()) return false;
    for (std::size_t i = 0; i < n; ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}
std::vector<std::string> split(const std::string& path) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : path) {
        if (c == '/' || c == '\\') { if (!cur.empty()) parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}
} // namespace

struct LittleFsFileSystem::Impl {
    explicit Impl(storage::FlashDevice& d) : dev(d) {}
    storage::FlashDevice& dev;
    lfs_t lfs{};
    lfs_config cfg{};
    alignas(4) std::uint8_t readBuffer[kCacheBytes]{};
    alignas(4) std::uint8_t progBuffer[kCacheBytes]{};
    alignas(4) std::uint8_t lookahead[kLookaheadBytes]{};
    bool mounted{false}, formatted{false};
    int lastError{0};

    static int bdRead(const lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size) {
        auto* self = static_cast<Impl*>(c->context);
        return self->dev.read(static_cast<std::size_t>(block) * c->block_size + off, buffer, size) ? 0 : LFS_ERR_IO;
    }
    static int bdProg(const lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size) {
        auto* self = static_cast<Impl*>(c->context);
        return self->dev.program(static_cast<std::size_t>(block) * c->block_size + off, buffer, size) ? 0 : LFS_ERR_IO;
    }
    static int bdErase(const lfs_config* c, lfs_block_t block) {
        auto* self = static_cast<Impl*>(c->context);
        return self->dev.erase(static_cast<std::size_t>(block) * c->block_size, c->block_size) ? 0 : LFS_ERR_IO;
    }
    static int bdSync(const lfs_config*) { return 0; }

    std::size_t blockCount() const { return dev.eraseSize() ? dev.size() / dev.eraseSize() : 0; }
    void configure() {
        cfg = lfs_config{};
        cfg.context = this;
        cfg.read = bdRead;
        cfg.prog = bdProg;
        cfg.erase = bdErase;
        cfg.sync = bdSync;
        cfg.read_size = kReadSize;
        cfg.prog_size = static_cast<lfs_size_t>(dev.programSize());
        cfg.block_size = static_cast<lfs_size_t>(dev.eraseSize());
        cfg.block_count = static_cast<lfs_size_t>(blockCount());
        cfg.block_cycles = 500;                 // wear levelling of the metadata pairs
        cfg.cache_size = kCacheBytes;
        cfg.lookahead_size = kLookaheadBytes;
        cfg.read_buffer = readBuffer;
        cfg.prog_buffer = progBuffer;
        cfg.lookahead_buffer = lookahead;
    }
    Status fail(int err) { lastError = err; return map(err); }
    Status ensureMounted() {
        if (mounted) return Status::Ok;
        if (blockCount() < 2 || dev.programSize() > kCacheBytes || kCacheBytes % dev.programSize()) return Status::NotSupported;
        configure();
        formatted = false;
        int err = lfs_mount(&lfs, &cfg);
        if (err) {
            // Blank or foreign partition (first boot, layout change): it holds
            // nothing else, so it is formatted.
            err = lfs_format(&lfs, &cfg);
            if (!err) err = lfs_mount(&lfs, &cfg);
            formatted = err == 0;
        }
        if (err) { std::printf("[fs] LittleFS mount failed (%d)\n", err); return fail(err); }
        mounted = true;
        return Status::Ok;
    }
    // Resolves every component case-insensitively. The last component keeps
    // the given spelling when it does not exist (creation). Returns false when
    // an intermediate folder is missing.
    bool resolve(const std::string& path, std::string& out, bool& lastExists) {
        const auto parts = split(path);
        out.clear();
        lastExists = true;
        for (std::size_t k = 0; k < parts.size(); ++k) {
            lfs_dir_t dir{};
            std::string found;
            if (lfs_dir_open(&lfs, &dir, out.empty() ? "/" : out.c_str()) == 0) {
                lfs_info info{};
                while (lfs_dir_read(&lfs, &dir, &info) > 0)
                    if (sameName(info.name, parts[k])) { found = info.name; break; }
                lfs_dir_close(&lfs, &dir);
            }
            if (found.empty()) {
                if (k + 1 < parts.size()) return false;
                found = parts[k];
                lastExists = false;
            }
            out += "/" + found;
        }
        if (out.empty()) out = "/";
        return true;
    }
};

namespace {
class LfsReader final : public Reader {
public:
    LfsReader(lfs_t& lfs) : lfs_(lfs) {}
    ~LfsReader() override { if (open_) lfs_file_close(&lfs_, &file_); }
    Status open(const char* path) {
        fcfg_.buffer = buffer_;
        const int err = lfs_file_opencfg(&lfs_, &file_, path, LFS_O_RDONLY, &fcfg_);
        open_ = err == 0;
        return map(err);
    }
    Status read(void* data, std::size_t length, std::size_t& got) override {
        const lfs_ssize_t n = lfs_file_read(&lfs_, &file_, data, static_cast<lfs_size_t>(length));
        if (n < 0) { got = 0; return map(static_cast<int>(n)); }
        got = static_cast<std::size_t>(n);
        return Status::Ok;
    }
private:
    lfs_t& lfs_;
    lfs_file_t file_{};
    lfs_file_config fcfg_{};
    alignas(4) std::uint8_t buffer_[kCacheBytes]{};
    bool open_{false};
};
class LfsWriter final : public Writer {
public:
    LfsWriter(lfs_t& lfs) : lfs_(lfs) {}
    ~LfsWriter() override { if (open_) lfs_file_close(&lfs_, &file_); }
    Status open(const char* path) {
        fcfg_.buffer = buffer_;
        const int err = lfs_file_opencfg(&lfs_, &file_, path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC, &fcfg_);
        open_ = err == 0;
        return map(err);
    }
    Status write(const void* data, std::size_t length) override {
        if (!open_) return Status::IoError;
        const lfs_ssize_t n = lfs_file_write(&lfs_, &file_, data, static_cast<lfs_size_t>(length));
        if (n < 0) return map(static_cast<int>(n));
        return static_cast<std::size_t>(n) == length ? Status::Ok : Status::Full;
    }
    Status close() override {
        if (!open_) return Status::IoError;
        open_ = false;
        return map(lfs_file_close(&lfs_, &file_));   // the file is committed atomically here
    }
private:
    lfs_t& lfs_;
    lfs_file_t file_{};
    lfs_file_config fcfg_{};
    alignas(4) std::uint8_t buffer_[kCacheBytes]{};
    bool open_{false};
};
} // namespace

LittleFsFileSystem::LittleFsFileSystem(storage::FlashDevice& device) : impl_(std::make_unique<Impl>(device)) {}
LittleFsFileSystem::~LittleFsFileSystem() {
    if (impl_ && impl_->mounted) lfs_unmount(&impl_->lfs);
}
bool LittleFsFileSystem::mounted() const noexcept { return impl_->mounted; }
bool LittleFsFileSystem::formattedAtMount() const noexcept { return impl_->formatted; }
int LittleFsFileSystem::lastError() const noexcept { return impl_->lastError; }
std::uint64_t LittleFsFileSystem::totalBytes() const noexcept {
    return static_cast<std::uint64_t>(impl_->blockCount()) * impl_->dev.eraseSize();
}

std::string LittleFsFileSystem::describe() {
    char buf[96];
    std::snprintf(buf, sizeof buf, "LittleFS %.1f MB (%u blocchi da %u byte)",
                  static_cast<double>(totalBytes()) / 1048576.0, static_cast<unsigned>(impl_->blockCount()),
                  static_cast<unsigned>(impl_->dev.eraseSize()));
    return buf;
}

Status LittleFsFileSystem::status() { return impl_->ensureMounted(); }

Status LittleFsFileSystem::format() {
    if (impl_->mounted) { lfs_unmount(&impl_->lfs); impl_->mounted = false; }
    if (impl_->blockCount() < 2) return Status::NotSupported;
    impl_->configure();
    int err = lfs_format(&impl_->lfs, &impl_->cfg);
    if (!err) err = lfs_mount(&impl_->lfs, &impl_->cfg);
    if (err) return impl_->fail(err);
    impl_->mounted = true;
    return Status::Ok;
}

Status LittleFsFileSystem::list(const std::string& dirPath, std::vector<Entry>& out) {
    Status st = impl_->ensureMounted();
    if (st != Status::Ok) return st;
    std::string p; bool exists = false;
    if (!impl_->resolve(dirPath, p, exists) || !exists) return Status::NotFound;
    lfs_dir_t dir{};
    int err = lfs_dir_open(&impl_->lfs, &dir, p.c_str());
    if (err) return impl_->fail(err);
    out.clear();
    lfs_info info{};
    while ((err = lfs_dir_read(&impl_->lfs, &dir, &info)) > 0) {
        if (std::strcmp(info.name, ".") == 0 || std::strcmp(info.name, "..") == 0) continue;
        out.push_back({info.name, static_cast<std::uint32_t>(info.size), info.type == LFS_TYPE_DIR});
    }
    lfs_dir_close(&impl_->lfs, &dir);
    return err < 0 ? impl_->fail(err) : Status::Ok;
}

Status LittleFsFileSystem::stat(const std::string& path, Entry& out) {
    Status st = impl_->ensureMounted();
    if (st != Status::Ok) return st;
    std::string p; bool exists = false;
    if (!impl_->resolve(path, p, exists) || !exists) return Status::NotFound;
    if (p == "/") { out = {"", 0, true}; return Status::Ok; }
    lfs_info info{};
    const int err = lfs_stat(&impl_->lfs, p.c_str(), &info);
    if (err) return impl_->fail(err);
    out = {info.name, static_cast<std::uint32_t>(info.size), info.type == LFS_TYPE_DIR};
    return Status::Ok;
}

Status LittleFsFileSystem::openRead(const std::string& path, std::unique_ptr<Reader>& out) {
    Status st = impl_->ensureMounted();
    if (st != Status::Ok) return st;
    std::string p; bool exists = false;
    if (!impl_->resolve(path, p, exists) || !exists) return Status::NotFound;
    auto r = std::make_unique<LfsReader>(impl_->lfs);
    st = r->open(p.c_str());
    if (st == Status::Ok) out = std::move(r);
    return st;
}

Status LittleFsFileSystem::openWrite(const std::string& path, std::unique_ptr<Writer>& out) {
    Status st = impl_->ensureMounted();
    if (st != Status::Ok) return st;
    std::string p; bool exists = false;
    if (!impl_->resolve(path, p, exists)) return Status::NotFound;
    auto w = std::make_unique<LfsWriter>(impl_->lfs);
    st = w->open(p.c_str());
    if (st == Status::Ok) out = std::move(w);
    return st;
}

Status LittleFsFileSystem::remove(const std::string& path) {
    Status st = impl_->ensureMounted();
    if (st != Status::Ok) return st;
    std::string p; bool exists = false;
    if (!impl_->resolve(path, p, exists) || !exists) return Status::NotFound;
    const int err = lfs_remove(&impl_->lfs, p.c_str());
    return err == LFS_ERR_NOTEMPTY ? Status::IoError : (err ? impl_->fail(err) : Status::Ok);   // as FatFs: FR_DENIED
}

Status LittleFsFileSystem::rename(const std::string& from, const std::string& to) {
    Status st = impl_->ensureMounted();
    if (st != Status::Ok) return st;
    std::string a, b; bool aExists = false, bExists = false;
    if (!impl_->resolve(from, a, aExists) || !aExists) return Status::NotFound;
    if (!impl_->resolve(to, b, bExists)) return Status::NotFound;
    // Same file with another case ("sine" -> "SINE") is a rename, not a clash.
    if (bExists && !sameName(a.c_str(), b)) return Status::Exists;
    const int err = lfs_rename(&impl_->lfs, a.c_str(), bExists ? (b.substr(0, b.rfind('/') + 1) + split(to).back()).c_str() : b.c_str());
    return err ? impl_->fail(err) : Status::Ok;
}

Status LittleFsFileSystem::makeDirs(const std::string& path) {
    Status st = impl_->ensureMounted();
    if (st != Status::Ok) return st;
    std::string built;
    for (const auto& part : split(path)) {
        std::string p; bool exists = false;
        const std::string want = built.empty() ? part : built + "/" + part;
        if (!impl_->resolve(want, p, exists)) return Status::NotFound;
        if (exists) {
            lfs_info info{};
            if (lfs_stat(&impl_->lfs, p.c_str(), &info) != 0 || info.type != LFS_TYPE_DIR) return Status::Exists;
        } else {
            const int err = lfs_mkdir(&impl_->lfs, p.c_str());
            if (err && err != LFS_ERR_EXIST) return impl_->fail(err);
        }
        built = p;
    }
    return Status::Ok;
}

Status LittleFsFileSystem::freeSpace(std::uint64_t& bytes) {
    Status st = impl_->ensureMounted();
    if (st != Status::Ok) return st;
    const lfs_ssize_t used = lfs_fs_size(&impl_->lfs);
    if (used < 0) return impl_->fail(static_cast<int>(used));
    const std::uint64_t total = impl_->blockCount();
    bytes = (total > static_cast<std::uint64_t>(used) ? total - static_cast<std::uint64_t>(used) : 0) * impl_->dev.eraseSize();
    return Status::Ok;
}

} // namespace fx::files
