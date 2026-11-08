# Motore matematico — v1.8.0

Riferimenti storici: `archive/VPAM_SHARED_AST.md`, `archive/RUN_MATRIX_PHASE*.md` e
`archive/math_engine_original.py` descrivono le versioni precedenti (vedi [README.md](README.md)).

## Pipeline unica

```
editor Natural (ast::Node Row/Fraction/Radical/Power/Calculus/Call)
   │ ast::serialize            testo lineare canonico, es. "(1)/(3)+sqrt(2)"
   ▼
Lexer → Parser                 ast::Node Binary/Unary/Call/Group/Token
   ▼
evalNode<double>               modo Real
evalNode<std::complex<double>> modi a+bi e r∠θ
exactNode → exact::Value       forma esatta (Run-Matrix), sullo stesso AST
```

Esiste **un solo** parser e **un solo** valutatore numerico (templato sul tipo):
Run-Matrix, Graph, Table, Equation, Statistics e i test usano lo stesso codice
(`include/expression.hpp`). `CompiledExpression` analizza una volta e valuta molte
volte (grafici, tabelle, solver).

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
| Logica e confronti | `not`, operatori `and or xor`, `= ≠ < > <= >=` |
| Interi e arrotondamenti (v1.7.0) | `modexp(n,p,m)` (MOD_Exp), `rndfix(x,n)` (RndFix), operatori `Int÷`, `Rmdr` |
| Casuali (v1.7.0) | `rannorm(σ,μ)` (RanNorm#), `ranbin(n,p)` (RanBin#) |
| Calcolo (valutazione pigra) | integral(f,a,b) derivative(f,x₀) derivative2(f,x₀) sum(f,a,b) prod(f,a,b) solve(f,x₀) |
| Costanti | pi tau phi eulergamma sqrt2 ln2 ln10 e; fisiche: speedoflight/physc, physg, physh, na, kb, qe, g0 (anche con nomi estesi) |

## Metodi numerici

| Funzione | Metodo | Note |
|---|---|---|
| ∫ | Gauss–Kronrod G7/K15 adattivo | tolleranza 1e-10, profondità 18, max 20 000 valutazioni; anche per funzioni C++ (`integrateFunction`, usato da G-Solve ∫dx) |
| d/dx, d²/dx² | estrapolazione di Ridders sulle differenze centrali | d²: Ridders annidato; `differentiateFunction` per TRACE, Tangent/Norm e MAX/MIN di GRAPH |
| Radici, estremi, intersezioni di GRAPH | Brent–Dekker e minimizzazione di Brent | in `graph_solve.cpp`, vedi [GRAPH_ENGINE.md](GRAPH_ENGINE.md#g-solve) |
| SOLVE | Newton con derivata di Ridders | 100 iterazioni, controllo di convergenza |
| Σ, Π | somma/prodotto su indici interi | max 100 001 termini |
| Polinomi (Equation) | Aberth–Ehrlich | radici complesse, grado 2–6 |
| Sistemi lineari | eliminazione di Gauss con pivot parziale | 2–6 incognite |
| Disequazioni | radici reali + segno tra le radici | grado 1–6, unione di intervalli |
| Solver f(x)=g(x) | Newton con derivata numerica e protezione dalla divergenza | |
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

## Errori

| Classe | Titolo mostrato | Esempi |
|---|---|---|
| `SyntaxError` | Syntax ERROR | parentesi non bilanciate, operando mancante |
| `MathError` | Math ERROR, Argument ERROR | divisione per zero, dominio, non convergenza |
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

## Moduli

| File | Contenuto |
|---|---|
| `expression.hpp/.cpp` | lexer, parser, valutatore, calcolo numerico, `CompiledExpression` |
| `expression_ast.hpp` | `ast::Node` condiviso con l'editor e `ast::serialize` |
| `natural_editor.hpp/.cpp` | editor Natural V.P.A.M. (modelli, navigazione, undo/redo, limiti) |
| `number_format.hpp/.cpp` | formati di visualizzazione e frazioni esatte |
| `memory.hpp/.cpp` | variabili A–Z, Ans/PreAns (anche complessi) |
| `math_engine.hpp/.cpp` | matrici, vettori, frazioni, basi, sessagesimale, angolo |
| `solvers.hpp/.cpp` | polinomi, disequazioni, solver numerico |
| `statistics.hpp/.cpp` | statistiche, regressioni, distribuzioni, test |
| `exact.hpp/.cpp` | razionali, radicali, multipli di π, letterali periodici, regole di visualizzazione |
| `run_matrix.hpp/.cpp` | riga di Run-Matrix (→, →A~F, Pol/Rec), cronologia con ricalcolo, viste dei risultati |
| `graph_engine.hpp/.cpp`, `graph_solve.hpp/.cpp` | motore dei grafici e G-Solve sullo stesso valutatore ([GRAPH_ENGINE.md](GRAPH_ENGINE.md)) |

## Non ancora disponibile

Matrici, vettori e liste come valori del parser (esistono come API C++),
derivate e integrali simbolici, limiti, operatori bit a bit Base-N (and/or/xor
come operazioni sui bit nella modalità Base-N intera), simboli ingegneristici,
regressioni sinusoidale e logistica. Vedi [FEATURE_MATRIX.md](FEATURE_MATRIX.md).
