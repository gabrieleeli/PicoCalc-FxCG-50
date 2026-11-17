# Rapporto di sviluppo — v1.13.1 (versione di correzione)

Base: `main` = tag `v1.13.0` = commit `a72d901`. All'avvio della sessione
l'ultima release pubblicata era la **v1.11.0** mentre `main` era già al
commit `51691de` (v1.12.0, release in corso): `main` non coincideva con il
tag ed è stato usato `main`, come richiesto. Durante la ricerca degli errori
`main` è avanzato al commit `a72d901` (v1.13.0, sessione parallela) e la v1.13.0
è stata pubblicata (22:35 UTC del 16/11/2026): il branch di sessione è stato
portato in avanti (fast-forward, nessuna riscrittura) e **tutta la ricerca è
stata ripetuta sulla v1.13.0**; la versione di correzione è quindi la
**v1.13.1** (patch dell'ultima release). Nessuna funzione nuova.

Elenco degli errori con gravità, riproduzione, causa e correzione:
[ERRORI_v1.13.1.md](ERRORI_v1.13.1.md).

## 1. Ricerca degli errori (§2)

| Controllo | Strumento | Esito sulla v1.13.0 |
|---|---|---|
| Test host, due profili | `tests/run_host_tests.sh` | 45/45 per profilo (12 081 / 12 075 controlli) |
| Sanitizer | `--sanitize --tsan` | superati, ma UBSan non era bloccante (E11) |
| Relay AI | `python -m unittest` | 15/15 |
| Analisi statica | cppcheck 2.13; GCC `-Wall -Wextra -Wshadow -Wconversion -Wdouble-promotion -Wnull-dereference -Wduplicated-cond -Wlogical-op`; clang-tidy 18 (`clang-analyzer-*`, `bugprone-*`) | nessun errore reale tra gli avvisi; i cicli con contatore `double` segnalati da clang-tidy hanno portato a E02, E14, E15 |
| Stack | firmware con `-fstack-usage` | frame massimo non ricorsivo 2 016 byte (`SystemApp::enter`); valutatore ricorsivo ≤ 328 byte per livello, limitato dal budget di 16 KB e dalla profondità 32; nessun frame illimitato |
| Fuzzing | libFuzzer + ASan + UBSan bloccante (`tools/fuzz`) | E01 (lettura del workspace, due varianti), E10 (Ran#), E12 (Σ) |
| Revisione del codice | expression, exact, run_matrix, equation, natural_editor, workspace, csv_io, file_store, graph_file, graph_commands, statistics, table_model, calc_library | E02–E09, E14–E17 |
| Simulatore | 7 script, 77 schermate confrontate pixel per pixel con `docs/screenshots` | 9 diverse (E18) |
| Manuale fx-CG50 | esempi nelle suite e controlli puntuali | E05 (nCr/nPr) |
| Documentazione | confronto con il codice e con i test eseguiti | E13, E18 |
| Collaudo su hardware | — | **nessun esito hardware** |

### Fuzzing

Harness in `tools/fuzz` (script `tools/fuzz/run_fuzz.sh`), con un corpus
iniziale dalle fixture del workspace (v1.6.0–v1.12.0), dalle stringhe dei test
e da un progetto `.pcgraph`:

| Harness | Cosa prova | Esecuzioni (tutte le campagne) | Risultato |
|---|---|---|---|
| `fuzz_expr` | parser e valutatori (Comp, Base-N, complessi, esatto, collezioni) | 2,2 milioni | E10 (UB in `Ran# n`); dopo la correzione nessun errore |
| `fuzz_exactdiff` | forma esatta contro valore decimale | 1,4 milioni | solo differenze dovute agli arrotondamenti del calcolo decimale (`10^16+1−10^16`, `88888888888888888%8888888`) e `nCr(n,0)` esatto oltre il limite allora vigente (E05) |
| `fuzz_workspace` | lettura dei record e andata e ritorno | 15 milioni | E01 (coefficienti non finiti; Solver oltre 255 caratteri); dopo la correzione nessun errore |
| `fuzz_csv` | lettore CSV, liste e matrici | 8 milioni | nessun crash |
| `fuzz_pcgraph` | progetti `.pcgraph`, G-Mem, andata e ritorno | 4,7 milioni | nessun errore |
| `fuzz_equation` | SIML, POLY, SOLVER e visualizzazione | 0,76 milioni | nessun crash; un timeout: SolveN nell'equazione del SOLVER (A6, aperto) |
| `fuzz_ui` | sequenze di tasti sull'intero firmware | 21 000 sequenze | nessun crash |

La verifica finale (§4) ha rieseguito tutti gli harness dal repository con
`tools/fuzz/run_fuzz.sh build/fuzz 90` sul codice corretto: nessun crash.

## 2. Correzioni (§3)

Ogni correzione ha prima un gruppo di controlli in `tests/fixes_v1131_test.cpp`
eseguito sui sorgenti della v1.13.0 (falliti: 29 su 90 per E01–E09; nessuna
risposta per E02 con n e M grandi e per E14; UBSan per E10 ed E12; E15
restituisce 1000000; 1 controllo per E16 e 12 per E17), poi la
correzione, poi il test superato (142/142).

| Errore | File | Correzione |
|---|---|---|
| E01 | `src/workspace.cpp` | controlli dei blocchi v8/v9 anche nei blocchi vecchi (coefficienti finiti, Solver ≤ 255 caratteri, elementi delle liste finiti) |
| E02 | `src/calc_library.cpp` | somma ipergeometrica sul supporto, al più 10⁶ termini |
| E03 | `src/calc_library.cpp` | inverse discrete con la somma cumulativa (stessi risultati) |
| E04 | `src/equation.cpp` | forma esatta per sostituzione solo per i polinomi in `i`, confronto per componenti |
| E05 | `src/expression.cpp` | nCr/nPr con n < 1E10, ciclo fermato a 1E100 |
| E06 | `src/csv_io.cpp`, `include/csv_io.hpp`, `src/apps/matrix_editor.cpp` | `decode` in due passate, `maxCells` per le matrici |
| E07 | `src/file_store.cpp` | `writeAtomic` completa il salvataggio interrotto; `recover()` usa la stessa funzione |
| E08 | `src/statistics.cpp`, `src/calc_library.cpp` | quantili t con simmetria e intervallo allargato |
| E09 | `src/statistics.cpp` | 1-Variable pesato senza ripetere i dati |
| E10 | `src/expression.cpp` | controllo di `Ran# n` prima della conversione |
| E11 | `tests/CMakeLists.txt` | `-fno-sanitize-recover=undefined` |
| E12 | `src/expression.cpp` | Σ/Π contano i termini invece di convertire gli estremi |
| E13 | `docs/TESTING.md` | numeri rigenerati |
| E14 | `src/calc_library.cpp` | geometrica con `log1p`/`expm1` e inversa in forma chiusa |
| E15 | `src/calc_library.cpp` | `InvPoissonCD` oltre 1E6: Argument ERROR |
| E16 | `src/calc_library.cpp` | `logChoose` diretto con min(k, n−k) ≤ 64 |
| E17 | `src/calc_library.cpp` | nomi di V-WIN e TABLE in `displayName` |
| E18 | `tests/sim/run_matrix.txt`, `tests/sim/run_matrix_v111.txt`, `docs/screenshots` | percorsi di OPTN aggiornati, screenshot rigenerati |

Vincoli rispettati: un solo parser, un solo AST, un solo valutatore (nessun
percorso di calcolo nuovo); i dati restano nel workspace; il **formato del
record non cambia** (v9): i record validi delle versioni 1–9 si leggono come
prima (fixture v1.6.0–v1.12.0 nei test), si rifiutano solo i record che dopo il
primo salvataggio non si potrebbero più rileggere; nessun test indebolito,
disattivato o saltato; `architecture_test` invariato; la build con sanitizer è
più severa (E11).

## 3. Verifica finale (§4)

| Controllo | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| `tests/run_host_tests.sh` | 46/46: 44 suite C++ con **12 223** controlli (12 081 + 142 di `fixes_v1131_test`), `mem_pools_smoke` (≈ 333 000), `sd_interop_test` (41) | 46/46: **12 217** controlli + ≈ 333 000 + 41 |
| `--sanitize` (ASan + UBSan bloccante) | 46/46, nessun errore | 46/46, nessun errore |
| `--tsan` (`mem_pools_tsan`) | superato | superato |
| Relay AI | 15/15 | |
| `scripts/build.sh --clean` | nessun warning; immagine 1 848 768 byte (44 % di 4 MB, +7 160 rispetto alla v1.13.0), RAM statica 307 296 byte | nessun warning; 1 839 940 byte (88 % di 2 MB, +7 168), RAM statica 303 372 byte |
| Simulatore | 77/77 schermate uguali agli screenshot pubblicati | |
| Prova su PicoCalc fisica | **non eseguita** | **non eseguita** |

## 4. Problemi aperti

Vedi [ERRORI_v1.13.1.md](ERRORI_v1.13.1.md#problemi-rimasti-aperti): A1 (record
illeggibile dopo il ritorno a un firmware precedente), A2 (precisione di
`HypergeoPD` con argomenti tutti grandi), A3 (risultati sotto 1E−99 in
Run-Matrix, da verificare su una fx-CG50), A4 (screenshot storici non
rigenerabili), A5 (nessun collaudo su hardware), A6 (SOLVER di EQUATION con
SolveN/FMin/FMax nell'equazione: Time Out solo dopo un calcolo lungo).
