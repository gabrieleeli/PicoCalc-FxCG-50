# Recap del progetto e parità con la Casio fx-CG50 — v1.14.0

Fotografia del repository al commit `b9aac14` (tag `v1.14.0`, `main`), fatta
leggendo codice, registro delle modalità e documentazione. Serve come punto di
partenza per la sessione descritta in
[../PROMPT_BASI_APPLICAZIONI.md](../PROMPT_BASI_APPLICAZIONI.md), che mette le
basi comuni per tutte le applicazioni ancora da realizzare.

> In breve: il progetto **non** ha la parità con la fx-CG50. Su 20 modalità
> della calcolatrice, **3** hanno la parità software completa (Run-Matrix,
> Equation, 3D Graph), **2** sono quasi complete (Graph, Table), **3** sono
> parziali (Statistics, Memory, System), **10** hanno solo il modello dati,
> **1** non è iniziata (Python) e **1** non è realizzabile (E-CON4). Nessuna
> funzione è stata provata sulla PicoCalc fisica.

## 1. Cos'è il progetto

Firmware C++17 per la **ClockworkPi PicoCalc** che riproduce il comportamento
della Casio fx-CG50 (schermate, tasti, messaggi d'errore, risultati esatti), più
una chat AI (Claude / ChatGPT) tramite un relay HTTPS in Python che tiene le
chiavi lontane dal dispositivo.

| Voce | Valore |
|---|---|
| Schede | Pimoroni Pico Plus 2 W (RP2350B, 16 MB flash, 8 MB PSRAM) — profilo principale; Raspberry Pi Pico 2 W (4 MB flash, senza PSRAM) — profilo alternativo |
| Codice | ≈ 100 000 righe tracciate (sorgenti, test, documentazione, FatFs e LittleFS di terze parti) |
| Moduli | 23 voci nel menu principale (20 della fx-CG50 + Conversion, AI Chat, Symbols), registro in `src/app_registry.cpp` |
| Test | 47 test CTest per profilo: 45 suite C++ con 13 111 controlli (Plus) / 13 105 (Pico 2 W), stress dell'allocatore (≈ 333 000 verifiche), interoperabilità microSD con `fsck`; ASan/UBSan e TSan; 8 harness di fuzzing; relay 15/15 |
| Firmware | Plus: immagine di 1 948 024 byte (46 % di 4 MB); Pico 2 W: 1 939 180 byte (**92 % di 2 MB**) |
| RAM | statica 309 024 B (Plus) / 305 100 B; heap SRAM 182 496 B / 186 420 B; PSRAM 8 MB come pool TLSF separato (solo Plus) |
| Workspace | record versionato **v10** con CRC, A/B su flash (512 KB sulla Plus, 59,75 KB sul Pico 2 W); legge tutte le versioni dalla 1 alla 9 |
| Hardware | **nessuna prova fisica**: procedure HW-01…HW-189 pronte in `docs/HARDWARE_TEST_v1.*.md` |

## 2. Cronologia

| Versione | Contenuto principale |
|---|---|
| ≤ 1.5.4 | codice originale (non compilava per RP2350) |
| 1.6.0 | prima build RP2350, modalità di base, relay AI |
| 1.7.0 | audit, architettura delle applicazioni (registro, servizi, `architecture_test`), Run-Matrix con risultati esatti |
| 1.8.0 | GRAPH, microSD (SPI0 + FatFs), TABLE, collaudo software |
| 1.9.0 | Pimoroni Pico Plus 2 W: PSRAM, flash 16 MB, pool TLSF, primo 3D Graph |
| 1.10.0 | matrici, vettori e liste nel parser, editor MAT/VCT, Math/Line, CLIP |
| 1.11.0 | List 1–26, CSV, MatAns/ListAns, Base-N, simboli ingegneristici, comandi grafici |
| 1.12.0 | **EQUATION** completa (SIML, POLY, SOLVER) |
| 1.13.0 | **Run-Matrix** completa (List File, CALC/PROB/STAT/CONVERT/TVM, CATALOG, CAPTURE) |
| 1.13.1 | revisione completa, 18 errori corretti con un test ciascuno |
| 1.14.0 | **3D Graph** completo (capitolo 16) |

## 3. Architettura in una pagina

```
Modalità (src/apps/*_app.cpp)           un controller App per modalità
Framework: AppManager · Context · registro · widget (Form, ResultList, CellGrid, FileDialog)
UI: ui_kit, math_renderer, natural_editor, line_editor, font, canvas a tile 16×16
Nuclei: run_matrix, equation, graph_model/engine/solve, table_model, graph3d_model, calc_library
Motore: expression (un solo parser/AST/valutatore), exact, number_format, solvers, statistics
Workspace (dati condivisi tra modalità) → record versionato → RecordStore A/B con CRC-32
Firmware (boot, tastiera, autosalvataggio) → porta Pico (LCD SPI1, I2C, SD, Wi-Fi) / host (simulatore)
```

Regole verificate da `tests/architecture_test.cpp`: ogni modalità include solo
gli header dei servizi dichiarati, accede solo alle sezioni del workspace
dichiarate, apre solo le modalità collegate; il motore non include l'UI; nessun
ciclo di include. Le modalità non realizzate mostrano una schermata «non
disponibile» (nessuna applicazione finta).

## 4. Parità con la fx-CG50, modalità per modalità

Legenda: **Completa** = ogni voce della tabella di parità del manuale è
presente o è una differenza inevitabile documentata (solo software, non
collaudata su hardware); **Quasi** = mancano poche voci elencate;
**Parziale** = mancano parti importanti; **Modello** = solo modello dati in
`include/app_models.hpp` con validazione e test, nessuna schermata;
**Assente**; **Non realizzabile**.

| # | Modalità fx-CG50 | Manuale | Parità | Cosa c'è | Cosa manca |
|---|---|---|---|---|---|
| 1 | Run-Matrix | cap. 2–3 | **Completa** (v1.13.0) | Math/Line, esatti, S⇔D, matrici/vettori/liste, MAT/VCT, List File, OPTN/VARS/CATALOG, CALC, PROB, STAT, CONVERT, TVM, Base-N, comandi grafici, CAPTURE | differenze inevitabili in `SVILUPPO_v1.13.0.md` §6; A3 (risultati < 1E−99 da confrontare con una fx-CG50 vera) |
| 2 | Statistics | cap. 6 | **Parziale** | List 1–26 con sottonomi e CSV, 1/2-Var, 9 regressioni (ax+b, Med-Med, X², X³, X⁴, Log, aeᵇˣ, abˣ, Power), Scatter, xyLine, istogramma, box plot, 10 distribuzioni, Z/t a 1 campione e relativi intervalli | regressioni Sin e Logistic; grafici N-Dist, Broken, Med-Box, Pie, Bar e StatGraph 1–3; test 2-Sample Z/t, 1/2-Prop Z, χ² GOF e 2-way, 2-Sample F, LinearReg t, ANOVA (le API ANOVA e t a 2 campioni esistono); intervalli a 2 campioni e di proporzione; distribuzioni inverse, geometrica e ipergeometrica nell'interfaccia (esistono già in `calc_library` per Run-Matrix); CALC dal grafico, residui. Elenco da confermare con il capitolo 6 nella sessione che la completa |
| 3 | eActivity | cap. 10 | **Modello** | `EActivityDocument`/`EActivityStrip` | editor di strisce, valutazione con il nucleo di Run-Matrix, strisce grafiche, file |
| 4 | Spreadsheet | cap. 9 | **Modello** | `SpreadsheetModel` sparso, `parseCellRef` | griglia A–Z × 999, formule e ricalcolo, CELL/STAT/grafici, copia da/verso liste, file |
| 5 | Graph | cap. 5 | **Quasi** | 20 funzioni Y=, r=, Param, X=, disequazioni; TRACE, ZOOM, V-Window, SKETCH, G-Solve, Modify, PICT, `.pcgraph` (135 voci in `GRAPH_PARITY.md`) | Dual Screen G+G e GtoT, Graph to Table, FORMAT durante lo SKETCH |
| 6 | Dyna Graph | 5-42 | **Modello** | `DynaGraphModel`, dati D Start/End/Pitch nel workspace, comando DrawDyna | lista delle funzioni, animazione, Locus, velocità, SET |
| 7 | Table | 5-32…5-35 | **Quasi** | range/lista, Y=/r=/Param, derivate, EDIT/ROW, LISTMEM, GPH-CON/PLT | Dual Screen T+G |
| 8 | Recursion | 5-45 | **Modello** | `RecursionModel`, formule an+1 nel workspace, DrawR/DrawWeb | lista delle formule, tabella, grafici e web, Σ, SET |
| 9 | Conic Graphs | 5-50 | **Modello** | `ConicModel` + `classify()` | modelli di equazione, grafico, G-Solve specifici (fuochi, vertici, direttrice, asintoti…) |
| 10 | Equation | cap. 4 | **Completa** (v1.12.0) | SIML 2–6, POLY 2–6 esatti e complessi, SOLVER | A6 (SolveN/FMin/FMax dentro il SOLVER) |
| 11 | Program | cap. 8 | **Modello** | `SourceFile`, `validateProgramName` | editor, interprete (oggi deciso: linguaggio proprio, non Casio Basic), I/O, file |
| 12 | Financial | cap. 7 | **Modello** | `FinancialModel`; le 22 funzioni TVM esistono già in `calc_library` | schermate Simple/Compound/Cash Flow/Amortization/Conversion/Cost/Days/Depreciation/Bond, grafici dei flussi |
| 13 | E-CON4 | — | **Non realizzabile** | — | richiede un data logger esterno; al più import CSV nelle liste |
| 14 | Link | cap. 13 | **Modello** (solo nel registro) | — | trasferimento del record via USB seriale con formato proprio (il protocollo Casio non è previsto) |
| 15 | Memory | cap. 11 | **Parziale** | uso, cancellazione selettiva per sezione (anche 3D G-Mem/V-Win), salvataggio | browser dei file utente su microSD/LittleFS, backup/ripristino, ricerca, ottimizzazione |
| 16 | System | cap. 12 | **Parziale** | SET UP, retroilluminazione, archivio file, Wi-Fi, relay, INFO, DIAG, RESET | spegnimento automatico e gestione energetica, lingua, data/ora (non c'è RTC) |
| 17 | Python | cap. Python | **Assente** | `validateScriptName` | runtime MicroPython con heap separato (in PSRAM sulla Plus), editor, shell, moduli `casioplot`-simili |
| 18 | Geometry | cap. 14 | **Modello** | `GeometryObject` | costruzioni, misure, animazioni, file |
| 19 | Picture Plot | cap. 15 | **Modello** | `PicturePlotModel`; immagini `.pcpict`/BMP già lette da Graph | piazzamento dei punti, liste, regressioni, file |
| 20 | 3D Graph | cap. 16 | **Completa** (v1.14.0) | modelli, Z=, Param, Rot, V-Window, Setup, G-Mem, TRACE, G-Solve, SKETCH | griglia ≤ 25 e pittore senza PSRAM (Pico 2 W) |

Modalità in più rispetto alla fx-CG50: **Conversion** (unità, risultato in
Ans), **AI Chat** (relay HTTPS, nessuna chiave sul dispositivo), **Symbols**
(guida dei simboli).

### Sintesi

| Livello | Modalità | Numero |
|---|---|---|
| Completa | Run-Matrix, Equation, 3D Graph | 3 |
| Quasi | Graph, Table | 2 |
| Parziale | Statistics, Memory, System | 3 |
| Modello | eActivity, Spreadsheet, Dyna Graph, Recursion, Conic Graphs, Program, Financial, Link, Geometry, Picture Plot | 10 |
| Assente | Python | 1 |
| Non realizzabile | E-CON4 | 1 |

Le funzioni più usate a scuola (calcolo, equazioni, grafici, tabelle, 3D) ci
sono; mancano quasi tutte le modalità «di applicazione» del menu, ed è per
queste che serve una base comune: oggi ogni modalità realizzata ha il proprio
controller scritto da zero sui widget di base.

## 5. Qualità e verifica

- Ogni versione segue lo stesso metodo: analisi delle fonti e tabella di parità
  prima del codice (`docs/reports/ANALISI_FONTI_*`), sviluppo con un test per
  ogni voce, rapporto finale (`SVILUPPO_*`), procedura di collaudo
  (`HARDWARE_TEST_*`), un solo commit su `main` in fast-forward, tag e release
  dal workflow `.github/workflows/release.yml`.
- Test host su entrambi i profili, sanitizer, fuzzing, simulatore con confronto
  pixel per pixel degli screenshot.
- **Punto debole principale**: nessun collaudo fisico. Display, tastiera,
  PSRAM, flash, microSD, Wi-Fi/TLS e tempi sul Cortex-M33 sono verificati solo
  su PC.

## 6. Vincoli che pesano sulle prossime modalità

| Vincolo | Effetto |
|---|---|
| Flash del Pico 2 W al **92 %** (≈ 160 KB liberi su 2 MB) | non c'è spazio per le 10 modalità mancanti sul Pico 2 W: serve una scelta (modalità per scheda, ottimizzazione di dimensione, nuova partizione con migrazione) prima di scrivere codice nuovo |
| Heap SRAM ≈ 182–186 KB | interpreti (Program, Python) e fogli grandi solo con budget espliciti; sulla Plus i dati voluminosi vanno nel pool PSRAM |
| Record del workspace 59,75 KB sul Pico 2 W | documenti, programmi e fogli non possono stare nel record: vanno nei file (microSD o LittleFS) |
| Versione del record incrementata a ogni modalità | ogni modalità nuova oggi costringe a una versione e a una fixture: meglio sezioni estensibili |
| Stack di 32 KB con budget per il valutatore ricorsivo | gli interpreti devono essere iterativi o con profondità limitata |
| Nessun RTC | date fisse nei file; Financial (Days) richiede l'inserimento manuale delle date |

## 7. Problemi aperti

- A1–A6 di [ERRORI_v1.13.1.md](ERRORI_v1.13.1.md): record illeggibile dopo il
  ritorno a un firmware precedente, precisione di HypergeoPD con argomenti
  grandi, risultati sotto 1E−99, screenshot storici non rigenerabili, nessun
  collaudo, SOLVER con SolveN/FMin/FMax.
- microSD a settore singolo (niente CMD18/CMD25); ritardo di 16 ms della
  tastiera I2C; IRQ mascherati durante le scritture flash; animazioni di
  transizione non usate; gestione energetica assente.

## 8. Correzioni fatte alla documentazione in questo recap

Righe superate dal codice, corrette senza cambiare altro:

| File | Prima | Dopo |
|---|---|---|
| `docs/FEATURE_MATRIX.md` | «Simboli P, E e conversioni ►ENG/►ENG←: NOT STARTED» | TESTED ON SIMULATOR (v1.13.0), come la riga «ENG / ENG←» |
| `docs/FEATURE_MATRIX.md` | «Liste: List File 1–6 e `List "QTY"`: NOT STARTED» | TESTED ON HOST (v1.13.0), come le righe di Run-Matrix |
| `docs/GRAPH_PARITY.md` | «Comandi grafici in Run-Matrix: NOT STARTED» | TESTED ON SIMULATOR (v1.11.0, completati nella v1.13.0); il conteggio della sintesi di quel documento non è stato ricalcolato |
| `docs/README.md` | `main` indicato come tag `v1.13.1` | `main` = tag `v1.14.0` |

## 9. Prossimo passo

La sessione v1.15.0 descritta in
[../PROMPT_BASI_APPLICAZIONI.md](../PROMPT_BASI_APPLICAZIONI.md): budget per
scheda, workspace a sezioni estensibili (record v11), file tipizzati, guscio
comune delle modalità, servizi condivisi (grafico 2D con Dual Screen,
espressioni con variabili legate, lavoro a passi interrompibile con AC,
editor di testo, interprete) e nuclei puri testati per ogni modalità mancante.
Il collaudo fisico (HW-01…HW-189) resta la priorità appena la PicoCalc è
disponibile.
