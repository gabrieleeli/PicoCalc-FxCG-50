# Sviluppo della v1.18.0 — Statistics alla parità, Distribution Base

Obiettivo della versione: **Statistics** (capitolo 6 del manuale fx-CG50) alla
**parità completa** sulla Pimoroni Pico Plus 2 W e **Distribution** (capitolo
18) nel registro come modalità **Base**. Analisi, tabella di parità e
decisioni scritte prima del codice: [ANALISI_FONTI_v1.18.0.md](ANALISI_FONTI_v1.18.0.md).
Il **Raspberry Pi Pico 2 W resta congelato** alla v1.16.0. **Nessuna prova su
hardware**: [HARDWARE_TEST_v1.18.0.md](../HARDWARE_TEST_v1.18.0.md) (HW-246…HW-271).

## 1. Git

| Voce | Valore |
|---|---|
| Partenza | `main` = `aa2923c` (v1.17.0); `git fetch origin main` all'inizio e prima della pubblicazione |
| Branch di lavoro | `claude/eager-wozniak-bg3tqx` (commit intermedi: analisi, nucleo, controller e Distribution, Run-Matrix e test, screenshot, fuzzing e documentazione) |
| Pubblicazione | un solo commit su `main` in fast-forward, «PicoCalc FxCG-50 v1.18.0: …»; tag e release dal workflow |

## 2. Decisioni

Le domande aperte della fase 0 hanno le risposte scelte in
[ANALISI_FONTI §7](ANALISI_FONTI_v1.18.0.md#7-domande-aperte-e-risposte-scelte)
(Q1–Q16: DefG come ripristino dello StatGraph, quartili OnData, NPPlot di
Hazen, N-Dist con σx, Sin in radianti con COPY in ʳ, ANOVA a due vie
bilanciata, colori per lista, formula tolta dalla modifica a mano, nessun file
per Distribution, tasto O, Ans solo dalla DIST, Save Res in colonna, MSe del
manuale, x̂ di Sin non definito, valori iniziali degli StatGraph, VARS
dell'ultimo calcolo). Durante lo sviluppo si sono aggiunte:

| # | Decisione | Motivo |
|---|---|---|
| S1 | Dopo un errore di un'azione dell'editor la barra torna alla prima pagina (come al ritorno da un'altra schermata) | comportamento prevedibile; prima i tasti seguenti finivano nel sottomenu rimasto aperto |
| S2 | Nella finestra Start/Width dell'istogramma EXE su Start passa a Width, EXE su Width disegna | come la finestra della calcolatrice |
| S3 | «p̂» disegnato con un circonflesso combinante (U+0302) dal renderer solo sulla Plus | il font non ha p̂; i dati del font, condivisi col Pico 2 W, non cambiano |
| S4 | Coda destra di Inverse Normal e z critico calcolati per simmetria (μ − σ·z della coda sinistra) | con aree minuscole 1 − Area vale 1 in double (trovato dal fuzzing) |
| S5 | Un risultato infinito o NaN del nucleo (dati enormi, gradi di libertà enormi) è un Math ERROR | come l'overflow della calcolatrice (trovato dal fuzzing) |
| S6 | Le eccezioni generiche del motore numerico dentro la DIST diventano «Math ERROR: …» | messaggi della fx-CG50 anziché testi interni |

## 3. Tabella di parità finale (capitolo 6)

Tutte le voci del §3 dell'analisi sono **presenti**, ognuna con il suo test;
le differenze inevitabili sono al §5. Test: `statistics_flow_test` (SF, un caso
per voce, controller dal firmware completo), `statistics_test` (ST, nucleo
contro scipy/numpy/statsmodels), `apps_flow_test`, `lists_csv_test`.

| Voci | Stato | Test |
|---|---|---|
| E01 barra a tre pagine; E02/E03 SRT-A/SRT-D su più liste; E04 DEL; E05 DEL-ALL con conferma; E06 INS; E07 ADD; E08 JUMP | fatto | SF E01–E08 |
| E09 riga SUB e Sub Name; E10 colori delle liste; E11 formule nella testa (ricalcolo, Circular ERROR, modifica a mano, record); E12 List File dall'editor; E13 CSV dalla barra e da OPTN; E14/E15 celle e scorrimento | fatto | SF E09–E13, `lists_csv_test` |
| G01 menu GRAPH; G02 SET con le voci per tipo e DefG; G03 SELECT e DRAW di tre grafici | fatto | SF G01–G03, R16 |
| G04 Scatter con frequenze e marcatori; G05 xyLine; G06 NPPlot; G07 Histogram con Start/Width e classi; G08 MedBox con outlier; G09 N-Dist; G10 Broken; G11 Bar (Length/Horiz); G12 Pie con % Sto List e dati negativi; G13 regressioni come tipo | fatto | SF G04–G13, ST (NPP, N-Dist, istogramma, baffi, torta contro il calcolo indipendente) |
| G14 Stat Wind Auto/Manual; G15 Q1Q3 Type; G16 TRACE; G17 CALC dal grafico e DRAW della curva; G18 Graph Color e Color Link; G19 V-Window | fatto | SF G14–G19, ST (quartili) |
| R01–R12 le dodici regressioni (ax+b, a+bx, Med-Med con PTS, X², X³, X⁴, Log, ae^bx, ab^x, Power, Sin, Logistic) | fatto | ST contro `linregress`, `polyfit`, `least_squares`; SF R01, R02, R07–R12 |
| R13 righe per tipo; R14 Resid List; R15 COPY in Yn (formula valutata = ŷ, Sin anche in Deg); R16 DefG; R17 x̂/ŷ dei 12 tipi in Run-Matrix; R18 frequenze come dati ripetuti | fatto | SF, ST |
| C01 menu CALC e REG; C02 SET di CALC; C03 1-VAR con Mod:n e frequenze; C04 2-VAR con frequenze; C05 VARS › STAT dopo il riavvio | fatto | SF C01–C05, ST |
| T01–T12 Z 1-S/2-S/1-P/2-P, t 1-S/2-S (Pooled On/Off)/REG, χ² GOF (CNTRB) e 2-way (Mat attesa), F, ANOVA a una e due vie (step::Task, AC, grafico d'interazione); T13 DRAW; T14 Save Res e VARS RESULT | fatto | ST contro scipy/statsmodels; SF T01–T14 |
| I01–I06 intervalli Z e t | fatto | ST, SF |
| D01–D08 le 24 funzioni; D09 List (ListAns, Save Res, U.List); D10 DRAW; D11 step::Task con AC; D12 Ans | fatto | ST contro `scipy.stats`; SF D01–D12 |
| U01 SET UP; U02 sezione `0x0101` (andata e ritorno, danneggiata isolata, record v11 senza sezione); A01 VARS dell'ultimo calcolo; A02 Ans e ListAns in Run-Matrix | fatto | SF U01–U02, A01–A02; ST (codifica) |

## 4. Cosa c'è nel codice

| Parte | File | Note |
|---|---|---|
| Nucleo di Statistics | `include/stat_engine.hpp`, `src/stat_engine.cpp` | dati con frequenze, quartili Std/OnData, 12 regressioni (QR di Householder con colonne scalate per i polinomi; Sin con griglia di 480 frequenze e Levenberg–Marquardt, Logistic con sette avvii e LM; `FitTask` a passi), residui, x̂, test, intervalli, ANOVA (`AnovaTask`), modelli dei grafici, 90 variabili di VARS, stato `PlusState` e codifica della sezione `0x0101` |
| Nucleo delle distribuzioni | `include/dist_core.hpp`, `src/dist_core.cpp` | tabella delle 24 funzioni, `compute` sulle funzioni di `calc_library`, `ListTask`, modello del grafico per la DIST e per il DRAW dei test |
| Controller di Statistics | `src/apps/statistics_plus_app.cpp`, `src/apps/stat_plot.cpp` | `StatisticsApp` pImpl sulla Plus (vista solo con la modalità aperta), `ModeShell` per barre, domande e conferme, `step::Runner` per i lavori lunghi; `statistics_app.cpp` tiene la v1.16.0 nell'`#else` |
| Distribution | `include/distribution.hpp`, `src/distribution.cpp`, `src/apps/distribution_app.cpp` | nucleo della modalità (stato, sezione `0x0117`) e controller Base |
| Comuni (solo blocchi `#if` della Plus) | `expression.cpp` (nomi di VARS, x̂/ŷ dal nucleo), `calc_library.cpp` (nomi mostrati), `run_matrix_app.cpp` (sottomenu di VARS › STAT), `workspace.cpp` e `workspace_sections.cpp` (stato e sezioni), `app_registry.cpp`, `firmware.cpp`, `launcher.cpp` (icona), `widgets.cpp` (grafico delle distribuzioni), `font_renderer.cpp` (p̂) | sul Pico 2 W nessun byte di codice cambia |
| Strumenti | `tools/stat_reference.py`, `tests/fixtures/stat_reference.inc`, `tools/fuzz/fuzz_stats.cpp`, `tests/sim/statistics_v118.txt` | riferimenti indipendenti (`--check` nel cancello), fuzzing, screenshot |

## 5. Differenze inevitabili

Come previsto ([ANALISI_FONTI §5](ANALISI_FONTI_v1.18.0.md#5-differenze-inevitabili-previste)):
grafica e colori propri, tasti della PicoCalc, terza pagina dell'editor,
frequenze intere, metodo iterativo proprio per Sin e Logistic, ANOVA a due vie
bilanciata, striscia «Stat Graph» di eActivity con le sole liste. Nessuna
differenza nuova emersa durante lo sviluppo.

## 6. Errori trovati durante lo sviluppo

| # | Trovato da | Errore | Correzione | Prova |
|---|---|---|---|---|
| F1 | `fuzz_stats` | ANOVA (e somme di 1/2-Var, regressioni, test) con dati enormi: risultati infiniti mostrati come numeri | `finiteResult`: Math ERROR come l'overflow della calcolatrice | `statistics_test`, `tests/fixtures/fuzz/fuzz_stats_anova_overflow.bin` |
| F2 | `fuzz_stats` | Inverse Normal coda destra con Area = 1E−300: 1 − Area = 1 e un'eccezione generica | coda destra per simmetria; eccezioni generiche → Math ERROR | `statistics_test`, `fuzz_stats_invn_tiny_area.bin` |
| F3 | `fuzz_stats` | F C.D con d:df = 1E300: p = NaN accettato | risultato principale sempre finito | `statistics_test`, `fuzz_stats_fcd_nan.bin` |
| F4 | `statistics_flow_test` | cambiando Data in un modulo appena aperto i campi non cambiavano | il modulo ricorda Data e How Many con cui è stato costruito | SF D09, T01 |
| F5 | `statistics_flow_test` | dopo un errore di GRAPH la barra restava nel sottomenu | S1 | SF G12, R16 |
| F6 | simulatore | «p̂» disegnato «p?» | S3 | screenshot 84 |

## 7. Test, fuzzing, simulatore

| Verifica | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | **62/62**: 59 suite C++ con **20 063** controlli (+1 440 sulla v1.17.0; `mem_pools_smoke` escluso dal conteggio come nelle versioni precedenti), `mem_pools_smoke`, `sd_interop_test`, `link_tool_test` | **54/54**: 52 suite con **16 343** controlli (+46, solo `architecture_test`), `mem_pools_smoke`, `sd_interop_test` |
| Suite nuove | `statistics_flow_test` 174 | — |
| Suite cambiate | `statistics_test` 957 (+894: nucleo della Plus contro 483 riferimenti), `apps_init_test` 3 033 (+112: Distribution, sequenza di Statistics della Plus), `base_modes_test` 496 (+112), `base_flow_test` 129 (+17), `architecture_test` 2 454 (+130), `apps_flow_test`, `lists_csv_test`, `run_matrix_v112_test`, `workspace_v115_test` (barre e VARS della Plus, sezioni nuove) | `architecture_test` 2 083 (+46: 23 modalità); le altre identiche |
| ASan + UBSan (bloccante), TSan | 62/62, TSan superato | 54/54, TSan superato |
| Riferimenti numerici | `tools/stat_reference.py --check`: 483 valori di scipy 1.18.1, numpy 2.5.3, statsmodels 0.15.0 identici alla fixture | — |
| Fuzzing (≥ 300 s per harness nuovo o toccato) | nuovo: **`fuzz_stats`** 1 378 441 esecuzioni (309 s); toccati: `fuzz_modes` 1 479 362 (304 s, sezione di Distribution), `fuzz_ui` 16 122 (310 s, controller e tasto O), `fuzz_workspace` 2 262 380 (302 s, sezioni `0x0101`/`0x0117`), `fuzz_expr` 680 923 (315 s, x̂/ŷ e nomi di VARS), `fuzz_eactivity` 979 con il controller (315 s, strisce con Statistics) e 9 130 371 sui soli documenti (302 s): nessun crash né artefatto sul codice finale. Durante lo sviluppo `fuzz_stats` ha trovato i tre errori F1–F3 (§6) | — |
| Simulatore (`tools/sim_compare.py`) | **164 identici**: 134 precedenti invariati, 3 rigenerati per cambiamenti voluti (sotto), 27 nuovi (148–174, `tests/sim/statistics_v118.txt`) | le stesse schermate (la Statistics della Plus non esiste) |
| Relay AI | 15/15 | — |

Screenshot rigenerati: **52** e **53** (`run_matrix_v111.txt`: l'editor delle
liste della Plus ha quattro colonne e la barra GRAPH CALC TEST INTR DIST ▶),
**84** (`run_matrix_v112.txt`: VARS › STAT con i sottomenu X, Y, GRAPH, PTS,
INPUT, TEST, INTR, DIST). Tutti gli altri sono identici pixel per pixel.

## 8. Firmware e memoria

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Immagine | **2 807 216 B** = 66 % della partizione da 4 MB (+181 744 B sulla v1.17.0; budget 3 774 873 B: restano 967 657 B) | **1 944 228 B** = 92 % della partizione da 2 MB, **identica** alla v1.17.0 salvo la stringa della versione e la data di compilazione (5 punti, nessun byte di codice); restano 120 156 B sotto il budget di 2 064 384 B |
| RAM statica | 312 608 B fino a `__end__` = `0x2004C520` (+1 896 B: oggetti statici degli accessori e tabelle della vista); heap SRAM 178 912 B | 305 236 B (invariata); heap SRAM 186 284 B |
| Oggetti esclusi nella mappa | 0 (tutto compilato) | 179 oggetti/simboli della Plus (anche `stat_engine`, `dist_core`, i controller nuovi, `FitTask`, `AnovaTask`, `DistributionApp`), nessuno nell'immagine |
| Warning del compilatore | nessuno | nessuno |

Flash dei moduli nuovi sulla Plus (oggetti della mappa e `arm-none-eabi-size`):

| Modulo | Flash (B) |
|---|---:|
| `apps/statistics_plus_app.cpp` (editor, menu, moduli, lavori a passi) | 88 644 |
| `stat_engine.cpp` (nucleo) | 77 662 |
| `apps/stat_plot.cpp` (grafici) | 19 696 |
| `apps/distribution_app.cpp` | 18 042 |
| `dist_core.cpp` (24 distribuzioni, condiviso) | 15 316 |
| `distribution.cpp` (sezione `0x0117`) | 7 303 |

Il controller della v1.16.0 (`statistics_app.cpp`) non è più nell'immagine
della Plus (è nell'`#else`), da qui la crescita netta minore della somma.
Stack (`-fstack-usage`, Plus): frame più grandi del codice nuovo
`stats::decode` 1 792 B e `stats::reset` 1 640 B (uno `PlusState` temporaneo:
garanzia forte), `drawRegression` 976 B, `StatisticsApp::key` 880 B; il massimo
del firmware resta `SystemApp::enter` (2 016 B). Nessuna ricorsione nuova salvo
la ricerca delle radici di x̂ (profondità ≤ 4, grado del polinomio) e l'ordine
delle formule delle liste (≤ 26 livelli).

Dettagli e hash degli UF2: [BUILD_STATUS.md](../BUILD_STATUS.md).

## 9. Cancello

| Controllo | Esito |
|---|---|
| `git fetch origin main` | `main` fermo a `aa2923c`: fast-forward |
| `tests/run_host_tests.sh` (due profili) | 62/62 e 54/54 |
| `--sanitize --tsan` | 62/62 e 54/54, TSan superato su entrambi |
| Fuzzing ≥ 300 s per harness nuovo o toccato | 7 corse (1 nuovo, 5 toccati, `fuzz_eactivity` anche sui soli documenti), ognuna ≥ 300 s: nessun crash |
| Simulatore | 134 identici, 3 rigenerati con il motivo, 27 nuovi |
| Firmware | due schede `--clean`, nessun warning; Plus 66 % (< 90 %); Pico 2 W 1 944 228 B, identico alla v1.17.0 salvo versione e data |
| Congelamento del Pico 2 W | `architecture_test` («Pico 2 W frozen at v1.16.0», 23 modalità, nessuna Base); immagine identica alla v1.17.0 salvo i 4 byte della stringa della versione |
| Relay | 15/15 |
| Documentazione | ANALISI_FONTI e SVILUPPO v1.18.0, STATISTICS, DISTRIBUTION, HARDWARE_TEST_v1.18.0, FEATURE_MATRIX, ROADMAP (Geometry alla v1.19.0, Program e Python alla v1.20.0/v1.21.0, le altre rinumerate), BUILD_STATUS, CHANGELOG, APPLICATIONS_ARCHITECTURE, STORAGE, FILE_FORMATS, TESTING, README, docs/README, releases/v1.18.0, CONTINUE, VERSION |
