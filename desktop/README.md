# Emulatore desktop PicoCalc + interfaccia ispirata a Casio fx-CG50

Questa è una **simulazione software nativa per Linux**, non un emulatore della ROM Casio e non riproduce il firmware proprietario. La finestra mostra un corpo/tastiera in stile PicoCalc e un LCD 320×320 con impaginazione e palette ispirate alla fx-CG50. Il modo `Natural V.P.A.M.` usa il formatter matematico naturale già presente nel progetto; non è ancora un renderer 2D completo di frazioni e radicali come quello originale Casio.

## Architettura
- SDL2 + SDL2_ttf per finestra, disegno e input.
- Riutilizza direttamente il motore C++ in `src/` (parser, grafici 2D, statistiche, matrici e altre primitive).
- La tastiera fisica PicoCalc è rappresentata a schermo; i tasti del PC vengono usati come input. Non è richiesto hardware PicoCalc.

## Architettura Linux (Arch Linux)

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf sdl2 sdl2_ttf ttf-dejavu
cd PicoCalcFXAI
cmake -S desktop -B build-desktop -G Ninja
cmake --build build-desktop
./build-desktop/picocalc_cg50
```

Se il font non viene trovato, verificare che `ttf-dejavu` sia installato. Per ricompilare dopo modifiche:

```bash
cmake --build build-desktop
```

## Controlli
- Frecce: selezione nel menu.
- Invio: apri app / esegui calcolo.
- F1: menu.
- F2: attiva inserimento testo.
- F3: passa a GRAPH.
- F4: schermata TOOLS.
- F5: cambia DEG → RAD → GRA.
- F6: alterna Natural V.P.A.M. / visualizzazione lineare.
- Backspace: cancella un carattere; Canc: svuota input.
- Esc: torna al menu.

Nelle schermate che accettano dati, premi F2 e scrivi usando la tastiera del computer. In RUN-MAT puoi provare, per esempio: `sqrt(81)+sin(30)`, `log(8,2)`, `root(27,3)`, `5!`, `x^2+2*x+1` (in GRAPH/TABLE).

## Limiti dichiarati
Questa versione rende testabili da desktop le funzioni effettivamente collegate alla UI e quelle esposte dal motore C++. Non tutte le funzioni richieste nel file `funzioni.txt` sono già implementate o raggiungibili da un menu. La matrice `docs/FEATURE_MATRIX.md` distingue le primitive esistenti dalle parti ancora da completare. In particolare, AI richiede un relay HTTPS e chiavi API configurate dall'utente; non viene simulata una connessione funzionante.
