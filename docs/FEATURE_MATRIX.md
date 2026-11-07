# Feature matrix (truthful implementation status)

Legend: **Core tested** = algorithm implemented and covered by native C++ smoke tests; **Partial** = algorithm exists but not complete calculator-mode integration; **Prototype** = visual/algorithm prototype only; **Not implemented** = no usable implementation yet.

| Area | Current capability | Status |
|---|---|---|
| Arithmetic / scientific | Existing parser plus RunMatrixSession (Ans/PreAns, A-Z and 64-entry history), SI prefix/constants/unit-conversion APIs | Core tested backend subset; Natural V.P.A.M. 2D editor and full catalog remain partial |
| Combinatorics | factorial, nPr, nCr | Core (overflow edge cases still need hardening) |
| Fractions | normalized rational representation, add/subtract/multiply/divide, decimal approximation, improper/mixed formatting | Core tested API; Natural V.P.A.M. 2D editor and exact expression algebra still partial |
| Angle and coordinates | DEG/RAD/GRA conversion, polar/rectangular coordinates, decimal/sexagesimal | Core |
| Number systems | signed conversion bases 2..36, prime factorization, AND/OR/XOR/NOT and fixed-width shifts | Core tested APIs; interactive Base-N mode not integrated |
| Equations | stable quadratic roots including complex roots; Gaussian linear systems; bisection and Newton SOLVE callbacks | Core algorithms; general equation editor, quartic roots and inequality solver not implemented |
| Complex | arithmetic, power, square root, exp/log, sin/cos/tan, argument and formatter | Core tested APIs; parser and complex-mode UI integration still partial |
| Matrices / vectors | add/subtract/multiply/scale/transpose/trace/determinant/inverse/RREF/solve; vector add/subtract/scale/dot/cross/norm/angle | Core tested APIs; MatrixEditor remains 4x4 and full application UI is not implemented |
| Calculus | expression-bound 5-point numerical derivative, Simpson integral, bracket scan for roots and Newton SOLVE | Core tested APIs; root tangencies, robust bracketing and UI still partial |
| Statistics | mean, variance, standard deviation, median, quartiles, descriptive summary, linear and polynomial regression degrees 1..4 | Core algorithms; weighted frequencies and all regression families not complete |
| Probability | normal PDF/CDF/inverse CDF; binomial and Poisson PMF/CDF | Core algorithms; t, chi-square, F distributions, tests, intervals and ANOVA not implemented |
| Spreadsheet | 5 columns x 45 rows, numeric cell storage, sum and average | Core data model; formula parser, cell references, editing UI and persistence not implemented |
| Tables / graphing | 2D sampling and 3D projection prototype | Prototype; no complete CG50 graph suite; 3D surface evaluation is not yet full z=f(x,y) |
| UI / Graphics Engine | 320x320 RGB565 canvas; pixel/line/rect/rounded rect/circle/arc/polygon/bitmap/layer/blend/clip; shared 5×7 bitmap font at 1×–3× and selected math glyphs | Graphics/font primitives host-tested; full Greek set, proportional fonts, math layout, launcher focus, multi-app routing and physical LCD validation remain partial |
| LCD hardware | ST7365P-compatible init sequence, RGB565 framebuffer, PIO-SPI transfer and flush code present in `board_port.cpp` | Driver source implemented; panel timing/orientation/color and physical operation not validated; DMA path not claimed |
| Keyboard | I2C1 at 10 kHz; register 0x09 FIFO read; host-tested decoder for state/keycode, arrows, editing keys, Enter/Esc and F1–F6 | Application mapping and event flow implemented; physical keyboard test pending |
| AI | ChatGPT / Claude relay placeholder | Not implemented; no API network requests |
| SD / persistence | — | Not implemented |

The new v0.3 parser expands scientific expression coverage, but this is not full parity with FX-991CW. The FX-991CW's official feature list includes spreadsheet, equation/inequality, distribution and Math Box modes; only the specifically listed core algorithms above are implemented here. The PicoCalc firmware is not yet a flashable, hardware-validated UF2.

## v0.4 — nuovi moduli
| Funzione | Stato |
|---|---|
| Memoria A-Z, Ans, PreAns, M | Implementata nel core; non persistente |
| Richiamo variabili nelle espressioni | Implementato |
| Formatter matematica naturale Unicode | Parziale; non impaginazione 2D tipografica |
| Editor matrici MatA–MatD, fino a 4×4 | Modello dati implementato; UI statica, input da tastiera da integrare |
| Editor liste statistiche List1–List6 | Modello dati implementato; UI statica, input da tastiera da integrare |
| Statistiche descrittive e regressione lineare da liste | Implementate |
| ANOVA a una via | Implementata |
| Test t a un campione, appaiato e due campioni (Welch/pooled) | Implementati |
| Intervallo di confidenza t per una media | Implementato |
| Test t indipendente, Z, χ², F e intervalli Z | Da implementare |

## Algebra (v0.5)
| Funzione | Stato motore | Interfaccia/tastiera |
|---|---|---|
| Equazioni lineari | Implementata | Da collegare |
| Equazioni quadratiche | Implementata | Da collegare |
| Disequazioni lineari e quadratiche | Implementata | Da collegare |
| Sistemi quadrati di equazioni lineari | Implementati | Da collegare |
| Sistemi di disequazioni in una variabile | Implementati | Da collegare |
| Unione/intersezione di insiemi soluzione | Implementate | Da collegare |

## Compatibilità app fx-991CW — sintesi v0.9
Le 13 app ufficiali sono: Calculate, Statistics, Distribution, Spreadsheet, Table, Equation, Inequality, Complex, Base-N, Matrix, Vector, Ratio e Math Box. Nel progetto esistono primitive o algoritmi iniziali per 11 aree; Ratio e Math Box non hanno ancora un'implementazione utilizzabile. Nessuna delle 13 è ancora completa come flusso applicativo con tastiera, menu, input, output e test di compatibilità.

Le lacune principali sono: renderer Natural Textbook Display 2D; memoria persistente, undo/replay; conversioni metriche e costanti scientifiche; rapporti; casualità e sommatoria; modalità complesse completa; operatori logici Base-N; foglio di calcolo con formule/editor; app Table completa; solver generale, polinomi fino al 4° grado e disequazioni fino al 4° grado; regressioni e distribuzioni statistiche complete; Math Box; editor e navigazione hardware.

## v1.2 — Punto A backend increment

The point-A numerical backend is expanded and host-tested. See `POINT_A_COMPLETION.md` for API details and the exact remaining work. Do not mark Run-Matrix parity complete: the 2D natural editor, UI flows, parser-complex integration, full catalogs and device tests remain open.

## v1.3 — Natural V.P.A.M. editor increment
| Funzione | Stato verificato |
|---|---|
| Albero strutturato righe/token/frazioni/radicali/potenze/indici/gruppi | Implementato nel modello host |
| Navigazione logica cursore sinistra/destra/su/giù e cancellazione | API implementate e testate host; non collegate ai codici fisici I2C |
| Layout 2D con frazioni, radicali, apici/indici e cursore | Primitive di layout implementate e testate; font/posizionamento hardware non validati |
| Serializzazione in espressione per frazioni/radicali/potenze/gruppi | Implementata e valutata nei test host tramite parser esistente |
| Variabili con indice (es. x₁) | Rendering/modello parziale; non valutabili finché il parser non supporta identificatori indicizzati |
| UI drawNaturalEditor | Collegamento alle primitive `board::*` compilato isolatamente; non integrato nel flusso principale |

Non segnare Natural V.P.A.M. come completo: tastiera fisica, scroll/wrap, selezione, undo/redo, tipografia matematica completa e test PicoCalc restano aperti.


## v1.4 — keyboard + Run-Matrix integration
| Funzione | Stato verificato |
|---|---|
| Decoder FIFO STM32 PicoCalc | Host-tested: state nel byte alto, keycode nel byte basso; press/hold processati, release ignorato |
| Mappatura tasti Run-Matrix | Host-tested: frecce, Backspace/Delete, Enter, Esc, ASCII, F1–F6 |
| Flusso launcher/Run-Matrix | Controller host-tested: apertura, modifica, valutazione, risultato/errore, ritorno |
| Input Natural V.P.A.M. | Host-tested: frazione `1/3`, `sqrt(9)`, `2^3` e gruppo `(2+3)` inseriti da tasti funzione e valutati; pedice visualizzabile ma parser non supportato |
| Tastiera/display/Pico 2 W fisici | Non verificati |

Il dettaglio dei codici e delle limitazioni è in `PICOCALC_KEYBOARD_RUN_MATRIX.md`. Non segnare la parità Run-Matrix come completa: scrolling, undo/redo, selezione/clipboard, modificatori, catalogo comandi e prove hardware restano da fare.


## 2026-11-07 — Graphics Engine increment
| Funzione | Stato verificato |
|---|---|
| Canvas RGB565 senza allocazioni | Implementato; framebuffer fornito dal driver |
| Primitive geometriche e clipping | Implementate e testate host |
| Bitmap, livelli statici e chiave trasparente | Implementati e testati host |
| Alpha blending RGB565 | Implementato e testato host |
| Font condivisi e simboli matematici | Da implementare nella fase font |
| Dirty rectangles / DMA | Da implementare dopo verifica del pannello |
| Launcher interattivo e router multi-app | Parziale; Home e Run-Matrix sono i soli flussi integrati nel main corrente |


| Font renderer | Stato verificato |
|---|---|
| ASCII bitmap 5×7, scala 1×/2×/3×, newline, misurazione e clipping | Implementato e testato host |
| π, √, ×, ÷, Σ, θ, ∫, ∞, ≤, ≥ | Glifi dedicati implementati e testati nel renderer |
| Greco completo, font proporzionali e stili matematici | Da implementare |

## Graphics/UI status at v1.4.2

| Component | Status | Notes |
|---|---|---|
| RGB565 canvas and primitives | Implemented | Host-tested; shared framebuffer |
| Bitmap font renderer | Implemented | Compact 5x7 font with selected math glyphs |
| Natural math layout | Partial | Fractions, radicals, powers, subscripts, groups; Matrix node display only |
| Launcher navigation | Partial | 3x3 selection; RUN-MATRIX and GRAPH open |
| Shared widgets | Foundation | Panel, title bar, popup, selection primitives |
| Graph renderer | Partial | Cartesian function sampling, grid, axes, trace, zoom |
| Partial LCD flush | Implemented in source | Not physically verified on the panel |
| Animation timeline | Foundation | Non-blocking timing primitive; screen transitions not yet wired |
| Matrix/Statistics app keyboard workflows | Not complete | Existing data editor APIs remain; UI controllers are next |
| Pico 2 W UF2 | Not verified | Pico SDK path/toolchain absent in build environment |
