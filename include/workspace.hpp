#pragma once
#include "app_models.hpp"
#include "expression.hpp"
#include "graph_model.hpp"
#include "math_engine.hpp"
#include "memory.hpp"
#include "number_format.hpp"
#include "value.hpp"
#include <array>
#include <cstdint>
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

struct EquationState {
    std::uint8_t unknowns{2};
    std::array<double, 6 * 7> simultaneous{};    // row-major n x (n+1)
    std::uint8_t degree{2};
    std::array<double, 7> polynomial{};           // highest power first
    std::string solverExpr{"x^2-2=0"};
    double solverGuess{1};
    std::uint8_t ineqDegree{2};
    std::uint8_t ineqRelation{0};                 // 0 >, 1 <, 2 >=, 3 <=
    std::array<double, 7> inequality{};
};

struct ChatMessage { bool fromUser{}; std::string text; };

// 3D Graph mode (record v5): functions Z1..Z3, 3D V-Window, grid and view.
struct Graph3DState {
    models::Graph3DModel model{};
    float yaw{35.0F}, pitch{28.0F}, zoom{1.0F};
    Graph3DState() { model.z[0] = "sin(x)cos(y)"; }
};

// Dati condivisi da tutte le applicazioni. Risolve anche le variabili che
// Run-Matrix legge dalle altre modalita' con VARS (V-Window, intervallo di
// Table) e le matrici, i vettori e le liste citati in un'espressione.
struct Workspace final : NameResolver, CollectionResolver {
    // Per-board limits of the persistent data (limits.hpp): Pico 2 W 40 history
    // lines and lists of 255 elements (v1.8.0), Pico Plus 2 W 200 lines and
    // lists of 999 elements (fx-CG50) in its 2 x 512 KB record store.
    static constexpr std::size_t MaxHistory = limits::kHistoryEntries;
    static constexpr std::size_t ListCount = 6;
    static constexpr std::size_t MaxListLength = limits::kListMaxLength;
    static constexpr std::size_t MaxChatMessages = limits::kChatMessages;

    Settings settings;
    CalculatorMemory memory;
    std::vector<HistoryEntry> history;
    std::array<GraphFunction, MaxGraphFunctions> functions{};
    ViewWindow view;
    GraphState graph;
    TableRange table;
    std::array<std::vector<double>, ListCount> lists{};
    EquationState equation;
    std::vector<ChatMessage> chat;
    Graph3DState graph3d;
    CollectionMemory matrices;

    Workspace();
    void resetAll();
    // Applies the global angle mode used by the evaluator.
    void apply() const;
    EvalContext evalContext() const;            // memoria + modo complesso + nomi condivisi + Mat/Vct/List
    void addHistory(HistoryEntry entry);
    bool resolve(const std::string& lowerName, double& out) const override;

    // CollectionResolver: Mat A-Z, Vct A-Z, List 1-6 (le liste di Statistics) e
    // le memorie delle risposte.
    bool collection(Memory memory, int index, CalcValue& out) const override;
    // Assegnazioni (-> Mat A, -> Vct A, -> List 1). Controllano tipo, forma e
    // limiti della scheda e lanciano TypeError / DimensionError / MathError
    // ("Memory ERROR") senza modificare nulla.
    void storeMatrix(std::size_t slot, CalcValue value);     // slot 0..25 = A..Z
    void storeVector(std::size_t slot, CalcValue value);
    void storeList(std::size_t index, const CalcValue& value);   // indice 0..5 = List 1..6 (elementi reali)
    // Memorizza un risultato lista / matrice / vettore in ListAns, o in MatAns
    // (e anche in VctAns quando ha una riga o una colonna, come sulla fx-CG50).
    void storeCollectionAnswer(const CalcValue& value);
    // Copia di tutte le matrici, i vettori e le liste (il ricalcolo della
    // cronologia la ripristina quando la riga modificata non riesce).
    struct CollectionSnapshot { CollectionMemory matrices; std::array<std::vector<double>, ListCount> lists; };
    std::unique_ptr<CollectionSnapshot> snapshotCollections() const;
    void restoreCollections(CollectionSnapshot& snapshot);
};

// Binary, versioned serialisation used by the flash record store.
std::vector<std::uint8_t> serializeWorkspace(const Workspace& ws);
// Returns false (and leaves ws untouched) for corrupt or incompatible data.
bool deserializeWorkspace(const std::uint8_t* data, std::size_t size, Workspace& ws);
} // namespace fx
