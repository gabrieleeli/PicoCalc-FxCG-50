# PicoCalc FX AI — C++ firmware project (Pico 2 W / Pimoroni Pico Plus 2 W)

Version: **1.9.0** (see `VERSION`; the value is compiled into the firmware and
shown in *System*). Branch `version/v1.9.0`, built on `main` (= `version/v1.5.4`):
the separate `version/v1.6.0`–`version/v1.8.0` branches are not merged into it.

This is the first integrated C++ project scaffold built from the supplied `math_engine.py` and `funzioni.txt` requirements. It is **not yet a production-ready UF2**: the exact ST7365P panel init/flush and keyboard packet protocol must be validated against the PicoCalc mainboard/keyboard firmware revision, and the AI network client needs a secure HTTPS relay implementation.

## v1.9.0 — Pimoroni Pico Plus 2 W (PSRAM 8 MB + flash 16 MB)

Two boards are supported from the same sources (`FXAI_BOARD`):

| | Pico 2 W (`PICO2W`, default) | Pico Plus 2 W (`PICO_PLUS_2W`) |
|---|---|---|
| Memory | SRAM 520 KiB | SRAM 520 KiB + PSRAM 8 MiB (QMI CS1, GPIO47) |
| Flash | 4 MiB (FW 2 MiB + LittleFS ~2 MiB) | 16 MiB (FW 4 MiB + LittleFS ~12 MiB) |
| UF2 | `picocalc_fxai_pico2w.uf2` | `picocalc_fxai_pico_plus_2w.uf2` |

```sh
export PICO_SDK_PATH=/path/to/pico-sdk   # >= 2.1 (tested with 2.2.0)
./scripts/build.sh --board pico_plus_2w
./scripts/build.sh --board pico2w
```

New in 1.9.0: APS6404L PSRAM driver with boot self-test (SparkFun-derived
sequence), multi-pool allocator (SRAM heap + TLSF PSRAM pool + PicoCalc PSRAM
stub) with `std::pmr` adapters and pool-routed `operator new`, per-board and
memory-derived limits instead of fixed ones, "Memoria insufficiente" handling
with rollback, MSPLIM stack guards on both cores, LittleFS in the upper flash
with `flash_safe_execute()`, multi-function 2D graphs with sample/layer caches,
a 3D surface graph, and *System → Memoria / Limiti / Test diagnostici*.
The firmware now builds for both boards (the v1.5 sources never compiled for
ARM: C++ exceptions were disabled, `lwipopts.h` was missing and the LCD driver
had compile errors). Hardware validation is still pending; see
`docs/PICO_PLUS_2W.md` for details, memory map, limits and test status.

## Hardware target
- Raspberry Pi Pico 2 W (RP2350A, CYW43 Wi-Fi) **or** Pimoroni Pico Plus 2 W (RP2350B, RM2 Wi-Fi/BT, 8 MiB PSRAM, 16 MiB flash)
- ClockworkPi PicoCalc v2 mainboard
- 320x320 ST7365P LCD, driven by a dedicated PIO-SPI engine (GP10 SCK, GP11 MOSI; GP13 CS, GP14 DC, GP15 RESET)
- Keyboard controller over I2C1 (GP6 SDA, GP7 SCL, address 0x1F; 10 kHz bus)
- SD card interface reserved for settings, lists, graph files and history.

Check the exact mainboard revision and official schematic before wiring or changing pin definitions. The current `board_port.cpp` intentionally isolates hardware-specific panel and keyboard code rather than guessing an undocumented init sequence.

## Build
Install Raspberry Pi Pico SDK (>= 2.1), ARM GNU toolchain and CMake. Set `PICO_SDK_PATH`, then:

```sh
./scripts/build.sh --board pico2w          # -> build/pico2w/picocalc_fxai_pico2w.uf2
./scripts/build.sh --board pico_plus_2w    # -> build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2
# or by hand (one build directory per board):
cmake -S . -B build/pico2w -DFXAI_BOARD=PICO2W && cmake --build build/pico2w -j
```

`-DPICO_BOARD=pico2_w` alone still works (it selects `FXAI_BOARD=PICO2W`).
Options: `--opt Os|O2`, `--fb-psram`, `--bt`, `--lto`, `-DFXAI_ENABLE_3D/CAS/SD=ON|OFF`.
Flash with BOOTSEL + copy of the matching UF2 (images are not interchangeable
between boards). The board adapter compiles for both modules; the physical
LCD/keyboard/PSRAM behaviour still needs verification on a real PicoCalc.

## v1.5 — Window Manager (development)

The v1.5 increment introduces an allocation-free C++20 Window Manager for
centralized screen registration, active-window input/render dispatch, per-window
focus and a bounded navigation stack. Home, Run-Matrix, Graph, Cheatsheet and Unavailable
are registered while the existing `RunMatrixController` and Graphics Engine
remain in place. The launcher now mirrors the 20 Casio fx-CG50 mode entries shown in the supplied
reference image, with a separate AI Chat tile and Symbols/Cheatsheet utility tile.

Run the full native test suite with `./tests/run_host_tests.sh`. Build the RP2350
firmware with `PICO_SDK_PATH=/path/to/pico-sdk ./scripts/build_pico2w.sh`.
The new manager is host-tested; the RP2350 UF2 build and physical LCD/keyboard
acceptance are still pending. See `docs/WINDOW_MANAGER.md` and
`docs/V1_5_VALIDATION.md`.

## Graphics/UI increment 1.4.2

The firmware now includes a shared RGB565 canvas, bitmap font renderer, recursive
natural-math layout (fractions, radicals, powers, subscripts, groups and matrix
container rendering), a keyboard-navigable 4x5 Casio-style launcher with 20 colored mode tiles, a 2D Cartesian graph
renderer with trace/zoom, reusable panel/title/popup widgets, an animation
clock primitive, and dirty-rectangle tracking for partial LCD flushes.

The launcher currently opens RUN-MATRIX and GRAPH. The other Casio mode tiles and
AI Chat tile open explicit placeholders until their application controllers and
services are implemented; the visual presence of a tile does not imply feature parity. Matrix natural-node
editing/evaluation, complete Statistics/Matrix application input, integrated
screen transitions, and physical LCD validation remain on the roadmap.

Run native tests with:

```sh
./tests/run_host_tests.sh
```

## Current implemented math modules
- Angle-mode conversion (radians/degrees/gradians)
- Factorial, permutations and combinations
- Stable quadratic equation solver, including complex roots
- Gaussian elimination with partial pivoting for square linear systems
- Simpson numerical integration and bisection root finder
- Mean, variance, standard deviation, linear regression
- Vector addition, dot product and 3D cross product
- Scientific recursive-descent parser (`+ - * / % ^`, parentheses, implicit multiplication, x/y, pi/e, factorial, trig/hyperbolic and inverses, roots, logarithms, rounding, min/max, atan2/hypot, angle conversions)
- 2D function sampling and preliminary projected surface sampling for 3D visualization

The uploaded Python engine is preserved in `docs/math_engine_original.py`; its numerical features are being ported, not executed on the Pico.

## Roadmap / feature matrix
`docs/FEATURE_MATRIX.md` distinguishes implemented, scaffolded and planned functions. Do not treat the requested 2,000+ Casio operations as implemented until each operation has a test and UI entry. The source notes list requested features, not a verified complete Casio API catalogue.

## AI integration (ChatGPT + Claude)
The UI reserves separate ChatGPT and Claude panels. For a real deployment, use a small HTTPS relay on a trusted server: the Pico sends the prompt over TLS; the relay stores provider keys and calls the OpenAI or Anthropic API. Never hard-code provider keys in firmware or commit them to source control. `ai_client.cpp` is deliberately a safe placeholder until relay URL, TLS certificate handling, response limits and provider models are configured.

## Tests
`tests/run_host_tests.sh` runs the native math, graphics, font, layout, graph, launcher, widget, animation, window-manager, memory-pool, PSRAM self-test, limits, out-of-memory, LittleFS and system-menu suites without the Pico SDK (`FXAI_TEST_BOARD=PICO_PLUS_2W` selects the PSRAM profile, `FXAI_TEST_SANITIZE=1` enables ASan/UBSan). The same suites run through CTest: `cmake -S desktop -B build-desktop && cmake --build build-desktop && ctest --test-dir build-desktop`.

## License and references
This project contains original code and a preserved copy of the user's supplied Python file. Hardware details should be cross-checked with ClockworkPi's official PicoCalc documentation and schematic. No Casio firmware or proprietary code is included.

## Scientific calculator compatibility

The scientific engine now includes tested C++ building blocks for normalized fractions (basic operations), percent/reciprocal, nth roots, arbitrary-base logarithms, polar/rectangular conversion, sexagesimal conversion, base conversion, prime factorization, matrix add/multiply/determinant/inverse, vector norm, and the earlier equation/statistics functions. This is still **partial compatibility**, not a complete FX-991ES Plus or FX-991CW clone. See `docs/FEATURE_MATRIX.md` for exact status.

The launcher source now uses a compact CG50-inspired 4x5 icon grid designed around a 320x320 screen, with separate AI Chat and Symbols utility tiles. The LCD drawing primitives in `src/board_port.cpp` are still placeholders, and the keyboard protocol is not yet decoded; therefore this UI layout is not yet visible/usable on the physical PicoCalc until the board driver is completed and validated.


## Stato funzionale aggiornato

Il motore C++ ora comprende anche statistiche descrittive (mediana/quartili), regressione polinomiale fino al quarto grado, distribuzioni normale/binomiale/Poisson, derivate numeriche, SOLVE Newton, primitive per numeri complessi e un modello numerico Spreadsheet da 5 colonne x 45 righe. Il parser trigonometrico rispetta DEG/RAD/GRA.

**Limiti importanti:** non è ancora un firmware UF2 completo né ha parità totale con FX-991ES Plus/FX-991CW o FX-CG50. Il driver LCD ST7365P, la tastiera I2C/FIFO, l'interfaccia operativa, il vero grafico 3D z=f(x,y), il salvataggio SD, le modalità statistiche avanzate e la connessione AI sicura non sono ancora completati. Consultare `docs/FEATURE_MATRIX.md` e `docs/BUILD_STATUS.md`.


## Experimental v0.2 additions
- Expressions can use both `x` and `y`, enabling actual two-variable surface evaluation.
- `compose(f,g,x)` evaluates `f(g(x))`.
- `inverseValue(f,y,xMin,xMax)` numerically inverts a function on a selected bracket/branch; this is not symbolic inversion and requires a sign-changing bracket.
- `solvePolynomialInequality(a,b,c,relation)` returns intervals for linear and quadratic inequalities.
- 3D sampling evaluates `z=f(x,y)` and projects the result as points. It is not yet a full interactive 3D renderer.

See `docs/BUILD_STATUS.md` for verified and missing functionality, and `docs/CHANGELOG.md` for this increment.


## v0.3 calculator UI increment
The default application view is now a compact 320x320 calculator workspace with expression input area, result panel, history region and function-key grid. The screen is rendered through the board abstraction. **The physical LCD is not yet operational** because `board_port.cpp` still has stub drawing primitives, and keyboard events are not decoded. The scientific parser and its new operations are covered by native host tests; this is a meaningful software increment, not a claim of complete FX-991CW parity.


## v0.4 — memoria, editor e statistica inferenziale

Il core include ora memoria A-Z, Ans, PreAns e M indipendente, con valutazione di espressioni che richiamano le variabili. Il formatter della notazione naturale converte radici, π, operatori e potenze in simboli Unicode. Sono disponibili modelli dati per quattro matrici fino a 4×4 e sei liste statistiche, con schermate statiche di editor. Le statistiche includono ANOVA a una via, test t a un campione, appaiato e a due campioni (Welch o varianze uguali) e intervallo t per la media.

Questa release non equivale ancora a una FX-991CW completa: il display naturale non impagina ancora frazioni e strutture matematiche in 2D; gli editor non ricevono ancora input dalla tastiera; la memoria non è persistente; restano da implementare vari test e distribuzioni. Il driver LCD/tastiera non è ancora validato su hardware e non è incluso un UF2 verificato.

## v0.5 — equazioni, disequazioni e sistemi
- Equazioni di primo grado (`solveLinearEquation`) e di secondo grado (`solveQuadratic`).
- Disequazioni di primo e secondo grado (`solvePolynomialInequality`).
- Sistemi quadrati di equazioni lineari (`solveLinearSystem`).
- Sistemi di disequazioni in una variabile (`solveInequalitySystem`).
- Unione e intersezione di insiemi soluzione (`unionSolutionSets`, `intersectSolutionSets`).

Queste sono API del motore matematico: menu, inserimento da tastiera e schermate dedicate non sono ancora collegati.


## v0.8 integration preparation
- Corrected the keyboard I2C bus speed to 10 kHz and implemented the register-pointer/read transaction for `REG_ID_FIF` (`0x09`). `readKey()` now returns the raw event as `state << 8 | keycode`; application-level key mapping is still pending.
- Removed the incorrect assumption that the LCD is driven by hardware SPI1. PicoCalc uses a dedicated PIO-SPI display path; the ST7365P command sequence, PIO program, DMA flush and framebuffer are still pending.
- Added `tests/run_host_tests.sh`; the complete native math smoke suite passes.

This increment is not a flashable or hardware-validated firmware. The official PicoCalc repository and its v2.0 schematic remain the hardware reference: https://github.com/clockworkpi/PicoCalc.


## v1.0 — riferimento fx-CG50 e calibrazione display
La nuova risorsa `casio-cg50` è stata aggiunta come riferimento tecnico esterno per
la struttura dei menu, il comportamento dei tasti e la validazione visiva. Non viene
incorporata come emulatore nel PicoCalc: emula un SoC SH7305/SH-4A e periferiche Casio,
mentre il PicoCalc usa RP2350, tastiera I2C e LCD ST7365P. Per dettagli e vincoli vedere
`docs/REFERENCE_CASIO_CG50_EMULATOR.md`.

È stata aggiunta `ui::drawDisplayCalibration()`; abilitarla temporaneamente configurando CMake con
`-DFXAI_DISPLAY_CALIBRATION_SCREEN=ON`
per controllare colori, orientamento e area visibile sul pannello reale.

## Obiettivo di parità fx-CG50

La v1.0 **non** implementa tutto ciò che può fare la Casio fx-CG50. Per evitare di confondere algoritmi isolati con applicazioni complete, consultare `docs/CG50_PARITY_ROADMAP.md`: contiene la matrice delle aree ufficiali, lo stato effettivo, i vincoli hardware e i criteri necessari per dichiarare una funzione completata.


## v1.2 — Point A math backend increment

The new `include/point_a.hpp` / `src/point_a.cpp` module adds a tested Run-Matrix session, extended matrix/vector operations, rational fractions, complex functions, signed Base-N and bitwise operations, expression-based numerical calculus/SOLVE, unit conversions, SI prefixes and physical constants. Build/test integration is included in the Pico SDK and desktop CMake targets.

Run native tests with `./tests/run_host_tests.sh`. A passing host suite does not prove the firmware builds for RP2350 or that the calculator is FX-CG50 complete. The full Natural V.P.A.M. 2D editor, complete UI workflows, complex parser integration, full catalogs and physical-device verification remain open; see `docs/POINT_A_COMPLETION.md`.

## v1.4 — Run-Matrix interattivo (host-tested)

La versione 1.4 collega il decoder FIFO della tastiera PicoCalc al workspace Run-Matrix e all'editor Natural V.P.A.M. F1 apre Run-Matrix dal launcher; nell'editor F1 inserisce una frazione, F2 un radicale, F3 una potenza, F4 un pedice, F5 un gruppo e F6 pulisce l'espressione. Le frecce navigano la struttura, Backspace/Delete modificano, Enter valuta e Esc torna al launcher. I risultati passano attraverso `pointa::RunMatrixSession`.

Test host:

```sh
./tests/run_host_tests.sh
```

Il decoder usa il protocollo FIFO definito dal firmware ClockworkPi. Il test hardware sul PicoCalc e la build UF2 non sono ancora stati eseguiti; vedere `docs/PICOCALC_KEYBOARD_RUN_MATRIX.md` e `docs/BUILD_STATUS.md` per limiti e mappatura.


## Stato grafico corrente — 2026-11-07

L'audit della v1.4 e il primo incremento del Graphics Engine sono documentati in
`docs/BUILD_STATUS.md` e `docs/GRAPHICS_ENGINE.md`. Il motore `gfx::Canvas` lavora
sul framebuffer RGB565 esistente senza allocare o duplicare memoria; fornisce
primitive geometriche, clipping, bitmap/layer e blending. Le API `board::*`
precedenti sono mantenute.

Il font bitmap 5×7 condiviso supporta scale 1×–3× e, dalla v1.5.1-dev, l’intero alfabeto greco in approssimazione bitmap compatta, oltre a π, radici, ×, ÷, ∫, ∞, ≤ e ≥. La tipografia Unicode proporzionale non è presente. La UI applicativa resta incompleta: il launcher non ha ancora focus/selezione,
`main.cpp` collega Home e Run-Matrix, gli altri editor non sono tutti raggiungibili,
e il renderer naturale non impagina ancora formule 2D in modo completo. Il driver
LCD è implementato nel sorgente ma non è stato verificato su hardware. Non
confondere quindi i test host del motore grafico con una verifica del display o
con un firmware UF2 pronto all'uso.


## Graphics roadmap — phases 5 and 6

- **Phase 5 (Math Layout):** bounded recursive natural-expression measurement, viewport clipping, horizontal alignment and defensive handling of incomplete nodes. See `docs/GRAPHICS_PHASE5_MATH_LAYOUT.md`.
- **Phase 6 (Launcher):** 4x5 navigation for the 20 Casio fx-CG50 modes in the supplied reference, plus AI Chat and Symbols utility tiles. RUN-MATRIX and GRAPH are connected; other unimplemented modes display explicit placeholders. See `docs/LAUNCHER_PHASE6.md`.

Both phases have focused host tests. Full host-suite completion and Pico 2 W / physical LCD validation remain separate verification items.


## Symbol Cheatsheet app (v1.5.2-dev)

The launcher adds an offline **SYMBOLS / CHEATSHEET** app. Its English reference
contains 15 pages covering arithmetic, powers/roots, both Greek alphabets,
mathematical and physical constants (including ln2, ln10 and g0), functions, calculus, logic, statistics,
graphing, complex/matrix/vector notation, units and common symbols. Use Left/
Right to change category, Up/Down to select an entry, and ESC to return. The
selected entry includes a short usage example. It is a notation reference:
not every item is currently accepted by the expression parser. The data is
compile-time constant and adds no framebuffer, network or background task.
See `docs/SYMBOL_CHEATSHEET.md`.

## Input matematico automatico (v1.5.1-dev)

Nel Run-Matrix puoi digitare `sqrt` o `cbrt` per aprire direttamente una radice, `root5` (fino a `root99`) per una radice di indice specifico e `^` per entrare nell'esponente in apice. Scrivendo `alpha`, `Beta`, `omega` e gli altri nomi greci ottieni i rispettivi simboli. Le costanti fisiche esistenti sono disponibili anche con alias descrittivi come `speedOfLight`, `avogadro`, `boltzmann`, `elementaryCharge` e `standardGravity`. F1–F6 continuano a funzionare come scorciatoie. Dettagli e limiti: `docs/AUTO_SYMBOL_INPUT.md`.
