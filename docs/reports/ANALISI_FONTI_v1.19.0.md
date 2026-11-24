# Analisi delle fonti — v1.19.0: Geometry alla parità completa

Scritto **prima** del codice della v1.19.0 (fase 0). Obiettivo della versione:
**Geometry** (capitolo 14 del manuale fx-CG50) alla **parità completa** sulla
Pimoroni Pico Plus 2 W; la modalità passa da **Base** ad **Available**. Ogni
voce della tabella del §3 è presente con un test dedicato oppure documentata
come differenza inevitabile (§6).

Regole che non cambiano ([CONTINUE.md](../../CONTINUE.md)): manuale fx-CG50
v3.60 solo come descrizione del comportamento (nessun codice, grafica o testo
copiato da Casio); **Raspberry Pi Pico 2 W congelato alla v1.16.0** (Geometry
non vi esiste: nessun byte nuovo nella sua immagine salvo la stringa della
versione); codice nuovo solo sulla Plus dietro `#if FXAI_BOARD_PICO_PLUS_2W` /
`FXAI_MODE_GEOMETRY`; cancello completo; un solo commit su `main` in
fast-forward («PicoCalc FxCG-50 v1.19.0: …»); HARDWARE_TEST da **HW-272**;
nessun «TESTED ON HARDWARE» senza prova fisica. Le domande tecniche hanno una
risposta scelta qui (§7) senza chiedere conferme, come richiesto.

## 1. Fonti

| # | Fonte | Uso |
|---|---|---|
| 1 | *fx-CG50 Software User's Guide* v3.60, capitolo 14 (Geometry), pagine 14-1…14-66 | descrizione del comportamento. Il PDF non è scaricabile da questo container (come nella v1.18.0); le pagine 459–525 della copia HTML di manualslib (manuale 3093535, pagina 14-N = pagina 458+N) sono state lette **attraverso riassunti** (il testo arriva troncato: i tasti del font Casio sono glifi e alcune associazioni sono incerte). Nessun testo è stato copiato: le voci sotto sono descrizioni con parole nostre; i punti incerti sono le domande del §7 |
| 2 | Codice a `main` = `a7c861b` (v1.18.0) | punto di partenza (§2) |
| 3 | Geometria analitica elementare (intersezioni, trasformazioni affini, aree di Gauss) | formule del nucleo; nessuna libreria esterna |

Comportamento ricavato dalla fonte 1 (riassunto):

| Area | Comportamento della fx-CG50 |
|---|---|
| Tasti dei menu | nessuna barra in basso: F1 apre **File** (con la scheda **View**), F2 **Edit**, F3 **Draw** (con la scheda **Draw Spec**), F4 **Construct**, F5 **Transform**, F6 **Animate**, OPTN il menu **Option** (con la scheda **Properties**); ◀ ▶ cambiano scheda, le cifre scelgono la voce, EXIT chiude |
| File | 1 New, 2 Open (elenco, DELETE con conferma, memoria di archiviazione), 3 Save as (nome di 8 caratteri A–Z, 0–9, { }), 4 Key Help (tre schermate) |
| View | 1 Zoom Box (due angoli), 2 Pan (le frecce spostano la vista, EXIT), 3 Scroll (tasto «.»), 4 Zoom In (tasto «+», ×2), 5 Zoom Out (tasto «−», ×½), 6 Zoom to Fit (non vale per f(x)); V-Window con SHIFT F3 (Xmin, Xmax, Ymid, INITIAL); SET UP con Grid (On a punti, Off, Line), Grid Space (0,01…1000), Axes (On, Off, Scale), Angle (Deg, Rad), Length Unit (mm, cm, m, km, inch, feet, yard, mile, Off) |
| Edit | 1 Undo/Redo (subito dopo l'operazione; di nuovo subito dopo = Redo; non annullabili Clear All, vista, zoom, scroll, pan, SET UP), 2 Select All, 3 Deselect All (anche AC), 4 Select Figure (anche un tasto diretto), 5 Delete (anche DEL), 6 Clear All (conferma, «non annullabile») |
| Draw | 1 Point (resta attivo fino a EXIT/AC; sulla linea mette il punto sulla linea), 2 Line Segment, 3 Infinite Line, 4 Ray, 5 Vector (due punti; un punto esistente si riusa), 6 Circle (centro e punto della circonferenza), 7 Arc (centro, inizio, fine, in senso antiorario), 8 SemiCirc (Diam) (estremi del diametro, antiorario) |
| Draw Spec | 1 Triangle, 2 Isosc Triangle, 3 Rectangle, 4 Square (primo punto, poi un riquadro col secondo; il quadrato prende il lato minore; punti troppo vicini = la figura più grande che sta nello schermo), 5 Polygon (EXE per vertice, EXE sul primo chiude, EXIT lascia la spezzata aperta), 6 Regular n-gon (3–12 lati, poi come il triangolo), 7 Function f(x) (solo Y=f(x), sempre in radianti) |
| Construct | si seleziona prima, poi si sceglie: 1 Perp Bisector (segmento, lato o due punti), 2 Perpendicular (linea + punto), 3 Midpoint (segmento, lato o due punti), 4 Intersection (due tra segmento, retta, semiretta, vettore, lato, circonferenza, arco), 5 Angle Bisector (due linee che si incontrano: due bisettrici), 6 Parallel (linea + punto, con i segni di parallelismo), 7 Tangent («Select Curve», punto sulla curva: circonferenza, semicirconferenza, arco, grafico di f(x)), 8 Attached Angle (due lati: misura dell'angolo posata dove si sceglie, «Select Display Position») |
| Transform | 1 Reflection («Select Axis»: segmento, retta, semiretta, lato), 2 Translation (X e Y), 3 Trans(Sel Vec) («Select Vector»), 4 Rotation («Sel Rotation Center», angolo antiorario), 5 Dilation («Sel Dilation Center», fattore con valore assoluto 0,1…10), 6 Symmetry («Select Center Point», 180°); una selezione parziale trasforma solo quella parte |
| Selezione | il puntatore vicino a un oggetto lo evidenzia, EXE lo seleziona (spesso) o lo deseleziona; stato in alto («One vertex selected», «Three sides selected»); un tasto fa partire lo spostamento della selezione con le frecce, EXE lo posa; senza selezione lo stesso tasto sposta la vista |
| Casella di misura | in basso; tipi secondo la selezione: coordinate, distanza/lunghezza, pendenza, equazione, espressione, vettore, raggio, circonferenza, perimetro, area, angolo (e supplementare), tangenza, congruenza, incidenza, angolo di rotazione, fattore di dilatazione, etichetta; scrivere un valore + EXE cambia la figura **e blocca** la misura; icona del lucchetto (Lock/Unlock); **Paste** incolla la misura sullo schermo come testo che si aggiorna (con un'etichetta di 14 caratteri al più) |
| Option | 1 Text (31 caratteri), 2 Expression (con @1, @2… delle misure incollate, @nX/@nY), 3 Number Format (della misura scelta; iniziale Fix2; gli interi senza decimali), 4 Clr Constraint (toglie tutti i blocchi, anche quelli automatici delle figure), 5 Show All, 6 Hide, 7 Area Calc (aree delle figure colorate) |
| Properties | 1 to the front, 2 to the back, 3 All TEXT, 4 Fade I/O (luminosità dello sfondo), 5 Store Picture (Pict 1–20 o file) |
| FORMAT | SHIFT 5 sulla selezione: Line Style (Normal, Thick, Thin), Line Color, Area Color (con Clear), Paint Style (Normal, Lighter), Char Color |
| Animate | 1 Add Animation (punto + segmento, retta, circonferenza, arco o funzione; più punti insieme), 2 Replace Animation, 3 Trace, 4 Edit Animation (per punto: Steps 2–100, iniziale 20; t0, t1 −10…10 con 0 = A, 1 = B; Times, 0 = finché non si ferma; DELETE di una riga o di tutte), 5 Go (once), 6 Go (repeat) (stop con EXIT o AC), 7 Add Table (una colonna per misura, al più 26), 8 Display Table (STORE › LIST n 1–26, STORE › S-SHT, DELETE, DEL-ALL con conferma) |

### 1.1 Punto di partenza verificato

| Controllo | Esito |
|---|---|
| `git status`, `git fetch origin main` | branch `ccr-6804dfc4-cwp8zw` = `main` = `a7c861b`, albero pulito |
| `tests/run_host_tests.sh` | **62/62** (Plus, 2 min 19 s con 4 processori) e **54/54** (Pico 2 W) |
| Firmware della v1.18.0 ricompilato | Plus **2 807 216 B** (66 % di 4 MB; budget 3 774 873 B: restano 967 657 B); Pico 2 W **1 944 228 B** (budget 2 064 384 B), 179 oggetti esclusi, nessuno nell'immagine; nessun warning |
| Strumenti | GCC 13 host, clang 18 (libFuzzer), arm-none-eabi-gcc, Pico SDK 2.2.0 in `/opt/sdk/pico-sdk` |

## 2. Cosa c'è oggi (v1.17.0, Base)

| Parte | Stato |
|---|---|
| Nucleo `geometry.hpp/.cpp` (29 228 B con il controller) | 5 tipi (punto, segmento, retta, circonferenza, poligono) con **coordinate copiate** in ogni oggetto: nessun punto condiviso, nessuna dipendenza; misure come testo; vista (centro, scala); sezione `0x0110` e file `.pcgeo` **versione 1** (≤ 500 oggetti) |
| Controller `geometry_app.cpp` | barra DRAW SELECT MOVE MEASURE VIEW FILE; cursore a passo «rotondo»; MOVE di un oggetto; ZOOM ±1,5×, PAN, RESET; NEW/OPEN/SAVE·AS |
| Test | `base_modes_test` (nucleo), `base_flow_test` (flusso), `apps_init_test` (sequenza e sonda), `workspace_v117_test`, `fuzz_modes` (sezione e file); screenshot 144 |

Tutto il resto del §3 manca: grafo delle dipendenze, costruzioni, trasformazioni,
misure con blocco, animazioni, testo, etichette, formati, file v2.

## 3. Tabella di parità del capitolo 14 (voce per voce)

Legenda dello **stato v1.17.0**: ✔ presente come sulla fx-CG50, ◐ parziale,
✘ assente. **Test**: ogni voce ha il suo caso con l'id nel nome in
`tests/geometry_test.cpp` (**GT**, nucleo puro) e/o in
`tests/geometry_flow_test.cpp` (**GF**, controller dal firmware simulato, tasti
veri). Le colonne «Test» dicono cosa si controlla.

### 3.1 Tasti e menu (14-1…14-6)

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| K01 | F1 **File** con la scheda **View** (◀ ▶), F2 **Edit** con le schede **Option** e **Properties**, F3 **Draw** con **Draw Spec**, F4 **Construct**, F5 **Transform**, F6 **Animate**: menu a tendina con voci numerate, cifre e frecce per scegliere, EXIT chiude | ✘ barra a sottomenu | GF: ogni menu, ogni scheda, scelta con la cifra e con EXE, EXIT |
| K02 | Puntatore: frecce (SHIFT = passo ×4), cifre 1–9 = salto al settore 3×3 dello schermo, coordinate in alto; il puntatore vicino a un oggetto lo evidenzia; aggancio ai punti esistenti | ◐ cursore a passo | GF: passo, salti 1–9, evidenziazione, aggancio |
| K03 | Tasti diretti: «+» Zoom In, «−» Zoom Out, «.» Scroll, «x» Select Figure, «*» Undo/Redo, «v» sposta la selezione (senza selezione: Pan), DEL Delete, AC Deselect All / ferma l'animazione, OPTN casella di misura, SHIFT F3 V-Window, SHIFT MENU SET UP, SHIFT 5 FORMAT, MENU esce | ✘ | GF: un caso per tasto |

### 3.2 File (14-6…14-9)

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| F01 | **New**: con una costruzione sullo schermo chiede conferma, poi tutto vuoto (vista iniziale, nessun file) | ◐ conferma | GF |
| F02 | **Open**: elenco dei `.pcgeo` (formato v2 e v1 della v1.17.0), DELETE con conferma; con una costruzione aperta chiede conferma | ◐ senza DELETE | GF (apertura, DELETE, file v1) |
| F03 | **Save as**: nome fino a 8 caratteri (A–Z, 0–9, { }), sovrascrittura con conferma | ◐ | GF |
| F04 | **Key Help**: tre schermate con i tasti del disegno, ◀ ▶ / ↑ ↓ per cambiarle, EXIT | ✘ | GF: tre pagine, EXIT torna |
| F05 | **Option** (richiesta della versione; sulla fx-CG50 nel SET UP): Number Format iniziale, unità degli angoli, Length Unit, Grid, Grid Space, Axes — lo stesso modulo di SHIFT MENU (S01) | ✘ | GF: ogni voce cambia il comportamento |

### 3.3 View (14-33…14-37)

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| V01 | **Zoom Box**: primo angolo EXE, riquadro che segue il puntatore, secondo angolo EXE → il riquadro riempie lo schermo | ✘ | GF: finestra dopo lo zoom |
| V02 | **Pan**: icona in alto, le frecce spostano la vista, EXIT finisce | ◐ | GF |
| V03 | **Scroll** (tasto «.»): il puntatore sparisce, le frecce scorrono, EXIT finisce | ✘ | GF |
| V04 | **Zoom In** («+»): scala ×2 attorno al centro | ◐ ×1,5 | GF |
| V05 | **Zoom Out** («−»): scala ×½ | ◐ | GF |
| V06 | **Zoom to Fit**: tutti gli oggetti visibili nello schermo (non le funzioni) | ◐ RESET | GT (riquadro degli oggetti), GF |
| V07 | **Assi** On / Off / Scale (con le tacche e i numeri) | ◐ sempre | GF: pixel degli assi per i tre valori |
| V08 | **Griglia** Off / On (punti) / Line, **Grid Space** 0,01…1000 | ✘ | GF: pixel della griglia, passo fuori intervallo → errore |
| V09 | **V-Window** (SHIFT F3): Xmin, Xmax, Ymid (Y segue il rapporto 1:1 dei pixel), INITIAL, EXE | ✘ | GF |
| V10 | Voci di menu **Axes** e **Grid** nella scheda View (richiesta della versione: «assi e griglia on/off») | ✘ | GF |

### 3.4 Edit, selezione e spostamento (14-19…14-22)

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| E01 | **Undo/Redo** (anche «*»): annulla l'ultima operazione; scelto subito dopo un Undo la rifà; non annullabili Clear All, vista e SET UP | ✘ | GT (istantanea), GF: disegno → Undo → Redo, Clear All non annullabile |
| E02 | **Select All** | ✘ | GF |
| E03 | **Deselect All** (anche AC) | ◐ | GF |
| E04 | **Select Figure** (anche «x»): tutta la figura dell'oggetto selezionato (vertici, lati, poligono) | ✘ | GT (gruppo della figura), GF |
| E05 | **Delete** (anche DEL): gli oggetti scelti e quelli che ne dipendono | ◐ un oggetto | GT (dipendenti), GF |
| E06 | **Clear All**: conferma con l'avviso «non annullabile» | ✘ | GF |
| E07 | Selezione con EXE (seleziona / deseleziona, tratto spesso), riga di stato «1 vertice selezionato», «3 lati selezionati»… | ◐ un oggetto | GF |
| E08 | **Spostamento** («v», frecce, EXE posa): si muovono i punti liberi della selezione; un vertice di una figura vincolata (rettangolo, quadrato, isoscele, n-gono, fine dell'arco, punto su un oggetto) si muove lungo il suo grado di libertà; tutto ciò che dipende si aggiorna | ◐ un oggetto rigido | GT (ogni tipo di vertice), GF |

### 3.5 Option, Properties e FORMAT (14-5, 14-9, 14-21…14-22, 14-37…14-52)

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| O01 | **Text**: testo di 31 caratteri al puntatore, spostabile, colore del testo | ✘ | GF |
| O02 | **Label**: etichette automatiche dei punti (A, B, C…, poi A1…); modifica dell'etichetta di un oggetto (14 caratteri) dalla scheda Option e dall'icona della casella di misura | ✘ | GT (sequenza delle etichette), GF |
| O03 | **Expression**: espressione con @1, @2… (misure incollate) e @nX, @nY (coordinate e componenti), valutata dal motore unico e aggiornata | ✘ | GT, GF |
| O04 | **Number Format** della misura incollata scelta (Fix 0–9, Sci 1–10, Norm; iniziale Fix2; gli interi senza decimali) | ✘ | GT (formattazione), GF |
| O05 | **Clr Constraint**: toglie tutti i blocchi delle misure e i vincoli automatici delle figure di Draw Spec (rettangolo, quadrato, isoscele, n-gono diventano poligoni liberi) | ✘ | GT, GF |
| O06 | **Show All** | ✘ | GF |
| O07 | **Hide** degli oggetti scelti (non disegnati, non selezionabili, le dipendenze restano) | ✘ | GT, GF |
| O08 | **Area Calc**: somma delle aree delle figure chiuse colorate scelte, incollata come misura che si aggiorna | ✘ | GT, GF |
| O09 | **to the front / to the back**: ordine di disegno | ✘ | GT (ordine), GF (pixel sopra) |
| O10 | **All TEXT**: testi e misure incollate davanti a tutto | ✘ | GT |
| O11 | **Fade I/O**: luminosità dello sfondo 0–100 %; sfondo da un'immagine `.pcpict` (Recall Picture) | ✘ | GF |
| O12 | **Store Picture**: l'area del disegno in Pict 1–20 (gli stessi file di Graph) o in un file | ✘ | GF (file `PICT03.pcpict` e lettura) |
| O13 | **Clear Picture**: toglie lo sfondo (richiesta della versione) | ✘ | GF |
| O14 | **FORMAT** (SHIFT 5 e voce di Properties): Line Style Normal/Thick/Thin/Broken/Dot, Line Color, Area Color (o Clear), Paint Style Normal/Lighter, Char Color, solo le voci che valgono per la selezione | ✘ | GT (stile salvato), GF (pixel nel colore, tratto spesso e tratteggiato) |

### 3.6 Draw e Draw Spec (14-11…14-18)

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| D01 | **Point**: EXE mette un punto; resta attivo fino a EXIT; sopra una linea o una curva il punto **resta legato** all'oggetto (si muove solo lungo di esso) | ◐ libero | GT (punto su segmento, retta, circonferenza, arco, funzione, lato), GF |
| D02 | **Line Segment** (due punti; un punto esistente si riusa) | ◐ | GT, GF |
| D03 | **Infinite Line** | ◐ | GT, GF |
| D04 | **Ray** | ✘ | GT, GF |
| D05 | **Vector** (con la punta) | ✘ | GT, GF |
| D06 | **Circle** (centro, punto della circonferenza) | ◐ | GT, GF |
| D07 | **Arc** (centro, inizio, fine; antiorario) | ✘ | GT, GF |
| D08 | **SemiCirc (Diam)** | ✘ | GT, GF |
| D09 | **3-Point Arc** (per tre punti; voce richiesta dalla versione, in più rispetto alla fx-CG50) | ✘ | GT (allineati → indefinito), GF |
| D10 | **Triangle** dal riquadro di due punti | ◐ poligono | GT, GF |
| D11 | **Isosc Triangle** (resta isoscele) | ✘ | GT (AC = BC dopo ogni spostamento), GF |
| D12 | **Rectangle** (resta rettangolo) | ✘ | GT, GF |
| D13 | **Square** (lato = lato minore del riquadro; resta quadrato) | ✘ | GT, GF |
| D14 | **Polygon** (EXE per vertice, EXE sul primo chiude, EXIT = spezzata aperta) | ◐ | GT, GF |
| D15 | **Regular n-gon** (3–12 lati, resta regolare) | ✘ | GT, GF |
| D16 | **Function f(x)** (Y=f(x), radianti, disegnata sulla finestra) | ✘ | GT (valori, errori del testo), GF |
| D17 | Riquadro troppo piccolo → la figura più grande che sta nello schermo | ✘ | GT |

### 3.7 Construct (14-22…14-26)

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| C01 | **Perp Bisector** (segmento, lato o due punti) | ✘ | GT (equidistanza), GF |
| C02 | **Perpendicular** (linea + punto) | ✘ | GT (prodotto scalare 0), GF |
| C03 | **Midpoint** (segmento, lato o due punti) | ✘ | GT, GF |
| C04 | **Intersection** di due oggetti tra segmento, retta, semiretta, vettore, lato, circonferenza, arco, semicirconferenza: tutti i punti (0–2) | ✘ | GT (ogni coppia di tipi), GF |
| C05 | **Angle Bisector** di due linee che si incontrano: due bisettrici | ✘ | GT (angoli uguali), GF |
| C06 | **Parallel** (linea + punto, segni di parallelismo) | ✘ | GT, GF |
| C07 | **Tangent** («Select Curve», punto sulla curva: circonferenza, arco, semicirconferenza, f(x)) | ✘ | GT (tangente ⟂ raggio; pendenza = f′), GF |
| C08 | **Attached Angle** (due lati: misura dell'angolo posata con «Select Display Position») | ✘ | GT, GF |
| C09 | **Dipendenze**: gli oggetti costruiti restano legati ai genitori; spostando un punto tutto si aggiorna, con **ricalcolo incrementale** (solo i discendenti) | ✘ | GT (contatore delle valutazioni, incrementale = completo), GF |
| C10 | **Casi degeneri**: rette parallele, cerchi concentrici, punti coincidenti → oggetto **indefinito** (non disegnato, misure «Undefined»), che torna quando la configurazione lo permette | ✘ | GT |
| C11 | Selezione non adatta → messaggio (che cosa selezionare) | ✘ | GF |

### 3.8 Transform (14-26…14-29)

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| T01 | **Reflection** (asse: segmento, retta, semiretta, vettore, lato) | ✘ | GT, GF |
| T02 | **Translation** con i valori X, Y | ✘ | GT, GF |
| T03 | **Trans(Sel Vec)** con un vettore selezionato | ✘ | GT, GF |
| T04 | **Rotation** (centro, angolo antiorario nell'unità degli angoli) | ✘ | GT, GF |
| T05 | **Dilation** (centro, fattore con 0,1 ≤ \|k\| ≤ 10) | ✘ | GT (fuori intervallo → errore), GF |
| T06 | **Symmetry** (centro: rotazione di 180°; un centro nuovo viene disegnato) | ✘ | GT, GF |
| T07 | L'**immagine resta legata** all'originale e alla trasformazione (spostare un vertice, l'asse, il centro o il vettore aggiorna l'immagine) | ✘ | GT, GF |
| T08 | Selezione parziale: solo quella parte | ✘ | GT |
| T09 | Proprietà: riflessione due volte = identità; la rotazione conserva le distanze; la dilatazione di fattore k moltiplica le lunghezze per \|k\| e l'area per k²; la traslazione conserva i vettori; Symmetry = rotazione di 180°; due riflessioni su rette che si incontrano con angolo θ = rotazione di 2θ | ✘ | GT (proprietà su costruzioni casuali, §8.2) |

### 3.9 Casella di misura (14-37…14-52)

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| M01 | **Casella** in basso (OPTN o EXIT; EXIT o OPTN di nuovo la chiude): tipo scelto (← → o F1 TYPE), valore, lucchetto, azioni (F2 LOCK, F3 PASTE, F4 TABLE, F5 LABEL); si aggiorna con la selezione | ◐ finestra MEASURE | GF |
| M02 | **Coordinate** di un punto (bloccabili: il punto resta fisso) | ◐ | GT, GF |
| M03 | **Distanza / lunghezza** (due punti; segmento, vettore, lato) | ◐ | GT, GF |
| M04 | **Pendenza** (linea) | ◐ nell'equazione | GT |
| M05 | **Direzione** (angolo con l'asse x di una linea o di un vettore) | ◐ | GT |
| M06 | **Angolo** tra due linee (o con tre punti, al vertice centrale) | ◐ poligono | GT, GF |
| M07 | **Supplementare** | ✘ | GT |
| M08 | **Incidenza** di un punto su una linea, circonferenza, arco o vettore (Sì/No; bloccata: il punto resta sopra) | ✘ | GT |
| M09 | **Tangenza** (retta–circonferenza, circonferenza–circonferenza; bloccata: restano tangenti) | ✘ | GT |
| M10 | **Congruenza** di due segmenti (bloccata: stessa lunghezza) | ✘ | GT |
| M11 | **Area** (poligono, lati che chiudono una figura, circonferenza; quella della circonferenza bloccabile) | ◐ | GT |
| M12 | **Perimetro** | ◐ | GT |
| M13 | **Equazione** (retta, segmento, semiretta, circonferenza, arco, f(x)) | ◐ | GT |
| M14 | **Componenti del vettore** (bloccabili) | ✘ | GT |
| M15 | **Raggio** (bloccabile) | ◐ | GT |
| M16 | **Circonferenza** (lunghezza della circonferenza o dell'arco) | ◐ | GT |
| M17 | **Angolo di rotazione** e **fattore di dilatazione** (un punto e la sua immagine) | ✘ | GT |
| M18 | **Valore scritto + EXE** cambia la figura (spostamento minimo) e **blocca** la misura | ✘ | GT (spostamento minimo, residuo), GF |
| M19 | **Lucchetto**: Lock / Unlock; le misure bloccate restano vere spostando i punti (gli altri punti si adattano) | ✘ | GT (dopo ogni spostamento), GF |
| M20 | **Paste**: misura incollata come testo con l'etichetta del tipo (14 caratteri, modificabile), spostabile, **aggiornata** a ogni cambiamento | ✘ | GT, GF |
| M21 | Blocchi incompatibili → «Math ERROR» e nessun cambiamento | ✘ | GT, GF |
| M22 | **Etichetta** dalla casella (icona Label) | ✘ | GF |

### 3.10 Animate (14-55…14-66)

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| A01 | **Add Animation**: punto + segmento, retta, semiretta, vettore, lato, circonferenza, arco o funzione; più punti animati insieme | ✘ | GT (posizioni per ogni percorso), GF |
| A02 | **Replace Animation** | ✘ | GT, GF |
| A03 | **Trace**: i punti scelti lasciano la traccia (sostituisce i punti con traccia di prima) | ✘ | GT (punti della traccia), GF (pixel) |
| A04 | **Edit Animation**: per punto Steps (2–100, iniziale 20), t0 e t1 (−10…10; 0 = inizio, 1 = fine), Times (0 = senza fine); DELETE di una riga, DEL-ALL | ✘ | GT (limiti), GF |
| A05 | **Go (once)** | ✘ | GT, GF |
| A06 | **Go (repeat)** (Times volte, 0 = finché AC/EXIT) | ✘ | GT, GF |
| A07 | **Add Table**: la misura della casella diventa una colonna (al più 26) | ✘ | GT, GF |
| A08 | **Display Table**: una riga per passo con i valori delle colonne | ✘ | GT, GF |
| A09 | Tabella › **STORE › LIST** n (1–26) | ✘ | GF |
| A10 | Tabella › **STORE › STAT** (richiesta della versione: tutte le colonne in List 1… e apertura di Statistics) | ✘ | GF |
| A11 | Tabella › **DELETE** di una colonna, **DEL-ALL** con conferma | ✘ | GF |
| A12 | Animazione e calcolo della tabella in **step::Task** con AC («Break»); alla fine i punti tornano dov'erano | ✘ | GT (task), GF (AC a metà) |

### 3.11 SET UP, persistenza e limiti

| Id | Voce | v1.17.0 | Test |
|---|---|---|---|
| S01 | **SET UP** (SHIFT MENU): Grid, Grid Space, Axes, Angle (Deg, Rad), Length Unit (Off, mm, cm, m, km, inch, feet, yard, mile), Number Format iniziale | ✘ | GF |
| P01 | **`.pcgeo` versione 2** (intestazione con la versione, oggetti con definizioni e genitori, blocchi, animazioni, tabella, opzioni, vista); andata e ritorno identica | ◐ v1 | GT |
| P02 | **Lettura dei file della v1.17.0** (versione 1): migrazione in punti liberi e figure legate ai loro punti | ✘ | GT con una fixture prodotta dal codice della v1.17.0 (`tests/fixtures/geometry_v1_17_0.inc`) |
| P03 | Sezione **`0x0110` versione 2** del record v12 (la v1 della v1.17.0 si legge; danneggiata → isolata); striscia Geometry di eActivity | ◐ v1 | GT, `workspace_v117_test`, `eactivity_test` |
| P04 | **Limiti**: oggetti (1 000 con la PSRAM, 250 solo SRAM), vertici di un poligono (50), animazioni (16), colonne (26), righe (100), blocchi (64), testi (31), etichette (14) → «Memory ERROR» / «Argument ERROR» senza perdita | ◐ 500 oggetti | GT, GF |
| R01 | Registro: Geometry **Available** (nessun `missing`), servizi e dati dichiarati; Pico 2 W invariato | ✘ Base | `architecture_test`, `apps_init_test` |

## 4. Progetto (solo Pimoroni Pico Plus 2 W)

### 4.1 Struttura

| Parte | File | Note |
|---|---|---|
| Nucleo puro | `include/geometry.hpp`; `src/geometry.cpp` (oggetti, grafo, ricalcolo, intersezioni), `src/geometry_build.cpp` (Draw, Construct, Transform, Delete, Clr Constraint, spostamenti), `src/geometry_measure.cpp` (misure, blocchi e risolutore), `src/geometry_anim.cpp` (animazioni, traccia, tabella), `src/geometry_file.cpp` (sezione e file v1/v2) | nessuna interfaccia, nessun workspace: funzioni su `geo::State`; servizio **ModeCore** (solo `geometry.hpp`, `mode_codec.hpp`, `limits.hpp`) più Expression (f(x), Expression) e StepTask |
| Controller | `src/apps/geometry_app.cpp` (tasti, menu, strumenti, casella, moduli), `src/apps/geometry_view.cpp` (disegno: assi, griglia, oggetti, stili, sfondo, tabella) | sul `ModeShell` (PopupMenu, AskBox, ConfirmBox, Form), `TypedFileMenu`, `step::Runner` |

### 4.2 Modello: oggetti, definizioni, grafo delle dipendenze

Ogni oggetto ha un **id** stabile (u16), un **tipo** (Point, Segment, Line, Ray,
Vector, Circle, Arc, Polygon, Function, Text, Measure, Expression, AngleMark,
Transform), una **definizione** (come si calcola), i **genitori** (id), i
propri **scalari liberi**, lo stile, l'etichetta, la figura di appartenenza e
l'ordine di disegno. La geometria calcolata (punto; due punti per le linee;
centro, raggio, angolo iniziale e ampiezza per circonferenze e archi) è una
cache di esecuzione, non salvata.

| Definizione | Genitori | Scalari | Geometria |
|---|---|---|---|
| Free | — | x, y | il punto |
| On (punto su un oggetto) | curva | t | punto della curva al parametro t (linee: A + t·(B − A); circonferenza e arco: angolo; funzione: x = t) |
| Midpoint | A, B (o un segmento) | — | (A + B)/2 |
| Intersection | c1, c2; ramo k | — | k-esima intersezione (§4.4), indefinita se non c'è |
| Image | P, trasformazione | — | T(P) |
| RectVertex / SquareVertex / IsoApex / NgonVertex / ArcEnd | i punti della figura | w, h, φ secondo il tipo | vertici vincolati di Draw Spec e fine dell'arco |
| Segment, Line, Ray, Vector | A, B | — | per due punti (indefiniti se A = B) |
| PerpBisector, Perpendicular, Parallel, AngleBisector (ramo), Tangent | secondo la voce | — | rette costruite |
| Circle | C, P | — | centro C, raggio \|CP\| |
| Arc, Semicircle, Arc3 | C, S, E / A, B / A, M, B | verso | archi antiorari (o orari dopo una riflessione) |
| Polygon | V1…Vn | aperto/chiuso | lati come segmenti della stessa figura |
| Function | — | testo dell'espressione | y = f(x) in radianti |
| Transform | asse / centro / vettore | dx, dy / angolo / k | Reflection, Translation, TransVec, Rotation, Dilation, Symmetry |
| Text, Measure, Expression, AngleMark | misurati / — | posizione, testo, formato | testi, misure incollate e angoli posati |

**Invariante**: i genitori hanno un id **minore** del figlio e gli oggetti sono
in ordine di id, quindi l'ordine del vettore è un ordine topologico. Il
**ricalcolo incrementale** segna sporchi gli oggetti cambiati e, in un solo
passaggio in avanti, quelli con un genitore sporco; valuta solo quelli
(`recompute` restituisce quante valutazioni ha fatto: i test confrontano
l'incrementale con il completo). La cancellazione toglie anche i discendenti
(stesso passaggio) e i blocchi, le animazioni e le colonne che li citano.

Le **immagini** di una trasformazione sono punti `Image` (uno per ogni punto
della chiusura della selezione) e figure dello stesso tipo costruite su quei
punti; la riflessione inverte il verso degli archi. Le funzioni non si
trasformano (§7 Q9).

### 4.3 Blocchi e risolutore

Un blocco è (tipo di misura, operandi, valore). Le coordinate bloccate di un
punto libero lo rendono costante; gli altri blocchi sono equazioni
`g(x) = v` sugli scalari liberi **antenati** degli operandi. Dopo ogni modifica
(valore scritto, spostamento) il risolutore fa una **proiezione di
Gauss–Newton a norma minima pesata**: Δx = −W⁻¹Jᵀ(JW⁻¹Jᵀ + λI)⁻¹ r, con J per
differenze centrali, ricerca lineare a dimezzamenti, al più 60 iterazioni; i
punti che l'utente sta spostando pesano 10⁴ (si muovono gli altri). Se non
converge, lo stato torna com'era: «Math ERROR: vincoli incompatibili» per un
valore scritto, nessun movimento per uno spostamento. Il risolutore è un
`step::Task` (un'unità = una valutazione del grafo): AC lo interrompe e
rimette lo stato iniziale. I vincoli delle figure di Draw Spec non passano dal
risolutore: sono definizioni (RectVertex…), e i loro vertici si spostano in
forma chiusa lungo il loro grado di libertà.

### 4.4 Intersezioni e tolleranze dichiarate

Scala `s` = max(1, |coordinate in gioco|).

| Caso | Regola | Tolleranza |
|---|---|---|
| retta–retta | parallele (anche coincidenti) se \|sin θ\| tra le direzioni ≤ ε | ε = 1e−12 |
| retta–circonferenza | tangente (una radice doppia) se \|d − r\| ≤ ε·s | ε = 1e−9 |
| circonferenza–circonferenza | concentriche se \|C1C2\| ≤ ε·s (nessuna intersezione); tangenti se \|d − (r1 + r2)\| o \|d − \|r1 − r2\|\| ≤ 1e−9·s | ε = 1e−12 |
| segmento, semiretta, vettore, lato | parametro in [−ε, 1 + ε] (semiretta: ≥ −ε) | ε = 1e−9 |
| arco | angolo nell'ampiezza con ±ε rad | ε = 1e−9 |
| punti coincidenti | segmento, retta, … indefiniti se \|AB\| ≤ ε·s | ε = 1e−12 |
| Sì/No di incidenza, tangenza, congruenza | uguaglianza a meno di ε·s | ε = 1e−9 |
| risolutore | \|rᵢ\| ≤ ε·max(1, scala della misura) | ε = 1e−10 |
| test delle proprietà | relativa | 1e−9 |

I rami delle intersezioni sono ordinati per parametro sulla prima curva
(stabili mentre la figura si muove con continuità).

### 4.5 Animazioni

`geo::AnimationTask` (step::Task): un fotogramma per passo; ogni punto animato
prende la posizione del suo percorso a t = t0 + (t1 − t0)·i/(Steps − 1)
(fermo all'ultimo valore se ha meno passi degli altri); ricalcolo incrementale,
punti della traccia, riga della tabella. Go (repeat) riparte da t0 Times volte
(0 = finché AC). Alla fine o con AC i punti tornano dov'erano (lo stato salvato
non cambia). La tabella si calcola anche senza disegno (Display Table prima di
ogni Go).

### 4.6 Sezione `0x0110` e file `.pcgeo` versione 2

| Campo | Contenuto |
|---|---|
| versione (u8) | **2** (la v1 della v1.17.0 si legge e si migra) |
| oggetti | id, tipo, definizione, genitori, scalari, ramo/verso, stile (colori, tratto, area, carattere, nascosto, etichetta visibile), figura, ordine, etichetta, testo |
| blocchi, animazioni, punti con traccia, colonne e righe della tabella | come §4.3, §4.5 |
| opzioni | Number Format, angolo, unità di lunghezza, griglia, passo, assi |
| vista | centro, scala; sfondo (file) e Fade |
| prossimi id | id, figura, etichetta |

Il file ha la stessa codifica senza il nome del file aperto. Decodifica con
**garanzia forte**: valori finiti, genitori esistenti con id minore, tipi dei
genitori compatibili con la definizione, limiti del §3.11 P04; altrimenti il file
è rifiutato («Data ERROR») e la sezione isolata.

**Migrazione v1 → v2**: ogni punto diventa un punto libero; segmento, retta e
circonferenza diventano due punti liberi più la figura legata; il poligono
diventa n punti liberi, n lati e il poligono, tutti della stessa figura; vista e
nome del file restano. Fixture `tests/fixtures/geometry_v1_17_0.inc` (sezione e
file scritti dal codice della v1.17.0).

### 4.7 Registro

Geometry diventa **Available** senza `missing`, con i servizi Expression, Ui,
Storage, ModeShell, TypedFiles, Files (immagini Pict), StepTask, ModeCore e i
dati Settings, Memory, Lists (STORE › LIST e STAT), Geometry; collegamento verso
Statistics (STORE › STAT). Il Pico 2 W non compila nulla di Geometry.

## 5. Memoria e prestazioni (stime)

| Voce | Stima |
|---|---|
| Flash nuova (nucleo + controller) | ~150–200 KB (Statistics: 182 KB), sotto il margine di 967 657 B |
| Stato | ~64 B per oggetto: 1 000 oggetti ≈ 64 KB (PSRAM); 250 senza PSRAM ≈ 16 KB; Undo: una istantanea codificata |
| Ricalcolo | lineare nel numero di oggetti (incrementale: nei discendenti); f(x) campionata per colonna di pixel solo nel disegno |
| Risolutore | (n + 1) ricalcoli per iterazione, n = scalari antenati dei blocchi; in step::Task |

## 6. Differenze inevitabili (previste)

| # | Voce | Motivo |
|---|---|---|
| D1 | Grafica, icone, font e colori propri; menu a tendina disegnati dal progetto; barra in basso con i nomi dei sei menu | nessuna grafica Casio; la barra serve a vedere i menu sulla PicoCalc |
| D2 | Tasti della PicoCalc: F6 = SHIFT+F1, SHIFT = F7, OPTN = F8, SET UP = F10; i tasti diretti della fx-CG50 diventano caratteri («+», «−», «.», «x», «*», «v») | tastiera diversa |
| D3 | La casella di misura si apre con **OPTN** (richiesta della versione) e anche con EXIT come sulla fx-CG50; le voci del menu Option della fx-CG50 sono nelle schede **Option** e **Properties** di F2 Edit; le azioni della casella sono sui tasti F | OPTN è usato per la casella come chiesto |
| D4 | File › **Option** riunisce le impostazioni che la fx-CG50 mette nel SET UP e in Number Format (lo stesso modulo resta in SHIFT MENU) | richiesta della versione |
| D5 | Clr Constraint toglie i blocchi e i vincoli delle figure di Draw Spec, **non** le costruzioni e le trasformazioni, che restano legate | il grafo delle dipendenze è richiesto dalla versione; sulla fx-CG50 anche alcune costruzioni sono vincoli cancellabili |
| D6 | Il primo valore di lunghezza scritto non riscala tutta la costruzione: la figura prende la misura esatta e, se esce dallo schermo, la vista si adatta (Zoom to Fit) | stessa figura finale, nessun cambio nascosto della scala delle altre figure |
| D7 | Sfondi `.pcpict` (non `.g3p`); Store Picture nei file Pict di Graph | formati Casio proprietari |
| D8 | STORE › S-SHT sostituito da STORE › STAT (colonne in List 1… e Statistics); il foglio può prendere le liste con LIST › List→colonna | la modalità non scrive i dati di un'altra modalità (regola del registro) |
| D9 | In più rispetto alla fx-CG50: 3-Point Arc, tratti Broken e Dot, voci Axes e Grid in View, Clear Picture (richieste della versione) | estensioni documentate |

## 7. Domande aperte e risposte scelte

| # | Domanda | Risposta scelta | Motivo |
|---|---|---|---|
| Q1 | Tasto della casella di misura | OPTN (e EXIT a riposo, come la fx-CG50); il menu Option va nelle schede di Edit | D3 |
| Q2 | Undo a più livelli? | No: un livello che si alterna (Undo/Redo) come la fx-CG50; Clear All non annullabile | parità |
| Q3 | Un punto messo su una linea resta legato? | Sì (punto `On` con parametro) e si sposta solo lungo l'oggetto | geometria dinamica; serve alle animazioni |
| Q4 | Etichette automatiche | i punti prendono A…Z, poi A1…Z1… nell'ordine di creazione; le immagini prendono l'etichetta con l'apice (A′) | come le figure del capitolo |
| Q5 | Figure di Draw Spec dal riquadro | triangolo A(sx, basso), B(dx, basso), C(sx + ¼ larghezza, alto); isoscele con C al centro; rettangolo e quadrato sui lati del riquadro; n-gono inscritto nel cerchio di raggio metà del lato minore con il primo vertice in alto | descrizione «riquadro» del manuale, scelte proprie per i vertici |
| Q6 | Arco: dove finisce? | la fine è un punto legato alla circonferenza (ArcEnd): l'arco va dall'inizio, in senso antiorario, fino a quel punto | il terzo clic dà la direzione della fine |
| Q7 | Intersezione senza punti al momento della costruzione | «Math ERROR: nessuna intersezione»; dopo, un'intersezione che sparisce è indefinita e torna quando c'è | nessun oggetto inutile |
| Q8 | Angle Bisector di linee parallele | «Math ERROR: le linee non si incontrano» | il manuale le chiede incidenti |
| Q9 | Trasformare f(x) | non si può («Argument ERROR: f(x) non trasformabile»); testi e misure incollate si saltano | l'immagine non è una funzione in generale |
| Q10 | Unità dell'angolo di Rotation | l'unità di Option (Deg iniziale) | coerente con le misure |
| Q11 | Animazione su retta e semiretta | t come sul segmento (0 = A, 1 = B, fuori da [0, 1] oltre i punti; semiretta t ≥ 0); circonferenza: t = 0 sul punto che la definisce, 1 = un giro antiorario; arco: inizio → fine; funzione: x = t (t0, t1 iniziali −5 e 5) | definizione del manuale per il segmento, estesa |
| Q12 | Go (repeat) | riparte da t0 Times volte (0 = finché AC o EXIT) | semplice e prevedibile |
| Q13 | Dopo l'animazione | i punti tornano dov'erano; la traccia resta fino alla modifica seguente | lo stato salvato non cambia |
| Q14 | Colonne della tabella con due valori (coordinate, componenti) | due colonne (x, y) | le liste sono di numeri |
| Q15 | Testi Sì/No | «Sì» / «No» (incidenza, tangenza, congruenza) | lingua dell'interfaccia del progetto |
| Q16 | Limiti | 1 000 oggetti con la PSRAM, 250 senza; 50 vertici; 16 animazioni; 26 colonne; 100 righe; 64 blocchi | §5 |
| Q17 | Misure delle immagini | Rotation angle e Scale of dilation dal punto e dalla sua immagine; il valore scritto cambia l'angolo o il fattore della trasformazione (senza blocco) | il manuale le dice non bloccabili |
| Q18 | Area Calc | somma delle aree delle figure chiuse scelte con Area Color, incollata come misura | il manuale la limita alle figure colorate |

## 8. Piano dei test, del fuzzing e del simulatore

### 8.1 Suite

| Verifica | Contenuto |
|---|---|
| `geometry_test` (nuova, Plus) | un caso per voce del nucleo (K…R con GT) e le proprietà del §8.2; migrazione v1 con la fixture; tolleranze del §4.4 |
| `geometry_flow_test` (nuova, Plus) | un caso per voce con GF dal firmware simulato (tasti, menu, schermate, pixel, file, liste, Statistics) |
| `base_modes_test`, `base_flow_test`, `apps_init_test`, `workspace_v117_test`, `eactivity_test` | Geometry sul nucleo nuovo (sequenza e sonda di `apps_init_test`; i casi di `base_*` sostituiti dalle suite nuove) |
| `architecture_test` | Geometry Available sulla Plus, servizi e dati; Pico 2 W congelato (23 modalità, nessuna Base nuova) |

### 8.2 Proprietà (su costruzioni casuali con seme fisso)

Riflessione due volte = identità; rotazione: distanze e angoli conservati;
dilatazione k: lunghezze ×|k|, aree ×k²; traslazione: vettori conservati;
Symmetry = rotazione di 180°; due riflessioni = rotazione di 2θ; punto medio
equidistante; intersezioni sopra entrambe le curve; asse equidistante;
tangente ⟂ raggio; bisettrici ad angoli uguali; ricalcolo incrementale =
completo; codifica e decodifica identiche; dopo il risolutore residui entro la
tolleranza.

### 8.3 Fuzzing (≥ 300 s per harness nuovo o toccato)

Nuovo **`fuzz_geometry`**: (a) byte casuali come sezione e come file `.pcgeo`
(v1 e v2): decodifica senza crash, ricodifica identica; (b) **sequenze di
comandi** decise dai byte (disegna, costruisci, trasforma, sposta, blocca,
cancella, Undo, anima a passi, Clr Constraint, codifica) con le invarianti
(ordine topologico, nessun NaN nella geometria definita, residui dei blocchi,
andata e ritorno). Toccati: `fuzz_modes` (sezione v2), `fuzz_workspace`,
`fuzz_ui` (controller), `fuzz_eactivity` (striscia).

### 8.4 Simulatore

`tests/sim/geometry_v119.txt`, screenshot dal **175**: almeno uno per ogni menu
(File, View, Edit, Option, Properties, Draw, Draw Spec, Construct, Transform,
Animate), la casella di misura con un blocco, una misura incollata, FORMAT, Key
Help, SET UP, l'animazione con la traccia, la tabella, gli esempi della guida.
Gli screenshot 01–174 identici salvo il **144** (Geometry della v1.17.0:
rigenerato con lo script adattato ai menu nuovi, motivo nel rapporto).

### 8.5 HARDWARE_TEST

`docs/HARDWARE_TEST_v1.19.0.md` da **HW-272**: tempi del ricalcolo e del
risolutore con 1 000 oggetti, fluidità delle animazioni, f(x) e tratti sul
pannello, Store Picture, file v1 e v2 sulla microSD.
