#include "workspace.hpp"
#include "mem_pools.hpp"
#include "graph_engine.hpp"
#include "solvers.hpp"
#include "statistics.hpp"
#include "table_model.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace fx {
Workspace::Workspace() { resetAll(); }

// ---------------------------------------------------------------- EQUATION
EquationState::EquationState()
    : simultaneous(MaxUnknowns * SimStride), polynomial(MaxDegree + 1), inequality(MaxDegree + 1) {}
std::vector<eq::Number> EquationState::simCoefficients() const {
    std::vector<eq::Number> out;
    out.reserve(static_cast<std::size_t>(unknowns) * (unknowns + 1u));
    for (std::size_t r = 0; r < unknowns; ++r) for (std::size_t c = 0; c <= unknowns; ++c) out.push_back(sim(r, c));
    return out;
}
std::vector<eq::Number> EquationState::polyCoefficients() const {
    return std::vector<eq::Number>(polynomial.begin(), polynomial.begin() + degree + 1);
}

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
    graph3d.reset();
    matrices.clear();
    for (auto& f : listFiles) f = ListFileData{};
    for (auto& f : functionMemories) f.clear();
    for (auto& t : strings) t.clear();
    modeVars = ModeVariables{};
    preservedSections.clear();
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
        throw TypeError(std::string("Argument ERROR: in ") + what + " si memorizza una matrice, non una " + valueTypeName(v.type));
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
// Forme esatte di tutte le memorie insieme (limits::kExactStoreElements, v1.12.0):
// oltre il budget della scheda la nuova memoria conserva solo i valori.
std::size_t exactForms(const CollectionMemory& m) noexcept {
    std::size_t n = m.matAns.exact.size() + m.vctAns.exact.size() + m.listAns.exact.size();
    for (const CalcValue& v : m.mat) n += v.exact.size();
    for (const CalcValue& v : m.vct) n += v.exact.size();
    return n;
}
void fitExact(const CollectionMemory& m, CalcValue& value, std::size_t freed) {
    if (!value.hasExact() || value.complex()) { value.clearExact(); return; }
    const std::size_t used = exactForms(m) - std::min(freed, exactForms(m));
    if (used + value.exact.size() > limits::kExactStoreElements) value.clearExact();
}
} // namespace

bool Workspace::collection(Memory memory, int index, CalcValue& out) const {
    const CalcValue* src = nullptr;
    if (memory == Memory::Equation) {
        // VARS › EQUATION (manuale 1-33): soluzioni salvate dall'ultimo SOLVE,
        // coefficienti attuali (errore se non ne e' stato inserito nessuno).
        mem::PlacementScope heavy(mem::Policy::PREFER_PSRAM);
        auto coefficients = [&](const std::vector<eq::Number>& c, std::size_t rows, std::size_t cols) {
            if (std::all_of(c.begin(), c.end(), [](const eq::Number& n) { return n.isZero(); })) return false;
            out = eq::toMatrix(c, rows, cols);
            return true;
        };
        switch (index) {
        case SimRes: src = &equation.simResult; break;
        case PlyRes: src = &equation.polyResult; break;
        case SimCoef: return coefficients(equation.simCoefficients(), equation.unknowns, equation.unknowns + 1u);
        case PlyCoef: return coefficients(equation.polyCoefficients(), 1, equation.degree + 1u);
        default: return false;
        }
        if (src->empty()) return false;
        out = *src;
        return true;
    }
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
    fitExact(matrices, value, matrices.mat[slot].exact.size());
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
    fitExact(matrices, value, matrices.vct[slot].exact.size());
    matrices.vct[slot] = std::move(value);
}
void Workspace::storeList(std::size_t index, const CalcValue& value) {
    if (index >= lists.size()) throw MathError("Math ERROR: sono disponibili List 1-" + std::to_string(lists.size()));
    if (!value.isList() || value.empty()) throw TypeError(std::string("Argument ERROR: in List si memorizza una lista, non una ") + valueTypeName(value.type));
    if (value.complex() && std::any_of(value.im.begin(), value.im.end(), [](double d) { return d != 0.0; }))
        throw MathError("Complex Number In List: le liste contengono solo numeri reali");
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
    for (const auto& f : listFiles) n += f.elements();        // List File 1-6: budget comune
    return n;
}
bool Workspace::special(const std::string& n, CalcValue& out) const {
    // VARS › EQUATION (SimRes, SimCoef, PlyRes, PlyCoef) e' in collection(Memory::Equation).
    if (n == "tbl_result") {
        // La tabella di TABLE: colonna della variabile e una colonna per funzione selezionata.
        // Sullo heap: il motore e' troppo grande per lo stack da 32 KB dell'RP2350.
        const auto enginePtr = std::make_unique<graph::GraphEngine>();
        graph::GraphEngine& engine = *enginePtr;
        engine.build({&functions, &memory, this, &lists, nullptr});
        const std::vector<table::Column> cols = table::columns(engine, table.funcType, false);
        if (cols.empty()) throw MathError("Math ERROR: nessuna funzione selezionata in TABLE");
        std::vector<double> rows = table.generated && !table.rows.empty() ? table.rows : table::rangeRows(table.start, table.end, table.step);
        if (rows.empty() || rows.size() * (cols.size() + 1) > limits::kMatrixElements) throw DimensionError("Dimension ERROR: tabella troppo grande per una matrice");
        out = CalcValue::matrix(rows.size(), cols.size() + 1);
        for (std::size_t r = 0; r < rows.size(); ++r) {
            out.set(r * (cols.size() + 1), {rows[r], 0});
            for (std::size_t c = 0; c < cols.size(); ++c) {
                double v = 0;
                if (!table::cell(engine, cols[c], rows[r], v)) throw MathError("Math ERROR: valore della tabella non definito");
                out.set(r * (cols.size() + 1) + c + 1, {v, 0});
            }
        }
        return true;
    }
    return false;
}
int Workspace::listByName(const std::string& subName) const {
    if (subName.empty()) return -1;
    for (std::size_t i = 0; i < listNames.size(); ++i) if (listNames[i] == subName) return static_cast<int>(i);
    return -1;
}
void Workspace::selectListFile(std::size_t file) {
    if (file >= listFiles.size()) throw MathError("Argument ERROR: List File da 1 a " + std::to_string(listFiles.size()));
    if (file == settings.listFile) return;
    ListFileData& active = listFiles[settings.listFile];
    active.lists = std::move(lists); active.names = std::move(listNames);
    ListFileData& chosen = listFiles[file];
    lists = std::move(chosen.lists); listNames = std::move(chosen.names);
    chosen = ListFileData{};
    settings.listFile = static_cast<std::uint8_t>(file);
}
void Workspace::setString(std::size_t index, std::string text) {
    if (index >= strings.size()) throw MathError("Argument ERROR: Str da 1 a " + std::to_string(strings.size()));
    if (text.size() > limits::kStringLength) throw MathError("Argument ERROR: una stringa ha al massimo " + std::to_string(limits::kStringLength) + " caratteri");
    strings[index] = std::move(text);
}
void Workspace::setRecursionFormula(std::string formula) {
    if (formula.size() > limits::kFunctionMemoryLength) throw MathError("Memory ERROR: formula troppo lunga");
    if (!formula.empty()) (void)parseExpression(formula);
    modeVars.recFormula = std::move(formula);
}
void Workspace::setFunctionMemory(std::size_t index, std::string expression) {
    if (index >= functionMemories.size()) throw MathError("Argument ERROR: f1-f" + std::to_string(functionMemories.size()));
    if (expression.size() > limits::kFunctionMemoryLength) throw MathError("Memory ERROR: espressione troppo lunga");
    if (!expression.empty()) (void)parseExpression(expression);        // Syntax ERROR prima di memorizzare
    functionMemories[index] = std::move(expression);
}
int Workspace::pixelTest(int row, int column) const { return pixelProbe ? pixelProbe(row, column) : 0; }
std::string Workspace::functionMemory(int n) const {
    return n >= 1 && static_cast<std::size_t>(n) <= functionMemories.size() ? functionMemories[static_cast<std::size_t>(n - 1)] : std::string();
}
bool Workspace::storeSystemVariable(const std::string& n, double v) {
    if (!std::isfinite(v)) throw MathError("Math ERROR: valore non finito");
    // V-Window: stessi controlli di GRAPH (Range ERROR).
    double ViewWindow::* const viewFields[] = {&ViewWindow::xmin, &ViewWindow::xmax, &ViewWindow::xscl, &ViewWindow::ymin, &ViewWindow::ymax,
                                               &ViewWindow::yscl, &ViewWindow::tmin, &ViewWindow::tmax, &ViewWindow::tstep};
    const char* const viewNames[] = {"xmin", "xmax", "xscl", "ymin", "ymax", "yscl", "tmin", "tmax", "tptch"};
    for (std::size_t i = 0; i < 9; ++i) {
        if (n != viewNames[i]) continue;
        ViewWindow w = view;
        w.*viewFields[i] = v;
        const std::string bad = validateView(w);
        // Appendice α: un parametro della V-Window non valido e' un Range ERROR.
        if (!bad.empty()) throw MathError("Range ERROR: " + bad.substr(bad.find(':') == std::string::npos ? 0 : bad.find(':') + 2));
        view = w;
        return true;
    }
    if (n == "xfct" || n == "yfct") {
        if (!(v > 0)) throw MathError("Argument ERROR: il fattore di zoom e' positivo");
        (n == "xfct" ? graph.settings.xFactor : graph.settings.yFactor) = v;
        return true;
    }
    if (n == "tblstart") { table.start = v; return true; }
    if (n == "tblend") { table.end = v; return true; }
    if (n == "tblpitch") { if (v == 0) throw MathError("Argument ERROR: TblPitch diverso da 0"); table.step = v; return true; }
    ModeVariables& m = modeVars;
    const struct { const char* name; double* field; } fields[] = {
        {"d_start", &m.dynaStart}, {"d_end", &m.dynaEnd}, {"d_pitch", &m.dynaPitch}, {"r_start", &m.recStart}, {"r_end", &m.recEnd},
        {"rec_a0", &m.recInitial[0]}, {"rec_a1", &m.recInitial[1]}, {"rec_a2", &m.recInitial[2]}, {"rec_b0", &m.recInitial[3]},
        {"rec_b1", &m.recInitial[4]}, {"rec_b2", &m.recInitial[5]}, {"rec_c0", &m.recInitial[6]}, {"rec_c1", &m.recInitial[7]},
        {"rec_c2", &m.recInitial[8]}, {"rec_anstart", &m.recNStart[0]}, {"rec_bnstart", &m.recNStart[1]}, {"rec_cnstart", &m.recNStart[2]},
        {"tvm_n", &m.tvmN}, {"tvm_i", &m.tvmI}, {"tvm_pv", &m.tvmPV}, {"tvm_pmt", &m.tvmPMT}, {"tvm_fv", &m.tvmFV},
        {"tvm_py", &m.tvmPY}, {"tvm_cy", &m.tvmCY}};
    for (const auto& f : fields) {
        if (n != f.name) continue;
        if ((n == "r_start" || n == "r_end") && (v != std::floor(v) || v < 0 || v > 999)) throw MathError("Argument ERROR: R Start / R End interi da 0 a 999");
        if ((n == "tvm_py" || n == "tvm_cy") && !(v > 0)) throw MathError("Argument ERROR: P/Y e C/Y positivi");
        *f.field = v;
        return true;
    }
    return false;
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
        CalcValue l = value; l.normalize();
        fitExact(matrices, l, matrices.listAns.exact.size());
        matrices.listAns = std::move(l);
        return;
    }
    if (!value.isMatrixLike()) return;
    checkStorable(value, "MatAns");
    const bool vector = value.vectorShaped();
    checkBudget(matrices, matrices.matAns.size() + (vector ? matrices.vctAns.size() : 0), value.size() * (vector ? 2 : 1));
    CalcValue m = value;
    m.type = ValueType::Matrix; m.normalize();
    fitExact(matrices, m, matrices.matAns.exact.size() + (vector ? matrices.vctAns.exact.size() : 0));
    if (vector) { CalcValue v = m; v.type = ValueType::Vector; matrices.vctAns = std::move(v); }
    matrices.matAns = std::move(m);
}
std::unique_ptr<Workspace::CollectionSnapshot> Workspace::snapshotCollections() const {
    mem::PlacementScope heavy(mem::Policy::PREFER_PSRAM);
    auto s = std::make_unique<CollectionSnapshot>();
    s->matrices = matrices;
    s->lists = lists;
    s->strings = strings; s->functionMemories = functionMemories; s->modeVars = modeVars; s->view = view;
    s->tableStart = table.start; s->tableEnd = table.end; s->tableStep = table.step;
    s->xFactor = graph.settings.xFactor; s->yFactor = graph.settings.yFactor;
    return s;
}
void Workspace::restoreCollections(CollectionSnapshot& snapshot) {
    matrices = std::move(snapshot.matrices);
    lists = std::move(snapshot.lists);
    strings = std::move(snapshot.strings); functionMemories = std::move(snapshot.functionMemories); modeVars = snapshot.modeVars;
    view = snapshot.view; table.start = snapshot.tableStart; table.end = snapshot.tableEnd; table.step = snapshot.tableStep;
    graph.settings.xFactor = snapshot.xFactor; graph.settings.yFactor = snapshot.yFactor;
}

void Workspace::apply() const { setAngleMode(settings.angle); }

EvalContext Workspace::evalContext() const {
    EvalContext c;
    c.memory = &memory;
    c.complexMode = settings.complex;
    c.names = this;
    c.collections = this;
    c.services = this;
    c.displayMode = static_cast<std::uint8_t>(settings.display.mode);
    c.displayDigits = settings.display.digits;
    return c;
}

bool Workspace::resolve(const std::string& n, double& out) const {
    const struct { const char* name; double value; } vars[] = {
        {"xmin", view.xmin}, {"xmax", view.xmax}, {"xscl", view.xscl}, {"ymin", view.ymin}, {"ymax", view.ymax},
        {"yscl", view.yscl}, {"tmin", view.tmin}, {"tmax", view.tmax}, {"tptch", view.tstep},
        {"tblstart", table.start}, {"tblend", table.end}, {"tblpitch", table.step}, {"xdot", viewXdot(view)}};
    for (const auto& v : vars) if (n == v.name) { out = v.value; return true; }
    // v1.12.0: VARS FACTOR, DYNA, RECURSION, TVM
    const ModeVariables& m = modeVars;
    const struct { const char* name; double value; } more[] = {
        {"xfct", graph.settings.xFactor}, {"yfct", graph.settings.yFactor}, {"d_start", m.dynaStart}, {"d_end", m.dynaEnd},
        {"d_pitch", m.dynaPitch}, {"r_start", m.recStart}, {"r_end", m.recEnd}, {"rec_a0", m.recInitial[0]}, {"rec_a1", m.recInitial[1]},
        {"rec_a2", m.recInitial[2]}, {"rec_b0", m.recInitial[3]}, {"rec_b1", m.recInitial[4]}, {"rec_b2", m.recInitial[5]},
        {"rec_c0", m.recInitial[6]}, {"rec_c1", m.recInitial[7]}, {"rec_c2", m.recInitial[8]}, {"rec_anstart", m.recNStart[0]},
        {"rec_bnstart", m.recNStart[1]}, {"rec_cnstart", m.recNStart[2]}, {"tvm_n", m.tvmN}, {"tvm_i", m.tvmI}, {"tvm_pv", m.tvmPV},
        {"tvm_pmt", m.tvmPMT}, {"tvm_fv", m.tvmFV}, {"tvm_py", m.tvmPY}, {"tvm_cy", m.tvmCY}};
    for (const auto& v : more) if (n == v.name) { out = v.value; return true; }
    // VARS STAT: risultati calcolati dalle liste scelte in Statistics (XList, YList).
    if (n.compare(0, 5, "stat_") == 0 || n.compare(0, 4, "reg_") == 0) return statisticVariable(n, out);
    return false;
}
bool Workspace::statisticVariable(const std::string& n, double& out) const {
    const auto& x = lists[settings.statXList];
    const auto& y = lists[settings.statYList];
    auto need = [](bool ok) { if (!ok) throw MathError("Math ERROR: dati di Statistics insufficienti (XList / YList)"); };
    if (n == "reg_type") { out = settings.statRegType == 0xFF ? -1 : settings.statRegType; return true; }
    if (n.compare(0, 4, "reg_") == 0) {
        if (settings.statRegType >= static_cast<std::uint8_t>(stats::RegType::Count))
            throw MathError("Math ERROR: nessuna regressione calcolata in Statistics");
        need(!x.empty() && x.size() == y.size());
        const stats::Regression r = stats::regress(static_cast<stats::RegType>(settings.statRegType), x, y);
        const char* const coef[] = {"reg_a", "reg_b", "reg_c", "reg_d", "reg_e"};
        for (std::size_t i = 0; i < 5; ++i) if (n == coef[i]) { out = i < r.coef.size() ? r.coef[i] : 0; return true; }
        if (n == "reg_r") { out = r.r; return true; }
        if (n == "reg_r2") { out = r.r2; return true; }
        if (n == "reg_mse") { out = r.mse; return true; }
        return false;
    }
    const bool second = n == "stat_meany" || n == "stat_sumy" || n == "stat_sumy2" || n == "stat_sumxy" || n == "stat_popsdy" ||
                        n == "stat_sdy" || n == "stat_miny" || n == "stat_maxy";
    if (second) {
        need(!x.empty() && x.size() == y.size());
        const stats::TwoVar t = stats::twoVariable(x, y);
        const struct { const char* name; double value; } v[] = {{"stat_meany", t.meanY}, {"stat_sumy", t.sumY}, {"stat_sumy2", t.sumY2},
            {"stat_sumxy", t.sumXY}, {"stat_popsdy", t.popSdY}, {"stat_sdy", t.sampleSdY}, {"stat_miny", t.minY}, {"stat_maxy", t.maxY}};
        for (const auto& e : v) if (n == e.name) { out = e.value; return true; }
        return false;
    }
    need(!x.empty());
    const stats::OneVar o = stats::oneVariable(x);
    const struct { const char* name; double value; } v[] = {{"stat_n", o.n}, {"stat_meanx", o.mean}, {"stat_sumx", o.sum},
        {"stat_sumx2", o.sumSq}, {"stat_popsdx", o.popSd}, {"stat_sdx", o.sampleSd}, {"stat_minx", o.min}, {"stat_maxx", o.max},
        {"stat_q1", o.q1}, {"stat_q3", o.q3}, {"stat_med", o.median}, {"stat_mod", o.modes.empty() ? o.mean : o.modes.front()},
        {"stat_hstart", o.min}, {"stat_hpitch", (o.max - o.min) / 10.0}};
    for (const auto& e : v) if (n == e.name) { out = e.value; return true; }
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
// Version 8 (v1.12.0) sposta i dati di EQUATION in coda: coefficienti come
// numeri con forma esatta (eq::Number), griglia SIML fissa 6 x 7, limiti e
// variabile del Solver, ultime soluzioni (SimRes, PlyRes). Il blocco della
// versione 1-7 (coefficienti double) non viene piu' scritto; i record 1-7 si
// convertono senza perdite (fromStoredDouble, celle SIML riportate alla loro
// riga e colonna, valore iniziale del Solver nella variabile X).
// Version 9 (v1.13.0, completamento di Run-Matrix) aggiunge in coda: List File attivo, Sub Name e tipo
// dell'ultima regressione; le variabili r e θ; le liste e i sottonomi degli
// altri List File (1-6); function memory f1-f20; Str 1-20; le variabili di
// Dyna Graph, Recursion e TVM; per le righe della cronologia lo spostamento
// ENG, i risultati intermedi di ◢ e i risultati stringa. I record delle
// versioni 1-8 si caricano con i valori predefiniti di questi campi.
// Version 10 (v1.14.0, completamento di 3D Graph) toglie il blocco 3D della
// versione 5 dal mezzo del record e aggiunge in coda il nuovo stato del 3D
// Graph: le tre funzioni (tipo, modulo, espressioni, coefficienti, selezione,
// colori), la 3D View Window con Xgrid/Ygrid, S, T, θ e φ, il Setup, lo zoom,
// le 3D V-Win 1-6 e le 3D G-Mem 1-20 (g3d::encode). I record 5-9 convertono
// Z1-Z3 in grafici Z=, la griglia (celle) in punti e yaw/pitch in θ/φ.
// Version 11 (v1.15.0, fondamenta comuni) aggiunge in coda le sezioni TLV:
// u16 numero, poi per sezione u16 id, u16 versione, u32 lunghezza e i dati,
// in ordine di id crescente (id duplicati o fuori ordine: record non valido).
// Le sezioni sconosciute o piu' nuove restano in Workspace::preservedSections
// e vengono riscritte intatte. Dalla v1.15.0 ogni modalita' nuova aggiunge una
// sezione invece di una versione del formato.
constexpr std::uint16_t Version = 11;
constexpr std::size_t MaxGraph3DSet = 16 * 1024;   // un insieme codificato (tre espressioni da 1000 byte al massimo)
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
// Numero di EQUATION (v8): indicatori (1 forma esatta, 2 decimale), valore
// complesso, forma esatta (parte reale e immaginaria).
void writeNumber(Writer& w, const eq::Number& n) {
    w.u8(static_cast<std::uint8_t>((n.hasExact ? 1 : 0) | (n.decimal ? 2 : 0)));
    w.f64(n.value.real()); w.f64(n.value.imag());
    if (n.hasExact) { w.slot(ExactSlot{n.exact.re, true, false}); w.slot(ExactSlot{n.exact.im, true, false}); }
}
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
    bool number(eq::Number& out) {
        const std::uint8_t flags = u8();
        const double re = f64(), im = f64();
        if (!ok || flags > 3 || !std::isfinite(re) || !std::isfinite(im)) return ok = false;
        if (flags & 1) {
            const ExactSlot a = slot(), b = slot();
            if (!ok || !a.valid || !b.valid) return ok = false;
            out = eq::Number::fromExact({a.value, b.value}, (flags & 2) != 0);
            // La forma esatta deve corrispondere al valore memorizzato.
            const Complex z = out.value;
            if (!(std::abs(z.real() - re) <= 1e-12 * std::max(1.0, std::abs(re))) || !(std::abs(z.imag() - im) <= 1e-12 * std::max(1.0, std::abs(im)))) return ok = false;
        } else {
            if (flags & 2) return ok = false;
            out = eq::Number::fromComplex({re, im});
        }
        return true;
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

std::uint16_t workspaceRecordVersion() noexcept { return Version; }

std::vector<std::uint8_t> serializeWorkspace(const Workspace& ws) {
    std::size_t count = 0;
    const SectionCodec* codecs = workspaceSectionCodecs(count);
    return serializeWorkspace(ws, codecs, count);
}

std::vector<std::uint8_t> serializeWorkspace(const Workspace& ws, const SectionCodec* codecs, std::size_t count) {
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
    // (v1-v7: qui c'era il blocco di EQUATION, ora in coda al record v8)
    w.u16(static_cast<std::uint16_t>(ws.chat.size()));
    for (const auto& m : ws.chat) { w.u8(m.fromUser ? 1 : 0); w.str(m.text); }
    // v5 (v10: il blocco 3D della versione 5 e' in coda al record)
    w.u8(static_cast<std::uint8_t>(ws.settings.fileStore));
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
    // v8: EQUATION
    const EquationState& e = ws.equation;
    w.u8(e.unknowns); for (const eq::Number& n : e.simultaneous) writeNumber(w, n);
    w.u8(e.degree); for (const eq::Number& n : e.polynomial) writeNumber(w, n);
    w.str(e.solverExpr); w.f64(e.solverLower); w.f64(e.solverUpper); w.u8(static_cast<std::uint8_t>(e.solverVariable));
    w.u8(e.ineqDegree); w.u8(e.ineqRelation); for (const eq::Number& n : e.inequality) writeNumber(w, n);
    w.value(e.simResult); exactForms(e.simResult);
    w.value(e.polyResult); exactForms(e.polyResult);
    // v9: Run-Matrix (v1.13.0)
    w.u8(st.listFile); w.u8(st.subName ? 1 : 0); w.u8(st.statRegType);
    for (char slot : {CalculatorMemory::RSlot, CalculatorMemory::ThetaSlot}) { w.f64(ws.memory.recall(slot)); w.slot(ws.memory.exactSlot(slot)); w.u8(ws.memory.used(slot) ? 1 : 0); }
    for (const ListFileData& f : ws.listFiles) {
        for (const auto& l : f.lists) { w.u16(static_cast<std::uint16_t>(l.size())); for (double d : l) w.f64(d); }
        for (const auto& n : f.names) w.str(n);
    }
    for (const auto& f : ws.functionMemories) w.str(f);
    for (const auto& t : ws.strings) w.str(t);
    const ModeVariables& mv = ws.modeVars;
    for (double d : {mv.dynaStart, mv.dynaEnd, mv.dynaPitch, mv.recStart, mv.recEnd}) w.f64(d);
    for (double d : mv.recInitial) w.f64(d);
    for (double d : mv.recNStart) w.f64(d);
    for (double d : {mv.tvmN, mv.tvmI, mv.tvmPV, mv.tvmPMT, mv.tvmFV, mv.tvmPY, mv.tvmCY}) w.f64(d);
    w.str(mv.recFormula);
    // Celle digitate come decimali (bit per elemento) delle memorie con forme esatte.
    auto decimals = [&](const CalcValue& v) {
        const bool has = v.hasExact() && v.anyDecimal();
        w.u8(has ? 1 : 0);
        if (!has) return;
        for (std::size_t k = 0; k < v.size(); k += 8) {
            std::uint8_t b = 0;
            for (std::size_t j = 0; j < 8 && k + j < v.size(); ++j) if (v.decimalAt(k + j)) b = static_cast<std::uint8_t>(b | (1u << j));
            w.u8(b);
        }
    };
    for (const CalcValue& v : cm.mat) decimals(v);
    for (const CalcValue& v : cm.vct) decimals(v);
    decimals(cm.matAns); decimals(cm.vctAns); decimals(cm.listAns);
    auto extra8 = [](const HistoryEntry& h) { return h.engShift != 0 || !h.displays.empty() || !h.text.empty(); };
    std::uint16_t extras8 = 0;
    for (const auto& h : ws.history) if (extra8(h)) ++extras8;
    w.u16(extras8);
    for (std::size_t i = 0; i < ws.history.size(); ++i) {
        const HistoryEntry& h = ws.history[i];
        if (!extra8(h)) continue;
        w.u16(static_cast<std::uint16_t>(i)); w.u8(static_cast<std::uint8_t>(h.engShift));
        w.u8(static_cast<std::uint8_t>(std::min<std::size_t>(h.displays.size(), 255)));
        for (std::size_t k = 0; k < h.displays.size() && k < 255; ++k) w.str(h.displays[k]);
        w.str(h.text);
    }
    // v10: 3D Graph (v1.14.0)
    const Graph3DState& g3 = ws.graph3d;
    w.blob(g3d::encode(g3.set));
    w.f32(g3.zoom);
    w.u8(g3.vmemUsed);
    for (const g3d::Window& vw : g3.vmem) w.blob(g3d::encodeWindow(vw));
    for (const std::string& m : g3.gmem) w.blob(m);
    // v11: sezioni TLV (v1.15.0). Una sezione conservata vince sulla codifica
    // conosciuta con lo stesso id solo se e' di una versione piu' nuova (i dati
    // di un firmware successivo non si perdono).
    std::vector<RecordSection> sections;
    for (std::size_t i = 0; i < count; ++i) {
        const SectionCodec& c = codecs[i];
        bool newer = false;
        for (const RecordSection& p : ws.preservedSections) newer = newer || (p.id == c.id && p.version > c.version);
        if (newer || !c.present(ws)) continue;
        RecordSection sec;
        sec.id = c.id; sec.version = c.version;
        c.encode(ws, sec.data);
        sections.push_back(std::move(sec));
    }
    for (const RecordSection& p : ws.preservedSections) {
        bool known = false;
        for (const RecordSection& q : sections) known = known || q.id == p.id;
        if (!known) sections.push_back(p);
    }
    std::sort(sections.begin(), sections.end(), [](const RecordSection& a, const RecordSection& b) { return a.id < b.id; });
    w.u16(static_cast<std::uint16_t>(sections.size()));
    for (const RecordSection& sec : sections) {
        w.u16(sec.id); w.u16(sec.version); w.u32(static_cast<std::uint32_t>(sec.data.size()));
        w.out.insert(w.out.end(), sec.data.begin(), sec.data.end());
    }
    return std::move(w.out);
}

namespace {
// Blocco 3D delle versioni 5-9 (v1.9.0-v1.13.1): superfici Z1-Z3 con la
// selezione, finestra X/Y/Z, griglia in celle (5-128) e vista yaw/pitch/zoom.
// Diventa lo stato della v1.14.0: grafici Z=, Xgrid = Ygrid = celle + 1 punti
// (2-50), θ e φ con la stessa orientazione della vista (yaw = -θ - 90,
// pitch = 90 - φ), stesso zoom. Gli stessi controlli della versione 9.
bool readLegacyGraph3D(Reader& r, Graph3DState& g3) {
    if (r.u8() != g3d::Functions) return false;
    std::array<std::string, g3d::Functions> z{};
    std::array<bool, g3d::Functions> sel{};
    for (std::size_t i = 0; i < g3d::Functions; ++i) {
        z[i] = r.str(); sel[i] = r.u8() != 0;
        if (z[i].size() > MaxGraphExpression) return false;
    }
    double box[6];
    for (double& d : box) d = r.f64();
    const int grid = r.u8();
    const float yaw = r.f32(), pitch = r.f32(), zoom = r.f32();
    if (!r.ok) return false;
    if (!(box[0] < box[1]) || !(box[2] < box[3]) || !(box[4] < box[5]) || grid < 5 || grid > 128) return false;
    for (std::size_t i = 0; i < g3d::Functions; ++i) if (sel[i] && z[i].empty()) return false;
    if (!std::isfinite(yaw) || !std::isfinite(pitch) || !(zoom > 0.05F && zoom < 20.0F)) return false;
    g3.reset();                                   // in place: no Graph3DState temporary on the stack
    for (std::size_t i = 0; i < g3d::Functions; ++i) {
        g3d::Function& f = g3.set.fns[i];
        if (z[i].empty()) continue;
        f.kind = g3d::Kind::Z; f.expr[0] = std::move(z[i]); f.selected = sel[i];
    }
    g3d::Window& w = g3.set.window;
    w.xmin = box[0]; w.xmax = box[1]; w.ymin = box[2]; w.ymax = box[3]; w.zmin = box[4]; w.zmax = box[5];
    w.xgrid = w.ygrid = std::clamp(grid + 1, g3d::MinGrid, g3d::MaxGrid);
    w.theta = g3d::normalizeTheta(-(static_cast<double>(yaw) + 90.0));
    w.phi = g3d::normalizePhi(90.0 - static_cast<double>(pitch));
    if (!g3d::validate(w).empty()) {   // valori oltre 1E98 accettati dalla v1.9.0: finestra iniziale, stessa vista
        const double theta = w.theta, phi = w.phi;
        w = g3d::Window{};
        w.theta = theta; w.phi = phi;
    }
    g3.zoom = zoom;
    return true;
}
} // namespace

bool deserializeWorkspace(const std::uint8_t* data, std::size_t size, Workspace& out) {
    std::size_t count = 0;
    const SectionCodec* codecs = workspaceSectionCodecs(count);
    return deserializeWorkspace(data, size, out, codecs, count);
}

bool deserializeWorkspace(const std::uint8_t* data, std::size_t size, Workspace& out, const SectionCodec* codecs, std::size_t count) {
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
        if (h.view > (version >= 9 ? 10 : 7)) return false;          // v9: ENG, ►r∠θ, ►a+bi
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
    // v1.13.1 (E01): un record si accetta solo se il record che se ne ricava al
    // salvataggio successivo si rilegge. Le liste con un elemento non finito
    // erano accettate qui ma rifiutate nel blocco dei List File (v9) dopo un
    // cambio di List File, e il workspace veniva azzerato all'avvio successivo.
    for (std::size_t li = 0; li < LegacyLists; ++li) {
        auto& l = ws.lists[li];
        const auto n = r.u16(); if (n > Workspace::MaxListLength) return false;
        l.resize(n); for (auto& d : l) { d = r.f64(); if (!std::isfinite(d)) return false; }
    }
    EquationState& e = ws.equation;
    double legacyGuess = 0;
    if (version < 8) {
        // Record 1-7: coefficienti double, griglia SIML n x (n+1) per righe.
        e.unknowns = r.u8();
        double sim[42], poly[7], ineq[7];
        for (double& d : sim) d = r.f64();
        e.degree = r.u8(); for (double& d : poly) d = r.f64();
        e.solverExpr = r.str(); legacyGuess = r.f64();
        e.ineqDegree = r.u8(); e.ineqRelation = r.u8(); for (double& d : ineq) d = r.f64();
        if (e.unknowns < 2 || e.unknowns > 6 || e.degree < 2 || e.degree > 6 || e.ineqDegree < 1 || e.ineqDegree > 6 || e.ineqRelation > 3) return false;
        // v1.13.1 (E01): gli stessi controlli del blocco v8, che rileggera' questi
        // dati dopo il primo salvataggio (coefficienti finiti, equazione del Solver
        // entro il limite).
        for (double d : sim) if (!std::isfinite(d)) return false;
        for (double d : poly) if (!std::isfinite(d)) return false;
        for (double d : ineq) if (!std::isfinite(d)) return false;
        if (e.solverExpr.size() > limits::kEquationExpression) return false;
        // Le celle del sistema attivo tornano alla loro riga e colonna; le altre
        // (valori di sistemi di altre dimensioni) occupano, nell'ordine, le
        // posizioni libere della griglia 6 x 7: nessun valore va perso.
        const std::size_t n = e.unknowns, active = n * (n + 1);
        std::vector<bool> taken(e.simultaneous.size(), false);
        for (std::size_t k = 0; k < active; ++k) {
            const std::size_t at = (k / (n + 1)) * EquationState::SimStride + k % (n + 1);
            e.simultaneous[at] = eq::fromStoredDouble(sim[k]); taken[at] = true;
        }
        std::size_t free = 0;
        for (std::size_t k = active; k < 42; ++k) {
            while (taken[free]) ++free;
            e.simultaneous[free] = eq::fromStoredDouble(sim[k]); taken[free] = true;
        }
        for (std::size_t k = 0; k < 7; ++k) { e.polynomial[k] = eq::fromStoredDouble(poly[k]); e.inequality[k] = eq::fromStoredDouble(ineq[k]); }
    }
    const auto cn = r.u16(); if (cn > Workspace::MaxChatMessages) return false;
    for (std::uint16_t i = 0; i < cn && r.ok; ++i) { ChatMessage m; m.fromUser = r.u8() != 0; m.text = r.str(); ws.chat.push_back(std::move(m)); }
    if (version >= 5) {
        const auto store = r.u8(); if (store > 1) return false;
        s.fileStore = static_cast<Settings::FileStore>(store);
        if (version < 10 && !readLegacyGraph3D(r, ws.graph3d)) return false;
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
            l.resize(n); for (auto& d : l) { d = r.f64(); if (!std::isfinite(d)) return false; }   // E01
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
            // v1.12.0: anche le disequazioni (Graph Y>, Graph X<= ...); ogni tipo valido si accetta.
            if (cg.expr.empty() || cg.expr.size() > MaxGraphExpression || cg.expr2.size() > MaxGraphExpression) return false;
            ws.graph.commandGraphs.push_back(std::move(cg));
        }
    }
    if (version >= 8) {
        e.unknowns = r.u8();
        for (eq::Number& n : e.simultaneous) if (!r.number(n)) return false;
        e.degree = r.u8();
        for (eq::Number& n : e.polynomial) if (!r.number(n)) return false;
        e.solverExpr = r.str(); e.solverLower = r.f64(); e.solverUpper = r.f64();
        e.solverVariable = static_cast<char>(r.u8());
        e.ineqDegree = r.u8(); e.ineqRelation = r.u8();
        for (eq::Number& n : e.inequality) if (!r.number(n)) return false;
        if (!r.ok || e.unknowns < 2 || e.unknowns > 6 || e.degree < 2 || e.degree > 6 || e.ineqDegree < 1 || e.ineqDegree > 6 || e.ineqRelation > 3) return false;
        if (e.solverExpr.size() > limits::kEquationExpression || !std::isfinite(e.solverLower) || !std::isfinite(e.solverUpper) || !(e.solverLower < e.solverUpper)) return false;
        if (e.solverVariable != 0 && (e.solverVariable < 'A' || e.solverVariable > 'Z')) return false;
        auto result = [&](CalcValue& v, std::size_t maxRows, std::size_t maxCols) {
            if (!r.value(v)) return false;
            if (!v.empty() && (v.type != ValueType::Matrix || v.rows > maxRows || v.cols > maxCols)) return false;
            const auto has = r.u8();
            if (has > 1 || (has && (v.empty() || v.complex() || v.size() > CalcValue::ExactElements))) return false;
            if (!has) return true;
            v.exact.resize(v.size());
            for (std::size_t k = 0; k < v.size(); ++k) {
                const ExactSlot slot = r.slot();
                if (!r.ok || !slot.valid) return false;
                v.exact[k] = slot.value;
                if (!(std::abs(slot.value.toDouble() - v.re[k]) <= 1e-12 * std::max(1.0, std::abs(v.re[k])))) return false;
            }
            return true;
        };
        if (!result(e.simResult, EquationState::MaxUnknowns, EquationState::MaxUnknowns + 1) ||
            !result(e.polyResult, EquationState::MaxDegree, 1)) return false;
    } else {
        // Il valore iniziale del Solver (v1-v7, sempre per X) diventa il valore di X,
        // che e' il valore iniziale della fx-CG50, se X non e' stata usata.
        if (legacyGuess != 0 && std::isfinite(legacyGuess) && !ws.memory.used('X')) ws.memory.store('X', legacyGuess);
    }
    if (version >= 9) {
        Settings& st = ws.settings;
        const auto file = r.u8(), sub = r.u8(), reg = r.u8();
        if (file >= limits::kListFiles || sub > 1 || (reg != 0xFF && reg >= static_cast<std::uint8_t>(stats::RegType::Count))) return false;
        st.listFile = file; st.subName = sub != 0; st.statRegType = reg;
        for (char slot : {CalculatorMemory::RSlot, CalculatorMemory::ThetaSlot}) {
            const double v = r.f64(); const ExactSlot e = r.slot(); const auto used = r.u8();
            if (!r.ok || used > 1 || !std::isfinite(v)) return false;
            if (used) { if (e.valid) ws.memory.storeExact(slot, v, e.value, e.decimal); else ws.memory.store(slot, v); }
        }
        for (std::size_t k = 0; k < ws.listFiles.size() && r.ok; ++k) {
            ListFileData& f = ws.listFiles[k];
            for (auto& l : f.lists) {
                const auto n = r.u16(); if (n > Workspace::MaxListLength) return false;
                l.resize(n); for (auto& d : l) { d = r.f64(); if (!std::isfinite(d)) return false; }
            }
            for (auto& n : f.names) {
                n = r.str();
                if (n.size() > limits::kListNameLength) return false;
                for (char ch : n) if (static_cast<unsigned char>(ch) < 0x20 || static_cast<unsigned char>(ch) > 0x7E || ch == '"') return false;
            }
            // Il file attivo e' in lists / listNames: il suo slot e' vuoto.
            if (k == file && (f.elements() != 0 || std::any_of(f.names.begin(), f.names.end(), [](const std::string& x) { return !x.empty(); }))) return false;
        }
        for (auto& f : ws.functionMemories) { f = r.str(); if (f.size() > limits::kFunctionMemoryLength) return false; }
        for (auto& t : ws.strings) { t = r.str(); if (t.size() > limits::kStringLength) return false; }
        ModeVariables& mv = ws.modeVars;
        for (double* d : {&mv.dynaStart, &mv.dynaEnd, &mv.dynaPitch, &mv.recStart, &mv.recEnd}) *d = r.f64();
        for (double& d : mv.recInitial) d = r.f64();
        for (double& d : mv.recNStart) d = r.f64();
        for (double* d : {&mv.tvmN, &mv.tvmI, &mv.tvmPV, &mv.tvmPMT, &mv.tvmFV, &mv.tvmPY, &mv.tvmCY}) *d = r.f64();
        mv.recFormula = r.str();
        if (mv.recFormula.size() > limits::kFunctionMemoryLength) return false;
        if (!r.ok || !std::isfinite(mv.dynaStart) || !std::isfinite(mv.tvmPY) || mv.tvmPY <= 0 || mv.tvmCY <= 0) return false;
        auto decimals = [&](CalcValue& v) {
            const auto has = r.u8();
            if (has > 1 || (has && !v.hasExact())) return false;
            if (!has) return true;
            v.decimal.assign(v.size(), false);
            for (std::size_t k = 0; k < v.size(); k += 8) {
                const std::uint8_t b = r.u8();
                for (std::size_t j = 0; j < 8 && k + j < v.size(); ++j) v.decimal[k + j] = (b >> j) & 1u;
            }
            return r.ok;
        };
        CollectionMemory& cm8 = ws.matrices;
        for (CalcValue& v : cm8.mat) if (!decimals(v)) return false;
        for (CalcValue& v : cm8.vct) if (!decimals(v)) return false;
        if (!decimals(cm8.matAns) || !decimals(cm8.vctAns) || !decimals(cm8.listAns)) return false;
        const auto extras8 = r.u16(); if (extras8 > ws.history.size()) return false;
        std::size_t next8 = 0;
        for (std::uint16_t k = 0; k < extras8 && r.ok; ++k) {
            const std::size_t index = r.u16();
            if (index < next8 || index >= ws.history.size()) return false;
            next8 = index + 1;
            HistoryEntry& h = ws.history[index];
            h.engShift = static_cast<std::int8_t>(r.u8());
            const auto shown = r.u8();
            h.displays.clear();
            for (std::uint8_t d = 0; d < shown && r.ok; ++d) h.displays.push_back(r.str());
            h.text = r.str();
            if (h.engShift < -40 || h.engShift > 40) return false;
        }
    }
    if (version >= 10) {
        Graph3DState& g3 = ws.graph3d;
        if (!g3d::decode(r.blob(MaxGraph3DSet), g3.set)) return false;
        g3.zoom = r.f32();
        g3.vmemUsed = r.u8();
        if (!r.ok || !(g3.zoom >= 0.05F && g3.zoom <= 20.0F) || g3.vmemUsed >= (1u << g3d::WindowMemories)) return false;
        for (g3d::Window& vw : g3.vmem) if (!g3d::decodeWindow(r.blob(256), vw)) return false;
        std::size_t bytes = 0;
        for (std::string& m : g3.gmem) {
            m = r.blob(MaxGraph3DSet);
            bytes += m.size();
            g3d::GraphSet probe;
            if (!m.empty() && !g3d::decode(m, probe)) return false;
        }
        if (bytes > limits::kGraph3DMemoryBytes) return false;
    }
    if (version >= 11) {
        // v1.15.0: coda TLV. Id crescenti, lunghezze dentro il record; le
        // sezioni conosciute si decodificano, le altre si conservano.
        const std::uint16_t n = r.u16();
        if (!r.ok || n > section::MaxSections) return false;
        int previous = -1;
        for (std::uint16_t k = 0; k < n; ++k) {
            const std::uint16_t id = r.u16(), sv = r.u16();
            const std::uint32_t len = r.u32();
            if (!r.ok || static_cast<int>(id) <= previous || len > section::MaxSectionBytes || !r.need(len)) return false;
            previous = id;
            const std::uint8_t* bytes = r.p + r.i;
            r.i += len;
            const SectionCodec* codec = nullptr;
            for (std::size_t c = 0; c < count; ++c) if (codecs[c].id == id) codec = &codecs[c];
            if (codec && sv <= codec->version) {
                if (!codec->decode(ws, sv, bytes, len)) return false;
            } else {
                RecordSection sec;
                sec.id = id; sec.version = sv;
                sec.data.assign(bytes, bytes + len);
                ws.preservedSections.push_back(std::move(sec));
            }
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
