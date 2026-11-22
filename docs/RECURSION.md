# Recursion — guida breve (v1.17.0, modalità Base)

Solo **Pimoroni Pico Plus 2 W**. MENU › **8**. Funzioni minime delle sezioni
5-45…5-49 della fx-CG50. Stato **Base**.

- F1 **TYPE**: an+1 = f(n, an) oppure an+2 = f(n, an, an+1).
- F2 **EDIT** (o EXE): la formula con l'editor naturale; F1 inserisce **n**,
  F2 **an**, F3 **an+1** (solo per an+2). La formula an+1 è la stessa di VARS ›
  RECURSION di Run-Matrix (DrawR, DrawWeb).
- F5 **SET** (o SHIFT MENU): Start, End (0…999, interi), a0, a1, colonna Σan.
- F6 **TABLE**: tabella da n = Start a End calcolata **a passi** (AC = Break),
  con **Σan**; F5 **G-PLT** disegna i termini (n, an), F6 **WEB** il diagramma
  a ragnatela (solo an+1) con y = f(x) e y = x, sulla V-Window di Graph.
- Errori con il termine: «Math ERROR … (n=5)»; termini oltre 1E100: Math ERROR.
- F3 FILE: OPEN, SAVE·AS (`.pcrecur` in `PICOCALC/RECUR`: tipo, formule an+1 e an+2, Start/End, a0/a1 e Σan).

Mancano per la parità: bn e cn, il tipo an = f(n), il grafico di fase, la
convergenza e divergenza della ragnatela.
