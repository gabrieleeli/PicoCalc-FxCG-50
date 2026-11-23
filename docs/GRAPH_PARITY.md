# GRAPH e TABLE — tabella di parità con la fx-CG50 (v1.18.0)

Verifica voce per voce delle modalità **Graph** e **Table** rispetto alla
fx-CG50, riscritta nella v1.18.0 sul manuale **v3.60** (la versione precedente
di questo documento era della v1.8.0, sul manuale v3.50, con conteggi non più
veri). Analisi e decisioni: [reports/ANALISI_FONTI_v1.18.0.md](reports/ANALISI_FONTI_v1.18.0.md);
sviluppo, test e firmware: [reports/SVILUPPO_v1.18.0.md](reports/SVILUPPO_v1.18.0.md).

**Riferimento funzionale**: *fx-CG50 Software User's Guide* v3.60, capitolo 5
pagine **5-1…5-41** e **5-52…5-63** e Setup **1-35…1-37**. Dyna Graph,
Recursion e Conic Graphs (5-42…5-51 e l'analisi delle coniche) sono modalità
separate e **fuori ambito**. Il manuale è usato solo come descrizione del
comportamento: nessun codice, grafica, font, testo o formato di file Casio
(le immagini usano il formato proprio `.pcpict`, non `g3p`).

**Esito (v1.18.0)**: ogni voce della tabella è presente e provata, oppure è una
differenza documentata (§10). Graph e Table sono **Available** su **entrambe le
schede** (Pimoroni Pico Plus 2 W e Raspberry Pi Pico 2 W): nessuna differenza
fra le schede. **Nessuna voce è TESTED ON HARDWARE**: prove sul PC (test su
due profili, sanitizer, fuzzing, simulatore) e build del firmware;
procedure fisiche in [HARDWARE_TEST_v1.18.0.md](HARDWARE_TEST_v1.18.0.md).

## Stati

| Stato | Significato |
|---|---|
| **TESTED ON HOST** | test automatici sul PC superati, sullo stesso sorgente del firmware, su entrambi i profili di scheda |
| **TESTED ON SIMULATOR** | in più, flusso verificato nel simulatore con screenshot (`docs/screenshots/`) |
| **TESTED ON HARDWARE** | provata sulla PicoCalc fisica (nessuna voce) |
| **FUORI AMBITO** | funzione di un'altra modalità |

## Sintesi

| Stato | Graph (G01–G83) | Table (T01–T13) | Setup (S01–S18) | Totale |
|---|---:|---:|---:|---:|
| TESTED ON SIMULATOR | 32 | 6 | 4 | 42 |
| TESTED ON HOST | 51 | 6 | 14 | 71 |
| FUORI AMBITO | 0 | 1 | 0 | 1 |
| IMPLEMENTED, PARZIALE, NOT STARTED | **0** | **0** | **0** | **0** |
| **Totale** | **83** | **13** | **18** | **114** |

Prima della v1.18.0 (audit della fase 0): 24 TESTED ON SIMULATOR, 50 TESTED ON
HOST, 2 IMPLEMENTED, 16 PARZIALI, 6 diverse dal manuale (errori E1–E6), 15 NOT
STARTED, 1 fuori ambito. Le estensioni del progetto (§9) non contano.

Test della v1.18.0: `graph_v118_test` (514 controlli su ciascuna scheda), `table_v118_test`
(42), `workspace_v118_test` (35 sulla Plus, 34 sul Pico 2 W), più i test di
Graph e Table delle versioni precedenti (`graph_app_test`, `graph_engine_test`,
`graph_solve_test`, `graph_file_test`, `graph_verify_test`, `table_test`,
`graph_cmd_v112_test`). Screenshot nuovi **148–160** (`tests/sim/graph_v118.txt`).

## 1. Lista delle relazioni, tipi, proprietà (5-1…5-4, 5-13…5-20)

| # | Funzione (pagina) | Stato | Implementazione | Verifica |
|---|---|---|---|---|
| G01 | Lista Y1…Y20, colore della riga = colore del grafico (5-1) | TESTED ON SIMULATOR | `GraphApp::drawList` | graph_app_test, scr. 20 |
| G02 | DRAW con F6 o EXE; → per modificare (5-2, 5-17, 5-18) | TESTED ON HOST | `listKey` (v1.18.0, era E5) | graph_v118_test G02 |
| G03 | G⇔T (SHIFT F6); AC dal grafico torna alla lista (5-2) | TESTED ON HOST | `plotKey` | graph_v118_test G02/G03 |
| G04 | TYPE: Y=, r=, Param, X=, disequazioni Y e X (5-3, 5-13) | TESTED ON SIMULATOR | menu TYPE | graph_app_test, scr. 20/22/23 |
| G05 | CONVERT solo tra rettangolari e disequazioni (5-17) | TESTED ON HOST | `rewriteIdentifiers` | graph_app_test |
| G06 | TOOL › STYLE: Normal, Thick, Broken, Dot, Thin (5-3, 5-17) | TESTED ON SIMULATOR | | graph_app_test, scr. 20 |
| G07 | Ineq Type Intsect / Union (5-3) | TESTED ON SIMULATOR | | scr. 23/34 |
| G08 | Suffissi °, ʳ, ᵍ: OPTN › ANGLE negli editor di Graph e Table; grafico, G-Solve, tabella (5-4) | TESTED ON SIMULATOR | `Menu::EditAngle`, `TableApp` OPTN | graph_v118_test G08, table_v118_test, scr. 158 |
| G09 | Funzione di un altro tipo su una riga occupata: errore (5-13) | TESTED ON HOST | `storeEdit` | graph_app_test |
| G10 | Parametriche Xt/Yt; DELETE della coppia (5-14, 5-18) | TESTED ON HOST | | graph_app_test |
| G11 | Funzioni composte, al più 5 livelli (5-14) | TESTED ON HOST | `FunctionResolver` | graph_engine_test |
| G12 | FORMAT dalla lista: Line Style, Line Color, scelta anche con i tasti numerici (5-15, 5-16) | TESTED ON HOST | `formatKey` | graph_v118_test G12 |
| G13 | FORMAT dal grafico: lampeggio, ↑/↓, EXE; con un solo grafico direttamente (5-16) | TESTED ON HOST | `Action::Format` | graph_app_test, graph_v118_test G14 |
| G14 | Grafici incollati: FORMAT dal grafico; sovrapposizioni e liste: nessun cambio (5-28, 5-30, 5-31) | TESTED ON HOST | `pastedFormat_`, `GraphEngine::setPlotFormat` (era E3) | graph_v118_test G14 |
| G15 | Modifica (→) e cancellazione (F2 o DEL, Sì/No) (5-17, 5-18) | TESTED ON HOST | | graph_app_test |
| G16 | SELECT disegno/non disegno, «=» evidenziato (5-18) | TESTED ON HOST | | graph_app_test |
| G17 | Grid On/Line, Axes On/Off/Scale, Label (5-18, 5-19) | TESTED ON SIMULATOR | `graphview::drawFrame` | graph_app_test, scr. 24/26 |
| G18 | Graph Memory STORE/RECALL 1–20 (5-19, 5-20) | TESTED ON HOST | `PCGMEM 1` | graph_app_test, graph_file_test |

## 2. V-Window, zoom, pan, sfondo (5-5…5-12)

| # | Funzione (pagina) | Stato | Implementazione | Verifica |
|---|---|---|---|---|
| G19 | V-Window a 10 parametri, Xdot ↔ Xmax, espressioni (5-5, 5-6) | TESTED ON SIMULATOR | | graph_app_test, scr. 29 |
| G20 | INITIAL / TRIG / STANDRD (5-5) | TESTED ON HOST | | graph_engine_test, graph_app_test |
| G21 | V-MEM STORE/RECALL 1–6 (5-6, 5-7) | TESTED ON HOST | | graph_app_test |
| G22 | SQUARE: Y-BASE / X-BASE (anche per il riquadro del Dual Screen) (5-5) | TESTED ON HOST | `squareWindow(v, rows)` | graph_app_test |
| G23 | BGV-WIN nella V-Window, solo con uno sfondo aperto (5-5, 5-11) | TESTED ON HOST | | graph_app_test |
| G24 | Precauzioni della V-Window (5-6) | TESTED ON HOST | `validateView` | graph_app_test |
| G25 | Intervallo `,[a,b]` per tutti i tipi (5-7) | TESTED ON HOST | `splitFunctionText` | graph_engine_test |
| G26 | ZOOM BOX (5-8, 5-9) | TESTED ON HOST | | graph_app_test |
| G27 | ZOOM FACTOR, IN, OUT (5-8) | TESTED ON HOST | | graph_app_test |
| G28 | ZOOM AUTO (5-8) | TESTED ON HOST | | graph_engine_test |
| G29 | ZOOM ORIGINAL, SQUARE, ROUND, INTEGER, PREVIOUS (5-8) | TESTED ON HOST | | graph_app_test |
| G30 | Zoom con + e − (5-9) | TESTED ON HOST | | graph_app_test |
| G31 | PAN anche in Table; non in G+G e GtoT (5-9, 5-10) | TESTED ON HOST | `Menu::DualOptn` | table_v118_test, graph_v118_test G41–G48 |
| G32 | Scorrimento del grafico con i tasti cursore | TESTED ON HOST | `plotKey` | graph_v118_test G32 |
| G33 | Background None / PICT n / OPEN con cartelle, conferma della V-Window (5-10, 5-11) | TESTED ON HOST | `openFiles` | graph_app_test, graph_v118_test G38 |
| G34 | BGV-WIN SAVE e SAVE•AS con la cartella (5-11, 5-12) | TESTED ON HOST | `Browse::BgvSaveAs` | graph_app_test |
| G35 | Fade I/O: cursore a passi del 5 %, valore digitato, salvato con l'immagine di sfondo (5-12) | TESTED ON HOST | `fadeSlider_`, `bgvSave(…, keepView)` | graph_v118_test G35 |

## 3. Immagini (5-20…5-22)

| # | Funzione (pagina) | Stato | Implementazione | Verifica |
|---|---|---|---|---|
| G36 | Immagine del grafico e immagine dello sfondo separate, V-Window (5-20) | TESTED ON HOST | `.pcpict` versione 2 a due livelli (`pict::compose`), Fade I/O solo sullo sfondo (decisione D3) | graph_v118_test G36, workspace_v118_test |
| G37 | STORE in Pict 1–20 (5-21) | TESTED ON HOST | `PICOCALC/PICT/PICTnn.pcpict` | graph_app_test |
| G38 | SAVE•AS e OPEN con la scelta della cartella (5-21, 5-22) | TESTED ON SIMULATOR | cartelle sotto `PICOCALC/PICT` | graph_v118_test G38, scr. 157 |
| G39 | Uno schermo diviso non si salva in un'immagine (5-21) | TESTED ON HOST | `Menu::Picture` | graph_v118_test G41–G48 |
| G40 | RECALL dietro i grafici, sopra lo sfondo; Cls la cancella (5-22) | TESTED ON HOST | | graph_app_test, graph_v118_test G38 |

## 4. Dual Screen G+G (5-23, 5-24)

Disposizione sulla PicoCalc: principale **sopra**, sotto-schermo **sotto**,
320×140 ciascuno (decisione D1, differenza DI-1). Il principale è sempre attivo;
lo ZOOM parte dalla V-Window del principale e scrive quella del sotto-schermo
(decisione D2). Codice: `src/apps/graph_dual.cpp`, `src/apps/graph_view.cpp`,
`graph_plot.cpp` (curve per riquadro).

| # | Funzione (pagina) | Stato | Verifica |
|---|---|---|---|
| G41 | Setup «Dual Screen» = G+G: principale e sotto-schermo (5-23) | TESTED ON SIMULATOR | graph_v118_test, scr. 148, 150 |
| G42 | V-Window del sotto-schermo: F6 SOTTO / SOPRA (5-23: RIGHT/LEFT) | TESTED ON SIMULATOR | graph_v118_test, scr. 149 |
| G43 | OPTN › COPY (5-23) | TESTED ON SIMULATOR | graph_v118_test, scr. 150 |
| G44 | OPTN › SWAP: grafici e V-Window scambiati (5-23) | TESTED ON HOST | graph_v118_test |
| G45 | Indicatori S (solo sotto) ed E (entrambi) nella lista; SELECT li cancella (5-23, 5-24) | TESTED ON SIMULATOR | graph_v118_test, scr. 151 |
| G46 | FORMAT solo per il principale; S non modificabile (5-24) | TESTED ON HOST | graph_v118_test |
| G47 | Trace, Sketch, G-Solve sul principale; ZOOM verso il sotto-schermo (5-23) | TESTED ON SIMULATOR | graph_v118_test, scr. 152 |
| G48 | AC dal grafico doppio torna alla lista (5-24) | TESTED ON HOST | graph_v118_test |

## 5. Disegno manuale (5-25…5-31)

| # | Funzione (pagina) | Stato | Implementazione | Verifica |
|---|---|---|---|---|
| G49 | Comandi grafici di Run-Matrix (SKETCH › GRAPH, Param, G∫dx) (5-25…5-27) | TESTED ON SIMULATOR | `graph_commands.cpp` | graph_cmd_v112_test, run_matrix_v111_test, scr. 61/85/86 |
| G50 | Grafici delle funzioni predefinite senza variabile (`Graph Y=sin`, `Graph r=cos`, x², x⁻¹, 10^, e^, √, ∛, log, ln, d/dx, d²/dx², ∫) (5-26) | TESTED ON HOST | `builtInGraph` | graph_v118_test G50 |
| G51 | Sovrapposizione `[A=…]`: colori in sequenza, ciano/giallo → colore predefinito anche per il primo; variabile non X, Y, r, θ, T (5-27, 5-28) | TESTED ON HOST | `GraphEngine::build` (era E2) | graph_v118_test G51, graph_engine_test |
| G52 | Grafici da lista: stessa lunghezza o Dimension ERROR, colori (5-29, 5-30) | TESTED ON HOST | | graph_engine_test, graph_v118_test G51 |
| G53 | Copia e incolla «Y=…»/«…», blu e Normal, solo con Dual Screen Off, 30 grafici (5-30, 5-31) | TESTED ON HOST | `pasteGraph` | graph_app_test, graph_v118_test |

## 6. Modify (5-38…5-41)

| # | Funzione (pagina) | Stato | Verifica |
|---|---|---|---|
| G54 | MODIFY: 1–5 variabili, Step, ←/→, valore digitato, magenta (5-38, 5-39) | TESTED ON SIMULATOR | graph_app_test, scr. 28 |
| G55 | BUILT-IN come Dyna Graph (5-39) | TESTED ON HOST | graph_app_test |
| G56 | COPY nella lista, «Expression in use» (5-41) | TESTED ON HOST | graph_app_test |
| G57 | MODIFY con Dual Screen Off (5-39) | TESTED ON HOST | graph_v118_test |

## 7. Sketch (5-52, 5-53)

| # | Funzione (pagina) | Stato | Implementazione | Verifica |
|---|---|---|---|---|
| G58 | Sketch Line e Plot/LineCol come valori iniziali (5-52) | TESTED ON HOST | | graph_app_test, graph_v118_test G61 |
| G59 | Cls, Plot/PlotOn/PlotOff/PlotChg, Line, F-Line, Circle, Vertical, Horz, PEN, Text (5-52) | TESTED ON SIMULATOR | | graph_app_test, scr. 27 |
| G60 | Inverse disegnato subito con Sketch Line e Plot/LineCol (5-53) | TESTED ON HOST | | graph_app_test |
| G61 | Tangent / Norm con Sketch Line e Plot/LineCol, una dopo l'altra (5-53) | TESTED ON SIMULATOR | `traceKey` (era E1) | graph_v118_test G61, scr. 156 |
| G62 | FORMAT durante l'operazione (stile e colore delle linee, colore di punti e testo) (5-52) | TESTED ON SIMULATOR | `sketchFormatOpen` | graph_v118_test G62, scr. 156 |
| G63 | Funzioni a due punti (5-53) | TESTED ON HOST | | graph_app_test |

## 8. Analisi (5-54…5-61)

| # | Funzione (pagina) | Stato | Implementazione | Verifica |
|---|---|---|---|---|
| G64 | TRACE: centro, ←/→, ↑/↓ tra i grafici, uscita con SHIFT F1 (5-54) | TESTED ON SIMULATOR | | graph_app_test, scr. 24 |
| G65 | Coordinate in basso **e accanto al puntatore** (5-54) | TESTED ON SIMULATOR | `pointerLabel_` | graph_v118_test G65, scr. 155 |
| G66 | Linee ausiliarie; SHIFT ↓ / SHIFT ↑ (5-54) | TESTED ON HOST | `traceLines_` | graph_v118_test G66 |
| G67 | Valore con il tasto X,θ,T o una cifra (5-54) | TESTED ON HOST | `traceKey` | graph_v118_test G67 |
| G68 | Puntatore invisibile fuori dall'area o senza valore; Coord Off (5-54) | TESTED ON HOST | | graph_app_test, graph_v118_test |
| G69 | Formato per tipo; disequazioni > < con «:» (5-54) | TESTED ON HOST | `drawOverlays` | graph_v118_test G69 |
| G70 | EXE: punto con etichetta, pieno o vuoto; DEL toglie l'ultimo (5-55) | TESTED ON SIMULATOR | | scr. 24 |
| G71 | Derivative On in Trace (5-55) | TESTED ON SIMULATOR | | scr. 24 |
| G72 | **Graph to Table** (GtoT): EXE memorizza, CHANGE, R•DEL, DEL•ALL, LISTMEM, derivate (5-55) | TESTED ON SIMULATOR | `storeGtoT`, `tableKeyGtoT` | graph_v118_test G72, scr. 153, 154 |
| G73 | ZOOM ROUND (5-56) | TESTED ON HOST | | graph_engine_test |
| G74 | G-SOLVE ROOT con la derivata (5-56, 5-57) | TESTED ON SIMULATOR | | graph_solve_test, scr. 25 |
| G75 | MAX, MIN, Y-ICEPT (5-56) | TESTED ON SIMULATOR | | scr. 06 |
| G76 | INTSECT (5-57, 5-58) | TESTED ON HOST | | graph_app_test |
| G77 | Y-CAL / X-CAL (5-58) | TESTED ON HOST | | graph_app_test |
| G78 | ∫dx con il puntatore o digitato, riempimento (5-59) | TESTED ON SIMULATOR | | scr. 07 |
| G79 | ∫dx ROOT, Not Found, errore con ≥ 21 radici (5-59, 5-60) | TESTED ON SIMULATOR | | scr. 26, graph_v118_test G79 |
| G80 | ∫dx INTSECT, errore con ≥ 21 intersezioni (5-60) | TESTED ON HOST | | graph_app_test, graph_v118_test G79 |
| G81 | ∫dx MIXED (5-61) | TESTED ON HOST | | graph_app_test |
| G82 | Precauzioni: tangenze e flessi (5-56, 5-58) | TESTED ON HOST | | graph_solve_test |
| G83 | G-Solve solo Y= e disequazioni Y; «Not Found» | TESTED ON HOST | | graph_app_test |

## 9. Table (5-1, 5-9, 5-32…5-37)

| # | Funzione (pagina) | Stato | Implementazione | Verifica |
|---|---|---|---|---|
| T01 | Lista delle relazioni di Table (5-1, 5-32) | TESTED ON SIMULATOR | | table_test, scr. 35 |
| T02 | Variable: range o LIST (5-32, 5-33) | TESTED ON SIMULATOR | | table_test, scr. 36 |
| T03 | FORMAT (Line Style, Line Color) nell'OPTN della lista; STYLE = stile (5-15, 5-33) | TESTED ON HOST | `TableApp` (era E6) | table_v118_test, table_test |
| T04 | SELECT, TABLE (5-33) | TESTED ON SIMULATOR | | table_test |
| T05 | Celle di 6 cifre, valore completo (5-33) | TESTED ON SIMULATOR | `graphview::drawTable` | scr. 36 |
| T06 | Derivative On: colonne, «dY/dX», errore con intervallo/sovrapposizione (5-34) | TESTED ON SIMULATOR | | scr. 32 (posizione: DI-5) |
| T07 | Tipo Y=/r=/Param, un tipo per tabella (5-34) | TESTED ON HOST | | table_test |
| T08 | FORMULA, DELETE, ROW DELETE/INSERT/ADD, EDIT (5-34) | TESTED ON HOST | INSERT/ADD copiano la riga (il manuale non dice il valore della riga nuova) | table_test |
| T09 | LISTMEM, 999 valori su entrambe le schede (5-35) | TESTED ON HOST | `kListMaxLength` = 999 (decisione D4) | table_test, table_v118_test |
| T10 | GPH-CON / GPH-PLT; G⇔T o AC tornano alla tabella (5-35) | TESTED ON HOST | | table_v118_test, scr. 33 |
| T11 | Dopo GPH-CON: Trace sulle righe, Zoom, Sketch, FORMAT, PAN (5-36, 5-9) | TESTED ON HOST | `traceMove` (era E4) | table_v118_test |
| T12 | **T+G**: tabella sotto, grafico sopra; CHANGE o AC attivano la tabella (5-36, 5-37) | TESTED ON SIMULATOR | `TableApp::drawTable`, layout `TableGraph` | table_v118_test, scr. 159, 160 |
| T13 | Il Dual Screen di Table vale anche in Recursion (5-37) | FUORI AMBITO | Recursion è una modalità Base | — |

## Setup (1-35…1-37)

| # | Voce | Stato | Verifica |
|---|---|---|---|
| S01 | Func Type | TESTED ON HOST | graph_app_test |
| S02 | Draw Type | TESTED ON HOST | graph_app_test |
| S03 | Derivative (Trace, GtoT, T+G) | TESTED ON HOST | graph_v118_test G72, table_test |
| S04 | Angle | TESTED ON HOST | graph_app_test |
| S05 | Coord | TESTED ON HOST | graph_v118_test G65 |
| S06 | Grid | TESTED ON HOST | graph_app_test |
| S07 | Axes | TESTED ON SIMULATOR | scr. 24/26 |
| S08 | Label | TESTED ON SIMULATOR | scr. 24 |
| S09 | Graph Func | TESTED ON HOST | graph_app_test |
| S10 | Dual Screen G+G / GtoT / Off (Graph) | TESTED ON SIMULATOR | graph_v118_test, scr. 148 |
| S11 | Simul Graph | TESTED ON HOST | graph_app_test |
| S12 | Background | TESTED ON HOST | graph_app_test |
| S13 | Plot/LineCol (anche Tangent/Norm) | TESTED ON HOST | graph_v118_test G61 |
| S14 | Sketch Line (anche Tangent/Norm) | TESTED ON HOST | graph_v118_test G61 |
| S15 | Ineq Type | TESTED ON SIMULATOR | scr. 23/34 |
| S16 | Variable RANG/LIST (Table) | TESTED ON HOST | table_test |
| S17 | Dual Screen T+G / Off (Table) | TESTED ON HOST | table_v118_test |
| S18 | SET UP dentro Graph/Table | TESTED ON HOST | graph_app_test, table_test |

Input/Output, Frac Result, Complex Mode e Display sono impostazioni globali di
SYSTEM.

## 10. Differenze documentate

| # | Voce | Differenza | Motivo |
|---|---|---|---|
| DI-1 | G41–G48, G72, T12 | Dual Screen **sopra/sotto** (320×140 + 320×140) invece che sinistra/destra | lo schermo della PicoCalc è quadrato (320×320, area del grafico 320×282): due riquadri affiancati sarebbero 159×282, con punti 3,6 volte più alti che larghi; sopra/sotto la risoluzione orizzontale resta quella dello schermo pieno (decisione D1) |
| DI-2 | G42, G45 | F6 **SOTTO/SOPRA** al posto di RIGHT/LEFT; indicatori **S/E** al posto di R/B | conseguenza di DI-1 |
| DI-3 | G36…G40 | formato `.pcpict` (versione 2 a due livelli) invece di `g3p` | il formato Casio è proprietario; il comportamento (due livelli, V-Window, Fade, richiamo dietro i grafici) è quello del manuale |
| DI-4 | G38 | cartelle di un livello sotto `PICOCALC/PICT`; una cartella si crea dal browser delle immagini (NEW F) | la radice della memoria di massa è la cartella delle immagini del progetto sulla microSD o nella flash interna; sulla fx-CG50 le cartelle si creano in MEMORY |
| DI-5 | T06 | «dY/dX=» nella riga del valore in basso, non nella riga in alto | la riga in alto della PicoCalc è la barra di stato |
| DI-6 | G65 | nessuna riga d'aiuto «[EXE]: …» durante il TRACE | la fx-CG50 la mostra nella sua barra di stato; sulla PicoCalc la barra di stato mostra lo stato della calcolatrice e la riga in alto il grafico tracciato |
| DI-7 | S10 | «Dual Screen» nel Setup di Graph dopo Background (dove la v1.17.0 aveva «Fade I/O») | Fade I/O sulla fx-CG50 non è una voce di Setup ma il cursore di OPTN (5-12, G35); l'ordine delle prime righe resta quello già pubblicato |
| DI-8 | G70 | DEL della PicoCalc al posto di ALPHA DEL | tastiera della PicoCalc |
| DI-9 | T09 | sul Pico 2 W tutte le liste insieme restano al più 1 530 elementi (una sola lista piena da 999) | record del workspace da 59,75 KB; il limite per lista è quello della fx-CG50 |

Nessuna differenza fra le due schede: lo stesso codice (servizio GraphCore,
`graph_view.cpp`, `graph_dual.cpp`) è compilato sulla Pimoroni Pico Plus 2 W e
sul Raspberry Pi Pico 2 W.

## 11. Estensioni del progetto (non contano per la parità)

| Funzione | Stato | Verifica |
|---|---|---|
| Progetto `.pcgraph` (anche Dual Screen: `dual`, `subview`, `side`, `tabledual`), FILE NEW/OPEN/SAVE/SAVE AS/RENAME/DELETE | TESTED ON SIMULATOR | graph_file_test, workspace_v118_test, scr. 31 |
| Scrittura atomica e recupero, microSD (SPI0) e flash interna (LittleFS) | TESTED ON HOST | graph_file_test, sd_fatfs_test |
| Esportazione BMP | TESTED ON HOST | graph_app_test |
| ZOOM TRIG/STANDRD/INITIAL dal grafico; SKETCH › Elementi | TESTED ON HOST | graph_app_test |
| Campionamento adattivo (poli, domini, salti), clipping, disegno incrementale, budget dei punti per scheda, layer in PSRAM | TESTED ON HOST | graph_engine_test, graph_verify_test, graphics_memory_test |
| OPTN › → TABLE; persistenza di tabella e Dual Screen nel workspace (sezioni 0x0104/0x0106) e nelle strisce di eActivity (parte Dual) | TESTED ON HOST | table_test, workspace_v118_test |

## 12. Fuzzing

`fuzz_dual` (nuovo: tasti in G+G/GtoT/T+G con V-Window e tabelle casuali,
invarianti dei riquadri e della persistenza), `fuzz_ui`, `fuzz_workspace`,
`fuzz_pcgraph`, `fuzz_eactivity`: risultati in
[reports/SVILUPPO_v1.18.0.md](reports/SVILUPPO_v1.18.0.md).
