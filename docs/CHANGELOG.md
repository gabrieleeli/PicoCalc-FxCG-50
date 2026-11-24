# Changelog

## v1.18.0 — Statistics alla parità, Distribution Base

Base: `main` = `aa2923c` (v1.17.0). Sulla **Pimoroni Pico Plus 2 W**
**Statistics** arriva alla **parità completa** con il capitolo 6 del manuale
fx-CG50 e **Distribution** (capitolo 18) entra nel registro come modalità
**Base**. Il **Raspberry Pi Pico 2 W resta congelato** alla v1.16.0 (immagine
identica salvo la stringa della versione). Analisi e tabella di parità:
[reports/ANALISI_FONTI_v1.18.0.md](reports/ANALISI_FONTI_v1.18.0.md);
rapporto: [reports/SVILUPPO_v1.18.0.md](reports/SVILUPPO_v1.18.0.md); guide:
[STATISTICS.md](STATISTICS.md), [DISTRIBUTION.md](DISTRIBUTION.md).
**Nessuna prova su hardware fisico**: procedura
[HARDWARE_TEST_v1.18.0.md](HARDWARE_TEST_v1.18.0.md) (HW-246…HW-271).

### Nuovo (solo Pimoroni Pico Plus 2 W)
- **Editor delle liste** su tre pagine (GRAPH CALC TEST INTR DIST / SRT-A SRT-D
  DEL DEL-ALL INS / ADD JUMP COLOR FILE CSV): ordinamento di più liste con la
  lista base, inserimento sopra e sotto, JUMP, **colori** delle liste,
  **formule nella testa** con ricalcolo automatico e Circular ERROR, List File
  dall'editor, CSV dalla barra.
- **StatGraph1–3** con SET (20 tipi: Scatter, xyLine, NPPlot, Pie, Hist,
  MedBox, N-Dist, Broken, 11 regressioni, Bar), DefG, SELECT, Mark Type, Graph
  Color, Color Link, Outliers, Start/Width, % Sto List, Stick Style; Stat Wind
  Auto/Manual, TRACE, V-Window, CALC dal grafico.
- **CALC**: 1-VAR e 2-VAR con le frequenze, Q1Q3 Type Std/OnData; **12
  regressioni** (anche a+bx, **Sin** e **Logistic**, calcolate a passi con AC)
  con r, r², MSe secondo il manuale, residui nella Resid List, COPY nell'elenco
  Y1–Y20 di Graph, DRAW.
- **TEST** (Z 1-S/2-S/1-P/2-P, t 1-S/2-S con Pooled, LinearReg t, χ² GOF con
  CNTRB, χ² 2-way con la matrice attesa, 2-Sample F, ANOVA a una e due vie con
  il grafico d'interazione), **INTR** (Z e t), **DIST** con le 24 funzioni (PD,
  CD, inverse; Data List/Variable; Ans, ListAns, Save Res; DRAW con l'area).
- **SET UP** di Statistics; **VARS › STAT** in Run-Matrix con X, Y, GRAPH,
  PTS, INPUT, TEST, INTR, DIST (valori dell'ultimo calcolo); x̂( e ŷ( per i 12
  tipi.
- **Distribution** (MENU › O, 24ª icona, Base): le 24 funzioni con Data
  Variable/List, Ans/ListAns, Save Res, DRAW, AC; sezione `0x0117`; stesso
  nucleo (`dist_core`) del DIST di Statistics.
- Record v12: sezioni `0x0101` (Statistics) e `0x0117` (Distribution).
- Nucleo `stat_engine` e `dist_core` verificati contro **scipy, numpy e
  statsmodels** (`tools/stat_reference.py`, 483 valori, tolleranze dichiarate).

### Corretto
- Trovati dal fuzzing (`fuzz_stats`) nel codice nuovo e corretti prima della
  pubblicazione: risultati infiniti con dati enormi (ora Math ERROR), InvN con
  la coda destra e un'area minuscola, F C.D con gradi di libertà enormi (NaN).

### Test
- `statistics_test` esteso (nucleo contro i riferimenti), **`statistics_flow_test`**
  nuovo (un caso per voce), Distribution in `apps_init_test`, `base_modes_test`,
  `base_flow_test`; `architecture_test` (Distribution Base solo sulla Plus,
  motore DIST unico, Pico 2 W con 23 modalità); fuzzer **`fuzz_stats`**;
  screenshot 148–174 (52, 53 e 84 rigenerati: editor e VARS › STAT nuovi).

## v1.17.0 — base di tutte le modalità

Base: `main` = `eb2338a` (v1.16.0). Ogni modalità del registro si apre, fa il
suo lavoro principale, salva e rimette a posto il suo stato ed esce senza
perdite di memoria. Sulla **Pimoroni Pico Plus 2 W** arrivano dieci modalità
nel nuovo stato **Base** (aperte e usabili, parità con la fx-CG50 non ancora
completa). Il **Raspberry Pi Pico 2 W resta congelato** alla v1.16.0 (solo una
correzione condivisa). Dalla v1.18.0 una modalità per versione arriva alla
parità completa, a partire da Statistics. Analisi:
[reports/ANALISI_FONTI_v1.17.0.md](reports/ANALISI_FONTI_v1.17.0.md);
rapporto: [reports/SVILUPPO_v1.17.0.md](reports/SVILUPPO_v1.17.0.md).
**Nessuna prova su hardware fisico**: procedura
[HARDWARE_TEST_v1.17.0.md](HARDWARE_TEST_v1.17.0.md) (HW-220…HW-245).

### Nuovo
- **Stato Base** nel registro (`AppStatus::Base`, `isOpen()`): la modalità si
  apre come una Available, con un triangolo ambra sull'icona e «EXE: apri
  (base)»; Available resta riservato alla parità completa.
- Dieci modalità **Base**, solo sulla Pimoroni Pico Plus 2 W, ognuna sul guscio
  comune con la sua sezione del record, il suo tipo di file in MEMORY › FILES e
  le operazioni lunghe interrompibili con AC:
  - **Spreadsheet** (MENU › 4): celle A1…Z999, numeri, testi, formule `=` con
    riferimenti relativi e assoluti e intervalli (Sum, Mean, Min, Max,
    Median…), ricalcolo automatico a passi, «Circular ERROR», EDIT › COPY,
    CUT, PASTE, FILL, CLEAR su blocchi, colonna ↔ List, file `.pcsheet`
    ([SPREADSHEET.md](SPREADSHEET.md));
  - **Dyna Graph** (MENU › 6): funzione con un coefficiente variabile,
    Start/End/Pitch di VARS › DYNA, animazione sulla V-Window di Graph con tre
    velocità, Cont/Stop, file `.pcdyna` ([DYNA_GRAPH.md](DYNA_GRAPH.md));
  - **Recursion** (MENU › 8): an+1 e an+2 con a0/a1, tabella con Σan, grafico
    dei punti e ragnatela; la formula an+1 della v1.13.0 resta la stessa
    variabile; file `.pcrecur` ([RECURSION.md](RECURSION.md));
  - **Conic Graphs** (MENU › 9): parabole, circonferenza, ellisse, iperboli;
    disegno, TRACE, G-SOLVE (vertice, fuoco, centro, raggio, asintoti,
    eccentricità); file `.pcconic` ([CONIC_GRAPHS.md](CONIC_GRAPHS.md));
  - **Program** (MENU › B): programmi C, C++ e C# sul nucleo della v1.15.0,
    elenco, NEW con il modello del dialetto, editor con la sintassi colorata,
    RUN con input e AC, errori con riga, colonna e cursore, grafici in Graph;
    file `.pcprog` (dialetto nella prima riga) e `.c/.cpp/.cs` ([PROGRAM.md](PROGRAM.md));
  - **Financial** (MENU › C): interesse semplice e composto sulle variabili
    di VARS › TVM (BGN/END), flussi di cassa (NPV, IRR, PBP, NFV),
    ammortamento, conversione dei tassi, giorni tra due date, →Ans; file
    `.pcfin` ([FINANCIAL.md](FINANCIAL.md));
  - **Link** (MENU › E): invio e ricezione su USB seriale del workspace e dei
    file in pacchetti `.pclink` (trame con CRC-32, conferme, ritrasmissione,
    Time Out, AC), APPLY con conferma; strumento **`tools/picocalc_link.py`**
    per il PC (send, receive, list) ([LINK.md](LINK.md));
  - **Geometry** (MENU › H): punti, segmenti, rette, circonferenze, poligoni;
    selezione, MOVE, MEASURE, ZOOM, PAN; file `.pcgeo` ([GEOMETRY.md](GEOMETRY.md));
  - **Picture Plot** (MENU › I): sfondo `.pcpict`, punti numerati con il
    cursore, List 1/List 2, regressione di Statistics sopra lo sfondo; file
    `.pcplot` ([PICTURE_PLOT.md](PICTURE_PLOT.md));
  - **Python** (MENU › L): **MicroPython v1.24.1** (licenza MIT,
    `third_party/micropython`), SHELL con blocchi e cronologia, script `.py`,
    `input()`, AC → KeyboardInterrupt, esecuzione a passi su una pila propria,
    heap separato (512 KB in PSRAM, 48 KB in SRAM), modulo `casioplot`
    ([PYTHON.md](PYTHON.md)).
- Record del workspace **v12** (solo Plus): firma di scheda e versione, CRC-32
  per sezione; una sezione di modalità danneggiata viene isolata (la modalità
  riparte dai valori iniziali con «Data ERROR: dati danneggiati di …») invece
  di rendere illeggibile il record; migrazione dal record v11 della v1.16.0
  ([STORAGE.md](STORAGE.md)).
- Strisce dell'eActivity Conics, Recur, Dynamic Graph, Financial,
  SpreadSheet, Geometry e Picture Plot disponibili sulla Plus (parti `0x40 +
  AppId`).
- Servizio **ModeCore** (nuclei puri delle modalità Base, `mode_codec.hpp`);
  `ui::TypedFileMenu` e campi dei moduli nel guscio comune; porta USB seriale
  nella piattaforma (`Platform::serialPort`, solo Plus).

### Corretto
- Conversion non segnava Ans da salvare: un risultato portato in Ans si
  perdeva con un riavvio prima di un altro salvataggio (trovato da
  `apps_init_test`; correzione condivisa con il Pico 2 W).

### Cambiato
- Tipi di file della Plus: Spreadsheet usa `.pcsheet` (prima `.sheet`, mai
  scritto), Program apre anche `.pcprog`; nuovi tipi Dyna Graph, Recursion,
  Conic Graphs, Financial e Link ([FILE_FORMATS.md](FILE_FORMATS.md)). Il
  Pico 2 W tiene la tabella della v1.16.0.
- La schermata «non disponibile» di E-CON4 sostituisce lo screenshot 19 (le
  modalità aperte non hanno più quella schermata); MEMORY › FILES e il menu
  delle strisce mostrano i tipi nuovi della Plus (screenshot 105, 106, 115).

### Test
- `apps_init_test` (due profili): ogni modalità aperta in sette casi
  d'inizializzazione con sei controlli, comprese le perdite dell'allocatore.
- `base_modes_test`, `base_flow_test`, `workspace_v117_test`, `link_tool_test`
  (solo Plus); fuzzer `fuzz_sheet`, `fuzz_link`, `fuzz_modes`, `fuzz_python`;
  `architecture_test` controlla lo stato Base, ModeCore e il congelamento del
  Pico 2 W; 20 screenshot nuovi (128–147).

## v1.16.0 — eActivity su entrambe le schede; Raspberry Pi Pico 2 W congelato

Base: `main` = `08a4e6a` (v1.15.0). Milestone **M2**: eActivity alla parità
con il capitolo 10 del manuale fx-CG50 sul **Raspberry Pi Pico 2 W** e sulla
**Pimoroni Pico Plus 2 W**. Con questa versione il Pico 2 W è **congelato**:
le versioni successive aggiungono modalità solo alla Plus. Analisi:
[reports/ANALISI_FONTI_v1.16.0.md](reports/ANALISI_FONTI_v1.16.0.md);
rapporto: [reports/SVILUPPO_v1.16.0.md](reports/SVILUPPO_v1.16.0.md).
**Nessuna prova su hardware fisico**: procedura
[HARDWARE_TEST_v1.16.0.md](HARDWARE_TEST_v1.16.0.md) (HW-202…HW-219).

### Nuovo
- **eActivity** (MENU › 3), Available su entrambe le schede:
  - elenco dei documenti `PICOCALC/EACT/*.pceact` con cartelle fino a tre
    livelli; OPEN, NEW (nomi A–Z 0–9 { } ' ~), DELETE («Delete eActivity?»),
    SEARCH («Not Found»), MEMO con JUMP, EDIT, DETAIL, DELETE, DEL-ALL;
  - righe di **testo** (255 byte, a capo sulle parole, EXE a capo, A⇔a, CHAR,
    AC sulla riga visualizzata) con **espressioni Math** incorporate (MATH e
    modelli, senza a capo);
  - righe di **calcolo** valutate dal nucleo di Run-Matrix (forme esatte, S⇔D
    da OPTN) e **ricalcolo** delle righe seguenti fino alla riga di **stop**, a
    passi, interrompibile con AC («Break»);
  - INSERT (TEXT, CALC, STOP, PICTURE), DEL-LINE con conferma, JUMP,
    SHIFT ↑/↓, COLOR › MARKER e › CHAR, **memo** (una per riga, colore,
    Catalog, VIEW, DETAIL, «Only one memo allowed per line.»);
  - **immagini** `.pcpict` di `PICOCALC/PICT` e `PICOCALC/CAPT` con FORMAT
    (Line Style, Line Color, Clear);
  - **strisce** (24 tipi del manuale): RUN, Graph, Graph Editor, Table
    Editor, Stat Graph, List Editor, Solver, Notes, Matrix Editor, Vector
    Editor, Simul Equation, Poly Equation, 3D Graph, 3D Graph Editor; ogni
    striscia conserva i propri dati, la modalità si apre con la barra di stato
    azzurra e torna con SHIFT ' o MENU; SHIFT , cambia applicazione; FILE ›
    SIZE; le strisce di modalità non disponibili (o di versioni future) sono
    conservate e mostrate «non disponibile»;
  - memorie Mat/Vct/List **separate per documento** ('MAT/VCT, 'LIST);
    SAVE, SAVE·AS con sovrascrittura, OPT, CAPACITY, «Salvare il file?»;
  - limite del documento aperto: **24 KB** sul Pico 2 W, **256 KB** sulla
    Plus; oltre, «Memory ERROR» e il documento resta com'era.
- Record del workspace: sezione `0x0102` (cartella, ultimo file, dati del menu
  principale durante un documento o una striscia, rimessi a posto dopo
  un'interruzione di alimentazione). Formato del documento in
  [FILE_FORMATS.md](FILE_FORMATS.md).
- Codifica del workspace **per parti** (Graph, Table, Lists, Equation,
  Matrices, Graph3D, History) con la garanzia forte.
- Sessione da striscia nel `Context` (le modalità aprono la schermata della
  striscia senza conoscere eActivity).

### Cambiato
- Barra F1–F6 a più pagine: «▶» per la pagina seguente (il font non ha «▷»).
- Pico 2 W: eActivity e i sorgenti fuori dai percorsi di calcolo e di disegno
  sono compilati con `-Os`; immagine più piccola della v1.15.0 nonostante
  eActivity ([BUILD_STATUS.md](BUILD_STATUS.md)).
- Errori della fx-CG50 come titolo della finestra: «Not Found», «No MEMO»,
  «Invalid Name», «Break».

### Test
- `eactivity_test` (una prova per ogni voce E01–E56, due profili),
  `fuzz_eactivity` (documenti e controller guidato da tasti); `architecture_test`
  fallisce se le modalità Available del Pico 2 W cambiano dopo la v1.16.0;
  20 screenshot nuovi (108–127), i 97 precedenti identici.

## v1.15.0 — fondamenta comuni delle modalità

Base: `main` = `72d0921` (v1.14.0 + recap). Nessuna modalità nuova: le
modalità Available sono quelle della v1.14.0 su entrambe le schede. Prima
milestone (M1) del piano che porta il Pico 2 W alla v1.16.0 con eActivity e la
Pimoroni Pico Plus 2 W fino alla parità delle modalità realizzabili. Analisi:
[reports/ANALISI_FONTI_v1.15.0.md](reports/ANALISI_FONTI_v1.15.0.md);
rapporto: [reports/SVILUPPO_v1.15.0.md](reports/SVILUPPO_v1.15.0.md).
**Nessuna prova su hardware fisico**: procedura
[HARDWARE_TEST_v1.15.0.md](HARDWARE_TEST_v1.15.0.md) (HW-190…HW-201).

### Nuovo
- **Modalità e servizi per scheda** (F1): opzioni `FXAI_MODE_<NOME>` e
  `FXAI_SERVICE_<NOME>` in `cmake/fxai_sources.cmake`, campo `boards` nel
  registro; sul Pico 2 W le modalità della Plus mostrano «Non disponibile sul
  Raspberry Pi Pico 2 W: richiede la Pimoroni Pico Plus 2 W (flash e RAM).»
- **Controllo dell'immagine bloccante**: superare la partizione o il margine
  (32 KiB sul Pico 2 W, 10 % sulla Plus) è un errore del build, come un oggetto
  o un simbolo di una modalità esclusa nella mappa del linker.
- **Record del workspace v11** (F2): coda di sezioni TLV; le sezioni di
  modalità che il firmware non ha (o di versioni più nuove) sono conservate e
  riscritte intatte; lettura v1–v10 invariata (nuova fixture della v1.14.0).
- **File tipizzati** (F3): tabella dei tipi per scheda (`PICOCALC/EACT` su
  entrambe; `PROG`, `SHEET`, `PY`, `GEOM`, `PPLOT` sulla Plus), intestazione con
  versione e CRC, testo UTF-8 per sorgenti e script, API comune atomica,
  `FileDialog` unico (regola dei nomi, sovrascrittura, ricerca, conferma),
  **MEMORY › F3 FILES** con i file per tipo ([FILE_FORMATS.md](FILE_FORMATS.md)).
- **Guscio comune** `ModeShell` (F4): pila di schermate, barra F1–F6 da tabella
  con sottomenu/pagine/SHIFT, lista, conferma, richiesta di valore, SET UP da
  tabella, VARS e CATALOG dai resolver; **TABLE** portata sul guscio con gli
  stessi screenshot.
- **Editor di testo multiriga** (F5) con a capo, ricerca, salto alla riga,
  clipboard di testo del `Context`, limiti per scheda.
- **StepTask** (F6): lavoro a passi con budget di unità e di tempo, AC →
  «Break», avanzamento; il calcolo progressivo del **3D Graph** passa al
  runner con lo stesso budget (screenshot identici).
- Solo **Pimoroni Pico Plus 2 W**: grafico 2D condiviso a schermo diviso (F7),
  `BindingResolver` con celle A1…Z999, intervalli e nomi legati dallo stesso
  lexer e «Circular ERROR» (F8), **nucleo dell'interprete dei programmi in C,
  C++ e C#** (F9, decisione dell'utente per Program).

### Strumenti
- `tools/map_sizes.py` (dimensioni per modulo dalla mappa del linker),
  `tools/sim_compare.py` (confronto pixel per pixel di tutti gli screenshot e
  compressione senza perdita); fuzzing: `fuzz_typedfile`, `fuzz_texteditor`,
  `fuzz_program`, `fuzz_workspace` con le sezioni, `fuzz_expr` con celle e nomi
  legati; `FUZZ_ONLY` in `run_fuzz.sh`.

## v1.14.0 — completamento totale di 3D Graph

Base: tag `v1.13.1` (commit `62b3f8f` = `main`). Il **3D Graph** raggiunge la
parità con la fx-CG50 (capitolo 16 del manuale software, OS 3.x): ogni voce
della tabella di parità è presente o è una differenza inevitabile
documentata. Analisi: [reports/ANALISI_FONTI_v1.14.0.md](reports/ANALISI_FONTI_v1.14.0.md);
rapporto e tabella finale: [reports/SVILUPPO_v1.14.0.md](reports/SVILUPPO_v1.14.0.md);
guida: [GRAPH3D.md](GRAPH3D.md). **Nessuna prova su hardware fisico**:
procedura [HARDWARE_TEST_v1.14.0.md](HARDWARE_TEST_v1.14.0.md) (HW-170…HW-189).

### Nuovo
- **Tipi di grafico**: modelli Line (EXPRESS, VECTOR, P&V, POINTS), Plane
  (EXPRESS, VECTOR, POINTS), Sphere (FACTOR, EXPAND), Cylinder, Cone con le
  condizioni «Invalid Setting» del manuale; input diretto Z=, Param (Xst,
  Yst, Zst in S e T), Rot X e Rot Y (solidi di rotazione); fino a tre grafici
  di tipo qualsiasi nella stessa scena.
- **Lista**: `1:`…`3:` con il `:` evidenziato, SELECT, DELETE con conferma,
  TYPE e ←/→, schermata Select Template, 3D-GMEM STORE/RECALL 1–20, DRAW o
  EXE, FORMAT con Line Color e Area Color (Clear compreso).
- **V-Window 3D**: X/Y/Z, Xgrid/Ygrid (2–50), S/T con Sgrid/Tgrid, Angle θ e
  φ in gradi, INITIAL, 3D-VMEM 1–6, «Out of Domain» e «Invalid Setting».
- **SET UP** della modalità: Axes, Box, Label, Plot/LineCol, Input/Output,
  Angle, Display; tasti • e , sul grafico.
- **Grafico**: rotazione con i tasti (θ e φ della V-Window), ROTATE
  automatico (due giri, AC), ZOOM IN/OUT/VIEW-X/VIEW-Y/VIEW-Z/ORIGINAL,
  SKETCH (Cls, Plot, Text), PICTURE STORE (Pict 1–20), CAPTURE (SHIFT 7),
  G⇔T; superficie nell'Area Color con il reticolo nel Line Color, scatola,
  etichette e indicatore degli assi.
- **TRACE** su ogni tipo con X/Y/Z, Next/Back e finestra delle coordinate.
- **G-Solve**: CROSS (sezioni analitiche X/Y/Z, UP/DOWN, viste), INTSECT
  (retta–retta, retta–piano, piano–piano; NOT FOUND, INFINITE) e RELATION
  (intersecanti, perpendicolari, parallele, sghembe, coincidenti…).
- **MEMORY**: cancellazione separata di 3D Graph, 3D G-Mem e 3D V-Win.

### Motore e memoria
- `graph3d_model` (nuovo, puro): modello, validazione, geometria canonica,
  INTSECT/RELATION analitici, codifica delle G-Mem e del record.
- `graph3d` (riscritto): reticoli di ogni tipo, taglio esatto al box,
  proiezione θ/φ, z-buffer condiviso da superfici, reticoli e rette (PSRAM)
  o pittore (SRAM), sezioni CROSS; calcolo progressivo interrotto da AC;
  immagine della scena in PSRAM per TRACE/SKETCH.
- Limiti: 50 punti per lato con la PSRAM, 25 senza; 3D G-Mem 64 KB / 12 KB;
  Memory ERROR senza memoria trattenuta.

### Workspace
- **Record v10**: il blocco 3D della versione 5 non è più scritto in mezzo al
  record; il nuovo stato del 3D Graph va in coda. I record v5–v9 vengono
  convertiti (Z1–Z3 → grafici Z=, griglia in punti, yaw/pitch → θ/φ con la
  stessa orientazione). Fixture nuova `tests/fixtures/workspace_v1_13_1_graph3d.inc`
  generata con i sorgenti della v1.13.1.

### Test e strumenti
- `tests/graph3d_v114_test.cpp` (541 controlli per profilo), `graph3d_test`
  portato alla nuova API, `oom_test`, `architecture_test` (gruppo 3D),
  `limits_smoke`; fuzzing `tools/fuzz/fuzz_graph3d.cpp` (ha trovato una
  conversione di NaN in `int` con coordinate enormi, corretta con un test);
  simulatore `tests/sim/graph3d_v114.txt` (screenshot 88–104) e
  `tests/sim/graph3d.txt` (40–43 rigenerati).

### Cambiamenti di comportamento
- La lista del 3D Graph è vuota al primo avvio (prima Z1 = sin(x)cos(y)).
- EXE sulla lista disegna (come la fx-CG50); per modificare una riga ←/→ o
  TYPE; digitando su una riga vuota o Z= si apre l'input di Z= (estensione).
- La griglia è il numero di punti (2–50) per asse; prima celle 5–128.
- AUTO sull'intervallo z è in OPTN › Z-AUTO (estensione).

## v1.13.1 — versione di correzione

Base: tag `v1.13.0` (commit `a72d901` = `main`). **Nessuna funzione nuova**:
solo correzioni di errori, incoerenze e documentazione. Elenco completo con
gravità, riproduzione e causa: [reports/ERRORI_v1.13.1.md](reports/ERRORI_v1.13.1.md);
rapporto: [reports/SVILUPPO_v1.13.1.md](reports/SVILUPPO_v1.13.1.md).
**Nessuna prova su hardware fisico**: procedura [HARDWARE_TEST_v1.13.1.md](HARDWARE_TEST_v1.13.1.md).

### Correzioni
- **Workspace** (E01, alta): un record v1–v8 con un coefficiente di EQUATION
  non finito, un'equazione del Solver oltre 255 caratteri o una lista con un
  elemento non finito veniva caricato, ma il record salvato subito dopo era
  rifiutato all'avvio successivo (workspace azzerato). Ora si applicano gli
  stessi controlli alla lettura; i record validi si leggono come prima.
- **Distribuzioni** (E02, E03, E14, E15, E16): `HypergeoCD`/`InvHypergeoCD`
  con x, n o M grandi non terminavano (ora somma sul solo supporto, al più
  10⁶ termini); `InvBinomialCD`/`InvPoissonCD`/`InvHypergeoCD` in tempo
  lineare invece che quadratico (stessi risultati); `InvGeoCD` con p piccolo
  non terminava e `GeoCD`/`GeoPD` perdevano precisione; `InvPoissonCD` oltre
  10⁶ restituiva 1000000 (ora Argument ERROR); `HypergeoPD` poteva superare 1
  con argomenti grandi.
- **EQUATION** (E04): un coefficiente con una funzione non polinomiale di `i`
  (`1E10+Abs(i)`, `Conjg`, `ReP`…) poteva avere una forma esatta sbagliata.
- **nCr / nPr** (E05): ammessi n < 1E10 come sulla fx-CG50 (prima n ≤ 100000),
  risultato < 1E100.
- **CSV** (E06): righe e colonne si controllano prima di allocare; un file
  con troppe righe o colonne dà Dimension ERROR invece di esaurire la memoria
  del Pico 2 W.
- **File** (E07): un salvataggio dopo un salvataggio interrotto completa prima
  quello interrotto: due interruzioni di seguito non possono più lasciare un
  file troncato.
- **Statistics** (E08, E09): intervallo t con C-Level vicino a 1 e `InvTCD`
  nelle code estreme corretti, `InvTCD(0.5, df)` = 0; 1-Variable con le
  frequenze senza replicare i dati in memoria.
- **Σ / Π e Ran#** (E10, E12): estremi oltre 9,2E18 e `Ran# n` fuori intervallo
  non provocano più conversioni indefinite (sulla PicoCalc Σ poteva non
  terminare).
- **VARS** (E17): V-WIN e TABLE mostrano Xmin…Tptch, TblStart… invece dei nomi
  interni.

### Test e strumenti
- Nuova suite `fixes_v1131_test` (un gruppo per errore, ognuno fallisce con
  la v1.13.0); build con sanitizer: UBSan bloccante (`-fno-sanitize-recover`,
  E11), prima un comportamento indefinito non faceva fallire i test.
- Fuzzing con libFuzzer + ASan/UBSan di parser, workspace, CSV, `.pcgraph`,
  EQUATION e sequenze di tasti: strumenti in `tools/fuzz` (`run_fuzz.sh`).

### Problemi aperti
- A1–A6 di [reports/ERRORI_v1.13.1.md](reports/ERRORI_v1.13.1.md#problemi-rimasti-aperti),
  tra cui il SOLVER di EQUATION con SolveN/FMin/FMax nell'equazione (Time Out
  solo dopo un calcolo lungo) e nessun collaudo su hardware.

### Documentazione
- `TESTING.md` con i numeri reali (E13); script del simulatore aggiornati al
  nuovo ordine di OPTN e screenshot 17, 18, 51, 55, 60 rigenerati (E18).

## v1.13.0 — completamento totale di Run-Matrix (parità con la fx-CG50)

Base: tag `v1.11.0` (commit `f6933fd`), unito con la v1.12.0 di EQUATION (commit `51691de` di `main`). Analisi delle
fonti e tabella di parità iniziale: [reports/ANALISI_FONTI_v1.13.0.md](reports/ANALISI_FONTI_v1.13.0.md);
rapporto con la tabella finale: [reports/SVILUPPO_v1.13.0.md](reports/SVILUPPO_v1.13.0.md).
**Nessuna prova su hardware fisico**: procedura [HARDWARE_TEST_v1.13.0.md](HARDWARE_TEST_v1.13.0.md).

### Calcolo (stesso parser, stesso AST, stesso valutatore)
- **CALC**: Solve con limiti e forma `a=b`, **SolveN** (fino a 10 soluzioni in
  List Ans), **FMin/FMax** (`{x, f(x)}`), **Σ/Π con variabile e passo**, ∫ con
  tolleranza, d²/dx² anche come modello, funzioni vietate negli argomenti di
  Solve/SolveN/FMin/FMax (manuale 2-28).
- **PROB**: Ran# n (sequenze 1–10, `Ran# 0`), RanInt#/RanNorm#/RanBin# con n,
  **RanList#**, **RanSamp#**, **P( Q( R( t(**.
- **NUMERIC** Rnd (cifre del display); **ANGLE** ʳ ᵍ ° ′ ″, Pol/Rec in List
  Ans; **COMPLEX** ∠ in input, ►r∠θ / ►a+bi; Arg elemento per elemento.
- **STAT** in Run-Matrix: x̂( ŷ( (ultima regressione di Statistics), 24
  distribuzioni (Norm, t, χ², F, Binomial, Poisson, Geo, Hypergeo: PD, CD,
  Inv…CD), S-Dev(, Var(, Mean/Median con frequenze.
- **TVM** (22 funzioni) e **CONVERT** (11 categorie, 103 unità).
- Simboli **P** ed **E**, **ENG / ENG←**, overflow |x| ≥ 10¹⁰⁰ (Math ERROR).
- **Simplify Manual**: frazioni non ridotte per tutte le operazioni (m.c.m.
  dei denominatori per + e −, prodotti per × e ÷); ▶Simp su una frazione
  irriducibile senza «F=».
- **Base-N**: Bin a 16 bit; variabili A–F in Hex distinte dalle cifre (`v_a`,
  VARS o Alt+lettera, sottolineate); liste e matrici nella base della riga.
- Nuovo modulo del motore `calc_library` (unità, distribuzioni, TVM, nomi,
  CATALOG): puro calcolo.

### Run-Matrix
- Istruzioni multiple **`:`** e **`◢`**; **stringhe** e **Str 1–20**;
  **function memory f1–f20** (OPTN › FUNCMEM); variabili **r** e **θ**;
  variabili di sistema assegnabili (`10→Xmax`, Xfct, TblStart, D Start,
  R Start, a0…, n I% PV PMT FV P/Y C/Y, `"formula"→an+1`).
- **List "nome"**, **List File 1–6** (SET UP, budget comune), **Sub Name**,
  `n→Dim List k`, `Fill(v, List/Mat)` che modifica la memoria.
- Matrici: forme esatte **senza il limite di 64 elementi** (budget per
  scheda), celle decimali conservate, **Swap / \*Row / \*Row+ / Row+**.
- VARS completo: V-WIN (anche in scrittura), FACTOR, STAT, GRAPH, DYNA, TABLE
  (Result), RECURSION, EQUATION (SimRes, SimCoef, PlyRes, PlyCoef della v1.12.0), TVM, Str.
- **OPTN nell'ordine della fx-CG50**; **CATALOG** (SHIFT 4); **CAPTURE**
  (SHIFT 7) con RclCapt.
- Messaggi dell'Appendice α: `Argument ERROR` al posto di «Type ERROR»,
  `Complex Number In List / In Matrix`, `Can't Solve!`, `Iteration ERROR`,
  `Range ERROR`.

### Comandi grafici
- **PxlTest(**, **Graph Y> Y< Y≥ Y≤ X> X< X≥ X≤**, **G∫dx**, **SketchNormal /
  Thick / Broken / Dot / Thin**, **StoPict / RclPict / BG-Pict / RclCapt**,
  **DrawGraph, DrawStat, DrawDyna, DrawR-Con/Plt, DrawWeb, DrawFTG-Con/Plt**,
  **G-SelOn/Off**; disegnati dal motore e dal renderer di GRAPH.
- I grafici dei comandi si salvano nei progetti `.pcgraph` (righe `c=`).

### Editor Math/Line
- Conversione testo ↔ albero esatta nelle due direzioni (corpus di test);
  parentesi implicite di una frazione accostata a un operando
  (`6÷2[1/3]` = 9, come sulla calcolatrice); testo Line con le parentesi
  minime; modelli **d²/dx²** e **Σ/Π con la variabile**; nomi dei comandi
  grafici come un solo token.
- Font: glifi ◢ ʳ ᵍ ′ ″ ŷ x̂ x̄ ȳ Å disegnati dal progetto.

### Workspace
- **Record v9** in coda al v7 (List File, Sub Name, regressione, r e θ, Str,
  function memory, variabili di Dyna/Recursion/TVM, formula an+1, celle
  decimali, ENG, ◢ e risultati stringa della cronologia); v1–v8 letti senza
  perdite; fixture della v1.11.0 e della v1.12.0 (migrazioni v7→v9 e v8→v9).

### Test
- Nuove suite `calc_v112_test`, `run_matrix_v112_test`, `graph_cmd_v112_test`,
  `editor_v112_test`; `architecture_test` più severo (parser unico,
  `calc_library` puro, separazione di Run-Matrix ampliata); script del
  simulatore `tests/sim/run_matrix_v112.txt` (screenshot 77–87).

### Firmware
- Il motore dei grafici usato da DrawFTG e da TABLE Result (VARS) è allocato
  sullo heap: i frame dei comandi grafici e di `Workspace::special` sono
  al massimo 400 e 176 byte (`-fstack-usage`) sullo stack da 32 KB.
- Build delle due schede senza warning: immagine di 1 841 608 byte sulla Plus
  (44 % di 4 MB) e di 1 832 772 byte sul Pico 2 W (87 % di 2 MB).

## v1.12.0 — completamento di EQUATION: SIML, POLY e SOLVER come la fx-CG50

Base: tag `v1.11.0` (commit `f6933fd`, uguale a `origin/main`). Analisi delle
fonti con la tabella di parità: [reports/ANALISI_FONTI_v1.12.0.md](reports/ANALISI_FONTI_v1.12.0.md);
rapporto: [reports/SVILUPPO_v1.12.0.md](reports/SVILUPPO_v1.12.0.md); guida:
[EQUATION.md](EQUATION.md). **Nessuna prova su hardware fisico**: procedura
[HARDWARE_TEST_v1.12.0.md](HARDWARE_TEST_v1.12.0.md).

### EQUATION (manuale fx-CG50 4-1…4-5, 1-29, 1-33)
- Schermate del manuale: "Select Type" (SIMUL, POLY, SOLVER; INEQ come
  estensione), "Number of Unknowns?" / "Degree?", griglia dei coefficienti con
  intestazione `aₙX+bₙY=cₙ`, parentesi di matrice, cella corrente e valore in
  forma naturale in basso a destra; tasti **SOLVE DELETE CLEAR EDIT**, EXE
  verso la cella successiva, EXIT che annulla l'immissione, **REPEAT**.
- **Coefficienti come espressioni** dello stesso parser di Run-Matrix
  (frazioni, radici, π, A–Z, Ans, complessi) conservati **in forma esatta**
  (anche complessa) e modificabili con EDIT; editor Math/Line di Run-Matrix
  (nuovo widget `MathField`).
- **SIML** 2–6 incognite: Gauss-Jordan esatto (frazioni, radicali, π,
  complessi) o in virgola mobile con equilibratura e pivoting; soluzioni in
  colonna con la forma naturale; **No Solution**; **Infinitely Many
  Solutions con la formula** (`X=−½i`, `Y=1.4−1.8i−(2+i)Z`, `Z=Z`); decimali
  solo per i risultati che dipendono da decimali digitati.
- **POLY** grado 2–6: reali in ordine decrescente poi complesse, radici
  multiple una volta con **×k**, Complex Mode **Real / a+bi / r∠θ** (anche
  esatto: `1∠π`), radici esatte (`(1+√5)/2`, `(−3+√11 i)/2`, `√2`, radici
  razionali) con scomposizione senza quadrati (Yun), Underflow / Ma ERROR
  dell'intervallo di calcolo.
- **SOLVER**: equazione in input naturale, tabella delle variabili (A–Z
  nell'ordine dell'equazione), **Lower/Upper**, scelta dell'incognita, F6
  SOLVE, RECALL, DELETE; risultato `V=…`, **Lft**, **Rgt**, **Retry**;
  messaggi **No Variable**, **Can't Solve!**, **Time Out**; la variabile
  risolta resta memorizzata.
- **S⇔D** (F5) sulle soluzioni; formati Norm/Fix/Sci/Eng anche in 2D;
  risultati lunghi scorrevoli con ◀ ▶.
- **VARS › EQUATION** in Run-Matrix: `SimRes`, `SimCoef`, `PlyRes`, `PlyCoef`
  (nomi dello stesso lexer, risolti dal workspace con `CollectionResolver`,
  risultato in MatAns; la Rref con No Solution / Infinitely Many Solutions).

### Comune
- **SET UP** aperto da una modalità: EXIT torna a quella modalità (prima al
  menu principale); EQUATION riprende dalla stessa schermata.
- Titoli degli errori senza "ERROR" della fx-CG50 (Can't Solve!, Time Out,
  No Variable, Underflow) nella finestra di errore.
- **Correzione dell'editor Math** (tutte le modalità): un modello di frazione
  seguito da un fattore implicito (`½X`) era letto come `1/(2X)`; ora il testo
  lineare è `((1)/(2))X` e si reimporta come frazione. `GT²` eleva al quadrato
  solo `T` (prima `(GT)²`), come sulla fx-CG50. Le espressioni già salvate
  conservano il loro testo.

### Dati
- Record del workspace **v8**: EQUATION in coda (coefficienti `eq::Number`
  con forma esatta, griglia SIML fissa 6×7, equazione, Lower/Upper, variabile
  del Solver, ultime soluzioni). I record v1–v7 si leggono senza perdite:
  coefficienti convertiti con il decimale più corto, celle del sistema attivo
  alla loro riga e colonna, valore iniziale del Solver nella variabile X.
  Fixture della v1.11.0 (`tests/fixtures/workspace_v1_11_0.inc`).
- `limits.hpp`: `kEquationUnknowns`, `kEquationDegree`,
  `kEquationExpression`, `kSolverEvaluations`.

### Test
- Nuova suite `equation_v112_test` (esempi del manuale, casi numerici
  difficili, migrazione v7→v8, VARS, editor); `architecture_test` più severo
  (servizio EquationCore, nessuna lettura propria dei numeri, nessun percorso
  numerico separato, Run-Matrix solo tramite `CollectionResolver`);
  `apps_flow_test` adattato alle nuove schermate. Script
  `tests/sim/equation.txt`, screenshot 08 e 62–76.

## v1.11.0 — completamento di Run-Matrix: List 1–26, CSV, MatAns/ListAns, Line (INS, ⌟), simboli ingegneristici, Simp, Base-N, comandi grafici

Base: tag `v1.10.0` (commit `dfdb08b`, uguale a `origin/main`). Analisi delle
fonti: [reports/ANALISI_FONTI_v1.11.0.md](reports/ANALISI_FONTI_v1.11.0.md);
rapporto: [reports/SVILUPPO_v1.11.0.md](reports/SVILUPPO_v1.11.0.md).
**Nessuna prova su hardware fisico**: procedura
[HARDWARE_TEST_v1.11.0.md](HARDWARE_TEST_v1.11.0.md).

### Liste
- **List 1–26** nei calcoli (`List 26[3]`, `→List 20`), in Statistics (editor
  con 26 colonne, SET XList/YList per CALC, GRAPH e TEST) e in GRAPH/TABLE
  (grafici da lista, variabile della tabella, LISTMEM, progetti `.pcgraph`).
- **Sottonomi** delle liste (riga SUB dell'editor, 8 caratteri) salvati nel
  workspace.
- Budget di tutte le liste per scheda (`limits::kListStoreElements`): 1 530
  elementi sul Pico 2 W (l'occupazione massima della v1.10.0), 20 000 sulla
  Pico Plus 2 W; `Memory ERROR` oltre, senza modifiche.

### File CSV
- Nuovo modulo `csv_io` (motore, nessuna UI): CSV LOAD / SAVE·AS dall'editor
  delle liste (OPTN) e dall'editor MAT/VCT (F4), sulla microSD e sulla flash
  interna (LittleFS), cartella `PICOCALC/CSV`, scrittura atomica.
- Separatore `,`/`;` e simbolo decimale `.`/`,` (SET UP e CSV SET);
  intestazione con virgolette ignorata; CR/LF/CRLF; errori `Format ERROR`,
  `Dimension ERROR`, `Memory ERROR`, `Storage ERROR`, `Complex Number In Data`.

### Run-Matrix
- **Schermate MatAns / VctAns / ListAns**: i risultati oltre 6×6 (o 20
  elementi) si aprono in una griglia scorrevole con la cella corrente; F4
  dalla cronologia.
- **Editor MAT/VCT**: celle con espressioni, forme esatte (`1/3`, `2√3`, `π/4`)
  e numeri complessi; colonne scorrevoli con ◀ ▶ e indicazione `r c`; F4 CSV.
  Le memorie fino a 64 elementi conservano le forme esatte (`Mat A×6`
  esatto).
- **Modalità Line**: INS (SHIFT DEL) per la sovrascrittura (`OVR`), frazioni
  `⌟` (`2⌟3`, `1⌟2⌟3`) in input e nei risultati, risultati più lunghi della
  riga scorrevoli con ←/→.
- **SHIFT+INS** in Math: il valore a destra del cursore diventa l'argomento
  del modello successivo (`1+(2+3)+4` → `1+√(2+3)+4`).
- **Simboli ingegneristici** (OPTN › ESYM: m µ n p f k M G T) e display **Eng**
  con i simboli (`999k+25k` = `1.024M`).
- **▶Simp** e **▶Simp n** con il fattore mostrato (`5/20  F=3`), impostazione
  **Simplify** Auto/Manual.
- **Base-N intera** (SET UP › Mode: Dec, Hex, Bin, Oct): interi a 32 bit in
  complemento a due, And/Or/Xor/**Xnor** bit a bit, **Not**, **Neg**, prefissi
  d h b o, ▶Dec/▶Hex/▶Bin/▶Oct; ogni riga ricorda la sua base.
- **Comandi grafici** (OPTN › SKETCH): Cls, ViewWindow, Graph Y=/r=/X=/(X,Y)=,
  Plot/PlotOn/PlotOff/PlotChg, Line, F-Line, Circle, Vertical, Horizontal,
  Text, PxlOn/PxlOff/PxlChg, Tangent, Normal, Inverse; li esegue il nuovo
  interprete `graph_commands` e li disegna GRAPH, che si apre sul grafico ed
  EXIT torna a Run-Matrix.

### Motore
- Un solo lexer/parser/valutatore: nuovi token `#k`… (simboli ingegneristici,
  postfissi), letterali `a⌟b`/`a⌟b⌟c`, operatore `Xnor`, funzione `Neg(`,
  prefisso `0d`; `parseExpression(testo, base)` ed `EvalContext::integerBase`
  per la modalità Base-N; `CalcValue::exact` per le forme esatte delle memorie.
- `FormattedNumber::symbol`: Eng con i simboli f…T.
- Glifo `⌟` (U+231F) disegnato dal progetto nei due font (`tools/gen_fonts.py`,
  `MANUAL_GLYPHS`).

### Workspace
- Record **versione 7**: Base-N, Simplify, formato CSV, liste di Statistics,
  List 7–26, sottonomi, forme esatte delle memorie, base e frazione ▶Simp
  della cronologia, grafici dei comandi. I record v1–v6 si leggono ancora:
  nuovo fixture generato con i sorgenti della v1.10.0
  (`tests/fixtures/workspace_v1_10_0.inc`).

### Architettura
- Nuovo servizio `GraphCmd` (`graph_commands.hpp`) nel registro; Run-Matrix
  collegato a GRAPH (e GRAPH a Run-Matrix); sezioni dati dei membri e dei
  metodi delle liste (`dataSectionOfMember`). Il test di architettura è più
  stretto: Run-Matrix non può usare `ws.graph`, `SketchItem`,
  `CommandGraph`, `graph::` né primitive di disegno, deve passare da
  `graphcmd::execute` e GRAPH deve disegnare i grafici dei comandi; i nuovi
  moduli `value`, `csv_io` e `graph_commands` sono nel livello del motore.

### Comportamenti cambiati volutamente
- In Line il tasto frazione scrive `⌟` invece di `/` (come la fx-CG50).
- Display Eng: simboli ingegneristici invece di ×10ⁿ tra f e T; in Eng i
  risultati frazionari si mostrano in decimale (S⇔D per la forma esatta).
- Nella riga SUB dell'editor delle liste EXE scende alla prima cella.

### Test
- Nuove suite `lists_csv_test`, `calc_v111_test`, `run_matrix_v111_test`;
  suite esistenti adattate solo ai cambiamenti voluti (versione del record,
  List 7–26 ora valide, F1 in Line, formato Eng). Screenshot 52–61 nuovi e
  quelli di GRAPH, TABLE, 3D e Run-Matrix rigenerati, compressi senza
  perdita. Numeri in [TESTING.md](TESTING.md).

## v1.10.0 — matrici, vettori e liste; editor MAT/VCT; Math/Line; CLIP; parentesi colorate; cursore sugli errori

Base: `main` al commit `bb2bdee` (la v1.9.0; il tag `v1.9.0` mancante viene
creato su quel commit dal workflow di release). Analisi delle fonti:
[reports/ANALISI_FONTI_v1.10.0.md](reports/ANALISI_FONTI_v1.10.0.md); rapporto:
[reports/SVILUPPO_v1.10.0.md](reports/SVILUPPO_v1.10.0.md).
**Nessuna prova su hardware fisico**: procedura
[HARDWARE_TEST_v1.10.0.md](HARDWARE_TEST_v1.10.0.md).

### Matrici, vettori e liste nel parser
- Valori di prima classe dello **stesso parser e dello stesso AST**:
  `[[1,2][3,4]]`, `{1,2,3}`, `Mat A`–`Mat Z`, `Vct A`–`Vct Z`, `List 1`–`List 6`,
  `Mat Ans`, `Vct Ans`, `List Ans`, elementi `Mat A[1,2]`, `List 1[3]`.
  Nuovo tipo `CalcValue` (`include/value.hpp`) e nuova API `evaluateAny()` /
  `evaluateExactAny()`; i valutatori scalari esistenti accettano le stesse
  espressioni quando il risultato è un numero (`Det Mat A`, `DotP(…)`).
- Operazioni del manuale fx-CG50 (2-49…2-63, 3-7…3-16): + − × tra matrici,
  numero × matrice, potenze intere e inversa, `Det`, `Trn`, `Identity`, `Dim`,
  `Augment`, `Fill`, `Ref`, `Rref`, `Mat→List`, `List→Mat`, `DotP`, `CrossP`,
  `Angle`, `UnitV`, `Norm`; liste elemento per elemento con `Sum`, `Prod`, `Min`,
  `Max`, `Mean`, `Median`, `Cuml`, `Percent`, `ΔList`, `SortA`, `SortD`, `Seq`.
- Forme esatte negli elementi (`[[1,2][3,4]]^−1` = `[[−2,1][3/2,−1/2]]`), anche
  con S⇔D; numeri complessi negli elementi nei modi a+bi e r∠θ.
- Errori nello stile della fx-CG50: `Dimension ERROR` (`DimensionError`),
  `Type ERROR` (`TypeError`), matrice singolare, limiti della scheda.
- Memorizzazione: `→Mat A`, `→Vct A`, `→List 1`, `→Mat A[r,c]`,
  `→List 1[n]`, `{m,n}→Dim Mat A`; risposte in `MatAns`, `VctAns`, `ListAns`
  (Ans resta l'ultimo numero).
- Limiti per scheda (`limits.hpp`): lato 40 / 999, 400 / 10 000 elementi per
  matrice, 1 200 / 20 000 elementi in tutte le memorie (Pico 2 W / Pico Plus 2 W).

### Editor MAT/VCT (Run-Matrix, SHIFT 3)
- Elenco Mat A–Z / Vct A–Z (e Ans in sola lettura) con le dimensioni, DIM,
  celle con EXE verso destra, ROW-OP (SWAP, ×Rw, ×Rw+, Rw+), ROW e COLUMN
  (DELETE, INSERT, ADD), DELETE, DEL-ALL, M⇔V; valori nel workspace, quindi
  conservati cambiando applicazione e dopo lo spegnimento (record v6).
- MATH › MAT/VCT: modelli 2×2, 3×3, m×n fino a 6×6, 2×1, 3×1, 1×2, 1×3;
  risultati disegnati a griglia fino a 6×6 (liste fino a 20 elementi).

### Editor delle espressioni
- **Modalità Math / Line** (SYSTEM › Input/Output, primo campo del SET UP,
  salvato e indicato nella barra di stato): in Line l'input e i risultati
  sono su una riga e i tasti dei modelli inseriscono testo lineare; il cambio
  converte la riga in corso.
- **Selezione parziale CLIP** (SHIFT 8, frecce, F1 COPY, F2 CUT, DEL; SHIFT 9
  PASTE; Ctrl+C/X/V): limitata al campo del cursore o all'intero modello, con
  le parentesi sempre bilanciate.
- **Parentesi colorate** per livello (blu, rosso, verde, magenta, nero), con
  contrasto verificato (≥ 4,5:1) su ogni sfondo dell'input; nere dopo
  l'esecuzione.
- **Cursore sulla causa di un errore**: il lexer e il parser conservano la
  posizione di token e sotto-espressioni (`ast::Node::pos/len`,
  `ErrorLocation`), il valutatore indica la sotto-espressione che non riesce
  (`EvalInfo::error`), l'editor riporta la posizione nell'albero
  (`ast::SerializeMap`, `NaturalEditor::markError`): il cursore va alla fine
  della causa e la causa resta evidenziata.

### Workspace
- Record **versione 6**: modalità Input/Output, memorie Mat/Vct/MatAns/VctAns/
  ListAns, risultati lista/matrice/vettore della cronologia (con le forme
  esatte). I record delle versioni 1–5 (v1.6.0–v1.9.0) si leggono ancora:
  verificato con un record generato dai sorgenti della v1.9.0
  (`tests/fixtures/workspace_v1_9_0.inc`).
- MEMORY: nuova voce "Matrici e vettori" (conteggio e cancellazione).

### Correzioni
- `List 1`…`List 6` scritti in Run-Matrix davano "non disponibile qui": ora
  sono le liste di Statistics (in GRAPH restano un elemento per grafico).
- `^` e la frazione dopo una lista o una matrice digitate (`{1,2}^2`) creavano
  una base vuota: ora l'intero gruppo è l'operando.
- Un record salvato più grande della memoria libera all'avvio (es. Pico Plus
  2 W con la PSRAM disattivata) faceva terminare il firmware con
  `std::bad_alloc` prima del ciclo principale: ora l'avvio prosegue con un
  workspace vuoto, mostra "Memory ERROR" e **sospende i salvataggi**, così i
  dati in flash non vengono sovrascritti (`oom_test`).

### Compilazione e pubblicazione
- Nuovo workflow GitHub Actions `.github/workflows/release.yml`: test host di
  entrambi i profili (anche ASan/UBSan e TSan), test del relay, firmware UF2
  per Pimoroni Pico Plus 2 W e Raspberry Pi Pico 2 W, pacchetto di release;
  con l'avvio manuale crea il tag e la GitHub Release con i file compilati
  nella stessa esecuzione.

### Test
- Nuove suite `matrix_test` (parser, valutazione numerica ed esatta, errori e
  posizioni, memorie, nucleo di Run-Matrix, record v6, retrocompatibilità) e
  `editor_v110_test` (modelli MAT/VCT, Math/Line, CLIP, parentesi colorate e
  contrasto, cursore sugli errori, 600 sequenze casuali di modifica, flussi
  completi sul firmware con riavvio). Suite esistenti adattate solo dove la
  v1.10.0 cambia volutamente il comportamento (versione del record, primo
  campo del SET UP). Numeri in [TESTING.md](TESTING.md).

## v1.9.0 — consolidamento: v1.8.0 completa + Pimoroni Pico Plus 2 W

Branch canonico: `main` (tag `v1.9.0` creato dal workflow di release della v1.10.0 sul commit `bb2bdee`). Il primo lavoro sulla 1.9.0 era stato
sviluppato su `main`/`version/v1.5.4` e non conteneva le 24 commit della
v1.6.0–v1.8.0; questa versione le riunisce: architettura e funzioni della
v1.8.0 al completo, layer hardware e di memoria della prima 1.9.0 portato su
quell'architettura. Rapporto completo:
[reports/CONSOLIDAMENTO_v1.9.0.md](reports/CONSOLIDAMENTO_v1.9.0.md).
**Nessuna prova su hardware fisico**: procedura
[HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md).

### Scheda e build
- Scheda principale **Pimoroni Pico Plus 2 W** (`FXAI_BOARD=PICO_PLUS_2W`,
  predefinita), profilo alternativo Raspberry Pi Pico 2 W (`PICO2W`); una
  cartella di build e un UF2 per scheda (`picocalc_fxai_<scheda>.uf2`);
  `scripts/build.sh --board ...`, `build_pico2w.sh` invariato nell'uso.
- `-O2` predefinito su entrambe (era `-O3`), `-fstack-usage`, controllo della
  dimensione dell'immagine nella partizione firmware, LTO sperimentale.
- `scripts/make_release.sh` produce lo zip dei sorgenti, i due UF2 e
  `SHA256SUMS`. **Corretto**: il controllo "UF2 aggiornato" confrontava le date
  dei file, ma il Pico SDK tocca `CMakeLists.txt` a ogni build quando compila
  picotool dai sorgenti, quindi dopo la seconda scheda il primo UF2 risultava
  sempre vecchio; ora `build.sh` registra il commit dei sorgenti e l'hash
  dell'UF2 e la release li verifica.
- Pin della PicoCalc invariati (driver ufficiali della v1.8.0: LCD su SPI1,
  tastiera STM32 su I²C1, microSD su SPI0), raccolti in `board_config.hpp`.

### Memoria (Pico Plus 2 W)
- Driver PSRAM APS6404L con self-test all'avvio e avvio solo in SRAM in caso
  di guasto; pool SRAM/PSRAM (TLSF), `operator new` instradato (≥ 512 byte in
  PSRAM), `std::pmr`, sicuro su due core.
- Limiti: persistenti per scheda (Plus: liste da **999** elementi come la
  fx-CG50, cronologia di 200 righe; Pico 2 W invariato); RAM scelti dai pool
  presenti (curve di GRAPH 400 000 punti, PICT 1 MB, 3D griglia 128 con
  z-buffer); strutturali invariati.
- Record del workspace da 1 MB (2 × 512 KB) sulla Plus.

### Flash
- Partizioni: firmware, settore diagnostico, **LittleFS** (≈ 11 MB sulla Plus,
  ≈ 1,9 MB sul Pico 2 W) come archivio interno per progetti e immagini GRAPH
  (SYSTEM → "Archivio file"), BTstack, workspace.
- **Corretto**: lo slot B del workspace usava l'ultimo settore della flash, che
  il workaround del bootrom per l'erratum RP2350-E10 sovrascrive durante gli
  aggiornamenti UF2; ora nessun salvataggio lo scrive (sul Pico 2 W la
  posizione della v1.8.0 è conservata: i dati sopravvivono all'aggiornamento).
- **Corretto per le schede con PSRAM**: `flash_range_program` riceveva i dati
  direttamente dal buffer del chiamante; ora passano da un buffer di rimbalzo
  in SRAM (un buffer in PSRAM non è leggibile con l'XIP fermo); cancellazione
  un settore alla volta; verifica/ripristino della configurazione QMI M1.
- Migrazione una tantum del workspace di una v1.8.0 eseguita sulla stessa Plus.
- **UF2 della scheda sbagliata**: all'avvio viene letto il JEDEC ID della flash;
  con un'immagine Plus su un Pico 2 W (chip da 4 MB) le regioni oltre il chip
  sono disattivate invece di finire, per aliasing degli indirizzi, sul
  firmware e sui dati; l'avvio dice quale UF2 installare.

### Applicazioni
- **3D Graph** (prima solo modello dati): Z1–Z3, finestra 3D, griglia,
  rotazione, zoom, AUTO, campionamento incrementale, z-buffer in PSRAM o
  algoritmo del pittore, una famiglia di colore per superficie (blu, rosso,
  verde), salvato nel workspace (record **v5**); screenshot 40–43 dal
  simulatore, che con il profilo Plus usa ora il pool PSRAM come il modulo.
- GRAPH: budget dei punti dalla memoria presente; **layer statici in PSRAM**
  durante TRACE e puntatore (immagine identica al pixel; sul PC 0,05 ms invece
  di 18,6 ms per passo con 249 000 punti).
- SYSTEM: INFO con SRAM/PSRAM; DIAG con memoria/PSRAM, allocatore su 1 e 2
  core, flash↔PSRAM, LCD, flash interna, oltre a microSD e GRAPH; impostazione
  "Archivio file". MEMORY: cancellazione delle funzioni 3D.

### Robustezza
- Stack del core 0 con guardia MSPLIM, HardFault con rapporto al riavvio.
- **Corretto**: segnalare "Memory ERROR" allocava memoria (il messaggio era una
  `std::string`), quindi con la memoria esaurita l'eccezione usciva dal ciclo
  principale; errori, toast e finestra d'errore ora usano buffer riservati.
- **Corretto**: un `std::bad_alloc` durante l'autosalvataggio usciva dal ciclo
  principale (terminazione del firmware); ora "Memory ERROR: salvataggio
  rimandato" e le modifiche restano in attesa: il salvataggio riparte da solo
  quando la memoria torna (prima la modifica andava persa se non ne seguiva
  un'altra). Il ciclo principale intercetta comunque ogni eccezione residua.
- `deserializeWorkspace` non tiene più un `Workspace` (5,4 KB) sullo stack.
- 3D: corretta la normalizzazione dello yaw per rotazioni negative oltre 360°.

### Test
- Suite host eseguita per **entrambi i profili** (`FXAI_TEST_BOARD`), con
  ASan/UBSan e ThreadSanitizer (allocatore su due thread). Nuove suite:
  `mem_pools_smoke`, `mem_new_smoke`, `psram_selftest_smoke`, `limits_smoke`
  (dalla prima 1.9.0, adattate), `internal_fs_test`, `graphics_memory_test`,
  `graph3d_test`, `board_profile_test`, `system_diag_test`, `oom_test`.
  Numeri in [BUILD_STATUS.md](BUILD_STATUS.md) e [TESTING.md](TESTING.md).

### Rimosso (sostituito già nella v1.6.0–v1.8.0)
- Window manager, `ui.cpp`, driver LCD PIO, `main.cpp`, `graph_renderer` e
  emulatore SDL2 della base v1.5.4 modificati dalla prima 1.9.0: le loro
  funzioni esistono nell'architettura della v1.8.0 (framework delle app,
  driver ufficiali, motore GRAPH, simulatore con screenshot).
- `FXAI_ENABLE_CAS` (riservava memoria per un modulo CAS inesistente),
  `FXAI_ENABLE_SD` (la v1.8.0 ha lo stack microSD completo), contatore di avvii
  scritto in flash a ogni accensione, buffer AI da 2 MB (il protocollo del
  relay limita le risposte a 4000 caratteri).
- Opzione `FXAI_ENABLE_3D` della prima 1.9.0: non aveva effetto su alcun
  sorgente (la modalità 3D è sempre compilata e si adatta alla memoria).
- Test `flash_fs_smoke`, `graphics_memory_smoke`, `oom_smoke`,
  `system_app_smoke` della prima 1.9.0: non più compilati (API della base
  v1.5.4); i loro controlli sono in `internal_fs_test`, `board_profile_test`,
  `graphics_memory_test`, `graph3d_test`, `oom_test`, `expression_test` e
  `system_diag_test`.

## 1.9.0 (prima versione, sostituita) — supporto Pico Plus 2 W sulla base v1.5.4

Branch `version/v1.9.0` (= `main` fino al consolidamento), commit `f1f3089` e
`9ed7fdb`. Driver PSRAM, memoria multi-pool, limiti dinamici, LittleFS,
grafico 3D, diagnostica e test della memoria sviluppati sul codice della
v1.5.4 (senza la v1.6.0–v1.8.0). Questi componenti sono stati portati nella
v1.9.0 consolidata; il resto (UI e driver della v1.5.4) era già stato
sostituito dalla v1.6.0–v1.8.0. La cronologia completa resta nella storia Git.

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
