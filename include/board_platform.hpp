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
    std::size_t storageCapacity() override { return storage::RecordStore(board::storageFlash()).capacity(); }
    const char* name() override { return board::name(); }
    net::ChatTransport* chat() override { return board::chatTransport(); }
    files::FileSystem* files() override { return board::fileSystem(); }
    sysinfo::Hardware hardware() override {
        const auto fb = board::framebufferInfo();
        const auto fl = board::flashStats();
        sysinfo::Hardware h;
        h.framebufferBytes = fb.bytes; h.framebufferInPsram = fb.inPsram; h.framebufferRequestedPsram = fb.requestedPsram;
        h.framebufferPixels = fb.pixels;
        h.lastPresentUs = board::lastPresentUs();
        h.flashJobs = fl.jobs; h.flashFailures = fl.failures;
        h.firmwareBytes = board::firmwareBytes();
        const auto chip = board::flashChip();
        h.flashChipBytes = chip.bytes;
        h.flashJedec[0] = chip.manufacturer; h.flashJedec[1] = chip.type; h.flashJedec[2] = chip.capacity;
        return h;
    }
    storage::FlashDevice* diagnosticFlash() override { return board::diagnosticFlash(); }
    files::FileSystem* internalFiles() override { return board::internalFileSystem(); }
    std::uint32_t benchmarkPresent() override { return board::benchmarkFullPresent(); }
};
} // namespace fx
