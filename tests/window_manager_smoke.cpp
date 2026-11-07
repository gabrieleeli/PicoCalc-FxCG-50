#include "window_manager.hpp"
#include <cassert>

namespace {
struct Probe { int events{}; int draws{}; fx::InputAction last{fx::InputAction::None}; };
void onEvent(void* p, const fx::InputEvent& e) {
    auto& probe = *static_cast<Probe*>(p);
    ++probe.events;
    probe.last = e.action;
}
void onDraw(void* p) { ++static_cast<Probe*>(p)->draws; }
}

int main() {
    fx::WindowManager wm;
    Probe home{}, graph{};
    assert(!wm.show(fx::WindowId::Home));
    assert(wm.registerWindow(fx::WindowId::Home, &home, onEvent, onDraw, 4));
    assert(wm.registerWindow(fx::WindowId::Graph, &graph, onEvent, onDraw, 2));
    assert(!wm.registerWindow(fx::WindowId::Home, &home, onEvent, onDraw));
    assert(!wm.registerWindow(fx::WindowId::RunMatrix, &home, nullptr, onDraw));

    assert(wm.show(fx::WindowId::Home));
    assert(wm.active() == fx::WindowId::Home);
    assert(wm.focusCount() == 4 && wm.focus() == 0);
    assert(wm.setFocus(3));
    assert(!wm.setFocus(4));
    assert(wm.moveFocus(1) && wm.focus() == 0);
    assert(wm.moveFocus(-1) && wm.focus() == 3);

    wm.dispatch({fx::InputAction::Right, 0});
    wm.render();
    assert(home.events == 1 && home.draws == 1);
    assert(home.last == fx::InputAction::Right);

    assert(wm.open(fx::WindowId::Graph));
    assert(wm.stackDepth() == 1 && wm.active() == fx::WindowId::Graph);
    assert(wm.focusCount() == 2 && wm.focus() == 0);
    assert(wm.setFocus(1));
    wm.dispatch({fx::InputAction::Evaluate, 0});
    wm.render();
    assert(graph.events == 1 && graph.draws == 1);
    assert(!wm.open(fx::WindowId::Unavailable));
    assert(wm.close());
    assert(wm.active() == fx::WindowId::Home && wm.focus() == 3);
    assert(!wm.close());
    assert(wm.moveFocus(1));
    assert(wm.focus() == 0);
    return 0;
}
