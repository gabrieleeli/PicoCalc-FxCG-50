// Parser di C e C++ (v1.20.0): token preprocessati -> albero comune.
//
// Come i parser veri del C, distingue dichiarazioni ed espressioni sapendo
// quali nomi sono tipi (primitivi, typedef, struct/class/enum, template,
// parametri dei template, nomi di std:: con "using namespace std"). I
// dichiaratori seguono la regola "dall'interno verso l'esterno" (puntatori a
// funzione, array di puntatori). Ricorsione limitata da MaxNesting.
#include "program_ast.hpp"
#include <algorithm>
#include <set>
#include <unordered_set>

namespace fx::prog::ast {
namespace {

const char* const kStdTypes[] = {"string", "vector", "map", "set", "unordered_map", "unordered_set", "pair", "stack", "queue",
    "deque", "list", "ostream", "istream", "ifstream", "ofstream", "fstream", "stringstream", "istringstream", "ostringstream",
    "exception", "runtime_error", "logic_error", "out_of_range", "invalid_argument", "length_error", "domain_error",
    "overflow_error", "underflow_error", "range_error", "bad_alloc", "bad_cast", "size_t", "greater", "less", "priority_queue",
    "function", "unique_ptr", "shared_ptr", "array", "tuple", "optional", "variant", "string_view", "ptrdiff_t", "int8_t",
    "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "multimap", "multiset", "bitset", nullptr};
const char* const kStdTemplates[] = {"vector", "map", "set", "unordered_map", "unordered_set", "pair", "stack", "queue", "deque",
    "list", "greater", "less", "priority_queue", "function", "unique_ptr", "shared_ptr", "array", "tuple", "optional",
    "variant", "multimap", "multiset", "bitset", "make_pair", "numeric_limits", "make_unique", "make_shared", nullptr};
// Nomi di tipo sempre noti (typedef delle intestazioni standard).
const char* const kAlwaysTypes[] = {"size_t", "ptrdiff_t", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
    "uint32_t", "uint64_t", "intptr_t", "uintptr_t", "FILE", "time_t", "clock_t", "wchar_t", "char16_t", "char32_t",
    "FxStat1", "FxStat2", "FxReg", nullptr};                // risultati della libreria fx (C)

class CParser {
public:
    CParser(const Lexed& lx, Unit& unit, ParseContext& ctx) : lx_(lx), toks_(lx.tokens), unit_(unit), ctx_(ctx), syms_(*ctx.syms), pool_(*ctx.pool) {
        cpp_ = unit.dialect == Dialect::Cpp;
        for (const char* const* t = kAlwaysTypes; *t; ++t) known_.insert(*t);
        if (cpp_) {
            for (const char* const* t = kStdTypes; *t; ++t) known_.insert(std::string("std::") + *t);
            for (const char* const* t = kStdTemplates; *t; ++t) templates_.insert(std::string("std::") + *t);
            known_.insert("std::vector::iterator");
            for (const char* t : {"fx::Stat1", "fx::Stat2", "fx::Reg"}) known_.insert(t);   // risultati della libreria fx (C++)
        }
    }
    void run() {
        Node* root = pool_.node(K::Unit, cur().pos);
        unit_.root = root;
        while (!atEnd()) topLevel(root->list);
    }

private:
    // ------------------------------------------------------------ token
    const Token& cur() const { return toks_[p_]; }
    const Token& peek(std::size_t k = 1) const { return toks_[std::min(p_ + k, toks_.size() - 1)]; }
    bool atEnd() const { return cur().t == T::End; }
    bool isP(P p) const { return cur().t == T::Punct && cur().p == p; }
    bool isP(const Token& t, P p) const { return t.t == T::Punct && t.p == p; }
    bool isWord(const char* w) const { return cur().t == T::Ident && syms_[cur().sym] == w; }
    bool isWord(const Token& t, const char* w) const { return t.t == T::Ident && syms_[t.sym] == w; }
    bool accept(P p) { if (isP(p)) { ++p_; return true; } return false; }
    bool acceptWord(const char* w) { if (isWord(w)) { ++p_; return true; } return false; }
    void expect(P p) {
        if (accept(p)) return;
        // '>>' chiude due liste di argomenti dei template (C++11)
        if (p == Gt && isP(Shr)) { toks_[p_].p = Gt; return; }
        fail(std::string("atteso '") + punctText(p) + "'" + near(), cur().pos);
    }
    std::string near() const {
        const Token& t = cur();
        if (t.t == T::End) return " alla fine del file";
        if (t.t == T::Ident) return " prima di '" + syms_[t.sym] + "'";
        if (t.t == T::Punct) return std::string(" prima di '") + punctText(t.p) + "'";
        return "";
    }
    Sym ident(const char* what = "un nome") {
        if (cur().t != T::Ident || isKeyword(cur().sym)) fail(std::string("atteso ") + what + near(), cur().pos);
        return toks_[p_++].sym;
    }
    [[noreturn]] void error(const std::string& m) const { fail(m, cur().pos); }
    struct Depth {
        explicit Depth(CParser& c) : c_(c) {
            if (++c_.depth_ > ParseDepth) fail("Stack ERROR: annidamento troppo profondo nel sorgente", c_.cur().pos);
            if (++c_.polls_ % 2048 == 0 && c_.ctx_.poll) c_.ctx_.poll();
        }
        ~Depth() { --c_.depth_; }
        CParser& c_;
    };
    bool isKeyword(Sym s) const {
        static const char* const kC[] = {"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum",
            "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short", "signed", "sizeof",
            "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Bool", nullptr};
        static const char* const kCpp[] = {"class", "namespace", "template", "typename", "this", "new", "delete", "public", "private",
            "protected", "virtual", "friend", "operator", "using", "try", "catch", "throw", "explicit", "mutable", "static_cast",
            "dynamic_cast", "const_cast", "reinterpret_cast", "constexpr", "noexcept", "decltype", "typeid", "nullptr", "true", "false", "bool", nullptr};
        const std::string& w = syms_[s];
        for (const char* const* k = kC; *k; ++k) if (w == *k) return true;
        if (cpp_) for (const char* const* k = kCpp; *k; ++k) if (w == *k) return true;
        return false;
    }

    // ------------------------------------------------------------ nomi di tipo
    std::string nsPrefix() const {
        std::string s;
        for (const auto& n : ns_) s += n + "::";
        return s;
    }
    // `path` = "a::b::c" (gia' unito). Vero se e' un tipo visibile da qui.
    bool knownType(const std::string& path) const {
        if (path.empty()) return false;
        for (const auto& tp : tparams_) for (Sym s : tp) if (syms_[s] == path) return true;
        if (lookup(known_, path)) return true;
        return false;
    }
    bool knownTemplate(const std::string& path) const { return lookup(templates_, path); }
    // Definizione di un costruttore o distruttore fuori dalla classe: A::A( , A<T>::A( , A::~A(
    bool ctorDefinitionAhead() const {
        std::size_t q = p_;
        if (isP(toks_[q], Scope)) ++q;
        Sym last = 0;
        while (toks_[q].t == T::Ident) {
            const Sym s = toks_[q].sym;
            ++q;
            if (isP(toks_[q], Lt) && knownTemplate(syms_[s])) {
                int depth = 0;
                for (; q < toks_.size() && toks_[q].t != T::End; ++q) {
                    if (isP(toks_[q], Lt)) ++depth;
                    else if (isP(toks_[q], Gt)) { if (--depth == 0) { ++q; break; } }
                    else if (isP(toks_[q], Shr)) { depth -= 2; if (depth <= 0) { ++q; break; } }
                    else if (isP(toks_[q], Semi) || isP(toks_[q], LBrace)) return false;
                }
            }
            if (!isP(toks_[q], Scope)) return false;
            ++q;
            if (isP(toks_[q], Tilde)) return toks_[q + 1].t == T::Ident && toks_[q + 1].sym == s && isP(toks_[q + 2], LParen);
            if (toks_[q].t == T::Ident && toks_[q].sym == s && isP(toks_[q + 1], LParen)) return true;
            last = s;
        }
        (void)last;
        return false;
    }
    bool isTParam(const std::string& path) const {
        for (const auto& tp : tparams_) for (Sym s : tp) if (syms_[s] == path) return true;
        return false;
    }
    bool lookup(const std::set<std::string>& set, const std::string& path) const {
        // classe e namespace che racchiudono (dal piu' interno), poi la radice, poi using namespace
        std::vector<std::string> scopes;
        for (std::size_t k = classes_.size(); k-- > 0;) scopes.push_back(classes_[k] + "::");
        std::string acc;
        std::vector<std::string> nsScopes{""};
        for (const auto& n : ns_) { acc += n + "::"; nsScopes.push_back(acc); }
        for (std::size_t k = nsScopes.size(); k-- > 0;) scopes.push_back(nsScopes[k]);
        for (const auto& sc : scopes) if (set.count(sc + path)) return true;
        for (const auto& u : usingNs_) if (set.count(u + "::" + path)) return true;
        return false;
    }
    void declareType(Sym name, bool isTemplate = false) {
        std::string full = classes_.empty() ? nsPrefix() : classes_.back() + "::";
        full += syms_[name];
        known_.insert(full);
        if (isTemplate) templates_.insert(full);
    }
    // Il token corrente inizia un nome di tipo (con eventuali qualificatori)?
    // `len` = token del nome qualificato (senza argomenti dei template).
    bool typeNameAt(std::size_t at, std::string* pathOut = nullptr, std::size_t* lenOut = nullptr) const {
        std::size_t q = at;
        std::string path;
        if (isP(toks_[q], Scope)) ++q;
        while (true) {
            if (toks_[q].t != T::Ident) return false;
            path += syms_[toks_[q].sym];
            ++q;
            // argomenti di un template: Nome<...>::
            if (isP(toks_[q], Lt) && knownTemplate(path)) {
                int depth = 0;
                std::size_t r = q;
                for (; r < toks_.size() && toks_[r].t != T::End; ++r) {
                    if (isP(toks_[r], Lt)) ++depth;
                    else if (isP(toks_[r], Gt)) { if (--depth == 0) break; }
                    else if (isP(toks_[r], Shr)) { depth -= 2; if (depth <= 0) break; }
                    else if (isP(toks_[r], Semi) || isP(toks_[r], LBrace)) return false;
                }
                if (r >= toks_.size() || toks_[r].t == T::End) return false;
                if (isP(toks_[r + 1], Scope) && toks_[r + 2].t == T::Ident) {
                    // Nome<T>::iterator e simili
                    path += "::" + syms_[toks_[r + 2].sym];
                    if (pathOut) *pathOut = path;
                    if (lenOut) *lenOut = q - at;
                    return knownType(path) || syms_[toks_[r + 2].sym] == "iterator" || syms_[toks_[r + 2].sym] == "const_iterator" ||
                           syms_[toks_[r + 2].sym] == "size_type" || syms_[toks_[r + 2].sym] == "value_type";
                }
                break;
            }
            if (isP(toks_[q], Scope) && toks_[q + 1].t == T::Ident) {
                // a::b: tipo se il percorso completo lo e', anche se `a` e' un namespace
                const std::string longer = path + "::" + syms_[toks_[q + 1].sym];
                if (knownType(path) && !knownType(longer) && !nsKnown(path)) break;
                path += "::";
                ++q;
                continue;
            }
            break;
        }
        if (pathOut) *pathOut = path;
        if (lenOut) *lenOut = q - at;
        return knownType(path);
    }
    bool nsKnown(const std::string& path) const { return path == "std" || namespaces_.count(path) != 0; }
    bool builtinWord(const Token& t) const {
        if (t.t != T::Ident) return false;
        const std::string& w = syms_[t.sym];
        return w == "void" || w == "char" || w == "short" || w == "int" || w == "long" || w == "float" || w == "double" ||
               w == "signed" || w == "unsigned" || w == "_Bool" || w == "bool" || (cpp_ && w == "auto") || w == "wchar_t";
    }
    bool specifierWord(const Token& t) const {
        if (t.t != T::Ident) return false;
        const std::string& w = syms_[t.sym];
        return w == "const" || w == "static" || w == "extern" || w == "inline" || w == "register" || w == "volatile" ||
               w == "restrict" || (cpp_ && (w == "virtual" || w == "explicit" || w == "mutable" || w == "constexpr" || w == "friend" || w == "typename")) ||
               w == "struct" || w == "union" || w == "enum" || (cpp_ && w == "class");
    }
    // Inizia una dichiarazione (tipo, specificatori)?
    bool startsDecl() const {
        const Token& t = cur();
        if (t.t != T::Ident) return isP(Scope) && typeNameAt(p_);
        if (builtinWord(t) || specifierWord(t)) return true;
        if (cpp_ && isWord("auto")) return true;
        std::string path;
        std::size_t len = 0;
        if (!typeNameAt(p_, &path, &len)) return false;
        // T(...) all'inizio di un'istruzione: espressione (conversione o costruzione)
        std::size_t q = p_ + len;
        if (isP(toks_[q], Lt)) {                         // salta gli argomenti del template
            int depth = 0;
            for (; q < toks_.size(); ++q) {
                if (isP(toks_[q], Lt)) ++depth;
                else if (isP(toks_[q], Gt)) { if (--depth == 0) { ++q; break; } }
                else if (isP(toks_[q], Shr)) { depth -= 2; if (depth <= 0) { ++q; break; } }
                else if (toks_[q].t == T::End) return false;
            }
            while (isP(toks_[q], Scope) && toks_[q + 1].t == T::Ident) q += 2;
        }
        const Token& n = toks_[q];
        if (n.t == T::Ident) return true;                // T x
        if (isP(n, Star) || isP(n, Amp) || isP(n, AndAnd)) {
            // T * x  /  T & x
            std::size_t r = q;
            while (isP(toks_[r], Star) || isP(toks_[r], Amp) || isP(toks_[r], AndAnd) || isWord(toks_[r], "const")) ++r;
            return toks_[r].t == T::Ident || isP(toks_[r], LParen);
        }
        if (isP(n, LParen) && isP(toks_[q + 1], Star)) return true;   // T (*f)(...)
        if (isWord(n, "const")) return true;
        return false;
    }

    // ------------------------------------------------------------ tipi
    // Specificatori di tipo e di memoria: tipo base e modificatori.
    TypeNode* declSpecifiers(std::uint32_t& mods, Node** definedClass = nullptr) {
        Depth guard(*this);
        const Pos at = cur().pos;
        bool isUnsigned = false, isSigned = false, any = false, isConst = false;
        int longs = 0;
        std::string base;
        TypeNode* named = nullptr;
        while (true) {
            const Token& t = cur();
            if (t.t != T::Ident && !isP(Scope)) break;
            if (t.t == T::Ident) {
                const std::string w = syms_[t.sym];
                if (w == "const") { isConst = true; ++p_; continue; }
                if (w == "volatile" || w == "restrict" || w == "register") { ++p_; continue; }
                if (w == "static") { mods |= MStatic; ++p_; continue; }
                if (w == "extern") {
                    ++p_;
                    if (cur().t == T::Str) ++p_;             // extern "C"
                    mods |= MExtern;
                    continue;
                }
                if (w == "inline") { mods |= MInline; ++p_; continue; }
                if (cpp_ && w == "virtual") { mods |= MVirtual; ++p_; continue; }
                if (cpp_ && w == "explicit") { mods |= MExplicit; ++p_; continue; }
                if (cpp_ && w == "mutable") { ++p_; continue; }
                if (cpp_ && w == "constexpr") { mods |= MConstexpr | MConst; isConst = true; ++p_; continue; }
                if (cpp_ && w == "friend") { mods |= MFriend; ++p_; continue; }
                if (cpp_ && w == "typename") { ++p_; continue; }
                if (w == "thread_local" || w == "_Thread_local") error("thread_local non supportato (niente thread)");
                if (w == "unsigned") { isUnsigned = true; any = true; ++p_; continue; }
                if (w == "signed") { isSigned = true; any = true; ++p_; continue; }
                if (w == "long") { ++longs; any = true; ++p_; continue; }
                if (w == "short" || w == "int" || w == "char" || w == "float" || w == "double" || w == "void" || w == "bool" || w == "_Bool" || (cpp_ && w == "auto")) {
                    if (!base.empty() && !(base == "int" && (w == "int"))) {
                        if (!(base == "short" && w == "int") && !(base == "int" && w == "short")) error("due tipi nella stessa dichiarazione");
                    }
                    if (w == "int" && (base == "short")) { ++p_; any = true; continue; }
                    base = w == "_Bool" ? "bool" : w;
                    any = true;
                    ++p_;
                    continue;
                }
                if (w == "wchar_t" || w == "char16_t" || w == "char32_t") error("caratteri larghi non supportati");
                if (w == "struct" || w == "class" || w == "union" || w == "enum") {
                    if (named || !base.empty()) error("due tipi nella stessa dichiarazione");
                    named = classSpecifier(definedClass);
                    any = true;
                    continue;
                }
                if (w == "decltype") error("decltype non supportato");
            }
            if (any || named) break;
            // A::A(...) / A::~A() / ns::A::A(...) fuori dalla classe: niente tipo di ritorno
            if (cpp_ && ctorDefinitionAhead()) { base = "void"; any = true; break; }
            // nome di tipo (typedef, classe, template, std::...)
            if (typeNameAt(p_)) { named = qualifiedType(); any = true; continue; }
            break;
        }
        if (!any) error("atteso un tipo" + near());
        TypeNode* t = named;
        if (!t) {
            if (base.empty()) base = "int";
            if (base == "char") base = isUnsigned ? "unsigned char" : isSigned ? "signed char" : "char";
            else if (base == "short") base = isUnsigned ? "unsigned short" : "short";
            else if (base == "int") {
                if (longs >= 2) base = isUnsigned ? "unsigned long long" : "long long";
                else if (longs == 1) base = isUnsigned ? "unsigned long" : "long";
                else base = isUnsigned ? "unsigned int" : "int";
            } else if (base == "double" && longs) base = "double";   // long double = double
            else if ((isUnsigned || isSigned || longs) && base != "int") error("unsigned/long con un tipo non intero");
            t = pool_.type(TF::Named, at);
            if (base == "auto") t->f = TF::Auto;
            t->name = syms_.intern(base);
            t->builtin = true;
        }
        if (isConst) t->isConst = true;
        return t;
    }
    // Nome di tipo qualificato con argomenti dei template.
    TypeNode* qualifiedType() {
        Depth guard(*this);
        TypeNode* t = pool_.type(TF::Named, cur().pos);
        if (accept(Scope)) t->global = true;
        std::string path;
        while (true) {
            const Sym s = ident("un nome di tipo");
            path += syms_[s];
            if (isP(Lt) && knownTemplate(path)) {
                t->args = templateArgs();
            }
            if (isP(Scope) && peek().t == T::Ident) {
                const std::string longer = path + "::" + syms_[peek().sym];
                // Tipo::nome e' un tipo solo se lo e' il percorso completo (Col::Verde, B::s() sono valori;
                // T::value_type di un parametro del template resta un tipo dipendente)
                if (knownType(longer) || nsKnown(path) || !t->args.empty() || isTParam(path)) {
                    if (!t->args.empty()) {
                        // Nome<T>::membro: tipo annidato del template (iterator...)
                        t->qual.push_back(s);
                        ++p_;
                        TypeNode* outer = pool_.type(TF::Named, t->pos);
                        *outer = *t;
                        t = pool_.type(TF::Named, outer->pos);
                        t->inner = outer;                // proprietario con argomenti
                        t->name = ident("un nome di tipo");
                        return t;
                    }
                    t->qual.push_back(s);
                    path += "::";
                    ++p_;
                    continue;
                }
            }
            t->name = s;
            break;
        }
        return t;
    }
    std::vector<TypeNode*> templateArgs() {
        Depth guard(*this);
        std::vector<TypeNode*> args;
        expect(Lt);
        if (isP(Gt)) { ++p_; return args; }
        while (true) {
            if (startsTypeArg()) args.push_back(typeId());
            else {
                // argomento intero (template<int N>)
                TypeNode* v = pool_.type(TF::Named, cur().pos);
                v->name = syms_.intern("#value");
                // il '>' chiude gli argomenti (Pila<int, 4> p): non e' un confronto
                const bool saved = noGt_;
                noGt_ = true;
                v->size = binary(1);
                noGt_ = saved;
                args.push_back(v);
            }
            if (isP(Gt)) { ++p_; break; }
            if (isP(Shr)) { toks_[p_].p = Gt; break; }
            expect(Comma);
        }
        return args;
    }
    bool startsTypeArg() const {
        const Token& t = cur();
        if (builtinWord(t) || isWord("const") || isWord("struct") || isWord("unsigned") || isWord("signed")) return true;
        return typeNameAt(p_);
    }
    // type-id: specificatori + dichiaratore astratto (per cast, sizeof, argomenti).
    TypeNode* typeId() {
        std::uint32_t mods = 0;
        TypeNode* base = declSpecifiers(mods);
        Sym name = 0;
        std::vector<Node*> params;
        bool isFn = false;
        return declarator(base, name, params, isFn, true);
    }
    // Dichiaratore: puntatori, nome (o dichiaratore tra parentesi), suffissi.
    // `name` = 0 per un dichiaratore astratto. isFunction se e' una funzione.
    TypeNode* declarator(TypeNode* base, Sym& name, std::vector<Node*>& fnParams, bool& isFunction, bool abstractOk,
                         std::uint32_t* fnMods = nullptr, Node** qualOwner = nullptr) {
        Depth guard(*this);
        // prefissi
        std::vector<std::pair<TF, bool>> prefix;     // (Pointer/Reference, const)
        while (true) {
            if (accept(Star)) { bool c = false; while (isWord("const") || isWord("volatile") || isWord("restrict")) { c = c || isWord("const"); ++p_; } prefix.push_back({TF::Pointer, c}); continue; }
            if (cpp_ && isP(Amp)) { ++p_; prefix.push_back({TF::Reference, false}); continue; }
            if (cpp_ && isP(AndAnd)) error("riferimenti && (semantica di spostamento) non supportati");
            break;
        }
        TypeNode* t = base;
        for (const auto& pr : prefix) {
            TypeNode* n = pool_.type(pr.first, t->pos);
            n->inner = t;
            n->isConst = pr.second;
            t = n;
        }
        // parte diretta
        bool innerDecl = false;
        std::size_t innerStart = 0, innerEnd = 0;
        if (isP(LParen) && (isP(peek(), Star) || isP(peek(), Amp) || (isP(peek(), LParen)))) {
            // ( dichiaratore ): lo si analizza dopo i suffissi
            innerDecl = true;
            ++p_;
            innerStart = p_;
            int depth = 1;
            for (; p_ < toks_.size() && toks_[p_].t != T::End; ++p_) {
                if (isP(LParen)) ++depth;
                if (isP(RParen) && --depth == 0) break;
            }
            innerEnd = p_;
            expect(RParen);
        } else if (cur().t == T::Ident && !isKeyword(cur().sym)) {
            name = toks_[p_++].sym;
            // A::f, A::A, A::~A, ns::f, Pila<T>::f (definizioni fuori dalla classe)
            std::vector<TypeNode*> ownerArgs;
            if (qualOwner && isP(Lt) && knownTemplate(syms_[name])) {
                const std::size_t save = p_;
                ownerArgs = templateArgs();
                if (!isP(Scope)) { p_ = save; ownerArgs.clear(); }
            }
            while (isP(Scope) && (peek().t == T::Ident || isP(peek(), Tilde))) {
                if (qualOwner) {
                    Node* q = pool_.node(K::Name, toks_[p_ - 1].pos);
                    q->sym = name;
                    q->targs = ownerArgs;
                    ownerArgs.clear();
                    if (*qualOwner) { Node* s = pool_.node(K::Scope, q->pos); s->a = *qualOwner; s->sym = name; *qualOwner = s; }
                    else *qualOwner = q;
                }
                ++p_;
                if (accept(Tilde)) { name = syms_.intern("~" + syms_[ident()]); break; }
                if (isWord("operator")) { name = operatorName(); break; }
                name = ident();
            }
        } else if (cpp_ && isWord("operator")) {
            name = operatorName();
        } else if (cpp_ && isP(Tilde) && peek().t == T::Ident) {
            ++p_;
            name = syms_.intern("~" + syms_[ident()]);
        } else if (!abstractOk) {
            error("atteso un nome" + near());
        }
        // suffissi (dall'ultimo al primo)
        struct Suffix { bool fn; Node* size; std::vector<Node*> params; std::uint32_t mods; TypeNode* trailing; };
        std::vector<Suffix> sufs;
        while (true) {
            if (isP(LBracket)) {
                ++p_;
                Suffix s{false, nullptr, {}, 0, nullptr};
                if (!isP(RBracket)) s.size = expression();
                expect(RBracket);
                sufs.push_back(s);
                continue;
            }
            if (isP(LParen) && (name != 0 || innerDecl || abstractOk) && looksLikeParams()) {
                ++p_;
                Suffix s{true, nullptr, parameters(), 0, nullptr};
                // qualificatori del metodo
                while (true) {
                    if (acceptWord("const")) { s.mods |= MConstMethod; continue; }
                    if (cpp_ && acceptWord("override")) { s.mods |= MOverride; continue; }
                    if (cpp_ && acceptWord("final")) { s.mods |= MFinal; continue; }
                    if (cpp_ && acceptWord("noexcept")) { if (isP(LParen)) skipBalanced(); continue; }
                    if (cpp_ && isWord("throw") && isP(peek(), LParen)) { ++p_; skipBalanced(); continue; }
                    if (cpp_ && isP(Arrow)) { ++p_; s.trailing = typeId(); continue; }
                    break;
                }
                sufs.push_back(s);
                continue;
            }
            break;
        }
        for (std::size_t k = sufs.size(); k-- > 0;) {
            Suffix& s = sufs[k];
            if (s.fn) {
                TypeNode* f = pool_.type(TF::Func, t->pos);
                f->inner = s.trailing ? s.trailing : t;
                for (Node* prm : s.params) f->args.push_back(prm->type);
                t = f;
                if (k == 0 && !innerDecl) { isFunction = true; fnParams = s.params; if (fnMods) *fnMods |= s.mods; }
            } else {
                TypeNode* a = pool_.type(TF::Array, t->pos);
                a->inner = t;
                a->size = s.size;
                t = a;
            }
        }
        if (innerDecl) {
            // il dichiaratore tra parentesi si applica al tipo costruito fin qui
            const std::size_t save = p_;
            p_ = innerStart;
            std::vector<Node*> innerParams;
            bool innerFn = false;
            Node* q = nullptr;
            t = declarator(t, name, innerParams, innerFn, abstractOk, nullptr, &q);
            if (p_ != innerEnd) error("dichiaratore non valido");
            p_ = save;
            if (innerFn) { isFunction = true; fnParams = innerParams; }
        }
        return t;
    }
    // "(" inizia una lista di parametri (e non un inizializzatore Punto p(1, 2))?
    bool looksLikeParams() const {
        const Token& n = peek();
        if (isP(n, RParen)) return true;                  // ()
        if (n.t != T::Ident && !isP(n, Scope)) return isP(n, Ellipsis);
        if (builtinWord(n) || specifierWord(n)) return true;
        return typeNameAt(p_ + 1);
    }
    std::vector<Node*> parameters() {
        std::vector<Node*> ps;
        if (accept(RParen)) return ps;
        if (isWord("void") && isP(peek(), RParen)) { p_ += 2; return ps; }
        while (true) {
            if (isP(Ellipsis)) error("funzioni variadiche (...) dell'utente non supportate");
            Node* prm = pool_.node(K::Param, cur().pos);
            std::uint32_t mods = 0;
            TypeNode* base = declSpecifiers(mods);
            Sym name = 0;
            std::vector<Node*> inner;
            bool isFn = false;
            prm->type = declarator(base, name, inner, isFn, true);
            if (isFn) {                                   // parametro funzione = puntatore a funzione
                TypeNode* ptr = pool_.type(TF::Pointer, prm->type->pos);
                ptr->inner = prm->type;
                prm->type = ptr;
            }
            prm->sym = name;
            prm->mods = mods;
            if (accept(Assign)) prm->a = assignment();
            ps.push_back(prm);
            if (accept(RParen)) break;
            expect(Comma);
        }
        return ps;
    }
    void skipBalanced() {
        expect(LParen);
        int depth = 1;
        while (depth > 0 && !atEnd()) { if (isP(LParen)) ++depth; if (isP(RParen)) --depth; ++p_; }
    }
    Sym operatorName() {
        const Pos at = cur().pos;
        ++p_;                                            // operator
        std::string op;
        if (isP(LParen) && isP(peek(), RParen)) { p_ += 2; op = "()"; }
        else if (isP(LBracket) && isP(peek(), RBracket)) { p_ += 2; op = "[]"; }
        else if (isWord("new") || isWord("delete")) fail("operator new/delete dell'utente non supportati", at);
        else if (cur().t == T::Punct) {
            op = punctText(cur().p);
            ++p_;
            if (op == "->" || op == "," || op == "->*") fail("operator" + op + " non supportato", at);
        } else fail("conversioni dell'utente (operator T()) non supportate", at);
        return syms_.intern("operator" + op);
    }

    // ------------------------------------------------------------ classi ed enum
    TypeNode* classSpecifier(Node** defined) {
        Depth guard(*this);
        const Pos at = cur().pos;
        const std::string kw = syms_[cur().sym];
        ++p_;
        int kind = kw == "class" ? 0 : kw == "struct" ? 1 : kw == "union" ? 2 : 4;
        if (kind == 4 && (isWord("class") || isWord("struct"))) { ++p_; kind = 5; }
        if (kind == 2) fail("union non supportata", at);
        Sym name = 0;
        if (cur().t == T::Ident && !isKeyword(cur().sym)) {
            name = toks_[p_++].sym;
            while (isP(Scope) && peek().t == T::Ident) { ++p_; name = toks_[p_++].sym; }   // struct ns::X (riferimento)
        }
        TypeNode* enumBase = nullptr;
        if (kind >= 4 && accept(Colon)) enumBase = typeId();
        if (cpp_ && kind < 4 && isWord("final")) ++p_;
        const bool body = isP(LBrace) || (kind < 4 && isP(Colon));
        TypeNode* t = pool_.type(TF::Named, at);
        if (!body) {
            if (!name) error("struct senza nome e senza corpo");
            // riferimento: struct P (in C il nome dei tag)
            t->name = name;
            if (!cpp_) t->qual.push_back(syms_.intern(kind >= 4 ? "enum" : "struct"));
            if (cpp_ && !knownType(syms_[name])) declareType(name);   // dichiarazione anticipata
            return t;
        }
        if (!name) name = syms_.intern("<anonima" + std::to_string(++anon_) + ">");
        Node* cls = pool_.node(K::Class, at);
        cls->sym = name;
        cls->op = static_cast<std::uint8_t>(kind);
        cls->type = enumBase;
        if (cpp_) declareType(name);
        if (!cpp_) tags_.insert(syms_[name]);
        if (kind >= 4) {
            enumBody(cls);
        } else {
            if (accept(Colon)) {
                do {
                    Node* b = pool_.node(K::BaseSpec, cur().pos);
                    while (true) {
                        if (acceptWord("public")) { b->mods |= MPublic; continue; }
                        if (acceptWord("protected")) { b->mods |= MProtected; continue; }
                        if (acceptWord("private")) { b->mods |= MPrivate; continue; }
                        if (acceptWord("virtual")) fail("ereditarieta' virtuale non supportata", b->pos);
                        break;
                    }
                    b->type = qualifiedType();
                    cls->list2.push_back(b);
                } while (accept(Comma));
                if (cls->list2.size() > 1) fail("ereditarieta' multipla non supportata", cls->list2[1]->pos);
            }
            std::string full = classes_.empty() ? nsPrefix() : classes_.back() + "::";
            full += syms_[name];
            classes_.push_back(full);
            expect(LBrace);
            std::uint32_t access = kind == 0 ? MPrivate : MPublic;
            while (!isP(RBrace)) {
                if (atEnd()) fail("manca '}' alla fine della classe " + syms_[name], at);
                member(cls, access);
            }
            ++p_;
            classes_.pop_back();
        }
        if (defined) *defined = cls;
        else pendingClasses_.push_back(cls);
        t->name = name;
        if (!cpp_) t->qual.push_back(syms_.intern(kind >= 4 ? "enum" : "struct"));
        t->builtin = false;
        return t;
    }
    void enumBody(Node* cls) {
        expect(LBrace);
        while (!isP(RBrace)) {
            Node* e = pool_.node(K::Enumerator, cur().pos);
            e->sym = ident("il nome di un valore dell'enum");
            if (accept(Assign)) e->a = conditional();
            cls->list.push_back(e);
            if (!accept(Comma)) break;
        }
        expect(RBrace);
        if (cls->op == 4 && cpp_) {
            // i valori di un enum semplice sono nomi del contesto (non tipi)
        }
    }
    void member(Node* cls, std::uint32_t& access) {
        Depth guard(*this);
        if (accept(Semi)) return;
        if ((isWord("public") || isWord("private") || isWord("protected")) && isP(peek(), Colon)) {
            access = isWord("public") ? MPublic : isWord("private") ? MPrivate : MProtected;
            p_ += 2;
            return;
        }
        if (isWord("template")) { Node* t = templateDecl(true); t->mods |= access; cls->list.push_back(t); return; }
        if (isWord("using")) { Node* u = usingDecl(); if (u) { u->mods |= access; cls->list.push_back(u); } return; }
        if (isWord("typedef")) { typedefDecl(cls->list); return; }
        if (isWord("static_assert")) { ++p_; skipBalanced(); accept(Semi); return; }
        if (isWord("friend")) {
            const Pos at = cur().pos;
            ++p_;
            if ((isWord("class") || isWord("struct")) && peek().t == T::Ident && isP(peek(2), Semi)) {
                Node* f = pool_.node(K::Friend, at);
                p_ += 1;
                f->sym = ident();
                expect(Semi);
                cls->list.push_back(f);
                return;
            }
            // funzione amica (dichiarazione o definizione): e' una funzione globale
            std::vector<Node*> out;
            declaration(out, MFriend, true);
            for (Node* n : out) { Node* f = pool_.node(K::Friend, at); f->a = n; cls->list.push_back(f); }
            return;
        }
        const Sym clsName = cls->sym;
        // costruttore e distruttore
        std::size_t q = p_;
        std::uint32_t mods = access;
        while (isWord(toks_[q], "explicit") || isWord(toks_[q], "inline") || isWord(toks_[q], "constexpr") || isWord(toks_[q], "virtual")) {
            if (isWord(toks_[q], "virtual")) mods |= MVirtual;
            ++q;
        }
        if ((toks_[q].t == T::Ident && toks_[q].sym == clsName && isP(toks_[q + 1], LParen)) ||
            (isP(toks_[q], Tilde) && toks_[q + 1].t == T::Ident && toks_[q + 1].sym == clsName)) {
            p_ = q;
            const bool dtor = accept(Tilde);
            const Pos at = cur().pos;
            ++p_;
            Node* f = pool_.node(K::Func, at);
            f->sym = dtor ? syms_.intern("~" + syms_[clsName]) : clsName;
            f->mods = mods | (dtor ? 0 : 0);
            f->op = dtor ? 2 : 1;                          // 1 costruttore, 2 distruttore
            expect(LParen);
            f->list = parameters();
            while (acceptWord("noexcept") || acceptWord("override")) {}
            if (accept(Assign)) {
                if (acceptWord("default")) f->mods |= MDefault;
                else if (acceptWord("delete")) f->mods |= MDeleted;
                else error("atteso default o delete");
                expect(Semi);
                cls->list.push_back(f);
                return;
            }
            if (accept(Colon)) memberInits(f);
            if (isP(LBrace)) f->b = block();
            else expect(Semi);
            cls->list.push_back(f);
            return;
        }
        std::vector<Node*> out;
        declaration(out, access, true, cls);
        for (Node* n : out) cls->list.push_back(n);
    }
    void memberInits(Node* f) {
        do {
            Node* mi = pool_.node(K::MemberInit, cur().pos);
            if (typeNameAt(p_) && !(cur().t == T::Ident && isP(peek(), LParen) && !knownType(syms_[cur().sym]))) mi->type = qualifiedType();
            else mi->sym = ident("il nome di un membro");
            if (isP(LBrace)) { mi->a = initList(); }
            else {
                expect(LParen);
                if (!isP(RParen)) { do mi->list.push_back(assignment()); while (accept(Comma)); }
                expect(RParen);
            }
            f->list2.push_back(mi);
        } while (accept(Comma));
    }

    // ------------------------------------------------------------ livello superiore
    void topLevel(std::vector<Node*>& out) {
        Depth guard(*this);
        if (accept(Semi)) return;
        if (cpp_ && isWord("namespace")) { out.push_back(namespaceDecl()); return; }
        if (cpp_ && isWord("using")) { Node* u = usingDecl(); if (u) out.push_back(u); return; }
        if (cpp_ && isWord("template")) { out.push_back(templateDecl(false)); return; }
        if (isWord("typedef")) { typedefDecl(out); return; }
        if (isWord("static_assert") || isWord("_Static_assert")) { ++p_; skipBalanced(); accept(Semi); return; }
        if (isWord("extern") && peek().t == T::Str && isP(peek(2), LBrace)) {
            p_ += 3;                                     // extern "C" { ... }
            while (!isP(RBrace)) { if (atEnd()) error("manca '}' di extern \"C\""); topLevel(out); }
            ++p_;
            return;
        }
        if (isWord("asm") || isWord("__asm__")) error("asm non supportato");
        declaration(out, 0, false);
    }
    Node* namespaceDecl() {
        const Pos at = cur().pos;
        ++p_;
        Node* ns = pool_.node(K::Namespace, at);
        std::string full;
        std::vector<std::string> parts;
        if (cur().t == T::Ident) {
            parts.push_back(syms_[ident()]);
            while (accept(Scope)) parts.push_back(syms_[ident()]);
        } else parts.push_back("<anonimo>");
        for (const auto& s : parts) full += (full.empty() ? "" : "::") + s;
        ns->sym = syms_.intern(full);
        if (accept(Assign)) error("alias di namespace non supportati");
        expect(LBrace);
        for (const auto& s : parts) {
            ns_.push_back(s);
            std::string acc;
            for (const auto& n : ns_) acc += (acc.empty() ? "" : "::") + n;
            namespaces_.insert(acc);
        }
        while (!isP(RBrace)) { if (atEnd()) fail("manca '}' del namespace", at); topLevel(ns->list); }
        ++p_;
        for (std::size_t k = 0; k < parts.size(); ++k) ns_.pop_back();
        return ns;
    }
    Node* usingDecl() {
        const Pos at = cur().pos;
        ++p_;
        Node* u = pool_.node(K::Using, at);
        if (acceptWord("namespace")) {
            u->op = 1;
            std::string path;
            if (accept(Scope)) {}
            path = syms_[ident()];
            while (accept(Scope)) path += "::" + syms_[ident()];
            u->sym = syms_.intern(path);
            expect(Semi);
            // using namespace X: i tipi di X diventano visibili
            std::string full = path;
            usingNs_.push_back(full);
            for (const auto& n : ns_) { (void)n; }
            return u;
        }
        // using T = tipo;  /  using X::nome;
        if (cur().t == T::Ident && isP(peek(), Assign)) {
            Node* td = pool_.node(K::Typedef, at);
            td->sym = ident();
            ++p_;
            td->type = typeId();
            expect(Semi);
            declareType(td->sym);
            return td;
        }
        u->op = 2;
        u->type = qualifiedTypeOrName();
        expect(Semi);
        // using std::string: il nome diventa visibile
        if (u->type) {
            std::string full;
            for (Sym s : u->type->qual) full += syms_[s] + "::";
            full += syms_[u->type->name];
            if (known_.count(full)) declareType(u->type->name);
            if (templates_.count(full)) declareType(u->type->name, true);
        }
        return u;
    }
    TypeNode* qualifiedTypeOrName() {
        TypeNode* t = pool_.type(TF::Named, cur().pos);
        accept(Scope);
        Sym s = ident();
        while (accept(Scope)) { t->qual.push_back(s); s = ident(); }
        t->name = s;
        return t;
    }
    Node* templateDecl(bool inClass) {
        const Pos at = cur().pos;
        ++p_;
        Node* t = pool_.node(K::Template, at);
        expect(Lt);
        std::vector<Sym> names;
        while (!isP(Gt)) {
            Node* tp = pool_.node(K::TParam, cur().pos);
            if (acceptWord("typename") || acceptWord("class")) {
                if (isP(Ellipsis)) fail("template variadici non supportati", cur().pos);
                tp->sym = ident("il nome del parametro del template");
                if (accept(Assign)) tp->type = typeId();
            } else {
                tp->op = 1;
                std::uint32_t mods = 0;
                tp->type = declSpecifiers(mods);
                tp->sym = ident("il nome del parametro del template");
                if (accept(Assign)) tp->a = conditional();
            }
            names.push_back(tp->sym);
            t->list.push_back(tp);
            if (!accept(Comma)) break;
        }
        expect(Gt);
        if (t->list.empty()) fail("specializzazione dei template (template<>) non supportata", at);
        tparams_.push_back(names);
        if (isWord("struct") || isWord("class")) {
            // template di classe: il nome diventa un template
            const std::size_t save = p_;
            ++p_;
            if (cur().t == T::Ident) {
                const Sym n = cur().sym;
                if (isP(peek(), Lt)) fail("specializzazione parziale dei template non supportata", cur().pos);
                declareType(n, true);
            }
            p_ = save;
        }
        std::vector<Node*> out;
        if (inClass) {
            // un template membro: come una dichiarazione della classe
            declaration(out, MPublic, true, nullptr);
        } else {
            declaration(out, 0, false);
        }
        tparams_.pop_back();
        if (out.size() != 1) fail("un template dichiara una sola funzione o classe", at);
        t->a = out[0];
        if (t->a->k == K::Func) templates_.insert((classes_.empty() ? nsPrefix() : classes_.back() + "::") + syms_[t->a->sym]);
        if (t->a->k == K::Class) templates_.insert((classes_.empty() ? nsPrefix() : classes_.back() + "::") + syms_[t->a->sym]);
        return t;
    }
    void typedefDecl(std::vector<Node*>& out) {
        const Pos at = cur().pos;
        ++p_;
        std::uint32_t mods = 0;
        Node* defined = nullptr;
        TypeNode* base = declSpecifiers(mods, &defined);
        if (defined) out.push_back(defined);
        do {
            Sym name = 0;
            std::vector<Node*> params;
            bool isFn = false;
            Node* td = pool_.node(K::Typedef, at);
            td->type = declarator(base, name, params, isFn, false);
            td->sym = name;
            declareType(name);
            out.push_back(td);
        } while (accept(Comma));
        expect(Semi);
    }
    // Dichiarazione (globale, membro o locale): variabili, funzioni, classi.
    void declaration(std::vector<Node*>& out, std::uint32_t extraMods, bool inClass, Node* cls = nullptr) {
        Depth guard(*this);
        const Pos at = cur().pos;
        std::uint32_t mods = extraMods;
        Node* defined = nullptr;
        TypeNode* base = declSpecifiers(mods, &defined);
        if (defined) {
            out.push_back(defined);
            if (accept(Semi)) return;                    // struct P { ... };
        } else if (isP(Semi) && base->f == TF::Named && !base->builtin) {
            ++p_;                                        // struct P; (dichiarazione anticipata)
            return;
        }
        Node* vars = nullptr;
        bool first = true;
        while (true) {
            Sym name = 0;
            std::vector<Node*> params;
            bool isFn = false;
            std::uint32_t fnMods = 0;
            Node* owner = nullptr;
            const Pos dpos = cur().pos;
            TypeNode* type = declarator(base, name, params, isFn, false, &fnMods, &owner);
            if (isFn) {
                Node* f = pool_.node(K::Func, dpos);
                f->sym = name;
                f->type = type->inner;                   // tipo di ritorno
                f->list = params;
                f->mods = mods | fnMods;
                if (owner) {
                    // T A::f(...) / A::A(...): la classe e' nel percorso
                    TypeNode* ot = pool_.type(TF::Named, owner->pos);
                    std::vector<Sym> parts;
                    for (Node* q = owner; q; q = q->k == K::Scope ? q->a : nullptr) { parts.push_back(q->sym); if (q->k != K::Scope) break; }
                    std::reverse(parts.begin(), parts.end());
                    ot->name = parts.back();
                    parts.pop_back();
                    ot->qual = parts;
                    if (owner->k == K::Name) ot->args = owner->targs;
                    else if (owner->k == K::Scope && owner->a == nullptr) {}
                    f->type2 = ot;
                    const std::string& n = syms_[name];
                    if (n == syms_[ot->name]) f->op = 1;          // A::A
                    else if (n.size() > 1 && n[0] == '~') f->op = 2;  // A::~A
                    // nel corpo della definizione i nomi della classe sono visibili
                }
                if (accept(Assign)) {
                    if (cur().t == T::Int && cur().i == 0) { ++p_; f->mods |= MPure; }
                    else if (acceptWord("default")) f->mods |= MDefault;
                    else if (acceptWord("delete")) f->mods |= MDeleted;
                    else error("atteso = 0, = default o = delete");
                    expect(Semi);
                    out.push_back(f);
                    return;
                }
                if (f->op == 1 && accept(Colon)) memberInits(f);
                if (isP(LBrace)) {
                    if (!first) error("definizione di funzione dopo altre dichiarazioni");
                    if (owner && f->type2) {
                        std::string full;
                        for (Sym s : f->type2->qual) full += syms_[s] + "::";
                        full += syms_[f->type2->name];
                        classes_.push_back(full);
                        f->b = block();
                        classes_.pop_back();
                    } else {
                        f->b = block();
                    }
                    out.push_back(f);
                    return;
                }
                out.push_back(f);
                if (accept(Comma)) { first = false; continue; }
                expect(Semi);
                return;
            }
            if (!vars) {
                vars = pool_.node(inClass ? K::Field : K::VarDecl, at);
                vars->mods = mods;
                out.push_back(vars);
            }
            Node* d = pool_.node(K::Declarator, dpos);
            d->sym = name;
            d->type = type;
            d->mods = mods;
            if (accept(Assign)) {
                d->op = 1;
                d->a = isP(LBrace) ? initList() : assignment();
            } else if (isP(LBrace) && cpp_) {
                d->op = 3;
                d->a = initList();
            } else if (isP(LParen) && cpp_ && !inClass) {
                d->op = 2;                                // Punto p(1, 2)
                ++p_;
                if (!isP(RParen)) { do d->list.push_back(assignment()); while (accept(Comma)); }
                expect(RParen);
            }
            vars->list.push_back(d);
            first = false;
            if (accept(Comma)) continue;
            expect(Semi);
            return;
        }
        (void)cls;
    }
    // ------------------------------------------------------------ istruzioni
    Node* block() {
        Depth guard(*this);
        Node* b = pool_.node(K::Block, cur().pos);
        expect(LBrace);
        while (!isP(RBrace)) {
            if (atEnd()) fail("manca '}' alla fine del blocco", b->pos);
            b->list.push_back(statement());
        }
        ++p_;
        return b;
    }
    Node* statement() {
        Depth guard(*this);
        const Pos at = cur().pos;
        if (isP(LBrace)) return block();
        if (accept(Semi)) return pool_.node(K::Empty, at);
        if (cur().t == T::Ident) {
            const std::string w = syms_[cur().sym];
            if (w == "if") return ifStmt();
            if (w == "while") { ++p_; Node* n = pool_.node(K::While, at); expect(LParen); n->a = condition(); expect(RParen); n->b = statement(); return n; }
            if (w == "do") { ++p_; Node* n = pool_.node(K::DoWhile, at); n->b = statement(); if (!acceptWord("while")) error("atteso while dopo il corpo del do"); expect(LParen); n->a = expression(); expect(RParen); expect(Semi); return n; }
            if (w == "for") return forStmt();
            if (w == "switch") { ++p_; Node* n = pool_.node(K::Switch, at); expect(LParen); n->a = expression(); expect(RParen); n->b = statement(); return n; }
            if (w == "case") { ++p_; Node* n = pool_.node(K::Case, at); n->a = conditional(); expect(Colon); return n; }
            if (w == "default" && isP(peek(), Colon)) { p_ += 2; return pool_.node(K::Case, at); }
            if (w == "break") { ++p_; expect(Semi); return pool_.node(K::Break, at); }
            if (w == "continue") { ++p_; expect(Semi); return pool_.node(K::Continue, at); }
            if (w == "return") { ++p_; Node* n = pool_.node(K::Return, at); if (!isP(Semi)) n->a = isP(LBrace) ? initList() : expression(); expect(Semi); return n; }
            if (w == "goto") { ++p_; Node* n = pool_.node(K::Goto, at); n->sym = ident("un'etichetta"); expect(Semi); return n; }
            if (cpp_ && w == "try") return tryStmt();
            if (cpp_ && w == "throw") { ++p_; Node* n = pool_.node(K::Throw, at); if (!isP(Semi)) n->a = expression(); expect(Semi); return n; }
            if (w == "typedef") { Node* b = pool_.node(K::Block, at); b->op = 1; typedefDecl(b->list); return b; }
            if (cpp_ && w == "using") { Node* u = usingDecl(); return u ? u : pool_.node(K::Empty, at); }
            if (isP(peek(), Colon) && !isKeyword(cur().sym) && !(cpp_ && isP(peek(), Scope))) {
                Node* n = pool_.node(K::Label, at);
                n->sym = toks_[p_].sym;
                p_ += 2;
                n->a = isP(RBrace) ? pool_.node(K::Empty, at) : statement();
                return n;
            }
            if (w == "static_assert") { ++p_; skipBalanced(); expect(Semi); return pool_.node(K::Empty, at); }
            if (w == "struct" || w == "union" || w == "enum" || (cpp_ && w == "class")) {
                // definizione locale di una struct (o dichiarazione di variabile)
                Node* b = pool_.node(K::Block, at);
                b->op = 1;                               // blocco trasparente (nessuno scope)
                declaration(b->list, 0, false);
                return b->list.size() == 1 ? b->list[0] : b;
            }
        }
        if (cpp_ && isWord("auto") && isP(peek(), LBracket)) return bindingDecl(at, false);
        if (startsDecl()) {
            std::vector<Node*> out;
            declaration(out, 0, false);
            if (out.size() == 1) return out[0];
            Node* b = pool_.node(K::Block, at);
            b->op = 1;
            b->list = out;
            return b;
        }
        Node* e = pool_.node(K::ExprStmt, at);
        e->a = expression();
        expect(Semi);
        return e;
    }
    // auto [a, b] = espressione;
    Node* bindingDecl(Pos at, bool inFor) {
        ++p_;                                            // auto
        Node* n = pool_.node(K::Binding, at);
        if (accept(Amp)) n->op = 1;
        expect(LBracket);
        do { Node* nm = pool_.node(K::Name, cur().pos); nm->sym = ident(); n->list.push_back(nm); } while (accept(Comma));
        expect(RBracket);
        if (inFor) return n;
        expect(Assign);
        n->a = expression();
        expect(Semi);
        return n;
    }
    Node* condition() {
        // C++17: if (int x = f(); x > 0) e if (auto x = ...) non sono supportati in forma completa
        return expression();
    }
    Node* ifStmt() {
        const Pos at = cur().pos;
        ++p_;
        Node* n = pool_.node(K::If, at);
        if (cpp_ && isWord("constexpr")) ++p_;
        expect(LParen);
        if (startsDecl()) {
            std::vector<Node*> out;
            declaration(out, 0, false);
            if (out.size() != 1) error("dichiarazione non valida in if");
            n->d = out[0];
        }
        n->a = expression();
        expect(RParen);
        n->b = statement();
        if (acceptWord("else")) n->c = statement();
        return n;
    }
    Node* forStmt() {
        const Pos at = cur().pos;
        ++p_;
        expect(LParen);
        // range-for: for (decl : espressione)
        if (cpp_) {
            std::size_t q = p_;
            int depth = 0, questions = 0;
            bool range = false;
            for (; q < toks_.size() && toks_[q].t != T::End; ++q) {
                if (isP(toks_[q], LParen) || isP(toks_[q], LBracket) || isP(toks_[q], LBrace)) ++depth;
                else if (isP(toks_[q], RParen) || isP(toks_[q], RBracket) || isP(toks_[q], RBrace)) { if (depth == 0) break; --depth; }
                else if (isP(toks_[q], Semi) && depth == 0) break;
                else if (isP(toks_[q], Question) && depth == 0) ++questions;
                else if (isP(toks_[q], Colon) && depth == 0) { if (questions > 0) { --questions; continue; } range = true; break; }
            }
            if (range) {
                Node* n = pool_.node(K::ForEach, at);
                if (isWord("auto") && isP(peek(), LBracket)) {
                    n->a = bindingDecl(cur().pos, true);
                } else if ((isWord("auto") || isWord("const")) && (isP(peek(), Amp) || isP(peek(2), Amp)) && (isP(peek(), LBracket) || isP(peek(2), LBracket) || isP(peek(3), LBracket))) {
                    if (isWord("const")) ++p_;
                    n->a = bindingDecl(cur().pos, true);
                } else {
                    std::uint32_t mods = 0;
                    TypeNode* base = declSpecifiers(mods);
                    Sym name = 0;
                    std::vector<Node*> params;
                    bool isFn = false;
                    Node* d = pool_.node(K::Declarator, cur().pos);
                    d->type = declarator(base, name, params, isFn, false);
                    d->sym = name;
                    n->a = d;
                }
                expect(Colon);
                n->b = isP(LBrace) ? initList() : expression();
                expect(RParen);
                n->c = statement();
                return n;
            }
        }
        Node* n = pool_.node(K::For, at);
        if (!accept(Semi)) {
            if (startsDecl()) {
                std::vector<Node*> out;
                declaration(out, 0, false);
                n->a = out.size() == 1 ? out[0] : nullptr;
                if (!n->a) { Node* b = pool_.node(K::Block, at); b->op = 1; b->list = out; n->a = b; }
            } else {
                Node* e = pool_.node(K::ExprStmt, cur().pos);
                e->a = expression();
                n->a = e;
                expect(Semi);
            }
        }
        if (!isP(Semi)) n->b = expression();
        expect(Semi);
        if (!isP(RParen)) n->c = expression();
        expect(RParen);
        n->d = statement();
        return n;
    }
    Node* tryStmt() {
        const Pos at = cur().pos;
        ++p_;
        Node* n = pool_.node(K::Try, at);
        n->a = block();
        while (isWord("catch")) {
            Node* c = pool_.node(K::Catch, cur().pos);
            ++p_;
            expect(LParen);
            if (accept(Ellipsis)) { /* catch(...) */ }
            else {
                std::uint32_t mods = 0;
                TypeNode* base = declSpecifiers(mods);
                Sym name = 0;
                std::vector<Node*> params;
                bool isFn = false;
                c->type = declarator(base, name, params, isFn, true);
                c->sym = name;
            }
            expect(RParen);
            c->a = block();
            n->list.push_back(c);
        }
        if (n->list.empty()) error("try senza catch");
        return n;
    }

    // ------------------------------------------------------------ espressioni
    Node* expression() {
        Depth guard(*this);
        Node* e = assignment();
        while (isP(Comma)) {
            const Pos at = cur().pos;
            ++p_;
            Node* c = pool_.node(K::Comma, at);
            c->a = e;
            c->b = assignment();
            e = c;
        }
        return e;
    }
    static bool assignOp(P p) {
        return p == Assign || p == PlusEq || p == MinusEq || p == StarEq || p == SlashEq || p == PercentEq || p == AmpEq ||
               p == PipeEq || p == CaretEq || p == ShlEq || p == ShrEq;
    }
    Node* assignment() {
        Depth guard(*this);
        if (cpp_ && isWord("throw")) {
            Node* t = pool_.node(K::ThrowExpr, cur().pos);
            ++p_;
            if (!isP(Semi) && !isP(RParen) && !isP(Colon) && !isP(Comma)) t->a = assignment();
            return t;
        }
        Node* l = conditional();
        if (cur().t == T::Punct && assignOp(cur().p)) {
            Node* a = pool_.node(K::Assign, cur().pos);
            a->op = cur().p;
            ++p_;
            a->a = l;
            a->b = isP(LBrace) ? initList() : assignment();
            return a;
        }
        return l;
    }
    Node* conditional() {
        Depth guard(*this);
        Node* c = binary(1);
        if (isP(Question)) {
            Node* n = pool_.node(K::Cond, cur().pos);
            ++p_;
            n->a = c;
            n->b = expression();
            expect(Colon);
            n->c = assignment();
            return n;
        }
        return c;
    }
    static int precOf(P p) {
        switch (p) {
        case OrOr: return 1; case AndAnd: return 2; case Pipe: return 3; case Caret: return 4; case Amp: return 5;
        case EqEq: case Ne: return 6; case Lt: case Gt: case Le: case Ge: return 7; case Shl: case Shr: return 8;
        case Plus: case Minus: return 9; case Star: case Slash: case Percent: return 10;
        default: return -1;
        }
    }
    Node* binary(int minPrec) {
        Node* l = unary();
        while (cur().t == T::Punct) {
            const P p = cur().p;
            const int pr = precOf(p);
            if (pr < 0 || pr < minPrec) break;
            if (noGt_ && (p == Gt || p == Shr)) break;
            Node* n = pool_.node(K::Binary, cur().pos);
            ++p_;
            n->op = p;
            n->a = l;
            Depth guard(*this);
            n->b = binary(pr + 1);
            l = n;
        }
        return l;
    }
    Node* unary() {
        Depth guard(*this);
        const Pos at = cur().pos;
        if (cur().t == T::Punct) {
            switch (cur().p) {
            case PlusPlus: { ++p_; Node* n = pool_.node(K::PreInc, at); n->a = unary(); return n; }
            case MinusMinus: { ++p_; Node* n = pool_.node(K::PreDec, at); n->a = unary(); return n; }
            case Plus: case Minus: case Bang: case Tilde: { Node* n = pool_.node(K::Unary, at); n->op = cur().p; ++p_; n->a = unary(); return n; }
            case Amp: { ++p_; Node* n = pool_.node(K::AddrOf, at); n->a = unary(); return n; }
            case Star: { ++p_; Node* n = pool_.node(K::Deref, at); n->a = unary(); return n; }
            case AndAnd: error("&& come indirizzo di un'etichetta non supportato");
            case LParen:
                if (castAhead()) {
                    ++p_;
                    Node* n = pool_.node(K::Cast, at);
                    n->type = typeId();
                    expect(RParen);
                    if (isP(LBrace)) { n->a = initList(); n->op = 5; return n; }   // (struct P){...} letterale composto
                    n->a = unary();
                    return n;
                }
                break;
            default: break;
            }
        }
        if (isWord("sizeof")) {
            ++p_;
            if (isP(LParen) && castAhead()) {
                ++p_;
                Node* n = pool_.node(K::SizeofType, at);
                n->type = typeId();
                expect(RParen);
                return n;
            }
            Node* n = pool_.node(K::SizeofExpr, at);
            n->a = unary();
            return n;
        }
        if (cpp_ && (isWord("new") || (isP(Scope) && isWord(peek(), "new")))) return newExpr();
        if (cpp_ && (isWord("delete") || (isP(Scope) && isWord(peek(), "delete")))) {
            accept(Scope);
            ++p_;
            Node* n = pool_.node(K::Delete, at);
            if (isP(LBracket) && isP(peek(), RBracket)) { p_ += 2; n->op = 1; }
            n->a = unary();
            return n;
        }
        return postfix(primary());
    }
    // "(" inizia un cast (o sizeof(tipo))?
    bool castAhead() const {
        const Token& n = peek();
        if (n.t != T::Ident && !isP(n, Scope)) return false;
        if (builtinWord(n) || isWord(n, "const") || isWord(n, "struct") || isWord(n, "enum") || isWord(n, "unsigned") || isWord(n, "signed") || isWord(n, "volatile")) return true;
        if (!typeNameAt(p_ + 1)) return false;
        // ( T ) seguito da qualcosa che non e' un operatore binario: cast; (x) da solo: espressione
        std::size_t q = p_ + 1;
        int depth = 0;
        for (; q < toks_.size() && toks_[q].t != T::End; ++q) {
            if (isP(toks_[q], LParen)) ++depth;
            else if (isP(toks_[q], RParen)) { if (depth == 0) break; --depth; }
        }
        return true;
    }
    Node* newExpr() {
        const Pos at = cur().pos;
        accept(Scope);
        ++p_;
        std::uint32_t mods = 0;
        TypeNode* base = declSpecifiers(mods);
        while (accept(Star)) { TypeNode* pt = pool_.type(TF::Pointer, base->pos); pt->inner = base; base = pt; }
        if (isP(LBracket)) {
            Node* n = pool_.node(K::NewArray, at);
            n->type = base;
            ++p_;
            n->list.push_back(expression());
            expect(RBracket);
            while (isP(LBracket)) { ++p_; n->list.push_back(conditional()); expect(RBracket); }
            if (isP(LBrace)) n->a = initList();
            return n;
        }
        Node* n = pool_.node(K::New, at);
        n->type = base;
        if (isP(LParen)) {
            ++p_;
            if (!isP(RParen)) { do n->list.push_back(assignment()); while (accept(Comma)); }
            expect(RParen);
        } else if (isP(LBrace)) {
            n->a = initList();
            n->op = 1;
        }
        return n;
    }
    Node* initList() {
        Depth guard(*this);
        Node* n = pool_.node(K::InitList, cur().pos);
        expect(LBrace);
        while (!isP(RBrace)) {
            if (isP(Dot) && peek().t == T::Ident) {
                // .campo = valore (C99)
                Node* d = pool_.node(K::Label, cur().pos);
                ++p_;
                d->sym = ident();
                expect(Assign);
                d->a = isP(LBrace) ? initList() : assignment();
                n->list.push_back(d);
            } else if (isP(LBracket) && !cpp_) {
                // [i] = valore (C99)
                Node* d = pool_.node(K::Index, cur().pos);
                ++p_;
                d->a = nullptr;
                d->list.push_back(conditional());
                expect(RBracket);
                expect(Assign);
                d->b = isP(LBrace) ? initList() : assignment();
                n->list.push_back(d);
            } else {
                n->list.push_back(isP(LBrace) ? initList() : assignment());
            }
            if (!accept(Comma)) break;
        }
        expect(RBrace);
        return n;
    }
    Node* primary() {
        Depth guard(*this);
        const Token& t = cur();
        const Pos at = t.pos;
        switch (t.t) {
        case T::Int: { Node* n = pool_.node(K::IntLit, at); n->ival = t.i; n->op = t.flags; ++p_; return n; }
        case T::Float: { Node* n = pool_.node(K::FloatLit, at); n->dval = t.d; n->op = t.flags; ++p_; return n; }
        case T::Char: { Node* n = pool_.node(K::CharLit, at); n->ival = t.i; ++p_; return n; }
        case T::Str: {
            // letterali adiacenti: "a" "b" -> "ab"
            std::string s = lx_.strings[t.str];
            ++p_;
            while (cur().t == T::Str) { s += lx_.strings[cur().str]; ++p_; }
            Node* n = pool_.node(K::StrLit, at);
            unit_.strings.push_back(std::move(s));
            n->str = static_cast<std::uint32_t>(unit_.strings.size() - 1);
            return n;
        }
        case T::Punct:
            if (t.p == LParen) {
                ++p_;
                Node* e = expression();
                expect(RParen);
                return e;
            }
            if (cpp_ && t.p == LBracket) return lambda();
            if (cpp_ && t.p == Scope) {
                ++p_;
                Node* n = pool_.node(K::Scope, at);
                n->a = nullptr;
                n->sym = ident();
                return n;
            }
            if (t.p == LBrace && cpp_) return initList();
            break;
        case T::Ident: {
            const std::string& w = syms_[t.sym];
            if (w == "true" || w == "false") { Node* n = pool_.node(K::BoolLit, at); n->ival = w == "true"; ++p_; return n; }
            if ((cpp_ && w == "nullptr") || w == "NULL") { Node* n = pool_.node(K::NullLit, at); ++p_; return n; }
            if (cpp_ && w == "this") { ++p_; return pool_.node(K::This, at); }
            if (cpp_ && (w == "static_cast" || w == "dynamic_cast" || w == "const_cast" || w == "reinterpret_cast")) {
                if (w == "reinterpret_cast") fail("reinterpret_cast non supportato", at);
                ++p_;
                Node* n = pool_.node(K::Cast, at);
                n->op = w == "static_cast" ? 1 : w == "dynamic_cast" ? 2 : 3;
                expect(Lt);
                n->type = typeId();
                expect(Gt);
                expect(LParen);
                n->a = expression();
                expect(RParen);
                return n;
            }
            if (cpp_ && (w == "typeid" || w == "decltype" || w == "co_await" || w == "co_yield" || w == "requires")) fail(w + " non supportato", at);
            // T(...) / T{...}: conversione o costruzione di un tipo
            if ((builtinWord(t) && !isWord("auto")) || (typeNameAt(p_) && functionalCastAhead())) {
                Node* n = pool_.node(K::TypeExpr, at);
                std::uint32_t mods = 0;
                n->type = declSpecifiers(mods);
                if (isP(LParen)) {
                    Node* c = pool_.node(K::Call, cur().pos);
                    c->a = n;
                    ++p_;
                    if (!isP(RParen)) { do c->list.push_back(assignment()); while (accept(Comma)); }
                    expect(RParen);
                    return c;
                }
                if (isP(LBrace)) {
                    Node* c = pool_.node(K::Call, cur().pos);
                    c->a = n;
                    c->op = 1;
                    Node* il = initList();
                    c->list = il->list;
                    return c;
                }
                if (isP(Scope)) return n;                 // T::membro statico
                return n;
            }
            if (isKeyword(t.sym) && w != "operator") fail("'" + w + "' non atteso qui", at);
            Node* n = pool_.node(K::Name, at);
            n->sym = t.sym;
            ++p_;
            if (w == "operator") { --p_; n->sym = operatorName(); }
            // nome<argomenti> di un template di funzione
            if (isP(Lt) && knownTemplate(syms_[n->sym]) && templateArgsAhead()) n->targs = templateArgs();
            return n;
        }
        default: break;
        }
        error("attesa un'espressione" + near());
    }
    bool functionalCastAhead() const {
        std::string path;
        std::size_t len = 0;
        if (!typeNameAt(p_, &path, &len)) return false;
        std::size_t q = p_ + len;
        if (isP(toks_[q], Lt)) {
            int depth = 0;
            for (; q < toks_.size(); ++q) {
                if (isP(toks_[q], Lt)) ++depth;
                else if (isP(toks_[q], Gt)) { if (--depth == 0) { ++q; break; } }
                else if (isP(toks_[q], Shr)) { depth -= 2; if (depth <= 0) { ++q; break; } }
                else if (toks_[q].t == T::End) return false;
            }
        }
        return isP(toks_[q], LParen) || isP(toks_[q], LBrace) || isP(toks_[q], Scope);
    }
    // "<" dopo il nome di un template di funzione apre gli argomenti?
    bool templateArgsAhead() const {
        int depth = 0;
        for (std::size_t q = p_; q < toks_.size() && toks_[q].t != T::End; ++q) {
            if (isP(toks_[q], Lt)) ++depth;
            else if (isP(toks_[q], Gt)) { if (--depth == 0) return isP(toks_[q + 1], LParen); }
            else if (isP(toks_[q], Shr)) { depth -= 2; if (depth <= 0) return isP(toks_[q + 1], LParen); }
            else if (isP(toks_[q], Semi) || isP(toks_[q], LBrace) || isP(toks_[q], RParen) || isP(toks_[q], AndAnd) || isP(toks_[q], OrOr)) return false;
        }
        return false;
    }
    Node* lambda() {
        const Pos at = cur().pos;
        Node* n = pool_.node(K::Lambda, at);
        expect(LBracket);
        while (!isP(RBracket)) {
            if (isP(Assign) && (isP(peek(), Comma) || isP(peek(), RBracket))) { n->flags = 1; ++p_; }
            else if (isP(Amp) && (isP(peek(), Comma) || isP(peek(), RBracket))) { n->flags = 2; ++p_; }
            else if (cpp_ && isWord("this")) { Node* c = pool_.node(K::This, cur().pos); ++p_; n->list2.push_back(c); }
            else {
                Node* c = pool_.node(K::Name, cur().pos);
                if (accept(Amp)) c->mods |= MRef;
                c->sym = ident("una variabile da catturare");
                if (accept(Assign)) fail("catture con inizializzatore non supportate", c->pos);
                n->list2.push_back(c);
            }
            if (!accept(Comma)) break;
        }
        expect(RBracket);
        if (accept(LParen)) {
            --p_;
            ++p_;
            n->list = parameters();
        }
        while (acceptWord("mutable") || acceptWord("noexcept") || acceptWord("constexpr")) {}
        if (accept(Arrow)) n->type = typeId();
        n->b = block();
        return n;
    }
    Node* postfix(Node* e) {
        while (true) {
            Depth guard(*this);
            const Pos at = cur().pos;
            if (isP(LParen)) {
                ++p_;
                Node* c = pool_.node(K::Call, at);
                c->a = e;
                if (!isP(RParen)) { do c->list.push_back(isP(LBrace) ? initList() : assignment()); while (accept(Comma)); }
                expect(RParen);
                e = c;
                continue;
            }
            if (isP(LBracket)) {
                ++p_;
                Node* ix = pool_.node(K::Index, at);
                ix->a = e;
                ix->list.push_back(expression());
                expect(RBracket);
                e = ix;
                continue;
            }
            if (isP(Dot) || isP(Arrow)) {
                const bool arrow = isP(Arrow);
                ++p_;
                Node* m = pool_.node(arrow ? K::Arrow : K::Member, at);
                m->a = e;
                if (cpp_ && isP(Tilde)) { ++p_; m->sym = syms_.intern("~" + syms_[ident()]); }
                else if (cpp_ && isWord("template")) { ++p_; m->sym = ident(); }
                else if (cpp_ && isWord("operator")) m->sym = operatorName();
                else m->sym = ident("il nome di un membro");
                // d.B::f(): membro della classe base B, chiamata non virtuale
                while (cpp_ && isP(Scope) && peek().t == T::Ident) {
                    if (!m->type2) { m->type2 = pool_.type(TF::Named, at); m->type2->name = m->sym; }
                    else { m->type2->qual.push_back(m->type2->name); m->type2->name = m->sym; }
                    ++p_;
                    m->sym = ident("il nome di un membro");
                }
                if (isP(Lt) && templateArgsAhead() && knownTemplate(syms_[m->sym])) m->targs = templateArgs();
                e = m;
                continue;
            }
            if (cpp_ && isP(Scope) && (peek().t == T::Ident || isP(peek(), Tilde))) {
                ++p_;
                Node* s = pool_.node(K::Scope, at);
                s->a = e;
                if (accept(Tilde)) s->sym = syms_.intern("~" + syms_[ident()]);
                else if (isWord("operator")) s->sym = operatorName();
                else s->sym = ident();
                std::string path = scopePath(s);
                if (isP(Lt) && (knownTemplate(path) || knownTemplate(syms_[s->sym])) && templateArgsAhead()) s->targs = templateArgs();
                e = s;
                continue;
            }
            if (isP(PlusPlus)) { ++p_; Node* n = pool_.node(K::PostInc, at); n->a = e; e = n; continue; }
            if (isP(MinusMinus)) { ++p_; Node* n = pool_.node(K::PostDec, at); n->a = e; e = n; continue; }
            if (isP(DotStar) || isP(ArrowStar)) error("puntatori a membri non supportati");
            return e;
        }
    }
    std::string scopePath(const Node* n) const {
        if (!n) return "";
        if (n->k == K::Name) return syms_[n->sym];
        if (n->k == K::Scope) { const std::string l = scopePath(n->a); return l.empty() ? syms_[n->sym] : l + "::" + syms_[n->sym]; }
        if (n->k == K::TypeExpr && n->type) {
            std::string s;
            for (Sym q : n->type->qual) s += syms_[q] + "::";
            return s + syms_[n->type->name];
        }
        return "";
    }

    const Lexed& lx_;
    std::vector<Token> toks_;
    Unit& unit_;
    ParseContext& ctx_;
    Symbols& syms_;
    NodePool& pool_;
    bool cpp_{false};
    bool noGt_{false};                                  // dentro gli argomenti di un template
    std::size_t p_{0};
    int depth_{0};
    std::size_t polls_{0};
    int anon_{0};
    std::set<std::string> known_, templates_, tags_, namespaces_;
    std::vector<std::string> ns_, classes_, usingNs_;
    std::vector<std::vector<Sym>> tparams_;
    std::vector<Node*> pendingClasses_;
};

} // namespace

void parseCFamily(const Lexed& lexed, Unit& unit, ParseContext& ctx) {
    CParser p(lexed, unit, ctx);
    p.run();
}

} // namespace fx::prog::ast
