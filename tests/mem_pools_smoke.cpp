// Host tests for the TLSF allocator and the multi-pool registry (simulated
// SRAM/PSRAM arenas): stress, fragmentation, alignment, controlled failure,
// pmr adapters, policies, placement scopes and dual-thread safety.
#include "mem_pools.hpp"
#include "tlsf.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <cstring>
#include <memory_resource>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace {
using mem::detail::Tlsf;

struct Live { unsigned char* p; std::size_t n; unsigned char tag; };

void fill(const Live& l) { std::memset(l.p, l.tag, l.n); }
bool intact(const Live& l) {
    for (std::size_t i = 0; i < l.n; ++i) if (l.p[i] != l.tag) return false;
    return true;
}

void tlsfBasics() {
    std::vector<unsigned char> arena(1u << 20);
    Tlsf t;
    FX_CHECK(!t.init(arena.data(), 8));          // too small
    FX_CHECK(t.init(arena.data() + 3, arena.size() - 3)); // unaligned base is fixed up
    FX_CHECK(t.check());
    const auto empty = t.stats();
    FX_CHECK(empty.usedBytes == 0 && empty.freeBlocks == 1);
    FX_CHECK(empty.largestFree > (1u << 20) - 64 * 1024); // guaranteed size loses < 1/32

    void* a = t.allocate(100);
    void* b = t.allocate(1);
    void* c = t.allocate(0);
    FX_CHECK(a && b && c && a != b && b != c);
    FX_CHECK(reinterpret_cast<std::uintptr_t>(a) % Tlsf::kAlign == 0);
    FX_CHECK(t.usableSize(a) >= 100);
    FX_CHECK(t.owns(a) && !t.owns(arena.data() + arena.size() + 64));
    FX_CHECK(t.check());
    FX_CHECK(t.deallocate(b));
    FX_CHECK(!t.deallocate(b));            // double free is detected and ignored
    int foreign = 0;
    FX_CHECK(!t.deallocate(&foreign));     // pointer outside the arena is ignored
    FX_CHECK(t.stats().invalidFrees == 2);
    FX_CHECK(t.deallocate(a) && t.deallocate(c));
    FX_CHECK(t.check());
    FX_CHECK(t.stats().usedBytes == 0 && t.stats().freeBlocks == 1); // fully coalesced
}

void tlsfAlignment() {
    std::vector<unsigned char> arena(4u << 20);
    Tlsf t;
    FX_CHECK(t.init(arena.data(), arena.size()));
    std::vector<void*> blocks;
    for (std::size_t align = 8; align <= 65536; align <<= 1) {
        for (std::size_t size : {1u, 24u, 200u, 5000u}) {
            void* p = t.allocate(size, align);
            FX_CHECK(p != nullptr);
            FX_CHECK(reinterpret_cast<std::uintptr_t>(p) % align == 0);
            std::memset(p, 0x5A, size);
            blocks.push_back(p);
        }
        FX_CHECK(t.check());
    }
    FX_CHECK(t.allocate(16, 24) == nullptr); // non power-of-two alignment rejected
    for (void* p : blocks) FX_CHECK(t.deallocate(p));
    FX_CHECK(t.check() && t.stats().usedBytes == 0 && t.stats().freeBlocks == 1);
}

void tlsfStress() {
    std::vector<unsigned char> arena(2u << 20);
    Tlsf t;
    FX_CHECK(t.init(arena.data(), arena.size()));
    fxtest::Rng rng;
    std::vector<Live> live;
    std::size_t failures = 0;
    for (int op = 0; op < 200000; ++op) {
        const bool doAlloc = live.empty() || rng.below(100) < 55;
        if (doAlloc) {
            std::size_t n = 1 + rng.below(rng.below(10) == 0 ? 65536 : 512);
            const std::size_t align = std::size_t{8} << rng.below(rng.below(8) == 0 ? 8 : 1);
            auto* p = static_cast<unsigned char*>(t.allocate(n, align));
            if (!p) { ++failures; continue; }
            FX_CHECK(reinterpret_cast<std::uintptr_t>(p) % align == 0);
            Live l{p, n, static_cast<unsigned char>(rng.next())};
            fill(l);
            live.push_back(l);
        } else {
            const std::size_t i = rng.below(static_cast<unsigned>(live.size()));
            FX_CHECK(intact(live[i]));           // no other block overlapped this one
            FX_CHECK(t.deallocate(live[i].p));
            live[i] = live.back();
            live.pop_back();
        }
        if (op % 20000 == 0) FX_CHECK(t.check());
    }
    for (const auto& l : live) { FX_CHECK(intact(l)); FX_CHECK(t.deallocate(l.p)); }
    FX_CHECK(t.check());
    const auto s = t.stats();
    FX_CHECK(s.usedBytes == 0 && s.freeBlocks == 1 && s.liveAllocations == 0);
    FX_CHECK(s.failures == failures);
    FX_CHECK(s.peakUsedBytes > 0 && s.peakUsedBytes <= s.totalBytes);
}

void tlsfExhaustionAndFragmentation() {
    std::vector<unsigned char> arena(256u * 1024u);
    Tlsf t;
    FX_CHECK(t.init(arena.data(), arena.size()));
    std::vector<void*> blocks;
    while (void* p = t.allocate(1000)) blocks.push_back(p);
    FX_CHECK(blocks.size() > 200);
    FX_CHECK(t.stats().failures == 1);       // the exhausted request failed cleanly
    FX_CHECK(t.allocate(1u << 30) == nullptr); // absurd request fails without crashing
    // Free every other block: lots of free memory, all of it fragmented.
    for (std::size_t i = 0; i < blocks.size(); i += 2) FX_CHECK(t.deallocate(blocks[i]));
    const auto frag = t.stats();
    FX_CHECK(frag.freeBytes > 100u * 1024u);
    FX_CHECK(frag.largestFree <= 2100);      // honest largest-free (last hole merges with the tail)
    FX_CHECK(t.allocate(4000) == nullptr);   // cannot be satisfied: no contiguous hole
    FX_CHECK(t.check());
    for (std::size_t i = 1; i < blocks.size(); i += 2) FX_CHECK(t.deallocate(blocks[i]));
    const auto healed = t.stats();
    FX_CHECK(healed.freeBlocks == 1 && healed.largestFree > 200u * 1024u); // coalescing restored it
    FX_CHECK(t.allocate(200u * 1024u) != nullptr);
    FX_CHECK(t.check());
}

void registryPlusProfile() {
    mem::testing::simulateSram(256u * 1024u);
    mem::testing::simulatePsram(8u << 20);
    FX_CHECK(mem::available(mem::Pool::SRAM) && mem::available(mem::Pool::PSRAM_BOARD));
    FX_CHECK(!mem::available(mem::Pool::PSRAM_PICOCALC));
    FX_CHECK(mem::tryAlloc(mem::Pool::PSRAM_PICOCALC, 16) == nullptr); // stub pool

    void* big = mem::alloc(mem::Policy::PREFER_PSRAM, 1u << 20);
    FX_CHECK(mem::poolOf(big) == mem::Pool::PSRAM_BOARD);
    void* small = mem::alloc(mem::Policy::PREFER_SRAM, 64);
    FX_CHECK(mem::poolOf(small) == mem::Pool::SRAM);
    void* sramOnly = mem::alloc(mem::Pool::SRAM, 1000, 64);
    FX_CHECK(mem::poolOf(sramOnly) == mem::Pool::SRAM && reinterpret_cast<std::uintptr_t>(sramOnly) % 64 == 0);
    FX_CHECK(mem::tryAlloc(mem::Policy::SRAM_ONLY, 300u * 1024u) == nullptr); // bigger than SRAM
    void* fallback = mem::tryAlloc(mem::Policy::PREFER_SRAM, 300u * 1024u); // falls back to PSRAM
    FX_CHECK(fallback && mem::poolOf(fallback) == mem::Pool::PSRAM_BOARD);

    const auto ps = mem::stats(mem::Pool::PSRAM_BOARD);
    FX_CHECK(ps.available && ps.total > (8u << 20) - 4096 && ps.used >= (1u << 20) + 300u * 1024u);
    FX_CHECK(ps.allocations == 2 && ps.peakUsed >= ps.used);
    FX_CHECK(mem::largestFree(mem::Policy::PSRAM_ONLY) == ps.largestFree);
    FX_CHECK(mem::canAllocate(4u << 20, mem::Policy::PREFER_PSRAM));
    FX_CHECK(!mem::canAllocate(16u << 20, mem::Policy::PREFER_PSRAM));

    bool threw = false;
    try { (void)mem::alloc(mem::Policy::PSRAM_ONLY, 64u << 20); } catch (const std::bad_alloc&) { threw = true; }
    FX_CHECK(threw);
    FX_CHECK(mem::outOfMemoryEvents() >= 1);
    FX_CHECK(mem::stats(mem::Pool::PSRAM_BOARD).failures >= 1);

    // Exhaust PSRAM: PREFER_PSRAM keeps working from SRAM, PSRAM_ONLY fails.
    std::vector<void*> hog;
    for (std::size_t chunk : {256u * 1024u, 16u * 1024u, 1024u, 64u})
        while (void* p = mem::tryAlloc(mem::Pool::PSRAM_BOARD, chunk)) hog.push_back(p);
    void* spill = mem::tryAlloc(mem::Policy::PREFER_PSRAM, 32u * 1024u);
    FX_CHECK(spill && mem::poolOf(spill) == mem::Pool::SRAM);
    FX_CHECK(mem::tryAlloc(mem::Policy::PSRAM_ONLY, 300u * 1024u) == nullptr);
    for (void* p : hog) mem::free(p);
    mem::free(spill);

    // pmr containers draw from the requested pool and report failure as bad_alloc.
    {
        std::pmr::vector<double> samples(mem::resource(mem::Pool::PSRAM_BOARD));
        samples.resize(100000, 1.5);
        FX_CHECK(mem::poolOf(samples.data()) == mem::Pool::PSRAM_BOARD);
        std::pmr::string text("pmr string placed in SRAM, long enough to leave SSO storage", mem::resource(mem::Pool::SRAM));
        FX_CHECK(mem::poolOf(text.data()) == mem::Pool::SRAM);
        std::pmr::vector<int> limited(mem::resource(mem::Pool::SRAM));
        limited.push_back(7);
        bool caught = false;
        try { limited.resize(1u << 20); } catch (const std::bad_alloc&) { caught = true; }
        FX_CHECK(caught && limited.size() == 1 && limited[0] == 7); // strong guarantee kept
        std::pmr::vector<int> viaPolicy(mem::resource(mem::Policy::PREFER_PSRAM));
        viaPolicy.assign(5000, 3);
        FX_CHECK(mem::poolOf(viaPolicy.data()) == mem::Pool::PSRAM_BOARD);
    }

    mem::free(big); mem::free(small); mem::free(sramOnly); mem::free(fallback);
    mem::free(nullptr);
    FX_CHECK(mem::checkIntegrity());
    FX_CHECK(mem::stats(mem::Pool::PSRAM_BOARD).allocations == 0);
    FX_CHECK(mem::stats(mem::Pool::SRAM).allocations == 0);
}

void registryPico2wProfile() {
    mem::testing::simulateSram(128u * 1024u);
    mem::testing::simulatePsram(0);
    FX_CHECK(!mem::available(mem::Pool::PSRAM_BOARD));
    FX_CHECK(!mem::stats(mem::Pool::PSRAM_BOARD).available);
    void* p = mem::alloc(mem::Policy::PREFER_PSRAM, 4096); // same API, lands in SRAM
    FX_CHECK(mem::poolOf(p) == mem::Pool::SRAM);
    FX_CHECK(mem::tryAlloc(mem::Policy::PSRAM_ONLY, 16) == nullptr);
    FX_CHECK(mem::largestFree(mem::Policy::PSRAM_ONLY) == 0);
    FX_CHECK(mem::newPolicyFor(1u << 20) == mem::Policy::PREFER_PSRAM); // policy is board-agnostic
    mem::free(p);
    FX_CHECK(mem::tryAlloc(mem::Policy::PREFER_SRAM, 1u << 20) == nullptr); // controlled failure
    FX_CHECK(mem::checkIntegrity());
}

void placementScopes() {
    FX_CHECK(mem::newPolicyFor(16) == mem::Policy::PREFER_SRAM);
    FX_CHECK(mem::newPolicyFor(mem::newThreshold()) == mem::Policy::PREFER_PSRAM);
    {
        mem::PlacementScope outer(mem::Policy::PSRAM_ONLY);
        FX_CHECK(mem::newPolicyFor(16) == mem::Policy::PSRAM_ONLY);
        {
            mem::PlacementScope inner(mem::Policy::SRAM_ONLY);
            FX_CHECK(mem::newPolicyFor(1u << 20) == mem::Policy::SRAM_ONLY);
        }
        FX_CHECK(mem::newPolicyFor(16) == mem::Policy::PSRAM_ONLY);
    }
    FX_CHECK(mem::newPolicyFor(16) == mem::Policy::PREFER_SRAM);
}

void dualThreadStress() {
    mem::testing::simulateSram(512u * 1024u);
    mem::testing::simulatePsram(4u << 20);
    auto worker = [](unsigned seed) {
        fxtest::Rng rng;
        rng.s ^= seed * 0x100000001B3ull;
        std::vector<Live> live;
        for (int op = 0; op < 40000; ++op) {
            if (live.empty() || rng.below(100) < 52) {
                const std::size_t n = 1 + rng.below(4096);
                const auto policy = static_cast<mem::Policy>(rng.below(4));
                auto* p = static_cast<unsigned char*>(mem::tryAlloc(policy, n));
                if (!p) continue;
                Live l{p, n, static_cast<unsigned char>(seed + op)};
                fill(l);
                live.push_back(l);
            } else {
                const std::size_t i = rng.below(static_cast<unsigned>(live.size()));
                FX_CHECK(intact(live[i]));
                mem::free(live[i].p);
                live[i] = live.back();
                live.pop_back();
            }
        }
        for (const auto& l : live) { FX_CHECK(intact(l)); mem::free(l.p); }
    };
    std::thread a(worker, 1u), b(worker, 2u);
    a.join();
    b.join();
    FX_CHECK(mem::checkIntegrity());
    FX_CHECK(mem::stats(mem::Pool::PSRAM_BOARD).allocations == 0);
    FX_CHECK(mem::stats(mem::Pool::SRAM).allocations == 0);
}
} // namespace

int main() {
    tlsfBasics();
    tlsfAlignment();
    tlsfStress();
    tlsfExhaustionAndFragmentation();
    placementScopes();
    registryPlusProfile();
    registryPico2wProfile();
    dualThreadStress();
    mem::testing::reset();
    std::puts("Memory pools (TLSF, policies, pmr, threads): PASS");
    return 0;
}
