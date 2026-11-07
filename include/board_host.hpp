#pragma once
#include <cstdint>
#include <string>

// Host-only extensions of the board interface (simulator and tests).
namespace board::host {
void pushKey(std::uint16_t raw);              // queue a FIFO word (state<<8 | code)
void advance(std::uint32_t ms);               // advance the simulated clock
bool savePng(const std::string& path, int scale = 2);
std::size_t flushCount();                     // number of present() calls that sent pixels
void setBattery(int percent, bool charging);
} // namespace board::host
