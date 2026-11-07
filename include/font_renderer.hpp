#pragma once
#include "graphics_engine.hpp"
#include <cstdint>
namespace font {
struct TextMetrics { int width{}; int height{}; };
TextMetrics measure(const char* text, int scale=1) noexcept;
TextMetrics drawText(gfx::Canvas& canvas, int x, int y, const char* text,
                     std::uint16_t color, int scale=1, int maxWidth=0) noexcept;
}
