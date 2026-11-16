# Sviluppo della v1.12.0 — rapporto (completamento di EQUATION)

Data: 16 novembre 2026. Branch di destinazione: **`main`**; tag `v1.12.0`;
GitHub Release "PicoCalc FxCG-50 v1.12.0". Guida della modalità:
[../EQUATION.md](../EQUATION.md).

## 1. Punto di partenza

- `git fetch origin main --tags`: `origin/main` = tag annotato `v1.11.0` =
  commit `f6933fd7f2c527906992d8bc58d496425983fddd`; il branch di sessione
  coincideva: **nessun commit successivo** da conservare. Versione: la minor
  successiva, **1.12.0**.
- Test host del profilo Pico Plus 2 W sulla base, prima di ogni modifica:
  40/40 superati.
- Prima di modificare il codice è stato generato, con i sorgenti della
  v1.11.0, il record v7 `tests/fixtures/workspace_v1_11_0.inc` (sistema del
  manuale 4-2, celle fuori dal sistema attivo con 0.1 e 1/3, POLY di grado 4
  con decimali, equazione del Solver con valore iniziale, disequazione)
  usato dalla prova di migrazione v7→v8.
- Lavoro parallelo: prima della pubblicazione `main` è stato ricontrollato
  (§8).

## 2. Fonti e analisi

Rapporto scritto prima del codice, con la tabella di parità che è il
perimetro del lavoro: [ANALISI_FONTI_v1.12.0.md](ANALISI_FONTI_v1.12.0.md).
Fonte principale: il **manuale software ufficiale fx-CG50 v3.60** (PDF Casio,
678 pagine; nelle versioni precedenti del progetto non era scaricabile),
capitolo 4 letto per intero e pagine 4-1…4-5, 1-29 e 1-36 lette anche come
immagini per i tasti funzione, i formati e i messaggi; confronto con il
manuale fx-9860GIII v3.40. Nessun codice, ROM, font o grafica Casio.

## 3. Vincoli

| Vincolo | Come è stato rispettato |
|---|---|
| Un solo parser, un solo AST, un solo valutatore | coefficienti ed equazioni con `parseExpression` + `evaluateAny`/`evaluateExact`/`evaluateReal`; la forma esatta dei complessi si ottiene dallo stesso `evaluateExact` (sostituzione di `i` con interi e differenze finite); `SimRes`… sono token dello stesso lexer risolti da `CollectionResolver`; `architecture_test` vieta ogni lettura propria dei numeri nei sorgenti di Equation (`strtod`, `atof`, `stod`, `sscanf`, `istringstream`, `evalNumber`) e i vecchi percorsi in `double` |
| Dati solo nel workspace, tramite interfacce | `EquationState` (record v8); Run-Matrix legge le matrici di EQUATION solo come `Memory::Equation` (vietati `ws.equation`, `eq::`, `equation.hpp`); servizio `EquationCore` dichiarato solo da Equation; test di architettura più severo, nessuna regola tolta |
| Memoria embedded | coefficienti e soluzioni in `std::vector` sull'heap (PSRAM sulla Plus), copia delle memorie del Solver sull'heap, nessuna matrice di lavoro sullo stack; limiti in `limits.hpp` (`kEquationUnknowns`, `kEquationDegree`, `kEquationExpression`, `kSolverEvaluations`), uguali sulle due schede come sulla fx-CG50 |
| Compatibilità | record **v8**; v1–v7 letti senza perdite (decimale più corto che riproduce il double; celle del sistema attivo alla loro riga e colonna; valore iniziale del Solver in X); fixture generata con i sorgenti della v1.11.0 |
| Niente CAS | forme esatte solo razionali, radicali quadratici, multipli di π e complessi con queste parti; il resto in virgola mobile come la fx-CG50 |

## 4. Tabella di parità finale

Stato v1.12.0: **presente**, oppure **inevitabile** (differenza inevitabile
documentata, §4.8). Gli ID sono quelli dell'analisi delle fonti; "Prova" è
il test o lo screenshot che lo verifica (`eq` = `equation_v112_test`,
`flow` = `apps_flow_test`, `arch` = `architecture_test`).

### 4.1 Menu, navigazione, impostazioni

| ID | Voce | Stato | Prova |
|---|---|---|---|
| E1 | Icona Equation, tasto diretto | presente | arch |
| E2 | Select Type, SIMUL POLY SOLVER (INEQ estensione) | presente | eq, screenshot 62 |
| E3 | Barra di stato | presente | screenshot 62–76 |
| E4 | Number of Unknowns? / Degree? F1(2)…F5(6) | presente | eq, flow |
| E5 | EXIT torna indietro di una schermata | presente | eq, flow |
| E6 | REPEAT conserva i coefficienti | presente | eq |
| E7 | SET UP dalla modalità con ritorno alla modalità | presente (EXIT torna a EQUATION, soluzioni ricalcolate) | eq |
| E8 | Dati conservati spegnendo | presente (record v8) | eq, storage_test |

### 4.2 SIML

| ID | Voce | Stato | Prova |
|---|---|---|---|
| S1 | 2…6 incognite | presente | eq |
| S2 | Intestazione `aₙX+bₙY+cₙZ=dₙ` | presente; 4–6 incognite `X1…X6`: **inevitabile** (manuale senza esempio) | screenshot 63 |
| S3 | Griglia `a b c d`, righe numerate, parentesi, cella evidenziata, valore in basso a destra | presente (4 colonne visibili con scorrimento: **inevitabile**, 320×320) | screenshot 63 |
| S4 | EXE e ordine a1→b1→c1… | presente | eq, flow |
| S5 | Frazioni e variabili come coefficienti (forma esatta) | presente | eq |
| S6 | EXIT annulla l'immissione | presente | eq |
| S7 | Modifica spostandosi sulla cella | presente | eq |
| S8 | F3 CLEAR | presente | eq |
| S9 | F2 DELETE | presente | eq |
| S10 | F4 EDIT | presente | eq, screenshot 65 |
| S11 | F1 SOLVE | presente | eq |
| S12 | Schermata delle soluzioni | presente | screenshot 64, 66 |
| S13 | Soluzioni esatte in Math | presente | eq |
| S14 | Decimali in Line | presente | eq |
| S15 | No Solution | presente | eq, screenshot 67 |
| S16 | Infinitely Many Solutions con la formula | presente (esempio del manuale identico) | eq, screenshot 68 |
| S17 | Ma ERROR fuori intervallo (e Underflow) | presente | eq |
| S18 | Coefficienti e soluzioni complessi | presente | eq |
| S19 | Complex Mode a+bi / r∠θ | presente | eq |
| S20 | Inversione/eliminazione con 15 cifre interne, 10 mostrate | presente | eq (Hilbert 6×6, coefficienti 1E50) |

### 4.3 POLY

| ID | Voce | Stato | Prova |
|---|---|---|---|
| P1 | Grado 2…6 | presente | eq |
| P2 | Intestazione `aX³+bX²+cX+d=0` | presente | screenshot 08 |
| P3 | Celle su una riga | presente | screenshot 69 |
| P4 | Frazioni e variabili | presente | eq |
| P5 | EXE / EXIT / modifica / CLEAR | presente | eq |
| P6 | DELETE, EDIT | presente | eq |
| P7 | a ≠ 0 (Ma ERROR) | presente | eq |
| P8 | Soluzioni X1…Xn | presente | screenshot 08 |
| P9 | Ordine del manuale | presente | eq, flow |
| P10 | ×k | presente | eq, screenshot 69 |
| P11 | Real: solo radici reali | presente; nessuna radice reale → "No Solution": **inevitabile** (il manuale non lo mostra; messaggio della tabella α-3) | eq |
| P12 | a+bi | presente | eq |
| P13 | r∠θ, esatto `1∠π` | presente | eq, screenshot 71 |
| P14 | Radici esatte, `(−3+√11 i)/2` | presente | eq, screenshot 70 |
| P15 | Errore senza soluzione, Underflow | presente | eq |
| P16 | Coefficienti complessi | presente | eq |

### 4.4 SOLVER

| ID | Voce | Stato | Prova |
|---|---|---|---|
| V1 | Input naturale | presente | screenshot 72 |
| V2 | x = X | presente | eq |
| V3 | Senza "=": = 0 | presente | eq |
| V4 | Più di un "=": errore | presente | eq |
| V5 | Tabella delle variabili | presente (A–Z; r = memoria R, θ non variabile: **inevitabile**, modello delle variabili del firmware) | eq, screenshot 73 |
| V6 | Lower / Upper | presente | eq |
| V7 | Variabile scelta + F6 SOLVE | presente | eq |
| V8 | F1 RECALL, F2 DELETE | presente; significato interpretato: **inevitabile** (il manuale non li descrive) | eq |
| V9 | `Eq:`, `V=`, `Lft=`, `Rgt=`, REPEAT | presente | eq, screenshot 74 |
| V10 | Newton dal valore della variabile | presente | eq |
| V11 | Retry | presente | eq |
| V12 | Can't Solve! | presente | eq |
| V13 | Time Out | presente | eq |
| V14 | No Variable | presente | eq |
| V15 | Variabile risolta memorizzata | presente | eq |
| V16 | Esempio H=VT−½GT² → 16.8, 14, 14 | presente | eq, screenshot 74 |
| V17 | Una sola soluzione | presente | eq |
| — | Interruzione con AC durante il calcolo | **inevitabile**: il calcolo è limitato (`kSolverEvaluations`) e termina con Time Out | eq |

### 4.5 Visualizzazione e uso dei risultati

| ID | Voce | Stato | Prova |
|---|---|---|---|
| D1 | Forme naturali con i modelli di Run-Matrix | presente | eq |
| D2 | Frac Result d/c, ab/c | presente | eq |
| D3 | Norm / Fix / Sci / Eng | presente | eq |
| D4 | Risultati lunghi scorrevoli | presente | eq |
| D5 | S⇔D | presente (su F5: **inevitabile**, nessun tasto S⇔D sulla PicoCalc) | eq |
| D6 | VARS › SimRes, SimCoef, PlyRes, PlyCoef in MatAns | presente | eq, screenshot 75 |
| D7 | Rref con No/Infinite | presente | eq |
| D8 | Errore senza coefficienti o soluzioni | presente | eq |
| D9 | SimRes e SimCoef non insieme | presente | eq |

### 4.6 Messaggi

| ID | Messaggio | Stato |
|---|---|---|
| M1 | Syntax ERROR | presente |
| M2 | Ma ERROR | presente |
| M3 | Stack ERROR | presente (motore condiviso) |
| M4 | Non-Real ERROR | presente |
| M5 | Can't Solve! | presente (titolo della finestra di errore) |
| M6 | Time Out | presente |
| M7 | Underflow | presente |
| M8 | Infinitely Many Solutions / No Solution / No Variable | presente |
| M9 | Retry | presente |

### 4.7 Estensioni

| ID | Funzione | Stato |
|---|---|---|
| X1 | Disequazioni polinomiali 1…6 | mantenuta (F4 INEQ, segnata come estensione; screenshot 76) |
| X2 | `solveInequalitySystem` | API di libreria, segnata come estensione (eq) |

### 4.8 Differenze inevitabili

Schermo 320×320 (griglia di 4 colonne scorrevole, valore corrente su una riga
dedicata); tastiera della PicoCalc (modelli su F1–F6 durante l'immissione,
S⇔D su F5, SET UP su F10); nomi `X1…X6` da 4 incognite; "No Solution" per
POLY senza radici reali nel modo Real; RECALL/DELETE del Solver interpretati;
variabili del Solver A–Z; nessuna interruzione con AC (calcolo limitato). Le
strip di eActivity e i comandi di programma `Sim Coef`… sono fuori perimetro
(eActivity e Program non sono realizzati).

## 5. Funzionalità e file

| Requisito | Realizzazione | File |
|---|---|---|
| 4.1 Menu e schermate | `EquationApp` riscritta: Select Type, scelta, coefficienti, soluzioni, tabella e risultato del Solver | `src/apps/equation_app.cpp`, `include/apps.hpp` |
| 4.2 Coefficienti | `eq::Number` (valore + `ExactComplex`), `evaluateNumber`, `editText`; editor `MathField` | `include/equation.hpp`, `src/equation.cpp`, `src/apps/math_field.cpp` |
| 4.3 SIML | `solveSimultaneous`: Rref esatta o in virgola mobile, formula delle infinite soluzioni, decimali per elemento | `src/equation.cpp` |
| 4.4 POLY | `solvePolynomial`: Yun, radici razionali, quadratiche e biquadratiche esatte, Aberth complesso, molteplicità, ordine | `src/equation.cpp` |
| 4.5 SOLVER | `solverVariables`, `solveFor` (Newton + bisezione, Lower/Upper, messaggi) | `src/equation.cpp` |
| 4.6 Uso dei risultati | `CollectionResolver::Memory::Equation`, nomi nel lexer e nell'editor, VARS di Run-Matrix, regola SimRes/SimCoef | `include/expression.hpp`, `src/expression.cpp`, `src/natural_editor.cpp`, `src/workspace.cpp`, `src/run_matrix.cpp`, `src/apps/run_matrix_app.cpp` |
| 4.7 Visualizzazione | `numberText`, `numberTree`, `formulaTree`; alberi di Run-Matrix resi pubblici | `src/equation.cpp`, `include/run_matrix.hpp`, `src/run_matrix.cpp` |
| 4.8 Estensioni | INEQ mantenuta, `solveInequalitySystem` documentata | `src/apps/equation_app.cpp` |
| Comune | SET UP con ritorno alla modalità, titoli degli errori della fx-CG50, servizio EquationCore | `src/app_manager.cpp`, `include/app.hpp`, `src/apps/system_app.cpp`, `src/app_registry.cpp`, `include/app_registry.hpp` |
| Dati | record v8, `limits.hpp` | `include/workspace.hpp`, `src/workspace.cpp`, `include/limits.hpp` |

**Difetti preesistenti trovati e corretti** (dal simulatore, durante la
prova del Solver): nell'editor Math un modello di frazione seguito da un
fattore implicito (`½X`, `½GT²`) veniva serializzato `(1)/(2)X` e letto come
1/(2X) in tutte le modalità; ora `((1)/(2))X`, reimportato come frazione. Il
modello x²/^ prendeva come operando tutte le lettere precedenti (`GT²` =
(GT)²); ora il numero o la sola variabile prima del cursore (con `Mat A`,
`List 12` interi). Le espressioni salvate prima conservano il loro testo.

Comportamenti cambiati volutamente (test adattati): schermate e nomi di
EQUATION; radici in ordine decrescente; nel modo Real solo le radici reali;
F5 REL per le disequazioni; SOLVER senza valore iniziale separato; il SET UP
aperto da una modalità torna alla modalità; record alla versione 8.

## 6. Test

| Verifica | Risultato |
|---|---|
| Test host, profilo Pimoroni Pico Plus 2 W | 41/41 test, **10 316** controlli nelle 39 suite C++ + ≈ 333 000 dello stress dell'allocatore + 41 dello script FAT/exFAT, 0 errori |
| Test host, profilo Raspberry Pi Pico 2 W | 41/41 test, **10 310** controlli + ≈ 334 000 + 41, 0 errori |
| AddressSanitizer + UBSan (entrambi i profili) | 41/41 su entrambi i profili, 0 errori |
| ThreadSanitizer (allocatore su due thread, entrambi i profili) | 0 errori |
| Relay AI | 15/15 |
| Simulatore | nuovo script `tests/sim/equation.txt`, screenshot 08 (rigenerato) e 62–76, compressi senza perdita (palette PNG, `compress_level` 9, pixel verificati: 36 KB) |
| **Hardware fisico** | **non eseguito**: nessuna PicoCalc disponibile in questa sessione; procedura [HARDWARE_TEST_v1.12.0.md](../HARDWARE_TEST_v1.12.0.md) (HW-120…HW-134) |

Nuova suite `equation_v112_test` (244 controlli): un gruppo per ogni
punto del §4 della richiesta — coefficienti (frazioni, radicali, π, A–Z, Ans,
complessi esatti, Non-Real, Type, EDIT, conversione dei double), SIML
(esempio del manuale, frazioni, radicali e π, Hilbert 6×6 esatta e decimale,
2–6 incognite, coefficienti 1E50, No Solution, Infinitely Many Solutions
anche con l'esempio complesso del manuale, r∠θ, Ma ERROR/Underflow, Fix/Sci/Eng,
S⇔D, frazioni miste), POLY (esempi del manuale 4-3, 4-4, 1-29, ordine, ×k,
radicali, biquadratiche, grado 6, x⁶−1, decimali, irriducibili, radice doppia
irrazionale, radici vicine distinte, coefficienti complessi, costanti 10⁹,
a = 0, Underflow), SOLVER (esempio del manuale per V e per T, x = X, senza
"=", Lower/Upper, Can't Solve!, Time Out, No Variable, Syntax ERROR, polo,
Retry), schermate e tasti sul firmware (menu, EXE/EXIT, EDIT, SOLVE, REPEAT,
CLEAR, frazione F1, S⇔D, Syntax ERROR con l'immissione aperta, Line, ×3,
Complex Mode dal SET UP con ritorno, No Solution, Ma ERROR, Solver completo
con RECALL, Lower/Upper, DELETE, No Variable), VARS › EQUATION (errori,
SimRes esatto, MatAns, SimRes×6, SimRes+SimCoef, Rref, PlyRes, PlyCoef,
Trn), record v8 (round trip, troncamenti, indicatori non validi) e
migrazione v7→v8 dal fixture, correzioni dell'editor, estensioni.
`architecture_test` più severo (§3); `apps_flow_test`, `storage_test`,
`table_test`, `matrix_test`, `lists_csv_test`, `board_profile_test` adattati
solo ai cambiamenti voluti (versione 8 del record, nuove schermate).

## 7. Compilazione del firmware

| | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Esito | compilato, 0 warning | compilato, 0 warning |
| Immagine flash | 1 679 688 byte (40 % di 4 MB; +84 732 rispetto alla v1.11.0) | 1 670 916 byte (79 % di 2 MB; +84 748) |
| RAM statica | 302 248 byte (−308); heap SRAM 191 320 byte | 298 324 byte (−308); heap SRAM 195 244 byte |
| SHA-256 dell'UF2 locale | `6f28c35d3744fc1d6b61a3634bf2308c5e0cf9bc055043b1a208bae8a48d1125` | `67fee0302167ee2d1fe98bfcd0fc661cc2d371966d5fa8ff4d4bd6fb608a1590` |

Pico SDK 2.2.0, `gcc-arm-none-eabi` 13.2.1, `-O2`, build pulite
(`scripts/build.sh --clean`); la versione `1.12.0 (pico_plus_2w)` / `1.12.0 (pico2w)` è nel binario (`picotool` non è installato in questo ambiente). Frame di stack (`-fstack-usage`): il massimo resta `SystemApp::enter` 1 840 B; codice nuovo: forma esatta dei complessi 784 B (tabelle sull'heap; inline erano 1 928 B in `evaluateNumber`), `solveSimultaneous` 776 B, `solvePolynomial` 760 B, `numericPolynomial` 712 B; nessuna ricorsione nuova. Nessun pin, driver, partizione o script del
linker cambiato; EQUATION ha gli stessi limiti sulle due schede; il blocco v8
occupa da ≈ 1,2 KB (coefficienti nulli) a ≈ 4 KB nel record (59,75 KB sul
Pico 2 W, 511,75 KB sulla Plus). Gli UF2 della release sono quelli compilati
dal workflow sul commit pubblicato.

## 8. Pubblicazione

- Prima della pubblicazione `main` è stato ricontrollato: `origin/main` era ancora `f6933fd` (tag `v1.11.0`): nessun lavoro parallelo da integrare, nessuna rinumerazione necessaria.
- Un solo commit della v1.12.0 su `main` (fast-forward); i commit intermedi
  restano sul branch di sessione `ccr-bbf29b2b-0ipbnx`. Nessun force push.
- Tag e release dal workflow `Build, test e release` avviato a mano su
  `main` con *publish* = true (*tag_v190* = false): tag annotato `v1.12.0`,
  release stabile con le note di [../releases/v1.12.0.md](../releases/v1.12.0.md),
  UF2 delle due schede, zip dei sorgenti e `SHA256SUMS`.

## 9. Limitazioni note

- **Nessuna prova su hardware fisico** (HW-120…HW-134).
- Forme esatte complesse solo per espressioni polinomiali in `i`
  (`1/(1+i)` resta decimale); radici cubiche e fattori irriducibili di grado
  ≥ 3 in virgola mobile; quartiche non pari non scomposte in forma esatta.
- Variabili del Solver A–Z (niente r e θ separati); nessuna interruzione con
  AC.
- Interfaccia per i sistemi di disequazioni non realizzata (estensione).
