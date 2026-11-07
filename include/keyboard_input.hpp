#pragma once
#include <cstdint>

namespace fx {
enum class InputAction {
    None, Character, Left, Right, Up, Down, Backspace, Delete,
    Fraction, Radical, Power, Subscript, Group, Evaluate, Escape, Clear
};
struct InputEvent {
    InputAction action{InputAction::None};
    char character{};
};

// Decode the PicoCalc STM32 FIFO word: high byte = key state,
// low byte = key code. Only press/hold events generate application input.
InputEvent decodePicoCalcKey(std::uint16_t raw);
}
