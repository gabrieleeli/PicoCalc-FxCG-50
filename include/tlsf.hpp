#pragma once
// Two-Level Segregated Fit allocator (O(1) malloc/free, bounded fragmentation).
//
// Original implementation for this project, following the algorithm published
// by M. Masmano, I. Ripoll, A. Crespo, J. Real: "TLSF: a New Dynamic Memory
// Allocator for Real-Time Systems" (ECRTS 2004).
//
// Properties:
//  * one contiguous arena per instance (no growth, no sbrk);
//  * boundary tags with a physical-previous pointer -> immediate coalescing;
//  * arbitrary power-of-two alignment (leading gap becomes a free block);
//  * never touches memory outside [base, base+size);
//  * NOT thread-safe by itself: mem::Pool wraps it with a dual-core lock.
//
// The class is SDK-independent so it is exercised by the host test suite
// (stress, fragmentation, alignment, exhaustion, integrity walk).

#include <cstddef>
#include <cstdint>

namespace mem::detail {

struct TlsfStats {
    std::size_t totalBytes{};     // bytes managed by the arena (after alignment/sentinel)
    std::size_t usedBytes{};      // bytes held by allocated blocks (headers included)
    std::size_t freeBytes{};      // totalBytes - usedBytes
    std::size_t largestFree{};    // largest payload that a single allocation can obtain now
    std::size_t peakUsedBytes{};  // high-water mark of usedBytes
    std::size_t liveAllocations{};
    std::size_t totalAllocations{};
    std::size_t totalFrees{};
    std::size_t failures{};       // allocation requests that could not be satisfied
    std::size_t invalidFrees{};   // frees of foreign/double-freed pointers (ignored)
    std::size_t freeBlocks{};
};

class Tlsf {
public:
    static constexpr std::size_t kAlign = __STDCPP_DEFAULT_NEW_ALIGNMENT__ < 8 ? 8 : __STDCPP_DEFAULT_NEW_ALIGNMENT__;

    constexpr Tlsf() noexcept = default;
    Tlsf(const Tlsf&) = delete;
    Tlsf& operator=(const Tlsf&) = delete;

    // Takes ownership of [base, base+bytes). Returns false if the arena is too small.
    bool init(void* base, std::size_t bytes) noexcept;
    void reset() noexcept;                 // forget the arena (memory is not touched)
    bool ready() const noexcept { return first_ != nullptr; }

    void* allocate(std::size_t bytes, std::size_t align = kAlign) noexcept;
    // Returns false when `p` is not a live block of this arena (nothing is changed).
    bool deallocate(void* p) noexcept;
    bool owns(const void* p) const noexcept;
    std::size_t usableSize(const void* p) const noexcept;

    TlsfStats stats() const noexcept;
    // Walks every physical block and every free list; false on any inconsistency.
    bool check() const noexcept;

    std::uintptr_t arenaBegin() const noexcept { return arenaBegin_; }
    std::uintptr_t arenaEnd() const noexcept { return arenaEnd_; }

    static constexpr std::size_t kSlLog2 = 5;
    static constexpr std::size_t kSlCount = std::size_t{1} << kSlLog2;
    static constexpr std::size_t kAlignLog2 = kAlign == 8 ? 3 : (kAlign == 16 ? 4 : 5);
    static constexpr std::size_t kFlShift = kSlLog2 + kAlignLog2;
    static constexpr std::size_t kSmallBlock = std::size_t{1} << kFlShift;
    static constexpr std::size_t kFlMaxLog2 = 30;  // arenas up to 1 GiB (host tests); device uses <= 8 MiB
    static constexpr std::size_t kFlCount = kFlMaxLog2 - kFlShift + 1;

private:
    struct Block {
        std::size_t sizeFlags;   // total block size (header included) | kFreeBit
        Block* prevPhys;         // physically previous block, nullptr for the first one
        Block* nextFree;         // valid only while free
        Block* prevFree;         // valid only while free
    };
    static constexpr std::size_t kFreeBit = 1;
    static constexpr std::size_t kHeader = (sizeof(std::size_t) + sizeof(Block*) + kAlign - 1) & ~(kAlign - 1);
    static constexpr std::size_t kMinBlock = (kHeader + 2 * sizeof(Block*) + kAlign - 1) & ~(kAlign - 1);

    static std::size_t sizeOf(const Block* b) noexcept { return b->sizeFlags & ~kFreeBit; }
    static bool isFree(const Block* b) noexcept { return (b->sizeFlags & kFreeBit) != 0; }
    static Block* next(const Block* b) noexcept;
    static void* payload(const Block* b) noexcept;
    static Block* fromPayload(const void* p) noexcept;
    static void mappingInsert(std::size_t size, std::size_t& fl, std::size_t& sl) noexcept;
    static bool mappingSearch(std::size_t size, std::size_t& fl, std::size_t& sl) noexcept;

    void insertFree(Block* b) noexcept;
    void removeFree(Block* b) noexcept;
    Block* findSuitable(std::size_t& fl, std::size_t& sl) const noexcept;
    Block* splitTail(Block* b, std::size_t size) noexcept;   // keeps the head, frees the tail if large enough
    Block* mergeNeighbours(Block* b) noexcept;
    bool isLiveBlock(const Block* b) const noexcept;

    std::uint32_t flBitmap_{};
    std::uint32_t slBitmap_[kFlCount]{};
    Block* heads_[kFlCount][kSlCount]{};
    Block* first_{};
    Block* sentinel_{};
    std::uintptr_t arenaBegin_{}, arenaEnd_{};
    std::size_t total_{}, used_{}, peak_{}, live_{}, allocs_{}, frees_{}, failures_{}, invalid_{}, freeBlocks_{};
};

} // namespace mem::detail
