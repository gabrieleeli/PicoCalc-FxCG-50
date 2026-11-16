# Architettura — v1.13.0

Architettura di ogni modalità, servizi condivisi, dati scambiati e regole
verificate dai test: [APPLICATIONS_ARCHITECTURE.md](APPLICATIONS_ARCHITECTURE.md).

## Strati

```
┌──────────────────────────────────────────────────────────────────────┐
│ Applicazioni (src/apps/)                                             │
│ Launcher · Run-Matrix · Statistics · Graph · Table · Equation ·      │
│ 3D Graph · System · Memory · Conversion · AI Chat · Symbols ·        │
│ "non disponibile"                                                    │
├──────────────────────────────────────────────────────────────────────┤
│ Framework app (app.hpp, app_manager.cpp, app_widgets.hpp)            │
│ Registro (app_registry): stato, servizi, dati, collegamenti          │
│ AppManager: routing tasti, MENU/SHIFT/EXIT lungo, errori → box CG50  │
│ Context: Workspace + Platform + clipboard + toast                    │
├───────────────────────────┬──────────────────────────────────────────┤
│ UI kit (ui_kit, widgets)  │ Motore matematico                        │
│ Natural editor + renderer │ expression (tokenizer→AST→valutatori     │
│ font_renderer, graphics   │ numerico ed esatto), exact, run_matrix,  │
│ (Canvas RGB565, TileDiff) │ number_format, solvers, statistics,      │
│ graph3d (superfici 3D)    │ math_engine, point_a, app_models,        │
│                           │ graph_engine, graph_solve (GraphCore),   │
│                           │ graph_commands (comandi grafici, v1.11)  │
│                           │ calc_library (CONVERT, DIST, TVM, v1.12) │
├───────────────────────────┴──────────────────────────────────────────┤
│ File (file_store, graph_file, pict, csv_io) → FatFs → protocollo SD  │
│                                     → LittleFS (flash_fs) → flash    │
├──────────────────────────────────────────────────────────────────────┤
│ Firmware (firmware.cpp): boot, migrazione, decoder tasti,            │
│ autosalvataggio, redraw                                              │
├──────────────────────────────────────────────────────────────────────┤
│ Memoria (v1.9.0): mem_pools (heap SRAM + PSRAM TLSF), operator new   │
│ instradato, limits (persistenti / RAM / strutturali), psram (driver  │
│ e self-test), system_runtime (MSPLIM, HardFault, core 1), sysinfo    │
├──────────────────────────────────────────────────────────────────────┤
│ Porta board (board_port.hpp) + board_config.hpp (pin, risorse, flash)│
│  ├─ Pico: board_pico.cpp (PSRAM, LCD, tastiera, regioni flash),      │
│  │        sd_spi_pico.cpp (microSD su SPI0), net_pico.cpp (Wi-Fi,    │
│  │        TLS), main_pico.cpp (avvio, rapporto guasti, ciclo)        │
│  └─ Host: board_host.cpp (microSD e flash in RAM) + simulator.cpp    │
│ Schede: Pimoroni Pico Plus 2 W (principale), Raspberry Pi Pico 2 W   │
└──────────────────────────────────────────────────────────────────────┘
```
Tutto ciò che sta sopra la porta board è **portabile** e viene compilato sia per
l'RP2350 sia per il PC con la stessa lista di sorgenti (`cmake/fxai_sources.cmake`):
la logica testata sul PC è esattamente quella del firmware.

## Ciclo principale (firmware e simulatore)

```
paintStacks(), takeLastFault()     (guardia MSPLIM attiva dal reset)
board::init()                      PSRAM + self-test, pool, LCD, tastiera, JEDEC della
                                   flash, regioni flash (limitate al chip reale)
Firmware::boot()                   carica il workspace (o migra quello della v1.8.0)
avvisi                             UF2 sbagliato, guasto precedente o PSRAM disattivata
loop:
  board::service()                 tastiera I2C non bloccante, batteria, Wi-Fi poll
  while board::pollKey(raw): Firmware::rawKey(raw) → KeyDecoder → AppManager::key
  if Firmware::step():             tick dell'app attiva, toast, autosalvataggio
      Firmware::draw(canvas)       l'app ridisegna tutto il frame
      board::present()             invia al display solo i tile 16×16 cambiati
  board::idle()
```

## Dati condivisi: `Workspace`

`include/workspace.hpp` contiene tutto lo stato persistente: impostazioni,
memoria (A–Z, Ans/PreAns anche complessi, con la forma esatta), storia di
Run-Matrix (con forma esatta e vista), funzioni Y1–Y20 e V-Window di Graph,
stato di Graph (Setup, memorie V-Window e grafiche, SKETCH, progetto aperto),
range, variabile, tipo e righe di Table, liste 1–26 con i sottonomi (v1.11.0), stato di Equation,
conversazione AI e, dalla v1.10.0, le memorie di matrici e vettori
(`CollectionMemory`: Mat A–Z, Vct A–Z, MatAns, VctAns, ListAns) e la modalità
Input/Output. I file
dell'utente (progetti GRAPH, immagini) sono sulla microSD
([STORAGE.md](STORAGE.md)).
Le app comunicano tramite questo modello (es. Statistics copia una regressione in
Graph, Graph apre Table con G↔T, Table imposta la finestra di Graph con G-PLT).

Il workspace implementa anche `NameResolver`: Run-Matrix legge in sola lettura
le variabili di Graph e Table (`Xmin`, `TblStart`, …) senza accedere direttamente
a quelle sezioni. Dalla v1.10.0 implementa anche `CollectionResolver`: il
valutatore legge `Mat A`, `Vct B`, `List 1` (le liste di Statistics) e le
memorie delle risposte tramite questa interfaccia; le assegnazioni passano da
metodi controllati (`storeMatrix`, `storeVector`, `storeList`,
`storeCollectionAnswer`) che verificano tipo, forma e limiti della scheda. Così
Run-Matrix non accede direttamente a `ws.lists` (lo verifica `architecture_test`).

`serializeWorkspace()` produce un blob binario versionato (versione 8 dalla
v1.13.0 con List File 1–6, Sub Name, r e θ, Str, function memory, variabili
di Dyna/Recursion/TVM, celle decimali, ENG e ◢ della cronologia; versione 8 dalla
v1.12.0 con i dati di EQUATION in coda e le forme esatte dei coefficienti;
versione 7 dalla
v1.11.0 con Base-N, Simplify, formato CSV, List 7–26 e sottonomi, forme
esatte delle memorie, base e ▶Simp della cronologia, grafici dei comandi;
versione 6 dalla v1.10.0 con Input/Output, matrici, vettori e risultati
collezione della cronologia; versione 5 dalla v1.9.0 con il 3D Graph e
l'archivio dei file; le versioni 1–7 vengono ancora lette); `RecordStore` lo scrive in uno dei due
slot flash con CRC-32 dell'intestazione e del contenuto. Formato in
[STORAGE.md](STORAGE.md#workspace-in-flash).

## Motore matematico

Una sola rappresentazione (`ast::Node`) è condivisa da editor, parser, valutatori
(numerico ed esatto) e renderer. L'editor costruisce nodi Row/Token/Fraction/Radical/Power/Calculus/Call/Matrix;
`ast::serialize` produce il testo lineare canonico, che il parser trasforma in nodi
Binary/Unary/Call/Matrix/List valutati da un unico valutatore templato su `double` (modo Real) e
`std::complex<double>` (modi a+bi e r∠θ). Vedi [MATH_ENGINE.md](MATH_ENGINE.md).

### Matrici, vettori e liste (v1.10.0)

```
testo ──Lexer (token con posizione)──▶ Parser ──▶ ast::Node (pos, len, FlagCollection)
                                                     │
                 ┌───────────────────────────────────┴────────────────────────┐
     nodo senza collezioni                                       nodo con collezioni
     evalNode<double|complex>, exactNode                         CollEval<NumDomain|ExactDomain>
     (valutatori scalari della v1.9.0)                           (+ − × ^ Det Trn DotP Sum Seq…)
                 └───────────────◀── sotto-espressioni numeriche ──────────────┘
                                                     │
                       CalcValue (numero, lista, matrice, vettore) / ExactCollection
```

- **Un solo parser**: i letterali `[[…][…]]` e `{…}`, i riferimenti `Mat A`,
  `Vct A`, `List n` e gli indici `[…]` sono produzioni della stessa grammatica;
  ogni nodo ha l'intervallo di byte del testo (`pos`, `len`) e l'indicatore
  `FlagCollection` quando il suo sotto-albero contiene collezioni.
- **Un solo valutatore per tipo di elemento**: `CollEval` è un template sul
  dominio degli elementi (`NumDomain`: reale o complesso secondo il Complex
  Mode; `ExactDomain`: razionali, radicali, multipli di π). Le sotto-espressioni
  numeriche passano ai valutatori scalari esistenti, quindi un numero ha
  esattamente il comportamento di prima ovunque (GRAPH, TABLE, Equation…).
- **Valore unificato**: `CalcValue` (`include/value.hpp`) è usato dal valutatore,
  dalle memorie del workspace, dall'editor MAT/VCT e dalla visualizzazione; le
  parti immaginarie esistono solo se servono (8 byte per elemento reale).
- **Errori con posizione**: `SyntaxError`/`MathError` hanno `where`
  (`ErrorLocation`); il parser indica il token, il valutatore la
  sotto-espressione in corso (`EvalInfo::error`, registrata da una guardia RAII
  solo mentre l'eccezione risale: nessun costo per GRAPH che campiona in un
  ciclo). `ast::SerializeMap` riporta la posizione del testo nell'albero
  dell'editor (`NaturalEditor::markError`).

### Novità del motore e dei dati (v1.13.0, Run-Matrix)

- **Parser, AST e valutatore unici**: SolveN, FMin, FMax, distribuzioni, TVM,
  CONVERT, ∠, ʳ ᵍ, ° ′ ″, stringhe, Str, function memory e variabili di VARS
  sono token e nodi dello stesso parser; `:`/`◢` dividono la riga prima del
  parser. `architecture_test` verifica che nessun altro sorgente definisca un
  lexer, un parser o istanze del valutatore.
- **`calc_library`**: unità, distribuzioni, TVM, nomi e CATALOG; puro calcolo.
- **Interfacce del workspace per il valutatore**: `CollectionResolver::listByName`
  (List "QTY"), `CollectionResolver::special` (TABLE Result; VARS EQUATION da `Memory::Equation`),
  `ServiceResolver` (PxlTest(, function memory), `NameResolver` (FACTOR, STAT,
  DYNA, RECURSION, TVM). Run-Matrix assegna i dati solo con metodi controllati
  (`storeSystemVariable`, `setString`, `setFunctionMemory`, `setRecursionFormula`,
  `setList`, `storeList`, `storeMatrix`); List File si cambia solo dal SET UP
  (`selectListFile`). Nuova sezione dati `Strings` nel registro delle app.
- **PxlTest(**: `Workspace::pixelProbe` (non salvato) è collegato
  dall'AppManager a `GraphApp::probePixel`, che disegna la schermata del
  grafico nel framebuffer e ne legge il pixel: Run-Matrix non disegna nulla.
- **Comandi grafici**: nuovi elementi SKETCH `Integral` e `Data`, grafici dei
  comandi di tipo disequazione, richiesta `GraphState::storePict`; tutto
  disegnato dal motore e dal renderer di GRAPH e salvato nei `.pcgraph`.

### EQUATION (v1.12.0)

```
EquationApp (schermate del manuale)   MathField (NaturalEditor + math_renderer)
   │ testo delle celle / dell'equazione
   ▼
equation.hpp (motore, senza UI) ── parseExpression / evaluateAny / evaluateExact
   │ eq::Number (valore + forma esatta complessa)
   │ solveSimultaneous · solvePolynomial · solveFor · numberTree
   ▼
ws.equation (record v8: coefficienti, equazione, limiti, SimRes/PlyRes)
   │ CollectionResolver::Memory::Equation
   ▼
Run-Matrix: VARS › EQUATION (SimRes, SimCoef, PlyRes, PlyCoef → MatAns)
```

- Nessun parser separato: anche i nomi `SimRes`… sono token dello stesso
  lexer; Run-Matrix non vede `ws.equation` né il motore di EQUATION
  (`architecture_test`), ma solo le matrici attraverso `CollectionResolver`.
- Le soluzioni si disegnano con gli alberi 2D di Run-Matrix
  (`exactValueTree`, `appendNumberTree`) e `math_renderer`.
- Il SET UP aperto da una modalità torna a quella modalità
  (`Context::leaveSetup`, `Context::resuming`).

### Novità del motore e dei dati (v1.11.0)

```
Run-Matrix (riga di input)
   │ "Plot 1,2", "Graph Y=x^2"…           │ espressione (anche ▶Simp, Base-N)
   ▼                                      ▼
graph_commands (motore, senza UI)     run_matrix::evaluateEntry
   │ argomenti → parseExpression          │ parseExpression(testo, base)
   │            + evaluateReal            │ EvalContext::integerBase
   ▼                                      ▼
ws.view, ws.graph.sketch,             CalcValue / exact::Value
ws.graph.commandGraphs                     │
   │ (GraphState::commandScreen)           ▼
   ▼                                   cronologia (base, frazione ▶Simp)
GRAPH: GraphEngine::build(…, commands) → curve e SKETCH disegnati da GRAPH
```

- **Comandi grafici**: Run-Matrix non disegna. `graphcmd::execute` separa gli
  argomenti, li valuta con l'unico parser/valutatore e scrive V-Window,
  elementi SKETCH e grafici dei comandi nel workspace (tutto o niente);
  Run-Matrix apre GRAPH (collegamento dichiarato), che li disegna con il suo
  motore (`GraphSource::commands`) e il suo renderer e con EXIT torna a
  Run-Matrix. `architecture_test` lo verifica (nessun `ws.graph`, `SketchItem`,
  `graph::` o primitiva di disegno nei sorgenti di Run-Matrix).
- **Base-N**: il lexer riceve la base (numeri senza prefisso, cifre A–F,
  prefissi 0d/0x/0b/0o), il valutatore applica la semantica intera a 32 bit
  quando `EvalContext::integerBase` è impostato (`combine()`: + − × ÷ interi,
  bit a bit per And/Or/Xor/Xnor, Not/Neg); lo stesso albero e lo stesso
  valutatore del calcolo normale.
- **CSV**: `csv_io` (motore) codifica/decodifica e scrive con
  `files::writeAtomic` su `files::FileSystem` (microSD o LittleFS);
  `loadLists`/`loadMatrix` scrivono nel workspace con i controlli della scheda.
- **Liste**: 26 liste nel workspace (`lists`, `listNames`), budget di tutte le
  liste (`limits::kListStoreElements`), metodi `setList`, `checkListLength`,
  `listName`, `setListName`; il registro associa membri e metodi alla
  sezione dati *Lists* (`dataSectionOfMember`).
- **Forme esatte delle memorie**: `CalcValue::exact` (fino a 64 elementi); il
  dominio esatto di `CollEval` le usa al posto dei soli interi.

### Editor (v1.10.0, v1.11.0, v1.12.0)

Dalla v1.12.0 `ast::serialize` racchiude tra parentesi un modello di frazione
seguito da un fattore implicito (`((1)/(2))X`, prima `(1)/(2)X` = 1/(2X)) e
l'importazione lo riconosce come frazione; l'operando di x²/^ è il numero o la
sola variabile prima del cursore (`GT²` = G·T²). Il widget `MathField`
(`src/apps/math_field.cpp`) porta lo stesso editor nelle celle di EQUATION e
nel Solver.

Dalla v1.11.0 l'editor ha anche la sovrascrittura in Line (`setOverwrite`),
l'argomento di SHIFT+INS in Math (`beginArgument`: il prossimo modello prende
il valore a destra) e scrive `⌟` con il tasto frazione in Line; il renderer
disegna il cursore in tre forme (`CursorTarget::cursorStyle`).


`NaturalEditor` ha due modalità (`setLinear`): Math (modelli 2D, importazione
strutturale) e Line (solo token, testo lineare). La selezione CLIP è un
intervallo della riga attiva (`Range`), estesa per bilanciare le parentesi;
il renderer (`math::layout`) riceve in `CursorTarget` l'intervallo da
evidenziare e la richiesta di colorare le parentesi per livello, e produce
operazioni `Highlight` e testi con un indice di colore (`NaturalDrawOp::color`)
che `math::draw` traduce con una `Palette`.

## Gestione errori

Il motore lancia `SyntaxError`, `MathError`, `NonRealError` con testo nello stile
Casio ("Math ERROR: divisione per zero"). `AppManager` intercetta ogni eccezione
(anche `std::bad_alloc` → "Memory ERROR") e mostra la finestra di errore; il tasto
successivo la chiude e l'input resta modificabile. Dalla v1.9.0 il percorso
dell'errore (messaggio, toast, finestra) usa buffer riservati e non alloca,
l'autosalvataggio intercetta `std::bad_alloc` e il ciclo principale del firmware
intercetta ogni eccezione residua (`oom_test`).

## Memoria

- Framebuffer statico 320×320 RGB565 (200 KB) in SRAM; nessun secondo
  framebuffer: le modifiche si rilevano con hash per tile (1,6 KB).
- Stack di 32 KB in SRAM (linker script `src/platform/pico/memmap_picocalc.ld`)
  con guardia MSPLIM.
- Heap SRAM ≈ 186–190 KB per AST, stringhe brevi, mbedTLS (handshake TLS
  ~50–60 KB), lwIP; sulla **Pimoroni Pico Plus 2 W** il pool **PSRAM da 8 MB**
  riceve ogni allocazione C++ da 512 byte in su (`operator new` instradato):
  punti delle curve di GRAPH, layer di GRAPH durante TRACE, immagini, mesh e
  z-buffer 3D, liste, cronologia, workspace serializzato. Regole e mappa:
  [PICO_PLUS_2W.md](PICO_PLUS_2W.md).
- Limiti (`limits.hpp`): persistenti per scheda (Pico 2 W: storia 40, liste
  255 e 1 530 elementi in tutte le 26 liste; Pico Plus 2 W: storia 200, liste
  999 e 20 000 elementi in tutto), RAM dai pool presenti (curve
  6000/24 000 o 40 000/400 000 punti, PICT 96 KB o 1 MB, 3D griglia 24 o 128),
  chat 30 messaggi, risposta HTTP 16 KB, tabella 999 righe.
- Limiti contro l'esaurimento dello stack: espressioni di 320 token e 32 livelli
  annidati, editor con 12 modelli annidati e 400 nodi, budget di 16 KB per
  valutazione (vedi [MATH_ENGINE.md](MATH_ENGINE.md)).

## Struttura delle cartelle

| Percorso | Contenuto |
|---|---|
| `include/`, `src/` | codice portabile |
| `src/apps/` | modalità e widget applicativi |
| `src/platform/pico/` | porta RP2350, configurazioni lwIP/mbedTLS, linker script |
| `src/platform/host/` | porta host e simulatore |
| `third_party/fatfs/` | FatFs R0.16 (non modificato) e `ffconf.h` del progetto |
| `third_party/littlefs/` | LittleFS v2.11.3 (non modificato) |
| `tests/` | test host (CTest); `tests/sim/` script degli screenshot |
| `relay/` | server relay AI (Python) e test |
| `tools/gen_fonts.py` | generatore dei font bitmap |
| `certs/relay_ca.pem` | CA fidate per il relay (ISRG Root X1/X2) |
| `cmake/` | lista sorgenti condivisa, selezione della scheda e partizioni (`fxai_board.cmake`), controllo della dimensione, template |
| `scripts/` | `build.sh --board`, `build_pico2w.sh`, `make_release.sh` |
