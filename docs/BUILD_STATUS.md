# Build status — v1.19.0 (Geometry alla parità sulla Plus; Raspberry Pi Pico 2 W congelato)

Stato delle build per versione; le novità sono in [CHANGELOG.md](CHANGELOG.md).
Le sezioni dopo "v1.19.0" descrivono le build delle versioni precedenti.

## v1.19.0 — `main`

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`;
sul Pico 2 W `-Os` per i sorgenti di `FXAI_PICO2W_SIZE_SOURCES`), nello stesso
ambiente della v1.18.0 (Pico SDK 2.2.0, `gcc-arm-none-eabi` 13.2.1). Novità:
Geometry alla parità con il capitolo 14 sulla Plus
([reports/SVILUPPO_v1.19.0.md](reports/SVILUPPO_v1.19.0.md)). **Nessuna prova su hardware.**

> **Raspberry Pi Pico 2 W congelato alla v1.16.0**: 1 944 228 byte, confrontato
> byte per byte con la v1.18.0 compilata nello stesso ambiente: 4 byte diversi,
> tutti la cifra della stringa della versione; stesse modalità, stessa RAM statica.

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (5 967 872 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 889 152 byte) |
| Immagine flash | 2 983 560 byte (`__flash_binary_end` = `0x102D8688`) = 71 % della partizione da 4 MB (budget < 90 %: 3 774 873 B); +176 344 byte rispetto alla v1.18.0: i sette moduli di Geometry 206 919 B, meno la Geometry della v1.17.0 (29 228 B) | **1 944 228 byte** (`0x101DAAA4`) = 92 % della partizione da 2 MB; invariato |
| Codice nuovo nell'immagine | `geometry` (cinque file del nucleo), `geometry_app`, `geometry_view`; registro | nessuno (mappa: **185** oggetti/simboli esclusi, nessuno nell'immagine) |
| RAM statica | 312 736 byte fino a `__end__` = `0x2004C5A0` (+128) | 305 236 byte (`0x2004A854`, invariata) |
| Heap SRAM | 178 784 byte (`__end__` → `0x20078000`) | 186 284 byte (invariato) |
| Frame di stack (`-fstack-usage`) | massimo invariato `SystemApp::enter` 2 016 B; codice nuovo: `geo::transform` 784 B, `openSetup` 784 B, `geo::addSpec` 704 B, `GeometryApp::key` 584 B | invariati |
| Warning | **nessuno** del compilatore | nessuno |
| SHA-256 dell'UF2 locale | `7531c083d9a48b3e20c82570b9b7c066507c0f9fa84da4d82ec8c70cb7113ced` | `0548b1f031c4d2dada1e967d06ffbabe7fa7a75fdc5ba30deb0122e925eab53a` |

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 64/64: 61 suite C++ con **20 594** controlli, `mem_pools_smoke`, `sd_interop_test`, `link_tool_test` | 54/54: 52 suite con **16 378** controlli, `mem_pools_smoke`, `sd_interop_test` |
| AddressSanitizer + UBSan bloccante (`--sanitize`) | 64/64 | 54/54 |
| ThreadSanitizer (`--tsan`) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| Fuzzing (≥ 300 s per harness nuovo o toccato) | nuovo: `fuzz_geometry` 82 133 esecuzioni; toccati: `fuzz_modes` 4 449 143, `fuzz_workspace` 1 670 584, `fuzz_ui` 10 634, `fuzz_eactivity` 1 108 con il controller e 6 249 384 sui soli documenti: nessun crash sul codice finale (due errori trovati da `fuzz_geometry` durante lo sviluppo e corretti) | — |
| Simulatore (`tools/sim_compare.py`) | 192 identici: 162 precedenti invariati, 2 rigenerati per cambiamenti voluti (144, 172), 28 nuovi (175–202) | — |

## v1.18.0

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`;
sul Pico 2 W `-Os` per i sorgenti di `FXAI_PICO2W_SIZE_SOURCES`), nello stesso
ambiente della v1.17.0 (Pico SDK 2.2.0, `gcc-arm-none-eabi` 13.2.1). Novità:
Statistics alla parità con il capitolo 6 e Distribution Base sulla Plus
([reports/SVILUPPO_v1.18.0.md](reports/SVILUPPO_v1.18.0.md)). **Nessuna prova su hardware.**

> **Raspberry Pi Pico 2 W congelato alla v1.16.0**: 1 944 228 byte, identico
> alla v1.17.0 salvo la stringa della versione e la data di compilazione
> (confronto byte per byte); stesse modalità, stessa RAM statica.

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (5 615 104 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 889 152 byte) |
| Immagine flash | 2 807 216 byte (`__flash_binary_end` = `0x102AD5B0`) = 66 % della partizione da 4 MB (budget < 90 %: 3 774 873 B); +181 744 byte rispetto alla v1.17.0: controller di Statistics 108 340 B, nucleo 77 662 B, distribuzioni 15 316 B, Distribution 25 345 B, meno il controller della v1.16.0 | **1 944 228 byte** (`0x101DAAA4`) = 92 % della partizione da 2 MB; invariato |
| Codice nuovo nell'immagine | `stat_engine`, `dist_core`, `statistics_plus_app`, `stat_plot`, `distribution`, `distribution_app`; blocchi `#if` in expression, calc_library, run_matrix_app, workspace, launcher, widgets, font_renderer | nessuno (mappa: **179** oggetti/simboli esclusi, nessuno nell'immagine) |
| RAM statica | 312 608 byte fino a `__end__` = `0x2004C520` (+1 896) | 305 236 byte (`0x2004A854`, invariata) |
| Heap SRAM | 178 912 byte (`__end__` → `0x20078000`) | 186 284 byte (invariato) |
| Frame di stack (`-fstack-usage`) | massimo invariato `SystemApp::enter` 2 016 B; codice nuovo: `stats::decode` 1 792 B, `stats::reset` 1 640 B, `drawRegression` 976 B, `StatisticsApp::key` 880 B | invariati |
| Warning | **nessuno** del compilatore | nessuno |
| SHA-256 dell'UF2 locale | `2a9990e3063659878abc8581be688c63b1660b1fa92246bb018221de97433f88` | `c56b8f66e58bc087b0563241e1476948ba0b4de27ff245be302a72c87a98a443` |

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 62/62: 59 suite C++ con **20 063** controlli, `mem_pools_smoke`, `sd_interop_test`, `link_tool_test` | 54/54: 52 suite con **16 343** controlli, `mem_pools_smoke`, `sd_interop_test` |
| AddressSanitizer + UBSan bloccante (`--sanitize`) | 62/62 | 54/54 |
| ThreadSanitizer (`--tsan`) | superato | superato |
| Riferimenti numerici (`tools/stat_reference.py --check`) | 483 valori di scipy/numpy/statsmodels identici | — |
| Relay AI (Python, `unittest`) | 15/15 | — |
| Fuzzing (≥ 300 s per harness nuovo o toccato) | nuovo: `fuzz_stats` 1 378 441 esecuzioni; toccati: `fuzz_modes` 1 479 362, `fuzz_ui` 16 122, `fuzz_workspace` 2 262 380, `fuzz_expr` 680 923, `fuzz_eactivity` 979 con il controller e 9 130 371 sui soli documenti: nessun crash sul codice finale (tre errori trovati durante lo sviluppo e corretti) | — |
| Simulatore (`tools/sim_compare.py`) | 164 identici: 134 precedenti invariati, 3 rigenerati per cambiamenti voluti (52, 53, 84), 27 nuovi (148–174) | — |

## v1.17.0

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`;
sul Pico 2 W `-Os` per i sorgenti di `FXAI_PICO2W_SIZE_SOURCES`), nello stesso
ambiente della v1.16.0 (Pico SDK 2.2.0, `gcc-arm-none-eabi` 13.2.1). Novità:
stato Base e dieci modalità Base sulla Plus, record v12, MicroPython
([reports/SVILUPPO_v1.17.0.md](reports/SVILUPPO_v1.17.0.md)). `picotool info
-a`: «PicoCalc FX-CG50 AI», versione `1.17.0 (pico_plus_2w)` / `1.17.0
(pico2w)`, SDK 2.2.0. **Nessuna prova su hardware.**

> **Raspberry Pi Pico 2 W congelato alla v1.16.0**: 1 944 228 byte (+16 B, la
> sola correzione condivisa di Conversion e la stringa di versione); stesse
> modalità, stessa RAM statica, budget 2 064 384 B.

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (5 251 584 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 889 152 byte) |
| Immagine flash | 2 625 472 byte (`__flash_binary_end` = `0x10280FC0`) = 62 % della partizione da 4 MB (budget < 90 %: 3 774 873 B); +527 924 byte rispetto alla v1.16.0: le dieci modalità Base (398 884 B, di cui Python e MicroPython 135 631 B) e l'interprete F9 ora collegato (105 270 B) | **1 944 228 byte** (`0x101DAAA4`) = 92 % della partizione da 2 MB; +16 byte |
| Codice nuovo nell'immagine | nuclei e controller di Spreadsheet, Dyna Graph, Recursion, Conic Graphs, Financial, Program, Link, Geometry, Picture Plot, Python; MicroPython v1.24.1; record v12; `TypedFileMenu`; porta USB seriale | nessuno (mappa: **169** oggetti/simboli esclusi, nessuno nell'immagine) |
| RAM statica | 310 712 byte fino a `__end__` = `0x2004BDB8` (+1 548); 312 760 byte con la sezione `.heap` minima | 305 236 byte (`0x2004A854`, invariata); 307 284 byte con `.heap` |
| Heap SRAM | 180 808 byte (`__end__` → `0x20078000`) | 186 284 byte (invariato) |
| PSRAM | 8 MB come pool TLSF separato; Python vi prende 512 KB per l'heap di MicroPython solo mentre la modalità è aperta | — |
| Frame di stack (`-fstack-usage`) | massimo invariato `SystemApp::enter` 2 016 B; codice nuovo: `fillPage` di Financial 976 B, `DynaGraphApp::key` 808 B, `fillSet` di Recursion 664 B; MicroPython sulla pila della coroutine (24 KB): `pc_input` 264 B, `mp_execute_bytecode` 128 B per livello | invariati |
| Warning | **nessuno** del compilatore | nessuno |
| SHA-256 dell'UF2 locale | `b06018ccac8eaffbdbe79605c754748e36d27441b3626f784491009ee2df06ca` | `3047191f8b481ad4578a26a955e0c90ce5a5dbd5f2d3b45ef324a13dc720de15` |

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 61/61: 58 suite C++ con **18 623** controlli, `mem_pools_smoke`, `sd_interop_test`, `link_tool_test` | 54/54: 52 suite con **16 297** controlli, `mem_pools_smoke`, `sd_interop_test` |
| AddressSanitizer + UBSan bloccante (`--sanitize`) | 61/61 | 54/54 |
| ThreadSanitizer (`--tsan`) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| Fuzzing (≥ 300 s per harness nuovo o toccato) | nuovi: `fuzz_sheet` 551 536 esecuzioni, `fuzz_link` 3 543 604, `fuzz_modes` 2 457 080, `fuzz_python` 5 615 993; toccati: `fuzz_workspace` 2 370 949, `fuzz_ui` 16 191 (sul codice finale dei controller), `fuzz_typedfile` 118 272 391, `fuzz_eactivity` 3 024 con il controller e 13 830 763 sui soli documenti; ognuno ≥ 300 s: nessun crash né artefatto sul codice finale (durante lo sviluppo `fuzz_modes` aveva trovato i NaN in Financial) | — |
| Simulatore (`tools/sim_compare.py`) | 137 identici: 113 precedenti invariati, 4 rigenerati per cambiamenti voluti (19, 105, 106, 115), 20 nuovi (128–147) | — |

## v1.16.0

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`;
sul Pico 2 W `-Os` per i sorgenti di `FXAI_PICO2W_SIZE_SOURCES`), nello stesso
ambiente della v1.15.0 (Pico SDK 2.2.0, `gcc-arm-none-eabi` 13.2.1). Novità:
eActivity su entrambe le schede
([reports/SVILUPPO_v1.16.0.md](reports/SVILUPPO_v1.16.0.md)). **Nessuna prova
su hardware.**

> **Dimensione finale del Raspberry Pi Pico 2 W (congelato alla v1.16.0):
> 1 944 212 byte** (budget 2 064 384 B; restano 120 172 B solo per correzioni
> di errori condivise).

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (4 195 840 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 889 152 byte) |
| Immagine flash | 2 097 548 byte (`__flash_binary_end` = `0x1020018C`) = 50 % della partizione da 4 MB (budget < 90 %: 3 774 873 B); +118 708 byte rispetto alla v1.15.0 (eActivity, parti del workspace, servizi ora collegati: editor di testo, guscio) | **1 944 212 byte** (`0x101DAA94`) = 92 % della partizione da 2 MB; **−25 596 byte** rispetto alla v1.15.0 nonostante eActivity, grazie a `-Os` su eActivity, record del workspace, file, impostazioni e relay AI |
| Codice nuovo nell'immagine | `eactivity.cpp` + `eactivity_app.cpp`, codifica per parti, sezione `0x0102`, sessione da striscia, editor di testo (memo), immagini nelle righe | gli stessi; `graph2d`, `binding`, `program_*` **non compilati** (mappa: 18 oggetti/simboli esclusi, nessuno nell'immagine) |
| RAM statica | 309 164 byte fino a `__end__` = `0x2004B7AC` (+100); 311 212 byte con la sezione `.heap` minima | 305 236 byte (`0x2004A854`, +96); 307 284 byte con `.heap` |
| Heap SRAM | 182 356 byte (`__end__` → `0x20078000`) | 186 284 byte |
| PSRAM | 8 MB come pool TLSF separato | — |
| Frame di stack (`-fstack-usage`) | massimo invariato `SystemApp::enter` 2 016 B; codice nuovo: `decodePart` 1 080 B, `EActivityApp::docAction` 360 B, `eact::encode` 320 B, `execute` 288 B; nessuna ricorsione nuova | massimo `SystemApp::enter` 1 968 B (`-Os`), `decodePart` 1 048 B, `docAction` 360 B |
| Warning | **nessuno** del compilatore | nessuno |
| SHA-256 dell'UF2 locale | `4e3602db9adbab14b2126f232ddcf9d429d9ea41dd6a15b4972f53156b3384b9` | `f41d66441988a55cdad3938702a5adeac7d83ba4fcf8931796d2a76a6c8785a9` |

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 56/56: 54 suite C++ con **14 658** controlli, `mem_pools_smoke` (333 435), `sd_interop_test` | 53/53: 51 suite con **14 563** controlli, `mem_pools_smoke` (333 465), `sd_interop_test` |
| AddressSanitizer + UBSan bloccante (`--sanitize`) | 56/56 | 53/53 |
| ThreadSanitizer (`--tsan`) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| Fuzzing (≥ 300 s per harness nuovo o toccato) | `fuzz_eactivity` 988 esecuzioni con il controller (firmware riavviato a ogni input) e 8 716 498 sui soli documenti (`FXAI_FUZZ_EACT_DOCS=1`), `fuzz_ui` 10 826, `fuzz_workspace` 2 208 879, ognuno ≥ 300 s: nessun crash né artefatto sul codice finale (prima della correzione la modalità documenti aveva trovato le righe con un a capo) | — |
| Simulatore (`tools/sim_compare.py`) | 97 schermate identiche agli screenshot pubblicati, 20 nuove (108–127) | — |

## v1.15.0

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`),
una cartella per scheda, nello stesso ambiente della v1.14.0 (Pico SDK 2.2.0,
`gcc-arm-none-eabi` 13.2.1). Novità: fondamenta comuni F1–F9
([reports/SVILUPPO_v1.15.0.md](reports/SVILUPPO_v1.15.0.md)); dalla v1.15.0 il
controllo dopo il link è **bloccante** (partizione, margine della scheda,
oggetti e simboli delle modalità/servizi esclusi nella mappa del linker).
**Nessuna prova su hardware.**

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (3 958 272 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 940 352 byte) |
| Immagine flash | 1 978 840 byte (`__flash_binary_end` = `0x101E31D8`) = 47 % della partizione da 4 MB (budget < 90 %: 3 774 873 B); +30 816 byte rispetto alla v1.14.0 | **1 969 808 byte** (`0x101E0E90`) = 93 % della partizione da 2 MB; **budget ≤ 2 064 384 B** (margine di 32 KiB): restano 94 576 B per la v1.16.0; +30 628 byte |
| Codice nuovo nell'immagine | registro risolto, record v11, file tipizzati, `FileDialog` unico e MEMORY › FILES, guscio (TABLE), runner (3D), celle e nomi legati nel lexer; `graph2d`, `binding` e `program_*` compilati ma scartati dal linker finché nessuna modalità li usa | gli stessi; `graph2d`, `binding`, `program_*` **non compilati** (mappa: 18 oggetti/simboli esclusi, nessuno nell'immagine) |
| RAM statica | 309 064 byte fino a `__end__` = `0x2004B748` (+40); 311 112 byte contando la sezione `.heap` minima (valore del controllo dopo il link) | 305 140 byte (`0x2004A7F4`, +40); 307 188 byte con `.heap` |
| Heap SRAM | 182 456 byte (`__end__` → `0x20078000`) | 186 380 byte |
| PSRAM | 8 MB come pool TLSF separato | — |
| Frame di stack (`-fstack-usage`) | massimo invariato `SystemApp::enter` 2 016 B; codice nuovo: analizzatore dei programmi ≤ 304 B per livello con al più 48 livelli (≈ 19 KB nel caso peggiore, poi «Stack ERROR»), macchina non ricorsiva (`printf` 800 B, `step` 448 B) | come la Plus (senza l'interprete) |
| Warning | **nessuno** del compilatore | nessuno |
| SHA-256 dell'UF2 locale | `8a275975a47d97e0fd4a43f6b8b6e3275e364f5b365a5f4871d9e771a18c84f0` | `d99d412effadb5251cb3ebccf3c66378fdc7ad0fbc717eb7dd8848e6081023a2` |

Controlli del build provati sul PC (HW-200/HW-201): un margine di 200 000 B sul
Pico 2 W fa fallire il build («leaves less than 200000 B…»); una mappa con
`src/graph2d.cpp.o` fa fallire il controllo («excluded mode/service found in
the image»); `-DFXAI_SERVICE_GRAPH2D=ON` sul Pico 2 W è rifiutato dalla
configurazione.

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 55/55: 53 suite C++ con **14 304** controlli, `mem_pools_smoke`, `sd_interop_test` | 52/52: 50 suite con **14 209** controlli |
| AddressSanitizer + UBSan bloccante (`--sanitize`) | 55/55 | 52/52 |
| ThreadSanitizer (`--tsan`) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| Fuzzing (`tools/fuzz`, ≥ 300 s per harness nuovo o toccato) | `fuzz_workspace` 2 284 459, `fuzz_expr` 683 978, `fuzz_ui` 11 912, `fuzz_typedfile` 62 249 374, `fuzz_texteditor` 2 555 841, `fuzz_program` 2 532 316 esecuzioni: nessun crash | — |
| Simulatore (`tools/sim_compare.py`) | 94 schermate identiche agli screenshot pubblicati, 3 nuove (105–107) | — |

## v1.14.0

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`),
una cartella per scheda, nello stesso ambiente delle versioni precedenti (Pico
SDK 2.2.0, `gcc-arm-none-eabi` 13.2.1). Novità: 3D Graph completo
([reports/SVILUPPO_v1.14.0.md](reports/SVILUPPO_v1.14.0.md)). **Nessuna prova su hardware.**

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (3 896 832 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 878 912 byte) |
| Immagine flash | 1 948 024 byte (`__flash_binary_end` = `0x101DB978`) = 46 % della partizione firmware da 4 MB; +99 256 byte rispetto alla v1.13.1 (modello e disegno del 3D, controller, schermate) | 1 939 180 byte (`0x101D96EC`) = 92 % della partizione da 2 MB; +99 240 byte |
| RAM statica | 309 024 byte fino a `__end__` = `0x2004B720` (+1 728 byte: stato del controller del 3D) | 305 100 byte (`0x2004A7CC`, +1 728 byte) |
| Heap SRAM | 182 496 byte (`__end__` → `0x20078000`) | 186 420 byte |
| PSRAM | 8 MB come pool TLSF separato: anche reticoli, z-buffer (180 KB) e immagine della scena (180 KB) del 3D Graph | — |
| Frame di stack (`-fstack-usage`) | massimo invariato `SystemApp::enter` 2 016 B; codice nuovo: `g3d::render` 1 800 B (poligoni tagliati di al più 12 punti), `Graph3DApp::loadWindowForm` 1 136 B, `drawList` 856 B, `drawPlot` 736 B; `readLegacyGraph3D` e `Workspace::resetAll` riportati sotto i 1 712 B azzerando lo stato 3D sul posto | come la Plus |
| Warning | **nessuno** del compilatore (resta l'avviso di CMake dell'SDK sulla ricerca di picotool) | nessuno |
| SHA-256 dell'UF2 locale | `68355c8be8e3e0969275a5d1598d61f7dafbbac2701c57963ddaebf68cc2efe2` | `ab2f3be0d44e7b8ef74f466159cbbbb133e58632fcba0ce7c524f442bc05269a` |

Gli UF2 allegati alla GitHub Release sono compilati dal workflow sul commit
pubblicato e verificati con `sha256sum -c SHA256SUMS`; i loro hash possono
differire da quelli locali.

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 47/47: 45 suite C++ con **13 111** controlli (+541 di `graph3d_v114_test`), `mem_pools_smoke` (≈ 333 000 verifiche) e `sd_interop_test` (41) | 47/47: **13 105** controlli + ≈ 333 000 + 41 |
| AddressSanitizer + UBSan bloccante (`--sanitize`) | 47/47 | 47/47 |
| ThreadSanitizer (`--tsan`) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| Fuzzing (`tools/fuzz`) | `fuzz_graph3d` ≈ 870 000 esecuzioni (un errore trovato e corretto: NaN convertito in `int` con coordinate enormi), `fuzz_workspace` 7,1 milioni con i record v10: nessun crash sul codice finale | — |
| Simulatore | `tests/sim/graph3d.txt` e `tests/sim/graph3d_v114.txt`: 21 schermate identiche agli screenshot pubblicati (40–43, 88–104) | — |

## v1.13.1

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`),
una cartella per scheda, nello stesso ambiente della v1.13.0 (Pico SDK 2.2.0,
`gcc-arm-none-eabi` 13.2.1). Solo correzioni ([reports/ERRORI_v1.13.1.md](reports/ERRORI_v1.13.1.md)).
**Nessuna prova su hardware.**

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (3 698 176 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 680 768 byte) |
| Immagine flash | 1 848 768 byte (`__flash_binary_end` = `0x101C35C0`) = 44 % della partizione firmware da 4 MB; +7 160 byte rispetto alla v1.13.0 (controlli e calcoli corretti) | 1 839 940 byte (`0x101C1344`) = 88 % della partizione da 2 MB; +7 168 byte |
| RAM statica | 307 296 byte fino a `__end__` = `0x2004B060` (invariata) | 303 372 byte (`0x2004A10C`, invariata) |
| Frame di stack (`-fstack-usage`) | massimo `SystemApp::enter` 2 016 B; valutatore ricorsivo ≤ 328 B per livello (budget 16 KB, profondità 32); unico frame dinamico in `std::to_string` (limitato) | come la Plus |
| Warning | **nessuno** del compilatore (resta l'avviso di CMake dell'SDK sulla ricerca di picotool) | nessuno |
| SHA-256 dell'UF2 locale | `c9ab78b2044e294924252e87e421b7099e91b6893223e695674fb0373089d7e1` | `33e4dada2cb9299c4ffa761d37a1a9211748a45fae3179351d9ac37fa1ed3853` |

Gli UF2 allegati alla GitHub Release sono compilati dal workflow sul commit
pubblicato e verificati con `sha256sum -c SHA256SUMS`; i loro hash possono
differire da quelli locali.

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 46/46: 44 suite C++ con **12 223** controlli (+142 di `fixes_v1131_test`), `mem_pools_smoke` (≈ 333 000 verifiche) e `sd_interop_test` (41) | 46/46: **12 217** controlli + ≈ 333 000 + 41 |
| AddressSanitizer + UBSan bloccante (`--sanitize`) | 46/46 | 46/46 |
| ThreadSanitizer (`--tsan`) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| Analisi statica | cppcheck, GCC con warning extra, clang-tidy: nessun errore reale (dettaglio in [reports/ERRORI_v1.13.1.md](reports/ERRORI_v1.13.1.md#verifiche-senza-errori)) | stesso codice |
| Fuzzing (`tools/fuzz`) | parser, workspace, CSV, `.pcgraph`, EQUATION, tasti: nessun crash dopo le correzioni; un timeout del SOLVER di EQUATION con SolveN nell'equazione (A6, aperto) | — |
| Simulatore | 7 script, 77 schermate identiche agli screenshot pubblicati (script aggiornati all'ordine di OPTN; 17, 18, 51, 55, 60 rigenerati) | — |

## v1.13.0

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`)
sul codice unito con la v1.12.0 (EQUATION), una cartella per scheda, nello
stesso ambiente delle versioni precedenti (Pico SDK 2.2.0,
`gcc-arm-none-eabi` 13.2.1). **Nessuna prova su hardware.**

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (3 683 840 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 666 432 byte) |
| Immagine flash | 1 841 608 byte (`__flash_binary_end` = `0x101C19C8`) = 44 % della partizione firmware da 4 MB; +161 920 byte rispetto alla v1.12.0 (libreria CALC/STAT/TVM/CONVERT, menu, CATALOG, comandi grafici) | 1 832 772 byte (`0x101BF744`) = 87 % della partizione da 2 MB; +161 856 byte |
| Resto della flash | invariato (diagnostica, LittleFS ≈ 11 MB anche per CSV, Pict e Capture, BTstack, workspace 2 × 512 KB) | invariato (LittleFS ≈ 1,9 MB, workspace 128 KB) |
| RAM statica | 307 296 byte fino a `__end__` = `0x2004B060` (+5 048 byte rispetto alla v1.12.0: glifi nuovi, tabelle dei nomi e del CATALOG, stato dei menu) | 303 372 byte (`0x2004A10C`, +5 048 byte) |
| Heap SRAM | 184 224 byte (`__end__` → `0x20078000`) | 188 148 byte |
| PSRAM | 8 MB come pool TLSF separato (liste di tutti i List File, matrici con le forme esatte, catture, coefficienti di EQUATION) | — |
| Stack | core 0: 32 KB con guardia MSPLIM; core 1: 4 KB in SCRATCH_X (invariati) | uguale |
| Frame di stack (`-fstack-usage`) | massimo `SystemApp::enter` 2 016 B (due voci nuove del SET UP; non è sul percorso di una ricorsione), poi `diag::testMemory` e `GraphApp::openProject` 1 712 B; codice nuovo: `evaluateStatement` 800 B, `evaluateEntry` 488 B, `RunMatrixApp::runAction` 480 B, comandi grafici al massimo 400 B, `CollEval::solveN` 392 B, `Workspace::special` 176 B (il motore dei grafici di DrawFTG e TABLE Result è sullo heap); la ricorsione del valutatore resta sotto il budget di 16 KB | uguale |
| Warning | **nessuno** del compilatore (0 righe `warning:`; resta l'avviso di CMake dell'SDK sulla ricerca di picotool, come nelle versioni precedenti) | nessuno |
| SHA-256 dell'UF2 locale | `f4eb56b52055cb3ee5ee605227ce31dfba3455991296bb1f8a5d0459b3eae61e` | `82b40c2acfe67eca7a64e1fa2455f18d486cdea61b582dc90f9dbeeb217b0deb` |

Gli UF2 allegati alla GitHub Release sono compilati dal workflow
`.github/workflows/release.yml` sul commit pubblicato e verificati con
`sha256sum -c SHA256SUMS`; i loro hash possono differire da quelli locali.

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 45/45: 43 suite C++ con **12 081** controlli, `mem_pools_smoke` (≈ 333 000 verifiche) e `sd_interop_test` (41) | 45/45: **12 075** controlli + ≈ 334 000 + 41 |
| AddressSanitizer + UBSan (`--sanitize`) | 45/45 | 45/45 |
| ThreadSanitizer (`--tsan`, allocatore su due thread) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| Simulatore | screenshot 77–87 nuovi (`tests/sim/run_matrix_v112.txt`), compressi senza perdita; lo script di EQUATION della v1.12.0 (`tests/sim/equation.txt`, adattato al VARS della v1.13.0) ridà gli screenshot 08 e 62–76 identici pixel per pixel | |

Suite della v1.13.0: `calc_v112_test` (207 controlli), `run_matrix_v112_test`
(152, con le migrazioni v7→v9 e v8→v9), `graph_cmd_v112_test` (56),
`editor_v112_test` (488); `architecture_test` 1 320. Il dispositivo non è
stato usato.

## v1.12.0 — EQUATION (tag `v1.12.0`, commit `51691de`)

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`),
una cartella per scheda, stesso ambiente della v1.11.0 (Pico SDK 2.2.0,
`gcc-arm-none-eabi` 13.2.1). **Nessuna prova su hardware.**

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (3 360 256 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 342 848 byte) |
| Versione nel binario | `1.12.0 (pico_plus_2w)` (`picotool` non installato in questo ambiente; lo verifica il workflow) | `1.12.0 (pico2w)` |
| Immagine flash | 1 679 688 byte (`__flash_binary_end` = `0x1019A148`) = 40 % della partizione da 4 MB; +84 732 byte rispetto alla v1.11.0 | 1 670 916 byte (`0x10197F04`) = 79 % della partizione da 2 MB; +84 748 byte |
| Resto della flash | invariato | invariato |
| RAM statica | 302 248 byte (−308: la vecchia griglia di EQUATION non c'è più) | 298 324 byte (−308) |
| Heap SRAM | 191 320 byte (`__end__` = `0x200494A8` → `0x20078000`) | 195 244 byte |
| PSRAM | 8 MB come pool TLSF separato (anche i coefficienti di EQUATION, ≈ 7,6 KB) | — |
| Frame di stack (`-fstack-usage`) | massimo invariato `SystemApp::enter` 1 840 B; codice nuovo: forma esatta dei complessi 784 B (tabelle sull'heap), `solveSimultaneous` 776 B, `solvePolynomial` 760 B, `numericPolynomial` 712 B, `EquationApp::solve` 552 B; nessuna ricorsione nuova | uguale |
| Warning | **nessuno** (0 righe `warning:`) | nessuno |
| SHA-256 dell'UF2 locale | `6f28c35d3744fc1d6b61a3634bf2308c5e0cf9bc055043b1a208bae8a48d1125` | `67fee0302167ee2d1fe98bfcd0fc661cc2d371966d5fa8ff4d4bd6fb608a1590` |

Gli UF2 della release sono compilati dal workflow sul commit pubblicato e
possono avere hash diversi da quelli locali.

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 41/41: 39 suite C++ con **10 316** controlli, `mem_pools_smoke` (≈ 333 000) e `sd_interop_test` (41) | 41/41: **10 310** controlli + ≈ 334 000 + 41 |
| AddressSanitizer + UBSan (`--sanitize`) | 41/41 | 41/41 |
| ThreadSanitizer (`--tsan`) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |

## v1.11.0 — tag `v1.11.0`

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`),
una cartella per scheda, nello stesso ambiente della v1.9.0/v1.10.0 (Pico SDK
2.2.0, `gcc-arm-none-eabi` 13.2.1). **Nessuna prova su hardware.**

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (3 190 784 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 172 864 byte) |
| `picotool info` | "PicoCalc FX-CG50 AI", versione `1.11.0 (pico_plus_2w)`, binary end `0x1018564C` | "PicoCalc FX-CG50 AI", versione `1.11.0 (pico2w)`, binary end `0x101833F8` |
| Immagine flash | 1 594 956 byte (`__flash_binary_end` = `0x1018564C`) = 38 % della partizione firmware da 4 MB; +79 684 byte rispetto alla v1.10.0 | 1 586 168 byte (`0x101833F8`) = 76 % della partizione da 2 MB; +79 652 byte |
| Resto della flash | invariato (diagnostica, LittleFS ≈ 11 MB anche per i CSV, BTstack, workspace 2 × 512 KB, ultimo settore libero) | invariato (LittleFS ≈ 1,9 MB, workspace 128 KB) |
| RAM statica | 302 556 byte fino a `__end__` = `0x200495DC` (+924 byte: glifo ⌟, stato delle nuove schermate e del dialogo dei file) | 298 632 byte (`0x20048688`, +924 byte) |
| Heap SRAM | 191 012 byte (`__end__` → `0x20078000`) | 194 936 byte |
| PSRAM | 8 MB come pool TLSF separato (liste, matrici, CSV letti, forme esatte da 512 byte in su) | — |
| Stack | core 0: 32 KB con guardia MSPLIM; core 1: 4 KB in SCRATCH_X (invariati) | uguale |
| Frame di stack (`-fstack-usage`) | massimo `SystemApp::enter` 1 840 B (le quattro voci nuove del SET UP; non è sul percorso di una ricorsione), poi `diag::testMemory` 1 712 B; codice nuovo: `evaluateEntry` 760 B (era 656), `csv::decode` 424 B, `graphcmd::Runner::run` 328 B, `AnswerView::open` 328 B, `MatrixEditorView::writeCell` 296 B, `Lexer::hexIdentifier` 152 B; la ricorsione del valutatore resta sotto il budget di 16 KB esistente | uguale |
| Warning | **nessuno** (0 righe `warning:` in tutta la build) | nessuno |
| SHA-256 dell'UF2 locale | `5aff577afbae2ee20a644f319ab166606170322cc962523097b569687fc53018` | `b6e04305448a7720f6a7089e3b6a05ac88c2fe72f5e386c61d5e9a4cb085ccbe` |

Gli UF2 allegati alla GitHub Release sono compilati dal workflow
`.github/workflows/release.yml` sul commit pubblicato (Ubuntu 24.04, stesso
SDK) e verificati con `sha256sum -c SHA256SUMS`; i loro hash sono in
`SHA256SUMS` nella release e possono differire da quelli locali se la
toolchain del runner non è identica byte per byte.

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 40/40: 38 suite C++ con **9 757** controlli, `mem_pools_smoke` (≈ 333 000 verifiche) e `sd_interop_test` (41) | 40/40: **9 751** controlli + ≈ 334 000 + 41 |
| AddressSanitizer + UBSan (`--sanitize`) | 40/40 | 40/40 |
| ThreadSanitizer (`--tsan`, allocatore su due thread) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| Simulatore | screenshot 52–61 nuovi; 06, 07, 18, 20–37, 40–45, 51 rigenerati; tutti compressi senza perdita | |

Nuove suite: `lists_csv_test` (721 controlli), `calc_v111_test` (133) e
`run_matrix_v111_test` (106); dettagli in [TESTING.md](TESTING.md). Il
dispositivo non è stato usato.

## v1.10.0 — tag `v1.10.0`

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`),
una cartella per scheda, nello stesso ambiente della v1.9.0 (in fondo alla
pagina). **Nessuna prova su hardware.**

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (3 031 552 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (3 013 632 byte) |
| `picotool info` | "PicoCalc FX-CG50 AI", versione `1.10.0 (pico_plus_2w)`, RP2350 ARM Secure, `pimoroni_pico_plus2_w_rp2350`, SDK 2.2.0 | "PicoCalc FX-CG50 AI", versione `1.10.0 (pico2w)`, RP2350 ARM Secure, `pico2_w`, SDK 2.2.0 |
| Immagine flash | 1 515 272 byte (`__flash_binary_end` = `0x10171F08`) = 36 % della partizione firmware da 4 MB; +129 056 byte rispetto alla v1.9.0 | 1 506 516 byte (`0x1016FCD4`) = 71 % della partizione da 2 MB; +129 000 byte |
| Resto della flash | invariato rispetto alla v1.9.0 (workspace 2 × 512 KB) | invariato (workspace 128 KB) |
| RAM statica | 301 632 byte fino a `__end__` = `0x20049240` (+2 188 byte: palette dei colori, stato dell'editor MAT/VCT e della selezione) | 297 708 byte (`0x200482EC`, +2 188 byte) |
| Heap SRAM | 191 936 byte (`__end__` → `0x20078000`) | 195 860 byte |
| PSRAM | 8 MB come pool TLSF separato (le matrici da 512 byte in su finiscono qui) | — |
| Stack | core 0: 32 KB con guardia MSPLIM; core 1: 4 KB in SCRATCH_X (invariati) | uguale |
| Frame di stack (`-fstack-usage`) | massimi invariati (`diag::testMemory` 1712 B, `GraphApp::openProject` 1696 B); codice nuovo: `CollEval::eval` 248 B (numerico) / 384 B (esatto), `CollEval::call` 504 B / 856 B, `MatrixEditorView` 368 B; la ricorsione delle collezioni passa per il controllo del budget di stack esistente | uguale |
| Warning | **nessuno** (0 righe `warning:` in tutta la build) | nessuno |
| Avvisi CMake | solo quello dell'SDK su picotool (compilato dai sorgenti) | uguale |
| SHA-256 dell'UF2 locale | `ad810d07598f8239d485429bdb27ea4fcaff033a4a1d792a97837e465d194553` | `ab6a3d7e8d5cd8eff64d818511873486ce305401b09e3dd02429b6076f58915e` |

Gli UF2 allegati alla GitHub Release sono compilati dal workflow
`.github/workflows/release.yml` sul commit pubblicato (Ubuntu 24.04, stesso
SDK) e verificati con `sha256sum -c SHA256SUMS`; i loro hash sono in
`SHA256SUMS` nella release e possono differire da quelli locali se la
toolchain del runner non è identica byte per byte.

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 37/37: 35 suite C++ con **8 587** controlli, `mem_pools_smoke` (≈ 333 000 verifiche) e `sd_interop_test` (41) | 37/37: **8 581** controlli + ≈ 334 000 + 41 |
| AddressSanitizer + UBSan (`--sanitize`) | 37/37 | 37/37 |
| ThreadSanitizer (`--tsan`, allocatore su due thread) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| Simulatore | screenshot 12–19 rigenerati, 44–51 nuovi | |

Nuove suite: `matrix_test` (673 controlli) ed `editor_v110_test` (161); dettagli
in [TESTING.md](TESTING.md). Il dispositivo non è stato usato.

## v1.9.0 — `main`

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`),
una cartella per scheda. Ambiente in fondo alla pagina (invariato; relay con
`anthropic` 2.7.0 e `openai` 3.48.0). **Nessuna prova su hardware.**

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (2 772 992 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (2 755 584 byte) |
| `picotool info` | "PicoCalc FX-CG50 AI", versione `1.9.0 (pico_plus_2w)`, RP2350 ARM Secure, `pimoroni_pico_plus2_w_rp2350`, SDK 2.2.0 | "PicoCalc FX-CG50 AI", versione `1.9.0 (pico2w)`, RP2350 ARM Secure, `pico2_w`, SDK 2.2.0 |
| Immagine flash | 1 386 216 byte (`__flash_binary_end` = `0x101526E8`) = 33 % della partizione firmware da 4 MB | 1 377 516 byte (`0x101504EC`) = 65 % della partizione da 2 MB |
| Resto della flash | diagnostica 4 KB, LittleFS 11 248 KB, BTstack 8 KB, workspace 1024 KB (2 × 512 KB), ultimo settore libero (E10) | diagnostica 4 KB, LittleFS 1908 KB, BTstack 8 KB, workspace 128 KB nella posizione della v1.8.0 |
| RAM statica | 299 444 byte fino a `__end__` = `0x200491B4` (di cui 200 KB framebuffer) | 295 520 byte (`0x20048260`) |
| Heap SRAM | 192 076 byte (`__end__` → `0x20078000`) | 196 000 byte |
| PSRAM | 8 MB come pool TLSF separato, creato a runtime dopo il self-test; nessuna sezione statica in PSRAM | — |
| Stack | core 0: 32 KB (`0x20078000`–`0x20080000`, guardia MSPLIM); core 1: 4 KB in SCRATCH_X | uguale |
| Frame di stack massimi (`-fstack-usage`) | `diag::testMemory` 1712 B, `GraphApp::openProject` 1696 B, `main` 1584 B; moduli nuovi: `graph3d` 552 B, `Graph3DApp` 520 B, `board_pico` 608 B, `flash_fs` 456 B, `psram` 128 B, TLSF 40 B | uguale |
| Warning | **nessuno** (0 in tutta la build, SDK incluso; FatFs e LittleFS compilati con `-w`) | nessuno |
| Avvisi CMake | solo quello dell'SDK su picotool (non installato: compilato dai sorgenti) | uguale |
| SHA-256 dell'UF2 | `805a6c4912aae0ae65d7abb5b5350d9893cdc9bca6c9cf8ec23cdf3e99881f65` | `489911a97bcbfeab4c717cdc0bce2bd1f5f57a8589e49cff83d79a3beeb56591` |
| Riproducibilità | due build pulite in cartelle diverse e una incrementale producono UF2 identici byte per byte | uguale |

Rispetto alla v1.8.0 (`-O3`, 1 505 640 byte di immagine) l'immagine è più
piccola perché il livello predefinito ora è `-O2` (codice più compatto per la
cache XIP da 16 KB condivisa con la PSRAM); `-O3` resta disponibile
(`--opt O3`). La RAM statica cresce di 6,7 KB sulla Plus (stato del 3D,
metadati dei pool, buffer di rimbalzo della flash, buffer riservati per i
messaggi d'errore) e di 2,9 KB sul Pico 2 W, che non ha l'oggetto del pool
PSRAM.

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 35/35: 33 suite C++ con 7 705 controlli, `mem_pools_smoke` (≈ 333 000 verifiche di integrità, numero variabile perché due thread si contendono i pool) e `sd_interop_test` (41) | 35/35: 7 699 controlli + ≈ 333 000 + 41 |
| AddressSanitizer + UBSan (`--sanitize`) | 35/35 | 35/35 |
| ThreadSanitizer (`--tsan`, allocatore su due thread) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| v1.8.0 rieseguita come riferimento | 25/25, **7 179** controlli + 41 (la documentazione della v1.8.0 indicava 7169: il conteggio di `architecture_test` era fermo a 529, il test ne esegue 539) | |

`scripts/make_release.sh` (dopo le due build) produce lo zip dei sorgenti, i due
UF2 e `SHA256SUMS`; verificato su questo commit. Il pacchetto non è stato
pubblicato come release GitHub.

Dettagli per suite in [TESTING.md](TESTING.md). Le build dei test usano gli
stessi sorgenti portabili del firmware; il dispositivo non è stato usato.

## v1.8.0 — collaudo software di GRAPH e microSD, completamento di TABLE

Stesso ambiente. Build pulita con `scripts/build_pico2w.sh` (Release, `-O3`).

| Voce | Valore |
|---|---|
| Output | `build/pico2w/picocalc_fxai.uf2` (3 012 096 byte); `picotool info`: "PicoCalc FX-CG50 AI", versione 1.8.0, RP2350, `pico2_w`, SDK 2.2.0 |
| Immagine flash | ≈1470 KB (`__flash_binary_end` = `0x1016F968`) su 4096 KB (+67 KB: TABLE, diagnostica, ricerca dei poli, recupero della microSD) |
| RAM statica | ≈286 KB fino a `__end__` = `0x200476E8` (+16 byte) |
| Heap | ≈194 KB; stack 32 KB |
| Warning | nessuno nei sorgenti del progetto |
| Test host | 25 test CTest: 24 suite con 7169 controlli + `sd_interop_test` (41 controlli con fsck.fat, fsck.exfat, mtools); tutti superati anche con ASan + UBSan |
| Relay AI | 15 test Python superati |
| Stack | frame massimi dei moduli toccati: diagnostica 808 B, `writeAtomic` 640 B, `graph::integrate` 616 B, `TableApp::drawList` 424 B |
| Hardware | **non provato**: procedura in [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md) |

## v1.8.0 (branch `version/v1.8.0`) — sessione GRAPH

Stesso ambiente della v1.7.0 (sotto). Build pulita con
`scripts/build_pico2w.sh` (Release, `-O3`).

| Voce | Valore |
|---|---|
| Output | `build/pico2w/picocalc_fxai.uf2` (2 874 880 byte); `picotool info`: nome "PicoCalc FX-CG50 AI", versione 1.8.0, RP2350 ARM Secure, `pico2_w`, SDK 2.2.0 |
| Immagine flash | ≈1403 KB (`__flash_binary_end` = `0x1015EDA4`) su 4096 KB; area dati utente (ultimi 128 KB) libera |
| RAM statica | ≈286 KB fino a `__end__` = `0x200476D8` (di cui 200 KB framebuffer) |
| Heap | ≈194 KB (`0x200476D8` → `0x20078000`) |
| Stack core 0 | 32 KB (`0x20078000`–`0x20080000`) |
| Warning | nessuno nei sorgenti del progetto; FatFs (`third_party/fatfs`) è compilato con `-w` |
| Test host | 22 suite, 6540 controlli, tutti superati; anche con ASan + UBSan |
| Relay AI | 15 test Python superati |
| Stack dei frame di GRAPH | misurato con `-fstack-usage` (massimo `GraphApp::openProject` 1712 byte, vedi [GRAPH_ENGINE.md](GRAPH_ENGINE.md#stack-e-prestazioni-sul-target)) |
| Hardware | **non provato** sulla PicoCalc (né display né microSD) |

Crescita rispetto alla v1.7.0: +273 KB di flash (motore GRAPH, G-Solve, nuova
interfaccia GRAPH, progetti e immagini, driver SD e FatFs), +3,2 KB di RAM
statica (stato GRAPH nel workspace, oggetti del driver SD, buffer LFN statico
di FatFs; il volume FatFs con il suo settore è allocato nell'heap); l'heap
scende di 3,2 KB. Memoria dei grafici a runtime limitata a 96 KB di punti
(`MaxTotalCurvePoints`) più le immagini RLE (≤ 96 KB ciascuna).

## v1.7.0 (branch `version/v1.7.0`)

Stesso ambiente della v1.6.0 (sotto), ricreato da zero nel container: toolchain
`gcc-arm-none-eabi` 13.2.rel1, Pico SDK 2.2.0 con i submodule tinyusb,
cyw43-driver, lwip, mbedtls.

| Voce | Valore |
|---|---|
| Output | `build/pico2w/picocalc_fxai.uf2` (2 315 776 byte, build pulita) |
| Immagine flash | ≈1130 KB (`__flash_binary_end` = `0x1011A978`) su 4096 KB; area dati utente (ultimi 128 KB) libera |
| RAM statica | ≈283 KB fino a `__end__` = `0x200469E4` (di cui 200 KB framebuffer) |
| Heap | ≈197 KB (`0x200469E4` → `0x20078000`) |
| Stack core 0 | 32 KB (`0x20078000`–`0x20080000`) |
| Warning | nessuno nei sorgenti del progetto (build pulita; solo i due avvisi CMake dell'SDK su picotool e btstack) |
| Test host | 17 suite, 5347 controlli, tutti superati; anche con ASan + UBSan |
| Relay AI | 15 test Python superati |
| Stack dei nuovi frame | misurato con `-fstack-usage` (vedi [MATH_ENGINE.md](MATH_ENGINE.md)) |
| Hardware | **non provato** sulla PicoCalc |

Crescita rispetto alla v1.6.0: +111 KB di flash (aritmetica esatta, valutatore
esatto, registro e modelli, nuova interfaccia di Run-Matrix), +2,6 KB di RAM
statica (forme esatte di A–Z/Ans nel workspace).

## Ambiente di build verificato

| Componente | Versione |
|---|---|
| Sistema | Ubuntu 24.04.5 (container x86_64) |
| Toolchain ARM | `arm-none-eabi-gcc` 13.2.1 (pacchetto `gcc-arm-none-eabi` 15:13.2.rel1-2) |
| Pico SDK | 2.2.0 (submodule: tinyusb, cyw43-driver, lwip, mbedtls 3.6.2) |
| CMake / Ninja | 3.28.3 / 1.11 |
| Compilatore host | g++ 13.3 (C++20, `-Wall -Wextra -Werror`) |
| Python (relay) | 3.13, `anthropic` 2.7.0, `openai` 3.48.0 (v1.9.0; v1.8.0: 2.6.0 / 3.46.0) |
| picotool | 2.2.0, compilato dall'SDK durante la build |

## Audit della v1.5.4 di partenza

La v1.5.4 **non poteva essere compilata per l'RP2350** e conteneva difetti che
avrebbero impedito il funzionamento sulla PicoCalc:

| # | Problema trovato | Correzione v1.6.0 |
|---|---|---|
| 1 | `board_port.cpp` usava `glyph()` non dichiarata → errore di compilazione | driver riscritto (`src/platform/pico/board_pico.cpp`) |
| 2 | Il Pico SDK disabilita le eccezioni C++, ma il motore usa `throw` ovunque | `PICO_CXX_ENABLE_EXCEPTIONS=1` |
| 3 | Link a `pico_cyw43_arch_lwip_*` senza `lwipopts.h` → errore | `src/platform/pico/config/lwipopts.h` |
| 4 | Stack di 2 KB (SCRATCH_Y 4 KB) insufficiente per parser ricorsivo ed eccezioni | linker script con stack da 32 KB in SRAM |
| 5 | Init LCD diversa dal driver ufficiale: senza `INVON` (colori invertiti), parametri B0/B1/B4/B6 scambiati, senza gamma | sequenza ufficiale ClockworkPi |
| 6 | Documentazione e codice affermavano che il display richiede PIO-SPI: i driver ufficiali usano **SPI1 hardware** | SPI1 a 25 MHz come i driver ufficiali |
| 7 | Lettura tastiera con repeated-start senza STOP e attesa (il driver ufficiale usa STOP + 16 ms) | macchina a stati non bloccante con STOP + attesa |
| 8 | Cursore dell'editor calcolato a 8 px/glifo, espressione disegnata a 6 px/glifo → cursore spostato | metriche uniche nel renderer |
| 9 | Test basati su `assert`: con `NDEBUG` tutti i controlli sparivano | macro `CHECK` sempre attive |
| 10 | `binomialPmf` andava in overflow (`nCr` a 64 bit) per n > ~60 | forma log-gamma |
| 11 | Due parser diversi (uno a stringa, uno AST con ~10 funzioni) | pipeline unica tokenizer → AST → valutatore |
| 12 | Nessun limite di lunghezza/annidamento: sul target ARM ogni livello del valutatore occupava ~1,3 KB di stack, quindi una somma di ~25 termini avrebbe superato lo stack da 32 KB (misurato con `-fstack-usage`) | frame ricorsivi ridotti a 32–128 byte, catene valutate in modo iterativo, limiti di token/annidamento, budget di stack con "Stack ERROR" |

## Risultati di build (firmware)

```
cmake -S . -B build/pico2w -G Ninja -DCMAKE_BUILD_TYPE=Release   (PICO_SDK_PATH=/opt/sdk/pico-sdk)
cmake --build build/pico2w
```

| Voce | Valore |
|---|---|
| Output | `build/pico2w/picocalc_fxai.uf2` (≈2,0 MB, famiglia `rp2350-arm-s`) |
| Immagine flash | ≈1019 KB su 4096 KB (include firmware CYW43, lwIP, mbedTLS) |
| Area dati utente | ultimi 128 KB della flash (2 slot A/B), nessuna sovrapposizione con l'immagine |
| RAM statica | ≈261 KB (di cui 200 KB framebuffer 320×320 RGB565) |
| Heap | ≈200 KB (`__end__` → `0x20078000`) |
| Stack core 0 | 32 KB (`0x20078000`–`0x20080000`) |
| Warning | nessuno nei sorgenti del progetto |

`picotool info -a` conferma: nome "PicoCalc FX-CG50 AI", versione 1.6.0,
`pico_board: pico2_w`, SDK 2.2.0, image type ARM Secure, target RP2350.

## Risultati dei test (host)

`tests/run_host_tests.sh` e `tests/run_host_tests.sh --sanitize` (ASan + UBSan):

| Suite | Controlli |
|---|---|
| expression_test | 142 |
| editor_test | 166 |
| math_engine_test | 108 |
| storage_test | 727 |
| app_test | 62 |
| apps_flow_test | 54 |
| solvers_test | 60 |
| statistics_test | 63 |
| ai_test | 138 |
| graphics_smoke | 57 |
| font_smoke | 2556 |
| animation_smoke | 6 |
| cheatsheet_smoke | 343 |
| vpam_ast_smoke | 13 |
| **Totale** | **4495 controlli, 0 errori** (con e senza sanitizer) |

Relay AI (`cd relay && python -m unittest discover -s tests`): **15 test superati**.

## Non verificato (richiede la PicoCalc fisica)

- Display: colori, orientamento, area visibile, velocità di aggiornamento.
- Tastiera: tutti i codici, tenuta/ripetizione, modificatori, tempi I2C.
- Batteria e retroilluminazione via registri STM32.
- Scrittura flash con `flash_safe_execute` e ripristino dopo riavvio/blackout.
- PSRAM APS6404L: rilevamento, self-test, frequenza, convivenza con la cache XIP e con le scritture flash (v1.9.0).
- Lettura del JEDEC ID della flash all'avvio e partizioni della flash da 16 MB della Pico Plus 2 W.
- Stack con guardia MSPLIM, rapporto HardFault, core 1 della diagnostica.
- Wi-Fi CYW43, DNS, handshake TLS con verifica del certificato, consumo di RAM di mbedTLS.
- Stabilità prolungata e consumi.

Nessun test "su hardware" è dichiarato superato: la matrice usa al massimo lo stato
**TESTED ON HOST**.
