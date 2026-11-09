# Consolidamento della v1.9.0 — rapporto

Data: 9 novembre 2026. Branch risultante: **`main`** (tag `v1.9.0`).
Obiettivo: una 1.9.0 che contenga tutta la v1.8.0 più il supporto completo del
Pimoroni Pico Plus 2 W (RP2350B, 8 MB di PSRAM, 16 MB di flash), scegliendo per
ogni componente l'implementazione migliore, senza merge indiscriminati.

**Nessuna prova su hardware fisico**: tutti i risultati qui sotto sono test
host, build del firmware o simulazioni. La procedura di collaudo è in
[HARDWARE_TEST_v1.9.0.md](../HARDWARE_TEST_v1.9.0.md).

## 1. Sintesi

- La prima 1.9.0 (`main` = `version/v1.9.0`, commit `f1f3089` e `9ed7fdb`) era
  stata sviluppata **sulla v1.5.4**: aveva il layer hardware della Plus ma non
  le 24 commit della v1.6.0–v1.8.0 (framework delle app, Run-Matrix esatto,
  GRAPH, TABLE, microSD, AI, driver ufficiali). Era quindi funzionalmente
  inferiore alla v1.8.0.
- Il consolidamento ha unito le due linee con un merge reale
  (`352f147`, conflitti risolti a favore dell'architettura della v1.8.0) e ha
  poi portato i moduli hardware della 1.9.0 dentro quell'architettura
  (`cef21ec`), con le correzioni trovate durante il lavoro (`4bbc80a`,
  `3fd82fb`, `6b532ed`, `daa5fe6`) e la documentazione (`d23e3e3`).
- Risultato: tutte le modalità e i test della v1.8.0 (nessuna suite rimossa o
  ridotta) + PSRAM come pool separato con avvio solo SRAM in caso di guasto,
  flash da 16 MB partizionata, LittleFS, 3D Graph, diagnostica estesa;
  Pico 2 W mantenuto come profilo alternativo con build separata.

## 2. Branch analizzati

Confronti eseguiti con `git merge-base`, `git rev-list`, `git log` e `git diff`
su tutti i branch remoti (stato iniziale; backup completo di tutti i branch in
un bundle Git prima di ogni operazione).

| Branch | Testa | Base | Contenuto | Commit non presenti altrove |
|---|---|---|---|---|
| `main` (iniziale) | `9ed7fdb` | v1.5.4 (`537d591`) | prima 1.9.0: v1.5.4 + supporto Pico Plus 2 W | — (diventa la base del consolidamento) |
| `version/v1.9.0` | `9ed7fdb` | = `main` | identico a `main` | 0 |
| `release/1.9.0` | `f1f3089` | v1.5.4 | prima commit della 1.9.0, senza `9ed7fdb` (solo documentazione) | 0 |
| `claude/pimoroni-pico-plus-2w-94lg56` | `9ed7fdb` | = `main` | branch di lavoro della prima 1.9.0 | 0 |
| `ccr-90a0c133-k0rf3a` | `9ed7fdb` | = `main` | branch di lavoro della prima 1.9.0 | 0 |
| `version/v1.8.0` | `c595ef6` | v1.5.4 | v1.6.0 → v1.8.0, 24 commit assenti in `main` | 24 (tutte integrate) |
| `claude/loving-meitner-jifxhg` | `c595ef6` | = `version/v1.8.0` | branch di lavoro della v1.8.0 | 0 |
| `version/v1.7.0`, `claude/laughing-mendel-jewowt` | `72285e9` | antenato di v1.8.0 | v1.7.0 | 0 |
| `claude/peaceful-euler-auyrm5` | `7dc65cd` | antenato di v1.8.0 | v1.6.0 + documentazione | 0 |
| `version/v1.6.0` | `1e8bfca` | antenato di v1.8.0 | v1.6.0 | 0 |
| `version/v1.5.4` | `537d591` | — | codice originale | 0 |

`merge-base(main, version/v1.8.0)` = `537d591` (v1.5.4): le due linee si erano
separate dalla v1.5.4. Dopo il consolidamento **ogni branch remoto è antenato
di `main`** (0 commit unici rispetto a `main`).

## 3. Commit importanti

| Commit | Linea | Contenuto |
|---|---|---|
| `0fa281b` | v1.6.0 | base RP2350, driver ufficiali PicoCalc, pipeline matematica unica (sostituisce window manager, UI e driver PIO della v1.5.4) |
| `b6fc2f4`, `2fb13c9` | v1.6.0 | modalità Equation, Statistics, Graph, Table, System, Memory, Conversion; relay AI HTTPS e chat |
| `c15f489` | v1.6.0 | budget di stack di parser/valutatore/editor per i 32 KB dell'RP2350 |
| `25063e6`, `a41b527` | v1.7.0 | Run-Matrix con risultati esatti; registro delle applicazioni e test di architettura |
| `04cef02`…`fd6be96` | v1.8.0 | motore GRAPH, G-Solve, progetti, driver microSD + FatFs, budget di memoria |
| `75fd63c`, `7cd5dad`, `43b1b8c`, `c595ef6` | v1.8.0 | collaudo software di GRAPH e microSD, TABLE completa, diagnostica |
| `f1f3089` | prima 1.9.0 | Pico Plus 2 W: profili di scheda, PSRAM, TLSF, pool, `operator new`, limiti, runtime di sistema, LittleFS, 3D, test della memoria (su base v1.5.4) |
| `9ed7fdb` | prima 1.9.0 | documentazione della base |
| `352f147` | consolidamento | merge di `version/v1.8.0` nella linea 1.9.0 (architettura della v1.8.0) |
| `cef21ec` | consolidamento | layer hardware della 1.9.0 collegato al firmware della v1.8.0, 3D Graph, correzioni, test |
| `4bbc80a` | consolidamento | autosalvataggio ripreso dopo un "Memory ERROR" |
| `3fd82fb` | consolidamento | protezione della flash con l'UF2 della scheda sbagliata; codice morto rimosso |
| `6b532ed` | consolidamento | colori delle superfici 3D, simulatore con il pool PSRAM, screenshot |
| `d23e3e3` | consolidamento | documentazione della v1.9.0 e questo rapporto |
| `daa5fe6` | consolidamento | `make_release.sh`: verifica dei sorgenti di ogni UF2 per commit |

## 4. Differenze tra la v1.8.0 e la prima 1.9.0

| Area | v1.8.0 (`version/v1.8.0`) | prima 1.9.0 (`main` iniziale) |
|---|---|---|
| Base | v1.5.4 + 24 commit | v1.5.4 + 2 commit |
| Schede | Pico 2 W | Pico Plus 2 W e Pico 2 W |
| Interfaccia | framework delle app, 10 modalità complete, launcher a 23 icone | window manager e UI della v1.5.4 |
| Matematica | pipeline unica, risultati esatti, S⇔D, cronologia modificabile | motore della v1.5.4 |
| GRAPH / TABLE | motore completo, G-Solve, SKETCH, PICT, progetti, TABLE completa | `graph_renderer` della v1.5.4 |
| Driver | LCD SPI1 e tastiera STM32 ufficiali, microSD SPI0 + FatFs | LCD via PIO della v1.5.4, sola sonda della microSD (`FXAI_ENABLE_SD`), nessun file system |
| AI | relay HTTPS, client Wi-Fi/TLS, chat | stub della v1.5.4 ("AI non configurata"), nessuna connessione |
| Memoria | heap SRAM ≈ 194 KB, budget fissi | PSRAM 8 MB, pool TLSF, `operator new` instradato, limiti dinamici |
| Flash | record di 128 KB alla fine della flash | partizioni per scheda, LittleFS, BTstack |
| Robustezza | budget di stack | MSPLIM, HardFault, core 1 |
| Test | 25 test (7 179 controlli misurati) | test della memoria/PSRAM + test della v1.5.4 |

## 5. Matrice di provenienza dei componenti

Per ogni componente: da dove viene il codice nella 1.9.0 consolidata e perché.

| Componente | Scelta | Motivo |
|---|---|---|
| Framework delle app, launcher, registro, test di architettura | v1.8.0 | sostituisce window manager e UI della v1.5.4 (rimossi già nella v1.6.0, `0fa281b`) |
| Run-Matrix, editor Natural, parser/valutatore, risultati esatti | v1.8.0 | la prima 1.9.0 modificava il motore della v1.5.4, superato |
| GRAPH, G-Solve, TABLE, progetti `.pcgraph`, PICT | v1.8.0 + budget della 1.9.0 | i limiti dei punti e delle immagini ora vengono da `limits::ram()`; cache dei layer nuova |
| Statistics, Equation, Memory, System, Conversion, Symbols | v1.8.0 | estese: liste da 999 sulla Plus, MEMORY per il 3D, SYSTEM INFO/DIAG/"Archivio file" |
| Driver LCD, tastiera, microSD + FatFs | v1.8.0 | driver ufficiali; pin invariati |
| Relay AI, protocollo, chat | v1.8.0 (identici) | chiavi solo sul relay |
| Profili di scheda, `board_config.hpp`, `cmake/fxai_board.cmake` | prima 1.9.0, riscritti | mappa della flash per scheda con `static_assert`; pin della v1.8.0 |
| Driver PSRAM e self-test | prima 1.9.0 | avviato in `board::init()` prima dei pool; stub host che segue il pool simulato |
| TLSF, pool di memoria, `operator new` instradato | prima 1.9.0 | nessun oggetto pool PSRAM sul Pico 2 W (3,6 KB di SRAM risparmiati) |
| Limiti | prima 1.9.0, riscritti | tre categorie: persistenti per scheda, RAM dalla memoria presente, strutturali |
| Runtime di sistema (MSPLIM, HardFault, core 1) | prima 1.9.0 | helper di `flash_safe_execute` del progetto; rapporto del guasto al riavvio |
| LittleFS | prima 1.9.0, adattato | backend di `files::FileSystem` (stessa interfaccia di FatFs) |
| Diagnostica | v1.8.0 + prima 1.9.0 | gruppi MEM/PSR/ALC/FLS/LCD/INT uniti a SD/GPH della v1.8.0 |
| 3D | prima 1.9.0 (motore), riscritto | motore a più superfici con z-buffer o pittore e app 3D Graph nel framework |
| Storage del workspace | v1.8.0 + correzioni | coda riservata (E10), formato v5, migrazione dalla v1.8.0 |
| Canvas (poligoni oltre 32 vertici, copie di layer) | prima 1.9.0 (merge automatico) | provato da `graphics_memory_test` |
| Script di build | prima 1.9.0, aggiornati | `build.sh --board`, `build_pico2w.sh` come scorciatoia, release con i due UF2 verificati per commit |
| Test | v1.8.0 + test della memoria della 1.9.0 adattati + 6 suite nuove | entrambi i profili, sanitizer |

Rimossi: window manager, `ui.cpp`, driver LCD PIO, `main.cpp`, `graph_renderer`,
emulatore SDL2 e i loro test (già sostituiti nella v1.6.0, `0fa281b`);
`include/lwipopts.h` della 1.9.0 (avrebbe nascosto la configurazione TLS);
`FXAI_ENABLE_CAS` (riservava memoria per un modulo inesistente),
`FXAI_ENABLE_SD` (la v1.8.0 ha lo stack microSD completo), contatore di avvii
scritto in flash a ogni accensione (usura), buffer AI da 2 MB (il relay limita
le risposte a 4000 caratteri), `FXAI_ENABLE_3D` (senza effetto), quattro test
della prima 1.9.0 scritti per l'API della v1.5.4 (coperti dalle nuove suite).

## 6. Funzionalità recuperate dalla v1.8.0

Tutte: Run-Matrix (input naturale 2D, risultati esatti, S⇔D, frazioni miste,
periodo, cronologia modificabile con ricalcolo, Base-N, DMS, complessi,
undo/redo, copia/incolla); Statistics (liste, 1/2-Var, 9 regressioni, grafici,
distribuzioni, test e intervalli); GRAPH (20 relazioni di tutti i tipi,
disequazioni, TRACE, ZOOM, V-Window e memorie, SKETCH, G-SOLVE con ∫dx,
Modify, memorie grafiche, PICT e sfondo, progetti); TABLE completa; Equation;
Memory; System; Conversion; AI Chat con relay HTTPS; Symbols; microSD con FatFs
e recupero dopo l'estrazione; diagnostica SYSTEM → DIAG; simulatore con
screenshot; tutte le 25 suite di test. Verifica: `architecture_test` apre ogni
modalità del registro nel firmware reale; le suite della v1.8.0 passano con
gli stessi o più controlli.

## 7. Componenti hardware recuperati dalla prima 1.9.0

Selezione della scheda e cartelle di build separate; driver PSRAM APS6404L
(QMI CS1 su GPIO47, QPI, 75 MHz con clk_sys 150 MHz, ID 0x0D/0x5D, self-test
all'avvio, verifica e ripristino della configurazione M1); TLSF e pool
SRAM/PSRAM con politiche esplicite e `std::pmr`; instradamento di
`operator new`; limiti dinamici; guardia MSPLIM dello stack del core 0,
gestore HardFault con record in watchdog scratch, avvio del core 1; LittleFS
v2.11.3; informazioni di sistema; motore 3D; opzione BTstack; controllo della
dimensione dell'immagine nella partizione firmware.

## 8. Modifiche architetturali

- Avvio: `paintStacks()`/`takeLastFault()` → `board::init()` (PSRAM e
  self-test, pool, LCD, tastiera, JEDEC della flash, regioni) →
  `Firmware::boot()` (workspace o migrazione) → avvisi.
- Memoria: due pool distinti (la PSRAM non è trattata come SRAM): stack,
  framebuffer, codice critico e dati usati con l'XIP fermo restano in SRAM; i
  dati pesanti vanno in PSRAM tramite `operator new` (≥ 512 byte) o
  `mem::tryAlloc(PREFER_PSRAM)`.
- Limiti in tre categorie (`limits.hpp`); il profilo RAM è scelto a runtime,
  quindi una Plus con la PSRAM guasta usa i limiti del Pico 2 W.
- Flash: mappa per scheda, regioni limitate all'immagine e al chip reale,
  tutte le scritture da `flash_safe_execute` con buffer di rimbalzo in SRAM.
- Workspace formato v5 (archivio dei file, stato del 3D); letti v1–v4.
- File dell'utente: `files::FileSystem` con due backend (FatFs sulla microSD,
  LittleFS nella flash interna), scelti in SYSTEM.
- Registro delle applicazioni: 3D Graph disponibile; servizi `Graph3D` e
  `Hardware`; sezione dati `graph3d`; il test di architettura li verifica.
- Errori: percorso di "Memory ERROR" senza allocazioni (buffer riservati); il
  ciclo principale intercetta ogni eccezione residua.

## 9. Moduli integrati

`board_config.hpp`, `cmake/fxai_board.cmake`, `psram.cpp`, `tlsf.cpp`,
`mem_pools.cpp`, `mem_new.cpp`, `limits.cpp`, `system_runtime.cpp`,
`system_info.cpp`, `hardware_info.hpp`, `flash_fs.cpp` (+ `third_party/littlefs`),
`graph3d.cpp`, `apps/graph3d_app.cpp`, diagnostica unita, `board_pico.cpp`
(regioni flash, JEDEC, framebuffer opzionale in PSRAM), `main_pico.cpp`
(avvisi, ciclo protetto), `firmware.cpp` (migrazione, autosalvataggio sicuro),
`app_manager.cpp`/`ui_kit.cpp` (errori senza allocazioni), `storage.cpp`
(coda riservata), `workspace.cpp` (v5), app GRAPH (cache dei layer), SYSTEM,
MEMORY.

## 10. Bug individuati e corretti

| # | Difetto | Origine | Correzione | Test |
|---|---|---|---|---|
| 1 | Lo slot B del workspace usava l'ultimo settore della flash, sovrascritto dal workaround del bootrom per l'erratum RP2350-E10 durante gli aggiornamenti UF2 | v1.8.0 | coda riservata di 4 KB: nessun salvataggio la scrive; la posizione del Pico 2 W resta quella della v1.8.0 | `board_profile_test`, `storage_test` |
| 2 | `flash_range_program` leggeva i dati dal buffer del chiamante, che con la PSRAM può stare in PSRAM (illeggibile con l'XIP fermo) | v1.8.0 su scheda con PSRAM | buffer di rimbalzo in SRAM, un settore alla volta, verifica di M1 | `system_diag_test` (logica), HW-45 |
| 3 | Segnalare "Memory ERROR" allocava una `std::string`: con la memoria esaurita il `bad_alloc` usciva da `AppManager::key` e terminava il firmware | v1.8.0 | errori, toast, avvisi e finestra d'errore con buffer riservati | `oom_test` (ha trovato il difetto) |
| 4 | Un `bad_alloc` durante l'autosalvataggio usciva dal ciclo principale | v1.8.0 | intercettato, messaggio | `oom_test` |
| 5 | Dopo un salvataggio fallito per memoria la modifica non veniva più salvata se non ne seguiva un'altra | v1.8.0 | modifiche in attesa, nuovo tentativo dopo il ritardo dell'autosalvataggio | `oom_test` |
| 6 | `deserializeWorkspace` teneva un `Workspace` di 5,4 KB sullo stack | v1.8.0 | allocato sull'heap | `-fstack-usage` |
| 7 | Yaw del 3D negativo per rotazioni oltre −360° | prima 1.9.0 | normalizzazione corretta | `graph3d_test` |
| 8 | Un UF2 della Plus su un Pico 2 W avrebbe formattato LittleFS su indirizzi che, oltre i 4 MB, ricadono sul firmware e sui dati | nuovo con i due UF2 | JEDEC ID letto all'avvio, regioni oltre il chip disattivate, avviso | `board_profile_test`, `system_diag_test`, HW-49 |
| 9 | Due superfici 3D con colori quasi uguali (verde su verde) | prima 1.9.0 | una famiglia di colore per superficie | `graph3d_test`, screenshot 41 |
| 10 | `FXAI_ENABLE_CAS` riservava memoria per un modulo inesistente; contatore di avvii scritto in flash a ogni accensione; buffer AI da 2 MB | prima 1.9.0 | rimossi | — |
| 11 | `FXAI_ENABLE_3D` non aveva effetto | prima 1.9.0 | opzione rimossa | — |
| 12 | `make_release.sh` rifiutava sempre il primo UF2 dopo la build della seconda scheda: il Pico SDK tocca `CMakeLists.txt` a ogni build quando compila picotool dai sorgenti e il controllo usava le date dei file | prima 1.9.0 / due UF2 | `build.sh` registra commit dei sorgenti e hash dell'UF2, la release li verifica | flusso completo e tre casi negativi eseguiti |
| 13 | La documentazione della v1.8.0 dichiarava 7169 controlli; il test ne esegue 7179 (`architecture_test` 539, non 529) | documentazione v1.8.0 | numeri misurati in TESTING/BUILD_STATUS | — |

Difetti dei test della prima 1.9.0 corretti durante l'adattamento: API dei
limiti cambiata (`limits_smoke` riscritto), assunzioni valide solo per il
Pico 2 W (cronologia, record pieno, LISTMEM) rese dipendenti dalla scheda.

## 11. Ottimizzazioni della memoria

- Pool PSRAM separato per i dati pesanti; la SRAM resta libera per stack,
  framebuffer e percorsi critici.
- Limiti persistenti per scheda (Plus: liste da 999, cronologia di 200 righe,
  record di 512 KB per slot) e limiti RAM dalla memoria realmente presente.
- Nessun oggetto pool PSRAM sul Pico 2 W (3,6 KB di SRAM).
- Buffer di rimbalzo della flash da 1 KB invece di un settore intero.
- `Workspace` temporaneo della deserializzazione fuori dallo stack.
- `-O2` come livello predefinito: immagine di 1,32 MB contro 1,44 MB della
  v1.8.0 a `-O3`, più codice nella cache XIP da 16 KB condivisa con la PSRAM.
- Framebuffer opzionalmente in PSRAM (`FXAI_FRAMEBUFFER_IN_PSRAM`, 200 KB di
  heap SRAM in più), con ripiego in SRAM.
- Frammentazione: TLSF a tempo costante; esaurimento e frammentazione provati
  da `mem_pools_smoke`; integrità dei pool verificata in DIAG.

## 12. Utilizzo effettivo della PSRAM

Sulla Plus, dopo il self-test, gli 8 MB diventano il pool `PSRAM_BOARD`
(alias con cache 0x11000000). Vanno in PSRAM:

| Dato | Dimensione tipica / massima |
|---|---|
| Punti delle curve GRAPH | fino a 400 000 punti (1,6 MB) per schermata |
| Cache dei layer statici di GRAPH durante TRACE | ≈ 180 KB |
| Immagini PICT, sfondo, BMP importato | fino a 1 MB ciascuna |
| Mesh e z-buffer del 3D | griglia fino a 128 × 128 per 3 superfici, z-buffer della vista |
| Scratch dei poligoni pieni | fino a 4096 vertici |
| Workspace serializzato per il salvataggio | fino a 512 KB |
| Liste da 999 elementi, cronologia, tabelle, buffer grandi delle app | tramite `operator new` (≥ 512 byte) |
| Framebuffer | solo con `FXAI_FRAMEBUFFER_IN_PSRAM=ON` |

Restano in SRAM: stack dei due core, framebuffer predefinito, strutture di
controllo TLSF, codice e dati usati con l'XIP fermo (scritture flash),
buffer di rimbalzo. Nessuna sezione statica è collocata in PSRAM (verificato
con `objdump -h`). La percentuale d'uso reale sul dispositivo si legge in
SYSTEM → INFO/DIAG (HW-41, HW-42).

## 13. Ottimizzazioni grafiche

- **Cache dei layer di GRAPH** (solo con PSRAM): durante TRACE e il puntatore
  si ricopia l'immagine dei livelli statici invece di ridisegnarli. Misura sul
  PC con 249 000 punti: 0,05 ms per passo invece di 18,6 ms; immagine identica
  al pixel. Motivazione misurata: con i budget della PSRAM il ridisegno
  completo cresce da 2,0 ms (24 000 punti) a 23,6 ms (249 000 punti).
- **3D**: campionamento a passi nel `tick()` (tastiera reattiva), rotazione e
  zoom con la sola riproiezione (nessun nuovo campionamento), z-buffer in PSRAM.
- Poligoni pieni senza il limite di 32 vertici; capacità delle regioni sporche
  per scheda (32 / 16).
- Non adottati, per mancanza di un miglioramento misurabile senza hardware:
  DMA del display (il collo di bottiglia è il bus SPI), disegno sul core 1
  (oggi usato solo dalla diagnostica: richiederebbe sincronizzazione su canvas
  e pool), framebuffer in PSRAM come predefinito.
- Parser e operazioni matriciali: invariati rispetto alla v1.8.0 (GRAPH
  compila le espressioni una volta per modello); l'API delle matrici accetta
  fino a 999×999 con la PSRAM, l'editor MAT non esiste ancora.

## 14. Risultati dei test

| Prova | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (35 test: 34 suite C++ + interoperabilità) | 35/35, 7 705 controlli + ≈ 333 000 verifiche dello stress dell'allocatore + 41 | 35/35, 7 699 + ≈ 333 000 + 41 |
| AddressSanitizer + UBSan | 35/35 | 35/35 |
| ThreadSanitizer (allocatore su due thread) | superato | superato |
| Relay AI | 15/15 (anthropic 2.7.0, openai 3.48.0, TLS in loopback) | — |
| Simulatore | screenshot 40–43 del 3D (profilo Plus con pool PSRAM) | — |
| v1.8.0 di riferimento | 25/25, 7 179 + 41 controlli | |
| Prima 1.9.0 di riferimento | suite della base v1.5.4 + test della memoria superati su entrambi i profili | |

Distinzione: test host (logica sul PC), build del firmware (sotto), test
simulati (PSRAM, flash NOR, scheda SD, simulatore), **test hardware: nessuno**.

## 15. Risultati delle compilazioni

Pico SDK 2.2.0, `arm-none-eabi-gcc` 13.2.1, build pulite con
`scripts/build.sh --clean`, `-O2`.

| | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| UF2 | `picocalc_fxai_pico_plus_2w.uf2`, 2 772 992 byte | `picocalc_fxai_pico2w.uf2`, 2 755 584 byte |
| Immagine | 1 386 216 byte, 33 % della partizione da 4 MB | 1 377 516 byte, 65 % della partizione da 2 MB |
| RAM statica / heap SRAM | 299 444 / 192 076 byte | 295 520 / 196 000 byte |
| PSRAM | pool di 8 MB a runtime | — |
| Stack | core 0 32 KB (MSPLIM), core 1 4 KB; frame massimo 1712 byte | uguale |
| Warning | 0 (anche nell'SDK) | 0 |
| `picotool info` | 1.9.0 (pico_plus_2w), `pimoroni_pico_plus2_w_rp2350` | 1.9.0 (pico2w), `pico2_w` |
| SHA-256 dell'UF2 | `805a6c49…881f65` | `489911a9…b56591` |
| Release | `make_release.sh`: zip + due UF2 + `SHA256SUMS` (non pubblicata) | |

## 16. Branch e tag

Prima della riorganizzazione: bundle Git di tutti i branch, verifica che ogni
branch da eliminare fosse antenato di `main` (o di `version/v1.8.0`, a sua
volta antenato di `main`), nessun force push, nessuna riscrittura della
cronologia pubblica (`main` è avanzato solo con fast-forward, da `9ed7fdb`).

| Branch | Esito |
|---|---|
| `main` | **mantenuto**: v1.9.0 ufficiale |
| `version/v1.5.4`, `version/v1.6.0`, `version/v1.7.0`, `version/v1.8.0` | **mantenuti**: snapshot storici, invariati |
| `version/v1.9.0` (`9ed7fdb`) | **da eliminare**: duplicato della 1.9.0, contenuto in `main` |
| `release/1.9.0` (`f1f3089`) | **da eliminare**: duplicato non aggiornato, contenuto in `main` |
| `claude/pimoroni-pico-plus-2w-94lg56` (`9ed7fdb`) | **da eliminare**: branch di lavoro, contenuto in `main` |
| `ccr-90a0c133-k0rf3a` (`9ed7fdb`) | **da eliminare**: branch di lavoro, contenuto in `main` |
| `claude/loving-meitner-jifxhg` (`c595ef6`) | **da eliminare**: = `version/v1.8.0` |
| `claude/laughing-mendel-jewowt` (`72285e9`) | **da eliminare**: = `version/v1.7.0` |
| `claude/peaceful-euler-auyrm5` (`7dc65cd`) | **da eliminare**: antenato di `version/v1.8.0` |

**Eliminazione non eseguita.** Tutti e sette sono stati verificati (0 commit
non contenuti in `main`, backup in un bundle Git), ma l'ambiente della sessione
ha rifiutato `git push origin --delete` con HTTP 403 (policy del proxy Git, non
un errore di rete) e gli strumenti GitHub disponibili non eliminano branch.
Vanno eliminati dal proprietario, dalla pagina *Branches* del repository su
GitHub oppure da un clone locale:

```bash
git push origin --delete version/v1.9.0 release/1.9.0 claude/laughing-mendel-jewowt claude/loving-meitner-jifxhg claude/peaceful-euler-auyrm5 claude/pimoroni-pico-plus-2w-94lg56 ccr-90a0c133-k0rf3a
```

Nessun lavoro va perso: ogni commit di quei branch è già in `main`.

Tag: `v1.9.0` sull'ultima commit del consolidamento in `main`. La protezione
del branch `main` su GitHub (Settings → Branches) va attivata dal
proprietario del repository: non è configurabile dagli strumenti usati.

## 17. Problemi ancora presenti

- Nessuna verifica su hardware: display, tastiera, PSRAM (timing con i valori
  SparkFun per l'APS6404L), flash, microSD, Wi-Fi, TLS, tempi di GRAPH/3D.
- Le scritture flash mascherano gli IRQ per un settore (≈ 45 ms tipici) o per
  1 KB di programmazione.
- Sul Pico 2 W il record del workspace è di 59,75 KB per slot: molte
  conversazioni AI lunghe possono riempirlo (segnalato).
- Parità con la fx-CG50 non completa (Dual Screen, Graph to Table, modalità con
  solo modello dati, superfici parametriche 3D, editor MAT).
- `PSRAM_PICOCALC` (PSRAM della mainboard) è solo un'interfaccia.
- LTO dell'intero target non supportato dall'SDK (`--wrap`).
- Senza RTC: date dei certificati non verificate, data fissa in FatFs.

## 18. Funzionalità che richiedono la verifica fisica

PSRAM (rilevamento, self-test, 75 MHz, convivenza con la cache XIP e le
scritture flash: HW-40…HW-45); lettura del JEDEC ID e protezione dall'UF2
sbagliato (HW-40, HW-49); partizioni della flash da 16 MB, LittleFS,
aggiornamento UF2 senza perdita di dati, migrazione dalla v1.8.0 (HW-47,
HW-70…HW-74); MSPLIM, HardFault, core 1 (HW-42, HW-44, HW-81); tempi del LCD e
della copia dei layer (HW-46); GRAPH con i limiti della PSRAM (HW-50…HW-54);
3D Graph (HW-60…HW-64); stabilità (HW-80); Wi-Fi e AI (HW-82); tutte le prove
HW-01…HW-36 della v1.8.0 (display, tastiera, microSD, GRAPH, TABLE).

## Fonti

- Erratum RP2350-E10 e spostamento dei dati dall'ultimo settore:
  [raspberrypi/pico-sdk#2412](https://github.com/raspberrypi/pico-sdk/pull/2412).
- Pin e driver della PicoCalc: repository ufficiale
  [clockworkpi/PicoCalc](https://github.com/clockworkpi/PicoCalc).
- Scheda `pimoroni_pico_plus2_w_rp2350` e API `flash_safe_execute`,
  `flash_do_cmd`: Pico SDK 2.2.0.
