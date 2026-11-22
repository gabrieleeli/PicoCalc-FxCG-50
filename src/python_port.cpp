// Nucleo della modalita' Python (v1.17.0): vedi python_port.hpp.
//
// La sessione esegue MicroPython in una coroutine con una pila propria:
//  * sul PC con ucontext (makecontext/swapcontext), annotato per
//    AddressSanitizer quando il build lo usa;
//  * sul dispositivo (Cortex-M33) con pc_ctx_switch, che salva r4-r11, lr,
//    s16-s31 e MSPLIM: il limite della pila segue la pila attiva (la guardia
//    della pila del core 0 resta valida fuori dalla coroutine).
// La coroutine resta viva per tutta la sessione (le variabili della SHELL
// restano tra una riga e l'altra); l'ultimo lavoro e' mp_deinit.
#include "python_port.hpp"
#include "board_config.hpp"
#include "mem_pools.hpp"
#include "mode_codec.hpp"
#include <algorithm>
#include <cstring>
#include <new>
#include <stdexcept>

#if !FXAI_ON_DEVICE
#include <ucontext.h>
#if defined(__SANITIZE_ADDRESS__)
#define FXAI_PY_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define FXAI_PY_ASAN 1
#endif
#endif
#ifdef FXAI_PY_ASAN
// Dal runtime di AddressSanitizer (le stesse dichiarazioni di sanitizer/common_interface_defs.h).
extern "C" void __sanitizer_start_switch_fiber(void** fakeStackSave, const void* bottom, std::size_t size);
extern "C" void __sanitizer_finish_switch_fiber(void* fakeStackSave, const void** bottomOld, std::size_t* sizeOld);
#endif
#endif

extern "C" {
void pc_py_glue_init(void* heap, std::size_t heapSize, void* stackTop, std::size_t stackLimit);
void pc_py_glue_deinit(void);
int pc_py_glue_exec(const char* src, std::size_t len, int single);
void pc_py_glue_interrupt(void);
int pc_py_glue_continue(const char* text);
}

namespace fx::py {
namespace {
enum class Job : std::uint8_t { None, Script, Line, Continue, Quit };
Session::Impl* g_impl = nullptr;
} // namespace

struct Session::Impl {
    Session& owner;
    Canvas* canvas;
    std::uint8_t* heap{nullptr};
    std::uint8_t* stack{nullptr};
    std::size_t heapBytes{0};
    Job job{Job::None};
    std::string source;
    int result{0};
    bool jobDone{false};
    bool finished{false};                         // la coroutine e' terminata (dopo Quit o un errore fatale)
    bool waiting{false};                          // input() aspetta una riga
    bool haveLine{false};
    bool breakInput{false};
    std::string line;
    step::Budget* budget{nullptr};
#if FXAI_ON_DEVICE
    void* mainSp{nullptr};
    void* coroSp{nullptr};
#else
    ucontext_t mainCtx{}, coroCtx{};
#ifdef FXAI_PY_ASAN
    void* fakeMain{nullptr};
    void* fakeCoro{nullptr};
    const void* mainBottom{nullptr};
    std::size_t mainSize{0};
#endif
#endif
    explicit Impl(Session& s, Canvas* c) : owner(s), canvas(c) {}
    void toCoroutine();
    void toMain();
};

} // namespace fx::py

// ---------------------------------------------------------------- cambio di contesto
#if FXAI_ON_DEVICE
// r0 = dove salvare la pila attuale, r1 = la pila da riprendere (AAPCS).
extern "C" __attribute__((naked, noinline)) void pc_ctx_switch(void** /*saveSp*/, void* /*nextSp*/) {
    __asm volatile(
        "push {r4-r11, lr}\n"
#if defined(__ARM_FP)
        "vpush {s16-s31}\n"
#endif
        "mrs r2, msplim\n"
        "push {r2, r3}\n"
        "mov r2, sp\n"
        "str r2, [r0]\n"
        "movs r2, #0\n"
        "msr msplim, r2\n"
        "mov sp, r1\n"
        "pop {r2, r3}\n"
        "msr msplim, r2\n"
#if defined(__ARM_FP)
        "vpop {s16-s31}\n"
#endif
        "pop {r4-r11, pc}\n");
}
#endif

namespace fx::py {
namespace {
void coroutineBody() {
    Session::Impl* im = g_impl;
    pc_py_glue_init(im->heap, im->heapBytes, im->stack + StackBytes, StackBytes - StackMarginBytes);
    for (;;) {
        if (im->job == Job::Quit) break;
        if (im->job == Job::Continue) {
            // Il lexer di MicroPython alloca nell'heap GC: si lavora sulla pila della coroutine.
            im->result = pc_py_glue_continue(im->source.c_str());
            im->job = Job::None;
            im->jobDone = true;
        } else if (im->job == Job::Script || im->job == Job::Line) {
            im->result = pc_py_glue_exec(im->source.c_str(), im->source.size(), im->job == Job::Line ? 1 : 0);
            im->job = Job::None;
            im->jobDone = true;
        }
        im->toMain();
    }
    pc_py_glue_deinit();
    im->finished = true;
    im->toMain();
}
#if FXAI_ON_DEVICE
extern "C" void pc_coro_entry() {
    coroutineBody();
    for (;;) {}
}
#else
void coroEntry() {
#ifdef FXAI_PY_ASAN
    __sanitizer_finish_switch_fiber(nullptr, &g_impl->mainBottom, &g_impl->mainSize);
#endif
    coroutineBody();
}
#endif
} // namespace

void Session::Impl::toCoroutine() {
    if (finished) return;
    g_impl = this;
#if FXAI_ON_DEVICE
    pc_ctx_switch(&mainSp, coroSp);
#else
#ifdef FXAI_PY_ASAN
    __sanitizer_start_switch_fiber(&fakeMain, stack, StackBytes);
#endif
    swapcontext(&mainCtx, &coroCtx);
#ifdef FXAI_PY_ASAN
    __sanitizer_finish_switch_fiber(fakeMain, nullptr, nullptr);
#endif
#endif
}
void Session::Impl::toMain() {
#if FXAI_ON_DEVICE
    pc_ctx_switch(&coroSp, mainSp);
#else
#ifdef FXAI_PY_ASAN
    __sanitizer_start_switch_fiber(finished ? nullptr : &fakeCoro, mainBottom, mainSize);
#endif
    swapcontext(&coroCtx, &mainCtx);
#ifdef FXAI_PY_ASAN
    __sanitizer_finish_switch_fiber(fakeCoro, &mainBottom, &mainSize);
#endif
#endif
}

// ---------------------------------------------------------------- sessione
namespace { bool g_active = false; }
bool Session::active() noexcept { return g_active; }

Session::Session(Clock clock, Canvas* canvas) : impl_(std::make_unique<Impl>(*this, canvas)), clock_(std::move(clock)) {
    if (g_active) throw std::logic_error("Python: una sola sessione alla volta");
    lines_.assign(1, std::string());
    // Heap separato: nella PSRAM se c'e', altrimenti un heap piccolo in SRAM.
    const bool psram = mem::available(mem::Pool::PSRAM_BOARD);
    heapBytes_ = psram ? PsramHeapBytes : SramHeapBytes;
    impl_->heapBytes = heapBytes_;
    impl_->heap = static_cast<std::uint8_t*>(mem::tryAlloc(psram ? mem::Pool::PSRAM_BOARD : mem::Pool::SRAM, heapBytes_, 16));
    impl_->stack = static_cast<std::uint8_t*>(mem::tryAlloc(mem::Pool::SRAM, StackBytes, 16));
    if (!impl_->heap || !impl_->stack) {
        mem::free(impl_->heap);
        mem::free(impl_->stack);
        throw std::bad_alloc();
    }
#if FXAI_ON_DEVICE
    // Cornice iniziale letta da pc_ctx_switch: MSPLIM, r3, [s16-s31], r4-r11, pc.
    auto* top = reinterpret_cast<std::uint32_t*>(impl_->stack + StackBytes);
#if defined(__ARM_FP)
    constexpr int words = 2 + 16 + 9;
#else
    constexpr int words = 2 + 9;
#endif
    std::uint32_t* sp = top - words;
    std::memset(sp, 0, words * 4);
    sp[0] = reinterpret_cast<std::uint32_t>(impl_->stack + 64);          // MSPLIM della coroutine
    sp[words - 1] = reinterpret_cast<std::uint32_t>(&pc_coro_entry);      // pc (bit Thumb gia' nel puntatore)
    impl_->coroSp = sp;
#else
    getcontext(&impl_->coroCtx);
    impl_->coroCtx.uc_stack.ss_sp = impl_->stack;
    impl_->coroCtx.uc_stack.ss_size = StackBytes;
    impl_->coroCtx.uc_link = nullptr;
    makecontext(&impl_->coroCtx, reinterpret_cast<void (*)()>(&coroEntry), 0);
#endif
    g_active = true;
    impl_->toCoroutine();                         // mp_init sulla pila della coroutine
}

Session::~Session() {
    Impl& im = *impl_;
    // Uno script ancora in corso: KeyboardInterrupt, poi la fine della sessione.
    for (int k = 0; k < 64 && !im.finished && (state_ != State::Idle); ++k) {
        interrupt();
        step::Budget b;
        im.budget = &b;
        im.toCoroutine();
        im.budget = nullptr;
        if (im.jobDone) state_ = State::Idle;
    }
    if (!im.finished) {
        im.job = Job::Quit;
        im.toCoroutine();
    }
    mem::free(im.heap);
    mem::free(im.stack);
    g_active = false;
    if (g_impl == impl_.get()) g_impl = nullptr;
}

void Session::runScript(const std::string& source) {
    impl_->source = source;
    impl_->job = Job::Script;
    impl_->jobDone = false;
    state_ = State::Running;
    outcome_ = Outcome::Ok;
}
void Session::runLine(const std::string& source) {
    impl_->source = source;
    impl_->job = Job::Line;
    impl_->jobDone = false;
    state_ = State::Running;
    outcome_ = Outcome::Ok;
}
bool Session::needsMoreInput(const std::string& text) const {
    if (state_ != State::Idle || impl_->finished) return false;
    Impl& im = *impl_;
    im.source = text;
    im.job = Job::Continue;
    im.jobDone = false;
    im.toCoroutine();
    return im.jobDone && im.result != 0;
}
void Session::provideInput(const std::string& line) {
    impl_->line = line;
    impl_->haveLine = true;
    if (state_ == State::WaitingInput) state_ = State::Running;
}
void Session::interrupt() noexcept {
    if (state_ == State::WaitingInput) { impl_->breakInput = true; state_ = State::Running; return; }
    if (state_ == State::Running) pc_py_glue_interrupt();
}
bool Session::step(step::Budget& budget) {
    if (state_ == State::Idle) return true;
    if (state_ == State::WaitingInput) return false;
    Impl& im = *impl_;
    im.budget = &budget;
    im.toCoroutine();
    im.budget = nullptr;
    if (im.finished) { state_ = State::Idle; outcome_ = Outcome::Exception; return true; }
    if (im.waiting) { state_ = State::WaitingInput; return false; }
    if (im.jobDone) {
        state_ = State::Idle;
        outcome_ = im.result == 0 ? Outcome::Ok : im.result == 2 ? Outcome::Interrupted : im.result == 3 ? Outcome::Exit : Outcome::Exception;
        return true;
    }
    return false;
}
void Session::write(const char* s, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const char c = s[i];
        if (c == '\r') continue;
        if (c == '\n' || lines_.back().size() >= 200) {
            lines_.emplace_back();
            if (lines_.size() > ConsoleLines) lines_.erase(lines_.begin());
            if (c == '\n') continue;
        }
        lines_.back() += c;
    }
}

// ---------------------------------------------------------------- stato nel workspace
void reset(State& s) { s = State{}; }
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1);
    w.str(s.lastScript);
    w.u8(static_cast<std::uint8_t>(std::min<std::size_t>(s.history.size(), 20)));
    for (std::size_t i = 0; i < s.history.size() && i < 20; ++i) w.str(s.history[i]);
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    t.lastScript = r.str(64);
    const std::size_t n = r.u8();
    if (n > 20) return false;
    for (std::size_t i = 0; i < n; ++i) t.history.push_back(r.str(255));
    if (!r.end()) return false;
    s = std::move(t);
    return true;
}
} // namespace fx::py

// ---------------------------------------------------------------- funzioni chiamate dal C
using fx::py::g_impl;
extern "C" {
void pc_py_poll(void) {
    if (!g_impl) return;
    // Fetta di tempo finita (o budget di unita'): torna al ciclo del firmware.
    if (g_impl->budget && !g_impl->budget->consume(32)) g_impl->toMain();
}
// Nessuna eccezione C++ attraversa il codice C di MicroPython.
void pc_py_write(const char* s, std::size_t n) {
    if (!g_impl) return;
    try { g_impl->owner.write(s, n); } catch (...) {}
}
void pc_py_fatal(void) {
    if (!g_impl) return;
    g_impl->finished = true;
    pc_py_write("\nFatal: MicroPython\n", 20);
    for (;;) g_impl->toMain();
}
int pc_py_read_line(char* buf, int max) {
    if (!g_impl) return -1;
    while (!g_impl->haveLine && !g_impl->breakInput) {
        g_impl->waiting = true;
        g_impl->toMain();
    }
    g_impl->waiting = false;
    if (g_impl->breakInput) { g_impl->breakInput = false; return -2; }
    g_impl->haveLine = false;
    const int n = static_cast<int>(std::min<std::size_t>(g_impl->line.size(), static_cast<std::size_t>(max - 1)));
    std::memcpy(buf, g_impl->line.data(), static_cast<std::size_t>(n));
    buf[n] = 0;
    pc_py_write(g_impl->line.data(), g_impl->line.size());
    pc_py_write("\n", 1);
    return n;
}
void pc_py_set_pixel(int x, int y, int r, int g, int b) {
    if (g_impl && g_impl->canvas) try { g_impl->canvas->setPixel(x, y, static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b)); } catch (...) {}
}
int pc_py_get_pixel(int x, int y, int* r, int* g, int* b) {
    std::uint8_t rr = 0, gg = 0, bb = 0;
    try { if (!g_impl || !g_impl->canvas || !g_impl->canvas->getPixel(x, y, rr, gg, bb)) return 0; } catch (...) { return 0; }
    *r = rr; *g = gg; *b = bb;
    return 1;
}
void pc_py_draw_string(int x, int y, const char* s, int r, int g, int b, int large) {
    if (g_impl && g_impl->canvas) try { g_impl->canvas->drawString(x, y, s, static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), large != 0); } catch (...) {}
}
void pc_py_clear_screen(void) { if (g_impl && g_impl->canvas) try { g_impl->canvas->clear(); } catch (...) {} }
void pc_py_show_screen(void) {
    if (!g_impl || !g_impl->canvas) return;
    try { g_impl->canvas->show(); } catch (...) {}
    g_impl->toMain();                            // il firmware ridisegna lo schermo
}
}
