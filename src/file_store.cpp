#include "file_store.hpp"
#include "storage.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fx::files {
namespace {
std::string lowerCopy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string lastComponent(const std::string& path) {
    std::string p = path;
    while (!p.empty() && p.back() == '/') p.pop_back();
    const auto slash = p.rfind('/');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}
// Writer adapter that computes the CRC-32 and the length of what passes through.
class CrcWriter final : public Writer {
public:
    explicit CrcWriter(Writer& w) : w_(w) {}
    Status write(const void* data, std::size_t n) override {
        const Status st = w_.write(data, n);
        if (st == Status::Ok) { crc_ = storage::crc32(data, n, crc_); size_ += n; }
        return st;
    }
    Status close() override { return Status::Ok; }   // closed by writeAtomic
    std::uint32_t crc() const noexcept { return crc_; }
    std::size_t size() const noexcept { return size_; }
private:
    Writer& w_;
    std::uint32_t crc_{0};
    std::size_t size_{0};
};
Status checksum(FileSystem& fs, const std::string& path, std::uint32_t& crc, std::size_t& size) {
    std::unique_ptr<Reader> r;
    Status st = fs.openRead(path, r);
    if (st != Status::Ok) return st;
    std::uint8_t buf[512];
    crc = 0; size = 0;
    for (;;) {
        std::size_t got = 0;
        st = r->read(buf, sizeof(buf), got);
        if (st != Status::Ok) return st;
        if (got == 0) return Status::Ok;
        crc = storage::crc32(buf, got, crc);
        size += got;
    }
}
bool exists(FileSystem& fs, const std::string& path) { Entry e; return fs.stat(path, e) == Status::Ok; }
} // namespace

const char* statusMessage(Status s) noexcept {
    switch (s) {
    case Status::Ok: return "operazione completata";
    case Status::NoCard: return "scheda microSD assente";
    case Status::NotFound: return "file non trovato";
    case Status::Exists: return "nome gia' usato";
    case Status::Full: return "scheda microSD piena";
    case Status::IoError: return "errore di lettura/scrittura sulla scheda";
    case Status::Corrupt: return "file danneggiato o non valido";
    case Status::InvalidName: return "nome non valido";
    case Status::TooLarge: return "file troppo grande";
    case Status::NotSupported: return "operazione non supportata";
    }
    return "errore";
}

bool validName(const std::string& name, std::size_t maxLength) {
    if (name.empty() || name.size() > maxLength) return false;
    for (unsigned char c : name) if (!(std::isalnum(c) || c == '_' || c == '-')) return false;
    return true;
}
std::string join(const std::string& dir, const std::string& name) { return dir.empty() ? name : dir + "/" + name; }
bool hasExtension(const std::string& name, const char* ext) {
    const std::size_t n = std::strlen(ext);
    return name.size() > n && lowerCopy(name.substr(name.size() - n)) == lowerCopy(ext);
}
std::string stem(const std::string& name) {
    const auto dot = name.rfind('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

Status readAll(FileSystem& fs, const std::string& path, std::vector<std::uint8_t>& out, std::size_t maxBytes) {
    Entry e;
    Status st = fs.stat(path, e);
    if (st != Status::Ok) return st;
    if (e.directory) return Status::NotFound;
    if (e.size > maxBytes) return Status::TooLarge;
    std::unique_ptr<Reader> r;
    st = fs.openRead(path, r);
    if (st != Status::Ok) return st;
    std::vector<std::uint8_t> data(e.size);
    std::size_t at = 0;
    while (at < data.size()) {
        std::size_t got = 0;
        st = r->read(data.data() + at, data.size() - at, got);
        if (st != Status::Ok) return st;
        if (got == 0) return Status::IoError;          // shorter than its directory entry
        at += got;
    }
    out.swap(data);
    return Status::Ok;
}

Status writeAtomic(FileSystem& fs, const std::string& path, const std::function<Status(Writer&)>& producer) {
    Status st = fs.status();
    if (st != Status::Ok) return st;
    const std::string tmp = path + ".tmp", bak = path + ".bak";
    {
        std::unique_ptr<Writer> w;
        st = fs.openWrite(tmp, w);
        if (st != Status::Ok) return st;
        CrcWriter cw(*w);
        st = producer(cw);
        const Status closed = w->close();
        if (st == Status::Ok) st = closed;
        w.reset();
        if (st != Status::Ok) { (void)fs.remove(tmp); return st; }
        // Read-back verification of the temporary file.
        std::uint32_t crc = 0; std::size_t size = 0;
        st = checksum(fs, tmp, crc, size);
        if (st == Status::Ok && (crc != cw.crc() || size != cw.size())) st = Status::IoError;
        if (st != Status::Ok) { (void)fs.remove(tmp); return st; }
    }
    const bool hadOld = exists(fs, path);
    if (hadOld) {
        if (exists(fs, bak) && (st = fs.remove(bak)) != Status::Ok) { (void)fs.remove(tmp); return st; }
        if ((st = fs.rename(path, bak)) != Status::Ok) { (void)fs.remove(tmp); return st; }
    }
    if ((st = fs.rename(tmp, path)) != Status::Ok) {
        if (hadOld) (void)fs.rename(bak, path);
        return st;
    }
    if (hadOld) (void)fs.remove(bak);                 // a leftover .bak is removed by recover()
    return Status::Ok;
}
Status writeAtomic(FileSystem& fs, const std::string& path, const std::uint8_t* data, std::size_t size) {
    return writeAtomic(fs, path, [&](Writer& w) { return size ? w.write(data, size) : Status::Ok; });
}

Status recover(FileSystem& fs, const std::string& dir) {
    std::vector<Entry> list;
    Status st = fs.list(dir, list);
    if (st != Status::Ok) return st;
    // 1) temporary files: complete the rename when the old file was already moved away.
    for (const auto& e : list) {
        if (e.directory || !hasExtension(e.name, ".tmp")) continue;
        const std::string base = join(dir, stem(e.name)), tmp = join(dir, e.name), bak = base + ".bak";
        if (exists(fs, base)) st = fs.remove(tmp);
        else if (exists(fs, bak)) { st = fs.rename(tmp, base); if (st == Status::Ok) st = fs.remove(bak); }
        else st = fs.remove(tmp);                      // a new file whose save did not finish
        if (st != Status::Ok) return st;
    }
    // 2) backups: drop them when the new file exists, restore them otherwise.
    list.clear();
    if ((st = fs.list(dir, list)) != Status::Ok) return st;
    for (const auto& e : list) {
        if (e.directory || !hasExtension(e.name, ".bak")) continue;
        const std::string base = join(dir, stem(e.name)), bak = join(dir, e.name);
        st = exists(fs, base) ? fs.remove(bak) : fs.rename(bak, base);
        if (st != Status::Ok) return st;
    }
    return Status::Ok;
}

// ---------------------------------------------------------------- RAM card
namespace {
class RamReader final : public Reader {
public:
    explicit RamReader(std::vector<std::uint8_t> data) : data_(std::move(data)) {}
    Status read(void* dst, std::size_t n, std::size_t& got) override {
        got = std::min(n, data_.size() - pos_);
        if (got) std::memcpy(dst, data_.data() + pos_, got);
        pos_ += got;
        return Status::Ok;
    }
private:
    std::vector<std::uint8_t> data_;
    std::size_t pos_{0};
};
} // namespace

class RamWriter final : public Writer {
public:
    RamWriter(RamFileSystem& fs, std::string key) : fs_(fs), key_(std::move(key)) {}
    Status write(const void* data, std::size_t n) override {
        const Status st = fs_.gate(true);
        if (st != Status::Ok) return st;
        auto it = fs_.nodes_.find(key_);
        if (it == fs_.nodes_.end()) return Status::IoError;
        const std::size_t used = fs_.used();
        const std::size_t room = fs_.capacity_ > used ? fs_.capacity_ - used : 0;
        const std::size_t fits = std::min(n, room);
        const std::size_t written = fs_.consume(fits);
        const auto* p = static_cast<const std::uint8_t*>(data);
        it->second.data.insert(it->second.data.end(), p, p + written);
        if (written < fits) return Status::IoError;   // power loss in the middle of the write
        return fits < n ? Status::Full : Status::Ok;
    }
    Status close() override { return fs_.gate(true); }
private:
    RamFileSystem& fs_;
    std::string key_;
};

RamFileSystem::RamFileSystem(std::size_t capacity) : capacity_(capacity) {}

std::string RamFileSystem::key(const std::string& path) {
    std::string out;
    for (char c : path) {
        if (c == '/' && (out.empty() || out.back() == '/')) continue;
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    while (!out.empty() && out.back() == '/') out.pop_back();
    return out;
}
std::string RamFileSystem::parentKey(const std::string& k) {
    const auto slash = k.rfind('/');
    return slash == std::string::npos ? std::string() : k.substr(0, slash);
}
Status RamFileSystem::gate(bool modifying) {
    if (!present_) return Status::NoCard;
    if (dead_) return Status::IoError;
    if (modifying && failAt_ > 0 && --failAt_ == 0) return Status::IoError;
    return Status::Ok;
}
std::size_t RamFileSystem::consume(std::size_t want) {
    if (!armed_) return want;
    if (want > budget_) { const std::size_t n = budget_; budget_ = 0; dead_ = true; return n; }
    budget_ -= want;
    return want;
}
std::size_t RamFileSystem::used() const noexcept {
    std::size_t n = 0;
    for (const auto& kv : nodes_) n += kv.second.data.size();
    return n;
}
bool RamFileSystem::exists(const std::string& path) const { return nodes_.count(key(path)) != 0; }
std::vector<std::uint8_t>* RamFileSystem::data(const std::string& path) {
    auto it = nodes_.find(key(path));
    return it == nodes_.end() || it->second.directory ? nullptr : &it->second.data;
}
Status RamFileSystem::status() { return gate(false); }
Status RamFileSystem::list(const std::string& dir, std::vector<Entry>& out) {
    const Status st = gate(false);
    if (st != Status::Ok) return st;
    const std::string dk = key(dir);
    if (!dk.empty()) { auto it = nodes_.find(dk); if (it == nodes_.end() || !it->second.directory) return Status::NotFound; }
    out.clear();
    for (const auto& kv : nodes_)
        if (parentKey(kv.first) == dk) out.push_back({kv.second.name, static_cast<std::uint32_t>(kv.second.data.size()), kv.second.directory});
    return Status::Ok;
}
Status RamFileSystem::stat(const std::string& path, Entry& out) {
    const Status st = gate(false);
    if (st != Status::Ok) return st;
    auto it = nodes_.find(key(path));
    if (it == nodes_.end()) return Status::NotFound;
    out = {it->second.name, static_cast<std::uint32_t>(it->second.data.size()), it->second.directory};
    return Status::Ok;
}
Status RamFileSystem::openRead(const std::string& path, std::unique_ptr<Reader>& out) {
    const Status st = gate(false);
    if (st != Status::Ok) return st;
    auto it = nodes_.find(key(path));
    if (it == nodes_.end() || it->second.directory) return Status::NotFound;
    out = std::make_unique<RamReader>(it->second.data);
    return Status::Ok;
}
Status RamFileSystem::openWrite(const std::string& path, std::unique_ptr<Writer>& out) {
    const Status st = gate(true);
    if (st != Status::Ok) return st;
    const std::string k = key(path);
    if (k.empty()) return Status::InvalidName;
    const std::string parent = parentKey(k);
    if (!parent.empty()) { auto p = nodes_.find(parent); if (p == nodes_.end() || !p->second.directory) return Status::NotFound; }
    auto it = nodes_.find(k);
    if (it != nodes_.end() && it->second.directory) return Status::Exists;
    Node& n = nodes_[k];
    n.name = lastComponent(path); n.directory = false; n.data.clear();
    out = std::make_unique<RamWriter>(*this, k);
    return Status::Ok;
}
Status RamFileSystem::remove(const std::string& path) {
    const Status st = gate(true);
    if (st != Status::Ok) return st;
    const std::string k = key(path);
    auto it = nodes_.find(k);
    if (it == nodes_.end()) return Status::NotFound;
    if (it->second.directory)
        for (const auto& kv : nodes_) if (parentKey(kv.first) == k) return Status::IoError;   // not empty
    nodes_.erase(it);
    return Status::Ok;
}
Status RamFileSystem::rename(const std::string& from, const std::string& to) {
    const Status st = gate(true);
    if (st != Status::Ok) return st;
    const std::string a = key(from), b = key(to);
    auto it = nodes_.find(a);
    if (it == nodes_.end()) return Status::NotFound;
    if (a != b && nodes_.count(b)) return Status::Exists;
    const std::string parent = parentKey(b);
    if (!parent.empty()) { auto p = nodes_.find(parent); if (p == nodes_.end() || !p->second.directory) return Status::NotFound; }
    Node n = std::move(it->second);
    nodes_.erase(it);
    n.name = lastComponent(to);
    if (n.directory) {
        // Move the contents of a directory.
        std::vector<std::pair<std::string, Node>> moved;
        for (auto c = nodes_.begin(); c != nodes_.end();) {
            if (c->first.compare(0, a.size() + 1, a + "/") == 0) { moved.emplace_back(b + c->first.substr(a.size()), std::move(c->second)); c = nodes_.erase(c); }
            else ++c;
        }
        for (auto& m : moved) nodes_[m.first] = std::move(m.second);
    }
    nodes_[b] = std::move(n);
    return Status::Ok;
}
Status RamFileSystem::makeDirs(const std::string& path) {
    const Status st = gate(true);
    if (st != Status::Ok) return st;
    const std::string k = key(path);
    std::string orig;
    for (char c : path) if (!(c == '/' && (orig.empty() || orig.back() == '/'))) orig.push_back(c);
    std::size_t pos = 0;
    while (pos <= k.size() && !k.empty()) {
        const std::size_t slash = k.find('/', pos);
        const std::string part = k.substr(0, slash == std::string::npos ? k.size() : slash);
        auto it = nodes_.find(part);
        if (it == nodes_.end()) { Node n; n.name = lastComponent(orig.substr(0, part.size())); n.directory = true; nodes_[part] = std::move(n); }
        else if (!it->second.directory) return Status::Exists;
        if (slash == std::string::npos) break;
        pos = slash + 1;
    }
    return Status::Ok;
}
Status RamFileSystem::freeSpace(std::uint64_t& bytes) {
    const Status st = gate(false);
    if (st != Status::Ok) return st;
    const std::size_t u = used();
    bytes = capacity_ > u ? capacity_ - u : 0;
    return Status::Ok;
}
} // namespace fx::files
