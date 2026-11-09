// Global operator new/delete routed through the memory pools.
//
// Every C++ allocation (std::vector, std::string, std::make_shared, ...) goes
// through mem::tryAlloc() with mem::newPolicyFor(size):
//   * size >= FXAI_PSRAM_NEW_THRESHOLD -> PREFER_PSRAM (PSRAM_BOARD, then SRAM)
//   * smaller requests                  -> PREFER_SRAM (SRAM with reserve, then PSRAM)
//   * an active mem::PlacementScope on the current core overrides both rules.
// On the Pico 2 W the PSRAM pool does not exist, so every request lands in the
// SRAM heap exactly as with the default allocator.
//
// Failure follows the standard: the new_handler is invoked if installed,
// otherwise std::bad_alloc is thrown (nothrow variants return nullptr). The
// application turns std::bad_alloc into the "Memoria insufficiente" message.

#include "mem_pools.hpp"

#include <cstddef>
#include <new>

namespace {
void* allocateOrNull(std::size_t size, std::size_t align) noexcept {
    return mem::tryAlloc(mem::newPolicyFor(size), size, align);
}

void* allocateOrThrow(std::size_t size, std::size_t align) {
    for (;;) {
        if (void* p = allocateOrNull(size, align)) return p;
        std::new_handler handler = std::get_new_handler();
        if (!handler) throw std::bad_alloc();
        handler();
    }
}
} // namespace

void* operator new(std::size_t size) { return allocateOrThrow(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__); }
void* operator new[](std::size_t size) { return allocateOrThrow(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__); }
void* operator new(std::size_t size, std::align_val_t align) {
    return allocateOrThrow(size, static_cast<std::size_t>(align));
}
void* operator new[](std::size_t size, std::align_val_t align) {
    return allocateOrThrow(size, static_cast<std::size_t>(align));
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return allocateOrNull(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return allocateOrNull(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
}
void* operator new(std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    return allocateOrNull(size, static_cast<std::size_t>(align));
}
void* operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    return allocateOrNull(size, static_cast<std::size_t>(align));
}

void operator delete(void* p) noexcept { mem::free(p); }
void operator delete[](void* p) noexcept { mem::free(p); }
void operator delete(void* p, std::size_t) noexcept { mem::free(p); }
void operator delete[](void* p, std::size_t) noexcept { mem::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { mem::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { mem::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { mem::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { mem::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { mem::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { mem::free(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { mem::free(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { mem::free(p); }
