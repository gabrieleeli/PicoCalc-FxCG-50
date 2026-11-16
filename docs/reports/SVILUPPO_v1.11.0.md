# Sviluppo della v1.11.0 — rapporto

Data: 15 novembre 2026. Branch di destinazione: **`main`**; tag `v1.11.0`;
GitHub Release "PicoCalc FxCG-50 v1.11.0".

## 1. Punto di partenza

- Richiesta: partire **esattamente dal tag `v1.10.0`** (commit `dfdb08b`).
  `git fetch origin main --tags`: il tag annotato `v1.10.0` esiste e punta a
  `dfdb08b2eb296440e2110743374595a6f6c400e0`; `main`, `origin/main` e il
  branch di sessione coincidevano, quindi **nessuna modifica remota
  successiva** da preservare.
- Test host del profilo Pico Plus 2 W eseguiti sulla base prima di ogni
  modifica: 37/37 superati.
- Prima di modificare il codice è stato generato con i sorgenti della v1.10.0
  il fixture del record v6 (`tests/fixtures/workspace_v1_10_0.inc`) usato
  dalla prova di migrazione v6→v7.
- Sviluppo sul branch di sessione `ccr-b2793f7c-vk8p8o` con commit
  intermedi; su `main` arriva un solo commit con tutta la v1.11.0
  (avanzamento fast-forward da `dfdb08b`).

## 2. Fonti

Rapporto dedicato, scritto prima dell'implementazione:
[ANALISI_FONTI_v1.11.0.md](ANALISI_FONTI_v1.11.0.md). Il PDF del manuale
fx-CG50 non è scaricabile da questo ambiente (il sito Casio restituisce una
pagina HTML): sono state consultate le pagine pubblicate del manuale
fx-CG10/CG20 sui CSV (3-18…3-20), la pagina del manuale fx-CG50 sul formato
CSV (MEMORY › CSV › SET) e il manuale Base-N della fx-CG100; per il resto la
conoscenza del manuale fx-CG50 già usata nelle versioni precedenti. Le scelte
non verificabili sono indicate come tali nel rapporto delle fonti e in
[RUN_MATRIX.md](../RUN_MATRIX.md#limiti-noti).

## 3. Studio preliminare e vincoli

Letti per intero prima di scrivere codice: `README.md`, `CONTINUE.md`,
`docs/RUN_MATRIX.md`, `FEATURE_MATRIX.md`, `ARCHITECTURE.md`, `STORAGE.md`,
`MATH_ENGINE.md`, `reports/SVILUPPO_v1.10.0.md`, `src/expression.cpp`,
`include/expression.hpp`, `include/expression_ast.hpp`, `include/value.hpp`,
`src/run_matrix.cpp`, `src/apps/run_matrix_app.cpp`,
`src/apps/matrix_editor.cpp`, `src/natural_editor.cpp`,
`src/math_renderer.cpp`, `src/workspace.cpp`, `include/limits.hpp`,
`src/app_registry.cpp`, `tests/architecture_test.cpp` (e, per le liste e i
grafici, `statistics_app.cpp`, `graph_engine.cpp`, `graph_plot.cpp`,
`table_app.cpp`, `widgets.cpp`, `file_store.hpp`).

| Vincolo | Come è stato rispettato |
|---|---|
| Un solo parser, un solo AST, un solo valutatore | simboli ingegneristici (`#k`, postfissi), frazioni `a⌟b`, prefisso `0d`, numeri Base-N e l'operatore `Xnor` sono token e produzioni dello **stesso lexer/parser**; la semantica intera della Base-N è nello stesso valutatore (`combine()` al posto di `applyBinary()` in nodi, catene ed elementi di `CollEval`, attivata da `EvalContext::integerBase`); i comandi grafici non sono espressioni ma ogni loro argomento è valutato da `parseExpression` + `evaluateReal` |
| Dati solo nel workspace, tramite interfacce | List 1–26 e sottonomi nel workspace con metodi controllati (`setList`, `checkListLength`, `setListName`); i comandi grafici scrivono V-Window, SKETCH e grafici dei comandi tramite il modulo `graph_commands`; GRAPH li disegna. Test di architettura **più stretto**: nuove regole su Run-Matrix (vietati `ws.graph`, `SketchItem`, `CommandGraph`, `graph::`, `c.circle(`, `mathSegment`; obbligatori `graphcmd::execute` e `open(AppId::Graph)`), GRAPH deve disegnare i grafici dei comandi, i nuovi moduli (`value`, `csv_io`, `graph_commands`) sono verificati come livello del motore, i membri e i metodi delle liste sono associati alla sezione *Lists* (`dataSectionOfMember`) |
| Memoria embedded | nessun oggetto grande sullo stack (tabelle CSV, forme esatte, liste sull'heap/PSRAM); budget di tutte le liste per scheda (`kListStoreElements` 1 530 / 20 000); forme esatte solo per memorie fino a 64 elementi; CSV letti con un limite di byte per scheda |
| Retrocompatibilità | record **v7** in coda al v6, le liste 1–6 restano nel blocco della v4; record v1–v6 letti: provato con i fixture della v1.6.0, v1.8.0, v1.9.0 e il nuovo della v1.10.0 |

## 4. Funzionalità

| Requisito | Realizzazione | File principali |
|---|---|---|
| 4.1 Liste complete | List 1–26 nel parser (`list1`…`list26`), nel workspace (26 liste e sottonomi), in Statistics (26 colonne, riga SUB, SET XList/YList), in GRAPH (grafici da lista, menu LIST) e TABLE (variabile, LISTMEM), nei progetti `.pcgraph`; budget per scheda | `expression.cpp`, `workspace.*`, `limits.hpp`, `statistics_app.cpp`, `widgets.cpp`, `graph_engine.*`, `graph_app.cpp`, `table_app.cpp`, `graph_file.cpp`, `memory_app.cpp` |
| 4.2 CSV | modulo `csv_io` (codifica, decodifica, file, caricamento in liste e matrici), dialogo `FileDialog`, OPTN dell'editor delle liste, F4 CSV dell'editor MAT/VCT; microSD e LittleFS; scrittura atomica; separatore e decimale nel SET UP e in CSV SET | `csv_io.*`, `app_widgets.hpp`, `widgets.cpp`, `statistics_app.cpp`, `matrix_editor.cpp`, `system_app.cpp` |
| 4.3 MatAns/ListAns | `AnswerView`: griglia scorrevole in sola lettura, apertura automatica oltre la vista naturale, F4 dalla cronologia | `answer_view.cpp`, `run_matrix_app.cpp` |
| 4.4 Editor MAT/VCT | celle con espressioni (`setText` della griglia), forme esatte (`CalcValue::exact`, `setExact`) e complessi; colonne scorrevoli con ◀ ▶ e `r c`; forme esatte usate dal valutatore esatto | `value.*`, `matrix_editor.cpp`, `widgets.cpp`, `expression.cpp`, `run_matrix.cpp`, `workspace.cpp` |
| 4.5 Line | INS (sovrascrittura, cursore sottolineato, `OVR`), SHIFT+INS in Math (argomento del modello), `⌟` dal tasto frazione e nei risultati, risultati più larghi della riga scorrevoli, glifo `⌟` nei font | `natural_editor.*`, `math_renderer.*`, `run_matrix_app.cpp`, `run_matrix.cpp`, `font_data.cpp`, `tools/gen_fonts.py` |
| 4.6 Calcolo | simboli ingegneristici (OPTN › ESYM) e Eng con i simboli; ▶Simp con e senza fattore, Simplify Auto/Manual; Base-N intera (Mode, 32 bit, And/Or/Xor/Xnor/Not/Neg, prefissi, ▶Dec…▶Oct, base per riga della cronologia) | `expression.*`, `number_format.*`, `run_matrix.*`, `run_matrix_app.cpp`, `system_app.cpp` |
| 4.7 Comandi grafici | interprete `graph_commands` (Cls, ViewWindow, Graph Y=/r=/X=/(X,Y)=, Plot…, Line, F-Line, Circle, Vertical, Horizontal, Text, Pxl…, Tangent, Normal, Inverse; tutto o niente); GRAPH disegna i grafici dei comandi (`GraphSource::commands`), apre il grafico e torna a Run-Matrix con EXIT; registro: servizio GraphCmd, collegamenti Run-Matrix ⇄ Graph | `graph_commands.*`, `graph_model.hpp`, `graph_engine.*`, `graph_plot.cpp`, `graph_app.cpp`, `app_registry.*` |

Comportamenti cambiati volutamente (test adattati di conseguenza): in Line
il tasto frazione scrive `⌟`; Eng mostra i simboli e, per le frazioni, il
decimale; la riga SUB dell'editor delle liste scende alla prima cella con EXE.

Difetti trovati durante lo sviluppo (dai test e dal simulatore) e corretti
prima della pubblicazione: comando `Plot A,B` non riconosciuto per lo spazio
dopo il nome; in Hex `A xor F` diventava `AxorF` (l'editor non conserva gli
spazi) e veniva letto come variabili; i valori digitati dopo un sottonome
finivano nei sottonomi; il carattere `"` del comando Text non era ammesso
dall'editor.

## 5. Compatibilità hardware

Una sola base di codice per le due schede (`FXAI_BOARD=PICO_PLUS_2W` e
`PICO2W`); nessun pin, driver, partizione della flash o script del linker
cambiato. Le novità sono software; le differenze di memoria sono limiti per
scheda in `limits.hpp`:

| Limite | Raspberry Pi Pico 2 W | Pimoroni Pico Plus 2 W |
|---|---|---|
| Elementi per lista / in tutte le 26 liste | 255 / 1 530 (≈ 12 KB nel record da 59,75 KB) | 999 / 20 000 (≈ 156 KB nel record da 511,75 KB, PSRAM) |
| CSV letto al massimo | ≈ 41 KB | ≈ 484 KB |
| Matrici (invariato) | lato 40, 400 per matrice, 1 200 in tutto | lato 999, 10 000, 20 000 |

Dimensioni del firmware, RAM statica e verifica degli UF2 in §7 e in
[BUILD_STATUS.md](../BUILD_STATUS.md).

## 6. Test

| Verifica | Risultato |
|---|---|
| Test host, profilo Pimoroni Pico Plus 2 W | 40/40 test, **9 757** controlli nelle 38 suite C++ + ≈ 333 000 dello stress dell'allocatore + 41 dello script FAT/exFAT, 0 errori |
| Test host, profilo Raspberry Pi Pico 2 W | 40/40 test, **9 751** controlli + ≈ 334 000 + 41, 0 errori |
| AddressSanitizer + UBSan (entrambi i profili) | 40/40, 0 errori |
| ThreadSanitizer (allocatore su due thread, entrambi i profili) | 0 errori |
| Relay AI | 15/15 |
| Simulatore | nuovo script `tests/sim/run_matrix_v111.txt`, screenshot 52–61; rigenerati 06, 07, 18, 20–37, 40–45, 51 |
| GitHub Actions (`.github/workflows/release.yml`) | stessi passi a ogni push (ora anche sui branch di sessione `ccr-**`); l'esecuzione avviata su `main` con *publish* crea tag e release (§8) |
| **Hardware fisico** | **non eseguito**: nessuna PicoCalc disponibile in questa sessione; procedura [HARDWARE_TEST_v1.11.0.md](../HARDWARE_TEST_v1.11.0.md) |

Rispetto alla v1.10.0 (8 587 / 8 581 controlli): +960 delle tre suite nuove,
+92 in `architecture_test` (servizio, sorgenti e regole nuove), +12 in
`storage_test` e +102 in `matrix_test` (record più lungo, più tagli provati),
+3 in `editor_v110_test`, +1 in `expression_test`. Nessuna verifica è stata
tolta; i test esistenti sono stati adattati solo ai cambiamenti voluti
(versione 7 del record, List 7–26 ora valide, `⌟` dal tasto frazione in Line,
formato Eng con i simboli).

Nuove suite: `lists_csv_test` (List 1–26 nei calcoli, sottonomi, budget per
scheda, GRAPH con List 12, Statistics con 26 colonne, SUB e SET, codifica e
decodifica CSV con tutti gli errori, CSV su microSD e LittleFS, interruzione
di alimentazione durante il salvataggio sulla flash interna, CSV dalle
applicazioni anche senza scheda, celle esatte e complesse e scorrimento
dell'editor MAT/VCT, forme esatte nei calcoli e nel record, migrazione v6→v7
dal fixture della v1.10.0), `calc_v111_test` (simboli ingegneristici, Eng,
`⌟`, Simp con e senza fattore e Simplify Manual con ricalcolo e
persistenza, Base-N: letterali, limiti, complemento a due, operatori bit a
bit, Run-Matrix con basi diverse nella cronologia) e `run_matrix_v111_test`
(INS e SHIFT+INS nell'editor e da tastiera, `⌟`, scorrimento dei risultati,
MatAns/ListAns, tutti i comandi grafici con errori, limiti, ricalcolo e
persistenza, apertura di GRAPH e ritorno con EXIT, menu SKETCH, nuove voci
del SET UP).

Simulatore: nuovo script `tests/sim/run_matrix_v111.txt` con gli screenshot
52–61; rigenerati gli screenshot cambiati (Run-Matrix 18, 44, 45, 51 e quelli
di GRAPH/TABLE/3D che non mostravano ancora l'indicatore `Math` della
v1.10.0). Tutti compressi senza perdita (palette PNG, `compress_level` 9),
con la verifica che i pixel siano identici: da 46,7 MB non compressi a 245 KB.

## 7. Compilazione del firmware

| | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Esito | compilato, 0 warning | compilato, 0 warning |
| Immagine flash | 1 594 956 byte (38 % di 4 MB; +79 684 rispetto alla v1.10.0) | 1 586 168 byte (76 % di 2 MB; +79 652) |
| RAM statica | 302 556 byte (+924) | 298 632 byte (+924) |
| Heap SRAM | 191 012 byte | 194 936 byte |
| SHA-256 dell'UF2 locale | `5aff577afbae2ee20a644f319ab166606170322cc962523097b569687fc53018` | `b6e04305448a7720f6a7089e3b6a05ac88c2fe72f5e386c61d5e9a4cb085ccbe` |

Pico SDK 2.2.0, `gcc-arm-none-eabi` 13.2.1, `-O2`, build pulite
(`scripts/build.sh --clean`); `picotool info` riporta la versione
`1.11.0 (pico_plus_2w)` / `1.11.0 (pico2w)`. Frame di stack più grande:
`SystemApp::enter` 1 840 byte su 32 KB (nuove voci del SET UP, nessuna
ricorsione). Gli UF2 della release sono quelli compilati dal workflow sul
commit pubblicato. Dettagli in [BUILD_STATUS.md](../BUILD_STATUS.md).

## 8. Pubblicazione

- Commit unico della v1.11.0 su `main` (avanzamento fast-forward da
  `dfdb08b`; i commit intermedi restano sul branch di sessione
  `ccr-b2793f7c-vk8p8o`). Nessun force push, nessuna riscrittura della
  cronologia.
- Tag e release: il push dei tag da questo ambiente è rifiutato (HTTP 403),
  quindi il workflow `Build, test e release` viene avviato a mano su `main`
  con *publish* = true (*tag_v190* = false): esegue tutti i test, compila i
  due UF2, crea il tag annotato `v1.11.0` sul commit compilato e la release
  stabile "PicoCalc FxCG-50 v1.11.0" con le note di
  [../releases/v1.11.0.md](../releases/v1.11.0.md) e i file della stessa
  esecuzione (UF2 dei due moduli, zip dei sorgenti, `SHA256SUMS`).
- Hash del commit, link della release e verifica degli allegati sono nel
  rapporto finale della sessione.

## 9. Limitazioni note

- **Nessuna prova su hardware fisico**: procedura
  [HARDWARE_TEST_v1.11.0.md](../HARDWARE_TEST_v1.11.0.md) (HW-100…HW-115).
- Comandi grafici: realizzato un sottoinsieme coerente con la SKETCH di
  GRAPH; mancano `PxlTest(`, `DrawStat`, `DrawDyna`, `DrawR`, `DrawFTG`,
  `G-SelOn/Off`, `StoPict`/`RclPict`, `BG-Pict`, gli stili, le disequazioni e
  `G∫dx`; i grafici dei comandi non vanno nei progetti `.pcgraph`.
- Liste: niente List File 1–6 né riferimenti per sottonome nei calcoli.
- Simplify Manual: solo la frazione digitata resta non ridotta.
- Base-N: variabili A–F non raggiungibili in Hex; un esadecimale non può
  iniziare con `0d`/`0b`/`0o`/`0x`.
- Simboli P, E e conversioni ►ENG non realizzati.
