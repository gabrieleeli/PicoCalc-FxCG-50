// Tabella delle funzioni native (v1.20.0) e sua consultazione dal
// compilatore: firme in testo ("double(double)", "T&(size_t)"), proprietari
// dei metodi (string, vector, List...), intestazioni standard visibili (con
// quelle incluse da altre), costanti della libreria (INT_MAX, M_PI,
// int.MaxValue, std::endl...). Le voci sono in program_lib*.cpp e program_fx.cpp.
#include "program_compile.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace fx::prog {

// Voci dei file delle librerie.
const NativeEntry* libCoreEntries(std::size_t& n);
const NativeEntry* libCEntries(std::size_t& n);
const NativeEntry* libCppEntries(std::size_t& n);
const NativeEntry* libCsEntries(std::size_t& n);
const NativeEntry* libFxEntries(std::size_t& n);
const NativeEntry* libJavaEntries(std::size_t& n);

namespace {
const std::vector<NativeEntry>& table() {
    static const std::vector<NativeEntry> t = [] {
        std::vector<NativeEntry> v;
        auto add = [&](const NativeEntry* e, std::size_t n) { for (std::size_t i = 0; i < n; ++i) v.push_back(e[i]); };
        std::size_t n = 0;
        const NativeEntry* e = libCoreEntries(n); add(e, n);
        e = libCEntries(n); add(e, n);
        e = libCppEntries(n); add(e, n);
        e = libCsEntries(n); add(e, n);
        e = libFxEntries(n); add(e, n);
        e = libJavaEntries(n); add(e, n);
        return v;
    }();
    return t;
}
const NativeTypeEntry kTypes[] = {
    {L::Cpp, "string", "string", NK::None, 0},
    {L::Cpp, "vector", "vector", NK::Vector, 1},
    {L::Cpp, "map", "map", NK::Map, 2},
    {L::Cpp, "set", "set", NK::Set, 1},
    {L::Cpp, "unordered_map", "unordered_map", NK::UMap, 2},
    {L::Cpp, "unordered_set", "unordered_set", NK::USet, 1},
    {L::Cpp, "utility", "pair", NK::Pair, 2},
    {L::Cpp, "stack", "stack", NK::Stack, 1},
    {L::Cpp, "queue", "queue", NK::Queue, 1},
    {L::Cpp, "deque", "deque", NK::Deque, 1},
    {L::Cpp, "list", "list", NK::List, 1},
    {L::Cpp, "iostream", "ostream", NK::OStream, 0},
    {L::Cpp, "iostream", "istream", NK::IStream, 0},
    {L::Cpp, "fstream", "ifstream", NK::IFStream, 0},
    {L::Cpp, "fstream", "ofstream", NK::OFStream, 0},
    {L::Cpp, "fstream", "fstream", NK::FStream, 0},
    {L::Cpp, "sstream", "stringstream", NK::StringStream, 0},
    {L::Cpp, "sstream", "istringstream", NK::StringStream, 0},
    {L::Cpp, "sstream", "ostringstream", NK::StringStream, 0},
    {L::Cpp, "functional", "greater", NK::Greater, 1},
    {L::Cpp, "functional", "less", NK::Less, 1},
    {L::Cs, "System.Collections.Generic", "List", NK::CsList, 1},
    {L::Cs, "System.Collections.Generic", "Dictionary", NK::CsDict, 2},
    {L::Cs, "System.Collections.Generic", "HashSet", NK::CsHashSet, 1},
    {L::Cs, "System.Collections.Generic", "Queue", NK::CsQueue, 1},
    {L::Cs, "System.Collections.Generic", "Stack", NK::CsStack, 1},
    {L::Cs, "System.Collections.Generic", "KeyValuePair", NK::CsKvp, 2},
    // interfacce delle collezioni come parametri (IList<double> x): sono le collezioni stesse
    {L::Cs, "System.Collections.Generic", "IList", NK::CsList, 1},
    {L::Cs, "System.Collections.Generic", "ICollection", NK::CsList, 1},
    {L::Cs, "System.Collections.Generic", "IEnumerable", NK::CsList, 1},
    {L::Cs, "System.Collections.Generic", "IReadOnlyList", NK::CsList, 1},
    {L::Cs, "System.Collections.Generic", "IReadOnlyCollection", NK::CsList, 1},
    {L::Cs, "System.Collections.Generic", "IDictionary", NK::CsDict, 2},
    {L::Cs, "System.Collections.Generic", "IReadOnlyDictionary", NK::CsDict, 2},
    {L::Cs, "System.Collections.Generic", "ISet", NK::CsHashSet, 1},
    {L::Cs, "System.Text", "StringBuilder", NK::CsStringBuilder, 0},
    {L::Cs, "System.IO", "StreamReader", NK::CsStreamReader, 0},
    {L::Cs, "System.IO", "StreamWriter", NK::CsStreamWriter, 0},
    {L::Cs, "System", "Random", NK::CsRandom, 0},
    {L::Java, "java.util", "ArrayList", NK::JArrayList, 1},
    {L::Java, "java.util", "LinkedList", NK::JLinkedList, 1},
    {L::Java, "java.util", "HashMap", NK::JHashMap, 2},
    {L::Java, "java.util", "TreeMap", NK::JTreeMap, 2},
    {L::Java, "java.util", "HashSet", NK::JHashSet, 1},
    {L::Java, "java.util", "TreeSet", NK::JTreeSet, 1},
    {L::Java, "java.util", "ArrayDeque", NK::JArrayDeque, 1},
    {L::Java, "java.lang", "StringBuilder", NK::JStringBuilder, 0},
    {L::Java, "java.util", "Scanner", NK::JScanner, 0},
    {L::Java, "java.util", "Random", NK::JRandom, 0},
};
// Intestazioni standard di C e C++ (e quelle che includono).
struct HeaderInfo { std::uint8_t langs; const char* name; const char* implies; };
const HeaderInfo kHeaders[] = {
    {L::CC, "stdio.h", ""}, {L::CC, "stdlib.h", ""}, {L::CC, "string.h", ""}, {L::CC, "math.h", ""}, {L::CC, "ctype.h", ""},
    {L::CC, "time.h", ""}, {L::CC, "stdbool.h", ""}, {L::CC, "stdint.h", ""}, {L::CC, "stddef.h", ""}, {L::CC, "limits.h", ""},
    {L::CC, "float.h", ""}, {L::CC, "assert.h", ""}, {L::CC, "errno.h", ""}, {L::CC, "inttypes.h", "stdint.h"}, {L::CC, "fx.h", ""},
    {L::Cpp, "fx", "fx.h"},
    {L::Cpp, "cstdio", "stdio.h"}, {L::Cpp, "cstdlib", "stdlib.h"}, {L::Cpp, "cstring", "string.h"}, {L::Cpp, "cmath", "math.h"},
    {L::Cpp, "cctype", "ctype.h"}, {L::Cpp, "ctime", "time.h"}, {L::Cpp, "climits", "limits.h"}, {L::Cpp, "cfloat", "float.h"},
    {L::Cpp, "cstdint", "stdint.h"}, {L::Cpp, "cstddef", "stddef.h"}, {L::Cpp, "cassert", "assert.h"},
    {L::Cpp, "iostream", "string|ios|ostream|istream"}, {L::Cpp, "ostream", "ios|string"}, {L::Cpp, "istream", "ios|string"},
    {L::Cpp, "ios", ""}, {L::Cpp, "iomanip", "ios"}, {L::Cpp, "string", ""}, {L::Cpp, "vector", ""}, {L::Cpp, "map", "utility"},
    {L::Cpp, "set", ""}, {L::Cpp, "unordered_map", "utility"}, {L::Cpp, "unordered_set", ""}, {L::Cpp, "utility", ""},
    {L::Cpp, "stack", "deque"}, {L::Cpp, "queue", "deque|vector"}, {L::Cpp, "deque", ""}, {L::Cpp, "list", ""},
    {L::Cpp, "algorithm", "utility"}, {L::Cpp, "numeric", ""}, {L::Cpp, "functional", ""}, {L::Cpp, "fstream", "ios|istream|ostream|string"},
    {L::Cpp, "sstream", "ios|istream|ostream|string"}, {L::Cpp, "stdexcept", "exception|string"}, {L::Cpp, "exception", ""},
    {L::Cpp, "iterator", ""}, {L::Cpp, "limits", ""}, {L::Cpp, "random", ""}, {L::Cpp, "chrono", ""},
};
const char* const kUnsupportedHeaders[] = {"thread", "mutex", "atomic", "memory", "regex", "filesystem", "array", "tuple", "optional",
    "variant", "any", "bitset", "complex", "complex.h", "wchar.h", "locale.h", "signal.h", "setjmp.h", "pthread.h", "unistd.h",
    "windows.h", "conio.h", "graphics.h", nullptr};
} // namespace

const NativeEntry* nativeEntries(std::size_t& count) noexcept { count = table().size(); return table().data(); }
const NativeTypeEntry* nativeTypes(std::size_t& count) noexcept { count = sizeof(kTypes) / sizeof(kTypes[0]); return kTypes; }
NativeFn nativeAt(std::int32_t index) noexcept { return table()[static_cast<std::size_t>(index)].fn; }
const NativeEntry& nativeEntry(std::int32_t index) noexcept { return table()[static_cast<std::size_t>(index)]; }
std::int32_t nativeIndex(const char* scope, const char* owner, const char* name, const char* sig) noexcept {
    const auto& t = table();
    for (std::size_t i = 0; i < t.size(); ++i) {
        const NativeEntry& e = t[i];
        if (std::strcmp(e.name, name) != 0 || std::strcmp(e.owner, owner) != 0) continue;
        if (sig && sig[0] && std::strcmp(e.sig, sig) != 0) continue;
        if (scope && scope[0] && !std::strstr(e.scope, scope)) continue;
        return static_cast<std::int32_t>(i);
    }
    return 0;                                          // la voce 0 e' un errore chiaro a tempo di esecuzione
}
bool standardHeader(Dialect d, const std::string& name) noexcept {
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << static_cast<unsigned>(d));
    for (const HeaderInfo& h : kHeaders) if ((h.langs & mask) && name == h.name) return true;
    return false;
}
bool unsupportedHeader(const std::string& name) noexcept {
    for (const char* const* h = kUnsupportedHeaders; *h; ++h) if (name == *h) return true;
    return false;
}
std::vector<std::string> impliedHeaders(Dialect d, const std::string& name) {
    std::vector<std::string> out;
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << static_cast<unsigned>(d));
    for (const HeaderInfo& h : kHeaders) {
        if (!(h.langs & mask) || name != h.name) continue;
        std::string imp = h.implies;
        std::size_t b = 0;
        while (b < imp.size()) {
            std::size_t e = imp.find('|', b);
            if (e == std::string::npos) e = imp.size();
            if (e > b) out.push_back(imp.substr(b, e - b));
            b = e + 1;
        }
    }
    return out;
}

// ================================================================ compilatore: tipi delle firme
TypeP Compiler::parseSig(const char* sig, std::vector<TypeP>& params, bool& variadic, TypeP receiver, Dialect d, std::vector<bool>* refs) {
    params.clear();
    variadic = false;
    if (refs) refs->clear();
    const std::string s = sig;
    const std::size_t lp = s.find('(');
    const std::string retName = s.substr(0, lp);
    const std::string inner = lp == std::string::npos ? std::string() : s.substr(lp + 1, s.rfind(')') - lp - 1);
    TypeP recvBase = receiver ? stripRef(receiver) : nullptr;
    if (recvBase && recvBase->k == TK::Ptr) recvBase = recvBase->elem;
    // argomenti di tipo del ricevente (per gli iteratori: quelli del contenitore)
    std::vector<TypeP> targs;
    if (recvBase && recvBase->k == TK::Native) {
        targs = recvBase->args;
        if ((recvBase->nk == NK::Iter || recvBase->nk == NK::MapIter || recvBase->nk == NK::MapEntry) && !recvBase->args.empty() && recvBase->args[0]->k == TK::Native) targs = recvBase->args[0]->args;
        if (recvBase->nk == NK::MapEntry) targs = recvBase->args;
    }
    if (recvBase && (recvBase->k == TK::Nullable || recvBase->k == TK::Array) && recvBase->elem) targs = {recvBase->elem};
    auto one = [&](std::string t) -> TypeP {
        bool ref = false;
        if (!t.empty() && t.back() == '&') { ref = true; t.pop_back(); }
        TypeP r = nullptr;
        const bool cs = d == Dialect::CSharp || d == Dialect::Java;
        if (t == "void") r = tVoid_;
        else if (t == "bool") r = tBool_;
        else if (t == "char") r = charType(d);
        else if (t == "schar") r = tI8_;
        else if (t == "uchar" || t == "byte") r = tU8_;
        else if (t == "short") r = tI16_;
        else if (t == "ushort") r = tU16_;
        else if (t == "int") r = tI32_;
        else if (t == "uint") r = tU32_;
        else if (t == "long" || t == "llong") r = tI64_;
        else if (t == "ulong" || t == "size_t") r = tU64_;
        else if (t == "float") r = tF32_;
        else if (t == "double") r = tF64_;
        else if (t == "string") r = d == Dialect::Cpp ? tCppStr_ : d == Dialect::C ? tCStr_ : tCsStr_;
        else if (t == "cstr" || t == "charptr") r = tCStr_;
        else if (t == "voidptr") r = tVoidPtr_;
        else if (t == "file") r = ptrTo(nativeOf(NK::CFile, {}));
        else if (t == "any" || t == "func") r = tDyn_;
        else if (t == "anyiter") r = tAnyIter_;
        else if (t == "object") r = cs ? tObject_ : tDyn_;
        else if (t == "self") r = recvBase ? recvBase : tDyn_;
        else if (t == "T") r = targs.size() > 0 ? targs[0] : tDyn_;
        else if (t == "K") r = targs.size() > 0 ? targs[0] : tDyn_;
        else if (t == "V") r = targs.size() > 1 ? targs[1] : tDyn_;
        else if (t == "iter") r = recvBase ? nativeOf(NK::Iter, {recvBase}) : tDyn_;
        else if (t == "mapiter") r = recvBase ? nativeOf(NK::MapIter, {recvBase->k == TK::Native && recvBase->nk == NK::MapIter ? recvBase->args[0] : recvBase}) : tDyn_;
        else if (t == "mapentry") r = nativeOf(NK::MapEntry, {targs.size() > 0 ? targs[0] : tDyn_, targs.size() > 1 ? targs[1] : (targs.size() > 0 ? targs[0] : tDyn_)});
        else if (t == "pairib") r = nativeOf(NK::Pair, {recvBase ? nativeOf(NK::MapIter, {recvBase}) : tDyn_, tBool_});
        else if (t == "pairkv") r = nativeOf(NK::Pair, {targs.size() > 0 ? targs[0] : tDyn_, targs.size() > 1 ? targs[1] : tDyn_});
        else if (t == "kvp") r = nativeOf(NK::CsKvp, {targs.size() > 0 ? targs[0] : tDyn_, targs.size() > 1 ? targs[1] : tDyn_});
        else if (t == "manip") r = nativeOf(NK::Manip, {});
        else if (t == "pair<arg0,arg1>") r = nativeOf(NK::Pair, {tArg_[0], tArg_[1]});
        else if (t == "E0") r = tElem_[0];
        else if (t == "cmp<T>") r = funcOf(tI32_, {targs.size() > 0 ? targs[0] : tDyn_, targs.size() > 0 ? targs[0] : tDyn_});
        else if (t == "pred<T>") r = funcOf(tBool_, {targs.size() > 0 ? targs[0] : tDyn_});
        else if (t == "action<T>") r = funcOf(tVoid_, {targs.size() > 0 ? targs[0] : tDyn_});
        else if (t == "cmp<E0>") r = funcOf(tI32_, {tElem_[0], tElem_[0]});
        else if (t == "pred<E0>") r = funcOf(tBool_, {tElem_[0]});
        else if (t == "action<E0>") r = funcOf(tVoid_, {tElem_[0]});
        else if (t == "arg0") r = tArg_[0];
        else if (t == "arg1") r = tArg_[1];
        else if (t == "arg2") r = tArg_[2];
        else if (t == "mapderef") {
            // *it di una mappa: la voce (first, second); di un insieme: la chiave
            TypeP cont = recvBase && recvBase->k == TK::Native && !recvBase->args.empty() ? recvBase->args[0] : nullptr;
            if (cont && cont->k == TK::Native && (cont->nk == NK::Set || cont->nk == NK::USet)) r = cont->args.empty() ? tDyn_ : cont->args[0];
            else r = nativeOf(NK::MapEntry, {targs.size() > 0 ? targs[0] : tDyn_, targs.size() > 1 ? targs[1] : tDyn_});
        }
        else if (t == "elem") {
            // elemento di un contenitore o di un iteratore (T per i vector, char per le stringhe)
            TypeP cont = recvBase;
            if (cont && cont->k == TK::Native && (cont->nk == NK::Iter || cont->nk == NK::MapIter) && !cont->args.empty()) cont = cont->args[0];
            if (cont && cont->k == TK::Str) r = charType(d);
            else if (cont && cont->k == TK::Array) r = cont->elem;
            else r = cont && cont->k == TK::Native && !cont->args.empty() ? cont->args[0] : tDyn_;
        }
        else if (t == "ostream") r = nativeOf(NK::OStream, {});
        else if (t == "istream") r = nativeOf(NK::IStream, {});
        else if (t == "array<T>") r = arrayOf(targs.size() > 0 ? targs[0] : tDyn_, -1, 1);
        else if (t == "array<K>") r = arrayOf(targs.size() > 0 ? targs[0] : tDyn_, -1, 1);
        else if (t == "array<V>") r = arrayOf(targs.size() > 1 ? targs[1] : tDyn_, -1, 1);
        else if (t == "array<string>") r = arrayOf(stringType(d), -1, 1);
        else if (t == "array<char>") r = arrayOf(charType(d), -1, 1);
        else if (t == "array<int>") r = arrayOf(tI32_, -1, 1);
        else if (t == "array<double>") r = arrayOf(tF64_, -1, 1);
        else if (t == "array<any>") r = arrayOf(tDyn_, -1, 1);
        else if (t == "array2<double>") r = arrayOf(tF64_, -1, 2);
        else if (t == "array<array<double>>") r = arrayOf(arrayOf(tF64_, -1, 1), -1, 1);
        else if (t == "list<K>") r = nativeOf(d == Dialect::Java ? NK::JArrayList : NK::CsList, {targs.size() > 0 ? targs[0] : tDyn_});
        else if (t == "list<V>") r = nativeOf(d == Dialect::Java ? NK::JArrayList : NK::CsList, {targs.size() > 1 ? targs[1] : tDyn_});
        else if (t == "list<T>") r = nativeOf(d == Dialect::Java ? NK::JArrayList : NK::CsList, {targs.size() > 0 ? targs[0] : tDyn_});
        else if (t == "list<string>") r = nativeOf(d == Dialect::Java ? NK::JArrayList : NK::CsList, {stringType(d)});
        else if (t == "vector<double>") r = nativeOf(NK::Vector, {tF64_});
        else if (t == "vector<vector<double>>") r = nativeOf(NK::Vector, {nativeOf(NK::Vector, {tF64_})});
        else if (t == "vector<string>") r = nativeOf(NK::Vector, {tCppStr_});
        else if (t == "set<K>") r = nativeOf(d == Dialect::Java ? NK::JHashSet : NK::CsHashSet, {targs.size() > 0 ? targs[0] : tDyn_});
        else if (t.rfind("class:", 0) == 0) {
            ClassInfo* c = exceptionClass(d, t.substr(6));
            if (!c) {
                for (const auto& k : prog_.classes) if (k->isNative && k->name == t.substr(6) && k->lang == d) c = k.get();
            }
            r = c ? classType(c) : tDyn_;
        }
        else r = tDyn_;
        if (ref) { if (refs) refs->push_back(true); return refTo(r); }
        if (refs) refs->push_back(false);
        return r;
    };
    // parametri
    std::size_t b = 0;
    int depth = 0;
    std::string cur;
    for (std::size_t i = 0; i <= inner.size(); ++i) {
        const char c = i < inner.size() ? inner[i] : ',';
        if (c == '<') ++depth;
        if (c == '>') --depth;
        if (c == ',' && depth == 0) {
            std::string t = cur;
            while (!t.empty() && t[0] == ' ') t.erase(0, 1);
            if (t == "...") variadic = true;
            else if (!t.empty()) params.push_back(one(t));
            cur.clear();
            continue;
        }
        cur += c;
    }
    (void)b;
    std::vector<bool> saveRefs;
    if (refs) saveRefs = *refs;
    TypeP ret = one(retName);
    if (refs) *refs = saveRefs;
    // il ricevente e' il primo parametro nella firma di un metodo: si toglie
    return ret;
}

// ================================================================ compilatore: proprietari e nomi
std::string Compiler::ownerKey(TypeP t) const {
    if (!t) return "object";
    t = t->k == TK::Ref ? t->elem : t;
    switch (t->k) {
    case TK::Str: return "string";
    case TK::Array: return "array";
    case TK::Nullable: return "nullable";
    case TK::Enum: return "enum";
    case TK::Bool: return "bool";
    case TK::C8: case TK::C16: return "char";
    case TK::I8: return "sbyte";
    case TK::U8: return "byte";
    case TK::I16: return "short";
    case TK::U16: return "ushort";
    case TK::I32: return "int";
    case TK::U32: return "uint";
    case TK::I64: return "long";
    case TK::U64: return "ulong";
    case TK::F32: return "float";
    case TK::F64: return "double";
    case TK::Class: return t->cls && t->cls->isException ? (t->cls->lang == Dialect::Cpp ? "exception" : "Exception") : "object";
    case TK::Ptr: if (t->elem && t->elem->k == TK::Native) return ownerKey(t->elem); return "pointer";
    case TK::Native:
        switch (t->nk) {
        case NK::Vector: return "vector"; case NK::Map: return "map"; case NK::Set: return "set"; case NK::UMap: return "unordered_map";
        case NK::USet: return "unordered_set"; case NK::Pair: return "pair"; case NK::Stack: return "stack"; case NK::Queue: return "queue";
        case NK::Deque: return "deque"; case NK::List: return "list"; case NK::OStream: return "ostream"; case NK::IStream: return "istream";
        case NK::IFStream: return "ifstream"; case NK::OFStream: return "ofstream"; case NK::FStream: return "fstream";
        case NK::StringStream: return "stringstream"; case NK::Iter: return "iterator"; case NK::MapIter: return "mapiterator";
        case NK::MapEntry: return "mapentry"; case NK::Greater: case NK::Less: return "functor"; case NK::CFile: return "FILE";
        case NK::CsList: return "List"; case NK::CsDict: return "Dictionary"; case NK::CsHashSet: return "HashSet"; case NK::CsQueue: return "Queue";
        case NK::CsStack: return "Stack"; case NK::CsKvp: return "KeyValuePair"; case NK::CsStringBuilder: return "StringBuilder";
        case NK::CsStreamReader: return "StreamReader"; case NK::CsStreamWriter: return "StreamWriter"; case NK::CsRandom: return "Random";
        case NK::JArrayList: return "ArrayList"; case NK::JLinkedList: return "LinkedList"; case NK::JHashMap: return "HashMap";
        case NK::JTreeMap: return "TreeMap"; case NK::JHashSet: return "JHashSet"; case NK::JTreeSet: return "TreeSet";
        case NK::JArrayDeque: return "ArrayDeque"; case NK::JStringBuilder: return "JStringBuilder"; case NK::JScanner: return "Scanner";
        case NK::JRandom: return "JRandom"; case NK::JIterator: return "JIterator"; case NK::JMapEntry: return "JMapEntry";
        default: return "object";
        }
    default: return "object";
    }
}
namespace {
// Proprietari "padri" (gli stream derivati hanno i metodi degli stream base).
std::vector<std::string> ownerChain(const std::string& oIn) {
    // alias dei tipi di C# (Int32.Parse = int.Parse...)
    static const char* const kAlias[][2] = {{"Int32", "int"}, {"Int64", "long"}, {"Int16", "short"}, {"Double", "double"}, {"Single", "float"},
        {"String", "string"}, {"Char", "char"}, {"Boolean", "bool"}, {"Byte", "byte"}, {"UInt32", "uint"}, {"UInt64", "ulong"}, {"SByte", "sbyte"}};
    std::string o = oIn;
    for (const auto& a : kAlias) if (o == a[0]) o = a[1];
    if (o == "ifstream") return {"ifstream", "istream", "ios"};
    if (o == "ofstream") return {"ofstream", "ostream", "ios"};
    if (o == "fstream") return {"fstream", "ifstream", "ofstream", "istream", "ostream", "ios"};
    if (o == "stringstream") return {"stringstream", "istream", "ostream", "ios"};
    if (o == "istream" || o == "ostream") return {o, "ios"};
    if (o == "deque") return {"deque", "vector"};
    if (o == "list") return {"list", "deque", "vector"};
    if (o == "unordered_map") return {"unordered_map", "map"};
    if (o == "unordered_set") return {"unordered_set", "set"};
    if (o == "LinkedList") return {"LinkedList", "ArrayList"};
    if (o == "ArrayDeque") return {"ArrayDeque", "LinkedList", "ArrayList"};
    if (o == "TreeMap") return {"TreeMap", "HashMap"};
    if (o == "TreeSet") return {"TreeSet", "JHashSet"};
    return {o};
}
} // namespace
bool Compiler::headerVisible(const char* scope) const {
    if (!scope || !scope[0]) return true;
    // C: le funzioni della libreria standard sono note anche senza #include
    // (come i builtin di GCC, che le compila con un avviso); il C++ vuole l'intestazione.
    if (lang_ == Dialect::C) return true;
    // intestazioni incluse dall'unita' (con quelle che portano con se')
    std::vector<std::string> all = headers_;
    for (std::size_t i = 0; i < all.size(); ++i) for (const std::string& h : impliedHeaders(lang_, all[i])) if (std::find(all.begin(), all.end(), h) == all.end()) all.push_back(h);
    const std::string s = scope;
    std::size_t b = 0;
    while (b < s.size()) {
        std::size_t e = s.find('|', b);
        if (e == std::string::npos) e = s.size();
        const std::string h = s.substr(b, e - b);
        if (std::find(all.begin(), all.end(), h) != all.end()) return true;
        b = e + 1;
    }
    return false;
}
std::vector<std::int32_t> Compiler::nativeFunctions(const std::string& nm, bool qualifiedStd) {
    std::vector<std::int32_t> out;
    std::size_t count = 0;
    const NativeEntry* es = nativeEntries(count);
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << static_cast<unsigned>(lang_));
    bool usingStd = false;
    for (Namespace* ns = ns_; ns && !usingStd; ns = ns->parent) for (Namespace* u : ns->usings) if (u->name == "std") usingStd = true;
    std::vector<std::string> names;
    if (lang_ == Dialect::Cpp) {
        if (qualifiedStd) { names.push_back(nm); names.push_back(nm.substr(5)); }
        else { names.push_back(nm); if (usingStd) names.push_back("std::" + nm); }
    } else names.push_back(nm);
    for (std::size_t i = 0; i < count; ++i) {
        const NativeEntry& e = es[i];
        if (!(e.langs & mask) || e.owner[0]) continue;
        bool match = false;
        for (const std::string& n : names) if (n == e.name) match = true;
        if (!match) continue;
        // std::printf: le funzioni del C dalle intestazioni <cXXX>
        if (qualifiedStd && std::strncmp(e.name, "std::", 5) != 0 && !std::strstr(e.scope, "|c")) continue;
        if ((lang_ == Dialect::C || lang_ == Dialect::Cpp) && !headerVisible(e.scope)) continue;
        out.push_back(static_cast<std::int32_t>(i));
    }
    return out;
}
std::vector<std::int32_t> Compiler::nativeMembers(const std::string& owner, const std::string& nm, std::uint8_t needFlags, std::uint8_t avoidFlags) {
    std::vector<std::int32_t> out;
    std::size_t count = 0;
    const NativeEntry* es = nativeEntries(count);
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << static_cast<unsigned>(lang_));
    for (const std::string& o : ownerChain(owner)) {
        for (std::size_t i = 0; i < count; ++i) {
            const NativeEntry& e = es[i];
            if (!(e.langs & mask) || o != e.owner || nm != e.name) continue;
            if ((e.flags & needFlags) != needFlags || (e.flags & avoidFlags)) continue;
            out.push_back(static_cast<std::int32_t>(i));
        }
        if (!out.empty()) break;
    }
    return out;
}
std::vector<std::int32_t> Compiler::nativeCtors(TypeP t) {
    return nativeMembers(ownerKey(t), "#ctor", NF::Ctor, 0);
}
std::string Compiler::headerHint(const std::string& nm) const {
    std::size_t count = 0;
    const NativeEntry* es = nativeEntries(count);
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << static_cast<unsigned>(lang_));
    for (std::size_t i = 0; i < count; ++i) {
        const NativeEntry& e = es[i];
        if (!(e.langs & mask) || e.owner[0]) continue;
        if (nm == e.name || ("std::" + nm) == e.name) {
            std::string sc = e.scope;
            const std::size_t bar = sc.find('|');
            if (lang_ == Dialect::Cpp && bar != std::string::npos) sc = sc.substr(bar + 1);
            else if (bar != std::string::npos) sc = sc.substr(0, bar);
            if (std::string(e.name).rfind("std::", 0) == 0 && nm.rfind("std::", 0) != 0) return "std:: (o using namespace std) e #include <" + sc + ">";
            return "#include <" + sc + ">";
        }
    }
    return {};
}

// Costanti della libreria.
bool Compiler::nativeConstant(const std::string& nm, EV& out, ast::Pos at) {
    (void)at;
    struct C { std::uint8_t langs; const char* scope; const char* name; char kind; double v; };
    static const C kConst[] = {
        {L::CC, "stdlib.h|cstdlib", "RAND_MAX", 'i', 32767}, {L::CC, "stdio.h|cstdio", "EOF", 'i', -1},
        {L::CC, "stdlib.h|cstdlib", "EXIT_SUCCESS", 'i', 0}, {L::CC, "stdlib.h|cstdlib", "EXIT_FAILURE", 'i', 1},
        {L::CC, "limits.h|climits", "INT_MAX", 'i', 2147483647.0}, {L::CC, "limits.h|climits", "INT_MIN", 'i', -2147483648.0},
        {L::CC, "limits.h|climits", "UINT_MAX", 'u', 4294967295.0}, {L::CC, "limits.h|climits", "LONG_MAX", 'l', 9223372036854775807.0},
        {L::CC, "limits.h|climits", "LONG_MIN", 'l', -9223372036854775808.0}, {L::CC, "limits.h|climits", "LLONG_MAX", 'l', 9223372036854775807.0},
        {L::CC, "limits.h|climits", "LLONG_MIN", 'l', -9223372036854775808.0}, {L::CC, "limits.h|climits", "ULONG_MAX", 'U', 0},
        {L::CC, "limits.h|climits", "CHAR_BIT", 'i', 8}, {L::CC, "limits.h|climits", "CHAR_MAX", 'i', 127}, {L::CC, "limits.h|climits", "CHAR_MIN", 'i', -128},
        {L::CC, "limits.h|climits", "SCHAR_MAX", 'i', 127}, {L::CC, "limits.h|climits", "SCHAR_MIN", 'i', -128}, {L::CC, "limits.h|climits", "UCHAR_MAX", 'i', 255},
        {L::CC, "limits.h|climits", "SHRT_MAX", 'i', 32767}, {L::CC, "limits.h|climits", "SHRT_MIN", 'i', -32768}, {L::CC, "limits.h|climits", "USHRT_MAX", 'i', 65535},
        {L::CC, "float.h|cfloat", "DBL_MAX", 'd', 1.7976931348623157e308}, {L::CC, "float.h|cfloat", "DBL_MIN", 'd', 2.2250738585072014e-308},
        {L::CC, "float.h|cfloat", "DBL_EPSILON", 'd', 2.220446049250313e-16}, {L::CC, "float.h|cfloat", "FLT_MAX", 'f', 3.4028234663852886e38},
        {L::CC, "float.h|cfloat", "FLT_MIN", 'f', 1.1754943508222875e-38}, {L::CC, "float.h|cfloat", "FLT_EPSILON", 'f', 1.1920928955078125e-07},
        {L::CC, "float.h|cfloat", "DBL_DIG", 'i', 15}, {L::CC, "float.h|cfloat", "FLT_DIG", 'i', 6},
        {L::CC, "math.h|cmath", "M_PI", 'd', 3.14159265358979323846}, {L::CC, "math.h|cmath", "M_E", 'd', 2.7182818284590452354},
        {L::CC, "math.h|cmath", "M_PI_2", 'd', 1.57079632679489661923}, {L::CC, "math.h|cmath", "M_PI_4", 'd', 0.78539816339744830962},
        {L::CC, "math.h|cmath", "M_1_PI", 'd', 0.31830988618379067154}, {L::CC, "math.h|cmath", "M_2_PI", 'd', 0.63661977236758134308},
        {L::CC, "math.h|cmath", "M_SQRT2", 'd', 1.41421356237309504880}, {L::CC, "math.h|cmath", "M_SQRT1_2", 'd', 0.70710678118654752440},
        {L::CC, "math.h|cmath", "M_LN2", 'd', 0.69314718055994530942}, {L::CC, "math.h|cmath", "M_LN10", 'd', 2.30258509299404568402},
        {L::CC, "math.h|cmath", "M_LOG2E", 'd', 1.4426950408889634074}, {L::CC, "math.h|cmath", "M_LOG10E", 'd', 0.43429448190325182765},
        {L::CC, "math.h|cmath", "NAN", 'f', std::numeric_limits<double>::quiet_NaN()}, {L::CC, "math.h|cmath", "INFINITY", 'f', std::numeric_limits<double>::infinity()},
        {L::CC, "math.h|cmath", "HUGE_VAL", 'd', std::numeric_limits<double>::infinity()},
        {L::CC, "time.h|ctime", "CLOCKS_PER_SEC", 'l', 1000000},
        {L::C, "", "true", 'i', 1}, {L::C, "", "false", 'i', 0},
        // C#
        {L::Cs, "", "int.MaxValue", 'i', 2147483647.0}, {L::Cs, "", "int.MinValue", 'i', -2147483648.0},
        {L::Cs, "", "long.MaxValue", 'l', 9223372036854775807.0}, {L::Cs, "", "long.MinValue", 'l', -9223372036854775808.0},
        {L::Cs, "", "uint.MaxValue", 'u', 4294967295.0}, {L::Cs, "", "short.MaxValue", 's', 32767}, {L::Cs, "", "short.MinValue", 's', -32768},
        {L::Cs, "", "byte.MaxValue", 'b', 255}, {L::Cs, "", "byte.MinValue", 'b', 0}, {L::Cs, "", "char.MaxValue", 'c', 65535}, {L::Cs, "", "char.MinValue", 'c', 0},
        {L::Cs, "", "double.MaxValue", 'd', 1.7976931348623157e308}, {L::Cs, "", "double.MinValue", 'd', -1.7976931348623157e308},
        {L::Cs, "", "double.Epsilon", 'd', 4.9406564584124654e-324}, {L::Cs, "", "double.NaN", 'd', std::numeric_limits<double>::quiet_NaN()},
        {L::Cs, "", "double.PositiveInfinity", 'd', std::numeric_limits<double>::infinity()}, {L::Cs, "", "double.NegativeInfinity", 'd', -std::numeric_limits<double>::infinity()},
        {L::Cs, "", "float.MaxValue", 'f', 3.4028234663852886e38}, {L::Cs, "", "float.MinValue", 'f', -3.4028234663852886e38},
        {L::Cs, "", "Math.PI", 'd', 3.14159265358979323846}, {L::Cs, "", "Math.E", 'd', 2.7182818284590452354},
        {L::Cs, "", "Math.Tau", 'd', 6.28318530717958647692},
        {L::Cs, "", "Int32.MaxValue", 'i', 2147483647.0}, {L::Cs, "", "Int32.MinValue", 'i', -2147483648.0},
        {L::Cs, "", "Double.MaxValue", 'd', 1.7976931348623157e308}, {L::Cs, "", "Double.NaN", 'd', std::numeric_limits<double>::quiet_NaN()},
        // opzioni di string.Split (enum di .NET come int)
        {L::Cs, "", "StringSplitOptions.None", 'i', 0}, {L::Cs, "", "StringSplitOptions.RemoveEmptyEntries", 'i', 1},
        {L::Cs, "", "StringSplitOptions.TrimEntries", 'i', 2},
    };
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << static_cast<unsigned>(lang_));
    for (const C& c : kConst) {
        if (!(c.langs & mask) || nm != c.name) continue;
        if ((lang_ == Dialect::C || lang_ == Dialect::Cpp) && c.scope[0] && !headerVisible(c.scope)) continue;
        out = EV();
        out.hasConst = true;
        switch (c.kind) {
        case 'i': out.type = tI32_; pushInt(static_cast<std::int64_t>(c.v), VK::I32); out.constValue = Value::i32(static_cast<std::int64_t>(c.v)); break;
        case 'u': out.type = tU32_; pushInt(static_cast<std::int64_t>(c.v), VK::U32); out.constValue = Value::ofInt(VK::U32, static_cast<std::int64_t>(c.v)); break;
        case 's': out.type = tI16_; pushInt(static_cast<std::int64_t>(c.v), VK::I16); out.constValue = Value::ofInt(VK::I16, static_cast<std::int64_t>(c.v)); break;
        case 'b': out.type = tU8_; pushInt(static_cast<std::int64_t>(c.v), VK::U8); out.constValue = Value::ofInt(VK::U8, static_cast<std::int64_t>(c.v)); break;
        case 'c': out.type = tChar16_; pushInt(static_cast<std::int64_t>(c.v), VK::C16); out.constValue = Value::ofInt(VK::C16, static_cast<std::int64_t>(c.v)); break;
        case 'l': {
            const std::int64_t v = c.v >= 9.2233720368547758e18 ? INT64_MAX : c.v <= -9.2233720368547758e18 ? INT64_MIN : static_cast<std::int64_t>(c.v);
            out.type = tI64_; pushInt(v, VK::I64); out.constValue = Value::i64(v); break;
        }
        case 'U': out.type = tU64_; pushInt(-1, VK::U64); out.constValue = Value::ofInt(VK::U64, -1); break;
        case 'f': out.type = lang_ == Dialect::CSharp ? tF32_ : tF64_; pushDouble(c.v, lang_ == Dialect::CSharp ? VK::F32 : VK::F64); out.hasConst = false; break;
        default: out.type = tF64_; pushDouble(c.v, VK::F64); out.hasConst = false; break;
        }
        return true;
    }
    // C++: oggetti e manipolatori della libreria (std::cout, std::endl, std::fixed...)
    if (lang_ == Dialect::Cpp || lang_ == Dialect::C) {
        std::string n = nm;
        bool qualified = n.rfind("std::", 0) == 0;
        bool usingStd = false;
        for (Namespace* ns = ns_; ns && !usingStd; ns = ns->parent) for (Namespace* u : ns->usings) if (u->name == "std") usingStd = true;
        if (qualified) n = n.substr(5);
        else if (lang_ == Dialect::Cpp && !usingStd && n != "stdin" && n != "stdout" && n != "stderr" && n != "string.npos" && n != "errno") return false;
        struct Obj { const char* name; const char* scope; const char* sig; };
        static const Obj kObjs[] = {
            {"cout", "iostream|ostream", "ostream()"}, {"cerr", "iostream|ostream", "ostream()"}, {"clog", "iostream|ostream", "ostream()"},
            {"cin", "iostream|istream", "istream()"},
            {"endl", "iostream|ostream|ios", "manip()"}, {"flush", "iostream|ostream|ios", "manip()"}, {"fixed", "iostream|ios|iomanip", "manip()"},
            {"scientific", "iostream|ios|iomanip", "manip()"}, {"defaultfloat", "iostream|ios|iomanip", "manip()"}, {"boolalpha", "iostream|ios", "manip()"},
            {"noboolalpha", "iostream|ios", "manip()"}, {"left", "iostream|ios|iomanip", "manip()"}, {"right", "iostream|ios|iomanip", "manip()"},
            {"hex", "iostream|ios", "manip()"}, {"dec", "iostream|ios", "manip()"}, {"oct", "iostream|ios", "manip()"},
            {"showpoint", "iostream|ios", "manip()"}, {"noshowpoint", "iostream|ios", "manip()"}, {"ws", "iostream|istream", "manip()"},
        };
        for (const Obj& o : kObjs) {
            if (n != o.name || lang_ != Dialect::Cpp) continue;
            if (!headerVisible(o.scope)) continue;
            const std::int32_t idx = nativeIndex("", "#std", o.name, nullptr);
            std::vector<TypeP> ps;
            bool variadic = false;
            out = EV();
            out.type = parseSig(o.sig, ps, variadic, nullptr, lang_);
            emit(Op::CallN, idx, 0, 1);
            if (out.type->k == TK::Native && (out.type->nk == NK::OStream || out.type->nk == NK::IStream)) { out.cat = EV::Ptr; out.lvalue = true; emit(Op::ConvT, ptrTo(out.type)->index); }
            return true;
        }
        if ((n == "stdin" || n == "stdout" || n == "stderr") && headerVisible("stdio.h|cstdio")) {
            pushInt(n == "stdin" ? 0 : n == "stdout" ? 1 : 2, VK::I32);
            emit(Op::CallN, nativeIndex("", "#std", "#stdio", nullptr), 1, 1);
            out = EV();
            out.type = ptrTo(nativeOf(NK::CFile, {}));
            return true;
        }
        if (n == "string.npos" || nm == "string.npos") {
            pushInt(-1, VK::U64);
            out = EV();
            out.type = tU64_;
            out.hasConst = true;
            out.constValue = Value::ofInt(VK::U64, -1);
            return true;
        }
    }
    if (lang_ == Dialect::CSharp && (nm == "string.Empty" || nm == "String.Empty")) { pushString("", lang_, false); out = EV(); out.type = tCsStr_; return true; }
    if (lang_ == Dialect::CSharp && nm == "Environment.NewLine") { pushString("\n", lang_, false); out = EV(); out.type = tCsStr_; return true; }
    return false;
}

} // namespace fx::prog
