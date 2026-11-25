// Compilatore dei programmi (v1.20.0), parte 1: tipi, namespace, classi,
// template, dichiarazioni, disposizione dei campi, vtable, punto d'ingresso.
// Vedi program_compile.hpp.
#include "program_compile.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace fx::prog {

// ================================================================ funzioni comuni dei tipi
int bitsOf(VK k) noexcept {
    switch (k) {
    case VK::Bool: return 1;
    case VK::C8: case VK::I8: case VK::U8: return 8;
    case VK::C16: case VK::I16: case VK::U16: return 16;
    case VK::I32: case VK::U32: case VK::F32: return 32;
    default: return 64;
    }
}
double Value::num() const noexcept {
    switch (k) {
    case VK::F32: case VK::F64: return d;
    case VK::U64: return static_cast<double>(u);
    case VK::Null: return 0;
    default: return static_cast<double>(i);
    }
}
std::int64_t Value::integer() const noexcept {
    if (k == VK::F32 || k == VK::F64) {
        if (std::isnan(d)) return 0;
        if (d >= 9.2233720368547758e18) return INT64_MAX;
        if (d <= -9.2233720368547758e18) return INT64_MIN;
        return static_cast<std::int64_t>(d);
    }
    return i;
}
VK vkOf(TypeP t) noexcept {
    if (!t) return VK::Void;
    switch (t->k) {
    case TK::Void: return VK::Void;
    case TK::Bool: return VK::Bool;
    case TK::C8: return VK::C8;
    case TK::C16: return VK::C16;
    case TK::I8: return VK::I8;
    case TK::U8: return VK::U8;
    case TK::I16: return VK::I16;
    case TK::U16: return VK::U16;
    case TK::I32: return VK::I32;
    case TK::U32: return VK::U32;
    case TK::I64: return VK::I64;
    case TK::U64: return VK::U64;
    case TK::F32: return VK::F32;
    case TK::F64: return VK::F64;
    case TK::Null: return VK::Null;
    case TK::Ptr: return VK::Ptr;
    case TK::Func: return VK::Fn;
    case TK::Enum: return t->cls && t->cls->underlying ? vkOf(t->cls->underlying) : VK::I32;
    case TK::Nullable: return vkOf(t->elem);
    case TK::Ref: return VK::Ptr;
    default: return VK::Ref;
    }
}
bool isNumeric(TypeP t) noexcept { return t && ((t->k >= TK::Bool && t->k <= TK::F64) || t->k == TK::Enum); }
bool isIntegral(TypeP t) noexcept { return t && ((t->k >= TK::Bool && t->k <= TK::U64) || t->k == TK::Enum); }
bool isRefLike(TypeP t) noexcept { return t && (t->k == TK::Ptr || t->k == TK::Null || (t->k == TK::Class && t->cls && !t->cls->valueType) || (t->k == TK::Str && !t->cppValue)); }

std::string TypeRef::name() const {
    switch (k) {
    case TK::Void: return "void";
    case TK::Bool: return "bool";
    case TK::C8: case TK::C16: return "char";
    case TK::I8: return cls ? "sbyte" : "signed char";
    case TK::U8: return "unsigned char";
    case TK::I16: return "short";
    case TK::U16: return "unsigned short";
    case TK::I32: return "int";
    case TK::U32: return "unsigned int";
    case TK::I64: return "long";
    case TK::U64: return "unsigned long";
    case TK::F32: return "float";
    case TK::F64: return "double";
    case TK::Null: return "null";
    case TK::Str: return cppValue ? "std::string" : "string";
    case TK::Ptr: return (elem ? elem->name() : std::string("void")) + "*";
    case TK::Ref: return (elem ? elem->name() : std::string("?")) + "&";
    case TK::Array: {
        std::string s = elem ? elem->name() : "?";
        if (size >= 0) return s + "[" + std::to_string(size) + "]";
        s += "[";
        for (int r = 1; r < rank; ++r) s += ",";
        return s + "]";
    }
    case TK::Class: case TK::Enum: return cls ? cls->name : "?";
    case TK::Nullable: return (elem ? elem->name() : std::string("?")) + "?";
    case TK::Func: return "funzione";
    case TK::Dynamic: return size == -2 ? "auto" : size >= 0x10000 ? std::to_string(size - 0x10000) : "?";
    case TK::Native: {
        static const char* const names[] = {"", "std::vector", "std::map", "std::set", "std::unordered_map", "std::unordered_set",
            "std::pair", "std::stack", "std::queue", "std::deque", "std::list", "std::ostream", "std::istream", "std::ifstream",
            "std::ofstream", "std::fstream", "std::stringstream", "iterator", "iterator", "std::greater", "std::less", "std::exception",
            "FILE", "pair", "manipolatore", "List", "Dictionary", "HashSet", "Queue", "Stack", "KeyValuePair", "StringBuilder", "StreamReader",
            "StreamWriter", "Random", "ArrayList", "LinkedList", "HashMap", "TreeMap", "HashSet", "TreeSet", "ArrayDeque",
            "StringBuilder", "Scanner", "Random", "Iterator", "Map.Entry", "Optional"};
        std::string s = static_cast<std::size_t>(nk) < sizeof(names) / sizeof(names[0]) ? names[static_cast<std::size_t>(nk)] : "?";
        if (!args.empty()) {
            s += "<";
            for (std::size_t i = 0; i < args.size(); ++i) s += (i ? ", " : "") + (args[i] ? args[i]->name() : std::string("?"));
            s += ">";
        }
        return s;
    }
    }
    return "?";
}
bool ClassInfo::derivesFrom(const ClassInfo* other) const noexcept {
    if (!other) return false;
    for (const ClassInfo* c = this; c; c = c->base) {
        if (c == other) return true;
        for (const ClassInfo* i : c->interfaces) if (i == other || i->derivesFrom(other)) return true;
    }
    return false;
}
const FieldInfo* ClassInfo::findField(const std::string& n) const noexcept {
    for (const ClassInfo* c = this; c; c = c->base)
        for (const FieldInfo& f : c->fields) if (f.name == n) return &f;
    return nullptr;
}
std::int32_t ClassInfo::fieldSlot(std::int32_t localIndex) const noexcept { return (base ? base->slotCount : 0) + localIndex; }
std::string Function::signature() const {
    std::string s = name + "(";
    for (std::size_t i = 0; i < params.size(); ++i) s += (i ? ", " : "") + (params[i].type ? params[i].type->name() : std::string("?"));
    return s + ")";
}

// ================================================================ compilatore
using namespace ast;

Compiler::Compiler(Program& prog, std::vector<Unit>& units, Symbols& syms, NodePool& pool, std::function<void()> poll)
    : prog_(prog), units_(units), syms_(syms), pool_(pool), poll_(std::move(poll)) {
    root_ = std::make_unique<Namespace>();
    tVoid_ = prim(TK::Void); tBool_ = prim(TK::Bool); tChar8_ = prim(TK::C8); tChar16_ = prim(TK::C16);
    tI8_ = prim(TK::I8); tU8_ = prim(TK::U8); tI16_ = prim(TK::I16); tU16_ = prim(TK::U16);
    tI32_ = prim(TK::I32); tU32_ = prim(TK::U32); tI64_ = prim(TK::I64); tU64_ = prim(TK::U64);
    tF32_ = prim(TK::F32); tF64_ = prim(TK::F64); tNull_ = prim(TK::Null); tDyn_ = prim(TK::Dynamic);
    TypeRef s; s.k = TK::Str; s.cppValue = true; tCppStr_ = intern(s);
    s.cppValue = false; tCsStr_ = intern(s);
    tVoidPtr_ = ptrTo(tVoid_);
    tCStr_ = ptrTo(tChar8_);
    TypeRef o; o.k = TK::Dynamic; o.size = -3; tObject_ = intern(o);   // object di C#: qualsiasi valore
    TypeRef ai; ai.k = TK::Dynamic; ai.size = -4; tAnyIter_ = intern(ai);
    for (int i = 0; i < 3; ++i) { TypeRef m; m.k = TK::Dynamic; m.size = -10 - i; tArg_[i] = intern(m); }
    for (int i = 0; i < 3; ++i) { TypeRef m; m.k = TK::Dynamic; m.size = -13 - i; tElem_[i] = intern(m); }
    for (TK k : {TK::Void, TK::Bool, TK::C8, TK::C16, TK::I8, TK::U8, TK::I16, TK::U16, TK::I32, TK::U32, TK::I64, TK::U64, TK::F32, TK::F64})
        prog_.prim[static_cast<int>(k)] = prim(k);
    prog_.cppString = tCppStr_;
    prog_.csString = tCsStr_;
    prog_.dynamic = tDyn_;
    prog_.object = tObject_;
    prog_.voidPtr = tVoidPtr_;
    prog_.charPtr = tCStr_;
}

void Compiler::error(const std::string& msg, Pos at) const { fail(msg, at); }

TypeP Compiler::intern(const TypeRef& t) {
    std::string key;
    key.reserve(48);
    key += std::to_string(static_cast<int>(t.k)) + "," + std::to_string(static_cast<int>(t.nk)) + "," + (t.cppValue ? "1" : "0") + "," +
           std::to_string(t.size) + "," + std::to_string(t.rank) + "," + std::to_string(t.elem ? t.elem->index : -1) + "," +
           std::to_string(t.cls ? t.cls->id : -1) + "," + std::to_string(t.ret ? t.ret->index : -1);
    for (TypeP a : t.args) key += ";" + std::to_string(a ? a->index : -1);
    const auto it = typeIndex_.find(key);
    if (it != typeIndex_.end()) return it->second;
    auto p = std::make_unique<TypeRef>(t);
    p->isConst = false;
    p->index = static_cast<std::int32_t>(prog_.types.size());
    TypeP r = p.get();
    prog_.types.push_back(std::move(p));
    typeIndex_.emplace(key, r);
    return r;
}
TypeP Compiler::prim(TK k) { TypeRef t; t.k = k; return intern(t); }
TypeP Compiler::ptrTo(TypeP t) { TypeRef r; r.k = TK::Ptr; r.elem = t; return intern(r); }
TypeP Compiler::refTo(TypeP t) { if (t && t->k == TK::Ref) return t; TypeRef r; r.k = TK::Ref; r.elem = t; return intern(r); }
TypeP Compiler::arrayOf(TypeP elem, std::int32_t size, std::uint8_t rank) { TypeRef r; r.k = TK::Array; r.elem = elem; r.size = size; r.rank = rank; return intern(r); }
TypeP Compiler::nativeOf(NK nk, std::vector<TypeP> args) { TypeRef r; r.k = TK::Native; r.nk = nk; r.args = std::move(args); return intern(r); }
TypeP Compiler::funcOf(TypeP ret, std::vector<TypeP> params) { TypeRef r; r.k = TK::Func; r.ret = ret; r.args = std::move(params); return intern(r); }
TypeP Compiler::nullableOf(TypeP t) { if (!t || t->k == TK::Nullable || (!isValueType(t) && !isNumeric(t))) return t; TypeRef r; r.k = TK::Nullable; r.elem = t; return intern(r); }
TypeP Compiler::classType(ClassInfo* c) {
    if (c->self) return c->self;
    TypeRef r;
    r.k = c->isEnum ? TK::Enum : TK::Class;
    r.cls = c;
    c->self = intern(r);
    return c->self;
}
TypeP Compiler::stringType(Dialect d) { return d == Dialect::Cpp ? tCppStr_ : d == Dialect::C ? tCStr_ : tCsStr_; }
std::string Compiler::typeName(TypeP t) const { return t ? t->name() : "?"; }

bool Compiler::isValueType(TypeP t) const {
    if (!t) return false;
    switch (t->k) {
    case TK::Class: return t->cls && t->cls->valueType;
    case TK::Str: return t->cppValue;
    case TK::Native:
        switch (t->nk) {
        case NK::Vector: case NK::Map: case NK::Set: case NK::UMap: case NK::USet: case NK::Pair: case NK::Stack: case NK::Queue:
        case NK::Deque: case NK::List: case NK::StringStream: case NK::CsKvp:
            return true;
        default: return false;
        }
    case TK::Array: return t->size >= 0;                 // array del C/C++ dentro una struct
    default: return false;
    }
}
bool Compiler::needsDtor(TypeP t) const {
    if (!t) return false;
    if (t->k == TK::Class && t->cls) return t->cls->hasDtorChain;
    if (t->k == TK::Array && t->size >= 0) return needsDtor(t->elem);
    return false;
}

std::int64_t Compiler::alignOf(TypeP t) {
    if (!t) return 1;
    switch (t->k) {
    case TK::Bool: case TK::C8: case TK::I8: case TK::U8: return 1;
    case TK::C16: case TK::I16: case TK::U16: return 2;
    case TK::I32: case TK::U32: case TK::F32: case TK::Enum: return 4;
    case TK::Array: return alignOf(t->elem);
    case TK::Class: {
        std::int64_t a = 1;
        if (t->cls) for (const ClassInfo* c = t->cls; c; c = c->base) for (const auto& f : c->fields) if (!f.isStatic) a = std::max(a, alignOf(f.type));
        return a;
    }
    default: return 8;
    }
}
std::int64_t Compiler::sizeOf(TypeP t, Pos at) {
    if (!t) error("sizeof di un tipo sconosciuto", at);
    switch (t->k) {
    case TK::Void: error("sizeof(void) non valido", at);
    case TK::Bool: case TK::C8: case TK::I8: case TK::U8: return 1;
    case TK::C16: case TK::I16: case TK::U16: return 2;
    case TK::I32: case TK::U32: case TK::F32: case TK::Enum: return 4;
    case TK::I64: case TK::U64: case TK::F64: case TK::Ptr: case TK::Func: case TK::Null: return 8;
    case TK::Ref: return sizeOf(t->elem, at);
    case TK::Str: return 32;                             // std::string (libstdc++)
    case TK::Array:
        if (t->size < 0) error("sizeof di un array senza dimensione", at);
        return t->size * sizeOf(t->elem, at);
    case TK::Class: {
        if (!t->cls || !t->cls->complete) error("sizeof di un tipo incompleto", at);
        std::int64_t off = 0, al = 1;
        std::vector<const ClassInfo*> chain;
        for (const ClassInfo* c = t->cls; c; c = c->base) chain.push_back(c);
        std::reverse(chain.begin(), chain.end());
        bool virt = false;
        for (const ClassInfo* c : chain) if (!c->vtable.empty()) virt = true;
        if (virt) { off = 8; al = 8; }
        for (const ClassInfo* c : chain)
            for (const auto& f : c->fields) {
                if (f.isStatic) continue;
                const std::int64_t a = alignOf(f.type);
                al = std::max(al, a);
                off = (off + a - 1) / a * a;
                off += sizeOf(f.type, at);
            }
        if (off == 0) return 1;
        return (off + al - 1) / al * al;
    }
    case TK::Native:
        switch (t->nk) {
        case NK::Vector: return 24;
        case NK::Map: case NK::Set: return 48;
        case NK::Pair: return sizeOf(t->args[0], at) + sizeOf(t->args[1], at);
        default: return 56;
        }
    default: return 8;
    }
}

// ================================================================ namespace
std::string Compiler::qualify(Namespace* ns, const std::string& n) const {
    if (!ns || ns->name.empty()) return n;
    return ns->name + "::" + n;
}
Namespace* Compiler::namespaceFor(Namespace* from, const std::string& qualified, bool create) {
    Namespace* ns = from ? from : root_.get();
    std::size_t b = 0;
    while (b <= qualified.size()) {
        std::size_t e = qualified.find("::", b);
        const std::string part = qualified.substr(b, e == std::string::npos ? std::string::npos : e - b);
        if (part.empty()) break;
        auto it = ns->members.find(part);
        if (it != ns->members.end() && it->second.kind == NsEntry::Namespace) ns = it->second.ns;
        else if (it != ns->members.end() && it->second.kind == NsEntry::Class && classScope_.count(it->second.cls)) ns = classScope_[it->second.cls];
        else {
            if (!create) return nullptr;
            auto child = std::make_unique<Namespace>();
            child->name = ns->name.empty() ? part : ns->name + "::" + part;
            child->parent = ns;
            Namespace* raw = child.get();
            ns->children.push_back(std::move(child));
            NsEntry e2;
            e2.kind = NsEntry::Namespace;
            e2.ns = raw;
            ns->members[part] = e2;
            ns = raw;
        }
        if (e == std::string::npos) break;
        b = e + 2;
    }
    return ns;
}
NsEntry* Compiler::lookupScoped(Namespace* ns, const std::string& n) {
    if (!ns) return nullptr;
    const std::size_t sep = n.rfind("::");
    if (sep != std::string::npos) {
        Namespace* target = namespaceFor(ns, n.substr(0, sep), false);
        if (!target) return nullptr;
        return lookupScoped(target, n.substr(sep + 2));
    }
    const auto it = ns->members.find(n);
    return it == ns->members.end() ? nullptr : &it->second;
}
// Nome non qualificato dal punto di vista di `from` (e della classe `cls`).
NsEntry* Compiler::lookupGlobalName(const std::string& n, Namespace* from, ClassInfo* cls) {
    // classi (tipi annidati e membri statici) dalla piu' interna
    for (ClassInfo* c = cls; c; c = c->outer) {
        for (ClassInfo* k = c; k; k = k->base) {
            auto sc = classScope_.find(k);
            if (sc != classScope_.end()) {
                NsEntry* e = sc->second ? lookupScoped(sc->second, n) : nullptr;
                if (e) return e;
            }
        }
    }
    const bool qualified = n.find("::") != std::string::npos;
    for (Namespace* ns = from ? from : root_.get(); ns; ns = ns->parent) {
        if (qualified) {
            // A::b relativo a questo namespace
            if (NsEntry* e = lookupScoped(ns, n)) return e;
            continue;
        }
        const auto it = ns->members.find(n);
        if (it != ns->members.end()) return &it->second;
        for (Namespace* u : ns->usings) {
            const auto ui = u->members.find(n);
            // C++: using namespace geo rende visibili anche i namespace annidati (piano::area);
            // C#: using System non importa System.Collections come "Collections"
            if (ui != u->members.end() && (ui->second.kind != NsEntry::Namespace || lang_ == Dialect::Cpp)) return &ui->second;
        }
    }
    if (qualified) return lookupScoped(root_.get(), n);
    return nullptr;
}

// ================================================================ classi native
ClassInfo* Compiler::newClass(const std::string& nm, Dialect d) {
    auto c = std::make_unique<ClassInfo>();
    c->name = nm;
    c->qualified = nm;
    c->lang = d;
    c->id = static_cast<std::int32_t>(prog_.classes.size());
    ClassInfo* raw = c.get();
    prog_.classes.push_back(std::move(c));
    return raw;
}
Function* Compiler::newFunction(const std::string& nm, Dialect d, ClassInfo* owner) {
    auto f = std::make_unique<Function>();
    f->name = owner ? owner->name + (d == Dialect::CSharp || d == Dialect::Java ? "." : "::") + nm : nm;
    f->simple = nm;
    f->lang = d;
    f->owner = owner;
    f->index = static_cast<std::int32_t>(prog_.functions.size());
    f->library = library_;
    f->ret = tVoid_;
    Function* raw = f.get();
    prog_.functions.push_back(std::move(f));
    return raw;
}
// C/C++: sqrt, sin, strlen... come valori (puntatori a funzione): una funzione
// senza istruzioni del sorgente che passa i parametri al nativo e ne restituisce
// il risultato, una per nativo. Fra piu' versioni (C++: float/double) si sceglie
// quella con parametri e risultato dello stesso tipo, poi la prima.
Function* Compiler::nativeWrapper(const std::vector<std::int32_t>& natives, ast::Pos at) {
    std::int32_t pick = -1;
    TypeP ret = nullptr;
    std::vector<TypeP> params;
    for (std::int32_t idx : natives) {
        const NativeEntry& ne = nativeEntry(idx);
        if (ne.owner[0] || (ne.flags & (NF::Property | NF::Indexer | NF::Ctor | NF::Operator))) continue;
        std::vector<TypeP> ps;
        bool variadic = false;
        TypeP r = parseSig(ne.sig, ps, variadic, nullptr, lang_);
        bool plain = !variadic && r;
        for (TypeP t : ps) if (!t || t->k == TK::Dynamic || t->k == TK::Ref) plain = false;
        if (!plain || (r->k == TK::Dynamic)) continue;
        bool uniform = true;
        for (TypeP t : ps) if (t != r) uniform = false;
        if (pick < 0 || (uniform && r->k == TK::F64)) { pick = idx; ret = r; params = ps; if (uniform && r->k == TK::F64) break; }
    }
    if (pick < 0) error("questa funzione della libreria non si usa come valore (usa una funzione tua che la chiama)", at);
    const auto it = nativeWrappers_.find(pick);
    if (it != nativeWrappers_.end()) return it->second;
    const NativeEntry& ne = nativeEntry(pick);
    Function* f = newFunction(ne.name, lang_, nullptr);
    f->isStatic = true;
    f->hasThis = false;
    f->defined = true;
    f->library = -1;
    f->file = at.file;
    f->line = static_cast<int>(at.line);
    f->column = at.col;
    f->ret = ret;
    for (std::size_t i = 0; i < params.size(); ++i) {
        Param p;
        p.name = "x" + std::to_string(i + 1);
        p.type = params[i];
        f->params.push_back(p);
    }
    f->nlocals = static_cast<std::int32_t>(params.size());
    const bool value = ret->k != TK::Void;
    auto put = [&](Op op, std::int32_t a, std::uint8_t b, std::uint16_t c) {
        Instr ins;
        ins.op = op;
        ins.a = a;
        ins.b = b;
        ins.c = c;
        f->code.push_back(ins);
        SrcPos sp;
        sp.file = at.file;
        sp.line = at.line;
        sp.column = at.col;
        f->pos.push_back(sp);
        f->stmt.push_back(0);                      // il debugger non si ferma qui
    };
    for (std::size_t i = 0; i < params.size(); ++i) put(Op::LoadL, static_cast<std::int32_t>(i), 0, 0);
    put(Op::CallN, pick, static_cast<std::uint8_t>(params.size()), value ? 1 : 0);
    put(Op::Ret, 0, value ? 1 : 0, 0);
    nativeWrappers_[pick] = f;
    return f;
}
// Classi del nucleo: eccezioni standard dei linguaggi. Campi: 0 messaggio,
// 1 eccezione interna, 2 pila (testo).
void Compiler::setupNativeClasses() {
    auto exc = [&](Dialect d, const std::string& nm, ClassInfo* base, const std::string& nsName) -> ClassInfo* {
        ClassInfo* c = newClass(nm, d);
        c->isNative = true;
        c->isException = true;
        c->base = base;
        c->complete = true;
        c->valueType = d == Dialect::Cpp;            // in C++ si lanciano per valore
        if (!base) {
            FieldInfo m; m.name = "#message"; m.type = d == Dialect::Cpp ? tCppStr_ : tCsStr_; m.access = Access::Private; m.owner = c;
            FieldInfo in; in.name = "#inner"; in.type = tObject_; in.access = Access::Private; in.owner = c;
            FieldInfo st; st.name = "#stack"; st.type = d == Dialect::Cpp ? tCppStr_ : tCsStr_; st.access = Access::Private; st.owner = c;
            FieldInfo pn; pn.name = "#param"; pn.type = d == Dialect::Cpp ? tCppStr_ : tCsStr_; pn.access = Access::Private; pn.owner = c;   // C#: ParamName
            c->fields = {m, in, st, pn};
            c->slotCount = 4;
            for (std::int32_t i = 0; i < 4; ++i) c->fields[static_cast<std::size_t>(i)].slot = i;
        } else {
            c->slotCount = base->slotCount;
        }
        classType(c);
        nativeClasses_[(d == Dialect::Cpp ? "c++:" : d == Dialect::Java ? "java:" : "cs:") + nm] = c;
        Namespace* ns = namespaceFor(root_.get(), nsName, true);
        NsEntry e;
        e.kind = NsEntry::Class;
        e.cls = c;
        ns->members[nm] = e;
        c->qualified = nsName.empty() ? nm : nsName + "::" + nm;
        return c;
    };
    if (lang_ == Dialect::Cpp || prog_.dialect == Dialect::Cpp) {
        ClassInfo* e = exc(Dialect::Cpp, "exception", nullptr, "std");
        ClassInfo* le = exc(Dialect::Cpp, "logic_error", e, "std");
        ClassInfo* re = exc(Dialect::Cpp, "runtime_error", e, "std");
        exc(Dialect::Cpp, "out_of_range", le, "std");
        exc(Dialect::Cpp, "invalid_argument", le, "std");
        exc(Dialect::Cpp, "length_error", le, "std");
        exc(Dialect::Cpp, "domain_error", le, "std");
        exc(Dialect::Cpp, "overflow_error", re, "std");
        exc(Dialect::Cpp, "underflow_error", re, "std");
        exc(Dialect::Cpp, "range_error", re, "std");
        ClassInfo* ba = exc(Dialect::Cpp, "bad_alloc", e, "std");
        exc(Dialect::Cpp, "bad_cast", e, "std");
        prog_.exceptionRoot[static_cast<int>(Dialect::Cpp)] = e;
        prog_.faultClass[static_cast<int>(Dialect::Cpp)][static_cast<int>(FaultKind::Memory)] = ba;
    }
    if (prog_.dialect == Dialect::CSharp) {
        const int d = static_cast<int>(Dialect::CSharp);
        ClassInfo* e = exc(Dialect::CSharp, "Exception", nullptr, "System");
        ClassInfo* se = exc(Dialect::CSharp, "SystemException", e, "System");
        ClassInfo* ae = exc(Dialect::CSharp, "ArgumentException", se, "System");
        ClassInfo* ane = exc(Dialect::CSharp, "ArgumentNullException", ae, "System");
        ClassInfo* aoe = exc(Dialect::CSharp, "ArgumentOutOfRangeException", ae, "System");
        ClassInfo* ioe = exc(Dialect::CSharp, "InvalidOperationException", se, "System");
        ClassInfo* fe = exc(Dialect::CSharp, "FormatException", se, "System");
        ClassInfo* ae2 = exc(Dialect::CSharp, "ArithmeticException", se, "System");
        ClassInfo* ofe = exc(Dialect::CSharp, "OverflowException", ae2, "System");
        ClassInfo* dze = exc(Dialect::CSharp, "DivideByZeroException", ae2, "System");
        ClassInfo* ire = exc(Dialect::CSharp, "IndexOutOfRangeException", se, "System");
        ClassInfo* nre = exc(Dialect::CSharp, "NullReferenceException", se, "System");
        ClassInfo* kne = exc(Dialect::CSharp, "KeyNotFoundException", se, "System::Collections::Generic");
        ClassInfo* nie = exc(Dialect::CSharp, "NotImplementedException", se, "System");
        ClassInfo* nse = exc(Dialect::CSharp, "NotSupportedException", se, "System");
        ClassInfo* io = exc(Dialect::CSharp, "IOException", se, "System::IO");
        ClassInfo* fnf = exc(Dialect::CSharp, "FileNotFoundException", io, "System::IO");
        ClassInfo* uae = exc(Dialect::CSharp, "UnauthorizedAccessException", se, "System");
        ClassInfo* ice = exc(Dialect::CSharp, "InvalidCastException", se, "System");
        ClassInfo* ome = exc(Dialect::CSharp, "OutOfMemoryException", se, "System");
        exc(Dialect::CSharp, "StackOverflowException", se, "System");
        exc(Dialect::CSharp, "ApplicationException", e, "System");
        ClassInfo* fxe = exc(Dialect::CSharp, "FxException", e, "System");
        prog_.exceptionRoot[d] = e;
        auto& f = prog_.faultClass[d];
        f[static_cast<int>(FaultKind::Generic)] = se;
        f[static_cast<int>(FaultKind::NullPointer)] = nre;
        f[static_cast<int>(FaultKind::Index)] = ire;
        f[static_cast<int>(FaultKind::DivZero)] = dze;
        f[static_cast<int>(FaultKind::Overflow)] = ofe;
        f[static_cast<int>(FaultKind::Cast)] = ice;
        f[static_cast<int>(FaultKind::Memory)] = ome;
        f[static_cast<int>(FaultKind::Argument)] = aoe;
        f[static_cast<int>(FaultKind::Format)] = fe;
        f[static_cast<int>(FaultKind::Key)] = kne;
        f[static_cast<int>(FaultKind::InvalidOp)] = ioe;
        f[static_cast<int>(FaultKind::Io)] = io;
        f[static_cast<int>(FaultKind::NotFound)] = fnf;
        f[static_cast<int>(FaultKind::Unauthorized)] = uae;
        f[static_cast<int>(FaultKind::NotSupported)] = nse;
        f[static_cast<int>(FaultKind::NotImplemented)] = nie;
        f[static_cast<int>(FaultKind::Generic) + 20] = fxe;     // eccezioni della libreria fx
        (void)ane;
        // System.Type di GetType(): Name e FullName (sola lettura)
        ClassInfo* ty = newClass("Type", Dialect::CSharp);
        ty->isNative = true;
        ty->complete = true;
        ty->isSealed = true;
        for (const char* nm : {"Name", "FullName"}) {
            FieldInfo fi; fi.name = nm; fi.type = tCsStr_; fi.access = Access::Public; fi.isReadonly = true; fi.owner = ty;
            fi.slot = static_cast<std::int32_t>(ty->fields.size());
            ty->fields.push_back(fi);
        }
        ty->slotCount = 2;
        classType(ty);
        nativeClasses_["cs:Type"] = ty;
        NsEntry te;
        te.kind = NsEntry::Class;
        te.cls = ty;
        namespaceFor(root_.get(), "System", true)->members["Type"] = te;
        ty->qualified = "System::Type";
        prog_.typeClass = ty;
    }
    // Risultati delle statistiche della libreria fx (§4.5): campi double in sola lettura
    // (C/C++: struct per valore). C: FxStat1, FxStat2, FxReg; C++: fx::Stat1...; C#/Java: Fx.Stat1...
    static const char* const kStat1[] = {"n", "mean", "sum", "sumSq", "popSd", "sampleSd", "min", "q1", "median", "q3", "max", nullptr};
    static const char* const kStat2[] = {"n", "meanX", "sumX", "sumX2", "popSdX", "sampleSdX", "meanY", "sumY", "sumY2", "popSdY", "sampleSdY",
                                         "sumXY", "minX", "maxX", "minY", "maxY", nullptr};
    static const char* const kReg[] = {"a", "b", "c", "d", "e", "r", "r2", "n", nullptr};
    static const char* const* const kFields[3] = {kStat1, kStat2, kReg};
    static const char* const kNames[3] = {"Stat1", "Stat2", "Reg"};
    auto fxResults = [&](Dialect d) {
        const bool cfam = d == Dialect::C || d == Dialect::Cpp;
        for (int k = 0; k < 3; ++k) {
            const std::string nm = d == Dialect::C ? std::string("Fx") + kNames[k] : std::string(kNames[k]);
            ClassInfo* c = newClass(nm, d);
            c->isNative = true;
            c->complete = true;
            c->isStruct = cfam;
            c->valueType = cfam;
            for (const char* const* f = kFields[k]; *f; ++f) {
                FieldInfo fi;
                fi.name = *f;
                if (d == Dialect::CSharp) fi.name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(fi.name[0])));   // Mean, SampleSd
                fi.type = tF64_;
                fi.access = Access::Public;
                fi.isReadonly = !cfam;
                fi.owner = c;
                fi.slot = static_cast<std::int32_t>(c->fields.size());
                c->fields.push_back(fi);
            }
            c->slotCount = static_cast<std::int32_t>(c->fields.size());
            classType(c);
            NsEntry e;
            e.kind = NsEntry::Class;
            e.cls = c;
            if (d == Dialect::C) { root_->members[nm] = e; c->qualified = nm; }
            else if (d == Dialect::Cpp) { namespaceFor(root_.get(), "fx", true)->members[nm] = e; c->qualified = "fx::" + nm; }
            else c->qualified = "Fx::" + nm;                 // Fx.Stat1: si risolve in resolveType
            prog_.fxResult[static_cast<int>(d)][k] = c;
        }
    };
    if (prog_.dialect == Dialect::C || prog_.dialect == Dialect::Cpp) { fxResults(Dialect::C); if (prog_.dialect == Dialect::Cpp) fxResults(Dialect::Cpp); }
    else fxResults(prog_.dialect);
}
ClassInfo* Compiler::exceptionClass(Dialect d, const std::string& nm) {
    const auto it = nativeClasses_.find((d == Dialect::Cpp ? "c++:" : d == Dialect::Java ? "java:" : "cs:") + nm);
    return it == nativeClasses_.end() ? nullptr : it->second;
}
ClassInfo* Compiler::nativeClass(const std::string& qualified, Dialect d) {
    std::string n = qualified;
    const std::size_t sep = n.rfind("::");
    if (sep != std::string::npos) n = n.substr(sep + 2);
    return exceptionClass(d, n);
}

// ================================================================ risoluzione dei tipi
namespace {
TK builtinKind(const std::string& w, Dialect d, bool& ok) {
    ok = true;
    const bool cs = d == Dialect::CSharp || d == Dialect::Java;
    if (w == "void") return TK::Void;
    if (w == "bool" || w == "boolean") return TK::Bool;
    if (w == "char") return cs ? TK::C16 : TK::C8;
    if (w == "signed char" || w == "sbyte") return TK::I8;
    if (w == "unsigned char") return TK::U8;
    if (w == "byte") return d == Dialect::Java ? TK::I8 : TK::U8;
    if (w == "short") return TK::I16;
    if (w == "unsigned short" || w == "ushort") return TK::U16;
    if (w == "int") return TK::I32;
    if (w == "unsigned int" || w == "uint") return TK::U32;
    if (w == "long" || w == "long long") return TK::I64;
    if (w == "unsigned long" || w == "unsigned long long" || w == "ulong") return TK::U64;
    if (w == "float") return TK::F32;
    if (w == "double") return TK::F64;
    ok = false;
    return TK::Void;
}
} // namespace

TypeP Compiler::resolveType(TypeNode* t, bool allowVoid) {
    if (!t) return tDyn_;
    switch (t->f) {
    case TF::Auto: { TypeRef a; a.k = TK::Dynamic; a.size = -2; return intern(a); }
    case TF::Pointer: {
        TypeP in = resolveType(t->inner, true);
        if (in && in->k == TK::Ref) error("puntatore a un riferimento non valido", t->pos);
        return ptrTo(in);
    }
    case TF::Reference: {
        TypeP in = resolveType(t->inner, false);
        if (in->k == TK::Void) error("riferimento a void non valido", t->pos);
        return refTo(in);
    }
    case TF::Array: {
        TypeP in = resolveType(t->inner, false);
        if (in->k == TK::Void) error("array di void non valido", t->pos);
        std::int32_t size = -1;
        if (t->size) {
            std::int64_t v = 0;
            if (constInt(t->size, v)) {
                if (v <= 0) error("la dimensione di un array deve essere positiva", t->pos);
                if (v > 16 * 1024 * 1024) error("array troppo grande", t->pos);
                size = static_cast<std::int32_t>(v);
            } else {
                size = -4;                               // array a lunghezza variabile (VLA)
            }
        }
        if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) return arrayOf(in, -1, t->rank);
        return arrayOf(in, size, 1);
    }
    case TF::Nullable: {
        TypeP in = resolveType(t->inner, false);
        if (in->k == TK::Class && in->cls && !in->cls->valueType) return in;   // riferimento: gia' annullabile
        if (in->k == TK::Str) return in;
        return nullableOf(in);
    }
    case TF::Func: {
        TypeP ret = resolveType(t->inner, true);
        std::vector<TypeP> ps;
        for (TypeNode* a : t->args) {
            TypeP pt = resolveType(a, true);
            if (pt->k == TK::Array) pt = ptrTo(pt->elem);
            if (pt->k != TK::Void) ps.push_back(pt);
        }
        return funcOf(ret, ps);
    }
    case TF::Named: break;
    }
    const std::string& w = syms_[t->name];
    if (t->builtin) {
        bool ok = false;
        const TK k = builtinKind(w, lang_, ok);
        if (ok) {
            if (k == TK::Void && !allowVoid) error("void non e' un tipo valido qui", t->pos);
            return prim(k);
        }
        if (w == "string") return stringType(lang_ == Dialect::C ? Dialect::Cpp : lang_);
        if (w == "object") return tObject_;
        error("tipo '" + w + "' non supportato", t->pos);
    }
    // argomento intero di un template (Pila<int, 4>): segnaposto con il valore
    if (w == "#value") {
        std::int64_t v = 0;
        if (!t->size || !constInt(t->size, v)) error("l'argomento del template deve essere una costante intera", t->pos);
        if (v < 0 || v > 0x3FFFFFFF) error("argomento intero del template fuori campo (0..1073741823)", t->pos);
        TypeRef r;
        r.k = TK::Dynamic;
        r.size = static_cast<std::int32_t>(v + 0x10000);
        return intern(r);
    }
    // parametro di template legato
    if (t->qual.empty() && !t->inner) {
        for (auto it = tbind_.rbegin(); it != tbind_.rend(); ++it) if (it->first == w) return it->second;
    }
    std::string path;
    for (Sym q : t->qual) path += syms_[q] + "::";
    // C: struct X / enum X (spazio dei tag)
    if ((lang_ == Dialect::C) && t->qual.size() == 1 && (syms_[t->qual[0]] == "struct" || syms_[t->qual[0]] == "enum")) {
        NsEntry* e = lookupGlobalName(syms_[t->qual[0]] + " " + w, ns_, cls_);
        if (!e || (e->kind != NsEntry::Class)) error(syms_[t->qual[0]] + " " + w + " non definita", t->pos);
        return classType(e->cls);
    }
    // tipi annidati di un template: vector<int>::iterator
    if (t->inner) {
        TypeP owner = resolveType(t->inner, false);
        if (owner->k == TK::Native) {
            if (w == "iterator" || w == "const_iterator" || w == "reverse_iterator") {
                if (owner->nk == NK::Map || owner->nk == NK::UMap || owner->nk == NK::Set || owner->nk == NK::USet) return nativeOf(NK::MapIter, {owner});
                return nativeOf(NK::Iter, {owner});
            }
            if (w == "size_type") return tU64_;
            if (w == "value_type") return owner->args.empty() ? tDyn_ : owner->args[0];
        }
        if (owner->k == TK::Class && owner->cls) {
            auto sc = classScope_.find(owner->cls);
            if (sc != classScope_.end()) if (NsEntry* e = lookupScoped(sc->second, w)) {
                if (e->kind == NsEntry::Class) return classType(e->cls);
                if (e->kind == NsEntry::Typedef) return e->type;
            }
        }
        error("tipo " + owner->name() + "::" + w + " non supportato", t->pos);
    }
    // nomi standard sempre noti
    if (path.empty() || path == "std::") {
        if (w == "size_t") return tU64_;
        if (w == "ptrdiff_t" || w == "intptr_t" || w == "time_t" || w == "clock_t") return tI64_;
        if (w == "uintptr_t") return tU64_;
        if (w == "int8_t") return tI8_;
        if (w == "int16_t") return tI16_;
        if (w == "int32_t") return tI32_;
        if (w == "int64_t") return tI64_;
        if (w == "uint8_t") return tU8_;
        if (w == "uint16_t") return tU16_;
        if (w == "uint32_t") return tU32_;
        if (w == "uint64_t") return tU64_;
        if (w == "FILE" && (lang_ == Dialect::C || lang_ == Dialect::Cpp)) return nativeOf(NK::CFile, {});
        if (w == "wchar_t" || w == "char16_t" || w == "char32_t") error("caratteri larghi non supportati", t->pos);
    }
    if (lang_ == Dialect::CSharp) {
        // nomi del namespace System
        std::string sp = path;
        if (sp == "System::") sp.clear();
        if (sp.empty()) {
            if (w == "String") return tCsStr_;
            if (w == "Object") return tObject_;
            if (w == "Int32") return tI32_;
            if (w == "Int64") return tI64_;
            if (w == "Int16") return tI16_;
            if (w == "Byte") return tU8_;
            if (w == "Double") return tF64_;
            if (w == "Single") return tF32_;
            if (w == "Boolean") return tBool_;
            if (w == "Char") return tChar16_;
            if (w == "UInt32") return tU32_;
            if (w == "UInt64") return tU64_;
            if (w == "Decimal") error("decimal non supportato: usa double", t->pos);
            // delegati standard: Func<..., R>, Action<...>, Predicate<T>, Comparison<T>
            if (w == "Func" || w == "Action" || w == "Predicate" || w == "Comparison") {
                std::vector<TypeP> as;
                for (TypeNode* a : t->args) as.push_back(resolveType(a, false));
                if (w == "Func") {
                    if (as.empty()) error("Func vuole almeno il tipo del risultato", t->pos);
                    TypeP r = as.back();
                    as.pop_back();
                    return funcOf(r, as);
                }
                if (w == "Action") return funcOf(tVoid_, as);
                if (as.size() != 1) error(w + " vuole un argomento di tipo", t->pos);
                if (w == "Predicate") return funcOf(tBool_, as);
                return funcOf(tI32_, {as[0], as[0]});
            }
        }
    }
    // Fx.Stat1, Fx.Stat2, Fx.Reg (C#, Java): i risultati della libreria fx
    if ((lang_ == Dialect::CSharp || lang_ == Dialect::Java) && path == "Fx::") {
        static const char* const kFx[3] = {"Stat1", "Stat2", "Reg"};
        for (int k = 0; k < 3; ++k) if (w == kFx[k] && prog_.fxResult[static_cast<int>(lang_)][k]) return classType(prog_.fxResult[static_cast<int>(lang_)][k]);
    }
    // classi, typedef e template dell'utente (e classi native)
    const std::string full = path + w;
    NsEntry* e = lookupGlobalName(full, ns_, cls_);
    if (!e && lang_ == Dialect::CSharp && !path.empty()) {
        // System.Collections.Generic.List...: si toglie il namespace standard
        if (path.rfind("System::", 0) == 0) e = lookupGlobalName(w, ns_, cls_);
    }
    if (e) {
        switch (e->kind) {
        case NsEntry::Class:
            if (!t->args.empty()) error("'" + full + "' non e' un template", t->pos);
            return classType(e->cls);
        case NsEntry::Typedef: return e->type;
        case NsEntry::Template: {
            if (t->args.empty()) {
                // dentro il template stesso il nome senza argomenti e' l'istanza corrente
                if (cls_ && !cls_->typeArgs.empty() && cls_->name.rfind(w + "<", 0) == 0) return classType(cls_);
                error("'" + full + "' vuole gli argomenti del template", t->pos);
            }
            std::vector<TypeP> args;
            for (TypeNode* a : t->args) args.push_back(resolveType(a, false));
            return classType(instantiateClass(e->tmpl, args, t->pos));
        }
        default: break;
        }
    }
    // tipi nativi delle librerie
    std::size_t count = 0;
    const NativeTypeEntry* nts = nativeTypes(count);
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << static_cast<unsigned>(lang_));
    std::string want = w;
    const bool stdQual = path == "std::";
    for (std::size_t i = 0; i < count; ++i) {
        const NativeTypeEntry& nt = nts[i];
        if (!(nt.langs & mask) || want != nt.name) continue;
        if (lang_ == Dialect::Cpp) {
            // std::X, o X con using namespace std
            bool visible = stdQual;
            if (!visible) for (Namespace* ns = ns_; ns && !visible; ns = ns->parent) for (Namespace* u : ns->usings) if (u->name == "std") visible = true;
            if (!visible && !path.empty()) continue;
            if (!visible) continue;
        }
        if (nt.nk == NK::None && std::string(nt.name) == "string") {
            if (!t->args.empty()) error("string non e' un template", t->pos);
            return tCppStr_;
        }
        std::vector<TypeP> args;
        for (TypeNode* a : t->args) args.push_back(resolveType(a, false));
        if (args.size() != nt.typeArgs) {
            if (nt.nk == NK::Greater || nt.nk == NK::Less) { if (args.empty()) args.push_back(tDyn_); }
            else error(std::string(nt.name) + " vuole " + std::to_string(nt.typeArgs) + " argomenti di tipo", t->pos);
        }
        if (nt.nk == NK::None) error(std::string(nt.name) + " non supportato", t->pos);
        return nativeOf(nt.nk, args);
    }
    if (lang_ == Dialect::Cpp && (w == "unique_ptr" || w == "shared_ptr" || w == "weak_ptr")) error("smart pointer non supportati: usa new e delete", t->pos);
    if (lang_ == Dialect::Cpp && w == "function") error("std::function non supportato: usa un puntatore a funzione o auto", t->pos);
    if (lang_ == Dialect::Cpp && (w == "array" || w == "tuple" || w == "optional" || w == "variant" || w == "string_view" || w == "bitset" || w == "priority_queue" || w == "multimap" || w == "multiset"))
        error("std::" + w + " non supportato", t->pos);
    if (lang_ == Dialect::Cpp && (w == "string" || w == "vector" || w == "map") && path.empty())
        error("tipo '" + w + "' sconosciuto: scrivi std::" + w + " o using namespace std", t->pos);
    error("tipo '" + full + "' sconosciuto", t->pos);
}

// ================================================================ dichiarazioni
void Compiler::declareUnit(Unit& u, std::uint16_t unitIndex) {
    lang_ = u.dialect;
    unit_ = unitIndex;
    library_ = u.library;
    ns_ = root_.get();
    cls_ = nullptr;
    for (Node* n : u.root->list) declareIn(root_.get(), n, nullptr);
}
// Prima passata: nomi di namespace, classi, template, typedef (C/C++ in ordine).
void Compiler::declareIn(Namespace* ns, Node* n, ClassInfo* owner) {
    if (++polls_ % 256 == 0 && poll_) poll_();
    switch (n->k) {
    case K::Namespace: {
        std::string nm = syms_[n->sym];
        for (char& c : nm) if (c == '.') c = ':';
        // "A.B" di C# -> "A::B"
        std::string norm;
        for (std::size_t i = 0; i < nm.size(); ++i) { if (nm[i] == ':' && (i + 1 >= nm.size() || nm[i + 1] != ':') && (i == 0 || nm[i - 1] != ':')) norm += "::"; else norm += nm[i]; }
        Namespace* child = namespaceFor(ns, norm, true);
        Namespace* save = ns_;
        ns_ = child;
        for (Node* m : n->list) declareIn(child, m, nullptr);
        ns_ = save;
        return;
    }
    case K::Class: { declareClass(ns, n, owner); return; }
    case K::Template: {
        auto t = std::make_unique<TemplateDef>();
        t->node = n;
        t->lang = lang_;
        t->library = library_;
        t->unitIndex = unit_;
        t->owner = owner;
        Namespace* scopeNs = owner ? classScope_[owner] : ns;
        Node* d = n->a;
        if (d->k == K::Class) {
            const std::string nm = syms_[d->sym];
            t->qualified = qualify(scopeNs, nm);
            NsEntry e;
            e.kind = NsEntry::Template;
            e.tmpl = t.get();
            scopeNs->members[nm] = e;
        } else if (d->k == K::Func) {
            if (d->type2) {
                // template<class T> T Pila<T>::f(): definizione fuori dalla classe di un membro
                t->qualified = "#outofclass";
                pendingOutOfClass_.push_back({t.get(), ns});
                templates_.push_back(std::move(t));
                return;
            }
            const std::string nm = syms_[d->sym];
            t->qualified = qualify(scopeNs, nm);
            NsEntry& e = scopeNs->members[nm];
            if (e.kind != NsEntry::None && e.kind != NsEntry::Functions) error("'" + nm + "' gia' dichiarato", n->pos);
            e.kind = NsEntry::Functions;
            e.fnTemplates.push_back(t.get());
        } else error("template di questo genere non supportato", n->pos);
        templates_.push_back(std::move(t));
        return;
    }
    default:
        // il resto (funzioni, variabili, typedef) nella seconda passata
        laterDecls_.push_back({n, ns, owner, lang_, unit_, library_});
        return;
    }
}
ClassInfo* Compiler::declareClass(Namespace* ns, Node* n, ClassInfo* outer) {
    const std::string nm = syms_[n->sym];
    Namespace* scopeNs = outer ? classScope_[outer] : ns;
    const bool cTag = lang_ == Dialect::C;
    const std::string key = cTag ? std::string(n->op >= 4 ? "enum " : "struct ") + nm : nm;
    auto it = scopeNs->members.find(key);
    ClassInfo* c = nullptr;
    if (it != scopeNs->members.end() && it->second.kind == NsEntry::Class) {
        c = it->second.cls;
        if (classNodes_.count(c) && classNodes_[c] && classNodes_[c]->list.size() && n->list.size()) {
            // stessa struct da due file del progetto (intestazione inclusa due volte): va bene
            if (classNodes_[c] != n && lang_ != Dialect::CSharp) return c;
            if (lang_ == Dialect::CSharp && !(n->mods & MPartial)) error("tipo '" + nm + "' definito due volte", n->pos);
        }
    } else if (it != scopeNs->members.end() && it->second.kind != NsEntry::None) {
        error("'" + nm + "' gia' dichiarato", n->pos);
    }
    if (!c) {
        c = newClass(nm, lang_);
        c->qualified = qualify(scopeNs, nm);
        c->outer = outer;
        c->line = static_cast<int>(n->pos.line);
        c->column = n->pos.col;
        c->file = n->pos.file;
        NsEntry e;
        e.kind = NsEntry::Class;
        e.cls = c;
        scopeNs->members[key] = e;
        auto scope = std::make_unique<Namespace>();
        scope->name = c->qualified;
        scope->parent = scopeNs;
        classScope_[c] = scope.get();
        scopeNs->children.push_back(std::move(scope));
        classNs_[c] = ns;
        if (cTag && n->op < 4) {
            // struct senza typedef: in C serve "struct X"; il compilatore usa la stessa classe
        }
    }
    if (n->list.empty() && n->list2.empty() && n->op < 4 && lang_ != Dialect::CSharp) {
        if (!classNodes_.count(c)) classNodes_[c] = nullptr;    // dichiarazione anticipata
        return c;
    }
    if (classNodes_.count(c) && classNodes_[c] && lang_ == Dialect::CSharp && (n->mods & MPartial)) {
        partialParts_[c].push_back(n);
        return c;
    }
    classNodes_[c] = n;
    c->isStruct = n->op == 1;
    c->isInterface = n->op == 3;
    c->isEnum = n->op >= 4;
    c->isAbstract = (n->mods & MAbstract) != 0 || c->isInterface;
    c->isSealed = (n->mods & (MSealed | MFinal)) != 0;
    c->isStaticClass = (n->mods & MStatic) != 0;
    c->valueType = (lang_ == Dialect::C || lang_ == Dialect::Cpp) ? !c->isEnum : (c->isStruct && lang_ == Dialect::CSharp);
    classOrder_.push_back(c);
    classUnit_[c] = {lang_, unit_, library_, ns};
    // tipi annidati
    for (Node* m : n->list) {
        if (m->k == K::Class) declareClass(ns, m, c);
        else if (m->k == K::Template) declareIn(ns, m, c);
    }
    return c;
}

} // namespace fx::prog
