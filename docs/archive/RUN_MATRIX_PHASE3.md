# Run-Matrix — fase 3: modelli VPAM per il calcolo

## Incremento

- `NaturalNodeKind::Calculus` introduce nodi strutturati per integrale, derivata e sommatoria.
- `NaturalEditor::insertIntegral()`, `insertDerivative()` e `insertSummation()` creano modelli con righe di argomento indipendenti. Il cursore può passare tra le righe con SU/GIU; all'interno dell'integrando restano disponibili i modelli annidati, ad esempio le potenze.
- La serializzazione collega i modelli al parser: `integral(f,a,b)`, `derivative(f,x)` e `sum(f,a,b)`.
- `sum(f,a,b)` valuta la somma discreta inclusiva per indici interi, anche con limiti decrescenti; sono rifiutati limiti non interi e intervalli oltre 10001 termini.
- Il renderer naturale mostra ∫, d/dx e Σ. Le forme ASCII restano digitabili dalla tastiera PicoCalc e sono convertite nella notazione matematica in visualizzazione.

## Uso

- Integrale: `integral(x^2,0,3)` → 9.
- Derivata numerica: `derivative(x^2,3)` → circa 6.
- Sommatoria: `sum(x^2,1,3)` → 14.

I tre metodi `insert...()` espongono modelli VPAM modificabili al codice UI/controller. In questa fase non sono ancora assegnati a tasti fisici dedicati: dalla tastiera si possono comunque digitare le forme ASCII complete, che il parser valuta e il renderer converte. L'integrale e la derivata restano metodi numerici; non sono calcolo simbolico.

## Verifica

Eseguire `bash tests/run_host_tests.sh`. I test controllano la navigazione tra campi, la serializzazione, il rendering dei simboli e i risultati del parser. La suite host non equivale a una build UF2 né a un test sul PicoCalc fisico.
