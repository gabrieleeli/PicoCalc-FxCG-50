#pragma once
// Strutture interne del nucleo dell'interprete (v1.20.0, solo Pimoroni Pico
// Plus 2 W): valori, tipi statici, classi, funzioni, istruzioni, programma.
// Condivise dal compilatore (program_compile*.cpp) e dalla macchina
// (program_vm.cpp, program_heap.cpp, program_lib*.cpp, program_fx.cpp).
// Progetto: docs/reports/ANALISI_FONTI_v1.20.0.md §5.
#include "program.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fx::prog {

// ---------------------------------------------------------------- valori
// Tipo dinamico di un valore. Le variabili hanno un tipo statico e ogni
// scrittura converte il valore a quel tipo: il tipo dinamico coincide con il
// tipo statico e l'aritmetica segue le regole del linguaggio.
enum class VK : std::uint8_t {
    Void = 0, Null, Bool,
    C8,      // char di C/C++ (8 bit con segno)
    C16,     // char di C#/Java (16 bit senza segno)
    I8, U8, I16, U16, I32, U32, I64, U64,
    F32, F64,
    Ref,     // riferimento a un oggetto
    Ptr,     // puntatore: oggetto + cella (Whole = l'oggetto intero)
    Fn,      // funzione: indice (f = 0) o chiusura (f = 1, o = oggetto)
};
constexpr std::uint32_t Whole = 0xFFFFFFFFu;
inline bool isInt(VK k) noexcept { return k >= VK::C8 && k <= VK::U64; }
inline bool isFloat(VK k) noexcept { return k == VK::F32 || k == VK::F64; }
inline bool isNum(VK k) noexcept { return isInt(k) || isFloat(k) || k == VK::Bool; }
inline bool isSigned(VK k) noexcept { return k == VK::C8 || k == VK::I8 || k == VK::I16 || k == VK::I32 || k == VK::I64; }
int bitsOf(VK k) noexcept;                               // 1 (Bool), 8, 16, 32, 64

struct Obj;
struct Value {
    VK k{VK::Void};
    std::uint8_t f{0};
    std::uint16_t aux{0};
    std::uint32_t idx{0};
    union {
        std::int64_t i;
        std::uint64_t u;
        double d;
        Obj* o;
    };
    Value() : i(0) {}
    static Value ofInt(VK k, std::int64_t v) { Value x; x.k = k; x.i = v; return x; }
    static Value i32(std::int64_t v) { return ofInt(VK::I32, static_cast<std::int32_t>(v)); }
    static Value i64(std::int64_t v) { return ofInt(VK::I64, v); }
    static Value boolean(bool b) { Value x; x.k = VK::Bool; x.i = b ? 1 : 0; return x; }
    static Value f64(double v) { Value x; x.k = VK::F64; x.d = v; return x; }
    static Value f32(double v) { Value x; x.k = VK::F32; x.d = static_cast<double>(static_cast<float>(v)); return x; }
    static Value null() { Value x; x.k = VK::Null; x.o = nullptr; return x; }
    static Value ref(Obj* p) { Value x; if (p) { x.k = VK::Ref; x.o = p; } else { x.k = VK::Null; x.o = nullptr; } return x; }
    static Value ptr(Obj* p, std::uint32_t cell) { Value x; x.k = VK::Ptr; x.o = p; x.idx = cell; return x; }
    static Value fn(std::int32_t index) { Value x; x.k = VK::Fn; x.f = 0; x.i = index; return x; }
    static Value closure(Obj* c) { Value x; x.k = VK::Fn; x.f = 1; x.o = c; return x; }
    bool isNull() const noexcept { return k == VK::Null || ((k == VK::Ref || k == VK::Ptr) && o == nullptr); }
    bool holdsObj() const noexcept { return (k == VK::Ref || k == VK::Ptr || (k == VK::Fn && f == 1)) && o != nullptr; }
    double num() const noexcept;                         // come double
    std::int64_t integer() const noexcept;               // come intero (troncamento)
};

// ---------------------------------------------------------------- tipi statici
enum class TK : std::uint8_t {
    Void, Bool, C8, C16, I8, U8, I16, U16, I32, U32, I64, U64, F32, F64,
    Null,       // tipo di null/nullptr/NULL
    Str,        // C++ std::string (cppValue) o string di C#/Java
    Ptr,        // puntatore a elem
    Ref,        // riferimento C++ (T&) / ref-out C# a elem
    Array,      // array di elem (C: dimensione fissa `size`; C#: rank; Java: rank 1 annidato)
    Class,      // istanza di cls (classe, struct, interfaccia, eccezione, classe nativa)
    Native,     // contenitore o oggetto nativo (nk) con argomenti di tipo
    Func,       // puntatore a funzione / delegato / lambda: ret + params
    Enum,       // enum (cls con isEnum): valore intero
    Nullable,   // C# T? di un tipo valore
    Dynamic,    // sconosciuto (nessuna conversione)
};
enum class NK : std::uint8_t {
    None,
    // C++
    Vector, Map, Set, UMap, USet, Pair, Stack, Queue, Deque, List, OStream, IStream, IFStream, OFStream, FStream,
    StringStream, Iter, MapIter, Greater, Less, Exception, CFile, MapEntry, Manip,
    // C#
    CsList, CsDict, CsHashSet, CsQueue, CsStack, CsKvp, CsStringBuilder, CsStreamReader, CsStreamWriter, CsRandom,
    // Java (v1.21.0)
    JArrayList, JLinkedList, JHashMap, JTreeMap, JHashSet, JTreeSet, JArrayDeque, JStringBuilder, JScanner, JRandom,
    JIterator, JMapEntry, JOptional,
    Count
};
struct ClassInfo;
struct TypeRef {
    TK k{TK::Void};
    NK nk{NK::None};
    bool cppValue{false};       // Str: std::string (valore modificabile) invece di string immutabile
    bool isConst{false};
    std::int32_t size{-1};      // Array del C: numero di elementi (-1 sconosciuto/non fisso)
    std::uint8_t rank{1};       // Array di C#: 1 = [], 2 = [,] ...
    const TypeRef* elem{nullptr};
    ClassInfo* cls{nullptr};
    std::vector<const TypeRef*> args;   // Native/Class generici: argomenti; Func: parametri
    const TypeRef* ret{nullptr};        // Func
    std::int32_t index{0};      // posizione in Program::types
    std::string name() const;   // per i messaggi ("int", "std::vector<int>", "Punto*")
};
using TypeP = const TypeRef*;
VK vkOf(TypeP t) noexcept;                               // tipo dinamico dei valori di t
bool isNumeric(TypeP t) noexcept;
bool isIntegral(TypeP t) noexcept;
bool isRefLike(TypeP t) noexcept;                        // oggetti per riferimento (C#/Java), puntatori

// ---------------------------------------------------------------- classi
enum class Access : std::uint8_t { Public, Protected, Private, Internal };
struct Function;
struct FieldInfo {
    std::string name;
    TypeP type{nullptr};
    Access access{Access::Public};
    bool isStatic{false}, isConst{false}, isReadonly{false};
    std::int32_t slot{-1};                               // istanza: slot nell'oggetto; static: slot globale
    ClassInfo* owner{nullptr};
    int line{0}, column{0};
};
struct PropertyInfo {
    std::string name;
    TypeP type{nullptr};
    Function* getter{nullptr};
    Function* setter{nullptr};
    bool isStatic{false}, isIndexer{false};
    Access access{Access::Public}, setAccess{Access::Public};
    std::int32_t backing{-1};                            // proprieta' automatica: slot del campo nascosto
    std::string backingName;                             // proprieta' automatica: nome del campo nascosto
    ClassInfo* owner{nullptr};
};
struct ClassInfo {
    std::string name;                                    // "Punto", "Pila<int>"
    std::string qualified;                               // "geo::Punto", "Demo.Punto"
    Dialect lang{Dialect::Cpp};
    ClassInfo* base{nullptr};
    std::vector<ClassInfo*> interfaces;
    bool isStruct{false};       // struct (C/C++/C#)
    bool valueType{false};      // copia nell'assegnazione (C/C++ struct e classi per valore, struct di C#)
    bool isInterface{false}, isAbstract{false}, isEnum{false}, isStaticClass{false}, isSealed{false};
    bool isNative{false};       // classe del nucleo (eccezioni, Fx.Stat1...)
    bool isException{false};    // deriva da un'eccezione standard
    bool hasDtorChain{false};   // C++: distruttore proprio o di membri/base
    bool complete{false};
    std::int32_t id{0};
    std::int32_t slotCount{0};                           // campi d'istanza (con quelli della base)
    std::vector<FieldInfo> fields;                       // solo quelli dichiarati qui
    std::vector<PropertyInfo> properties;
    std::vector<Function*> methods;                      // dichiarati qui (anche static)
    std::vector<Function*> ctors;
    Function* dtor{nullptr};
    Function* staticInit{nullptr};                       // C#: costruttore statico e inizializzatori static
    Function* fieldInit{nullptr};                        // inizializzatori dei campi d'istanza
    std::vector<Function*> vtable;
    std::unordered_map<std::string, Function*> selectors;   // "nome/arita'" -> implementazione (interfacce)
    std::vector<std::pair<std::string, std::int64_t>> enumValues;
    TypeP self{nullptr};                                 // TypeRef Class di questa classe
    TypeP underlying{nullptr};                           // enum: tipo intero
    std::vector<TypeP> typeArgs;                         // istanza di un template/generico
    ClassInfo* outer{nullptr};                           // classe che contiene questa (annidata)
    std::vector<ClassInfo*> friends;
    std::vector<std::string> friendFunctions;
    int line{0}, column{0}, file{0};
    std::int64_t sizeBytes{0};                           // sizeof (modello LP64), per malloc e sizeof a tempo di esecuzione
    bool derivesFrom(const ClassInfo* other) const noexcept;
    const FieldInfo* findField(const std::string& n) const noexcept;    // anche nelle basi
    std::int32_t fieldSlot(std::int32_t localIndex) const noexcept;
};

// ---------------------------------------------------------------- istruzioni
enum class Op : std::uint8_t {
    Nop,
    PushI,       // a: valore a 32 bit, b: VK
    PushL,       // a: indice in ints, b: VK
    PushD,       // a: indice in doubles, b: VK
    PushS,       // a: indice in strings; b: 0 array di char del C (sola lettura), 1 std::string, 2 string C#/Java
    PushNull, PushVoid,
    PushFn,      // a: funzione
    LoadL, StoreL,          // a: slot; StoreL b=1: tiene il valore
    LoadG, StoreG,          // a: slot globale
    LoadBox, StoreBox,      // a: slot con una Box
    NewBox,                 // a: slot: valore -> Box (variabile di cui si prende l'indirizzo)
    AddrL,                  // a: slot con Box -> puntatore
    AddrG,                  // a: slot globale -> puntatore
    KillL,                  // a: slot: fine del blocco (Box morta, oggetto per valore liberato, slot svuotato)
    Pop,
    Dup,                    // b: quanti valori (0 = 1)
    DupX,                   // b: copia la cima sotto i b valori che la precedono
    Swap,
    GetF, SetF, AddrF,      // a: slot; [obj] -> v; [obj v] -> (b=1: v); [obj] -> ptr
    GetE, SetE, AddrE,      // [arr i] -> v; [arr i v] -> (b=1: v); [arr i] -> ptr
    GetE2, SetE2, AddrE2,   // array rettangolari di C#: [arr i j] ...
    ArrLen,                 // [arr] -> int (b: dimensione per GetLength)
    LoadP, StoreP,          // [ptr] -> v; [ptr v] -> (b=1: v)
    IncL, IncG, IncP,       // a: slot (IncP: nessuno); b: 1 post, 2 decremento; c: VK
    Conv,                   // b: VK di arrivo; c: 1 = conversione C# esplicita con controllo (non usata)
    ConvT,                  // a: tipo (indice) di arrivo per puntatori/riferimenti/Nullable
    ToStr,                  // b: dialetto: valore -> stringa del linguaggio (puo' chiamare ToString())
    ToBool,
    Add, Sub, Mul, Div, Rem, Neg, Not, BitNot, And, Or, Xor, Shl, Shr, UShr,
    Eq, Ne, Lt, Le, Gt, Ge,
    Concat,                 // [a b] -> stringa (C#/Java, operandi gia' stringhe)
    StrEq, StrCmp,          // confronto di stringhe (C#/Java ==, C++ std::string)
    Jmp, Jz, Jnz, JzKeep, JnzKeep,
    Switch,                 // a: tabella
    Call,                   // a: funzione, b: argomenti
    CallV,                  // a: slot della vtable, b: argomenti (il primo e' il ricevente)
    CallI,                  // a: selettore (Program::selectors), b: argomenti
    CallN,                  // a: NativeId, b: argomenti, c: indice del tipo (per i generici nativi)
    CallFn,                 // b: argomenti; sotto gli argomenti la funzione o la chiusura
    Ret,                    // b: 1 con valore
    New,                    // a: classe -> oggetto con i campi ai valori iniziali
    NewArr,                 // a: tipo dell'elemento, b: dimensioni; [n...] -> array
    ArrLit,                 // a: tipo dell'elemento, c: numero; [v1..vn] -> array
    NewNative,              // a: tipo (Native) -> contenitore vuoto
    Copy,                   // [obj] -> copia profonda (oggetti per valore); puo' chiamare il costruttore di copia
    CopyInto,               // [dst src] -> copia i campi di src in dst (assegnazione per valore)
    Destroy,                // [obj] -> distruttore (C++) e distruttori dei membri
    Delete,                 // [ptr] -> delete del C++ (b=1: delete[])
    Free,                   // [ptr] -> free del C
    InstOf,                 // a: classe -> bool
    CastCk,                 // a: classe -> lo stesso riferimento o InvalidCast
    DynCast,                // a: classe -> riferimento o null
    MkClosure,              // a: funzione, b: catture; [c1..cn] -> chiusura
    LoadCap,                // a: indice della cattura (la chiusura e' lo slot 0)
    Throw,                  // [eccezione]
    Rethrow,                // a: slot con l'eccezione catturata
    Fault,                  // a: FaultKind, b: 0; messaggio in strings[c]
    Halt,
};
struct Instr {
    Op op{Op::Nop};
    std::uint8_t b{0};
    std::uint16_t c{0};
    std::int32_t a{0};
};
struct SrcPos {
    std::uint16_t file{0};
    std::uint16_t column{0};
    std::uint32_t line{0};
};
// Gestore di un blocco try (o di pulizia: distruttori, finally).
struct Handler {
    std::int32_t start{0}, end{0}, target{0};           // [start, end) -> target
    ClassInfo* catches{nullptr};                         // nullptr = tutte le eccezioni (catch(...), finally)
    TypeP catchType{nullptr};                            // C++: catch di un valore non oggetto (int, const char*...)
    std::int32_t slot{-1};                               // slot dove va l'eccezione
    std::uint32_t depth{0};                              // altezza della pila operandi (sopra i locali) al gestore
    bool cleanup{false};                                 // finally/distruttori: rilancia alla fine
};
struct LocalInfo {
    std::string name;
    TypeP type{nullptr};
    std::int32_t slot{0};
    std::int32_t from{0}, to{0};                         // istruzioni in cui e' viva
    bool boxed{false};
};
struct Param {
    std::string name;
    TypeP type{nullptr};
    bool byRef{false};                                   // C++ T&, C# ref/out
    bool isOut{false};
    bool isParams{false};                                // C# params T[]
    bool hasDefault{false};
};
struct Function {
    std::string name;                                    // "main", "Punto::area", "Program.Main"
    std::string simple;                                  // "area"
    Dialect lang{Dialect::Cpp};
    ClassInfo* owner{nullptr};
    std::int32_t index{0};
    std::vector<Param> params;                           // senza this
    TypeP ret{nullptr};
    bool isStatic{true}, isVirtual{false}, isAbstract{false}, isCtor{false}, isDtor{false}, isConst{false};
    bool isOperator{false}, isLambda{false}, isOverride{false}, isNative{false}, defined{false};
    bool hasThis{false};                                 // slot 0 = this
    Access access{Access::Public};
    std::int32_t vslot{-1};
    std::int32_t nlocals{0};
    std::int32_t library{-1};                            // origine: -1 programma, >= 0 libreria (cartella dati)
    std::vector<Instr> code;
    std::vector<SrcPos> pos;
    std::vector<std::uint8_t> stmt;                      // 1 = inizio di un'istruzione del sorgente (debug)
    std::vector<Handler> handlers;
    std::vector<LocalInfo> locals;
    std::vector<std::int32_t> defaults;                  // C++/C#: funzioni che calcolano i valori predefiniti
    int file{0}, line{0}, column{0};
    std::string signature() const;
};
struct SwitchTable {
    std::vector<std::pair<std::int64_t, std::int32_t>> cases;   // ordinati per valore
    std::vector<std::pair<std::u16string, std::int32_t>> strCases;
    std::vector<std::pair<std::string, std::int32_t>> cstrCases;   // C++? non usato
    bool strings{false};
    std::int32_t fallback{-1};
};
// Costante stringa: byte (C/C++) o UTF-16 (C#/Java).
struct StrConst {
    std::string bytes;
    std::u16string wide;
};
enum class FaultKind : std::uint8_t {
    Generic, NullPointer, Index, DivZero, Overflow, Cast, Memory, Stack, DeadPointer, ReadOnly, Argument, Format,
    Key, InvalidOp, Io, NotFound, Unauthorized, NotSupported, NotImplemented, Time,
};

class Program {
public:
    Dialect dialect{Dialect::Cpp};
    std::vector<std::string> files;                      // file per le posizioni
    std::vector<std::unique_ptr<Function>> functions;
    std::vector<std::unique_ptr<ClassInfo>> classes;
    std::vector<std::unique_ptr<TypeRef>> types;
    std::vector<std::int64_t> ints;
    std::vector<double> doubles;
    std::vector<StrConst> strings;
    std::vector<SwitchTable> switches;
    std::vector<std::string> selectors;                  // "nome/arita'"
    std::vector<std::string> globalNames;
    std::vector<TypeP> globalTypes;
    std::vector<std::int32_t> globalFile;
    std::vector<LibraryRef> libraries;
    std::int32_t init{-1};                               // inizializzazione dei globali (e static di C#)
    std::int32_t entry{-1};                              // main / Main
    std::int32_t entryArgs{0};                           // 0: main(), 1: main(argc, argv) / Main(string[]), 2: solo argc
    bool entryReturnsInt{false};
    // Classi del nucleo usate dalla macchina (eccezioni per i guasti del linguaggio).
    ClassInfo* faultClass[DialectCount][24] = {};
    ClassInfo* exceptionRoot[DialectCount] = {};
    ClassInfo* typeClass{nullptr};                       // System.Type di C# (GetType())
    // Risultati della libreria fx per linguaggio: 0 Stat1, 1 Stat2, 2 Reg (FxStat1, fx::Stat1, Fx.Stat1...).
    ClassInfo* fxResult[DialectCount][3] = {};
    std::size_t sourceBytes{0};
    // Tipi di base per le funzioni native (creati dal compilatore).
    const TypeRef* prim[16] = {};                        // per TK (Void ... F64)
    const TypeRef* cppString{nullptr};
    const TypeRef* csString{nullptr};
    const TypeRef* dynamic{nullptr};
    const TypeRef* object{nullptr};
    const TypeRef* voidPtr{nullptr};
    const TypeRef* charPtr{nullptr};
    const TypeRef* typeAt(std::int32_t i) const { return types[static_cast<std::size_t>(i)].get(); }
};

// Ricerca di una posizione: file, riga e colonna dell'istruzione `pc` di `fn`.
inline SrcPos posOf(const Function& fn, std::int32_t pc) {
    if (fn.pos.empty()) return SrcPos{};
    const std::size_t i = pc < 0 ? 0 : static_cast<std::size_t>(pc) < fn.pos.size() ? static_cast<std::size_t>(pc) : fn.pos.size() - 1;
    return fn.pos[i];
}

} // namespace fx::prog
