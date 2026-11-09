# Changelog

## 1.9.0 — Pimoroni Pico Plus 2 W (PSRAM 8 MB + flash 16 MB), memoria multi-pool

Base: `main` / `version/v1.5.4` (scelta esplicita). Le modifiche dei branch
`version/v1.6.0`–`version/v1.8.0` non sono incluse. Branch: `version/v1.9.0`.

- **Versione**: `VERSION` era fermo a `1.5.3-dev`; ora `1.9.0` ed è l'unica
  sorgente della versione (CMake → `FXAI_VERSION` → firmware, schermata System,
  `pico_set_program_version`).
- **Build reale**: il firmware v1.5 non era mai stato compilato per ARM (eccezioni
  C++ disabilitate dall'SDK, `lwipopts.h` mancante, errori nel driver LCD:
  `constexpr pio0`, `pio_sm_is_tx_stalled` inesistente, `glyph()` non dichiarata).
  Ora compila senza warning per entrambe le schede con Pico SDK 2.2.0.
- **Selezione scheda**: `FXAI_BOARD=PICO2W|PICO_PLUS_2W` prima dell'import
  dell'SDK; definizioni `FXAI_BOARD_*`, `FXAI_HAS_PSRAM`, `FXAI_FLASH_SIZE`;
  `scripts/build.sh --board ...` con directory e UF2 separati
  (`build_pico2w.sh` resta come alias); `include/board_config.hpp` con pin
  PicoCalc invariati e capacità dal board header SDK (ADC su GPIO40–47 sul
  RP2350B documentato).
- **Driver PSRAM APS6404L** (QMI CS1, GPIO47 dal board header) portato da
  SparkFun `sfe_psram.c` (MIT): ID, reset, QPI, timing M1 da `clk_sys`,
  `0xEB/0x38`, `WRITABLE_M1`; self-test data/address bus, pattern casuali,
  percorso cached/non cached; in caso di errore PSRAM disabilitata e avvio
  solo in SRAM; riapplicazione del timing dopo cambi di `clk_sys`.
- **Memoria multi-pool**: `mem::alloc/tryAlloc/free/stats`, pool SRAM (heap),
  PSRAM_BOARD (TLSF), PSRAM_PICOCALC (stub); policy PREFER_PSRAM/SRAM_ONLY/
  PSRAM_ONLY/PREFER_SRAM; adattatori `std::pmr`; `operator new` globale
  instradato per soglia o `mem::PlacementScope`; thread-safe su due core.
- **Carico in PSRAM**: cronologia, matrici, liste, foglio di calcolo, campioni e
  layer dei grafici, mesh/z-buffer 3D, snapshot finestre, buffer AI.
- **Limiti dinamici** (`include/limits.hpp`): window manager (8/8 → dinamico
  16/64), dirty regions, poligoni (32 → 256/4096), matrici (4×4 → 40/999),
  liste (6×256 → 26×999/1M), foglio (45×5 → 999×26 sparso), cronologia
  (30 → 30/10 000), funzioni Y= (1 → 20/64), 3D (pittore/z-buffer), buffer AI;
  limiti strutturali mantenuti e aggiunti (profondità parser prima illimitata).
- **"Memoria insufficiente"** con rollback e senza abort (anche il messaggio è
  scritto senza allocare); `PICO_MALLOC_PANIC=0`.
- **Stack**: stack applicativo core 0 da 32 KiB in SRAM (prima 2 KiB in
  SCRATCH_Y) con MSPLIM; core 1 8 KiB con guardia SDK; handler HardFault in
  SRAM con stack d'emergenza e report al riavvio.
- **Flash**: partizioni da `PICO_FLASH_SIZE_BYTES`, LittleFS v2.11.3,
  scritture solo via `flash_safe_execute()` con buffer SRAM, verifica/ripristino
  di QMI M1 nella zona sicura, test "PSRAM dopo scrittura flash"; `-Os/-O2`,
  gc-sections, LTO sperimentale (solo sorgenti app), controllo dimensione.
- **Grafica**: grafici 2D multi-funzione con cache campioni/layer e
  sovracampionamento; grafico 3D (z-buffer o pittore); framebuffer in SRAM di
  default con `FXAI_FRAMEBUFFER_IN_PSRAM`; benchmark `present()` on-device.
- **System**: schermate Memoria (SRAM/PSRAM/Flash, picchi, stack), Limiti e
  Test diagnostici (PSRAM, allocatore 1/2 core, flash+PSRAM, framebuffer,
  Wi-Fi con scansione, LittleFS, SD).
- **Periferiche**: `wifiConnect()` attiva la modalità STA; probe SD
  (`FXAI_ENABLE_SD`); Bluetooth LE opzionale (`FXAI_ENABLE_BT`, default OFF).
- **Test**: suite host ristrutturata (libreria comune) + 8 nuove suite; CTest
  in `desktop/CMakeLists.txt` (emulatore SDL2 opzionale). Nessun test su
  hardware reale eseguito: vedi `docs/PICO_PLUS_2W.md` §11.

## v1.5.2-dev — Casio-style 20-mode launcher
- Sostituita la griglia generica 3x3 con una griglia 4x5 che riproduce le 20 modalità visibili nell'immagine di riferimento FX-CG50: Run-Matrix, Statistics, eActivity, Spreadsheet, Graph, Dyna Graph, Table, Recursion, Conic Graphs, Equation, Program, Financial, E-CON4, Link, Memory, System, Geometry, Picture Plot, 3D Graph e Conversion.
- Ogni modalità ha un riquadro dedicato con colore, pittogramma compatto, etichetta e badge del tasto (1–9, A–K), ispirati alle icone originali.
- Aggiunti due riquadri separati per AI Chat e Symbols/Cheatsheet, mantenendo le funzioni specifiche del progetto.
- Aggiornata la navigazione a 4 colonne e 5 righe; RUN-MATRIX, GRAPH e Cheatsheet restano collegati. Le altre modalità aprono una schermata esplicita di modulo non ancora implementato.
- Aggiornati i test del launcher; suite host completa superata. Rendering verificato con compilazione host di `src/ui.cpp`; test su LCD fisico e build UF2 restano da eseguire.

## v1.5.2-dev — Symbol Cheatsheet app
- Aggiunta al launcher una decima app, SYMBOLS / CHEATSHEET, mantenendo le nove voci esistenti.
- Implementata una guida offline in inglese con 15 pagine: aritmetica, potenze/radici, alfabeti greci minuscolo/maiuscolo, costanti matematiche/fisiche (inclusi ln2, ln10 e g0), funzioni, calcolo, insiemi/logica, statistica, grafici, complessi/matrici/vettori, unità e notazione.
- Navigazione con frecce: sinistra/destra cambia categoria, su/giù seleziona la voce, ESC torna al launcher.
- Il pannello inferiore mostra un esempio/nota per la voce selezionata.
- Tabelle `constexpr`, nessuna allocazione dinamica, rete o framebuffer aggiuntivo.
- Aggiunti test host per contenuti, navigazione e apertura/chiusura dell'app. Il supporto del parser è distinto esplicitamente dalla notazione descritta.

## v1.5.1-dev — input matematico automatico
- `^` crea automaticamente una struttura Power nell'editor naturale: l'esponente viene scritto in apice senza premere F3.
- `sqrt` e `cbrt` diventano modelli di radice quadrata/cubica; `root3`–`root99` inseriscono radici di indice arbitrario senza F2.
- Aggiunta la conversione di tutte le 24 lettere greche minuscole e maiuscole dai nomi ASCII alla notazione grafica compatta.
- Esteso il formatter con costanti fisiche del progetto (`c`, `G`, `h`, `Nₐ`, `k_B`, `qₑ`, `g₀`) e costanti matematiche (`τ`, `φ`, costante di Eulero-Mascheroni, `√2`, `ln 2`, `ln 10`).
- Il parser valuta gli alias descrittivi delle costanti fisiche, evitando collisioni con le variabili di memoria a singola lettera.
- La conversione è locale, senza rete, task in background o framebuffer aggiuntivo. Le scorciatoie F1–F6 restano disponibili.
- Aggiunti test automatici per input, rendering greco, potenze, radici di indice 3–99 e costanti; suite host completa superata.
- Limite noto: le lettere greche sono simboli visuali e non variabili numeriche assegnabili; il font 5×7 usa approssimazioni bitmap compatte.


## v0.3
- Esteso il parser scientifico con fattoriale postfix, moltiplicazione implicita, modulo, funzioni numeriche, funzioni a due argomenti e conversioni angolari.
- Aggiunti test automatici per le nuove funzioni del parser.
- Aggiunta schermata calcolatrice compatta per il display 320x320: input, risultato, messaggio di stato, cronologia e griglia funzioni.
- Impostata la vista calcolatrice come schermata iniziale.
- Aggiornato lo stato del progetto per distinguere UI applicativa da driver LCD/tastiera ancora non operativi.

## v0.2
- Aggiunte valutazione di espressioni in x/y, composizione, inversa numerica, disequazioni quadratiche e campionamento 3D corretto.
- Corretto l'ordine di precedenza tra segno unario ed esponente (`-2^2 = -4`, `2^-2 = 0.25`).

## v0.4 — memoria, editor e statistiche
- Aggiunto `CalculatorMemory`: 26 variabili A-Z, Ans, PreAns e memoria indipendente M; supporto alla lettura delle variabili direttamente nelle espressioni.
- Aggiunto formatter per notazione matematica leggibile: radice `√`, π, moltiplicazione `×`, divisione `÷` e potenze con apici Unicode.
- Aggiunto `MatrixEditor` con quattro slot MatA–MatD, dimensioni fino a 4×4, modifica/lettura celle e accesso alle matrici per le operazioni esistenti.
- Aggiunto `StatisticsEditor` con sei liste, inserimento/cancellazione, statistiche descrittive e regressione lineare.
- Aggiunti ANOVA a una via, test t a un campione, test t appaiato e a due campioni (Welch o varianze uguali) e intervallo t per la media, con beta incompleta regolarizzata e quantile t numerico.
- Aggiunti disegni UI dedicati per editor matrici e liste statistiche.
- Test host estesi per memoria, formatter, editor, ANOVA, test t e intervalli di confidenza.

## v0.5 (solver algebraico)
- Aggiunta API esplicita `solveLinearEquation(a,b)` per equazioni di primo grado.
- Confermato il supporto alle equazioni di secondo grado tramite `solveQuadratic` (che gestisce anche i casi degeneri lineari, identità e impossibilità).
- Confermato il supporto alle disequazioni di primo e secondo grado tramite `solvePolynomialInequality`.
- Aggiunte unione e intersezione di insiemi soluzione rappresentati da intervalli.
- Aggiunto `solveInequalitySystem` per risolvere sistemi di disequazioni in una variabile, intersecando i singoli insiemi soluzione.
- `solveLinearSystem` risolve sistemi quadrati di equazioni lineari; non è un risolutore generale di sistemi non lineari.
- Aggiunti test automatici per equazione lineare, sistema 2x2, unione, intersezione e sistema di disequazioni.


## v1.0 — riferimento emulatore fx-CG50 e calibrazione LCD
- Registrato l'emulatore indipendente Casio fx-CG50 come riferimento esterno per menu, tasti e comportamento UI.
- Documentata la separazione tra SH7305/SH-4A + LCD R61524 dell'emulatore e RP2350 + ST7365P del PicoCalc.
- Aggiunta schermata di calibrazione LCD con barre RGB565, griglia e marcatori angolari, attivabile a compile-time.
- Nessun firmware Casio, dump ROM o asset proprietario importato.

## v1.1 — specifica verificabile di parità fx-CG50
- Aggiunta una matrice completa delle famiglie di funzionalità pubblicate da Casio per la fx-CG50.
- Distinti algoritmi host, applicazioni utilizzabili, integrazione hardware e funzioni dipendenti da periferiche esterne.
- Documentati i limiti dell'emulatore SH-4A come riferimento e il divieto tecnico di considerarlo un firmware RP2350 pronto all'uso.
- Definiti criteri di test per evitare dichiarazioni di parità non verificate.
- Suite host v1.0 eseguita: PASS. Nessuna nuova funzione matematica o prova hardware è dichiarata in questa release.

## v1.2 — Punto A: ampliamento del core matematico

- Aggiunto `point_a.hpp/.cpp` con sessione Run-Matrix, operazioni avanzate per matrici/vettori, frazioni, complessi, Base-N, calcolo numerico, conversioni, prefissi SI e costanti.
- Integrato il nuovo modulo nei target Pico SDK e desktop e nella suite host.
- Rafforzata la gestione overflow delle frazioni esistenti.
- Estesi i test automatici; suite host: PASS.
- La UI Natural V.P.A.M. 2D, l'integrazione completa delle modalità e la verifica sul PicoCalc restano esplicitamente aperte.

## v1.3 — Natural V.P.A.M. editor (incremento)
- Aggiunto modello ad albero editabile per righe, frazioni, radicali, potenze, indici e gruppi.
- Aggiunti eventi logici tastiera, navigazione gerarchica, cancellazione e cursore 2D.
- Aggiunta serializzazione per il parser esistente e layout con linee di frazione/radicale.
- Aggiunta `ui::drawNaturalEditor()` e integrazione nei target firmware/desktop/test.
- Test host, ASan/UBSan e compilazione isolata UI superati.
- Mappatura I2C fisica, workspace nel main, scrolling, undo/redo e test UF2/hardware restano aperti.

## v1.4 — integrazione tastiera e Run-Matrix

- Aggiunto decoder host-testable per il FIFO tastiera PicoCalc (state nel byte alto, keycode nel byte basso), basato sulle definizioni del firmware ClockworkPi.
- Collegati F1–F6, frecce, Backspace, Delete, Enter, Esc e caratteri ASCII al controller applicativo.
- Aggiunto `RunMatrixController`: launcher → Run-Matrix → editor Natural V.P.A.M. → `RunMatrixSession` → risultato/errore.
- Il `main.cpp` ora esegue il loop di input e aggiorna la schermata solo quando arriva un evento valido.
- Aggiunta schermata Run-Matrix e test host del decoder e del flusso end-to-end.
- La mappatura applicativa F1–F6 è documentata; la verifica su hardware e la build UF2 restano da fare.

## 2026-11-07 — Fase 1 audit + Fase 2 Graphics Engine

- Eseguito inventario di firmware, desktop, header, test e roadmap della release v1.4.
- Aggiunto canvas RGB565 riutilizzabile e senza allocazioni dinamiche; collegato al framebuffer esistente del driver.
- Aggiunte primitive geometriche, clipping, bitmap/layer e blending RGB565.
- Mantenute le API legacy `board::clear`, `board::pixel` e `board::rect`.
- Aggiunti test host grafici e aggiornati BUILD_STATUS e documentazione driver.
- Host test matematici e grafici superati. Build UF2 e display fisico non verificati.

## 2026-11-07 — Fase 3: font bitmap condiviso (incremento iniziale)

- Estratto il font 5×7 dal driver e creato `font_renderer` indipendente dall'hardware.
- Aggiunte misurazione, newline, scala bitmap 1×/2×/3× e clipping.
- Aggiunti glifi dedicati per simboli matematici selezionati e test host/sanitizer.
- `board::text()` usa il renderer condiviso; il Math Layout Engine 2D completo resta da implementare.

## 1.4.2 — UI integration increment

- Added recursive math layout module and wired it into natural editor screens.
- Added 3x3 keyboard-navigable launcher and connected RUN-MATRIX and GRAPH.
- Added Cartesian graph renderer with expression sampling, zoom and trace.
- Added reusable panel/title/popup/selection widgets and non-blocking animation
  timeline primitive.
- Added Canvas dirty bounds and partial LCD flush path.
- Added host tests for layout, graph, launcher, widgets, animation and dirty
  rectangles. Physical hardware and Pico SDK build remain unverified.


## 1.5.0-dev — Window Manager increment

- Added a fixed-capacity, allocation-free C++20 Window Manager.
- Centralized window registration, active screen selection, event dispatch and draw dispatch.
- Added per-window focus state and a bounded open/close navigation stack.
- Registered Home, Run-Matrix, Graph and Unavailable in the firmware main loop.
- Preserved the existing RunMatrixController and Graphics Engine interfaces.
- Added focused host tests and a Pico 2 W build script / hardware validation checklist.
- Host regression suite passes; RP2350 build and physical hardware validation remain pending.

## 1.5.2-dev — Run-Matrix fase 1: storico visibile

- Collegata la cronologia reale della `RunMatrixSession` alla schermata Run-Matrix.
- La UI mostra gli ultimi due calcoli precedenti con espressione in notazione naturale e risultato; il calcolo corrente resta nel pannello Risultato.
- Ridisposti pannelli e messaggi per il display 320×320, senza cambiare il motore di valutazione.
- Eseguita l'intera suite host: PASS.
- Questa è una prima tranche, non una dichiarazione di parità con fx-CG50. Replay con frecce, undo/redo, clipboard, menu/catalogo, formati e verifica fisica restano da implementare.

## Run-Matrix phase 2 — input testuale naturale e guida italiana

- Added function-style keyboard entry for `sqrt(3)`, `root3(27)`, ordinary function calls and parenthesized/negative exponents.
- Added parser aliases `rootN(x)`, numerical `integral(f(x),a,b)` and `derivative(f(x),x)`.
- Added natural display aliases for integral and derivative notation.
- Matched Run-Matrix history retention to the fx-CG50 manual's 30 calculation entries.
- Expanded the symbol guide to 16 Italian pages with short definitions and examples.
- Host test suite passes; PicoCalc firmware/hardware validation remains outstanding.

## 1.5.2-dev — Run-Matrix fase 3: modelli VPAM di calcolo

- Aggiunti nodi strutturati modificabili per integrale, derivata e sommatoria, con campi separati e navigazione SU/GIU.
- Collegata la serializzazione dei modelli al parser; aggiunta la sommatoria discreta inclusiva `sum(f,a,b)` con limiti interi e limite di ampiezza.
- Renderer naturale: simboli ∫, d/dx e Σ; le forme ASCII restano compatibili con la tastiera PicoCalc.
- Aggiunti test su modelli annidati con potenze, navigazione, simboli e valutazione.
- Non sono ancora assegnati tasti fisici dedicati ai tre modelli; il calcolo integrale/derivativo è numerico, non simbolico. Test hardware e build UF2 non eseguiti.


## 1.5.3-dev — VPAM parser correctness increment (2026-11-07)

- Audited the actual v1.5.3 archive: its embedded VERSION was `1.5.2-dev`; the working tree now identifies itself as `1.5.3-dev`.
- Standardized the public n-th-root call to `root(degree, radicand)`, matching the requested ASCII syntax. Updated NaturalEditor serialization, natural formatting, cheatsheet and tests.
- Added two-argument `frac(numerator, denominator)` while preserving one-argument `frac(x)` as the fractional-part function.
- Added callable `factorial(n)`, `nCr(n,r)` and `nPr(n,r)` aliases with integer/range validation.
- Added regression tests for `root(4,16)`, fractions, factorial, combinations, permutations and invalid inputs.
- Full host suite: PASS. Pico SDK/ARM toolchain are not configured here; no UF2 build or physical-device test is claimed.
- Remaining major VPAM work: a shared typed AST, full parser-to-editor structural conversion, structural selection/clipboard/undo-redo, complete matrix/vector/complex expression parsing, natural function and calculus templates, and renderer scrolling.

## 1.5.3-dev — VPAM shared AST increment (2026-11-07)

- Introduced `fx::ast::Node` / `Kind` as the canonical typed tree in `expression_ast.hpp`.
- Replaced the editor-owned node declaration with aliases to the shared AST; the renderer continues to consume that same node type.
- Centralized AST shape validation and scalar serialization; incomplete structures and currently unsupported subscript/matrix evaluation fail explicitly.
- Added `evaluate(const fx::ast::Node&, x)` so the calculator can evaluate the exact tree exposed by the natural editor.
- Added host smoke coverage for fraction/power trees, direct evaluation, invalid shape rejection and editor/evaluator type sharing.
- Important boundary: the legacy text parser still evaluates while parsing; parser-to-AST conversion and native AST evaluation are the next increment, not claimed complete.

## 2026-11-07 — VPAM AST end-to-end foundation
- Added precedence-aware `parseExpression()` producing typed AST nodes.
- Replaced AST evaluation's serialize-and-reparse bridge with a direct recursive visitor for the supported scalar subset.
- Added editor import through `NaturalEditor::setExpression()` and renderer support for binary, unary, and function-call nodes.
- Expanded VPAM smoke coverage to parse/evaluate/editor/render integration.
- Documented remaining VPAM parity gaps explicitly.
