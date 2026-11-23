# Analisi delle fonti — v1.18.0: Graph e Table alla parità completa

Scritto **prima** del codice della v1.18.0 (fase 0). Obiettivo della versione:
portare le modalità **Graph** e **Table** alla parità completa con la fx-CG50
su **entrambe le schede** (Pimoroni Pico Plus 2 W e Raspberry Pi Pico 2 W),
così che restino **Available** secondo la definizione del progetto: ogni voce
della tabella di parità presente e testata, oppure documentata come differenza
inevitabile. La v1.18.0 era prevista per Statistics: **Statistics e l'ingresso
di Distribution come Base passano alla v1.19.0** (ROADMAP e CONTINUE da
aggiornare con il codice).

Regole che non cambiano: manuale fx-CG50 v3.60 solo come descrizione del
comportamento (niente codice, grafica, font o testi copiati); un test per voce;
nessun «TESTED ON HARDWARE» senza prova fisica (le prove nuove partono da
**HW-246**); screenshot esistenti identici, quelli nuovi dal **148**
(`tests/sim/graph_v118.txt`); un solo commit «PicoCalc FxCG-50 v1.18.0: …».
Nuovo in questa versione: **eccezione al congelamento del Pico 2 W limitata a
Graph e Table** (§9).

## 1. Fonti

| # | Fonte | Uso |
|---|---|---|
| 1 | *fx-CG50 … Software User's Guide* **v3.60** (PDF ufficiale Casio, 678 pagine, lo stesso della v1.15.0), letto il 23/11/2026: cap. 5 pagine **5-1…5-41** (lista delle relazioni, V-Window, zoom, pan, sfondo, disegno, immagini, Dual Graph, disegno manuale, Table, Modify) e **5-52…5-63** (Sketch, analisi); Setup **1-35…1-37** | solo descrizione del comportamento: menu, tasti, messaggi, limiti; le schermate del manuale sono state guardate per capire la disposizione (nessuna copiata) |
| 2 | Codice e documentazione a `aa2923c` (v1.17.0, branch `claude/graph-table-parity-v118-1is1v6` = `main`) | stato reale di ogni voce |
| 3 | Sonda sul motore (programma temporaneo fuori dal repository, collegato a `libfxai_core.a`) | verifica dei suffissi d'angolo nel percorso di Graph/Table (§4, voce aperta 6) |

Dyna Graph (5-42…5-44), Recursion (5-45…5-49) e Conic Graphs (5-50, 5-51,
5-61…5-63 «Conic Section Graph Analysis») sono **fuori ambito**.

### 1.1 Punto di partenza verificato

| Controllo | Esito |
|---|---|
| `git status` | branch di sessione = `main` (`aa2923c`, v1.17.0), albero pulito |
| Build host (profilo Plus) | compilata senza warning (`build/host-pico_plus_2w`) |
| Firmware Pico 2 W (`--clean`) | **1 944 228 B** (= v1.17.0), RAM statica 307 284 B con `.heap`; 169 oggetti esclusi, nessuno nell'immagine |
| Firmware Plus (`--clean`) | **2 625 472 B** (= v1.17.0, 62 % di 4 MB, budget 3 774 873 B), RAM statica 312 760 B |
| Moduli di Graph/Table nel Pico 2 W (mappa del linker) | `apps/graph_plot.cpp` 69 593 B, `apps/graph_app.cpp` 37 064 B, `graph_engine.cpp` 32 294 B, `apps/table_app.cpp` 29 139 B, `graph_file.cpp` 28 764 B, `graph_solve.cpp` 17 486 B, `apps/graph_files.cpp` 15 373 B |

## 2. Perché rifare l'audit

`docs/GRAPH_PARITY.md` è della v1.8.0, si basa sul manuale v3.50 e ha conteggi
non più veri: la sintesi dice 135 voci con 6 NOT STARTED, ma i comandi grafici
di Run-Matrix (completati nella v1.13.0) e i suffissi d'angolo (motore, v1.13.0)
sono ancora contati come mancanti, il conteggio non include le voci aggiunte
dopo, alcune righe uniscono più funzioni del manuale e altre del manuale
mancano del tutto (EXE sulla lista, AC dal grafico, etichetta delle coordinate
accanto al puntatore, «:» delle disequazioni strette, FORMAT dei grafici
incollati, cartelle delle immagini, cursore di Fade I/O, grafici delle funzioni
predefinite in Run-Matrix). La nuova tabella (§3) ha **una riga per funzione
del manuale v3.60**, con la pagina, lo stato **di oggi** verificato nel codice
e nei test, e l'azione della v1.18.0. Le estensioni del progetto (file,
microSD, BMP, campionamento) stanno in una tabella a parte (§3.11) e non
contano per la parità.

Stati: **NOT STARTED** (nessun codice), **PARZIALE** (codice che copre solo una
parte della voce), **IMPLEMENTED** (codice senza test dedicato), **TESTED ON
HOST**, **TESTED ON SIMULATOR**, **FUORI AMBITO**. Nessuna voce è TESTED ON
HARDWARE. «Errore» = comportamento presente ma diverso dal manuale (trovato
in questa analisi, si corregge con un test che fallisce prima della correzione).

## 3. Tabella di parità (manuale v3.60) — stato di oggi

### 3.1 Lista delle relazioni, tipi, proprietà (5-1…5-4, 5-13…5-20)

| # | Funzione (pagina) | Oggi | Dove / note | Test oggi | v1.18.0 |
|---|---|---|---|---|---|
| G01 | Lista Y1…Y20, colore della riga = colore del grafico (5-1) | TESTED ON SIMULATOR | `GraphApp::drawList`, `MaxGraphFunctions` | graph_app_test, scr. 20 | — |
| G02 | DRAW con F6 **o EXE** dalla lista; **→** per modificare (5-2, 5-17, 5-18) | **Errore** | EXE apre l'editor (`listKey`): sul manuale EXE disegna e → modifica | — | EXE disegna, → modifica (script del simulatore adattati, schermate identiche) |
| G03 | G⇔T (SHIFT F6) tra lista e grafico; **AC** dal grafico torna alla lista (5-2) | PARZIALE | G⇔T sì; AC non gestito sul grafico | graph_app_test (G⇔T) | AC = ritorno alla lista |
| G04 | TYPE: Y=, r=, Param, X=, Y>, Y<, Y≥, Y≤, X>, X<, X≥, X≤ (5-3, 5-13) | TESTED ON SIMULATOR | menu TYPE | graph_app_test, scr. 20/22/23 | — |
| G05 | CONVERT solo tra rettangolari e disequazioni (5-17) | TESTED ON HOST | `rewriteIdentifiers` | graph_app_test | — |
| G06 | TOOL › STYLE: Normal, Thick, Broken, Dot, Thin (5-3, 5-17) | TESTED ON SIMULATOR | | graph_app_test, scr. 20 | — |
| G07 | Ineq Type Intsect / Union (5-3) | TESTED ON SIMULATOR | | scr. 23/34 | — |
| G08 | Suffissi d'angolo °, ʳ, ᵍ nelle funzioni (5-4, esempio 2: `sin xʳ` con Deg) | IMPLEMENTED (motore) / **NOT STARTED (inserimento)** | il motore li valuta (sonda §4); l'editor di Graph e quello di Table non hanno OPTN › ANGLE | — | OPTN › ANGLE (°, ʳ, ᵍ) negli editor di Graph e Table; test su grafico, Trace, G-Solve, tabella, `.pcgraph` |
| G09 | Funzione di un tipo diverso su una riga occupata: errore (5-13) | TESTED ON HOST | `storeEdit` | graph_app_test | — |
| G10 | Parametriche Xt/Yt; DELETE della coppia (5-14, 5-18) | TESTED ON HOST | | graph_app_test | — |
| G11 | Funzioni composte, al più 5 livelli (5-14) | TESTED ON HOST | `FunctionResolver` | graph_engine_test | — |
| G12 | FORMAT dalla lista: Line Style, Line Color, scelta anche con i tasti numerici (5-15, 5-16) | TESTED ON HOST | dialogo FORMAT; tasti numerici da verificare | graph_app_test | test dei tasti numerici |
| G13 | FORMAT dal grafico: grafico lampeggiante, ↑/↓, EXE; con un solo grafico direttamente (5-16) | TESTED ON HOST | `Action::Format` | graph_app_test | — |
| G14 | Grafici incollati: FORMAT **solo dal grafico**; grafici di sovrapposizione e di lista: **nessun** cambio di colore/stile (5-28, 5-30, 5-31) | **Errore** | `startSelect(Format)` esclude gli incollati (non si possono cambiare) e ammette le varianti k > 0 delle sovrapposizioni | — | stile/colore per grafico incollato; varianti escluse |
| G15 | Modifica (→) e cancellazione (F2 o DEL, Sì/No) (5-17, 5-18) | TESTED ON HOST | | graph_app_test | — |
| G16 | SELECT disegno/non disegno, «=» evidenziato (5-18) | TESTED ON HOST | | graph_app_test | — |
| G17 | Grid On / Line (spariscono con Xscale/Yscale = 0 o troppo fitte), Axes On/Off/Scale, Label (5-18, 5-19) | TESTED ON SIMULATOR | `drawLayers` | graph_app_test, scr. 24/26 | — |
| G18 | Graph Memory STORE/RECALL 1–20 (funzioni, tipi, stili, colori, selezione, una V-Window; il richiamo sostituisce la lista; memoria piena → errore) (5-19, 5-20) | TESTED ON HOST | `PCGMEM 1` | graph_app_test, graph_file_test | — |

### 3.2 V-Window, zoom, pan, sfondo (5-5…5-12)

| # | Funzione (pagina) | Oggi | Dove / note | Test oggi | v1.18.0 |
|---|---|---|---|---|---|
| G19 | V-Window: Xmin, Xmax, Xscale, Xdot, Ymin, Ymax, Yscale, Tθmin, Tθmax, Tθptch; Xdot ↔ Xmax; espressioni (5-5, 5-6) | TESTED ON SIMULATOR | | graph_app_test, scr. 29 | — |
| G20 | INITIAL / TRIG / STANDRD (5-5) | TESTED ON HOST | | graph_engine_test, graph_app_test | — |
| G21 | V-MEM STORE/RECALL 1–6 (5-6, 5-7) | TESTED ON HOST | | graph_app_test | — |
| G22 | SQUARE: Y-BASE / X-BASE (5-5) | TESTED ON HOST | | graph_app_test | — |
| G23 | BGV-WIN nella V-Window, solo con uno sfondo aperto (tutto tranne T) (5-5, 5-11) | TESTED ON HOST | | graph_app_test | — |
| G24 | Precauzioni: Tθptch = 0, input non valido, Tθmax < Tθmin con passo negativo, asse fuori schermo indicato sul bordo, intervallo ±9,999999999E97 (5-6) | TESTED ON HOST | `validateView` | graph_app_test | — |
| G25 | Intervallo `,[a,b]` per tutti i tipi (5-7) | TESTED ON HOST | `splitFunctionText` | graph_engine_test | — |
| G26 | ZOOM BOX (due punti non allineati) (5-8, 5-9) | TESTED ON HOST | | graph_app_test | — |
| G27 | ZOOM FACTOR, IN, OUT sul puntatore (5-8) | TESTED ON HOST | | graph_app_test | — |
| G28 | ZOOM AUTO (5-8) | TESTED ON HOST | | graph_engine_test | — |
| G29 | ZOOM ORIGINAL, SQUARE, ROUND, INTEGER, PREVIOUS (5-8) | TESTED ON HOST | | graph_app_test | — |
| G30 | Zoom con + e − sul centro, fattori di FACTOR (5-9) | TESTED ON HOST | | graph_app_test | — |
| G31 | PAN (OPTN): afferra/rilascia con EXE, cursori, EXIT; anche in **Table** (grafico di GPH-CON); **non** in G+G e GtoT (5-9, 5-10) | PARZIALE | Graph sì; Table e Dual Screen non provati/non gestiti | graph_app_test | test in Table; PAN rifiutato in G+G/GtoT |
| G32 | Scorrimento del grafico con i tasti cursore | IMPLEMENTED | `plotKey` (1/8 della finestra) | — | test |
| G33 | Background None / PICT n / OPEN, conferma «V-Window dello sfondo?» Sì/No (tutto tranne T) (5-10, 5-11) | PARZIALE | OPEN elenca solo `PICOCALC/PICT`, senza scelta della cartella | graph_app_test | cartelle (G38) |
| G34 | BGV-WIN SAVE («aggiornare la V-Window?») e SAVE•AS (cartella, nome ≤ 8, diventa lo sfondo) (5-11, 5-12) | PARZIALE | cartella fissa | graph_app_test | cartelle (G38) |
| G35 | Fade I/O: **cursore** con ←/→ a passi del 5 %, valore digitato, **salvato con l'immagine di sfondo** (5-12) | PARZIALE | finestra numerica; il valore va nel file solo con BGV-WIN SAVE | graph_app_test | cursore a passi del 5 %; salvataggio nel file dello sfondo |

### 3.3 Immagini (5-20…5-22)

| # | Funzione (pagina) | Oggi | Dove / note | Test oggi | v1.18.0 |
|---|---|---|---|---|---|
| G36 | Contenuto del file: **immagine del grafico e immagine dello sfondo separate** (assi, griglia, etichette, sfondo con Fade), V-Window senza T (5-20) | PARZIALE (dichiarata) | `.pcpict` a un solo livello (RLE + V-Window + Fade, CRC) | graph_file_test | **domanda D3** (§13) |
| G37 | PICTURE › STORE in Pict 1–20 (sostituisce) (5-21) | TESTED ON HOST | `PICOCALC/PICT/PICTnn.pcpict` | graph_app_test | — |
| G38 | SAVE•AS / OPEN: **scelta della cartella** (radice o cartella), nome ≤ 8 (5-21, 5-22) | PARZIALE | cartella fissa `PICOCALC/PICT` | graph_app_test | cartelle di primo livello sotto `PICOCALC/PICT` (come la memoria di massa della fx-CG50: radice + cartelle) nello stesso elenco dei file di GRAPH |
| G39 | Uno schermo diviso **non** si salva in Pict (5-21) | NOT STARTED | dipende dal Dual Screen | — | errore con G+G/GtoT/T+G |
| G40 | RECALL Pict 1–20 / OPEN: immagine dietro i grafici e sopra lo sfondo; Cls la cancella (5-22) | TESTED ON HOST | | graph_app_test | — |

### 3.4 Dual Screen G+G (5-23, 5-24)

| # | Funzione (pagina) | Oggi | v1.18.0 |
|---|---|---|---|
| G41 | Setup «Dual Screen» = G+G: schermo principale e sotto-schermo (5-23) | NOT STARTED | §6 (disposizione: **domanda D1**) |
| G42 | V-Window del sotto-schermo: F6 RIGHT / F6 LEFT nella V-Window (5-23) | NOT STARTED | seconda V-Window (salvata) |
| G43 | OPTN › COPY: copia il grafico principale nel sotto-schermo (5-23) | NOT STARTED | |
| G44 | OPTN › SWAP: scambia i contenuti dei due schermi (5-23) | NOT STARTED | |
| G45 | Indicatori nella lista: grafico nel sotto-schermo / su entrambi; il disegno li rispetta; SELECT li cancella (5-23, 5-24) | NOT STARTED | |
| G46 | Proprietà solo per il grafico principale; su «entrambi» valgono per i due; non modificabili per «solo sotto-schermo» (5-24) | NOT STARTED | |
| G47 | Trace, Zoom, Sketch, G-Solve con G+G; il sotto-schermo «prodotto copiando **o con lo zoom** del principale» (5-23) | NOT STARTED | **domanda D2** |
| G48 | AC dal grafico doppio torna alla lista (5-24) | NOT STARTED | con G03 |

### 3.5 Disegno manuale (5-25…5-31)

| # | Funzione (pagina) | Oggi | Dove / note | Test oggi | v1.18.0 |
|---|---|---|---|---|---|
| G49 | Comandi grafici di Run-Matrix in Line: SKETCH › GRAPH con Y=, r=, Param, X=, G∫dx e le disequazioni; Param con Func Type; G∫dx con valore in basso a sinistra e area (5-25…5-27) | TESTED ON SIMULATOR | `graph_commands.cpp` (v1.11.0–v1.13.0) | graph_cmd_v112_test, run_matrix_v111_test, scr. 61/85/86 | — (voce che GRAPH_PARITY contava ancora come mancante) |
| G50 | Grafici delle **funzioni predefinite** senza variabile (`Graph Y=sin`, cos, tan, inverse, iperboliche, √, x², log, ln, 10^, e^, x⁻¹, ∛, d/dx, d²/dx², ∫dx; anche in r= con θ) (5-26) | NOT STARTED | `Graph Y=sin` dà Syntax ERROR | — | il comando completa la variabile (x o θ) solo quando l'espressione è una funzione predefinita sola |
| G51 | Sovrapposizione `[A=…]`: blu, rosso, verde, magenta, nero; **ciano o giallo → colore predefinito della riga, anche per il primo grafico**; una sola variabile, non X, Y, r, θ, T; Simul Graph (5-27, 5-28) | **Errore** | il primo grafico resta ciano/giallo (`graph_engine.cpp`); nomi vietati da verificare | graph_engine_test | correzione + test dei nomi vietati |
| G52 | Grafici da lista: liste della stessa lunghezza (altrimenti Dimension ERROR), colori come sopra (5-29, 5-30) | TESTED ON HOST | (GRAPH_PARITY diceva «lunghezze diverse gestite»: il codice dà già Dimension ERROR) | graph_engine_test | test del colore ciano/giallo |
| G53 | Copia e incolla: «Y=espr.» e «espr.», spazi iniziali ignorati, blu e Normal, solo con Dual Screen Off, 30 grafici per Trace, titolo «Y=…», ridisegnati finché non si fa Cls (5-30, 5-31) | PARZIALE | `pasteGraph`; il vincolo del Dual Screen non esiste ancora | graph_app_test | errore con Dual Screen ≠ Off |

### 3.6 Modify (5-38…5-41)

| # | Funzione (pagina) | Oggi | Test oggi | v1.18.0 |
|---|---|---|---|---|
| G54 | MODIFY (F5): una funzione con 1–5 variabili (altrimenti errore), le altre senza variabili disegnate insieme; variabili e Step in basso a sinistra, magenta la scelta; ←/→ di Step; valore digitato; EXIT (5-38, 5-39) | TESTED ON SIMULATOR | graph_app_test, scr. 28 | — |
| G55 | BUILT-IN (TOOL) come Dyna Graph (5-39) | TESTED ON HOST | graph_app_test | — |
| G56 | OPTN › COPY nella lista: riga scelta, sovrascrittura, «Expression in use» (5-41) | TESTED ON HOST | graph_app_test | — |
| G57 | MODIFY richiede Dual Screen = Off (5-39) | NOT STARTED | — | errore con Dual Screen ≠ Off |

### 3.7 Sketch (5-52, 5-53)

| # | Funzione (pagina) | Oggi | Dove / note | Test oggi | v1.18.0 |
|---|---|---|---|---|---|
| G58 | Sketch Line e Plot/LineCol del Setup come valori iniziali (5-52) | TESTED ON HOST | | graph_app_test | — |
| G59 | Cls, Plot/PlotOn/PlotOff/PlotChg, Line, F-Line, Circle, Vertical, Horz, PEN, Text (5-52) | TESTED ON SIMULATOR | | graph_app_test, scr. 27 | — |
| G60 | Inverse disegnato subito, sempre con Sketch Line e Plot/LineCol (5-53) | TESTED ON HOST | `selected(Inverse)` | graph_app_test | — |
| G61 | Tangent / Norm con Sketch Line e Plot/LineCol; tangenti una dopo l'altra (5-53) | **Errore** | `traceKey`: l'elemento resta blu e Normal (stile e colore del Setup ignorati) | — | correzione + test |
| G62 | **FORMAT durante l'operazione**: Line Style e Line Color per Tangent, Norm, Line, F-Line, Circle, Vertical, Horz, PEN; colore per Plot, PlotOn, PlotChg, Text (5-52) | PARZIALE | si cambia solo dopo, da SKETCH › Elementi | graph_app_test | OPTN › FORMAT (o SHIFT 5 se mappato) durante l'operazione, valido per gli elementi che seguono nella stessa operazione |
| G63 | Funzioni a due punti: secondo punto dopo EXE (5-53) | TESTED ON HOST | | graph_app_test | — |

### 3.8 Analisi (5-54…5-61)

| # | Funzione (pagina) | Oggi | Dove / note | Test oggi | v1.18.0 |
|---|---|---|---|---|---|
| G64 | TRACE: puntatore al centro, ←/→, ↑/↓ tra i grafici alla stessa x, uscita con SHIFT F1 (5-54) | TESTED ON SIMULATOR | | graph_app_test, scr. 24 | — |
| G65 | Coordinate **in basso e accanto al puntatore**; riga «[EXE]: mostra le coordinate» (5-54, schermate di 5-54/5-55) | PARZIALE | solo la riga in basso | — | etichetta accanto al puntatore (a destra o a sinistra), riga di aiuto in alto |
| G66 | Linee ausiliarie verso gli assi; SHIFT ↓ le nasconde, SHIFT ↑ le mostra (5-54) | IMPLEMENTED | `traceLines_` | (solo scr. 24) | test |
| G67 | Valore digitato: **tasto X,θ,T** o una cifra apre la finestra (5-54) | PARZIALE | solo cifre, `-`, `.`, `(` | graph_app_test | tasto della variabile |
| G68 | Puntatore invisibile fuori dall'area o senza valore; Coord Off (5-54) | TESTED ON HOST | | graph_app_test | — |
| G69 | Formato per tipo: r=/θ=; T= poi X=/Y=; disequazioni ≥ ≤ con «=», **> < con «:»** (5-54) | PARZIALE | «=» anche per > e < | — | «:» per le disequazioni strette |
| G70 | EXE: punto con etichetta (Trace, G-Solve…), pieno se incluso, vuoto se no; ALPHA DEL toglie l'ultimo (5-55) | TESTED ON SIMULATOR | DEL della PicoCalc | scr. 24 | — |
| G71 | Derivative On: derivata in Trace (5-55) | TESTED ON SIMULATOR | | scr. 24 | — |
| G72 | **Graph to Table** (GtoT): Trace + EXE memorizza le coordinate nella tabella e mette il punto; OPTN › CHANGE attiva la tabella; con la tabella attiva R•DEL e DEL•ALL; Derivative nella tabella (5-55, 1-35) | NOT STARTED | | — | §6 |
| G73 | ZOOM ROUND e coordinate arrotondate (5-56) | TESTED ON HOST | | graph_engine_test | — |
| G74 | G-SOLVE ROOT (lampeggio, ↑/↓, EXE, ←/→ valori successivi/precedenti, derivata con Derivative On) (5-56, 5-57) | TESTED ON SIMULATOR | | graph_solve_test, scr. 25 | — |
| G75 | MAX, MIN, Y-ICEPT (5-56) | TESTED ON SIMULATOR | | scr. 06 | — |
| G76 | INTSECT: due grafici (scelta se ≥ 3), solo Y= e disequazioni Y (5-57, 5-58) | TESTED ON HOST | | graph_app_test | — |
| G77 | Y-CAL / X-CAL, più risultati, X-CAL non per le parametriche (5-58) | TESTED ON HOST | | graph_app_test | — |
| G78 | ∫dx: limiti con il puntatore o digitati, valore che segue il puntatore, EXE riempie, solo rettangolari (5-59) | TESTED ON SIMULATOR | | scr. 07 | — |
| G79 | ∫dx ROOT: dalla radice più a sinistra, Not Found, errore con ≥ 21 radici (5-59, 5-60) | TESTED ON SIMULATOR | | scr. 26 | test del limite delle 21 radici |
| G80 | ∫dx INTSECT: idem con le intersezioni (5-60) | TESTED ON HOST | | graph_app_test | test del limite |
| G81 | ∫dx MIXED: scelta dei grafici, x qualsiasi dai tasti numerici (5-61) | TESTED ON HOST | | graph_app_test | — |
| G82 | Precauzioni: tangenze e flessi (5-56, 5-58) | TESTED ON HOST | | graph_solve_test | — |
| G83 | G-Solve solo per Y= e disequazioni Y; «Not Found» | TESTED ON HOST | | graph_app_test | — |

### 3.9 Table (5-1, 5-9, 5-32…5-37)

| # | Funzione (pagina) | Oggi | Dove / note | Test oggi | v1.18.0 |
|---|---|---|---|---|---|
| T01 | Lista delle relazioni di Table (stesse funzioni e colori di Graph) (5-1, 5-32) | TESTED ON SIMULATOR | | table_test, scr. 35 | — |
| T02 | Variable: range (SET Start/End/Step) o LIST (Setup › Variable › LIST n) (5-32, 5-33) | TESTED ON SIMULATOR | | table_test, scr. 36 | — |
| T03 | Colore dei caratteri della tabella dal dialogo FORMAT della lista, come in Graph (5-33); STYLE = stile della linea | **Errore** | F4 STYLE cambia il colore; non c'è FORMAT né lo stile della linea dei grafici GPH-CON | table_test | FORMAT (Line Style, Line Color) nell'OPTN della lista; F4 STYLE = stili (etichette F1–F6 invariate) |
| T04 | SELECT («=» evidenziato), TABLE genera (5-33) | TESTED ON SIMULATOR | | table_test | — |
| T05 | Celle di 6 cifre col segno; valore completo della cella (5-33) | TESTED ON SIMULATOR | | scr. 36 | — |
| T06 | Derivative On: colonne delle derivate, «dY/dX» sulla cella della derivata, errore con intervallo o sovrapposizione (5-34) | TESTED ON SIMULATOR | «dY/dX=» nella riga del valore in basso (sulla PicoCalc la riga in alto è la barra di stato) | scr. 32 | — (differenza di posizione documentata) |
| T07 | TYPE Y=/r=/Param, un solo tipo per tabella (5-34) | TESTED ON HOST | | table_test | — |
| T08 | FORMULA, DELETE, ROW DELETE/INSERT/ADD, EDIT; valore non valido → errore e valore originale; colonne non x non modificabili (5-34) | TESTED ON HOST | INSERT/ADD copiano la riga: il manuale non dice quale valore prende la riga nuova (§4, voce 8) | table_test | la voce passa da «differenza dichiarata» a scelta documentata |
| T09 | LISTMEM (OPTN): colonna del cursore in List n (5-35) | PARZIALE sul Pico 2 W | 999 sulla Plus, 255 sul Pico 2 W | table_test | **domanda D4** |
| T10 | GPH-CON / GPH-PLT con la V-Window; G⇔T **o AC** tornano alla tabella (5-35) | PARZIALE | AC non gestito | table_test, scr. 33 | AC |
| T11 | Dopo GPH-CON: Trace, Zoom, Sketch, FORMAT dal grafico (5-36) | PARZIALE / **Errore** | il Trace avanza di Step tra Start ed End invece di seguire le righe della tabella (Variable = LIST o righe modificate) | graph_app_test | Trace sulle righe; test di Zoom, Sketch, FORMAT, PAN dopo GPH-CON |
| T12 | **T+G**: Setup «Dual Screen» = T+G in Table; tabella nel sotto-schermo, grafico GPH-CON/GPH-PLT nel principale; OPTN › CHANGE o AC rendono attiva la tabella (5-36, 5-37) | NOT STARTED | | — | §6 |
| T13 | Il Dual Screen di Table vale anche in Recursion (5-37) | FUORI AMBITO | Recursion è una modalità Base | — | nota |

### 3.10 Setup (1-35…1-37)

| # | Voce | Oggi | v1.18.0 |
|---|---|---|---|
| S01 | Func Type (12 tipi; cambia il tasto X,θ,T) | TESTED ON HOST | — |
| S02 | Draw Type Connect/Plot | TESTED ON HOST | — |
| S03 | Derivative: Trace, **Graph-to-Table**, **Table & Graph** | PARZIALE | colonne delle derivate in GtoT e T+G |
| S04 | Angle | TESTED ON HOST | — |
| S05 | Coord | TESTED ON HOST | — |
| S06 | Grid On/Off/Line | TESTED ON HOST | — |
| S07 | Axes On/Off/Scale | TESTED ON SIMULATOR | — |
| S08 | Label | TESTED ON SIMULATOR | — |
| S09 | Graph Func | TESTED ON HOST | — |
| S10 | **Dual Screen G+G / GtoT / Off** (Graph) | NOT STARTED | voce nuova del Setup di Graph |
| S11 | Simul Graph | TESTED ON HOST | — |
| S12 | Background None / PICT n / OPEN | TESTED ON HOST | cartelle (G38) |
| S13 | Plot/LineCol | TESTED ON HOST | applicato anche a Tangent/Norm (G61) |
| S14 | Sketch Line | TESTED ON HOST | idem |
| S15 | Ineq Type | TESTED ON SIMULATOR | — |
| S16 | Variable RANG/LIST (Table) | TESTED ON HOST | — |
| S17 | **Dual Screen T+G / Off** (Table) | NOT STARTED | voce nuova del Setup di Table |
| S18 | SET UP dentro Graph/Table apre il Setup della modalità | TESTED ON HOST | voci nuove in fondo: lo screenshot 30 resta identico (§6.4) |
| — | Input/Output, Frac Result, Complex Mode, Display | globali (SYSTEM) | — |

### 3.11 Estensioni del progetto (non contano per la parità)

Progetti `.pcgraph`, FILE NEW/OPEN/SAVE/SAVE AS/RENAME/DELETE, scrittura
atomica e recupero, microSD e LittleFS, esportazione BMP, ZOOM TRIG/STANDRD/
INITIAL dal grafico, SKETCH › Elementi, campionamento adattivo con poli e
domini, clipping, disegno incrementale, budget dei punti per scheda, layer in
PSRAM, OPTN › → TABLE, persistenza della tabella nel workspace, comandi grafici
nei `.pcgraph`: tutte TESTED ON HOST/SIMULATOR come nella v1.17.0; restano
nella sezione «Estensioni» di GRAPH_PARITY.md.

### 3.12 Conteggio di oggi

| Stato (oggi) | Graph (G01–G83) | Table (T01–T13) | Setup (S01–S18) | Totale |
|---|---:|---:|---:|---:|
| TESTED ON SIMULATOR | 16 | 5 | 3 | 24 |
| TESTED ON HOST | 36 | 2 | 12 | 50 |
| IMPLEMENTED (senza test) | 2 | 0 | 0 | 2 |
| PARZIALE | 13 | 2 | 1 | 16 |
| Errore (presente ma diverso dal manuale, §5) | 4 | 2 | 0 | 6 |
| NOT STARTED | 12 | 1 | 2 | 15 |
| FUORI AMBITO | 0 | 1 | 0 | 1 |
| **Totale** | **83** | **13** | **18** | **114** |

Una riga = una funzione del manuale: G08 conta come PARZIALE (motore sì,
inserimento no), T11 come errore (E4), G41–G48 come otto voci del Dual Screen.

**Da chiudere nella v1.18.0: 39 voci** (16 parziali, 6 errori, 15 non
iniziate, 2 senza test; G36 dipende dalla domanda D3, T09 dalla D4), più i
test mancanti delle voci presenti indicati nella colonna «v1.18.0» (G12, G52,
G79, G80).

## 4. Le nove voci aperte note, verificate

| # | Voce | Verifica | Esito |
|---|---|---|---|
| 1 | Dual Screen G+G (due grafici, COPY, SWAP, sotto-schermo) | nessun codice in Graph; `graph2d::Layout` esiste solo sulla Plus e non è usato da Graph | **NOT STARTED** (G41–G48); progetto in §6 |
| 2 | GtoT e Graph to Table | nessun codice | **NOT STARTED** (G72) |
| 3 | T+G in Table | nessun codice; GPH-CON apre GRAPH a schermo pieno (`ws.table.draw`) | **NOT STARTED** (T12) |
| 4 | Voce di Setup «Dual Screen» e interazioni | nessuna voce; dal manuale: PAN non in G+G/GtoT (5-9), incolla solo con Off (5-31), Modify con Off (5-39), schermo diviso non salvabile in Pict (5-21), FORMAT solo a sinistra (5-24), Derivative anche in GtoT e T+G (1-35), voce di Table T+G valida anche per Recursion (5-37) | **NOT STARTED** (S10, S17, G31, G39, G53, G57); interazioni in §6.4 |
| 5 | FORMAT durante lo SKETCH | si cambia solo dopo; **trovato in più** che Tangent/Norm ignorano Sketch Line e Plot/LineCol | **PARZIALE** (G62) + **errore** (G61) |
| 6 | Suffissi °, ʳ, ᵍ in Graph/Table | sonda: con Angle = Deg `sin(xʳ)` a x = π/2 vale 1; `sin(50ᵍ+x°)` e `3sin(2θʳ)` (r=) corretti; cella di Table corretta; **nessun modo di inserirli** dagli editor di Graph e Table (manca OPTN › ANGLE) | motore OK senza test; inserimento **NOT STARTED** (G08) |
| 7 | Voci IMPLEMENTED senza test | scorrimento con i cursori (G32), linee ausiliarie del TRACE (G66): nessun test le prova | test nuovi |
| 8 | Immagine a due livelli; INSERT/ADD in Table | **immagine**: realizzabile (formato proprio `.pcpict` v2 con due livelli), non è una differenza inevitabile → **domanda D3**. **INSERT/ADD**: il manuale dice solo «insert/add row»; il valore della riga nuova non è documentato, quindi la copia della riga non contraddice il manuale | INSERT/ADD: la voce diventa presente e testata, con la scelta documentata (nessuna differenza) |
| 9 | LISTMEM a 999 sul Pico 2 W | `kListMaxLength` = 255 sul Pico 2 W ma il totale di tutte le liste (`kListStoreElements`) resta 1 530 elementi (12 KB, record da 59,75 KB); la lunghezza nel record è un `u16`. Portare il limite per lista a 999 **non costa RAM né spazio nel record** (il totale non cambia) e quasi nulla in flash; però il limite è dei dati condivisi `Lists` (vale anche per Statistics e Run-Matrix del Pico 2 W) e un firmware ≤ v1.17.0 rifiuta un record con una lista di più di 255 elementi | **domanda D4** |

Voci trovate in più nell'audit (non nell'elenco iniziale): G02, G03/G48/T10
(AC), G14, G33/G34/G38 (cartelle), G35 (cursore di Fade I/O), G50 (funzioni
predefinite), G51 (colore ciano/giallo), G61 (Tangent/Norm), G65 (etichetta del
puntatore), G67 (tasto X,θ,T), G69 («:»), T03 (FORMAT e STYLE di Table), T11
(Trace sulle righe della tabella).

## 5. Errori trovati (ognuno avrà un test che fallisce prima della correzione)

| # | Errore | Dove | Correzione |
|---|---|---|---|
| E1 | Tangent e Norm disegnati sempre blu e Normal (Plot/LineCol e Sketch Line ignorati) | `GraphApp::traceKey` (EXE con Tangent/Normal) | colore e stile dal Setup (e da FORMAT, G62) |
| E2 | Sovrapposizione o lista con colore ciano/giallo: il primo grafico resta ciano/giallo | `GraphEngine::build` | il primo grafico prende il colore predefinito della riga |
| E3 | FORMAT dal grafico: i grafici incollati non si possono scegliere, le varianti delle sovrapposizioni sì | `startSelect(Action::Format)` | stile/colore propri dei grafici incollati; varianti escluse |
| E4 | Trace dopo GPH-CON con Variable = LIST o righe modificate: il puntatore segue Start/Step/End | `traceMove`, `startTrace` | il puntatore passa da una riga della tabella all'altra |
| E5 | EXE sulla lista delle relazioni di Graph apre l'editor invece di disegnare | `GraphApp::listKey` | EXE disegna, → modifica |
| E6 | Table: F4 STYLE cambia il colore (non lo stile); manca FORMAT | `TableApp` | STYLE = stili, FORMAT = stile + colore |

## 6. Dual Screen su uno schermo 320×320

### 6.1 Il problema

La fx-CG50 ha uno schermo largo (384×216) e divide **in verticale**: due
riquadri quasi quadrati (circa 190×170 punti), principale a sinistra,
sotto-schermo (o tabella) a destra. La PicoCalc ha 320×320 con la barra di
stato (18 px) e la barra F1–F6 (20 px): l'area del grafico è 320×282.

| Divisione | Riquadro | Rapporto L:A | V-Window INITIAL (12,6 × 6,2) | Tabella |
|---|---|---|---|---|
| **Verticale** come la fx-CG50 | 159×282 | 0,56 | Xdot 0,080, Ydot 0,022: punti 3,6 volte più alti che larghi, curve schiacciate in orizzontale | 159 px: X + una colonna (6 caratteri a 8 px), come la fx-CG50 |
| **Orizzontale** (principale sopra, sotto-schermo sotto) | 320×140 | 2,29 | Xdot 0,040, Ydot 0,045: punti quasi quadrati (come la fx-CG50 a schermo pieno) | 320 px: X + 3 colonne, 7 righe |

### 6.2 Proposta (raccomandata): divisione orizzontale

Principale **sopra** (righe 18…157), linea di separazione di 2 px, sotto-schermo
o tabella **sotto** (righe 160…299); ognuno 320×140. Motivi: la risoluzione
orizzontale resta quella del grafico a schermo pieno (320 colonne), le V-Window
abituali danno punti quasi quadrati, la tabella ha spazio per X e tre colonne.
Le funzioni sono le stesse della fx-CG50; cambiano solo le parole «sinistra/
destra» → «sopra/sotto»: F6 RIGHT/LEFT della V-Window diventano **F6 SOTTO /
SOPRA**, gli indicatori della lista sono **S** (solo sotto-schermo) ed **E**
(entrambi). Differenza documentata (disposizione dello schermo). Alternativa:
divisione verticale identica alla fx-CG50 (domanda **D1**).

### 6.3 Comportamento (dal manuale; dove il manuale tace, la scelta è scritta)

**G+G** (Graph, Setup Dual Screen = G+G)
- Il principale disegna le funzioni selezionate senza indicatore e quelle con
  «E»; il sotto-schermo quelle con «S» o «E», ognuno con la sua V-Window
  (V-Window del sotto-schermo salvata nel workspace e nel `.pcgraph`).
- OPTN › COPY: le funzioni del principale diventano «E» e la V-Window del
  sotto-schermo prende quella del principale; OPTN › SWAP: scambia
  indicatori (nessuno ↔ S, E resta) e le due V-Window.
- SELECT su una riga con indicatore lo cancella (la funzione torna solo nel
  principale); FORMAT solo sui grafici del principale; su «E» vale per i due;
  su «S» rifiutato.
- Trace, Sketch, G-Solve, Modify… agiscono sul **principale**; ZOOM: domanda
  **D2**. PAN, incolla, Modify, PICTURE STORE: errore «non disponibile con il
  Dual Screen» (5-9, 5-31, 5-39, 5-21).
- AC o EXIT: lista; G⇔T: lista.

**GtoT** (Graph, Dual Screen = GtoT)
- Grafico nel principale, tabella nel sotto-schermo (colonna della variabile
  e una colonna per funzione selezionata dello stesso tipo, con le derivate se
  Derivative = On, celle di 6 caratteri, valore completo della cella in basso).
- Trace: EXE aggiunge la riga (valore della variabile del punto) e il punto
  con l'etichetta; OPTN › CHANGE (o AC) attiva la tabella: cursore, R•DEL (F1)
  e DEL•ALL (F2), OPTN › LISTMEM; CHANGE torna al grafico.
- La tabella del GtoT è **separata** da quella di Table (non la sovrascrive),
  al più 999 righe come Table, salvata nella sezione di Graph (§8). Il tipo
  della tabella è quello del grafico del primo punto (Y=, r=, Param); un punto
  di un grafico di tipo diverso dà «Argument ERROR» finché la tabella non è
  vuotata con DEL•ALL.

**T+G** (Table, Setup Dual Screen = T+G)
- La schermata della tabella è divisa: grafico (vuoto finché non si fa
  GPH-CON/GPH-PLT) nel principale, tabella nel sotto-schermo con tutte le
  funzioni della tabella (cursore, EDIT, ROW, DELETE, LISTMEM, FORMULA).
- GPH-CON/GPH-PLT disegnano nel principale e rendono **attivo il grafico**
  (Trace sulle righe, Zoom, Sketch, FORMAT); OPTN › CHANGE o AC tornano alla
  tabella attiva, con il grafico che resta disegnato.

### 6.4 Interazioni del Setup «Dual Screen»

| Voce / funzione | Off | G+G | GtoT | T+G (Table) |
|---|---|---|---|---|
| Disegno, Trace, Sketch, G-Solve | schermo pieno | principale (D2 per lo ZOOM) | principale | principale (grafico dei punti della tabella) |
| PAN (5-9) | sì | no | no | sì (Table, 5-9) |
| Incolla (5-31) | sì | no | no | — |
| Modify (5-39) | sì | no | no | — |
| Overwrite / List Graph | sì | sì (in ciascun riquadro) | sì | — |
| PICTURE STORE (5-21) | sì | no | no | no |
| Background | sì | non mostrato (l'immagine è 320×282) | non mostrato | non mostrato |
| FORMAT dal grafico (5-24) | tutti | solo principale | principale | principale |
| Derivative (1-35) | Trace | Trace | Trace e tabella | tabella |
| Coord, Grid, Axes, Label, Graph Func, Simul Graph | schermo | ogni riquadro | riquadro del grafico | riquadro del grafico |
| Budget dei punti delle curve | intero | diviso tra i due riquadri | intero al grafico | intero al grafico |

Le voci di Setup già esistenti restano nello stesso ordine; «Dual Screen» è
aggiunta **in fondo** al Setup di Graph e di Table, così lo screenshot 30 (Setup
di Graph, prima pagina) resta identico.

## 7. Architettura: un solo motore per le due schede

- **Non sopra Graph2D** (solo Plus): il Dual Screen va nel servizio
  **GraphCore**, compilato su entrambe le schede. Oggi `graph_plot.cpp` usa le
  costanti `graphgeo::Left/Top/Columns/Rows`; diventano la geometria del
  **riquadro** (`graph::Viewport` accetta già left/top/columns/rows).
- Nuovo modulo condiviso `include/graph_view.hpp` + `src/graph_view.cpp`
  (servizio GraphCore, entrambe le schede): geometria dello schermo diviso,
  disegno di griglia/assi/etichette/curve/punti di un riquadro (spostati da
  `graph_plot.cpp` senza cambiare un pixel nello schermo pieno), curve dei
  punti della tabella (da `buildTableCurves`) e **riquadro della tabella**
  (celle di 6 caratteri, cursore, valore completo) usato da GraphApp (GtoT,
  grafico attivo in T+G) e da TableApp (T+G con la tabella attiva). Così
  TableApp non chiama GraphApp (regola delle modalità) e non c'è codice
  duplicato.
- GraphApp: per il G+G un secondo insieme di curve per il sotto-schermo
  (stesso motore, stessa compilazione delle funzioni), la seconda V-Window e
  gli indicatori; per GtoT la tabella dei punti.
- Sul Pico 2 W i sorgenti nuovi (`graph_view.cpp` per la parte tabella, codifiche,
  `graph_files.cpp`) vanno in `FXAI_PICO2W_SIZE_SOURCES` (`-Os`); il disegno delle
  curve resta a `-O2`. Niente `#if` di scheda nel Dual Screen: stessa
  implementazione.
- `graph2d::layout` (Plus, modalità Base) resta com'è; Recursion userà il
  Dual Screen quando arriverà alla parità.

## 8. Persistenza e compatibilità

| Dato | Dove | Compatibilità |
|---|---|---|
| Dual Screen di Graph (Off/G+G/GtoT), V-Window del sotto-schermo, indicatori S/E delle 20 funzioni, tabella del GtoT (tipo + ≤ 999 valori), stile/colore dei grafici incollati se diversi | **sezione `0x0104`** (0x0100 + AppId::Graph) del record, scritta solo se lo stato non è quello iniziale | Plus: record **v12** (CRC della sezione); **Pico 2 W: resta il record v11** (la coda TLV esiste dalla v1.15.0). Un firmware v1.15.0–v1.17.0 conserva la sezione sconosciuta intatta (`preservedSections`) e ignora il Dual Screen; nessun cambio di versione del record |
| Dual Screen di Table (Off/T+G) | **sezione `0x0106`** (AppId::Table), solo se ≠ Off | idem |
| Progetto `.pcgraph` | chiavi nuove `dual=`, `subview=`, `side=` (indicatori), `gtot=` (tabella del GtoT), `tabledual=` | le chiavi sconosciute sono ignorate dai lettori v1.8.0–v1.17.0 (`graph_file.cpp`); versione del formato invariata (`PCGRAPH 1`) |
| Strisce di eActivity (parti) | nuova parte `part::GraphDual` scritta **solo** se lo stato del Dual Screen non è quello iniziale | `decodeParts` dei firmware precedenti rifiuta una parte sconosciuta: i documenti senza Dual Screen restano leggibili; con il Dual Screen no (limite documentato, come il record v12 della v1.17.0) |
| Immagini (solo se D3 = sì) | `.pcpict` **versione 2** con due livelli; la v1 resta leggibile | un firmware ≤ v1.17.0 non legge le immagini v2 («versione futura») |
| Liste di 999 sul Pico 2 W (solo se D4 = sì) | nessun cambio di formato (`u16`) | un firmware ≤ v1.17.0 rifiuta un record del Pico 2 W che contiene una lista di più di 255 elementi |

## 9. Pico 2 W: eccezione al congelamento solo per Graph e Table

**Budget.** Leggo il vincolo come: immagine ≤ 2 064 384 B **e** almeno 48 KB
(49 152 B) di quel budget liberi per le correzioni future, cioè **immagine ≤
2 015 232 B**. Partenza 1 944 228 B: restano **71 004 B** per tutta la v1.18.0
sul Pico 2 W. RAM statica ≤ +8 KB (≤ 315 476 B con `.heap`).

**Controllo bloccante sul margine.** `cmake/fxai_board.cmake`: margine del Pico
2 W da 32 768 B a **81 920 B** (32 KiB + 48 KiB) → `check_firmware_size.cmake`
fallisce oltre 2 015 232 B; `architecture_test` controlla che il margine
dichiarato per PICO2W sia 81 920 B.

**Eccezione esplicita e verificata in `architecture_test`.**
1. L'insieme delle modalità aperte del Pico 2 W resta quello della v1.16.0,
   nessuna Base (come oggi).
2. **Manifesto del congelamento**: `tests/fixtures/pico2w_frozen_v1_17_0.txt`
   con l'impronta (FNV-1a a 64 bit, sul solo codice senza commenti) di ogni
   sorgente compilato sul Pico 2 W e di ogni header incluso, presa a `aa2923c`.
   Il test fallisce se un file del manifesto cambia, **tranne** quelli
   dell'elenco esplicito dell'eccezione v1.18.0 (`kGraphTableException`: i
   sorgenti e gli header di Graph/Table, `graph_view`, le codifiche della
   sezione e del `.pcgraph`, `pict` se D3 = sì, e le righe del registro di
   Graph/Table). Una correzione condivisa futura aggiorna il manifesto con il
   motivo scritto nel rapporto della versione.
3. Le voci del registro di Graph e Table sono le sole che possono cambiare
   `sources`/`services` sul Pico 2 W.

**Se una voce non entrasse** nel budget del Pico 2 W resterebbe sulla Plus e
il Pico 2 W la avrebbe documentata come differenza fra le schede (Graph/Table
del Pico 2 W senza parità completa). Con le stime di §10 tutte le voci entrano.

## 10. Stime di flash e RAM

Stime confrontando i moduli della mappa del linker (§1.1); saranno misurate con
`tools/map_sizes.py`.

| Blocco | Pico 2 W (B, `-Os` dove possibile) | Plus (B, `-O2`) |
|---|---:|---:|
| Riquadri in `graph_plot.cpp` (geometria a runtime al posto delle costanti) | 2 000–4 000 | 3 000–5 000 |
| G+G (seconda V-Window e form F6, COPY, SWAP, indicatori, disegno dei due riquadri, restrizioni) | 10 000–14 000 | 13 000–18 000 |
| GtoT (memorizzazione col Trace, riquadro della tabella, CHANGE, R•DEL, DEL•ALL, LISTMEM, derivate) | 6 000–9 000 | 8 000–11 000 |
| T+G (tabella divisa in Table, grafico statico dei punti, ritorno con CHANGE/AC) | 5 000–8 000 | 6 000–10 000 |
| Voci piccole (FORMAT nello Sketch, etichetta e «:» del Trace, X,θ,T, AC, EXE, ANGLE negli editor, cursore di Fade, FORMAT/STYLE di Table, funzioni predefinite di Run-Matrix, errori E1–E6) | 5 000–8 000 | 6 000–10 000 |
| Cartelle delle immagini (G38) | 2 500–4 000 | 3 000–5 000 |
| Persistenza (sezioni 0x0104/0x0106, chiavi `.pcgraph`, parte delle strisce) | 2 500–4 000 | 3 000–5 000 |
| `.pcpict` v2 a due livelli (solo se D3 = sì) | 2 000–3 500 | 2 500–4 000 |
| **Totale** | **35 000–55 000** (resta ≥ 16 000 B oltre i 48 KB) | **44 500–68 000** (≈ 2,69 MB, 64–65 % di 4 MB) |

| RAM | Pico 2 W | Plus |
|---|---|---|
| Statica | +0,5–1,5 KB (membri di GraphApp/TableApp: seconda V-Window, stato del riquadro attivo, indicatori; nessun buffer) | idem |
| Heap, solo con la modalità aperta | curve del sotto-schermo dentro il budget esistente (24 000 punti divisi fra i due riquadri, nessun aumento); tabella del GtoT ≤ 8 KB; con D3: un secondo buffer RLE solo durante STORE | curve in PSRAM (400 000 punti divisi); cache dei layer in PSRAM (stessi 180 KB, un riquadro per volta) |
| Record | sezione 0x0104 ≤ ~8,2 KB con il GtoT pieno (il record da 59,75 KB può riempirsi solo con dati estremi: «Memoria flash piena» già gestito) | ≤ ~8,2 KB su 512 KB |

## 11. Test, fuzzing, simulatore, hardware

- **Un test per voce** della tabella §3, in suite nuove `graph_v118_test` e
  `table_v118_test` (entrambi i profili), più `workspace_v118_test` (sezioni
  0x0104/0x0106, record v11 sul Pico 2 W e v12 sulla Plus, conservazione da
  parte di un lettore della v1.17.0 con la fixture `workspace_v1_16_0.inc` e
  una nuova fixture v1.17.0 per il Pico 2 W), aggiunte a `graph_file_test`
  (chiavi `.pcgraph`, `.pcpict` v2), `graph_engine_test` (E2, suffissi),
  `architecture_test` (eccezione, manifesto, margine).
- **Fuzzing ≥ 300 s** per gli harness toccati: `fuzz_ui` (tasti del Dual
  Screen), `fuzz_workspace` (sezioni nuove), `fuzz_pcgraph` (chiavi nuove,
  `.pcpict` v2), `fuzz_eactivity` (parte nuova); **nuovo `fuzz_dual`**:
  sequenze di tasti in G+G/GtoT/T+G con V-Window e tabelle casuali, verifica
  che nessuna coordinata esca dal riquadro e che lo stato si salvi e rilegga.
- **Simulatore**: `tests/sim/graph_v118.txt` con gli screenshot dal **148** (G+G,
  COPY, SWAP, V-Window del sotto-schermo, lista con S/E, GtoT con la tabella
  attiva, T+G, Trace con l'etichetta, FORMAT nello Sketch, cartelle delle
  immagini, Setup con Dual Screen); `tools/sim_compare.py` con i 137
  screenshot di oggi identici.
- **Hardware**: `docs/HARDWARE_TEST_v1.18.0.md` da **HW-246** (Dual Screen sul
  pannello reale, tempi del disegno di due riquadri sul Cortex-M33, tastiera nel
  GtoT, record v11 del Pico 2 W con la sezione nuova letto dalla v1.16.0).

## 12. Piano delle versioni

| Versione | Contenuto |
|---|---|
| **v1.18.0** | **Graph e Table** alla parità completa, su entrambe le schede (eccezione al congelamento del Pico 2 W solo per Graph/Table) |
| **v1.19.0** | **Statistics** alla parità completa (cap. 6), solo Plus; **Distribution** entra come Base |
| v1.20.0… | come la proposta della v1.17.0, spostata di una versione |

## 13. Domande prima del codice

| # | Domanda | Raccomandazione |
|---|---|---|
| D1 | Divisione dello schermo 320×320 per G+G, GtoT, T+G | **orizzontale**: principale sopra, sotto-schermo/tabella sotto (320×140 ciascuno; F6 SOTTO/SOPRA; indicatori S/E) — differenza documentata. Alternativa: verticale come la fx-CG50 (159×282) |
| D2 | G+G: quale riquadro è attivo e dove va il risultato dello ZOOM | **principale sempre attivo**; lo ZOOM calcola dalla V-Window del principale e mette il risultato nel **sotto-schermo** (il manuale: «il sotto-schermo è prodotto copiando o con lo zoom del principale»), SWAP per analizzare l'altro. Alternative: ZOOM sul principale come a schermo pieno; tasto per rendere attivo il sotto-schermo (estensione) |
| D3 | Immagine a due livelli (grafico e sfondo separati) | **realizzarla**: `.pcpict` v2 con il livello del grafico (curve, sketch, punti, immagine richiamata) e quello dello sfondo (immagine con Fade, griglia, assi, etichette); Fade I/O schiarisce solo il livello dello sfondo; RECALL mostra i due livelli; v1 ancora leggibile. Costo 2–4 KB per scheda. Alternativa: differenza documentata |
| D4 | LISTMEM a 999 sul Pico 2 W | **sì**: limite per lista 999 anche sul Pico 2 W, totale di tutte le liste invariato (1 530 elementi, 12 KB): nessun costo di RAM o record. Effetto collaterale: vale per tutte le modalità che usano le liste (sono dati condivisi) e un firmware ≤ v1.17.0 non legge un record con una lista > 255. Alternativa: 255 come differenza fra le schede, e Table del Pico 2 W senza parità completa |

Scelte che prendo se non vengono indicate altre: margine del Pico 2 W letto
come immagine ≤ 2 015 232 B (§9); tabella del GtoT separata da quella di Table
(§6.3); manifesto del congelamento in `architecture_test` (§9); EXE sulla lista
di Graph disegna (E5); voce «Dual Screen» di Table separata da quella di Graph
(due valori, come i due Setup del manuale).

### 13.1 Risposte (23/11/2026)

| # | Risposta | Conseguenza |
|---|---|---|
| D1 | «Sopra/sotto» | principale sopra, sotto-schermo o tabella sotto (320×140 ciascuno); F6 SOTTO/SOPRA nella V-Window; indicatori S/E nella lista; differenza di disposizione documentata |
| D2 | «Zoom nel sotto-schermo» | in G+G il principale è sempre attivo; lo ZOOM parte dalla V-Window del principale e scrive il risultato nella V-Window del sotto-schermo; SWAP per analizzare l'altro grafico |
| D3 | «Realizzare 2 livelli» | `.pcpict` versione 2 con il livello del grafico e quello dello sfondo; Fade I/O solo sullo sfondo; v1 ancora leggibile |
| D4 | «Sì, 999» | `kListMaxLength` = 999 anche sul Pico 2 W, totale `kListStoreElements` invariato (1 530) |

Le scelte non chieste restano quelle scritte sopra (margine del Pico 2 W:
immagine ≤ 2 015 232 B; tabella del GtoT separata; manifesto del congelamento;
EXE sulla lista di Graph disegna; due voci «Dual Screen» separate).
