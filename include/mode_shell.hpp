#pragma once
// Guscio comune delle modalita' (v1.15.0, F4).
//
// I pezzi che ogni modalita' della fx-CG50 ripete, scritti una volta:
//  * ScreenStack: pila di schermate (EXIT torna alla precedente, la radice
//    torna al menu principale; le schermate "modulo" come SET e SET UP
//    tornano dove sono state aperte);
//  * FKeyBar: barra F1-F6 da tabella, con sottomenu (EXIT risale), pagine
//    (F6 "▶" passa alla pagina seguente) e la pagina di SHIFT;
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
#include "typed_files.hpp"
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
// Voci del CATALOG (lo stesso elenco di Run-Matrix, calc_library): etichetta e
// testo da inserire, filtrate per prefisso (vuoto: tutte).
std::vector<VarsItem> shellCatalog(const std::string& prefix = {});

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

#if FXAI_BOARD_PICO_PLUS_2W
// ---- v1.17.0 (solo Pimoroni Pico Plus 2 W): FILE delle modalita' Base.
// OPEN, SAVE·AS (con la conferma di sovrascrittura) e DELETE dei file
// tipizzati del tipo della modalita' sul FileDialog comune, nell'archivio
// scelto in SYSTEM (microSD o flash interna). Gli errori diventano MathError
// con il messaggio della fx-CG50 ("Data ERROR", "Memory ERROR").
class TypedFileMenu {
public:
    explicit TypedFileMenu(files::FileType type) noexcept : type_(type) {}
    enum class Purpose : std::uint8_t { None, Open, SaveAs, Delete };
    void openList(Context& ctx, Purpose purpose = Purpose::Open);   // elenco dei file del tipo
    void openSaveAs(Context& ctx, const std::string& currentStem);
    bool active() const noexcept { return dialog_.active(); }
    Purpose purpose() const noexcept { return purpose_; }
    // 1: scelta fatta (file() con l'estensione), -1: annullata, 0: ancora aperto.
    int key(const KeyEvent& ev);
    void draw(gfx::Canvas& c) const { dialog_.draw(c); }
    const std::string& file() const noexcept { return file_; }
    std::vector<std::uint8_t> load(Context& ctx, const std::string& file) const;
    void save(Context& ctx, const std::string& file, const std::vector<std::uint8_t>& payload) const;
    void remove(Context& ctx, const std::string& file) const;
    static std::string stem(const std::string& file);
private:
    files::FileType type_;
    FileDialog dialog_;
    Purpose purpose_{Purpose::None};
    std::string file_;
    std::vector<std::string> names_;
};
// Riquadro del titolo di una schermata delle modalita' Base (sotto la barra di stato).
void drawModeHeader(gfx::Canvas& c, const std::string& text);
// Campi dei moduli (Form) delle modalita' Base.
inline FormField numberField(std::string label, std::string text) { FormField f; f.label = std::move(label); f.text = std::move(text); return f; }
inline FormField textField(std::string label, std::string text) { FormField f = numberField(std::move(label), std::move(text)); f.type = FormField::Type::Text; return f; }
inline FormField choiceField(std::string label, std::vector<std::string> choices, int choice) {
    FormField f; f.label = std::move(label); f.type = FormField::Type::Choice; f.choices = std::move(choices); f.choice = choice;
    if (choice >= 0 && static_cast<std::size_t>(choice) < f.choices.size()) f.text = f.choices[static_cast<std::size_t>(choice)];
    return f;
}
// Numero con 10 cifre significative ("%.10G"), per i campi dei moduli.
std::string formNumber(double v);
#endif
} // namespace fx::app
