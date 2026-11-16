# Analisi delle fonti — v1.11.0

Rapporto preparato **prima** dell'implementazione della v1.11.0 (base: tag
`v1.10.0`, commit `dfdb08b`, verificato uguale a `origin/main`). Per ogni
fonte: cosa documenta, quali funzioni riguarda, i requisiti estratti e come
viene usata. Le fonti Casio sono usate **solo come riferimento di
comportamento**: nessun codice, ROM, font o grafica Casio entra nel progetto.

## 0. Punto di partenza

- `git fetch origin main --tags`: il tag annotato `v1.10.0` esiste e punta a
  `dfdb08b2eb29…`; `main`, `origin/main` e il branch di sessione coincidono,
  nessuna modifica remota successiva da preservare.
- Test host del profilo Pimoroni Pico Plus 2 W eseguiti prima di ogni
  modifica: 37/37 superati (base di confronto).
- Prima di toccare il codice è stato generato con i sorgenti della v1.10.0 un
  record del workspace versione 6 (`tests/fixtures/workspace_v1_10_0.inc`):
  impostazioni Deg/Line/ab/c/a+bi, cronologia con frazioni, matrici reali e
  complesse, vettori, liste, un risultato 7×7 (elementi omessi), funzioni di
  GRAPH, intervallo di TABLE, chat e 3D. Serve alla prova di migrazione v6→v7.

## 1. Fonti e uso

| # | Fonte | Cosa documenta | Funzioni | Uso |
|---|---|---|---|---|
| 1 | Manuale software fx-CG50 (Casio, OS 3.x; capitoli 1 *Basic Operation*, 2 *Manual Calculations*, 3 *List Function*, 5 *Graphing*, 8 *Programming* — elenco dei comandi) | comportamento della calcolatrice | tutte | riferimento di comportamento; dove la memoria del manuale non è certa, il comportamento scelto è indicato come tale qui e in `docs/RUN_MATRIX.md` |
| 2 | Manuale fx-CG10/CG20 (stesso sistema operativo), pagine 3-18…3-20 «Using CSV Files» (consultate su manualslib) | CSV dell'editor delle liste: LOAD, SAVE·AS, separatore, simbolo decimale | 4.2 | regole del formato (sotto) |
| 3 | Manuale fx-CG50, pagina con `K2(MEMORY)2(CSV)2(SET)` (manualslib) | dove si imposta il formato CSV | 4.2 | il formato è un'impostazione globale |
| 4 | Manuale fx-CG100, capitolo *Base-N* (support.casio.com) e manuali fx-82/991 | Neg, Not, and, or, xor, xnor; complemento a due a 32 bit | 4.6 | conferma dell'insieme di operatori e della larghezza a 32 bit |
| 5 | Sorgenti e documenti del progetto (`README.md`, `CONTINUE.md`, `docs/RUN_MATRIX.md`, `FEATURE_MATRIX.md`, `ARCHITECTURE.md`, `STORAGE.md`, `MATH_ENGINE.md`, `reports/SVILUPPO_v1.10.0.md`) | architettura reale, formato del record v6, limiti per scheda, regole dei test di architettura | tutte | vincoli di progetto |
| 6 | Pico SDK 2.2.0 (`/opt/sdk/pico-sdk`), `cmake/fxai_board.cmake`, `board_config.hpp`, script del linker | flash, RAM, PSRAM e partizioni delle due schede | §5 | verifica dei limiti per scheda (nessuna modifica all'hardware) |

Il PDF del manuale fx-CG50 non è scaricabile da questo ambiente (il sito Casio
restituisce una pagina HTML); i requisiti del manuale sono quindi presi dalle
pagine consultate (fonti 2–4) e dalla conoscenza del manuale già usata nelle
versioni precedenti. I punti dubbi sono segnalati con **(scelta)**.

## 2. Requisiti estratti

### 4.1 Liste (manuale cap. 3)
- 26 liste, `List 1`…`List 26`, ciascuna fino a 999 elementi sulla
  calcolatrice; nei calcoli `List n` e `List n[k]` per ogni n.
- Ogni lista ha un **sottonome** (riga *SUB* sotto l'intestazione
  dell'editor) di al più 8 caratteri, modificabile portando il cursore sulla
  riga del sottonome; il sottonome **non** viene scritto nei file CSV (fonte 2).
- Statistics usa le liste scelte nel suo SET (XList, YList; predefinite
  List 1 e List 2); GRAPH e TABLE accettano qualunque lista (grafici da lista,
  variabile di tabella da lista, LISTMEM).
- Progetto: limite di elementi **di tutte le liste insieme** per scheda
  (`limits::kListStoreElements`): sul Pico 2 W resta il massimo della v1.10.0
  (6 × 255 = 1 530 elementi, ≈ 12 KB nel record da 59,75 KB), sulla Pico
  Plus 2 W 20 000 elementi (≈ 156 KB nel record da 512 KB, PSRAM).

### 4.2 CSV (fonti 2 e 3, manuale 2-48 per le matrici)
- Editor delle liste: CSV › **LOAD** (dalla lista del cursore) e
  **SAVE·AS** (nome di 8 caratteri al massimo); editor MAT/VCT: CSV › LOAD e
  SAVE·AS della memoria selezionata.
- Separatore `,` o `;`, simbolo decimale `.` o `,` (impostazione globale,
  CSV › SET); **(scelta)** la combinazione separatore `,` + decimale `,` è
  rifiutata perché ambigua.
- Fine riga CR, LF o CRLF; il tabulatore non è un separatore ammesso.
- Se la riga 1 contiene virgolette (`"` o `'`) è un'intestazione e viene
  ignorata.
- Errori: file mancante, formato errato (testo non numerico, righe di
  lunghezza diversa per una matrice), dimensioni oltre i limiti della scheda.
  **(scelta)** gli elementi complessi non si salvano in CSV
  (`Complex Number In Data`), come la calcolatrice che scrive solo numeri reali.
- Progetto: stesso salvataggio atomico dei progetti GRAPH (`.tmp`, verifica
  CRC, `.bak`, rinomina) sulla microSD **e** sulla flash interna (LittleFS),
  cartella `PICOCALC/CSV`, archivio scelto in SYSTEM.

### 4.3 MatAns / ListAns (manuale cap. 2, matrici; 1-14…1-22)
- Un risultato oltre la vista naturale (più di 6 × 6, o più di 20 elementi
  per una lista) si legge in una schermata dedicata della risposta, con
  scorrimento in entrambe le direzioni e indicazione della cella; EXIT
  torna ai calcoli.

### 4.4 Editor MAT/VCT (manuale 2-42…2-48)
- Una cella accetta un'espressione (frazioni, radici, π) e, nei modi
  complessi, numeri complessi; la cella mostra la forma esatta quando esiste.
- Le colonne oltre quelle visibili scorrono; l'editor indica riga e colonna
  della cella corrente.

### 4.5 Modalità Line (manuale cap. 1)
- **INS** (SHIFT DEL sulla fx-CG50): in Line alterna inserimento e
  sovrascrittura; il cursore di sovrascrittura è diverso.
- In Math lo stesso tasto (SHIFT+INS) rende **argomento di un modello** il
  valore o l'espressione a destra del cursore (es. `1+(2+3)+4` → `1+√(2+3)+4`).
- Frazioni in Line: il tasto frazione inserisce `⌟` (`2⌟3`, frazione mista
  `1⌟2⌟3`); i risultati frazionari in Line si scrivono con `⌟`.
- I risultati più lunghi della riga si scorrono orizzontalmente.

### 4.6 Calcolo (manuale cap. 2)
- Simboli ingegneristici (OPTN › ESYM): m (10⁻³), µ (10⁻⁶), n (10⁻⁹), p
  (10⁻¹²), f (10⁻¹⁵), k (10³), M (10⁶), G (10⁹), T (10¹²); la modalità di
  visualizzazione **Eng** mostra i risultati con il simbolo (`999k+25k` =
  `1.024M`, `9÷10` = `900m`). Fuori da f…T resta la forma ×10ⁿ.
- **Simp** (frazioni, impostazione *Simplify* Auto/Manual): `▶Simp` divide
  numeratore e denominatore per il più piccolo fattore primo comune,
  `▶Simp n` per il fattore n; il risultato mostra il fattore usato (`F=3`).
  **(scelta)** In Manual una frazione digitata resta come è stata scritta
  (`15/60`); i risultati di altre operazioni sono ridotti; un fattore che non
  divide entrambi i termini dà Math ERROR; una frazione già irriducibile dà
  `F=1`.
- **Base-N** come modalità di calcolo intera (SET UP › Mode: Comp, Dec, Hex,
  Bin, Oct): interi a 32 bit in complemento a due (Dec −2147483648…2147483647,
  Hex 0…FFFFFFFF, Bin 32 cifre, Oct 0…37777777777); divisione intera;
  operatori bit a bit **And, Or, Xor, Xnor**, **Not** (complemento a uno),
  **Neg** (complemento a due); prefissi d, h, b, o per la base di un singolo
  numero; risultati fuori intervallo → Math ERROR; numeri decimali con la
  virgola e funzioni scientifiche non ammessi.

### 4.7 Comandi grafici da Run-Matrix (manuale cap. 5 *Sketch*, cap. 8)
- I comandi SKETCH eseguiti in Run-Matrix disegnano nella schermata grafica,
  che compare al termine; EXIT torna a Run-Matrix.
- Sottoinsieme realizzato (coerente con la SKETCH di GRAPH già esistente):
  `Cls`, `ViewWindow`, `Graph Y=`, `Graph r=`, `Graph X=`,
  `Graph(X,Y)=(xt,yt)`, `Plot`, `PlotOn`, `PlotOff`, `PlotChg`, `Line`,
  `F-Line`, `Circle`, `Vertical`, `Horizontal`, `Text`, `PxlOn`, `PxlOff`,
  `PxlChg`, `Tangent`, `Normal`, `Inverse`.
- Esclusi e documentati: `PxlTest(`, `DrawStat`, `DrawDyna`, `DrawR`,
  `DrawFTG`, `G-SelOn/Off`, `StoPict`/`RclPict`, `BG-Pict`, `SketchThick` e
  gli altri stili, `Graph Y>`/disequazioni, `G∫dx` (richiedono i programmi o
  modalità non realizzate).

## 3. Vincoli di progetto e scelte

| Vincolo | Come viene rispettato |
|---|---|
| Un solo parser, un solo AST, un solo valutatore | simboli ingegneristici, `⌟`, letterali Base-N e prefissi sono nuovi token dello **stesso lexer**; gli operatori bit a bit sono le stesse produzioni di And/Or/Xor con la semantica intera scelta da `EvalContext` (modalità Base-N); `Not`/`Neg` funzioni dello stesso valutatore. I comandi grafici non sono espressioni: un interprete di comandi (modulo del motore, senza UI) separa gli argomenti e valuta **ciascuno con il parser e il valutatore esistenti**. |
| Dati solo nel workspace, tramite interfacce | List 1–26 e sottonomi nel workspace (`storeList`, `setListName`); i comandi grafici scrivono V-Window, SKETCH e grafici dei comandi tramite il modulo `graph_commands`, che GRAPH disegna con il suo motore e il suo renderer. Run-Matrix continua a **non** disegnare e a non toccare `ws.view`/`ws.lists` (test di architettura rafforzato, non indebolito). |
| Memoria embedded | nessun oggetto grande sullo stack; CSV letti con limite di dimensione per scheda; forme esatte delle celle solo per memorie fino a 64 elementi; limiti per scheda in `limits.hpp`. |
| Retrocompatibilità | record **v7** in coda al v6; v1–v6 letti senza perdite; fixture v1.10.0 nei test. |

## 4. Nuovi file previsti

`include/graph_commands.hpp` + `src/graph_commands.cpp` (interprete dei
comandi grafici), `include/csv_io.hpp` + `src/csv_io.cpp` (CSV),
`src/apps/answer_view.cpp` (schermate MatAns/ListAns), nuove suite
`tests/lists_csv_test.cpp`, `tests/calc_v111_test.cpp`,
`tests/run_matrix_v111_test.cpp`, script del simulatore
`tests/sim/run_matrix_v111.txt`.
