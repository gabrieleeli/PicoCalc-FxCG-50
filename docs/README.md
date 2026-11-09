# Documentazione

## Documenti attuali (v1.9.0)

| Documento | Contenuto |
|---|---|
| [BUILD_STATUS.md](BUILD_STATUS.md) | ambiente di build, audit della v1.5.4, risultati di build e test, cosa non è verificato |
| [FEATURE_MATRIX.md](FEATURE_MATRIX.md) | stato di ogni funzione (NOT STARTED … TESTED ON SIMULATOR … TESTED ON HARDWARE) |
| [GRAPH_PARITY.md](GRAPH_PARITY.md) | GRAPH: audit della v1.7.0 e matrice di parità voce per voce con il capitolo 5 del manuale fx-CG50 |
| [GRAPH_ENGINE.md](GRAPH_ENGINE.md) | motore dei grafici: modello, coordinate, campionamento, rendering, ZOOM, G-Solve, memoria e stack |
| [STORAGE.md](STORAGE.md) | flash interna e microSD: workspace, file store, FatFs, protocollo SD, pin SPI0 |
| [PCGRAPH_FORMAT.md](PCGRAPH_FORMAT.md) | formati `.pcgraph`, `PCGMEM`, `.pcpict`, BMP; scrittura atomica e recupero |
| [TABLE.md](TABLE.md) | TABLE: lista delle relazioni, variabile range/lista, tipi Y=/r=/Param, editing delle righe, LISTMEM, persistenza |
| [PICO_PLUS_2W.md](PICO_PLUS_2W.md) | Pimoroni Pico Plus 2 W: schede supportate, mappa della memoria, driver PSRAM, pool e limiti, partizioni della flash, grafica, stack, build, diagnostica |
| [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) | collaudo della v1.9.0 sulla PicoCalc fisica (PSRAM, memoria, flash, 3D, guasti; HW-40…HW-82) |
| [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md) | procedura di collaudo sulla PicoCalc fisica (GRAPH, microSD, TABLE; HW-01…HW-36, da ripetere con la v1.9.0) e dati da inviare |
| [APPLICATIONS_ARCHITECTURE.md](APPLICATIONS_ARCHITECTURE.md) | registro delle modalità, servizi condivisi, dati scambiati, progetto delle modalità future, test di architettura |
| [RUN_MATRIX.md](RUN_MATRIX.md) | Run-Matrix: tasti, cronologia, risultati esatti, S⇔D, frazioni miste, periodo, variabili |
| [ARCHITECTURE.md](ARCHITECTURE.md) | strati, ciclo principale, workspace, memoria, cartelle |
| [HARDWARE.md](HARDWARE.md) | PicoCalc + Pico Plus 2 W / Pico 2 W: display, tastiera, microSD, flash, collaudo |
| [GRAPHICS_ENGINE.md](GRAPHICS_ENGINE.md) | canvas, aggiornamento parziale, font, renderer matematico, UI kit |
| [MATH_ENGINE.md](MATH_ENGINE.md) | parser, valutatore, metodi numerici, formati, limiti |
| [AI_INTEGRATION.md](AI_INTEGRATION.md) | relay, protocollo, sicurezza, Claude e ChatGPT |
| [TESTING.md](TESTING.md) | suite, comandi, simulatore |
| [INSTALLATION.md](INSTALLATION.md) | UF2, compilazione, relay AI, configurazione |
| [ROADMAP.md](ROADMAP.md) | cosa manca e priorità |
| [CHANGELOG.md](CHANGELOG.md) | cronologia delle versioni |
| [SYMBOL_CHEATSHEET.md](SYMBOL_CHEATSHEET.md) | simboli e notazione (contenuto dell'app Symbols) |
| [reports/](reports/) | rapporti di sessione (v1.7.0, GRAPH v1.8.0, collaudo e TABLE v1.8.0) e del consolidamento della v1.9.0 ([CONSOLIDAMENTO_v1.9.0.md](reports/CONSOLIDAMENTO_v1.9.0.md)) |
| [../CONTINUE.md](../CONTINUE.md) | stato e istruzioni per riprendere lo sviluppo |

## Versioni e branch

Un solo branch per versione. `main` è la versione corrente (1.9.0); le versioni
precedenti hanno un branch storico `version/<n>` che non va modificato. Gli
altri branch ancora presenti su GitHub (`version/v1.9.0`, `release/1.9.0`,
`claude/*`, `ccr-*`) sono duplicati interamente contenuti in `main`, da
eliminare ([rapporto](reports/CONSOLIDAMENTO_v1.9.0.md#16-branch-e-tag)).

| Branch / tag | Contenuto |
|---|---|
| **`main`** (tag `v1.9.0` da creare) | v1.9.0: v1.8.0 completa + Pimoroni Pico Plus 2 W (PSRAM, flash 16 MB, 3D Graph) |
| `version/v1.5.4` | codice originale caricato (non compilava per RP2350) |
| `version/v1.6.0` | prima build RP2350, modalità complete della v1.6.0, relay AI |
| `version/v1.7.0` | Sessione 1: audit, architettura delle applicazioni, Run-Matrix con risultati esatti |
| `version/v1.8.0` | GRAPH, microSD, TABLE e collaudo software (Pico 2 W) |

## Documenti storici (versioni ≤ 1.5.4)

Spostati in [`archive/`](archive/); conservati come riferimento; descrivono fasi precedenti e **possono non
corrispondere al codice attuale** (es. PIO-SPI, font 5×7, window manager).

| Documento | Argomento |
|---|---|
| [AUTO_SYMBOL_INPUT.md](archive/AUTO_SYMBOL_INPUT.md) | input automatico di simboli (v1.5.1) |
| [CG50_PARITY_ROADMAP.md](archive/CG50_PARITY_ROADMAP.md) | obiettivi di parità delle versioni 1.x |
| [DISPLAY_DRIVER.md](archive/DISPLAY_DRIVER.md) | driver display PIO-SPI (sostituito da SPI1 hardware) |
| [GRAPHICS_ENGINE_ARCHITECTURE.md](archive/GRAPHICS_ENGINE_ARCHITECTURE.md), [GRAPHICS_ENGINE_AUDIT.md](archive/GRAPHICS_ENGINE_AUDIT.md), [GRAPHICS_ENGINE_PHASE2.md](archive/GRAPHICS_ENGINE_PHASE2.md), [GRAPHICS_PHASE5_MATH_LAYOUT.md](archive/GRAPHICS_PHASE5_MATH_LAYOUT.md) | fasi del motore grafico |
| [LAUNCHER_PHASE6.md](archive/LAUNCHER_PHASE6.md) | launcher a 20 modalità |
| [NATURAL_VPAM_EDITOR.md](archive/NATURAL_VPAM_EDITOR.md), [VPAM_AUDIT_V1_5_3.md](archive/VPAM_AUDIT_V1_5_3.md), [VPAM_SHARED_AST.md](archive/VPAM_SHARED_AST.md) | evoluzione dell'editor Natural |
| [PICOCALC_KEYBOARD_RUN_MATRIX.md](archive/PICOCALC_KEYBOARD_RUN_MATRIX.md) | prima integrazione tastiera |
| [POINT_A_COMPLETION.md](archive/POINT_A_COMPLETION.md) | ampliamento del core matematico (v1.2) |
| [REFERENCE_CASIO_CG50_EMULATOR.md](archive/REFERENCE_CASIO_CG50_EMULATOR.md) | uso del progetto di riferimento solo come documentazione di comportamento |
| [RUN_MATRIX_PHASE1.md](archive/RUN_MATRIX_PHASE1.md), [RUN_MATRIX_PHASE2.md](archive/RUN_MATRIX_PHASE2.md), [RUN_MATRIX_PHASE3.md](archive/RUN_MATRIX_PHASE3.md) | fasi di Run-Matrix |
| [V1_5_VALIDATION.md](archive/V1_5_VALIDATION.md) | validazione della v1.5 |
| [WINDOW_MANAGER.md](archive/WINDOW_MANAGER.md) | window manager (sostituito dal framework di app) |
| [funzioni_richieste.txt](archive/funzioni_richieste.txt) | elenco originale delle funzioni richieste |
| [math_engine_original.py](archive/math_engine_original.py) | prototipo Python del motore matematico |
