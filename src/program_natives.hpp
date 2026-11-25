#pragma once
// Funzioni e tipi nativi delle librerie standard (v1.20.0): la tabella che il
// compilatore consulta per i nomi della libreria (printf, std::vector,
// Console.WriteLine, List<T>...) e gli indici delle implementazioni
// (program_lib*.cpp, program_fx.cpp). Le firme sono testi corti analizzati dal
// compilatore ("double(double)", "T&(size_t)", "void(any,...)").
#include "program_internal.hpp"
#include <cstdint>

namespace fx::prog {

class Vm;
struct NativeCall {
    Value* args{nullptr};
    int argc{0};
    Value ret;
    TypeP type{nullptr};          // tipo statico associato (contenitore, elemento...)
    Dialect lang{Dialect::Cpp};
};
using NativeFn = void (*)(Vm& vm, NativeCall& call);

// Bit dei dialetti.
namespace L {
constexpr std::uint8_t C = 1, Cpp = 2, Cs = 4, Java = 8, CC = 3, All = 15;
}
// Opzioni delle voci.
namespace NF {
constexpr std::uint8_t Static = 1, Property = 2, Indexer = 4, Ctor = 8, Operator = 16, Setter = 32, Lvalue = 64;
}

struct NativeEntry {
    std::uint8_t langs;
    const char* scope;            // C/C++: intestazione ("stdio.h"); C#/Java: classe o namespace ("Console", "System.IO")
    const char* owner;            // tipo del ricevente ("vector", "string", "List") o "" (funzione libera / statica)
    const char* name;
    const char* sig;              // "ret(par,...)"
    NativeFn fn;
    std::uint8_t flags;
};
struct NativeTypeEntry {
    std::uint8_t langs;
    const char* scope;            // intestazione o namespace
    const char* name;             // "vector", "List", "StringBuilder"
    NK nk;
    std::uint8_t typeArgs;        // numero di argomenti di tipo
};
const NativeEntry* nativeEntries(std::size_t& count) noexcept;
const NativeTypeEntry* nativeTypes(std::size_t& count) noexcept;
NativeFn nativeAt(std::int32_t index) noexcept;       // indice nella tabella delle voci
const NativeEntry& nativeEntry(std::int32_t index) noexcept;
std::int32_t nativeIndex(const char* scope, const char* owner, const char* name, const char* sig) noexcept;

// Intestazioni standard virtuali per dialetto (true se `name` e' standard).
bool standardHeader(Dialect d, const std::string& name) noexcept;
// Intestazioni incluse implicitamente da un'altra (<iostream> porta <string>...).
std::vector<std::string> impliedHeaders(Dialect d, const std::string& name);
// Intestazioni standard che la PicoCalc non offre (<thread>, <memory>...): errore chiaro.
bool unsupportedHeader(const std::string& name) noexcept;

} // namespace fx::prog
