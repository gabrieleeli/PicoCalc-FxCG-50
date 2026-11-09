// Host test for the global operator new routing (src/mem_new.cpp): standard
// containers land in PSRAM when they are large, in SRAM when small, follow a
// PlacementScope, and allocation failure surfaces as std::bad_alloc.
#include "mem_pools.hpp"
#include "test_support.hpp"

#include <memory>
#include <new>
#include <string>
#include <vector>

int main() {
    mem::testing::simulateSram(512u * 1024u);
    mem::testing::simulatePsram(8u << 20);
    {
        std::vector<double> matrixRow(1000, 2.0);      // 8000 bytes -> PSRAM
        FX_CHECK(mem::poolOf(matrixRow.data()) == mem::Pool::PSRAM_BOARD);
        auto node = std::make_shared<int>(42);          // tiny -> SRAM
        FX_CHECK(mem::poolOf(node.get()) == mem::Pool::SRAM);
        std::string history(300, 'x');                  // below threshold -> SRAM
        FX_CHECK(mem::poolOf(history.data()) == mem::Pool::SRAM);
        {
            mem::PlacementScope scope(mem::Policy::PSRAM_ONLY);
            std::string archived(64, 'y');              // small, but scoped to PSRAM
            FX_CHECK(mem::poolOf(archived.data()) == mem::Pool::PSRAM_BOARD);
        }
        struct alignas(64) Line { char bytes[64]; };
        auto* line = new Line{};
        FX_CHECK(reinterpret_cast<std::uintptr_t>(line) % 64 == 0);
        delete line;
        auto* raw = new (std::nothrow) char[16];
        FX_CHECK(raw != nullptr);
        delete[] raw;
    }
    FX_CHECK(mem::stats(mem::Pool::PSRAM_BOARD).allocations == 0);
    FX_CHECK(mem::checkIntegrity());

    // Pico 2 W profile: no PSRAM, small SRAM -> large requests fail with bad_alloc
    // and leave the program in a usable state.
    mem::testing::simulateSram(96u * 1024u);
    mem::testing::simulatePsram(0);
    {
        std::vector<double> ok(1000, 1.0);
        FX_CHECK(mem::poolOf(ok.data()) == mem::Pool::SRAM);
        bool threw = false;
        try {
            std::vector<double> tooBig(1u << 20);
            (void)tooBig;
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        FX_CHECK(threw);
        FX_CHECK(ok.size() == 1000 && ok[999] == 1.0);
        auto* nothrowBig = new (std::nothrow) char[1u << 20];
        FX_CHECK(nothrowBig == nullptr);
    }
    FX_CHECK(mem::stats(mem::Pool::SRAM).allocations == 0);
    mem::testing::reset();
    return fxtest::finish("mem_new_smoke");
}
