# Analisi delle fonti — v1.20.0: Program in C, C++ e C# sulla microSD e gestore delle librerie

> **Nota a fine sviluppo.** Questo documento è il piano scritto prima del
> codice e resta com'era. Gli scostamenti (editor a blocchi fino a 4 MB e non
> 8 MB, password in `CODE/.passwords`, nomi di 40 caratteri, campi
> `minFirmware` e indice con una voce per versione, limiti delle librerie,
> niente redirect HTTPS, nomi delle suite e dei fuzzer, SET UP ridotto) e le
> voci rimandate alla v1.21.0 sono in
> [SVILUPPO_v1.20.0.md §3](SVILUPPO_v1.20.0.md#3-scostamenti-dallanalisi).

Scritto **prima** del codice della v1.20.0 (fase 0). Obiettivo della versione:
scrivere sulla PicoCalc programmi veri in **C, C++ e C#**, interpretati dal
sorgente, salvati come file sorgente standard (`.c/.h`, `.cpp/.hpp`, `.cs`)
**sulla microSD** e riaperti, modificati e riusati quando servono; un
**gestore delle librerie** comune a tutti i linguaggi per installare sulla SD
librerie scaricabili e usarle dai propri programmi. La v1.21.0 aggiunge Java
sullo stesso nucleo, la v1.22.0 porta Python alla parità del capitolo 17 con
lo stesso archivio, lo stesso editor e lo stesso gestore delle librerie.

Regole che non cambiano ([CONTINUE.md](../../CONTINUE.md)): manuale fx-CG50
v3.60 solo come descrizione del comportamento (nessun codice, grafica o testo
copiato da Casio); **Raspberry Pi Pico 2 W congelato alla v1.16.0** (Program
non vi esiste: nessun byte nuovo nella sua immagine salvo la stringa della
versione); codice nuovo solo sulla **Pimoroni Pico Plus 2 W** dietro
`FXAI_BOARD_PICO_PLUS_2W` / `FXAI_MODE_PROGRAM`; cancello completo; un solo
commit su `main` in fast-forward («PicoCalc FxCG-50 v1.20.0: Program …»);
HARDWARE_TEST da **HW-296**; nessun «TESTED ON HARDWARE» senza prova fisica.
Le scelte tecniche hanno una risposta scritta qui (§9) senza chiedere
conferme, come richiesto. **Program resta Base** in questa versione: passa ad
Available solo nella v1.21.0, se la tabella del §3 è completa anche per Java.

## 1. Fonti

| # | Fonte | Uso |
|---|---|---|
| 1 | *fx-CG50 Software User's Guide* v3.60, capitolo 8 (Programming), pagine 8-1…8-14 (modalità Program; il resto del capitolo è il riferimento dei comandi Casio Basic) | descrizione del comportamento della **modalità**. Il PDF non è scaricabile da questo container (come nelle versioni precedenti); le pagine 297–310 della copia HTML di manualslib (manuale 3093535, pagina 8-N = 296+N) sono state lette attraverso un riassunto: i tasti del font Casio arrivano come glifi e alcuni numeri di tasto non sono certi. Nessun testo è stato copiato; i punti incerti sono segnati «(incerto)» e risolti nel §9 |
| 2 | Codice a `main` = `bcd28af` (v1.19.0) | punto di partenza (§1.1, §2) |
| 3 | Norme dei linguaggi come descrizione del comportamento: C11 (ISO/IEC 9899), C++17 (ISO/IEC 14882), C# 10 (ECMA-334 e la documentazione di .NET), semver 2.0.0, RFC 8259 (JSON), FIPS 180-4 (SHA-256), RFC 9110/9112 (HTTP) | semantica dei costrutti del §4, formati del §6. Nessun codice esterno |

Comportamento della modalità ricavato dalla fonte 1 (riassunto):

| Area | Comportamento della fx-CG50 |
|---|---|
| Elenco dei file (8-1, 8-2) | in ordine alfabetico, con i byte usati da ogni programma; i programmi protetti hanno un asterisco dopo il nome; senza programmi compaiono solo NEW e LOAD; la modalità usa sempre input/output lineari |
| Tasti dell'elenco (8-2, 8-6…8-9) | EXE esegue, EDIT apre l'editor, NEW crea, DELETE (tasto 4) e DEL-ALL (tasto 5) con conferma Sì (1) / No (6), SEARCH e RENAME (numeri di tasto incerti), seconda pagina con F6: SAVE (programma → file di testo) e LOAD (testo → programma) |
| Nomi (8-1) | fino a 8 caratteri A–Z 0–9 { } ' ~; ogni nome occupa 32 byte |
| NEW e password (8-10, 8-11) | NEW, nome, opzione password, password scritta «come un nome di file», EXE; un programma esistente si protegge o si sprotegge (con la password attuale) dall'elenco; EXE esegue senza password; EDIT chiede la password e una password sbagliata dà «Mismatch»; un programma protetto non si converte in testo |
| Esecuzione (8-2, 8-4) | EXE sull'elenco; il programma chiede i suoi input; un errore mostra la finestra d'errore e un tasto porta alla posizione dell'errore nell'editor con il cursore lampeggiante; non disponibile per i programmi protetti (8-5). Dal Run-Matrix: `Prog "nome"` |
| Editor (8-3…8-6) | TOP e BOTTOM (inizio e fine), SEARCH nel programma (testo + EXE: primo risultato dal cursore in avanti; EXE o SEARCH per il successivo; «Not Found.»; digitare o muovere il cursore annulla la ricerca), menu dei comandi (PRGM), maiuscole/minuscole, CHAR, SET UP e FORMAT come comandi |
| Ricerca e rinomina nell'elenco (8-7) | SEARCH: le prime lettere del nome, il primo file che inizia così si evidenzia, «Not Found» altrimenti; RENAME: nome nuovo, l'elenco si riordina, «Already Exists» se il nome c'è già |
| Debug (8-4, 8-5) | «bug» = errore durante l'esecuzione o risultato diverso dall'atteso; si corregge con l'editor (TOP, BOTTOM, SEARCH) e con il salto alla posizione dell'errore. **Nessuna esecuzione passo passo, punto d'interruzione o vista delle variabili** è descritta: il DEBUG passo passo richiesto per questa versione è un'**estensione** (differenza D8) |

### 1.1 Punto di partenza verificato

| Controllo | Esito |
|---|---|
| `git status`, `git fetch origin main` | branch `claude/blissful-knuth-x1ei65` = `main` = `bcd28af`, albero pulito |
| `tests/run_host_tests.sh --board PICO_PLUS_2W` | **64/64** (23,8 s di test con 4 processori) |
| Firmware della v1.19.0 ricompilato | Plus **2 983 560 B** (71 % di 4 MB; budget 3 774 873 B: restano **791 313 B** per le tre versioni v1.20.0–v1.22.0); Pico 2 W **1 944 228 B** (budget 2 064 384 B), 185 oggetti esclusi, nessuno nell'immagine; nessun warning. L'immagine del Pico 2 W è conservata per il confronto byte per byte alla fine |
| Strumenti | GCC 13 host, clang 18 con libFuzzer (`libclang-rt-18-dev` installato in questa sessione), arm-none-eabi-gcc, Pico SDK 2.2.0 in `/opt/sdk/pico-sdk` con mbedTLS **3.6.2**, openssl, dosfstools, mtools |

## 2. Cosa c'è oggi (v1.17.0, Base)

| Parte | Stato |
|---|---|
| Interprete `program.hpp`, `src/program_lang.cpp` (1 578 righe), `src/program_vm.cpp` (875) | compilatore a una passata verso un bytecode per una macchina a pila iterativa. Tipi: int, double, bool, char, stringa, array 1D/2D; funzioni libere; **niente struct, classi, puntatori, eccezioni, contenitori, file**; `char s[n]` trattato come stringa; un solo file per programma, `#include` ignorati |
| Modalità `src/apps/program_app.cpp` (340 righe), `src/program_mode.cpp` | elenco piatto dei `.pcprog` nell'archivio scelto in SYSTEM (microSD **o flash interna**), NEW/EDIT/DEL/RUN, editor `TextEditor` con evidenziazione minima, RUN a passi con input, AC, errori con riga e colonna |
| File | `.pcprog` = testo con la riga `// pcprog: C|C++|C#`, in `PICOCALC/PROG`; Python in `PICOCALC/PY` |
| Mancano per la parità | DEBUG, SEARCH, RENAME, password, DEL-ALL, libreria `fx` completa, cartelle e progetti, file sulla SD, librerie |

## 3. Tabella di parità (a): la modalità Program del capitolo 8

Legenda della colonna **v1.19**: ✔ presente come sulla fx-CG50, ◐ parziale,
✘ assente. **Test**: ogni voce ha il suo caso con l'id nel nome. Suite:
**PF** = `program_flow_test` (firmware completo guidato dai tasti della
PicoCalc), **CS** = `code_store_test` (archivio sulla SD), **CE** =
`code_editor_test` (editor), **LK** = `lang_core_test` (nucleo comune),
**LM** = `lib_manager_test` (librerie con trasporto simulato), **LH** =
`lib_https_test` (librerie con il server HTTPS di prova sul PC).
Le voci segnate *estensione* non esistono sulla fx-CG50 e sono richieste da
questa versione.

### 3.1 Elenco dei programmi e gestione dei file (8-1, 8-2, 8-6, 8-7)

| Id | Voce | v1.19 | Test |
|---|---|---|---|
| PM-01 | **Elenco** di `PICOCALC/CODE/<linguaggio>`: alla radice le cartelle dei linguaggi (C, C++, C#; Java dalla v1.21.0), poi cartelle (progetti) e file con la dimensione in byte; ordine alfabetico come la fx-CG50; segno `*` dopo il nome dei file protetti | ◐ piatto | PF: radice, cartelle, file, dimensioni, ordine, asterisco |
| PM-02 | Navigazione: EXE o → entra in una cartella, EXIT o ← risale; EXIT dalla radice torna al menu principale | ✘ | PF |
| PM-03 | **EXE / F1 RUN** sul file selezionato; su una cartella di progetto esegue il file con `main` (`Main` in C#; più file con `main`: si sceglie) | ◐ | PF |
| PM-04 | **F2 EDIT** apre il file nell'editor (anche `.h`, `.hpp`, testi e CSV del progetto) | ◐ | PF |
| PM-05 | **F3 NEW**: file o progetto (cartella con il file principale), nome, **modello** per linguaggio (saluto, input e calcolo, funzioni, classi, grafico `fx`, file, uso di una libreria), password facoltativa come in 8-10 | ◐ un modello | PF: ogni modello compila ed esegue |
| PM-06 | **F4 DEL** del file o della cartella selezionati con conferma Sì (F1/EXE) / No (F6/EXIT); per una cartella la conferma dice quanti file contiene | ◐ | PF |
| PM-07 | **F5 DEL-ALL**: tutti i file e le cartelle della cartella corrente, con conferma | ✘ | PF |
| PM-08 | **SEARCH** per inizio del nome (le lettere digitate; il primo nome che inizia così si evidenzia; «Not Found» altrimenti) | ✘ | PF |
| PM-09 | **RENAME** del file o della cartella; estensione conservata; l'elenco si riordina; «Already Exists» | ✘ | PF, CS |
| PM-10 | **COPY** e **MOVE** in un'altra cartella dello stesso linguaggio (scelta della destinazione, conferma di sovrascrittura) — *estensione* | ✘ | PF, CS |
| PM-11 | **SORT**: nome A→Z (della fx-CG50), nome Z→A, dimensione, estensione — *estensione* | ✘ | PF |
| PM-12 | **SET UP** della modalità (SHIFT MENU): linguaggio dei file nuovi, ordine, rientro automatico, tabulazione (2/4), numeri di riga, limite di tempo (Off, 10 s, 1 min, 10 min), URL dell'indice delle librerie | ✘ | PF |
| PM-13 | **File recenti**: gli ultimi 8 file aperti nel record (sezione `0x010A`), riaperti dal menu RECENT con la riga del cursore | ✘ | PF |
| PM-14 | **LIB**: il gestore delle librerie si apre dall'elenco (§6) | ✘ | PF |
| PM-15 | Senza microSD: «Inserire la scheda SD»; NEW apre un file **senza nome** che si esegue ma non si salva (SD-02) | ✘ | PF |

### 3.2 Password (8-10, 8-11)

| Id | Voce | v1.19 | Test |
|---|---|---|---|
| PW-01 | **Registrare** la password: a NEW o su un file esistente (PASS), fino a 8 caratteri come un nome; il file mostra `*` | ✘ | PF |
| PW-02 | **EDIT** (e DEBUG, che mostra il sorgente) chiede la password; sbagliata → «Mismatch»; **RUN** non la chiede | ✘ | PF |
| PW-03 | **Togliere** la protezione: PASS con la password attuale | ✘ | PF |
| PW-04 | Il salto alla posizione dell'errore non si fa per i file protetti (8-5): la finestra d'errore mostra file, riga e colonna, l'editor non si apre | ✘ | PF |
| PW-05 | La password non è nel sorgente: file di metadati `PICOCALC/CODE/.meta` con SHA-256 salato (il sorgente resta un file standard; differenza D4) | ✘ | CS |
| PW-06 | RENAME, COPY e MOVE portano la protezione con il file; DEL la toglie dal `.meta` | ✘ | CS |

### 3.3 Esecuzione ed errori (8-2, 8-4, 8-5)

| Id | Voce | v1.19 | Test |
|---|---|---|---|
| RN-01 | Esecuzione **a passi** sul ciclo del firmware (tastiera e display vivi), output sulla console di testo, input con la riga «?» (scanf, cin, Console.ReadLine) | ✔ | PF, LK |
| RN-02 | **AC = Break** in ogni momento (calcolo, attesa dell'input, I/O su file, compilazione di un progetto grande); finestra «Break»; nessun file lasciato aperto | ◐ | PF, LK |
| RN-03 | Fine: «Done» nella barra; la console resta consultabile (↑/↓ fino a 200 righe), EXE/EXIT tornano all'elenco | ◐ | PF |
| RN-04 | **Errori di compilazione** con messaggio, **file**, riga e colonna; EXE/EXIT → editor sul file giusto, cursore sulla colonna, riga evidenziata | ◐ un file | PF, LK |
| RN-05 | **Errori di esecuzione** (Math, Dimension, Pointer, Memory, Nesting, eccezioni non catturate) con la pila delle chiamate (file e riga di ogni cornice) e lo stesso salto all'editor | ◐ | PF, LK |
| RN-06 | **Limiti** con errori chiari: tempo (se impostato), memoria dell'heap («Memory ERROR» / `OutOfMemoryException`), ricorsione («Nesting ERROR» / `StackOverflowException`), lunghezza del sorgente | ◐ | LK |
| RN-07 | Comandi grafici disegnati da Graph (F6 GRAPH dalla console; EXIT torna) | ✔ | PF |
| RN-08 | Il programma riceve gli argomenti da RUN? No: `argc` = 1, `args` vuoto (come un'icona sul desktop) | ✔ | LK |

### 3.4 Editor (8-3…8-6) e richieste della versione

| Id | Voce | v1.19 | Test |
|---|---|---|---|
| ED-01 | **Numeri di riga** nella colonna di sinistra (SET UP li nasconde) | ✘ | CE, PF |
| ED-02 | **Evidenziazione per linguaggio**: C, C++, C#, Java, Python (parole chiave, tipi, commenti anche su più righe, stringhe, caratteri, numeri, direttive del preprocessore, annotazioni) | ◐ minima | CE |
| ED-03 | **Rientro automatico**: EXE copia il rientro della riga, un livello in più dopo `{` (e dopo `:` in Python), un livello in meno digitando `}` | ✘ | CE |
| ED-04 | **Parentesi abbinate**: con il cursore su ( ) [ ] { } la compagna è evidenziata; JUMP › MATCH salta alla compagna; parentesi senza compagna in rosso | ✘ | CE |
| ED-05 | **TOP**, **BOTTOM**, **GOTO** riga (8-5), salto alla riga dell'errore | ◐ | CE, PF |
| ED-06 | **SEARCH** come 8-5 (dal cursore in avanti, poi dall'inizio; EXE per il successivo; «Not Found»; un tasto di modifica annulla la ricerca) e **REPLACE** (uno per volta con conferma, o tutti), maiuscole/minuscole a scelta | ◐ ricerca | CE, PF |
| ED-07 | **Copia, taglia, incolla** (selezione con SHIFT + frecce, Ctrl+C/X/V come gli altri editor) | ◐ | CE |
| ED-08 | **Undo e redo** (200 operazioni, 64 KB di testo) | ✘ | CE |
| ED-09 | **SAVE**, **SAVE·AS** (nome nuovo, anche in un'altra cartella), uscita con modifiche non salvate: «Salvare?» Sì / No / AC | ◐ | PF, CE |
| ED-10 | **File grandi a blocchi** dalla SD senza caricarli interi: indice delle righe, pagine lette dalla SD, modifiche in RAM, salvataggio in streaming atomico. Limiti dichiarati: **8 MB**, **200 000 righe**, riga ≤ **4 096 byte**, **256 KB** di modifiche tra due salvataggi con la PSRAM (24 KB senza); oltre: messaggio chiaro, nessuna perdita | ✘ | CE (file di 2 MB e di 200 000 righe con RAM misurata) |
| ED-11 | **MENU** dei costrutti del linguaggio e delle funzioni della libreria `fx` (inseriti al cursore, come il menu PRGM della fx-CG50) e **CHAR** (simboli) | ✘ | PF |
| ED-12 | Tab → spazi (SET UP: 2 o 4), testo UTF-8, CRLF e BOM dei file del PC conservati al salvataggio | ✘ | CE |
| ED-13 | **BRK**: punto d'interruzione sulla riga del cursore (segno nella colonna dei numeri) | ✘ | CE, PF |

### 3.5 DEBUG passo passo (*estensione*, differenza D8)

| Id | Voce | Test |
|---|---|---|
| DB-01 | **DEBUG** dall'elenco o dall'editor: il programma parte fermo alla prima istruzione di `main` (`Main`); la riga corrente è evidenziata nel sorgente | LK, PF |
| DB-02 | **STEP** (entra nelle funzioni), **OVER** (passa sopra le chiamate), **OUT** (fino al ritorno della funzione), **CONT** (fino al prossimo punto d'interruzione o alla fine) | LK, PF |
| DB-03 | **Punti d'interruzione** per riga (fino a 32, in qualsiasi file del progetto), messi dall'editor o dal debug | LK, PF |
| DB-04 | **VARS**: locali della funzione corrente e globali con tipo e valore; struct, oggetti, array, contenitori e stringhe espandibili (campi ed elementi); `this` | LK, PF |
| DB-05 | **CALLS**: pila delle chiamate con funzione, file e riga; scelta di una cornice per vederne le variabili | LK, PF |
| DB-06 | **OUT**put: la console del programma durante il debug; input (scanf, cin, ReadLine) funziona anche in debug | PF |
| DB-07 | Un errore durante il debug ferma sulla riga dell'errore con le variabili visibili; AC interrompe; EXIT torna all'editor sulla riga corrente | LK, PF |

### 3.6 Archivio sulla microSD

| Id | Voce | Test |
|---|---|---|
| SD-01 | Struttura creata alla prima apertura (§6.1); programmi e librerie **solo** sulla SD; nella flash interna restano impostazioni, file recenti e record del workspace | CS |
| SD-02 | Senza SD: «Inserire la scheda SD», nessun crash; si lavora ancora in un file senza nome (Program) e nella SHELL (Python) senza salvare | CS, PF |
| SD-03 | SD **piena** («SD piena: liberare spazio») o **in sola lettura** («SD in sola lettura»): il salvataggio fallisce pulito, il file precedente resta intatto | CS |
| SD-04 | **Scrittura atomica** di sorgenti, `.meta`, file scritti dai programmi e librerie (file temporaneo, verifica, rename); **SD tolta durante il salvataggio** (interruzione simulata in ogni operazione e dopo ogni byte scritto): il file è il vecchio o il nuovo, mai un misto; i residui `.tmp` si recuperano | CS |
| SD-05 | **SD tolta durante l'esecuzione**: l'I/O del programma fallisce con un errore pulito (`fopen` NULL / `IOException`), i file aperti si chiudono senza scrivere metà dati | LK, CS |
| SD-06 | **File sorgenti veri**: nessuna riga aggiunta; si leggono i file scritti sul PC (CRLF, BOM UTF-8, tab) e si riscrivono uguali salvo le modifiche | CS, CE |
| SD-07 | Nomi: 1–32 caratteri `A-Z a-z 0-9 _ -` più l'estensione del linguaggio; cartelle fino a 4 livelli sotto la cartella del linguaggio | CS |

### 3.7 Migrazione dalla v1.17.0

| Id | Voce | Test |
|---|---|---|
| MG-01 | Al primo ingresso in Program o in Python, se ci sono programmi in `PICOCALC/PROG` o `PICOCALC/PY` (flash interna o SD): «Spostare N programmi della v1.17.0 sulla scheda SD?» Sì/No; No → richiesta di nuovo da SET UP | CS, PF |
| MG-02 | Conversione: `.pcprog` → sorgente standard con l'estensione del dialetto della riga `// pcprog:` (riga tolta); `.c/.cpp/.cs/.py` copiati come sono; conflitto di nome → suffisso `_1`, `_2` | CS |
| MG-03 | Sicurezza: scrittura atomica sulla SD, rilettura e confronto, solo dopo l'originale viene tolto; un'interruzione a metà riprende alla volta successiva senza duplicati | CS |
| MG-04 | Record: stato della migrazione e file recenti nella sezione `0x010A` **versione 2**; la versione 1 della v1.17.0 si legge (l'ultimo file diventa il primo recente, con il percorso nuovo se migrato) | CS, `fuzz_modes`, `fuzz_workspace` |

### 3.8 Libreria `fx` (servizi del capitolo 8 ai programmi)

| Id | Voce | Test |
|---|---|---|
| FX-01 | `calc("espressione")` con il motore unico e le variabili del programma legate; `calcExact` restituisce il testo della forma esatta («√2/2») | LK |
| FX-02 | Variabili A–Z, r, θ e Ans: `getVar`, `setVar`, `getAns` | LK |
| FX-03 | Liste List 1–26: elemento, assegnazione, lunghezza, ridimensionamento, lista intera come array del linguaggio (`getList(n)`, `setList(n, array)`) | LK |
| FX-04 | Matrici Mat A–Z: elemento, assegnazione, dimensioni, matrice intera come array 2D (C/C++: `double[r][c]`/`vector<vector<double>>`, C#: `double[,]`) | LK |
| FX-05 | Grafici come i comandi del capitolo 8: `cls`, `viewWindow`, `plot`, `line`, `circle`, `text`, `graphY`, `vertical`, `horizontal`, `pxlOn`, `pxlOff`, `pxlChg`, `pxlTest`, `drawGraph`, `setColor` (i sette colori della fx-CG50), `command` (un comando di Run-Matrix) | LK, PF |
| FX-06 | Testo e tasti: `locate`, `clrText`, `getKey` (codici del Getkey della fx-CG50 per i tasti che esistono sulla PicoCalc), `pause` (aspetta EXE come ◢) | LK, PF |
| FX-07 | Statistiche: `stat1Var` (n, x̄, Σx, Σx², σx, sx, minX, Q1, Med, Q3, maxX), `stat2Var`, `regression` (i 12 tipi di Statistics: a, b, c, d, e, r, r²), distribuzioni PD/CD/inverse (Normale, t, χ², F, Binomiale, Poisson, Geometrica) con il nucleo `dist_core` | LK |
| FX-08 | Finanza: TVM (`tvmN`, `tvmI`, `tvmPV`, `tvmPMT`, `tvmFV`), interesse semplice e composto, `npv`, `irr`, ammortamento (`amortBal`, `amortInt`, `amortPrn`), conversione dei tassi | LK |

## 4. Specifica dei sottoinsiemi (b), costrutto per costrutto

Un costrutto **supportato** ha la semantica della norma del linguaggio salvo
le note; un costrutto **non supportato** dà, alla compilazione, «Syntax ERROR:
*costrutto* non supportato» con file, riga e colonna (mai un comportamento
sbagliato in silenzio). Suite: **LC** = `lang_c_test`, **LP** =
`lang_cpp_test`, **LS** = `lang_cs_test`; ogni riga ha il suo caso con l'id.

### 4.1 Comune ai tre linguaggi

| Id | Costrutto | Note |
|---|---|---|
| LK-01 | Commenti `//`, `/* */`; letterali decimali, esadecimali `0x`, binari `0b`, ottali `0` (C/C++), separatori di cifre (`'` in C++14, `_` in C#), suffissi; caratteri ed escape (`\n \t \\ \' \" \0 \xHH`, ottali in C/C++, `\uXXXX` in C#) | |
| LK-02 | Operatori con precedenza e associatività della norma: assegnazioni composte, ternario, logici con corto circuito, bit, scorrimenti, confronto, aritmetici, unari, `++`/`--` prefissi e postfissi, cast, virgola (C/C++) | |
| LK-03 | Controllo di flusso: blocchi, `if`/`else`, `while`, `do`/`while`, `for`, `switch`, `break`, `continue`, `return`, `goto` ed etichette | regole del `switch` per linguaggio sotto |
| LK-04 | Funzioni con parametri e ricorsione (profondità limitata, §5.7) | |
| LK-05 | Aritmetica intera a 8/16/32/64 bit in complemento a due con **avvolgimento** (in C e C++ l'overflow con segno è indefinito: qui è definito, D9), divisione intera verso zero, `%` con il segno del dividendo, divisione intera per zero → errore (`Math ERROR` / `DivideByZeroException`); `float` arrotondato a 32 bit a ogni operazione, `double` IEEE 754 | |
| LK-06 | Errori con file, riga e colonna; messaggi in italiano col nome dell'errore della fx-CG50 (Syntax, Math, Dimension, Memory, Nesting, Argument, Go) o il nome dell'eccezione del linguaggio | |
| LK-07 | Più file nello stesso progetto e librerie installate (§6.8) | |

### 4.2 C (C11, sottoinsieme)

| Id | Costrutto | Stato |
|---|---|---|
| LC-01 | `int main()`, `int main(void)`, `int main(int argc, char* argv[])`; valore di ritorno = codice d'uscita; prototipi; chiamare una funzione non dichiarata → errore | supportato |
| LC-02 | Interi: `char` (8 bit con segno), `signed`/`unsigned char`, `short`, `int` (32), `unsigned`, `long` e `long long` (64, modello **LP64** come Linux), `unsigned long`, `size_t`, `int8_t`…`uint64_t`; conversioni e promozioni del C | supportato |
| LC-03 | `float`, `double`, `long double` (= `double`); conversioni usuali | supportato |
| LC-04 | `bool`, `true`, `false` (con o senza `<stdbool.h>`) | supportato |
| LC-05 | Letterali: interi con suffissi `u`, `l`, `ll`; reali con `f`; caratteri; stringhe con concatenazione dei letterali adiacenti | supportato |
| LC-06 | `sizeof` di tipi ed espressioni in byte del modello LP64 (struct con allineamento) | supportato |
| LC-07 | `switch` con fall-through e `default`; `goto` ed etichette nella stessa funzione | supportato |
| LC-08 | Array 1D, 2D e fino a 4 dimensioni; inizializzatori anche parziali e designati (`[3] = 1`); array a lunghezza variabile; **controllo dei limiti a ogni accesso** («Dimension ERROR: indice 5 fuori da [0, 4]»); array come parametri (decadimento a puntatore) | supportato |
| LC-09 | Stringhe = array di `char` terminati da `'\0'`; i letterali sono in sola lettura (scriverci → errore) | supportato |
| LC-10 | `struct` (annidate, array di struct, struct nelle struct), inizializzatori `{…}` e designati `.campo =`, assegnazione, passaggio e ritorno per valore (copia), `typedef` | supportato |
| LC-11 | `enum` (valori espliciti, usati come `int`), `typedef enum` | supportato |
| LC-12 | **Puntatori controllati**: `&`, `*`, `->`, aritmetica e confronto nello stesso array, differenza, `NULL`, puntatori a puntatori, `void*` con conversione implicita, `const`. Errori: dereferenziazione di NULL («Pointer ERROR: puntatore NULL»), fuori dai limiti, memoria liberata, puntatore a una variabile locale uscita dal suo blocco | supportato |
| LC-13 | Puntatori a funzione: `T (*f)(params)`, `typedef`, array di puntatori a funzione, chiamata con `f(x)` e `(*f)(x)` | supportato |
| LC-14 | Variabili globali, `static` locali (conservano il valore), `extern` tra i file del progetto, `const`; `register`, `volatile`, `inline` accettati | supportato |
| LC-15 | Preprocessore: `#include "file.h"` (cartella del file, poi del progetto), `#include <x.h>` (intestazioni standard, poi PICOCALC/LIB/C), `#define` semplici e con parametri, `#undef`, `#if`/`#ifdef`/`#ifndef`/`#elif`/`#else`/`#endif` con `defined`, `#pragma once`, `#error`; macro predefinite `NULL`, `EOF`, `RAND_MAX`, `INT_MAX`…, `M_PI`, `M_E`, `__LINE__`, `__FILE__` | supportato |
| LC-16 | `<stdio.h>`: `printf`, `fprintf`, `sprintf`, `snprintf` (formati `%d %i %u %ld %lld %lu %x %X %o %c %s %f %F %e %E %g %G %p %%`, flag `- + 0 # spazio`, larghezza e precisione anche `*`); `scanf`, `fscanf`, `sscanf` (`%d %i %u %ld %lld %f %lf %c %s %[…]`, larghezza, valore di ritorno, EOF); `getchar`, `putchar`, `puts`, `fgets(buf, n, stdin)` | supportato |
| LC-17 | File: `fopen` (`"r" "w" "a" "r+" "w+" "a+"`, `"b"` accettato), `fclose`, `fgets`, `fputs`, `fgetc`/`getc`, `fputc`/`putc`, `fprintf`, `fscanf`, `feof`, `ferror`, `fflush`, `remove`, `rename`, **solo nella cartella del programma** (percorsi relativi, niente `..` né percorsi assoluti: `fopen` restituisce NULL e `perror` spiega); scrittura atomica alla chiusura (§5.9) | supportato |
| LC-18 | `<stdlib.h>`: `malloc`, `calloc`, `realloc`, `free` su **heap controllato** (il blocco prende il tipo alla prima conversione; doppio `free`, `free` di un puntatore non allocato, uso dopo `free` → errore), `rand`/`srand`/`RAND_MAX`, `abs`, `labs`, `llabs`, `atoi`, `atol`, `atof`, `strtol`, `strtod`, `exit`, `qsort`, `bsearch` | supportato |
| LC-19 | `<string.h>`: `strlen`, `strcpy`, `strncpy`, `strcat`, `strncat`, `strcmp`, `strncmp`, `strchr`, `strrchr`, `strstr`, `strtok`, `strdup`, `memset`, `memcpy`, `memmove`, `memcmp` (scritture oltre la fine → errore) | supportato |
| LC-20 | `<ctype.h>`, `<math.h>` (funzioni comuni, `isnan`, `isinf`, `NAN`, `INFINITY`, `hypot`, `cbrt`, `log2`), `<time.h>` (`time`, `clock`, `CLOCKS_PER_SEC`, `difftime`: tempo dall'accensione, la PicoCalc non ha l'orologio), `<limits.h>`, `<float.h>`, `<stdint.h>`, `<stddef.h>`, `<stdbool.h>` | supportato |
| LC-21 | `<fx.h>`: la libreria `fx` con i nomi `fx_…` (§4.5) | supportato |
| LC-22 | **Non supportati** (errore chiaro): `union`, campi di bit, funzioni variadiche dell'utente (`...`), conversioni tra puntatori e interi, cast tra puntatori di tipi diversi (salvo `void*`), `setjmp`/`longjmp`, `signal`, thread, `asm`, `#` e `##` nelle macro, `__VA_ARGS__`, `gets` (usa `fgets`), `<complex.h>`, caratteri larghi, intestazioni non disponibili | errore chiaro |

### 4.3 C++ (C++17, sottoinsieme)

| Id | Costrutto | Stato |
|---|---|---|
| LP-01 | Tutto il §4.2 (con le regole del C++: `bool` e `char` nativi, `struct` = classe con membri pubblici) | supportato |
| LP-02 | **Classi** e struct: `public`/`private`/`protected` **controllati alla compilazione**, campi, metodi nella classe e fuori (`T A::f()`), `this`, membri `static`, metodi `const`, `friend` | supportato |
| LP-03 | **Costruttori** (predefinito, con parametri, overload, lista di inizializzazione, delega), costruttore di copia e `operator=` dell'utente, **distruttori** (fine del blocco in ordine inverso, `delete`, membri, elementi dei contenitori, uscita per eccezione) | supportato |
| LP-04 | `new`/`delete`, `new[]`/`delete[]` controllati (doppio `delete`, `delete` di un puntatore non da `new` → errore), `nullptr` | supportato |
| LP-05 | **Ereditarietà semplice** (`public`, `protected`, `private`), costruttore della base nella lista, `virtual`, `override`, `final`, funzioni virtuali pure (`= 0`) e classi astratte (istanziarle → errore), distruttore virtuale, `Base::metodo()`, slicing nella copia per valore, `dynamic_cast` su puntatori (`nullptr` se fallisce) e riferimenti (`std::bad_cast`) | supportato |
| LP-06 | **Overload** di funzioni, metodi e costruttori (corrispondenza esatta > promozione > conversione; ambiguità → errore), argomenti predefiniti; **operatori** dell'utente (`+ - * / % == != < > <= >= += -= *= /= [] () ! -` unario, `++`/`--`, `<<` e `>>` con `ostream`/`istream`) | supportato |
| LP-07 | **Riferimenti**: parametri `T&` e `const T&`, variabili riferimento, ritorno per riferimento | supportato |
| LP-08 | **Namespace** (annidati e riaperti), `::`, `using namespace`, `using X::nome`, `std` | supportato |
| LP-09 | **Template semplici** di funzione e di classe (`typename`/`class`, parametri interi, deduzione dagli argomenti, argomenti espliciti) | supportato |
| LP-10 | `auto`, range-for (`auto`, `auto&`, `const auto&`), binding strutturati di `pair` (`auto [k, v]`), `constexpr` (come `const`), `enum class`, `static_cast`, **lambda** semplici (`[]`, `[=]`, `[&]`, `[x, &y]`) come comparatori e in variabili `auto` | supportato |
| LP-11 | `std::string`: `size`/`length`, `empty`, `[]`, `at`, `+`, `+=`, confronti, `substr`, `find`, `rfind`, `replace`, `insert`, `erase`, `append`, `push_back`, `pop_back`, `c_str`, `compare`, `front`, `back`, `clear`; `to_string`, `stoi`, `stol`, `stod`, `stof`, `getline` | supportato |
| LP-12 | `<iostream>`: `cout`, `cin` (int, reali, parole, char, `getline`), `cerr`, `endl`; `<iomanip>`: `setw`, `setprecision`, `fixed`, `scientific`, `setfill`, `left`, `right`, `boolalpha`, `hex`, `dec`, `oct` | supportato |
| LP-13 | Contenitori: `vector` (costruttori, `push_back`, `emplace_back`, `pop_back`, `size`, `empty`, `[]`, `at` → `std::out_of_range`, `front`, `back`, `clear`, `resize`, `reserve`, `insert`/`erase` con `begin()+i`, iteratori per range-for e algoritmi, vettori di vettori e di oggetti), `map` (`[]`, `at`, `insert`, `find`/`end`, `count`, `erase`, ordine delle chiavi), `set`, `unordered_map`, `unordered_set` (iterazione nell'ordine d'inserimento, D10), `pair`/`make_pair`, `stack`, `queue`, `deque`, `list` (metodi principali) | supportato |
| LP-14 | `<algorithm>`/`<numeric>`: `sort` (anche con comparatore), `stable_sort`, `reverse`, `find`, `count`, `min`, `max`, `swap`, `min_element`, `max_element`, `accumulate`, `fill` | supportato |
| LP-15 | **Eccezioni**: `try`/`catch` (per tipo, per riferimento, `catch(...)`), `throw` di valori e di oggetti, `throw;`, `std::exception::what()`, `runtime_error`, `logic_error`, `out_of_range`, `invalid_argument`, `bad_alloc` (memoria finita); eccezione non catturata → errore con tipo, messaggio e posizione | supportato |
| LP-16 | `<fstream>`: `ifstream`, `ofstream`, `fstream` (`open`, `is_open`, `>>`, `<<`, `getline`, `eof`, `good`, `fail`, `close`) confinati come LC-17; `<sstream>`: `stringstream`, `istringstream`, `ostringstream` (`getline` con separatore per i CSV, `str()`) | supportato |
| LP-17 | `<cmath>`, `<cstdlib>`, `<cstring>`, `<cctype>`, `<ctime>`, `<climits>`, `<cstdio>` come in C, anche con `std::` | supportato |
| LP-18 | Libreria `fx` nel namespace `fx::` (§4.5); librerie C (PICOCALC/LIB/C) usabili dal C++ | supportato |
| LP-19 | **Non supportati** (errore chiaro): ereditarietà multipla e virtuale, specializzazione dei template, template variadici, SFINAE, concetti, riferimenti `&&` e semantica di spostamento (`std::move` accettato come copia), smart pointer, `std::function`, thread, `typeid`, `reinterpret_cast`, letterali dell'utente, coroutine, moduli, `union`, `operator new` dell'utente, conversioni dell'utente (`operator T()`) | errore chiaro |

### 4.4 C# (C# 10, sottoinsieme)

| Id | Costrutto | Stato |
|---|---|---|
| LS-01 | `using` di `System`, `System.Collections.Generic`, `System.IO`, `System.Text`, `System.Linq` (accettato; i metodi LINQ danno errore), `namespace` a blocco e di file, **più file `.cs` dello stesso progetto**, `static void/int Main()` con o senza `string[] args`, istruzioni di primo livello in un solo file | supportato |
| LS-02 | Tipi `int`, `long`, `short`, `byte`, `sbyte`, `uint`, `ulong`, `ushort`, `float`, `double`, `bool`, `char` (16 bit), `string`, `object`, `var`; conversioni implicite ed esplicite di C# (`double` → `int` solo con cast) | supportato |
| LS-03 | Tipi nullable di valore (`int?`, `HasValue`, `Value`, `??`, `??=`), operatore `?.` | supportato |
| LS-04 | Array `int[]` (`new int[n]`, `new[] {…}`, inizializzatori), multidimensionali `int[,]`, frastagliati `int[][]`, `Length`, `GetLength`, `Rank`, `IndexOutOfRangeException`, `Array.Sort`/`Reverse`/`IndexOf`/`Copy`/`Resize` | supportato |
| LS-05 | **Classi**: campi con inizializzatori, `const`, `static`, `readonly`, metodi statici e d'istanza, overload, parametri predefiniti, `ref`, `out`, `params`, `this`, costruttori (overload, `: this(…)`, `: base(…)`), costruttore statico, classi `static`, `abstract`, `sealed`, `partial` | supportato |
| LS-06 | **Proprietà**: automatiche (`{ get; set; }`, `{ get; private set; }`, con inizializzatore), complete (`get`/`set` con `value`), a corpo d'espressione (`=>`), indicizzatori `this[int i]` | supportato |
| LS-07 | **struct** con semantica per valore (copia nell'assegnazione e nel passaggio) e costruttori | supportato |
| LS-08 | **Ereditarietà**: una classe base, `virtual`/`override`/`abstract`/`sealed`/`new`, `base.Metodo()`, polimorfismo, `object` come radice (`ToString`, `Equals`, `GetHashCode` sovrascrivibili), `is`, `as`, `is Tipo nome` | supportato |
| LS-09 | **Interfacce semplici**: metodi e proprietà, più interfacce per classe, variabili di tipo interfaccia, `IComparable<T>` (usata da `Sort`) | supportato |
| LS-10 | `enum` (valori espliciti, cast, `ToString()` col nome) | supportato |
| LS-11 | `switch` su `int`, `char`, `string`, `enum` **senza fall-through implicito** (errore come CS0163); `foreach` su array, `List`, `Dictionary`, `HashSet`, `Queue`, `Stack`, `string` | supportato |
| LS-12 | Stringhe immutabili: `+`, `==`, interpolazione `$"{x,8:F2}"`, `@"…"`, `Length`, `Substring`, `IndexOf`, `LastIndexOf`, `Contains`, `StartsWith`, `EndsWith`, `ToUpper`, `ToLower`, `Trim`, `TrimStart`, `TrimEnd`, `Split`, `Replace`, `Insert`, `Remove`, `PadLeft`, `PadRight`, `ToCharArray`, `CompareTo`, `Equals`, indicizzatore; `string.Join`, `string.Format`, `string.Concat`, `string.IsNullOrEmpty`, `IsNullOrWhiteSpace`, `string.Empty`; `StringBuilder`; metodi di `char` | supportato |
| LS-13 | `Console.Write`/`WriteLine` (anche con il formato composito `{0}`, `{0:F2}`, `{0,5}`), `ReadLine`, `ReadKey` (tasto della PicoCalc), `Clear` | supportato |
| LS-14 | `Math` (`Abs`, `Max`, `Min`, `Pow`, `Sqrt`, trigonometriche, `Floor`, `Ceiling`, `Round` con arrotondamento bancario e con le cifre, `Truncate`, `Sign`, `Log`, `Log10`, `Exp`, `Clamp`, `PI`, `E`), `Convert` (`ToInt32` con arrotondamento bancario, `ToInt64`, `ToDouble`, `ToString`, `ToBoolean`, `ToChar`), `Parse`/`TryParse`, `ToString("F2")`/`"N2"`/`"0.00"`/`"D3"`/`"X"`, `Random` (`Next`, `NextDouble`; con il seme la sequenza di .NET) | supportato |
| LS-15 | Collezioni: `List<T>` (`Add`, `AddRange`, `Insert`, `Remove`, `RemoveAt`, `RemoveAll`, `Contains`, `IndexOf`, `Find`, `FindIndex`, `Exists`, `Count`, `Clear`, `Sort()`, `Sort(Comparison)`, `Reverse`, `ToArray`, `ForEach`, indicizzatore), `Dictionary<K,V>` (`Add` con `ArgumentException`, indicizzatore con `KeyNotFoundException`, `ContainsKey`, `ContainsValue`, `TryGetValue`, `Remove`, `Count`, `Keys`, `Values`, `Clear`, ordine d'inserimento), `HashSet<T>`, `Queue<T>`, `Stack<T>`, `KeyValuePair<K,V>`; **classi e metodi generici dell'utente** (`Box<T>`, `T Max<T>(…) where T : IComparable<T>`) | supportato |
| LS-16 | **Lambda** (`x => …`, `(a, b) => …`, con blocco) come argomenti dei metodi delle collezioni e in variabili `Func<…>`, `Action<…>`, `Predicate<T>`, `Comparison<T>`; chiusure sulle variabili locali | supportato |
| LS-17 | **Eccezioni**: `try`/`catch` (tipo, variabile), `finally`, `throw`, `throw;`, eccezioni dell'utente derivate da `Exception` (`: base(message)`), `Message`, `StackTrace` con riga e colonna, `ToString()`; standard: `Exception`, `SystemException`, `ArgumentException`, `ArgumentNullException`, `ArgumentOutOfRangeException`, `InvalidOperationException`, `FormatException`, `OverflowException`, `DivideByZeroException`, `IndexOutOfRangeException`, `NullReferenceException`, `KeyNotFoundException`, `NotImplementedException`, `NotSupportedException`, `IOException`, `FileNotFoundException`, `UnauthorizedAccessException`, `InvalidCastException`, `OutOfMemoryException`; `StackOverflowException` è fatale come in .NET | supportato |
| LS-18 | **File confinati**: `File.ReadAllText`, `WriteAllText`, `AppendAllText`, `ReadAllLines`, `WriteAllLines`, `Exists`, `Delete`; `StreamReader` (`ReadLine`, `ReadToEnd`, `EndOfStream`, `Close`, `using`), `StreamWriter` (`Write`, `WriteLine`, `Close`, `using`, aggiunta); `Directory.Exists`/`CreateDirectory`/`GetFiles` e `Path.Combine`/`GetFileName`/`GetExtension` nella cartella del programma; fuori → `UnauthorizedAccessException` | supportato |
| LS-19 | `using NomeLibreria;` risolto in PICOCALC/LIB/CS (campo `namespace` del manifest) | supportato |
| LS-20 | Libreria `Fx` (classe statica, §4.5) | supportato |
| LS-21 | Operatori dell'utente (`public static T operator +(T a, T b)`, confronti, unari) | supportato |
| LS-22 | **Non supportati** (errore chiaro): LINQ, `async`/`await`, `Task`, thread, `delegate` ed `event` dell'utente, reflection, attributi, `unsafe` e puntatori, `dynamic`, `record`, tuple, `switch` come espressione, pattern oltre `is T x`, funzioni locali, metodi di estensione, `yield`, `decimal`, `checked`, `goto case`, `nameof`, `typeof`, finalizzatori, conversioni dell'utente (`implicit`/`explicit operator`) | errore chiaro |

### 4.5 Libreria `fx` nei linguaggi

Gli stessi servizi del §3.8 con i nomi di ogni linguaggio (stesso codice
nativo, `src/program_fx.cpp`):

| Servizio | C (`#include <fx.h>`) | C++ (`#include <fx.h>` o `<fx>`) | C# | Java (v1.21.0) |
|---|---|---|---|---|
| espressione | `fx_calc("…")`, `fx_calc_exact` | `fx::calc`, `fx::calcExact` | `Fx.Calc`, `Fx.CalcExact` | `Fx.calc`, `Fx.calcExact` |
| variabili | `fx_getvar('A')`, `fx_setvar` | `fx::getVar`, `fx::setVar` | `Fx.GetVar`, `Fx.SetVar` | `Fx.getVar`, `Fx.setVar` |
| liste | `fx_getlist(1, i)`, `fx_setlist`, `fx_listsize`, `fx_getlist_all(1, buf, n)` | `fx::getList(1, i)`, `fx::getList(1)` → `vector<double>`, `fx::setList(1, v)` | `Fx.GetList(1, i)`, `Fx.GetList(1)` → `double[]` | `Fx.getList(1)` → `double[]` |
| matrici | `fx_getmat('A', r, c)`, `fx_setmat`, `fx_matrows`, `fx_matcols` | `fx::getMat`, `fx::getMatrix('A')` → `vector<vector<double>>` | `Fx.GetMatrix('A')` → `double[,]` | `Fx.getMatrix('A')` → `double[][]` |
| grafici | `fx_cls()`, `fx_plot(x, y)`, … | `fx::cls()`, `fx::plot(x, y)`, … | `Fx.Cls()`, `Fx.Plot(x, y)`, … | `Fx.cls()`, … |
| statistiche | `fx_stat1var(1, 0)` → `FxStat1` | `fx::stat1Var(1)` → `fx::Stat1` | `Fx.Stat1Var(1)` → `Fx.Stat1` | `Fx.stat1Var(1)` |
| finanza | `fx_tvm_fv(n, i, pv, pmt, py, cy)` | `fx::tvmFV(…)` | `Fx.TvmFV(…)` | `Fx.tvmFV(…)` |

I file `fx.h` e le classi `Fx` non sono file sulla SD: sono intestazioni
virtuali del nucleo, come `<stdio.h>`.

## 5. Progetto del nucleo condiviso

### 5.1 Pipeline (dal sorgente, a ogni RUN)

```
file sulla SD ──► caricatore del progetto ──► lexer + preprocessore ──► parser (C/C++ | C# | Java v1.21.0)
                  (include/using/import,         (comune, parole           │
                   cartella poi LIB, SHA-256      chiave per linguaggio)    ▼
                   dei file delle librerie)                        AST comune (program_ast.hpp)
                                                                           │
                                         analisi: nomi, tipi statici, classi, vtable, overload
                                                                           │
                                                                           ▼
                                   istruzioni interne in RAM (mai salvate) ──► macchina iterativa
```

I programmi sono **interpretati dal sorgente**: a ogni RUN il sorgente viene
letto dalla SD, analizzato e tradotto in una rappresentazione interna in RAM
(un albero e poi una lista di istruzioni per una macchina a pila) che si butta
alla fine dell'esecuzione. Non esiste compilazione nativa né un file di
bytecode: niente `.o`, niente `.class`, niente `.dll`. La rappresentazione
interna serve solo a eseguire velocemente e senza ricorsione nativa (Q1).

| File | Contenuto |
|---|---|
| `include/program.hpp` | API pubblica (la stessa della v1.15.0, estesa): `Dialect` (C, Cpp, CSharp, Java), `Project`/`SourceProvider`, `compile`, `Machine` (step::Task), `Host`, debug |
| `src/program_internal.hpp`, `src/program_ast.hpp` | tipi, valori, oggetti, istruzioni; AST comune |
| `src/program_lex.cpp` | lexer comune e preprocessore C/C++ |
| `src/program_parse.cpp`, `src/program_parse_cs.cpp` (v1.21.0: `program_parse_java.cpp`) | parser per famiglia |
| `src/program_sema.cpp`, `src/program_codegen.cpp` | analisi dei nomi e dei tipi, classi, generazione delle istruzioni |
| `src/program_vm.cpp`, `src/program_heap.cpp` | macchina, cornici, eccezioni, debug; heap, GC, stringhe, contenitori |
| `src/program_lib.cpp`, `src/program_lib_cs.cpp` | librerie standard (C, C++, C#; v1.21.0 `program_lib_java.cpp`) |
| `src/program_fx.cpp` | libreria `fx` |
| `src/program_project.cpp` | progetto, risoluzione di include/using/import, confinamento dei percorsi |

### 5.2 Valori e tipi

- **Valore** (16 byte): un tipo dinamico (`void`, `null`, `bool`, `char8`,
  `char16`, `i8`…`u64`, `f32`, `f64`, riferimento, puntatore, funzione) e il
  dato. Le variabili hanno il **tipo statico** dichiarato: ogni scrittura
  converte il valore al tipo della variabile (o del parametro, del campo,
  del ritorno), quindi il tipo dinamico coincide con quello statico e
  l'aritmetica segue le regole del linguaggio (promozioni, divisione intera,
  avvolgimento a 32/64 bit, `float` a 32 bit) senza un caso per linguaggio
  nella macchina.
- **Puntatore** = (oggetto, indice della cella): una cella di un array, di
  una scatola (variabile locale di cui si prende l'indirizzo), un campo di
  una struct o l'oggetto intero. Aritmetica e confronti solo nello stesso
  oggetto; ogni dereferenziazione controlla limiti, oggetto liberato e
  scatola morta (variabile uscita dal blocco).
- **Riferimento** (C++ `T&`, C# `ref`/`out`): un puntatore dereferenziato in
  automatico dal compilatore.
- **Tipi statici** nel compilatore: primitivi, puntatore a T, array di T,
  classe (per valore o per riferimento), stringa, istanza di un generico,
  funzione; servono per overload, campi e metodi risolti alla compilazione,
  conversioni implicite, errori alla compilazione. `auto`/`var` prendono il
  tipo dell'inizializzatore.

### 5.3 Oggetti, memoria, GC

- **Un solo modello a oggetti** per tutti i linguaggi: array, istanze di
  classe/struct (campi in slot, puntatore alla classe), stringhe (byte in
  C/C++, UTF-16 in C#/Java), scatole, contenitori nativi (vettore, deque,
  lista, mappa ordinata, mappa e insieme a hash con ordine d'inserimento),
  StringBuilder, file aperti, chiusure (lambda e puntatori a funzione).
- **Classi**: nome, base, interfacce, campi (slot, tipo, accesso, static),
  metodi (overload, virtuali con indice nella vtable), costruttori,
  distruttore (C++), proprietà (C#: metodi get/set), valore o riferimento
  (struct di C/C++ e oggetti per valore del C++, struct di C#), astratta,
  parametri di tipo. Le **classi native** (eccezioni, collezioni, `Fx.Stat1`)
  sono descritte nella stessa tabella: una classe dell'utente può derivare
  da `Exception` (C#), `std::exception` (C++), `Exception` (Java).
- **Heap** dell'interprete con un budget in byte (§5.7) e **GC mark-and-sweep**
  con una pila esplicita (nessuna ricorsione): radici = pila dei valori,
  cornici, globali e statici, file aperti, blocchi di `malloc`/`new` del C e
  del C++ non ancora liberati (restano allocati finché `free`/`delete`, come
  in C: un programma che perde memoria in un ciclo arriva a «Memory ERROR»).
  Cicli di riferimenti (liste doppie, grafi) liberati dal GC. Il GC parte
  quando l'heap cresce di metà dall'ultimo ciclo o quando un'allocazione
  non trova spazio (poi «Memory ERROR» / `OutOfMemoryException` /
  `OutOfMemoryError` in Java, catturabile).
- **Semantica per valore**: copiare una struct C, un oggetto C++ per valore o
  una struct C# alloca un oggetto nuovo e copia i campi (in profondità per
  le struct annidate e per `std::string`/`vector`/`map` del C++, che sono
  valori); il costruttore di copia dell'utente viene chiamato.

### 5.4 Errori ed eccezioni

Un solo meccanismo: le eccezioni del linguaggio (`throw`) e gli errori della
macchina (indice fuori dai limiti, divisione per zero, NULL, memoria) sono
**oggetti eccezione** con classe, messaggio e pila (file, riga e colonna di
ogni cornice). Ogni funzione ha la tabella dei blocchi `try` (intervallo di
istruzioni, classe catturata, gestore); `finally` (C#, Java) e i distruttori
del C++ all'uscita per eccezione sono gestori «per tutte» che eseguono il
codice e rilanciano. Gli errori della macchina hanno la classe del
linguaggio (`IndexOutOfRangeException` in C#, `std::out_of_range` solo da
`at()` in C++, `ArrayIndexOutOfBoundsException` in Java) e si catturano dove
il linguaggio lo permette; in C (e in C++ per gli accessi senza `at`) sono
errori fatali con il nome della fx-CG50. Un'eccezione non catturata ferma il
programma con la finestra d'errore.

### 5.5 Esecuzione a passi, coroutine, AC

La macchina è **iterativa**: una chiamata del programma è una cornice in un
vettore (nessuna ricorsione nativa), così la profondità dipende solo dalla
memoria. Compilazione ed esecuzione girano in una **coroutine con una pila
propria** (lo stesso cambio di contesto di Python, estratto in
`src/coroutine.cpp`: ucontext sul PC, `pc_ctx_switch` con MSPLIM sul
Cortex-M33): la macchina restituisce il controllo al ciclo del firmware alla
fine di ogni fetta di tempo, anche nel mezzo di una funzione nativa che
richiama codice del programma (comparatori di `sort`, `ToString()`,
`operator<<`, `hashCode`), e l'analisi di un progetto grande non blocca
l'interfaccia. **AC** imposta la richiesta d'interruzione: la macchina la
legge a ogni gruppo di istruzioni e le funzioni native lunghe (letture di
file, ordinamenti, costruzione di stringhe) la leggono nei loro cicli;
l'interruzione chiude i file aperti e libera l'heap.

### 5.6 Debug

Il compilatore segna l'inizio di ogni istruzione del sorgente (file, riga,
colonna) e i nomi delle variabili di ogni blocco con l'intervallo di
istruzioni in cui sono vive. La macchina ha una modalità debug: si ferma
all'inizio di un'istruzione se (a) è un punto d'interruzione, (b) è il passo
chiesto (STEP: qualsiasi istruzione nuova; OVER: istruzione nuova con la
cornice uguale o più esterna; OUT: dopo il ritorno della cornice). In pausa
si leggono pila delle chiamate e variabili (valori formattati come li
stamperebbe il linguaggio, oggetti e array espandibili).

### 5.7 Limiti (con la PSRAM / senza)

| Limite | Plus con PSRAM | Plus senza PSRAM | Errore |
|---|---|---|---|
| Sorgente di un file | 256 KB | 64 KB | «Memory ERROR: sorgente troppo grande» |
| Sorgente del progetto con le librerie | 1 MB | 128 KB | come sopra |
| Heap del programma | 2 MB | 96 KB | «Memory ERROR» / `OutOfMemoryException` |
| Pila dei valori | 65 536 valori (1 MB) | 4 096 | «Nesting ERROR» |
| Chiamate annidate | 2 000 | 200 | «Nesting ERROR» / `StackOverflowException` |
| Annidamento nel sorgente (blocchi, espressioni) | 64 | 64 | «Stack ERROR» |
| Pila della coroutine (SRAM) | 48 KB | 48 KB | — |
| File aperti insieme | 8 | 8 | `fopen` NULL / `IOException` |
| Tempo (SET UP) | Off, 10 s, 1 min, 10 min (Off iniziale) | | «Time ERROR» |

### 5.8 Come si aggiunge Java (v1.21.0) senza riscrivere il nucleo

Java è **un front-end in più**: `program_parse_java.cpp` produce lo stesso AST
(classi, interfacce, enum, metodi, campi, eccezioni, generics, lambda), la
stessa analisi assegna slot e vtable e genera le stesse istruzioni. Quello
che serve a Java è già nel nucleo per C++ e C#: classi con ereditarietà
singola e interfacce, oggetti per riferimento, `null`, eccezioni con
`finally`, GC mark-and-sweep con i cicli, chiusure (lambda di C++ e C#),
stringhe UTF-16 immutabili (C#), collezioni native (le `List`/`Dictionary` di
C# diventano `ArrayList`/`HashMap`, con l'ordine di iterazione di Java),
confinamento dei file, risoluzione di `import` nella cartella del progetto e
poi in PICOCALC/LIB/JAVA, libreria `fx` (classe `Fx`). Nella v1.21.0 si
aggiungono solo: il parser, le regole dei tipi di Java (overflow, `>>>`,
`char` senza segno, conversioni), le librerie `java.lang`/`java.util`/`java.io`
minime e i nomi delle eccezioni; il resto del nucleo non cambia.

### 5.9 File scritti dai programmi

Un file aperto in scrittura da un programma va in `nome.tmp` e diventa il file
vero solo alla chiusura (`fclose`, `close()`, `Dispose`, fine del `using`) o
all'uscita normale del programma (come lo svuotamento dei buffer del C a
`exit`); dopo un errore o AC il `.tmp` si scarta e il file vecchio resta
intatto (un messaggio nella console lo dice). Le aggiunte (`"a"`) copiano il
file nel `.tmp` e scrivono in coda.

## 6. Progetto del gestore delle librerie (c)

### 6.1 Struttura sulla SD

```
PICOCALC/CODE/C/          programmi C (.c, .h); sottocartelle = progetti
PICOCALC/CODE/CPP/        programmi C++ (.cpp, .hpp, .h)
PICOCALC/CODE/CS/         programmi C# (.cs)
PICOCALC/CODE/JAVA/       programmi Java (.java; le sottocartelle seguono i package) — v1.21.0
PICOCALC/CODE/PY/         script Python (.py; pacchetti con __init__.py) — editor e LIB dalla v1.22.0
PICOCALC/CODE/.meta       password (percorso → SHA-256 salato)
PICOCALC/LIB/<LING>/<nome>/library.json   una cartella per libreria, con il suo manifest
PICOCALC/LIB/.cache/      index.json scaricato
PICOCALC/LIB/.tmp/        scaricamenti e installazioni in corso, giornale journal.txt
PICOCALC/LIBDATA/<LING>/<nome>/            dati delle librerie (I/O confinato), conservati da UPDATE
```

### 6.2 Manifest `library.json`

```json
{
  "name": "matrici",
  "version": "1.2.0",
  "language": "c",
  "description": "Operazioni su matrici quadrate",
  "author": "PicoCalc FxCG-50",
  "license": "MIT",
  "firmware": "1.20.0",
  "namespace": "Matrici",
  "files": [
    {"path": "include/matrici.h", "size": 812, "sha256": "…64 cifre esadecimali…"},
    {"path": "src/matrici.c", "size": 2310, "sha256": "…"}
  ],
  "dependencies": {"vettori": "^1.0.0"}
}
```

| Campo | Regola |
|---|---|
| `name` | 1–32 caratteri `a-z 0-9 _ -`, uguale alla cartella |
| `version` | semver 2.0.0 (`MAJOR.MINOR.PATCH`, pre-release `-…` con l'ordine di semver, build `+…` ignorata) |
| `language` | `c`, `cpp`, `cs`, `java`, `py` (cartella `C`, `CPP`, `CS`, `JAVA`, `PY`) |
| `description`, `author`, `license` | testo ≤ 200 caratteri (obbligatori `description` e `license`) |
| `firmware` | versione minima del firmware (semver); più nuova del firmware → «richiede il firmware X» |
| `namespace` | C#: nome usato da `using`; Java: package radice; Python: nome del pacchetto/modulo (facoltativo per C/C++) |
| `files` | 1–64 voci `{path, size, sha256}`: percorsi relativi con componenti `A-Z a-z 0-9 _ - .`, niente `..`, assoluti, nascosti, `\`, profondità ≤ 4; `size` ≤ 128 KB; somma ≤ 512 KB; nessun doppione; `library.json` non è nell'elenco |
| `dependencies` | ≤ 16 voci nome → intervallo semver (`1.2.3`, `=1.2.3`, `^1.2`, `~1.2.3`, `>=1.0.0 <2.0.0`, `>`, `<`, `<=`, `*`) |

Un manifest che non rispetta le regole dà un errore chiaro con il campo e la
posizione nel JSON («library.json: riga 4, colonna 15: version "1.2" non è
semver»). JSON secondo RFC 8259 con un parser proprio (profondità ≤ 16,
stringhe ≤ 4 KB, UTF-8 valido, numeri finiti, chiavi ripetute rifiutate).

### 6.3 Indice `index.json`

```json
{
  "format": 1,
  "libraries": [
    {"name": "matrici", "language": "c", "description": "…",
     "versions": [{"version": "1.2.0", "path": "c/matrici/1.2.0/", "manifest_sha256": "…", "size": 3122, "firmware": "1.20.0"}]}
  ]
}
```

L'URL dell'indice è in SET UP (predefinito: la cartella `libraries/` di
questo repository su GitHub; vale se il repository è pubblico, altrimenti si
imposta un altro indirizzo). I percorsi delle versioni sono relativi
all'indice; i file di una versione sono in `<path>/<percorso del file>`. Limiti:
indice ≤ 256 KB, 500 librerie, 32 versioni per libreria. Il repository ha
la cartella **`libraries/`** con `index.json` e librerie d'esempio: C
(`matrici`, `geometria`, `statistica`), C++ (`matrix`, `geom2d`, `stringhe`),
C# (`Matrici`, `Geometria`, `Statistica`); `tools/libraries.py` rigenera
manifest e indice (hash e dimensioni) e controlla che i file non siano
cambiati senza aggiornare la versione.

### 6.4 Menu LIB

| Tasto | Azione |
|---|---|
| elenco | librerie installate di tutti i linguaggi: nome, versione, linguaggio, stato (OK, **da verificare**, **danneggiata**, **firmware vecchio**) |
| F1 INFO | manifest: descrizione, autore, licenza, file con dimensione, dipendenze, chi la usa |
| F2 INSTALL | › INDICE: scarica l'indice (Wi-Fi + HTTPS), elenco delle librerie del linguaggio con le versioni, scelta, piano con le dipendenze, conferma, scaricamento con avanzamento; › PC (USB): riceve una libreria da `tools/picocalc_link.py send-lib` |
| F3 UPDATE | versioni nuove dall'indice (una o tutte); **downgrade solo con conferma** |
| F4 REMOVE | conferma; avviso se un'altra libreria dipende da questa; chiede se togliere anche i dati (LIBDATA) |
| F5 VERIFY | ricontrolla dimensioni e SHA-256 di tutti i file (mancanti, in più, cambiati) |

### 6.5 Fonti d'installazione

1. **Wi-Fi + HTTPS**: client HTTPS con lwIP `altcp_tls` e mbedTLS (già nel
   firmware) in un file nuovo solo della Plus (`src/platform/pico/fetch_pico.cpp`;
   `net_pico.cpp`, condiviso con il Pico 2 W, non cambia), richieste GET con
   corpo in streaming verso la SD (niente file interi in RAM), `Content-Length`
   e `chunked`, fino a 3 redirect solo verso `https`, certificati della CA in
   **`certs/library_ca.pem`** (radici pubbliche: ISRG Root X1/X2, DigiCert Global
   Root G2, USERTrust RSA, GTS Root R1, Amazon Root CA 1), nome dell'host
   verificato. Credenziali Wi-Fi quelle di SYSTEM (le stesse di AI Chat).
2. **Dal PC**: `tools/picocalc_link.py send-lib CARTELLA` controlla o genera
   il manifest (`lib-manifest`) e invia la libreria come pacchetto `.pclink`
   (voci `PICOCALC/LIB/<LING>/<nome>/…`); la PicoCalc la riceve da LIB ›
   INSTALL › PC (USB) o da Link e la installa con lo stesso procedimento
   atomico (dipendenze controllate tra le librerie già installate).
3. **Copia manuale** nella cartella della SD (dal PC): la libreria compare
   come «da verificare»; VERIFY controlla gli hash e la rende usabile.

### 6.6 Installazione atomica

1. piano: libreria scelta + dipendenze risolte (§6.8) + controlli (firmware,
   linguaggio, spazio libero ≥ 2 × la dimensione);
2. ogni libreria del piano si scarica in `LIB/.tmp/<ling>-<nome>-<versione>/`:
   il manifest (SHA-256 confrontato con l'indice), poi ogni file in streaming
   con SHA-256 e conteggio dei byte calcolati mentre arrivano (oltre la
   dimensione dichiarata → stop subito);
3. quando **tutte** le librerie del piano sono scaricate e verificate, il
   giornale `LIB/.tmp/journal.txt` (scritto in modo atomico) elenca le
   sostituzioni; per ognuna la cartella vecchia diventa `LIB/.tmp/old-…`, la
   nuova si rinomina in `LIB/<LING>/<nome>`, poi la vecchia si cancella;
4. all'ingresso nel LIB (e prima di ogni RUN) il recupero legge il giornale:
   rimette la cartella vecchia se la nuova non è al suo posto, completa le
   sostituzioni a metà, cancella il resto di `.tmp`. Un'interruzione (rete,
   AC, SD tolta, batteria) **non lascia mai una libreria a metà**: c'è la
   versione vecchia o quella nuova, completa.

### 6.7 Sicurezza e limiti

- L'installazione **non esegue mai codice**: copia file e controlla hash.
- Le librerie girano **nello stesso interprete confinato** dei programmi:
  l'I/O su file del codice di una libreria va solo nella sua cartella dati
  `PICOCALC/LIBDATA/<LING>/<nome>/`; quello del programma solo nella cartella
  del programma; niente percorsi assoluti, `..`, nomi nascosti, cartelle di
  sistema (test dedicati).
- Limiti: file ≤ 128 KB, libreria ≤ 512 KB e ≤ 64 file, manifest ≤ 16 KB,
  indice ≤ 256 KB, ≤ 16 dipendenze, piano ≤ 32 librerie.
- Manifest o indice malformati → errore chiaro (fuzzer `fuzz_libjson`).
- Al RUN ogni file di libreria letto viene ricontrollato col suo SHA-256: un
  file cambiato dopo l'installazione → «libreria X danneggiata: VERIFY».

### 6.8 Risoluzione

| Direttiva | Ricerca |
|---|---|
| `#include "x.h"` | cartella del file che include, poi cartella del progetto |
| `#include <x.h>` | intestazioni standard virtuali; poi la cartella del progetto; poi le librerie di PICOCALC/LIB/C (in C++: LIB/CPP e poi LIB/C) che hanno un file `x.h` (alla radice o in `include/`). I sorgenti `.c`/`.cpp` della libreria si compilano insieme al programma |
| `using X;` (C#) | namespace standard; poi i file `.cs` del progetto; poi la libreria di LIB/CS con `namespace` = X (o `name` = x) |
| `import p.C;` (Java, v1.21.0) | cartella del progetto secondo il package, poi LIB/JAVA |
| `import x` (Python, v1.22.0) | cartella dello script, poi LIB/PY (`sys.path`) |

Manca → «Syntax ERROR: libreria `<x.h>` non trovata (installa con LIB)» con
file, riga e colonna. **Dipendenze**: per ogni libreria del piano la versione
più alta dell'indice che soddisfa tutti gli intervalli e il firmware; una già
installata che li soddisfa resta; intervalli incompatibili → «conflitto: X
^1 (da A) e ^2 (da B)»; dipendenza assente dall'indice → «dipendenza
mancante: X»; ciclo → «ciclo di dipendenze: A → B → A» (rifiutato, Q14).

## 7. Archivio, editor e modalità (progetto)

| Parte | File | Note |
|---|---|---|
| Archivio del codice | `include/code_store.hpp`, `src/code_store.cpp` | cartelle, nomi, elenco con ordinamento, copia, spostamento, rinomina e cancellazione ricorsive, `.meta`, migrazione, messaggi «Inserire la scheda SD»/«SD piena»/«SD in sola lettura»; sopra `files::FileSystem` (SD); `files::Reader` acquista `seek` (FatFs `f_lseek`, RAM) per i file grandi; `RamFileSystem` acquista sola lettura e rinomina di cartelle |
| Editor | `include/code_editor.hpp`, `src/code_editor.cpp`, `src/code_syntax.cpp` | documento a pagine (indice sparso delle righe ogni 64, cache di pagine da 4 KB, pezzi della SD e righe modificate in RAM, salvataggio in streaming con `writeAtomic`), undo/redo, ricerca e sostituzione a passi per i file grandi, rientro, parentesi, evidenziazione per linguaggio con lo stato dei commenti ai punti dell'indice. Il `TextEditor` delle altre modalità non cambia |
| Librerie | `include/lib_manager.hpp`, `src/lib_manager.cpp`, `src/lib_json.cpp`, `src/sha256.cpp`, `include/net_fetch.hpp` | manifest, indice, semver, piano, installazione a passi (step::Task con AC), giornale e recupero, VERIFY, REMOVE; trasporto astratto `net::Fetch` (dispositivo: `fetch_pico.cpp`; PC: simulato nei test e mbedTLS + socket in `lib_https_test`) |
| Modalità | `src/apps/program_app.cpp`, `src/apps/program_files.cpp`, `src/apps/program_edit.cpp`, `src/apps/program_debug.cpp`, `src/apps/program_lib.cpp`, `src/apps/program_ui.hpp` | gestore dei file, editor, esecuzione e debug, LIB sul `ModeShell` |
| Record | sezione `0x010A` **versione 2** | cartella corrente, linguaggio, file recenti (8, con riga e colonna), ordine, opzioni dell'editor, limite di tempo, URL dell'indice, stato della migrazione |

## 8. Differenze inevitabili (previste)

| # | Differenza | Motivo |
|---|---|---|
| D1 | Linguaggi C, C++, C# (Java dalla v1.21.0) al posto del Casio Basic; i comandi del capitolo 8 (Locate, Getkey, comandi grafici, liste, matrici, statistiche) sono funzioni della libreria `fx` | scelta dell'utente (v1.15.0) |
| D2 | Nomi di 1–32 caratteri con minuscole ed estensione (fx-CG50: 8 maiuscole) | file sorgenti veri, nomi delle classi Java, file del PC |
| D3 | Cartelle e progetti con più file (fx-CG50: elenco piatto) | richiesta della versione |
| D4 | Password nel file `.meta` della SD: protegge solo l'interfaccia, come sulla fx-CG50 (il sorgente resta leggibile sul PC) | i sorgenti non si toccano |
| D5 | SAVE/LOAD in testo della fx-CG50 non servono: i programmi sono già file di testo (SAVE·AS dell'editor) | |
| D6 | `Prog "nome"` dal Run-Matrix e tra programmi non c'è: un programma ne usa un altro con include/using/import e le librerie | i programmi non sono Casio Basic |
| D7 | Grafica, font, tasti e barre propri della PicoCalc | |
| D8 | DEBUG passo passo con variabili e punti d'interruzione: **estensione** (la fx-CG50 ha solo il salto all'errore) | richiesta della versione |
| D9 | L'overflow intero con segno di C e C++ (indefinito nella norma) è definito: avvolgimento a complemento a due; `long` a 64 bit (LP64) | comportamento prevedibile per un interprete |
| D10 | `unordered_map`/`unordered_set` del C++ iterano nell'ordine d'inserimento (la norma non fissa l'ordine) | deterministico |
| D11 | I temporanei del C++ (valori di ritorno, copie nelle espressioni) non chiamano il distruttore, come con la copy elision; i distruttori si chiamano per le variabili, `delete`, i membri e gli elementi dei contenitori | semplicità; la norma permette l'elisione in molti casi |
| D12 | `time()` conta i secondi dall'accensione | la PicoCalc non ha l'orologio |

## 9. Domande aperte e risposte scelte

| # | Domanda | Risposta |
|---|---|---|
| Q1 | «Interpretati dal sorgente, niente bytecode»: si può avere una rappresentazione interna? | Sì, solo in RAM durante l'esecuzione (albero → istruzioni di una macchina a pila iterativa), creata a ogni RUN dal sorgente sulla SD e mai salvata. Un interprete ad albero ricorsivo userebbe 1–3 KB di pila nativa per ogni chiamata del programma: con 2 000 chiamate annidate non starebbe in SRAM |
| Q2 | Come richiamare codice del programma dalle funzioni native senza bloccare l'interfaccia? | Coroutine con pila propria (48 KB in SRAM) come Python: le funzioni native possono chiamare il programma (comparatori, `ToString`, `operator<<`) e la fetta di tempo può finire ovunque |
| Q3 | `long` in C/C++? | 64 bit (LP64, come un PC Linux); `int` 32 |
| Q4 | Overflow con segno | avvolgimento definito (D9) |
| Q5 | Divisione intera per zero | errore in tutti i linguaggi (C/C++: «Math ERROR: divisione per zero»; C#: `DivideByZeroException`); `INT_MIN / -1` avvolge in C/C++, in C# dà `OverflowException` come .NET |
| Q6 | Gestione della memoria | un heap con GC mark-and-sweep per tutti; in C/C++ `malloc`/`new` restano finché `free`/`delete` (radici) |
| Q7 | Distruttori dei temporanei | non chiamati (D11) |
| Q8 | Tipo di un blocco di `malloc` | alla prima conversione a `T*` diventa un array di `size / sizeof(T)` elementi di T (resto ≠ 0 → errore); `calloc` azzera; `realloc` conserva il tipo |
| Q9 | File scritti dai programmi | atomici alla chiusura; uscita normale = chiusura; errore/AC = scartati (§5.9) |
| Q10 | Cartella dati delle librerie | `PICOCALC/LIBDATA/<LING>/<nome>`, conservata da UPDATE, tolta da REMOVE su conferma |
| Q11 | Dove sta la password | `PICOCALC/CODE/.meta`, una riga per file: percorso, sale e SHA-256 di sale+password |
| Q12 | Il C++ può usare le librerie C? | Sì: ogni file si analizza nel suo dialetto e il nucleo li collega in un solo programma |
| Q13 | Come `using X` trova una libreria C# | campo `namespace` del manifest (o il nome) |
| Q14 | Dipendenze cicliche | rifiutate con l'errore che mostra il ciclo |
| Q15 | SHA-256 | implementazione propria FIPS 180-4 (`src/sha256.cpp`, vettori del NIST nei test), uguale sul PC e sul dispositivo |
| Q16 | Trasporto HTTPS | un client nuovo solo della Plus (`fetch_pico.cpp`) sopra lo stesso Wi-Fi; `net_pico.cpp` (anche del Pico 2 W) non cambia; CA in `certs/library_ca.pem` |
| Q17 | URL predefinito dell'indice | `https://raw.githubusercontent.com/gabrieleeli/PicoCalc-FxCG-50/main/libraries/index.json`, modificabile in SET UP |
| Q18 | Senza SD | Program apre un file senza nome che si esegue ma non si salva; Python ha la SHELL |
| Q19 | Stato di Program | Base fino alla v1.21.0 (Java) |
| Q20 | Python nella v1.20.0 | gli script passano a `PICOCALC/CODE/PY` sulla SD con la migrazione (stessa regola «programmi solo sulla SD»); il resto di Python nella v1.22.0 |
| Q21 | DEL-ALL | tutti i file e le cartelle della cartella corrente (non le altre cartelle dei linguaggi), conferma con i numeri |
| Q22 | Granularità del debug | per istruzione del sorgente (una riga può averne più d'una); punti d'interruzione per riga |
| Q23 | Interprete della v1.15.0 | sostituito dal nucleo nuovo con la stessa API (`program.hpp`); `program_core_test` resta come prova di regressione (le voci che erano «non supportato» e ora funzionano, come `int *p`, cambiano aspettativa con il motivo) |
| Q24 | Limite di tempo | impostazione di SET UP, iniziale Off (AC sempre attivo) |
| Q25 | Lambda in C++ e C# | sì, semplici: lo stesso modello di chiusure serve a Java nella v1.21.0 |
| Q26 | `rand()` del C | generatore lineare della norma (esempio di C11 7.22.2.2, `RAND_MAX` 32767), deterministico con `srand`; `Random(seed)` di C# riproduce la sequenza di .NET |
| Q27 | Stringhe | C/C++: byte UTF-8 (`strlen` conta byte); C#/Java: UTF-16 (`Length` conta unità UTF-16) |
| Q28 | `sizeof` | modello LP64 (char 1, short 2, int 4, long 8, float 4, double 8, puntatore 8, struct con allineamento) |
| Q29 | Programmi con lo stesso nome in linguaggi diversi | ammessi (cartelle diverse) |
| Q30 | File grandi nell'editor e nell'interprete | l'editor li apre fino a 8 MB a blocchi; l'interprete esegue sorgenti fino a 256 KB per file (§5.7) |

## 10. Piano dei test, del fuzzing e del simulatore

| Suite | Contenuto |
|---|---|
| `lang_c_test`, `lang_cpp_test`, `lang_cs_test` | un caso per costrutto (LC-01…LC-22, LP-01…LP-19, LS-01…LS-22) con uscita attesa scritta a mano; per i «non supportati» l'errore con riga e colonna |
| `lang_core_test` | LK-01…LK-07, FX-01…FX-08, DB-01…DB-07 (API del debug), RN-01…RN-06, limiti, AC in ogni stato, GC (cicli, budget), nessun file aperto dopo AC o errore, confinamento dei percorsi, SD tolta durante l'I/O |
| `program_core_test` | la suite della v1.15.0 sul nucleo nuovo (regressione) |
| `code_store_test` | SD-01…SD-07, PW-05/06, MG-01…MG-04; interruzione simulata in ogni operazione e dopo ogni byte |
| `code_editor_test` | ED-01…ED-13; file di 2 MB e di 200 000 righe con la RAM misurata; undo/redo casuali confrontati con un modello |
| `lib_manager_test` | LB-01…LB-15 con il trasporto simulato: successo, hash sbagliato, dimensione sbagliata, interruzione a ogni byte, AC, SD tolta a ogni operazione, dipendenze mancanti, conflitti, cicli, downgrade, recupero dal giornale |
| `lib_https_test` | server HTTPS di prova sul PC (`tools/lib_test_server.py`, certificati generati con openssl), client mbedTLS 3.6.2 del Pico SDK compilato per il PC: successo, hash sbagliato, connessione chiusa a metà, dipendenze mancanti, ciclo; saltato se mancano SDK od openssl |
| `program_flow_test` | PM-01…PM-15, PW-01…PW-04, RN-*, ED-* dall'interfaccia, DB-* dall'interfaccia, LIB dall'interfaccia, migrazione |
| `link_tool_test` | `send-lib`, `lib-manifest`, file di codice |
| Esistenti | `architecture_test` (servizi e sorgenti nuovi, Pico 2 W congelato), `apps_init_test`, `base_modes_test`, `base_flow_test`, `typed_files_test`, `workspace_v117_test` aggiornati dove cambiano Program e Python |

Fuzzer (≥ 300 s ciascuno sul codice finale): **nuovi** `fuzz_program_cpp`,
`fuzz_program_cs`, `fuzz_libjson` (manifest, indice, semver),
`fuzz_codeeditor`; **toccati** `fuzz_program` (C, nucleo nuovo), `fuzz_modes`
(sezione `0x010A` v2), `fuzz_workspace`, `fuzz_link` (voci di codice e
librerie), `fuzz_ui` (Program). Simulatore: screenshot nuovi per il gestore
dei file, l'editor, il LIB e il DEBUG; i vecchi identici salvo 141–142 (la
Program della v1.17.0 diventa il gestore dei file e l'editor nuovo:
rigenerati con il motivo).

## 11. Prove su hardware previste (HW-296…)

Installazione di una libreria via Wi-Fi reale (indice, dipendenze, AC a metà,
rete tolta), SD tolta durante il salvataggio e durante l'esecuzione, SD piena
e protetta, migrazione dei `.pcprog` dalla v1.19.0, file di 2 MB nell'editor
(tempi di apertura, scorrimento, ricerca, salvataggio), tempi
dell'interprete (ciclo di 10⁶ iterazioni, ricorsione profonda, GC), pila della
coroutine del nucleo nuovo (MSPLIM, nessun `[fault]`), DEBUG, password, LIB
dal PC con `picocalc_link.py send-lib`. Dettaglio in
[HARDWARE_TEST_v1.20.0.md](../HARDWARE_TEST_v1.20.0.md).
