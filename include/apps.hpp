#pragma once
// Application classes of the PicoCalc FX firmware. Each mode owns its screen
// state, input handling and drawing; shared data lives in fx::Workspace.
#include "app.hpp"
#include "natural_editor.hpp"
#include "line_editor.hpp"
#include <array>
#include <string>
#include <vector>

namespace fx::app {

void drawAppIcon(gfx::Canvas& c, AppId id, int x, int y, int w, int h, bool available);

class LauncherApp final : public App {
public:
    AppId id() const override { return AppId::MainMenu; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    std::size_t selection() const noexcept { return selected_; }
    void select(std::size_t index) noexcept { selected_ = index < AppCount ? index : 0; }
    bool available(std::size_t index) const noexcept;
    void setAvailability(std::size_t index, bool on) noexcept { if (index < AppCount) available_[index] = on; }
private:
    std::size_t selected_{0};
    int firstRow_{0};
    std::array<bool, AppCount> available_{};
};

class UnavailableApp final : public App {
public:
    explicit UnavailableApp(AppManager& m) : manager_(m) {}
    AppId id() const override { return manager_.requestedUnavailable(); }
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
private:
    AppManager& manager_;
};

// Generic modal popup list used by OPTN / MATH / ZOOM / G-SOLVE menus.
struct PopupMenu {
    const char* title{};
    std::vector<std::string> items;
    int selected{0}, first{0};
    bool open{false};
    void show(const char* t, std::vector<std::string> list) { title = t; items = std::move(list); selected = 0; first = 0; open = true; }
    void close() { open = false; }
    // Returns the chosen index (>=0), -1 when still open, -2 when cancelled.
    int key(const KeyEvent& ev);
    void draw(gfx::Canvas& c) const;
};

class RunMatrixApp final : public App {
public:
    AppId id() const override { return AppId::RunMatrix; }
    void enter(Context& ctx) override;
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
    NaturalEditor& editor() noexcept { return editor_; }
    int browsing() const noexcept { return browse_; }
private:
    void evaluate(Context& ctx);
    void insertCatalogItem(Context& ctx, const std::string& item);
    NaturalEditor editor_;
    int browse_{-1};                // selected history entry, -1 = editing
    int scrollX_{0};
    PopupMenu menu_;
    int menuKind_{0};               // 1 MATH, 2 OPTN categories, 3 OPTN items, 4 VARS
    int optnCategory_{0};
};

class SymbolsApp final : public App {
public:
    AppId id() const override { return AppId::Symbols; }
    void key(Context& ctx, const KeyEvent& ev) override;
    void draw(Context& ctx, gfx::Canvas& c) override;
private:
    unsigned page_{0}, selected_{0};
};

} // namespace fx::app
