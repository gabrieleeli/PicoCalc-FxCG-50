# VPAM audit — actual archive v1.5.3-dev

## Provenance and baseline

- Input archive: `PicoCalcFXAI-v1.5.3.zip`.
- Embedded `VERSION` before changes: `1.5.2-dev` (archive naming and embedded version did not match).
- Actual project directory: `picocalc151/`.
- Baseline host test suite: PASS before modifications.
- Working version after this increment: `1.5.3-dev`.

## Implemented

- `NaturalEditor` owns a recursive `NaturalNode` tree for rows, tokens, fractions, radicals, powers, subscripts, groups, matrices (render structure), and calculus templates (integral, derivative, sum).
- Fraction, radical, power, group and calculus insertion templates exist. Cursor navigation supports left/right and vertical movement between template fields.
- Renderer draws fractions, radicals, powers, groups, matrices and calculus symbols using recursive layout operations.
- Expression evaluator supports scalar arithmetic, implicit multiplication, powers, factorial postfix, trigonometric/hyperbolic/logarithmic functions, roots, numeric derivative, Simpson integral, and finite integer summation.
- Run-Matrix session stores up to 30 expression/result pairs and calculator memory.
- PicoCalc keyboard decoder maps directional keys, Enter, Escape, delete/backspace and F1–F6 logical actions.
- Host suite covers math, graphics, renderer, launcher, keyboard integration, cheatsheet, widgets, animation and window manager.

## Partially implemented

- The editor tree is a UI-specific `NaturalNode`, not yet a shared typed AST consumed by parser and evaluator.
- `NaturalNodeKind::Matrix` is renderable but serialization/evaluation is explicitly unsupported. Vectors and complex values exist as separate math-engine APIs, not as editable expression nodes.
- Derivative and integral aliases are numerical only; derivative uses finite differences and integral uses a fixed Simpson rule. No symbolic differentiation/integration is implemented.
- The editor has no complete selection model, clipboard, undo/redo, or arbitrary tree-path cursor abstraction.
- The renderer clips oversized layouts but does not yet provide complete two-axis viewport scrolling.
- Function aliases are evaluated from strings; not all are represented as editable function nodes.
- Physical keyboard scan codes are mapped in firmware code, but this archive has no evidence of a physical keyboard validation run.

## Missing

- Shared typed AST and one canonical parser/serializer/evaluator path.
- Structured parser support for matrices, vectors, complex numbers, products and general multi-argument nested functions.
- Natural editor templates for functions, n-th roots with editable index, product, matrix and vector entries.
- Robust selection, cut/copy/paste, undo/redo and node replacement.
- Symbolic derivatives, exact symbolic integrals, exact rational result propagation and configurable result formats.
- Persistent structured history and complete variable/answer recall UI.
- On-device Pico 2 W build and physical LCD/keyboard tests.

## Increment delivered in this archive

- Canonical `root(degree,radicand)` semantics, including editor serialization and display formatting.
- `frac(a,b)`, `factorial(n)`, `nCr(n,r)` and `nPr(n,r)` evaluation aliases with validation.
- Regression tests for the above plus invalid root index and zero fraction denominator.

## Validation

Run `bash tests/run_host_tests.sh` from the project root. Host tests pass. `PICO_SDK_PATH` is unset and `arm-none-eabi-g++` is unavailable in the current environment, so firmware build and hardware validation remain unverified.
