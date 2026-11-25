// Coroutine con pila propria (v1.20.0): vedi coroutine.hpp. Il cambio di
// contesto e' quello della porta di MicroPython della v1.17.0, spostato qui
// per essere condiviso da Python e dall'interprete dei programmi.
#include "coroutine.hpp"
#include "board_config.hpp"
#include "mem_pools.hpp"
#include <cstring>
#include <new>

#if !FXAI_ON_DEVICE
#include <ucontext.h>
#if defined(__SANITIZE_ADDRESS__)
#define FXAI_CORO_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define FXAI_CORO_ASAN 1
#endif
#endif
#ifdef FXAI_CORO_ASAN
// Dal runtime di AddressSanitizer (le stesse dichiarazioni di sanitizer/common_interface_defs.h).
extern "C" void __sanitizer_start_switch_fiber(void** fakeStackSave, const void* bottom, std::size_t size);
extern "C" void __sanitizer_finish_switch_fiber(void* fakeStackSave, const void** bottomOld, std::size_t* sizeOld);
#endif
#endif

namespace fx::coro {
namespace {
Coroutine* g_current = nullptr;
constexpr std::uint8_t Pattern = 0xA5;
} // namespace

Coroutine* current() noexcept { return g_current; }

struct Coroutine::Impl {
#if FXAI_ON_DEVICE
    void* mainSp{nullptr};
    void* coroSp{nullptr};
#else
    ucontext_t mainCtx{}, coroCtx{};
#ifdef FXAI_CORO_ASAN
    void* fakeMain{nullptr};
    void* fakeCoro{nullptr};
    const void* mainBottom{nullptr};
    std::size_t mainSize{0};
#endif
#endif
};

} // namespace fx::coro

// ---------------------------------------------------------------- cambio di contesto
#if FXAI_ON_DEVICE
// r0 = dove salvare la pila attuale, r1 = la pila da riprendere (AAPCS).
extern "C" __attribute__((naked, noinline)) void fx_coro_switch(void** /*saveSp*/, void* /*nextSp*/) {
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

namespace fx::coro {

Coroutine::Coroutine(std::size_t stackBytes, Body body, void* arg) : size_(stackBytes), body_(body), arg_(arg) {
    impl_ = new Impl();
    stack_ = static_cast<std::uint8_t*>(mem::tryAlloc(mem::Pool::SRAM, size_, 16));
    if (!stack_) {
        delete impl_;
        impl_ = nullptr;
        throw std::bad_alloc();
    }
    std::memset(stack_, Pattern, size_);
}

Coroutine::~Coroutine() {
    mem::free(stack_);
    delete impl_;
    if (g_current == this) g_current = nullptr;
}

std::size_t Coroutine::stackUntouched() const noexcept {
    std::size_t n = 0;
    while (n < size_ && stack_[n] == Pattern) ++n;
    return n;
}

void Coroutine::entry() {  // primo ingresso (trampolino)
    Coroutine* self = g_current;
#if !FXAI_ON_DEVICE && defined(FXAI_CORO_ASAN)
    __sanitizer_finish_switch_fiber(nullptr, &self->impl_->mainBottom, &self->impl_->mainSize);
#endif
    self->body_(self->arg_);
    self->finished_ = true;
    self->yield();
    for (;;) {}                                   // mai ripresa dopo la fine
}

#if FXAI_ON_DEVICE
// Il trampolino della cornice iniziale: un puntatore a funzione C semplice.
extern "C" void fx_coro_trampoline();
#endif

void Coroutine::resume() {
    if (finished_ || inside_) return;
    Coroutine* outer = g_current;
    g_current = this;
    inside_ = true;
    if (!started_) {
        started_ = true;
#if FXAI_ON_DEVICE
        // Cornice iniziale letta da fx_coro_switch: MSPLIM, r3, [s16-s31], r4-r11, pc.
        auto* top = reinterpret_cast<std::uint32_t*>(stack_ + size_);
#if defined(__ARM_FP)
        constexpr int words = 2 + 16 + 9;
#else
        constexpr int words = 2 + 9;
#endif
        std::uint32_t* sp = top - words;
        std::memset(sp, 0, words * 4);
        sp[0] = reinterpret_cast<std::uint32_t>(stack_ + 64);              // MSPLIM della coroutine
        sp[words - 1] = reinterpret_cast<std::uint32_t>(&fx_coro_trampoline);  // pc (bit Thumb gia' nel puntatore)
        impl_->coroSp = sp;
#else
        getcontext(&impl_->coroCtx);
        impl_->coroCtx.uc_stack.ss_sp = stack_;
        impl_->coroCtx.uc_stack.ss_size = size_;
        impl_->coroCtx.uc_link = nullptr;
        makecontext(&impl_->coroCtx, reinterpret_cast<void (*)()>(&Coroutine::entry), 0);
#endif
    }
#if FXAI_ON_DEVICE
    fx_coro_switch(&impl_->mainSp, impl_->coroSp);
#else
#ifdef FXAI_CORO_ASAN
    __sanitizer_start_switch_fiber(&impl_->fakeMain, stack_, size_);
#endif
    swapcontext(&impl_->mainCtx, &impl_->coroCtx);
#ifdef FXAI_CORO_ASAN
    __sanitizer_finish_switch_fiber(impl_->fakeMain, nullptr, nullptr);
#endif
#endif
    inside_ = false;
    g_current = outer;
}

void Coroutine::yield() {
#if FXAI_ON_DEVICE
    fx_coro_switch(&impl_->coroSp, impl_->mainSp);
#else
#ifdef FXAI_CORO_ASAN
    __sanitizer_start_switch_fiber(finished_ ? nullptr : &impl_->fakeCoro, impl_->mainBottom, impl_->mainSize);
#endif
    swapcontext(&impl_->coroCtx, &impl_->mainCtx);
#ifdef FXAI_CORO_ASAN
    __sanitizer_finish_switch_fiber(impl_->fakeCoro, &impl_->mainBottom, &impl_->mainSize);
#endif
#endif
}

} // namespace fx::coro

#if FXAI_ON_DEVICE
extern "C" void fx_coro_trampoline() {
    // Primo ingresso nella coroutine attiva: esegue il corpo (che finisce con yield()).
    fx::coro::Coroutine::entry();
    for (;;) {}
}
#endif
