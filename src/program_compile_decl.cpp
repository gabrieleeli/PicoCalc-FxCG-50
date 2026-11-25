// Compilatore dei programmi (v1.20.0), parte 2: seconda passata delle
// dichiarazioni (funzioni, variabili, typedef, using), completamento delle
// classi (basi, campi, metodi, proprieta', enum), disposizione, vtable e
// interfacce, istanze dei template e dei generici, punto d'ingresso,
// compilazione dei corpi. Vedi program_compile.hpp.
#include "program_compile.hpp"
#include <algorithm>

namespace fx::prog {
using namespace ast;

void Compiler::enterUnitContext(Dialect d, std::uint16_t unit, std::int32_t library, Namespace* ns, ClassInfo* cls) {
    lang_ = d;
    unit_ = unit;
    library_ = library;
    ns_ = ns ? ns : root_.get();
    cls_ = cls;
    if (unit < units_.size()) { headers_ = units_[unit].standardHeaders; usings_ = units_[unit].usings; }
}

std::string Compiler::templateKey(const std::vector<TypeP>& args) const {
    std::string k;
    for (TypeP a : args) k += std::to_string(a ? a->index : -1) + ",";
    return k;
}

void Compiler::run() {
    setupNativeClasses();
    for (std::size_t i = 0; i < units_.size(); ++i) declareUnit(units_[i], static_cast<std::uint16_t>(i));
    // using prima di tutto (C#: namespace delle librerie; C++: using namespace)
    for (const LaterDecl& d : laterDecls_) if (d.node->k == K::Using) processLater(d);
    // enum: le costanti servono alle dimensioni degli array e ai case
    for (ClassInfo* c : std::vector<ClassInfo*>(classOrder_)) if (c->isEnum) completeClass(c);
    for (const LaterDecl& d : laterDecls_) if (d.node->k != K::Using) processLater(d);
    finishClasses();
    setupEntry();
    compileBodies();
    // sizeof delle struct e delle classi del C/C++ per i nativi (qsort, malloc, memcpy)
    for (const auto& c : prog_.classes) {
        if (c->isNative || !c->complete || !c->self || (c->lang != Dialect::C && c->lang != Dialect::Cpp)) continue;
        try { c->sizeBytes = sizeOf(c->self, ast::Pos{}); } catch (const Error&) {}
    }
}

// ---------------------------------------------------------------- seconda passata
void Compiler::processLater(const LaterDecl& d) {
    enterUnitContext(d.lang, d.unit, d.library, d.ns, d.owner);
    Node* n = d.node;
    switch (n->k) {
    case K::Using: {
        if (n->op == 1) {
            std::string path = syms_[n->sym];
            std::string norm;
            for (std::size_t i = 0; i < path.size(); ++i) { if (path[i] == '.') norm += "::"; else norm += path[i]; }
            Namespace* target = nullptr;
            for (Namespace* s = d.ns; s && !target; s = s->parent) target = namespaceFor(s, norm, false);
            if (!target) target = namespaceFor(root_.get(), norm, true);
            if (std::find(d.ns->usings.begin(), d.ns->usings.end(), target) == d.ns->usings.end()) d.ns->usings.push_back(target);
        } else if (n->op == 2 && n->type) {
            // using X::nome: come using namespace X per quel nome
            std::string path;
            for (Sym q : n->type->qual) path += (path.empty() ? "" : "::") + syms_[q];
            Namespace* target = namespaceFor(root_.get(), path, path == "std");
            if (target) {
                const std::string nm = syms_[n->type->name];
                NsEntry* e = lookupScoped(target, nm);
                if (e) d.ns->members[nm] = *e;
                else if (std::find(d.ns->usings.begin(), d.ns->usings.end(), target) == d.ns->usings.end()) d.ns->usings.push_back(target);
            }
        }
        return;
    }
    case K::Typedef: {
        Namespace* scopeNs = d.owner ? classScope_[d.owner] : d.ns;
        TypeP t = resolveType(n->type, true);
        const std::string nm = syms_[n->sym];
        NsEntry& e = scopeNs->members[nm];
        if (e.kind == NsEntry::Typedef && e.type == t) return;
        if (e.kind == NsEntry::Class && t->k == TK::Class && t->cls == e.cls) return;   // typedef struct X X;
        if (e.kind != NsEntry::None && e.kind != NsEntry::Typedef) error("'" + nm + "' gia' dichiarato", n->pos);
        e.kind = NsEntry::Typedef;
        e.type = t;
        return;
    }
    case K::Func: (void)declareFunction(d.ns, n, d.owner); return;
    case K::VarDecl: case K::Field: declareVariables(d.ns, n, d.owner); return;
    case K::Block: for (Node* m : n->list) processLater({m, d.ns, d.owner, d.lang, d.unit, d.library}); return;
    case K::Friend: return;
    case K::Empty: return;
    default: error("dichiarazione non valida qui", n->pos);
    }
}

std::vector<TypeP> Compiler::paramTypes(Node* fn) {
    std::vector<TypeP> ps;
    for (Node* p : fn->list) {
        TypeP t = resolveType(p->type, true);
        if (t->k == TK::Void && fn->list.size() == 1 && !p->sym) break;
        if ((lang_ == Dialect::C || lang_ == Dialect::Cpp) && t->k == TK::Array) t = ptrTo(t->elem);   // decadimento
        if ((lang_ == Dialect::C || lang_ == Dialect::Cpp) && t->k == TK::Func && t->size != 1) t = ptrTo(t);   // non le lambda (template)
        if (p->mods & (MRef | MOut | MIn)) t = refTo(t);
        ps.push_back(t);
    }
    return ps;
}
bool Compiler::sameParams(const Function* f, const std::vector<TypeP>& ps) const {
    if (f->params.size() != ps.size()) return false;
    for (std::size_t i = 0; i < ps.size(); ++i) if (f->params[i].type != ps[i]) return false;
    return true;
}

Function* Compiler::declareFunction(Namespace* ns, Node* n, ClassInfo* owner, bool friendFn) {
    // T A::f(...) { }: definizione di un membro fuori dalla classe
    if (n->type2 && !owner) {
        TypeP ot = nullptr;
        std::string path;
        for (Sym q : n->type2->qual) path += syms_[q] + "::";
        path += syms_[n->type2->name];
        NsEntry* e = lookupGlobalName(path, ns, nullptr);
        if (e && e->kind == NsEntry::Namespace) {
            Namespace* save = ns_;
            ns_ = e->ns;
            Node copy = *n;
            copy.type2 = nullptr;
            Node* c = pool_.node(K::Func, n->pos);
            *c = copy;
            Function* f = declareFunction(e->ns, c, nullptr);
            ns_ = save;
            return f;
        }
        if (e && e->kind == NsEntry::Template) {
            // definizione di un membro di un template di classe senza template<>: errore
            error("membro del template " + path + " definito senza template<...>", n->pos);
        }
        ot = resolveType(n->type2, false);
        if (ot->k != TK::Class || !ot->cls) error("'" + path + "' non e' una classe", n->pos);
        completeClass(ot->cls);
        std::vector<Function*> fs = attachOutOfClass(ot->cls, n, ns);
        return fs.empty() ? nullptr : fs[0];
    }
    const std::string nm = syms_[n->sym];
    if (owner) cls_ = owner;
    Function* f = newFunction(n->op == 1 ? (lang_ == Dialect::Cpp ? owner->name : ".ctor") : nm, lang_, owner);
    if (n->op == 1) { f->simple = owner ? owner->name : nm; f->name = owner ? owner->name + (lang_ == Dialect::Cpp ? "::" : ".") + owner->name : nm; }
    f->file = n->pos.file;
    f->line = static_cast<int>(n->pos.line);
    f->column = n->pos.col;
    f->isCtor = n->op == 1;
    f->isDtor = n->op == 2;
    f->isOperator = nm.rfind("operator", 0) == 0 && nm.size() > 8 && !std::isalnum(static_cast<unsigned char>(nm[8]));
    const bool cs = lang_ == Dialect::CSharp || lang_ == Dialect::Java;
    f->isStatic = owner == nullptr || friendFn || (n->mods & MStatic) != 0;
    if (owner && owner->isStaticClass && !(n->mods & MStatic) && cs) error("una classe static ha solo membri static", n->pos);
    f->hasThis = !f->isStatic;
    f->isConst = (n->mods & MConstMethod) != 0;
    f->isOverride = (n->mods & MOverride) != 0;
    f->isAbstract = (n->mods & MPure) != 0 || ((n->mods & MAbstract) != 0) || (owner && owner->isInterface && !n->b && !(n->mods & MStatic));
    f->isVirtual = (n->mods & (MVirtual | MOverride)) != 0 || f->isAbstract || (lang_ == Dialect::Java && owner && !f->isStatic && !f->isCtor && !(n->mods & (MPrivate | MFinal)));
    if (owner && owner->isInterface) f->isVirtual = !f->isStatic;
    // accesso
    if (n->mods & MPrivate) f->access = Access::Private;
    else if (n->mods & MProtected) f->access = Access::Protected;
    else if (n->mods & MPublic) f->access = Access::Public;
    else if (n->mods & MInternal) f->access = Access::Public;
    else if (cs && owner && !owner->isInterface) f->access = lang_ == Dialect::Java ? Access::Public : Access::Private;
    else f->access = Access::Public;
    // parametri
    const std::vector<TypeP> ps = paramTypes(n);
    for (std::size_t i = 0; i < ps.size(); ++i) {
        Param p;
        Node* pn = n->list[i];
        p.name = syms_[pn->sym];
        p.type = ps[i];
        p.byRef = ps[i]->k == TK::Ref;
        p.isOut = (pn->mods & MOut) != 0;
        p.isParams = (pn->mods & MParams) != 0;
        p.hasDefault = pn->a != nullptr;
        if (p.isParams && (ps[i]->k != TK::Array)) error("params vuole un array", pn->pos);
        f->params.push_back(p);
    }
    if (f->isCtor || f->isDtor) f->ret = tVoid_;
    else {
        f->ret = resolveType(n->type, true);
        if (f->ret->k == TK::Array && !cs) error("una funzione non puo' restituire un array", n->pos);
    }
    if (f->isDtor && !ps.empty()) error("il distruttore non ha parametri", n->pos);
    fnDecls_[f] = n;
    // registrazione
    if (owner) {
        if (f->isCtor && (n->mods & MStatic)) {
            if (owner->staticInit) error("costruttore statico gia' dichiarato", n->pos);
            owner->staticInit = f;
            f->hasThis = false;
            f->isStatic = true;
        } else if (f->isCtor) {
            for (Function* g : owner->ctors) if (sameParams(g, ps)) {
                if (g->defined || !n->b) error("costruttore " + owner->name + " gia' dichiarato con questi parametri", n->pos);
            }
            owner->ctors.push_back(f);
        } else if (f->isDtor) {
            if (owner->dtor) error("distruttore gia' dichiarato", n->pos);
            owner->dtor = f;
            if (n->mods & MVirtual) f->isVirtual = true;
        } else {
            for (Function* g : owner->methods) if (g->simple == f->simple && sameParams(g, ps) && g->isStatic == f->isStatic)
                error("metodo " + f->signature() + " gia' dichiarato", n->pos);
            owner->methods.push_back(f);
        }
    } else {
        NsEntry& e = ns->members[nm];
        if (e.kind != NsEntry::None && e.kind != NsEntry::Functions) error("'" + nm + "' gia' dichiarato", n->pos);
        e.kind = NsEntry::Functions;
        for (Function* g : e.fns) {
            if (sameParams(g, ps)) {
                // prototipo e definizione della stessa funzione
                if (g->ret != f->ret) error("'" + nm + "' dichiarata con un tipo di ritorno diverso", n->pos);
                if (n->b) {
                    if (pendingBody(g)) error("funzione '" + nm + "' definita due volte", n->pos);
                    fnDecls_[g] = n;
                    pending_.push_back({g, n, n->b, ns, lang_, unit_, tbind_});
                    g->defined = true;
                }
                prog_.functions.pop_back();
                return g;
            }
            if (lang_ == Dialect::C) error("in C non si possono avere due funzioni '" + nm + "' (overload: usa il C++)", n->pos);
        }
        e.fns.push_back(f);
    }
    if (n->mods & MDeleted) { f->defined = true; deletedFns_.insert(f); return f; }
    if (n->mods & MDefault) { f->defined = true; defaultedFns_.insert(f); return f; }
    if (n->b) {
        pending_.push_back({f, n, n->b, ns, lang_, unit_, tbind_});
        f->defined = true;
    }
    return f;
}
bool Compiler::pendingBody(Function* f) const {
    for (const Pending& p : pending_) if (p.fn == f) return true;
    return false;
}

std::vector<Function*> Compiler::attachOutOfClass(ClassInfo* c, Node* n, Namespace* ns) {
    const Namespace* saveNs = ns_;
    ClassInfo* saveCls = cls_;
    cls_ = c;
    const std::string nm = syms_[n->sym];
    const std::vector<TypeP> ps = paramTypes(n);
    std::vector<Function*> out;
    Function* match = nullptr;
    if (n->op == 1 || nm == c->name) {
        for (Function* g : c->ctors) if (sameParams(g, ps)) match = g;
        if (!match) error("costruttore " + c->name + "(...) non dichiarato nella classe", n->pos);
    } else if (n->op == 2 || (!nm.empty() && nm[0] == '~')) {
        match = c->dtor;
        if (!match) error("distruttore non dichiarato nella classe " + c->name, n->pos);
    } else {
        for (Function* g : c->methods) if (g->simple == nm && sameParams(g, ps)) match = g;
        if (!match) error("metodo " + c->name + "::" + nm + " non dichiarato nella classe", n->pos);
    }
    if (match->defined && pendingBody(match)) error("metodo " + match->name + " definito due volte", n->pos);
    if (n->b) {
        // i valori predefiniti restano quelli della dichiarazione
        Node* decl = fnDecls_[match];
        Node* merged = pool_.node(K::Func, n->pos);
        *merged = *n;
        if (decl) for (std::size_t i = 0; i < merged->list.size() && i < decl->list.size(); ++i) if (!merged->list[i]->a && decl->list[i]->a) {
            Node* p = pool_.node(K::Param, merged->list[i]->pos);
            *p = *merged->list[i];
            p->a = decl->list[i]->a;
            merged->list[i] = p;
        }
        pending_.push_back({match, merged, n->b, ns, lang_, unit_, tbind_});
        match->defined = true;
        match->file = n->pos.file;
        match->line = static_cast<int>(n->pos.line);
        match->column = n->pos.col;
    }
    out.push_back(match);
    ns_ = const_cast<Namespace*>(saveNs);
    cls_ = saveCls;
    return out;
}

// Variabili globali, campi static e campi d'istanza.
void Compiler::declareVariables(Namespace* ns, Node* n, ClassInfo* owner) {
    for (Node* d : n->list) {
        if (d->k != K::Declarator) continue;
        const std::string nm = syms_[d->sym];
        TypeP t = resolveType(d->type, false);
        const bool isConst = (d->mods & (MConst | MConstexpr)) != 0 || (d->type && d->type->isConst);
        const bool cs = lang_ == Dialect::CSharp || lang_ == Dialect::Java;
        if (t->k == TK::Dynamic && t->size == -2 && !d->a) error("auto/var vuole un valore iniziale", d->pos);
        if (t->k == TK::Void) error("variabile di tipo void", d->pos);
        const bool isStatic = !owner || (d->mods & MStatic) != 0 || (cs && (d->mods & MConst));
        if (owner && !isStatic) {
            if (t->k == TK::Dynamic && t->size == -2) error("auto non e' ammesso per i campi", d->pos);
            if (t->k == TK::Array && t->size == -4) error("un campo array vuole una dimensione costante", d->pos);
            // per valore (C/C++, struct di C#) una classe non contiene se stessa; i riferimenti di C# e Java si'
            if (t->k == TK::Class && t->cls == owner && owner->valueType) error("la classe " + owner->name + " non puo' contenere se stessa (usa un puntatore)", d->pos);
            FieldInfo f;
            f.name = nm;
            f.type = t;
            f.owner = owner;
            f.isConst = isConst;
            f.isReadonly = (d->mods & MReadonly) != 0;
            f.access = (d->mods & MPrivate) ? Access::Private : (d->mods & MProtected) ? Access::Protected : (d->mods & MPublic) ? Access::Public :
                       (cs && lang_ == Dialect::CSharp) ? Access::Private : Access::Public;
            if (d->mods & MInternal) f.access = Access::Public;
            f.line = static_cast<int>(d->pos.line);
            f.column = d->pos.col;
            for (const FieldInfo& g : owner->fields) if (g.name == nm) error("campo '" + nm + "' gia' dichiarato", d->pos);
            owner->fields.push_back(f);
            if (d->a || !d->list.empty()) fieldInits_[owner].push_back(d);
            continue;
        }
        // globale o static
        auto g = std::make_unique<GlobalVar>();
        g->name = owner ? owner->name + "::" + nm : qualify(ns, nm);
        g->type = t;
        g->isConst = isConst;
        g->slot = static_cast<std::int32_t>(prog_.globalNames.size());
        prog_.globalNames.push_back(g->name);
        prog_.globalTypes.push_back(t);
        prog_.globalFile.push_back(d->pos.file);
        // costante intera di compilazione: const int N = 10;
        std::int64_t cv = 0;
        if (isConst && d->a && isIntegral(t) && constInt(d->a, cv)) { g->hasConstValue = true; g->constValue = Value::ofInt(vkOf(t), cv); }
        Namespace* scopeNs = owner ? classScope_[owner] : ns;
        NsEntry& e = scopeNs->members[nm];
        if (e.kind == NsEntry::Global) {
            // extern int x; ... int x = 1; (stesso globale)
            if (e.global->type != t) error("'" + nm + "' dichiarata con tipi diversi", d->pos);
            if (d->a) inits_.push_back({d, e.global, ns, lang_, unit_, owner});
            prog_.globalNames.pop_back(); prog_.globalTypes.pop_back(); prog_.globalFile.pop_back();
            continue;
        }
        if (e.kind != NsEntry::None) error("'" + nm + "' gia' dichiarato", d->pos);
        e.kind = NsEntry::Global;
        e.global = g.get();
        if (owner) { FieldInfo f; f.name = nm; f.type = t; f.isStatic = true; f.isConst = isConst; f.slot = g->slot; f.owner = owner;
                     f.access = (d->mods & MPrivate) ? Access::Private : (d->mods & MProtected) ? Access::Protected : (d->mods & MPublic) ? Access::Public :
                                (cs && lang_ == Dialect::CSharp) ? Access::Private : Access::Public;
                     owner->fields.push_back(f); }
        inits_.push_back({d, g.get(), ns, lang_, unit_, owner});
        globals_.push_back(std::move(g));
    }
}

// ---------------------------------------------------------------- classi
void Compiler::declareEnum(ClassInfo* c, Node* n) {
    c->underlying = n->type ? resolveType(n->type, false) : tI32_;
    if (!isIntegral(c->underlying)) error("il tipo di un enum deve essere intero", n->pos);
    classType(c);
    std::int64_t next = 0;
    Namespace* scope = classScope_[c];
    Namespace* outerScope = c->outer ? classScope_[c->outer] : classNs_[c];
    const bool unscoped = (lang_ == Dialect::C || lang_ == Dialect::Cpp) && n->op == 4;
    for (Node* e : n->list) {
        if (e->a) {
            std::int64_t v = 0;
            if (!constInt(e->a, v)) error("il valore di un enum deve essere una costante intera", e->pos);
            next = v;
        }
        const std::string nm = syms_[e->sym];
        for (const auto& ev : c->enumValues) if (ev.first == nm) error("valore '" + nm + "' gia' nell'enum", e->pos);
        c->enumValues.push_back({nm, next});
        NsEntry ent;
        ent.kind = NsEntry::EnumConst;
        ent.cls = c;
        ent.enumValue = next;
        ent.type = lang_ == Dialect::C ? tI32_ : c->self;
        scope->members[nm] = ent;
        if (unscoped) {
            NsEntry& o = outerScope->members[nm];
            if (o.kind != NsEntry::None && o.kind != NsEntry::EnumConst) error("'" + nm + "' gia' dichiarato", e->pos);
            o = ent;
        }
        ++next;
    }
}

void Compiler::declareProperty(ClassInfo* c, Node* n) {
    PropertyInfo p;
    const bool indexer = n->op == 2;
    p.name = indexer ? "this" : syms_[n->sym];
    p.type = resolveType(n->type, false);
    p.isStatic = (n->mods & MStatic) != 0;
    p.isIndexer = indexer;
    p.owner = c;
    p.access = (n->mods & MPrivate) ? Access::Private : (n->mods & MProtected) ? Access::Protected : (n->mods & (MPublic | MInternal)) ? Access::Public :
               c->isInterface ? Access::Public : Access::Private;
    const std::uint32_t setMods = static_cast<std::uint32_t>(n->dval);
    p.setAccess = (setMods & MPrivate) ? Access::Private : (setMods & MProtected) ? Access::Protected : p.access;
    const bool hasGet = (n->ival & 1) != 0 || n->a;
    const bool hasSet = (n->ival & 2) != 0;
    const bool autoProp = n->op == 1 && !c->isInterface && !(n->mods & MAbstract);
    std::string backing;
    if (autoProp) {
        backing = "<" + p.name + ">k__BackingField";
        Node* fd = pool_.node(K::Field, n->pos);
        fd->mods = (p.isStatic ? static_cast<std::uint32_t>(MStatic) : 0u) | static_cast<std::uint32_t>(MPrivate);
        Node* d = pool_.node(K::Declarator, n->pos);
        d->sym = syms_.intern(backing);
        d->type = n->type;
        d->mods = fd->mods;
        if (n->c) { d->op = 1; d->a = n->c; }
        fd->list.push_back(d);
        declareVariables(classNs_[c], fd, c);
        p.backingName = backing;                         // { get; } si assegna nel costruttore
    } else if (n->c) {
        error("solo le proprieta' automatiche hanno un valore iniziale", n->pos);
    }
    auto accessor = [&](bool get) -> Function* {
        // metodo get_X / set_X (come .NET): virtuale come la proprieta'
        Node* fn = pool_.node(K::Func, n->pos);
        fn->sym = syms_.intern((get ? "get_" : "set_") + (indexer ? std::string("Item") : p.name));
        if (get) fn->type = n->type;
        else {
            TypeNode* vt = pool_.type(TF::Named, n->pos);
            vt->name = syms_.intern("void");
            vt->builtin = true;
            fn->type = vt;
        }
        fn->mods = n->mods & (MStatic | MVirtual | MOverride | MAbstract | MSealed | MPublic | MPrivate | MProtected | MInternal | MNew);
        if (!get && (setMods & (MPrivate | MProtected))) { fn->mods &= ~(MPublic | MInternal); fn->mods |= setMods & (MPrivate | MProtected); }
        for (Node* ip : n->list) fn->list.push_back(ip);
        if (!get) {
            Node* v = pool_.node(K::Param, n->pos);
            v->sym = syms_.intern("value");
            v->type = n->type;
            fn->list.push_back(v);
        }
        Node* body = get ? n->a : n->b;
        if (autoProp) {
            body = pool_.node(K::Block, n->pos);
            Node* target = pool_.node(K::Name, n->pos);
            target->sym = syms_.intern(backing);
            if (get) { Node* r = pool_.node(K::Return, n->pos); r->a = target; body->list.push_back(r); }
            else {
                Node* as = pool_.node(K::Assign, n->pos);
                as->op = Assign;
                as->a = target;
                Node* v = pool_.node(K::Name, n->pos);
                v->sym = syms_.intern("value");
                as->b = v;
                Node* st = pool_.node(K::ExprStmt, n->pos);
                st->a = as;
                body->list.push_back(st);
            }
        }
        if (body && body->k == K::Empty) body = nullptr;
        fn->b = body;
        if (!body && !c->isInterface && !(n->mods & MAbstract)) error("accessor senza corpo", n->pos);
        if (!body) fn->mods |= MAbstract;
        Function* f = declareFunction(classNs_[c], fn, c);
        f->access = get ? p.access : p.setAccess;
        return f;
    };
    if (hasGet) p.getter = accessor(true);
    if (hasSet) p.setter = accessor(false);
    if (!hasGet && !hasSet) error("proprieta' senza get ne' set", n->pos);
    for (const PropertyInfo& q : c->properties) if (q.name == p.name && !indexer) error("proprieta' '" + p.name + "' gia' dichiarata", n->pos);
    c->properties.push_back(p);
}

void Compiler::completeClass(ClassInfo* c) {
    if (!c || c->complete || c->isNative) return;
    if (completing_.count(c)) error("la classe " + c->name + " dipende da se stessa", Pos{static_cast<std::uint16_t>(c->file), static_cast<std::uint16_t>(c->column), static_cast<std::uint32_t>(c->line)});
    auto nodeIt = classNodes_.find(c);
    if (nodeIt == classNodes_.end() || !nodeIt->second) {
        if (c->isEnum) return;
        return;                                          // dichiarata ma non definita: si segnala all'uso
    }
    Node* n = nodeIt->second;
    completing_.insert(c);
    // contesto della dichiarazione (salvato e rimesso alla fine)
    const Dialect saveLang = lang_;
    Namespace* saveNs = ns_;
    ClassInfo* saveCls = cls_;
    const std::uint16_t saveUnit = unit_;
    const std::int32_t saveLib = library_;
    auto saveTbind = tbind_;
    auto saveHeaders = headers_;
    const UnitCtx uc = classUnit_[c];
    enterUnitContext(uc.lang, uc.unit, uc.library, uc.ns, c);
    if (classTbind_.count(c)) tbind_ = classTbind_[c];
    if (c->isEnum) {
        declareEnum(c, n);
        c->complete = true;
    } else {
        // basi
        for (Node* b : n->list2) {
            // C#/Java: IComparable<T>, IEquatable<T>, IDisposable (Comparable<T>...) dichiarano
            // solo metodi che il nucleo trova per nome (CompareTo, Equals, Dispose): niente da collegare
            if ((lang_ == Dialect::CSharp || lang_ == Dialect::Java) && b->type && b->type->name) {
                const std::string bn = syms_[b->type->name];
                static const char* const markers[] = {"IComparable", "IEquatable", "IDisposable", "Comparable", "Cloneable", "Serializable", "AutoCloseable"};
                bool marker = false;
                for (const char* m : markers) if (bn == m) marker = true;
                if (marker) {
                    bool known = true;
                    try { (void)resolveType(b->type, false); } catch (const Error&) { known = false; }
                    if (!known) continue;
                }
            }
            TypeP bt = resolveType(b->type, false);
            if (bt->k != TK::Class || !bt->cls) error("la base deve essere una classe", b->pos);
            ClassInfo* bc = bt->cls;
            completeClass(bc);
            if (!bc->complete && !bc->isNative) error("la classe base " + bc->name + " non e' definita", b->pos);
            if (bc->isInterface) {
                if (!c->isInterface && lang_ != Dialect::CSharp && lang_ != Dialect::Java) error("le interfacce sono di C# e Java", b->pos);
                c->interfaces.push_back(bc);
            } else {
                if (c->isInterface) error("un'interfaccia eredita solo da interfacce", b->pos);
                if (c->base) error("ereditarieta' multipla non supportata", b->pos);
                if (bc->isSealed) error("la classe " + bc->name + " e' sealed/final", b->pos);
                if (c->isStruct && lang_ == Dialect::CSharp) error("una struct di C# non eredita da una classe", b->pos);
                c->base = bc;
                if ((b->mods & MPrivate) || (lang_ == Dialect::Cpp && !c->isStruct && !(b->mods & (MPublic | MProtected)))) privateBase_.insert(c);
            }
        }
        if (c->base) { c->isException = c->base->isException; c->valueType = c->valueType || (c->base->valueType && lang_ == Dialect::Cpp); }
        std::vector<Node*> members = n->list;
        for (Node* part : partialParts_[c]) for (Node* m : part->list) members.push_back(m);
        for (Node* part : partialParts_[c]) for (Node* b : part->list2) { (void)b; }
        for (Node* m : members) {
            switch (m->k) {
            case K::Field: declareVariables(classNs_[c], m, c); break;
            case K::Func: declareFunction(classNs_[c], m, c); break;
            case K::Property: declareProperty(c, m); break;
            case K::Typedef: {
                TypeP t = resolveType(m->type, true);
                NsEntry e; e.kind = NsEntry::Typedef; e.type = t;
                classScope_[c]->members[syms_[m->sym]] = e;
                break;
            }
            case K::Using: break;
            case K::Friend:
                if (m->a && m->a->k == K::Func) {
                    // funzione amica: globale, con accesso ai membri
                    Function* f = declareFunction(classNs_[c] ? classNs_[c] : root_.get(), m->a, nullptr, true);
                    if (f) friendFns_[c].insert(f);
                } else if (m->sym) {
                    friendNames_[c].insert(syms_[m->sym]);
                }
                cls_ = c;
                break;
            case K::Class: case K::Template: break;   // gia' dichiarate
            case K::Empty: break;
            default: error("membro non valido nella classe", m->pos);
            }
            cls_ = c;
        }
        layoutClass(c);
        c->complete = true;
        bool dtor = c->dtor != nullptr || (c->base && c->base->hasDtorChain);
        for (const FieldInfo& f : c->fields) if (!f.isStatic && needsDtor(f.type)) dtor = true;
        c->hasDtorChain = dtor && lang_ == Dialect::Cpp;
        buildVtable(c);
        // classi annidate
        for (Node* m : members) if (m->k == K::Class) {
            NsEntry* e = lookupScoped(classScope_[c], syms_[m->sym]);
            if (e && e->kind == NsEntry::Class) completeClass(e->cls);
        }
    }
    completing_.erase(c);
    lang_ = saveLang; ns_ = saveNs; cls_ = saveCls; unit_ = saveUnit; library_ = saveLib; tbind_ = saveTbind; headers_ = saveHeaders;
}

void Compiler::layoutClass(ClassInfo* c) {
    std::int32_t slot = c->base ? c->base->slotCount : 0;
    for (FieldInfo& f : c->fields) if (!f.isStatic) f.slot = slot++;
    c->slotCount = slot;
    if (slot > 4096) error("troppi campi nella classe " + c->name, Pos{});
}

void Compiler::buildVtable(ClassInfo* c) {
    if (c->base) c->vtable = c->base->vtable;
    auto params = [](const Function* f) { std::vector<TypeP> v; for (const Param& p : f->params) v.push_back(p.type); return v; };
    for (Function* m : c->methods) {
        if (m->isStatic || m->isCtor) continue;
        // un metodo con lo stesso nome e gli stessi parametri della base: override
        bool found = false;
        for (std::size_t i = 0; i < c->vtable.size(); ++i) {
            Function* b = c->vtable[i];
            if (b->simple == m->simple && sameParams(b, params(m))) {
                if (m->lang == Dialect::CSharp && !m->isOverride && !(fnDecls_[m] && (fnDecls_[m]->mods & MNew))) {
                    // C#: senza override il metodo nasconde (new): slot nuovo
                    if (b->isAbstract) error("manca override per il metodo astratto " + b->name, Pos{static_cast<std::uint16_t>(m->file), static_cast<std::uint16_t>(m->column), static_cast<std::uint32_t>(m->line)});
                    break;
                }
                if (fnDecls_[m] && (fnDecls_[m]->mods & MNew)) break;
                if (b->ret != m->ret && !(b->ret && m->ret && b->ret->k == TK::Ptr && m->ret->k == TK::Ptr) &&
                    !(b->ret && m->ret && b->ret->k == TK::Class && m->ret->k == TK::Class && m->ret->cls->derivesFrom(b->ret->cls)))
                    error("l'override di " + b->name + " ha un tipo di ritorno diverso", Pos{static_cast<std::uint16_t>(m->file), static_cast<std::uint16_t>(m->column), static_cast<std::uint32_t>(m->line)});
                c->vtable[i] = m;
                m->vslot = static_cast<std::int32_t>(i);
                m->isVirtual = true;
                found = true;
                break;
            }
        }
        // C#/Java: override dei metodi virtuali di object (ToString, Equals, GetHashCode)
        const bool objectVirtual = (m->lang == Dialect::CSharp && ((m->simple == "ToString" && m->params.empty()) || (m->simple == "Equals" && m->params.size() == 1) ||
                                                                   (m->simple == "GetHashCode" && m->params.empty()))) ||
                                   (m->lang == Dialect::Java && ((m->simple == "toString" && m->params.empty()) || (m->simple == "equals" && m->params.size() == 1) ||
                                                                 (m->simple == "hashCode" && m->params.empty())));
        if (!found && m->isOverride && objectVirtual) m->isVirtual = true;
        else if (!found && m->isOverride && !c->isInterface) {
            // override di un metodo di un'interfaccia o di una base senza virtual
            bool ok = false;
            for (ClassInfo* b = c->base; b && !ok; b = b->base) for (Function* g : b->methods) if (g->simple == m->simple && sameParams(g, params(m))) ok = true;
            error(std::string(ok ? "il metodo della base non e' virtual: " : "nessun metodo da sovrascrivere: ") + m->signature(),
                  Pos{static_cast<std::uint16_t>(m->file), static_cast<std::uint16_t>(m->column), static_cast<std::uint32_t>(m->line)});
        }
        if (!found && m->isVirtual) {
            m->vslot = static_cast<std::int32_t>(c->vtable.size());
            c->vtable.push_back(m);
        }
    }
    if (c->dtor && c->dtor->isVirtual) {}
    if (c->base && c->base->dtor && c->base->dtor->isVirtual && c->dtor) c->dtor->isVirtual = true;
    // classe astratta: metodi astratti rimasti
    for (Function* v : c->vtable) if (v->isAbstract) c->isAbstract = true;
    // interfacce: selettori nome/arita' -> implementazione
    std::vector<ClassInfo*> ifaces;
    for (ClassInfo* k = c; k; k = k->base) for (ClassInfo* i : k->interfaces) ifaces.push_back(i);
    for (std::size_t q = 0; q < ifaces.size(); ++q) for (ClassInfo* sub : ifaces[q]->interfaces) ifaces.push_back(sub);
    if (c->base) c->selectors = c->base->selectors;
    for (Function* m : c->methods) if (!m->isStatic && !m->isCtor) c->selectors[m->simple + "/" + std::to_string(m->params.size())] = m;
    for (const Function* v : c->vtable) c->selectors[v->simple + "/" + std::to_string(v->params.size())] = c->vtable[static_cast<std::size_t>(v->vslot)];
    if (!c->isInterface) {
        for (ClassInfo* i : ifaces) {
            for (Function* im : i->methods) {
                if (im->isStatic) continue;
                const std::string key = im->simple + "/" + std::to_string(im->params.size());
                auto it = c->selectors.find(key);
                Function* impl = it == c->selectors.end() ? nullptr : it->second;
                if (impl && impl->owner && impl->owner->isInterface) impl = nullptr;
                if (!impl) {
                    if (!im->isAbstract && im->defined) { c->selectors[key] = im; continue; }   // metodo default di un'interfaccia
                    if (c->isAbstract) continue;
                    error("la classe " + c->name + " non implementa " + i->name + "." + im->simple,
                          Pos{static_cast<std::uint16_t>(c->file), static_cast<std::uint16_t>(c->column), static_cast<std::uint32_t>(c->line)});
                }
                if (impl->access != Access::Public && lang_ == Dialect::CSharp)
                    error("il metodo " + impl->name + " che implementa " + i->name + " deve essere public",
                          Pos{static_cast<std::uint16_t>(impl->file), static_cast<std::uint16_t>(impl->column), static_cast<std::uint32_t>(impl->line)});
            }
        }
    }
}

void Compiler::finishClasses() {
    for (std::size_t i = 0; i < classOrder_.size(); ++i) {
        ClassInfo* c = classOrder_[i];
        completeClass(c);
        if (!c->complete && classNodes_.count(c) && classNodes_[c]) error("classe " + c->name + " incompleta", Pos{});
    }
}

// ---------------------------------------------------------------- template
ClassInfo* Compiler::instantiateClass(TemplateDef* t, const std::vector<TypeP>& argsIn, Pos at) {
    Node* tn = t->node;
    std::vector<TypeP> args = argsIn;
    const std::size_t np = tn->list.size();
    if (args.size() > np) error("troppi argomenti per il template " + t->qualified, at);
    // argomenti predefiniti
    for (std::size_t i = args.size(); i < np; ++i) {
        Node* p = tn->list[i];
        if (p->op == 0 && p->type) args.push_back(resolveType(p->type, false));
        else error("mancano argomenti per il template " + t->qualified, at);
    }
    const std::string key = templateKey(args);
    auto it = t->classes.find(key);
    if (it != t->classes.end()) return it->second;
    if (++instantiations_ > 256) error("troppe istanze di template", at);
    std::string nm = syms_[tn->a->sym] + "<";
    for (std::size_t i = 0; i < args.size(); ++i) nm += (i ? ", " : "") + typeName(args[i]);
    nm += ">";
    ClassInfo* c = newClass(nm, t->lang);
    t->classes[key] = c;
    c->typeArgs = args;
    Namespace* declNs = t->owner ? classScope_[t->owner] : root_.get();
    // namespace della dichiarazione del template
    {
        const std::size_t sep = t->qualified.rfind("::");
        if (sep != std::string::npos) declNs = namespaceFor(root_.get(), t->qualified.substr(0, sep), true);
    }
    c->qualified = qualify(declNs, nm);
    std::vector<std::pair<std::string, TypeP>> bind;
    for (std::size_t i = 0; i < np; ++i) {
        Node* p = tn->list[i];
        if (p->op == 1) {
            // parametro intero: si lega come costante (tipo segnaposto con la dimensione)
            bind.push_back({syms_[p->sym], args[i]});
        } else bind.push_back({syms_[p->sym], args[i]});
    }
    classTbind_[c] = bind;
    Node* cn = tn->a;
    classNodes_[c] = cn;
    classUnit_[c] = {t->lang, t->unitIndex, t->library, declNs};
    classNs_[c] = declNs;
    auto scope = std::make_unique<Namespace>();
    scope->name = c->qualified;
    scope->parent = declNs;
    classScope_[c] = scope.get();
    declNs->children.push_back(std::move(scope));
    c->isStruct = cn->op == 1;
    c->isInterface = cn->op == 3;
    c->valueType = t->lang == Dialect::Cpp || t->lang == Dialect::C || (c->isStruct && t->lang == Dialect::CSharp);
    c->isAbstract = (cn->mods & MAbstract) != 0 || c->isInterface;
    c->outer = t->owner;
    c->line = static_cast<int>(cn->pos.line);
    c->column = cn->pos.col;
    c->file = cn->pos.file;
    classOrder_.push_back(c);
    // nomi del template dentro la classe: Pila (senza argomenti) = questa istanza
    NsEntry self;
    self.kind = NsEntry::Class;
    self.cls = c;
    classScope_[c]->members[syms_[cn->sym]] = self;
    // tipi annidati
    const Dialect saveLang = lang_;
    const std::uint16_t saveUnit = unit_;
    const std::int32_t saveLib = library_;
    lang_ = t->lang; unit_ = t->unitIndex; library_ = t->library;
    for (Node* m : cn->list) if (m->k == K::Class) declareClass(declNs, m, c);
    lang_ = saveLang; unit_ = saveUnit; library_ = saveLib;
    completeClass(c);
    // membri definiti fuori dalla classe: template<class T> T Pila<T>::f()
    for (const OutOfClass& o : pendingOutOfClass_) {
        Node* f = o.tmpl->node->a;
        if (!f->type2 || syms_[f->type2->name] != syms_[cn->sym]) continue;
        // i nomi dei parametri della definizione possono essere diversi: si legano agli argomenti
        std::vector<std::pair<std::string, TypeP>> b2;
        for (std::size_t i = 0; i < o.tmpl->node->list.size() && i < args.size(); ++i) b2.push_back({syms_[o.tmpl->node->list[i]->sym], args[i]});
        auto saveT = tbind_;
        auto saveL = lang_;
        auto saveU = unit_;
        tbind_ = b2;
        lang_ = o.tmpl->lang;
        unit_ = o.tmpl->unitIndex;
        Node* copy = pool_.node(K::Func, f->pos);
        *copy = *f;
        copy->type2 = nullptr;
        attachOutOfClass(c, copy, o.ns);
        tbind_ = saveT;
        lang_ = saveL;
        unit_ = saveU;
    }
    return c;
}

Function* Compiler::instantiateFunction(TemplateDef* t, const std::vector<TypeP>& args, Pos at) {
    Node* tn = t->node;
    if (args.size() != tn->list.size()) error("argomenti del template " + t->qualified + " non dedotti", at);
    const std::string key = templateKey(args);
    auto it = t->functions.find(key);
    if (it != t->functions.end()) return it->second[0];
    if (++instantiations_ > 512) error("troppe istanze di template", at);
    auto saveT = tbind_;
    const Dialect saveLang = lang_;
    const std::uint16_t saveUnit = unit_;
    const std::int32_t saveLib = library_;
    Namespace* saveNs = ns_;
    ClassInfo* saveCls = cls_;
    std::vector<std::pair<std::string, TypeP>> bind = t->owner && classTbind_.count(t->owner) ? classTbind_[t->owner] : std::vector<std::pair<std::string, TypeP>>{};
    for (std::size_t i = 0; i < args.size(); ++i) bind.push_back({syms_[tn->list[i]->sym], args[i]});
    tbind_ = bind;
    lang_ = t->lang; unit_ = t->unitIndex; library_ = t->library;
    Namespace* declNs = root_.get();
    const std::size_t sep = t->qualified.rfind("::");
    if (sep != std::string::npos) declNs = namespaceFor(root_.get(), t->qualified.substr(0, sep), true);
    if (t->owner) declNs = classNs_[t->owner];
    ns_ = declNs;
    Node* fn = pool_.node(K::Func, tn->a->pos);
    *fn = *tn->a;
    // il nome dell'istanza non va nel namespace: si chiama solo attraverso il template
    Namespace scratch;
    scratch.parent = declNs;
    scratch.name = declNs->name;
    Function* f = declareFunction(t->owner ? declNs : &scratch, fn, t->owner);
    if (!f) error("istanza del template non valida", at);
    std::string nm = f->name + "<";
    for (std::size_t i = 0; i < args.size(); ++i) nm += (i ? ", " : "") + typeName(args[i]);
    f->name = nm + ">";
    // il corpo si compila nel namespace vero
    for (Pending& p : pending_) if (p.fn == f) { p.ns = declNs; p.tbind = bind; }
    t->functions[key].assign(1, f);                 // non = {f}: falso positivo di -Warray-bounds (GCC 13, sanitizer)
    tbind_ = saveT; lang_ = saveLang; unit_ = saveUnit; library_ = saveLib; ns_ = saveNs; cls_ = saveCls;
    return f;
}

// ---------------------------------------------------------------- ingresso
void Compiler::setupEntry() {
    Function* entry = nullptr;
    const Dialect d = prog_.dialect;
    if (d == Dialect::C || d == Dialect::Cpp) {
        NsEntry* e = lookupScoped(root_.get(), "main");
        if (!e || e->kind != NsEntry::Functions || e->fns.empty()) {
            const Pos at = units_.empty() ? Pos{} : units_[0].root->pos;
            error("manca la funzione main()", at);
        }
        entry = e->fns[0];
        if (e->fns.size() > 1) error("main definita piu' volte", Pos{});
    } else {
        for (const auto& c : prog_.classes) {
            if (c->isNative) continue;
            for (Function* m : c->methods) {
                if (m->simple != (d == Dialect::Java ? "main" : "Main") || !m->isStatic) continue;
                if (entry && d == Dialect::CSharp) error("piu' metodi Main: uno solo per programma", Pos{static_cast<std::uint16_t>(m->file), static_cast<std::uint16_t>(m->column), static_cast<std::uint32_t>(m->line)});
                if (!entry) entry = m;
            }
        }
        if (!entry) error(d == Dialect::Java ? "manca public static void main(String[] args)" : "manca static void Main()", Pos{});
    }
    if (!entry->defined) error("main dichiarata ma non definita", Pos{static_cast<std::uint16_t>(entry->file), static_cast<std::uint16_t>(entry->column), static_cast<std::uint32_t>(entry->line)});
    prog_.entry = entry->index;
    prog_.entryArgs = static_cast<std::int32_t>(entry->params.size());
    if (entry->params.size() > 2) error("main con troppi parametri", Pos{static_cast<std::uint16_t>(entry->file), static_cast<std::uint16_t>(entry->column), static_cast<std::uint32_t>(entry->line)});
    prog_.entryReturnsInt = isIntegral(entry->ret);
    if (entry->ret->k != TK::Void && !isIntegral(entry->ret)) error("main deve restituire int o void", Pos{static_cast<std::uint16_t>(entry->file), static_cast<std::uint16_t>(entry->column), static_cast<std::uint32_t>(entry->line)});
}

// ---------------------------------------------------------------- corpi
void Compiler::compileBodies() {
    // inizializzazione dei globali e dei campi static (in ordine di dichiarazione)
    Function* init = newFunction("<init>", prog_.dialect, nullptr);
    init->defined = true;
    prog_.init = init->index;
    initPending_ = true;
    for (std::size_t i = 0; i < pending_.size(); ++i) {
        if (++polls_ % 16 == 0 && poll_) poll_();
        Pending p = pending_[i];
        enterUnitContext(p.lang, p.unit, p.fn->library, p.ns, p.fn->owner);
        tbind_ = p.tbind;
        if (p.fn->owner && classTbind_.count(p.fn->owner) && tbind_.empty()) tbind_ = classTbind_[p.fn->owner];
        compileFunctionBody(p.fn, p.body, p.decl);
    }
    // corpo dell'inizializzazione
    enterUnitContext(prog_.dialect, 0, -1, root_.get(), nullptr);
    beginFunction(init);
    for (const GlobalInit& gi : inits_) {
        enterUnitContext(gi.lang, gi.unit, gi.unit < units_.size() ? units_[gi.unit].library : -1, gi.ns, gi.cls);
        if (gi.cls && classTbind_.count(gi.cls)) tbind_ = classTbind_[gi.cls];
        fn_->lang = gi.lang;
        setPos(gi.decl->pos);
        Node* d = gi.decl;
        TypeP t = gi.g->type;
        if (t->k == TK::Dynamic && t->size == -2) {
            TypeP vt = rvalue(d->a, nullptr);
            gi.g->type = vt;
            prog_.globalTypes[static_cast<std::size_t>(gi.g->slot)] = vt;
            emit(Op::StoreG, gi.g->slot);
            continue;
        }
        if (d->a && d->a->k == K::InitList) {
            initFromList(t, d->a, d->pos);
            emit(Op::StoreG, gi.g->slot);
        } else if (d->a) {
            EV e = expr(d->a);
            load(e, d->pos);
            convert(e.type, t, d->pos, false, &e);
            copyIfLvalue(e, t, d->pos);
            emit(Op::StoreG, gi.g->slot);
        } else if (!d->list.empty() || (t->k == TK::Class && t->cls && !t->cls->isEnum) || isValueType(t)) {
            if (t->k == TK::Class && t->cls && t->cls->valueType) {
                completeClass(t->cls);
                constructObject(t->cls, d->list, d->pos, true);
            } else defaultInit(t, d->pos);
            emit(Op::StoreG, gi.g->slot);
        } else {
            zeroValue(t, d->pos);
            emit(Op::StoreG, gi.g->slot);
        }
    }
    // costruttori statici di C# (dopo gli inizializzatori static)
    for (const auto& c : prog_.classes) if (c->staticInit) { emit(Op::Call, c->staticInit->index, 0); }
    emit(Op::Ret, 0, 0);
    endFunction();
    // funzioni dichiarate, usate e mai definite
    for (Function* f : calledUndefined_) {
        if (f->defined || f->isAbstract || f->isNative) continue;
        error("funzione '" + f->name + "' dichiarata ma non definita", Pos{static_cast<std::uint16_t>(f->file), static_cast<std::uint16_t>(f->column), static_cast<std::uint32_t>(f->line)});
    }
}

} // namespace fx::prog
