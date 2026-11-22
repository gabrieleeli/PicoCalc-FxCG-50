# Analisi delle fonti e del punto di partenza — v1.15.0 (M0)

Questo documento è la milestone **M0** del piano che porta il progetto da
v1.14.0 fino alla v1.29.0: fotografia del codice di partenza, capitoli del
manuale per ogni modalità mancante, matrice «modalità × fondamenta» che
giustifica i servizi della **v1.15.0 (M1)**, decisioni prese prima del codice e
tabella dei requisiti delle fondamenta (un test per voce). È scritto **prima**
del codice della v1.15.0.

## 1. Fonti

| # | Fonte | Uso | Note |
|---|---|---|---|
| 1 | *fx-CG50/fx-CG50 AU/fx-CG20/fx-CG20 AU/fx-CG10 Software User's Guide*, **versione 3.60** (PDF ufficiale Casio, `casio.com/content/dam/casio/global/support/manuals/calculators/pdf/004-en/f/fx-CG50_Soft_v360_EN.pdf`, 678 pagine, scaricato il 21/11/2026) | **solo descrizione del comportamento**: menu, tasti, messaggi, limiti | nessun codice, icona, font, immagine o testo copiato oltre ai nomi di comandi e messaggi |
| 2 | Codice e documentazione del repository a `main` = `72d0921` (v1.14.0 + recap) | punto di partenza | §2 |
| 3 | Standard dei linguaggi C (ISO/IEC 9899), C++ (ISO/IEC 14882), C# (ECMA-334) | sintassi e semantica del sottoinsieme di Program (§6.1) | solo la definizione dei linguaggi, nessun codice di compilatori o interpreti |

### 1.1 Capitoli del manuale (indice verificato)

Numeri ricontrollati sull'indice (pagine ii–v) e sulle pagine stampate. La
colonna «PDF» è la pagina del file, utile per l'estrazione.

| Capitolo | Titolo nel manuale | Pagine stampate | PDF |
|---|---|---|---|
| 5-23…5-24 | Drawing Two Graphs on the Same Screen (Dual Screen G+G) | 5-23 | 164–165 |
| 5-32…5-37 | Using Tables (GtoT, T+G, Graph to Table nel §5 del cap. 5) | 5-32 | 173–178 |
| 5-42…5-44 | Dynamic Graphing (**Dyna Graph**) | 5-42 | 183–185 |
| 5-45…5-49 | Graphing a Recursion Formula (**Recursion**) | 5-45 | 186–190 |
| 5-50…5-51 | Graphing a Conic Section (**Conic Graphs**) | 5-50 | 191–192 |
| 6 | Statistical Graphs and Calculations (**Statistics**) | 6-1…6-72 | 205–276 |
| 7 | Financial Calculation (**Financial**) | 7-1…7-20 | 277–296 |
| 8 | Programming (**Program**) | 8-1…8-68 | 297–364 |
| 9 | Spreadsheet | 9-1…9-36 | 365–400 |
| 10 | **eActivity** | 10-1…10-23 | 401–423 |
| 11 | Memory Manager (**Memory**) | 11-1…11-13 | 424–436 |
| 12 | System Manager (**System**) | 12-1…12-8 | 437–444 |
| 13 | Data Communication (**Link**) | 13-1…13-14 | 445–458 |
| 14 | **Geometry** | 14-1…14-64 | 459–522 |
| 15 | **Picture Plot** | 15-1…15-18 | 523–540 |
| 16 | 3D Graph Function (completo dalla v1.14.0) | 16-1…16-19 | 541–559 |
| 17 | **Python** (solo fx-CG50) | 17-1…17-32 | 560–591 |
| 18 | **Distribution** (solo fx-CG50) | 18-1…18-11 | 592–602 |
| α | Error Message Table, Input Ranges | α-1…α-15 | 603–617 |
| ε | E-CON4 Application | ε-1…ε-51 | — |

> **Scoperta della M0.** Il capitolo 18 descrive una modalità **Distribution**
> (icona del menu principale della fx-CG50 dalla versione 3.40) che **non è nel
> registro** (`src/app_registry.cpp` ha 20 modalità della calcolatrice più 3
> estensioni) e non è nell'elenco degli obiettivi. Il calcolo è lo stesso del
> DIST di Statistics (§6-50…6-65), che la M3 completa; la modalità aggiunge la
> schermata del grafico con le code, il ricalcolo inverso e i colori. Non viene
> aggiunta al registro senza una decisione dell'utente (cambierebbe tasti e
> numero di icone); la domanda è riportata in CONTINUE.md e nel rapporto della
> v1.15.0.

## 2. Punto di partenza (M0)

`git fetch origin main`: `main` = `72d0921` («Docs: tolto il prompt delle
fondamenta…»), sopra il tag `v1.14.0` (`b9aac14`). Branch di lavoro
`claude/fervent-albattani-hepkis`, partito da `main` senza differenze.

### 2.1 Cancello eseguito sul codice attuale

| Controllo | Risultato |
|---|---|
| `tests/run_host_tests.sh` (Plus e Pico 2 W) | **47/47** per profilo, 0 errori |
| `tests/run_host_tests.sh --sanitize --tsan` | **47/47** per profilo con ASan/UBSan, `mem_pools_tsan` verde (vedi §2.4) |
| Relay AI (`unittest`) | **15/15** |
| Simulatore (`tools/sim_compare.py`, nuovo, tutti i `tests/sim/*.txt`) | **94** screenshot identici pixel per pixel a `docs/screenshots` |
| `scripts/build.sh --clean --board pico_plus_2w` | 0 warning; immagine **1 948 024 B** (46 % di 4 MB); RAM statica 311 072 B |
| `scripts/build.sh --clean --board pico2w` | 0 warning; immagine **1 939 180 B** (92 % di 2 MB); RAM statica 307 148 B |
| Toolchain | arm-none-eabi-gcc 13.2.1, Pico SDK 2.2.0, GCC 13.3 host, clang 18 (fuzzing) |

La RAM statica misurata (311 072 / 307 148 B) è di 2 048 B più alta del valore
di `BUILD_STATUS.md` (309 024 / 305 100 B): la differenza è esattamente la
sezione `.heap` minima del linker (2 048 B), che lo script di controllo somma
alle sezioni in SRAM; d'ora in poi `BUILD_STATUS.md` riporta il valore dello
script, che è quello misurato.

### 2.2 Dimensioni per modulo (mappa del linker)

Strumento nuovo `tools/map_sizes.py` (somma per file oggetto le sezioni di
ingresso caricate in flash e in SRAM; le stringhe unite dal linker sono contate
prima dell'unione, quindi i totali superano l'immagine di circa 80 KB).

| Area (Pico 2 W) | Flash (B) | SRAM (B) |
|---|---:|---:|
| `src` (nuclei, motore, servizi) | 1 045 799 | 2 043 |
| `src/apps` (controller) | 449 005 | 76 |
| Pico SDK (cyw43, lwIP, mbedTLS…) | 425 787 | 93 398 |
| libc, libstdc++, libgcc | 50 555 | 22 864 |
| `third_party` (FatFs, LittleFS) | 31 933 | 1 126 |
| `src/platform` (porta Pico, framebuffer) | 18 589 | 224 404 |

| Modulo (Pico 2 W) | Flash (B) | | Modulo | Flash (B) |
|---|---:|---|---|---:|
| `expression.cpp` | 247 277 | | `graph_engine.cpp` | 32 294 |
| `cyw43_ll.c` (firmware Wi-Fi) | 232 993 | | `statistics.cpp` | 31 505 |
| `math_engine.cpp` | 95 418 | | `graph3d.cpp` | 29 482 |
| `apps/graph_plot.cpp` | 69 561 | | `apps/table_app.cpp` | 28 538 |
| `equation.cpp` | 67 788 | | `apps/graph3d_plot.cpp` | 26 978 |
| `workspace.cpp` | 62 108 | | `calc_library.cpp` | 26 831 |
| `run_matrix.cpp` | 53 457 | | `graph_commands.cpp` | 26 029 |
| `apps/run_matrix_app.cpp` | 50 282 | | `graph3d_model.cpp` | 25 571 |
| `natural_editor.cpp` | 47 388 | | `exact.cpp` | 25 434 |
| `apps/statistics_app.cpp` | 38 378 | | `apps/equation_app.cpp` | 24 951 |
| `apps/graph_app.cpp` | 37 082 | | `diagnostics.cpp` | 23 407 |
| `apps/graph3d_app.cpp` | 36 183 | | `apps/matrix_editor.cpp` | 23 290 |
| `apps/graph_files.cpp` | 35 894 | | `math_renderer.cpp` | 20 056 |
| `graph_file.cpp` | 35 531 | | `solvers.cpp` | 18 465 |

La Plus ha gli stessi moduli con +9 KB in `src` (pool PSRAM, limiti più
grandi): 1 054 437 B di nuclei e servizi, 449 072 B di controller.

### 2.3 Budget delle schede (dal §BUDGET del piano)

| Scheda | Oggi | Limite | Spazio |
|---|---:|---:|---:|
| Pico 2 W, partizione 2 097 152 B | 1 939 180 B | ≤ **2 064 384 B** dopo eActivity (margine ≥ 32 KiB) | **125 204 B** per M1 + M2 |
| Pico Plus 2 W, partizione 4 194 304 B | 1 948 024 B | < 90 % = 3 774 873 B | 1 826 849 B per M1…M15 |

Ripartizione decisa per il Pico 2 W: **M1 ≤ 45 KB** (F1–F6, solo il codice
che serve a eActivity e alle modalità della v1.14.0), **M2 ≤ 75 KB**
(eActivity con le strisce), il resto come riserva. `check_firmware_size.cmake`
diventa bloccante: superare la partizione o il margine di 32 KiB (soglia per
scheda) è un **errore** del build.

### 2.4 Fuzzing e simulatore di partenza

8 harness (`fuzz_expr`, `fuzz_exactdiff`, `fuzz_workspace`, `fuzz_csv`,
`fuzz_pcgraph`, `fuzz_equation`, `fuzz_ui`, `fuzz_graph3d`) compilati con clang
18 + libFuzzer; nella v1.15.0 `fuzz_workspace` cambia (sezioni TLV) e si
aggiungono `fuzz_typedfile`, `fuzz_texteditor` e `fuzz_program`: ognuno
almeno 300 s.

## 3. Modalità da realizzare: capitoli e contenuto

Elenco dal manuale (titoli dei paragrafi «k» del capitolo); la tabella di
parità completa di ogni modalità viene scritta nell'`ANALISI_FONTI` della sua
versione, prima del codice.

| Modalità | Cap. | Versione | Scheda | Contenuto del capitolo (riassunto) |
|---|---|---|---|---|
| **eActivity** | 10 | v1.16.0 (M2) | entrambe | elenco file (OPEN, NEW, DELETE, SEARCH, MEMO con JUMP/EDIT/DETAIL/DELETE/DEL-ALL); righe di testo (255 byte, a capo automatico, EXE = a capo), righe di calcolo (come Run-Matrix in Math, ricalcolo delle righe seguenti fino a una riga di stop), righe di stop, righe immagine (bordo FORMAT: stile e colore), strisce (23 tipi, titolo di 16 caratteri, EXE apre la modalità con i dati della striscia, SHIFT , torna, barra di stato azzurra, SIZE), striscia Notes (editor di testo lungo); FILE (SAVE, SAVE·AS, OPT, CAPACITY), JUMP (TOP, BOTTOM, PageUp, PageDown), DEL-LINE, INSERT (TEXT, CALC, STOP, PICTURE), MAT/VCT e LIST con memorie **separate per file**; COLOR (MARKER, CHAR), MEMO (INSERT, DELETE, Catalog, VIEW/DETAIL); file fino a ~29 000 byte; nomi di 8 caratteri A–Z { } ' ~ 0–9; uscita con «Save?» Sì/No/AC; messaggi «No MEMO», «Not Found» |
| **Statistics** | 6 | v1.17.0 (M3) | Plus | StatGraph 1–3 e parametri generali; Scatter, xyLine, NPP, Pie, Histogram, Med-box, Bar, N-Dist, Broken; calcoli dai grafici; regressioni (ax+b, a+bx, Med-Med, X²…X⁴, Log, aeᵇˣ, abˣ, Power, **Sin**, **Logistic**), residui, copia in Graph; 1/2-Var, stime x̂ ŷ; TEST (Z, t, χ², 2-Sample F, ANOVA 1 e 2 vie, LinearReg t); INTR (Z, t); DIST (Normal, t, χ², F, Binomial, Poisson, Geometric, Hypergeometric: Pd/Cd/InvCd); termini di input e output (6-66…6-68) |
| Graph: Dual Screen, GtoT, T+G | 5-23, 5-32…5-37 | v1.18.0 (M4) | Plus | G+G con COPY e SWAP, schermo grafico + tabella (GtoT), tabella + grafico (T+G), Graph to Table durante il TRACE, FORMAT durante lo SKETCH |
| **Recursion** | 5-45…5-49 | v1.19.0 (M5) | Plus | an+1, an+2, an con bn e cn, SET (Start/End, a0/a1, Σ), tabella con Σ, grafici (connessi e a punti), fase (an, bn), WEB con convergenza/divergenza |
| **Dyna Graph** | 5-42…5-44 | v1.20.0 (M6) | Plus | funzioni predefinite (BUILT-IN) e personalizzate, Dynamic Var, Start/End/Pitch, Stop/Cont, velocità, Locus, DOT, Dynamic Graph Memory, tabella da formula |
| **Conic Graphs** | 5-50…5-51 | v1.21.0 (M7) | Plus | modelli cartesiani (parabola ×4, cerchio ×2, ellisse, iperbole ×2), polari e parametrici; G-Solve: fuochi, vertici, direttrice, asse, asintoti, eccentricità, latus rectum, centro, raggio, intercette; TRACE, ZOOM |
| **Financial** | 7 | v1.22.0 (M8) | Plus | SET (Payment, Date Mode, PM/AM, Periods/Year), grafici dei flussi; Simple, Compound (n, I%, PV, PMT, FV, P/Y, C/Y), Cash Flow (NPV, IRR, PBP, NFV), Amortization (BAL, INT, PRN, ΣINT, ΣPRN), Conversion (▶EFF, ▶APR), Cost/Sell/Margin, Days, Depreciation (SL, FP, SYD, DB), Bond (prezzo, rendimento), funzioni nei calcoli |
| **Spreadsheet** | 9 | v1.23.0 (M9) | Plus | griglia A–Z × 999, FILE (NEW, OPEN, SAVE·AS, RECALL, CSV), EDIT (CUT, COPY, PASTE, CELL, JUMP, SEQ, FILL, SORT), DELETE/INSERT/CLEAR, formule con riferimenti relativi e assoluti, comandi CellIf, CellMin, CellMax, CellMean, CellMedian, CellProd, CellSum, formattazione condizionale, GRAPH e CALC dalle celle, memorie verso liste/matrici, «Circular ERROR», «Range ERROR» |
| **Program** | 8 | v1.24.0 (M10) | Plus | elenco file, editor, RUN, DEBUG, SEARCH, RENAME, password, comandi (decisione §6.1) |
| **Picture Plot** | 15 | v1.25.0 (M11) | Plus | SET UP specifico, file, piazzamento dei punti, Plot List, AXTRANS (T-Y, T-X), Y=f(x), regressioni, Fade I/O |
| **Geometry** | 14 | v1.26.0 (M12) | Plus | menu File/Edit/View/Draw/Construct/Transform/Animate, puntatore, oggetti, vincoli e costruzioni, trasformazioni, misure (blocco, incolla), testo ed etichette, assi e griglia, view window, pan/zoom, animazioni e tabelle di animazione, file |
| **Memory**, **System**, **Link** | 11, 12, 13 | v1.27.0 (M13) | Plus | Memory: informazioni, cartelle, dettagli, selezione, copia, backup della memoria principale, ottimizzazione; System: luminosità, Power Properties (spegnimento automatico), lingua, versioni, reset, batteria, nome utente; Link: trasferimento verso il PC (formato proprio, §6.4) |
| **Python** | 17 | v1.28.0 (M14) | Plus | elenco file, SHELL, editor, menu delle funzioni, CATALOG, `import`, `casioplot`, debug degli script, compatibilità dei file |
| E-CON4 | ε | — | — | resta **External**: richiede un data logger; al più import CSV nelle liste |

## 4. Matrice «modalità × fondamenta»

Ogni colonna è un servizio di M1; una ✔ significa che la modalità lo usa (e
quindi che il servizio serve). «P» = solo sulla Pico Plus 2 W.

| Modalità | F1 opzioni per scheda | F2 sezione TLV | F3 file tipizzati | F4 ModeShell | F5 editor di testo | F6 StepTask | F7 grafico 2D (P) | F8 BindingResolver (P) | F9 interprete (P) |
|---|---|---|---|---|---|---|---|---|---|
| eActivity (M2) | ✔ (su entrambe) | ✔ stato dell'elenco e ultimo file | ✔ `EACT` | ✔ elenco, menu, conferme | ✔ righe di testo, Notes, memo | ✔ ricalcolo delle righe con AC | — | — | — |
| Statistics (M3) | ✔ completamento solo Plus | ✔ StatGraph 1–3, impostazioni | — | ✔ | — | ✔ test lunghi | ✔ grafici e TRACE | — | — |
| Graph/Table Dual (M4) | ✔ | ✔ impostazioni Dual | — | ✔ (Table già portata in M1) | — | — | ✔ G+G, GtoT, T+G | — | — |
| Recursion (M5) | ✔ | ✔ formule, SET (da `Strings`) | — | ✔ | — | ✔ tabelle lunghe | ✔ successioni, WEB, fase | ✔ n, an, an+1, bn, cn | — |
| Dyna Graph (M6) | ✔ | ✔ funzioni, Dynamic Var, memorie | — | ✔ | — | ✔ animazione | ✔ famiglia, Locus | ✔ coefficiente | — |
| Conic Graphs (M7) | ✔ | ✔ modello e coefficienti | — | ✔ | — | — | ✔ coniche, G-Solve | — | — |
| Financial (M8) | ✔ | ✔ variabili e SET | — | ✔ form e SOLVE | — | — | ✔ grafici dei flussi | ✔ variabili TVM | — |
| Spreadsheet (M9) | ✔ | ✔ foglio aperto | ✔ `SHEET` | ✔ | ✔ barra della formula | ✔ ricalcolo con AC | ✔ grafici dalle celle | ✔ celle e intervalli, Circular ERROR | — |
| Program (M10) | ✔ | ✔ ultimo file | ✔ `PROG` | ✔ | ✔ editor dei sorgenti | ✔ esecuzione con AC (Break) | ✔ comandi grafici | ✔ variabili locali in `calc()` | ✔ |
| Picture Plot (M11) | ✔ | ✔ | ✔ `PPLOT` | ✔ | — | — | ✔ punti, regressioni | — | — |
| Geometry (M12) | ✔ | ✔ | ✔ `GEOM` | ✔ | — | ✔ animazioni | ✔ oggetti geometrici | — | — |
| Memory/System/Link (M13) | ✔ | ✔ | ✔ vista per tipo, backup | ✔ | — | ✔ trasferimento | — | — | — |
| Python (M14) | ✔ | ✔ | ✔ `PY` | ✔ | ✔ editor e SHELL | ✔ esecuzione con AC | ✔ `casioplot` | — | — |

Conseguenza per il **Pico 2 W**: servono solo F1–F6 (eActivity e il porting di
Table al guscio); F7, F8 e F9 si compilano solo sulla Plus
(`FXAI_SERVICE_GRAPH2D`, `FXAI_SERVICE_BINDING`, `FXAI_SERVICE_INTERP` spenti
sul Pico 2 W). eActivity non ha bisogno di F8: le righe di calcolo sono valutate
da `evaluateEntry` del nucleo di Run-Matrix con il workspace, come in
Run-Matrix.

## 5. Decisioni prese in M0

### 5.1 Modalità per scheda (F1)

- Il registro guadagna il campo `boards` (bit `Board::Plus`, `Board::Pico2W`).
  Su una scheda non elencata la modalità è `Planned` con il testo
  «Non disponibile sul Raspberry Pi Pico 2 W: richiede la Pimoroni Pico Plus
  2 W (flash e RAM).», senza sorgenti né collegamenti.
- Opzioni CMake in `cmake/fxai_sources.cmake`: una per modalità
  (`FXAI_MODE_<NOME>`) e una per servizio (`FXAI_SERVICE_<NOME>`), con il
  valore predefinito della scheda; le definizioni arrivano al codice come
  `FXAI_MODE_<NOME>=0/1`. Lo stesso file serve firmware e test host, quindi
  il profilo host del Pico 2 W compila esattamente i sorgenti del firmware del
  Pico 2 W.
- Dopo il link, `cmake/check_firmware_size.cmake` legge la mappa del linker e
  fallisce se vi compare un oggetto di una modalità o di un servizio esclusi
  (o un simbolo del loro controller).

### 5.2 Workspace a sezioni (F2)

- Record **v11** = campi della v10 invariati + coda TLV:
  `u16 numero`, poi per ogni sezione `u16 id`, `u16 versione`, `u32 lunghezza`,
  dati. Sezioni scritte in ordine di id; id duplicati, lunghezze oltre il
  record o oltre `MaxSectionBytes` rendono il record non valido (si tiene quello
  dell'altro slot, come oggi).
- Sezione con id sconosciuto **o** con versione più nuova di quella che il
  firmware sa leggere: conservata byte per byte e riscritta intatta. Così un
  firmware più vecchio, o il Pico 2 W, non perde i dati delle modalità che non
  ha. È anche la soluzione al problema aperto A1 per le versioni future:
  dalla v1.15.0 il formato principale resta la v11 e ogni modalità nuova
  aggiunge una sezione invece di una versione.
- Id: `0x0100 + AppId` per la sezione di una modalità (eActivity = `0x0102`),
  `0x0001…0x00FF` riservati alle sezioni comuni.

### 5.3 Strisce di eActivity (per M2, progettate ora)

Una striscia conserva **le sezioni del workspace** della sua modalità
(codificate come le sezioni TLV). Aprendo la striscia eActivity salva le
sezioni correnti, carica quelle della striscia, apre la modalità con il
contesto «da striscia» (barra di stato azzurra, SHIFT , torna); al ritorno le
sezioni aggiornate rientrano nella striscia e quelle salvate vengono
ripristinate. Così i dati della striscia sono indipendenti da quelli delle
modalità aperte dal menu principale, come dice il manuale (10-19). Le
memorie Mat/Vct/List sono per file (10-9) con lo stesso meccanismo. La
codifica per parti delle sezioni esistenti (Functions+View+Graph, Table,
Lists, Equation, Matrices, Graph3D, History) si scrive nella M2, insieme alle
strisce che la usano (decisione presa durante la M1).

### 5.4 File (F3)

| Tipo | Cartella | Estensione | Formato | Scheda |
|---|---|---|---|---|
| eActivity | `PICOCALC/EACT` | `.pceact` | binario con intestazione e CRC | entrambe |
| Program | `PICOCALC/PROG` | `.c`, `.cpp`, `.cs` | testo UTF-8 (modificabile sul PC) | Plus |
| Spreadsheet | `PICOCALC/SHEET` | `.sheet` | binario con intestazione e CRC | Plus |
| Python | `PICOCALC/PY` | `.py` | testo UTF-8 | Plus |
| Geometry | `PICOCALC/GEOM` | `.pcgeo` | binario con intestazione e CRC | Plus |
| Picture Plot | `PICOCALC/PPLOT` | `.pcplot` | binario con intestazione e CRC | Plus |

Intestazione binaria: magia di 4 byte per tipo, versione del formato, flag,
lunghezza dei dati, CRC-32 dei dati. I file di testo non hanno intestazione
(si aprono con un editor sul PC); sono validati come UTF-8 senza NUL entro il
limite della scheda. Formati in `docs/FILE_FORMATS.md`.

### 5.5 Guscio comune (F4)

`ModeShell`: pila di schermate (EXIT torna alla precedente, AC dove previsto,
ritorno da SET UP), barra F1–F6 da tabella con sottomenu, pagine (▷) e
SHIFT, lista selezionabile, conferma Sì/No, richiesta di un valore, SET UP
della modalità da tabella, aggancio a OPTN/VARS/CATALOG tramite i resolver.
**Table** viene portata sul guscio con gli stessi screenshot (20–37, 06/07
dove Table compare) pixel per pixel.

### 5.6 Editor di testo (F5), lavoro a passi (F6)

- `TextEditor`: buffer di righe UTF-8, righe lunghe con scorrimento
  orizzontale o a capo automatico, ricerca, salto alla riga, selezione e
  clipboard di testo nel `Context`, limite di byte per scheda
  (`limits::kTextEditorBytes`: 24 KB sul Pico 2 W, 256 KB sulla Plus), garanzia
  forte su `std::bad_alloc`.
- `StepTask`/`StepRunner`: passo con budget di unità **e** di tempo (orologio
  della piattaforma, simulato nei test), AC → «Break» (finestra come la
  calcolatrice), barra di avanzamento comune. Il calcolo progressivo del 3D
  Graph passa al runner con lo stesso budget di 600 punti per passo, quindi gli
  screenshot 88–104 non cambiano.

### 5.7 Grafico 2D (F7, Plus), espressioni legate (F8, Plus)

- `graph2d`: disposizioni a schermo diviso (pieno, G+G, GtoT, T+G) con due
  `graph::Viewport`, sorgenti di punti generiche (funzione, successione, conica,
  lista, oggetto geometrico) con campionamento a passi, cursore di TRACE e ZOOM
  comuni sulla V-Window unica di `View`; disegno incrementale identico a un
  ridisegno completo (test pixel per pixel).
- `BindingResolver`: catena di `NameResolver` con variabili legate (n, an, an+1,
  an+2, bn, cn, coefficiente di Dyna, variabili TVM, variabili locali di
  Program) e riferimenti di cella A1…Z999 e intervalli (`A1:B5`) letti **dallo
  stesso lexer** con un'opzione di analisi; grafo delle dipendenze iterativo
  (algoritmo di Kahn) con «Circular ERROR» per i cicli.

## 6. Decisione per Program (domanda all'utente)

Domanda posta in M0: (a) mantenere il linguaggio proprio, (b) linguaggio con i
comandi e la sintassi del capitolo 8. **Risposta dell'utente: «usa il c, c++ e
c#».**

### 6.1 Interpretazione e conseguenze

La risposta non sceglie né (a) né (b): Program userà **tre linguaggi, C, C++ e
C#**, come sottoinsiemi interpretati sul dispositivo (un compilatore vero non
sta nei 32 KB di stack e nella RAM della scheda). Conseguenze, da confermare
con l'utente se l'interpretazione non è quella voluta:

| Voce | Decisione |
|---|---|
| Linguaggi | un nucleo comune (F9) con tre varianti: `.c` (C), `.cpp` (C++), `.cs` (C#) |
| Sottoinsieme comune | `int`/`long`, `double`/`float`, `bool`, `char`, stringhe (`std::string`, `string`; in C array di `char` con le funzioni di `<string.h>` più usate), array a una e due dimensioni, funzioni con parametri e ricorsione a profondità limitata, `if`/`else`, `for`, `while`, `do…while`, `switch`, `break`, `continue`, `return`, `goto` ed etichette (salti), operatori di C con precedenza standard, costanti |
| I/O | C: `printf`, `puts`, `scanf`, `getchar`; C++: `std::cout <<`, `std::cin >>`, `std::endl`, `std::getline`; C#: `Console.Write`, `Console.WriteLine`, `Console.ReadLine`, `int.Parse`, `double.Parse` — tutti verso l'I/O astratto del nucleo (display della PicoCalc) |
| Matematica | `<math.h>`/`<cmath>`/`Math.`: `sin`, `cos`, `tan`, `sqrt`, `pow`, `exp`, `log`, `fabs`/`abs`, `floor`, `ceil`, `round`… con la semantica del linguaggio (double IEEE) |
| Calcolatrice | libreria `fx`: `fx::calc("espressione")` (C++), `Fx.Calc(...)` (C#), `fx_calc(...)` (C) valuta un'espressione della calcolatrice con il motore unico (forme esatte comprese) e con le variabili del programma legate tramite `BindingResolver` (F8); funzioni per le variabili A–Z, le liste e le matrici del workspace |
| Comandi grafici | libreria `fx`: `cls`, `viewWindow`, `plot`, `line`, `fLine`, `circle`, `text`, `drawGraph`… eseguiti da `graph_commands` (lo stesso interprete dei comandi grafici di Run-Matrix) e disegnati da Graph |
| Testo e tasti | `locate(x, y, testo)`, `clrText()`, `getKey()` come i comandi del capitolo 8 |
| Esecuzione | bytecode eseguito da una macchina a pila **iterativa** (nessuna ricorsione nativa), budget di passi e di memoria, interruzione con AC tramite `StepTask` («Break»), errori con riga e colonna |
| Parità con il capitolo 8 | la tabella di parità di Program (M10) riguarda la **modalità** (elenco file, editor, RUN, debug passo passo, ricerca, rinomina, password) e i **servizi** che il capitolo 8 dà ai programmi (I/O, grafici, liste, matrici, statistiche, finanza) tramite la libreria `fx`; la sintassi Casio Basic (If/Then, Lbl/Goto, ◢…) non è prevista per scelta dell'utente: va scritta come differenza voluta, non come parità |
| Architettura | il linguaggio dei programmi non è la grammatica delle espressioni della calcolatrice: il nucleo F9 ha un proprio analizzatore **dei sorgenti C/C++/C#**, dichiarato nel registro e ammesso da `architecture_test` solo in `src/program_*.cpp`; le espressioni della calcolatrice continuano ad avere **un solo** parser (`fx::calc` lo usa) |

## 7. Requisiti delle fondamenta (tabella «di parità» di M1)

Nessuna modalità nuova diventa Available in M1. Ogni voce ha almeno un test.

| # | Requisito | Scheda | Test |
|---|---|---|---|
| F1.1 | opzioni `FXAI_MODE_*`/`FXAI_SERVICE_*` con valori per scheda, definizioni `=0/1` nel codice | entrambe | `architecture_test`: registro ⇔ definizioni del build |
| F1.2 | campo `boards` del registro; testo «Non disponibile sul Raspberry Pi Pico 2 W…» | entrambe | `architecture_test`, `board_profile_test` |
| F1.3 | registro ⇔ sorgenti compilati per scheda | entrambe | `architecture_test` con l'elenco dei sorgenti passato da CMake |
| F1.4 | nessun oggetto/simbolo escluso nell'immagine del Pico 2 W | Pico 2 W | controllo sulla mappa nel build (`check_firmware_size.cmake`) |
| F1.5 | funzioni della v1.14.0 identiche sul Pico 2 W (modalità Available, screenshot) | Pico 2 W | `architecture_test` (insieme Available della v1.14.0), `sim_compare` |
| F1.6 | dimensione bloccante (partizione e margine) | entrambe | `check_firmware_size.cmake` (errore) |
| F2.1 | record v11 con coda TLV | entrambe | `workspace_v115_test` |
| F2.2 | sezioni sconosciute e più nuove conservate e riscritte intatte | entrambe | `workspace_v115_test` |
| F2.3 | sezioni corrotte, troncate, duplicate rifiutate senza toccare il workspace | entrambe | `workspace_v115_test`, `fuzz_workspace` |
| F2.4 | lettura v1–v10 invariata, fixture `workspace_v1_14_0.inc` | entrambe | `workspace_v115_test` + test esistenti |
| F2.5 | ~~`encodeSection`/`decodeSection` delle sezioni esistenti (per le strisce)~~ → spostato alla M2 (v1.16.0), dove le strisce lo usano | entrambe | — |
| F3.1 | tabella dei tipi per scheda (cartella, estensione, validatore, formato) | entrambe (EACT) / Plus | `typed_files_test` |
| F3.2 | intestazione con versione e CRC; testo UTF-8 validato | entrambe | `typed_files_test`, `fuzz_typedfile` |
| F3.3 | API elenco/apri/salva/salva come atomico/rinomina/elimina | entrambe | `typed_files_test` con `RamFileSystem` e guasti |
| F3.4 | microSD simulata e LittleFS con interruzione di alimentazione | entrambe | `typed_files_test` (FatFs su `SimSdCard`, LittleFS su flash simulata) |
| F3.5 | `FileDialog` unico (elenco, nome con validatore, sovrascrittura, ricerca) | entrambe | `typed_files_test` (dialogo da tastiera) |
| F3.6 | MEMORY: vista dei file per tipo | entrambe | `apps_flow_test`/`typed_files_test` |
| F4.1 | pila di schermate, EXIT/AC, ritorno da SET UP | entrambe | `mode_shell_test` |
| F4.2 | barra F1–F6 da tabella con sottomenu, pagine, SHIFT | entrambe | `mode_shell_test` |
| F4.3 | lista selezionabile, conferma, richiesta di valore, SET UP da tabella | entrambe | `mode_shell_test` |
| F4.4 | OPTN/VARS/CATALOG tramite i resolver | entrambe | `mode_shell_test` |
| F4.5 | Table sul guscio con screenshot identici | entrambe | `table_test`, `sim_compare` (06, 07, 20–37) |
| F5.1 | righe lunghe, scorrimento, a capo automatico | entrambe | `text_editor_test` |
| F5.2 | ricerca, salto alla riga, clipboard di `Context` | entrambe | `text_editor_test` |
| F5.3 | limite per scheda, memoria esaurita senza perdita | entrambe | `text_editor_test`, `oom_test`, `fuzz_texteditor` |
| F6.1 | passo con budget di unità e di tempo, orologio simulato | entrambe | `step_task_test` |
| F6.2 | AC → «Break», barra di avanzamento | entrambe | `step_task_test` |
| F6.3 | 3D Graph sul runner, screenshot identici | entrambe | `graph3d_v114_test`, `sim_compare` (88–104) |
| F7.1 | disposizioni pieno/G+G/GtoT/T+G | Plus | `graph2d_test` |
| F7.2 | sorgenti di punti generiche, campionamento a passi | Plus | `graph2d_test` |
| F7.3 | TRACE/ZOOM comuni, V-Window di `View` | Plus | `graph2d_test` |
| F7.4 | disegno incrementale identico al ridisegno completo | Plus | `graph2d_test` (pixel per pixel) |
| F8.1 | variabili legate componibili con i resolver esistenti | Plus | `binding_test` |
| F8.2 | celle A1…Z999 e intervalli dallo stesso lexer | Plus | `binding_test`, `expression_test` invariato |
| F8.3 | dipendenze circolari → «Circular ERROR» | Plus | `binding_test` |
| F9.1 | lessico e sintassi del sottoinsieme C/C++/C# | Plus | `program_core_test` |
| F9.2 | macchina a pila iterativa, if/for/while/do/switch, salti, funzioni a profondità limitata | Plus | `program_core_test`, `-fstack-usage` |
| F9.3 | I/O astratto, comandi grafici tramite `graph_commands`, `fx::calc` | Plus | `program_core_test` |
| F9.4 | budget di passi e di memoria, AC con StepTask | Plus | `program_core_test` |
| F9.5 | errori con riga e colonna | Plus | `program_core_test` |
| F9.6 | fuzzing | Plus | `fuzz_program` (≥ 300 s) |
