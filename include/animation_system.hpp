#pragma once
#include "graphics_engine.hpp"
#include <cstdint>
namespace ui {
class Animation {
public:
    void start(std::uint32_t now,std::uint16_t duration) noexcept {start_=now;duration_=duration;active_=duration!=0;}
    bool active(std::uint32_t now) const noexcept {return active_&&static_cast<std::uint32_t>(now-start_)<duration_;}
    std::uint8_t alpha(std::uint32_t now) noexcept;
    void cancel() noexcept {active_=false;}
private: std::uint32_t start_{};std::uint16_t duration_{};bool active_{};
};
}
