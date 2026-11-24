// Compilatore dei programmi (v1.20.0), parte 5: chiamate, overload,
// template dedotti, funzioni native, operatori (anche dell'utente),
// assegnazioni, incrementi, condizionale, cast, new, lambda con le catture,
// stringhe interpolate, inizializzatori di C#, is/as.
#include "program_compile.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace fx::prog {
using namespace ast;

// ================================================================ chiamate
std::vector<TypeP> Compiler::argTypesFor(const std::vector<Node*>& args) {
    std::vector<TypeP> out;
    for (Node* a : args) {
        if (a->k == K::Lambda || a->k == K::InitList) { out.push_back(nullptr); continue; }
        EV e = exprType(a);
        out.push_back(e.type);
    }
    return out;
}

// Tipi segnaposto delle firme native (tipo o elemento di un argomento) -> tipi veri.
TypeP Compiler::substMarkers(TypeP t, const std::vector<EV>& probes) {
    if (!t) return t;
    if (t->k == TK::Dynamic && t->size <= -10 && t->size >= -15) {
        const bool elem = t->size <= -13;
        const std::size_t ai = static_cast<std::size_t>(elem ? -13 - t->size : -10 - t->size);
        TypeP at = ai < probes.size() && probes[ai].type ? stripRef(probes[ai].type) : tDyn_;
        if (!elem) return at->k == TK::Array && at->elem && (lang_ == Dialect::C || lang_ == Dialect::Cpp) ? ptrTo(at->elem) : at;
        if (at->k == TK::Array || at->k == TK::Ptr) return at->elem ? at->elem : tDyn_;
        if (at->k == TK::Str) return charType(lang_);
        if (at->k == TK::Native && !at->args.empty()) return at->args[0];
        return tDyn_;
    }
    if (t->k == TK::Ref) { TypeP e = substMarkers(t->elem, probes); return e == t->elem ? t : refTo(e); }
    if (t->k == TK::Func) {
        bool changed = false;
        std::vector<TypeP> ps;
        for (TypeP a : t->args) { TypeP b = substMarkers(a, probes); changed = changed || b != a; ps.push_back(b); }
        TypeP r = substMarkers(t->ret, probes);
        if (!changed && r == t->ret) return t;
        return funcOf(r, ps);
    }
    return t;
}

EV Compiler::call(Node* n) {
    Node* callee = n->a;
    // T(args) / T{args}: costruzione o conversione
    if (callee->k == K::TypeExpr) {
        TypeP t = resolveType(callee->type, false);
        return construct(t, n->list, n->pos, n->op == 1);
    }
    EV f;
    implicitThis_ = false;
    if (callee->k == K::Name) {
        // variabile con una funzione (puntatore, lambda, delegato)?
        f = name(callee);
    } else {
        f = expr(callee);
    }
    const bool selfRecv = implicitThis_;
    implicitThis_ = false;
    if (f.cat == EV::TypeName) {
        if (templateName_) error("argomenti del template mancanti", n->pos);
        return construct(f.type, n->list, n->pos, false);
    }
    if (f.cat == EV::FuncGroup) {
        std::vector<TypeP> targs;
        for (TypeNode* t : callee->targs) targs.push_back(resolveType(t, false));
        const bool recv = f.hasReceiver && !selfRecv;
        std::string what = callee->k == K::Name || callee->k == K::Member || callee->k == K::Arrow || callee->k == K::Scope ? syms_[callee->sym] : std::string("funzione");
        EV r = callOverloads(f.fns, f.natives, f.templates, n->list, targs, n->pos, recv, f.type, f.superCall, what);
        return r;
    }
    // valore funzione
    return callValue(f, n->list, n->pos);
}

EV Compiler::callValue(EV fv, const std::vector<Node*>& args, Pos at) {
    load(fv, at);
    TypeP t = stripRef(fv.type);
    if (t->k == TK::Ptr && t->elem && t->elem->k == TK::Func) t = t->elem;
    if (t->k != TK::Func && t->k != TK::Dynamic) error("chiamata di un valore che non e' una funzione (" + typeName(t) + ")", at);
    EV r;
    if (t->k == TK::Func) {
        if (args.size() != t->args.size()) error("la funzione vuole " + std::to_string(t->args.size()) + " argomenti", at);
        for (std::size_t i = 0; i < args.size(); ++i) {
            TypeP pt = t->args[i];
            if (pt->k == TK::Ref) { EV a = expr(args[i]); if (!a.lvalue) error("argomento per riferimento: serve una variabile", args[i]->pos); addressOfEV(a, args[i]->pos); }
            else rvalue(args[i], pt);
        }
        emit(Op::CallFn, 0, static_cast<std::uint8_t>(args.size()));
        r.type = t->ret ? t->ret : tDyn_;
    } else {
        for (Node* a : args) rvalue(a, nullptr);
        emit(Op::CallFn, 0, static_cast<std::uint8_t>(args.size()));
        r.type = tDyn_;
    }
    if (r.type->k == TK::Void) r.cat = EV::Void;
    if (r.type->k == TK::Ref) { r.cat = EV::Ptr; r.type = r.type->elem; r.lvalue = true; }
    return r;
}

// Deduzione di un argomento di template da un parametro.
bool Compiler::deduce(TypeNode* pt, TypeP at, std::vector<std::pair<std::string, TypeP>>& bind, const std::vector<std::string>& names) {
    if (!pt || !at) return true;
    at = stripRef(at);
    switch (pt->f) {
    case TF::Reference: return deduce(pt->inner, at, bind, names);
    case TF::Pointer:
        if (at->k == TK::Ptr || at->k == TK::Array) return deduce(pt->inner, at->elem, bind, names);
        return at->k == TK::Null;
    case TF::Array:
        if (at->k == TK::Array || at->k == TK::Ptr) return deduce(pt->inner, at->elem, bind, names);
        return false;
    case TF::Named: {
        const std::string& w = syms_[pt->name];
        if (pt->qual.empty() && pt->args.empty() && std::find(names.begin(), names.end(), w) != names.end()) {
            TypeP v = at;
            if (v->k == TK::Array && (lang_ == Dialect::Cpp || lang_ == Dialect::C)) v = ptrTo(v->elem);   // decadimento per valore
            for (auto& b : bind) if (b.first == w) {
                if (b.second == v) return true;
                if (b.second && v && isNumeric(b.second) && isNumeric(v) && lang_ != Dialect::Cpp) { b.second = arithmetic(b.second, v, pt->pos); return true; }
                return false;
            }
            bind.push_back({w, v});
            return true;
        }
        if (!pt->args.empty()) {
            if (at->k == TK::Native && at->args.size() == pt->args.size()) {
                for (std::size_t i = 0; i < pt->args.size(); ++i) if (!deduce(pt->args[i], at->args[i], bind, names)) return false;
                return true;
            }
            if (at->k == TK::Class && at->cls && at->cls->typeArgs.size() == pt->args.size()) {
                for (std::size_t i = 0; i < pt->args.size(); ++i) if (!deduce(pt->args[i], at->cls->typeArgs[i], bind, names)) return false;
                return true;
            }
        }
        return true;
    }
    default: return true;
    }
}

namespace {
struct Candidate {
    Function* fn{nullptr};
    std::int32_t native{-1};
    std::vector<TypeP> params;
    std::vector<bool> refs;
    bool variadic{false};
    TypeP ret{nullptr};
    int cost{0};
    std::size_t fixed{0};                                // parametri prima di ... / params
    bool expandParams{false};                            // C#: params in forma espansa
    bool isStatic{true};
};
} // namespace

EV Compiler::callOverloads(const std::vector<Function*>& fns, const std::vector<std::int32_t>& natives, const std::vector<TemplateDef*>& tmpls,
                           const std::vector<Node*>& args, const std::vector<TypeP>& targs, Pos at, bool receiverOnStack, TypeP receiver,
                           bool superCall, const std::string& what) {
    // tipi degli argomenti (compilazione di prova)
    std::vector<EV> probes;
    probes.reserve(args.size());
    for (Node* a : args) {
        EV p;
        if (a->k == K::Lambda) { p.cat = EV::FuncGroup; p.slot = static_cast<std::int32_t>(a->list.size()); p.type = nullptr; }
        else if (a->k == K::InitList) { p.type = nullptr; p.cat = EV::Void; }
        else if (a->k == K::Declarator) {
            // out var x / out int x
            p.type = resolveType(a->type, false);
            p.lvalue = true;
            p.cat = EV::Local;
        } else if (a->k == K::Name && a->sym == 0 && convTemp_ >= 0) { p.type = convTempType_; }
        else p = exprType(a);
        probes.push_back(p);
    }
    std::vector<Candidate> cands;
    auto argCost = [&](std::size_t i, TypeP pt, bool byRef) -> int {
        const EV& p = probes[i];
        Node* a = args[i];
        if (a->k == K::Lambda) {
            TypeP base = stripRef(substMarkers(pt, probes));
            if (base->k == TK::Func && base->args.size() == static_cast<std::size_t>(p.slot)) return 0;
            if (base->k == TK::Dynamic) return 2;
            if (base->k == TK::Ptr && base->elem && base->elem->k == TK::Func && base->elem->args.size() == static_cast<std::size_t>(p.slot)) return 0;
            return -1;
        }
        if (a->k == K::InitList) {
            TypeP base = stripRef(pt);
            return (base->k == TK::Class || base->k == TK::Native || base->k == TK::Array || (base->k == TK::Str && base->cppValue)) ? 2 : -1;
        }
        if (!p.type) return -1;
        if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) {
            const bool argRef = (a->mods & (MRef | MOut)) != 0 || a->k == K::Declarator;
            if (argRef != byRef) return -1;
            if (byRef && a->k == K::Declarator && p.type && p.type->k == TK::Dynamic && p.type->size == -2) return 0;   // out var x
            if (byRef) return stripRef(p.type) == stripRef(pt) ? 0 : -1;
        }
        if (byRef) {
            if (p.lvalue && stripRef(p.type) == stripRef(pt)) return 0;
            if (p.lvalue && stripRef(pt)->k == TK::Dynamic) return 1;      // nativi: any& (swap, cin >>)
            const TypeP at2 = stripRef(p.type), pt2 = stripRef(pt);
            if (p.lvalue && at2->k == TK::Class && pt2->k == TK::Class && at2->cls && pt2->cls && at2->cls->derivesFrom(pt2->cls)) return 1;
            const int c = convCost(p.type, pt2, &p);
            return c < 0 ? -1 : c + 1;                    // riferimento const a un temporaneo
        }
        if (p.cat == EV::FuncGroup && stripRef(pt)->k == TK::Func) return 1;   // nome di funzione come delegato
        if (p.cat == EV::FuncGroup && stripRef(pt)->k == TK::Ptr) return 1;
        if (p.cat == EV::FuncGroup && stripRef(pt)->k == TK::Dynamic) return 2;   // nativi: qsort(..., cmp), List.Sort(Confronta)
        if (p.cat == EV::FuncGroup) return -1;
        return convCost(p.type, pt, &p);
    };
    auto consider = [&](Candidate c) {
        const std::size_t np = c.params.size();
        std::size_t needed = np;
        // valori predefiniti
        if (c.fn) {
            needed = 0;
            for (std::size_t i = 0; i < c.fn->params.size(); ++i) if (!c.fn->params[i].hasDefault && !c.fn->params[i].isParams) needed = i + 1;
        }
        if (args.size() < needed) return;
        const bool paramsArray = c.fn && !c.fn->params.empty() && c.fn->params.back().isParams;
        if (args.size() > np && !c.variadic && !paramsArray) return;
        int total = 0;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i >= np || (paramsArray && i >= np - 1 && !(i == np - 1 && args.size() == np && probes[i].type && stripRef(probes[i].type)->k == TK::Array))) {
                if (paramsArray && i >= np - 1) {
                    TypeP et = c.params.back()->elem;
                    const int k = argCost(i, et, false);
                    if (k < 0) return;
                    total += k + 1;
                    c.expandParams = true;
                    continue;
                }
                if (!c.variadic) return;
                total += 2;
                continue;
            }
            const int k = argCost(i, c.params[i], c.refs.size() > i && c.refs[i]);
            if (k < 0) return;
            total += k;
        }
        c.cost = total;
        cands.push_back(c);
    };
    for (Function* f : fns) {
        Candidate c;
        c.fn = f;
        for (const Param& p : f->params) { c.params.push_back(p.type); c.refs.push_back(p.byRef); }
        c.ret = f->ret;
        c.isStatic = f->isStatic || f->isCtor == false ? f->isStatic : false;
        if (deletedFns_.count(f)) continue;
        consider(c);
    }
    for (TemplateDef* t : tmpls) {
        Node* tn = t->node;
        Node* fn = tn->a;
        std::vector<std::string> names;
        for (Node* p : tn->list) names.push_back(syms_[p->sym]);
        std::vector<std::pair<std::string, TypeP>> bind;
        for (std::size_t i = 0; i < targs.size() && i < names.size(); ++i) bind.push_back({names[i], targs[i]});
        bool ok = true;
        // i parametri dati in <...> non si deducono: gli argomenti si convertono (maxi<double>(2.5, 1))
        std::vector<std::string> deducible(names.begin() + static_cast<std::ptrdiff_t>(std::min(targs.size(), names.size())), names.end());
        // contesto del template per i tipi dei parametri
        auto saveT = tbind_;
        for (std::size_t i = 0; i < fn->list.size() && i < args.size() && ok; ++i) {
            if (!probes[i].type) continue;
            ok = deduce(fn->list[i]->type, probes[i].type, bind, deducible);
        }
        tbind_ = saveT;
        if (!ok) continue;
        std::vector<TypeP> targsAll;
        for (const std::string& nm : names) {
            TypeP v = nullptr;
            for (auto& b : bind) if (b.first == nm) v = b.second;
            if (!v) {
                Node* p = tn->list[targsAll.size()];
                if (p->type && p->op == 0) {
                    v = resolveType(p->type, false);
                }
            }
            if (!v) { ok = false; break; }
            targsAll.push_back(v);
        }
        if (!ok) continue;
        Function* inst = instantiateFunction(t, targsAll, at);
        Candidate c;
        c.fn = inst;
        for (const Param& p : inst->params) { c.params.push_back(p.type); c.refs.push_back(p.byRef); }
        c.ret = inst->ret;
        c.isStatic = inst->isStatic;
        consider(c);
    }
    for (std::int32_t idx : natives) {
        const NativeEntry& ne = nativeEntry(idx);
        Candidate c;
        c.native = idx;
        std::vector<bool> refs;
        c.ret = parseSig(ne.sig, c.params, c.variadic, receiver, lang_, &refs);
        c.refs = refs;
        c.isStatic = (ne.flags & NF::Static) != 0 || !ne.owner[0];
        consider(c);
    }
    if (cands.empty()) {
        std::string list;
        for (Function* f : fns) list += "\n  " + f->signature();
        if (fns.empty() && natives.empty() && tmpls.empty()) error("'" + what + "' non e' una funzione", at);
        std::string argsText;
        for (std::size_t i = 0; i < probes.size(); ++i) argsText += (i ? ", " : "") + (probes[i].type ? typeName(probes[i].type) : std::string(args[i]->k == K::Lambda ? "lambda" : "{...}"));
        if (fns.size() + natives.size() + tmpls.size() == 1) {
            const std::size_t want = !fns.empty() ? fns[0]->params.size() : 0;
            if (!fns.empty() && args.size() != want) error("'" + what + "' vuole " + std::to_string(want) + " argomenti, non " + std::to_string(args.size()), at);
            error("argomenti non compatibili con " + (fns.empty() ? what : fns[0]->signature()) + ": (" + argsText + ")", at);
        }
        error("nessuna versione di '" + what + "' accetta (" + argsText + ")" + list, at);
    }
    // la migliore: costo minimo; a parita' con firme diverse e' ambigua
    std::size_t best = 0;
    for (std::size_t i = 1; i < cands.size(); ++i) if (cands[i].cost < cands[best].cost) best = i;
    for (std::size_t i = 0; i < cands.size(); ++i) {
        if (i == best || cands[i].cost != cands[best].cost) continue;
        // un'istanza di template perde contro una funzione normale
        if (cands[best].fn && cands[i].fn) {
            bool bestT = cands[best].fn->name.find('<') != std::string::npos, iT = cands[i].fn->name.find('<') != std::string::npos;
            if (bestT && !iT) { best = i; continue; }
            if (!bestT && iT) continue;
        }
        if (cands[best].fn && cands[i].fn && cands[best].fn == cands[i].fn) continue;
        if (cands[best].native >= 0 && cands[i].native >= 0 && std::strcmp(nativeEntry(cands[best].native).sig, nativeEntry(cands[i].native).sig) == 0) continue;
        // C#: istanza contro static con lo stesso nome (non succede)
        error("chiamata ambigua di '" + what + "'" + (cands[best].fn ? ": " + cands[best].fn->signature() : std::string()) + (cands[i].fn ? " o " + cands[i].fn->signature() : std::string()), at);
    }
    Candidate& c = cands[best];
    if (c.native >= 0) for (TypeP& pt : c.params) pt = substMarkers(pt, probes);
    // ricevente
    Function* f = c.fn;
    const bool needsThis = f ? (f->hasThis || f->isCtor) : !c.isStatic;
    if (needsThis && !receiverOnStack) {
        if (f && f->isCtor) error("costruttore chiamato senza oggetto", at);
        if (fn_ && fn_->hasThis) emit(Op::LoadL, 0);
        else if (!lambdas_.empty() && lambdas_.back()->outerFn && lambdas_.back()->outerFn->hasThis) { EV t = captureThis(at); load(t, at); }
        else error("il metodo '" + what + "' vuole un oggetto (siamo in una funzione static)", at);
    }
    if (!needsThis && receiverOnStack) emit(Op::Pop);
    if (f) {
        checkAccess(f->access, f->owner, f->name, at);
        if (!f->defined && !f->isAbstract) calledUndefined_.push_back(f);
    }
    // argomenti
    std::size_t argc = 0;
    const std::size_t np = c.params.size();
    const bool paramsArray = f && !f->params.empty() && f->params.back().isParams;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (paramsArray && c.expandParams && i == np - 1) {
            // params: gli argomenti in piu' in un array
            const std::size_t extra = args.size() - i;
            TypeP et = c.params.back()->elem;
            for (std::size_t k = i; k < args.size(); ++k) emitArg(args[k], probes[k], et, false, at);
            emit(Op::ArrLit, et->index, 0, static_cast<std::uint16_t>(extra));
            ++argc;
            break;
        }
        if (i < np) { emitArg(args[i], probes[i], c.params[i], c.refs.size() > i && c.refs[i], at); ++argc; }
        else { emitVarArg(args[i], at); ++argc; }
    }
    // valori predefiniti mancanti (e params vuoto)
    if (f && args.size() < f->params.size()) {
        Node* decl = fnDecls_.count(f) ? fnDecls_[f] : nullptr;
        for (std::size_t i = args.size(); i < f->params.size(); ++i) {
            if (f->params[i].isParams) { emit(Op::ArrLit, f->params[i].type->elem->index, 0, 0); ++argc; continue; }
            Node* def = decl && i < decl->list.size() ? decl->list[i]->a : nullptr;
            if (!def) error("argomento mancante per " + f->signature(), at);
            // il valore predefinito si valuta nel contesto della dichiarazione
            auto saveCls = cls_;
            cls_ = f->owner ? f->owner : cls_;
            rvalue(def, f->params[i].type);
            cls_ = saveCls;
            ++argc;
        }
    }
    const std::uint8_t total = static_cast<std::uint8_t>(argc + (needsThis ? 1 : 0));
    EV r;
    if (f) {
        if (f->isAbstract && superCall) error("chiamata di un metodo astratto della base", at);
        const bool iface = f->owner && f->owner->isInterface && !f->isStatic;
        if (iface && !superCall) emit(Op::CallI, selector(f->simple, f->params.size()), total);
        else if (f->isVirtual && f->vslot >= 0 && !superCall && !f->isCtor) emit(Op::CallV, f->vslot, total);
        else emit(Op::Call, f->index, total);
        r.type = f->ret;
    } else {
        const NativeEntry& ne = nativeEntry(c.native);
        TypeP rt = c.ret;
        if (rt && rt->k == TK::Native && rt->nk == NK::Pair) {
            // std::make_pair: pair<tipo del primo, tipo del secondo> (i char* diventano std::string)
            std::vector<TypeP> as;
            bool changed = false;
            for (TypeP x : rt->args) {
                TypeP y = substMarkers(x, probes);
                if (y != x && y->k == TK::Ptr && y->elem && y->elem->k == TK::C8) y = tCppStr_;
                changed = changed || y != x;
                as.push_back(y);
            }
            if (changed) rt = nativeOf(rt->nk, as);
        } else rt = substMarkers(rt, probes);
        c.ret = rt;
        emit(Op::CallN, c.native, total, rt->k == TK::Void ? 0 : 1);
        // tipo per i nativi: il ricevente (contenitore) per i metodi, il tipo del risultato per le funzioni
        TypeP passT = c.isStatic ? stripRef(rt) : receiver;
        const std::int32_t pix = passT && passT->index <= 0x7FFF ? passT->index : 0;
        fn_->code.back().c = static_cast<std::uint16_t>((rt->k == TK::Void ? 0 : 1) | (pix << 1));
        (void)ne;
        r.type = rt;
    }
    if (!r.type) r.type = tVoid_;
    if (r.type->k == TK::Void) { r.cat = EV::Void; return r; }
    if (r.type->k == TK::Ref) { r.cat = EV::Ptr; r.type = r.type->elem; r.lvalue = true; return r; }
    r.temp = isValueType(r.type);
    return r;
}

void Compiler::emitArg(Node* arg, const EV& probe, TypeP param, bool byRef, Pos at) {
    (void)probe;
    if (arg->k == K::Declarator) {
        // out var x: la variabile nasce qui
        TypeP t = resolveType(arg->type, false);
        if (t->k == TK::Dynamic && t->size == -2) t = stripRef(param);
        LocalVar& v = addLocal(syms_[arg->sym], t, arg->pos, csCaptured_.count(syms_[arg->sym]) != 0);
        zeroValue(t, arg->pos);
        emit(Op::StoreL, v.slot);
        if (v.boxed) { emit(Op::NewBox, v.slot); emit(Op::AddrL, v.slot, 1); }
        else emit(Op::AddrL, v.slot);
        return;
    }
    if (byRef) {
        EV a = expr(arg);
        TypeP at2 = stripRef(a.type), pt2 = stripRef(param);
        if (a.lvalue && (at2 == pt2 || pt2->k == TK::Dynamic || (at2->k == TK::Class && pt2->k == TK::Class && at2->cls && pt2->cls && at2->cls->derivesFrom(pt2->cls)))) {
            if (a.isConst && lang_ != Dialect::Cpp) error("argomento ref di una costante", arg->pos);
            addressOfEV(a, arg->pos);
            return;
        }
        // riferimento a un temporaneo (const T&): copia in una variabile nascosta
        load(a, arg->pos);
        convert(a.type, pt2, arg->pos, false, &a);
        if (isValueType(pt2) || pt2->k == TK::Class) { copyIfLvalue(a, pt2, arg->pos); emit(Op::ConvT, ptrTo(pt2)->index); return; }
        LocalVar& tmp = addLocal("", pt2, arg->pos);
        emit(Op::StoreL, tmp.slot);
        emit(Op::AddrL, tmp.slot);
        return;
    }
    if (arg->k == K::Lambda) { lambda(arg, stripRef(param)); return; }
    if (arg->k == K::InitList) { initFromList(param, arg, arg->pos); return; }
    rvalue(arg, param);
    (void)at;
}
// Argomenti delle funzioni variadiche (printf): promozioni del C.
void Compiler::emitVarArg(Node* arg, Pos at) {
    EV a = expr(arg);
    load(a, at);
    TypeP t = stripRef(a.type);
    if (t->k == TK::F32) convert(t, tF64_, at);
    else if (t->k == TK::Bool || t->k == TK::C8 || t->k == TK::I8 || t->k == TK::U8 || t->k == TK::I16 || t->k == TK::U16) convert(t, tI32_, at);
    else if (t->k == TK::Array && t->elem) convert(t, ptrTo(t->elem), at);
}

// T(args): conversione (tipi semplici) o oggetto temporaneo (classi).
EV Compiler::construct(TypeP t, const std::vector<Node*>& args, Pos at, bool braces) {
    EV r;
    if (t->k == TK::Class && t->cls && !t->cls->isEnum) {
        completeClass(t->cls);
        if (lang_ != Dialect::Cpp && lang_ != Dialect::C) error("un oggetto si crea con new", at);
        if (braces && t->cls->ctors.empty()) {
            Node* il = pool_.node(K::InitList, at);
            il->list = args;
            initFromList(t, il, at);
        } else constructObject(t->cls, args, at, true);
        r.type = t;
        r.temp = true;
        return r;
    }
    if (t->k == TK::Native || (t->k == TK::Str && t->cppValue)) {
        if (braces && t->k == TK::Native) {
            Node* il = pool_.node(K::InitList, at);
            il->list = args;
            listToNative(t, il, at);
        } else constructNative(t, args, at);
        r.type = t;
        r.temp = true;
        return r;
    }
    if (args.size() != 1) {
        if (args.empty()) { zeroValue(t, at); r.type = t; return r; }
        error("conversione con piu' argomenti", at);
    }
    EV a = expr(args[0]);
    load(a, at);
    convert(a.type, t, at, true, &a);
    r.type = t;
    return r;
}

// ================================================================ operatori dell'utente
bool Compiler::operatorCall(P op, Node* a, Node* b, Pos at, EV& out) {
    EV pa = exprType(a);
    EV pb = b ? exprType(b) : EV{};
    TypeP ta = stripRef(pa.type), tb = b ? stripRef(pb.type) : nullptr;
    auto userClass = [](TypeP t) { return t && t->k == TK::Class && t->cls && !t->cls->isEnum && !t->cls->isNative; };
    const bool stream = ta && ta->k == TK::Native && (ta->nk == NK::OStream || ta->nk == NK::IStream || ta->nk == NK::IFStream ||
                                                       ta->nk == NK::OFStream || ta->nk == NK::FStream || ta->nk == NK::StringStream);
    if (!userClass(ta) && !userClass(tb) && !stream && !(ta && ta->k == TK::Native) && !(ta && ta->k == TK::Str && ta->cppValue && lang_ == Dialect::Cpp) &&
        !(tb && tb->k == TK::Str && tb->cppValue && lang_ == Dialect::Cpp))
        return false;
    const std::string opName = std::string("operator") + punctText(op);
    std::vector<Function*> members, frees;
    if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) {
        // C#: operatori static di una delle due classi
        for (TypeP t : {ta, tb}) if (userClass(t)) {
            bool found = false;
            for (Function* m : findMethods(t->cls, opName, &found)) if (m->isStatic && std::find(frees.begin(), frees.end(), m) == frees.end()) frees.push_back(m);
        }
        if (frees.empty()) return false;
        std::vector<Node*> args{a};
        if (b) args.push_back(b);
        out = callOverloads(frees, {}, {}, args, {}, at, false, nullptr, false, opName);
        return true;
    }
    // C++: membro di a, funzione libera (namespace, amiche), nativi (stream, string)
    if (userClass(ta)) {
        bool found = false;
        for (Function* m : findMethods(ta->cls, opName, &found)) if (!m->isStatic) members.push_back(m);
    }
    auto collectFree = [&](Namespace* ns) {
        for (Namespace* s = ns; s; s = s->parent) {
            auto it = s->members.find(opName);
            if (it != s->members.end() && it->second.kind == NsEntry::Functions)
                for (Function* f : it->second.fns) if (std::find(frees.begin(), frees.end(), f) == frees.end()) frees.push_back(f);
            for (Namespace* u : s->usings) {
                auto ui = u->members.find(opName);
                if (ui != u->members.end() && ui->second.kind == NsEntry::Functions)
                    for (Function* f : ui->second.fns) if (std::find(frees.begin(), frees.end(), f) == frees.end()) frees.push_back(f);
            }
        }
    };
    collectFree(ns_);
    for (TypeP t : {ta, tb}) if (userClass(t)) collectFree(classNs_[t->cls]);
    std::vector<std::int32_t> nat;
    if (ta && (ta->k == TK::Native || (ta->k == TK::Str && ta->cppValue))) nat = nativeMembers(ownerKey(ta), opName, 0, 0);
    // "testo" op std::string, 'c' + std::string: il primo operando diventa std::string
    if (nat.empty() && members.empty() && frees.empty() && lang_ == Dialect::Cpp && tb && tb->k == TK::Str && tb->cppValue && !stream &&
        !(ta && ta->k == TK::Str && ta->cppValue) && !userClass(ta)) {
        std::vector<std::int32_t> sn = nativeMembers("string", opName, 0, 0);
        if (!sn.empty() && b) {
            if (ta && ta->k == TK::C8) { rvalue(a, nullptr); callBuiltinOnStack("string", "#fromchar", "string(char)", 1, tCppStr_); }
            else rvalue(a, tCppStr_);
            std::vector<Node*> rb{b};
            out = callOverloads({}, sn, {}, rb, {}, at, true, tCppStr_, false, opName);
            return true;
        }
    }
    if (members.empty() && frees.empty() && nat.empty()) return false;
    // si prova prima con i membri (ricevente a), poi le funzioni libere
    std::vector<Node*> rest;
    if (b) rest.push_back(b);
    std::vector<Node*> both{a};
    if (b) both.push_back(b);
    // scelta come in C++: un solo insieme di candidati, vince la conversione piu' economica
    // (una conversione dell'utente col costruttore costa piu' di una standard: cout << 0.5
    // non passa da un operator<< dell'utente che accetta una classe costruibile da double)
    std::vector<EV> argProbes;
    for (Node* x : both) argProbes.push_back(exprType(x));
    auto argCostOf = [&](std::size_t argIndex, TypeP pt, bool byRef) -> int {
        const EV& p = argProbes[argIndex];
        if (!p.type) return -1;
        if (byRef) {
            if (stripRef(p.type) == stripRef(pt)) return 0;
            const int c = convCost(p.type, stripRef(pt), &p);
            return c < 0 ? -1 : c;
        }
        return convCost(p.type, pt, &p);
    };
    // costo minimo di un gruppo; `first` = indice in `both` del primo argomento (1 se a e' il ricevente)
    auto bestFns = [&](const std::vector<Function*>& fs, std::size_t first) -> int {
        int best = -1;
        for (Function* f : fs) {
            if (f->params.size() != both.size() - first) continue;
            int total = 0;
            for (std::size_t i = 0; i < f->params.size() && total >= 0; ++i) {
                const int c = argCostOf(first + i, f->params[i].type, f->params[i].byRef);
                total = c < 0 ? -1 : total + c;
            }
            if (total >= 0 && (best < 0 || total < best)) best = total;
        }
        return best;
    };
    auto bestNatives = [&]() -> int {
        int best = -1;
        for (std::int32_t idx : nat) {
            const NativeEntry& ne = nativeEntry(idx);
            std::vector<TypeP> ps;
            std::vector<bool> refs;
            bool variadic = false;
            (void)parseSig(ne.sig, ps, variadic, ta, lang_, &refs);
            if (ps.size() != rest.size()) continue;
            int total = 0;
            for (std::size_t i = 0; i < ps.size() && total >= 0; ++i) {
                const int c = argCostOf(1 + i, substMarkers(ps[i], argProbes), i < refs.size() && refs[i]);
                total = c < 0 ? -1 : total + c;
            }
            if (total >= 0 && (best < 0 || total < best)) best = total;
        }
        return best;
    };
    const int cm = members.empty() ? -1 : bestFns(members, 1);
    const int cf = frees.empty() ? -1 : bestFns(frees, 0);
    const int cn = nat.empty() ? -1 : bestNatives();
    auto beats = [](int x, int y) { return x >= 0 && (y < 0 || x <= y); };
    if (cm >= 0 && beats(cm, cf)) {
        EV recv = expr(a);
        if (recv.lvalue && !isValueType(stripRef(recv.type))) {}
        load(recv, at);
        out = callOverloads(members, {}, {}, rest, {}, at, true, ta, false, opName);
        return true;
    }
    if (cf >= 0 && (cn < 0 || cf <= cn)) {
        out = callOverloads(frees, {}, {}, both, {}, at, false, nullptr, false, opName);
        return true;
    }
    if (!nat.empty()) {
        if (ta && (ta->k == TK::Native || (ta->k == TK::Str && ta->cppValue))) {
            EV recv = expr(a);
            if (recv.lvalue && (op == Shl || op == Shr || op == PlusEq)) { EV addr = addressOfEV(recv, at); (void)addr; }
            else load(recv, at);
            out = callOverloads({}, nat, {}, rest, {}, at, true, ta, false, opName);
        } else {
            error("operatore " + opName + " non applicabile", at);
        }
        return true;
    }
    if (!members.empty()) { EV recv = expr(a); load(recv, at); out = callOverloads(members, {}, {}, rest, {}, at, true, ta, false, opName); return true; }
    out = callOverloads(frees, {}, {}, both, {}, at, false, nullptr, false, opName);
    return true;
}

EV Compiler::methodOn(EV& recv, const std::string& m, Node* callNode, const std::vector<Node*>* args, Pos at) {
    TypeP t = stripRef(recv.type);
    std::vector<std::int32_t> nat = nativeMembers(ownerKey(t), m, 0, NF::Static);
    if (nat.empty()) error(typeName(t) + " non ha '" + m + "'", at);
    const NativeEntry& ne = nativeEntry(nat[0]);
    if (ne.flags & NF::Property) return nativeMember(nat, t, m, callNode);
    return callOverloads({}, nat, {}, args ? *args : std::vector<Node*>{}, {}, at, true, t, false, m);
}
EV Compiler::callBuiltinOnStack(const char* owner, const char* nm, const char* sig, int argc, TypeP t) {
    const std::int32_t idx = nativeIndex("", owner, nm, sig);
    std::vector<TypeP> ps;
    bool variadic = false;
    TypeP rt = parseSig(nativeEntry(idx).sig, ps, variadic, t, lang_);
    emit(Op::CallN, idx, static_cast<std::uint8_t>(argc), rt->k == TK::Void ? 0 : 1);
    fn_->code.back().c = static_cast<std::uint16_t>((rt->k == TK::Void ? 0 : 1) | ((t ? t->index & 0x7FFF : 0) << 1));
    EV r;
    r.type = rt;
    if (rt->k == TK::Void) r.cat = EV::Void;
    if (rt->k == TK::Ref) { r.cat = EV::Ptr; r.type = rt->elem; r.lvalue = true; }
    return r;
}

// ================================================================ operatori
EV Compiler::binary(Node* n) {
    const P op = static_cast<P>(n->op);
    EV r;
    if (op == AndAnd || op == OrOr) {
        TypeP res = boolResult(lang_);
        condition(n->a);
        emit(Op::ToBool);
        const std::int32_t j = emit(op == AndAnd ? Op::JzKeep : Op::JnzKeep, 0);
        emit(Op::Pop);
        condition(n->b);
        emit(Op::ToBool);
        patch(j, here());
        if (res->k != TK::Bool) emit(Op::Conv, 0, static_cast<std::uint8_t>(VK::I32));
        r.type = res;
        return r;
    }
    if (op == QQ) {
        EV pa = exprType(n->a);
        EV pb = exprType(n->b);
        TypeP ta = stripRef(pa.type), tb = stripRef(pb.type);
        TypeP res = ta->k == TK::Nullable ? (tb->k == TK::Nullable ? ta : ta->elem) : ta;
        if (ta->k == TK::Nullable && isNumeric(tb) && ta->elem != tb) res = arithmetic(ta->elem, tb, n->pos);
        rvalue(n->a, nullptr);
        emit(Op::Dup);
        const std::int32_t j = emit(Op::Jnz, 0);
        emit(Op::Pop);
        rvalue(n->b, res);
        const std::int32_t end = emit(Op::Jmp, 0);
        patch(j, here());
        if (ta->k == TK::Nullable && res == ta->elem) emit(Op::ConvT, res->index);
        patch(end, here());
        r.type = res;
        return r;
    }
    // operatori dell'utente e delle librerie (stream, std::string)
    if (operatorCall(op, n->a, n->b, n->pos, r)) return r;
    EV pa = exprType(n->a);
    EV pb = exprType(n->b);
    TypeP ta = stripRef(pa.type), tb = stripRef(pb.type);
    if (pa.cat == EV::FuncGroup || pb.cat == EV::FuncGroup) error("funzione usata come valore in un'operazione", n->pos);
    const bool cs = lang_ == Dialect::CSharp || lang_ == Dialect::Java;
    // concatenazione di stringhe (C#, Java)
    if (op == Plus && cs && (ta->k == TK::Str || tb->k == TK::Str)) {
        rvalue(n->a, nullptr);
        if (ta->k == TK::Enum && lang_ == Dialect::CSharp) callBuiltinOnStack("enum", "#box", "any(self)", 1, ta);
        if (ta->k != TK::Str) emit(Op::ToStr, 0, static_cast<std::uint8_t>(lang_));
        rvalue(n->b, nullptr);
        if (tb->k == TK::Enum && lang_ == Dialect::CSharp) callBuiltinOnStack("enum", "#box", "any(self)", 1, tb);
        if (tb->k != TK::Str) emit(Op::ToStr, 0, static_cast<std::uint8_t>(lang_));
        emit(Op::Concat);
        r.type = tCsStr_;
        return r;
    }
    // confronti tra stringhe di C#/Java e con null
    if ((op == EqEq || op == Ne) && cs) {
        if (ta->k == TK::Str && tb->k == TK::Str && lang_ == Dialect::CSharp) {
            rvalue(n->a, nullptr); rvalue(n->b, nullptr);
            emit(Op::StrEq);
            if (op == Ne) emit(Op::Not);
            r.type = tBool_;
            return r;
        }
        if (!isNumeric(ta) || !isNumeric(tb)) {
            // riferimenti, null, nullable
            if (ta->k == TK::Nullable || tb->k == TK::Nullable) {
                rvalue(n->a, nullptr); rvalue(n->b, nullptr);
                emit(op == EqEq ? Op::Eq : Op::Ne);
                r.type = tBool_;
                return r;
            }
            if (isNumeric(ta) != isNumeric(tb) && ta->k != TK::Null && tb->k != TK::Null) error("confronto tra " + typeName(ta) + " e " + typeName(tb), n->pos);
            rvalue(n->a, nullptr); rvalue(n->b, nullptr);
            emit(op == EqEq ? Op::Eq : Op::Ne);
            r.type = tBool_;
            return r;
        }
    }
    const bool cmp = op == EqEq || op == Ne || op == Lt || op == Gt || op == Le || op == Ge;
    // puntatori (C, C++)
    if (!cs && (ta->k == TK::Ptr || tb->k == TK::Ptr || ta->k == TK::Array || tb->k == TK::Array || ta->k == TK::Null || tb->k == TK::Null || ta->k == TK::Func || tb->k == TK::Func)) {
        TypeP pa2 = ta->k == TK::Array ? ptrTo(ta->elem) : ta;
        TypeP pb2 = tb->k == TK::Array ? ptrTo(tb->elem) : tb;
        if (cmp) {
            rvalue(n->a, pa2->k == TK::Null && pb2->k == TK::Ptr ? nullptr : pa2);
            if (isIntegral(tb) && pb.hasConst && pb.constValue.i == 0 && pa2->k == TK::Ptr) { rvalue(n->b, nullptr); emit(Op::Pop); emit(Op::PushNull); }
            else rvalue(n->b, pb2);
            emit(op == EqEq ? Op::Eq : op == Ne ? Op::Ne : op == Lt ? Op::Lt : op == Gt ? Op::Gt : op == Le ? Op::Le : Op::Ge);
            r.type = boolResult(lang_);
            return r;
        }
        if (op == Plus || op == Minus) {
            if (pa2->k == TK::Ptr && isIntegral(tb)) {
                if (pa2->elem && pa2->elem->k == TK::Void) error("aritmetica su un void*", n->pos);
                rvalue(n->a, pa2);
                rvalue(n->b, tI64_);
                emit(op == Plus ? Op::Add : Op::Sub);
                r.type = pa2;
                return r;
            }
            if (op == Plus && pb2->k == TK::Ptr && isIntegral(ta)) {
                rvalue(n->b, pb2);
                rvalue(n->a, tI64_);
                emit(Op::Add);
                r.type = pb2;
                return r;
            }
            if (op == Minus && pa2->k == TK::Ptr && pb2->k == TK::Ptr) {
                if (pa2->elem != pb2->elem) error("differenza tra puntatori di tipi diversi", n->pos);
                rvalue(n->a, pa2);
                rvalue(n->b, pb2);
                emit(Op::Sub);
                r.type = tI64_;
                return r;
            }
        }
        error("operazione non valida tra " + typeName(ta) + " e " + typeName(tb), n->pos);
    }
    // confronti di C++ tra std::string li gestisce operatorCall (nativi); qui solo numeri e bool
    if (cmp) {
        if (ta->k == TK::Str || tb->k == TK::Str) {
            if (!cs) error("confronto tra stringhe del C: usa strcmp", n->pos);
            error("le stringhe si confrontano con == o con CompareTo", n->pos);
        }
        if (ta->k == TK::Enum && tb->k == TK::Enum) {
            if (ta != tb) error("confronto tra enum diversi", n->pos);
            rvalue(n->a, nullptr); rvalue(n->b, nullptr);
        } else if ((ta->k == TK::Bool && tb->k == TK::Bool) && (op == EqEq || op == Ne)) {
            rvalue(n->a, nullptr); rvalue(n->b, nullptr);
        } else if (!isNumeric(ta) || !isNumeric(tb)) {
            if ((ta->k == TK::Class && ta->cls && !ta->cls->valueType) || (tb->k == TK::Class && tb->cls && !tb->cls->valueType) || ta->k == TK::Dynamic || tb->k == TK::Dynamic) {
                if (op != EqEq && op != Ne) error("confronto < > tra oggetti: serve CompareTo", n->pos);
                rvalue(n->a, nullptr); rvalue(n->b, nullptr);
            } else error("confronto non valido tra " + typeName(ta) + " e " + typeName(tb), n->pos);
        } else {
            TypeP ct = arithmetic(ta, tb, n->pos);
            rvalue(n->a, ct);
            rvalue(n->b, ct);
        }
        emit(op == EqEq ? Op::Eq : op == Ne ? Op::Ne : op == Lt ? Op::Lt : op == Gt ? Op::Gt : op == Le ? Op::Le : Op::Ge);
        r.type = boolResult(lang_);
        return r;
    }
    // bool & | ^ (C#, Java: logici senza corto circuito)
    if ((op == Amp || op == Pipe || op == Caret) && ta->k == TK::Bool && tb->k == TK::Bool) {
        rvalue(n->a, nullptr); rvalue(n->b, nullptr);
        emit(op == Amp ? Op::And : op == Pipe ? Op::Or : Op::Xor);
        r.type = tBool_;
        return r;
    }
    if (!isNumeric(ta) || !isNumeric(tb)) {
        if (ta->k == TK::Str || tb->k == TK::Str) error("operazione '" + std::string(punctText(op)) + "' su stringhe", n->pos);
        error("operazione '" + std::string(punctText(op)) + "' non valida tra " + typeName(ta) + " e " + typeName(tb), n->pos);
    }
    if (op == Shl || op == Shr || op == UShr) {
        if (!isIntegral(ta) || !isIntegral(tb)) error("scorrimento di un valore non intero", n->pos);
        TypeP lt = arithmetic(ta, ta, n->pos);
        rvalue(n->a, lt);
        rvalue(n->b, tI32_, true);
        emit(op == Shl ? Op::Shl : op == Shr ? Op::Shr : Op::UShr);
        r.type = lt;
        return r;
    }
    TypeP ct = arithmetic(ta, tb, n->pos);
    if ((op == Percent || op == Amp || op == Pipe || op == Caret) && !isIntegral(ct) && !(op == Percent && (cs || lang_ == Dialect::Java)))
        error("operazione '" + std::string(punctText(op)) + "' su numeri reali" + (op == Percent ? ": usa fmod" : ""), n->pos);
    rvalue(n->a, ct);
    rvalue(n->b, ct);
    switch (op) {
    case Plus: emit(Op::Add); break;
    case Minus: emit(Op::Sub); break;
    case Star: emit(Op::Mul); break;
    case Slash: emit(Op::Div); break;
    case Percent: emit(Op::Rem); break;
    case Amp: emit(Op::And); break;
    case Pipe: emit(Op::Or); break;
    case Caret: emit(Op::Xor); break;
    default: error("operatore non supportato", n->pos);
    }
    r.type = ct;
    if (pa.hasConst && pb.hasConst && isIntegral(ct)) {
        std::int64_t v = 0;
        if (constInt(n, v)) { r.hasConst = true; r.constValue = Value::ofInt(vkOf(ct), v); }
    }
    return r;
}

EV Compiler::unary(Node* n) {
    const P op = static_cast<P>(n->op);
    EV r;
    if (operatorCall(op, n->a, nullptr, n->pos, r)) return r;
    EV pa = exprType(n->a);
    TypeP t = stripRef(pa.type);
    if (op == Bang) {
        if ((lang_ == Dialect::CSharp || lang_ == Dialect::Java) && t->k != TK::Bool) error("! vuole un bool", n->pos);
        condition(n->a);
        emit(Op::Not);
        r.type = boolResult(lang_);
        return r;
    }
    if (!isNumeric(t)) error("operatore '" + std::string(punctText(op)) + "' su " + typeName(t), n->pos);
    if (op == Tilde && !isIntegral(t)) error("~ su un numero reale", n->pos);
    TypeP pt = arithmetic(t, t, n->pos);
    rvalue(n->a, pt);
    if (op == Minus) emit(Op::Neg);
    else if (op == Tilde) emit(Op::BitNot);
    r.type = pt;
    if (pa.hasConst && isIntegral(pt)) {
        std::int64_t v = 0;
        if (constInt(n, v)) { r.hasConst = true; r.constValue = Value::ofInt(vkOf(pt), v); }
    }
    return r;
}

EV Compiler::assign(Node* n) { return assignImpl(n, true); }
EV Compiler::assignImpl(Node* n, bool needValue) {
    const P op = static_cast<P>(n->op);
    EV r;
    if (op == Assign) {
        EV probeT = exprType(n->a);
        TypeP tt = stripRef(probeT.type);
        // C++: oggetti per valore: operator= dell'utente o copia dei campi nell'oggetto esistente
        if ((lang_ == Dialect::Cpp || lang_ == Dialect::C) && (isValueType(tt)) && probeT.cat != EV::Prop) {
            if (tt->k == TK::Class && tt->cls && lang_ == Dialect::Cpp) {
                bool found = false;
                std::vector<Function*> ms = findMethods(tt->cls, "operator=", &found);
                if (found) {
                    EV target = expr(n->a);
                    load(target, n->pos);
                    r = callOverloads(ms, {}, {}, {n->b}, {}, n->pos, true, tt, false, "operator=");
                    if (!needValue && r.cat != EV::Void) { if (r.cat == EV::Ptr) emit(Op::Pop); else emit(Op::Pop); r.cat = EV::Void; }
                    return r;
                }
            }
            EV target = expr(n->a);
            if (!target.lvalue) error("assegnazione a un valore temporaneo", n->pos);
            if (target.isConst) error("assegnazione a una costante", n->pos);
            if (tt->k == TK::Array) error("un array non si assegna: copia gli elementi (o usa memcpy)", n->pos);
            load(target, n->pos);
            if (n->b->k == K::InitList) initFromList(tt, n->b, n->b->pos);
            else {
                EV v = expr(n->b);
                load(v, n->b->pos);
                convert(v.type, tt, n->b->pos, false, &v);
            }
            emit(Op::CopyInto, tt->k == TK::Class && tt->cls ? tt->cls->id : -1);
            if (needValue) { EV again = expr(n->a); return again; }
            r.cat = EV::Void;
            r.type = tVoid_;
            return r;
        }
        EV target = expr(n->a);
        if (!target.lvalue) error("l'espressione a sinistra di = non si puo' assegnare", n->a->pos);
        TypeP t = stripRef(target.type);
        if (n->b->k == K::InitList) initFromList(t, n->b, n->b->pos);
        else if (n->b->k == K::Lambda) lambda(n->b, t);
        else {
            EV v = expr(n->b);
            if (v.cat == EV::FuncGroup && (t->k == TK::Ptr || t->k == TK::Func)) {
                // f = nome_funzione: si sceglie la versione con la firma giusta
                TypeP ft = t->k == TK::Ptr ? t->elem : t;
                Function* pick = nullptr;
                for (Function* g : v.fns) {
                    std::vector<TypeP> ps;
                    for (const Param& p : g->params) ps.push_back(p.type);
                    if (ft && ft->k == TK::Func && funcOf(g->ret, ps) == ft) pick = g;
                }
                if (!pick && v.fns.size() == 1) pick = v.fns[0];
                if (!pick) error("nessuna versione della funzione ha la firma richiesta", n->b->pos);
                emit(Op::PushFn, pick->index);
            } else {
                load(v, n->b->pos);
                convert(v.type, t, n->b->pos, false, &v);
                copyIfLvalue(v, t, n->b->pos);
            }
        }
        store(target, needValue, n->pos);
        r.type = t;
        if (!needValue) r.cat = EV::Void;
        return r;
    }
    if (op == QQEq) {
        EV target = expr(n->a);
        TypeP t = stripRef(target.type);
        dupComponents(*this, target);
        EV cur = target;
        load(cur, n->pos);
        const std::int32_t j = emit(Op::Jnz, 0);
        rvalue(n->b, t);
        store(target, false, n->pos);
        const std::int32_t end = emit(Op::Jmp, 0);
        patch(j, here());
        for (std::int32_t k = 0; k < componentsOf(target); ++k) emit(Op::Pop);
        patch(end, here());
        if (needValue) { EV again = expr(n->a); load(again, n->pos); r.type = t; return r; }
        r.cat = EV::Void;
        return r;
    }
    // composta: a op= b
    P bop = PNone;
    switch (op) {
    case PlusEq: bop = Plus; break; case MinusEq: bop = Minus; break; case StarEq: bop = Star; break; case SlashEq: bop = Slash; break;
    case PercentEq: bop = Percent; break; case AmpEq: bop = Amp; break; case PipeEq: bop = Pipe; break; case CaretEq: bop = Caret; break;
    case ShlEq: bop = Shl; break; case ShrEq: bop = Shr; break; case UShrEq: bop = UShr; break;
    default: error("operatore di assegnazione non supportato", n->pos);
    }
    EV probeT = exprType(n->a);
    TypeP tt = stripRef(probeT.type);
    // operatori dell'utente: a += b
    if (tt->k == TK::Class && tt->cls && !tt->cls->isEnum) {
        if (lang_ == Dialect::Cpp) {
            bool found = false;
            std::vector<Function*> ms = findMethods(tt->cls, std::string("operator") + punctText(op), &found);
            if (found) {
                EV target = expr(n->a);
                load(target, n->pos);
                r = callOverloads(ms, {}, {}, {n->b}, {}, n->pos, true, tt, false, punctText(op));
                if (!needValue && r.cat != EV::Void) { emit(Op::Pop); r.cat = EV::Void; }
                return r;
            }
        }
        // a = a op b con l'operatore binario
        Node* bin = pool_.node(K::Binary, n->pos);
        bin->op = bop;
        bin->a = n->a;
        bin->b = n->b;
        Node* as = pool_.node(K::Assign, n->pos);
        as->op = Assign;
        as->a = n->a;
        as->b = bin;
        return assignImpl(as, needValue);
    }
    if (tt->k == TK::Str && tt->cppValue && op == PlusEq) {
        EV target = expr(n->a);
        EV addr = addressOfEV(target, n->pos);
        (void)addr;
        std::vector<std::int32_t> nat = nativeMembers("string", "operator+=", 0, 0);
        r = callOverloads({}, nat, {}, {n->b}, {}, n->pos, true, tt, false, "+=");
        if (!needValue && r.cat != EV::Void) { emit(Op::Pop); r.cat = EV::Void; }
        return r;
    }
    EV target = expr(n->a);
    if (!target.lvalue) error("l'espressione a sinistra di " + std::string(punctText(op)) + " non si puo' assegnare", n->a->pos);
    if (target.isConst) error("assegnazione a una costante", n->pos);
    dupComponents(*this, target);
    EV cur = target;
    load(cur, n->pos);
    TypeP t = stripRef(target.type);
    EV pb = exprType(n->b);
    TypeP tb = stripRef(pb.type);
    const bool cs = lang_ == Dialect::CSharp || lang_ == Dialect::Java;
    if (cs && t->k == TK::Str && op == PlusEq) {
        rvalue(n->b, nullptr);
        if (tb->k != TK::Str) emit(Op::ToStr, 0, static_cast<std::uint8_t>(lang_));
        emit(Op::Concat);
    } else if (t->k == TK::Ptr && (op == PlusEq || op == MinusEq)) {
        if (!isIntegral(tb)) error("puntatore += vuole un intero", n->pos);
        rvalue(n->b, tI64_);
        emit(op == PlusEq ? Op::Add : Op::Sub);
    } else if (t->k == TK::Bool && cs && (op == AmpEq || op == PipeEq || op == CaretEq)) {
        rvalue(n->b, tBool_);
        emit(op == AmpEq ? Op::And : op == PipeEq ? Op::Or : Op::Xor);
    } else {
        if (!isNumeric(t) || !isNumeric(tb)) error("operazione " + std::string(punctText(op)) + " non valida tra " + typeName(t) + " e " + typeName(tb), n->pos);
        if (bop == Shl || bop == Shr || bop == UShr) {
            TypeP lt = arithmetic(t, t, n->pos);
            convert(t, lt, n->pos);
            rvalue(n->b, tI32_, true);
            emit(bop == Shl ? Op::Shl : bop == Shr ? Op::Shr : Op::UShr);
            convert(lt, t, n->pos, true);
        } else {
            TypeP ct = arithmetic(t, tb, n->pos);
            if ((bop == Percent || bop == Amp || bop == Pipe || bop == Caret) && !isIntegral(ct) && !(bop == Percent && cs)) error("operazione su numeri reali", n->pos);
            convert(t, ct, n->pos);
            rvalue(n->b, ct);
            emit(bop == Plus ? Op::Add : bop == Minus ? Op::Sub : bop == Star ? Op::Mul : bop == Slash ? Op::Div : bop == Percent ? Op::Rem :
                 bop == Amp ? Op::And : bop == Pipe ? Op::Or : Op::Xor);
            convert(ct, t, n->pos, true);                // a += 1.5 su un int: troncamento come la norma
        }
    }
    store(target, needValue, n->pos);
    r.type = t;
    if (!needValue) r.cat = EV::Void;
    return r;
}

EV Compiler::incdec(Node* n, bool pre, bool dec) {
    EV r;
    EV probeT = exprType(n->a);
    TypeP t = stripRef(probeT.type);
    if (t->k == TK::Class && t->cls && !t->cls->isEnum) {
        // operator++ / operator-- dell'utente (C++: il postfisso ha un int fittizio)
        const std::string nm = std::string("operator") + (dec ? "--" : "++");
        bool found = false;
        std::vector<Function*> ms = findMethods(t->cls, nm, &found);
        if (!found) error("la classe " + t->cls->name + " non ha " + nm, n->pos);
        if (lang_ == Dialect::CSharp) {
            // C#: static T operator++(T x); a = op(a)
            EV target = expr(n->a);
            dupComponents(*this, target);
            EV cur = target;
            load(cur, n->pos);
            if (!pre) { if (componentsOf(target) == 0) emit(Op::Dup); else emit(Op::DupX, 0, static_cast<std::uint8_t>(componentsOf(target))); }
            emit(Op::Call, ms[0]->index, 1);
            store(target, pre, n->pos);
            r.type = t;
            return r;
        }
        Function* pick = nullptr;
        for (Function* m : ms) if (m->params.size() == (pre ? 0u : 1u)) pick = m;
        if (!pick) for (Function* m : ms) if (m->params.empty()) pick = m;
        if (!pick) error(nm + " non adatto", n->pos);
        EV target = expr(n->a);
        load(target, n->pos);
        if (pick->params.size() == 1) pushInt(0, VK::I32);
        emit(Op::Call, pick->index, static_cast<std::uint8_t>(1 + pick->params.size()));
        r.type = pick->ret;
        if (r.type->k == TK::Void) r.cat = EV::Void;
        if (r.type->k == TK::Ref) { r.cat = EV::Ptr; r.type = r.type->elem; r.lvalue = true; }
        return r;
    }
    if (t->k == TK::Native && (t->nk == NK::Iter || t->nk == NK::MapIter)) {
        // iteratori del C++: it = #next(it)
        EV target = expr(n->a);
        if (!target.lvalue) error("++/-- vuole una variabile", n->pos);
        const std::int32_t comps = componentsOf(target);
        dupComponents(*this, target);
        EV cur = target;
        load(cur, n->pos);
        if (!pre) { if (comps == 0) emit(Op::Dup); else emit(Op::DupX, 0, static_cast<std::uint8_t>(comps)); }
        callBuiltinOnStack(t->nk == NK::Iter ? "iterator" : "mapiterator", dec ? "#prev" : "#next", "self(self)", 1, t);
        store(target, pre, n->pos);
        r.type = t;
        if (!pre) return r;
        return r;
    }
    if (!isNumeric(t) && t->k != TK::Ptr) error((dec ? "--" : "++") + std::string(" su ") + typeName(t), n->pos);
    if ((lang_ == Dialect::CSharp || lang_ == Dialect::Java) && t->k == TK::Bool) error("++ su un bool", n->pos);
    EV target = expr(n->a);
    if (!target.lvalue) error("++/-- vuole una variabile", n->pos);
    if (target.isConst) error("++/-- su una costante", n->pos);
    const std::uint8_t flags = static_cast<std::uint8_t>((pre ? 0 : 1) | (dec ? 2 : 0));
    if (target.cat == EV::Local) { emit(Op::IncL, target.slot, flags); r.type = t; return r; }
    if (target.cat == EV::Global) { emit(Op::IncG, target.slot, flags); r.type = t; return r; }
    if (target.cat == EV::Ptr) { emit(Op::IncP, 0, flags); r.type = t; return r; }
    // generico: componenti, valore, +1, scrittura
    const std::int32_t comps = componentsOf(target);
    dupComponents(*this, target);
    EV cur = target;
    load(cur, n->pos);
    if (!pre) {
        if (comps == 0) emit(Op::Dup); else emit(Op::DupX, 0, static_cast<std::uint8_t>(comps));
    }
    if (t->k == TK::Ptr) { pushInt(1, VK::I64); emit(dec ? Op::Sub : Op::Add); }
    else {
        TypeP ct = arithmetic(t, tI32_, n->pos);
        convert(t, ct, n->pos);
        if (isFloat(vkOf(ct))) pushDouble(1, vkOf(ct)); else pushInt(1, vkOf(ct));
        emit(dec ? Op::Sub : Op::Add);
        convert(ct, t, n->pos, true);
    }
    store(target, pre, n->pos);
    r.type = t;
    return r;
}

EV Compiler::conditional(Node* n) {
    // prima la condizione: i nomi dei pattern (o is Q q ? q.Area() : 0) valgono nei rami
    condition(n->a);
    const std::int32_t jz = emit(Op::Jz, 0);
    EV pa = exprType(n->b);
    EV pb = exprType(n->c);
    TypeP ta = stripRef(pa.type), tb = stripRef(pb.type);
    TypeP rt = ta;
    if (ta != tb) {
        if (isNumeric(ta) && isNumeric(tb)) rt = arithmetic(ta, tb, n->pos);
        else if (ta->k == TK::Null) rt = tb->k == TK::Array && (lang_ == Dialect::C || lang_ == Dialect::Cpp) ? ptrTo(tb->elem) : tb;
        else if (tb->k == TK::Null) rt = ta->k == TK::Array && (lang_ == Dialect::C || lang_ == Dialect::Cpp) ? ptrTo(ta->elem) : ta;
        else if (ta->k == TK::Array && tb->k == TK::Array && ta->elem == tb->elem) rt = ptrTo(ta->elem);
        else if ((ta->k == TK::Array || ta->k == TK::Ptr) && (tb->k == TK::Array || tb->k == TK::Ptr) && ta->elem == tb->elem) rt = ptrTo(ta->elem);
        else if (ta->k == TK::Class && tb->k == TK::Class && ta->cls && tb->cls && tb->cls->derivesFrom(ta->cls)) rt = ta;
        else if (ta->k == TK::Class && tb->k == TK::Class && ta->cls && tb->cls && ta->cls->derivesFrom(tb->cls)) rt = tb;
        else if (ta->k == TK::Str && tb->k == TK::Array && ta->cppValue) rt = ta;
        else if (tb->k == TK::Str && ta->k == TK::Array && tb->cppValue) rt = tb;
        else if (ta->k == TK::Dynamic || tb->k == TK::Dynamic) rt = tDyn_;
        else if (ta->k == TK::Void && tb->k == TK::Void) rt = tVoid_;
        else error("i due rami di ?: hanno tipi diversi (" + typeName(ta) + " e " + typeName(tb) + ")", n->pos);
    } else if (ta->k == TK::Array && (lang_ == Dialect::C || lang_ == Dialect::Cpp)) rt = ptrTo(ta->elem);
    if (rt->k == TK::Void) discard(n->b); else rvalue(n->b, rt);
    const std::int32_t j = emit(Op::Jmp, 0);
    patch(jz, here());
    if (rt->k == TK::Void) discard(n->c); else rvalue(n->c, rt);
    patch(j, here());
    EV r;
    r.type = rt;
    if (rt->k == TK::Void) r.cat = EV::Void;
    return r;
}

EV Compiler::cast(Node* n) {
    TypeP t = resolveType(n->type, true);
    EV r;
    if (n->op == 5) {                                    // (struct P){...}
        initFromList(t, n->a, n->pos);
        r.type = t;
        r.temp = true;
        return r;
    }
    if (n->op == 2) {
        // dynamic_cast<T*>(p) / dynamic_cast<T&>(r)
        EV v = expr(n->a);
        TypeP target = stripRef(t);
        if (target->k == TK::Ptr && target->elem && target->elem->k == TK::Class) {
            load(v, n->pos);
            emit(Op::DynCast, target->elem->cls->id);
            r.type = target;
            return r;
        }
        if (t->k == TK::Ref && target->k == TK::Class) {
            EV addr = addressOfEV(v, n->pos);
            (void)addr;
            emit(Op::CastCk, target->cls->id, 1);
            r.cat = EV::Ptr;
            r.type = target;
            r.lvalue = true;
            return r;
        }
        error("dynamic_cast vuole un puntatore o un riferimento a una classe", n->pos);
    }
    if (t->k == TK::Ref) {
        // static_cast<T&>(x)
        EV v = expr(n->a);
        addressOfEV(v, n->pos);
        r.cat = EV::Ptr;
        r.type = t->elem;
        r.lvalue = true;
        return r;
    }
    EV v = expr(n->a);
    if (v.cat == EV::FuncGroup && t->k == TK::Ptr && t->elem && t->elem->k == TK::Func) {
        if (v.fns.size() != 1) error("funzione con piu' versioni nel cast", n->pos);
        load(v, n->pos);
        r.type = t;
        return r;
    }
    load(v, n->pos);
    if (t->k == TK::Void) { emit(Op::Pop); r.cat = EV::Void; r.type = tVoid_; return r; }
    convert(v.type, t, n->pos, true, &v);
    r.type = t;
    return r;
}

EV Compiler::isExpr(Node* n) {
    TypeP t = resolveType(n->type, false);
    EV r;
    r.type = tBool_;
    EV v = expr(n->a);
    load(v, n->pos);
    if (t->k != TK::Class || !t->cls) {
        if (isNumeric(t) || t->k == TK::Str || t->k == TK::Bool || t->k == TK::Enum || t->k == TK::Nullable || t->k == TK::Array || t->k == TK::Native) {
            // x is int / x is string / x is int n: controllo del tipo dinamico (e legame se vero)
            const std::int32_t tmp = addLocal("", tObject_, n->pos).slot;
            convert(v.type, tObject_, n->pos, true);
            emit(Op::StoreL, tmp);
            emit(Op::LoadL, tmp);
            pushInt(t->index, VK::I64);
            emit(Op::CallN, nativeIndex("", "object", "#is", "bool(any,long)"), 2, 1);
            if (n->sym) {
                TypeP bt = t->k == TK::Nullable && t->elem ? t->elem : t;
                LocalVar& lv = addLocal(syms_[n->sym], bt, n->pos);
                const std::int32_t slot = lv.slot;
                emit(Op::Dup);
                const std::int32_t skip = emit(Op::Jz, 0);
                emit(Op::LoadL, tmp);
                convert(tObject_, bt, n->pos, true);
                emit(Op::StoreL, slot);
                patch(skip, here());
            }
            return r;
        }
        error("is vuole un tipo", n->pos);
    }
    if (n->sym) {
        LocalVar& lv = addLocal(syms_[n->sym], t, n->pos);
        emit(Op::Dup);
        emit(Op::StoreL, lv.slot);
    }
    emit(Op::InstOf, t->cls->id);
    return r;
}

// ================================================================ new
EV Compiler::newExpr(Node* n) {
    EV r;
    const bool cfam = lang_ == Dialect::C || lang_ == Dialect::Cpp;
    if (n->k == K::NewArray) {
        if (lang_ == Dialect::C) error("new non esiste in C: usa malloc", n->pos);
        if (!n->type) {
            // new[] { ... }: tipo dagli elementi
            if (!n->a || n->a->list.empty()) error("new[] {} vuole almeno un valore", n->pos);
            EV p = exprType(n->a->list[0]);
            TypeP et = stripRef(p.type);
            for (Node* item : n->a->list) {
                EV q = exprType(item);
                TypeP qt = stripRef(q.type);
                if (qt != et) { if (isNumeric(qt) && isNumeric(et)) et = arithmetic(qt, et, item->pos); else if (et->k == TK::Null) et = qt; }
            }
            TypeP at = arrayOf(et, -1, 1);
            initFromList(at, n->a, n->pos);
            r.type = at;
            return r;
        }
        TypeP et = resolveType(n->type, false);
        if (!cfam) {
            const std::uint8_t rank = static_cast<std::uint8_t>(n->ival > 0 ? n->ival : 1);
            TypeP at = arrayOf(et, -1, rank);
            if (n->list.empty()) {
                if (!n->a) error("new T[] vuole la dimensione o i valori", n->pos);
                initFromList(at, n->a, n->pos);
                r.type = at;
                return r;
            }
            for (Node* d : n->list) rvalue(d, tI64_);
            emit(Op::NewArr, et->index, static_cast<std::uint8_t>(n->list.size()), static_cast<std::uint16_t>(rank == 2 ? 2 : 0));
            if (n->a) {
                // new int[3] {1, 2, 3}: dimensione e valori
                const std::int32_t tmpSlot = addLocal("", at, n->pos).slot;
                emit(Op::StoreL, tmpSlot);
                std::int32_t i = 0;
                for (Node* item : n->a->list) {
                    emit(Op::LoadL, tmpSlot);
                    pushInt(i++, VK::I64);
                    rvalue(item, et);
                    emit(Op::SetE);
                }
                emit(Op::LoadL, tmpSlot);
            }
            r.type = at;
            return r;
        }
        // C++: new T[n] (oggetti costruiti con il costruttore predefinito)
        if (n->list.size() > 1) {
            for (std::size_t k = 1; k < n->list.size(); ++k) {
                std::int64_t v = 0;
                if (!constInt(n->list[k], v)) error("le dimensioni interne di new T[n][m] devono essere costanti", n->list[k]->pos);
                et = arrayOf(et, static_cast<std::int32_t>(v), 1);
            }
        }
        rvalue(n->list[0], tI64_);
        emit(Op::NewArr, et->index, 1, 1);               // c = 1: allocazione esplicita (delete[])
        if (et->k == TK::Class && et->cls && !et->cls->ctors.empty()) emitArrayCtorLoop(et->cls, n->pos);
        if (n->a) {
            const std::int32_t tmpSlot = addLocal("", arrayOf(et, -1, 1), n->pos).slot;
            emit(Op::StoreL, tmpSlot);
            std::int32_t i = 0;
            for (Node* item : n->a->list) { emit(Op::LoadL, tmpSlot); pushInt(i++, VK::I64); rvalue(item, et); emit(Op::SetE); }
            emit(Op::LoadL, tmpSlot);
        }
        emit(Op::ConvT, ptrTo(et)->index);
        r.type = ptrTo(et);
        return r;
    }
    TypeP t = resolveType(n->type, false);
    if (lang_ == Dialect::C) error("new non esiste in C: usa malloc", n->pos);
    if (t->k == TK::Class && t->cls && !t->cls->isEnum) {
        ClassInfo* c = t->cls;
        completeClass(c);
        const bool argFamily = lang_ == Dialect::CSharp && c->isNative &&
                               (c->name == "ArgumentException" || c->name == "ArgumentNullException" || c->name == "ArgumentOutOfRangeException");
        if (argFamily && !n->list.empty() && n->list.size() <= 2) {
            // ArgumentException(messaggio, nomeParametro), ArgumentNullException(nomeParametro[, messaggio]),
            // ArgumentOutOfRangeException(nomeParametro[, messaggio]): Message e ParamName come .NET
            const bool second = n->list.size() == 2;
            EV p2 = second ? exprType(n->list[1]) : EV();
            const bool innerArg = second && c->name == "ArgumentException" && p2.type && stripRef(p2.type)->k == TK::Class;
            if (!innerArg) {
                emit(Op::New, c->id, 0, 0);
                emit(Op::Dup);
                pushInt(c->name == "ArgumentException" ? 0 : c->name == "ArgumentNullException" ? 1 : 2, VK::I32);
                rvalue(n->list[0], tCsStr_);
                if (second) rvalue(n->list[1], tCsStr_); else emit(Op::PushNull);
                emit(Op::CallN, nativeIndex("", "Exception", "#argInit", "void(any,int,string,string)"), 4, 0);
                r.type = t;
                return r;
            }
        }
        if (c->isNative && c->isException) {
            // eccezioni standard: new Exception("messaggio")
            emit(Op::New, c->id, 0, static_cast<std::uint16_t>(cfam ? 1 : 0));
            if (!n->list.empty()) {
                emit(Op::Dup);
                rvalue(n->list[0], cfam ? tCppStr_ : tCsStr_);
                emit(Op::SetF, 0);
            }
            if (n->list.size() > 1) { emit(Op::Dup); rvalue(n->list[1], tObject_); emit(Op::SetF, 1); }
            if (n->list.size() > 2) error("troppi argomenti per " + c->name, n->pos);
        } else {
            if (c->isAbstract) error("la classe " + c->name + " e' astratta: non si puo' creare", n->pos);
            if (c->isInterface) error("non si crea un'interfaccia", n->pos);
            if (c->isStaticClass) error("una classe static non si crea", n->pos);
            emit(Op::New, c->id, 0, static_cast<std::uint16_t>(cfam ? 1 : 0));
            if (c->ctors.empty()) {
                if (!n->list.empty()) error("la classe " + c->name + " non ha costruttori con argomenti", n->pos);
                if (n->a && cfam) {
                    // new T{a, b}: aggregato
                    emit(Op::Pop);
                    initFromList(t, n->a, n->pos);
                    emit(Op::ConvT, ptrTo(t)->index);
                    r.type = ptrTo(t);
                    return r;
                }
                ensureFieldInit(c);
                if (c->fieldInit) { emit(Op::Dup); emit(Op::Call, c->fieldInit->index, 1); }
            } else {
                emit(Op::Dup);
                std::vector<Node*> args = n->list;
                if (n->a && n->op == 1) args = n->a->list;
                callOverloads(c->ctors, {}, {}, args, {}, n->pos, true, t, true, c->name);
            }
        }
        if (cfam) { emit(Op::ConvT, ptrTo(t)->index); r.type = ptrTo(t); return r; }
        r.type = t;
        return r;
    }
    if (t->k == TK::Native || (t->k == TK::Str && !cfam)) {
        if (cfam) {
            // new std::vector<int>(...): oggetto sullo heap
            constructNative(t, n->list, n->pos);
            emit(Op::ConvT, ptrTo(t)->index, 1);
            r.type = ptrTo(t);
            return r;
        }
        if (t->k == TK::Str) error("new string(...) non supportato: usa new string(c, n) non disponibile, scrivi new String(...) come testo", n->pos);
        constructNative(t, n->list, n->pos);
        r.type = t;
        return r;
    }
    if (cfam) {
        // new int(5): una cella sullo heap (c = 5: esplicita, non un array: delete senza [])
        pushInt(1, VK::I64);
        emit(Op::NewArr, t->index, 1, 5);
        if (!n->list.empty() || n->a) {
            emit(Op::Dup);
            pushInt(0, VK::I64);
            Node* v = !n->list.empty() ? n->list[0] : n->a->list.empty() ? nullptr : n->a->list[0];
            if (v) rvalue(v, t); else zeroValue(t, n->pos);
            emit(Op::SetE);
        }
        emit(Op::ConvT, ptrTo(t)->index);
        r.type = ptrTo(t);
        return r;
    }
    error("new non valido per " + typeName(t), n->pos);
}
// Costruttore predefinito per ogni elemento dell'array sulla pila.
void Compiler::emitArrayCtorLoop(ClassInfo* c, Pos at) {
    Function* def = nullptr;
    for (Function* f : c->ctors) {
        bool ok = true;
        for (const Param& p : f->params) if (!p.hasDefault) ok = false;
        if (ok) def = f;
    }
    if (!def) error("la classe " + c->name + " non ha un costruttore predefinito (serve per gli array)", at);
    const std::int32_t arr = addLocal("", tDyn_, at).slot;
    const std::int32_t i = addLocal("", tI64_, at).slot;
    emit(Op::StoreL, arr);
    pushInt(0, VK::I64);
    emit(Op::StoreL, i);
    const std::int32_t start = here();
    emit(Op::LoadL, i);
    emit(Op::LoadL, arr);
    emit(Op::ArrLen, 0, 1);
    emit(Op::Lt);
    const std::int32_t jz = emit(Op::Jz, 0);
    emit(Op::LoadL, arr);
    emit(Op::LoadL, i);
    emit(Op::GetE);
    Node* decl = fnDecls_.count(def) ? fnDecls_[def] : nullptr;
    for (std::size_t k = 0; k < def->params.size(); ++k) {
        Node* dv = decl && k < decl->list.size() ? decl->list[k]->a : nullptr;
        if (!dv) error("valore predefinito mancante", at);
        rvalue(dv, def->params[k].type);
    }
    emit(Op::Call, def->index, static_cast<std::uint8_t>(1 + def->params.size()));
    emit(Op::IncL, i, 0);
    emit(Op::Pop);
    emit(Op::Jmp, start);
    patch(jz, here());
    emit(Op::LoadL, arr);
}

// ================================================================ lambda
void Compiler::collectCaptures(Node* body) {
    // C#: i nomi usati dentro le lambda vanno in una Box nella funzione che le contiene
    std::vector<Node*> todo{body};
    while (!todo.empty()) {
        Node* n = todo.back();
        todo.pop_back();
        if (!n) continue;
        if (n->k == K::Lambda) {
            std::vector<Node*> inner{n->b};
            while (!inner.empty()) {
                Node* m = inner.back();
                inner.pop_back();
                if (!m) continue;
                if (m->k == K::Name) csCaptured_.insert(syms_[m->sym]);
                inner.push_back(m->a); inner.push_back(m->b); inner.push_back(m->c); inner.push_back(m->d);
                for (Node* x : m->list) inner.push_back(x);
                for (Node* x : m->list2) inner.push_back(x);
            }
        }
        todo.push_back(n->a); todo.push_back(n->b); todo.push_back(n->c); todo.push_back(n->d);
        for (Node* x : n->list) todo.push_back(x);
        for (Node* x : n->list2) todo.push_back(x);
    }
}

bool Compiler::captureName(const std::string& nm, Pos at, EV& out) {
    LambdaCtx* lc = lambdas_.back();
    // gia' catturata?
    for (std::size_t i = 0; i < lc->captured.size(); ++i) {
        if (lc->captured[i].first != nm) continue;
        if (lc->byRef[i]) { emit(Op::LoadCap, lc->captured[i].second); out.cat = EV::Ptr; }
        else { out.cat = EV::Cap; out.slot = lc->captured[i].second; }
        out.type = lc->sources[i].type;
        out.lvalue = true;
        out.isConst = !lc->byRef[i];
        return true;
    }
    // variabile della funzione esterna (o di una lambda esterna)
    LocalVar* found = nullptr;
    for (std::size_t i = lc->outerScopes.size(); i-- > 0 && !found;)
        for (auto it = lc->outerScopes[i].vars.rbegin(); it != lc->outerScopes[i].vars.rend(); ++it) if (it->name == nm) { found = &*it; break; }
    EV src;
    bool viaParent = false;
    if (found) {
        if (found->global >= 0) return false;
        src.type = found->type;
        src.local = found;
    } else {
        // lambda annidata: la cattura la fa prima la lambda esterna
        if (lambdas_.size() < 2) return false;
        LambdaCtx* saveTop = lambdas_.back();
        lambdas_.pop_back();
        EV parent;
        auto saveFn = fn_;
        // la cattura della lambda esterna si registra senza emettere codice qui
        auto saveCode = fn_->code.size();
        bool ok = captureName(nm, at, parent);
        fn_->code.resize(saveCode); fn_->pos.resize(saveCode); fn_->stmt.resize(saveCode);
        fn_ = saveFn;
        lambdas_.push_back(saveTop);
        if (!ok) return false;
        viaParent = true;
        src = parent;
    }
    bool byRef = false;
    const bool explicitRef = std::find(lc->explicitRef.begin(), lc->explicitRef.end(), nm) != lc->explicitRef.end();
    const bool explicitVal = std::find(lc->explicitVal.begin(), lc->explicitVal.end(), nm) != lc->explicitVal.end();
    if (lang_ == Dialect::Cpp) {
        if (explicitRef) byRef = true;
        else if (explicitVal) byRef = false;
        else if (lc->defaultCapture == 2) byRef = true;
        else if (lc->defaultCapture == 1) byRef = false;
        else error("'" + nm + "' non e' catturata dalla lambda (usa [=], [&] o [" + nm + "])", at);
    } else {
        byRef = true;                                    // C#: per riferimento (Box)
        if (found && !found->boxed && !found->isRef) byRef = false;   // parametro non catturabile: per valore
    }
    if (found && found->isRef) byRef = true;
    const std::int32_t idx = static_cast<std::int32_t>(lc->captured.size());
    lc->captured.push_back({nm, idx});
    lc->byRef.push_back(byRef);
    if (viaParent) {
        EV s;
        s.type = src.type;
        s.cat = EV::Cap;
        s.slot = src.slot;
        s.components = src.cat == EV::Ptr ? 1 : 0;       // 1: la cattura del padre e' gia' un puntatore
        s.local = nullptr;
        lc->sources.push_back(s);
    } else lc->sources.push_back(src);
    if (byRef) { emit(Op::LoadCap, idx); out.cat = EV::Ptr; }
    else { out.cat = EV::Cap; out.slot = idx; }
    out.type = src.type;
    out.lvalue = true;
    out.isConst = !byRef;
    return true;
}
EV Compiler::captureThis(Pos at) {
    LambdaCtx* lc = lambdas_.back();
    for (std::size_t i = 0; i < lc->captured.size(); ++i) if (lc->captured[i].first == "#this") {
        EV e; e.cat = EV::Cap; e.slot = lc->captured[i].second; e.type = lc->sources[i].type; return e;
    }
    if (lang_ == Dialect::Cpp && !lc->captureThis && lc->defaultCapture == 0) error("this non catturato dalla lambda (usa [this] o [=])", at);
    const std::int32_t idx = static_cast<std::int32_t>(lc->captured.size());
    lc->captured.push_back({"#this", idx});
    lc->byRef.push_back(false);
    EV s;
    s.type = lc->outerCls ? (lang_ == Dialect::Cpp ? ptrTo(classType(lc->outerCls)) : classType(lc->outerCls)) : tDyn_;
    s.cat = EV::Local;
    s.slot = 0;
    s.slot = -100;                                       // this del creatore
    lc->sources.push_back(s);
    EV e;
    e.cat = EV::Cap;
    e.slot = idx;
    e.type = s.type;
    return e;
}

EV Compiler::lambda(Node* n, TypeP want) {
    EV r;
    if (probing_) {
        r.cat = EV::FuncGroup;
        r.slot = static_cast<std::int32_t>(n->list.size());
        r.type = want ? want : tDyn_;
        return r;
    }
    want = want ? stripRef(want) : nullptr;
    if (want && want->k == TK::Ptr && want->elem && want->elem->k == TK::Func) {
        if (!n->list2.empty() || n->flags) error("una lambda con catture non diventa un puntatore a funzione", n->pos);
        want = want->elem;
    }
    // tipi dei parametri e del risultato
    std::vector<TypeP> ps;
    for (std::size_t i = 0; i < n->list.size(); ++i) {
        Node* p = n->list[i];
        if (p->type && p->type->f != TF::Auto) ps.push_back(resolveType(p->type, false));
        else if (want && want->k == TK::Func && i < want->args.size()) ps.push_back(want->args[i]);
        else if (lang_ == Dialect::Cpp && p->type && p->type->f == TF::Auto) error("lambda generiche (parametri auto) non supportate", p->pos);
        else error("tipo del parametro della lambda sconosciuto", p->pos);
    }
    TypeP ret = nullptr;
    if (n->type) ret = resolveType(n->type, true);
    else if (want && want->k == TK::Func && want->ret && !(want->ret->k == TK::Dynamic && want->ret->size == -2)) ret = want->ret;
    if (ret && ret->k == TK::Dynamic) ret = nullptr;
    LambdaCtx lc;
    lc.outerFn = fn_;
    lc.outerScopes = scopes_;
    lc.defaultCapture = n->flags == 1 ? 1 : n->flags == 2 ? 2 : 0;
    lc.outerCls = cls_;
    for (Node* c : n->list2) {
        if (c->k == K::This) { lc.captureThis = true; continue; }
        if (c->mods & MRef) lc.explicitRef.push_back(syms_[c->sym]); else lc.explicitVal.push_back(syms_[c->sym]);
    }
    Function* f = newFunction("lambda#" + std::to_string(++lambdaCount_), lang_, nullptr);
    f->isLambda = true;
    f->isStatic = true;
    f->hasThis = false;
    f->defined = true;
    f->library = library_;
    f->file = n->pos.file;
    f->line = static_cast<int>(n->pos.line);
    f->column = n->pos.col;
    for (std::size_t i = 0; i < ps.size(); ++i) {
        Param p;
        p.name = syms_[n->list[i]->sym];
        p.type = ps[i];
        p.byRef = ps[i]->k == TK::Ref;
        f->params.push_back(p);
    }
    TypeRef autoT;
    autoT.k = TK::Dynamic;
    autoT.size = -2;
    f->ret = ret ? ret : intern(autoT);
    lc.fn = f;
    // stato della funzione esterna
    Function* saveFn = fn_;
    auto saveScopes = std::move(scopes_);
    auto saveLoops = std::move(loops_);
    auto saveFin = std::move(finallies_);
    auto saveLabels = std::move(labels_);
    auto saveGotos = std::move(gotos_);
    auto saveFree = std::move(freeSlots_);
    auto saveCatch = std::move(catchSlots_);
    auto saveSwitch = std::move(switchCases_);
    auto saveCaptured = csCaptured_;
    const std::int32_t saveRet = retSlot_;
    const Pos savePos = pos_;
    ClassInfo* saveCls = cls_;
    const bool saveStmt = stmtStart_;
    lambdas_.push_back(&lc);
    // C++ [] senza catture: una funzione semplice (puntatore a funzione), parametri da 0;
    // altrimenti slot 0 = la chiusura e parametri da 1
    const bool plainFn = lang_ == Dialect::Cpp && n->list2.empty() && !n->flags;
    const std::int32_t first = plainFn ? 0 : 1;
    beginFunction(f);
    f->nlocals = first + static_cast<std::int32_t>(f->params.size());
    for (std::size_t i = 0; i < f->params.size(); ++i) {
        LocalVar v;
        v.name = f->params[i].name;
        v.type = f->params[i].byRef ? f->params[i].type->elem : f->params[i].type;
        v.isRef = f->params[i].byRef;
        v.slot = static_cast<std::int32_t>(i) + first;
        scopes_.back().vars.push_back(v);
        LocalInfo li; li.name = v.name; li.type = f->params[i].type; li.slot = v.slot; f->locals.push_back(li);
    }
    csCaptured_.clear();
    if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) collectCaptures(n->b);
    markStatement();
    for (Node* s : n->b->list) statement(s);
    if (f->ret->k == TK::Dynamic && f->ret->size == -2) f->ret = tVoid_;
    emitScopeCleanup(scopes_.back());
    if (f->ret->k == TK::Void) emit(Op::Ret, 0, 0);
    else if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) emit(Op::Fault, static_cast<std::int32_t>(FaultKind::InvalidOp), 0, static_cast<std::uint16_t>(constString("la lambda finisce senza return", false)));
    else { zeroValue(f->ret, pos_); emit(Op::Ret, 0, 1); }
    endFunction();
    lambdas_.pop_back();
    fn_ = saveFn;
    scopes_ = std::move(saveScopes);
    loops_ = std::move(saveLoops);
    finallies_ = std::move(saveFin);
    labels_ = std::move(saveLabels);
    gotos_ = std::move(saveGotos);
    freeSlots_ = std::move(saveFree);
    catchSlots_ = std::move(saveCatch);
    switchCases_ = std::move(saveSwitch);
    csCaptured_ = saveCaptured;
    retSlot_ = saveRet;
    pos_ = savePos;
    cls_ = saveCls;
    stmtStart_ = saveStmt;
    // creazione della chiusura nella funzione esterna: i valori catturati
    for (std::size_t i = 0; i < lc.sources.size(); ++i) {
        const EV& s = lc.sources[i];
        if (s.slot == -100 && s.cat == EV::Local) {
            if (fn_->hasThis) emit(Op::LoadL, 0);
            else if (!lambdas_.empty()) { EV t = captureThis(n->pos); load(t, n->pos); }
            else error("this non disponibile per la lambda", n->pos);
            continue;
        }
        if (s.cat == EV::Cap) {
            // cattura di una lambda esterna
            emit(Op::LoadCap, s.slot);
            if (!lc.byRef[i] && s.components == 1) emit(Op::LoadP);
            continue;
        }
        LocalVar* v = s.local;
        if (!v) error("cattura non valida", n->pos);
        if (lc.byRef[i]) {
            if (v->isRef) emit(Op::LoadL, v->slot);
            else if (v->boxed) emit(Op::AddrL, v->slot, 1);
            else emit(Op::AddrL, v->slot);
        } else {
            if (v->isRef) { emit(Op::LoadL, v->slot); emit(Op::LoadP); }
            else if (v->boxed) emit(Op::LoadBox, v->slot);
            else emit(Op::LoadL, v->slot);
            if (isValueType(v->type)) emit(Op::Copy, v->type->k == TK::Class && v->type->cls ? v->type->cls->id : -1);
        }
    }
    if (plainFn) {
        if (!lc.sources.empty()) error("la lambda [] usa una variabile locale senza catturarla (scrivi [&] o [=])", n->pos);
        emit(Op::PushFn, f->index);
    } else emit(Op::MkClosure, f->index, static_cast<std::uint8_t>(lc.sources.size()));
    std::vector<TypeP> fps;
    for (const Param& p : f->params) fps.push_back(p.type);
    r.type = funcOf(f->ret, fps);
    return r;
}

// ================================================================ C#: interpolazione, inizializzatori
EV Compiler::interpolation(Node* n) {
    EV r;
    r.type = tCsStr_;
    if (n->list.empty()) { pushString("", lang_, false); return r; }
    bool first = true;
    for (Node* part : n->list) {
        if (part->k == K::StrLit) pushString(units_[unit_].strings[part->str], lang_, false);
        else {
            // {valore,allineamento:formato}
            const TypeP ht = rvalue(part->a, nullptr);
            if (ht && stripRef(ht)->k == TK::Enum && lang_ == Dialect::CSharp) callBuiltinOnStack("enum", "#box", "any(self)", 1, stripRef(ht));
            const std::string fmt = part->str ? units_[unit_].strings[part->str - 1] : std::string();
            pushString(fmt, lang_, false);
            pushInt(part->ival, VK::I32);
            emit(Op::CallN, nativeIndex("", "#format", "#hole", "string(any,string,int)"), 3, 1);
        }
        if (!first) emit(Op::Concat);
        first = false;
    }
    return r;
}
EV Compiler::objInit(Node* n) {
    EV obj = newExpr(n->a);
    TypeP t = obj.type;
    for (Node* item : n->list) {
        if (item->k == K::Label) {
            // Proprieta = valore
            emit(Op::Dup);
            EV base; base.type = t; base.cat = EV::RValue;
            EV m = memberOfType(base, t, syms_[item->sym], item);
            if (!m.lvalue) error("'" + syms_[item->sym] + "' non si assegna", item->pos);
            if (item->a->k == K::InitList) {
                // Lista = { 1, 2 }: elementi aggiunti alla collezione esistente
                load(m, item->pos);
                for (Node* x : item->a->list) { emit(Op::Dup); EV recv; recv.type = stripRef(m.type); std::vector<Node*> one{x}; EV rr = methodOn(recv, "Add", nullptr, &one, x->pos); if (rr.cat != EV::Void) emit(Op::Pop); }
                emit(Op::Pop);
                continue;
            }
            rvalue(item->a, stripRef(m.type));
            store(m, false, item->pos);
            continue;
        }
        if (item->k == K::Index) {
            // [chiave] = valore
            emit(Op::Dup);
            EV base; base.type = t;
            Node fake;
            std::vector<std::int32_t> nat = nativeMembers(ownerKey(t), "operator[]", 0, 0);
            if (nat.empty()) error("indicizzatore non disponibile", item->pos);
            EV loc = callOverloads({}, nat, {}, item->list, {}, item->pos, true, t, false, "[]");
            rvalue(item->b, loc.type);
            store(loc, false, item->pos);
            continue;
        }
        // collezione: Add(valore) o Add(chiave, valore)
        emit(Op::Dup);
        EV recv;
        recv.type = t;
        std::vector<Node*> args = item->k == K::InitList ? item->list : std::vector<Node*>{item};
        bool userAdd = false;
        if (t->k == TK::Class && t->cls) {
            bool found = false;
            std::vector<Function*> ms = findMethods(t->cls, "Add", &found);
            if (found) { EV rr = callOverloads(ms, {}, {}, args, {}, item->pos, true, t, false, "Add"); if (rr.cat != EV::Void) emit(Op::Pop); userAdd = true; }
        }
        if (!userAdd) { EV rr = methodOn(recv, "Add", nullptr, &args, item->pos); if (rr.cat != EV::Void) emit(Op::Pop); }
    }
    EV r;
    r.type = t;
    return r;
}
void Compiler::disposeValue(TypeP t, Pos at) {
    t = stripRef(t);
    if (t->k == TK::Class && t->cls) {
        bool found = false;
        std::vector<Function*> ms = findMethods(t->cls, "Dispose", &found);
        if (!found) error("using vuole un oggetto con Dispose()", at);
        EV r = callOverloads(ms, {}, {}, {}, {}, at, true, t, false, "Dispose");
        if (r.cat != EV::Void) emit(Op::Pop);
        return;
    }
    std::vector<std::int32_t> nat = nativeMembers(ownerKey(t), "Dispose", 0, NF::Static);
    if (nat.empty()) error("using vuole un oggetto con Dispose()", at);
    EV r = callOverloads({}, nat, {}, {}, {}, at, true, t, false, "Dispose");
    if (r.cat != EV::Void) emit(Op::Pop);
}

// Liste { } per i contenitori del C++ e le stringhe.
void Compiler::listToNative(TypeP t, Node* list, Pos at) {
    if (t->k == TK::Str) {
        if (list->list.size() == 1) { rvalue(list->list[0], t); return; }
        if (list->list.empty()) { pushString("", Dialect::Cpp, true); return; }
        error("std::string{...} con piu' valori", at);
    }
    emit(Op::NewNative, t->index);
    TypeP e = t->args.empty() ? tDyn_ : t->args[0];
    const bool mapLike = t->nk == NK::Map || t->nk == NK::UMap || t->nk == NK::CsDict;
    if (t->nk == NK::Pair) {
        if (list->list.size() != 2) error("una coppia vuole due valori", at);
        for (int i = 0; i < 2; ++i) {
            emit(Op::Dup);
            rvalue(list->list[static_cast<std::size_t>(i)], t->args[static_cast<std::size_t>(i)]);
            emit(Op::SetF, i);
        }
        return;
    }
    for (Node* item : list->list) {
        emit(Op::Dup);
        if (mapLike) {
            if (item->k != K::InitList || item->list.size() != 2) error("una mappa si inizializza con { {chiave, valore}, ... }", item->pos);
            rvalue(item->list[0], t->args[0]);
            rvalue(item->list[1], t->args[1]);
            callBuiltinOnStack("#init", "put", "void(self,any,any)", 3, t);
        } else {
            if (item->k == K::InitList) initFromList(e, item, item->pos); else rvalue(item, e);
            callBuiltinOnStack("#init", "add", "void(self,any)", 2, t);
        }
    }
}

} // namespace fx::prog
