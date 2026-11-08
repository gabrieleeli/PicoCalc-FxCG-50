# Motore matematico — v1.6.0

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
```

Esiste **un solo** parser e **un solo** valutatore (templato sul tipo numerico):
Run-Matrix, Graph, Table, Equation, Statistics e i test usano lo stesso codice
(`include/expression.hpp`). `CompiledExpression` analizza una volta e valuta molte
volte (grafici, tabelle, solver).

## Sintassi e priorità (come nel manuale fx-CG50)

1. parentesi, funzioni con parentesi; le parentesi finali si possono omettere;
2. postfissi: `!`, `°` (gradi);
3. potenza `^`, associativa a destra, esponente con segno (`2^-2`);
4. segno unario (`-2^2 = -4`);
5. **moltiplicazione implicita** (`2x`, `2(3)`, `2pi`, `x sin x`), più forte di `*` e `/`
   (`6/2(1+2) = 1`, `1/2x = 1/(2x)`);
6. funzioni prefisse senza parentesi prendono il prodotto implicito che segue
   (`sin 2x = sin(2x)`), fermandosi alla funzione successiva (`sin x cos x`);
7. `*`, `/`, `%` (modulo);
8. `+`, `-`.

Le lettere digitate vengono unite in parole note (`sinx`, `2pix`); le variabili sono
le lettere A–Z, `Ans`, `PreAns`, `θ` e le variabili legate dalle app (x, y, …).
Letterali `0x`, `0b`, `0o` per Base-N.

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
| Calcolo (valutazione pigra) | integral(f,a,b) derivative(f,x₀) derivative2(f,x₀) sum(f,a,b) prod(f,a,b) solve(f,x₀) |
| Costanti | pi tau phi eulergamma sqrt2 ln2 ln10 e; fisiche: speedoflight/physc, physg, physh, na, kb, qe, g0 (anche con nomi estesi) |

## Metodi numerici

| Funzione | Metodo | Note |
|---|---|---|
| ∫ | Gauss–Kronrod G7/K15 adattivo | tolleranza 1e-10, profondità 18, max 20 000 valutazioni |
| d/dx, d²/dx² | estrapolazione di Ridders sulle differenze centrali | d²: Ridders annidato |
| SOLVE | Newton con derivata di Ridders | 100 iterazioni, controllo di convergenza |
| Σ, Π | somma/prodotto su indici interi | max 100 001 termini |
| Polinomi (Equation) | Aberth–Ehrlich | radici complesse, grado 2–6 |
| Sistemi lineari | eliminazione di Gauss con pivot parziale | 2–6 incognite |
| Disequazioni | radici reali + segno tra le radici | grado 1–6, unione di intervalli |
| Solver f(x)=g(x) | Newton con derivata numerica e protezione dalla divergenza | |
| Distribuzioni | beta e gamma regolarizzate | normale, binomiale (log-gamma), Poisson, t, χ², F |

## Formati di visualizzazione

`number_format.hpp`: Norm1/Norm2 (10 cifre significative, soglie esponenziali
come la CG50), Fix 0–9 decimali, Sci 1–10 cifre, Eng; frazioni esatte (frazioni continue,
denominatore ≤ 9999) per S⇔D; Base-N Hex/Bin/Oct; sessagesimale; complessi in
forma `a+bi` o `r∠θ`. Gli zeri con segno vengono normalizzati (`√-4 = 2i`, non `-2i`).

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
| `run_matrix.hpp/.cpp` | esecuzione di una riga di Run-Matrix (→, Pol/Rec, S⇔D) |

## Non ancora disponibile

Matrici e vettori come valori del parser (esistono come API C++), derivate e
integrali simbolici, limiti, operatori logici Base-N nel parser, regressioni
sinusoidale e logistica. Vedi [FEATURE_MATRIX.md](FEATURE_MATRIX.md).
