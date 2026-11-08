# Punto A — Core matematico Run-Matrix (release 1.2)

Questa release amplia il motore indipendente del PicoCalc. Non contiene codice, dump, asset o firmware Casio. Il codice resta C++17 compatibile con la base Pico SDK esistente.

## Nuovi moduli

- `include/point_a.hpp`, `src/point_a.cpp`: API aggiuntive per il punto A.
- `RunMatrixSession`: esecuzione delle espressioni con `CalculatorMemory`, `Ans`/`PreAns`, variabili A-Z e cronologia limitata agli ultimi 64 calcoli riusciti.
- Matrici: sottrazione, prodotto per scalare, trasposta, traccia, RREF e soluzione di sistemi quadrati non singolari. Le primitive preesistenti continuano a fornire somma, prodotto, determinante e inversa.
- Vettori: sottrazione, prodotto per scalare e angolo (rispetta DEG/RAD/GRA), oltre a somma, prodotto scalare, prodotto vettoriale 3D e norma già presenti.
- Frazioni: sottrazione, divisione, approssimazione razionale di un decimale e formattazione impropria/mista. Il core preesistente è stato corretto per rilevare overflow nelle operazioni razionali e per non applicare `abs()` al valore `INT64_MIN`.
- Complessi: somma, sottrazione, prodotto, divisione, esponenziale, logaritmo, seno, coseno, tangente, argomento e formattazione `a±bi`; potenza e radice quadrata erano già presenti.
- Base-N: conversione signed con segno esplicito e operazioni bitwise AND/OR/XOR/NOT e shift a larghezza configurabile.
- Calcolo numerico: derivata a 5 punti, integrale Simpson, ricerca di radici su intervallo e SOLVE di Newton, tutti con espressioni del parser esistente.
- Conversioni: lunghezza, area, volume, tempo, velocità, massa, pressione, energia, potenza e temperatura. Prefissi SI e costanti fisiche comuni.
- Presentazione: formato testuale di espressione con simboli × e ÷, formato complesso, frazionario e notazione ingegneristica.

## Integrazione build

`src/point_a.cpp` è incluso nel firmware Pico SDK e nel target desktop. La suite host compila il modulo e verifica i nuovi percorsi.

## Verifica effettuata

Comando:

```sh
./tests/run_host_tests.sh
```

Esito verificato durante la generazione della release:

```text
PicoCalc FX AI host tests: PASS
```

I test comprendono casi per matrici/RREF/sistemi, angoli vettoriali, frazioni, complessi, numeri negativi in Base-N, operazioni bitwise, derivata, integrale, radici, SOLVE, conversioni di temperatura/unità, costanti e prefissi.

## Limiti non risolti: non dichiarare il punto A completamente finito

Questa release completa una parte sostanziale del **backend matematico**, ma non equivale ancora all'applicazione Run-Matrix della fx-CG50. Restano da implementare e verificare:

1. Editor Natural V.P.A.M. realmente bidimensionale e modificabile (frazioni impilate, radicali, matrici, integrali e cursore strutturato). L'attuale `formatNatural()` è una conversione testuale e non un renderer 2D.
2. Collegamento dei nuovi servizi a menu, eventi e schermate PicoCalc. Il launcher attuale è dimostrativo e non offre ancora flussi completi per ciascuna operazione.
3. Integrazione dei numeri complessi nel parser e nell'editor: il parser attuale restituisce `double`; le API complesse sono un servizio separato.
4. Modalità matrici completa e persistente: l'editor esistente gestisce quattro slot da massimo 4×4; non è ancora la gestione dati/UI completa Casio.
5. Sistemi rettangolari e descrizione parametrica di sistemi indeterminati nell'interfaccia; l'API `solve()` risolve sistemi quadrati con soluzione unica.
6. SOLVE numerico robusto con intervallo/bracketing, gestione esplicita delle radici multiple e diagnostica di convergenza. Newton può non convergere a seconda del valore iniziale; la scansione radici può non rilevare una radice tangente che non cambia segno.
7. Formati esatti e conversioni automatiche fra decimale e frazione equivalenti al comportamento Casio; l'approssimazione razionale è limitata dal denominatore massimo scelto.
8. Catalogo completo di costanti, unità, simboli tecnici e comandi; il set implementato è quello elencato in questo documento, non l'intero catalogo fx-CG50.
9. Test differenziali contro la calcolatrice originale per ogni funzione, precisione, errore e comportamento limite.
10. Test di compilazione firmware ARM e prova sul PicoCalc fisico: i test host non attestano prestazioni, memoria o funzionamento hardware.

**Stato corretto:** backend matematico ampliato e test host superati; parità Run-Matrix e Natural V.P.A.M. ancora in corso.

## v1.3 — editor strutturato Natural V.P.A.M. (incremento host-tested)

Aggiunto `include/natural_editor.hpp` e `src/natural_editor.cpp` con un modello ad albero editabile per righe, token, frazioni, radicali, potenze, indici e gruppi. L'editor espone eventi logici per caratteri, frecce, Backspace, Delete ed Enter; le frecce verticali passano tra numeratore/denominatore ed esponente/base. Il layout produce primitive 2D di testo, linee di frazione/radicale e cursore. `ui::drawNaturalEditor()` collega tali primitive all'astrazione grafica `board::*`.

`toExpression()` serializza frazioni, radicali, potenze e gruppi in sintassi accettata dal parser numerico attuale; test di round-trip valutativo inclusi. Gli indici sono rappresentabili e impaginati, ma la serializzazione valutabile è deliberatamente rifiutata: il parser esistente non implementa variabili indicizzate.

Verifiche eseguite:
- `./tests/run_host_tests.sh`: PASS con `-std=c++17 -O2 -Wall -Wextra -Werror`.
- Suite host ricompilata ed eseguita con AddressSanitizer e UndefinedBehaviorSanitizer: PASS.
- `src/ui.cpp` compilato isolatamente con `-Wall -Wextra -Werror`: PASS.

Limiti ancora aperti: gli eventi logici non sono ancora collegati ai codici raw del controller I2C della tastiera PicoCalc; il protocollo/tastiera reale deve essere decodificato e validato sulla revisione hardware effettiva. Il renderer non ha ancora scrolling, wrapping, selezione, undo/redo, font matematico completo o test sul display. Non è stata eseguita build Pico SDK/UF2 né prova su hardware. Questo incremento non completa il punto A né dimostra parità completa fx-CG50.
