#include "window_manager.hpp"

namespace fx {

WindowManager::Entry* WindowManager::find(WindowId id) noexcept {
    for (auto& entry : entries_)
        if (entry.registered && entry.id == id) return &entry;
    return nullptr;
}

const WindowManager::Entry* WindowManager::find(WindowId id) const noexcept {
    for (const auto& entry : entries_)
        if (entry.registered && entry.id == id) return &entry;
    return nullptr;
}

bool WindowManager::registerWindow(WindowId id, void* context,
                                   EventHandler onEvent, DrawHandler onDraw,
                                   std::uint16_t focusCount) noexcept {
    if (find(id) || !onEvent || !onDraw) return false;
    for (auto& entry : entries_) {
        if (!entry.registered) {
            entry = {id, context, onEvent, onDraw, focusCount, 0, true};
            return true;
        }
    }
    return false;
}

bool WindowManager::show(WindowId id) noexcept {
    if (!find(id)) return false;
    active_ = id;
    activeValid_ = true;
    return true;
}

bool WindowManager::open(WindowId id) noexcept {
    if (!find(id) || (activeValid_ && stackSize_ == MaxStackDepth)) return false;
    if (activeValid_) stack_[stackSize_++] = active_;
    active_ = id;
    activeValid_ = true;
    return true;
}

bool WindowManager::close() noexcept {
    if (stackSize_ == 0) return false;
    active_ = stack_[--stackSize_];
    activeValid_ = true;
    return true;
}

void WindowManager::dispatch(const InputEvent& event) const {
    if (!activeValid_ || event.action == InputAction::None) return;
    const Entry* entry = find(active_);
    if (entry) entry->onEvent(entry->context, event);
}

void WindowManager::render() const {
    if (!activeValid_) return;
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

} // namespace fx
