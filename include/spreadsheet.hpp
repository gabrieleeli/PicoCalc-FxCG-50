#pragma once
// Nucleo di Spreadsheet (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
//
// Foglio A..Z x 999 sparso: ogni cella ha il testo digitato (numero, testo o
// formula che inizia con '=') e, per le formule, il valore calcolato o
// l'errore. Le formule sono espressioni della calcolatrice lette dal lexer
// unico con ParseOptions::cellReferences (A1, $A$1, A1:B5) e valutate con
// bind::BindingResolver: un solo parser, un solo valutatore, le variabili A-Z
// e le liste del workspace restano disponibili. Il ricalcolo ordina le celle
// con l'algoritmo di Kahn (nessuna ricorsione); le celle di un ciclo hanno
// "Circular ERROR". Il ricalcolo e' un step::Task: la modalita' lo esegue a
// passi e AC lo interrompe. Nessuna interfaccia qui.
#include "expression.hpp"
#include "step_task.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fx::sheet {

constexpr int Columns = 26;
constexpr int Rows = 999;
constexpr std::size_t MaxInput = 255;             // caratteri di una cella (fx-CG50: 255)
constexpr const char* CircularError = "Circular ERROR";

struct Ref {
    int col{0};                                    // 0..25 = A..Z
    int row{1};                                    // 1..999
    bool operator==(const Ref& o) const noexcept { return col == o.col && row == o.row; }
    bool valid() const noexcept { return col >= 0 && col < Columns && row >= 1 && row <= Rows; }
};
std::string refName(Ref r);                        // {1, 12} -> "B12"
bool parseRef(const std::string& text, Ref& out) noexcept;   // "B12", "$b$12"
inline std::uint32_t key(Ref r) noexcept { return static_cast<std::uint32_t>(r.row) * 32u + static_cast<std::uint32_t>(r.col); }
inline Ref refOf(std::uint32_t k) noexcept { return Ref{static_cast<int>(k % 32u), static_cast<int>(k / 32u)}; }

enum class Kind : std::uint8_t { Empty = 0, Number = 1, Text = 2, Formula = 3 };
struct Cell {
    std::string input;                             // testo digitato ("12", "Ciao", "=A1*2")
    Kind kind{Kind::Empty};
    double value{0};                               // numero o risultato della formula
    std::string error;                             // errore della formula ("" = nessuno)
    bool stale{false};                             // formula non ancora ricalcolata (ricalcolo interrotto)
    bool numeric() const noexcept { return kind == Kind::Number || (kind == Kind::Formula && error.empty() && !stale); }
};
// Tipo del testo digitato: '=' formula, numero (lo stesso parser della
// calcolatrice per le costanti: "1.5E3", "-2"), altrimenti testo.
Kind classify(const std::string& input, double& number);

// Blocco di celle (estremi compresi, in qualsiasi ordine).
struct Block {
    Ref a, b;
    int c0() const noexcept { return a.col < b.col ? a.col : b.col; }
    int c1() const noexcept { return a.col < b.col ? b.col : a.col; }
    int r0() const noexcept { return a.row < b.row ? a.row : b.row; }
    int r1() const noexcept { return a.row < b.row ? b.row : a.row; }
    bool contains(Ref r) const noexcept { return r.col >= c0() && r.col <= c1() && r.row >= r0() && r.row <= r1(); }
};

class Sheet {
public:
    // Celle non vuote al piu' (26 000 con la PSRAM, 3 000 solo SRAM): oltre "Memory ERROR".
    static std::size_t maxCells() noexcept;
    // Scrive una cella (testo vuoto = cancella). Lancia MathError("Memory ERROR")
    // oltre maxCells() o MaxInput senza cambiare nulla. Le formule si
    // controllano subito (Syntax ERROR) e restano da ricalcolare.
    void set(Ref r, const std::string& input);
    void clear(Ref r) noexcept;
    void clearAll() noexcept { cells_.clear(); }
    const Cell* cell(Ref r) const noexcept;
    Cell* cell(Ref r) noexcept;
    std::size_t size() const noexcept { return cells_.size(); }
    bool empty() const noexcept { return cells_.empty(); }
    const std::map<std::uint32_t, Cell>& cells() const noexcept { return cells_; }
    // Valore numerico per le formule e per i riferimenti (false: vuota, testo o errore).
    bool numberAt(Ref r, double& out) const noexcept;
    // Testo mostrato nella griglia ("" vuota, numero formattato dal chiamante).
    bool operator==(const Sheet& o) const;
private:
    std::map<std::uint32_t, Cell> cells_;
};

// Riferimenti relativi spostati di (dCol, dRow); quelli con $ restano. false se
// un riferimento esce dal foglio (la cella diventa "Range ERROR").
bool shiftFormula(const std::string& formula, int dCol, int dRow, std::string& out);
// Celle e intervalli citati da una formula (per l'ordine del ricalcolo).
std::vector<Block> formulaReferences(const std::string& formula);

// Ricalcolo completo a passi (un'unita' = una formula valutata).
class Recalc final : public step::Task {
public:
    Recalc(Sheet& sheet, EvalContext base);
    bool step(step::Budget& budget) override;
    std::size_t total() const override { return order_.size(); }
    std::size_t done() const override { return next_; }
    void cancelled() override;                     // le formule non valutate restano "stale"
    void runAll();                                 // senza budget
private:
    void evaluate(std::uint32_t k);
    Sheet& sheet_;
    EvalContext base_;
    std::vector<std::uint32_t> order_;
    std::size_t next_{0};
};
// Ricalcolo senza passi (test, piccoli fogli).
void recalculate(Sheet& sheet, const EvalContext& base);

// CUT / COPY / PASTE / FILL (manuale 9-11...9-13).
struct Clipboard {
    bool valid{false};
    bool cut{false};
    Block source{};
    std::vector<std::pair<Ref, std::string>> items;   // posizione relativa all'angolo in alto a sinistra
};
void copyBlock(const Sheet& sheet, Block block, bool cut, Clipboard& out);
// Incolla con l'angolo in `at`; le formule spostano i riferimenti relativi. Dopo
// un CUT le celle d'origine si svuotano e gli appunti si chiudono.
void paste(Sheet& sheet, Clipboard& clip, Ref at);
// FILL: lo stesso testo in tutte le celle del blocco; una formula e' scritta per
// la prima cella e i riferimenti relativi si spostano per le altre.
void fill(Sheet& sheet, Block block, const std::string& input);
void clearBlock(Sheet& sheet, Block block) noexcept;

// Colonna <-> lista (manuale 9-20: memoria delle celle verso List e viceversa).
std::vector<double> columnValues(const Sheet& sheet, int col, int r0, int r1);
void setColumn(Sheet& sheet, int col, int r0, const std::vector<double>& values);

// Stato della modalita' nel workspace (sezione 0x0103 del record).
struct State {
    Sheet sheet;
    Ref cursor{0, 1};
    int top{1}, left{0};                           // prima riga e colonna visibili
    std::string file;                              // file .pcsheet aperto ("" = nessuno)
    bool autoCalc{true};
    bool operator==(const State& o) const { return sheet == o.sheet && cursor == o.cursor && top == o.top && left == o.left && file == o.file && autoCalc == o.autoCalc; }
};
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
// false (stato invariato) se i dati non sono validi.
bool decode(State& s, const std::uint8_t* data, std::size_t size);
// File .pcsheet: solo le celle (versione 1).
std::vector<std::uint8_t> encodeFile(const Sheet& sheet);
bool decodeFile(const std::uint8_t* data, std::size_t size, Sheet& out);

} // namespace fx::sheet
