# Sviluppo della v1.19.0 — Geometry alla parità

Obiettivo della versione: **Geometry** (capitolo 14 del manuale fx-CG50) alla
**parità completa** sulla Pimoroni Pico Plus 2 W; la modalità passa da **Base**
ad **Available**. Analisi, tabella di parità e decisioni scritte prima del
codice: [ANALISI_FONTI_v1.19.0.md](ANALISI_FONTI_v1.19.0.md). Il **Raspberry Pi
Pico 2 W resta congelato** alla v1.16.0. **Nessuna prova su hardware**:
[HARDWARE_TEST_v1.19.0.md](../HARDWARE_TEST_v1.19.0.md) (HW-272…HW-295).

## 1. Git

| Voce | Valore |
|---|---|
| Partenza | `main` = `a7c861b` (v1.18.0); `git fetch origin main` all'inizio e prima della pubblicazione |
| Branch di lavoro | `ccr-6804dfc4-cwp8zw` (commit intermedi: analisi e fixture, nucleo e controller con i test, simulatore e fuzzing, errori del fuzzing, documentazione) |
| Pubblicazione | un solo commit su `main` in fast-forward, «PicoCalc FxCG-50 v1.19.0: …» |

## 2. Decisioni

Le domande della fase 0 hanno le risposte scelte in
[ANALISI_FONTI §7](ANALISI_FONTI_v1.19.0.md#7-domande-aperte-e-risposte-scelte)
(Q1–Q18: casella di misura con OPTN e con EXIT, Undo a un livello, punti legati
alle curve, etichette automatiche, figure dal riquadro, fine dell'arco legata,
intersezioni senza punti, bisettrici di parallele, f(x) non trasformabile, unità
degli angoli, parametri delle animazioni, Go (repeat), punti rimessi a posto,
colonne doppie, testi Sì/No, limiti, misure delle immagini, Area Calc). Durante
lo sviluppo si sono aggiunte:

| # | Decisione | Motivo |
|---|---|---|
| S1 | Tutti gli strumenti di Draw restano attivi dopo la figura (non solo Point) finché non si preme EXIT | si disegnano più figure di seguito; la fx-CG50 lo dice per Point |
| S2 | La casella di misura torna al primo tipo quando cambia la selezione | un valore scritto va sempre alla misura che si vede (errore F3) |
| S3 | Un numero entro 1e−9 relativo da un intero si mostra come intero | una lunghezza bloccata a 5 resta «5», non «5.00» per l'arrotondamento del risolutore |
| S4 | Animazione: un fotogramma per ciclo del firmware, senza cadenza a tempo | la velocità è quella del disegno; da misurare sul dispositivo (HW-291) |
| S5 | Dalla striscia di eActivity EXIT a riposo (senza selezione) torna al documento; la casella si apre con OPTN | stessa regola delle altre strisce |
| S6 | Il titolo dei menu a tendina elenca le schede, quella aperta tra parentesi quadre, con ◀▶ | le schede si vedono senza provarle |
| S7 | Moduli di testo (Text, Label, Expression): EXE conferma, EXIT annulla, AC cancella la riga | come l'editor di riga comune |
| S8 | Colonne della tabella con il nome dei vertici («CD Lunghezza») | si riconosce la misura nell'elenco |
| S9 | Store Picture scrive nelle memorie Pict 1–20, che sono i file `PICTnn.pcpict` di `PICOCALC/PICT` (gli stessi di Graph); nessun nome libero | O12 chiedeva «Pict 1–20 o un file»: le memorie sono già file leggibili da Graph, eActivity e Picture Plot |

## 3. Tabella di parità finale (capitolo 14)

Tutte le voci del §3 dell'analisi sono **presenti**, ognuna con il suo test; le
differenze inevitabili sono al §5. Test: `geometry_test` (GT, nucleo puro,
262 controlli) e `geometry_flow_test` (GF, firmware completo con i tasti della
PicoCalc, 214 controlli); i nomi dei casi portano l'id della voce.

| Voci | Stato | Test |
|---|---|---|
| K01 menu F1–F6 con le schede; K02 puntatore (passo, SHIFT, salti 1–9, evidenziazione); K03 tasti diretti e uscita dalla striscia | fatto | GF K01–K03 |
| F01 New; F02 Open (DELETE, file della v1.17.0); F03 Save as; F04 Key Help; F05 Option | fatto | GF F01–F05 |
| V01 Zoom Box; V02 Pan; V03 Scroll; V04/V05 Zoom In/Out; V06 Zoom to Fit; V07 assi; V08 griglia; V09 V-Window; V10 Axes e Grid nel menu | fatto | GT V01, V04, V06, V09; GF V01–V10 (pixel degli assi e della griglia) |
| E01 Undo/Redo; E02 Select All; E03 Deselect All; E04 Select Figure; E05 Delete con i dipendenti; E06 Clear All; E07 selezione; E08 spostamento | fatto | GT E01, E04, E05, E08; GF E01–E08 |
| O01 Text; O02 Label ed etichette automatiche; O03 Expression; O04 Number Format; O05 Clr Constraint; O06 Show All; O07 Hide; O08 Area Calc; O09 ordine; O10 All TEXT; O11 Fade I/O e sfondo; O12 Store Picture; O13 Clear Picture; O14 FORMAT | fatto | GT O02–O05, O07, O09, O10, O14; GF O01–O14 (pixel del tratto rosso spesso) |
| D01 Point (anche legato); D02–D05 linee; D06 Circle; D07 Arc; D08 SemiCirc; D09 3-Point Arc; D10–D13 figure del riquadro (vincolate); D14 Polygon; D15 n-gono; D16 f(x); D17 riquadro piccolo | fatto | GT D01–D17; GF D01–D17 |
| C01–C08 le otto costruzioni; C09 dipendenze e ricalcolo incrementale; C10 casi degeneri; C11 selezione sbagliata | fatto | GT C01–C10 (intersezioni di ogni coppia di tipi, tolleranze); GF C01–C11 |
| T01–T06 le sei trasformazioni; T07 immagini legate; T08 selezione parziale; T09 proprietà | fatto | GT T01–T09 e G01–G06; GF T01–T09 |
| M01 casella; M02–M17 i tipi di misura; M18 valore scritto e blocco; M19 lucchetto; M20 Paste; M21 vincoli incompatibili; M22 etichetta | fatto | GT M02–M21, G14; GF M01–M22 |
| A01 Add Animation (tutti i percorsi); A02 Replace; A03 Trace; A04 Edit Animation; A05 Go (once); A06 Go (repeat); A07 Add Table; A08 Display Table; A09 LIST; A10 STAT; A11 DELETE/DEL-ALL; A12 step::Task con AC e punti rimessi | fatto | GT A01–A12; GF A01–A12 |
| S01 SET UP; P01 `.pcgeo` v2; P02 file e sezione della v1.17.0; P03 sezione `0x0110` v2 e riavvio; P04 limiti con Memory ERROR; R01 registro | fatto | GT P01–P04; GF S01, P03, P04; `architecture_test`, `apps_init_test` |

## 4. Cosa c'è nel codice

| Parte | File | Note |
|---|---|---|
| Nucleo: oggetti e grafo | `include/geometry.hpp`, `src/geometry.cpp`, `src/geometry_internal.hpp` | id crescenti = ordine topologico; 32 definizioni; forma calcolata come cache; `recompute` completo o incrementale (segna i discendenti in un passaggio); intersezioni retta/retta, retta/circonferenza, circonferenza/circonferenza con segmenti, semirette e archi e le tolleranze dichiarate; f(x) col valutatore unico in radianti; selezione col puntatore; vista |
| Nucleo: costruzione | `src/geometry_build.cpp` | Draw, Draw Spec (vertici vincolati: rettangolo, quadrato, isoscele, n-gono, fine dell'arco), Construct, Transform (punti immagine e figure sulle immagini, curve costruite come `ImageCurve`), cancellazione con i dipendenti, Clr Constraint, ordine, spostamenti in forma chiusa lungo i gradi di libertà; garanzia forte con una transazione |
| Nucleo: misure e blocchi | `src/geometry_measure.cpp` | 19 tipi di misura secondo la selezione, formati, testi; blocchi come equazioni; `SolveTask` (step::Task): proiezione di Gauss-Newton a norma minima pesata, Jacobiana per differenze, ricerca lineare, AC rimette lo stato |
| Nucleo: animazioni | `src/geometry_anim.cpp` | `AnimationTask` (step::Task, un fotogramma per unità), tracce, tabella con celle indefinite, Edit Animation, colonne |
| Nucleo: file | `src/geometry_file.cpp` | sezione `0x0110` e `.pcgeo` versione 2 con lettura a garanzia forte; formato 1 della v1.17.0 letto e migrato |
| Controller | `src/apps/geometry_app.cpp`, `src/apps/geometry_view.cpp`, `src/apps/geometry_ui.hpp` | menu a tendina con schede sul `ModeShell`, strumenti, casella di misura, moduli, lavori a passi sul `step::Runner` (animazioni, tabella, risolutore), Store/Recall Picture con `pict`; disegno con ritaglio di Liang–Barsky, cinque tratti, aree, etichette, tracce, anteprime |
| Comuni | `include/apps.hpp` (classe `GeometryApp`), `src/app_registry.cpp` (Available, servizi, `linkTo(Statistics)`, intestazioni del nucleo), `cmake/fxai_sources.cmake` | sul Pico 2 W nessun byte di codice cambia |
| Strumenti | `tools/fuzz/fuzz_geometry.cpp`, `tests/sim/geometry_v119.txt`, `tests/fixtures/geometry_v1_17_0.inc` (con il generatore) | fuzzing del formato e delle sequenze di comandi, screenshot, fixture della v1.17.0 |

## 5. Differenze inevitabili

Come previsto ([ANALISI_FONTI §6](ANALISI_FONTI_v1.19.0.md#6-differenze-inevitabili-previste)),
D1–D9: grafica, font e barra propri; tasti della PicoCalc; casella anche con
OPTN e menu Option della fx-CG50 nelle schede di F2; File › Option; Clr
Constraint che non tocca costruzioni e trasformazioni; nessun riscalamento
nascosto al primo valore di lunghezza (la vista si adatta); sfondi `.pcpict`;
STORE › STAT al posto di S-SHT; voci in più richieste dalla versione. Nessuna
differenza nuova emersa durante lo sviluppo.

## 6. Errori trovati durante lo sviluppo

| # | Trovato da | Errore | Correzione | Prova |
|---|---|---|---|---|
| F1 | `fuzz_geometry` | animazione con dei blocchi: il risolutore spostava anche punti non animati e alla fine tornavano a posto solo quelli animati | l'animazione salva e rimette tutti gli scalari | `geometry_test` A12, `tests/fixtures/fuzz/fuzz_geometry_anim_locks.bin` |
| F2 | `fuzz_geometry` | cambiare l'angolo di rotazione (o il fattore) dalla casella non risolveva di nuovo i blocchi sulle immagini | risoluzione dopo il cambio; se è impossibile «Math ERROR: vincoli incompatibili» e nulla cambia | `geometry_test` M17/M21, `fuzz_geometry_rotation_locks.bin` |
| F3 | `geometry_flow_test` | la casella teneva il tipo di misura della selezione precedente: il valore scritto andava a un'altra misura | S2 | GF M18 |
| F4 | `geometry_flow_test` | `AnimationTask::step` consumava l'unità prima del fotogramma: con un'unità per passo l'animazione non avanzava mai | prima il fotogramma, poi il budget | GT A05 («primo fotogramma»), GF A05 |
| F5 | build del firmware | `dynamic_cast` nel controller (il firmware è senza RTTI) | lavori in puntatori tipizzati | build della Plus senza errori |
| F6 | `geometry_flow_test` | una lunghezza bloccata a 5 mostrata «5.00» | S3 | GF M20 |
| F7 | fuzzing | `run_fuzz.sh` si fermava (SIGPIPE con `pipefail`) con più di 3 000 stringhe nei test | la pipeline delle stringhe tollera la chiusura di `head` | `tools/fuzz/run_fuzz.sh` |
| F8 | AddressSanitizer (`--sanitize`) | tabella › STORE › STAT apriva Statistics dentro la scelta del menu: `leave()` liberava la schermata di Geometry e il resto del tasto la usava ancora (heap-use-after-free) | Statistics si apre come ultima cosa del tasto | GF A10 sotto ASan |
| F9 | build con i sanitizer (GCC 13) | avvisi trattati come errori solo con `-fsanitize`: assegnazione da lista a un vettore (falso positivo di `-Warray-bounds`) e un buffer di 8 byte per `%d` | `assign(1, c)`, buffer di 16 | build `--sanitize` senza avvisi |

Errori dei test stessi corretti durante lo sviluppo (non del firmware): selezioni
sul punto medio invece che sul segmento, finestre d'errore chiuse dai tasti
seguenti, `x^2-2` scritto senza chiudere l'esponente nell'editor Math (che dà
`x^(2-2)`, come in Run-Matrix), aspettative sul risolutore che distribuisce lo
spostamento fra più punti.

## 7. Test, fuzzing, simulatore

| Verifica | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | **64/64**: 61 suite C++ con **20 594** controlli (+531 sulla v1.18.0; `mem_pools_smoke` escluso dal conteggio come nelle versioni precedenti), `mem_pools_smoke`, `sd_interop_test`, `link_tool_test` | **54/54**: 52 suite con **16 378** controlli (+35, solo `architecture_test` 2 118), `mem_pools_smoke`, `sd_interop_test` |
| Suite nuove | `geometry_test` 262, `geometry_flow_test` 214 | — |
| Suite cambiate | `architecture_test` 2 572 (+118: Geometry Available, nucleo senza interfaccia), `apps_init_test` 3 033 (sequenza nuova di Geometry), `base_modes_test` 444 e `base_flow_test` 118 (i casi di Geometry sono passati alle suite nuove), `workspace_v117_test` (costruzione con l'API nuova) | `architecture_test` 2 118 (+35); le altre identiche |
| ASan + UBSan (bloccante), TSan | 64/64, TSan superato | 54/54, TSan superato |
| Fuzzing (≥ 300 s per harness nuovo o toccato) | nuovo: **`fuzz_geometry`** 82 133 esecuzioni (315 s, sequenze di comandi da 48 passi con le invarianti dopo ognuno; formato v1 e v2); toccati: `fuzz_modes` 4 449 143 esecuzioni (331 s, sezione e file v2), `fuzz_workspace` 1 670 584 (328 s, sezione `0x0110` v2), `fuzz_ui` 10 634 (319 s, controller e striscia), `fuzz_eactivity` 1 108 con il controller (326 s, striscia di Geometry) e 6 249 384 sui soli documenti (328 s): nessun crash né artefatto sul codice finale. Durante lo sviluppo `fuzz_geometry` ha trovato i due errori F1–F2 (§6); `fuzz_ui` e `fuzz_eactivity` ripetuti dopo la correzione F8 | — |
| Simulatore (`tools/sim_compare.py`) | **192 identici** al confronto finale: 162 precedenti invariati, 2 rigenerati per cambiamenti voluti (sotto), 28 nuovi (175–202, `tests/sim/geometry_v119.txt`) | le stesse schermate (Geometry non esiste) |
| Relay AI | 15/15 | — |

Screenshot rigenerati: **144** (`base_v117.txt`: la Geometry della v1.17.0 con
la barra DRAW…FILE e la finestra MEASURE ora è il triangolo con i menu nuovi e
la casella di misura) e **172** (menu principale: l'icona di Geometry non ha più
il triangolo ambra delle modalità Base; differenza limitata al riquadro 22×22
dell'angolo dell'icona). Tutti gli altri sono identici pixel per pixel.

## 8. Firmware e memoria

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Immagine | **2 983 560 B** = 71 % della partizione da 4 MB (+176 344 B sulla v1.18.0; budget 3 774 873 B: restano 791 313 B) | **1 944 228 B** = 92 % della partizione da 2 MB, **identica** alla v1.18.0 salvo la stringa della versione (4 byte); restano 120 156 B sotto il budget di 2 064 384 B |
| RAM statica | 312 736 B fino a `__end__` = `0x2004C5A0` (+128 B); heap SRAM 178 784 B | 305 236 B (invariata); heap SRAM 186 284 B |
| Oggetti esclusi nella mappa | 0 (tutto compilato) | **185** oggetti/simboli della Plus (+6: i file nuovi di Geometry e `2fx3geo`), nessuno nell'immagine |
| Warning del compilatore | nessuno | nessuno |

Flash dei moduli di Geometry sulla Plus (mappa del linker, `tools/map_sizes.py`):

| Modulo | Flash (B) |
|---|---:|
| `apps/geometry_app.cpp` (tasti, menu, strumenti, casella, moduli, lavori a passi) | 46 723 |
| `geometry_build.cpp` (Draw, Construct, Transform, Edit) | 37 405 |
| `geometry_measure.cpp` (misure, blocchi, risolutore) | 35 396 |
| `apps/geometry_view.cpp` (disegno) | 27 669 |
| `geometry.cpp` (oggetti, grafo, intersezioni, vista) | 27 572 |
| `geometry_file.cpp` (sezione e `.pcgeo` v1/v2) | 20 705 |
| `geometry_anim.cpp` (animazioni e tabella) | 11 449 |
| **Totale** | **206 919** |

La Geometry della v1.17.0 (29 228 B) non c'è più, da qui la crescita netta
minore della somma. Stack (`-fstack-usage`, Plus): frame più grandi del codice
nuovo `geo::transform` 784 B, `openSetup` 784 B, `geo::addSpec` 704 B,
`GeometryApp::key` 584 B, `evalShape` 552 B, `drawMain` 504 B; il massimo del
firmware resta `SystemApp::enter` (2 016 B). Nessuna ricorsione nel nucleo
(ricalcolo, discendenti e chiusure in un passaggio o con una pila esplicita);
la misura di un'espressione legge le misure incollate a un solo livello.

Dettagli e hash degli UF2: [BUILD_STATUS.md](../BUILD_STATUS.md).

## 9. Cancello

| Controllo | Esito |
|---|---|
| `git fetch origin main` | `main` fermo a `a7c861b`: fast-forward |
| `tests/run_host_tests.sh` (due profili) | 64/64 e 54/54 |
| `--sanitize --tsan` | 64/64 e 54/54, TSan superato su entrambi (sulla Plus dopo le correzioni F8 e F9) |
| Fuzzing ≥ 300 s per harness nuovo o toccato | 6 corse sul codice finale (1 nuovo, 4 toccati, `fuzz_eactivity` anche sui soli documenti), ognuna ≥ 300 s: nessun crash |
| Simulatore | 162 identici, 2 rigenerati con il motivo (144, 172), 28 nuovi; confronto finale 192/192 |
| Firmware | due schede `--clean`, nessun warning; Plus 71 % (< 90 %); Pico 2 W 1 944 228 B, identico alla v1.18.0 salvo la stringa della versione |
| Congelamento del Pico 2 W | `architecture_test` («Pico 2 W frozen at v1.16.0», 23 modalità, nessuna Base); immagine di 1 944 228 B confrontata byte per byte con la v1.18.0 compilata dallo stesso ambiente: **4 byte diversi**, tutti la cifra della versione («1.18.0» → «1.19.0») |
| Relay | 15/15 |
| Documentazione | ANALISI_FONTI e SVILUPPO v1.19.0, GEOMETRY (guida completa con gli esempi), HARDWARE_TEST_v1.19.0, FEATURE_MATRIX, ROADMAP, BUILD_STATUS, CHANGELOG, APPLICATIONS_ARCHITECTURE, FILE_FORMATS, STORAGE, TESTING, README, docs/README, releases/v1.19.0, CONTINUE, VERSION |
