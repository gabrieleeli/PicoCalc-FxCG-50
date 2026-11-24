# Architettura delle applicazioni — v1.17.0

Questo documento descrive **come sono costruite tutte le modalità** (quelle
disponibili e quelle future), quali servizi condivisi usano, quali dati si
scambiano e come la separazione tra applicazioni viene verificata.
Lo stato funzionale di ogni voce è in [FEATURE_MATRIX.md](FEATURE_MATRIX.md).

> Nessuna applicazione "finta": le modalità non realizzate non hanno menu né
> schermate dimostrative. Aprendole compare una schermata che dice
> esplicitamente che non sono disponibili e cosa manca.

## 1. Il registro: una sola fonte di verità

`include/app_registry.hpp` + `src/app_registry.cpp` contengono una tabella con
le 23 modalità del menu principale (24 sulla Pimoroni Pico Plus 2 W dalla v1.18.0, con Distribution in coda all'enum). Per ognuna dichiarano:

| Campo | Significato |
|---|---|
| `id`, `name`, `key`, `color` | identità, didascalia, tasto di accesso diretto (1–9, A–N), colore dell'icona |
| `status` | `Available` (controller registrato, parità con la fx-CG50), `Base` (v1.17.0: controller registrato, si apre ed è usabile, parità non completa; `missing` dice cosa manca), `Planned` (progettata, controller non scritto), `External` (richiede hardware/protocolli non disponibili); `isOpen()` = Available o Base |
| `role` | responsabilità della modalità |
| `missing` | testo mostrato all'utente finché non è disponibile |
| `services` | servizi condivisi usati (vedi §3) — per le modalità future: quelli previsti |
| `data` | sezioni del `Workspace` a cui i sorgenti possono accedere (vedi §4) |
| `links` | modalità che può aprire direttamente (es. Graph → Table) |
| `sources` | file che la implementano |
| `boards` (v1.15.0) | schede su cui la modalità può esistere: `boards::Both` o `boards::PicoPlus2W` |

**Risoluzione per scheda (v1.15.0, F1).** `appInfo()` restituisce la voce
risolta per la build: se la scheda non è in `boards`, o se l'opzione
`FXAI_MODE_<NOME>` della modalità è spenta, la modalità è `Planned` senza
sorgenti né collegamenti; sul Pico 2 W il testo è «Non disponibile sul
Raspberry Pi Pico 2 W: richiede la Pimoroni Pico Plus 2 W (flash e RAM).»
(`declaredAppInfo()` dà la voce come è scritta, per i test). Le opzioni sono
in `cmake/fxai_sources.cmake` (`fxai_feature(MODE|SERVICE <NOME> BOARDS …
SOURCES … SYMBOLS …)`), arrivano al codice come `FXAI_MODE_<NOME>=0/1` e
`FXAI_SERVICE_<NOME>=0/1` (`include/features.hpp`) e decidono anche quali
sorgenti si compilano: il profilo host del Pico 2 W compila esattamente i
sorgenti del firmware del Pico 2 W. Dopo il link `cmake/check_firmware_size.cmake`
fallisce se l'immagine supera la partizione o il margine della scheda o se la
mappa del linker contiene un oggetto o un simbolo di una modalità/servizio
esclusi.

**Raspberry Pi Pico 2 W congelato alla v1.16.0.** Sul Pico 2 W le modalità
aperte sono quelle della v1.14.0 più eActivity, tutte Available e nessuna Base;
`architecture_test` fallisce se una versione successiva cambia questo insieme.
Dalla v1.17.0 le modalità nuove arrivano solo sulla Pimoroni Pico Plus 2 W
(`boards::PicoPlus2W`, codice dietro `#if FXAI_BOARD_PICO_PLUS_2W` con quello
della v1.16.0 nell'`#else`);
sul Pico 2 W sono ammesse solo correzioni di errori condivise. Per stare nella
partizione di 2 MB con margine, sul Pico 2 W i sorgenti di eActivity e quelli
fuori dai percorsi di calcolo e di disegno (record del workspace, file,
impostazioni, relay AI) sono compilati con `-Os`
(`FXAI_PICO2W_SIZE_SOURCES` in `cmake/fxai_sources.cmake`); il comportamento
è lo stesso.

**Stato Base (v1.17.0).** Una modalità Base si apre dal menu come una
Available; il launcher disegna un piccolo triangolo ambra nell'angolo in basso
a destra dell'icona e il piede dice «EXE: apri (base)» (solo Pimoroni Pico Plus
2 W: sul Pico 2 W non esistono modalità Base). `Available` è riservato alle
modalità con la parità completa: dalla v1.18.0 una modalità per versione passa
da Base ad Available (prima Statistics). `architecture_test` verifica che sulla
Plus le modalità della v1.16.0 restino Available e quelle della v1.17.0 siano
Base, e che sul Pico 2 W l'insieme delle modalità aperte resti quello della
v1.16.0 senza modalità Base.

La tabella è usata da launcher (icone, tasti, disponibilità), `AppManager`
(schermata "non disponibile") e dal **test di architettura** (§7): se un
sorgente usa un header, una sezione del workspace o un collegamento non
dichiarato, il test fallisce.

## 2. Strati e flusso

```
┌───────────────────────────────────────────────────────────────────────────┐
│ Modalità (src/apps/*_app.cpp)   un controller per modalità (classe App)   │
├───────────────────────────────────────────────────────────────────────────┤
│ Framework: AppManager · Context · registro · widget (form, griglie, liste)│
├──────────────────────────────┬────────────────────────────────────────────┤
│ UI: ui_kit, math_renderer,   │ Nuclei di modalità: run_matrix (calcolo e  │
│ natural_editor, font, canvas │ cronologia), app_models (modelli futuri)   │
│                              ├────────────────────────────────────────────┤
│                              │ Motore: expression (parser/AST/valutatore),│
│                              │ exact, number_format, solvers, statistics, │
│                              │ math_engine                                │
├──────────────────────────────┴────────────────────────────────────────────┤
│ Workspace (dati condivisi) → serializzazione versionata → storage A/B      │
├───────────────────────────────────────────────────────────────────────────┤
│ Firmware (boot, decoder tasti, autosalvataggio) → porta board Pico / host │
└───────────────────────────────────────────────────────────────────────────┘
```

Regole (verificate dal test):
- il **motore** non include header dell'interfaccia o delle app;
- ogni modalità include solo gli header dei servizi che dichiara;
- non esistono cicli di include in `include/`;
- le modalità **non chiamano** altre modalità: si aprono tramite
  `Context::open()` solo dove il collegamento è dichiarato e si scambiano dati
  solo tramite il `Workspace`.

Ciclo di una modalità: `enter()` → `key()` (le eccezioni diventano finestre di
errore CG50 in `AppManager`) → `draw()` a ogni frame (aggiornamento a tile del
display) → `tick()` per lavoro periodico (rete) → `leave()`.

## 3. Servizi condivisi

| Servizio | Header | Usato da |
|---|---|---|
| Expression | `expression.hpp`, `expression_ast.hpp`, `calc_library.hpp` (v1.13.0: CONVERT, distribuzioni, PROB, TVM, nomi del CATALOG; funzioni pure senza stato) | tutte le modalità di calcolo: **un solo** parser e valutatore |
| Exact | `exact.hpp` | Run-Matrix (frazioni, radicali, π), Equation (v1.12.0), eActivity (v1.16.0); previsto per Program |
| Editor | `natural_editor.hpp`, `line_editor.hpp` | Run-Matrix, Graph, Equation (celle e Solver tramite `MathField`, v1.12.0), form |
| MathRender | `math_renderer.hpp` | Run-Matrix, Graph, Symbols, eActivity (espressioni nel testo, righe di calcolo) |
| Format | `number_format.hpp` | tutte (Norm/Fix/Sci/Eng, complessi) |
| Solvers | `solvers.hpp` | Equation (disequazioni, estensione), Conic Graphs (v1.17.0) |
| EquationCore (v1.12.0) | `equation.hpp` | solo Equation: coefficienti esatti, SIML, POLY, SOLVER, visualizzazione delle soluzioni |
| Stats | `statistics.hpp`; sulla Plus dalla v1.18.0 anche `stat_engine.hpp` (nucleo alla parità del cap. 6) e `dist_core.hpp` (24 distribuzioni, una sola tabella sul motore di `calc_library`) | Statistics, Spreadsheet, Picture Plot (v1.17.0: regressioni sopra lo sfondo), Distribution (v1.18.0) |
| MathCore | `math_engine.hpp` | Run-Matrix (Base-N, DMS, Pol/Rec), angolo |
| Widgets | `ui_kit.hpp`, `app_widgets.hpp` | tutte |
| Network | `net.hpp`, `ai_protocol.hpp` | AI Chat, System |
| Storage | `storage.hpp` (via `Firmware`) | persistenza del workspace |
| Units | `point_a.hpp` | Conversion |
| Help | `cheatsheet.hpp` | Symbols |
| RunCore | `run_matrix.hpp` | Run-Matrix, Equation, eActivity (righe di calcolo, v1.16.0); previsto per Program |
| GraphCmd (v1.11.0) | `graph_commands.hpp` | Run-Matrix: comandi grafici (Plot, F-Line, Graph Y=…) che scrivono V-Window, SKETCH e grafici dei comandi; li disegna Graph |
| Models | `app_models.hpp` | modelli dati delle modalità future |
| GraphCore | `graph_model.hpp`, `graph_engine.hpp`, `graph_solve.hpp`, `table_model.hpp` | Graph, Table; previsto per Dyna Graph, Recursion, Conic Graphs (campionamento, viewport, G-Solve) — [GRAPH_ENGINE.md](GRAPH_ENGINE.md) |
| Files | `file_store.hpp`, `graph_file.hpp`, `pict.hpp` | Graph (progetti `.pcgraph`, immagini `.pcpict`, BMP sulla microSD), eActivity (righe immagine, cartelle); previsto per Picture Plot, Program — [STORAGE.md](STORAGE.md) |
| Diag | `diagnostics.hpp` | System (SYSTEM → DIAG: memoria, PSRAM, allocatore, flash, LCD, microSD, flash interna, tempi di GRAPH sul dispositivo, log USB) |
| Graph3D | `graph3d_model.hpp`, `graph3d.hpp`, `limits.hpp` | 3D Graph: modello puro (funzioni, modelli, V-Window, Setup, validazione del manuale, geometria analitica, INTSECT/RELATION, codifica delle G-Mem) e disegno (reticoli, taglio al box, proiezione θ/φ, z-buffer o pittore, sezioni CROSS), budget dalla memoria presente |
| Hardware | `board_config.hpp`, `mem_pools.hpp`, `psram.hpp`, `limits.hpp`, `system_info.hpp`, `system_runtime.hpp` | System (INFO, "Archivio file", DIAG): scheda, pool di memoria, PSRAM, stack — [PICO_PLUS_2W.md](PICO_PLUS_2W.md) |
| TypedFiles (v1.15.0, entrambe) | `typed_files.hpp` | MEMORY (vista per tipo), eActivity (`.pceact`, v1.16.0), le dieci modalità Base (v1.17.0, `ui::TypedFileMenu`) — [FILE_FORMATS.md](FILE_FORMATS.md) |
| ModeShell (v1.15.0, entrambe) | `mode_shell.hpp` | TABLE, eActivity (barre a pagine e sottomenu, conferme, menu), le dieci modalità Base (v1.17.0, con `TypedFileMenu`, `drawModeHeader` e i campi `numberField`/`textField`/`choiceField`, solo Plus) |
| TextEdit (v1.15.0, entrambe) | `text_editor.hpp` | eActivity (memo e DETAIL, v1.16.0), Program, Python, Spreadsheet (editor di cella) (v1.17.0) |
| StepTask (v1.15.0, entrambe) | `step_task.hpp` | 3D Graph (calcolo progressivo), eActivity (ricalcolo fino alla riga di stop, AC → Break), v1.17.0: ricalcolo del foglio, animazione di Dyna, tabella di Recursion, esecuzione di Program e di Python, trasferimenti di Link |
| Graph2D (v1.15.0, solo Plus) | `graph2d.hpp` | Dyna Graph, Recursion, Conic Graphs, Geometry, Picture Plot (v1.17.0); previsto per Dual Screen |
| Binding (v1.15.0, solo Plus) | `binding.hpp` | Spreadsheet (celle e intervalli), Dyna Graph (coefficiente), Recursion (`n`, `an`, `an+1`), Program (`fx::calc`) (v1.17.0) |
| Interp (v1.15.0, solo Plus) | `program.hpp` | Program (v1.17.0): sottoinsiemi interpretati di C, C++ e C# (decisione della v1.15.0, confermata nella v1.17.0) |
| EActivityCore (v1.16.0, entrambe) | `eactivity.hpp`, `limits.hpp` | eActivity: documento, righe, testo con espressioni e a capo, 24 tipi di striscia, codifica `.pceact`, limite per scheda |
| ModeCore (v1.17.0, solo Plus) | `spreadsheet.hpp`, `dyna.hpp`, `recursion.hpp`, `conic.hpp`, `financial.hpp`, `program_mode.hpp`, `link_protocol.hpp`, `geometry.hpp`, `picture_plot.hpp`, `python_port.hpp`, `mode_codec.hpp`, `limits.hpp` | i nuclei puri delle dieci modalità Base; ogni modalità può includere **solo il proprio** nucleo più `mode_codec.hpp` e `limits.hpp` (`modeCoreHeaders()`, verificato da `architecture_test`) |

Errori: il motore lancia `SyntaxError`, `MathError`, `NonRealError` con testi
nello stile CG50; `AppManager` li mostra nella finestra di errore. Memoria:
nessuna allocazione dinamica nei percorsi ricorsivi; limiti espliciti
(token, annidamento, budget di stack) in [MATH_ENGINE.md](MATH_ENGINE.md).

## 4. Workspace: dati condivisi

| Sezione (`data::`) | Contenuto | Scritta da | Letta da |
|---|---|---|---|
| `Settings` | Input/Output (Math/Line, v1.10.0), angolo, display, complessi, forme esatte, Frac Result, rete; Mode (Comp/Base-N), Simplify, formato CSV, XList/YList di Statistics (v1.11.0) | System, Statistics (SET, CSV SET) | tutte |
| `Memory` | A–Z, Ans, PreAns con la forma esatta | Run-Matrix, Equation, Conversion, Graph | tutte le modalità di calcolo |
| `History` | cronologia di Run-Matrix (con forma esatta e vista) | Run-Matrix | Memory |
| `Functions` | Y1–Y20: tipo (12), espressione, Yt, selezione, colore, stile | Graph, Table, Statistics (copia della regressione) | Graph, Table |
| `View` | V-Window (con T e Xdot) | Graph, Table (GPH-CON/PLT); Run-Matrix solo con il comando `ViewWindow` tramite `graph_commands` (v1.11.0) | Graph, Table; Run-Matrix in sola lettura via `NameResolver` (VARS › V-WIN, `Xdot`) |
| `Table` | Start/End/Step, variabile (range/lista), tipo, righe generate, richiesta GPH-CON/PLT | Table, Graph (progetto) | Table, Graph |
| `Lists` | List 1–26 e i sottonomi (v1.11.0), List File 1–6 (v1.13.0) | Statistics (anche CSV), Table (LISTMEM), System (List File), Run-Matrix tramite `storeList` e `selectListFile` | Statistics, Graph e Table (grafici e tabelle da lista), Run-Matrix tramite `CollectionResolver` (anche `List "nome"` con `listByName`) |
| `Graph` | Setup di Graph, memorie V-Window e grafiche, SKETCH, immagine richiamata, progetto aperto, grafici dei comandi di Run-Matrix (v1.11.0) | Graph; Run-Matrix solo tramite `graph_commands` (comandi grafici) | Graph, Table (Setup Derivative) |
| `Equation` | coefficienti con forma esatta, equazione e limiti del Solver, ultime soluzioni (record v8) | Equation | Equation; Run-Matrix solo tramite `CollectionResolver` (VARS › SimRes, SimCoef, PlyRes, PlyCoef) |
| `Chat` | conversazione AI | AI Chat | AI Chat |
| `Graph3D` | tre funzioni (tipo, modulo, espressioni, coefficienti, selezione, colori), V-Window 3D con θ/φ, Setup, zoom, 3D G-Mem 1–20, 3D V-Win 1–6 (record v10) | 3D Graph, Memory | 3D Graph, Memory |
| `Matrices` (v1.10.0) | Mat A–Z, Vct A–Z, MatAns, VctAns, ListAns | Run-Matrix (calcoli ed editor MAT/VCT) | Run-Matrix, Memory |
| `EActivity` (v1.16.0) | cartella e ultimo file dell'elenco; dati del menu principale durante un documento (memorie) o una striscia (parti), rimessi a posto al riavvio (sezione `0x0102`) | eActivity | eActivity |
| `Strings` (v1.13.0) | Str 1–20, function memory f1–f20, variabili di modo (Dyna, Recursion, TVM) | Run-Matrix tramite `setString`, `setFunctionMemory`, `setRecursionFormula`, `storeSystemVariable`; Dyna Graph, Recursion, Financial (v1.17.0) | Run-Matrix (VARS, `functionMemory` tramite `ServiceResolver`), GRAPH (DrawDyna/DrawR tramite `graph_commands`), Dyna Graph, Recursion, Financial |
| `Sheet`, `Dyna`, `Recursion`, `Conic`, `Financial`, `Program`, `Link`, `Geometry`, `PicturePlot`, `Python` (v1.17.0, solo Plus) | lo stato di ogni modalità Base (`ws.sheet`, `ws.dyna`, …, `ws.python`; sezione `0x0100 + AppId` del record v12) | la modalità | la modalità; Link e Memory leggono tutto (pacchetti del workspace, cancellazione) |

Memory e System (RESET) possono cancellare ogni sezione. Le variabili di altre
modalità entrano in un'espressione **solo** tramite `NameResolver`
(`Workspace::resolve`): nomi `Xmin … Tptch`, `Xdot`, `TblStart/TblEnd/TblPitch`.
Le funzioni di Graph entrano in un'altra funzione solo tramite
`FunctionResolver` (riferimenti `Y1(x)` … delle funzioni composte).
Matrici, vettori e liste entrano in un'espressione solo tramite
`CollectionResolver` (`Workspace::collection`: `Mat A`, `Vct A`, `List 1`–`List 26`
e le memorie Ans) e si assegnano solo con i metodi controllati del workspace
(`storeMatrix`, `storeVector`, `storeList`, `storeCollectionAnswer`): così
Run-Matrix legge e scrive le liste di Statistics senza accedere a `ws.lists`.
Non esistono sistemi di variabili separati per modalità.

Persistenza: `serializeWorkspace()` (versione **12** dalla v1.17.0, solo sulla
Pimoroni Pico Plus 2 W: dopo il numero di versione la firma di scheda e
versione che ha scritto il record e un CRC-32 per ogni sezione; una sezione con
il CRC sbagliato o che non si decodifica viene isolata (`damagedSections`, valori
iniziali per quella modalità, avviso «Data ERROR» all'avvio) e il resto del
record si legge; il Pico 2 W continua a scrivere la versione 11.
Versione 11 dalla v1.15.0 con la coda di
sezioni TLV: ogni modalità realizzata dopo la v1.15.0 aggiunge la propria
sezione `0x0100 + AppId` in `src/workspace_sections.cpp`, compilata solo sulle
schede che hanno la modalità; le sezioni sconosciute o più nuove restano in
`Workspace::preservedSections` e vengono riscritte intatte — [STORAGE.md](STORAGE.md);
versione 10 dalla v1.14.0 con il 3D Graph
completo in coda; versione 9 dalla v1.13.0 con List File,
stringhe, function memory, variabili di modo, viste ENG/∠/a+bi e decimali
delle celle; versione 8 dalla v1.12.0 con i dati di
EQUATION in coda; versione 7 dalla v1.11.0 con Base-N,
Simplify, CSV, List 7–26, sottonomi, forme esatte delle memorie e grafici dei
comandi; versione 6 dalla v1.10.0 con Input/Output, matrici e vettori; versione 5 dalla v1.9.0 con 3D Graph e archivio dei file;
le versioni 1 della v1.6.0, 2 della v1.7.0, 3 e 4 della v1.8.0, 5 della v1.9.0,
6 della v1.10.0, 7 della v1.11.0, 8 della v1.12.0 e 9 della v1.13.x vengono ancora lette) → `RecordStore` con due slot e CRC-32. I file
dell'utente (progetti GRAPH, immagini) sono sulla microSD o nella flash interna
(LittleFS), scelta in SYSTEM, tramite il servizio Files ([STORAGE.md](STORAGE.md)).

## 5. Modalità disponibili

| Modalità | Controller | Responsabilità | Servizi | Dati | Collegamenti |
|---|---|---|---|---|---|
| Run-Matrix | `RunMatrixApp` + `MatrixEditorView` (`matrix_editor.cpp`) + `AnswerView` (`answer_view.cpp`, v1.11.0) + nucleo `run_matrix.cpp` | calcoli e risultati: input Math o Line (INS, ⌟), forme esatte, matrici/vettori/liste ed editor MAT/VCT con CSV, MatAns/ListAns, cronologia modificabile, variabili, Base-N intera, simboli ingegneristici, Simp, DMS, CLIP; comandi grafici eseguiti da `graph_commands` e disegnati da Graph | Expression, Exact, Editor, MathRender, Format, MathCore, RunCore, Storage, Files, GraphCmd | Settings, Memory, History, Matrices, View e Graph (solo tramite `graph_commands`) | Graph |
| Statistics | `StatisticsApp`; sulla Plus dalla v1.18.0 `statistics_plus_app.cpp` (editor, menu, moduli, lavori a passi) e `stat_plot.cpp` (grafici), nucleo `stat_engine` e `dist_core` (servizio Stats); sul Pico 2 W il codice della v1.16.0 (`statistics_app.cpp`, nell'`#else`) | parità completa con il capitolo 6 sulla Plus ([STATISTICS.md](STATISTICS.md)): editor a tre pagine, StatGraph1–3, CALC con 12 regressioni, TEST, INTR, DIST, SET UP, VARS › STAT | Expression, Stats, Widgets, Storage, Files; Plus: ModeShell, StepTask | Settings, Lists, Functions; Plus: View, Memory, Matrices, StatData (`0x0101`) | — |
| Graph | `GraphApp` (`graph_app.cpp`, `graph_plot.cpp`, `graph_files.cpp`) | grafici Y=, r=, parametrici, X=, disequazioni (Y1–Y20); TRACE, ZOOM, V-Window, SKETCH, G-SOLVE, Modify, memorie, PICT, progetti `.pcgraph` — [GRAPH_PARITY.md](GRAPH_PARITY.md) | Expression, Editor, Format, Widgets, MathRender, Storage, GraphCore, Files | Settings, Memory, Functions, View, Table, Lists, Graph | Table |
| Table | `TableApp` + `table_model` | tabelle di Y=, r= e parametriche da range o lista, derivate, EDIT/ROW/DELETE, LISTMEM, GPH-CON/GPH-PLT — [TABLE.md](TABLE.md) | Expression, Editor, Format, Widgets, MathRender, Storage, GraphCore | Settings, Memory, Functions, View, Table, Lists, Graph | Graph |
| Equation | `EquationApp` + `MathField` (`math_field.cpp`) + motore `equation.cpp` | SIML, POLY e SOLVER come il manuale fx-CG50 (4-1…4-5), disequazioni come estensione | Expression, Exact, Editor, MathRender, Format, RunCore, Solvers, Storage, EquationCore | Settings, Memory, Equation | — |
| 3D Graph | `Graph3DApp` (`graph3d_app.cpp` lista e input, `graph3d_plot.cpp` grafico) + `graph3d_model.cpp`, `graph3d.cpp` | capitolo 16 della fx-CG50 ([GRAPH3D.md](GRAPH3D.md)): modelli Line/Plane/Sphere/Cylinder/Cone, Z=, Param, Rot X/Y, V-Window, Setup, FORMAT, 3D G-Mem/V-Mem, rotazione e ROTATE, ZOOM e viste, TRACE, SKETCH, G-Solve CROSS/INTSECT/RELATION, PICTURE, CAPTURE, calcolo progressivo | Expression, Widgets, Editor, Format, MathRender, Storage, Files, Graph3D | Settings, Memory, Graph3D | — |
| Memory | `MemoryApp` | uso e cancellazione dei dati (anche 3D Graph, 3D G-Mem, 3D V-Win) | Widgets, Storage | tutte | — |
| System | `SystemApp` | SET UP, retroilluminazione, archivio dei file, Wi-Fi, relay, reset, INFO, DIAG | Widgets, Editor, Format, Network, Storage, Diag, Hardware | tutte (RESET) | — |
| Conversion | `ConversionApp` | conversioni di unità, risultato in Ans | Expression, Units | Settings, Memory | — |
| AI Chat | `AiChatApp` | chat con Claude/ChatGPT tramite relay | Network, Editor | Settings, Chat | System |
| Symbols | `SymbolsApp` | guida dei simboli | Help | — | — |
| Distribution (v1.18.0, **Base**, solo Plus) | `DistributionApp` (`distribution_app.cpp`) + nucleo `distribution.cpp` (ModeCore) | 24 funzioni di distribuzione con Data Variable/List, Ans/ListAns, Save Res, DRAW — [DISTRIBUTION.md](DISTRIBUTION.md); stesso nucleo `dist_core` del DIST di Statistics | Expression, Widgets, Editor, Format, Stats, Storage, ModeShell, TypedFiles, StepTask, ModeCore | Settings, Memory, Lists, Distribution (`0x0117`) | — |
| eActivity (v1.16.0, entrambe) | `EActivityApp` (`eactivity_app.cpp`) + nucleo `eactivity.cpp` | capitolo 10 della fx-CG50: elenco dei file e cartelle, righe di testo con espressioni Math, righe di calcolo (nucleo di Run-Matrix) e ricalcolo fino alla riga di stop, colori, marcatori, memo, immagini, strisce, Notes, memorie per file, SAVE/SAVE·AS/OPT/CAPACITY | Expression, Exact, Editor, MathRender, Format, Widgets, RunCore, Storage, Files, TypedFiles, ModeShell, TextEdit, StepTask, EActivityCore | Settings, Memory, Matrices, Lists (memorie del file), EActivity; le parti delle strisce solo con `encodeParts`/`decodeParts`/`resetPart` | Run-Matrix, Graph, Table, Statistics, Equation, 3D Graph (sessione da striscia del `Context`) |

### Modalità Base (v1.17.0, solo Pimoroni Pico Plus 2 W)

Ogni modalità Base è un controller sottile su `ModeShell` (`src/apps/<nome>_app.cpp`,
classe con pImpl dichiarata da `FXAI_BASE_APP` in `include/apps.hpp`) sopra un
nucleo puro testato sul PC (servizio ModeCore). Ognuna ha la sua sezione del
record v12, il suo tipo di file in MEMORY › FILES e le operazioni lunghe in un
`step::Task` interrompibile con AC. Guide: link nella colonna «Modalità».

| Modalità | Nucleo | Responsabilità (v1.17.0) | Servizi | Dati | Collegamenti |
|---|---|---|---|---|---|
| [Spreadsheet](SPREADSHEET.md) | `spreadsheet.cpp` | celle A1…Z999, formule `=` con riferimenti e intervalli (BindingResolver), ricalcolo con ordinamento di Kahn, Circular ERROR, blocchi COPY/CUT/PASTE/FILL/CLEAR, colonna ↔ List, `.pcsheet` | Expression, Widgets, Editor, Format, Stats, Storage, ModeShell, TypedFiles, TextEdit, StepTask, Binding, ModeCore | Settings, Memory, Lists, Sheet | — |
| [Dyna Graph](DYNA_GRAPH.md) | `dyna.cpp` | funzione con un coefficiente, Start/End/Pitch, ≤ 100 fotogrammi, tre velocità, Stop/Repeat | Expression, Widgets, Editor, Format, Storage, ModeShell, TypedFiles, StepTask, Graph2D, Binding, ModeCore | Settings, Memory, View, Strings, Dyna | — |
| [Recursion](RECURSION.md) | `recursion.cpp` | an+1 / an+2, tabella con Σan, grafico e ragnatela | come Dyna Graph | Settings, Memory, View, Strings, Recursion | — |
| [Conic Graphs](CONIC_GRAPHS.md) | `conic.cpp` | sei modelli, disegno con due rami per l'iperbole, TRACE, G-SOLVE | Expression, Widgets, Editor, Format, Solvers, Storage, ModeShell, TypedFiles, Graph2D, ModeCore | Settings, Memory, View, Conic | — |
| [Financial](FINANCIAL.md) | `financial.cpp` + `calc_library::cmpdSolve`/`amortizationAt` | SMPL, CMPD (BGN/END), CASH, AMT, CNVT, DAYS, →Ans | Expression, Widgets, Editor, Format, Storage, ModeShell, TypedFiles, StepTask, ModeCore | Settings, Memory, Strings, Financial | — |
| [Program](PROGRAM.md) | `program_mode.cpp` + Interp | elenco, NEW per dialetto, editor con sintassi colorata, RUN con input e AC, errori con riga e colonna; grafici in Graph | Expression, Widgets, Editor, Format, Storage, ModeShell, TypedFiles, TextEdit, StepTask, Binding, Interp, GraphCmd, ModeCore | Settings, Memory, Lists, Program | Graph |
| [Link](LINK.md) | `link_protocol.cpp` | trame con CRC, Sender/Receiver a passi, pacchetti del workspace e dei file, APPLY | Widgets, Storage, Files, ModeShell, TypedFiles, StepTask, ModeCore | tutte (pacchetto del workspace) | — |
| [Geometry](GEOMETRY.md) | `geometry.cpp` | punti, segmenti, rette, circonferenze, poligoni; MOVE, MEASURE, ZOOM, PAN | Expression, Widgets, Editor, Format, Storage, ModeShell, TypedFiles, Graph2D, ModeCore | Settings, Memory, Geometry | — |
| [Picture Plot](PICTURE_PLOT.md) | `picture_plot.cpp` | sfondo `.pcpict`, punti numerati, List 1/2, regressione | Expression, Widgets, Editor, Format, Stats, Storage, Files, ModeShell, TypedFiles, Graph2D, ModeCore | Settings, Memory, Lists, PicturePlot | — |
| [Python](PYTHON.md) | `python_port.cpp` + MicroPython v1.24.1 (`third_party/micropython`) | SHELL, script, `input()`, KeyboardInterrupt, coroutine con pila propria, heap separato, `casioplot` | Widgets, Editor, Storage, ModeShell, TypedFiles, TextEdit, StepTask, Hardware, ModeCore | Settings, Python | — |

Le modalità Base con una sezione propria compaiono anche nelle **strisce**
dell'eActivity: la parte `part::ModeBase + AppId` (`0x40 + AppId`) salva e
ricarica la sezione della modalità, così Conics, Recur, Dynamic Graph,
Financial, SpreadSheet, Geometry e Picture Plot diventano disponibili come
strisce sulla Plus (`eact::stripAvailable` usa `isOpen`).

La separazione richiesta è rispettata e **verificata**: i grafici di funzioni
sono solo in Graph (Table li apre con G-CON/G-PLT), quelli statistici solo in
Statistics, le conversioni solo in Conversion, le tabelle solo in Table,
equazioni e disequazioni solo in Equation; Run-Matrix mostra solo calcoli e
risultati (nessun riferimento a grafici, liste, tabelle o conversioni).

## 6. Modalità non ancora disponibili

Dalla v1.17.0 tutte le modalità del registro si aprono, tranne **E-CON4**
(External). Le modalità Base (§5) arrivano alla parità completa una per
versione: **Statistics** è alla parità dalla v1.18.0 (sulla Plus), quando
**Distribution** è entrata come nuova modalità Base; la prossima è Geometry
(v1.19.0). Il piano è in [ROADMAP.md](ROADMAP.md) e in `CONTINUE.md`.
Sul Raspberry Pi Pico 2 W (congelato alla v1.16.0) le modalità della Plus
restano `Planned` con il testo «Non disponibile sul Raspberry Pi Pico 2 W…».

### Strisce di eActivity (v1.16.0): la sessione da striscia

Una striscia apre un'altra modalità con i **propri dati**: eActivity salva le
parti del workspace della modalità (`part::Graph`, `Table`, `Lists`,
`Equation`, `Matrices`, `Graph3D`, `History`) nella sezione `0x0102`, carica
quelle della striscia e chiama `Context::beginStrip(owner, target, entry)`.
La modalità legge `Context::strip().entry` in `enter()` per aprire la
schermata della striscia (grafico, lista, SOLVER, SIML/POLY, editor MAT/VCT,
RUN in Math); la barra di stato è azzurra; `AppManager` riporta a eActivity con
MENU, SHIFT ' e SHIFT , (cambio di applicazione), `Context::home()` (EXIT dalla
schermata principale) fa lo stesso. Le modalità non conoscono eActivity: nessun
`#include` né `AppId::EActivity` nei loro sorgenti, solo `stripentry`.
Dalla v1.17.0 le modalità Base della Plus (Conic, Recursion, Dyna, Financial,
Spreadsheet, Geometry, Picture Plot) aggiungono la loro parte `0x40 + AppId`:
la striscia diventa disponibile da sola (`eact::stripAvailable`).

### E-CON4 — External
- Richiede un data logger esterno e il relativo protocollo: **non realizzabile** con l'hardware della PicoCalc.
- Parte realizzabile in futuro: importazione di campioni da file CSV nelle liste (`Lists`) e analisi in Statistics.

## 7. Verifica: `tests/architecture_test.cpp`

| Controllo | Come |
|---|---|
| Registro coerente | ID in ordine, tasti e nomi unici, `Available` ⇔ `sources` ⇔ nessun `missing`, collegamenti solo verso modalità disponibili, file e header esistenti |
| Registrazione nel firmware | il `Firmware` reale apre ogni modalità: le `Available` hanno il proprio controller, le altre la schermata "non disponibile"; ogni schermata si disegna senza errori; ogni tasto di accesso diretto apre la modalità giusta |
| Servizi | ogni `#include` dei sorgenti di una modalità appartiene ai servizi dichiarati |
| Dati | ogni `ws.<sezione>` usato è dichiarato (`resetAll` richiede tutte le sezioni) |
| Collegamenti | ogni `open(AppId::X)` è dichiarato |
| Run-Matrix | nessun riferimento a grafici, statistiche, tabelle, conversioni; le liste solo tramite `CollectionResolver` (mai `ws.lists`); i dati di GRAPH solo tramite `graphcmd::execute` (mai `ws.graph`, `ws.view`, `SketchItem`, `CommandGraph`, `graph::`) e nessuna primitiva di disegno; deve aprire GRAPH per mostrare i comandi (v1.11.0); legge le matrici di EQUATION solo come `CollectionResolver::Memory::Equation`, senza `eq::` né `equation.hpp` (v1.12.0) |
| Equation (v1.12.0) | nessuna lettura propria dei numeri (`strtod`, `atof`, `stod`, `sscanf`, `istringstream`, `evalNumber`); nessun percorso numerico separato (`CellGrid`, `solveLinearSystem`, `polynomialRoots`, `solveEquation`); deve usare il motore (`eq::evaluateNumber`, `eq::solveSimultaneous`, `eq::solvePolynomial`, `eq::solveFor`, `eq::numberTree`) e `MathField`; il motore valuta con `parseExpression`/`evaluateAny`/`evaluateExact`; solo Equation dichiara EquationCore |
| Stratificazione | gli header e i sorgenti del motore non includono UI/app; nessun ciclo di include |
| Modelli | validazioni e helper dei modelli delle modalità future |
| Stato Base (v1.17.0) | le modalità aperte (`isOpen`) hanno controller e sorgenti, le Base hanno un `missing` che dice cosa manca alla parità; sulla Plus le modalità della v1.16.0 restano Available e quelle della v1.17.0 sono Base; sul Pico 2 W l'insieme delle modalità aperte è quello della v1.16.0 e nessuna è Base |
| ModeCore (v1.17.0) | ogni modalità Base include solo il proprio nucleo (`modeCoreHeaders`) più `mode_codec.hpp` e `limits.hpp`; la regola della V-Window di Graph ammette le modalità Base con Graph2D e `View` |
| Inizializzazione (v1.17.0) | `apps_init_test`: ogni modalità aperta nei casi C1–C7 (freddo, dopo RESET, record v11 valido/corrotto/troncato, solo SRAM, memoria quasi esaurita, microSD assente/presente) con i controlli K1–K6 ([reports/ANALISI_FONTI_v1.17.0.md](reports/ANALISI_FONTI_v1.17.0.md)) |

Il test è stato provato introducendo apposta in Run-Matrix un `#include
"statistics.hpp"`, un accesso a `ws.view` e un `open(AppId::Graph)`: fallisce
con 5 errori; senza la modifica passa (468 controlli).

## 8. Come aggiungere una modalità

1. Implementare il nucleo (logica senza UI) usando i servizi esistenti; test host.
2. Mettere lo stato nel `Workspace` come **sezione TLV** (id `0x0100 + AppId`,
   codifica con `codec::Writer/Reader` di `mode_codec.hpp` in
   `src/workspace_sections.cpp`, versione della sezione propria, `reset` per
   RESET e strisce; nessuna nuova versione del record: dalla v1.17.0 la
   sezione ha anche il suo CRC nel record v12).
3. Scrivere il controller in `src/apps/<nome>_app.cpp` sul guscio comune
   (`ModeShell`), registrarlo in `Firmware` sotto `FXAI_MODE_<NOME>`;
   aggiungere i sorgenti a `fxai_feature(MODE <NOME> …)`.
4. Aggiornare la voce del registro (`status`: `Base` finché la tabella di parità
   non è completa, poi `Available`; `services`, `data`, `links`, `sources`, `boards`).
   Aggiungere il caso della modalità ad `apps_init_test` (succede da solo per
   ogni modalità aperta) e il tipo di file a `typed_files.cpp`.
5. `architecture_test` verifica che dichiarazioni e sorgenti coincidano.
6. Aggiornare [FEATURE_MATRIX.md](FEATURE_MATRIX.md) e questo documento.
