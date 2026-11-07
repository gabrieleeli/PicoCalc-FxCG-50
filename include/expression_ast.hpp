#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fx::ast {
// Canonical VPAM tree shared by the editor, layout engine and evaluator.
enum class Kind { Row, Token, Fraction, Radical, Power, Subscript, Group, Matrix, Calculus, Binary, Unary, Call };
struct Node {
    Kind kind{Kind::Row};
    std::string text;
    std::vector<std::shared_ptr<Node>> children;
};
using NodePtr = std::shared_ptr<Node>;
inline NodePtr make(Kind kind, std::string text = {}) {
    auto n = std::make_shared<Node>(); n->kind = kind; n->text = std::move(text); return n;
}
inline bool validShape(const Node& n) noexcept {
    const auto count=n.children.size();
    switch(n.kind) {
    case Kind::Row: case Kind::Matrix: return true;
    case Kind::Token: return count==0;
    case Kind::Fraction: case Kind::Power: case Kind::Subscript: case Kind::Binary: return count==2;
    case Kind::Radical: case Kind::Group: case Kind::Unary: return count==1;
    case Kind::Call: return !n.text.empty() && count>=1;
    case Kind::Calculus: return (n.text=="derivative"&&count==2)||((n.text=="integral"||n.text=="sum")&&count==3);
    }
    return false;
}
inline std::string serialize(const Node& n) {
    if(!validShape(n)) throw std::runtime_error("Nodo VPAM incompleto o non valido");
    auto child=[&](std::size_t i)->std::string {
        if(!n.children[i]) throw std::runtime_error("Figlio AST nullo");
        return serialize(*n.children[i]);
    };
    switch(n.kind) {
    case Kind::Row: { std::string out; for(const auto& c:n.children) { if(!c) throw std::runtime_error("Figlio AST nullo"); out+=serialize(*c); } return out; }
    case Kind::Token: return n.text;
    case Kind::Fraction: return "("+child(0)+")/("+child(1)+")";
    case Kind::Radical: return (n.text.empty()||n.text=="2") ? "sqrt("+child(0)+")" : "root("+n.text+","+child(0)+")";
    case Kind::Power: return "("+child(0)+")^("+child(1)+")";
    case Kind::Binary: return "("+child(0)+")"+n.text+"("+child(1)+")";
    case Kind::Unary: return n.text+"("+child(0)+")";
    case Kind::Call: { std::string out=n.text+"("; for(std::size_t i=0;i<n.children.size();++i){if(i)out+=",";out+=child(i);} return out+")"; }
    case Kind::Group: return "("+child(0)+")";
    case Kind::Subscript: throw std::runtime_error("Gli indici non sono ancora valutabili");
    case Kind::Matrix: throw std::runtime_error("Le matrici non sono ancora collegate al valutatore scalare");
    case Kind::Calculus:
        return n.text+"("+child(0)+","+child(1)+(n.children.size()==3?","+child(2):"")+")";
    }
    throw std::runtime_error("Tipo di nodo VPAM sconosciuto");
}
} // namespace fx::ast
