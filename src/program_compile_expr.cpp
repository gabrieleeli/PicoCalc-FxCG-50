// Compilatore dei programmi (v1.20.0), parte 4: espressioni. Luoghi (EV)
// caricati e scritti, conversioni implicite ed esplicite con le regole di
// ogni linguaggio, aritmetica con le promozioni, nomi, membri, indici,
// chiamate con la risoluzione degli overload (funzioni dell'utente, template
// dedotti, funzioni native), operatori dell'utente, cast, new/delete,
// lambda con le catture, stringhe interpolate, inizializzatori di C#.
#include "program_compile.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace fx::prog {
using namespace ast;

// ================================================================ luoghi
void Compiler::load(EV& e, Pos at) {
    switch (e.cat) {
    case EV::RValue: break;
    case EV::Void: error("un'espressione void non ha valore", at);
    case EV::Local: emit(Op::LoadL, e.slot); break;
    case EV::LocalBox: emit(Op::LoadBox, e.slot); break;
    case EV::Global: emit(Op::LoadG, e.slot); break;
    case EV::Field: emit(Op::GetF, e.slot); break;
    case EV::Elem: emit(Op::GetE); break;
    case EV::Elem2: emit(Op::GetE2); break;
    case EV::Ptr: emit(Op::LoadP); break;
    case EV::Cap: emit(Op::LoadCap, e.slot); break;
    case EV::Prop: case EV::Indexer: {
        Function* g = e.getter;
        if (!g && e.cat == EV::Indexer && e.natives.size() == 2) {
            // indicizzatore nativo (Dictionary, List di C#): #get
            emit(Op::CallN, e.natives[0], static_cast<std::uint8_t>(componentsOf(e)), 1);
            fn_->code.back().c = static_cast<std::uint16_t>(1 | ((e.slot & 0x7FFF) << 1));
            break;
        }
        if (!g) error("la proprieta' " + (e.prop ? e.prop->name : std::string("?")) + " non ha get", at);
        checkAccess(g->access, g->owner, "get di " + (e.prop ? e.prop->name : g->name), at);
        const std::uint8_t argc = static_cast<std::uint8_t>(componentsOf(e));
        if (g->isVirtual && g->vslot >= 0 && !e.superCall) emit(Op::CallV, g->vslot, argc);
        else if (g->owner && g->owner->isInterface) emit(Op::CallI, selector(g->simple, g->params.size()), argc);
        else emit(Op::Call, g->index, argc);
        break;
    }
    case EV::FuncGroup: {
        if (e.fns.size() == 1 && e.natives.empty() && e.templates.empty()) {
            Function* f = e.fns[0];
            if (e.hasReceiver) {
                // metodo legato a un oggetto (delegato di C#)
                emit(Op::MkClosure, f->index, 1, 1);
            } else emit(Op::PushFn, f->index);
            std::vector<TypeP> ps;
            for (const Param& p : f->params) ps.push_back(p.type);
            e.type = funcOf(f->ret, ps);
            break;
        }
        if (!e.natives.empty() && e.fns.empty() && !e.hasReceiver) {
            // integra(sin, 0, pi): la funzione della libreria passa da una funzione che la chiama
            Function* w = nativeWrapper(e.natives, at);
            emit(Op::PushFn, w->index);
            std::vector<TypeP> ps;
            for (const Param& p : w->params) ps.push_back(p.type);
            e.type = funcOf(w->ret, ps);
            break;
        }
        if (!e.natives.empty() && e.fns.empty()) error("una funzione della libreria non si usa come valore", at);
        error("nome di funzione con piu' versioni: non si sa quale usare", at);
    }
    case EV::TypeName: error("'" + typeName(e.type) + "' e' un tipo, non un valore", at);
    case EV::NsName: error("un namespace non e' un valore", at);
    }
    e.cat = EV::RValue;
}

void Compiler::store(EV& e, bool keep, Pos at) {
    if (e.isConst) error("assegnazione a una costante", at);
    const std::uint8_t k = keep ? 1 : 0;
    switch (e.cat) {
    case EV::Local: emit(Op::StoreL, e.slot, k); return;
    case EV::LocalBox: emit(Op::StoreBox, e.slot, k); return;
    case EV::Global: emit(Op::StoreG, e.slot, k); return;
    case EV::Field: emit(Op::SetF, e.slot, k); return;
    case EV::Elem: emit(Op::SetE, 0, k); return;
    case EV::Elem2: emit(Op::SetE2, 0, k); return;
    case EV::Ptr: emit(Op::StoreP, 0, k); return;
    case EV::Prop: case EV::Indexer: {
        Function* s = e.setter;
        if (!s && e.cat == EV::Indexer && e.natives.size() == 2) {
            const std::int32_t comps = componentsOf(e);
            if (keep) emit(Op::DupX, 0, static_cast<std::uint8_t>(comps));
            emit(Op::CallN, e.natives[1], static_cast<std::uint8_t>(comps + 1), 0);
            fn_->code.back().c = static_cast<std::uint16_t>((e.slot & 0x7FFF) << 1);
            return;
        }
        if (!s && e.cat == EV::Prop && e.prop && !e.prop->backingName.empty() && fn_ && fn_->owner == e.prop->owner &&
            ((!e.prop->isStatic && fn_->isCtor && fn_->hasThis) || (e.prop->isStatic && fn_->owner->staticInit == fn_))) {
            // proprieta' automatica { get; }: si assegna nel costruttore (il campo nascosto)
            ClassInfo* own = nullptr;
            if (const FieldInfo* bf = findField(e.prop->owner, e.prop->backingName, &own)) {
                if (e.prop->isStatic) emit(Op::StoreG, bf->slot, k);
                else emit(Op::SetF, bf->slot, k);
                return;
            }
        }
        if (!s) error("la proprieta' " + (e.prop ? e.prop->name : std::string("?")) + " e' di sola lettura", at);
        checkAccess(s->access, s->owner, "set di " + (e.prop ? e.prop->name : s->name), at);
        const std::int32_t comps = componentsOf(e);
        if (keep) emit(Op::DupX, 0, static_cast<std::uint8_t>(comps));
        const std::uint8_t argc = static_cast<std::uint8_t>(comps + 1);
        if (s->isVirtual && s->vslot >= 0 && !e.superCall) emit(Op::CallV, s->vslot, argc);
        else if (s->owner && s->owner->isInterface) emit(Op::CallI, selector(s->simple, s->params.size()), argc);
        else emit(Op::Call, s->index, argc);
        return;
    }
    case EV::Cap: error("variabile catturata per valore: non si assegna (usa [&] o mutable)", at);
    default: break;
    }
    error("l'espressione a sinistra non si puo' assegnare", at);
}

// ================================================================ conversioni
namespace {
int rankOf(TK k) {
    switch (k) {
    case TK::Bool: return 0;
    case TK::C8: case TK::I8: case TK::U8: return 1;
    case TK::C16: case TK::I16: case TK::U16: return 2;
    case TK::I32: case TK::U32: return 3;
    case TK::I64: case TK::U64: return 4;
    case TK::F32: return 5;
    case TK::F64: return 6;
    default: return -1;
    }
}
bool unsignedK(TK k) { return k == TK::U8 || k == TK::U16 || k == TK::U32 || k == TK::U64 || k == TK::C16 || k == TK::Bool; }
bool intK(TK k) { return k >= TK::Bool && k <= TK::U64; }
// C#: conversione implicita numerica (tabella della norma).
bool csImplicit(TK from, TK to) {
    if (from == to) return true;
    switch (from) {
    case TK::I8: return to == TK::I16 || to == TK::I32 || to == TK::I64 || to == TK::F32 || to == TK::F64;
    case TK::U8: return to == TK::I16 || to == TK::U16 || to == TK::I32 || to == TK::U32 || to == TK::I64 || to == TK::U64 || to == TK::F32 || to == TK::F64;
    case TK::I16: return to == TK::I32 || to == TK::I64 || to == TK::F32 || to == TK::F64;
    case TK::U16: case TK::C16: return to == TK::I32 || to == TK::U32 || to == TK::I64 || to == TK::U64 || to == TK::F32 || to == TK::F64 || (from == TK::C16 && to == TK::U16);
    case TK::I32: return to == TK::I64 || to == TK::F32 || to == TK::F64;
    case TK::U32: return to == TK::I64 || to == TK::U64 || to == TK::F32 || to == TK::F64;
    case TK::I64: case TK::U64: return to == TK::F32 || to == TK::F64;
    case TK::F32: return to == TK::F64;
    default: return false;
    }
}
bool javaImplicit(TK from, TK to) {
    if (from == to) return true;
    const int rf = rankOf(from), rt = rankOf(to);
    if (from == TK::C16) return to == TK::I32 || to == TK::I64 || to == TK::F32 || to == TK::F64;
    if (to == TK::C16) return false;
    return rf >= 1 && rt > rf && from != TK::Bool;
}
bool fitsIn(std::int64_t v, TK k) {
    switch (k) {
    case TK::I8: return v >= -128 && v <= 127;
    case TK::U8: return v >= 0 && v <= 255;
    case TK::I16: return v >= -32768 && v <= 32767;
    case TK::U16: case TK::C16: return v >= 0 && v <= 65535;
    case TK::I32: return v >= INT32_MIN && v <= INT32_MAX;
    case TK::U32: return v >= 0 && v <= 4294967295LL;
    case TK::I64: return true;
    case TK::U64: return v >= 0;
    default: return true;
    }
}
} // namespace

int Compiler::convCost(TypeP from, TypeP to, const EV* src) const {
    from = from && from->k == TK::Ref ? from->elem : from;
    TypeP toBase = to && to->k == TK::Ref ? to->elem : to;
    if (!from || !toBase) return -1;
    if (from == toBase) return 0;
    if (toBase->k == TK::Dynamic && toBase->size == -4) {
        // anyiter: iteratori di qualsiasi contenitore e puntatori (costruttori da un intervallo)
        if (from->k == TK::Ptr || from->k == TK::Array || (from->k == TK::Native && (from->nk == NK::Iter || from->nk == NK::MapIter))) return 1;
        return -1;
    }
    if (toBase->k == TK::Dynamic) return toBase->size == -3 ? 3 : 1;   // object: dopo le conversioni vere
    if (from->k == TK::Dynamic) return 2;
    const bool cs = lang_ == Dialect::CSharp, java = lang_ == Dialect::Java, cfam = lang_ == Dialect::C || lang_ == Dialect::Cpp;
    // numeri
    const bool fn = isNumeric(from), tn = isNumeric(toBase);
    if (fn && tn) {
        TK fk = from->k == TK::Enum ? TK::I32 : from->k, tk = toBase->k == TK::Enum ? TK::I32 : toBase->k;
        if (from->k == TK::Enum && toBase->k == TK::Enum) return -1;
        if (toBase->k == TK::Enum) return cfam && lang_ == Dialect::C ? 2 : (src && src->hasConst && src->constValue.i == 0 && cs ? 2 : -1);
        if (from->k == TK::Enum) return lang_ == Dialect::Cpp ? (from->cls && classNodes_.count(from->cls) && classNodes_.at(from->cls) && classNodes_.at(from->cls)->op == 5 ? -1 : 2) : lang_ == Dialect::C ? 1 : -1;
        if (cfam) {
            if (fk == TK::Bool || tk == TK::Bool) return 2;
            if (rankOf(fk) < 3 && tk == TK::I32) return 1;               // promozione intera
            if (fk == TK::F32 && tk == TK::F64) return 1;
            return 2;
        }
        if (fk == TK::Bool || tk == TK::Bool) return -1;
        if (cs ? csImplicit(fk, tk) : javaImplicit(fk, tk)) return rankOf(tk) == rankOf(fk) + 1 ? 1 : 2;
        if (src && src->hasConst && intK(fk) && intK(tk) && tk != TK::C16 && fitsIn(src->constValue.i, tk)) return 2;
        return -1;
    }
    switch (toBase->k) {
    case TK::Bool:
        if (cfam && (from->k == TK::Ptr || from->k == TK::Null || from->k == TK::Array || from->k == TK::Func)) return 2;
        return -1;
    case TK::Ptr: {
        if (from->k == TK::Null) return 1;
        if (src && src->hasConst && src->constValue.i == 0 && isIntegral(from) && cfam) return 2;   // 0 come puntatore nullo
        TypeP fe = from->k == TK::Ptr ? from->elem : from->k == TK::Array ? from->elem : nullptr;
        if (from->k == TK::Func && toBase->elem && toBase->elem->k == TK::Func) return from == toBase->elem ? 0 : -1;
        if (!fe) return -1;
        if (fe == toBase->elem) return from->k == TK::Array ? 0 : 0;
        if (toBase->elem->k == TK::Void) return 2;
        if (fe->k == TK::Void) return lang_ == Dialect::C ? 2 : 2;    // void* -> T* (il C++ vuole un cast: tollerato)
        if (fe->k == TK::Class && toBase->elem->k == TK::Class && fe->cls && toBase->elem->cls && fe->cls->derivesFrom(toBase->elem->cls)) return 1;
        if (fe->k == TK::C8 && toBase->elem->k == TK::C8) return 0;
        if ((fe->k == TK::C8 || fe->k == TK::I8 || fe->k == TK::U8) && (toBase->elem->k == TK::C8 || toBase->elem->k == TK::I8 || toBase->elem->k == TK::U8)) return 2;
        return -1;
    }
    case TK::Str:
        if (toBase->cppValue) {
            if (from->k == TK::Str && from->cppValue) return 0;
            if (from->k == TK::Array && from->elem && from->elem->k == TK::C8) return 2;
            if (from->k == TK::Ptr && from->elem && from->elem->k == TK::C8) return 2;
            return -1;
        }
        if (from->k == TK::Null) return 1;
        if (from->k == TK::Str) return 0;
        return -1;
    case TK::Class: {
        if (from->k == TK::Null) return toBase->cls && !toBase->cls->valueType ? 1 : -1;
        if (from->k == TK::Class && from->cls && toBase->cls && from->cls->derivesFrom(toBase->cls)) return 1;
        if (toBase->cls && toBase->cls->valueType && lang_ == Dialect::Cpp && !toBase->cls->ctors.empty()) {
            // costruttore di conversione (non explicit) con un argomento
            for (Function* c : toBase->cls->ctors) {
                if (c->params.size() < 1 || (fnDecls_.count(c) && fnDecls_.at(c) && (fnDecls_.at(c)->mods & MExplicit))) continue;
                bool rest = true;
                for (std::size_t i = 1; i < c->params.size(); ++i) if (!c->params[i].hasDefault) rest = false;
                if (!rest) continue;
                if (from->k == TK::Class && from->cls == toBase->cls) continue;
                const int k = convCost(from, c->params[0].type, src);
                if (k >= 0 && k <= 2) return 3;
            }
        }
        return -1;
    }
    case TK::Nullable:
        if (from->k == TK::Null) return 1;
        if (from == toBase->elem) return 1;
        if (isNumeric(from) && isNumeric(toBase->elem)) { const int k = convCost(from, toBase->elem, src); return k < 0 ? -1 : k + 1; }
        return -1;
    case TK::Func:
        if (from->k == TK::Null) return 1;
        if (from->k == TK::Func) return from == toBase ? 0 : (from->args.size() == toBase->args.size() ? 2 : -1);
        if (from->k == TK::Ptr && from->elem && from->elem->k == TK::Func) return from->elem == toBase ? 0 : -1;
        return -1;
    case TK::Native:
        if (from->k == TK::Null) return isValueType(toBase) ? -1 : 1;
        if (from->k == TK::Native && from->nk == toBase->nk && from->args.size() == toBase->args.size()) {
            for (std::size_t i = 0; i < from->args.size(); ++i) if (from->args[i] != toBase->args[i] && toBase->args[i]->k != TK::Dynamic) return -1;
            return 0;
        }
        return -1;
    case TK::Array:
        if (from->k == TK::Null) return cs || java ? 1 : -1;
        if (from->k == TK::Array && from->elem == toBase->elem && from->rank == toBase->rank) return 0;
        if (from->k == TK::Array && toBase->elem && toBase->elem->k == TK::Dynamic) return 2;
        return -1;
    default: return -1;
    }
}

void Compiler::convert(TypeP from, TypeP to, Pos at, bool explicitCast, const EV* src) {
    if (!from || !to) return;
    if (to->k == TK::Ref) to = to->elem;
    if (from->k == TK::Ref) from = from->elem;
    if (from == to) return;
    if (to->k == TK::Void) { if (explicitCast) { emit(Op::Pop); emit(Op::PushVoid); return; } error("valore usato come void", at); }
    if (from->k == TK::Void) error("un'espressione void non ha valore", at);
    if (to->k == TK::Dynamic) {
        // C#: un enum diventa object con il suo tipo (Console.WriteLine(Giorno.Mar) stampa "Mar")
        if (lang_ == Dialect::CSharp && from->k == TK::Enum && from->cls) callBuiltinOnStack("enum", "#box", "any(self)", 1, from);
        return;
    }
    const bool cs = lang_ == Dialect::CSharp, java = lang_ == Dialect::Java, cfam = lang_ == Dialect::C || lang_ == Dialect::Cpp;
    if (from->k == TK::Dynamic) {
        // object -> tipo: controllo a tempo di esecuzione
        if (isNumeric(to)) { emit(Op::Conv, 0, static_cast<std::uint8_t>(vkOf(to)), 1); return; }
        if (to->k == TK::Class && to->cls) { emit(Op::CastCk, to->cls->id); return; }
        emit(Op::ConvT, to->index);
        return;
    }
    const int cost = convCost(from, to, src);
    const bool fnum = isNumeric(from), tnum = isNumeric(to);
    if (fnum && tnum) {
        if (cost < 0 && !explicitCast) {
            if (to->k == TK::Enum || from->k == TK::Enum) error("conversione implicita tra enum e int non ammessa: usa un cast", at);
            if ((cs || java) && (from->k == TK::Bool || to->k == TK::Bool)) error("bool e numeri non si convertono in " + std::string(cs ? "C#" : "Java"), at);
            error("conversione implicita da " + typeName(from) + " a " + typeName(to) + " non possibile: usa un cast", at);
        }
        if ((cs || java) && (from->k == TK::Bool || to->k == TK::Bool)) error("bool e numeri non si convertono", at);
        if (vkOf(from) != vkOf(to)) emit(Op::Conv, 0, static_cast<std::uint8_t>(vkOf(to)));
        return;
    }
    switch (to->k) {
    case TK::Bool:
        if (cfam && (from->k == TK::Ptr || from->k == TK::Null || from->k == TK::Array || from->k == TK::Func)) { emit(Op::ToBool); return; }
        if (lang_ == Dialect::Cpp && explicitCast && from->k == TK::Native &&
            (from->nk == NK::IStream || from->nk == NK::OStream || from->nk == NK::IFStream || from->nk == NK::OFStream || from->nk == NK::FStream || from->nk == NK::StringStream)) {
            // (bool)f, static_cast<bool>(cin): lo stato del flusso (operator bool esplicito)
            callBuiltinOnStack("ios", "#bool", "bool(self)", 1, from);
            return;
        }
        break;
    case TK::Ptr: {
        if (from->k == TK::Null) return;
        if (isIntegral(from) && cfam) {
            if (src && src->hasConst && src->constValue.i == 0) { emit(Op::Pop); emit(Op::PushNull); return; }
            error("conversione tra interi e puntatori non supportata", at);
        }
        if (from->k == TK::Func && to->elem && to->elem->k == TK::Func) {
            if (from != to->elem) error("puntatore a funzione con una firma diversa", at);
            return;
        }
        TypeP fe = from->k == TK::Ptr ? from->elem : from->k == TK::Array ? from->elem : nullptr;
        if (!fe) break;
        if (from->k == TK::Array) emit(Op::ConvT, to->index);                    // decadimento: &a[0]
        if (fe == to->elem) return;
        if (to->elem->k == TK::Void) return;
        if (fe->k == TK::Void) { emit(Op::ConvT, to->index); return; }           // tipizza i blocchi di malloc
        if (fe->k == TK::Class && to->elem->k == TK::Class && fe->cls && to->elem->cls) {
            if (fe->cls->derivesFrom(to->elem->cls)) return;
            if (explicitCast && to->elem->cls->derivesFrom(fe->cls)) return;      // static_cast verso la derivata
        }
        if ((fe->k == TK::C8 || fe->k == TK::I8 || fe->k == TK::U8) && (to->elem->k == TK::C8 || to->elem->k == TK::I8 || to->elem->k == TK::U8)) return;
        error("conversione tra puntatori di tipi diversi (" + typeName(from) + " -> " + typeName(to) + ") non supportata", at);
    }
    case TK::Str:
        if (to->cppValue) {
            if ((from->k == TK::Array || from->k == TK::Ptr) && from->elem && from->elem->k == TK::C8) {
                emit(Op::CallN, nativeIndex("string", "string", "#fromcstr", "string(cstr)"), 1, 1);
                return;
            }
            break;
        }
        if (from->k == TK::Null) return;
        break;
    case TK::Class:
        if (from->k == TK::Null) { if (to->cls && to->cls->valueType) error("null non e' un valore di " + typeName(to), at); return; }
        if (from->k == TK::Class && from->cls && to->cls) {
            if (from->cls->derivesFrom(to->cls)) {
                // C++: oggetto per valore della derivata nella base (slicing)
                if (to->cls->valueType && lang_ == Dialect::Cpp && from->cls != to->cls) emit(Op::Copy, to->cls->id);
                return;
            }
            if (explicitCast && to->cls->derivesFrom(from->cls)) { emit(Op::CastCk, to->cls->id); return; }
            if (explicitCast && (to->cls->isInterface || from->cls->isInterface)) { emit(Op::CastCk, to->cls->id); return; }
        }
        if (to->cls && to->cls->valueType && lang_ == Dialect::Cpp && !to->cls->ctors.empty() && cost == 3) {
            // costruttore di conversione: temporaneo
            LocalVar& tmp = addLocal("", from, at);
            emit(Op::StoreL, tmp.slot);
            emit(Op::New, to->cls->id);
            emit(Op::Dup);
            Node* fake = pool_.node(K::Name, at);
            fake->sym = 0;
            convTemp_ = tmp.slot;
            convTempType_ = from;
            callOverloads(to->cls->ctors, {}, {}, {fake}, {}, at, true, to, true, to->cls->name);
            convTemp_ = -1;
            return;
        }
        break;
    case TK::Nullable:
        if (from->k == TK::Null) return;
        if (from == to->elem) return;
        if (isNumeric(from) && isNumeric(to->elem)) { convert(from, to->elem, at, explicitCast, src); return; }
        break;
    case TK::Func:
        if (from->k == TK::Null) return;
        if (from->k == TK::Func) return;
        if (from->k == TK::Ptr && from->elem && from->elem->k == TK::Func) return;
        break;
    case TK::Native:
        if (from->k == TK::Null && !isValueType(to)) return;
        if (cost >= 0) return;
        break;
    case TK::Array:
        if (from->k == TK::Null && (cs || java)) return;
        if (cost >= 0) return;
        break;
    case TK::Enum:
        break;
    default: break;
    }
    if (from->k == TK::Nullable && from->elem == to) {
        if (!explicitCast && cs) error("conversione implicita da " + typeName(from) + " a " + typeName(to) + ": usa .Value o un cast", at);
        emit(Op::ConvT, to->index);
        return;
    }
    if (from->k == TK::Nullable && isNumeric(to) && explicitCast) { emit(Op::ConvT, from->elem->index); convert(from->elem, to, at, true); return; }
    error("conversione da " + typeName(from) + " a " + typeName(to) + " non possibile", at);
}

TypeP Compiler::arithmetic(TypeP a, TypeP b, Pos at) {
    a = stripRef(a);
    b = stripRef(b);
    if (!isNumeric(a) || !isNumeric(b)) error("operazione aritmetica su " + typeName(!isNumeric(a) ? a : b), at);
    TK ka = a->k == TK::Enum ? (lang_ == Dialect::Cpp || lang_ == Dialect::C ? TK::I32 : vkOf(a) == VK::I32 ? TK::I32 : TK::I64) : a->k;
    TK kb = b->k == TK::Enum ? (lang_ == Dialect::Cpp || lang_ == Dialect::C ? TK::I32 : vkOf(b) == VK::I32 ? TK::I32 : TK::I64) : b->k;
    if ((lang_ == Dialect::CSharp || lang_ == Dialect::Java) && (a->k == TK::Enum || b->k == TK::Enum)) error("aritmetica su un enum: usa un cast a int", at);
    if (ka == TK::F64 || kb == TK::F64) return tF64_;
    if (ka == TK::F32 || kb == TK::F32) return tF32_;
    auto promote = [&](TK k) -> TK { return rankOf(k) < 3 ? TK::I32 : k; };
    if (lang_ == Dialect::CSharp) {
        if ((ka == TK::Bool) || (kb == TK::Bool)) error("aritmetica su bool", at);
        if (ka == TK::U64 || kb == TK::U64) {
            const TK o = ka == TK::U64 ? kb : ka;
            if (o == TK::I8 || o == TK::I16 || o == TK::I32 || o == TK::I64) error("operazione ambigua tra ulong e un tipo con segno", at);
            return tU64_;
        }
        if (ka == TK::I64 || kb == TK::I64) return tI64_;
        if (ka == TK::U32 || kb == TK::U32) {
            const TK o = ka == TK::U32 ? kb : ka;
            if (o == TK::I8 || o == TK::I16 || o == TK::I32) return tI64_;
            return tU32_;
        }
        return tI32_;
    }
    if (lang_ == Dialect::Java) {
        if (ka == TK::Bool || kb == TK::Bool) error("aritmetica su boolean", at);
        if (ka == TK::I64 || kb == TK::I64) return tI64_;
        return tI32_;
    }
    ka = promote(ka);
    kb = promote(kb);
    if (ka == kb) return prim(ka);
    const bool ua = unsignedK(ka), ub = unsignedK(kb);
    if (ua == ub) return rankOf(ka) >= rankOf(kb) ? prim(ka) : prim(kb);
    const TK u = ua ? ka : kb, s = ua ? kb : ka;
    if (rankOf(u) >= rankOf(s)) return prim(u);
    return prim(s);                                       // il tipo con segno piu' largo contiene il senza segno
}

TypeP Compiler::condition(Node* n) {
    EV e = expr(n);
    load(e, n->pos);
    TypeP t = stripRef(e.type);
    if ((lang_ == Dialect::CSharp || lang_ == Dialect::Java) && t->k != TK::Bool && t->k != TK::Dynamic)
        error("la condizione deve essere bool (in " + std::string(lang_ == Dialect::Java ? "Java" : "C#") + " niente numeri come condizione)", n->pos);
    if (t->k == TK::Class && t->cls && lang_ == Dialect::Cpp) error("un oggetto non e' una condizione", n->pos);
    if (t->k == TK::Str && t->cppValue) error("una std::string non e' una condizione", n->pos);
    if (t->k == TK::Native && (t->nk == NK::IStream || t->nk == NK::OStream || t->nk == NK::IFStream || t->nk == NK::OFStream ||
                               t->nk == NK::FStream || t->nk == NK::StringStream)) {
        // if (f) / while (cin >> x) / while (getline(f, s)): lo stato del flusso
        callBuiltinOnStack("ios", "#bool", "bool(self)", 1, t);
        return tBool_;
    }
    if (t->k == TK::Native && lang_ == Dialect::Cpp) error(typeName(t) + " non e' una condizione", n->pos);
    return t;
}

TypeP Compiler::rvalue(Node* n, TypeP want, bool explicitCast) {
    if (n->k == K::Lambda) {
        EV e = lambda(n, want);
        return e.type;
    }
    if (n->k == K::InitList && want) { initFromList(want, n, n->pos); return want; }
    if (n->k == K::Name && n->sym == 0 && convTemp_ >= 0) { emit(Op::LoadL, convTemp_); if (want) convert(convTempType_, want, n->pos, explicitCast); return want ? want : convTempType_; }
    EV e = expr(n);
    load(e, n->pos);
    if (want) {
        convert(e.type, want, n->pos, explicitCast, &e);
        copyIfLvalue(e, want, n->pos);
        return want;
    }
    return e.type;
}
void Compiler::discard(Node* n) {
    // assegnazioni e incrementi senza il valore
    if (n->k == K::Assign) { EV e = assignImpl(n, false); (void)e; return; }
    if (n->k == K::PostInc || n->k == K::PreInc || n->k == K::PostDec || n->k == K::PreDec) {
        EV e = incdec(n, true, n->k == K::PostDec || n->k == K::PreDec);
        if (e.cat == EV::RValue && e.type && e.type->k != TK::Void) emit(Op::Pop);
        return;
    }
    EV e = expr(n);
    if (e.cat == EV::Void || (e.type && e.type->k == TK::Void && e.cat == EV::RValue)) return;
    if (e.cat == EV::TypeName || e.cat == EV::NsName) error("istruzione senza effetto", n->pos);
    if (e.cat == EV::FuncGroup) error("funzione nominata ma non chiamata: mancano le ()", n->pos);
    if (e.cat != EV::RValue) {
        // valore non usato di un luogo: si scartano i componenti
        const std::int32_t c = componentsOf(e);
        for (std::int32_t i = 0; i < c; ++i) emit(Op::Pop);
        if (e.cat == EV::Prop || e.cat == EV::Indexer) {}
        return;
    }
    emit(Op::Pop);
}

// Colore::Rosso (enum class), Classe::COSTANTE: il valore costante calcolato dal
// compilatore delle espressioni (solo dentro una funzione: case, array locali).
bool Compiler::constFromExpr(Node* n, std::int64_t& out) {
    if (!fn_) return false;
    try {
        EV x = exprType(n);
        const TypeP xt = x.type ? stripRef(x.type) : nullptr;
        if (x.hasConst && xt && (isIntegral(xt) || xt->k == TK::Enum || (xt->k == TK::Class && xt->cls && xt->cls->isEnum))) {
            out = x.constValue.i;
            return true;
        }
    } catch (const Error&) {
    }
    return false;
}
bool Compiler::constInt(Node* n, std::int64_t& out) {
    if (!n) return false;
    switch (n->k) {
    case K::IntLit: case K::CharLit: out = n->ival; return true;
    case K::BoolLit: out = n->ival; return true;
    case K::Unary: {
        std::int64_t v = 0;
        if (!constInt(n->a, v)) return false;
        switch (n->op) {
        case Minus: out = static_cast<std::int64_t>(0u - static_cast<std::uint64_t>(v)); return true;
        case Plus: out = v; return true;
        case Tilde: out = ~v; return true;
        case Bang: out = !v; return true;
        default: return false;
        }
    }
    case K::Binary: {
        std::int64_t a = 0, b = 0;
        if (!constInt(n->a, a) || !constInt(n->b, b)) return false;
        switch (n->op) {
        case Plus: out = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b)); return true;
        case Minus: out = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b)); return true;
        case Star: out = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b)); return true;
        case Slash: if (b == 0) return false; out = (a == INT64_MIN && b == -1) ? a : a / b; return true;
        case Percent: if (b == 0) return false; out = b == -1 ? 0 : a % b; return true;
        case Shl: out = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) << (b & 63)); return true;
        case Shr: out = a >> (b & 63); return true;
        case Amp: out = a & b; return true;
        case Pipe: out = a | b; return true;
        case Caret: out = a ^ b; return true;
        case EqEq: out = a == b; return true;
        case Ne: out = a != b; return true;
        case Lt: out = a < b; return true;
        case Gt: out = a > b; return true;
        case Le: out = a <= b; return true;
        case Ge: out = a >= b; return true;
        case AndAnd: out = a && b; return true;
        case OrOr: out = a || b; return true;
        default: return false;
        }
    }
    case K::Cond: {
        std::int64_t c = 0, a = 0, b = 0;
        if (!constInt(n->a, c) || !constInt(n->b, a) || !constInt(n->c, b)) return false;
        out = c ? a : b;
        return true;
    }
    case K::Cast: {
        std::int64_t v = 0;
        if (!constInt(n->a, v)) return false;
        out = v;
        return true;
    }
    case K::SizeofType: {
        if (!fn_ && !ns_) return false;
        if (n->op == 1) return false;                       // typeof
        try { out = sizeOf(resolveType(n->type, false), n->pos); return true; } catch (const Error&) { return false; }
    }
    case K::Name: case K::Scope: case K::Member: {
        // costanti: enum, const int, static const
        std::string nm;
        if (n->k == K::Name) nm = syms_[n->sym];
        else if (n->k == K::Scope && n->a && n->a->k == K::Name) nm = syms_[n->a->sym] + "::" + syms_[n->sym];
        else if (n->k == K::Member && n->a && n->a->k == K::Name) nm = syms_[n->a->sym] + "::" + syms_[n->sym];
        else if (n->k == K::Scope && !n->a) nm = syms_[n->sym];
        else return constFromExpr(n, out);                 // Colore::Rosso (Colore tipo), A::B::c
        if (fn_) {
            if (LocalVar* v = findLocal(nm)) { if (v->hasConstValue) { out = v->constValue.i; return true; } return false; }
        }
        for (auto it = tbind_.rbegin(); it != tbind_.rend(); ++it)
            if (it->first == nm && it->second && it->second->k == TK::Dynamic && it->second->size >= 0x10000) { out = it->second->size - 0x10000; return true; }
        NsEntry* e = lookupGlobalName(nm, ns_, cls_);
        if (e && e->kind == NsEntry::EnumConst) { out = e->enumValue; return true; }
        if (e && e->kind == NsEntry::Global && e->global->hasConstValue) { out = e->global->constValue.i; return true; }
        EV c;
        if (n->k == K::Name && nativeConstant(nm, c, n->pos) && c.hasConst && isIntegral(c.type)) { out = c.constValue.i; return true; }
        return constFromExpr(n, out);
    }
    case K::TypeExpr: return constFromExpr(n, out);            // Colore::Rosso con Colore tipo
    default: return false;
    }
}

// ================================================================ espressioni
EV Compiler::exprType(Node* n) {
    const auto it = probeCache_.find(n);
    if (it != probeCache_.end()) return it->second;
    if (!fn_) {
        EV e;
        e.type = tDyn_;
        return e;
    }
    // compilazione di prova: il codice emesso si butta
    const std::size_t codeSize = fn_->code.size();
    const std::size_t handlers = fn_->handlers.size();
    const std::size_t locals = fn_->locals.size();
    const std::int32_t nlocals = fn_->nlocals;
    auto saveScopes = scopes_;
    auto saveFree = freeSlots_;
    const bool saveProbing = probing_;
    const bool saveStmt = stmtStart_;
    probing_ = true;
    EV r;
    try {
        r = expr(n);
    } catch (...) {
        probing_ = saveProbing;
        fn_->code.resize(codeSize); fn_->pos.resize(codeSize); fn_->stmt.resize(codeSize);
        fn_->handlers.resize(handlers); fn_->locals.resize(locals); fn_->nlocals = nlocals;
        scopes_ = saveScopes; freeSlots_ = saveFree; stmtStart_ = saveStmt;
        throw;
    }
    probing_ = saveProbing;
    fn_->code.resize(codeSize); fn_->pos.resize(codeSize); fn_->stmt.resize(codeSize);
    fn_->handlers.resize(handlers); fn_->locals.resize(locals); fn_->nlocals = nlocals;
    scopes_ = saveScopes; freeSlots_ = saveFree; stmtStart_ = saveStmt;
    r.local = nullptr;
    if (n->k != K::Lambda) probeCache_[n] = r;
    return r;
}

EV Compiler::expr(Node* n) {
    if (++depth_ > ParseDepth) error("Stack ERROR: espressione troppo annidata", n->pos);
    struct D { int& d; ~D() { --d; } } guard{depth_};
    const Pos save = pos_;
    setPos(n->pos);
    EV e;
    switch (n->k) {
    case K::IntLit: {
        const bool u = (n->op & 1) != 0, l = (n->op & 2) != 0, ll = (n->op & 4) != 0;
        const std::uint64_t v = static_cast<std::uint64_t>(n->ival);
        TypeP t;
        if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) {
            if (lang_ == Dialect::Java) t = l ? tI64_ : (v <= 2147483648ULL ? tI32_ : nullptr);
            else if (u && l) t = tU64_;
            else if (u) t = v <= 0xFFFFFFFFULL ? tU32_ : tU64_;
            else if (l) t = v <= 0x7FFFFFFFFFFFFFFFULL ? tI64_ : tU64_;
            else t = v <= 0x7FFFFFFFULL ? tI32_ : v <= 0xFFFFFFFFULL ? tU32_ : v <= 0x7FFFFFFFFFFFFFFFULL ? tI64_ : tU64_;
            if (!t) error("intero troppo grande per int (usa il suffisso L)", n->pos);
        } else {
            if (u) t = (l || ll || v > 0xFFFFFFFFULL) ? tU64_ : tU32_;
            else if (l || ll) t = v <= 0x7FFFFFFFFFFFFFFFULL ? tI64_ : tU64_;
            else t = v <= 0x7FFFFFFFULL ? tI32_ : v <= 0x7FFFFFFFFFFFFFFFULL ? tI64_ : tU64_;
        }
        if (!probing_) pushInt(static_cast<std::int64_t>(v), vkOf(t)); else pushInt(0, vkOf(t));
        e.type = t;
        e.hasConst = true;
        e.constValue = Value::ofInt(vkOf(t), static_cast<std::int64_t>(v));
        break;
    }
    case K::FloatLit: {
        const bool f = (n->op & 1) != 0;
        pushDouble(f ? static_cast<double>(static_cast<float>(n->dval)) : n->dval, f ? VK::F32 : VK::F64);
        e.type = f ? tF32_ : tF64_;
        break;
    }
    case K::CharLit: {
        TypeP t = lang_ == Dialect::C ? tI32_ : charType(lang_);
        pushInt(lang_ == Dialect::Cpp || lang_ == Dialect::C ? static_cast<std::int64_t>(static_cast<signed char>(n->ival)) : n->ival, vkOf(t));
        e.type = t;
        e.hasConst = true;
        e.constValue = Value::ofInt(vkOf(t), n->ival);
        break;
    }
    case K::StrLit: {
        const std::string& s = units_[unit_].strings[n->str];
        if (lang_ == Dialect::C || lang_ == Dialect::Cpp) {
            pushString(s, Dialect::C, false);
            e.type = arrayOf(tChar8_, static_cast<std::int32_t>(s.size() + 1), 1);
            e.isStringLit = true;
        } else {
            pushString(s, lang_, false);
            e.type = tCsStr_;
        }
        break;
    }
    case K::BoolLit:
        emit(Op::PushI, static_cast<std::int32_t>(n->ival), static_cast<std::uint8_t>(lang_ == Dialect::C ? VK::I32 : VK::Bool));
        e.type = lang_ == Dialect::C ? tI32_ : tBool_;
        e.hasConst = true;
        e.constValue = Value::ofInt(lang_ == Dialect::C ? VK::I32 : VK::Bool, n->ival);
        break;
    case K::NullLit:
        emit(Op::PushNull);
        e.type = tNull_;
        e.isNull = true;
        e.hasConst = true;
        e.constValue = Value::ofInt(VK::I64, 0);
        break;
    case K::Name: e = name(n); break;
    case K::Scope: e = scope(n); break;
    case K::Member: e = member(n, false); break;
    case K::Arrow: e = member(n, true); break;
    case K::NullCond: e = nullCond(n); break;
    case K::Index: e = index(n); break;
    case K::Call: e = call(n); break;
    case K::Unary: e = unary(n); break;
    case K::PreInc: e = incdec(n, true, false); break;
    case K::PreDec: e = incdec(n, true, true); break;
    case K::PostInc: e = incdec(n, false, false); break;
    case K::PostDec: e = incdec(n, false, true); break;
    case K::Binary: e = binary(n); break;
    case K::Assign: e = assignImpl(n, true); break;
    case K::Cond: e = conditional(n); break;
    case K::Cast: e = cast(n); break;
    case K::SizeofType: {
        TypeP t = resolveType(n->type, false);
        if (n->op == 1) {
            // typeof(T): lo stesso System.Type di GetType()
            const auto [nm, full] = csTypeNames(t);
            pushString(nm, lang_, false);
            pushString(full, lang_, false);
            emit(Op::CallN, nativeIndex("", "Type", "#make", "class:Type(string,string)"), 2, 1);
            e.type = exceptionClass(Dialect::CSharp, "Type") ? classType(exceptionClass(Dialect::CSharp, "Type")) : tObject_;
            break;
        }
        if (lang_ == Dialect::CSharp) {
            // sizeof di C#: solo i tipi primitivi (e gli enum), risultato int
            std::int64_t sz = 0;
            switch (t->k) {
            case TK::Bool: case TK::I8: case TK::U8: sz = 1; break;
            case TK::C16: case TK::I16: case TK::U16: sz = 2; break;
            case TK::I32: case TK::U32: case TK::F32: case TK::Enum: sz = 4; break;
            case TK::I64: case TK::U64: case TK::F64: sz = 8; break;
            default: error("sizeof(" + typeName(t) + ") non e' una costante in C# (solo i tipi primitivi)", n->pos);
            }
            pushInt(sz, VK::I32);
            e.type = tI32_;
            e.hasConst = true;
            e.constValue = Value::i32(static_cast<std::int32_t>(sz));
            break;
        }
        if (t->k == TK::Class && t->cls) completeClass(t->cls);
        pushInt(sizeOf(t, n->pos), VK::U64);
        e.type = tU64_;
        e.hasConst = true;
        e.constValue = Value::ofInt(VK::U64, sizeOf(t, n->pos));
        break;
    }
    case K::SizeofExpr: {
        EV t = exprType(n->a);
        TypeP tt = stripRef(t.type);
        if (tt->k == TK::Class && tt->cls) completeClass(tt->cls);
        const std::int64_t sz = sizeOf(tt, n->pos);
        pushInt(sz, VK::U64);
        e.type = tU64_;
        e.hasConst = true;
        e.constValue = Value::ofInt(VK::U64, sz);
        break;
    }
    case K::New: e = newExpr(n); break;
    case K::NewArray: e = newExpr(n); break;
    case K::Delete: {
        EV p = expr(n->a);
        load(p, n->pos);
        TypeP t = stripRef(p.type);
        if (t->k != TK::Ptr) error("delete vuole un puntatore", n->a->pos);
        ClassInfo* c = t->elem && t->elem->k == TK::Class ? t->elem->cls : nullptr;
        emit(Op::Delete, c ? c->id : -1, static_cast<std::uint8_t>(n->op == 1 ? 1 : 0));
        e.cat = EV::Void;
        e.type = tVoid_;
        break;
    }
    case K::This: {
        if (!fn_ || !fn_->hasThis) {
            if (!lambdas_.empty() && lambdas_.back()->outerFn && lambdas_.back()->outerFn->hasThis) { e = captureThis(n->pos); break; }
            error("this fuori da un metodo", n->pos);
        }
        emit(Op::LoadL, 0);
        e.type = lang_ == Dialect::Cpp ? ptrTo(classType(cls_ ? cls_ : fn_->owner)) : classType(cls_ ? cls_ : fn_->owner);
        break;
    }
    case K::Base: {
        if (!fn_ || !fn_->hasThis || !fn_->owner || !fn_->owner->base) error("base fuori da un metodo di una classe derivata", n->pos);
        emit(Op::LoadL, 0);
        e.type = classType(fn_->owner->base);
        e.superCall = true;
        break;
    }
    case K::Lambda: e = lambda(n, nullptr); break;
    case K::InitList: error("lista { } senza un tipo di destinazione", n->pos);
    case K::Is: e = isExpr(n); break;
    case K::As: {
        EV v = expr(n->a);
        load(v, n->pos);
        TypeP t = resolveType(n->type, false);
        if (t->k == TK::Str || t->k == TK::Nullable || t->k == TK::Array || t->k == TK::Native) {
            // o as string, o as int?, o as int[], o as List<int>: il valore se il tipo e' giusto, altrimenti null
            emit(Op::Dup);
            pushInt(t->index, VK::I64);
            emit(Op::CallN, nativeIndex("", "object", "#is", "bool(any,long)"), 2, 1);
            const std::int32_t ok = emit(Op::Jnz, 0);
            emit(Op::Pop);
            emit(Op::PushNull);
            patch(ok, here());
            e.type = t;
            break;
        }
        if (t->k != TK::Class || !t->cls || t->cls->valueType) error("as vuole un tipo riferimento o nullable", n->pos);
        emit(Op::DynCast, t->cls->id);
        e.type = t;
        break;
    }
    case K::Interp: e = interpolation(n); break;
    case K::Comma: {
        discard(n->a);
        e = expr(n->b);
        break;
    }
    case K::AddrOf: e = addressOf(n->a); break;
    case K::Deref: {
        EV p = expr(n->a);
        load(p, n->pos);
        TypeP t = stripRef(p.type);
        if (t->k == TK::Array) { convert(t, ptrTo(t->elem), n->pos); t = ptrTo(t->elem); }
        if (t->k == TK::Native && (t->nk == NK::Iter || t->nk == NK::MapIter)) {
            // *it: l'elemento (riferimento) dell'iteratore
            p.type = t;
            e = methodOn(p, "*", nullptr, nullptr, n->pos);
            break;
        }
        if (t->k == TK::Class && t->cls && lang_ == Dialect::Cpp) {
            bool found = false;
            std::vector<Function*> ms = findMethods(t->cls, "operator*", &found);
            std::vector<Function*> unary;
            for (Function* m : ms) if (m->params.empty() && !m->isStatic) unary.push_back(m);
            if (!unary.empty()) { e = callOverloads(unary, {}, {}, {}, {}, n->pos, true, t, false, "operator*"); break; }
        }
        if (t->k != TK::Ptr) error("* su un valore che non e' un puntatore (" + typeName(t) + ")", n->pos);
        if (t->elem && t->elem->k == TK::Void) error("dereferenziazione di un void*: serve un cast", n->pos);
        if (t->elem && t->elem->k == TK::Func) { e.type = t->elem; break; }   // (*f)(x)
        e.cat = EV::Ptr;
        e.type = t->elem;
        e.lvalue = true;
        break;
    }
    case K::Default: {
        TypeP t = n->type ? resolveType(n->type, false) : nullptr;
        if (!t) { emit(Op::PushNull); e.type = tNull_; break; }
        zeroValue(t, n->pos);
        e.type = t;
        break;
    }
    case K::TypeExpr: {
        e.cat = EV::TypeName;
        e.type = resolveType(n->type, false);
        if (e.type->k == TK::Class || e.type->k == TK::Enum) e.typeCls = e.type->cls;
        break;
    }
    case K::ThrowExpr: {
        if (!n->a) { if (catchSlots_.empty()) error("throw; fuori da un catch", n->pos); emit(Op::Rethrow, catchSlots_.back()); }
        else { rvalue(n->a, nullptr); emit(Op::Throw); }
        emit(Op::PushVoid);
        e.type = tDyn_;
        break;
    }
    case K::ObjInit: e = objInit(n); break;
    case K::Declarator: error("dichiarazione non valida qui", n->pos);
    default: error("espressione non supportata", n->pos);
    }
    pos_ = save;
    return e;
}

// ---------------------------------------------------------------- nomi
EV Compiler::name(Node* n) {
    const std::string nm = syms_[n->sym];
    EV e;
    // variabili locali (e catture delle lambda)
    if (fn_) {
        std::size_t depth = 0;
        if (LocalVar* v = findLocal(nm, &depth)) {
            if (v->global >= 0) { e.cat = EV::Global; e.slot = v->global; e.type = v->type; e.lvalue = true; return e; }
            if (v->hasConstValue && !probing_) {}
            e.local = v;
            e.type = v->type;
            e.lvalue = true;
            e.isConst = v->isConst && !v->isRef;
            if (v->hasConstValue) { e.hasConst = true; e.constValue = v->constValue; }
            if (v->isRef) { emit(Op::LoadL, v->slot); e.cat = EV::Ptr; e.isConst = v->isConst; return e; }
            if (v->boxed) { e.cat = EV::LocalBox; e.slot = v->slot; return e; }
            e.cat = EV::Local;
            e.slot = v->slot;
            return e;
        }
        if (!lambdas_.empty()) {
            EV c;
            if (captureName(nm, n->pos, c)) return c;
        }
    }
    // membri della classe (campi, proprieta', metodi) con this implicito
    if (cls_) {
        ClassInfo* owner = nullptr;
        const FieldInfo* f = findField(cls_, nm, &owner);
        if (f) {
            if (f->isStatic) { e.cat = EV::Global; e.slot = f->slot; e.type = f->type; e.lvalue = true; e.isConst = f->isConst; checkAccess(f->access, owner, nm, n->pos);
                if (GlobalVar* g = globalFor(f->slot)) if (g->hasConstValue) { e.hasConst = true; e.constValue = g->constValue; }
                return e; }
            if (!fn_ || !fn_->hasThis) {
                if (!lambdas_.empty() && lambdas_.back()->outerFn && lambdas_.back()->outerFn->hasThis) {
                    EV t = captureThis(n->pos);
                    load(t, n->pos);
                } else error("il campo '" + nm + "' vuole un oggetto (siamo in una funzione static)", n->pos);
            } else emit(Op::LoadL, 0);
            checkAccess(f->access, owner, nm, n->pos);
            e.cat = EV::Field;
            e.slot = f->slot;
            e.type = f->type;
            e.lvalue = true;
            e.isConst = (f->isConst || (f->isReadonly && !(fn_ && fn_->isCtor))) && lang_ != Dialect::Cpp;
            if (lang_ == Dialect::Cpp && f->isConst && !(fn_ && fn_->isCtor)) e.isConst = true;
            if (fn_ && fn_->isConst && lang_ == Dialect::Cpp) e.isConst = true;
            return e;
        }
        if (PropertyInfo* p = findProperty(cls_, nm)) {
            if (!p->isStatic) {
                if (!fn_ || !fn_->hasThis) {
                    if (!lambdas_.empty() && lambdas_.back()->outerFn && lambdas_.back()->outerFn->hasThis) { EV t = captureThis(n->pos); load(t, n->pos); }
                    else error("la proprieta' '" + nm + "' vuole un oggetto", n->pos);
                } else emit(Op::LoadL, 0);
            }
            e.cat = EV::Prop;
            e.prop = p;
            e.getter = p->getter;
            e.setter = p->setter;
            e.type = p->type;
            e.lvalue = true;
            return e;
        }
        bool found = false;
        std::vector<Function*> ms = findMethods(cls_, nm, &found);
        if (found) {
            e.cat = EV::FuncGroup;
            e.fns = ms;
            e.templates = methodTemplates(cls_, nm);
            e.hasReceiver = false;
            for (Function* m : ms) if (!m->isStatic) e.hasReceiver = true;
            e.type = tDyn_;
            // il ricevente (this) si carica alla chiamata
            implicitThis_ = true;
            return e;
        }
    }
    // namespace, classi, globali, funzioni
    NsEntry* ent = lookupGlobalName(nm, ns_, cls_);
    if (ent) return entryValue(ent, nm, n);
    // parametri interi dei template (template<int N>)
    for (auto it = tbind_.rbegin(); it != tbind_.rend(); ++it) {
        if (it->first == nm && it->second && it->second->k == TK::Dynamic && it->second->size >= 0x10000) {
            pushInt(it->second->size - 0x10000, VK::I32);
            e.type = tI32_;
            e.hasConst = true;
            e.constValue = Value::i32(it->second->size - 0x10000);
            return e;
        }
        if (it->first == nm) { e.cat = EV::TypeName; e.type = it->second; return e; }
    }
    // libreria: costanti e funzioni
    if (nativeConstant(nm, e, n->pos)) return e;
    std::vector<std::int32_t> nat = nativeFunctions(nm, false);
    if (!nat.empty()) { e.cat = EV::FuncGroup; e.natives = nat; e.type = tDyn_; return e; }
    if (lang_ == Dialect::CSharp && (nm == "Console" || nm == "Math" || nm == "Convert" || nm == "File" || nm == "Directory" || nm == "Path" ||
                                     nm == "Array" || nm == "Fx" || nm == "String" || nm == "Char" || nm == "Int32" || nm == "Double" || nm == "Environment" || nm == "Enum")) {
        e.cat = EV::TypeName;
        e.type = tDyn_;
        staticClass_ = nm;
        return e;
    }
    // nomi della libreria senza l'intestazione: suggerimento
    std::string hint = headerHint(nm);
    if (!hint.empty()) error("'" + nm + "' non dichiarata: manca " + hint, n->pos);
    error("'" + nm + "' non dichiarata", n->pos);
}
GlobalVar* Compiler::globalFor(std::int32_t slot) {
    for (auto& g : globals_) if (g->slot == slot) return g.get();
    return nullptr;
}
EV Compiler::entryValue(NsEntry* ent, const std::string& nm, Node* n) {
    EV e;
    switch (ent->kind) {
    case NsEntry::Global:
        e.cat = EV::Global;
        e.slot = ent->global->slot;
        e.type = ent->global->type;
        e.lvalue = true;
        e.isConst = ent->global->isConst;
        if (ent->global->hasConstValue) { e.hasConst = true; e.constValue = ent->global->constValue; }
        if (e.type->k == TK::Dynamic && e.type->size == -2) error("'" + nm + "' usata prima del suo valore (auto)", n->pos);
        return e;
    case NsEntry::EnumConst:
        pushInt(ent->enumValue, vkOf(ent->type));
        e.type = ent->type;
        e.hasConst = true;
        e.constValue = Value::ofInt(vkOf(ent->type), ent->enumValue);
        return e;
    case NsEntry::Functions:
        e.cat = EV::FuncGroup;
        e.fns = ent->fns;
        e.templates = ent->fnTemplates;
        e.type = tDyn_;
        for (Function* f : ent->fns) if (!f->defined) { calledUndefined_.push_back(f); }
        return e;
    case NsEntry::Class: case NsEntry::Typedef: case NsEntry::Template:
        e.cat = EV::TypeName;
        if (ent->kind == NsEntry::Class) { completeClass(ent->cls); e.type = classType(ent->cls); e.typeCls = ent->cls; }
        else if (ent->kind == NsEntry::Typedef) { e.type = ent->type; if (e.type->k == TK::Class) e.typeCls = e.type->cls; }
        else { e.type = tDyn_; templateName_ = ent->tmpl; }
        return e;
    case NsEntry::Namespace:
        e.cat = EV::NsName;
        e.ns = ent->ns;
        return e;
    default: break;
    }
    error("'" + nm + "' non utilizzabile qui", n->pos);
}

EV Compiler::scope(Node* n) {
    // ::x, A::b, std::cout, Base::metodo, Enum::Valore
    const std::string member = syms_[n->sym];
    if (!n->a) {
        NsEntry* e = lookupScoped(root_.get(), member);
        if (!e) error("'::" + member + "' non dichiarata", n->pos);
        return entryValue(e, member, n);
    }
    std::string path = scopePath(n->a);
    // fx:: (libreria della calcolatrice, sempre disponibile in C++)
    if (path == "fx" && lang_ == Dialect::Cpp && !lookupScoped(root_.get(), "fx::" + member)) {
        std::vector<std::int32_t> nat = nativeFunctions("fx::" + member, false);
        if (!nat.empty()) {
            EV g;
            g.cat = EV::FuncGroup;
            g.natives = nat;
            g.type = tDyn_;
            return g;
        }
        std::size_t count = 0;
        const NativeTypeEntry* nt = nativeTypes(count);
        (void)nt;
        error("'fx::" + member + "' non esiste nella libreria fx", n->pos);
    }
    // ios::app, std::ios::out...: modi di apertura dei file
    if (lang_ == Dialect::Cpp && (path == "ios" || path == "std::ios" || path == "ios_base" || path == "std::ios_base" || path == "fstream" ||
                                  path == "std::fstream" || path == "ofstream" || path == "std::ofstream" || path == "ifstream" || path == "std::ifstream")) {
        static const std::pair<const char*, int> kModes[] = {{"app", 1}, {"ate", 2}, {"binary", 4}, {"in", 8}, {"out", 16}, {"trunc", 32}};
        for (const auto& m : kModes) if (member == m.first) {
            EV e;
            pushInt(m.second, VK::I32);
            e.type = tI32_;
            e.hasConst = true;
            e.constValue = Value::i32(m.second);
            return e;
        }
    }
    // std:: della libreria
    if (path == "std") {
        EV e;
        if (nativeConstant("std::" + member, e, n->pos)) return e;
        std::vector<std::int32_t> nat = nativeFunctions("std::" + member, true);
        if (!nat.empty()) {
            EV g;
            g.cat = EV::FuncGroup;
            g.natives = nat;
            g.type = tDyn_;
            return g;
        }
        NsEntry* ent = lookupScoped(root_.get(), "std::" + member);
        if (ent) return entryValue(ent, member, n);
        // un tipo standard usato come valore (std::string("x") passa da TypeExpr)
        error("'std::" + member + "' non disponibile (intestazione mancante o non supportata)", n->pos);
    }
    EV left = expr(n->a);
    if (left.cat == EV::NsName) {
        NsEntry* e = lookupScoped(left.ns, member);
        if (!e) {
            for (Namespace* u : left.ns->usings) if ((e = lookupScoped(u, member))) break;
        }
        if (!e) error("'" + path + "::" + member + "' non dichiarata", n->pos);
        return entryValue(e, member, n);
    }
    if (left.cat == EV::TypeName) {
        EV dummy;
        return staticMember(left, member, n);
    }
    error("'" + path + "' non e' un namespace ne' una classe", n->a->pos);
}
std::string Compiler::scopePath(const Node* n) const {
    if (!n) return "";
    if (n->k == K::Name) return syms_[n->sym];
    if (n->k == K::Scope) { const std::string l = scopePath(n->a); return l.empty() ? syms_[n->sym] : l + "::" + syms_[n->sym]; }
    if (n->k == K::TypeExpr && n->type) { std::string s; for (Sym q : n->type->qual) s += syms_[q] + "::"; return s + syms_[n->type->name]; }
    return "?";
}
// Nomi di System.Type come .NET (typeof(int).Name = "Int32", FullName = "System.Int32").
std::pair<std::string, std::string> Compiler::csTypeNames(TypeP t) {
    t = stripRef(t);
    std::string name, ns = "System";
    switch (t->k) {
    case TK::Bool: name = "Boolean"; break;
    case TK::C8: case TK::C16: name = "Char"; break;
    case TK::I8: name = "SByte"; break;
    case TK::U8: name = "Byte"; break;
    case TK::I16: name = "Int16"; break;
    case TK::U16: name = "UInt16"; break;
    case TK::I32: name = "Int32"; break;
    case TK::U32: name = "UInt32"; break;
    case TK::I64: name = "Int64"; break;
    case TK::U64: name = "UInt64"; break;
    case TK::F32: name = "Single"; break;
    case TK::F64: name = "Double"; break;
    case TK::Str: name = "String"; break;
    case TK::Void: name = "Void"; break;
    case TK::Array: {
        const auto inner = csTypeNames(t->elem);
        const std::string dims = t->rank == 2 ? "[,]" : "[]";
        return {inner.first + dims, inner.second + dims};
    }
    case TK::Class: case TK::Enum: {
        name = t->cls ? t->cls->name : "Object";
        const std::size_t lt = name.find('<');
        if (lt != std::string::npos) {
            int n = 1, depth = 0;
            for (std::size_t i = lt + 1; i < name.size(); ++i) { if (name[i] == '<') ++depth; else if (name[i] == '>') --depth; else if (name[i] == ',' && depth == 0) ++n; }
            name = name.substr(0, lt) + "`" + std::to_string(n);
        }
        ns.clear();
        if (t->cls) {
            const std::string& q = t->cls->qualified;
            const std::size_t sep = q.rfind("::", q.find('<'));
            if (sep != std::string::npos) ns = q.substr(0, sep);
        }
        break;
    }
    case TK::Native:
        ns = "System.Collections.Generic";
        switch (t->nk) {
        case NK::CsList: name = "List`1"; break;
        case NK::CsDict: name = "Dictionary`2"; break;
        case NK::CsHashSet: name = "HashSet`1"; break;
        case NK::CsQueue: name = "Queue`1"; break;
        case NK::CsStack: name = "Stack`1"; break;
        case NK::CsKvp: name = "KeyValuePair`2"; break;
        case NK::CsStringBuilder: name = "StringBuilder"; ns = "System.Text"; break;
        case NK::CsStreamReader: name = "StreamReader"; ns = "System.IO"; break;
        case NK::CsStreamWriter: name = "StreamWriter"; ns = "System.IO"; break;
        case NK::CsRandom: name = "Random"; ns = "System"; break;
        default: name = "Object"; ns = "System"; break;
        }
        break;
    default: name = "Object"; break;
    }
    for (std::size_t p = ns.find("::"); p != std::string::npos; p = ns.find("::")) ns.replace(p, 2, ".");
    return {name, ns.empty() ? name : ns + "." + name};
}
// Membro statico di una classe (o di un tipo): Classe::x, Classe.Metodo, Enum.Valore.
EV Compiler::staticMember(EV& left, const std::string& member, Node* n) {
    EV e;
    TypeP t = left.type;
    if (left.typeCls) {
        ClassInfo* c = left.typeCls;
        completeClass(c);
        if (c->isEnum) {
            for (const auto& v : c->enumValues) if (v.first == member) {
                pushInt(v.second, vkOf(c->self));
                e.type = c->self;
                e.hasConst = true;
                e.constValue = Value::ofInt(vkOf(c->self), v.second);
                return e;
            }
            error("l'enum " + c->name + " non ha il valore '" + member + "'", n->pos);
        }
        ClassInfo* owner = nullptr;
        if (const FieldInfo* f = findField(c, member, &owner)) {
            checkAccess(f->access, owner, member, n->pos);
            if (f->isStatic) {
                e.cat = EV::Global; e.slot = f->slot; e.type = f->type; e.lvalue = true; e.isConst = f->isConst;
                if (GlobalVar* g = globalFor(f->slot)) if (g->hasConstValue) { e.hasConst = true; e.constValue = g->constValue; }
                return e;
            }
            // C++: Base::campo dentro un metodo (this implicito)
            if (fn_ && fn_->hasThis && cls_ && cls_->derivesFrom(c)) {
                emit(Op::LoadL, 0);
                e.cat = EV::Field; e.slot = f->slot; e.type = f->type; e.lvalue = true;
                return e;
            }
            error("il campo '" + member + "' non e' static", n->pos);
        }
        if (PropertyInfo* p = findProperty(c, member)) {
            if (!p->isStatic) error("la proprieta' '" + member + "' non e' static", n->pos);
            e.cat = EV::Prop; e.prop = p; e.getter = p->getter; e.setter = p->setter; e.type = p->type; e.lvalue = true;
            return e;
        }
        bool found = false;
        std::vector<Function*> ms = findMethods(c, member, &found);
        std::vector<TemplateDef*> mt = methodTemplates(c, member);
        if (found || !mt.empty()) {
            e.cat = EV::FuncGroup;
            e.fns = ms;
            e.templates = mt;
            e.type = tDyn_;
            // Base::metodo() dentro un metodo: chiamata non virtuale con this
            if (fn_ && fn_->hasThis && cls_ && cls_->derivesFrom(c)) { e.superCall = true; for (Function* m : ms) if (!m->isStatic) e.hasReceiver = true; implicitThis_ = e.hasReceiver; }
            return e;
        }
        // tipi annidati
        auto sc = classScope_.find(c);
        if (sc != classScope_.end()) if (NsEntry* ne = lookupScoped(sc->second, member)) return entryValue(ne, member, n);
        // eccezioni native: membri statici non ci sono
        error("la classe " + c->name + " non ha il membro '" + member + "'", n->pos);
    }
    // tipi nativi e primitivi: int.MaxValue, string.Join, Math.Sqrt, std::string::npos
    std::string owner = left.cat == EV::TypeName && !staticClass_.empty() && t == tDyn_ ? staticClass_ : ownerKey(t);
    staticClass_.clear();
    if (nativeConstant(owner + "." + member, e, n->pos)) return e;
    std::vector<std::int32_t> nat = nativeMembers(owner, member, NF::Static, 0);
    if (!nat.empty()) {
        const NativeEntry& ne = nativeEntry(nat[0]);
        if (ne.flags & NF::Property) {
            emit(Op::CallN, nat[0], 0, 1);
            std::vector<TypeP> ps;
            bool variadic = false;
            e.type = parseSig(ne.sig, ps, variadic, t, lang_);
            return e;
        }
        e.cat = EV::FuncGroup;
        e.natives = nat;
        e.type = tDyn_;
        return e;
    }
    error("'" + typeName(t) + "' non ha il membro statico '" + member + "'", n->pos);
}

// Metodi generici (template di funzione membro) di una classe e delle sue basi.
std::vector<TemplateDef*> Compiler::methodTemplates(ClassInfo* c, const std::string& nm) {
    std::vector<TemplateDef*> out;
    for (ClassInfo* k = c; k; k = k->base) {
        auto it = classScope_.find(k);
        if (it == classScope_.end() || !it->second) continue;
        auto m = it->second->members.find(nm);
        if (m == it->second->members.end() || m->second.kind != NsEntry::Functions) continue;
        for (TemplateDef* t : m->second.fnTemplates) if (std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
    }
    return out;
}

// ---------------------------------------------------------------- membri
const FieldInfo* Compiler::findField(ClassInfo* c, const std::string& n, ClassInfo** owner) {
    for (ClassInfo* k = c; k; k = k->base) {
        completeClass(k);
        for (const FieldInfo& f : k->fields) if (f.name == n) { if (owner) *owner = k; return &f; }
    }
    // classi che racchiudono: membri statici
    for (ClassInfo* o = c ? c->outer : nullptr; o; o = o->outer)
        for (const FieldInfo& f : o->fields) if (f.name == n && f.isStatic) { if (owner) *owner = o; return &f; }
    return nullptr;
}
PropertyInfo* Compiler::findProperty(ClassInfo* c, const std::string& n, bool indexer) {
    for (ClassInfo* k = c; k; k = k->base) {
        completeClass(k);
        for (PropertyInfo& p : k->properties) if ((indexer && p.isIndexer) || (!indexer && !p.isIndexer && p.name == n)) return &p;
        for (ClassInfo* i : k->interfaces) if (PropertyInfo* p = findProperty(i, n, indexer)) return p;
    }
    return nullptr;
}
std::vector<Function*> Compiler::findMethods(ClassInfo* c, const std::string& n, bool* found) {
    std::vector<Function*> out;
    for (ClassInfo* k = c; k; k = k->base) {
        completeClass(k);
        for (Function* m : k->methods) if (m->simple == n) {
            // un override nella derivata nasconde la versione della base
            bool hidden = false;
            for (Function* o : out) if (sameParams(o, [&] { std::vector<TypeP> v; for (const Param& p : m->params) v.push_back(p.type); return v; }())) hidden = true;
            if (!hidden) out.push_back(m);
        }
        if (!out.empty() && lang_ == Dialect::Cpp) break;   // C++: il nome nella derivata nasconde la base
        for (ClassInfo* i : k->interfaces) {
            std::vector<ClassInfo*> todo{i};
            for (std::size_t q = 0; q < todo.size(); ++q) {
                for (Function* m : todo[q]->methods) if (m->simple == n) {
                    bool dup = false;
                    for (Function* o : out) if (o->params.size() == m->params.size() && sameParams(o, [&] { std::vector<TypeP> v; for (const Param& p : m->params) v.push_back(p.type); return v; }())) dup = true;
                    if (!dup) out.push_back(m);
                }
                for (ClassInfo* s : todo[q]->interfaces) todo.push_back(s);
            }
        }
    }
    for (ClassInfo* o = c ? c->outer : nullptr; o && out.empty(); o = o->outer)
        for (Function* m : o->methods) if (m->simple == n && m->isStatic) out.push_back(m);
    if (found) *found = !out.empty();
    return out;
}
bool Compiler::canAccess(Access a, ClassInfo* owner) const {
    if (a == Access::Public || a == Access::Internal || !owner) return true;
    // amici (C++)
    if (fn_) {
        auto ff = friendFns_.find(owner);
        if (ff != friendFns_.end() && ff->second.count(fn_)) return true;
    }
    for (ClassInfo* c = cls_; c; c = c->outer) {
        if (c == owner) return true;
        if (a == Access::Protected && c->derivesFrom(owner)) return true;
        auto fr = friendNames_.find(owner);
        if (fr != friendNames_.end() && fr->second.count(c->name)) return true;
    }
    // le lambda nei metodi vedono i membri privati
    if (!lambdas_.empty() && lambdas_.back()->outerCls) {
        for (ClassInfo* c = lambdas_.back()->outerCls; c; c = c->outer) if (c == owner || (a == Access::Protected && c->derivesFrom(owner))) return true;
    }
    return false;
}
void Compiler::checkAccess(Access a, ClassInfo* owner, const std::string& what, Pos at) const {
    if (canAccess(a, owner)) return;
    error("'" + what + "' e' " + std::string(a == Access::Private ? "private" : "protected") + " in " + (owner ? owner->name : std::string("?")), at);
}
EV Compiler::fieldOf(TypeP objType, const FieldInfo* f, Pos at) {
    (void)objType;
    (void)at;
    EV e;
    e.cat = EV::Field;
    e.slot = f->slot;
    e.type = f->type;
    e.lvalue = true;
    e.isConst = f->isConst;
    return e;
}

EV Compiler::member(Node* n, bool arrow) {
    const std::string m = syms_[n->sym];
    EV base;
    if ((lang_ == Dialect::CSharp || lang_ == Dialect::Java) && n->a && n->a->k == K::Name) {
        // StringSplitOptions.RemoveEmptyEntries: costanti di tipi della libreria senza classe nel nucleo
        try {
            base = expr(n->a);
        } catch (const Error&) {
            EV c;
            if (nativeConstant(syms_[n->a->sym] + "." + m, c, n->pos)) return c;
            throw;
        }
    } else base = expr(n->a);
    if (base.cat == EV::NsName) {
        NsEntry* e = lookupScoped(base.ns, m);
        if (!e) for (Namespace* u : base.ns->usings) if ((e = lookupScoped(u, m))) break;
        if (!e) {
            // System.Console, System.Math...: namespace standard di C#
            if (lang_ == Dialect::CSharp) {
                Node fake = *n;
                fake.k = K::Name;
                return name(&fake);
            }
            error("'" + m + "' non trovato nel namespace " + base.ns->name, n->pos);
        }
        return entryValue(e, m, n);
    }
    if (base.cat == EV::TypeName) return staticMember(base, m, n);
    if (base.cat == EV::FuncGroup) error("un metodo non ha membri: mancano le ()", n->pos);
    load(base, n->a->pos);
    TypeP t = stripRef(base.type);
    if (arrow) {
        if (t->k == TK::Array) { convert(t, ptrTo(t->elem), n->pos); t = ptrTo(t->elem); }
        if (t->k == TK::Native && (t->nk == NK::Iter || t->nk == NK::MapIter)) {
            // it->campo: l'elemento dell'iteratore
            EV r = methodOn(base, "*", nullptr, nullptr, n->pos);
            load(r, n->pos);
            base = r;
            t = stripRef(r.type);
        } else {
            if (t->k != TK::Ptr) error("-> su un valore che non e' un puntatore (usa .)", n->pos);
            t = t->elem;
        }
    } else if (t->k == TK::Ptr && (lang_ == Dialect::C || lang_ == Dialect::Cpp) && t->elem && t->elem->k == TK::Class) {
        error("'.' su un puntatore: usa ->", n->pos);
    }
    base.type = t;
    if (n->type2) {
        // d.B::f(): B e' la classe dell'oggetto o una sua base; niente chiamata virtuale
        TypeP q = resolveType(n->type2, false);
        if (q->k != TK::Class || !q->cls || t->k != TK::Class || !t->cls) error("qualificazione " + typeName(q) + "::" + m + " non valida", n->pos);
        bool ok = false;
        for (ClassInfo* k = t->cls; k; k = k->base) if (k == q->cls) ok = true;
        if (!ok) error(q->cls->name + " non e' una base di " + t->cls->name, n->pos);
        base.type = q;
        base.superCall = true;
        return memberOfType(base, q, m, n);
    }
    return memberOfType(base, t, m, n);
}
EV Compiler::memberOfType(EV& base, TypeP t, const std::string& m, Node* n) {
    EV e;
    if (t->k == TK::Class && t->cls && !t->cls->isEnum) {
        ClassInfo* c = t->cls;
        completeClass(c);
        if (!c->complete && !c->isNative) error("tipo " + c->name + " incompleto", n->pos);
        ClassInfo* owner = nullptr;
        if (const FieldInfo* f = findField(c, m, &owner)) {
            checkAccess(f->access, owner, m, n->pos);
            if (f->isStatic) {
                emit(Op::Pop);
                e.cat = EV::Global; e.slot = f->slot; e.type = f->type; e.lvalue = true; e.isConst = f->isConst;
                return e;
            }
            e.cat = EV::Field;
            e.slot = f->slot;
            e.type = f->type;
            e.lvalue = true;
            e.isConst = (f->isConst || (f->isReadonly && !(fn_ && fn_->isCtor && fn_->owner == owner)));
            return e;
        }
        if (PropertyInfo* p = findProperty(c, m)) {
            if (p->isStatic) emit(Op::Pop);
            e.cat = EV::Prop;
            e.prop = p;
            e.getter = p->getter;
            e.setter = p->setter;
            e.type = p->type;
            e.lvalue = true;
            e.superCall = base.superCall;
            return e;
        }
        bool found = false;
        std::vector<Function*> ms = findMethods(c, m, &found);
        std::vector<TemplateDef*> mt = methodTemplates(c, m);
        if (found || !mt.empty()) {
            e.cat = EV::FuncGroup;
            e.fns = ms;
            e.templates = mt;
            e.hasReceiver = true;
            e.superCall = base.superCall;
            e.type = t;                                  // tipo del ricevente
            return e;
        }
        // eccezioni: Message, what(), StackTrace
        if (c->isException) {
            std::vector<std::int32_t> nat = nativeMembers(lang_ == Dialect::Cpp ? "exception" : "Exception", m, 0, NF::Static);
            if (!nat.empty()) return nativeMember(nat, t, m, n);
        }
        if (lang_ == Dialect::CSharp) {
            std::vector<std::int32_t> nat = nativeMembers("object", m, 0, NF::Static);
            if (!nat.empty()) return nativeMember(nat, t, m, n);
        }
        error("la classe " + c->name + " non ha il membro '" + m + "'", n->pos);
    }
    if (t->k == TK::Nullable) {
        if (m == "HasValue") { emit(Op::CallN, nativeIndex("", "nullable", "HasValue", "bool(any)"), 1, 1); e.type = tBool_; return e; }
        if (m == "Value") { emit(Op::ConvT, t->elem->index); e.type = t->elem; return e; }
        if (m == "GetValueOrDefault") { e.cat = EV::FuncGroup; e.natives = nativeMembers("nullable", m, 0, NF::Static); e.hasReceiver = true; e.type = t; return e; }
    }
    // tipi nativi (stringhe, contenitori, array, primitivi di C#)
    const std::string owner = ownerKey(t);
    std::vector<std::int32_t> nat = nativeMembers(owner, m, 0, NF::Static);
    if (nat.empty() && lang_ == Dialect::CSharp && owner != "object") nat = nativeMembers("object", m, 0, NF::Static);
    if (nat.empty()) {
        if (t->k == TK::Ptr) error("membro '" + m + "' di un puntatore: usa -> o (*p).", n->pos);
        error(typeName(t) + " non ha il membro '" + m + "'", n->pos);
    }
    return nativeMember(nat, t, m, n);
}
EV Compiler::nativeMember(const std::vector<std::int32_t>& nat, TypeP t, const std::string& m, Node* n) {
    EV e;
    const NativeEntry& ne = nativeEntry(nat[0]);
    if (ne.flags & NF::Property) {
        // proprieta' native: Length, Count, first/second...
        std::vector<TypeP> ps;
        bool variadic = false;
        std::vector<bool> refs;
        TypeP rt = parseSig(ne.sig, ps, variadic, t, lang_, &refs);
        emit(Op::CallN, nat[0], 1, 1);
        fn_->code.back().c = static_cast<std::uint16_t>(1 | ((t->index & 0x7FFF) << 1));
        if (rt->k == TK::Ref) { e.cat = EV::Ptr; e.type = rt->elem; e.lvalue = true; return e; }
        e.type = rt;
        (void)m;
        (void)n;
        return e;
    }
    e.cat = EV::FuncGroup;
    e.natives = nat;
    e.hasReceiver = true;
    e.type = t;
    return e;
}

EV Compiler::nullCond(Node* n) {
    // a?.b: null se a e' null, altrimenti a.b
    EV base = expr(n->a);
    load(base, n->pos);
    TypeP t = stripRef(base.type);
    emit(Op::Dup);
    const std::int32_t jz = emit(Op::Jz, 0);
    EV r = memberOfType(base, t, syms_[n->sym], n);
    load(r, n->pos);
    TypeP rt = stripRef(r.type);
    if (isNumeric(rt)) rt = nullableOf(rt);
    const std::int32_t jmp = emit(Op::Jmp, 0);
    patch(jz, here());
    emit(Op::Pop);
    emit(Op::PushNull);
    patch(jmp, here());
    EV e;
    e.type = rt;
    return e;
}

// ---------------------------------------------------------------- indici
EV Compiler::index(Node* n) {
    EV base = expr(n->a);
    if (base.cat == EV::TypeName) error("indice su un tipo", n->pos);
    TypeP t = stripRef(base.type);
    // indicizzatori e operator[] dell'utente
    if (t->k == TK::Class && t->cls && !t->cls->isEnum) {
        load(base, n->a->pos);
        ClassInfo* c = t->cls;
        if (PropertyInfo* p = findProperty(c, "", true)) {
            if (n->list.size() + (p->getter ? 0 : 0) != (p->getter ? p->getter->params.size() : p->setter->params.size() - 1)) error("numero di indici sbagliato", n->pos);
            Function* acc = p->getter ? p->getter : p->setter;
            for (std::size_t i = 0; i < n->list.size(); ++i) rvalue(n->list[i], acc->params[i].type);
            EV e;
            e.cat = EV::Indexer;
            e.prop = p;
            e.getter = p->getter;
            e.setter = p->setter;
            e.type = p->type;
            e.components = static_cast<std::int32_t>(1 + n->list.size());
            e.lvalue = true;
            return e;
        }
        bool found = false;
        std::vector<Function*> ms = findMethods(c, "operator[]", &found);
        if (found) return callOverloads(ms, {}, {}, n->list, {}, n->pos, true, t, false, "operator[]");
        error("la classe " + c->name + " non ha l'operatore []", n->pos);
    }
    load(base, n->a->pos);
    if (t->k == TK::Array) {
        if (t->rank == 2) {
            if (n->list.size() != 2) error("un array [,] vuole due indici", n->pos);
            rvalue(n->list[0], tI64_);
            rvalue(n->list[1], tI64_);
            EV e; e.cat = EV::Elem2; e.type = t->elem; e.lvalue = true; return e;
        }
        if (n->list.size() != 1) error("un solo indice per volta: a[i][j]", n->pos);
        EV ix = expr(n->list[0]);
        load(ix, n->pos);
        if (!isIntegral(stripRef(ix.type)) || (stripRef(ix.type)->k == TK::Bool && lang_ != Dialect::C && lang_ != Dialect::Cpp))
            error("l'indice deve essere intero", n->list[0]->pos);
        if ((lang_ == Dialect::CSharp || lang_ == Dialect::Java) && stripRef(ix.type)->k == TK::Enum) error("l'indice deve essere intero", n->list[0]->pos);
        convert(ix.type, tI64_, n->pos, true, &ix);
        EV e;
        e.cat = EV::Elem;
        e.type = t->elem;
        e.lvalue = true;
        e.isConst = base.isStringLit;
        return e;
    }
    if (t->k == TK::Ptr) {
        // p[i] = *(p + i)
        if (n->list.size() != 1) error("un solo indice", n->pos);
        if (t->elem && t->elem->k == TK::Void) error("indice su un void*", n->pos);
        rvalue(n->list[0], tI64_);
        emit(Op::Add);
        EV e; e.cat = EV::Ptr; e.type = t->elem; e.lvalue = true; return e;
    }
    // C#/Java: indicizzatori nativi con get e set separati (Dictionary: get senza la chiave -> KeyNotFoundException)
    if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) {
        std::vector<std::int32_t> g = nativeMembers(ownerKey(t), "#get", 0, 0);
        std::vector<std::int32_t> st = nativeMembers(ownerKey(t), "#set", 0, 0);
        if (!g.empty() && !st.empty()) {
            if (n->list.size() != 1) error("un solo indice", n->pos);
            std::vector<TypeP> ps;
            bool variadic = false;
            TypeP rt = parseSig(nativeEntry(g[0]).sig, ps, variadic, t, lang_);
            rvalue(n->list[0], ps.empty() ? tDyn_ : ps[0]);
            EV e;
            e.cat = EV::Indexer;
            e.natives = {g[0], st[0]};
            e.slot = t->index;
            e.type = rt;
            e.components = 2;
            e.lvalue = true;
            return e;
        }
    }
    // contenitori nativi: vector[i], map[k], string[i], List[i], Dictionary[k]
    std::vector<std::int32_t> nat = nativeMembers(ownerKey(t), "operator[]", 0, 0);
    if (nat.empty()) error("indice [] su un valore di tipo " + typeName(t), n->pos);
    return callOverloads({}, nat, {}, n->list, {}, n->pos, true, t, false, "[]");
}

// ---------------------------------------------------------------- indirizzi
EV Compiler::addressOfEV(EV& e, Pos at) {
    EV r;
    r.lvalue = false;
    TypeP t = stripRef(e.type);
    // un oggetto (classe, contenitore, array) e' gia' il suo indirizzo
    if ((t->k == TK::Class && t->cls && t->cls->valueType) || (t->k == TK::Array && t->size >= 0) || (t->k == TK::Native && isValueType(t)) || (t->k == TK::Str && t->cppValue)) {
        load(e, at);
        emit(Op::ConvT, ptrTo(t)->index);
        r.type = ptrTo(t);
        return r;
    }
    switch (e.cat) {
    case EV::Local: emit(Op::AddrL, e.slot); break;
    case EV::LocalBox: emit(Op::AddrL, e.slot, 1); break;
    case EV::Global: emit(Op::AddrG, e.slot); break;
    case EV::Field: emit(Op::AddrF, e.slot); break;
    case EV::Elem: emit(Op::AddrE); break;
    case EV::Elem2: emit(Op::AddrE2); break;
    case EV::Ptr: break;
    case EV::FuncGroup: { load(e, at); r.type = ptrTo(e.type); return r; }
    default: error("& di un valore che non ha un indirizzo", at);
    }
    r.type = ptrTo(t);
    return r;
}
EV Compiler::addressOf(Node* n) {
    if (lang_ == Dialect::CSharp || lang_ == Dialect::Java) error("& (indirizzo) non esiste in questo linguaggio", n->pos);
    EV e = expr(n);
    if (e.cat == EV::FuncGroup) {
        if (e.fns.size() != 1) error("& di una funzione con piu' versioni", n->pos);
        load(e, n->pos);
        EV r; r.type = ptrTo(e.type); return r;
    }
    if (!e.lvalue && !(stripRef(e.type)->k == TK::Class)) error("& di un valore temporaneo", n->pos);
    return addressOfEV(e, n->pos);
}

} // namespace fx::prog
