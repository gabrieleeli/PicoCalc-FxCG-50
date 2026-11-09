# TABLE — v1.8.0

Tabelle numeriche delle funzioni della lista delle relazioni, come nella
modalità Table della fx-CG50 (manuale, capitolo 5, pagine 5-32…5-35; il
manuale è usato solo come riferimento di comportamento). Codice:
`include/table_model.hpp`, `src/table_model.cpp` (logica, senza interfaccia),
`src/apps/table_app.cpp` (modalità), `src/apps/graph_plot.cpp`
(GPH-CON/GPH-PLT disegnati da GRAPH).

## Funzioni

Le funzioni Y1…Y20 sono **le stesse di GRAPH** (stessa memoria, stessi colori
e stessa selezione). La lista delle relazioni di TABLE mostra tutte le righe;
la tabella usa solo le funzioni selezionate del **tipo della tabella**
(Table Func):

| Tipo (TYPE) | Variabile | Colonne |
|---|---|---|
| Y= | X | Y1, Y2, … |
| r= | θ | r1, r2, … |
| Param | T | Xt1, Yt1, Xt2, Yt2, … |

Come sulla CG50 non si possono mescolare tipi diversi nella stessa tabella;
le righe di altri tipi (X=, disequazioni, …) sono mostrate in grigio e non
entrano nella tabella. Una riga vuota prende il tipo della tabella.

Tasti della lista: F1 SELECT, F2 DELETE (con conferma), F3 TYPE, F4 STYLE
(colore della colonna e del grafico), F5 SET, F6 TABLE, EXE o → per
modificare la riga (editor naturale: F1 a/b, F2 √, F3 potenza, F4 variabile,
F5 |x|), digitare per sostituirla. SETUP apre il Setup della modalità.

## Variabile (Setup "Variable") e SET

- **Range**: SET → Start, End, Step (Step > 0, Start ≤ End); righe Start,
  Start+Step, … fino a End (tolleranza di arrotondamento: −0.3…0.3 passo 0.1
  dà esattamente 0 al centro).
- **List 1…6**: i valori della lista diventano la variabile.
- Setup di TABLE: Variable (Range, List 1–6), Derivative (On/Off), Angle
  (Deg/Rad/Gra). Derivative e Angle sono condivisi con GRAPH.

Limiti: **999 righe** (Memory ERROR oltre); lista vuota → Dimension ERROR;
Step ≤ 0 o End < Start → Argument ERROR (il modulo SET torna ai valori
precedenti, EXIT esce).

## La tabella

- F6 TABLE genera la tabella; le righe restano (anche dopo lo spegnimento,
  nel workspace in flash) finché non si genera di nuovo o si cancella.
- Celle di **al massimo 6 caratteri** compreso il segno (manuale 5-33), con
  il maggior numero di cifre significative: `-2.625`, `0.3333`, `1.23E6`,
  `1.3E-4`; `ERROR` dove la funzione non è definita. In basso il valore
  completo della cella selezionata nel formato di visualizzazione corrente
  (`dY/dX=` / `dr/dθ=` per le colonne delle derivate).
- Derivative On aggiunge una colonna di derivata per ogni funzione (Y1',
  Yt1' = dY/dX per le parametriche, r1' = dr/dθ); una funzione con
  intervallo `[a,b]` o sovrapposizione `[A=…]` dà errore (manuale 5-34).
- Cursore: ←/→ tra le colonne (anche la colonna della variabile), ↑/↓,
  PgUp/PgDn, Home/End. Si vedono 4 colonne di funzioni e ~12 righe; lo
  scorrimento segue il cursore.

Tasti della tabella:

| Tasto | Funzione |
|---|---|
| F1 FORMULA / EXIT | torna alla lista delle relazioni |
| F2 DELETE | cancella la tabella (con conferma) |
| F3 ROW | DELETE (riga), INSERT (copia della riga sopra di essa), ADD (copia sotto); poi EDIT cambia il valore |
| F4 EDIT / EXE | nuovo valore della variabile della riga (anche espressioni: `pi/4`); un valore non valido (1/0) dà errore e lascia il valore originale |
| cifra, `-`, `.`, `(` | come EDIT, con il primo carattere digitato |
| F5 GPH-CON / F6 GPH-PLT | GRAPH disegna i punti della tabella collegati / isolati (Y=: (x, y); r=: polare; Param: (Xt, Yt)); G⇔T o EXIT torna alla tabella |
| OPTN → LISTMEM | copia la colonna del cursore (anche la variabile) in List 1–6; al massimo 255 valori (limite delle liste), una cella non definita dà errore |

Le colonne delle funzioni non si modificano direttamente (manuale 5-34).

## Persistenza

- Workspace in flash, **versione 4**: variabile, lista, tipo, righe generate
  (anche modificate) e stato della tabella; i record v1–v3 sono letti (la
  tabella va generata di nuovo).
- Progetto `.pcgraph`: `table=Start;End;Step`, `tablevar=Range|List1…6`,
  `tablefunc=Y=|r=|Param` (chiavi aggiunte in modo compatibile). Le righe
  non fanno parte del progetto e non lo marcano come modificato.

## Memoria e prestazioni

Le righe occupano 8 byte ciascuna (≤ 8 KB); le celle non sono memorizzate:
sono calcolate per le righe visibili a ogni ridisegno (al più ~50 celle; le
derivate usano Ridders). Il motore è ricompilato solo quando cambiano
funzioni, variabili A–Z, liste, angolo, Derivative o tipo (impronta FNV-1a).
Tempi sul RP2350 da misurare (vedi `docs/HARDWARE_TEST_v1.8.0.md`, HW-34).

## Differenze dichiarate rispetto alla CG50

- INSERT/ADD inseriscono una copia della riga (il valore si cambia con
  EDIT); la CG50 non documenta il valore della nuova riga.
- Liste di 999 elementi come la CG50 sul Pimoroni Pico Plus 2 W; sul Pico 2 W
  255 (record del workspace da 64 KB): lì LISTMEM di tabelle più lunghe dà
  Dimension ERROR.
- Con una sovrapposizione `[A=…]` senza Derivative la colonna usa il primo
  valore di A.
- Dual Screen (T+G) e Graph to Table non sono ancora implementati (v1.9.0).

## Test

`table_test` (171 controlli): righe (intervalli positivi, negativi, misti,
limiti), celle di 6 caratteri, valori contro formule chiuse (polinomio,
seno in Rad e Deg, esponenziale, logaritmo, razionale con polo, radice),
derivate, r= e Param, flussi da tastiera (lista, SET, TABLE, modifica della
funzione e aggiornamento, scorrimento, EDIT, ROW, LISTMEM, GPH-CON, DELETE,
Variable = List, Setup, STYLE, SELECT, 999 righe), persistenza v4 e lettura
v3, chiavi `.pcgraph`. Screenshot `32`, `35`–`37`.
