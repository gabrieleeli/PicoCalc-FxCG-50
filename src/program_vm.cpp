// Macchina dei programmi (v1.20.0): ciclo delle istruzioni, cornici,
// chiamate (anche dalle funzioni native), eccezioni e loro propagazione,
// guasti per linguaggio, debug passo passo, esecuzione nella coroutine e
// API pubblica Machine. Vedi program_vm.hpp e program.hpp.
#include "program_vm.hpp"
#include "board_config.hpp"
#include "coroutine.hpp"
#include "mem_pools.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>

namespace fx::prog {

Limits defaultLimits(bool psram) noexcept {
    Limits l;
    if (!psram) {
        l.heapBytes = 96u * 1024u;
        l.stackValues = 4096;
        l.callDepth = 200;
        l.sourceBytes = 64u * 1024u;
        l.projectBytes = 128u * 1024u;
    }
    return l;
}
Limits limitsForCells(std::size_t cells) noexcept {
    Limits l;
    l.heapBytes = std::max<std::size_t>(cells * 4u, 16u * 1024u);
    return l;
}

namespace {
constexpr std::size_t CoroStackDevice = 48u * 1024u;
constexpr std::size_t CoroStackHost = 1024u * 1024u;

std::int64_t wrap(VK k, std::int64_t v) noexcept {
    switch (k) {
    case VK::Bool: return v != 0;
    case VK::C8: case VK::I8: return static_cast<std::int8_t>(v);
    case VK::U8: return static_cast<std::uint8_t>(v);
    case VK::I16: return static_cast<std::int16_t>(v);
    case VK::U16: case VK::C16: return static_cast<std::uint16_t>(v);
    case VK::I32: return static_cast<std::int32_t>(v);
    case VK::U32: return static_cast<std::uint32_t>(v);
    default: return v;
    }
}
bool unsignedKind(VK k) noexcept { return k == VK::U8 || k == VK::U16 || k == VK::U32 || k == VK::U64 || k == VK::C16 || k == VK::Bool; }
VK commonKind(VK a, VK b) noexcept {
    if (a == b) return a;
    if (isFloat(a) || isFloat(b)) return VK::F64;
    if (a == VK::U64 || b == VK::U64) return VK::U64;
    if (a == VK::I64 || b == VK::I64 || a == VK::U32 || b == VK::U32) return VK::I64;
    return VK::I32;
}
} // namespace

// ================================================================ Vm
Vm::Vm(std::shared_ptr<const Program> program, Host& host, Limits limits)
    : prog_(std::move(program)), host_(host), limits_(limits), heap_(*this, limits.heapBytes) {
    if (!heap_.ready()) throw Error("Memory ERROR: memoria insufficiente per il programma", 0, 0);
    stack.resize(std::max<std::size_t>(limits.stackValues, 256));
    frames.reserve(64);
    temps_.reserve(64);
}
Vm::~Vm() {
    closeAllFiles(false);
}

Dialect Vm::lang() const noexcept { return frames.empty() ? prog_->dialect : frames.back().fn->lang; }
std::int32_t Vm::library() const noexcept { return frames.empty() ? -1 : frames.back().fn->library; }

std::string Vm::where() const {
    if (frames.empty()) return "";
    const Frame& f = frames.back();
    const SrcPos p = posOf(*f.fn, f.pc > 0 ? f.pc - 1 : 0);
    std::string file = p.file < prog_->files.size() ? prog_->files[p.file] : std::string();
    return file + ":" + std::to_string(p.line);
}
std::string Vm::stackTrace() const {
    std::string out;
    for (std::size_t i = frames.size(); i-- > 0;) {
        const Frame& f = frames[i];
        if (f.fn->name == "<init>") continue;
        const SrcPos p = posOf(*f.fn, f.pc > 0 ? f.pc - 1 : 0);
        const std::string file = p.file < prog_->files.size() ? prog_->files[p.file] : std::string();
        out += "  " + f.fn->name + " (" + file + ":" + std::to_string(p.line) + ")\n";
        if (out.size() > 2000) { out += "  ...\n"; break; }
    }
    return out;
}

ClassInfo* Vm::classNamed(const std::string& n, Dialect d) const {
    for (const auto& c : prog_->classes) if (c->isNative && c->lang == d && c->name == n) return c.get();
    return nullptr;
}

// Errore fatale della fx-CG50 (C/C++: niente eccezioni per i guasti).
[[noreturn]] void Vm::fatal(const std::string& msg) {
    int line = 0, col = 0;
    std::string file;
    if (!frames.empty()) {
        const Frame& f = frames.back();
        const SrcPos p = posOf(*f.fn, f.pc > 0 ? f.pc - 1 : 0);
        line = static_cast<int>(p.line);
        col = p.column;
        file = p.file < prog_->files.size() ? prog_->files[p.file] : std::string();
    }
    Error e(msg, line, col, file);
    for (std::size_t i = frames.size(); i-- > 0;) {
        const Frame& f = frames[i];
        if (f.fn->name == "<init>") continue;
        const SrcPos p = posOf(*f.fn, f.pc > 0 ? f.pc - 1 : 0);
        e.trace.push_back(f.fn->name + " (" + (p.file < prog_->files.size() ? prog_->files[p.file] : std::string()) + ":" + std::to_string(p.line) + ")");
        if (e.trace.size() > 32) break;
    }
    throw e;
}
Value Vm::makeException(ClassInfo* c, const std::string& msg) {
    InstObj* o = newInstance(c, c->lang == Dialect::Cpp);
    o->fields[0] = newString(c->lang, msg);
    o->fields[2] = newString(c->lang, stackTrace());
    return Value::ref(o);
}
[[noreturn]] void Vm::fault(FaultKind k, const std::string& msg) {
    const Dialect d = lang();
    ClassInfo* c = prog_->faultClass[static_cast<int>(d)][static_cast<int>(k)];
    if (d == Dialect::CSharp || d == Dialect::Java) {
        if (k == FaultKind::Stack) fatal(d == Dialect::Java ? "StackOverflowError: " + msg : "StackOverflowException: " + msg);
        if (!c) c = prog_->faultClass[static_cast<int>(d)][static_cast<int>(FaultKind::Generic)];
        if (c) throwValue(makeException(c, msg));
    }
    if (d == Dialect::Cpp && k == FaultKind::Memory && c) throwValue(makeException(c, "std::bad_alloc"));
    // nomi della fx-CG50
    const char* tag = "Math ERROR: ";
    switch (k) {
    case FaultKind::Index: tag = "Dimension ERROR: "; break;
    case FaultKind::NullPointer: case FaultKind::DeadPointer: tag = "Pointer ERROR: "; break;
    case FaultKind::Memory: tag = "Memory ERROR: "; break;
    case FaultKind::Stack: tag = "Nesting ERROR: "; break;
    case FaultKind::Argument: case FaultKind::Format: tag = "Argument ERROR: "; break;
    case FaultKind::ReadOnly: tag = "Pointer ERROR: "; break;
    case FaultKind::Time: tag = "Time ERROR: "; break;
    case FaultKind::Io: case FaultKind::NotFound: case FaultKind::Unauthorized: tag = "Storage ERROR: "; break;
    case FaultKind::Cast: tag = "Type ERROR: "; break;
    default: break;
    }
    fatal(tag + msg);
}
[[noreturn]] void Vm::raise(const char* cls, const std::string& msg) {
    const Dialect d = lang();
    ClassInfo* c = classNamed(cls, d);
    if (!c && d == Dialect::Cpp) c = classNamed("exception", d);
    if (!c && (d == Dialect::CSharp || d == Dialect::Java)) c = prog_->exceptionRoot[static_cast<int>(d)];
    if (!c) fatal(std::string("Math ERROR: ") + msg);
    throwValue(makeException(c, msg));
}
[[noreturn]] void Vm::throwValue(const Value& v, bool rethrow) {
    if (v.isNull() && (lang() == Dialect::CSharp || lang() == Dialect::Java)) fault(FaultKind::NullPointer, "throw di null");
    if (!rethrow && !frames.empty()) {
        const Frame& f = frames.back();
        const SrcPos p = posOf(*f.fn, f.pc > 0 ? f.pc - 1 : 0);
        lastThrowLine = static_cast<int>(p.line);
        lastThrowColumn = p.column;
        lastThrowFile = p.file < prog_->files.size() ? prog_->files[p.file] : std::string();
        lastThrowTrace.clear();
        for (std::size_t i = frames.size(); i-- > 0 && lastThrowTrace.size() < 32;) {
            const Frame& g = frames[i];
            if (g.fn->name == "<init>") continue;
            const SrcPos q = posOf(*g.fn, g.pc > 0 ? g.pc - 1 : 0);
            lastThrowTrace.push_back(g.fn->name + " (" + (q.file < prog_->files.size() ? prog_->files[q.file] : std::string()) + ":" + std::to_string(q.line) + ")");
        }
        // pila nel campo dell'eccezione (StackTrace)
        if (v.holdsObj() && v.o->kind == OK::Inst) {
            InstObj* o = static_cast<InstObj*>(v.o);
            if (o->cls && o->cls->isException && o->nfields > 2) o->fields[2] = newString(o->cls->lang, stackTrace());
        }
    }
    throw ProgramThrow{v};
}
ProgramThrow Vm::memoryThrow() {
    heap_.collect();
    const Dialect d = lang();
    ClassInfo* c = prog_->faultClass[static_cast<int>(d)][static_cast<int>(FaultKind::Memory)];
    if (d == Dialect::Java) c = classNamed("OutOfMemoryError", d);
    if (!c) fatal(d == Dialect::CSharp ? "OutOfMemoryException: memoria del programma esaurita" : "Memory ERROR: memoria del programma esaurita (" + std::to_string(heap_.budget() / 1024) + " KB)");
    try {
        return ProgramThrow{makeException(c, d == Dialect::Cpp ? "std::bad_alloc" : "memoria del programma esaurita")};
    } catch (std::bad_alloc&) {
        fatal("Memory ERROR: memoria del programma esaurita");
    }
}

// ---------------------------------------------------------------- fette di tempo
std::size_t Vm::refill() {
    if (abortRequested) throw ProgramAbort{};
    if (limits_.timeLimitMs) {
        const std::uint32_t now = host_.millis();
        runElapsed += now - runStart;
        runStart = now;
        if (runElapsed > limits_.timeLimitMs) fault(FaultKind::Time, "programma oltre il limite di tempo (SET UP)");
    }
    for (;;) {
        if (budget) {
            std::size_t r = budget->remaining();
            if (r == std::numeric_limits<std::size_t>::max()) r = 256;
            if (r > 0 && !budget->exhausted()) {
                const std::size_t n = std::min<std::size_t>(r, 256);
                budget->consume(n);
                return n;
            }
        }
        yieldNow();
        if (abortRequested) throw ProgramAbort{};
        runStart = host_.millis();
    }
}
void Vm::yieldNow() {
    if (coroutine && coroutine->inside()) coroutine->yield();
    else if (!coroutine) throw ProgramAbort{};
}
void Vm::poll() {
    if (abortRequested) throw ProgramAbort{};
    if (quota > 64) { quota -= 64; return; }
    quota = refill();
}
bool Vm::readLine(std::string& out) {
    for (;;) {
        if (abortRequested) throw ProgramAbort{};
        if (host_.readLine(out)) { waitingInput = false; return true; }
        waitingInput = true;
        if (!coroutine || !coroutine->inside()) { waitingInput = false; return false; }
        coroutine->yield();
        runStart = host_.millis();
    }
}

// ---------------------------------------------------------------- debug
void Vm::debugCheck(const Function* f, std::int32_t pc) {
    const SrcPos p = f->pos[static_cast<std::size_t>(pc)];
    bool stop = false;
    if (firstStop) { firstStop = false; stop = f->name != "<init>"; }
    if (!stop) {
        for (const auto& b : breakpoints) if (b.first == p.file && b.second == static_cast<std::int32_t>(p.line)) stop = true;
    }
    if (!stop) {
        switch (stepMode) {
        case StepMode::Into: stop = static_cast<std::int32_t>(p.line) != stepLine || p.file != stepFile || frames.size() != stepDepth; break;
        case StepMode::Over: stop = frames.size() <= stepDepth && (static_cast<std::int32_t>(p.line) != stepLine || p.file != stepFile || frames.size() < stepDepth); break;
        case StepMode::Out: stop = frames.size() < stepDepth; break;
        case StepMode::Run: break;
        }
    }
    if (!stop || f->name == "<init>") return;
    paused = true;
    stepLine = static_cast<std::int32_t>(p.line);
    stepFile = p.file;
    stepDepth = frames.size();
    while (paused) {
        if (!coroutine || !coroutine->inside()) { paused = false; break; }
        coroutine->yield();
        if (abortRequested) throw ProgramAbort{};
    }
    runStart = host_.millis();
}

// ---------------------------------------------------------------- avvio
void Vm::initObjects() {
    // globali: un array di valori (gli indirizzi &g sono celle di questo oggetto)
    const std::size_t n = prog_->globalTypes.size();
    globals = heap_.make<ArrayObj>();
    globals->flags |= FRoot;
    globals->ek = VK::Ref;
    globals->esize = sizeof(Value);
    globals->count = static_cast<std::uint32_t>(n);
    if (n) {
        globals->data = static_cast<std::uint8_t*>(heap_.raw(n * sizeof(Value), alignof(Value)));
        for (std::size_t i = 0; i < n; ++i) new (globals->data + i * sizeof(Value)) Value();
        for (std::size_t i = 0; i < n; ++i) globals->set(static_cast<std::uint32_t>(i), defaultValue(prog_->globalTypes[i]));
    }
    literals.assign(prog_->strings.size(), nullptr);
    cppLiterals.assign(prog_->strings.size(), nullptr);
}
void Vm::runProgram() {
    initObjects();
    runStart = host_.millis();
    if (prog_->init >= 0) invokeFunction(prog_->functions[static_cast<std::size_t>(prog_->init)].get(), nullptr, 0, false);
    const Function* main = prog_->functions[static_cast<std::size_t>(prog_->entry)].get();
    Value args[2];
    int argc = 0;
    if (main->params.size() >= 1) {
        const TypeP pt = main->params[0].type;
        if (pt->k == TK::Array && pt->rank >= 1 && main->lang != Dialect::C && main->lang != Dialect::Cpp) {
            ArrayObj* a = newArray(pt->elem, 0);
            args[0] = Value::ref(a);
            argc = 1;
        } else {
            args[0] = Value::i32(1);
            argc = 1;
            if (main->params.size() >= 2) {
                // argv: { "programma", NULL }
                TypeP strT = main->params[1].type->elem;
                ArrayObj* argv = newArray(strT ? strT : prog_->typeAt(0), 2);
                ArrayObj* name = newCString("programma", false);
                argv->set(0, Value::ptr(name, 0));
                argv->set(1, Value::null());
                args[1] = Value::ptr(argv, 0);
                argc = 2;
            }
        }
    }
    Value r = invokeFunction(main, args, argc, prog_->entryReturnsInt);
    if (prog_->entryReturnsInt) throw ProgramExit{static_cast<int>(r.integer())};
    throw ProgramExit{0};
}

void Vm::pushFrame(const Function* f, int argc) {
    if (frames.size() >= limits_.callDepth) {
        const Dialect d = f->lang;
        if (d == Dialect::CSharp) fatal("StackOverflowException: ricorsione oltre " + std::to_string(limits_.callDepth) + " chiamate");
        if (d == Dialect::Java) {
            ClassInfo* so = classNamed("StackOverflowError", Dialect::Java);
            if (so) throwValue(makeException(so, "ricorsione oltre " + std::to_string(limits_.callDepth) + " chiamate"));
        }
        fatal("Nesting ERROR: ricorsione oltre " + std::to_string(limits_.callDepth) + " chiamate");
    }
    if (!f->defined && f->code.empty()) {
        if (f->isAbstract) fault(FaultKind::NotImplemented, "metodo astratto " + f->name + " senza implementazione");
        fatal("Syntax ERROR: funzione '" + f->name + "' dichiarata ma non definita");
    }
    Frame fr;
    fr.fn = f;
    fr.pc = 0;
    fr.base = static_cast<std::uint32_t>(sp - static_cast<std::size_t>(argc));
    fr.serial = ++serials;
    const std::size_t need = fr.base + static_cast<std::size_t>(f->nlocals) + 64;
    if (need > stack.size()) fault(FaultKind::Stack, "pila dei valori piena (" + std::to_string(stack.size()) + " valori)");
    for (std::size_t i = sp; i < fr.base + static_cast<std::size_t>(f->nlocals); ++i) stack[i] = Value();
    sp = std::max<std::size_t>(sp, fr.base + static_cast<std::size_t>(f->nlocals));
    frames.push_back(fr);
}

Value Vm::invokeFunction(const Function* f, const Value* args, int argc, bool hasResult) {
    if (++nativeDepth > 48) { --nativeDepth; fatal("Nesting ERROR: troppe chiamate annidate dalle funzioni della libreria"); }
    struct Guard { int& d; ~Guard() { --d; } } g{nativeDepth};
    const std::size_t depth = frames.size();
    if (sp + static_cast<std::size_t>(argc) + 2 >= stack.size()) fault(FaultKind::Stack, "pila dei valori piena");
    const std::size_t base = sp;
    for (int i = 0; i < argc; ++i) stack[sp++] = args[i];
    pushFrame(f, argc);
    execute(depth);
    Value r;
    if (hasResult && sp > base) r = stack[--sp];
    sp = base;
    return r;
}
Value Vm::invoke(const Value& fn, const Value* args, int argc) {
    if (fn.k != VK::Fn) fault(FaultKind::NullPointer, "chiamata di una funzione nulla");
    std::vector<Value> full;
    const Function* f = nullptr;
    if (fn.f == 1) {
        ClosureObj* c = static_cast<ClosureObj*>(fn.o);
        f = prog_->functions[static_cast<std::size_t>(c->fn)].get();
        full.push_back(c->bound ? c->caps[0] : Value::ref(c));
    } else if (fn.f == 0) {
        f = prog_->functions[static_cast<std::size_t>(fn.i)].get();
    } else fault(FaultKind::Generic, "valore non chiamabile");
    for (int i = 0; i < argc; ++i) full.push_back(args[i]);
    return invokeFunction(f, full.data(), static_cast<int>(full.size()), f->ret && f->ret->k != TK::Void);
}

// ---------------------------------------------------------------- eccezioni
namespace {
bool catches(const Vm& vm, const Handler& h, const Value& v) {
    (void)vm;
    if (!h.catches && !h.catchType) return true;
    if (h.catches) {
        if (v.k != VK::Ref && v.k != VK::Ptr) return false;
        if (!v.o || v.o->kind != OK::Inst) return false;
        const InstObj* o = static_cast<const InstObj*>(v.o);
        return o->cls && o->cls->derivesFrom(h.catches);
    }
    // C++: catch di un tipo semplice (int, const char*, Classe*)
    const TypeP t = h.catchType;
    if (t->k == TK::Ptr) {
        if (v.k != VK::Ptr && v.k != VK::Null) return false;
        if (t->elem && t->elem->k == TK::Class && v.o && v.o->kind == OK::Inst) return static_cast<const InstObj*>(v.o)->cls->derivesFrom(t->elem->cls);
        if (t->elem && (t->elem->k == TK::C8) && v.o && v.o->kind == OK::Array) return static_cast<const ArrayObj*>(v.o)->ek == VK::C8;
        return v.k == VK::Null;
    }
    if (t->k == TK::Str) return v.o && v.o->kind == OK::StrB;
    return vkOf(t) == v.k;
}
} // namespace

void Vm::unwind(ProgramThrow& t, std::size_t stopDepth) {
    while (frames.size() > stopDepth) {
        Frame& fr = frames.back();
        const std::int32_t pc = fr.pc - 1;
        for (const Handler& h : fr.fn->handlers) {
            if (pc < h.start || pc >= h.end) continue;
            if (!catches(*this, h, t.value)) continue;
            sp = fr.base + static_cast<std::size_t>(fr.fn->nlocals) + h.depth;
            if (h.slot >= 0) stack[fr.base + static_cast<std::size_t>(h.slot)] = t.value;
            else stack[sp++] = t.value;
            fr.pc = h.target;
            return;
        }
        sp = fr.base;
        frames.pop_back();
    }
    throw t;                                     // a chi ha chiamato questo ciclo (nativo o programma)
}

// ================================================================ ciclo delle istruzioni
#define FX_PUSH(v) do { if (sp >= stackCap) fault(FaultKind::Stack, "pila dei valori piena"); stack[sp++] = (v); } while (0)

void Vm::execute(std::size_t stopDepth) {
    const std::size_t tempMark = temps_.size();
    const std::size_t stackCap = stack.size();
    for (;;) {
        try {
            while (frames.size() > stopDepth) {
                Frame* fr = &frames.back();
                const Function* f = fr->fn;
                if (quota == 0) quota = refill();
                --quota;
                ++executed;
                if (static_cast<std::size_t>(fr->pc) >= f->code.size()) fatal("Syntax ERROR: fine della funzione " + f->name + " senza return");
                if (debug && f->stmt[static_cast<std::size_t>(fr->pc)]) { debugCheck(f, fr->pc); fr = &frames.back(); }
                const Instr& in = f->code[static_cast<std::size_t>(fr->pc++)];
                Value* const S = stack.data();
                const std::size_t base = fr->base;
                switch (in.op) {
                case Op::Nop: break;
                case Op::PushI: { Value v; v.k = static_cast<VK>(in.b); if (isFloat(v.k)) v.d = in.a; else v.i = wrap(v.k, in.a); FX_PUSH(v); break; }
                case Op::PushL: { Value v; v.k = static_cast<VK>(in.b); v.i = prog_->ints[static_cast<std::size_t>(in.a)]; if (isFloat(v.k)) v.d = static_cast<double>(v.i); FX_PUSH(v); break; }
                case Op::PushD: { const double d = prog_->doubles[static_cast<std::size_t>(in.a)]; FX_PUSH(in.b == static_cast<std::uint8_t>(VK::F32) ? Value::f32(d) : Value::f64(d)); break; }
                case Op::PushS: {
                    const std::size_t idx = static_cast<std::size_t>(in.a);
                    if (in.b == 0) {
                        // letterale del C: array di char in sola lettura, sempre lo stesso
                        if (!literals[idx]) { ArrayObj* a = newCString(prog_->strings[idx].bytes, true); a->flags |= FRoot; literals[idx] = a; }
                        FX_PUSH(Value::ref(literals[idx]));
                    } else if (in.b == 1) {
                        FX_PUSH(Value::ref(newStrB(prog_->strings[idx].bytes)));
                    } else {
                        if (!literals[idx]) { StrWObj* s = newStrW(prog_->strings[idx].wide); s->flags |= FRoot; literals[idx] = s; }
                        FX_PUSH(Value::ref(literals[idx]));
                    }
                    break;
                }
                case Op::PushNull: FX_PUSH(Value::null()); break;
                case Op::PushVoid: FX_PUSH(Value()); break;
                case Op::PushFn: FX_PUSH(Value::fn(in.a)); break;
                case Op::LoadL: FX_PUSH(S[base + static_cast<std::size_t>(in.a)]); break;
                case Op::StoreL: S[base + static_cast<std::size_t>(in.a)] = in.b ? S[sp - 1] : S[--sp]; break;
                case Op::LoadG: FX_PUSH(globals->get(static_cast<std::uint32_t>(in.a))); break;
                case Op::StoreG: globals->set(static_cast<std::uint32_t>(in.a), in.b ? S[sp - 1] : S[--sp]); break;
                case Op::NewBox: { BoxObj* b = heap_.make<BoxObj>(); b->v = S[base + static_cast<std::size_t>(in.a)]; S[base + static_cast<std::size_t>(in.a)] = Value::ref(b); break; }
                case Op::LoadBox: { Obj* o = S[base + static_cast<std::size_t>(in.a)].o; FX_PUSH(static_cast<BoxObj*>(o)->v); break; }
                case Op::StoreBox: { Obj* o = S[base + static_cast<std::size_t>(in.a)].o; static_cast<BoxObj*>(o)->v = in.b ? S[sp - 1] : S[--sp]; break; }
                case Op::AddrL: {
                    if (in.b == 1) { FX_PUSH(Value::ptr(S[base + static_cast<std::size_t>(in.a)].o, 0)); break; }
                    Value p;
                    p.k = VK::Ptr;
                    p.f = 2;                              // puntatore a una variabile della pila
                    p.idx = static_cast<std::uint32_t>(base + static_cast<std::size_t>(in.a));
                    p.aux = static_cast<std::uint16_t>(std::min<std::size_t>(frames.size() - 1, 65535));
                    p.u = fr->serial;
                    FX_PUSH(p);
                    break;
                }
                case Op::AddrG: FX_PUSH(Value::ptr(globals, static_cast<std::uint32_t>(in.a))); break;
                case Op::KillL: {
                    Value& v = S[base + static_cast<std::size_t>(in.a)];
                    if (f->lang == Dialect::C || f->lang == Dialect::Cpp) killValue(v);
                    v = Value();
                    break;
                }
                case Op::Pop: --sp; break;
                case Op::Dup: {
                    const std::size_t n = in.b ? in.b : 1;
                    if (sp + n > stackCap) fault(FaultKind::Stack, "pila dei valori piena");
                    for (std::size_t i = 0; i < n; ++i) S[sp + i] = S[sp - n + i];
                    sp += n;
                    break;
                }
                case Op::DupX: {
                    const std::size_t n = in.b;
                    if (sp + 1 > stackCap) fault(FaultKind::Stack, "pila dei valori piena");
                    const Value top = S[sp - 1];
                    for (std::size_t i = 0; i <= n; ++i) S[sp - i] = S[sp - 1 - i];
                    S[sp - 1 - n] = top;
                    ++sp;
                    break;
                }
                case Op::Swap: std::swap(S[sp - 1], S[sp - 2]); break;
                case Op::GetF: {
                    InstObj* o = static_cast<InstObj*>(objOf(S[sp - 1]));
                    if (o->kind != OK::Inst || static_cast<std::uint32_t>(in.a) >= o->nfields) fault(FaultKind::Generic, "campo non valido");
                    S[sp - 1] = o->fields[in.a];
                    break;
                }
                case Op::SetF: {
                    InstObj* o = static_cast<InstObj*>(objOf(S[sp - 2]));
                    if (o->kind != OK::Inst || static_cast<std::uint32_t>(in.a) >= o->nfields) fault(FaultKind::Generic, "campo non valido");
                    o->fields[in.a] = S[sp - 1];
                    if (in.b) { S[sp - 2] = S[sp - 1]; --sp; } else sp -= 2;
                    break;
                }
                case Op::AddrF: {
                    Obj* o = objOf(S[sp - 1]);
                    S[sp - 1] = Value::ptr(o, static_cast<std::uint32_t>(in.a));
                    break;
                }
                case Op::GetE: case Op::SetE: case Op::AddrE: {
                    const std::size_t args = in.op == Op::SetE ? 3 : 2;
                    Obj* o = objOf(S[sp - args]);
                    if (o->kind != OK::Array) fault(FaultKind::Generic, "indice su un valore che non e' un array");
                    ArrayObj* a = static_cast<ArrayObj*>(o);
                    const std::int64_t i = S[sp - args + 1].integer();
                    if (a->ek == VK::Void) fault(FaultKind::Generic, "memoria di malloc usata prima di avere un tipo (manca il cast?)");
                    if (i < 0 || static_cast<std::uint64_t>(i) >= a->count) {
                        const Dialect d = f->lang;
                        fault(FaultKind::Index, d == Dialect::CSharp ? "l'indice era fuori dai limiti della matrice" :
                              "indice " + std::to_string(i) + " fuori da [0, " + std::to_string(a->count == 0 ? -1 : static_cast<std::int64_t>(a->count) - 1) + "]");
                    }
                    if (in.op == Op::GetE) { S[sp - 2] = a->get(static_cast<std::uint32_t>(i)); --sp; }
                    else if (in.op == Op::AddrE) { S[sp - 2] = Value::ptr(a, static_cast<std::uint32_t>(i)); --sp; }
                    else {
                        if (a->flags & FReadOnly) fault(FaultKind::ReadOnly, "scrittura su una stringa letterale");
                        a->set(static_cast<std::uint32_t>(i), S[sp - 1]);
                        if (in.b) { S[sp - 3] = S[sp - 1]; sp -= 2; } else sp -= 3;
                    }
                    break;
                }
                case Op::GetE2: case Op::SetE2: case Op::AddrE2: {
                    const std::size_t args = in.op == Op::SetE2 ? 4 : 3;
                    ArrayObj* a = static_cast<ArrayObj*>(objOf(S[sp - args]));
                    if (a->kind != OK::Array || a->rank != 2) fault(FaultKind::Generic, "array [,] atteso");
                    const std::int64_t r = S[sp - args + 1].integer(), c = S[sp - args + 2].integer();
                    if (r < 0 || c < 0 || static_cast<std::uint64_t>(r) >= a->rows || static_cast<std::uint64_t>(c) >= a->cols)
                        fault(FaultKind::Index, "l'indice era fuori dai limiti della matrice");
                    const std::uint32_t i = static_cast<std::uint32_t>(r) * a->cols + static_cast<std::uint32_t>(c);
                    if (in.op == Op::GetE2) { S[sp - 3] = a->get(i); sp -= 2; }
                    else if (in.op == Op::AddrE2) { S[sp - 3] = Value::ptr(a, i); sp -= 2; }
                    else { a->set(i, S[sp - 1]); if (in.b) { S[sp - 4] = S[sp - 1]; sp -= 3; } else sp -= 4; }
                    break;
                }
                case Op::ArrLen: {
                    Obj* o = objOf(S[sp - 1]);
                    if (o->kind != OK::Array) fault(FaultKind::Generic, "Length di un valore che non e' un array");
                    const ArrayObj* a = static_cast<const ArrayObj*>(o);
                    S[sp - 1] = in.b == 1 ? Value::i64(a->count) : Value::i32(a->count);
                    break;
                }
                case Op::LoadP: S[sp - 1] = loadPtr(S[sp - 1]); break;
                case Op::StoreP: {
                    storePtr(S[sp - 2], S[sp - 1]);
                    if (in.b) { S[sp - 2] = S[sp - 1]; --sp; } else sp -= 2;
                    break;
                }
                case Op::IncL: case Op::IncG: case Op::IncP: {
                    Value cur;
                    Value ptr;
                    if (in.op == Op::IncL) cur = S[base + static_cast<std::size_t>(in.a)];
                    else if (in.op == Op::IncG) cur = globals->get(static_cast<std::uint32_t>(in.a));
                    else { ptr = S[--sp]; cur = loadPtr(ptr); }
                    const bool dec = (in.b & 2) != 0;
                    Value nv = cur;
                    if (isFloat(cur.k)) nv.d = cur.d + (dec ? -1.0 : 1.0), nv = cur.k == VK::F32 ? Value::f32(nv.d) : nv;
                    else if (cur.k == VK::Ptr) {
                        if (cur.f == 2) fault(FaultKind::Generic, "aritmetica su un puntatore a una variabile semplice");
                        if (!cur.o) fault(FaultKind::NullPointer, "++ su un puntatore NULL");
                        nv.idx = cur.idx + (dec ? 0xFFFFFFFFu : 1u);
                    } else if (isInt(cur.k) || cur.k == VK::Bool) nv.i = wrap(cur.k, static_cast<std::int64_t>(static_cast<std::uint64_t>(cur.i) + (dec ? ~0ull : 1ull)));
                    else if (cur.k == VK::Void) fault(FaultKind::Generic, "++ su una variabile senza valore");
                    if (in.op == Op::IncL) S[base + static_cast<std::size_t>(in.a)] = nv;
                    else if (in.op == Op::IncG) globals->set(static_cast<std::uint32_t>(in.a), nv);
                    else storePtr(ptr, nv);
                    FX_PUSH((in.b & 1) ? cur : nv);
                    break;
                }
                case Op::Conv: S[sp - 1] = convertTo(S[sp - 1], static_cast<VK>(in.b)); break;
                case Op::ConvT: {
                    TypeP t = prog_->typeAt(in.a);
                    S[sp - 1] = castTo(S[sp - 1], t);
                    if (in.b == 1 && S[sp - 1].o) S[sp - 1].o->flags |= FExplicit;
                    break;
                }
                case Op::ToStr: {
                    if (in.a > 0) {
                        // enum: il nome del valore
                        TypeP t = prog_->typeAt(in.a);
                        if (t->k == TK::Enum && t->cls) {
                            const std::int64_t v = S[sp - 1].integer();
                            for (const auto& ev : t->cls->enumValues) if (ev.second == v) { S[sp - 1] = newString(static_cast<Dialect>(in.b), ev.first); goto toStrDone; }
                        }
                    }
                    S[sp - 1] = toLangString(S[sp - 1], static_cast<Dialect>(in.b));
                toStrDone:
                    break;
                }
                case Op::ToBool: S[sp - 1] = Value::boolean(truthy(S[sp - 1])); break;
                case Op::Add: case Op::Sub: case Op::Mul: case Op::Div: case Op::Rem:
                case Op::And: case Op::Or: case Op::Xor: case Op::Shl: case Op::Shr: case Op::UShr: {
                    Value& a = S[sp - 2];
                    const Value b = S[sp - 1];
                    --sp;
                    if (a.k == VK::Ptr || b.k == VK::Ptr) {
                        // aritmetica dei puntatori
                        if (in.op == Op::Sub && a.k == VK::Ptr && b.k == VK::Ptr) {
                            if (a.o != b.o || a.f == 2 || b.f == 2) fault(FaultKind::Generic, "differenza tra puntatori di oggetti diversi");
                            a = Value::i64(static_cast<std::int64_t>(a.idx) - static_cast<std::int64_t>(b.idx));
                            break;
                        }
                        Value p = a.k == VK::Ptr ? a : b;
                        const std::int64_t n = (a.k == VK::Ptr ? b : a).integer();
                        if (p.f == 2) fault(FaultKind::Generic, "aritmetica su un puntatore a una variabile semplice");
                        if (!p.o) fault(FaultKind::NullPointer, "aritmetica su un puntatore NULL");
                        if (p.idx == Whole) p.idx = 0;
                        const std::int64_t ni = static_cast<std::int64_t>(p.idx) + (in.op == Op::Sub ? -n : n);
                        if (ni < -1 || ni > 0x7FFFFFFF) fault(FaultKind::Index, "puntatore fuori dall'oggetto");
                        p.idx = static_cast<std::uint32_t>(ni);
                        a = p;
                        break;
                    }
                    VK k = a.k == b.k ? a.k : commonKind(a.k, b.k);
                    if (a.k != k) a = convertTo(a, k);
                    const Value bb = b.k != k && in.op != Op::Shl && in.op != Op::Shr && in.op != Op::UShr ? convertTo(b, k) : b;
                    if (isFloat(k)) {
                        double r = 0;
                        switch (in.op) {
                        case Op::Add: r = a.d + bb.d; break;
                        case Op::Sub: r = a.d - bb.d; break;
                        case Op::Mul: r = a.d * bb.d; break;
                        case Op::Div: r = a.d / bb.d; break;
                        case Op::Rem: r = std::fmod(a.d, bb.d); break;
                        default: fault(FaultKind::Generic, "operazione su bit con numeri reali");
                        }
                        a = k == VK::F32 ? Value::f32(r) : Value::f64(r);
                        break;
                    }
                    if (k == VK::Void || k == VK::Null || k == VK::Ref || k == VK::Fn) fault(FaultKind::Generic, "operazione aritmetica su un valore non numerico");
                    const std::uint64_t x = static_cast<std::uint64_t>(a.i), y = static_cast<std::uint64_t>(bb.i);
                    const bool uns = unsignedKind(k);
                    std::int64_t r = 0;
                    switch (in.op) {
                    case Op::Add: r = static_cast<std::int64_t>(x + y); break;
                    case Op::Sub: r = static_cast<std::int64_t>(x - y); break;
                    case Op::Mul: r = static_cast<std::int64_t>(x * y); break;
                    case Op::Div: case Op::Rem: {
                        if (y == 0) fault(FaultKind::DivZero, f->lang == Dialect::CSharp ? "tentativo di dividere per zero" : f->lang == Dialect::Java ? "/ by zero" : "divisione per zero");
                        if (uns) {
                            const std::uint64_t ux = k == VK::U64 ? x : static_cast<std::uint64_t>(a.i), uy = k == VK::U64 ? y : static_cast<std::uint64_t>(bb.i);
                            r = static_cast<std::int64_t>(in.op == Op::Div ? ux / uy : ux % uy);
                        } else {
                            const std::int64_t sx = a.i, sy = bb.i;
                            const std::int64_t minK = k == VK::I32 ? INT32_MIN : k == VK::I64 ? INT64_MIN : INT64_MIN;
                            if (sy == -1 && sx == minK) {
                                if (f->lang == Dialect::CSharp && in.op == Op::Div) fault(FaultKind::Overflow, "l'operazione aritmetica ha causato un overflow");
                                r = in.op == Op::Div ? sx : 0;
                            } else r = in.op == Op::Div ? sx / sy : sx % sy;
                        }
                        break;
                    }
                    case Op::And: r = static_cast<std::int64_t>(x & y); break;
                    case Op::Or: r = static_cast<std::int64_t>(x | y); break;
                    case Op::Xor: r = static_cast<std::int64_t>(x ^ y); break;
                    case Op::Shl: case Op::Shr: case Op::UShr: {
                        const int bits = bitsOf(k) < 32 ? 32 : bitsOf(k);
                        const unsigned s = static_cast<unsigned>(bb.i) & static_cast<unsigned>(bits - 1);
                        if (in.op == Op::Shl) r = static_cast<std::int64_t>(x << s);
                        else if (in.op == Op::UShr) r = static_cast<std::int64_t>((bits == 32 ? (x & 0xFFFFFFFFull) : x) >> s);
                        else r = uns ? static_cast<std::int64_t>((k == VK::U64 ? x : (x & (bits == 32 ? 0xFFFFFFFFull : ~0ull))) >> s) : (a.i >> s);
                        break;
                    }
                    default: break;
                    }
                    a.k = k;
                    a.i = wrap(k, r);
                    break;
                }
                case Op::Neg: {
                    Value& a = S[sp - 1];
                    if (isFloat(a.k)) a.d = -a.d;
                    else if (isInt(a.k)) a.i = wrap(a.k, static_cast<std::int64_t>(0u - static_cast<std::uint64_t>(a.i)));
                    else fault(FaultKind::Generic, "- su un valore non numerico");
                    break;
                }
                case Op::Not: S[sp - 1] = Value::boolean(!truthy(S[sp - 1])); break;
                case Op::BitNot: { Value& a = S[sp - 1]; if (!isInt(a.k)) fault(FaultKind::Generic, "~ su un valore non intero"); a.i = wrap(a.k, ~a.i); break; }
                case Op::Eq: case Op::Ne: case Op::Lt: case Op::Le: case Op::Gt: case Op::Ge: {
                    const Value a = S[sp - 2], b = S[sp - 1];
                    --sp;
                    int c = 0;
                    bool unordered = false;
                    if ((isNum(a.k)) && (isNum(b.k))) {
                        const VK k = a.k == b.k ? a.k : commonKind(a.k, b.k);
                        if (isFloat(k)) {
                            const double x = a.num(), y = b.num();
                            if (std::isnan(x) || std::isnan(y)) unordered = true;
                            c = x < y ? -1 : x > y ? 1 : 0;
                        } else if (k == VK::U64) {
                            const std::uint64_t x = a.k == VK::U64 ? a.u : static_cast<std::uint64_t>(a.i), y = b.k == VK::U64 ? b.u : static_cast<std::uint64_t>(b.i);
                            c = x < y ? -1 : x > y ? 1 : 0;
                        } else c = a.i < b.i ? -1 : a.i > b.i ? 1 : 0;
                    } else if (a.k == VK::Ptr || b.k == VK::Ptr || a.isNull() || b.isNull()) {
                        if (a.isNull() || b.isNull()) c = (a.isNull() && b.isNull()) ? 0 : 1;
                        else if (a.f == 2 || b.f == 2) c = (a.f == b.f && a.idx == b.idx && a.u == b.u) ? 0 : (a.idx < b.idx ? -1 : 1);
                        else if (a.k == VK::Ptr && b.k == VK::Ptr) {
                            const std::uint32_t ia = a.idx == Whole ? 0 : a.idx, ib = b.idx == Whole ? 0 : b.idx;
                            if (a.o != b.o) { if (in.op != Op::Eq && in.op != Op::Ne) fault(FaultKind::Generic, "confronto < > tra puntatori di oggetti diversi"); c = 1; }
                            else c = ia < ib ? -1 : ia > ib ? 1 : 0;
                        } else c = (a.holdsObj() && b.holdsObj() && a.o == b.o) ? 0 : 1;
                    } else if (a.k == VK::Ref && b.k == VK::Ref) c = a.o == b.o ? 0 : 1;
                    else if (a.k == VK::Fn && b.k == VK::Fn) c = (a.f == b.f && (a.f == 0 ? a.i == b.i : a.o == b.o)) ? 0 : 1;
                    else c = 1;
                    bool r = false;
                    switch (in.op) {
                    case Op::Eq: r = !unordered && c == 0; break;
                    case Op::Ne: r = unordered || c != 0; break;
                    case Op::Lt: r = !unordered && c < 0; break;
                    case Op::Le: r = !unordered && c <= 0; break;
                    case Op::Gt: r = !unordered && c > 0; break;
                    default: r = !unordered && c >= 0; break;
                    }
                    S[sp - 1] = Value::boolean(r);
                    if (f->lang == Dialect::C) S[sp - 1] = Value::i32(r ? 1 : 0);
                    break;
                }
                case Op::Concat: {
                    const std::u16string a = S[sp - 2].isNull() ? std::u16string() : utf16(S[sp - 2]);
                    const std::u16string b = S[sp - 1].isNull() ? std::u16string() : utf16(S[sp - 1]);
                    if (a.size() + b.size() > 4u * 1024u * 1024u) fault(FaultKind::Memory, "stringa troppo lunga");
                    StrWObj* s = newStrW(a + b);
                    --sp;
                    S[sp - 1] = Value::ref(s);
                    break;
                }
                case Op::StrEq: {
                    const Value a = S[sp - 2], b = S[sp - 1];
                    --sp;
                    bool r = false;
                    if (a.isNull() || b.isNull()) r = a.isNull() && b.isNull();
                    else r = utf16(a) == utf16(b);
                    S[sp - 1] = Value::boolean(r);
                    break;
                }
                case Op::StrCmp: {
                    const int c = compareValues(S[sp - 2], S[sp - 1]);
                    --sp;
                    S[sp - 1] = Value::i32(c);
                    break;
                }
                case Op::Jmp: fr->pc = in.a; break;
                case Op::Jz: if (!truthy(S[--sp])) fr->pc = in.a; break;
                case Op::Jnz: if (truthy(S[--sp])) fr->pc = in.a; break;
                case Op::JzKeep: if (!truthy(S[sp - 1])) fr->pc = in.a; break;
                case Op::JnzKeep: if (truthy(S[sp - 1])) fr->pc = in.a; break;
                case Op::Switch: {
                    const SwitchTable& t = prog_->switches[static_cast<std::size_t>(in.a)];
                    const Value v = S[--sp];
                    std::int32_t target = t.fallback;
                    if (t.strings) {
                        if (!v.isNull()) {
                            const std::u16string s = utf16(v);
                            for (const auto& c : t.strCases) if (c.first == s) { target = c.second; break; }
                        }
                    } else {
                        const std::int64_t x = v.integer();
                        auto it = std::lower_bound(t.cases.begin(), t.cases.end(), std::make_pair(x, INT32_MIN));
                        if (it != t.cases.end() && it->first == x) target = it->second;
                    }
                    fr->pc = target;
                    break;
                }
                case Op::Call: {
                    const Function* g = prog_->functions[static_cast<std::size_t>(in.a)].get();
                    pushFrame(g, in.b);
                    break;
                }
                case Op::CallV: case Op::CallI: {
                    const Value& recv = S[sp - in.b];
                    Obj* o = objOf(recv);
                    if (o->kind != OK::Inst) fault(FaultKind::Generic, "metodo su un valore che non e' un oggetto");
                    ClassInfo* c = static_cast<InstObj*>(o)->cls;
                    const Function* g = nullptr;
                    if (in.op == Op::CallV) {
                        if (static_cast<std::size_t>(in.a) >= c->vtable.size()) fault(FaultKind::Generic, "metodo virtuale non trovato");
                        g = c->vtable[static_cast<std::size_t>(in.a)];
                    } else {
                        const std::string& sel = prog_->selectors[static_cast<std::size_t>(in.a)];
                        auto it = c->selectors.find(sel);
                        if (it == c->selectors.end()) fault(FaultKind::Cast, "l'oggetto non implementa " + sel.substr(0, sel.find('/')));
                        g = it->second;
                    }
                    if (g->isAbstract && !g->defined) fault(FaultKind::NotImplemented, "metodo astratto " + g->name);
                    pushFrame(g, in.b);
                    break;
                }
                case Op::CallN: {
                    NativeCall call;
                    call.argc = in.b;
                    call.args = S + (sp - in.b);
                    call.lang = f->lang;
                    const std::uint32_t tix = in.c >> 1;
                    call.type = tix && tix < prog_->types.size() ? prog_->typeAt(static_cast<std::int32_t>(tix)) : nullptr;
                    const std::size_t mark = temps_.size();
                    ++nativeDepth;
                    try {
                        nativeAt(in.a)(*this, call);
                    } catch (...) {
                        --nativeDepth;
                        throw;
                    }
                    --nativeDepth;
                    sp -= in.b;
                    if (in.c & 1) FX_PUSH(call.ret);
                    unroot(mark);
                    break;
                }
                case Op::CallFn: {
                    const std::size_t fnPos = sp - in.b - 1;
                    const Value fv = S[fnPos];
                    if (fv.k != VK::Fn) fault(FaultKind::NullPointer, f->lang == Dialect::C || f->lang == Dialect::Cpp ? "chiamata di un puntatore a funzione NULL" : "delegato null");
                    if (fv.f == 0) {
                        for (std::size_t i = fnPos; i + 1 < sp; ++i) S[i] = S[i + 1];
                        --sp;
                        pushFrame(prog_->functions[static_cast<std::size_t>(fv.i)].get(), in.b);
                    } else if (fv.f == 1) {
                        ClosureObj* c = static_cast<ClosureObj*>(fv.o);
                        S[fnPos] = c->bound ? c->caps[0] : Value::ref(c);
                        pushFrame(prog_->functions[static_cast<std::size_t>(c->fn)].get(), in.b + 1);
                    } else fault(FaultKind::Generic, "valore non chiamabile");
                    break;
                }
                case Op::Ret: {
                    Value r;
                    if (in.b) r = S[sp - 1];
                    sp = fr->base;
                    frames.pop_back();
                    if (in.b) FX_PUSH(r);
                    break;
                }
                case Op::New: {
                    ClassInfo* c = prog_->classes[static_cast<std::size_t>(in.a)].get();
                    InstObj* o = newInstance(c, in.c == 1);
                    FX_PUSH(Value::ref(o));
                    break;
                }
                case Op::NewArr: {
                    TypeP et = prog_->typeAt(in.a);
                    const std::size_t dims = in.b ? in.b : 1;
                    const bool explicitAlloc = (in.c & 1) != 0;
                    if (in.c == 2) {
                        const std::int64_t r = S[sp - 2].integer(), c = S[sp - 1].integer();
                        if (r < 0 || c < 0) fault(FaultKind::Overflow, "dimensione negativa");
                        if (static_cast<std::uint64_t>(r) * static_cast<std::uint64_t>(c) > 50000000ull) fault(FaultKind::Memory, "array troppo grande");
                        ArrayObj* a = newArray(et, static_cast<std::uint32_t>(r * c));
                        a->rank = 2;
                        a->rows = static_cast<std::uint32_t>(r);
                        a->cols = static_cast<std::uint32_t>(c);
                        sp -= 2;
                        FX_PUSH(Value::ref(a));
                        break;
                    }
                    // dimensioni annidate (C: int m[3][4]): array di array
                    std::int64_t n[8];
                    for (std::size_t i = 0; i < dims && i < 8; ++i) {
                        n[i] = S[sp - dims + i].integer();
                        if (n[i] < 0) fault(f->lang == Dialect::CSharp ? FaultKind::Overflow : FaultKind::Argument, "dimensione negativa dell'array");
                        if (n[i] > 50000000) fault(FaultKind::Memory, "array troppo grande");
                    }
                    sp -= dims;
                    std::function<ArrayObj*(std::size_t, TypeP)> build = [&](std::size_t d, TypeP inner) -> ArrayObj* {
                        if (d + 1 == dims) return newArray(inner, static_cast<std::uint32_t>(n[d]), explicitAlloc && d == 0);
                        ArrayObj* outer = newArray(nullptr, static_cast<std::uint32_t>(n[d]), explicitAlloc && d == 0);
                        for (std::int64_t k = 0; k < n[d]; ++k) {
                            ArrayObj* sub = build(d + 1, inner);
                            outer->set(static_cast<std::uint32_t>(k), Value::ref(sub));
                        }
                        return outer;
                    };
                    ArrayObj* a = build(0, et);
                    if (explicitAlloc) a->flags |= FArrayNew;
                    FX_PUSH(Value::ref(a));
                    break;
                }
                case Op::ArrLit: {
                    TypeP et = prog_->typeAt(in.a);
                    const std::size_t n = in.c;
                    ArrayObj* a = newArray(et, static_cast<std::uint32_t>(n));
                    for (std::size_t i = 0; i < n; ++i) a->set(static_cast<std::uint32_t>(i), S[sp - n + i]);
                    sp -= n;
                    FX_PUSH(Value::ref(a));
                    break;
                }
                case Op::NewNative: FX_PUSH(newNative(prog_->typeAt(in.a))); break;
                case Op::Copy: S[sp - 1] = copyValue(S[sp - 1], in.a); break;
                case Op::CopyInto: {
                    const Value dst = S[sp - 2], src = S[sp - 1];
                    sp -= 2;
                    copyInto(dst, src);
                    break;
                }
                case Op::Destroy: {
                    const Value v = S[--sp];
                    destroyValue(v, in.a >= 0 ? prog_->classes[static_cast<std::size_t>(in.a)].get() : nullptr);
                    break;
                }
                case Op::Delete: {
                    const Value v = S[--sp];
                    if (v.isNull()) break;
                    if (v.k == VK::Ptr && v.f == 2) fault(FaultKind::Generic, "delete di una variabile locale (non e' stata creata con new)");
                    Obj* o = v.o;
                    if (!o) break;
                    if (o->dead()) fault(FaultKind::DeadPointer, "delete di memoria gia' liberata");
                    if (!(o->flags & FExplicit)) fault(FaultKind::Generic, "delete di memoria non creata con new");
                    if (v.k == VK::Ptr && v.idx != 0 && v.idx != Whole) fault(FaultKind::Generic, "delete di un puntatore che non punta all'inizio");
                    const bool isArr = (o->flags & FArrayNew) != 0;
                    if (in.b && !isArr) fault(FaultKind::Generic, "delete[] di memoria creata con new (senza [])");
                    if (!in.b && isArr) fault(FaultKind::Generic, "delete di un array creato con new[]: usa delete[]");
                    ClassInfo* sc = in.a >= 0 ? prog_->classes[static_cast<std::size_t>(in.a)].get() : nullptr;
                    if (o->kind == OK::Array) {
                        ArrayObj* a = static_cast<ArrayObj*>(o);
                        if (a->refs()) for (std::uint32_t i = a->count; i-- > 0;) { const Value e = a->get(i); if (e.k == VK::Ref) destroyValue(e, sc); }
                        if (a->count == 1 && !a->refs()) {}
                    } else {
                        destroyValue(Value::ref(o), sc);
                    }
                    killValue(Value::ref(o));
                    break;
                }
                case Op::Free: {
                    const Value v = S[--sp];
                    if (v.isNull()) break;
                    if (v.k == VK::Ptr && v.f == 2) fault(FaultKind::Generic, "free di una variabile locale (non e' memoria di malloc)");
                    Obj* o = v.o;
                    if (!o) break;
                    if (o->dead()) fault(FaultKind::DeadPointer, "free di memoria gia' liberata (doppio free)");
                    if (!(o->flags & FExplicit)) fault(FaultKind::Generic, "free di memoria non allocata con malloc");
                    if (v.k == VK::Ptr && v.idx != 0 && v.idx != Whole) fault(FaultKind::Generic, "free di un puntatore che non punta all'inizio del blocco");
                    killValue(Value::ref(o));
                    break;
                }
                case Op::InstOf: {
                    const Value v = S[sp - 1];
                    ClassInfo* c = prog_->classes[static_cast<std::size_t>(in.a)].get();
                    bool r = false;
                    if (v.holdsObj() && v.o->kind == OK::Inst) r = static_cast<InstObj*>(v.o)->cls->derivesFrom(c);
                    S[sp - 1] = Value::boolean(r);
                    break;
                }
                case Op::CastCk: case Op::DynCast: {
                    const Value v = S[sp - 1];
                    ClassInfo* c = prog_->classes[static_cast<std::size_t>(in.a)].get();
                    if (v.isNull()) break;
                    Obj* o = v.k == VK::Ptr && v.f != 2 && v.idx != Whole ? objOf(loadPtr(v), true) : v.o;
                    const bool ok = o && o->kind == OK::Inst && static_cast<InstObj*>(o)->cls->derivesFrom(c);
                    if (!ok) {
                        if (in.op == Op::DynCast) { S[sp - 1] = v.k == VK::Ptr ? Value::null() : Value::null(); break; }
                        if (in.b == 1) raise("bad_cast", "std::bad_cast");
                        fault(FaultKind::Cast, "impossibile convertire un oggetto di tipo " + std::string(o && o->kind == OK::Inst ? static_cast<InstObj*>(o)->cls->name : "?") + " in " + c->name);
                    }
                    break;
                }
                case Op::MkClosure: {
                    ClosureObj* c = heap_.make<ClosureObj>(heap_.resource());
                    root(c);
                    c->fn = in.a;
                    c->bound = in.c == 1;
                    const std::size_t n = in.b;
                    for (std::size_t i = 0; i < n; ++i) c->caps.push_back(S[sp - n + i]);
                    sp -= n;
                    FX_PUSH(Value::closure(c));
                    break;
                }
                case Op::LoadCap: {
                    const Value cv = S[base];
                    if (!cv.o || cv.o->kind != OK::Closure) fault(FaultKind::Generic, "cattura senza chiusura");
                    ClosureObj* c = static_cast<ClosureObj*>(cv.o);
                    if (static_cast<std::size_t>(in.a) >= c->caps.size()) fault(FaultKind::Generic, "cattura non valida");
                    FX_PUSH(c->caps[static_cast<std::size_t>(in.a)]);
                    break;
                }
                case Op::Throw: throwValue(S[--sp]);
                case Op::Rethrow: throwValue(S[base + static_cast<std::size_t>(in.a)], true);
                case Op::Fault: fault(static_cast<FaultKind>(in.a), prog_->strings[in.c].bytes);
                case Op::Halt: frames.resize(stopDepth); break;
                default: fatal("Syntax ERROR: istruzione non valida");
                }
                if (temps_.size() > tempMark && nativeDepth == 0) unroot(tempMark);
            }
            return;
        } catch (ProgramThrow& t) {
            unwind(t, stopDepth);
        } catch (std::bad_alloc&) {
            ProgramThrow t = memoryThrow();
            unwind(t, stopDepth);
        }
    }
}

// ================================================================ Machine
struct Machine::Impl {
    std::unique_ptr<Vm> vm;
    std::unique_ptr<coro::Coroutine> coro;
    // debug: valori espandibili mostrati (handle = indice + 1), validi fino al prossimo passo
    mutable std::vector<std::pair<Value, TypeP>> handles;
    bool finished{false};
    bool aborted{false};
    std::unique_ptr<Error> error;
    int exitCode{0};
    static void body(void* arg);
    static Error uncaught(Vm& vm, const Value& v);
};

Error Machine::Impl::uncaught(Vm& vm, const Value& v) {
    std::string msg;
    const Dialect d = vm.prog_->dialect;
    if (v.holdsObj() && v.o->kind == OK::Inst && static_cast<InstObj*>(v.o)->cls->isException) {
        InstObj* o = static_cast<InstObj*>(v.o);
        const std::string m = o->nfields > 0 && !o->fields[0].isNull() ? vm.utf8(o->fields[0]) : std::string();
        if (d == Dialect::Cpp) msg = "Uncaught exception " + std::string(o->cls->isNative ? "std::" : "") + o->cls->name + (m.empty() ? "" : ": " + m);
        else msg = o->cls->name + (m.empty() ? "" : ": " + m);
    } else if (v.holdsObj() && v.o->kind == OK::Inst) {
        msg = "Uncaught exception: oggetto " + static_cast<InstObj*>(v.o)->cls->name;
    } else {
        msg = "Uncaught exception: " + vm.format(v, d == Dialect::C ? Dialect::Cpp : d);
    }
    return Error(msg, 0, 0);
}

void Machine::Impl::body(void* arg) {
    Impl* im = static_cast<Impl*>(arg);
    Vm& vm = *im->vm;
    try {
        try {
            vm.runProgram();
        } catch (ProgramThrow& t) {
            // eccezione non catturata: posizione del lancio
            Error e = uncaught(vm, t.value);
            if (vm.lastThrowLine > 0) { e.line = vm.lastThrowLine; e.column = vm.lastThrowColumn; e.file = vm.lastThrowFile; }
            e.trace = vm.lastThrowTrace;
            throw e;
        }
    } catch (ProgramExit& e) {
        im->exitCode = e.code;
        vm.closeAllFiles(true);
    } catch (ProgramAbort&) {
        im->aborted = true;
        vm.closeAllFiles(false);
    } catch (Error& e) {
        im->error = std::make_unique<Error>(e);
        vm.closeAllFiles(false);
    } catch (std::bad_alloc&) {
        im->error = std::make_unique<Error>("Memory ERROR: memoria del programma esaurita", 0, 0);
        vm.closeAllFiles(false);
    } catch (std::exception& e) {
        im->error = std::make_unique<Error>(std::string("Math ERROR: ") + e.what(), 0, 0);
        vm.closeAllFiles(false);
    } catch (...) {
        im->error = std::make_unique<Error>("Math ERROR: errore interno dell'interprete", 0, 0);
        vm.closeAllFiles(false);
    }
    im->finished = true;
}

Machine::Machine(std::shared_ptr<const Program> program, Host& host, Limits limits) : impl_(std::make_unique<Impl>()) {
    if (!program) throw Error("Syntax ERROR: programma vuoto", 0, 0);
    impl_->vm = std::make_unique<Vm>(std::move(program), host, limits);
    impl_->coro = std::make_unique<coro::Coroutine>(FXAI_ON_DEVICE ? CoroStackDevice : CoroStackHost, &Impl::body, impl_.get());
    impl_->vm->coroutine = impl_->coro.get();
}
Machine::Machine(std::shared_ptr<const Program> program, Host& host, std::size_t memoryCells)
    : Machine(std::move(program), host, limitsForCells(memoryCells)) {}
Machine::~Machine() {
    if (impl_ && impl_->coro && impl_->coro->started() && !impl_->coro->finished()) {
        impl_->vm->abortRequested = true;
        impl_->vm->paused = false;
        for (int k = 0; k < 8 && !impl_->coro->finished(); ++k) impl_->coro->resume();
    }
}
bool Machine::step(step::Budget& budget) {
    if (state_ == State::Done || state_ == State::Failed) return true;
    if (state_ == State::Paused && impl_->vm->paused) return false;
    Vm& vm = *impl_->vm;
    vm.budget = &budget;
    vm.quota = 0;
    state_ = State::Running;
    impl_->coro->resume();
    vm.budget = nullptr;
    executed_ = vm.executed;
    if (impl_->finished) {
        if (impl_->error) { state_ = State::Failed; throw Error(*impl_->error); }
        if (impl_->aborted) { state_ = State::Failed; return true; }
        state_ = State::Done;
        exitCode_ = impl_->exitCode;
        return true;
    }
    if (vm.waitingInput) { state_ = State::WaitingInput; return false; }
    if (vm.paused) { state_ = State::Paused; return false; }
    return false;
}
void Machine::cancelled() {
    if (!impl_->coro->started() || impl_->coro->finished()) { state_ = State::Failed; return; }
    impl_->vm->abortRequested = true;
    impl_->vm->paused = false;
    for (int k = 0; k < 8 && !impl_->coro->finished(); ++k) impl_->coro->resume();
    state_ = State::Failed;
}
void Machine::runToEnd(std::size_t maxSteps) {
    const std::size_t start = executed_;
    while (state_ != State::Done && state_ != State::Failed) {
        const std::size_t used = executed_ - start;
        if (used >= maxSteps) {
            const int line = currentLine();
            std::string file = currentFile();
            cancelled();
            throw Error("Time Out: programma oltre " + std::to_string(maxSteps) + " istruzioni", line, 0, file);
        }
        step::Budget b(maxSteps - used, 0, nullptr);
        step(b);
        if (state_ == State::WaitingInput || state_ == State::Paused) return;
    }
}
int Machine::currentLine() const noexcept {
    const Vm& vm = *impl_->vm;
    if (vm.frames.empty()) return 0;
    const Frame& f = vm.frames.back();
    return static_cast<int>(posOf(*f.fn, f.pc > 0 ? f.pc - 1 : 0).line);
}
std::string Machine::currentFile() const {
    const Vm& vm = *impl_->vm;
    if (vm.frames.empty()) return {};
    const Frame& f = vm.frames.back();
    const SrcPos p = posOf(*f.fn, f.pc > 0 ? f.pc - 1 : 0);
    return p.file < vm.prog_->files.size() ? vm.prog_->files[p.file] : std::string();
}
void Machine::setDebug(bool on) { impl_->vm->debug = on; impl_->vm->firstStop = on; impl_->vm->stepMode = StepMode::Into; }
void Machine::setBreakpoints(const std::vector<std::pair<std::string, int>>& fileLines) {
    Vm& vm = *impl_->vm;
    vm.breakpoints.clear();
    for (const auto& fl : fileLines) {
        for (std::size_t i = 0; i < vm.prog_->files.size(); ++i)
            if (vm.prog_->files[i] == fl.first) vm.breakpoints.push_back({static_cast<std::uint16_t>(i), fl.second});
    }
}
void Machine::resume(StepMode mode) {
    impl_->handles.clear();
    Vm& vm = *impl_->vm;
    vm.stepMode = mode;
    vm.paused = false;
    if (state_ == State::Paused) state_ = State::Running;
}
std::vector<FrameInfo> Machine::callStack() const {
    std::vector<FrameInfo> out;
    const Vm& vm = *impl_->vm;
    for (std::size_t i = vm.frames.size(); i-- > 0;) {
        const Frame& f = vm.frames[i];
        if (f.fn->name == "<init>") continue;
        const SrcPos p = posOf(*f.fn, f.pc > 0 ? f.pc - 1 : 0);
        FrameInfo fi;
        fi.function = f.fn->name;
        fi.file = p.file < vm.prog_->files.size() ? vm.prog_->files[p.file] : std::string();
        fi.line = static_cast<int>(p.line);
        fi.column = p.column;
        out.push_back(fi);
    }
    return out;
}
std::string debugValue(Vm& vm, const Value& v, TypeP t, Dialect d, bool& expandable);
namespace {
VarInfo varInfo(Vm& vm, const std::string& name, const Value& v, TypeP t, Dialect d, std::vector<std::pair<Value, TypeP>>& handles) {
    VarInfo vi;
    vi.name = name;
    vi.type = t ? t->name() : std::string();
    bool expandable = false;
    try {
        vi.value = debugValue(vm, v, t, d, expandable);
    } catch (...) {
        vi.value = "?";
    }
    vi.expandable = expandable;
    if (expandable && handles.size() < 4096) {
        handles.emplace_back(v, t);
        vi.handle = static_cast<std::uint32_t>(handles.size());
    }
    return vi;
}
} // namespace
std::vector<VarInfo> Machine::variables(std::size_t frame) const {
    std::vector<VarInfo> out;
    Vm& vm = *impl_->vm;
    // la cornice `frame` contando dalla piu' interna (come callStack)
    std::size_t k = 0;
    const Frame* fr = nullptr;
    for (std::size_t i = vm.frames.size(); i-- > 0;) {
        if (vm.frames[i].fn->name == "<init>") continue;
        if (k++ == frame) { fr = &vm.frames[i]; break; }
    }
    if (fr) {
        const Dialect d = fr->fn->lang;
        const std::int32_t pc = fr->pc > 0 ? fr->pc - 1 : 0;
        for (const LocalInfo& l : fr->fn->locals) {
            if (l.name.empty() || l.name[0] == '#' || pc < l.from || pc >= l.to) continue;
            const std::size_t at = fr->base + static_cast<std::size_t>(l.slot);
            if (at >= vm.stack.size()) continue;
            Value v = vm.stack[at];
            if (l.boxed && v.holdsObj() && v.o->kind == OK::Box) v = static_cast<BoxObj*>(v.o)->v;
            bool dup = false;
            for (VarInfo& o : out) if (o.name == l.name) { o = varInfo(vm, l.name, v, l.type, d, impl_->handles); dup = true; }
            if (!dup) out.push_back(varInfo(vm, l.name, v, l.type, d, impl_->handles));
        }
    }
    const Program& p = *vm.prog_;
    if (vm.globals) {
        for (std::size_t i = 0; i < p.globalNames.size() && i < vm.globals->count; ++i) {
            const std::string& n = p.globalNames[i];
            if (n.empty() || n[0] == '#' || n.find('#') != std::string::npos) continue;
            out.push_back(varInfo(vm, "::" + n, vm.globals->get(static_cast<std::uint32_t>(i)), i < p.globalTypes.size() ? p.globalTypes[i] : nullptr,
                                  p.dialect, impl_->handles));
        }
    }
    return out;
}
std::vector<VarInfo> Machine::children(std::uint32_t handle) const {
    std::vector<VarInfo> out;
    if (handle == 0 || handle > impl_->handles.size()) return out;
    Vm& vm = *impl_->vm;
    const auto [v0, t0] = impl_->handles[handle - 1];
    const Dialect d = vm.prog_->dialect;
    Value v = v0;
    try {
        if (v.k == VK::Ptr && v.f != 2 && v.idx != Whole && v.o && v.o->kind != OK::Array) v = vm.loadPtr(v);
        if (!v.holdsObj() || v.o->dead()) return out;
        Obj* o = v.o;
        constexpr std::size_t Max = 200;
        switch (o->kind) {
        case OK::Inst: {
            InstObj* x = static_cast<InstObj*>(o);
            if (!x->cls) {
                const TypeP a = t0 && t0->k == TK::Native && t0->args.size() > 0 ? t0->args[0] : nullptr;
                const TypeP b = t0 && t0->k == TK::Native && t0->args.size() > 1 ? t0->args[1] : nullptr;
                out.push_back(varInfo(vm, d == Dialect::CSharp ? "Key" : "first", x->fields[0], a, d, impl_->handles));
                out.push_back(varInfo(vm, d == Dialect::CSharp ? "Value" : "second", x->fields[1], b, d, impl_->handles));
                break;
            }
            for (const ClassInfo* c = x->cls; c; c = c->base)
                for (const FieldInfo& f : c->fields) {
                    if (f.isStatic || f.slot < 0 || static_cast<std::uint32_t>(f.slot) >= x->nfields || f.name.empty() || f.name[0] == '#' || f.name[0] == '<') continue;
                    out.push_back(varInfo(vm, f.name, x->fields[f.slot], f.type, c->lang, impl_->handles));
                }
            break;
        }
        case OK::Array: {
            ArrayObj* a = static_cast<ArrayObj*>(o);
            if (a->ek == VK::Void) break;
            for (std::uint32_t i = 0; i < a->count && out.size() < Max; ++i) {
                std::string n = "[" + std::to_string(i) + "]";
                if (a->rank == 2 && a->cols) n = "[" + std::to_string(i / a->cols) + "," + std::to_string(i % a->cols) + "]";
                out.push_back(varInfo(vm, n, a->get(i), a->elem, d, impl_->handles));
            }
            break;
        }
        case OK::Vec: {
            VecObj* vv = static_cast<VecObj*>(o);
            const TypeP et = vv->type && !vv->type->args.empty() ? vv->type->args[0] : nullptr;
            for (std::size_t i = 0; i < vv->items.size() && out.size() < Max; ++i) out.push_back(varInfo(vm, "[" + std::to_string(i) + "]", vv->items[i], et, d, impl_->handles));
            break;
        }
        case OK::Map: {
            MapObj* m = static_cast<MapObj*>(o);
            const TypeP kt = m->type && !m->type->args.empty() ? m->type->args[0] : nullptr;
            const TypeP vt = m->type && m->type->args.size() > 1 ? m->type->args[1] : nullptr;
            std::vector<std::uint32_t> cells;
            if (m->mode == 0) cells.assign(m->order.begin(), m->order.end());
            else for (std::uint32_t c = 0; c < m->keys.size(); ++c) if (m->used[c]) cells.push_back(c);
            for (std::uint32_t c : cells) {
                if (out.size() >= Max) break;
                bool e = false;
                const std::string key = debugValue(vm, m->keys[c], kt, d, e);
                if (m->isSet) out.push_back(varInfo(vm, key, m->keys[c], kt, d, impl_->handles));
                else out.push_back(varInfo(vm, "[" + key + "]", m->vals[c], vt, d, impl_->handles));
            }
            break;
        }
        case OK::Box: out.push_back(varInfo(vm, "valore", static_cast<BoxObj*>(o)->v, t0, d, impl_->handles)); break;
        default: break;
        }
    } catch (...) {
    }
    return out;
}
std::size_t Machine::heapUsed() const noexcept { return impl_->vm->heap_.used(); }
std::size_t Machine::gcCycles() const noexcept { return impl_->vm->heap_.cycles(); }
std::size_t Machine::openFiles() const noexcept {
    std::size_t n = 0;
    for (FileObj* f : impl_->vm->openFiles) if (f && f->open && !f->console && !f->memory) ++n;
    return n;
}

} // namespace fx::prog
