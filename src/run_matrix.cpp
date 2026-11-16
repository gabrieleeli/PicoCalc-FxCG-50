#include "run_matrix.hpp"
#include "graph_commands.hpp"
#include "limits.hpp"
#include "math_engine.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <numeric>

namespace fx {
unsigned integerBaseOf(std::uint8_t mode) noexcept {
    switch (static_cast<Settings::BaseMode>(mode)) {
    case Settings::BaseMode::Dec: return 10;
    case Settings::BaseMode::Hex: return 16;
    case Settings::BaseMode::Bin: return 2;
    case Settings::BaseMode::Oct: return 8;
    default: return 0;
    }
}
namespace {
const char* const kArrow = "\xE2\x86\x92";   // U+2192
const char* const kSimp = "\xE2\x96\xB6Simp";  // ▶Simp (fx-CG50 OPTN, frazioni)
const char* const kFractionMark = "\xE2\x8C\x9F";   // ⌟ della modalita' Line

std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}
ast::NodePtr tok(const std::string& t) { return ast::make(ast::Kind::Token, t); }
ast::NodePtr rowOf(const std::string& text) {
    auto r = ast::make(ast::Kind::Row);
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        const std::size_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        r->children.push_back(tok(text.substr(i, len)));
        i += len;
    }
    return r;
}
void appendText(ast::Node& row, const std::string& text) {
    auto r = rowOf(text);
    for (auto& c : r->children) row.children.push_back(c);
}
ast::NodePtr fraction(ast::NodePtr top, ast::NodePtr bottom) {
    auto f = ast::make(ast::Kind::Fraction);
    f->children = {std::move(top), std::move(bottom)};
    return f;
}
ast::NodePtr radical(std::int64_t radicand) {
    auto r = ast::make(ast::Kind::Radical, "2");
    r->children = {rowOf(std::to_string(radicand))};
    return r;
}
bool rationalOf(const HistoryEntry& e, exact::Rational& r) { return e.exact.valid && e.exact.value.rational(r); }
bool isIntegerExact(const HistoryEntry& e) { exact::Rational r; return rationalOf(e, r) && r.isInteger(); }
bool plainResult(const HistoryEntry& e) { return e.error.empty() && e.value.imag() == 0; }
// ---------------------------------------------------------------- frazioni non ridotte (Simplify Manual, ▶Simp)
const ast::Node* unwrapGroups(const ast::Node* n) {
    while (n && n->kind == ast::Kind::Group && n->children.size() == 1 && n->children[0]) n = n->children[0].get();
    return n;
}
// "▶Simp" o "▶Simp n" in coda all'espressione.
struct SimpRequest { bool active{false}; std::int64_t factor{0}; };
SimpRequest takeSimp(std::string& expr) {
    SimpRequest r;
    const std::size_t at = expr.rfind(kSimp);
    if (at == std::string::npos) return r;
    std::string rest = trim(expr.substr(at + std::strlen(kSimp)));
    if (!rest.empty()) {
        if (rest.size() > 9 || !std::all_of(rest.begin(), rest.end(), [](char c) { return c >= '0' && c <= '9'; }))
            throw SyntaxError("Syntax ERROR: dopo \xE2\x96\xB6Simp serve un fattore intero", static_cast<int>(at), static_cast<int>(expr.size() - at));
        r.factor = std::strtoll(rest.c_str(), nullptr, 10);
    }
    r.active = true;
    expr = trim(expr.substr(0, at));
    if (expr.empty()) expr = "Ans";                     // "▶Simp" da solo: l'ultimo risultato, come sulla calcolatrice
    return r;
}
std::int64_t gcd64(std::int64_t a, std::int64_t b) { a = a < 0 ? -a : a; b = b < 0 ? -b : b; while (b) { const std::int64_t t = a % b; a = b; b = t; } return a; }

// ---------------------------------------------------------------- destinazioni della memorizzazione
// Cosa indica la freccia di memorizzazione: A, A~F, Mat A, Vct A, List 1, un
// loro elemento, o "Dim Mat A" (una lista {righe, colonne} crea una matrice di zeri).
struct StoreTarget {
    // v1.12.0: System (Xmin, D Start, n...), String (Str n), FunctionMemory (f1-f20).
    enum class Kind : std::uint8_t { None, Variables, Memory, Element, Dim, System, String, FunctionMemory } kind{Kind::None};
    std::string system;                                     // System: nome canonico ("xmax", "tvm_pv")
    std::string subName;                                    // List "QTY": risolto con il workspace
    char first{}, last{};                                   // Variables
    CollectionResolver::Memory memory{CollectionResolver::Memory::Mat};
    int index{};                                            // 0..25
    ast::NodePtr row, column;                               // Element: espressioni degli indici (column: solo matrici)
    std::string label;                                      // "Mat A", "List 1[3]"...
    ErrorLocation where;                                    // intervallo della freccia e della destinazione nell'input
};
bool memoryName(const std::string& text, CollectionResolver::Memory& memory, int& index) {
    using Memory = CollectionResolver::Memory;
    // Il parser legge List 1-26 come le variabili di lista condivise "list1".."list26".
    if (text.size() >= 5 && text.size() <= 6 && text.compare(0, 4, "list") == 0 && text[4] != '0' &&
        std::all_of(text.begin() + 4, text.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        memory = Memory::List; index = std::atoi(text.c_str() + 4) - 1;
        return index >= 0 && index < CollectionResolver::Slots;
    }
    const std::size_t space = text.find(' ');
    if (space == std::string::npos || space + 1 >= text.size()) return false;
    const std::string word = text.substr(0, space), name = text.substr(space + 1);
    if (word == "Mat") memory = Memory::Mat; else if (word == "Vct") memory = Memory::Vct; else if (word == "List") memory = Memory::List; else return false;
    if (name == "Ans") { index = CollectionResolver::Answer; return true; }
    if (memory == Memory::List && name.size() >= 2 && name.front() == '"') { index = -2; return true; }   // List "QTY"
    if (memory == Memory::List) { index = std::atoi(name.c_str()) - 1; return index >= 0 && index < CollectionResolver::Slots; }
    if (name.size() != 1 || name[0] < 'A' || name[0] > 'Z') return false;
    index = name[0] - 'A';
    return true;
}
StoreTarget parseTarget(const std::string& text) {
    StoreTarget t;
    std::string compact = text;
    compact.erase(std::remove(compact.begin(), compact.end(), ' '), compact.end());
    auto letter = [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); };
    auto alpha = [](char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; };
    if (compact.size() == 1 && alpha(compact[0])) { t.kind = StoreTarget::Kind::Variables; t.first = t.last = letter(compact[0]); return t; }
    // v1.12.0: r, θ, la variabile scritta esplicitamente (v_a), Str n, f1-f20, variabili di sistema.
    if (compact == "r_var" || compact == "theta" || compact == "\xCE\xB8") {
        t.kind = StoreTarget::Kind::Variables;
        t.first = t.last = compact == "r_var" ? CalculatorMemory::RSlot : CalculatorMemory::ThetaSlot;
        return t;
    }
    if (compact.size() == 3 && compact[0] == 'v' && compact[1] == '_' && compact[2] >= 'a' && compact[2] <= 'z') {
        t.kind = StoreTarget::Kind::Variables; t.first = t.last = letter(compact[2]); return t;
    }
    auto numbered = [&](const char* prefix, int max, int& index) {
        const std::size_t n = std::strlen(prefix);
        if (compact.size() <= n || compact.size() > n + 2 || compact.compare(0, n, prefix) != 0) return false;
        if (!std::all_of(compact.begin() + static_cast<std::ptrdiff_t>(n), compact.end(), [](char c) { return c >= '0' && c <= '9'; })) return false;
        index = std::atoi(compact.c_str() + n);
        return index >= 1 && index <= max && compact[n] != '0';
    };
    int numberedIndex = 0;
    if (compact == "rec_an1") { t.kind = StoreTarget::Kind::String; t.index = -1; t.label = "an+1"; return t; }   // "formula"->an+1
    if (numbered("str_", static_cast<int>(limits::kStrings), numberedIndex)) { t.kind = StoreTarget::Kind::String; t.index = numberedIndex - 1; t.label = "Str " + std::to_string(numberedIndex); return t; }
    if (numbered("f_", static_cast<int>(limits::kFunctionMemories), numberedIndex)) { t.kind = StoreTarget::Kind::FunctionMemory; t.index = numberedIndex - 1; t.label = "f" + std::to_string(numberedIndex); return t; }
    {
        std::string low = compact;
        for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (const char* const* n = systemVariableNames(); *n; ++n) {
            if (low != *n || low.compare(0, 4, "list") == 0 || low.compare(0, 5, "stat_") == 0 || low.compare(0, 4, "reg_") == 0 || low == "xdot") continue;
            t.kind = StoreTarget::Kind::System; t.system = low; t.label = low;
            return t;
        }
    }
    if (compact.size() == 3 && compact[1] == '~' && alpha(compact[0]) && alpha(compact[2])) {
        t.kind = StoreTarget::Kind::Variables; t.first = letter(compact[0]); t.last = letter(compact[2]);
        if (t.last >= t.first) return t;
        throw SyntaxError("Syntax ERROR: dopo -> serve una variabile A-Z o un intervallo A~F");
    }
    // Mat A, Vct A, List 1, un loro elemento o Dim Mat A: la destinazione la
    // legge lo stesso parser delle espressioni.
    ast::NodePtr tree;
    try { tree = parseExpression(text); } catch (const std::exception&) { tree.reset(); }
    if (tree) {
        const ast::Node* n = tree.get();
        bool dim = false;
        if (n->kind == ast::Kind::Call && n->text == "dim" && n->children.size() == 1 && n->children[0]) { dim = true; n = n->children[0].get(); }
        const ast::Node* ref = n;
        const bool element = !dim && n->kind == ast::Kind::Call && n->text == "elem" && n->children.size() >= 2 && n->children[0];
        if (element) ref = n->children[0].get();
        if (ref->kind == ast::Kind::Token && memoryName(ref->text, t.memory, t.index)) {
            if (t.index == CollectionResolver::Answer) throw SyntaxError("Syntax ERROR: le memorie Ans non si assegnano con ->");
            t.label = ref->text;
            if (t.index == -2) { t.subName = ref->text.substr(6, ref->text.size() - 7); t.label = ref->text; }
            else if (t.memory == CollectionResolver::Memory::List) t.label = "List " + std::to_string(t.index + 1);
            if (dim) t.kind = StoreTarget::Kind::Dim;
            else if (element) {
                t.kind = StoreTarget::Kind::Element;
                t.row = n->children[1];
                if (n->children.size() > 2) t.column = n->children[2];
                t.label += "[" + std::string(t.column ? "r,c" : "n") + "]";
            } else t.kind = StoreTarget::Kind::Memory;
            return t;
        }
    }
    throw SyntaxError("Syntax ERROR: dopo -> serve una variabile A-Z, un intervallo A~F, Mat, Vct o List");
}
std::size_t indexValue(const ast::NodePtr& n, const EvalContext& ctx, const char* what) {
    EvalContext quiet = ctx;
    quiet.info = nullptr;                    // le posizioni nel testo della destinazione non sono posizioni dell'input
    const double d = evaluateReal(*n, quiet);
    if (d != std::floor(d) || d < 1 || d > 1e6) throw MathError(std::string("Argument ERROR: ") + what + " deve essere un intero positivo");
    return static_cast<std::size_t>(d);
}
// numero -> Mat A[r,c], Vct A[n], List 1[n]
void storeElement(Workspace& ws, const StoreTarget& t, Complex value, const EvalContext& ctx, const ExactSlot* exactForm) {
    using Memory = CollectionResolver::Memory;
    const std::string name = t.label.substr(0, t.label.find('['));
    if (t.memory == Memory::List) {
        if (t.column) throw SyntaxError("Argument ERROR: una lista ha un solo indice, es. List 1[3]");
        CalcValue list;
        if (!ws.collection(Memory::List, t.index, list)) { list = CalcValue{}; list.type = ValueType::List; list.cols = 1; }
        const std::size_t i = indexValue(t.row, ctx, "l'indice");
        // Come sulla fx-CG50 una lista cresce di una cella alla volta.
        if (i > list.size() + 1) throw DimensionError("Dimension ERROR: indice " + std::to_string(i) + " oltre la fine di " + name);
        if (value.imag() != 0) throw MathError("Complex Number In List: le liste contengono solo numeri reali");
        if (i == list.size() + 1) { list.re.push_back(value.real()); list.rows = static_cast<std::uint16_t>(list.re.size()); }
        else list.re[i - 1] = value.real();
        ws.storeList(static_cast<std::size_t>(t.index), list);
        return;
    }
    CalcValue& m = (t.memory == Memory::Mat ? ws.matrices.mat : ws.matrices.vct)[static_cast<std::size_t>(t.index)];
    if (m.empty()) throw DimensionError("Dimension ERROR: " + name + " non definita");
    std::size_t pos = 0;
    if (t.column) {
        const std::size_t r = indexValue(t.row, ctx, "l'indice di riga"), c = indexValue(t.column, ctx, "l'indice di colonna");
        if (r > m.rows || c > m.cols) throw DimensionError("Dimension ERROR: [" + std::to_string(r) + "," + std::to_string(c) + "] fuori da " + name + " (" + dimensionText(m) + ")");
        pos = (r - 1) * m.cols + (c - 1);
    } else {
        if (!m.vectorShaped()) throw SyntaxError("Argument ERROR: una matrice ha due indici, es. Mat A[1,2]");
        const std::size_t i = indexValue(t.row, ctx, "l'indice");
        if (i > m.size()) throw DimensionError("Dimension ERROR: indice " + std::to_string(i) + " fuori da " + name + " (" + dimensionText(m) + ")");
        pos = i - 1;
    }
    // La forma esatta dell'elemento resta nella memoria (1/3 -> Mat A[1,1]).
    if (exactForm && exactForm->valid && value.imag() == 0) m.setExact(pos, exactForm->value);
    else m.set(pos, value);
    m.normalize();
}
// Gli errori dell'assegnazione si segnalano sulla freccia.
template <typename F> void atTarget(const StoreTarget& t, F&& store) {
    try { store(); }
    catch (SyntaxError& e) { if (!e.where.known()) e.where = t.where; throw; }
    catch (MathError& e) { if (!e.where.known()) e.where = t.where; throw; }
}
void locate(ErrorLocation& where, const EvalInfo& info, int offset) {
    if (!where.known() && info.error.known()) where = info.error;
    if (where.known()) where.position += offset;
}
bool naturalSize(const CalcValue& v) {
    if (v.isList()) return v.size() <= limits::kNaturalListLength;
    return v.rows <= limits::kNaturalMatrixDim && v.cols <= limits::kNaturalMatrixDim;
}
// Elementi per cui si tenta la valutazione esatta (che ripete il calcolo): dalla
// v1.12.0 il budget delle forme esatte della scheda, non piu' 64.
constexpr std::size_t kExactElements = CalcValue::ExactElements;

// Riga che fornisce Ans alla riga `previous` (l'ultima senza errore e numerica).
const HistoryEntry* answerEntry(const Workspace& ws, std::size_t previous) {
    for (std::size_t k = std::min(previous, ws.history.size()); k-- > 0;) {
        const HistoryEntry& h = ws.history[k];
        if (h.command || h.collection || !h.text.empty() || (!h.error.empty() && h.error[0] != '=')) continue;
        return &h;
    }
    return nullptr;
}
void scalarResult(Workspace& ws, const std::string& raw, const ast::Node& tree, const EvalContext& ctx, const EvalInfo& info,
                  const StoreTarget& target, Complex value, RunOutcome& out, HistoryEntry& entry,
                  const SimpRequest& simp, const HistoryEntry* previous, std::uint8_t baseMode) {
    out.value = value;
    // Pol( / Rec( danno una lista {r,θ} / {x,y} in List Ans (v1.12.0, collectionResult).
    // Exact form, evaluated on the same AST. When it exists it is the value:
    // its floating-point image replaces the rounded one (1/3*3 is exactly 1).
    ExactSlot slot;
    if (out.value.imag() == 0 && out.pairText.empty()) {
        exact::Value v;
        bool decimal = false;
        if (evaluateExact(tree, ctx, v, &decimal) && std::isfinite(v.toDouble())) {
            slot = ExactSlot{v, true, decimal};
            out.value = {v.toDouble(), 0.0};
            out.exact = true;
        }
    }
    atTarget(target, [&] {
        switch (target.kind) {
        case StoreTarget::Kind::None: break;
        case StoreTarget::Kind::Variables:
            if (out.value.imag() != 0) throw MathError("Argument ERROR: le variabili A-Z sono reali");
            out.storedTo = target.first; out.storedLast = target.last;
            for (char v = target.first; v <= target.last; ++v) {
                if (slot.valid) ws.memory.storeExact(v, out.value.real(), slot.value, slot.decimal);
                else ws.memory.store(v, out.value.real());
            }
            break;
        case StoreTarget::Kind::Element:
            storeElement(ws, target, out.value, ctx, &slot);
            out.storedIn = target.label;
            break;
        case StoreTarget::Kind::Dim:
            if (target.memory == CollectionResolver::Memory::List) {
                // n→Dim List k: una lista di n zeri (manuale 3-7).
                const double len = out.value.real();
                if (out.value.imag() != 0 || len != std::floor(len) || len < 1 || len > static_cast<double>(limits::kListMaxLength))
                    throw DimensionError("Dimension ERROR: la lunghezza di una lista va da 1 a " + std::to_string(limits::kListMaxLength));
                ws.setList(static_cast<std::size_t>(target.index), std::vector<double>(static_cast<std::size_t>(len), 0.0));
                out.storedIn = target.label;
                break;
            }
            [[fallthrough]];
        case StoreTarget::Kind::Memory:
            throw TypeError("Argument ERROR: in " + target.label + " si memorizza una " +
                            (target.memory == CollectionResolver::Memory::List ? "lista {1,2,3}" : "matrice [[1,2][3,4]]") + ", non un numero");
        case StoreTarget::Kind::System:
            if (out.value.imag() != 0) throw MathError("Argument ERROR: le variabili di sistema sono reali");
            if (!ws.storeSystemVariable(target.system, out.value.real())) throw SyntaxError("Syntax ERROR: " + target.label + " non si assegna");
            out.storedIn = target.label;
            break;
        case StoreTarget::Kind::String:
            throw TypeError("Argument ERROR: in " + target.label + " si memorizza una stringa \"...\"");
        case StoreTarget::Kind::FunctionMemory: break;      // gestita prima della valutazione
        }
    });
    // Frazione non ridotta: ▶Simp (con o senza fattore) o Simplify Manual.
    std::int64_t fracNum = 0, fracDen = 0;
    std::uint32_t factor = 0;
    exact::Rational r;
    const bool rational = slot.valid && slot.value.rational(r) && !r.isInteger();
    std::int64_t num = 0, den = 0;
    // La frazione come la calcolatrice la scrive con Simplify Manual: quella
    // digitata o il risultato non ridotto delle operazioni tra frazioni
    // (m.c.m. dei denominatori per + e −, prodotti per × e ÷).
    const bool unreducedForm = evaluateUnreducedFraction(tree, num, den) && den > 1 && num % den != 0;
    if (simp.active) {
        if (!rational) throw MathError("Math ERROR: \xE2\x96\xB6Simp richiede una frazione");
        // Ans▶Simp riprende la frazione mostrata dalla riga precedente.
        const ast::Node* t = unwrapGroups(&tree);
        if (t->kind == ast::Kind::Token && (t->text == "Ans" || t->text == "ans") && previous && previous->fracDen != 0) { num = previous->fracNum; den = previous->fracDen; }
        else if (!unreducedForm) { num = r.num; den = r.den; }
        const std::int64_t g = gcd64(num, den);
        std::int64_t f = 1;
        if (simp.factor != 0) {
            f = simp.factor;
            // Un divisore che non divide entrambi i termini e' un errore (manuale: "An error occurs").
            if (f < 2 || num % f != 0 || den % f != 0)
                throw MathError("Math ERROR: " + std::to_string(f) + " non divide numeratore e denominatore");
        } else {
            for (std::int64_t p = 2; p <= g / p; ++p) if (g % p == 0) { f = p; break; }
            if (f == 1 && g > 1) f = g;                       // g primo
        }
        // Frazione irriducibile: resta il valore originale, senza "F=" (manuale).
        fracNum = num / f; fracDen = den / f; factor = f > 1 ? static_cast<std::uint32_t>(f) : 0;
    } else if (ws.settings.simplifyManual && unreducedForm && rational && ws.settings.complex == ComplexMode::Real) {
        fracNum = num; fracDen = den;                      // la frazione resta non ridotta
    }
    if (slot.valid) ws.memory.setAnswerExact(out.value.real(), slot.value, slot.decimal);
    else ws.memory.setAnswer(out.value.real(), out.value.imag());
    entry = HistoryEntry{};
    entry.expression = raw;
    entry.value = out.value;
    entry.exact = slot;
    entry.numerical = info.numerical;
    entry.base = baseMode;
    entry.fracNum = fracNum; entry.fracDen = fracDen; entry.simpFactor = factor;
    if (!out.pairText.empty()) entry.error = std::string("=") + out.pairText;   // '=' prefix: informative text, not an error
}

void collectionResult(Workspace& ws, const std::string& raw, const ast::Node& tree, const EvalContext& ctx,
                      const StoreTarget& target, CalcValue value, RunOutcome& out, HistoryEntry& entry, std::uint8_t baseMode) {
    using Memory = CollectionResolver::Memory;
    out.type = value.type;
    auto result = std::make_shared<CollectionResult>();
    // Forme esatte degli elementi, dallo stesso AST. Come per i numeri, quando
    // esistono i loro valori in virgola mobile sono quelli memorizzati.
    if (value.size() <= kExactElements && !value.complex()) {
        ExactCollection ex;
        if (evaluateExactAny(tree, ctx, ex) && ex.type == value.type && ex.rows == value.rows && ex.cols == value.cols &&
            ex.elements.size() == value.size()) {
            for (std::size_t i = 0; i < value.size(); ++i) value.re[i] = ex.elements[i].toDouble();
            // Le memorie (Mat Ans, -> Mat A) conservano le forme esatte (v1.11.0).
            if (value.size() <= CalcValue::ExactElements) value.exact = ex.elements;
            result->exact = std::move(ex.elements);
            result->decimal = ex.decimal;
            out.exact = true;
        }
    }
    ws.storeCollectionAnswer(value);
    atTarget(target, [&] {
        const auto slot = static_cast<std::size_t>(target.index);
        switch (target.kind) {
        case StoreTarget::Kind::None: break;
        case StoreTarget::Kind::Variables:
            throw TypeError(std::string("Argument ERROR: una ") + valueTypeName(value.type) + " non si memorizza in una variabile A-Z (usa Mat, Vct o List)");
        case StoreTarget::Kind::Element:
        case StoreTarget::Kind::System:
            throw TypeError("Argument ERROR: in " + target.label + " si memorizza un numero");
        case StoreTarget::Kind::String:
            throw TypeError("Argument ERROR: in " + target.label + " si memorizza una stringa \"...\"");
        case StoreTarget::Kind::FunctionMemory: break;
        case StoreTarget::Kind::Memory:
            if (target.memory == Memory::List) ws.storeList(slot, value);
            else if (target.memory == Memory::Mat) ws.storeMatrix(slot, value);
            else ws.storeVector(slot, value);
            out.storedIn = target.label;
            break;
        case StoreTarget::Kind::Dim: {
            // {righe, colonne} -> Dim Mat A: una nuova matrice piena di zeri.
            if (target.memory == Memory::List || !value.isList() || value.size() != 2 || value.complex())
                throw TypeError("Argument ERROR: {righe,colonne}\xE2\x86\x92" "Dim Mat A crea una matrice di zeri");
            const double r = value.re[0], c = value.re[1];
            if (r != std::floor(r) || c != std::floor(c) || r < 1 || c < 1 || r > limits::kMatrixMaxDim || c > limits::kMatrixMaxDim)
                throw DimensionError("Dimension ERROR: dimensioni non valide per " + target.label);
            CalcValue zero = CalcValue::matrix(static_cast<std::size_t>(r), static_cast<std::size_t>(c));
            if (target.memory == Memory::Mat) ws.storeMatrix(slot, std::move(zero)); else ws.storeVector(slot, std::move(zero));
            out.storedIn = target.label;
            break;
        }
        }
    });
    entry = HistoryEntry{};
    entry.expression = raw;
    entry.base = baseMode;                                 // Base-N: elementi mostrati nella base della riga
    // La cronologia conserva cio' che mostra la vista naturale; i risultati piu'
    // grandi si leggono dalla memoria delle risposte (la riga indica dove sono).
    value.exact.clear();                                   // nella cronologia le forme esatte sono in result->exact
    if (naturalSize(value)) result->value = std::move(value);
    else { result->value.type = value.type; result->value.rows = value.rows; result->value.cols = value.cols; result->exact.clear(); }
    entry.collection = std::move(result);
}
} // namespace

namespace {
const char* const kDisplayMark = "\xE2\x97\xA2";      // ◢ comando di visualizzazione
// Istruzioni di una riga separate da ':' e '◢' al livello 0 (fuori da
// parentesi, quadre, graffe e virgolette): testo, posizione, risultato mostrato.
struct Statement { std::string text; std::size_t start{}; bool display{}; };
std::vector<Statement> splitStatements(const std::string& raw) {
    std::vector<Statement> out;
    int depth = 0;
    bool quoted = false;
    std::size_t start = 0;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const char c = raw[i];
        if (c == '"') { quoted = !quoted; continue; }
        if (quoted) continue;
        if (c == '(' || c == '[' || c == '{') ++depth;
        else if (c == ')' || c == ']' || c == '}') --depth;
        else if (depth <= 0 && (c == ':' || raw.compare(i, 3, kDisplayMark) == 0)) {
            const bool display = c != ':';
            out.push_back({raw.substr(start, i - start), start, display});
            i += display ? 2 : 0;
            start = i + 1;
        }
    }
    out.push_back({raw.substr(start), start, false});
    return out;
}
// Stringa "..." scritta da sola (o Str n): il testo, false altrimenti.
bool stringValue(const Workspace& ws, const std::string& expr, std::string& text) {
    const std::string t = trim(expr);
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"' && t.find('"', 1) == t.size() - 1) { text = t.substr(1, t.size() - 2); return true; }
    if (t.size() >= 5 && t.size() <= 6 && t.compare(0, 4, "str_") == 0 && std::all_of(t.begin() + 4, t.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        const int n = std::atoi(t.c_str() + 4);
        if (n < 1 || n > static_cast<int>(limits::kStrings)) return false;
        text = ws.strings[static_cast<std::size_t>(n - 1)];
        return true;
    }
    return false;
}
// Comandi delle righe di una matrice (fx-CG50 Swap, *Row, *Row+, Row+):
//   Swap A,r1,r2   *Row k,A,r   *Row+ k,A,r1,r2   Row+ A,r1,r2
bool rowCommand(Workspace& ws, const std::string& expr, HistoryEntry& entry, const std::string& raw) {
    std::string t = trim(expr);
    int kind = -1;
    std::size_t skip = 0;
    if (t.rfind("Swap", 0) == 0) { kind = 0; skip = 4; }
    else if (t.rfind("*Row+", 0) == 0) { kind = 2; skip = 5; }
    else if (t.rfind("*Row", 0) == 0) { kind = 1; skip = 4; }
    else if (t.rfind("Row+", 0) == 0) { kind = 3; skip = 4; }
    if (kind < 0 || (skip < t.size() && std::isalpha(static_cast<unsigned char>(t[skip])) && t[skip - 1] != '+')) return false;
    std::vector<std::string> args;
    std::string cur;
    int depth = 0;
    for (std::size_t i = skip; i < t.size(); ++i) {
        const char c = t[i];
        if (c == '(' || c == '[' || c == '{') ++depth;
        if (c == ')' || c == ']' || c == '}') --depth;
        if (c == ',' && depth == 0) { args.push_back(trim(cur)); cur.clear(); continue; }
        cur += c;
    }
    args.push_back(trim(cur));
    static const std::size_t counts[] = {3, 3, 4, 3};
    static const char* const usage[] = {"Swap A,r1,r2", "*Row k,A,r", "*Row+ k,A,r1,r2", "Row+ A,r1,r2"};
    if (args.size() != counts[kind]) throw SyntaxError(std::string("Syntax ERROR: uso ") + usage[kind], 0, static_cast<int>(raw.size()));
    const std::size_t matArg = kind == 0 || kind == 3 ? 0 : 1;
    std::string name = args[matArg];
    if (name.rfind("Mat", 0) == 0) name = trim(name.substr(3));
    if (name.size() != 1 || !std::isalpha(static_cast<unsigned char>(name[0]))) throw SyntaxError("Syntax ERROR: serve una matrice A-Z", 0, static_cast<int>(raw.size()));
    const std::size_t slot = static_cast<std::size_t>(std::toupper(static_cast<unsigned char>(name[0])) - 'A');
    CalcValue m = ws.matrices.mat[slot];
    if (m.empty()) throw DimensionError("Dimension ERROR: Mat " + std::string(1, static_cast<char>('A' + slot)) + " non definita");
    EvalContext ctx = ws.evalContext();
    auto number = [&](const std::string& a) { return evaluateValue(*parseExpression(a), ctx); };
    auto row = [&](const std::string& a) {
        const double r = number(a).real();
        if (r != std::floor(r) || r < 1 || r > m.rows) throw DimensionError("Dimension ERROR: riga " + a + " fuori dalla matrice");
        return static_cast<std::size_t>(r) - 1;
    };
    const std::size_t cols = m.cols;
    auto at = [&](std::size_t r, std::size_t c) { return m.at(r * cols + c); };
    if (kind == 0) {
        const std::size_t a = row(args[1]), b = row(args[2]);
        for (std::size_t c = 0; c < cols; ++c) { const Complex x = at(a, c); m.set(a * cols + c, at(b, c)); m.set(b * cols + c, x); }
    } else if (kind == 1) {
        const Complex k = number(args[0]);
        const std::size_t r = row(args[2]);
        for (std::size_t c = 0; c < cols; ++c) m.set(r * cols + c, k * at(r, c));
    } else {
        const Complex k = kind == 2 ? number(args[0]) : Complex(1, 0);
        const std::size_t a = row(args[kind == 2 ? 2 : 1]), b = row(args[kind == 2 ? 3 : 2]);
        for (std::size_t c = 0; c < cols; ++c) m.set(b * cols + c, at(b, c) + k * at(a, c));
    }
    m.clearExact();
    m.normalize();
    ws.storeMatrix(slot, std::move(m));
    entry = HistoryEntry{};
    entry.expression = raw;
    entry.text = "Done";
    return true;
}
// Fill(v, Mat A / Vct A / List n / List "QTY") cambia anche la memoria (manuale 2-52, 3-8).
void fillTarget(Workspace& ws, const ast::Node& tree, const CalcValue& value) {
    const ast::Node* t = unwrapGroups(&tree);
    if (t->kind != ast::Kind::Call || t->text != "fill" || t->children.size() != 2 || !t->children[1]) return;
    const ast::Node* ref = unwrapGroups(t->children[1].get());
    if (ref->kind != ast::Kind::Token) return;
    CollectionResolver::Memory memory{};
    int index = 0;
    if (!memoryName(ref->text, memory, index) || index == CollectionResolver::Answer) return;
    if (index == -2) index = ws.listByName(ref->text.substr(6, ref->text.size() - 7));
    if (index < 0) return;
    const auto slot = static_cast<std::size_t>(index);
    if (memory == CollectionResolver::Memory::List) ws.storeList(slot, value);
    else if (memory == CollectionResolver::Memory::Mat) ws.storeMatrix(slot, value);
    else ws.storeVector(slot, value);
}
RunOutcome evaluateStatement(Workspace& ws, const std::string& raw, const std::string& statement, HistoryEntry& entry, const EntryOptions& options) {
    const std::size_t lead = statement.find_first_not_of(" \t");
    const int offset = lead == std::string::npos ? 0 : static_cast<int>(lead);
    std::string expr = trim(statement);
    RunOutcome out;
    EvalInfo info;
    try {
        if (expr.empty()) throw SyntaxError("Syntax ERROR: istruzione vuota", 0, 0);
        // Comando grafico (Plot, F-Line, Graph Y=...): lo esegue l'interprete dei
        // comandi sui dati di GRAPH; la riga mostra "Done".
        if (graphcmd::isCommand(expr)) {
            ws.apply();
            graphcmd::execute(ws, expr);
            entry = HistoryEntry{};
            entry.expression = raw;
            entry.command = true;
            out.graphics = true;
            return out;
        }
        // VARS › EQUATION: coefficienti e soluzioni di un sistema lineare non si
        // richiamano insieme (manuale fx-CG50 1-33, nota *4).
        if (expr.find("SimRes") != std::string::npos && expr.find("SimCoef") != std::string::npos)
            throw MathError("Ma ERROR: SimRes e SimCoef non si richiamano nella stessa espressione", static_cast<int>(expr.find("SimCoef")) + offset, 7);
        if (rowCommand(ws, expr, entry, raw)) return out;
        const SimpRequest simp = takeSimp(expr);
        StoreTarget target;
        std::size_t arrow = expr.rfind(kArrow);
        std::size_t arrowLen = 3;
        if (arrow == std::string::npos) { arrow = expr.rfind("->"); arrowLen = 2; }
        if (arrow != std::string::npos) {
            const ErrorLocation where{static_cast<int>(arrow), static_cast<int>(expr.size() - arrow)};
            try { target = parseTarget(trim(expr.substr(arrow + arrowLen))); }
            catch (SyntaxError& e) { e.where = where; throw; }
            target.where = where;
            expr = expr.substr(0, arrow);
            expr.erase(expr.find_last_not_of(" \t") + 1);       // le posizioni prima della freccia non cambiano
            if (!target.subName.empty()) {
                target.index = ws.listByName(target.subName);
                if (target.index < 0) throw MathError("Argument ERROR: nessuna lista con il nome \"" + target.subName + "\"", where.position, where.length);
            }
        }
        // f1-f20 (FUNCMEM STORE): si memorizza il testo dell'espressione, non il valore.
        if (target.kind == StoreTarget::Kind::FunctionMemory) {
            ws.setFunctionMemory(static_cast<std::size_t>(target.index), trim(expr));
            entry = HistoryEntry{};
            entry.expression = raw;
            entry.text = "Done";
            out.storedIn = target.label;
            return out;
        }
        // Stringhe: "ABC" (anche ->Str n) o Str n da soli.
        std::string text;
        if (stringValue(ws, expr, text)) {
            if (target.kind == StoreTarget::Kind::String && target.index < 0) { ws.setRecursionFormula(text); out.storedIn = target.label; }
            else if (target.kind == StoreTarget::Kind::String) { ws.setString(static_cast<std::size_t>(target.index), text); out.storedIn = target.label; }
            else if (target.kind != StoreTarget::Kind::None) throw TypeError("Argument ERROR: una stringa si memorizza in Str 1-20", target.where.position, target.where.length);
            entry = HistoryEntry{};
            entry.expression = raw;
            entry.text = text;
            return out;
        }
        ws.apply();
        // Base-N: interi a 32 bit nella base del SET UP (o di quando la riga e' stata calcolata).
        const std::uint8_t baseMode = options.base >= 0 ? static_cast<std::uint8_t>(options.base) : static_cast<std::uint8_t>(ws.settings.baseMode);
        const unsigned base = integerBaseOf(baseMode);
        const auto tree = base ? parseExpression(expr, base) : parseExpression(expr);
        EvalContext ctx = ws.evalContext();
        ctx.info = &info;
        if (base) { ctx.integerBase = static_cast<std::uint8_t>(base); ctx.complexMode = ComplexMode::Real; }
        CalcValue value = evaluateAny(*tree, ctx);
        const HistoryEntry* previous = answerEntry(ws, options.previous);
        if (value.isScalar()) scalarResult(ws, raw, *tree, ctx, info, target, value.scalar(), out, entry, simp, previous, base ? baseMode : 0);
        else {
            if (simp.active) throw MathError("Math ERROR: \xE2\x96\xB6Simp richiede una frazione");
            fillTarget(ws, *tree, value);
            collectionResult(ws, raw, *tree, ctx, target, std::move(value), out, entry, base ? baseMode : 0);
        }
    }
    catch (SyntaxError& e) { locate(e.where, info, offset); throw; }
    catch (MathError& e) { locate(e.where, info, offset); throw; }
    return out;
}
} // namespace

RunOutcome evaluateEntry(Workspace& ws, const std::string& raw, HistoryEntry& entry, const EntryOptions& options) {
    const std::vector<Statement> statements = splitStatements(raw);
    if (statements.size() == 1) return evaluateStatement(ws, raw, raw, entry, options);
    // Piu' istruzioni (manuale 2-9): si eseguono in ordine; ◢ mostra il risultato
    // intermedio, la riga mostra l'ultimo. Gli errori indicano la posizione nella riga intera.
    RunOutcome out;
    std::vector<std::string> displays;
    for (std::size_t k = 0; k < statements.size(); ++k) {
        const Statement& st = statements[k];
        HistoryEntry part;
        try {
            out = evaluateStatement(ws, raw, st.text, part, options);
        } catch (SyntaxError& e) { if (e.where.known()) e.where.position += static_cast<int>(st.start); else e.where = {static_cast<int>(st.start), static_cast<int>(st.text.size())}; throw; }
        catch (MathError& e) { if (e.where.known()) e.where.position += static_cast<int>(st.start); else e.where = {static_cast<int>(st.start), static_cast<int>(st.text.size())}; throw; }
        if (st.display) displays.push_back(resultText(part, ws.settings));
        if (k + 1 == statements.size()) { entry = std::move(part); entry.expression = raw; }
    }
    entry.displays = std::move(displays);
    return out;
}

RunOutcome runCalculation(Workspace& ws, const std::string& raw) {
    HistoryEntry h;
    const RunOutcome out = evaluateEntry(ws, raw, h);
    ws.addHistory(std::move(h));
    return out;
}

std::size_t recalculateHistory(Workspace& ws, std::size_t from, ErrorLocation* firstError) {
    auto& hist = ws.history;
    if (firstError) *firstError = ErrorLocation{};
    if (from >= hist.size()) return 0;
    // Ans before the first recalculated entry is the result of the entry before
    // it (0 when there is none, as on the fx-CG50); PreAns the one before that.
    // Liste, matrici e vettori vanno nelle loro memorie delle risposte, non in Ans.
    auto answerOf = [&](std::size_t count, double& re, double& im, ExactSlot& slot) {
        re = im = 0; slot = ExactSlot{exact::Value{}, true, false};
        for (std::size_t k = count; k-- > 0;) {
            const HistoryEntry& h = hist[k];
            if (!h.error.empty() && h.error[0] != '=') continue;     // failed entries do not change Ans
            if (h.collection || h.command || !h.text.empty()) continue;
            re = h.value.real(); im = h.value.imag(); slot = h.exact;
            return k;
        }
        return static_cast<std::size_t>(-1);
    };
    double ar = 0, ai = 0, pr = 0, pi = 0;
    ExactSlot ae, pe;
    const std::size_t last = answerOf(from, ar, ai, ae);
    if (last != static_cast<std::size_t>(-1)) (void)answerOf(last, pr, pi, pe);
    else pe = ExactSlot{exact::Value{}, true, false};
    ws.memory.restoreAnswers(ar, ai, ae, pr, pi, pe);
    std::size_t failures = 0;
    for (std::size_t i = from; i < hist.size(); ++i) {
        HistoryEntry fresh;
        auto failed = [&](const char* what, const ErrorLocation& where) {
            fresh = HistoryEntry{};
            fresh.expression = hist[i].expression;
            fresh.value = {std::nan(""), 0.0};
            fresh.error = what;
            if (fresh.error.empty() || fresh.error[0] == '=') fresh.error = "ERROR";
            if (i == from && firstError) *firstError = where;
            ++failures;
        };
        // I comandi grafici delle righe successive non si rieseguono (disegnerebbero
        // di nuovo gli stessi elementi); la riga modificata si esegue.
        if (hist[i].command && i != from) continue;
        try {
            EntryOptions opt;
            opt.base = i == from ? -1 : hist[i].base;          // la riga modificata usa la modalita' corrente
            opt.previous = i;
            (void)evaluateEntry(ws, hist[i].expression, fresh, opt);
            fresh.view = static_cast<std::uint8_t>(hist[i].view == ViewDecimal || hist[i].view == ViewExact ? hist[i].view : static_cast<std::uint8_t>(ViewAuto));
            fresh.flipMixed = hist[i].flipMixed;
            if (hist[i].view == ViewEng) { fresh.view = ViewEng; fresh.engShift = hist[i].engShift; }
        }
        catch (const SyntaxError& e) { failed(e.what(), e.where); }
        catch (const MathError& e) { failed(e.what(), e.where); }
        catch (const std::exception& e) { failed(e.what(), ErrorLocation{}); }
        hist[i] = std::move(fresh);
    }
    return failures;
}

std::size_t editHistoryEntry(Workspace& ws, std::size_t index, const std::string& expression) {
    if (index >= ws.history.size()) throw std::out_of_range("Voce della cronologia inesistente");
    // Copie sullo heap: CalculatorMemory con le forme esatte occupa ~1,7 KB,
    // troppo per un frame sullo stack di 32 KB dell'RP2350. Il ricalcolo puo'
    // anche assegnare matrici, vettori e liste: vengono ripristinati con il resto.
    const std::vector<HistoryEntry> savedHistory = ws.history;
    const auto savedMemory = std::make_unique<CalculatorMemory>(ws.memory);
    const auto savedCollections = ws.snapshotCollections();
    ws.history[index].expression = expression;
    ErrorLocation where;
    const std::size_t failures = recalculateHistory(ws, index, &where);
    const HistoryEntry& edited = ws.history[index];
    if (!edited.error.empty() && edited.error[0] != '=') {
        const std::string what = edited.error;
        ws.history = savedHistory;
        ws.memory = *savedMemory;
        ws.restoreCollections(*savedCollections);
        // Re-raise with the original class so the error box shows the right title.
        if (what.rfind("Syntax", 0) == 0) throw SyntaxError(what, where.position, where.length);
        throw MathError(what, where.position, where.length);
    }
    return failures;
}

// ---------------------------------------------------------------- views
namespace {
bool integerValue(const exact::Value& v) { exact::Rational r; return v.rational(r) && r.isInteger(); }
// Un risultato lista / matrice / vettore mostra le forme esatte dei suoi elementi.
bool collectionExactView(const HistoryEntry& e, const Settings& s) {
    const CollectionResult& c = *e.collection;
    if (!e.error.empty() || c.value.empty() || c.exact.size() != c.value.size()) return false;
    if (e.view == ViewExact) return true;
    return e.view == ViewAuto && s.fractionResults && !c.decimal && s.display.mode != DisplayMode::Eng;
}
// Forma esatta mostrata per l'elemento i (interi e forme oltre i limiti di
// visualizzazione si mostrano come decimali, come i risultati numerici).
const exact::Value* elementExact(const HistoryEntry& e, std::size_t i, const Settings& s) {
    if (!collectionExactView(e, s)) return nullptr;
    const exact::Value& v = e.collection->exact[i];
    if (integerValue(v)) return nullptr;
    return (e.view == ViewExact ? exact::fitsRelaxed(v) : exact::fitsNatural(v, s.mixedFractions)) ? &v : nullptr;
}
bool anyElementExact(const HistoryEntry& e, const Settings& s) {
    for (std::size_t i = 0; i < e.collection->value.size(); ++i) if (elementExact(e, i, s)) return true;
    return false;
}
} // namespace

bool showsExact(const HistoryEntry& e, const Settings& s) {
    if (e.collection) return anyElementExact(e, s);
    if (e.command || e.base != 0) return false;
    if (!plainResult(e) || !e.exact.valid || isIntegerExact(e)) return false;
    // Frazione di ▶Simp o di Simplify Manual: mostrata finche' non si chiede il decimale.
    if (e.fracDen != 0 && (e.view == ViewAuto || e.view == ViewExact)) return true;
    if (e.view == ViewExact) return exact::fitsRelaxed(e.exact.value);
    // Eng (v1.11.0): i risultati si mostrano in notazione ingegneristica (S⇔D: forma esatta).
    if (e.view != ViewAuto || !s.fractionResults || e.exact.decimal || s.display.mode == DisplayMode::Eng) return false;
    return exact::fitsNatural(e.exact.value, s.mixedFractions != e.flipMixed);
}
bool showsMixed(const HistoryEntry& e, const Settings& s) {
    exact::Rational r;
    if (e.fracDen != 0) return false;                     // la frazione non ridotta resta impropria
    return showsExact(e, s) && rationalOf(e, r) && (s.mixedFractions != e.flipMixed) && std::llabs(r.num) > r.den;
}
bool isApproximation(const HistoryEntry& e, const Settings& s) {
    return plainResult(e) && e.numerical && !showsExact(e, s) && e.view <= ViewDecimal;
}

bool toggleExactDecimal(HistoryEntry& e, const Settings& s) {
    if (!e.error.empty()) return false;
    if (e.collection) {
        // Tutti gli elementi passano insieme tra forme esatte e decimali.
        if (anyElementExact(e, s)) { e.view = ViewDecimal; return true; }
        HistoryEntry probe = e;
        probe.view = ViewAuto;
        if (anyElementExact(probe, s)) { e.view = ViewAuto; return true; }
        probe.view = ViewExact;
        if (anyElementExact(probe, s)) { e.view = ViewExact; return true; }
        return false;
    }
    if (showsExact(e, s) || e.view == ViewPeriodic) { e.view = ViewDecimal; return true; }
    if (!e.exact.valid || isIntegerExact(e) || !exact::fitsRelaxed(e.exact.value)) {
        if (e.view != ViewAuto && e.view != ViewDecimal) { e.view = ViewAuto; return true; }   // back from Base-N / DMS
        return false;
    }
    HistoryEntry probe = e;
    probe.view = ViewAuto;
    e.view = showsExact(probe, s) ? ViewAuto : ViewExact;
    return true;
}
bool toggleMixedFraction(HistoryEntry& e, const Settings& s) {
    if (e.collection) return false;
    exact::Rational r;
    if (!showsExact(e, s) || !rationalOf(e, r) || std::llabs(r.num) <= r.den) return false;
    e.flipMixed = !e.flipMixed;
    // The other format may need more characters than the automatic view allows.
    if (!showsExact(e, s)) e.view = ViewExact;
    return true;
}
bool togglePeriodic(HistoryEntry& e) {
    exact::Rational r;
    if (e.collection || !e.error.empty() || !rationalOf(e, r) || r.isInteger()) return false;
    e.view = e.view == ViewPeriodic ? ViewAuto : ViewPeriodic;
    return true;
}

namespace {
std::string periodicText(const exact::Rational& r) {
    const exact::Expansion x = exact::decimalExpansion(r);
    std::string s = (x.negative ? "-" : "") + x.integer;
    if (!x.fixed.empty() || !x.period.empty()) s += ".";
    s += x.fixed;
    if (!x.period.empty()) s += "(" + x.period + ")";
    if (x.truncated) s += "\xE2\x80\xA6";
    return s;
}
std::string baseText(const HistoryEntry& e) {
    const double v = e.value.real();
    if (v != std::floor(v) || std::abs(v) > 9.0e15) return "Math ERROR: Base-N richiede un intero";
    const unsigned base = e.view == ViewHex ? 16 : e.view == ViewBin ? 2 : 8;
    const long long iv = static_cast<long long>(v);
    const std::string digits = toBase(static_cast<std::uint64_t>(iv < 0 ? -iv : iv), base);
    const char* prefix = e.view == ViewHex ? "0x" : e.view == ViewBin ? "0b" : "0o";
    return (iv < 0 ? "-" : "") + std::string(prefix) + digits;
}
std::string dmsText(const HistoryEntry& e, const Settings& s) {
    const double v = e.value.real();
    const auto sx = decimalToSexagesimal(v);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d\xC2\xB0%d'%s\"", sx.degrees, sx.minutes, formatReal(std::round(sx.seconds * 1e6) / 1e6, s.display).c_str());
    return (v < 0 && sx.degrees == 0 ? "-" : "") + std::string(buf);
}
// 2D tree of an exact value (fractions stacked, radicals, pi).
ast::NodePtr exactTree(const exact::Value& v, bool mixed) {
    auto root = ast::make(ast::Kind::Row);
    if (v.isZero()) { appendText(*root, "0"); return root; }
    if (v.form() == exact::Value::Form::Pi) {
        const exact::Rational& c = v.term(0).coef;
        const std::int64_t a = std::llabs(c.num);
        if (c.num < 0) appendText(*root, "-");
        auto top = rowOf(a == 1 ? std::string() : std::to_string(a));
        top->children.push_back(tok("pi"));
        if (c.isInteger()) { for (auto& t : top->children) root->children.push_back(t); return root; }
        root->children.push_back(fraction(top, rowOf(std::to_string(c.den))));
        return root;
    }
    exact::Rational r;
    if (v.rational(r)) {
        const std::int64_t n = std::llabs(r.num);
        if (r.num < 0) appendText(*root, "-");
        if (mixed && n > r.den) {
            appendText(*root, std::to_string(n / r.den));
            root->children.push_back(fraction(rowOf(std::to_string(n % r.den)), rowOf(std::to_string(r.den))));
        } else {
            root->children.push_back(fraction(rowOf(std::to_string(n)), rowOf(std::to_string(r.den))));
        }
        return root;
    }
    // Surd: numerators over the least common denominator, as on the fx-CG50.
    std::int64_t lcd = 1;
    for (std::size_t i = 0; i < v.termCount(); ++i) { const std::int64_t d = v.term(i).coef.den; lcd = lcd / std::gcd(lcd, d) * d; }
    auto top = ast::make(ast::Kind::Row);
    bool leadingMinus = false;
    for (std::size_t i = 0; i < v.termCount(); ++i) {
        const exact::Term& t = v.term(i);
        const std::int64_t num = t.coef.num * (lcd / t.coef.den), a = std::llabs(num);
        if (num < 0) { if (i == 0 && v.termCount() == 1 && lcd > 1) leadingMinus = true; else appendText(*top, "-"); }
        else if (i > 0) appendText(*top, "+");
        if (t.radicand == 1) appendText(*top, std::to_string(a));
        else { if (a != 1) appendText(*top, std::to_string(a)); top->children.push_back(radical(t.radicand)); }
    }
    if (lcd == 1) return top;
    if (leadingMinus) appendText(*root, "-");
    root->children.push_back(fraction(top, rowOf(std::to_string(lcd))));
    return root;
}
} // namespace

namespace {
// Numero come si disegna in un risultato: mantissa e, in notazione scientifica, x10^n.
void appendNumber(ast::Node& row, double value, DisplayFormat format) {
    const FormattedNumber fnum = formatNumber(value, format);
    appendText(row, fnum.mantissa);
    if (!fnum.symbol.empty()) appendText(row, fnum.symbol);
    if (fnum.scientific) {
        row.children.push_back(tok("*"));
        auto p = ast::make(ast::Kind::Power);
        p->children = {rowOf("10"), rowOf(std::to_string(fnum.exponent))};
        row.children.push_back(p);
    }
}
// "Mat 10×12 → Mat Ans": un risultato troppo grande per la vista naturale.
std::string collectionSummary(const CalcValue& v) {
    if (v.isList()) return "List (" + std::to_string(v.rows) + ") \xE2\x86\x92 List Ans";
    const char* word = v.type == ValueType::Vector ? "Vct " : "Mat ";
    return word + std::to_string(v.rows) + "\xC3\x97" + std::to_string(v.cols) + " \xE2\x86\x92 " + (v.type == ValueType::Vector ? "Vct Ans" : "Mat Ans");
}
std::string collectionText(const HistoryEntry& e, const Settings& s) {
    const CalcValue& v = e.collection->value;
    if (v.empty()) return collectionSummary(v);
    std::string out = v.isList() ? "{" : "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (v.isList()) { if (i) out += ","; }
        else out += i % v.cols == 0 ? "[" : ",";
        out += elementText(e, i, s);
        if (!v.isList() && i % v.cols == v.cols - 1u) out += "]";
    }
    return out + (v.isList() ? "}" : "]");
}
ast::NodePtr cellTree(const HistoryEntry& e, std::size_t i, const Settings& s) {
    if (const exact::Value* x = elementExact(e, i, s)) return exactTree(*x, s.mixedFractions);
    auto row = ast::make(ast::Kind::Row);
    if (displayedBase(e)) { appendText(*row, elementText(e, i, s)); return row; }
    const Complex z = e.collection->value.at(i);
    if (z.imag() == 0) appendNumber(*row, z.real(), s.display);
    else appendText(*row, formatValue(z, s.complex == ComplexMode::Real ? ComplexMode::Rectangular : s.complex, s.display));
    return row;
}
} // namespace

namespace {
struct EngParts { std::string mantissa; int exponent{0}; };
// Notazione ingegneristica della riga: esponente multiplo di 3 spostato di engShift passi.
EngParts engParts(const HistoryEntry& e) {
    const double v = e.value.real();
    EngParts p;
    int base = 0;
    if (v != 0) base = 3 * static_cast<int>(std::floor(std::log10(std::abs(v)) / 3.0));
    p.exponent = base + 3 * e.engShift;
    const double m = v / std::pow(10.0, p.exponent);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.10g", m);
    p.mantissa = buf;
    return p;
}
} // namespace
bool shiftEngineering(HistoryEntry& e, int direction) {
    if (!e.error.empty() || e.collection || e.command || !e.text.empty() || e.value.imag() != 0 || !std::isfinite(e.value.real())) return false;
    // Prima pressione: ENG mostra la notazione ingegneristica (1234 -> 1.234E+03),
    // ENG← l'esponente inferiore (1234E+00); le successive spostano di 3.
    if (e.view != ViewEng) { e.view = ViewEng; e.engShift = static_cast<std::int8_t>(direction > 0 ? 0 : -1); return true; }
    const int next = e.engShift + (direction > 0 ? 1 : -1);
    if (next < -40 || next > 40) return false;
    e.engShift = static_cast<std::int8_t>(next);
    return true;
}
bool setComplexView(HistoryEntry& e, bool polar) {
    if (!e.error.empty() || e.collection || e.command || e.value.imag() == 0) return false;
    e.view = polar ? ViewPolar : ViewRect;
    return true;
}

std::string baseNText(double value, unsigned base) {
    if (!std::isfinite(value) || value != std::floor(value) || value < -2147483648.0 || value > 2147483647.0) return "Math ERROR";
    const auto v = static_cast<std::int32_t>(value);
    if (base == 10) return std::to_string(v);
    // Bin: 16 bit in complemento a due (manuale 2-39); fuori intervallo Math ERROR.
    if (base == 2 && (v < -32768 || v > 32767)) return "Math ERROR";
    std::uint32_t u = base == 2 ? static_cast<std::uint32_t>(static_cast<std::uint16_t>(v)) : static_cast<std::uint32_t>(v);
    if (u == 0) return "0";
    std::string digits;
    while (u) { const unsigned d = u % base; digits.insert(digits.begin(), static_cast<char>(d < 10 ? '0' + d : 'A' + d - 10)); u /= base; }
    return digits;
}
unsigned displayedBase(const HistoryEntry& e) {
    if (e.base == 0) return 0;
    switch (e.view) {
    case ViewHex: return 16;
    case ViewBin: return 2;
    case ViewOct: return 8;
    case ViewDecimal: return 10;
    default: return integerBaseOf(e.base);
    }
}
namespace {
// Output Line della fx-CG50: frazioni con ⌟ ("1⌟3", "2⌟1⌟3"); le altre forme esatte come sempre.
std::string lineFractionText(const exact::Value& v, bool mixed) {
    exact::Rational r;
    if (!v.rational(r) || r.isInteger()) return exact::toText(v, mixed);
    const std::int64_t n = std::llabs(r.num);
    std::string out = r.num < 0 ? "-" : "";
    if (mixed && n > r.den) return out + std::to_string(n / r.den) + kFractionMark + std::to_string(n % r.den) + kFractionMark + std::to_string(r.den);
    return out + std::to_string(n) + kFractionMark + std::to_string(r.den);
}
std::string simpText(const HistoryEntry& e, bool lineStyle) {
    std::string t = (e.fracNum < 0 ? "-" : "") + std::to_string(std::llabs(e.fracNum)) + (lineStyle ? kFractionMark : "/") + std::to_string(e.fracDen);
    if (e.simpFactor) t += "  F=" + std::to_string(e.simpFactor);
    return t;
}
} // namespace

ast::NodePtr exactValueTree(const exact::Value& value, bool mixed) { return exactTree(value, mixed); }
void appendNumberTree(ast::Node& row, double value, DisplayFormat format) { appendNumber(row, value, format); }
void appendTextTokens(ast::Node& row, const std::string& text) { appendText(row, text); }
std::string exactLineText(const exact::Value& value, bool mixed) { return lineFractionText(value, mixed); }

std::string elementText(const HistoryEntry& e, std::size_t i, const Settings& s) {
    if (!e.collection || i >= e.collection->value.size()) return {};
    // Base-N (v1.12.0): gli elementi di liste e matrici nella base della riga.
    if (const unsigned base = displayedBase(e)) return baseNText(e.collection->value.at(i).real(), base);
    if (const exact::Value* x = elementExact(e, i, s)) return exact::toText(*x, s.mixedFractions);
    const Complex z = e.collection->value.at(i);
    return formatValue(z, z.imag() != 0 && s.complex == ComplexMode::Real ? ComplexMode::Rectangular : s.complex, s.display);
}

std::string resultText(const HistoryEntry& e, const Settings& s, bool lineStyle) {
    if (e.command && e.error.empty()) return "Done";
    if (!e.text.empty() && e.error.empty()) return e.text;
    if (!e.error.empty() && e.error[0] == '=') return e.error.substr(1);
    if (!e.error.empty()) return e.error;
    if (e.collection) {
        std::string t = collectionText(e, s);
        if (lineStyle) {
            // Le frazioni degli elementi in stile Line: 1/2 -> 1⌟2.
            const CalcValue& v = e.collection->value;
            if (!v.empty() && collectionExactView(e, s)) {
                t = v.isList() ? "{" : "[";
                for (std::size_t i = 0; i < v.size(); ++i) {
                    if (v.isList()) { if (i) t += ","; }
                    else t += i % v.cols == 0 ? "[" : ",";
                    const exact::Value* x = elementExact(e, i, s);
                    t += x ? lineFractionText(*x, s.mixedFractions) : elementText(e, i, s);
                    if (!v.isList() && i % v.cols == v.cols - 1u) t += "]";
                }
                t += v.isList() ? "}" : "]";
            }
        }
        return t;
    }
    if (e.value.imag() == 0) {
        if (const unsigned base = displayedBase(e)) return baseNText(e.value.real(), base);
        if (e.view >= ViewHex && e.view <= ViewOct) return baseText(e);
        if (e.view == ViewDms) return dmsText(e, s);
        if (e.view == ViewEng) { const EngParts p = engParts(e); return p.mantissa + "E" + (p.exponent < 0 ? "-" : "+") + (std::abs(p.exponent) < 10 ? "0" : "") + std::to_string(std::abs(p.exponent)); }
        exact::Rational r;
        if (e.view == ViewPeriodic && rationalOf(e, r)) return periodicText(r);
        if (e.fracDen != 0 && showsExact(e, s)) return simpText(e, lineStyle);
        if (showsExact(e, s)) return lineStyle ? lineFractionText(e.exact.value, showsMixed(e, s)) : exact::toText(e.exact.value, showsMixed(e, s));
    }
    if (e.value.imag() != 0 && (e.view == ViewPolar || e.view == ViewRect))
        return formatValue(e.value, e.view == ViewPolar ? ComplexMode::Polar : ComplexMode::Rectangular, s.display);
    return formatValue(e.value, s.complex, s.display);
}

ast::NodePtr resultTree(const HistoryEntry& e, const Settings& s) {
    auto root = ast::make(ast::Kind::Row);
    if (e.collection && e.error.empty()) {
        const CalcValue& v = e.collection->value;
        if (v.empty()) { appendText(*root, collectionSummary(v)); return root; }
        if (v.isList()) {
            // Vista naturale di una lista: {a, b, c} con ogni elemento disegnato in 2D.
            root->children.push_back(tok("{"));
            for (std::size_t i = 0; i < v.size(); ++i) {
                if (i) root->children.push_back(tok(","));
                const ast::NodePtr cell = cellTree(e, i, s);       // tenuto in vita durante il ciclo
                for (const auto& c : cell->children) root->children.push_back(c);
            }
            root->children.push_back(tok("}"));
            return root;
        }
        auto m = ast::make(ast::Kind::Matrix, std::to_string(v.cols));
        for (std::size_t i = 0; i < v.size(); ++i) m->children.push_back(cellTree(e, i, s));
        root->children.push_back(m);
        return root;
    }
    if (e.command && e.error.empty()) { appendText(*root, "Done"); return root; }
    if (!e.text.empty() && e.error.empty()) { appendText(*root, e.text); return root; }
    if (plainResult(e) && e.view == ViewEng) {
        // ENG / ENG←: mantissa ×10^n con n multiplo di 3.
        const EngParts p = engParts(e);
        appendText(*root, p.mantissa);
        root->children.push_back(tok("*"));
        auto pw = ast::make(ast::Kind::Power);
        pw->children = {rowOf("10"), rowOf(std::to_string(p.exponent))};
        root->children.push_back(pw);
        return root;
    }
    if (plainResult(e) && displayedBase(e)) { appendText(*root, resultText(e, s)); return root; }
    if (plainResult(e) && e.fracDen != 0 && showsExact(e, s)) {
        // Frazione non ridotta (▶Simp, Simplify Manual) con il fattore usato: F=3.
        if (e.fracNum < 0) appendText(*root, "-");
        root->children.push_back(fraction(rowOf(std::to_string(std::llabs(e.fracNum))), rowOf(std::to_string(e.fracDen))));
        if (e.simpFactor) appendText(*root, "  F=" + std::to_string(e.simpFactor));
        return root;
    }
    if (plainResult(e)) {
        if (showsExact(e, s)) return exactTree(e.exact.value, showsMixed(e, s));
        exact::Rational r;
        if (e.view == ViewPeriodic && rationalOf(e, r)) {
            const exact::Expansion x = exact::decimalExpansion(r);
            appendText(*root, (x.negative ? "-" : "") + x.integer);
            if (!x.fixed.empty() || !x.period.empty()) appendText(*root, ".");
            appendText(*root, x.fixed);
            if (!x.period.empty()) {
                auto over = ast::make(ast::Kind::Call, "overline");
                over->children = {rowOf(x.period)};
                root->children.push_back(over);
            }
            if (x.truncated) appendText(*root, "\xE2\x80\xA6");
            return root;
        }
        if (e.view <= ViewDecimal || e.view == ViewExact) {
            if (isApproximation(e, s)) appendText(*root, "\xE2\x89\x88");      // ≈ numerical method
            appendNumber(*root, e.value.real(), s.display);
            return root;
        }
    }
    // Complex, base-N, DMS and pair results are shown as plain text tokens.
    appendText(*root, resultText(e, s));
    return root;
}

std::string resultExpression(const HistoryEntry& e) {
    char buf[96];
    if (e.collection) {
        const CollectionResult& c = *e.collection;
        const CalcValue& v = c.value;
        // Un risultato tenuto solo nella memoria delle risposte si riusa attraverso di essa.
        if (v.empty()) return v.isList() ? "List Ans" : v.type == ValueType::Vector ? "Vct Ans" : "Mat Ans";
        const bool exactForms = c.exact.size() == v.size();
        std::string out = v.isList() ? "{" : "[";
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (v.isList()) { if (i) out += ","; }
            else out += i % v.cols == 0 ? "[" : ",";
            if (exactForms) out += exact::toExpression(c.exact[i]);
            else {
                const Complex z = v.at(i);
                if (z.imag() == 0) std::snprintf(buf, sizeof(buf), "%.15g", z.real());
                else std::snprintf(buf, sizeof(buf), "(%.15g%+.15gi)", z.real(), z.imag());
                out += buf;
            }
            if (!v.isList() && i % v.cols == v.cols - 1u) out += "]";
        }
        return out + (v.isList() ? "}" : "]");
    }
    // Un intero Base-N si riusa con il prefisso decimale (valido in ogni base).
    if (e.base != 0 && plainResult(e)) { const long long v = static_cast<long long>(e.value.real()); return (v < 0 ? "-0d" : "0d") + std::to_string(std::llabs(v)); }
    if (e.exact.valid && plainResult(e)) return exact::toExpression(e.exact.value);
    if (e.value.imag() == 0) { std::snprintf(buf, sizeof(buf), "%.15g", e.value.real()); return buf; }
    std::snprintf(buf, sizeof(buf), "(%.15g%+.15gi)", e.value.real(), e.value.imag());
    return buf;
}
} // namespace fx
