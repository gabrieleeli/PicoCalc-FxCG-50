# Graphics Engine Audit — PicoCalc FX-CG50 AI v1.4.2

Date: 2026-11-07  
Scope: source archive `PicoCalcFXAI-v1.4.2-graphics-integration.zip` as extracted for this audit.

## Initial verification

- `tests/run_host_tests.sh`: **PASS** (all eight host test executables built with `-std=c++17 -O2 -Wall -Wextra -Werror` and ran successfully).
- Pico SDK firmware build: **NOT RUN**. `PICO_SDK_PATH` and the target toolchain have not been verified in this environment.
- Desktop SDL build: **NOT RUN**. SDL2 and SDL2_ttf development packages have not been verified.
- Physical PicoCalc display: **NOT VERIFIED**. No physical device is attached to this host session.

## Existing components

| Component | Existing implementation | Current limits / audit notes |
|---|---|---|
| RGB565 2D canvas | `include/graphics_engine.hpp`, `src/graphics_engine.cpp` | Allocation-free caller-owned framebuffer; pixel, clipped line, h/v line, dashed line, rectangle, rounded rectangle, circle, ellipse, arc, polygon, polyline, Bézier, transformed polyline, bitmap, layer, alpha blend, RGB conversion and fixed-capacity dirty regions. No general transform stack or transformed bitmap/rectangle helpers yet. Filled polygons used to reject more than 32 vertices; since v1.9.0 larger polygons use a pooled scratch buffer (cap per board in `fx::limits`). |
| Font renderer | `include/font_renderer.hpp`, `src/font_renderer.cpp` | 5x7 glyph-oriented renderer and scale support. Public API is C-string based; no explicit UTF-8 decoding, proportional font metrics, style, kerning, baseline/bounds API or word wrapping. Unicode coverage must not be assumed. |
| Natural/math editor | `include/natural_editor.hpp`, `src/natural_editor.cpp` | Existing structured editor is present and must be preserved. `math_renderer` adapts its current `NaturalNode` model; it is not yet the complete AST specified in the project mission. |
| Math layout | `include/math_renderer.hpp`, `src/math_renderer.cpp` | Produces `NaturalDrawOp` operations and size/clipping metadata. Needs audit and expansion for nested fractions/radicals, integral/sum/product layouts, matrix layout, structural cursor navigation and viewport scrolling. |
| Graph renderer | `include/graph_renderer.hpp`, `src/graph_renderer.cpp` | One expression plotted as an explicit y=f(x) curve, rectangular grid/axes, trace cursor, zoom and pan. No polar/parametric/inequality/conic/statistical/3D families. View-range validation and discontinuity handling need hardening. |
| UI / launcher | `include/ui.hpp`, `src/ui.cpp`, `src/main.cpp` | Home and application screens exist; current UI is procedural rather than a complete retained widget/window/focus architecture. |
| Widgets | `include/widget_system.hpp`, `src/widget_system.cpp` | Reusable panel/title/popup/selection drawing primitives only; not interactive stateful controls. |
| Animation | `include/animation_system.hpp`, `src/animation_system.cpp` | Minimal time-based alpha animation primitive. No scheduler, transitions or animation collection. |
| Board display | `include/board_port.hpp`, `src/board_port.cpp` | Pico SDK-specific backend with static 320x320 RGB565 framebuffer and dirty-rectangle transfer. Pin assignments, controller sequence, orientation and offset are source constants; panel-specific physical behavior is not proven. |
| Host desktop | `desktop/` | Separate SDL2/SDL_ttf app; current CMake target includes core calculator sources, not the shared graphics/font/UI modules. |

## Build and architecture findings

1. The project mission requests C++20, but root firmware CMake, desktop CMake and host test script currently select C++17.
2. Firmware `CMakeLists.txt` directly imports Pico SDK and binds board code to Pico SDK peripherals. There is no abstract `DisplayBackend` contract yet.
3. `board::graphics()` exposes the shared `gfx::Canvas`, but the host desktop path does not currently instantiate the same display abstraction or use the shared renderer stack.
4. The graphics engine uses a single dirty bounding rectangle. This is simple and deterministic but can cause excess transfer for disjoint updates.
5. `Canvas::line` uses Bresenham and calls `pixel` for every sample. Extreme off-screen endpoints can cause long loops; clipping before rasterization is a performance/safety improvement.
6. (Resolved in v1.9.0) Filled polygon scan conversion used fixed local storage for at most 32 vertices; it now falls back to a pooled buffer and, if that fails, draws the outline.
7. `GraphRenderer::draw` assumes nonzero finite ranges and a drawable viewport. Degenerate or non-finite ranges should be rejected before coordinate conversion.
8. `board_port.cpp` contains a second private 5x7 glyph path (`drawChar`/`glyph`) in addition to `font_renderer.cpp`; the duplicate `drawChar` is currently unused and should be removed or routed through the common renderer after checking the full board build.
9. Existing tests are smoke tests. They verify basic behavior and compilation, but there is no golden-image harness, fuzzing, allocation instrumentation, performance budget or target hardware test.

## Missing modules / capabilities

- Abstract display backend shared by host and PicoCalc.
- Explicit framebuffer format/endian conversion and configurable panel geometry.
- Complete Text Layout Engine.
- Complete structured Math Layout Engine and structural cursor.
- Retained widget system, Window Manager and centralized focus/input dispatch.
- Full launcher paging and app registration for all planned applications.
- Robust multi-family graph engine and statistical plot engine.
- Non-blocking animation scheduler.
- Software 3D renderer.
- Expanded deterministic unit and golden-image tests.

## Dependency direction (target)

`DisplayBackend` owns output transfer only. `Framebuffer`/`Canvas` own pixel storage and 2D drawing. Font and text layout depend on Canvas. Math layout depends on text/font metrics and emits positioned draw operations. Widgets and WindowManager depend on the shared drawing/layout interfaces. Graph, statistics and 3D renderers emit primitives through the same Canvas. Applications depend on UI/services, never on a private display renderer.

## Implementation plan

1. Audit and architecture (this document; preserve current APIs while defining seams).
2. Consolidate 2D rasterization, clipping, transforms, color helpers and dirty tracking.
3. Expand font metrics and UTF-8 fallback while retaining current glyph API.
4. Add a reusable text layout/wrapping engine.
5. Extend the existing natural-expression model into a structured math layout and cursor model.
6. Complete launcher registration/paging and route all screens through shared UI primitives.
7. Introduce stateful widgets, focus dispatch and WindowManager.
8. Expand graph plotting (validated viewports, robust sampling, discontinuity breaks, polar/parametric/inequality/conics).
9. Add statistical plot primitives and plot types.
10. Add non-blocking animation scheduler.
11. Introduce `DisplayBackend`, host backend and PicoCalc adapter; preserve board-specific driver behavior.
12. Add bounded-memory software 3D renderer.
13. Integrate applications and run host/firmware/hardware verification.

## Required verification labels

- **Host verified**: built and tested on host.
- **Hardware build verified**: firmware compiled and linked with the configured Pico SDK/toolchain.
- **Physical display verified**: flashed and visually checked on the actual PicoCalc panel.

No host result may be promoted to hardware or physical-display verification.

## Changes made in this audit session

- Added this audit and `docs/GRAPHICS_ENGINE_ARCHITECTURE.md` with the current-state inventory, target layering and migration constraints.
- Updated firmware CMake, desktop CMake and the host test script to request **C++20** (the host test suite still passes under C++20).
- Re-ran `tests/run_host_tests.sh` after the standard change: **PASS**.
- Attempted firmware CMake configuration: **BLOCKED**, because `PICO_SDK_PATH` is unset and the Pico SDK import file cannot be found.
- Attempted desktop CMake configuration: **BLOCKED**, because SDL2 development package is not installed in this environment.

## Session result

The audit phase is complete. The codebase's host-testable modules now compile under the requested C++20 language mode. No claim is made yet that the Pico SDK firmware or SDL desktop executable builds successfully; those toolchains/dependencies are unavailable in this session.


## Phase 2 progress (2026-11-07)

Implemented: dashed lines; filled/outline ellipses; polylines; Bézier curves with 2–8 control points; affine transform helpers and transformed polylines; RGB888/RGB565 conversion and byte swapping; fixed-capacity dirty-region tracking; Cohen–Sutherland line clipping before Bresenham rasterization. Existing `dirtyRect()` and `consumeDirty()` APIs remain compatible.

Verification in this increment: the focused `graphics_smoke` executable builds with C++20, `-Wall -Wextra -Werror` and passes, including a line spanning endpoints at ±1,000,000,000. A full `tests/run_host_tests.sh` invocation did not complete within the available execution window; therefore the full suite is **not claimed as verified in this increment**. Pico SDK build and physical LCD remain unverified.
