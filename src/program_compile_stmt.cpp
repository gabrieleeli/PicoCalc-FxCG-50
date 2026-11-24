// Compilatore dei programmi (v1.20.0), parte 3: emissione, funzioni,
// scope e variabili locali, istruzioni (if, cicli, foreach, switch, try,
// return, goto), costruzione e inizializzazione degli oggetti.
#include "program_compile.hpp"
#include <algorithm>
#include <cstring>

namespace fx::prog {
using namespace ast;

// ================================================================ emissione
std::int32_t Compiler::emit(Op op, std::int32_t a, std::uint8_t b, std::uint16_t c) {
    Instr ins;
    ins.op = op;
    ins.a = a;
    ins.b = b;
    ins.c = c;
    fn_->code.push_back(ins);
    SrcPos sp;
    sp.file = pos_.file;
    sp.line = pos_.line;
    sp.column = pos_.col;
    fn_->pos.push_back(sp);
    fn_->stmt.push_back(stmtStart_ ? 1 : 0);
    stmtStart_ = false;
    if (fn_->code.size() > 400000) error("programma troppo lungo (oltre 400 000 istruzioni in una funzione)", pos_);
    return static_cast<std::int32_t>(fn_->code.size() - 1);
}
std::int32_t Compiler::here() const { return static_cast<std::int32_t>(fn_->code.size()); }
void Compiler::patch(std::int32_t at, std::int32_t target) { fn_->code[static_cast<std::size_t>(at)].a = target; }
void Compiler::markStatement() { stmtStart_ = true; }
std::int32_t Compiler::constIndex(std::int64_t v) {
    for (std::size_t i = 0; i < prog_.ints.size(); ++i) if (prog_.ints[i] == v) return static_cast<std::int32_t>(i);
    prog_.ints.push_back(v);
    return static_cast<std::int32_t>(prog_.ints.size() - 1);
}
std::int32_t Compiler::constDouble(double v) {
    for (std::size_t i = 0; i < prog_.doubles.size(); ++i) if (std::memcmp(&prog_.doubles[i], &v, sizeof v) == 0) return static_cast<std::int32_t>(i);
    prog_.doubles.push_back(v);
    return static_cast<std::int32_t>(prog_.doubles.size() - 1);
}
namespace {
std::u16string toU16(const std::string& s) {
    std::u16string out;
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char b = static_cast<unsigned char>(s[i]);
        unsigned cp = b;
        std::size_t n = 1;
        if (b >= 0xF0 && i + 3 < s.size() + 0 && i + 3 <= s.size() - 1 + 1) { cp = ((b & 7u) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3Fu); n = 4; }
        else if (b >= 0xE0 && i + 2 < s.size()) { cp = ((b & 0xFu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 2]) & 0x3Fu); n = 3; }
        else if (b >= 0xC0 && i + 1 < s.size()) { cp = ((b & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu); n = 2; }
        if (cp >= 0x10000) { cp -= 0x10000; out += static_cast<char16_t>(0xD800 + (cp >> 10)); out += static_cast<char16_t>(0xDC00 + (cp & 0x3FF)); }
        else out += static_cast<char16_t>(cp);
        i += n;
    }
    return out;
}
} // namespace
std::int32_t Compiler::constString(const std::string& bytes, bool wide) {
    for (std::size_t i = 0; i < prog_.strings.size(); ++i) if (prog_.strings[i].bytes == bytes) {
        if (wide && prog_.strings[i].wide.empty() && !bytes.empty()) prog_.strings[i].wide = toU16(bytes);
        return static_cast<std::int32_t>(i);
    }
    StrConst sc;
    sc.bytes = bytes;
    if (wide) sc.wide = toU16(bytes);
    prog_.strings.push_back(std::move(sc));
    return static_cast<std::int32_t>(prog_.strings.size() - 1);
}
std::int32_t Compiler::selector(const std::string& n, std::size_t arity) {
    const std::string key = n + "/" + std::to_string(arity);
    for (std::size_t i = 0; i < prog_.selectors.size(); ++i) if (prog_.selectors[i] == key) return static_cast<std::int32_t>(i);
    prog_.selectors.push_back(key);
    return static_cast<std::int32_t>(prog_.selectors.size() - 1);
}
void Compiler::pushInt(std::int64_t v, VK k) {
    if (v >= INT32_MIN && v <= INT32_MAX) emit(Op::PushI, static_cast<std::int32_t>(v), static_cast<std::uint8_t>(k));
    else emit(Op::PushL, constIndex(v), static_cast<std::uint8_t>(k));
}
void Compiler::pushDouble(double d, VK k) { emit(Op::PushD, constDouble(d), static_cast<std::uint8_t>(k)); }
void Compiler::pushString(const std::string& s, Dialect d, bool cppValue) {
    const bool wide = d == Dialect::CSharp || d == Dialect::Java;
    emit(Op::PushS, constString(s, wide), static_cast<std::uint8_t>(wide ? 2 : cppValue ? 1 : 0));
}

// ================================================================ funzioni
void Compiler::beginFunction(Function* f) {
    fn_ = f;
    probeCache_.clear();                           // i tipi di prova dipendono dalla funzione (istanze dei template)
    scopes_.clear();
    loops_.clear();
    finallies_.clear();
    labels_.clear();
    gotos_.clear();
    freeSlots_.clear();
    retSlot_ = -1;
    f->code.clear();
    f->pos.clear();
    f->stmt.clear();
    f->handlers.clear();
    f->locals.clear();
    f->nlocals = (f->hasThis ? 1 : 0) + static_cast<std::int32_t>(f->params.size());
    pushScope();
}
void Compiler::endFunction() {
    while (!scopes_.empty()) popScope(false);
    for (const auto& g : gotos_) {
        const auto it = labels_.find(g.second.first);
        if (it == labels_.end()) {
            const Pos at = g.second.second;
            throw Error("Go ERROR: etichetta '" + g.second.first + "' non trovata", static_cast<int>(at.line), at.col,
                        at.file < prog_.files.size() ? prog_.files[at.file] : std::string());
        }
        patch(g.first, it->second);
    }
    fn_ = nullptr;
}
void Compiler::pushScope(bool transparent) {
    BlockScope s;
    s.transparent = transparent;
    scopes_.push_back(s);
}
LocalVar* Compiler::findLocal(const std::string& nm, std::size_t* depth) {
    for (std::size_t i = scopes_.size(); i-- > 0;) {
        for (auto it = scopes_[i].vars.rbegin(); it != scopes_[i].vars.rend(); ++it) {
            if (it->name == nm) { if (depth) *depth = i; return &*it; }
        }
    }
    return nullptr;
}
LocalVar& Compiler::addLocal(const std::string& nm, TypeP t, Pos at, bool boxed) {
    if (!nm.empty()) {
        for (const LocalVar& v : scopes_.back().vars) if (v.name == nm) error("'" + nm + "' gia' dichiarata in questo blocco", at);
        if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) {
            std::size_t d = 0;
            if (findLocal(nm, &d)) error("'" + nm + "' e' gia' usata in un blocco che contiene questo", at);
        }
    }
    LocalVar v;
    v.name = nm;
    v.type = t;
    if (!freeSlots_.empty()) { v.slot = freeSlots_.back(); freeSlots_.pop_back(); }
    else v.slot = fn_->nlocals++;
    if (fn_->nlocals > 4000) error("troppe variabili locali in una funzione", at);
    v.boxed = boxed;
    LocalInfo li;
    li.name = nm;
    li.type = t;
    li.slot = v.slot;
    li.from = here();
    li.to = -1;
    li.boxed = boxed;
    v.infoIndex = static_cast<std::int32_t>(fn_->locals.size());
    fn_->locals.push_back(li);
    scopes_.back().vars.push_back(v);
    return scopes_.back().vars.back();
}
void Compiler::emitScopeCleanup(BlockScope& s, const LocalVar* except) {
    for (auto it = s.vars.rbegin(); it != s.vars.rend(); ++it) {
        if (&*it == except || it->moved) continue;
        if (it->needsDtor) { emit(Op::LoadL, it->slot); emit(Op::Destroy, -1); }
        if (it->needsKill) emit(Op::KillL, it->slot);
    }
}
void Compiler::popScope(bool emitCleanup) {
    BlockScope s = std::move(scopes_.back());
    scopes_.pop_back();
    if (emitCleanup) emitScopeCleanup(s);
    // C++: distruttori anche all'uscita per eccezione (gestori di pulizia)
    std::vector<const LocalVar*> dtors;
    for (const LocalVar& v : s.vars) if (v.needsDtor && !v.moved && fn_) dtors.push_back(&v);
    if (!dtors.empty() && emitCleanup && s.cleanupStart >= 0) {
        const std::int32_t skip = emit(Op::Jmp, 0);
        std::vector<std::int32_t> starts(dtors.size());
        std::vector<std::int32_t> ranges(dtors.size());
        const std::int32_t excSlot = fn_->nlocals++;
        for (std::size_t k = dtors.size(); k-- > 0;) {
            starts[k] = here();
            emit(Op::LoadL, dtors[k]->slot);
            emit(Op::Destroy, -1);
            if (dtors[k]->needsKill) emit(Op::KillL, dtors[k]->slot);
            if (k == 0) emit(Op::Rethrow, excSlot);
        }
        // un gestore per variabile, dalla piu' interna (costruita per ultima)
        for (std::size_t k = dtors.size(); k-- > 0;) {
            Handler h;
            h.start = s.cleanupStart;
            h.end = starts[k];
            h.target = starts[k];
            h.slot = excSlot;
            h.cleanup = true;
            fn_->handlers.push_back(h);
        }
        // i blocchi intermedi saltano al successivo: ogni blocco finisce nel seguente
        patch(skip, here());
    }
    for (const LocalVar& v : s.vars) {
        if (v.infoIndex >= 0 && fn_) fn_->locals[static_cast<std::size_t>(v.infoIndex)].to = here();
        if (fn_ && v.slot >= (fn_->hasThis ? 1 : 0) + static_cast<std::int32_t>(fn_->params.size()) && !v.boxed) freeSlots_.push_back(v.slot);
    }
}
// Uscita da scope annidati (break, continue, return): pulizie e finally.
void Compiler::emitExitTo(std::size_t depth, std::int32_t finallyDepth) {
    for (std::size_t i = scopes_.size(); i-- > depth;) emitScopeCleanup(scopes_[i]);
    for (std::size_t f = finallies_.size(); f-- > static_cast<std::size_t>(finallyDepth);) {
        // copia del corpo del finally (senza il gestore attivo)
        auto save = finallies_;
        finallies_.resize(f);
        FinallyCtx fc = save[f];
        block(fc.block);
        finallies_ = save;
    }
}

void Compiler::compileFunctionBody(Function* f, Node* body, Node* decl) {
    beginFunction(f);
    setPos(decl ? decl->pos : body->pos);
    std::int32_t slot = f->hasThis ? 1 : 0;
    // nomi catturati dalle lambda di C# (le variabili vanno in una Box)
    csCaptured_.clear();
    if ((lang_ == Dialect::CSharp || lang_ == Dialect::Java) && body) collectCaptures(body);
    for (std::size_t i = 0; i < f->params.size(); ++i, ++slot) {
        LocalVar v;
        v.name = f->params[i].name;
        v.type = f->params[i].type;
        v.slot = slot;
        v.isRef = f->params[i].type && f->params[i].type->k == TK::Ref;
        if (v.isRef) v.type = f->params[i].type->elem;
        LocalInfo li;
        li.name = v.name;
        li.type = f->params[i].type;
        li.slot = slot;
        li.from = 0;
        li.to = -1;
        v.infoIndex = static_cast<std::int32_t>(f->locals.size());
        f->locals.push_back(li);
        if (!v.isRef && csCaptured_.count(v.name)) {
            v.boxed = true;
            emit(Op::NewBox, slot);
        }
        // C/C++: parametri per valore di tipo classe: la copia e' gia' fatta dal chiamante
        scopes_.back().vars.push_back(v);
    }
    if (f->isCtor && f->owner) emitCtorPrologue(f, decl);
    markStatement();
    if (body) {
        if (body->k == K::Block) for (Node* s : body->list) statement(s);
        else statement(body);
    }
    // ritorno implicito
    setPos(body ? Pos{body->pos.file, body->pos.col, lastLine(body)} : pos_);
    emitScopeCleanup(scopes_.back());
    if (f->ret->k == TK::Void || f->isCtor || f->isDtor) emit(Op::Ret, 0, 0);
    else if (f->index == prog_.entry || f->simple == "main") { pushInt(0, vkOf(f->ret)); emit(Op::Ret, 0, 1); }
    else if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) {
        emit(Op::Fault, static_cast<std::int32_t>(FaultKind::InvalidOp), 0, static_cast<std::uint16_t>(constString("la funzione " + f->name + " finisce senza return", false)));
    } else {
        zeroValue(f->ret, pos_);
        emit(Op::Ret, 0, 1);
    }
    endFunction();
}
std::uint32_t Compiler::lastLine(Node* n) const {
    std::uint32_t l = n->pos.line;
    if (!n->list.empty()) l = std::max(l, lastLine(n->list.back()));
    return l;
}

// C++: lista di inizializzazione (base, membri), membri con valore predefinito.
// C#: inizializzatori dei campi, poi : base(...) / : this(...).
void Compiler::emitCtorPrologue(Function* f, Node* decl) {
    ClassInfo* c = f->owner;
    Node* baseInit = nullptr;
    Node* thisInit = nullptr;
    std::map<std::string, Node*> memberInits;
    if (decl) for (Node* mi : decl->list2) {
        if (mi->k != K::MemberInit) continue;
        const std::string nm = mi->type ? syms_[mi->type->name] : syms_[mi->sym];
        if (nm == "base" || (c->base && (nm == c->base->name || (mi->type && resolveType(mi->type, false)->cls == c->base)) )) baseInit = mi;
        else if (nm == "this" || nm == c->name) thisInit = mi;
        else {
            bool isField = false;
            for (const FieldInfo& fi : c->fields) if (fi.name == nm && !fi.isStatic) isField = true;
            if (!isField) error("'" + nm + "' non e' un membro di " + c->name, mi->pos);
            memberInits[nm] = mi;
        }
    }
    auto callCtor = [&](ClassInfo* target, Node* mi) {
        std::vector<Node*> args = mi ? mi->list : std::vector<Node*>{};
        if (mi && mi->a && mi->a->k == K::InitList) args = mi->a->list;
        if (target->ctors.empty()) {
            if (!args.empty()) error("la classe " + target->name + " non ha costruttori con argomenti", mi ? mi->pos : pos_);
            if (target->fieldInit) { emit(Op::LoadL, 0); emit(Op::Call, target->fieldInit->index, 1); }
            return;
        }
        emit(Op::LoadL, 0);
        callOverloads(target->ctors, {}, {}, args, {}, mi ? mi->pos : pos_, true, classType(target), true, target->name);
    };
    if (lang_ == Dialect::CSharp) {
        if (!thisInit) emitFieldInits(c, decl);
        if (thisInit) callCtor(c, thisInit);
        else if (c->base && !c->base->isNative) callCtor(c->base, baseInit);
        else if (c->base && c->base->isNative && baseInit) nativeBaseCtor(c->base, baseInit);
        return;
    }
    if (lang_ == Dialect::Java) {
        if (thisInit) { callCtor(c, thisInit); return; }
        if (c->base && !c->base->isNative) callCtor(c->base, baseInit);
        else if (c->base && c->base->isNative && baseInit) nativeBaseCtor(c->base, baseInit);
        emitFieldInits(c, decl);
        return;
    }
    // C++
    if (thisInit) { callCtor(c, thisInit); return; }
    if (c->base) {
        if (c->base->isNative) { if (baseInit) nativeBaseCtor(c->base, baseInit); }
        else callCtor(c->base, baseInit);
    }
    for (const FieldInfo& fi : c->fields) {
        if (fi.isStatic) continue;
        auto it = memberInits.find(fi.name);
        Node* init = nullptr;
        if (it != memberInits.end()) init = it->second;
        if (init) {
            setPos(init->pos);
            if (fi.type->k == TK::Class && fi.type->cls && fi.type->cls->valueType && !fi.type->cls->isEnum) {
                completeClass(fi.type->cls);
                std::vector<Node*> args = init->list;
                if (init->a && init->a->k == K::InitList) args = init->a->list;
                // un solo argomento dello stesso tipo: copia
                if (args.size() == 1) {
                    EV probe = exprType(args[0]);
                    if (stripRef(probe.type) == fi.type && fi.type->cls->ctors.empty()) {
                        emit(Op::LoadL, 0); emit(Op::GetF, fi.slot);
                        rvalue(args[0], fi.type);
                        emit(Op::CopyInto, fi.type->cls->id);
                        continue;
                    }
                }
                if (!fi.type->cls->ctors.empty()) {
                    emit(Op::LoadL, 0); emit(Op::GetF, fi.slot);
                    callOverloads(fi.type->cls->ctors, {}, {}, args, {}, init->pos, true, fi.type, true, fi.type->cls->name);
                } else if (!args.empty()) {
                    error("la classe " + fi.type->cls->name + " non ha costruttori con argomenti", init->pos);
                }
                continue;
            }
            emit(Op::LoadL, 0);
            if (init->a && init->a->k == K::InitList) initFromList(fi.type, init->a, init->pos);
            else if (init->list.size() == 1) rvalue(init->list[0], fi.type);
            else if (init->list.empty()) zeroValue(fi.type, init->pos);
            else {
                // vector<int> v(5, 0) e simili: costruttore del contenitore
                constructNative(fi.type, init->list, init->pos);
            }
            emit(Op::SetF, fi.slot);
            continue;
        }
        // valore predefinito del membro (int x = 5; nella classe)
        Node* nsdmi = nullptr;
        for (Node* d : fieldInits_[c]) if (syms_[d->sym] == fi.name) nsdmi = d;
        if (nsdmi) {
            setPos(nsdmi->pos);
            emit(Op::LoadL, 0);
            if (nsdmi->a && nsdmi->a->k == K::InitList) initFromList(fi.type, nsdmi->a, nsdmi->pos);
            else if (nsdmi->a) rvalue(nsdmi->a, fi.type);
            else constructNative(fi.type, nsdmi->list, nsdmi->pos);
            emit(Op::SetF, fi.slot);
            continue;
        }
        // membro di classe con costruttore predefinito dell'utente
        if (fi.type->k == TK::Class && fi.type->cls && fi.type->cls->valueType && !fi.type->cls->isEnum) {
            completeClass(fi.type->cls);
            if (!fi.type->cls->ctors.empty()) {
                emit(Op::LoadL, 0); emit(Op::GetF, fi.slot);
                callOverloads(fi.type->cls->ctors, {}, {}, {}, {}, pos_, true, fi.type, true, fi.type->cls->name);
            }
        }
    }
}
void Compiler::emitFieldInits(ClassInfo* c, Node* at) {
    for (Node* d : fieldInits_[c]) {
        const FieldInfo* fi = nullptr;
        for (const FieldInfo& f : c->fields) if (f.name == syms_[d->sym] && !f.isStatic) fi = &f;
        if (!fi) continue;
        setPos(d->pos);
        emit(Op::LoadL, 0);
        if (d->a && d->a->k == K::InitList) initFromList(fi->type, d->a, d->pos);
        else if (d->a) rvalue(d->a, fi->type);
        else constructNative(fi->type, d->list, d->pos);
        emit(Op::SetF, fi->slot);
    }
    (void)at;
}
// Funzione degli inizializzatori dei campi (classi senza costruttori dell'utente).
void Compiler::ensureFieldInit(ClassInfo* c) {
    if (c->fieldInit || c->isNative) return;
    bool any = !fieldInits_[c].empty();
    if (!any) return;
    Function* f = newFunction("<fields>", c->lang, c);
    f->isStatic = false;
    f->hasThis = true;
    f->defined = true;
    c->fieldInit = f;
    // si compila subito con il contesto della classe
    Function* saveFn = fn_;
    auto saveScopes = std::move(scopes_);
    auto saveLoops = std::move(loops_);
    auto saveFin = std::move(finallies_);
    auto saveLabels = labels_;
    auto saveGotos = gotos_;
    auto saveFree = freeSlots_;
    const Pos savePos = pos_;
    const Dialect saveLang = lang_;
    Namespace* saveNs = ns_;
    ClassInfo* saveCls = cls_;
    const UnitCtx uc = classUnit_[c];
    lang_ = uc.lang;
    ns_ = uc.ns;
    cls_ = c;
    beginFunction(f);
    if (lang_ == Dialect::Cpp) {
        for (const FieldInfo& fi : c->fields) {
            if (fi.isStatic) continue;
            for (Node* d : fieldInits_[c]) if (syms_[d->sym] == fi.name) {
                setPos(d->pos);
                emit(Op::LoadL, 0);
                if (d->a && d->a->k == K::InitList) initFromList(fi.type, d->a, d->pos);
                else if (d->a) rvalue(d->a, fi.type);
                else constructNative(fi.type, d->list, d->pos);
                emit(Op::SetF, fi.slot);
            }
        }
    } else emitFieldInits(c, nullptr);
    emit(Op::Ret, 0, 0);
    endFunction();
    fn_ = saveFn;
    scopes_ = std::move(saveScopes);
    loops_ = std::move(saveLoops);
    finallies_ = std::move(saveFin);
    labels_ = saveLabels;
    gotos_ = saveGotos;
    freeSlots_ = saveFree;
    pos_ = savePos;
    lang_ = saveLang;
    ns_ = saveNs;
    cls_ = saveCls;
}
void Compiler::nativeBaseCtor(ClassInfo* base, Node* mi) {
    // eccezioni standard: Exception(message[, inner]) / std::runtime_error(what)
    std::vector<Node*> args = mi->list;
    if (mi->a && mi->a->k == K::InitList) args = mi->a->list;
    if (args.size() > 2) error("troppi argomenti per " + base->name, mi->pos);
    if (!args.empty()) {
        emit(Op::LoadL, 0);
        rvalue(args[0], base->lang == Dialect::Cpp ? tCppStr_ : tCsStr_);
        emit(Op::SetF, 0);
    }
    if (args.size() == 2) {
        emit(Op::LoadL, 0);
        rvalue(args[1], tObject_);
        emit(Op::SetF, 1);
    }
}

// ================================================================ istruzioni
void Compiler::block(Node* n, bool newScope) {
    if (!n) return;
    if (n->k != K::Block) { statement(n); return; }
    if (newScope) pushScope();
    for (Node* s : n->list) statement(s);
    if (newScope) popScope();
}

void Compiler::statement(Node* n) {
    if (!n) return;
    if (++depth_ > ParseDepth) error("Stack ERROR: annidamento troppo profondo", n->pos);
    struct D { int& d; ~D() { --d; } } guard{depth_};
    if (++polls_ % 512 == 0 && poll_) poll_();
    setPos(n->pos);
    if (n->k != K::Block && n->k != K::Case && n->k != K::Label && n->k != K::Empty) markStatement();
    switch (n->k) {
    case K::Empty: return;
    case K::Block:
        if (n->op == 1) { for (Node* s : n->list) statement(s); return; }   // blocco trasparente (piu' dichiarazioni)
        block(n);
        return;
    case K::ExprStmt: discard(n->a); return;
    case K::VarDecl: declareLocalVars(n); return;
    case K::If: ifStmt(n); return;
    case K::While: case K::DoWhile: case K::For: loopStmt(n); return;
    case K::ForEach: forEachStmt(n); return;
    case K::Switch: switchStmt(n); return;
    case K::Case: {
        if (switchCases_.empty()) error("case fuori da uno switch", n->pos);
        SwitchBuild& sb = *switchCases_.back();
        if (!n->a) {
            if (sb.defaultTarget >= 0) error("due default nello stesso switch", n->pos);
            sb.defaultTarget = here();
        } else if (sb.strings) {
            if (n->a->k != K::StrLit) error("case di uno switch su stringhe: serve una stringa costante", n->a->pos);
            const std::string& s = units_[unit_].strings[n->a->str];
            for (const auto& c : sb.strCases) if (c.first == s) error("case \"" + s + "\" ripetuto", n->pos);
            sb.strCases.push_back({s, here()});
        } else {
            std::int64_t v = 0;
            if (!constInt(n->a, v)) error("il valore di un case deve essere una costante intera", n->a->pos);
            for (const auto& c : sb.cases) if (c.first == v) error("case " + std::to_string(v) + " ripetuto", n->pos);
            sb.cases.push_back({v, here()});
        }
        return;
    }
    case K::Break: {
        if (loops_.empty()) error("break fuori da un ciclo o da uno switch", n->pos);
        std::size_t li = loops_.size() - 1;
        if (n->sym) { while (true) { if (loops_[li].label == n->sym) break; if (li == 0) error("etichetta del break non trovata", n->pos); --li; } }
        emitExitTo(loops_[li].scopeDepth, loops_[li].finallyDepth);
        loops_[li].breaks.push_back(emit(Op::Jmp, 0));
        return;
    }
    case K::Continue: {
        std::size_t li = loops_.size();
        while (li > 0 && loops_[li - 1].isSwitch) --li;
        if (li == 0) error("continue fuori da un ciclo", n->pos);
        --li;
        if (n->sym) { while (true) { if (loops_[li].label == n->sym && !loops_[li].isSwitch) break; if (li == 0) error("etichetta del continue non trovata", n->pos); --li; } }
        emitExitTo(loops_[li].scopeDepth, loops_[li].finallyDepth);
        loops_[li].continues.push_back(emit(Op::Jmp, 0));
        return;
    }
    case K::Return: returnStmt(n); return;
    case K::Goto: {
        if (!finallies_.empty()) error("goto fuori da un try/finally non supportato", n->pos);
        gotos_.push_back({emit(Op::Jmp, 0), {syms_[n->sym], n->pos}});
        return;
    }
    case K::Label: {
        const std::string nm = syms_[n->sym];
        if (labels_.count(nm)) error("etichetta '" + nm + "' ripetuta", n->pos);
        labels_[nm] = here();
        if (n->a && (n->a->k == K::While || n->a->k == K::DoWhile || n->a->k == K::For || n->a->k == K::ForEach)) {
            pendingLabel_ = n->sym;
        }
        statement(n->a);
        return;
    }
    case K::Try: tryStmt(n); return;
    case K::Throw: {
        if (!n->a) {
            // throw; dentro un catch: rilancia l'eccezione catturata
            if (catchSlots_.empty()) error("throw; fuori da un catch", n->pos);
            emit(Op::Rethrow, catchSlots_.back());
            return;
        }
        EV e = expr(n->a);
        load(e, n->pos);
        TypeP t = stripRef(e.type);
        if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) {
            if (!(t->k == TK::Class && t->cls && t->cls->isException) && t->k != TK::Null && t != tObject_)
                error("throw vuole un'eccezione (una classe derivata da Exception)", n->a->pos);
        } else if (t->k == TK::Class && t->cls && t->cls->valueType) {
            copyIfLvalue(e, t, n->pos);
        } else if (t->k == TK::Array && t->elem && t->elem->k == TK::C8) {
            convert(t, ptrTo(tChar8_), n->pos);
        }
        emit(Op::Throw);
        return;
    }
    case K::UsingStmt: usingStmt(n); return;
    case K::Class: {
        // struct/class/enum locale (C/C++): come una dichiarazione del namespace
        ClassInfo* c = declareClass(ns_, n, nullptr);
        classUnit_[c] = {lang_, unit_, library_, ns_};
        completeClass(c);
        return;
    }
    case K::Typedef: {
        TypeP t = resolveType(n->type, true);
        NsEntry e;
        e.kind = NsEntry::Typedef;
        e.type = t;
        localTypedefs_[ns_].push_back(syms_[n->sym]);
        ns_->members[syms_[n->sym]] = e;
        return;
    }
    case K::Using: processLater({n, ns_, nullptr, lang_, unit_, library_}); return;
    case K::Binding: bindingDecl(n); return;
    case K::Func: error("funzioni annidate non supportate", n->pos);
    case K::Template: error("template locali non supportati", n->pos);
    default: break;
    }
    error("istruzione non valida", n->pos);
}

void Compiler::ifStmt(Node* n) {
    pushScope();
    if (n->d) statement(n->d);
    setPos(n->a->pos);
    condition(n->a);
    const std::int32_t jz = emit(Op::Jz, 0);
    statement(n->b);
    if (n->c) {
        const std::int32_t jmp = emit(Op::Jmp, 0);
        patch(jz, here());
        statement(n->c);
        patch(jmp, here());
    } else patch(jz, here());
    popScope();
}

void Compiler::loopStmt(Node* n) {
    LoopCtx lc;
    lc.label = pendingLabel_;
    pendingLabel_ = 0;
    pushScope();
    lc.scopeDepth = scopes_.size();
    lc.finallyDepth = static_cast<std::int32_t>(finallies_.size());
    std::int32_t continueTarget = -1;
    if (n->k == K::While) {
        const std::int32_t start = here();
        setPos(n->a->pos);
        condition(n->a);
        const std::int32_t jz = emit(Op::Jz, 0);
        loops_.push_back(lc);
        statement(n->b);
        lc = loops_.back();
        loops_.pop_back();
        emit(Op::Jmp, start);
        patch(jz, here());
        continueTarget = start;
    } else if (n->k == K::DoWhile) {
        const std::int32_t start = here();
        loops_.push_back(lc);
        statement(n->b);
        lc = loops_.back();
        loops_.pop_back();
        continueTarget = here();
        setPos(n->a->pos);
        markStatement();
        condition(n->a);
        emit(Op::Jnz, start);
    } else {
        if (n->a) statement(n->a);
        const std::int32_t start = here();
        std::int32_t jz = -1;
        if (n->b) { setPos(n->b->pos); condition(n->b); jz = emit(Op::Jz, 0); }
        loops_.push_back(lc);
        pushScope();
        statement(n->d);
        popScope();
        lc = loops_.back();
        loops_.pop_back();
        continueTarget = here();
        if (n->c) { setPos(n->c->pos); markStatement(); discard(n->c); }
        emit(Op::Jmp, start);
        if (jz >= 0) patch(jz, here());
    }
    for (std::int32_t b : lc.breaks) patch(b, here());
    for (std::int32_t c : lc.continues) patch(c, continueTarget);
    popScope();
}

void Compiler::switchStmt(Node* n) {
    setPos(n->a->pos);
    EV e = expr(n->a);
    load(e, n->a->pos);
    TypeP t = stripRef(e.type);
    SwitchBuild sb;
    sb.strings = t->k == TK::Str;
    if (!sb.strings && !isIntegral(t)) error("switch su un valore non intero", n->a->pos);
    if (sb.strings && (lang_ == Dialect::C || lang_ == Dialect::Cpp)) error("switch su una stringa non e' ammesso in C/C++", n->a->pos);
    if (isIntegral(t) && t->k != TK::I64 && t->k != TK::U64) convert(t, tI64_, n->a->pos, true);
    const std::int32_t sw = emit(Op::Switch, 0);
    LoopCtx lc;
    lc.isSwitch = true;
    pushScope();
    lc.scopeDepth = scopes_.size();
    lc.finallyDepth = static_cast<std::int32_t>(finallies_.size());
    loops_.push_back(lc);
    switchCases_.push_back(&sb);
    if (n->op == 1) {
        // C#: nessun passaggio implicito da un case al successivo
        bool sectionOpen = false;
        Node* last = nullptr;
        for (Node* s : n->b->list) {
            if (s->k == K::Case) {
                if (sectionOpen && last && !jumpStatement(last)) error("il controllo non puo' passare da un case all'altro: manca break", s->pos);
                sectionOpen = false;
                last = nullptr;
            } else { sectionOpen = true; last = s; }
            statement(s);
        }
        if (sectionOpen && last && !jumpStatement(last)) error("l'ultimo case dello switch deve finire con break", last->pos);
    } else {
        block(n->b, false);
    }
    switchCases_.pop_back();
    lc = loops_.back();
    loops_.pop_back();
    popScope();
    const std::int32_t end = here();
    SwitchTable tab;
    tab.strings = sb.strings;
    for (auto& c : sb.cases) tab.cases.push_back(c);
    std::sort(tab.cases.begin(), tab.cases.end());
    for (auto& c : sb.strCases) {
        std::u16string w;
        for (std::size_t i = 0; i < c.first.size(); ++i) w += static_cast<char16_t>(static_cast<unsigned char>(c.first[i]));
        const std::int32_t idx = constString(c.first, true);
        tab.strCases.push_back({prog_.strings[static_cast<std::size_t>(idx)].wide, c.second});
    }
    tab.fallback = sb.defaultTarget >= 0 ? sb.defaultTarget : end;
    prog_.switches.push_back(std::move(tab));
    patch(sw, static_cast<std::int32_t>(prog_.switches.size() - 1));
    for (std::int32_t b : lc.breaks) patch(b, end);
}
bool Compiler::jumpStatement(Node* s) const {
    if (!s) return false;
    switch (s->k) {
    case K::Break: case K::Return: case K::Throw: case K::Continue: case K::Goto: return true;
    case K::Block: return !s->list.empty() && jumpStatement(s->list.back());
    case K::If: return s->c && jumpStatement(s->b) && jumpStatement(s->c);
    case K::ExprStmt: return s->a && s->a->k == K::ThrowExpr;
    default: return false;
    }
}

void Compiler::returnStmt(Node* n) {
    Function* f = fn_;
    if (lambdaReturnType_ && !n->a) { /* lambda void */ }
    TypeP rt = f->ret;
    if (rt->k == TK::Dynamic && rt->size == -2) {
        // auto: il tipo dal primo return
        if (n->a) { EV probe = exprType(n->a); rt = stripRef(probe.type); if (rt->k == TK::Dynamic) rt = tDyn_; }
        else rt = tVoid_;
        f->ret = rt;
    }
    const bool cleanups = hasCleanups();
    if (!n->a) {
        if (rt->k != TK::Void && !f->isCtor && n->op != 1) error("return senza valore in una funzione che restituisce " + typeName(rt), n->pos);
        emitExitTo(1, 0);
        emitScopeCleanup(scopes_[0]);
        emit(Op::Ret, 0, 0);
        return;
    }
    if (rt->k == TK::Void) {
        if (n->op == 1) { discard(n->a); emitExitTo(1, 0); emitScopeCleanup(scopes_[0]); emit(Op::Ret, 0, 0); return; }   // corpo d'espressione void
        error("return con un valore in una funzione void", n->a->pos);
    }
    const LocalVar* moved = nullptr;
    if (rt->k == TK::Ref) {
        // ritorno per riferimento: l'indirizzo del luogo
        EV a = addressOf(n->a);
        (void)a;
    } else if (n->a->k == K::InitList) {
        initFromList(rt, n->a, n->a->pos);
    } else if (n->a->k == K::Lambda) {
        // return x => ...: i tipi dei parametri dal tipo del risultato (Func<int, Func<int, int>>)
        rvalue(n->a, rt);
    } else {
        EV e = expr(n->a);
        // restituire una variabile locale per valore: niente copia, niente distruttore
        if (e.cat == EV::Local && e.local && isValueType(rt) && !e.local->isRef && stripRef(e.type) == rt) {
            moved = e.local;
            e.local->moved = true;
            load(e, n->a->pos);
        } else {
            load(e, n->a->pos);
            convert(e.type, rt, n->a->pos, false, &e);
            copyIfLvalue(e, rt, n->a->pos);
        }
    }
    if (cleanups) {
        if (retSlot_ < 0) retSlot_ = fn_->nlocals++;
        emit(Op::StoreL, retSlot_);
        emitExitTo(1, 0);
        for (auto it = scopes_[0].vars.rbegin(); it != scopes_[0].vars.rend(); ++it) {
            if (&*it == moved || it->moved) continue;
            if (it->needsDtor) { emit(Op::LoadL, it->slot); emit(Op::Destroy, -1); }
            if (it->needsKill) emit(Op::KillL, it->slot);
        }
        emit(Op::LoadL, retSlot_);
    }
    if (moved) const_cast<LocalVar*>(moved)->moved = false;   // altri return della funzione possono usarla
    emit(Op::Ret, 0, 1);
}
bool Compiler::hasCleanups() const {
    if (!finallies_.empty()) return true;
    for (const BlockScope& s : scopes_) for (const LocalVar& v : s.vars) if (v.needsDtor || v.needsKill) return true;
    return false;
}

void Compiler::tryStmt(Node* n) {
    if (lang_ == Dialect::C) error("try non esiste in C", n->pos);
    const std::int32_t tryStart = here();
    const std::size_t handlerBase = fn_->handlers.size();
    if (n->b) { FinallyCtx fc; fc.block = n->b; fc.scopeDepth = scopes_.size(); finallies_.push_back(fc); }
    block(n->a);
    const std::int32_t tryEnd = here();
    std::vector<std::int32_t> exits;
    exits.push_back(emit(Op::Jmp, 0));
    std::vector<Handler> catches;
    bool filtered = false;
    for (Node* c : n->list) if (c->b) filtered = true;
    if (filtered && (lang_ == Dialect::CSharp || lang_ == Dialect::Java)) {
        // catch ... when (filtro): un solo gestore che prova le clausole in ordine;
        // se nessuna accetta, l'eccezione prosegue (rilancio con la pila originale)
        Handler h;
        h.start = tryStart;
        h.end = tryEnd;
        h.target = here();
        h.catches = prog_.exceptionRoot[static_cast<int>(lang_)];
        pushScope();
        LocalVar& tmp = addLocal("", tObject_, n->pos);
        const std::int32_t tmpSlot = tmp.slot;
        h.slot = tmpSlot;
        catches.push_back(h);
        for (Node* c : n->list) {
            std::vector<std::int32_t> skip;
            pushScope();
            TypeP ct = tObject_;
            if (c->type) {
                ct = stripRef(resolveType(c->type, false));
                if (ct->k != TK::Class || !ct->cls || !ct->cls->isException) error("catch vuole una classe di eccezione", c->pos);
                completeClass(ct->cls);
                emit(Op::LoadL, tmpSlot);
                emit(Op::InstOf, ct->cls->id);
                skip.push_back(emit(Op::Jz, 0));
            }
            if (c->sym) {
                LocalVar& v = addLocal(syms_[c->sym], ct, c->pos);
                emit(Op::LoadL, tmpSlot);
                emit(Op::StoreL, v.slot);
            }
            if (c->b) {
                setPos(c->b->pos);
                condition(c->b);
                skip.push_back(emit(Op::Jz, 0));
            }
            catchSlots_.push_back(tmpSlot);
            block(c->a);
            catchSlots_.pop_back();
            popScope();
            exits.push_back(emit(Op::Jmp, 0));
            for (std::int32_t x : skip) patch(x, here());
        }
        emit(Op::Rethrow, tmpSlot);
        popScope();
    } else
    for (Node* c : n->list) {
        Handler h;
        h.start = tryStart;
        h.end = tryEnd;
        h.target = here();
        pushScope();
        TypeP ct = nullptr;
        if (c->type) {
            ct = resolveType(c->type, false);
            TypeP base = stripRef(ct);
            if (base->k == TK::Ptr && base->elem && base->elem->k == TK::Class && lang_ == Dialect::Cpp) {
                h.catchType = base;                  // catch (Classe* p)
            } else if (base->k == TK::Class && base->cls) {
                completeClass(base->cls);
                h.catches = base->cls;
                if ((lang_ == Dialect::CSharp || lang_ == Dialect::Java) && !base->cls->isException) error("catch vuole una classe di eccezione", c->pos);
            } else if (lang_ == Dialect::Cpp) {
                h.catchType = base;                  // catch (int e), catch (const char* m)
            } else error("catch vuole una classe di eccezione", c->pos);
            ct = base;
        } else if (lang_ == Dialect::CSharp) {
            h.catches = prog_.exceptionRoot[static_cast<int>(Dialect::CSharp)];
        }
        LocalVar& v = addLocal(c->sym ? syms_[c->sym] : std::string(), ct ? ct : tObject_, c->pos);
        if (ct && isValueType(ct)) v.needsKill = false;
        h.slot = v.slot;
        catches.push_back(h);
        catchSlots_.push_back(v.slot);
        block(c->a);
        catchSlots_.pop_back();
        popScope();
        exits.push_back(emit(Op::Jmp, 0));
    }
    // i gestori dei catch vanno prima di quelli dei try piu' esterni, dopo quelli interni
    for (const Handler& h : catches) fn_->handlers.push_back(h);
    (void)handlerBase;
    const std::int32_t catchEnd = here();
    if (n->b) {
        finallies_.pop_back();
        // uscita per eccezione: finally e poi rilancio
        Handler fh;
        fh.start = tryStart;
        fh.end = catchEnd;
        fh.target = here();
        fh.cleanup = true;
        fh.slot = fn_->nlocals++;
        fn_->handlers.push_back(fh);
        block(n->b);
        emit(Op::Rethrow, fh.slot);
        // uscita normale: finally
        for (std::int32_t x : exits) patch(x, here());
        block(n->b);
    } else {
        for (std::int32_t x : exits) patch(x, here());
    }
}

void Compiler::usingStmt(Node* n) {
    // using (risorsa) corpo  ->  try { corpo } finally { risorsa.Dispose(); }
    pushScope();
    std::int32_t slot = -1;
    TypeP rt = nullptr;
    if (n->a->k == K::VarDecl) {
        declareLocalVars(n->a);
        LocalVar* v = findLocal(syms_[n->a->list[0]->sym]);
        slot = v->slot;
        rt = v->type;
    } else {
        EV e = expr(n->a);
        load(e, n->a->pos);
        LocalVar& v = addLocal("", e.type, n->a->pos);
        emit(Op::StoreL, v.slot);
        slot = v.slot;
        rt = e.type;
    }
    const std::int32_t start = here();
    if (n->op == 1) {
        // using var: il resto del blocco (lo gestisce il blocco esterno)
        usingVars_.push_back({slot, rt});
        popScopeKeep_ = true;
    }
    if (n->b) block(n->b);
    const std::int32_t end = here();
    auto dispose = [&] {
        emit(Op::LoadL, slot);
        const std::int32_t skip = emit(Op::Jz, 0);
        emit(Op::LoadL, slot);
        disposeValue(rt, n->pos);
        patch(skip, here());
    };
    const std::int32_t jmp = emit(Op::Jmp, 0);
    Handler h;
    h.start = start;
    h.end = end;
    h.target = here();
    h.cleanup = true;
    h.slot = fn_->nlocals++;
    fn_->handlers.push_back(h);
    dispose();
    emit(Op::Rethrow, h.slot);
    patch(jmp, here());
    dispose();
    popScope();
}

// ================================================================ variabili locali
void Compiler::declareLocalVars(Node* n) {
    const bool isStatic = (n->mods & MStatic) != 0;
    for (Node* d : n->list) {
        if (d->k != K::Declarator) continue;
        setPos(d->pos);
        const std::string nm = syms_[d->sym];
        TypeP t = resolveType(d->type, false);
        if (t->k == TK::Void) error("variabile di tipo void", d->pos);
        const bool isConst = (d->mods & (MConst | MConstexpr)) != 0 || (d->type && d->type->isConst);
        if (isStatic && lang_ != Dialect::CSharp && lang_ != Dialect::Java) {
            // static locale: un globale con il nome nascosto, inizializzato una volta
            staticLocal(d, t, nm);
            continue;
        }
        const bool boxed = csCaptured_.count(nm) != 0 && (lang_ == Dialect::CSharp || lang_ == Dialect::Java);
        if (t->k == TK::Dynamic && t->size == -2) {
            // auto / var: il tipo dall'inizializzatore
            if (!d->a && d->list.empty()) error("auto/var vuole un valore iniziale", d->pos);
            if (d->a && d->a->k == K::InitList) error("auto con una lista { } non supportato", d->pos);
            Node* initExpr = d->a ? d->a : d->list[0];
            EV e = expr(initExpr);
            load(e, d->pos);
            TypeP vt = stripRef(e.type);
            if (vt->k == TK::Null) error("var non puo' valere null", d->pos);
            if (vt->k == TK::Void) error("auto/var di un'espressione void", d->pos);
            if (vt->k == TK::Array && vt->size >= 0 && (lang_ == Dialect::C || lang_ == Dialect::Cpp)) { convert(vt, ptrTo(vt->elem), d->pos); vt = ptrTo(vt->elem); }
            if (e.isStringLit && lang_ == Dialect::Cpp) { vt = ptrTo(tChar8_); convert(e.type, vt, d->pos); }
            copyIfLvalue(e, vt, d->pos);
            // auto& x = espressione: riferimento
            if (d->type && d->type->f == TF::Auto) {}
            LocalVar& v = addLocal(nm, vt, d->pos, boxed);
            v.isConst = isConst;
            if (isValueType(vt) && (lang_ == Dialect::C || lang_ == Dialect::Cpp)) { v.needsKill = true; v.needsDtor = needsDtor(vt); }
            if (boxed) { emit(Op::StoreL, v.slot); emit(Op::NewBox, v.slot); }
            else emit(Op::StoreL, v.slot);
            if (v.needsDtor && scopes_.back().cleanupStart < 0) scopes_.back().cleanupStart = here();
            continue;
        }
        if (t->k == TK::Ref) {
            // riferimento: si lega a un luogo
            if (!d->a) error("un riferimento vuole un valore iniziale", d->pos);
            TypeP target = t->elem;
            EV e = expr(d->a);
            if (!e.lvalue) {
                // const T& x = temporaneo: si tiene una copia
                load(e, d->pos);
                convert(e.type, target, d->pos, false, &e);
                LocalVar& v = addLocal(nm, target, d->pos);
                v.isConst = true;
                emit(Op::StoreL, v.slot);
                continue;
            }
            EV addr = addressOfEV(e, d->a->pos);
            (void)addr;
            if (stripRef(e.type) != target && !(stripRef(e.type)->k == TK::Class && target->k == TK::Class && stripRef(e.type)->cls->derivesFrom(target->cls)))
                error("riferimento di tipo " + typeName(target) + " a un valore di tipo " + typeName(stripRef(e.type)), d->a->pos);
            LocalVar& v = addLocal(nm, target, d->pos);
            v.isRef = true;
            v.isConst = isConst || (d->type && d->type->inner && d->type->inner->isConst);
            emit(Op::StoreL, v.slot);
            continue;
        }
        if (t->k == TK::Array && t->size == -4) {
            // VLA: int a[n];
            LocalVar& v = addLocal(nm, arrayOf(t->elem, -1, 1), d->pos);
            v.needsKill = true;
            rvalue(d->type->size, tI64_);
            emit(Op::NewArr, t->elem->index, 1);
            emit(Op::StoreL, v.slot);
            if (d->a) error("un array a lunghezza variabile non si inizializza", d->pos);
            continue;
        }
        if (t->k == TK::Array && t->size < 0 && d->a && d->a->k == K::InitList && (lang_ == Dialect::C || lang_ == Dialect::Cpp)) {
            // int a[] = {1, 2, 3}: la dimensione dall'inizializzatore
            std::int32_t count = static_cast<std::int32_t>(d->a->list.size());
            for (Node* item : d->a->list) if (item->k == K::Index && item->list.size() == 1) {
                std::int64_t ix = 0;
                if (constInt(item->list[0], ix)) count = std::max<std::int32_t>(count, static_cast<std::int32_t>(ix + 1));
            }
            t = arrayOf(t->elem, std::max(count, 1), 1);
        }
        if (t->k == TK::Array && t->size < 0 && d->a && d->a->k == K::StrLit && t->elem && t->elem->k == TK::C8) {
            t = arrayOf(t->elem, static_cast<std::int32_t>(units_[unit_].strings[d->a->str].size() + 1), 1);   // char s[] = "ciao"
        }
        LocalVar& v = addLocal(nm, t, d->pos, boxed);
        v.isConst = isConst;
        if ((lang_ == Dialect::C || lang_ == Dialect::Cpp) && (isValueType(t) || (t->k == TK::Array))) { v.needsKill = true; v.needsDtor = needsDtor(t); }
        // costante intera di compilazione (const int N = 5;)
        std::int64_t cv = 0;
        if (isConst && d->a && isIntegral(t) && d->a->k != K::InitList && constInt(d->a, cv)) { v.hasConstValue = true; v.constValue = Value::ofInt(vkOf(t), cv); }
        initVariable(v, d, d->pos);
        if (boxed) emit(Op::NewBox, v.slot);
        if (v.needsDtor && scopes_.back().cleanupStart < 0) scopes_.back().cleanupStart = here();
    }
}
void Compiler::staticLocal(Node* d, TypeP t, const std::string& nm) {
    auto g = std::make_unique<GlobalVar>();
    g->name = fn_->name + "::" + nm;
    g->type = t;
    g->slot = static_cast<std::int32_t>(prog_.globalNames.size());
    prog_.globalNames.push_back(g->name);
    prog_.globalTypes.push_back(t);
    prog_.globalFile.push_back(d->pos.file);
    staticLocals_.push_back({nm, g.get(), scopes_.size()});
    // inizializzazione una sola volta: costante all'avvio, espressione al primo passaggio (C++)
    std::int64_t cv = 0;
    const bool constant = !d->a || (d->a->k != K::InitList && constInt(d->a, cv)) || d->a->k == K::FloatLit || d->a->k == K::StrLit;
    if (constant || lang_ == Dialect::C) {
        inits_.push_back({d, g.get(), ns_, lang_, unit_, cls_});
    } else {
        auto guard = std::make_unique<GlobalVar>();
        guard->name = g->name + "#guard";
        guard->type = tBool_;
        guard->slot = static_cast<std::int32_t>(prog_.globalNames.size());
        prog_.globalNames.push_back(guard->name);
        prog_.globalTypes.push_back(tBool_);
        prog_.globalFile.push_back(d->pos.file);
        emit(Op::LoadG, guard->slot);
        const std::int32_t skip = emit(Op::Jnz, 0);
        emit(Op::PushI, 1, static_cast<std::uint8_t>(VK::Bool));
        emit(Op::StoreG, guard->slot);
        rvalue(d->a, t);
        emit(Op::StoreG, g->slot);
        patch(skip, here());
        globals_.push_back(std::move(guard));
    }
    LocalVar v;
    v.name = nm;
    v.type = t;
    v.slot = -1;
    v.global = g->slot;
    scopes_.back().vars.push_back(v);
    globals_.push_back(std::move(g));
}

void Compiler::initVariable(LocalVar& v, Node* d, Pos at) {
    TypeP t = v.type;
    if (d->op == 2 || (d->op == 3 && t->k == TK::Class && t->cls && !t->cls->ctors.empty())) {
        // T x(args) / T x{args} con costruttori
        std::vector<Node*> args = d->op == 2 ? d->list : d->a->list;
        if (t->k == TK::Class && t->cls && !t->cls->isEnum) {
            completeClass(t->cls);
            constructObject(t->cls, args, at, true);
        } else if (t->k == TK::Native || (t->k == TK::Str && t->cppValue)) {
            constructNative(t, args, at);
        } else {
            if (args.size() != 1) error("inizializzazione con () di un tipo semplice vuole un valore", at);
            rvalue(args[0], t);
        }
        emit(Op::StoreL, v.slot);
        return;
    }
    if (d->a && d->a->k == K::InitList) {
        initFromList(t, d->a, at);
        emit(Op::StoreL, v.slot);
        return;
    }
    if (d->a) {
        // char s[20] = "testo" (C)
        if (t->k == TK::Array && t->elem && t->elem->k == TK::C8 && d->a->k == K::StrLit) {
            const std::string& s = units_[unit_].strings[d->a->str];
            if (t->size >= 0 && static_cast<std::int32_t>(s.size()) > t->size) error("stringa piu' lunga dell'array", d->a->pos);
            pushInt(t->size, VK::I64);
            emit(Op::NewArr, t->elem->index, 1);
            emit(Op::Dup);
            pushString(s, Dialect::C, false);
            emit(Op::CallN, nativeIndex("string.h", "", "strcpy", "charptr(charptr,cstr)"), 2, 1);
            emit(Op::Pop);
            emit(Op::StoreL, v.slot);
            return;
        }
        EV e = d->a->k == K::Lambda ? lambda(d->a, stripRef(t)) : expr(d->a);
        load(e, at);
        // Punto p = q; (copia) / C++: T x = 5 con un costruttore da int
        if (t->k == TK::Class && t->cls && t->cls->valueType && !t->cls->isEnum && stripRef(e.type) != t &&
            !(stripRef(e.type)->k == TK::Class && stripRef(e.type)->cls && stripRef(e.type)->cls->derivesFrom(t->cls))) {
            if (lang_ == Dialect::Cpp && !t->cls->ctors.empty()) {
                // conversione con un costruttore a un argomento: T x = valore
                emit(Op::Pop);
                constructObject(t->cls, {d->a}, at, true);
                emit(Op::StoreL, v.slot);
                return;
            }
        }
        convert(e.type, t, at, false, &e);
        copyIfLvalue(e, t, at);
        emit(Op::StoreL, v.slot);
        return;
    }
    defaultInit(t, at);
    emit(Op::StoreL, v.slot);
}
void Compiler::copyIfLvalue(EV& e, TypeP t, Pos at) {
    if (!isValueType(t) || e.temp) return;
    if (e.cat == EV::RValue && !e.lvalue) return;
    emit(Op::Copy, t->k == TK::Class && t->cls ? t->cls->id : -1);
    (void)at;
}
void Compiler::zeroValue(TypeP t, Pos at) {
    (void)at;
    switch (t->k) {
    case TK::F32: case TK::F64: pushDouble(0, vkOf(t)); return;
    case TK::Ptr: case TK::Null: case TK::Func: emit(Op::PushNull); return;
    case TK::Class: case TK::Str: case TK::Native: case TK::Array: case TK::Dynamic: case TK::Nullable:
        if (isValueType(t)) { defaultInit(t, at); return; }
        emit(Op::PushNull);
        return;
    case TK::Void: emit(Op::PushVoid); return;
    default: emit(Op::PushI, 0, static_cast<std::uint8_t>(vkOf(t))); return;
    }
}
// Valore iniziale senza inizializzatore.
void Compiler::defaultInit(TypeP t, Pos at) {
    switch (t->k) {
    case TK::Class:
        if (t->cls && t->cls->valueType && !t->cls->isEnum) {
            completeClass(t->cls);
            if (!t->cls->complete) error("tipo " + t->cls->name + " incompleto", at);
            constructObject(t->cls, {}, at, true);
            return;
        }
        if (t->cls && t->cls->isEnum) { emit(Op::PushI, 0, static_cast<std::uint8_t>(vkOf(t))); return; }
        emit(Op::PushNull);
        return;
    case TK::Str:
        if (t->cppValue) { pushString("", Dialect::Cpp, true); return; }
        emit(Op::PushNull);
        return;
    case TK::Native:
        if (isValueType(t)) { emit(Op::NewNative, t->index); return; }
        emit(Op::PushNull);
        return;
    case TK::Array:
        if (t->size >= 0) {
            // array del C/C++ (anche di array): dimensioni dal tipo
            TypeP e = t;
            std::vector<std::int32_t> dims;
            while (e->k == TK::Array && e->size >= 0) { dims.push_back(e->size); e = e->elem; }
            for (std::int32_t dm : dims) pushInt(dm, VK::I64);
            emit(Op::NewArr, e->index, static_cast<std::uint8_t>(dims.size()));
            return;
        }
        emit(Op::PushNull);
        return;
    default: zeroValue(t, at); return;
    }
}
// Crea un oggetto della classe e chiama il costruttore adatto agli argomenti.
void Compiler::constructObject(ClassInfo* c, const std::vector<Node*>& args, Pos at, bool onStack) {
    (void)onStack;
    completeClass(c);
    if (c->isAbstract) error("la classe " + c->name + " e' astratta: non si puo' creare", at);
    if (c->isInterface) error("non si crea un'interfaccia", at);
    emit(Op::New, c->id);
    if (c->ctors.empty()) {
        if (!args.empty()) {
            // aggregato (struct senza costruttori): T x{a, b} / T(a, b) non e' ammesso
            if (c->lang == Dialect::C || c->lang == Dialect::Cpp) {
                Node* il = pool_.node(K::InitList, at);
                il->list = args;
                emit(Op::Pop);
                initFromList(classType(c), il, at);
                return;
            }
            error("la classe " + c->name + " non ha costruttori con argomenti", at);
        }
        ensureFieldInit(c);
        if (c->fieldInit) { emit(Op::Dup); emit(Op::Call, c->fieldInit->index, 1); }
        return;
    }
    emit(Op::Dup);
    callOverloads(c->ctors, {}, {}, args, {}, at, true, classType(c), true, c->name);
}
// Contenitori del C++ (vector<int> v(5, 0), string s(3, 'x')...).
void Compiler::constructNative(TypeP t, const std::vector<Node*>& args, Pos at) {
    if (t->k == TK::Str && t->cppValue) {
        if (args.empty()) { pushString("", Dialect::Cpp, true); return; }
        EV recv;
        pushString("", Dialect::Cpp, true);
        std::vector<std::int32_t> ctors = nativeCtors(t);
        callOverloads({}, ctors, {}, args, {}, at, true, t, false, "std::string");
        return;
    }
    if (t->k != TK::Native) {
        if (args.size() == 1) { rvalue(args[0], t); return; }
        if (args.empty()) { defaultInit(t, at); return; }
        error("inizializzazione non valida per " + typeName(t), at);
    }
    emit(Op::NewNative, t->index);
    if (args.empty()) return;
    std::vector<std::int32_t> ctors = nativeCtors(t);
    if (ctors.empty()) error(typeName(t) + " non ha costruttori con argomenti", at);
    emit(Op::Dup);
    EV r = callOverloads({}, ctors, {}, args, {}, at, true, t, false, typeName(t));
    if (r.type && r.type->k != TK::Void) emit(Op::Pop);
}
// { a, b, c } per array, struct, contenitori e classi.
void Compiler::initFromList(TypeP t, Node* list, Pos at) {
    if (++depth_ > ParseDepth) error("Stack ERROR: inizializzatore troppo annidato", at);
    struct D { int& d; ~D() { --d; } } guard{depth_};
    t = stripRef(t);
    if (t->k == TK::Array) {
        TypeP e = t->elem;
        std::int32_t size = t->size;
        const bool cs = lang_ == Dialect::CSharp || lang_ == Dialect::Java;
        if (cs) size = static_cast<std::int32_t>(list->list.size());
        if (size < 0) size = static_cast<std::int32_t>(list->list.size());
        if (static_cast<std::int32_t>(list->list.size()) > size && !cs) error("troppi valori nell'inizializzazione (" + std::to_string(list->list.size()) + " per " + std::to_string(size) + ")", list->pos);
        // array C#: [,] da liste annidate
        if (cs && t->rank == 2) {
            std::int32_t rows = static_cast<std::int32_t>(list->list.size()), cols = -1;
            for (Node* r : list->list) {
                if (r->k != K::InitList) error("array [,] vuole righe { }", r->pos);
                if (cols >= 0 && cols != static_cast<std::int32_t>(r->list.size())) error("righe di lunghezza diversa", r->pos);
                cols = static_cast<std::int32_t>(r->list.size());
            }
            pushInt(rows, VK::I64);
            pushInt(std::max(cols, 0), VK::I64);
            emit(Op::NewArr, e->index, 2, 2);
            for (std::int32_t r = 0; r < rows; ++r) for (std::int32_t c = 0; c < cols; ++c) {
                emit(Op::Dup);
                pushInt(r, VK::I64);
                pushInt(c, VK::I64);
                rvalue(list->list[static_cast<std::size_t>(r)]->list[static_cast<std::size_t>(c)], e);
                emit(Op::SetE2);
            }
            return;
        }
        // array con dimensione: elementi predefiniti, poi i valori
        TypeP inner = e;
        std::vector<std::int32_t> dims{size};
        while (inner->k == TK::Array && inner->size >= 0 && !cs) { dims.push_back(inner->size); inner = inner->elem; }
        for (std::int32_t dm : dims) pushInt(dm, VK::I64);
        emit(Op::NewArr, inner->index, static_cast<std::uint8_t>(dims.size()));
        std::int32_t idx = 0;
        for (Node* item : list->list) {
            Node* value = item;
            if (item->k == K::Index && !item->a) {
                std::int64_t ix = 0;
                if (!constInt(item->list[0], ix) || ix < 0 || ix >= size) error("indice dell'inizializzatore non valido", item->pos);
                idx = static_cast<std::int32_t>(ix);
                value = item->b;
            }
            if (idx >= size) error("troppi valori nell'inizializzazione", item->pos);
            if (e->k == TK::Array && e->size >= 0 && value->k == K::InitList) {
                // riga di una matrice: si riempie la sotto-array esistente
                emit(Op::Dup);
                pushInt(idx, VK::I64);
                emit(Op::GetE);
                fillArray(e, value);
                emit(Op::Pop);
            } else if (e->k == TK::Array && e->size >= 0 && value->k == K::StrLit && e->elem->k == TK::C8) {
                emit(Op::Dup);
                pushInt(idx, VK::I64);
                emit(Op::GetE);
                pushString(units_[unit_].strings[value->str], Dialect::C, false);
                emit(Op::CallN, nativeIndex("string.h", "", "strcpy", "charptr(charptr,cstr)"), 2, 1);
                emit(Op::Pop);
            } else if (value->k == K::InitList || (isValueType(e) && e->k == TK::Class)) {
                emit(Op::Dup);
                pushInt(idx, VK::I64);
                if (value->k == K::InitList) initFromList(e, value, value->pos);
                else { EV v = expr(value); load(v, value->pos); convert(v.type, e, value->pos, false, &v); copyIfLvalue(v, e, value->pos); }
                emit(Op::SetE);
            } else {
                emit(Op::Dup);
                pushInt(idx, VK::I64);
                rvalue(value, e);
                emit(Op::SetE);
            }
            ++idx;
        }
        return;
    }
    if (t->k == TK::Class && t->cls) {
        ClassInfo* c = t->cls;
        completeClass(c);
        if (!c->ctors.empty() && lang_ == Dialect::Cpp) { constructObject(c, list->list, at, true); return; }
        if (c->lang == Dialect::CSharp || c->lang == Dialect::Java) error("una classe di C# si crea con new", at);
        // aggregato: campi in ordine (anche della base), designati .campo =
        emit(Op::New, c->id);
        std::vector<const FieldInfo*> order;
        std::vector<const ClassInfo*> chain;
        for (const ClassInfo* k = c; k; k = k->base) chain.push_back(k);
        std::reverse(chain.begin(), chain.end());
        for (const ClassInfo* k : chain) for (const FieldInfo& f : k->fields) if (!f.isStatic) order.push_back(&f);
        std::size_t next = 0;
        for (Node* item : list->list) {
            Node* value = item;
            const FieldInfo* f = nullptr;
            if (item->k == K::Label) {
                for (std::size_t i = 0; i < order.size(); ++i) if (order[i]->name == syms_[item->sym]) { f = order[i]; next = i; }
                if (!f) error("la struct " + c->name + " non ha il campo '" + syms_[item->sym] + "'", item->pos);
                value = item->a;
            } else {
                if (next >= order.size()) error("troppi valori per la struct " + c->name, item->pos);
                f = order[next];
            }
            ++next;
            if (isValueType(f->type) && (value->k == K::InitList || f->type->k == TK::Array)) {
                if (f->type->k == TK::Array && value->k == K::StrLit && f->type->elem->k == TK::C8) {
                    emit(Op::Dup); emit(Op::GetF, f->slot);
                    pushString(units_[unit_].strings[value->str], Dialect::C, false);
                    emit(Op::CallN, nativeIndex("string.h", "", "strcpy", "charptr(charptr,cstr)"), 2, 1);
                    emit(Op::Pop);
                    continue;
                }
                if (value->k == K::InitList && f->type->k == TK::Array) {
                    emit(Op::Dup); emit(Op::GetF, f->slot);
                    fillArray(f->type, value);
                    emit(Op::Pop);
                    continue;
                }
                emit(Op::Dup);
                if (value->k == K::InitList) initFromList(f->type, value, value->pos);
                else rvalue(value, f->type);
                emit(Op::SetF, f->slot);
                continue;
            }
            emit(Op::Dup);
            if (value->k == K::InitList) initFromList(f->type, value, value->pos);
            else {
                EV v = expr(value);
                load(v, value->pos);
                convert(v.type, f->type, value->pos, false, &v);
                copyIfLvalue(v, f->type, value->pos);
            }
            emit(Op::SetF, f->slot);
        }
        return;
    }
    if (t->k == TK::Native || (t->k == TK::Str && t->cppValue)) {
        listToNative(t, list, at);
        return;
    }
    if (list->list.size() == 1 && !isValueType(t)) { rvalue(list->list[0], t); return; }
    if (list->list.empty()) { zeroValue(t, at); return; }
    error("inizializzazione { } non valida per " + typeName(t), at);
}
// Riempie l'array sulla pila con i valori della lista (righe di una matrice del C).
void Compiler::fillArray(TypeP t, Node* list) {
    TypeP e = t->elem;
    if (static_cast<std::int32_t>(list->list.size()) > t->size) error("troppi valori nella riga", list->pos);
    std::int32_t idx = 0;
    for (Node* item : list->list) {
        if (e->k == TK::Array && e->size >= 0 && item->k == K::InitList) {
            emit(Op::Dup); pushInt(idx, VK::I64); emit(Op::GetE);
            fillArray(e, item);
            emit(Op::Pop);
        } else {
            emit(Op::Dup);
            pushInt(idx, VK::I64);
            if (item->k == K::InitList) initFromList(e, item, item->pos); else rvalue(item, e);
            emit(Op::SetE);
        }
        ++idx;
    }
}

void Compiler::bindingDecl(Node* n) {
    // auto [a, b] = espressione (pair, struct)
    EV e = expr(n->a);
    load(e, n->pos);
    TypeP t = stripRef(e.type);
    LocalVar& hidden = addLocal("", t, n->pos);
    emit(Op::StoreL, hidden.slot);
    const std::int32_t hs = hidden.slot;
    bindNames(n, t, hs, n->op == 1);
}
void Compiler::bindNames(Node* n, TypeP t, std::int32_t slot, bool byRef) {
    std::vector<std::pair<std::string, TypeP>> parts;
    if (t->k == TK::Native && (t->nk == NK::Pair || t->nk == NK::CsKvp || t->nk == NK::MapEntry)) {
        if (n->list.size() != 2) error("una coppia si scompone in due nomi", n->pos);
        for (int i = 0; i < 2; ++i) {
            emit(Op::LoadL, slot);
            const char* m = i == 0 ? "first" : "second";
            EV recv; recv.cat = EV::RValue; recv.type = t;
            EV r = methodOn(recv, m, nullptr, nullptr, n->pos);
            if (r.lvalue && !byRef) { load(r, n->pos); }
            else if (r.lvalue) { EV a = addressOfEV(r, n->pos); (void)a; }
            TypeP pt = stripRef(r.type);
            LocalVar& v = addLocal(syms_[n->list[static_cast<std::size_t>(i)]->sym], pt, n->pos);
            if (r.lvalue && byRef) v.isRef = true;
            else if (isValueType(pt)) { copyIfLvalue(r, pt, n->pos); }
            emit(Op::StoreL, v.slot);
        }
        return;
    }
    if (t->k == TK::Class && t->cls) {
        std::vector<const FieldInfo*> fs;
        for (const FieldInfo& f : t->cls->fields) if (!f.isStatic) fs.push_back(&f);
        if (fs.size() != n->list.size()) error("numero di nomi diverso dai campi della struct", n->pos);
        for (std::size_t i = 0; i < fs.size(); ++i) {
            emit(Op::LoadL, slot);
            if (byRef) emit(Op::AddrF, fs[i]->slot); else emit(Op::GetF, fs[i]->slot);
            LocalVar& v = addLocal(syms_[n->list[i]->sym], fs[i]->type, n->pos);
            v.isRef = byRef;
            if (!byRef && isValueType(fs[i]->type)) emit(Op::Copy, -1);
            emit(Op::StoreL, v.slot);
        }
        return;
    }
    error("binding strutturato non supportato per " + typeName(t), n->pos);
}

// ================================================================ foreach / range-for
void Compiler::forEachStmt(Node* n) {
    LoopCtx lc;
    lc.label = pendingLabel_;
    pendingLabel_ = 0;
    pushScope();
    // la collezione in una variabile nascosta
    EV ce = expr(n->b);
    TypeP ct = stripRef(ce.type);
    if (ce.lvalue && isValueType(ct)) {
        // riferimento alla collezione (niente copia): range-for su un vector per riferimento
        EV addr = addressOfEV(ce, n->b->pos);
        (void)addr;
        LocalVar& h = addLocal("", ptrTo(ct), n->b->pos);
        emit(Op::StoreL, h.slot);
        emit(Op::LoadL, h.slot);
        emit(Op::LoadP);
    } else {
        load(ce, n->b->pos);
    }
    if (n->b->k == K::InitList) error("range-for su una lista { } non supportato", n->b->pos);
    LocalVar& coll = addLocal("", ct, n->b->pos);
    const std::int32_t collSlot = coll.slot;
    emit(Op::StoreL, collSlot);
    LocalVar& idx = addLocal("", tI64_, n->pos);
    const std::int32_t idxSlot = idx.slot;
    pushInt(0, VK::I64);
    emit(Op::StoreL, idxSlot);
    // ciclo: while (i < size) { x = c[i]; corpo; i++ }
    lc.scopeDepth = scopes_.size();
    lc.finallyDepth = static_cast<std::int32_t>(finallies_.size());
    const std::int32_t start = here();
    setPos(n->pos);
    markStatement();
    emit(Op::LoadL, idxSlot);
    emit(Op::LoadL, collSlot);
    TypeP elemT = nullptr;
    bool mapLike = false;
    if (ct->k == TK::Array) {
        if (ct->size >= 0) { emit(Op::Pop); pushInt(ct->size, VK::I64); }
        else emit(Op::ArrLen, 0, 1);
        elemT = ct->elem;
    } else if (ct->k == TK::Str) {
        emit(Op::CallN, nativeIndex("", "#iter", "size", "long(any)"), 1, 1);
        elemT = charType(lang_);
    } else if (ct->k == TK::Native) {
        emit(Op::CallN, nativeIndex("", "#iter", "size", "long(any)"), 1, 1);
        switch (ct->nk) {
        case NK::Map: case NK::UMap: elemT = nativeOf(NK::MapEntry, {ct->args[0], ct->args[1]}); mapLike = true; break;
        case NK::CsDict: case NK::JHashMap: case NK::JTreeMap: elemT = nativeOf(NK::CsKvp, {ct->args[0], ct->args[1]}); mapLike = true; break;
        default: elemT = ct->args.empty() ? tDyn_ : ct->args[0]; break;
        }
    } else if (ct->k == TK::Ptr) {
        error("range-for su un puntatore: serve un array o un contenitore", n->b->pos);
    } else error("foreach su un valore che non e' una collezione (" + typeName(ct) + ")", n->b->pos);
    emit(Op::Lt);
    const std::int32_t jz = emit(Op::Jz, 0);
    pushScope();
    // variabile del ciclo
    Node* d = n->a;
    if (d->k == K::Binding) {
        // auto [k, v] : mappa
        emit(Op::LoadL, collSlot);
        emit(Op::LoadL, idxSlot);
        emit(Op::CallN, nativeIndex("", "#iter", "at", "any(any,long)"), 2, 1);
        LocalVar& h = addLocal("", elemT, d->pos);
        emit(Op::StoreL, h.slot);
        bindNames(d, elemT, h.slot, d->op == 1);
    } else {
        TypeP vt = resolveType(d->type, false);
        const bool byRef = vt->k == TK::Ref || (d->type && d->type->f == TF::Reference);
        if (vt->k == TK::Ref) vt = vt->elem;
        if (vt->k == TK::Dynamic && vt->size == -2) vt = elemT;
        emit(Op::LoadL, collSlot);
        emit(Op::LoadL, idxSlot);
        if (ct->k == TK::Array) {
            if (byRef) emit(Op::AddrE); else emit(Op::GetE);
        } else if (mapLike) {
            emit(Op::CallN, nativeIndex("", "#iter", "at", "any(any,long)"), 2, 1);
        } else {
            emit(Op::CallN, nativeIndex("", "#iter", byRef ? "ref" : "at", byRef ? "any(any,long)" : "any(any,long)"), 2, 1);
        }
        LocalVar& v = addLocal(syms_[d->sym], vt, d->pos, csCaptured_.count(syms_[d->sym]) != 0);
        if (byRef && !mapLike) v.isRef = true;
        else {
            EV tmp; tmp.cat = EV::RValue; tmp.type = elemT; tmp.lvalue = true;
            if (vt != elemT && !mapLike) convert(elemT, vt, d->pos, lang_ == Dialect::CSharp, &tmp);
            if (isValueType(vt) && !mapLike) emit(Op::Copy, vt->k == TK::Class && vt->cls ? vt->cls->id : -1);
        }
        emit(Op::StoreL, v.slot);
        if (v.boxed) emit(Op::NewBox, v.slot);
    }
    loops_.push_back(lc);
    statement(n->c);
    lc = loops_.back();
    loops_.pop_back();
    popScope();
    const std::int32_t cont = here();
    emit(Op::IncL, idxSlot, 0, static_cast<std::uint16_t>(VK::I64));
    emit(Op::Pop);
    emit(Op::Jmp, start);
    patch(jz, here());
    for (std::int32_t b : lc.breaks) patch(b, here());
    for (std::int32_t c : lc.continues) patch(c, cont);
    popScope();
}

} // namespace fx::prog
