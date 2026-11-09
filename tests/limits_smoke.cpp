// Board profiles and memory-derived limits (limits.hpp): persistent limits per
// board, RAM limits chosen from the pools that exist at run time (a Pico Plus
// 2 W whose PSRAM is disabled falls back to the SRAM values), structural
// limits identical on both boards.
#include "board_config.hpp"
#include "expression.hpp"
#include "graph_engine.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"
#include "natural_editor.hpp"
#include "pict.hpp"
#include "test_util.hpp"
#include "workspace.hpp"

#include <vector>

int main() {
    using namespace fx::limits;
    namespace cfg = board::config;
    // ---- persistent limits
    CHECK(fx::Workspace::MaxHistory == kHistoryEntries && fx::Workspace::MaxListLength == kListMaxLength);
    if (cfg::kIsPicoPlus2W) {
        CHECK(kHistoryEntries == 200 && kListMaxLength == 999);       // fx-CG50 lists, larger history
        CHECK(cfg::kFlashRecordSize == 1024u * 1024u);
    } else {
        CHECK(kHistoryEntries == 40 && kListMaxLength == 255);        // v1.8.0 values, unchanged record
        CHECK(cfg::kFlashRecordSize == 128u * 1024u);
        CHECK(cfg::kFlashRecordOffset == cfg::kFlashSize - 128u * 1024u);   // v1.8.0 location
    }
    CHECK(kChatMessages == 30);
    // ---- RAM profiles
    CHECK(kPsramProfile.graphTotalCurvePoints > kSramProfile.graphTotalCurvePoints);
    CHECK(kPsramProfile.graphCurvePoints > kSramProfile.graphCurvePoints);
    CHECK(kPsramProfile.pictEncodedBytes > kSramProfile.pictEncodedBytes);
    CHECK(kPsramProfile.graph3dMaxGrid > kSramProfile.graph3dMaxGrid);
    CHECK(kPsramProfile.graph3dZBuffer && !kSramProfile.graph3dZBuffer);
    // v1.8.0 values are the SRAM profile
    CHECK(kSramProfile.graphCurvePoints == 6000 && kSramProfile.graphTotalCurvePoints == 24000);
    CHECK(kSramProfile.pictEncodedBytes == 96u * 1024u);
    // ---- structural limits do not depend on the board
    CHECK(fx::MaxExpressionTokens == 320 && fx::MaxExpressionNesting == 32);
    CHECK(fx::NaturalEditor::MaxTemplateDepth == 12);

    // Without a PSRAM pool (Pico 2 W, or PSRAM disabled on the Pico Plus 2 W)
    // the RAM limits are the SRAM ones.
    mem::testing::simulateSram(192u * 1024u);
    mem::testing::simulatePsram(0);
    CHECK(&ram() == &kSramProfile);
    CHECK(fx::graph::maxCurvePoints() == 6000 && fx::graph::maxTotalCurvePoints() == 24000);
    CHECK(fx::pict::maxEncodedBytes() == 96u * 1024u);
    CHECK(!graph3dZBuffer(312u * 236u));
    CHECK(graph3dGrid(312u * 236u) <= kSramProfile.graph3dMaxGrid && graph3dGrid(312u * 236u) >= 4);
    const std::size_t sramBudget = heavyBudget();
    CHECK(sramBudget > 0 && sramBudget < 192u * 1024u);

    // With 8 MB of PSRAM the heavy data is served by the PSRAM pool.
    mem::testing::simulatePsram(8u << 20);
    CHECK(&ram() == &kPsramProfile);
    CHECK(fx::graph::maxTotalCurvePoints() == kPsramProfile.graphTotalCurvePoints);
    CHECK(fx::pict::maxEncodedBytes() == 1024u * 1024u);
    CHECK(graph3dZBuffer(312u * 236u));
    CHECK(graph3dGrid(312u * 236u) == kPsramProfile.graph3dMaxGrid);
    const std::size_t budget = heavyBudget();
    CHECK(budget > 7u * 1024u * 1024u);
    const std::size_t dim = matrixMaxDim(), list = listMaxLength(), hist = historyCapacity();
    CHECK(dim >= 1 && dim <= kMatrixMaxDim && list >= 1 && hist >= 1);
    if (cfg::kHasPsram) CHECK(dim > 300 && list > 100000 && hist == 10000);

    // Consuming memory lowers the derived limits; releasing it restores them.
    std::vector<void*> hog;
    hog.reserve(4096);
    for (std::size_t chunk : {64u * 1024u, 4096u, 512u})
        while (hog.size() < hog.capacity()) {
            void* p = mem::tryAlloc(mem::Policy::PREFER_PSRAM, chunk);
            if (!p) break;
            hog.push_back(p);
        }
    CHECK(heavyBudget() < budget / 16);
    CHECK(!fitsHeavy(256u * 1024u));
    CHECK(!graph3dZBuffer(312u * 236u));                 // no room for the depth buffer now
    CHECK(graph3dGrid(312u * 236u) < kPsramProfile.graph3dMaxGrid);
    CHECK(listMaxLength() < list && historyCapacity() <= hist);
    for (void* p : hog) mem::free(p);
    CHECK(heavyBudget() == budget && matrixMaxDim() == dim && listMaxLength() == list);
    CHECK(mem::checkIntegrity());
    mem::testing::reset();
    CHECK(&ram() == &kSramProfile);                      // pool gone: back to the SRAM profile
    return testutil::finish("limits_smoke");
}
