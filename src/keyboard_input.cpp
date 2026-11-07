#include "keyboard_input.hpp"

namespace fx {
namespace {
constexpr std::uint8_t KEY_STATE_PRESSED = 1;
constexpr std::uint8_t KEY_STATE_HOLD = 2;
constexpr std::uint8_t KEY_LEFT = 0xB4;
constexpr std::uint8_t KEY_UP = 0xB5;
constexpr std::uint8_t KEY_DOWN = 0xB6;
constexpr std::uint8_t KEY_RIGHT = 0xB7;
constexpr std::uint8_t KEY_ESC = 0xB1;
constexpr std::uint8_t KEY_DELETE = 0xD4;
constexpr std::uint8_t KEY_BACKSPACE = 0x08;
constexpr std::uint8_t KEY_ENTER = 0x0A;
constexpr std::uint8_t KEY_TAB = 0x09;
}

InputEvent decodePicoCalcKey(std::uint16_t raw) {
    if (raw == 0) return {};
    const auto state = static_cast<std::uint8_t>(raw >> 8);
    const auto code = static_cast<std::uint8_t>(raw & 0xFF);
    if (state != KEY_STATE_PRESSED && state != KEY_STATE_HOLD) return {};

    switch (code) {
    case KEY_LEFT: return {InputAction::Left, 0};
    case KEY_RIGHT: return {InputAction::Right, 0};
    case KEY_UP: return {InputAction::Up, 0};
    case KEY_DOWN: return {InputAction::Down, 0};
    case KEY_BACKSPACE: return {InputAction::Backspace, 0};
    case KEY_DELETE: return {InputAction::Delete, 0};
    case KEY_ENTER: return {InputAction::Evaluate, 0};
    case KEY_ESC: return {InputAction::Escape, 0};
    case KEY_TAB: return {InputAction::Escape, 0}; // Logical MENU/back alias; keyboard has no dedicated MENU scan code.
    case 0x81: return {InputAction::Fraction, 0}; // F1
    case 0x82: return {InputAction::Radical, 0};  // F2
    case 0x83: return {InputAction::Power, 0};    // F3
    case 0x84: return {InputAction::Subscript, 0}; // F4
    case 0x85: return {InputAction::Group, 0};    // F5
    case 0x86: return {InputAction::Clear, 0};    // F6
    default:
        if (code >= 0x20 && code <= 0x7E)
            return {InputAction::Character, static_cast<char>(code)};
        return {};
    }
}
}
