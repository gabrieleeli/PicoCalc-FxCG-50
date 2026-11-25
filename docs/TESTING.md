# Test — v1.20.0 (Program sulla microSD con C, C++ e C# e librerie, solo Plus: `program_lang_test` con un caso per costrutto e il debugger, `program_corpus_test` con 51 programmi confrontati con gcc, g++ e Mono, `code_store_test`, `lib_manager_test`, `libraries_test` con gli esempi, `program_flow_test`, `https_lib_test`, fuzzer `fuzz_c`, `fuzz_cpp`, `fuzz_cs`, `fuzz_libjson`, `fuzz_http`, `fuzz_codestore`, screenshot 203–223; v1.19.0: Geometry alla parità sulla Plus: `geometry_test` con un caso per voce del nucleo, proprietà su costruzioni casuali e migrazione dei file della v1.17.0, `geometry_flow_test` con un caso per voce dal firmware simulato, fuzzer `fuzz_geometry` (formato e sequenze di comandi), screenshot 175–202; v1.18.0: Statistics alla parità sulla Plus: `statistics_test` contro scipy/numpy/statsmodels con `tools/stat_reference.py`, `statistics_flow_test` con un caso per voce, Distribution in `apps_init_test`/`base_modes_test`/`base_flow_test`, fuzzer `fuzz_stats`, screenshot 148–174; v1.17.0: base di tutte le modalità: `apps_init_test` su ogni modalità aperta dei due profili, `base_modes_test`, `base_flow_test`, `workspace_v117_test`, `link_tool_test`, fuzzer `fuzz_sheet`/`fuzz_link`/`fuzz_modes`/`fuzz_python`, screenshot 128–147; v1.16.0: eActivity; v1.15.0: fondamenta comuni)

## Cosa viene provato e dove

| Livello | Strumento | Stato |
|---|---|---|
| Logica portabile (motore, editor, app, grafici, tabelle, 3D, file, storage, memoria, protocollo AI) | test host C++ (CTest), **per entrambi i profili di scheda** (`FXAI_TEST_BOARD=PICO_PLUS_2W` e `PICO2W`) | eseguito: v1.20.0 68 suite sulla Plus (22 482 controlli) e 52 sul Pico 2 W (16 722, solo `architecture_test` cambia); v1.19.0 61 suite sulla Plus (20 594 controlli) e 52 sul Pico 2 W (16 378) (le suite dei servizi e delle modalità solo Plus — `graph2d_test`, `binding_test`, `program_core_test`, `base_modes_test`, `base_flow_test`, `workspace_v117_test`, `statistics_flow_test`, `geometry_test`, `geometry_flow_test`, `link_tool_test` — si compilano solo dove ci sono); conteggi in [reports/SVILUPPO_v1.19.0.md](reports/SVILUPPO_v1.19.0.md) |
| Modalità e servizi per scheda (v1.15.0) | `architecture_test` (registro ⇔ definizioni ⇔ sorgenti compilati) e controllo della mappa del linker nel build del firmware; dalla v1.16.0 anche il **congelamento del Pico 2 W** (le modalità aperte del Pico 2 W devono restare quelle della v1.16.0, nessuna Base); dalla v1.17.0 lo stato Base e il servizio ModeCore | eseguito su entrambe le schede |
| Screenshot | `tools/sim_compare.py` esegue tutti i `tests/sim/*.txt` e confronta ogni PNG con `docs/screenshots` pixel per pixel | eseguito: v1.20.0 190 precedenti identici, 2 rigenerati per cambiamenti voluti (141, 142: la Program della v1.17.0 diventa il gestore dei file e l'editor nuovi), 21 nuovi (203–223, Program e LIB); v1.19.0 162 precedenti identici, 2 rigenerati (144, 172), 28 nuovi (175–202, Geometry) |
| Integrazione continua | GitHub Actions (`.github/workflows/release.yml`, `ubuntu-24.04`) | a ogni push: test host dei due profili, ASan/UBSan, TSan, relay, firmware dei due moduli, pacchetto |
| Architettura (registro ↔ sorgenti ↔ firmware) | `architecture_test` | eseguito |
| Stessi test con AddressSanitizer + UBSan | `--sanitize` (dalla v1.13.1 UBSan è bloccante: `-fno-sanitize-recover=undefined`) | eseguito su entrambi i profili |
| Allocatore su due thread con ThreadSanitizer | `--tsan` (`mem_pools_tsan`, cartella di build propria) | eseguito su entrambi i profili |
| Memoria esaurita con `operator new` instradato nei pool | `oom_test` (SRAM simulata limitata, senza PSRAM) | eseguito |
| PSRAM | pool PSRAM simulato sul PC (arena TLSF; 8 MB nei test di sistema e nel simulatore), self-test con modelli di guasto | eseguito; il chip APS6404L reale non è provato |
| Flash interna | modello NOR (`storage::RamFlash`: erase a 0xFF, program che azzera bit, interruzione dopo n byte) per il record A/B e per LittleFS | eseguito; la flash reale non è provata |
| Interfaccia | simulatore host + screenshot PNG | eseguito (`docs/screenshots/`: Run-Matrix 12–19, 44–51 e 52–61, EQUATION 08 e 62–76, GRAPH/TABLE 06, 07, 20–37, 3D Graph 40–43 e 88–104, MEMORY › FILES 105–107, eActivity 108–127, modalità Base 128–147, Statistics e Distribution 148–174, Geometry 175–202) |
| microSD (protocollo SD in SPI + FatFs) | scheda SD simulata nei test host | eseguito (`sd_fatfs_test`); bus SPI0 reale non provato |
| Interoperabilità FAT/exFAT | `sd_interop_test` (immagini con mkfs.fat/mkfs.exfat, fsck, mtools) | eseguito: 41 controlli |
| Relay AI | `unittest` Python con SDK ufficiali e API simulate, TLS in loopback | eseguito: 15 test |
| Link con il PC (v1.17.0) | `link_tool_test` (CTest, solo Plus): `tools/test_picocalc_link.py` guida `tools/picocalc_link.py` contro `tests/link_peer` (lo stesso `src/link_protocol.cpp` del firmware su stdin/stdout, con righe di log prima delle trame) | eseguito: 6 test (trame con rumore, pacchetti e `.pclink`, percorsi, PC → dispositivo con e senza rumore, dispositivo → PC con estrazione e reinvio, pacchetto danneggiato rifiutato); USB reale non provata |
| Compilazione firmware RP2350 | Pico SDK 2.2.0, `scripts/build.sh --board pico_plus_2w` e `--board pico2w` | eseguita, nessun warning, `picotool info` verificato ([BUILD_STATUS.md](BUILD_STATUS.md)) |
| Stack sul target | `-fstack-usage` su tutto il firmware | eseguito nella v1.13.1: frame massimo 2 016 byte (`SystemApp::enter`) su 32 KB; valutatore ricorsivo ≤ 328 byte per livello, limitato dal budget di 16 KB |
| Fuzzing (v1.13.1, v1.14.0, v1.15.0) | libFuzzer + ASan + UBSan (`tools/fuzz`, `run_fuzz.sh`, `FUZZ_ONLY` per eseguire solo gli harness toccati); v1.20.0: **`fuzz_c`**, **`fuzz_cpp`**, **`fuzz_cs`** (nucleo dell'interprete per linguaggio, semi dai test, dagli esempi, dalle librerie e dal corpus), **`fuzz_libjson`** (manifest, indice, semver, percorsi), **`fuzz_http`** (risposte HTTP e URL), **`fuzz_codestore`** (archivio CODE con interruzioni); toccati `fuzz_program`, `fuzz_modes`, `fuzz_ui`: parser e valutatori (v1.15.0: anche celle e nomi legati), forma esatta contro decimale, workspace con andata e ritorno (anche record v11 con sezioni), CSV, `.pcgraph`, EQUATION, sequenze di tasti; `fuzz_graph3d`; v1.15.0: **`fuzz_typedfile`**, **`fuzz_texteditor`**, **`fuzz_program`** (solo Plus); v1.16.0: **`fuzz_eactivity`** (documenti `.pceact` e controller guidato da tasti; con `FXAI_FUZZ_EACT_DOCS=1` solo documenti, molto più veloce); v1.17.0 (solo Plus): **`fuzz_sheet`** (formule e file `.pcsheet`, ricalcolo), **`fuzz_link`** (trame con rumore, pacchetti, trasferimento completo), **`fuzz_modes`** (sezioni e file delle modalità Base; v1.18.0 anche Distribution), **`fuzz_python`** (script MicroPython con budget e heap limitati); v1.18.0 (solo Plus): **`fuzz_stats`** (sezioni `0x0101`/`0x0117`, statistiche e regressioni a passi, test, intervalli, ANOVA, 24 distribuzioni con dati casuali; i casi trovati sono in `tests/fixtures/fuzz/fuzz_stats_*`); v1.19.0 (solo Plus): **`fuzz_geometry`** (sezione `0x0110` e file `.pcgeo` v1/v2 con andata e ritorno; sequenze di comandi — disegno, costruzioni, trasformazioni, spostamenti, valori e blocchi, cancellazione, Undo, Clr Constraint, animazioni a passi, tabella — con le invarianti: ordine topologico, definizioni coerenti, geometria finita, ricalcolo incrementale = completo, residui dei blocchi; caso trovato in `tests/fixtures/fuzz/fuzz_geometry_anim_locks.bin`) | eseguito sul profilo Plus (conteggi in [reports/SVILUPPO_v1.13.1.md](reports/SVILUPPO_v1.13.1.md) e [reports/SVILUPPO_v1.14.0.md](reports/SVILUPPO_v1.14.0.md)) |
| Collaudo fisico | procedure [HARDWARE_TEST_v1.20.0.md](HARDWARE_TEST_v1.20.0.md), [HARDWARE_TEST_v1.19.0.md](HARDWARE_TEST_v1.19.0.md), [HARDWARE_TEST_v1.18.0.md](HARDWARE_TEST_v1.18.0.md), [HARDWARE_TEST_v1.17.0.md](HARDWARE_TEST_v1.17.0.md), [HARDWARE_TEST_v1.16.0.md](HARDWARE_TEST_v1.16.0.md), [HARDWARE_TEST_v1.15.0.md](HARDWARE_TEST_v1.15.0.md), [HARDWARE_TEST_v1.14.0.md](HARDWARE_TEST_v1.14.0.md), [HARDWARE_TEST_v1.13.1.md](HARDWARE_TEST_v1.13.1.md), [HARDWARE_TEST_v1.13.0.md](HARDWARE_TEST_v1.13.0.md), [HARDWARE_TEST_v1.12.0.md](HARDWARE_TEST_v1.12.0.md), [HARDWARE_TEST_v1.11.0.md](HARDWARE_TEST_v1.11.0.md), [HARDWARE_TEST_v1.10.0.md](HARDWARE_TEST_v1.10.0.md), [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) e [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md), SYSTEM → DIAG | **non eseguito: in attesa dei risultati dal dispositivo** |

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
python3 tools/test_picocalc_link.py build/host-pico_plus_2w/link_peer   # strumento Link (anche in CTest: link_tool_test)
python3 tools/stat_reference.py --check     # v1.18.0: riferimenti di scipy/numpy/statsmodels identici alla fixture
python3 tools/stat_reference.py             # rigenera tests/fixtures/stat_reference.inc (pip install scipy numpy statsmodels pandas)
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
| program_lang_test **nuova v1.20.0** (solo Plus) | 180 | — | un caso per costrutto delle tabelle LK/LC/LP/LS (C, C++, C#) con le uscite di gcc, g++ e .NET, libreria `fx` (FX), debugger DB-01…DB-07 (prima fermata, punti d'interruzione all'ingresso della riga, STEP/OVER/OUT, ricorsione, variabili e figli, AC in pausa), regressioni dei fuzzer |
| program_corpus_test **nuova v1.20.0** (solo Plus) | 52 | — | 51 programmi tipici di `tests/program_corpus` (C 12, C++ 25, C# 14) eseguiti su una SD in RAM con l'input `.in`; uscita uguale byte per byte a quella di gcc, g++ e Mono (`tools/program_corpus_ref.py`) |
| code_store_test **nuova v1.20.0** (solo Plus) | 223 | — | archivio `PICOCALC/CODE`: cartelle, nomi, NEW dai modelli, RENAME/COPY/MOVE/DEL/DEL-ALL, SEARCH, password che seguono i file, scritture atomiche interrotte dopo ogni byte, file a blocchi, migrazione della v1.17.0 da flash e SD |
| lib_manager_test **nuova v1.20.0** (solo Plus) | 903 | — | manifest e indice (campi, limiti, percorsi), semver e intervalli (anche `^0.x`), piani con dipendenze, conflitti, cicli e firmware minimo, installazione a passi con giornale e recupero dopo un'interruzione a ogni passo, VERIFY, dipendenti, parser HTTP |
| libraries_test **nuova v1.20.0** (solo Plus) | 50 | — | le 9 librerie di `libraries/` installate dal repository vero, VERIFY, uso da programmi C, C++ e C# (uscite di gcc/g++/Mono); i 5 esempi di `docs/examples/program` con le uscite `.out` |
| program_flow_test **nuova v1.20.0** (solo Plus) | 96 | — | Program dai tasti della PicoCalc: migrazione, gestore dei file, editor (FIND, REPL anche con il testo vuoto, UNDO/REDO, JUMP, rientro), file di 3 MB a blocchi, password (anche per DEBUG), RUN con input, DEBUG, LIB con LOCAL, SET UP |
| https_lib_test **nuova v1.20.0** (solo Plus) | ✓ | — | server HTTPS di prova sul PC (`tests/https_lib_server.py`, OpenSSL) e client del firmware: successo, impronta sbagliata, connessione chiusa a metà, dipendenze mancanti, ciclo |
| libraries_index_test **nuova v1.20.0** (solo Plus) | ✓ | — | `tools/libraries.py check`: manifest e indice di `libraries/` aggiornati |
| geometry_test **nuova v1.19.0** (solo Plus) | 258 | — | nucleo puro di Geometry: un caso per voce della tabella del capitolo 14 (Draw, Draw Spec, Construct con le intersezioni di ogni coppia di tipi e i casi degeneri, Transform, misure, valori e blocchi col risolutore, vincoli incompatibili, Edit, opzioni, animazioni e tabella, `.pcgeo` v2, migrazione della sezione e del file della v1.17.0 con la fixture `geometry_v1_17_0.inc`, limiti con Memory ERROR) e le proprietà G01–G14 su 60 costruzioni casuali (riflessione due volte = identità, rotazione isometrica, dilatazione k → area ×k², due riflessioni = rotazione di 2θ, ricalcolo incrementale = completo, residui dei blocchi) |
| geometry_flow_test **nuova v1.19.0** (solo Plus) | 211 | — | un caso per voce con i tasti della PicoCalc sul firmware completo: menu e schede, puntatore, File (anche il file della v1.17.0 sulla microSD), View, Edit, Draw, Construct, Transform, casella di misura (valore, LOCK, PASTE, LABEL, vincoli incompatibili), Option/Properties (Store/Recall Picture), Animate (Go, AC, tabella, LIST, STAT verso Statistics), SET UP, riavvio, Memory ERROR |
| statistics_flow_test **nuova v1.18.0** (solo Plus) | 174 | — | un caso per voce della tabella di parità del capitolo 6 (E01–E13 editor, G01–G19 grafici, C01–C05 CALC, R01–R18 regressioni, T01–T14 TEST, I01–I06 INTR, D01–D12 DIST, U01–U02 SET UP e sezione `0x0101`, A01–A02 VARS e Ans): tasti della PicoCalc sul firmware completo, schermate, liste, variabili, pixel dei grafici, AC durante SinReg, ANOVA e liste della DIST |
| statistics_test (v1.18.0: nucleo della Plus) | 957 | 63 | v1.16.0: valori di tabella; v1.18.0 (Plus): 1/2-Var con frequenze e Q1Q3 Std/OnData, 12 regressioni con e senza frequenze, Sin/Logistic contro `least_squares`, test e intervalli, χ² GOF/2-way, ANOVA (statsmodels), 24 funzioni di distribuzione e liste, modelli dei grafici, sezione `0x0101` — contro `tests/fixtures/stat_reference.inc` (483 valori di scipy/numpy/statsmodels, tolleranze dell'ANALISI_FONTI §6); overflow e casi trovati dal fuzzing |
| apps_init_test **nuova v1.17.0** | 2 921 | 1 556 | ogni modalità aperta del registro (Available e Base) nei casi C1 avvio a freddo, C2 dopo SYSTEM › RESET, C3 record v11 valido, C4 record v11 corrotto e troncato, C5 solo SRAM, C6 memoria quasi esaurita («Memory ERROR» senza crash), C7 microSD assente e presente, con i controlli K1 si apre da MENU (tasto diretto), K2 disegna la schermata e la barra F1–F6, K3 sequenza tipica senza errori inattesi, K4 esce con MENU e con SHIFT ' dalle strisce dell'eActivity, K5 nessuna perdita dell'allocatore (allocazioni e byte vivi, `mem_new`), K6 stato conservato dopo uscita, rientro e riavvio simulato (record autosalvato) |
| base_modes_test **nuova v1.17.0** (solo Plus) | 384 | — | nuclei delle dieci modalità Base: riferimenti e formule del foglio, ricalcolo (Kahn, Circular ERROR, AC), blocchi con garanzia forte, colonna ↔ List, codifiche; Dyna (fotogrammi, errori), Recursion (an+1 = 2an+1, Fibonacci, Σ, ragnatela), Conic (punti notevoli, due rami), Financial (TVM END/BGN, NPV/IRR, ammortamento, CNVT, giorni; NaN rifiutati), file `.pcdyna`, `.pcrecur`, `.pcfin` (solo dati validi, garanzia forte), Program (`.pcprog`, sintassi, modelli), Link (trame con rumore, Sender/Receiver su porta in memoria, ritrasmissione, Time Out, AC, percorsi non validi), Geometry (misure, hitTest, vista), Picture Plot (coordinate, finestra), Python (espressioni, interi grandi, eccezioni, `input()`, KeyboardInterrupt, RecursionError, MemoryError, casioplot) |
| base_flow_test **nuova v1.17.0** (solo Plus) | 112 | — | le dieci modalità dai tasti della PicoCalc con il firmware completo (`app_rig`): sequenze tipiche, FILE › SAVE·AS/OPEN (anche Start/End/Pitch di Dyna e Start/End/a0/a1 di Recursion nei file), errori nei moduli, AC, riavvio con lo stato conservato |
| workspace_v117_test **nuova v1.17.0** (solo Plus) | 40 | — | record v12: firma, CRC per sezione, migrazione dal record v11 della v1.16.0 (`tests/fixtures/workspace_v1_16_0.inc`), sezione con CRC sbagliato o non decodificabile isolata e avviso, errori di struttura che rendono non valido il record, parti `0x40 + AppId` delle strisce, RESET |
| eactivity_test **nuova v1.16.0** | 289 | 289 | una prova per ogni voce E01–E56 della tabella di parità: nucleo (codifica `.pceact` con garanzia forte e tipi sconosciuti conservati, unità del testo, a capo, nomi, 24 strisce per scheda), parti del workspace (tutto o niente, ripristino dopo un'interruzione durante una striscia), controller dai tasti della PicoCalc (file e cartelle, testo con espressioni Math, calcoli e ricalcolo fino allo stop con Break, colori, memo, immagini con il bordo rosso, strisce con dati propri e barra azzurra, SHIFT ' e SHIFT ,, Notes, memorie per file, SAVE/SAVE·AS/OPT/CAPACITY, Save?, limite del documento con Memory ERROR, microSD estratta) |
| workspace_v115_test **nuova v1.15.0** | 114 | 114 | record v11: le 8 fixture v1.6.0…v1.14.0 lette e riscritte identiche in v11, coda TLV vuota, sezioni sconosciute conservate in ordine e riscritte byte per byte (anche dopo altre modifiche), sezione nota decodificata, sezione nota più nuova conservata e vincente, versione più vecchia letta, dati non validi rifiutati, duplicate/fuori ordine/troncate in ogni punto/lunghezze oltre il record o oltre 4 MB/troppe sezioni/byte in più rifiutate senza toccare il workspace, RESET |
| typed_files_test **nuova v1.15.0** (v1.17.0: tipi della Plus) | 450 | 430 | tabella dei tipi per scheda, regole dei nomi (manuale 10-5), intestazione (ogni byte alterato e ogni troncamento rifiutati, versione futura, tipo sbagliato), UTF-8 (forme troppo lunghe, surrogati, NUL), limiti; API su RamFileSystem (scheda assente, guasto di ogni operazione, interruzione dopo n byte: file vecchio o nuovo, scheda piena), su FatFs (RAM disk) e su LittleFS con interruzione di alimentazione; ricerca; FileDialog (regola, sovrascrittura Sì/No, ricerca, conferma); MEMORY › FILES dal firmware |
| mode_shell_test **nuova v1.15.0** | 53 | 53 | pila di schermate, barra F1–F6 (sottomenu, EXIT, pagine ▷, SHIFT), lista, conferma (anche con errore nell'azione), richiesta, menu, disegno dei modali, SET UP da tabella, VARS e CATALOG dai resolver, TABLE sul guscio dal firmware (SET, SET UP, ritorno, DELETE, TYPE) |
| text_editor_test **nuova v1.15.0** | 72 | 72 | editing, UTF-8, righe incollate, `\r\n`, colonna voluta, PageUp/Down, ricerca circolare, salto alla riga, selezione e clipboard (Ctrl+C/X/V, Ctrl+frecce), a capo sulle parole, scorrimento orizzontale, prima riga visibile, limiti di riga e di testo, limite della scheda, memoria esaurita a ogni allocazione con testo intatto |
| step_task_test **nuova v1.15.0** | 19 | 19 | budget di unità, budget di tempo con orologio simulato (anche il giro a 32 bit), budget combinati, AC → Break con i dati parziali, errore → Failed e eccezione, 3D Graph sul runner dal firmware |
| graph2d_test **nuova v1.15.0** (solo Plus) | 26 | — | disposizioni Full/G+G/GtoT/T+G, sorgenti (funzione, buchi, parametrica, punti), campionamento a passi, asintoti, disegno incrementale identico al completo e confinato al riquadro, TRACE, ZOOM |
| binding_test **nuova v1.15.0** (solo Plus) | 32 | — | nomi legati e resolver padre, celle A1…Z999 con `$`, intervalli come liste (Sum, Mean), cella vuota, A0 = A×0, nomi legati dei programmi, grafo delle dipendenze (ordine, ricalcolo, Circular ERROR, 20 000 nodi senza ricorsione) |
| program_core_test **nuova v1.15.0** (solo Plus) | 47 | — | C (printf, #define, ricorsione, divisione intera, stringhe, scanf con attesa dell'input, controllo di flusso, goto, array 1D/2D), C++ (cout/cin/getline, string, auto, static_cast), C# (Console, Math, array, interpolazione con formati, ReadLine), comandi grafici tramite graph_commands, fx::calc con le variabili del programma, errori con riga e colonna, Nesting/Memory/Stack ERROR, Time Out, AC a passi, exit |
| expression_test (v1.10.0: 154) | 155 | 155 | priorità Casio, funzioni, complessi, formati e arrotondamento come la calcolatrice, limiti di lunghezza/annidamento, budget di stack |
| exact_test | 152 | 152 | razionali (overflow, confronto esatto), radicali, π, letterali decimali e periodici, sviluppo periodico, limiti di visualizzazione CG50, valutatore esatto (trigonometria esatta, radici, log, n!, nCr, relazioni, Σ/Π, memoria esatta, catene lunghe, budget di stack), coerenza esatto/virgola mobile |
| fixes_v1131_test **nuova v1.13.1** | 142 | 142 | un gruppo per ogni errore di [ERRORI_v1.13.1.md](reports/ERRORI_v1.13.1.md), che fallisce con i sorgenti della v1.13.0: record v1–v8 non rileggibili dopo il salvataggio (fixture della v1.11.0 modificata), HypergeoCD/InvHypergeoCD con x, n, M grandi, inverse discrete in tempo lineare (anche contro la definizione), forme esatte complesse con Abs/Conjg/ReP, nCr/nPr fino a 1E10, CSV senza allocazioni prima dei controlli (byte misurati con un `operator new` contato), salvataggio atomico dopo due interruzioni, quantili t, 1-Variable con frequenze, Ran# n, Σ/Π con estremi enormi, distribuzione geometrica con p piccolo, InvPoissonCD oltre 1E6, HypergeoPD con argomenti grandi, nomi di VARS › V-WIN e TABLE |
| calc_v112_test **nuova v1.13.0** | 207 | 207 | CALC con i valori del manuale (d/dx 52, d²/dx² 26, ∫ 404/3, Σ con variabile e passo, Solve con limiti ed equazione, SolveN anche con radice doppia e poli, FMin/FMax, funzioni vietate), PROB (sequenze di Ran#, RanInt#/RanNorm#/RanBin# con n, RanList#, RanSamp# con e senza ripetizione, P Q R t), Rnd, ʳ ᵍ ° ′ ″, Pol/Rec in List Ans, ∠ e ►r∠θ/►a+bi, 24 distribuzioni con valori di riferimento, S-Dev/Var/Mean/Median con frequenze, x̂ ŷ e VARS STAT, 22 funzioni TVM, CONVERT (anche su liste, grandezze diverse), P ed E, ENG/ENG←, overflow, Simplify Manual (m.c.m., prodotti, ▶Simp senza F=, divisore non valido, modo complesso), Base-N (Bin 16 bit, Hex con v_a, liste e matrici nella base, Oct), messaggi dell'Appendice α |
| run_matrix_v112_test **nuova v1.13.0** | 152 | 152 | `:` e `◢` (posizione degli errori, ricalcolo), stringhe e Str, function memory (X, argomento, ricorsione), r e θ, variabili di sistema (V-Window con Range ERROR, Xfct, TblStart, D Start, R End, TVM), formula an+1, List "QTY", List File 1–6 e budget comune, n→Dim List, Fill su liste e matrici, Swap/\*Row/\*Row+/Row+, forme esatte oltre 64 elementi e celle decimali, VARS EQUATION e TABLE Result, migrazioni v7→v9 e v8→v9 dai fixture della v1.11.0 e della v1.12.0, record v9 con tutti i campi nuovi e record troncati, interfaccia (OPTN nell'ordine della fx-CG50, ENG, CONVERT, VARS STAT, Hex con v_a, FUNCMEM, CATALOG, CAPTURE e RclCapt) |
| graph_cmd_v112_test **nuova v1.13.0** | 56 | 56 | le otto disequazioni, G∫dx, prefissi di stile (anche da soli), StoPict/RclPict/BG-Pict/RclCapt, G-SelOn/Off, DrawGraph, DrawStat, DrawR-Con/Plt, DrawWeb, DrawFTG-Con/Plt, DrawDyna (anche il limite di 10 grafici, tutto o niente), limite dei punti per scheda, `.pcgraph` con i grafici dei comandi (codifica, decodifica, identità), PxlTest( sul firmware (punto acceso, assi, fuori schermo, errore), StoPict eseguito da GRAPH |
| editor_v112_test **nuova v1.13.0** | 488 | 488 | corpus di 68 testi canonici (testo → albero → testo e stesso disegno, Math e Line), 13 alberi costruiti con i modelli (gruppo con frazione, radice con indice n+1, d²/dx², Σ e Π con variabile, matrice, frazione mista, potenze, |x|, logₐb, ∫), parentesi implicite (`6÷2[1/3]` = 9, con × = 1), testo Line minimo e stesso valore, Math ↔ Line della riga di input, nomi mostrati, sottolineatura di v_a, glifi nuovi nei due font, voci del CATALOG |
| equation_v112_test **nuova v1.12.0** | 244 | 244 | EQUATION, un gruppo per punto della richiesta: coefficienti (frazioni, radicali, π, A–Z, Ans, complessi esatti come polinomi in i, Non-Real/Type/Syntax, EDIT, conversione senza perdita dei double), SIML (esempio del manuale 4-2, frazioni, radicali e π, Hilbert 6×6 esatta e decimale, 2–6 incognite, 1E50, No Solution, Infinitely Many Solutions con l'esempio complesso del manuale, r∠θ, Ma ERROR/Underflow, Fix/Sci/Eng, S⇔D, ab/c), POLY (esempi 4-3, 4-4, 1-29, ordine, ×k, radicali, biquadratiche, quartiche pari, grado 6, decimali, irriducibili, radice doppia irrazionale, radici vicine, coefficienti complessi, 10⁹, a = 0, Underflow), SOLVER (esempio 4-5 per V e T, x = X, senza "=", Lower/Upper, Can't Solve!, Time Out, No Variable, Syntax ERROR, polo, Retry), schermate e tasti sul firmware (menu, EXE/EXIT, EDIT, REPEAT, CLEAR, frazione F1, S⇔D, errori con l'immissione aperta, Line, Complex Mode dal SET UP con ritorno, Solver con RECALL, Lower/Upper, DELETE), VARS › EQUATION (errori, SimRes esatto, MatAns, SimRes+SimCoef, Rref, PlyRes, PlyCoef, Trn), record v8 e migrazione v7→v8 dal fixture della v1.11.0, correzioni dell'editor (½X, GT²), estensioni |
| lists_csv_test **nuova v1.11.0** | 878 | 878 | List 1–26 nei calcoli (anche `List12`, `→List 20[1]`), sottonomi (limiti, caratteri, record), budget di tutte le liste per scheda (tutto o niente, 20 liste da 999 sulla Plus, 6 da 255 sul Pico 2 W), GRAPH con List 12, Statistics (26 colonne, riga SUB, SET XList, 1-Var), CSV: codifica e decodifica (separatori, decimale `,`, CR/LF/CRLF, intestazione, celle vuote, testo non numerico, tabulatore, file vuoto, oltre i limiti, complessi), liste e matrici dal file, file su microSD e LittleFS, file mancante e nomi non validi, interruzione di alimentazione durante il salvataggio sulla flash interna (vecchio o nuovo, mai misto), CSV dalle applicazioni (OPTN, F4) sulle due memorie, Format ERROR senza modifiche, scheda assente; editor MAT/VCT con celle esatte, complesse ed espressioni, scorrimento delle colonne; forme esatte nei calcoli (`Mat A×6`, `Det`, `→Mat A[2,2]`) e nel record v7 (anche forme incoerenti rifiutate); migrazione v6→v7 dal fixture della v1.10.0 (valori predefiniti, calcoli, round trip, troncamenti) |
| calc_v111_test **nuova v1.11.0** | 133 | 133 | simboli ingegneristici (valori, priorità, errori, forma esatta decimale, riscrittura), display Eng con i simboli (f…T, oltre ×10ⁿ) e in Run-Matrix (`999k+25k`, `9/10`, S⇔D), frazioni `⌟` (valori, errori, output Line anche negli elementi), ▶Simp con e senza fattore (Ans, errori, segno, S⇔D), Simplify Manual (modello, `⌟`, ricalcolo, persistenza), Base-N: letterali Hex/Bin/Oct/Dec, complemento a due, limiti a 32 bit, prefissi, divisione intera, operatori bit a bit anche senza spazi (`FFxor0F`), Not/Neg, funzioni escluse, testo dei risultati, Run-Matrix con basi diverse nella cronologia, viste ▶Dec…▶Oct, ricalcolo e record v7 |
| run_matrix_v111_test **nuova v1.11.0** | 106 | 106 | INS (sovrascrittura in Line, anche di `⌟`; nessuna in Math), SHIFT+INS (gruppo, numero, funzione, frazione, annullamento), tasti sul firmware (SHIFT DEL, F1 → `⌟`, scorrimento del risultato con ←/→, SHIFT+INS + √), MatAns/ListAns (apertura automatica, navigazione, sola lettura, F4 dalla cronologia, vista naturale senza schermata), comandi grafici (ognuno, variabili negli argomenti, Line, pixel, Tangent/Normal, Graph dei 4 tipi, disegno nel motore, errori tutto o niente con posizione, limite di 64 elementi, Cls, ricalcolo senza riesecuzione, riga modificata rieseguita, persistenza), schermata di GRAPH e ritorno con EXIT, menu SKETCH, riga di comando modificata che apre GRAPH, SET UP (Mode, Simplify, CSV con la combinazione rifiutata) |
| matrix_test (v1.10.0: 673) | 1 019 | 1 019 | parsing di matrici, vettori, liste, riferimenti e indici; posizioni dei token e dei nodi; matrici (+ − × ^, inversa, Det, Trn, Identity, Dim, Augment, Fill, Ref, Rref, Mat→List, List→Mat, Abs/Int), vettori (DotP, CrossP, Norm, UnitV, Angle), liste (elemento per elemento, Sum, Prod, Mean, Median, Min, Max, Cuml, Percent, ΔList, SortA/D, Seq, Augment), complessi; errori di dimensione e di tipo con la posizione; annidamento profondo e Stack ERROR; forme esatte; nucleo di Run-Matrix (→Mat/Vct/List, elementi, Dim, MatAns/VctAns/ListAns, risultati oltre 6×6, viste, modifica della cronologia); limiti per scheda; record v6 (round trip, troncamenti) e lettura dei record v1.9.0 e v1.8.0 |
| editor_v110_test (v1.10.0: 161) | 164 | 164 | modelli MAT/VCT (inserimento, navigazione a griglia, importazione, disegno), Math/Line (modelli lineari, conversione, incolla), CLIP (selezione, copia/taglia/incolla, parentesi bilanciate, nome di funzione, intero modello), parentesi colorate per livello e contrasto ≥ 4,5:1, cursore sulla causa degli errori (anche dentro i modelli), 600 sequenze casuali di modifica; sul firmware: errori e cursore, editor MAT/VCT (DIM, celle, ROW-OP, ROW, M⇔V, vettori), CLIP con SHIFT 8/9, Line da SET UP, riavvio con matrici e impostazione conservate |
| run_matrix_test | 182 | 182 | esempi frazione ⇔ decimale, frazioni miste, periodo, input decimale, irrazionali mai convertiti, radicali e π (esempi del manuale), Ans/A–Z esatti, →A~F, variabili condivise, Int÷/Rmdr/MOD_Exp/logica, modifica della cronologia con ricalcolo, alberi 2D, persistenza e lettura dei record v1.6.0 (anche con V-Window non più valida), dati corrotti, flussi completi da tastiera attraverso il `Firmware` |
| architecture_test (v1.11.0: 686; v1.12.0: 764; v1.17.0: Base, ModeCore) | 2 324 | 2 037 | Run-Matrix (v1.13.0): nessun altro lexer, parser o valutatore, `calc_library` puro, Run-Matrix solo tramite le interfacce del workspace (anche List File, Str, function memory, PxlTest(); EQUATION (v1.12.0): servizio EquationCore solo per Equation, nessuna lettura propria dei numeri né percorsi numerici separati, uso obbligato del motore `eq::` e di `MathField`, motore che valuta con il valutatore condiviso, Run-Matrix solo tramite `CollectionResolver::Memory::Equation`, `equation.hpp`/`equation.cpp` nel livello del motore; registro coerente, sezioni dei membri e dei metodi del workspace (`dataSectionOfMember`), comandi grafici solo tramite `graphcmd::execute` e disegnati da GRAPH, moduli `value`/`csv_io`/`graph_commands` nel livello del motore, controller registrati = modalità disponibili, schermate "non disponibile", include/sezioni del workspace/collegamenti dichiarati (anche GraphCore, Files, dato Graph), separazione di Run-Matrix, stratificazione motore/UI, nessun ciclo di include, modelli dati delle modalità future; 3D Graph e servizi Graph3D/Hardware |
| editor_test | 180 | 180 | modelli 2D (anche frazione mista, periodo, radice con indice), navigazione, undo/redo, copia/incolla, layout e cursore, limiti |
| math_engine_test (v1.8.0: 108) | 108 | 109 | matrici, vettori, basi, sessagesimale, frazioni, calcolo numerico; capacità della cronologia di sessione per scheda |
| storage_test (v1.11.0: 809) | 1 020 | 1 020 | serializzazione del workspace (formato v5 con lo stato di GRAPH, TABLE e 3D Graph; lettura dei formati precedenti), slot A/B, CRC, interruzione di alimentazione in ogni punto della scrittura |
| app_test | 62 | 62 | AppManager, tasti globali, errori, barra di stato, TileDiff, rendering delle schermate |
| apps_flow_test (v1.11.0: 56) | 62 | 62 | percorsi completi: Statistics → Graph, Graph ↔ Table, Equation (schermate della v1.12.0), System, Memory, Conversion |
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
| oom_test (v1.9.0: 18) | 27 | 27 | "Memory ERROR" con `operator new` instradato nei pool e SRAM limitata (condizioni del Pico 2 W): Run-Matrix, autosalvataggio rimandato e ripreso da solo, GRAPH, 3D (griglia ridotta o errore), editor; stato intatto e nessuna perdita di memoria; v1.10.0: avvio con un record più grande della memoria libera (nessuna terminazione, salvataggi sospesi, dati in flash intatti) |
| graphics_memory_test **nuova** | 23 | 23 | poligoni pieni oltre i 32 vertici, copie di layer e invalidate, regioni sporche per scheda, budget PICT, 20 parametriche dense disegnate per intero con la PSRAM, cache dei layer di GRAPH identica al pixel |
| internal_fs_test **nuova** | 197 | 197 | LittleFS su flash NOR simulata: contratto `files::FileSystem` come FatFs, partizione piena, interruzione di alimentazione durante `writeAtomic`, rimontaggio senza riformattare, progetti GRAPH nell'archivio interno |
| graph3d_test (v1.9.0: 111; portata alla v1.14.0) | 88 | 88 | motore 3D (campionamento incrementale, buchi, z-buffer e pittore, due superfici con lo stesso test di profondità, taglio al box, rotazione θ/φ, colori) e modalità (lista, input Z=, V-Window, calcolo nei tick, rotazione e zoom senza ricalcolo, ORIGINAL, persistenza, profili con e senza PSRAM) |
| graph3d_v114_test **nuova v1.14.0** | 541 | 541 | un gruppo per voce della tabella di parità del 3D Graph: V-Window (Out of Domain, Invalid Setting, normalizzazione di θ/φ), modelli e loro «Invalid Setting», conversioni dei moduli, codifica delle G-Mem, INTSECT/RELATION con gli esempi del manuale, sezioni CROSS, taglio al box (nessun pixel fuori dall'involucro del box), z-buffer condiviso con le rette, proiezione e viste, colori FORMAT, coordinate enormi (fuzzing), memoria esaurita, record v10 e migrazione delle fixture v1.6.0…v1.13.1, tutta la modalità dalla tastiera su entrambi i profili |
| system_diag_test **nuova** | 15 | 12 | SYSTEM → DIAG completa (MEM, PSR, ALC, FLS, LCD, SD, INT, GPH) con la memoria della scheda; Plus con PSRAM fallita; pagina INFO; "Archivio file"; riga del chip della flash; kernel di stress dell'allocatore |
| **Totale (v1.17.0)** | **18 623** in 58 suite C++ + ≈ 334 000 dello stress + 41 dello script + 6 test Python di Link | **16 297** in 52 suite + ≈ 334 000 + 41 | 0 errori, con e senza sanitizer (ASan + UBSan bloccante); `mem_pools_tsan` con ThreadSanitizer. I conteggi delle singole righe sono quelli della versione in cui la suite è stata misurata l'ultima volta |

v1.17.0: 61 test CTest sulla Plus (58 suite C++, lo stress dell'allocatore,
l'interoperabilità FAT/exFAT e `link_tool_test`) e 54 sul Pico 2 W (52 + 2):
18 623 controlli sulla Plus e 16 297 sul Pico 2 W (v1.16.0: 14 658 / 14 563).
Si aggiungono `apps_init_test` (due profili), `base_modes_test`,
`base_flow_test`, `workspace_v117_test`, `geometry_test`, `geometry_flow_test` e `link_tool_test` (Plus);
`architecture_test` cresce per lo stato Base, ModeCore e il congelamento.
Test esistenti adattati solo a cambiamenti voluti: la versione del record
viene da `workspaceRecordVersion()` (12 sulla Plus, 11 sul Pico 2 W) in
`board_profile_test`, `equation_v112_test`, `graph3d_v114_test`,
`lists_csv_test`, `matrix_test`, `run_matrix_v112_test`; `workspace_v115_test`
e `table_test` scrivono i record v11 con `serializeWorkspaceV11` sulla Plus;
`typed_files_test` conosce i tipi nuovi della Plus; `eactivity_test` le
strisce che sulla Plus diventano disponibili (E-CON Top resta «non
disponibile» per E47). Nessuna verifica è stata tolta. `apps_init_test` ha
trovato un errore condiviso (Conversion non segnava Ans da salvare), corretto
con la sua prova.
`workspace_v117_test` usa `tests/fixtures/workspace_v1_16_0.inc`, un record v11
generato con i sorgenti della v1.16.0 (commit `eb2338a`, generatore in
`tests/fixtures/gen_workspace_v1_16_0.cpp.txt`) con dati in Run-Matrix,
Statistics, Graph, Equation, eActivity e nelle variabili di modo.

`run_matrix_test` usa `tests/fixtures/workspace_v1_6_0.inc`, un record salvato
dai sorgenti della v1.6.0 (commit 7dc65cd), per verificare la compatibilità.
`board_profile_test` usa `tests/fixtures/workspace_v1_8_0.inc`, un record
generato con i sorgenti della v1.8.0, per la migrazione al formato v5.
`matrix_test` usa anche `tests/fixtures/workspace_v1_9_0.inc`, un record v5
generato con i sorgenti della v1.9.0 (commit `bb2bdee`), per la lettura nel
formato v6. `lists_csv_test` usa `tests/fixtures/workspace_v1_10_0.inc`, un
record v6 generato con i sorgenti della v1.10.0 (tag `v1.10.0`, commit
`dfdb08b`) prima di ogni modifica della v1.11.0, per la migrazione al v7.
`run_matrix_v112_test` usa `tests/fixtures/workspace_v1_11_0_run_matrix.inc`,
un record v7 generato con i sorgenti della v1.11.0 (tag `v1.11.0`, commit
`f6933fd`) prima di ogni modifica della v1.13.0 (Run-Matrix: Simplify, CSV,
sottonomi, Hex, comandi grafici), e `tests/fixtures/workspace_v1_12_0.inc`, un
record v8 generato con i sorgenti della v1.12.0 (commit `51691de` di `main`,
con i dati di EQUATION), per le migrazioni v7→v9 e v8→v9.
`graph3d_v114_test` legge tutte le fixture (v1.6.0, v1.8.0, v1.9.0, v1.10.0,
v1.11.0, v1.12.0) e `tests/fixtures/workspace_v1_13_1_graph3d.inc`, un record
v9 generato con i sorgenti della v1.13.1 (tag `v1.13.1`, commit `62b3f8f`)
prima di ogni modifica della v1.14.0, con funzioni 3D, finestra, vista e
griglia non predefinite: migrazione v1…v9 → v10 e conservazione
dell'orientazione della vista.

v1.14.0: 47 test per profilo (45 suite C++, lo stress dell'allocatore e
l'interoperabilità FAT/exFAT): 13 111 controlli sulla Plus e 13 105 sul Pico 2 W;
si aggiunge `graph3d_v114_test` (541 controlli); `architecture_test` cresce di 59
controlli (gruppo del 3D Graph), `oom_test` di 3 (modelli sotto esaurimento),
`storage_test` (944 → 1 020) con la lunghezza del record v10 (le prove
d'interruzione dipendono dalla dimensione del record), `graph3d_test` passa da
111 a 88 controlli nella nuova API (le 30 verifiche delle famiglie di colore
per altezza, che non esistono più, sono sostituite dai colori FORMAT verificati in
`graph3d_v114_test`; AUTO è in OPTN › Z-AUTO). Test esistenti adattati solo
ai cambiamenti voluti della v1.14.0: record alla versione 10 (controlli del
byte di versione in `matrix_test`, `lists_csv_test`, `equation_v112_test`,
`run_matrix_v112_test`, `board_profile_test`; la «versione futura» diventa
11; in `table_test` la lunghezza della coda del record), modello del 3D
(`Graph3DState`: lista vuota al primo avvio, griglia in punti 2–50) in
`board_profile_test`, `lists_csv_test`, `matrix_test`, `limits_smoke`,
`architecture_test` e `oom_test`; `graph3d_test` portata alla nuova API con le
stesse verifiche (le espressioni dell'editor naturale sono nella forma
canonica `(x)^(2)`). Nessuna verifica è stata tolta senza sostituirla.

v1.13.1: 46 test per profilo (44 suite C++, lo stress dell'allocatore e
l'interoperabilità FAT/exFAT): 12 223 controlli sulla Plus e 12 217 sul Pico 2 W;
rispetto alla v1.13.0 (12 081 / 12 075) si aggiunge `fixes_v1131_test` (142
controlli, un gruppo per errore). Nessun test esistente è stato modificato; la
build con sanitizer è più severa (UBSan bloccante). La tabella sopra riporta i
conteggi misurati nella v1.13.1 (nella v1.13.0 l'intestazione e la riga
«Totale» erano rimaste ai numeri delle versioni precedenti).

v1.13.0: 45 test per profilo (43 suite C++, lo stress dell'allocatore e
l'interoperabilità FAT/exFAT): 12 081 controlli sulla Plus e 12 075 sul Pico 2 W
più ≈ 334 000 verifiche dello stress dell'allocatore; rispetto alla v1.12.0
(10 316 / 10 310) si aggiungono le quattro suite nuove (903 controlli) e
`architecture_test` (1 320) cresce per le regole nuove (parser
unico, `calc_library` puro, separazione di Run-Matrix ampliata). Test
esistenti adattati solo a cambiamenti voluti, con il motivo accanto: Pol/Rec
danno una lista, Bin a 16 bit, Simplify Manual con il m.c.m. e ▶Simp senza
«F=1», Eng con P ed E, record alla versione 9 (anche nei controlli di versione di `equation_v112_test`), modello Σ con la variabile,
OPTN nell'ordine della fx-CG50 (posizioni delle voci nei test dell'interfaccia).
Nessuna verifica è stata tolta.

`equation_v112_test` usa `tests/fixtures/workspace_v1_11_0.inc`, un record v7
generato con i sorgenti della v1.11.0 (tag `v1.11.0`, commit `f6933fd`) prima
di ogni modifica della v1.12.0, per la migrazione al v8.

v1.12.0: rispetto alla v1.11.0 (9 757 / 9 751 controlli) si aggiunge
`equation_v112_test` (244); `architecture_test` cresce (+78) per il servizio
EquationCore, i sorgenti nuovi e le regole di EQUATION, `storage_test` (+60)
perché il record è più lungo, `apps_flow_test` (+6) per le schermate nuove.
Test esistenti adattati solo a cambiamenti voluti: `apps_flow_test` (schermate,
ordine decrescente delle radici, modo Real, F5 REL, Solver in Math),
`storage_test` (coefficienti `eq::Number`), `table_test` (costruzione di un
record v3 con il blocco di EQUATION delle versioni 1–7), `board_profile_test`,
`matrix_test` e `lists_csv_test` (record alla versione 8). Nessuna verifica è
stata tolta.

v1.11.0: rispetto alla v1.10.0 (8 587 / 8 581 controlli) si aggiungono le tre
suite nuove (960 controlli); `architecture_test` cresce perché il registro ha
un servizio, due sorgenti e regole in più, `storage_test`, `matrix_test` ed
`expression_test` perché il record è più lungo e il formato Eng ha un caso
in più. Test esistenti adattati solo a cambiamenti voluti: `board_profile_test`,
`table_test` e `matrix_test` (record alla versione 7, List 7–26 ora valide),
`editor_v110_test` (in Line il tasto frazione scrive `⌟`), `expression_test`
(Eng con i simboli). Nessuna verifica è stata tolta.

v1.10.0: rispetto alla v1.9.0 (7 705 / 7 699 controlli) si aggiungono le due
suite nuove (834 controlli) e 6 controlli in `oom_test`; `architecture_test`
cresce perché il registro ha un file e una sezione dati in più, `storage_test`
perché il record è più lungo (più tagli provati). Due test esistenti sono stati adattati a cambiamenti
voluti: `board_profile_test` e `table_test` (il record ora è alla versione 6),
`system_diag_test` e `apps_flow_test` (il primo campo del SET UP è
Input/Output, come sulla fx-CG50). Nessuna verifica è stata tolta.

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
`tests/sim/run_matrix_v110.txt` (editor MAT/VCT, matrici, parentesi colorate,
CLIP, cursore sull'errore, modalità Line: 44–51), `tests/sim/run_matrix_v111.txt`
(List 1–26 e sottonomi, CSV, MatAns, celle esatte, Line con ⌟ e INS,
risultato scorrevole, Eng, Simp, Base-N, comandi grafici: 52–61),
`tests/sim/graph.txt` (GRAPH e TABLE 06, 07, 20–37), `tests/sim/foundations_v115.txt` (MEMORY › FILES 105–107), `tests/sim/eactivity.txt` (eActivity 108–127), `tests/sim/base_v117.txt` (modalità Base 128–147, profilo Plus; 144 rigenerato nella v1.19.0), `tests/sim/statistics_v118.txt` (Statistics e Distribution 148–174), `tests/sim/geometry_v119.txt` (Geometry 175–202: i menu, le costruzioni, la casella con un blocco, FORMAT, Key Help, SET UP, griglia, i quattro esempi della guida, animazione, tabella, f(x), archi, Draw Spec; generato da un elenco di passi del puntatore), `tests/sim/graph3d.txt`
(3D Graph 40–43, rigenerati nella v1.14.0), `tests/sim/graph3d_v114.txt` (3D Graph
v1.14.0: 88–104) e `tests/sim/run_matrix_v112.txt` (SolveN e FMin, OPTN,
CATALOG, distribuzioni, CONVERT, modelli d²/dx² e Σ, istruzioni multiple,
VARS STAT, Graph Y>, G∫dx, ENG: 77–87). Il simulatore scrive PNG non compressi: prima di copiarli in
`docs/screenshots` vanno compressi senza perdita (nella v1.11.0: palette PNG
con `compress_level` 9 tramite Pillow, con la verifica che i pixel restino
identici):

```bash
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix_v110.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix_v111.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix_v112.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph3d.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph3d_v114.txt
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
  (`1.10.0 (pico_plus_2w)` / `1.10.0 (pico2w)`), board, SDK.
- Simboli del linker: stack 32 KB (`__StackBottom` 0x20078000 – `__StackTop`
  0x20080000), fine dell'immagine flash prima dell'area dati.
- `-fstack-usage` su tutto il firmware (file `.su` nella cartella di build) per
  controllare i frame, anche ricorsivi
  (vedi [MATH_ENGINE.md](MATH_ENGINE.md#limiti-protezione-dello-stack-da-32-kb-dellrp2350)).
- Controllo della dimensione dell'immagine nella partizione firmware a ogni
  build (errore se la supera).

## Collaudo su hardware (da fare)

Procedure in [HARDWARE_TEST_v1.17.0.md](HARDWARE_TEST_v1.17.0.md) (HW-220…HW-245),
[HARDWARE_TEST_v1.10.0.md](HARDWARE_TEST_v1.10.0.md) (HW-90…HW-99),
[HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) (HW-40…HW-82) e
[HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md) (HW-01…HW-36, da ripetere con
la 1.9.0); vedi anche [HARDWARE.md](HARDWARE.md#collaudo-consigliato-sul-dispositivo).
Quando una voce sarà provata sulla PicoCalc, aggiornare
[FEATURE_MATRIX.md](FEATURE_MATRIX.md) a *TESTED ON HARDWARE* indicando data,
revisione della scheda e risultato.
