# Tastiera PicoCalc e Run-Matrix — v1.4

## Protocollo verificato nel sorgente ClockworkPi

Il firmware STM32 ufficiale della tastiera espone il dispositivo come slave I²C `0x1F`. Il firmware host scrive il registro FIFO `0x09` e legge due byte. Nel sorgente ClockworkPi `receiveEvent()` restituisce prima `fifo_item.state` e poi `fifo_item.key`; `keyboard.h` definisce gli stati `IDLE=0`, `PRESSED=1`, `HOLD=2`, `RELEASED=3` e i codici dei tasti.

`board::readKey()` mantiene la convenzione del progetto e restituisce `(state << 8) | key`. La nuova `fx::decodePicoCalcKey()` interpreta esattamente questa forma. Gli eventi `PRESSED` e `HOLD` generano input; `IDLE`, `RELEASED` e codici non riconosciuti sono ignorati.

Fonti primarie: [firmware FIFO `picocalc_keyboard.ino`](https://github.com/clockworkpi/PicoCalc/blob/master/Code/picocalc_keyboard/picocalc_keyboard.ino) (`REG_ID_FIF`) e [definizioni `keyboard.h`](https://github.com/clockworkpi/PicoCalc/blob/master/Code/picocalc_keyboard/keyboard.h).

## Mappatura applicativa scelta

| Codice controller | Tasto fisico | Azione Run-Matrix |
|---:|---|---|
| `0xB4` | Freccia sinistra | Cursore a sinistra |
| `0xB7` | Freccia destra | Cursore a destra |
| `0xB5` | Freccia su | Riga/struttura superiore |
| `0xB6` | Freccia giù | Riga/struttura inferiore |
| `0x08` | Backspace | Cancella prima |
| `0xD4` | Delete | Cancella dopo |
| `0x0A` | Enter | Valuta l'espressione |
| `0xB1` | Esc | Torna al launcher |
| `0x81` / F1 | F1 | Apre Run-Matrix dal launcher; inserisce frazione nell'editor |
| `0x82` / F2 | F2 | Inserisce radicale |
| `0x83` / F3 | F3 | Crea potenza sull'operando precedente |
| `0x84` / F4 | F4 | Crea pedice sull'operando precedente |
| `0x85` / F5 | F5 | Inserisce gruppo |
| `0x86` / F6 | F6 | Pulisce l'espressione |
| ASCII stampabile | Tasti alfanumerici/operatori | Inserisce il carattere |

La semantica F1–F6 è una scelta dell'applicazione PicoCalc FXAI, non una funzione incorporata nel controller. I codici grezzi provengono invece dalle definizioni ClockworkPi.

## Flusso applicativo

- Il launcher viene mostrato all'avvio.
- F1 o Enter apre Run-Matrix.
- I tasti vengono decodificati e inviati al `RunMatrixController`.
- Le operazioni strutturali chiamano l'API già esistente di `NaturalEditor`.
- Enter serializza l'albero con `toExpression()` e invoca `pointa::RunMatrixSession::execute()`.
- Il risultato viene formattato con 10 cifre significative; eccezioni di parsing/valutazione e strutture incomplete sono mostrate come messaggio, senza terminare il programma.
- Esc torna al launcher; F6 azzera l'editor.

## Test riproducibili

```sh
./tests/run_host_tests.sh
```

I test includono stati FIFO press/hold/release, codici freccia/Enter/Esc/Backspace/Delete/F1–F6, caratteri ASCII, apertura/uscita dal workspace, valutazione di `1+2`, frazione `1/3`, radicale `sqrt(9)`, potenza `2^3`, gruppo `(2+3)` e rifiuto gestito del pedice non valutabile.

## Limiti non verificati

- Nessuna prova è stata eseguita su una tastiera PicoCalc fisica: il mapping è coperto da sorgente firmware e test host, non da un test elettrico sul dispositivo.
- Nessuna build Pico SDK/ARM o UF2: `PICO_SDK_PATH` e `arm-none-eabi-g++` non sono disponibili nell'ambiente.
- Il renderer non implementa ancora scrolling/wrapping dell'espressione, undo/redo, selezione o clipboard.
- Il protocollo ufficiale usa eventi di stato; i modificatori e i layout alternativi non sono ancora esposti come funzioni di editing dedicate.
- Il driver LCD è presente nel sorgente, ma orientamento, colori, temporizzazione e leggibilità devono essere validati sul display reale.
