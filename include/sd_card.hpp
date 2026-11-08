#pragma once
// SD card in SPI mode (SD Physical Layer Simplified Specification): power-up
// clocks, CMD0, CMD8, ACMD41 (HCS), CMD58 (CCS), CMD16, CMD9 (CSD capacity),
// CMD17/CMD24 single-block transfers with data tokens, busy waits and
// timeouts. The bus is abstract: the RP2350 build drives SPI0 of the PicoCalc
// (src/platform/pico/sd_spi_pico.cpp); the host tests use a simulated card,
// so the whole protocol is exercised without hardware.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fx::sd {

class SpiBus {
public:
    virtual ~SpiBus() = default;
    virtual void setClock(std::uint32_t hz) = 0;
    virtual void select(bool selected) = 0;              // chip select (active low on the wire)
    virtual std::uint8_t transfer(std::uint8_t out) = 0; // one full-duplex byte
    virtual void read(std::uint8_t* dst, std::size_t n) { for (std::size_t i = 0; i < n; ++i) dst[i] = transfer(0xFF); }
    virtual void write(const std::uint8_t* src, std::size_t n) { for (std::size_t i = 0; i < n; ++i) (void)transfer(src[i]); }
    virtual bool cardPresent() = 0;                      // card-detect switch
    virtual std::uint32_t millis() = 0;
};

// 512-byte sector device used by the FAT file system.
class BlockDevice {
public:
    static constexpr std::size_t SectorSize = 512;
    virtual ~BlockDevice() = default;
    virtual bool present() = 0;
    virtual bool init() = 0;                             // (re)initialises the medium
    virtual bool ready() const = 0;                      // initialised and usable
    virtual std::uint64_t sectors() const = 0;
    virtual bool read(std::uint64_t lba, std::uint8_t* dst, std::uint32_t count) = 0;
    virtual bool write(std::uint64_t lba, const std::uint8_t* src, std::uint32_t count) = 0;
    virtual bool sync() = 0;
};

class SdCard final : public BlockDevice {
public:
    enum class Type : std::uint8_t { None, SDv1, SDv2, SDHC };   // SDHC also covers SDXC (block addressing)
    static constexpr std::uint32_t InitClockHz = 400000;
    static constexpr std::uint32_t FastClockHz = 12500000;
    explicit SdCard(SpiBus& bus) : bus_(bus) {}
    bool present() override { return bus_.cardPresent(); }
    bool init() override;
    bool ready() const override { return type_ != Type::None; }
    std::uint64_t sectors() const override { return sectors_; }
    bool read(std::uint64_t lba, std::uint8_t* dst, std::uint32_t count) override;
    bool write(std::uint64_t lba, const std::uint8_t* src, std::uint32_t count) override;
    bool sync() override;
    Type type() const noexcept { return type_; }
    void reset() noexcept { type_ = Type::None; sectors_ = 0; }
    std::uint32_t errors() const noexcept { return errors_; }
private:
    std::uint8_t command(std::uint8_t cmd, std::uint32_t arg);   // R1 (0xFF on timeout)
    std::uint8_t appCommand(std::uint8_t cmd, std::uint32_t arg);
    bool waitReady(std::uint32_t timeoutMs);
    bool receive(std::uint8_t* dst, std::size_t n, std::uint32_t timeoutMs);
    bool transmit(const std::uint8_t* src, std::uint8_t token);
    void deselect();
    bool fail();
    std::uint32_t address(std::uint64_t lba) const { return type_ == Type::SDHC ? static_cast<std::uint32_t>(lba) : static_cast<std::uint32_t>(lba * SectorSize); }
    SpiBus& bus_;
    Type type_{Type::None};
    std::uint64_t sectors_{0};
    std::uint32_t errors_{0};
};

// RAM-backed sectors (host tests).
class RamDisk final : public BlockDevice {
public:
    explicit RamDisk(std::size_t sectors) : data_(sectors * SectorSize, 0) {}
    bool present() override { return present_; }
    bool init() override { ready_ = present_; return ready_; }
    bool ready() const override { return ready_; }
    std::uint64_t sectors() const override { return data_.size() / SectorSize; }
    bool read(std::uint64_t lba, std::uint8_t* dst, std::uint32_t count) override;
    bool write(std::uint64_t lba, const std::uint8_t* src, std::uint32_t count) override;
    bool sync() override { return ready_; }
    void setPresent(bool p) { present_ = p; if (!p) ready_ = false; }
    void failWrites(bool f) { failWrites_ = f; }
private:
    std::vector<std::uint8_t> data_;
    bool present_{true}, ready_{false}, failWrites_{false};
};
} // namespace fx::sd
