// Servizi comuni delle librerie native e voci del nucleo (v1.20.0): foreach
// e inizializzatori delle collezioni, iteratori del C++ (puntatori nelle
// sequenze, celle delle mappe), coppie, eccezioni, object di C#, buchi delle
// stringhe interpolate, oggetti standard (cout, cin, stdin...), mappe e
// insiemi, ordinamento stabile, console condivisa, file confinati nella
// cartella del programma con la scrittura atomica alla chiusura.
#include "program_lib.hpp"
#include "file_store.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace fx::prog {
using namespace lib;

// ================================================================ argomenti e oggetti
namespace lib {
std::string S(Vm& vm, const NativeCall& c, int i) {
    const Value& v = c.args[i];
    if (v.k == VK::Ptr && v.f != 2 && v.o && v.idx != Whole && v.o->kind == OK::StrB) {
        const auto& s = static_cast<StrBObj*>(v.o)->s;
        return v.idx <= s.size() ? std::string(s.substr(v.idx)) : std::string();
    }
    if (v.k == VK::Ptr && v.f == 2) {
        const Value x = vm.loadPtr(v);
        if (x.holdsObj() && (x.o->kind == OK::StrB || x.o->kind == OK::StrW)) return vm.utf8(x);
    }
    if (v.isNull()) {
        if (vm.lang() == Dialect::C || vm.lang() == Dialect::Cpp) vm.fault(FaultKind::NullPointer, "stringa NULL");
        return {};
    }
    return vm.utf8(v);
}
std::u16string W(Vm& vm, const NativeCall& c, int i) {
    const Value& v = c.args[i];
    if (v.holdsObj() && v.o->kind == OK::StrW) return std::u16string(static_cast<StrWObj*>(v.o)->s);
    if (v.isNull()) return {};
    return vm.utf16(v);
}
Obj* objArg(Vm& vm, const Value& v) {
    Value x = v;
    if (x.k == VK::Ptr && x.f == 2) x = vm.loadPtr(x);
    if (x.k == VK::Ptr && x.f == 2) x = vm.loadPtr(x);
    return vm.objOf(x);
}
namespace {
[[noreturn]] void wrongKind(Vm& vm, const char* what) { vm.fault(FaultKind::Generic, std::string("atteso un oggetto ") + what); }
} // namespace
VecObj* vecArg(Vm& vm, const Value& v) { Obj* o = objArg(vm, v); if (o->kind != OK::Vec) wrongKind(vm, "contenitore"); return static_cast<VecObj*>(o); }
MapObj* mapArg(Vm& vm, const Value& v) { Obj* o = objArg(vm, v); if (o->kind != OK::Map) wrongKind(vm, "mappa"); return static_cast<MapObj*>(o); }
StrBObj* strbArg(Vm& vm, const Value& v) { Obj* o = objArg(vm, v); if (o->kind != OK::StrB) wrongKind(vm, "std::string"); return static_cast<StrBObj*>(o); }
StrWObj* strwArg(Vm& vm, const Value& v) { Obj* o = objArg(vm, v); if (o->kind != OK::StrW) wrongKind(vm, "string"); return static_cast<StrWObj*>(o); }
SbObj* sbArg(Vm& vm, const Value& v) { Obj* o = objArg(vm, v); if (o->kind != OK::Sb) wrongKind(vm, "StringBuilder"); return static_cast<SbObj*>(o); }
FileObj* fileArg(Vm& vm, const Value& v) {
    Value x = v;
    if (x.isNull()) vm.fault(FaultKind::NullPointer, vm.lang() == Dialect::C ? "FILE* NULL (fopen non riuscita?)" : "flusso null");
    Obj* o = objArg(vm, x);
    if (o->kind != OK::File) wrongKind(vm, "file");
    return static_cast<FileObj*>(o);
}
InstObj* instArg(Vm& vm, const Value& v) { Obj* o = objArg(vm, v); if (o->kind != OK::Inst) wrongKind(vm, "istanza"); return static_cast<InstObj*>(o); }
ArrayObj* arrayArg(Vm& vm, const Value& v) { Obj* o = objArg(vm, v); if (o->kind != OK::Array) wrongKind(vm, "array"); return static_cast<ArrayObj*>(o); }
NativeObj* nativeArg(Vm& vm, const Value& v) { Obj* o = objArg(vm, v); if (o->kind != OK::Native) wrongKind(vm, "della libreria"); return static_cast<NativeObj*>(o); }

TypeP typeArgOf(TypeP t, std::size_t i) {
    if (!t) return nullptr;
    if (t->k == TK::Ref || t->k == TK::Ptr) t = t->elem;
    if (t && t->k == TK::Native && (t->nk == NK::Iter || t->nk == NK::MapIter) && !t->args.empty()) t = t->args[0];
    if (!t) return nullptr;
    if (t->k == TK::Array) return i == 0 ? t->elem : nullptr;
    if (t->k == TK::Nullable) return i == 0 ? t->elem : nullptr;
    return i < t->args.size() ? t->args[i] : nullptr;
}
TypeP containerType(const NativeCall& c) {
    TypeP t = c.type;
    if (t && (t->k == TK::Ref || t->k == TK::Ptr)) t = t->elem;
    if (t && t->k == TK::Native && (t->nk == NK::Iter || t->nk == NK::MapIter) && !t->args.empty()) t = t->args[0];
    return t;
}
TypeP typeArg(const NativeCall& c, std::size_t i) { return typeArgOf(c.type, i); }

// ================================================================ valori
Value strOf(Vm& vm, Dialect d, std::string_view s) { return vm.newString(d, s); }
Value str(Vm& vm, std::string_view s) { return vm.newString(vm.lang(), s); }
Value wstr(Vm& vm, std::u16string_view s) { return Value::ref(vm.newStrW(s)); }
Value boolOf(Dialect d, bool b) { return d == Dialect::C ? Value::i32(b ? 1 : 0) : Value::boolean(b); }
Value toType(Vm& vm, const Value& v, TypeP t) {
    if (!t) return v;
    if (t->k == TK::Ref) t = t->elem;
    if (t->k == TK::Dynamic) return v;
    if (isNumeric(t) || t->k == TK::Enum) {
        if (v.k == VK::Null && t->k != TK::Enum) return v;
        return vm.convertTo(v, vkOf(t));
    }
    if (t->k == TK::Str && t->cppValue) {
        if (v.holdsObj() && v.o->kind == OK::StrB) return v;
        if (v.holdsObj() && (v.o->kind == OK::Array || v.o->kind == OK::StrW)) return Value::ref(vm.newStrB(vm.utf8(v)));
    }
    if (t->k == TK::Str && !t->cppValue && v.holdsObj() && v.o->kind == OK::StrB) return wstr(vm, utf8To16(static_cast<StrBObj*>(v.o)->s));
    return v;
}
Value stored(Vm& vm, const Value& v, TypeP elem) {
    Value x = toType(vm, v, elem);
    if (elem && isValueTypeRT(elem) && x.holdsObj()) x = vm.copyValue(x);
    return x;
}

// ================================================================ sequenze
std::uint32_t Seq::size() const {
    switch (o->kind) {
    case OK::Vec: return static_cast<std::uint32_t>(static_cast<VecObj*>(o)->items.size());
    case OK::StrB: return static_cast<std::uint32_t>(static_cast<StrBObj*>(o)->s.size());
    case OK::StrW: return static_cast<std::uint32_t>(static_cast<StrWObj*>(o)->s.size());
    case OK::Array: return static_cast<ArrayObj*>(o)->count;
    default: return 0;
    }
}
Value Seq::get(Vm& vm, std::uint32_t i) const {
    if (i >= size()) vm.fault(FaultKind::Index, "iteratore oltre la fine (posizione " + std::to_string(i) + " su " + std::to_string(size()) + ")");
    switch (o->kind) {
    case OK::Vec: return static_cast<VecObj*>(o)->items[i];
    case OK::StrB: return Value::ofInt(VK::C8, static_cast<signed char>(static_cast<StrBObj*>(o)->s[i]));
    case OK::StrW: return Value::ofInt(VK::C16, static_cast<StrWObj*>(o)->s[i]);
    case OK::Array: return static_cast<ArrayObj*>(o)->get(i);
    default: return Value();
    }
}
void Seq::set(Vm& vm, std::uint32_t i, const Value& v) const {
    if (i >= size()) vm.fault(FaultKind::Index, "scrittura oltre la fine (posizione " + std::to_string(i) + " su " + std::to_string(size()) + ")");
    switch (o->kind) {
    case OK::Vec: static_cast<VecObj*>(o)->items[i] = v; ++static_cast<VecObj*>(o)->version; return;
    case OK::StrB: static_cast<StrBObj*>(o)->s[i] = static_cast<char>(v.integer()); return;
    case OK::Array: {
        ArrayObj* a = static_cast<ArrayObj*>(o);
        if (a->flags & FReadOnly) vm.fault(FaultKind::ReadOnly, "scrittura su una stringa letterale");
        a->set(i, v);
        return;
    }
    default: vm.fault(FaultKind::InvalidOp, "sequenza di sola lettura");
    }
}
Seq seqOf(Vm& vm, const Value& v) {
    Value x = v;
    if (x.k == VK::Ptr && x.f == 2) x = vm.loadPtr(x);
    if (x.isNull()) vm.fault(FaultKind::NullPointer, "sequenza NULL");
    if (!x.holdsObj()) vm.fault(FaultKind::Generic, "atteso un contenitore o un iteratore");
    Obj* o = x.o;
    if (x.k == VK::Ptr && x.idx != Whole && o->kind != OK::Vec && o->kind != OK::StrB && o->kind != OK::Array && o->kind != OK::StrW) o = objArg(vm, x);
    if (o->kind == OK::Box || o->kind == OK::Inst) o = objArg(vm, x);
    if (o->dead()) vm.fault(FaultKind::DeadPointer, "contenitore gia' liberato");
    Seq s;
    s.o = o;
    return s;
}
void rangeOf(Vm& vm, const Value& first, const Value& last, Seq& seq, std::uint32_t& b, std::uint32_t& e) {
    auto pos = [&](const Value& v) -> std::uint32_t {
        if (v.k == VK::Ptr && v.f != 2) return v.idx == Whole ? 0 : v.idx;
        if (v.k == VK::Ref) return 0;
        vm.fault(FaultKind::Generic, "atteso un iteratore o un puntatore");
    };
    if (first.isNull() || last.isNull()) vm.fault(FaultKind::NullPointer, "iteratore NULL");
    if (first.o != last.o) vm.fault(FaultKind::Generic, "iteratori di contenitori diversi");
    seq = seqOf(vm, first.k == VK::Ptr ? Value::ref(first.o) : first);
    b = pos(first);
    e = pos(last);
    if (b > e) vm.fault(FaultKind::Argument, "intervallo con l'inizio dopo la fine");
    if (e > seq.size()) vm.fault(FaultKind::Index, "intervallo oltre la fine del contenitore");
}

// ================================================================ mappe e insiemi
namespace {
bool descending(const MapObj* m) {
    const TypeP t = m->type;
    if (!t || t->k != TK::Native) return false;
    const std::size_t i = m->isSet ? 1 : 2;
    return t->args.size() > i && t->args[i] && t->args[i]->k == TK::Native && t->args[i]->nk == NK::Greater;
}
int cmpKeys(Vm& vm, const MapObj* m, const Value& a, const Value& b) {
    const int c = vm.compareValues(a, b);
    return descending(m) ? -c : c;
}
// mode 0: posizione in order della prima chiave >= key
std::size_t lowerPos(Vm& vm, MapObj* m, const Value& key) {
    std::size_t lo = 0, hi = m->order.size();
    while (lo < hi) {
        const std::size_t mid = (lo + hi) / 2;
        if (cmpKeys(vm, m, m->keys[m->order[mid]], key) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}
std::size_t posOfCell(Vm& vm, MapObj* m, std::uint32_t cell) {
    std::size_t p = lowerPos(vm, m, m->keys[cell]);
    if (p < m->order.size() && m->order[p] == cell) return p;
    for (std::size_t i = 0; i < m->order.size(); ++i) if (m->order[i] == cell) return i;
    return m->order.size();
}
void rehash(Vm& vm, MapObj* m, std::size_t want) {
    std::size_t n = 8;
    while (n < want) n <<= 1;
    m->buckets.assign(n, -1);
    m->next.assign(m->keys.size(), -1);
    for (std::uint32_t c = 0; c < m->keys.size(); ++c) {
        if (!m->used[c]) continue;
        const std::size_t b = vm.hashValue(m->keys[c]) & (n - 1);
        m->next[c] = m->buckets[b];
        m->buckets[b] = static_cast<std::int32_t>(c);
    }
}
} // namespace
std::int32_t mapFind(Vm& vm, MapObj* m, const Value& key) {
    if (m->count == 0) return -1;
    if (m->mode == 0) {
        const std::size_t p = lowerPos(vm, m, key);
        if (p < m->order.size() && cmpKeys(vm, m, m->keys[m->order[p]], key) == 0) return static_cast<std::int32_t>(m->order[p]);
        return -1;
    }
    if (m->buckets.empty()) rehash(vm, m, m->keys.size() * 2);
    const std::size_t b = vm.hashValue(key) & (m->buckets.size() - 1);
    for (std::int32_t c = m->buckets[b]; c >= 0; c = m->next[static_cast<std::size_t>(c)]) {
        if (m->used[static_cast<std::size_t>(c)] && vm.equalValues(m->keys[static_cast<std::size_t>(c)], key)) return c;
    }
    return -1;
}
std::uint32_t mapInsert(Vm& vm, MapObj* m, const Value& key, const Value& val, bool* inserted) {
    const std::int32_t f = mapFind(vm, m, key);
    if (f >= 0) { if (inserted) *inserted = false; return static_cast<std::uint32_t>(f); }
    std::uint32_t cell;
    if (!m->freeSlots.empty()) {
        cell = m->freeSlots.back();
        m->freeSlots.pop_back();
    } else {
        cell = static_cast<std::uint32_t>(m->keys.size());
        if (cell >= 0x7FFFFFF0u) vm.fault(FaultKind::Memory, "troppi elementi");
        m->keys.push_back(Value());
        m->vals.push_back(Value());
        m->used.push_back(0);
        m->next.push_back(-1);
    }
    m->keys[cell] = key;
    m->vals[cell] = val;
    m->used[cell] = 1;
    ++m->count;
    ++m->version;
    if (m->mode == 0) {
        const std::size_t p = lowerPos(vm, m, key);
        m->order.insert(m->order.begin() + static_cast<std::ptrdiff_t>(p), cell);
    } else {
        if (m->buckets.empty() || m->count * 4 > m->buckets.size() * 3) rehash(vm, m, m->buckets.size() * 2 + 8);
        else {
            const std::size_t b = vm.hashValue(key) & (m->buckets.size() - 1);
            m->next[cell] = m->buckets[b];
            m->buckets[b] = static_cast<std::int32_t>(cell);
        }
    }
    if (inserted) *inserted = true;
    return cell;
}
void mapEraseCell(Vm& vm, MapObj* m, std::uint32_t cell) {
    if (cell >= m->keys.size() || !m->used[cell]) return;
    if (m->mode == 0) {
        const std::size_t p = posOfCell(vm, m, cell);
        if (p < m->order.size()) m->order.erase(m->order.begin() + static_cast<std::ptrdiff_t>(p));
    } else if (!m->buckets.empty()) {
        const std::size_t b = vm.hashValue(m->keys[cell]) & (m->buckets.size() - 1);
        std::int32_t* link = &m->buckets[b];
        bool unlinked = false;
        while (*link >= 0) {
            if (static_cast<std::uint32_t>(*link) == cell) { *link = m->next[cell]; unlinked = true; break; }
            link = &m->next[static_cast<std::size_t>(*link)];
        }
        if (!unlinked) {
            // hash cambiato (oggetto modificato): ricostruzione
            m->used[cell] = 0;
            rehash(vm, m, m->buckets.size());
            m->used[cell] = 1;
        }
    }
    m->used[cell] = 0;
    m->keys[cell] = Value();
    m->vals[cell] = Value();
    m->next[cell] = -1;
    m->freeSlots.push_back(cell);
    --m->count;
    ++m->version;
}
bool mapErase(Vm& vm, MapObj* m, const Value& key) {
    const std::int32_t c = mapFind(vm, m, key);
    if (c < 0) return false;
    mapEraseCell(vm, m, static_cast<std::uint32_t>(c));
    return true;
}
void mapClear(MapObj* m) {
    m->keys.clear(); m->vals.clear(); m->used.clear(); m->order.clear(); m->buckets.clear(); m->next.clear(); m->freeSlots.clear();
    m->count = 0;
    ++m->version;
}
std::uint32_t mapCellAt(MapObj* m, std::uint32_t i) {
    if (m->mode == 0) return i < m->order.size() ? m->order[i] : End;
    if (m->count == m->keys.size()) return i < m->keys.size() ? i : End;
    std::uint32_t k = 0;
    for (std::uint32_t c = 0; c < m->keys.size(); ++c) if (m->used[c]) { if (k == i) return c; ++k; }
    return End;
}
std::uint32_t mapBegin(MapObj* m) { return m->count == 0 ? End : mapCellAt(m, 0); }
std::uint32_t mapNext(Vm& vm, MapObj* m, std::uint32_t cell) {
    if (cell == End) vm.fault(FaultKind::Index, "++ oltre end() della mappa");
    if (cell >= m->keys.size() || !m->used[cell]) vm.fault(FaultKind::DeadPointer, "iteratore di un elemento rimosso dalla mappa");
    if (m->mode == 0) {
        const std::size_t p = posOfCell(vm, m, cell);
        return p + 1 < m->order.size() ? m->order[p + 1] : End;
    }
    for (std::uint32_t c = cell + 1; c < m->keys.size(); ++c) if (m->used[c]) return c;
    return End;
}
std::uint32_t mapPrev(Vm& vm, MapObj* m, std::uint32_t cell) {
    if (m->count == 0) vm.fault(FaultKind::Index, "-- su una mappa vuota");
    if (m->mode == 0) {
        if (cell == End) return m->order.back();
        const std::size_t p = posOfCell(vm, m, cell);
        if (p == 0 || p >= m->order.size()) vm.fault(FaultKind::Index, "-- prima di begin() della mappa");
        return m->order[p - 1];
    }
    std::uint32_t c = cell == End ? static_cast<std::uint32_t>(m->keys.size()) : cell;
    while (c > 0) { --c; if (m->used[c]) return c; }
    vm.fault(FaultKind::Index, "-- prima di begin() della mappa");
}
std::uint32_t mapBound(Vm& vm, MapObj* m, const Value& key, bool upper) {
    if (m->mode != 0) vm.fault(FaultKind::InvalidOp, "lower_bound/upper_bound vogliono una mappa ordinata");
    std::size_t p = lowerPos(vm, m, key);
    if (upper && p < m->order.size() && cmpKeys(vm, m, m->keys[m->order[p]], key) == 0) ++p;
    return p < m->order.size() ? m->order[p] : End;
}
std::vector<std::uint32_t> mapCells(MapObj* m) {
    std::vector<std::uint32_t> out;
    out.reserve(m->count);
    if (m->mode == 0) { out.assign(m->order.begin(), m->order.end()); return out; }
    for (std::uint32_t c = 0; c < m->keys.size(); ++c) if (m->used[c]) out.push_back(c);
    return out;
}

// ================================================================ ordinamento
void stableSort(Vm& vm, std::vector<Value>& items, const std::function<bool(const Value&, const Value&)>& less) {
    const std::size_t n = items.size();
    if (n < 2) return;
    // radice: i valori restano raggiungibili anche se il confronto cambia il contenitore
    VecObj* keep = vm.newVec(nullptr);
    vm.root(keep);
    keep->items.assign(items.begin(), items.end());
    std::vector<Value> tmp(n);
    std::vector<Value>* src = &items;
    std::vector<Value>* dst = &tmp;
    for (std::size_t width = 1; width < n; width *= 2) {
        for (std::size_t lo = 0; lo < n; lo += 2 * width) {
            const std::size_t mid = std::min(lo + width, n), hi = std::min(lo + 2 * width, n);
            std::size_t i = lo, j = mid, k = lo;
            while (i < mid && j < hi) {
                if (less((*src)[j], (*src)[i])) (*dst)[k++] = (*src)[j++];
                else (*dst)[k++] = (*src)[i++];
            }
            while (i < mid) (*dst)[k++] = (*src)[i++];
            while (j < hi) (*dst)[k++] = (*src)[j++];
        }
        std::swap(src, dst);
        vm.poll();
    }
    if (src != &items) items = *src;
}
bool lessWith(Vm& vm, const Value* cmp, const Value& a, const Value& b, bool intComparator) {
    if (!cmp || cmp->isNull() || cmp->k == VK::Void) return vm.compareValues(a, b) < 0;
    if (cmp->k == VK::Fn) {
        Value args[2] = {a, b};
        const Value r = vm.invoke(*cmp, args, 2);
        if (intComparator) return r.integer() < 0;
        return vm.truthy(r);
    }
    if (cmp->holdsObj() && cmp->o->kind == OK::Native) {
        const NativeObj* f = static_cast<NativeObj*>(cmp->o);
        if (f->nk == NK::Greater) return vm.compareValues(a, b) > 0;
        if (f->nk == NK::Less) return vm.compareValues(a, b) < 0;
    }
    vm.fault(FaultKind::Argument, "comparatore non valido");
}

// ================================================================ console
int conPeek(Vm& vm) {
    while (vm.inputPos >= vm.inputBuffer.size()) {
        if (vm.inputEof) return -1;
        std::string line;
        if (!vm.readLine(line)) return -1;
        vm.inputBuffer = line;
        vm.inputBuffer += '\n';
        vm.inputPos = 0;
    }
    return static_cast<unsigned char>(vm.inputBuffer[vm.inputPos]);
}
int conGet(Vm& vm) {
    const int c = conPeek(vm);
    if (c >= 0) ++vm.inputPos;
    return c;
}
void conUnget(Vm& vm) { if (vm.inputPos > 0) --vm.inputPos; }
bool conLine(Vm& vm, std::string& out) {
    out.clear();
    int c = conGet(vm);
    if (c < 0) return false;
    while (c >= 0 && c != '\n') { out += static_cast<char>(c); c = conGet(vm); }
    if (!out.empty() && out.back() == '\r') out.pop_back();
    return true;
}
bool conToken(Vm& vm, std::string& out) {
    out.clear();
    int c = conPeek(vm);
    while (c >= 0 && std::isspace(c)) { conGet(vm); c = conPeek(vm); }
    if (c < 0) return false;
    while (c >= 0 && !std::isspace(c)) { out += static_cast<char>(c); conGet(vm); c = vm.inputPos < vm.inputBuffer.size() ? static_cast<unsigned char>(vm.inputBuffer[vm.inputPos]) : -1; }
    return true;
}
void conOut(Vm& vm, int which, const std::string& text) {
    (void)which;
    if (!text.empty()) vm.host().print(text);
}

// ================================================================ file
std::string confine(Vm& vm, const std::string& pathIn, std::string& why) {
    std::string base = vm.host().programFolder();
    const std::int32_t lib = vm.library();
    if (lib >= 0 && static_cast<std::size_t>(lib) < vm.program().libraries.size()) base = vm.program().libraries[static_cast<std::size_t>(lib)].dataFolder;
    if (base.empty()) { why = "nessuna cartella del programma: file non disponibili"; return {}; }
    std::string p = pathIn;
    for (char& ch : p) if (ch == '\\') ch = '/';
    if (p.empty()) { why = "nome del file vuoto"; return {}; }
    if (p[0] == '/' || p.find(':') != std::string::npos) { why = "percorso assoluto non ammesso (solo file nella cartella del programma)"; return {}; }
    std::vector<std::string> parts;
    std::size_t b = 0;
    while (b <= p.size()) {
        std::size_t e = p.find('/', b);
        if (e == std::string::npos) e = p.size();
        const std::string seg = p.substr(b, e - b);
        if (seg == "..") { why = "'..' non ammesso: solo file nella cartella del programma"; return {}; }
        if (!seg.empty() && seg != ".") {
            for (char ch : seg) if (static_cast<unsigned char>(ch) < 32 || std::strchr("*?\"<>|", ch)) { why = "carattere non valido nel nome del file"; return {}; }
            parts.push_back(seg);
        }
        b = e + 1;
    }
    if (parts.empty()) { why = "nome del file vuoto"; return {}; }
    std::string out = base;
    for (const std::string& s : parts) out += "/" + s;
    if (out.size() > 240) { why = "percorso troppo lungo"; return {}; }
    return out;
}
namespace {
files::FileSystem* fsFor(Vm& vm, std::string& why) {
    files::FileSystem* fs = vm.host().files();
    if (!fs) { why = "Inserire la scheda SD"; return nullptr; }
    const files::Status st = fs->status();
    if (st == files::Status::NoCard) { why = "Inserire la scheda SD"; return nullptr; }
    if (st != files::Status::Ok) { why = files::statusMessage(st); return nullptr; }
    return fs;
}
std::string statusWhy(files::Status st) {
    switch (st) {
    case files::Status::NoCard: return "Inserire la scheda SD";
    case files::Status::Full: return "SD piena";
    case files::Status::NotFound: return "file non trovato";
    default: return files::statusMessage(st);
    }
}
bool readWhole(Vm& vm, files::FileSystem& fs, const std::string& path, std::pmr::string& out, std::string& why) {
    std::unique_ptr<files::Reader> r;
    const files::Status st = fs.openRead(path, r);
    if (st != files::Status::Ok) { why = statusWhy(st); return false; }
    char chunk[512];
    for (;;) {
        std::size_t got = 0;
        const files::Status rs = r->read(chunk, sizeof chunk, got);
        if (rs != files::Status::Ok) { why = statusWhy(rs); return false; }
        if (got == 0) break;
        out.append(chunk, got);
        vm.poll();
    }
    return true;
}
void track(Vm& vm, FileObj* f) {
    if (std::find(vm.openFiles.begin(), vm.openFiles.end(), f) == vm.openFiles.end()) vm.openFiles.push_back(f);
}
void untrack(Vm& vm, FileObj* f) {
    vm.openFiles.erase(std::remove(vm.openFiles.begin(), vm.openFiles.end(), f), vm.openFiles.end());
}
bool refill(Vm& vm, FileObj* f) {
    if (!f->reader || f->readerEnd) return false;
    char chunk[512];
    std::size_t got = 0;
    const files::Status st = f->reader->read(chunk, sizeof chunk, got);
    if (st != files::Status::Ok) { f->error = true; f->readerEnd = true; return false; }
    if (got == 0) { f->readerEnd = true; return false; }
    f->buf.assign(chunk, got);
    f->pos = 0;
    f->filePos += got;
    (void)vm;
    return true;
}
} // namespace
bool fileOpen(Vm& vm, FileObj* f, const std::string& path, const std::string& mode, std::string& why) {
    files::FileSystem* fs = fsFor(vm, why);
    if (!fs) return false;
    const std::string full = confine(vm, path, why);
    if (full.empty()) return false;
    if (f->open) fileClose(vm, f);
    const bool plus = mode.find('+') != std::string::npos;
    const char m0 = mode.empty() ? 'r' : mode[0];
    f->buf.clear();
    f->pos = 0;
    f->filePos = 0;
    f->eof = f->fail = f->error = f->dirty = false;
    f->reader.reset();
    f->readerEnd = false;
    f->unget = -1;
    f->path = full;
    f->library = vm.library();
    if (m0 == 'r' && !plus) {
        const files::Status st = fs->openRead(full, f->reader);
        if (st != files::Status::Ok) { why = statusWhy(st); return false; }
        f->mode = 1;
    } else if (m0 == 'w') {
        // la cartella deve esistere: si crea quella del file
        const std::size_t slash = full.rfind('/');
        if (slash != std::string::npos) fs->makeDirs(full.substr(0, slash));
        f->mode = plus ? 8 : 2;
        f->dirty = true;                                       // "w" crea il file anche vuoto
    } else if (m0 == 'a' || (m0 == 'r' && plus)) {
        files::Entry e;
        const bool exists = fs->stat(full, e) == files::Status::Ok;
        if (!exists && m0 == 'r') { why = "file non trovato"; return false; }
        if (exists && !readWhole(vm, *fs, full, f->buf, why)) return false;
        f->mode = m0 == 'a' ? (plus ? 8 | 4 : 4) : 8;
        if (m0 == 'a') { f->pos = f->buf.size(); f->dirty = !exists; }
    } else { why = "modo di apertura non valido: " + mode; return false; }
    f->open = true;
    track(vm, f);
    return true;
}
bool fileClose(Vm& vm, FileObj* f) {
    if (!f->open || f->console || f->memory) { f->open = f->memory ? f->open : false; return true; }
    bool ok = !f->error;
    if ((f->mode & (2 | 4 | 8)) && f->dirty) {
        std::string why;
        files::FileSystem* fs = fsFor(vm, why);
        if (!fs) ok = false;
        else {
            const files::Status st = files::writeAtomic(*fs, f->path, reinterpret_cast<const std::uint8_t*>(f->buf.data()), f->buf.size());
            if (st != files::Status::Ok) ok = false;
        }
    }
    f->reader.reset();
    f->open = false;
    f->dirty = false;
    f->buf.clear();
    f->buf.shrink_to_fit();
    untrack(vm, f);
    if (!ok) f->error = true;
    return ok;
}
int filePeek(Vm& vm, FileObj* f) {
    if (f->console) return conPeek(vm);
    if (f->unget >= 0) return f->unget;
    if (!f->open) return -1;
    if (!(f->mode & (1 | 8)) && !f->memory) return -1;
    while (f->pos >= f->buf.size()) {
        if (f->memory || !f->reader) return -1;
        if (!refill(vm, f)) return -1;
    }
    return static_cast<unsigned char>(f->buf[f->pos]);
}
int fileGet(Vm& vm, FileObj* f) {
    if (f->console) { const int c = conGet(vm); if (c < 0) f->eof = true; return c; }
    if (f->unget >= 0) { const int c = f->unget; f->unget = -1; return c; }
    const int c = filePeek(vm, f);
    if (c < 0) { f->eof = true; return -1; }
    ++f->pos;
    return c;
}
void fileUnget(Vm& vm, FileObj* f, int c) {
    if (c < 0) return;
    if (f->console) { conUnget(vm); return; }
    if (f->pos > 0 && f->pos <= f->buf.size() && static_cast<unsigned char>(f->buf[f->pos - 1]) == c) --f->pos;
    else f->unget = c;
    f->eof = false;
}
bool fileLine(Vm& vm, FileObj* f, std::string& out, char delim) {
    out.clear();
    int c = fileGet(vm, f);
    if (c < 0) return false;
    while (c >= 0 && c != static_cast<unsigned char>(delim)) {
        out += static_cast<char>(c);
        if ((out.size() & 1023) == 0) vm.poll();
        c = fileGet(vm, f);
    }
    if (c >= 0) f->eof = false;
    if (delim == '\n' && !out.empty() && out.back() == '\r') out.pop_back();
    return true;
}
bool fileToken(Vm& vm, FileObj* f, std::string& out) {
    out.clear();
    int c = filePeek(vm, f);
    while (c >= 0 && std::isspace(c)) { fileGet(vm, f); c = filePeek(vm, f); }
    if (c < 0) { f->eof = true; return false; }
    while (c >= 0 && !std::isspace(c)) { out += static_cast<char>(c); fileGet(vm, f); c = filePeek(vm, f); }
    if (c < 0) f->eof = true;
    return true;
}
void filePut(Vm& vm, FileObj* f, std::string_view text) {
    if (f->console) { conOut(vm, f == vm.stdio[2] ? 2 : 1, std::string(text)); return; }
    if (!f->open) { f->fail = true; return; }
    if (!(f->mode & (2 | 4 | 8)) && !f->memory) { f->fail = true; return; }
    f->buf.append(text.data(), text.size());
    f->dirty = true;
}
bool fileExists(Vm& vm, const std::string& path) {
    std::string why;
    files::FileSystem* fs = fsFor(vm, why);
    if (!fs) return false;
    const std::string full = confine(vm, path, why);
    if (full.empty()) return false;
    files::Entry e;
    return fs->stat(full, e) == files::Status::Ok && !e.directory;
}
bool fileRemove(Vm& vm, const std::string& path, std::string& why) {
    files::FileSystem* fs = fsFor(vm, why);
    if (!fs) return false;
    const std::string full = confine(vm, path, why);
    if (full.empty()) return false;
    const files::Status st = fs->remove(full);
    if (st != files::Status::Ok) { why = statusWhy(st); return false; }
    return true;
}
bool fileRename(Vm& vm, const std::string& from, const std::string& to, std::string& why) {
    files::FileSystem* fs = fsFor(vm, why);
    if (!fs) return false;
    const std::string a = confine(vm, from, why);
    if (a.empty()) return false;
    const std::string b = confine(vm, to, why);
    if (b.empty()) return false;
    const files::Status st = fs->rename(a, b);
    if (st != files::Status::Ok) { why = st == files::Status::Exists ? "il file di destinazione esiste gia'" : statusWhy(st); return false; }
    return true;
}
bool fileReadAll(Vm& vm, const std::string& path, std::string& out, std::string& why) {
    files::FileSystem* fs = fsFor(vm, why);
    if (!fs) return false;
    const std::string full = confine(vm, path, why);
    if (full.empty()) return false;
    std::pmr::string buf(vm.heap().resource());
    if (!readWhole(vm, *fs, full, buf, why)) return false;
    out.assign(buf.data(), buf.size());
    return true;
}
bool fileWriteAll(Vm& vm, const std::string& path, const std::string& text, bool append, std::string& why) {
    files::FileSystem* fs = fsFor(vm, why);
    if (!fs) return false;
    const std::string full = confine(vm, path, why);
    if (full.empty()) return false;
    std::pmr::string buf(vm.heap().resource());
    if (append) {
        files::Entry e;
        if (fs->stat(full, e) == files::Status::Ok && !readWhole(vm, *fs, full, buf, why)) return false;
    }
    buf.append(text.data(), text.size());
    const std::size_t slash = full.rfind('/');
    if (slash != std::string::npos) fs->makeDirs(full.substr(0, slash));
    const files::Status st = files::writeAtomic(*fs, full, reinterpret_cast<const std::uint8_t*>(buf.data()), buf.size());
    if (st != files::Status::Ok) { why = statusWhy(st); return false; }
    return true;
}

// ================================================================ numeri
std::size_t parseInt(const std::string& s, std::size_t pos, int base, std::int64_t& out, bool& overflow) {
    out = 0;
    overflow = false;
    std::size_t i = pos;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    bool neg = false;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) { neg = s[i] == '-'; ++i; }
    if ((base == 0 || base == 16) && i + 1 < s.size() && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X') && i + 2 < s.size() && std::isxdigit(static_cast<unsigned char>(s[i + 2]))) { i += 2; base = 16; }
    else if (base == 0 && i < s.size() && s[i] == '0') base = 8;
    else if (base == 0) base = 10;
    const std::size_t start = i;
    std::uint64_t v = 0;
    while (i < s.size()) {
        const char ch = s[i];
        int d = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'z' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'Z' ? ch - 'A' + 10 : 99;
        if (d >= base) break;
        if (v > (UINT64_MAX - static_cast<std::uint64_t>(d)) / static_cast<std::uint64_t>(base)) overflow = true;
        else v = v * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(d);
        ++i;
    }
    if (i == start) { out = 0; return base == 8 && start > pos && s[start - 1] == '0' ? start - pos : 0; }
    if (!neg && v > static_cast<std::uint64_t>(INT64_MAX)) overflow = true;
    if (neg && v > static_cast<std::uint64_t>(INT64_MAX) + 1u) overflow = true;
    out = neg ? static_cast<std::int64_t>(0 - v) : static_cast<std::int64_t>(v);
    return i - pos;
}
std::size_t parseDouble(const std::string& s, std::size_t pos, double& out) {
    out = 0;
    if (pos >= s.size()) return 0;
    const char* b = s.c_str() + pos;
    char* e = nullptr;
    errno = 0;
    const double d = std::strtod(b, &e);
    if (e == b) return 0;
    out = d;
    return static_cast<std::size_t>(e - b);
}
} // namespace lib

// ================================================================ Vm: file aperti
void Vm::closeAllFiles(bool commit) {
    std::vector<FileObj*> list = openFiles;
    for (FileObj* f : list) {
        if (commit) {
            try { lib::fileClose(*this, f); } catch (...) {}
        } else {
            f->reader.reset();
            f->open = false;
            f->dirty = false;
        }
    }
    openFiles.clear();
}

// ================================================================ voci del nucleo
namespace {
using lib::I;
// ---- foreach
void iterSize(Vm& vm, NativeCall& c) {
    Obj* o = objArg(vm, c.args[0]);
    std::uint64_t n = 0;
    switch (o->kind) {
    case OK::Vec: n = static_cast<VecObj*>(o)->items.size(); break;
    case OK::StrB: n = static_cast<StrBObj*>(o)->s.size(); break;
    case OK::StrW: n = static_cast<StrWObj*>(o)->s.size(); break;
    case OK::Sb: n = static_cast<SbObj*>(o)->s.size(); break;
    case OK::Map: n = static_cast<MapObj*>(o)->count; break;
    case OK::Array: n = static_cast<ArrayObj*>(o)->count; break;
    default: vm.fault(FaultKind::Generic, "foreach su un valore che non e' una collezione");
    }
    c.ret = Value::i64(static_cast<std::int64_t>(n));
}
void iterAt(Vm& vm, NativeCall& c) {
    Obj* o = objArg(vm, c.args[0]);
    const std::int64_t i = I(c, 1);
    if (o->kind == OK::Map) {
        MapObj* m = static_cast<MapObj*>(o);
        const std::uint32_t cell = mapCellAt(m, static_cast<std::uint32_t>(i));
        if (cell == End) vm.fault(FaultKind::InvalidOp, "la collezione e' cambiata durante il foreach");
        if (m->isSet) { c.ret = m->keys[cell]; return; }
        const NK nk = m->type ? m->type->nk : NK::Map;
        if (nk == NK::Map || nk == NK::UMap) { c.ret = Value::ptr(m, cell); return; }
        Value k = m->keys[cell], v = m->vals[cell];
        c.ret = Value::ref(vm.newPair(nullptr, k, v));
        return;
    }
    if (o->kind == OK::Sb) {
        const auto& s = static_cast<SbObj*>(o)->s;
        if (i < 0 || static_cast<std::size_t>(i) >= s.size()) vm.fault(FaultKind::InvalidOp, "la collezione e' cambiata durante il foreach");
        c.ret = Value::ofInt(VK::C16, s[static_cast<std::size_t>(i)]);
        return;
    }
    Seq s;
    s.o = o;
    if (i < 0 || static_cast<std::uint64_t>(i) >= s.size()) vm.fault(FaultKind::InvalidOp, "la collezione e' cambiata durante il foreach");
    c.ret = s.get(vm, static_cast<std::uint32_t>(i));
}
void iterRef(Vm& vm, NativeCall& c) {
    Obj* o = objArg(vm, c.args[0]);
    const std::int64_t i = I(c, 1);
    if (o->kind == OK::Vec || o->kind == OK::StrB || o->kind == OK::Array) {
        Seq s;
        s.o = o;
        if (i < 0 || static_cast<std::uint64_t>(i) >= s.size()) vm.fault(FaultKind::InvalidOp, "il contenitore e' cambiato durante il ciclo");
        c.ret = Value::ptr(o, static_cast<std::uint32_t>(i));
        return;
    }
    iterAt(vm, c);
}
// ---- inizializzatori delle collezioni
void initPut(Vm& vm, NativeCall& c) {
    MapObj* m = mapArg(vm, c.args[0]);
    const TypeP kt = typeArgOf(m->type, 0), vt = typeArgOf(m->type, 1);
    const Value k = stored(vm, c.args[1], kt);
    vm.root(k.holdsObj() ? k.o : nullptr);
    const Value v = stored(vm, c.args[2], vt);
    vm.root(v.holdsObj() ? v.o : nullptr);
    bool inserted = false;
    const std::uint32_t cell = mapInsert(vm, m, k, v, &inserted);
    if (!inserted && (vm.lang() == Dialect::CSharp)) vm.raise("ArgumentException", "elemento con la stessa chiave gia' presente: " + vm.format(k, Dialect::CSharp));
    if (!inserted && vm.lang() == Dialect::Java) m->vals[cell] = v;
}
void initAdd(Vm& vm, NativeCall& c) {
    Obj* o = objArg(vm, c.args[0]);
    if (o->kind == OK::Vec) {
        VecObj* v = static_cast<VecObj*>(o);
        v->items.push_back(stored(vm, c.args[1], typeArgOf(v->type, 0)));
        ++v->version;
        return;
    }
    if (o->kind == OK::Map) {
        MapObj* m = static_cast<MapObj*>(o);
        const Value k = stored(vm, c.args[1], typeArgOf(m->type, 0));
        mapInsert(vm, m, k, Value(), nullptr);
        return;
    }
    vm.fault(FaultKind::Generic, "inizializzatore di una collezione non valido");
}
// ---- object (C#)
void objIs(Vm& vm, NativeCall& c) {
    const Value& v = c.args[0];
    const std::int64_t ti = I(c, 1);
    TypeP t = ti >= 0 && static_cast<std::size_t>(ti) < vm.program().types.size() ? vm.program().typeAt(static_cast<std::int32_t>(ti)) : nullptr;
    bool r = false;
    if (t) {
        if (t->k == TK::Nullable && t->elem) t = t->elem;          // x is int? / x as int?
        if (t->k == TK::Str) r = v.holdsObj() && (v.o->kind == OK::StrW || v.o->kind == OK::StrB);
        else if (t->k == TK::Enum) r = v.k == VK::I32 && v.f == 0x45 && t->cls && v.aux == static_cast<std::uint32_t>(t->cls->id);
        else if (isNumeric(t) || t->k == TK::Bool) r = v.k == vkOf(t) && v.f != 0x45;
        else if (t->k == TK::Array) r = v.holdsObj() && v.o->kind == OK::Array && (!t->elem || static_cast<const ArrayObj*>(v.o)->elem == t->elem);
        else if (t->k == TK::Class && t->cls) r = v.holdsObj() && v.o->kind == OK::Inst && static_cast<const InstObj*>(v.o)->cls && static_cast<const InstObj*>(v.o)->cls->derivesFrom(t->cls);
        else if (t->k == TK::Native) {
            if (v.holdsObj()) {
                switch (t->nk) {
                case NK::CsList: case NK::CsQueue: case NK::CsStack: r = v.o->kind == OK::Vec; break;
                case NK::CsDict: r = v.o->kind == OK::Map && !static_cast<const MapObj*>(v.o)->isSet; break;
                case NK::CsHashSet: r = v.o->kind == OK::Map && static_cast<const MapObj*>(v.o)->isSet; break;
                case NK::CsStringBuilder: r = v.o->kind == OK::Sb; break;
                default: r = v.o->kind == OK::Native && static_cast<const NativeObj*>(v.o)->nk == t->nk; break;
                }
            }
        } else if (t->k == TK::Dynamic) r = !v.isNull();
    }
    c.ret = Value::boolean(r);
}
void objToString(Vm& vm, NativeCall& c) { c.ret = vm.toLangString(c.args[0], vm.lang()); }
void objToStringFmt(Vm& vm, NativeCall& c) { c.ret = str(vm, formatCsSpec(vm, c.args[0], S(vm, c, 1))); }
// C#: un solo System.Type per nome completo, cosi' GetType() == typeof(T) confronta i riferimenti
Value typeObject(Vm& vm, const std::string& name, const std::string& full) {
    for (const auto& t : vm.typeCache) if (t.first == full) return Value::ref(t.second);
    ClassInfo* tc = vm.program().typeClass;
    if (!tc) vm.fault(FaultKind::NotSupported, "System.Type non disponibile");
    InstObj* t = vm.newInstance(tc, false);
    vm.typeCache.push_back({full, t});
    t->fields[0] = str(vm, name);
    t->fields[1] = str(vm, full);
    return Value::ref(t);
}
// C#: GetType() -> System.Type con Name e FullName (come .NET per i tipi del sottoinsieme)
void objGetType(Vm& vm, NativeCall& c) {
    const Value& v = c.args[0];
    std::string name, ns = "System";
    switch (v.k) {
    case VK::Null: case VK::Void: vm.fault(FaultKind::NullPointer, "GetType() su null");
    case VK::Bool: name = "Boolean"; break;
    case VK::C8: case VK::C16: name = "Char"; break;
    case VK::I8: name = "SByte"; break;
    case VK::U8: name = "Byte"; break;
    case VK::I16: name = "Int16"; break;
    case VK::U16: name = "UInt16"; break;
    case VK::I32:
        if (v.f == 0x45 && v.aux < vm.program().classes.size()) {
            const ClassInfo* e = vm.program().classes[v.aux].get();
            name = e->name;
            ns.clear();
            const std::size_t sep = e->qualified.rfind("::");
            if (sep != std::string::npos) ns = e->qualified.substr(0, sep);
        } else name = "Int32";
        break;
    case VK::U32: name = "UInt32"; break;
    case VK::I64: name = "Int64"; break;
    case VK::U64: name = "UInt64"; break;
    case VK::F32: name = "Single"; break;
    case VK::F64: name = "Double"; break;
    case VK::Fn: name = "Delegate"; break;
    default: {
        const Obj* o = v.o;
        if (!o) vm.fault(FaultKind::NullPointer, "GetType() su null");
        switch (o->kind) {
        case OK::StrW: case OK::StrB: name = "String"; break;
        case OK::Sb: name = "StringBuilder"; ns = "System.Text"; break;
        case OK::Array: {
            const ArrayObj* a = static_cast<const ArrayObj*>(o);
            const TypeRef* et = a->elem;
            name = !et ? "Object" : et->k == TK::I32 ? "Int32" : et->k == TK::F64 ? "Double" : et->k == TK::Str ? "String" :
                   et->k == TK::Bool ? "Boolean" : et->k == TK::C16 ? "Char" : et->k == TK::I64 ? "Int64" : et->name();
            name += a->rank == 2 ? "[,]" : "[]";
            break;
        }
        case OK::Vec: name = "List`1"; ns = "System.Collections.Generic"; break;
        case OK::Map: name = static_cast<const MapObj*>(o)->isSet ? "HashSet`1" : "Dictionary`2"; ns = "System.Collections.Generic"; break;
        case OK::Inst: {
            const InstObj* x = static_cast<const InstObj*>(o);
            if (!x->cls) { name = "KeyValuePair`2"; ns = "System.Collections.Generic"; break; }
            name = x->cls->name;
            const std::size_t lt = name.find('<');
            if (lt != std::string::npos) {
                // Pila<int> -> Pila`1
                int n = 1, depth = 0;
                for (std::size_t i = lt + 1; i < name.size(); ++i) { if (name[i] == '<') ++depth; else if (name[i] == '>') --depth; else if (name[i] == ',' && depth == 0) ++n; }
                name = name.substr(0, lt) + "`" + std::to_string(n);
            }
            ns.clear();
            std::string q = x->cls->qualified;
            const std::size_t sep = q.rfind("::", q.find('<'));
            if (sep != std::string::npos) ns = q.substr(0, sep);
            break;
        }
        default: name = "Object"; break;
        }
    }
    }
    for (std::size_t p = ns.find("::"); p != std::string::npos; p = ns.find("::")) ns.replace(p, 2, ".");
    c.ret = typeObject(vm, name, ns.empty() ? name : ns + "." + name);
}
void typeMake(Vm& vm, NativeCall& c) { c.ret = typeObject(vm, S(vm, c, 0), S(vm, c, 1)); }
void objEquals(Vm& vm, NativeCall& c) { c.ret = Value::boolean(vm.equalValues(c.args[0], c.args[1])); }
void objHash(Vm& vm, NativeCall& c) { c.ret = Value::i32(static_cast<std::int32_t>(vm.hashValue(c.args[0]))); }
void objCompareTo(Vm& vm, NativeCall& c) {
    if (c.args[1].isNull()) { c.ret = Value::i32(1); return; }
    c.ret = Value::i32(vm.compareValues(c.args[0], c.args[1]));
}
// ---- stringhe interpolate
void formatHole(Vm& vm, NativeCall& c) {
    const std::string spec = S(vm, c, 1);
    std::string text = spec.empty() ? vm.format(c.args[0], vm.lang()) : formatCsSpec(vm, c.args[0], spec);
    const std::int64_t align = I(c, 2);
    const std::size_t w = static_cast<std::size_t>(align < 0 ? -align : align);
    std::u16string t16 = utf8To16(text);
    if (t16.size() < w) {
        const std::u16string pad(w - t16.size(), u' ');
        t16 = align < 0 ? t16 + pad : pad + t16;
    }
    c.ret = wstr(vm, t16);
}
// ---- nullable
void nullableHas(Vm&, NativeCall& c) { c.ret = Value::boolean(!c.args[0].isNull()); }
void nullableOrDefault(Vm& vm, NativeCall& c) {
    if (!c.args[0].isNull()) { c.ret = c.args[0]; return; }
    c.ret = c.argc > 1 ? c.args[1] : vm.defaultValue(typeArg(c, 0));
}
// ---- oggetti standard
Value manip(int kind, std::int64_t param = 0) {
    Value v;
    v.k = VK::I64;
    v.f = 0x4D;
    v.aux = static_cast<std::uint16_t>(kind);
    v.i = param;
    return v;
}
void stdCout(Vm& vm, NativeCall& c) { c.ret = Value::ref(vm.stdStream(1)); }
void stdCerr(Vm& vm, NativeCall& c) { c.ret = Value::ref(vm.stdStream(2)); }
void stdCin(Vm& vm, NativeCall& c) { c.ret = Value::ref(vm.stdStream(0)); }
template <int K> void stdManip(Vm&, NativeCall& c) { c.ret = manip(K); }
void stdStdio(Vm& vm, NativeCall& c) {
    const std::int64_t w = I(c, 0);
    c.ret = Value::ptr(vm.stdStream(static_cast<int>(w < 0 || w > 2 ? 1 : w)), Whole);
}
// ---- eccezioni
void excWhat(Vm& vm, NativeCall& c) {
    InstObj* e = instArg(vm, c.args[0]);
    const Value m = e->nfields > 0 ? e->fields[0] : Value();
    if (m.holdsObj() && m.o->kind == OK::StrB) { c.ret = Value::ptr(m.o, 0); return; }
    c.ret = Value::ptr(vm.newCString(e->cls ? e->cls->name : "std::exception", true), 0);
}
void excMessage(Vm& vm, NativeCall& c) {
    InstObj* e = instArg(vm, c.args[0]);
    Value m = e->nfields > 0 ? e->fields[0] : Value();
    if (m.isNull()) {
        // messaggio predefinito di .NET
        const std::string n = e->cls ? e->cls->name : "Exception";
        m = str(vm, "Exception of type 'System." + n + "' was thrown.");
    }
    c.ret = m;
}
// ArgumentException e figlie: (0) messaggio[, parametro]; (1, 2) parametro[, messaggio]
void excArgInit(Vm& vm, NativeCall& c) {
    InstObj* e = instArg(vm, c.args[0]);
    const std::int64_t kind = I(c, 1);
    const bool hasB = !c.args[3].isNull();
    const std::string a = c.args[2].isNull() ? std::string() : S(vm, c, 2);
    const std::string b = hasB ? S(vm, c, 3) : std::string();
    std::string param = kind == 0 ? b : a;
    std::string msg = kind == 0 ? a : hasB ? b : kind == 1 ? "Value cannot be null." : "Specified argument was out of the range of valid values.";
    if (!param.empty()) msg += " (Parameter '" + param + "')";
    if (e->nfields > 0) e->fields[0] = str(vm, msg);
    if (e->nfields > 3) e->fields[3] = param.empty() ? Value::null() : str(vm, param);
}
void excParam(Vm& vm, NativeCall& c) { InstObj* e = instArg(vm, c.args[0]); c.ret = e->nfields > 3 ? e->fields[3] : Value::null(); }
void excStack(Vm& vm, NativeCall& c) { InstObj* e = instArg(vm, c.args[0]); c.ret = e->nfields > 2 ? e->fields[2] : Value::null(); }
void excInner(Vm& vm, NativeCall& c) { InstObj* e = instArg(vm, c.args[0]); c.ret = e->nfields > 1 ? e->fields[1] : Value::null(); }
void excToString(Vm& vm, NativeCall& c) {
    InstObj* e = instArg(vm, c.args[0]);
    std::string s = vm.format(c.args[0], vm.lang());
    const std::string st = e->nfields > 2 && !e->fields[2].isNull() ? vm.utf8(e->fields[2]) : std::string();
    if (!st.empty()) s += "\n" + st;
    c.ret = str(vm, s);
}
// ---- array di C#/Java
void arrLength(Vm& vm, NativeCall& c) { c.ret = Value::i32(arrayArg(vm, c.args[0])->count); }
void arrLongLength(Vm& vm, NativeCall& c) { c.ret = Value::i64(arrayArg(vm, c.args[0])->count); }
void arrRank(Vm& vm, NativeCall& c) { c.ret = Value::i32(arrayArg(vm, c.args[0])->rank); }
void arrGetLength(Vm& vm, NativeCall& c) {
    ArrayObj* a = arrayArg(vm, c.args[0]);
    const std::int64_t d = I(c, 1);
    if (d < 0 || d >= a->rank) vm.fault(FaultKind::Index, "dimensione " + std::to_string(d) + " non esistente");
    c.ret = Value::i32(a->rank == 2 ? (d == 0 ? a->rows : a->cols) : a->count);
}
// ---- iteratori delle sequenze (puntatori)
std::uint32_t seqSize(Vm& vm, const Value& it) {
    if (it.k != VK::Ptr || it.f == 2 || !it.o) vm.fault(FaultKind::NullPointer, "iteratore non valido");
    Seq s;
    s.o = it.o;
    return s.size();
}
void itDeref(Vm& vm, NativeCall& c) {
    const Value& it = c.args[0];
    if (it.k != VK::Ptr || it.f == 2 || !it.o) vm.fault(FaultKind::NullPointer, "iteratore non valido");
    if (it.idx >= seqSize(vm, it)) vm.fault(FaultKind::Index, "dereferenziazione di end() o di un iteratore oltre la fine");
    c.ret = it;
}
void itNext(Vm& vm, NativeCall& c) {
    Value it = c.args[0];
    if (it.idx >= seqSize(vm, it)) vm.fault(FaultKind::Index, "++ oltre end()");
    ++it.idx;
    c.ret = it;
}
void itPrev(Vm& vm, NativeCall& c) {
    Value it = c.args[0];
    (void)seqSize(vm, it);
    if (it.idx == 0) vm.fault(FaultKind::Index, "-- prima di begin()");
    --it.idx;
    c.ret = it;
}
void itAdd(Vm& vm, NativeCall& c) {
    Value it = c.args[0];
    const std::int64_t n = static_cast<std::int64_t>(it.idx) + I(c, 1);
    if (n < 0 || n > static_cast<std::int64_t>(seqSize(vm, it))) vm.fault(FaultKind::Index, "iteratore fuori dal contenitore (posizione " + std::to_string(n) + ")");
    it.idx = static_cast<std::uint32_t>(n);
    c.ret = it;
}
void itSub(Vm& vm, NativeCall& c) {
    Value it = c.args[0];
    const std::int64_t n = static_cast<std::int64_t>(it.idx) - I(c, 1);
    if (n < 0 || n > static_cast<std::int64_t>(seqSize(vm, it))) vm.fault(FaultKind::Index, "iteratore fuori dal contenitore (posizione " + std::to_string(n) + ")");
    it.idx = static_cast<std::uint32_t>(n);
    c.ret = it;
}
void itDiff(Vm& vm, NativeCall& c) {
    if (c.args[0].o != c.args[1].o) vm.fault(FaultKind::Generic, "differenza di iteratori di contenitori diversi");
    c.ret = Value::i64(static_cast<std::int64_t>(c.args[0].idx) - static_cast<std::int64_t>(c.args[1].idx));
}
template <int Op> void itCmp(Vm&, NativeCall& c) {
    const Value& a = c.args[0];
    const Value& b = c.args[1];
    bool r = false;
    const bool same = a.o == b.o;
    switch (Op) {
    case 0: r = same && a.idx == b.idx; break;
    case 1: r = !(same && a.idx == b.idx); break;
    case 2: r = same && a.idx < b.idx; break;
    case 3: r = same && a.idx <= b.idx; break;
    case 4: r = same && a.idx > b.idx; break;
    default: r = same && a.idx >= b.idx; break;
    }
    c.ret = Value::boolean(r);
}
void itIndex(Vm& vm, NativeCall& c) {
    Value it = c.args[0];
    const std::int64_t n = static_cast<std::int64_t>(it.idx) + I(c, 1);
    if (n < 0 || n >= static_cast<std::int64_t>(seqSize(vm, it))) vm.fault(FaultKind::Index, "indice fuori dal contenitore");
    it.idx = static_cast<std::uint32_t>(n);
    c.ret = it;
}
// ---- iteratori delle mappe
MapObj* mapOfIt(Vm& vm, const Value& it) {
    if (it.k != VK::Ptr || it.f == 2 || !it.o || it.o->kind != OK::Map) vm.fault(FaultKind::NullPointer, "iteratore della mappa non valido");
    if (it.o->dead()) vm.fault(FaultKind::DeadPointer, "mappa gia' liberata");
    return static_cast<MapObj*>(it.o);
}
void mitDeref(Vm& vm, NativeCall& c) {
    MapObj* m = mapOfIt(vm, c.args[0]);
    const std::uint32_t cell = c.args[0].idx;
    if (cell == End) vm.fault(FaultKind::Index, "dereferenziazione di end() della mappa");
    if (cell >= m->keys.size() || !m->used[cell]) vm.fault(FaultKind::DeadPointer, "iteratore di un elemento rimosso");
    c.ret = m->isSet ? m->keys[cell] : c.args[0];
}
void mitNext(Vm& vm, NativeCall& c) { MapObj* m = mapOfIt(vm, c.args[0]); Value it = c.args[0]; it.idx = mapNext(vm, m, it.idx); c.ret = it; }
void mitPrev(Vm& vm, NativeCall& c) { MapObj* m = mapOfIt(vm, c.args[0]); Value it = c.args[0]; it.idx = mapPrev(vm, m, it.idx); c.ret = it; }
void mitEq(Vm&, NativeCall& c) { c.ret = Value::boolean(c.args[0].o == c.args[1].o && c.args[0].idx == c.args[1].idx); }
void mitNe(Vm&, NativeCall& c) { c.ret = Value::boolean(!(c.args[0].o == c.args[1].o && c.args[0].idx == c.args[1].idx)); }
void mentFirst(Vm& vm, NativeCall& c) {
    MapObj* m = mapOfIt(vm, c.args[0]);
    const std::uint32_t cell = c.args[0].idx;
    if (cell >= m->keys.size() || !m->used[cell]) vm.fault(FaultKind::DeadPointer, "elemento della mappa non valido");
    c.ret = m->keys[cell];
}
void mentSecond(Vm& vm, NativeCall& c) {
    MapObj* m = mapOfIt(vm, c.args[0]);
    const std::uint32_t cell = c.args[0].idx;
    if (cell >= m->keys.size() || !m->used[cell]) vm.fault(FaultKind::DeadPointer, "elemento della mappa non valido");
    c.ret = Value::ptr(m, cell);
}
// ---- coppie
void pairFirst(Vm& vm, NativeCall& c) { c.ret = Value::ptr(instArg(vm, c.args[0]), 0); }
void pairSecond(Vm& vm, NativeCall& c) { c.ret = Value::ptr(instArg(vm, c.args[0]), 1); }
void pairKey(Vm& vm, NativeCall& c) { c.ret = instArg(vm, c.args[0])->fields[0]; }
void pairValue(Vm& vm, NativeCall& c) { c.ret = instArg(vm, c.args[0])->fields[1]; }
template <int Op> void pairCmp(Vm& vm, NativeCall& c) {
    InstObj* a = instArg(vm, c.args[0]);
    InstObj* b = instArg(vm, c.args[1]);
    const int r = vm.compareValues(Value::ref(a), Value::ref(b));
    bool out = false;
    switch (Op) {
    case 0: out = r == 0; break;
    case 1: out = r != 0; break;
    case 2: out = r < 0; break;
    case 3: out = r <= 0; break;
    case 4: out = r > 0; break;
    default: out = r >= 0; break;
    }
    c.ret = Value::boolean(out);
}
void missing(Vm& vm, NativeCall&) { vm.fault(FaultKind::NotImplemented, "funzione della libreria non disponibile"); }
// ---- enum di C#: il valore con il suo tipo (nome nella stampa)
void enumBox(Vm&, NativeCall& c) {
    Value v = c.args[0];
    const TypeP t = c.type && c.type->k == TK::Ref ? c.type->elem : c.type;
    if (t && t->k == TK::Enum && t->cls && isInt(v.k)) { v.f = 0x45; v.aux = static_cast<std::uint16_t>(t->cls->id); }
    c.ret = v;
}
void enumToString(Vm& vm, NativeCall& c) {
    NativeCall b = c;
    enumBox(vm, b);
    c.ret = str(vm, vm.format(b.ret, vm.lang()));
}

constexpr std::uint8_t P = NF::Property, St = NF::Static;
const NativeEntry kCore[] = {
    {L::All, "", "#missing", "#missing", "void()", missing, St},     // voce 0: errore chiaro
    {L::All, "", "#iter", "size", "long(any)", iterSize, St},
    {L::All, "", "#iter", "at", "any(any,long)", iterAt, St},
    {L::All, "", "#iter", "ref", "any(any,long)", iterRef, St},
    {L::All, "", "#init", "put", "void(self,any,any)", initPut, St},
    {L::All, "", "#init", "add", "void(self,any)", initAdd, St},
    {L::All, "", "object", "#is", "bool(any,long)", objIs, St},
    {L::Cs | L::Java, "", "object", "ToString", "string()", objToString, 0},
    {L::Cs, "", "object", "ToString", "string(string)", objToStringFmt, 0},
    {L::Cs, "", "object", "Equals", "bool(object)", objEquals, 0},
    {L::Cs, "", "object", "GetHashCode", "int()", objHash, 0},
    {L::Cs, "", "object", "GetType", "class:Type()", objGetType, 0},
    {L::Cs, "", "Type", "#make", "class:Type(string,string)", typeMake, St},
    {L::Cs, "", "object", "CompareTo", "int(object)", objCompareTo, 0},
    {L::Java, "", "object", "equals", "bool(object)", objEquals, 0},
    {L::Java, "", "object", "hashCode", "int()", objHash, 0},
    {L::Java, "", "object", "toString", "string()", objToString, 0},
    {L::Java, "", "object", "compareTo", "int(object)", objCompareTo, 0},
    {L::Cs | L::Java, "", "#format", "#hole", "string(any,string,int)", formatHole, St},
    {L::Cs, "", "nullable", "HasValue", "bool(any)", nullableHas, St},
    {L::Cs, "", "nullable", "GetValueOrDefault", "T()", nullableOrDefault, 0},
    {L::Cs, "", "nullable", "GetValueOrDefault", "T(T)", nullableOrDefault, 0},
    // oggetti e manipolatori standard del C++
    {L::Cpp, "iostream", "#std", "cout", "ostream()", stdCout, St},
    {L::Cpp, "iostream", "#std", "cerr", "ostream()", stdCerr, St},
    {L::Cpp, "iostream", "#std", "clog", "ostream()", stdCerr, St},
    {L::Cpp, "iostream", "#std", "cin", "istream()", stdCin, St},
    {L::Cpp, "iostream", "#std", "endl", "manip()", stdManip<1>, St},
    {L::Cpp, "iostream", "#std", "flush", "manip()", stdManip<2>, St},
    {L::Cpp, "iostream", "#std", "fixed", "manip()", stdManip<3>, St},
    {L::Cpp, "iostream", "#std", "scientific", "manip()", stdManip<4>, St},
    {L::Cpp, "iostream", "#std", "defaultfloat", "manip()", stdManip<5>, St},
    {L::Cpp, "iostream", "#std", "boolalpha", "manip()", stdManip<6>, St},
    {L::Cpp, "iostream", "#std", "noboolalpha", "manip()", stdManip<7>, St},
    {L::Cpp, "iostream", "#std", "left", "manip()", stdManip<8>, St},
    {L::Cpp, "iostream", "#std", "right", "manip()", stdManip<9>, St},
    {L::Cpp, "iostream", "#std", "hex", "manip()", stdManip<10>, St},
    {L::Cpp, "iostream", "#std", "dec", "manip()", stdManip<11>, St},
    {L::Cpp, "iostream", "#std", "oct", "manip()", stdManip<12>, St},
    {L::Cpp, "iostream", "#std", "showpoint", "manip()", stdManip<13>, St},
    {L::Cpp, "iostream", "#std", "noshowpoint", "manip()", stdManip<14>, St},
    {L::Cpp, "iostream", "#std", "ws", "manip()", stdManip<15>, St},
    {L::CC, "stdio.h", "#std", "#stdio", "file(int)", stdStdio, St},
    // eccezioni
    {L::Cpp, "exception", "exception", "what", "cstr()", excWhat, 0},
    {L::Cs, "System", "Exception", "Message", "string()", excMessage, P},
    {L::Cs, "System", "Exception", "StackTrace", "string()", excStack, P},
    {L::Cs, "System", "Exception", "InnerException", "class:Exception()", excInner, P},
    {L::Cs, "System", "Exception", "ParamName", "string()", excParam, P},
    {L::Cs, "System", "Exception", "#argInit", "void(any,int,string,string)", excArgInit, St},
    {L::Cs, "System", "Exception", "ToString", "string()", excToString, 0},
    {L::Java, "java.lang", "Exception", "getMessage", "string()", excMessage, 0},
    // array di C#/Java
    {L::Cs, "", "enum", "#box", "any(self)", enumBox, St},
    {L::Cs, "", "enum", "ToString", "string()", enumToString, 0},
    {L::Cs, "", "array", "Length", "int()", arrLength, P},
    {L::Cs, "", "array", "LongLength", "long()", arrLongLength, P},
    {L::Cs, "", "array", "Rank", "int()", arrRank, P},
    {L::Cs, "", "array", "GetLength", "int(int)", arrGetLength, 0},
    {L::Java, "", "array", "length", "int()", arrLength, P},
    // iteratori delle sequenze
    {L::Cpp, "iterator", "iterator", "*", "elem&()", itDeref, 0},
    {L::Cpp, "iterator", "iterator", "operator*", "elem&()", itDeref, 0},
    {L::Cpp, "iterator", "iterator", "#next", "self(self)", itNext, St},
    {L::Cpp, "iterator", "iterator", "#prev", "self(self)", itPrev, St},
    {L::Cpp, "iterator", "iterator", "operator+", "self(long)", itAdd, 0},
    {L::Cpp, "iterator", "iterator", "operator-", "self(long)", itSub, 0},
    {L::Cpp, "iterator", "iterator", "operator-", "long(self)", itDiff, 0},
    {L::Cpp, "iterator", "iterator", "operator==", "bool(self)", itCmp<0>, 0},
    {L::Cpp, "iterator", "iterator", "operator!=", "bool(self)", itCmp<1>, 0},
    {L::Cpp, "iterator", "iterator", "operator<", "bool(self)", itCmp<2>, 0},
    {L::Cpp, "iterator", "iterator", "operator<=", "bool(self)", itCmp<3>, 0},
    {L::Cpp, "iterator", "iterator", "operator>", "bool(self)", itCmp<4>, 0},
    {L::Cpp, "iterator", "iterator", "operator>=", "bool(self)", itCmp<5>, 0},
    {L::Cpp, "iterator", "iterator", "operator[]", "elem&(long)", itIndex, 0},
    // iteratori delle mappe e voci
    {L::Cpp, "map", "mapiterator", "*", "mapderef()", mitDeref, 0},
    {L::Cpp, "map", "mapiterator", "operator*", "mapderef()", mitDeref, 0},
    {L::Cpp, "map", "mapiterator", "#next", "self(self)", mitNext, St},
    {L::Cpp, "map", "mapiterator", "#prev", "self(self)", mitPrev, St},
    {L::Cpp, "map", "mapiterator", "operator==", "bool(self)", mitEq, 0},
    {L::Cpp, "map", "mapiterator", "operator!=", "bool(self)", mitNe, 0},
    {L::Cpp, "map", "mapentry", "first", "K()", mentFirst, P},
    {L::Cpp, "map", "mapentry", "second", "V&()", mentSecond, P},
    // coppie
    {L::Cpp, "utility", "pair", "first", "T&()", pairFirst, P},
    {L::Cpp, "utility", "pair", "second", "V&()", pairSecond, P},
    {L::Cpp, "utility", "pair", "operator==", "bool(self)", pairCmp<0>, 0},
    {L::Cpp, "utility", "pair", "operator!=", "bool(self)", pairCmp<1>, 0},
    {L::Cpp, "utility", "pair", "operator<", "bool(self)", pairCmp<2>, 0},
    {L::Cpp, "utility", "pair", "operator<=", "bool(self)", pairCmp<3>, 0},
    {L::Cpp, "utility", "pair", "operator>", "bool(self)", pairCmp<4>, 0},
    {L::Cpp, "utility", "pair", "operator>=", "bool(self)", pairCmp<5>, 0},
    {L::Cs, "System.Collections.Generic", "KeyValuePair", "Key", "K()", pairKey, P},
    {L::Cs, "System.Collections.Generic", "KeyValuePair", "Value", "V()", pairValue, P},
};
} // namespace

const NativeEntry* libCoreEntries(std::size_t& n) { n = sizeof(kCore) / sizeof(kCore[0]); return kCore; }

} // namespace fx::prog
