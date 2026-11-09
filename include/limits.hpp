#pragma once
// Capacity limits: per-board profile + runtime limits derived from free memory.
//
// Two kinds of limits exist and are kept deliberately separate:
//  * capacity limits (data sizes): the board profile gives an upper bound and
//    the runtime functions lower it to what the pools can actually hold now
//    (mem::largestFree / free bytes). On the Pico Plus 2 W the caps are the
//    largest values the 8 MiB PSRAM can serve, not values sized for 520 KiB.
//  * structural safety limits (recursion depth, loop counts): they protect the
//    SRAM stacks and the CPU time budget, are the same on both boards and are
//    never removed.
// Every large allocation must still check for failure: a limit computed now
// can be invalidated by the next allocation (see "Memoria insufficiente").

#include "board_config.hpp"

#include <cstddef>
#include <cstdint>

namespace fx::limits {

struct Profile {
    const char* name;
    // Calculator data
    std::size_t matrixSlots;          // Mat A..Z
    std::size_t matrixMaxDim;         // rows/cols upper bound
    std::size_t statLists;            // List 1..26
    std::size_t listMaxLength;        // elements per list
    std::size_t spreadsheetRows;
    std::size_t spreadsheetCols;
    std::size_t historyEntries;       // Run-Matrix history
    std::size_t editorMaxTokens;      // natural-editor nodes per expression
    // Graphs
    std::size_t graphMaxFunctions;    // simultaneous Y= functions
    std::size_t graphSamplesPerPixel; // oversampling of each plotted column
    bool graphLayerCache;             // keep the rendered plot as an off-screen layer
    std::size_t graph3dMaxGrid;       // surface grid (cells per side)
    bool graph3dZBuffer;              // z-buffer (else painter's algorithm)
    // UI
    std::size_t maxWindows;
    std::size_t maxNavigationDepth;
    std::size_t windowSnapshots;      // full-screen snapshots kept by the window manager
    std::size_t polygonMaxVertices;
    // Text / AI / CAS
    std::size_t aiRequestBytes;
    std::size_t aiResponseBytes;
    std::size_t textBufferBytes;
    std::size_t casArenaBytes;
};

inline constexpr Profile kPico2WProfile{
    "Pico 2 W (SRAM 520 KiB)",
    26, 40, 26, 999, 999, 26, 30, 256,
    20, 1, false, 24, false,
    16, 16, 0, 256,
    8u * 1024u, 16u * 1024u, 4u * 1024u, 0u,
};

inline constexpr Profile kPicoPlus2WProfile{
    "Pico Plus 2 W (SRAM 520 KiB + PSRAM 8 MiB)",
    26, 999, 26, 1000000, 999, 26, 10000, 4096,
    64, 8, true, 128, true,
    64, 64, 8, 4096,
    256u * 1024u, 2u * 1024u * 1024u, 256u * 1024u, 1024u * 1024u,
};

inline constexpr const Profile& kProfile = board::config::kHasPsram ? kPicoPlus2WProfile : kPico2WProfile;

// ---- Compile-time limits ------------------------------------------------------
// Canvas dirty-rectangle list lives inside gfx::Canvas (no allocation in the
// hot drawing path): sized per board at compile time.
inline constexpr std::size_t kDirtyRegionCapacity = board::config::kHasPsram ? 32 : 16;
// Structural, stack-bound recursion limits (same stack size on both boards).
// Derived from the ARM frame sizes reported by -fstack-usage for the recursive
// descent parser (expr->term->unary->power->postfix->atom) and the layout
// engine; see docs/PICO_PLUS_2W.md, "Limiti strutturali".
inline constexpr unsigned kMaxParseDepth = 48;    // nested unary/parenthesis levels
inline constexpr unsigned kMaxAstDepth = 64;      // AST evaluation/serialisation depth
inline constexpr unsigned kMaxLayoutDepth = 32;   // math layout recursion
inline constexpr unsigned kMaxEditorNesting = 12; // nested VPAM templates
// CPU-time guard (not memory): terms of sum(f,a,b).
inline constexpr std::size_t kMaxSummationTerms = 10001;

// ---- Runtime limits -------------------------------------------------------------
const Profile& profile() noexcept;
// Bytes usable for heavy data right now (PREFER_PSRAM pools minus reserves).
std::size_t heavyBudget() noexcept;
// True when `bytes` of heavy data can be allocated now (largest block and budget).
bool fitsHeavy(std::size_t bytes) noexcept;

std::size_t matrixMaxDim() noexcept;        // square matrix that can still be inverted
std::size_t listMaxLength() noexcept;
std::size_t historyCapacity() noexcept;
std::size_t editorMaxTokens() noexcept;
std::size_t graphMaxFunctions() noexcept;
std::size_t graphSamplesPerPixel(std::size_t functions, std::size_t columns) noexcept;
std::size_t graph3dGrid(std::size_t viewportPixels) noexcept;
bool graph3dZBuffer(std::size_t viewportPixels) noexcept;
std::size_t windowSnapshots(std::size_t frameBytes) noexcept;
std::size_t aiRequestCapacity() noexcept;
std::size_t aiResponseCapacity() noexcept;

// User-facing message for allocation failures.
inline constexpr const char* kOutOfMemoryMessage = "Memoria insufficiente";

} // namespace fx::limits
