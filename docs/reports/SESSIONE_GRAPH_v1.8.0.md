# Report — Sessione GRAPH (v1.7.0 → v1.8.0)

Branch di lavoro `claude/loving-meitner-jifxhg`, pubblicato anche su
`version/v1.8.0` (creato da `version/v1.7.0`, commit `72285e9`).
Riferimento funzionale: *fx-CG50 Software User's Guide* 3.50, capitolo 5 e
voci di Setup del capitolo 1, usato solo come descrizione del comportamento.

## 1. Stato iniziale

- Branch verificati: `main`, `version/v1.5.4`, `version/v1.6.0`,
  `version/v1.7.0` (completo: build UF2 e 17 suite di test superate), branch
  `claude/*` delle sessioni precedenti. `version/v1.8.0` non esisteva ed è
  stato creato da `version/v1.7.0`.
- GRAPH in v1.7.0 (audit completo in [GRAPH_PARITY.md](../GRAPH_PARITY.md#0-audit-iniziale-v170-commit-72285e9)):
  un solo file di 516 righe con calcolo e interfaccia mescolati; 6 funzioni
  (Y=, r=, 4 disequazioni Y); un punto per colonna unito al successivo (asintoti
  disegnati come segmenti); V-Window senza T né memorie; ZOOM ridotto; SKETCH
  solo tangente/normale; nessun X=, parametrica, disequazione X, intervallo,
  sovrapposizione, funzione composta, stile di linea, memoria grafica, PICT,
  progetto o microSD; nessun test dedicato.

## 2. Implementazione

| Modulo | File | Contenuto |
|---|---|---|
| Model | `graph_model.hpp/.cpp` | funzioni (20), 12 tipi, stili, colori, V-Window, Setup, SKETCH, memorie, validazione |
| Engine / Evaluator | `graph_engine.hpp/.cpp`, `expression.cpp` | compilazione della lista con il parser condiviso, intervalli, sovrapposizioni, liste, riferimenti `Y1(x)` (FunctionResolver, ≤ 5 livelli), grafici incollati |
| Transform / Viewport | `graph_engine.hpp` | colonne ↔ x, righe ↔ y, Xdot/Ydot, clipping Liang–Barsky |
| Sampling | `graph_engine.cpp` | campionamento adattivo incrementale: poli, salti, bordi di dominio, Plot, bordi delle disequazioni, budget di punti |
| Renderer | `apps/graph_plot.cpp` | livelli (sfondo, riempimenti, area, griglia, assi, immagine, curve, SKETCH, punti, testi), penne, disegno non bloccante, Simul Graph |
| Controller / UI | `apps/graph_app.cpp` | lista delle relazioni, editor, TYPE/CONVERT, TOOL, V-Window, Setup proprio, FORMAT, Modify, menu |
| Tools | `graph_solve.hpp/.cpp`, `apps/graph_plot.cpp` | TRACE, ZOOM (13), SKETCH (12), G-SOLVE (8 + 4 ∫dx), PAN, incolla |
| Persistence | `graph_file`, `pict`, `file_store`, `apps/graph_files.cpp`, `workspace.cpp` | `.pcgraph`, `PCGMEM`, `.pcpict`, BMP, scrittura atomica, recupero, workspace v3 |
| microSD | `sd_card`, `fatfs_store`, `third_party/fatfs`, `platform/pico/sd_spi_pico.cpp` | protocollo SD in SPI, FatFs R0.16, SPI0 della PicoCalc |
| TABLE | `apps/table_app.cpp` | stesso motore, dY/dX, GPH-CON/GPH-PLT |

Il driver microSD non usa API inventate: il bus usa solo `hardware_spi` e
`hardware_gpio` del Pico SDK e i pin dei sorgenti ufficiali ClockworkPi; tutto
il resto (protocollo, FatFs, file) è portabile e provato sul PC con una scheda
simulata, come chiesto ("interfacce testabili con mock").

## 3. Funzionalità GRAPH

Stato voce per voce in [GRAPH_PARITY.md](../GRAPH_PARITY.md) (132 voci).

| Gruppo | Stato |
|---|---|
| Tipi di grafico, lista, editor, stili, colori, SELECT, DELETE, CONVERT, BUILT-IN | TESTED ON SIMULATOR / HOST |
| V-Window (10 campi, preset, memorie, SQUARE, BGV-WIN), Setup (16 voci) | TESTED ON SIMULATOR / HOST |
| Disegno: discontinuità, domini, clipping, disequazioni Union/Intsect, Simul Graph, Draw Type, disegno incrementale | TESTED ON HOST |
| TRACE, ZOOM (13 funzioni), PAN, incolla | TESTED ON HOST |
| G-SOLVE: ROOT, MAX, MIN, Y-ICEPT, INTSECT, Y-CAL, X-CAL, ∫dx, ∫dx ROOT/INTSECT/MIXED | TESTED ON SIMULATOR / HOST |
| SKETCH: Cls, Tangent, Norm, Inverse, PLOT (4), LINE (2), Circle, Vertical, Horz, PEN, Text, elenco | TESTED ON HOST |
| Modify, memorie grafiche, PICT, sfondo e Fade, esportazione BMP | TESTED ON HOST |
| Progetti `.pcgraph` sulla microSD, ciclo completo 12.4 | TESTED ON SIMULATOR / HOST (scheda simulata) |
| TABLE: dY/dX, GPH-CON/GPH-PLT | TESTED ON SIMULATOR |
| Parziali | formato immagine a un livello, FORMAT durante lo SKETCH, lista delle relazioni di TABLE |
| Mancanti | Dual Screen G+G/GtoT, Graph to Table, comandi grafici di Run-Matrix/programmi, TABLE r=/parametriche/LIST/editing/LISTMEM, Variable, suffissi angolari |

**La parità completa con la fx-CG50 non è raggiunta.**

## 4. Test

| Verifica | Risultato |
|---|---|
| `tests/run_host_tests.sh` | 22 suite, 6540 controlli, 0 errori |
| `tests/run_host_tests.sh --sanitize` (ASan + UBSan) | 22 suite superate |
| Nuove suite | `graph_engine_test` 351, `graph_solve_test` 51, `graph_file_test` 345, `graph_app_test` 249, `sd_fatfs_test` 84 controlli |
| Relay (`python -m unittest discover -s tests`) | 15 test superati |
| `scripts/build_pico2w.sh` | UF2 2 874 880 byte, nessun warning nei sorgenti del progetto; `picotool info`: 1.8.0, RP2350, `pico2_w`, SDK 2.2.0 |
| Memoria del firmware | flash ≈1403 KB / 4096 KB; RAM statica ≈286 KB; heap ≈194 KB; stack 32 KB |
| Stack (`-fstack-usage`) | frame massimo di GRAPH 1712 byte (`openProject`), riferimenti ≈230 byte per livello (≤ 5) |
| Screenshot del simulatore | 06, 07, 20–34 (`tests/sim/graph.txt`), valori controllati: trace di x³ − 4x in 1,5 (−2,625; dY/dX 2,75), MAX (−2/√3; 16/(3√3)), ∫₋₂⁰ = 4, ∫₀² = −4, tangente in x = 1 (y = −x − 2), Modify A = 1,5 |
| Hardware | **non eseguito**: nessuna voce è TESTED ON HARDWARE |

Difetti trovati e corretti durante la sessione (con test di regressione):
profondità dei riferimenti che non contava gli argomenti; ricostruzione dei
grafici quando un'altra modalità cambia i dati; riempimenti che coprivano gli
assi; area ∫dx che restava dopo un nuovo G-Solve; marcatori fuori dall'area del
grafico; frame di stack di 3,4 KB per il progetto (ora nell'heap); punti delle
curve senza limite globale (ora 24 000); V-Window legacy che avrebbe fatto
scartare tutto il workspace v1/v2.

## 5. GitHub

| Commit | Contenuto |
|---|---|
| `04cef02` | GRAPH v1.8.0 (1/3): motore, G-Solve, progetti e nuova modalità GRAPH |
| `a626c44` | GRAPH v1.8.0 (2/3): test end-to-end e correzioni |
| `99d32a1` | GRAPH v1.8.0 (3/4): driver microSD (SPI + FatFs), screenshot, correzioni del disegno |
| ultimo | GRAPH v1.8.0 (4/4): budget di memoria, record legacy, documentazione e matrice di parità |

Ogni commit è pubblicato su `claude/loving-meitner-jifxhg` e `version/v1.8.0`
(push normale, nessun force-push) e verificato con `git ls-remote`. Nessuna
pull request, nessun tag, nessuna release; `main` e i branch delle versioni
precedenti non sono stati modificati. Nessun segreto o chiave nei file
aggiunti (controllo con ricerca di pattern di chiavi API).

## 6. Stato della versione

- **Completo** (sul PC): motore GRAPH, modalità GRAPH con tutte le funzioni
  della matrice marcate TESTED, progetti e immagini, driver SD e FatFs con
  scheda simulata, TABLE di base.
- **Parziale**: TABLE, formato delle immagini, FORMAT durante lo SKETCH.
- **Mancante**: Dual Screen, Graph to Table, comandi grafici dei programmi,
  funzioni di TABLE elencate sopra.
- **Host vs hardware**: tutto è provato solo su PC (test, sanitizer,
  simulatore) e con la compilazione del firmware. Display, tastiera, tempi di
  disegno e microSD reale restano da collaudare sulla PicoCalc.

La v1.8.0 non è stata rilasciata: il branch `version/v1.8.0` resta in
sviluppo fino all'approvazione.

## 7. Prossima sessione → v1.9.0

Proposta in [ROADMAP.md](../ROADMAP.md#2-prossima-sessione--v190-proposta):
1) collaudo hardware di GRAPH e microSD; 2) TABLE completa; 3) Dual Screen e
Graph to Table; 4) Dyna Graph, Recursion, Conic Graphs sul servizio GraphCore;
5) FORMAT nello SKETCH e suffissi angolari; 6) Punto 3 (Natural V.P.A.M. ↔
formato lineare).
