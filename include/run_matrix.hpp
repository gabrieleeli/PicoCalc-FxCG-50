#pragma once
// Run-Matrix calculation core (no UI): evaluation of one input line, history
// with in-place editing and recalculation, and the result views (exact form,
// decimal, mixed/improper fraction, repeating decimal, Base-N, DMS).
#include "expression_ast.hpp"
#include "workspace.hpp"
#include <string>

namespace fx {

// Result of one RUN-MATRIX calculation.
struct RunOutcome {
    Complex value{};
    char storedTo{};            // first variable letter for "expr->A" / "expr->A~F", 0 otherwise
    char storedLast{};          // last letter of an A~F range (== storedTo for one variable)
    std::string pairText;       // "r=5  θ=53.13" for Pol(), "x=..  y=.." for Rec()
    bool exact{};               // the value has an exact form (fraction, surd, multiple of pi)
    ValueType type{ValueType::Scalar};   // List, Matrix o Vector per un risultato collezione
    std::string storedIn;       // "Mat A", "Vct B", "List 1", "Mat A[1,2]"... quando la freccia indica una memoria
    bool graphics{false};       // comando grafico eseguito: va mostrata la schermata del grafico (GRAPH)
};

// Opzioni di evaluateEntry (v1.11.0): base della modalita' Base-N (-1: quella
// del SET UP; per il ricalcolo, quella con cui la riga e' stata calcolata) e
// numero di righe della cronologia che precedono la riga (per ▶Simp su Ans).
struct EntryOptions {
    int base{-1};
    std::size_t previous{static_cast<std::size_t>(-1)};
};
// Base numerica di Settings::BaseMode: 0 (Comp), 10, 16, 2, 8.
unsigned integerBaseOf(std::uint8_t baseMode) noexcept;

// Valuta un'espressione lineare esattamente come la modalita' RUN-MATRIX in
// `entry`: freccia di memorizzazione (U+2192 o "->"), coppie Pol/Rec,
// valutazione esatta accanto a quella in virgola mobile, aggiornamento di
// Ans/PreAns. Il risultato puo' essere un numero, una lista, una matrice o un
// vettore; liste e matrici aggiornano ListAns / MatAns / VctAns invece di Ans.
// Destinazioni dopo la freccia:
//   A   A~F   Mat A   Vct A   List 1   Mat A[1,2]   Vct A[2]   List 1[3]
//   Dim Mat A  ({righe,colonne} crea una matrice di zeri)
// Lancia errori Syntax/Math (la voce resta invariata); il loro membro `where`
// e' l'intervallo di byte della causa in `expression`.
//
// v1.11.0: modalita' Base-N intera (Settings::baseMode), "▶Simp" e "▶Simp n"
// in coda (frazione ridotta per il piu' piccolo fattore primo comune o per n,
// risultato con "F=n"), frazioni non ridotte con Simplify Manual, comandi
// grafici (Plot, F-Line, Graph Y=..., vedi graph_commands.hpp: la riga mostra
// "Done" e RunOutcome::graphics chiede la schermata del grafico).
//
// v1.12.0: piu' istruzioni separate da ":" (si mostra l'ultimo risultato) o
// da "◢" (il risultato prima di ◢ si mostra in HistoryEntry::displays);
// destinazioni r, θ, Str n, f1-f20 (si memorizza il testo dell'espressione),
// le variabili di sistema (Xmin, Xfct, TblStart, D Start, R Start, a0, n,
// I%...), "n→Dim List k", List "QTY"; stringhe ("ABC", Str 1) come risultato;
// Fill(v, Mat A / List n) che modifica la memoria; i comandi delle righe
// Swap, *Row, *Row+, Row+ sulle matrici; Simplify Manual con le regole di
// evaluateUnreducedFraction().
RunOutcome evaluateEntry(Workspace& ws, const std::string& expression, HistoryEntry& entry, const EntryOptions& options = {});
// evaluateEntry() + append to the workspace history.
RunOutcome runCalculation(Workspace& ws, const std::string& expression);

// fx-CG50 history editing: replaces the expression of history[index] and
// recalculates it and every later entry in order (Ans chains and stores are
// replayed). If the edited entry itself fails, history and memory are
// restored and the error is thrown; later entries that fail keep their error
// text. Returns the number of later entries that failed.
std::size_t editHistoryEntry(Workspace& ws, std::size_t index, const std::string& expression);
// Ricalcola history[from..] con Ans/PreAns presi dalle voci precedenti.
// firstError (facoltativo) riceve la posizione dell'errore di history[from].
std::size_t recalculateHistory(Workspace& ws, std::size_t from, ErrorLocation* firstError = nullptr);

// Result views stored in HistoryEntry::view.
enum ResultView : std::uint8_t {
    ViewAuto = 0,       // exact form when the fx-CG50 would show it, otherwise decimal
    ViewDecimal = 1,
    ViewHex = 2, ViewBin = 3, ViewOct = 4,
    ViewDms = 5,
    ViewExact = 6,      // exact form requested with S<->D (also for decimal input)
    ViewPeriodic = 7,   // repeating decimal of a rational: 0.1(6)
    // v1.12.0
    ViewEng = 8,        // ENG / ENG←: notazione ingegneristica con HistoryEntry::engShift
    ViewPolar = 9,      // ►r∠θ di un risultato complesso
    ViewRect = 10,      // ►a+bi di un risultato complesso
};
// ENG (+1) / ENG← (-1) sul risultato (OPTN › ESYM): l'esponente cambia di 3.
// False quando il risultato non e' un numero reale.
bool shiftEngineering(HistoryEntry& entry, int direction);
// ►r∠θ / ►a+bi: vista polare o rettangolare di un risultato complesso.
bool setComplexView(HistoryEntry& entry, bool polar);

// True when the entry currently shows its exact form (fraction, surd, pi).
bool showsExact(const HistoryEntry& entry, const Settings& settings);
// True when an exact fraction is shown in mixed form (2 1/3).
bool showsMixed(const HistoryEntry& entry, const Settings& settings);
// True when the decimal shown is the result of a numerical method (shown with ≈).
bool isApproximation(const HistoryEntry& entry, const Settings& settings);

// fx-CG50 S<->D: exact form <-> decimal. Returns false when the value has no
// other form (irrational or numerical result, integer, error).
bool toggleExactDecimal(HistoryEntry& entry, const Settings& settings);
// a b/c <-> d/c on an exact fraction result. False when no fraction is shown.
bool toggleMixedFraction(HistoryEntry& entry, const Settings& settings);
// Repeating-decimal view of an exact rational (toggles back to the automatic view).
bool togglePeriodic(HistoryEntry& entry);

// Testo lineare di un risultato nella vista corrente ("1/3", "2 1/3", "2√3/3",
// "π/2", "0.3333333333", "0.1(6)", "0x1F", "30°15'0\"", "[[1,2][3,4]]",
// "{1,2,3}", Base-N "FF", ▶Simp "5/20  F=3", comando "Done").
// lineStyle: testo dell'output Line della fx-CG50, con le frazioni scritte
// con ⌟ ("1⌟3", "2⌟1⌟3").
std::string resultText(const HistoryEntry& entry, const Settings& settings, bool lineStyle = false);
// Base-N: testo di un intero a 32 bit nella base 2, 8, 10 o 16 (complemento a
// due per i negativi in 2, 8, 16: -1 -> "FFFFFFFF").
std::string baseNText(double value, unsigned base);
// Base mostrata per una riga calcolata in Base-N (0 se la riga non e' Base-N).
unsigned displayedBase(const HistoryEntry& entry);
// Vero quando il risultato e' una lista, una matrice o un vettore.
inline bool isCollection(const HistoryEntry& entry) noexcept { return entry.collection != nullptr; }
// Testo di un elemento di un risultato collezione nella vista corrente.
std::string elementText(const HistoryEntry& entry, std::size_t index, const Settings& settings);
// 2D tree used to draw the result (stacked fraction, radicals, ×10^n, overline).
ast::NodePtr resultTree(const HistoryEntry& entry, const Settings& settings);
// Modelli 2D condivisi con EQUATION (v1.12.0): gli stessi alberi dei
// risultati di Run-Matrix, disegnati da math_renderer.
//  * exactValueTree: forma esatta (frazione sovrapposta, mista, radicali, π);
//  * appendNumberTree: un decimale nel formato del display (mantissa e ×10^n,
//    simbolo Eng);
//  * appendTextTokens: testo come token (un carattere UTF-8 ciascuno);
//  * exactLineText: forma esatta dell'output Line (frazioni con ⌟).
ast::NodePtr exactValueTree(const exact::Value& value, bool mixed);
void appendNumberTree(ast::Node& row, double value, DisplayFormat format);
void appendTextTokens(ast::Node& row, const std::string& text);
std::string exactLineText(const exact::Value& value, bool mixed);
// Expression that reproduces the stored value exactly when possible (exact
// form, else 15 significant digits); used to reuse a result in a new input.
std::string resultExpression(const HistoryEntry& entry);
} // namespace fx
