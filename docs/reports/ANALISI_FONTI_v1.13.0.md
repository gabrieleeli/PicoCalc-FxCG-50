# Analisi delle fonti — v1.13.0 (completamento totale di Run-Matrix)

Rapporto preparato **prima** di scrivere codice per la v1.13.0. Base:
`main` = tag `v1.11.0` = commit `f6933fd` (verificato con `git fetch origin
main --tags`: nessun commit successivo al tag da conservare). Le fonti Casio
sono usate solo come **riferimento di comportamento**: nessun codice, ROM,
font o grafica Casio entra nel progetto.

Stato del repository prima di ogni modifica:

- test host del profilo Pimoroni Pico Plus 2 W: **40/40 superati** (base di
  confronto);
- fixture `tests/fixtures/workspace_v1_11_0.inc` generata **con i sorgenti
  della v1.11.0** (record versione 7: Deg, Simplify Manual, CSV `;`/`,`,
  XList/YList di Statistics, ▶Simp, simboli ingegneristici, forme esatte in
  `Mat A`, List 3 e List 26 con sottonomi, una riga Hex, comandi grafici,
  Y1): serve alla prova di migrazione v7 → v9 (oggi `tests/fixtures/workspace_v1_11_0_run_matrix.inc`;
  dopo il merge della v1.12.0 si aggiunge il record v8 di quella versione, vedi
  [SVILUPPO_v1.13.0.md](SVILUPPO_v1.13.0.md)).

## 1. Fonti

| # | Fonte | Cosa documenta | Uso |
|---|---|---|---|
| 1 | Manuale software fx-CG50 (OS 3.x), testo consultato su manualslib (`manualslib.com/manual/3093535`): cap. 1 *Basic Operation*, 2 *Manual Calculations*, 3 *List Function*, 5 *Graphing* (SKETCH), 6 *Statistics* (funzioni in Run-Matrix), 8 *Programming* (elenco dei comandi) | comportamento della calcolatrice | riferimento; le pagine indicate sotto sono quelle del manuale stampato (`2-25`…); quelle segnate **≈** sono ricostruite dalla struttura dei capitoli perché l'estratto disponibile non le riporta |
| 2 | Manuali fx-CG10/CG20 (stesso sistema operativo) e fx-9860GIII (stessi comandi di Run-Matrix) | conferme su sintassi di `Σ(`, `SolveN(`, `FMin(`, `RanSamp#(`, distribuzioni, Base-N | conferma della sintassi |
| 3 | Documenti e sorgenti del progetto: README, `docs/RUN_MATRIX.md` (Limiti noti), FEATURE_MATRIX, ROADMAP (Punto 3), ARCHITECTURE, APPLICATIONS_ARCHITECTURE, MATH_ENGINE, STORAGE, TESTING; `src/expression.cpp`, `run_matrix.cpp`, `natural_editor.cpp`, `math_renderer.cpp`, `apps/matrix_editor.cpp`, `graph_commands.cpp`, `workspace.cpp` | architettura reale, record v7, vincoli | perimetro e vincoli |
| 4 | Pico SDK 2.2.0, `cmake/fxai_board.cmake`, `board_config.hpp`, script del linker | flash, RAM, PSRAM delle due schede | §5 |

Il PDF ufficiale non è scaricabile da questo ambiente (support.casio.com
risponde 404, il sito Casio UK 403): il testo è stato letto tramite
manualslib. Dove il manuale non è stato verificabile riga per riga il
comportamento scelto è segnato **(scelta)** qui e in `docs/RUN_MATRIX.md`.

## 2. Tabella di parità (perimetro del lavoro)

Stato **prima** della v1.13.0: **P** presente, **D** diverso, **M**
mancante, **I** differenza inevitabile (vedi §3). Ogni voce D o M va portata
a P; la colonna «Intervento» dice come. La tabella finale con lo stato di
ogni voce è in `docs/reports/SVILUPPO_v1.13.0.md`.

### 2.1 Calcoli di base e funzioni speciali (cap. 2-1…2-11)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| B1 | `+ − × ÷`, priorità del manuale, moltiplicazione implicita | 2-1…2-4 | P | — |
| B2 | Fix / Sci / Norm 1-2 / Eng | 2-10 | P | — |
| B3 | Overflow: risultato con \|x\| ≥ 10¹⁰⁰ → `Math ERROR` | 2-4 | D (solo ∞) | controllo dell'intervallo nei valutatori |
| B4 | Variabili A–Z, **r**, **θ** (28 variabili) | 2-5 | D (mancano r e θ) | due nuove variabili nella memoria (record v9), VARS e `→r`, `→θ` |
| B5 | `→` e `→A~F` | 2-6 | P | — |
| B6 | Ans | 2-8 | P | — |
| B7 | **Multi-istruzione** `:` (calcoli in sequenza, si mostra l'ultimo) | 2-9 ≈ | M | separazione al livello 0 nel motore di Run-Matrix, stesso parser per ogni istruzione |
| B8 | **Comando di visualizzazione** `◢` (mostra il risultato intermedio) | 2-9 ≈ | M | come `:`, ogni risultato intermedio diventa una riga della cronologia |
| B9 | **Function memory** f1–f20 (OPTN › FUNCMEM: STORE, RECALL, fn, SEE) | 2-? ≈ | M | 20 memorie di funzione nel workspace (record v9), `fn` nel parser |
| B10 | Ripetizione (EXE su riga vuota), cronologia modificabile e ricalcolo | 1-10 | P | — |
| B11 | Copia / incolla (CLIP, PASTE) | 1-11 | P | — |

### 2.2 SET UP di Run-Matrix (cap. 1-35…1-41)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| S1 | Input/Output Math / Line | P | — |
| S2 | Mode Comp / Dec / Hex / Bin / Oct | P | — |
| S3 | Frac Result d/c / ab/c | P | — |
| S4 | Angle Deg / Rad / Gra | P | — |
| S5 | Complex Mode Real / a+bi / r∠θ | P | — |
| S6 | Display Fix / Sci / Norm / Eng | P | — |
| S7 | Simplify Auto / Manual | D (regola) | regola del manuale, §2.6 |
| S8 | **List File** File 1–6 | M | sei insiemi di 26 liste (record v9), SET UP |
| S9 | **Sub Name** On / Off | M | impostazione (record v9): l'editor delle liste mostra o nasconde la riga SUB |
| S10 | Sketch Line (Normal, Thick, Broken, Dot, Thin) | P (in GRAPH) | usata dai comandi `SketchNormal`… |
| S11 | Func Type, Draw Type, Derivative, Coord, Grid, Axes, Label, Background | P (in GRAPH) | — |

### 2.3 Funzioni (cap. 2-12…2-24)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| F1 | sin cos tan e inverse, log ln 10ˣ eˣ, logₐb | 2-12 | P | — |
| F2 | HYPERBL sinh…atanh | 2-14 | P | — |
| F3 | √ ³√ ˣ√ x² x⁻¹ x! | 2-17 | P | — |
| F4 | ANGLE `°` dopo un valore | 2-15 | P | — |
| F5 | ANGLE **`r`** e **`g`** dopo un valore (radianti, gradi centesimali) | 2-15 | M | postfissi `ʳ` `ᵍ` nel lexer (stesso parser) |
| F6 | ANGLE **° ' "** in input (`2°20'30"`) | 2-15 | D (solo `dms(d,m,s)`) | postfisso `'` e `"` dopo `°` nel parser |
| F7 | ►DMS | 2-15 | P | — |
| F8 | **Pol( / Rec(** con risultato in List Ans (`{r,θ}`, `{x,y}`) | 2-20 | D (testo «r= θ=») | risultato lista (List Ans), riga come sulla calcolatrice |
| F9 | Ran# | 2-18 | P | — |
| F10 | **Ran# n** (sequenze 1–10, `Ran# 0` le reinizializza) | 2-18 | M | generatore con semi fissi per n=1…10 |
| F11 | RanInt#(a,b[,n]) | 2-19 | D (senza n) | terzo argomento: lista di n numeri |
| F12 | RanNorm#(σ,μ[,n]), RanBin#(n,p[,m]) | 2-19 | D (senza n) | idem |
| F13 | **RanList#(n)** | 2-19 | M | lista di n numeri casuali |
| F14 | **RanSamp#(lista,n[,m])** | 2-19 ≈ | M | campione senza (m=0) o con (m=1) ripetizione |
| F15 | nPr nCr | 2-21 | P | — |
| F16 | **P( Q( R( t(** (distribuzione normale standard) | 2-21 ≈ | M | funzioni del valutatore |
| F17 | Frazioni, frazione mista, ab/c ⇔ d/c, S⇔D | 2-22 | P | — |
| F18 | ESYM m µ n p f k M G T | 2-23 | P | — |
| F19 | ESYM **P** (10¹⁵) ed **E** (10¹⁸) | 2-23 | M | due simboli in più |
| F20 | **ENG / ENG←** (sposta l'esponente di 3 nel risultato mostrato) | 2-23 | M | vista del risultato (comando del menu, come ▶S⇔D) |
| F21 | LOGIC And Or Not Xor | 2-24 | P | — |
| F22 | NUMERIC Abs Int Frac Intg RndFix GCD LCM MOD MOD_Exp | 2-17 ≈ | P | — |
| F23 | NUMERIC **Rnd** (arrotonda alle cifre della visualizzazione) | 2-17 ≈ | D (round all'intero) | `Rnd` secondo Fix/Sci/Norm |

### 2.4 Calcoli numerici CALC (cap. 2-25…2-35)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| C1 | Int÷ Rmdr | 2-25 | P | — |
| C2 | Simp | 2-26 | P | regola Simplify Manual: §2.6 |
| C3 | **Solve(f, stima[, inf, sup])** (anche equazione `a=b`) | 2-28 | D (senza limiti, solo f=0) | limiti e forma «equazione» |
| C4 | d/dx(f, a) | 2-29 | P | — |
| C5 | d²/dx²(f, a) — anche **modello Math** | 2-30 | D (manca il modello) | modello Math a due campi |
| C6 | ∫(f, a, b[, tol]) | 2-31 | D (senza tol) | quarto argomento |
| C7 | **SolveN(f[=g][, x][, inf, sup])** fino a 10 soluzioni in List Ans | 2-33 ≈ | M | ricerca delle radici con il valutatore unico |
| C8 | **FMin(f, a, b[, n]) / FMax(…)** risultato `{x, f(x)}` in List Ans | 2-35 ≈ | M | ricerca del minimo/massimo (sezione aurea + campionamento) |
| C9 | **Σ(f, k, a, b[, n])** con variabile e passo; modello Math con la variabile | 2-34 | D (variabile fissa x, senza passo) | quattro/cinque argomenti; modello Σ con il campo della variabile |
| C10 | Funzioni vietate dentro Solve/SolveN/FMin/FMax/Σ (Solve, SolveN, FMin, FMax, Σ annidati) | 2-28 | D | stesso controllo nel valutatore |

### 2.5 Complessi (cap. 2-36…2-38)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| X1 | i, Abs, Arg, Conjg, ReP, ImP | P | — |
| X2 | **►r∠θ / ►a+bi** | M | viste del risultato (come ▶Dec) |
| X3 | Input **r∠θ** (simbolo ∠) | M | operatore `∠` nel parser |
| X4 | Liste e matrici complesse; Arg/Conjg elemento per elemento | D (Arg no) | Arg ammessa sugli elementi |

### 2.6 Frazioni e Simplify (cap. 2-22, 2-26)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| R1 | Simplify **Auto**: risultato ridotto | P | — |
| R2 | Simplify **Manual**: ogni operazione tra frazioni dà la frazione **non ridotta** ottenuta con le regole del manuale (somma/differenza: denominatore = prodotto dei denominatori se diversi, comune se uguali; prodotto: prodotti di numeratori e denominatori; quoziente: prodotto per il reciproco; potenza intera: potenze di numeratore e denominatore; intero ⇄ frazione con denominatore 1) | D (solo la frazione digitata) | aritmetica di frazioni non ridotte accanto a quella esatta (stesso AST) |
| R3 | ▶Simp, ▶Simp n, F=n | P | — |

### 2.7 Base-N (cap. 2-39…2-42)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| N1 | Dec/Hex/Oct 32 bit in complemento a due | P | — |
| N2 | **Bin 16 bit** (0…0111111111111111, negativi 1000000000000000…1111111111111111) | D (32 bit) | limite di 16 bit in Bin (Math ERROR fuori) |
| N3 | prefissi d h b o, Neg Not and or xor xnor, ►Dec ►Hex ►Bin ►Oct | P | — |
| N4 | **Variabili A–F in Hex** distinte dalle cifre A–F | D (non raggiungibili) | token variabile `$A`…`$F` (VARS, Alt+lettera), mostrato sottolineato |
| N5 | Liste e matrici in Base-N: vista dedicata (elementi nella base) | D | elementi mostrati nella base della riga |
| N6 | Messaggi: Syntax ERROR (virgola, cifra non valida), Math ERROR (fuori intervallo, funzioni) | P | verificati per le quattro basi |

### 2.8 Matrici e vettori (cap. 2-43…2-63)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| T1 | Editor MAT/VCT: DELETE, DEL-ALL, DIM, CSV, M⇔V, ROW-OP, ROW, COLUMN, EDIT | P | — |
| T2 | **Forme esatte** delle celle **senza limite di 64 elementi** | D | limite tolto (resta il limite di memoria della scheda) |
| T3 | **Un decimale digitato resta decimale** (`0.25` → `0.25`) | D (mostrato 1/4) | indicatore «decimale» per cella |
| T4 | Mat, Det, Trn, Augment, Identity, Dim, Fill, Ref, Rref, Mat→List, List→Mat, potenze, inversa | P | — |
| T5 | **Fill(v, Mat A)** modifica la memoria | D (solo valore) | memorizza nella memoria indicata |
| T6 | **Swap / ×Row / ×Row+ / Row+** come comandi | M | comandi delle righe su Mat A–Z |
| T7 | Vct, DotP, CrossP, Angle, UnitV, Norm, aritmetica dei vettori | P | — |
| T8 | MatAns / VctAns | P | — |

### 2.9 Liste (cap. 3)

| # | Voce | Pag. | Stato | Intervento |
|---|---|---|---|---|
| L1 | List 1–26, List Ans, `List n[k]` | 3-1 | P | — |
| L2 | **`List "QTY"`** (riferimento per sottonome) | 3-? ≈ | M | stringa letterale nel lexer, risolta dal workspace |
| L3 | **List File 1–6** (SET UP) | 3-17 ≈ | M | S8 |
| L4 | Dim (lettura) e **`n→Dim List k`** (crea n zeri) | 3-7 | D (solo lettura) | destinazione `Dim List` |
| L5 | **Fill(v, List n)** modifica la lista | 3-8 | D | come T5 |
| L6 | Seq, Augment, Sum, Prod, Cuml, %, ΔList | 3-8…3-13 | P | — |
| L7 | **Min/Max di due liste** (`Min(List1,List2)` elemento per elemento) | 3-9 | M | due argomenti lista |
| L8 | **Mean/Median con frequenze** (`Mean(List1,List2)`) | 3-10 | M | secondo argomento |
| L9 | Aritmetica e funzioni scientifiche sulle liste | 3-14 | P | — |
| L10 | CSV | 3-18 | P | — |
| L11 | Sottonomi (riga SUB), 999 elementi | 3-3 | P / I | 255 sul Pico 2 W (inevitabile, §3) |

### 2.10 OPTN STAT in Run-Matrix (cap. 6)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| D1 | **x̂( / ŷ(** (valori stimati della regressione dell'ultimo calcolo di Statistics) | M | tipo di regressione salvato (record v9), formule nel valutatore |
| D2 | **DIST**: NormPD, NormCD, InvNormCD, tPD, tCD, InvTCD, ChiPD, ChiCD, InvChiCD, FPD, FCD, InvFCD, BinomialPD, BinomialCD, InvBinomialCD, PoissonPD, PoissonCD, InvPoissonCD, GeoPD, GeoCD, InvGeoCD, HypergeoPD, HypergeoCD, InvHypergeoCD | M | funzioni del valutatore (statistics.hpp) |
| D3 | **S-Dev( / Var(** di una lista (con frequenze) | M | funzioni delle liste |

### 2.11 Conversioni metriche (cap. 2-64…)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| U1 | **CONVERT**: `valore[unità]►[unità]` per lunghezza, area, volume, tempo, temperatura, velocità, massa, forza, pressione, energia, potenza | M (solo l'app Conversion) | operatore di conversione nel parser con la tabella delle unità di `point_a` |

### 2.12 Altri menu OPTN

| # | Voce | Stato | Intervento |
|---|---|---|---|
| O1 | **PICTURE** StoPict / RclPict | M | comandi grafici (G6) |
| O2 | **FUNCMEM** | M | B9 |
| O3 | **CAPTURE** RclCapt | M | richiama una cattura dello schermo salvata (immagine PICT) |
| O4 | **TVM** (Smpl_SI, Smpl_SFV, Cmpd_PV, Cmpd_FV, Cmpd_PMT, Cmpd_n, Cmpd_I%, Cash_NPV, Cash_IRR, Cash_PBP, Cash_NFV, Amt_BAL, Amt_INT, Amt_PRN, Amt_ΣINT, Amt_ΣPRN, Cnvt_EFF, Cnvt_APR, Cost, Sell, Margin, Days_Prd) | M | funzioni del valutatore (modulo `finance`) |

### 2.13 VARS (cap. 1-31)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| V1 | V-WIN (lettura) | P | — |
| V2 | **V-WIN in scrittura** (`5→Xmax`) | M | destinazione «variabile di sistema» con i controlli di GRAPH |
| V3 | **FACTOR** Xfct, Yfct | M | dal workspace (GRAPH) |
| V4 | **STAT** n, x̄, Σx, Σx², σx, sx, minX, maxX, ȳ, Σy, Σy², Σxy, σy, sy, minY, maxY, a…e, r, r², MSe, Q1, Q3, Med, Mod | M | calcolati dalle liste di Statistics |
| V5 | GRAPH Y, r, Xt, Yt, X | P | — |
| V6 | **DYNA** Start, End, Pitch | M | valori nel workspace (record v9) |
| V7 | TABLE Start, End, Pitch (lettura) / **scrittura**, **Result** | D | scrittura; Result = Mat Ans della tabella |
| V8 | **RECURSION** R.Start, R.End, a0…c2, anStart… | M | valori nel workspace (record v9) |
| V9 | **EQUATION** SimRes, SimCoef, PlyRes, PlyCoef | M | calcolati dai dati di Equation |
| V10 | **TVM** n, I%, PV, PMT, FV, P/Y, C/Y | M | valori nel workspace (record v9), usati da O4 |
| V11 | **Str 1–20** | M | stringhe nel workspace: `"ABC"→Str 1`, Str n nei calcoli come testo |

### 2.14 Comandi grafici di Run-Matrix (cap. 5 SKETCH, cap. 8)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| G1 | Cls, ViewWindow, Graph Y=/r=/X=/(X,Y)=, Plot…, Line, F-Line, Circle, Vertical, Horizontal, Text, PxlOn/Off/Chg, Tangent, Normal, Inverse | P | — |
| G2 | **PxlTest(** riga, colonna (1 se il pixel è acceso) | M | funzione del valutatore che chiede lo stato del pixel a GRAPH tramite il servizio GraphCmd |
| G3 | **Graph Y> Y< Y≥ Y≤, X> X< X≥ X≤** | M | grafici dei comandi di tipo disequazione |
| G4 | **G∫dx f(x), a, b** (area colorata) | M | elemento SKETCH «integrale» |
| G5 | **SketchNormal / SketchThick / SketchBroken / SketchDot / SketchThin** (prefisso di un comando) | M | stile dell'elemento successivo |
| G6 | **StoPict n / RclPict n / BG-Pict n** | M | immagini PICT del workspace/file (`pict.hpp`) |
| G7 | **DrawStat** (grafici statistici di StatGraph1–3) | M | elementi dei grafici statistici disegnati da GRAPH |
| G8 | **DrawGraph, DrawDyna, DrawR-Con/Plt, DrawFTG-Con/Plt** | M | disegno della lista delle relazioni, della funzione Dyna (Y1 con la variabile Dyna), delle successioni e della tabella |
| G9 | **G-SelOn / G-SelOff n** | M | selezione Y1–Y20 |
| G10 | **Zoom Auto? / ZoomIn/Out?** | — | non sono comandi di Run-Matrix (solo programmi): fuori perimetro |
| G11 | Grafici dei comandi nei progetti `.pcgraph` | D (non salvati) | **decisione**: sì, sezione `[COMMANDS]` del formato |

### 2.15 Editor Math/Line (cap. 1-14…1-24, ROADMAP Punto 3)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| E1 | Modelli: frazione, mista, √, ˣ√, ³√, x², x^, x⁻¹, logₐb, eˣ, 10ˣ, \|x\|, d/dx, ∫, Σ, Π, MAT/VCT, ( ) | P | — |
| E2 | **d²/dx²** modello | M | C5 |
| E3 | **Σ con la variabile** nel modello | D | C9 |
| E4 | Conversione testo ↔ albero esatta in entrambe le direzioni (gruppi, radice con indice qualunque, tutti i modelli) | D | serializzazione/import senza perdite, corpus di test |
| E5 | **× obbligatorio prima di una frazione** e parentesi implicite in Line (`2 1/3` → `2(1/3)`) | M | regola nella conversione Math → Line |
| E6 | INS / sovrascrittura, SHIFT+INS, CLIP, parentesi colorate | P | — |
| E7 | **CATALOG** (SHIFT 4): elenco alfabetico di tutti i comandi | M | finestra CATALOG in Run-Matrix |

### 2.16 Messaggi di errore (Appendice α)

| # | Voce | Stato | Intervento |
|---|---|---|---|
| ER1 | Syntax, Math, Stack, Memory, Argument, Dimension, Non-Real ERROR | P | — |
| ER2 | **Type ERROR** non esiste sulla calcolatrice | D | diventa `Argument ERROR` (la classe resta TypeError) |
| ER3 | **Complex Number In List / In Matrix** (memorizzazione di complessi dove servono reali) | D (Math ERROR) | messaggi della calcolatrice |
| ER4 | **Can't Solve!** / **Iteration ERROR** (Solve, SolveN) | D | `Iteration ERROR` quando Solve non converge, `Can't Solve!` quando SolveN non trova soluzioni **(scelta)** |
| ER5 | **Range ERROR** (V-Window da ViewWindow / `→Xmin`) | D | testo di validateView con il prefisso Range ERROR |
| ER6 | Cursore sulla causa dopo EXIT | P | — |

## 3. Differenze inevitabili e scelte

| Differenza | Motivo | Scelta |
|---|---|---|
| Schermo 320×320 (calcolatrice 396×224) | display PicoCalc | area grafica 320×282: riga 1–282, colonna 1–320 per `PxlOn`, `PxlTest(`, `Text` (sulla calcolatrice 1–187 e 1–379); Run-Matrix mostra più righe di cronologia |
| Nessun tasto ALPHA / SHIFT dedicati | tastiera PicoCalc | SHIFT = F7 (blocco), OPTN = F8, VARS = F9, SET UP = F10; le lettere si digitano direttamente (A–Z, minuscole equivalenti) |
| Variabili A–F in Hex | senza ALPHA le lettere digitate sono cifre | variabile da VARS › A~Z o con Alt+lettera, mostrata sottolineata (la calcolatrice usa un carattere diverso) |
| Variabile `r` | `r` digitata è la variabile R | `r` si inserisce da VARS (token `rvar`); θ con il tasto θ (SHIFT F6) o VARS |
| CATALOG | SHIFT 4 come sulla calcolatrice | elenco filtrabile per lettera iniziale |
| Simbolo `◢` | il carattere non è nel font del progetto | mostrato come `◢` aggiunto al font (glifo disegnato dal progetto) |
| Liste: 255 elementi e 1 530 in tutto sul Pico 2 W | 520 KB di SRAM, record da 59,75 KB | per scheda in `limits.hpp`; List File 1–6 condividono lo stesso budget |
| CONVERT è un add-in sulla calcolatrice | — | integrato (la stessa tabella delle unità dell'app Conversion) |
| Velocità di SolveN / FMin / Σ sul Cortex-M33 | CPU diversa | limiti di iterazioni identici sulle due schede; tempi misurati in DIAG |
| Grafici e immagini Casio (font, icone) | proprietà Casio | grafica propria del progetto |

## 4. Vincoli e come vengono rispettati

| Vincolo | Rispetto |
|---|---|
| Un solo parser, un solo AST, un solo valutatore | tutte le nuove funzioni sono nodi `Call` dello stesso parser; unità, `∠`, `ʳ`, `ᵍ`, `° ' "`, stringhe `"…"` e `$A` sono token dello stesso lexer; `:`/`◢` separano le istruzioni prima del parser (ciascuna passa dal parser unico); i comandi grafici continuano a valutare gli argomenti con `evaluateReal`/`evaluateAny` |
| Dati solo nel workspace tramite interfacce | List File, function memory, Str, variabili di Dyna/Recursion/TVM, tipo di regressione, r e θ: membri del workspace, letti tramite `NameResolver`/`CollectionResolver`; Run-Matrix non tocca direttamente `ws.lists`, `ws.view`, `ws.graph` (test di architettura **più severo**: nuove sezioni vietate e nuovi controlli) |
| Memoria embedded | niente oggetti grandi sullo stack (argomenti delle funzioni in un array fisso di 6, risultati delle collezioni nello heap); List File con budget comune in `limits.hpp` |
| Compatibilità | record **v9** in coda al v8 (EQUATION, v1.12.0); v1–v8 letti senza perdite; fixture v1.11.0 e v1.12.0 |
| Niente CAS | derivate, integrali, Σ, Solve, SolveN, FMin/FMax numerici |
