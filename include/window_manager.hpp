#pragma once

#include "keyboard_input.hpp"
#include <cstddef>
#include <cstdint>

namespace fx {

enum class WindowId : std::uint8_t {
    Home = 0,
    RunMatrix,
    Graph,
    Cheatsheet,
    Unavailable
};

// Allocation-free screen/window coordinator. Application-specific behavior stays
// in callbacks; the manager owns registration, active-window routing and focus.
class WindowManager {
public:
    using EventHandler = void (*)(void* context, const InputEvent& event);
    using DrawHandler = void (*)(void* context);

    static constexpr std::size_t MaxWindows = 8;
    static constexpr std::size_t MaxStackDepth = 8;

    bool registerWindow(WindowId id, void* context, EventHandler onEvent,
                        DrawHandler onDraw, std::uint16_t focusCount = 0) noexcept;
    bool show(WindowId id) noexcept;
    bool open(WindowId id) noexcept;
    bool close() noexcept;
    void dispatch(const InputEvent& event) const;
    void render() const;

    bool setFocus(std::uint16_t index) noexcept;
    bool moveFocus(int delta) noexcept;
    std::uint16_t focus() const noexcept;
    std::uint16_t focusCount() const noexcept;
    WindowId active() const noexcept { return active_; }
    bool hasWindow(WindowId id) const noexcept;
    std::size_t stackDepth() const noexcept { return stackSize_; }

private:
    struct Entry {
        WindowId id{};
        void* context{};
        EventHandler onEvent{};
        DrawHandler onDraw{};
        std::uint16_t focusCount{};
        std::uint16_t focused{};
        bool registered{};
    };

    Entry* find(WindowId id) noexcept;
    const Entry* find(WindowId id) const noexcept;
    Entry entries_[MaxWindows]{};
    WindowId stack_[MaxStackDepth]{};
    std::size_t stackSize_{};
    WindowId active_{WindowId::Home};
    bool activeValid_{};
};

} // namespace fx
