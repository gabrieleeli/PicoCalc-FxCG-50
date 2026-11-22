# Spreadsheet — guida breve (v1.17.0, modalità Base)

Solo **Pimoroni Pico Plus 2 W**. MENU › **4**. Funzioni minime del capitolo 9
della fx-CG50 (il manuale descrive il comportamento; nessun codice o testo
copiato). Stato **Base**: usabile, parità non completa (vedi in fondo).

## Griglia e celle
- Foglio **A–Z × 999**; frecce, PageUp/PageDown, Home (A1); il cursore è il
  riquadro blu, la riga in basso mostra il riferimento e il testo della cella.
- Si digita direttamente: un **numero** (`12`, `-3E2`), un **testo** (qualsiasi
  cosa che non sia un numero) o una **formula** che inizia con `=`.
- EXE conferma e scende; ↑/↓ confermano e si spostano; EXIT annulla; EXE su una
  cella piena la modifica; DEL la svuota.
- Formule: espressioni della calcolatrice (stesso parser e valutatore di
  Run-Matrix) con **riferimenti** `A1`, **assoluti** `$A$1`, `A$1`, `$A1`,
  **intervalli** `A1:A10` con `Sum`, `Mean`, `Min`, `Max`, `Median` (e le altre
  funzioni di lista), variabili A–Z e liste.
- **Ricalcolo automatico** a passi (AC = Break) dopo ogni modifica; un ciclo dà
  **Circular ERROR** (cella «Circ», l'errore completo nella riga in basso);
  le altre celle con errore mostrano «ERR». SHIFT MENU (SET UP) alterna
  Auto Calc On/Off; F5 CALC ricalcola a mano.

## Menu F1–F6
| Tasto | Voce |
|---|---|
| F1 FILE | NEW, OPEN, SAVE·AS (file `.pcsheet` in `PICOCALC/SHEET`) |
| F2 EDIT | CUT, COPY, PASTE (i riferimenti relativi si spostano), FILL (il testo della cella d'ancora in tutto il blocco), SELECT (blocco con le frecce) |
| F3 DELETE | svuota la cella o, con conferma, il blocco selezionato |
| F4 LIST | COL→L (i numeri della colonna in List n), L→COL (List n nella colonna, dalla cella) |
| F5 CALC | ricalcolo manuale |
| F6 JUMP | va a una cella (A1…Z999) |

Limiti: 26 000 celle con la PSRAM, 3 000 solo in SRAM («Memory ERROR»); 255
caratteri per cella.

## Mancano per la parità (versioni successive)
SEQ, SORT, CellIf/CellProd e gli altri comandi Cell, formattazione
condizionale, GRAPH e CALC dalle celle, CSV, memorie verso matrici.
