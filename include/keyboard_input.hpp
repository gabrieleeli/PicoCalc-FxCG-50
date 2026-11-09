#pragma once
#include <cstdint>

namespace fx {

// Physical key codes reported by the PicoCalc STM32 keyboard firmware
// (clockworkpi/PicoCalc Code/picocalc_keyboard/keyboard.h).
namespace pckey {
constexpr std::uint8_t JoyUp = 0x01, JoyDown = 0x02, JoyLeft = 0x03, JoyRight = 0x04, JoyCenter = 0x05;
constexpr std::uint8_t Backspace = 0x08, Tab = 0x09, Enter = 0x0A;
constexpr std::uint8_t F1 = 0x81, F2 = 0x82, F3 = 0x83, F4 = 0x84, F5 = 0x85;
constexpr std::uint8_t F6 = 0x86, F7 = 0x87, F8 = 0x88, F9 = 0x89, F10 = 0x90;
constexpr std::uint8_t Power = 0x91;
constexpr std::uint8_t ModAlt = 0xA1, ModShiftL = 0xA2, ModShiftR = 0xA3, ModSym = 0xA4, ModCtrl = 0xA5;
constexpr std::uint8_t Esc = 0xB1, Left = 0xB4, Up = 0xB5, Down = 0xB6, Right = 0xB7;
constexpr std::uint8_t CapsLock = 0xC1;
constexpr std::uint8_t Break = 0xD0, Insert = 0xD1, Home = 0xD2, Del = 0xD4, End = 0xD5;
constexpr std::uint8_t PageUp = 0xD6, PageDown = 0xD7;
} // namespace pckey

// FIFO word as returned by board::pollKey(): state in the high byte, key code in the low byte.
enum class KeyState : std::uint8_t { Idle = 0, Pressed = 1, Hold = 2, Released = 3 };

// Logical keys used by the applications, modelled on the fx-CG50 keypad.
enum class Key : std::uint8_t {
    None,
    Char,                         // printable ASCII in KeyEvent::ch
    F1, F2, F3, F4, F5, F6,       // soft keys (F6 = Shift+F1 on the PicoCalc)
    Shift,                        // fx-CG50 SHIFT (PicoCalc F7 = Shift+F2)
    Optn,                         // OPTN catalogue (F8 = Shift+F3)
    Vars,                         // VARS (F9 = Shift+F4)
    Setup,                        // SET UP (F10 = Shift+F5)
    Up, Down, Left, Right,
    Exe,                          // Enter
    Exit,                         // Esc
    Menu,                         // Tab -> main menu
    Del,                          // Backspace (delete left)
    DelForward,                   // Del
    AC,                           // Break (Shift+Esc): clear
    Store,                        // Insert (Shift+Enter): -> store arrow
    Home, End, PageUp, PageDown,
};

struct KeyEvent {
    Key key{Key::None};
    char ch{};
    bool longPress{};             // first HOLD report of a non-repeating key
    bool ctrl{};
    bool alt{};
    bool is(Key k) const noexcept { return key == k; }
    bool isChar(char c) const noexcept { return key == Key::Char && ch == c; }
    bool none() const noexcept { return key == Key::None; }
};

const char* keyName(Key key) noexcept;

// Converts raw FIFO words into logical events. The STM32 firmware already
// applies Shift/Caps to characters (CFG_USE_MODS) and auto-repeats printable
// characters and arrows by re-sending PRESSED every 100 ms after 300 ms. Other
// keys are reported as HOLD while held; only the first HOLD becomes a
// long-press event so function keys never auto-repeat.
class KeyDecoder {
public:
    KeyEvent decode(std::uint16_t raw) noexcept;
    bool ctrl() const noexcept { return ctrl_; }
    bool alt() const noexcept { return alt_; }
    bool shift() const noexcept { return shiftL_ || shiftR_; }
    bool sym() const noexcept { return sym_; }
    void reset() noexcept { *this = KeyDecoder{}; }
private:
    bool ctrl_{}, alt_{}, shiftL_{}, shiftR_{}, sym_{};
    std::uint8_t holdCode_{};     // key whose long-press was already reported
};

} // namespace fx
