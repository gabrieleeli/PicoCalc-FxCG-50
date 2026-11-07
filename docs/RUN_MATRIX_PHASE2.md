# Run-Matrix — fase 2: input da tastiera e notazione

Questa fase usa il manuale fx-CG50 v3.50 come riferimento per l'input naturale. Il manuale descrive frazioni, potenze, quadrati, reciproci, radici quadrate/cubiche e radici di indice, logaritmi, valore assoluto, derivate, integrali, sommatorie, matrici e vettori nel modo Math input/output. Descrive inoltre una cronologia di 30 espressioni e risultati.

## Implementato in questa fase

- La tastiera PicoCalc continua a inviare caratteri ASCII e tasti funzione; non richiede i tasti dedicati Casio.
- `sqrt(3)` e `cbrt(27)` diventano modelli naturali di radice mentre si digita.
- `rootN(x)` e la forma `rootN(x)` con N da 2 a 99 vengono riconosciute; ad esempio `root3(27)` diventa un nodo radice cubica. La parentesi aperta/chiusa opzionale è gestita dal modello, non lasciata come testo superfluo.
- `^` continua a creare un esponente strutturato; sono accettate parentesi e segni nell'esponente, ad esempio `x^(-1)`.
- Le normali parentesi di funzione sono ora accettate dal NaturalEditor: `sin(30)`, `log(100)` e le altre chiamate del parser si possono digitare dalla tastiera.
- Il parser riconosce anche le forme testuali `root3(27)` e `root5(32)`, non soltanto il formato interno `root(3,27)`.
- Aggiunte le funzioni numeriche `integral(f(x),a,b)` (Simpson composito) e `derivative(f(x),x)` (differenza centrale). Sono funzioni testuali: non sono ancora modelli grafici VPAM dedicati come quelli del menu MATH Casio.
- Il formatter visualizza `integral` come ∫ e `derivative` come d/dx.
- La cronologia Run-Matrix è limitata a 30 risultati, come documentato dal manuale.
- La guida simboli è stata ampliata a 16 pagine e tradotta in italiano: aritmetica, potenze/radici, funzioni, calcolo, combinatoria, statistica, grafici, matrici/vettori, unità e formati di visualizzazione. Ogni voce ha una spiegazione breve e un esempio.

## Esempi verificati

| Input da tastiera | Struttura / risultato atteso |
|---|---|
| `sqrt(3)` | Radice quadrata naturale; valore √3 |
| `root3(27)` | Radice cubica naturale; valore 3 |
| `root5(32)` | Radice quinta; valore 2 |
| `2^3` | 2 con esponente 3; valore 8 |
| `x^(-1)` | Potenza con esponente negativo |
| `sin(30)` in DEG | 0,5 |
| `integral(x^2,0,3)` | 9 |
| `derivative(x^2,3)` | circa 6 |

## Limiti ancora aperti

- Derivate e integrali sono ancora funzioni testuali, non strutture grafiche con cursore e limiti separati come sul display Casio.
- Le sommatorie/produttorie, le matrici/vettori in input naturale, i numeri misti, il catalogo comandi e la gestione completa di SHIFT/ALPHA non sono ancora completati.
- Undo/Redo, copia/taglia/incolla e modifica diretta delle voci di cronologia restano attività successive.
- I test sono host C++: non certificano ancora compilazione firmware, mappatura di ogni tasto o resa sul display PicoCalc reale.

## Test

Eseguire `bash tests/run_host_tests.sh`. La suite verifica gli alias radice, parentesi, funzioni, integrale, derivata, cronologia da 30 voci e regressioni grafiche/editor.
