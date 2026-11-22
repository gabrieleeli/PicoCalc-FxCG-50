#pragma once
#include "app.hpp"
#include "line_editor.hpp"
#include <functional>
#include <string>
#include <vector>

namespace fx::app {

// Generic modal popup list used by OPTN / MATH / ZOOM / G-SOLVE menus.
struct PopupMenu {
    const char* title{};
    std::vector<std::string> items;
    int selected{0}, first{0};
    bool open{false};
    void show(const char* t, std::vector<std::string> list) { title = t; items = std::move(list); selected = 0; first = 0; open = true; }
    void close() { open = false; }
    // Returns the chosen index (>=0), -1 when still open, -2 when cancelled.
    int key(const KeyEvent& ev);
    void draw(gfx::Canvas& c) const;
};


// Store for user files chosen in SYSTEM ("Archivio file"): the microSD card
// or LittleFS on the internal flash. nullptr when the build has neither.
files::FileSystem* userFiles(Context& ctx);

// Evaluates a numeric entry with the workspace memory (A-Z, Ans) in Real mode.
double evalNumber(Context& ctx, const std::string& text);
std::string fmt(Context& ctx, double v);

// Vertical form of labelled fields (V-Window, DIST/TEST parameters, settings).
struct FormField {
    std::string label;
    std::string text;                    // numeric expression or free text
    enum class Type { Number, Text, Choice, Secret } type{Type::Number};
    std::vector<std::string> choices;    // Choice fields
    int choice{0};
};
class Form {
public:
    std::vector<FormField> fields;
    int selected{0};
    bool editing() const noexcept { return editing_; }
    // Up/Down select, Left/Right change choices, characters or EXE start editing,
    // EXE confirms (numbers are validated by evaluation). Returns true if consumed.
    bool key(Context& ctx, const KeyEvent& ev);
    void draw(gfx::Canvas& c, int top, int bottom) const;
    double number(Context& ctx, std::size_t i) const { return evalNumber(ctx, fields.at(i).text); }
private:
    bool editing_{false};
    LineEditor editor_;
    mutable int first_{0};
};

// Scrollable "label = value" result screen.
class ResultList {
public:
    std::string title;
    std::vector<std::pair<std::string, std::string>> rows;
    void clear() { rows.clear(); first_ = 0; }
    void add(std::string label, std::string value) { rows.emplace_back(std::move(label), std::move(value)); }
    bool key(const KeyEvent& ev);
    void draw(gfx::Canvas& c, int top, int bottom) const;
private:
    int first_{0};
};

// Numeric cell grid with column headers and an edit line at the bottom.
class CellGrid {
public:
    int rows{1}, cols{1};
    int visibleCols{4};
    bool growRows{false};                 // statistics lists grow when typing below the last row
    bool rowMajor{false};                 // EXE advances to the next column (coefficient entry)
    std::function<int(int)> columnLength; // optional: cursor may not go past the first empty cell
    bool rowLabels{false};                // numeri di riga a sinistra (anche senza growRows)
    std::function<std::string(int)> header;
    std::function<bool(int, int, double&)> get;     // false = empty cell
    std::function<std::string(int, int)> text;      // facoltativo: testo di una cella (valori complessi...)
    std::function<void(int, int, double)> set;
    std::function<void(int, int)> erase;
    // v1.11.0, facoltativi:
    //  * setText: la cella riceve il testo digitato (espressioni, forme esatte,
    //    complessi) invece del numero valutato da evalNumber; editText da'
    //    il testo iniziale quando si modifica una cella con EXE;
    //  * subHeader / setSubHeader: riga SUB sotto le intestazioni (sottonomi
    //    delle liste della fx-CG50), raggiunta con ↑ dalla prima riga (row == -1);
    //  * scrollMarks: ◀ ▶ nell'intestazione quando ci sono colonne nascoste.
    std::function<void(int, int, const std::string&)> setText;
    std::function<std::string(int, int)> editText;
    std::function<std::string(int)> subHeader;
    std::function<void(int, const std::string&)> setSubHeader;
    bool scrollMarks{false};
    int row{0}, col{0};
    static constexpr int SubRow = -1;
    bool editing() const noexcept { return editing_; }
    bool key(Context& ctx, const KeyEvent& ev);
    void draw(Context& ctx, gfx::Canvas& c, int top, int bottom) const;
    int firstColumn() const noexcept { return firstCol_; }
private:
    bool editing_{false};
    LineEditor editor_;
    mutable int firstRow_{0}, firstCol_{0};
};

// Finestra per i file (v1.11.0 per i CSV; v1.15.0, F3: dialogo unico di tutti i
// file tipizzati): elenco dei file da aprire (LOAD/OPEN), nome da scrivere
// (SAVE·AS, con la regola dei nomi del tipo e la conferma di sovrascrittura),
// ricerca per prefisso (SEARCH della fx-CG50) e conferma Si'/No (DELETE).
// Con i valori predefiniti e' esattamente la finestra dei CSV della v1.11.0.
class FileDialog {
public:
    enum class Mode : std::uint8_t { None, Open, SaveAs, Confirm, Search };
    void openList(std::string title, std::vector<std::string> names);
    void openName(std::string title, const std::string& initial);
    // v1.15.0: domanda Si'/No (F1 o EXE: 1; F6, EXIT o AC: -1).
    void openConfirm(std::string title, std::string question);
    // v1.15.0: testo da cercare all'inizio dei nomi; EXE restituisce 1 con name() = testo.
    void openSearch(std::string title);
    void close() noexcept { mode_ = Mode::None; overwrite_ = false; }
    bool active() const noexcept { return mode_ != Mode::None; }
    Mode mode() const noexcept { return mode_; }
    // 1: scelta fatta (name()), -1: annullato, 0: ancora aperto.
    int key(const KeyEvent& ev);
    const std::string& name() const noexcept { return chosen_; }
    void draw(gfx::Canvas& c) const;
    // ---- v1.15.0: opzioni (restano finche' non vengono cambiate)
    void setExtension(std::string extension) { extension_ = std::move(extension); }   // mostrata dopo i nomi dell'elenco
    void setEmptyText(std::string text) { empty_ = std::move(text); }
    // Caratteri ammessi nel nome, maiuscole/minuscole e lunghezza (predefiniti: A-Z a-z 0-9 _ -, 8).
    void setNameRule(std::function<bool(char)> allowed, std::function<char(char)> caseMap, std::size_t maxLength);
    // Nomi gia' presenti: SAVE·AS di uno di questi chiede "Sovrascrivere?" prima di restituire 1.
    void setExisting(std::vector<std::string> names) { existing_ = std::move(names); }
    bool overwriting() const noexcept { return overwrite_; }  // il nome scelto esiste e l'utente ha confermato
    int selectedIndex() const noexcept { return selected_; }
    void select(int index) noexcept { selected_ = index; }
private:
    Mode mode_{Mode::None};
    std::string title_, chosen_, question_;
    std::vector<std::string> names_;
    int selected_{0};
    mutable int first_{0};
    LineEditor editor_{8};
    std::string extension_{".csv"};
    std::string empty_{"Nessun file CSV nella cartella"};
    std::function<bool(char)> allowed_;
    std::function<char(char)> caseMap_;
    std::size_t maxLength_{8};
    std::vector<std::string> existing_;
    bool overwrite_{false}, asking_{false};
};

} // namespace fx::app
