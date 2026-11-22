#pragma once
#include "app_models.hpp"
#include "equation.hpp"
#include "expression.hpp"
#include "graph_model.hpp"
#include "graph3d_model.hpp"
#include "math_engine.hpp"
#include "memory.hpp"
#include "number_format.hpp"
#include "value.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fx {

struct Settings {
    AngleMode angle{AngleMode::Radians};      // fx-CG50 factory default
    DisplayFormat display{};
    ComplexMode complex{ComplexMode::Real};
    bool fractionResults{true};          // natural output: show exact forms (fractions, surds, pi) automatically
    bool mixedFractions{false};          // fx-CG50 "Frac Result": false = d/c (improper), true = ab/c (mixed)
    std::uint8_t lcdBacklight{160};      // STM32 REG_ID_BKL (0..255)
    std::uint8_t kbdBacklight{0};        // STM32 REG_ID_BK2 (0..255)
    std::string wifiSsid;
    std::string wifiPassword;
    std::string relayHost;               // HTTPS relay for AI chat (no provider keys on device)
    std::uint16_t relayPort{443};
    std::string relayPath{"/v1/chat"};
    std::string relayToken;              // per-device token issued by the relay
    std::uint8_t aiProvider{0};          // 0 = Claude (Anthropic), 1 = ChatGPT (OpenAI)
    std::string aiModel;                 // empty = relay default
    std::uint16_t aiMaxChars{1200};           // reply length requested from the relay (200..4000)
    // Where GRAPH projects and pictures are saved (record v5): the microSD card
    // or LittleFS on the internal flash (about 11 MB on the Pico Plus 2 W).
    enum class FileStore : std::uint8_t { MicroSd = 0, Internal = 1 };
    FileStore fileStore{FileStore::MicroSd};
    // Setup "Input/Output" della fx-CG50 (record v6): Math = input e risultati in
    // forma naturale (frazioni, radici, potenze, matrici come sul libro),
    // Line = input e risultati su una riga.
    enum class InputMode : std::uint8_t { Math = 0, Line = 1 };
    InputMode inputMode{InputMode::Math};
    // ---- v1.11.0 (record v7)
    // SET UP "Mode" della fx-CG50 in Run-Matrix: Comp (calcolo normale) o Base-N
    // intera a 32 bit in complemento a due (Dec, Hex, Bin, Oct).
    enum class BaseMode : std::uint8_t { Comp = 0, Dec = 1, Hex = 2, Bin = 3, Oct = 4 };
    BaseMode baseMode{BaseMode::Comp};
    // SET UP "Simplify": Auto riduce le frazioni, Manual lascia la frazione come
    // e' stata scritta (la si riduce con ▶Simp).
    bool simplifyManual{false};
    // Formato dei file CSV (fx-CG50 CSV > SET): separatore ',' o ';', simbolo decimale '.' o ','.
    char csvSeparator{','};
    char csvDecimal{'.'};
    // Liste usate da Statistics (SET: 1Var/2Var XList, YList), indici 0..25.
    std::uint8_t statXList{0};
    std::uint8_t statYList{1};
    // ---- v1.12.0 (record v8)
    // SET UP "List File": file di liste attivo (0..5 = File 1..6).
    std::uint8_t listFile{0};
    // SET UP "Sub Name": l'editor delle liste mostra la riga dei sottonomi.
    bool subName{true};
    // Tipo dell'ultima regressione calcolata in Statistics (stats::RegType),
    // per x̂, ŷ e VARS STAT a..e, r, r²; 0xFF = nessuna.
    std::uint8_t statRegType{0xFF};
};

// Un file di liste (fx-CG50 List File 1-6): 26 liste con i sottonomi.
struct ListFileData {
    std::array<std::vector<double>, 26> lists{};
    std::array<std::string, 26> names{};
    std::size_t elements() const noexcept { std::size_t n = 0; for (const auto& l : lists) n += l.size(); return n; }
};

// Variabili delle modalita' Dyna Graph, Recursion e Financial che VARS rende
// disponibili in Run-Matrix (record v8): si leggono e si assegnano con ->.
struct ModeVariables {
    double dynaStart{1}, dynaEnd{5}, dynaPitch{1};
    double recStart{0}, recEnd{10};
    std::array<double, 9> recInitial{};          // a0 a1 a2 b0 b1 b2 c0 c1 c2
    std::array<double, 3> recNStart{};           // anStart bnStart cnStart
    double tvmN{0}, tvmI{0}, tvmPV{0}, tvmPMT{0}, tvmFV{0}, tvmPY{12}, tvmCY{12};
    // Formula della successione an+1 = f(n, an) (variabili rec_n, rec_an), assegnata
    // con "formula"->an+1 come nei programmi della fx-CG50; usata da DrawR e DrawWeb.
    std::string recFormula;
};

// Risultato di un calcolo che e' una lista, una matrice o un vettore. I
// risultati piu' grandi della vista naturale (limits::kNaturalMatrixDim,
// kNaturalListLength) conservano qui solo tipo e dimensioni: gli elementi sono
// nella memoria delle risposte.
struct CollectionResult {
    CalcValue value;
    std::vector<exact::Value> exact;     // uno per elemento quando ogni elemento ha una forma esatta, altrimenti vuoto
    bool decimal{false};                 // le forme esatte vengono da numeri decimali
};

struct HistoryEntry {
    std::string expression;              // canonical linear text (editor round-trip)
    Complex value{};
    std::string error;                   // non-empty for failed evaluations ('=' prefix: Pol/Rec pair text)
    std::uint8_t view{0};                // ResultView (run_matrix.hpp)
    ExactSlot exact;                     // exact form of the value, when one exists
    bool numerical{false};               // produced by a numerical method (integral, d/dx, SOLVE)
    bool flipMixed{false};               // this result shows the other fraction format (ab/c <-> d/c)
    std::shared_ptr<const CollectionResult> collection;   // risultato lista / matrice / vettore (value e' allora 0)
    // ---- v1.11.0 (record v7)
    std::uint8_t base{0};                // Settings::BaseMode del calcolo (0 = Comp): Base-N intero a 32 bit
    // Frazione non ridotta da mostrare (Simplify Manual, ▶Simp): fracDen == 0 se assente.
    std::int64_t fracNum{0}, fracDen{0};
    std::uint32_t simpFactor{0};         // fattore usato da ▶Simp (mostrato "F=n"), 0 = nessuno
    bool command{false};                 // comando grafico (Plot, F-Line, Graph Y=...): il risultato e' "Done"
    // ---- v1.12.0 (record v8)
    std::int8_t engShift{0};             // ENG / ENG←: spostamento dell'esponente ingegneristico (passi di 3)
    std::vector<std::string> displays;   // risultati intermedi mostrati da ◢ in una riga a piu' istruzioni
    std::string text;                    // risultato stringa ("ABC", Str 1) mostrato com'e'
};

// Mat A-Z, Vct A-Z e le memorie delle risposte MatAns, VctAns, ListAns (record
// v6). Un CalcValue vuoto e' una memoria non definita ("None" nell'editor).
// Condivise dai calcoli di Run-Matrix e dall'editor MAT/VCT; stanno nel
// workspace, quindi i valori restano cambiando applicazione e spegnendo.
struct CollectionMemory {
    static constexpr std::size_t Slots = 26;
    std::vector<CalcValue> mat, vct;
    CalcValue matAns, vctAns, listAns;
    CollectionMemory() : mat(Slots), vct(Slots) {}
    void clear();
    // Elementi di Mat, Vct, MatAns e VctAns insieme (limits::kMatrixStoreElements).
    std::size_t elements() const noexcept;
};

// Graph functions, V-Window and the other GRAPH data: graph_model.hpp.

struct TableRange {
    double start{-3}, end{3}, step{1};
    // TABLE -> GRAPH request (not saved): draw the table points as a connected
    // (GPH-CON) or plot-type (GPH-PLT) graph; GRAPH clears it on leaving.
    enum class Draw : std::uint8_t { None = 0, Connect = 1, Plot = 2 };
    Draw draw{Draw::None};
    // Variable of the table (Setup "Variable", manual 5-32): the table range or a list.
    enum class Variable : std::uint8_t { Range = 0, List = 1 };
    Variable variable{Variable::Range};
    std::uint8_t list{0};                    // List 1..6 (0-based) when variable == List
    GraphType funcType{GraphType::Y};        // Table Func: Y=, r= or Param, one type per table (manual 5-34)
    std::vector<double> rows;                // the generated table: values of x / θ / T, editable (ROW, EDIT)
    bool generated{false};                   // false: TABLE shows the relation list (no table)
};

// Dati della modalita' EQUATION (record v8). I coefficienti sono numeri con la
// loro forma esatta (eq::Number): SIML in una griglia fissa 6 x 7 (riga r,
// colonna c all'indice r*7+c: cambiando il numero di incognite i valori
// restano al loro posto), POLY e le disequazioni dal grado piu' alto.
struct EquationState {
    static constexpr std::size_t MaxUnknowns = limits::kEquationUnknowns;
    static constexpr std::size_t MaxDegree = limits::kEquationDegree;
    static constexpr std::size_t SimStride = MaxUnknowns + 1;
    static constexpr double DefaultLower = -9e99, DefaultUpper = 9e99;   // Lower / Upper (manuale 4-5)
    std::uint8_t unknowns{2};
    std::vector<eq::Number> simultaneous;     // MaxUnknowns x SimStride
    std::uint8_t degree{2};
    std::vector<eq::Number> polynomial;       // MaxDegree + 1, dal grado piu' alto
    std::string solverExpr;                   // equazione del Solver (testo lineare canonico)
    double solverLower{DefaultLower}, solverUpper{DefaultUpper};
    char solverVariable{0};                   // ultima variabile risolta ('A'..'Z', 0 = nessuna)
    std::uint8_t ineqDegree{2};
    std::uint8_t ineqRelation{0};             // 0 >, 1 <, 2 >=, 3 <=  (estensione)
    std::vector<eq::Number> inequality;       // MaxDegree + 1
    // Ultime soluzioni (VARS › EQUATION: SimRes, PlyRes); vuote: nessuna.
    CalcValue simResult, polyResult;
    EquationState();
    eq::Number& sim(std::size_t r, std::size_t c) { return simultaneous[r * SimStride + c]; }
    const eq::Number& sim(std::size_t r, std::size_t c) const { return simultaneous[r * SimStride + c]; }
    // Coefficienti attivi per righe: unknowns x (unknowns + 1).
    std::vector<eq::Number> simCoefficients() const;
    std::vector<eq::Number> polyCoefficients() const;     // degree + 1
};
struct ChatMessage { bool fromUser{}; std::string text; };

// 3D Graph mode (record v10, v1.14.0; manual chapter 16): the three graph
// functions with selection and colours, the 3D View Window (with the view
// angles θ and φ), the Setup items, the zoom factor of ZOOM IN/OUT and the
// memories 3D G-Mem 1-20 and 3D V-Win 1-6. Records v5-v9 (surfaces Z1-Z3)
// are converted when read.
struct Graph3DState {
    g3d::GraphSet set{};
    float zoom{1.0F};
    std::array<std::string, g3d::GraphMemories> gmem{};       // encoded GraphSet (g3d::encode), empty = free
    std::array<g3d::Window, g3d::WindowMemories> vmem{};
    std::uint8_t vmemUsed{0};                                 // bit k: 3D V-Win k+1 stored
    std::size_t memoryBytes() const noexcept { std::size_t n = 0; for (const auto& m : gmem) n += m.size(); return n; }
    // Back to the initial state in place (no ~2 KB temporary on the stack).
    void reset() {
        for (std::size_t i = 0; i < g3d::Functions; ++i) set.fns[i] = g3d::emptyFunction(i);
        set.window = g3d::Window{};
        set.setup = g3d::Setup{};
        zoom = 1.0F;
        for (auto& m : gmem) { m.clear(); m.shrink_to_fit(); }
        vmem.fill(g3d::Window{});
        vmemUsed = 0;
    }
};

// v1.15.0 (record v11): una sezione della coda TLV del record (id, versione
// della sezione, dati). Le sezioni che questo firmware non conosce, o che sono
// di una versione piu' nuova di quella che sa leggere, restano qui e vengono
// riscritte intatte: un firmware piu' vecchio, o il Pico 2 W, non perde i dati
// delle modalita' che non ha (docs/STORAGE.md).
struct RecordSection {
    std::uint16_t id{};
    std::uint16_t version{};
    std::vector<std::uint8_t> data;
    bool operator==(const RecordSection& o) const noexcept { return id == o.id && version == o.version && data == o.data; }
};
namespace section {
// 0x0001-0x00FF: sezioni comuni; 0x0100 + AppId: la sezione di una modalita'.
constexpr std::uint16_t ModeBase = 0x0100;
constexpr std::size_t MaxSections = 64;                     // sezioni di un record
constexpr std::size_t MaxSectionBytes = 4u * 1024u * 1024u; // una sezione (il record e' comunque limitato dallo slot)
} // namespace section
struct Workspace;
// Codifica di una sezione conosciuta da questo firmware (src/workspace_sections.cpp).
struct SectionCodec {
    std::uint16_t id;
    std::uint16_t version;                                   // versione scritta (e la piu' nuova che sa leggere)
    // false: nulla da salvare (la sezione non viene scritta).
    bool (*present)(const Workspace& ws);
    void (*encode)(const Workspace& ws, std::vector<std::uint8_t>& out);
    // Legge una versione <= `version`; false = dati non validi (il record viene rifiutato).
    bool (*decode)(Workspace& ws, std::uint16_t version, const std::uint8_t* data, std::size_t size);
};
// Codifiche delle sezioni compilate in questa build (dipendono dalle modalita' della scheda).
const SectionCodec* workspaceSectionCodecs(std::size_t& count) noexcept;

// Dati condivisi da tutte le applicazioni. Risolve anche le variabili che
// Run-Matrix legge dalle altre modalita' con VARS (V-Window, intervallo di
// Table) e le matrici, i vettori e le liste citati in un'espressione.
struct Workspace final : NameResolver, CollectionResolver, ServiceResolver {
    // Per-board limits of the persistent data (limits.hpp): Pico 2 W 40 history
    // lines and lists of 255 elements (v1.8.0), Pico Plus 2 W 200 lines and
    // lists of 999 elements (fx-CG50) in its 2 x 512 KB record store.
    static constexpr std::size_t MaxHistory = limits::kHistoryEntries;
    static constexpr std::size_t ListCount = limits::kListCount;          // List 1..26 (v1.11.0)
    static constexpr std::size_t MaxListLength = limits::kListMaxLength;
    static constexpr std::size_t MaxListElements = limits::kListStoreElements;   // tutte le liste insieme
    static constexpr std::size_t MaxChatMessages = limits::kChatMessages;

    Settings settings;
    CalculatorMemory memory;
    std::vector<HistoryEntry> history;
    std::array<GraphFunction, MaxGraphFunctions> functions{};
    ViewWindow view;
    GraphState graph;
    TableRange table;
    std::array<std::vector<double>, ListCount> lists{};
    std::array<std::string, ListCount> listNames{};   // sottonomi delle liste ("QTY"), al piu' 8 caratteri
    EquationState equation;
    std::vector<ChatMessage> chat;
    Graph3DState graph3d;
    CollectionMemory matrices;
    // ---- v1.12.0 (record v8)
    // List File 1-6: il file attivo e' in lists / listNames; gli altri qui (lo
    // slot del file attivo resta vuoto). Il budget kListStoreElements vale per
    // tutti i file insieme.
    std::array<ListFileData, limits::kListFiles> listFiles{};
    std::array<std::string, limits::kFunctionMemories> functionMemories{};   // f1-f20 (OPTN › FUNCMEM)
    std::array<std::string, limits::kStrings> strings{};                    // Str 1-20 (VARS › Str)
    ModeVariables modeVars;
    // Servizio di GRAPH per PxlTest( (non salvato): 1 se il pixel della schermata
    // del grafico e' acceso. Lo registra l'app GRAPH; senza, PxlTest( da' 0.
    std::function<int(int, int)> pixelProbe;
    // ---- v1.15.0 (record v11)
    // Sezioni del record non lette da questo firmware (id sconosciuto o versione
    // piu' nuova), in ordine di id: riscritte intatte al salvataggio.
    std::vector<RecordSection> preservedSections;

    Workspace();
    void resetAll();
    // Applies the global angle mode used by the evaluator.
    void apply() const;
    EvalContext evalContext() const;            // memoria + modo complesso + nomi condivisi + Mat/Vct/List
    void addHistory(HistoryEntry entry);
    bool resolve(const std::string& lowerName, double& out) const override;

    // CollectionResolver: Mat A-Z, Vct A-Z, List 1-26 (le liste di Statistics),
    // le memorie delle risposte e le matrici di EQUATION (SimRes, SimCoef,
    // PlyRes, PlyCoef: v1.12.0, in sola lettura).
    bool collection(Memory memory, int index, CalcValue& out) const override;
    // Assegnazioni (-> Mat A, -> Vct A, -> List 1). Controllano tipo, forma e
    // limiti della scheda e lanciano TypeError / DimensionError / MathError
    // ("Memory ERROR") senza modificare nulla.
    void storeMatrix(std::size_t slot, CalcValue value);     // slot 0..25 = A..Z
    void storeVector(std::size_t slot, CalcValue value);
    void storeList(std::size_t index, const CalcValue& value);   // indice 0..25 = List 1..26 (elementi reali)
    // Liste di Statistics, TABLE (LISTMEM) e CSV: sostituisce List index+1
    // controllando lunghezza e budget della scheda (garanzia forte).
    void setList(std::size_t index, std::vector<double> values);
    // Elementi di tutte le liste; true se List index+1 puo' avere `length` elementi.
    std::size_t listElements() const noexcept;
    bool listFits(std::size_t index, std::size_t length) const noexcept;
    // Lancia "Memory ERROR" / "Dimension ERROR" se List index+1 non puo' avere `length` elementi.
    void checkListLength(std::size_t index, std::size_t length) const;
    // Sottonome di List index+1 (fx-CG50: riga SUB dell'editor delle liste).
    const std::string& listName(std::size_t index) const;
    void setListName(std::size_t index, std::string name);    // al piu' 8 caratteri stampabili
    // v1.12.0: List "QTY" (CollectionResolver), List File 1-6, Str, function memory,
    // variabili di sistema assegnabili e servizi del valutatore.
    int listByName(const std::string& subName) const override;
    bool special(const std::string& lowerName, CalcValue& out) const override;   // VARS EQUATION, TABLE Result
    void selectListFile(std::size_t file);                    // 0..5; il file attivo passa in lists/listNames
    // Assegna una variabile di sistema (Xmin, Xfct, TblStart, D Start, R Start,
    // a0, n, I%, ...): false se il nome non e' assegnabile. Lancia Range ERROR /
    // Argument ERROR se il valore non e' valido, senza cambiare nulla.
    bool storeSystemVariable(const std::string& lowerName, double value);
    void setString(std::size_t index, std::string text);     // Str 1..20 (indice 0..19)
    void setFunctionMemory(std::size_t index, std::string expression);   // f1..f20 (indice 0..19)
    void setRecursionFormula(std::string formula);           // an+1 = f(rec_n, rec_an), "" = nessuna
    int pixelTest(int row, int column) const override;
    std::string functionMemory(int n) const override;
    bool statisticVariable(const std::string& lowerName, double& out) const;
    // Memorizza un risultato lista / matrice / vettore in ListAns, o in MatAns
    // (e anche in VctAns quando ha una riga o una colonna, come sulla fx-CG50).
    void storeCollectionAnswer(const CalcValue& value);
    // Copia di tutte le matrici, i vettori e le liste (il ricalcolo della
    // cronologia la ripristina quando la riga modificata non riesce).
    struct CollectionSnapshot {
        CollectionMemory matrices; std::array<std::vector<double>, ListCount> lists;
        // v1.12.0: anche Str, function memory e variabili di sistema assegnabili con ->.
        std::array<std::string, limits::kStrings> strings; std::array<std::string, limits::kFunctionMemories> functionMemories;
        ModeVariables modeVars; ViewWindow view; double tableStart{}, tableEnd{}, tableStep{}, xFactor{}, yFactor{};
    };
    std::unique_ptr<CollectionSnapshot> snapshotCollections() const;
    void restoreCollections(CollectionSnapshot& snapshot);
};

// Binary, versioned serialisation used by the flash record store.
std::vector<std::uint8_t> serializeWorkspace(const Workspace& ws);
// Returns false (and leaves ws untouched) for corrupt or incompatible data.
bool deserializeWorkspace(const std::uint8_t* data, std::size_t size, Workspace& ws);
// Le stesse funzioni con un insieme di codifiche esplicito (test della coda TLV).
std::vector<std::uint8_t> serializeWorkspace(const Workspace& ws, const SectionCodec* codecs, std::size_t count);
bool deserializeWorkspace(const std::uint8_t* data, std::size_t size, Workspace& ws, const SectionCodec* codecs, std::size_t count);
// Versione del formato del record scritta da questo firmware (11 dalla v1.15.0).
std::uint16_t workspaceRecordVersion() noexcept;
} // namespace fx
