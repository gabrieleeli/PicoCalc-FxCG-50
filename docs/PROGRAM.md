# Program — guida breve (v1.17.0, modalità Base)

Solo **Pimoroni Pico Plus 2 W**. MENU › **B**. Linguaggi scelti dall'utente
(v1.15.0, confermati nella v1.17.0): **sottoinsiemi interpretati di C, C++ e
C#** (interprete scritto da zero, `include/program.hpp`). Il capitolo 8 della
fx-CG50 descrive solo la modalità (elenco, RUN, errori). Stato **Base**.

- Elenco dei programmi in `PICOCALC/PROG`: F1 **EXE** (RUN), F2 **EDIT**,
  F3 **NEW** (nome, programma d'esempio nel dialetto di F5), F4 **DEL**
  (conferma), F5 **LANG** (dialetto dei programmi nuovi).
- File **`.pcprog`**: testo UTF-8 con la prima riga `// pcprog: C|C++|C#`
  (nascosta nell'editor). I `.c`, `.cpp`, `.cs` copiati dal PC si aprono e si
  salvano come testo.
- Editor con **evidenziazione minima** (parole chiave, commenti, stringhe,
  numeri, direttive): F1 RUN (salva ed esegue), F2 SAVE, F3 LANG; EXIT salva.
- Esecuzione **a passi**: output sul display, **input** dalla tastiera (riga
  con «?»), **AC = Break**; comandi grafici (`fx::plot`, `fx::line`,
  `fx::circle`, `fx::graphY`, …) eseguiti come i comandi di Run-Matrix e
  disegnati da **Graph** (F6 GRAPH); variabili A–Z (`fx::getVar`,
  `fx::setVar`), **liste** (`fx::getList(n, i)`, `fx::setList(n, i, v)`,
  `fx::listSize(n)`; in C# `Fx.…`, in C `fx_…`), `fx::calc("espressione")`.
- **Errori con riga e colonna**: la finestra d'errore, poi l'editor con il
  cursore sull'errore (riga evidenziata).

Mancano per la parità della modalità: DEBUG passo passo, SEARCH, RENAME,
password, la libreria `fx` completa (statistiche, finanza).
