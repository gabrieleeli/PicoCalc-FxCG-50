// Heap dei programmi (v1.20.0): arena TLSF con budget (PSRAM se c'e'), GC
// mark-and-sweep con una pila esplicita, oggetti e valori: array tipizzati,
// istanze, stringhe, contenitori, copie per valore, distruttori del C++,
// puntatori controllati, conversioni, confronti e hash. Vedi program_vm.hpp.
#include "program_vm.hpp"
#include "file_store.hpp"
#include "mem_pools.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace fx::prog {

// ================================================================ testo
std::u16string utf8To16(std::string_view s) {
    std::u16string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char b = static_cast<unsigned char>(s[i]);
        unsigned cp = b;
        std::size_t n = 1;
        if (b >= 0xF0 && i + 3 < s.size()) { cp = ((b & 7u) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3Fu); n = 4; }
        else if (b >= 0xE0 && i + 2 < s.size()) { cp = ((b & 0xFu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 2]) & 0x3Fu); n = 3; }
        else if (b >= 0xC0 && i + 1 < s.size()) { cp = ((b & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu); n = 2; }
        else if (b >= 0x80) cp = 0xFFFD;
        if (cp >= 0x10000) { cp -= 0x10000; out += static_cast<char16_t>(0xD800 + (cp >> 10)); out += static_cast<char16_t>(0xDC00 + (cp & 0x3FF)); }
        else out += static_cast<char16_t>(cp);
        i += n;
    }
    return out;
}
std::string utf16To8(std::u16string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        unsigned cp = s[i];
        if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00); ++i; }
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    }
    return out;
}

// ================================================================ heap
struct Heap::Resource final : std::pmr::memory_resource {
    explicit Resource(Heap& h) : heap(h) {}
    Heap& heap;
    void* do_allocate(std::size_t bytes, std::size_t align) override { return heap.raw(bytes ? bytes : 1, align); }
    void do_deallocate(void* p, std::size_t, std::size_t) override { heap.free(p); }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
};

Heap::Heap(Vm& vm, std::size_t budget) : vm_(vm), budget_(budget) {
    arena_ = mem::tryAlloc(mem::Policy::PREFER_PSRAM, budget, 16);
    if (arena_ && !tlsf_.init(arena_, budget)) { mem::free(arena_); arena_ = nullptr; }
    res_ = std::make_unique<Resource>(*this);
}
Heap::~Heap() {
    // tutti gli oggetti (i distruttori liberano la memoria interna nell'arena)
    Obj* o = head_;
    while (o) {
        Obj* n = o->next;
        o->~Obj();
        o = n;
    }
    head_ = nullptr;
    if (arena_) mem::free(arena_);
}
std::pmr::memory_resource* Heap::resource() noexcept { return res_.get(); }
std::size_t Heap::used() const noexcept { return arena_ ? tlsf_.stats().usedBytes : 0; }
void* Heap::raw(std::size_t bytes, std::size_t align) {
    if (!arena_) throw std::bad_alloc();
    if (align < 8) align = 8;
    void* p = tlsf_.allocate(bytes, align);
    // GC quando la crescita dall'ultimo ciclo supera la meta' del budget, o quando non c'e' spazio
    sinceGc_ += bytes;
    if (!p || (sinceGc_ > budget_ / 2 && !collecting_)) {
        if (!collecting_) {
            collect();
            if (!p) p = tlsf_.allocate(bytes, align);
        }
    }
    if (!p) throw std::bad_alloc();
    return p;
}
void Heap::free(void* p) noexcept { if (p) tlsf_.deallocate(p); }
void Heap::link(Obj* o, std::size_t bytes) {
    o->next = head_;
    head_ = o;
    o->bytes = static_cast<std::uint32_t>(bytes);
    ++objects_;
    vm_.root(o);
}
void Heap::destroy(Obj* o) noexcept {
    o->~Obj();
    tlsf_.deallocate(o);
    --objects_;
}

// Radici: pila, globali, letterali, file aperti, oggetti temporanei dei
// nativi, blocchi di malloc/new non liberati.
void Vm::markRoots(std::vector<Obj*>& work) {
    auto val = [&](const Value& v) { if (v.holdsObj() && !(v.k == VK::Ptr && v.f == 2)) work.push_back(v.o); };
    for (std::size_t i = 0; i < sp && i < stack.size(); ++i) val(stack[i]);
    if (globals) work.push_back(globals);
    for (Obj* o : temps_) if (o) work.push_back(o);
    for (Obj* o : literals) if (o) work.push_back(o);
    for (Obj* o : cppLiterals) if (o) work.push_back(o);
    for (FileObj* f : openFiles) if (f) work.push_back(f);
    for (FileObj* f : stdio) if (f) work.push_back(f);
    if (coutObj) work.push_back(coutObj);
    if (cinObj) work.push_back(cinObj);
    val(strtokState);
}
void Heap::collect() {
    if (collecting_) return;
    collecting_ = true;
    ++cycles_;
    std::vector<Obj*> work;
    try {
        work.reserve(256);
        vm_.markRoots(work);
        for (Obj* o = head_; o; o = o->next) if ((o->flags & (FRoot | FExplicit)) && !o->dead()) work.push_back(o);
        auto val = [&](const Value& v) { if (v.holdsObj() && !(v.k == VK::Ptr && v.f == 2)) work.push_back(v.o); };
        while (!work.empty()) {
            Obj* o = work.back();
            work.pop_back();
            if (!o || (o->flags & FMark)) continue;
            o->flags |= FMark;
            switch (o->kind) {
            case OK::Array: { ArrayObj* a = static_cast<ArrayObj*>(o); if (a->refs() && a->data) for (std::uint32_t i = 0; i < a->count; ++i) val(a->get(i)); break; }
            case OK::Inst: { InstObj* x = static_cast<InstObj*>(o); for (std::uint32_t i = 0; i < x->nfields; ++i) val(x->fields[i]); break; }
            case OK::Box: val(static_cast<BoxObj*>(o)->v); break;
            case OK::Vec: for (const Value& v : static_cast<VecObj*>(o)->items) val(v); break;
            case OK::Map: { MapObj* m = static_cast<MapObj*>(o); for (std::size_t i = 0; i < m->keys.size(); ++i) if (m->used[i]) { val(m->keys[i]); if (i < m->vals.size()) val(m->vals[i]); } break; }
            case OK::Closure: for (const Value& v : static_cast<ClosureObj*>(o)->caps) val(v); break;
            case OK::Native: for (const Value& v : static_cast<NativeObj*>(o)->slots) val(v); break;
            default: break;
            }
        }
    } catch (std::bad_alloc&) {
        // senza memoria per la pila del GC: nessuna raccolta (si segna tutto)
        for (Obj* o = head_; o; o = o->next) o->flags &= static_cast<std::uint8_t>(~FMark);
        collecting_ = false;
        return;
    }
    Obj** link = &head_;
    while (*link) {
        Obj* o = *link;
        if (o->flags & FMark) { o->flags &= static_cast<std::uint8_t>(~FMark); link = &o->next; continue; }
        *link = o->next;
        destroy(o);
    }
    sinceGc_ = 0;
    collecting_ = false;
}

// ================================================================ array
ArrayObj::~ArrayObj() { if (data && heap) heap->free(data); }
namespace {
std::uint8_t elemSize(VK k) {
    switch (k) {
    case VK::Bool: case VK::C8: case VK::I8: case VK::U8: return 1;
    case VK::C16: case VK::I16: case VK::U16: return 2;
    case VK::I32: case VK::U32: case VK::F32: return 4;
    case VK::I64: case VK::U64: case VK::F64: return 8;
    default: return static_cast<std::uint8_t>(sizeof(Value));
    }
}
bool packed(VK k) { return elemSize(k) != sizeof(Value); }
} // namespace
Value ArrayObj::get(std::uint32_t i) const noexcept {
    const std::uint8_t* p = data + static_cast<std::size_t>(i) * esize;
    Value v;
    v.k = ek;
    switch (ek) {
    case VK::Bool: case VK::U8: v.i = *p; break;
    case VK::C8: case VK::I8: v.i = static_cast<std::int8_t>(*p); break;
    case VK::C16: case VK::U16: { std::uint16_t x; std::memcpy(&x, p, 2); v.i = x; break; }
    case VK::I16: { std::int16_t x; std::memcpy(&x, p, 2); v.i = x; break; }
    case VK::I32: { std::int32_t x; std::memcpy(&x, p, 4); v.i = x; break; }
    case VK::U32: { std::uint32_t x; std::memcpy(&x, p, 4); v.i = x; break; }
    case VK::I64: case VK::U64: std::memcpy(&v.i, p, 8); break;
    case VK::F32: { float x; std::memcpy(&x, p, 4); v.d = x; break; }
    case VK::F64: std::memcpy(&v.d, p, 8); break;
    default: std::memcpy(&v, p, sizeof(Value)); break;
    }
    return v;
}
void ArrayObj::set(std::uint32_t i, const Value& v) noexcept {
    std::uint8_t* p = data + static_cast<std::size_t>(i) * esize;
    const std::int64_t n = (v.k == VK::F32 || v.k == VK::F64) ? v.integer() : v.i;
    switch (ek) {
    case VK::Bool: *p = n != 0; break;
    case VK::C8: case VK::I8: case VK::U8: *p = static_cast<std::uint8_t>(n); break;
    case VK::C16: case VK::I16: case VK::U16: { const std::uint16_t x = static_cast<std::uint16_t>(n); std::memcpy(p, &x, 2); break; }
    case VK::I32: case VK::U32: { const std::uint32_t x = static_cast<std::uint32_t>(n); std::memcpy(p, &x, 4); break; }
    case VK::I64: case VK::U64: std::memcpy(p, &n, 8); break;
    case VK::F32: { const float x = static_cast<float>(v.num()); std::memcpy(p, &x, 4); break; }
    case VK::F64: { const double x = v.num(); std::memcpy(p, &x, 8); break; }
    default: std::memcpy(p, &v, sizeof(Value)); break;
    }
}

void Vm::root(Obj* o) { temps_.push_back(o); }

const std::vector<TypeP>& Vm::slotsOf(const ClassInfo* c) {
    auto it = slotTypes.find(c);
    if (it != slotTypes.end()) return it->second;
    std::vector<TypeP> v(static_cast<std::size_t>(c->slotCount), nullptr);
    for (const ClassInfo* k = c; k; k = k->base)
        for (const FieldInfo& f : k->fields) if (!f.isStatic && f.slot >= 0 && static_cast<std::size_t>(f.slot) < v.size()) v[static_cast<std::size_t>(f.slot)] = f.type;
    return slotTypes.emplace(c, std::move(v)).first->second;
}

ArrayObj* Vm::newArray(TypeP elem, std::uint32_t count, bool explicitAlloc) {
    ArrayObj* a = heap_.make<ArrayObj>();
    a->heap = &heap_;
    a->elem = elem;
    a->type = nullptr;
    if (explicitAlloc) a->flags |= FExplicit;
    const VK k = elem ? vkOf(elem) : VK::Ref;
    if (elem && elem->k == TK::Void) {
        // blocco di malloc: prende il tipo alla prima conversione
        a->ek = VK::Void;
        a->rawBytes = count;
        return a;
    }
    a->ek = packed(k) ? k : VK::Ref;
    a->esize = elemSize(a->ek);
    a->count = count;
    const std::size_t bytes = static_cast<std::size_t>(count) * a->esize;
    if (bytes > heap_.budget()) throw std::bad_alloc();
    if (bytes) {
        a->data = static_cast<std::uint8_t*>(heap_.raw(bytes, 8));
        std::memset(a->data, 0, bytes);
        a->bytes += static_cast<std::uint32_t>(std::min<std::size_t>(bytes, 0xFFFFFFFFu));
        if (!packed(k)) {
            for (std::uint32_t i = 0; i < count; ++i) new (a->data + static_cast<std::size_t>(i) * sizeof(Value)) Value();
            if (elem) {
                const bool zeroOnly = !(vkOf(elem) == VK::Ref && isValueTypeRT(elem)) && elem->k != TK::Nullable;
                if (!zeroOnly) for (std::uint32_t i = 0; i < count; ++i) a->set(i, defaultValue(elem));
                else if (elem->k == TK::Ptr || elem->k == TK::Func || elem->k == TK::Class || elem->k == TK::Str || elem->k == TK::Array || elem->k == TK::Native || elem->k == TK::Dynamic || elem->k == TK::Null)
                    for (std::uint32_t i = 0; i < count; ++i) a->set(i, Value::null());
            } else for (std::uint32_t i = 0; i < count; ++i) a->set(i, Value::null());
        } else if (isFloat(k)) {
            // 0.0 come bit tutti a zero: gia' a posto
        }
    }
    return a;
}
ArrayObj* Vm::newCString(std::string_view s, bool readOnly) {
    ArrayObj* a = heap_.make<ArrayObj>();
    a->heap = &heap_;
    a->ek = VK::C8;
    a->esize = 1;
    a->count = static_cast<std::uint32_t>(s.size() + 1);
    a->data = static_cast<std::uint8_t*>(heap_.raw(s.size() + 1, 8));
    std::memcpy(a->data, s.data(), s.size());
    a->data[s.size()] = 0;
    if (readOnly) a->flags |= FReadOnly;
    return a;
}
InstObj* Vm::newInstance(ClassInfo* c, bool explicitAlloc) {
    const std::uint32_t n = static_cast<std::uint32_t>(std::max<std::int32_t>(c->slotCount, 0));
    const std::size_t bytes = sizeof(InstObj) + static_cast<std::size_t>(n) * sizeof(Value);
    void* mem = heap_.raw(bytes, alignof(InstObj));
    InstObj* o = new (mem) InstObj();
    o->cls = c;
    o->nfields = n;
    o->fields = reinterpret_cast<Value*>(reinterpret_cast<std::uint8_t*>(mem) + sizeof(InstObj));
    for (std::uint32_t i = 0; i < n; ++i) new (&o->fields[i]) Value();
    o->type = c->self;
    o->lang = static_cast<std::uint16_t>(c->lang);
    if (explicitAlloc) o->flags |= FExplicit;
    heap_.link(o, bytes);
    const std::vector<TypeP>& st = slotsOf(c);
    for (std::uint32_t i = 0; i < n && i < st.size(); ++i) if (st[i]) o->fields[i] = defaultValue(st[i]);
    return o;
}
InstObj* Vm::newPair(TypeP pairType, const Value& a, const Value& b) {
    const std::size_t bytes = sizeof(InstObj) + 2 * sizeof(Value);
    void* mem = heap_.raw(bytes, alignof(InstObj));
    InstObj* o = new (mem) InstObj();
    o->cls = nullptr;
    o->nfields = 2;
    o->fields = reinterpret_cast<Value*>(reinterpret_cast<std::uint8_t*>(mem) + sizeof(InstObj));
    new (&o->fields[0]) Value(a);
    new (&o->fields[1]) Value(b);
    o->type = pairType;
    heap_.link(o, bytes);
    return o;
}
StrBObj* Vm::newStrB(std::string_view s) {
    StrBObj* o = heap_.make<StrBObj>(heap_.resource());
    o->s.assign(s.data(), s.size());
    o->lang = static_cast<std::uint16_t>(Dialect::Cpp);
    return o;
}
StrWObj* Vm::newStrW(std::u16string_view s) {
    StrWObj* o = heap_.make<StrWObj>(heap_.resource());
    o->s.assign(s.data(), s.size());
    return o;
}
Value Vm::newString(Dialect d, std::string_view utf8s) {
    if (d == Dialect::Cpp) return Value::ref(newStrB(utf8s));
    if (d == Dialect::C) return Value::ptr(newCString(utf8s, false), 0);
    return Value::ref(newStrW(utf8To16(utf8s)));
}
VecObj* Vm::newVec(TypeP t) { VecObj* v = heap_.make<VecObj>(heap_.resource()); v->type = t; return v; }
MapObj* Vm::newMap(TypeP t) { MapObj* m = heap_.make<MapObj>(heap_.resource()); m->type = t; return m; }
SbObj* Vm::newSb() { return heap_.make<SbObj>(heap_.resource()); }
FileObj* Vm::newFile() { return heap_.make<FileObj>(heap_.resource()); }
NativeObj* Vm::newNativeObj(NK nk) { NativeObj* o = heap_.make<NativeObj>(heap_.resource()); o->nk = nk; return o; }
FileObj::~FileObj() = default;

bool isValueTypeRT(TypeP t) noexcept {
    if (!t) return false;
    switch (t->k) {
    case TK::Class: return t->cls && t->cls->valueType && !t->cls->isEnum;
    case TK::Str: return t->cppValue;
    case TK::Array: return t->size >= 0;
    case TK::Native:
        switch (t->nk) {
        case NK::Vector: case NK::Map: case NK::Set: case NK::UMap: case NK::USet: case NK::Pair: case NK::Stack: case NK::Queue:
        case NK::Deque: case NK::List: case NK::StringStream: case NK::CsKvp: return true;
        default: return false;
        }
    default: return false;
    }
}

Value Vm::newNative(TypeP t) {
    switch (t->nk) {
    case NK::Vector: case NK::Stack: case NK::Queue: case NK::Deque: case NK::List:
    case NK::CsList: case NK::CsQueue: case NK::CsStack: case NK::JArrayList: case NK::JLinkedList: case NK::JArrayDeque:
        return Value::ref(newVec(t));
    case NK::Map: case NK::Set: case NK::JTreeMap: case NK::JTreeSet: {
        MapObj* m = newMap(t);
        m->mode = 0;
        m->isSet = t->nk == NK::Set || t->nk == NK::JTreeSet;
        return Value::ref(m);
    }
    case NK::UMap: case NK::USet: case NK::CsDict: case NK::CsHashSet: {
        MapObj* m = newMap(t);
        m->mode = 1;
        m->isSet = t->nk == NK::USet || t->nk == NK::CsHashSet;
        return Value::ref(m);
    }
    case NK::JHashMap: case NK::JHashSet: {
        MapObj* m = newMap(t);
        m->mode = 2;
        m->isSet = t->nk == NK::JHashSet;
        return Value::ref(m);
    }
    case NK::Pair: case NK::CsKvp: {
        Value a = t->args.size() > 0 ? defaultValue(t->args[0]) : Value();
        root(a.holdsObj() ? a.o : nullptr);
        Value b = t->args.size() > 1 ? defaultValue(t->args[1]) : Value();
        return Value::ref(newPair(t, a, b));
    }
    case NK::CsStringBuilder: case NK::JStringBuilder: { SbObj* s = newSb(); s->type = t; return Value::ref(s); }
    case NK::StringStream: case NK::IFStream: case NK::OFStream: case NK::FStream: case NK::CsStreamReader: case NK::CsStreamWriter: case NK::JScanner: {
        FileObj* f = newFile();
        f->type = t;
        f->memory = t->nk == NK::StringStream;
        f->open = f->memory;
        f->mode = f->memory ? 9 : 0;
        return Value::ref(f);
    }
    default: {
        NativeObj* o = newNativeObj(t->nk);
        o->type = t;
        return Value::ref(o);
    }
    }
}

Value Vm::defaultValue(TypeP t) {
    if (!t) return Value();
    switch (t->k) {
    case TK::Void: return Value();
    case TK::Bool: return Value::boolean(false);
    case TK::F32: return Value::f32(0);
    case TK::F64: return Value::f64(0);
    case TK::Enum: return Value::ofInt(vkOf(t), 0);
    case TK::Ptr: case TK::Null: case TK::Func: case TK::Nullable: case TK::Dynamic: case TK::Ref: return Value::null();
    case TK::Str: return t->cppValue ? Value::ref(newStrB("")) : Value::null();
    case TK::Class:
        if (t->cls && t->cls->valueType && !t->cls->isEnum) return Value::ref(newInstance(t->cls));
        return Value::null();
    case TK::Array:
        if (t->size >= 0) {
            ArrayObj* a = newArray(t->elem, static_cast<std::uint32_t>(t->size));
            a->type = t;
            return Value::ref(a);
        }
        return Value::null();
    case TK::Native: return isValueTypeRT(t) ? newNative(t) : Value::null();
    default: return Value::ofInt(vkOf(t), 0);
    }
}

// ================================================================ semantica per valore
Value Vm::copyValue(const Value& v, std::int32_t sliceClass) {
    if (!v.holdsObj() || (v.k == VK::Ptr && v.f == 2)) return v;
    Obj* o = v.o;
    if (v.k == VK::Ptr && v.idx != Whole) {
        const Value inner = loadPtr(v);
        return copyValue(inner, sliceClass);
    }
    if (o->dead()) fault(FaultKind::DeadPointer, "copia di un oggetto gia' liberato");
    switch (o->kind) {
    case OK::Inst: {
        InstObj* src = static_cast<InstObj*>(o);
        ClassInfo* c = sliceClass >= 0 ? prog_->classes[static_cast<std::size_t>(sliceClass)].get() : src->cls;
        if (!c) {
            // coppia (std::pair, KeyValuePair)
            Value a = copyValue(src->fields[0]);
            root(a.holdsObj() ? a.o : nullptr);
            Value b = copyValue(src->fields[1]);
            return Value::ref(newPair(src->type, a, b));
        }
        if (src->cls && !src->cls->derivesFrom(c)) c = src->cls;
        // costruttore di copia dell'utente (C++)
        if (c->lang == Dialect::Cpp && !c->isNative) {
            for (Function* ctor : c->ctors) {
                if (ctor->params.size() != 1 || !ctor->params[0].byRef) continue;
                TypeP pt = ctor->params[0].type->elem;
                if (!pt || pt->k != TK::Class || pt->cls != c) continue;
                InstObj* dst = newInstance(c);
                Value args[2] = {Value::ref(dst), Value::ptr(src, Whole)};
                invokeFunction(ctor, args, 2, false);
                return Value::ref(dst);
            }
        }
        InstObj* dst = newInstance(c);
        const std::vector<TypeP>& st = slotsOf(c);
        const std::uint32_t n = std::min(dst->nfields, src->nfields);
        for (std::uint32_t i = 0; i < n; ++i) {
            const TypeP ft = i < st.size() ? st[i] : nullptr;
            dst->fields[i] = isValueTypeRT(ft) ? copyValue(src->fields[i]) : src->fields[i];
        }
        return Value::ref(dst);
    }
    case OK::StrB: return Value::ref(newStrB(static_cast<StrBObj*>(o)->s));
    case OK::Vec: {
        VecObj* src = static_cast<VecObj*>(o);
        VecObj* dst = newVec(src->type);
        const TypeP et = src->type && !src->type->args.empty() ? src->type->args[0] : nullptr;
        const bool deep = isValueTypeRT(et);
        dst->items.reserve(src->items.size());
        for (const Value& e : src->items) dst->items.push_back(deep ? copyValue(e) : e);
        return Value::ref(dst);
    }
    case OK::Map: {
        MapObj* src = static_cast<MapObj*>(o);
        MapObj* dst = newMap(src->type);
        dst->mode = src->mode;
        dst->isSet = src->isSet;
        const TypeP kt = src->type && !src->type->args.empty() ? src->type->args[0] : nullptr;
        const TypeP vt = src->type && src->type->args.size() > 1 ? src->type->args[1] : nullptr;
        dst->keys = src->keys;
        dst->vals = src->vals;
        dst->used = src->used;
        dst->order = src->order;
        dst->buckets = src->buckets;
        dst->next = src->next;
        dst->freeSlots = src->freeSlots;
        dst->count = src->count;
        if (isValueTypeRT(kt)) for (std::size_t i = 0; i < dst->keys.size(); ++i) if (dst->used[i]) dst->keys[i] = copyValue(dst->keys[i]);
        if (isValueTypeRT(vt)) for (std::size_t i = 0; i < dst->vals.size(); ++i) if (dst->used[i]) dst->vals[i] = copyValue(dst->vals[i]);
        return Value::ref(dst);
    }
    case OK::Array: {
        ArrayObj* src = static_cast<ArrayObj*>(o);
        if (src->ek == VK::Void) fault(FaultKind::Generic, "copia di memoria senza tipo");
        ArrayObj* dst = newArray(src->elem, src->count);
        dst->type = src->type;
        dst->rank = src->rank;
        dst->rows = src->rows;
        dst->cols = src->cols;
        const bool deep = isValueTypeRT(src->elem) || !src->elem;
        for (std::uint32_t i = 0; i < src->count; ++i) { const Value e = src->get(i); dst->set(i, deep && src->refs() ? copyValue(e) : e); }
        return Value::ref(dst);
    }
    case OK::File: {
        FileObj* src = static_cast<FileObj*>(o);
        if (!src->memory) return v;
        FileObj* dst = newFile();
        dst->type = src->type;
        dst->memory = true;
        dst->open = true;
        dst->mode = src->mode;
        dst->buf = src->buf;
        dst->pos = src->pos;
        return Value::ref(dst);
    }
    default: return v;
    }
}
void Vm::copyInto(const Value& dstV, const Value& srcV) {
    Obj* d = objOf(dstV);
    if (srcV.isNull()) fault(FaultKind::NullPointer, "assegnazione da un oggetto nullo");
    Value src = srcV;
    if (src.k == VK::Ptr && src.idx != Whole && src.f != 2) src = loadPtr(src);
    Obj* s = objOf(src);
    if (d == s) return;
    switch (d->kind) {
    case OK::Inst: {
        InstObj* dst = static_cast<InstObj*>(d);
        InstObj* sr = static_cast<InstObj*>(s);
        if (s->kind != OK::Inst) fault(FaultKind::Cast, "assegnazione tra oggetti di tipi diversi");
        const std::vector<TypeP>* st = dst->cls ? &slotsOf(dst->cls) : nullptr;
        const std::uint32_t n = std::min(dst->nfields, sr->nfields);
        for (std::uint32_t i = 0; i < n; ++i) {
            const TypeP ft = st && i < st->size() ? (*st)[i] : nullptr;
            if (isValueTypeRT(ft) && dst->fields[i].holdsObj() && sr->fields[i].holdsObj()) {
                // membro con operator= dell'utente
                if (ft->k == TK::Class && ft->cls && ft->cls->lang == Dialect::Cpp) {
                    Function* op = nullptr;
                    for (Function* m : ft->cls->methods) if (m->simple == "operator=" && m->params.size() == 1) op = m;
                    if (op) { Value args[2] = {dst->fields[i], Value::ptr(sr->fields[i].o, Whole)}; invokeFunction(op, args, 2, op->ret && op->ret->k != TK::Void); continue; }
                }
                copyInto(dst->fields[i], sr->fields[i]);
            } else if (isValueTypeRT(ft)) dst->fields[i] = copyValue(sr->fields[i]);
            else dst->fields[i] = sr->fields[i];
        }
        return;
    }
    case OK::StrB: {
        if (s->kind == OK::StrB) static_cast<StrBObj*>(d)->s = static_cast<StrBObj*>(s)->s;
        else static_cast<StrBObj*>(d)->s = std::pmr::string(utf8(src), heap_.resource());
        return;
    }
    case OK::Vec: {
        Value c = copyValue(src);
        static_cast<VecObj*>(d)->items = static_cast<VecObj*>(c.o)->items;
        ++static_cast<VecObj*>(d)->version;
        return;
    }
    case OK::Map: {
        Value c = copyValue(src);
        MapObj* dm = static_cast<MapObj*>(d);
        MapObj* cm = static_cast<MapObj*>(c.o);
        dm->keys = cm->keys; dm->vals = cm->vals; dm->used = cm->used; dm->order = cm->order;
        dm->buckets = cm->buckets; dm->next = cm->next; dm->freeSlots = cm->freeSlots; dm->count = cm->count;
        ++dm->version;
        return;
    }
    case OK::Array: {
        ArrayObj* da = static_cast<ArrayObj*>(d);
        ArrayObj* sa = static_cast<ArrayObj*>(s);
        if (s->kind != OK::Array) fault(FaultKind::Cast, "assegnazione di un array");
        const std::uint32_t n = std::min(da->count, sa->count);
        for (std::uint32_t i = 0; i < n; ++i) { const Value e = sa->get(i); da->set(i, da->refs() && isValueTypeRT(da->elem) ? copyValue(e) : e); }
        return;
    }
    case OK::File: {
        if (s->kind == OK::File) { static_cast<FileObj*>(d)->buf = static_cast<FileObj*>(s)->buf; static_cast<FileObj*>(d)->pos = static_cast<FileObj*>(s)->pos; }
        return;
    }
    default: fault(FaultKind::Generic, "assegnazione non valida");
    }
}

// C++: distruttore della classe (virtuale: quello della classe vera), poi
// i membri in ordine inverso, poi la base.
void Vm::destroyValue(const Value& v, ClassInfo* staticCls) {
    if (!v.holdsObj() || (v.k == VK::Ptr && v.f == 2)) return;
    Obj* o = v.o;
    if (v.k == VK::Ptr && v.idx != Whole) { const Value inner = loadPtr(v); destroyValue(inner, staticCls); return; }
    if (o->dead()) return;
    if (o->kind == OK::Array) {
        ArrayObj* a = static_cast<ArrayObj*>(o);
        if (a->refs()) for (std::uint32_t i = a->count; i-- > 0;) destroyValue(a->get(i), nullptr);
        return;
    }
    if (o->kind == OK::Vec) {
        VecObj* vv = static_cast<VecObj*>(o);
        for (std::size_t i = vv->items.size(); i-- > 0;) destroyValue(vv->items[i], nullptr);
        return;
    }
    if (o->kind != OK::Inst) return;
    InstObj* x = static_cast<InstObj*>(o);
    if (x->destroyed || !x->cls || x->cls->lang != Dialect::Cpp) return;
    ClassInfo* start = x->cls;
    if (staticCls && x->cls->derivesFrom(staticCls)) {
        // distruttore non virtuale: solo la parte del tipo statico
        bool virt = false;
        for (ClassInfo* k = staticCls; k; k = k->base) if (k->dtor && k->dtor->isVirtual) virt = true;
        if (!virt) start = staticCls;
    }
    x->destroyed = true;
    for (ClassInfo* c = start; c; c = c->base) {
        if (c->dtor && c->dtor->defined && !c->dtor->code.empty()) {
            Value self = Value::ref(x);
            invokeFunction(c->dtor, &self, 1, false);
        }
        for (std::size_t k = c->fields.size(); k-- > 0;) {
            const FieldInfo& f = c->fields[k];
            if (f.isStatic || f.slot < 0 || static_cast<std::uint32_t>(f.slot) >= x->nfields) continue;
            if (isValueTypeRT(f.type)) destroyValue(x->fields[f.slot], f.type->k == TK::Class ? f.type->cls : nullptr);
        }
    }
}
// Fine della vita di un oggetto per valore (blocco finito, free, delete): i
// puntatori che restano lo trovano "morto".
void Vm::killValue(const Value& v) {
    if (!v.holdsObj() || (v.k == VK::Ptr && v.f == 2)) return;
    Obj* o = v.o;
    if (v.k == VK::Ptr && v.idx != Whole) return;
    if (o->dead() || (o->flags & FRoot)) return;
    o->flags |= FDead;
    o->flags &= static_cast<std::uint8_t>(~FExplicit);
    switch (o->kind) {
    case OK::Array: {
        ArrayObj* a = static_cast<ArrayObj*>(o);
        if (a->refs() && a->data) for (std::uint32_t i = 0; i < a->count; ++i) { const Value e = a->get(i); if (e.k == VK::Ref && isValueTypeRT(a->elem)) killValue(e); }
        if (a->data) { heap_.free(a->data); a->data = nullptr; }
        a->count = 0;
        break;
    }
    case OK::Inst: {
        InstObj* x = static_cast<InstObj*>(o);
        if (x->cls) {
            const std::vector<TypeP>& st = slotsOf(x->cls);
            for (std::uint32_t i = 0; i < x->nfields && i < st.size(); ++i) if (isValueTypeRT(st[i])) killValue(x->fields[i]);
        }
        for (std::uint32_t i = 0; i < x->nfields; ++i) x->fields[i] = Value();
        break;
    }
    case OK::StrB: { std::pmr::string().swap(static_cast<StrBObj*>(o)->s); break; }
    case OK::Vec: { std::pmr::vector<Value>(heap_.resource()).swap(static_cast<VecObj*>(o)->items); break; }
    case OK::Map: {
        MapObj* m = static_cast<MapObj*>(o);
        m->keys.clear(); m->vals.clear(); m->used.clear(); m->order.clear(); m->buckets.clear(); m->next.clear(); m->freeSlots.clear(); m->count = 0;
        break;
    }
    default: break;
    }
}

// ================================================================ puntatori
Obj* Vm::objOf(const Value& v, bool allowNull) {
    Value x = v;
    if (x.k == VK::Ptr && x.f == 2) x = loadPtr(x);
    if (x.isNull() || !x.holdsObj()) {
        if (allowNull) return nullptr;
        if (x.k == VK::Null || ((x.k == VK::Ref || x.k == VK::Ptr) && !x.o)) {
            const Dialect d = lang();
            fault(FaultKind::NullPointer, d == Dialect::CSharp ? "riferimento a un oggetto null" : d == Dialect::Java ? "oggetto null" : "puntatore NULL");
        }
        fault(FaultKind::Generic, "valore che non e' un oggetto");
    }
    if (x.k == VK::Ptr && x.idx != Whole) {
        Obj* c = x.o;
        if (c->kind == OK::Array || c->kind == OK::Vec || c->kind == OK::Map || c->kind == OK::Box) {
            const Value e = loadPtr(x);
            if (e.holdsObj() && e.k == VK::Ref) x = e;
            else if (c->kind == OK::Array && static_cast<ArrayObj*>(c)->ek != VK::Ref) {
                // &a[0] di un array di numeri usato come oggetto: l'array stesso (memset, strcpy...)
                return c;
            } else x = e;
            if (!x.holdsObj()) fault(FaultKind::NullPointer, "puntatore a un elemento vuoto");
        } else if (c->kind == OK::Inst) {
            const Value e = loadPtr(x);
            if (e.holdsObj()) x = e;
        }
    }
    if (x.o->dead()) fault(FaultKind::DeadPointer, "oggetto gia' liberato o uscito dal suo blocco");
    return x.o;
}
Value Vm::loadPtr(const Value& p) {
    if (p.k == VK::Ref) { if (!p.o) fault(FaultKind::NullPointer, "riferimento null"); if (p.o->dead()) fault(FaultKind::DeadPointer, "oggetto gia' liberato"); return p; }
    if (p.k != VK::Ptr || (!p.o && p.f != 2)) {
        if (p.isNull()) fault(FaultKind::NullPointer, "dereferenziazione di un puntatore NULL");
        fault(FaultKind::Generic, "dereferenziazione di un valore che non e' un puntatore");
    }
    if (p.f == 2) {
        if (p.aux >= frames.size() || frames[p.aux].serial != p.u) fault(FaultKind::DeadPointer, "puntatore a una variabile locale non piu' valida (la funzione e' finita)");
        const Value& v = stack[p.idx];
        if (v.k == VK::Void && false) fault(FaultKind::DeadPointer, "puntatore a una variabile uscita dal suo blocco");
        return v;
    }
    Obj* o = p.o;
    if (o->dead()) fault(FaultKind::DeadPointer, (o->flags & FExplicit) ? "memoria gia' liberata" : "memoria gia' liberata o variabile uscita dal suo blocco");
    if (p.idx == Whole) return Value::ref(o);
    switch (o->kind) {
    case OK::Array: {
        ArrayObj* a = static_cast<ArrayObj*>(o);
        if (a->ek == VK::Void) fault(FaultKind::Generic, "memoria di malloc usata prima di avere un tipo (manca la conversione a T*)");
        if (p.idx >= a->count) fault(FaultKind::Index, "puntatore oltre la fine dell'array (indice " + std::to_string(static_cast<std::int32_t>(p.idx)) + " su " + std::to_string(a->count) + ")");
        return a->get(p.idx);
    }
    case OK::Inst: {
        InstObj* x = static_cast<InstObj*>(o);
        if (p.idx >= x->nfields) fault(FaultKind::Index, "campo non valido");
        return x->fields[p.idx];
    }
    case OK::Box: return static_cast<BoxObj*>(o)->v;
    case OK::Vec: {
        VecObj* v = static_cast<VecObj*>(o);
        if (p.idx >= v->items.size()) fault(FaultKind::Index, "iteratore o riferimento oltre la fine del contenitore");
        return v->items[p.idx];
    }
    case OK::StrB: {
        StrBObj* s = static_cast<StrBObj*>(o);
        if (p.idx > s->s.size()) fault(FaultKind::Index, "indice oltre la fine della stringa");
        return Value::ofInt(VK::C8, p.idx == s->s.size() ? 0 : static_cast<signed char>(s->s[p.idx]));
    }
    case OK::Map: {
        MapObj* m = static_cast<MapObj*>(o);
        if (p.idx >= m->vals.size() || !m->used[p.idx]) fault(FaultKind::DeadPointer, "riferimento a un elemento rimosso dalla mappa");
        return m->vals[p.idx];
    }
    default: fault(FaultKind::Generic, "puntatore non valido");
    }
}
void Vm::storePtr(const Value& p, const Value& v) {
    if (p.k == VK::Ref) { copyInto(p, v); return; }
    if (p.k != VK::Ptr || (!p.o && p.f != 2)) {
        if (p.isNull()) fault(FaultKind::NullPointer, "scrittura attraverso un puntatore NULL");
        fault(FaultKind::Generic, "scrittura attraverso un valore che non e' un puntatore");
    }
    if (p.f == 2) {
        if (p.aux >= frames.size() || frames[p.aux].serial != p.u) fault(FaultKind::DeadPointer, "puntatore a una variabile locale non piu' valida (la funzione e' finita)");
        stack[p.idx] = v;
        return;
    }
    Obj* o = p.o;
    if (o->dead()) fault(FaultKind::DeadPointer, "scrittura in memoria gia' liberata");
    if (p.idx == Whole) { copyInto(Value::ref(o), v); return; }
    switch (o->kind) {
    case OK::Array: {
        ArrayObj* a = static_cast<ArrayObj*>(o);
        if (a->flags & FReadOnly) fault(FaultKind::ReadOnly, "scrittura su una stringa letterale");
        if (a->ek == VK::Void) fault(FaultKind::Generic, "memoria di malloc usata prima di avere un tipo");
        if (p.idx >= a->count) fault(FaultKind::Index, "scrittura oltre la fine dell'array (indice " + std::to_string(static_cast<std::int32_t>(p.idx)) + " su " + std::to_string(a->count) + ")");
        a->set(p.idx, v);
        return;
    }
    case OK::Inst: { InstObj* x = static_cast<InstObj*>(o); if (p.idx >= x->nfields) fault(FaultKind::Index, "campo non valido"); x->fields[p.idx] = v; return; }
    case OK::Box: static_cast<BoxObj*>(o)->v = v; return;
    case OK::Vec: { VecObj* vv = static_cast<VecObj*>(o); if (p.idx >= vv->items.size()) fault(FaultKind::Index, "scrittura oltre la fine del contenitore"); vv->items[p.idx] = v; return; }
    case OK::StrB: { StrBObj* s = static_cast<StrBObj*>(o); if (p.idx >= s->s.size()) fault(FaultKind::Index, "scrittura oltre la fine della stringa"); s->s[p.idx] = static_cast<char>(v.integer()); return; }
    case OK::Map: { MapObj* m = static_cast<MapObj*>(o); if (p.idx >= m->vals.size() || !m->used[p.idx]) fault(FaultKind::DeadPointer, "elemento rimosso dalla mappa"); m->vals[p.idx] = v; return; }
    default: fault(FaultKind::Generic, "puntatore non valido");
    }
}

// ================================================================ conversioni
Value Vm::convertTo(const Value& v, VK k) {
    if (v.k == k) return v;
    Value r;
    r.k = k;
    if (k == VK::F32 || k == VK::F64) {
        double d = 0;
        if (isFloat(v.k)) d = v.d;
        else if (v.k == VK::U64) d = static_cast<double>(v.u);
        else if (isInt(v.k) || v.k == VK::Bool) d = static_cast<double>(v.i);
        else if (v.k == VK::Null) d = 0;
        else fault(FaultKind::Cast, "conversione di un valore non numerico");
        r.d = k == VK::F32 ? static_cast<double>(static_cast<float>(d)) : d;
        return r;
    }
    if (k == VK::Bool) { r.i = truthy(v) ? 1 : 0; return r; }
    if (!isInt(k)) return v;
    std::int64_t n = 0;
    if (isFloat(v.k)) {
        const double d = v.d;
        const Dialect lg = lang();
        const int bits = bitsOf(k);
        const bool uns = k == VK::U8 || k == VK::U16 || k == VK::U32 || k == VK::U64 || k == VK::C16;
        if (std::isnan(d)) n = lg == Dialect::Java ? 0 : (bits >= 32 && !uns ? (bits == 32 ? INT32_MIN : INT64_MIN) : 0);
        else if (lg == Dialect::Java) {
            // Java: saturazione a int o long, poi restringimento
            if (bits <= 32) { const double c = std::max(-2147483648.0, std::min(2147483647.0, std::trunc(d))); n = static_cast<std::int64_t>(c); }
            else { n = d >= 9.2233720368547758e18 ? INT64_MAX : d <= -9.2233720368547758e18 ? INT64_MIN : static_cast<std::int64_t>(d); }
        } else if (uns && k == VK::U64) {
            n = d >= 1.8446744073709552e19 || d <= -1.0 ? (d < 0 ? static_cast<std::int64_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(d))) : 0) : static_cast<std::int64_t>(static_cast<std::uint64_t>(d));
        } else {
            const double lim = bits == 64 ? 9.2233720368547758e18 : 2147483648.0;
            if (d >= lim || d < -lim) n = bits == 64 ? INT64_MIN : INT32_MIN;   // fuori campo: come l'istruzione del processore
            else n = static_cast<std::int64_t>(d);
            if (uns && bits == 32 && d >= 0 && d < 4294967296.0) n = static_cast<std::int64_t>(static_cast<std::uint32_t>(d));
        }
    } else if (isInt(v.k) || v.k == VK::Bool) n = v.i;
    else if (v.k == VK::Null) n = 0;
    else if (v.k == VK::Ptr || v.k == VK::Ref) fault(FaultKind::Cast, "conversione tra puntatori e interi non supportata");
    else fault(FaultKind::Cast, "conversione di un valore non numerico");
    // ampiezza
    switch (k) {
    case VK::C8: case VK::I8: r.i = static_cast<std::int8_t>(n); break;
    case VK::U8: r.i = static_cast<std::uint8_t>(n); break;
    case VK::I16: r.i = static_cast<std::int16_t>(n); break;
    case VK::U16: case VK::C16: r.i = static_cast<std::uint16_t>(n); break;
    case VK::I32: r.i = static_cast<std::int32_t>(n); break;
    case VK::U32: r.i = static_cast<std::uint32_t>(n); break;
    default: r.i = n; break;
    }
    return r;
}
std::int64_t Vm::sizeOfType(TypeP t) const {
    if (!t) return 8;
    switch (t->k) {
    case TK::Bool: case TK::C8: case TK::I8: case TK::U8: return 1;
    case TK::C16: case TK::I16: case TK::U16: return 2;
    case TK::I32: case TK::U32: case TK::F32: case TK::Enum: return 4;
    case TK::Array: return t->size >= 0 ? t->size * sizeOfType(t->elem) : 8;
    case TK::Class: return t->cls && t->cls->sizeBytes > 0 ? t->cls->sizeBytes : 8;
    case TK::Str: return 32;
    default: return 8;
    }
}
Value Vm::castTo(const Value& v, TypeP t) {
    if (!t) return v;
    switch (t->k) {
    case TK::Ptr: {
        if (v.isNull()) return Value::null();
        if (v.k == VK::Ref) {
            if (v.o->kind == OK::Array && !(t->elem && t->elem->k == TK::Array)) return Value::ptr(v.o, 0);
            return Value::ptr(v.o, Whole);
        }
        if (v.k == VK::Ptr && v.f != 2 && v.o && v.o->kind == OK::Array && t->elem && t->elem->k != TK::Void) {
            ArrayObj* a = static_cast<ArrayObj*>(v.o);
            if (a->ek == VK::Void) {
                // malloc: il blocco diventa un array del tipo
                const std::int64_t es = sizeOfType(t->elem);
                if (es <= 0) fault(FaultKind::Generic, "tipo senza dimensione");
                if (a->rawBytes % static_cast<std::uint64_t>(es) != 0) fault(FaultKind::Argument, "malloc di " + std::to_string(a->rawBytes) + " byte non e' un multiplo di sizeof(" + t->elem->name() + ") = " + std::to_string(es));
                const std::uint64_t n = a->rawBytes / static_cast<std::uint64_t>(es);
                const VK k = vkOf(t->elem);
                a->elem = t->elem;
                a->ek = packed(k) ? k : VK::Ref;
                a->esize = elemSize(a->ek);
                a->count = static_cast<std::uint32_t>(n);
                const std::size_t bytes = static_cast<std::size_t>(n) * a->esize;
                if (bytes) {
                    a->data = static_cast<std::uint8_t*>(heap_.raw(bytes, 8));
                    std::memset(a->data, 0, bytes);
                    if (!packed(k)) {
                        for (std::uint32_t i = 0; i < a->count; ++i) new (a->data + static_cast<std::size_t>(i) * sizeof(Value)) Value();
                        for (std::uint32_t i = 0; i < a->count; ++i) a->set(i, isValueTypeRT(t->elem) ? defaultValue(t->elem) : Value::null());
                    }
                }
            } else if (a->elem && t->elem && a->elem != t->elem && vkOf(a->elem) != vkOf(t->elem) && t->elem->k != TK::Dynamic &&
                       !((a->ek == VK::C8 || a->ek == VK::U8 || a->ek == VK::I8) && (vkOf(t->elem) == VK::C8 || vkOf(t->elem) == VK::U8 || vkOf(t->elem) == VK::I8))) {
                fault(FaultKind::Cast, "memoria di tipo " + a->elem->name() + " usata come " + t->elem->name());
            }
        }
        return v;
    }
    case TK::Nullable: return v;
    case TK::Str:
        if (v.isNull()) return v;
        if (v.holdsObj() && (v.o->kind == OK::StrW || v.o->kind == OK::StrB)) return v;
        fault(FaultKind::Cast, "il valore non e' una stringa");
    default:
        if (isNumeric(t) && v.k == VK::Null) fault(FaultKind::InvalidOp, "l'oggetto nullable deve avere un valore");
        if (isNumeric(t)) return convertTo(v, vkOf(t));
        return v;
    }
}
bool Vm::truthy(const Value& v) const noexcept {
    switch (v.k) {
    case VK::Void: case VK::Null: return false;
    case VK::F32: case VK::F64: return v.d != 0;
    case VK::Ref: return v.o != nullptr;
    case VK::Ptr: return v.f == 2 || v.o != nullptr;
    case VK::Fn: return true;
    default: return v.i != 0;
    }
}

// ================================================================ testo dei valori
std::string Vm::cstring(const Value& p) {
    if (p.isNull()) fault(FaultKind::NullPointer, "stringa NULL");
    Value x = p;
    if (x.k == VK::Ref && x.o->kind == OK::Array) x = Value::ptr(x.o, 0);
    if (x.k == VK::Ref && x.o->kind == OK::StrB) return std::string(static_cast<StrBObj*>(x.o)->s);
    if (x.k != VK::Ptr || x.f == 2 || !x.o || x.o->kind != OK::Array) {
        if (x.k == VK::Ptr && x.o && x.o->kind == OK::StrB) return std::string(static_cast<StrBObj*>(x.o)->s.substr(x.idx == Whole ? 0 : x.idx));
        fault(FaultKind::Generic, "atteso un char* (stringa del C)");
    }
    ArrayObj* a = static_cast<ArrayObj*>(x.o);
    if (a->dead()) fault(FaultKind::DeadPointer, "stringa in memoria gia' liberata");
    if (a->ek != VK::C8 && a->ek != VK::U8 && a->ek != VK::I8) fault(FaultKind::Generic, "atteso un array di char");
    std::string s;
    const std::uint32_t start = x.idx == Whole ? 0 : x.idx;
    for (std::uint32_t i = start;; ++i) {
        if (i >= a->count) fault(FaultKind::Index, "stringa senza '\\0' finale (oltre la fine dell'array)");
        const char c = static_cast<char>(a->data[i]);
        if (c == 0) break;
        s += c;
    }
    return s;
}
std::string Vm::utf8(const Value& v) {
    if (v.isNull()) return "";
    if (v.holdsObj()) {
        Obj* o = v.o;
        if (v.k == VK::Ptr && v.f == 2) return utf8(loadPtr(v));
        switch (o->kind) {
        case OK::StrB: return std::string(static_cast<StrBObj*>(o)->s);
        case OK::StrW: return utf16To8(static_cast<StrWObj*>(o)->s);
        case OK::Sb: return utf16To8(static_cast<SbObj*>(o)->s);
        case OK::Array: return cstring(v);
        default: break;
        }
    }
    return format(v, lang() == Dialect::C ? Dialect::Cpp : lang());
}
std::u16string Vm::utf16(const Value& v) {
    if (v.holdsObj() && v.o->kind == OK::StrW) return std::u16string(static_cast<StrWObj*>(v.o)->s);
    if (v.holdsObj() && v.o->kind == OK::Sb) return std::u16string(static_cast<SbObj*>(v.o)->s);
    if (v.k == VK::C16) return std::u16string(1, static_cast<char16_t>(v.i));
    return utf8To16(utf8(v));
}
Value Vm::toLangString(const Value& v, Dialect d) {
    if ((d == Dialect::CSharp || d == Dialect::Java) && v.holdsObj() && v.o->kind == OK::StrW) return v;
    if (d == Dialect::Cpp && v.holdsObj() && v.o->kind == OK::StrB) return v;
    return newString(d, format(v, d));
}

// ================================================================ confronti e hash
int Vm::compareValues(const Value& a, const Value& b) {
    if (isNum(a.k) && isNum(b.k)) {
        if (isFloat(a.k) || isFloat(b.k)) { const double x = a.num(), y = b.num(); return x < y ? -1 : x > y ? 1 : 0; }
        if (a.k == VK::U64 || b.k == VK::U64) { const std::uint64_t x = a.u, y = b.u; return x < y ? -1 : x > y ? 1 : 0; }
        return a.i < b.i ? -1 : a.i > b.i ? 1 : 0;
    }
    if (a.isNull() || b.isNull()) return a.isNull() && b.isNull() ? 0 : a.isNull() ? -1 : 1;
    if (a.holdsObj() && b.holdsObj()) {
        Obj* x = a.o;
        Obj* y = b.o;
        if (x->kind == OK::StrB && y->kind == OK::StrB) { const int c = static_cast<StrBObj*>(x)->s.compare(static_cast<StrBObj*>(y)->s); return c < 0 ? -1 : c > 0 ? 1 : 0; }
        if ((x->kind == OK::StrW || x->kind == OK::StrB) && (y->kind == OK::StrW || y->kind == OK::StrB)) {
            const std::u16string s = utf16(a), t = utf16(b);
            if (lang() == Dialect::CSharp) {
                // String.CompareTo di .NET: confronto culturale (qui: senza maiuscole prima, poi ordinale)
                std::u16string ls = s, lt = t;
                for (auto& c : ls) if (c >= 'A' && c <= 'Z') c = static_cast<char16_t>(c + 32);
                for (auto& c : lt) if (c >= 'A' && c <= 'Z') c = static_cast<char16_t>(c + 32);
                const int c1 = ls.compare(lt);
                if (c1 != 0) return c1 < 0 ? -1 : 1;
                // a parita': la minuscola prima
                for (std::size_t i = 0; i < s.size() && i < t.size(); ++i) if (s[i] != t[i]) return (s[i] >= 'a' && s[i] <= 'z') ? -1 : 1;
                return 0;
            }
            const int c = s.compare(t);
            return c < 0 ? -1 : c > 0 ? 1 : 0;
        }
        if (x->kind == OK::Array && y->kind == OK::Array && (a.k == VK::Ptr || b.k == VK::Ptr)) {
            const std::string s = cstring(a), t = cstring(b);
            const int c = s.compare(t);
            return c < 0 ? -1 : c > 0 ? 1 : 0;
        }
        if (x->kind == OK::Inst && y->kind == OK::Inst) {
            InstObj* p = static_cast<InstObj*>(x);
            InstObj* q = static_cast<InstObj*>(y);
            if (!p->cls && !q->cls) {
                // coppie: prima il primo campo, poi il secondo
                const int c = compareValues(p->fields[0], q->fields[0]);
                return c != 0 ? c : compareValues(p->fields[1], q->fields[1]);
            }
            if (p->cls) {
                // C++: operator<; C#: CompareTo; Java: compareTo
                for (Function* m : p->cls->methods) {
                    if (m->simple == "operator<" && m->params.size() == 1) {
                        Value args1[2] = {a, b};
                        const bool lt = truthy(invokeFunction(m, args1, 2, true));
                        if (lt) return -1;
                        Value args2[2] = {b, a};
                        return truthy(invokeFunction(m, args2, 2, true)) ? 1 : 0;
                    }
                }
                auto it = p->cls->selectors.find(lang() == Dialect::Java ? "compareTo/1" : "CompareTo/1");
                if (it != p->cls->selectors.end()) {
                    Value args[2] = {a, b};
                    return static_cast<int>(invokeFunction(it->second, args, 2, true).integer());
                }
                if (p->cls->lang == Dialect::Cpp) fault(FaultKind::Generic, "la classe " + p->cls->name + " non ha operator< (serve per ordinare)");
                fault(FaultKind::InvalidOp, "la classe " + p->cls->name + " non implementa IComparable (serve per ordinare)");
            }
        }
        return x < y ? -1 : x > y ? 1 : 0;
    }
    if (a.k == VK::Bool && b.k == VK::Bool) return a.i < b.i ? -1 : a.i > b.i ? 1 : 0;
    return 0;
}
bool Vm::equalValues(const Value& a, const Value& b) {
    if (isNum(a.k) && isNum(b.k)) return compareValues(a, b) == 0 && !(isFloat(a.k) && std::isnan(a.d));
    if (a.isNull() || b.isNull()) return a.isNull() && b.isNull();
    if (a.holdsObj() && b.holdsObj()) {
        Obj* x = a.o;
        Obj* y = b.o;
        if ((x->kind == OK::StrW || x->kind == OK::StrB) && (y->kind == OK::StrW || y->kind == OK::StrB)) return utf16(a) == utf16(b);
        if (x->kind == OK::Inst && y->kind == OK::Inst) {
            InstObj* p = static_cast<InstObj*>(x);
            if (!p->cls) return compareValues(a, b) == 0;
            auto it = p->cls->selectors.find(lang() == Dialect::Java ? "equals/1" : "Equals/1");
            if (it != p->cls->selectors.end()) { Value args[2] = {a, b}; return truthy(invokeFunction(it->second, args, 2, true)); }
            for (Function* m : p->cls->methods) if (m->simple == "operator==" && m->params.size() == 1) { Value args[2] = {a, b}; return truthy(invokeFunction(m, args, 2, true)); }
            // struct di C#: uguaglianza dei campi
            if (p->cls->valueType && p->cls->lang == Dialect::CSharp) {
                InstObj* q = static_cast<InstObj*>(y);
                if (p->cls != q->cls) return false;
                for (std::uint32_t i = 0; i < p->nfields; ++i) if (!equalValues(p->fields[i], q->fields[i])) return false;
                return true;
            }
        }
        return x == y;
    }
    if (a.k == VK::Fn && b.k == VK::Fn) return a.f == b.f && (a.f == 0 ? a.i == b.i : a.o == b.o);
    return a.k == b.k && a.i == b.i;
}
std::uint32_t Vm::hashValue(const Value& v) {
    if (isNum(v.k)) {
        if (isFloat(v.k)) { double d = v.d; if (d == 0) d = 0; std::uint64_t b; std::memcpy(&b, &d, 8); return static_cast<std::uint32_t>(b ^ (b >> 32)); }
        const std::uint64_t b = static_cast<std::uint64_t>(v.i);
        return static_cast<std::uint32_t>(b ^ (b >> 32));
    }
    if (v.isNull()) return 0;
    if (v.holdsObj()) {
        Obj* o = v.o;
        if (o->kind == OK::StrW || o->kind == OK::StrB) {
            // hash di Java (s[0]*31^(n-1) + ...): stesso ordine delle HashMap di Java
            std::int32_t h = 0;
            for (char16_t c : utf16(v)) h = static_cast<std::int32_t>(31u * static_cast<std::uint32_t>(h) + c);
            return static_cast<std::uint32_t>(h);
        }
        if (o->kind == OK::Inst) {
            InstObj* p = static_cast<InstObj*>(o);
            if (!p->cls) return hashValue(p->fields[0]) * 31u + hashValue(p->fields[1]);
            auto it = p->cls->selectors.find(lang() == Dialect::Java ? "hashCode/0" : "GetHashCode/0");
            if (it != p->cls->selectors.end()) return static_cast<std::uint32_t>(invokeFunction(it->second, &v, 1, true).integer());
            if (p->cls->valueType && p->cls->lang == Dialect::CSharp) { std::uint32_t h = 17; for (std::uint32_t i = 0; i < p->nfields; ++i) h = h * 31u + hashValue(p->fields[i]); return h; }
        }
        const std::uintptr_t a = reinterpret_cast<std::uintptr_t>(o);
        return static_cast<std::uint32_t>(a >> 4) * 2654435761u;
    }
    return static_cast<std::uint32_t>(v.i);
}

// ================================================================ file
FileObj* Vm::stdStream(int which) {
    if (!stdio[which]) {
        FileObj* f = newFile();
        f->console = true;
        f->open = true;
        f->mode = which == 0 ? 1 : 2;
        f->flags |= FRoot;
        stdio[which] = f;
    }
    return stdio[which];
}

} // namespace fx::prog
