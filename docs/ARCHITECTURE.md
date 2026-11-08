# Architettura — v1.8.0

Architettura di ogni modalità, servizi condivisi, dati scambiati e regole
verificate dai test: [APPLICATIONS_ARCHITECTURE.md](APPLICATIONS_ARCHITECTURE.md).

## Strati

```
┌──────────────────────────────────────────────────────────────────────┐
│ Applicazioni (src/apps/)                                             │
│ Launcher · Run-Matrix · Statistics · Graph · Table · Equation ·      │
│ System · Memory · Conversion · AI Chat · Symbols · "non disponibile" │
├──────────────────────────────────────────────────────────────────────┤
│ Framework app (app.hpp, app_manager.cpp, app_widgets.hpp)            │
│ Registro (app_registry): stato, servizi, dati, collegamenti          │
│ AppManager: routing tasti, MENU/SHIFT/EXIT lungo, errori → box CG50  │
│ Context: Workspace + Platform + clipboard + toast                   │
├───────────────────────────┬──────────────────────────────────────────┤
│ UI kit (ui_kit, widgets)  │ Motore matematico                        │
│ Natural editor + renderer │ expression (tokenizer→AST→valutatori     │
│ font_renderer, graphics   │ numerico ed esatto), exact, run_matrix,  │
│ (Canvas RGB565, TileDiff) │ number_format, solvers, statistics,      │
│                           │ math_engine, point_a, app_models,        │
│                           │ graph_engine, graph_solve (GraphCore)    │
├───────────────────────────┴──────────────────────────────────────────┤
│ File (file_store, graph_file, pict) → FatFs (third_party/fatfs) →    │
│ protocollo SD (sd_card) su un bus SPI astratto                       │
├──────────────────────────────────────────────────────────────────────┤
│ Firmware (firmware.cpp): boot, decoder tasti, autosalvataggio, redraw│
├──────────────────────────────────────────────────────────────────────┤
│ Porta board (board_port.hpp)                                         │
│  ├─ Pico: board_pico.cpp (LCD, tastiera, flash), sd_spi_pico.cpp     │
│  │        (microSD su SPI0), net_pico.cpp (Wi-Fi, TLS), main_pico.cpp│
│  └─ Host: board_host.cpp (scheda microSD in RAM) + simulator.cpp     │
└──────────────────────────────────────────────────────────────────────┘
```

Tutto ciò che sta sopra la porta board è **portabile** e viene compilato sia per
l'RP2350 sia per il PC con la stessa lista di sorgenti (`cmake/fxai_sources.cmake`):
la logica testata sul PC è esattamente quella del firmware.

## Ciclo principale (firmware e simulatore)

```
board::init() → Firmware::boot()   (carica il workspace dalla flash)
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
range di Table, liste 1–6, stato di Equation, conversazione AI. I file
dell'utente (progetti GRAPH, immagini) sono sulla microSD
([STORAGE.md](STORAGE.md)).
Le app comunicano tramite questo modello (es. Statistics copia una regressione in
Graph, Graph apre Table con G↔T, Table imposta la finestra di Graph con G-PLT).

Il workspace implementa anche `NameResolver`: Run-Matrix legge in sola lettura
le variabili di Graph e Table (`Xmin`, `TblStart`, …) senza accedere direttamente
a quelle sezioni.

`serializeWorkspace()` produce un blob binario versionato (versione 3 dalla
v1.8.0; le versioni 1 e 2 vengono ancora lette); `RecordStore` lo scrive in uno dei due
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
successivo la chiude e l'input resta modificabile. Nessuna eccezione esce dal ciclo
principale.

## Memoria

- Framebuffer statico 320×320 RGB565 (200 KB); nessun secondo framebuffer: le
  modifiche si rilevano con hash per tile (1,6 KB).
- Stack di 32 KB in SRAM (linker script `src/platform/pico/memmap_picocalc.ld`).
- Heap ≈194 KB per AST, liste, storia, mbedTLS (handshake TLS ~50–60 KB) e
  GRAPH (punti delle curve ≤ 96 KB, immagini RLE ≤ 96 KB ciascuna, volume
  FatFs).
- Limiti espliciti: storia 40 voci, liste 255 elementi, chat 30 messaggi,
  risposta HTTP 16 KB, Σ/Π 100001 termini, tabella 500 righe.
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
| `tests/` | test host (CTest); `tests/sim/` script degli screenshot |
| `relay/` | server relay AI (Python) e test |
| `tools/gen_fonts.py` | generatore dei font bitmap |
| `certs/relay_ca.pem` | CA fidate per il relay (ISRG Root X1/X2) |
| `cmake/` | lista sorgenti condivisa e template |
