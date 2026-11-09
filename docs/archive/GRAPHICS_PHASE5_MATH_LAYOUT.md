# Phase 5 — Natural Math Layout

## Implemented
- Recursive measurement for rows, tokens, fractions, radicals, powers, subscripts, groups and matrix containers.
- Bounded recursion and saturating width/height accumulation to keep malformed or very large trees from overflowing layout arithmetic.
- Layout in a caller-provided viewport with left, centered and right alignment.
- Text clipping at character-cell boundaries and line clipping against the viewport; clipping is reported through `Layout::clipped`.
- Empty or incomplete structures are handled defensively by measurement and drawing rather than dereferenced.
- Existing `measure()` and `layout(node,x,baseline,maxWidth,maxHeight)` call sites remain source-compatible.

## Coordinate contract
`baseline` is the text baseline in screen coordinates. `Layout::top` is the top edge derived from the measured baseline offset. `maxWidth` and `maxHeight` define the viewport beginning at `(x, top)`. `Align` controls horizontal placement when the expression is narrower than the viewport. `Layout::size` reports the intrinsic measured size; `Layout::clipped` reports any content omitted or cut by the viewport.

## Verification
`tests/math_renderer_smoke.cpp` covers nested fraction layout, fraction rule generation, too-small viewports, all horizontal alignments, partial text clipping, line clipping and incomplete-node safety. Host test compiled with C++20, `-Wall -Wextra -Werror` and passed.

## Remaining limitations
This is a compact natural-layout engine, not a full CASIO font/typesetting clone. Glyph advances assume a six-pixel cell; matrix nodes are presentation containers and do not yet define editable matrix-cell semantics. Multi-line automatic wrapping and scalable font metrics are not part of this phase.
