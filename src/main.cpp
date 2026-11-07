#include "board_port.hpp"
#include "ui.hpp"
#include "math_engine.hpp"
#include "keyboard_input.hpp"
#include "run_matrix.hpp"
#include "window_manager.hpp"
#include "pico/stdlib.h"

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
    ui::drawGraph(app.graphExpression().c_str(), app.graphView());
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

fx::WindowId windowFor(fx::AppScreen screen) {
    switch (screen) {
    case fx::AppScreen::Home: return fx::WindowId::Home;
    case fx::AppScreen::Graph: return fx::WindowId::Graph;
    case fx::AppScreen::Cheatsheet: return fx::WindowId::Cheatsheet;
    case fx::AppScreen::Unavailable: return fx::WindowId::Unavailable;
    case fx::AppScreen::RunMatrix: return fx::WindowId::RunMatrix;
    }
    return fx::WindowId::Home;
}
}

int main() {
    board::init();
    fx::setAngleMode(fx::AngleMode::Degrees);
    App app;
    fx::WindowManager windows;

    windows.registerWindow(fx::WindowId::Home, &app, handleApp, drawHome, 10);
    windows.registerWindow(fx::WindowId::Graph, &app, handleApp, drawGraph, 0);
    windows.registerWindow(fx::WindowId::Cheatsheet, &app, handleApp, drawCheatsheet, 0);
    windows.registerWindow(fx::WindowId::Unavailable, &app, handleApp, drawUnavailable, 0);
    windows.registerWindow(fx::WindowId::RunMatrix, &app, handleApp, drawRunMatrix, 0);
    windows.show(fx::WindowId::Home);
    windows.render();

    while (true) {
        const int raw = board::readKey();
        if (raw >= 0) {
            const auto event = fx::decodePicoCalcKey(static_cast<std::uint16_t>(raw));
            if (event.action != fx::InputAction::None) {
                const fx::WindowId previous = windows.active();
                windows.dispatch(event);
                const fx::WindowId next = windowFor(app.screen());
                if (next != previous) {
                    if (next == fx::WindowId::Home && previous != fx::WindowId::Home) {
                        if (!windows.close()) windows.show(next);
                    } else {
                        if (!windows.open(next)) windows.show(next);
                    }
                }
                if (app.screen() == fx::AppScreen::Home)
                    windows.setFocus(static_cast<std::uint16_t>(app.selection()));
                windows.render();
            }
        }
        sleep_ms(12);
    }
}
