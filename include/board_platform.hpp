#pragma once
#include "app.hpp"
#include "board_port.hpp"

namespace fx {
// app::Platform backed by the board:: functions (works for both ports).
class BoardPlatform final : public app::Platform {
public:
    std::uint32_t millis() override { return board::millis(); }
    int batteryPercent() override { return board::batteryPercent(); }
    bool charging() override { return board::charging(); }
    void setBacklight(std::uint8_t lcd, std::uint8_t kbd) override { board::setBacklight(lcd, kbd); }
    std::size_t freeHeap() override { return board::freeHeap(); }
    std::size_t storageCapacity() override { return board::storageFlash().size() / 2; }
    const char* name() override { return board::name(); }
    net::ChatTransport* chat() override { return board::chatTransport(); }
    files::FileSystem* files() override { return board::fileSystem(); }
};
} // namespace fx
