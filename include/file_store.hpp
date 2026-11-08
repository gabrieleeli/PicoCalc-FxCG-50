#pragma once
// User files on the microSD card: a small portable interface shaped after
// FatFs, safe replacement of files and a RAM card with fault injection.
//  - RP2350: FatFs on the PicoCalc microSD slot (src/platform/pico/sd_card.cpp,
//    src/fatfs_store.cpp);
//  - host tests and simulator: RamFileSystem (no card, full card, I/O errors,
//    power loss after n bytes, failure of the n-th operation).
// Paths use '/' and are case-insensitive like FAT: "PICOCALC/GRAPH/SINE.pcgraph".
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fx::files {

enum class Status : std::uint8_t { Ok = 0, NoCard, NotFound, Exists, Full, IoError, Corrupt, InvalidName, TooLarge, NotSupported };
const char* statusMessage(Status s) noexcept;   // text for the error boxes ("No SD card", ...)

struct Entry { std::string name; std::uint32_t size{}; bool directory{}; };

class Writer {
public:
    virtual ~Writer() = default;
    virtual Status write(const void* data, std::size_t length) = 0;
    virtual Status close() = 0;                 // the file is complete only after Ok
};
class Reader {
public:
    virtual ~Reader() = default;
    virtual Status read(void* data, std::size_t length, std::size_t& got) = 0;   // got == 0 at the end
};

class FileSystem {
public:
    virtual ~FileSystem() = default;
    virtual const char* name() const noexcept = 0;
    // Ok when a card is inserted and mounted (mounts on demand, re-mounts after a change).
    virtual Status status() = 0;
    virtual Status list(const std::string& dir, std::vector<Entry>& out) = 0;
    virtual Status stat(const std::string& path, Entry& out) = 0;
    virtual Status openRead(const std::string& path, std::unique_ptr<Reader>& out) = 0;
    virtual Status openWrite(const std::string& path, std::unique_ptr<Writer>& out) = 0;   // creates or truncates
    virtual Status remove(const std::string& path) = 0;
    virtual Status rename(const std::string& from, const std::string& to) = 0;           // Exists if `to` exists
    virtual Status makeDirs(const std::string& path) = 0;                                 // Ok if it exists
    virtual Status freeSpace(std::uint64_t& bytes) = 0;
};

Status readAll(FileSystem& fs, const std::string& path, std::vector<std::uint8_t>& out, std::size_t maxBytes);
// Safe replacement of a file:
//   1. the data goes to "<path>.tmp", which is read back and compared (CRC-32);
//   2. an existing "<path>" becomes "<path>.bak";
//   3. "<path>.tmp" becomes "<path>";  4. "<path>.bak" is removed.
// After an interruption either the old or the new complete file exists;
// recover() finishes or rolls back the operation.
Status writeAtomic(FileSystem& fs, const std::string& path, const std::uint8_t* data, std::size_t size);
Status writeAtomic(FileSystem& fs, const std::string& path, const std::function<Status(Writer&)>& producer);
Status recover(FileSystem& fs, const std::string& dir);

// 1..maxLength characters A-Z a-z 0-9 _ - (fx-CG50 file names have 8).
bool validName(const std::string& name, std::size_t maxLength = 8);
std::string join(const std::string& dir, const std::string& name);
bool hasExtension(const std::string& name, const char* ext);   // case-insensitive, ext with the dot
std::string stem(const std::string& name);                      // name without the last extension

// Folders used on the card.
constexpr const char* GraphFolder = "PICOCALC/GRAPH";
constexpr const char* PictFolder = "PICOCALC/PICT";
constexpr const char* ProjectExtension = ".pcgraph";
constexpr const char* PictExtension = ".pcpict";
constexpr const char* BmpExtension = ".bmp";

class RamFileSystem final : public FileSystem {
public:
    explicit RamFileSystem(std::size_t capacity = 8u * 1024u * 1024u);
    const char* name() const noexcept override { return "RAM"; }
    Status status() override;
    Status list(const std::string& dir, std::vector<Entry>& out) override;
    Status stat(const std::string& path, Entry& out) override;
    Status openRead(const std::string& path, std::unique_ptr<Reader>& out) override;
    Status openWrite(const std::string& path, std::unique_ptr<Writer>& out) override;
    Status remove(const std::string& path) override;
    Status rename(const std::string& from, const std::string& to) override;
    Status makeDirs(const std::string& path) override;
    Status freeSpace(std::uint64_t& bytes) override;

    // Fault injection.
    void setPresent(bool present) noexcept { present_ = present; }
    void setCapacity(std::size_t bytes) noexcept { capacity_ = bytes; }
    // Power loss: the write that reaches `bytes` more written bytes is cut
    // there and every later operation fails until powerOn().
    void failAfterBytes(std::size_t bytes) noexcept { budget_ = bytes; armed_ = true; }
    // The n-th next modifying operation (open for writing, write, close, rename,
    // remove, mkdir) fails with IoError; n = 1 is the next one.
    void failOperation(int n) noexcept { failAt_ = n; }
    void powerOn() noexcept { dead_ = false; armed_ = false; failAt_ = 0; }
    std::size_t used() const noexcept;
    bool exists(const std::string& path) const;
    std::vector<std::uint8_t>* data(const std::string& path);     // test access to a file
private:
    friend class RamWriter;
    struct Node { std::string name; bool directory{}; std::vector<std::uint8_t> data; };
    static std::string key(const std::string& path);
    static std::string parentKey(const std::string& key);
    Status gate(bool modifying);                                   // fault checks
    std::size_t consume(std::size_t want);                         // power-loss budget
    std::map<std::string, Node> nodes_;
    std::size_t capacity_;
    bool present_{true}, armed_{false}, dead_{false};
    std::size_t budget_{};
    int failAt_{0};
};
} // namespace fx::files
