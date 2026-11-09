# Window Manager — v1.5 development increment

## Purpose

`fx::WindowManager` centralizes the active application window, input dispatch,
render dispatch, focus index and a bounded return stack. It is deliberately
independent of `gfx::Canvas`: each screen's existing UI renderer continues to
paint through the existing `board` / Graphics Engine path.

## Constraints

- C++20; no `std::function`, exceptions or RTOS dependency; every operation is `noexcept`.
- v1.9.0: capacity is no longer fixed at 8/8. The registry and the return
  stack grow through the memory pools up to the per-board caps of
  `fx::limits` (16/16 on Pico 2 W, 64/64 on Pico Plus 2 W); an allocation
  failure makes `registerWindow()`/`open()` return `false` (no state change).
- v1.9.0: optional full-screen snapshots of covered windows, stored in PSRAM
  only (`limits::windowSnapshots`, 0 on Pico 2 W). `close()` restores a
  snapshot instead of redrawing when the window's state stamp is unchanged.
  `closeAll()` and `isPrevious()` support "back" navigation without growing the stack.
- A window is registered with an ID, opaque context, event callback, draw
  callback and focusable-item count.
- `show()` changes the active window without changing the return stack.
- `open()` pushes the previous active window; `close()` restores it.
- Focus is stored per window and wraps with `moveFocus()`.
- Invalid IDs, duplicate registration, missing callbacks and capacity overflow
  return `false` rather than changing manager state.

## Current firmware integration

The firmware registers Home, Run-Matrix, Graph and Unavailable. The existing
`RunMatrixController` remains the source of application behavior and state;
the manager forwards events to it and selects the renderer for the resulting
`AppScreen`. The Home launcher's nine positions are mirrored as nine focus slots.
No Graphics Engine API or framebuffer ownership was changed.

## Host validation

Run `./tests/run_host_tests.sh`. The dedicated `window_manager_smoke` test checks
registration errors, active-window dispatch, drawing, focus wrapping, per-window
focus retention, stack open/close and invalid transitions.

## Hardware validation status

Not yet verified on a physical PicoCalc. The manager is hardware-independent;
LCD and keyboard validation remain separate acceptance steps. See
`DISPLAY_DRIVER.md` and `PICOCALC_KEYBOARD_RUN_MATRIX.md`.
