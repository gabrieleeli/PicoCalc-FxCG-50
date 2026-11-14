# Analisi delle fonti — v1.10.0

Rapporto scritto **prima** dell'implementazione della v1.10.0 (base: commit
`bb2bdee`, indicato come `v1.9.0`). Per ogni fonte: che cosa documenta, quali
funzioni del progetto riguarda, i requisiti estratti e come viene usata.

## 1. Accesso alle fonti

L'ambiente di sviluppo raggiunge solo gli host consentiti dalla sua rete. Gli
host della documentazione sono stati aggiunti a un ambiente dedicato al
repository; alcuni siti rispondono comunque con una pagina di verifica
anti-bot (HTTP 403) e **non sono stati letti**.

| Fonte | Esito |
|---|---|
| Manuale software fx-CG50 in PDF (`www.casio.com`, 5,99 MB) | **letto** (testo estratto con `pdftotext`; pagine citate sotto) |
| Manuale HTML, guida rapida, guida Run-Matrix, foglio matrici/vettori, guida introduttiva (`education.casio.co.uk`) | **non leggibili**: HTTP 403 (pagina di verifica "Just a moment…"). Gli stessi argomenti sono nel PDF |
| Documentazione Pico (`www.raspberrypi.com`) | **non leggibile**: HTTP 403 |
| Datasheet Pico 2 W (`datasheets.raspberrypi.com`, reindirizzato a `pip.raspberrypi.com`) | **letto** (24 pagine) |
| Datasheet RP2350, "Hardware design with RP2350" (`datasheets.raspberrypi.com`) | **non ancora letti** |
| Pico SDK 2.2.0 (`github.com/raspberrypi/pico-sdk`) | **letto**: header delle schede `pico2_w.h` e `pimoroni_pico_plus2_w_rp2350.h`, usati per compilare |
| pico-examples (`github.com/raspberrypi/pico-examples`) | raggiungibile; già usato dalla v1.8.0 per le configurazioni lwIP/mbedTLS, nessuna novità necessaria |
| Pagina prodotto Pimoroni Pico Plus 2 W (`shop.pimoroni.com`) | **letta** |
| Crafting Interpreters (`craftinginterpreters.com`) | **letto**: capitoli *Scanning* e *Parsing Expressions* |
| cppreference (`en.cppreference.com`) | **non leggibile**: HTTP 403. Il codice usa solo C++20 già presente nel progetto |
| Documentazione CMake (`cmake.org`) | raggiungibile; nessuna nuova funzione di CMake è servita |
| Scheda «Fonti (Sources)» di GitHub | **non esiste** nei repository GitHub. Sono state analizzate le fonti elencate nei documenti del progetto (sezioni «Fonti» di `docs/HARDWARE.md`, `docs/PICO_PLUS_2W.md`, `docs/reports/CONSOLIDAMENTO_v1.9.0.md`) |
| Documentazione del progetto | **letta**: `README.md`, `CMakeLists.txt`, `cmake/`, `VERSION`, `docs/CHANGELOG.md`, `docs/ARCHITECTURE.md`, `docs/RUN_MATRIX.md`, `docs/MATH_ENGINE.md`, `docs/HARDWARE.md`, `docs/APPLICATIONS_ARCHITECTURE.md`, sorgenti di parser, AST, valutatori, editor, renderer, workspace e porta board |

## 2. Manuale fx-CG50: requisiti estratti

| Pagine | Argomento | Requisito usato nella v1.10.0 |
|---|---|---|
| 1-8, 1-9 | Colori delle parentesi | Sequenza **blu, rosso, verde, magenta, nero** dall'esterno verso l'interno, ripetuta oltre i cinque livelli; la parentesi di chiusura prende il colore di quella di apertura; parentesi dello stesso livello hanno lo stesso colore; **dopo l'esecuzione tutte le parentesi tornano nere** (quindi i colori valgono solo durante l'inserimento e la modifica) |
| 1-10 | Correzione dopo un errore | Dopo il messaggio, EXIT riporta all'espressione con il **cursore posizionato automaticamente nel punto che ha causato l'errore** (esempio `14÷0×2.3`) |
| 1-11, 1-12 | Appunti (CLIP) | `SHIFT 8` (CLIP) fissa un estremo, le frecce evidenziano l'intervallo, `F1 COPY` copia senza modificare, `F2 CUT` taglia, `EXIT` annulla; `SHIFT 9` (PASTE) incolla al cursore. In modo Math l'intervallo è limitato ai movimenti del cursore; di una parentesi si seleziona una parte del contenuto oppure l'intera espressione tra parentesi |
| 1-14 … 1-24 | Modo Math | Input naturale con modelli; i risultati (frazioni, matrici fino a 6×6, vettori fino a 1×6/6×1, liste fino a 20 elementi) sono mostrati in formato naturale; oltre questi limiti si usa la schermata MatAns/VctAns/ListAns |
| 1-25, 1-26 | Matrici in modo Math | Menu MATH › MAT/VCT con i modelli 2×2, 3×3, m×n (fino a 6×6), 2×1, 3×1, 1×2, 1×3; DEL sulla prima cella elimina il modello |
| 1-34, 1-35 | Setup | «Input/Output»: **Math** (predefinito) o **Line**; indicato nella barra di stato |
| 2-42 … 2-48 | Editor delle matrici | `▶MAT/VCT`: elenco Mat A…Z con dimensioni o «None»; DELETE, DEL-ALL (con conferma), DIM (m, n), M⇔V; una matrice nuova contiene zeri; cambiare le dimensioni cancella il contenuto; una lettera salta alla matrice; EXE apre le celle (EXE passa alla cella a destra); ROW-OP (Swap, ×Row, ×Row+, Row+), ROW e COLUMN (DELETE, INSERT, ADD), EDIT |
| 2-49 … 2-53 | Comandi | `[[a,b][c,d]]→Mat A`, `Mat A[m,n]` (lettura e scrittura), `Identity n`, `Dim Mat A` (risultato lista), `{m,n}→Dim Mat A`, `Fill(x,Mat A)`, `Augment(Mat A,Mat B)` (stesso numero di righe), `Mat→List(Mat A,c)` |
| 2-53 … 2-58 | Calcoli | +, − (stesse dimensioni), × (colonne = righe), scalare × matrice, `Det`, `Trn`, `Ref`, `Rref`, inversa `x⁻¹` (solo quadrate, determinante ≠ 0), quadrato, potenza intera (fino a 32766), Abs/Int/Frac/Intg sugli elementi, elementi complessi; il risultato va in **MatAns** (`MatAns→Mat A`) |
| 2-59 … 2-63 | Vettori | Vct A…Z e **VctAns**; un vettore è m×1 oppure 1×n; `DotP(`, `CrossP(` (solo 2 o 3 elementi), `Angle(`, `UnitV(`, `Norm(`; l'editor dei vettori è quello delle matrici senza ROW-OP; i risultati m×1/1×n vanno anche in VctAns; `Vct A→Mat B` è ammesso |
| 3-13 … 3-15 | Liste | `{1,2,3}`, `List n`, `List n[k]`, `→List n`, operazioni elemento per elemento tra liste della stessa lunghezza o tra lista e numero; risultato in **ListAns**; `Dim`, `Sum`, `Prod`, `Min`, `Max`, `Mean`, `Median`, `Cuml`, `Augment`, `Seq`, `ΔList`, `List→Mat` |
| α-1 … α-3 | Messaggi d'errore | «Syntax ERROR», «Dimension ERROR», «Invalid Type», «Memory ERROR», «Non-Real ERROR» |

Scelte dichiarate dove il progetto si discosta dalla calcolatrice:

- Sulla fx-CG50 `▶MAT/VCT` è F3; nella PicoCalc F1–F6 inseriscono già i
  modelli (frazione, radice, potenza…), quindi l'editor si apre da
  **OPTN › MAT/VCT › ▶Editor** e con **Ctrl+M**, senza cambiare i tasti esistenti.
- CLIP: `SHIFT` poi `8` come sulla calcolatrice, più Ctrl+X/C/V della tastiera.
- Le liste sono le 6 del progetto (la fx-CG50 ne ha 26); matrici e vettori
  hanno un limite di elementi che dipende dalla scheda (§4).
- Titoli d'errore nello stile già usato dal progetto («Math ERROR»), con i
  nuovi «Dimension ERROR» e «Type ERROR».

## 3. Parser e AST

*Crafting Interpreters* (capitoli 4 e 6): lo scanner conserva per ogni token
la posizione nel sorgente (`start`/`current`) e l'errore di sintassi viene
riferito al token; il parser a discesa ricorsiva resta uno solo e ogni
livello di priorità è una funzione. Applicazione al progetto, **senza
sostituire il parser esistente**:

- ogni token del lexer riceve l'intervallo di byte nel testo lineare;
- ogni nodo dell'AST prodotto dal parser riceve l'intervallo del sorgente;
- `SyntaxError`/`MathError` portano l'intervallo; gli errori di valutazione
  lo prendono dal nodo più interno che li ha sollevati;
- `ast::serialize` produce anche la mappa «riga dell'editor, indice → byte»,
  con cui l'editor naturale converte l'intervallo nella posizione del cursore;
- matrici, vettori e liste entrano nella stessa grammatica (letterali
  `[[…]]` e `{…}`, riferimenti `Mat A`, `Vct A`, `List n`, indici `[…]`) e
  sono valutati dallo stesso motore con un tipo di valore unificato.

## 4. Schede

| | Raspberry Pi Pico 2 W | Pimoroni Pico Plus 2 W | Fonte |
|---|---|---|---|
| `PICO_BOARD` | `pico2_w` | `pimoroni_pico_plus2_w_rp2350` | Pico SDK 2.2.0, `src/boards/include/boards/` |
| MCU | RP2350A (`PICO_RP2350A 1`) | RP2350B (`PICO_RP2350A 0`), 48 GPIO | header delle schede; pagina Pimoroni |
| Flash | 4 MB (`PICO_FLASH_SIZE_BYTES`) | 16 MB | header delle schede; pagina Pimoroni |
| PSRAM | assente | 8 MB, CS su GPIO 47 (`PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN`) | header; pagina Pimoroni |
| SRAM | 520 KB | 520 KB | pagina Pimoroni (RP2350) |
| Wi-Fi/Bluetooth | CYW43439 | modulo Raspberry Pi RM2 (stesso driver CYW43) | header (`CYW43_DEFAULT_PIN_WL_*`: REG_ON 23, DATA 24, CS 25, CLOCK 29 su entrambe); datasheet Pico 2 W (GPIO23/24/25/29 riservati al modulo radio, GPIO29 condiviso con ADC3 per VSYS/3) |
| Alimentazione | VSYS 1,8–5,5 V | 3–5,5 V | datasheet Pico 2 W; pagina Pimoroni |
| Altro | — | USB-C, connettore Qw/ST (I2C), tasto BOOT su GP45, regolatore 600 mA | pagina Pimoroni |

Conseguenze per la v1.10.0:

- le due schede **non** hanno la stessa memoria: restano due profili
  (`FXAI_BOARD`), una sola base di codice, cartelle di build e UF2 separati;
- i pin usati dalla PicoCalc (LCD SPI1, tastiera I2C1, microSD SPI0) sono gli
  stessi con i due moduli: nessuna modifica alla porta board;
- i nuovi dati persistenti (matrici e vettori) hanno un limite per scheda,
  perché il record del workspace è di 64 KB per slot sul Pico 2 W e di 512 KB
  sulla Plus; sulla Plus le allocazioni grandi finiscono nella PSRAM tramite
  l'`operator new` instradato, senza codice specifico;
- il connettore Qw/ST e il Bluetooth non sono usati dal firmware (invariato).

## 5. Fonti più importanti e uso previsto

1. **Manuale fx-CG50 (PDF)** — comportamento di editor MAT/VCT, comandi,
   CLIP, colori delle parentesi, modi Math/Line, cursore sull'errore.
2. **Sorgenti e documenti del progetto** — architettura da preservare
   (un solo parser, un solo valutatore, workspace come unico canale tra le
   modalità, regole di `tests/architecture_test.cpp`).
3. **Pico SDK 2.2.0** — definizioni delle schede e compilazione dei due UF2.
4. **Crafting Interpreters** — posizioni nei token e negli errori.
5. **Pagina Pimoroni** — differenze hardware documentate in `docs/HARDWARE.md`.
