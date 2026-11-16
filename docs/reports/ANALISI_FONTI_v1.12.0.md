# Analisi delle fonti della v1.12.0 — EQUATION

Data: 16 novembre 2026. Rapporto scritto **prima** dell'implementazione: la
tabella di parità del §4 è il perimetro del lavoro della v1.12.0
(completamento della modalità EQUATION). Lo stato finale di ogni voce è nel
rapporto di sviluppo ([SVILUPPO_v1.12.0.md](SVILUPPO_v1.12.0.md), §4).

## 1. Fonti consultate

| Fonte | Uso |
|---|---|
| Manuale software **fx-CG50 / fx-CG50 AU, versione 3.60** (PDF ufficiale Casio, `fx-CG50_Soft_v360_EN.pdf`, scaricato da casio.com; 678 pagine) | riferimento principale: capitolo 4 "Equation Calculations" (pagine **4-1…4-5**), §1-29 "Math Input/Output Mode Input and Result Display in the Equation Mode", §1-33 VARS › EQUATION, §1-35…1-36 SET UP, §2-27 Solve/SolveN, tabella degli errori **α-1…α-3**, elenco delle strip eActivity (10-x) e dei comandi di programma (8-x) |
| Manuale software fx-9860GIII v3.40 (PDF ufficiale) | confronto: stesso capitolo 4 con i nomi brevi dei tasti (SIML, SOLV, REPT, CLR); conferma che la struttura è comune alla famiglia |
| Pagine renderizzate del PDF (4-1…4-5, 1-29, 1-36) | lette come immagini per i dettagli che il testo non contiene: tasti funzione di ogni schermata, intestazioni, formato delle celle, marcatore di molteplicità, schermata "Infinitely Many Solutions" |
| Sorgenti del progetto alla v1.11.0 | stato attuale (colonna "v1.11.0") |

Solo riferimento di comportamento: nessun codice, ROM, font, icona o grafica
Casio è stato copiato. I testi visualizzati dalla PicoCalc sono riscritti
(messaggi di errore con la parola chiave del manuale seguita da una
spiegazione in italiano, come nel resto del firmware).

## 2. Che cosa mostra il manuale (sintesi)

- **Menu** (4-1): schermata "Equation / Select Type / F1:Simultaneous /
  F2:Polynomial / F3:Solver", tasti **SIMUL POLY SOLVER**.
- **SIML** (4-1, 4-2): numero di incognite 2…6 con F1…F5; schermata dei
  coefficienti con l'intestazione `anX+bnY+cnZ=dn`, colonne `a b c d`,
  righe numerate 1…n tra parentesi di matrice, cella corrente evidenziata e
  il suo valore in basso a destra; tasti **SOLVE DELETE CLEAR EDIT**; EXE
  memorizza e passa alla cella successiva (a1→b1→c1…), EXIT annulla
  l'immissione e rimette il valore precedente; frazioni e variabili come
  coefficienti. Soluzioni in colonna (`X Y Z`) con la cella evidenziata e il
  valore in basso a destra, tasto **REPEAT**. Messaggi "No Solution",
  "Ma ERROR" (soluzione non trovata), "Infinitely Many Solutions" **con la
  formula** (esempio con coefficienti complessi: `X=−½i`,
  `Y=1.4−1.8i−(2+i)Z`, `Z=Z`). Metodo: inversione della matrice (precisione
  ridotta vicino a determinante nullo); 15 cifre interne, 10 mostrate.
- **POLY** (4-3, 4-4): grado 2…6, intestazione `aX³+bX²+cX+d=0`, celle
  `a b c d` su una riga, stessi tasti. Soluzioni `X1…Xn`: reali in ordine
  **decrescente** (2, 1, −1), poi le complesse con parte immaginaria
  positiva prima; radici multiple mostrate **una volta con `×3`** (magenta).
  Complex Mode: **Real** mostra solo le radici reali, **a+bi** tutte
  (`−0.5+1.3228i` nella cella, valore completo in basso), **r∠θ** in forma
  polare (`1∠3.1415` nella cella, **`1∠π`** in basso: forma esatta). Errore se
  non trova soluzioni; precisione ridotta con radici multiple.
- **Math** (1-29): soluzioni di SIML e POLY in forma naturale (frazioni, √,
  π) "quando possibile"; esempio x²+3x+5=0 in a+bi: celle `−1.5+1.6583i`,
  valore in basso `(−3+√11 i)/2`. SOLVER accetta l'input naturale.
- **SOLVER** (4-4, 4-5): equazione scritta come sul libro (X e x sono la
  stessa variabile; senza "=" il secondo membro è 0; più di un "=" è un
  errore); tabella delle variabili con i valori, **Lower/Upper**
  (−9×10⁹⁹ / 9×10⁹⁹), tasti **RECALL DELETE … SOLVE**; si evidenzia la
  variabile e si preme SOLVE; risultato `Eq:…`, `V=16.8`, `Lft=14`,
  `Rgt=14`, tasto REPEAT; metodo di Newton; "Retry" se la convergenza non è
  sufficiente; errore se la soluzione esce dall'intervallo; una sola
  soluzione.
- **VARS › EQUATION** (1-33): `SimRes`, `SimCoef`, `PlyRes`, `PlyCoef` in
  Run-Matrix: matrici delle soluzioni e dei coefficienti, memorizzate in
  MatAns; errore se mancano coefficienti o soluzioni; con "Infinitely Many
  Solutions" o "No Solution" il risultato è la **Rref**; coefficienti e
  soluzioni del sistema non si richiamano insieme.
- **Errori** (α-1…α-3): Syntax ERROR, Ma ERROR, Stack ERROR, Non-Real ERROR,
  **Can't Solve!** (fuori dall'intervallo), **Time Out** (nessuna
  convergenza), **Underflow** (anche nelle equazioni:
  `1×10⁹⁹x²+1×10⁹⁹x+1×10⁻⁹⁹=0`), e quelli della modalità: Infinitely Many
  Solutions, No Solution, **No Variable** (equazione del Solver senza
  variabili).

## 3. Stato del codice alla v1.11.0

`src/apps/equation_app.cpp` (216 righe) e `src/solvers.cpp`: menu con
quattro voci (SIML, POLY, SOLVER, INEQ), griglia numerica `CellGrid` con
valori `double` (le espressioni sono valutate subito: forme esatte, complessi
e testo persi), soluzioni come elenco di testi `x1 = …` (sempre decimali e,
per POLY, sempre `a+bi`), radici reali in ordine **crescente**, nessuna
molteplicità, nessuna forma esatta, "Infinite soluzioni" senza formula,
SOLVER solo in `x` con un valore iniziale (nessuna tabella delle variabili,
nessun limite, X sempre memorizzata), nessun VARS › EQUATION. Il record del
workspace (v7) conserva coefficienti `double`, l'espressione del Solver e il
valore iniziale.

## 4. Tabella di parità (perimetro del lavoro)

Stato v1.11.0: **P** presente, **D** diverso, **M** mancante. "Pag." è la
pagina del manuale fx-CG50 v3.60.

### 4.1 Menu, navigazione, impostazioni

| ID | Voce del manuale | Pag. | v1.11.0 | Intervento v1.12.0 |
|---|---|---|---|---|
| E1 | Icona Equation nel menu principale (tasto diretto) | 1-2 | P | — |
| E2 | Schermata iniziale "Select Type" F1 Simultaneous / F2 Polynomial / F3 Solver, tasti SIMUL POLY SOLVER | 4-1 | D (testi e 4ª voce) | schermata come il manuale; INEQ resta su F4 come estensione segnata |
| E3 | Barra di stato Math/Line, angolo, Display, d/c-ab/c, Complex Mode | 4-1 | P | — |
| E4 | Scelta del numero di incognite / del grado con F1(2)…F5(6) | 4-1, 4-3 | P (titoli diversi) | domande "Number of Unknowns?" / "Degree?" |
| E5 | EXIT torna alla schermata precedente (soluzioni → coefficienti → scelta → menu) | 4-2 | P | mantenuto per tutte le schermate nuove |
| E6 | REPEAT torna ai coefficienti conservandoli | 4-2, 4-4 | P (anche EXE/EXIT) | REPEAT su F1, valori conservati |
| E7 | SET UP dalla modalità (SHIFT MENU): Input/Output, Angle, Complex Mode, Display, Frac Result | 1-35, 1-36 | P (SET UP globale di SYSTEM) | le impostazioni ora cambiano davvero la vista dei risultati (§4.5) |
| E8 | Dati della modalità conservati spegnendo (coefficienti, equazione, variabili) | 1-33 | P (solo `double`) | record v8 con le forme esatte |

### 4.2 SIML — sistemi lineari

| ID | Voce del manuale | Pag. | v1.11.0 | Intervento v1.12.0 |
|---|---|---|---|---|
| S1 | 2…6 incognite | 4-1 | P | — |
| S2 | Intestazione `anX+bnY=cn`, `anX+bnY+cnZ=dn` | 4-2 | D (`an·x1+bn·x2+...=rhs`) | intestazioni del manuale; 4–6 incognite `X1…X6` (scelta, §5) |
| S3 | Griglia: colonne `a b c d…`, righe 1…n, parentesi di matrice, cella evidenziata, valore della cella in basso a destra | 4-2 | D (colonne `an…rhs`, nessuna parentesi, valore in basso a sinistra) | griglia come il manuale, valore in forma naturale in basso a destra |
| S4 | EXE memorizza e passa alla cella successiva a1→b1→c1…→an→bn→cn | 4-1 | P | — |
| S5 | Frazioni e variabili come coefficienti | 4-1 | D (valutate in `double`: forma esatta persa) | coefficienti = espressioni dello stesso parser, valore **esatto** conservato (frazioni, radici, π, Ans, A–Z, complessi) |
| S6 | EXIT durante l'immissione annulla e rimette il valore precedente | 4-1 | P | — |
| S7 | Spostarsi su un coefficiente e digitare il nuovo valore | 4-1 | P | — |
| S8 | F3 CLEAR: tutti i coefficienti a zero | 4-1 | P | — |
| S9 | F2 DELETE | 4-2 | D (DEL) | F2 DELETE: il coefficiente corrente torna 0 |
| S10 | F4 EDIT: modifica del coefficiente corrente | 4-2 | M (solo EXE) | F4 EDIT con il valore esatto nell'editor |
| S11 | F1 SOLVE | 4-2 | P | — |
| S12 | Schermata delle soluzioni: `X Y Z` (o `X1…`) in colonna tra parentesi, cella evidenziata, valore in basso a destra, ↑↓ | 4-2 | D (elenco di testi, riga "rango") | schermata del manuale |
| S13 | Soluzioni esatte in Math (frazioni, √, π) | 1-29 | M | eliminazione di Gauss-Jordan **esatta** (razionali, radicali, π, complessi esatti), decimali se una forma esatta non esiste |
| S14 | Line: soluzioni decimali | 1-29 | P | frazioni `⌟` dell'output Line come Run-Matrix |
| S15 | "No Solution" | 4-2 | D ("Nessuna soluzione") | messaggio del manuale |
| S16 | "Infinitely Many Solutions" **con la formula** (variabili libere, `Z=Z`) | 4-2 | D (solo testo, nessuna formula) | formula parametrica dalla Rref, esatta quando possibile |
| S17 | "Ma ERROR" se una soluzione non si trova (fuori intervallo, overflow) | 4-2 | M | Ma ERROR per soluzioni ≥ 10¹⁰⁰ o coefficienti non finiti |
| S18 | Coefficienti e soluzioni complessi (esempio della formula con `i`) | 4-2 | M (solo reali) | coefficienti complessi, soluzioni complesse esatte o decimali |
| S19 | Complex Mode a+bi / r∠θ per le soluzioni | 1-36 | M | formati a+bi e r∠θ (esatto in Math quando possibile) |
| S20 | Precisione: 15 cifre interne, 10 mostrate; inversione della matrice | 4-2 | P (pivoting parziale) | pivoting parziale con soglia relativa; risultati arrotondati al formato del display |

### 4.3 POLY — equazioni di grado 2…6

| ID | Voce del manuale | Pag. | v1.11.0 | Intervento v1.12.0 |
|---|---|---|---|---|
| P1 | Grado 2…6 | 4-3 | P | — |
| P2 | Intestazione `aX²+bX+c=0`, `aX³+bX²+cX+d=0`… | 4-3 | D (minuscole) | intestazione del manuale |
| P3 | Celle `a b c…` su una riga | 4-3 | P | griglia come SIML (parentesi, valore in basso a destra) |
| P4 | Frazioni e variabili come coefficienti | 4-3 | D (`double`) | come S5 |
| P5 | EXE / EXIT / modifica / F3 CLEAR | 4-3 | P | — |
| P6 | F2 DELETE, F4 EDIT | 4-3 (tasti 4-2) | D / M | come S9, S10 |
| P7 | `a ≠ 0` | 4-3 | P ("Math ERROR") | "Ma ERROR" |
| P8 | Soluzioni `X1…Xn` in colonna, cella evidenziata, valore in basso a destra | 4-3 | D (elenco) | schermata del manuale |
| P9 | Ordine: reali **decrescenti**, poi complesse (parte immaginaria positiva prima) | 4-3, 4-4 | D (reali crescenti) | ordine del manuale |
| P10 | Radici multiple una sola volta con `×k` | 4-3 | M (ripetute, con rumore numerico) | scomposizione senza quadrati (algoritmo di Yun) sui razionali: molteplicità esatte; raggruppamento numerico per gli altri casi |
| P11 | Complex Mode Real: solo radici reali | 4-4 | M (sempre a+bi) | come il manuale |
| P12 | Complex Mode a+bi | 4-4 | P | — |
| P13 | Complex Mode r∠θ (cella decimale, `1∠π` esatto in basso) | 4-4 | M | forma polare, esatta in Math (modulo radicale, argomento multiplo di π o in gradi) |
| P14 | Radici esatte in Math: frazioni e radicali, complesse `(−3+√11 i)/2` | 1-29 | M | radici razionali esatte (teorema delle radici razionali), fattori di grado 2 risolti con la formula esatta, radici complesse esatte con denominatore comune |
| P15 | Errore se la soluzione non si trova; Underflow per valori troppo distanti | 4-4, α-2 | M | Ma ERROR / Underflow come il manuale |
| P16 | Coefficienti complessi | 1-29 | M | Aberth-Ehrlich sui coefficienti complessi |

### 4.4 SOLVER

| ID | Voce del manuale | Pag. | v1.11.0 | Intervento v1.12.0 |
|---|---|---|---|---|
| V1 | Equazione scritta come sul libro, input naturale in Math | 4-4, 1-29 | D (testo lineare) | editor Math/Line di Run-Matrix |
| V2 | X e x sono la stessa variabile | 4-4 | P | — |
| V3 | Senza "=": secondo membro 0 | 4-4 | P | — |
| V4 | Più di un "=": errore | 4-4 | P | "Syntax ERROR" |
| V5 | **Tabella delle variabili** dell'equazione con i valori (le memorie A–Z) | 4-5 | M (solo X) | tabella di tutte le variabili, valori = memorie A–Z, modificabili con espressioni |
| V6 | **Lower / Upper** (−9×10⁹⁹ / 9×10⁹⁹) | 4-5 | M | limiti nella tabella, conservati nel record |
| V7 | Scelta della variabile da risolvere + F6 SOLVE | 4-5 | M (sempre X, F1/F6) | qualsiasi variabile |
| V8 | F1 RECALL, F2 DELETE | 4-5 | M | RECALL: valori delle variabili prima dell'ultimo SOLVE; DELETE: cancella l'equazione (riga Eq) o azzera la variabile (scelta, §5) |
| V9 | Risultato `Eq:…`, `V=…`, `Lft=`, `Rgt=`, REPEAT | 4-5 | D (testi diversi) | schermata del manuale con l'equazione in forma naturale |
| V10 | Metodo di Newton, valore iniziale = valore della variabile | 4-5 | P (solo X) | Newton + bisezione nell'intervallo, per ogni variabile |
| V11 | "Retry" se la convergenza non è sufficiente | 4-5 | M | risultato mostrato con "Retry" |
| V12 | Errore se la soluzione esce da Lower/Upper ("Can't Solve!") | 4-5, α-2 | M | come il manuale |
| V13 | "Time Out" se non converge | 2-27, α-2 | D ("SOLVE non converge") | "Time Out" |
| V14 | "No Variable" se l'equazione non ha variabili | α-3 | M | come il manuale |
| V15 | La variabile risolta resta memorizzata (V=16.8 nella tabella dopo REPEAT) | 4-5 | D (solo X) | la variabile scelta riceve la soluzione |
| V16 | Esempio H=VT−½GT² (H=14, T=2, G=9.8 → V=16.8, Lft=Rgt=14) | 4-5 | M | caso di test |
| V17 | Una sola soluzione (per più soluzioni POLY) | 4-5 | P | — |

### 4.5 Visualizzazione e uso dei risultati

| ID | Voce del manuale | Pag. | v1.11.0 | Intervento v1.12.0 |
|---|---|---|---|---|
| D1 | Math: forme naturali (frazioni, √, π) con gli stessi modelli di Run-Matrix | 1-29 | M | alberi 2D di Run-Matrix (`exactValueTree`) disegnati da `math_renderer` |
| D2 | Frac Result d/c / ab/c | 1-35 | M | frazioni miste o improprie come Run-Matrix |
| D3 | Display Norm / Fix / Sci / Eng | 1-36 | P (solo testo) | stesso `formatNumber` anche nella vista 2D (×10ⁿ, simboli Eng) |
| D4 | Risultati lunghi: valore completo leggibile | 1-23 | M (troncati) | valore in basso scorrevole con ◀ ▶ |
| D5 | S⇔D (tasto della tastiera) | 2-x | M | F5 S⇔D sulle schermate delle soluzioni, come in Run-Matrix |
| D6 | VARS › EQUATION: SimRes, SimCoef, PlyRes, PlyCoef in Run-Matrix (in MatAns) | 1-33 | M | quattro nomi dello **stesso parser**, risolti dal workspace con `CollectionResolver`; menu VARS di Run-Matrix |
| D7 | Rref come SimRes con No Solution / Infinitely Many Solutions | 1-33 | M | come il manuale |
| D8 | Errore senza coefficienti o senza soluzioni | 1-33 | M | "Ma ERROR" |
| D9 | Coefficienti e soluzioni del sistema non richiamabili insieme | 1-33 | M | stessa regola (una sola espressione non può usare SimRes e SimCoef) |

### 4.6 Messaggi

| ID | Messaggio | Pag. | v1.11.0 | v1.12.0 |
|---|---|---|---|---|
| M1 | Syntax ERROR | α-1 | P | — |
| M2 | Ma ERROR | α-1 | D ("Math ERROR") | i messaggi della modalità iniziano con "Ma ERROR" |
| M3 | Stack ERROR | α-1 | P | — |
| M4 | Non-Real ERROR (coefficiente non reale nel modo Real) | α-2 | P | il modo Real accetta coefficienti con `i` come Run-Matrix (calcolati in complesso) |
| M5 | Can't Solve! Adjust initial value or bounds. Then try again | α-2 | M | V12 |
| M6 | Time Out | α-2 | D | V13 |
| M7 | Underflow | α-2 | M | P15 |
| M8 | Infinitely Many Solutions / No Solution / No Variable | α-3 | D / M | S15, S16, V14 |
| M9 | Retry | 4-5 | M | V11 |

### 4.7 Estensioni (non presenti sulla fx-CG50, mantenute)

| ID | Funzione | Stato |
|---|---|---|
| X1 | Disequazioni polinomiali di grado 1…6 (`>`, `<`, `≥`, `≤`) con insieme delle soluzioni | mantenuta (F4 INEQ nel menu, segnata come estensione) |
| X2 | API `solveInequalitySystem` (sistemi di disequazioni, `math_engine.hpp`) | interfaccia di libreria, documentata come estensione |

## 5. Differenze inevitabili e scelte

| Differenza | Motivo | Scelta |
|---|---|---|
| Schermo 320×320 invece di 396×224 | hardware della PicoCalc | griglia di 4 colonne visibili con scorrimento ◀ ▶ (come l'editor MAT/VCT), valore della cella in basso a destra su una riga dedicata |
| Nessun tasto dedicato a frazione, √, x², π, S⇔D | tastiera della PicoCalc | durante l'immissione F1 frazione (SHIFT: mista), F2 √ (SHIFT: x²), F3 potenza (SHIFT: π), F4 MATH, come in Run-Matrix; S⇔D su F5 come in Run-Matrix |
| Intestazione per 4–6 incognite | il manuale mostra solo 2 e 3 incognite | `X1…X6` (`anX1+bnX2+…`), come le soluzioni `X1…` di POLY |
| Messaggio di POLY senza radici reali nel modo Real | il manuale non lo mostra | "No Solution" (messaggio della tabella α-3 della modalità) |
| RECALL / DELETE del Solver | il manuale mostra i tasti ma non li descrive | RECALL rimette i valori che le variabili avevano prima dell'ultimo SOLVE; DELETE cancella l'equazione (riga Eq) o azzera la variabile selezionata |
| Variabili r e θ del Solver | nel firmware `r` è la memoria R (Run-Matrix non ha una variabile r separata) e θ non è una memoria | variabili A–Z (x = X); θ non ammesso nel Solver ("No Variable" se è l'unica) |
| Interruzione del Solve con AC | il calcolo della PicoCalc è limitato (iterazioni e tempo) e termina sempre | "Time Out" alla fine del limite |
| Strip eActivity (Simul/Poly Equation, Solver), comandi di programma `Sim Coef`… | eActivity e Program non sono realizzati | fuori dal perimetro (modalità pianificate) |
| Cifre mostrate nelle celle | larghezza delle colonne | valore troncato con "…" nella cella, completo in basso (come la fx-CG50) |

## 6. Vincoli tecnici rispettati dal piano

- Un solo parser, AST e valutatore: i coefficienti sono espressioni lette da
  `parseExpression` e valutate da `evaluateAny`/`evaluateExact`; i nomi
  SimRes… sono token dello stesso lexer; nessun parser separato.
- Dati solo nel workspace: `EquationState` (record **v8**), letto da Run-Matrix
  solo tramite `CollectionResolver` (il test di architettura vieta
  `ws.equation` fuori da Equation e dal workspace).
- Memoria: coefficienti e soluzioni sull'heap (vettori), nessuna matrice di
  lavoro sullo stack; limiti di dimensione in `limits.hpp` (uguali sulle due
  schede: 6 incognite e grado 6 come la fx-CG50).
- Niente CAS: forme esatte solo dove la fx-CG50 le dà (razionali, radicali
  quadratici, π, complessi con queste parti).
