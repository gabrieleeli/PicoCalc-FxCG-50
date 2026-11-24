# Analisi delle fonti — v1.18.0: Statistics alla parità completa, Distribution Base

Scritto **prima** del codice della v1.18.0 (fase 0). Obiettivo della versione:

1. **Statistics** (capitolo 6 del manuale fx-CG50) alla **parità completa** sulla
   Pimoroni Pico Plus 2 W: ogni voce della tabella del §3 presente con un test
   dedicato, oppure documentata come differenza inevitabile (§5).
2. **Distribution** (capitolo 18) entra nel registro come modalità **Base**
   (risposta Q1 della v1.17.0), con la sezione `0x0100 + AppId` del record v12
   e un nucleo puro **condiviso** con il DIST di Statistics.

Regole che non cambiano ([CONTINUE.md](../../CONTINUE.md)): manuale fx-CG50
v3.60 solo come descrizione del comportamento (nessun codice, grafica o testo
copiato da Casio); **Raspberry Pi Pico 2 W congelato alla v1.16.0** (nessuna
funzione, schermata o crescita dell'immagine salvo correzioni di errori
condivise); codice nuovo solo sulla Plus, dietro `#if FXAI_BOARD_PICO_PLUS_2W`
con il codice della v1.16.0 nell'`#else`; cancello completo; un solo commit su
`main` in fast-forward; HARDWARE_TEST da **HW-246**; nessun «TESTED ON
HARDWARE» senza prova fisica.

## 1. Fonti

| # | Fonte | Uso |
|---|---|---|
| 1 | *fx-CG50 Software User's Guide* v3.60, cap. 6 (Statistical Graphs and Calculations), cap. 3 (List Function: editor delle liste, ordinamento, file di liste, CSV), cap. 18 (Distribution), Appendice α (messaggi d'errore) | descrizione del comportamento. Il PDF **non è scaricabile da questo container** (support.casio.com risponde 404, i siti europei 403): le voci sono descritte dalle analisi delle versioni precedenti (v1.11.0–v1.13.0 per liste, CSV, List File, VARS › STAT, DIST di Run-Matrix) e dalla conoscenza del comportamento della calcolatrice; i punti incerti sono le domande del §7 con la risposta scelta |
| 2 | Copia HTML parziale del manuale (manualslib, pagine 205–265 lette solo in parte) | conferme puntuali: menu GRAPH {GRAPH1, GRAPH2, GRAPH3, SELECT, SET} e tipo iniziale Scatter (p. 205); SET UP «Stat Wind» Auto/Manual (p. 256) e «Resid List» (p. 244); COPY della formula di regressione nell'elenco delle relazioni di GRAPH (p. 244); χ² 2-way con matrici Observed/Expected almeno 2×2 (pp. 245–246); ANOVA a due vie con CALC o DRAW del grafico d'interazione, Dependent e due liste di fattori (pp. 248–250); Binomial CD con una lista: solo la lista superiore e Lower = 0, nessun grafico (p. 263) |
| 3 | Codice a `main` = `aa2923c` (v1.17.0) | punto di partenza (§2) |
| 4 | scipy 1.18 e numpy (fuori dal firmware) | riferimenti numerici indipendenti dei test (`tools/stat_reference.py`, §6) |

### 1.1 Punto di partenza verificato

| Controllo | Esito |
|---|---|
| `git status`, `git fetch origin main` | branch `claude/eager-wozniak-bg3tqx` = `main` = `aa2923c`, albero pulito |
| `tests/run_host_tests.sh` (due profili) | **61/61** (Plus) e **54/54** (Pico 2 W), 4 min 46 s con 4 processori |
| Firmware della v1.17.0 ricompilato | Plus **2 625 472 B** (62 % di 4 MB; limite 3 774 873 B); Pico 2 W **1 944 228 B** (budget 2 064 384 B); nessun warning del compilatore (solo l'avviso di CMake sul picotool dell'SDK) |
| Strumenti | GCC 13 host, clang 18, arm-none-eabi-gcc, Pico SDK in `/opt/sdk/pico-sdk`, scipy/numpy installati per gli script di riferimento |

## 2. Cosa c'è oggi (v1.17.0)

| Parte | Stato |
|---|---|
| Nucleo `statistics.hpp` | 1-Var (con frequenze intere ≤ 100 000, quartili «Std»), 2-Var senza frequenze, 9 regressioni (ax+b, Med-Med, X², X³, X⁴, Log, ae^bx, ab^x, Power) con r, r², MSe sulla scala originale, Normal/Binomial/Poisson/t/χ²/F (densità e cumulate di base), Z e t a un campione, intervalli Z e t a un campione |
| `calc_library.hpp` (Run-Matrix) | **le 24 funzioni di distribuzione** della fx-CG50 (PD, CD e inverse di Normale, t, χ², F, Binomiale, Poisson, Geometrica, Ipergeometrica), PROB P( Q( R( t( |
| Controller `statistics_app.cpp` | editor di List 1–26 (`CellGrid`, riga SUB, intestazione «Fn» del List File), F1 GRAPH (popup: Scatter, xyLine, Histogram a classi automatiche, Box plot senza outlier), F2 CALC (popup: 1-Var, 2-Var, 9 regressioni, SET XList/YList), F3 TEST (popup: Z e t a un campione, intervalli Z e t), F4 DIST (10 calcolatori: Normal PD/CD/Inverse sinistra, Binomial PD/CD, Poisson PD/CD, t CD, χ² CD, F CD), F5 SORT di una lista, F6 cancellazione della lista, OPTN (CSV LOAD/SAVE·AS/SET, DEL-ALL di tutte le liste, SET); risultato della regressione con DRAW e COPY nella prima funzione vuota di GRAPH |
| Run-Matrix | VARS › STAT (n, x̄, Σx, Σx², σx, sx, minX, maxX, ȳ…, a–e, r, r², MSe, Q1, Q3, Med, Mod, H-Start, H-pitch) **ricalcolati dalle liste** a ogni lettura; x̂( e ŷ( per i 9 tipi |
| Workspace | `Settings::statXList`, `statYList`, `statRegType`, `listFile`, `subName`, CSV; nessuna sezione propria di Statistics |
| Test | `statistics_test` (63 controlli numerici su valori di tabella), parti di `apps_flow_test`, `lists_csv_test`, `apps_init_test`, `eactivity_test` (strisce Stat Graph e ‘LIST) |

## 3. Tabella di parità del capitolo 6 (voce per voce)

Legenda dello **stato v1.17.0**: ✔ presente come sulla fx-CG50, ◐ parziale,
✘ assente. **Test previsto**: ogni voce ha il proprio caso (id della voce nel
nome del caso) in `tests/statistics_flow_test.cpp` (controller, solo Plus) e,
per i numeri, in `tests/statistics_test.cpp` contro i riferimenti di scipy
(`tests/fixtures/stat_reference.inc`, §6).

### 3.1 Editor delle liste (cap. 3 e 6-1)

| Id | Voce (comportamento della fx-CG50) | v1.17.0 | Manca | Test previsto |
|---|---|---|---|---|
| E01 | Barra F1–F6 su tre pagine: **GRAPH CALC TEST INTR DIST ▷** / **SRT-A SRT-D DEL DEL-ALL INS ▷** / **ADD JUMP COLOR FILE CSV ▷** (la terza pagina raccoglie le funzioni che la fx-CG50 mette in altri tasti: JUMP, colore, List File, CSV) | ◐ una pagina con popup | pagine, INTR separato, F6 ▷ | etichette delle tre pagine, F6 cicla, SHIFT non cambia pagina |
| E02 | **SRT-A** con «How Many Lists?» (1–6), poi «Select Base List» e le liste secondarie riordinate come la base; liste di lunghezza diversa → Dimension ERROR | ◐ ordina solo la lista del cursore | domande, liste secondarie | 1 lista; 3 liste con la base al centro; lunghezze diverse; n fuori 1–6 |
| E03 | **SRT-D** come SRT-A in ordine decrescente | ◐ (SHIFT F5) | come E02 | 2 liste decrescenti, stabilità sui pari |
| E04 | **DEL**: cancella la cella e fa salire le seguenti | ◐ tasto Del | tasto F | cella centrale, ultima cella, lista vuota |
| E05 | **DEL-ALL**: cancella tutta la lista del cursore dopo la conferma | ◐ F6 senza conferma | conferma | conferma Sì/No, sottonome conservato |
| E06 | **INS**: inserisce una cella con 0 sopra il cursore | ✘ | tutto | inserimento in mezzo, limite 999 → Dimension ERROR |
| E07 | **ADD**: aggiunge una cella con 0 sotto il cursore | ✘ | tutto | in coda e in mezzo |
| E08 | **JUMP**: TOP, BOTTOM e riga n | ◐ Home/End | menu e riga | TOP/BOTTOM, riga oltre la fine |
| E09 | Riga **SUB** (sottonomi ≤ 8 caratteri), nascosta con SET UP Sub Name Off | ✔ (v1.11.0) | — | modifica, Off nasconde |
| E10 | **Colore della lista** (Blue, Red, Magenta, Green, Cyan, Yellow, Black): numeri della colonna nel colore scelto, usato dai grafici con Color Link | ✘ | tutto | scelta, persistenza nella sezione, pixel della colonna |
| E11 | **Formula nella testa della lista** (`List 1×2`, `List 1+List 2`, `Seq(…)`): la lista è generata e ricalcolata quando cambiano le liste citate; una cella modificata a mano toglie la formula; formula che cita sé stessa o un ciclo → Circular ERROR | ✘ | tutto | generazione, ricalcolo dopo una modifica di List 1, ciclo, modifica manuale, persistenza |
| E12 | **List File 1–6** dall'editor (FILE) e dal SET UP, intestazione «F n» | ◐ solo SET UP | FILE | cambio di file, liste conservate per file |
| E13 | **CSV** LOAD (dalla colonna del cursore), SAVE·AS, SET (separatore, decimale) | ✔ (v1.11.0, OPTN) | tasto CSV | load/save sull'archivio simulato |
| E14 | Celle con espressioni valutate, 26 liste da 999 elementi (budget della scheda) | ✔ | — | (esistenti in `lists_csv_test`) |
| E15 | Cursore, scorrimento di 26 colonne, numeri di riga | ✔ | — | (esistenti) |

### 3.2 Grafici statistici (6-2…6-9)

| Id | Voce | v1.17.0 | Manca | Test previsto |
|---|---|---|---|---|
| G01 | Menu **GRAPH**: GRAPH1, GRAPH2, GRAPH3, SELECT, SET; GRAPHn disegna lo StatGraph n | ◐ popup di 4 tipi | tutto il menu | ogni tasto apre il grafico giusto |
| G02 | **SET di StatGraph1–3**: Graph Type, XList, YList, Frequency (1 o List n), Mark Type (□ × • ○), Graph Color, Color Link (Off, X&Freq, OnData), Outliers (MedBox), Data/Display/% Sto (Pie), Data1–3/Stick Style (Bar); voci mostrate secondo il tipo; **DefG** riporta lo StatGraph scelto ai valori iniziali (§7 Q1) | ✘ | tutto | voci per tipo, cambio di tipo, DefG, persistenza |
| G03 | **SELECT**: DrawOn/DrawOff dei tre grafici e DRAW di quelli accesi insieme | ✘ | tutto | 3 grafici accesi, 0 accesi → nessun grafico (messaggio) |
| G04 | **Scatter** con frequenza (punto ripetuto conta una volta, frequenza 0 non disegnata) e marcatore | ◐ senza frequenza e con un solo marcatore | frequenza, marcatori | pixel dei marcatori □ × • ○ |
| G05 | **xyLine** (punti uniti nell'ordine delle liste) | ✔ | colore/marcatore | segmento tra due punti |
| G06 | **NPPlot** (dati ordinati contro i quantili normali, posizioni (i−0,5)/n, §7 Q3) | ✘ | tutto | coordinate dei punti contro scipy |
| G07 | **Histogram** con **Start** e **Width** chiesti prima del disegno (valori proposti: minX e (maxX−minX)/10), frequenze | ◐ classi automatiche | Start/Width, frequenze | conteggi per classe, dato sul bordo |
| G08 | **MedBox** con **Outliers** On/Off (baffi fino ai dati entro Q1−1,5·IQR e Q3+1,5·IQR, outlier come punti) | ◐ senza outlier | outlier | baffi e outlier contro il calcolo manuale |
| G09 | **N-Dist**: curva normale con x̄ e σx dei dati (§7 Q4) | ✘ | tutto | picco della curva = 1/(σ√2π) |
| G10 | **Broken**: spezzata dei centri delle cime dell'istogramma | ✘ | tutto | vertici della spezzata |
| G11 | **Bar**: Data1–3 (lista o None), barre affiancate per indice, Stick Style Length/Horiz | ✘ | tutto | altezze in pixel, stile orizzontale |
| G12 | **Pie**: Data, Display (%, Data, None), % Sto (None o List n con le percentuali) | ✘ | tutto | angoli delle fette, % salvate in lista, dato negativo → Math ERROR |
| G13 | **Regressioni come tipo di grafico** (X, Med, X², X³, X⁴, Log, Exp ae^bx, Exp ab^x, Power, Sin, Logistic): punti + curva | ◐ solo dopo CALC | tipi nel SET | curva disegnata per ogni tipo |
| G14 | SET UP **Stat Wind** Auto (V-Window dai dati di tutti i grafici accesi) / Manual (V-Window di Graph) | ✘ (sempre automatico) | tutto | finestra Auto contro il calcolo; Manual usa `ws.view` |
| G15 | SET UP **Q1Q3 Type** Std (mediane delle metà) / OnData (dato con frequenza cumulata > 1/4, media di due dati se = 1/4; §7 Q2) | ✘ | OnData | quartili dei due metodi su 6 insiemi |
| G16 | **TRACE** sui grafici: punto per punto (Scatter, xyLine, NPP), lungo la curva (regressioni, N-Dist), classi (Histogram) con le coordinate in basso | ✘ | tutto | coordinate dopo → ← |
| G17 | **CALC dal grafico**: 1-VAR, 2-VAR e regressioni sulle liste dello StatGraph disegnato; DRAW sovrappone la curva | ◐ | dal grafico | regressione con le liste di StatGraph2 |
| G18 | Colori: Graph Color per grafico (valori iniziali Blue, Red, Green), Color Link X&Freq (colore per frequenza) e OnData (colore della lista X) | ✘ | tutto | pixel nel colore scelto |
| G19 | **V-Window** (SHIFT F3) dalla schermata del grafico per Stat Wind Manual | ✘ | tutto | modifica e ridisegno |

### 3.3 Regressioni (6-10…6-21) e CALC (6-22…6-30)

Formule dei coefficienti e dell'errore quadratico medio come descritte dal
manuale: MSe = Σ(scarto)²/(n−2) per i modelli a due parametri, sulla scala
**trasformata** per Log (y contro ln x), Exp (ln y) e Power (ln y contro ln x);
/(n−3), /(n−4), /(n−5) per X², X³, X⁴; Sin e Logistic sulla scala originale
con /(n−2) (§7 Q13).

| Id | Voce | v1.17.0 | Manca | Test previsto |
|---|---|---|---|---|
| R01 | Lineare **ax+b**: a, b, r, r², MSe | ◐ (MSe /(n−2) già giusto) | frequenze | contro `scipy.stats.linregress` |
| R02 | Lineare **a+bx** (stessa retta con i nomi scambiati) | ✘ | tutto | a, b scambiati rispetto a R01 |
| R03 | **Med-Med**: a, b (tre gruppi con la regola n mod 3) e i punti riassuntivi x1…y3 (VARS › PTS) | ◐ senza PTS | PTS, frequenze | contro il calcolo dei gruppi in numpy |
| R04–R06 | **X², X³, X⁴**: coefficienti, R², MSe /(n−3…n−5) | ◐ MSe e R² da rivedere | frequenze, MSe del manuale | contro `numpy.polyfit` |
| R07 | **Log** y = a + b·ln x: a, b, r, r², MSe | ◐ MSe sulla scala sbagliata | MSe | contro linregress(ln x, y) |
| R08 | **Exp ae^bx**: a, b, r, r², MSe (scala ln y) | ◐ | MSe | contro linregress(x, ln y) |
| R09 | **Exp ab^x** | ◐ | MSe | come R08 con b = e^pendenza |
| R10 | **Power** ax^b | ◐ | MSe | linregress(ln x, ln y) |
| R11 | **Sin** y = a·sin(bx+c)+d: a, b, c, d, MSe; radianti qualunque sia Angle (§7 Q5); ≥ 4 dati | ✘ | tutto | `scipy.optimize.least_squares` dallo stesso avvio, SSres e coefficienti |
| R12 | **Logistic** y = c/(1+a·e^(−bx)): a, b, c, MSe; ≥ 3 dati | ✘ | tutto | `least_squares`, SSres |
| R13 | Schermata dei risultati con r, r², MSe secondo il tipo (Med-Med: solo a, b; Sin e Logistic senza r) | ◐ | righe per tipo | righe di ogni tipo |
| R14 | **Residui** nella lista del SET UP **Resid List** (None o List n): yᵢ − f(xᵢ) dopo ogni regressione | ✘ | tutto | lista dei residui per ogni tipo |
| R15 | **COPY**: elenco delle relazioni di GRAPH (Y1–Y20 con il contenuto), EXE scrive la formula nella Yn scelta | ◐ prima Yn vuota | scelta | copia in Y7, formula valutata = ŷ |
| R16 | **DefG** (§7 Q1): nello SET dello StatGraph riporta il grafico scelto ai valori iniziali | ✘ | tutto | (con G02) |
| R17 | **x̂( e ŷ(** in Run-Matrix per tutti i tipi (Quad: due valori; X³/X⁴: radici reali in lista; Sin: Math ERROR, §7 Q14) | ◐ 9 tipi | a+bx, Sin, Logistic, X³/X⁴ | ogni tipo contro la formula |
| R18 | **Frequenze** nelle regressioni (2Var Freq di CALC, Frequency dello StatGraph): come dati ripetuti | ✘ | tutto | frequenza 2 = dato duplicato, per ogni tipo |
| C01 | Menu **CALC**: 1-VAR, 2-VAR, REG, SET; REG: X (ax+b, a+bx), Med, X², X³, X⁴, Log, Exp (ae^bx, ab^x), Pwr, Sin, Lgst | ◐ popup | menu a tasti | ogni voce raggiunta |
| C02 | **SET** di CALC: 1Var XList, 1Var Freq, 2Var XList, 2Var YList, 2Var Freq | ◐ XList/YList | frequenze, 1Var XList separata | persistenza, uso nei calcoli |
| C03 | **1-VAR**: x̄, Σx, Σx², σx, sx, n, minX, Q1, Med, Q3, maxX, Mod, Mod:n, Mod:F | ◐ senza Mod:n, senza frequenze dalla UI | Mod:n, Freq | contro numpy, con e senza frequenze, Q1Q3 Std/OnData |
| C04 | **2-VAR**: x̄, Σx, Σx², σx, sx, n, ȳ, Σy, Σy², σy, sy, Σxy, minX, maxX, minY, maxY | ◐ senza frequenze | Freq | contro numpy |
| C05 | **VARS › STAT** in Run-Matrix: X, Y, GRAPH, PTS (x1…y3), INPUT (n, x̄, sx, n1, n2, x̄1, x̄2, sx1, sx2, sp), RESULT (TEST, INTR, DIST) | ◐ X, Y, GRAPH ricalcolati | PTS, INPUT, RESULT, valori dell'ultimo calcolo | ogni nome dopo un calcolo, persistenza dopo riavvio |

### 3.4 TEST (6-31…6-46)

Comune a tutti: ingresso **List/Var** dove la fx-CG50 lo prevede, verso
dell'ipotesi (≠ < >), **Save Res** (None o List n: i risultati in colonna),
esecuzione con **CALC** e **DRAW** (grafico della distribuzione della
statistica con l'area del p-value evidenziata, statistica e p scritti).

| Id | Voce | v1.17.0 | Manca | Test previsto (riferimento) |
|---|---|---|---|---|
| T01 | **1-Sample Z**: z, p, x̄, sx, n | ◐ List senza Freq, nessun DRAW | Var, Freq, Save Res, DRAW | `scipy.stats.norm` |
| T02 | **2-Sample Z**: z, p, x̄1, x̄2, sx1, sx2, n1, n2 | ✘ | tutto | norm |
| T03 | **1-Prop Z**: z, p, p̂, n (x intero ≤ n) | ✘ | tutto | norm |
| T04 | **2-Prop Z**: z, p, p̂1, p̂2, p̂, n1, n2 | ✘ | tutto | norm |
| T05 | **1-Sample t**: t, p, x̄, sx, n | ◐ | Var, Freq, Save Res, DRAW | `scipy.stats.ttest_1samp` |
| T06 | **2-Sample t** con **Pooled** On/Off: t, p, df (Welch se Off), x̄1, x̄2, sx1, sx2, sp, n1, n2 | ✘ | tutto | `ttest_ind(equal_var=…)` |
| T07 | **LinearReg t** (β & ρ ≠ < > 0): t, p, df, a, b, s, r, r² | ✘ | tutto | linregress + t |
| T08 | **χ² GOF**: Observed, Expected, df; χ², p, df; **CNTRB** in una lista | ✘ | tutto | `scipy.stats.chisquare` |
| T09 | **χ² 2-way**: Observed (Mat), Expected scritta in una Mat; χ², p, df | ✘ | tutto | `chi2_contingency(correction=False)` |
| T10 | **2-Sample F** (σ1 ≠ < > σ2): F, p, sx1, sx2, x̄1, x̄2, n1, n2 | ✘ | tutto | `scipy.stats.f` |
| T11 | **ANOVA a una via** (Factor A, Dependent): righe A ed ERR con df, SS, MS, F, p; in **step::Task** con AC | ✘ | tutto | `f_oneway` |
| T12 | **ANOVA a due vie** (Factor A, Factor B, Dependent, disegno bilanciato §7 Q6): righe A, B, AB (con repliche), ERR; DRAW = grafico d'interazione; step::Task | ✘ | tutto | formule dei disegni bilanciati in numpy |
| T13 | **DRAW** dei test Z, t, χ², F (area evidenziata, una o due code) | ✘ | tutto | pixel dell'area, testo z/p |
| T14 | **Save Res** e variabili di risultato (VARS › STAT › RESULT › TEST) | ✘ | tutto | lista scritta, VARS |

### 3.5 INTR (6-47…6-53)

| Id | Voce | v1.17.0 | Manca | Test previsto |
|---|---|---|---|---|
| I01 | **1-Sample Z Int**: Left, Right, x̄, sx, n | ◐ List senza Freq | Var, Freq, Save Res | norm.ppf |
| I02 | **2-Sample Z Int**: Left, Right, x̄1, x̄2, sx1, sx2, n1, n2 | ✘ | tutto | norm.ppf |
| I03 | **1-Prop Z Int**: Left, Right, p̂, n | ✘ | tutto | norm.ppf |
| I04 | **2-Prop Z Int**: Left, Right, p̂1, p̂2, n1, n2 | ✘ | tutto | norm.ppf |
| I05 | **1-Sample t Int**: Left, Right, x̄, sx, n | ◐ | Var, Freq, Save Res | t.ppf |
| I06 | **2-Sample t Int** con Pooled: Left, Right, df, x̄1, x̄2, sx1, sx2, sp, n1, n2 | ✘ | tutto | t.ppf (Welch) |

### 3.6 DIST (6-54…6-73)

Calcolatori della fx-CG50, ognuno con ingresso **List** (risultati in una lista:
ListAns e Save Res) o **Var** (risultato in Ans), **CALC**, **DRAW** per le
distribuzioni continue (PD: curva e punto; CD e inverse: area evidenziata). Le
inverse discrete e le liste lunghe girano in **step::Task** con AC.

| Id | Voce | v1.17.0 | Manca | Test previsto |
|---|---|---|---|---|
| D01 | **Normal**: Npd (x, σ, μ), Ncd (Lower, Upper; p, z:Low, z:Up), InvN (Tail Left/Right/Central, Area; x1Inv, x2Inv) | ◐ solo Var, InvN sinistra | List, Tail R/C, DRAW | `scipy.stats.norm` |
| D02 | **Student-t**: tpd, tcd (p, t:Low, t:Up), Invt (area superiore) | ◐ solo tcd | tpd, Invt, List, DRAW | `scipy.stats.t` |
| D03 | **χ²**: Cpd, Ccd, InvC (area superiore) | ◐ solo Ccd | Cpd, InvC, List, DRAW | `scipy.stats.chi2` |
| D04 | **F**: Fpd, Fcd, InvF (area superiore) | ◐ solo Fcd | Fpd, InvF, List, DRAW | `scipy.stats.f` |
| D05 | **Binomial**: Bpd, Bcd (Var: Lower/Upper; List: solo U.List con Lower 0), InvB | ◐ Bpd/Bcd ≤ x | Lower, List, InvB | `scipy.stats.binom` |
| D06 | **Poisson**: Ppd, Pcd, InvP (μ) | ◐ Ppd/Pcd | Lower, List, InvP | `scipy.stats.poisson` |
| D07 | **Geometric**: Gpd, Gcd, InvG | ✘ (solo in Run-Matrix) | tutto | `scipy.stats.geom` |
| D08 | **Hypergeometric**: Hpd, Hcd, InvH (n, M, N) | ✘ (solo in Run-Matrix) | tutto | `scipy.stats.hypergeom` |
| D09 | Risultati in **liste** (ListAns, Save Res) con ingresso List; un elemento fuori dominio → errore con il numero dell'elemento | ✘ | tutto | lista di 5 elementi per ogni calcolatore |
| D10 | **DRAW** delle continue (curva, punto, area) e risultato scritto sul grafico | ✘ | tutto | pixel dell'area |
| D11 | Inverse e liste lunghe in **step::Task** con AC («Break») | ✘ | tutto | AC a metà di una lista di 999 inverse |
| D12 | **Ans**: il risultato p (o x) di un calcolo Var va in Ans | ✘ | tutto | Ans dopo Ncd e InvN |

### 3.7 SET UP, Ans e variabili condivise

| Id | Voce | v1.17.0 | Manca | Test previsto |
|---|---|---|---|---|
| U01 | SET UP di Statistics (SHIFT MENU): **Stat Wind**, **Resid List**, **Q1Q3 Type**, **List File**, **Sub Name**, Angle, Display | ◐ (List File, Sub Name, Angle e Display nel SET UP globale) | Stat Wind, Resid List, Q1Q3 Type nel SET UP della modalità | ogni voce cambia il comportamento |
| U02 | Persistenza: StatGraph1–3, SET di CALC, colori e formule delle liste, SET UP, ultimi risultati in una **sezione propria del record v12** (`0x0101`), isolata se danneggiata | ✘ | tutto | andata e ritorno, sezione danneggiata, record v11 della v1.16.0 |
| A01 | **VARS › STAT** con i valori dell'**ultimo calcolo** (come la fx-CG50), non ricalcolati a ogni lettura | ◐ ricalcolati | valori salvati | dopo un calcolo e un cambio delle liste |
| A02 | Ans e ListAns condivisi con Run-Matrix (D12, D09) | ✘ | tutto | Run-Matrix legge Ans dopo DIST |

## 4. Progetto (solo Pimoroni Pico Plus 2 W)

### 4.1 Struttura

| Parte | File | Note |
|---|---|---|
| Nucleo statistico esteso | `include/stat_engine.hpp`, `src/stat_engine.cpp` (servizio **Stats**, solo Plus) | 1/2-Var con frequenze e quartili Std/OnData, 12 regressioni con frequenze, residui, punti Med-Med, x̂/ŷ, test Z/t/χ²/F, ANOVA (step::Task), intervalli, grafici come modelli puri (classi, baffi, NPP, torta, barre); stato persistente `stats::PlusState` con codifica della sezione `0x0101` |
| Nucleo delle distribuzioni (**condiviso**) | `include/dist_core.hpp`, `src/dist_core.cpp` (servizio **Stats**) | tabella delle 24 funzioni (nome, parametri, dominio, continua/discreta, grafico), calcolo con il motore unico `calc_library` (le stesse funzioni di OPTN › STAT › DIST di Run-Matrix), ingresso List con `dist::ListTask` (step::Task, un elemento per unità), modello del grafico (densità campionata e intervallo evidenziato) |
| Controller di Statistics | `src/apps/statistics_plus_app.cpp` (editor, menu, moduli, risultati), `src/apps/stat_plot.cpp` (grafici, TRACE, V-Window) | sul `ModeShell` (FKeyBar a pagine e sottomenu, AskBox, ConfirmBox); `statistics_app.cpp` tiene il codice della v1.16.0 nell'`#else`; la classe `StatisticsApp` diventa pImpl solo sulla Plus |
| Distribution | `include/distribution.hpp`, `src/distribution.cpp` (ModeCore), `src/apps/distribution_app.cpp` | §4.3 |
| Run-Matrix ed espressioni | `src/expression.cpp`, `src/calc_library.cpp`, `src/apps/run_matrix_app.cpp`, `src/workspace.cpp` | solo blocchi `#if FXAI_BOARD_PICO_PLUS_2W`: nomi nuovi di VARS › STAT, x̂/ŷ per i tipi nuovi, valori dell'ultimo calcolo |

Registro (Plus): Statistics resta **Available** con i servizi `ModeShell`,
`StepTask` in più e le sezioni dati `View` (V-Window per Stat Wind Manual),
`Memory` (Ans), `Matrices` (matrici del χ² 2-way, ListAns) e la sezione nuova
`statistics`; Run-Matrix non include `statistics.hpp` né `stats::` (regola di
`architecture_test` invariata: legge tutto dal workspace).

### 4.2 Sezione `0x0101` (Statistics, record v12, solo Plus)

Contenuto (versione 1): StatGraph1–3 (tipo, liste, frequenza, marcatore,
colore, Color Link, outlier, opzioni di Pie e Bar, DrawOn/Off, Start/Width
dell'istogramma), SET di CALC (1Var XList/Freq, 2Var XList/YList/Freq: le
liste 2Var restano anche in `Settings::statXList/statYList` per compatibilità
col record v11), SET UP (Stat Wind, Resid List, Q1Q3 Type), colori e formule
delle 26 liste, ultimi risultati (regressione con tipo e coefficienti, punti
Med-Med, 1/2-Var, INPUT, RESULT di TEST/INTR/DIST). Scritta solo se diversa dai
valori iniziali; codifica con numeri finiti soltanto (una sezione con NaN o
infiniti è rifiutata e isolata, come Financial nella v1.17.0). Un record v11
della v1.16.0 non ha la sezione: Statistics parte dai valori iniziali con le
liste e il SET 2Var del record.

### 4.3 Distribution (cap. 18) come modalità **Base**

| Voce | Decisione |
|---|---|
| Registro | `AppId::Distribution` **in coda all'enum solo sulla Plus** (dopo Symbols: gli id e le sezioni esistenti non cambiano; sul Pico 2 W `AppCount` resta 23 e l'immagine non cambia); nome «Distribution», tasto diretto **O** (il menu passa a 24 icone, sei righe piene da quattro); stato **Base** |
| Ruolo | scelta della distribuzione (Normale, t, χ², F, Binomiale, Poisson, Geometrica, Ipergeometrica) e della funzione (PD, CD, inversa), parametri con ingresso Var o List, CALC, risultato in Ans / ListAns, grafico con l'area per le continue |
| `missing` | «Base v1.18.0: mancano per la parità (cap. 18) lo schermo diviso grafico/parametri con l'aggiornamento dal vivo, lo spostamento dei limiti con il cursore sul grafico, la tabella dei valori della distribuzione e le strisce di eActivity.» |
| Nucleo | `dist_core.hpp` (condiviso con il DIST di Statistics: **una sola tabella e un solo calcolo**) + `distribution.hpp` (stato della modalità: funzione scelta, parametri di ogni funzione, Data, ultimo risultato; encode/decode/reset/validate) |
| Sezione | **`0x0117`** (`0x0100 + 23`), scritta solo se diversa dai valori iniziali, isolata se danneggiata |
| Tipo di file | **nessuno** (§7 Q9): lo stato è piccolo e sta nella sezione; i risultati vanno nelle liste |
| Servizi | Expression, Widgets/Editor/Format, Stats, Storage, ModeShell, TypedFiles (solo il guscio comune), StepTask, ModeCore; dati Settings, Memory, Lists, Matrices (ListAns), Distribution |
| Operazioni lunghe | inverse e liste in `dist::ListTask` con AC |
| Test | `apps_init_test` (sequenza e sonda), `base_modes_test` (nucleo: codifica, validazione, sezione danneggiata), `base_flow_test` (flusso: menu, CALC, List, DRAW, AC, Ans), `architecture_test` (Base sulla Plus, assente sul Pico 2 W) |

### 4.4 Cosa non cambia sul Pico 2 W

Nessun file nuovo compilato (`stat_engine.cpp`, `dist_core.cpp`,
`statistics_plus_app.cpp`, `stat_plot.cpp`, `distribution*.cpp` solo con
`FXAI_BOARD_PICO_PLUS_2W`, dichiarati in `cmake/fxai_sources.cmake` così il
controllo della mappa del linker del Pico 2 W verifica che non entrino);
`statistics_app.cpp`, `statistics.hpp/.cpp`, `expression.cpp`,
`calc_library.cpp`, `workspace.cpp`, `run_matrix_app.cpp` cambiano solo dentro
blocchi `#if FXAI_BOARD_PICO_PLUS_2W`. Controllo: immagine del Pico 2 W con la
stessa dimensione della v1.17.0 a meno della stringa della versione, e
schermate del simulatore identiche.

## 5. Differenze inevitabili (previste)

| # | Voce | Motivo |
|---|---|---|
| D1 | Disegni, font e colori propri (marcatori, palette di 7 colori del progetto) | nessuna grafica Casio copiata |
| D2 | Tasti della PicoCalc: F6 = SHIFT+F1, SHIFT = F7, OPTN = F8, VARS = F9, SET UP = F10 | tastiera diversa (come nelle versioni precedenti) |
| D3 | La terza pagina dell'editor (ADD, JUMP, COLOR, FILE, CSV) riunisce funzioni che la fx-CG50 raggiunge con altri tasti | tastiera con meno tasti dedicati |
| D4 | Frequenze intere (0…100 000) | regola della v1.13.1 (E09) mantenuta: frequenze decimali non accettate |
| D5 | Coefficienti di Sin e Logistic: metodo iterativo proprio (stima iniziale + Levenberg–Marquardt); su dati mal condizionati può fermarsi in un altro minimo locale rispetto alla calcolatrice | l'algoritmo della fx-CG50 non è descritto |
| D6 | ANOVA a due vie solo con disegno bilanciato | §7 Q6 |
| D7 | La striscia «Stat Graph» di eActivity porta le liste (come nella v1.16.0), non gli StatGraph | il capitolo 10 è chiuso dalla v1.16.0; nessun cambio di formato dei documenti |

## 6. Verifica numerica indipendente

`tools/stat_reference.py` (fuori dal firmware) calcola con **scipy/numpy** i
valori attesi di insiemi di dati fissi (scritti nello script) e genera
`tests/fixtures/stat_reference.inc`; `statistics_test` confronta il nucleo con
le **tolleranze dichiarate** sotto. Lo script ha `--check` (rigenera in memoria
e confronta col file salvato): il cancello lo esegue.

| Grandezze | Tolleranza |
|---|---|
| Statistiche descrittive (somme, medie, deviazioni, quartili) | relativa 1e−12 (assoluta 1e−12 vicino a 0) |
| Regressioni in forma chiusa (lineari, polinomiali, Log, Exp, Power), r, r², MSe | relativa 1e−9 |
| Sin e Logistic (iterative) | SSres relativa 1e−9 rispetto a `least_squares` dallo stesso avvio; coefficienti relativa 1e−6 |
| Densità e cumulate continue, p dei test | relativa 1e−9, assoluta 1e−14 |
| Discrete (PD, CD) | relativa 1e−10 |
| Inverse continue | relativa 1e−8 |
| Inverse discrete, df, conteggi | esatte |
| ANOVA (SS, MS, F, p) | relativa 1e−9 |

## 7. Domande aperte e risposte scelte

Il manuale non è leggibile per intero da qui (§1); per ogni punto incerto la
risposta scelta, con il motivo. Sono tutte modificabili senza cambiare il
formato della sezione.

| # | Domanda | Risposta scelta | Motivo |
|---|---|---|---|
| Q1 | Che cosa fa **DefG**? | Tasto F5 del SET di StatGraph: riporta lo StatGraph scelto ai valori iniziali (Scatter, List 1, List 2, Frequency 1, □, colore iniziale del grafico, DrawOn per StatGraph1) | la pagina che lo descrive non era leggibile; un ripristino dei valori iniziali nel SET è la lettura più coerente con «Def(ault) G(raph)» e non tocca altri dati |
| Q2 | Regola **OnData** dei quartili | Q1 = primo dato (ordinato, con le frequenze) la cui frequenza cumulata relativa supera 1/4; se è esattamente 1/4, media di quel dato e del successivo; Q3 lo stesso con 3/4 | è la descrizione «sui dati» della fx-CG50 (quartili che sono dati, non mediane delle metà) |
| Q3 | Posizioni del **NPPlot** | quantile normale di (i − 0,5)/n | posizione di Hazen, simmetrica; documentata |
| Q4 | Deviazione della curva **N-Dist** | σx (popolazione) | la curva normale della fx-CG50 usa x̄ e σx dei dati |
| Q5 | Unità d'angolo della **regressione sinusoidale** | sempre radianti; la formula copiata in GRAPH (COPY) porta il suffisso ʳ così il grafico è giusto anche in Deg/Gra | la fx-CG50 calcola Sin in radianti |
| Q6 | **ANOVA a due vie** con celle di numerosità diverse | Dimension ERROR («disegno non bilanciato») | le formule dei disegni bilanciati sono quelle del manuale; un disegno sbilanciato richiede scelte (somme di tipo I/II/III) che il manuale non descrive |
| Q7 | Ambito del **colore delle liste** | per indice di lista (List 1…26), indipendente dal List File | semplice e prevedibile; persistente nella sezione |
| Q8 | **Formula nella testa**: cosa succede se si modifica una cella? | la formula viene tolta (la lista resta con i valori) | evita che il ricalcolo cancelli il dato appena scritto |
| Q9 | **Distribution** ha bisogno di un tipo di file? | no | stato piccolo (sezione `0x0117`); i risultati vanno in liste/Ans |
| Q10 | Posizione e tasto di **Distribution** nel menu | in coda (24ª icona), tasto **O** | non sposta gli id né le sezioni delle modalità esistenti; completa la sesta riga del menu |
| Q11 | Cosa va in **Ans** | DIST con ingresso Var: il risultato principale (p, o x per le inverse; per InvN centrale x1); TEST e INTR solo nelle variabili di VARS › STAT › RESULT | il risultato della DIST è un numero che si riusa in Run-Matrix; i test hanno più risultati |
| Q12 | **Save Res** | None o List 1–26; i risultati mostrati (numeri) in colonna, nell'ordine della schermata | comportamento della fx-CG50 |
| Q13 | **MSe** di Sin e Logistic | Σ(scarto)²/(n−2) sulla scala originale | formula del manuale per i modelli non lineari |
| Q14 | **x̂(** con Sin | Math ERROR («x̂ non disponibile per Sin»); X³ e X⁴: radici reali in una lista; Logistic: formula inversa | Sin non è invertibile in modo unico |
| Q15 | Valori iniziali degli StatGraph | StatGraph1 DrawOn, 2 e 3 DrawOff; colori Blue, Red, Green; tutti Scatter List 1 / List 2, Frequency 1, □ | valori iniziali della calcolatrice (tipo Scatter, p. 205) |
| Q16 | VARS › STAT dopo una modifica delle liste | valori dell'ultimo calcolo (non ricalcolati); con un record v11 senza sezione, ricalcolo dalle liste come nella v1.17.0 | la fx-CG50 conserva i risultati dell'ultimo calcolo |

## 8. Piano dei test, del fuzzing e del simulatore

| Verifica | Contenuto |
|---|---|
| `statistics_test` (esteso) | nucleo della Plus contro `stat_reference.inc`: ogni voce numerica dei §3.3–3.6 |
| `statistics_flow_test` (nuovo, Plus) | un caso per voce E01…E13, G01…G19, R13…R18, C01…C05, T01…T14, I01…I06, D01…D12, U01, U02, A01, A02 (tasti dal firmware simulato, schermate, pixel, liste e variabili) |
| `apps_init_test` | Distribution nei casi C1–C7 con K1–K6; sequenza di Statistics della Plus (F2 F1 invece del popup) |
| `base_modes_test`, `base_flow_test` | Distribution: nucleo e flusso |
| `architecture_test` | Statistics Available con i servizi nuovi; Distribution Base solo sulla Plus; Pico 2 W congelato (insieme invariato, 23 modalità) |
| Fuzzing (≥ 300 s ciascuno) | nuovo `fuzz_stats` (sezione `0x0101`, nucleo con dati casuali, dist_core); toccati `fuzz_workspace` (sezioni nuove), `fuzz_modes` (sezione di Distribution), `fuzz_ui` (controller) |
| Simulatore | `tests/sim/statistics_v118.txt`, screenshot dal **148**; gli screenshot 01–147 identici (quelli del Pico 2 W e di Statistics della v1.11.0 compresi: il profilo Plus mostra la barra nuova, vedi SVILUPPO se cambia) |
| HARDWARE_TEST | `docs/HARDWARE_TEST_v1.18.0.md` da HW-246 |
