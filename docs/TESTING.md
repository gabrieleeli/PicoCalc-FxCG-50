# Test — v1.7.0

## Cosa viene provato e dove

| Livello | Strumento | Stato |
|---|---|---|
| Logica portabile (motore, editor, app, storage, protocollo AI) | test host C++ (CTest) | eseguito: 17 suite, 5347 controlli |
| Architettura (registro ↔ sorgenti ↔ firmware) | `architecture_test` | eseguito |
| Stessi test con AddressSanitizer + UBSan | `--sanitize` | eseguito |
| Interfaccia | simulatore host + screenshot PNG | eseguito (`docs/screenshots/`, Run-Matrix 12–19) |
| Stack sul target | `arm-none-eabi-g++ -fstack-usage` sui sorgenti del motore | eseguito (vedi MATH_ENGINE.md) |
| Relay AI | `unittest` Python con SDK ufficiali e API simulate | eseguito: 15 test |
| Compilazione firmware RP2350 | Pico SDK 2.2.0 | eseguita, `picotool info` verificato |
| Hardware PicoCalc | — | **non eseguito** |

Nessuna funzione è dichiarata *TESTED ON HARDWARE*: i test host provano la logica
compilata dallo stesso sorgente del firmware, ma non display, tastiera I2C, flash
reale, Wi-Fi o TLS sul Pico.

## Comandi

```bash
tests/run_host_tests.sh              # build/host, tutte le suite
tests/run_host_tests.sh --sanitize   # build/host-asan con ASan + UBSan
ctest --test-dir build/host --output-on-failure -R expression   # una sola suite

cd relay && python -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python -m unittest discover -s tests -v
```

I test usano macro proprie (`tests/test_util.hpp`: `CHECK`, `CHECK_NEAR`,
`CHECK_THROWS_AS`, …) che restano attive anche con `NDEBUG` e riportano ogni
errore, non solo il primo.

## Suite host

| Suite | Controlli | Contenuto |
|---|---|---|
| expression_test | 154 | priorità Casio, funzioni, complessi, formati e arrotondamento come la calcolatrice, limiti di lunghezza/annidamento, budget di stack |
| exact_test | 152 | razionali (overflow, confronto esatto), radicali, π, letterali decimali e periodici, sviluppo periodico, limiti di visualizzazione CG50, valutatore esatto (trigonometria esatta, radici, log, n!, nCr, relazioni, Σ/Π, memoria esatta, catene lunghe, budget di stack), coerenza esatto/virgola mobile |
| run_matrix_test | 179 | esempi frazione ⇔ decimale, frazioni miste, periodo, input decimale, irrazionali mai convertiti, radicali e π (esempi del manuale), Ans/A–Z esatti, →A~F, variabili condivise, Int÷/Rmdr/MOD_Exp/logica, modifica della cronologia con ricalcolo, alberi 2D, persistenza v2 e lettura dei record v1.6.0, dati corrotti, flussi completi da tastiera attraverso il `Firmware` |
| architecture_test | 468 | registro coerente, controller registrati = modalità disponibili, schermate "non disponibile", include/sezioni del workspace/collegamenti dichiarati, separazione di Run-Matrix, stratificazione motore/UI, nessun ciclo di include, modelli dati delle modalità future |
| editor_test | 179 | modelli 2D (anche frazione mista, periodo, radice con indice), navigazione, undo/redo, copia/incolla, layout e cursore, limiti |
| math_engine_test | 108 | matrici, vettori, basi, sessagesimale, frazioni, calcolo numerico |
| storage_test | 733 | serializzazione del workspace (formato v2), slot A/B, CRC, interruzione di alimentazione in ogni punto della scrittura |
| app_test | 62 | AppManager, tasti globali, errori, barra di stato, TileDiff, rendering delle schermate |
| apps_flow_test | 54 | percorsi completi: Statistics → Graph, Graph ↔ Table, Equation, System, Memory, Conversion |
| solvers_test | 60 | polinomi (Aberth), disequazioni, solver numerico |
| statistics_test | 63 | 1/2-Var, regressioni, distribuzioni, test e intervalli (valori di riferimento) |
| ai_test | 138 | JSON, richiesta HTTP, parser della risposta (chunked, errori), app AI Chat con trasporto simulato |
| graphics_smoke | 57 | primitive, clipping, blending, conversione RGB666 |
| font_smoke | 2556 | copertura dei glifi usati, UTF-8, misure |
| animation_smoke | 6 | primitiva di animazione |
| cheatsheet_smoke | 365 | contenuti e navigazione di Symbols (17 pagine) |
| vpam_ast_smoke | 13 | albero condiviso editor/parser |
| **Totale** | **5347** | 0 errori, con e senza sanitizer (ASan + UBSan) |

`run_matrix_test` usa `tests/fixtures/workspace_v1_6_0.inc`, un record salvato
dai sorgenti della v1.6.0 (commit 7dc65cd), per verificare la compatibilità.

## Simulatore

```bash
build/host/picocalc_sim <cartella-output> <script|->
```

Comandi dello script: `type <testo>`, `key <NOME>` (EXE, EXIT, MENU, DEL, AC, STO,
F1–F10, SHIFT, OPTN, VARS, SETUP, frecce, HOME, END, PGUP, PGDN), `hold <NOME>`,
`ctrl <lettera>`, `wait <ms>`, `shot <nome>` (PNG 640×640, scala 2×).

Lo script degli screenshot 12–19 di Run-Matrix è `tests/sim/run_matrix.txt`:

```bash
build/host/picocalc_sim /tmp/shots tests/sim/run_matrix.txt
```

## Verifiche del firmware senza hardware

- `picotool info -a build/pico2w/picocalc_fxai.uf2`: nome, versione, board, SDK.
- Simboli del linker: stack 32 KB (`__StackBottom` 0x20078000 – `__StackTop`
  0x20080000), fine dell'immagine flash prima dell'area dati.
- `-fstack-usage` sui sorgenti del motore per controllare i frame ricorsivi
  (vedi [MATH_ENGINE.md](MATH_ENGINE.md#limiti-protezione-dello-stack-da-32-kb-dellrp2350)).

## Collaudo su hardware (da fare)

Procedura in [HARDWARE.md](HARDWARE.md#collaudo-consigliato-sul-dispositivo).
Quando una voce sarà provata sulla PicoCalc, aggiornare
[FEATURE_MATRIX.md](FEATURE_MATRIX.md) a *TESTED ON HARDWARE* indicando data,
revisione della scheda e risultato.
