#include "storage.hpp"
#include <algorithm>
#include <cstring>

namespace storage {
namespace {
constexpr std::uint32_t Magic = 0x58464350;   // "PCFX"
constexpr std::uint32_t FormatVersion = 1;
constexpr std::size_t HeaderSize = sizeof(std::uint32_t) * 6;
}

std::uint32_t crc32(const void* data, std::size_t n, std::uint32_t seed) noexcept {
    std::uint32_t c = ~seed;
    const auto* p = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

RamFlash::RamFlash(std::size_t size, std::size_t eraseSize, std::size_t programSize)
    : data_(size, 0xFF), erase_(eraseSize), program_(programSize) {}
bool RamFlash::consume(std::size_t& length) {
    if (dead_) return false;
    if (!armed_) return true;
    if (length > budget_) { length = budget_; budget_ = 0; dead_ = true; return true; }
    budget_ -= length;
    return true;
}
bool RamFlash::read(std::size_t off, void* dst, std::size_t len) {
    if (off + len > data_.size()) return false;
    std::memcpy(dst, data_.data() + off, len);
    return true;
}
bool RamFlash::erase(std::size_t off, std::size_t len) {
    if (off % erase_ || len % erase_ || off + len > data_.size()) return false;
    std::size_t n = len;
    if (!consume(n)) return false;
    std::fill(data_.begin() + static_cast<std::ptrdiff_t>(off), data_.begin() + static_cast<std::ptrdiff_t>(off + n), 0xFF);
    return n == len;
}
bool RamFlash::program(std::size_t off, const void* src, std::size_t len) {
    if (off % program_ || len % program_ || off + len > data_.size()) return false;
    std::size_t n = len;
    if (!consume(n)) return false;
    const auto* s = static_cast<const std::uint8_t*>(src);
    for (std::size_t i = 0; i < n; ++i) data_[off + i] &= s[i];   // NOR: only 1 -> 0
    return n == len;
}

RecordStore::RecordStore(FlashDevice& d) : dev_(d), slotSize_((d.size() / 2) / d.eraseSize() * d.eraseSize()) {}
std::size_t RecordStore::capacity() const noexcept {
    const std::size_t page = dev_.programSize();
    return slotSize_ > page ? slotSize_ - page : 0;
}
bool RecordStore::readHeader(int slot, Header& h) {
    std::uint32_t w[6];
    if (!dev_.read(slotOffset(slot), w, sizeof(w))) return false;
    h = {w[0], w[1], w[2], w[3], w[4], w[5]};
    return true;
}
bool RecordStore::slotValid(int slot, Header& h) {
    if (!readHeader(slot, h)) return false;
    if (h.magic != Magic || h.version != FormatVersion) return false;
    if (crc32(&h, HeaderSize - 4) != h.headerCrc) return false;
    if (h.length > capacity()) return false;
    std::vector<std::uint8_t> buf(h.length);
    if (h.length && !dev_.read(slotOffset(slot) + dev_.programSize(), buf.data(), h.length)) return false;
    return crc32(buf.data(), buf.size()) == h.payloadCrc;
}
bool RecordStore::load(std::vector<std::uint8_t>& out) {
    Header a{}, b{};
    const bool va = slotValid(0, a), vb = slotValid(1, b);
    if (!va && !vb) { slot_ = -1; seq_ = 0; return false; }
    // Newest valid record wins (sequence numbers are compared with wrap-around).
    int pick = 0;
    if (va && vb) pick = static_cast<std::int32_t>(b.sequence - a.sequence) > 0 ? 1 : 0;
    else pick = va ? 0 : 1;
    const Header& h = pick == 0 ? a : b;
    out.resize(h.length);
    if (h.length && !dev_.read(slotOffset(pick) + dev_.programSize(), out.data(), h.length)) return false;
    slot_ = pick; seq_ = h.sequence;
    return true;
}
bool RecordStore::save(const std::vector<std::uint8_t>& payload) {
    if (payload.size() > capacity()) return false;
    if (slot_ < 0) { std::vector<std::uint8_t> tmp; (void)load(tmp); }
    const int target = slot_ == 0 ? 1 : 0;
    const std::size_t base = slotOffset(target), page = dev_.programSize();
    const std::size_t used = page + (payload.size() + page - 1) / page * page;
    const std::size_t eraseLen = (used + dev_.eraseSize() - 1) / dev_.eraseSize() * dev_.eraseSize();
    if (!dev_.erase(base, eraseLen)) return false;
    // 1) payload pages
    std::vector<std::uint8_t> buf(page);
    for (std::size_t off = 0; off < payload.size(); off += page) {
        std::fill(buf.begin(), buf.end(), 0xFF);
        std::memcpy(buf.data(), payload.data() + off, std::min(page, payload.size() - off));
        if (!dev_.program(base + page + off, buf.data(), page)) return false;
    }
    // 2) header page last: the record only becomes valid once it is complete.
    Header h{Magic, FormatVersion, seq_ + 1, static_cast<std::uint32_t>(payload.size()), crc32(payload.data(), payload.size()), 0};
    h.headerCrc = crc32(&h, HeaderSize - 4);
    std::fill(buf.begin(), buf.end(), 0xFF);
    std::memcpy(buf.data(), &h, sizeof(h));
    if (!dev_.program(base, buf.data(), page)) return false;
    Header check{};
    if (!slotValid(target, check)) return false;   // read-back verification
    slot_ = target; seq_ = h.sequence;
    return true;
}
bool RecordStore::eraseAll() {
    slot_ = -1; seq_ = 0;
    return dev_.erase(0, slotSize_ * 2);
}
} // namespace storage
