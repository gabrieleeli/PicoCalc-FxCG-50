#include "mem_pools.hpp"

#include "board_config.hpp"
#include "tlsf.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>

#if FXAI_ON_DEVICE
#include "hardware/sync.h"
#include "pico/critical_section.h"
#include "pico/platform.h"
#include <malloc.h>
#include <unistd.h>
extern "C" char __end__;      // first byte of the newlib heap (linker script)
extern "C" char __HeapLimit;  // end of the heap region (linker script)
#else
#include <malloc.h>
#include <mutex>
#endif

namespace mem {
namespace {

// ---------------------------------------------------------------------------
// Locking: critical_section = spin lock shared by both cores + IRQ masking on
// the calling core, so allocation is safe from either core (never call it from
// an ISR that can preempt a flash write, see flash_fs.cpp).
#if FXAI_ON_DEVICE
class PoolLock {
public:
    void init() noexcept {
        if (!ready_) { critical_section_init(&cs_); ready_ = true; }
    }
    void lock() noexcept { if (ready_) critical_section_enter_blocking(&cs_); }
    void unlock() noexcept { if (ready_) critical_section_exit(&cs_); }
private:
    critical_section_t cs_{};
    bool ready_{};
};
#else
class PoolLock {
public:
    void init() noexcept {}
    void lock() noexcept { m_.lock(); }
    void unlock() noexcept { m_.unlock(); }
private:
    std::mutex m_;
};
#endif

class LockGuard {
public:
    explicit LockGuard(PoolLock& l) noexcept : l_(l) { l_.lock(); }
    ~LockGuard() { l_.unlock(); }
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
private:
    PoolLock& l_;
};

void updateMax(std::atomic<std::size_t>& target, std::size_t value) noexcept {
    std::size_t seen = target.load(std::memory_order_relaxed);
    while (value > seen && !target.compare_exchange_weak(seen, value, std::memory_order_relaxed)) {}
}

// ---------------------------------------------------------------------------
// TLSF-backed pool (PSRAM on the device, simulated arenas on the host).
class TlsfPool {
public:
    bool init(void* base, std::size_t bytes) noexcept {
        lock_.init();
        LockGuard g(lock_);
        return tlsf_.init(base, bytes);
    }
    void reset() noexcept { LockGuard g(lock_); tlsf_.reset(); }
    bool ready() const noexcept { return tlsf_.ready(); }
    bool owns(const void* p) const noexcept { return tlsf_.owns(p); }
    void* allocate(std::size_t n, std::size_t a) noexcept { LockGuard g(lock_); return tlsf_.allocate(n, a); }
    bool deallocate(void* p) noexcept { LockGuard g(lock_); return tlsf_.deallocate(p); }
    detail::TlsfStats stats() noexcept { LockGuard g(lock_); return tlsf_.stats(); }
    bool check() noexcept { LockGuard g(lock_); return tlsf_.check(); }
private:
    PoolLock lock_{};
    detail::Tlsf tlsf_{};
};

// ---------------------------------------------------------------------------
// Standard heap (newlib malloc, mutex-protected by pico_malloc on device).
// Over-aligned requests (rare) over-allocate with malloc and remember the
// {aligned, raw} pair in a small lock-free registry, so mem::free() never has
// to read outside a block to recognise them. The registry is only scanned
// while at least one over-aligned block is alive.
class HeapPool {
public:
    static constexpr std::size_t kAlignedSlots = 64;

    void* allocate(std::size_t n, std::size_t align) noexcept {
        if (n == 0) n = 1;
        void* result = nullptr;
        void* raw = nullptr;
        if (align <= alignof(std::max_align_t)) {
            result = raw = std::malloc(n);
        } else if ((align & (align - 1)) == 0 && n <= static_cast<std::size_t>(-1) - align) {
            raw = std::malloc(n + align);
            if (raw) {
                const auto aligned = (reinterpret_cast<std::uintptr_t>(raw) + align - 1) & ~static_cast<std::uintptr_t>(align - 1);
                result = reinterpret_cast<void*>(aligned);
                if (result != raw && !remember(result, raw)) { // registry full: refuse cleanly
                    std::free(raw);
                    result = raw = nullptr;
                }
            }
        }
        if (!result) {
            failures_.fetch_add(1, std::memory_order_relaxed);
            return nullptr;
        }
        live_.fetch_add(1, std::memory_order_relaxed);
#if FXAI_ON_DEVICE
        updateMax(peakBreak_, reinterpret_cast<std::uintptr_t>(sbrk(0)));
#else
        const std::size_t usable = malloc_usable_size(raw);
        updateMax(peakTracked_, tracked_.fetch_add(usable, std::memory_order_relaxed) + usable);
#endif
        return result;
    }

    void deallocate(void* p) noexcept {
        if (!p) return;
        void* raw = forget(p);
#if !FXAI_ON_DEVICE
        tracked_.fetch_sub(malloc_usable_size(raw), std::memory_order_relaxed);
#endif
        std::free(raw);
        live_.fetch_sub(1, std::memory_order_relaxed);
    }

    // Cheap estimate of contiguous never-claimed heap (device), used to keep
    // the SRAM safety reserve without walking malloc's bins on every request.
    std::size_t tailBytes() const noexcept {
#if FXAI_ON_DEVICE
        const auto brk = reinterpret_cast<std::uintptr_t>(sbrk(0));
        const auto limit = reinterpret_cast<std::uintptr_t>(&__HeapLimit);
        return brk < limit ? static_cast<std::size_t>(limit - brk) : 0u;
#else
        return static_cast<std::size_t>(-1) / 4;
#endif
    }

    PoolStats stats() noexcept {
        PoolStats s;
        s.name = "SRAM";
        s.available = true;
        s.failures = failures_.load(std::memory_order_relaxed);
        s.allocations = live_.load(std::memory_order_relaxed);
#if FXAI_ON_DEVICE
        const auto start = reinterpret_cast<std::uintptr_t>(&__end__);
        const auto limit = reinterpret_cast<std::uintptr_t>(&__HeapLimit);
        const struct mallinfo mi = mallinfo();
        s.total = static_cast<std::size_t>(limit - start);
        s.used = static_cast<std::size_t>(mi.uordblks);
        s.free = s.total > s.used ? s.total - s.used : 0;
        // The top chunk (keepcost) is contiguous with the never-claimed tail.
        std::size_t largest = tailBytes() + static_cast<std::size_t>(mi.keepcost);
        largest = largest > 64 ? largest - 64 : 0; // chunk header/alignment margin
        s.largestFree = largest < s.free ? largest : s.free;
        updateMax(peakBreak_, reinterpret_cast<std::uintptr_t>(sbrk(0)));
        const std::size_t peakBreak = peakBreak_.load(std::memory_order_relaxed);
        s.peakUsed = peakBreak > start ? static_cast<std::size_t>(peakBreak - start) : s.used;
        if (s.peakUsed < s.used) s.peakUsed = s.used;
#else
        s.total = 0; // host heap: unbounded
        s.used = tracked_.load(std::memory_order_relaxed);
        s.free = static_cast<std::size_t>(-1) / 4;
        s.largestFree = s.free;
        s.peakUsed = peakTracked_.load(std::memory_order_relaxed);
#endif
        return s;
    }

private:
    bool remember(void* aligned, void* raw) noexcept {
        for (std::size_t i = 0; i < kAlignedSlots; ++i) {
            void* expected = nullptr;
            if (alignedPtr_[i].compare_exchange_strong(expected, aligned, std::memory_order_acq_rel)) {
                rawPtr_[i].store(raw, std::memory_order_release);
                alignedLive_.fetch_add(1, std::memory_order_acq_rel);
                return true;
            }
        }
        return false;
    }
    void* forget(void* p) noexcept {
        if (alignedLive_.load(std::memory_order_acquire) == 0) return p;
        for (std::size_t i = 0; i < kAlignedSlots; ++i) {
            if (alignedPtr_[i].load(std::memory_order_acquire) == p) {
                void* raw = rawPtr_[i].load(std::memory_order_acquire);
                rawPtr_[i].store(nullptr, std::memory_order_relaxed);
                alignedPtr_[i].store(nullptr, std::memory_order_release);
                alignedLive_.fetch_sub(1, std::memory_order_acq_rel);
                return raw;
            }
        }
        return p;
    }

    std::atomic<void*> alignedPtr_[kAlignedSlots]{};
    std::atomic<void*> rawPtr_[kAlignedSlots]{};
    std::atomic<std::size_t> alignedLive_{0};
    std::atomic<std::size_t> live_{0};
    std::atomic<std::size_t> failures_{0};
    std::atomic<std::size_t> tracked_{0};
    std::atomic<std::size_t> peakTracked_{0};
    std::atomic<std::size_t> peakBreak_{0};
};

// A device build for a board without PSRAM (Pico 2 W) has no PSRAM pool at
// all: the TLSF control block (3.6 KB) would only waste SRAM there.
#if FXAI_ON_DEVICE && !FXAI_HAS_PSRAM
struct NoPool {
    bool init(void*, std::size_t) noexcept { return false; }
    void reset() noexcept {}
    bool ready() const noexcept { return false; }
    bool owns(const void*) const noexcept { return false; }
    void* allocate(std::size_t, std::size_t) noexcept { return nullptr; }
    bool deallocate(void*) noexcept { return false; }
    detail::TlsfStats stats() noexcept { return {}; }
    bool check() noexcept { return true; }
};
using PsramPool = NoPool;
#else
using PsramPool = TlsfPool;
#endif

// ---------------------------------------------------------------------------
struct Registry {
    HeapPool heap{};
#if !FXAI_ON_DEVICE
    TlsfPool sramSim{}; // host simulation of a bounded SRAM heap (tests only)
#endif
    std::atomic<bool> sramSimulated{false};
    PsramPool psram{};
    std::atomic<bool> psramReady{false};
    std::uintptr_t psramBase{0};
    std::size_t psramBytes{0};
    std::atomic<std::size_t> outOfMemory{0};
    std::atomic<std::size_t> invalidFrees{0};
    void* hostSramArena{nullptr};
    void* hostPsramArena{nullptr};
};
constinit Registry g_reg{};

constexpr std::uint8_t kNoScope = 0xFF;
#if FXAI_ON_DEVICE
std::uint8_t g_scope[2] = {kNoScope, kNoScope};
inline std::uint8_t& scopeSlot() noexcept { return g_scope[get_core_num() & 1u]; }
inline bool inInterrupt() noexcept { return __get_current_exception() != 0; }
#else
thread_local std::uint8_t g_scope = kNoScope;
inline std::uint8_t& scopeSlot() noexcept { return g_scope; }
inline bool inInterrupt() noexcept { return false; }
#endif

bool psramUsable() noexcept { return g_reg.psramReady.load(std::memory_order_acquire); }

// Translate the uncached PSRAM alias (0x15xxxxxx) to the cached address the
// allocator manages (0x11xxxxxx).
const void* canonical(const void* p) noexcept {
#if FXAI_ON_DEVICE
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    constexpr auto delta = board::config::kPsramUncachedBase - board::config::kPsramCachedBase;
    if (a >= board::config::kPsramUncachedBase && a < board::config::kPsramUncachedBase + board::config::kPsramWindowBytes)
        return reinterpret_cast<const void*>(a - delta);
#endif
    return p;
}

#if !FXAI_ON_DEVICE
TlsfPool* simulatedSram() noexcept {
    return g_reg.sramSimulated.load(std::memory_order_acquire) ? &g_reg.sramSim : nullptr;
}
#else
constexpr TlsfPool* simulatedSram() noexcept { return nullptr; }
#endif

void* sramAllocate(std::size_t n, std::size_t align) noexcept {
    if (TlsfPool* sim = simulatedSram()) return sim->allocate(n, align);
    return g_reg.heap.allocate(n, align);
}

void* psramAllocate(std::size_t n, std::size_t align) noexcept {
    if (!psramUsable()) return nullptr;
    return g_reg.psram.allocate(n, align);
}

bool sramIsLow(std::size_t n) noexcept {
    if (TlsfPool* sim = simulatedSram()) return sim->stats().largestFree < n + kSramReserveBytes;
    return g_reg.heap.tailBytes() < n + kSramReserveBytes;
}

PoolStats tlsfToPoolStats(const char* name, const detail::TlsfStats& t) noexcept {
    PoolStats s;
    s.name = name;
    s.available = true;
    s.total = t.totalBytes;
    s.used = t.usedBytes;
    s.free = t.freeBytes;
    s.largestFree = t.largestFree;
    s.peakUsed = t.peakUsedBytes;
    s.failures = t.failures;
    s.allocations = t.liveAllocations;
    return s;
}

// ---------------------------------------------------------------------------
class PoolResource final : public std::pmr::memory_resource {
public:
    constexpr PoolResource(bool usePolicy, std::uint8_t which) noexcept : usePolicy_(usePolicy), which_(which) {}
private:
    void* do_allocate(std::size_t bytes, std::size_t align) override {
        void* p = usePolicy_ ? tryAlloc(static_cast<Policy>(which_), bytes, align)
                             : tryAlloc(static_cast<Pool>(which_), bytes, align);
        if (!p) throw std::bad_alloc();
        return p;
    }
    void do_deallocate(void* p, std::size_t, std::size_t) noexcept override { mem::free(p); }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
    bool usePolicy_;
    std::uint8_t which_;
};

PoolResource g_poolResources[kPoolCount] = {
    PoolResource(false, 0), PoolResource(false, 1), PoolResource(false, 2)};
PoolResource g_policyResources[4] = {
    PoolResource(true, 0), PoolResource(true, 1), PoolResource(true, 2), PoolResource(true, 3)};

} // namespace

// ---------------------------------------------------------------------------
void* tryAlloc(Pool pool, std::size_t size, std::size_t align) noexcept {
    if (align == 0 || (align & (align - 1)) != 0) return nullptr;
    switch (pool) {
    case Pool::SRAM: return sramAllocate(size, align);
    case Pool::PSRAM_BOARD: return psramAllocate(size, align);
    case Pool::PSRAM_PICOCALC: return nullptr; // no verified driver yet
    }
    return nullptr;
}

void* tryAlloc(Policy policy, std::size_t size, std::size_t align) noexcept {
    if (align == 0 || (align & (align - 1)) != 0) return nullptr;
    void* p = nullptr;
    switch (policy) {
    case Policy::SRAM_ONLY:
        p = sramAllocate(size, align);
        break;
    case Policy::PSRAM_ONLY:
        p = psramAllocate(size, align);
        break;
    case Policy::PREFER_PSRAM:
        p = psramAllocate(size, align);
        if (!p) p = sramAllocate(size, align);
        break;
    case Policy::PREFER_SRAM:
        if (psramUsable() && sramIsLow(size)) {
            p = psramAllocate(size, align);
            if (!p) p = sramAllocate(size, align);
        } else {
            p = sramAllocate(size, align);
            if (!p) p = psramAllocate(size, align);
        }
        break;
    }
    if (!p) g_reg.outOfMemory.fetch_add(1, std::memory_order_relaxed);
    return p;
}

void* alloc(Pool pool, std::size_t size, std::size_t align) {
    void* p = tryAlloc(pool, size, align);
    if (!p) throw std::bad_alloc();
    return p;
}

void* alloc(Policy policy, std::size_t size, std::size_t align) {
    void* p = tryAlloc(policy, size, align);
    if (!p) throw std::bad_alloc();
    return p;
}

Pool poolOf(const void* p) noexcept {
    const void* c = canonical(p);
    if (psramUsable() && g_reg.psram.owns(c)) return Pool::PSRAM_BOARD;
    return Pool::SRAM;
}

void free(void* p) noexcept {
    if (!p) return;
    const void* c = canonical(p);
    if (psramUsable() && g_reg.psram.owns(c)) {
        g_reg.psram.deallocate(const_cast<void*>(c));
        return;
    }
    if (TlsfPool* sim = simulatedSram(); sim && sim->owns(p)) {
        sim->deallocate(p);
        return;
    }
#if FXAI_ON_DEVICE
    // Never hand flash/ROM/peripheral addresses to free(): count and ignore.
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    if (a < board::config::kSramBase || a >= board::config::kSramBase + board::config::kSramBytes) {
        g_reg.invalidFrees.fetch_add(1, std::memory_order_relaxed);
        return;
    }
#endif
    g_reg.heap.deallocate(p);
}

PoolStats stats(Pool pool) noexcept {
    switch (pool) {
    case Pool::SRAM:
        if (TlsfPool* sim = simulatedSram()) return tlsfToPoolStats("SRAM", sim->stats());
        return g_reg.heap.stats();
    case Pool::PSRAM_BOARD:
        if (psramUsable()) return tlsfToPoolStats("PSRAM", g_reg.psram.stats());
        break;
    case Pool::PSRAM_PICOCALC:
        break;
    }
    PoolStats s;
    s.name = name(pool);
    return s;
}

bool available(Pool pool) noexcept {
    switch (pool) {
    case Pool::SRAM: return true;
    case Pool::PSRAM_BOARD: return psramUsable();
    case Pool::PSRAM_PICOCALC: return false;
    }
    return false;
}

const char* name(Pool pool) noexcept {
    switch (pool) {
    case Pool::SRAM: return "SRAM";
    case Pool::PSRAM_BOARD: return "PSRAM";
    case Pool::PSRAM_PICOCALC: return "PSRAM PicoCalc";
    }
    return "?";
}

std::size_t largestFree(Policy policy) noexcept {
    const std::size_t sram = stats(Pool::SRAM).largestFree;
    const std::size_t sramUsable = sram > kSramReserveBytes ? sram - kSramReserveBytes : 0;
    const std::size_t psram = available(Pool::PSRAM_BOARD) ? stats(Pool::PSRAM_BOARD).largestFree : 0;
    switch (policy) {
    case Policy::SRAM_ONLY: return sram;
    case Policy::PSRAM_ONLY: return psram;
    case Policy::PREFER_PSRAM:
    case Policy::PREFER_SRAM: return psram > sramUsable ? psram : sramUsable;
    }
    return 0;
}

bool canAllocate(std::size_t bytes, Policy policy, std::size_t reserve) noexcept {
    const std::size_t largest = largestFree(policy);
    return largest >= reserve && bytes <= largest - reserve;
}

std::pmr::memory_resource* resource(Pool pool) noexcept {
    return &g_poolResources[static_cast<std::size_t>(pool) % kPoolCount];
}

std::pmr::memory_resource* resource(Policy policy) noexcept {
    return &g_policyResources[static_cast<std::size_t>(policy) % 4u];
}

PlacementScope::PlacementScope(Policy policy) noexcept : previous_(scopeSlot()) {
    scopeSlot() = static_cast<std::uint8_t>(policy);
}

PlacementScope::~PlacementScope() { scopeSlot() = previous_; }

std::size_t newThreshold() noexcept { return FXAI_PSRAM_NEW_THRESHOLD; }

Policy newPolicyFor(std::size_t size) noexcept {
    if (inInterrupt()) return Policy::SRAM_ONLY;
    const std::uint8_t scoped = scopeSlot();
    if (scoped != kNoScope) return static_cast<Policy>(scoped);
    return size >= newThreshold() ? Policy::PREFER_PSRAM : Policy::PREFER_SRAM;
}

void init() noexcept {
    // The SRAM heap needs no setup; touching the statistics records the
    // initial heap break for the peak computation.
    (void)stats(Pool::SRAM);
}

bool registerPsramBoard(void* cachedBase, std::size_t bytes) noexcept {
    if (!cachedBase || bytes == 0) return false;
    if (!g_reg.psram.init(cachedBase, bytes)) return false;
    g_reg.psramBase = reinterpret_cast<std::uintptr_t>(cachedBase);
    g_reg.psramBytes = bytes;
    g_reg.psramReady.store(true, std::memory_order_release);
    return true;
}

bool checkIntegrity() noexcept {
    bool ok = true;
    if (psramUsable()) ok = g_reg.psram.check() && ok;
    if (TlsfPool* sim = simulatedSram()) ok = sim->check() && ok;
    return ok;
}

std::size_t outOfMemoryEvents() noexcept { return g_reg.outOfMemory.load(std::memory_order_relaxed); }
std::size_t invalidFrees() noexcept { return g_reg.invalidFrees.load(std::memory_order_relaxed); }

namespace testing {
void simulateSram(std::size_t bytes) {
#if !FXAI_ON_DEVICE
    g_reg.sramSimulated.store(false, std::memory_order_release);
    g_reg.sramSim.reset();
    std::free(g_reg.hostSramArena);
    g_reg.hostSramArena = nullptr;
    if (bytes) {
        g_reg.hostSramArena = std::malloc(bytes);
        if (g_reg.hostSramArena && g_reg.sramSim.init(g_reg.hostSramArena, bytes))
            g_reg.sramSimulated.store(true, std::memory_order_release);
    }
#else
    (void)bytes;
#endif
}

void simulatePsram(std::size_t bytes) {
#if !FXAI_ON_DEVICE
    g_reg.psramReady.store(false, std::memory_order_release);
    g_reg.psram.reset();
    std::free(g_reg.hostPsramArena);
    g_reg.hostPsramArena = nullptr;
    g_reg.psramBase = 0;
    g_reg.psramBytes = 0;
    if (bytes) {
        g_reg.hostPsramArena = std::malloc(bytes);
        if (g_reg.hostPsramArena) registerPsramBoard(g_reg.hostPsramArena, bytes);
    }
#else
    (void)bytes;
#endif
}

void reset() {
    simulateSram(0);
    simulatePsram(0);
}
} // namespace testing

} // namespace mem
