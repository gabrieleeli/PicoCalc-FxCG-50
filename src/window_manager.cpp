#include "window_manager.hpp"

#include "mem_pools.hpp"

#include <cstring>
#include <new>

namespace fx {

WindowManager::~WindowManager() {
    for (std::size_t i = 0; i < stackSize_; ++i) dropSnapshot(stack_[i]);
    mem::free(entries_);
    mem::free(stack_);
}

WindowManager::Entry* WindowManager::find(WindowId id) noexcept {
    for (std::size_t i = 0; i < entryCount_; ++i)
        if (entries_[i].id == id) return &entries_[i];
    return nullptr;
}

const WindowManager::Entry* WindowManager::find(WindowId id) const noexcept {
    for (std::size_t i = 0; i < entryCount_; ++i)
        if (entries_[i].id == id) return &entries_[i];
    return nullptr;
}

bool WindowManager::reserveEntries(std::size_t count) noexcept {
    if (count <= entryCapacity_) return true;
    if (count > MaxWindows) return false;
    std::size_t capacity = entryCapacity_ ? entryCapacity_ * 2 : 8;
    if (capacity < count) capacity = count;
    if (capacity > MaxWindows) capacity = MaxWindows;
    auto* grown = static_cast<Entry*>(mem::tryAlloc(mem::Policy::PREFER_SRAM, capacity * sizeof(Entry), alignof(Entry)));
    if (!grown) return false; // registry unchanged
    for (std::size_t i = 0; i < entryCount_; ++i) new (&grown[i]) Entry(entries_[i]);
    mem::free(entries_);
    entries_ = grown;
    entryCapacity_ = capacity;
    return true;
}

bool WindowManager::reserveStack(std::size_t count) noexcept {
    if (count <= stackCapacity_) return true;
    if (count > MaxStackDepth) return false;
    std::size_t capacity = stackCapacity_ ? stackCapacity_ * 2 : 8;
    if (capacity < count) capacity = count;
    if (capacity > MaxStackDepth) capacity = MaxStackDepth;
    auto* grown = static_cast<StackItem*>(mem::tryAlloc(mem::Policy::PREFER_SRAM, capacity * sizeof(StackItem), alignof(StackItem)));
    if (!grown) return false;
    for (std::size_t i = 0; i < stackSize_; ++i) new (&grown[i]) StackItem(stack_[i]);
    mem::free(stack_);
    stack_ = grown;
    stackCapacity_ = capacity;
    return true;
}

bool WindowManager::registerWindow(WindowId id, void* context,
                                   EventHandler onEvent, DrawHandler onDraw,
                                   std::uint16_t focusCount, StampHandler onStamp) noexcept {
    if (find(id) || !onEvent || !onDraw) return false;
    if (!reserveEntries(entryCount_ + 1)) return false;
    new (&entries_[entryCount_]) Entry{id, context, onEvent, onDraw, onStamp, focusCount, 0};
    ++entryCount_;
    return true;
}

bool WindowManager::show(WindowId id) noexcept {
    if (!find(id)) return false;
    active_ = id;
    activeValid_ = true;
    skipNextDraw_ = false;
    return true;
}

bool WindowManager::open(WindowId id) noexcept {
    if (!find(id)) return false;
    if (activeValid_) {
        if (!reserveStack(stackSize_ + 1)) return false;
        StackItem item{active_, nullptr, 0};
        captureSnapshot(item);
        stack_[stackSize_++] = item;
    }
    active_ = id;
    activeValid_ = true;
    skipNextDraw_ = false;
    return true;
}

bool WindowManager::close() noexcept {
    if (stackSize_ == 0) return false;
    StackItem item = stack_[--stackSize_];
    active_ = item.id;
    activeValid_ = true;
    skipNextDraw_ = restoreSnapshot(item);
    dropSnapshot(item);
    return true;
}

bool WindowManager::closeAll() noexcept {
    if (stackSize_ == 0) return false;
    while (stackSize_ > 1) dropSnapshot(stack_[--stackSize_]);
    return close();
}

void WindowManager::dispatch(const InputEvent& event) const {
    if (!activeValid_ || event.action == InputAction::None) return;
    const Entry* entry = find(active_);
    if (entry) entry->onEvent(entry->context, event);
}

void WindowManager::render() {
    if (!activeValid_) return;
    if (skipNextDraw_) { // snapshot already restored and flushed by close()
        skipNextDraw_ = false;
        return;
    }
    const Entry* entry = find(active_);
    if (entry) entry->onDraw(entry->context);
}

bool WindowManager::setFocus(std::uint16_t index) noexcept {
    Entry* entry = activeValid_ ? find(active_) : nullptr;
    if (!entry || index >= entry->focusCount) return false;
    entry->focused = index;
    return true;
}

bool WindowManager::moveFocus(int delta) noexcept {
    Entry* entry = activeValid_ ? find(active_) : nullptr;
    if (!entry || entry->focusCount == 0 || delta == 0) return false;
    const int count = static_cast<int>(entry->focusCount);
    int next = (static_cast<int>(entry->focused) + (delta % count)) % count;
    if (next < 0) next += count;
    entry->focused = static_cast<std::uint16_t>(next);
    return true;
}

std::uint16_t WindowManager::focus() const noexcept {
    const Entry* entry = activeValid_ ? find(active_) : nullptr;
    return entry ? entry->focused : 0;
}

std::uint16_t WindowManager::focusCount() const noexcept {
    const Entry* entry = activeValid_ ? find(active_) : nullptr;
    return entry ? entry->focusCount : 0;
}

bool WindowManager::hasWindow(WindowId id) const noexcept { return find(id) != nullptr; }

void WindowManager::setSnapshotHooks(const SnapshotHooks& hooks) noexcept { hooks_ = hooks; }

std::size_t WindowManager::snapshotsStored() const noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < stackSize_; ++i) n += stack_[i].snapshot != nullptr;
    return n;
}

void WindowManager::captureSnapshot(StackItem& item) noexcept {
    const Entry* entry = find(item.id);
    if (!entry || !entry->onStamp || !hooks_.framebuffer || !hooks_.bytes) return;
    if (snapshotsStored() >= limits::windowSnapshots(hooks_.bytes)) return;
    // Snapshots are a PSRAM-only luxury: never take SRAM for them.
    auto* copy = static_cast<std::uint16_t*>(mem::tryAlloc(mem::Policy::PSRAM_ONLY, hooks_.bytes, 32));
    if (!copy) return;
    std::memcpy(copy, hooks_.framebuffer, hooks_.bytes);
    item.snapshot = copy;
    item.stamp = entry->onStamp(entry->context);
}

bool WindowManager::restoreSnapshot(StackItem& item) noexcept {
    if (!item.snapshot || !hooks_.framebuffer || !hooks_.invalidateAndPresent) return false;
    const Entry* entry = find(item.id);
    if (!entry || !entry->onStamp || entry->onStamp(entry->context) != item.stamp) return false;
    std::memcpy(hooks_.framebuffer, item.snapshot, hooks_.bytes);
    hooks_.invalidateAndPresent();
    ++restores_;
    return true;
}

void WindowManager::dropSnapshot(StackItem& item) noexcept {
    mem::free(item.snapshot);
    item.snapshot = nullptr;
}

} // namespace fx
