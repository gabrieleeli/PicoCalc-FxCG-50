# Sviluppo della v1.18.0 — Graph e Table alla parità completa

Obiettivo della versione: portare **Graph** e **Table** alla **parità completa**
con la fx-CG50 (manuale v3.60, capitolo 5 pagine 5-1…5-41 e 5-52…5-63, Setup
1-35…1-37) su **entrambe le schede**, così da dichiararle **Available**. Dyna
Graph, Recursion e Conic Graphs sono modalità separate e fuori ambito.
Statistics e Distribution come Base passano alla v1.19.0
([ROADMAP.md](../ROADMAP.md)). Analisi, audit voce per voce e domande scritte
prima del codice: [ANALISI_FONTI_v1.18.0.md](ANALISI_FONTI_v1.18.0.md). Tabella
di parità finale: [GRAPH_PARITY.md](../GRAPH_PARITY.md). **Nessuna prova su
hardware**: [HARDWARE_TEST_v1.18.0.md](../HARDWARE_TEST_v1.18.0.md)
(HW-246…HW-265).

Il manuale è servito solo come descrizione del comportamento: nessun codice,
grafica, font, testo o formato di file Casio (le immagini restano nel formato
proprio `.pcpict`).

## 1. Git

| Voce | Valore |
|---|---|
| Partenza | `main` = `aa2923c` (v1.17.0) |
| Branch di lavoro | `claude/graph-table-parity-v118-1is1v6` |
| Pubblicazione | un solo commit «PicoCalc FxCG-50 v1.18.0: Graph e Table alla parità completa (…)» |

## 2. Decisioni (fase 0)

Risposte dell'utente alle domande di
[ANALISI_FONTI §13](ANALISI_FONTI_v1.18.0.md#13-domande-prima-del-codice):

| # | Domanda | Risposta | Conseguenza |
|---|---|---|---|
| D1 | Disposizione del Dual Screen su 320×320 | sopra/sotto | principale sopra (righe 18…157), linea di separazione (158–159), sotto-schermo o tabella sotto (160…299); F6 SOTTO/SOPRA nella V-Window, indicatori «S» (sotto) ed «E» (entrambi) nella lista (DI-1, DI-2) |
| D2 | ZOOM in G+G | il risultato va nel sotto-schermo | lo ZOOM parte dalla V-Window del principale e scrive quella del sotto-schermo, come «sotto-schermo prodotto con lo zoom del principale» (5-23); il principale resta sempre l'unico attivo |
| D3 | Immagine a due livelli | realizzarla | `.pcpict` versione 2: livello dello sfondo (assi, griglia, etichette, sfondo con Fade) e livello dei grafici; l'immagine composta resta per i lettori della versione 1 |
| D4 | LISTMEM fino a 999 anche sul Pico 2 W | sì | `kListMaxLength` = 999 su entrambe le schede; il totale di tutte le liste del Pico 2 W resta 1 530 elementi (nessuna RAM, nessun byte in più nel record) (DI-9) |

## 3. Parità finale

| Stato | Graph (G01–G83) | Table (T01–T13) | Setup (S01–S18) | Totale | Prima (fase 0) |
|---|---:|---:|---:|---:|---:|
| TESTED ON SIMULATOR | 32 | 6 | 4 | **42** | 24 |
| TESTED ON HOST | 51 | 6 | 14 | **71** | 50 |
| FUORI AMBITO | 0 | 1 | 0 | 1 | 1 |
| IMPLEMENTED senza test | 0 | 0 | 0 | **0** | 2 |
| PARZIALE | 0 | 0 | 0 | **0** | 16 |
| Errore (diverso dal manuale) | 0 | 0 | 0 | **0** | 6 |
| NOT STARTED | 0 | 0 | 0 | **0** | 15 |
| **Totale** | **83** | **13** | **18** | **114** | 114 |

Graph e Table sono **Available** sulla Pimoroni Pico Plus 2 W e sul Raspberry
Pi Pico 2 W (`architecture_test` lo verifica nel registro: stato Available,
sorgenti `graph_view.cpp` e `graph_dual.cpp` compilati su entrambe). Nessuna
voce è TESTED ON HARDWARE.

### 3.1 Le 39 voci chiuse

| Voci | Prima | Cosa è stato fatto | Prova |
|---|---|---|---|
| S10, G41 | NOT STARTED | Setup di Graph «Dual Screen» G+G / GtoT / Off | graph_v118_test G41–G48, scr. 148 |
| G42 | NOT STARTED | seconda V-Window del sotto-schermo, F6 SOTTO/SOPRA nella V-Window, salvata nel workspace e nei progetti | graph_v118_test, scr. 149 |
| G43, G44 | NOT STARTED | OPTN › COPY (grafici scelti → «E») e SWAP (lati e V-Window scambiati) | graph_v118_test, scr. 150 |
| G45, G46 | NOT STARTED | indicatori S/E nella lista, SELECT li toglie; FORMAT rifiutato sulle righe «S» | graph_v118_test, scr. 151 |
| G47 | NOT STARTED | TRACE, SKETCH e G-SOLVE solo sui grafici del principale; ZOOM nel sotto-schermo (D2) | graph_v118_test, scr. 152 |
| G48, G03 | NOT STARTED / PARZIALE | AC dal grafico (anche doppio) torna alla lista | graph_v118_test G02/G03 |
| G31, G39, G53, G57 | PARZIALE / NOT STARTED | PAN, PICTURE › STORE e SAVE•AS, incolla e MODIFY danno «Argument ERROR» con il Dual Screen; PAN anche sul grafico di GPH-CON | graph_v118_test, table_v118_test |
| G72, S03 | NOT STARTED / PARZIALE | Graph to Table: TRACE ed EXE memorizzano il punto (≤ 999 righe, un solo tipo), OPTN › CHANGE attiva la tabella, R•DEL, DEL•ALL, LISTMEM; colonne delle derivate con Derivative On | graph_v118_test G72, scr. 153, 154 |
| S17, T12 | NOT STARTED | Setup di Table «Dual Screen» T+G / Off: tabella sotto, grafico di GPH-CON/GPH-PLT sopra, la riga segue il puntatore del TRACE, CHANGE e AC | table_v118_test T12, scr. 159, 160 |
| G08 | NOT STARTED (inserimento) | OPTN › ANGLE (°, ʳ, ᵍ) negli editor di Graph e di Table | graph_v118_test G08, table_v118_test, scr. 158 |
| G65, G69 | PARZIALE | coordinate anche accanto al puntatore (stile dei punti dello SKETCH); «:» per le disequazioni strette | graph_v118_test G65/G69, scr. 155 |
| G66, G32 | IMPLEMENTED senza test | linee ausiliarie (SHIFT ↓/↑) e scorrimento con i cursori: ora provati | graph_v118_test G66, G32 |
| G67 | PARZIALE | il tasto della variabile (x/θ/T) apre la finestra del valore | graph_v118_test G67 |
| G61, G62 | Errore (E1) / PARZIALE | Tangent e Norm con Sketch Line e Plot/LineCol; OPTN (FORMAT) durante l'operazione per tutti gli strumenti dello SKETCH | graph_v118_test G61/G62, scr. 156 |
| G33, G34, G38 | PARZIALE | cartelle di un livello sotto `PICOCALC/PICT`: NEW F, apertura, SAVE•AS e OPEN nella cartella, anche per Background e BGV-WIN | graph_v118_test G38, scr. 157 |
| G35 | PARZIALE | Fade I/O con il cursore (←/→ a passi del 5 %, cifre), salvato con l'immagine di sfondo | graph_v118_test G35 |
| G36 | PARZIALE (D3) | immagini a due livelli (`.pcpict` v2), Fade solo sul livello dello sfondo | graph_v118_test G36, workspace_v118_test |
| G50 | NOT STARTED | `Graph Y=sin`, `Graph r=cos` e le altre funzioni predefinite senza variabile | graph_v118_test G50 |
| G02 | Errore (E5) | EXE sulla lista disegna, → modifica | graph_v118_test G02 |
| G14 | Errore (E3) | FORMAT dal grafico: i grafici incollati si scelgono, le varianti delle sovrapposizioni no | graph_v118_test G14 |
| G51 | Errore (E2) | il primo grafico di una sovrapposizione o di una lista ciano/gialla prende il colore della riga | graph_v118_test G51, graph_engine_test |
| T03 | Errore (E6) | Table: STYLE cambia lo stile della linea, OPTN › FORMAT stile e colore | table_v118_test T03, table_test |
| T10 | PARZIALE | AC dopo GPH-CON torna alla tabella | table_v118_test |
| T11 | Errore (E4) | TRACE dopo GPH-CON passa da una riga della tabella all'altra (anche con Variable = LIST o righe modificate) | table_v118_test |
| T09 | PARZIALE sul Pico 2 W (D4) | LISTMEM di 999 valori su entrambe le schede | table_v118_test T09, table_test, limits_smoke |

Voci presenti a cui mancava solo il test, ora provate: G12 (cifre 1–9 nei menu
del FORMAT), G52 (colori ciano/giallo), G79, G80 (∫dx ROOT/INTSECT con 21
radici o più).

### 3.2 Errori corretti

| # | Errore | Correzione | Test che falliva prima |
|---|---|---|---|
| E1 | Tangent e Norm sempre blu e Normal | stile e colore dal Setup e dal FORMAT dell'operazione | graph_v118_test G61 |
| E2 | primo grafico ciano/giallo nelle sovrapposizioni e nelle liste | `GraphEngine` dà al primo grafico il colore della riga | graph_v118_test G51 |
| E3 | FORMAT: incollati non selezionabili, varianti sì | selezione dei grafici incollati con stile proprio, varianti escluse | graph_v118_test G14 |
| E4 | TRACE di GPH-CON su Start/Step/End | il puntatore segue le righe della tabella | table_v118_test T11 |
| E5 | EXE sulla lista apre l'editor | EXE disegna, → modifica | graph_v118_test G02 |
| E6 | STYLE di Table cambiava il colore, FORMAT assente | STYLE = stili di linea, FORMAT = stile e colore | table_v118_test T03 |

## 4. Differenze inevitabili

Dettagli e motivi in [GRAPH_PARITY.md §10](../GRAPH_PARITY.md#10-differenze-documentate).

| # | Differenza | Motivo |
|---|---|---|
| DI-1 | Dual Screen sopra/sotto (320×140 + 320×140) invece che affiancato | schermo quadrato: due riquadri affiancati sarebbero 159×282 (D1) |
| DI-2 | F6 SOTTO/SOPRA e indicatori S/E | conseguenza di DI-1 |
| DI-3 | `.pcpict` v2 invece di `g3p` | formato Casio proprietario; il comportamento è quello del manuale |
| DI-4 | cartelle di un livello sotto `PICOCALC/PICT`, create dal browser (NEW F) | sulla fx-CG50 le cartelle si creano in MEMORY |
| DI-5 | «dY/dX=» nella riga in basso | la riga in alto è la barra di stato della PicoCalc |
| DI-6 | nessuna riga «[EXE]: …» durante il TRACE | la barra di stato della PicoCalc ha un altro uso |
| DI-7 | «Dual Screen» nel Setup di Graph dove la v1.17.0 aveva «Fade I/O» | Fade I/O è il cursore di OPTN (5-12); l'ordine delle prime righe resta |
| DI-8 | DEL al posto di ALPHA DEL | tastiera della PicoCalc |
| DI-9 | sul Pico 2 W tutte le liste insieme restano 1 530 elementi | record del workspace da 59,75 KB (D4) |

## 5. Differenze fra le schede

**Nessuna.** Tutto è andato anche sul Raspberry Pi Pico 2 W: Dual Screen G+G,
GtoT e T+G, voci piccole, correzioni E1–E6, immagini a due livelli, cartelle,
LISTMEM a 999. Una sola implementazione (servizio GraphCore: `graph_view.cpp`,
`graph_dual.cpp`), nessun codice duplicato per scheda. L'unico limite diverso
è quello già esistente del totale delle liste (DI-9).

## 6. Architettura e persistenza

| Parte | Contenuto |
|---|---|
| `include/graph_view.hpp`, `src/apps/graph_view.cpp` (nuovi, GraphCore, entrambe le schede) | riquadri `Pane` (Full/Main/Sub), viewport per riquadro, cornice (griglia, assi, scala, etichette), penne e stili, curve della tabella, disegno della tabella condiviso da Table e dal GtoT; codice spostato da `graph_plot.cpp` e `table_app.cpp` senza cambiare un pixel (137 screenshot identici) |
| `src/apps/graph_dual.cpp` (nuovo, `-Os` sul Pico 2 W) | regole del Dual Screen (errori, destinazione dello ZOOM), colonne e tasti della tabella del GtoT, memorizzazione dei punti |
| `GraphState::dual` (`GraphDual`) | modo, V-Window del sotto-schermo, lato di ogni riga (Main/Sub/Both), tipo e valori della tabella del GtoT (≤ 999); `TableRange::dual` per T+G |
| Record del workspace | sezioni **0x0104** (Graph) e **0x0106** (Table) su entrambe le schede, scritte solo se lo stato non è quello iniziale: il record di chi non usa il Dual Screen resta byte per byte quello della v1.17.0. Il Pico 2 W resta sul record **v11**, la Plus sul v12 (sezione danneggiata isolata) |
| Strisce di eActivity | parte **Dual** (8), aggiunta solo se serve; un documento senza Dual Screen si apre ancora con la v1.17.0 |
| `.pcgraph` | chiavi `dual=`, `subview=`, `side=`, `tabledual=` scritte solo se diverse dal predefinito; la v1.17.0 le ignora |
| `.pcpict` | versione 2 con due livelli (flag 3) solo quando l'immagine li ha; altrimenti i byte della versione 1 invariati |
| Ritorno alla v1.17.0 | Dual Screen Off e liste ≤ 255: il record si legge; la v1.17.0 conserva le sezioni 0x0104/0x0106 che non conosce. Sul Pico 2 W un record con una lista di più di 255 elementi non si legge con la v1.17.0 (limite noto, [STORAGE.md](../STORAGE.md)) |

## 7. Pico 2 W: eccezione al congelamento

Regola (anche in [CONTINUE.md](../../CONTINUE.md) e [ROADMAP.md](../ROADMAP.md)):
il Pico 2 W resta congelato alla v1.17.0 per tutto, **con l'unica eccezione
verificata dei file di Graph/Table della v1.18.0**. `architecture_test` legge
`tests/fixtures/pico2w_frozen_v1_17_0.txt` (FNV-1a a 64 bit dei byte di ognuno
dei 165 file compilati per il Pico 2 W e degli header alla v1.17.0, generato da
`tools/freeze_manifest.py` sul commit `aa2923c`) e fallisce se un file
compilato sul Pico 2 W cambia fuori dall'insieme `kGraphTableV118` o se ne
compare uno nuovo non dichiarato (verificato con una modifica di prova a
`statistics_app.cpp`). Il build del Pico 2 W ha un margine bloccante di
81 920 B dai 2 MB: immagine ≤ 2 015 232 B, cioè la partizione da 2 064 384 B
meno 48 KB di riserva. Nessuna modalità nuova aperta sul Pico 2 W.

| Regola | Esito |
|---|---|
| Immagine ≤ 2 064 384 B con ≥ 48 KB di margine | **1 972 608 B**: margine 91 776 B dalla partizione, 42 624 B oltre la riserva |
| RAM statica ≤ +8 KB | **+120 B** |
| `-Os` per i sorgenti nuovi | `graph_dual.cpp` in `FXAI_PICO2W_SIZE_SOURCES` (`graph_view.cpp` è codice spostato, con l'ottimizzazione dei file da cui viene) |
| Buffer grandi solo sull'heap con la modalità aperta | curve del sotto-schermo, tabella del GtoT, livelli dell'immagine: membri di `GraphApp`/`TableApp`, liberati all'uscita; nessun buffer statico nuovo |
| Una sola implementazione | sì, nessun `#if` di scheda nel codice nuovo |

## 8. Test, fuzzing, simulatore

| Verifica | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | **64/64**: 61 suite C++ con **19 594** controlli (+971 sulla v1.17.0), `mem_pools_smoke`, `sd_interop_test`, `link_tool_test` | **57/57**: 55 suite con **17 352** controlli (+1 055), `mem_pools_smoke`, `sd_interop_test` |
| Suite nuove | `graph_v118_test` 514, `table_v118_test` 42, `workspace_v118_test` 35 | 514, 42, 34 |
| Suite cresciute | `architecture_test` 2 703 (manifesto del congelamento), `table_test` 172, `lists_csv_test` 971 | 2 500, 172, 969; `limits_smoke` 36 |
| AddressSanitizer + UBSan bloccante | 64/64 | 57/57 |
| ThreadSanitizer (`mem_pools_tsan`) | superato | superato |
| Relay AI | 15/15 | — |

Errore trovato durante il cancello e corretto prima del commit, con il suo
test in `graph_v118_test`: uscendo da Graph (`GraphApp::leave`) le curve del
sotto-schermo, la tabella del GtoT e lo stato «tabella attiva» restavano in
memoria; ora sono liberati e al rientro è attivo il grafico. Dopo la
correzione sono stati ripetuti test, sanitizer, fuzzing di `fuzz_dual` e
`fuzz_ui`, simulatore e firmware.

Test esistenti adattati solo a cambiamenti voluti (nessuna verifica tolta):
`graph_app_test` (Fade I/O è un cursore che si chiude con EXIT; SAVE•AS con il
browser delle cartelle), `table_test` (STYLE con gli stili, FORMAT del colore,
LISTMEM 999), `limits_smoke` (999 per lista, 1 530 in tutto sul Pico 2 W),
`lists_csv_test` e `graph_cmd_v112_test` (liste da 999 sul Pico 2 W),
`workspace_v115_test` (due codec di sezione in più).

Fuzzing (libFuzzer + ASan + UBSan, `tools/fuzz/run_fuzz.sh` con `FUZZ_ONLY`,
≥ 300 s per ogni harness nuovo o toccato, nessun crash né timeout, cartelle
degli artefatti vuote):

| Harness | Profilo Plus | Profilo Pico 2 W |
|---|---|---|
| `fuzz_dual` (**nuovo**: G+G, GtoT e T+G con tasti, V-Window e tabelle casuali; curve dentro il proprio riquadro, tabella ≤ 999 righe finite, andata e ritorno di record, parti delle strisce e chiavi `.pcgraph`) | 7 774 esecuzioni in 320 s (Graph e Table interi a ogni input) | 7 973 in 301 s |
| `fuzz_workspace` (sezioni 0x0104/0x0106, parte Dual) | 2 322 626 in 302 s | 1 851 614 in 303 s |
| `fuzz_ui` (Graph e Table dai tasti) | 3 165 in 307 s sul codice finale (20 139 in 321 s prima della correzione di `GraphApp::leave`) | — |
| `fuzz_pcgraph` (chiavi nuove dei progetti) | 3 849 422 in 304 s | — |
| `fuzz_eactivity` (strisce con la parte Dual) | 3 790 con il controller in 311 s; 5 813 979 sui soli documenti in 306 s | — |

Simulatore (`tools/sim_compare.py`, pixel per pixel): **150 screenshot**, i
**137 precedenti identici** (nessuno rigenerato) e **13 nuovi** (148–160,
`tests/sim/graph_v118.txt`): Setup «Dual Screen», V-Window del sotto-schermo,
COPY, lista con gli indicatori, ZOOM nel sotto-schermo, TRACE e tabella del
GtoT, etichetta del TRACE, FORMAT dello SKETCH, cartelle delle immagini, menu
ANGLE, T+G e TRACE in T+G.

## 9. Firmware e memoria

Build pulite (`scripts/build.sh --clean`), Pico SDK 2.2.0, nessun warning;
`picotool info -a`: versione `1.18.0 (pico_plus_2w)` / `1.18.0 (pico2w)`.

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Immagine | **2 655 532 B** = 63 % di 4 MB (+30 060 B; budget < 90 %: 3 774 873 B) | **1 972 608 B** = 94 % di 2 MB (+28 380 B; budget 2 015 232 B, restano 42 624 B) |
| RAM statica | 310 832 B fino a `__end__` (+120 B); 312 880 B con `.heap`; heap SRAM 180 688 B | 305 356 B (+120 B); 307 404 B con `.heap`; heap SRAM 186 164 B |
| Oggetti esclusi nella mappa | 0 | 169, nessuno nell'immagine |
| Frame di stack più grandi | `SystemApp::enter` 2 016 B (invariato); nel codice toccato `openProject` 1 832 B, `openSetup` 1 504 B, `drawPaneLayers` 632 B | `SystemApp::enter` 1 968 B; `openProject` 1 856 B, `newProject` 1 824 B |
| Warning | nessuno | nessuno |

Byte per modulo nell'immagine del Pico 2 W (v1.17.0 → v1.18.0): `graph_plot`
69 593 → 73 195, `graph_app` 37 064 → 39 674, `table_app` 29 139 → 28 065
(il disegno della tabella è passato in `graph_view`), `graph_files` 15 373 →
16 846, `graph_file` 28 764 → 30 174, `graph_commands` 26 049 → 29 818,
`workspace` 55 795 → 58 428, `graph_engine` 32 294 → 32 378; nuovi
`graph_view` 9 196 e `graph_dual` 2 802 (sulla Plus con `-O2` 3 160); `pict`
6 651. La stima della fase 0 ([ANALISI §10](ANALISI_FONTI_v1.18.0.md#10-stime-di-flash-e-ram)) era 35–55 KB sul Pico 2 W e +0,5–1,5 KB di RAM statica: misurati +28 380 B e +120 B.

Dettagli e hash degli UF2: [BUILD_STATUS.md](../BUILD_STATUS.md).

## 10. Cancello

| Controllo | Esito |
|---|---|
| `tests/run_host_tests.sh` (due profili) | 64/64 e 57/57 |
| `--sanitize --tsan` | 64/64 e 57/57 con ASan + UBSan bloccante; `mem_pools_tsan` superato su entrambi |
| Fuzzing ≥ 300 s per harness nuovo o toccato | 6 harness sul profilo Plus (1 nuovo, 5 toccati) più `fuzz_dual` e `fuzz_workspace` sul Pico 2 W: nessun crash |
| Simulatore | 137 identici, 13 nuovi |
| Firmware | due schede `--clean`, nessun warning; Plus 63 % (< 90 %); Pico 2 W 1 972 608 B ≤ 2 015 232 B; RAM statica +120 B |
| Congelamento del Pico 2 W | `architecture_test`: manifesto della v1.17.0 rispettato fuori da Graph/Table, margine di flash bloccante nel build |
| Relay | 15/15 |
| Documentazione | ANALISI_FONTI e SVILUPPO v1.18.0, HARDWARE_TEST_v1.18.0 (HW-246…HW-265), GRAPH_PARITY (riscritto), TABLE, PCGRAPH_FORMAT, STORAGE, APPLICATIONS_ARCHITECTURE, FEATURE_MATRIX, TESTING, BUILD_STATUS, CHANGELOG, ROADMAP, CONTINUE, README, docs/README, releases/v1.18.0, VERSION |
