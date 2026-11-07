# Graphics Engine Architecture

## Current architecture (v1.4.2 archive)

The current graphics stack is a compact immediate-mode RGB565 canvas. `gfx::Canvas` wraps caller-owned pixel storage and centralizes clipping, raster primitives, alpha blending and dirty bounds. `board_port.cpp` owns the PicoCalc framebuffer and sends dirty pixels to the LCD. UI functions draw application screens through this board port. The font renderer and math/graph renderers are host-testable modules, but the desktop SDL application is not yet wired to this same stack.

## Target layering

```text
Applications (RUN-MATRIX, GRAPH, STATISTICS, AI, ...)
                    |
          Window Manager / Focus
                    |
           Reusable Widget System
                    |
     Text Layout / Math Layout / Renderers
                    |
        Font Renderer + Graphics Engine
                    |
       Framebuffer (RGB565) / Canvas
                    |
             DisplayBackend
               /       \
       HostBackend   PicoCalcBackend
```

## Contracts

- The framebuffer is RGB565 and has one explicit width, height and stride.
- Canvas coordinates are logical top-left-origin pixel coordinates; backend rotation/offset mapping belongs in the backend, not in applications.
- Clipping is centralized in Canvas and inherited by all renderers.
- Renderers draw into a supplied Canvas and do not own or transmit the physical display.
- Display backends transfer full or dirty regions and report their configured geometry/capabilities.
- No renderer may allocate per pixel or require a private framebuffer.
- Hardware-specific assumptions must be represented as configuration and verified on target.

## Migration rule

Keep current public APIs operational while introducing the new contracts. Move one application at a time to the shared interfaces; do not replace the existing calculator/math implementation as part of graphics refactoring.
