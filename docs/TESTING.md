# Test — v1.9.0 (consolidamento v1.8.0 + Pimoroni Pico Plus 2 W)

## Cosa viene provato e dove

| Livello | Strumento | Stato |
|---|---|---|
| Logica portabile (motore, editor, app, grafici, tabelle, 3D, file, storage, memoria, protocollo AI) | test host C++ (CTest), **per entrambi i profili di scheda** (`FXAI_TEST_BOARD=PICO_PLUS_2W` e `PICO2W`) | eseguito: 34 suite; 7 705 controlli (Plus) / 7 699 (Pico 2 W) più ≈ 333 000 verifiche dello stress dell'allocatore (`mem_pools_smoke`) |
| Architettura (registro ↔ sorgenti ↔ firmware) | `architecture_test` | eseguito |
| Stessi test con AddressSanitizer + UBSan | `--sanitize` | eseguito su entrambi i profili |
| Allocatore su due thread con ThreadSanitizer | `--tsan` (`mem_pools_tsan`, cartella di build propria) | eseguito su entrambi i profili |
| Memoria esaurita con `operator new` instradato nei pool | `oom_test` (SRAM simulata limitata, senza PSRAM) | eseguito |
| PSRAM | pool PSRAM simulato sul PC (arena TLSF; 8 MB nei test di sistema e nel simulatore), self-test con modelli di guasto | eseguito; il chip APS6404L reale non è provato |
| Flash interna | modello NOR (`storage::RamFlash`: erase a 0xFF, program che azzera bit, interruzione dopo n byte) per il record A/B e per LittleFS | eseguito; la flash reale non è provata |
| Interfaccia | simulatore host + screenshot PNG | eseguito (`docs/screenshots/`: Run-Matrix 12–19, GRAPH/TABLE 06, 07, 20–37, 3D Graph 40–43) |
| microSD (protocollo SD in SPI + FatFs) | scheda SD simulata nei test host | eseguito (`sd_fatfs_test`); bus SPI0 reale non provato |
| Interoperabilità FAT/exFAT | `sd_interop_test` (immagini con mkfs.fat/mkfs.exfat, fsck, mtools) | eseguito: 41 controlli |
| Relay AI | `unittest` Python con SDK ufficiali e API simulate, TLS in loopback | eseguito: 15 test |
| Compilazione firmware RP2350 | Pico SDK 2.2.0, `scripts/build.sh --board pico_plus_2w` e `--board pico2w` | eseguita, nessun warning, `picotool info` verificato ([BUILD_STATUS.md](BUILD_STATUS.md)) |
| Stack sul target | `-fstack-usage` su tutto il firmware | eseguito (frame massimo 1712 byte su 32 KB) |
| Collaudo fisico | procedure [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) e [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md), SYSTEM → DIAG | **non eseguito: in attesa dei risultati dal dispositivo** |

Quattro livelli distinti, mai confusi nella documentazione:
**test host** (logica compilata dallo stesso sorgente del firmware ed eseguita
sul PC), **test di compilazione firmware** (build, link, dimensioni, warning,
stack), **test simulati** (simulatore host, hardware modellato: PSRAM, flash
NOR, scheda SD) e **test hardware reali** (nessuno finora). Nessuna funzione è
dichiarata *TESTED ON HARDWARE*: display, tastiera I2C, PSRAM e flash reali,
microSD reale, Wi-Fi e TLS sul modulo non sono stati provati.

## Comandi

```bash
tests/run_host_tests.sh                     # entrambi i profili: build/host-pico_plus_2w, build/host-pico2w
tests/run_host_tests.sh --sanitize --tsan   # ASan + UBSan (build/host-<scheda>-asan) e ThreadSanitizer (-tsan)
tests/run_host_tests.sh --board PICO2W      # un solo profilo
ctest --test-dir build/host-pico_plus_2w --output-on-failure -R graph3d   # una sola suite

cd relay && python -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python -m unittest discover -s tests -v
```

I test usano macro proprie (`tests/test_util.hpp`: `CHECK`, `CHECK_NEAR`,
`CHECK_THROWS_AS`, …) che restano attive anche con `NDEBUG` e riportano ogni
errore, non solo il primo.

## Suite host

| Suite | Pico Plus 2 W | Pico 2 W | Contenuto |
|---|---|---|---|
| expression_test | 154 | 154 | priorità Casio, funzioni, complessi, formati e arrotondamento come la calcolatrice, limiti di lunghezza/annidamento, budget di stack |
| exact_test | 152 | 152 | razionali (overflow, confronto esatto), radicali, π, letterali decimali e periodici, sviluppo periodico, limiti di visualizzazione CG50, valutatore esatto (trigonometria esatta, radici, log, n!, nCr, relazioni, Σ/Π, memoria esatta, catene lunghe, budget di stack), coerenza esatto/virgola mobile |
| run_matrix_test | 182 | 182 | esempi frazione ⇔ decimale, frazioni miste, periodo, input decimale, irrazionali mai convertiti, radicali e π (esempi del manuale), Ans/A–Z esatti, →A~F, variabili condivise, Int÷/Rmdr/MOD_Exp/logica, modifica della cronologia con ricalcolo, alberi 2D, persistenza e lettura dei record v1.6.0 (anche con V-Window non più valida), dati corrotti, flussi completi da tastiera attraverso il `Firmware` |
| architecture_test (v1.8.0: 539) | 557 | 557 | registro coerente, controller registrati = modalità disponibili, schermate "non disponibile", include/sezioni del workspace/collegamenti dichiarati (anche GraphCore, Files, dato Graph), separazione di Run-Matrix, stratificazione motore/UI, nessun ciclo di include, modelli dati delle modalità future; 3D Graph e servizi Graph3D/Hardware |
| editor_test | 179 | 179 | modelli 2D (anche frazione mista, periodo, radice con indice), navigazione, undo/redo, copia/incolla, layout e cursore, limiti |
| math_engine_test (v1.8.0: 108) | 108 | 109 | matrici, vettori, basi, sessagesimale, frazioni, calcolo numerico; capacità della cronologia di sessione per scheda |
| storage_test (v1.8.0: 786) | 792 | 792 | serializzazione del workspace (formato v5 con lo stato di GRAPH, TABLE e 3D Graph; lettura dei formati precedenti), slot A/B, CRC, interruzione di alimentazione in ogni punto della scrittura |
| app_test | 62 | 62 | AppManager, tasti globali, errori, barra di stato, TileDiff, rendering delle schermate |
| apps_flow_test | 56 | 56 | percorsi completi: Statistics → Graph, Graph ↔ Table, Equation, System, Memory, Conversion |
| solvers_test | 60 | 60 | polinomi (Aberth), disequazioni, solver numerico |
| statistics_test | 63 | 63 | 1/2-Var, regressioni, distribuzioni, test e intervalli (valori di riferimento) |
| ai_test | 138 | 138 | JSON, richiesta HTTP, parser della risposta (chunked, errori), app AI Chat con trasporto simulato |
| graph_engine_test | 351 | 351 | viewport e clipping, testi delle funzioni (intervallo, sovrapposizione), compilazione della lista, riferimenti tra funzioni (≤ 5 livelli), grafici da lista e incollati, campionamento (poli, salti, bordi di dominio, Plot), disequazioni, polari e parametriche, limiti di punti, ZOOM e preset della V-Window |
| graph_solve_test | 51 | 51 | Brent, radici (anche di tangenza), poli scartati, estremi, intersezioni, Y/X-CAL, integrali e aree contro valori analitici |
| graph_file_test | 345 | 345 | `.pcgraph` (round trip, CRC, versioni, ogni campo non valido), `PCGMEM`, `.pcpict` e BMP, file store su RAM con guasti iniettati, scrittura atomica e recupero |
| graph_app_test (v1.8.0: 301) | 303 | 303 | GRAPH dalla tastiera: lista e editor, tipi e CONVERT, disegno incrementale e TRACE, ZOOM, V-Window e memorie, Setup, BUILT-IN, Draw Type, Simul Graph, budget di memoria, G-SOLVE completo (anche ∫dx attraverso un polo), SKETCH e limite PEN, Modify, memorie grafiche, PICT e sfondo, TABLE, incolla, progetti e ciclo 12.4 (anche sull'intera pila microSD simulata), scheda assente/piena/non formattata, record oltre la capacità, SYSTEM > DIAG; budget dei punti dal profilo RAM, record oltre la capacità anche con lo slot da 512 KB della Plus |
| graph_verify_test | 182 | 182 | precisione al pixel di punti e segmenti (18 casi Y=, X=, r=, Param), discontinuità e domini, G-Solve, derivate e integrali contro valori analitici, poli, salti e singolarità integrabili, gradi/gradienti |
| table_test | 171 | 171 | TABLE: righe (intervalli positivi, negativi, misti, limiti), celle di 6 caratteri, valori contro formule chiuse, derivate, r= e Param, flussi da tastiera (SET, TABLE, aggiornamento, scorrimento, EDIT, ROW, LISTMEM, GPH-CON, DELETE, Variable List, Setup), 999 righe, persistenza v4 e v3, `.pcgraph`; LISTMEM con le liste da 999 (Plus) o 255 elementi |
| sd_fatfs_test | 302 | 302 | protocollo SD su schede simulate SDHC/SDv2/SDv1, FatFs, nomi lunghi, salvataggio atomico, estrazione e reinserimento; scheda non formattata, spenta e riaccesa, errori di lettura, scritture rifiutate, dimensioni ai bordi di settore/cluster, cartelle annidate, 150 file, rinomina, scheda piena |
| sd_interop_test (script) | 41 | 41 | FAT16/FAT32/exFAT scritti dal codice del firmware, verificati con fsck.fat/fsck.exfat/mtools e viceversa |
| graphics_smoke | 57 | 57 | primitive, clipping, blending, conversione RGB666 |
| font_smoke | 2 556 | 2 556 | copertura dei glifi usati, UTF-8, misure |
| animation_smoke | 6 | 6 | primitiva di animazione |
| cheatsheet_smoke | 365 | 365 | contenuti e navigazione di Symbols (17 pagine) |
| vpam_ast_smoke | 13 | 13 | albero condiviso editor/parser |
| board_profile_test **nuova** | 78 | 74 | profilo della scheda e mappa della flash (partizioni, ultimo settore E10, posizione della v1.8.0), pin della PicoCalc invariati, capacità della flash dal JEDEC ID e regioni oltre il chip (UF2 della scheda sbagliata), limiti persistenti, record v5, migrazione di un record generato dai sorgenti della v1.8.0 (`tests/fixtures/workspace_v1_8_0.inc`) |
| limits_smoke **nuova** | 35 | 35 | limiti persistenti, RAM e strutturali per scheda; profilo scelto dai pool presenti; budget per i dati pesanti |
| mem_pools_smoke **nuova** | ≈ 333 000 | ≈ 334 000 | TLSF (base, allineamento, stress, esaurimento e frammentazione), `PlacementScope`, registro dei pool per Plus e Pico 2 W, stress su due thread (anche con ThreadSanitizer). Il numero di verifiche varia a ogni esecuzione (contesa tra i thread) |
| mem_new_smoke **nuova** | 13 | 13 | instradamento di `operator new` (soglia 512 byte, politiche, allineamenti) |
| psram_selftest_smoke **nuova** | 10 | 10 | self-test della PSRAM con modelli di guasto (bus dati, pattern, percorso con cache) |
| oom_test **nuova** | 18 | 18 | "Memory ERROR" con `operator new` instradato nei pool e SRAM limitata (condizioni del Pico 2 W): Run-Matrix, autosalvataggio rimandato e ripreso da solo, GRAPH, 3D (griglia ridotta o errore), editor; stato intatto e nessuna perdita di memoria |
| graphics_memory_test **nuova** | 23 | 23 | poligoni pieni oltre i 32 vertici, copie di layer e invalidate, regioni sporche per scheda, budget PICT, 20 parametriche dense disegnate per intero con la PSRAM, cache dei layer di GRAPH identica al pixel |
| internal_fs_test **nuova** | 197 | 197 | LittleFS su flash NOR simulata: contratto `files::FileSystem` come FatFs, partizione piena, interruzione di alimentazione durante `writeAtomic`, rimontaggio senza riformattare, progetti GRAPH nell'archivio interno |
| graph3d_test **nuova** | 111 | 111 | motore 3D (campionamento, poli e domini, proiezione, z-buffer e pittore, rotazione, colori per superficie) e modalità 3D Graph (lista, editor, V-Window, campionamento incrementale, rotazione/zoom senza ricalcolo, AUTO, persistenza, budget di memoria con e senza PSRAM) |
| system_diag_test **nuova** | 15 | 12 | SYSTEM → DIAG completa (MEM, PSR, ALC, FLS, LCD, SD, INT, GPH) con la memoria della scheda; Plus con PSRAM fallita; pagina INFO; "Archivio file"; riga del chip della flash; kernel di stress dell'allocatore |
| **Totale** | **7 705** + ≈ 333 000 dello stress + 41 dello script | **7 699** + ≈ 333 000 + 41 | 0 errori, con e senza sanitizer (ASan + UBSan); `mem_pools_tsan` con ThreadSanitizer |

`run_matrix_test` usa `tests/fixtures/workspace_v1_6_0.inc`, un record salvato
dai sorgenti della v1.6.0 (commit 7dc65cd), per verificare la compatibilità.
`board_profile_test` usa `tests/fixtures/workspace_v1_8_0.inc`, un record
generato con i sorgenti della v1.8.0, per la migrazione al formato v5.

Conteggi: la v1.8.0 rieseguita come riferimento dà 7 179 controlli (la sua
documentazione diceva 7169 perché il conteggio di `architecture_test` era fermo
a 529, il test ne esegue 539). Nessuna suite della v1.8.0 è stata rimossa o
ridotta. I test della prima 1.9.0 (`flash_fs_smoke`, `graphics_memory_smoke`,
`oom_smoke`, `system_app_smoke`) usavano l'API della base v1.5.4 e non erano
più compilati: i loro controlli sono in `internal_fs_test`,
`board_profile_test`, `graphics_memory_test`, `graph3d_test`, `oom_test`,
`expression_test` (limiti strutturali) e `system_diag_test`.

## Simulatore

```bash
build/host-pico_plus_2w/picocalc_sim <cartella-output> <script|->
```

Con il profilo Pico Plus 2 W il simulatore crea il pool PSRAM da 8 MB (come il
firmware dopo il self-test), quindi GRAPH, PICT e 3D usano i limiti della Plus;
con `build/host-pico2w` quelli del Pico 2 W.

Comandi dello script: `type <testo>`, `key <NOME>` (EXE, EXIT, MENU, DEL, AC, STO,
F1–F10, SHIFT, OPTN, VARS, SETUP, frecce, HOME, END, PGUP, PGDN), `hold <NOME>`,
`ctrl <lettera>`, `wait <ms>`, `shot <nome>` (PNG 640×640, scala 2×).

Gli script degli screenshot sono `tests/sim/run_matrix.txt` (Run-Matrix 12–19),
`tests/sim/graph.txt` (GRAPH e TABLE 06, 07, 20–37) e `tests/sim/graph3d.txt`
(3D Graph 40–43):

```bash
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix.txt
build/host-pico_plus_2w/picocalc_sim docs/screenshots tests/sim/graph.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph3d.txt
```

Il simulatore usa una scheda microSD in RAM (16 MB) oppure, con
`PICOCALC_SD_IMAGE=<immagine>`, un'immagine FAT/exFAT reale; `wait <ms>` serve
anche a far scadere i messaggi temporanei (1,6 s) prima di uno screenshot.

```bash
truncate -s 64M sd.img && mkfs.fat -F 32 sd.img      # oppure: dd da una scheda reale
PICOCALC_SD_IMAGE=sd.img build/host-pico_plus_2w/picocalc_sim /tmp/shots -
build/host-pico_plus_2w/sd_image_tool sd.img ls PICOCALC/GRAPH
fsck.fat -n sd.img
```

## Verifiche del firmware senza hardware

- `picotool info -a build/<scheda>/picocalc_fxai_<scheda>.uf2`: nome, versione
  (`1.9.0 (pico_plus_2w)` / `1.9.0 (pico2w)`), board, SDK.
- Simboli del linker: stack 32 KB (`__StackBottom` 0x20078000 – `__StackTop`
  0x20080000), fine dell'immagine flash prima dell'area dati.
- `-fstack-usage` su tutto il firmware (file `.su` nella cartella di build) per
  controllare i frame, anche ricorsivi
  (vedi [MATH_ENGINE.md](MATH_ENGINE.md#limiti-protezione-dello-stack-da-32-kb-dellrp2350)).
- Controllo della dimensione dell'immagine nella partizione firmware a ogni
  build (errore se la supera).

## Collaudo su hardware (da fare)

Procedure in [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) (HW-40…HW-82) e
[HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md) (HW-01…HW-36, da ripetere con
la 1.9.0); vedi anche [HARDWARE.md](HARDWARE.md#collaudo-consigliato-sul-dispositivo).
Quando una voce sarà provata sulla PicoCalc, aggiornare
[FEATURE_MATRIX.md](FEATURE_MATRIX.md) a *TESTED ON HARDWARE* indicando data,
revisione della scheda e risultato.
