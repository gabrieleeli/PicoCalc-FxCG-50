#pragma once
// Guscio comune delle modalita' (v1.15.0, F4).
//
// I pezzi che ogni modalita' della fx-CG50 ripete, scritti una volta:
//  * ScreenStack: pila di schermate (EXIT torna alla precedente, la radice
//    torna al menu principale; le schermate "modulo" come SET e SET UP
//    tornano dove sono state aperte);
//  * FKeyBar: barra F1-F6 da tabella, con sottomenu (EXIT risale), pagine
//    (F6 "▷" passa alla pagina seguente) e la pagina di SHIFT;
//  * SelectList: lista di voci selezionabili con scorrimento;
//  * ConfirmBox ("Eliminare...? F1/EXE Si', F6/EXIT No") e AskBox (richiesta
//    di un valore con l'editor Math), disegnati come in TABLE dalla v1.8.0;
//  * SetupTable: voci di SET UP della modalita' da tabella (etichetta,
//    scelte, lettura e scrittura nel workspace) sul Form comune;
//  * menu VARS costruito dai resolver del workspace (NameResolver,
//    CollectionResolver): le modalita' non hanno elenchi di variabili propri.
// ModeShell raccoglie i modali e li instrada nell'ordine conferma, richiesta,
// menu. TABLE e' la prima modalita' portata sul guscio (screenshot identici).
#include "app_widgets.hpp"
#include "natural_editor.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace fx::app {

class ScreenStack {
public:
    static constexpr std::size_t MaxDepth = 8;
    void reset(int root) noexcept { items_[0] = root; depth_ = 1; }
    void push(int screen) noexcept;              // oltre MaxDepth sostituisce la cima
    void replace(int screen) noexcept { items_[depth_ - 1] = screen; }
    bool pop() noexcept;                         // false alla radice (nulla da togliere)
    int top() const noexcept { return items_[depth_ - 1]; }
    int below() const noexcept { return depth_ > 1 ? items_[depth_ - 2] : items_[0]; }
    int root() const noexcept { return items_[0]; }
    std::size_t depth() const noexcept { return depth_; }
private:
    std::array<int, MaxDepth> items_{};
    std::size_t depth_{1};
};

// Una voce della barra F1-F6. action >= 0 e' restituita da FKeyBar::key();
// submenu apre una barra figlia (EXIT torna a quella di prima).
struct FKeyMenu;
struct FKey {
    const char* label{};
    int action{-1};
    const FKeyMenu* submenu{};
};
struct FKeyPage { FKey keys[6]; };
struct FKeyMenu {
    const FKeyPage* pages{};
    std::size_t count{0};                        // pagine; con piu' di una F6 e' "▷"
    const FKeyPage* shiftPage{};                 // tasti con SHIFT (nullptr: nessuno)
};
class FKeyBar {
public:
    static constexpr int None = -1;              // tasto non usato dalla barra
    static constexpr int Handled = -2;           // pagina o sottomenu cambiati
    static constexpr std::size_t MaxDepth = 4;
    void set(const FKeyMenu* menu) noexcept;      // barra radice; azzera pagina e sottomenu
    // F1-F6 (e EXIT dentro un sottomenu): azione scelta (>= 0), Handled o None.
    int key(const KeyEvent& ev, bool shift);
    void back() noexcept;                         // risale di un sottomenu
    bool inSubmenu() const noexcept { return depth_ > 1; }
    std::size_t page() const noexcept { return page_; }
    const FKeyPage* current(bool shift) const noexcept;
    void labels(bool shift, const char* out[6]) const noexcept;
    void draw(gfx::Canvas& c, bool shift) const;
private:
    std::array<const FKeyMenu*, MaxDepth> stack_{};
    std::size_t depth_{0}, page_{0};
};

class SelectList {
public:
    std::vector<std::string> items;
    int selected{0};
    // Up/Down (circolari), PageUp/PageDown, Home/End: true se il tasto e' stato usato.
    bool key(const KeyEvent& ev, int pageRows = 6);
    void clampSelection() noexcept;
    void draw(gfx::Canvas& c, gfx::Rect area, int rowH) const;
    int first() const noexcept { return first_; }
private:
    mutable int first_{0};
};

class ConfirmBox {
public:
    void open(std::string text, int tag = 0) { text_ = std::move(text); tag_ = tag; open_ = true; }
    void close() noexcept { open_ = false; }
    bool active() const noexcept { return open_; }
    int tag() const noexcept { return tag_; }
    const std::string& text() const noexcept { return text_; }
    // 1: Si' (F1/EXE), 0: No (F6/EXIT), -1: ancora aperta (il tasto e' ignorato).
    int key(const KeyEvent& ev);
    void draw(gfx::Canvas& c) const;
private:
    std::string text_;
    int tag_{0};
    bool open_{false};
};

class AskBox {
public:
    void open(std::string prompt, int tag = 0, char first = 0);
    void close() noexcept { open_ = false; }
    bool active() const noexcept { return open_; }
    int tag() const noexcept { return tag_; }
    NaturalEditor& input() noexcept { return input_; }
    const std::string& prompt() const noexcept { return prompt_; }
    // Testo lineare digitato ("" se vuoto).
    std::string text() const { return input_.empty() ? std::string() : input_.toExpression(); }
    // 1: EXE (la richiesta si chiude), 0: EXIT (chiusa), -1: tasto di modifica.
    int key(const KeyEvent& ev);
    void draw(gfx::Canvas& c) const;
private:
    NaturalEditor input_;
    std::string prompt_;
    int tag_{0};
    bool open_{false};
};

// SET UP della modalita' da tabella (scelte del Form comune).
struct SetupItem {
    const char* label;
    std::vector<std::string> choices;
    std::function<int(const Context&)> get;
    std::function<void(Context&, int)> set;
};
void fillSetup(const Context& ctx, const std::vector<SetupItem>& items, Form& form);
void applySetup(Context& ctx, const std::vector<SetupItem>& items, const Form& form);
// Schermata di un modulo (SET, SET UP): intestazione, campi, tasti.
void drawFormScreen(gfx::Canvas& c, const std::string& title, const Form& form, int height, const char* const keys[6]);

// Voci di VARS dai resolver del workspace: etichetta e testo da inserire
// (variabili di sistema di NameResolver, liste e matrici definite di
// CollectionResolver). Nessun elenco di variabili proprio delle modalita'.
struct VarsItem { std::string label, insert; };
std::vector<VarsItem> varsItems(const Context& ctx);

// I modali di una modalita', nell'ordine in cui ricevono i tasti.
struct ModeShell {
    ScreenStack screens;
    FKeyBar fkeys;
    ConfirmBox confirm;
    AskBox ask;
    PopupMenu menu;
    int menuTag{0};
    void closeModals() noexcept { confirm.close(); ask.close(); menu.close(); menuTag = 0; }
    bool modalOpen() const noexcept { return confirm.active() || ask.active() || menu.open; }
    void showMenu(const char* title, std::vector<std::string> items, int tag) { menu.show(title, std::move(items)); menuTag = tag; }
    // Instrada il tasto al modale aperto: onConfirm(tag, si'), onAnswer(tag),
    // onMenu(tag, scelta). True se un modale ha usato il tasto.
    bool modalKey(const KeyEvent& ev, const std::function<void(int, bool)>& onConfirm, const std::function<void(int)>& onAnswer,
                  const std::function<void(int, int)>& onMenu);
    void drawModals(gfx::Canvas& c) const;      // menu, richiesta, conferma
};

} // namespace fx::app
