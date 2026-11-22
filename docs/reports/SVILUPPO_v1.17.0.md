# Sviluppo della v1.17.0 — base di tutte le modalità

Obiettivo della versione: **ogni modalità del registro si inizializza
correttamente ed è usabile** (si apre, fa il suo lavoro principale, salva e
rimette a posto lo stato, esce senza perdite). Le versioni successive portano
una modalità per volta alla parità completa con la fx-CG50 (prima Statistics,
v1.18.0). Analisi e domande scritte prima del codice:
[ANALISI_FONTI_v1.17.0.md](ANALISI_FONTI_v1.17.0.md). Il **Raspberry Pi Pico
2 W resta congelato** alla v1.16.0: le dieci modalità nuove sono solo per la
Pimoroni Pico Plus 2 W. **Nessuna prova su hardware**:
[HARDWARE_TEST_v1.17.0.md](../HARDWARE_TEST_v1.17.0.md) (HW-220…HW-245).

## 1. Git

| Voce | Valore |
|---|---|
| Partenza | `main` = `eb2338a` (v1.16.0); `git fetch origin main` all'inizio e prima della pubblicazione |
| Branch di lavoro | `claude/eager-lamport-9fht7w` (commit intermedi: analisi, codice, test, strumenti) |
| Pubblicazione | un solo commit su `main` in fast-forward, «PicoCalc FxCG-50 v1.17.0: …»; tag e release dal workflow |

## 2. Decisioni (fase 0)

Risposte dell'utente alle domande di [ANALISI_FONTI §7](ANALISI_FONTI_v1.17.0.md#7-domande-aperte-prima-del-codice):

| # | Domanda | Risposta | Conseguenza |
|---|---|---|---|
| Q1 | Distribution (cap. 18) nel registro? | «Sì, nella v1.18.0» | menu invariato (23 icone); Distribution entra come Base con la parità di Statistics |
| Q2 | Program in C, C++ e C#? | «Confermo» | nucleo F9 della v1.15.0 con i tre dialetti; `.pcprog` con la riga del dialetto |
| Q3 | Python: MicroPython o sottoinsieme proprio? | «MicroPython vero» | porta `embed` v1.24.1 (MIT) in `third_party/micropython`, solo Plus |
| Q4 | Record: v12 su entrambe le schede? | «v12 solo sulla Plus» | il Pico 2 W scrive ancora la v11 (congelato) |

Stato nuovo **`AppStatus::Base`**: la modalità ha un controller registrato e si
apre come una Available, con un triangolo ambra nell'angolo dell'icona e il
piede «EXE: apri (base)»; il campo `missing` dice cosa manca alla parità.
**Available** resta riservato alle modalità con la parità completa. `isOpen()`
(Available o Base) sostituisce i controlli «Available» in launcher, AppManager,
eActivity (strisce) e test.

## 3. Fase 1: inizializzazione di tutte le modalità

`tests/apps_init_test.cpp` prende dal registro ogni modalità aperta della build
(12 sul Pico 2 W, 22 sulla Plus) e la prova nei casi C1–C7 con i controlli
K1–K6 di [ANALISI_FONTI §6](ANALISI_FONTI_v1.17.0.md#6-fase-1-casi-di-inizializzazione):
avvio a freddo, dopo SYSTEM › RESET, record v11 della v1.16.0 valido, corrotto
e troncato, solo SRAM, memoria quasi esaurita, microSD assente e presente; si
apre da MENU con il tasto diretto, disegna la schermata e la barra F1–F6,
accetta la sequenza tipica, esce con MENU (e con SHIFT ' / MENU dalla striscia
di eActivity), non lascia memoria allocata (allocazioni e byte vivi contati con
`operator new` instradato, tolleranza di 64 B per l'arrotondamento del TLSF),
conserva la sonda di stato dopo uscita, rientro e riavvio simulato. **2 921**
controlli sulla Plus, **1 556** sul Pico 2 W.

| # | Trovato | Correzione | Prova |
|---|---|---|---|
| F1 | **Conversion** non segnava Ans da salvare: dopo una conversione il risultato in Ans si perdeva con un riavvio prima di un altro salvataggio (errore condiviso, anche sul Pico 2 W) | `ctx.markDirty()` dopo `setAnswer` in `src/apps/conversion_app.cpp` | K6 di Conversion in `apps_init_test` (fallisce senza la correzione) |
| F2 | AI Chat senza rete mostra una finestra d'errore: è il comportamento voluto («rete non disponibile»), non un errore | nessuna; la sonda della modalità lo accetta | K3 di AI Chat |
| F3 | Memory e Symbols non hanno dati propri | la sonda verifica che il workspace resti identico | K6 |

Nessun'altra modalità della v1.16.0 ha mostrato perdite, crash o stato perso
nei sette casi. Le correzioni dell'harness (etichette costruite prima del
controllo che allocavano, allocazioni del test durante la memoria esaurita,
soglia dei pixel della barra) non sono errori del firmware.

## 4. Fase 2: le dieci modalità Base (solo Pimoroni Pico Plus 2 W)

Ogni modalità: controller sottile su `ModeShell` (`src/apps/<nome>_app.cpp`,
classe con pImpl dichiarata da `FXAI_BASE_APP`), nucleo puro testato sul PC
(servizio **ModeCore**, che permette a ogni modalità solo il proprio header più
`mode_codec.hpp` e `limits.hpp`), sezione propria del record v12
(`0x0100 + AppId`, scritta solo se lo stato non è quello iniziale), tipo di
file in MEMORY › FILES (`ui::TypedFileMenu`), operazioni lunghe in un
`step::Task` interrompibile con AC. Codice nuovo sempre dietro
`#if FXAI_BOARD_PICO_PLUS_2W` con il codice della v1.16.0 nell'`#else`.

| Modalità | Fatto (v1.17.0) | Manca alla parità (campo `missing`) | Sezione | File | Guida |
|---|---|---|---|---|---|
| Spreadsheet | celle A1…Z999 (26 000 con la PSRAM, 3 000 in SRAM), numeri/testi/formule, riferimenti relativi e `$` assoluti, intervalli, ricalcolo con ordinamento di Kahn a passi, Circular ERROR, COPY/CUT/PASTE/FILL/CLEAR con garanzia forte, colonna ↔ List | SEQ, SORT, CellIf/CellProd, formattazione condizionale, GRAPH e CALC dalle celle, CSV, memorie verso matrici (cap. 9) | `0x0103` | `.pcsheet` | [SPREADSHEET.md](../SPREADSHEET.md) |
| Dyna Graph | Y= con un coefficiente, Start/End/Pitch (VARS › DYNA), ≤ 100 fotogrammi, tre velocità, Cont/Stop, AC, V-Window di Graph | BUILT-IN, Locus, Dynamic Graph Memory, tabella (5-42) | `0x0105` | `.pcdyna` | [DYNA_GRAPH.md](../DYNA_GRAPH.md) |
| Recursion | an+1 e an+2 con a0/a1, tabella a passi con Σan, grafico, ragnatela | bn, cn, an = f(n), grafico di fase, convergenza (5-45) | `0x0107` | `.pcrecur` | [RECURSION.md](../RECURSION.md) |
| Conic Graphs | sei modelli cartesiani, due rami per l'iperbole, TRACE, G-SOLVE vertice/fuoco/centro/raggio/asintoti/eccentricità | forme polari e parametriche, direttrice, asse, latus rectum, intercette, ZOOM (5-50) | `0x0108` | `.pcconic` | [CONIC_GRAPHS.md](../CONIC_GRAPHS.md) |
| Financial | SMPL, CMPD con BGN/END (`calclib::cmpdSolve`), CASH (NPV, IRR, PBP, NFV), AMT (`amortizationAt`), CNVT, DAYS, →Ans | Cost/Sell/Margin, Depreciation, Bond, grafici dei flussi, anno di 360 giorni (cap. 7) | `0x010B` | `.pcfin` | [FINANCIAL.md](../FINANCIAL.md) |
| Program | elenco, NEW con il modello del dialetto, editor con la sintassi colorata, RUN sul nucleo F9 con input e AC, errori con riga/colonna e cursore, comandi grafici disegnati da Graph, liste con `fx::` | DEBUG, SEARCH, RENAME, password, libreria `fx` completa (cap. 8) | `0x010A` | `.pcprog`, `.c/.cpp/.cs` | [PROGRAM.md](../PROGRAM.md) |
| Link | trame con CRC-32 su USB CDC, Sender/Receiver a passi, ritrasmissione e Time Out, pacchetti del workspace e dei file, APPLY con conferma; `tools/picocalc_link.py` | più file selezionati, immagini dello schermo; il protocollo Casio non è supportato | `0x010D` | `.pclink` | [LINK.md](../LINK.md) |
| Geometry | punti, segmenti, rette, circonferenze, poligoni (≤ 500 oggetti); selezione, MOVE, MEASURE, ZOOM, PAN | Construct, trasformazioni, animazioni, testo ed etichette (cap. 14) | `0x0110` | `.pcgeo` | [GEOMETRY.md](../GEOMETRY.md) |
| Picture Plot | sfondo `.pcpict`, ≤ 999 punti numerati, List 1/2, regressione di Statistics sopra lo sfondo | fotogrammi, AXTRANS, Plot List, Y=f(x), Fade I/O (cap. 15) | `0x0111` | `.pcplot` | [PICTURE_PLOT.md](../PICTURE_PLOT.md) |
| Python | MicroPython v1.24.1 (interi grandi, `math`, `sys`, eccezioni), SHELL con blocchi e cronologia, script `.py` con l'editor di testo, `input()`, AC → KeyboardInterrupt, coroutine con pila propria, heap separato, `casioplot` | menu delle funzioni, CATALOG, debug, moduli `random`, `turtle`, `matplotl` (cap. 17) | `0x0114` | `.py` | [PYTHON.md](../PYTHON.md) |

Statistics è stata provata solo nella fase 1 (resta Available con le funzioni
della v1.16.0); E-CON4 resta External.

**Record v12** (solo Plus, [STORAGE.md](../STORAGE.md)): firma
`"<scheda> <versione>"` dopo il numero di versione e CRC-32 per sezione; una
sezione con il CRC sbagliato o non decodificabile viene isolata
(`damagedSections`, avviso «Data ERROR: dati danneggiati di …» all'avvio, la
modalità riparte dai valori iniziali). Migrazione dal record v11 della v1.16.0
provata con la fixture `tests/fixtures/workspace_v1_16_0.inc`, generata dai
sorgenti di `eb2338a`. Le strisce di eActivity delle modalità Base (Conics,
Recur, Dynamic Graph, Financial, SpreadSheet, Geometry, Picture Plot) usano le
parti `0x40 + AppId`.

**Python sul dispositivo.** L'interprete gira su una pila propria di 24 KB
(margine di 4 KB sotto il limite di MicroPython per i callback e gli IRQ) con
un cambio di contesto Cortex-M33 scritto in assembly (`pc_ctx_switch`: salva
r4–r11, lr, s16–s31 e cambia MSPLIM con la pila), così la guardia di stack
dell'RP2350 protegge anche la coroutine. L'hook della VM (`pc_py_poll` ogni 32
istruzioni) restituisce il controllo al firmware alla fine della fetta di tempo
(budget di 64 000 unità per tick), quindi tastiera e display restano vivi e AC
chiede un KeyboardInterrupt (`mp_sched_keyboard_interrupt`). Sul PC la stessa
coroutine usa `ucontext` con le annotazioni di fibra di ASan (pila 64 KB,
margine 16 KB per gli intercettori dei sanitizer). Heap: 512 KB nella PSRAM,
48 KB in SRAM senza PSRAM; heap e pila sono allocati solo mentre la modalità è
aperta.

## 5. Differenze inevitabili e scelte

| # | Voce | Motivo |
|---|---|---|
| D1 | Link non parla il protocollo Casio tra calcolatrici | la PicoCalc non ha la porta a 3 pin; protocollo proprio su USB CDC con uno strumento per il PC |
| D2 | Program usa C, C++ e C# interpretati invece del linguaggio dei programmi Casio | decisione dell'utente (v1.15.0, confermata in Q2) |
| D3 | formati dei file propri (`.pcsheet`, `.pcprog`, `.pcgeo`, …) | i formati Casio sono proprietari; nessun file Casio viene letto |
| D4 | Picture Plot usa sfondi `.pcpict` | come Graph ed eActivity dalla v1.16.0 |
| D5 | Python: `casioplot` minimo, niente `turtle`/`matplotl` | arrivano con la parità di Python (v1.29.0 nella proposta) |
| D6 | il ritorno alla v1.16.0 sulla Plus non legge il record v12 | limite noto dei ritorni indietro ([STORAGE.md](../STORAGE.md)) |

## 6. Test, fuzzing, simulatore

| Verifica | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | **61/61**: 58 suite C++ con **18 623** controlli (+3 965 sulla v1.16.0), `mem_pools_smoke`, `sd_interop_test`, `link_tool_test` (6 test Python) | **54/54**: 52 suite con **16 297** controlli (+1 734), `mem_pools_smoke`, `sd_interop_test` |
| Suite nuove | `apps_init_test` 2 921, `base_modes_test` 384, `base_flow_test` 112, `workspace_v117_test` 40, `link_tool_test` | `apps_init_test` 1 556 |
| Suite cambiate | `architecture_test` 2 324 (stato Base, ModeCore, congelamento), `typed_files_test` 450 (tipi della Plus), versione del record da `workspaceRecordVersion()` in sei suite, scrittura v11 esplicita in `workspace_v115_test` e `table_test`, strisce della Plus in `eactivity_test` | `architecture_test` 2 037; le altre identiche |
| ASan + UBSan (bloccante), TSan | 61/61, TSan superato | 54/54, TSan superato |
| Fuzzing (≥ 300 s per harness nuovo o toccato) | nuovi: `fuzz_sheet` 551 536 esecuzioni, `fuzz_link` 3 543 604, `fuzz_modes` 2 457 080, `fuzz_python` 5 615 993; toccati: `fuzz_workspace` 2 370 949, `fuzz_ui` 16 191 (sul codice finale dei controller), `fuzz_typedfile` 118 272 391, `fuzz_eactivity` 3 024 con il controller e 13 830 763 sui soli documenti; ognuno ≥ 300 s: nessun crash né artefatto sul codice finale (durante lo sviluppo `fuzz_modes` aveva trovato i NaN in Financial) | — |
| Simulatore (`tools/sim_compare.py`) | **137 identici**: 113 screenshot precedenti invariati, 4 rigenerati per cambiamenti voluti (sotto), 20 nuovi (128–147, `tests/sim/base_v117.txt`) | le stesse schermate (le modalità Base non esistono) |
| Relay AI | 15/15 | — |

Screenshot rigenerati: **19** (lo script apriva una modalità Planned per
mostrare «non disponibile»: ora mostra E-CON4, l'unica rimasta), **105**
(MEMORY: la dimensione del record v12 è diversa), **106** (MEMORY › FILES con
i tipi nuovi della Plus), **115** (menu delle strisce: le strisce della Plus
non sono più «non disponibili»).

Errori trovati durante lo sviluppo e corretti prima della pubblicazione:
**trovato dal fuzzing** (`fuzz_modes`): la sezione di Financial accettava NaN e
infiniti (i calcoli davano risultati non numerici) — ora la decodifica li
rifiuta e la sezione viene isolata; il caso è in
`tests/fixtures/fuzz/fuzz_modes_fin_nan.bin` e in `base_modes_test`.
**Trovato scrivendo FILE_FORMATS**: i file `.pcdyna` e `.pcrecur` salvavano
solo la sezione, senza Start/End/Pitch e senza Start/End/a0/a1 (che stanno
nelle variabili di modo della v1.13.0): ora i file li contengono, con
`dyna::encodeFile/decodeFile`, `rec::encodeFile/decodeFile` e
`fin::encodeFile/decodeFile` nei nuclei (solo dati validi, garanzia forte),
provati in `base_modes_test`, `base_flow_test` (con una prova a mutazione) e
`fuzz_modes`. Altri: con l'orologio simulato del PC un budget solo a tempo non
finiva mai la fetta di Python (budget a unità); AC durante una ricezione di
Link non chiudeva la schermata; sotto ASan la RecursionError superava la pila
della coroutine (margine del PC portato a 16 KB); le scelte dei moduli senza
testo e il grafico di `casioplot` chiesto dalla SHELL non si vedevano negli
screenshot.

## 7. Firmware e memoria

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Immagine | **2 625 472 B** = 62 % della partizione da 4 MB (+527 924 B sulla v1.16.0; budget 3 774 873 B = partizione meno il margine di 419 431 B: restano 1 149 401 B) | **1 944 228 B** = 92 % della partizione da 2 MB (+16 B: correzione condivisa di Conversion e stringa della versione); restano 120 156 B sotto il budget di 2 064 384 B |
| RAM statica | 310 712 B fino a `__end__` (+1 548 B); 312 760 B con `.heap`; heap SRAM 180 808 B | 305 236 B (invariata); heap SRAM 186 284 B |
| Oggetti esclusi nella mappa | 0 (tutte le modalità compilate) | 169 oggetti/simboli delle modalità della Plus, nessuno nell'immagine |
| Warning del compilatore | nessuno | nessuno |

Flash per modalità Base (mappa del linker della Plus, nucleo + controller):

| Modalità | Flash (B) |
|---|---:|
| Python + MicroPython (78 oggetti) | 135 631 (SRAM statica 9 B) |
| Link | 38 953 |
| Spreadsheet | 33 607 |
| Financial | 32 658 (più `calc_library`: `cmpdSolve`, `amortizationAt`) |
| Picture Plot | 32 024 |
| Geometry | 29 228 |
| Dyna Graph | 26 350 |
| Recursion | 25 975 |
| Program (solo modalità; l'interprete F9, `program_lang` 62 702 + `program_vm` 42 568, è contato sotto) | 22 975 |
| Conic Graphs | 21 483 |
| **Totale delle dieci modalità** | **398 884** |

Il resto della crescita della Plus è codice della v1.15.0 che il linker ora
tiene perché usato (nella v1.16.0 non lo chiamava nessuno) (interprete F9 105 270 B, Graph2D 4 580 B, Binding 3 899 B), il
record v12 e le codifiche, `TypedFileMenu`, la porta USB seriale.

Stack: frame più grandi del codice nuovo (`-fstack-usage`, Plus):
`FinancialApp` `fillPage` 976 B, `DynaGraphApp::key` 808 B,
`RecursionApp` `fillSet` 664 B, `PlotCanvas::drawString` 616 B; MicroPython
(sulla pila della coroutine) `pc_input` 264 B, `mp_obj_str_binary_op` 160 B,
`mp_execute_bytecode` 128 B per livello; il massimo del firmware resta
`SystemApp::enter` (2 016 B). Nessuna ricorsione nuova nel codice del progetto
(ricalcolo del foglio con Kahn, decodifiche e disegno iterativi); la ricorsione
degli script Python è limitata da `MICROPY_STACK_CHECK` sulla pila della
coroutine.

Dettagli e hash degli UF2: [BUILD_STATUS.md](../BUILD_STATUS.md).

## 8. Cancello

| Controllo | Esito |
|---|---|
| `git fetch origin main` | `main` fermo a `eb2338a`: fast-forward |
| `tests/run_host_tests.sh` (due profili) | 61/61 e 54/54 |
| `--sanitize --tsan` | 61/61 e 54/54, TSan superato su entrambi |
| Fuzzing ≥ 300 s per harness nuovo o toccato | 9 harness (4 nuovi, 5 toccati), ognuno ≥ 300 s: nessun crash |
| Simulatore | 113 identici, 4 rigenerati con il motivo, 20 nuovi |
| Firmware | due schede `--clean`, nessun warning; Plus 62 % (< 90 %); Pico 2 W 1 944 228 B (+16 B, solo la correzione condivisa) |
| Congelamento del Pico 2 W | `architecture_test` («Pico 2 W frozen at v1.16.0», nessuna modalità Base), stesse modalità e stesse schermate; unica differenza la correzione condivisa di Conversion |
| Relay | 15/15 |
| Documentazione | ANALISI_FONTI e SVILUPPO v1.17.0, HARDWARE_TEST_v1.17.0, dieci guide delle modalità, FEATURE_MATRIX, APPLICATIONS_ARCHITECTURE, FILE_FORMATS, STORAGE, TESTING, BUILD_STATUS, CHANGELOG, ROADMAP, CONTINUE, README, docs/README, releases/v1.17.0, VERSION |
