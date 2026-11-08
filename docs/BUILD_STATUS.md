# Build status — v1.8.0 (collaudo software e TABLE)

Le note storiche delle versioni v0.3–v1.5.4 restano nella cronologia Git e in
[CHANGELOG.md](CHANGELOG.md). Le sezioni dopo "v1.8.0" descrivono le build
precedenti e l'audit della v1.6.0 e restano valide come base.

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
| Sistema | Ubuntu 24.04 (container x86_64) |
| Toolchain ARM | `arm-none-eabi-gcc` 13.2.1 (pacchetto `gcc-arm-none-eabi` 15:13.2.rel1-2) |
| Pico SDK | 2.2.0 (submodule: tinyusb, cyw43-driver, lwip, mbedtls 3.6.2) |
| CMake / Ninja | 3.28.3 / 1.11 |
| Compilatore host | g++ 13.3 (C++20, `-Wall -Wextra -Werror`) |
| Python (relay) | 3.13, `anthropic` 2.6.0, `openai` 3.46.0 |

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
- Wi-Fi CYW43, DNS, handshake TLS con verifica del certificato, consumo di RAM di mbedTLS.
- Stabilità prolungata e consumi.

Nessun test "su hardware" è dichiarato superato: la matrice usa al massimo lo stato
**TESTED ON HOST**.
