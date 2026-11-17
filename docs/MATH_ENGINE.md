# Motore matematico — v1.13.0

Riferimenti storici: `archive/VPAM_SHARED_AST.md`, `archive/RUN_MATRIX_PHASE*.md` e
`archive/math_engine_original.py` descrivono le versioni precedenti (vedi [README.md](README.md)).

## Pipeline unica

```
editor Natural (ast::Node Row/Fraction/Radical/Power/Calculus/Call/Matrix)
   │ ast::serialize            testo lineare canonico, es. "(1)/(3)+sqrt(2)", "[[1,2][3,4]]"
   ▼
Lexer → Parser                 ast::Node Binary/Unary/Call/Group/Token/Matrix/List
   ▼                           (ogni nodo con pos/len nel testo; FlagCollection)
evalNode<double>               modo Real
evalNode<std::complex<double>> modi a+bi e r∠θ
exactNode → exact::Value       forma esatta (Run-Matrix), sullo stesso AST
CollEval<NumDomain|ExactDomain> nodi con liste, matrici, vettori (v1.10.0)
evaluateAny → CalcValue        numero, lista, matrice o vettore
```

Dalla v1.11.0 lo stesso lexer riconosce anche i simboli ingegneristici
(`#k`…), le frazioni lineari `a⌟b` e, con `parseExpression(testo, base)`, i
numeri della modalità Base-N; lo stesso valutatore applica la semantica
intera a 32 bit quando `EvalContext::integerBase` è impostato.

Esiste **un solo** parser e **un solo** valutatore numerico (templato sul tipo):
Run-Matrix, Graph, Table, Equation, Statistics e i test usano lo stesso codice
(`include/expression.hpp`). `CompiledExpression` analizza una volta e valuta molte
volte (grafici, tabelle, solver).

## Costanti (v1.14.0, in sviluppo)

Le costanti con nome non sono più in `kConstants[]` di `expression.cpp`, ma
nella tabella unica `kTable` di `src/physical_constants.cpp` (73 costanti,
[PHYSICAL_CONSTANTS.md](PHYSICAL_CONSTANTS.md)):

- `isConstantName()` e la valutazione usano `physconst::findConstant()` per i
  nomi di almeno due lettere; `e` resta il caso a parte di una lettera (il
  numero di Nepero, mentre `E` è la variabile), con il valore della tabella;
- i nomi della v1.13 valgono come prima; i nomi nuovi (`phys` + sigla) non
  allungano un nome riservato di cifre o di una lettera, quindi letture come
  `physc2` = c·2 o `kbT` = k_B·T non cambiano; cambia solo una sequenza di
  lettere che ora è un nome intero (`physme`, prima p·h·y·s·m·e). Regole e
  confronto differenziale nel documento;
- `isNonConstantName()` (nuova, `expression.hpp`) riconosce i nomi del
  parser che non sono costanti; il test verifica che nessuna costante lo sia;
- `pi` e `tau` restano esatti (multipli di π) nella valutazione esatta; le
  altre costanti sono decimali come prima;
- glifi: `calclib::displayName()` restituisce il glifo della tabella, quindi
  l'editor e la cronologia mostrano `mₑ×c²` invece di `physme*physc^2`;
  `formatNatural` usa la stessa tabella senza distinguere le maiuscole.

## Correzioni della v1.13.1

Stesso parser, stesso AST, stesso valutatore; nessuna funzione nuova
([reports/ERRORI_v1.13.1.md](reports/ERRORI_v1.13.1.md)):

- `nCr`/`nPr`: n < 1E10, risultato < 1E100 (prima n ≤ 100000);
- `Σ`/`Π`: i termini (al più 100 001) si contano in double, la variabile vale
  `a + i·passo`; gli estremi non si convertono più in `long long`;
- `Ran# n`: n fuori da 0…10 rifiutato prima della conversione in `int`;
- `calc_library`: inverse discrete con la somma cumulativa, HypergeoCD sul
  supporto (al più 10⁶ termini), distribuzione geometrica con `log1p`/`expm1`,
  InvPoissonCD oltre 10⁶ come Argument ERROR, `logChoose` diretto con
  min(k, n−k) ≤ 64, InvTCD e i quantili t con la simmetria;
- EQUATION: forma esatta complessa per sostituzione solo per i polinomi in `i`.

## Novità della v1.13.0 (Run-Matrix)

Tutto resta nel **parser, AST e valutatore unici**; il nuovo modulo
`calc_library` (puro calcolo, senza workspace né parser) fornisce unità,
distribuzioni, funzioni TVM, nomi mostrati e CATALOG.

| Area | Testo canonico | Valutazione |
|---|---|---|
| CALC | `solve(f,s[,a,b])` anche `a=b`, `solve_n(…)`, `f_min(…)`, `f_max(…)`, `sum(f,k,a,b[,n])`, `prod(…)`, `integral(f,a,b[,tol])`, `derivative2(…)` | Solve in `callCalculus`; SolveN, FMin, FMax in `CollEval` (risultato lista); funzioni vietate negli argomenti con `Env::solving` |
| PROB | `ran 3`, `ran_list(n)`, `ran_samp(l,n[,m])`, `randint/rannorm/ranbin(…,n)`, `prob_p`…`prob_t` | sequenze di Ran# con semi fissi; liste in `CollEval` |
| NUMERIC, ANGLE | `num_rnd(x)`, `30ʳ`, `50ᵍ`, `2°20′30″`, `pol/rec` (lista) | Rnd con `EvalContext::displayMode/displayDigits` |
| COMPLEX | `r∠θ` (operatore tra × e +) | complesso; Non-Real ERROR nel modo Real |
| STAT | `x_hat`, `y_hat`, `norm_pd`…`invhypergeo_cd`, `s_dev`, `stat_var`, `mean(l,f)`, `median(l,f)` | regressione e dati da `NameResolver` (`reg_*`, `stat_*`) |
| TVM | `smpl_si`, `cmpd_pv`…, `cash_npv(I%,lista)`…, `amt_bal`… (fino a 7 argomenti), `days_prd` | `calclib`; `MaxCallArguments` = 8 |
| CONVERT | `10[cm]▶[in]` | token unità `U`/`V` del lexer, nodo `convert`, anche su liste |
| ESYM | `#P`, `#E` | fattori 10¹⁵ e 10¹⁸; display Eng con P ed E |
| Overflow | | `checkFinite`: \|x\| ≥ 10¹⁰⁰ è Math ERROR a ogni passaggio |
| Variabili | `v_a`…`v_z`, `r_var`, `theta`, `f_1`…`f_20`, `str_1`…`str_20` | memoria (A–Z, r, θ), `ServiceResolver::functionMemory` |
| VARS | `xfct`, `stat_n`…, `reg_a`…, `d_start`…, `r_start`, `rec_a0`…, `tvm_n`…, `SimRes`…, `tbl_result` | `NameResolver` / `CollectionResolver::special` |
| Liste | `List "QTY"` | `CollectionResolver::listByName` |
| Stringhe | `"testo"` | token stringa: argomenti di InvNormCD e List, Str, risultati stringa |
| Simplify Manual | | `evaluateUnreducedFraction`: dominio delle frazioni non ridotte sullo stesso AST (m.c.m. per + −, prodotti per × ÷, potenze intere) |
| Base-N | | Bin a 16 bit (`baseBits`), Dec/Oct/Hex a 32 |
| PxlTest( | `pxl_test(r,c)` | `ServiceResolver::pixelTest` (GRAPH tramite l'AppManager) |

Il testo canonico usa nomi con `_` per i comandi nuovi: un solo token, che non
si forma digitando lettere; editor e renderer li mostrano con il nome della
calcolatrice (`calclib::displayName`). Le parentesi implicite di una frazione
accostata a un operando sono scritte nel testo canonico (`2((1)/(3))`).

## Novità della v1.12.0 (EQUATION)

Il motore di EQUATION (`include/equation.hpp`, `src/equation.cpp`, nessuna
interfaccia) usa **lo stesso parser e lo stesso valutatore**: ogni
coefficiente si legge con `parseExpression` e si valuta con `evaluateAny` e
`evaluateExact`. Novità:

- **Numeri complessi esatti** (`eq::ExactComplex`): parte reale e immaginaria
  `exact::Value`, con somma, differenza, prodotto e quoziente esatti (o
  `false`). La forma esatta di un coefficiente complesso si ottiene con
  l'unico valutatore esatto: `i` è sostituita da un intero t = 0…4,
  `evaluateExact` dà f(t), le differenze finite danno il polinomio in t
  (grado ≤ 3, verificato dalla quarta differenza) e i² = −1 lo riduce a
  A + Bi; il risultato è accettato solo se coincide con il valore complesso.
  Così `(1+√3 i)/2`, `(1+i)²` sono esatti; `1/(1+i)` resta decimale.
- **SIML**: Gauss-Jordan esatto sulla matrice completa n×(n+1) (Rref, anche
  per `SimRes`), con l'indicazione per elemento dei valori che dipendono da
  decimali digitati; in virgola mobile con equilibratura delle righe e
  pivoting parziale (soglia relativa per colonna). Rank e pivot danno
  Unique / No Solution / Infinitely Many Solutions con la formula
  parametrica.
- **POLY**: con coefficienti razionali scomposizione senza quadrati di Yun
  su `exact::Rational` (molteplicità esatte), radici razionali (teorema delle
  radici razionali, con limiti sui divisori), fattori di grado 1–2 con la
  formula esatta (radicali e complessi), biquadratiche e quartiche pari
  scomponibili in due quadratiche; gli altri fattori e i coefficienti non
  razionali con Aberth-Ehrlich (anche complesso), rifinitura di Newton,
  coppie coniugate e raggruppamento delle radici multiple confermato dalle
  derivate. Ordine della fx-CG50; Underflow e Ma ERROR fuori da 1E-99…1E+100.
- **SOLVER** (`eq::solveFor`): qualsiasi variabile A–Z (copia delle memorie
  sull'heap), Newton con differenze centrali dal valore attuale, poi ricerca
  di un cambio di segno entro Lower/Upper e bisezione; Retry, Can't Solve!,
  Time Out (anche oltre `limits::kSolverEvaluations` valutazioni), No Variable.
- **Visualizzazione** (`eq::numberText`, `eq::numberTree`): gli stessi alberi
  2D di Run-Matrix (`exactValueTree`, `appendNumberTree`, ora pubblici in
  `run_matrix.hpp`), complessi su denominatore comune (`(−3+√11 i)/2`),
  forma polare esatta (modulo radicale, argomento multiplo di π/12 o in gradi
  e gradianti riconosciuto dalla tangente esatta).
- **Lexer**: `SimRes`, `SimCoef`, `PlyRes`, `PlyCoef` sono nomi di matrice
  (`CollectionResolver::Memory::Equation`), risolti dal workspace.
- **Testo lineare dell'editor**: un modello di frazione seguito da un fattore
  implicito è scritto `((a)/(b))X` (nel testo lineare la moltiplicazione
  implicita precede la divisione, come sulla fx-CG50); l'operando di x² e ^
  è il numero o la sola variabile prima del cursore.

## Novità della v1.11.0

### Simboli ingegneristici

| Testo canonico | Mostrato | Fattore |
|---|---|---|
| `#k` `#M` `#G` `#T` | k M G T | 10³ 10⁶ 10⁹ 10¹² |
| `#m` `#u` `#n` `#p` `#f` | m µ n p f | 10⁻³ 10⁻⁶ 10⁻⁹ 10⁻¹² 10⁻¹⁵ |

Postfissi (priorità di `!` e `°`): `2#k` è il prodotto per il token `#1e3`.
Il valore esatto è razionale ma il token marca l'input come decimale, così il
risultato si mostra in decimale. `formatNumber` in Eng restituisce il simbolo
in `FormattedNumber::symbol` per gli esponenti −15…12 (`1.024M`, `900m`,
`4.7µ`); fuori resta ×10ⁿ.

### Frazioni lineari ⌟

`a⌟b` e `a⌟b⌟c` (U+231F) sono **un solo token numerico**, solo con interi:
a/b e a + b/c (con il segno del modello misto: `-1⌟2⌟3` = −(1 + 2/3)).
Esatte nel valutatore esatto. Errori: `⌟` senza intero dopo, più di due `⌟`,
termini decimali (Syntax ERROR); denominatore nullo (Math ERROR).

### Base-N intera

`parseExpression(testo, base)` con base 2, 8, 10, 16 e `EvalContext::integerBase`:

- numeri senza prefisso nella base data; prefissi `0d`, `0x`, `0b`, `0o` per
  un singolo numero; in base 16 le cifre A–F anche dentro le parole
  (`FFxor0F` = FF Xor 0F, `AandF`), variabili G–Z;
- interi a 32 bit in complemento a due: in Hex/Bin/Oct `FFFFFFFF` = −1,
  in Dec al massimo 2147483647; oltre → Math ERROR;
- `combine()` sostituisce `applyBinary()` in tutti i punti del valutatore
  (nodi, catene, elementi di `CollEval`): `+ − ×` con controllo di
  traboccamento, `÷` e `Int÷` troncano verso zero, `Rmdr`, relazioni,
  `And`/`Or`/`Xor`/`Xnor` bit a bit; segno meno con controllo; funzioni solo
  `Not` (complemento a uno), `Neg` (complemento a due), `Abs`;
- numeri con la virgola, simboli ingegneristici: Syntax ERROR; valori non
  interi (costanti, variabili): Math ERROR; nessuna forma esatta.

Nel calcolo normale `Xnor` è l'operatore logico (1 se i due valori sono
entrambi nulli o entrambi non nulli) e `Neg(x)` è −x.

### Forme esatte delle memorie

`CalcValue::exact` conserva una forma esatta per elemento (memorie reali fino
a `CalcValue::ExactElements` = 64): `ExactDomain::load` la usa al posto dei
soli interi, quindi `Mat A×6` con `Mat A` = `[[1/3,1/2][2,1/5]]` è esatto.

## Valutazione esatta (v1.7.0)

`evaluateExact(ast, ctx, out, &decimalInput)` percorre **lo stesso AST** con
`exact::Value` (`include/exact.hpp`): razionali a 64 bit con controllo di
overflow, radicali con al più due termini `a√b/c ± d√e/f` (radicando senza
quadrati, ≤ 10⁶) e multipli razionali di π. Ogni passo restituisce il valore
esatto oppure "nessuna forma esatta": il modulo **non approssima mai**.

| Supportato | Non esatto (mai) |
|---|---|
| + − × ÷, potenze intere, potenze razionali con radici perfette, √ (√12 = 2√3), ∛, ˣ√, denominatori razionalizzati (1/(1+√2) = √2−1) | funzioni trascendenti in punti generici (sin 1, ln 2, e) |
| sin/cos/tan ai multipli di 30° e 45° (Deg, Rad come multipli di π, Gra); asin/acos/atan dei valori notevoli | ∫, d/dx, d²/dx², SOLVE (metodi numerici) |
| log, log₂, logₐb di potenze esatte; ln 1; exp(0) | numeri casuali |
| \|x\|, Int, Intg, Frac, Round/RndFix, sign, n! (≤ 20), nCr, nPr, GCD, LCM, MOD, Int÷, Rmdr | variabili che contengono valori approssimati |
| relazioni e operatori logici; Σ e Π (≤ 10 000 termini) con termini esatti; letterali decimali e periodici | numeri complessi non reali |

Memoria: Ans, PreAns e A–Z conservano la forma esatta (`ExactSlot`) e se il
valore viene da un input decimale. Run-Matrix usa la forma esatta come valore
(il suo valore in virgola mobile sostituisce quello arrotondato), con le regole
di visualizzazione descritte in [RUN_MATRIX.md](RUN_MATRIX.md). Se l'esatto
fallisce (anche per budget di stack) resta il valore numerico: nessun errore in
più rispetto alla valutazione numerica.

## Sintassi e priorità (come nel manuale fx-CG50)

0. (più bassa) `Or`, `Xor` < `And` < relazioni `= ≠ < > ≤ ≥` (risultato 1/0);
1. parentesi, funzioni con parentesi; le parentesi finali si possono omettere;
2. postfissi: `!`, `°` (gradi);
3. potenza `^`, associativa a destra, esponente con segno (`2^-2`);
4. segno unario (`-2^2 = -4`);
5. **moltiplicazione implicita** (`2x`, `2(3)`, `2pi`, `x sin x`), più forte di `*` e `/`
   (`6/2(1+2) = 1`, `1/2x = 1/(2x)`);
6. funzioni prefisse senza parentesi prendono il prodotto implicito che segue
   (`sin 2x = sin(2x)`), fermandosi alla funzione successiva (`sin x cos x`);
7. `*`, `/`, `%` (modulo), `Int÷` (quoziente intero), `Rmdr` (resto);
8. `+`, `-`;
9. relazioni, poi `And`, poi `Or`/`Xor` (vedi punto 0).

Le lettere digitate vengono unite in parole note (`sinx`, `2pix`); le variabili sono
le lettere A–Z, `Ans`, `PreAns`, `θ` e le variabili legate dalle app (x, y, …).
Letterali `0x`, `0b`, `0o` per Base-N; decimali periodici `0.1_6` = 0.1666…
(nell'editor: modello "periodo" con la linea sopra le cifre). Nomi condivisi
`xmin … tptch`, `xdot`, `tblstart/tblend/tblpitch` risolti dal workspace tramite
`EvalContext::names` (`NameResolver`): fuori da Run-Matrix danno Math ERROR.
In GRAPH e TABLE anche `list1`…`list6` (grafici da lista).

### Riferimenti tra funzioni di GRAPH (v1.8.0)

Le funzioni composte (`Y2 = Y1(x)+1`, `Y3 = Y1(Y2(x))`) usano token atomici
`@Y1`, `@R1`, `@XT1`, `@YT1`, `@X1` (inseriti da VARS GRAPH; nell'editor e nei
testi appaiono come `Y1`, `r1`, `Xt1`, `Yt1`, `X1`). Il lexer li riconosce, il
parser li tratta come chiamate con al più 3 argomenti e il valutatore li
risolve tramite `EvalContext::functions` (`FunctionResolver`, implementato da
`graph::GraphEngine`): il primo argomento posizionale lega la variabile della
funzione (x, θ, T o y), gli argomenti `A=valore` legano temporaneamente le
lettere A–Z. Profondità massima 5, contando anche i riferimenti dentro gli
argomenti (`MaxFunctionReferenceDepth`): oltre (o con un ciclo) "Stack ERROR".
Il valutatore esatto non li gestisce (risultato solo numerico).

`rewriteIdentifiers(testo, mappa)` riscrive i nomi di un'espressione
ri-emettendo i token (usato da CONVERT per scambiare x ↔ y e da Modify COPY
per sostituire i coefficienti con i loro valori).

## Funzioni

| Gruppo | Nomi |
|---|---|
| Trigonometriche | sin cos tan asin acos atan (Deg/Rad/Gra, valori esatti ai multipli di 90°/100 gon) |
| Iperboliche | sinh cosh tanh asinh acosh atanh |
| Esponenziali e log | exp ln log (log(x) e log(a,b) = logₐb come la CG50) log2 log10 |
| Radici e potenze | sqrt cbrt root(n,x) rootN(x) (N=2–99) pow recip, potenze razionali di negativi (`(-8)^(1/3) = -2`) |
| Arrotondamenti | abs floor ceil round(x[,n]) trunc int intg frac sign |
| Interi | fact / `!` ncr npr gcd lcm mod |
| Varie | min max hypot atan2 percent deg rad grad dms(d,m,s) pol(x,y) rec(r,θ) |
| Casuali | rand() ran randint(a,b) |
| Complessi | re im arg conj real imag, unità `i` |
| Frazioni | `mixed(w,n,d)` (modello a b/c: −2 1/3 = −(2+1/3)), `frac(a,b)` |
| Logica e confronti | `not`, operatori `and or xor xnor`, `= ≠ < > <= >=` (bit a bit in Base-N; `neg(x)` = −x, complemento a due in Base-N) |
| Interi e arrotondamenti (v1.7.0) | `modexp(n,p,m)` (MOD_Exp), `rndfix(x,n)` (RndFix), operatori `Int÷`, `Rmdr` |
| Casuali (v1.7.0) | `rannorm(σ,μ)` (RanNorm#), `ranbin(n,p)` (RanBin#) |
| Calcolo (valutazione pigra) | integral(f,a,b) derivative(f,x₀) derivative2(f,x₀) sum(f,a,b) prod(f,a,b) solve(f,x₀) |
| Costanti | tabella unica `physical_constants.hpp` (v1.14.0): matematiche `pi e tau phi eulergamma sqrt2 ln2 ln10`; fisiche CODATA 2022 / SI 2019 e astronomiche IAU, es. `physc physh physhbar physme physmp na kb qe physrgas g0 physau physpc` e alias come `electronmass`; elenco completo in [PHYSICAL_CONSTANTS.md](PHYSICAL_CONSTANTS.md) |

## Metodi numerici

| Funzione | Metodo | Note |
|---|---|---|
| ∫ | Gauss–Kronrod G7/K15 adattivo | tolleranza 1e-10, profondità 18, max 20 000 valutazioni; anche per funzioni C++ (`integrateFunction`, usato da G-Solve ∫dx) |
| d/dx, d²/dx² | estrapolazione di Ridders sulle differenze centrali | d²: Ridders annidato; `differentiateFunction` per TRACE, Tangent/Norm e MAX/MIN di GRAPH |
| Radici, estremi, intersezioni di GRAPH | Brent–Dekker e minimizzazione di Brent | in `graph_solve.cpp`, vedi [GRAPH_ENGINE.md](GRAPH_ENGINE.md#g-solve) |
| SOLVE | Newton con derivata di Ridders | 100 iterazioni, controllo di convergenza |
| Σ, Π | somma/prodotto su indici interi | max 100 001 termini |
| Polinomi (Equation) | Yun + radici razionali + formula quadratica esatta; Aberth–Ehrlich | radici esatte e complesse con molteplicità, grado 2–6 (v1.12.0) |
| Sistemi lineari | Gauss-Jordan esatto; in virgola mobile con equilibratura e pivot parziale | 2–6 incognite, Rref, formula delle infinite soluzioni (v1.12.0) |
| Disequazioni | radici reali + segno tra le radici | grado 1–6, unione di intervalli |
| Solver di EQUATION | Newton con differenze centrali, cambio di segno e bisezione entro Lower/Upper | qualsiasi variabile, Retry, al più 4 000 valutazioni (v1.12.0) |
| Distribuzioni | beta e gamma regolarizzate | normale, binomiale (log-gamma), Poisson, t, χ², F |

## Formati di visualizzazione

`number_format.hpp`: Norm1/Norm2 (10 cifre significative, soglie esponenziali
come la CG50), Fix 0–9 decimali, Sci 1–10 cifre, Eng; Base-N Hex/Bin/Oct;
sessagesimale; complessi in forma `a+bi` o `r∠θ`. Gli zeri con segno vengono
normalizzati (`√-4 = 2i`, non `-2i`).

Arrotondamento (v1.7.0): il valore viene prima ridotto alla mantissa decimale a
15 cifre, poi arrotondato per eccesso a metà alle cifre mostrate, come la
calcolatrice (`0.125` in Fix2 → `0.13`, `2.675` → `2.68`; prima `printf`
arrotondava il valore binario a pari: `0.12`, `2.67`).

Frazioni, radicali e π per S⇔D vengono dalla **valutazione esatta** (sopra),
non più dalla ricerca di una frazione vicina al valore in virgola mobile.
`exactFraction()` (frazioni continue) resta solo come API di compatibilità.

## Matrici, vettori e liste (v1.10.0)

Grammatica aggiunta (stesse priorità di prima; una matrice o una lista è un
operando come un numero):

```
atomo   := … | matrice | lista | memoria [ "[" espr ("," espr)? "]" ]
matrice := "[" ( "[" espr ("," espr)* "]" )+ "]"      righe della stessa lunghezza
lista   := "{" espr ("," espr)* "}"
memoria := "Mat" (A–Z | Ans) | "Vct" (A–Z | Ans) | "List" (1–26 | Ans)
postfisso := … | "#" (k | M | G | T | m | u | n | p | f)       (v1.11.0)
numero  := … | intero "⌟" intero [ "⌟" intero ]                 (v1.11.0)
```

- `Mat`, `Vct`, `List` sono riconosciuti solo con questa grafia (come li
  inserisce OPTN): `mat` digitato resta il prodotto m·a·t. Il nome può essere
  attaccato (`MatA`, `List1`). `List 1`–`List 26` sono il token
  `list1`…`list26` usato anche da GRAPH (dove indica un elemento per grafico)
  e da TABLE.
- `[` dopo un riferimento è un indice (`Mat A[1,2]`), `[[` inizia una matrice
  (prodotto implicito).
- Il parser marca con `ast::FlagCollection` ogni nodo il cui sotto-albero
  contiene collezioni; solo questi nodi passano a `CollEval`. Le funzioni
  `Det`, `Trn`, `Identity`, `Dim`, `Augment`, `Fill`, `Ref`, `Rref`,
  `mattolist`, `listtomat`, `DotP`, `CrossP`, `Angle`, `UnitV`, `Norm`, `Seq`,
  `Mean`, `Median`, `Cuml`, `dlist`, `SortA`, `SortD` e `Sum`/`Prod`/`Min`/`Max`
  con un argomento sono funzioni di collezione.
- `CollEval<D>` è scritto una volta per i due domini: `NumDomain` (reale nel
  modo Real, complesso negli altri; gli elementi numerici passano a `evalNode`)
  e `ExactDomain` (tutto o niente: un passo senza forma esatta abbandona la
  valutazione esatta). Inversa e Det con eliminazione di Gauss con pivot
  parziale (un pivot è nullo se |p| ≤ 10⁻¹² × il massimo della matrice; nel
  dominio esatto se è esattamente 0), potenze con quadrati successivi.
- Un nodo con collezioni dentro un'espressione numerica (es. `1+Det(Mat A)`)
  viene valutato da `CollEval` e deve dare un numero (`TypeError` altrimenti).

## Posizione degli errori (v1.10.0)

Il lexer conserva `pos`/`len` di ogni token (testo fino a 60 000 byte); il
parser lancia `SyntaxError(messaggio, posizione, lunghezza)` sul token che non
può usare (sulla fine del testo se l'input finisce troppo presto) e registra in
ogni nodo l'intervallo della sotto-espressione. Durante la valutazione
`Env::at` indica il nodo dell'operazione in corso; una guardia RAII
(`ErrorLocationGuard`) lo copia in `EvalInfo::error` solo mentre un'eccezione
risale, quindi la valutazione senza errori non costa nulla (GRAPH campiona
milioni di punti). `evaluateEntry` aggiunge lo scostamento degli spazi iniziali
e riporta gli errori della destinazione dopo `→` sulla freccia.

## Errori

| Classe | Titolo mostrato | Esempi |
|---|---|---|
| `SyntaxError` | Syntax ERROR | parentesi non bilanciate, operando mancante |
| `MathError` | Math ERROR, Argument ERROR | divisione per zero, dominio, non convergenza, matrice singolare |
| `DimensionError` (v1.10.0) | Dimension ERROR | `[[1,2]]+[[1,2,3]]`, `Det([[1,2,3]])`, `{1,2}+{1,2,3}`, memoria non definita, oltre i limiti |
| `TypeError` (v1.10.0) | Type ERROR | `{1,2}+[[1,2]]`, `sin([[1]])`, `DotP({1},{2})`, `1/[[1]]` |
| `NonRealError` | Non-Real ERROR | `√-1` in modo Real |
| `MathError` | Stack ERROR | annidamento oltre i limiti |
| `MathError` | Memory ERROR | espressione troppo lunga; `std::bad_alloc` |

## Limiti (protezione dello stack da 32 KB dell'RP2350)

| Limite | Valore | Errore |
|---|---|---|
| Token per espressione (`MaxExpressionTokens`) | 320 | Memory ERROR |
| Livelli annidati: parentesi, segni, esponenti, argomenti (`MaxExpressionNesting`) | 32 | Stack ERROR |
| Funzioni di calcolo annidate (∫, d/dx, Σ, Π, SOLVE) | 4 | Stack ERROR |
| Budget di stack per valutazione (`FXAI_EVAL_STACK_BUDGET`) | 16 KB sul firmware | Stack ERROR |
| Modelli annidati nell'editor (`NaturalEditor::MaxTemplateDepth`) | 12 | Stack ERROR (modifica annullata) |
| Nodi nell'editor (`NaturalEditor::MaxNodes`) | 400 | Memory ERROR (modifica annullata) |

Le catene associative a sinistra (`1+2+3+…`, `2·x·y…`) sono valutate in modo
iterativo: la ricorsione dipende dall'annidamento, non dalla lunghezza.
Misure con `-fstack-usage` sul target ARM: `evalNode` 32–80 byte,
`evalChain` 104–128 byte, `callFunction` 80–128 byte; le funzioni con frame
grandi (`applyFunction`, `ridders` ≈1,2 KB) non restano attive durante la ricorsione.
Riferimenti tra funzioni di GRAPH (v1.8.0): `callReference` ≈230 byte per
livello, al massimo 5 livelli.

Valutatore esatto (v1.7.0, `arm-none-eabi-g++ -O3 -mcpu=cortex-m33`): `exactNode`
80 byte, `exactChain` 152, `exactToken` 184, `exactCall` 216, `exactSum` 288;
`exactApply` (1136 byte) e `exactBinary` (224) sono foglie non attive durante la
ricorsione. Le catene sono iterative come nel valutatore numerico e il budget
di stack viene controllato: oltre il budget l'esatto rinuncia senza errori.
Nucleo Run-Matrix: `evaluateEntry` 552 byte, `recalculateHistory` 432,
`editHistoryEntry` 96 (le copie di sicurezza di memoria e cronologia sono sull'heap).

v1.10.0 (`arm-none-eabi-g++ -O2`, Pico Plus 2 W): `CollEval::eval` 248 byte
(numerico) / 384 (esatto), `CollEval::call` 504 / 856, `binary` 432 / 832,
`evaluateAny` 472, `evaluateEntry` 656, `recalculateHistory` 488. Per ogni
livello di funzioni di collezione annidate servono ≈ 0,75 KB (≈ 1,2 KB
nell'esatto): `CollEval::eval` controlla lo stesso budget di 16 KB e oltre
lancia "Stack ERROR" (l'esatto rinuncia senza errori); con 20 livelli
annidati il calcolo riesce, con un budget ridotto il test `matrix_test`
verifica lo Stack ERROR invece di un crash. Gli elementi delle matrici sono
sull'heap (PSRAM sulla Plus), mai sullo stack.

## Moduli

| File | Contenuto |
|---|---|
| `expression.hpp/.cpp` | lexer, parser, valutatore (anche `CollEval` per liste, matrici e vettori), calcolo numerico, `CompiledExpression` |
| `value.hpp/.cpp` | `CalcValue`: numero, lista, matrice o vettore (v1.10.0) |
| `expression_ast.hpp` | `ast::Node` condiviso con l'editor e `ast::serialize` |
| `natural_editor.hpp/.cpp` | editor Natural V.P.A.M. (modelli, navigazione, undo/redo, limiti) |
| `number_format.hpp/.cpp` | formati di visualizzazione e frazioni esatte |
| `memory.hpp/.cpp` | variabili A–Z, Ans/PreAns (anche complessi) |
| `math_engine.hpp/.cpp` | matrici, vettori, frazioni, basi, sessagesimale, angolo |
| `solvers.hpp/.cpp` | polinomi, disequazioni, solver numerico |
| `statistics.hpp/.cpp` | statistiche, regressioni, distribuzioni, test |
| `exact.hpp/.cpp` | razionali, radicali, multipli di π, letterali periodici, regole di visualizzazione |
| `run_matrix.hpp/.cpp` | riga di Run-Matrix (→ verso variabili, memorie, Str, f1–f20, variabili di sistema; `:` e `◢`; Swap/\*Row), cronologia con ricalcolo, viste dei risultati (ENG, ►r∠θ) |
| `calc_library.hpp/.cpp` | v1.13.0: unità di CONVERT, distribuzioni, TVM, nomi mostrati, CATALOG (puro calcolo) |
| `physical_constants.hpp/.cpp` | v1.14.0: tabella unica delle costanti (nomi, alias, glifi, nomi italiani, valori SI, unità, incertezze, gruppi) |
| `graph_engine.hpp/.cpp`, `graph_solve.hpp/.cpp` | motore dei grafici e G-Solve sullo stesso valutatore ([GRAPH_ENGINE.md](GRAPH_ENGINE.md)) |

## Non ancora disponibile

(Aggiornato alla v1.13.0.) Derivate e integrali simbolici e limiti non
esistono sulla fx-CG50 (niente CAS): restano numerici come sulla
calcolatrice. Le regressioni sinusoidale e logistica (modalità Statistics)
non sono realizzate; Run-Matrix è completo ([RUN_MATRIX.md](RUN_MATRIX.md)).

### Prima della v1.11.0

Liste 7–26 e nomi delle liste, derivate e integrali simbolici, limiti, operatori bit a bit Base-N (and/or/xor
come operazioni sui bit nella modalità Base-N intera), simboli ingegneristici,
regressioni sinusoidale e logistica. Vedi [FEATURE_MATRIX.md](FEATURE_MATRIX.md).
