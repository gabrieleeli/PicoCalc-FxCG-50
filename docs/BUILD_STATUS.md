# Build status — v1.9.0 (consolidamento: v1.8.0 + Pimoroni Pico Plus 2 W)

Le note storiche delle versioni v0.3–v1.5.4 restano nella cronologia Git e in
[CHANGELOG.md](CHANGELOG.md). Le sezioni dopo "v1.9.0" descrivono le build
precedenti e restano valide come confronto.

## v1.9.0 — `main`

Build pulite con `scripts/build.sh --clean --board <scheda>` (Release, `-O2`),
una cartella per scheda. Ambiente in fondo alla pagina (invariato; relay con
`anthropic` 2.7.0 e `openai` 3.48.0). **Nessuna prova su hardware.**

| Voce | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Output | `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2` (2 772 992 byte) | `build/pico2w/picocalc_fxai_pico2w.uf2` (2 755 584 byte) |
| `picotool info` | "PicoCalc FX-CG50 AI", versione `1.9.0 (pico_plus_2w)`, RP2350 ARM Secure, `pimoroni_pico_plus2_w_rp2350`, SDK 2.2.0 | "PicoCalc FX-CG50 AI", versione `1.9.0 (pico2w)`, RP2350 ARM Secure, `pico2_w`, SDK 2.2.0 |
| Immagine flash | 1 386 216 byte (`__flash_binary_end` = `0x101526E8`) = 33 % della partizione firmware da 4 MB | 1 377 516 byte (`0x101504EC`) = 65 % della partizione da 2 MB |
| Resto della flash | diagnostica 4 KB, LittleFS 11 248 KB, BTstack 8 KB, workspace 1024 KB (2 × 512 KB), ultimo settore libero (E10) | diagnostica 4 KB, LittleFS 1908 KB, BTstack 8 KB, workspace 128 KB nella posizione della v1.8.0 |
| RAM statica | 299 444 byte fino a `__end__` = `0x200491B4` (di cui 200 KB framebuffer) | 295 520 byte (`0x20048260`) |
| Heap SRAM | 192 076 byte (`__end__` → `0x20078000`) | 196 000 byte |
| PSRAM | 8 MB come pool TLSF separato, creato a runtime dopo il self-test; nessuna sezione statica in PSRAM | — |
| Stack | core 0: 32 KB (`0x20078000`–`0x20080000`, guardia MSPLIM); core 1: 4 KB in SCRATCH_X | uguale |
| Frame di stack massimi (`-fstack-usage`) | `diag::testMemory` 1712 B, `GraphApp::openProject` 1696 B, `main` 1584 B; moduli nuovi: `graph3d` 552 B, `Graph3DApp` 520 B, `board_pico` 608 B, `flash_fs` 456 B, `psram` 128 B, TLSF 40 B | uguale |
| Warning | **nessuno** (0 in tutta la build, SDK incluso; FatFs e LittleFS compilati con `-w`) | nessuno |
| Avvisi CMake | solo quello dell'SDK su picotool (non installato: compilato dai sorgenti) | uguale |
| SHA-256 dell'UF2 | `805a6c4912aae0ae65d7abb5b5350d9893cdc9bca6c9cf8ec23cdf3e99881f65` | `489911a97bcbfeab4c717cdc0bce2bd1f5f57a8589e49cff83d79a3beeb56591` |
| Riproducibilità | due build pulite in cartelle diverse e una incrementale producono UF2 identici byte per byte | uguale |

Rispetto alla v1.8.0 (`-O3`, 1 505 640 byte di immagine) l'immagine è più
piccola perché il livello predefinito ora è `-O2` (codice più compatto per la
cache XIP da 16 KB condivisa con la PSRAM); `-O3` resta disponibile
(`--opt O3`). La RAM statica cresce di 6,7 KB sulla Plus (stato del 3D,
metadati dei pool, buffer di rimbalzo della flash, buffer riservati per i
messaggi d'errore) e di 2,9 KB sul Pico 2 W, che non ha l'oggetto del pool
PSRAM.

### Test

| Suite | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | 35/35: 33 suite C++ con 7 705 controlli, `mem_pools_smoke` (≈ 333 000 verifiche di integrità, numero variabile perché due thread si contendono i pool) e `sd_interop_test` (41) | 35/35: 7 699 controlli + ≈ 333 000 + 41 |
| AddressSanitizer + UBSan (`--sanitize`) | 35/35 | 35/35 |
| ThreadSanitizer (`--tsan`, allocatore su due thread) | superato | superato |
| Relay AI (Python, `unittest`) | 15/15 | — |
| v1.8.0 rieseguita come riferimento | 25/25, **7 179** controlli + 41 (la documentazione della v1.8.0 indicava 7169: il conteggio di `architecture_test` era fermo a 529, il test ne esegue 539) | |

`scripts/make_release.sh` (dopo le due build) produce lo zip dei sorgenti, i due
UF2 e `SHA256SUMS`; verificato su questo commit. Il pacchetto non è stato
pubblicato come release GitHub.

Dettagli per suite in [TESTING.md](TESTING.md). Le build dei test usano gli
stessi sorgenti portabili del firmware; il dispositivo non è stato usato.

## v1.8.0 — collaudo software di GRAPH e microSD, completamento di TABLE

Stesso ambiente. Build pulita con `scripts/build_pico2w.sh` (Release, `-O3`).

| Voce | Valore |
|---|---|
| Output | `build/pico2w/picocalc_fxai.uf2` (3 012 096 byte); `picotool info`: "PicoCalc FX-CG50 AI", versione 1.8.0, RP2350, `pico2_w`, SDK 2.2.0 |
| Immagine flash | ≈1470 KB (`__flash_binary_end` = `0x1016F968`) su 4096 KB (+67 KB: TABLE, diagnostica, ricerca dei poli, recupero della microSD) |
| RAM statica | ≈286 KB fino a `__end__` = `0x200476E8` (+16 byte) |
| Heap | ≈194 KB; stack 32 KB |
| Warning | nessuno nei sorgenti del progetto |
| Test host | 25 test CTest: 24 suite con 7169 controlli + `sd_interop_test` (41 controlli con fsck.fat, fsck.exfat, mtools); tutti superati anche con ASan + UBSan |
| Relay AI | 15 test Python superati |
| Stack | frame massimi dei moduli toccati: diagnostica 808 B, `writeAtomic` 640 B, `graph::integrate` 616 B, `TableApp::drawList` 424 B |
| Hardware | **non provato**: procedura in [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md) |

## v1.8.0 (branch `version/v1.8.0`) — sessione GRAPH

Stesso ambiente della v1.7.0 (sotto). Build pulita con
`scripts/build_pico2w.sh` (Release, `-O3`).

| Voce | Valore |
|---|---|
| Output | `build/pico2w/picocalc_fxai.uf2` (2 874 880 byte); `picotool info`: nome "PicoCalc FX-CG50 AI", versione 1.8.0, RP2350 ARM Secure, `pico2_w`, SDK 2.2.0 |
| Immagine flash | ≈1403 KB (`__flash_binary_end` = `0x1015EDA4`) su 4096 KB; area dati utente (ultimi 128 KB) libera |
| RAM statica | ≈286 KB fino a `__end__` = `0x200476D8` (di cui 200 KB framebuffer) |
| Heap | ≈194 KB (`0x200476D8` → `0x20078000`) |
| Stack core 0 | 32 KB (`0x20078000`–`0x20080000`) |
| Warning | nessuno nei sorgenti del progetto; FatFs (`third_party/fatfs`) è compilato con `-w` |
| Test host | 22 suite, 6540 controlli, tutti superati; anche con ASan + UBSan |
| Relay AI | 15 test Python superati |
| Stack dei frame di GRAPH | misurato con `-fstack-usage` (massimo `GraphApp::openProject` 1712 byte, vedi [GRAPH_ENGINE.md](GRAPH_ENGINE.md#stack-e-prestazioni-sul-target)) |
| Hardware | **non provato** sulla PicoCalc (né display né microSD) |

Crescita rispetto alla v1.7.0: +273 KB di flash (motore GRAPH, G-Solve, nuova
interfaccia GRAPH, progetti e immagini, driver SD e FatFs), +3,2 KB di RAM
statica (stato GRAPH nel workspace, oggetti del driver SD, buffer LFN statico
di FatFs; il volume FatFs con il suo settore è allocato nell'heap); l'heap
scende di 3,2 KB. Memoria dei grafici a runtime limitata a 96 KB di punti
(`MaxTotalCurvePoints`) più le immagini RLE (≤ 96 KB ciascuna).

## v1.7.0 (branch `version/v1.7.0`)

Stesso ambiente della v1.6.0 (sotto), ricreato da zero nel container: toolchain
`gcc-arm-none-eabi` 13.2.rel1, Pico SDK 2.2.0 con i submodule tinyusb,
cyw43-driver, lwip, mbedtls.

| Voce | Valore |
|---|---|
| Output | `build/pico2w/picocalc_fxai.uf2` (2 315 776 byte, build pulita) |
| Immagine flash | ≈1130 KB (`__flash_binary_end` = `0x1011A978`) su 4096 KB; area dati utente (ultimi 128 KB) libera |
| RAM statica | ≈283 KB fino a `__end__` = `0x200469E4` (di cui 200 KB framebuffer) |
| Heap | ≈197 KB (`0x200469E4` → `0x20078000`) |
| Stack core 0 | 32 KB (`0x20078000`–`0x20080000`) |
| Warning | nessuno nei sorgenti del progetto (build pulita; solo i due avvisi CMake dell'SDK su picotool e btstack) |
| Test host | 17 suite, 5347 controlli, tutti superati; anche con ASan + UBSan |
| Relay AI | 15 test Python superati |
| Stack dei nuovi frame | misurato con `-fstack-usage` (vedi [MATH_ENGINE.md](MATH_ENGINE.md)) |
| Hardware | **non provato** sulla PicoCalc |

Crescita rispetto alla v1.6.0: +111 KB di flash (aritmetica esatta, valutatore
esatto, registro e modelli, nuova interfaccia di Run-Matrix), +2,6 KB di RAM
statica (forme esatte di A–Z/Ans nel workspace).

## Ambiente di build verificato

| Componente | Versione |
|---|---|
| Sistema | Ubuntu 24.04.5 (container x86_64) |
| Toolchain ARM | `arm-none-eabi-gcc` 13.2.1 (pacchetto `gcc-arm-none-eabi` 15:13.2.rel1-2) |
| Pico SDK | 2.2.0 (submodule: tinyusb, cyw43-driver, lwip, mbedtls 3.6.2) |
| CMake / Ninja | 3.28.3 / 1.11 |
| Compilatore host | g++ 13.3 (C++20, `-Wall -Wextra -Werror`) |
| Python (relay) | 3.13, `anthropic` 2.7.0, `openai` 3.48.0 (v1.9.0; v1.8.0: 2.6.0 / 3.46.0) |
| picotool | 2.2.0, compilato dall'SDK durante la build |

## Audit della v1.5.4 di partenza

La v1.5.4 **non poteva essere compilata per l'RP2350** e conteneva difetti che
avrebbero impedito il funzionamento sulla PicoCalc:

| # | Problema trovato | Correzione v1.6.0 |
|---|---|---|
| 1 | `board_port.cpp` usava `glyph()` non dichiarata → errore di compilazione | driver riscritto (`src/platform/pico/board_pico.cpp`) |
| 2 | Il Pico SDK disabilita le eccezioni C++, ma il motore usa `throw` ovunque | `PICO_CXX_ENABLE_EXCEPTIONS=1` |
| 3 | Link a `pico_cyw43_arch_lwip_*` senza `lwipopts.h` → errore | `src/platform/pico/config/lwipopts.h` |
| 4 | Stack di 2 KB (SCRATCH_Y 4 KB) insufficiente per parser ricorsivo ed eccezioni | linker script con stack da 32 KB in SRAM |
| 5 | Init LCD diversa dal driver ufficiale: senza `INVON` (colori invertiti), parametri B0/B1/B4/B6 scambiati, senza gamma | sequenza ufficiale ClockworkPi |
| 6 | Documentazione e codice affermavano che il display richiede PIO-SPI: i driver ufficiali usano **SPI1 hardware** | SPI1 a 25 MHz come i driver ufficiali |
| 7 | Lettura tastiera con repeated-start senza STOP e attesa (il driver ufficiale usa STOP + 16 ms) | macchina a stati non bloccante con STOP + attesa |
| 8 | Cursore dell'editor calcolato a 8 px/glifo, espressione disegnata a 6 px/glifo → cursore spostato | metriche uniche nel renderer |
| 9 | Test basati su `assert`: con `NDEBUG` tutti i controlli sparivano | macro `CHECK` sempre attive |
| 10 | `binomialPmf` andava in overflow (`nCr` a 64 bit) per n > ~60 | forma log-gamma |
| 11 | Due parser diversi (uno a stringa, uno AST con ~10 funzioni) | pipeline unica tokenizer → AST → valutatore |
| 12 | Nessun limite di lunghezza/annidamento: sul target ARM ogni livello del valutatore occupava ~1,3 KB di stack, quindi una somma di ~25 termini avrebbe superato lo stack da 32 KB (misurato con `-fstack-usage`) | frame ricorsivi ridotti a 32–128 byte, catene valutate in modo iterativo, limiti di token/annidamento, budget di stack con "Stack ERROR" |

## Risultati di build (firmware)

```
cmake -S . -B build/pico2w -G Ninja -DCMAKE_BUILD_TYPE=Release   (PICO_SDK_PATH=/opt/sdk/pico-sdk)
cmake --build build/pico2w
```

| Voce | Valore |
|---|---|
| Output | `build/pico2w/picocalc_fxai.uf2` (≈2,0 MB, famiglia `rp2350-arm-s`) |
| Immagine flash | ≈1019 KB su 4096 KB (include firmware CYW43, lwIP, mbedTLS) |
| Area dati utente | ultimi 128 KB della flash (2 slot A/B), nessuna sovrapposizione con l'immagine |
| RAM statica | ≈261 KB (di cui 200 KB framebuffer 320×320 RGB565) |
| Heap | ≈200 KB (`__end__` → `0x20078000`) |
| Stack core 0 | 32 KB (`0x20078000`–`0x20080000`) |
| Warning | nessuno nei sorgenti del progetto |

`picotool info -a` conferma: nome "PicoCalc FX-CG50 AI", versione 1.6.0,
`pico_board: pico2_w`, SDK 2.2.0, image type ARM Secure, target RP2350.

## Risultati dei test (host)

`tests/run_host_tests.sh` e `tests/run_host_tests.sh --sanitize` (ASan + UBSan):

| Suite | Controlli |
|---|---|
| expression_test | 142 |
| editor_test | 166 |
| math_engine_test | 108 |
| storage_test | 727 |
| app_test | 62 |
| apps_flow_test | 54 |
| solvers_test | 60 |
| statistics_test | 63 |
| ai_test | 138 |
| graphics_smoke | 57 |
| font_smoke | 2556 |
| animation_smoke | 6 |
| cheatsheet_smoke | 343 |
| vpam_ast_smoke | 13 |
| **Totale** | **4495 controlli, 0 errori** (con e senza sanitizer) |

Relay AI (`cd relay && python -m unittest discover -s tests`): **15 test superati**.

## Non verificato (richiede la PicoCalc fisica)

- Display: colori, orientamento, area visibile, velocità di aggiornamento.
- Tastiera: tutti i codici, tenuta/ripetizione, modificatori, tempi I2C.
- Batteria e retroilluminazione via registri STM32.
- Scrittura flash con `flash_safe_execute` e ripristino dopo riavvio/blackout.
- PSRAM APS6404L: rilevamento, self-test, frequenza, convivenza con la cache XIP e con le scritture flash (v1.9.0).
- Lettura del JEDEC ID della flash all'avvio e partizioni della flash da 16 MB della Pico Plus 2 W.
- Stack con guardia MSPLIM, rapporto HardFault, core 1 della diagnostica.
- Wi-Fi CYW43, DNS, handshake TLS con verifica del certificato, consumo di RAM di mbedTLS.
- Stabilità prolungata e consumi.

Nessun test "su hardware" è dichiarato superato: la matrice usa al massimo lo stato
**TESTED ON HOST**.
