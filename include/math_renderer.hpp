#pragma once
#include "graphics_engine.hpp"
#include "natural_editor.hpp"
#include <cstddef>
#include <vector>

namespace fx::math {
struct Size { int width{}, height{}, baseline{}; };   // baseline = ascent above the baseline
enum class Align { Left, Center, Right };

// Where to draw the editing cursor: a Row of the tree and an index in it.
struct CursorTarget { const NaturalNode* row{}; std::size_t index{}; };

struct Layout {
    std::vector<NaturalDrawOp> ops;
    Size size;
    int x{}, top{}, width{}, height{};
    bool clipped{};
    bool hasCursor{};
    int cursorX{}, cursorTop{}, cursorBottom{};       // before clipping, for scrolling
};

// Font faces used by the layout: level 0 text uses Medium (8x16), exponents,
// indices and limits use Small (6x12).
constexpr std::uint8_t FaceMedium = 0, FaceSmall = 1;

Size measure(const NaturalNode& node) noexcept;
// baseline is the y of the text baseline; maxWidth/maxHeight define the clip box
// (x .. x+maxWidth, baseline-ascent .. +maxHeight).
Layout layout(const NaturalNode& node, int x, int baseline, int maxWidth, int maxHeight = 220,
              Align align = Align::Left, const CursorTarget* cursor = nullptr);

// Renders layout operations onto a canvas.
void draw(gfx::Canvas& canvas, const Layout& layout, std::uint16_t ink,
          std::uint16_t cursorColor, std::uint16_t placeholderColor);

// Glyph text shown for one editor token ("*" -> "×", "pi" -> "π", ...).
std::string displayToken(const std::string& text);
} // namespace fx::math
