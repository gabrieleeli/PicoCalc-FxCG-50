# Dyna Graph — guida breve (v1.17.0, modalità Base)

Solo **Pimoroni Pico Plus 2 W**. MENU › **6**. Funzioni minime della sezione
5-42 della fx-CG50 (descrizione del comportamento). Stato **Base**.

- Modulo: **Y=** (funzione di X con un coefficiente), **Dynamic Var** (A–Z,
  ◀ ▶), **Start**, **End**, **Pitch** (le variabili di VARS › DYNA di
  Run-Matrix), **Dynamic Type** (Cont = ciclo continuo avanti e indietro, Stop
  = una passata), **Speed** (Slow, Normal, Fast).
- F1 **DYNA**: animazione sulla **V-Window di Graph** (F2 V-WIN la modifica);
  ogni fotogramma è campionato a passi (la tastiera resta pronta); in alto il
  valore del coefficiente. ←/→ o F1/F2 cambiano la velocità; **AC** ferma, EXE
  riprende, EXIT torna al modulo.
- Al più **100 fotogrammi** (Range ERROR); Pitch nullo o di segno sbagliato:
  Argument ERROR. Il coefficiente è legato solo nel calcolo: la variabile A–Z
  della memoria non cambia.
- F5 FILE: OPEN, SAVE·AS (`.pcdyna` in `PICOCALC/DYNA`: funzione, coefficiente, tipo, velocità e Start/End/Pitch).
- Striscia **Dynamic Graph** di eActivity: si apre sull'animazione.

Mancano per la parità: funzioni BUILT-IN, Locus, Dynamic Graph Memory, tabella
da formula.
