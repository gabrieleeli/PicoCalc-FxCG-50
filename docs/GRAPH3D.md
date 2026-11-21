# 3D GRAPH — guida della modalità (v1.14.0)

La modalità **3D Graph** (tasto `J` nel menu principale) riproduce il
capitolo 16 "3D Graph Function" del manuale software fx-CG50 (OS 3.x, 3D
Graph versione 1.01, pagine 16-1…16-19) con le parti collegate: SET UP
(1-35…1-38), immagini del grafico (5-20), messaggi dell'Appendice α (α-11,
α-12) e i dati 3DGRAPH, 3DGMEM n, 3DVWIN_n del Memory Manager. L'analisi
delle fonti e la tabella di parità sono in
[reports/ANALISI_FONTI_v1.14.0.md](reports/ANALISI_FONTI_v1.14.0.md); lo
stato finale di ogni voce in [reports/SVILUPPO_v1.14.0.md](reports/SVILUPPO_v1.14.0.md).
I comportamenti che il manuale non descrive riga per riga sono segnati
**(scelta)**; le differenze imposte dalla PicoCalc sono nel §11.

Schermate: [88](screenshots/88_graph3d_select.png) Select Template,
[89](screenshots/89_graph3d_sphere_coef.png) coefficienti della sfera,
[90](screenshots/90_graph3d_sphere.png) esempio 1 (sfera),
[91](screenshots/91_graph3d_z_example2.png) esempio 2 (Z=X²+Y²−3),
[92](screenshots/92_graph3d_list_templates.png) lista con i modelli,
[93](screenshots/93_graph3d_intsect.png) INTSECT retta–retta,
[94](screenshots/94_graph3d_relation.png) RELATION retta–piano,
[95](screenshots/95_graph3d_plane_plane.png) INTSECT piano–piano,
[96](screenshots/96_graph3d_cylinder_cone.png) sfera, cilindro e cono,
[97](screenshots/97_graph3d_cross.png) CROSS,
[98](screenshots/98_graph3d_trace.png) TRACE,
[99](screenshots/99_graph3d_vwin.png) V-Window,
[100](screenshots/100_graph3d_param_rot.png) Param e Rot X,
[101](screenshots/101_graph3d_setup.png) SET UP,
[102](screenshots/102_graph3d_view_z.png) VIEW-Z,
[103](screenshots/103_graph3d_format.png) FORMAT,
[104](screenshots/104_graph3d_wireframe.png) Area Color Clear;
[40](screenshots/40_graph3d_list.png)–[43](screenshots/43_graph3d_auto.png)
superfici z = f(x, y). Script del simulatore: `tests/sim/graph3d.txt`,
`tests/sim/graph3d_v114.txt`.

## 1. Lista delle funzioni (16-6…16-8)

```
3D Graph
1:Sphere                              [—]
  (X-0)²+(Y-0)²+(Z-0)²=2²
2:Z=X²+Y²-3                           [—]
3:                                    [—]
[SELECT][DELETE][TYPE][3D-GMEM][    ][DRAW]
```

- Tre righe, cioè **fino a tre grafici disegnati insieme**. Il `:` evidenziato
  indica che la riga viene disegnata; `[—]` è nel colore del grafico.
  Al primo avvio la lista è vuota (colori: Line Black, Area Blue/Red/Green).
- **F1 SELECT** disegna / non disegnare; **F2 DELETE** (o DEL) chiede
  conferma (F1 Yes, F6 No) e lascia i colori della riga; **F3 TYPE** (o
  ←/→) apre la schermata di selezione; su una riga già registrata ←/→ aprono
  direttamente la sua schermata di input; **F4 3D-GMEM** STORE / RECALL;
  **F6 DRAW** o **EXE** disegna.
- **SHIFT 5** (o OPTN) **FORMAT**: Line Color e Area Color fra Black, Blue,
  Red, Magenta, Green, Cyan, Yellow, White, Clear. Il grafico ha l'Area
  Color, con Area Clear il solo reticolo nel Line Color; entrambi Clear danno
  «Invalid Setting»; la retta usa l'Area Color.
- SHIFT F1/F2/F4/F5 disegnano e aprono TRACE, ZOOM, SKETCH, G-SOLVE;
  **SHIFT F3** V-Window; **SHIFT F6 (G⇔T)** passa al grafico senza ricalcolo
  se nulla è cambiato.
- Estensione: digitando su una riga vuota o Z= si apre l'input di Z= con quel
  carattere.

## 2. Tipi di grafico (16-8…16-13)

Schermata **Select Template** (F3 o ←/→ dalla lista): Line, Plane, Sphere,
Cylinder, Cone (↑↓, EXE apre i coefficienti); **F1 Template**, **F2 Z=**,
**F3 Param**, **F4 Rotate** (F1 Rot X, F2 Rot Y). La schermata si apre sul
tipo della riga.

| Tipo | Moduli (tasti) | Coefficienti | «Invalid Setting» |
|---|---|---|---|
| **Line** | F1 EXPRESS `(X−X0)/a=(Y−Y0)/b=(Z−Z0)/c` · F2 VECTOR `r=r0+t·v` · F3 P&V punto + direzione · F4 POINTS P1, P2 · F5 EDIT | 6 | a=0 o b=0 o c=0; v nullo; direzione nulla; P1 = P2 |
| **Plane** | F1 EXPRESS `aX+bY+cZ+d=0` · F2 VECTOR `r=r0+s·u+t·v` · F3 POINTS P1, P2, P3 · F4 EDIT | 4 / 9 | a=b=c=0; u o v nullo, u e v nella stessa direzione; due punti uguali, tre punti allineati |
| **Sphere** | F1 FACTOR `(X−a)²+(Y−b)²+(Z−c)²=r²` · F2 EXPAND `X²+Y²+Z²+aX+bY+cZ+d=0` · F3 EDIT | 4 | r ≤ 0; a²+b²+c² ≤ 4d |
| **Cylinder** | Radius, Zmin, Zmax, Center X, Y (asse parallelo a z) | 5 | Radius ≤ 0; Zmin = Zmax |
| **Cone** | Radius (base), Zmin (base), Zmax (vertice), Center X, Y | 5 | Radius ≤ 0; Zmin = Zmax |
| **Z=** | Z = f(X, Y) | — | Syntax ERROR |
| **Param** | Xst=, Yst=, Zst= in S e T | — | le tre espressioni sono necessarie |
| **Rot X** / **Rot Y** | Y = f(X) ruotata attorno all'asse X / Y | — | — |

Coefficienti: il cursore si sposta fra le celle con le frecce, il valore
della cella è in basso a destra; si digita (anche un'espressione: `1/2`,
`A`, `√2`) e **EXE** memorizza e passa alla cella successiva; EDIT modifica
il valore; **F6 SET** controlla le condizioni del manuale e torna alla lista
(EXIT annulla). Le celle dei vettori si riempiono per righe come sullo
schermo (riga X: r0 u v, poi Y, poi Z). Riselezionando un modello registrato
si ritrovano i coefficienti. Cambiando modulo il modello viene convertito
nello stesso oggetto quando è possibile **(scelta)**.

Input diretto: come in GRAPH (editor naturale, Input/Output del SET UP);
durante la digitazione **F1 X / F2 Y** (Param: **S / T**), F3 frazione, F4
√, F5 potenza, **F6 SET**; EXE conferma la riga (Param passa alla riga
successiva).

## 3. V-Window 3D (16-4, 16-5)

`Xmin, max, grid, Ymin, max, grid, Zmin, max, Smin, max, grid, Tmin, max,
grid, Angleθ, Angleφ` (SHIFT F3 o F3 dal grafico). Valori iniziali (F1
**INITIAL**): X, Y, Z −3…3, grid 25, S e T −3…3 con grid 25 **(scelta)**,
θ = 20°, φ = 70° **(scelta)**. grid è il numero di punti calcolati (2…50);
|min|, |max| < 1E98; θ (rotazione oraria dell'asse x, −180 < θ ≤ 180) e φ
(angolo fra la linea di vista e l'asse z, 0 ≤ φ < 360) sempre in gradi.
Valori fuori intervallo: «Out of Domain»; min ≥ max: «Invalid Setting».
Per i modelli Xgrid/Ygrid sono le divisioni del grafico stesso (meridiani e
paralleli della sfera, giri e altezze di cilindro e cono); S e T valgono solo
per Param. **F4 3D-VMEM** STORE / RECALL 1…6. EXIT applica.

## 4. SET UP del 3D Graph (16-1, 16-2, 16-15)

SHIFT MENU (F10) apre il Setup della modalità: Input/Output, Angle,
Display **(scelta: le voci comuni che cambiano la modalità)**, **Plot/LineCol**
(colore di SKETCH), **Axes** (indicatore x, y, z in alto a destra), **Box**
(scatola), **Label** (nomi degli assi e valori sugli spigoli), tutte On al
primo avvio. Sul grafico il tasto **•** (punto) cicla Axes/Box On/On →
On/Off → Off/On → Off/Off e il tasto **,** cambia Label.

## 5. Schermata del grafico (16-14…16-16)

Area dei grafici di GRAPH (320 × 282). Tasti come in GRAPH: **F1 TRACE, F2
ZOOM, F3 V-WIN, F4 SKETCH, F5 G-SOLVE, F6 G⇔T** (anche con SHIFT); **OPTN**:
PICTURE, ROTATE, Z-AUTO (estensione); **AC** o EXIT tornano alla lista.

- **Rotazione**: ← → cambiano θ di 15°, ↑ ↓ φ di 15° **(scelta)**; i valori
  della V-Window seguono la rotazione, giro completo in entrambe le direzioni.
- **ROTATE** (OPTN F2): F1 L→R, F2 R→L, F3 T→B, F4 B→T; passi di 10°, si
  ferma dopo due giri, **AC** la interrompe.
- **ZOOM** (F2): IN, OUT (×1,5 sul centro dello schermo), VIEW-X (θ 0, φ 90),
  VIEW-Y (θ 90, φ 90), VIEW-Z (θ −90, φ 0), ORIGINAL (vista e zoom del
  disegno). Anche `+` / `−`.
- **SKETCH** (F4): Cls, Plot (puntatore, EXE mette un punto), Text (si
  digita il testo); colore Plot/LineCol. Un nuovo disegno o un cambio di vista
  cancella i punti e i testi **(scelta)**.
- **PICTURE** (OPTN F1 F1): l'area del grafico in Pict 1…20
  (`PICOCALC/PICT/PICTnn.pcpict`, utilizzabile come sfondo in GRAPH), senza
  la V-Window. **CAPTURE**: SHIFT 7 (catture `PICOCALC/CAPT`).
- Disegno: superficie nell'Area Color con luce, reticolo nel Line Color,
  rette spesse 2 pixel; **z-buffer condiviso** fra tutti i grafici (anche le
  rette) con la PSRAM, algoritmo del pittore senza; tutto è **tagliato
  esattamente** sulle sei facce del box.
- Calcolo: 600 punti per tick, la tastiera resta reattiva; **AC** interrompe
  e lascia il disegno parziale. DRAW senza grafici disegna la scatola vuota.

## 6. TRACE (16-14, 16-15)

F1 (o SHIFT F1): il puntatore si muove sul reticolo del grafico (← → la prima
variabile, ↑ ↓ la seconda; sui modelli chiusi il giro continua); in basso a
sinistra X, Y, Z nel formato Display, in alto la funzione, a destra «TRACE».
**F1 Next / F2 Back** passano al grafico successivo/precedente. Per Z= e
Param, il tasto **x** (X,θ,T) o un numero apre la finestra delle coordinate
(X e Y, oppure S e T) e il puntatore salta al punto. SHIFT F1 o EXIT
terminano. Le variabili X, Y, S, T del grafico non cambiano A–Z.

## 7. G-Solve (16-16…16-19)

F5: **F1 CROSS**, **F2 INTSECT**, **F3 RELATION**.

- **CROSS** (solo modelli): F1 X / F2 Y / F3 Z sceglie il piano
  perpendicolare all'asse; la sezione è disegnata in primo piano nel colore
  opposto all'Area Color; se un piano del grafico coincide con la sezione se
  ne evidenzia il contorno. **F1 UP / F2 DOWN** spostano la sezione di un
  passo (max−min)/(grid−1), un numero digitato la posiziona; F3–F5 VIEW-X/Y/Z,
  F6 ORIGINAL. Sezioni analitiche: punto (retta), segmento (piano),
  circonferenza (sfera, cilindro e cono trasversali), rettangolo (cilindro),
  conica (cono, calcolata punto per punto).
- **INTSECT** (rette e piani): retta–retta e retta–piano danno il punto
  (X=, Y=, Z=), piano–piano la retta d'intersezione `X=x0+at, Y=y0+bt,
  Z=z0+ct` disegnata sul grafico; «NOT FOUND» se non si intersecano,
  «INFINITE» se coincidono (o la retta giace nel piano).
- **RELATION**: INTERSECT, PERPENDICULAR, PARALLEL, SKEW, SAME LINE, ON THE
  PLANE, SAME PLANE **(scelta dei testi)**.
- Con tre rette/piani F1 Next / F2 Back scorrono le coppie (1,2), (1,3),
  (2,3); EXIT cancella il risultato. Meno di due: «INTSECT requires multiple
  lines or planes.» / «RELATION requires multiple lines or planes.».

## 8. 3D Graph Memory (16-7, 16-8)

F4 3D-GMEM: **STORE** e **RECALL** 1…20. Un insieme contiene le tre
funzioni con selezione e colori, la V-Window e il Setup; STORE sovrascrive,
RECALL sostituisce lista, V-Window e Setup. Budget di tutte le memorie: 12 KB
sul Pico 2 W, 64 KB sulla Plus («Memory ERROR» oltre). MEMORY (tasto F) può
cancellare a parte i dati 3D, le 3D G-Mem e le 3D V-Win.

## 9. Memoria e prestazioni

| | Pico 2 W (solo SRAM) | Pico Plus 2 W (PSRAM 8 MB) |
|---|---|---|
| Punti per lato | ≤ 25 | ≤ 50 (limite del manuale) |
| Disegno | algoritmo del pittore | z-buffer 180 KB in PSRAM |
| Immagine della scena per TRACE/SKETCH | no (ridisegno) | 180 KB in PSRAM |
| 3D G-Mem | 12 KB | 64 KB |

Reticoli e buffer passano da `mem::resource(PREFER_PSRAM)`; un
`std::bad_alloc` dimezza la griglia e poi dà «Memory ERROR» senza trattenere
memoria; uscendo dalla modalità tutto viene liberato.

## 10. Salvataggio

Record del workspace **v10** (`docs/STORAGE.md`): funzioni, V-Window, Setup,
zoom, 3D V-Win e 3D G-Mem. I record v5–v9 (superfici Z1–Z3) vengono
convertiti: grafici Z= con la stessa selezione, griglia (celle) + 1 punti,
yaw/pitch → θ/φ con la stessa orientazione.

## 11. Differenze inevitabili

| Voce | Differenza | Motivo |
|---|---|---|
| Schermo | area 320 × 282 | display PicoCalc 320 × 320 |
| Tasti | F1–F6 del grafico anche senza SHIFT (come GRAPH), SHIFT = F7, OPTN = F8, SET UP = F10, X,θ,T = `x`, AC = Shift+Esc, FORMAT = SHIFT 5 o OPTN | tastiera PicoCalc |
| Griglia sul Pico 2 W | al più 25 punti per lato | 520 KB di SRAM |
| Z-buffer | solo con la PSRAM | 180 KB non stanno nella SRAM libera |
| File `.g3m` del Memory Manager | non realizzati | il Memory Manager con copia dei file non esiste per nessuna modalità |
| Grafica | icone, puntatore, indicatore degli assi propri | proprietà Casio |
