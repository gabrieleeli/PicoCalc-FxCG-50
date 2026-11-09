// RP2350 entry point: board bring-up (PSRAM, memory pools, LCD, keyboard,
// flash regions), then the portable Firmware loop.
#include "board_config.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "firmware.hpp"
#include "psram.hpp"
#include "system_info.hpp"
#include "system_runtime.hpp"
#include "ui_kit.hpp"
#include "pico/stdlib.h"
#include <cstdio>
#include <exception>
#include <string>

#ifndef FXAI_VERSION
#define FXAI_VERSION "dev"
#endif

namespace {
#if FXAI_DISPLAY_CALIBRATION_SCREEN
// Hardware bring-up pattern: verifies colour order, orientation and offsets.
void calibrationScreen() {
    auto& c = board::graphics();
    c.clear(0x0000);
    const std::uint16_t bars[8] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0xFFE0, 0xF81F, 0x07FF, 0x7BEF};
    for (int i = 0; i < 8; ++i) c.rect(i * 40, 30, 40, 60, bars[i], true);
    ui::text(c, 4, 4, "LCD TEST 320x320  R G B W Y M C Grey", 0xFFFF, font::Face::Small);
    c.rect(0, 0, 320, 320, 0xFFFF, false);
    c.rect(0, 100, 20, 20, 0xF800, true);   ui::text(c, 24, 102, "TOP-LEFT = RED", 0xFFFF);
    c.rect(300, 300, 20, 20, 0x001F, true); ui::text(c, 150, 302, "BOTTOM-RIGHT = BLUE", 0xFFFF);
    for (int x = 0; x < 320; x += 16) c.vLine(x, 130, 150, 0x4208);
    ui::text(c, 40, 200, "Premi un tasto per continuare", 0xFFFF);
    board::invalidate();
    board::present();
    std::uint16_t raw = 0;
    while (true) { board::service(); if (board::pollKey(raw) && (raw >> 8) == 1) break; board::idle(); }
}
#endif

void logMemory(fx::app::Platform& platform, const char* when) {
    char lines[24][fx::sysinfo::kLineChars];
    const std::size_t n = fx::sysinfo::formatMemory(fx::sysinfo::collect(platform.hardware()), lines, 24);
    std::printf("[mem] --- %s ---\n", when);
    for (std::size_t i = 0; i < n; ++i) std::printf("[mem] %s\n", lines[i]);
}
} // namespace

int main() {
    // The 32 KB core 0 stack (linker script) is guarded by MSPLIM since reset;
    // paint its unused part for the high-water mark shown by SYSTEM > INFO.
    fx::runtime::paintStacks();
    fx::runtime::FaultRecord fault;
    const bool hadFault = fx::runtime::takeLastFault(fault);
    board::init();   // PSRAM + memory pools first, then LCD, keyboard, flash regions
    std::printf("PicoCalc FX %s booting (%s, %s)\n", FXAI_VERSION, board::name(), board::config::kBoardName);
    if (hadFault)
        std::printf("[fault] previous reset caused by %s on core %u (CFSR 0x%08lX PC 0x%08lX LR 0x%08lX)\n",
                    fx::runtime::describe(fault.kind), fault.core, static_cast<unsigned long>(fault.cfsr),
                    static_cast<unsigned long>(fault.pc), static_cast<unsigned long>(fault.lr));
    const bool psramFailed = board::config::kHasPsram && !psram::available();
    if (psramFailed)
        std::printf("[psram] DISABLED: %s -> running from SRAM only\n", psram::info().error ? psram::info().error : "?");
#if FXAI_DISPLAY_CALIBRATION_SCREEN
    calibrationScreen();
#endif
    fx::BoardPlatform platform;
    static fx::Firmware firmware(platform, board::storageFlash(), board::legacyStorageFlash());
    firmware.boot();
    std::printf("workspace %s%s, record capacity %u bytes, free SRAM heap %u bytes\n",
                firmware.loadedFromFlash() ? "restored" : "new", firmware.migrated() ? " (imported from v1.8.0)" : "",
                static_cast<unsigned>(platform.storageCapacity()), static_cast<unsigned>(board::freeHeap()));
    logMemory(platform, "boot");
    // A UF2 built for a larger flash than the chip (Pico Plus 2 W image on a
    // Pico 2 W): the storage regions beyond the chip were disabled by
    // board::init(); say which image to install. Checked first: it explains
    // the other symptoms (no PSRAM, nothing saved).
    const auto chip = board::flashChip();
    if (chip.bytes && chip.bytes < board::config::kFlashSize) {
        char text[160];
        std::snprintf(text, sizeof text, "System ERROR: UF2 per %s su una flash da %u MB. Installare %s: i dati non vengono salvati",
                      board::config::kBoardName, static_cast<unsigned>(chip.bytes >> 20),
                      board::config::kIsPicoPlus2W ? "picocalc_fxai_pico2w.uf2" : "l'UF2 di questa scheda");
        std::printf("[flash] %s\n", text);
        firmware.notice(static_cast<const char*>(text));
    } else if (hadFault)
        firmware.notice(std::string("System ERROR: riavvio dopo ") + fx::runtime::describe(fault.kind) +
                        " (dettagli su USB seriale)");
    else if (psramFailed)
        firmware.notice(std::string("Memory ERROR: PSRAM disabilitata, avvio solo in SRAM (") +
                        (psram::info().error ? psram::info().error : "?") + ")");
    board::invalidate();
    firmware.draw(board::graphics());
    board::present();
    while (true) {
        // The AppManager already turns application exceptions into error
        // boxes; this is the last line of defence of the main loop.
        try {
            board::service();
            std::uint16_t raw = 0;
            while (board::pollKey(raw)) firmware.rawKey(raw);
            if (firmware.step()) {
                firmware.draw(board::graphics());
                board::present();
            }
        } catch (const std::bad_alloc&) {
            std::printf("[loop] std::bad_alloc\n");
            firmware.notice("Memory ERROR: memoria insufficiente");   // allocation-free path
        } catch (const std::exception& e) {
            std::printf("[loop] %s\n", e.what());
            char text[160];
            std::snprintf(text, sizeof text, "ERROR: %s", e.what());
            firmware.notice(static_cast<const char*>(text));
        }
        board::idle();
    }
}
