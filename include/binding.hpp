#pragma once
// Espressioni con variabili legate (v1.15.0, F8; solo Pimoroni Pico Plus 2 W).
//
// BindingResolver e' un NameResolver/CollectionResolver che si mette davanti a
// quelli del workspace: aggiunge le variabili legate da una modalita' (n, an,
// an+1, an+2, bn, cn di Recursion con i nomi interni rec_*, le variabili TVM
// di Financial, le variabili locali di un programma) e le celle di un foglio
// (cell_a1 ... cell_z999 e gli intervalli range_a1_b5, letti dallo stesso
// lexer con ParseOptions::cellReferences). Cio' che non conosce lo chiede ai
// resolver padre: nessun secondo sistema di variabili, nessun secondo parser.
// DependencyGraph ordina le valutazioni senza ricorsione (algoritmo di Kahn) e
// rifiuta i cicli con il messaggio della fx-CG50 "Circular ERROR".
#include "expression.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace fx::bind {

constexpr const char* CircularError = "Circular ERROR";
constexpr std::size_t MaxBoundNames = 64;

struct CellRef {
    int col{0};                                  // 0..25 = A..Z
    int row{1};                                  // 1..999
    bool operator==(const CellRef& o) const noexcept { return col == o.col && row == o.row; }
};
std::string cellVariable(CellRef c);             // {1, 12} -> "cell_b12"
bool parseCellVariable(const std::string& lowerName, CellRef& out) noexcept;
bool parseRangeVariable(const std::string& lowerName, CellRef& from, CellRef& to) noexcept;

class BindingResolver final : public NameResolver, public CollectionResolver {
public:
    // Lettura di una cella: false = vuota o non numerica (Math ERROR nella formula).
    using CellReader = std::function<bool(CellRef cell, double& out)>;
    explicit BindingResolver(const NameResolver* names = nullptr, const CollectionResolver* collections = nullptr) noexcept
        : parentNames_(names), parentCollections_(collections) {}
    void bind(const std::string& lowerName, double value);   // aggiunge o aggiorna (oltre MaxBoundNames: Memory ERROR)
    bool unbind(const std::string& lowerName) noexcept;
    bool bound(const std::string& lowerName) const noexcept;
    void clear() noexcept { names_.clear(); }
    void setCells(CellReader reader) { cells_ = std::move(reader); }
    // NameResolver: nomi legati, poi celle, poi il padre.
    bool resolve(const std::string& lowerName, double& out) const override;
    // CollectionResolver: intervalli di celle come liste, il resto dal padre.
    bool collection(Memory memory, int index, CalcValue& out) const override;
    int listByName(const std::string& subName) const override;
    bool special(const std::string& lowerName, CalcValue& out) const override;
    // Contesto di valutazione con questo resolver al posto di quelli di `base`.
    EvalContext context(EvalContext base) const;
private:
    const NameResolver* parentNames_;
    const CollectionResolver* parentCollections_;
    std::vector<std::pair<std::string, double>> names_;
    CellReader cells_;
};

// Celle e intervalli citati da un albero (visita iterativa, senza ricorsione).
std::vector<CellRef> cellReferences(const ast::Node& root);

// Grafo delle dipendenze: il nodo `from` dipende da `to`. order() restituisce
// un ordine in cui ogni nodo segue quelli da cui dipende; con un ciclo lancia
// MathError("Circular ERROR") e cycleNode() e' un nodo del ciclo.
class DependencyGraph {
public:
    explicit DependencyGraph(std::size_t nodes = 0) { resize(nodes); }
    void resize(std::size_t nodes);
    std::size_t size() const noexcept { return deps_.size(); }
    void depend(std::size_t from, std::size_t to);
    std::vector<std::size_t> order() const;
    // Nodi da ricalcolare quando cambia `changed` (lui compreso), in ordine.
    std::vector<std::size_t> affected(std::size_t changed) const;
    std::size_t cycleNode() const noexcept { return cycle_; }
private:
    std::vector<std::vector<std::size_t>> deps_;     // deps_[a]: nodi da cui a dipende
    mutable std::size_t cycle_{static_cast<std::size_t>(-1)};
};

} // namespace fx::bind
