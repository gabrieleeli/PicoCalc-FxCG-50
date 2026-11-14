# Analisi delle fonti — v1.10.0

Rapporto preparato **prima** dell'implementazione della v1.10.0 (base: commit
`bb2bdee`, la v1.9.0 di `main`). Per ogni fonte: cosa documenta, quali funzioni
riguarda, i requisiti estratti, se è aggiornata e come è stata usata.

## 0. Scheda "Fonti" (Sources) di GitHub

**Non accessibile, perché non esiste.** La pagina del repository mostra le
schede *Code, Issues, Pull requests, Actions, Projects, Security and quality,
Insights*: nessuna scheda "Sources"/"Fonti", nessun Wiki, nessuna Discussions.
La sezione *About* è vuota (nessuna descrizione, sito o topic), non ci sono
release pubblicate (solo una bozza "V1.9.1" senza tag e senza testo) né
workflow di GitHub Actions. Sono state quindi analizzate **tutte le risorse
presenti nel repository**: documentazione (`docs/`, 24 documenti e 4 rapporti),
`CONTINUE.md`, i sorgenti, i test e l'immagine
`Screenshot_20261106_221534_Chrome.jpg` (pagina 1-3 del manuale italiano della
fx-CG50: elenco delle modalità, già riflesso nel registro delle applicazioni).

## 1. Fonti più importanti e uso previsto

| # | Fonte | Cosa documenta | Funzioni interessate | Aggiornata | Uso |
|---|---|---|---|---|---|
| 1 | Manuale software fx-CG50 v3.00 (PDF Casio, letto per intero nelle parti citate) | comportamento della calcolatrice | tutte le funzioni di questa versione | sì (versione corrente del sistema operativo 3.x) | riferimento di comportamento, **nessun codice** |
| 2 | Sorgenti e documenti del progetto (`docs/ARCHITECTURE.md`, `RUN_MATRIX.md`, `MATH_ENGINE.md`, `STORAGE.md`, `ROADMAP.md`, `APPLICATIONS_ARCHITECTURE.md`) | architettura reale, formato del record, regole verificate dai test | tutte | sì (v1.9.0) | vincoli di progetto: un solo parser, un solo AST, separazione delle applicazioni |
| 3 | Pico SDK 2.2.0: `boards/pico2_w.h`, `boards/pimoroni_pico_plus2_w_rp2350.h`, `hardware/platform_defs.h` | identificativi delle schede, flash, pin interni, package RP2350A/B | compatibilità hardware | sì (SDK installato: tag 2.2.0) | verifica di `board_config.hpp` e di `cmake/fxai_board.cmake` |
| 4 | Pagina prodotto Pimoroni Pico Plus 2 W (specifiche, schema, pinout) | RP2350B, 16 MB di flash, 8 MB di PSRAM, modulo RM2, Qw/ST, SWD | compatibilità hardware | sì (lotto attuale: stepping A4) | differenze rispetto al Pico 2 W |
| 5 | *Crafting Interpreters* (capitoli Scanning e Parsing Expressions) | token con posizione, discesa ricorsiva per livelli di priorità, errore riferito al token | posizione degli errori, estensione del parser | sì | metodo, applicato al parser esistente (non sostituito) |
| 6 | Repository ClockworkPi PicoCalc (già citato in `HARDWARE.md`) | pin di LCD, tastiera, microSD | compatibilità hardware | invariato rispetto alla v1.9.0 | nessuna modifica ai driver |

## 2. Requisiti estratti dal manuale fx-CG50

### Editor MAT/VCT (pagine 2-42…2-48, 2-59…2-60)
- 26 memorie `Mat A`…`Mat Z` più `MatAns`; 26 memorie `Vct A`…`Vct Z` più `VctAns`.
- Elenco delle memorie con le dimensioni `m × n` oppure `None`; una lettera
  porta alla memoria corrispondente.
- **DIM**: righe, colonne, conferma; una matrice nuova contiene zeri; **cambiare
  le dimensioni cancella il contenuto**; memoria insufficiente → `Memory ERROR`.
- Celle: il valore si scrive nella cella evidenziata e **EXE passa alla cella a
  destra**.
- Menu della matrice aperta: **ROW-OP** (SWAP, ×Rw, ×Rw+, Rw+), **ROW**
  (DELETE, INSERT, ADD), **COLUMN** (DELETE, INSERT, ADD), **EDIT**.
- **DELETE** / **DEL-ALL** con conferma; **M⇔V** passa dall'editor delle matrici
  a quello dei vettori.
- Vettore = matrice `m × 1` oppure `1 × n`; l'editor dei vettori non ha ROW-OP e
  DIM accetta solo quelle due forme. Le memorie dei vettori e delle matrici sono
  compatibili (`Vct A → Mat B`).
- Un risultato matriciale `m × 1` o `1 × n` aggiorna anche `VctAns`; un
  risultato vettoriale aggiorna anche `MatAns`.

### Matrici, vettori e liste nei calcoli (2-49…2-63, 3-7…3-16)
- Letterale di matrice `[[a,b][c,d]]`, di lista `{a,b,c}`; `→ Mat A`,
  `→ Vct A`, `→ List 1`; elemento `Mat A[m,n]`, `List 1[n]` in lettura e in
  scrittura; `{m,n} → Dim Mat A`.
- Matrici: `+ − ×`, scalare × matrice, `Det`, `Trn`, `Identity`, `Dim`,
  `Augment(`, `Fill(`, `Ref`, `Rref`, `Mat→List(`, inversa `x⁻¹`, quadrato,
  potenza intera (fino a 32766), `Abs`/`Frac`/`Int`/`Intg` elemento per elemento.
- Vettori: `+ − ×`, `DotP(`, `CrossP(` (solo dimensioni 2 e 3), `Angle(`,
  `UnitV(`, `Norm(` (anche per le matrici).
- Liste: operazioni elemento per elemento tra due liste della stessa lunghezza
  o tra lista e numero; funzioni scientifiche applicate a ogni elemento; `Dim`,
  `Seq(`, `Min(`, `Max(`, `Mean(`, `Median(`, `Sum`, `Prod`, `Cuml`, `Percent`,
  `ΔList`, `Augment(`, `List→Mat(`; il risultato va in `ListAns`.
- Errori (tabella α-1…α-3): `Dimension ERROR` (dimensioni incompatibili),
  `Invalid Type` e "Input value must be a matrix / a list" (tipo sbagliato),
  `Improper Number of Elements` (oltre 999 elementi o 999 × 999).

### Modalità Math / Line (1-14…1-22, 1-34)
- SET UP › **Input/Output**: `Math` (predefinita) o `Line`.
- Math: frazioni, potenze, radici, modelli e matrici in forma naturale; il
  menu MATH › MAT/VCT inserisce i modelli 2×2, 3×3, m×n (fino a 6×6), 2×1, 3×1,
  1×2, 1×3; i risultati mostrano in forma naturale matrici fino a 6×6, vettori
  fino a 6 elementi e liste fino a 20; oltre, il risultato si consulta nella
  memoria delle risposte.
- Line: inserimento e risultati su una sola riga.

### Selezione e appunti (1-11, 1-12)
- **SHIFT 8 (CLIP)** avvia la selezione, le frecce estendono l'evidenziazione,
  **F1 COPY** copia, **F2 CUT** taglia, **EXIT** annulla; **SHIFT 9 (PASTE)**
  incolla nel punto del cursore.
- In modalità Math l'intervallo è limitato ai movimenti del cursore; per le
  parentesi si può selezionare una parte dell'interno oppure l'intera
  espressione tra parentesi (mai una parentesi senza la sua corrispondente).

### Colori delle parentesi (1-8, 1-9)
- Sequenza dall'esterno verso l'interno: **blu, rosso, verde, magenta, nero**,
  poi di nuovo dal blu; la parentesi di chiusura prende il colore di quella di
  apertura; parentesi dello stesso livello hanno lo stesso colore;
  **dopo l'esecuzione tutte le parentesi diventano nere**.

### Posizione dell'errore (1-10)
- Dopo un errore, EXIT riporta all'espressione con il **cursore posizionato
  automaticamente nel punto che ha causato l'errore** (esempio `14÷0×2.3`).

## 3. Requisiti estratti dalle fonti hardware

| Voce | Raspberry Pi Pico 2 W | Pimoroni Pico Plus 2 W | Fonte |
|---|---|---|---|
| `PICO_BOARD` | `pico2_w` | `pimoroni_pico_plus2_w_rp2350` | header dell'SDK |
| Package | RP2350A (`PICO_RP2350A 1`), 30 GPIO | RP2350B (`PICO_RP2350A 0`), 48 GPIO | header dell'SDK |
| Flash | 4 MB | 16 MB | `PICO_FLASH_SIZE_BYTES` |
| PSRAM | assente | 8 MB, CS su GPIO47 (`PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN`) | header dell'SDK, pagina Pimoroni |
| SRAM | 520 KB | 520 KB | datasheet RP2350 |
| Wi-Fi / Bluetooth | CYW43439, pin 23/24/25/29 | modulo RM2 (CYW43439), stessi pin | header dell'SDK |
| VSYS | GPIO29 | GPIO43 | header dell'SDK |
| Altro | — | USB-C, pulsante BOOT su GPIO45, connettore Qw/ST (I²C), SWD JST-SH | pagina Pimoroni |

Conseguenze per questa versione: nessun nuovo driver e nessun pin nuovo; le
due schede **non** hanno la stessa memoria, quindi i limiti delle matrici
(dimensione massima, elementi totali salvati) sono definiti per scheda in
`limits.hpp`, come già avviene per liste e cronologia; il formato del record
resta lo stesso sulle due schede, con capacità diverse (2 × 64 KB e 2 × 512 KB).

## 4. Requisiti estratti dall'architettura esistente

- **Un solo motore di espressioni**: `Lexer → Parser → ast::Node → valutatori`
  in `src/expression.cpp`; matrici, vettori e liste vanno aggiunti a questo
  parser e a questo AST, non a un parser separato.
- `ast::Kind::Matrix` esiste già nell'AST e nel renderer, ma non è collegato al
  valutatore; `MatrixEditor` e `matrix*()` di `math_engine.hpp` sono API
  storiche non usate dalle applicazioni.
- L'editor produce il testo lineare canonico (`ast::serialize`) e lo reimporta:
  la posizione dell'errore è un offset in quel testo e va riportata all'albero
  dell'editor.
- Gli appunti (`Context::clipboard`) sono già un albero condiviso tra le
  applicazioni: la selezione parziale deve produrre un albero dello stesso tipo.
- Il record del workspace è versionato (versione 5): le aggiunte vanno in coda
  come versione 6 e le versioni 1–5 devono restare leggibili.
- `tests/architecture_test.cpp` verifica che Run-Matrix acceda solo alle
  sezioni del workspace dichiarate nel registro: la nuova sezione delle matrici
  va dichiarata.
- Limiti dello stack (32 KB): nessuna ricorsione nuova senza il controllo
  `checkStack`, nessun oggetto grande sullo stack.
