#pragma once
// Editor di testo multiriga (v1.15.0, F5): righe di testo e striscia Notes di
// eActivity, memo, sorgenti di Program e script Python.
//
// Il testo e' un elenco di righe UTF-8 (separate da '\n' in text()). Il
// cursore sta sempre all'inizio di un carattere; le righe piu' larghe della
// finestra vanno a capo (wrap) oppure scorrono in orizzontale. Ogni modifica
// rispetta due limiti (byte di tutto il testo e di una riga: limits.hpp) e da'
// la garanzia forte: se manca memoria (std::bad_alloc) o il limite e'
// superato il testo resta com'era e full() lo dice (la modalita' mostra
// "Memory ERROR"). La clipboard e' quella di testo del Context, passata a key().
#include "graphics_engine.hpp"
#include "keyboard_input.hpp"
#include "limits.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace fx {

class TextEditor {
public:
    explicit TextEditor(std::size_t maxBytes = limits::kTextEditorBytes, std::size_t maxLineBytes = limits::kTextLineBytes);

    // ---- contenuto
    bool setText(const std::string& text);              // false (testo invariato) oltre i limiti o senza memoria
    std::string text() const;                           // righe unite con '\n'
    std::size_t lineCount() const noexcept { return lines_.size(); }
    const std::string& line(std::size_t i) const { return lines_.at(i); }
    std::size_t bytes() const noexcept { return bytes_; }   // byte di text()
    std::size_t maxBytes() const noexcept { return maxBytes_; }
    bool empty() const noexcept { return lines_.size() == 1 && lines_[0].empty(); }
    void clear();
    bool full() const noexcept { return full_; }       // l'ultima modifica e' stata rifiutata

    // ---- cursore (riga, byte nella riga)
    std::size_t cursorLine() const noexcept { return line_; }
    std::size_t cursorColumn() const noexcept { return col_; }
    void moveTo(std::size_t line, std::size_t column) noexcept;
    void left() noexcept;
    void right() noexcept;
    void up() noexcept;
    void down() noexcept;
    void home() noexcept { col_ = 0; }
    void end() noexcept { col_ = lines_[line_].size(); }
    void top() noexcept { line_ = 0; col_ = 0; }
    void bottom() noexcept { line_ = lines_.size() - 1; col_ = lines_[line_].size(); }
    void pageUp() noexcept;
    void pageDown() noexcept;

    // ---- modifiche (false = rifiutata, testo invariato)
    bool insert(const std::string& utf8);               // '\n' nel testo crea righe nuove
    bool newline();
    void backspace();
    void deleteForward();
    void clearLine();                                   // AC: svuota la riga del cursore
    void deleteLine();                                  // toglie la riga (ne resta almeno una)

    // ---- selezione e clipboard
    void beginSelection() noexcept { selecting_ = true; anchorLine_ = line_; anchorCol_ = col_; }
    void clearSelection() noexcept { selecting_ = false; }
    bool hasSelection() const noexcept { return selecting_ && (anchorLine_ != line_ || anchorCol_ != col_); }
    std::string selectedText() const;
    bool eraseSelection();                              // CUT senza copia
    bool paste(const std::string& text) { if (hasSelection() && !eraseSelection()) return false; return insert(text); }

    // ---- ricerca e salto
    // Cerca `needle` dopo il cursore (poi dall'inizio): true e cursore sul testo trovato.
    bool find(const std::string& needle);
    bool gotoLine(std::size_t oneBased) noexcept;       // false oltre l'ultima riga

    // ---- tastiera: caratteri, EXE (a capo, se ammesso), DEL, DEL avanti, AC,
    // frecce, Home/End, PageUp/PageDown; Ctrl+C/X/V con `clipboard`; Ctrl+frecce
    // estendono la selezione. True se il tasto e' stato usato.
    bool key(const KeyEvent& ev, std::string* clipboard = nullptr);
    void setNewlineOnExe(bool on) noexcept { exeNewline_ = on; }

    // ---- vista
    void setWrap(bool on) noexcept { wrap_ = on; }
    bool wrap() const noexcept { return wrap_; }
    void setVisibleRows(int rows) noexcept { visibleRows_ = rows > 0 ? rows : 1; }
    // Disegna nel rettangolo (font Medium, 16 px a riga) con il cursore;
    // tiene il cursore visibile (prima riga e scorrimento orizzontale).
    void draw(gfx::Canvas& c, gfx::Rect area, std::uint16_t ink, bool showCursor = true) const;
    // Righe visive della riga `line` larga `width` pixel (a capo: inizio di ogni pezzo).
    std::vector<std::size_t> wrapPoints(std::size_t line, int width) const;
    std::size_t firstLine() const noexcept { return first_; }

private:
    bool fits(std::size_t addBytes, std::size_t lineBytes) const noexcept;
    bool insertPlain(const std::string& s);              // senza '\n'
    void deleteRange(std::size_t l0, std::size_t c0, std::size_t l1, std::size_t c1);
    std::vector<std::string> lines_{std::string()};
    std::size_t line_{0}, col_{0};
    std::size_t bytes_{0};
    std::size_t maxBytes_, maxLineBytes_;
    bool full_{false}, selecting_{false}, wrap_{true}, exeNewline_{true};
    std::size_t anchorLine_{0}, anchorCol_{0};
    std::size_t wantedCol_{0};
    int visibleRows_{12};
    mutable std::size_t first_{0};
    mutable int scrollX_{0};
};

} // namespace fx
