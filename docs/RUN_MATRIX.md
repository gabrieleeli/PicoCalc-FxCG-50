# Run-Matrix — v1.11.0

Run-Matrix mostra **calcoli e risultati**; grafici, statistiche, tabelle e
conversioni sono nelle rispettive modalità (i comandi grafici eseguiti qui li
disegna GRAPH). Riferimento di comportamento: manuale fx-CG50, capitoli 1-3…1-31,
2-1…2-63 (matrici e vettori 2-42…2-63), 3-7…3-20 (liste e CSV), 5 (SKETCH) e 8
(comandi). Stato per voce: [FEATURE_MATRIX.md](FEATURE_MATRIX.md).

Novità della v1.11.0 (dettagli nelle sezioni sotto): **List 1–26** con i
sottonomi, **file CSV** di liste e matrici, schermate **MatAns/ListAns**,
celle dell'editor MAT/VCT con **espressioni esatte e complesse** e
scorrimento delle colonne, modalità Line con **INS** (sovrascrittura),
**SHIFT+INS**, frazioni **⌟** e **risultati scorrevoli**, **simboli
ingegneristici** e display **Eng** con i simboli, **Simp** delle frazioni,
modalità **Base-N intera** (Dec/Hex/Bin/Oct, operatori bit a bit), **comandi
grafici** (Plot, F-Line, Graph Y=…) disegnati da GRAPH.

Novità della v1.10.0: **matrici, vettori e
liste** come valori del parser, **editor MAT/VCT**, modalità **Math/Line**,
**selezione parziale CLIP**, **parentesi colorate** per livello e **cursore
sulla causa di un errore**.

![List 1–26 e sottonomi](screenshots/52_lists_sub.png) ![CSV SAVE·AS](screenshots/53_csv_saveas.png) ![MatAns](screenshots/54_matans.png) ![Celle esatte](screenshots/55_mat_editor_exact.png) ![Line: ⌟ e INS](screenshots/56_line_ins.png)

![Risultato scorrevole](screenshots/57_long_result.png) ![Eng con i simboli](screenshots/58_eng_symbols.png) ![Simp](screenshots/59_simp.png) ![Base-N](screenshots/60_base_n.png) ![Comandi grafici](screenshots/61_graph_commands.png)

![Editor MAT/VCT](screenshots/44_mat_editor_cells.png) ![Matrici e liste](screenshots/46_run_matrix_matrices.png) ![Parentesi colorate](screenshots/47_parentheses.png) ![CLIP](screenshots/48_clip.png) ![Cursore sull'errore](screenshots/50_error_cursor.png) ![Modalità Line](screenshots/51_line_mode.png)

![Risultati esatti](screenshots/12_run_matrix_exact.png) ![Periodo e approssimazioni](screenshots/13_run_matrix_periodic.png)

## Tasti

| Tasto | Normale | Dopo SHIFT (F7) |
|---|---|---|
| F1 | frazione `□/□` (prende l'operando a sinistra); in Line `⌟` | frazione mista `□ □/□`; in Line `⌟` |
| F2 | radice `√□` | quadrato `□²` |
| F3 | potenza `□^□` | π |
| F4 | menu MATH | `→` (memorizza) |
| F5 | **S⇔D** sull'ultimo risultato | **a b/c ⇔ d/c** |
| F6 | `Ans` | θ |
| OPTN (F8) | CALC, PROB, NUMERIC (anche **▶Simp**), ANGLE, COMPLEX, HYPERBL, LOG, LOGIC, RELAZIONI, CONST, **MAT/VCT**, **LIST**, **ESYM** (simboli ingegneristici), **SKETCH** (comandi grafici), RISULTATO, **BASE-N** (d h b o, And Or Xor Xnor Not Neg, ▶Dec ▶Hex ▶Bin ▶Oct) | |
| DEL | cancella a sinistra | **INS**: in Line alterna sovrascrittura/inserimento (barra di stato `OVR`), in Math il prossimo modello prende come argomento il valore a destra del cursore |
| VARS (F9) | A~Z (con il valore esatto), Ans/PreAns, V-WIN di Graph, TABLE, EQUATION (SimRes, SimCoef, PlyRes, PlyCoef: matrici di EQUATION, v1.12.0, [EQUATION.md](EQUATION.md#5-uso-dei-risultati)) | |
| ↑ | entra nella cronologia (con il cursore fuori dai modelli) | |
| EXE | calcola; su riga vuota ripete l'ultimo calcolo | |
| EXIT | cancella l'input; su input vuoto torna al menu | |
| AC (Shift+Esc) | cancella l'input; su input vuoto cancella la cronologia | |
| SHIFT `3` | | **editor MAT/VCT** (fx-CG50 SHIFT 3 'MAT/VCT') |
| SHIFT `8` | | **CLIP**: inizia la selezione |
| SHIFT `9` | | **PASTE**: incolla gli appunti |
| Ctrl+C/X/V/Z/Y | copia, taglia, incolla, annulla, ripeti (con una selezione CLIP attiva: copia/taglia la selezione) | |

MATH: **MAT/VCT** (2×2, 3×3, m×n fino a 6×6, 2×1, 3×1, 1×2, 1×3), frazione
mista, `ˣ√□` con indice modificabile, `∛□`, `□²`, `□⁻¹`, `logₐb`, `eˣ`, `10ˣ`,
`|□|`, d/dx, ∫dx, Σ, Π, `( )`, **periodo** (dopo le cifre decimali).

### Cronologia (funzione History della CG50)

Con ↑ si seleziona una riga; i tasti funzione diventano:

| Tasto | Azione |
|---|---|
| EXE / F1 MODIF | modifica la riga **sul posto**; EXE la ricalcola insieme a tutte le righe successive (Ans e memorizzazioni vengono rieseguite); EXIT annulla |
| F2 COPIA | copia l'espressione nella riga di input |
| F3 RIS | inserisce il risultato nella riga di input (forma esatta se esiste) |
| F4 PERIOD | mostra/nasconde il periodo del risultato; per un risultato lista/matrice/vettore **MatAns**/**VctAns**/**ListAns** apre la schermata della risposta |
| ←/→ | su un risultato più largo della riga lo **scorrono** (◀ ▶ indicano le parti nascoste); altrimenti copiano l'espressione nella riga di input |
| F5 S⇔D (SHIFT: ab/c) | cambia la vista della riga selezionata |
| F6 ELIM / DEL | elimina la riga |
| Ctrl+C | copia l'espressione negli appunti |

Se la riga modificata produce un errore, la cronologia e la memoria tornano come
prima e la riga resta in modifica; se falliscono righe successive, conservano il
messaggio d'errore (in rosso). La prima riga vede `Ans = 0`, come sulla CG50.

## Risultati esatti e decimali

Ogni calcolo viene valutato due volte sullo **stesso AST**: in virgola mobile
(sempre) e in forma esatta (quando possibile, `evaluateExact`). La forma esatta,
se esiste, è il valore: il suo valore in virgola mobile sostituisce quello
arrotondato (`1/3×3` è esattamente 1, `10¹⁶+1−10¹⁶` è 1).

Forme esatte (`include/exact.hpp`):
- **razionali** p/q a 64 bit con controllo di overflow;
- **radicali** con al più due termini: `a√b/c ± d√e/f` (√12 = 2√3, √2+√8 = 3√2,
  1/(1+√2) = −1+√2 con il denominatore razionalizzato);
- **multipli razionali di π**: 156π, 2π/3.

Funzioni esatte: + − × ÷, potenze intere e razionali (radici perfette), √, ∛,
ˣ√, sin/cos/tan ai multipli di 30° e 45° (in Deg, Rad con π, Gra), asin/acos/atan
dei valori notevoli, log/log₂/logₐb di potenze esatte, ln 1, exp(0), |x|, Int, Frac,
Intg, arrotondamenti, n!, nCr, nPr, GCD, LCM, MOD, Int÷, Rmdr, relazioni e
operatori logici, Σ e Π con termini esatti. Le funzioni trascendenti, i metodi
numerici (∫, d/dx, d²/dx², SOLVE) e i numeri casuali **non** producono mai una
forma esatta.

### Regole di visualizzazione (come la fx-CG50)

| Caso | Vista automatica | S⇔D |
|---|---|---|
| `1/2`, `3/4`, `1/3` | `1/2`, `3/4`, `1/3` | `0.5`, `0.75`, `0.3333333333` |
| `0.25`, `0.125` (input decimale) | `0.25`, `0.125` | `1/4`, `1/8` |
| frazioni + decimali (`1/2×0.5`) | decimale `0.25` (manuale 2-23 *3) | `1/4` |
| più di 10 caratteri (`1/2578+1/4572`) | decimale `6.066202547E−04` | `3575/5893308` |
| radicali nei limiti (1≤a<100, 1<b<1000, c<100) | `3√2`, `6−4√5`, `√3/2` | decimale |
| radicali fuori limite (`35√2×3`) | decimale `148.492424` | `105√2` |
| π: nπ con \|n\|<10⁶, b/c π con ≤8 cifre e c≤3 cifre | `156π`, `2π/3` | decimale |
| irrazionali/numerici (`sin 1`, `π²`, `∫`) | decimale | nessun cambio (avviso) |

- **Frac Result** (System › "Frac Result" d/c o ab/c, indicata nella barra di
  stato): frazioni improprie o miste (`22/7` ↔ `3 1/7`, `−7/3` ↔ `−2 1/3`);
  SHIFT+F5 cambia il formato del singolo risultato.
- **Forme esatte** (System › "Forme esatte" On/Off): con Off i risultati sono
  decimali, ma S⇔D mostra comunque la forma esatta memorizzata.
- Un decimale digitato è un razionale **esatto** (`0.3333333333` è
  3333333333/10¹⁰, non 1/3): nessuna conversione inventa un valore diverso.
- **Periodo**: OPTN › RISULTATO › ▶Periodo (o F4 nella cronologia) mostra lo
  sviluppo periodico con la linea sopra il periodo: `1/7 = 0.‾142857‾`,
  `1/6 = 0.1‾6‾`; testo lineare `0.1(6)`. Periodi oltre 40 cifre sono troncati con `…`.
- **Input periodico**: MATH › periodo dopo le cifre decimali (`0.1` + `6` = 0.1666… = 1/6).
- **≈** davanti al risultato: valore prodotto da un metodo numerico (∫, d/dx, SOLVE).
- Il cambio di vista **non modifica** né il valore né Ans: cambia solo come il
  risultato è mostrato.
- Arrotondamento di Fix/Sci come la calcolatrice: mantissa decimale a 15 cifre,
  poi arrotondamento per eccesso a metà (`0.125` in Fix2 → `0.13`, `2.675` → `2.68`).

## Variabili e memoria condivisa

- `→` memorizza in A–Z; `→A~F` in un intervallo di variabili.
- Ans, PreAns e A–Z conservano la forma esatta (`1/7→A`, poi `7A` = 1) e
  l'indicazione "input decimale" (`0.5` poi `Ans+1/3` è decimale, S⇔D → 5/6).
- VARS › V-WIN e TABLE inseriscono `Xmin…Tptch`, `TblStart/TblEnd/TblPitch`: sono
  letti dal workspace al momento del calcolo (non copie) tramite `NameResolver`.
- Le variabili sono le stesse usate da Graph, Table, Equation e Conversion.

## Operatori aggiunti nella v1.7.0

| Gruppo | Sintassi | Esempio |
|---|---|---|
| Relazioni | `=`, `≠`, `<`, `>`, `≤`/`<=`, `≥`/`>=` | `3>2` → 1 |
| Logica | `And`, `Or`, `Xor`, `Not` | `3 And 2` → 1 (manuale 2-24) |
| Interi | `Int÷`, `Rmdr` | `107 Int÷ 7` → 15, `107 Rmdr 7` → 2 (manuale 2-25) |
| Numerici | `MOD_Exp(n,p,m)`, `RndFix(x,n)` | `MOD_Exp(5,3,3)` → 2; `RndFix(200/7,3)×14` → 399.994 |
| Casuali | `RanNorm#(σ,μ)`, `RanBin#(n,p)` | |
| Frazioni | `mixed(w,n,d)` (modello a b/c), periodo `0.1_6` | |

Priorità (dalla più bassa): Or/Xor < And < relazioni < + − < × ÷ Int÷ Rmdr <
moltiplicazione implicita < segno < potenza < postfissi < parentesi/funzioni.

## Matrici, vettori e liste (v1.10.0)

Sono **valori di prima classe dello stesso parser** delle espressioni (nessun
parser separato): un'espressione può dare un numero, una lista, una matrice o
un vettore, e gli stessi nodi dell'AST sono usati dal valutatore numerico, da
quello esatto, dall'editor e dal renderer.

| Scrittura | Significato |
|---|---|
| `[[1,2][3,4]]` | matrice 2×2 (formato della fx-CG50: un gruppo `[ ]` per riga); in modalità Math anche come modello MATH › MAT/VCT |
| `{1,2,3}` | lista |
| `Mat A`…`Mat Z`, `Mat Ans` | memorie delle matrici (OPTN › MAT/VCT › Mat; `Mat x` = `Mat X`) |
| `Vct A`…`Vct Z`, `Vct Ans` | memorie dei vettori (matrici con una riga o una colonna) |
| `List 1`…`List 26`, `List Ans` | le liste di Statistics (26 dalla v1.11.0) e la lista risposta |
| `Mat A[2,1]`, `Vct B[3]`, `List 1[4]` | un elemento (in lettura e, dopo `→`, in scrittura) |
| `→Mat A`, `→Vct A`, `→List 1`, `→Mat A[1,2]`, `{2,3}→Dim Mat A` | memorizzazione (Dim crea una matrice di zeri) |

Operazioni (manuale 2-49…2-63, 3-7…3-16):

| Gruppo | Funzioni |
|---|---|
| Aritmetica | `+ −` tra matrici della stessa dimensione, `×` (anche implicito) riga per colonna, numero × matrice, matrice ÷ numero, `^n` intero tra −32766 e 32766 (`^−1` = inversa, `^0` = identità), `□²`; tra liste e numeri elemento per elemento (anche relazioni e funzioni scientifiche: `sin({0,30})`) |
| Matrici | `Det`, `Trn`, `Identity`, `Dim`, `Augment(`, `Fill(`, `Ref`, `Rref`, `Mat→List(`, `List→Mat(`, `Abs`/`Int`/`Frac`/`Intg` e parti complesse elemento per elemento, `Norm(` |
| Vettori | `DotP(`, `CrossP(` (dimensioni 2 e 3), `Angle(` (nell'unità angolare corrente), `UnitV(`, `Norm(` |
| Liste | `Sum`, `Prod`, `Min(`, `Max(`, `Mean(`, `Median(`, `Cuml`, `Percent`, `ΔList`, `SortA(`, `SortD(`, `Seq(espr,var,inizio,fine,passo)`, `Augment(`, `Dim` |

- **Risultati**: una lista va in `List Ans`, una matrice in `Mat Ans` (e anche
  in `Vct Ans` se ha una riga o una colonna, come sulla fx-CG50), un vettore in
  `Vct Ans` e `Mat Ans`; `Ans` resta l'ultimo **numero**.
- **Forme esatte negli elementi**: `[[1,2][3,4]]^−1` = `[[−2,1][3/2,−1/2]]`;
  S⇔D passa insieme tutti gli elementi tra forme esatte e decimali. Le matrici
  memorizzate partecipano alla valutazione esatta con i loro elementi interi.
- **Vista naturale** (modalità Math): matrici e vettori fino a 6×6 disegnati a
  griglia tra parentesi quadre, liste fino a 20 elementi tra graffe; oltre,
  la riga mostra `Mat 7×7 → Mat Ans` e il risultato si apre subito nella
  **schermata MatAns/VctAns/ListAns** (sotto); si riusa con `Mat Ans`.
- **Errori** (tabella α-1 del manuale): `Dimension ERROR` (dimensioni
  incompatibili, memoria non definita, indice fuori intervallo, oltre i limiti
  della scheda), `Type ERROR` (tipo sbagliato: lista con matrice, `sin` di una
  matrice, numero al posto di una matrice…), `Math ERROR` (matrice singolare),
  `Non-Real ERROR` (elementi complessi nel modo Real), `Memory ERROR`
  (memoria di matrici e vettori piena).
- **Limiti per scheda** (`limits.hpp`): lato massimo 40 (Pico 2 W) / 999
  (Pico Plus 2 W); elementi per matrice 400 / 10 000; elementi di tutte le
  memorie insieme 1 200 / 20 000. Liste: 255 / 999 elementi per lista e
  1 530 / 20 000 elementi in tutte le 26 liste insieme (v1.11.0).

### Schermate MatAns / VctAns / ListAns (v1.11.0)

Un risultato più grande della vista naturale (oltre 6×6, o più di 20
elementi per una lista) apre al termine del calcolo la schermata della
risposta, come sulla fx-CG50: griglia scorrevole in entrambe le direzioni
(◀ ▶ quando ci sono colonne nascoste), riga e colonna della cella corrente
(`r4 c6`, `elemento 12`), valore completo in basso (forme esatte e complessi
compresi). È in sola lettura; EXIT (o EXE) torna ai calcoli. Dalla
cronologia, F4 **MatAns/ListAns** sulla riga riapre la schermata (mostra il
contenuto attuale della memoria della risposta, cioè l'ultimo risultato di
quel tipo).

![MatAns](screenshots/54_matans.png)

### Editor MAT/VCT (SHIFT 3)

| Schermata | Tasti |
|---|---|
| Elenco `Mat A`…`Mat Z`, `Mat Ans` con le dimensioni (`2×3`, `None`) | ↑↓ seleziona, una **lettera** salta alla memoria, EXE/→ apre le celle (DIM se non definita), F1 **DELETE**, F2 **DEL-ALL** (con conferma), F3 **DIM**, F4 **CSV** (LOAD, SAVE·AS: v1.11.0), F6 **M⇔V** (elenco dei vettori), EXIT torna ai calcoli |
| DIM | righe `m`, colonne `n` (↑↓ cambia campo, EXE passa al successivo e conferma); vettori solo `m×1` o `1×n`; le nuove dimensioni azzerano il contenuto, come sulla fx-CG50 |
| Celle | il valore digitato va nella cella evidenziata, **EXE passa alla cella a destra** (poi alla riga successiva); dalla v1.11.0 una cella accetta **un'espressione** (`1/3`, `2√3`, `π/4`, `Mat B[1,2]×2`) con la sua **forma esatta** e i **numeri complessi** (`1+2i`, anche nel modo Real); le colonne oltre le 4 visibili scorrono (◀ ▶) e l'intestazione indica la cella (`r1 c6`); F1 **ROW-OP** (SWAP, ×Rw, ×Rw+, Rw+; solo matrici), F2 **ROW** e F3 **COLUMN** (DELETE, INSERT, ADD), F4 EDIT (riapre il testo della cella, forma esatta compresa); DEL azzera la cella; EXIT torna all'elenco |

`Mat Ans` e `Vct Ans` sono in sola lettura. I valori sono quelli del
workspace (`ws.matrices`): li usano i calcoli, restano cambiando applicazione e
vengono salvati in flash con il resto dei dati (record v6; dal v7 anche le
forme esatte).

**Forme esatte nelle memorie (v1.11.0).** Una memoria reale fino a 64
elementi conserva la forma esatta di ogni elemento (celle digitate come
`1/3`, risultati esatti memorizzati con `→Mat A`, `1/7→Mat A[2,2]`): i calcoli
esatti la usano (`[[1/3,1/2][2,1/5]]→Mat A`, poi `Mat A×6` = `[[2,3][12,6/5]]`,
`Det(Mat A)` = `−14/15`). Un elemento complesso o un decimale non intero
inserito con ROW-OP toglie le forme esatte della memoria (restano i valori);
un numero decimale digitato in una cella si conserva come il suo valore
esatto (`0.25` → `1/4`).

**CSV (v1.11.0, manuale 2-48).** F4 **CSV** nell'elenco: **LOAD** carica un
file nella memoria selezionata (righe del file = righe della matrice; un
vettore deve avere una riga o una colonna), **SAVE·AS** salva la memoria
selezionata (anche Mat Ans) con un nome di 8 caratteri (proposto `MATA`,
`VCTB`…). Formato e regole: sezione [File CSV](#file-csv-v1110).

![Celle esatte](screenshots/55_mat_editor_exact.png)

![Elenco delle matrici](screenshots/45_mat_editor_list.png)

## Modalità Math / Line (v1.10.0)

SYSTEM › **Input/Output** (primo campo del SET UP, come sulla fx-CG50),
salvato nel workspace e indicato nella barra di stato (`Math`/`Line`).

| | Math (predefinita) | Line |
|---|---|---|
| Input | modelli 2D: frazioni, radici, potenze, Σ/∫, matrici | una riga di testo: F1 inserisce `/`, F2 `√(`, F3 `^`, i modelli MATH il loro testo (`log(`, `abs(`, `[[,][,]]`…); frazione mista non disponibile (come sulla calcolatrice) |
| Cronologia | espressioni in 2D | espressioni come digitate |
| Risultati | frazioni impilate, radicali, ×10ⁿ, matrici a griglia | su una riga: `1/2`, `2√3/3`, `[[1,2][3,4]]`, `{1,2,3}` |

Cambiando modalità la riga di input in corso viene convertita (testo lineare ⇄
modelli) senza perdere nulla.

### Modalità Line: ⌟, INS, risultati scorrevoli (v1.11.0)

- **Frazioni**: in Line il tasto frazione (F1, e SHIFT F1) scrive `⌟` come la
  fx-CG50: `2⌟3` = 2/3, `1⌟2⌟3` = 1 2/3 (frazione mista). `⌟` lega solo
  interi ed è un unico numero (`1/2⌟3` = 1 ÷ (2/3)); la divisione resta `/`
  (`÷`). I risultati frazionari in Line si scrivono `1⌟3`, `7⌟3` o, con Frac
  Result ab/c, `2⌟1⌟3`; anche negli elementi di matrici e liste.
- **INS** (SHIFT DEL): alterna inserimento e **sovrascrittura**; in
  sovrascrittura il carattere sotto il cursore viene sostituito, il cursore è
  una sottolineatura e la barra di stato mostra `OVR`. Resta attiva finché
  non si preme di nuovo INS (cambiando modalità torna l'inserimento).
- **Risultati più lunghi della riga**: selezionando la riga con ↑, ←/→ la
  scorrono a passi di 48 pixel; ◀ ▶ indicano le parti nascoste (anche in
  Math). Sulle righe corte ←/→ copiano l'espressione come prima.

![Line: ⌟ e INS](screenshots/56_line_ins.png) ![Risultato scorrevole](screenshots/57_long_result.png)

### SHIFT+INS: un valore come argomento (Math, v1.11.0)

Come nel manuale (1-17): con il cursore a sinistra di un valore o di
un'espressione, **SHIFT DEL** (INS) e poi un tasto di modello rendono quel
valore l'argomento del modello: `1+(2+3)+4` con il cursore prima di `(` e
√ diventa `1+√(2+3)+4`. Vale per √, ˣ√, ∛, |x|, logₐb, eˣ, 10ˣ, d/dx, ∫, Σ, Π,
la frazione (il valore diventa il numeratore) e gli altri modelli; il valore
è un numero o un nome, un gruppo tra parentesi (le parentesi esterne
spariscono), una funzione con la sua parentesi (`sin(30)`) o un modello
intero. Il cursore ha la forma di `[` finché il modello non arriva; una
freccia o un altro tasto annullano.

## Simboli ingegneristici e display Eng (v1.11.0)

OPTN › **ESYM** inserisce i simboli della fx-CG50: **m** (10⁻³), **µ** (10⁻⁶),
**n** (10⁻⁹), **p** (10⁻¹²), **f** (10⁻¹⁵), **k** (10³), **M** (10⁶),
**G** (10⁹), **T** (10¹²). Un simbolo moltiplica il valore che precede, con la
priorità dei postfissi (`1/2k` = 1/(2000), `2k²` = 4×10⁶). Nel testo canonico
si scrivono `#k`, `#u` (µ)…, così non si confondono con le variabili `k`, `M`.
Il valore è esatto ma, come un numero con la virgola, si mostra in decimale
(S⇔D dà la frazione: `5m` → `1/200`).

Con **SET UP › Display = Eng** i risultati si mostrano con il simbolo:
`999k+25k` = `1.024M`, `9÷10` = `900m`, `47µ×2` = `94µ`; fuori da f…T resta
la forma ×10ⁿ (`1E−18`). In Eng le frazioni si mostrano in decimale e S⇔D
mostra la forma esatta.

![Eng con i simboli](screenshots/58_eng_symbols.png)

## Simp e Simplify (v1.11.0)

- **▶Simp** (OPTN › NUMERIC) in coda all'espressione semplifica una frazione
  dividendo numeratore e denominatore per il **più piccolo fattore primo
  comune**; **▶Simp n** per il fattore n. Il risultato mostra la frazione e
  il fattore usato: `15/60▶Simp` → `5/20  F=3`, poi `▶Simp` (o `Ans▶Simp`) →
  `1/4  F=5`; `15/60▶Simp 5` → `3/12  F=5`. Il valore (e Ans) non cambia.
- Errori: un fattore che non divide entrambi i termini, un fattore minore di
  2, un risultato che non è una frazione (intero, radicale, decimale non
  esatto) → Math ERROR. Una frazione già irriducibile dà `F=1`.
- **SET UP › Simplify**: *Auto* (predefinito) riduce sempre; *Manual* lascia
  una frazione digitata come è stata scritta (`15/60`, anche col modello
  frazione o `15⌟60`), da ridurre con ▶Simp; i risultati di altre operazioni
  sono comunque ridotti (scelta del progetto, vedi
  [ANALISI_FONTI_v1.11.0](reports/ANALISI_FONTI_v1.11.0.md)).
- S⇔D passa dal decimale alla frazione non ridotta e viceversa; la frazione e
  il fattore sono salvati con la cronologia (record v7).

![Simp](screenshots/59_simp.png)

## Base-N intera (v1.11.0)

**SET UP › Mode**: *Comp* (calcolo normale) oppure **Dec**, **Hex**, **Bin**,
**Oct** — la modalità Base-N della fx-CG50, indicata nella barra di stato.

| Regola | Dettaglio |
|---|---|
| Valori | interi a **32 bit in complemento a due**: Dec da −2147483648 a 2147483647; Hex `0`…`FFFFFFFF`, Bin 32 cifre, Oct `0`…`37777777777` (`FFFFFFFF` = −1) |
| Input | i numeri senza prefisso sono nella base della modalità; in Hex `A`–`F` sono cifre (anche attaccate agli operatori: `FFxor0F`), `G`–`Z` variabili; prefissi **d h b o** (OPTN › BASE-N) per un singolo numero (`d10+h10`) |
| Operatori | `+ − × ÷` interi (la divisione scarta la parte frazionaria: `7÷2` = 3, `−7÷2` = −3), Int÷, Rmdr, relazioni; **And, Or, Xor, Xnor** bit a bit; **Not(** complemento a uno, **Neg(** complemento a due |
| Errori | risultato o numero fuori dai 32 bit → Math ERROR; numeri con la virgola, cifre non valide per la base, simboli ingegneristici → Syntax ERROR; funzioni scientifiche → Math ERROR |
| Risultati | nella base della modalità (`FF+1` = `100`, `0−1` = `FFFFFFFF`); ▶Dec ▶Hex ▶Bin ▶Oct (OPTN › BASE-N o RISULTATO) mostrano la riga in un'altra base |
| Cronologia | ogni riga ricorda la base in cui è stata calcolata (record v7): tornando a Comp resta come era; il ricalcolo delle righe successive usa la loro base, la riga modificata quella corrente |

Nel calcolo normale `And`, `Or`, `Xor`, `Xnor` restano operatori logici
(risultato 1/0) e `Neg(x)` è −x.

![Base-N](screenshots/60_base_n.png)

## Comandi grafici (v1.11.0)

OPTN › **SKETCH** inserisce i comandi della fx-CG50 che, eseguiti con EXE,
**disegnano nella schermata di GRAPH** (che compare al termine; EXIT o G⇔T
tornano a Run-Matrix e la riga mostra `Done`):

| Comando | Effetto |
|---|---|
| `Cls` | cancella gli elementi SKETCH e i grafici dei comandi |
| `ViewWindow Xmin,Xmax,Xscl,Ymin,Ymax,Yscl[,Tmin,Tmax,Tptch]` | imposta la V-Window di GRAPH (con i suoi controlli) |
| `Graph Y=f(x)`, `Graph r=f(θ)`, `Graph X=f(y)`, `Graph(X,Y)=(xt,yt)` | un grafico in più, senza occupare Y1–Y20 (al massimo 10) |
| `Plot x,y`, `PlotOn x,y`, `PlotOff x,y`, `PlotChg x,y` | punto acceso, spento o invertito |
| `Line` | unisce gli ultimi due punti di Plot |
| `F-Line x1,y1,x2,y2`, `Circle x,y,r`, `Vertical x`, `Horizontal y` | segmento, circonferenza, rette |
| `Text riga,colonna,"testo"` | testo nel pixel indicato (riga 1–282, colonna 1–320) |
| `PxlOn riga,colonna`, `PxlOff …`, `PxlChg …` | un pixel della schermata |
| `Tangent f(x),x`, `Normal f(x),x`, `Inverse f(x)` | tangente, normale, grafico inverso |

Gli argomenti sono espressioni dello stesso parser (variabili comprese:
`Plot A,A×2`). Un comando è **tutto o niente**: errori di sintassi, di
calcolo o di limiti (64 elementi SKETCH, 10 grafici) non cambiano nulla e il
cursore va sulla causa. Il ricalcolo della cronologia non riesegue i comandi
delle righe successive (disegnerebbero di nuovo); una riga di comando
modificata si esegue. Gli elementi e i grafici sono dati di GRAPH nel
workspace (salvati nel record v7) e si vedono anche aprendo GRAPH dal menu.

![Comandi grafici](screenshots/61_graph_commands.png)

## File CSV (v1.11.0)

Liste (editor di Statistics, OPTN › CSV LOAD / CSV SAVE·AS) e matrici
(editor MAT/VCT, F4 CSV) si salvano e si caricano come file CSV nella
cartella `PICOCALC/CSV` dell'archivio scelto in SYSTEM (**microSD** o **flash
interna** LittleFS), con nomi di 8 caratteri al massimo.

- Formato: **separatore** `,` o `;` e **simbolo decimale** `.` o `,` (SET UP
  › CSV Separator / CSV Decimal, oppure CSV SET nell'editor delle liste;
  `,` + `,` è rifiutato); fine riga CR, LF o CRLF; una prima riga con
  virgolette è un'intestazione e si ignora; il tabulatore non è un separatore.
- Liste: una colonna per lista (SAVE·AS scrive List 1 … l'ultima non vuota;
  le liste più corte lasciano celle vuote; i sottonomi non si scrivono, come
  sulla calcolatrice); LOAD scrive le colonne del file a partire dalla lista
  del cursore.
- Matrici: righe del file = righe della matrice, nessuna cella vuota; solo
  numeri reali (`Complex Number In Data` per un elemento complesso).
- **Scrittura atomica** (`.tmp`, verifica CRC, `.bak`, rinomina): dopo
  un'interruzione resta il file vecchio o quello nuovo; il caricamento
  completa o annulla un salvataggio interrotto.
- Errori senza modifiche ai dati: file mancante (`Storage ERROR: file non
  trovato`), testo non numerico o celle vuote nel mezzo (`Format ERROR`),
  righe/colonne oltre i limiti o liste oltre il budget della scheda
  (`Dimension ERROR`, `Memory ERROR`), scheda assente (`Storage ERROR`).

## Selezione parziale CLIP (v1.10.0)

Come sulla fx-CG50 (manuale 1-11): **SHIFT 8** inizia la selezione nel punto
del cursore, **←/→** la estendono (evidenziata), **F1 COPY** copia, **F2 CUT**
taglia, **DEL** elimina, **EXIT** annulla; **SHIFT 9** (o Ctrl+V) incolla.

- In modalità Math la selezione resta nel campo in cui si trova il cursore
  (numeratore, cella, esponente…); spostandosi oltre il bordo del campo
  seleziona l'**intero modello**.
- Copia e taglia **non spezzano mai la struttura**: una selezione con una
  parentesi (tonda, quadra o graffa) senza la sua corrispondente si estende
  fino a includerla, e una parentesi di funzione porta con sé il nome
  (`(30)` dentro `sin(30)` copia `sin(30)`).
- Gli appunti sono condivisi da tutte le modalità; incollando in modalità Line
  i modelli diventano testo lineare.

## Parentesi colorate (v1.10.0)

Durante l'input le parentesi sono colorate per livello, dall'esterno verso
l'interno: **blu, rosso, verde, magenta, nero**, poi di nuovo dal blu
(manuale 1-8). La parentesi di chiusura ha il colore di quella di apertura,
parentesi dello stesso livello hanno lo stesso colore, il gruppo `( )` del
menu MATH segue le stesse regole; dopo l'esecuzione (cronologia) tornano nere.
I colori sono più scuri di quelli puri per restare leggibili su ogni sfondo
dell'input (bianco, azzurro della riga in modifica, evidenziazioni): il test
`editor_v110_test` verifica un contrasto di almeno 4,5:1 (WCAG AA).

## Cursore sulla causa di un errore (v1.10.0)

Il parser registra la posizione di ogni token e di ogni sotto-espressione;
per un errore di sintassi indica il token che non ha potuto usare, per un
errore di calcolo la sotto-espressione che non è riuscita (`14÷0` in
`14÷0×2.3`, `Det([[1,2]])` in `1+Det([[1,2]])`). Chiusa la finestra
d'errore, **il cursore è alla fine della causa** (come nell'esempio 1-10 del
manuale: ← e `1` correggono `14÷0` in `14÷10`) e la causa è **evidenziata**
fino al tasto successivo; dentro i modelli il cursore va nel campo giusto
(numeratore, cella, esponente). Vale anche per la modifica di una riga della
cronologia e per gli errori dopo la freccia (segnalati sulla freccia).

![Finestra d'errore](screenshots/49_error_box.png)

## Implementazione

| File | Contenuto |
|---|---|
| `include/run_matrix.hpp`, `src/run_matrix.cpp` | `evaluateEntry` (anche destinazioni Mat/Vct/List ed elementi), `runCalculation`, `editHistoryEntry`, `recalculateHistory`, viste (`toggleExactDecimal`, `toggleMixedFraction`, `togglePeriodic`), `resultText`, `resultTree`, `resultExpression`, `elementText` |
| `src/apps/run_matrix_app.cpp` | interfaccia: tasti, menu, cronologia, modifica sul posto, CLIP, Math/Line, disegno |
| `src/apps/matrix_editor.cpp` | editor MAT/VCT (`MatrixEditorView`) |
| `include/value.hpp`, `src/value.cpp` | `CalcValue`: numero, lista, matrice o vettore |
| `include/exact.hpp`, `src/exact.cpp` | aritmetica esatta |
| `src/expression.cpp` | lexer, parser e valutatori (virgola mobile ed esatto; `CollEval` per liste, matrici e vettori) |
| `src/natural_editor.cpp`, `src/math_renderer.cpp` | modelli MAT/VCT, Math/Line, CLIP, posizione degli errori, INS e SHIFT+INS, ⌟; griglia delle matrici, parentesi colorate, evidenziazioni, forme del cursore |
| `src/apps/answer_view.cpp` | schermate MatAns / VctAns / ListAns (v1.11.0) |
| `include/graph_commands.hpp`, `src/graph_commands.cpp` | interprete dei comandi grafici (v1.11.0): argomenti valutati dal parser unico, risultato nei dati di GRAPH |
| `include/csv_io.hpp`, `src/csv_io.cpp` | file CSV di liste e matrici (v1.11.0) |

## Limiti noti

- Liste: i nomi delle liste (sottonomi) si modificano e si vedono
  nell'editor di Statistics ma non si usano nei calcoli come riferimento
  (`List "QTY"` non è realizzato); i file di lista multipli (List File 1–6
  della fx-CG50) non sono realizzati: c'è un solo insieme di 26 liste.
- Editor MAT/VCT: le forme esatte si conservano per memorie reali fino a 64
  elementi; un decimale digitato in una cella si mostra come frazione esatta
  (`0.25` → `1/4`).
- Simplify Manual: solo una frazione digitata resta non ridotta; le altre
  operazioni danno frazioni ridotte (la regola esatta della calcolatrice per
  le operazioni tra frazioni non è stata verificata).
- Base-N: le variabili A–F non sono raggiungibili in Hex (le lettere sono
  cifre); un numero esadecimale non può iniziare con `0d`/`0b`/`0o`/`0x`
  (sono i prefissi). Liste e matrici in Base-N seguono le stesse regole
  intere ma non hanno una vista dedicata.
- Comandi grafici: non realizzati `PxlTest(`, `DrawStat`, `DrawDyna`, `DrawR`,
  `DrawFTG`, `G-SelOn/Off`, `StoPict`/`RclPict`, `BG-Pict`, gli stili
  (`SketchThick`…), `Graph Y>` e le altre disequazioni, `G∫dx`; i grafici dei
  comandi non vanno nei progetti `.pcgraph`.
- Simboli ingegneristici: `P` ed `E` (10¹⁵, 10¹⁸) e le conversioni ►ENG/►ENG←
  non sono realizzati.
- Nessuna prova su hardware PicoCalc (tasti SHIFT DEL, colori, tempi dei CSV
  sulla microSD reale: [HARDWARE_TEST_v1.11.0.md](HARDWARE_TEST_v1.11.0.md)).
