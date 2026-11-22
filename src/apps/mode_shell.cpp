// Guscio comune delle modalita' (v1.15.0, F4): vedi mode_shell.hpp. Il disegno
// di conferme, richieste e moduli e' quello di TABLE dalla v1.8.0, cosi' le
// modalita' portate sul guscio restano identiche al pixel.
#include "mode_shell.hpp"
#include "expression.hpp"
#include "math_renderer.hpp"
#include "workspace.hpp"
#include <algorithm>

namespace fx::app {
using namespace ui::theme;

// ---------------------------------------------------------------- ScreenStack
void ScreenStack::push(int screen) noexcept {
    if (depth_ < MaxDepth) items_[depth_++] = screen;
    else items_[MaxDepth - 1] = screen;
}
bool ScreenStack::pop() noexcept {
    if (depth_ <= 1) return false;
    --depth_;
    return true;
}

// ---------------------------------------------------------------- FKeyBar
void FKeyBar::set(const FKeyMenu* menu) noexcept {
    stack_.fill(nullptr);
    stack_[0] = menu;
    depth_ = menu ? 1 : 0;
    page_ = 0;
}
const FKeyPage* FKeyBar::current(bool shift) const noexcept {
    if (depth_ == 0) return nullptr;
    const FKeyMenu* m = stack_[depth_ - 1];
    if (!m) return nullptr;
    if (shift && m->shiftPage) return m->shiftPage;
    if (!m->pages || m->count == 0) return nullptr;
    return &m->pages[page_ < m->count ? page_ : 0];
}
void FKeyBar::labels(bool shift, const char* out[6]) const noexcept {
    const FKeyPage* p = current(shift);
    for (int i = 0; i < 6; ++i) out[i] = p ? p->keys[i].label : nullptr;
    const FKeyMenu* m = depth_ ? stack_[depth_ - 1] : nullptr;
    if (!shift && m && m->count > 1 && !out[5]) out[5] = "\xE2\x96\xB7";      // ▷: pagina seguente
}
void FKeyBar::draw(gfx::Canvas& c, bool shift) const {
    const char* l[6];
    labels(shift, l);
    ui::softKeys(c, l);
}
void FKeyBar::back() noexcept {
    if (depth_ > 1) { stack_[--depth_] = nullptr; page_ = 0; }
}
int FKeyBar::key(const KeyEvent& ev, bool shift) {
    if (depth_ == 0) return None;
    if (ev.is(Key::Exit) && inSubmenu()) { back(); return Handled; }
    int index = -1;
    switch (ev.key) {
    case Key::F1: index = 0; break;
    case Key::F2: index = 1; break;
    case Key::F3: index = 2; break;
    case Key::F4: index = 3; break;
    case Key::F5: index = 4; break;
    case Key::F6: index = 5; break;
    default: return None;
    }
    const FKeyMenu* m = stack_[depth_ - 1];
    const FKeyPage* p = current(shift);
    if (!p) return None;
    const FKey& k = p->keys[index];
    if (index == 5 && !k.label && !shift && m->count > 1) { page_ = (page_ + 1) % m->count; return Handled; }   // ▷
    if (k.submenu) {
        if (depth_ < MaxDepth) { stack_[depth_++] = k.submenu; page_ = 0; }
        return Handled;
    }
    return k.label ? k.action : None;
}

// ---------------------------------------------------------------- SelectList
void SelectList::clampSelection() noexcept {
    const int n = static_cast<int>(items.size());
    selected = n == 0 ? 0 : std::clamp(selected, 0, n - 1);
}
bool SelectList::key(const KeyEvent& ev, int pageRows) {
    const int n = static_cast<int>(items.size());
    if (n == 0) return false;
    clampSelection();
    switch (ev.key) {
    case Key::Up: selected = (selected + n - 1) % n; return true;
    case Key::Down: selected = (selected + 1) % n; return true;
    case Key::PageUp: selected = std::max(0, selected - pageRows); return true;
    case Key::PageDown: selected = std::min(n - 1, selected + pageRows); return true;
    case Key::Home: selected = 0; return true;
    case Key::End: selected = n - 1; return true;
    default: return false;
    }
}
void SelectList::draw(gfx::Canvas& c, gfx::Rect area, int rowH) const {
    const int n = static_cast<int>(items.size());
    const int visible = std::max(1, area.h / rowH);
    if (selected < first_) first_ = selected;
    if (selected >= first_ + visible) first_ = selected - visible + 1;
    first_ = std::clamp(first_, 0, std::max(0, n - visible));
    for (int i = 0; i < visible && first_ + i < n; ++i) {
        const int idx = first_ + i, y = area.y + i * rowH;
        const bool sel = idx == selected;
        if (sel) c.rect(area.x, y, area.w, rowH, Select, true);
        ui::textFit(c, area.x + 6, y + (rowH - 16) / 2, items[static_cast<std::size_t>(idx)], area.w - 12, sel ? SelectInk : Ink);
    }
    ui::scrollBar(c, area.x + area.w - 3, area.y, visible * rowH, n, visible, first_);
}

// ---------------------------------------------------------------- ConfirmBox / AskBox
int ConfirmBox::key(const KeyEvent& ev) {
    if (!open_) return -1;
    if (ev.is(Key::F1) || ev.is(Key::Exe)) return 1;
    if (ev.is(Key::F6) || ev.is(Key::Exit)) return 0;
    return -1;
}
void ConfirmBox::draw(gfx::Canvas& c) const {
    if (!open_) return;
    const gfx::Rect r{16, 110, ui::ScreenW - 32, 80};
    ui::panel(c, r, 0xFFFF, Warn);
    ui::text(c, r.x + 8, r.y + 8, text_.c_str(), Ink);
    ui::text(c, r.x + 8, r.y + r.h - 20, "F1/EXE: S\xC3\xAC   F6/EXIT: No", Muted, font::Face::Small);
}
void AskBox::open(std::string prompt, int tag, char first) {
    prompt_ = std::move(prompt);
    tag_ = tag;
    input_.clear();
    if (first) input_.insert(first);
    open_ = true;
}
int AskBox::key(const KeyEvent& ev) {
    switch (ev.key) {
    case Key::Exe: open_ = false; return 1;
    case Key::Exit: open_ = false; return 0;
    case Key::Left: input_.moveLeft(); return -1;
    case Key::Right: input_.moveRight(); return -1;
    case Key::Del: input_.backspace(); return -1;
    case Key::AC: input_.clear(); return -1;
    case Key::Char: if (!ev.ctrl) input_.insert(ev.ch); return -1;
    default: return -1;
    }
}
void AskBox::draw(gfx::Canvas& c) const {
    if (!open_) return;
    const gfx::Rect r{20, 120, ui::ScreenW - 40, 58};
    ui::panel(c, r, 0xFFFF, Accent);
    ui::textFit(c, r.x + 8, r.y + 6, prompt_, r.w - 16, Accent, font::Face::Small);
    const math::CursorTarget cur{input_.activeRowPtr(), input_.cursorIndex()};
    const auto sz = math::measure(input_.root());
    auto l = math::layout(input_.root(), r.x + 8, r.y + 24 + sz.baseline, r.w - 16, 30, math::Align::Left, &cur);
    math::draw(c, l, Ink, Cursor, Placeholder);
}

// ---------------------------------------------------------------- SET UP da tabella
void fillSetup(const Context& ctx, const std::vector<SetupItem>& items, Form& form) {
    form.fields.clear();
    for (const SetupItem& it : items) {
        const int sel = std::clamp(it.get(ctx), 0, static_cast<int>(it.choices.size()) - 1);
        form.fields.push_back(FormField{it.label, it.choices[static_cast<std::size_t>(sel)], FormField::Type::Choice, it.choices, sel});
    }
    form.selected = 0;
}
void applySetup(Context& ctx, const std::vector<SetupItem>& items, const Form& form) {
    for (std::size_t i = 0; i < items.size() && i < form.fields.size(); ++i) items[i].set(ctx, form.fields[i].choice);
}
void drawFormScreen(gfx::Canvas& c, const std::string& title, const Form& form, int height, const char* const keys[6]) {
    const int top = ui::ContentTop + 1;
    c.rect(0, top, ui::ScreenW, 20, SelectSoft, true);
    ui::text(c, 6, top + 2, title.c_str(), Ink);
    form.draw(c, top + 22, top + 22 + height);
    if (keys) ui::softKeys(c, keys);
}

// ---------------------------------------------------------------- VARS dai resolver
std::vector<VarsItem> varsItems(const Context& ctx) {
    std::vector<VarsItem> out;
    const Workspace& ws = ctx.ws;
    const NameResolver& names = ws;
    for (const char* const* n = systemVariableNames(); *n; ++n) {
        double v = 0;
        if (names.resolve(*n, v)) out.push_back(VarsItem{std::string(*n) + " = " + std::to_string(v), *n});
    }
    const CollectionResolver& coll = ws;
    for (int i = 0; i < CollectionResolver::Slots; ++i) {
        CalcValue v;
        if (coll.collection(CollectionResolver::Memory::List, i, v))
            out.push_back(VarsItem{"List " + std::to_string(i + 1), "List " + std::to_string(i + 1)});
    }
    for (int i = 0; i < CollectionResolver::Slots; ++i) {
        CalcValue v;
        const std::string name = std::string("Mat ") + static_cast<char>('A' + i);
        if (coll.collection(CollectionResolver::Memory::Mat, i, v)) out.push_back(VarsItem{name, name});
    }
    return out;
}

// ---------------------------------------------------------------- ModeShell
bool ModeShell::modalKey(const KeyEvent& ev, const std::function<void(int, bool)>& onConfirm, const std::function<void(int)>& onAnswer,
                         const std::function<void(int, int)>& onMenu) {
    if (confirm.active()) {
        const int r = confirm.key(ev);
        if (r == 1) {
            const std::string before = confirm.text();
            const int tag = confirm.tag();
            if (onConfirm) onConfirm(tag, true);                    // un errore lascia la conferma aperta (come TABLE)
            if (confirm.text() == before && confirm.tag() == tag) confirm.close();
        } else if (r == 0) {
            confirm.close();
        }
        return true;
    }
    if (ask.active()) {
        if (ask.key(ev) == 1 && onAnswer) onAnswer(ask.tag());
        return true;
    }
    if (menu.open) {
        const int c = menu.key(ev);
        if (c == -2) { menuTag = 0; return true; }
        if (c >= 0) {
            menu.close();
            const int tag = menuTag;
            menuTag = 0;
            if (onMenu) onMenu(tag, c);
        }
        return true;
    }
    return false;
}
void ModeShell::drawModals(gfx::Canvas& c) const {
    if (menu.open) menu.draw(c);
    ask.draw(c);
    confirm.draw(c);
}

} // namespace fx::app
