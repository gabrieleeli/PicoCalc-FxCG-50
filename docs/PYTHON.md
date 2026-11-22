# Python — guida breve (v1.17.0, modalità Base)

Solo **Pimoroni Pico Plus 2 W**. MENU › **L**. **MicroPython v1.24.1** (porta
*embed*, licenza MIT, `third_party/micropython`): Python vero, scelto
dall'utente nella v1.17.0. Stato **Base**.

- Elenco degli script `.py` in `PICOCALC/PY`: F1 **RUN**, F2 **OPEN**
  (editor), F3 **NEW**, F4 **DEL**, F5 **SHELL**.
- **SHELL** (REPL): `>>>` e `...` per i blocchi (riga vuota per eseguire), ↑/↓
  la cronologia (20 righe, conservata), le variabili restano finché si resta
  nella modalità.
- Moduli: **math**, **sys** (`sys.exit`), **gc** e **casioplot** minimo
  (`set_pixel`, `get_pixel`, `draw_string`, `clear_screen`, `show_screen`): il
  disegno compare con `show_screen()` o alla fine dello script; EXIT torna.
- `input()` sospende lo script finché si preme EXE sulla riga «?».
- **AC = KeyboardInterrupt** (anche durante `input()`); errori con il
  traceback e il numero di riga; ricorsione troppo profonda: RuntimeError.
- **Heap separato**: 512 KB nella PSRAM (48 KB in SRAM senza PSRAM),
  `MemoryError` quando finisce; heap e pila si liberano uscendo dalla modalità.
- L'interprete gira **a passi** su una pila propria (coroutine): la tastiera e
  il display restano vivi durante uno script lungo.

Mancano per la parità: menu delle funzioni, CATALOG, debug degli script,
moduli random, turtle, matplotl.
