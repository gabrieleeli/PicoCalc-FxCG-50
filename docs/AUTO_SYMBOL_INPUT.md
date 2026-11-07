# Automatic mathematical input — v1.5.1

## Goal

The calculator accepts readable ASCII aliases from the PicoCalc keyboard and converts them at input/render time into compact mathematical notation. The conversion is local C++ code: it does not call an AI service, does not use network access, and does not add a lookup table in dynamic memory.

## Automatic editor transformations

- Typing `sqrt` immediately creates a square-root template. The cursor moves into the radicand; no F2 key or parentheses are required.
- Typing `cbrt` creates a cube-root template.
- Typing `root3` through `root99` creates an indexed-root template. Example: `root5` followed by `32` evaluates as the fifth root of 32.
- Typing `^` after an operand creates a power template. Example: `x^3` is laid out with 3 as a superscript; F3 remains available as an optional shortcut.
- Existing F1–F6 editor shortcuts remain supported.

The editor serializes roots as `sqrt(radicand)` or `root(index,radicand)`, and powers as `(base)^(exponent)`, so the numerical parser remains independent of the display glyphs.

## Greek alphabet

Type the English transliteration to display a Greek letter. Lowercase aliases are `alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu nu xi omicron pi rho sigma tau upsilon phi chi psi omega`. Capitalized aliases (`Alpha`, `Beta`, etc.) display uppercase Greek. The compact 5x7 font maps the full Greek range to legible on-device bitmap approximations; it is not a typographic Unicode font.

Greek names are currently symbolic display identifiers. They are not yet separately assignable numeric variables in CalculatorMemory, so an expression containing a Greek variable is not guaranteed to evaluate. This does not affect Greek labels or constants.

## Constants and aliases

The parser accepts the project's physical constants through descriptive identifiers and short aliases:

| Alias | Display | Value |
|---|---:|---:|
| `speedOfLight` / `physc` | c | 299792458 m/s |
| `gravitationalConstant` / `physg` | G | 6.67430e-11 |
| `planckConstant` / `physh` | h | 6.62607015e-34 |
| `avogadro` / `NA` | Nₐ | 6.02214076e23 |
| `boltzmann` / `kB` | k_B | 1.380649e-23 |
| `elementaryCharge` / `qe` | qₑ | 1.602176634e-19 C |
| `standardGravity` / `g0` | g₀ | 9.80665 m/s² |

Additional mathematical constants supported by the parser include `pi`, `e`, `tau`, `phi` / `goldenRatio`, `eulerGamma`, `sqrt2`, `ln2` and `ln10`.

The one-letter identifiers `c`, `G`, and `h` are also valid calculator memory-variable names in the existing memory model. Use the descriptive aliases above when the physical constant is intended, avoiding ambiguity with user memory.

## Resource impact

The feature reuses the existing expression tree and bitmap text renderer. It adds small static alias tables and string formatting during input/rendering; it does not allocate a second framebuffer, cache rendered glyphs, or add a background task. `std::string` temporaries follow the same allocation model already used by the editor and formatter.

## Validation

`tests/run_host_tests.sh` covers automatic `^`, `sqrt`, `cbrt`, indexed roots, Greek display aliases, the complete existing physical-constant set, mathematical constants, and the normal graphics/layout/window-manager regression suites. Host tests do not replace a Pico 2 W build or physical keyboard/LCD test.


## v1.5.2 — function-style roots and calculus aliases

The editor now accepts the Casio-like typed forms `sqrt(3)`, `cbrt(27)` and `rootN(x)` while still converting the root into a structured natural node. Ordinary function parentheses are accepted for trigonometric/logarithmic calls. The parser also accepts `integral(f(x),a,b)` and `derivative(f(x),x)` as numerical operations; these are text forms and are not yet full VPAM templates.
