# Natural V.P.A.M. editor — v1.3

## API

- `fx::NaturalEditor::insert(char)` inserisce un carattere ammesso.
- `insertFraction()`, `insertRadical()`, `insertPower()`, `insertSubscript()`, `insertGroup()` creano strutture annidate.
- `handle(NaturalKeyEvent)` accetta eventi logici `Left`, `Right`, `Up`, `Down`, `Backspace`, `Delete`, `Enter`, `Character`.
- `toExpression()` produce sintassi per il parser corrente; per indici lancia `std::runtime_error` perché gli identificatori indicizzati non sono supportati dal parser.
- `layout(x, baseline, maxWidth)` restituisce primitive `Text`, `Line`, `Cursor` per il renderer.
- `ui::drawNaturalEditor(editor, message)` esegue il disegno attraverso `board::*`.

## Test

Dalla root del progetto:

```sh
./tests/run_host_tests.sh
```

Casi inclusi: `1/3`, `sqrt(9)`, `2^3`, gruppi, annidamento in frazione, navigazione verticale, input e backspace, layout e cursore, rifiuto esplicito della valutazione di un pedice.

## Limiti

Gli eventi sono astratti: non interpretano ancora il valore raw a 16 bit restituito da `board::readKey()`. Non è corretto inventare la mappatura senza verificare il firmware del controller tastiera sul PicoCalc in uso. La schermata è disegnabile ma non ancora selezionabile dal launcher/main loop. Il layout attuale non implementa scrolling/wrapping, selezione, clipboard, undo/redo o un font matematico completo.
