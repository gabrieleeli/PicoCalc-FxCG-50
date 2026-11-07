# Graphics Engine 2D — Phase 2 progress

## Implemented in this increment

- Dashed line rasterization.
- Filled and outline ellipses.
- Polylines and quadratic/cubic/general Bézier curves (2–8 control points).
- Affine 2D transform helpers (translation, scaling, rotation) and transformed polylines.
- RGB888 ↔ RGB565 conversion and RGB565 byte-order helper.
- Fixed-capacity, allocation-free dirty-region tracking with merge and overflow fallback.
- Cohen–Sutherland clipping before Bresenham line rasterization.
- Regression tests for the new APIs and existing drawing behavior.

## Verification

Run `tests/run_host_tests.sh`. The complete host suite passes with C++20,
`-Wall -Wextra -Werror` in the current development container.

## Not yet implemented / next increment

- Polygon fill arithmetic hardening for adversarial coordinate ranges.
- Dashed-line clipping and phase preservation for very distant endpoints.
- Explicit transform composition and transformed rectangle/bitmap helpers.
- Golden-image regression fixtures and performance benchmarks.
- PicoCalc firmware compilation and physical LCD verification (not available in this environment).

## Compatibility

Existing `dirtyRect()` and `consumeDirty()` remain supported. New dirty-region consumers
must call `consumeDirtyRegions()` and transfer each returned rectangle. The capacity is
16; overflow intentionally degrades to one union rectangle rather than dropping updates.
