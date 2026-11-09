#pragma once

#include "keyboard_input.hpp"
#include "limits.hpp"
#include <cstddef>
#include <cstdint>

namespace fx {

enum class WindowId : std::uint8_t {
    Home = 0,
    RunMatrix,
    Graph,
    Cheatsheet,
    Unavailable,
    GraphFunctions,
    Graph3D,
    System,
    Memory,
    Diagnostics
};

// Screen/window coordinator. Application-specific behavior stays in callbacks;
// the manager owns registration, active-window routing, focus and the
// navigation stack.
//
// v1.9.0: registry and navigation stack are dynamic (grown through the memory
// pools, never above the per-board caps of fx::limits) instead of fixed arrays
// of 8. All operations stay noexcept: an allocation failure simply makes
// registerWindow()/open() return false. On boards with PSRAM the manager can
// keep full-screen snapshots of covered windows in PSRAM and restore them on
// close() when the window's state stamp is unchanged (no redraw needed).
class WindowManager {
public:
    using EventHandler = void (*)(void* context, const InputEvent& event);
    using DrawHandler = void (*)(void* context);
    // Returns a value that changes whenever the window would draw differently.
    using StampHandler = std::uint32_t (*)(void* context);

    // Per-board caps (compile-time); the runtime capacity grows up to them.
    static constexpr std::size_t MaxWindows = limits::kProfile.maxWindows;
    static constexpr std::size_t MaxStackDepth = limits::kProfile.maxNavigationDepth;

    struct SnapshotHooks {
        std::uint16_t* framebuffer{};
        std::size_t bytes{};
        void (*invalidateAndPresent)(){}; // marks the frame dirty and flushes it
    };

    WindowManager() noexcept = default;
    ~WindowManager();
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    bool registerWindow(WindowId id, void* context, EventHandler onEvent,
                        DrawHandler onDraw, std::uint16_t focusCount = 0,
                        StampHandler onStamp = nullptr) noexcept;
    bool show(WindowId id) noexcept;
    bool open(WindowId id) noexcept;
    bool close() noexcept;
    bool closeAll() noexcept;      // back to the root window of the stack
    void dispatch(const InputEvent& event) const;
    void render();

    bool setFocus(std::uint16_t index) noexcept;
    bool moveFocus(int delta) noexcept;
    std::uint16_t focus() const noexcept;
    std::uint16_t focusCount() const noexcept;
    WindowId active() const noexcept { return active_; }
    bool hasWindow(WindowId id) const noexcept;
    std::size_t stackDepth() const noexcept { return stackSize_; }
    // True when `id` is the window directly below the active one (going "back").
    bool isPrevious(WindowId id) const noexcept { return stackSize_ && stack_[stackSize_ - 1].id == id; }
    std::size_t windowCount() const noexcept { return entryCount_; }

    void setSnapshotHooks(const SnapshotHooks& hooks) noexcept;
    std::size_t snapshotsStored() const noexcept;
    std::size_t snapshotRestores() const noexcept { return restores_; }

private:
    struct Entry {
        WindowId id{};
        void* context{};
        EventHandler onEvent{};
        DrawHandler onDraw{};
        StampHandler onStamp{};
        std::uint16_t focusCount{};
        std::uint16_t focused{};
    };
    struct StackItem {
        WindowId id{};
        std::uint16_t* snapshot{};
        std::uint32_t stamp{};
    };

    Entry* find(WindowId id) noexcept;
    const Entry* find(WindowId id) const noexcept;
    bool reserveEntries(std::size_t count) noexcept;
    bool reserveStack(std::size_t count) noexcept;
    void captureSnapshot(StackItem& item) noexcept;
    bool restoreSnapshot(StackItem& item) noexcept;
    void dropSnapshot(StackItem& item) noexcept;

    Entry* entries_{};
    std::size_t entryCount_{};
    std::size_t entryCapacity_{};
    StackItem* stack_{};
    std::size_t stackSize_{};
    std::size_t stackCapacity_{};
    WindowId active_{WindowId::Home};
    bool activeValid_{};
    bool skipNextDraw_{};
    SnapshotHooks hooks_{};
    std::size_t restores_{};
};

} // namespace fx
