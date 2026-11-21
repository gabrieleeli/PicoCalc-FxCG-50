# Prompt — fondamenta comuni per tutte le applicazioni (v1.15.0)

Questo file contiene il prompt da dare a una nuova sessione di sviluppo
(Claude Code o altro agente) per **mettere le basi comuni** su cui costruire
tutte le modalità che la PicoCalc FX-CG50 può ospitare. Lo stato di partenza è
descritto in [reports/RECAP_v1.14.0.md](reports/RECAP_v1.14.0.md).

Come usarlo: copiare il blocco del §1 come primo messaggio della sessione.
Il §2 contiene i prompt brevi, uno per modalità, da usare **dopo** che la
v1.15.0 è su `main`.

---

## 1. Prompt principale

````text
Sei lo sviluppatore del repository gabrieleeli/PicoCalc-FxCG-50: firmware C++17
per la ClockworkPi PicoCalc (Pimoroni Pico Plus 2 W come scheda principale,
Raspberry Pi Pico 2 W come profilo alternativo) che riproduce la Casio fx-CG50.
Scrivi documentazione e messaggi di commit in italiano, come il resto del repo.

OBIETTIVO DELLA SESSIONE — v1.15.0 «Fondamenta delle applicazioni»
Costruire l'infrastruttura comune che serve a TUTTE le modalità non ancora
realizzate o incomplete, così che ogni modalità successiva sia «nucleo puro +
controller sottile» sopra servizi già testati. Le modalità interessate:
Statistics (completamento), Graph/Table (Dual Screen, Graph to Table),
eActivity, Spreadsheet, Dyna Graph, Recursion, Conic Graphs, Program,
Financial, Link, Memory e System (completamento), Geometry, Picture Plot,
Python; E-CON4 resta External (solo import CSV nelle liste).
Questa sessione NON rende «Available» nessuna modalità nuova: produce servizi,
modelli persistiti, nuclei puri e test. Nessuna schermata dimostrativa o
finta: le modalità Planned continuano a mostrare la schermata «non disponibile».

PASSO 0 — Leggere prima di toccare il codice
- CONTINUE.md, docs/reports/RECAP_v1.14.0.md, docs/APPLICATIONS_ARCHITECTURE.md,
  docs/ARCHITECTURE.md, docs/STORAGE.md, docs/PICO_PLUS_2W.md, docs/TESTING.md,
  docs/BUILD_STATUS.md, docs/ROADMAP.md, docs/FEATURE_MATRIX.md.
- Codice: include/app.hpp, include/app_registry.hpp, src/app_registry.cpp,
  include/app_models.hpp, include/workspace.hpp, src/workspace.cpp (record v10),
  include/app_widgets.hpp, include/ui_kit.hpp, include/graph_engine.hpp,
  include/file_store.hpp, include/limits.hpp, include/mem_pools.hpp,
  src/apps/graph3d_app.cpp (calcolo progressivo con AC), tests/architecture_test.cpp,
  cmake/fxai_sources.cmake, scripts/build.sh, .github/workflows/release.yml.
- Verificare il punto di partenza: `tests/run_host_tests.sh` (entrambi i profili)
  e `scripts/build.sh --board pico_plus_2w` / `--board pico2w` devono passare
  senza warning PRIMA di modificare qualcosa. Annotare commit di partenza,
  numero di controlli e dimensioni del firmware.

PASSO 1 — Analisi delle fonti (docs/reports/ANALISI_FONTI_v1.15.0.md), prima del codice
Usa il manuale software ufficiale della fx-CG50 (versione 3.60 o successiva,
PDF Casio) SOLO come descrizione del comportamento: niente codice, icone,
font, immagini o testi copiati oltre ai nomi dei comandi e dei messaggi.
Per ogni modalità elencata sopra, dal capitolo corrispondente (verifica i
numeri nell'indice: Statistics 6, Financial 7, Program 8, Spreadsheet 9,
eActivity 10, Memory 11, System 12, Link 13, Geometry 14, Picture Plot 15,
Dyna Graph 5-42, Recursion 5-45, Conic Graphs 5-50, Dual Screen nel cap. 5,
Python nel suo capitolo) ricava:
  a) le schermate e i tasti funzione (F1–F6, sottomenu, SHIFT, OPTN, VARS);
  b) i dati che la modalità conserva e i limiti (dimensioni, numeri massimi);
  c) i dati che scambia con le altre modalità (liste, V-Window, Y=, variabili);
  d) le operazioni lunghe (ricalcolo, animazione, esecuzione) che vanno
     interrotte con AC;
  e) i componenti comuni che usa (dalla tabella del PASSO 2).
Il risultato è una matrice «modalità × fondamenta» che giustifica ogni
servizio del PASSO 2 con almeno due modalità che lo usano. Un servizio usato
da una sola modalità resta nel nucleo di quella modalità.

PASSO 2 — Fondamenta da realizzare (ognuna con test host dedicati)

F1. Budget per scheda (da fare per primo)
    Il firmware del Pico 2 W occupa il 92 % della partizione da 2 MB: le
    modalità mancanti non ci stanno. Misura (`arm-none-eabi-size`, mappa del
    linker, `-fstack-usage`) e proponi in ANALISI_FONTI una delle strade,
    motivandola con i numeri:
      1. modalità selezionabili per scheda: opzioni CMake `FXAI_MODE_<NOME>`
         in cmake/fxai_sources.cmake, campo nel registro (`boards`) e
         `architecture_test` che verifica che una modalità esclusa sia
         `Planned` su quella scheda con il motivo nel testo `missing`;
      2. riduzione di dimensione (`-Os` sui moduli UI, `--gc-sections`
         controllato, tabelle di stringhe condivise) con misura prima/dopo;
      3. nuova mappa della flash del Pico 2 W (4 MB fisici) con migrazione del
         workspace e della LittleFS, test in `board_profile_test`.
    Implementa la strada scelta (si possono combinare 1 e 2). La Pico Plus 2 W
    resta il profilo completo. `cmake/check_firmware_size.cmake` deve fallire
    se una scheda supera il proprio limite.

F2. Workspace a sezioni estensibili — record v11
    Oggi ogni modalità nuova costringe a una nuova versione del record. Introduci
    una coda di sezioni «di modalità» in formato TLV (id della sezione, versione
    della sezione, lunghezza, dati) dopo i campi della v10:
      - una sezione sconosciuta viene conservata intatta e riscritta
        (così un firmware più vecchio con il record v11 non perde i dati: questo
        risolve anche A1 di ERRORI_v1.13.1.md per le versioni successive);
      - lettura di tutte le versioni 1–10 invariata; fixture nuova
        `tests/fixtures/workspace_v1_14_0.inc` generata con i sorgenti della v1.14.0;
      - budget del record per scheda (59,75 KB sul Pico 2 W, 512 KB sulla Plus)
        con messaggio «Memoria flash piena» già esistente;
      - `fuzz_workspace` esteso ai record v11 con sezioni corrotte, troncate,
        duplicate e sconosciute.
    Sposta da include/app_models.hpp nel workspace, come sezioni TLV, i modelli
    piccoli e di stato: Recursion, Dyna Graph, Conic Graphs, Financial
    (variabili TVM e Setup), Picture Plot (riferimenti), Geometry (stato della
    vista). Default vuoti, `resetAll`, MEMORY che li mostra e li cancella.
    Dichiara le nuove sezioni `data::` nel registro: `architecture_test` deve
    continuare a verificare chi le usa.

F3. File tipizzati per i documenti dell'utente
    Programmi, documenti eActivity, fogli, script Python, costruzioni Geometry e
    progetti Picture Plot NON stanno nel record: vanno nei file (microSD o
    LittleFS, scelta già presente in SYSTEM) tramite il servizio Files.
      - tabella dei tipi: cartella (`PICOCALC/PROG`, `EACT`, `SHEET`, `PY`,
        `GEOM`, `PPLOT`, oltre alle esistenti GRAPH e PICT), estensione,
        validatore del nome (`validateProgramName`, `validateScriptName`, …),
        intestazione con versione e CRC per i formati binari;
      - API comune: elenco, apri, salva/salva come con scrittura atomica,
        rinomina, elimina, dimensione, errori dello stile fx-CG50;
      - `FileDialog` usa la tabella dei tipi (un solo widget per tutte le
        modalità); MEMORY ottiene la vista dei file per tipo;
      - test con `RamFileSystem` e iniezione di guasti, microSD simulata e
        LittleFS con interruzione di alimentazione (come `internal_fs_test`).
    Documenta i formati in un nuovo docs/FILE_FORMATS.md.

F4. Guscio comune delle modalità (`ModeShell`)
    Estrai dai controller esistenti (Graph, Table, Equation, 3D Graph) gli
    schemi ripetuti, senza cambiare il comportamento visibile:
      - pila di schermate (lista → editor → risultato → grafico) con EXIT/AC
        coerenti, ritorno alla schermata giusta da SET UP (`Context::resuming`);
      - barra F1–F6 con sottomenu, pagine (▷) e SHIFT, descritta da tabelle;
      - lista di voci selezionabili con stato, colore e stile (come la lista
        Y= e la lista del 3D), riusabile da Recursion, Dyna, Conic, Spreadsheet;
      - SET UP per modalità (`ownsSetup`) descritto da una tabella di voci;
      - aggancio a OPTN, VARS e CATALOG di Run-Matrix tramite i resolver
        esistenti, mai accedendo ai dati di un'altra modalità.
    Prova di non regressione: porta almeno Table (e, se il budget lo consente,
    Equation) sul guscio; gli screenshot del simulatore devono restare
    identici pixel per pixel (`tests/sim/*.txt`).

F5. Grafico 2D condiviso con Dual Screen
    Graph, Table (T+G), Dyna Graph, Recursion, Conic Graphs, Statistics,
    Picture Plot e Geometry disegnano su un piano cartesiano. Generalizza
    GraphCore (`graph::Viewport`, `GraphEngine`, `sampleStep`, G-Solve, TRACE,
    layer statici in PSRAM) in modo che:
      - l'area possa essere divisa (sinistra/destra come la fx-CG50: G+G,
        GtoT, T+G) con due viewport indipendenti e una schermata attiva;
      - una sorgente di punti generica (funzione, successione, conica in forma
        parametrica, lista, oggetti geometrici) passi dallo stesso disegno, dal
        TRACE e dallo ZOOM;
      - la V-Window resti quella di `View` (una sola, come sulla calcolatrice).
    Test: immagini identiche a un ridisegno completo, schermo diviso con
    entrambi i profili, budget dei punti invariato.

F6. Espressioni con variabili legate
    Recursion (n, an, an+1, bn…), Dyna Graph (coefficiente A–Z), Spreadsheet
    (riferimenti di cella A1…Z999 e intervalli), Financial (variabili TVM) e
    Program (variabili locali) valutano espressioni con nomi propri. Aggiungi
    al motore un `BindingResolver` componibile con `NameResolver`,
    `CollectionResolver` e `FunctionResolver` esistenti, con espressioni
    compilate una volta e valutate molte volte (`CompiledExpression`), forme
    esatte dove il motore le supporta. Nessun secondo parser.
    Test: legatura, priorità sui nomi globali, riferimenti di cella e intervalli,
    dipendenze circolari rifiutate con un errore fx-CG50.

F7. Lavoro a passi interrompibile
    Ricalcolo del foglio, animazione di Dyna Graph, tabelle lunghe di
    Recursion, esecuzione di Program e Python e trasferimenti di Link sono
    lunghi. Generalizza il calcolo progressivo del 3D Graph in un servizio
    `StepTask` (passo limitato nel tempo dentro `tick()`, AC che interrompe,
    barra di avanzamento, «Break» nello stile della calcolatrice, nessuna
    allocazione nel percorso di interruzione). Test con un orologio simulato.

F8. Editor di testo multiriga
    Program, Python ed eActivity (strisce di testo) hanno bisogno di un editor
    di testo con righe lunghe, scorrimento verticale, ricerca, salto alla riga,
    copia/incolla con la clipboard di `Context`, e un limite di dimensione per
    scheda. Riusa `LineEditor` dove possibile. Test di editing, limiti e OOM.

F9. Interprete di base (solo il nucleo, senza modalità)
    Decisione da confermare con l'utente all'inizio della sessione e da
    registrare in ANALISI_FONTI: il registro dice oggi «linguaggio proprio, non
    il codice Casio». Qualunque sia la scelta, realizza un nucleo iterativo
    (niente ricorsione C++ illimitata: stack di 32 KB) con: istruzioni,
    If/For/While/Do, Goto/Lbl o equivalente, chiamate di sottoprogrammi con
    profondità limitata, I/O astratto (testo, input, attesa tasto, comandi
    grafici tramite `graph_commands`), accesso al workspace solo tramite i
    resolver, budget di passi e memoria, interruzione con `StepTask`.
    Program ed eActivity lo useranno; Python NO (avrà MicroPython).
    Test: controllo di flusso, limiti, errori con riga e colonna, fuzzing.

F10. Python: studio di fattibilità (nessun codice di runtime in questa sessione)
    Misura in ANALISI_FONTI: dimensione di MicroPython per RP2350 senza
    moduli superflui, heap nel pool PSRAM della Plus, impatto sul Pico 2 W
    (probabile esclusione via F1), licenza (MIT), integrazione come libreria in
    CMake. Definisci l'interfaccia I/O (`casioplot`-simile) che userà la
    modalità.

PASSO 3 — Registro, architettura e test
- Ogni nuova fondamenta è un servizio in `app_registry` (`service::…`) con i
  suoi header; ogni modalità Planned dichiara i servizi che userà.
- `tests/architecture_test.cpp`: le nuove regole (servizi, sezioni TLV, file
  tipizzati, modalità per scheda, nessuna modalità che legga i dati di
  un'altra fuori dai resolver). Prova che il test fallisce introducendo apposta
  una violazione, poi toglila (come fatto nella v1.7.0).
- Suite nuove in tests/ (nomi `<fondamenta>_v115_test.cpp`), registrate in
  tests/CMakeLists.txt; `oom_test` e `limits_smoke` estesi.
- Tutto deve passare: `tests/run_host_tests.sh`, `--sanitize --tsan`, entrambi i
  profili, fuzzing (`tools/fuzz/run_fuzz.sh`, almeno 300 s per harness nuovo o
  modificato), simulatore con screenshot identici, firmware di entrambe le
  schede senza warning e dentro i limiti di F1, relay 15/15.

PASSO 4 — Documentazione
- docs/APPLICATIONS_ARCHITECTURE.md: nuovi servizi, sezioni, file, §6 di ogni
  modalità aggiornato con le fondamenta che usa, §8 «Come aggiungere una
  modalità» riscritto sul guscio comune.
- docs/STORAGE.md (record v11, sezioni TLV), docs/FILE_FORMATS.md (nuovo),
  docs/PICO_PLUS_2W.md (budget), docs/FEATURE_MATRIX.md, docs/ROADMAP.md,
  docs/BUILD_STATUS.md, docs/CHANGELOG.md, docs/TESTING.md, CONTINUE.md,
  docs/README.md.
- docs/reports/SVILUPPO_v1.15.0.md: cosa è stato fatto, numeri prima/dopo
  (controlli, dimensioni, RAM, stack), decisioni, cosa resta.
- docs/HARDWARE_TEST_v1.15.0.md: prove fisiche nuove (HW-190…), almeno: record
  v11 dopo un aggiornamento da v1.14.0, file su microSD e LittleFS, tempi del
  guscio, schermo diviso, StepTask con AC, budget per scheda.
- docs/releases/v1.15.0.md e la versione 1.15.0 in CMakeLists.txt.
- Mai scrivere «parità con la fx-CG50» o «TESTED ON HARDWARE» per ciò che non
  lo è.

PASSO 5 — Git
- Parti dal `main` aggiornato (`git fetch origin main`), lavora sul branch di
  sessione con commit intermedi. Se `main` avanza durante il lavoro, fai il
  merge di `main` e ripeti tutti i test.
- Alla fine un solo commit su `main` in fast-forward con messaggio
  «PicoCalc FxCG-50 v1.15.0: fondamenta comuni delle applicazioni (…)».
  Mai force-push, mai spostare tag; tag e release dal workflow
  (`workflow_dispatch` con «publish»).

CRITERI DI ACCETTAZIONE
[ ] Matrice modalità × fondamenta in ANALISI_FONTI_v1.15.0.md, prima del codice.
[ ] Budget per scheda deciso con i numeri e verificato dal build.
[ ] Record v11 con sezioni TLV; lettura v1–v10; sezioni sconosciute conservate.
[ ] File tipizzati con scrittura atomica, FileDialog unico, vista in MEMORY.
[ ] ModeShell usato da almeno una modalità esistente, screenshot identici.
[ ] Grafico 2D con schermo diviso e sorgenti di punti generiche.
[ ] BindingResolver, StepTask, editor multiriga, nucleo dell'interprete testati.
[ ] Studio di fattibilità di MicroPython con i numeri.
[ ] architecture_test con le nuove regole; tutti i test, sanitizer, fuzzing
    e build verdi su entrambi i profili.
[ ] Nessuna modalità nuova marcata Available; nessuna schermata finta.
[ ] Documentazione e rapporto finale aggiornati; un solo commit su main.
````

---

## 2. Prompt per le modalità (dopo la v1.15.0)

Ogni prompt va preceduto da: «Leggi CONTINUE.md, docs/reports/SVILUPPO_v1.15.0.md
e docs/APPLICATIONS_ARCHITECTURE.md. Segui lo stesso metodo delle versioni
precedenti: ANALISI_FONTI con la tabella di parità del capitolo prima del
codice, un test per ogni voce, simulatore con screenshot, SVILUPPO con la
tabella di parità finale e le differenze inevitabili, HARDWARE_TEST, un solo
commit su main». L'ordine proposto va dal più piccolo e più riusato al più
grande.

| Ordine | Versione | Modalità | Prompt |
|---|---|---|---|
| 1 | 1.16.0 | Statistics | «Completa STATISTICS alla parità con il capitolo 6: regressioni Sin e Logistic, StatGraph 1–3 con tutti i tipi di grafico (N-Dist, Broken, Med-Box, Pie, Bar), CALC dal grafico, residui, tutti i TEST e gli INTR (2-Sample, Prop, χ², F, LinearReg, ANOVA), DIST con le distribuzioni di `calc_library`, risultati verso le variabili statistiche di VARS. Grafici sul servizio 2D di F5.» |
| 2 | 1.17.0 | Graph + Table | «Completa Graph e Table: Dual Screen G+G (COPY, SWAP), GtoT, T+G, Graph to Table, FORMAT durante lo SKETCH, sullo schermo diviso di F5; aggiorna GRAPH_PARITY.md e ricalcola la sintesi.» |
| 3 | 1.18.0 | Recursion | «Realizza RECURSION (5-45…): tipi an, an+1, an+2 con bn e cn, SET, TABLE con Σ, grafici e web/fase, VARS › RECURSION già presenti in Run-Matrix; nucleo su BindingResolver, tabelle su `table_model`, grafici su F5.» |
| 4 | 1.19.0 | Dyna Graph | «Realizza DYNA GRAPH (5-42…): funzioni predefinite e personalizzate, Dynamic Var, SET Start/End/Pitch, Stop/Cont, Locus, velocità, B-IN, animazione su StepTask, DrawDyna coerente con il comando di Run-Matrix.» |
| 5 | 1.20.0 | Conic Graphs | «Realizza CONIC GRAPHS (5-50…): tutti i modelli di equazione cartesiani, polari e parametrici del manuale, grafico su F5, G-Solve specifici (fuochi, vertici, direttrice, asse, asintoti, eccentricità, latus rectum, centro, raggio, intercette), TRACE e ZOOM.» |
| 6 | 1.21.0 | Financial | «Realizza FINANCIAL (capitolo 7): Simple, Compound, Cash Flow (NPV, IRR, PBP, NFV), Amortization, Conversion, Cost/Sell/Margin, Days, Depreciation, Bond, con i SET del manuale, riusando le 22 funzioni TVM di `calc_library`; date inserite a mano (nessun RTC).» |
| 7 | 1.22.0 | Spreadsheet | «Realizza SPREADSHEET (capitolo 9): griglia A–Z × 999 virtualizzata, formule con riferimenti e intervalli su BindingResolver, Auto Calc e ricalcolo su StepTask, CELL/EDIT/DELETE/INSERT/CLEAR, funzioni di cella del manuale, STAT e grafici dalle celle su F5, copia da/verso liste, file `.sheet` (F3).» |
| 8 | 1.23.0 | eActivity | «Realizza eACTIVITY (capitolo 10): strisce di testo, calcolo (nucleo di Run-Matrix), e strisce delle modalità già disponibili; file (F3), editor multiriga (F8).» |
| 9 | 1.24.0 | Program | «Realizza PROGRAM (capitolo 8) sul nucleo dell'interprete di F9: elenco file, editor, esecuzione con StepTask, I/O, comandi grafici tramite `graph_commands`, errori con riga, debug passo passo se previsto dall'analisi.» |
| 10 | 1.25.0 | Picture Plot | «Realizza PICTURE PLOT (capitolo 15): immagini `.pcpict`/BMP di sfondo, piazzamento dei punti, liste, regressioni di Statistics, file del progetto.» |
| 11 | 1.26.0 | Geometry | «Realizza GEOMETRY (capitolo 14): costruzioni, vincoli, misure, trasformazioni, animazioni, file, sul servizio 2D.» |
| 12 | 1.27.0 | Memory + System + Link | «Completa MEMORY (vista dei file per tipo, backup/ripristino del workspace come file, ricerca) e SYSTEM (spegnimento automatico e gestione energetica, voci mancanti del capitolo 12); realizza LINK come trasferimento del record e dei file via USB seriale con formato proprio e CRC, con uno strumento lato PC in `tools/`.» |
| 13 | 1.28.0 | Python | «Integra MicroPython secondo lo studio di F10: heap nel pool PSRAM (solo Plus se F1 lo esclude dal Pico 2 W), shell, editor (F8), file `.py` (F3), modulo grafico, Break con AC.» |
| — | — | E-CON4 | resta External: al più «Importa campioni CSV nelle liste» dentro Statistics. |

Dopo ogni versione: aggiornare CONTINUE.md, il recap (nuovo
`docs/reports/RECAP_v<versione>.md` quando cambia la sintesi della parità) e
la tabella del §2 di questo file.
