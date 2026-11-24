# Statistics — guida (v1.18.0, parità con il capitolo 6)

MENU › **2**. Sulla **Pimoroni Pico Plus 2 W** Statistics è alla **parità
completa** con il capitolo 6 del manuale fx-CG50 (il manuale descrive il
comportamento; nessun codice, grafica o testo copiato). Tabella delle voci,
decisioni e differenze: [reports/ANALISI_FONTI_v1.18.0.md](reports/ANALISI_FONTI_v1.18.0.md),
[reports/SVILUPPO_v1.18.0.md](reports/SVILUPPO_v1.18.0.md).
Sul **Raspberry Pi Pico 2 W** (congelato) Statistics resta quella della v1.16.0
(liste, 1/2-Var, 9 regressioni, grafici di base, Z/t, DIST di base).

Tasti della PicoCalc: F1–F5, **F6** = SHIFT+F1, **SHIFT** = F7, **OPTN** = F8,
**VARS** = F9, **SET UP** = F10, **AC** = Shift+Esc, EXE = Enter, EXIT = Esc.

## Editor delle liste

List 1–26 (999 elementi l'una, budget della scheda), quattro colonne visibili,
numeri di riga a sinistra, valore completo della cella in basso.

| Pagina | F1 | F2 | F3 | F4 | F5 | F6 |
|---|---|---|---|---|---|---|
| 1 | GRAPH | CALC | TEST | INTR | DIST | ▶ |
| 2 | SRT-A | SRT-D | DEL | DEL-ALL | INS | ▶ |
| 3 | ADD | JUMP | COLOR | FILE | CSV | ▶ |

- **Celle**: si digita un'espressione (stesso motore di Run-Matrix) e EXE; EXE su
  una cella piena la modifica; Backspace o DEL la cancella (le seguenti salgono).
- **SRT-A / SRT-D**: «How Many Lists?» (1–6), poi la lista base e le altre
  liste, riordinate come la base. Liste di lunghezza diversa: Dimension ERROR.
- **DEL-ALL** cancella la lista del cursore dopo la conferma (il sottonome
  resta); OPTN › DEL-ALL cancella tutte le liste.
- **INS** inserisce uno 0 sopra il cursore, **ADD** sotto.
- **JUMP**: TOP, BOTTOM, ROW (numero di riga).
- **Riga SUB** (↑ dalla prima cella): sottonome fino a 8 caratteri, usabile
  come `List "QTY"` nei calcoli; si nasconde con SET UP › Sub Name Off.
- **Testa della lista** (↑ dalla riga SUB): una **formula** genera la lista,
  per esempio `List 1*2` o `Seq(X^2,X,1,10,1)`, ed è ricalcolata quando cambiano
  le liste che cita (anche all'ingresso nella modalità). Una formula che cita
  sé stessa o un ciclo di formule: **Circular ERROR**. Modificare a mano una
  cella della lista toglie la formula (DEL sulla testa la toglie). La testa
  della lista con una formula è sottolineata.
- **COLOR**: colore dei dati della lista (Blue, Red, Magenta, Green, Cyan,
  Yellow, Black); usato anche dai grafici con Color Link OnData.
- **FILE**: List File 1–6 (l'intestazione mostra «Fn» fuori dal File 1).
- **CSV**: LOAD (dalla colonna del cursore), SAVE·AS (tutte le liste), SET
  (separatore e decimale); anche da OPTN. File in `PICOCALC/CSV` sull'archivio
  scelto in SYSTEM.

## SET UP (SHIFT MENU)

Stat Wind (Auto: V-Window dai dati; Manual: la V-Window di Graph), Resid List
(None o List n: i residui di ogni regressione), Q1Q3 Type (Std: mediane delle
metà; OnData: il dato con frequenza cumulata oltre 1/4 e 3/4, media di due dati
quando è esattamente 1/4), List File, Sub Name, Angle, Display.

## Grafici (GRAPH)

GRAPH1, GRAPH2, GRAPH3 disegnano lo StatGraph scelto; **SELECT** accende e
spegne i tre grafici (DrawOn/DrawOff) e DRAW li disegna insieme; **SET** apre
le impostazioni dello StatGraph (F1–F3 per sceglierlo, F5 **DefG** lo riporta
ai valori iniziali: Scatter, List 1, List 2, Frequency 1, quadrato).

| Tipo | Voci del SET |
|---|---|
| Scatter, xyLine | XList, YList, Frequency, Mark Type (Square, Cross, Dot, Circle), Color Link (Off, X&Freq, OnData), Graph Color |
| NPPlot | XList, Frequency, Mark Type, Graph Color (quantili normali di (i − 0,5)/n) |
| Hist, Broken | XList, Frequency, Graph Color; **Start** e **Width** chiesti prima del disegno (EXE su Start passa a Width, EXE su Width disegna) |
| MedBox | XList, Frequency, Graph Color, Outliers (baffi fino ai dati entro Q1 − 1,5·IQR e Q3 + 1,5·IQR, outlier come punti) |
| N-Dist | XList, Frequency, Graph Color (curva con x̄ e σx) |
| X, Med, X², X³, X⁴, Log, Exp (ae^bx, ab^x), Power, Sin, Logistic | come Scatter: punti e curva della regressione |
| Pie | Data, Display (%, Data, None), % Sto List (percentuali nella lista) |
| Bar | Data1, Data2, Data3 (None o lista), Stick Style (Length, Horiz), Graph Color |

Nella schermata del grafico: **CALC** (1-VAR, 2-VAR e le regressioni sulle
liste del primo grafico disegnato; DRAW sovrappone la curva), **TRACE**
(← → tra i punti, le classi, i cinque valori del box; ↑ ↓ cambia grafico),
**V-WIN** (modifica la finestra e la V-Window di Graph).

## CALC

1-VAR (x̄, Σx, Σx², σx, sx, n, minX, Q1, Med, Q3, maxX, Mod, Mod:n, Mod:F),
2-VAR (x̄, Σx, Σx², σx, sx, n, ȳ, Σy, Σy², σy, sy, Σxy, minX, maxX, minY, maxY),
**REG**: X (ax+b, a+bx), Med, X², X³, X⁴ | Log, Exp (ae^bx, ab^x), Pwr, Sin,
Lgst; **SET**: 1Var XList, 1Var Freq, 2Var XList, 2Var YList, 2Var Freq.

| Regressione | Risultati | MSe |
|---|---|---|
| ax+b, a+bx, Log, Exp, Power | a, b, r, r², MSe | Σ scarti² /(n − 2) sulla scala del modello lineare (ln y per Exp e Power) |
| Med-Med | a, b (punti x1…y3 in VARS › STAT › PTS) | — |
| X², X³, X⁴ | coefficienti, R², MSe | /(n − 3), /(n − 4), /(n − 5) |
| Sin (y = a·sin(bx+c)+d, radianti) | a, b, c, d, MSe | /(n − 2) |
| Logistic (y = c/(1+a·e^(−bx))) | a, b, c, MSe | /(n − 2) |

Sin e Logistic sono calcolate a passi (stima iniziale e Levenberg–Marquardt):
la schermata «Calcolo in corso» mostra la percentuale e **AC** dà «Break».
Nel risultato: **COPY** apre l'elenco Y1–Y20 di GRAPH e scrive la formula
nella Yn scelta (Sin con il suffisso ʳ: giusta anche in Deg), **DRAW** disegna
punti e curva. Con SET UP › Resid List i residui vanno nella lista.

## TEST, INTR, DIST

Moduli con **Data** List/Variable dove la fx-CG50 lo prevede, verso
dell'ipotesi (≠ < >), **Save Res** (None o List n: i risultati in colonna),
**CALC** (F1) e **DRAW** (F6, curva della statistica con l'area del p-value).

- **TEST**: Z (1-SMPL, 2-SMPL, 1-PROP, 2-PROP), t (1-SMPL, 2-SMPL con Pooled,
  REG), CHI (GOF con Observed, Expected, df e CNTRB; 2WAY con Observed e
  Expected in Mat A–Z), F (2 campioni), ANOVA (How Many 1 o 2: Factor A,
  Factor B, Dependent; due vie con disegno bilanciato, DRAW = grafico
  d'interazione). L'ANOVA gira a passi (AC = Break).
- **INTR**: Z (1-SMPL, 2-SMPL, 1-PROP, 2-PROP), t (1-SMPL, 2-SMPL con Pooled).
- **DIST**: NORM (Npd, Ncd, InvN con Tail Left/Right/Central), t, CHI, F,
  BINOMIAL | POISSON, GEO, H.GEO, ognuna con PD, CD e inversa. Data Variable:
  risultato in **Ans** e grafico per le continue; Data List: un risultato per
  elemento in **ListAns** (e Save Res); CD continue con Lower List e Upper
  List, CD discrete con la sola Upper List e Lower 0. Inverse e liste a passi
  (AC = Break). È lo stesso nucleo della modalità **Distribution**.

## VARS › STAT (Run-Matrix e le altre modalità)

X, Y, GRAPH (a–e, r, r², MSe, Q1, Q3, Med, Mod, H-Start, H-pitch), PTS
(x1…y3 del Med-Med), INPUT (n, x̄, sx, n1, n2, x̄1, x̄2, sx1, sx2, sp), TEST
(p, z, t, χ², F, p̂, p̂1, p̂2, df, se, r, r², righe dell'ANOVA), INTR (Left,
Right, p̂, p̂1, p̂2, df), DIST (p, xInv, x1InvN, x2InvN, zLow, zUp, tLow, tUp):
i valori dell'**ultimo calcolo**, conservati nel record (sezione `0x0101`).
x̂( e ŷ( di OPTN › STAT usano l'ultima regressione (tutti i 12 tipi; X³ e X⁴
danno le radici reali in una lista; Sin non ha x̂).

## Differenze inevitabili

Disegni e colori propri; tasti della PicoCalc; la terza pagina dell'editor
raccoglie funzioni che la fx-CG50 raggiunge con altri tasti; frequenze intere
(0…100 000); Sin e Logistic con un metodo iterativo proprio (su dati mal
condizionati può trovare un altro minimo locale); ANOVA a due vie solo
bilanciata; la striscia «Stat Graph» di eActivity porta le liste, non gli
StatGraph. Dettagli in [ANALISI_FONTI §5](reports/ANALISI_FONTI_v1.18.0.md#5-differenze-inevitabili-previste).
