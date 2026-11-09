# Architettura delle applicazioni — v1.8.0

Questo documento descrive **come sono costruite tutte le modalità** (quelle
disponibili e quelle future), quali servizi condivisi usano, quali dati si
scambiano e come la separazione tra applicazioni viene verificata.
Lo stato funzionale di ogni voce è in [FEATURE_MATRIX.md](FEATURE_MATRIX.md).

> Nessuna applicazione "finta": le modalità non realizzate non hanno menu né
> schermate dimostrative. Aprendole compare una schermata che dice
> esplicitamente che non sono disponibili e cosa manca.

## 1. Il registro: una sola fonte di verità

`include/app_registry.hpp` + `src/app_registry.cpp` contengono una tabella con
le 23 modalità del menu principale. Per ognuna dichiarano:

| Campo | Significato |
|---|---|
| `id`, `name`, `key`, `color` | identità, didascalia, tasto di accesso diretto (1–9, A–N), colore dell'icona |
| `status` | `Available` (controller registrato), `Planned` (progettata, controller non scritto), `External` (richiede hardware/protocolli non disponibili) |
| `role` | responsabilità della modalità |
| `missing` | testo mostrato all'utente finché non è disponibile |
| `services` | servizi condivisi usati (vedi §3) — per le modalità future: quelli previsti |
| `data` | sezioni del `Workspace` a cui i sorgenti possono accedere (vedi §4) |
| `links` | modalità che può aprire direttamente (es. Graph → Table) |
| `sources` | file che la implementano |

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
| Expression | `expression.hpp`, `expression_ast.hpp` | tutte le modalità di calcolo: **un solo** parser e valutatore |
| Exact | `exact.hpp` | Run-Matrix (frazioni, radicali, π); previsto per eActivity, Program, Equation |
| Editor | `natural_editor.hpp`, `line_editor.hpp` | Run-Matrix, Graph, Equation (solver), form |
| MathRender | `math_renderer.hpp` | Run-Matrix, Graph, Symbols |
| Format | `number_format.hpp` | tutte (Norm/Fix/Sci/Eng, complessi) |
| Solvers | `solvers.hpp` | Equation; previsto per Financial e Conic |
| Stats | `statistics.hpp` | Statistics; previsto per Spreadsheet e Picture Plot |
| MathCore | `math_engine.hpp` | Run-Matrix (Base-N, DMS, Pol/Rec), angolo |
| Widgets | `ui_kit.hpp`, `app_widgets.hpp` | tutte |
| Network | `net.hpp`, `ai_protocol.hpp` | AI Chat, System |
| Storage | `storage.hpp` (via `Firmware`) | persistenza del workspace |
| Units | `point_a.hpp` | Conversion |
| Help | `cheatsheet.hpp` | Symbols |
| RunCore | `run_matrix.hpp` | Run-Matrix; previsto per eActivity e Program (righe di calcolo) |
| Models | `app_models.hpp` | modelli dati delle modalità future |
| GraphCore | `graph_model.hpp`, `graph_engine.hpp`, `graph_solve.hpp`, `table_model.hpp` | Graph, Table; previsto per Dyna Graph, Recursion, Conic Graphs (campionamento, viewport, G-Solve) — [GRAPH_ENGINE.md](GRAPH_ENGINE.md) |
| Files | `file_store.hpp`, `graph_file.hpp`, `pict.hpp` | Graph (progetti `.pcgraph`, immagini `.pcpict`, BMP sulla microSD); previsto per Picture Plot, Program, eActivity — [STORAGE.md](STORAGE.md) |
| Diag | `diagnostics.hpp` | System (SYSTEM → DIAG: test della microSD e tempi di GRAPH sul dispositivo, log USB) |

Errori: il motore lancia `SyntaxError`, `MathError`, `NonRealError` con testi
nello stile CG50; `AppManager` li mostra nella finestra di errore. Memoria:
nessuna allocazione dinamica nei percorsi ricorsivi; limiti espliciti
(token, annidamento, budget di stack) in [MATH_ENGINE.md](MATH_ENGINE.md).

## 4. Workspace: dati condivisi

| Sezione (`data::`) | Contenuto | Scritta da | Letta da |
|---|---|---|---|
| `Settings` | angolo, display, complessi, forme esatte, Frac Result, rete | System | tutte |
| `Memory` | A–Z, Ans, PreAns con la forma esatta | Run-Matrix, Equation, Conversion, Graph | tutte le modalità di calcolo |
| `History` | cronologia di Run-Matrix (con forma esatta e vista) | Run-Matrix | Memory |
| `Functions` | Y1–Y20: tipo (12), espressione, Yt, selezione, colore, stile | Graph, Table, Statistics (copia della regressione) | Graph, Table |
| `View` | V-Window (con T e Xdot) | Graph, Table (GPH-CON/PLT) | Graph, Table; Run-Matrix in sola lettura via `NameResolver` (VARS › V-WIN, `Xdot`) |
| `Table` | Start/End/Step, variabile (range/lista), tipo, righe generate, richiesta GPH-CON/PLT | Table, Graph (progetto) | Table, Graph |
| `Lists` | List 1–6 | Statistics, Table (LISTMEM) | Statistics, Graph e Table (grafici e tabelle da lista) |
| `Graph` | Setup di Graph, memorie V-Window e grafiche, SKETCH, immagine richiamata, progetto aperto | Graph | Graph, Table (Setup Derivative) |
| `Equation` | coefficienti, solver | Equation | Equation |
| `Chat` | conversazione AI | AI Chat | AI Chat |

Memory e System (RESET) possono cancellare ogni sezione. Le variabili di altre
modalità entrano in un'espressione **solo** tramite `NameResolver`
(`Workspace::resolve`): nomi `Xmin … Tptch`, `Xdot`, `TblStart/TblEnd/TblPitch`.
Le funzioni di Graph entrano in un'altra funzione solo tramite
`FunctionResolver` (riferimenti `Y1(x)` … delle funzioni composte).
Non esistono sistemi di variabili separati per modalità.

Persistenza: `serializeWorkspace()` (versione 4 dalla v1.8.0 con TABLE; le
versioni 1 della v1.6.0, 2 della v1.7.0 e 3 vengono ancora lette) → `RecordStore` con due slot
e CRC-32. I file dell'utente (progetti GRAPH, immagini) sono sulla microSD
tramite il servizio Files ([STORAGE.md](STORAGE.md)).

## 5. Modalità disponibili

| Modalità | Controller | Responsabilità | Servizi | Dati | Collegamenti |
|---|---|---|---|---|---|
| Run-Matrix | `RunMatrixApp` + nucleo `run_matrix.cpp` | calcoli e risultati: input naturale, forme esatte, cronologia modificabile, variabili, Base-N, DMS | Expression, Exact, Editor, MathRender, Format, MathCore, RunCore | Settings, Memory, History | — |
| Statistics | `StatisticsApp` | liste, 1/2-Var, regressioni, grafici statistici, DIST, TEST, INTR | Expression, Stats, Widgets | Settings, Lists, Functions | — |
| Graph | `GraphApp` (`graph_app.cpp`, `graph_plot.cpp`, `graph_files.cpp`) | grafici Y=, r=, parametrici, X=, disequazioni (Y1–Y20); TRACE, ZOOM, V-Window, SKETCH, G-SOLVE, Modify, memorie, PICT, progetti `.pcgraph` — [GRAPH_PARITY.md](GRAPH_PARITY.md) | Expression, Editor, Format, Widgets, MathRender, Storage, GraphCore, Files | Settings, Memory, Functions, View, Table, Lists, Graph | Table |
| Table | `TableApp` + `table_model` | tabelle di Y=, r= e parametriche da range o lista, derivate, EDIT/ROW/DELETE, LISTMEM, GPH-CON/GPH-PLT — [TABLE.md](TABLE.md) | Expression, Editor, Format, Widgets, MathRender, Storage, GraphCore | Settings, Memory, Functions, View, Table, Lists, Graph | Graph |
| Equation | `EquationApp` | sistemi, polinomi, solver, disequazioni | Expression, Solvers | Settings, Memory, Equation | — |
| Memory | `MemoryApp` | uso e cancellazione dei dati | Widgets, Storage | tutte | — |
| System | `SystemApp` | SET UP, retroilluminazione, Wi-Fi, relay, reset, DIAG | Widgets, Network, Diag | tutte (RESET) | — |
| Conversion | `ConversionApp` | conversioni di unità, risultato in Ans | Expression, Units | Settings, Memory | — |
| AI Chat | `AiChatApp` | chat con Claude/ChatGPT tramite relay | Network, Editor | Settings, Chat | System |
| Symbols | `SymbolsApp` | guida dei simboli | Help | — | — |

La separazione richiesta è rispettata e **verificata**: i grafici di funzioni
sono solo in Graph (Table li apre con G-CON/G-PLT), quelli statistici solo in
Statistics, le conversioni solo in Conversion, le tabelle solo in Table,
equazioni e disequazioni solo in Equation; Run-Matrix mostra solo calcoli e
risultati (nessun riferimento a grafici, liste, tabelle o conversioni).

## 6. Modalità non ancora disponibili

Per ognuna: responsabilità, componenti UI necessari, servizi, modello dati
(`include/app_models.hpp`, con validazione e test), dipendenze e test previsti.
I modelli **non** sono ancora nel record salvato: entreranno nel `Workspace`
(con una nuova versione del formato) nella sessione che realizza la modalità.

### eActivity — Planned
- **Responsabilità**: documenti con righe di testo e righe di calcolo.
- **UI**: editor di righe (testo con `LineEditor`, calcolo con `NaturalEditor`), elenco file.
- **Servizi**: RunCore (stessa valutazione e forme esatte di Run-Matrix), Exact, MathRender, Storage.
- **Modello**: `EActivityDocument { name, strips[] }`, `EActivityStrip { Text | Calculation, content }`.
- **Dipendenze**: Run-Matrix (nucleo), Memory (variabili). Area file: servizio Files sulla microSD (v1.8.0).
- **Test**: valutazione delle righe tramite `evaluateEntry`, serializzazione dei documenti.

### Spreadsheet — Planned
- **Responsabilità**: foglio A–Z × 999 righe, valori e formule `=A1+B2`, statistiche sulle colonne.
- **UI**: griglia scorrevole (`CellGrid` esteso), editor di cella, barra della formula.
- **Servizi**: Expression (con un `NameResolver` per i riferimenti di cella), Stats, Storage.
- **Modello**: `SpreadsheetModel` sparso (`set/get`), `CellRef` + `parseCellRef("B12")`.
- **Dipendenze**: Statistics (copia colonne ↔ liste tramite `Lists`).
- **Test**: riferimenti, ricalcolo, dipendenze circolari rifiutate.

### Dyna Graph — Planned
- **Responsabilità**: famiglia di grafici al variare di un coefficiente.
- **UI**: elenco delle 7 funzioni predefinite + personalizzata, form Start/End/Pitch/velocità, animazione.
- **Servizi**: Expression (`CompiledExpression` con il coefficiente legato), MathRender, Storage; V-Window condivisa.
- **Modello**: `DynaGraphModel` (funzione, coefficiente A–Z, start/end/pitch, Stop/Cont, locus, velocità), `frameCount()` ≤ 100.
- **Dipendenze**: `View` (V-Window di Graph). Nessun codice di Graph copiato: dalla v1.8.0 il campionamento è nel servizio condiviso GraphCore (`graph::sampleStep`, `GraphEngine`, `Viewport`).
- **Test**: numero di fotogrammi, valori del coefficiente, limiti.

### Recursion — Planned
- **Responsabilità**: successioni an, an+1, an+2 (fino a tre), tabella, grafico, diagramma di fase.
- **UI**: elenco formule, tipo, SET (intervallo n, termini iniziali), tabella, grafico.
- **Servizi**: Expression (variabili `n`, `an`, `an1` legate), Storage; V-Window condivisa.
- **Modello**: `RecursionModel` (tipo, formule, selezione, n 0…999, termini iniziali a0/a1 o a1/a2).
- **Test**: Fibonacci e an+1 = 2an+1 (esempi del manuale 5-45/5-46), limiti di n.

### Conic Graphs — Planned
- **Responsabilità**: parabole, circonferenze, ellissi, iperboli in forma cartesiana, polare, parametrica.
- **UI**: scelta del modello di equazione, coefficienti, grafico con G-Solve specifici (fuochi, vertici…).
- **Servizi**: Expression, Solvers, Storage; V-Window condivisa.
- **Modello**: `ConicModel` (forma generale A…F) + `classify()` (discriminante e determinante).
- **Test**: classificazione (già presente), punti notevoli.

### Program — Planned
- **Responsabilità**: programmi in un **linguaggio proprio** (non il codice Casio) che usano motore e workspace.
- **UI**: elenco file, editor di testo, esecuzione con output sul display.
- **Servizi**: Expression, Exact, RunCore, Storage.
- **Modello**: `SourceFile { name, text }`, nomi validati da `validateProgramName()` (1–8 caratteri).
- **Dipendenze**: accesso a Memory, Lists, Functions, View tramite il workspace.
- **Test**: interprete (controllo di flusso, limiti di ricorsione/tempo), file.

### Financial — Planned
- **Responsabilità**: TVM (interesse semplice e composto), flussi di cassa, ammortamento, conversioni di tasso, date.
- **UI**: form delle variabili, SOLVE della variabile scelta, grafici dei flussi.
- **Servizi**: Expression, Solvers (equazione TVM), Storage.
- **Modello**: `FinancialModel` (n, I%, PV, PMT, FV, P/Y, C/Y, BEGIN/END, 365/360).
- **Test**: valori di riferimento del capitolo 7 del manuale.

### E-CON4 — External
- Richiede un data logger esterno e il relativo protocollo: **non realizzabile** con l'hardware della PicoCalc.
- Parte realizzabile in futuro: importazione di campioni da file CSV nelle liste (`Lists`) e analisi in Statistics.

### Link — Planned
- **Responsabilità**: trasferimento del workspace verso un PC via USB seriale con il **formato proprio** (record versionato con CRC).
- Il protocollo Link Casio tra calcolatrici **non** è supportato.
- **Servizi**: Storage (stesso formato di `serializeWorkspace`), Widgets.
- **Test**: round trip del record, rifiuto di dati corrotti (già coperto da `storage_test`).

### Geometry — Planned
- **Responsabilità**: costruzioni (punti, segmenti, rette, semirette, circonferenze, poligoni) e misure.
- **Servizi**: Expression, MathCore (vettori), Storage.
- **Modello**: `GeometryObject { kind, coordinates }` con `validate()`.
- **Test**: misure (distanze, aree, angoli) con valori noti.

### Picture Plot — Planned
- **Responsabilità**: punti su un'immagine di sfondo, liste di coordinate e regressioni.
- **Servizi**: Stats (regressioni), Storage, Files; immagini dalla microSD (`.pcpict`).
- **Modello**: `PicturePlotModel { background, points[] }`.
- **Dipendenze**: `Lists` e `Functions` (copia della regressione, come Statistics).

### 3D Graph — Planned
- **Responsabilità**: superfici z = f(x, y) (fino a 3), finestra 3D, rotazione.
- **Servizi**: Expression (`CompiledExpression::eval2`), Storage. Esiste già `sample3DSurface()` in `graph.hpp`.
- **Modello**: `Graph3DModel` (funzioni, selezione, finestra x/y/z, griglia 5–50).
- **Test**: proiezione e campionamento.

### Python — Planned
- **Responsabilità**: script MicroPython.
- **Requisiti**: integrazione del runtime come libreria con **heap separato** (la RAM disponibile è ~200 KB) e I/O sul display; file `.py` (`validateScriptName()`).

## 7. Verifica: `tests/architecture_test.cpp`

| Controllo | Come |
|---|---|
| Registro coerente | ID in ordine, tasti e nomi unici, `Available` ⇔ `sources` ⇔ nessun `missing`, collegamenti solo verso modalità disponibili, file e header esistenti |
| Registrazione nel firmware | il `Firmware` reale apre ogni modalità: le `Available` hanno il proprio controller, le altre la schermata "non disponibile"; ogni schermata si disegna senza errori; ogni tasto di accesso diretto apre la modalità giusta |
| Servizi | ogni `#include` dei sorgenti di una modalità appartiene ai servizi dichiarati |
| Dati | ogni `ws.<sezione>` usato è dichiarato (`resetAll` richiede tutte le sezioni) |
| Collegamenti | ogni `open(AppId::X)` è dichiarato |
| Run-Matrix | nessun riferimento a grafici, statistiche, tabelle, liste, conversioni |
| Stratificazione | gli header e i sorgenti del motore non includono UI/app; nessun ciclo di include |
| Modelli | validazioni e helper dei modelli delle modalità future |

Il test è stato provato introducendo apposta in Run-Matrix un `#include
"statistics.hpp"`, un accesso a `ws.view` e un `open(AppId::Graph)`: fallisce
con 5 errori; senza la modifica passa (468 controlli).

## 8. Come aggiungere una modalità

1. Implementare il nucleo (logica senza UI) usando i servizi esistenti; test host.
2. Spostare il modello da `app_models.hpp` nel `Workspace`, aggiungerlo alla
   serializzazione con una nuova versione (mantenendo la lettura delle precedenti).
3. Scrivere il controller in `src/apps/<nome>_app.cpp`, registrarlo in `Firmware`.
4. Aggiornare la voce del registro (`status`, `services`, `data`, `links`, `sources`).
5. `architecture_test` verifica che dichiarazioni e sorgenti coincidano.
6. Aggiornare [FEATURE_MATRIX.md](FEATURE_MATRIX.md) e questo documento.
