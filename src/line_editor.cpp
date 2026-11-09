#include "line_editor.hpp"
#include "font_renderer.hpp"
#include <algorithm>

namespace fx {
namespace {
bool continuation(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }
std::size_t glyphIndex(const std::string& s, std::size_t byte) {
    std::size_t g = 0;
    for (std::size_t i = 0; i < byte && i < s.size(); ++i) if (!continuation(s[i])) ++g;
    return g;
}
} // namespace

void LineEditor::set(std::string text) {
    if (text.size() > max_) text.resize(max_);
    text_ = std::move(text);
    cursor_ = text_.size();
    scroll_ = 0;
}
void LineEditor::insert(const std::string& s) {
    if (text_.size() + s.size() > max_) return;
    text_.insert(cursor_, s);
    cursor_ += s.size();
}
bool LineEditor::key(const KeyEvent& ev) {
    switch (ev.key) {
    case Key::Char:
        if (ev.ctrl) return false;
        insert(std::string(1, ev.ch));
        return true;
    case Key::Left:
        if (cursor_ > 0) { --cursor_; while (cursor_ > 0 && continuation(text_[cursor_])) --cursor_; }
        return true;
    case Key::Right:
        if (cursor_ < text_.size()) { ++cursor_; while (cursor_ < text_.size() && continuation(text_[cursor_])) ++cursor_; }
        return true;
    case Key::Home: cursor_ = 0; return true;
    case Key::End: cursor_ = text_.size(); return true;
    case Key::Del:
        if (cursor_ > 0) {
            std::size_t start = cursor_ - 1;
            while (start > 0 && continuation(text_[start])) --start;
            text_.erase(start, cursor_ - start); cursor_ = start;
        }
        return true;
    case Key::DelForward:
        if (cursor_ < text_.size()) {
            std::size_t end = cursor_ + 1;
            while (end < text_.size() && continuation(text_[end])) ++end;
            text_.erase(cursor_, end - cursor_);
        }
        return true;
    case Key::AC: clear(); return true;
    case Key::Store: insert("\xE2\x86\x92"); return true;
    default: return false;
    }
}
void LineEditor::draw(gfx::Canvas& c, int x, int y, int width, std::uint16_t ink, bool showCursor, bool masked) const {
    const int adv = font::advance(font::Face::Medium);
    const int visible = std::max(1, width / adv - 1);
    const int cur = static_cast<int>(glyphIndex(text_, cursor_));
    if (cur < scroll_) scroll_ = cur;
    if (cur > scroll_ + visible) scroll_ = cur - visible;
    std::string shown;
    if (masked) shown.assign(glyphIndex(text_, text_.size()), '*');
    else shown = text_;
    // Skip scroll_ glyphs.
    std::size_t start = 0; int g = 0;
    while (start < shown.size() && g < scroll_) { ++start; while (start < shown.size() && continuation(shown[start])) ++start; ++g; }
    font::drawClipped(c, font::Face::Medium, x, y, shown.c_str() + start, ink, width);
    if (showCursor) {
        const int cx = x + (cur - scroll_) * adv;
        c.vLine(cx, y + 1, 14, ink);
        c.vLine(cx + 1, y + 1, 14, ink);
    }
    if (scroll_ > 0) c.rect(x, y, 2, 16, ink, true);
}
} // namespace fx
