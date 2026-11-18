# Analisi delle fonti — v1.14.0 (completamento totale di 3D Graph)

Rapporto preparato **prima** di scrivere codice per la v1.14.0. Base:
`main` = tag `v1.13.1` = commit `62b3f8f` (verificato con `git log`: nessun
commit successivo al tag). Le fonti Casio sono usate solo come **riferimento
di comportamento**: nessun codice, ROM, font, icona o grafica Casio entra nel
progetto (le icone dei modelli, i colori e i testi della schermata sono
disegnati dal progetto).

Stato del repository prima di ogni modifica:

- test host del profilo Pimoroni Pico Plus 2 W: **46/46 superati** (base di
  confronto, `tests/run_host_tests.sh --board PICO_PLUS_2W`);
- fixture `tests/fixtures/workspace_v1_13_1_graph3d.inc` generata **con i
  sorgenti della v1.13.1** (record versione 9: Deg, A = 2,5; Z1 = x^2−y^2
  disegnata, Z2 = sin(x)+cos(y) non disegnata, Z3 = A·x·y disegnata; finestra
  X −4…5, Y −2…6, Z −10…8; griglia 30; vista yaw 60, pitch 40, zoom 1,5):
  serve alla prova di migrazione v9 → v10. È nel primo commit del branch,
  prima di qualunque modifica a `workspace.cpp`.

## 1. Fonti

| # | Fonte | Cosa documenta | Uso |
|---|---|---|---|
| 1 | *fx-CG50/fx-CG50 AU/fx-CG20/fx-CG20 AU/fx-CG10 Software User's Guide*, versione 3.60 (PDF ufficiale Casio, `casio.com/content/dam/casio/global/support/manuals/calculators/pdf/004-en/f/fx-CG50_Soft_v360_EN.pdf`, 678 pagine): **cap. 16 "3D Graph Function"** (pagine 16-1…16-19, testo e schermate), Appendice α (messaggi d'errore α-11, α-12), cap. 1-35…1-38 (SET UP), cap. 5-20 (immagini del grafico), cap. 11/13 (nomi dei dati 3DGRAPH, 3DGMEM n, 3DVWIN_n) | comportamento della calcolatrice (3D Graph versione 1.01, OS ≥ 3.10) | riferimento principale; le pagine indicate sotto sono quelle stampate |
| 2 | Lo stesso capitolo su manualslib (`manualslib.com/manual/3093535`, pagine 541–559) | conferma del testo | controllo incrociato |
| 3 | Manuale ClassPad 300 PLUS (3D Graph, stessa famiglia): intervalli della 3D View Window (2 ≤ grid ≤ 50, −180 < θ ≤ 180) | conferma degli intervalli; valori iniziali di θ e φ | solo per le voci segnate **(scelta)** |
| 4 | Materiale Casio Education ("Using graphing technology to plot and analyse 3D lines", pagine prodotto fx-CG50) | elenco dei tipi (sfera, cilindro, piano, retta, solidi di rotazione) | conferma |
| 5 | Documenti e sorgenti del progetto: CONTINUE, ROADMAP (riga 3D Graph), APPLICATIONS_ARCHITECTURE, ARCHITECTURE, STORAGE, PICO_PLUS_2W §6, GRAPHICS_ENGINE, TESTING; `include/graph3d.hpp`, `src/graph3d.cpp`, `src/apps/graph3d_app.cpp`, `include/app_models.hpp` (`Graph3DModel`), `src/workspace.cpp` (blocco 3D del record v9), `src/limits.cpp`, `tests/graph3d_test.cpp`, `tests/sim/graph3d.txt` | architettura reale, record v9, vincoli | perimetro e vincoli |

Il PDF è cifrato contro la copia ma leggibile e stampabile: il testo è stato
estratto per la lettura e le schermate sono state guardate pagina per pagina.
Dove il manuale non dice qualcosa riga per riga, il comportamento scelto è
segnato **(scelta)** qui, in `docs/GRAPH3D.md` e nel rapporto di sviluppo.

## 2. Tabella di parità (perimetro del lavoro)

Stato **prima** della v1.14.0: **P** presente, **D** diverso, **M**
mancante, **I** differenza inevitabile (§3). Ogni voce D o M va portata a P
o documentata come I con un motivo concreto. La tabella finale è in
`docs/reports/SVILUPPO_v1.14.0.md`.

### 2.1 Modalità e lista delle funzioni (16-1, 16-2, 16-3, 16-6…16-8)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| G1 | Modalità 3D Graph nel menu principale | 16-1 | P | — |
| G2 | Fino a tre grafici disegnati insieme | 16-1 | P | (Z1–Z3) diventano le righe 1–3 di qualunque tipo |
| G3 | Lista «3D Graph» con le righe `1:` `2:` `3:`; il «:» evidenziato indica «disegna»; indicatore `[—]` nel colore del grafico | 16-2, 16-6 | D (righe Z1= Z2= Z3= con quadratino) | layout del manuale |
| G4 | SELECT (F1): disegna / non disegnare | 16-6 | P | — |
| G5 | DELETE (F2 o DEL) con la finestra di conferma Yes/No | 16-6 | D (cancella subito) | conferma F1 Yes / F6 No |
| G6 | TYPE (F3) o ←/→: schermata di selezione; su una funzione registrata ←/→ aprono direttamente la sua schermata di input | 16-8 | M | schermata di selezione |
| G7 | 3D-GMEM (F4): STORE / RECALL 1–20; un insieme contiene le tre funzioni, la V-Window, il Setup, la selezione e i colori; STORE sovrascrive, errore se la memoria è piena; RECALL sostituisce tutto | 16-7, 16-8 | M | 20 memorie nel workspace (record v10), budget per scheda |
| G8 | DRAW (F6) **o EXE** | 16-3 | D (EXE modifica la riga) | EXE disegna |
| G9 | FORMAT (SHIFT 5): Line Color e Area Color (Black, Blue, Red, Magenta, Green, Cyan, Yellow, White, Clear); il grafico prende l'Area Color, con Area Clear il Line Color; entrambi Clear → «Invalid Setting»; la retta (Line) usa l'Area Color | 16-6, 16-7 | M (famiglie di colore fisse) | due colori per grafico (record v10) |
| G10 | G⇔T (SHIFT F6): lista ⇄ grafico dopo il disegno | 16-3 | D (solo F6 dal grafico) | SHIFT F6 e F6 in entrambe le direzioni |
| G11 | AC (o EXIT) dal grafico torna alla lista | 16-3 | D (solo EXIT/F6) | AC e EXIT |
| G12 | Valori iniziali: lista vuota, colori Line Black e Area Blue/Red/Green per le righe 1/2/3 | 16-2, 16-6 | D (Z1 = sin(x)cos(y) predefinita) | lista vuota al primo avvio; i record precedenti conservano le loro funzioni |

### 2.2 Schermata di selezione e tipi di grafico (16-8…16-13)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| T1 | «Select Template»: Line, Plane, Sphere, Cylinder, Cone (con icona); F1 Template, F2 Z=, F3 Param, F4 Rotate (F1 Rot X, F2 Rot Y) | 16-8, 16-9 | M | schermata e sottomenu |
| T2 | EXE su un modello apre i coefficienti; riselezionare un modello registrato mostra i coefficienti precedenti; F6 SET torna alla lista | 16-9 | M | |
| T3 | Line **EXPRESS**: (X−X₀)/a = (Y−Y₀)/b = (Z−Z₀)/c (X₀ Y₀ Z₀ / a b c) | 16-9 | M | |
| T4 | Line **VECTOR**: r⃗ = r⃗₀ + t·v⃗ | 16-9 | M | |
| T5 | Line **P&V**: punto (X₀,Y₀,Z₀) e vettore direzione [a,b,c] | 16-10 | M | |
| T6 | Line **POINTS**: P1, P2 | 16-10 | M | |
| T7 | Line «Invalid Setting»: EXPRESS a=0 o b=0 o c=0; VECTOR v⃗ nullo; P&V direzione nulla; POINTS P1 = P2 | 16-10 | M | |
| T8 | Plane **EXPRESS**: aX+bY+cZ+d=0 | 16-10 | M | |
| T9 | Plane **VECTOR**: r⃗ = r⃗₀ + s·u⃗ + t·v⃗ | 16-10 | M | |
| T10 | Plane **POINTS**: P1, P2, P3 | 16-11 | M | |
| T11 | Plane «Invalid Setting»: EXPRESS a=b=c=0; VECTOR u⃗ o v⃗ nullo, u⃗ e v⃗ nella stessa direzione; POINTS due punti uguali, tre punti allineati | 16-11 | M | |
| T12 | Sphere **FACTOR**: (X−a)²+(Y−b)²+(Z−c)²=r² | 16-11 | M | |
| T13 | Sphere **EXPAND**: X²+Y²+Z²+aX+bY+cZ+d=0 | 16-11 | M | |
| T14 | Sphere «Invalid Setting»: r ≤ 0; a²+b²+c² ≤ 4d | 16-11 | M | |
| T15 | **Cylinder**: Radius, Zmin, Zmax, Center Point X, Y; «Invalid Setting» con Radius ≤ 0 o Zmin = Zmax | 16-12 | M | |
| T16 | **Cone**: Radius (base), Zmin (base), Zmax (vertice), X, Y del centro della base; «Invalid Setting» con Radius ≤ 0 o Zmin = Zmax | 16-12 | M | |
| T17 | Input dei coefficienti: cursore sulle celle, EDIT, valore della cella in basso a destra, tasti dei moduli (EXPRESS VECTOR P&V POINTS / FACTOR EXPAND) | 16-9…16-12 (schermate) | M | |
| T18 | **Z=** f(X,Y): input e modifica come in GRAPH, F6 SET | 16-12, 16-13 | D (editor lineare, nessun SET) | editor naturale di GRAPH |
| T19 | **Param**: Xst=, Yst=, Zst= in S e T; X,θ,T inserisce T | 16-13 | M | |
| T20 | **Rot X**: Y=f(X) ruotata attorno all'asse X (solido di rotazione) | 16-13 | M | |
| T21 | **Rot Y**: Y=f(X) ruotata attorno all'asse Y | 16-13 | M | |
| T22 | La lista mostra il nome del modello e i coefficienti nell'equazione (Sphere «(X−0)²+(Y−0)²+(Z−0)²=2²», Line «P1=(…) P2=(…)», «r⃗=…+t…») | 16-2, 16-6, 16-18, 16-19 | M | |

### 2.3 3D View Window (16-4, 16-5)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| V1 | SHIFT F3 (V-WIN) dalla lista e dal grafico | 16-4 | D (F3) | SHIFT F3 e F3 |
| V2 | Xmin/Xmax/**Xgrid**, Ymin/Ymax/**Ygrid**, Zmin/Zmax | 16-4 | D (una sola «Griglia» 5–128 celle) | due griglie, numero di punti |
| V3 | Smin/Smax/Sgrid, Tmin/Tmax/Tgrid (solo grafici parametrici) | 16-4, 16-5 | M | |
| V4 | **Angle θ** (rotazione oraria dell'asse x, −180° < θ ≤ 180°) e **Angle φ** (angolo fra la linea di vista e l'asse z, 0° ≤ φ < 360°), sempre in gradi | 16-4 | M (yaw/pitch fuori dalla V-Window) | θ e φ nella V-Window |
| V5 | \|min\|,\|max\| < 1×10⁹⁸; 2 ≤ grid ≤ 50; valori fuori intervallo «Out of Domain», combinazioni non valide (Xmin ≥ Xmax) «Invalid Setting» | 16-4, α-12 | D (Argument ERROR, 5–128) | messaggi dell'Appendice α |
| V6 | Valori iniziali (X e Y −3…3, grid 25, Z −3…3) e F1 INITIAL | 16-4 | D (griglia 40, nessun θ/φ) | INITIAL del manuale |
| V7 | Per i modelli Xgrid/Ygrid sono le divisioni del grafico stesso (meridiani e paralleli della sfera…) | 16-5 | M | |
| V8 | **3D-VMEM** (F4): STORE / RECALL 1–6 | 16-5 | M | 6 memorie nel workspace |
| V9 | EXIT chiude la V-Window | 16-4 | P | — |

### 2.4 SET UP del 3D Graph (16-1, 16-2, 16-15)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| S1 | SHIFT MENU in 3D Graph apre il Setup della modalità | 16-1 | D (apre SYSTEM) | Setup proprio |
| S2 | **Axes** On/Off (indicatore degli assi x, y, z), predefinito On | 16-1 | M | |
| S3 | **Box** On/Off (scatola delle coordinate), predefinito On | 16-1 | D (sempre) | |
| S4 | **Label** On/Off (nomi degli assi e valori sugli spigoli), predefinito On | 16-2 | M | |
| S5 | Tasto **•** (punto) sul grafico: Axes/Box On/On → On/Off → Off/On → Off/Off → On/On | 16-1 | M | |
| S6 | Tasto **,** sul grafico: Label On ⇄ Off | 16-2 | M | |
| S7 | **Plot/LineCol**: colore dei punti e dei testi di SKETCH | 16-15 | M | |
| S8 | Voci comuni della modalità: Input/Output, Angle, Display **(scelta)**: il manuale non elenca le voci comuni del 3D Graph; sono quelle che cambiano il comportamento della modalità (editor, funzioni trigonometriche, coordinate di TRACE) | 1-35…1-38 | D | |

### 2.5 Schermata del grafico (16-2, 16-3, 16-14…16-16)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| P1 | Grafico a tutto schermo con la scatola, le etichette e l'indicatore degli assi | 16-2, 16-3 | D (riquadro ridotto con una riga di intestazione) | area dei grafici di GRAPH |
| P2 | Rotazione con i tasti cursore in alto, basso, sinistra, destra; θ e φ della V-Window seguono la rotazione | 16-14 | D (yaw/pitch separati, inclinazione limitata a −5…85°) | θ e φ della V-Window, giro completo |
| P3 | **Rotazione automatica** (OPTN F2 ROTATE: F1 L→R, F2 R→L, F3 T→B, F4 B→T), si ferma dopo due giri, AC la interrompe | 16-14 | M | |
| P4 | Schermata come immagine (OPTN F1 PICTURE, STORE come a 5-20), senza la V-Window | 16-14, 5-20 | M | |
| P5 | ZOOM (SHIFT F2): F1 IN, F2 OUT sul centro dello schermo | 16-15 | D (F1/F2 ZOOM±) | |
| P6 | ZOOM: F3 VIEW-X, F4 VIEW-Y, F5 VIEW-Z (vista lungo l'asse positivo) | 16-15 | M | |
| P7 | ZOOM F6 ORIGINAL: vista non ruotata e non ingrandita (quella del disegno) | 16-15 | D (F5 torna a una vista fissa) | |
| P8 | SKETCH (SHIFT F4): F1 Cls, F2 Plot (puntatore, EXE), F3 Text | 16-15, 16-16 | M | |
| P9 | Superficie nel colore Area con le linee della griglia nel colore Line; Area Clear: solo reticolo | 16-2, 16-7 | D (colore per altezza, nessun reticolo) | |
| P10 | Più grafici di tipo diverso nella stessa scena, intersezioni visibili | 16-1 | P (solo z=f) | z-buffer condiviso da tutti i tipi (rette comprese); pittore senza PSRAM |
| P11 | Taglio al box (V-Window) per ogni tipo | 16-4 | D (si scartano i quadrilateri con un vertice fuori) | taglio esatto dei triangoli e delle rette sulle 6 facce |
| P12 | Calcolo progressivo, tastiera reattiva; AC interrompe il calcolo | — (progetto), 16-14 (AC) | D (AC ignorato) | AC ferma il calcolo e lascia il disegno parziale |

### 2.6 TRACE (16-14, 16-15)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| R1 | SHIFT F1 (TRACE): puntatore sul grafico, i cursori lo muovono lungo il grafico | 16-14 | M | sul reticolo di ogni tipo |
| R2 | Lettura di X, Y, Z (in basso a sinistra), «TRACE» a destra, funzione in alto a sinistra | 16-15 | M | |
| R3 | F1 Next / F2 Back fra i grafici disegnati | 16-14 | M | |
| R4 | X,θ,T: finestra per le coordinate (X e Y per Z=, S e T per Param); anche digitando un valore | 16-15 | M | |
| R5 | SHIFT F1 (o EXIT) termina il TRACE | 16-15 | M | |

### 2.7 G-Solve (16-16…16-19, α-12)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| X1 | SHIFT F5 (G-SOLVE): F1 CROSS, F2 INTSECT, F3 RELATION | 16-16 | M | |
| X2 | CROSS: F1 X / F2 Y / F3 Z, piano perpendicolare all'asse; sezione nel colore opposto all'Area Color, in primo piano; se un piano coincide con la sezione se ne evidenzia il contorno | 16-16, 16-17 | M | sezioni analitiche |
| X3 | CROSS: F1 UP / F2 DOWN muovono la sezione, un valore digitato la posiziona; F3–F5 VIEW-X/Y/Z, F6 ORIGINAL; «X=0» e «CROSS X» sullo schermo | 16-17 | M | |
| X4 | CROSS solo per i grafici dei modelli (Line, Plane, Sphere, Cylinder, Cone) | 16-16 | M | |
| X5 | INTSECT: retta–retta (punto), retta–piano (punto), piano–piano (retta); «NOT FOUND», «INFINITE»; F1 Next / F2 Back fra le combinazioni; EXIT cancella | 16-18 | M | intersezioni analitiche |
| X6 | RELATION: retta–retta (intersecanti, perpendicolari, parallele, sghembe, coincidenti); retta–piano (intersecante, perpendicolare, parallela, giacente nel piano); piano–piano (intersecanti, perpendicolari, paralleli, coincidenti); Next/Back; EXIT | 16-19 | M | |
| X7 | «INTSECT requires multiple lines or planes.» / «RELATION requires multiple lines or planes.» | α-12 | M | |

### 2.8 Integrazione con il resto

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| I1 | Un solo parser e valutatore; X, Y (Z=, Rot), S, T (Param) variabili del grafico che non toccano A–Z; A–Z, π, e e le funzioni nelle espressioni | 16-12 | P (x, y) | S, T con lo stesso meccanismo (`EvalContext::bind`) |
| I2 | Unità d'angolo del SET UP per le funzioni; θ e φ sempre in gradi | 16-4 | D (solo yaw/pitch interni) | |
| I3 | Complex Mode: i valori non reali sono punti non definiti (buchi), come in GRAPH **(scelta)** | — | P | |
| I4 | Espressioni esatte (frazioni, radici) valutate in decimale, come in GRAPH | — | P | |
| I5 | Coefficienti dei modelli come espressioni valutate (frazioni, A–Z) | 16-9 | M | valutatore unico |
| I6 | Memory: cancellazione selettiva dei dati del 3D Graph, delle 3D G-Mem e delle 3D V-Win (dati 3DGRAPH, 3DGMEM n, 3DVWIN_n del manuale) | 11/13 | D (una voce «Funzioni 3D Graph») | tre voci |
| I7 | Salvataggio nel workspace | progetto | P (record v9) | record **v10** con migrazione v1–v9 |
| I8 | CAPTURE (SHIFT 7) del grafico | 1-? | M | come in Run-Matrix, nelle catture PICOCALC/CAPT |
| I9 | File su microSD: la fx-CG50 copia 3DGMEM/3DVWIN nella memoria di archiviazione solo con il Memory Manager (cap. 11, file `.g3m`) | 11, 13 | — | fuori perimetro: il Memory Manager con copia dei file non esiste per nessuna modalità del progetto |
| I10 | VARS: il manuale (1-31) non elenca variabili del 3D Graph per Run-Matrix | 1-31 | P | nessuna variabile nuova |

### 2.9 Messaggi d'errore

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| E1 | Syntax ERROR nell'input delle funzioni e dei coefficienti | α | P | anche per i coefficienti |
| E2 | Math ERROR nei coefficienti (divisione per zero…) | α | M | |
| E3 | Memory ERROR (calcolo, z-buffer, G-Mem piene) | α, 16-8 | P (superfici) | per ogni tipo, senza memoria trattenuta |
| E4 | «Invalid Setting» (modelli, colori, V-Window) | α-12 | M | |
| E5 | «Out of Domain» (valori fuori intervallo) | α-12 | M | |
| E6 | «NOT FOUND», «INFINITE» (sullo schermo, non finestre d'errore) | 16-18 | M | |
| E7 | DRAW senza funzioni da disegnare | — | D (Argument ERROR) | scatola vuota, come la lista vuota della fx-CG50 **(scelta)** |

## 3. Differenze inevitabili e scelte

| Voce | Differenza | Motivo |
|---|---|---|
| Schermo | area del grafico 320×282 (righe 18–299), come GRAPH | display PicoCalc 320×320 (calcolatrice 384×216) |
| Tastiera | SHIFT = F7, OPTN = F8, SET UP = F10; sul grafico F1–F6 senza SHIFT (TRACE, ZOOM, V-WIN, SKETCH, G-SOLV, G⇔T) come in GRAPH, SHIFT F1–F6 funziona ugualmente; FORMAT = SHIFT 5; X,θ,T = tasto `x`; AC = Shift+Esc (Break); CAPTURE = SHIFT 7 | tastiera PicoCalc (nessun tasto SHIFT/ALPHA/X,θ,T dedicato) |
| Griglia sul Pico 2 W | fino a 25 punti per lato (il valore iniziale della fx-CG50); con la PSRAM della Plus fino a 50 come la calcolatrice | 520 KB di SRAM senza PSRAM; il valore della V-Window resta quello digitato, si disegna con la griglia possibile |
| Z-buffer | solo con la PSRAM; senza, algoritmo del pittore (intersezioni approssimate al quadrilatero) | 180 KB di z-buffer non stanno nella SRAM libera |
| Velocità | calcolo e disegno sul Cortex-M33 diversi dalla calcolatrice | CPU diversa; tempi in `HARDWARE_TEST_v1.14.0.md` |
| Grafica | icone dei modelli, puntatore, indicatore degli assi propri | proprietà Casio |

Scelte (il manuale non le descrive riga per riga):

| Voce | Scelta |
|---|---|
| θ e φ iniziali | 20° e 70° (valori iniziali della 3D View Window della ClassPad, stessa definizione degli angoli): l'asse x punta verso l'osservatore e a sinistra, y a destra, z in alto, come nelle schermate 16-2/16-3 |
| S e T iniziali | Smin = Tmin = −3, Smax = Tmax = 3, Sgrid = Tgrid = 25 (come X e Y) |
| Significato di grid | numero di punti calcolati (grid − 1 intervalli), come dice il manuale («number of calculation points») |
| Passi | rotazione 15° per tasto (→ θ +15°, ↑ φ +15°); ZOOM IN/OUT ×1,5; rotazione automatica a passi di 10° (72 passi = due giri) |
| VIEW-X/Y/Z | φ = 90°, θ = 0° (x verso l'osservatore); φ = 90°, θ = 90°; φ = 0°, θ = −90° (x a destra, y in alto) |
| Cilindro e cono | superfici chiuse (con le basi) |
| Conversione fra i moduli | passando fra EXPRESS/VECTOR/P&V/POINTS (o FACTOR/EXPAND) i coefficienti vengono convertiti nello stesso oggetto geometrico se validi, altrimenti azzerati |
| Testi di RELATION | INTERSECT, PERPENDICULAR, PARALLEL, SKEW, SAME LINE, ON THE PLANE, SAME PLANE (inglese maiuscolo come «NOT FOUND»/«INFINITE»/«PARALLEL» delle schermate) |
| Retta d'intersezione piano–piano | mostrata in forma vettoriale r⃗ = P + t·d⃗ e disegnata |
| Combinazioni Next/Back | coppie nell'ordine delle righe (1,2), (1,3), (2,3) |
| Passo di CROSS | (max − min)/(grid − 1) dell'asse (Z con Xgrid) |
| TRACE sui modelli | sul reticolo del grafico (retta: Xgrid punti del segmento visibile) |
| SKETCH | punti e testi in coordinate dello schermo, cancellati da Cls, da un nuovo disegno e da ogni cambio di vista |
| Lista vuota al DRAW | scatola vuota (nessun errore) |
| ZOOM AUTO sull'intervallo z | conservato come estensione in OPTN (era della v1.9.0) |

## 4. Vincoli di memoria e prestazioni per scheda (`limits.hpp`)

| Risorsa | Pico 2 W (solo SRAM, ≈ 194 KB di heap) | Pico Plus 2 W (PSRAM 8 MB) |
|---|---|---|
| Punti per lato di un grafico | fino a 25 (24 intervalli) | fino a 50 (limite del manuale) |
| Vertici di una scena (3 grafici, cilindro/cono con le basi) | ≤ 3 × 3 × 25² ≈ 5 600 × 16 B ≈ 90 KB nel caso peggiore, tipico < 30 KB | ≤ 3 × 3 × 50² × 16 B ≈ 360 KB in PSRAM |
| Vertici proiettati | 12 B per vertice | 12 B per vertice (PSRAM) |
| Z-buffer | no (pittore: 8 B per quadrilatero) | 320 × 282 × 2 B = 180 KB in PSRAM |
| Immagine della scena per TRACE/SKETCH (cache) | no (ridisegno) | 180 KB in PSRAM |
| 3D G-Mem (20 insiemi) | 12 KB in tutto | 64 KB in tutto |
| 3D V-Win (6) | 6 × 104 B | 6 × 104 B |

- Calcolo: 600 valutazioni per tick (come la v1.9.0), quindi la tastiera
  resta reattiva; AC ferma il calcolo. I modelli (Line, Plane, Sphere,
  Cylinder, Cone) non valutano espressioni: il loro reticolo è analitico.
- Ogni allocazione passa da `mem::resource(PREFER_PSRAM)`; un `bad_alloc`
  dimezza la griglia (fino a 2 punti) e poi dà «Memory ERROR» senza trattenere
  memoria; lo z-buffer che non trova spazio lascia il posto al pittore.
- Tempi stimati sul Cortex-M33 a 150 MHz (da misurare, HW-170…): z = f(x,y)
  con 25 × 25 punti ≈ 625 valutazioni ≈ 0,1–0,3 s; 50 × 50 ≈ 2 500
  valutazioni ≈ 0,5–1 s; proiezione e disegno di 3 grafici 50 × 50 con lo
  z-buffer ≈ 0,2–0,5 s per vista.

## 5. Formato del record

Il modello cambia (tipi, moduli, coefficienti, colori, griglie, S/T, θ/φ,
Setup, 3D G-Mem, 3D V-Win): record **v10**. Il blocco 3D della versione 5
(in mezzo al record) non viene più scritto; il nuovo blocco 3D va **in coda**
al v9, come è stato fatto per EQUATION nella v8. I record v1–v4 non hanno
dati 3D (valori iniziali); i record v5–v9 convertono le funzioni Z1–Z3 in
grafici Z= con la stessa selezione, la finestra X/Y/Z, la griglia (celle
5–128 → punti 2–50), la vista (yaw/pitch → θ/φ con la stessa orientazione) e
lo zoom. Prove: fixture v1.6.0…v1.12.0 già presenti e la nuova
`workspace_v1_13_1_graph3d.inc` (v9 con dati 3D non predefiniti), andata e
ritorno del record v10, record danneggiati rifiutati.

## 6. Vincoli e come vengono rispettati

| Vincolo | Rispetto |
|---|---|
| Un solo parser e valutatore | funzioni Z=, Param, Rot e coefficienti passano da `CompiledExpression`/`evalNumber`; X, Y, S, T legate con `EvalContext::bind` (A–Z non cambiano) |
| Geometria pura e testabile | `graph3d_model.hpp` (modello, validazione, geometria canonica, intersezioni e relazioni analitiche, sezioni, G-Mem) senza grafica; `graph3d.hpp` (reticoli, taglio, proiezione, z-buffer/pittore) senza UI |
| Controller e UI separati | `src/apps/graph3d_app.cpp` (lista, tipi, coefficienti, V-Window, Setup, FORMAT, memorie) e `src/apps/graph3d_plot.cpp` (grafico, TRACE, G-Solve, SKETCH, rotazione, immagini); `architecture_test` aggiornato |
| Memoria embedded | niente oggetti grandi sullo stack; buffer grandi da `mem::resource(PREFER_PSRAM)`; tutto rilasciato all'uscita e dopo un errore |
