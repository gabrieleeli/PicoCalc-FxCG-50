#include "workspace.hpp"
#include "mem_pools.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace fx {
Workspace::Workspace() { resetAll(); }

void Workspace::resetAll() {
    settings = Settings{};
    memory.clearAll();
    history.clear();
    for (std::size_t i = 0; i < functions.size(); ++i) functions[i] = emptyGraphFunction(i);
    functions[0].expr = "sin(x)";
    view = ViewWindow{};
    graph = GraphState{};
    table = TableRange{};
    for (auto& l : lists) l.clear();
    for (auto& n : listNames) n.clear();
    equation = EquationState{};
    chat.clear();
    graph3d = Graph3DState{};
    matrices.clear();
}

// ---------------------------------------------------------------- matrici, vettori, liste
void CollectionMemory::clear() {
    for (CalcValue& v : mat) v = CalcValue{};
    for (CalcValue& v : vct) v = CalcValue{};
    matAns = vctAns = listAns = CalcValue{};
}
std::size_t CollectionMemory::elements() const noexcept {
    std::size_t total = matAns.size() + vctAns.size();
    for (const CalcValue& v : mat) total += v.size();
    for (const CalcValue& v : vct) total += v.size();
    return total;
}
namespace {
// Una matrice o un vettore memorizzabile: forma e limiti della scheda.
void checkStorable(const CalcValue& v, const char* what) {
    if (!v.isMatrixLike() || v.empty() || static_cast<std::size_t>(v.rows) * v.cols != v.size())
        throw TypeError(std::string("Type ERROR: in ") + what + " si memorizza una matrice, non una " + valueTypeName(v.type));
    if (v.rows > limits::kMatrixMaxDim || v.cols > limits::kMatrixMaxDim || v.size() > limits::kMatrixElements)
        throw DimensionError("Dimension ERROR: matrice troppo grande (massimo " + std::to_string(limits::kMatrixMaxDim) +
                             " per lato e " + std::to_string(limits::kMatrixElements) + " elementi)");
}
// Le memorie insieme restano nel budget della scheda (RAM e record in flash).
void checkBudget(const CollectionMemory& m, std::size_t freed, std::size_t added) {
    const std::size_t used = m.elements() - std::min(freed, m.elements());
    if (used + added > limits::kMatrixStoreElements)
        throw MathError("Memory ERROR: memoria di matrici e vettori piena (" + std::to_string(used + added) + " elementi, massimo " +
                        std::to_string(limits::kMatrixStoreElements) + ")");
}
} // namespace

bool Workspace::collection(Memory memory, int index, CalcValue& out) const {
    const CalcValue* src = nullptr;
    if (memory == Memory::List) {
        if (index == Answer) src = &matrices.listAns;
        else if (index >= 0 && static_cast<std::size_t>(index) < lists.size()) {
            const auto& l = lists[static_cast<std::size_t>(index)];
            if (l.empty()) return false;
            mem::PlacementScope heavy(mem::Policy::PREFER_PSRAM);
            out = CalcValue{};
            out.type = ValueType::List; out.rows = static_cast<std::uint16_t>(l.size()); out.cols = 1;
            out.re = l;
            return true;
        } else return false;
    } else {
        const auto& slots = memory == Memory::Mat ? matrices.mat : matrices.vct;
        if (index == Answer) src = memory == Memory::Mat ? &matrices.matAns : &matrices.vctAns;
        else if (index >= 0 && static_cast<std::size_t>(index) < slots.size()) src = &slots[static_cast<std::size_t>(index)];
        else return false;
    }
    if (src->empty()) return false;
    mem::PlacementScope heavy(mem::Policy::PREFER_PSRAM);
    out = *src;
    return true;
}
void Workspace::storeMatrix(std::size_t slot, CalcValue value) {
    if (slot >= matrices.mat.size()) throw std::out_of_range("Mat A-Z");
    checkStorable(value, "Mat");
    checkBudget(matrices, matrices.mat[slot].size(), value.size());
    value.type = ValueType::Matrix;
    value.normalize();
    if (!value.hasExact() || value.complex() || value.size() > CalcValue::ExactElements) value.exact.clear();
    matrices.mat[slot] = std::move(value);
}
void Workspace::storeVector(std::size_t slot, CalcValue value) {
    if (slot >= matrices.vct.size()) throw std::out_of_range("Vct A-Z");
    checkStorable(value, "Vct");
    if (!value.vectorShaped())
        throw DimensionError("Dimension ERROR: un vettore ha una riga o una colonna, non " + dimensionText(value));
    checkBudget(matrices, matrices.vct[slot].size(), value.size());
    value.type = ValueType::Vector;
    value.normalize();
    if (!value.hasExact() || value.complex() || value.size() > CalcValue::ExactElements) value.exact.clear();
    matrices.vct[slot] = std::move(value);
}
void Workspace::storeList(std::size_t index, const CalcValue& value) {
    if (index >= lists.size()) throw MathError("Math ERROR: sono disponibili List 1-" + std::to_string(lists.size()));
    if (!value.isList() || value.empty()) throw TypeError(std::string("Type ERROR: in List si memorizza una lista, non una ") + valueTypeName(value.type));
    if (value.complex() && std::any_of(value.im.begin(), value.im.end(), [](double d) { return d != 0.0; }))
        throw MathError("Math ERROR: le liste contengono solo numeri reali");
    checkListLength(index, value.size());
    mem::PlacementScope heavy(mem::Policy::PREFER_PSRAM);
    std::vector<double> fresh = value.re;       // strong guarantee on std::bad_alloc
    lists[index].swap(fresh);
}
void Workspace::setList(std::size_t index, std::vector<double> values) {
    if (index >= lists.size()) throw MathError("Argument ERROR: List da 1 a " + std::to_string(lists.size()));
    checkListLength(index, values.size());
    for (double d : values) if (!std::isfinite(d)) throw MathError("Math ERROR: valore non finito in una lista");
    lists[index].swap(values);
}
std::size_t Workspace::listElements() const noexcept {
    std::size_t n = 0;
    for (const auto& l : lists) n += l.size();
    return n;
}
bool Workspace::listFits(std::size_t index, std::size_t length) const noexcept {
    if (index >= lists.size() || length > MaxListLength) return false;
    return listElements() - lists[index].size() + length <= MaxListElements;
}
void Workspace::checkListLength(std::size_t index, std::size_t length) const {
    if (index >= lists.size()) throw MathError("Argument ERROR: List da 1 a " + std::to_string(lists.size()));
    if (length > MaxListLength) throw DimensionError("Dimension ERROR: massimo " + std::to_string(MaxListLength) + " elementi in una lista");
    const std::size_t used = listElements() - lists[index].size();
    if (used + length > MaxListElements)
        throw MathError("Memory ERROR: memoria delle liste piena (" + std::to_string(used + length) + " elementi, massimo " +
                        std::to_string(MaxListElements) + ")");
}
const std::string& Workspace::listName(std::size_t index) const {
    static const std::string none;
    return index < listNames.size() ? listNames[index] : none;
}
void Workspace::setListName(std::size_t index, std::string name) {
    if (index >= listNames.size()) throw MathError("Argument ERROR: List da 1 a " + std::to_string(listNames.size()));
    if (name.size() > limits::kListNameLength) throw MathError("Argument ERROR: un sottonome ha al massimo " + std::to_string(limits::kListNameLength) + " caratteri");
    for (char c : name) if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E || c == '"')
        throw MathError("Argument ERROR: sottonome con caratteri non ammessi");
    listNames[index] = std::move(name);
}
void Workspace::storeCollectionAnswer(const CalcValue& value) {
    mem::PlacementScope heavy(mem::Policy::PREFER_PSRAM);
    if (value.isList()) {
        matrices.listAns = value; matrices.listAns.normalize();
        if (!matrices.listAns.hasExact() || matrices.listAns.size() > CalcValue::ExactElements) matrices.listAns.exact.clear();
        return;
    }
    if (!value.isMatrixLike()) return;
    checkStorable(value, "MatAns");
    const bool vector = value.vectorShaped();
    checkBudget(matrices, matrices.matAns.size() + (vector ? matrices.vctAns.size() : 0), value.size() * (vector ? 2 : 1));
    CalcValue m = value;
    m.type = ValueType::Matrix; m.normalize();
    if (!m.hasExact() || m.complex() || m.size() > CalcValue::ExactElements) m.exact.clear();
    if (vector) { CalcValue v = m; v.type = ValueType::Vector; matrices.vctAns = std::move(v); }
    matrices.matAns = std::move(m);
}
std::unique_ptr<Workspace::CollectionSnapshot> Workspace::snapshotCollections() const {
    mem::PlacementScope heavy(mem::Policy::PREFER_PSRAM);
    auto s = std::make_unique<CollectionSnapshot>();
    s->matrices = matrices;
    s->lists = lists;
    return s;
}
void Workspace::restoreCollections(CollectionSnapshot& snapshot) {
    matrices = std::move(snapshot.matrices);
    lists = std::move(snapshot.lists);
}

void Workspace::apply() const { setAngleMode(settings.angle); }

EvalContext Workspace::evalContext() const {
    EvalContext c;
    c.memory = &memory;
    c.complexMode = settings.complex;
    c.names = this;
    c.collections = this;
    return c;
}

bool Workspace::resolve(const std::string& n, double& out) const {
    const struct { const char* name; double value; } vars[] = {
        {"xmin", view.xmin}, {"xmax", view.xmax}, {"xscl", view.xscl}, {"ymin", view.ymin}, {"ymax", view.ymax},
        {"yscl", view.yscl}, {"tmin", view.tmin}, {"tmax", view.tmax}, {"tptch", view.tstep},
        {"tblstart", table.start}, {"tblend", table.end}, {"tblpitch", table.step}, {"xdot", viewXdot(view)}};
    for (const auto& v : vars) if (n == v.name) { out = v.value; return true; }
    return false;
}

void Workspace::addHistory(HistoryEntry entry) {
    history.push_back(std::move(entry));
    if (history.size() > MaxHistory) history.erase(history.begin());
}

// ---------------------------------------------------------------- serialisation
namespace {
constexpr std::uint32_t Magic = 0x46584157;   // "WAXF"
// Version 2 (v1.7.0) adds exact values, the Frac Result setting and complex Ans;
// version 3 (v1.8.0) the GRAPH data: 20 functions with line style and the
// parametric Yt, Setup items, V-Window/graph memories, sketches, overlay and
// project name. Version 1 (v1.6.0), 2 and 3 records are still accepted.
// Version 4 (v1.8.0, TABLE completion) adds the table variable, the table
// function type and the generated (possibly edited) rows.
// Version 5 (v1.9.0) appends the file store setting and the 3D Graph data.
// Version 6 (v1.10.0) aggiunge in coda la modalita' Input/Output, le memorie
// di matrici e vettori (Mat A-Z, Vct A-Z, MatAns, VctAns, ListAns) e i
// risultati lista / matrice / vettore della cronologia. I record delle
// versioni 1-5 si caricano con i valori predefiniti: Math, nessuna matrice.
// Version 7 (v1.11.0) aggiunge in coda: modalita' Base-N, Simplify, formato
// CSV e liste di Statistics; List 7-26 (List 1-6 restano dove erano) e i
// sottonomi delle 26 liste; le forme esatte delle memorie di matrici e
// vettori; base e frazione non ridotta (▶Simp) delle righe della cronologia;
// i grafici dei comandi di Run-Matrix. I record delle versioni 1-6 si caricano
// con i valori predefiniti di questi campi.
constexpr std::uint16_t Version = 7;
constexpr std::size_t LegacyLists = 6;     // liste del blocco delle versioni 1-6
constexpr std::uint8_t NoValue = 0xFF;      // byte di tipo di una memoria non definita
constexpr std::size_t LegacyFunctions = 6;

struct Writer {
    std::vector<std::uint8_t> out;
    void u8(std::uint8_t v) { out.push_back(v); }
    void u16(std::uint16_t v) { u8(static_cast<std::uint8_t>(v)); u8(static_cast<std::uint8_t>(v >> 8)); }
    void u32(std::uint32_t v) { u16(static_cast<std::uint16_t>(v)); u16(static_cast<std::uint16_t>(v >> 16)); }
    void f64(double v) { std::uint64_t b; std::memcpy(&b, &v, 8); u32(static_cast<std::uint32_t>(b)); u32(static_cast<std::uint32_t>(b >> 32)); }
    void str(const std::string& s) { const auto n = static_cast<std::uint16_t>(std::min<std::size_t>(s.size(), 4000)); u16(n); out.insert(out.end(), s.begin(), s.begin() + n); }
    void blob(const std::string& s) { u32(static_cast<std::uint32_t>(s.size())); out.insert(out.end(), s.begin(), s.end()); }
    void f32(float v) { std::uint32_t b; std::memcpy(&b, &v, 4); u32(b); }
    void i64(std::int64_t v) { const auto b = static_cast<std::uint64_t>(v); u32(static_cast<std::uint32_t>(b)); u32(static_cast<std::uint32_t>(b >> 32)); }
    void slot(const ExactSlot& e) {
        u8(static_cast<std::uint8_t>((e.valid ? 1 : 0) | (e.decimal ? 2 : 0) | (e.value.form() == exact::Value::Form::Pi ? 4 : 0)));
        u8(static_cast<std::uint8_t>(e.value.termCount()));
        for (std::size_t i = 0; i < e.value.termCount(); ++i) { const auto& t = e.value.term(i); i64(t.coef.num); i64(t.coef.den); i64(t.radicand); }
    }
    // Lista / matrice / vettore: tipo, righe, colonne, indicatori (1 = parti
    // immaginarie, 2 = elementi omessi), poi gli elementi (8 byte l'uno, 16 se
    // complessi).
    void value(const CalcValue& v, bool withElements = true) {
        if (v.rows == 0 || v.cols == 0 || (withElements && v.empty())) { u8(NoValue); return; }
        const bool elements = withElements && !v.empty();
        u8(static_cast<std::uint8_t>(v.type)); u16(v.rows); u16(v.cols);
        u8(static_cast<std::uint8_t>((elements && v.complex() ? 1 : 0) | (elements ? 0 : 2)));
        if (!elements) return;
        for (double d : v.re) f64(d);
        if (v.complex()) for (double d : v.im) f64(d);
    }
};
struct Reader {
    const std::uint8_t* p; std::size_t n; std::size_t i{}; bool ok{true};
    bool need(std::size_t k) { if (!ok || i + k > n) { ok = false; return false; } return true; }
    std::uint8_t u8() { if (!need(1)) return 0; return p[i++]; }
    std::uint16_t u16() { const std::uint16_t a = u8(); return static_cast<std::uint16_t>(a | (u8() << 8)); }
    std::uint32_t u32() { const std::uint32_t a = u16(); return a | (static_cast<std::uint32_t>(u16()) << 16); }
    double f64() { const std::uint64_t a = u32(); const std::uint64_t b = u32(); const std::uint64_t bits = a | (b << 32); double v; std::memcpy(&v, &bits, 8); return v; }
    std::string str() { const std::uint16_t k = u16(); if (!need(k)) return {}; std::string s(reinterpret_cast<const char*>(p + i), k); i += k; return s; }
    std::string blob(std::size_t max) { const std::uint32_t k = u32(); if (k > max) { ok = false; return {}; } if (!need(k)) return {}; std::string s(reinterpret_cast<const char*>(p + i), k); i += k; return s; }
    float f32() { const std::uint32_t b = u32(); float v; std::memcpy(&v, &b, 4); return v; }
    std::int64_t i64() { const std::uint64_t a = u32(); const std::uint64_t b = u32(); return static_cast<std::int64_t>(a | (b << 32)); }
    // Exact slots are rebuilt through the normalising constructors, so a
    // corrupted record can never produce a non-canonical value.
    ExactSlot slot() {
        ExactSlot e;
        const std::uint8_t flags = u8(), count = u8();
        if (count > exact::Value::MaxTerms || flags > 7) { ok = false; return e; }
        exact::Term t[exact::Value::MaxTerms]{};
        for (std::uint8_t k = 0; k < count; ++k) {
            const std::int64_t num = i64(), den = i64(), rad = i64();
            if (!exact::Rational::make(num, den, t[k].coef) || !(t[k].coef == exact::Rational{num, den}) || rad < 1 || rad > exact::Value::MaxRadicand) { ok = false; return e; }
            t[k].radicand = rad;
        }
        if (flags & 4) {
            if (count != 1 || t[0].radicand != 1) { ok = false; return e; }
            e.value = exact::Value::piTimes(t[0].coef);
        } else if (!exact::Value::fromTerms(t, count, e.value) || e.value.termCount() != count) { ok = false; return e; }
        e.valid = (flags & 1) != 0; e.decimal = (flags & 2) != 0;
        return e;
    }
    // Legge cio' che Writer::value() ha scritto e lo controlla con i limiti di
    // questa scheda (un record lo legge solo la scheda che l'ha scritto).
    // allowOmitted: i risultati della cronologia possono avere solo tipo e dimensioni.
    bool value(CalcValue& out, bool allowOmitted = false) {
        out = CalcValue{};
        const std::uint8_t type = u8();
        if (!ok) return false;
        if (type == NoValue) return true;
        if (type < 1 || type > 3) return ok = false;
        const std::uint16_t rows = u16(), cols = u16();
        const std::uint8_t flags = u8();
        if (!ok || flags > 3 || rows < 1 || cols < 1) return ok = false;
        const std::size_t count = static_cast<std::size_t>(rows) * cols;
        const bool list = type == static_cast<std::uint8_t>(ValueType::List);
        if (list ? (cols != 1 || rows > limits::kListMaxLength)
                 : (rows > limits::kMatrixMaxDim || cols > limits::kMatrixMaxDim || count > limits::kMatrixElements)) return ok = false;
        out.type = static_cast<ValueType>(type); out.rows = rows; out.cols = cols;
        if (flags & 2) return allowOmitted ? true : (ok = false);
        if (!need(count * ((flags & 1) ? 16 : 8))) return false;
        out.re.resize(count);
        for (double& d : out.re) { d = f64(); if (!std::isfinite(d)) return ok = false; }
        if (flags & 1) { out.im.resize(count); for (double& d : out.im) { d = f64(); if (!std::isfinite(d)) return ok = false; } }
        return ok;
    }
};
} // namespace

std::vector<std::uint8_t> serializeWorkspace(const Workspace& ws) {
    Writer w;
    w.u32(Magic); w.u16(Version);
    const Settings& s = ws.settings;
    w.u8(static_cast<std::uint8_t>(s.angle)); w.u8(static_cast<std::uint8_t>(s.display.mode)); w.u8(s.display.digits);
    w.u8(static_cast<std::uint8_t>(s.complex)); w.u8(s.fractionResults ? 1 : 0); w.u8(s.lcdBacklight); w.u8(s.kbdBacklight);
    w.str(s.wifiSsid); w.str(s.wifiPassword); w.str(s.relayHost); w.u16(s.relayPort); w.str(s.relayPath); w.str(s.relayToken);
    w.u8(s.aiProvider); w.str(s.aiModel); w.u16(s.aiMaxChars);
    w.u8(s.mixedFractions ? 1 : 0);
    for (char v = 'A'; v <= 'Z'; ++v) { w.f64(ws.memory.recall(v)); w.slot(ws.memory.exactSlot(v)); }
    const CalculatorMemory& m = ws.memory;
    w.f64(m.answer()); w.f64(m.answerImag()); w.slot(m.exactAnswerSlot());
    w.f64(m.previousAnswer()); w.f64(m.previousAnswerImag()); w.slot(m.exactPreviousAnswerSlot());
    w.u16(static_cast<std::uint16_t>(ws.history.size()));
    for (const auto& h : ws.history) {
        w.str(h.expression); w.f64(h.value.real()); w.f64(h.value.imag()); w.str(h.error); w.u8(h.view);
        w.slot(h.exact); w.u8(static_cast<std::uint8_t>((h.numerical ? 1 : 0) | (h.flipMixed ? 2 : 0)));
    }
    w.u8(static_cast<std::uint8_t>(ws.functions.size()));
    for (const auto& f : ws.functions) {
        w.str(f.expr); w.u8(static_cast<std::uint8_t>(f.type)); w.u8(f.selected ? 1 : 0); w.u8(f.color);
        w.u8(static_cast<std::uint8_t>(f.style)); w.str(f.expr2);
    }
    auto view = [&](const ViewWindow& v) { for (double d : {v.xmin, v.xmax, v.xscl, v.ymin, v.ymax, v.yscl, v.tmin, v.tmax, v.tstep}) w.f64(d); };
    view(ws.view);
    const GraphState& g = ws.graph;
    const GraphSettings& gs = g.settings;
    w.u8(static_cast<std::uint8_t>(gs.funcType));
    w.u8(static_cast<std::uint8_t>((gs.plotDraw ? 1 : 0) | (gs.derivative ? 2 : 0) | (gs.coord ? 4 : 0) | (gs.label ? 8 : 0) |
                                   (gs.graphFunc ? 16 : 0) | (gs.simulGraph ? 32 : 0) | (gs.ineqUnion ? 64 : 0)));
    w.u8(static_cast<std::uint8_t>(gs.grid)); w.u8(static_cast<std::uint8_t>(gs.axes));
    w.u8(static_cast<std::uint8_t>(gs.sketchLine)); w.u8(gs.plotColor);
    w.f64(gs.xFactor); w.f64(gs.yFactor);
    w.u8(gs.background); w.str(gs.backgroundFile); w.u8(gs.fade);
    w.u8(g.vmemUsed);
    for (const auto& m : g.vmem) view(m);
    for (const auto& m : g.gmem) w.blob(m);
    w.u16(static_cast<std::uint16_t>(g.sketch.size()));
    for (const auto& s : g.sketch) {
        w.u8(static_cast<std::uint8_t>(s.kind)); w.f64(s.x0); w.f64(s.y0); w.f64(s.x1); w.f64(s.y1);
        w.u8(s.color); w.u8(static_cast<std::uint8_t>(s.style)); w.str(s.text);
        w.u16(static_cast<std::uint16_t>(s.path.size())); for (float f : s.path) w.f32(f);
    }
    w.str(g.overlay); w.str(g.project); w.u8(g.projectModified ? 1 : 0);
    w.f64(ws.table.start); w.f64(ws.table.end); w.f64(ws.table.step);
    w.u8(static_cast<std::uint8_t>(ws.table.variable)); w.u8(ws.table.list); w.u8(static_cast<std::uint8_t>(ws.table.funcType));
    w.u8(ws.table.generated ? 1 : 0);
    w.u16(static_cast<std::uint16_t>(ws.table.rows.size())); for (double d : ws.table.rows) w.f64(d);
    for (std::size_t li = 0; li < LegacyLists; ++li) { const auto& l = ws.lists[li]; w.u16(static_cast<std::uint16_t>(l.size())); for (double d : l) w.f64(d); }
    const EquationState& e = ws.equation;
    w.u8(e.unknowns); for (double d : e.simultaneous) w.f64(d);
    w.u8(e.degree); for (double d : e.polynomial) w.f64(d);
    w.str(e.solverExpr); w.f64(e.solverGuess);
    w.u8(e.ineqDegree); w.u8(e.ineqRelation); for (double d : e.inequality) w.f64(d);
    w.u16(static_cast<std::uint16_t>(ws.chat.size()));
    for (const auto& m : ws.chat) { w.u8(m.fromUser ? 1 : 0); w.str(m.text); }
    // v5
    w.u8(static_cast<std::uint8_t>(ws.settings.fileStore));
    const auto& g3 = ws.graph3d;
    w.u8(static_cast<std::uint8_t>(models::Graph3DModel::Functions));
    for (std::size_t i = 0; i < models::Graph3DModel::Functions; ++i) { w.str(g3.model.z[i]); w.u8(g3.model.selected[i] ? 1 : 0); }
    for (double d : {g3.model.xmin, g3.model.xmax, g3.model.ymin, g3.model.ymax, g3.model.zmin, g3.model.zmax}) w.f64(d);
    w.u8(static_cast<std::uint8_t>(g3.model.grid));
    w.f32(g3.yaw); w.f32(g3.pitch); w.f32(g3.zoom);
    // v6
    w.u8(static_cast<std::uint8_t>(ws.settings.inputMode));
    const CollectionMemory& cm = ws.matrices;
    for (const CalcValue& v : cm.mat) w.value(v);
    for (const CalcValue& v : cm.vct) w.value(v);
    w.value(cm.matAns); w.value(cm.vctAns); w.value(cm.listAns);
    std::uint16_t results = 0;
    for (const auto& h : ws.history) if (h.collection) ++results;
    w.u16(results);
    for (std::size_t i = 0; i < ws.history.size(); ++i) {
        const auto& c = ws.history[i].collection;
        if (!c) continue;
        w.u16(static_cast<std::uint16_t>(i));
        w.value(c->value, !c->value.empty());
        const bool exact = !c->value.empty() && c->exact.size() == c->value.size();
        w.u8(static_cast<std::uint8_t>((exact ? 1 : 0) | (c->decimal ? 2 : 0)));
        if (exact) for (const exact::Value& e : c->exact) w.slot(ExactSlot{e, true, false});
    }
    // v7
    const Settings& st = ws.settings;
    w.u8(static_cast<std::uint8_t>(st.baseMode)); w.u8(st.simplifyManual ? 1 : 0);
    w.u8(static_cast<std::uint8_t>(st.csvSeparator)); w.u8(static_cast<std::uint8_t>(st.csvDecimal));
    w.u8(st.statXList); w.u8(st.statYList);
    for (std::size_t li = LegacyLists; li < Workspace::ListCount; ++li) {
        const auto& l = ws.lists[li]; w.u16(static_cast<std::uint16_t>(l.size())); for (double d : l) w.f64(d);
    }
    for (const auto& n : ws.listNames) w.str(n);
    auto exactForms = [&](const CalcValue& v) {
        const bool has = v.hasExact();
        w.u8(has ? 1 : 0);
        if (has) for (const exact::Value& e : v.exact) w.slot(ExactSlot{e, true, false});
    };
    for (const CalcValue& v : cm.mat) exactForms(v);
    for (const CalcValue& v : cm.vct) exactForms(v);
    exactForms(cm.matAns); exactForms(cm.vctAns); exactForms(cm.listAns);
    std::uint16_t extras = 0;
    auto extra = [](const HistoryEntry& h) { return h.base != 0 || h.fracDen != 0 || h.simpFactor != 0 || h.command; };
    for (const auto& h : ws.history) if (extra(h)) ++extras;
    w.u16(extras);
    for (std::size_t i = 0; i < ws.history.size(); ++i) {
        const HistoryEntry& h = ws.history[i];
        if (!extra(h)) continue;
        w.u16(static_cast<std::uint16_t>(i)); w.u8(h.base); w.i64(h.fracNum); w.i64(h.fracDen); w.u32(h.simpFactor);
        w.u8(h.command ? 1 : 0);
    }
    w.u8(static_cast<std::uint8_t>(g.commandGraphs.size()));
    for (const CommandGraph& cg : g.commandGraphs) {
        w.u8(static_cast<std::uint8_t>(cg.type)); w.str(cg.expr); w.str(cg.expr2); w.u8(cg.color); w.u8(static_cast<std::uint8_t>(cg.style));
    }
    return std::move(w.out);
}

bool deserializeWorkspace(const std::uint8_t* data, std::size_t size, Workspace& out) {
    if (!data) return false;
    Reader r{data, size};
    if (r.u32() != Magic || !r.ok) return false;
    const std::uint16_t version = r.u16();
    if (version < 1 || version > Version || !r.ok) return false;
    const bool v2 = version >= 2;
    // The workspace being decoded lives on the heap (PSRAM on the Pico Plus
    // 2 W): as a local it took about 5.4 KB of the 32 KB stack.
    const auto holder = std::make_unique<Workspace>();
    Workspace& ws = *holder;
    Settings& s = ws.settings;
    const auto angle = r.u8(); if (angle > 2) return false; s.angle = static_cast<AngleMode>(angle);
    const auto dmode = r.u8(); if (dmode > 4) return false; s.display.mode = static_cast<DisplayMode>(dmode);
    s.display.digits = r.u8(); if (s.display.digits > 10) return false;
    const auto cm = r.u8(); if (cm > 2) return false; s.complex = static_cast<ComplexMode>(cm);
    s.fractionResults = r.u8() != 0; s.lcdBacklight = r.u8(); s.kbdBacklight = r.u8();
    s.wifiSsid = r.str(); s.wifiPassword = r.str(); s.relayHost = r.str(); s.relayPort = r.u16(); s.relayPath = r.str(); s.relayToken = r.str();
    s.aiProvider = r.u8(); s.aiModel = r.str(); s.aiMaxChars = r.u16();
    if (v2) { const auto mixed = r.u8(); if (mixed > 1) return false; s.mixedFractions = mixed != 0; }
    for (char v = 'A'; v <= 'Z'; ++v) {
        const double d = r.f64();
        const ExactSlot e = v2 ? r.slot() : ExactSlot{};
        if (e.valid && !e.value.isZero()) ws.memory.storeExact(v, d, e.value, e.decimal);
        else if (d != 0) ws.memory.store(v, d);
    }
    if (v2) {
        const double ans = r.f64(), ansIm = r.f64(); const ExactSlot ansE = r.slot();
        const double pre = r.f64(), preIm = r.f64(); const ExactSlot preE = r.slot();
        ws.memory.restoreAnswers(ans, ansIm, ansE, pre, preIm, preE);
    } else {
        const double ans = r.f64(), pre = r.f64();
        ws.memory.setAnswer(pre); ws.memory.setAnswer(ans);
    }
    const auto hn = r.u16(); if (hn > Workspace::MaxHistory) return false;
    for (std::uint16_t i = 0; i < hn && r.ok; ++i) {
        HistoryEntry h; h.expression = r.str(); const double re = r.f64(), im = r.f64(); h.value = {re, im}; h.error = r.str(); h.view = r.u8();
        if (h.view > 7) return false;
        if (v2) {
            h.exact = r.slot();
            const auto flags = r.u8(); if (flags > 3) return false;
            h.numerical = (flags & 1) != 0; h.flipMixed = (flags & 2) != 0;
        }
        ws.history.push_back(std::move(h));
    }
    auto readView = [&](ViewWindow& v) { for (double* d : {&v.xmin, &v.xmax, &v.xscl, &v.ymin, &v.ymax, &v.yscl, &v.tmin, &v.tmax, &v.tstep}) *d = r.f64(); };
    ViewWindow& v = ws.view;
    if (version >= 3) {
        const auto count = r.u8(); if (count != ws.functions.size()) return false;
        for (auto& f : ws.functions) {
            f.expr = r.str(); const auto t = r.u8(); if (t >= GraphTypeCount) return false; f.type = static_cast<GraphType>(t);
            f.selected = r.u8() != 0; f.color = r.u8(); if (f.color >= GraphColorCount) return false;
            const auto st = r.u8(); if (st >= LineStyleCount) return false; f.style = static_cast<LineStyle>(st);
            f.expr2 = r.str();
            if (f.expr.size() > MaxGraphExpression || f.expr2.size() > MaxGraphExpression) return false;
        }
        readView(v);
        GraphState& g = ws.graph;
        GraphSettings& gs = g.settings;
        const auto ft = r.u8(); if (ft >= GraphTypeCount) return false; gs.funcType = static_cast<GraphType>(ft);
        const auto flags = r.u8(); if (flags > 127) return false;
        gs.plotDraw = flags & 1; gs.derivative = flags & 2; gs.coord = flags & 4; gs.label = flags & 8;
        gs.graphFunc = flags & 16; gs.simulGraph = flags & 32; gs.ineqUnion = flags & 64;
        const auto grid = r.u8(), axes = r.u8(), sk = r.u8(); if (grid > 2 || axes > 2 || sk >= LineStyleCount) return false;
        gs.grid = static_cast<GridMode>(grid); gs.axes = static_cast<AxesMode>(axes); gs.sketchLine = static_cast<LineStyle>(sk);
        gs.plotColor = r.u8(); if (gs.plotColor >= GraphColorCount) return false;
        gs.xFactor = r.f64(); gs.yFactor = r.f64();
        if (!(gs.xFactor > 0) || !(gs.yFactor > 0) || !std::isfinite(gs.xFactor) || !std::isfinite(gs.yFactor)) return false;
        gs.background = r.u8(); gs.backgroundFile = r.str(); gs.fade = r.u8();
        if (gs.background > BackgroundFile || gs.fade > 100) return false;
        g.vmemUsed = r.u8(); if (g.vmemUsed >> VWindowMemories) return false;
        for (auto& m : g.vmem) readView(m);
        std::size_t gbytes = 0;
        for (auto& m : g.gmem) { m = r.blob(MaxGraphMemoryBytes); gbytes += m.size(); }
        if (gbytes > MaxGraphMemoryBytes) return false;
        const auto sn = r.u16(); if (sn > MaxSketchItems) return false;
        for (std::uint16_t i = 0; i < sn && r.ok; ++i) {
            SketchItem s;
            const auto kind = r.u8(); s.kind = static_cast<SketchItem::Kind>(kind);
            s.x0 = r.f64(); s.y0 = r.f64(); s.x1 = r.f64(); s.y1 = r.f64();
            s.color = r.u8(); const auto style = r.u8(); if (style >= LineStyleCount) return false; s.style = static_cast<LineStyle>(style);
            s.text = r.str();
            const auto pn = r.u16(); if (pn > 2 * MaxPenPoints) return false;
            s.path.resize(pn); for (float& f : s.path) f = r.f32();
            if (r.ok && !validateSketch(s).empty()) return false;
            g.sketch.push_back(std::move(s));
        }
        g.overlay = r.str(); g.project = r.str(); g.projectModified = r.u8() != 0;
    } else {
        for (std::size_t i = 0; i < LegacyFunctions; ++i) {
            GraphFunction& f = ws.functions[i];
            f.expr = r.str(); const auto t = r.u8(); if (t > 5) return false; f.type = static_cast<GraphType>(t);
            f.selected = r.u8() != 0; f.color = static_cast<std::uint8_t>(r.u8() % 6);
        }
        for (std::size_t i = LegacyFunctions; i < ws.functions.size(); ++i) ws.functions[i] = emptyGraphFunction(i);
        readView(v);
        ws.graph.settings.grid = r.u8() != 0 ? GridMode::On : GridMode::Off;   // v1/v2 kept the grid in the V-Window
    }
    ws.table.start = r.f64(); ws.table.end = r.f64(); ws.table.step = r.f64();
    if (version >= 4) {
        const std::uint8_t var = r.u8(), list = r.u8(), type = r.u8(), generated = r.u8();
        if (var > 1 || list >= ws.lists.size() || type >= GraphTypeCount) return false;
        ws.table.variable = static_cast<TableRange::Variable>(var);
        ws.table.list = list;
        ws.table.funcType = static_cast<GraphType>(type);
        if (ws.table.funcType != GraphType::Y && ws.table.funcType != GraphType::Polar && ws.table.funcType != GraphType::Param) return false;
        ws.table.generated = generated != 0;
        const auto n = r.u16();
        if (n > 999) return false;
        ws.table.rows.resize(n);
        for (double& d : ws.table.rows) { d = r.f64(); if (!std::isfinite(d)) return false; }
    }
    for (std::size_t li = 0; li < LegacyLists; ++li) {
        auto& l = ws.lists[li];
        const auto n = r.u16(); if (n > Workspace::MaxListLength) return false;
        l.resize(n); for (auto& d : l) d = r.f64();
    }
    EquationState& e = ws.equation;
    e.unknowns = r.u8(); for (double& d : e.simultaneous) d = r.f64();
    e.degree = r.u8(); for (double& d : e.polynomial) d = r.f64();
    e.solverExpr = r.str(); e.solverGuess = r.f64();
    e.ineqDegree = r.u8(); e.ineqRelation = r.u8(); for (double& d : e.inequality) d = r.f64();
    if (e.unknowns < 2 || e.unknowns > 6 || e.degree < 2 || e.degree > 6 || e.ineqDegree < 1 || e.ineqDegree > 6 || e.ineqRelation > 3) return false;
    const auto cn = r.u16(); if (cn > Workspace::MaxChatMessages) return false;
    for (std::uint16_t i = 0; i < cn && r.ok; ++i) { ChatMessage m; m.fromUser = r.u8() != 0; m.text = r.str(); ws.chat.push_back(std::move(m)); }
    if (version >= 5) {
        const auto store = r.u8(); if (store > 1) return false;
        s.fileStore = static_cast<Settings::FileStore>(store);
        auto& g3 = ws.graph3d;
        if (r.u8() != models::Graph3DModel::Functions) return false;
        for (std::size_t i = 0; i < models::Graph3DModel::Functions; ++i) {
            g3.model.z[i] = r.str(); g3.model.selected[i] = r.u8() != 0;
            if (g3.model.z[i].size() > MaxGraphExpression) return false;
        }
        for (double* d : {&g3.model.xmin, &g3.model.xmax, &g3.model.ymin, &g3.model.ymax, &g3.model.zmin, &g3.model.zmax}) *d = r.f64();
        g3.model.grid = r.u8();
        g3.yaw = r.f32(); g3.pitch = r.f32(); g3.zoom = r.f32();
        if (r.ok && (!models::validate(g3.model).empty() || !std::isfinite(g3.yaw) || !std::isfinite(g3.pitch) ||
                     !(g3.zoom > 0.05F && g3.zoom < 20.0F)))
            return false;
    }
    if (version >= 6) {
        const auto mode = r.u8(); if (mode > 1) return false;
        s.inputMode = static_cast<Settings::InputMode>(mode);
        CollectionMemory& cm = ws.matrices;
        for (CalcValue& v : cm.mat) { if (!r.value(v)) return false; if (!v.empty() && v.type != ValueType::Matrix) return false; }
        for (CalcValue& v : cm.vct) { if (!r.value(v)) return false; if (!v.empty() && (v.type != ValueType::Vector || !v.vectorShaped())) return false; }
        if (!r.value(cm.matAns) || !r.value(cm.vctAns) || !r.value(cm.listAns)) return false;
        if ((!cm.matAns.empty() && cm.matAns.type != ValueType::Matrix) || (!cm.listAns.empty() && !cm.listAns.isList()) ||
            (!cm.vctAns.empty() && (cm.vctAns.type != ValueType::Vector || !cm.vctAns.vectorShaped()))) return false;
        if (cm.elements() > limits::kMatrixStoreElements) return false;
        const auto results = r.u16(); if (results > ws.history.size()) return false;
        std::size_t next = 0;                       // indici strettamente crescenti
        for (std::uint16_t k = 0; k < results && r.ok; ++k) {
            const std::size_t index = r.u16();
            if (index < next || index >= ws.history.size()) return false;
            next = index + 1;
            auto c = std::make_shared<CollectionResult>();
            if (!r.value(c->value, true) || c->value.rows == 0) return false;
            const auto flags = r.u8(); if (flags > 3) return false;
            c->decimal = (flags & 2) != 0;
            if (flags & 1) {
                if (c->value.empty()) return false;
                c->exact.resize(c->value.size());
                for (exact::Value& e : c->exact) { const ExactSlot slot = r.slot(); if (!r.ok || !slot.valid) return false; e = slot.value; }
            }
            ws.history[index].collection = std::move(c);
        }
    }
    if (version >= 7) {
        const auto base = r.u8(), simplify = r.u8(), sep = r.u8(), dec = r.u8(), xl = r.u8(), yl = r.u8();
        if (base > 4 || simplify > 1 || xl >= Workspace::ListCount || yl >= Workspace::ListCount) return false;
        s.baseMode = static_cast<Settings::BaseMode>(base); s.simplifyManual = simplify != 0;
        s.csvSeparator = static_cast<char>(sep); s.csvDecimal = static_cast<char>(dec);
        if ((s.csvSeparator != ',' && s.csvSeparator != ';') || (s.csvDecimal != '.' && s.csvDecimal != ',') || s.csvSeparator == s.csvDecimal) return false;
        s.statXList = xl; s.statYList = yl;
        for (std::size_t li = LegacyLists; li < Workspace::ListCount && r.ok; ++li) {
            auto& l = ws.lists[li];
            const auto n = r.u16(); if (n > Workspace::MaxListLength) return false;
            l.resize(n); for (auto& d : l) d = r.f64();
        }
        for (auto& n : ws.listNames) {
            n = r.str();
            if (n.size() > limits::kListNameLength) return false;
            for (char ch : n) if (static_cast<unsigned char>(ch) < 0x20 || static_cast<unsigned char>(ch) > 0x7E || ch == '"') return false;
        }
        CollectionMemory& cm = ws.matrices;
        auto exactForms = [&](CalcValue& v) {
            const auto has = r.u8();
            if (has > 1 || (has && (v.empty() || v.complex() || v.size() > CalcValue::ExactElements))) return false;
            if (!has) return true;
            v.exact.resize(v.size());
            for (exact::Value& e : v.exact) {
                const ExactSlot slot = r.slot();
                if (!r.ok || !slot.valid) return false;
                e = slot.value;
            }
            // Le forme esatte devono corrispondere ai valori memorizzati.
            for (std::size_t k = 0; k < v.size(); ++k) {
                const double d = v.exact[k].toDouble(), x = v.re[k];
                if (!(std::abs(d - x) <= 1e-12 * std::max(1.0, std::abs(x)))) return false;
            }
            return true;
        };
        for (CalcValue& v : cm.mat) if (!exactForms(v)) return false;
        for (CalcValue& v : cm.vct) if (!exactForms(v)) return false;
        if (!exactForms(cm.matAns) || !exactForms(cm.vctAns) || !exactForms(cm.listAns)) return false;
        const auto extras = r.u16(); if (extras > ws.history.size()) return false;
        std::size_t next = 0;
        for (std::uint16_t k = 0; k < extras && r.ok; ++k) {
            const std::size_t index = r.u16();
            if (index < next || index >= ws.history.size()) return false;
            next = index + 1;
            HistoryEntry& h = ws.history[index];
            h.base = r.u8(); h.fracNum = r.i64(); h.fracDen = r.i64(); h.simpFactor = r.u32();
            const auto flags = r.u8();
            if (h.base > 4 || flags > 1 || h.fracDen < 0) return false;
            h.command = (flags & 1) != 0;
        }
        const auto graphs = r.u8(); if (graphs > MaxCommandGraphs) return false;
        for (std::uint8_t k = 0; k < graphs && r.ok; ++k) {
            CommandGraph cg;
            const auto type = r.u8(); cg.expr = r.str(); cg.expr2 = r.str(); cg.color = r.u8(); const auto style = r.u8();
            if (type >= GraphTypeCount || cg.color >= GraphColorCount || style >= LineStyleCount) return false;
            cg.type = static_cast<GraphType>(type); cg.style = static_cast<LineStyle>(style);
            if (cg.type != GraphType::Y && cg.type != GraphType::Polar && cg.type != GraphType::X && cg.type != GraphType::Param) return false;
            if (cg.expr.empty() || cg.expr.size() > MaxGraphExpression || cg.expr2.size() > MaxGraphExpression) return false;
            ws.graph.commandGraphs.push_back(std::move(cg));
        }
    }
    if (ws.listElements() > Workspace::MaxListElements) return false;
    if (!r.ok || r.i != r.n) return false;
    if (version < 3) {
        // Older forms accepted values the v1.8.0 checks reject: keep the record
        // (the user's data) and repair only the window parameters.
        if (v.xmin < v.xmax && v.ymin < v.ymax) {
            v.xscl = std::abs(v.xscl); v.yscl = std::abs(v.yscl);
            if (!std::isfinite(v.tstep) || v.tstep == 0 || v.tmin == v.tmax || (v.tmax - v.tmin) / v.tstep < 0) {
                const ViewWindow d; v.tmin = d.tmin; v.tmax = d.tmax; v.tstep = d.tstep;
            }
        }
        if (!validateView(v).empty()) v = ViewWindow{};
    }
    if (!validateView(v).empty()) return false;
    out = std::move(ws);
    return true;
}
} // namespace fx
