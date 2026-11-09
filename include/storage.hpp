#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace storage {

std::uint32_t crc32(const void* data, std::size_t length, std::uint32_t seed = 0) noexcept;

// NOR-flash region abstraction: erase sets bytes to 0xFF, program can only
// clear bits. Offsets are relative to the start of the region.
class FlashDevice {
public:
    virtual ~FlashDevice() = default;
    virtual std::size_t size() const = 0;
    virtual std::size_t eraseSize() const = 0;      // 4096 on RP2350
    virtual std::size_t programSize() const = 0;    // 256 on RP2350
    virtual bool read(std::size_t offset, void* dst, std::size_t length) = 0;
    virtual bool erase(std::size_t offset, std::size_t length) = 0;
    virtual bool program(std::size_t offset, const void* src, std::size_t length) = 0;
    // Bytes at the end of the region that must never be written. On the RP2350
    // the bootrom's workaround for erratum RP2350-E10 overwrites the last flash
    // block during a UF2 drag-and-drop update (pico-sdk PR #2412): the Pico 2 W
    // record store, kept at its v1.8.0 location (the last 128 KB), reports 4 KB.
    virtual std::size_t reservedTail() const { return 0; }
};

// RAM model with NOR semantics and power-loss injection (host tests, simulator).
class RamFlash final : public FlashDevice {
public:
    RamFlash(std::size_t size, std::size_t eraseSize = 4096, std::size_t programSize = 256);
    std::size_t size() const override { return data_.size(); }
    std::size_t eraseSize() const override { return erase_; }
    std::size_t programSize() const override { return program_; }
    bool read(std::size_t offset, void* dst, std::size_t length) override;
    bool erase(std::size_t offset, std::size_t length) override;
    bool program(std::size_t offset, const void* src, std::size_t length) override;
    // After `budget` more programmed/erased bytes the device "loses power":
    // the current operation is truncated and every later operation fails.
    void failAfter(std::size_t budget) { budget_ = budget; armed_ = true; }
    void powerOn() { armed_ = false; dead_ = false; }
    std::vector<std::uint8_t>& raw() { return data_; }
    std::size_t reservedTail() const override { return tail_; }
    // Writes into the reserved tail fail, as on the device.
    void setReservedTail(std::size_t bytes) { tail_ = bytes; }
    std::size_t writes() const noexcept { return writes_; }   // erase + program calls that succeeded
private:
    bool consume(std::size_t& length);
    bool inTail(std::size_t offset, std::size_t length) const { return tail_ && offset + length > data_.size() - tail_; }
    std::vector<std::uint8_t> data_;
    std::size_t erase_, program_;
    std::size_t budget_{};
    std::size_t tail_{};
    std::size_t writes_{};
    bool armed_{}, dead_{};
};

// Read-only view of a part of another device (migration of old records).
class FlashWindow final : public FlashDevice {
public:
    FlashWindow(FlashDevice& base, std::size_t offset, std::size_t size) : base_(base), offset_(offset), size_(size) {}
    std::size_t size() const override { return size_; }
    std::size_t eraseSize() const override { return base_.eraseSize(); }
    std::size_t programSize() const override { return base_.programSize(); }
    bool read(std::size_t offset, void* dst, std::size_t length) override {
        return offset + length <= size_ && base_.read(offset_ + offset, dst, length);
    }
    bool erase(std::size_t, std::size_t) override { return false; }
    bool program(std::size_t, const void*, std::size_t) override { return false; }
private:
    FlashDevice& base_;
    std::size_t offset_, size_;
};

// Two-slot (A/B) record store. Each save goes to the slot that does not hold
// the newest valid record; the header (magic, sequence, length, CRCs) is
// programmed last, so an interrupted save never destroys the previous record.
// Saves never write the device's reserved tail; records that an older
// firmware stored there are still read (they are CRC-checked like any other).
class RecordStore {
public:
    explicit RecordStore(FlashDevice& device);
    std::size_t capacity() const noexcept;          // maximum payload size of a save
    bool load(std::vector<std::uint8_t>& out);
    bool save(const std::vector<std::uint8_t>& payload);
    bool eraseAll();
    std::uint32_t sequence() const noexcept { return seq_; }
    int activeSlot() const noexcept { return slot_; }
private:
    struct Header {
        std::uint32_t magic, version, sequence, length, payloadCrc, headerCrc;
    };
    bool readHeader(int slot, Header& h);
    bool slotValid(int slot, Header& h);
    std::size_t slotOffset(int slot) const noexcept { return static_cast<std::size_t>(slot) * slotSize_; }
    std::size_t readCapacity() const noexcept;      // largest payload accepted when loading
    FlashDevice& dev_;
    std::size_t slotSize_;
    std::uint32_t seq_{};
    int slot_{-1};
};
} // namespace storage
