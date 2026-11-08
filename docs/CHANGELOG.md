# Changelog

## v1.8.0 (continuazione) — collaudo di GRAPH e microSD, completamento di TABLE

Sullo stesso branch `version/v1.8.0`. Verifiche su PC; **nessuna prova sulla
PicoCalc fisica** (procedura in [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md)).
Rapporto: [reports/SESSIONE_v1.8.0_COLLAUDO_TABLE.md](reports/SESSIONE_v1.8.0_COLLAUDO_TABLE.md).

### GRAPH
- `graph_verify_test`: ogni punto disegnato entro 1 pixel dalla curva vera
  (18 casi Y= e X=, r=, Param), discontinuità, domini, G-Solve, derivate e
  integrali contro valori analitici.
- Corretto: ∫dx attraverso un polo dava un numero senza senso → "Math ERROR:
  integrale divergente"; integrali di funzioni a salto imprecisi (4,500001)
  → divisione ai salti e ai picchi integrabili.
- Corretto: tratti PEN illimitati potevano rendere impossibile il salvataggio
  del workspace in flash → 1200 punti in totale; messaggio "Memoria flash
  piena" con la dimensione.
- Tratteggio delle disequazioni 4 volte più rapido (immagine identica).

### microSD
- Corretto: dopo l'estrazione e il reinserimento della scheda tra due
  operazioni ogni accesso falliva → reinizializzazione, nuovo montaggio e un
  nuovo tentativo; lettura ripetuta dopo un errore transitorio.
- Corretto: scheda non formattata segnalata come file danneggiato → stato
  `Unformatted` con istruzioni.
- Test sull'intera pila (GraphApp → FatFs → protocollo SD → scheda simulata),
  guasti, dimensioni, cartelle, scheda piena; `sd_interop_test` con
  fsck.fat/fsck.exfat/mtools; `tools/sd_image_tool`; simulatore con
  `PICOCALC_SD_IMAGE`.
- SYSTEM → F2 DIAG: test della microSD e tempi di GRAPH sul dispositivo,
  stampati anche sulla seriale USB.

### TABLE
- Riscritta: lista delle relazioni propria (SELECT, DELETE, TYPE Y=/r=/Param,
  STYLE, SET, TABLE), Setup (Variable Range/List, Derivative, Angle), fino a
  999 righe, celle di 6 caratteri, EDIT, ROW DELETE/INSERT/ADD, DELETE,
  LISTMEM, GPH-CON/GPH-PLT dalle righe effettive — [TABLE.md](TABLE.md).
- Corretti: lettura fuori dai limiti dopo un intervallo non valido; modulo SET
  che non si chiudeva con valori errati.
- Workspace versione 4 (righe e impostazioni della tabella), `.pcgraph` con
  `tablevar`/`tablefunc`.

### Test
- 25 test CTest (24 suite, 7169 controlli + 41 di interoperabilità), anche con
  ASan/UBSan; UF2 senza warning (3 012 096 byte).

## v1.8.0 — 2026-11-08 — Sessione GRAPH

Branch `version/v1.8.0` (creato da `version/v1.7.0`, commit `72285e9`).
Non ancora provata sulla PicoCalc fisica. Parità con la fx-CG50 **non**
completa: elenco voce per voce in [GRAPH_PARITY.md](GRAPH_PARITY.md).

### Motore GRAPH (nuovo, `graph_model`, `graph_engine`, `graph_solve`)
- Modello separato dall'interfaccia: 20 funzioni, 12 tipi (Y=, r=, Param,
  X=, Y>/Y</Y≥/Y≤, X>/X</X≥/X≤), stili e colori, V-Window con T e Xdot,
  Setup della modalità, memorie, SKETCH.
- Viewport con clipping di Liang–Barsky; campionamento adattivo incrementale
  con rilevamento di poli, salti e bordi di dominio; Draw Type Plot; bordi
  delle disequazioni; limite di 6000 punti per curva e 24 000 in totale.
- Intervalli `[a,b]`, sovrapposizioni `[A=…]`, grafici da lista, funzioni
  composte (`Y1(x)`, ≤ 5 livelli) tramite `FunctionResolver` nel valutatore
  condiviso; grafici incollati (≤ 10).
- G-Solve con Brent–Dekker, minimizzazione di Brent, derivata di Ridders,
  Gauss–Kronrod adattivo e area divisa alle radici.
- ZOOM BOX/FACTOR/IN/OUT/AUTO/ORIGINAL/SQUARE/ROUND/INTEGER/PREVIOUS e preset
  TRIG/STANDRD/INITIAL.

### Modalità GRAPH (riscritta)
- Lista delle relazioni Y1–Y20 con TYPE (13 voci, CONVERT), TOOL (STYLE,
  GPH-MEM, BUILT-IN, FORMAT, FILE), MODIFY, DRAW; editor naturale con VARS
  GRAPH e riferimenti.
- V-Window a 10 campi con INITIAL/TRIG/STANDRD, V-MEM, SQUARE, BGV-WIN; Setup
  proprio della modalità (16 voci) aperto dal tasto SETUP dentro GRAPH.
- Disegno incrementale non bloccante (600 valutazioni per ciclo), Simul Graph,
  livelli (sfondo con Fade, riempimenti Union/Intsect, area ∫dx, griglia,
  assi con scala ed etichette, immagine richiamata, curve, SKETCH, punti).
- TRACE con valore digitato, cambio grafico, derivata, linee ausiliarie e
  punti con etichetta; ZOOM; PAN; SKETCH (Cls, Tangent, Norm, Inverse,
  Plot/PlotOn/PlotOff/PlotChg, Line/F-Line, Circle, Vertical, Horz, PEN,
  Text, elenco degli elementi); G-SOLVE completo con ∫dx ROOT/INTSECT/MIXED;
  Modify con COPY; 20 memorie grafiche; PICTURE STORE/SAVE•AS/RECALL/OPEN,
  esportazione BMP, immagine di sfondo.
- Progetti `.pcgraph` sulla microSD: NEW, OPEN, SAVE, SAVE AS, RENAME,
  DELETE, file manager con spazio libero; errori di scheda assente/piena/
  danneggiata come "Storage ERROR".
- TABLE sullo stesso motore: colonne dY/dX, GPH-CON/GPH-PLT, → TABLE e
  GRAPH tra le due modalità.

### File e microSD (nuovo)
- Interfaccia `files::FileSystem`, scrittura atomica con verifica CRC e
  recupero dopo interruzione, scheda in RAM con iniezione di guasti.
- Formati documentati: `.pcgraph` (testo con CRC-32), `PCGMEM`, `.pcpict`
  (RLE RGB565), BMP ([PCGRAPH_FORMAT.md](PCGRAPH_FORMAT.md)).
- Protocollo SD in SPI (SDSC/SDHC/SDXC), FatFs R0.16 in `third_party/fatfs`,
  bus SPI0 della PicoCalc (GP16–19, card detect GP22).

### Persistenza
- Workspace versione 3 (stato GRAPH); lettura delle versioni 1 e 2 con
  correzione delle V-Window che non superano i nuovi controlli.

### Correzioni
- Le funzioni composte contano anche i riferimenti dentro gli argomenti; i
  riempimenti non coprono più gli assi; un nuovo G-Solve cancella l'area ∫dx
  precedente; i marcatori dei risultati non escono dall'area del grafico;
  heap esaurito durante il disegno → "Memory ERROR" senza ripetizioni.

### Test
- Nuove suite `graph_engine_test`, `graph_solve_test`, `graph_file_test`,
  `graph_app_test`, `sd_fatfs_test`: 22 suite, 6540 controlli, tutti superati
  anche con ASan/UBSan. Screenshot 06, 07, 20–34 da `tests/sim/graph.txt`.
- UF2 compilato senza warning nei sorgenti del progetto (2 874 880 byte,
  `picotool info` verificato); stack dei frame di GRAPH misurato con
  `-fstack-usage`.

## v1.7.0 — 2026-11-08 — Sessione 1: audit, architettura delle applicazioni, Run-Matrix

Branch `version/v1.7.0` (partito da `claude/peaceful-euler-auyrm5` = v1.6.0 + documentazione).
Non ancora provata sulla PicoCalc fisica.

### Architettura
- Registro delle 23 modalità (`app_registry.hpp/.cpp`): stato (Available / Planned /
  External), responsabilità, servizi condivisi, sezioni del workspace, collegamenti
  consentiti, file sorgente. Usato da launcher, AppManager e test.
- Modelli dati iniziali delle modalità future (`app_models.hpp/.cpp`): Recursion,
  Dyna Graph, Conic Graphs (con classificazione), 3D Graph, Financial, Spreadsheet
  (riferimenti di cella), file di Program/Python/eActivity, Geometry, Picture Plot.
- `SymbolsApp` in un modulo proprio; `docs/APPLICATIONS_ARCHITECTURE.md`.
- `architecture_test`: verifica registro ↔ sorgenti ↔ firmware, separazione delle
  app (Run-Matrix solo calcoli e risultati), stratificazione motore/UI, assenza di
  cicli di include.

### Motore matematico
- Aritmetica esatta (`exact.hpp/.cpp`): razionali, radicali con due termini,
  multipli di π; valutatore esatto sullo stesso AST del parser.
- Operatori relazionali e logici, Int÷, Rmdr, MOD_Exp, RndFix, RanNorm#, RanBin#,
  frazioni miste, decimali periodici, variabili condivise (Xmin…, TblStart…).
- Arrotondamento di Fix/Sci come la calcolatrice (0.125 → 0.13 in Fix2).

### Run-Matrix
- Risultati esatti (frazioni, √, π) con le regole di visualizzazione della CG50;
  S⇔D che non altera il valore; Ans e A–Z conservano la forma esatta.
- Frac Result d/c / ab/c (System) e conversione per singolo risultato; modello di
  frazione mista; vista e input dei numeri periodici; radice con indice modificabile.
- Cronologia: modifica sul posto con ricalcolo delle righe successive, riuso di
  espressione o risultato, eliminazione; →A~F; VARS per categorie (A–Z, Ans,
  V-WIN, TABLE); OPTN con LOGIC, RELAZIONI e RISULTATO; "≈" sui risultati numerici;
  barra di scorrimento della cronologia; indicatore d/c / ab/c nella barra di stato.

### Persistenza
- Formato del workspace versione 2 (valori esatti, Frac Result, Ans complesso);
  i record v1.6.0 (versione 1) vengono ancora letti.

### Test
- Nuove suite `exact_test`, `run_matrix_test`, `architecture_test`; 17 suite in
  totale, tutte superate anche con ASan/UBSan. Screenshot 12–19 dal simulatore.
- UF2 compilato senza warning nei sorgenti del progetto; stack dei nuovi frame
  misurato con `-fstack-usage` sul target.

## v1.6.0 — 2026-11-08 — prima build RP2350 per PicoCalc + Pico 2 W

Prima versione che **compila per l'RP2350** (UF2 verificato con `picotool`). Non
ancora provata sulla PicoCalc fisica. Dettagli in [BUILD_STATUS.md](BUILD_STATUS.md).

### Piattaforma
- Driver riscritti secondo i sorgenti ufficiali ClockworkPi: LCD ST7365P su SPI1
  hardware a 25 MHz con la sequenza di init ufficiale (RGB666, `INVON`, gamma),
  tastiera STM32 su I2C1 con STOP + attesa di 16 ms (macchina a stati non bloccante),
  batteria e retroilluminazione via registri STM32.
- Aggiornamento parziale del display a tile 16×16 senza secondo framebuffer.
- Eccezioni C++ abilitate, `lwipopts.h`/`mbedtls_config.h`, linker script con
  stack da 32 KB.
- Storage flash A/B con CRC-32 e `flash_safe_execute`, autosalvataggio.
- Simulatore host con screenshot PNG; build host con CMake/CTest.

### Interfaccia
- Framework di app (`AppManager`) con tasti globali MENU, EXIT lungo, SHIFT, SET UP,
  finestre di errore in stile CG50, toast.
- Font GNU Unifont 8×16 e Spleen 6×12; barra di stato e tasti funzione F1–F6.
- Menu principale a icone con 23 modalità; quelle non realizzate sono marcate e
  aprono una spiegazione.

### Matematica
- Pipeline unica tokenizer → AST → valutatore (reale e complesso) al posto dei due
  parser precedenti; priorità Casio, funzioni prefisse, costanti, Base-N, DMS,
  Pol/Rec, ∫/d/dx/d²/dx²/Σ/Π/SOLVE.
- Editor Natural con undo/redo, copia/incolla, scorrimento, metriche uniche con il
  renderer (cursore allineato).
- Limiti contro l'esaurimento dello stack: 320 token, 32 livelli di annidamento,
  12 modelli annidati e 400 nodi nell'editor, budget di stack di 16 KB per
  valutazione; catene lunghe (`1+2+…+n`) valutate in modo iterativo.

### Modalità
- Nuove: Statistics, Graph, Table, Equation, System, Memory, Conversion, AI Chat.
- Run-Matrix con storia, OPTN, VARS, S⇔D, numeri complessi.

### AI
- Relay Python con SDK ufficiali Anthropic (`claude-opus-5-5`, fallback lato server
  sui rifiuti) e OpenAI; token per dispositivo, limiti, TLS.
- Client firmware Wi-Fi + HTTPS con verifica obbligatoria del certificato.

### Test
- 14 suite host, 4495 controlli, anche con ASan/UBSan; 15 test del relay.

### Correzioni rispetto alla v1.5.4
- Vedi la tabella "Audit della v1.5.4" in [BUILD_STATUS.md](BUILD_STATUS.md).
- Rimossi i moduli sostituiti (vecchio `ui.cpp`, `graph_renderer`,
  `window_manager`, `widget_system`, PIO-SPI, frontend SDL); i loro test sono stati
  portati sulle nuove API.

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
