// Espressioni con variabili legate (v1.15.0, F8): vedi binding.hpp.
#include "binding.hpp"
#include <algorithm>

namespace fx::bind {

std::string cellVariable(CellRef c) {
    return "cell_" + std::string(1, static_cast<char>('a' + c.col)) + std::to_string(c.row);
}
bool parseCellVariable(const std::string& n, CellRef& out) noexcept {
    if (!isCellVariableName(n)) return false;
    int row = 0;
    for (std::size_t i = 6; i < n.size(); ++i) row = row * 10 + (n[i] - '0');
    out = CellRef{n[5] - 'a', row};
    return true;
}
bool parseRangeVariable(const std::string& n, CellRef& from, CellRef& to) noexcept {
    if (!isCellRangeName(n)) return false;
    try {
        const std::size_t u = n.find('_', 6);
        return parseCellVariable("cell_" + n.substr(6, u - 6), from) && parseCellVariable("cell_" + n.substr(u + 1), to);
    } catch (...) {
        return false;
    }
}

void BindingResolver::bind(const std::string& name, double value) {
    for (auto& e : names_) if (e.first == name) { e.second = value; return; }
    if (names_.size() >= MaxBoundNames) throw MathError("Memory ERROR: troppe variabili legate");
    names_.emplace_back(name, value);
}
bool BindingResolver::unbind(const std::string& name) noexcept {
    const auto it = std::find_if(names_.begin(), names_.end(), [&](const auto& e) { return e.first == name; });
    if (it == names_.end()) return false;
    names_.erase(it);
    return true;
}
bool BindingResolver::bound(const std::string& name) const noexcept {
    return std::any_of(names_.begin(), names_.end(), [&](const auto& e) { return e.first == name; });
}

bool BindingResolver::resolve(const std::string& name, double& out) const {
    for (const auto& e : names_) if (e.first == name) { out = e.second; return true; }
    CellRef c;
    if (cells_ && parseCellVariable(name, c)) {
        if (!cells_(c, out)) throw MathError("Math ERROR: cella " + std::string(1, static_cast<char>('A' + c.col)) + std::to_string(c.row) + " vuota o non numerica");
        return true;
    }
    return parentNames_ && parentNames_->resolve(name, out);
}
bool BindingResolver::collection(Memory memory, int index, CalcValue& out) const {
    return parentCollections_ && parentCollections_->collection(memory, index, out);
}
int BindingResolver::listByName(const std::string& subName) const {
    return parentCollections_ ? parentCollections_->listByName(subName) : -1;
}
bool BindingResolver::special(const std::string& name, CalcValue& out) const {
    CellRef a, b;
    if (cells_ && parseRangeVariable(name, a, b)) {
        // Intervallo come lista, per colonne (A1:B2 = A1, A2, B1, B2): le celle vuote non contano.
        const int c0 = std::min(a.col, b.col), c1 = std::max(a.col, b.col), r0 = std::min(a.row, b.row), r1 = std::max(a.row, b.row);
        std::vector<double> values;
        for (int col = c0; col <= c1; ++col)
            for (int row = r0; row <= r1; ++row) {
                double v;
                if (cells_(CellRef{col, row}, v)) values.push_back(v);
            }
        if (values.empty()) throw MathError("Math ERROR: intervallo senza valori");
        out = CalcValue{};
        out.type = ValueType::List;
        out.rows = static_cast<std::uint16_t>(std::min<std::size_t>(values.size(), 0xFFFF));
        out.cols = 1;
        out.re = std::move(values);
        return true;
    }
    return parentCollections_ && parentCollections_->special(name, out);
}
EvalContext BindingResolver::context(EvalContext base) const {
    base.names = this;
    base.collections = this;
    return base;
}

std::vector<CellRef> cellReferences(const ast::Node& root) {
    std::vector<CellRef> out;
    std::vector<const ast::Node*> stack{&root};
    while (!stack.empty()) {
        const ast::Node* n = stack.back();
        stack.pop_back();
        CellRef a, b;
        if (parseCellVariable(n->text, a)) out.push_back(a);
        else if (parseRangeVariable(n->text, a, b)) {
            for (int col = std::min(a.col, b.col); col <= std::max(a.col, b.col); ++col)
                for (int row = std::min(a.row, b.row); row <= std::max(a.row, b.row); ++row) out.push_back(CellRef{col, row});
        }
        for (const auto& c : n->children) if (c) stack.push_back(c.get());
    }
    return out;
}

void DependencyGraph::resize(std::size_t nodes) { deps_.assign(nodes, {}); }
void DependencyGraph::depend(std::size_t from, std::size_t to) {
    if (from >= deps_.size() || to >= deps_.size()) throw std::out_of_range("DependencyGraph");
    auto& d = deps_[from];
    if (std::find(d.begin(), d.end(), to) == d.end()) d.push_back(to);
}
std::vector<std::size_t> DependencyGraph::order() const {
    const std::size_t n = deps_.size();
    std::vector<std::size_t> pending(n, 0);
    std::vector<std::vector<std::size_t>> users(n);
    for (std::size_t a = 0; a < n; ++a)
        for (std::size_t b : deps_[a]) { ++pending[a]; users[b].push_back(a); }
    std::vector<std::size_t> ready, out;
    out.reserve(n);
    for (std::size_t a = 0; a < n; ++a) if (pending[a] == 0) ready.push_back(a);
    std::reverse(ready.begin(), ready.end());                 // ordine stabile: indici crescenti
    while (!ready.empty()) {
        const std::size_t a = ready.back();
        ready.pop_back();
        out.push_back(a);
        for (std::size_t u : users[a]) if (--pending[u] == 0) ready.push_back(u);
    }
    if (out.size() != n) {
        for (std::size_t a = 0; a < n; ++a) if (pending[a] != 0) { cycle_ = a; break; }
        throw MathError(CircularError);
    }
    cycle_ = static_cast<std::size_t>(-1);
    return out;
}
std::vector<std::size_t> DependencyGraph::affected(std::size_t changed) const {
    const std::size_t n = deps_.size();
    if (changed >= n) return {};
    std::vector<std::vector<std::size_t>> users(n);
    for (std::size_t a = 0; a < n; ++a) for (std::size_t b : deps_[a]) users[b].push_back(a);
    std::vector<bool> mark(n, false);
    std::vector<std::size_t> stack{changed};
    mark[changed] = true;
    while (!stack.empty()) {
        const std::size_t a = stack.back();
        stack.pop_back();
        for (std::size_t u : users[a]) if (!mark[u]) { mark[u] = true; stack.push_back(u); }
    }
    std::vector<std::size_t> out;
    for (std::size_t a : order()) if (mark[a]) out.push_back(a);
    return out;
}

} // namespace fx::bind
