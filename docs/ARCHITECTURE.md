# Architettura — v1.9.0

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
│                           │ graph_engine, graph_solve (GraphCore)    │
├───────────────────────────┴──────────────────────────────────────────┤
│ File (file_store, graph_file, pict) → FatFs → protocollo SD (SPI)    │
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
range, variabile, tipo e righe di Table, liste 1–6, stato di Equation,
conversazione AI. I file
dell'utente (progetti GRAPH, immagini) sono sulla microSD
([STORAGE.md](STORAGE.md)).
Le app comunicano tramite questo modello (es. Statistics copia una regressione in
Graph, Graph apre Table con G↔T, Table imposta la finestra di Graph con G-PLT).

Il workspace implementa anche `NameResolver`: Run-Matrix legge in sola lettura
le variabili di Graph e Table (`Xmin`, `TblStart`, …) senza accedere direttamente
a quelle sezioni.

`serializeWorkspace()` produce un blob binario versionato (versione 5 dalla
v1.9.0 con il 3D Graph e l'archivio dei file; le versioni 1–4 vengono ancora
lette); `RecordStore` lo scrive in uno dei due
slot flash con CRC-32 dell'intestazione e del contenuto.

## Motore matematico

Una sola rappresentazione (`ast::Node`) è condivisa da editor, parser, valutatori
(numerico ed esatto) e renderer. L'editor costruisce nodi Row/Token/Fraction/Radical/Power/Calculus/Call;
`ast::serialize` produce il testo lineare canonico, che il parser trasforma in nodi
Binary/Unary/Call valutati da un unico valutatore templato su `double` (modo Real) e
`std::complex<double>` (modi a+bi e r∠θ). Vedi [MATH_ENGINE.md](MATH_ENGINE.md).

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
- Limiti (`limits.hpp`): persistenti per scheda (Pico 2 W: storia 40, liste 255;
  Pico Plus 2 W: storia 200, liste 999), RAM dai pool presenti (curve
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
