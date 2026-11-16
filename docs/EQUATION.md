# EQUATION — guida della modalità (v1.12.0)

La modalità **EQUATION** (tasto `A` nel menu principale) riproduce il
capitolo 4 "Equation Calculations" del manuale fx-CG50 (pagine 4-1…4-5) con
le parti collegate: input/output Math (1-29), VARS › EQUATION (1-33), SET UP
(1-35, 1-36) e i messaggi della tabella degli errori (α-1…α-3). L'analisi
delle fonti e la tabella di parità sono in
[reports/ANALISI_FONTI_v1.12.0.md](reports/ANALISI_FONTI_v1.12.0.md); lo
stato finale di ogni voce in [reports/SVILUPPO_v1.12.0.md](reports/SVILUPPO_v1.12.0.md).

Schermate: [62](screenshots/62_equation_menu.png) menu,
[63](screenshots/63_equation_siml_coefficients.png)/[64](screenshots/64_equation_siml_solutions.png)
SIML, [65](screenshots/65_equation_coefficient_edit.png) EDIT,
[66](screenshots/66_equation_siml_exact.png) soluzione esatta,
[67](screenshots/67_equation_no_solution.png) No Solution,
[68](screenshots/68_equation_infinite.png) Infinitely Many Solutions,
[08](screenshots/08_equation_roots.png)/[69](screenshots/69_equation_poly_multiple.png)/[70](screenshots/70_equation_poly_complex.png)/[71](screenshots/71_equation_poly_polar.png)
POLY, [72](screenshots/72_equation_solver_input.png)/[73](screenshots/73_equation_solver_table.png)/[74](screenshots/74_equation_solver_result.png)
SOLVER, [75](screenshots/75_equation_vars_simres.png) VARS › SimRes,
[76](screenshots/76_equation_inequality.png) disequazioni (estensione).
Script del simulatore: `tests/sim/equation.txt`.

## 1. Schermata iniziale

```
Equation
Select Type
F1:Simultaneous
F2:Polynomial
F3:Solver
F4:Inequality      estensione
[SIMUL][POLY][SOLVER][INEQ]
```

F1…F3 (o ↑↓ ed EXE) scelgono il tipo; **F4 INEQ** è un'estensione della
PicoCalc (disequazioni polinomiali), assente sulla fx-CG50. EXIT torna al
menu principale.

## 2. SIML — sistemi lineari (2…6 incognite)

1. **F1 SIMUL**, poi "Number of Unknowns?" F1(2)…F5(6).
2. Schermata dei coefficienti: intestazione `aₙX+bₙY+cₙZ=dₙ` (da 4 incognite
   `aₙX1+bₙX2+…`), colonne `a b c d…`, righe numerate tra parentesi di
   matrice, cella corrente evidenziata e il suo valore **in forma naturale in
   basso a destra**. Con più di 4 colonne la griglia scorre (◀ ▶).
3. Si digita il valore e **EXE** lo memorizza passando alla cella successiva
   (a1 → b1 → c1 … → aₙ → bₙ → cₙ); EXE senza input passa oltre lasciando il
   valore. **EXIT** durante l'immissione annulla e rimette il valore precedente.
4. Tasti: **F1 SOLVE**, **F2 DELETE** (la cella torna 0), **F3 CLEAR** (tutti
   i coefficienti a 0, cursore su a1), **F4 EDIT** (modifica del valore
   attuale nell'editor). EXIT torna alla scelta delle incognite.

### Coefficienti

Ogni coefficiente è un'**espressione dello stesso parser di Run-Matrix**:
frazioni (modello F1 in Math, `/` o `⌟` in Line), radici, π, potenze,
funzioni, variabili A–Z, `Ans`, numeri complessi con `i` (anche nel modo
Real, come nell'editor MAT/VCT). Il valore è conservato **in forma esatta**
(frazione, radicale quadratico, multiplo di π; per i complessi una parte
reale e una immaginaria esatte quando l'espressione è un polinomio in `i`,
per esempio `(1+√3 i)/2`, `(1+i)²`), altrimenti in virgola mobile. Durante
l'immissione i tasti funzione diventano: F1 frazione (SHIFT: mista), F2 √
(SHIFT: x²), F3 potenza (SHIFT: π), F4 menu MATH, F5 `i`, F6 Ans (SHIFT: θ).

### Soluzioni

- Colonna `X Y Z` (o `X1…X6`) tra parentesi, riga evidenziata con ↑↓; la
  cella mostra il decimale, **in basso a destra il valore in forma naturale**
  (frazioni, √, π: manuale 1-29). Un valore più largo dello schermo scorre
  con ◀ ▶.
- **F1 REPEAT** (o EXIT) torna ai coefficienti conservandoli; **F5 S⇔D**
  alterna forma esatta e decimale della riga selezionata.
- **No Solution**: il sistema non ha soluzioni.
- **Infinitely Many Solutions** con la **formula** di ogni incognita: le
  incognite libere compaiono come `Z=Z`; esempio del manuale 4-2:
  `X=−½i`, `Y=1.4−1.8i−(2+i)Z`, `Z=Z`.
- I decimali digitati restano decimali nei risultati che ne dipendono (come
  `Y=1.4−1.8i…` accanto a `X=−½i` nel manuale).
- Calcolo: Gauss-Jordan **esatto** (razionali, radicali, π, complessi) se
  tutti i coefficienti hanno una forma esatta; altrimenti in virgola mobile
  con equilibratura delle righe e pivoting parziale. Soluzioni con
  |x| ≥ 10¹⁰⁰: **Ma ERROR**; non nulle sotto 10⁻⁹⁹: **Underflow**.

## 3. POLY — equazioni di grado 2…6

1. **F2 POLY**, "Degree?" F1(2)…F5(6); intestazione `aX²+bX+c=0`,
   `aX³+bX²+cX+d=0`…; coefficienti su una riga, stessi tasti di SIML.
2. `a = 0` dà **Ma ERROR**.
3. Soluzioni `X1…Xn`: **reali in ordine decrescente**, poi le complesse
   (parte reale decrescente, parte immaginaria positiva prima); una radice
   multipla compare **una volta con `×k`** (magenta).
4. **Complex Mode** (SET UP): **Real** mostra solo le radici reali ("No
   Solution" se non ce ne sono), **a+bi** tutte, **r∠θ** in forma polare
   (esatta in Math: `1∠π`; in gradi `1∠180`).
5. Forme esatte (coefficienti razionali): scomposizione senza quadrati
   (algoritmo di Yun) per le molteplicità esatte, radici razionali, fattori di
   grado 2 con la formula esatta, biquadratiche (`x⁴−5x²+6`) e quartiche pari
   scomponibili (`x⁴+x²+1`); esempio del manuale: `x²+3x+5=0` →
   `(−3+√11 i)/2`. Gli altri fattori (per esempio `x³−2`) e i coefficienti non
   razionali si risolvono in virgola mobile (Aberth-Ehrlich con rifinitura di
   Newton e raggruppamento delle radici multiple).

## 4. SOLVER

1. **F3 SOLVER**: la riga `Eq:` si scrive come sul libro (editor Math o
   Line). `X` e `x` sono la stessa variabile; senza `=` il secondo membro è 0;
   più di un `=` dà **Syntax ERROR**.
2. La **tabella delle variabili** elenca le variabili dell'equazione
   nell'ordine in cui compaiono, con il valore delle memorie A–Z (che sono
   anche le variabili di Run-Matrix), poi **Lower** e **Upper**
   (−9×10⁹⁹ e 9×10⁹⁹). EXE o un carattere modificano la riga; i valori sono
   espressioni.
3. Si evidenzia la variabile da risolvere e si preme **F6 SOLVE**: il valore
   della variabile è il valore iniziale; metodo di Newton e, se non
   converge, ricerca di un cambio di segno con bisezione entro Lower/Upper.
4. Risultato: `Eq:`, `V=16.8`, `Lft=14`, `Rgt=14` (esempio del manuale 4-5);
   **Retry** se la convergenza non è sufficiente; **F1 REPEAT** torna alla
   tabella. La variabile risolta resta memorizzata.
5. **F1 RECALL** rimette i valori che le variabili avevano prima dell'ultimo
   SOLVE; **F2 DELETE** cancella l'equazione (riga Eq), azzera la variabile o
   rimette Lower/Upper predefiniti.
6. Messaggi: **No Variable** (nessuna variabile), **Can't Solve!**
   (soluzione fuori da Lower–Upper), **Time Out** (nessuna convergenza).

## 5. Uso dei risultati

- **VARS › EQUATION** in Run-Matrix (VARS, ultima categoria): `SimRes`
  (soluzioni n×1, o la **Rref** della matrice completa con No Solution /
  Infinitely Many Solutions), `SimCoef` (n×(n+1)), `PlyRes` (radici mostrate,
  m×1), `PlyCoef` (1×(g+1)). Sono nomi dello stesso parser: `SimRes*6`,
  `Trn(PlyCoef)`… Il risultato va in **MatAns** come ogni matrice. Senza
  soluzioni o coefficienti: **Ma ERROR**; `SimRes` e `SimCoef` non si usano
  nella stessa espressione (nota *4 del manuale 1-33).
- Il Solver scrive la soluzione nella variabile risolta (A–Z), usabile in
  Run-Matrix, GRAPH, TABLE.
- Coefficienti, equazione, limiti, ultima variabile risolta e ultime
  soluzioni restano nel record del workspace (**v8**).

## 6. Impostazioni (SET UP)

Dalla modalità, SHIFT MENU (PicoCalc F10) apre il SET UP; **EXIT torna a
EQUATION** nella stessa schermata, con le soluzioni ricalcolate. Contano:
Input/Output (Math: forme naturali; Line: decimali, S⇔D dà le frazioni con
`⌟`), Angolo (argomento di r∠θ, funzioni nei coefficienti), Display
(Norm/Fix/Sci/Eng, anche ×10ⁿ in 2D), Complessi (Real/a+bi/r∠θ), Forme
esatte, Frac Result (d/c, ab/c).

## 7. Estensione INEQ

Disequazioni polinomiali `P(X) > < ≥ ≤ 0` di grado 1…6 (F5 REL cambia la
relazione): insieme delle soluzioni e intervalli. La funzione di libreria
`solveInequalitySystem` (`math_engine.hpp`) risolve sistemi di disequazioni di
secondo grado; non ha una schermata.

## 8. Differenze dalla fx-CG50

Schermo 320×320 (4 colonne visibili con scorrimento), tasti dei modelli sui
tasti funzione durante l'immissione, S⇔D su F5, nomi `X1…X6` da 4
incognite, "No Solution" per POLY senza radici reali nel modo Real, RECALL /
DELETE del Solver interpretati (il manuale non li descrive), variabili del
Solver A–Z (r è la memoria R, θ non è una variabile), nessuna interruzione
con AC (il calcolo è limitato e termina con Time Out). Dettagli e motivi nel
rapporto delle fonti, §5.
