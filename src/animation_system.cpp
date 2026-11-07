#include "animation_system.hpp"
#include <algorithm>
namespace ui {
std::uint8_t Animation::alpha(std::uint32_t now) noexcept {if(!active(now)){active_=false;return 255;}const auto elapsed=static_cast<std::uint32_t>(now-start_);return static_cast<std::uint8_t>(std::min<std::uint32_t>(255,(elapsed*255u)/duration_));}
}
