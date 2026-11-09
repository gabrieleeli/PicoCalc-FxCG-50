#include "board_port.hpp"
#include "board_config.hpp"
#include "flash_fs.hpp"
#include "keyboard_input.hpp"
#include "limits.hpp"
#include "math_engine.hpp"
#include "mem_pools.hpp"
#include "psram.hpp"
#include "run_matrix.hpp"
#include "system_info.hpp"
#include "system_runtime.hpp"
#include "ui.hpp"
#include "window_manager.hpp"
#include "pico/stdlib.h"

#include <cstdio>
#include <new>
#include <string>

namespace {
using App = fx::RunMatrixController;
void handleApp(void* context, const fx::InputEvent& event) {
    static_cast<App*>(context)->handle(event);
}
void drawHome(void* context) {
    auto& app = *static_cast<App*>(context);
    ui::drawHome(app.selection());
}
void drawGraph(void* context) {
    auto& app = *static_cast<App*>(context);
    ui::drawGraph(app.graphFunctions(), app.graphView(), app.graphSelected());
}
void drawGraphFunctions(void* context) {
    auto& app = *static_cast<App*>(context);
    ui::drawGraphFunctions(app.graphFunctions(), app.graphSelected(), app.message().c_str());
}
void drawGraph3D(void* context) {
    auto& app = *static_cast<App*>(context);
    ui::drawGraph3D(app.graph3dExpression().c_str(), app.graph3dView());
}
void drawCheatsheet(void* context) {
    auto& app = *static_cast<App*>(context);
    ui::drawCheatsheet(app.cheatsheetPage(), app.cheatsheetSelection());
}
void drawUnavailable(void* context) {
    auto& app = *static_cast<App*>(context);
    ui::drawUnavailable(app.selectedAppName(), app.message().c_str());
}
void drawRunMatrix(void* context) {
    auto& app = *static_cast<App*>(context);
    ui::drawRunMatrix(app.editor(), app.result().c_str(), app.message().c_str(), app.session().history());
}
void drawSystem(void* context) {
    auto& app = *static_cast<App*>(context);
    ui::drawSystemMenu(app.systemSelection());
}
void drawMemory(void* context) {
    auto& app = *static_cast<App*>(context);
    ui::drawMemory(app.memoryPage());
}
void drawDiagnostics(void* context) {
    auto& app = *static_cast<App*>(context);
    ui::drawDiagnostics(app.diagnosticSelection(), app.lastDiagnostic(), app.diagnosticPending());
}

App* g_app = nullptr;
std::uint32_t stampHome(void*) { return g_app->stamp(fx::AppScreen::Home); }
std::uint32_t stampGraph(void*) { return g_app->stamp(fx::AppScreen::Graph); }
std::uint32_t stampGraph3D(void*) { return g_app->stamp(fx::AppScreen::Graph3D); }
std::uint32_t stampSystem(void*) { return g_app->stamp(fx::AppScreen::System); }

fx::WindowId windowFor(fx::AppScreen screen) {
    switch (screen) {
    case fx::AppScreen::Home: return fx::WindowId::Home;
    case fx::AppScreen::Graph: return fx::WindowId::Graph;
    case fx::AppScreen::GraphFunctions: return fx::WindowId::GraphFunctions;
    case fx::AppScreen::Graph3D: return fx::WindowId::Graph3D;
    case fx::AppScreen::Cheatsheet: return fx::WindowId::Cheatsheet;
    case fx::AppScreen::Unavailable: return fx::WindowId::Unavailable;
    case fx::AppScreen::RunMatrix: return fx::WindowId::RunMatrix;
    case fx::AppScreen::System: return fx::WindowId::System;
    case fx::AppScreen::Memory: return fx::WindowId::Memory;
    case fx::AppScreen::Diagnostics: return fx::WindowId::Diagnostics;
    }
    return fx::WindowId::Home;
}

void logMemory(const char* when) {
    const auto report = fx::sysinfo::collect();
    char lines[24][fx::sysinfo::kLineChars];
    const std::size_t n = fx::sysinfo::formatMemory(report, lines, 24);
    std::printf("[mem] --- %s ---\n", when);
    for (std::size_t i = 0; i < n; ++i) std::printf("[mem] %s\n", lines[i]);
}

// Runs one UI step, converting allocation failures into a user message
// instead of letting std::bad_alloc escape (no abort, no reset).
template <class F>
void guarded(App& app, F&& step) {
    try {
        step();
    } catch (const std::bad_alloc&) {
        app.reportOutOfMemory();
        std::printf("[mem] out of memory handled (OOM events: %u)\n", static_cast<unsigned>(mem::outOfMemoryEvents()));
    } catch (const std::exception& e) {
        app.reportError(e.what());
    } catch (...) {
        app.reportError("Errore interno");
    }
}

void appMain() {
    stdio_init_all();
    fx::runtime::FaultRecord fault;
    const bool hadFault = fx::runtime::takeLastFault(fault);

    // Memory first: the PSRAM pool must exist before anything large is allocated
    // (framebuffer in PSRAM, heavy containers through operator new).
    mem::init();
    const bool psramOk = psram::init();
    board::init();
    const auto fb = board::framebuffer();
    fx::sysinfo::setFramebuffer({fb.bytes, fb.inPsram, fb.requestedPsram});
    fx::storage::mount();

    std::printf("PicoCalc FX-CG50 AI %s on %s\n", board::config::kFirmwareVersion, board::config::kBoardName);
    if (board::config::kHasPsram && !psramOk)
        std::printf("[psram] DISABLED: %s -> running from SRAM only\n", psram::info().error ? psram::info().error : "?");
    if (hadFault)
        std::printf("[fault] previous reset caused by %s on core %u (CFSR 0x%08lX PC 0x%08lX)\n",
                    fx::runtime::describe(fault.kind), fault.core, static_cast<unsigned long>(fault.cfsr),
                    static_cast<unsigned long>(fault.pc));
    logMemory("boot");

    fx::setAngleMode(fx::AngleMode::Degrees);
    static App app;
    static fx::WindowManager windows;
    g_app = &app;
    // Window snapshots (PSRAM only, see fx::limits::windowSnapshots): restoring a
    // covered window whose state did not change skips its redraw.
    windows.setSnapshotHooks({fb.pixels, fb.bytes, [] {
        board::graphics().invalidate({0, 0, board::graphics().width(), board::graphics().height()});
        board::present();
    }});
    if (hadFault) app.reportError(fx::runtime::describe(fault.kind));
    else if (board::config::kHasPsram && !psramOk) app.reportError("PSRAM disabilitata: avvio solo in SRAM");

    windows.registerWindow(fx::WindowId::Home, &app, handleApp, drawHome, 22, stampHome);
    windows.registerWindow(fx::WindowId::Graph, &app, handleApp, drawGraph, 0, stampGraph);
    windows.registerWindow(fx::WindowId::GraphFunctions, &app, handleApp, drawGraphFunctions, 0);
    windows.registerWindow(fx::WindowId::Graph3D, &app, handleApp, drawGraph3D, 0, stampGraph3D);
    windows.registerWindow(fx::WindowId::Cheatsheet, &app, handleApp, drawCheatsheet, 0);
    windows.registerWindow(fx::WindowId::Unavailable, &app, handleApp, drawUnavailable, 0);
    windows.registerWindow(fx::WindowId::RunMatrix, &app, handleApp, drawRunMatrix, 0);
    windows.registerWindow(fx::WindowId::System, &app, handleApp, drawSystem, 3, stampSystem);
    windows.registerWindow(fx::WindowId::Memory, &app, handleApp, drawMemory, 0);
    windows.registerWindow(fx::WindowId::Diagnostics, &app, handleApp, drawDiagnostics, 0);
    windows.show(fx::WindowId::Home);
    guarded(app, [&] { windows.render(); });

    while (true) {
        const int raw = board::readKey();
        if (raw >= 0) {
            const auto event = fx::decodePicoCalcKey(static_cast<std::uint16_t>(raw));
            if (event.action != fx::InputAction::None) {
                const fx::WindowId previous = windows.active();
                guarded(app, [&] { windows.dispatch(event); });
                const fx::WindowId next = windowFor(app.screen());
                if (next != previous) {
                    if (windows.isPrevious(next)) {
                        windows.close();                       // "back": restores a snapshot if still valid
                    } else if (next == fx::WindowId::Home) {
                        windows.closeAll();
                        if (windows.active() != next) windows.show(next);
                    } else if (!windows.open(next)) {
                        windows.show(next); // navigation stack full or OOM: replace instead of failing
                    }
                }
                if (app.screen() == fx::AppScreen::Home)
                    windows.setFocus(static_cast<std::uint16_t>(app.selection()));
                guarded(app, [&] { windows.render(); });
                if (app.hasToast()) {
                    ui::drawToast(app.toast());
                    app.clearToast();
                }
                if (app.diagnosticPending()) {
                    // The "running" frame is already on screen; run the test, then redraw.
                    guarded(app, [&] { app.runPendingDiagnostic(); });
                    guarded(app, [&] { windows.render(); });
                    logMemory("after diagnostic");
                }
            }
        }
        sleep_ms(12);
    }
}
} // namespace

int main() {
    // The SDK boot stack is 4 KiB in SCRATCH_Y; the application (recursive
    // parser, layout engine, exceptions) runs on the larger guarded SRAM stack.
    fx::runtime::runOnAppStack(appMain);
}
