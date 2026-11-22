// Editor di testo multiriga (v1.15.0, F5): vedi text_editor.hpp.
#include "text_editor.hpp"
#include "font_renderer.hpp"
#include <algorithm>
#include <new>

namespace fx {
namespace {
constexpr font::Face Face = font::Face::Medium;
bool continuation(unsigned char c) noexcept { return (c & 0xC0) == 0x80; }
std::size_t prevBoundary(const std::string& s, std::size_t i) noexcept {
    if (i == 0) return 0;
    --i;
    while (i > 0 && continuation(static_cast<unsigned char>(s[i]))) --i;
    return i;
}
std::size_t nextBoundary(const std::string& s, std::size_t i) noexcept {
    if (i >= s.size()) return s.size();
    ++i;
    while (i < s.size() && continuation(static_cast<unsigned char>(s[i]))) ++i;
    return i;
}
// Caratteri (code point) in s[a, b).
std::size_t glyphsBetween(const std::string& s, std::size_t a, std::size_t b) noexcept {
    std::size_t n = 0;
    for (std::size_t i = a; i < b && i < s.size(); ++i) if (!continuation(static_cast<unsigned char>(s[i]))) ++n;
    return n;
}
// Byte dopo `glyphs` caratteri a partire da a.
std::size_t advanceGlyphs(const std::string& s, std::size_t a, std::size_t glyphs) noexcept {
    std::size_t i = a;
    while (glyphs > 0 && i < s.size()) { i = nextBoundary(s, i); --glyphs; }
    return i;
}
// Riporta un indice all'inizio del carattere che lo contiene.
std::size_t snap(const std::string& s, std::size_t i) noexcept {
    if (i >= s.size()) return s.size();
    while (i > 0 && continuation(static_cast<unsigned char>(s[i]))) --i;
    return i;
}
} // namespace

TextEditor::TextEditor(std::size_t maxBytes, std::size_t maxLineBytes) : maxBytes_(maxBytes), maxLineBytes_(maxLineBytes) {}

bool TextEditor::fits(std::size_t addBytes, std::size_t lineBytes) const noexcept {
    return bytes_ + addBytes <= maxBytes_ && lineBytes <= maxLineBytes_;
}

void TextEditor::clear() {
    lines_.assign(1, std::string());
    line_ = col_ = bytes_ = 0;
    first_ = 0; scrollX_ = 0;
    selecting_ = full_ = false;
}

bool TextEditor::setText(const std::string& text) {
    full_ = false;
    if (text.size() > maxBytes_) { full_ = true; return false; }
    try {
        std::vector<std::string> lines;
        std::size_t start = 0;
        while (true) {
            const std::size_t nl = text.find('\n', start);
            const std::size_t end = nl == std::string::npos ? text.size() : nl;
            std::string l = text.substr(start, end - start);
            if (!l.empty() && l.back() == '\r') l.pop_back();             // file scritti su Windows
            if (l.size() > maxLineBytes_) { full_ = true; return false; }
            lines.push_back(std::move(l));
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        std::size_t bytes = lines.size() - 1;
        for (const auto& l : lines) bytes += l.size();
        lines_.swap(lines);
        bytes_ = bytes;
    } catch (const std::bad_alloc&) {
        full_ = true;
        return false;
    }
    line_ = col_ = 0; first_ = 0; scrollX_ = 0; selecting_ = false;
    return true;
}

std::string TextEditor::text() const {
    std::string out;
    out.reserve(bytes_);
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (i) out += '\n';
        out += lines_[i];
    }
    return out;
}

void TextEditor::moveTo(std::size_t line, std::size_t column) noexcept {
    line_ = std::min(line, lines_.size() - 1);
    col_ = snap(lines_[line_], column);
    wantedCol_ = glyphsBetween(lines_[line_], 0, col_);
}
void TextEditor::left() noexcept {
    if (col_ > 0) col_ = prevBoundary(lines_[line_], col_);
    else if (line_ > 0) { --line_; col_ = lines_[line_].size(); }
    wantedCol_ = glyphsBetween(lines_[line_], 0, col_);
}
void TextEditor::right() noexcept {
    if (col_ < lines_[line_].size()) col_ = nextBoundary(lines_[line_], col_);
    else if (line_ + 1 < lines_.size()) { ++line_; col_ = 0; }
    wantedCol_ = glyphsBetween(lines_[line_], 0, col_);
}
void TextEditor::up() noexcept {
    if (line_ == 0) { col_ = 0; return; }
    --line_;
    col_ = advanceGlyphs(lines_[line_], 0, wantedCol_);
}
void TextEditor::down() noexcept {
    if (line_ + 1 >= lines_.size()) { col_ = lines_[line_].size(); return; }
    ++line_;
    col_ = advanceGlyphs(lines_[line_], 0, wantedCol_);
}
void TextEditor::pageUp() noexcept {
    const std::size_t n = static_cast<std::size_t>(visibleRows_);
    line_ = line_ > n ? line_ - n : 0;
    col_ = advanceGlyphs(lines_[line_], 0, wantedCol_);
}
void TextEditor::pageDown() noexcept {
    const std::size_t n = static_cast<std::size_t>(visibleRows_);
    line_ = std::min(lines_.size() - 1, line_ + n);
    col_ = advanceGlyphs(lines_[line_], 0, wantedCol_);
}

bool TextEditor::insertPlain(const std::string& s) {
    std::string& l = lines_[line_];
    if (!fits(s.size(), l.size() + s.size())) { full_ = true; return false; }
    std::string updated;
    updated.reserve(l.size() + s.size());                // puo' lanciare: nulla e' cambiato
    updated.append(l, 0, col_);
    updated += s;
    updated.append(l, col_, std::string::npos);
    l.swap(updated);
    col_ += s.size();
    bytes_ += s.size();
    return true;
}

bool TextEditor::insert(const std::string& utf8) {
    full_ = false;
    if (utf8.empty()) return true;
    if (hasSelection() && !eraseSelection()) return false;
    selecting_ = false;
    try {
        if (utf8.find('\n') == std::string::npos) {
            const bool ok = insertPlain(utf8);
            if (ok) wantedCol_ = glyphsBetween(lines_[line_], 0, col_);
            return ok;
        }
        // Testo su piu' righe: si costruisce il risultato a parte (garanzia forte).
        std::vector<std::string> pieces;
        std::size_t start = 0;
        while (true) {
            const std::size_t nl = utf8.find('\n', start);
            pieces.push_back(utf8.substr(start, (nl == std::string::npos ? utf8.size() : nl) - start));
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        const std::string& cur = lines_[line_];
        std::vector<std::string> fresh;
        fresh.reserve(pieces.size());
        fresh.push_back(cur.substr(0, col_) + pieces.front());
        for (std::size_t i = 1; i + 1 < pieces.size(); ++i) fresh.push_back(pieces[i]);
        fresh.push_back(pieces.back() + cur.substr(col_));
        for (const auto& f : fresh) if (f.size() > maxLineBytes_) { full_ = true; return false; }
        if (!fits(utf8.size(), 0)) { full_ = true; return false; }
        const std::size_t newCol = pieces.back().size();
        lines_.reserve(lines_.size() + fresh.size() - 1);
        lines_[line_].swap(fresh.front());
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(line_) + 1,
                      std::make_move_iterator(fresh.begin() + 1), std::make_move_iterator(fresh.end()));
        line_ += fresh.size() - 1;
        col_ = newCol;
        bytes_ += utf8.size();
        wantedCol_ = glyphsBetween(lines_[line_], 0, col_);
        return true;
    } catch (const std::bad_alloc&) {
        full_ = true;
        return false;
    }
}

bool TextEditor::newline() { return insert("\n"); }

void TextEditor::deleteRange(std::size_t l0, std::size_t c0, std::size_t l1, std::size_t c1) {
    // [l0:c0, l1:c1) con l0:c0 <= l1:c1; non alloca oltre il risultato della riga unita.
    if (l0 == l1) {
        lines_[l0].erase(c0, c1 - c0);
        bytes_ -= c1 - c0;
    } else {
        std::size_t removed = lines_[l0].size() - c0 + 1;            // resto della prima riga + '\n'
        for (std::size_t i = l0 + 1; i < l1; ++i) removed += lines_[i].size() + 1;
        removed += c1;
        std::string merged = lines_[l0].substr(0, c0) + lines_[l1].substr(c1);
        lines_[l0].swap(merged);
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(l0) + 1, lines_.begin() + static_cast<std::ptrdiff_t>(l1) + 1);
        bytes_ -= removed;
    }
    line_ = l0; col_ = c0;
    wantedCol_ = glyphsBetween(lines_[line_], 0, col_);
}

void TextEditor::backspace() {
    full_ = false;
    try {
        if (hasSelection()) { (void)eraseSelection(); return; }
        selecting_ = false;
        if (col_ > 0) deleteRange(line_, prevBoundary(lines_[line_], col_), line_, col_);
        else if (line_ > 0) deleteRange(line_ - 1, lines_[line_ - 1].size(), line_, 0);
    } catch (const std::bad_alloc&) { full_ = true; }
}
void TextEditor::deleteForward() {
    full_ = false;
    try {
        if (hasSelection()) { (void)eraseSelection(); return; }
        selecting_ = false;
        if (col_ < lines_[line_].size()) deleteRange(line_, col_, line_, nextBoundary(lines_[line_], col_));
        else if (line_ + 1 < lines_.size()) deleteRange(line_, col_, line_ + 1, 0);
    } catch (const std::bad_alloc&) { full_ = true; }
}
void TextEditor::clearLine() {
    selecting_ = false;
    bytes_ -= lines_[line_].size();
    lines_[line_].clear();
    col_ = 0; wantedCol_ = 0;
}
void TextEditor::deleteLine() {
    selecting_ = false;
    if (lines_.size() == 1) { clearLine(); return; }
    bytes_ -= lines_[line_].size() + 1;
    lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(line_));
    if (line_ >= lines_.size()) line_ = lines_.size() - 1;
    col_ = 0; wantedCol_ = 0;
}

std::string TextEditor::selectedText() const {
    if (!hasSelection()) return {};
    std::size_t l0 = anchorLine_, c0 = anchorCol_, l1 = line_, c1 = col_;
    if (l1 < l0 || (l1 == l0 && c1 < c0)) { std::swap(l0, l1); std::swap(c0, c1); }
    if (l0 == l1) return lines_[l0].substr(c0, c1 - c0);
    std::string out = lines_[l0].substr(c0);
    for (std::size_t i = l0 + 1; i < l1; ++i) { out += '\n'; out += lines_[i]; }
    out += '\n';
    out += lines_[l1].substr(0, c1);
    return out;
}
bool TextEditor::eraseSelection() {
    if (!hasSelection()) { selecting_ = false; return true; }
    std::size_t l0 = anchorLine_, c0 = anchorCol_, l1 = line_, c1 = col_;
    if (l1 < l0 || (l1 == l0 && c1 < c0)) { std::swap(l0, l1); std::swap(c0, c1); }
    try {
        deleteRange(l0, c0, l1, c1);
    } catch (const std::bad_alloc&) { full_ = true; return false; }
    selecting_ = false;
    return true;
}

bool TextEditor::find(const std::string& needle) {
    if (needle.empty()) return false;
    const std::size_t n = lines_.size();
    auto at = [&](std::size_t li, std::size_t hit) {
        selecting_ = false;
        line_ = li; col_ = hit;
        wantedCol_ = glyphsBetween(lines_[line_], 0, col_);
        return true;
    };
    // la riga del cursore dopo il cursore, poi le righe seguenti e da capo
    // (l'ultimo giro rilegge la riga del cursore dall'inizio)
    const std::string& cur = lines_[line_];
    if (col_ < cur.size()) {
        const std::size_t hit = cur.find(needle, nextBoundary(cur, col_));
        if (hit != std::string::npos) return at(line_, hit);
    }
    for (std::size_t k = 1; k <= n; ++k) {
        const std::size_t li = (line_ + k) % n;
        const std::size_t hit = lines_[li].find(needle);
        if (hit != std::string::npos) return at(li, hit);
    }
    return false;
}

bool TextEditor::gotoLine(std::size_t oneBased) noexcept {
    if (oneBased == 0 || oneBased > lines_.size()) return false;
    selecting_ = false;
    line_ = oneBased - 1; col_ = 0; wantedCol_ = 0;
    return true;
}

bool TextEditor::key(const KeyEvent& ev, std::string* clipboard) {
    if (ev.key == Key::Char && ev.ctrl) {
        const char c = static_cast<char>(ev.ch | 0x20);
        if (c == 'c' || c == 'x') {
            if (!clipboard) return true;
            *clipboard = hasSelection() ? selectedText() : lines_[line_];
            if (c == 'x') { if (hasSelection()) (void)eraseSelection(); else deleteLine(); }
            return true;
        }
        if (c == 'v') { if (clipboard) (void)paste(*clipboard); return true; }
        return false;
    }
    const bool extend = ev.ctrl && (ev.key == Key::Left || ev.key == Key::Right || ev.key == Key::Up || ev.key == Key::Down);
    if (extend && !selecting_) beginSelection();
    if (!extend && ev.key != Key::Char && ev.key != Key::Del && ev.key != Key::DelForward) selecting_ = false;
    switch (ev.key) {
    case Key::Char: (void)insert(std::string(1, ev.ch)); return true;
    case Key::Exe: if (!exeNewline_) return false; (void)newline(); return true;
    case Key::Del: backspace(); return true;
    case Key::DelForward: deleteForward(); return true;
    case Key::AC: clearLine(); return true;
    case Key::Left: left(); return true;
    case Key::Right: right(); return true;
    case Key::Up: up(); return true;
    case Key::Down: down(); return true;
    case Key::Home: if (ev.ctrl) top(); else home(); return true;
    case Key::End: if (ev.ctrl) bottom(); else end(); return true;
    case Key::PageUp: pageUp(); return true;
    case Key::PageDown: pageDown(); return true;
    default: return false;
    }
}

std::vector<std::size_t> TextEditor::wrapPoints(std::size_t line, int width) const {
    std::vector<std::size_t> out{0};
    const std::string& s = lines_.at(line);
    const int adv = font::advance(Face);
    const std::size_t perRow = static_cast<std::size_t>(std::max(1, width / std::max(1, adv)));
    if (!wrap_) return out;
    std::size_t start = 0;
    while (glyphsBetween(s, start, s.size()) > perRow) {
        std::size_t cut = advanceGlyphs(s, start, perRow);
        // a capo dopo l'ultimo spazio del pezzo, se c'e' (Word Wrap della fx-CG50)
        const std::size_t space = s.rfind(' ', cut - 1);
        if (space != std::string::npos && space >= start && space + 1 > start && space + 1 < cut) cut = space + 1;
        out.push_back(cut);
        start = cut;
    }
    return out;
}

void TextEditor::draw(gfx::Canvas& c, gfx::Rect area, std::uint16_t ink, bool showCursor) const {
    const int lh = font::lineHeight(Face), adv = font::advance(Face);
    const int rows = std::max(1, area.h / lh);
    // Riga visiva del cursore e prima riga mostrata.
    if (line_ < first_) first_ = line_;
    auto rowsOf = [&](std::size_t l) { return wrap_ ? wrapPoints(l, area.w).size() : std::size_t{1}; };
    auto cursorRowIn = [&](std::size_t l) {
        if (!wrap_) return std::size_t{0};
        const auto pts = wrapPoints(l, area.w);
        std::size_t r = 0;
        while (r + 1 < pts.size() && col_ >= pts[r + 1]) ++r;
        return r;
    };
    while (true) {                                                  // il cursore deve stare nelle righe visibili
        std::size_t used = 0;
        for (std::size_t l = first_; l < line_; ++l) used += rowsOf(l);
        used += cursorRowIn(line_) + 1;
        if (used <= static_cast<std::size_t>(rows) || first_ >= line_) break;
        ++first_;
    }
    if (!wrap_) {                                                    // scorrimento orizzontale
        const int cx = static_cast<int>(glyphsBetween(lines_[line_], 0, col_)) * adv;
        if (cx - scrollX_ > area.w - adv) scrollX_ = cx - area.w + adv;
        if (cx < scrollX_) scrollX_ = cx;
    } else {
        scrollX_ = 0;
    }
    std::size_t sl0 = 0, sc0 = 0, sl1 = 0, sc1 = 0;
    const bool sel = hasSelection();
    if (sel) {
        sl0 = anchorLine_; sc0 = anchorCol_; sl1 = line_; sc1 = col_;
        if (sl1 < sl0 || (sl1 == sl0 && sc1 < sc0)) { std::swap(sl0, sl1); std::swap(sc0, sc1); }
    }
    int y = area.y;
    for (std::size_t l = first_; l < lines_.size() && y + lh <= area.y + area.h; ++l) {
        const std::string& s = lines_[l];
        const auto pts = wrap_ ? wrapPoints(l, area.w) : std::vector<std::size_t>{0};
        for (std::size_t r = 0; r < pts.size() && y + lh <= area.y + area.h; ++r) {
            const std::size_t a = pts[r], b = r + 1 < pts.size() ? pts[r + 1] : s.size();
            const std::string piece = s.substr(a, b - a);
            if (sel && l >= sl0 && l <= sl1) {                        // evidenziazione della selezione
                const std::size_t from = l == sl0 ? std::max(a, sc0) : a, to = l == sl1 ? std::min(b, sc1) : b;
                if (from < to) {
                    const int x0 = area.x + static_cast<int>(glyphsBetween(s, a, from)) * adv - scrollX_;
                    const int w = static_cast<int>(glyphsBetween(s, from, to)) * adv;
                    c.rect(x0, y, w, lh, gfx::rgb888To565(200, 222, 252), true);
                }
            }
            if (scrollX_ == 0) font::draw(c, Face, area.x, y, piece.c_str(), ink, area.w);
            else {
                const std::size_t skip = advanceGlyphs(piece, 0, static_cast<std::size_t>(scrollX_ / adv));
                font::draw(c, Face, area.x, y, piece.c_str() + skip, ink, area.w);
            }
            const bool here = l == line_ && col_ >= a && (col_ < b || r + 1 == pts.size());
            if (showCursor && here) {
                const int cx = area.x + static_cast<int>(glyphsBetween(s, a, col_)) * adv - scrollX_;
                c.rect(cx, y + 1, 2, lh - 2, ink, true);
            }
            y += lh;
        }
    }
}

} // namespace fx
