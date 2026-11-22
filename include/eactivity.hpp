#pragma once
// Nucleo di eActivity (v1.16.0, manuale fx-CG50 capitolo 10): documento,
// righe, strisce e codifica del file `.pceact`. Nessuna interfaccia: il
// controller e' src/apps/eactivity_app.cpp.
//
// Un documento e' un elenco di righe:
//  * testo: fino a 255 byte UTF-8 (10-7) con espressioni Math incorporate,
//    racchiuse tra IslandStart e IslandEnd (testo lineare canonico del
//    parser unico); un attributo per byte: colore del testo e marcatore;
//  * calcolo: espressione di Run-Matrix (testo lineare canonico) e risultato
//    (una HistoryEntry, la stessa di Run-Matrix);
//  * stop, immagine (file della cartella PICT con il bordo), striscia (tipo,
//    titolo di 16 caratteri e dati: le parti del workspace della sua modalita'
//    o il testo di Notes);
// piu' le memorie Mat/Vct/List del file (10-9). Le strisce di tipi
// sconosciuti si conservano byte per byte.
#include "app_registry.hpp"
#include "workspace.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fx::eact {

constexpr std::size_t MaxTextLineBytes = 255;       // riga di testo (10-7)
constexpr std::size_t MaxMemoBytes = 255;           // memo (10-12)
constexpr std::size_t MaxStripTitleBytes = 16;      // titolo di una striscia (10-18)
constexpr std::size_t MaxLines = 2000;
constexpr std::uint16_t FormatVersion = 1;
// Espressione Math in una riga di testo: U+E000 testo-lineare U+E001.
constexpr const char* IslandStart = "\xEE\x80\x80";
constexpr const char* IslandEnd = "\xEE\x80\x81";

enum class LineKind : std::uint8_t { Text = 0, Calc = 1, Stop = 2, Picture = 3, Strip = 4 };

// Tipi di striscia nell'ordine della tabella del manuale (10-17).
enum class StripKind : std::uint8_t {
    Run = 0, Graph, GraphEditor, TableEditor, ConicsGraph, ConicsEditor, StatGraph, ListEditor, Solver, RecurEditor, Notes,
    MatrixEditor, VectorEditor, SimulEquation, PolyEquation, DynamicGraph, Financial, SpreadSheet, EConTop, EConResult,
    Graph3D, Graph3DEditor, Geometry, PicturePlot, Count
};
constexpr std::size_t StripKindCount = static_cast<std::size_t>(StripKind::Count);
struct StripInfo {
    StripKind kind;
    const char* name;            // nome nel dialogo STRIP
    app::AppId app;              // modalita' che apre (Notes: eActivity stessa)
    std::uint8_t parts;          // bit (1 << part::X) delle parti del workspace conservate
};
const StripInfo& stripInfo(StripKind kind) noexcept;
// Il tipo si puo' aprire su questa scheda (modalita' disponibile; E-CON4 mai).
bool stripAvailable(StripKind kind) noexcept;
std::vector<std::uint8_t> stripParts(StripKind kind);   // elenco degli id di part::

// Colori dei testi (COLOR > CHAR) e dei marcatori (COLOR > MARKER), 10-10.
enum : std::uint8_t { TextDefault = 0, TextBlue, TextRed, TextMagenta, TextGreen, TextCyan, TextYellow, TextColorCount };
enum : std::uint8_t { MarkerNone = 0, MarkerMagenta, MarkerGreen, MarkerCyan, MarkerYellow, MarkerCount };
constexpr std::uint8_t attr(std::uint8_t color, std::uint8_t marker) noexcept { return static_cast<std::uint8_t>((color & 0x0F) | (marker << 4)); }
constexpr std::uint8_t attrColor(std::uint8_t a) noexcept { return a & 0x0F; }
constexpr std::uint8_t attrMarker(std::uint8_t a) noexcept { return static_cast<std::uint8_t>(a >> 4); }
// Colori del memo e del bordo delle immagini (Line Color, 10-16): 1..8, 0xFF = Clear.
constexpr std::uint8_t BorderClear = 0xFF;
enum : std::uint8_t { BorderNormal = 0, BorderThick = 1, BorderThin = 2 };

struct Memo {
    bool present{false};
    std::uint8_t color{0};       // 0..6 (colori dei testi)
    std::string text;
};

struct Line {
    LineKind kind{LineKind::Text};
    std::string text;                        // testo / espressione / file dell'immagine / titolo della striscia
    std::vector<std::uint8_t> attr;          // un byte per byte di `text` (righe di testo e di calcolo)
    Memo memo;
    std::shared_ptr<HistoryEntry> result;    // riga di calcolo: risultato (nullptr: non calcolata)
    std::uint8_t borderStyle{BorderNormal};
    std::uint8_t borderColor{BorderClear};   // immagine: bordo
    std::uint8_t strip{0};                   // tipo di striscia (anche sconosciuto)
    std::vector<std::uint8_t> data;          // striscia: parti del workspace o testo di Notes
    static Line textLine(std::string text = {});
    static Line calcLine(std::string expression = {});
    void setText(std::string t);             // anche gli attributi (predefiniti)
};

struct Document {
    std::vector<Line> lines;
    std::vector<std::uint8_t> memories;      // encodeParts(Matrices, Lists) del file; vuoto = memorie vuote
    std::size_t memoCount() const noexcept;
};

// Contenuto del file (dentro l'intestazione PCEA). decode() da' la garanzia
// forte; false per dati non validi o oltre i limiti.
std::vector<std::uint8_t> encode(const Document& doc);
bool decode(const std::uint8_t* data, std::size_t size, Document& out);
std::size_t encodedSize(const Document& doc);

// ---- testo con espressioni Math
// Fine del carattere o dell'espressione che inizia in `pos`.
std::size_t nextUnit(const std::string& text, std::size_t pos) noexcept;
std::size_t prevUnit(const std::string& text, std::size_t pos) noexcept;
// Espressione che inizia in `pos` (IslandStart): true, testo lineare in `expr`, fine in `end`.
bool islandAt(const std::string& text, std::size_t pos, std::string& expr, std::size_t& end);
std::string island(const std::string& expr);
bool validText(const std::string& text) noexcept;   // isole chiuse, niente isole annidate, UTF-8

// Righe di testo in pezzi da disegnare: testo semplice o espressione; a capo
// solo tra le parole del testo (le espressioni non vanno a capo, 10-7).
struct Piece {
    std::size_t from{0}, to{0};      // byte di `text`
    bool island{false};
    int width{0};
};
// `measure(text, from, to, island)` da' la larghezza in pixel di un pezzo.
std::vector<std::vector<Piece>> wrap(const std::string& text, int width,
                                     int (*measure)(const std::string& text, std::size_t from, std::size_t to, bool island, void* user),
                                     void* user);

// Ricerca dei documenti (10-5) e nomi validi (8 caratteri A-Z 0-9 { } ' ~).
bool validName(const std::string& name);

} // namespace fx::eact
