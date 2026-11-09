# Build status

## v1.9.0 (2026-11-09)

Verificato in questo ambiente (Pico SDK 2.2.0, arm-none-eabi-gcc 13.2.1):
- `./scripts/build.sh --board pico2w` e `--board pico_plus_2w`: **build riuscita**, zero warning.
  Pico 2 W 456 716 B (21% della partizione FW da 2 MiB), SRAM statica 320 KiB;
  Pico Plus 2 W 505 364 B (12% di 4 MiB), SRAM statica 321 KiB.
- Varianti compilate: Bluetooth (entrambe), framebuffer in PSRAM, -O2, LTO limitato ai sorgenti app.
- Suite host (19) per entrambi i profili, anche con ASan/UBSan; TSan sull'allocatore; CTest 19/19.

Non verificato: nessun test su hardware (PSRAM, flash/LittleFS, LCD, tastiera,
Wi-Fi, SD, BT). Usare *System → Test diagnostici* sul dispositivo.
Dettagli: `docs/PICO_PLUS_2W.md`.

# Storico — v0.3

## Verificato
- Motore matematico compilato e testato su host con `g++ -std=c++17 -Wall -Wextra -Werror`.
- Parser scientifico: precedenza aritmetica, parentesi, potenze, moltiplicazione implicita (`2pi`, `2(x+1)`), modulo, fattoriale postfix, variabili x/y, costanti pi/e.
- Funzioni parser: trigonometriche e inverse, iperboliche e inverse, sqrt/cbrt, exp, ln/log10/log2, abs, floor/ceil/round/trunc/frac/sign, reciprocal, conversioni deg/rad/grad, min/max, logaritmo in base, root n-esima, atan2, hypot, pow, mod.
- API di composizione e inversione numerica già presenti; solver quadratiche e disequazioni quadratiche già presenti.
- Nuova schermata calcolatrice 320x320 disegnata tramite primitive `board::*`, con barra superiore, area input, risultato, cronologia e griglia tasti funzione.
- La schermata calcolatrice è ora la vista iniziale; il launcher resta richiamabile da `ui::drawHome()`.

## Non ancora verificato / incompleto
- Nessuna build ARM/UF2: Pico SDK e toolchain ARM non sono disponibili in questo ambiente.
- Il driver LCD in `src/board_port.cpp` mantiene `clear/text/rect/pixel` come stub: quindi la UI è implementata a livello applicativo ma non può ancora apparire sul display fisico.
- `readKey()` non ha ancora una mappatura verificata del protocollo del microcontrollore tastiera. La schermata mostra i tasti, ma non è ancora possibile digitare o calcolare dal dispositivo.
- Non è ancora una compatibilità completa FX-991CW: restano da implementare/integrare template di calcolo naturale, memoria Ans/variabili, editor di matrici/vettori/liste, solver e sistemi completi, distribuzioni/test/intervalli statistici, conversioni/unità e flussi UI delle modalità.
- Le API del motore non equivalgono ancora a un'applicazione completa: servono test di regressione estesi e verifica numerica indipendente.

## Prossimi blocchi necessari
1. Driver ST7365P e framebuffer/flush reali, verificati sulla revisione hardware.
2. Protocollo tastiera I2C e navigazione UI.
3. Editor di espressioni e memoria Ans, modalità DEG/RAD/GRA e formato di visualizzazione.
4. Modalità complete FX-991CW e test di conformità per ogni funzione.

## Aggiornamento v0.4
Implementati e testati su host:
- Memoria: variabili A-Z, Ans, PreAns, M indipendente; `evaluate(expr, memory)` legge le variabili.
- Notazione naturale: formatter Unicode per radici, pi greco, operatori e potenze numeriche. Non è ancora un renderer MathML/2D con frazioni impilate e radicali a barra estesa.
- Editor matrici: quattro matrici fino a 4×4, API per dimensioni e celle; UI statica di visualizzazione.
- Editor statistiche: sei liste numeriche, inserimento/rimozione, riepilogo descrittivo e regressione lineare; UI statica di visualizzazione.
- Statistica inferenziale: ANOVA one-way, test t a un campione, appaiato e a due campioni (Welch/varianze uguali), intervallo di confidenza t per la media.

Ancora da completare per la piena compatibilità FX-991CW: renderer naturale tipografico completo (frazioni verticali, integrali, sommatorie e matrici 2D), persistenza memoria, interazione reale degli editor con tastiera e navigazione, test t indipendente, test Z, χ² e F, intervalli Z, distribuzioni complete e schermate/modi applicativi mancanti. L'interfaccia LCD e la tastiera fisica restano non operative finché il driver hardware non viene implementato e validato sul dispositivo.

## Aggiornamento v0.5
Implementate e testate su host le API per equazioni lineari/quadratiche, disequazioni lineari/quadratiche, sistemi quadrati di equazioni lineari, sistemi di disequazioni in una variabile, unione e intersezione degli insiemi soluzione. Test host compilati con `-Wall -Wextra -Werror` e superati. Menu e input da tastiera non ancora collegati; nessuna build Pico SDK/UF2 o prova hardware.


## Aggiornamento v0.8 — preparazione integrazione hardware
Verificato in questo ambiente:
- Suite host completa compilata con `g++ -std=c++17 -O2 -Wall -Wextra -Werror` ed eseguita con esito PASS tramite `tests/run_host_tests.sh`.
- Driver tastiera aggiornato: I2C1 a 10 kHz, selezione registro `0x09` e lettura FIFO a due byte. `readKey()` restituisce l'evento grezzo `state << 8 | keycode`.
- Rimossa l'inizializzazione hardware SPI1 impropria per il display: il PicoCalc utilizza un driver PIO-SPI dedicato.

Non verificato / ancora necessario:
- Decodifica del byte stato/codice in tasti logici e collegamento al ciclo UI.
- Driver PIO-SPI ST7365P completo, init panel, framebuffer, DMA e test sul display fisico.
- SD/FatFs o LittleFS, persistenza impostazioni e cronologia.
- Wi-Fi + HTTPS relay per API AI; nessuna chiave API va salvata nel firmware.
- Menu/applicazioni e collegamento delle API matematiche ai flussi UI.
- Build ARM con Pico SDK, generazione UF2 e prova sul Raspberry Pi Pico 2 W.

Non dichiarare questa versione firmware completa: è un incremento verificato del core e della transazione FIFO, non una build UF2.

## Aggiornamento v0.9 — primo driver display
Implementato nel sorgente:
- framebuffer RGB565 320×320;
- trasmettitore SPI via PIO su GP10/GP11;
- primitive LCD reali per pixel, rettangoli e testo 5×7;
- finestra indirizzi ST7365P e invio framebuffer;
- primitive UI che eseguono un singolo `present()` a fine schermata.

Verifica: test host del motore matematico superati. Non è stata eseguita una build ARM/UF2 perché in questo ambiente non è disponibile la toolchain Pico SDK. Il driver LCD non è ancora stato provato su un PicoCalc fisico: init, MADCTL, gamma e offset Y richiedono calibrazione sul display effettivo. Il formatter `formatNatural()` resta testuale; non è ancora un renderer Natural V.P.A.M. 2D con frazioni impilate e cursore strutturato.


## v1.2 — Punto A backend increment (2026-11-06)

Verified on host using `tests/run_host_tests.sh` with `g++ -std=c++17 -O2 -Wall -Wextra -Werror`: PASS. The test target now compiles `src/point_a.cpp` and covers the added RunMatrixSession, matrix/vector operations, fraction arithmetic and overflow guard, complex functions, signed Base-N and bitwise operations, expression calculus/SOLVE, unit conversions, SI prefixes and constants.

No Pico SDK/ARM build or physical PicoCalc test was performed for this increment. Run-Matrix Natural V.P.A.M. 2D, complete mode navigation, parser integration for complex values and full FX-CG50 behavioral parity remain incomplete. See `POINT_A_COMPLETION.md`.

## Aggiornamento v1.3 — editor Natural V.P.A.M.

Verificato in questo incremento:
- `./tests/run_host_tests.sh`: PASS con C++17, `-O2 -Wall -Wextra -Werror`.
- Suite host eseguita anche con AddressSanitizer + UndefinedBehaviorSanitizer: PASS.
- `src/ui.cpp` compilato isolatamente con `-Wall -Wextra -Werror`: PASS.
- Test editor per frazioni, radicali, potenze, gruppi, indici non valutabili, navigazione verticale, inserimento/cancellazione, serializzazione e layout/cursore.

Non verificato:
- Nessuna build Pico SDK/ARM o UF2.
- Nessun test su PicoCalc fisico/display.
- Nessuna mappatura del pacchetto raw `board::readKey()` agli eventi logici NaturalEditor: i codici tasto non sono ancora documentati/verificati.
- `ui::drawNaturalEditor()` è disponibile ma il `main.cpp` dimostrativo non lo presenta ancora come workspace raggiungibile.

Stato: editor matematico strutturato host-tested e primitive UI collegate; non è ancora un editor Run-Matrix completo né parità fx-CG50.

## Aggiornamento v1.4 — tastiera e Run-Matrix

### Verificato su host
- `./tests/run_host_tests.sh`: PASS con `g++ -std=c++17 -O2 -Wall -Wextra -Werror`.
- Suite compilata ed eseguita anche con AddressSanitizer + UndefinedBehaviorSanitizer: PASS.
- `src/ui.cpp` compilato isolatamente con `-Wall -Wextra -Werror`: PASS.
- Test aggiunti per decodifica FIFO press/hold/release, caratteri ASCII, frecce, Backspace/Delete, Enter/Esc, F1–F6.
- Test end-to-end del controller: apertura Run-Matrix, `1+2 = 3`, frazione `1/3`, radicale `sqrt(9)`, potenza `2^3`, gruppo `(2+3)`, rifiuto del pedice non valutabile, input incompleto e ritorno al launcher.

### Integrazione implementata nel sorgente
- `board::readKey()` restituisce il pacchetto FIFO come `state << 8 | keycode`, coerente con l'ordine dei byte nel firmware STM32 ClockworkPi.
- Nuovo decoder `decodePicoCalcKey()` e nuovo `RunMatrixController` indipendenti dall'hardware, quindi verificabili su host.
- `main.cpp` esegue il polling e presenta launcher/Run-Matrix; UI ridisegnata soltanto su eventi validi.
- La schermata Run-Matrix visualizza l'editor strutturato, risultato e messaggi.

### Non verificato
- Build ARM/Pico SDK/UF2 non eseguita: `PICO_SDK_PATH` non è impostato e `arm-none-eabi-g++` non è presente.
- Nessun test con tastiera, LCD o Pico 2 W fisici. La mappatura raw è derivata dal firmware ufficiale ClockworkPi e testata con pacchetti sintetici host. Il sorgente LCD corrente contiene già init ST7365P, framebuffer RGB565 e trasmissione PIO-SPI; resta da validare sul pannello fisico (la precedente nota storica sui driver stub è superata dagli incrementi v0.9/v1.x).
- Il desktop SDL non è stato configurato perché manca la dipendenza `sdl2` (CMake ha interrotto la configurazione).
- Non è ancora parità completa Run-Matrix: mancano scrolling/wrapping, undo/redo, selezione/clipboard, gestione completa dei modificatori, catalogo comandi e validazione sul dispositivo.

## Fase grafica 1 — audit e fase 2 — Graphics Engine (2026-11-07)

### Audit iniziale del repository v1.4

- Inventariati tutti i file di firmware, header, desktop, test e documentazione presenti nell'archivio v1.4.
- Confermato che `src/board_port.cpp` non contiene più stub grafici: implementa framebuffer RGB565 statico 320×320, primitive pixel/rettangolo/testo, init LCD e flush PIO-SPI completo. Il driver resta non verificato su display fisico.
- Confermato che `src/ui.cpp` contiene schermate launcher, calibrazione, calcolatrice, editor matrici/statistiche e Run-Matrix, ma il launcher è statico e non ha selezione/focus applicativo.
- Confermato che `src/main.cpp` collega attualmente soltanto Home e Run-Matrix. Gli altri modelli/applicazioni non sono tutti raggiungibili da tastiera.
- `src/natural_editor.cpp` fornisce un albero editabile e operazioni strutturali, ma il layout naturale non è ancora un sistema tipografico completo e non gestisce scrolling/wrapping.
- `src/graph.cpp` è un modulo iniziale e non costituisce ancora un'applicazione grafica interattiva completa.
- `src/ai_client.cpp` resta un placeholder di rete, indipendente dal lavoro grafico.
- La roadmap `CG50_PARITY_ROADMAP.md` e la matrice `FEATURE_MATRIX.md` restano i riferimenti per non confondere moduli matematici con applicazioni finite.

### Implementato in fase 2

- Aggiunti `include/graphics_engine.hpp` e `src/graphics_engine.cpp` con canvas RGB565 allocation-free e indipendente dal Pico SDK.
- Aggiunte primitive: pixel, linee, rettangoli, rettangoli arrotondati, cerchi, archi, poligoni, bitmap/livelli, clipping e blending RGB565.
- `board::graphics()` espone il canvas che riutilizza il framebuffer statico esistente. `board::clear/pixel/rect` mantengono la compatibilità e delegano al motore.
- Integrato `src/graphics_engine.cpp` nella build Pico SDK.
- Aggiunti test host dedicati in `tests/graphics_smoke.cpp` e aggiornato `tests/run_host_tests.sh`.

### Test realmente eseguiti

- `./tests/run_host_tests.sh` — PASS (suite matematica esistente + suite graphics engine, compilate con C++17, `-Wall -Wextra -Werror`).

### Non verificato / prossimi interventi

- Non è stata eseguita una build Pico SDK/UF2: `PICO_SDK_PATH` non è impostato e `arm-none-eabi-g++` non è installato nell'ambiente.
- Nessuna verifica elettrica o visiva su PicoCalc fisico.
- Il flush rimane full-frame PIO-SPI; DMA e dirty rectangles sono rinviati fino alla validazione hardware.
- Font bitmap condivisi, UTF-8/simboli matematici, layout 2D naturale, widget/menu, focus launcher e navigazione multi-app restano da implementare.

## Fase 3 — primo renderer bitmap condiviso (2026-11-07)

### Implementato

- Aggiunti `include/font_renderer.hpp` e `src/font_renderer.cpp`.
- Spostato il font bitmap ASCII 5×7 fuori dal driver hardware: il renderer supporta scala intera 1×, 2× e 3×, misurazione testo, newline e limite di larghezza.
- Aggiunti glifi bitmap dedicati per π, √, ×, ÷, Σ, θ, ∫, ∞, ≤ e ≥; gli altri codepoint non disponibili usano un fallback leggibile.
- `board::text()` ora delega al renderer condiviso. Le schermate esistenti mantengono la stessa API.
- Aggiunti `tests/font_smoke.cpp` e integrazione nella suite host.

### Test realmente eseguiti

- `./tests/run_host_tests.sh` — PASS (matematica + graphics engine + font renderer).
- Font renderer con AddressSanitizer e UndefinedBehaviorSanitizer — PASS.
- Compilazione isolata `font_renderer.cpp` con `-Wall -Wextra -Wpedantic -Werror` — PASS.
- Compilazione isolata `ui.cpp` con `-Wall -Wextra -Werror` — PASS.

### Limiti ancora aperti

- Il font è bitmap 5×7 scalato per nearest-neighbour; non è ancora un set tipografico proporzionale né include l'intero alfabeto greco.
- Le dimensioni matematiche annidate, parentesi adattive, frazioni e radicali a barra estesa richiedono il Math Layout Engine dedicato.
- Il renderer usa il clipping del Canvas, ma non implementa ancora wrapping automatico o scroll.

## v1.4.2 — incremento UI/graphics

Host suite aggiornata: matematica esistente, primitive RGB565, font, layout
matematico, graph renderer, launcher, widget e timeline animazioni. Il comando
`./tests/run_host_tests.sh` è stato eseguito nell'ambiente host e ha terminato
con PASS. I translation unit UI e moduli grafici principali sono stati compilati
con `-Wall -Wextra -Werror`.

La configurazione Pico SDK è stata tentata ma non può procedere: `PICO_SDK_PATH`
non è impostato e CMake non trova `external/pico_sdk_import.cmake`; di
conseguenza `pico_sdk_init()` non è definito. Non è stata generata alcuna UF2.
Non è disponibile un PicoCalc fisico in questo ambiente: driver LCD, tastiera,
orientamento, offset e aggiornamento parziale restano da verificare sul
pannello reale.
