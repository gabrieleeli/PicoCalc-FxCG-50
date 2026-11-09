#include "tlsf.hpp"

#include <cstdint>

namespace mem::detail {
namespace {
constexpr std::size_t alignUp(std::size_t v, std::size_t a) noexcept { return (v + a - 1) & ~(a - 1); }
constexpr std::uintptr_t alignUpPtr(std::uintptr_t v, std::size_t a) noexcept {
    return (v + a - 1) & ~static_cast<std::uintptr_t>(a - 1);
}
inline unsigned fls(std::size_t x) noexcept { // index of the most significant set bit (x != 0)
    return 63u - static_cast<unsigned>(__builtin_clzll(static_cast<unsigned long long>(x)));
}
inline unsigned ffs32(std::uint32_t x) noexcept { return static_cast<unsigned>(__builtin_ctz(x)); }
inline unsigned fls32(std::uint32_t x) noexcept { return 31u - static_cast<unsigned>(__builtin_clz(x)); }
} // namespace

Tlsf::Block* Tlsf::next(const Block* b) noexcept {
    return reinterpret_cast<Block*>(reinterpret_cast<std::uintptr_t>(b) + sizeOf(b));
}
void* Tlsf::payload(const Block* b) noexcept {
    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(b) + kHeader);
}
Tlsf::Block* Tlsf::fromPayload(const void* p) noexcept {
    return reinterpret_cast<Block*>(reinterpret_cast<std::uintptr_t>(p) - kHeader);
}

void Tlsf::mappingInsert(std::size_t size, std::size_t& fl, std::size_t& sl) noexcept {
    if (size < kSmallBlock) {
        fl = 0;
        sl = size / (kSmallBlock / kSlCount);
    } else {
        const unsigned f = fls(size);
        sl = (size >> (f - kSlLog2)) ^ kSlCount;
        fl = f - (kFlShift - 1);
    }
}

bool Tlsf::mappingSearch(std::size_t size, std::size_t& fl, std::size_t& sl) noexcept {
    if (size >= kSmallBlock) {
        // Round up to the next list so that every block of the selected list fits.
        const std::size_t round = (std::size_t{1} << (fls(size) - kSlLog2)) - 1;
        if (size > static_cast<std::size_t>(-1) - round) return false;
        size += round;
    }
    mappingInsert(size, fl, sl);
    return fl < kFlCount;
}

void Tlsf::reset() noexcept {
    flBitmap_ = 0;
    for (auto& m : slBitmap_) m = 0;
    for (auto& row : heads_)
        for (auto& h : row) h = nullptr;
    first_ = sentinel_ = nullptr;
    arenaBegin_ = arenaEnd_ = 0;
    total_ = used_ = peak_ = live_ = allocs_ = frees_ = failures_ = invalid_ = freeBlocks_ = 0;
}

bool Tlsf::init(void* base, std::size_t bytes) noexcept {
    reset();
    if (!base) return false;
    const std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(base);
    if (bytes > static_cast<std::uintptr_t>(-1) - raw) return false;
    const std::uintptr_t begin = alignUpPtr(raw, kAlign);
    const std::uintptr_t end = (raw + bytes) & ~static_cast<std::uintptr_t>(kAlign - 1);
    if (end <= begin || end - begin < kMinBlock + kHeader) return false;
    std::size_t firstSize = static_cast<std::size_t>(end - begin) - kHeader; // room for the sentinel
    const std::size_t maxBlock = (std::size_t{1} << kFlMaxLog2) - kAlign;
    if (firstSize > maxBlock) firstSize = maxBlock;
    firstSize &= ~(kAlign - 1);

    first_ = reinterpret_cast<Block*>(begin);
    first_->sizeFlags = firstSize | kFreeBit;
    first_->prevPhys = nullptr;
    sentinel_ = next(first_);
    sentinel_->sizeFlags = 0; // permanently "used", size 0: stops forward coalescing
    sentinel_->prevPhys = first_;
    arenaBegin_ = begin;
    arenaEnd_ = reinterpret_cast<std::uintptr_t>(sentinel_) + kHeader;
    total_ = firstSize;
    insertFree(first_);
    return true;
}

void Tlsf::insertFree(Block* b) noexcept {
    std::size_t fl = 0, sl = 0;
    mappingInsert(sizeOf(b), fl, sl);
    Block* head = heads_[fl][sl];
    b->nextFree = head;
    b->prevFree = nullptr;
    if (head) head->prevFree = b;
    heads_[fl][sl] = b;
    flBitmap_ |= std::uint32_t{1} << fl;
    slBitmap_[fl] |= std::uint32_t{1} << sl;
    ++freeBlocks_;
}

void Tlsf::removeFree(Block* b) noexcept {
    std::size_t fl = 0, sl = 0;
    mappingInsert(sizeOf(b), fl, sl);
    if (b->prevFree) b->prevFree->nextFree = b->nextFree;
    else heads_[fl][sl] = b->nextFree;
    if (b->nextFree) b->nextFree->prevFree = b->prevFree;
    if (!heads_[fl][sl]) {
        slBitmap_[fl] &= ~(std::uint32_t{1} << sl);
        if (!slBitmap_[fl]) flBitmap_ &= ~(std::uint32_t{1} << fl);
    }
    --freeBlocks_;
}

Tlsf::Block* Tlsf::findSuitable(std::size_t& fl, std::size_t& sl) const noexcept {
    std::uint32_t slMap = slBitmap_[fl] & (~std::uint32_t{0} << sl);
    if (!slMap) {
        if (fl + 1 >= kFlCount) return nullptr;
        const std::uint32_t flMap = flBitmap_ & (~std::uint32_t{0} << (fl + 1));
        if (!flMap) return nullptr;
        fl = ffs32(flMap);
        slMap = slBitmap_[fl];
    }
    sl = ffs32(slMap);
    return heads_[fl][sl];
}

Tlsf::Block* Tlsf::splitTail(Block* b, std::size_t size) noexcept {
    const std::size_t have = sizeOf(b);
    if (have < size + kMinBlock) return nullptr;
    Block* rest = reinterpret_cast<Block*>(reinterpret_cast<std::uintptr_t>(b) + size);
    rest->sizeFlags = (have - size) | kFreeBit;
    rest->prevPhys = b;
    b->sizeFlags = size | (b->sizeFlags & kFreeBit);
    Block* after = next(rest);
    after->prevPhys = rest;
    if (isFree(after)) {
        removeFree(after);
        rest->sizeFlags = (sizeOf(rest) + sizeOf(after)) | kFreeBit;
        next(rest)->prevPhys = rest;
    }
    insertFree(rest);
    return rest;
}

Tlsf::Block* Tlsf::mergeNeighbours(Block* b) noexcept {
    Block* n = next(b);
    if (isFree(n)) {
        removeFree(n);
        b->sizeFlags = (sizeOf(b) + sizeOf(n)) | kFreeBit;
        next(b)->prevPhys = b;
    }
    Block* p = b->prevPhys;
    if (p && isFree(p)) {
        removeFree(p);
        p->sizeFlags = (sizeOf(p) + sizeOf(b)) | kFreeBit;
        next(p)->prevPhys = p;
        b = p;
    }
    return b;
}

void* Tlsf::allocate(std::size_t bytes, std::size_t align) noexcept {
    if (!first_) { ++failures_; return nullptr; }
    if (align < kAlign) align = kAlign;
    if ((align & (align - 1)) != 0) { ++failures_; return nullptr; }
    if (bytes == 0) bytes = 1;
    const std::size_t maxRequest = std::size_t{1} << kFlMaxLog2;
    if (bytes > maxRequest || align > maxRequest) { ++failures_; return nullptr; }

    std::size_t size = alignUp(bytes, kAlign) + kHeader;
    if (size < kMinBlock) size = kMinBlock;
    const std::size_t gapAllowance = align > kAlign ? align + kMinBlock : 0;
    std::size_t fl = 0, sl = 0;
    if (!mappingSearch(size + gapAllowance, fl, sl)) { ++failures_; return nullptr; }
    Block* b = findSuitable(fl, sl);
    if (!b) { ++failures_; return nullptr; }
    removeFree(b);
    b->sizeFlags &= ~kFreeBit;

    if (align > kAlign) {
        const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(payload(b));
        std::uintptr_t aligned = alignUpPtr(start, align);
        std::size_t gap = static_cast<std::size_t>(aligned - start);
        if (gap != 0 && gap < kMinBlock) {
            aligned = alignUpPtr(start + kMinBlock, align);
            gap = static_cast<std::size_t>(aligned - start);
        }
        if (gap) {
            // The leading gap becomes a free block; its physical predecessor is
            // necessarily in use because free neighbours are always merged.
            const std::size_t whole = sizeOf(b);
            Block* moved = reinterpret_cast<Block*>(reinterpret_cast<std::uintptr_t>(b) + gap);
            moved->sizeFlags = whole - gap;
            moved->prevPhys = b;
            next(moved)->prevPhys = moved;
            b->sizeFlags = gap | kFreeBit;
            insertFree(b);
            b = moved;
        }
    }
    splitTail(b, size);

    used_ += sizeOf(b);
    if (used_ > peak_) peak_ = used_;
    ++live_;
    ++allocs_;
    return payload(b);
}

bool Tlsf::owns(const void* p) const noexcept {
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    return first_ && a >= arenaBegin_ + kHeader && a < arenaEnd_;
}

bool Tlsf::isLiveBlock(const Block* b) const noexcept {
    const auto a = reinterpret_cast<std::uintptr_t>(b);
    if ((a & (kAlign - 1)) != 0 || b < first_ || b >= sentinel_) return false;
    if (isFree(b)) return false; // double free
    const std::size_t size = sizeOf(b);
    if (size < kMinBlock || (size & (kAlign - 1)) != 0) return false;
    const auto end = a + size;
    if (end > reinterpret_cast<std::uintptr_t>(sentinel_)) return false;
    if (next(b)->prevPhys != b) return false;
    if (b == first_) return b->prevPhys == nullptr;
    return b->prevPhys != nullptr && next(b->prevPhys) == b;
}

bool Tlsf::deallocate(void* p) noexcept {
    if (!p) return true;
    if (!owns(p)) { ++invalid_; return false; }
    Block* b = fromPayload(p);
    if (!isLiveBlock(b)) { ++invalid_; return false; }
    used_ -= sizeOf(b);
    --live_;
    ++frees_;
    b->sizeFlags |= kFreeBit;
    insertFree(mergeNeighbours(b));
    return true;
}

std::size_t Tlsf::usableSize(const void* p) const noexcept {
    if (!p || !owns(p)) return 0;
    const Block* b = fromPayload(p);
    return isLiveBlock(b) ? sizeOf(b) - kHeader : 0;
}

TlsfStats Tlsf::stats() const noexcept {
    TlsfStats s;
    s.totalBytes = total_;
    s.usedBytes = used_;
    s.freeBytes = total_ - used_;
    s.peakUsedBytes = peak_;
    s.liveAllocations = live_;
    s.totalAllocations = allocs_;
    s.totalFrees = frees_;
    s.failures = failures_;
    s.invalidFrees = invalid_;
    s.freeBlocks = freeBlocks_;
    if (flBitmap_) {
        // Largest request that is *guaranteed* to succeed: the lower bound of
        // the highest non-empty list (requests are rounded up to a list).
        const std::size_t fl = fls32(flBitmap_);
        const std::size_t sl = fls32(slBitmap_[fl]);
        std::size_t lower = 0;
        if (fl == 0) lower = sl * (kSmallBlock / kSlCount);
        else {
            const std::size_t base = std::size_t{1} << (fl + kFlShift - 1);
            lower = base + sl * (base >> kSlLog2);
        }
        s.largestFree = lower > kHeader ? lower - kHeader : 0;
    }
    return s;
}

bool Tlsf::check() const noexcept {
    if (!first_) return true;
    std::size_t usedSeen = 0, freeSeen = 0, freeBytesSeen = 0;
    const Block* prev = nullptr;
    bool prevFree = false;
    for (const Block* b = first_; b != sentinel_; b = next(b)) {
        if (reinterpret_cast<std::uintptr_t>(b) >= reinterpret_cast<std::uintptr_t>(sentinel_)) return false;
        const std::size_t size = sizeOf(b);
        if (size < kMinBlock || (size & (kAlign - 1)) != 0) return false;
        if (b->prevPhys != prev) return false;
        if (isFree(b)) {
            if (prevFree) return false; // two adjacent free blocks: coalescing failed
            ++freeSeen;
            freeBytesSeen += size;
        } else {
            usedSeen += size;
        }
        prevFree = isFree(b);
        prev = b;
    }
    if (sentinel_->prevPhys != prev || sizeOf(sentinel_) != 0) return false;
    if (usedSeen != used_ || freeSeen != freeBlocks_ || usedSeen + freeBytesSeen != total_) return false;

    std::size_t listed = 0;
    for (std::size_t fl = 0; fl < kFlCount; ++fl) {
        const bool flBit = (flBitmap_ >> fl) & 1u;
        if (flBit != (slBitmap_[fl] != 0)) return false;
        for (std::size_t sl = 0; sl < kSlCount; ++sl) {
            const Block* head = heads_[fl][sl];
            const bool slBit = (slBitmap_[fl] >> sl) & 1u;
            if (slBit != (head != nullptr)) return false;
            const Block* before = nullptr;
            for (const Block* b = head; b; b = b->nextFree) {
                if (!isFree(b) || b->prevFree != before) return false;
                std::size_t f = 0, s = 0;
                mappingInsert(sizeOf(b), f, s);
                if (f != fl || s != sl) return false;
                if (++listed > freeBlocks_) return false; // cycle guard
                before = b;
            }
        }
    }
    return listed == freeBlocks_;
}

} // namespace mem::detail
