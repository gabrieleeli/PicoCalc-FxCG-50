# Rapporto di sviluppo — v1.14.0 (completamento totale di 3D Graph)

Base: `main` = tag `v1.13.1` = commit `62b3f8f` (verificato all'inizio e
prima della pubblicazione con `git fetch origin main`: nessun commit
successivo). Analisi preliminare e perimetro:
[ANALISI_FONTI_v1.14.0.md](ANALISI_FONTI_v1.14.0.md). Riferimento di
comportamento: manuale software fx-CG50 versione 3.60, capitolo 16 "3D Graph
Function" (16-1…16-19, testo e schermate), Appendice α (α-11, α-12), SET UP
(1-35…1-38), immagini del grafico (5-20); nessun codice, ROM, font, icona o
grafica Casio. Guida della modalità: [GRAPH3D.md](../GRAPH3D.md).

## 1. Cosa è stato fatto

| Gruppo | Contenuto |
|---|---|
| Lista (16-6…16-8) | righe `1:` `2:` `3:` con il `:` evidenziato, `[—]` nel colore, SELECT, DELETE con conferma Yes/No, TYPE e ←/→ (su una riga registrata aprono il suo input), 3D-GMEM, DRAW o EXE, FORMAT (SHIFT 5/OPTN) con Line e Area Color, SHIFT F1…F6, lista vuota al primo avvio |
| Tipi (16-8…16-13) | schermata Select Template con icone proprie; **Line** EXPRESS, VECTOR, P&V, POINTS; **Plane** EXPRESS, VECTOR, POINTS; **Sphere** FACTOR, EXPAND; **Cylinder**; **Cone**; **Z=**; **Param** (S, T); **Rot X**, **Rot Y**; tutte le condizioni «Invalid Setting» del manuale; coefficienti come espressioni del valutatore unico; conversione fra i moduli |
| V-Window (16-4, 16-5) | Xmin/max/grid, Ymin/max/grid, Zmin/max, Smin/max/grid, Tmin/max/grid, Angle θ e φ in gradi; INITIAL; 3D-VMEM 1–6; «Out of Domain» e «Invalid Setting» |
| SET UP (16-1, 16-2, 16-15) | Setup proprio della modalità (SHIFT MENU): Input/Output, Angle, Display, Plot/LineCol, Axes, Box, Label; tasti • (Axes/Box) e , (Label) sul grafico |
| Grafico (16-14…16-16) | area dei grafici di GRAPH; superficie nell'Area Color con luce e reticolo nel Line Color; scatola, etichette, indicatore degli assi; rotazione con i tasti (θ/φ della V-Window); ROTATE L→R, R→L, T→B, B→T (due giri, AC); ZOOM IN, OUT, VIEW-X/Y/Z, ORIGINAL; SKETCH Cls, Plot, Text; PICTURE STORE; CAPTURE; G⇔T |
| TRACE (16-14, 16-15) | puntatore sul reticolo di ogni tipo, X/Y/Z nel formato Display, Next/Back, finestra delle coordinate con X,θ,T (Z= e Param) o un numero, SHIFT F1/EXIT |
| G-Solve (16-16…16-19) | CROSS X/Y/Z con sezioni analitiche nel colore opposto e contorno del piano coincidente, UP/DOWN, valore digitato, viste; INTSECT retta–retta, retta–piano, piano–piano con NOT FOUND/INFINITE; RELATION; Next/Back sulle coppie; «INTSECT/RELATION requires multiple lines or planes.» |
| Memorie | 3D G-Mem 1–20 (funzioni, V-Window, Setup, selezione, colori) e 3D V-Win 1–6 nel workspace; MEMORY cancella a parte 3D Graph, 3D G-Mem e 3D V-Win |
| Motore | taglio esatto di triangoli e segmenti sulle sei facce del box; z-buffer condiviso da superfici, reticoli e rette; pittore con le rette suddivise; calcolo progressivo con AC; cache dell'immagine della scena in PSRAM per TRACE/SKETCH |
| Workspace | **record v10** con il nuovo stato in coda; conversione dei record v5–v9; fixture `workspace_v1_13_1_graph3d.inc` generata con i sorgenti della v1.13.1 prima delle modifiche |

## 2. Architettura

- **`graph3d_model.hpp/.cpp`** (nuovo, puro, nel livello del motore): tipi,
  moduli e layout dei coefficienti, colori, V-Window, Setup, validazione del
  manuale, forma canonica dei modelli (`Shape`), conversione fra i moduli,
  INTSECT e RELATION analitici (prodotti vettoriali, tolleranze relative),
  codifica binaria degli insiemi (3D G-Mem e record v10). Nessun parser,
  workspace o grafica (`architecture_test` lo verifica).
- **`graph3d.hpp/.cpp`** (riscritto): reticoli (u, v) di ogni tipo con
  campionamento incrementale, `templatePatches`/`templatePoint` analitici,
  `clipLine`, proiezione `Projector` (θ, φ, zoom), `render()` con z-buffer a
  16 bit (profondità fissa del cubo) o pittore, taglio di Sutherland–Hodgman
  e di Liang–Barsky in coordinate normalizzate, etichette e indicatore,
  `crossSection()` analitica, disegni sovrapposti. Nessuna UI.
- **`src/apps/graph3d_app.cpp`** (lista, selezione, coefficienti, input,
  V-Window, Setup, FORMAT, memorie, finestre di immissione) e
  **`src/apps/graph3d_plot.cpp`** (calcolo, grafico, TRACE, G-Solve, SKETCH,
  rotazione, immagini). Il controller legge e scrive solo `ws.graph3d`,
  `ws.settings` e `ws.memory` (sola lettura), come dichiara il registro.
- **Un solo parser e valutatore**: Z=, Rot X/Y con `CompiledExpression(…,
  "x")` e `eval2(x, "y", y)`, Param con `"s"` e `"t"`, coefficienti con
  `evalNumber`; le variabili del grafico sono legate in `EvalContext`, A–Z
  non vengono mai scritte (provato in `graph3d_v114_test` e nel fuzzing).
- **Registro**: servizi Expression, Widgets, Editor, Format, MathRender,
  Storage, Files, Graph3D (`graph3d.hpp`, `graph3d_model.hpp`, `limits.hpp`);
  dati Settings, Memory, Graph3D. `architecture_test` ha un gruppo nuovo per
  il 3D (nessuna lettura propria dei numeri, nessuna scrittura di A–Z,
  modello puro, libreria di disegno senza UI, record validato dal modello).
- Titoli d'errore nuovi nell'`AppManager`: «Invalid Setting», «Out of
  Domain», «INTSECT requires», «RELATION requires» (α-12).

## 3. Memoria per scheda (`limits.hpp`)

| Limite | Pico 2 W | Pico Plus 2 W |
|---|---|---|
| Punti per lato (`graph3dMaxGrid`) | 25 | 50 |
| Z-buffer (`graph3dZBuffer`) | no (pittore) | sì, 180 KB in PSRAM |
| Cache dell'immagine della scena | no | 180 KB in PSRAM |
| 3D G-Mem (`kGraph3DMemoryBytes`) | 12 KB | 64 KB |
| Vertice / vertice proiettato / ordine del pittore | 16 / 16 / 16 B | uguale, in PSRAM |

`graph3dPoints(pixel, grafici)` sceglie i punti per lato dalla memoria libera
(48 B per punto e grafico più z-buffer e cache); un `std::bad_alloc` dimezza
la griglia fino a 2 punti, poi «Memory ERROR» senza memoria trattenuta
(`retainedBytes() == 0`, provato in `oom_test` e `graph3d_v114_test`). Nessun
oggetto grande sullo stack: poligoni tagliati in array fissi di 16 punti,
sezioni in vettori `pmr` dai pool.

## 4. Test eseguiti

| Prova | Esito |
|---|---|
| `tests/run_host_tests.sh` (profili Plus e Pico 2 W) | vedi §4.1 |
| `--sanitize --tsan` | vedi §4.1 |
| Nuova suite `graph3d_v114_test` | 535 controlli per profilo: V-Window, modelli e «Invalid Setting», conversioni, codifica, INTSECT/RELATION (esempi 16-18 e 16-19), sezioni, taglio al box (nessun pixel fuori dall'involucro del box in tre viste, z-buffer e pittore), z-buffer condiviso con le rette, proiezione e viste, colori, record v10 e migrazione di tutte le fixture v1…v9, modalità dalla tastiera su entrambi i profili (ogni voce della tabella §5) |
| `graph3d_test` (v1.9.0, portata alla nuova API) | 88 controlli per profilo |
| `oom_test` | 3D con Z= e cilindro sotto esaurimento: griglia ridotta o Memory ERROR, nulla trattenuto |
| Fuzzing | `tools/fuzz/fuzz_graph3d.cpp` (insiemi G-Mem, espressioni Z=/Param, finestre) e `fuzz_workspace` con i semi v10: vedi §4.2 |
| Simulatore | `tests/sim/graph3d.txt` (40–43, rigenerati) e `tests/sim/graph3d_v114.txt` (88–104), PNG a palette compressi senza perdita (verifica pixel per pixel) |
| Firmware | vedi [BUILD_STATUS.md](../BUILD_STATUS.md) |

Nessuna prova su hardware: procedure in
[HARDWARE_TEST_v1.14.0.md](../HARDWARE_TEST_v1.14.0.md) (HW-170…HW-189).

## 5. Tabella di parità finale

**P** presente, **I** differenza inevitabile documentata (§6), **—** fuori
perimetro. Le voci sono quelle
dell'[analisi](ANALISI_FONTI_v1.14.0.md#2-tabella-di-parità-perimetro-del-lavoro).

| # | Voce | v1.13.1 | v1.14.0 | Note |
|---|---|---|---|---|
| G1 | modalità nel menu | P | P | |
| G2 | tre grafici insieme | P | P | di ogni tipo |
| G3 | lista `1:` con `:` evidenziato e `[—]` | D | P | |
| G4 | SELECT | P | P | |
| G5 | DELETE con conferma | D | P | F1 Yes / F6 No; i colori della riga restano **(scelta)** |
| G6 | TYPE e ←/→ | M | P | |
| G7 | 3D-GMEM 1–20 | M | P | budget per scheda: **I** (12 KB sul Pico 2 W) |
| G8 | DRAW o EXE | D | P | |
| G9 | FORMAT Line/Area Color, Clear, Invalid Setting | M | P | SHIFT 5 o OPTN: **I** (tasti) |
| G10 | G⇔T | D | P | SHIFT F6 e F6 |
| G11 | AC torna alla lista | D | P | |
| G12 | valori iniziali | D | P | lista vuota; i record precedenti conservano le funzioni |
| T1 | Select Template, Template/Z=/Param/Rotate | M | P | icone proprie: **I** |
| T2 | coefficienti, riselezione, SET | M | P | |
| T3–T6 | Line EXPRESS, VECTOR, P&V, POINTS | M | P | |
| T7 | Invalid Setting della retta | M | P | |
| T8–T10 | Plane EXPRESS, VECTOR, POINTS | M | P | |
| T11 | Invalid Setting del piano | M | P | anche vettori opposti (stessa giacitura) |
| T12–T13 | Sphere FACTOR, EXPAND | M | P | |
| T14 | Invalid Setting della sfera | M | P | |
| T15 | Cylinder | M | P | chiuso dalle basi **(scelta)** |
| T16 | Cone | M | P | chiuso dalla base **(scelta)** |
| T17 | celle, EDIT, valore in basso a destra, tasti dei moduli | M | P | conversione fra moduli **(scelta)** |
| T18 | Z= come in GRAPH, SET | D | P | F1 X, F2 Y durante la digitazione **(scelta)** |
| T19 | Param Xst/Yst/Zst in S, T | M | P | |
| T20–T21 | Rot X, Rot Y | M | P | |
| T22 | lista con nome e coefficienti | M | P | |
| V1 | SHIFT F3 | D | P | anche F3 dal grafico |
| V2 | X/Y/Z min-max, Xgrid, Ygrid | D | P | grid = punti (2–50); 25 al più sul Pico 2 W: **I** |
| V3 | S, T | M | P | iniziali −3…3 **(scelta)** |
| V4 | Angle θ, φ in gradi | M | P | iniziali 20°, 70° **(scelta)** |
| V5 | limiti, Out of Domain / Invalid Setting | D | P | |
| V6 | iniziali e INITIAL | D | P | |
| V7 | grid come divisioni dei modelli | M | P | |
| V8 | 3D-VMEM 1–6 | M | P | |
| V9 | EXIT | P | P | |
| S1 | Setup della modalità | D | P | voci comuni **(scelta)** |
| S2–S4 | Axes, Box, Label | M/D | P | |
| S5–S6 | tasti • e , | M | P | |
| S7 | Plot/LineCol | M | P | |
| S8 | Input/Output, Angle, Display | D | P | **(scelta)** |
| P1 | grafico a tutto schermo | D | P | 320 × 282: **I** |
| P2 | rotazione con i cursori, θ/φ seguono | D | P | 15° per tasto **(scelta)** |
| P3 | ROTATE, due giri, AC | M | P | 10° per passo **(scelta)** |
| P4 | PICTURE | M | P | file `.pcpict` del progetto: **I** (formato g3p Casio) |
| P5 | ZOOM IN, OUT | D | P | ×1,5 **(scelta)** |
| P6 | VIEW-X/Y/Z | M | P | orientazione di VIEW-Z **(scelta)** |
| P7 | ORIGINAL | D | P | |
| P8 | SKETCH Cls, Plot, Text | M | P | cancellati da ogni nuova vista **(scelta)** |
| P9 | Area Color + reticolo Line Color | D | P | |
| P10 | z-buffer condiviso fra tipi | P | P | rette comprese; pittore senza PSRAM: **I** |
| P11 | taglio al box | D | P | esatto, provato al pixel |
| P12 | calcolo progressivo, AC | D | P | |
| R1–R5 | TRACE, X/Y/Z, Next/Back, coordinate, uscita | M | P | X,θ,T = `x`: **I**; sui modelli sul reticolo **(scelta)** |
| X1 | G-SOLVE CROSS / INTSECT / RELATION | M | P | |
| X2–X4 | CROSS | M | P | passo (max−min)/(grid−1) **(scelta)** |
| X5 | INTSECT | M | P | piano–piano in forma parametrica **(scelta)** |
| X6 | RELATION | M | P | testi **(scelta)** |
| X7 | requires multiple lines or planes | M | P | |
| I1 | parser unico, X Y S T legate | P | P | |
| I2 | unità d'angolo, θ/φ in gradi | D | P | |
| I3 | Complex Mode: punti non reali = buchi | P | P | **(scelta)** |
| I4 | espressioni esatte in decimale | P | P | |
| I5 | coefficienti come espressioni | M | P | |
| I6 | MEMORY 3DGRAPH, 3DGMEM, 3DVWIN | D | P | |
| I7 | workspace | P | P | record v10 |
| I8 | CAPTURE | M | P | SHIFT 7 |
| I9 | file `.g3m` del Memory Manager | — | — | capitolo 11, nessun Memory Manager con file nel progetto |
| I10 | VARS | P | P | il manuale non prevede variabili 3D |
| E1 | Syntax ERROR | P | P | anche nei coefficienti |
| E2 | Math ERROR nei coefficienti | M | P | |
| E3 | Memory ERROR | P | P | ogni tipo, nulla trattenuto |
| E4 | Invalid Setting | M | P | |
| E5 | Out of Domain | M | P | |
| E6 | NOT FOUND, INFINITE | M | P | |
| E7 | DRAW senza grafici | D | P | scatola vuota **(scelta)** |

Voci rimaste «diverse» o «mancanti»: **nessuna**.

## 6. Differenze inevitabili e scelte

| Voce | Differenza | Motivo |
|---|---|---|
| Schermo | area del grafico 320 × 282 | display PicoCalc 320 × 320 |
| Tastiera | F1–F6 del grafico anche senza SHIFT, SHIFT = F7, OPTN = F8, SET UP = F10, X,θ,T = `x`, AC = Shift+Esc, FORMAT = SHIFT 5 o OPTN | tastiera PicoCalc |
| Griglia e z-buffer sul Pico 2 W | ≤ 25 punti per lato, pittore | 520 KB di SRAM senza PSRAM |
| 3D G-Mem | 12 KB sul Pico 2 W | record da 59,75 KB |
| PICTURE | file `.pcpict` del progetto | il formato g3p è Casio |
| Grafica | icone, puntatore, indicatore propri | proprietà Casio |
| Scelte | θ/φ iniziali, S/T iniziali, passi, VIEW-Z, cilindro e cono chiusi, conversione dei moduli, testi di RELATION, forma della retta piano–piano, passo di CROSS, TRACE sui modelli, SKETCH per vista, scatola vuota | il manuale non le descrive riga per riga (elenco in [ANALISI_FONTI_v1.14.0.md](ANALISI_FONTI_v1.14.0.md#3-differenze-inevitabili-e-scelte)) |

## 7. Problemi rimasti

- Nessuna prova su hardware PicoCalc (HW-170…HW-189); i tempi di calcolo e di
  disegno sul Cortex-M33 sono stime.
- Il pittore (Pico 2 W) ordina per quadrilateri: due superfici che si
  intersecano dentro lo stesso quadrilatero possono mostrare piccoli errori
  d'ordine (lo z-buffer della Plus è esatto al pixel).
- I valori iniziali di θ e φ e alcuni passi sono scelte motivate, da
  confrontare con una fx-CG50 reale.
- File `.g3m` del Memory Manager non realizzati (fuori perimetro, nessuna
  modalità del progetto ha il Memory Manager con file).
