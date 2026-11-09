#pragma once
// Capacity limits of the firmware: per-board profile + memory-derived limits.
//
// Three kinds of limits are kept deliberately separate:
//  * persistent limits (data stored in the workspace record on the internal
//    flash): fixed per board build, because a record saved by a firmware must
//    always be readable by the same firmware. The Pico Plus 2 W has a 1 MB
//    record store (2 x 512 KB slots) and 8 MB of PSRAM, so its lists reach the
//    fx-CG50 size (999 elements) and the Run-Matrix history keeps more lines;
//    the Pico 2 W keeps the v1.8.0 values (its record store is 2 x 64 KB);
//  * RAM limits (buffers that are never saved: GRAPH curve points, pictures,
//    3D meshes): chosen at run time from the pools that actually exist, so a
//    Pico Plus 2 W whose PSRAM failed its self-test falls back to the Pico 2 W
//    values instead of running out of SRAM;
//  * structural limits (recursion depth, CPU time): they protect the SRAM
//    stacks and are identical on both boards (see expression.hpp).
// Every large allocation still has to handle std::bad_alloc: a limit computed
// now can be invalidated by the next allocation.

#include "board_config.hpp"

#include <cstddef>
#include <cstdint>

namespace fx::limits {

// ---- Persistent limits (workspace record format v4, both boards) ---------------
inline constexpr std::size_t kHistoryEntries = board::config::kIsPicoPlus2W ? 200 : 40;   // Run-Matrix history
inline constexpr std::size_t kListMaxLength = board::config::kIsPicoPlus2W ? 999 : 255;   // statistics lists (fx-CG50: 999)
inline constexpr std::size_t kChatMessages = 30;   // AI conversation (the relay keeps the last 20 turns)

// ---- RAM limits -------------------------------------------------------------------
struct RamProfile {
    const char* name;
    std::size_t graphCurvePoints;       // points kept per plot (int16 x,y: 4 bytes each)
    std::size_t graphTotalCurvePoints;  // points of all the plots of one graph screen
    std::size_t pictEncodedBytes;       // RLE picture (PICT, background, BMP import) kept in RAM
    std::size_t graph3dMaxGrid;         // surface cells per side
    bool graph3dZBuffer;                // per-pixel depth test (else painter's algorithm)
    std::size_t polygonMaxVertices;     // filled polygons (scanline scratch from the pools)
    bool graphLayerCache;               // GRAPH keeps its static layers (180 KB) while the cursor moves
};
// SRAM only (Pico 2 W, or Pico Plus 2 W with the PSRAM disabled): the v1.8.0
// budgets, sized for the ~194 KB heap left next to the 200 KB framebuffer.
inline constexpr RamProfile kSramProfile{"SRAM 520 KB", 6000, 24000, 96u * 1024u, 24, false, 256, false};
// PSRAM pool active: the same data, served by the 8 MB PSRAM.
inline constexpr RamProfile kPsramProfile{"SRAM 520 KB + PSRAM 8 MB", 40000, 400000, 1024u * 1024u, 128, true, 4096, true};
// Profile for the pools that exist now.
const RamProfile& ram() noexcept;

// ---- Memory-derived limits ----------------------------------------------------------
// Bytes usable for heavy data right now (PREFER_PSRAM pools minus reserves).
std::size_t heavyBudget() noexcept;
// True when `bytes` of heavy data can be allocated now (largest block and budget).
bool fitsHeavy(std::size_t bytes) noexcept;
// 3D surface: grid and depth buffer that fit the viewport and the free memory.
std::size_t graph3dGrid(std::size_t viewportPixels) noexcept;
bool graph3dZBuffer(std::size_t viewportPixels) noexcept;
// Legacy calculator API (editors.hpp, math_engine.hpp): Mat A..Z, List 1..26.
inline constexpr std::size_t kMatrixSlots = 26;
inline constexpr std::size_t kMatrixMaxDim = board::config::kHasPsram ? 999 : 40;
inline constexpr std::size_t kStatLists = 26;
inline constexpr std::size_t kSpreadsheetRows = 999, kSpreadsheetCols = 26;
std::size_t matrixMaxDim() noexcept;     // square matrix that can still be inverted now
std::size_t listMaxLength() noexcept;    // elements of one legacy list that fit now
std::size_t historyCapacity() noexcept;  // legacy Run-Matrix session (point_a.hpp)

// ---- Canvas -------------------------------------------------------------------------
// Dirty-rectangle list inside gfx::Canvas (no allocation in the drawing hot path).
inline constexpr std::size_t kDirtyRegionCapacity = board::config::kHasPsram ? 32 : 16;

// User-facing message for allocation failures.
inline constexpr const char* kOutOfMemoryMessage = "Memory ERROR: memoria insufficiente";

} // namespace fx::limits
