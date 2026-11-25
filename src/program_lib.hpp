#pragma once
// Servizi comuni delle librerie native (v1.20.0): accesso agli argomenti,
// oggetti dal ricevente, sequenze (vector, string, array) viste dagli
// iteratori, mappe e insiemi (ordinati, in ordine d'inserimento), ordinamento
// stabile con il confronto del programma, ingresso della console condiviso
// (scanf, cin, Console.ReadLine), file confinati nella cartella del programma.
// Usato da program_lib*.cpp e program_fx.cpp.
#include "program_vm.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace fx::prog::lib {

constexpr std::uint32_t End = 0xFFFFFFFEu;                // iteratore end() delle mappe

// ---------------------------------------------------------------- argomenti
inline std::int64_t I(const NativeCall& c, int i) { return c.args[i].integer(); }
inline double D(const NativeCall& c, int i) { return c.args[i].num(); }
std::string S(Vm& vm, const NativeCall& c, int i);         // testo di una stringa del linguaggio o di un char*
std::u16string W(Vm& vm, const NativeCall& c, int i);      // testo UTF-16 (C#/Java)
// Oggetto dal valore: riferimento, puntatore all'oggetto o a una variabile che lo contiene.
Obj* objArg(Vm& vm, const Value& v);
VecObj* vecArg(Vm& vm, const Value& v);
MapObj* mapArg(Vm& vm, const Value& v);
StrBObj* strbArg(Vm& vm, const Value& v);
StrWObj* strwArg(Vm& vm, const Value& v);
SbObj* sbArg(Vm& vm, const Value& v);
FileObj* fileArg(Vm& vm, const Value& v);
InstObj* instArg(Vm& vm, const Value& v);
ArrayObj* arrayArg(Vm& vm, const Value& v);
NativeObj* nativeArg(Vm& vm, const Value& v);
// Tipi: argomento di tipo i del contenitore (anche dal tipo di un iteratore).
TypeP containerType(const NativeCall& c);
TypeP typeArg(const NativeCall& c, std::size_t i);
TypeP typeArgOf(TypeP container, std::size_t i);

// ---------------------------------------------------------------- valori
Value str(Vm& vm, std::string_view s);                     // stringa del linguaggio in esecuzione
Value strOf(Vm& vm, Dialect d, std::string_view s);
Value wstr(Vm& vm, std::u16string_view s);                 // string di C#/Java
Value toType(Vm& vm, const Value& v, TypeP t);             // conversione al tipo statico (numeri, char*->string)
Value boolOf(Dialect d, bool b);                           // int in C, bool altrove
inline Value i32(std::int64_t v) { return Value::i32(v); }
inline Value i64(std::int64_t v) { return Value::i64(v); }
inline Value u64(std::uint64_t v) { return Value::ofInt(VK::U64, static_cast<std::int64_t>(v)); }
inline Value f64(double d) { return Value::f64(d); }
inline Value chr(Dialect d, int c) { return Value::ofInt(d == Dialect::C || d == Dialect::Cpp ? VK::C8 : VK::C16, d == Dialect::C || d == Dialect::Cpp ? static_cast<signed char>(c) : static_cast<char16_t>(c)); }
// Valore da memorizzare in un contenitore: copia degli oggetti per valore.
Value stored(Vm& vm, const Value& v, TypeP elem);

// ---------------------------------------------------------------- sequenze
// vector/deque/list/stack/queue (Vec), std::string (StrB), array (Array),
// string di C#/Java (StrW, sola lettura): gli iteratori sono puntatori
// (oggetto, posizione).
struct Seq {
    Obj* o{nullptr};
    std::uint32_t size() const;
    Value get(Vm& vm, std::uint32_t i) const;
    void set(Vm& vm, std::uint32_t i, const Value& v) const;
};
Seq seqOf(Vm& vm, const Value& v);                         // dal contenitore o da un iteratore/puntatore
// Intervallo [first, last) di due iteratori dello stesso contenitore.
void rangeOf(Vm& vm, const Value& first, const Value& last, Seq& seq, std::uint32_t& b, std::uint32_t& e);

// ---------------------------------------------------------------- mappe
std::int32_t mapFind(Vm& vm, MapObj* m, const Value& key);
std::uint32_t mapInsert(Vm& vm, MapObj* m, const Value& key, const Value& val, bool* inserted);
bool mapErase(Vm& vm, MapObj* m, const Value& key);
void mapEraseCell(Vm& vm, MapObj* m, std::uint32_t cell);
void mapClear(MapObj* m);
std::uint32_t mapCellAt(MapObj* m, std::uint32_t i);      // i-esimo elemento nell'ordine di visita
std::uint32_t mapBegin(MapObj* m);
std::uint32_t mapNext(Vm& vm, MapObj* m, std::uint32_t cell);
std::uint32_t mapPrev(Vm& vm, MapObj* m, std::uint32_t cell);
std::uint32_t mapBound(Vm& vm, MapObj* m, const Value& key, bool upper);
std::vector<std::uint32_t> mapCells(MapObj* m);           // celle nell'ordine di visita

// ---------------------------------------------------------------- ordinamento e confronto
// Ordinamento stabile (merge sort) dei valori; `less` puo' chiamare il programma.
void stableSort(Vm& vm, std::vector<Value>& items, const std::function<bool(const Value&, const Value&)>& less);
// Confronto "a < b" con un comparatore del programma (C++: bool; C#/Java: int < 0) o naturale.
bool lessWith(Vm& vm, const Value* cmp, const Value& a, const Value& b, bool intComparator);

// ---------------------------------------------------------------- console
int conGet(Vm& vm);                                        // carattere dell'input (-1 = fine)
int conPeek(Vm& vm);
void conUnget(Vm& vm);
bool conLine(Vm& vm, std::string& out);                    // resto della riga (senza '\n')
bool conToken(Vm& vm, std::string& out);                   // parola dopo gli spazi
void conOut(Vm& vm, int which, const std::string& text);   // 1 stdout, 2 stderr

// ---------------------------------------------------------------- file
// Percorso del programma -> percorso sulla SD (confinato): vuoto se non ammesso.
std::string confine(Vm& vm, const std::string& path, std::string& why);
// Apertura di un FileObj ("r", "w", "a", "r+"...): false con il motivo.
bool fileOpen(Vm& vm, FileObj* f, const std::string& path, const std::string& mode, std::string& why);
bool fileClose(Vm& vm, FileObj* f);                       // scrittura atomica del contenuto
int fileGet(Vm& vm, FileObj* f);                           // -1 alla fine
int filePeek(Vm& vm, FileObj* f);
void fileUnget(Vm& vm, FileObj* f, int c);
bool fileLine(Vm& vm, FileObj* f, std::string& out, char delim = '\n');
bool fileToken(Vm& vm, FileObj* f, std::string& out);
void filePut(Vm& vm, FileObj* f, std::string_view text);
bool fileExists(Vm& vm, const std::string& path);
bool fileRemove(Vm& vm, const std::string& path, std::string& why);
bool fileRename(Vm& vm, const std::string& from, const std::string& to, std::string& why);
bool fileReadAll(Vm& vm, const std::string& path, std::string& out, std::string& why);
bool fileWriteAll(Vm& vm, const std::string& path, const std::string& text, bool append, std::string& why);

// ---------------------------------------------------------------- numeri dal testo
// Analisi come strtol/strtod (prefisso valido): caratteri usati, 0 se nessuno.
std::size_t parseInt(const std::string& s, std::size_t pos, int base, std::int64_t& out, bool& overflow);
std::size_t parseDouble(const std::string& s, std::size_t pos, double& out);

} // namespace fx::prog::lib
