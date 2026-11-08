# GRAPH — matrice di parità con la fx-CG50 (v1.8.0)

Verifica sistematica, voce per voce, delle funzioni della modalità **Graph**
della fx-CG50 rispetto al firmware PicoCalc FX-CG50 AI v1.8.0.

**Riferimento funzionale**: *fx-CG50 Software User's Guide* (versione 3.50),
capitolo 5 "Graphing" (pagine 5-1…5-63) e voci di Setup del capitolo 1
(pagine 1-35…1-37). Il manuale è usato **solo** come riferimento di
comportamento: il firmware non contiene codice, ROM, grafica, font o formati di
file Casio (le immagini usano il formato originale `.pcpict`, non `g3p`).

## Stati

| Stato | Significato |
|---|---|
| **NOT STARTED** (mancante) | nessun codice |
| **IN PROGRESS** (parziale) | codice presente ma la funzione CG50 è coperta solo in parte |
| **IMPLEMENTED** (implementata) | codice completo, senza test automatico dedicato |
| **TESTED ON HOST** (testata su host) | test automatici su PC superati (stesso sorgente del firmware) |
| **TESTED ON SIMULATOR** | in più, flusso verificato nel simulatore host con screenshot (`docs/screenshots/`) |
| **TESTED ON HARDWARE** | provata sulla PicoCalc fisica |
| **COMPLETE** | testata su hardware e completa rispetto alla CG50 |

**Nessuna voce è TESTED ON HARDWARE o COMPLETE**: in questa sessione la
PicoCalc fisica non era disponibile; tutte le prove sono state eseguite su PC
(test host, sanitizer, simulatore) e con la compilazione del firmware UF2.

## Sintesi

Conteggio delle righe delle tabelle seguenti (sezioni 1–13, Setup,
persistenza):

| Stato | Voci |
|---|---|
| TESTED ON SIMULATOR | 26 |
| TESTED ON HOST | 87 |
| IMPLEMENTED | 3 |
| IN PROGRESS (parziale) | 3 |
| NOT STARTED | 10 |
| Fuori ambito GRAPH (Conic Graphs, impostazioni globali) | 3 |
| **Totale** | **132** |

Le modalità Dyna Graph, Recursion e Conic Graphs (sezioni 9–11 del capitolo 5)
sono modalità separate e non sono contate.

**La parità completa con la fx-CG50 NON è raggiunta.** Mancano in
particolare: Dual Screen (G+G, GtoT) e Graph to Table, i comandi grafici di
Run-Matrix/programmi (`Graph Y=`, `DrawGraph`, ...), le tabelle con variabile
da lista, l'editing delle righe e LISTMEM, le tabelle di r= e parametriche,
FORMAT durante lo SKETCH, i suffissi di unità angolare (°, ʳ, ᵍ) nelle
espressioni. Elenco completo nella sezione "Mancanze".

## 0. Audit iniziale (v1.7.0, commit `72285e9`)

| Area | Stato in v1.7.0 |
|---|---|
| Struttura | un solo file `graph_app.cpp` di 516 righe; calcolo dei punti dentro l'interfaccia; nessun modulo grafico separato né testabile |
| Funzioni | 6 righe (`Y=`, `r=`, 4 disequazioni Y); nessun `X=`, nessuna parametrica, nessuna disequazione X, nessun intervallo `[a,b]`, nessuna sovrapposizione `[A=...]`, nessuna funzione composta |
| Campionamento | un punto per colonna unito al successivo: asintoti disegnati come segmenti verticali, nessuna gestione dei bordi di dominio, nessun clipping |
| V-Window | 6 campi (senza Xdot, T); nessuna memoria V-Window; INITIAL/TRIG |
| ZOOM | IN/OUT e preset; nessun BOX, FACTOR, AUTO, ORIGINAL, SQUARE, ROUND, INTEGER, PREVIOUS |
| TRACE | puntatore per colonna, senza cambio grafico per tipo, senza valore digitato |
| SKETCH | solo tangente/normale come testo calcolato, "Cancella" |
| G-SOLVE | ROOT, MAX, MIN, Y-ICEPT, ISCT, ∫dx semplice; nessun Y-CAL/X-CAL, ∫dx ROOT/INTSECT/MIXED |
| Stile | solo colore (6); nessuno stile di linea, nessun FORMAT |
| File | nessun progetto, nessuna immagine, nessuna microSD |
| Setup | solo griglia on/off |
| Test | nessun test dedicato a GRAPH (solo un flusso in `apps_flow_test`) |

## 1. Esempi e lista delle relazioni (5-1…5-4)

| Funzione CG50 | Stato | Implementazione | Verifica |
|---|---|---|---|
| Lista delle relazioni Y1…Y20 con colore e stile accanto al nome | TESTED ON SIMULATOR | `GraphApp::drawList`, 20 funzioni (`MaxGraphFunctions`) | graph_app_test, screenshot 20 |
| Inserimento e modifica in formato naturale (VPAM) | TESTED ON SIMULATOR | stesso `NaturalEditor` di Run-Matrix | graph_app_test, screenshot 21 |
| DRAW (F6) | TESTED ON SIMULATOR | rendering incrementale, tastiera attiva | screenshot 22 |
| SELECT (F1) on/off per riga | TESTED ON HOST | `GraphFunction::selected` | graph_app_test |

## 2. Controllo della schermata (5-5…5-12)

| Funzione CG50 | Stato | Implementazione | Verifica |
|---|---|---|---|
| V-Window: Xmin, Xmax, Xscale, Xdot, Ymin, Ymax, Yscale, Tθmin, Tθmax, Tθptch | TESTED ON SIMULATOR | 10 campi, Xdot ↔ Xmax collegati, espressioni ammesse, valori esatti conservati | graph_app_test, screenshot 29 |
| INITIAL / TRIG / STANDRD | TESTED ON HOST | `initialWindow`, `trigWindow`, `standardWindow` (dipendono dall'unità angolare) | graph_engine_test, graph_app_test |
| V-Window non valida (Xmax ≤ Xmin, scale negative, pitch nullo) | TESTED ON HOST | `validateView`: errore, nessuna modifica | graph_app_test |
| Tmax < Tmin con pitch negativo | TESTED ON HOST | ammesso come sulla CG50 | graph_app_test |
| V-Window Memory STORE/RECALL (6 memorie) | TESTED ON HOST | `GraphState::vmem` | graph_app_test, graph_file_test |
| SQUARE dalla V-Window | TESTED ON HOST | Y-BASE / X-BASE | graph_app_test |
| Intervallo del grafico `, [a,b]` | TESTED ON HOST | `splitFunctionText`, ammesso per tutti i tipi | graph_engine_test |
| ZOOM BOX | TESTED ON HOST | due angoli con il puntatore; errore se degeneri | graph_app_test |
| ZOOM FACTOR (Xfact, Yfact) | TESTED ON HOST | `GraphSettings::xFactor/yFactor` | graph_app_test |
| ZOOM IN / OUT (centro sul puntatore) | TESTED ON HOST | `zoomAt` | graph_app_test |
| ZOOM AUTO | TESTED ON HOST | `autoWindow` sui valori campionati, scarta gli estremi (2–98%) | graph_engine_test, graph_app_test |
| ZOOM ORIGINAL / PREVIOUS | TESTED ON HOST | finestra originale e pila (≤16) | graph_app_test |
| ZOOM SQUARE / ROUND / INTEGER | TESTED ON HOST | `squareWindow`, `roundWindow`, `integerWindow` | graph_engine_test, graph_app_test |
| ZOOM TRIG / STANDRD / INITIAL dal grafico | TESTED ON HOST | stessi preset della V-Window | graph_app_test |
| Zoom con i tasti + / − | TESTED ON HOST | `plotKey` | graph_app_test |
| PAN (afferra e trascina) | TESTED ON HOST | modalità puntatore `Action::Pan` | graph_app_test |
| Scorrimento con i tasti cursore (1/8 di finestra) | IMPLEMENTED | `plotKey` | — |
| Immagine di sfondo (Background: None / PICT n / OPEN) | TESTED ON HOST | Setup, F1 sul campo apre il file manager | graph_app_test |
| BGV-WIN dalla V-Window (carica la V-Window dello sfondo) | TESTED ON HOST | conferma, poi `loadWindowForm` | graph_app_test |
| BGV-WIN SAVE / SAVE•AS dal grafico | TESTED ON HOST | `bgvSave`: V-Window e Fade salvate nel file `.pcpict` | graph_app_test |
| Fade I/O (luminosità dello sfondo 0–100%) | TESTED ON HOST | OPTN e Setup | graph_app_test |

## 3. Disegno di un grafico (5-13…5-19)

| Funzione CG50 | Stato | Implementazione | Verifica |
|---|---|---|---|
| Tipi Y=, r=, Parm (Xt/Yt), X= | TESTED ON SIMULATOR | `GraphType`, menu TYPE | graph_engine_test, graph_app_test, screenshot 20/22 |
| Disequazioni Y>, Y<, Y≥, Y≤, X>, X<, X≥, X≤ | TESTED ON SIMULATOR | riempimento a tratteggio; linea tratteggiata per le strette | graph_engine_test, screenshot 23/34 |
| CONVERT tra Y=/disequazioni Y e X=/disequazioni X | TESTED ON HOST | `rewriteIdentifiers` scambia x ↔ y | graph_app_test |
| Funzione nuova di tipo diverso su una riga occupata: errore | TESTED ON HOST | `storeEdit` | graph_app_test |
| Funzioni composte (Y1(x), Y2(Y1), r, Xt, Yt, X; max 5 livelli) | TESTED ON HOST | `FunctionResolver`, riferimenti `@Y1`…; inserimento da VARS GRAPH | graph_engine_test |
| Proprietà: stile (Normal, Thick, Broken, Dot, Thin) e colore (7) | TESTED ON SIMULATOR | TOOL > STYLE, FORMAT | graph_app_test, screenshot 20 |
| FORMAT dal grafico (scelta del grafico lampeggiante) | TESTED ON HOST | `Action::Format`, dialogo di formato | graph_app_test |
| Modifica e cancellazione (DELETE con conferma, Xt e Yt insieme) | TESTED ON HOST | | graph_app_test |
| Selezione per il disegno (SELECT) | TESTED ON HOST | | graph_app_test |
| Assi, etichette, scala (Axes On/Off/Scale, Label) | TESTED ON SIMULATOR | asse fuori finestra indicato sul bordo | graph_app_test, screenshot 24/26 |
| Griglia (Grid On/Off/Line) | TESTED ON HOST | | graph_app_test |
| Graph Memory STORE/RECALL (20 memorie, anche V-Window) | TESTED ON HOST | testo `PCGMEM 1`, ≤24 KB per memoria | graph_app_test, graph_file_test |
| Discontinuità e asintoti (tan, 1/x, int, √ con dominio) | TESTED ON HOST | campionamento adattivo: bisezione della corda più lunga, test di salto (4 px), bordo di dominio | graph_engine_test |
| Clipping delle linee (Liang–Barsky) | TESTED ON HOST | `Viewport::clip` | graph_engine_test |
| Simul Graph On (tutti i grafici avanzano insieme) | TESTED ON HOST | round-robin del budget di campionamento | graph_app_test |
| Draw Type Plot (punti non collegati) | TESTED ON HOST | `sampleStep(plotOnly)` | graph_app_test |
| Rendering non bloccante | TESTED ON HOST | budget di 600 valutazioni per tick; TRACE attivo durante il disegno | graph_app_test |

## 4. Salvataggio e richiamo delle immagini (5-20…5-22)

| Funzione CG50 | Stato | Implementazione | Verifica |
|---|---|---|---|
| PICTURE STORE in Pict 1–20 | TESTED ON HOST | file `PICOCALC/PICT/PICTnn.pcpict` sulla microSD | graph_app_test |
| PICTURE SAVE•AS con nome (≤8 caratteri) | TESTED ON HOST | | graph_app_test |
| RECALL da Pict 1–20 e OPEN da file | TESTED ON HOST | immagine dietro i grafici, bianco trasparente | graph_app_test |
| Cls cancella l'immagine richiamata | TESTED ON HOST | | graph_app_test |
| Formato dell'immagine | IN PROGRESS | formato proprio `.pcpict` (RLE RGB565 + V-Window + Fade, CRC-32), un solo livello: la CG50 salva grafico e sfondo separati (g3p, formato proprietario non riprodotto) | graph_file_test |
| Esportazione BMP (estensione) | TESTED ON HOST | BMP 24 bit, 320×282 | graph_app_test, graph_file_test |

## 5. Due grafici sulla stessa schermata (5-23…5-24)

| Funzione CG50 | Stato | Note |
|---|---|---|
| Dual Screen G+G (COPY, SWAP sul sotto-schermo) | NOT STARTED | richiede la divisione dello schermo 320×320 |
| Dual Screen GtoT (grafico + tabella) | NOT STARTED | |

## 6. Disegno manuale (5-25…5-31)

| Funzione CG50 | Stato | Implementazione / note | Verifica |
|---|---|---|---|
| Comandi grafici in Run-Matrix (`Graph Y=`, `Graph r=`, `Graph(X,Y)=`, `Graph ∫`, ...) | NOT STARTED | richiedono il linguaggio dei programmi | — |
| Sovrapposizione `[A=1,2,3]` (Overwrite Graph, max 10 valori) | TESTED ON HOST | colori in sequenza | graph_engine_test, graph_app_test |
| Grafici da lista (List Graph, `List1`…`List6`) | TESTED ON HOST | liste di lunghezza diversa gestite | graph_engine_test |
| Copia e incolla di una funzione sul grafico (Ctrl+V, max 10) | TESTED ON HOST | `pasteGraph` | graph_app_test |

## 7. Tabelle e integrazione GRAPH ↔ TABLE (5-32…5-37)

| Funzione CG50 | Stato | Implementazione / note | Verifica |
|---|---|---|---|
| Tabella dalle funzioni Y= selezionate (stesso motore dei grafici) | TESTED ON SIMULATOR | `TableApp` usa `GraphEngine` | graph_app_test, screenshot 32 |
| SET (Start, End, Step) | TESTED ON HOST | `TableRange` | graph_app_test |
| Tabella delle derivate (Derivative On) | TESTED ON SIMULATOR | colonne dY/dX | graph_app_test, screenshot 32 |
| GPH-CON / GPH-PLT | TESTED ON SIMULATOR | i punti della tabella disegnati da GRAPH | graph_app_test, screenshot 33 |
| → TABLE dal grafico / dalla lista (OPTN) e GRAPH dalla tabella | TESTED ON HOST | | graph_app_test |
| Tabelle di r= e parametriche | NOT STARTED | solo funzioni di tipo Y | — |
| Variable = LIST (x da una lista) | NOT STARTED | | — |
| Editing della tabella (ROW DELETE/INSERT/ADD, EDIT di x, DELETE) | NOT STARTED | | — |
| LISTMEM (colonna → lista) | NOT STARTED | | — |
| Lista delle relazioni propria di TABLE (FORMULA) | IN PROGRESS | TABLE usa la lista di GRAPH | — |

## 8. Modify (5-38…5-41)

| Funzione CG50 | Stato | Implementazione | Verifica |
|---|---|---|---|
| MODIFY: variabili A–Z della funzione (max 5), passo, ←/→ | TESTED ON SIMULATOR | ridisegno immediato | graph_app_test, screenshot 28 |
| Valore digitato e passo modificabile | TESTED ON HOST | | graph_app_test |
| COPY dell'espressione con i valori correnti nella lista | TESTED ON HOST | prima riga libera, "Expression in use" se già presente | graph_app_test |
| Modify in Conic Graphs | Fuori ambito | modalità Conic Graphs non ancora implementata | — |

## 9–11. Dyna Graph, Recursion, Conic Graphs (5-42…5-51)

Modalità separate dalla Graph: solo architettura e modelli dati
(`app_models.hpp`), vedi `FEATURE_MATRIX.md`. **Fuori ambito** di questa
sessione. La TOOL > BUILT-IN di GRAPH riusa i 7 tipi predefiniti del Dyna Graph
(estensione, TESTED ON HOST in graph_app_test).

## 12. Sketch (5-52…5-53)

| Funzione CG50 | Stato | Implementazione | Verifica |
|---|---|---|---|
| Cls (cancella sketch, immagine richiamata e grafici incollati) | TESTED ON HOST | con conferma | graph_app_test |
| Tangent (calcolata con la derivata del motore, anche X= e parametriche) | TESTED ON SIMULATOR | `SketchItem::Tangent` | graph_app_test, screenshot 27 |
| Norm (normale) | TESTED ON HOST | | graph_app_test |
| Inverse (grafico inverso) | TESTED ON HOST | | graph_app_test |
| PLOT: Plot, PlotOn, PlotOff, PlotChg | TESTED ON HOST | | graph_app_test |
| LINE: Line (ultimi due punti PLOT), F-Line (due punti qualsiasi) | TESTED ON HOST | | graph_app_test |
| Circle, Vertical, Horz | TESTED ON SIMULATOR | coordinate matematiche (ellisse se la finestra non è quadrata, come sulla CG50) | graph_app_test, screenshot 27 |
| PEN (mano libera) | TESTED ON HOST | ≤400 punti | graph_app_test |
| Text | TESTED ON SIMULATOR | ≤32 caratteri | graph_app_test, screenshot 27 |
| Stile e colore delle linee di sketch (Sketch Line, Plot/LineCol) | TESTED ON HOST | dal Setup | graph_app_test |
| FORMAT durante l'operazione di sketch | IN PROGRESS | non disponibile durante l'operazione; lo stile e il colore si cambiano dopo con SKETCH > Elementi > FORMAT | graph_app_test |
| Elementi SKETCH: elenco, eliminazione singola, FORMAT (estensione) | TESTED ON HOST | | graph_app_test |
| Sketch salvati nel progetto e nel workspace | TESTED ON HOST | | graph_app_test, graph_file_test |

## 13. Analisi delle funzioni (5-54…5-63)

| Funzione CG50 | Stato | Implementazione | Verifica |
|---|---|---|---|
| TRACE: puntatore, coordinate, cambio grafico ↑/↓ alla stessa x | TESTED ON SIMULATOR | per Y=, X=, r=, parametriche, disequazioni | graph_app_test, screenshot 24 |
| TRACE: valore digitato nel pop-up (x, y, θ o T) | TESTED ON HOST | | graph_app_test |
| TRACE durante il disegno | TESTED ON HOST | | graph_app_test |
| Linee ausiliarie verso gli assi (SHIFT+↑/↓) | IMPLEMENTED | | screenshot 24 |
| Punto con etichetta con EXE, rimozione dell'ultimo con DEL (pieno/vuoto per le disequazioni strette) | TESTED ON SIMULATOR | | screenshot 24 |
| Coord On/Off, Graph Func On/Off | TESTED ON HOST | | graph_app_test |
| Derivata (Derivative On) in TRACE e nei risultati | TESTED ON SIMULATOR | derivata numerica di Ridders | graph_app_test, screenshot 24 |
| Graph to Table (con Dual Screen GtoT) | NOT STARTED | richiede il Dual Screen | — |
| Arrotondamento delle coordinate (ZOOM ROUND) | TESTED ON HOST | Xdot arrotondato: le coordinate del TRACE diventano tonde | graph_engine_test, graph_app_test |
| G-SOLVE ROOT (tutte le radici, ←/→) | TESTED ON SIMULATOR | segni + radici di tangenza; Brent–Dekker; poli e salti scartati | graph_solve_test, graph_app_test, screenshot 25 |
| MAX / MIN | TESTED ON SIMULATOR | Brent sulla derivata di Ridders, ripiego su minimizzazione di Brent | graph_solve_test, graph_app_test, screenshot 06 |
| Y-ICEPT | TESTED ON HOST | | graph_app_test |
| INTSECT (scelta dei due grafici se sono ≥3) | TESTED ON HOST | radici di f−g | graph_solve_test, graph_app_test |
| Y-CAL / X-CAL (tutte le x per una y) | TESTED ON HOST | | graph_app_test |
| ∫dx con limiti dal puntatore o digitati, area riempita, Ans | TESTED ON SIMULATOR | Gauss–Kronrod adattivo | graph_solve_test, graph_app_test, screenshot 07 |
| ∫dx ROOT (integrale e area tra radici) | TESTED ON SIMULATOR | area divisa alle radici | graph_app_test, screenshot 26 |
| ∫dx INTSECT | TESTED ON HOST | | graph_app_test |
| ∫dx MIXED (radici, intersezioni o x qualsiasi) | TESTED ON HOST | | graph_app_test |
| G-SOLVE solo per Y= e disequazioni Y | TESTED ON HOST | errore per gli altri tipi | graph_app_test |
| "Not Found" se non ci sono soluzioni | TESTED ON HOST | | graph_app_test |
| Precauzioni (5-63): tangenza, flessi, poli | TESTED ON HOST | radici di tangenza trovate per minimo di |f|; poli rifiutati per residuo | graph_solve_test |
| Analisi delle coniche | Fuori ambito | modalità Conic Graphs | — |

## Setup della modalità Graph (1-35…1-37)

| Voce | Stato | Note |
|---|---|---|
| Func Type (12 tipi) | TESTED ON HOST | |
| Draw Type (Connect/Plot) | TESTED ON HOST | |
| Derivative | TESTED ON HOST | |
| Angle (Deg/Rad/Gra) | TESTED ON HOST | condivisa con le altre modalità |
| Coord | TESTED ON HOST | |
| Grid (On/Off/Line) | TESTED ON HOST | |
| Axes (On/Off/Scale) | TESTED ON SIMULATOR | |
| Label | TESTED ON SIMULATOR | |
| Display (Fix/Sci/Norm/Eng) | TESTED ON HOST | condivisa |
| Graph Func | TESTED ON HOST | |
| Simul Graph | TESTED ON HOST | |
| Background (None/PICT n/OPEN) | TESTED ON HOST | |
| Fade I/O | TESTED ON HOST | |
| Plot/LineCol | TESTED ON HOST | colore degli elementi di sketch |
| Sketch Line | TESTED ON HOST | stile delle linee di sketch |
| Ineq Type (Intsect/Union) | TESTED ON SIMULATOR | screenshot 23/34 |
| Dual Screen (G+G/GtoT/Off) | NOT STARTED | |
| Input/Output, Frac Result, Complex Mode | Fuori ambito | impostazioni globali di SYSTEM |
| Variable (RANG/LIST) di TABLE | NOT STARTED | solo RANG |

Il tasto SETUP dentro GRAPH apre il Setup della modalità (non SYSTEM),
come sulla CG50 (TESTED ON HOST).

## Persistenza e microSD (richiesta del progetto, non presente sulla CG50)

| Funzione | Stato | Implementazione | Verifica |
|---|---|---|---|
| Progetto `.pcgraph` versionato (funzioni, stili, V-Window, memorie V-Window, Setup, SKETCH, immagine richiamata, range di TABLE) | TESTED ON HOST | testo `PCGRAPH 1` con CRC-32, vedi `PCGRAPH_FORMAT.md` | graph_file_test (345 controlli) |
| FILE: NEW, OPEN, SAVE, SAVE AS, RENAME, DELETE | TESTED ON SIMULATOR | gestore file con spazio libero | graph_app_test, screenshot 31 |
| Ciclo completo 12.4 (crea, salva, chiudi, riapri, verifica, modifica, salva) | TESTED ON HOST | | graph_app_test |
| Scrittura atomica (tmp → verifica CRC → bak → rinomina) e recupero | TESTED ON HOST | `writeAtomic`, `recover` | graph_file_test, sd_fatfs_test |
| File corrotti, versione futura, campi non validi | TESTED ON HOST | messaggi "Storage ERROR" | graph_file_test |
| SD assente, piena, scrittura parziale, errori di lettura/scrittura | TESTED ON HOST | `RamFileSystem` con iniezione di guasti | graph_file_test, graph_app_test |
| Protocollo SD in SPI (SDSC/SDHC, CMD0/8/ACMD41/58/9/16/17/24/13) | TESTED ON HOST | scheda simulata SDv1, SDv2, SDHC | sd_fatfs_test |
| File system FAT/exFAT (FatFs R0.16) | TESTED ON HOST | su disco RAM formattato FAT | sd_fatfs_test |
| Bus SPI0 della PicoCalc (GP16–19, rilevamento GP22) | IMPLEMENTED | `sd_spi_pico.cpp`; pin dai sorgenti ufficiali ClockworkPi | da collaudare su hardware |
| Stato GRAPH nel workspace in flash (record v3, lettura v1/v2) | TESTED ON HOST | | graph_app_test, storage_test |

## Mancanze (per la v1.9.0 o successive)

1. Dual Screen G+G e GtoT, Graph to Table.
2. Comandi grafici di Run-Matrix e dei programmi (`Graph Y=`, `DrawGraph`,
   `ViewWindow`, `Plot`, `Line`, ...), insieme al linguaggio dei programmi.
3. TABLE: tabelle di r= e parametriche, Variable = LIST, editing delle righe,
   LISTMEM, lista delle relazioni propria.
4. FORMAT durante lo SKETCH (si cambia dopo, da Elementi).
5. Suffissi di unità angolare (°, ʳ, ᵍ) nelle espressioni.
6. Immagini a due livelli (grafico e sfondo separati, come g3p).
7. Collaudo su PicoCalc fisica: display, tastiera, tempi di disegno, microSD
   reale (inserimento/rimozione, schede SDXC exFAT).

## Test di riferimento

| Suite | Controlli | Contenuto |
|---|---|---|
| `graph_engine_test` | 351 | viewport, clipping, campionamento, discontinuità, tipi, riferimenti, overwrite, liste, zoom, limiti di punti |
| `graph_solve_test` | 51 | radici, estremi, intersezioni, integrali, aree, poli, tangenze |
| `graph_file_test` | 345 | `.pcgraph`, G-Mem, `.pcpict`, BMP, file store, scrittura atomica, guasti |
| `graph_app_test` | 249 | flussi completi dell'interfaccia GRAPH e TABLE, progetti, ciclo 12.4, budget di memoria delle curve |
| `sd_fatfs_test` | 84 | protocollo SD simulato, FatFs, rimozione della scheda |

Screenshot: `docs/screenshots/06`, `07`, `20`–`34`, prodotti dallo script
`tests/sim/graph.txt` con lo stesso codice del firmware.
