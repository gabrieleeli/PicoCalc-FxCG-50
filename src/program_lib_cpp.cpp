// Libreria standard del C++ (v1.20.0): std::string, flussi (cout, cin,
// cerr, fstream, stringstream) con i manipolatori di <iomanip>, contenitori
// (vector, deque, list, stack, queue, map, set, unordered_*, pair) con gli
// iteratori, algoritmi di <algorithm> e <numeric>, conversioni di <string>.
// Le eccezioni sono quelle della norma (std::out_of_range...).
#include "program_lib.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace fx::prog {
using namespace lib;

namespace {
constexpr std::int64_t Npos = -1;
inline Value npos() { return Value::ofInt(VK::U64, Npos); }
inline Value sz(std::size_t n) { return Value::ofInt(VK::U64, static_cast<std::int64_t>(n)); }
inline std::size_t U(const NativeCall& c, int i) { return static_cast<std::size_t>(c.args[i].u); }
std::string text(Vm& vm, const NativeCall& c, int i) { return S(vm, c, i); }
std::pmr::string& pstr(Vm& vm, const Value& v) { return strbArg(vm, v)->s; }
[[noreturn]] void outOfRange(Vm& vm, const std::string& msg) { vm.raise("out_of_range", msg); }

// ================================================================ std::string
void sCtorCstr(Vm& vm, NativeCall& c) { pstr(vm, c.args[0]).assign(text(vm, c, 1)); }
void sCtorStr(Vm& vm, NativeCall& c) { pstr(vm, c.args[0]).assign(text(vm, c, 1)); }
void sCtorFill(Vm& vm, NativeCall& c) {
    const std::size_t n = U(c, 1);
    if (n > vm.heap().budget()) vm.raise("length_error", "basic_string: lunghezza troppo grande");
    pstr(vm, c.args[0]).assign(n, static_cast<char>(I(c, 2)));
}
void sCtorSub(Vm& vm, NativeCall& c) {
    const std::string s = text(vm, c, 1);
    const std::size_t pos = U(c, 2);
    if (pos > s.size()) outOfRange(vm, "basic_string: __pos (which is " + std::to_string(pos) + ") > this->size() (which is " + std::to_string(s.size()) + ")");
    pstr(vm, c.args[0]).assign(s.substr(pos, c.argc > 3 ? U(c, 3) : std::string::npos));
}
void sCtorCount(Vm& vm, NativeCall& c) { const std::string s = text(vm, c, 1); pstr(vm, c.args[0]).assign(s.substr(0, std::min(s.size(), U(c, 2)))); }
void sFromCstr(Vm& vm, NativeCall& c) { c.ret = Value::ref(vm.newStrB(text(vm, c, 0))); }
void sFromChar(Vm& vm, NativeCall& c) { c.ret = Value::ref(vm.newStrB(std::string(1, static_cast<char>(I(c, 0))))); }
void sSize(Vm& vm, NativeCall& c) { c.ret = sz(pstr(vm, c.args[0]).size()); }
void sEmpty(Vm& vm, NativeCall& c) { c.ret = Value::boolean(pstr(vm, c.args[0]).empty()); }
void sIndex(Vm& vm, NativeCall& c) {
    StrBObj* s = strbArg(vm, c.args[0]);
    const std::size_t i = U(c, 1);
    if (i > s->s.size()) vm.fault(FaultKind::Index, "indice " + std::to_string(static_cast<std::int64_t>(i)) + " fuori dalla stringa (lunghezza " + std::to_string(s->s.size()) + ")");
    c.ret = Value::ptr(s, static_cast<std::uint32_t>(i));
}
void sAt(Vm& vm, NativeCall& c) {
    StrBObj* s = strbArg(vm, c.args[0]);
    const std::size_t i = U(c, 1);
    if (i >= s->s.size()) outOfRange(vm, "basic_string::at: __n (which is " + std::to_string(static_cast<std::int64_t>(i)) + ") >= this->size() (which is " + std::to_string(s->s.size()) + ")");
    c.ret = Value::ptr(s, static_cast<std::uint32_t>(i));
}
void sFront(Vm& vm, NativeCall& c) { StrBObj* s = strbArg(vm, c.args[0]); if (s->s.empty()) vm.fault(FaultKind::Index, "front() di una stringa vuota"); c.ret = Value::ptr(s, 0); }
void sBack(Vm& vm, NativeCall& c) { StrBObj* s = strbArg(vm, c.args[0]); if (s->s.empty()) vm.fault(FaultKind::Index, "back() di una stringa vuota"); c.ret = Value::ptr(s, static_cast<std::uint32_t>(s->s.size() - 1)); }
void sCstr(Vm& vm, NativeCall& c) { c.ret = Value::ptr(strbArg(vm, c.args[0]), 0); }
void sSubstr(Vm& vm, NativeCall& c) {
    const auto& s = pstr(vm, c.args[0]);
    const std::size_t pos = c.argc > 1 ? U(c, 1) : 0;
    if (pos > s.size()) outOfRange(vm, "basic_string::substr: __pos (which is " + std::to_string(static_cast<std::int64_t>(pos)) + ") > this->size() (which is " + std::to_string(s.size()) + ")");
    const std::size_t len = c.argc > 2 ? U(c, 2) : std::string::npos;
    c.ret = Value::ref(vm.newStrB(std::string_view(s.data(), s.size()).substr(pos, len)));
}
std::string needle(Vm& vm, const NativeCall& c, int i) {
    const Value& v = c.args[i];
    if (v.k == VK::C8 || v.k == VK::I32) return std::string(1, static_cast<char>(v.i));
    return text(vm, c, i);
}
template <int Kind> void sFind(Vm& vm, NativeCall& c) {
    const auto& s0 = pstr(vm, c.args[0]);
    const std::string_view s(s0.data(), s0.size());
    const std::string n = needle(vm, c, 1);
    const bool hasPos = c.argc > 2;
    std::size_t r;
    switch (Kind) {
    case 0: r = s.find(n, hasPos ? U(c, 2) : 0); break;
    case 1: r = s.rfind(n, hasPos ? U(c, 2) : std::string::npos); break;
    case 2: r = s.find_first_of(n, hasPos ? U(c, 2) : 0); break;
    case 3: r = s.find_last_of(n, hasPos ? U(c, 2) : std::string::npos); break;
    case 4: r = s.find_first_not_of(n, hasPos ? U(c, 2) : 0); break;
    default: r = s.find_last_not_of(n, hasPos ? U(c, 2) : std::string::npos); break;
    }
    c.ret = r == std::string::npos ? npos() : sz(r);
}
void checkPos(Vm& vm, std::size_t pos, std::size_t size, const char* what) {
    if (pos > size) outOfRange(vm, std::string("basic_string::") + what + ": __pos (which is " + std::to_string(static_cast<std::int64_t>(pos)) + ") > this->size() (which is " + std::to_string(size) + ")");
}
void sReplace(Vm& vm, NativeCall& c) {
    auto& s = pstr(vm, c.args[0]);
    const std::size_t pos = U(c, 1);
    checkPos(vm, pos, s.size(), "replace");
    const std::string r = text(vm, c, 3);
    s.replace(pos, U(c, 2), r.data(), r.size());
    c.ret = c.args[0];
}
void sInsert(Vm& vm, NativeCall& c) {
    auto& s = pstr(vm, c.args[0]);
    const std::size_t pos = U(c, 1);
    checkPos(vm, pos, s.size(), "insert");
    const std::string r = text(vm, c, 2);
    s.insert(pos, r.data(), r.size());
    c.ret = c.args[0];
}
void sInsertFill(Vm& vm, NativeCall& c) {
    auto& s = pstr(vm, c.args[0]);
    const std::size_t pos = U(c, 1);
    checkPos(vm, pos, s.size(), "insert");
    s.insert(pos, U(c, 2), static_cast<char>(I(c, 3)));
    c.ret = c.args[0];
}
void sErase(Vm& vm, NativeCall& c) {
    auto& s = pstr(vm, c.args[0]);
    const std::size_t pos = c.argc > 1 ? U(c, 1) : 0;
    checkPos(vm, pos, s.size(), "erase");
    s.erase(pos, c.argc > 2 ? U(c, 2) : std::string::npos);
    c.ret = c.args[0];
}
void sEraseIt(Vm& vm, NativeCall& c) {
    StrBObj* s = strbArg(vm, c.args[0]);
    const Value it = c.args[1];
    if (it.o != s || it.idx >= s->s.size()) vm.fault(FaultKind::Index, "erase con un iteratore non valido");
    s->s.erase(it.idx, 1);
    c.ret = Value::ptr(s, it.idx);
}
void sEraseRange(Vm& vm, NativeCall& c) {
    StrBObj* s = strbArg(vm, c.args[0]);
    const Value a = c.args[1], b = c.args[2];
    if (a.o != s || b.o != s || a.idx > b.idx || b.idx > s->s.size()) vm.fault(FaultKind::Index, "erase con iteratori non validi");
    s->s.erase(a.idx, b.idx - a.idx);
    c.ret = Value::ptr(s, a.idx);
}
void sAppend(Vm& vm, NativeCall& c) { const std::string r = needle(vm, c, 1); pstr(vm, c.args[0]).append(r.data(), r.size()); c.ret = c.args[0]; }
void sAppendFill(Vm& vm, NativeCall& c) { pstr(vm, c.args[0]).append(U(c, 1), static_cast<char>(I(c, 2))); c.ret = c.args[0]; }
void sPushBack(Vm& vm, NativeCall& c) { pstr(vm, c.args[0]).push_back(static_cast<char>(I(c, 1))); }
void sPopBack(Vm& vm, NativeCall& c) { auto& s = pstr(vm, c.args[0]); if (s.empty()) vm.fault(FaultKind::Index, "pop_back() di una stringa vuota"); s.pop_back(); }
void sClear(Vm& vm, NativeCall& c) { pstr(vm, c.args[0]).clear(); }
void sCompare(Vm& vm, NativeCall& c) {
    const auto& s = pstr(vm, c.args[0]);
    const int r = std::string_view(s.data(), s.size()).compare(text(vm, c, 1));
    c.ret = Value::i32(r < 0 ? -1 : r > 0 ? 1 : 0);
}
void sResize(Vm& vm, NativeCall& c) {
    const std::size_t n = U(c, 1);
    if (n > vm.heap().budget()) vm.raise("length_error", "basic_string::resize");
    pstr(vm, c.args[0]).resize(n, c.argc > 2 ? static_cast<char>(I(c, 2)) : '\0');
}
void sReserve(Vm& vm, NativeCall& c) { (void)vm; (void)c; }
void sCapacity(Vm& vm, NativeCall& c) { c.ret = sz(pstr(vm, c.args[0]).capacity()); }
void sSwap(Vm& vm, NativeCall& c) { std::swap(pstr(vm, c.args[0]), pstr(vm, c.args[1])); }
void sBegin(Vm& vm, NativeCall& c) { c.ret = Value::ptr(strbArg(vm, c.args[0]), 0); }
void sEnd(Vm& vm, NativeCall& c) { StrBObj* s = strbArg(vm, c.args[0]); c.ret = Value::ptr(s, static_cast<std::uint32_t>(s->s.size())); }
void sPlus(Vm& vm, NativeCall& c) {
    const auto& a = pstr(vm, c.args[0]);
    std::string r(a.data(), a.size());
    r += needle(vm, c, 1);
    c.ret = Value::ref(vm.newStrB(r));
}
void sPlusEq(Vm& vm, NativeCall& c) { const std::string r = needle(vm, c, 1); pstr(vm, c.args[0]).append(r.data(), r.size()); c.ret = c.args[0]; }
template <int Op> void sCmp(Vm& vm, NativeCall& c) {
    const auto& a = pstr(vm, c.args[0]);
    const int r = std::string_view(a.data(), a.size()).compare(text(vm, c, 1));
    bool out;
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
// conversioni di <string>
std::string fmtTo(const char* f, double d) { char b[400]; std::snprintf(b, sizeof b, f, d); return b; }
void toStringI(Vm& vm, NativeCall& c) { c.ret = Value::ref(vm.newStrB(std::to_string(I(c, 0)))); }
void toStringU(Vm& vm, NativeCall& c) { c.ret = Value::ref(vm.newStrB(std::to_string(c.args[0].u))); }
void toStringD(Vm& vm, NativeCall& c) { c.ret = Value::ref(vm.newStrB(fmtTo("%f", D(c, 0)))); }
template <VK K> void stoX(Vm& vm, NativeCall& c, const char* name) {
    const std::string s = text(vm, c, 0);
    const int base = c.argc > 2 ? static_cast<int>(I(c, 2)) : 10;
    std::int64_t v = 0;
    bool ovf = false;
    const std::size_t n = parseInt(s, 0, base, v, ovf);
    if (n == 0) vm.raise("invalid_argument", name);
    if (ovf) outOfRange(vm, name);
    if (K == VK::I32 && (v > INT32_MAX || v < INT32_MIN)) outOfRange(vm, name);
    if (c.argc > 1 && !c.args[1].isNull()) vm.storePtr(c.args[1], sz(n));
    c.ret = Value::ofInt(K, v);
}
void stoi_(Vm& vm, NativeCall& c) { stoX<VK::I32>(vm, c, "stoi"); }
void stol_(Vm& vm, NativeCall& c) { stoX<VK::I64>(vm, c, "stol"); }
void stoll_(Vm& vm, NativeCall& c) { stoX<VK::I64>(vm, c, "stoll"); }
void stoul_(Vm& vm, NativeCall& c) { stoX<VK::U64>(vm, c, "stoul"); }
void stod_(Vm& vm, NativeCall& c) {
    const std::string s = text(vm, c, 0);
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    double d = 0;
    const std::size_t n = parseDouble(s, i, d);
    if (n == 0) vm.raise("invalid_argument", "stod");
    if (std::isinf(d) && s.find("inf") == std::string::npos && s.find("INF") == std::string::npos) outOfRange(vm, "stod");
    if (c.argc > 1 && !c.args[1].isNull()) vm.storePtr(c.args[1], sz(i + n));
    c.ret = Value::f64(d);
}
void stof_(Vm& vm, NativeCall& c) { stod_(vm, c); c.ret = Value::f32(c.ret.d); }

// ================================================================ flussi
FileObj* stream(Vm& vm, const Value& v) { return fileArg(vm, v); }
std::string intText(FileObj* f, const Value& v) {
    if (f->base == 10) return v.k == VK::U64 ? std::to_string(v.u) : std::to_string(v.i);
    std::uint64_t u = static_cast<std::uint64_t>(v.i);
    if (v.k == VK::I32 || v.k == VK::I16 || v.k == VK::I8) u = static_cast<std::uint32_t>(v.i);
    char b[32];
    std::snprintf(b, sizeof b, f->base == 16 ? "%llx" : "%llo", static_cast<unsigned long long>(u));
    return b;
}
std::string cppText(Vm& vm, FileObj* f, const Value& v) {
    switch (v.k) {
    case VK::Void: return "";
    case VK::Null: return "0";
    case VK::Bool: return f->boolalpha ? (v.i ? "true" : "false") : (v.i ? "1" : "0");
    case VK::C8: case VK::I8: case VK::U8: return std::string(1, static_cast<char>(v.i));
    case VK::C16: return utf16To8(std::u16string(1, static_cast<char16_t>(v.i)));
    case VK::I16: case VK::U16: case VK::I32: case VK::U32: case VK::I64: case VK::U64: return intText(f, v);
    case VK::F32: case VK::F64: return formatDoubleCpp(v.d, f->precision, f->floatMode, f->showpoint);
    case VK::Fn: return "1";
    case VK::Ref: case VK::Ptr: {
        if (!v.o && v.f != 2) vm.fault(FaultKind::NullPointer, "<< di un puntatore NULL");
        if (v.k == VK::Ptr && v.f != 2 && v.idx != Whole && v.o->kind == OK::StrB) {
            const auto& s = static_cast<StrBObj*>(v.o)->s;
            return v.idx <= s.size() ? std::string(s.substr(v.idx)) : std::string();
        }
        if (v.f != 2) {
            Obj* o = v.o;
            if (o->kind == OK::StrB || o->kind == OK::StrW) return vm.utf8(v);
            if (o->kind == OK::Array && static_cast<ArrayObj*>(o)->ek == VK::C8) return vm.cstring(v.k == VK::Ref ? Value::ptr(o, 0) : v);
            if (o->kind == OK::Inst && v.k == VK::Ref) {
                InstObj* x = static_cast<InstObj*>(o);
                vm.fault(FaultKind::Generic, "nessun operator<< per " + (x->cls ? x->cls->name : std::string("std::pair")) + " (definiscilo: ostream& operator<<(ostream&, const T&))");
            }
            if (o->kind == OK::Vec || o->kind == OK::Map) vm.fault(FaultKind::Generic, "nessun operator<< per un contenitore (stampa gli elementi con un ciclo)");
        }
        return vm.format(v, Dialect::Cpp);
    }
    }
    return "";
}
void emitText(Vm& vm, FileObj* f, std::string t) {
    if (f->width > 0 && static_cast<int>(t.size()) < f->width) {
        const std::string pad(static_cast<std::size_t>(f->width) - t.size(), f->fill);
        t = f->left ? t + pad : pad + t;
    }
    f->width = 0;
    filePut(vm, f, t);
}
void oShl(Vm& vm, NativeCall& c) {
    FileObj* f = stream(vm, c.args[0]);
    const Value& v = c.args[1];
    if (v.k == VK::I64 && v.f == 0x4D) {
        // manipolatore
        switch (v.aux) {
        case 1: filePut(vm, f, "\n"); break;
        case 3: f->floatMode = 1; break;
        case 4: f->floatMode = 2; break;
        case 5: f->floatMode = 0; break;
        case 6: f->boolalpha = true; break;
        case 7: f->boolalpha = false; break;
        case 8: f->left = true; break;
        case 9: f->left = false; break;
        case 10: f->base = 16; break;
        case 11: f->base = 10; break;
        case 12: f->base = 8; break;
        case 13: f->showpoint = true; break;
        case 14: f->showpoint = false; break;
        case 20: f->width = static_cast<int>(v.i); break;
        case 21: f->precision = static_cast<int>(v.i); break;
        case 22: f->fill = static_cast<char>(v.i); break;
        default: break;
        }
        c.ret = c.args[0];
        return;
    }
    emitText(vm, f, cppText(vm, f, v));
    c.ret = c.args[0];
}
void oPut(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); const char ch = static_cast<char>(I(c, 1)); filePut(vm, f, std::string_view(&ch, 1)); c.ret = c.args[0]; }
void oFlush(Vm&, NativeCall& c) { c.ret = c.args[0]; }
void oPrecision(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); const int old = f->precision; if (c.argc > 1) f->precision = static_cast<int>(I(c, 1)); c.ret = Value::i64(old); }
void oWidth(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); const int old = f->width; if (c.argc > 1) f->width = static_cast<int>(I(c, 1)); c.ret = Value::i64(old); }
void oFill(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); const char old = f->fill; if (c.argc > 1) f->fill = static_cast<char>(I(c, 1)); c.ret = Value::ofInt(VK::C8, old); }
template <int K> void mParam(Vm&, NativeCall& c) {
    Value v;
    v.k = VK::I64;
    v.f = 0x4D;
    v.aux = static_cast<std::uint16_t>(K);
    v.i = I(c, 0);
    c.ret = v;
}
// lettura
bool readToken(Vm& vm, FileObj* f, std::string& out) {
    if (f->fail) return false;
    if (!fileToken(vm, f, out)) { f->fail = true; f->eof = true; return false; }
    return true;
}
template <VK K> void iShr(Vm& vm, NativeCall& c) {
    FileObj* f = stream(vm, c.args[0]);
    c.ret = c.args[0];
    if (f->fail) return;
    const Value target = c.args[1];
    // salta gli spazi
    int ch = filePeek(vm, f);
    while (ch >= 0 && std::isspace(ch)) { fileGet(vm, f); ch = filePeek(vm, f); }
    if (ch < 0) { f->fail = true; f->eof = true; if (isNum(K) && K != VK::C8) vm.storePtr(target, vm.convertTo(Value::i32(0), K)); return; }
    if (K == VK::C8) { vm.storePtr(target, Value::ofInt(VK::C8, static_cast<signed char>(fileGet(vm, f)))); return; }
    std::string t;
    if (K == VK::F32 || K == VK::F64) {
        auto take = [&] { t += static_cast<char>(fileGet(vm, f)); ch = filePeek(vm, f); };
        if (ch == '+' || ch == '-') take();
        while (ch >= 0 && std::isdigit(ch)) take();
        if (ch == '.') { take(); while (ch >= 0 && std::isdigit(ch)) take(); }
        if ((ch == 'e' || ch == 'E') && !t.empty()) {
            take();
            if (ch == '+' || ch == '-') take();
            while (ch >= 0 && std::isdigit(ch)) take();
        }
        double d = 0;
        if (parseDouble(t, 0, d) == 0) { f->fail = true; vm.storePtr(target, vm.convertTo(Value::f64(0), K)); return; }
        vm.storePtr(target, vm.convertTo(Value::f64(d), K));
        if (filePeek(vm, f) < 0) f->eof = true;
        return;
    }
    if (K == VK::Bool) {
        if (f->boolalpha) {
            readToken(vm, f, t);
            if (t == "true") vm.storePtr(target, Value::boolean(true));
            else if (t == "false") vm.storePtr(target, Value::boolean(false));
            else { f->fail = true; vm.storePtr(target, Value::boolean(false)); }
            return;
        }
    }
    // interi
    const int base = f->base;
    if (ch == '+' || ch == '-') { t += static_cast<char>(fileGet(vm, f)); ch = filePeek(vm, f); }
    auto digit = [&](int x) { if (x < 0) return false; if (base == 16) return std::isxdigit(x) != 0; if (base == 8) return x >= '0' && x <= '7'; return std::isdigit(x) != 0; };
    while (digit(ch)) { t += static_cast<char>(fileGet(vm, f)); ch = filePeek(vm, f); }
    if (ch < 0) f->eof = true;
    std::int64_t v = 0;
    bool ovf = false;
    if (parseInt(t, 0, base, v, ovf) == 0) {
        f->fail = true;
        for (std::size_t k = t.size(); k-- > 0;) fileUnget(vm, f, static_cast<unsigned char>(t[k]));
        vm.storePtr(target, vm.convertTo(Value::i32(0), K));
        return;
    }
    if (K == VK::Bool) {
        if (v != 0 && v != 1) { f->fail = true; vm.storePtr(target, Value::boolean(true)); return; }
        vm.storePtr(target, Value::boolean(v != 0));
        return;
    }
    // fuori campo: il massimo del tipo e failbit
    const int bits = bitsOf(K);
    const bool uns = K == VK::U8 || K == VK::U16 || K == VK::U32 || K == VK::U64;
    if (bits < 64) {
        const std::int64_t hi = uns ? (std::int64_t(1) << bits) - 1 : (std::int64_t(1) << (bits - 1)) - 1;
        const std::int64_t lo = uns ? 0 : -(std::int64_t(1) << (bits - 1));
        if (v > hi || (v < lo && !(uns && v < 0))) { ovf = true; v = v > hi ? hi : lo; }
    }
    if (ovf) f->fail = true;
    vm.storePtr(target, Value::ofInt(K, v));
}
void iShrString(Vm& vm, NativeCall& c) {
    FileObj* f = stream(vm, c.args[0]);
    c.ret = c.args[0];
    std::string t;
    if (!readToken(vm, f, t)) return;
    vm.storePtr(c.args[1], Value::ref(vm.newStrB(t)));
}
void iShrChars(Vm& vm, NativeCall& c) {
    FileObj* f = stream(vm, c.args[0]);
    c.ret = c.args[0];
    std::string t;
    if (!readToken(vm, f, t)) return;
    // char buf[N]: con il controllo dei limiti
    Value p = c.args[1];
    if (p.k == VK::Ref) p = Value::ptr(p.o, 0);
    if (p.k != VK::Ptr || p.f == 2 || !p.o || p.o->kind != OK::Array) vm.fault(FaultKind::Generic, ">> in un char* che non e' un array");
    ArrayObj* a = static_cast<ArrayObj*>(p.o);
    const std::uint32_t at = p.idx == Whole ? 0 : p.idx;
    if (at + t.size() + 1 > a->count) vm.fault(FaultKind::Index, "cin >> : " + std::to_string(t.size() + 1) + " caratteri in un array di " + std::to_string(a->count - at));
    std::memcpy(a->data + at, t.data(), t.size());
    a->data[at + t.size()] = 0;
}
void iShrManip(Vm& vm, NativeCall& c) {
    FileObj* f = stream(vm, c.args[0]);
    const Value& v = c.args[1];
    if (v.aux == 15) { int ch = filePeek(vm, f); while (ch >= 0 && std::isspace(ch)) { fileGet(vm, f); ch = filePeek(vm, f); } }
    else if (v.aux == 10) f->base = 16;
    else if (v.aux == 11) f->base = 10;
    else if (v.aux == 12) f->base = 8;
    else if (v.aux == 6) f->boolalpha = true;
    else if (v.aux == 7) f->boolalpha = false;
    c.ret = c.args[0];
}
void getlineImpl(Vm& vm, NativeCall& c, char delim) {
    FileObj* f = stream(vm, c.args[0]);
    c.ret = c.args[0];
    if (f->fail) return;
    std::string t;
    if (!fileLine(vm, f, t, delim)) { f->fail = true; f->eof = true; return; }
    vm.storePtr(c.args[1], Value::ref(vm.newStrB(t)));
}
void sGetline(Vm& vm, NativeCall& c) { getlineImpl(vm, c, '\n'); }
void sGetlineDelim(Vm& vm, NativeCall& c) { getlineImpl(vm, c, static_cast<char>(I(c, 2))); }
void iGetlineChars(Vm& vm, NativeCall& c) {
    FileObj* f = stream(vm, c.args[0]);
    c.ret = c.args[0];
    const std::int64_t n = I(c, 2);
    std::string t;
    if (!fileLine(vm, f, t, c.argc > 3 ? static_cast<char>(I(c, 3)) : '\n')) { f->fail = true; f->eof = true; t.clear(); }
    if (n <= 0) return;
    if (static_cast<std::int64_t>(t.size()) > n - 1) { t.resize(static_cast<std::size_t>(n - 1)); f->fail = true; }
    Value p = c.args[1];
    if (p.k == VK::Ref) p = Value::ptr(p.o, 0);
    if (p.k != VK::Ptr || p.f == 2 || !p.o || p.o->kind != OK::Array) vm.fault(FaultKind::Generic, "getline in un char* che non e' un array");
    ArrayObj* a = static_cast<ArrayObj*>(p.o);
    const std::uint32_t at = p.idx == Whole ? 0 : p.idx;
    if (at + t.size() + 1 > a->count) vm.fault(FaultKind::Index, "getline: oltre la fine dell'array");
    std::memcpy(a->data + at, t.data(), t.size());
    a->data[at + t.size()] = 0;
}
void iGet(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); const int ch = fileGet(vm, f); if (ch < 0) f->fail = true; c.ret = Value::i32(ch); }
void iGetChar(Vm& vm, NativeCall& c) {
    FileObj* f = stream(vm, c.args[0]);
    c.ret = c.args[0];
    const int ch = fileGet(vm, f);
    if (ch < 0) { f->fail = true; return; }
    vm.storePtr(c.args[1], Value::ofInt(VK::C8, static_cast<signed char>(ch)));
}
void iPeek(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); const int ch = filePeek(vm, f); if (ch < 0) f->eof = true; c.ret = Value::i32(ch); }
void iIgnore(Vm& vm, NativeCall& c) {
    FileObj* f = stream(vm, c.args[0]);
    const std::int64_t n = c.argc > 1 ? I(c, 1) : 1;
    const int delim = c.argc > 2 ? static_cast<int>(I(c, 2)) : -1;
    for (std::int64_t k = 0; k < n; ++k) {
        const int ch = fileGet(vm, f);
        if (ch < 0 || ch == delim) break;
    }
    c.ret = c.args[0];
}
void iUnget(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); if (f->console) conUnget(vm); else if (f->pos > 0) --f->pos; c.ret = c.args[0]; }
void iosGood(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); c.ret = Value::boolean(!f->fail && !f->error && !f->eof); }
void iosBool(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); c.ret = Value::boolean(!f->fail && !f->error && (f->open || f->console || f->memory)); }
void iosNot(Vm& vm, NativeCall& c) { NativeCall d = c; iosBool(vm, d); c.ret = Value::boolean(!d.ret.i); }
void iosEof(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); c.ret = Value::boolean(f->eof); }
void iosFail(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); c.ret = Value::boolean(f->fail || f->error || (!f->open && !f->console && !f->memory)); }
void iosBad(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); c.ret = Value::boolean(f->error); }
void iosClear(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); f->fail = f->eof = f->error = false; }
// file
std::string modeOf(std::int64_t flags, bool input, bool output) {
    const bool app = flags & 1, in = (flags & 8) || (input && !output && !(flags & 16)), out = (flags & 16) || (output && !input);
    if (app) return in ? "a+" : "a";
    if (in && out) return (flags & 32) ? "w+" : "r+";
    if (out) return "w";
    return "r";
}
void fsOpenImpl(Vm& vm, FileObj* f, const std::string& path, std::int64_t flags, bool input, bool output) {
    std::string why;
    if (!fileOpen(vm, f, path, modeOf(flags, input, output), why)) { f->fail = true; vm.lastIoError = why; f->open = false; }
}
bool isIn(const FileObj* f) { return f->type && (f->type->nk == NK::IFStream); }
bool isOut(const FileObj* f) { return f->type && (f->type->nk == NK::OFStream); }
void fsCtor(Vm& vm, NativeCall& c) {
    FileObj* f = stream(vm, c.args[0]);
    const bool fst = f->type && f->type->nk == NK::FStream;
    fsOpenImpl(vm, f, text(vm, c, 1), c.argc > 2 ? I(c, 2) : (fst ? 8 | 16 : 0), isIn(f) || fst, isOut(f) || fst);
}
void fsOpen(Vm& vm, NativeCall& c) { fsCtor(vm, c); }
void fsIsOpen(Vm& vm, NativeCall& c) { c.ret = Value::boolean(stream(vm, c.args[0])->open); }
void fsClose(Vm& vm, NativeCall& c) {
    FileObj* f = stream(vm, c.args[0]);
    if (!f->open) { f->fail = true; return; }
    if (!fileClose(vm, f)) { f->fail = true; vm.lastIoError = "scrittura non riuscita (SD piena o rimossa)"; }
}
// stringstream
void ssCtor(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); const std::string s = text(vm, c, 1); f->buf.assign(s.data(), s.size()); f->pos = 0; }
void ssStr(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); c.ret = Value::ref(vm.newStrB(std::string_view(f->buf.data(), f->buf.size()))); }
void ssSetStr(Vm& vm, NativeCall& c) { FileObj* f = stream(vm, c.args[0]); const std::string s = text(vm, c, 1); f->buf.assign(s.data(), s.size()); f->pos = 0; f->eof = f->fail = false; }

// ================================================================ contenitori sequenziali
VecObj* vec(Vm& vm, const NativeCall& c) { return vecArg(vm, c.args[0]); }
TypeP elemT(const NativeCall& c) { return typeArg(c, 0); }
void touch(VecObj* v) { ++v->version; }
void vCtorN(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    const std::size_t n = U(c, 1);
    if (n > vm.heap().budget() / sizeof(Value)) vm.raise("length_error", "cannot create std::vector larger than max_size()");
    const TypeP et = elemT(c);
    v->items.clear();
    v->items.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        v->items.push_back(c.argc > 2 ? stored(vm, c.args[2], et) : vm.defaultValue(et));
        if ((i & 1023) == 1023) vm.poll();
    }
    touch(v);
}
void vCtorCopy(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    VecObj* src = vecArg(vm, c.args[1]);
    const TypeP et = elemT(c);
    v->items.clear();
    for (const Value& x : src->items) v->items.push_back(stored(vm, x, et));
    touch(v);
}
void vCtorRange(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[1], c.args[2], s, b, e);
    const TypeP et = elemT(c);
    v->items.clear();
    for (std::uint32_t i = b; i < e; ++i) v->items.push_back(stored(vm, s.get(vm, i), et));
    touch(v);
}
void vPushBack(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    if (v->items.size() >= 0x7FFFFFF0u) vm.raise("length_error", "vector::push_back");
    const Value x = stored(vm, c.args[1], elemT(c));
    v->items.push_back(x);
    touch(v);
}
void vPushFront(Vm& vm, NativeCall& c) { VecObj* v = vec(vm, c); const Value x = stored(vm, c.args[1], elemT(c)); v->items.insert(v->items.begin(), x); touch(v); }
void vPopBack(Vm& vm, NativeCall& c) { VecObj* v = vec(vm, c); if (v->items.empty()) vm.fault(FaultKind::Index, "pop_back() di un contenitore vuoto"); v->items.pop_back(); touch(v); }
void vPopFront(Vm& vm, NativeCall& c) { VecObj* v = vec(vm, c); if (v->items.empty()) vm.fault(FaultKind::Index, "pop_front()/pop() di un contenitore vuoto"); v->items.erase(v->items.begin()); touch(v); }
void vSize(Vm& vm, NativeCall& c) { c.ret = sz(vec(vm, c)->items.size()); }
void vEmpty(Vm& vm, NativeCall& c) { c.ret = Value::boolean(vec(vm, c)->items.empty()); }
void vIndex(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    const std::size_t i = U(c, 1);
    if (i >= v->items.size()) vm.fault(FaultKind::Index, "indice " + std::to_string(static_cast<std::int64_t>(i)) + " fuori da [0, " + std::to_string(static_cast<std::int64_t>(v->items.size()) - 1) + "] (usa at() per l'eccezione)");
    c.ret = Value::ptr(v, static_cast<std::uint32_t>(i));
}
void vAt(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    const std::size_t i = U(c, 1);
    if (i >= v->items.size()) outOfRange(vm, "vector::_M_range_check: __n (which is " + std::to_string(static_cast<std::int64_t>(i)) + ") >= this->size() (which is " + std::to_string(v->items.size()) + ")");
    c.ret = Value::ptr(v, static_cast<std::uint32_t>(i));
}
void vFront(Vm& vm, NativeCall& c) { VecObj* v = vec(vm, c); if (v->items.empty()) vm.fault(FaultKind::Index, "front() di un contenitore vuoto"); c.ret = Value::ptr(v, 0); }
void vBack(Vm& vm, NativeCall& c) { VecObj* v = vec(vm, c); if (v->items.empty()) vm.fault(FaultKind::Index, "back()/top() di un contenitore vuoto"); c.ret = Value::ptr(v, static_cast<std::uint32_t>(v->items.size() - 1)); }
void vClear(Vm& vm, NativeCall& c) { VecObj* v = vec(vm, c); v->items.clear(); touch(v); }
void vResize(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    const std::size_t n = U(c, 1);
    if (n > vm.heap().budget() / sizeof(Value)) vm.raise("length_error", "vector::_M_default_append");
    const TypeP et = elemT(c);
    if (n < v->items.size()) v->items.resize(n);
    while (v->items.size() < n) v->items.push_back(c.argc > 2 ? stored(vm, c.args[2], et) : vm.defaultValue(et));
    touch(v);
}
void vReserve(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    const std::size_t n = U(c, 1);
    if (n > vm.heap().budget() / sizeof(Value)) vm.raise("length_error", "vector::reserve");
    v->items.reserve(n);
}
void vCapacity(Vm& vm, NativeCall& c) { c.ret = sz(vec(vm, c)->items.capacity()); }
void vBegin(Vm& vm, NativeCall& c) { c.ret = Value::ptr(vec(vm, c), 0); }
void vEnd(Vm& vm, NativeCall& c) { VecObj* v = vec(vm, c); c.ret = Value::ptr(v, static_cast<std::uint32_t>(v->items.size())); }
std::uint32_t itPos(Vm& vm, VecObj* v, const Value& it, bool allowEnd) {
    if (it.k != VK::Ptr || it.o != v) vm.fault(FaultKind::Generic, "iteratore di un altro contenitore");
    if (it.idx > v->items.size() || (!allowEnd && it.idx == v->items.size())) vm.fault(FaultKind::Index, "iteratore fuori dal contenitore");
    return it.idx;
}
void vInsert(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    const std::uint32_t p = itPos(vm, v, c.args[1], true);
    const Value x = stored(vm, c.args[2], elemT(c));
    v->items.insert(v->items.begin() + p, x);
    touch(v);
    c.ret = Value::ptr(v, p);
}
void vInsertN(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    const std::uint32_t p = itPos(vm, v, c.args[1], true);
    const std::size_t n = U(c, 2);
    for (std::size_t k = 0; k < n; ++k) v->items.insert(v->items.begin() + p, stored(vm, c.args[3], elemT(c)));
    touch(v);
    c.ret = Value::ptr(v, p);
}
void vErase(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    const std::uint32_t p = itPos(vm, v, c.args[1], false);
    v->items.erase(v->items.begin() + p);
    touch(v);
    c.ret = Value::ptr(v, p);
}
void vEraseRange(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    const std::uint32_t a = itPos(vm, v, c.args[1], true), b = itPos(vm, v, c.args[2], true);
    if (a > b) vm.fault(FaultKind::Argument, "erase: inizio dopo la fine");
    v->items.erase(v->items.begin() + a, v->items.begin() + b);
    touch(v);
    c.ret = Value::ptr(v, a);
}
void vAssign(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    v->items.clear();
    const std::size_t n = U(c, 1);
    for (std::size_t k = 0; k < n; ++k) v->items.push_back(stored(vm, c.args[2], elemT(c)));
    touch(v);
}
void vSwap(Vm& vm, NativeCall& c) { VecObj* a = vec(vm, c); VecObj* b = vecArg(vm, c.args[1]); a->items.swap(b->items); touch(a); touch(b); }
void vShrink(Vm& vm, NativeCall& c) { vec(vm, c)->items.shrink_to_fit(); }
void lSort(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    std::vector<Value> items(v->items.begin(), v->items.end());
    const Value* cmp = c.argc > 1 ? &c.args[1] : nullptr;
    stableSort(vm, items, [&](const Value& a, const Value& b) { return lessWith(vm, cmp, a, b, false); });
    v->items.assign(items.begin(), items.end());
    touch(v);
}
void lReverse(Vm& vm, NativeCall& c) { VecObj* v = vec(vm, c); std::reverse(v->items.begin(), v->items.end()); touch(v); }
void lRemove(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    const Value x = c.args[1];
    std::pmr::vector<Value> out(vm.heap().resource());
    for (const Value& e : v->items) if (!vm.equalValues(e, x)) out.push_back(e);
    v->items.swap(out);
    touch(v);
}
void lUnique(Vm& vm, NativeCall& c) {
    VecObj* v = vec(vm, c);
    std::pmr::vector<Value> out(vm.heap().resource());
    for (const Value& e : v->items) if (out.empty() || !vm.equalValues(out.back(), e)) out.push_back(e);
    v->items.swap(out);
    touch(v);
}

// ================================================================ mappe e insiemi
MapObj* mp(Vm& vm, const NativeCall& c) { return mapArg(vm, c.args[0]); }
Value keyOf(Vm& vm, const NativeCall& c, int i) { return stored(vm, c.args[i], typeArg(c, 0)); }
void mIndex(Vm& vm, NativeCall& c) {
    MapObj* m = mp(vm, c);
    const std::int32_t f = mapFind(vm, m, c.args[1]);
    if (f >= 0) { c.ret = Value::ptr(m, static_cast<std::uint32_t>(f)); return; }
    const Value k = keyOf(vm, c, 1);
    vm.root(k.holdsObj() ? k.o : nullptr);
    const Value d = vm.defaultValue(typeArg(c, 1));
    vm.root(d.holdsObj() ? d.o : nullptr);
    c.ret = Value::ptr(m, mapInsert(vm, m, k, d, nullptr));
}
void mAt(Vm& vm, NativeCall& c) {
    MapObj* m = mp(vm, c);
    const std::int32_t f = mapFind(vm, m, c.args[1]);
    if (f < 0) outOfRange(vm, "map::at");
    c.ret = Value::ptr(m, static_cast<std::uint32_t>(f));
}
Value pairResult(Vm& vm, MapObj* m, std::uint32_t cell, bool inserted) {
    return Value::ref(vm.newPair(nullptr, Value::ptr(m, cell), Value::boolean(inserted)));
}
void mInsertPair(Vm& vm, NativeCall& c) {
    MapObj* m = mp(vm, c);
    InstObj* p = instArg(vm, c.args[1]);
    const Value k = stored(vm, p->fields[0], typeArg(c, 0));
    vm.root(k.holdsObj() ? k.o : nullptr);
    const Value v = stored(vm, p->fields[1], typeArg(c, 1));
    vm.root(v.holdsObj() ? v.o : nullptr);
    bool ins = false;
    const std::uint32_t cell = mapInsert(vm, m, k, v, &ins);
    c.ret = pairResult(vm, m, cell, ins);
}
void mEmplace(Vm& vm, NativeCall& c) {
    MapObj* m = mp(vm, c);
    const Value k = keyOf(vm, c, 1);
    vm.root(k.holdsObj() ? k.o : nullptr);
    const Value v = stored(vm, c.args[2], typeArg(c, 1));
    vm.root(v.holdsObj() ? v.o : nullptr);
    bool ins = false;
    const std::uint32_t cell = mapInsert(vm, m, k, v, &ins);
    c.ret = pairResult(vm, m, cell, ins);
}
void mInsertOrAssign(Vm& vm, NativeCall& c) {
    MapObj* m = mp(vm, c);
    const Value k = keyOf(vm, c, 1);
    vm.root(k.holdsObj() ? k.o : nullptr);
    const Value v = stored(vm, c.args[2], typeArg(c, 1));
    vm.root(v.holdsObj() ? v.o : nullptr);
    bool ins = false;
    const std::uint32_t cell = mapInsert(vm, m, k, v, &ins);
    if (!ins) m->vals[cell] = v;
    c.ret = pairResult(vm, m, cell, ins);
}
void setInsert(Vm& vm, NativeCall& c) {
    MapObj* m = mp(vm, c);
    const Value k = keyOf(vm, c, 1);
    vm.root(k.holdsObj() ? k.o : nullptr);
    bool ins = false;
    const std::uint32_t cell = mapInsert(vm, m, k, Value(), &ins);
    c.ret = pairResult(vm, m, cell, ins);
}
void mFind(Vm& vm, NativeCall& c) { MapObj* m = mp(vm, c); const std::int32_t f = mapFind(vm, m, c.args[1]); c.ret = Value::ptr(m, f < 0 ? End : static_cast<std::uint32_t>(f)); }
void mCount(Vm& vm, NativeCall& c) { c.ret = sz(mapFind(vm, mp(vm, c), c.args[1]) >= 0 ? 1 : 0); }
void mContains(Vm& vm, NativeCall& c) { c.ret = Value::boolean(mapFind(vm, mp(vm, c), c.args[1]) >= 0); }
void mEraseKey(Vm& vm, NativeCall& c) { c.ret = sz(mapErase(vm, mp(vm, c), c.args[1]) ? 1 : 0); }
void mEraseIt(Vm& vm, NativeCall& c) {
    MapObj* m = mp(vm, c);
    const Value it = c.args[1];
    if (it.k != VK::Ptr || it.o != m || it.idx == End || it.idx >= m->keys.size() || !m->used[it.idx]) vm.fault(FaultKind::Index, "erase con un iteratore non valido");
    const std::uint32_t next = mapNext(vm, m, it.idx);
    mapEraseCell(vm, m, it.idx);
    c.ret = Value::ptr(m, next);
}
void mSize(Vm& vm, NativeCall& c) { c.ret = sz(mp(vm, c)->count); }
void mEmpty(Vm& vm, NativeCall& c) { c.ret = Value::boolean(mp(vm, c)->count == 0); }
void mClear(Vm& vm, NativeCall& c) { mapClear(mp(vm, c)); }
void mBegin(Vm& vm, NativeCall& c) { MapObj* m = mp(vm, c); c.ret = Value::ptr(m, mapBegin(m)); }
void mEnd(Vm& vm, NativeCall& c) { c.ret = Value::ptr(mp(vm, c), End); }
void mLower(Vm& vm, NativeCall& c) { MapObj* m = mp(vm, c); c.ret = Value::ptr(m, mapBound(vm, m, c.args[1], false)); }
void mUpper(Vm& vm, NativeCall& c) { MapObj* m = mp(vm, c); c.ret = Value::ptr(m, mapBound(vm, m, c.args[1], true)); }
void pairCtor(Vm& vm, NativeCall& c) {
    InstObj* p = instArg(vm, c.args[0]);
    p->fields[0] = stored(vm, c.args[1], typeArg(c, 0));
    p->fields[1] = stored(vm, c.args[2], typeArg(c, 1));
}
void makePair(Vm& vm, NativeCall& c) {
    Value a = c.args[0], b = c.args[1];
    auto fix = [&](Value& v) {
        if (v.holdsObj() && v.o->kind == OK::Array && static_cast<ArrayObj*>(v.o)->ek == VK::C8) v = Value::ref(vm.newStrB(vm.utf8(v)));
        else if (v.holdsObj() && (v.k == VK::Ref) && (v.o->kind == OK::StrB || v.o->kind == OK::Vec || v.o->kind == OK::Map || (v.o->kind == OK::Inst && static_cast<InstObj*>(v.o)->cls && static_cast<InstObj*>(v.o)->cls->valueType))) v = vm.copyValue(v);
        vm.root(v.holdsObj() ? v.o : nullptr);
    };
    fix(a);
    fix(b);
    c.ret = Value::ref(vm.newPair(c.type, a, b));
}

// ================================================================ algoritmi
void aSort(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    std::vector<Value> items;
    items.reserve(e - b);
    for (std::uint32_t i = b; i < e; ++i) items.push_back(s.get(vm, i));
    const Value* cmp = c.argc > 2 ? &c.args[2] : nullptr;
    stableSort(vm, items, [&](const Value& x, const Value& y) { return lessWith(vm, cmp, x, y, false); });
    for (std::uint32_t i = b; i < e; ++i) s.set(vm, i, items[i - b]);
}
void aReverse(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    while (b + 1 < e) { const Value x = s.get(vm, b), y = s.get(vm, e - 1); s.set(vm, b, y); s.set(vm, e - 1, x); ++b; --e; }
}
Value itAtPos(const Value& first, std::uint32_t i) { Value r = first; if (r.k == VK::Ref) r = Value::ptr(r.o, 0); r.idx = i; return r; }
void aFind(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    for (std::uint32_t i = b; i < e; ++i) if (vm.equalValues(s.get(vm, i), c.args[2])) { c.ret = itAtPos(c.args[0], i); return; }
    c.ret = c.args[1];
}
void aFindIf(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    for (std::uint32_t i = b; i < e; ++i) { Value x = s.get(vm, i); if (vm.truthy(vm.invoke(c.args[2], &x, 1))) { c.ret = itAtPos(c.args[0], i); return; } }
    c.ret = c.args[1];
}
void aCount(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    std::int64_t n = 0;
    for (std::uint32_t i = b; i < e; ++i) if (vm.equalValues(s.get(vm, i), c.args[2])) ++n;
    c.ret = Value::i64(n);
}
void aCountIf(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    std::int64_t n = 0;
    for (std::uint32_t i = b; i < e; ++i) { Value x = s.get(vm, i); if (vm.truthy(vm.invoke(c.args[2], &x, 1))) ++n; }
    c.ret = Value::i64(n);
}
template <bool Max> void aMinMaxElement(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    if (b == e) { c.ret = c.args[1]; return; }
    const Value* cmp = c.argc > 2 ? &c.args[2] : nullptr;
    std::uint32_t best = b;
    for (std::uint32_t i = b + 1; i < e; ++i) {
        const Value x = s.get(vm, i), y = s.get(vm, best);
        if (Max ? lessWith(vm, cmp, y, x, false) : lessWith(vm, cmp, x, y, false)) best = i;
    }
    c.ret = itAtPos(c.args[0], best);
}
Value addValues(Vm& vm, const Value& a, const Value& b) {
    if (a.holdsObj() && a.o->kind == OK::StrB) {
        std::string s = vm.utf8(a);
        s += b.k == VK::C8 ? std::string(1, static_cast<char>(b.i)) : vm.utf8(b);
        return Value::ref(vm.newStrB(s));
    }
    if (isFloat(a.k) || isFloat(b.k)) return Value::f64(a.num() + b.num());
    return Value::i64(static_cast<std::int64_t>(static_cast<std::uint64_t>(a.integer()) + static_cast<std::uint64_t>(b.integer())));
}
void aAccumulate(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    Value acc = c.args[2];
    const VK k = acc.k;
    for (std::uint32_t i = b; i < e; ++i) {
        const Value x = s.get(vm, i);
        if (c.argc > 3) { Value args[2] = {acc, x}; acc = vm.invoke(c.args[3], args, 2); }
        else acc = addValues(vm, acc, x);
        if (isNum(k)) acc = vm.convertTo(acc, k);
        vm.root(acc.holdsObj() ? acc.o : nullptr);
    }
    c.ret = acc;
}
void aFill(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    const TypeP et = s.o->kind == OK::Array ? static_cast<ArrayObj*>(s.o)->elem : s.o->kind == OK::Vec ? typeArgOf(s.o->type, 0) : nullptr;
    for (std::uint32_t i = b; i < e; ++i) s.set(vm, i, stored(vm, c.args[2], et));
}
void aIota(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    Value v = c.args[2];
    for (std::uint32_t i = b; i < e; ++i) {
        s.set(vm, i, v);
        v = isFloat(v.k) ? Value::f64(v.d + 1) : vm.convertTo(Value::i64(v.i + 1), v.k);
    }
}
std::uint32_t boundPos(Vm& vm, NativeCall& c, bool upper) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    const Value* cmp = c.argc > 3 ? &c.args[3] : nullptr;
    std::uint32_t lo = b, hi = e;
    while (lo < hi) {
        const std::uint32_t mid = lo + (hi - lo) / 2;
        const Value x = s.get(vm, mid);
        const bool go = upper ? !lessWith(vm, cmp, c.args[2], x, false) : lessWith(vm, cmp, x, c.args[2], false);
        if (go) lo = mid + 1; else hi = mid;
    }
    return lo;
}
void aLowerBound(Vm& vm, NativeCall& c) { c.ret = itAtPos(c.args[0], boundPos(vm, c, false)); }
void aUpperBound(Vm& vm, NativeCall& c) { c.ret = itAtPos(c.args[0], boundPos(vm, c, true)); }
void aBinarySearch(Vm& vm, NativeCall& c) {
    const std::uint32_t p = boundPos(vm, c, false);
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    const Value* cmp = c.argc > 3 ? &c.args[3] : nullptr;
    c.ret = Value::boolean(p < e && !lessWith(vm, cmp, c.args[2], s.get(vm, p), false));
}
void aUnique(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    if (b == e) { c.ret = c.args[1]; return; }
    std::uint32_t w = b + 1;
    for (std::uint32_t i = b + 1; i < e; ++i) {
        const Value x = s.get(vm, i);
        if (!vm.equalValues(x, s.get(vm, w - 1))) s.set(vm, w++, x);
    }
    c.ret = itAtPos(c.args[0], w);
}
void aRemove(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    std::uint32_t w = b;
    for (std::uint32_t i = b; i < e; ++i) { const Value x = s.get(vm, i); if (!vm.equalValues(x, c.args[2])) s.set(vm, w++, x); }
    c.ret = itAtPos(c.args[0], w);
}
void aRemoveIf(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    std::uint32_t w = b;
    for (std::uint32_t i = b; i < e; ++i) { Value x = s.get(vm, i); if (!vm.truthy(vm.invoke(c.args[2], &x, 1))) s.set(vm, w++, x); }
    c.ret = itAtPos(c.args[0], w);
}
void aCopy(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    Seq d = seqOf(vm, c.args[2].k == VK::Ptr ? Value::ref(c.args[2].o) : c.args[2]);
    std::uint32_t at = c.args[2].k == VK::Ptr && c.args[2].idx != Whole ? c.args[2].idx : 0;
    std::vector<Value> tmp;
    for (std::uint32_t i = b; i < e; ++i) tmp.push_back(s.get(vm, i));
    for (const Value& x : tmp) d.set(vm, at++, x);
    c.ret = itAtPos(c.args[2], at);
}
template <int Kind> void aPred(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    bool any = false, all = true;
    for (std::uint32_t i = b; i < e; ++i) {
        Value x = s.get(vm, i);
        const bool r = vm.truthy(vm.invoke(c.args[2], &x, 1));
        any = any || r;
        all = all && r;
    }
    c.ret = Value::boolean(Kind == 0 ? all : Kind == 1 ? any : !any);
}
void aForEach(Vm& vm, NativeCall& c) {
    Seq s;
    std::uint32_t b = 0, e = 0;
    rangeOf(vm, c.args[0], c.args[1], s, b, e);
    for (std::uint32_t i = b; i < e; ++i) { Value x = s.o->kind == OK::Vec || s.o->kind == OK::Array || s.o->kind == OK::StrB ? Value::ptr(s.o, i) : s.get(vm, i); Value v = vm.loadPtr(x); vm.invoke(c.args[2], &v, 1); }
}
void aSwap(Vm& vm, NativeCall& c) {
    const Value a = c.args[0], b = c.args[1];
    if (a.k == VK::Ptr && a.f != 2 && a.idx == Whole && b.k == VK::Ptr && b.f != 2 && b.idx == Whole) {
        // oggetti per valore: si scambia il contenuto
        const Value tmp = vm.copyValue(Value::ref(a.o));
        vm.root(tmp.holdsObj() ? tmp.o : nullptr);
        vm.copyInto(Value::ref(a.o), Value::ref(b.o));
        vm.copyInto(Value::ref(b.o), tmp);
        return;
    }
    const Value x = vm.loadPtr(a), y = vm.loadPtr(b);
    vm.storePtr(a, y);
    vm.storePtr(b, x);
}
template <bool Max> void aMinMax(Vm& vm, NativeCall& c) {
    const Value* cmp = c.argc > 2 ? &c.args[2] : nullptr;
    const Value& a = c.args[0];
    const Value& b = c.args[1];
    Value r = Max ? (lessWith(vm, cmp, a, b, false) ? b : a) : (lessWith(vm, cmp, b, a, false) ? b : a);
    if (isNum(a.k) && isNum(r.k)) r = vm.convertTo(r, isFloat(b.k) && !isFloat(a.k) ? b.k : a.k);
    c.ret = r;
}
void aMove(Vm&, NativeCall& c) { c.ret = c.args[0]; }
void aAbsI(Vm&, NativeCall& c) { const std::int64_t v = I(c, 0); c.ret = Value::i32(v < 0 ? -v : v); }
void aAbsL(Vm&, NativeCall& c) { const std::int64_t v = I(c, 0); c.ret = Value::i64(v < 0 ? -v : v); }
void aAbsD(Vm&, NativeCall& c) { c.ret = Value::f64(std::fabs(D(c, 0))); }
void aAbsF(Vm&, NativeCall& c) { c.ret = Value::f32(std::fabs(D(c, 0))); }

constexpr std::uint8_t P = NF::Property, St = NF::Static, Ct = NF::Ctor;
#define STD(scope, name, sig, fn) {L::Cpp, scope, "", "std::" name, sig, fn, St}
#define M(owner, name, sig, fn) {L::Cpp, owner, owner, name, sig, fn, 0}
constexpr const char* kAlgo = "algorithm|utility|iostream|string|vector|cmath|map|set|numeric|functional|deque|list";
const NativeEntry kCpp[] = {
    // std::string
    {L::Cpp, "string", "string", "#ctor", "void(cstr)", sCtorCstr, Ct},
    {L::Cpp, "string", "string", "#ctor", "void(string)", sCtorStr, Ct},
    {L::Cpp, "string", "string", "#ctor", "void(size_t,char)", sCtorFill, Ct},
    {L::Cpp, "string", "string", "#ctor", "void(string,size_t)", sCtorSub, Ct},
    {L::Cpp, "string", "string", "#ctor", "void(string,size_t,size_t)", sCtorSub, Ct},
    {L::Cpp, "string", "string", "#ctor", "void(cstr,size_t)", sCtorCount, Ct},
    {L::Cpp, "string", "string", "#fromcstr", "string(cstr)", sFromCstr, St},
    {L::Cpp, "string", "string", "#fromchar", "string(char)", sFromChar, St},
    M("string", "size", "size_t()", sSize), M("string", "length", "size_t()", sSize), M("string", "empty", "bool()", sEmpty),
    M("string", "operator[]", "char&(size_t)", sIndex), M("string", "at", "char&(size_t)", sAt),
    M("string", "front", "char&()", sFront), M("string", "back", "char&()", sBack),
    M("string", "c_str", "cstr()", sCstr), M("string", "data", "cstr()", sCstr),
    M("string", "substr", "string()", sSubstr), M("string", "substr", "string(size_t)", sSubstr), M("string", "substr", "string(size_t,size_t)", sSubstr),
    M("string", "find", "size_t(string)", sFind<0>), M("string", "find", "size_t(string,size_t)", sFind<0>),
    M("string", "find", "size_t(char)", sFind<0>), M("string", "find", "size_t(char,size_t)", sFind<0>),
    M("string", "rfind", "size_t(string)", sFind<1>), M("string", "rfind", "size_t(string,size_t)", sFind<1>),
    M("string", "rfind", "size_t(char)", sFind<1>), M("string", "rfind", "size_t(char,size_t)", sFind<1>),
    M("string", "find_first_of", "size_t(string)", sFind<2>), M("string", "find_first_of", "size_t(string,size_t)", sFind<2>), M("string", "find_first_of", "size_t(char)", sFind<2>),
    M("string", "find_last_of", "size_t(string)", sFind<3>), M("string", "find_last_of", "size_t(string,size_t)", sFind<3>), M("string", "find_last_of", "size_t(char)", sFind<3>),
    M("string", "find_first_not_of", "size_t(string)", sFind<4>), M("string", "find_first_not_of", "size_t(string,size_t)", sFind<4>), M("string", "find_first_not_of", "size_t(char)", sFind<4>),
    M("string", "find_last_not_of", "size_t(string)", sFind<5>), M("string", "find_last_not_of", "size_t(string,size_t)", sFind<5>), M("string", "find_last_not_of", "size_t(char)", sFind<5>),
    M("string", "replace", "self&(size_t,size_t,string)", sReplace),
    M("string", "insert", "self&(size_t,string)", sInsert), M("string", "insert", "self&(size_t,size_t,char)", sInsertFill),
    M("string", "erase", "self&()", sErase), M("string", "erase", "self&(size_t)", sErase), M("string", "erase", "self&(size_t,size_t)", sErase),
    M("string", "erase", "iter(iter)", sEraseIt), M("string", "erase", "iter(iter,iter)", sEraseRange),
    M("string", "append", "self&(string)", sAppend), M("string", "append", "self&(size_t,char)", sAppendFill),
    M("string", "push_back", "void(char)", sPushBack), M("string", "pop_back", "void()", sPopBack), M("string", "clear", "void()", sClear),
    M("string", "compare", "int(string)", sCompare),
    M("string", "resize", "void(size_t)", sResize), M("string", "resize", "void(size_t,char)", sResize),
    M("string", "reserve", "void(size_t)", sReserve), M("string", "capacity", "size_t()", sCapacity), M("string", "swap", "void(self&)", sSwap),
    M("string", "begin", "iter()", sBegin), M("string", "end", "iter()", sEnd), M("string", "cbegin", "iter()", sBegin), M("string", "cend", "iter()", sEnd),
    M("string", "operator+", "string(string)", sPlus), M("string", "operator+", "string(char)", sPlus),
    M("string", "operator+=", "self&(string)", sPlusEq), M("string", "operator+=", "self&(char)", sPlusEq),
    M("string", "operator==", "bool(string)", sCmp<0>), M("string", "operator!=", "bool(string)", sCmp<1>),
    M("string", "operator<", "bool(string)", sCmp<2>), M("string", "operator<=", "bool(string)", sCmp<3>),
    M("string", "operator>", "bool(string)", sCmp<4>), M("string", "operator>=", "bool(string)", sCmp<5>),
    STD("string", "to_string", "string(int)", toStringI), STD("string", "to_string", "string(long)", toStringI),
    STD("string", "to_string", "string(uint)", toStringI), STD("string", "to_string", "string(ulong)", toStringU),
    STD("string", "to_string", "string(double)", toStringD), STD("string", "to_string", "string(float)", toStringD),
    STD("string", "stoi", "int(string)", stoi_), STD("string", "stoi", "int(string,any)", stoi_), STD("string", "stoi", "int(string,any,int)", stoi_),
    STD("string", "stol", "long(string)", stol_), STD("string", "stol", "long(string,any,int)", stol_),
    STD("string", "stoll", "long(string)", stoll_), STD("string", "stoul", "ulong(string)", stoul_),
    STD("string", "stod", "double(string)", stod_), STD("string", "stod", "double(string,any)", stod_), STD("string", "stof", "float(string)", stof_),
    STD("string|iostream|istream|fstream|sstream", "getline", "any&(any&,string&)", sGetline),
    STD("string|iostream|istream|fstream|sstream", "getline", "any&(any&,string&,char)", sGetlineDelim),
    // flussi in uscita
    M("ostream", "operator<<", "self&(any)", oShl), M("ostream", "operator<<", "self&(manip)", oShl),
    M("ostream", "put", "self&(char)", oPut), M("ostream", "flush", "self&()", oFlush),
    M("ios", "precision", "long()", oPrecision), M("ios", "precision", "long(long)", oPrecision),
    M("ios", "width", "long()", oWidth), M("ios", "width", "long(long)", oWidth),
    M("ios", "fill", "char()", oFill), M("ios", "fill", "char(char)", oFill),
    STD("iomanip", "setw", "manip(int)", mParam<20>), STD("iomanip", "setprecision", "manip(int)", mParam<21>), STD("iomanip", "setfill", "manip(char)", mParam<22>),
    // flussi in ingresso
    M("istream", "operator>>", "self&(bool&)", iShr<VK::Bool>), M("istream", "operator>>", "self&(char&)", iShr<VK::C8>),
    M("istream", "operator>>", "self&(schar&)", iShr<VK::C8>), M("istream", "operator>>", "self&(uchar&)", iShr<VK::C8>),
    M("istream", "operator>>", "self&(short&)", iShr<VK::I16>), M("istream", "operator>>", "self&(ushort&)", iShr<VK::U16>),
    M("istream", "operator>>", "self&(int&)", iShr<VK::I32>), M("istream", "operator>>", "self&(uint&)", iShr<VK::U32>),
    M("istream", "operator>>", "self&(long&)", iShr<VK::I64>), M("istream", "operator>>", "self&(ulong&)", iShr<VK::U64>),
    M("istream", "operator>>", "self&(float&)", iShr<VK::F32>), M("istream", "operator>>", "self&(double&)", iShr<VK::F64>),
    M("istream", "operator>>", "self&(string&)", iShrString), M("istream", "operator>>", "self&(charptr)", iShrChars),
    M("istream", "operator>>", "self&(manip)", iShrManip),
    M("istream", "getline", "self&(charptr,long)", iGetlineChars), M("istream", "getline", "self&(charptr,long,char)", iGetlineChars),
    M("istream", "get", "int()", iGet), M("istream", "get", "self&(char&)", iGetChar), M("istream", "peek", "int()", iPeek),
    M("istream", "ignore", "self&()", iIgnore), M("istream", "ignore", "self&(long)", iIgnore), M("istream", "ignore", "self&(long,int)", iIgnore),
    M("istream", "unget", "self&()", iUnget), M("istream", "putback", "self&(char)", iUnget),
    // stato dei flussi
    M("ios", "good", "bool()", iosGood), M("ios", "eof", "bool()", iosEof), M("ios", "fail", "bool()", iosFail), M("ios", "bad", "bool()", iosBad),
    M("ios", "clear", "void()", iosClear), M("ios", "operator!", "bool()", iosNot),
    {L::Cpp, "ios", "ios", "#bool", "bool(self)", iosBool, St},
    // file
    {L::Cpp, "fstream", "ifstream", "#ctor", "void(string)", fsCtor, Ct}, {L::Cpp, "fstream", "ifstream", "#ctor", "void(string,int)", fsCtor, Ct},
    {L::Cpp, "fstream", "ofstream", "#ctor", "void(string)", fsCtor, Ct}, {L::Cpp, "fstream", "ofstream", "#ctor", "void(string,int)", fsCtor, Ct},
    {L::Cpp, "fstream", "fstream", "#ctor", "void(string)", fsCtor, Ct}, {L::Cpp, "fstream", "fstream", "#ctor", "void(string,int)", fsCtor, Ct},
    M("ifstream", "open", "void(string)", fsOpen), M("ifstream", "open", "void(string,int)", fsOpen),
    M("ofstream", "open", "void(string)", fsOpen), M("ofstream", "open", "void(string,int)", fsOpen),
    M("fstream", "open", "void(string)", fsOpen), M("fstream", "open", "void(string,int)", fsOpen),
    M("ifstream", "is_open", "bool()", fsIsOpen), M("ofstream", "is_open", "bool()", fsIsOpen), M("fstream", "is_open", "bool()", fsIsOpen),
    M("ifstream", "close", "void()", fsClose), M("ofstream", "close", "void()", fsClose), M("fstream", "close", "void()", fsClose),
    // stringstream
    {L::Cpp, "sstream", "stringstream", "#ctor", "void(string)", ssCtor, Ct},
    M("stringstream", "str", "string()", ssStr), M("stringstream", "str", "void(string)", ssSetStr),
    // vector (e deque, list per la catena dei proprietari)
    {L::Cpp, "vector", "vector", "#ctor", "void(size_t)", vCtorN, Ct},
    {L::Cpp, "vector", "vector", "#ctor", "void(size_t,T)", vCtorN, Ct},
    {L::Cpp, "vector", "vector", "#ctor", "void(self)", vCtorCopy, Ct},
    {L::Cpp, "vector", "vector", "#ctor", "void(iter,iter)", vCtorRange, Ct},
    {L::Cpp, "vector", "vector", "#ctor", "void(any,any)", vCtorRange, Ct},
    M("vector", "push_back", "void(T)", vPushBack), M("vector", "emplace_back", "void(T)", vPushBack), M("vector", "pop_back", "void()", vPopBack),
    M("vector", "size", "size_t()", vSize), M("vector", "empty", "bool()", vEmpty),
    M("vector", "operator[]", "T&(size_t)", vIndex), M("vector", "at", "T&(size_t)", vAt),
    M("vector", "front", "T&()", vFront), M("vector", "back", "T&()", vBack), M("vector", "clear", "void()", vClear),
    M("vector", "resize", "void(size_t)", vResize), M("vector", "resize", "void(size_t,T)", vResize),
    M("vector", "reserve", "void(size_t)", vReserve), M("vector", "capacity", "size_t()", vCapacity), M("vector", "shrink_to_fit", "void()", vShrink),
    M("vector", "begin", "iter()", vBegin), M("vector", "end", "iter()", vEnd), M("vector", "cbegin", "iter()", vBegin), M("vector", "cend", "iter()", vEnd),
    M("vector", "insert", "iter(iter,T)", vInsert), M("vector", "insert", "iter(iter,size_t,T)", vInsertN), M("vector", "emplace", "iter(iter,T)", vInsert),
    M("vector", "erase", "iter(iter)", vErase), M("vector", "erase", "iter(iter,iter)", vEraseRange),
    M("vector", "assign", "void(size_t,T)", vAssign), M("vector", "swap", "void(self&)", vSwap),
    M("deque", "push_front", "void(T)", vPushFront), M("deque", "emplace_front", "void(T)", vPushFront), M("deque", "pop_front", "void()", vPopFront),
    M("list", "sort", "void()", lSort), M("list", "sort", "void(any)", lSort), M("list", "reverse", "void()", lReverse),
    M("list", "remove", "void(T)", lRemove), M("list", "unique", "void()", lUnique),
    M("stack", "push", "void(T)", vPushBack), M("stack", "emplace", "void(T)", vPushBack), M("stack", "pop", "void()", vPopBack),
    M("stack", "top", "T&()", vBack), M("stack", "size", "size_t()", vSize), M("stack", "empty", "bool()", vEmpty),
    M("queue", "push", "void(T)", vPushBack), M("queue", "emplace", "void(T)", vPushBack), M("queue", "pop", "void()", vPopFront),
    M("queue", "front", "T&()", vFront), M("queue", "back", "T&()", vBack), M("queue", "size", "size_t()", vSize), M("queue", "empty", "bool()", vEmpty),
    // map (e unordered_map)
    M("map", "operator[]", "V&(K)", mIndex), M("map", "at", "V&(K)", mAt),
    M("map", "insert", "pairib(pairkv)", mInsertPair), M("map", "emplace", "pairib(K,V)", mEmplace), M("map", "insert_or_assign", "pairib(K,V)", mInsertOrAssign),
    M("map", "find", "mapiter(K)", mFind), M("map", "count", "size_t(K)", mCount), M("map", "contains", "bool(K)", mContains),
    M("map", "erase", "size_t(K)", mEraseKey), M("map", "erase", "mapiter(mapiter)", mEraseIt),
    M("map", "size", "size_t()", mSize), M("map", "empty", "bool()", mEmpty), M("map", "clear", "void()", mClear),
    M("map", "begin", "mapiter()", mBegin), M("map", "end", "mapiter()", mEnd), M("map", "cbegin", "mapiter()", mBegin), M("map", "cend", "mapiter()", mEnd),
    M("map", "lower_bound", "mapiter(K)", mLower), M("map", "upper_bound", "mapiter(K)", mUpper),
    // set (e unordered_set)
    M("set", "insert", "pairib(K)", setInsert), M("set", "emplace", "pairib(K)", setInsert),
    M("set", "find", "mapiter(K)", mFind), M("set", "count", "size_t(K)", mCount), M("set", "contains", "bool(K)", mContains),
    M("set", "erase", "size_t(K)", mEraseKey), M("set", "erase", "mapiter(mapiter)", mEraseIt),
    M("set", "size", "size_t()", mSize), M("set", "empty", "bool()", mEmpty), M("set", "clear", "void()", mClear),
    M("set", "begin", "mapiter()", mBegin), M("set", "end", "mapiter()", mEnd),
    M("set", "lower_bound", "mapiter(K)", mLower), M("set", "upper_bound", "mapiter(K)", mUpper),
    // pair
    {L::Cpp, "utility", "pair", "#ctor", "void(T,V)", pairCtor, Ct},
    STD("utility|map|algorithm|iostream", "make_pair", "pair<arg0,arg1>(any,any)", makePair),
    // algoritmi
    STD("algorithm", "sort", "void(any,any)", aSort), STD("algorithm", "sort", "void(any,any,any)", aSort),
    STD("algorithm", "stable_sort", "void(any,any)", aSort), STD("algorithm", "stable_sort", "void(any,any,any)", aSort),
    STD("algorithm", "reverse", "void(any,any)", aReverse),
    STD("algorithm", "find", "arg0(any,any,any)", aFind), STD("algorithm", "find_if", "arg0(any,any,any)", aFindIf),
    STD("algorithm", "count", "long(any,any,any)", aCount), STD("algorithm", "count_if", "long(any,any,any)", aCountIf),
    STD("algorithm", "min_element", "arg0(any,any)", aMinMaxElement<false>), STD("algorithm", "min_element", "arg0(any,any,any)", aMinMaxElement<false>),
    STD("algorithm", "max_element", "arg0(any,any)", aMinMaxElement<true>), STD("algorithm", "max_element", "arg0(any,any,any)", aMinMaxElement<true>),
    STD("numeric", "accumulate", "arg2(any,any,any)", aAccumulate), STD("numeric", "accumulate", "arg2(any,any,any,any)", aAccumulate),
    STD("algorithm", "fill", "void(any,any,any)", aFill), STD("numeric", "iota", "void(any,any,any)", aIota),
    STD("algorithm", "lower_bound", "arg0(any,any,any)", aLowerBound), STD("algorithm", "upper_bound", "arg0(any,any,any)", aUpperBound),
    STD("algorithm", "lower_bound", "arg0(any,any,any,any)", aLowerBound), STD("algorithm", "upper_bound", "arg0(any,any,any,any)", aUpperBound),
    STD("algorithm", "binary_search", "bool(any,any,any)", aBinarySearch), STD("algorithm", "binary_search", "bool(any,any,any,any)", aBinarySearch),
    STD("algorithm", "unique", "arg0(any,any)", aUnique), STD("algorithm", "remove", "arg0(any,any,any)", aRemove), STD("algorithm", "remove_if", "arg0(any,any,any)", aRemoveIf),
    STD("algorithm", "copy", "arg2(any,any,any)", aCopy),
    STD("algorithm", "all_of", "bool(any,any,any)", aPred<0>), STD("algorithm", "any_of", "bool(any,any,any)", aPred<1>), STD("algorithm", "none_of", "bool(any,any,any)", aPred<2>),
    STD("algorithm", "for_each", "void(any,any,any)", aForEach),
    {L::Cpp, kAlgo, "", "std::swap", "void(any&,any&)", aSwap, St},
    {L::Cpp, kAlgo, "", "std::min", "arg0(any,any)", aMinMax<false>, St}, {L::Cpp, kAlgo, "", "std::max", "arg0(any,any)", aMinMax<true>, St},
    {L::Cpp, kAlgo, "", "std::min", "arg0(any,any,any)", aMinMax<false>, St}, {L::Cpp, kAlgo, "", "std::max", "arg0(any,any,any)", aMinMax<true>, St},
    {L::Cpp, kAlgo, "", "std::move", "arg0(any)", aMove, St},
    {L::Cpp, "cmath|cstdlib|cmath", "", "std::abs", "int(int)", aAbsI, St}, {L::Cpp, "cmath|cstdlib", "", "std::abs", "long(long)", aAbsL, St},
    {L::Cpp, "cmath|cstdlib", "", "std::abs", "double(double)", aAbsD, St}, {L::Cpp, "cmath|cstdlib", "", "std::abs", "float(float)", aAbsF, St},
};
#undef STD
#undef M
} // namespace

const NativeEntry* libCppEntries(std::size_t& n) { n = sizeof(kCpp) / sizeof(kCpp[0]); return kCpp; }

} // namespace fx::prog
