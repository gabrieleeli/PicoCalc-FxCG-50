// Host tests for the board profiles and the runtime (memory-derived) limits.
#include "board_config.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"
#include "test_support.hpp"

#include <vector>

int main() {
    using namespace fx::limits;
    // The Pico Plus 2 W profile is strictly larger where memory matters.
    FX_CHECK(kPicoPlus2WProfile.matrixMaxDim > kPico2WProfile.matrixMaxDim);
    FX_CHECK(kPicoPlus2WProfile.listMaxLength > kPico2WProfile.listMaxLength);
    FX_CHECK(kPicoPlus2WProfile.historyEntries > kPico2WProfile.historyEntries);
    FX_CHECK(kPicoPlus2WProfile.graphMaxFunctions >= kPico2WProfile.graphMaxFunctions);
    FX_CHECK(kPicoPlus2WProfile.graph3dMaxGrid > kPico2WProfile.graph3dMaxGrid);
    FX_CHECK(kPicoPlus2WProfile.graph3dZBuffer && !kPico2WProfile.graph3dZBuffer);
    FX_CHECK(kPicoPlus2WProfile.windowSnapshots > 0 && kPico2WProfile.windowSnapshots == 0);
    FX_CHECK(kPico2WProfile.historyEntries == 30); // fx-CG50 parity kept on the SRAM-only board
    FX_CHECK(&profile() == (board::config::kHasPsram ? &kPicoPlus2WProfile : &kPico2WProfile));
    // Structural limits are identical for both boards (SRAM stacks).
    FX_CHECK(kMaxParseDepth >= 16 && kMaxLayoutDepth >= 16 && kMaxEditorNesting * 2 + 1 <= kMaxLayoutDepth);

    // Runtime limits follow the free memory of the pools.
    mem::testing::simulateSram(192u * 1024u);
    mem::testing::simulatePsram(board::config::kHasPsram ? (8u << 20) : 0u);
    const std::size_t budget = heavyBudget();
    const std::size_t dim = matrixMaxDim();
    const std::size_t list = listMaxLength();
    const std::size_t hist = historyCapacity();
    FX_CHECK(budget > 0 && dim >= 1 && list >= 1 && hist >= 1);
    FX_CHECK(dim <= profile().matrixMaxDim && list <= profile().listMaxLength && hist <= profile().historyEntries);
    if (board::config::kHasPsram) {
        // With 8 MiB of PSRAM the caps are bound by memory, far above SRAM-era values.
        FX_CHECK(dim > 300 && dim < 999);       // 4 n^2 doubles must fit
        FX_CHECK(list > 100000);
        FX_CHECK(hist == profile().historyEntries);
        FX_CHECK(graph3dZBuffer(304u * 224u));
        FX_CHECK(graph3dGrid(304u * 224u) >= 64);
        FX_CHECK(windowSnapshots(320u * 320u * 2u) >= 1);
        FX_CHECK(aiResponseCapacity() == profile().aiResponseBytes);
    } else {
        FX_CHECK(dim <= 40 && list <= 999 && hist == 30);
        FX_CHECK(!graph3dZBuffer(304u * 224u));
        FX_CHECK(windowSnapshots(320u * 320u * 2u) == 0);
    }

    // Consuming memory lowers the derived limits; releasing it restores them.
    std::vector<void*> hog;
    const mem::Policy heavy = mem::Policy::PREFER_PSRAM;
    hog.reserve(4096);
    for (std::size_t chunk : {64u * 1024u, 4096u, 512u})
        while (hog.size() < hog.capacity()) {
            void* p = mem::tryAlloc(heavy, chunk);
            if (!p) break;
            hog.push_back(p);
        }
    const std::size_t dimLow = matrixMaxDim(), listLow = listMaxLength(), histLow = historyCapacity();
    FX_CHECK(dimLow < dim || dim == 1);
    FX_CHECK(listLow < list);
    FX_CHECK(histLow <= hist && histLow >= 1);
    FX_CHECK(!fitsHeavy(256u * 1024u));
    FX_CHECK(graphMaxFunctions() >= 1 && graphSamplesPerPixel(20, 297) >= 1);
    for (void* p : hog) mem::free(p);
    FX_CHECK(matrixMaxDim() == dim && listMaxLength() == list && historyCapacity() == hist);
    FX_CHECK(mem::checkIntegrity());
    mem::testing::reset();
    std::printf("Limits (%s profile): PASS\n", profile().name);
    return 0;
}
