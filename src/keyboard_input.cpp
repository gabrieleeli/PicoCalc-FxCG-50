#include "keyboard_input.hpp"

namespace fx {
namespace {
Key mapCode(std::uint8_t code) noexcept {
    using namespace pckey;
    switch (code) {
    case Enter: case JoyCenter: return Key::Exe;
    case Backspace: return Key::Del;
    case Tab: return Key::Menu;
    case Esc: return Key::Exit;
    case Up: case JoyUp: return Key::Up;
    case Down: case JoyDown: return Key::Down;
    case Left: case JoyLeft: return Key::Left;
    case Right: case JoyRight: return Key::Right;
    case F1: return Key::F1;
    case F2: return Key::F2;
    case F3: return Key::F3;
    case F4: return Key::F4;
    case F5: return Key::F5;
    case F6: return Key::F6;
    case F7: return Key::Shift;
    case F8: return Key::Optn;
    case F9: return Key::Vars;
    case F10: return Key::Setup;
    case Break: return Key::AC;
    case Insert: return Key::Store;
    case Home: return Key::Home;
    case Del: return Key::DelForward;
    case End: return Key::End;
    case PageUp: return Key::PageUp;
    case PageDown: return Key::PageDown;
    default: return Key::None;
    }
}
} // namespace

const char* keyName(Key key) noexcept {
    switch (key) {
    case Key::None: return "NONE"; case Key::Char: return "CHAR";
    case Key::F1: return "F1"; case Key::F2: return "F2"; case Key::F3: return "F3";
    case Key::F4: return "F4"; case Key::F5: return "F5"; case Key::F6: return "F6";
    case Key::Shift: return "SHIFT"; case Key::Optn: return "OPTN"; case Key::Vars: return "VARS";
    case Key::Setup: return "SETUP"; case Key::Up: return "UP"; case Key::Down: return "DOWN";
    case Key::Left: return "LEFT"; case Key::Right: return "RIGHT"; case Key::Exe: return "EXE";
    case Key::Exit: return "EXIT"; case Key::Menu: return "MENU"; case Key::Del: return "DEL";
    case Key::DelForward: return "DELFWD"; case Key::AC: return "AC"; case Key::Store: return "STO";
    case Key::Home: return "HOME"; case Key::End: return "END"; case Key::PageUp: return "PGUP";
    case Key::PageDown: return "PGDN";
    }
    return "?";
}

KeyEvent KeyDecoder::decode(std::uint16_t raw) noexcept {
    const auto state = static_cast<KeyState>(raw >> 8);
    const auto code = static_cast<std::uint8_t>(raw & 0xFFU);
    if (raw == 0 || state == KeyState::Idle) return {};

    // Modifier tracking (CFG_REPORT_MODS): press/hold sets, release clears.
    bool* mod = nullptr;
    switch (code) {
    case pckey::ModAlt: mod = &alt_; break;
    case pckey::ModShiftL: mod = &shiftL_; break;
    case pckey::ModShiftR: mod = &shiftR_; break;
    case pckey::ModSym: mod = &sym_; break;
    case pckey::ModCtrl: mod = &ctrl_; break;
    default: break;
    }
    if (mod) { *mod = state != KeyState::Released; return {}; }
    if (code == pckey::CapsLock || code == pckey::Power) return {};

    if (state == KeyState::Released) {
        if (code == holdCode_) holdCode_ = 0;
        return {};
    }

    KeyEvent ev;
    ev.ctrl = ctrl_;
    ev.alt = alt_;
    if (code >= 0x20 && code <= 0x7E) {
        // Printable characters auto-repeat as PRESSED; a HOLD for them is not expected.
        if (state == KeyState::Hold) return {};
        ev.key = Key::Char;
        ev.ch = static_cast<char>(code);
        return ev;
    }
    ev.key = mapCode(code);
    if (ev.key == Key::None) return {};
    if (state == KeyState::Hold) {
        if (holdCode_ == code) return {};   // only the first HOLD is reported
        holdCode_ = code;
        ev.longPress = true;
    }
    return ev;
}

} // namespace fx
