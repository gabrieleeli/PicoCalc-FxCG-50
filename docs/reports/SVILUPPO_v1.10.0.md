# Sviluppo della v1.10.0 — rapporto

Data: 14 novembre 2026. Branch di destinazione: **`main`**; tag `v1.10.0`;
GitHub Release "PicoCalc FxCG-50 v1.10.0".

## 1. Punto di partenza

- Richiesta: partire **esattamente dal tag `v1.9.0`**. Il tag non esisteva né
  in locale né su GitHub (nessun tag nel repository): il suo push era stato
  rifiutato nella sessione della v1.9.0 (HTTP 403, vedi
  [CONSOLIDAMENTO_v1.9.0.md](CONSOLIDAMENTO_v1.9.0.md#16-branch-e-tag)). Lo
  sviluppo è stato fermato e il problema segnalato; il proprietario ha
  indicato come base **`main` al commit `bb2bdee`** (VERSION 1.9.0, il commit
  che quel rapporto indica come destinazione del tag).
- Anche in questa sessione il push dei tag è rifiutato dall'ambiente (HTTP
  403, `git push origin v1.9.0`). Per questo tag e release vengono creati dal
  workflow GitHub Actions aggiunto in questa versione (§8), che crea anche il
  tag `v1.9.0` mancante su `bb2bdee`.
- Branch remoto `main` verificato prima di iniziare (`bb2bdee`, nessuna
  modifica locale dell'utente da preservare); nessun force push, nessuna
  riscrittura della cronologia.

## 2. Fonti

Rapporto dedicato, scritto prima dell'implementazione:
[ANALISI_FONTI_v1.10.0.md](ANALISI_FONTI_v1.10.0.md). In sintesi: la scheda
"Fonti" del repository GitHub **non esiste** (schede presenti: Code, Issues,
Pull requests, Actions, Projects, Security and quality, Insights; sezione
About vuota); sono stati usati il manuale software fx-CG50 v3.00 (letti per
intero i capitoli su modifica delle espressioni, colori delle parentesi,
correzione degli errori, appunti, modalità Math/Line, MAT/VCT, matrici,
vettori, liste e la tabella degli errori), gli header delle schede del Pico
SDK 2.2.0, la pagina e le specifiche del Pimoroni Pico Plus 2 W,
*Crafting Interpreters* (posizione dei token e segnalazione degli errori nel
parser a discesa ricorsiva) e la documentazione del progetto.

## 3. Architettura ricostruita

Prima di scrivere codice sono stati letti per intero: tokenizer, parser e
valutatori (`src/expression.cpp`), valori esatti (`exact`), AST condiviso
(`expression_ast.hpp`), nucleo e applicazione di Run-Matrix, Natural Editor e
renderer 2D, UI kit e widget, workspace e formato del record, registro delle
applicazioni e test di architettura, limiti per scheda, configurazione CMake
e porta delle schede. Vincoli rispettati:

- **un solo parser e un solo AST**: matrici, vettori e liste sono nuove
  produzioni della stessa grammatica e nuovi tipi di nodo (`Matrix`, `List`),
  non un parser separato;
- **un solo valutatore**: le regole delle collezioni (`CollEval`) sono un
  template sul dominio degli elementi (virgola mobile o esatto) e delegano
  ogni sotto-espressione numerica ai valutatori scalari esistenti, quindi
  GRAPH, TABLE, Equation e gli altri utenti del motore non cambiano
  comportamento (le loro suite passano invariate);
- **dati solo nel workspace**, letti tramite interfacce (`CollectionResolver`)
  e scritti con metodi controllati; il test di architettura verifica che
  Run-Matrix non acceda a `ws.lists`;
- **memoria embedded**: elementi sull'heap (PSRAM sulla Plus), nessun oggetto
  grande sullo stack, ricorsione controllata dal budget di stack esistente;
- **retrocompatibilità**: record v6 in coda al v5, versioni 1–5 lette.

## 4. Funzionalità

| Requisito | Realizzazione | File principali |
|---|---|---|
| 4.1 Editor MAT/VCT | `MatrixEditorView`: elenco Mat/Vct A–Z e Ans con dimensioni, lettera per saltare, DIM, celle con EXE verso destra, ROW-OP (SWAP, ×Rw, ×Rw+, Rw+), ROW/COLUMN DELETE/INSERT/ADD, DELETE, DEL-ALL, M⇔V; SHIFT 3 e OPTN › MAT/VCT; valori nel workspace (salvati e conservati tra le applicazioni) | `src/apps/matrix_editor.cpp`, `include/apps.hpp`, `src/workspace.cpp` |
| 4.2 Matrici, vettori, liste nel parser | `CalcValue`; letterali `[[…]]` `{…}`, `Mat`/`Vct`/`List`, indici; `evaluateAny`, `evaluateExactAny`; aritmetica, Det, Trn, inversa, Identity, Dim, Augment, Fill, Ref, Rref, Mat→List, List→Mat, DotP, CrossP, Angle, UnitV, Norm, funzioni di lista; `DimensionError`, `TypeError`; memorizzazione `→Mat A`, `→List 1`, `→Mat A[r,c]`, `{m,n}→Dim Mat A`; MatAns/VctAns/ListAns | `include/value.hpp`, `src/value.cpp`, `src/expression.cpp`, `include/expression.hpp`, `src/run_matrix.cpp` |
| 4.3 Math / Line | impostazione persistente nel SET UP e nella barra di stato; editor lineare (`setLinear`), modelli come testo, importazione lineare, conversione dell'input; risultati su una riga in Line, 2D in Math (matrici a griglia, frazioni negli elementi) | `src/natural_editor.cpp`, `src/apps/run_matrix_app.cpp`, `src/apps/system_app.cpp`, `src/run_matrix.cpp` |
| 4.4 Selezione e CLIP | `beginSelection`, `selectLeft/Right`, `copySelection`, `cutSelection`, `deleteSelection`; limite al campo o all'intero modello; bilanciamento di parentesi tonde, quadre e graffe; SHIFT 8/9, F1/F2, Ctrl+C/X/V; appunti condivisi | `src/natural_editor.cpp`, `src/apps/run_matrix_app.cpp` |
| 4.5 Parentesi colorate | colore per livello nel renderer (`NaturalDrawOp::color`, `math::Palette`), anche per il gruppo `( )`; nere nella cronologia; contrasto ≥ 4,5:1 su ogni sfondo verificato nei test | `src/math_renderer.cpp`, `include/ui_kit.hpp` |
| 4.6 Cursore sugli errori | posizione di token e nodi (`pos`, `len`), `ErrorLocation` nelle eccezioni, sotto-espressione in corso (`EvalInfo::error`), `ast::SerializeMap`, `NaturalEditor::markError`: cursore alla fine della causa (come nell'esempio del manuale) ed evidenziazione | `src/expression.cpp`, `include/expression_ast.hpp`, `src/natural_editor.cpp`, `src/run_matrix.cpp` |

## 5. Compatibilità hardware

Una sola base di codice; le due configurazioni restano quelle della v1.9.0
(`FXAI_BOARD=PICO_PLUS_2W` e `PICO2W`, cartelle di build e UF2 separati).
Verificati sugli header dell'SDK: identificativi delle schede, flash
(16 MB / 4 MB), RP2350B/RP2350A, PSRAM (CS GPIO47) solo sulla Plus, pin
CYW43, VSYS; script del linker, SPI (LCD SPI1, microSD SPI0), I²C (tastiera
I2C1), Wi-Fi, microSD e display invariati. Le novità sono solo software; le
differenze di memoria sono gestite con limiti per scheda (`limits.hpp`):
lato 40 / 999, 400 / 10 000 elementi per matrice, 1 200 / 20 000 in tutto
(Pico 2 W / Plus). Dettagli in [HARDWARE.md](../HARDWARE.md#differenze-tra-le-schede-che-riguardano-la-v1100).

## 6. Test

| Verifica | Risultato |
|---|---|
| Test host, profilo Pimoroni Pico Plus 2 W | 37/37 test, **8 587** controlli + ≈ 333 000 dello stress dell'allocatore + 41 dello script FAT/exFAT, 0 errori |
| Test host, profilo Raspberry Pi Pico 2 W | 37/37 test, **8 581** controlli + ≈ 334 000 + 41, 0 errori |
| AddressSanitizer + UBSan (entrambi i profili) | 0 errori |
| ThreadSanitizer (allocatore su due thread, entrambi i profili) | 0 errori |
| Relay AI | 15/15 |
| Simulatore | screenshot 12–19 rigenerati, nuovi 44–51 (`tests/sim/run_matrix_v110.txt`) |
| GitHub Actions (`.github/workflows/release.yml`) | test host, sanitizer, relay, firmware dei due moduli e pacchetto a ogni push: superati sulle esecuzioni dei commit intermedi del branch di lavoro; l'esecuzione avviata su `main` con *publish* crea tag e release (§8) |
| **Hardware fisico** | **non eseguito**: nessuna PicoCalc disponibile in questa sessione; procedura [HARDWARE_TEST_v1.10.0.md](../HARDWARE_TEST_v1.10.0.md) |

Nuove suite: `matrix_test` (673 controlli: parsing di matrici, vettori e
liste, operazioni, errori di dimensione e di tipo con posizione, annidamento
e Stack ERROR, forme esatte, memorie e nucleo di Run-Matrix, limiti per
scheda, record v6 e lettura dei record v1.9.0/v1.8.0) ed `editor_v110_test`
(161 controlli: modelli MAT/VCT, Math/Line, CLIP, parentesi colorate e
contrasto, cursore sugli errori, 600 sequenze casuali di modifica, flussi sul
firmware reale con riavvio). `oom_test` verifica l'avvio con un record troppo
grande per la memoria. Test esistenti adattati solo a cambiamenti voluti
(versione del record, primo campo del SET UP).

Difetti trovati dai test durante lo sviluppo e corretti prima della
pubblicazione: un ciclo infinito nel bilanciamento della selezione con una
parentesi chiusa senza corrispondente (trovato dal test a sequenze casuali),
un temporaneo distrutto troppo presto nel disegno dei risultati lista (trovato
dal simulatore), l'operando di `^` dopo una lista digitata.

## 7. Compilazione del firmware

| | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Esito | compilato, 0 warning | compilato, 0 warning |
| Immagine flash | 1 515 272 byte (36 % di 4 MB; +129 056 rispetto alla v1.9.0) | 1 506 516 byte (71 % di 2 MB; +129 000) |
| RAM statica | 301 632 byte (+2 188) | 297 708 byte (+2 188) |
| SHA-256 dell'UF2 locale | `ad810d07598f8239d485429bdb27ea4fcaff033a4a1d792a97837e465d194553` | `ab6a3d7e8d5cd8eff64d818511873486ce305401b09e3dd02429b6076f58915e` |

Pico SDK 2.2.0, `gcc-arm-none-eabi` 13.2.1, `-O2`, build pulite
(`scripts/build.sh --clean`); `picotool info` riporta la versione
`1.10.0 (pico_plus_2w)` / `1.10.0 (pico2w)` e le schede
`pimoroni_pico_plus2_w_rp2350` / `pico2_w`. Gli UF2 della release sono quelli
compilati dal workflow sul commit pubblicato. Dettagli in
[BUILD_STATUS.md](../BUILD_STATUS.md).

## 8. Pubblicazione

- Commit unico della v1.10.0 su `main` (avanzamento fast-forward da
  `bb2bdee`; i commit intermedi restano sul branch di lavoro
  `claude/inspiring-maxwell-xm4clv`).
- Tag e release: il workflow `Build, test e release`, avviato a mano su
  `main` con *publish*, esegue tutti i test, compila i due UF2, crea il tag
  annotato `v1.9.0` su `bb2bdee` e `v1.10.0` sul commit compilato e pubblica
  la release con le note di [../releases/v1.10.0.md](../releases/v1.10.0.md)
  e i file compilati nella stessa esecuzione (UF2 dei due moduli, zip dei
  sorgenti, `SHA256SUMS`).

## 9. Limitazioni note

- Nessuna prova su hardware fisico (display, tastiera, tempi di Det e inversa
  con matrici grandi, colori delle parentesi sul pannello).
- Liste: List 1–6 nei calcoli; List 7–26, nomi delle liste e CSV non
  realizzati; schermate MatAns/ListAns dedicate oltre 6×6 / 20 elementi non
  realizzate (si usa l'editor MAT/VCT o `Mat Ans`).
- Editor MAT/VCT: celle inserite come numeri reali; 4 colonne visibili.
- Modalità Line: niente INS (sovrascrittura) né simbolo `⌟`.
- Con la PSRAM della Plus disattivata, un record con molte matrici può non
  entrare nella SRAM all'avvio: il firmware parte vuoto e sospende i
  salvataggi (i dati restano in flash).
