# Program — guida (v1.20.0)

Solo **Pimoroni Pico Plus 2 W**. MENU › **B**. Al posto del Casio Basic del
capitolo 8 della fx-CG50, Program esegue **sottoinsiemi interpretati di C,
C++ e C#** (Java arriva nella v1.21.0) scritti, salvati e organizzati come
**file sorgente standard sulla microSD**. Stato: **Base** (diventa Available
con Java, quando la tabella del capitolo 8 è completa per i quattro
linguaggi). Analisi e tabelle costrutto per costrutto:
[reports/ANALISI_FONTI_v1.20.0.md](reports/ANALISI_FONTI_v1.20.0.md);
scelte e differenze: [reports/SVILUPPO_v1.20.0.md](reports/SVILUPPO_v1.20.0.md);
librerie: [LIBRARIES.md](LIBRARIES.md); esempi completi:
[examples/program](examples/program/README.md).

## 1. Dove stanno i programmi

```
PICOCALC/CODE/C      .c  .h
PICOCALC/CODE/CPP    .cpp .hpp .h
PICOCALC/CODE/CS     .cs
PICOCALC/CODE/JAVA   (v1.21.0)
PICOCALC/CODE/PY     .py (gli script di Python, v1.22.0)
PICOCALC/LIB/<LING>/<libreria>   librerie installate (LIBRARIES.md)
```

- Un programma è **un file sorgente qualsiasi**, senza righe aggiunte: si
  apre e si compila uguale sul PC. Un **progetto** è una cartella: RUN di
  una cartella (o di un suo file) compila tutti i sorgenti del linguaggio
  della cartella e delle sottocartelle (al più 64 file).
- La flash interna tiene solo le impostazioni, i file recenti e la posizione
  del cursore (record del workspace). **Senza microSD** Program mostra
  «Inserire la scheda SD»; con la SD piena «SD piena»; mai un crash.
- **Salvataggi atomici**: si scrive un file temporaneo, si verifica, poi si
  rinomina. Una SD tolta durante il salvataggio lascia il file vecchio
  intatto. Gli stessi salvataggi atomici valgono per i file scritti dai
  programmi (scritti alla chiusura del file o alla fine del programma; dopo un
  errore o AC sono scartati).
- **Migrazione** dalla v1.17.0: i `.pcprog` di `PICOCALC/PROG` (flash o SD)
  e gli script di `PICOCALC/PY` diventano sorgenti standard nelle cartelle
  del loro linguaggio (`HELLO.pcprog` con `// pcprog: C++` →
  `CODE/CPP/HELLO.cpp`). Program la propone alla prima apertura (menu
  *Converti in PICOCALC/CODE (originali tenuti)* / *Converti ed elimina gli
  originali dalla flash* / *Non chiedere più*; EXIT la ripropone al prossimo
  ingresso); si rilancia con **MIGR** (pagina 3 dell'elenco o SET UP).

## 2. Gestore dei file

Elenco della cartella con le sottocartelle tra parentesi, dimensione dei
file, ordinamento per nome, tipo o dimensione. Lettera = salto al primo nome
che inizia così; EXE apre una cartella o esegue un file; EXIT sale di una
cartella.

| Pagina | F1 | F2 | F3 | F4 | F5 |
|---|---|---|---|---|---|
| 1 | **EXE** (RUN) | **EDIT** | **NEW** (file, progetto, cartella) | **DEL** (conferma) | **DEL·A** (tutto il contenuto della cartella, conferma) |
| 2 | **SRC** (SEARCH: nomi che iniziano con il testo, anche nelle sottocartelle) | **REN** | **COPY** | **MOVE** | **SORT** |
| 3 | **PASS** (password) | **LIB** | **SETUP** | **DEBUG** | **MIGR** |

- **NEW › File** crea un file dal modello del linguaggio (`main` che stampa
  «Ciao»); **NEW › Progetto** una cartella con `main`, un modulo e la sua
  intestazione (C/C++) o due classi (C#).
- **Nomi**: 1–40 caratteri `A-Z a-z 0-9 _ - .` (la fx-CG50 ne ammette 8
  maiuscoli, differenza D2).
- **Password** come sulla fx-CG50: chiesta per EDIT (e per toglierla), non
  per RUN; una password vuota la toglie. Si conserva come impronta SHA-256 in
  `PICOCALC/CODE/.passwords` e segue il file in REN/MOVE/COPY; protegge
  l'interfaccia, non il file sul PC (D4).

## 3. Editor

Numeri di riga, **evidenziazione** (parole chiave, tipi, stringhe, numeri,
commenti, direttive), **rientro automatico** dopo `{` e `}` riallineata,
**parentesi corrispondente** evidenziata, cursore con la colonna nella barra.

| Pagina | F1 | F2 | F3 | F4 | F5 |
|---|---|---|---|---|---|
| 1 | **RUN** (salva ed esegue) | **SAVE** | **FIND** (SHIFT+FIND: successivo) | **REPL** (sostituisci tutto) | **JUMP** (riga) |
| 2 | **UNDO** | **REDO** | **SAV·AS** | **BRK** (punto d'interruzione sulla riga) | **DEBUG** |
| 3 | **TOP** | **BTM** | **ERR** (cursore sull'ultimo errore, anche in un altro file) | | |

Tastiera: Ctrl+S salva, Ctrl+Z/Ctrl+Y annulla/ripete, Ctrl+F trova,
Ctrl+frecce selezionano, Ctrl+C/X/V copia/taglia/incolla, Tab inserisce
quattro spazi; EXIT salva e torna all'elenco. Nelle domande (Trova,
Sostituisci, riga, password) il testo proposto si sostituisce scrivendo o si
cancella con DEL; in *Sostituisci con* il testo vuoto cancella le
occorrenze.

**File grandi**: fino a 256 KB il file si apre per intero; da 256 KB a
**4 MB** (limite dichiarato e provato da `program_flow_test`) si apre **a
blocchi** di righe intere: Pagina giù alla fine del blocco e Pagina su
all'inizio passano al blocco successivo o precedente (il blocco modificato
si salva con una riscrittura atomica del file). L'interprete esegue file fino
a 256 KB ciascuno e progetti fino a 1 MB.

## 4. RUN, input, errori

- **RUN** (F1 dell'editor o EXE nell'elenco) salva, compila il sorgente
  dalla SD e lo esegue **a passi**: l'interfaccia resta viva, **AC = Break**
  in ogni momento (anche durante l'input o in pausa). L'uscita scorre sul
  display; F6 **GRAPH** mostra i disegni della libreria `fx`; F2 **EDIT**
  torna all'editor.
- **Input**: la riga con «?» raccoglie una riga di testo (`scanf`, `cin`,
  `Console.ReadLine`); **Ctrl+D** chiude l'input (EOF: `scanf` restituisce
  `EOF`, `cin` diventa falso, `ReadLine` restituisce `null`).
- **Errori** con file, riga e colonna, nel nome della fx-CG50 (Syntax, Math,
  Dimension, Memory, Nesting, Argument, Go…) o dell'eccezione del linguaggio
  (`NullReferenceException`, `std::out_of_range`…): la finestra d'errore,
  poi EXIT porta l'editor sul punto dell'errore con la riga evidenziata.

## 5. DEBUG (estensione, D8)

DEBUG (F5 della pagina 2 dell'editor o F4 della pagina 3 dell'elenco) esegue
il programma in pausa prima della prima istruzione di `main`. La schermata
mostra la funzione, il file e la riga **che sta per essere eseguita**, sette
righe di sorgente intorno (con i punti d'interruzione in rosa) e le variabili
della funzione.

| F1 STEP | F2 OVER | F3 OUT | F4 CONT | F5 VARS | F6 STOP |
|---|---|---|---|---|---|
| alla prossima riga, anche dentro le funzioni chiamate | alla prossima riga della stessa funzione | torna al chiamante (sulla riga della chiamata) | fino al prossimo punto d'interruzione | variabili locali e globali (`::nome`), oggetti, array e contenitori espandibili con EXE | ferma il programma |

Un **punto d'interruzione** (BRK sulla riga nell'editor) scatta quando
l'esecuzione **entra** nella riga: un ciclo scritto su una riga sola si ferma
una volta, il corpo di un ciclo su più righe a ogni giro, una funzione
ricorsiva in ogni chiamata. AC in pausa ferma il programma e chiude i file.

## 6. I linguaggi

Tabelle complete (LK, LC, LP, LS) in
[ANALISI_FONTI_v1.20.0.md §4](reports/ANALISI_FONTI_v1.20.0.md). In breve:

- **C (C11)**: tipi interi a 8–64 bit (modello LP64, overflow con
  avvolgimento, D9), `float`/`double`, array fino a 4 dimensioni e a
  lunghezza variabile con **controllo dei limiti**, struct, enum, typedef,
  **puntatori controllati** (NULL, oltre la fine, dopo `free` → errore),
  puntatori a funzione (anche alle funzioni della libreria: `integra(sin, 0,
  M_PI)`), preprocessore, `stdio.h` (printf/scanf completi, file), `stdlib.h`
  (malloc/calloc/realloc/free controllati, qsort, bsearch, rand), `string.h`,
  `ctype.h`, `math.h`, `time.h`, `limits.h`, `float.h`, `stdint.h`,
  `stdbool.h`. `strcmp`/`memcmp` restituiscono la differenza dei byte come
  glibc.
- **C++ (C++17)**: classi con accesso controllato, costruttori (liste di
  inizializzazione, delega, copia), distruttori a fine blocco, ereditarietà
  semplice e `virtual`, overload di funzioni e operatori (anche `friend
  operator<<`), riferimenti, namespace, **template** di funzione e di classe
  (anche con parametri interi e con lambda come argomenti), `auto`, range-for,
  binding strutturati, `enum class`, lambda con catture, eccezioni;
  `std::string`, `iostream`/`iomanip`, `fstream`/`sstream`, `vector`, `map`,
  `set`, `unordered_map`/`unordered_set` (ordine d'inserimento, D10),
  `pair`, `stack`, `queue`, `deque`, `list`, `<algorithm>`/`<numeric>`
  essenziali. Un `ofstream` locale si scrive alla fine del suo blocco (come il
  distruttore).
- **C# (C# 10)**: classi, struct, proprietà, interfacce (anche
  `IComparable<T>`), ereditarietà, generici dell'utente con vincoli, enum,
  `switch` senza fall-through, nullable, lambda e delegati (`Func`, `Action`,
  `Predicate`, `Comparison`; anche `Math.Sin` come delegato), eccezioni .NET
  con i messaggi inglesi di .NET, stringhe UTF-16, `StringBuilder`,
  interpolazione con allineamento e formati, `List`, `Dictionary`,
  `HashSet`, `Queue`, `Stack`, `File`/`StreamReader`/`StreamWriter`
  confinati, `Math`, `Convert`, `Parse`/`TryParse`, `Random` con la sequenza
  di .NET. I numeri si scrivono con la **cultura invariante** (`3.5`,
  `1,234.50`, `Infinity`).
- **Non supportati** con un errore chiaro e la posizione: union, campi di
  bit, variadiche dell'utente, ereditarietà multipla, specializzazione e
  template variadici, smart pointer, `std::function`, LINQ, `async`, `record`,
  tuple, `switch` come espressione e il resto delle righe «non supportato»
  delle tabelle.

I file aperti dai programmi stanno **nella cartella del programma** (percorsi
relativi; `..` o assoluti → errore). Le librerie installate si usano con
`#include <nome.h>` (C/C++) o `using Namespace;` (C#).

## 7. Libreria `fx` (servizi della calcolatrice)

Stessi servizi nei tre linguaggi; i nomi seguono le convenzioni di ciascuno:
**C** `fx_nome` in minuscolo (`fx_getvar`, `fx_stat1var`), **C++**
`fx::nome` in camelCase (`fx::getVar`, `fx::stat1Var`), **C#**
`Fx.Nome` in PascalCase (`Fx.GetVar`, `Fx.Stat1Var`).

| Gruppo | C++ (esempi) |
|---|---|
| Calcolo | `calc("2+3*sin(1)")`, `calcExact("1/3+1/6")` (risultato esatto come testo), `getAns()` |
| Variabili, liste, matrici | `getVar('A')`, `setVar('A', v)`, `getList(n, i)`, `getList(n)` (vettore), `setList(n, i, v)`, `setList(n, vettore)`, `listSize(n)`, `getMat('A', r, c)`, `setMat(...)`, `matRows`, `matCols`, `getMatrix('A')`, `setMatrix('A', vv)` |
| Testo e tasti | `locate(col, riga, valore)`, `clrText()`, `getKey()`, `waitKey()`, `pause()` (◢, EXE prosegue) |
| Grafica (in Graph, F6) | `cls()`, `viewWindow(xmin, xmax, xscl, ymin, ymax, yscl)`, `plot(x, y)`, `line(x1, y1, x2, y2)`, `circle(x, y, r)`, `text(x, y, "s")`, `graphY("x^2")`, `vertical(x)`, `horizontal(y)`, `pxlOn/pxlOff/pxlChg/pxlTest(r, c)`, `setColor("Red")`, `drawGraph()`, `command("comando Run-Matrix")` |
| Statistiche | `stat1Var(lista)` o `stat1Var(vettore)` → `n mean sum sumSq popSd sampleSd min q1 median q3 max`; `stat2Var(...)` → `n meanX … sumXY minX maxX minY maxY`; `regression(tipo, x, y)` → `a b c d e r r2 n` con i tipi 0–11 della fx-CG50 (0 ax+b, 1 Med-Med, 2 quadratica, 3 cubica, 4 quartica, 5 log, 6 ae^bx, 7 ab^x, 8 potenza, 9 sin, 10 logistica, 11 a+bx) |
| Distribuzioni | `normPD/normCD/invNorm`, `tPD/tCD/invT`, `chiPD/chiCD/invChi`, `fPD/fCD/invF`, `binomPD/binomCD/invBinom`, `poissonPD/poissonCD/invPoisson`, `geoPD/geoCD/invGeo` (motore di OPTN › STAT › DIST) |
| Finanza | `tvmN/tvmI/tvmPV/tvmPMT/tvmFV`, `smplSI/smplSFV`, `npv/nfv/pbp/irr`, `amortBal/amortInt/amortPrn/amortSumInt/amortSumPrn`, `eff/apr`, `days` (motore di Financial) |

In C i risultati statistici sono struct (`FxStat1`, `FxStat2`, `FxReg`) e
le liste si passano come array con la lunghezza (`fx_stat1var_data(a, n)`);
in C# i campi sono in PascalCase (`s.Mean`, `r.A`).

## 8. SET UP

**Indice LIB**: indirizzo HTTPS dell'`index.json` delle librerie
(predefinito: la cartella `libraries/` di questo repository su GitHub);
**Ordine**: nome, tipo o dimensione; F5 **MIGR** ripete la migrazione.

## 9. Differenze dalla fx-CG50

D1 linguaggi al posto del Casio Basic (i comandi del capitolo 8 sono la
libreria `fx`); D2 nomi lunghi; D3 cartelle e progetti; D4 password nella SD;
D5 niente SAVE/LOAD in testo (i programmi sono già testo); D6 niente `Prog`
(include/using e librerie); D7 grafica e tasti della PicoCalc; D8 DEBUG come
estensione; D9 overflow con avvolgimento e LP64; D10 ordine d'inserimento
degli `unordered_*`; D11 nessun distruttore per i temporanei del C++; D12
`time()` dall'accensione. Dettagli in
[ANALISI_FONTI_v1.20.0.md §8](reports/ANALISI_FONTI_v1.20.0.md).

## 10. Come è provato

`program_lang_test` (un caso per costrutto, uscite di gcc/g++/.NET, debugger
DB-01…DB-07), `program_corpus_test` (51 programmi in
`tests/program_corpus` con le uscite prodotte da gcc, g++ e Mono, confrontate
byte per byte), `program_core_test`, `code_store_test`, `program_flow_test`
(dall'interfaccia: gestore dei file, editor, file grandi, password, DEBUG,
LIB), `libraries_test` (librerie del repository ed esempi), i fuzzer
`fuzz_c`, `fuzz_cpp`, `fuzz_cs`, `fuzz_program`, `fuzz_codestore` e gli
screenshot 203–223 del simulatore. Prove sul dispositivo:
[HARDWARE_TEST_v1.20.0.md](HARDWARE_TEST_v1.20.0.md).
