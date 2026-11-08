# Documentazione

## Documenti attuali (v1.6.0)

| Documento | Contenuto |
|---|---|
| [BUILD_STATUS.md](BUILD_STATUS.md) | ambiente di build, audit della v1.5.4, risultati di build e test, cosa non è verificato |
| [FEATURE_MATRIX.md](FEATURE_MATRIX.md) | stato di ogni funzione (NOT STARTED … TESTED ON HARDWARE) |
| [ARCHITECTURE.md](ARCHITECTURE.md) | strati, ciclo principale, workspace, memoria, cartelle |
| [HARDWARE.md](HARDWARE.md) | PicoCalc + Pico 2 W: display, tastiera, flash, collaudo |
| [GRAPHICS_ENGINE.md](GRAPHICS_ENGINE.md) | canvas, aggiornamento parziale, font, renderer matematico, UI kit |
| [MATH_ENGINE.md](MATH_ENGINE.md) | parser, valutatore, metodi numerici, formati, limiti |
| [AI_INTEGRATION.md](AI_INTEGRATION.md) | relay, protocollo, sicurezza, Claude e ChatGPT |
| [TESTING.md](TESTING.md) | suite, comandi, simulatore |
| [INSTALLATION.md](INSTALLATION.md) | UF2, compilazione, relay AI, configurazione |
| [ROADMAP.md](ROADMAP.md) | cosa manca e priorità |
| [CHANGELOG.md](CHANGELOG.md) | cronologia delle versioni |
| [SYMBOL_CHEATSHEET.md](SYMBOL_CHEATSHEET.md) | simboli e notazione (contenuto dell'app Symbols) |
| [../CONTINUE.md](../CONTINUE.md) | stato e istruzioni per riprendere lo sviluppo |

## Versioni e branch

Ogni versione ha un branch `version/<n>` e un tag `v<n>`; non vanno modificati
dopo il rilascio. Lo sviluppo nuovo avviene in branch separati.

| Branch / tag | Contenuto |
|---|---|
| `version/v1.5.4` · `v1.5.4` | codice originale caricato (non compilava per RP2350) |
| `version/v1.6.0` · `v1.6.0` | prima build RP2350, modalità complete della v1.6.0, relay AI |

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
