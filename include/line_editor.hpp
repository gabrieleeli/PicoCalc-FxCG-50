#pragma once
#include "graphics_engine.hpp"
#include "keyboard_input.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace fx {
// Single-line UTF-8 text field with a cursor and horizontal scrolling. Used for
// numeric cells, settings, the solver equation and the AI chat input.
class LineEditor {
public:
    explicit LineEditor(std::size_t maxLength = 240) : max_(maxLength) {}
    void set(std::string text);
    void clear() { text_.clear(); cursor_ = 0; scroll_ = 0; }
    const std::string& text() const noexcept { return text_; }
    bool empty() const noexcept { return text_.empty(); }
    std::size_t cursor() const noexcept { return cursor_; }
    void insert(const std::string& s);
    // Handles printable characters, arrows, Home/End, DEL, DEL-forward and AC.
    // Returns true when the key was consumed.
    bool key(const KeyEvent& ev);
    void draw(gfx::Canvas& c, int x, int y, int width, std::uint16_t ink, bool showCursor = true,
              bool masked = false) const;
private:
    std::string text_;
    std::size_t cursor_{};          // byte offset on a code-point boundary
    std::size_t max_;
    mutable int scroll_{};          // first visible glyph
};
} // namespace fx
