#pragma once
#include "graphics_engine.hpp"
#include "natural_editor.hpp"
#include <cstddef>
#include <vector>

namespace fx::math {
struct Size { int width{}, height{}, baseline{}; };   // baseline = ascent above the baseline
enum class Align { Left, Center, Right };

// Dove disegnare il cursore: una Row dell'albero e un indice in essa.
// Facoltativi: un intervallo evidenziato di una riga (selezione CLIP o causa
// di un errore) e la colorazione delle parentesi per livello (solo per
// l'input in corso: dopo l'esecuzione le parentesi tornano nere).
struct CursorTarget {
    const NaturalNode* row{};
    std::size_t index{};
    const NaturalNode* markRow{};
    std::size_t markBegin{}, markEnd{};
    bool markIsError{};
    bool colorParens{};
};

// Colori usati per disegnare un layout.
struct Palette {
    std::uint16_t ink{}, cursor{}, placeholder{};
    std::uint16_t paren[5]{};          // livelli 1..5 (fx-CG50: blu, rosso, verde, magenta, nero)
    std::uint16_t selection{}, error{};  // sfondi degli intervalli evidenziati
};
// Colore delle parentesi del livello `level` (da 0, si ripete ogni 5).
constexpr std::uint8_t parenColorIndex(int level) noexcept { return static_cast<std::uint8_t>(1 + (level < 0 ? 0 : level % 5)); }

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

// Disegna le operazioni del layout: prima le evidenziazioni, poi testo,
// linee, segnaposto e cursore.
void draw(gfx::Canvas& canvas, const Layout& layout, const Palette& palette);
// Disegno in un solo colore (parentesi del colore dell'inchiostro, evidenziazioni predefinite).
void draw(gfx::Canvas& canvas, const Layout& layout, std::uint16_t ink,
          std::uint16_t cursorColor, std::uint16_t placeholderColor);

// Glyph text shown for one editor token ("*" -> "×", "pi" -> "π", ...).
std::string displayToken(const std::string& text);
} // namespace fx::math
