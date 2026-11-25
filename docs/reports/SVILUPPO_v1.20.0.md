# Sviluppo della v1.20.0 — Program sulla microSD, C/C++/C# e librerie

Obiettivo della versione: la modalità **Program** sulla Pimoroni Pico Plus 2 W
con i programmi **C, C++ e C#** come file sorgente standard sulla microSD, un
gestore dei file con le funzioni del capitolo 8, un editor per il codice
(anche file grandi), il DEBUG, la libreria `fx` completa e un **gestore delle
librerie** (indice HTTPS, SHA-256, dipendenze, installazione atomica).
Program resta **Base** fino a Java (v1.21.0). Analisi, tabelle e decisioni
scritte prima del codice: [ANALISI_FONTI_v1.20.0.md](ANALISI_FONTI_v1.20.0.md).
Il **Raspberry Pi Pico 2 W resta congelato** alla v1.16.0. **Nessuna prova su
hardware**: [HARDWARE_TEST_v1.20.0.md](../HARDWARE_TEST_v1.20.0.md)
(HW-296…HW-313).

## 1. Git

| Voce | Valore |
|---|---|
| Partenza | `main` = `bcd28af` (v1.19.0); `git fetch origin main` all'inizio e prima della pubblicazione |
| Branch di lavoro | `claude/blissful-knuth-x1ei65` (commit intermedi: analisi, nucleo, archivio ed editor, librerie, fuzzing, corpus, correzioni, documentazione) |
| Pubblicazione | un solo commit su `main` in fast-forward, «PicoCalc FxCG-50 v1.20.0: Program …» |

## 2. Decisioni

Le domande della fase 0 hanno le risposte in
[ANALISI_FONTI §9](ANALISI_FONTI_v1.20.0.md#9-domande-aperte-e-risposte-scelte)
(Q1–Q30). Durante lo sviluppo si sono aggiunte:

| # | Decisione | Motivo |
|---|---|---|
| S1 | **Corpus differenziale**: 51 programmi tipici (`tests/program_corpus`) con le uscite di gcc 13, g++ 13 (C11/C++17) e Mono 6 (`mcs`), generate da `tools/program_corpus_ref.py` e confrontate byte per byte dall'interprete (`program_corpus_test`) | i test per costrutto non bastavano: i due lotti del corpus hanno trovato più di 25 difetti reali (§6, F12–F18) |
| S2 | Il riferimento di C# è **.NET** (non Mono): dove Mono è diverso (`Math.Round(2.5)` dà 3 su Mono, 2 su .NET) il corpus evita il caso e la prova per costrutto usa il valore di .NET | .NET è l'implementazione di riferimento di C# |
| S3 | C# con la **cultura invariante**: `3.5`, `1,234.50`, `Infinity`/`-Infinity`/`NaN` anche in `ToString()` | la PicoCalc non ha impostazioni locali; prima `ToString()` scriveva `∞` e `F2` `Infinity` |
| S4 | `strcmp`, `strncmp`, `memcmp` (C) e `std::string::compare` restituiscono la **differenza dei byte** come glibc e libstdc++ | i programmi che stampano il risultato danno lo stesso numero del PC; la norma fissa solo il segno |
| S5 | Le **funzioni della libreria** si usano come valori: `integra(sin, 0, M_PI)` in C/C++ e `Simpson(Math.Sin, …)` in C# passano da una funzione generata che chiama il nativo (fra più versioni quella con parametri e risultato `double`) | idioma comune dei programmi numerici |
| S6 | Lambda passate a un **template** (`template <class F> double integra(F f, …)`): il tipo di F viene dai parametri dichiarati della lambda e dal suo primo `return`; è un tipo «oggetto lambda» che come parametro non diventa un puntatore a funzione, così passano anche le lambda con catture | C++ moderno per i metodi numerici |
| S7 | `ifstream`/`ofstream`/`fstream` locali sono valori: alla fine del blocco il «distruttore» scrive e chiude il file | `{ ofstream o("a.txt"); o << …; }` seguito dalla lettura è un idioma comune |
| S8 | DEBUG: la posizione mostrata è l'istruzione **che sta per essere eseguita**; un punto d'interruzione scatta quando l'esecuzione **entra** nella riga (cornice per cornice); OUT si ferma nel chiamante **sulla riga della chiamata** | prima la schermata mostrava l'istruzione precedente, i cicli su una riga si fermavano a ogni pezzo e OUT saltava alla riga dopo |
| S9 | Le domande di testo di Program (Trova, Sostituisci, Vai alla riga, password) hanno una finestra propria in `program_app.cpp` | la finestra dei nomi condivisa (`widgets.cpp`, congelata con il Pico 2 W) scrive sempre «Nome:»/«EXE salva» e non accetta il testo vuoto, che serve per «sostituisci con niente» e per togliere la password |
| S10 | Il testo proposto in una domanda (la ricerca precedente) è come selezionato: scrivendo si sostituisce, DEL lo cancella, le frecce lo tengono | prima si aggiungeva al testo vecchio |
| S11 | Un file protetto non si apre nemmeno con DEBUG (che mostra il sorgente) né con il salto all'errore senza la password della sessione | PW-02 e PW-04: prima DEBUG mostrava il sorgente senza chiedere |
| S12 | C#: `IComparable<T>`, `IEquatable<T>`, `IDisposable` (e in Java `Comparable`, `Cloneable`, `Serializable`, `AutoCloseable`) nella lista delle basi sono accettate senza una classe del nucleo: i metodi si trovano per nome | `List.Sort()` e i vincoli `where T : IComparable<T>` funzionavano già |
| S13 | Programmi senza Java nell'elenco: un `.java` dà «Java arriva nella v1.21.0»; un `.py` rimanda a Python | ordine delle versioni |
| S14 | La password si conserva come SHA-256 di `"picocalc-code:" + password` in `PICOCALC/CODE/.passwords` (non salato) | protegge solo l'interfaccia come sulla fx-CG50 (D4): il sorgente sulla SD resta leggibile, un sale non aggiungerebbe protezione |

## 3. Scostamenti dall'analisi

| Voce dell'analisi | Implementato | Motivo |
|---|---|---|
| ED-10: file grandi fino a **8 MB** e 200 000 righe | **4 MB** a blocchi di righe intere (256 KB si aprono per intero), provato con un file di 3 MB in `program_flow_test` | limite dichiarato e provato; l'indice dei blocchi resta piccolo in RAM |
| Q30: interprete fino a 256 KB per file | come previsto; progetto 1 MB, 64 file | |
| PW-05: password nel file `.meta` con SHA-256 salato | `.passwords`, SHA-256 non salato (S14) | |
| SD-07: nomi di 1–32 caratteri | 1–40 caratteri, `.` ammesso dentro il nome | nomi dei file del PC (`dati.2024.csv`) |
| §6.2–6.3: campi `firmware`, `format`, elenco `versions` nell'indice | `minFirmware`; indice `{"version": 1, "libraries": [...]}` con una voce per versione e `manifestSize` | una voce per versione rende l'indice lineare per il parser e per `tools/libraries.py` |
| §6.7: file ≤ 128 KB, libreria ≤ 512 KB, indice ≤ 256 KB | file ≤ 256 KB, libreria ≤ 1 MB, indice ≤ 64 KB | sorgenti più grandi delle librerie, indice in RAM durante il piano |
| §6.5: fino a 3 redirect; LIB › INSTALL › PC (USB) | solo risposte 200 (raw.githubusercontent.com non usa redirect); dal PC `send-library` scrive in `LIB/IMPORT` e LIB › LOCAL installa con gli stessi controlli | un solo percorso d'installazione da provare |
| §7: file separati (`program_files.cpp`, `program_edit.cpp`, `code_editor.cpp`…) | `src/apps/program_app.cpp` e il `TextEditor` comune con i blocchi gestiti dalla modalità | il `TextEditor` (condiviso) non cambia |
| §10: suite `lang_c_test`, `lang_cpp_test`, `lang_cs_test`, `lang_core_test`, `code_editor_test`; fuzzer `fuzz_program_cpp`, `fuzz_program_cs`, `fuzz_codeeditor` | `program_lang_test` (un caso per costrutto dei tre linguaggi, FX, DB), `program_corpus_test`, `code_store_test`, `lib_manager_test`, `libraries_test`, `program_flow_test`, `https_lib_test`; fuzzer `fuzz_c`, `fuzz_cpp`, `fuzz_cs`, `fuzz_codestore`, `fuzz_libjson`, `fuzz_http` | stesse voci, raggruppate per nucleo |
| PM-12 SET UP (linguaggio, rientro, tabulazione, numeri di riga, **limite di tempo**) | Indice LIB, ordine e MIGR | il limite di tempo non serve con AC sempre attivo (`Limits::timeLimitMs` resta nell'API) |
| ED-09: «Salvare?» all'uscita | EXIT salva | come le altre modalità con file della PicoCalc |
| ED-12: CRLF e BOM conservati | l'interprete legge BOM e CRLF; l'editor salva con LF | il `TextEditor` comune toglie `\r` |

**Rimandati alla v1.21.0** (quando Program diventa Available e la tabella del
capitolo 8 deve essere completa per i quattro linguaggi): PM-01 segno `*`
(ora «(password)»), PM-05 più modelli e password a NEW, PM-13 menu RECENT
(i file recenti sono già nel record), PM-15/SD-02 file senza nome senza SD,
ED-01 numeri di riga nascosti, ED-04 JUMP › MATCH e parentesi senza compagna
in rosso, ED-06 sostituzione una per volta e maiuscole/minuscole, ED-11 MENU
dei costrutti e CHAR, DB-05 schermata CALLS con la scelta della cornice
(oggi la pila si vede nell'intestazione e nell'errore), SD-03 messaggio
distinto per la SD protetta.

## 4. Cosa c'è nel codice

| Parte | File | Note |
|---|---|---|
| Nucleo dell'interprete | `include/program.hpp`, `src/program_lex.cpp`, `src/program_parse*.cpp`, `src/program_compile*.cpp`, `src/program_vm.cpp`, `src/program_heap.cpp`, `src/program_format.cpp`, `src/program_natives.cpp`, `src/program_lib*.cpp`, `src/program_fx.cpp`, `src/program_project.cpp` | sorgente → AST → istruzioni di una macchina a pila iterativa (nessuna ricorsione nativa per le chiamate del programma), valori da 16 byte, heap TLSF nell'arena (PSRAM) con GC mark-and-sweep, coroutine da 48 KB per i nativi che richiamano il programma, debug, limiti |
| Archivio | `include/code_store.hpp`, `src/code_store.cpp` | cartelle per linguaggio, progetti, nomi, scritture atomiche anche a blocchi (`spliceBlock`), password, ricerca, migrazione, `loadProject` con le librerie |
| Librerie | `include/lib_manager.hpp`, `src/lib_manager.cpp`, `include/json_lite.hpp`, `src/sha256.cpp`, `include/lib_fetch.hpp`, `src/lib_fetch.cpp`, `src/platform/pico/fetch_pico.cpp`, `certs/library_ca.pem` | manifest, indice, semver, piano, installazione a passi con giornale e recupero, VERIFY, dipendenti; HTTPS con mbedTLS solo sulla Plus |
| Modalità | `src/apps/program_app.cpp` | gestore dei file, editor, esecuzione, DEBUG, LIB, SET UP, migrazione sul `ModeShell` |
| Repository delle librerie | `libraries/` (9 librerie, `index.json`), `tools/libraries.py` | |
| Strumenti | `tools/picocalc_link.py send-library`, `tools/program_corpus_ref.py`, `tools/fuzz/fuzz_{c,cpp,cs,libjson,http,codestore}.cpp`, `tests/sim/program_v120.txt`, simulatore con `sdcopy` | |
| Esempi | `docs/examples/program` (5 programmi con le uscite) | |

## 5. Differenze inevitabili

Come previsto ([ANALISI_FONTI §8](ANALISI_FONTI_v1.20.0.md#8-differenze-inevitabili-previste)),
D1–D12. In più: C# con la cultura invariante (S3).

## 6. Errori trovati durante lo sviluppo

| # | Trovato da | Errore | Correzione | Prova |
|---|---|---|---|---|
| F1 | `program_lang_test` | il GC non liberava nulla durante `main` (i temporanei dei nativi restavano radici) | radici dei temporanei tolte dopo ogni istruzione | GC nei test dei limiti |
| F2 | `program_lang_test` | `malloc` non restituiva mai `NULL` | riserva di byte nell'heap | LC-18 |
| F3 | `program_flow_test` | lo stato del file grande aperto restava al file successivo | azzerato in `openEditor` | flusso «file grande» |
| F4 | `fuzz_http` | il corpo poteva superare il limite di un pacchetto | controllo prima di aggiungere | `tests/fixtures/fuzz/fuzz_http_*` |
| F5 | clang (build dei fuzzer) | `std::pair` con `json::Value` incompleto | `json::Field` | build dei fuzzer |
| F6 | `program_lang_test` (debugger) | la pausa mostrava la riga dell'istruzione precedente; i punti d'interruzione scattavano a ogni pezzo della riga; OUT saltava alla riga dopo | S8 | DB-01…DB-07 |
| F7 | screenshot 207, 216 | le domande dicevano «Nome:»/«EXE salva»; sostituire con niente e togliere la password erano impossibili | S9, S10 | `program_flow_test` |
| F8 | `fuzz_cs`, `fuzz_program` (218 artefatti) | `(` senza `)` fino alla fine del sorgente C#: lettura oltre i token | controllo della `)` in `castAhead` | `program_lang_test` |
| F9 | `fuzz_cpp` | una `std::string` lunga distrutta a fine blocco: scambio con una stringa dell'allocatore predefinito → `operator delete` sull'arena (heap corrotto anche sul dispositivo) | stessa risorsa dell'arena | `program_lang_test` (ciclo con stringhe di 200 byte) |
| F10 | `fuzz_cpp` | `std << 1`: tipo nullo dereferenziato | «operando che non e' un valore» | `program_lang_test` |
| F11 | `fuzz_modes` | un `.pcgeo` della v1.17.0 con il centro della vista oltre 1e150 si apriva ma dopo il salvataggio nel formato 2 non si rileggeva (errore della v1.19.0) | centro fuori scala all'origine nella migrazione | `geometry_test` P02 |
| F12 | corpus | `std::string s(5, 'x')` e `std::string s("ab")` lasciavano la pila sbagliata (valori casuali, errore al giro dopo) | la stringa nuova resta sulla pila | corpus `strings.cpp`, `program_lang_test` |
| F13 | corpus | `(std::string("a") < b)` e `(int(x) + 1)` presi per un cast | il cast vuole il tipo chiuso da `)` | corpus `strings.cpp` |
| F14 | corpus | `case Colore::Rosso:` (enum class) non era costante | costanti dal compilatore delle espressioni | corpus `misc.cpp` |
| F15 | corpus | `qsort` di un array di struct: `sizeof` della struct ignoto a tempo di esecuzione | dimensioni delle struct registrate dopo la compilazione | corpus `memory.c` |
| F16 | corpus | `ofstream` locale mai scritto alla fine del blocco | S7 | corpus `files.cpp` |
| F17 | corpus | C#: `IComparable<T>` ignoto, `T` ignoto negli inizializzatori dei campi delle classi generiche senza costruttore, `StringSplitOptions`, `new string(char[])`, `Array.Resize(ref …)`, `double.IsNaN`, `string.Equals` statico, `Punto + " "` (operatore dell'utente al posto della concatenazione), `bool` negli argomenti `params object[]`, `string.CompareOrdinal`, `StringBuilder.AppendFormat`, `Clone` degli array | ognuno corretto o aggiunto | corpus C# |
| F18 | corpus | C++: `c ? "testo" : std::string`, `vector` costruito da iteratori di `map`, lambda a un template, funzioni della libreria come puntatori | S5, S6 e le regole del ternario e del costruttore | corpus `quadratic.cpp`, `text_stats.cpp`, `numeric.cpp`, `funcptr.c` |
| F19 | `fuzz_cpp` sul codice di F18 | la deduzione dei template (S6) riassegnava gli scope del chiamante: `LocalVar&` di `initVariable` invalidato (use-after-free) | si aggiunge e si toglie solo lo scope temporaneo | `fuzz_cpp`, corpus sotto ASan |
| F20 | revisione dell'analisi | DEBUG dall'elenco mostrava il sorgente di un file protetto senza password | S11 | `program_flow_test` |

## 7. Test, fuzzing, simulatore

@@TEST@@

## 8. Firmware e memoria

@@FIRMWARE@@

## 9. Cancello

@@GATE@@
