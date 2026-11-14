#include "run_matrix.hpp"
#include "limits.hpp"
#include "math_engine.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <numeric>

namespace fx {
namespace {
const char* const kArrow = "\xE2\x86\x92";   // U+2192

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
// ---------------------------------------------------------------- destinazioni della memorizzazione
// Cosa indica la freccia di memorizzazione: A, A~F, Mat A, Vct A, List 1, un
// loro elemento, o "Dim Mat A" (una lista {righe, colonne} crea una matrice di zeri).
struct StoreTarget {
    enum class Kind : std::uint8_t { None, Variables, Memory, Element, Dim } kind{Kind::None};
    char first{}, last{};                                   // Variables
    CollectionResolver::Memory memory{CollectionResolver::Memory::Mat};
    int index{};                                            // 0..25
    ast::NodePtr row, column;                               // Element: espressioni degli indici (column: solo matrici)
    std::string label;                                      // "Mat A", "List 1[3]"...
    ErrorLocation where;                                    // intervallo della freccia e della destinazione nell'input
};
bool memoryName(const std::string& text, CollectionResolver::Memory& memory, int& index) {
    using Memory = CollectionResolver::Memory;
    // Il parser legge List 1-6 come le variabili di lista condivise "list1".."list6".
    if (text.size() == 5 && text.compare(0, 4, "list") == 0 && text[4] >= '1' && text[4] <= '6') { memory = Memory::List; index = text[4] - '1'; return true; }
    const std::size_t space = text.find(' ');
    if (space == std::string::npos || space + 1 >= text.size()) return false;
    const std::string word = text.substr(0, space), name = text.substr(space + 1);
    if (word == "Mat") memory = Memory::Mat; else if (word == "Vct") memory = Memory::Vct; else if (word == "List") memory = Memory::List; else return false;
    if (name == "Ans") { index = CollectionResolver::Answer; return true; }
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
            if (t.memory == CollectionResolver::Memory::List) t.label = "List " + std::to_string(t.index + 1);
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
void storeElement(Workspace& ws, const StoreTarget& t, Complex value, const EvalContext& ctx) {
    using Memory = CollectionResolver::Memory;
    const std::string name = t.label.substr(0, t.label.find('['));
    if (t.memory == Memory::List) {
        if (t.column) throw SyntaxError("Argument ERROR: una lista ha un solo indice, es. List 1[3]");
        CalcValue list;
        if (!ws.collection(Memory::List, t.index, list)) { list = CalcValue{}; list.type = ValueType::List; list.cols = 1; }
        const std::size_t i = indexValue(t.row, ctx, "l'indice");
        // Come sulla fx-CG50 una lista cresce di una cella alla volta.
        if (i > list.size() + 1) throw DimensionError("Dimension ERROR: indice " + std::to_string(i) + " oltre la fine di " + name);
        if (value.imag() != 0) throw MathError("Math ERROR: le liste 1-6 contengono solo numeri reali");
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
    m.set(pos, value);
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
// Elementi per cui si tenta la valutazione esatta (che ripete il calcolo).
constexpr std::size_t kExactElements = 64;

void scalarResult(Workspace& ws, const std::string& raw, const ast::Node& tree, const EvalContext& ctx, const EvalInfo& info,
                  const StoreTarget& target, Complex value, RunOutcome& out, HistoryEntry& entry) {
    out.value = value;
    // Pol / Rec at the top level produce a pair, like the fx-CG50.
    if (tree.kind == ast::Kind::Call && (tree.text == "pol" || tree.text == "rec") && tree.children.size() == 2) {
        const double a = evaluateReal(*tree.children[0], ctx), b = evaluateReal(*tree.children[1], ctx);
        const DisplayFormat f = ws.settings.display;
        if (tree.text == "pol") {
            const auto p = rectToPolar(a, b);
            out.pairText = "r=" + formatReal(p.first, f) + "  \xCE\xB8=" + formatReal(p.second, f);
        } else {
            const auto p = polarToRect(a, b);
            out.pairText = "x=" + formatReal(p.first, f) + "  y=" + formatReal(p.second, f);
        }
    }
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
            storeElement(ws, target, out.value, ctx);
            out.storedIn = target.label;
            break;
        case StoreTarget::Kind::Memory:
        case StoreTarget::Kind::Dim:
            throw TypeError("Type ERROR: in " + target.label + " si memorizza una " +
                            (target.memory == CollectionResolver::Memory::List ? "lista {1,2,3}" : "matrice [[1,2][3,4]]") + ", non un numero");
        }
    });
    if (slot.valid) ws.memory.setAnswerExact(out.value.real(), slot.value, slot.decimal);
    else ws.memory.setAnswer(out.value.real(), out.value.imag());
    entry = HistoryEntry{};
    entry.expression = raw;
    entry.value = out.value;
    entry.exact = slot;
    entry.numerical = info.numerical;
    if (!out.pairText.empty()) entry.error = std::string("=") + out.pairText;   // '=' prefix: informative text, not an error
}

void collectionResult(Workspace& ws, const std::string& raw, const ast::Node& tree, const EvalContext& ctx,
                      const StoreTarget& target, CalcValue value, RunOutcome& out, HistoryEntry& entry) {
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
            throw TypeError(std::string("Type ERROR: una ") + valueTypeName(value.type) + " non si memorizza in una variabile A-Z (usa Mat, Vct o List)");
        case StoreTarget::Kind::Element:
            throw TypeError("Type ERROR: in " + target.label + " si memorizza un numero");
        case StoreTarget::Kind::Memory:
            if (target.memory == Memory::List) ws.storeList(slot, value);
            else if (target.memory == Memory::Mat) ws.storeMatrix(slot, value);
            else ws.storeVector(slot, value);
            out.storedIn = target.label;
            break;
        case StoreTarget::Kind::Dim: {
            // {righe, colonne} -> Dim Mat A: una nuova matrice piena di zeri.
            if (target.memory == Memory::List || !value.isList() || value.size() != 2 || value.complex())
                throw TypeError("Type ERROR: {righe,colonne}\xE2\x86\x92" "Dim Mat A crea una matrice di zeri");
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
    // La cronologia conserva cio' che mostra la vista naturale; i risultati piu'
    // grandi si leggono dalla memoria delle risposte (la riga indica dove sono).
    if (naturalSize(value)) result->value = std::move(value);
    else { result->value.type = value.type; result->value.rows = value.rows; result->value.cols = value.cols; result->exact.clear(); }
    entry.collection = std::move(result);
}
} // namespace

RunOutcome evaluateEntry(Workspace& ws, const std::string& raw, HistoryEntry& entry) {
    const std::size_t lead = raw.find_first_not_of(" \t");
    const int offset = lead == std::string::npos ? 0 : static_cast<int>(lead);
    std::string expr = trim(raw);
    RunOutcome out;
    EvalInfo info;
    try {
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
        }
        ws.apply();
        const auto tree = parseExpression(expr);
        EvalContext ctx = ws.evalContext();
        ctx.info = &info;
        CalcValue value = evaluateAny(*tree, ctx);
        if (value.isScalar()) scalarResult(ws, raw, *tree, ctx, info, target, value.scalar(), out, entry);
        else collectionResult(ws, raw, *tree, ctx, target, std::move(value), out, entry);
    }
    catch (SyntaxError& e) { locate(e.where, info, offset); throw; }
    catch (MathError& e) { locate(e.where, info, offset); throw; }
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
            if (h.collection) continue;
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
        try {
            (void)evaluateEntry(ws, hist[i].expression, fresh);
            fresh.view = static_cast<std::uint8_t>(hist[i].view == ViewDecimal || hist[i].view == ViewExact ? hist[i].view : static_cast<std::uint8_t>(ViewAuto));
            fresh.flipMixed = hist[i].flipMixed;
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
    return e.view == ViewAuto && s.fractionResults && !c.decimal;
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
    if (!plainResult(e) || !e.exact.valid || isIntegerExact(e)) return false;
    if (e.view == ViewExact) return exact::fitsRelaxed(e.exact.value);
    if (e.view != ViewAuto || !s.fractionResults || e.exact.decimal) return false;
    return exact::fitsNatural(e.exact.value, s.mixedFractions != e.flipMixed);
}
bool showsMixed(const HistoryEntry& e, const Settings& s) {
    exact::Rational r;
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
    const Complex z = e.collection->value.at(i);
    if (z.imag() == 0) appendNumber(*row, z.real(), s.display);
    else appendText(*row, formatValue(z, s.complex == ComplexMode::Real ? ComplexMode::Rectangular : s.complex, s.display));
    return row;
}
} // namespace

std::string elementText(const HistoryEntry& e, std::size_t i, const Settings& s) {
    if (!e.collection || i >= e.collection->value.size()) return {};
    if (const exact::Value* x = elementExact(e, i, s)) return exact::toText(*x, s.mixedFractions);
    const Complex z = e.collection->value.at(i);
    return formatValue(z, z.imag() != 0 && s.complex == ComplexMode::Real ? ComplexMode::Rectangular : s.complex, s.display);
}

std::string resultText(const HistoryEntry& e, const Settings& s) {
    if (!e.error.empty() && e.error[0] == '=') return e.error.substr(1);
    if (!e.error.empty()) return e.error;
    if (e.collection) return collectionText(e, s);
    if (e.value.imag() == 0) {
        if (e.view >= ViewHex && e.view <= ViewOct) return baseText(e);
        if (e.view == ViewDms) return dmsText(e, s);
        exact::Rational r;
        if (e.view == ViewPeriodic && rationalOf(e, r)) return periodicText(r);
        if (showsExact(e, s)) return exact::toText(e.exact.value, showsMixed(e, s));
    }
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
    if (e.exact.valid && plainResult(e)) return exact::toExpression(e.exact.value);
    if (e.value.imag() == 0) { std::snprintf(buf, sizeof(buf), "%.15g", e.value.real()); return buf; }
    std::snprintf(buf, sizeof(buf), "(%.15g%+.15gi)", e.value.real(), e.value.imag());
    return buf;
}
} // namespace fx
