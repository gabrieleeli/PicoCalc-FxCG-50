# Motore GRAPH e G-Solve (v1.8.0)

La modalità GRAPH è divisa in un motore portabile senza interfaccia e in
un'applicazione che lo disegna. Ogni valore mostrato (curve, TRACE, G-Solve,
tabelle, Modify) viene dallo **stesso parser e valutatore** di Run-Matrix
(`expression.hpp`), con le stesse regole di priorità, funzioni e unità
angolari.

| File | Ruolo |
|---|---|
| `include/graph_model.hpp`, `src/graph_model.cpp` | modello dati: tipi di grafico, funzioni (20), V-Window, Setup della modalità, SKETCH, memorie; nessuna dipendenza dall'interfaccia |
| `include/graph_engine.hpp`, `src/graph_engine.cpp` | viewport, clipping, compilazione della lista delle relazioni, riferimenti tra funzioni, campionamento incrementale, aritmetica di V-Window e ZOOM |
| `include/graph_solve.hpp`, `src/graph_solve.cpp` | ROOT, MAX/MIN, INTSECT, Y-CAL/X-CAL, ∫dx e area |
| `include/graph_file.hpp`, `src/graph_file.cpp` | progetti `.pcgraph` e memorie `PCGMEM` ([PCGRAPH_FORMAT.md](PCGRAPH_FORMAT.md)) |
| `include/pict.hpp`, `src/pict.cpp` | immagini `.pcpict` (RLE RGB565) ed esportazione BMP |
| `src/apps/graph_app.cpp` | ciclo di vita, lista delle relazioni, editor, V-Window, Setup, FORMAT, Modify, menu |
| `src/apps/graph_plot.cpp` | rendering incrementale a livelli, TRACE, ZOOM, SKETCH, G-SOLVE, PAN, incolla |
| `src/apps/graph_files.cpp` | progetti sulla microSD, memorie grafiche, PICT, sfondo |
| `src/apps/table_app.cpp` | TABLE sullo stesso `GraphEngine` (colonne Y e dY/dX, GPH-CON/PLT) |

Il registro delle applicazioni dichiara i servizi `GraphCore` e `Files` e il
dato `Graph` del workspace; `architecture_test` verifica che GRAPH e TABLE
includano solo ciò che dichiarano ([APPLICATIONS_ARCHITECTURE.md](APPLICATIONS_ARCHITECTURE.md)).

## Modello dati

- **Funzioni**: `std::array<GraphFunction, 20>` nel workspace: espressione,
  `GraphType` (12 tipi: Y=, r=, Param, X=, Y>, Y<, Y≥, Y≤, X>, X<, X≥, X≤),
  selezione, colore (7), stile (Normal, Thick, Broken, Dot, Thin), seconda
  espressione (Yt) per le parametriche. Espressione ≤ 1000 caratteri.
- **V-Window**: Xmin, Xmax, Xscale, Ymin, Ymax, Yscale, Tmin, Tmax, Tpitch;
  Xdot è derivato. `validateView` rifiuta finestre vuote o invertite, scale
  negative, pitch nullo o con segno opposto a Tmax − Tmin, valori non finiti o
  oltre 1e100.
- **Setup della modalità** (`GraphSettings`): Func Type, Draw Type,
  Derivative, Coord, Grid, Axes, Label, Graph Func, Simul Graph, Ineq Type,
  Sketch Line, Plot/LineCol, fattori di ZOOM, sfondo e Fade. Angle e Display
  sono le impostazioni globali.
- **Stato** (`GraphState`): Setup, 6 memorie V-Window, 20 memorie grafiche
  (testo `PCGMEM`, ≤ 24 KB in totale), elementi SKETCH (≤ 64), immagine
  richiamata, nome del progetto e flag "modificato".

Tutto il modello è salvato nel workspace in flash (record v3) e, per il
progetto, nel file `.pcgraph`.

## Coordinate

Area del grafico: 320 × 282 pixel, da y = 18 (sotto la barra di stato) a
y = 299 (sopra il menu funzione).

```
Xdot = (Xmax − Xmin) / 319        Ydot = (Ymax − Ymin) / 281
colonna c → x = Xmin + c·Xdot     riga r → y = Ymax − r·Ydot
```

Con la V-Window iniziale (Xmin = −6.3, Xmax = 6.3) Xdot = 12.6/319. TRACE e
ZOOM ROUND/INTEGER lavorano sulle colonne: con Xdot "tondo" le coordinate del
TRACE sono tonde (arrotondamento delle coordinate, manuale 5-56).

`Viewport::clip` è il clipping di Liang–Barsky di un segmento contro il
rettangolo del grafico allargato di 3 pixel; le coordinate dello schermo sono
limitate a ±1e9 prima del clipping, così i valori enormi vicino agli asintoti
non producono overflow negli interi a 16 bit.

## Compilazione della lista delle relazioni

`GraphEngine::build` compila ogni funzione selezionata una sola volta con il
parser condiviso e produce i **plot**:

- **intervallo** `f(x), [a,b]`: il campionamento è limitato all'intervallo
  (per X= l'intervallo è su y, per r= e Param su θ/T);
- **sovrapposizione** `f(x), [A=1,2,3]`: un plot per valore (max 10); il
  primo ha il colore della funzione, i successivi seguono la sequenza blu,
  rosso, verde, magenta, nero;
- **grafici da lista**: un'espressione che usa `List1`…`List6` diventa un plot
  per elemento; liste vuote o di lunghezza diversa nella stessa espressione
  danno "Dimension ERROR" per quella funzione;
- **funzioni composte**: `Y1(x)`, `Y2(Y1(x))`, `r1(θ)`, `Xt1(t)`, `Yt1(t)`,
  `X1(y)`. Nell'espressione sono token atomici (`@Y1`) inseriti da VARS GRAPH;
  il valutatore li risolve tramite `FunctionResolver`. Al massimo 5 livelli,
  contando anche i riferimenti dentro gli argomenti (`Y1(Y2(x))`), come la
  CG50; un ciclo produce "Stack ERROR". `Y1(x, A=2)` assegna temporaneamente
  le variabili A–Z indicate;
- **grafici incollati** (Ctrl+V sul grafico): al massimo 10, insieme alle
  funzioni non più di 30 plot.

Un errore di sintassi segna solo la sua funzione (mostrato nella lista); le
altre vengono disegnate.

## Campionamento

`sampleStep` campiona un plot in modo incrementale, con un budget di
valutazioni per chiamata:

| Tipo | Campioni |
|---|---|
| Y=, disequazioni Y | una colonna per campione (320), o l'intervallo `[a,b]` |
| X=, disequazioni X | una riga per campione (282) |
| r=, Param | da Tmin a Tmax con passo Tpitch, al massimo 20 000 passi |

Tra due campioni consecutivi:

1. se entrambi sono definiti e la distanza sullo schermo è ≤ 4 pixel, si
   traccia il segmento;
2. altrimenti si **dimezza ripetutamente la metà con la corda più lunga**
   (fino a 12 volte, 1/4096 di passo): in un tratto continuo la corda diventa
   più corta di 4 pixel e il segmento viene tracciato con i punti intermedi;
   in un polo o in un salto (tan x, 1/x, int x) resta lunga e la penna si alza;
3. se uno dei due campioni non è definito (√x per x < 0, log, dominio di una
   composta) il **bordo del dominio** è localizzato con 12 bisezioni e la curva
   arriva fino al bordo (un semicerchio tocca l'asse x).

I punti sono interi a 16 bit `(x, y)`; `PenUp` (INT16_MIN) separa i tratti.
Due punti consecutivi uguali non vengono ripetuti. Draw Type **Plot** salva
punti isolati invece di segmenti. Per le disequazioni il motore salva anche il
**bordo** (una y per colonna, o una x per riga) usato dal riempimento.

### Memoria

| Limite | Valore |
|---|---|
| Punti per curva (`MaxCurvePoints`) | 6 000 (24 KB) |
| Punti dei tratti PEN di tutto lo SKETCH (`MaxSketchPenPoints`) | 1 200 (9,6 KB nel record in flash) |
| Punti di tutte le curve insieme (`MaxTotalCurvePoints`) | 24 000 (96 KB): il renderer assegna a ogni curva ciò che le altre non usano (`Curve::limit`) |
| Bordo di una disequazione | 320 float (1,25 KB) |
| Immagine RLE (sfondo o immagine richiamata) | ≤ 96 KB ciascuna |

Una curva che raggiunge il limite si ferma (`truncated`) e il grafico mostra
"Memory ERROR: grafici troppo complessi, disegno parziale" una volta. Se l'heap
si esaurisce comunque (immagini grandi + curve), il disegno si ferma con
"Memory ERROR" senza ripetere l'errore a ogni ciclo. Heap disponibile sul
firmware: circa 194 KB (vedi [BUILD_STATUS.md](BUILD_STATUS.md)).

## Rendering incrementale

`GraphApp::tick` chiama `renderStep` con **600 valutazioni per ciclo**: il
disegno procede a passi, la tastiera resta attiva (TRACE, ZOOM, EXIT funzionano
durante il disegno) e una barra sottile in alto indica l'avanzamento. Con
**Simul Graph On** il budget è diviso tra tutti i grafici (avanzano insieme);
con Off sono disegnati uno dopo l'altro nell'ordine della lista. Il modello è
ricompilato solo quando cambia qualcosa da cui dipende (impronta FNV-1a di
funzioni, variabili A–Z usate, liste, impostazioni, V-Window).

Livelli del grafico, dal basso verso l'alto:

1. immagine di sfondo con la luminosità di Fade I/O;
2. regioni delle disequazioni (tratteggio; Union: ogni regione, Intsect: solo
   la regione comune); il ciclo visita solo i pixel del motivo (¼ dell'area);
3. area dell'integrale ∫dx;
4. griglia (punti o linee);
5. assi, tacche, scala, etichette `x`, `y`, `O` (un asse fuori dalla finestra
   è indicato sul bordo più vicino all'origine);
6. immagine richiamata (RECALL, il bianco è trasparente);
7. curve: Thin 1 px, Normal 2 px, Thick 4 px, Broken (tratteggio 7/4), Dot;
   le disequazioni strette (Y>, Y<, X>, X<) hanno il bordo tratteggiato;
8. elementi SKETCH;
9. punti marcati con EXE (pieni se appartengono al grafico, vuoti per le
   disequazioni strette) con le coordinate;
10. sovrapposizioni: riga della funzione, coordinate, derivata, risultati,
    puntatore, pannello Modify, barra di avanzamento.

PICTURE STORE salva i livelli 1–9 (senza le sovrapposizioni del livello 10)
dopo il disegno completo.

## ZOOM e V-Window

| Funzione | Calcolo |
|---|---|
| BOX | i due angoli diventano la nuova finestra; errore se sono sulla stessa riga o colonna |
| IN / OUT | centro sul puntatore, finestra divisa/moltiplicata per Xfact, Yfact |
| AUTO | Y dai valori campionati tra Xmin e Xmax, scartando il 2% più alto e più basso se lontani dal resto (asintoti) |
| ORIGINAL / PREVIOUS | finestra prima del primo zoom / pila di 16 finestre |
| SQUARE | Xdot = Ydot correggendo X e mantenendo il centro; dalla V-Window anche X-BASE (corretta Y) |
| ROUND | Xdot e Ydot arrotondati a una cifra significativa, finestra allineata a multipli del passo intorno al puntatore |
| INTEGER | Xdot = Ydot = 1, finestra intorno al puntatore |
| TRIG / STANDRD / INITIAL | preset della CG50 (TRIG dipende da Deg/Rad/Gra) |

## TRACE, SKETCH, Modify

- **TRACE**: per Y= il parametro è la colonna; per X= la riga; per r= e Param
  il passo T. ↑/↓ cambia grafico alla stessa x (o dal centro, se il tipo è
  diverso); un numero digitato apre il pop-up del valore; Derivative On mostra
  dY/dX, dX/dY per i tipi X o dr/dθ per le polari (derivata di Ridders sul
  valutatore condiviso; per le parametriche dY/dX = (dY/dT)/(dX/dT)).
- **Tangent/Norm**: direzione dalla derivata del motore anche per X= e
  parametriche; salvate come punto + direzione e ridisegnate alla finestra
  corrente.
- **Modify**: le variabili A–Z della funzione (≤ 5) cambiano con ←/→ di un
  passo; il grafico è ricompilato e ridisegnato a ogni passo.

## G-Solve

Tutte le funzioni lavorano sull'intervallo Xmin…Xmax della finestra e su grafici
Y= o disequazioni Y (come la CG50), con la funzione valutata dal motore.

| Funzione | Metodo |
|---|---|
| ROOT | campionamento su 1 280 punti (4 per colonna); ogni cambio di segno è raffinato con **Brent–Dekker**; un cambio di segno con residuo grande (\|f\| > 1e-7 · scala) è un polo o un salto e viene scartato; un minimo locale di \|f\| senza cambio di segno è raffinato come estremo e accettato se \|f\| ≤ 1e-10 · scala (radici di tangenza, x² in 0) |
| MAX / MIN | minimi locali dei campioni di ±f; raffinamento con la radice di Brent della **derivata di Ridders** quando cambia segno nell'intervallo, altrimenti **minimizzazione di Brent** (interpolazione parabolica); i plateau sono contati una volta |
| INTSECT | ROOT di f − g |
| Y-CAL | valore del grafico in x (errore se non definito) |
| X-CAL | ROOT di f − y: tutte le x, scorribili con ←/→ |
| Y-ICEPT | valore in x = 0 |
| ∫dx | scansione di 64 punti (punti non definiti → errore); ricerca dei **poli** sui massimi locali di \|f\| (1 280 campioni, Brent) con un test di crescita: se \|f\| cresce almeno come 1/\|t−x\| avvicinandosi al punto → "Math ERROR: integrale divergente (polo in x=…)"; i **salti** (int x, funzioni a tratti, individuati per bisezione) e i picchi integrabili (1/√\|x\|) diventano punti di divisione; ogni tratto con **Gauss–Kronrod adattivo** (G7/K15, fino a 18 livelli e 20 000 valutazioni), tolleranza 1e-10 |
| area (∫dx ROOT/INTSECT/MIXED) | integrale del valore assoluto: l'intervallo è diviso alle radici (di f o di f − g) e i pezzi sono integrati separatamente |

Più soluzioni sono ordinate e senza duplicati (al massimo 100); ←/→ passa
alla successiva/precedente; "Not Found" se non ce ne sono. Il risultato di
∫dx va in `Ans`. Valori verificati in `graph_solve_test`, `graph_verify_test` e
`graph_app_test` contro risultati analitici (anche ln x, eˣ, x·e⁻ˣ, x + 1/x,
razionali con poli, gradi e gradienti, ∫₁ᵉ ln x = 1, ∫₀³·⁵ int x = 4,5,
poli rifiutati): radici di x³ − 4x, estremi ±16/(3√3) in ∓2/√3,
∫₋₂⁰ (x³ − 4x) dx = 4, ∫₋π^π sin x dx = 0 con area 4, intersezioni di x + 1 e x²
in (1 ± √5)/2, X-CAL di x³ − 4x = 2,2 (tre soluzioni).

## Precisione verificata

`graph_verify_test` controlla che **ogni punto disegnato sia a meno di 1 pixel**
dalla curva vera e che il punto medio di ogni segmento sia a meno di 1,5 pixel
(la spezzata segue la curva) per 18 casi: polinomi, seno e coseno in radianti
e gradi, eˣ, e^(−x²), 10ˣ, ln, log, √, |x|, una finestra di larghezza 0,002,
1/x, razionali, tan in radianti e gradi, int x; inoltre X=, r= (cerchio e
cardioide, distanza 2D) e parametriche. Lo scarto massimo misurato è 0,49 px.
Le curve continue restano un solo tratto; nessun segmento attraversa poli o
salti; ln x arriva al bordo inferiore vicino a 0; il semicerchio tocca l'asse
x in ±2.

## Stack e prestazioni sul target

Misure `-fstack-usage` con i flag del firmware (arm-none-eabi-gcc 13.2,
`-mcpu=cortex-m33 -O3`, build Release):

| Funzione | Frame |
|---|---|
| `GraphApp::openProject` | 1 712 B |
| `GraphApp::newProject` | 1 664 B |
| `GraphApp::openSetup` | 1 512 B |
| `GraphApp::recallGraphMemory` | 1 472 B |
| `graphfile::decodeMemory` | 1 304 B |
| `ridders` (derivata numerica) | 1 184–1 520 B (foglia) |
| `GraphEngine::build` | 672 B |
| `GraphApp::drawLayers` / `drawOverlays` | 480 / 488 B |
| `graph::integrate` (con la ricerca dei poli) | 616 B |
| `sampleStep` | 368 B |
| funzioni FatFs (`f_rename` il maggiore) | ≤ 288 B |

Nessuna è ricorsiva tranne la valutazione dei riferimenti (≤ 5 livelli,
~230 B ciascuno); il progetto decodificato (~2 KB) è allocato nell'heap.
I tempi di disegno sul Cortex-M33 non sono ancora misurati (nessuna prova su
hardware).
