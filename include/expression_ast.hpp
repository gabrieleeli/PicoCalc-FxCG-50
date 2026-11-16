#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fx::ast {
// Canonical VPAM tree shared by the editor, layout engine and evaluator.
//   Matrix: text = numero di colonne ("3"), children = le celle per righe
//           (nodi Row nell'editor, espressioni in un albero del parser).
//   List:   children = gli elementi di una lista letterale {a,b,c} (alberi del parser).
enum class Kind : std::uint8_t { Row, Token, Fraction, Radical, Power, Subscript, Group, Matrix, Calculus, Binary, Unary, Call, List };

// Indicatori impostati dal parser.
constexpr std::uint8_t FlagCollection = 1;   // il sotto-albero contiene matrici, vettori o liste

struct Node {
    Kind kind{Kind::Row};
    std::uint8_t flags{0};
    // Intervallo di byte del nodo nel testo analizzato (solo alberi del parser):
    // l'intera sotto-espressione, usata per portare il cursore sull'errore.
    std::uint16_t pos{0}, len{0};
    std::string text;
    std::vector<std::shared_ptr<Node>> children;
};
using NodePtr = std::shared_ptr<Node>;
inline NodePtr make(Kind kind, std::string text = {}) {
    auto n = std::make_shared<Node>(); n->kind = kind; n->text = std::move(text); return n;
}
inline bool involvesCollections(const Node& n) noexcept { return (n.flags & FlagCollection) != 0; }
// Colonne di un nodo Matrix (0 quando il testo non e' un numero di colonne valido).
inline std::size_t matrixColumns(const Node& n) noexcept {
    if (n.kind != Kind::Matrix || n.text.empty() || n.text.size() > 3) return 0;
    for (char c : n.text) if (c < '0' || c > '9') return 0;
    return static_cast<std::size_t>(std::atoi(n.text.c_str()));
}
inline bool validShape(const Node& n) noexcept {
    const auto count=n.children.size();
    switch(n.kind) {
    case Kind::Row: return true;
    case Kind::Matrix: { const std::size_t cols=matrixColumns(n); return cols>=1&&count>=cols&&count%cols==0; }
    case Kind::List: return count>=1;
    case Kind::Token: return count==0;
    case Kind::Fraction: case Kind::Power: case Kind::Subscript: case Kind::Binary: return count==2;
    case Kind::Radical: case Kind::Group: case Kind::Unary: return count==1;
    case Kind::Call: return !n.text.empty() && count>=1;
    // v1.12.0: d²/dx² (due campi) e Σ/Π con la variabile (espressione, variabile, inizio, fine).
    case Kind::Calculus: return ((n.text=="derivative"||n.text=="derivative2")&&count==2)||(n.text=="integral"&&count==3)||
                                ((n.text=="sum"||n.text=="prod")&&(count==3||count==4));
    }
    return false;
}

// Osservatore facoltativo di serialize(): registra dove finisce ogni Row
// dell'albero nel testo lineare, cosi' una posizione nel testo (quella di un
// errore del parser) si riporta a una posizione del cursore dell'editor.
struct SerializeMap {
    struct RowSpan {
        const Node* row{};
        std::vector<std::size_t> path;        // indici dei figli dalla radice alla riga
        std::size_t begin{}, end{};           // intervallo di byte della riga nel testo
        std::vector<std::size_t> childStart;  // posizione iniziale di ogni figlio della riga
    };
    std::vector<RowSpan> rows;
    std::vector<std::size_t> path;            // stato di lavoro durante la serializzazione
};

namespace detail {
inline void serializeInto(const Node& n, std::string& out, SerializeMap* map);
inline void serializeChild(const Node& n, std::size_t i, std::string& out, SerializeMap* map) {
    if(!n.children[i]) throw std::runtime_error("Figlio AST nullo");
    if(map) map->path.push_back(i);
    serializeInto(*n.children[i], out, map);
    if(map) map->path.pop_back();
}
inline void serializeInto(const Node& n, std::string& out, SerializeMap* map) {
    if(!validShape(n)) throw std::runtime_error("Nodo VPAM incompleto o non valido");
    auto child=[&](std::size_t i){ serializeChild(n, i, out, map); };
    switch(n.kind) {
    case Kind::Row: {
        std::size_t slot=0;
        if(map){ slot=map->rows.size(); map->rows.push_back({&n, map->path, out.size(), out.size(), {}}); map->rows[slot].childStart.reserve(n.children.size()); }
        for(std::size_t i=0;i<n.children.size();++i){ if(map) map->rows[slot].childStart.push_back(out.size()); child(i); }
        if(map) map->rows[slot].end=out.size();
        return;
    }
    case Kind::Token: out+=n.text; return;
    case Kind::Fraction: out+="("; child(0); out+=")/("; child(1); out+=")"; return;
    case Kind::Radical:
        if(n.text.empty()||n.text=="2") out+="sqrt("; else { out+="root("; out+=n.text; out+=","; }
        child(0); out+=")"; return;
    case Kind::Power: out+="("; child(0); out+=")^("; child(1); out+=")"; return;
    case Kind::Binary: out+="("; child(0); out+=")"; out+=n.text; out+="("; child(1); out+=")"; return;
    case Kind::Unary: out+=n.text; out+="("; child(0); out+=")"; return;
    case Kind::Call:
        // Repeating digits of a decimal (editor overline template): "0.1" + "_6" = 0.1666...
        if(n.text=="period"&&n.children.size()==1){ out+="_"; child(0); return; }
        out+=n.text; out+="(";
        for(std::size_t i=0;i<n.children.size();++i){ if(i) out+=","; child(i); }
        out+=")"; return;
    case Kind::Group: out+="("; child(0); out+=")"; return;
    case Kind::Subscript: throw std::runtime_error("Gli indici non sono ancora valutabili");
    case Kind::Matrix: {
        // forma lineare della fx-CG50: [[a,b][c,d]]
        const std::size_t cols=matrixColumns(n);
        out+="[";
        for(std::size_t i=0;i<n.children.size();++i){
            if(i%cols==0) out+="["; else out+=",";
            child(i);
            if(i%cols==cols-1) out+="]";
        }
        out+="]"; return;
    }
    case Kind::List:
        out+="{";
        for(std::size_t i=0;i<n.children.size();++i){ if(i) out+=","; child(i); }
        out+="}"; return;
    case Kind::Calculus:
        out+=n.text; out+="(";
        for(std::size_t i=0;i<n.children.size();++i){ if(i) out+=","; child(i); }
        out+=")"; return;
    }
    throw std::runtime_error("Tipo di nodo VPAM sconosciuto");
}
} // namespace detail

// Testo lineare canonico di un albero; `map` (facoltativo) riceve gli intervalli delle righe.
inline std::string serialize(const Node& n, SerializeMap* map = nullptr) {
    std::string out;
    if(map){ map->rows.clear(); map->path.clear(); }
    detail::serializeInto(n, out, map);
    return out;
}
} // namespace fx::ast
