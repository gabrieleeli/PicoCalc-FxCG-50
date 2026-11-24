#pragma once
// Macchina dei programmi (v1.20.0): oggetti, heap con budget e GC
// mark-and-sweep, cornici, eccezioni, servizi per le funzioni native.
// Interno: usato da program_vm.cpp, program_heap.cpp, program_format.cpp,
// program_lib*.cpp e program_fx.cpp. Vedi program.hpp.
#include "program_internal.hpp"
#include "program_natives.hpp"
#include "step_task.hpp"
#include "tlsf.hpp"
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "file_store.hpp"

namespace fx { namespace coro { class Coroutine; } }

namespace fx::prog {

// ---------------------------------------------------------------- oggetti
enum class OK : std::uint8_t { Array, Inst, Box, StrB, StrW, Vec, Map, Sb, File, Closure, Native };
enum OF : std::uint8_t { FMark = 1, FDead = 2, FExplicit = 4, FReadOnly = 8, FRoot = 16, FArrayNew = 32 };

struct Obj {
    Obj* next{nullptr};
    OK kind{OK::Box};
    std::uint8_t flags{0};
    std::uint16_t lang{0};
    std::uint32_t bytes{0};
    TypeP type{nullptr};                         // tipo statico dell'oggetto
    explicit Obj(OK k) : kind(k) {}
    virtual ~Obj() = default;
    bool dead() const noexcept { return (flags & FDead) != 0; }
};
class Heap;
struct ArrayObj final : Obj {
    Heap* heap{nullptr};
    VK ek{VK::Void};                             // Void = blocco di malloc non ancora tipizzato
    std::uint8_t esize{0};
    std::uint8_t rank{1};
    std::uint32_t count{0};
    std::uint32_t rows{0}, cols{0};              // rank 2 (C#)
    std::uint64_t rawBytes{0};                   // blocco non tipizzato: byte richiesti
    std::uint8_t* data{nullptr};
    TypeP elem{nullptr};
    ArrayObj() : Obj(OK::Array) {}
    ~ArrayObj() override;
    Value get(std::uint32_t i) const noexcept;
    void set(std::uint32_t i, const Value& v) noexcept;
    bool refs() const noexcept { return esize == sizeof(Value); }
};
struct InstObj final : Obj {
    ClassInfo* cls{nullptr};
    std::uint32_t nfields{0};
    Value* fields{nullptr};                      // subito dopo l'oggetto, nello stesso blocco
    bool destroyed{false};                       // C++: distruttore gia' eseguito
    InstObj() : Obj(OK::Inst) {}
    ~InstObj() override = default;
};
struct BoxObj final : Obj { Value v; BoxObj() : Obj(OK::Box) {} };
struct StrBObj final : Obj { std::pmr::string s; explicit StrBObj(std::pmr::memory_resource* r) : Obj(OK::StrB), s(r) {} };
struct StrWObj final : Obj { std::pmr::u16string s; std::int32_t hash{0}; bool hashed{false}; explicit StrWObj(std::pmr::memory_resource* r) : Obj(OK::StrW), s(r) {} };
struct VecObj final : Obj {
    std::pmr::vector<Value> items;
    std::uint32_t version{0};
    explicit VecObj(std::pmr::memory_resource* r) : Obj(OK::Vec), items(r) {}
};
// Mappe e insiemi: 0 = ordinati per chiave (std::map/set, TreeMap), 1 = ordine
// d'inserimento con le celle libere riusate (Dictionary, unordered_*), 2 = Java HashMap.
struct MapObj final : Obj {
    std::uint8_t mode{0};
    bool isSet{false};
    std::pmr::vector<Value> keys, vals;
    std::pmr::vector<std::uint8_t> used;
    std::pmr::vector<std::uint32_t> order;       // mode 0: celle in ordine di chiave
    std::pmr::vector<std::int32_t> buckets, next;
    std::pmr::vector<std::uint32_t> freeSlots;
    std::uint32_t count{0};
    std::uint32_t version{0};
    explicit MapObj(std::pmr::memory_resource* r) : Obj(OK::Map), keys(r), vals(r), used(r), order(r), buckets(r), next(r), freeSlots(r) {}
};
struct SbObj final : Obj { std::pmr::u16string s; explicit SbObj(std::pmr::memory_resource* r) : Obj(OK::Sb), s(r) {} };
struct ClosureObj final : Obj {
    std::int32_t fn{-1};
    bool bound{false};                           // metodo legato: caps[0] e' il ricevente
    std::pmr::vector<Value> caps;
    explicit ClosureObj(std::pmr::memory_resource* r) : Obj(OK::Closure), caps(r) {}
};
// File e flussi (FILE* del C, fstream/stringstream del C++, StreamReader/Writer di C#).
struct FileObj final : Obj {
    std::string path;                            // percorso sulla SD (vuoto: console o stringa)
    std::uint8_t mode{0};                        // 1 lettura, 2 scrittura, 4 aggiunta, 8 lettura+scrittura
    bool console{false};                         // stdin/stdout/stderr, cin/cout
    bool memory{false};                          // stringstream
    bool open{false}, eof{false}, fail{false}, error{false}, dirty{false};
    std::pmr::string buf;                        // contenuto (scrittura, stringstream, lettura a blocchi)
    std::size_t pos{0};                          // posizione di lettura in buf
    std::uint64_t filePos{0};                    // byte letti dal file
    std::unique_ptr<files::Reader> reader;
    bool readerEnd{false};
    int unget{-1};
    // formato degli stream del C++
    int precision{6}, width{0};
    char fill{' '};
    std::uint8_t floatMode{0};                   // 0 generale, 1 fixed, 2 scientific
    bool boolalpha{false}, left{false}, showpoint{false};
    int base{10};
    std::int32_t library{-1};                    // cartella dati della libreria che l'ha aperto
    explicit FileObj(std::pmr::memory_resource* r) : Obj(OK::File), buf(r) {}
    ~FileObj() override;
};
// Oggetti nativi con uno stato semplice (Random, iteratori...).
struct NativeObj final : Obj {
    NK nk{NK::None};
    std::pmr::vector<std::int64_t> state;
    std::pmr::vector<Value> slots;
    explicit NativeObj(std::pmr::memory_resource* r) : Obj(OK::Native), state(r), slots(r) {}
};

// ---------------------------------------------------------------- heap
class Vm;
class Heap {
public:
    Heap(Vm& vm, std::size_t budget);
    ~Heap();
    Heap(const Heap&) = delete;
    Heap& operator=(const Heap&) = delete;
    void* raw(std::size_t bytes, std::size_t align = 8);   // GC e nuovo tentativo; std::bad_alloc se finita
    void free(void* p) noexcept;
    std::pmr::memory_resource* resource() noexcept;
    template <class T, class... A> T* make(A&&... a) {
        void* mem = raw(sizeof(T), alignof(T));
        T* o = new (mem) T(std::forward<A>(a)...);
        link(o, sizeof(T));
        return o;
    }
    void link(Obj* o, std::size_t bytes);
    void destroy(Obj* o) noexcept;               // distruttore e memoria
    void collect();                              // GC
    std::size_t used() const noexcept;
    std::size_t budget() const noexcept { return budget_; }
    std::size_t cycles() const noexcept { return cycles_; }
    std::size_t objects() const noexcept { return objects_; }
    bool ready() const noexcept { return arena_ != nullptr; }
    Obj* first() const noexcept { return head_; }
private:
    Vm& vm_;
    mem::detail::Tlsf tlsf_;
    void* arena_{nullptr};
    std::size_t budget_{0};
    Obj* head_{nullptr};
    std::size_t objects_{0}, cycles_{0};
    std::size_t sinceGc_{0};
    bool collecting_{false};
    struct Resource;
    std::unique_ptr<Resource> res_;
};

// ---------------------------------------------------------------- eccezioni interne
struct ProgramThrow { Value value; };          // eccezione del programma (si cattura con try)
struct ProgramExit { int code; };              // exit()
struct ProgramAbort {};                        // AC: Break
struct ProgramPause {};

// ---------------------------------------------------------------- macchina
struct Frame {
    const Function* fn{nullptr};
    std::int32_t pc{0};
    std::uint32_t base{0};
    std::uint64_t serial{0};
};

class Vm {
public:
    Vm(std::shared_ptr<const Program> program, Host& host, Limits limits);
    ~Vm();
    // ---- esecuzione
    void runProgram();                           // inizializzazione, main (lancia ProgramExit/Error...)
    void execute(std::size_t stopDepth);        // fino a quando le cornici tornano a stopDepth
    Value invoke(const Value& fn, const Value* args, int argc);          // da una funzione nativa
    Value invokeFunction(const Function* f, const Value* args, int argc, bool hasResult);
    void pushFrame(const Function* f, int argc);
    // ---- servizi per i nativi
    const Program& program() const noexcept { return *prog_; }
    Host& host() noexcept { return host_; }
    Heap& heap() noexcept { return heap_; }
    Dialect lang() const noexcept;               // dialetto della funzione in esecuzione
    std::int32_t library() const noexcept;       // libreria della funzione in esecuzione
    [[noreturn]] void fault(FaultKind k, const std::string& msg);
    [[noreturn]] void raise(const char* cppClass, const std::string& msg);   // eccezione per nome (std::out_of_range, FormatException...)
    [[noreturn]] void throwValue(const Value& v, bool rethrow = false);
    [[noreturn]] void fatal(const std::string& msg);                         // errore della fx-CG50 (non catturabile)
    Value makeException(ClassInfo* c, const std::string& msg);
    // allocazioni (radici temporanee fino al ritorno della funzione nativa)
    ArrayObj* newArray(TypeP elem, std::uint32_t count, bool explicitAlloc = false);
    ArrayObj* newCString(std::string_view s, bool readOnly);
    InstObj* newInstance(ClassInfo* c, bool explicitAlloc = false);
    InstObj* newPair(TypeP pairType, const Value& a, const Value& b);
    StrBObj* newStrB(std::string_view s);
    StrWObj* newStrW(std::u16string_view s);
    Value newString(Dialect d, std::string_view utf8);       // la stringa del linguaggio
    VecObj* newVec(TypeP t);
    MapObj* newMap(TypeP t);
    SbObj* newSb();
    FileObj* newFile();
    NativeObj* newNativeObj(NK nk);
    Value newNative(TypeP t);                    // contenitore vuoto del tipo
    Value defaultValue(TypeP t);                 // valore iniziale (sotto-oggetti per valore compresi)
    Value copyValue(const Value& v, std::int32_t sliceClass = -1);      // copia profonda (semantica per valore)
    void copyInto(const Value& dst, const Value& src);
    void destroyValue(const Value& v, ClassInfo* staticCls);           // distruttori (C++)
    void killValue(const Value& v);              // fine della vita (oggetto per valore)
    void root(Obj* o);                           // radice temporanea
    std::size_t rootMark() const noexcept { return temps_.size(); }
    void unroot(std::size_t mark) noexcept { if (temps_.size() > mark) temps_.resize(mark); }
    // valori
    Obj* objOf(const Value& v, bool allowNull = false);       // oggetto di un Ref o di un Ptr intero
    Value loadPtr(const Value& p);
    void storePtr(const Value& p, const Value& v);
    std::string utf8(const Value& v);            // testo di una stringa (StrB, StrW, array di char, puntatore)
    std::u16string utf16(const Value& v);
    std::string cstring(const Value& p);         // char* del C fino a '\0'
    std::string format(const Value& v, Dialect d);           // come lo stampa il linguaggio
    Value toLangString(const Value& v, Dialect d);           // ToStr
    bool truthy(const Value& v) const noexcept;
    int compareValues(const Value& a, const Value& b);       // chiavi delle mappe, sort
    bool equalValues(const Value& a, const Value& b);
    std::uint32_t hashValue(const Value& v);
    Value convertTo(const Value& v, VK k);
    Value castTo(const Value& v, TypeP t);       // ConvT
    std::int64_t sizeOfType(TypeP t) const;
    // input, uscita, tempo
    bool readLine(std::string& out);             // aspetta una riga (la coroutine cede il controllo)
    void yieldNow();                             // fine della fetta di tempo
    void poll();                                 // AC e fetta di tempo (cicli lunghi dei nativi)
    std::string stackTrace() const;
    std::string where() const;                   // "file:riga" dell'istruzione corrente
    FileObj* stdStream(int which);               // 0 stdin, 1 stdout, 2 stderr
    void closeAllFiles(bool commit);
    std::vector<FileObj*> openFiles;
    // ---- stato
    std::shared_ptr<const Program> prog_;
    Host& host_;
    Limits limits_;
    Heap heap_;
    std::vector<Value> stack;
    std::size_t sp{0};
    std::vector<Frame> frames;
    ArrayObj* globals{nullptr};
    std::vector<Obj*> temps_;
    std::vector<Obj*> literals;                  // letterali (C: array di char; C#: string)
    std::vector<Obj*> cppLiterals;
    std::uint64_t serials{0};
    std::size_t executed{0};
    step::Budget* budget{nullptr};
    std::size_t quota{0};
    coro::Coroutine* coroutine{nullptr};
    bool abortRequested{false};
    bool waitingInput{false};
    bool paused{false};
    std::string inputBuffer;                     // righe digitate (cin, scanf)
    std::size_t inputPos{0};
    bool inputEof{false};
    std::uint32_t runStart{0}, runElapsed{0};
    std::uint32_t randState{1};                  // rand() del C
    bool randSeeded{false};
    Value strtokState;                           // strtok: posizione nella stringa
    std::string lastIoError;                     // perror / errno
    int lastErrno{0};
    int nativeDepth{0};
    // posizione dell'ultimo lancio (eccezione non catturata)
    int lastThrowLine{0}, lastThrowColumn{0};
    std::string lastThrowFile;
    std::vector<std::string> lastThrowTrace;
    std::unordered_map<const ClassInfo*, std::vector<TypeP>> slotTypes;
    const std::vector<TypeP>& slotsOf(const ClassInfo* c);
    ProgramThrow memoryThrow();
    FileObj* stdio[3] = {nullptr, nullptr, nullptr};
    Obj* coutObj{nullptr};
    Obj* cinObj{nullptr};
    // debug
    bool debug{false};
    StepMode stepMode{StepMode::Into};
    std::size_t stepDepth{0};
    std::int32_t stepLine{-1};
    std::uint16_t stepFile{0};
    std::vector<std::pair<std::uint16_t, std::int32_t>> breakpoints;
    bool firstStop{true};
    void debugCheck(const Function* f, std::int32_t pc);
    // cache delle classi per nome (eccezioni dei nativi)
    ClassInfo* classNamed(const std::string& n, Dialect d) const;
    void markRoots(std::vector<Obj*>& work);
private:
    void unwind(ProgramThrow& t, std::size_t stopDepth);
    void initObjects();
    std::size_t refill();
};

// Oggetti con semantica per valore (copia nell'assegnazione).
bool isValueTypeRT(TypeP t) noexcept;
// Arrotondamento decimale esatto di un double (half-even come printf o
// half-away come .NET): cifre dopo la virgola.
std::string fixedDecimal(double d, int decimals, bool awayFromZero);
// Testo UTF-8 <-> UTF-16.
std::u16string utf8To16(std::string_view s);
std::string utf16To8(std::u16string_view s);
// Formattazione dei numeri (program_format.cpp).
std::string formatPrintf(Vm& vm, const std::string& fmt, const Value* args, int argc, int& used);
std::string formatDoubleCs(double d);                         // ToString() di .NET (minimo che torna uguale)
std::string formatDoubleJava(double d);                       // Double.toString di Java
std::string formatDoubleCpp(double d, int precision, std::uint8_t mode, bool showpoint);   // ostream
std::string formatCsSpec(Vm& vm, const Value& v, const std::string& spec);   // {0:F2}, ToString("N2")
std::string formatComposite(Vm& vm, const std::string& fmt, const Value* args, int argc);  // string.Format

} // namespace fx::prog
