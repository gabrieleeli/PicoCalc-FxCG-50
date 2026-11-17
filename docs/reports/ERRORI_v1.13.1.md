# Errori trovati — v1.13.1 (versione di correzione)

Base: `main` = tag `v1.13.0` = commit `a72d901` (verificato: `main` coincide
con il tag). All'inizio della sessione l'ultima release pubblicata era la
v1.11.0 e `main` era già alla v1.12.0 (`51691de`, release in corso); durante
la ricerca degli errori `main` è avanzato alla v1.13.0 (sessione parallela,
pubblicata alle 22:35 UTC del 16/11/2026): la ricerca è stata ripetuta sulla
v1.13.0 e ogni errore qui sotto è riprodotto su quei sorgenti.

Nessuna nuova funzione: solo correzioni di errori, incoerenze e documentazione.
Ogni errore ha un gruppo di controlli in `tests/fixes_v1131_test.cpp` che
fallisce con i sorgenti della v1.13.0 e passa con le correzioni (esito sui
sorgenti della v1.13.0: 29 controlli falliti su 90 per E01–E09; il test non
termina per E02 con n e M grandi e per E14; E10 ed E12 fermati da UBSan; E15
restituisce 1000000 invece dell'errore; E16 1 controllo ed E17 12 controlli
falliti; dopo le correzioni 142/142).

Gravità: **critica** (dati persi o firmware inutilizzabile in uso normale),
**alta** (perdita di dati o blocco del dispositivo in casi raggiungibili),
**media** (risultato errato, errore sbagliato o memoria/tempo eccessivi),
**bassa** (casi limite, precisione, documentazione).

## Elenco

| Codice | Gravità | Modulo | Come si riproduce | Causa | Correzione | Stato |
|---|---|---|---|---|---|---|
| E01 | alta | `workspace.cpp` (lettura dei record v1–v8) | record v7 (fixture della v1.11.0) con una cella di SIML `NaN`, o con l'equazione del Solver oltre 255 caratteri, o una lista con `∞` (poi cambio di List File): il record si carica, ma quello salvato subito dopo è rifiutato all'avvio successivo → workspace azzerato e, con i due salvataggi seguenti, perso anche lo slot precedente | i blocchi vecchi non avevano i controlli dei blocchi v8/v9 (coefficienti finiti, limite del Solver, elementi finiti delle liste) | stessi controlli nei blocchi vecchi: un record che non si potrebbe rileggere dopo il salvataggio è rifiutato subito (il workspace resta intatto) | corretto, test E01 + fuzzing con il controllo di andata e ritorno |
| E02 | alta | `calc_library.cpp` `HypergeoCD`, `InvHypergeoCD` | `HypergeoCD(1E8,5,5,10)`: 0,9 s sul PC, minuti sulla PicoCalc; `HypergeoCD(1E9,1E9,1E9,2E9)` e `InvHypergeoCD(0.5,1E9,1E9,2E9)` non terminano (nessun AC durante il calcolo) | la somma andava da 0 a x senza limitarsi al supporto [max(0,n+M−N), min(n,M)] né al numero di termini | somma solo sul supporto, al più 10⁶ termini (come `BinomialCD` n ≤ 10⁶ e `PoissonCD` x ≤ 10⁶), oltre: Argument ERROR | corretto, test E02 |
| E03 | media | `calc_library.cpp` `InvBinomialCD`, `InvPoissonCD`, `InvHypergeoCD` | `InvBinomialCD(0.5,20000,0.5)` 2,5 s sul PC (minuti sulla PicoCalc), cresce con n² | per ogni candidato x si ricalcolava la somma da 0 | somma cumulativa accumulata una sola volta, stesse somme nello stesso ordine (stessi risultati) | corretto, test E03 (anche contro la definizione) |
| E04 | media | `equation.cpp` (coefficienti complessi esatti) | coefficiente `1E10+Abs(i)` memorizzato come `1E+10+i` (valore 10000000001); `1E10+Conjg(i)` come `1E+10+i` | la forma esatta per sostituzione di `i` con interi vale solo per polinomi in `i`; il controllo finale confrontava il modulo della differenza con 10⁻⁹·|z| | sostituzione solo se l'espressione è un polinomio in `i` (somme, prodotti, divisioni per costanti, potenze naturali); confronto per parte reale e immaginaria | corretto, test E04 |
| E05 | media | `expression.cpp` `nCr`, `nPr` | `nCr(1000000,2)` → Argument ERROR | limite n ≤ 100000 non della fx-CG50 (manuale, intervalli di input: 0 ≤ r ≤ n < 1E10, risultato < 1E100) | limite n < 1E10, ciclo fermato a 1E100 (Math ERROR) | corretto, test E05 |
| E06 | media | `csv_io.cpp` `decode` | CSV di 40 000 virgole o 20 000 righe (entro `maxFileBytes` del Pico 2 W): una stringa per campo e per riga, centinaia di KB (più dello heap del Pico 2 W) prima del Dimension ERROR; una matrice da file allocava tutte le celle prima del controllo di 10 000 / 400 elementi | righe e campi copiati prima di controllare le dimensioni | due passate: righe e colonne contate senza copiare, controlli (anche delle celle, `maxCells`) prima di allocare; stesso formato, stessi errori | corretto, test E06 (byte allocati misurati) |
| E07 | media | `file_store.cpp` `writeAtomic` | salvataggio interrotto dopo "file → .bak" (restano `.tmp` completo e `.bak`), poi un secondo salvataggio interrotto mentre scrive il `.tmp`: alla lettura `recover()` promuove il `.tmp` parziale e il file vecchio è perso | `writeAtomic` riscriveva il `.tmp` senza completare prima il salvataggio interrotto | `writeAtomic` completa prima il salvataggio interrotto dello stesso file (stessa funzione di `recover()`) | corretto, test E07 |
| E08 | bassa | `statistics.cpp` `tInterval`, `calc_library.cpp` `InvTCD` | `tInterval` con 2 dati e C-Level 0.99999: intervallo largo 1000 invece di 63 662; `InvTCD(0.9999999,1)` = −10000 invece di −3 183 098,862; `InvTCD(0.5,1)` = −1,05E−08 invece di 0 | ricerca limitata a [−1000, 1000] / [−10⁴, 10⁴] | simmetria della distribuzione t e intervallo allargato finché serve | corretto, test E08 |
| E09 | bassa | `statistics.cpp` `oneVariable` con frequenze | 3 dati con frequenza 100 000: 300 000 double copiati e ordinati (2,4 MB; fino a 10⁸ con 999 dati); funzione non usata dall'interfaccia | i dati venivano ripetuti in memoria | calcolo pesato senza ripetere i dati (stessi risultati); il percorso senza frequenze è invariato | corretto, test E09 |
| E10 | bassa | `expression.cpp` `Ran# n` | `Ran# 2E13` (trovato dal fuzzing): conversione di un double fuori intervallo in `int` (comportamento indefinito, UBSan) | controllo di 0 ≤ n ≤ 10 dopo la conversione | controllo prima della conversione | corretto, test E10 |
| E11 | media | `tests/CMakeLists.txt` (build `--sanitize`) | un comportamento indefinito durante i test veniva solo stampato da UBSan e il test risultava superato (anche nel workflow) | mancava `-fno-sanitize-recover=undefined` | UBSan si ferma al primo errore (verifica più severa, nessun test indebolito) | corretto; tutti i test superati con UBSan bloccante |
| E12 | media | `expression.cpp` Σ / Π | `Σ(x,x,1E19,1E19)`: conversione indefinita in `long long`; sulla PicoCalc (conversione saturata a 9,2E18) il contatore `k+=passo` traboccava: ciclo senza fine | estremi convertiti in `long long` | si contano i termini (al più 100 001) e la variabile vale `a + i·passo` | corretto, test E12 |
| E13 | bassa | `docs/TESTING.md` | intestazione con "39 suite; 10 316 controlli" (v1.12.0) e riga "Totale" con 9 757 / 9 751 (v1.11.0), mentre la v1.13.0 ha 43 suite e 12 081 / 12 075 controlli; tabella per suite non aggiornata | documento aggiornato solo in parte | numeri rigenerati dai test eseguiti | corretto (§5) |
| E14 | alta | `calc_library.cpp` `InvGeoCD`, `GeoCD`, `GeoPD` | `InvGeoCD(0.5,1E-20)` non termina; `InvGeoCD(0.5,1E-12)` 0,27 s sul PC; `GeoCD(1E20,1E-20)` = 0 invece di 0,632; `GeoPD(1E20,1E-20)` = 1E−20 invece di 3,68E−21 | `(1−p)^k` con p < 1E−16 vale 1 in double; oltre 2⁵³ `x+1 = x` | potenze con `log1p`/`expm1`; inversa dalla formula chiusa con al più 64 passi di correzione | corretto, test E14 |
| E15 | media | `calc_library.cpp` `InvPoissonCD` | `InvPoissonCD(0.5,1E9)` = 1000000 (risultato errato, senza errore) | senza un x ≤ 1E6 si restituiva il limite della ricerca | ricerca in [0, 1E6]; oltre, Argument ERROR | corretto, test E15 |
| E16 | bassa | `calc_library.cpp` `logChoose` (`HypergeoPD`) | `HypergeoPD(999999997,1E9,1E9,1E9+3)` = 1,000004 (una probabilità oltre 1; valore esatto 0,999999991) | differenza di tre `lgamma` dell'ordine di 2E10: si perdono ~1E−5 | con min(k, n−k) ≤ 64 il coefficiente binomiale si calcola termine per termine | corretto per questo caso; resta aperto il caso con entrambi i termini grandi (A2) |
| E17 | bassa | `calc_library.cpp` (nomi), VARS di Run-Matrix | VARS › V-WIN mostrava `xmin = -6.3`, `tptch`, VARS › TABLE `tblstart`: nomi interni (la v1.12.0 e la fx-CG50: Xmin, Tptch, TblStart) | il menu della v1.13.0 usa `calclib::displayName`, che non conteneva queste variabili | nomi aggiunti a `displayName` (gli stessi del renderer) | corretto, test E17, screenshot 17 |
| E18 | bassa | `tests/sim/run_matrix.txt`, `run_matrix_v111.txt`, `docs/screenshots` | gli script del simulatore usavano le posizioni di OPTN della v1.12.0: nella v1.13.0 producevano input sbagliati (13, 58, 59, 60, 61: `b0.16…`, `999≤+25≤…`, Syntax ERROR); 9 screenshot non corrispondevano al firmware | OPTN riordinato nella v1.13.0 senza aggiornare gli script e gli screenshot | percorsi aggiornati (ESYM voce 11, ▶Simp in CALC, BASE-N e RISULTATO); 13, 58, 59, 61 tornano identici agli screenshot pubblicati; 17, 18, 51, 55, 60 rigenerati (cambi voluti della v1.13.0 e E17), compressi senza perdita | corretto |

## Problemi rimasti aperti

| Codice | Modulo | Descrizione | Motivo per cui resta aperto |
|---|---|---|---|
| A1 | `firmware.cpp` (avvio) | se il record più recente supera il CRC ma non si legge (ad esempio dopo il ritorno a una versione più vecchia del firmware), si parte con il workspace vuoto e i salvataggi successivi sovrascrivono anche lo slot precedente | E01 elimina il caso raggiungibile con questa versione; per il ritorno a un firmware precedente serve una decisione di progetto (bloccare i salvataggi o leggere lo slot più vecchio) e una prova di interruzione su hardware |
| A2 | `calc_library.cpp` `logChoose` | con min(k, n−k) > 64 e n dell'ordine di 1E9 `HypergeoPD` ha un errore relativo fino a ~1E−5 (differenza di `lgamma`) | serve un algoritmo diverso (deviance di Loader, "saddle point"): una nuova implementazione numerica, fuori da una versione di correzione |
| A3 | Run-Matrix | risultati con \|x\| < 1E−99 (es. `2^-400`) sono mostrati (3.872591915E-121) | il manuale indica l'intervallo di calcolo ±1E−99…±9,999999999E+99 ma non dice se in Run-Matrix un risultato più piccolo diventa 0 o errore: da verificare su una fx-CG50 reale |
| A4 | `docs/screenshots` 01–05, 09–11 | screenshot storici (v1.5–v1.7) non prodotti da alcuno script attuale del simulatore | non si possono rigenerare né confrontare automaticamente; restano come documentazione storica |
| A6 | `equation.cpp` `solveFor` (SOLVER di EQUATION) | un'equazione con SolveN, FMin o FMax (trovata dal fuzzing: `SolveN(min−x−6)`) arriva a Time Out dopo 4 000 valutazioni, ciascuna con una ricerca numerica: 5,9 s sul PC, minuti sulla PicoCalc senza AC | il manuale vieta queste funzioni dentro Solve/SolveN/FMin/FMax di Run-Matrix (2-28) ma non lo dice per il SOLVER di EQUATION: va verificato su una fx-CG50; in alternativa serve un limite di tempo con l'orologio della piattaforma nel motore (una funzione nuova) |
| A5 | collaudo fisico | nessuna prova su PicoCalc (né Pico Plus 2 W né Pico 2 W) | **nessun esito hardware** fornito; procedure in `docs/HARDWARE_TEST_v1.13.1.md` |

## Verifiche senza errori

| Verifica | Esito |
|---|---|
| Test host (v1.13.0 di partenza) | 45/45 per profilo, 12 081 / 12 075 controlli |
| cppcheck 2.13 (`warning,performance,portability`, tutti i sorgenti) | 10 segnalazioni, nessun errore: formati `snprintf` con argomenti in più (diagnostica), `std::abs` su double scambiato per int, accesso dopo `std::move` protetto dalla garanzia forte di `push_back`, condizioni ripetute e passaggi per valore |
| GCC con `-Wall -Wextra -Wshadow -Wconversion -Wdouble-promotion -Wnull-dereference -Wduplicated-cond -Wlogical-op` (tutti i sorgenti, solo sintassi) | 55 avvisi distinti, nessun errore: 26 `-Wshadow` (variabili di ciclo e lambda volute, parametri dei costruttori), 25 `-Wconversion` (conteggi in double, coordinate in pixel < 2²⁴, byte del solo simulatore), 4 `-Wdouble-promotion` |
| clang-tidy 18 (`clang-analyzer-*`, `bugprone-*`) su 19 moduli del motore | 49 segnalazioni valutate: 40 `misplaced-widening-cast` (indici ≤ 42), virgola "mancante" che è una concatenazione voluta, due divisioni per zero non raggiungibili (denominatori ≥ 1, colonne ≥ 1), una scrittura inutile; i cicli con contatore `double` hanno portato a E02/E14/E15, gli altri sono limitati (≤ 1E6) |
| Fuzzing con libFuzzer + ASan/UBSan (Pico Plus 2 W) | vedi il rapporto di sviluppo: parser e valutatori, workspace (con andata e ritorno), CSV, `.pcgraph`, EQUATION, sequenze di tasti dell'intero firmware; trovati E01 (due varianti) ed E10; dopo le correzioni nessun crash; un timeout del SOLVER di EQUATION (A6) |
| Simulatore | 7 script, 77 schermate: dopo E18 identiche agli screenshot pubblicati; nessun testo tagliato o sovrapposto nelle schermate esaminate |
| Confronto con il manuale fx-CG50 | esempi del manuale nelle suite di Run-Matrix, EQUATION, GRAPH, TABLE e Statistics rieseguiti; controlli puntuali: overflow ≥ 1E100 e 69! corretti dalla v1.13.0; differenze trovate: E05 |
| Documentazione | ROADMAP, BUILD_STATUS, FEATURE_MATRIX, README, CONTINUE della v1.13.0 coerenti con il codice; differenze trovate: E13, E18 |
