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
// List 1-26 (v1.11.0, record v7): elementi di tutte le liste insieme. Sul Pico
// 2 W resta l'occupazione massima della v1.10.0 (6 liste x 255 = 1530 elementi,
// ~12 KB nel record da 59,75 KB); la Pico Plus 2 W tiene le liste nella PSRAM
// e nel record da 512 KB (20 000 elementi, ~156 KB, come le matrici).
inline constexpr std::size_t kListCount = 26;
inline constexpr std::size_t kListStoreElements = board::config::kIsPicoPlus2W ? 20000 : 1530;
inline constexpr std::size_t kListNameLength = 8;      // sottonome di una lista (fx-CG50: 8 caratteri)
// v1.12.0 (record v8): List File 1-6 della fx-CG50. Il budget kListStoreElements
// vale per le liste di tutti i file insieme (stessa RAM e stesso record).
inline constexpr std::size_t kListFiles = 6;
inline constexpr std::size_t kFunctionMemories = 20;   // f1-f20 (OPTN › FUNCMEM)
inline constexpr std::size_t kStrings = 20;            // Str 1-20 (VARS › Str)
inline constexpr std::size_t kStringLength = 255;      // caratteri di una stringa (fx-CG50: 255)
inline constexpr std::size_t kFunctionMemoryLength = 255;
inline constexpr std::size_t kChatMessages = 30;   // AI conversation (the relay keeps the last 20 turns)
// Matrici e vettori di Run-Matrix (Mat A-Z, Vct A-Z, MatAns, VctAns; record
// v6). Le due schede non hanno la stessa memoria: il Pico 2 W li tiene nel suo
// heap SRAM di ~190 KB e in uno slot del record da 64 KB, il Pico Plus 2 W
// nella PSRAM da 8 MB e in uno slot da 512 KB. Un elemento reale costa 8 byte,
// uno complesso 16.
inline constexpr std::size_t kMatrixElements = board::config::kIsPicoPlus2W ? 10000 : 400;        // una matrice o un vettore
inline constexpr std::size_t kMatrixStoreElements = board::config::kIsPicoPlus2W ? 20000 : 1200;  // tutte le memorie insieme
// Risultati mostrati in forma naturale nella cronologia (fx-CG50: matrici fino
// a 6 x 6, vettori fino a 6 elementi, liste fino a 20); i risultati piu' grandi
// si leggono dalle memorie delle risposte (MatAns, VctAns, ListAns).
// v1.12.0: forme esatte delle celle (1/3, 2√3, π/4) di tutte le memorie di
// matrici, vettori e risposte insieme. Nessun limite per singola memoria (la
// v1.11.0 si fermava a 64 elementi): conta solo la memoria della scheda
// (~26 byte per forma nel record: 10 KB sul Pico 2 W, 104 KB sulla Plus).
inline constexpr std::size_t kExactStoreElements = board::config::kIsPicoPlus2W ? 4000 : 400;
// v1.12.0: punti di DrawStat / DrawR / DrawFTG (elementi SKETCH "Data") di
// tutta la schermata del grafico (8 byte a punto nel record).
inline constexpr std::size_t kSketchDataPoints = board::config::kIsPicoPlus2W ? 4000 : 1000;
inline constexpr std::size_t kNaturalMatrixDim = 6;
inline constexpr std::size_t kNaturalListLength = 20;

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
