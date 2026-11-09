# Test — v1.8.0 (collaudo software)

## Cosa viene provato e dove

| Livello | Strumento | Stato |
|---|---|---|
| Logica portabile (motore, editor, app, grafici, tabelle, file, storage, protocollo AI) | test host C++ (CTest) | eseguito: 24 suite, 7169 controlli |
| Architettura (registro ↔ sorgenti ↔ firmware) | `architecture_test` | eseguito |
| Stessi test con AddressSanitizer + UBSan | `--sanitize` | eseguito |
| Interfaccia | simulatore host + screenshot PNG | eseguito (`docs/screenshots/`: Run-Matrix 12–19, GRAPH 06, 07, 20–34) |
| microSD (protocollo SD in SPI + FatFs) | scheda SD simulata nei test host | eseguito (`sd_fatfs_test`); bus SPI0 reale non provato |
| Interoperabilità FAT/exFAT | `sd_interop_test` (immagini con mkfs.fat/mkfs.exfat, fsck, mtools) | eseguito: 41 controlli |
| Collaudo fisico | procedura [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md) e SYSTEM → DIAG | **in attesa dei risultati dal dispositivo** |
| Stack sul target | `arm-none-eabi-g++ -fstack-usage` sui sorgenti del motore e di GRAPH | eseguito (vedi MATH_ENGINE.md e GRAPH_ENGINE.md) |
| Relay AI | `unittest` Python con SDK ufficiali e API simulate | eseguito: 15 test |
| Compilazione firmware RP2350 | Pico SDK 2.2.0 | eseguita, `picotool info` verificato |
| Hardware PicoCalc | — | **non eseguito** |

Nessuna funzione è dichiarata *TESTED ON HARDWARE*: i test host provano la logica
compilata dallo stesso sorgente del firmware, ma non display, tastiera I2C, flash
reale, microSD reale, Wi-Fi o TLS sul Pico.

## Comandi

```bash
tests/run_host_tests.sh              # build/host, tutte le suite
tests/run_host_tests.sh --sanitize   # build/host-asan con ASan + UBSan
ctest --test-dir build/host --output-on-failure -R expression   # una sola suite

cd relay && python -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python -m unittest discover -s tests -v
```

I test usano macro proprie (`tests/test_util.hpp`: `CHECK`, `CHECK_NEAR`,
`CHECK_THROWS_AS`, …) che restano attive anche con `NDEBUG` e riportano ogni
errore, non solo il primo.

## Suite host

| Suite | Controlli | Contenuto |
|---|---|---|
| expression_test | 154 | priorità Casio, funzioni, complessi, formati e arrotondamento come la calcolatrice, limiti di lunghezza/annidamento, budget di stack |
| exact_test | 152 | razionali (overflow, confronto esatto), radicali, π, letterali decimali e periodici, sviluppo periodico, limiti di visualizzazione CG50, valutatore esatto (trigonometria esatta, radici, log, n!, nCr, relazioni, Σ/Π, memoria esatta, catene lunghe, budget di stack), coerenza esatto/virgola mobile |
| run_matrix_test | 182 | esempi frazione ⇔ decimale, frazioni miste, periodo, input decimale, irrazionali mai convertiti, radicali e π (esempi del manuale), Ans/A–Z esatti, →A~F, variabili condivise, Int÷/Rmdr/MOD_Exp/logica, modifica della cronologia con ricalcolo, alberi 2D, persistenza e lettura dei record v1.6.0 (anche con V-Window non più valida), dati corrotti, flussi completi da tastiera attraverso il `Firmware` |
| architecture_test | 529 | registro coerente, controller registrati = modalità disponibili, schermate "non disponibile", include/sezioni del workspace/collegamenti dichiarati (anche GraphCore, Files, dato Graph), separazione di Run-Matrix, stratificazione motore/UI, nessun ciclo di include, modelli dati delle modalità future |
| editor_test | 179 | modelli 2D (anche frazione mista, periodo, radice con indice), navigazione, undo/redo, copia/incolla, layout e cursore, limiti |
| math_engine_test | 108 | matrici, vettori, basi, sessagesimale, frazioni, calcolo numerico |
| storage_test | 786 | serializzazione del workspace (formato v4 con lo stato di GRAPH e TABLE), slot A/B, CRC, interruzione di alimentazione in ogni punto della scrittura |
| app_test | 62 | AppManager, tasti globali, errori, barra di stato, TileDiff, rendering delle schermate |
| apps_flow_test | 56 | percorsi completi: Statistics → Graph, Graph ↔ Table, Equation, System, Memory, Conversion |
| solvers_test | 60 | polinomi (Aberth), disequazioni, solver numerico |
| statistics_test | 63 | 1/2-Var, regressioni, distribuzioni, test e intervalli (valori di riferimento) |
| ai_test | 138 | JSON, richiesta HTTP, parser della risposta (chunked, errori), app AI Chat con trasporto simulato |
| graph_engine_test | 351 | viewport e clipping, testi delle funzioni (intervallo, sovrapposizione), compilazione della lista, riferimenti tra funzioni (≤ 5 livelli), grafici da lista e incollati, campionamento (poli, salti, bordi di dominio, Plot), disequazioni, polari e parametriche, limiti di punti, ZOOM e preset della V-Window |
| graph_solve_test | 51 | Brent, radici (anche di tangenza), poli scartati, estremi, intersezioni, Y/X-CAL, integrali e aree contro valori analitici |
| graph_file_test | 345 | `.pcgraph` (round trip, CRC, versioni, ogni campo non valido), `PCGMEM`, `.pcpict` e BMP, file store su RAM con guasti iniettati, scrittura atomica e recupero |
| graph_app_test | 301 | GRAPH dalla tastiera: lista e editor, tipi e CONVERT, disegno incrementale e TRACE, ZOOM, V-Window e memorie, Setup, BUILT-IN, Draw Type, Simul Graph, budget di memoria, G-SOLVE completo (anche ∫dx attraverso un polo), SKETCH e limite PEN, Modify, memorie grafiche, PICT e sfondo, TABLE, incolla, progetti e ciclo 12.4 (anche sull'intera pila microSD simulata), scheda assente/piena/non formattata, record oltre la capacità, SYSTEM > DIAG |
| graph_verify_test | 182 | precisione al pixel di punti e segmenti (18 casi Y=, X=, r=, Param), discontinuità e domini, G-Solve, derivate e integrali contro valori analitici, poli, salti e singolarità integrabili, gradi/gradienti |
| table_test | 171 | TABLE: righe (intervalli positivi, negativi, misti, limiti), celle di 6 caratteri, valori contro formule chiuse, derivate, r= e Param, flussi da tastiera (SET, TABLE, aggiornamento, scorrimento, EDIT, ROW, LISTMEM, GPH-CON, DELETE, Variable List, Setup), 999 righe, persistenza v4 e v3, `.pcgraph` |
| sd_fatfs_test | 302 | protocollo SD su schede simulate SDHC/SDv2/SDv1, FatFs, nomi lunghi, salvataggio atomico, estrazione e reinserimento; scheda non formattata, spenta e riaccesa, errori di lettura, scritture rifiutate, dimensioni ai bordi di settore/cluster, cartelle annidate, 150 file, rinomina, scheda piena |
| sd_interop_test (script) | 41 | FAT16/FAT32/exFAT scritti dal codice del firmware, verificati con fsck.fat/fsck.exfat/mtools e viceversa |
| graphics_smoke | 57 | primitive, clipping, blending, conversione RGB666 |
| font_smoke | 2556 | copertura dei glifi usati, UTF-8, misure |
| animation_smoke | 6 | primitiva di animazione |
| cheatsheet_smoke | 365 | contenuti e navigazione di Symbols (17 pagine) |
| vpam_ast_smoke | 13 | albero condiviso editor/parser |
| **Totale** | **7169** (+ 41 dello script) | 0 errori, con e senza sanitizer (ASan + UBSan) |

`run_matrix_test` usa `tests/fixtures/workspace_v1_6_0.inc`, un record salvato
dai sorgenti della v1.6.0 (commit 7dc65cd), per verificare la compatibilità.

## Simulatore

```bash
build/host/picocalc_sim <cartella-output> <script|->
```

Comandi dello script: `type <testo>`, `key <NOME>` (EXE, EXIT, MENU, DEL, AC, STO,
F1–F10, SHIFT, OPTN, VARS, SETUP, frecce, HOME, END, PGUP, PGDN), `hold <NOME>`,
`ctrl <lettera>`, `wait <ms>`, `shot <nome>` (PNG 640×640, scala 2×).

Gli script degli screenshot sono `tests/sim/run_matrix.txt` (Run-Matrix 12–19)
e `tests/sim/graph.txt` (GRAPH e TABLE 06, 07, 20–37):

```bash
build/host/picocalc_sim /tmp/shots tests/sim/run_matrix.txt
build/host/picocalc_sim docs/screenshots tests/sim/graph.txt
```

Il simulatore usa una scheda microSD in RAM (16 MB) oppure, con
`PICOCALC_SD_IMAGE=<immagine>`, un'immagine FAT/exFAT reale; `wait <ms>` serve
anche a far scadere i messaggi temporanei (1,6 s) prima di uno screenshot.

```bash
truncate -s 64M sd.img && mkfs.fat -F 32 sd.img      # oppure: dd da una scheda reale
PICOCALC_SD_IMAGE=sd.img build/host/picocalc_sim /tmp/shots -
build/host/sd_image_tool sd.img ls PICOCALC/GRAPH
fsck.fat -n sd.img
```

## Verifiche del firmware senza hardware

- `picotool info -a build/pico2w/picocalc_fxai.uf2`: nome, versione, board, SDK.
- Simboli del linker: stack 32 KB (`__StackBottom` 0x20078000 – `__StackTop`
  0x20080000), fine dell'immagine flash prima dell'area dati.
- `-fstack-usage` sui sorgenti del motore per controllare i frame ricorsivi
  (vedi [MATH_ENGINE.md](MATH_ENGINE.md#limiti-protezione-dello-stack-da-32-kb-dellrp2350)).

## Collaudo su hardware (da fare)

Procedura in [HARDWARE.md](HARDWARE.md#collaudo-consigliato-sul-dispositivo).
Quando una voce sarà provata sulla PicoCalc, aggiornare
[FEATURE_MATRIX.md](FEATURE_MATRIX.md) a *TESTED ON HARDWARE* indicando data,
revisione della scheda e risultato.
