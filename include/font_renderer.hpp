#pragma once
#include "graphics_engine.hpp"
#include <cstddef>
#include <cstdint>

namespace font {

// Fixed-cell bitmap font. Rows are stored MSB-first, one byte per row
// (glyph width <= 8). Code points are sorted for binary search.
struct BitmapFont {
    std::uint8_t width;
    std::uint8_t height;
    std::uint8_t baseline;           // rows from the top of the cell to the baseline
    const std::uint32_t* codepoints;
    const std::uint8_t* bits;
    std::uint16_t count;
};
extern const BitmapFont kSmall;      // Spleen 6x12 (labels, status bar, exponents)
extern const BitmapFont kMedium;     // GNU Unifont 8x16 (main UI and math)

enum class Face : std::uint8_t { Tiny = 0, Small = 1, Medium = 2 };

struct TextMetrics { int width{}; int height{}; };

// Decodes one UTF-8 code point and advances p. Invalid sequences yield '?'.
std::uint32_t nextCodepoint(const unsigned char*& p) noexcept;
std::size_t glyphCount(const char* utf8) noexcept;

int advance(Face face) noexcept;      // horizontal advance in pixels
int lineHeight(Face face) noexcept;   // cell height in pixels
int baseline(Face face) noexcept;     // baseline offset from the top of the cell
bool hasGlyph(Face face, std::uint32_t codepoint) noexcept;

// New API: y is the TOP of the text cell. maxWidth<=0 means up to the canvas edge.
TextMetrics measure(Face face, const char* text) noexcept;
TextMetrics draw(gfx::Canvas& canvas, Face face, int x, int y, const char* text,
                 std::uint16_t color, int maxWidth = 0) noexcept;
// Draws text and returns the number of glyphs drawn before maxWidth was reached.
std::size_t drawClipped(gfx::Canvas& canvas, Face face, int x, int y, const char* text,
                        std::uint16_t color, int maxWidth) noexcept;

// Legacy 5x7 API (scale 1..3), kept for existing callers and tests.
TextMetrics measure(const char* text, int scale = 1) noexcept;
TextMetrics drawText(gfx::Canvas& canvas, int x, int y, const char* text,
                     std::uint16_t color, int scale = 1, int maxWidth = 0) noexcept;
} // namespace font
